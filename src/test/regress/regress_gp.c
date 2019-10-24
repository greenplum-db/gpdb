/*------------------------------------------------------------------------
 *
 * regress_gp.c
 *	 Greenplum specific code for various C-language functions defined as
 *	 part of the regression tests.
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 * src/test/regress/regress_gp.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "funcapi.h"
#include "tablefuncapi.h"
#include "miscadmin.h"

#include <float.h>
#include <math.h>
#include <unistd.h>

#include "libpq-fe.h"
#include "pgstat.h"
#include "access/transam.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/pg_language.h"
#include "catalog/pg_type.h"
#include "cdb/memquota.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbfts.h"
#include "cdb/cdbgang.h"
#include "cdb/cdbvars.h"
#include "cdb/ml_ipc.h"
#include "commands/sequence.h"
#include "commands/trigger.h"
#include "executor/executor.h"
#include "executor/spi.h"
#include "port/atomics.h"
#include "parser/parse_expr.h"
#include "storage/bufmgr.h"
#include "storage/buf_internals.h"
#include "libpq/auth.h"
#include "libpq/hba.h"
#include "utils/builtins.h"
#include "utils/geo_decls.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/resource_manager.h"
#include "utils/timestamp.h"

/* table_functions test */
extern Datum multiset_example(PG_FUNCTION_ARGS);
extern Datum multiset_scalar_null(PG_FUNCTION_ARGS);
extern Datum multiset_scalar_value(PG_FUNCTION_ARGS);
extern Datum multiset_scalar_tuple(PG_FUNCTION_ARGS);
extern Datum multiset_setof_null(PG_FUNCTION_ARGS);
extern Datum multiset_setof_value(PG_FUNCTION_ARGS);
extern Datum multiset_materialize_good(PG_FUNCTION_ARGS);
extern Datum multiset_materialize_bad(PG_FUNCTION_ARGS);

/* table functions + dynamic type support */
extern Datum sessionize(PG_FUNCTION_ARGS);
extern Datum describe(PG_FUNCTION_ARGS);
extern Datum project(PG_FUNCTION_ARGS);
extern Datum project_describe(PG_FUNCTION_ARGS);
extern Datum noop_project(PG_FUNCTION_ARGS);
extern Datum userdata_describe(PG_FUNCTION_ARGS);
extern Datum userdata_project(PG_FUNCTION_ARGS);

/* Resource queue/group support */
extern Datum checkResourceQueueMemoryLimits(PG_FUNCTION_ARGS);
extern Datum repeatPalloc(PG_FUNCTION_ARGS);
extern Datum resGroupPalloc(PG_FUNCTION_ARGS);

/* Gang management test support */
extern Datum gangRaiseInfo(PG_FUNCTION_ARGS);
extern Datum cleanupAllGangs(PG_FUNCTION_ARGS);
extern Datum hasGangsExist(PG_FUNCTION_ARGS);
extern Datum numActiveMotionConns(PG_FUNCTION_ARGS);
extern Datum hasBackendsExist(PG_FUNCTION_ARGS);

/* Transient types */
extern Datum assign_new_record(PG_FUNCTION_ARGS);

/* guc_env_var */
extern Datum udf_setenv(PG_FUNCTION_ARGS);
extern Datum udf_unsetenv(PG_FUNCTION_ARGS);

/* Auth Constraints */
extern Datum check_auth_time_constraints(PG_FUNCTION_ARGS);

/* XID wraparound */
extern Datum test_consume_xids(PG_FUNCTION_ARGS);
extern Datum gp_execute_on_server(PG_FUNCTION_ARGS);

/* Check shared buffer cache for a database Oid */
extern Datum check_shared_buffer_cache_for_dboid(PG_FUNCTION_ARGS);

/* oid wraparound tests */
extern Datum gp_set_next_oid(PG_FUNCTION_ARGS);
extern Datum gp_get_next_oid(PG_FUNCTION_ARGS);

/* Broken output function, for testing */
extern Datum broken_int4out(PG_FUNCTION_ARGS);

/* fts tests */
extern Datum gp_fts_probe_stats(PG_FUNCTION_ARGS);

/* Triggers */

typedef struct
{
	char	   *ident;
	int			nplans;
	void	  **splan;
}	EPlan;

static EPlan *FPlans = NULL;
static int	nFPlans = 0;
static EPlan *PPlans = NULL;
static int	nPPlans = 0;

extern Datum check_primary_key(PG_FUNCTION_ARGS);
extern Datum check_foreign_key(PG_FUNCTION_ARGS);
extern Datum autoinc(PG_FUNCTION_ARGS);
static EPlan *find_plan(char *ident, EPlan ** eplan, int *nplans);

extern Datum trigger_udf_return_new_oid(PG_FUNCTION_ARGS);


PG_FUNCTION_INFO_V1(multiset_scalar_null);
Datum
multiset_scalar_null(PG_FUNCTION_ARGS)
{
	PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(multiset_scalar_value);
Datum
multiset_scalar_value(PG_FUNCTION_ARGS)
{
	AnyTable             scan;
	HeapTuple            tuple;
	TupleDesc            in_tupdesc;
	Datum                values[1];
	bool				 nulls[1];
	int					 result;

	/*
	 * Sanity checking, shouldn't occur if our CREATE FUNCTION in SQL is done
	 * correctly.
	 */
	if (PG_NARGS() < 1 || PG_ARGISNULL(0))
		elog(ERROR, "invalid invocation of multiset_scalar_value");
	scan = PG_GETARG_ANYTABLE(0);  /* Should be the first parameter */

	/* Get the next value from the input scan */
	in_tupdesc  = AnyTable_GetTupleDesc(scan);
	tuple       = AnyTable_GetNextTuple(scan);

	/* check for end of scan */
	if (tuple == NULL)
		PG_RETURN_NULL();

	/*
	 * We expect an input of one integer column for this stupid
	 * table function, if that is not what we got then complain.
	 */
	if (in_tupdesc->natts != 1 || in_tupdesc->attrs[0]->atttypid != INT4OID)
	{
		ereport(ERROR,
				(errcode(ERRCODE_CANNOT_COERCE),
				 errmsg("invalid input tuple for function multiset_scalar_value"),
				 errhint("expected (integer, text) ")));
	}

	/* -----
	 * Extract fields from input tuple, there are several possibilities
	 * depending on if we want to fetch the rows by name, by number, or extract
	 * the full tuple contents.
	 *
	 *    - values[0] = GetAttributeByName(tuple->t_data, "a", &nulls[0]);
	 *    - values[0] = GetAttributeByNum(tuple->t_data, 1, &nulls[0]);
	 *    - heap_deform_tuple(tuple, in_tupdesc, values, nulls);
	 *
	 * In this case we have chosen to use getAttributeByNum
	 */
	values[0] = heap_getattr(tuple, 1, in_tupdesc, &nulls[0]);

	/* Handle NULL */
	if (nulls[0])
		PG_RETURN_NULL();

	/*
	 * Convert the Datum to an integer so we can operate on it.
	 *
	 * Since we are just going to return it directly we could skip this step,
	 * and simply call PG_RETURN_DATUM(values[0]), but this is more illustrative.
	 */
	result = DatumGetInt32(values[0]);

	PG_RETURN_INT32(result);
}

PG_FUNCTION_INFO_V1(multiset_scalar_tuple);
Datum
multiset_scalar_tuple(PG_FUNCTION_ARGS)
{
	ReturnSetInfo *rsi;
	TupleDesc	tupdesc;
	HeapTuple	tuple;
	Datum		values[2];
	bool		nulls[2];
	Datum		result;

	/* Double check that the output tupledesc is what we expect */
	rsi		= (ReturnSetInfo *) fcinfo->resultinfo;
	tupdesc	= rsi->expectedDesc;

	if (tupdesc->natts != 2 ||
		(tupdesc->attrs[0]->atttypid != INT4OID && !tupdesc->attrs[0]->attisdropped) ||
		(tupdesc->attrs[1]->atttypid != TEXTOID && !tupdesc->attrs[1]->attisdropped))
	{
		ereport(ERROR,
				(errcode(ERRCODE_CANNOT_COERCE),
				 errmsg("invalid output tupledesc for function multiset_scalar_tuple"),
				 errhint("Expected (integer, text).")));
	}

	/* Populate an output tuple. */
	values[0] = Int32GetDatum(1);
	values[1] = CStringGetTextDatum("Example");
	nulls[0] = nulls[1] = false;
	tuple = heap_form_tuple(tupdesc, values, nulls);
	result = HeapTupleGetDatum(tuple);

	PG_RETURN_DATUM(result);
}

PG_FUNCTION_INFO_V1(multiset_setof_null);
Datum
multiset_setof_null(PG_FUNCTION_ARGS)
{
	FuncCallContext		*fctx;
	ReturnSetInfo 		*rsi;

	if (SRF_IS_FIRSTCALL())
	{
		fctx = SRF_FIRSTCALL_INIT();
	}
	fctx = SRF_PERCALL_SETUP();

	/* This is just one way we might test that we are done: */
	if (fctx->call_cntr < 3)
	{
		/*
		 * set returning functions shouldn't return NULL results, in order to
		 * due so you need to sidestep the normal SRF_RETURN_NEXT mechanism.
		 * This is an error-condition test, not correct coding practices.
		 */
		rsi	= (ReturnSetInfo *) fcinfo->resultinfo;
		rsi->isDone = ExprMultipleResult;
		fctx->call_cntr++; /* would happen automatically with SRF_RETURN_NEXT */
		PG_RETURN_NULL();  /* see above: only for testing, don't do this */
	}
	else
	{
		/* do any user specific cleanup on last call */
		SRF_RETURN_DONE(fctx);
	}
}

PG_FUNCTION_INFO_V1(multiset_setof_value);
Datum
multiset_setof_value(PG_FUNCTION_ARGS)
{
	FuncCallContext		*fctx;

	if (SRF_IS_FIRSTCALL())
	{
		fctx = SRF_FIRSTCALL_INIT();
	}
	fctx = SRF_PERCALL_SETUP();

	/* This is just one way we might test that we are done: */
	if (fctx->call_cntr < 3)
	{
		SRF_RETURN_NEXT(fctx, Int32GetDatum(fctx->call_cntr));
	}
	else
	{
		/* do any user specific cleanup on last call */
		SRF_RETURN_DONE(fctx);
	}
}

PG_FUNCTION_INFO_V1(multiset_materialize_good);
Datum
multiset_materialize_good(PG_FUNCTION_ARGS)
{
	FuncCallContext		*fctx;
	ReturnSetInfo 		*rsi = (ReturnSetInfo *) fcinfo->resultinfo;

	/*
	 * To return SFRM_Materialize the correct convention is to first
	 * check if this is an allowed mode.  Currently it is not, so we
	 * expect this to raise an error.
	 */
	if (!rsi || !IsA(rsi, ReturnSetInfo) ||
		(rsi->allowedModes & SFRM_Materialize) == 0 ||
		rsi->expectedDesc == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("set-valued function called in context that cannot accept a set")));
	}


	if (SRF_IS_FIRSTCALL())
	{
		fctx = SRF_FIRSTCALL_INIT();
	}
	fctx = SRF_PERCALL_SETUP();
	SRF_RETURN_DONE(fctx);
}

