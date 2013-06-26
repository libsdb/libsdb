/*
 * sdb.h
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

#ifndef __SDB_H
#define __SDB_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************/
/*                                                                           */
/*                           I N F O R M A T I O N                           */
/*                                                                           */
/*****************************************************************************/

/*
 * libsdb version
 */
#define SDB_VERSION "0.96"

/*
 * SimpleDB API Version Supported
 */
#define SDB_AWS_VERSION "2009-04-15"

/*
 * SimpleDB Regions
 *
 * See http://docs.aws.amazon.com/general/latest/gr/rande.html#sdb_region
 *
 */
#define US_East_Northern_Virginia_Region   "sdb.amazonaws.com"
#define US_West_Oregon_Region              "sdb.us-west-2.amazonaws.com"
#define US_West_Northern_California Region "sdb.us-west-1.amazonaws.com"
#define EU_Ireland_Region                  "sdb.eu-west-1.amazonaws.com"
#define Asia_Pacific_Singapore_Region      "sdb.ap-southeast-1.amazonaws.com"
#define Asia_Pacific_Sydney_Region         "sdb.ap-southeast-2.amazonaws.com"
#define Asia_Pacific_Tokyo_Region          "sdb.ap-northeast-1.amazonaws.com"
#define South_America_Sao_Paulo_Region     "sdb.sa-east-1.amazonaws.com"

#define AWS_REGION_PROTOCOL                "https://"

#define AWS_REGION US_East_Northern_Virginia_Region

/*****************************************************************************/
/*                                                                           */
/*                           E R R O R   C O D E S                           */
/*                                                                           */
/*****************************************************************************/

/*
 * Error codes
 */
#define SDB_OK						0
#define SDB_E_CURL_INIT_FAILED		-1
#define SDB_E_OPEN_SSL_FAILED		-2
#define SDB_E_CAPACITY_TOO_SMALL	-3
#define SDB_E_INVALID_SIGNATURE_VER	-4
#define SDB_E_URL_ENCODE_FAILED		-5
#define SDB_E_NOT_INITIALIZED		-6
#define SDB_E_ALREADY_INITIALIZED	-7
#define SDB_E_INVALID_XML_RESPONSE	-8
#define SDB_E_INVALID_ERR_RESPONSE	-9
#define SDB_E_INVALID_META_RESPONSE	-10
#define SDB_E_FD_ERROR				-11
#define SDB_E_INTERNAL_ERROR		-12
#define SDB_E_AWS_INTERNAL_ERROR_2	-13
#define SDB_E_CURL_INTERNAL_ERROR	-14
#define SDB_E_RETRY_FAILED			-15

#define SDB_CURL_ERROR(code)		(-1000 - (code))
#define SDB_CURLM_ERROR(code)		(-1500 - (code))
#define SDB_AWS_ERROR(code)			(-2000 - (code))

#define SDB_AWS_NUM_ERRORS			42
#define SDB_AWS_ERROR_NAME(code)	((code) > -2000 || (code) <= (-2000 - SDB_AWS_NUM_ERRORS) ? "UnknownError" : SDB_AWS_ERRORS[-((code) + 2000)])
extern const char* SDB_AWS_ERRORS[];

