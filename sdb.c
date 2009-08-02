/*
 * sdb.c
 * Amazon SimpleDB C bindings
 * 
 * Created by Peter Macko on 11/26/08.
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

static int sdb_initialized = FALSE;

/*
 * After every deletion of a SDB handle, its statistics are added to the global
 * stats. Note that this feature is not thread safe, so it is disabled by
 * default.
 */ 
static struct sdb_statistics sdb_global_stat;


/**
 * Global initialization
 *
 * @return SDB_OK if no errors occurred
 */
int sdb_global_init(void)
{
	// Preliminary checks
	
	if (sdb_initialized) return SDB_E_ALREADY_INITIALIZED;
	
	
	// Initialize Curl
	
	if (curl_global_init(CURL_GLOBAL_NOTHING) != 0) return SDB_E_CURL_INIT_FAILED;
	
	
	// Initialize global statistics
	
	sdb_clear_statistics(NULL);
	
	
	// Finish
	
	sdb_initialized = TRUE;
	return SDB_OK;
}


/**
 * Global cleanup
 *
 * @return SDB_OK if no errors occurred
 */
int sdb_global_cleanup(void)
{
	// Preliminary checks
	
	if (!sdb_initialized) return SDB_E_NOT_INITIALIZED;
	
	
	// Cleanup Curl
	
	curl_global_cleanup();
	
	
	// Cleanup LibXML
	
	xmlCleanupParser();
	
	
	// Finish
	
	sdb_initialized = FALSE;
	return SDB_OK;
}


/**
 * Initialize the environment
 * 
 * @param sdb a pointer to the SimpleDB handle
 * @param key the SimpleDB key
 * @param secret the SimpleDB secret key
 * @return SDB_OK if no errors occurred
 */
int sdb_init(struct SDB** sdb, const char* key, const char* secret)
{
	// Preliminary checks
	
	if (!sdb_initialized) return SDB_E_NOT_INITIALIZED;
	
	assert(key != NULL); 
	assert(secret != NULL);
	
	
	// Allocate the SDB handle
	
	*sdb = (struct SDB*) malloc(sizeof(struct SDB));
	
	
	// Copy arguments
	
	(*sdb)->sdb_key = strdup(key);
	(*sdb)->sdb_secret = strdup(secret);
	
	(*sdb)->sdb_key_len = strlen(key);
	(*sdb)->sdb_secret_len = strlen(secret);
	
	
	// Set the HTTP headers
	
	(*sdb)->curl_headers = NULL;
	(*sdb)->curl_headers = curl_slist_append((*sdb)->curl_headers, SDB_HTTP_HEADER_CONTENT_TYPE);
	(*sdb)->curl_headers = curl_slist_append((*sdb)->curl_headers, SDB_HTTP_HEADER_USER_AGENT);
	
	
	// Initialize Curl
	
	(*sdb)->curl_handle = sdb_create_curl(*sdb);
	if ((*sdb)->curl_handle == NULL) return SDB_E_CURL_INIT_FAILED;
	
	(*sdb)->curl_multi = curl_multi_init();
	if ((*sdb)->curl_multi == NULL) return SDB_E_CURL_INIT_FAILED;
	
	
	// Allocate the buffers
	
	(*sdb)->rec.capacity = 64 * 1024;
	(*sdb)->rec.size = 0;
	(*sdb)->rec.buffer = (char*) malloc((*sdb)->rec.capacity);
	
	
	// Other initialization
	
	(*sdb)->sdb_signature_ver = 0;
	(*sdb)->sdb_signature_ver_str[0] = '0' + (*sdb)->sdb_signature_ver; 
	(*sdb)->sdb_signature_ver_str[1] = '\0';
	
	(*sdb)->multi = NULL;
	(*sdb)->multi_free = NULL;
	(*sdb)->multi_free_size = 0;
	
	(*sdb)->retry_count = 10;
	(*sdb)->retry_delay = 5000;		/* = 5 ms */
	
	(*sdb)->errout = NULL;
	(*sdb)->auto_next = 1;
	
	sdb_clear_statistics(*sdb);
	
	return SDB_OK;
}


/**
 * Destroy the environment
 * 
 * @param sdb a pointer to the SimpleDB handle
 * @return SDB_OK if no errors occurred
 */
int sdb_destroy(struct SDB** sdb)
{
	if (sdb == NULL || *sdb == NULL) return SDB_OK;
	
	
	// Update the global statistics (note that this is not yet thread safe)
	
	sdb_add_statistics(&sdb_global_stat, &(*sdb)->stat);
	
	
	// Internal data cleanup
	
	SAFE_FREE((*sdb)->sdb_key);
	SAFE_FREE((*sdb)->sdb_secret);
	
	
	// Buffer cleanup
	
	SAFE_FREE((*sdb)->rec.buffer);
	
	
	// Curl cleanup
	
	sdb_multi_destroy(*sdb, (*sdb)->multi);
	sdb_multi_destroy(*sdb, (*sdb)->multi_free);
	
	if ((*sdb)->curl_headers != NULL) {
		curl_slist_free_all((*sdb)->curl_headers);
		(*sdb)->curl_headers = NULL;
	}
	
	if ((*sdb)->curl_handle != NULL) {
		curl_easy_cleanup((*sdb)->curl_handle);
		(*sdb)->curl_handle = NULL;
	}
	
	if ((*sdb)->curl_multi != NULL) {
		curl_multi_cleanup((*sdb)->curl_multi);
		(*sdb)->curl_multi = NULL;
	}
	
	
	// Handle cleanup
	
	free(*sdb);
	*sdb = NULL;
	
	return SDB_OK;
}


/**
 * Free the response structure
 * 
 * @param response the data structure
 */
void sdb_free(struct sdb_response** response)
{
	if (response == NULL) return;
	if (*response == NULL) return;
	
	sdb_response_cleanup(*response);
	free(*response);
	
	*response = NULL;
}