PG_FUNCTION_INFO_V1(multiset_materialize_bad);
Datum
multiset_materialize_bad(PG_FUNCTION_ARGS)
{
	ReturnSetInfo *rsi = (ReturnSetInfo *) fcinfo->resultinfo;

	/*
	 * This function is "bad" because it does not check if the caller
	 * supports SFRM_Materialize before trying to return a materialized
	 * set.
	 */
	rsi->returnMode = SFRM_Materialize;
	PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(multiset_example);
Datum
multiset_example(PG_FUNCTION_ARGS)
{
	FuncCallContext		*fctx;
	ReturnSetInfo 		*rsi;
	AnyTable             scan;
	HeapTuple            tuple;
	TupleDesc            in_tupdesc;
	TupleDesc            out_tupdesc;
	Datum                tup_datum;
	Datum                values[2];
	bool				 nulls[2];

	/*
	 * Sanity checking, shouldn't occur if our CREATE FUNCTION in SQL is done
	 * correctly.
	 */
	if (PG_NARGS() < 1 || PG_ARGISNULL(0))
		elog(ERROR, "invalid invocation of multiset_example");
	scan = PG_GETARG_ANYTABLE(0);  /* Should be the first parameter */

	/* Basic set-returning function (SRF) protocol, setup the context */
	if (SRF_IS_FIRSTCALL())
	{
		fctx = SRF_FIRSTCALL_INIT();
	}
	fctx = SRF_PERCALL_SETUP();

	/* Get the next value from the input scan */
	rsi			= (ReturnSetInfo *) fcinfo->resultinfo;
	out_tupdesc = rsi->expectedDesc;
	in_tupdesc  = AnyTable_GetTupleDesc(scan);
	tuple       = AnyTable_GetNextTuple(scan);

	/* check for end of scan */
	if (tuple == NULL)
		SRF_RETURN_DONE(fctx);

	/*
	 * We expect an input/output of two columns (int, text) for this stupid
	 * table function, if that is not what we got then complain.
	 */
	if (in_tupdesc->natts != 2 ||
		in_tupdesc->attrs[0]->atttypid != INT4OID ||
		in_tupdesc->attrs[1]->atttypid != TEXTOID)
	{
		ereport(ERROR,
				(errcode(ERRCODE_CANNOT_COERCE),
				 errmsg("invalid input tuple for function multiset_example"),
				 errhint("Expected (integer, text).")));
	}

	/* For output tuple we also check for possibility of dropped columns */
	if (out_tupdesc->natts != 2 ||
		(out_tupdesc->attrs[0]->atttypid != INT4OID && !out_tupdesc->attrs[0]->attisdropped) ||
		(out_tupdesc->attrs[1]->atttypid != TEXTOID && !out_tupdesc->attrs[1]->attisdropped))
	{
		ereport(ERROR,
				(errcode(ERRCODE_CANNOT_COERCE),
				 errmsg("invalid output tuple for function multiset_example"),
				 errhint("Expected (integer, text).")));
	}


	/* -----
	 * Extract fields from input tuple, there are several possibilities
	 * depending on if we want to fetch the rows by name, by number, or extract
	 * the full tuple contents.
	 *
	 *    - values[0] = GetAttributeByName(tuple->t_data, "a", &nulls[0]);
	 *    - values[0] = GetAttributeByNum(tuple->t_data, 0, &nulls[0]);
	 *    - heap_deform_tuple(tuple, in_tupdesc, values, nulls);
	 *
	 * In this case we have chosen to do the whole tuple at once.
	 */
	heap_deform_tuple(tuple, in_tupdesc, values, nulls);

	/*
	 * Since we have already validated types we can form this directly
	 * into our output tuple without additional conversion.
	 */
	tuple = heap_form_tuple(out_tupdesc, values, nulls);

	/*
	 * Final output must always be a Datum, so convert the tuple as required
	 * by the API.
	 */
	tup_datum = HeapTupleGetDatum(tuple);

	/* Extract values from input tuple, build output tuple */
	SRF_RETURN_NEXT(fctx, tup_datum);
}

/*
 * Checks if memory limit of resource queue is in sync across
 * shared memory and catalog.
 * This function should ONLY be used for unit testing.
 */
PG_FUNCTION_INFO_V1(checkResourceQueueMemoryLimits);
Datum
checkResourceQueueMemoryLimits(PG_FUNCTION_ARGS)
{
	char *queueName = PG_GETARG_CSTRING(0);
	Oid queueId;
	ResQueue queue;
	double v1, v2;

	if (!IsResQueueEnabled())
		return (Datum)0;

	if (queueName == NULL)
		return (Datum)0;

	/* get OID for queue */
	queueId = GetResQueueIdForName(queueName);

	if (queueId == InvalidOid)
		return (Datum)0;

	/* ResQueueHashFind needs a lock */
	LWLockAcquire(ResQueueLock, LW_EXCLUSIVE);

	/* get shared memory version of queue */
	queue = ResQueueHashFind(queueId);

	LWLockRelease(ResQueueLock);

	if (!queue)
		return (Datum) 0;

	v1 = ceil(queue->limits[RES_MEMORY_LIMIT].threshold_value);
	v2 = ceil((double) ResourceQueueGetMemoryLimitInCatalog(queueId));

	PG_RETURN_BOOL(v1 == v2);
}

/*
 * Helper function to raise an INFO with options including DETAIL, HINT
 * From PostgreSQL 8.4, we can do this in plpgsql by RAISE statement, but now we
 * use PL/C.
 */
PG_FUNCTION_INFO_V1(gangRaiseInfo);
Datum
gangRaiseInfo(PG_FUNCTION_ARGS)
{
	ereport(INFO,
			(errmsg("testing hook function MPPnoticeReceiver"),
			 errdetail("this test aims at covering code paths not hit before"),
			 errhint("no special hint"),
			 errcontext("PL/C function defined in regress.c"),
			 errposition(0)));

	PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(cleanupAllGangs);
Datum
cleanupAllGangs(PG_FUNCTION_ARGS)
{
	if (Gp_role != GP_ROLE_DISPATCH)
		elog(ERROR, "cleanupAllGangs can only be executed on master");
	DisconnectAndDestroyAllGangs(false);
	PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(hasGangsExist);
Datum
hasGangsExist(PG_FUNCTION_ARGS)
{
	if (Gp_role != GP_ROLE_DISPATCH)
		elog(ERROR, "hasGangsExist can only be executed on master");
	if (cdbcomponent_qesExist())
		PG_RETURN_BOOL(true);
	PG_RETURN_BOOL(false);
}

PG_FUNCTION_INFO_V1(numActiveMotionConns);
Datum numActiveMotionConns(PG_FUNCTION_ARGS)
{
	uint32 num = 0;
	if (Gp_interconnect_type == INTERCONNECT_TYPE_UDPIFC)
		num = getActiveMotionConns();
	PG_RETURN_UINT32(num);
}


PG_FUNCTION_INFO_V1(assign_new_record);
Datum
assign_new_record(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx = NULL;

	if (SRF_IS_FIRSTCALL())
	{
		funcctx = SRF_FIRSTCALL_INIT();
		TupleDesc	tupdesc;

		tupdesc = CreateTemplateTupleDesc(1, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "c", INT4OID, -1, 0);

		BlessTupleDesc(tupdesc);
		funcctx->tuple_desc = tupdesc;

		/* dummy output */
		funcctx->max_calls = 10;
	}

	if (Gp_role == GP_ROLE_DISPATCH)
		SRF_RETURN_DONE(funcctx);

	/* stuff done on every call of the function */
	funcctx = SRF_PERCALL_SETUP();

	if (funcctx->call_cntr < funcctx->max_calls)
	{
		TupleDesc	tupdesc;
		HeapTuple	tuple;
		Datum		dummy_values[1];
		bool		dummy_nulls[1];
		int			i;

		tupdesc = CreateTemplateTupleDesc(funcctx->call_cntr, false);

		dummy_values[0] = Int32GetDatum(1);
		dummy_nulls[0] = false;

		for (i = 1; i <= funcctx->call_cntr; i++)
			TupleDescInitEntry(tupdesc, (AttrNumber) i, "c", INT4OID, -1, 0);

		BlessTupleDesc(tupdesc);

		tuple = heap_form_tuple(funcctx->tuple_desc, dummy_values, dummy_nulls);

		SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
	}
	else
	{
		/* nothing left */
		SRF_RETURN_DONE(funcctx);
	}
}

/*
 * GPDB_95_MERGE_FIXME: Commit d7cdf6ee36a introduce a similar function to this
 * one with regress_putenv().  When we catch to 9.5 we should switch over to
 * using that.
 */
PG_FUNCTION_INFO_V1(udf_setenv);
Datum
udf_setenv(PG_FUNCTION_ARGS)
{
	const char *name = (const char *) PG_GETARG_CSTRING(0);
	const char *value = (const char *) PG_GETARG_CSTRING(1);
	int ret = setenv(name, value, 1);

	PG_RETURN_BOOL(ret == 0);
}

PG_FUNCTION_INFO_V1(udf_unsetenv);
Datum
udf_unsetenv(PG_FUNCTION_ARGS)
{
	const char *name = (const char *) PG_GETARG_CSTRING(0);
	int ret = unsetenv(name);
	PG_RETURN_BOOL(ret == 0);
}

PG_FUNCTION_INFO_V1(repeatPalloc);
Datum
repeatPalloc(PG_FUNCTION_ARGS)
{
	int32 size = PG_GETARG_INT32(0);
	int32 count = PG_GETARG_INT32(1);
	int i;

	for (i = 0; i < count; i++)
		MemoryContextAlloc(TopMemoryContext, size * 1024 * 1024);

	PG_RETURN_INT32(0);
}

PG_FUNCTION_INFO_V1(resGroupPalloc);
Datum
resGroupPalloc(PG_FUNCTION_ARGS)
{
	float ratio = PG_GETARG_FLOAT8(0);
	int memLimit, slotQuota, sharedQuota;
	int size;
	int count;
	int i;

	if (!IsResGroupEnabled())
		PG_RETURN_INT32(0);

	ResGroupGetMemInfo(&memLimit, &slotQuota, &sharedQuota);
	size = ceilf(memLimit * ratio);
	count = size / 512;
	for (i = 0; i < count; i++)
		MemoryContextAlloc(TopMemoryContext, 512 * 1024 * 1024);

	size %= 512;
	MemoryContextAlloc(TopMemoryContext, size * 1024 * 1024);

	PG_RETURN_INT32(0);
}

/*
 * This is do-nothing table function that passes the input relation
 * to the output relation without any modification.
 */
PG_FUNCTION_INFO_V1(noop_project);
Datum
noop_project(PG_FUNCTION_ARGS)
{
	AnyTable			scan;
	FuncCallContext	   *fctx;
	ReturnSetInfo	   *rsi;
	HeapTuple			tuple;

	scan = PG_GETARG_ANYTABLE(0);
	if (SRF_IS_FIRSTCALL())
	{
		fctx = SRF_FIRSTCALL_INIT();
	}
	fctx = SRF_PERCALL_SETUP();
	rsi = (ReturnSetInfo *) fcinfo->resultinfo;
	tuple = AnyTable_GetNextTuple(scan);
	if (!tuple)
		SRF_RETURN_DONE(fctx);

	SRF_RETURN_NEXT(fctx, HeapTupleGetDatum(tuple));
}

/*
 * sessionize
 */
typedef struct session_state {
	int			id;
	Timestamp	time;
	int			counter;
} session_state;

PG_FUNCTION_INFO_V1(sessionize);
Datum
sessionize(PG_FUNCTION_ARGS)
{
	FuncCallContext		*fctx;
	ReturnSetInfo 		*rsi;
	AnyTable             scan;
	HeapTuple            tuple;
	TupleDesc            in_tupdesc;
	TupleDesc            out_tupdesc;
	Datum                tup_datum;
	Datum                values[3];
	bool				 nulls[3];
	session_state       *state;
	int                  newId;
	Timestamp            newTime;
	Interval            *threshold;

	/*
	 * Sanity checking, shouldn't occur if our CREATE FUNCTION in SQL is done
	 * correctly.
	 */
	if (PG_NARGS() != 2 || PG_ARGISNULL(0) || PG_ARGISNULL(1))
		elog(ERROR, "invalid invocation of sessionize");
	scan = PG_GETARG_ANYTABLE(0);  /* Should be the first parameter */
	threshold = PG_GETARG_INTERVAL_P(1);

	/* Basic set-returning function (SRF) protocol, setup the context */
	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldcontext;

		fctx = SRF_FIRSTCALL_INIT();

		oldcontext = MemoryContextSwitchTo(fctx->multi_call_memory_ctx);

		state = (session_state*) palloc0(sizeof(session_state));
		fctx->user_fctx = (void*) state;
		state->id = -9999;  /* gross hack: stupid special value for demo */

		MemoryContextSwitchTo(oldcontext);
	}
	fctx = SRF_PERCALL_SETUP();
	state = (session_state*) fctx->user_fctx;

	/* Get the next value from the input scan */
	rsi			= (ReturnSetInfo *) fcinfo->resultinfo;
	out_tupdesc = rsi->expectedDesc;
	in_tupdesc  = AnyTable_GetTupleDesc(scan);
	tuple       = AnyTable_GetNextTuple(scan);

	/* check for end of scan */
	if (tuple == NULL)
		SRF_RETURN_DONE(fctx);

	/*
	 * We expect an input/output of two columns (int, text) for this stupid
	 * table function, if that is not what we got then complain.
	 */
	if (in_tupdesc->natts != 2 ||
		in_tupdesc->attrs[0]->atttypid != INT4OID ||
		in_tupdesc->attrs[1]->atttypid != TIMESTAMPOID)
	{
		ereport(ERROR,
				(errcode(ERRCODE_CANNOT_COERCE),
				 errmsg("invalid input tuple for function sessionize"),
				 errhint("Expected (integer, timestamp).")));
	}

	/* For output tuple we also check for possibility of dropped columns */
	if (out_tupdesc->natts != 3 ||
		(out_tupdesc->attrs[0]->atttypid != INT4OID && !out_tupdesc->attrs[0]->attisdropped) ||
		(out_tupdesc->attrs[1]->atttypid != TIMESTAMPOID && !out_tupdesc->attrs[1]->attisdropped) ||
		(out_tupdesc->attrs[2]->atttypid != INT4OID && !out_tupdesc->attrs[2]->attisdropped))
	{
		ereport(ERROR,
				(errcode(ERRCODE_CANNOT_COERCE),
				 errmsg("invalid output tuple for function sessionize"),
				 errhint("Expected (integer, timestamp, integer).")));
	}

	/* -----
	 * Extract fields from input tuple, there are several possibilities
	 * depending on if we want to fetch the rows by name, by number, or extract
	 * the full tuple contents.
	 *
	 *    - values[0] = GetAttributeByName(tuple->t_data, "a", &nulls[0]);
	 *    - values[0] = GetAttributeByNum(tuple->t_data, 0, &nulls[0]);
	 *    - heap_deform_tuple(tuple, in_tupdesc, values, nulls);
	 *
	 * In this case we have chosen to do the whole tuple at once.
	 */
	heap_deform_tuple(tuple, in_tupdesc, values, nulls);
	newId = DatumGetInt32(values[0]);
	newTime = DatumGetTimestamp(values[1]);

	/* just skip null input */
	if (nulls[0] || nulls[1])
	{
		nulls[2] = true;
	}
	else
	{
		nulls[2] = false;

		/* handle state transition */
		if (newId == state->id)
		{
			Datum d;

			/* Calculate old timestamp + interval */
			d = DirectFunctionCall2(timestamp_pl_interval,
									TimestampGetDatum(state->time),
									IntervalPGetDatum(threshold));

			/* if that is less than new interval then bump counter */
			d = DirectFunctionCall2(timestamp_lt, d, TimestampGetDatum(newTime));

			if (DatumGetBool(d))
				state->counter++;
			state->time = newTime;
		}
		else
		{
			state->id	   = newId;
			state->time	   = newTime;
			state->counter = 1;
		}
	}

	/*
	 * Since we have already validated types we can form this directly
	 * into our output tuple without additional conversion.
	 */
	values[2] = Int32GetDatum(state->counter);
	tuple = heap_form_tuple(out_tupdesc, values, nulls);

	/*
	 * Final output must always be a Datum, so convert the tuple as required
	 * by the API.
	 */
	tup_datum = HeapTupleGetDatum(tuple);

	/* Extract values from input tuple, build output tuple */
	SRF_RETURN_NEXT(fctx, tup_datum);
}

/*
 * The absolute simplest of describe functions, ignore input and always return
 * the same tuple descriptor.  This is effectively the same as statically
 * defining the type in the CREATE TABLE definition, but this time we have
 * pushed it into a dynamic call time resolution context.
 */
PG_FUNCTION_INFO_V1(describe);
Datum
describe(PG_FUNCTION_ARGS)
{
	FuncExpr   *fexpr;
	TupleDesc	tupdesc;

	if (PG_NARGS() != 1 || PG_ARGISNULL(0))
		elog(ERROR, "invalid invocation of describe");

	fexpr = (FuncExpr*) PG_GETARG_POINTER(0);
	if (!IsA(fexpr, FuncExpr))
		ereport(ERROR, (errmsg("invalid parameters for describe")));

	/* Build a result tuple descriptor */
	tupdesc = CreateTemplateTupleDesc(3, false);
	TupleDescInitEntry(tupdesc, (AttrNumber) 1, "id", INT4OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 2, "time", TIMESTAMPOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 3, "sessionnum", INT4OID, -1, 0);

	PG_RETURN_POINTER(tupdesc);
}

PG_FUNCTION_INFO_V1(gp_fts_probe_stats);
Datum
gp_fts_probe_stats(PG_FUNCTION_ARGS)
{
	Assert(GpIdentity.dbid == MASTER_DBID);

	TupleDesc	tupdesc;
	int32		start_count = 0;
	int32		done_count = 0;
	uint8		status_version = 0;

	SpinLockAcquire(&ftsProbeInfo->lock);
	start_count = ftsProbeInfo->start_count;
	done_count    = ftsProbeInfo->done_count;
	status_version = ftsProbeInfo->status_version;
	SpinLockRelease(&ftsProbeInfo->lock);

	/* Build a result tuple descriptor */
	tupdesc = CreateTemplateTupleDesc(3, false);
	TupleDescInitEntry(tupdesc, (AttrNumber) 1, "start_count", INT4OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 2, "end_count", INT4OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 3, "status_version", INT2OID, -1, 0);

	tupdesc = BlessTupleDesc(tupdesc);

	{
		Datum values[3];
		bool nulls[3];
		HeapTuple tuple;
		Datum result;
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));

		values[0] = Int32GetDatum(start_count);
		values[1] = Int32GetDatum(done_count);
		values[2] = UInt8GetDatum(status_version);

		tuple = heap_form_tuple(tupdesc, values, nulls);
		result = HeapTupleGetDatum(tuple);
		PG_RETURN_DATUM(result);
	}
}

