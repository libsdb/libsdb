/*
 * response.c
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
#include "response.h"

#include "sdb.h"

#include <libxml/parser.h>
#include <libxml/tree.h>


/**
 * Initialize the response structure
 * 
 * @param meta the data structure
 */
void sdb_response_init(struct sdb_response* r)
{
	r->size = 0;
	r->type = SDB_R_NONE;
	r->has_more = FALSE;
	
	r->error = 0;
	r->error_message = NULL;
	r->num_errors = 0;
	
	r->box_usage = 0;
	r->multi_handle = NULL;
	r->return_code = SDB_OK;
	
	r->internal = sdb_response_internal_allocate();
}


/**
 * Allocate an internal response structure
 * 
 * @return the data structure
 */
struct sdb_response_internal* sdb_response_internal_allocate(void)
{
	struct sdb_response_internal* r = (struct sdb_response_internal*) malloc(sizeof(struct sdb_response_internal));
	r->doc = NULL;
	r->next_token = NULL;
	r->errout = NULL;
	r->next = NULL;
	r->params = NULL;
	r->command = NULL;
	r->to_free = NULL;
	r->empty_string[0] = '\0';
}


/**
 * Cleanup the response structure
 * 
 * @param meta the data structure
 */
void sdb_response_cleanup(struct sdb_response* r)
{
	int i;
	struct sdb_response_internal* p;
	struct sdb_response_internal* next = NULL;
	struct sdb_response_to_free* f;
	struct sdb_response_to_free* fnext = NULL;
	
	
	// Free the response
	
	switch (r->type) {
		case SDB_R_NONE: break;
		case SDB_R_DOMAIN_LIST: free(r->domains); break;
		case SDB_R_DOMAIN_METADATA: free(r->domain_metadata); break;
		case SDB_R_ATTRIBUTE_LIST: free(r->attributes); break;
		case SDB_R_ITEM_LIST:
			for (i = 0; i < r->size; i++) if (r->items[i].attributes != NULL) free(r->items[i].attributes);
			free(r->items);
			break; 
		default: assert(0);
	}
	
	
	// Free the internal data structures
	
	for (p = r->internal; p != NULL; p = next) {
		next = p->next;
		
		if (p->doc != NULL) {
			xmlFreeDoc(p->doc);
		}
		
		if (p->params != NULL) {
			sdb_params_free(p->params);
		}
		
		for (f = p->to_free; f != NULL; f = fnext) {
			fnext = f->next;
			free(f->p);
			free(f);
			f = fnext;
		}
		
		if (p->command != NULL) {
			free(p->command);
		}
		
		free(p);
		p = next;
	}
}


/**
 * Allocate the response structure
 * 
 * @return the data structure
 */
struct sdb_response* sdb_response_allocate(void)
{
	struct sdb_response* r = (struct sdb_response*) malloc(sizeof(struct sdb_response));
	sdb_response_init(r);
	return r;
}


/**
 * Prepare an existing response structure for appending
 * 
 * @param r the data structure
 */
void sdb_response_prepare_append(struct sdb_response* r)
{
	struct sdb_response_internal* next = r->internal;
	r->internal = sdb_response_internal_allocate();
	r->internal->next = next;
	
	r->internal->params = next->params;
	r->internal->command = next->command;
	r->internal->next->params = NULL;
	r->internal->next->command = NULL;
	
	r->return_code = 0;
	r->error = 0;
	r->error_message = NULL; 
}


/**
 * Print the response
 * 
 * @param f the output file 
 * @param r the data structure
 */