/**
 * Free the multi response structure
 * 
 * @param response the data structure
 */
void sdb_multi_free(struct sdb_multi_response** response)
{
	int i;
	
	if (response == NULL) return;
	if (*response == NULL) return;
	
	for (i = 0; i < (*response)->size; i++) sdb_free(&(*response)->responses[i]);
	
	free((*response)->responses);
	free(*response);
	
	*response = NULL;
}


/**
 * Print the response
 * 
 * @param r the data structure
 */
void sdb_print(struct sdb_response* r)
{
	if (r == NULL) return;
	sdb_response_print(stdout, r);
}


/**
 * Print the response
 * 
 * @param f the output file 
 * @param r the data structure
 */
void sdb_fprint(FILE* f, struct sdb_response* r)
{
	if (r == NULL) return;
	sdb_response_print(f, r);
}


/**
 * Count the number of failed commands in a multi response
 * 
 * @param response the data structure
 * @return the number of failed commands
 */
int sdb_multi_count_errors(struct sdb_multi_response* response)
{
	int i, count = 0;
	
	if (response == NULL) return 0;
	
	for (i = 0; i < response->size; i++) {
		struct sdb_response* r = response->responses[i];
		
		if (r == NULL) {
			count++;
		}
		else {
			if (SDB_FAILED(r->return_code)) count++;
		}
	}
	
	return count;
}


/**
 * Return the collected statistics
 * 
 * @param sdb the SimpleDB handle
 * @return the struct with statistics
 */
const struct sdb_statistics* sdb_get_statistics(struct SDB* sdb)
{
	return sdb == NULL ? &sdb_global_stat : &sdb->stat;
}


/**
 * Print the statistics to a file
 *
 * @param sdb the SimpleDB handle
 * @param f the output FILE
 */
void sdb_fprint_statistics(struct SDB* sdb, FILE* f)
{
	struct sdb_statistics* s = sdb == NULL ? &sdb_global_stat : &sdb->stat;
	
	fprintf(f, "Data Sent (bytes)                      : %lld (%0.2lf MB)\n", s->bytes_sent, (double) s->bytes_sent / 1048576.0);
	fprintf(f, "Data Received (bytes)                  : %lld (%0.2lf MB)\n", s->bytes_received, (double) s->bytes_received / 1048576.0);
	fprintf(f, "HTTP Overhead Sent (bytes)             : %lld (%0.2lf MB)\n", s->http_overhead_sent, (double) s->http_overhead_sent / 1048576.0);
	fprintf(f, "HTTP Overhead Received (bytes)         : %lld (%0.2lf MB)\n", s->http_overhead_received, (double) s->http_overhead_received / 1048576.0);
	fprintf(f, "Total bytes sent                       : %lld (%0.2lf MB)\n", s->http_overhead_sent + s->bytes_sent, (double) (s->http_overhead_sent + s->bytes_sent) / 1048576.0);
	fprintf(f, "Total bytes received                   : %lld (%0.2lf MB)\n", s->http_overhead_received + s->bytes_received, (double)  (s->http_overhead_received + s->bytes_received) / 1048576.0);
	fprintf(f, "Total number of PutAttributes commands : %lld\n", s->num_puts);
	fprintf(f, "Total number of commands sent          : %lld\n", s->num_commands);
	fprintf(f, "Total number of retries                : %lld\n", s->num_retries);
	fprintf(f, "Total box usage                        : %llf\n", s->box_usage);
}


/**
 * Print the statistics
 *
 * @param sdb the SimpleDB handle
 */
void sdb_print_statistics(struct SDB* sdb)
{
	sdb_fprint_statistics(sdb, stdout);
}


/**
 * Reset (clear) the statistics
 *
 * @param sdb the SimpleDB handle
 */
void sdb_clear_statistics(struct SDB* sdb)
{
	struct sdb_statistics* s = sdb == NULL ? &sdb_global_stat : &sdb->stat;
	
	memset(s, 0, sizeof(struct sdb_statistics));
	s->box_usage = 0;
}


/**
 * Set the output FILE for additional error messages
 * 
 * @param sdb the SimpleDB handle
 * @param f the output FILE
 */
void sdb_set_error_file(struct SDB* sdb, FILE* f)
{
	sdb->errout = f;
}


/**
 * Set the retry configuration
 * 
 * @param sdb the SimpleDB handle
 * @param count the maximum number of retries
 * @param delay the number of milliseconds between two retries
 */
void sdb_set_retry(struct SDB* sdb, int count, int delay)
{
	if (count < 0) count = 0;
	if (delay < 0) delay = 0;
	
	sdb->retry_count = count;
	sdb->retry_delay = delay * 1000;
} 


/**
 * Set automatic handling of the NEXT tokens
 * 
 * @param sdb the SimpleDB handle
 * @param value zero disables automatic NEXT handling, a non-zero value enables it
 */
void sdb_set_auto_next(struct SDB* sdb, int value)
{
	sdb->auto_next = value == 0 ? 0 : 1;
}


#define SDB_COMMAND_PREPARE(argc)									\
	struct sdb_params* __params = sdb_params_alloc(argc);
	
#define SDB_COMMAND_PARAM(key, value)								\
	SDB_SAFE(sdb_params_add(__params, key, value));
	
#define SDB_COMMAND_PARAM_MULTI(key, value)							\
	if (SDB_FAILED(sdb_params_add(__params, key, value))) 			\
		return SDB_MULTI_ERROR;

#define SDB_COMMAND_EXECUTE(name)									\
	int __r = sdb_execute(sdb, name, __params);						\
	int __retries = sdb->retry_count;								\
	while (__r == SDB_E_AWS_SERVICE_UNAVAILABLE && __retries --> 0){\
		usleep(sdb->retry_delay);									\
		sdb->stat.num_retries++;									\
		__r = sdb_execute(sdb, name, __params);						\
	}																\
	sdb_params_free(__params);										\
	return __r;