PG_FUNCTION_INFO_V1(project);
Datum
project(PG_FUNCTION_ARGS)
{
	FuncCallContext		*fctx;
	ReturnSetInfo		*rsi;
	AnyTable			 scan;
	HeapTuple			 tuple;
	TupleDesc			 in_tupdesc;
	TupleDesc			 out_tupdesc;
	Datum				 tup_datum;
	Datum				 values[1];
	bool				 nulls[1];
	int					 position;

	/*
	 * Sanity checking, shouldn't occur if our CREATE FUNCTION in SQL is done
	 * correctly.
	 */
	if (PG_NARGS() != 2 || PG_ARGISNULL(0) || PG_ARGISNULL(1))
		elog(ERROR, "invalid invocation of project");
	scan = PG_GETARG_ANYTABLE(0);  /* Should be the first parameter */
	position = PG_GETARG_INT32(1);

	/* Basic set-returning function (SRF) protocol, setup the context */
	if (SRF_IS_FIRSTCALL())
	{
		fctx = SRF_FIRSTCALL_INIT();
	}
	fctx = SRF_PERCALL_SETUP();

	/* Get the next value from the input scan */
	rsi			= (ReturnSetInfo *) fcinfo->resultinfo;
	out_tupdesc = rsi->expectedDesc;
	in_tupdesc  = AnyTable_GetTupleDesc(scan);
	tuple       = AnyTable_GetNextTuple(scan);

	/* Based on what the describe callback should have setup */
	if (position <= 0 || position > in_tupdesc->natts)
		ereport(ERROR, (errmsg("invalid position provided")));
	if (out_tupdesc->natts != 1)
		ereport(ERROR, (errmsg("only one expected tuple is allowed")));
	if (out_tupdesc->attrs[0]->atttypid != in_tupdesc->attrs[position-1]->atttypid)
		ereport(ERROR, (errmsg("input and output types do not match")));

	/* check for end of scan */
	if (tuple == NULL)
		SRF_RETURN_DONE(fctx);

	/* -----
	 * Extract fields from input tuple, there are several possibilities
	 * depending on if we want to fetch the rows by name, by number, or extract
	 * the full tuple contents.
	 *
	 *    - values[0] = GetAttributeByName(tuple->t_data, "a", &nulls[0]);
	 *    - values[0] = GetAttributeByNum(tuple->t_data, 0, &nulls[0]);
	 *    - heap_deform_tuple(tuple, in_tupdesc, values, nulls);
	 *
	 * In this case we have chosen to do extract by position
	 */
	values[0] = GetAttributeByNum(tuple->t_data, (AttrNumber) position, &nulls[0]);

	/* Construct the output tuple and convert to a datum */
	tuple = heap_form_tuple(out_tupdesc, values, nulls);
	tup_datum = HeapTupleGetDatum(tuple);

	/* Return the next result */
	SRF_RETURN_NEXT(fctx, tup_datum);
}