#define SDB_E_AWS_ACCESS_FAILURE							-2000
#define SDB_E_AWS_AUTH_FAILURE								-2001
#define SDB_E_AWS_AUTH_MISSING_FAILURE						-2002
#define SDB_E_AWS_FEATURE_DEPRECATED						-2003
#define SDB_E_AWS_INTERNAL_ERROR							-2004
#define SDB_E_AWS_INVALID_ACTION							-2005
#define SDB_E_AWS_INVALID_BATCH_REQUEST						-2006
#define SDB_E_AWS_INVALID_HTTP_AUTH_HEADER					-2007
#define SDB_E_AWS_INVALID_HTTP_REQUEST						-2008
#define SDB_E_AWS_INVALID_LITERAL							-2009
#define SDB_E_AWS_INVALID_NEXT_TOKEN						-2010
#define SDB_E_AWS_INVALID_NUMBER_PREDICATES					-2011
#define SDB_E_AWS_INVALID_NUMBER_VALUE_TESTS				-2012
#define SDB_E_AWS_INVALID_PARAMETER_COMBINATION				-2013
#define SDB_E_AWS_INVALID_PARAMETER_VALUE					-2014
#define SDB_E_AWS_INVALID_QUERY_EXPRESSION					-2015
#define SDB_E_AWS_INVALID_RESPONSE_GROUPS					-2016
#define SDB_E_AWS_INVALID_SERVICE							-2017
#define SDB_E_AWS_INVALID_SOAP_REQUEST						-2018
#define SDB_E_AWS_INVALID_URI								-2019
#define SDB_E_AWS_INVALID_WS_ADDRESSING_PROPERTY			-2020
#define SDB_E_AWS_MALFORMED_SOAP_SIGNATURE					-2021
#define SDB_E_AWS_MISSING_ACTION							-2022
#define SDB_E_AWS_MISSING_PARAMETER							-2023
#define SDB_E_AWS_MISSING_WS_ADDRESSING_PROPERTY			-2024
#define SDB_E_AWS_NO_SUCH_DOMAIN							-2025
#define SDB_E_AWS_NO_SUCH_VERSION							-2026
#define SDB_E_AWS_NOT_YET_IMPLEMENTED						-2027
#define SDB_E_AWS_NUMBER_DOMAINS_EXCEEDED					-2028
#define SDB_E_AWS_NUMBER_DOMAIN_ATTRIBUTES_EXCEEDED			-2029
#define SDB_E_AWS_NUMBER_DOMAIN_BYTES_EXCEEDED				-2030
#define SDB_E_AWS_NUMBER_ITEM_ATTRIBUTES_EXCEEDED			-2031
#define SDB_E_AWS_NUMBER_SUBMITTED_ATTRIBUTES_EXCEEDED		-2032
#define SDB_E_AWS_REQUEST_EXPIRED							-2033
#define SDB_E_AWS_REQUEST_TIMEOUT							-2034
#define SDB_E_AWS_SERVICE_UNAVAILABLE						-2035
#define SDB_E_AWS_SIGNATURE_DOES_NOT_MATCH					-2036
#define SDB_E_AWS_TOO_MANY_REQUESTED_ATTRIBUTES				-2037
#define SDB_E_AWS_UNSUPPORTED_HTTP_VERB						-2038
#define SDB_E_AWS_UNSUPPORTED_NEXT_TOKEN					-2039
#define SDB_E_AWS_URI_TOO_LONG								-2040
#define SDB_E_AWS_DUPLICATE_ITEM_NAME						-2041


/*
 * Error-related macros
 */
#define SDB_SUCCESS(x)				((x) == 0)
#define SDB_FAILED(x)				((x) != 0)
#define SDB_ASSERT(x)				{ int ____r = (x); assert(SDB_SUCCESS(____r)); }
#define SDB_SAFE(x)					{ int ____r = (x); if (SDB_FAILED(____r)) return ____r; }


/*
 * Error placeholders
 */
#define SDB_MULTI_ERROR				((void*) 0)



/*****************************************************************************/
/*                                                                           */
/*                          E N U M E R A T I O N S                          */
/*                                                                           */
/*****************************************************************************/

/*
 * Response types
 */
#define SDB_R_NONE					0
#define SDB_R_DOMAIN_LIST			1
#define SDB_R_DOMAIN_METADATA		2
#define SDB_R_ATTRIBUTE_LIST		3
#define SDB_R_ITEM_LIST				4




/*****************************************************************************/
/*                                                                           */
/*                      D A T A   S T R U C T U R E S                        */
/*                                                                           */
/*****************************************************************************/

/**
 * An internal response structure
 */
struct sdb_response_internal;


/**
 * A SimpleDB multi-interface handle
 */
typedef void* sdb_multi;


/**
 * Domain meta-data
 */