#define SDB_COMMAND_EXECUTE_RS(name)								\
	int __r = SDB_OK;												\
	int __retries = sdb->retry_count;								\
	*response = NULL;												\
	while (*response == NULL ? TRUE : ((*response)->has_more		\
										&& sdb->auto_next)) {		\
		if (SDB_FAILED(__r = sdb_execute_rs(sdb, name,				\
			__params, response))) {									\
			if (__r == SDB_E_AWS_SERVICE_UNAVAILABLE) {				\
				if (__retries-- <= 0) {								\
					sdb_free(response); break;						\
				}													\
				usleep(sdb->retry_delay);							\
				sdb->stat.num_retries++;							\
			}														\
			else break;												\
		}															\
	}																\
	sdb_params_free(__params);										\
	return __r;

#define SDB_COMMAND_EXECUTE_MULTI(name)								\
	sdb_multi __r = sdb_execute_multi(sdb, name, __params,			\
									  NULL, NULL, NULL);			\
	sdb_params_free(__params);										\
	return __r;

/**
 * Get more data (if the automatic handling of NEXT tokens is disabled)
 *
 * @param sdb the SimpleDB handle
 * @param response the result set of the previous call
 * @param append whether to append the data to the result set or replace it
 * @return SDB_OK if no errors occurred
 */
int sdb_next(struct SDB* sdb, struct sdb_response** response, int append)
{
	int __r = SDB_OK;
	int __retries = sdb->retry_count;
	
	
	// First, make sure that we actually have data
	
	if (!(*response)->has_more) {
		if (append) return SDB_OK;
		
		sdb_response_cleanup(*response);
		sdb_response_init(*response);
		return SDB_OK;
	}
	
	
	// Clean up the response structure if we are not appending
	
	if (!append) {
		
		// Copy the NEXT token and the command
		
		char* next = (char*) malloc(strlen((*response)->internal->next_token) + 4);
		strcpy(next, (*response)->internal->next_token);
		
		char* command = (char*) malloc(strlen((*response)->internal->command) + 4);
		strcpy(command, (*response)->internal->command);
		
		
		// Copy the parameters
		
		struct sdb_params* params = sdb_params_deep_copy((*response)->internal->params);
		
		
		// Recreate the response
		
		sdb_response_cleanup(*response);
		sdb_response_init(*response);
		
		(*response)->has_more = TRUE;
		(*response)->internal->next_token = next;
		(*response)->internal->command = command;
		(*response)->internal->params = params;
		
		(*response)->internal->to_free = (struct sdb_response_to_free*) malloc(sizeof(struct sdb_response_to_free));
		(*response)->internal->to_free->p = next;
		(*response)->internal->to_free->next = NULL;
	}
	
	
	// Now run the query
	
	struct sdb_params* params = (*response)->internal->params;
	char* command = (*response)->internal->command;
	assert(params);
	assert(command);
	
	do {
		if (SDB_FAILED(__r = sdb_execute_rs(sdb, command, params, response))) {
			if (__r == SDB_E_AWS_SERVICE_UNAVAILABLE) {
				if (__retries-- <= 0) {
					sdb_free(response); break;
				}
				usleep(sdb->retry_delay);
				sdb->stat.num_retries++;
			}
			else break;
		}
	}
	while ((*response)->has_more && sdb->auto_next);
	
	return __r;
}

/**
 * Create a domain
 * 
 * @param sdb the SimpleDB handle
 * @param name the domain name
 * @return SDB_OK if no errors occurred
 */
int sdb_create_domain(struct SDB* sdb, const char* name)
{
	SDB_COMMAND_PREPARE(8);
	SDB_COMMAND_PARAM("DomainName", name);
	SDB_COMMAND_EXECUTE("CreateDomain");
}

/**
 * Delete a domain
 * 
 * @param sdb the SimpleDB handle
 * @param name the domain name
 * @return SDB_OK if no errors occurred
 */
int sdb_delete_domain(struct SDB* sdb, const char* name)
{
	SDB_COMMAND_PREPARE(8);
	SDB_COMMAND_PARAM("DomainName", name);
	SDB_COMMAND_EXECUTE("DeleteDomain");
}


/**
 * List domains
 * 
 * @param sdb the SimpleDB handle
 * @param response a pointer to the place to store the response
 * @return SDB_OK if no errors occurred
 */
int sdb_list_domains(struct SDB* sdb, struct sdb_response** response)
{
	SDB_COMMAND_PREPARE(8);
	SDB_COMMAND_EXECUTE_RS("ListDomains");
}


/**
 * Get domain meta-data
 * 
 * @param sdb the SimpleDB handle
 * @param name the domain name
 * @param response a pointer to the place to store the response
 * @return SDB_OK if no errors occurred
 */
int sdb_domain_metadata(struct SDB* sdb, const char* name, struct sdb_response** response)
{
	SDB_COMMAND_PREPARE(8);
	SDB_COMMAND_PARAM("DomainName", name);
	SDB_COMMAND_EXECUTE_RS("DomainMetadata");
}


/**
 * Put an attribute to the database
 * 
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @param key the attribute key
 * @param value the attribute value
 * @return SDB_OK if no errors occurred
 */
int sdb_put(struct SDB* sdb, const char* domain, const char* item, const char* key, const char* value)
{
	SDB_COMMAND_PREPARE(8);
	SDB_COMMAND_PARAM("ItemName", item);
	SDB_COMMAND_PARAM("DomainName", domain);
	SDB_COMMAND_PARAM("Attribute.0.Name", key);
	SDB_COMMAND_PARAM("Attribute.0.Value", value);
	SDB_COMMAND_EXECUTE("PutAttributes");
}


/**
 * Replace an attribute in the database
 * 
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @param key the attribute key
 * @param value the attribute value
 * @return SDB_OK if no errors occurred
 */