/*
 * A more dynamic describe function that produces different results depending
 * on what sort of input it receives.
 */
PG_FUNCTION_INFO_V1(project_describe);
Datum
project_describe(PG_FUNCTION_ARGS)
{
	FuncExpr			*fexpr;
	List				*fargs;
	ListCell			*lc;
	Oid					*argtypes;
	int					 numargs;
	TableValueExpr		*texpr;
	Query				*qexpr;
	TupleDesc			 tdesc;
	TupleDesc			 odesc;
	int					 avalue;
	bool				 isnull;
	int					 i;

	/* Fetch and validate input */
	if (PG_NARGS() != 1 || PG_ARGISNULL(0))
		elog(ERROR, "invalid invocation of project_describe");

	fexpr = (FuncExpr*) PG_GETARG_POINTER(0);
	if (!IsA(fexpr, FuncExpr))
		ereport(ERROR, (errmsg("invalid parameters for project_describe")));

	/*
	 * We should know the type information of the arguments of our calling
	 * function, but this demonstrates how we could determine that if we
	 * didn't already know.
	 */
	fargs = fexpr->args;
	numargs = list_length(fargs);
	argtypes = palloc(sizeof(Oid)*numargs);
	foreach_with_count(lc, fargs, i)
	{
		Node *arg = lfirst(lc);
		argtypes[i] = exprType(arg);
	}

	/* --------
	 * Given that we believe we know that this function is tied to exactly
	 * one implementation, lets verify that the above types are what we
	 * were expecting:
	 *   - two arguments
	 *   - first argument "anytable"
	 *   - second argument "text"
	 * --------
	 */
	if (numargs != 2)
		ereport(ERROR, (errmsg("invalid argument number"),
				errdetail("Two arguments need to be provided to the function")));
	if (argtypes[0] != ANYTABLEOID)
		ereport(ERROR, (errmsg("first argument is not a table OID")));
	if (argtypes[1] != INT4OID)
		ereport(ERROR, (errmsg("second argument is not a integer OID")));

	/* Now get the tuple descriptor for the ANYTABLE we received */
	texpr = (TableValueExpr*) linitial(fargs);
	if (!IsA(texpr, TableValueExpr))
		ereport(ERROR, (errmsg("function argument is not a table")));

	qexpr = (Query*) texpr->subquery;
	if (!IsA(qexpr, Query))
		ereport(ERROR, (errmsg("subquery is not a Query object")));

	tdesc = ExecCleanTypeFromTL(qexpr->targetList, false);

	/*
	 * The intent of this table function is that it returns the Nth column
	 * from the input, which requires us to know what N is.  We get N from
	 * the second parameter to the table function.
	 *
	 * Try to evaluate that argument to a constant value.
	 */
	avalue = DatumGetInt32(ExecEvalFunctionArgToConst(fexpr, 1, &isnull));
	if (isnull)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("unable to resolve type for function")));

	if (avalue < 1 || avalue > tdesc->natts)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid column position %d", avalue)));

	/* Build an output tuple a single column based on the column number above */
	odesc = CreateTemplateTupleDesc(1, false);
	TupleDescInitEntry(odesc, 1,
					   NameStr(tdesc->attrs[avalue-1]->attname),
					   tdesc->attrs[avalue-1]->atttypid,
					   tdesc->attrs[avalue-1]->atttypmod,
					   0);

	/* Finally return that tupdesc */
	PG_RETURN_POINTER(odesc);
}

