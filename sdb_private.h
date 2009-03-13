/*
 * sdb_private.h
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

#ifndef __SDB_PRIVATE_H
#define __SDB_PRIVATE_H

#include "sdb.h"
#include "response.h"

#include <curl/curl.h>
#include <curl/easy.h>

#ifdef __cplusplus
extern "C" {
#endif


#define AWS_URL							"http://sdb.amazonaws.com"

#define SDB_HTTP_HEADER_CONTENT_TYPE	"Content-Type: application/x-www-form-urlencoded; charset=utf-8"
#define SDB_HTTP_HEADER_USER_AGENT		"User-Agent: libsdb"

#define SDB_MAX_MULTI_FREE				256
#define SDB_LEN_COMMAND					32


// Curl's cutoff for using 100-continue for POST

#define TINY_INITIAL_POST_SIZE 1024


struct sdb_params;


/**
 * A key-value pair
 */
struct sdb_pair
{
	char* key;
	char* value;
	struct sdb_params* parent;
};


/**
 * An array of key-value pairs
 */
struct sdb_params
{
	struct sdb_pair* params;
	size_t size;
	size_t capacity;
};


/**
 * A buffer
 */
struct sdb_buffer
{
	char* buffer;
	size_t size;
	size_t capacity;
};


/**
 * A data structure for the multi interface
 */
struct sdb_multi_data
{
	// Execution data
	
	CURL* curl;
	char* post;
	struct sdb_buffer rec;
	
	
	// Data for retry
	
	char command[32];
	struct sdb_params* params;
	
	
	// Linked list
	
	struct sdb_multi_data* next;
	
	
	// User data
	
	void* user_data;
	void* user_data_2;
	
	
	// Statistics
	
	long post_size;
};


/**
 * All we need to remember to do a retry
 */
struct sdb_retry_data
{
	char command[SDB_LEN_COMMAND];
	struct sdb_params* params;
	
	void* user_data;
	void* user_data_2;
	
	struct sdb_retry_data* next;
};


/**
 * A SimpleDB handle
 */
struct SDB
{
	// Curl
	
	CURL* curl_handle;
	CURLM* curl_multi;
	
	struct curl_slist *curl_headers;
	
	
	// SimpleDB Authentication
	
	char* sdb_key;
	char* sdb_secret;
	
	size_t sdb_key_len;
	size_t sdb_secret_len;
	
	int sdb_signature_ver;
	char sdb_signature_ver_str[2];
	
	
	// Buffer for receiving data
	
	struct sdb_buffer rec;
	
	
	// Multi interface
	
	struct sdb_multi_data* multi;
	struct sdb_multi_data* multi_free;
	int multi_free_size;
	
	
	// Retry configuration
	
	int retry_count;
	long retry_delay;
	
	
	// Other configuration
	
	FILE* errout;
	
	
	// Statistics
	
	struct sdb_statistics stat;
};


/**
 * Create a Curl handle
 * 
 * @param sdb the SimpleDB handle
 * @return the Curl handle, or NULL on error
 */
CURL* sdb_create_curl(struct SDB* sdb);

/**
 * Allocate a multi data structure
 * 
 * @param sdb the SimpleDB handle
 * @return the allocated data structure, or NULL on error
 */
struct sdb_multi_data* sdb_multi_alloc(struct SDB* sdb);

/**
 * Free a multi data structure, reusing it if possible
 * 
 * @param sdb the SimpleDB handle
 * @param m the data structure to free
 */
void sdb_multi_free_one(struct SDB* sdb, struct sdb_multi_data* m); 

/**
 * Free a chain of multi data structure, reusing it if possible
 * 
 * @param sdb the SimpleDB handle
 * @param m the first data structure of the chain
 */
void sdb_multi_free_chain(struct SDB* sdb, struct sdb_multi_data* m); 

/**
 * Destroy a chain of multi data structures
 * 
 * @param sdb the SimpleDB handle
 * @param m the first data structure in the chain
 */
void sdb_multi_destroy(struct SDB* sdb, struct sdb_multi_data* m);

/**
 * Find the multi data structure based on the Curl handle
 * 
 * @param chain the beginning of the chain
 * @param curl the Curl handle
 * @return the data structure, or NULL if not found
 */
struct sdb_multi_data* sdb_multi_find(struct sdb_multi_data* chain, CURL* curl);

/**
 * Create a formatted UTC timestamp
 * 
 * @param buffer the buffer to which to write the timestamp (must be at least 32 bytes long)
 * @return SDB_OK if no errors occurred
 */
int sdb_timestamp(char* buffer);

/**
 * Sign a string
 * 
 * @param sdb the SimpleDB handle
 * @param str the string to sign
 * @param buffer the buffer to write the signature to (must be at least EVP_MAX_MD_SIZE * 2 bytes)
 * @param plen the pointer to the place to store the length of the signature (can be NULL)
 * @return SDB_OK if no errors occurred
 */
int sdb_sign(struct SDB* sdb, const char* str, char* buffer, size_t* plen);

/**
 * Allocate an array for storing SimpleDB parameters
 * 
 * @param capacity the estimated capacity
 * @return the array
 */ 
struct sdb_params* sdb_params_alloc(size_t capacity);

/**
 * Free a parameters array
 * 
 * @param params the arrays to free
 */
void sdb_params_free(struct sdb_params* params);

