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

#ifndef _PXFBRIDGE_H
#define _PXFBRIDGE_H

#include "libchurl.h"
#include "pxf_fragment.h"

#include "cdb/cdbvars.h"
#include "nodes/pg_list.h"

/* Context for single query execution by PXF bridge */
typedef struct
{
	CHURL_HEADERS churl_headers;
	CHURL_HANDLE churl_handle;
	StringInfoData uri;
	ListCell   *current_fragment;
	Relation	relation;
	char	   *filterstr;
	ProjectionInfo *proj_info;
	List	   *quals;
	List	   *fragments;
	PxfOptions *options;
}			PxfContext;

/* Clean up churl related data structures from the context */
void		PxfBridgeCleanup(PxfContext * context);

/* Sets up data before starting import */
void		PxfBridgeImportStart(PxfContext * context);

/* Sets up data before starting export */
void		pxfBridgeExportStart(PxfContext * context);

/* Reads data from the PXF server into the given buffer of a given size */
int			PxfBridgeRead(PxfContext * context, char *databuf, int datalen);

/* Writes data from the given buffer of a given size to the PXF server */
int			PxfBridgeWrite(PxfContext * context, char *databuf, int datalen);

#endif							/* _PXFBRIDGE_H */