PG_FUNCTION_INFO_V1(userdata_project);
Datum
userdata_project(PG_FUNCTION_ARGS)
{
	FuncCallContext		*fctx;
	ReturnSetInfo		*rsi;
	AnyTable			 scan;
	HeapTuple			 tuple;
	TupleDesc			 out_tupdesc;
	Datum				 tup_datum;
	Datum				 values[1];
	bool				 nulls[1];
	bytea				*userdata;
	char				*message;

	/*
	 * Sanity checking, shouldn't occur if our CREATE FUNCTION in SQL is done
	 * correctly.
	 */
	if (PG_NARGS() != 1 || PG_ARGISNULL(0) || PG_ARGISNULL(1))
		elog(ERROR, "invalid invocation of userdata_project");
	scan = PG_GETARG_ANYTABLE(0);  /* Should be the first parameter */
	if (SRF_IS_FIRSTCALL())
	{
		fctx = SRF_FIRSTCALL_INIT();
	}
	fctx = SRF_PERCALL_SETUP();

	/* Get the next value from the input scan */
	rsi			= (ReturnSetInfo *) fcinfo->resultinfo;
	out_tupdesc = rsi->expectedDesc;
	tuple       = AnyTable_GetNextTuple(scan);
	if (tuple == NULL)
		SRF_RETURN_DONE(fctx);

	/* Receive message from describe function */
	userdata = TF_GET_USERDATA();
	if (userdata != NULL)
	{
		message = (char *) VARDATA(userdata);
		values[0] = CStringGetTextDatum(message);
		nulls[0] = false;
	}
	else
	{
		values[0] = (Datum) 0;
		nulls[0] = true;
	}
	/* Construct the output tuple and convert to a datum */
	tuple = heap_form_tuple(out_tupdesc, values, nulls);
	tup_datum = HeapTupleGetDatum(tuple);

	/* Return the next result */
	SRF_RETURN_NEXT(fctx, tup_datum);
}

PG_FUNCTION_INFO_V1(userdata_describe);
Datum
userdata_describe(PG_FUNCTION_ARGS)
{
	FuncExpr	   *fexpr;
	TupleDesc		tupdesc;
	bytea		   *userdata;
	size_t			bytes;
	const char	   *message = "copied data from describe function";

	/* Fetch and validate input */
	if (PG_NARGS() != 1 || PG_ARGISNULL(0))
		elog(ERROR, "invalid invocation of userdata_describe");

	fexpr = (FuncExpr*) PG_GETARG_POINTER(0);
	if (!IsA(fexpr, FuncExpr))
		ereport(ERROR, (errmsg("invalid parameters for userdata_describe")));

	/* Build a result tuple descriptor */
	tupdesc = CreateTemplateTupleDesc(1, false);
	TupleDescInitEntry(tupdesc, (AttrNumber) 1, "message", TEXTOID, -1, 0);

	/* Prepare user data */
	bytes = VARHDRSZ + 256;
	userdata = (bytea *) palloc0(bytes);
	SET_VARSIZE(userdata, bytes);
	strcpy(VARDATA(userdata), message);

	/* Set to send */
	TF_SET_USERDATA(userdata);

	PG_RETURN_POINTER(tupdesc);
}

/*
 * Simply invoke CheckAuthTimeConstraints from libpq/auth.h with given rolname.
 */
PG_FUNCTION_INFO_V1(check_auth_time_constraints);
Datum
check_auth_time_constraints(PG_FUNCTION_ARGS)
{
	char		   *rolname = PG_GETARG_CSTRING(0);
	TimestampTz 	timestamp = PG_GETARG_TIMESTAMPTZ(1);

	PG_RETURN_BOOL(check_auth_time_constraints_internal(rolname, timestamp) == STATUS_OK);
}

/*
 * check if backends exist
 * Args:
 * timeout: = 0, return result immediately
 * timeout: > 0, block until no backends exist or timeout expired.
 */
PG_FUNCTION_INFO_V1(hasBackendsExist);
Datum
hasBackendsExist(PG_FUNCTION_ARGS)
{
	int beid;
	int32 result;
	int timeout = PG_GETARG_INT32(0);

	if (timeout < 0)
		elog(ERROR, "timeout is expected not to be negative");

	int pid = getpid();

	while (timeout >= 0)
	{
		result = 0;
		pgstat_clear_snapshot();
		int tot_backends = pgstat_fetch_stat_numbackends();
		for (beid = 1; beid <= tot_backends; beid++)
		{
			PgBackendStatus *beentry = pgstat_fetch_stat_beentry(beid);
			if (beentry && beentry->st_procpid >0 && beentry->st_procpid != pid &&
				beentry->st_session_id == gp_session_id)
				result++;
		}
		if (result == 0 || timeout == 0)
			break;
		sleep(1); /* 1 second */
		timeout--;
	}

	if (result > 0)
		PG_RETURN_BOOL(true);
	PG_RETURN_BOOL(false);
}


/*
 * check_primary_key () -- check that key in tuple being inserted/updated
 *			 references existing tuple in "primary" table.
 * Though it's called without args You have to specify referenced
 * table/keys while creating trigger:  key field names in triggered table,
 * referenced table name, referenced key field names:
 * EXECUTE PROCEDURE
 * check_primary_key ('Fkey1', 'Fkey2', 'Ptable', 'Pkey1', 'Pkey2').
 */
PG_FUNCTION_INFO_V1(check_primary_key);
Datum
check_primary_key(PG_FUNCTION_ARGS)
{
	TriggerData *trigdata = (TriggerData *) fcinfo->context;
	Trigger    *trigger;		/* to get trigger name */
	int			nargs;			/* # of args specified in CREATE TRIGGER */
	char	  **args;			/* arguments: column names and table name */
	int			nkeys;			/* # of key columns (= nargs / 2) */
	Datum	   *kvals;			/* key values */
	char	   *relname;		/* referenced relation name */
	Relation	rel;			/* triggered relation */
	HeapTuple	tuple = NULL;	/* tuple to return */
	TupleDesc	tupdesc;		/* tuple description */
	EPlan	   *plan;			/* prepared plan */
	Oid		   *argtypes = NULL;	/* key types to prepare execution plan */
	bool		isnull;			/* to know is some column NULL or not */
	char		ident[2 * NAMEDATALEN]; /* to identify myself */
	int			ret;
	int			i;

#ifdef	DEBUG_QUERY
	elog(DEBUG4, "check_primary_key: Enter Function");
#endif

	/*
	 * Some checks first...
	 */

	/* Called by trigger manager ? */
	if (!CALLED_AS_TRIGGER(fcinfo))
		/* internal error */
		elog(ERROR, "check_primary_key: not fired by trigger manager");

	/* Should be called for ROW trigger */
	if (TRIGGER_FIRED_FOR_STATEMENT(trigdata->tg_event))
		/* internal error */
		elog(ERROR, "check_primary_key: can't process STATEMENT events");

	/* If INSERTion then must check Tuple to being inserted */
	if (TRIGGER_FIRED_BY_INSERT(trigdata->tg_event))
		tuple = trigdata->tg_trigtuple;

	/* Not should be called for DELETE */
	else if (TRIGGER_FIRED_BY_DELETE(trigdata->tg_event))
		/* internal error */
		elog(ERROR, "check_primary_key: can't process DELETE events");

	/* If UPDATion the must check new Tuple, not old one */
	else
		tuple = trigdata->tg_newtuple;

	trigger = trigdata->tg_trigger;
	nargs = trigger->tgnargs;
	args = trigger->tgargs;

	if (nargs % 2 != 1)			/* odd number of arguments! */
		/* internal error */
		elog(ERROR, "check_primary_key: odd number of arguments should be specified");

	nkeys = nargs / 2;
	relname = args[nkeys];
	rel = trigdata->tg_relation;
	tupdesc = rel->rd_att;

	/* Connect to SPI manager */
	if ((ret = SPI_connect()) < 0)
		/* internal error */
		elog(ERROR, "check_primary_key: SPI_connect returned %d", ret);

	/*
	 * We use SPI plan preparation feature, so allocate space to place key
	 * values.
	 */
	kvals = (Datum *) palloc(nkeys * sizeof(Datum));

	/*
	 * Construct ident string as TriggerName $ TriggeredRelationId and try to
	 * find prepared execution plan.
	 */
	snprintf(ident, sizeof(ident), "%s$%u", trigger->tgname, rel->rd_id);
	plan = find_plan(ident, &PPlans, &nPPlans);

	/* if there is no plan then allocate argtypes for preparation */
	if (plan->nplans <= 0)
		argtypes = (Oid *) palloc(nkeys * sizeof(Oid));

	/* For each column in key ... */
	for (i = 0; i < nkeys; i++)
	{
		/* get index of column in tuple */
		int			fnumber = SPI_fnumber(tupdesc, args[i]);

		/* Bad guys may give us un-existing column in CREATE TRIGGER */
		if (fnumber < 0)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_COLUMN),
					 errmsg("there is no attribute \"%s\" in relation \"%s\"",
							args[i], SPI_getrelname(rel))));

		/* Well, get binary (in internal format) value of column */
		kvals[i] = SPI_getbinval(tuple, tupdesc, fnumber, &isnull);

		/*
		 * If it's NULL then nothing to do! DON'T FORGET call SPI_finish ()!
		 * DON'T FORGET return tuple! Executor inserts tuple you're returning!
		 * If you return NULL then nothing will be inserted!
		 */
		if (isnull)
		{
			SPI_finish();
			return PointerGetDatum(tuple);
		}

		if (plan->nplans <= 0)	/* Get typeId of column */
			argtypes[i] = SPI_gettypeid(tupdesc, fnumber);
	}

	/*
	 * If we have to prepare plan ...
	 */
	if (plan->nplans <= 0)
	{
		void	   *pplan;
		char		sql[8192];

		/*
		 * Construct query: SELECT 1 FROM _referenced_relation_ WHERE Pkey1 =
		 * $1 [AND Pkey2 = $2 [...]]
		 */
		snprintf(sql, sizeof(sql), "select 1 from %s where ", relname);
		for (i = 0; i < nkeys; i++)
		{
			snprintf(sql + strlen(sql), sizeof(sql) - strlen(sql), "%s = $%d %s",
				  args[i + nkeys + 1], i + 1, (i < nkeys - 1) ? "and " : "");
		}

		/* Prepare plan for query */
		pplan = SPI_prepare(sql, nkeys, argtypes);
		if (pplan == NULL)
			/* internal error */
			elog(ERROR, "check_primary_key: SPI_prepare returned %d", SPI_result);

		/*
		 * Remember that SPI_prepare places plan in current memory context -
		 * so, we have to save plan in Top memory context for latter use.
		 */
		pplan = SPI_saveplan(pplan);
		if (pplan == NULL)
			/* internal error */
			elog(ERROR, "check_primary_key: SPI_saveplan returned %d", SPI_result);
		plan->splan = (void **) malloc(sizeof(void *));
		*(plan->splan) = pplan;
		plan->nplans = 1;
	}

	/*
	 * Ok, execute prepared plan.
	 */
	ret = SPI_execp(*(plan->splan), kvals, NULL, 1);
	/* we have no NULLs - so we pass   ^^^^   here */

	if (ret < 0)
		/* internal error */
		elog(ERROR, "check_primary_key: SPI_execp returned %d", ret);

	/*
	 * If there are no tuples returned by SELECT then ...
	 */
	if (SPI_processed == 0)
		ereport(ERROR,
				(errcode(ERRCODE_TRIGGERED_ACTION_EXCEPTION),
				 errmsg("tuple references non-existent key"),
				 errdetail("Trigger \"%s\" found tuple referencing non-existent key in \"%s\".", trigger->tgname, relname)));

	SPI_finish();

	return PointerGetDatum(tuple);
}