void sdb_response_print(FILE* f, struct sdb_response* r)
{
	int i, j;
	
	if (r->error != 0) {
		fprintf(f, "Error %s: %s\n", SDB_AWS_ERROR_NAME(r->error),
				r->error_message == NULL ? "(no details are available)" : r->error_message);
		return;
	}
	
	if (r->type == SDB_R_NONE) {
		fprintf(f, "OK\n");
		return;
	}
	
	if (r->type == SDB_R_DOMAIN_LIST) {
		for (i = 0; i < r->size; i++) fprintf(f, "%s\n", r->domains[i]);
		if (r->has_more) fprintf(f, "(incomplete)\n");
		return;
	}
	
	if (r->type == SDB_R_DOMAIN_METADATA) {
		fprintf(f, "Timestamp = %ld\n", r->domain_metadata->timestamp);
		fprintf(f, "ItemCount = %ld\n", r->domain_metadata->item_count);
		fprintf(f, "AttributeValueCount = %ld\n", r->domain_metadata->attr_value_count);
		fprintf(f, "AttributeNameCount = %ld\n", r->domain_metadata->attr_name_count);
		fprintf(f, "ItemNamesSizeBytes = %ld\n", r->domain_metadata->item_names_size);
		fprintf(f, "AttributeValuesSizeBytes = %ld\n", r->domain_metadata->attr_values_size);
		fprintf(f, "AttributeNamesSizeBytes = %ld\n", r->domain_metadata->attr_names_size);
		return;
	}
	
	if (r->type == SDB_R_ATTRIBUTE_LIST) {
		for (i = 0; i < r->size; i++) fprintf(f, "%s = %s\n", r->attributes[i].name, r->attributes[i].value);
		if (r->has_more) fprintf(f, "(incomplete)\n");
		return;
	}
	
	if (r->type == SDB_R_ITEM_LIST) {
		for (i = 0; i < r->size; i++) {
			fprintf(f, "%s\n", r->items[i].name);
			for (j = 0; j < r->items[i].size; j++) fprintf(f, "  %s = %s\n", r->items[i].attributes[j].name, r->items[i].attributes[j].value);
		}
		if (r->has_more) fprintf(f, "(incomplete)\n");
		return;
	}
	
	assert(0);
}


/**
 * Parse the response
 * 
 * @param response the response data structure
 * @param buffer the buffer to parse
 * @param length the length of the data in the buffer
 * @return SDB_OK if no errors occurred
 */
int sdb_response_parse(struct sdb_response* response, const char* buffer, size_t length)
{
	// Parse the XML
	
	response->internal->doc = xmlReadMemory(buffer, length, "response.xml", NULL, 0);
	if (response->internal->doc == NULL) return SDB_E_INVALID_XML_RESPONSE;
	
	
	// Get the root node
	
	xmlNodePtr root = xmlDocGetRootElement(response->internal->doc);
	
	
	// Iterate over the root node
	
	xmlNodePtr cur;
	for (cur = root->children; cur != NULL; cur = cur->next) {
		
		
		// Error node
		
		if (strcmp(cur->name, "Errors") == 0) {
			SDB_SAFE(sdb_response_parse_errors(response, cur));
			continue;
		}
		
		
		// Response metadata
		
		if (strcmp(cur->name, "ResponseMetadata") == 0) {
			SDB_SAFE(sdb_response_parse_metadata(response, cur));
			continue;
		}
		
		
		// List domains result
		
		if (strcmp(cur->name, "ListDomainsResult") == 0) {
			SDB_SAFE(sdb_response_parse_domains(response, cur));
			continue;
		}
		
		
		// Domain metadata result
		
		if (strcmp(cur->name, "DomainMetadataResult") == 0) {
			SDB_SAFE(sdb_response_parse_domain_metadata(response, cur));
			continue;
		}
		
		
		// Get attributes result
		
		if (strcmp(cur->name, "GetAttributesResult") == 0) {
			SDB_SAFE(sdb_response_parse_attributes(response, cur));
			continue;
		}
		
		
		// Get the items with attributes result
		
		if (strcmp(cur->name, "QueryResult") == 0 || strcmp(cur->name, "QueryWithAttributesResult") == 0 || strcmp(cur->name, "SelectResult") == 0) {
			SDB_SAFE(sdb_response_parse_items(response, cur));
			continue;
		}
		
		
		// The list of response nodes to ignore
		
		if (strcmp(cur->name, "RequestID") == 0) continue;
		if (strcmp(cur->name, "RequestId") == 0) continue;
		
		
		// Deal with errors here
		
		if (response->internal->errout != NULL) {
			fprintf(response->internal->errout, "SimpleDB ERROR: Invalid node \"%s\" in the AWS response\n", cur->name);
		}
		return SDB_E_INVALID_ERR_RESPONSE;
	}
	
	
	return SDB_OK;
}


