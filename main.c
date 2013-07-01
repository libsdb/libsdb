/*
 * main.c
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
#include "include/sdb.h"
#include <ctype.h>


/**
 * Read a line
 * 
 * @param buf the buffer
 * @param size the size of the buffer
 * @return SDB_OK if no errors occured 
 */
int readln(char* buf, size_t size)
{
	char* str = alloca(size + 4);
	if (!fgets(str, size - 1, stdin)) return -1;
	
	char* s = str;
	char* b = buf;
	size_t i;
	
	for (i = 0; *s != '\0' && *s != '\n' && *s != '\r' && i < (size - 1); s++) {
		char c = *s;
		
		if (c == '\b') {
			if (i > 0) {
				i--;
				b--;
			}
		}
		else {
			*(b++) = c;
			i++;
		}
	}
	
	*b = '\0';
	return 0;
}


#define BUF_SIZE				256
#define READ(prompt, var)		{ printf("%s: ", prompt); if (readln(var, BUF_SIZE)) { printf("\n"); break; } }
#define READ2(prompt, var)		{ printf("%s: ", prompt); if (readln(var, BUF_SIZE)) { printf("\n"); return 1; } }
#define COMMAND(statement)		{ res = NULL; int __r = statement; if (SDB_SUCCESS(__r)) sdb_print(res); sdb_free(&res);		\
								  if (SDB_FAILED(__r)) { printf("Error %d: %s\n", __r, SDB_AWS_ERROR_NAME(__r)); } }


/**
 * The entry point to the application
 * 
 * @param argc the number of command-line arguments
 * @param argv the command-line arguments
 * @return the exit code
 */
