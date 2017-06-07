/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef _PXFUTILS_H_
#define _PXFUTILS_H_

#include "postgres.h"
#include "libchurl.h"
#include "pxfuriparser.h"

typedef struct sClientContext
{
	CHURL_HEADERS http_headers;
	CHURL_HANDLE handle;
									/* part of the HTTP response - received	*/
									/* from one call to churl_read 			*/
	StringInfoData the_rest_buf; 	/* contains the complete HTTP response 	*/
} ClientContext;

/* checks if two ip strings are equal */
bool are_ips_equal(char *ip1, char *ip2);

/* override port str with given new port int */
void port_to_str(char** port, int new_port);

/* parse the REST message and issue the libchurl call */
void call_rest(GPHDUri *hadoop_uri, ClientContext *client_context, char* rest_msg);

/* convert input string to upper case and prepend "X-GP-" prefix */
char* normalize_key_name(const char* key);

/* get the name of the type, given the OID */
char* TypeOidGetTypename(Oid typid);

#endif	// _PXFUTILS_H_
