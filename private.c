/*
 * private.c
 * Amazon SimpleDB C bindings
 *
 * Created by Peter Macko on 1/16/09.
 *
 * Copyright 2009
 *      The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "stdafx.h"
#include "sdb.h"
#include "sdb_private.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#include <unistd.h>

const char* SDB_AWS_ERRORS[] = {
	"AccessFailure",
	"AuthFailure",
	"AuthMissingFailure",
	"FeatureDeprecated",
	"InternalError",
	"InvalidAction",
	"InvalidBatchRequest",
	"InvalidHTTPAuthHeader",
	"InvalidHttpRequest",
	"InvalidLiteral",
	"InvalidNextToken",
	"InvalidNumberPredicates",
	"InvalidNumberValueTests",
	"InvalidParameterCombination",
	"InvalidParameterValue",
	"InvalidQueryExpression",
	"InvalidResponseGroups",
	"InvalidService",
	"InvalidSOAPRequest",
	"InvalidURI",
	"InvalidWSAddressingProperty",
	"MalformedSOAPSignature",
	"MissingAction",
	"MissingParameter",
	"MissingWSAddressingProperty",
	"NoSuchDomain",
	"NoSuchVersion",
	"NotYetImplemented",
	"NumberDomainsExceeded",
	"NumberDomainAttributesExceeded",
	"NumberDomainBytesExceeded",
	"NumberItemAttributesExceeded",
	"NumberSubmittedAttributesExceeded",
	"RequestExpired",
	"RequestTimeout",
	"ServiceUnavailable",
	"SignatureDoesNotMatch",
	"TooManyRequestedAttributes",
	"UnsupportedHttpVerb",
	"UnsupportedNextToken",
	"URITooLong"
};


/**
 * Create a Curl handle
 * 
 * @param sdb the SimpleDB handle
 * @return the Curl handle, or NULL on error
 */
CURL* sdb_create_curl(struct SDB* sdb)
{
	CURL* h = NULL;
	
	// Create the handle
	
	if ((h = curl_easy_init()) == NULL) return NULL;
	
	
	// Configure the Curl handle
	
	curl_easy_setopt(h, CURLOPT_URL, AWS_URL);
	curl_easy_setopt(h, CURLOPT_HTTPHEADER, sdb->curl_headers);
	curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, sdb_write_callback);
	
	return h;
}


/**
 * Allocate a multi data structure
 * 
 * @param sdb the SimpleDB handle
 * @return the allocated data structure, or NULL on error
 */
struct sdb_multi_data* sdb_multi_alloc(struct SDB* sdb)
{
	struct sdb_multi_data* m = NULL;
	
	
	// Reuse an existing handle, if there is one
	
	if (sdb->multi_free != NULL) {
	
		m = sdb->multi_free;
		sdb->multi_free = sdb->multi_free->next;
		sdb->multi_free_size--;
		
		assert(m->post == NULL && m->curl != NULL && m->rec.size == 0);
	}
	else {
		
		// Otherwise allocate one
		
		m = (struct sdb_multi_data*) malloc(sizeof(struct sdb_multi_data));
		
		m->rec.size = 0;
		m->rec.capacity = 64 * 1024;
		m->rec.buffer = (char*) malloc(m->rec.capacity);
		
		m->post = NULL;
		m->curl = sdb_create_curl(sdb);
	}
	
	
	// Additional initialization
	
	m->command[0] = '\0';
	m->params = NULL;
	
	
	// Add it to the chain
	
	m->next = sdb->multi;
	sdb->multi = m;
	
	return m;
}


/**
 * Free a multi data structure, reusing it if possible
 * 
 * @param sdb the SimpleDB handle
 * @param m the data structure to free
 */