int sdb_replace(struct SDB* sdb, const char* domain, const char* item, const char* key, const char* value)
{
	SDB_COMMAND_PREPARE(8);
	SDB_COMMAND_PARAM("ItemName", item);
	SDB_COMMAND_PARAM("DomainName", domain);
	SDB_COMMAND_PARAM("Attribute.0.Name", key);
	SDB_COMMAND_PARAM("Attribute.0.Value", value);
	SDB_COMMAND_PARAM("Attribute.0.Replace", "true");
	SDB_COMMAND_EXECUTE("PutAttributes");
}


/**
 * Put several attributes to the database
 * 
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @param num the number of attributes
 * @param keys the attribute keys
 * @param values the attribute values
 * @return SDB_OK if no errors occurred
 */
int sdb_put_many(struct SDB* sdb, const char* domain, const char* item, size_t num, const char** keys, const char** values)
{
	size_t i;
	char buf[64];
	
	SDB_COMMAND_PREPARE(8 + num * 2);
	SDB_COMMAND_PARAM("ItemName", item);
	SDB_COMMAND_PARAM("DomainName", domain);
	
	for (i = 0; i < num; i++) {
		sprintf(buf, "Attribute.%d.Name"   , i); SDB_COMMAND_PARAM(buf, keys[i]);
		sprintf(buf, "Attribute.%d.Value"  , i); SDB_COMMAND_PARAM(buf, values[i]);
	}
	
	SDB_COMMAND_EXECUTE("PutAttributes");
}


/**
 * Replace several attributes in the database
 * 
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @param num the number of attributes
 * @param keys the attribute keys
 * @param values the attribute values
 * @return SDB_OK if no errors occurred
 */
int sdb_replace_many(struct SDB* sdb, const char* domain, const char* item, size_t num, const char** keys, const char** values)
{
	size_t i;
	char buf[64];
	
	SDB_COMMAND_PREPARE(8 + num * 3);
	SDB_COMMAND_PARAM("ItemName", item);
	SDB_COMMAND_PARAM("DomainName", domain);
	
	for (i = 0; i < num; i++) {
		sprintf(buf, "Attribute.%d.Name"   , i); SDB_COMMAND_PARAM(buf, keys[i]);
		sprintf(buf, "Attribute.%d.Value"  , i); SDB_COMMAND_PARAM(buf, values[i]);
		sprintf(buf, "Attribute.%d.Replace", i); SDB_COMMAND_PARAM(buf, "true");
	}
	
	SDB_COMMAND_EXECUTE("PutAttributes");
}


/**
 * Put attributes of several items to the database
 * 
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param num the number of items
 * @param items the array of items and their attributes
 * @return SDB_OK if no errors occurred
 */
int sdb_put_batch(struct SDB* sdb, const char* domain, size_t num, const struct sdb_item* items)
{
	size_t i, attrs;
	int j;
	char buf[64];
	
	attrs = 0;
	for (i = 0; i < num; i++) {
		attrs += items[i].size;
	}
	
	SDB_COMMAND_PREPARE(8 + num + attrs * 2);
	SDB_COMMAND_PARAM("DomainName", domain);
	
	for (i = 0; i < num; i++) {
		sprintf(buf, "Item.%d.ItemName", i); SDB_COMMAND_PARAM(buf, items[i].name);
		for (j = 0; j < items[i].size; j++) {
			sprintf(buf, "Item.%d.Attribute.%d.Name"   , i, j); SDB_COMMAND_PARAM(buf, items[i].attributes[j].name);
			sprintf(buf, "Item.%d.Attribute.%d.Value"  , i, j); SDB_COMMAND_PARAM(buf, items[i].attributes[j].value);
		}
	}
	
	SDB_COMMAND_EXECUTE("BatchPutAttributes");
}


/**
 * Replace attributes of several items in the database
 * 
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param num the number of items
 * @param items the array of items and their attributes
 * @return SDB_OK if no errors occurred
 */
int sdb_replace_batch(struct SDB* sdb, const char* domain, size_t num, const struct sdb_item* items)
{
	size_t i, attrs;
	int j;
	char buf[64];
	
	attrs = 0;
	for (i = 0; i < num; i++) {
		attrs += items[i].size;
	}
	
	SDB_COMMAND_PREPARE(8 + num + attrs * 3);
	SDB_COMMAND_PARAM("DomainName", domain);
	
	for (i = 0; i < num; i++) {
		sprintf(buf, "Item.%d.ItemName", i); SDB_COMMAND_PARAM(buf, items[i].name);
		for (j = 0; j < items[i].size; j++) {
			sprintf(buf, "Item.%d.Attribute.%d.Name"   , i, j); SDB_COMMAND_PARAM(buf, items[i].attributes[j].name);
			sprintf(buf, "Item.%d.Attribute.%d.Value"  , i, j); SDB_COMMAND_PARAM(buf, items[i].attributes[j].value);
			sprintf(buf, "Item.%d.Attribute.%d.Replace", i, j); SDB_COMMAND_PARAM(buf, "true");
		}
	}
	
	SDB_COMMAND_EXECUTE("BatchPutAttributes");
}


/**
 * Get an attribute
 * 
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @param key the attribute name
 * @param response a pointer to the place to store the response
 * @return SDB_OK if no errors occurred
 */
int sdb_get(struct SDB* sdb, const char* domain, const char* item, const char* key, struct sdb_response** response)
{
	SDB_COMMAND_PREPARE(8);
	SDB_COMMAND_PARAM("ItemName", item);
	SDB_COMMAND_PARAM("DomainName", domain);
	SDB_COMMAND_PARAM("AttributeName.0", key);
	SDB_COMMAND_EXECUTE_RS("GetAttributes");
}


/**
 * Get several attributes
 * 
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @param num the number of attributes
 * @param keys the attribute keys (names)
 * @param response a pointer to the place to store the response
 * @return SDB_OK if no errors occurred
 */