struct sdb_domain_metadata
{
	long timestamp;				// The date and time the metadata was last updated.
	long item_count;			// The number of all items in the domain.
	long attr_value_count;		// 	The number of all attribute name/value pairs in the domain.
	long attr_name_count; 		// The number of unique attribute names in the domain.
	long item_names_size;	 	// The total size of all item names in the domain, in bytes.
	long attr_values_size;	 	// The total size of all attribute values, in bytes.
	long attr_names_size;	 	// The total size of all unique attribute names, in bytes.
};


/**
 * An attribute
 */
struct sdb_attribute
{
	char* name;
	char* value;
};


/**
 * An item with attributes
 */
struct sdb_item
{
	char* name;
	int size;
	struct sdb_attribute* attributes;
};


/**
 * A response structure
 */
struct sdb_response
{
	// Result

	int size;
	int has_more;

	int type;

	union {
		char** domains;
		struct sdb_domain_metadata* domain_metadata;
		struct sdb_attribute* attributes;
		struct sdb_item* items;
	};


	// Errors

	int error;
	char* error_message;
	int num_errors;


	// Metadata

	double box_usage;


	// Metadata specific to the multi interface

	sdb_multi multi_handle;
	int return_code;


	// Internal data

	struct sdb_response_internal* internal;
};


/**
 * A multi-response structure
 */
struct sdb_multi_response
{
	int size;
	struct sdb_response** responses;
};


/**
 * Statistics
 */
struct sdb_statistics
{
	long long bytes_sent;
	long long bytes_received;
	long long http_overhead_sent;
	long long http_overhead_received;
	long long num_commands;
	long long num_puts;
	long long num_retries;
	long double box_usage;
};


/**
 * A SimpleDB handle
 */
struct SDB;


/*****************************************************************************/
/*                                                                           */
/*                G L O B A L   I N I T   &   C L E A N - U P                */
/*                                                                           */
/*****************************************************************************/

/**
 * Global initialization
 *
 * @return SDB_OK if no errors occurred
 */
int sdb_global_init(void);

/**
 * Global cleanup
 *
 * @return SDB_OK if no errors occurred
 */
int sdb_global_cleanup(void);

/**
 * Initialize the environment
 *
 * @param sdb a pointer to the SimpleDB handle
 * @param key the SimpleDB key
 * @param secret the SimpleDB secret key
 * @return SDB_OK if no errors occurred
 */
int sdb_init(struct SDB** sdb, const char* key, const char* secret, const char* region);

/**
 * Initialize the environment
 *
 * @param sdb a pointer to the SimpleDB handle
 * @param key the SimpleDB key
 * @param secret the SimpleDB secret key
 * @param service the service URL
 * @return SDB_OK if no errors occurred
 */
int sdb_init_ext(struct SDB** sdb, const char* key, const char* secret, const char* service);

/**
 * Destroy the environment
 *
 * @param sdb a pointer to the SimpleDB handle
 * @return SDB_OK if no errors occurred
 */
int sdb_destroy(struct SDB** sdb);



/*****************************************************************************/
/*                                                                           */
/*                              R E S P O N S E                              */
/*                                                                           */
/*****************************************************************************/

/**
 * Free the response structure
 *
 * @param response the data structure
 */
void sdb_free(struct sdb_response** response);

/**
 * Free the multi response structure
 *
 * @param response the data structure
 */
void sdb_multi_free(struct sdb_multi_response** response);

/**
 * Print the response
 *
 * @param r the data structure
 */
void sdb_print(struct sdb_response* r);

/**
 * Print the response
 *
 * @param f the output file
 * @param r the data structure
 */
void sdb_fprint(FILE* f, struct sdb_response* r);

/**
 * Count the number of failed commands in a multi response
 *
 * @param response the data structure
 * @return the number of failed commands
 */
int sdb_multi_count_errors(struct sdb_multi_response* response);



/*****************************************************************************/
/*                                                                           */
/*                         C O N F I G U R A T I O N                         */
/*                                                                           */
/*****************************************************************************/