void sdb_multi_free_one(struct SDB* sdb, struct sdb_multi_data* m)
{
	if (m == NULL) return;
	
	
	// Cleanup non-reusable parts of the data structure 
	
	m->next = NULL;
	m->rec.size = 0;
	
	if (m->post != NULL) {
		free(m->post);
		m->post = NULL;
	}
	
	sdb_params_free(m->params);
	m->params = NULL;
	m->command[0] = '\0';
	
	curl_multi_remove_handle(sdb->curl_multi, m->curl);
	
	
	// Destroy the handle if we have too many of them
	
	if (sdb->multi_free_size >= SDB_MAX_MULTI_FREE) {
		if (m->curl != NULL) curl_easy_cleanup(m->curl);
		if (m->rec.buffer != NULL) free(m->rec.buffer);
		free(m);
		return;
	}
	
	
	// Otherwise add it to the free list
	
	m->next = sdb->multi_free;
	sdb->multi_free = m;
	sdb->multi_free_size++;
} 


/**
 * Free a chain of multi data structure, reusing it if possible
 * 
 * @param sdb the SimpleDB handle
 * @param m the first data structure of the chain
 */
void sdb_multi_free_chain(struct SDB* sdb, struct sdb_multi_data* m)
{
	struct sdb_multi_data* next;
	
	for ( ; m != NULL; m = next) {
		next = m->next;
		sdb_multi_free_one(sdb, m);
	}
} 


/**
 * Destroy a chain of multi data structures
 * 
 * @param sdb the SimpleDB handle
 * @param m the first data structure in the chain
 */
void sdb_multi_destroy(struct SDB* sdb, struct sdb_multi_data* m)
{
	struct sdb_multi_data* next;
	
	for ( ; m != NULL; m = next) {
		next = m->next;
		
		curl_multi_remove_handle(sdb->curl_multi, m->curl);
		
		sdb_params_free(m->params);
		
		if (m->curl != NULL) curl_easy_cleanup(m->curl);
		if (m->post != NULL) free(m->post);
		if (m->rec.buffer != NULL) free(m->rec.buffer);
		
		free(m);
	}
} 


/**
 * Find the multi data structure based on the Curl handle
 * 
 * @param chain the beginning of the chain
 * @param curl the Curl handle
 * @return the data structure, or NULL if not found
 */
struct sdb_multi_data* sdb_multi_find(struct sdb_multi_data* chain, CURL* curl)
{
	struct sdb_multi_data* m;
	
	for (m = chain; m != NULL; m = m->next) {
		if (m->curl == curl) return m;
	}
	
	return NULL;
}


/**
 * Create a formatted UTC timestamp
 * 
 * @param buffer the buffer to which to write the timestamp (must be at least 32 bytes long)
 * @return SDB_OK if no errors occurred
 */
int sdb_timestamp(char* buffer)
{
	// Get the time
	
	time_t rawtime;
	struct tm* ptm;
	time(&rawtime);
	ptm = gmtime(&rawtime);
	
	
	// Format
	 
	sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02d.000Z", ptm->tm_year + 1900, ptm->tm_mon + 1,
					ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
	
	return SDB_OK;
}


/**
 * Sign a string
 * 
 * @param sdb the SimpleDB handle
 * @param str the string to sign
 * @param buffer the buffer to write the signature to (must be at least EVP_MAX_MD_SIZE * 2 bytes)
 * @param plen the pointer to the place to store the length of the signature (can be NULL)
 * @return SDB_OK if no errors occurred
 */
int sdb_sign(struct SDB* sdb, const char* str, char* buffer, size_t* plen)
{
	char md[EVP_MAX_MD_SIZE];
	size_t mdl;
	
	HMAC(EVP_sha1(), sdb->sdb_secret, sdb->sdb_secret_len, str, strlen(str), md, &mdl);
	 
	size_t l = base64(md, mdl, buffer, EVP_MAX_MD_SIZE * 2);
	
	if (l == 0) return SDB_E_OPEN_SSL_FAILED;
	if (plen != NULL) *plen = l;
	
	return SDB_OK;
}


/**
 * Allocate an array for storing SimpleDB parameters
 * 
 * @param capacity the estimated capacity
 * @return the array
 */ 
struct sdb_params* sdb_params_alloc(size_t capacity)
{
	struct sdb_params* p = (struct sdb_params*) malloc(sizeof(struct sdb_params));
	p->size = 0;
	p->capacity = capacity;
	p->params = (struct sdb_pair*) malloc((capacity + 8) * sizeof(struct sdb_pair));
	return p;
}


/**
 * Free a parameters array
 * 
 * @param params the arrays to free
 */