int sdb_get_many(struct SDB* sdb, const char* domain, const char* item, size_t num, const char** keys, struct sdb_response** response)
{
	size_t i;
	char buf[64];
	
	SDB_COMMAND_PREPARE(8 + num);
	SDB_COMMAND_PARAM("ItemName", item);
	SDB_COMMAND_PARAM("DomainName", domain);
	
	for (i = 0; i < num; i++) {
		sprintf(buf, "AttributeName.%d", i); SDB_COMMAND_PARAM(buf, keys[i]);
	}
	
	SDB_COMMAND_EXECUTE_RS("GetAttributes");
}


/**
 * Get all attributes
 * 
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @param response a pointer to the place to store the response
 * @return SDB_OK if no errors occurred
 */
int sdb_get_all(struct SDB* sdb, const char* domain, const char* item, struct sdb_response** response)
{
	SDB_COMMAND_PREPARE(8);
	SDB_COMMAND_PARAM("ItemName", item);
	SDB_COMMAND_PARAM("DomainName", domain);
	SDB_COMMAND_EXECUTE_RS("GetAttributes");
}


/**
 * Query the database and return the names of the matching records
 * 
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param query the query expression 
 * @param response a pointer to the place to store the response
 * @return SDB_OK if no errors occurred
 */
int sdb_query(struct SDB* sdb, const char* domain, const char* query, struct sdb_response** response)
{
	SDB_COMMAND_PREPARE(8);
	SDB_COMMAND_PARAM("DomainName", domain);
	SDB_COMMAND_PARAM("QueryExpression", query);
	SDB_COMMAND_EXECUTE_RS("Query");
}


/**
 * Query with attributes: return one attribute
 * 
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param query the query expression 
 * @param key the attribute name to return
 * @param response a pointer to the place to store the response
 * @return SDB_OK if no errors occurred
 */
int sdb_query_attr(struct SDB* sdb, const char* domain, const char* query, const char* key, struct sdb_response** response)
{
	SDB_COMMAND_PREPARE(8);
	SDB_COMMAND_PARAM("DomainName", domain);
	SDB_COMMAND_PARAM("QueryExpression", query);
	SDB_COMMAND_PARAM("AttributeName.0", key);
	SDB_COMMAND_EXECUTE_RS("QueryWithAttributes");
}


/**
 * Query with attributes: return several attributes
 * 
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param query the query expression 
 * @param num the number of attributes
 * @param keys the names of attributes to return
 * @param response a pointer to the place to store the response
 * @return SDB_OK if no errors occurred
 */
int sdb_query_attr_many(struct SDB* sdb, const char* domain, const char* query,
						size_t num, const char** keys, struct sdb_response** response)
{
	size_t i;
	char buf[64];
	
	SDB_COMMAND_PREPARE(8 + num);
	SDB_COMMAND_PARAM("DomainName", domain);
	SDB_COMMAND_PARAM("QueryExpression", query);
	
	for (i = 0; i < num; i++) {
		sprintf(buf, "AttributeName.%d", i); SDB_COMMAND_PARAM(buf, keys[i]);
	}
	
	SDB_COMMAND_EXECUTE_RS("QueryWithAttributes");
}


/**
 * Query with attributes: return all attributes
 * 
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param query the query expression 
 * @param response a pointer to the place to store the response
 * @return SDB_OK if no errors occurred
 */
int sdb_query_attr_all(struct SDB* sdb, const char* domain, const char* query, struct sdb_response** response)
{
	SDB_COMMAND_PREPARE(8);
	SDB_COMMAND_PARAM("DomainName", domain);
	SDB_COMMAND_PARAM("QueryExpression", query);
	SDB_COMMAND_EXECUTE_RS("QueryWithAttributes");
}


/**
 * Query using a SELECT expression
 * 
 * @param sdb the SimpleDB handle
 * @param expr the select expression 
 * @param response a pointer to the place to store the response
 * @return SDB_OK if no errors occurred
 */
int sdb_select(struct SDB* sdb, const char* expr, struct sdb_response** response)
{
	SDB_COMMAND_PREPARE(8);
	SDB_COMMAND_PARAM("SelectExpression", expr);
	SDB_COMMAND_EXECUTE_RS("Select");
}


/**
 * Perform all pending operations specified using sdb_multi_* functions
 * 
 * @param sdb the SimpleDB handle
 * @param response a pointer to the place to store the response
 * @return SDB_OK if no errors occurred
 */