int main(int argc, char** argv)
{
	struct SDB* sdb;
	struct sdb_response* res;
	FILE *f;
	
	char aws_id[BUF_SIZE], aws_secret[BUF_SIZE], ua[BUF_SIZE]; 
	char cmd[BUF_SIZE], arg1[BUF_SIZE], arg2[BUF_SIZE], arg3[BUF_SIZE], arg4[BUF_SIZE];
	
	
	// Ask for the AWS access information or get it from /etc/passwd-s3fs, if available
	
	aws_id[0] = aws_secret[0] = '\0';

	if ((f = fopen("/etc/passwd-s3fs", "r")) != NULL) {
		if (fgets(aws_id, BUF_SIZE, f) != NULL) {
			
			char* p = strchr(aws_id, ':');
			if (p != NULL) {
				
				*p = '\0';
				strncpy(aws_secret, p + 1, BUF_SIZE);
				aws_secret[BUF_SIZE-1] = '\0';
				
				for (p = aws_secret; *p != '\0'; p++) {
					if (isspace(*p)) {
						*p = '\0';
						break;
					}
				}
			}
		}
		
		fclose(f);
	}
	
	if (aws_id[0] == '\0' || aws_secret[0] == '\0') {
		printf("\nlibsdb %s Sample Application\n\n", SDB_VERSION);
		READ2("AWS ID", aws_id);
		READ2("AWS Secret", aws_secret);
	}
	
	
	// Initialize
	
	SDB_ASSERT(sdb_global_init());
	SDB_ASSERT(sdb_init(&sdb, aws_id, aws_secret, AWS_DEFAULT_REGION));
	
	sdb_set_error_file(sdb, stderr);
	
	sdb_set_compression(sdb, 1);
	strcpy(ua, "libsdb-test/");
	strcat(ua, SDB_VERSION);
	sdb_set_useragent(sdb, ua);
	
	// Main loop
	
	while (TRUE) {
	
		printf("\nlibsdb %s Sample Application\n\n", SDB_VERSION);
		printf("  0) Exit                          6) Add an attribute\n");
		printf("  1) Create a domain               7) Replace an attribute\n");
		printf("  2) Delete a domain               8) Delete an attribute\n");
		printf("  3) List domains                  9) Get all attributes\n");
		printf("  4) Output domain meta-data      10) Query with attributes\n");
		printf("  5) Delete an item               11) Query using SELECT\n");
		printf(" 12) Change the region\n");
		printf("\n");
		
		READ("Command", cmd);
		printf("\n");
		
		if (strcmp(cmd, "0") == 0) {
			break;
		}
		
		if (strcmp(cmd, "1") == 0) {
			printf("Create a domain\n");
			READ("Domain", arg1);
			COMMAND(sdb_create_domain(sdb, arg1)); 
			continue;
		}
		
		if (strcmp(cmd, "2") == 0) {
			printf("Delete a domain\n");
			READ("Domain", arg1);
			COMMAND(sdb_delete_domain(sdb, arg1)); 
			continue;
		}
		
		if (strcmp(cmd, "3") == 0) {
			printf("List domains\n");
			COMMAND(sdb_list_domains(sdb, &res));
			continue;
		}
		
		if (strcmp(cmd, "4") == 0) {
			printf("Output domain meta-data\n");
			READ("Domain", arg1);
			COMMAND(sdb_domain_metadata(sdb, arg1, &res)); 
			continue;
		}

		if (strcmp(cmd, "5") == 0) {
			printf("Delete an item\n");
			READ("Domain", arg1);
			READ("Item", arg2);
			COMMAND(sdb_delete(sdb, arg1, arg2)); 
			continue;
		}
		
		if (strcmp(cmd, "6") == 0) {
			printf("Add an attribute\n");
			READ("Domain", arg1);
			READ("Item", arg2);
			READ("Key", arg3);
			READ("Value", arg4);
			COMMAND(sdb_put(sdb, arg1, arg2, arg3, arg4)); 
			continue;
		}
		
		if (strcmp(cmd, "7") == 0) {
			printf("Replace an attribute\n");
			READ("Domain", arg1);
			READ("Item", arg2);
			READ("Key", arg3);
			READ("Value", arg4);
			COMMAND(sdb_replace(sdb, arg1, arg2, arg3, arg4)); 
			continue;
		}

		if (strcmp(cmd, "8") == 0) {
			printf("Deleting an attribute\n");
			READ("Domain", arg1);
			READ("Item", arg2);
			READ("Key", arg3);
			COMMAND(sdb_delete_attr(sdb, arg1, arg2, arg3)); 
			continue;
		}
		
		if (strcmp(cmd, "9") == 0) {
			printf("Get all attributes\n");
			READ("Domain", arg1);
			READ("Item", arg2);
			COMMAND(sdb_get_all(sdb, arg1, arg2, &res)); 
			continue;
		}
		
		if (strcmp(cmd, "10") == 0) {
			printf("Query with attributes\n");
			READ("Domain", arg1);
			READ("Query", arg2);
			COMMAND(sdb_query_attr_all(sdb, arg1, arg2, &res)); 
			continue;
		}
		
		if (strcmp(cmd, "11") == 0) {
			printf("Query using SELECT\n");
			READ("Query", arg1);
			COMMAND(sdb_select(sdb, arg1, &res)); 
			continue;
		}
		if (strcmp(cmd, "12") == 0) {

			printf("Please select the region:\n\n");
			printf("  1) US East Northern Virginia Region\n");
			printf("  2) US West Oregon Region\n");
			printf("  3) US West Northern California Region\n");
			printf("  4) EU Ireland Region\n");
			printf("  5) Asia Pacific Singapore Region\n");
			printf("  6) Asia Pacific Sydney Region\n" );
			printf("  7) Asia Pacific Tokyo Region\n");
			printf("  8) South America Sao Paulo Region\n");

			// Either free form text, or the region itself
			READ("\nRegion:", arg1);

			if (strcmp(arg1, "1") == 0) {
				COMMAND(sdb_set_region(&sdb, AWS_US_EAST_NORTHERN_VIRGINIA_REGION));
			}
			else if (strcmp(arg1, "2") == 0) {
				COMMAND(sdb_set_region(&sdb, AWS_US_WEST_OREGON_REGION));
			}
			else if (strcmp(arg1, "3") == 0) {
				COMMAND(sdb_set_region(&sdb, AWS_US_WEST_NORTHERN_CALIFORNIA_REGION));
			}
			else if (strcmp(arg1, "4") == 0) {
				COMMAND(sdb_set_region(&sdb, AWS_EU_IRELAND_REGION));
			}
			else if (strcmp(arg1, "5") == 0) {
				COMMAND(sdb_set_region(&sdb, AWS_ASIA_PACIFIC_SINGAPORE_REGION));
			}
			else if (strcmp(arg1, "6") == 0) {
				COMMAND(sdb_set_region(&sdb, AWS_ASIA_PACIFIC_SYDNEY_REGION));
			}
			else if (strcmp(arg1, "7") == 0) {
				COMMAND(sdb_set_region(&sdb, AWS_ASIA_PACIFIC_TOKYO_REGION));
			}
			else if (strcmp(arg1, "8") == 0) {
				COMMAND(sdb_set_region(&sdb, AWS_SOUTH_AMERICA_SAO_PAULO_REGION));
			} else {
				printf("Unknown region selected, expected 1 to 8 but got [");
				printf(arg1);
				printf("]\n");
			}

			continue;
		}
		
		if (strcmp(cmd, "v") == 0) {
			printf("Batch Put Attributes (large)\n");
			
			struct sdb_attribute a[256];
			char* strs = (char*) malloc(256 * 64 * 2);
			char* names = (char*) malloc(25 * 64);
			a[0].name = "name";
			a[0].value = "val:name";
			a[1].name = "key";
			a[1].value = "val:key";
			a[2].name = "ver";
			a[2].value = "val:ver";
			int i, j, l;
			for (i = 3; i < 256; i++) {
				sprintf(&strs[i * 64 * 2], "key%03d", i);
				l = 1 + rand() % 62;
				for (j = 0; j < l; j++) {
					strs[i * 64 * 2 + 64 + j] = ' ' + rand() % ('z' - ' ' + 1);
				}
				strs[i * 64 * 2 + 64 + l] = 0;
				a[i].name = &strs[i * 64 * 2];
				a[i].value = &strs[i * 64 * 2 + 64];
			}
			
			struct sdb_item x[25];
			for (i = 0; i < 25; i++) {
				sprintf(&names[i * 64], "item%03d", i);
				x[i].name = &names[i * 64];
				j = rand() % 128;
				x[i].size = 1 + rand() % (254 - j);
				x[i].attributes = &a[j];
			}
			COMMAND(sdb_put_batch(sdb, "test1", 25, x)); 
			free(strs);
			free(names);
			continue;
		}
		
		if (strcmp(cmd, "w") == 0) {
			printf("Batch Put Attributes\n");
			struct sdb_attribute a[3];
			a[0].name = "name";
			a[0].value = "val:name";
			a[1].name = "key";
			a[1].value = "val:key";
			a[2].name = "ver";
			a[2].value = "val:ver";
			struct sdb_item x[2];
			x[0].name = "i1";
			x[0].size = 2;
			x[0].attributes = a;
			x[1].name = "i2";
			x[1].size = 3;
			x[1].attributes = a;
			COMMAND(sdb_put_batch(sdb, "test1", 2, x)); 
			continue;
		}
		
		if (strcmp(cmd, "x") == 0) {
			struct sdb_multi_response* res;
			int i, r;
			printf("DEBUG MULTI\n");
			printf("  %p\n", sdb_multi_replace(sdb, "pass_test2", "item1", "name", "Item 1"));
			printf("  %p\n", sdb_multi_replace(sdb, "pass_test2", "item2", "name", "Item 2"));
			printf("  %p\n", sdb_multi_replace(sdb, "pass_test2", "item3", "name", "Item 3"));
			r = sdb_multi_run(sdb, &res);
			printf("  Executor returned %d\n", r);
			if (res != NULL) {
				for (i = 0; i < res->size; i++) {
					printf("  %p --> %d\n", res->responses[i]->multi_handle, res->responses[i]->return_code);
				} 
			}
			sdb_multi_free(&res);
			continue;
		}
		
		if (strcmp(cmd, "y") == 0) {
			struct sdb_multi_response* res;
			int i, r;
			printf("DEBUG MULTI\n");
			printf("  %p\n", sdb_multi_list_domains(sdb));
			printf("  %p\n", sdb_multi_list_domains(sdb));
			r = sdb_multi_run(sdb, &res);
			printf("  Executor returned %d\n", r);
			if (res != NULL) {
				for (i = 0; i < res->size; i++) {
					printf("  %p --> %d\n", res->responses[i]->multi_handle, res->responses[i]->return_code);
					sdb_print(res->responses[i]);
				} 
			}
			sdb_multi_free(&res);
			continue;
		}
		
		if (strcmp(cmd, "z") == 0) {
			struct sdb_response* res;
			int num = 10;
			sdb_set_auto_next(sdb, FALSE);
			sdb_list_domains(sdb, &res);
			sdb_print(res);
			while (res != NULL && res->has_more) {
				sdb_next(sdb, &res, TRUE);
				sdb_print(res);
				if (!(num --> 0)) break;
			}
			sdb_free(&res);
			printf("---\n"); num = 10;
			sdb_list_domains(sdb, &res);
			sdb_print(res);
			while (res != NULL && res->has_more) {
				sdb_next(sdb, &res, FALSE);
				sdb_print(res);
				if (!(num --> 0)) break;
			}
			sdb_free(&res);
			sdb_set_auto_next(sdb, TRUE);
			continue;
		}
		
		printf("Invalid option.\n");
	}
	
	
	// Clean up
	
	SDB_ASSERT(sdb_destroy(&sdb));
	SDB_ASSERT(sdb_global_cleanup());
	
	return 0;
}
