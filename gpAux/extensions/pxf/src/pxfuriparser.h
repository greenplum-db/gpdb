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
#define PXF_VERSION "v15" /* PXF version */
#define PROTOCOL_PXF "pxf://"
#define IS_PXF_URI(uri_str) (pg_strncasecmp(uri_str, PROTOCOL_PXF, strlen(PROTOCOL_PXF)) == 0)

/*
 * FragmentData - describes a single Hadoop file split / HBase table region
 * in means of location (ip, port), the source name of the specific file/table that is being accessed,
 * and the index of a of list of fragments (splits/regions) for that source name.
 * The index refers to the list of the fragments of that source name.
 * user_data is optional.
 */
typedef struct FragmentData
{
	char	 *authority;
	char	 *index;
	char	 *source_name;
	char	 *fragment_md;
	char	 *user_data;
	char	 *profile;
} FragmentData;

/*
 * Structure to store options data, such as types of fragmenters, accessors and resolvers
 */
typedef struct OptionData
{
	char	 *key;
	char	 *value;
} OptionData;

/*
 * GPHDUri - Describes the contents of a hadoop uri.
 */
typedef struct GPHDUri
{
    char 			*uri;		/* the unparsed user uri	*/
	char			*protocol;	/* the protocol name		*/
	char			*host;		/* host name str			*/
	char			*port;		/* port number as string	*/
	char			*data;      /* data location (path)     */
	char			*profile;   /* profile option			*/
	List			*fragments; /* list of FragmentData		*/
	List			*options;   /* list of OptionData 		*/
} GPHDUri;

/*
 * Parses a string URI into a data structure
 */
GPHDUri	*parseGPHDUri(const char *uri_str);

/*
 * Frees the elements of the data structure
 */
void 	 freeGPHDUri(GPHDUri *uri);

#endif	// _PXFURIPARSER_H_
