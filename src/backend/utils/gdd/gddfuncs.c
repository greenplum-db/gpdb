/*-------------------------------------------------------------------------
 *
 * gddfuncs.c
 *	  Global DeadLock Detector - Helper Functions
 *
 *
 * Copyright (c) 2018-Present VMware, Inc. or its affiliates.
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbgang.h"
#include "cdb/cdbvars.h"
#include "funcapi.h"
#include "libpq-fe.h"
#include "storage/lock.h"
#include "storage/proc.h"
#include "utils/builtins.h"

typedef struct GddWaitStatusCtx GddWaitStatusCtx;

struct GddWaitStatusCtx
{
	LockData	*lockData;
	CdbPgResults cdb_pgresults;

	int			waiter;
	int			holder;

	int			seg;
	int			row;
};

static bool isGranted(LockInstanceData *lock);
static bool lockEqual(LockInstanceData *lock1, LockInstanceData *lock2);
static bool lockIsHoldTillEndXact(LockInstanceData *lock);

static bool
isGranted(LockInstanceData *lock)
{
	return lock->holdMask != 0;
}

static bool
lockEqual(LockInstanceData *lock1, LockInstanceData *lock2)
{
	LOCKTAG		*tag1 = &lock1->locktag;
	LOCKTAG		*tag2 = &lock2->locktag;

	return memcmp(tag1, tag2, sizeof(LOCKTAG)) == 0;
}

static bool
lockIsHoldTillEndXact(LockInstanceData *lock)
{
	LOCKTAG		*tag = &lock->locktag;

	if (lock->holdTillEndXact)
		return true;

	if (tag->locktag_type == LOCKTAG_TRANSACTION)
		return true;

	/* FIXME: other types */

	return false;
}

/*
 * gp_dist_wait_status - produce a view with one row per waiting relation
 */
