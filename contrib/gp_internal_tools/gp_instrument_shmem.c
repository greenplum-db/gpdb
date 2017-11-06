/*-------------------------------------------------------------------------
 *
 * gp_instrument_shmem.c
 *    Functions for diagnos Instrumentation Shmem slots
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc.
 *
 *-------------------------------------------------------------------------
*/
#include "postgres.h"
#include "funcapi.h"
#include "cdb/cdbvars.h"
#include "utils/builtins.h"
#include "executor/instrument.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(gp_instrument_shmem_summary);
/*
 * Get summary of shmem instrument slot usage
 */
Datum
gp_instrument_shmem_summary(PG_FUNCTION_ARGS)
{
	TupleDesc	tupdesc;
	int			nattr = 3;

	tupdesc = CreateTemplateTupleDesc(nattr, false);
	TupleDescInitEntry(tupdesc, (AttrNumber) 1, "segid", INT4OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 2, "num_free", INT8OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 3, "num_used", INT8OID, -1, 0);
	tupdesc = BlessTupleDesc(tupdesc);

	Datum		values[nattr];
	bool		nulls[nattr];

	MemSet(nulls, 0, sizeof(nulls));

	values[0] = Int32GetDatum(Gp_segment);
	if (InstrumentGlobal)
	{
		values[1] = Int64GetDatum(InstrumentGlobal->free);
		values[2] = Int64GetDatum(InstrumentGlobal->used);
	}
	else
	{
		values[1] = Int64GetDatum(0);
		values[2] = Int64GetDatum(0);
	}
	HeapTuple	tuple = heap_form_tuple(tupdesc, values, nulls);
	Datum		result = HeapTupleGetDatum(tuple);

	PG_RETURN_DATUM(result);
}