/**
 * Add a parameter (this routine will make a copy of its inputs)
 * 
 * @param params the parameter array
 * @param key the key
 * @param value the value
 * @return SDB_OK if no errors occurred
 */
int sdb_params_add(struct sdb_params* params, const char* key, const char* value);

/**
 * Add elements from another parameter array without making a copy of the inputs
 * 
 * @param params the parameter array
 * @param other the other parameter array
 * @return SDB_OK if no errors occurred
 */
int sdb_params_add_all(struct sdb_params* params, struct sdb_params* other);

/**
 * Sort the parameters by the keys
 * 
 * @param params the parameter array
 * @return SDB_OK if no errors occurred
 */
int sdb_params_sort(struct sdb_params* params);

/**
 * Sign the parameters
 * 
 * @param sdb the SimpleDB handle
 * @param params the parameter array
 * @param buffer the buffer to write the signature to (must be at least EVP_MAX_MD_SIZE * 2 bytes)
 * @param plen the pointer to the place to store the length of the signature (can be NULL)
 * @return SDB_OK if no errors occurred
 */
int sdb_params_sign(struct SDB* sdb, struct sdb_params* params, char* buffer, size_t* plen);

/**
 * Create the URL-encoded parameters string
 * 
 * @param sdb the SimpleDB handle
 * @param params the parameter array
 * @param pbuffer the variable for the output buffer (will be allocated by the routine, but has to be freed by caller)
 * @return SDB_OK if no errors occurred
 */
int sdb_params_export(struct SDB* sdb, struct sdb_params* params, char** pbuffer);

/**
 * Add required SimpleDB parameters to the array
 * 
 * @param sdb the SimpleDB handle
 * @param params the parameter array
 * @return SDB_OK if no errors occurred
 */
int sdb_params_add_required(struct SDB* sdb, struct sdb_params* params);

/**
 * Deep copy the parameters
 * 
 * @param params the parameter array
 * @return the deep copy
 */
struct sdb_params* sdb_params_deep_copy(struct sdb_params* params);

/**
 * Create the command URL
 * 
 * @param sdb the SimpleDB handle
 * @param params the parameter array
 * @return the URL, or NULL on error
 */
char* sdb_url(struct SDB* sdb, struct sdb_params* params);

/**
 * Create the command POST data
 * 
 * @param sdb the SimpleDB handle
 * @param params the parameter array
 * @return the URL, or NULL on error
 */
char* sdb_post(struct SDB* sdb, struct sdb_params* params);

/**
 * The Curl write callback function
 * 
 * @param buffer the received buffer
 * @param size the size of an item
 * @param num the number of items
 * @param data the user data
 * @return the number of processed bytes
 */
size_t sdb_write_callback(void* buffer, size_t size, size_t num, void* data);

/**
 * Execute a command and ignore the result-set
 * 
 * @param sdb the SimpleDB handle
 * @param cmd the command name
 * @param params the parameters
 * @return the result
 */
int sdb_execute(struct SDB* sdb, const char* cmd, struct sdb_params* params);

/**
 * Execute a command
 * 
 * @param sdb the SimpleDB handle
 * @param cmd the command name
 * @param params the parameters
 * @param result the pointer to the result-set
 * @return the result
 */
int sdb_execute_rs(struct SDB* sdb, const char* cmd, struct sdb_params* params, struct sdb_response** response);

/**
 * Execute a command using Curl's multi interface
 * 
 * @param sdb the SimpleDB handle
 * @param cmd the command name
 * @param params the parameters
 * @param next_token the next token (optional)
 * @param user_data the user data (optional)
 * @param user_data_2 the user data (optional)
 * @return the handle to the deferred call, or SDB_MULTI_ERROR on error 
 */
sdb_multi sdb_execute_multi(struct SDB* sdb, const char* cmd, struct sdb_params* params, char* next_token, void* user_data, void* user_data_2);

/**
 * Run all deferred multi calls and wait for the result
 * 
 * @param sdb the SimpleDB handle
 * @return the result
 */
int sdb_multi_run_and_wait(struct SDB* sdb);

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
int sdb_parse_result(struct SDB* sdb, CURL* curl, long post_size, struct sdb_buffer* rec, struct sdb_response** response);

/**
 * Destroy a retry chain
 * 
 * @param retry the front of the chain
 */
void sdb_retry_destroy_chain(struct sdb_retry_data* retry);

/**
 * Update the size fields of the statistics (not the command count)
 * 
 * @param sdb the SimpleDB handle
 * @param curl the Curl handle
 * @param post_size the length of the post data
 * @param rec_size the length of the received data
 */ 
void sdb_update_size_stats(struct SDB* sdb, CURL* curl, long post_size, long rec_size);

/**
 * Add two statistics objects
 * 
 * @param a the first object (will be modified)
 * @param b the second object
 */
void sdb_add_statistics(struct sdb_statistics* a, const struct sdb_statistics* b);

/**
 * Estimate the HTTP overhead for sending data
 * 
 * @param sdb the SimpleDB handle
 * @param post_size the length of the post data
 * @return the estimated overhead in bytes
 */ 
long sdb_estimate_http_sent(struct SDB* sdb, long post_size);

/**
 * Estimate the HTTP overhead for receiving data
 * 
 * @param sdb the SimpleDB handle
 * @param response_size the length of the response data
 * @return the estimated overhead in bytes
 */ 
long sdb_estimate_http_received(struct SDB* sdb, long response_size);


#ifdef __cplusplus
}
#endif

#endif
