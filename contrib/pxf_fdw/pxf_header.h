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

#ifndef _PXFHEADERS_H_
#define _PXFHEADERS_H_

#define PXF_SERVICE_PREFIX "pxf"
#define PXF_VERSION "v15"

#include "libchurl.h"

#include "pxf_fdw.h"

#include "nodes/execnodes.h"
#include "nodes/nodeFuncs.h"
#include "utils/rel.h"

/*
 * Adds the headers necessary for PXF service call
 */
extern void BuildHttpHeaders(CHURL_HEADERS headers,
							 PxfOptions * options,
							 Relation relation,
							 char *filter_string,
							 ProjectionInfo *proj_info,
							 List *quals);

#endif							/* _PXFHEADERS_H_ */
