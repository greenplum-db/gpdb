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
#include "postgres.h"
#include "fmgr.h"
#include "access/extprotocol.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(pxfprotocol_export);
PG_FUNCTION_INFO_V1(pxfprotocol_import);
PG_FUNCTION_INFO_V1(pxfprotocol_validate_urls);

Datum pxfprotocol_export(PG_FUNCTION_ARGS);
Datum pxfprotocol_import(PG_FUNCTION_ARGS);
Datum pxfprotocol_validate_urls(PG_FUNCTION_ARGS);

typedef struct GpId {
    int4 numsegments; /* count of distinct segindexes */
    int4 dbid;        /* the dbid of this database */
    int4 segindex;    /* content indicator: -1 for entry database,
                       * 0, ..., n-1 for segment database *
                       * a primary and its mirror have the same segIndex */
} GpId;
extern GpId GpIdentity;

typedef struct {
    int32 row_count;
} context_t;


Datum
pxfprotocol_validate_urls(PG_FUNCTION_ARGS)
{
	elog(INFO, "Dummy PXF protocol validate");
	PG_RETURN_VOID();
}

Datum
pxfprotocol_export(PG_FUNCTION_ARGS)
{
	elog(INFO, "Dummy PXF protocol write");
    PG_RETURN_INT32(0);
}

Datum
pxfprotocol_import(PG_FUNCTION_ARGS)
{
    /* Must be called via the external table format manager */
    if (!CALLED_AS_EXTPROTOCOL(fcinfo))
        elog(ERROR, "extprotocol_import: not called by external protocol manager");

    /* retrieve user context */
    context_t *context = (context_t *) EXTPROTOCOL_GET_USER_CTX(fcinfo);

    /* last call -- cleanup */
    if (EXTPROTOCOL_IS_LAST_CALL(fcinfo)) {
        pfree(context);

        EXTPROTOCOL_SET_USER_CTX(fcinfo, NULL);
        PG_RETURN_INT32(0);
    }

    /* first call -- do any desired init */
    if (context == NULL) {
        context = palloc(sizeof(context_t));
        context->row_count=0;

        EXTPROTOCOL_SET_USER_CTX(fcinfo, context);
    }

    if (context->row_count > 0)
    {
        /* no more data to read */
        PG_RETURN_INT32(0);
    }

    char *data_buf = EXTPROTOCOL_GET_DATABUF(fcinfo);
    int32 data_len = EXTPROTOCOL_GET_DATALEN(fcinfo);

    /* produce a tuple */
    sprintf(data_buf, "%d,hello world %d", GpIdentity.segindex, GpIdentity.segindex);

    /* save state */
    context->row_count++;

    PG_RETURN_INT32(strlen(data_buf));
}
