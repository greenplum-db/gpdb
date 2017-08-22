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
#include "pxfuriparser.h"

/* convert input string to upper case and prepend "X-GP-" prefix */
char* normalize_key_name(const char* key);

/* get the name of the type, given the OID */
char* TypeOidGetTypename(Oid typid);

/* Concatenate multiple literal strings using stringinfo */
char* concat(int num_args, ...);

#define PXF_CLUSTER       "default"
#define PXF_PROFILE       "PROFILE"
#define FRAGMENTER        "FRAGMENTER"
#define ACCESSOR          "ACCESSOR"
#define RESOLVER          "RESOLVER"
#define ANALYZER          "ANALYZER"
#define PxfDefaultHost    "localhost"
#define PxfDefaultPort    51200

#endif  // _PXFUTILS_H_