/**
 * Parse the errors section of a response
 * 
 * @param response the response data structure (must be initalized)
 * @param errors the errors node
 * @return SDB_OK if no errors occurred
 */
int sdb_response_parse_errors(struct sdb_response* response, xmlNodePtr errors)
{
	xmlNodePtr topcur, cur, content; int i;
	for (topcur = errors->children; topcur != NULL; topcur = topcur->next) {
		
		
		// Handle an error node
		
		if (strcmp(topcur->name, "Error") == 0) {
			response->num_errors++;
			
			for (cur = topcur->children; cur != NULL; cur = cur->next) {
				
				// Parse the error code
				
				if (strcmp(cur->name, "Code") == 0) {
					assert(cur->children != NULL);
					content = cur->children;
					assert(XML_GET_CONTENT(content) != NULL);
					
					if (response->error == 0) {
						for (i = 0; i < SDB_AWS_NUM_ERRORS; i++) {
							if (strcmp(SDB_AWS_ERRORS[i], XML_GET_CONTENT(content)) == 0) {
								response->error = i;
							}
						}
						
						if (response->error == 0) {
							response->error = SDB_AWS_NUM_ERRORS;
							if (response->internal->errout != NULL) {
								fprintf(response->internal->errout, "SimpleDB ERROR: Unknown error code \"%s\"\n", XML_GET_CONTENT(content));
							}
						}
					}
					
					continue;
				}
				
				
				// Parse the error message
				
				if (strcmp(cur->name, "Message") == 0) {
					assert(cur->children != NULL);
					content = cur->children;
					assert(XML_GET_CONTENT(content) != NULL);
					
					if (response->error_message == NULL) response->error_message = XML_GET_CONTENT(content);
					
					if (response->internal->errout != NULL) {
						fprintf(response->internal->errout, "SimpleDB ERROR: %s\n", XML_GET_CONTENT(content));
					}
					continue;
				}
				
				
				// The list of nodes to ignore
				
				if (strcmp(cur->name, "BoxUsage") == 0) continue;
				
				
				// Invalid Amazon response
				
				if (response->internal->errout != NULL) {
					fprintf(response->internal->errout, "SimpleDB ERROR: Invalid node \"%s\" in the AWS error response\n", cur->name);
				}
				return SDB_E_INVALID_ERR_RESPONSE;
			}
			
			continue;
		}
		
		
		// The list of nodes to ignore
		
		if (strcmp(cur->name, "BoxUsage") == 0) continue;
		
				
		// Handle errors
		
		if (response->internal->errout != NULL) {
			fprintf(response->internal->errout, "SimpleDB ERROR: Invalid node \"%s\" in the AWS error response\n", topcur->name);
		}
		return SDB_E_INVALID_ERR_RESPONSE;
	}
	
	return SDB_OK;
}


/**
 * Parse the metadata section of a response
 * 
 * @param response the response data structure (must be initalized)
 * @param metadata the metadata node
 * @return SDB_OK if no errors occurred
 */
int sdb_response_parse_metadata(struct sdb_response* response, xmlNodePtr metadata)
{
	xmlNodePtr cur, content;
	for (cur = metadata->children; cur != NULL; cur = cur->next) {
		
		
		// The box usage statistic
		
		if (strcmp(cur->name, "BoxUsage") == 0) {
			assert(cur->children != NULL);
			content = cur->children;
			assert(XML_GET_CONTENT(content) != NULL);
			
			char* e = NULL;
			response->box_usage = strtod(XML_GET_CONTENT(content), &e);
			if (response->box_usage < 0 || e == NULL || *e != '\0') {
				response->box_usage = 0;
				if (response->internal->errout != NULL) {
					fprintf(response->internal->errout, "SimpleDB ERROR: Invalid box usage \"%s\" in the AWS meta-data response\n", XML_GET_CONTENT(content));
				}
				return SDB_E_INVALID_META_RESPONSE;
			}
			continue;
		}
		
		
		// The list of nodes to ignore
		
		if (strcmp(cur->name, "RequestID") == 0) continue;
		if (strcmp(cur->name, "RequestId") == 0) continue;
		
		
		// Handle errors
		
		if (response->internal->errout != NULL) {
			fprintf(response->internal->errout, "SimpleDB ERROR: Invalid node \"%s\" in the AWS meta-data response\n", cur->name);
		}
		return SDB_E_INVALID_META_RESPONSE;
	}
	
	return SDB_OK;
}


