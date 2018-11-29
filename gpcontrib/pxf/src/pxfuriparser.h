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

#ifndef _PXFURIPARSER_H_
#define _PXFURIPARSER_H_

#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"

/*
 * Path constants for accessing PXF.
 * All PXF's resources are under /PXF_SERVICE_PREFIX/PXF_VERSION/...
 */
#define PXF_SERVICE_PREFIX "pxf"
#define PXF_VERSION "v15"		/* PXF version */
#define PROTOCOL_PXF "pxf://"
#define IS_PXF_URI(uri_str) (pg_strncasecmp(uri_str, PROTOCOL_PXF, strlen(PROTOCOL_PXF)) == 0)

/*
 * Structure to store options data, such as types of fragmenters, accessors and resolvers
 */
typedef struct OptionData
{
	char	   *key;
	char	   *value;
} OptionData;

/*
 * GPHDUri - Describes the contents of a hadoop uri.
 */
typedef struct GPHDUri
{
	char	   *uri;			/* the unparsed user uri    */
	char	   *protocol;		/* the protocol name        */
	char	   *host;			/* host name str            */
	char	   *port;			/* port number as string    */
	char	   *data;			/* data location (path)     */
	char	   *profile;		/* profile option           */
	List	   *fragments;		/* list of fragments        */
	List	   *options;		/* list of OptionData       */
} GPHDUri;

/*
 * Parses a string URI into a data structure
 */
GPHDUri    *parseGPHDUri(const char *uri_str);
GPHDUri    *parseGPHDUriHostPort(const char *uri_str, const char *host, const int port);

/*
 * Validation functions
 */
bool		GPHDUri_opt_exists(GPHDUri *uri, char *key);
void		GPHDUri_verify_no_duplicate_options(GPHDUri *uri);
void		GPHDUri_verify_core_options_exist(GPHDUri *uri, List *coreOptions);

/*
 * Frees the elements of the data structure
 */
void		freeGPHDUri(GPHDUri *uri);

#endif							/* _PXFURIPARSER_H_ */