/**
 * Set the output FILE for additional error messages
 *
 * @param sdb the SimpleDB handle
 * @param f the output FILE
 */
void sdb_set_error_file(struct SDB* sdb, FILE* f);

/**
 * Set the retry configuration
 *
 * @param sdb the SimpleDB handle
 * @param count the maximum number of retries
 * @param delay the number of milliseconds between two retries
 */
void sdb_set_retry(struct SDB* sdb, int count, int delay);

/**
 * Set automatic handling of the NEXT tokens
 *
 * @param sdb the SimpleDB handle
 * @param value zero disables automatic NEXT handling, a non-zero value enables it
 */
void sdb_set_auto_next(struct SDB* sdb, int value);

/**
 * Enable gzip Content-Encoding for all service requests.
 *
 * @param sdb the SimpleDB handle
 * @param value zero disabled gzip encoding, a non-zero value enables it
 */
void sdb_set_compression(struct SDB* sdb, int value);

/**
 * Set the User-Agent header for service requests.
 *
 * @param sdb the SimpleDB handle
 * @param ua string to use
 */
void sdb_set_useragent(struct SDB* sdb, const char* ua);


/*****************************************************************************/
/*                                                                           */
/*                            S T A T I S T I C S                            */
/*                                                                           */
/*****************************************************************************/

/**
 * Return the collected statistics
 *
 * @param sdb the SimpleDB handle
 * @return the struct with statistics
 */
const struct sdb_statistics* sdb_get_statistics(struct SDB* sdb);

/**
 * Print the statistics to a file
 *
 * @param sdb the SimpleDB handle
 * @param f the output FILE
 */
void sdb_fprint_statistics(struct SDB* sdb, FILE* f);

/**
 * Print the statistics
 *
 * @param sdb the SimpleDB handle
 */
void sdb_print_statistics(struct SDB* sdb);

/**
 * Reset (clear) the statistics
 *
 * @param sdb the SimpleDB handle
 */
void sdb_clear_statistics(struct SDB* sdb);


/*****************************************************************************/
/*                                                                           */
/*                              C O M M A N D S                              */
/*                                                                           */
/*****************************************************************************/

/**
 * Get more data (if the automatic handling of NEXT tokens is disabled)
 *
 * @param sdb the SimpleDB handle
 * @param response the result set of the previous call
 * @param append whether to append the data to the result set or replace it
 * @return SDB_OK if no errors occurred
 */
int sdb_next(struct SDB* sdb, struct sdb_response** response, int append);

/**
 * Create a domain
 *
 * @param sdb the SimpleDB handle
 * @param name the domain name
 * @return SDB_OK if no errors occurred
 */
int sdb_create_domain(struct SDB* sdb, const char* name);

/**
 * Delete a domain
 *
 * @param sdb the SimpleDB handle
 * @param name the domain name
 * @return SDB_OK if no errors occurred
 */
int sdb_delete_domain(struct SDB* sdb, const char* name);

/**
 * List domains
 *
 * @param sdb the SimpleDB handle
 * @param response a pointer to the place to store the response
 * @return SDB_OK if no errors occurred
 */
int sdb_list_domains(struct SDB* sdb, struct sdb_response** response);

/**
 * Get domain meta-data
 *
 * @param sdb the SimpleDB handle
 * @param name the domain name
 * @param response a pointer to the place to store the response
 * @return SDB_OK if no errors occurred
 */
int sdb_domain_metadata(struct SDB* sdb, const char* name, struct sdb_response** response);

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
int sdb_put(struct SDB* sdb, const char* domain, const char* item, const char* key, const char* value);

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
int sdb_replace(struct SDB* sdb, const char* domain, const char* item, const char* key, const char* value);

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
int sdb_put_many(struct SDB* sdb, const char* domain, const char* item, size_t num, const char** keys, const char** values);

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
int sdb_replace_many(struct SDB* sdb, const char* domain, const char* item, size_t num, const char** keys, const char** values);