/**
 * Parse the list of domains
 * 
 * @param response the response data structure (must be initalized)
 * @param domains the list of domains
 * @return SDB_OK if no errors occurred
 */
int sdb_response_parse_domains(struct sdb_response* response, xmlNodePtr domains)
{
	xmlNodePtr cur, content;
	int size = 0;
	
	
	// Count the domains and determine whether we have more incoming data
	 
	response->has_more = FALSE;
	
	for (cur = domains->children; cur != NULL; cur = cur->next) {
		
		
		// The domain name
		
		if (strcmp(cur->name, "DomainName") == 0) {
			size++;
			continue;
		}
		
		
		// The next token
		
		if (strcmp(cur->name, "NextToken") == 0) {
			assert(cur->children != NULL);
			content = cur->children;
			response->internal->next_token = XML_GET_CONTENT(content); 
			assert(response->internal->next_token != NULL);
			response->has_more = TRUE;
			continue;
		}
		
		
		// The list of nodes to ignore
		
		if (strcmp(cur->name, "RequestID") == 0) continue;
		if (strcmp(cur->name, "RequestId") == 0) continue;
		
		
		// Handle errors
		
		if (response->internal->errout != NULL) {
			fprintf(response->internal->errout, "SimpleDB ERROR: Invalid node \"%s\" in the AWS list of domains\n", cur->name);
		}
		return SDB_E_INVALID_META_RESPONSE;
	}
	
	
	// Allocate the memory if this is a new result set, otherwise prepare to append
	
	int start = 0;
	
	if (response->type == SDB_R_NONE) {
		
		response->size = size;
		response->type = SDB_R_DOMAIN_LIST;
		response->domains = (char**) malloc(size * sizeof(char*));  
	}
	else if (response->type == SDB_R_DOMAIN_LIST) {
		
		char** old = response->domains;
		int newsize = response->size + size;
		start = response->size;
		
		response->domains = (char**) malloc(newsize * sizeof(char*));
		memcpy(response->domains, old, response->size * sizeof(char*));
		free(old);
		
		response->size = newsize;
	}
	else assert(0);
	
	
	
	// Pull out the domain names 
	
	int index = start;
	for (cur = domains->children; cur != NULL; cur = cur->next) {
		
		
		// The domain name
		
		if (strcmp(cur->name, "DomainName") == 0) {
			assert(cur->children != NULL);
			content = cur->children;
			assert(XML_GET_CONTENT(content) != NULL);
			response->domains[index++] = XML_GET_CONTENT(content); 
		}
	}
	
	assert(index - start == size);
	
	return SDB_OK;
}


#define SDB_RESPONSE_GET_LONG(variable)																	\
	{																									\
		assert(cur->children != NULL);																	\
		content = cur->children;																		\
		assert(XML_GET_CONTENT(content) != NULL);														\
																										\
		char* e = NULL;																					\
		response->variable = strtol(XML_GET_CONTENT(content), &e, 10);									\
		if (e == NULL || *e != '\0') {																	\
			if (!(e[0] == '.' && e[1] == '0' && e[2] == '\0')) { 										\
				response->variable = 0;																	\
				if (response->internal->errout != NULL) {												\
					fprintf(response->internal->errout, "SimpleDB ERROR: Invalid integer value "		\
							"\"%s\" in the AWS response\n", XML_GET_CONTENT(content));					\
				}																						\
				return SDB_E_INVALID_META_RESPONSE;														\
			}																							\
		}																								\
		continue;																						\
	}

