/*-------------------------------------------------------------------------
 *
 * resgroup.c
 *	  Greenplum internals code for resource groups.
 *
 *
 * Copyright (c) 2006-2017, Greenplum inc.
 *
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <time.h>

#include "pgstat.h"
#include "access/heapam.h"
#include "access/twophase.h"
#include "access/twophase_rmgr.h"
#include "access/xact.h"
#include "catalog/pg_type.h"
#include "cdb/cdbgang.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/lock.h"
#include "storage/lmgr.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/portal.h"
#include "utils/ps_status.h"
#include "utils/resowner.h"
#include "utils/resscheduler.h"
#include "cdb/memquota.h"

Datum
pg_resgroup_get_status_kv(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;

	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldcontext;
		TupleDesc	tupdesc;
		int			nattr = 3;

		funcctx = SRF_FIRSTCALL_INIT();

		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		tupdesc = CreateTemplateTupleDesc(nattr, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "rsgid", OIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "prop", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "value", TEXTOID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		if (PG_ARGISNULL(0))
		{
			funcctx->max_calls = 0;
		}
		else
		{
			/* dummy output */
			funcctx->max_calls = 2;
			funcctx->user_fctx = palloc0(sizeof(Datum) * funcctx->max_calls);
			((Datum *) funcctx->user_fctx)[0] = ObjectIdGetDatum(6437);
			((Datum *) funcctx->user_fctx)[1] = ObjectIdGetDatum(6438);
		}

		MemoryContextSwitchTo(oldcontext);
	}

	/* stuff done on every call of the function */
	funcctx = SRF_PERCALL_SETUP();

	if (funcctx->call_cntr < funcctx->max_calls)
	{
		/* for each row */
		char *		prop = text_to_cstring(PG_GETARG_TEXT_P(0));
		Datum		values[3];
		bool		nulls[3];
		HeapTuple	tuple;

		MemSet(values, 0, sizeof(values));
		MemSet(nulls, 0, sizeof(nulls));

		values[0] = ((Datum *) funcctx->user_fctx)[funcctx->call_cntr];
		values[1] = CStringGetTextDatum(prop);

		/* Fill with dummy values */
		if (!strcmp(prop, "num_running"))
			values[2] = CStringGetTextDatum("1");
		else if (!strcmp(prop, "num_queueing"))
			values[2] = CStringGetTextDatum("0");
		else if (!strcmp(prop, "cpu_usage"))
			values[2] = CStringGetTextDatum("0.0");
		else if (!strcmp(prop, "memory_usage"))
			values[2] = CStringGetTextDatum("0.0");
		else if (!strcmp(prop, "total_queue_duration"))
			values[2] = CStringGetTextDatum("00:00:00");
		else if (!strcmp(prop, "num_queued"))
			values[2] = CStringGetTextDatum("0");
		else if (!strcmp(prop, "num_executed"))
			values[2] = CStringGetTextDatum("0");
		else
			/* unknown property name */
			nulls[2] = true;

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);

		SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
	}
	else
	{
		/* nothing left */
		SRF_RETURN_DONE(funcctx);
	}
}

