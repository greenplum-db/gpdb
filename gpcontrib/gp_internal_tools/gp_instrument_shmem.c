/*-------------------------------------------------------------------------
 *
 * gp_instrument_shmem.c
 *    Functions for diagnos Instrumentation Shmem slots
 *
 * Copyright (c) 2017-Present VMware, Inc. or its affiliates.
 *
 *-------------------------------------------------------------------------
*/
#include "postgres.h"
#include "funcapi.h"
#include "cdb/cdbvars.h"
#include "utils/builtins.h"
#include "executor/instrument.h"

PG_MODULE_MAGIC;

Datum		gp_instrument_shmem_summary(PG_FUNCTION_ARGS);
Datum		gp_instrument_shmem_detail(PG_FUNCTION_ARGS);

/* Helper functions */
static InstrumentationSlot *next_used_slot(int32 *);

PG_FUNCTION_INFO_V1(gp_instrument_shmem_summary);
PG_FUNCTION_INFO_V1(gp_instrument_shmem_detail);

#define GET_SLOT_BY_INDEX(index) ((InstrumentationSlot*)(InstrumentGlobal + 1) + (index))

/*
 * Get summary of shmem instrument slot usage
 *
 * ---------------------------------------------------------------------
 * Interface to gp_instrument_shmem_summary function.
 *
 * The gp_instrument_shmem_summary function get the summary of shmem instrument slot usage.
 * It can be invoked by creating a function via psql that references it.
 * For example,
 *
 * CREATE FUNCTION gp_instrument_shmem_summary()
 *   RETURNS TABLE ( segid int4
 *   				,num_free int8
 *   				,num_used int8
 *                 )
 *   AS '$libdir/gp_instrument_shmem', 'gp_instrument_shmem_summary' LANGUAGE C IMMUTABLE;
 */
Datum
gp_instrument_shmem_summary(PG_FUNCTION_ARGS)
{
	TupleDesc	tupdesc;
#define GP_INSTRUMENT_SHMEM_SUMMARY_NATTR 3

	tupdesc = CreateTemplateTupleDesc(GP_INSTRUMENT_SHMEM_SUMMARY_NATTR);
	TupleDescInitEntry(tupdesc, (AttrNumber) 1, "segid", INT4OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 2, "num_free", INT8OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 3, "num_used", INT8OID, -1, 0);
	tupdesc = BlessTupleDesc(tupdesc);

	Datum		values[GP_INSTRUMENT_SHMEM_SUMMARY_NATTR];
	bool		nulls[GP_INSTRUMENT_SHMEM_SUMMARY_NATTR];

	MemSet(nulls, 0, sizeof(nulls));

	values[0] = Int32GetDatum(GpIdentity.segindex);
	if (InstrumentGlobal)
	{
		values[1] = Int64GetDatum(InstrumentGlobal->free);
		values[2] = Int64GetDatum(InstrShmemNumSlots() - InstrumentGlobal->free);
	}
	else
	{
		nulls[1] = true;
		nulls[2] = true;
	}
	HeapTuple	tuple = heap_form_tuple(tupdesc, values, nulls);
	Datum		result = HeapTupleGetDatum(tuple);

	PG_RETURN_DATUM(result);
}

static InstrumentationSlot *
next_used_slot(int32 *crtIndexPtr)
{
	if (InstrumentGlobal == NULL)
		return NULL;

	while (*crtIndexPtr < InstrShmemNumSlots() && SlotIsEmpty(GET_SLOT_BY_INDEX(*crtIndexPtr)))
		(*crtIndexPtr)++;
	return *crtIndexPtr >= InstrShmemNumSlots() ? NULL : GET_SLOT_BY_INDEX((*crtIndexPtr)++);
}

/*
 * Get summary of shmem instrument slot usage
 *
 * ---------------------------------------------------------------------
 * Interface to gp_instrument_shmem_detail function.
 *
 * The gp_instrument_shmem_detail function get the detail of shmem instrument slot usage.
 * It can be invoked by creating a function via psql that references it.
 * For example,
 *
 * CREATE FUNCTION gp_instrument_shmem_detail()
 *   RETURNS TABLE ( tmid int4
 *   				,ssid int4
 *   				,ccnt int2
 *   				,segid int2
 *   				,pid int4
 *   				,nid int2
 *   				,tuplecount int8
 *   				,nloops int8
 *   				,ntuples int8
 *                 )
 *   AS '$libdir/gp_instrument_shmem', 'gp_instrument_shmem_detail' LANGUAGE C IMMUTABLE;
 */
Datum
gp_instrument_shmem_detail(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	int32	   *crtIndexPtr;
	int			nattr = 9;

	if (SRF_IS_FIRSTCALL())
	{
		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/* Switch to memory context appropriate for multiple function calls */
		MemoryContext oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		TupleDesc	tupdesc = CreateTemplateTupleDesc(nattr);

		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "tmid", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "ssid", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "ccnt", INT2OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "segid", INT2OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "pid", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 6, "nid", INT2OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 7, "tuplecount", INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 8, "nloops", INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 9, "ntuples", INT8OID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		crtIndexPtr = (int32 *) palloc(sizeof(*crtIndexPtr));
		*crtIndexPtr = 0;
		funcctx->user_fctx = crtIndexPtr;
		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	crtIndexPtr = (int32 *) funcctx->user_fctx;
	while (true)
	{
		InstrumentationSlot *slot = next_used_slot(crtIndexPtr);

		if (slot == NULL)
		{
			/* Reached the end of the entry array, we're done */
			SRF_RETURN_DONE(funcctx);
		}
#define GP_INSTRUMENT_SHMEM_DETAIL_NATTR 9
		Datum		values[GP_INSTRUMENT_SHMEM_DETAIL_NATTR];
		bool		nulls[GP_INSTRUMENT_SHMEM_DETAIL_NATTR];

		memset(nulls, 0, sizeof(nulls));

		values[0] = Int32GetDatum((slot->tmid));
		values[1] = Int32GetDatum(slot->ssid);
		values[2] = Int16GetDatum(slot->ccnt);
		values[3] = Int16GetDatum(slot->segid);
		values[4] = Int32GetDatum(slot->pid);
		values[5] = Int16GetDatum(slot->nid);
		values[6] = Int64GetDatum((int64) ((slot->data).tuplecount));
		values[7] = Int64GetDatum((int64) ((slot->data).ntuples));
		values[8] = Int64GetDatum((int64) ((slot->data).nloops));

		HeapTuple	tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		Datum		result = HeapTupleGetDatum(tuple);

		SRF_RETURN_NEXT(funcctx, result);
	}
}