void sdb_params_free(struct sdb_params* params)
{
	if (params == NULL) return;
	
	size_t i;
	
	for (i = 0; i < params->size; i++)
	{
		if (params->params[i].parent == params) {
			free(params->params[i].key);
			free(params->params[i].value);
		}
	}
	free(params->params);
	free(params);
}


/**
 * Add a parameter (this routine will make a copy of its inputs)
 * 
 * @param params the parameter array
 * @param key the key
 * @param value the value
 * @return SDB_OK if no errors occurred
 */
int sdb_params_add(struct sdb_params* params, const char* key, const char* value)
{
	if (params->capacity <= params->size) return SDB_E_CAPACITY_TOO_SMALL;
	
	params->params[params->size].key = strdup(key);
	params->params[params->size].value = strdup(value);
	params->params[params->size].parent = params;
	
	params->size++;
	
	return SDB_OK;
}


/**
 * Add elements from another parameter array without making a copy of the inputs
 * 
 * @param params the parameter array
 * @param other the other parameter array
 * @return SDB_OK if no errors occurred
 */
int sdb_params_add_all(struct sdb_params* params, struct sdb_params* other)
{
	if (params->capacity < params->size + other->size) return SDB_E_CAPACITY_TOO_SMALL;
	
	int i;
	for (i = 0; i < other->size; i++) {
		params->params[params->size].key = other->params[i].key;
		params->params[params->size].value = other->params[i].value;
		params->params[params->size].parent = other;
		
		params->size++;
	}
	
	return SDB_OK;
}


/**
 * String comparison wrapper
 * 
 * @param a the first value
 * @param b the second value
 * @return the comparison return code
 */
int str_compare(const void* a, const void* b)
{
	return strcasecmp((const char*) a, (const char*) b);
}


/**
 * Sort the parameters by the keys
 * 
 * @param params the parameter array
 * @return SDB_OK if no errors occurred
 */
int sdb_params_sort(struct sdb_params* params)
{
	qsort(params->params, params->size, sizeof(struct sdb_pair), str_compare);
	return SDB_OK;
}


/**
 * Sign the parameters
 * 
 * @param sdb the SimpleDB handle
 * @param params the parameter array
 * @param buffer the buffer to write the signature to (must be at least EVP_MAX_MD_SIZE * 2 bytes)
 * @param plen the pointer to the place to store the length of the signature (can be NULL)
 * @return SDB_OK if no errors occurred
 */
int sdb_params_sign(struct SDB* sdb, struct sdb_params* params, char* buffer, size_t* plen)
{
	assert(params->size >= 2);
	
	
	// Signature version 0
	
	if (sdb->sdb_signature_ver == 0) {
	
		assert(strcmp(params->params[0].key, "Action") == 0);
		assert(strcmp(params->params[1].key, "Timestamp") == 0);
		
		size_t l = strlen(params->params[0].value) + strlen(params->params[1].value) + 4;
		char* b = (char*) alloca(l);
		
		strcpy(b, params->params[0].value);
		strcat(b, params->params[1].value);
		
		return sdb_sign(sdb, b, buffer, plen); 
	}
	
	
	// Signature version 1
	
	if (sdb->sdb_signature_ver == 1) {
		
		SDB_SAFE(sdb_params_sort(params));
		
		size_t i, l = 0;
		for (i = 0; i < params->size; i++) {
			l += strlen(params->params[i].key);
			l += strlen(params->params[i].value);
		}
		
		char* b = (char*) alloca(l);
		*b = '\0';
		
		for (i = 0; i < params->size; i++) {
			strcat(b, params->params[i].key);
			strcat(b, params->params[i].value);
		}
		
		return sdb_sign(sdb, b, buffer, plen); 
	}
	
	
	return SDB_E_INVALID_SIGNATURE_VER;
}


/**
 * Create the URL-encoded parameters string
 * 
 * @param sdb the SimpleDB handle
 * @param params the parameter array
 * @param pbuffer the variable for the output buffer (will be allocated by the routine, but has to be freed by caller)
 * @return SDB_OK if no errors occurred
 */