/**
 * Parse the domain metadata
 * 
 * @param response the response data structure (must be initalized)
 * @param domain_metadata the domain metadata
 * @return SDB_OK if no errors occurred
 */
int sdb_response_parse_domain_metadata(struct sdb_response* response, xmlNodePtr domain_metadata)
{
	// Allocate the memory
	
	assert(response->type == SDB_R_NONE);
	response->type = SDB_R_DOMAIN_METADATA;
	response->domain_metadata = (struct sdb_domain_metadata*) malloc(sizeof(struct sdb_domain_metadata));
	
	
	// Parse the response
	
	xmlNodePtr cur, content;
	for (cur = domain_metadata->children; cur != NULL; cur = cur->next) {
		
		
		// The metadata
		
		if (strcmp(cur->name, "Timestamp") == 0) SDB_RESPONSE_GET_LONG(domain_metadata->timestamp);
		if (strcmp(cur->name, "ItemCount") == 0) SDB_RESPONSE_GET_LONG(domain_metadata->item_count);
		if (strcmp(cur->name, "AttributeValueCount") == 0) SDB_RESPONSE_GET_LONG(domain_metadata->attr_value_count);
		if (strcmp(cur->name, "AttributeNameCount") == 0) SDB_RESPONSE_GET_LONG(domain_metadata->attr_name_count);
		if (strcmp(cur->name, "ItemNamesSizeBytes") == 0) SDB_RESPONSE_GET_LONG(domain_metadata->item_names_size);
		if (strcmp(cur->name, "AttributeValuesSizeBytes") == 0) SDB_RESPONSE_GET_LONG(domain_metadata->attr_values_size);
		if (strcmp(cur->name, "AttributeNamesSizeBytes") == 0) SDB_RESPONSE_GET_LONG(domain_metadata->attr_names_size);
		
		
		// The list of nodes to ignore
		
		if (strcmp(cur->name, "RequestID") == 0) continue;
		if (strcmp(cur->name, "RequestId") == 0) continue;
		
		
		// Handle errors
		
		if (response->internal->errout != NULL) {
			fprintf(response->internal->errout, "SimpleDB ERROR: Invalid node \"%s\" in the AWS domain meta-data response\n", cur->name);
		}
		return SDB_E_INVALID_META_RESPONSE;
	}
	
	return SDB_OK;
}


/**
 * Parse the list of attributes
 * 
 * @param response the response data structure (must be initalized)
 * @param attributes the list of attributes
 * @return SDB_OK if no errors occurred
 */
