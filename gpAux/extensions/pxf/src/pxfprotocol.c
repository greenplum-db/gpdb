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

#include "pxfbridge.h"
#include "pxffragment.h"
#include "pxfutils.h"

#include "access/extprotocol.h"
#include "nodes/pg_list.h"

/* define magic module unless run as a part of test cases */
#ifndef UNIT_TESTING
PG_MODULE_MAGIC;
#endif

PG_FUNCTION_INFO_V1(pxfprotocol_export);
PG_FUNCTION_INFO_V1(pxfprotocol_import);
PG_FUNCTION_INFO_V1(pxfprotocol_validate_urls);

/* public function declarations */
Datum		pxfprotocol_export(PG_FUNCTION_ARGS);
Datum		pxfprotocol_import(PG_FUNCTION_ARGS);
Datum		pxfprotocol_validate_urls(PG_FUNCTION_ARGS);

/* helper function declarations */
static gphadoop_context *create_context(PG_FUNCTION_ARGS, bool is_import);
static void cleanup_context(gphadoop_context *context);
static void check_caller(PG_FUNCTION_ARGS, const char *func_name);

/*
 * Validates external table URL
 */
Datum
pxfprotocol_validate_urls(PG_FUNCTION_ARGS)
{
	/* Must be called via the external table format manager */
	if (!CALLED_AS_EXTPROTOCOL_VALIDATOR(fcinfo))
		elog(ERROR, "cannot execute pxfprotocol_validate_urls outside protocol manager");

	/* There must be only ONE url. */
	if (EXTPROTOCOL_VALIDATOR_GET_NUM_URLS(fcinfo) != 1)
		ereport(ERROR,
				(errcode(ERRCODE_PROTOCOL_VIOLATION),
				 errmsg("number of URLs must be one")));

	char	   *uri_string = EXTPROTOCOL_VALIDATOR_GET_NTH_URL(fcinfo, 1);

	elog(DEBUG2, "pxfprotocol_validate_urls: uri %s", uri_string);
	GPHDUri    *uri = parseGPHDUri(uri_string);

	/* Test that Fragmenter or Profile was specified in the URI */
	if (!GPHDUri_opt_exists(uri, FRAGMENTER) && !GPHDUri_opt_exists(uri, PXF_PROFILE))
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("FRAGMENTER or PROFILE option must exist in %s", uri->uri)));

	/* No duplicate options. */
	GPHDUri_verify_no_duplicate_options(uri);

	/* Check for existence of core options if profile wasn't supplied */
	if (!GPHDUri_opt_exists(uri, PXF_PROFILE))
	{
		List	   *coreOptions = list_make2(ACCESSOR, RESOLVER);

		if (EXTPROTOCOL_VALIDATOR_GET_DIRECTION(fcinfo) != EXT_VALIDATE_WRITE)
			coreOptions = lcons(FRAGMENTER, coreOptions);
		GPHDUri_verify_core_options_exist(uri, coreOptions);
		list_free(coreOptions);
	}

	freeGPHDUri(uri);
	PG_RETURN_VOID();
}

/*
 * Writes to an external table
 */
Datum
pxfprotocol_export(PG_FUNCTION_ARGS)
{
	/* Must be called via the external table format manager */
	check_caller(fcinfo, "pxfprotocol_export");
	/* retrieve user context required for data write */
	gphadoop_context *context = (gphadoop_context *) EXTPROTOCOL_GET_USER_CTX(fcinfo);

	/* last call -- cleanup */
	if (EXTPROTOCOL_IS_LAST_CALL(fcinfo))
	{
		cleanup_context(context);
		EXTPROTOCOL_SET_USER_CTX(fcinfo, NULL);
		PG_RETURN_INT32(0);
	}

	/* first call -- do any desired init */
	if (context == NULL)
	{
		context = create_context(fcinfo, true);
		EXTPROTOCOL_SET_USER_CTX(fcinfo, context);
		gpbridge_export_start(context);
	}
	/* Read data */
	int			bytes_written = gpbridge_write(context, EXTPROTOCOL_GET_DATABUF(fcinfo), EXTPROTOCOL_GET_DATALEN(fcinfo));

	PG_RETURN_INT32(bytes_written);
}

/*
 * Reads tuples from an external table
 */
Datum
pxfprotocol_import(PG_FUNCTION_ARGS)
{
	/* Must be called via the external table format manager */
	check_caller(fcinfo, "pxfprotocol_import");
	/* retrieve user context required for data read */
	gphadoop_context *context = (gphadoop_context *) EXTPROTOCOL_GET_USER_CTX(fcinfo);

	/* last call -- cleanup */
	if (EXTPROTOCOL_IS_LAST_CALL(fcinfo))
	{
		cleanup_context(context);
		EXTPROTOCOL_SET_USER_CTX(fcinfo, NULL);
		PG_RETURN_INT32(0);
	}
	/* first call -- do any desired init */
	if (context == NULL)
	{
		context = create_context(fcinfo, true);
		EXTPROTOCOL_SET_USER_CTX(fcinfo, context);
		gpbridge_import_start(context);
	}
	/* Read data */
	int			bytes_read = gpbridge_read(context, EXTPROTOCOL_GET_DATABUF(fcinfo), EXTPROTOCOL_GET_DATALEN(fcinfo));

	PG_RETURN_INT32(bytes_read);
}

/*
 * Allocates context and sets values for the segment
 */
static gphadoop_context *
create_context(PG_FUNCTION_ARGS, bool is_import)
{
	/* parse and set uri */
	GPHDUri    *uri = parseGPHDUri(EXTPROTOCOL_GET_URL(fcinfo));
	Relation	relation = EXTPROTOCOL_GET_RELATION(fcinfo);

	if (is_import) {
		/* fetch data fragments */
		get_fragments(uri, relation);
	}

	/* set context */
	gphadoop_context *context = palloc0(sizeof(gphadoop_context));

	context->gphd_uri = uri;
	initStringInfo(&context->uri);
	initStringInfo(&context->write_file_name);
	context->relation = relation;

	return context;
}

/*
 * De-allocates context and dependent structures.
 */
static void
cleanup_context(gphadoop_context *context)
{
	if (context != NULL)
	{
		gpbridge_cleanup(context);
		pfree(context->uri.data);
		pfree(context->write_file_name.data);
		pfree(context);
	}
}

/*
 * Checks that the caller is External Protocol Manager
 */
static void
check_caller(PG_FUNCTION_ARGS, const char *func_name)
{
	if (!CALLED_AS_EXTPROTOCOL(fcinfo))
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("%s not called by external protocol manager", func_name)));
}
