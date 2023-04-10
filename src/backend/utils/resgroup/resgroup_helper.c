/*-------------------------------------------------------------------------
 *
 * Gp_resgroup_helper.c
 *	  Helper functions for resource group.
 *
 * Copyright (c) 2017-Present VMware, Inc. or its affiliates.
 *
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/table.h"
#include "funcapi.h"
#include "libpq-fe.h"
#include "miscadmin.h"
#include "catalog/pg_resgroup.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbvars.h"
#include "commands/resgroupcmds.h"
#include "storage/procarray.h"
#include "utils/builtins.h"
#include "utils/datetime.h"
#include "utils/resgroup.h"
#include "utils/cgroup.h"
#include "utils/resource_manager.h"

typedef struct ResGroupStat
{
	Datum groupId;

	StringInfo cpuUsage;
} ResGroupStat;

typedef struct ResGroupStatCtx
{
	int nGroups;
	ResGroupStat groups[1];
} ResGroupStatCtx;

static void calcCpuUsage(StringInfoData *str,
						 int64 usageBegin, TimestampTz timestampBegin,
						 int64 usageEnd, TimestampTz timestampEnd);
static void getResUsage(ResGroupStatCtx *ctx, Oid inGroupId);
static void dumpResGroupInfo(StringInfo str);

static void
calcCpuUsage(StringInfoData *str,
			 int64 usageBegin, TimestampTz timestampBegin,
			 int64 usageEnd, TimestampTz timestampEnd)
{
	int64 duration;
	long secs;
	int usecs;
	int64 usage;

	usage = usageEnd - usageBegin;

	TimestampDifference(timestampBegin, timestampEnd, &secs, &usecs);

	duration = secs * 1000000 + usecs;

	appendStringInfo(str, "\"%d\":%.2f",
					 GpIdentity.segindex,
					 cgroupOpsRoutine->convertcpuusage(usage, duration));
}

/*
 * Get resource usage.
 *
 * On QD this function dispatch the request to all QEs, collecting both
 * QEs' and QD's resource usage.
 *
 * On QE this function only collect the resource usage on itself.
 *
 * Memory & cpu usage are returned in JSON format.
 */
static void
getResUsage(ResGroupStatCtx *ctx, Oid inGroupId)
{
	int64 *usages;
	TimestampTz *timestamps;
	int i, j;

	usages = palloc(sizeof(*usages) * ctx->nGroups);
	timestamps = palloc(sizeof(*timestamps) * ctx->nGroups);

	for (j = 0; j < ctx->nGroups; j++)
	{
		ResGroupStat *row = &ctx->groups[j];
		Oid groupId = DatumGetObjectId(row->groupId);

		usages[j] = cgroupOpsRoutine->getcpuusage(groupId);
		timestamps[j] = GetCurrentTimestamp();
	}

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		CdbPgResults cdb_pgresults = {NULL, 0};
		StringInfoData buffer;

		initStringInfo(&buffer);
		appendStringInfo(&buffer,
						 "SELECT groupid, cpu_usage "
						 "FROM pg_resgroup_get_status(%u)",
						 inGroupId);

		CdbDispatchCommand(buffer.data, DF_WITH_SNAPSHOT, &cdb_pgresults);

		if (cdb_pgresults.numResults == 0)
			elog(ERROR, "pg_resgroup_get_status() didn't get back any resource statistic from the segDBs");

		for (i = 0; i < cdb_pgresults.numResults; i++)
		{
			struct pg_result *pg_result = cdb_pgresults.pg_results[i];

			/*
			 * Any error here should have propagated into errbuf, so we shouldn't
			 * ever see anything other that tuples_ok here.  But, check to be
			 * sure.
			 */
			if (PQresultStatus(pg_result) != PGRES_TUPLES_OK)
			{
				cdbdisp_clearCdbPgResults(&cdb_pgresults);
				elog(ERROR, "pg_resgroup_get_status(): resultStatus not tuples_Ok");
			}

			Assert(PQntuples(pg_result) == ctx->nGroups);
			for (j = 0; j < ctx->nGroups; j++)
			{
				const char *result;
				ResGroupStat *row = &ctx->groups[j];
				Oid groupId = atooid(PQgetvalue(pg_result, j, 0));

				Assert(groupId == row->groupId);

				if (row->cpuUsage->len == 0)
				{
					appendStringInfo(row->cpuUsage, "{");
					calcCpuUsage(row->cpuUsage, usages[j], timestamps[j],
								 cgroupOpsRoutine->getcpuusage(groupId),
								 GetCurrentTimestamp());
				}

				result = PQgetvalue(pg_result, j, 1);
				appendStringInfo(row->cpuUsage, ", %s", result);

				if (i == cdb_pgresults.numResults - 1)
					appendStringInfoChar(row->cpuUsage, '}');
			}
		}

		cdbdisp_clearCdbPgResults(&cdb_pgresults);
	}
	else
	{
		pg_usleep(300000);

		for (j = 0; j < ctx->nGroups; j++)
		{
			ResGroupStat *row = &ctx->groups[j];
			Oid groupId = DatumGetObjectId(row->groupId);

			calcCpuUsage(row->cpuUsage, usages[j], timestamps[j],
						 cgroupOpsRoutine->getcpuusage(groupId),
						 GetCurrentTimestamp());
		}
	}
}