int sdb_multi_run(struct SDB* sdb, struct sdb_multi_response** response)
{
	*response = NULL;
	if (sdb->multi == NULL) return SDB_OK;
	
	
	// Perform the database calls
	
	int r = sdb_multi_run_and_wait(sdb);
	if (SDB_FAILED(r)) {
		sdb_multi_free_chain(sdb, sdb->multi);
		sdb->multi = NULL;
		return r;
	}
	
	
	// Get the results
	
	int i;
	int remaining = 1;
	int index = -1;
	
	int has_more = FALSE;
	struct sdb_retry_data* retry_list = NULL;
	
	while (remaining) {
		
		index++;
		
		
		// Get the message
		
		CURLMsg* msg = curl_multi_info_read(sdb->curl_multi, &remaining);
		if (msg == NULL) {
			sdb_multi_free_chain(sdb, sdb->multi);
			sdb_retry_destroy_chain(retry_list);
			sdb->multi = NULL;
			return SDB_E_CURL_INTERNAL_ERROR;
		}
		
		
		// Allocate the response structure if this is the first iteration
		
		if (*response == NULL) {
			*response = (struct sdb_multi_response*) malloc(sizeof(struct sdb_multi_response));
			(*response)->size = remaining + 1;
			(*response)->responses = (struct sdb_response**) malloc(sizeof(struct sdb_response*) * (*response)->size);
			for (i = 0; i < (*response)->size; i++) (*response)->responses[i] = NULL; 
		}
		
		assert(index < (*response)->size);
		
		
		// Deal with the error code
		
		CURLcode cr = msg->data.result;
		if (cr != CURLE_OK) continue;
		
		
		// Find the result structure
		
		struct sdb_multi_data* m = sdb_multi_find(sdb->multi, msg->easy_handle);
		if (m == NULL) {
			if (sdb->errout != NULL) fprintf(sdb->errout, "SimpleDB Internal Error: Cannot find multi handle %p\n", msg->easy_handle);
			sdb_multi_free_chain(sdb, sdb->multi);
			sdb_retry_destroy_chain(retry_list);
			sdb->multi = NULL;
			return SDB_E_INTERNAL_ERROR;
		}
		
		
		// Parse the result
		
		int retry = FALSE;
		
		r = sdb_parse_result(sdb, msg->easy_handle, m->post_size, &m->rec, &(*response)->responses[index]);
		if ((*response)->responses[index] == NULL && r != SDB_E_AWS_SERVICE_UNAVAILABLE) {
			(*response)->responses[index] = (struct sdb_response*) malloc(sizeof(struct sdb_response));
			memset((*response)->responses[index], 0, sizeof(struct sdb_response));
			(*response)->responses[index]->error = r;
		}
		
		if ((*response)->responses[index] != NULL) {
			(*response)->responses[index]->multi_handle = msg->easy_handle;
			(*response)->responses[index]->return_code = r;
			
			if ((*response)->responses[index]->has_more && sdb->auto_next) {
				retry = TRUE;
				has_more = TRUE;
			}
				
			
			// Save the parameters in the case manual NEXT handling is allowed
			
			if ((*response)->responses[index]->has_more) {
				if (!sdb->auto_next && (*response)->responses[index]->internal->params == NULL) {
					if ((*response)->responses[index]->internal->next == NULL) {
						(*response)->responses[index]->internal->params = sdb_params_deep_copy(m->params);
						(*response)->responses[index]->internal->command = (char*) malloc(strlen(m->command) + 4);
						strcpy((*response)->responses[index]->internal->command, m->command);
					}
					else if ((*response)->responses[index]->internal->params == NULL) {
						(*response)->responses[index]->internal->params = (*response)->responses[index]->internal->next->params;
						(*response)->responses[index]->internal->command = (*response)->responses[index]->internal->next->command;
						(*response)->responses[index]->internal->next->params = NULL;
						(*response)->responses[index]->internal->next->command = NULL;
						assert((*response)->responses[index]->internal->params);
						assert((*response)->responses[index]->internal->command);
					}
				}
			}
		}
		
		if (r == SDB_E_AWS_SERVICE_UNAVAILABLE) {
			retry = TRUE;
			if (sdb->retry_count > 0) sdb->stat.num_retries++;
		}
		
		
		// Schedule a retry if we have to
		
		if (retry) {
			
			struct sdb_retry_data* x = (struct sdb_retry_data*) malloc(sizeof(struct sdb_retry_data));
			
			x->next = retry_list;
			retry_list = x;
			
			strncpy(x->command, m->command, SDB_LEN_COMMAND - 1);
			x->command[SDB_LEN_COMMAND - 1] = '\0';
			
			x->user_data = &(*response)->responses[index];
			x->user_data_2 = msg->easy_handle;
			
			x->params = m->params;
			m->params = NULL;
		}
	}
	
	
	// Cleanup
	
	sdb_multi_free_chain(sdb, sdb->multi);
	sdb->multi = NULL;
	
	
	// Retry
	
	int ri;
	struct sdb_retry_data* R;
	
	if (!sdb->auto_next) has_more = FALSE;
	
	for (ri = 0; has_more || ri < sdb->retry_count; has_more ? ri : ri++) {
		if (retry_list == NULL) break;
		if (!has_more) usleep(sdb->retry_delay);
		
		
		// Rebuild the commands
		
		int num_retry_cmds = 0;
		for (R = retry_list; R != NULL; R = R->next) {
			struct sdb_response** pres = (struct sdb_response**) R->user_data;
			char* next_token = *pres == NULL ? NULL : (*pres)->internal->next_token;
			if (sdb_execute_multi(sdb, R->command, R->params, next_token, R->user_data, R->user_data_2) == SDB_MULTI_ERROR) {
				sdb_multi_free_chain(sdb, sdb->multi);
				sdb_retry_destroy_chain(retry_list);
				sdb->multi = NULL;
				return SDB_E_RETRY_FAILED;
			}
			num_retry_cmds++;
		}
		
		sdb_retry_destroy_chain(retry_list);
		retry_list = NULL;
		has_more = FALSE;
		
		
		// Perform the database calls
		
		int r = sdb_multi_run_and_wait(sdb);
		if (SDB_FAILED(r)) {
			sdb_multi_free_chain(sdb, sdb->multi);
			sdb->multi = NULL;
			return r;
		}
		
		
		// Parse the results
		
		remaining = 1;
		int num_retries = 0;
		
		while (remaining) {
			
			
			// Get the message
			
			CURLMsg* msg = curl_multi_info_read(sdb->curl_multi, &remaining);
			if (msg == NULL) {
				sdb_multi_free_chain(sdb, sdb->multi);
				sdb_retry_destroy_chain(retry_list);
				sdb->multi = NULL;
				return SDB_E_CURL_INTERNAL_ERROR;
			}
			
			
			// Deal with the error code
			
			CURLcode cr = msg->data.result;
			if (cr != CURLE_OK) continue;
			
			
			// Find the result structure
			
			struct sdb_multi_data* m = sdb_multi_find(sdb->multi, msg->easy_handle);
			if (m == NULL) {
				if (sdb->errout != NULL) fprintf(sdb->errout, "SimpleDB Internal Error: Cannot find multi handle %p\n", msg->easy_handle);
				sdb_multi_free_chain(sdb, sdb->multi);
				sdb_retry_destroy_chain(retry_list);
				sdb->multi = NULL;
				return SDB_E_INTERNAL_ERROR;
			}
			
			struct sdb_response** pres = (struct sdb_response**) m->user_data;
			
			
			// Parse the result
			
			int retry = FALSE;
			
			r = sdb_parse_result(sdb, msg->easy_handle, m->post_size, &m->rec, pres);
			if (*pres == NULL && r != SDB_E_AWS_SERVICE_UNAVAILABLE) {
				*pres = (struct sdb_response*) malloc(sizeof(struct sdb_response));
				memset(*pres, 0, sizeof(struct sdb_response));
				(*pres)->error = r;
			}
			
			if (*pres != NULL) {
				(*pres)->multi_handle = m->user_data_2;
				(*pres)->return_code = r;
				
				if ((*pres)->has_more && sdb->auto_next) {
					retry = TRUE;
					has_more = TRUE;
				}
				
				
				// Save the parameters in the case manual NEXT handling is allowed
				
				if ((*pres)->has_more && !sdb->auto_next && (*pres)->internal->params == NULL) {
					if ((*pres)->internal->next == NULL) {
						(*pres)->internal->params = sdb_params_deep_copy(m->params);
						(*pres)->internal->command = (char*) malloc(strlen(m->command) + 4);
						strcpy((*pres)->internal->command, m->command);
					}
					else {
						(*pres)->internal->params = (*pres)->internal->next->params;
						(*pres)->internal->command = (*pres)->internal->next->command;
						(*pres)->internal->next->params = NULL;
						(*pres)->internal->next->command = NULL;
						assert((*pres)->internal->params);
						assert((*pres)->internal->command);
					}
				}
			}
			
			if (r == SDB_E_AWS_SERVICE_UNAVAILABLE) {
				retry = TRUE;
				num_retries++;
			}
			
			
			// Schedule a retry if we have to
			
			if (retry) {
				
				struct sdb_retry_data* x = (struct sdb_retry_data*) malloc(sizeof(struct sdb_retry_data));
				
				x->next = retry_list;
				retry_list = x;
				
				strncpy(x->command, m->command, SDB_LEN_COMMAND - 1);
				x->command[SDB_LEN_COMMAND - 1] = '\0';
				
				x->user_data = m->user_data;
				x->user_data_2 = m->user_data_2;
				
				x->params = m->params;
				m->params = NULL;
			}
		}
		
		if (has_more || (ri + 1) < sdb->retry_count) sdb->stat.num_retries += num_retries;
		
		
		// Cleanup
		
		sdb_multi_free_chain(sdb, sdb->multi);
		sdb->multi = NULL;
	}
	
	
	// Cleanup
	
	sdb_multi_free_chain(sdb, sdb->multi);
	sdb_retry_destroy_chain(retry_list);
	sdb->multi = NULL;
	
	return SDB_OK;
}