/*
 * check_foreign_key () -- check that key in tuple being deleted/updated
 *			 is not referenced by tuples in "foreign" table(s).
 * Though it's called without args You have to specify (while creating trigger):
 * number of references, action to do if key referenced
 * ('restrict' | 'setnull' | 'cascade'), key field names in triggered
 * ("primary") table and referencing table(s)/keys:
 * EXECUTE PROCEDURE
 * check_foreign_key (2, 'restrict', 'Pkey1', 'Pkey2',
 * 'Ftable1', 'Fkey11', 'Fkey12', 'Ftable2', 'Fkey21', 'Fkey22').
 */
PG_FUNCTION_INFO_V1(check_foreign_key);
Datum
check_foreign_key(PG_FUNCTION_ARGS)
{
	TriggerData *trigdata = (TriggerData *) fcinfo->context;
	Trigger    *trigger;		/* to get trigger name */
	int			nargs;			/* # of args specified in CREATE TRIGGER */
	char	  **args;			/* arguments: as described above */
	char	  **args_temp;
	int			nrefs;			/* number of references (== # of plans) */
	char		action;			/* 'R'estrict | 'S'etnull | 'C'ascade */
	int			nkeys;			/* # of key columns */
	Datum	   *kvals;			/* key values */
	char	   *relname;		/* referencing relation name */
	Relation	rel;			/* triggered relation */
	HeapTuple	trigtuple = NULL;		/* tuple to being changed */
	HeapTuple	newtuple = NULL;	/* tuple to return */
	TupleDesc	tupdesc;		/* tuple description */
	EPlan	   *plan;			/* prepared plan(s) */
	Oid		   *argtypes = NULL;	/* key types to prepare execution plan */
	bool		isnull;			/* to know is some column NULL or not */
	bool		isequal = true; /* are keys in both tuples equal (in UPDATE) */
	char		ident[2 * NAMEDATALEN]; /* to identify myself */
	int			is_update = 0;
	int			ret;
	int			i,
				r;

#ifdef DEBUG_QUERY
	elog(DEBUG4, "check_foreign_key: Enter Function");
#endif

	/*
	 * Some checks first...
	 */

	/* Called by trigger manager ? */
	if (!CALLED_AS_TRIGGER(fcinfo))
		/* internal error */
		elog(ERROR, "check_foreign_key: not fired by trigger manager");

	/* Should be called for ROW trigger */
	if (TRIGGER_FIRED_FOR_STATEMENT(trigdata->tg_event))
		/* internal error */
		elog(ERROR, "check_foreign_key: can't process STATEMENT events");

	/* Not should be called for INSERT */
	if (TRIGGER_FIRED_BY_INSERT(trigdata->tg_event))
		/* internal error */
		elog(ERROR, "check_foreign_key: can't process INSERT events");

	/* Have to check tg_trigtuple - tuple being deleted */
	trigtuple = trigdata->tg_trigtuple;

	/*
	 * But if this is UPDATE then we have to return tg_newtuple. Also, if key
	 * in tg_newtuple is the same as in tg_trigtuple then nothing to do.
	 */
	is_update = 0;
	if (TRIGGER_FIRED_BY_UPDATE(trigdata->tg_event))
	{
		newtuple = trigdata->tg_newtuple;
		is_update = 1;
	}
	trigger = trigdata->tg_trigger;
	nargs = trigger->tgnargs;
	args = trigger->tgargs;

	if (nargs < 5)				/* nrefs, action, key, Relation, key - at
								 * least */
		/* internal error */
		elog(ERROR, "check_foreign_key: too short %d (< 5) list of arguments", nargs);

	nrefs = pg_atoi(args[0], sizeof(int), 0);
	if (nrefs < 1)
		/* internal error */
		elog(ERROR, "check_foreign_key: %d (< 1) number of references specified", nrefs);
	action = tolower((unsigned char) *(args[1]));
	if (action != 'r' && action != 'c' && action != 's')
		/* internal error */
		elog(ERROR, "check_foreign_key: invalid action %s", args[1]);
	nargs -= 2;
	args += 2;
	nkeys = (nargs - nrefs) / (nrefs + 1);
	if (nkeys <= 0 || nargs != (nrefs + nkeys * (nrefs + 1)))
		/* internal error */
		elog(ERROR, "check_foreign_key: invalid number of arguments %d for %d references",
			 nargs + 2, nrefs);

	rel = trigdata->tg_relation;
	tupdesc = rel->rd_att;

	/* Connect to SPI manager */
	if ((ret = SPI_connect()) < 0)
		/* internal error */
		elog(ERROR, "check_foreign_key: SPI_connect returned %d", ret);

	/*
	 * We use SPI plan preparation feature, so allocate space to place key
	 * values.
	 */
	kvals = (Datum *) palloc(nkeys * sizeof(Datum));

	/*
	 * Construct ident string as TriggerName $ TriggeredRelationId and try to
	 * find prepared execution plan(s).
	 */
	snprintf(ident, sizeof(ident), "%s$%u", trigger->tgname, rel->rd_id);
	plan = find_plan(ident, &FPlans, &nFPlans);

	/* if there is no plan(s) then allocate argtypes for preparation */
	if (plan->nplans <= 0)
		argtypes = (Oid *) palloc(nkeys * sizeof(Oid));

	/*
	 * else - check that we have exactly nrefs plan(s) ready
	 */
	else if (plan->nplans != nrefs)
		/* internal error */
		elog(ERROR, "%s: check_foreign_key: # of plans changed in meantime",
			 trigger->tgname);

	/* For each column in key ... */
	for (i = 0; i < nkeys; i++)
	{
		/* get index of column in tuple */
		int			fnumber = SPI_fnumber(tupdesc, args[i]);

		/* Bad guys may give us un-existing column in CREATE TRIGGER */
		if (fnumber < 0)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_COLUMN),
					 errmsg("there is no attribute \"%s\" in relation \"%s\"",
							args[i], SPI_getrelname(rel))));

		/* Well, get binary (in internal format) value of column */
		kvals[i] = SPI_getbinval(trigtuple, tupdesc, fnumber, &isnull);

		/*
		 * If it's NULL then nothing to do! DON'T FORGET call SPI_finish ()!
		 * DON'T FORGET return tuple! Executor inserts tuple you're returning!
		 * If you return NULL then nothing will be inserted!
		 */
		if (isnull)
		{
			SPI_finish();
			return PointerGetDatum((newtuple == NULL) ? trigtuple : newtuple);
		}

		/*
		 * If UPDATE then get column value from new tuple being inserted and
		 * compare is this the same as old one. For the moment we use string
		 * presentation of values...
		 */
		if (newtuple != NULL)
		{
			char	   *oldval = SPI_getvalue(trigtuple, tupdesc, fnumber);
			char	   *newval;

			/* this shouldn't happen! SPI_ERROR_NOOUTFUNC ? */
			if (oldval == NULL)
				/* internal error */
				elog(ERROR, "check_foreign_key: SPI_getvalue returned %d", SPI_result);
			newval = SPI_getvalue(newtuple, tupdesc, fnumber);
			if (newval == NULL || strcmp(oldval, newval) != 0)
				isequal = false;
		}

		if (plan->nplans <= 0)	/* Get typeId of column */
			argtypes[i] = SPI_gettypeid(tupdesc, fnumber);
	}
	args_temp = args;
	nargs -= nkeys;
	args += nkeys;

	/*
	 * If we have to prepare plans ...
	 */
	if (plan->nplans <= 0)
	{
		void	   *pplan;
		char		sql[8192];
		char	  **args2 = args;

		plan->splan = (void **) malloc(nrefs * sizeof(void *));

		for (r = 0; r < nrefs; r++)
		{
			relname = args2[0];

			/*---------
			 * For 'R'estrict action we construct SELECT query:
			 *
			 *	SELECT 1
			 *	FROM _referencing_relation_
			 *	WHERE Fkey1 = $1 [AND Fkey2 = $2 [...]]
			 *
			 *	to check is tuple referenced or not.
			 *---------
			 */
			if (action == 'r')

				snprintf(sql, sizeof(sql), "select 1 from %s where ", relname);

			/*---------
			 * For 'C'ascade action we construct DELETE query
			 *
			 *	DELETE
			 *	FROM _referencing_relation_
			 *	WHERE Fkey1 = $1 [AND Fkey2 = $2 [...]]
			 *
			 * to delete all referencing tuples.
			 *---------
			 */

			/*
			 * Max : Cascade with UPDATE query i create update query that
			 * updates new key values in referenced tables
			 */


			else if (action == 'c')
			{
				if (is_update == 1)
				{
					int			fn;
					char	   *nv;
					int			k;

					snprintf(sql, sizeof(sql), "update %s set ", relname);
					for (k = 1; k <= nkeys; k++)
					{
						int			is_char_type = 0;
						char	   *type;

						fn = SPI_fnumber(tupdesc, args_temp[k - 1]);
						nv = SPI_getvalue(newtuple, tupdesc, fn);
						type = SPI_gettype(tupdesc, fn);

						if ((strcmp(type, "text") && strcmp(type, "varchar") &&
							 strcmp(type, "char") && strcmp(type, "bpchar") &&
							 strcmp(type, "date") && strcmp(type, "timestamp")) == 0)
							is_char_type = 1;
#ifdef	DEBUG_QUERY
						elog(DEBUG4, "check_foreign_key Debug value %s type %s %d",
							 nv, type, is_char_type);
#endif

						/*
						 * is_char_type =1 i set ' ' for define a new value
						 */
						snprintf(sql + strlen(sql), sizeof(sql) - strlen(sql),
								 " %s = %s%s%s %s ",
								 args2[k], (is_char_type > 0) ? "'" : "",
								 nv, (is_char_type > 0) ? "'" : "", (k < nkeys) ? ", " : "");
						is_char_type = 0;
					}
					strcat(sql, " where ");

				}
				else
					/* DELETE */
					snprintf(sql, sizeof(sql), "delete from %s where ", relname);

			}

			/*
			 * For 'S'etnull action we construct UPDATE query - UPDATE
			 * _referencing_relation_ SET Fkey1 null [, Fkey2 null [...]]
			 * WHERE Fkey1 = $1 [AND Fkey2 = $2 [...]] - to set key columns in
			 * all referencing tuples to NULL.
			 */
			else if (action == 's')
			{
				snprintf(sql, sizeof(sql), "update %s set ", relname);
				for (i = 1; i <= nkeys; i++)
				{
					snprintf(sql + strlen(sql), sizeof(sql) - strlen(sql),
							 "%s = null%s",
							 args2[i], (i < nkeys) ? ", " : "");
				}
				strcat(sql, " where ");
			}

			/* Construct WHERE qual */
			for (i = 1; i <= nkeys; i++)
			{
				snprintf(sql + strlen(sql), sizeof(sql) - strlen(sql), "%s = $%d %s",
						 args2[i], i, (i < nkeys) ? "and " : "");
			}

			/* Prepare plan for query */
			pplan = SPI_prepare(sql, nkeys, argtypes);
			if (pplan == NULL)
				/* internal error */
				elog(ERROR, "check_foreign_key: SPI_prepare returned %d", SPI_result);

			/*
			 * Remember that SPI_prepare places plan in current memory context
			 * - so, we have to save plan in Top memory context for latter
			 * use.
			 */
			pplan = SPI_saveplan(pplan);
			if (pplan == NULL)
				/* internal error */
				elog(ERROR, "check_foreign_key: SPI_saveplan returned %d", SPI_result);

			plan->splan[r] = pplan;

			args2 += nkeys + 1; /* to the next relation */
		}
		plan->nplans = nrefs;
#ifdef	DEBUG_QUERY
		elog(DEBUG4, "check_foreign_key Debug Query is :  %s ", sql);
#endif
	}

	/*
	 * If UPDATE and key is not changed ...
	 */
	if (newtuple != NULL && isequal)
	{
		SPI_finish();
		return PointerGetDatum(newtuple);
	}

	/*
	 * Ok, execute prepared plan(s).
	 */
	for (r = 0; r < nrefs; r++)
	{
		/*
		 * For 'R'estrict we may to execute plan for one tuple only, for other
		 * actions - for all tuples.
		 */
		int			tcount = (action == 'r') ? 1 : 0;

		relname = args[0];

		snprintf(ident, sizeof(ident), "%s$%u", trigger->tgname, rel->rd_id);
		plan = find_plan(ident, &FPlans, &nFPlans);
		ret = SPI_execp(plan->splan[r], kvals, NULL, tcount);
		/* we have no NULLs - so we pass   ^^^^  here */

		if (ret < 0)
			ereport(ERROR,
					(errcode(ERRCODE_TRIGGERED_ACTION_EXCEPTION),
					 errmsg("SPI_execp returned %d", ret)));

		/* If action is 'R'estrict ... */
		if (action == 'r')
		{
			/* If there is tuple returned by SELECT then ... */
			if (SPI_processed > 0)
				ereport(ERROR,
						(errcode(ERRCODE_TRIGGERED_ACTION_EXCEPTION),
						 errmsg("\"%s\": tuple is referenced in \"%s\"",
								trigger->tgname, relname)));
		}
		else
		{
#ifdef REFINT_VERBOSE
			elog(NOTICE, "%s: %d tuple(s) of %s are %s",
				 trigger->tgname, SPI_processed, relname,
				 (action == 'c') ? "deleted" : "set to null");
#endif
		}
		args += nkeys + 1;		/* to the next relation */
	}

	SPI_finish();

	return PointerGetDatum((newtuple == NULL) ? trigtuple : newtuple);
}