int sdb_params_export(struct SDB* sdb, struct sdb_params* params, char** pbuffer)
{
	// Sign the parameters
	
	char signature[EVP_MAX_MD_SIZE * 2];
	SDB_SAFE(sdb_params_sign(sdb, params, signature, NULL));
	SDB_SAFE(sdb_params_add(params, "Signature", signature));
	
	
	// Determine the appropriate size
	
	size_t i, l = 0;
	for (i = 0; i < params->size; i++) {
		l += strlen(params->params[i].key) + 1;
		l += strlen(params->params[i].value) * 3 + 1;
	}
	
	
	// Allocate buffer
	
	*pbuffer = (char*) malloc(l + 4);
	char* b = *pbuffer;
	*b = '\0';
	
	
	// Build the string
	
	for (i = 0; i < params->size; i++) {
	
		if (i > 0) strcat(b, "&");
		
		strcat(b, params->params[i].key);
		strcat(b, "=");
		
		char* e = curl_easy_escape(sdb->curl_handle, params->params[i].value, strlen(params->params[i].value));
		if (e == NULL) {
			free(b);
			*pbuffer = NULL;
			return SDB_E_URL_ENCODE_FAILED;
		} 
		
		strcat(b, e);
		
		curl_free(e);
	}
	
	
	return SDB_OK;
}


/**
 * Add required SimpleDB parameters to the array
 * 
 * @param sdb the SimpleDB handle
 * @param params the parameter array
 */
int sdb_params_add_required(struct SDB* sdb, struct sdb_params* params)
{
	SDB_SAFE(sdb_params_add(params, "SignatureVersion", sdb->sdb_signature_ver_str));
	SDB_SAFE(sdb_params_add(params, "Version", "2007-11-07"));
	SDB_SAFE(sdb_params_add(params, "AWSAccessKeyId", sdb->sdb_key));
	
	return SDB_OK;
}


/**
 * Deep copy the parameters
 * 
 * @param params the parameter array
 * @return the deep copy
 */
struct sdb_params* sdb_params_deep_copy(struct sdb_params* params)
{
	if (params == NULL) return NULL;
	
	int i;
	struct sdb_params* p = sdb_params_alloc(params->capacity);
	
	for (i = 0; i < params->size; i++) sdb_params_add(p, params->params[i].key, params->params[i].value);
	
	return p;
}


/**
 * Create the command URL
 * 
 * @param sdb the SimpleDB handle
 * @param params the parameter array
 * @return the URL, or NULL on error
 */
char* sdb_url(struct SDB* sdb, struct sdb_params* params)
{
	char* urlencoded;
	if (SDB_FAILED(sdb_params_export(sdb, params, &urlencoded))) return NULL;
	
	char* url = (char*) malloc(strlen(urlencoded) + 64);
	strcpy(url, "http://sdb.amazonaws.com/?");
	strcat(url, urlencoded);
	free(urlencoded);
	
	return url;
}


/**
 * Create the command POST data
 * 
 * @param sdb the SimpleDB handle
 * @param params the parameter array
 * @return the URL, or NULL on error
 */
char* sdb_post(struct SDB* sdb, struct sdb_params* params)
{
	char* urlencoded;
	if (SDB_FAILED(sdb_params_export(sdb, params, &urlencoded))) return NULL;
	
	return urlencoded;
}


/**
 * The Curl write callback function
 * 
 * @param buffer the received buffer
 * @param size the size of an item
 * @param num the number of items
 * @param data the user data
 * @return the number of processed bytes
 */
size_t sdb_write_callback(void* buffer, size_t size, size_t num, void* data)
{
	// Initialize
	
	struct sdb_buffer* rec = (struct sdb_buffer*) data;
	size_t bytes = size * num;
	
	
	// Check the buffer size
	
	if (rec->size + bytes > rec->capacity) {
		
		// Reallocate the buffer
		
		rec->capacity = 2 * (rec->size + bytes) + 4096;
		char* buf = (char*) malloc(rec->capacity);
		assert(buf);
		
		memcpy(buf, rec->buffer, rec->size);
		
		free(rec->buffer);
		rec->buffer = buf;
	}
	
	
	// Copy the buffer contents
	
	memcpy(rec->buffer + rec->size, buffer, bytes);
	rec->size += bytes;
	
	return bytes;
}