/**
 * Put attributes of several items to the database
 *
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param num the number of items
 * @param items the array of items and their attributes
 * @return SDB_OK if no errors occurred
 */
int sdb_put_batch(struct SDB* sdb, const char* domain, size_t num, const struct sdb_item* items);

/**
 * Replace attributes of several items in the database
 *
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param num the number of items
 * @param items the array of items and their attributes
 * @return SDB_OK if no errors occurred
 */
int sdb_replace_batch(struct SDB* sdb, const char* domain, size_t num, const struct sdb_item* items);

/**
 * Delete an item
 *
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @return SDB_OK if no errors occurred
 */
int sdb_delete(struct SDB* sdb, const char* domain, const char* item);

/**
 * Delete an attribute
 *
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @param key the attribute name
 * @return SDB_OK if no errors occurred
 */
int sdb_delete_attr(struct SDB* sdb, const char* domain, const char* item, const char* key);

/**
 * Delete multiple attributes
 *
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @param num the number of keys
 * @param keys the array of attribute names
 * @return SDB_OK if no errors occurred
 */
int sdb_delete_attr_many(struct SDB* sdb, const char* domain, const char* item, size_t num, const char** keys);

/**
 * Delete an attribute/value pairs
 *
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @param key the attribute name
 * @param value the attribute value
 * @return SDB_OK if no errors occurred
 */
int sdb_delete_attr_ext(struct SDB* sdb, const char* domain, const char* item, const char* key, const char* value);

/**
 * Delete multiple attribute/value pairs
 *
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @param num the number of keys
 * @param keys the array of attribute names
 * @param values the array of attribute values
 * @return SDB_OK if no errors occurred
 */
int sdb_delete_attr_ext_many(struct SDB* sdb, const char* domain, const char* item, size_t num, const char** keys, const char** values);

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
int sdb_get(struct SDB* sdb, const char* domain, const char* item, const char* key, struct sdb_response** response);

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
int sdb_get_many(struct SDB* sdb, const char* domain, const char* item, size_t num, const char** keys, struct sdb_response** response);

/**
 * Get all attributes
 *
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @param response a pointer to the place to store the response
 * @return SDB_OK if no errors occurred
 */
int sdb_get_all(struct SDB* sdb, const char* domain, const char* item, struct sdb_response** response);

/**
 * Query the database and return the names of the matching records
 *
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param query the query expression
 * @param response a pointer to the place to store the response
 * @return SDB_OK if no errors occurred
 */
int sdb_query(struct SDB* sdb, const char* domain, const char* query, struct sdb_response** response) __attribute__ ((deprecated));

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
int sdb_query_attr(struct SDB* sdb, const char* domain, const char* query, const char* key, struct sdb_response** response) __attribute__ ((deprecated));

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
int sdb_query_attr_many(struct SDB* sdb, const char* domain, const char* query, size_t num, const char** keys, struct sdb_response** response) __attribute__ ((deprecated));

/**
 * Query with attributes: return all attributes
 *
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param query the query expression
 * @param response a pointer to the place to store the response
 * @return SDB_OK if no errors occurred
 */
int sdb_query_attr_all(struct SDB* sdb, const char* domain, const char* query, struct sdb_response** response) __attribute__ ((deprecated));

/**
 * Query using a SELECT expression
 *
 * @param sdb the SimpleDB handle
 * @param expr the select expression
 * @param response a pointer to the place to store the response
 * @return SDB_OK if no errors occurred
 */
int sdb_select(struct SDB* sdb, const char* expr, struct sdb_response** response);



/*****************************************************************************/
/*                                                                           */
/*                        M U L T I - C O M M A N D S                        */
/*                                                                           */
/*****************************************************************************/

/**
 * Perform all pending operations specified using sdb_multi_* functions
 *
 * @param sdb the SimpleDB handle
 * @param response a pointer to the place to store the response
 * @return SDB_OK if no errors occurred
 */
int sdb_multi_run(struct SDB* sdb, struct sdb_multi_response** response);

