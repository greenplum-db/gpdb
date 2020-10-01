/*
 * Copyright (c) 2012 EMC Corporation All Rights Reserved
 *
 * This software is protected, without limitation, by copyright law
 * and international treaties. Use of this software and the intellectual
 * property contained therein is expressly limited to the terms and
 * conditions of the License Agreement under which it is provided by
 * or on behalf of EMC.
 *
 * ---------------------------------------------------------------------
 *
 * The dynamically linked library created from this source can be reference by
 * creating a function in psql that references it. For example,
 *
 * CREATE OR REPLACE FUNCTION gp_shared_cache_stats()
 *   RETURNS RECORD
 *   AS '$libdir/gp_shared_cache.so', 'gp_shared_cache_stats'
 *   LANGUAGE C;
 *
 */

#include "postgres.h"

#include "funcapi.h"
#include "cdb/cdbvars.h"
#include "utils/builtins.h"
#include "utils/workfile_mgr.h"
#include "miscadmin.h"

PG_MODULE_MAGIC;

Datum gp_workfile_mgr_cache_entries(PG_FUNCTION_ARGS);
Datum gp_workfile_mgr_used_diskspace(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(gp_workfile_mgr_cache_entries);

/*
 * Function returning all workfile cache entries for one segment.
 *
 * The implementation is in workfile_mgr.c, this is just a shim.
 */
Datum
gp_workfile_mgr_cache_entries(PG_FUNCTION_ARGS)
{
	return gp_workfile_mgr_cache_entries_internal(fcinfo);
}

PG_FUNCTION_INFO_V1(gp_workfile_mgr_used_diskspace);

/*
 * Returns the number of bytes used for workfiles on a segment
 * according to WorkfileDiskspace
 */
Datum
gp_workfile_mgr_used_diskspace(PG_FUNCTION_ARGS)
{
	/*
	 * Build a tuple descriptor for our result type
	 * The number and type of attributes have to match the definition of the
	 * view gp_workfile_mgr_diskspace
	 */
#define NUM_USED_DISKSPACE_ELEM 2
	TupleDesc tupdesc = CreateTemplateTupleDesc(NUM_USED_DISKSPACE_ELEM);

	TupleDescInitEntry(tupdesc, (AttrNumber) 1, "segid",
			INT4OID, -1 /* typmod */, 0 /* attdim */);
	TupleDescInitEntry(tupdesc, (AttrNumber) 2, "bytes",
			INT8OID, -1 /* typmod */, 0 /* attdim */);

	tupdesc =  BlessTupleDesc(tupdesc);

	Datum		values[NUM_USED_DISKSPACE_ELEM];
	bool		nulls[NUM_USED_DISKSPACE_ELEM];
	MemSet(nulls, 0, sizeof(nulls));

	values[0] = Int32GetDatum(GpIdentity.segindex);
	values[1] = Int64GetDatum(WorkfileSegspace_GetSize());

	HeapTuple tuple = heap_form_tuple(tupdesc, values, nulls);
	Datum result = HeapTupleGetDatum(tuple);

	PG_RETURN_DATUM(result);
}