/**
 * Execute a command and ignore the result-set
 * 
 * @param sdb the SimpleDB handle
 * @param cmd the command name
 * @param _params the parameters
 * @return the result
 */
int sdb_execute(struct SDB* sdb, const char* cmd, struct sdb_params* _params)
{
	// Prepare the command execution
	
	struct sdb_params* params = sdb_params_alloc(_params->size + 8);
	SDB_SAFE(sdb_params_add(params, "Action", cmd));
	char timestamp[32]; sdb_timestamp(timestamp);
	SDB_SAFE(sdb_params_add(params, "Timestamp", timestamp));
	
	
	// Add the command parameters
	
	SDB_SAFE(sdb_params_add_all(params, _params));
	
	
	// Add the required parameters
	
	SDB_SAFE(sdb_params_add_required(sdb, params));
	char* post = sdb_post(sdb, params);
	long postsize = strlen(post);
	sdb_params_free(params);
	
	
	// Configure Curl and execute the command
	
	CURL* curl = sdb->curl_handle;
	
	sdb->rec.size = 0;
	curl_easy_setopt(curl, CURLOPT_URL, AWS_URL);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sdb->rec);
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post);
	CURLcode cr = curl_easy_perform(curl);
	free(post);
	
	
	// Statistics
	
	if (cr == CURLE_OK) {
		sdb->stat.num_commands++;
		if (strncmp(cmd, "Put", 3) == 0) sdb->stat.num_puts++;
		sdb_update_size_stats(sdb, curl, postsize, sdb->rec.size);
	}
	
	
	// Handle Curl errors and internal AWS errors
	
	if (cr != CURLE_OK) return SDB_CURL_ERROR(cr);
	if (strncmp(sdb->rec.buffer, "<html", 5) == 0) return SDB_E_AWS_INTERNAL_ERROR_2;
	
#ifdef _DEBUG_PRINT_RESPONSE
	sdb->rec.buffer[sdb->rec.size] = '\0';
	printf("\n%s\n\n", sdb->rec.buffer);
#endif
	
	
	// Parse the response and check for errors
	
	struct sdb_response response; sdb_response_init(&response);
	response.internal->errout = sdb->errout;
	int __ret = sdb_response_parse(&response, sdb->rec.buffer, sdb->rec.size);
	
	if (SDB_FAILED(__ret)) {
		sdb_response_cleanup(&response);
		return __ret;
	}
	
	sdb->stat.box_usage += response.box_usage;
	
	if (response.error != 0) {
		__ret = response.error;
		sdb_response_cleanup(&response);
		return SDB_AWS_ERROR(__ret);
	}
	
	
	// Cleanup
	
	sdb_response_cleanup(&response);
	return SDB_OK;
}


/**
 * Execute a command
 * 
 * @param sdb the SimpleDB handle
 * @param cmd the command name
 * @param params the parameters
 * @param response the pointer to the result-set
 * @return the result
 */
int sdb_execute_rs(struct SDB* sdb, const char* cmd, struct sdb_params* _params, struct sdb_response** response)
{
	// Prepare the command execution
	
	struct sdb_params* params = sdb_params_alloc(_params->size + 8);
	SDB_SAFE(sdb_params_add(params, "Action", cmd));
	char timestamp[32]; sdb_timestamp(timestamp);
	SDB_SAFE(sdb_params_add(params, "Timestamp", timestamp));
	
	
	// Add the command parameters
	
	SDB_SAFE(sdb_params_add_all(params, _params));
	
	
	// Add the next token
	
	if (*response != NULL) {
		if ((*response)->has_more) {
			assert((*response)->internal->next_token);
			SDB_SAFE(sdb_params_add(params, "NextToken", (*response)->internal->next_token));
		}
	}
	
	
	// Add the required parameters
	
	SDB_SAFE(sdb_params_add_required(sdb, params));
	char* post = sdb_post(sdb, params);
	long postsize = strlen(post);
	sdb_params_free(params);
	
	
	// Configure Curl and execute the command
	
	CURL* curl = sdb->curl_handle;
	
	sdb->rec.size = 0;
	curl_easy_setopt(curl, CURLOPT_URL, AWS_URL);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sdb->rec);
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post);
	CURLcode cr = curl_easy_perform(curl);
	free(post);
	
	
	// Statistics
	
	if (cr == CURLE_OK) {
		sdb->stat.num_commands++;
		if (strncmp(cmd, "Put", 3) == 0) sdb->stat.num_puts++;
		sdb_update_size_stats(sdb, curl, postsize, sdb->rec.size);
	}
	
	
	// Handle Curl errors and internal AWS errors
	
	if (cr != CURLE_OK) return SDB_CURL_ERROR(cr);
	if (strncmp(sdb->rec.buffer, "<html", 5) == 0) return SDB_E_AWS_INTERNAL_ERROR_2;
	