int sdb_response_parse_attributes(struct sdb_response* response, xmlNodePtr attributes)
{
	xmlNodePtr cur, cur2, content;
	int size = 0;
	
	
	// Count the attributes and determine whether we have more incoming data
	 
	response->has_more = FALSE;
	
	for (cur = attributes->children; cur != NULL; cur = cur->next) {
		
		
		// The attribute
		
		if (strcmp(cur->name, "Attribute") == 0) {
			size++;
			continue;
		}
		
		
		// The next token
		
		if (strcmp(cur->name, "NextToken") == 0) {
			assert(cur->children != NULL);
			content = cur->children;
			response->internal->next_token = XML_GET_CONTENT(content); 
			assert(response->internal->next_token != NULL);
			response->has_more = TRUE;
			continue;
		}
		
		
		// The list of nodes to ignore
		
		if (strcmp(cur->name, "RequestID") == 0) continue;
		if (strcmp(cur->name, "RequestId") == 0) continue;
		
		
		// Handle errors
		
		if (response->internal->errout != NULL) {
			fprintf(response->internal->errout, "SimpleDB ERROR: Invalid node \"%s\" in the AWS list of attributes\n", cur->name);
		}
		return SDB_E_INVALID_META_RESPONSE;
	}
	
	
	// Allocate the memory if this is a new result set, otherwise prepare to append
	
	int start = 0;
	
	if (response->type == SDB_R_NONE) {
		
		response->size = size;
		response->type = SDB_R_ATTRIBUTE_LIST;
		response->attributes = (struct sdb_attribute*) malloc(size * sizeof(struct sdb_attribute));  
	}
	else if (response->type == SDB_R_ATTRIBUTE_LIST) {
		
		struct sdb_attribute* old = response->attributes;
		int newsize = response->size + size;
		start = response->size;
		
		response->attributes = (struct sdb_attribute*) malloc(newsize * sizeof(struct sdb_attribute));
		memcpy(response->attributes, old, response->size * sizeof(struct sdb_attribute));
		free(old);
		
		response->size = newsize;
	}
	else assert(0);
	
	
	
	// Pull out the attributes 
	
	int index = start;
	for (cur = attributes->children; cur != NULL; cur = cur->next) {
		
		
		// The attribute name
		
		if (strcmp(cur->name, "Attribute") == 0) {
			struct sdb_attribute* a = &(response->attributes[index++]);
			a->name = a->value = NULL;
			
			for (cur2 = cur->children; cur2 != NULL; cur2 = cur2->next) {
				
				if (strcmp(cur2->name, "Name") == 0) { 
					assert(cur2->children != NULL);
					content = cur2->children;
					assert(XML_GET_CONTENT(content) != NULL);
					a->name = XML_GET_CONTENT(content);
					continue;
				} 
				
				if (strcmp(cur2->name, "Value") == 0) { 
					if (cur2->children == NULL) {
						a->value = response->internal->empty_string;
						continue;
					}
					else {
						content = cur2->children;
						assert(XML_GET_CONTENT(content) != NULL);
						a->value = XML_GET_CONTENT(content);
						continue;
					}
				} 
				
				
				// Handle errors
		
				if (response->internal->errout != NULL) {
					fprintf(response->internal->errout, "SimpleDB ERROR: Invalid node \"%s\" in the AWS attribute\n", cur2->name);
				}
				return SDB_E_INVALID_META_RESPONSE;
			}
			
			
			// Check for incomplete attributes
			
			if (a->name == NULL || a->value == NULL) {
				if (response->internal->errout != NULL) {
					fprintf(response->internal->errout, "SimpleDB ERROR: Incomplete attribute in the AWS response\n");
				}
				return SDB_E_INVALID_META_RESPONSE;
			}
		}
	}
	
	assert(index - start == size);
	
	return SDB_OK;
}


/**
 * Parse a single item
 * 
 * @param response the response data structure
 * @param item the item data structure
 * @param node the item node
 * @return SDB_OK if no errors occurred
 */
int sdb_response_parse_item(struct sdb_response* response, struct sdb_item* item, xmlNodePtr node)
{
	xmlNodePtr cur, cur2, content;
	int size = 0;
	
	
	// Count the attributes and determine whether we have more incoming data
	
	for (cur = node->children; cur != NULL; cur = cur->next) {
		
		
		// The attribute
		
		if (strcmp(cur->name, "Attribute") == 0) {
			size++;
			continue;
		}
		
		
		// The item name
		
		if (strcmp(cur->name, "Name") == 0) continue;
		
		
		// Handle errors
		
		return SDB_E_INVALID_META_RESPONSE;
	}
	
	
	// Allocate the memory if this is a new result set, otherwise prepare to append
	
	item->size = size;
	item->attributes = (struct sdb_attribute*) malloc(size * sizeof(struct sdb_attribute));  
	
	
	// Pull out the attributes and the item name 
	
	int index = 0;
	item->name = NULL;
	
	for (cur = node->children; cur != NULL; cur = cur->next) {
		
		
		// The item name
		
		if (strcmp(cur->name, "Name") == 0) { 
			assert(cur->children != NULL);
			content = cur->children;
			assert(XML_GET_CONTENT(content) != NULL);
			item->name = XML_GET_CONTENT(content);
			continue;
		} 
		
		
		// The attribute
		
		if (strcmp(cur->name, "Attribute") == 0) {
			struct sdb_attribute* a = &(item->attributes[index++]);
			a->name = a->value = NULL;
			
			for (cur2 = cur->children; cur2 != NULL; cur2 = cur2->next) {
				
				if (strcmp(cur2->name, "Name") == 0) { 
					assert(cur2->children != NULL);
					content = cur2->children;
					assert(XML_GET_CONTENT(content) != NULL);
					a->name = XML_GET_CONTENT(content);
					continue;
				} 
				
				if (strcmp(cur2->name, "Value") == 0) { 
					if (cur2->children == NULL) {
						a->value = response->internal->empty_string;
						continue;
					}
					else {
						content = cur2->children;
						assert(XML_GET_CONTENT(content) != NULL);
						a->value = XML_GET_CONTENT(content);
						continue;
					}
				} 
				
				
				// Handle errors
		
				return SDB_E_INVALID_META_RESPONSE;
			}
			
			
			// Check for incomplete attributes
			
			if (a->name == NULL || a->value == NULL) {
				return SDB_E_INVALID_META_RESPONSE;
			}
		}
	}
	
	assert(item->name);
	assert(index == size);
	
	return SDB_OK;
}