/**
 * Create a domain
 *
 * @param sdb the SimpleDB handle
 * @param name the domain name
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_create_domain(struct SDB* sdb, const char* name);

/**
 * Delete a domain
 *
 * @param sdb the SimpleDB handle
 * @param name the domain name
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_delete_domain(struct SDB* sdb, const char* name);

/**
 * List domains
 *
 * @param sdb the SimpleDB handle
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_list_domains(struct SDB* sdb);

/**
 * Get domain meta-data
 *
 * @param sdb the SimpleDB handle
 * @param name the domain name
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_domain_metadata(struct SDB* sdb, const char* name);

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
sdb_multi sdb_multi_put(struct SDB* sdb, const char* domain, const char* item, const char* key, const char* value);

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
sdb_multi sdb_multi_replace(struct SDB* sdb, const char* domain, const char* item, const char* key, const char* value);

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
sdb_multi sdb_multi_put_many(struct SDB* sdb, const char* domain, const char* item, size_t num, const char** keys, const char** values);

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
sdb_multi sdb_multi_replace_many(struct SDB* sdb, const char* domain, const char* item, size_t num, const char** keys, const char** values);

/**
 * Put attributes of several items to the database
 *
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param num the number of items
 * @param items the array of items and their attributes
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_put_batch(struct SDB* sdb, const char* domain, size_t num, const struct sdb_item* items);

/**
 * Replace attributes of several items in the database
 *
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param num the number of items
 * @param items the array of items and their attributes
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_replace_batch(struct SDB* sdb, const char* domain, size_t num, const struct sdb_item* items);

/**
 * Delete an item
 *
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_delete(struct SDB* sdb, const char* domain, const char* item);

/**
 * Delete an attribute
 *
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @param key the attribute name
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_delete_attr(struct SDB* sdb, const char* domain, const char* item, const char* key);

/**
 * Delete multiple attributes
 *
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @param num the number of keys
 * @param keys the array of attribute names
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_delete_attr_many(struct SDB* sdb, const char* domain, const char* item, size_t num, const char** keys);

/**
 * Delete an attribute/value pairs
 *
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @param key the attribute name
 * @param value the attribute value
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_delete_attr_ext(struct SDB* sdb, const char* domain, const char* item, const char* key, const char* value);

/**
 * Delete multiple attribute/value pairs
 *
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @param num the number of keys
 * @param keys the array of attribute names
 * @param values the array of attribute values
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_delete_attr_ext_many(struct SDB* sdb, const char* domain, const char* item, size_t num, const char** keys, const char** values);

/**
 * Get an attribute
 *
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @param key the attribute name
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_get(struct SDB* sdb, const char* domain, const char* item, const char* key);

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
sdb_multi sdb_multi_get_many(struct SDB* sdb, const char* domain, const char* item, size_t num, const char** keys);

/**
 * Get all attributes
 *
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param item the item name
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_get_all(struct SDB* sdb, const char* domain, const char* item);

/**
 * Query the database and return the names of the matching records
 *
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param query the query expression
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_query(struct SDB* sdb, const char* domain, const char* query) __attribute__ ((deprecated));

/**
 * Query with attributes: return one attribute
 *
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param query the query expression
 * @param key the attribute name to return
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_query_attr(struct SDB* sdb, const char* domain, const char* query, const char* key) __attribute__ ((deprecated));

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
sdb_multi sdb_multi_query_attr_many(struct SDB* sdb, const char* domain, const char* query, size_t num, const char** keys) __attribute__ ((deprecated));

/**
 * Query with attributes: return all attributes
 *
 * @param sdb the SimpleDB handle
 * @param domain the domain name
 * @param query the query expression
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_query_attr_all(struct SDB* sdb, const char* domain, const char* query) __attribute__ ((deprecated));

/**
 * Query using a SELECT expression
 *
 * @param sdb the SimpleDB handle
 * @param expr the select expression
 * @return the command execution handle, or SDB_MULTI_ERROR on error
 */
sdb_multi sdb_multi_select(struct SDB* sdb, const char* expr);


#ifdef __cplusplus
}
#endif

#endif