#ifdef _DEBUG_PRINT_RESPONSE
	sdb->rec.buffer[sdb->rec.size] = '\0';
	printf("\n%s\n\n", sdb->rec.buffer);
#endif
	
	
	// Parse the response and check for errors
	
	if (*response == NULL) {
		*response = sdb_response_allocate();
	}
	else {
		sdb_response_prepare_append(*response);
	}
	(*response)->internal->errout = sdb->errout;
	int __ret = sdb_response_parse(*response, sdb->rec.buffer, sdb->rec.size);
	
	if (SDB_FAILED(__ret)) {
		sdb_free(response);
		return __ret;
	}
	
	sdb->stat.box_usage += (*response)->box_usage;
	
	if ((*response)->error != 0) {
		__ret = (*response)->error;
		if (SDB_AWS_ERROR(__ret) != SDB_E_AWS_SERVICE_UNAVAILABLE) sdb_free(response);
		return SDB_AWS_ERROR(__ret);
	}
	
	return SDB_OK;
}


/**
 * Execute a command using Curl's multi interface
 * 
 * @param sdb the SimpleDB handle
 * @param cmd the command name
 * @param _params the parameters
 * @param next_token the next token (optional)
 * @param user_data the user data (optional)
 * @param user_data_2 the user data (optional)
 * @return the handle to the deferred call, or SDB_MULTI_ERROR on error 
 */
sdb_multi sdb_execute_multi(struct SDB* sdb, const char* cmd, struct sdb_params* _params, char* next_token, void* user_data, void* user_data_2)
{
	// Prepare the command execution
	
	struct sdb_params* params = sdb_params_alloc(_params->size + 8);
	if (SDB_FAILED(sdb_params_add(params, "Action", cmd))) return SDB_MULTI_ERROR;
	char timestamp[32]; sdb_timestamp(timestamp);
	if (SDB_FAILED(sdb_params_add(params, "Timestamp", timestamp))) return SDB_MULTI_ERROR;
	
	
	// Add the command parameters
	
	if (SDB_FAILED(sdb_params_add_all(params, _params))) return SDB_MULTI_ERROR;
	
	
	// Add the next token
	
	if (next_token != NULL) {
		if (SDB_FAILED(sdb_params_add(params, "NextToken", next_token))) return SDB_MULTI_ERROR;
	}
	
	
	// Add the required parameters
	
	if (SDB_FAILED(sdb_params_add_required(sdb, params))) return SDB_MULTI_ERROR;
	char* post = sdb_post(sdb, params);
	long postsize = strlen(post);
	sdb_params_free(params);
	
	
	// Allocate a multi-data structure
	
	struct sdb_multi_data* m = sdb_multi_alloc(sdb);
	assert(m);
	
	m->post = post;
	strncpy(m->command, cmd, SDB_LEN_COMMAND - 1);
	m->command[SDB_LEN_COMMAND - 1] = '\0';
	m->params = sdb_params_deep_copy(_params);
	m->user_data = user_data;
	m->user_data_2 = user_data_2;
	m->post_size = postsize;
	
	
	// Create a Curl handle and defer it
	
	sdb->rec.size = 0;
	curl_easy_setopt(m->curl, CURLOPT_URL, AWS_URL);
	curl_easy_setopt(m->curl, CURLOPT_WRITEDATA, &m->rec);
	curl_easy_setopt(m->curl, CURLOPT_POST, 1L);
	curl_easy_setopt(m->curl, CURLOPT_POSTFIELDS, post);
	CURLMcode cr = curl_multi_add_handle(sdb->curl_multi, m->curl);
	
	
	// Handle Curl errors
	
	if (cr != CURLM_OK) {
		sdb_multi_free_one(sdb, m);
		return SDB_MULTI_ERROR;
	}
	
	
	// Statistics (the size statistics would be updated when the response is received)
	
	sdb->stat.num_commands++;
	if (strncmp(cmd, "Put", 3) == 0) sdb->stat.num_puts++;
	
	
	return m->curl;
}


