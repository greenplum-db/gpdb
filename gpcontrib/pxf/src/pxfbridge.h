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
#include "pxffragment.h"
#include "pxfuriparser.h"

#include "cdb/cdbvars.h"
#include "nodes/pg_list.h"

/*
 * Context for single query execution by PXF bridge
 */
typedef struct
{
	CHURL_HEADERS churl_headers;
	CHURL_HANDLE churl_handle;
	GPHDUri    *gphd_uri;
	StringInfoData uri;
	ListCell   *current_fragment;
	StringInfoData write_file_name;
	Relation	relation;
	char *filterstr;
} gphadoop_context;

/*
 * Clean up churl related data structures from the context
 */
void		gpbridge_cleanup(gphadoop_context *context);

/*
 * Sets up data before starting import
 */
void		gpbridge_import_start(gphadoop_context *context);

/*
 * Sets up data before starting export
 */
void		gpbridge_export_start(gphadoop_context *context);

/*
 * Reads data from the PXF server into the given buffer of a given size
 */
int			gpbridge_read(gphadoop_context *context, char *databuf, int datalen);

/*
 * Writes data from the given buffer of a given size to the PXF server
 */
int			gpbridge_write(gphadoop_context *context, char *databuf, int datalen);

#endif							/* _PXFBRIDGE_H */