static int
compareRow(const void *ptr1, const void *ptr2)
{
	const ResGroupStat *row1 = (const ResGroupStat *) ptr1;
	const ResGroupStat *row2 = (const ResGroupStat *) ptr2;

	return row1->groupId - row2->groupId;
}

/*
 * Get status of resource groups
 */
Datum
pg_resgroup_get_status(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	ResGroupStatCtx *ctx;

	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldcontext;
		TupleDesc	tupdesc;
		int			nattr = 7;

		funcctx = SRF_FIRSTCALL_INIT();

		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		tupdesc = CreateTemplateTupleDesc(nattr);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "groupid", OIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "num_running", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "num_queueing", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "num_queued", INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "num_executed", INT8OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 6, "total_queue_duration", INTERVALOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 7, "cpu_usage", JSONOID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		if (IsResGroupActivated())
		{
			Relation	pg_resgroup_rel;
			SysScanDesc	sscan;
			HeapTuple	tuple;
			Oid			inGroupId = PG_GETARG_OID(0);

			int ctxsize = sizeof(ResGroupStatCtx) +
				sizeof(ResGroupStat) * (MaxResourceGroups - 1);

			funcctx->user_fctx = palloc(ctxsize);
			ctx = (ResGroupStatCtx *) funcctx->user_fctx;

			/*
			 * others may be creating/dropping resource group concurrently,
			 * block until creating/dropping finish to avoid inconsistent
			 * resource group metadata
			 */
			pg_resgroup_rel = table_open(ResGroupRelationId, ExclusiveLock);

			sscan = systable_beginscan(pg_resgroup_rel, InvalidOid, false,
									   NULL, 0, NULL);
			while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
			{
				Oid			oid = ((Form_pg_resgroup) GETSTRUCT(tuple))->oid;

				if (inGroupId == InvalidOid || inGroupId == oid)
				{
					Assert(funcctx->max_calls < MaxResourceGroups);
					ctx->groups[funcctx->max_calls].cpuUsage = makeStringInfo();
					ctx->groups[funcctx->max_calls++].groupId = oid;

					if (inGroupId != InvalidOid)
						break;
				}
			}
			systable_endscan(sscan);

			ctx->nGroups = funcctx->max_calls;
			qsort(ctx->groups, ctx->nGroups, sizeof(ctx->groups[0]), compareRow);

			if (ctx->nGroups > 0)
				getResUsage(ctx, inGroupId);

			table_close(pg_resgroup_rel, ExclusiveLock);
		}

		MemoryContextSwitchTo(oldcontext);
	}

	/* stuff done on every call of the function */
	funcctx = SRF_PERCALL_SETUP();
	ctx = (ResGroupStatCtx *) funcctx->user_fctx;

	if (funcctx->call_cntr < funcctx->max_calls)
	{
		/* for each row */
		Datum		values[8];
		bool		nulls[8];
		HeapTuple	tuple;
		Oid			groupId;
		char		statVal[MAXDATELEN + 1];
		ResGroupStat *row = &ctx->groups[funcctx->call_cntr];

		MemSet(values, 0, sizeof(values));
		MemSet(nulls, 0, sizeof(nulls));
		MemSet(statVal, 0, sizeof(statVal));

		values[0] = row->groupId;
		groupId = DatumGetObjectId(values[0]);

		if (Gp_role == GP_ROLE_UTILITY)
		{
			nulls[1] = true;
			nulls[2] = true;
			nulls[3] = true;
			nulls[4] = true;
			nulls[5] = true;
		}
		else
		{
			values[1] = ResGroupGetStat(groupId, RES_GROUP_STAT_NRUNNING);
			values[2] = ResGroupGetStat(groupId, RES_GROUP_STAT_NQUEUEING);
			values[3] = ResGroupGetStat(groupId, RES_GROUP_STAT_TOTAL_QUEUED);
			values[4] = ResGroupGetStat(groupId, RES_GROUP_STAT_TOTAL_EXECUTED);
			values[5] = ResGroupGetStat(groupId, RES_GROUP_STAT_TOTAL_QUEUE_TIME);
		}

		values[6] = CStringGetTextDatum(row->cpuUsage->data);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);

		SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
	}
	else
	{
		/* nothing left */
		SRF_RETURN_DONE(funcctx);
	}
}