/**
 * Parse the list of items
 * 
 * @param response the response data structure (must be initalized)
 * @param items the list of items
 * @return SDB_OK if no errors occurred
 */
int sdb_response_parse_items(struct sdb_response* response, xmlNodePtr items)
{
	xmlNodePtr cur, cur2, content;
	int size = 0;
	
	
	// Count the attributes and determine whether we have more incoming data
	 
	response->has_more = FALSE;
	
	for (cur = items->children; cur != NULL; cur = cur->next) {
		
		
		// The item
		
		if (strcmp(cur->name, "Item") == 0 || strcmp(cur->name, "ItemName") == 0) {
			size++;
			continue;
		}
		
		
		// The next token
		
		if (strcmp(cur->name, "NextToken") == 0) {
			assert(cur->children != NULL);
			content = cur->children;
			response->internal->next_token = XML_GET_CONTENT(content); 
			assert(response->internal->next_token != NULL);
			response->has_more = TRUE;
			continue;
		}
		
		
		// The list of nodes to ignore
		
		if (strcmp(cur->name, "RequestID") == 0) continue;
		if (strcmp(cur->name, "RequestId") == 0) continue;
		
		
		// Handle errors
		
		if (response->internal->errout != NULL) {
			fprintf(response->internal->errout, "SimpleDB ERROR: Invalid node \"%s\" in the AWS list of items\n", cur->name);
		}
		return SDB_E_INVALID_META_RESPONSE;
	}
	
	
	// Allocate the memory if this is a new result set, otherwise prepare to append
	
	int start = 0;
	
	if (response->type == SDB_R_NONE) {
		
		response->size = size;
		response->type = SDB_R_ITEM_LIST;
		response->items = (struct sdb_item*) malloc(size * sizeof(struct sdb_item));  
	}
	else if (response->type == SDB_R_ITEM_LIST) {
		
		struct sdb_item* old = response->items;
		int newsize = response->size + size;
		start = response->size;
		
		response->items = (struct sdb_item*) malloc(newsize * sizeof(struct sdb_item));
		memcpy(response->items, old, response->size * sizeof(struct sdb_item));
		free(old);
		
		response->size = newsize;
	}
	else assert(0);
	
	
	
	// Pull out the items 
	
	int index = start;
	for (cur = items->children; cur != NULL; cur = cur->next) {
		
		
		// Item name
		
		if (strcmp(cur->name, "ItemName") == 0) {
			assert(cur->children != NULL);
			content = cur->children;
			response->items[index].name = XML_GET_CONTENT(content);
			response->items[index].size = 0; 
			response->items[index].attributes = NULL; 
			assert(response->items[index].name != NULL);
			index++;
			continue;
		}
		
		
		// Item with attributes
		
		if (strcmp(cur->name, "Item") == 0) {
			SDB_SAFE(sdb_response_parse_item(response, &response->items[index++], cur));
		}
	}
	
	assert(index - start == size);
	
	return SDB_OK;
}