/**
 * Run all deferred multi calls and wait for the result
 * 
 * @param sdb the SimpleDB handle
 * @return the result
 */
int sdb_multi_run_and_wait(struct SDB* sdb)
{
	// This code was inspired by the implementation of readdir() in s3fs by Randy Rizun
	
	int running, r;
	
	while (curl_multi_perform(sdb->curl_multi, &running) == CURLM_CALL_MULTI_PERFORM) usleep(5);
	
	while (running) {
		fd_set read_fd_set;
		fd_set write_fd_set;
		fd_set exc_fd_set;
		
		FD_ZERO(&read_fd_set);
		FD_ZERO(&write_fd_set);
		FD_ZERO(&exc_fd_set);
		
		long ms;
		if ((r = curl_multi_timeout(sdb->curl_multi, &ms)) != CURLM_OK) return SDB_CURLM_ERROR(r);
		
		if (ms < 0) ms = 50;
		if (ms > 0) {
			struct timeval timeout;
			timeout.tv_sec = 1000 * ms / 1000000;
			timeout.tv_usec = 1000 * ms % 1000000;
			
			int max_fd;
			if ((r = curl_multi_fdset(sdb->curl_multi, &read_fd_set, &write_fd_set, &exc_fd_set, &max_fd)) != CURLM_OK) return SDB_CURLM_ERROR(r);
			
			if (select(max_fd + 1, &read_fd_set, &write_fd_set, &exc_fd_set, &timeout) == -1) return SDB_E_FD_ERROR;
		}
		
		while (curl_multi_perform(sdb->curl_multi, &running) == CURLM_CALL_MULTI_PERFORM) usleep(5);
	}
	
	return SDB_OK;
}


/**
 * Parse the response
 * 
 * @param sdb the SimpleDB handle
 * @param curl the used Curl handle
 * @param post_size the post size (for statistics)
 * @param rec the buffer with the response
 * @param result the pointer to the result-set
 * @return the result
 */
int sdb_parse_result(struct SDB* sdb, CURL* curl, long post_size, struct sdb_buffer* rec, struct sdb_response** response)
{
	// Statistics
	
	sdb_update_size_stats(sdb, curl, post_size, rec->size);
	
	
	// Handle internal errors
	
	if (strncmp(rec->buffer, "<html", 5) == 0) return SDB_E_AWS_INTERNAL_ERROR_2;
	
#ifdef _DEBUG_PRINT_RESPONSE
	sdb->rec.buffer[rec->size] = '\0';
	printf("\n%s\n\n", rec->buffer);
#endif
	
	
	// Parse the response and check for errors
	
	if (*response == NULL) {
		*response = sdb_response_allocate();
	}
	else {
		sdb_response_prepare_append(*response);
	}
	(*response)->internal->errout = sdb->errout;
	int __ret = sdb_response_parse(*response, rec->buffer, rec->size);
	
	if (SDB_FAILED(__ret)) {
		sdb_free(response);
		return __ret;
	}
	
	sdb->stat.box_usage += (*response)->box_usage;
	
	if ((*response)->error != 0) {
		__ret = (*response)->error;
		if (SDB_AWS_ERROR(__ret) != SDB_E_AWS_SERVICE_UNAVAILABLE) sdb_free(response);
		return SDB_AWS_ERROR(__ret);
	}
	
	return SDB_OK;
}


/**
 * Destroy a retry chain
 * 
 * @param retry the front of the chain
 */