/**
 * Create a domain
 * 
 * @param sdb the SimpleDB handle
 * @param name the domain name
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_create_domain(struct SDB* sdb, const char* name)
{
	SDB_COMMAND_PREPARE(8);
	SDB_COMMAND_PARAM_MULTI("DomainName", name);
	SDB_COMMAND_EXECUTE_MULTI("CreateDomain");
}


/**
 * Delete a domain
 * 
 * @param sdb the SimpleDB handle
 * @param name the domain name
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_delete_domain(struct SDB* sdb, const char* name)
{
	SDB_COMMAND_PREPARE(8);
	SDB_COMMAND_PARAM_MULTI("DomainName", name);
	SDB_COMMAND_EXECUTE_MULTI("DeleteDomain");
}


/**
 * List domains
 * 
 * @param sdb the SimpleDB handle
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_list_domains(struct SDB* sdb)
{
	SDB_COMMAND_PREPARE(8);
	SDB_COMMAND_EXECUTE_MULTI("ListDomains");
}


/**
 * Get domain meta-data
 * 
 * @param sdb the SimpleDB handle
 * @param name the domain name
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_domain_metadata(struct SDB* sdb, const char* name)
{
	SDB_COMMAND_PREPARE(8);
	SDB_COMMAND_PARAM_MULTI("DomainName", name);
	SDB_COMMAND_EXECUTE_MULTI("DomainMetadata");
}


/**
 * Put an attribute to the database
 * 
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @param key the attribute key
 * @param value the attribute value
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_put(struct SDB* sdb, const char* domain, const char* item, const char* key, const char* value)
{
	SDB_COMMAND_PREPARE(8);
	SDB_COMMAND_PARAM_MULTI("ItemName", item);
	SDB_COMMAND_PARAM_MULTI("DomainName", domain);
	SDB_COMMAND_PARAM_MULTI("Attribute.0.Name", key);
	SDB_COMMAND_PARAM_MULTI("Attribute.0.Value", value);
	SDB_COMMAND_EXECUTE_MULTI("PutAttributes");
}


/**
 * Replace an attribute in the database
 * 
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @param key the attribute key
 * @param value the attribute value
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_replace(struct SDB* sdb, const char* domain, const char* item, const char* key, const char* value)
{
	SDB_COMMAND_PREPARE(8);
	SDB_COMMAND_PARAM_MULTI("ItemName", item);
	SDB_COMMAND_PARAM_MULTI("DomainName", domain);
	SDB_COMMAND_PARAM_MULTI("Attribute.0.Name", key);
	SDB_COMMAND_PARAM_MULTI("Attribute.0.Value", value);
	SDB_COMMAND_PARAM_MULTI("Attribute.0.Replace", "true");
	SDB_COMMAND_EXECUTE_MULTI("PutAttributes");
}


/**
 * Put several attributes to the database
 * 
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @param num the number of attributes
 * @param keys the attribute keys
 * @param values the attribute values
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_put_many(struct SDB* sdb, const char* domain, const char* item, size_t num, const char** keys, const char** values)
{
	size_t i;
	char buf[64];
	
	SDB_COMMAND_PREPARE(8 + num * 2);
	SDB_COMMAND_PARAM_MULTI("ItemName", item);
	SDB_COMMAND_PARAM_MULTI("DomainName", domain);
	
	for (i = 0; i < num; i++) {
		sprintf(buf, "Attribute.%d.Name"   , i); SDB_COMMAND_PARAM_MULTI(buf, keys[i]);
		sprintf(buf, "Attribute.%d.Value"  , i); SDB_COMMAND_PARAM_MULTI(buf, values[i]);
	}
	
	SDB_COMMAND_EXECUTE_MULTI("PutAttributes");
}


/**
 * Replace several attributes in the database
 * 
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @param num the number of attributes
 * @param keys the attribute keys
 * @param values the attribute values
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_replace_many(struct SDB* sdb, const char* domain, const char* item, size_t num, const char** keys, const char** values)
{
	size_t i;
	char buf[64];
	
	SDB_COMMAND_PREPARE(8 + num * 3);
	SDB_COMMAND_PARAM_MULTI("ItemName", item);
	SDB_COMMAND_PARAM_MULTI("DomainName", domain);
	
	for (i = 0; i < num; i++) {
		sprintf(buf, "Attribute.%d.Name"   , i); SDB_COMMAND_PARAM_MULTI(buf, keys[i]);
		sprintf(buf, "Attribute.%d.Value"  , i); SDB_COMMAND_PARAM_MULTI(buf, values[i]);
		sprintf(buf, "Attribute.%d.Replace", i); SDB_COMMAND_PARAM_MULTI(buf, "true");
	}
	
	SDB_COMMAND_EXECUTE_MULTI("PutAttributes");
}


/**
 * Get an attribute
 * 
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @param key the attribute name
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_get(struct SDB* sdb, const char* domain, const char* item, const char* key)
{
	SDB_COMMAND_PREPARE(8);
	SDB_COMMAND_PARAM_MULTI("ItemName", item);
	SDB_COMMAND_PARAM_MULTI("DomainName", domain);
	SDB_COMMAND_PARAM_MULTI("AttributeName.0", key);
	SDB_COMMAND_EXECUTE_MULTI("GetAttributes");
}


/**
 * Get several attributes
 * 
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @param num the number of attributes
 * @param keys the attribute keys (names)
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_get_many(struct SDB* sdb, const char* domain, const char* item, size_t num, const char** keys)
{
	size_t i;
	char buf[64];
	
	SDB_COMMAND_PREPARE(8 + num);
	SDB_COMMAND_PARAM_MULTI("ItemName", item);
	SDB_COMMAND_PARAM_MULTI("DomainName", domain);
	
	for (i = 0; i < num; i++) {
		sprintf(buf, "AttributeName.%d", i); SDB_COMMAND_PARAM_MULTI(buf, keys[i]);
	}
	
	SDB_COMMAND_EXECUTE_MULTI("GetAttributes");
}


/**
 * Get all attributes
 * 
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_get_all(struct SDB* sdb, const char* domain, const char* item)
{
	SDB_COMMAND_PREPARE(8);
	SDB_COMMAND_PARAM_MULTI("ItemName", item);
	SDB_COMMAND_PARAM_MULTI("DomainName", domain);
	SDB_COMMAND_EXECUTE_MULTI("GetAttributes");
}


/**
 * Query the database and return the names of the matching records
 * 
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param query the query expression 
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_query(struct SDB* sdb, const char* domain, const char* query)
{
	SDB_COMMAND_PREPARE(8);
	SDB_COMMAND_PARAM_MULTI("DomainName", domain);
	SDB_COMMAND_PARAM_MULTI("QueryExpression", query);
	SDB_COMMAND_EXECUTE_MULTI("Query");
}


/**
 * Query with attributes: return one attribute
 * 
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param query the query expression 
 * @param key the attribute name to return
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_query_attr(struct SDB* sdb, const char* domain, const char* query, const char* key)
{
	SDB_COMMAND_PREPARE(8);
	SDB_COMMAND_PARAM_MULTI("DomainName", domain);
	SDB_COMMAND_PARAM_MULTI("QueryExpression", query);
	SDB_COMMAND_PARAM_MULTI("AttributeName.0", key);
	SDB_COMMAND_EXECUTE_MULTI("QueryWithAttributes");
}


/**
 * Query with attributes: return several attributes
 * 
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param query the query expression 
 * @param num the number of attributes
 * @param keys the names of attributes to return
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_query_attr_many(struct SDB* sdb, const char* domain, const char* query,
									size_t num, const char** keys)
{
	size_t i;
	char buf[64];
	
	SDB_COMMAND_PREPARE(8 + num);
	SDB_COMMAND_PARAM_MULTI("DomainName", domain);
	SDB_COMMAND_PARAM_MULTI("QueryExpression", query);
	
	for (i = 0; i < num; i++) {
		sprintf(buf, "AttributeName.%d", i); SDB_COMMAND_PARAM_MULTI(buf, keys[i]);
	}
	
	SDB_COMMAND_EXECUTE_MULTI("QueryWithAttributes");
}


/**
 * Query with attributes: return all attributes
 * 
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param query the query expression 
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_query_attr_all(struct SDB* sdb, const char* domain, const char* query)
{
	SDB_COMMAND_PREPARE(8);
	SDB_COMMAND_PARAM_MULTI("DomainName", domain);
	SDB_COMMAND_PARAM_MULTI("QueryExpression", query);
	SDB_COMMAND_EXECUTE_MULTI("QueryWithAttributes");
}


/**
 * Query using a SELECT expression
 * 
 * @param sdb the SimpleDB handle
 * @param expr the select expression 
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_select(struct SDB* sdb, const char* expr)
{
	SDB_COMMAND_PREPARE(8);
	SDB_COMMAND_PARAM_MULTI("SelectExpression", expr);
	SDB_COMMAND_EXECUTE_MULTI("Select");
}