PG_FUNCTION_INFO_V1(autoinc);
Datum
autoinc(PG_FUNCTION_ARGS)
{
	TriggerData *trigdata = (TriggerData *) fcinfo->context;
	Trigger    *trigger;		/* to get trigger name */
	int			nargs;			/* # of arguments */
	int		   *chattrs;		/* attnums of attributes to change */
	int			chnattrs = 0;	/* # of above */
	Datum	   *newvals;		/* vals of above */
	char	  **args;			/* arguments */
	char	   *relname;		/* triggered relation name */
	Relation	rel;			/* triggered relation */
	HeapTuple	rettuple = NULL;
	TupleDesc	tupdesc;		/* tuple description */
	bool		isnull;
	int			i;

	if (!CALLED_AS_TRIGGER(fcinfo))
		/* internal error */
		elog(ERROR, "not fired by trigger manager");
	if (TRIGGER_FIRED_FOR_STATEMENT(trigdata->tg_event))
		/* internal error */
		elog(ERROR, "can't process STATEMENT events");
	if (TRIGGER_FIRED_AFTER(trigdata->tg_event))
		/* internal error */
		elog(ERROR, "must be fired before event");

	if (TRIGGER_FIRED_BY_INSERT(trigdata->tg_event))
		rettuple = trigdata->tg_trigtuple;
	else if (TRIGGER_FIRED_BY_UPDATE(trigdata->tg_event))
		rettuple = trigdata->tg_newtuple;
	else
		/* internal error */
		elog(ERROR, "can't process DELETE events");

	rel = trigdata->tg_relation;
	relname = SPI_getrelname(rel);

	trigger = trigdata->tg_trigger;

	nargs = trigger->tgnargs;
	if (nargs <= 0 || nargs % 2 != 0)
		/* internal error */
		elog(ERROR, "autoinc (%s): even number gt 0 of arguments was expected", relname);

	args = trigger->tgargs;
	tupdesc = rel->rd_att;

	chattrs = (int *) palloc(nargs / 2 * sizeof(int));
	newvals = (Datum *) palloc(nargs / 2 * sizeof(Datum));

	for (i = 0; i < nargs;)
	{
		int			attnum = SPI_fnumber(tupdesc, args[i]);
		int32		val;
		Datum		seqname;

		if (attnum < 0)
			ereport(ERROR,
					(errcode(ERRCODE_TRIGGERED_ACTION_EXCEPTION),
					 errmsg("\"%s\" has no attribute \"%s\"",
							relname, args[i])));

		if (SPI_gettypeid(tupdesc, attnum) != INT4OID)
			ereport(ERROR,
					(errcode(ERRCODE_TRIGGERED_ACTION_EXCEPTION),
					 errmsg("attribute \"%s\" of \"%s\" must be type INT4",
							args[i], relname)));

		val = DatumGetInt32(SPI_getbinval(rettuple, tupdesc, attnum, &isnull));

		if (!isnull && val != 0)
		{
			i += 2;
			continue;
		}

		i++;
		chattrs[chnattrs] = attnum;
		seqname = DirectFunctionCall1(textin,
									  CStringGetDatum(args[i]));
		newvals[chnattrs] = DirectFunctionCall1(nextval, seqname);
		/* nextval now returns int64; coerce down to int32 */
		newvals[chnattrs] = Int32GetDatum((int32) DatumGetInt64(newvals[chnattrs]));
		if (DatumGetInt32(newvals[chnattrs]) == 0)
		{
			newvals[chnattrs] = DirectFunctionCall1(nextval, seqname);
			newvals[chnattrs] = Int32GetDatum((int32) DatumGetInt64(newvals[chnattrs]));
		}
		pfree(DatumGetTextP(seqname));
		chnattrs++;
		i++;
	}

	if (chnattrs > 0)
	{
		rettuple = SPI_modifytuple(rel, rettuple, chnattrs, chattrs, newvals, NULL);
		if (rettuple == NULL)
			/* internal error */
			elog(ERROR, "autoinc (%s): %d returned by SPI_modifytuple",
				 relname, SPI_result);
	}

	pfree(relname);
	pfree(chattrs);
	pfree(newvals);

	return PointerGetDatum(rettuple);
}