void sdb_retry_destroy_chain(struct sdb_retry_data* retry)
{
	struct sdb_retry_data* next;
	struct sdb_retry_data* r;
	
	for (r = retry; r != NULL; r = next) {
		next = r->next;
		
		sdb_params_free(r->params);
		
		free(r);
	}
}


/**
 * Update the size fields of the statistics (not the command count)
 * 
 * @param sdb the SimpleDB handle
 * @param curl the Curl handle
 * @param post_size the length of the post data
 * @param rec_size the length of the received data
 */ 
void sdb_update_size_stats(struct SDB* sdb, CURL* curl, long post_size, long rec_size)
{
#ifdef ESTIMATE_HTTP_OVERHEAD
	
	sdb->stat.bytes_sent += post_size;
	sdb->stat.http_overhead_sent += sdb_estimate_http_sent(sdb, post_size);
	sdb->stat.bytes_received += rec_size;
	sdb->stat.http_overhead_received += sdb_estimate_http_received(sdb, rec_size);
	
	if (post_size > TINY_INITIAL_POST_SIZE) {
		sdb->stat.http_overhead_sent += 22;
		sdb->stat.http_overhead_received += 25;
	}
	
#else
	
	long request_size, http_received; double upload_size;
	curl_easy_getinfo(curl, CURLINFO_REQUEST_SIZE, &request_size);
	curl_easy_getinfo(curl, CURLINFO_HEADER_SIZE, &http_received);
	curl_easy_getinfo(curl, CURLINFO_SIZE_UPLOAD, &upload_size);
	
	if (post_size > request_size + (long long) upload_size) {
		// If we get here, this probably means that the server responded
		// negatively to "Expect: 100-continue"
		sdb->stat.http_overhead_sent += request_size;
	}
	else {
		sdb->stat.bytes_sent += post_size;
		sdb->stat.http_overhead_sent += request_size + (long long) upload_size - post_size;
	}
	
	sdb->stat.bytes_received += rec_size;
	sdb->stat.http_overhead_received += http_received;
#endif
}


/**
 * Add two statistics objects
 * 
 * @param a the first object (will be modified)
 * @param b the second object
 */
void sdb_add_statistics(struct sdb_statistics* a, const struct sdb_statistics* b)
{
	a->bytes_sent				+= b->bytes_sent;
	a->bytes_received			+= b->bytes_received;
	a->http_overhead_sent		+= b->http_overhead_sent;
	a->http_overhead_received	+= b->http_overhead_received;
	a->num_commands				+= b->num_commands;
	a->num_puts					+= b->num_puts;
	a->num_retries				+= b->num_retries;
	a->box_usage				+= b->box_usage;
}



/**
 * Estimate the HTTP overhead for sending data
 * 
 * @param sdb the SimpleDB handle
 * @param post_size the length of the post data
 * @return the estimated overhead in bytes
 */ 
long sdb_estimate_http_sent(struct SDB* sdb, long post_size)
{
	// Typical header of a request:
	// 
	// POST / HTTP/1.1
	// Host: http://sdb.amazonaws.com
	// Accept: */*
	// Content-Type: application/x-www-form-urlencoded; charset=utf-8
	// User-Agent: libsdb
	// Content-Length: 169
	
	return (strlen(SDB_HTTP_HEADER_CONTENT_TYPE) + strlen(SDB_HTTP_HEADER_USER_AGENT) + 2) + 76 /* size of other headers */ + digits(post_size, 10) + 2 /* \n\n after header */ - 1;
}


/**
 * Estimate the HTTP overhead for receiving data
 * 
 * @param sdb the SimpleDB handle
 * @param response_size the length of the response data
 * @return the estimated overhead in bytes
 */ 
long sdb_estimate_http_received(struct SDB* sdb, long response_size)
{
	// Typical response
	// 
	// HTTP/1.1 200 OK
	// Content-Type: text/xml
	// Transfer-Encoding: chunked
	// Date: Wed, 28 Jan 2009 04:04:45 GMT
	// Server: Amazon SimpleDB
	// 
	// 1c2
	// ... (response body goes here)
	// 0
	
	return 127 /* size of the headers and the separator */ + 6 /* average overhead of the chunked encoding (estimated) */;
}
