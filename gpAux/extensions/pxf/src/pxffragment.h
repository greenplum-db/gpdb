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
 *
 */

#ifndef GPDB_PXFFRAGMENT_H
#define GPDB_PXFFRAGMENT_H

#include "pxfuriparser.h"
#include "libchurl.h"
#include "pxfheaders.h"
#include "lib/stringinfo.h"

/*
 * Context for the Fragmenter
 */
typedef struct sClientContext
{
	CHURL_HEADERS http_headers;
	CHURL_HANDLE handle;
	/* part of the HTTP response - received	*/
	/* from one call to churl_read 			*/
	/* contains the complete HTTP response 	*/
	StringInfoData the_rest_buf;
} ClientContext;

/*
 * One debug level for all log messages from the data allocation algorithm
 */
#define FRAGDEBUG DEBUG2

#define REST_HEADER_JSON_RESPONSE "Accept: application/json"

extern void set_fragments(GPHDUri* uri, Relation relation);

#endif //GPDB_PXFFRAGMENT_H
