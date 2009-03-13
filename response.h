/*
 * response.h
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

#include <stdio.h>
#include <stdlib.h>

#include "sdb.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <libxml/parser.h>
#include <libxml/tree.h>


/**
 * An internal response structure
 */
struct sdb_response_internal
{
	// The parse tree
	
	xmlDocPtr doc;
	
	
	// Errors
	
	FILE* errout;
	
	
	// Metadata
	
	xmlChar* next_token;
	
	
	// Special stuff
	
	char empty_string[2];
	
	
	// Internal structure
	
	struct sdb_response_internal* next;
};


/**
 * Initialize the response structure
 * 
 * @param r the data structure
 */
void sdb_response_init(struct sdb_response* r);

/**
 * Cleanup the response structure
 * 
 * @param r the data structure
 */
void sdb_response_cleanup(struct sdb_response* r);

/**
 * Allocate the response structure
 * 
 * @return the data structure
 */
struct sdb_response* sdb_response_allocate(void);

/**
 * Prepare an existing response structure for appending
 * 
 * @param r the data structure
 */
void sdb_response_prepare_append(struct sdb_response* r);

/**
 * Allocate an internal response structure
 * 
 * @return the data structure
 */
struct sdb_response_internal* sdb_response_internal_allocate(void);

/**
 * Print the response
 * 
 * @param f the output file 
 * @param r the data structure
 */
void sdb_response_print(FILE* f, struct sdb_response* r);

/**
 * Parse the response
 * 
 * @param response the response data structure
 * @param buffer the buffer to parse
 * @param length the length of the data in the buffer
 * @return SDB_OK if no errors occurred
 */
int sdb_response_parse(struct sdb_response* response, const char* buffer, size_t length);

/**
 * Parse the errors section of a response
 * 
 * @param response the response data structure (must be initalized)
 * @param errors the errors node
 * @return SDB_OK if no errors occurred
 */
int sdb_response_parse_errors(struct sdb_response* response, xmlNodePtr errors);

/**
 * Parse the metadata section of a response
 * 
 * @param response the response data structure (must be initalized)
 * @param metadata the metadata node
 * @return SDB_OK if no errors occurred
 */
int sdb_response_parse_metadata(struct sdb_response* response, xmlNodePtr metadata);

/**
 * Parse the list of domains
 * 
 * @param response the response data structure (must be initalized)
 * @param domains the list of domains
 * @return SDB_OK if no errors occurred
 */
int sdb_response_parse_domains(struct sdb_response* response, xmlNodePtr domains);

/**
 * Parse the domain metadata
 * 
 * @param response the response data structure (must be initalized)
 * @param domain_metadata the domain metadata
 * @return SDB_OK if no errors occurred
 */
int sdb_response_parse_domain_metadata(struct sdb_response* response, xmlNodePtr domain_metadata);

/**
 * Parse the list of attributes
 * 
 * @param response the response data structure (must be initalized)
 * @param attributes the list of attributes
 * @return SDB_OK if no errors occurred
 */
int sdb_response_parse_attributes(struct sdb_response* response, xmlNodePtr attributes);

/**
 * Parse a single item
 * 
 * @param response the response data structure
 * @param item the item data structure
 * @param node the item node
 * @return SDB_OK if no errors occurred
 */
int sdb_response_parse_item(struct sdb_response* response, struct sdb_item* item, xmlNodePtr node);

/**
 * Parse the list of items
 * 
 * @param response the response data structure (must be initalized)
 * @param items the list of items
 * @return SDB_OK if no errors occurred
 */
int sdb_response_parse_items(struct sdb_response* response, xmlNodePtr items);

#ifdef __cplusplus
}
#endif