static EPlan *
find_plan(char *ident, EPlan ** eplan, int *nplans)
{
	EPlan	   *newp;
	int			i;

	if (*nplans > 0)
	{
		for (i = 0; i < *nplans; i++)
		{
			if (strcmp((*eplan)[i].ident, ident) == 0)
				break;
		}
		if (i != *nplans)
			return (*eplan + i);
		*eplan = (EPlan *) realloc(*eplan, (i + 1) * sizeof(EPlan));
		newp = *eplan + i;
	}
	else
	{
		newp = *eplan = (EPlan *) malloc(sizeof(EPlan));
		(*nplans) = i = 0;
	}

	newp->ident = (char *) malloc(strlen(ident) + 1);
	strcpy(newp->ident, ident);
	newp->nplans = 0;
	newp->splan = NULL;
	(*nplans)++;

	return (newp);
}


/*
 * trigger_udf_return_new_oid
 *
 * A helper function to assign a specific OID to a tuple on INSERT.
 */
PG_FUNCTION_INFO_V1(trigger_udf_return_new_oid);
Datum
trigger_udf_return_new_oid(PG_FUNCTION_ARGS)
{
	TriggerData *trigdata = (TriggerData *) fcinfo->context;
	Trigger    *trigger;
	char	  **args;
	HeapTuple	input_tuple;
	HeapTuple	ret_tuple;
	Oid			new_oid;

	if (!CALLED_AS_TRIGGER(fcinfo))
		elog(ERROR, "not fired by trigger manager");
	if (!TRIGGER_FIRED_FOR_ROW(trigdata->tg_event))
		elog(ERROR, "cannot process STATEMENT events");
	if (!TRIGGER_FIRED_BEFORE(trigdata->tg_event))
		elog(ERROR, "must be fired before event");

	if (!TRIGGER_FIRED_BY_INSERT(trigdata->tg_event))
		elog(ERROR, "trigger_udf_return_new_oid() called for a non-INSERT");

	/*
	 * Get the argument. (Trigger functions receive their arguments
	 * differently than normal functions.)
	 */
	trigger = trigdata->tg_trigger;
	if (trigger->tgnargs != 1)
		elog(ERROR, "trigger_udf_return_new_oid called with invalid number of arguments (%d, expected 1)",
			 trigger->tgnargs);
	args = trigger->tgargs;
	new_oid = DatumGetObjectId(DirectFunctionCall1(oidin,
												   CStringGetDatum(args[0])));

	elog(NOTICE, "trigger_udf_return_new_oid assigned OID %u to the new tuple", new_oid);

	input_tuple = trigdata->tg_trigtuple;
	ret_tuple = heap_copytuple(input_tuple);
	HeapTupleSetOid(ret_tuple, new_oid);

	return PointerGetDatum(ret_tuple);
}


/*
 * test_consume_xids(int4), for rapidly consuming XIDs, to test wraparound.
 *
 * Used by the 'autovacuum-template0' test.
 */
PG_FUNCTION_INFO_V1(test_consume_xids);
Datum
test_consume_xids(PG_FUNCTION_ARGS)
{
	int32		nxids = PG_GETARG_INT32(0);
	TransactionId topxid;
	TransactionId xid;
	TransactionId targetxid;

	/* make sure we have a top-XID first */
	topxid = GetCurrentTransactionId();

	xid = ReadNewTransactionId();

	targetxid = xid + nxids;
	while (targetxid < FirstNormalTransactionId)
		targetxid++;

	while (TransactionIdPrecedes(xid, targetxid))
	{
		elog(DEBUG1, "xid: %u", xid);
		xid = GetNewTransactionId(true);
	}

	PG_RETURN_VOID();
}

/*
 * Function to execute a DML/DDL command on segment with specified content id.
 * To use:
 *
 * CREATE FUNCTION gp_execute_on_server(content int, query text) returns text
 * language C as '$libdir/regress.so', 'gp_execute_on_server';
 */
PG_FUNCTION_INFO_V1(gp_execute_on_server);
Datum
gp_execute_on_server(PG_FUNCTION_ARGS)
{
	int32		content = PG_GETARG_INT32(0);
	char	   *query = TextDatumGetCString(PG_GETARG_TEXT_PP(1));
	CdbPgResults cdb_pgresults;
	StringInfoData result_str;

	if (!IS_QUERY_DISPATCHER())
		elog(ERROR, "cannot use gp_execute_on_server() when not in QD mode");

	CdbDispatchCommandToSegments(query,
								 DF_CANCEL_ON_ERROR | DF_WITH_SNAPSHOT,
								 list_make1_int(content),
								 &cdb_pgresults);

	/*
	 * Collect the results.
	 *
	 * All the result fields are appended to a string, with minimal
	 * formatting. That's not very pretty, but is good enough for
	 * regression tests.
	 */
	initStringInfo(&result_str);
	for (int resultno = 0; resultno < cdb_pgresults.numResults; resultno++)
	{
		struct pg_result *pgresult = cdb_pgresults.pg_results[resultno];

		if (PQresultStatus(pgresult) != PGRES_TUPLES_OK &&
			PQresultStatus(pgresult) != PGRES_COMMAND_OK)
		{
			cdbdisp_clearCdbPgResults(&cdb_pgresults);
			elog(ERROR, "execution failed with status %d", PQresultStatus(pgresult));
		}

		for (int rowno = 0; rowno < PQntuples(pgresult); rowno++)
		{
			if (rowno > 0)
				appendStringInfoString(&result_str, "\n");
			for (int colno = 0; colno < PQnfields(pgresult); colno++)
			{
				if (colno > 0)
					appendStringInfoString(&result_str, " ");
				appendStringInfoString(&result_str, PQgetvalue(pgresult, rowno, colno));
			}
		}
	}

	cdbdisp_clearCdbPgResults(&cdb_pgresults);
	PG_RETURN_TEXT_P(CStringGetTextDatum(result_str.data));
}

/*
 * Check if the shared buffer cache contains any pages that have the specified
 * database OID in their buffer tag. Return true if an entry is found, else
 * return false.
 */
PG_FUNCTION_INFO_V1(check_shared_buffer_cache_for_dboid);
Datum
check_shared_buffer_cache_for_dboid(PG_FUNCTION_ARGS)
{
	Oid databaseOid = PG_GETARG_OID(0);
	int i;

	for (i = 0; i < NBuffers; i++)
	{
		volatile BufferDesc *bufHdr = &BufferDescriptors[i];

		if (bufHdr->tag.rnode.dbNode == databaseOid)
			PG_RETURN_BOOL(true);
	}

	PG_RETURN_BOOL(false);
}

PG_FUNCTION_INFO_V1(gp_set_next_oid);
Datum
gp_set_next_oid(PG_FUNCTION_ARGS)
{
	Oid new_oid = PG_GETARG_OID(0);

	LWLockAcquire(OidGenLock, LW_EXCLUSIVE);

	ShmemVariableCache->nextOid = new_oid;

	LWLockRelease(OidGenLock);

	PG_RETURN_VOID();
}

PG_FUNCTION_INFO_V1(gp_get_next_oid);
Datum
gp_get_next_oid(PG_FUNCTION_ARGS)
{
	PG_RETURN_OID(ShmemVariableCache->nextOid);
}

/*
 * This is like int4out, but throws an error on '1234'.
 *
 * Used in the error handling test in 'gpcopy'.
 */
PG_FUNCTION_INFO_V1(broken_int4out);
Datum
broken_int4out(PG_FUNCTION_ARGS)
{
	int32		arg = PG_GETARG_INT32(0);

	if (arg == 1234)
		ereport(ERROR,
				(errcode(ERRCODE_FAULT_INJECT),
				 errmsg("testing failure in output function"),
				 errdetail("The trigger value was 1234")));

	return DirectFunctionCall1(int4out, Int32GetDatum(arg));
}

PG_FUNCTION_INFO_V1(get_tablespace_version_directory_name);
Datum
get_tablespace_version_directory_name(PG_FUNCTION_ARGS)
{
	PG_RETURN_TEXT_P(CStringGetTextDatum(GP_TABLESPACE_VERSION_DIRECTORY));
}