Datum
gp_dist_wait_status(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	HeapTuple	tuple;
	Datum		result;
	Datum		values[10];
	bool		nulls[10];
	char        tnbuf[32];
	GddWaitStatusCtx *ctx;

	MemSet(values, 0, sizeof(values));
	MemSet(nulls, false, sizeof(nulls));

	if (SRF_IS_FIRSTCALL())
	{
		TupleDesc	tupdesc;
		MemoryContext oldcontext;

		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/*
		 * switch to memory context appropriate for multiple function calls
		 */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/* build tupdesc for result tuples */
		tupdesc = CreateTemplateTupleDesc(10);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "segid",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "waiter_dxid",
						   XIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "holder_dxid",
						   XIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "holdTillEndXact",
						   BOOLOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "waiter_lpid",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 6, "holder_lpid",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 7, "waiter_lockmode",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 8, "waiter_locktype",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 9, "waiter_sessiondid",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 10, "holder_sessionid",
						   INT4OID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		ctx = palloc(sizeof(*ctx));
		funcctx->user_fctx = ctx;

		ctx->cdb_pgresults.pg_results = NULL;
		ctx->cdb_pgresults.numResults = 0;
		ctx->waiter = 0;
		ctx->holder = 0;
		ctx->seg = 0;
		ctx->row = 0;

		/*
		 * We need to collect the wait status from all the segments,
		 * so a dispatch is needed on QD.
		 */

		if (Gp_role == GP_ROLE_DISPATCH)
		{
			int			i;

			CdbDispatchCommand("SELECT * FROM pg_catalog.gp_dist_wait_status()",
							   DF_NONE, &ctx->cdb_pgresults);

			if (ctx->cdb_pgresults.numResults == 0)
				elog(ERROR, "gp_dist_wait_status() didn't get back any data from the segDBs");

			for (i = 0; i < ctx->cdb_pgresults.numResults; i++)
			{
				/*
				 * Any error here should have propagated into errbuf,
				 * so we shouldn't ever see anything other that tuples_ok here.
				 * But, check to be sure.
				 */
				if (PQresultStatus(ctx->cdb_pgresults.pg_results[i]) != PGRES_TUPLES_OK)
				{
					cdbdisp_clearCdbPgResults(&ctx->cdb_pgresults);
					elog(ERROR,"gp_dist_wait_status(): resultStatus not tuples_Ok");
				}

				/*
				 * This query better match the tupledesc we just made above.
				 */
				if (PQnfields(ctx->cdb_pgresults.pg_results[i]) != tupdesc->natts)
					elog(ERROR, "unexpected number of columns returned from pg_lock_status() on segment (%d, expected %d)",
						 PQnfields(ctx->cdb_pgresults.pg_results[i]), tupdesc->natts);
			}
		}

		/*
		 * QD information must be collected after the dispatch.
		 * Thus the wait relations on QD returned by this function
		 * guarantee to be more later than the wait relations on QEs.
		 * And this information can help while reducing edges on QD.
		 *
		 * Rule: When we are to reduce a dotted edge on QD, we have to think
		 * more. If the lock-holding vertex(transaction) of this QD-edge,
		 * is blocked on any QE. We should not reduce the edge at least
		 * now.
		 *
		 * If any QEs wait relation is later than QD's, we cannot use the
		 * above rule.
		 */
		ctx->lockData = GetLockStatusData();

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	ctx = funcctx->user_fctx;

	/*
	 * Find out all the waiting relations
	 *
	 * A relation is that:
	 * (waiter.granted == false &&
	 *  holder.granted == true &&
	 *  waiter.waitLockMode conflict with holder.holdMask &&
	 *  waiter.pid != holder.pid &&
	 *  waiter.locktype == holder.locktype &&
	 *  waiter.locktags == holder.locktags)
	 *
	 * The actual logic here is to perform a nested loop like this:
	 *
	 *     for (waiter = 0; waiter < nelements; waiter++)
	 *         for (holder = 0; holder < nelements; holder++)
	 *             if (is_a_relation(waiter, holder))
	 *                 record the relation;
	 *
	 * But as we can only return one relation per call, we need to
	 * simulate the loops manually.
	 *
	 * One transaction can wait for more than one transaction on the same
	 * segment. For example, Tx A, B, and C all open a relation with shared
	 * mode, and Tx D try to open the same relation with exclusive mode, D
	 * will be blocked. There are 3 waiting edges(D-->A, D--->B, D--->C).
	 */
	while (ctx->waiter < ctx->lockData->nelements)
	{
		int			waiter = ctx->waiter;
		LockInstanceData	   *w_lock = &ctx->lockData->locks[waiter];

		/* A waiter should have granted == false */
		if (isGranted(w_lock))
		{
			ctx->waiter++;
			ctx->holder = 0;
			continue;
		}

		while (ctx->holder < ctx->lockData->nelements)
		{
			TransactionId w_dxid;
			TransactionId h_dxid;
			const char   *locktypename;
			int			holder = ctx->holder++;
			LockInstanceData	   *h_lock = &ctx->lockData->locks[holder];

			if (holder == waiter)
				continue;
			/* A holder should have granted == true */
			if (!isGranted(h_lock))
				continue;
			if (w_lock->pid == h_lock->pid)
				continue;
			/* If waiter and holder have different lock methods, they must not be conflict */
			if (w_lock->locktag.locktag_lockmethodid != h_lock->locktag.locktag_lockmethodid)
				continue;
			/* If waiter and holder are not conflict, should skip this edge */
			if (!CheckWaitLockModeConflictHoldMask(w_lock->locktag,
					w_lock->waitLockMode, h_lock->holdMask))
				continue;
			if (!lockEqual(w_lock, h_lock))
				continue;

			/* A valid waiting relation is found */

			/* Find out dxid differently on QD and QE */
			w_dxid = w_lock->distribXid;
			h_dxid = h_lock->distribXid;

			values[0] = Int32GetDatum(GpIdentity.segindex);
			values[1] = TransactionIdGetDatum(w_dxid);
			values[2] = TransactionIdGetDatum(h_dxid);
			values[3] = BoolGetDatum(lockIsHoldTillEndXact(w_lock));
			values[4] = Int32GetDatum(w_lock->pid);
			values[5] = Int32GetDatum(h_lock->pid);
			values[6] = CStringGetTextDatum(GetLockmodeName(w_lock->locktag.locktag_lockmethodid, w_lock->waitLockMode));

			if (w_lock->locktag.locktag_type <= LOCKTAG_LAST_TYPE)
				locktypename = LockTagTypeNames[w_lock->locktag.locktag_type];
			else
			{
				snprintf(tnbuf, sizeof(tnbuf), "unknown %d",
						 (int) w_lock->locktag.locktag_type);
				locktypename = tnbuf;
			}
			values[7] = CStringGetTextDatum(locktypename);
			values[8] = Int32GetDatum(w_lock->mppSessionId);
			values[9] = Int32GetDatum(h_lock->mppSessionId);

			tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
			result = HeapTupleGetDatum(tuple);

			SRF_RETURN_NEXT(funcctx, result);
		}

		ctx->waiter++;
		ctx->holder = 0;
	}

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		while (ctx->seg < ctx->cdb_pgresults.numResults)
		{
			int seg = ctx->seg;
			struct pg_result *pg_result = ctx->cdb_pgresults.pg_results[seg];

			while (ctx->row < PQntuples(pg_result))
			{
				int row = ctx->row++;

				values[0] = Int32GetDatum(atoi(PQgetvalue(pg_result, row, 0)));
				values[1] = TransactionIdGetDatum(atoi(PQgetvalue(pg_result, row, 1)));
				values[2] = TransactionIdGetDatum(atoi(PQgetvalue(pg_result, row, 2)));
				values[3] = BoolGetDatum(strncmp(PQgetvalue(pg_result, row, 3), "t", 1) == 0);
				values[4] = Int32GetDatum(atoi(PQgetvalue(pg_result, row, 4)));
				values[5] = Int32GetDatum(atoi(PQgetvalue(pg_result, row, 5)));
				values[6] = CStringGetTextDatum(PQgetvalue(pg_result, row, 6));
				values[7] = CStringGetTextDatum(PQgetvalue(pg_result, row, 7));
				values[8] = Int32GetDatum(atoi(PQgetvalue(pg_result, row, 8)));
				values[9] = Int32GetDatum(atoi(PQgetvalue(pg_result, row, 9)));

				tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
				result = HeapTupleGetDatum(tuple);
				SRF_RETURN_NEXT(funcctx, result);
			}

			ctx->seg++;
			ctx->row = 0;
		}
	}

	cdbdisp_clearCdbPgResults(&ctx->cdb_pgresults);
	if (Gp_role == GP_ROLE_DISPATCH)
		DisconnectAndDestroyAllGangs(false);

	SRF_RETURN_DONE(funcctx);
}