/*
 * Get status of resource groups in key-value style
 */
Datum
pg_resgroup_get_status_kv(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	bool             do_dump;

	do_dump = (!PG_ARGISNULL(0) &&
			   strncmp(text_to_cstring(PG_GETARG_TEXT_P(0)), "dump", 4) == 0);

	if (do_dump && !superuser())
	{
		/* Only super user can call this function with para=dump. */
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("only superusers can call this function")));
	}

	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldcontext;
		TupleDesc	tupdesc;

		funcctx = SRF_FIRSTCALL_INIT();

		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		tupdesc = CreateTemplateTupleDesc(3);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "groupid", OIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "prop", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "value", TEXTOID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);
		funcctx->max_calls = do_dump ? 1 : 0;

		MemoryContextSwitchTo(oldcontext);
	}

	/* stuff done on every call of the function */
	funcctx = SRF_PERCALL_SETUP();

	if (funcctx->call_cntr < funcctx->max_calls && do_dump)
	{
		Datum			values[3] = {};
		bool			nulls[3] = {true, true, false};
		HeapTuple		tuple;
		StringInfoData  str;

		initStringInfo(&str);
		/* dump info in QD and collect info from QEs to form str.*/
		dumpResGroupInfo(&str);
		values[2] = CStringGetTextDatum(str.data);
		pfree(str.data);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);

		SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
	}

	SRF_RETURN_DONE(funcctx);
}

static void
dumpResGroupInfo(StringInfo str)
{
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		int               i;
		StringInfoData    str_qd;
		StringInfoData    buffer;
		CdbPgResults      cdb_pgresults = {NULL, 0};
		struct pg_result *pg_result;

		initStringInfo(&str_qd);
		initStringInfo(&buffer);
		appendStringInfo(&buffer,
						 "select * from pg_resgroup_get_status_kv('dump');");
		
		CdbDispatchCommand(buffer.data, 0, &cdb_pgresults);
		
		if (cdb_pgresults.numResults == 0)
			elog(ERROR, "dumpResGroupInfo didn't get back any results from the segDBs");
		
		LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);
		ResGroupDumpInfo(&str_qd);
		LWLockRelease(ResGroupLock);

		/* append all qes and qd together to form str */
		appendStringInfo(str, "{\"info\":[%s,", str_qd.data);
		for (i = 0; i < cdb_pgresults.numResults; i++)
		{
			pg_result = cdb_pgresults.pg_results[i];
			if (PQresultStatus(pg_result) != PGRES_TUPLES_OK)
			{
				cdbdisp_clearCdbPgResults(&cdb_pgresults);
				elog(ERROR, "pg_resgroup_get_status_kv(): resultStatus not tuples_Ok");
			}
			Assert(PQntuples(pg_result) == 1);
			appendStringInfo(str, "%s", PQgetvalue(pg_result, 0, 2));
			if (i < cdb_pgresults.numResults - 1)
				appendStringInfo(str, ",");
		}
		appendStringInfo(str, "]}");
		cdbdisp_clearCdbPgResults(&cdb_pgresults);
	}
	else
	{
		LWLockAcquire(ResGroupLock, LW_EXCLUSIVE);
		ResGroupDumpInfo(str);
		LWLockRelease(ResGroupLock);
	}
}

/*
 * move a query to a resource group
 */
Datum
pg_resgroup_move_query(PG_FUNCTION_ARGS)
{
	int sessionId;
	Oid groupId;
	const char *groupName;

	if (!IsResGroupEnabled())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("resource group is not enabled"))));

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("must be superuser to move query"))));

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		Oid currentGroupId;
		pid_t pid = PG_GETARG_INT32(0);
		groupName = text_to_cstring(PG_GETARG_TEXT_PP(1));

		groupId = get_resgroup_oid(groupName, false);
		sessionId = GetSessionIdByPid(pid);
		if (sessionId == -1)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 (errmsg("cannot find process: %d", pid))));

		currentGroupId = ResGroupGetGroupIdBySessionId(sessionId);
		if (currentGroupId == InvalidOid)
			ereport(ERROR,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 (errmsg("process %d is in IDLE state", pid))));
		if (currentGroupId == groupId)
			PG_RETURN_BOOL(true);

		ResGroupMoveQuery(sessionId, groupId, groupName);
	}
	else if (Gp_role == GP_ROLE_EXECUTE)
	{
		sessionId = PG_GETARG_INT32(0);
		groupName = text_to_cstring(PG_GETARG_TEXT_PP(1));
		groupId = get_resgroup_oid(groupName, false);
		ResGroupSignalMoveQuery(sessionId, NULL, groupId);
	}

	PG_RETURN_BOOL(true);
}
