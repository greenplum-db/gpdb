/*-------------------------------------------------------------------------
 *
 * lockfuncs.c
 *		Functions for SQL access to various lock-manager capabilities.
 *
 * Copyright (c) 2002-2009, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		$PostgreSQL: pgsql/src/backend/utils/adt/lockfuncs.c,v 1.33 2008/03/25 22:42:44 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "storage/proc.h"
#include "utils/builtins.h"

#include "libpq-fe.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbvars.h"

/* This must match enum LockTagType! */
static const char *const LockTagTypeNames[] = {
	"relation",
	"extend",
	"page",
	"tuple",
	"transactionid",
	"virtualxid",
	"resynchronize",
	"append-only segment file",
	"object",
	"resource queue",
	"userlock",
	"advisory"
};

/* Working status for pg_lock_status */
typedef struct
{
	LockData   *lockData;		/* state data from lmgr */
	int			currIdx;		/* current PROCLOCK index */
	int			numSegLocks;	/* Total number of locks being reported back to client */
	int			numsegresults;	/* If we dispatch to segDBs, the number of segresults */
	struct pg_result **segresults;	/* pg_result for each segDB */

} PG_Lock_Status;


/*
 * VXIDGetDatum - Construct a text representation of a VXID
 *
 * This is currently only used in pg_lock_status, so we put it here.
 */
static Datum
VXIDGetDatum(BackendId bid, LocalTransactionId lxid)
{
	/*
	 * The representation is "<bid>/<lxid>", decimal and unsigned decimal
	 * respectively.  Note that elog.c also knows how to format a vxid.
	 */
	char		vxidstr[32];

	snprintf(vxidstr, sizeof(vxidstr), "%d/%u", bid, lxid);

	return CStringGetTextDatum(vxidstr);
}


/*
 * pg_lock_status - produce a view with one row per held or awaited lock mode
 */
Datum
pg_lock_status(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	PG_Lock_Status *mystatus;
	LockData   *lockData;

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
		/* this had better match pg_locks view in system_views.sql */
		tupdesc = CreateTemplateTupleDesc(17, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "locktype",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "database",
						   OIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "relation",
						   OIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "page",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "tuple",
						   INT2OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 6, "virtualxid",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 7, "transactionid",
						   XIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 8, "classid",
						   OIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 9, "objid",
						   OIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 10, "objsubid",
						   INT2OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 11, "virtualtransaction",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 12, "pid",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 13, "mode",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 14, "granted",
						   BOOLOID, -1, 0);
		/*
		 * These next columns are specific to GPDB
		 */
		TupleDescInitEntry(tupdesc, (AttrNumber) 15, "mppSessionId",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 16, "mppIsWriter",
						   BOOLOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 17, "gp_segment_id",
						   INT4OID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		/*
		 * Collect all the locking information that we will format and send
		 * out as a result set.
		 */
		mystatus = (PG_Lock_Status *) palloc(sizeof(PG_Lock_Status));
		funcctx->user_fctx = (void *) mystatus;

		mystatus->lockData = GetLockStatusData();
		mystatus->currIdx = 0;
		mystatus->numSegLocks = 0;
		mystatus->numsegresults = 0;
		mystatus->segresults = NULL;

		/*
		 * Seeing the locks just from the masterDB isn't enough to know what is locked,
		 * or if there is a deadlock.  That's because the segDBs also take locks.
		 * Some locks show up only on the master, some only on the segDBs, and some on both.
		 *
		 * So, let's collect the lock information from all the segDBs.  Sure, this means
		 * there are a lot more rows coming back from pg_locks than before, since most locks
		 * on the segDBs happen across all the segDBs at the same time.  But not always,
		 * so let's play it safe and get them all.
		 */

		if (Gp_role == GP_ROLE_DISPATCH)
		{
			CdbPgResults cdb_pgresults = {NULL, 0};
			StringInfoData buffer;
			int i;
			initStringInfo(&buffer);

			/*
			 * This query has to match the tupledesc we just made above.
			 */

			appendStringInfo(&buffer,
					"SELECT * FROM  pg_lock_status() L "
					 " (locktype text, database oid, relation oid, page int4, tuple int2,"
					 " virtualxid text, transactionid xid, classid oid, objid oid, objsubid int2,"
					 " virtualtransaction text, pid int4, mode text, granted boolean, "
					 " mppSessionId int4, mppIsWriter boolean, gp_segment_id int4) ");

			/*
			 * Why dispatch something here, rather than do a UNION ALL in pg_locks view, and
			 * a join to gp_dist_random('gp_id')?  There are several important reasons.
			 *
			 * The union all method is much slower, and requires taking locks on gp_id.
			 * More importantly, applications such as pgAdmin do queries of this view that
			 * involve a correlated subqueries joining to other catalog tables,
			 * which works if we do it this way, but fails
			 * if the view includes the union all.  That completely breaks the server status
			 * display in pgAdmin.
			 *
			 * Why dispatch this way, rather than via SPI?  There are several advantages.
			 * First, it's easy to get "writer gang is busy" errors if we use SPI.
			 *
			 * Second, this should be much faster, as it doesn't require setting up
			 * the interconnect, and doesn't need to touch any actual data tables to be
			 * able to get the gp_segment_id.
			 *
			 * The downside is we get n result sets, where n == number of segDBs.
			 *
			 * It would be better yet if we sent a plan tree rather than a text string,
			 * so the segDBs don't need to parse it.  That would also avoid taking any relation locks
			 * on the segDB to get this info (normally need to get an accessShareLock on pg_locks on the segDB
			 * to make sure it doesn't go away during parsing).  But the only safe way I know to do this
			 * is to hand-build the plan tree, and I'm to lazy to do it right now. It's just a matter of
			 * building a function scan node, and filling it in with our result set info (from the tupledesc).
			 *
			 * One thing to note:  it's OK to join pg_locks with any catalog table or master-only table,
			 * but joining to a distributed table will result in "writer gang busy: possible attempt to
			 * execute volatile function in unsupported context" errors, because
			 * the scan of the distributed table might already be running on the writer gang
			 * when we want to dispatch this.
			 *
			 * This could be fixed by allocating a reader gang and dispatching to that, but the cost
			 * of setting up a new gang is high, and I've never seen anyone need to join this to a
			 * distributed table.
			 *
			 */

			CdbDispatchCommand(buffer.data, DF_WITH_SNAPSHOT, &cdb_pgresults);

			if (cdb_pgresults.numResults == 0)
				elog(ERROR, "pg_locks didn't get back any data from the segDBs");

			for (i = 0; i < cdb_pgresults.numResults; i++)
			{
				/*
				 * Any error here should have propagated into errbuf, so we shouldn't
				 * ever see anything other that tuples_ok here.  But, check to be
				 * sure.
				 */
				if (PQresultStatus(cdb_pgresults.pg_results[i]) != PGRES_TUPLES_OK)
				{
					cdbdisp_clearCdbPgResults(&cdb_pgresults);
					elog(ERROR,"pg_locks: resultStatus not tuples_Ok");
				}
				else
				{
					/*
					 * numSegLocks needs to be the total size we are returning to
					 * the application. At the start of this loop, it has the count
					 * for the masterDB locks.  Add each of the segDB lock counts.
					 */
					mystatus->numSegLocks += PQntuples(cdb_pgresults.pg_results[i]);
				}
			}

			mystatus->numsegresults = cdb_pgresults.numResults;
			/*
			 * cdbdisp_dispatchRMCommand copies the result sets into our memory, which
			 * will still exist on the subsequent calls.
			 */
			mystatus->segresults = cdb_pgresults.pg_results;
		}

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	mystatus = (PG_Lock_Status *) funcctx->user_fctx;
	lockData = mystatus->lockData;

	/*
	 * This loop returns all the local lock data from the segment we are running on.
	 */

	while (mystatus->currIdx < lockData->nelements)
	{
		PROCLOCK   *proclock;
		LOCK	   *lock;
		PGPROC	   *proc;
		bool		granted;
		LOCKMODE	mode = 0;
		const char *locktypename;
		char		tnbuf[32];
		Datum		values[17];
		bool		nulls[17];
		HeapTuple	tuple;
		Datum		result;

		proclock = &(lockData->proclocks[mystatus->currIdx]);
		lock = &(lockData->locks[mystatus->currIdx]);
		proc = &(lockData->procs[mystatus->currIdx]);

		/*
		 * Look to see if there are any held lock modes in this PROCLOCK. If
		 * so, report, and destructively modify lockData so we don't report
		 * again.
		 */
		granted = false;
		if (proclock->holdMask)
		{
			for (mode = 0; mode < MAX_LOCKMODES; mode++)
			{
				if (proclock->holdMask & LOCKBIT_ON(mode))
				{
					granted = true;
					proclock->holdMask &= LOCKBIT_OFF(mode);
					break;
				}
			}
		}

		/*
		 * If no (more) held modes to report, see if PROC is waiting for a
		 * lock on this lock.
		 */
		if (!granted)
		{
			if (proc->waitLock == proclock->tag.myLock)
			{
				/* Yes, so report it with proper mode */
				mode = proc->waitLockMode;

				/*
				 * We are now done with this PROCLOCK, so advance pointer to
				 * continue with next one on next call.
				 */
				mystatus->currIdx++;
			}
			else
			{
				/*
				 * Okay, we've displayed all the locks associated with this
				 * PROCLOCK, proceed to the next one.
				 */
				mystatus->currIdx++;
				continue;
			}
		}

		/*
		 * Form tuple with appropriate data.
		 */
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));

		if (lock->tag.locktag_type <= LOCKTAG_LAST_TYPE)
			locktypename = LockTagTypeNames[lock->tag.locktag_type];
		else
		{
			snprintf(tnbuf, sizeof(tnbuf), "unknown %d",
					 (int) lock->tag.locktag_type);
			locktypename = tnbuf;
		}
		values[0] = CStringGetTextDatum(locktypename);

		switch (lock->tag.locktag_type)
		{
			case LOCKTAG_RELATION:
			case LOCKTAG_RELATION_EXTEND:
			case LOCKTAG_RELATION_RESYNCHRONIZE:
				values[1] = ObjectIdGetDatum(lock->tag.locktag_field1);
				values[2] = ObjectIdGetDatum(lock->tag.locktag_field2);
				nulls[3] = true;
				nulls[4] = true;
				nulls[5] = true;
				nulls[6] = true;
				nulls[7] = true;
				nulls[8] = true;
				nulls[9] = true;
				break;
			case LOCKTAG_PAGE:
				values[1] = ObjectIdGetDatum(lock->tag.locktag_field1);
				values[2] = ObjectIdGetDatum(lock->tag.locktag_field2);
				values[3] = UInt32GetDatum(lock->tag.locktag_field3);
				nulls[4] = true;
				nulls[5] = true;
				nulls[6] = true;
				nulls[7] = true;
				nulls[8] = true;
				nulls[9] = true;
				break;
			case LOCKTAG_TUPLE:
				values[1] = ObjectIdGetDatum(lock->tag.locktag_field1);
				values[2] = ObjectIdGetDatum(lock->tag.locktag_field2);
				values[3] = UInt32GetDatum(lock->tag.locktag_field3);
				values[4] = UInt16GetDatum(lock->tag.locktag_field4);
				nulls[5] = true;
				nulls[6] = true;
				nulls[7] = true;
				nulls[8] = true;
				nulls[9] = true;
				break;
			case LOCKTAG_TRANSACTION:
				values[6] = TransactionIdGetDatum(lock->tag.locktag_field1);
				nulls[1] = true;
				nulls[2] = true;
				nulls[3] = true;
				nulls[4] = true;
				nulls[5] = true;
				nulls[7] = true;
				nulls[8] = true;
				nulls[9] = true;
				break;
			case LOCKTAG_VIRTUALTRANSACTION:
				values[5] = VXIDGetDatum(lock->tag.locktag_field1,
										 lock->tag.locktag_field2);
				nulls[1] = true;
				nulls[2] = true;
				nulls[3] = true;
				nulls[4] = true;
				nulls[6] = true;
				nulls[7] = true;
				nulls[8] = true;
				nulls[9] = true;
				break;
			case LOCKTAG_RELATION_APPENDONLY_SEGMENT_FILE:
				values[1] = ObjectIdGetDatum(lock->tag.locktag_field1);
				values[2] = ObjectIdGetDatum(lock->tag.locktag_field2);
				values[7] = ObjectIdGetDatum(lock->tag.locktag_field3);
				nulls[3] = true;
				nulls[4] = true;
				nulls[5] = true;
				nulls[6] = true;
				nulls[8] = true;
				nulls[9] = true;
				break;
			case LOCKTAG_RESOURCE_QUEUE:
				values[1] = ObjectIdGetDatum(proc->databaseId);
				values[8] = ObjectIdGetDatum(lock->tag.locktag_field1);
				nulls[2] = true;
				nulls[3] = true;
				nulls[4] = true;
				nulls[5] = true;
				nulls[6] = true;
				nulls[7] = true;
				nulls[9] = true;
				break;
			case LOCKTAG_OBJECT:
			case LOCKTAG_USERLOCK:
			case LOCKTAG_ADVISORY:
			default:			/* treat unknown locktags like OBJECT */
				values[1] = ObjectIdGetDatum(lock->tag.locktag_field1);
				values[7] = ObjectIdGetDatum(lock->tag.locktag_field2);
				values[8] = ObjectIdGetDatum(lock->tag.locktag_field3);
				values[9] = Int16GetDatum(lock->tag.locktag_field4);
				nulls[2] = true;
				nulls[3] = true;
				nulls[4] = true;
				nulls[5] = true;
				nulls[6] = true;
				break;
		}

		values[10] = VXIDGetDatum(proc->backendId, proc->lxid);
		if (proc->pid != 0)
			values[11] = Int32GetDatum(proc->pid);
		else
			nulls[11] = true;
		values[12] = CStringGetTextDatum(GetLockmodeName(LOCK_LOCKMETHOD(*lock), mode));
		values[13] = BoolGetDatum(granted);
		
		values[14] = Int32GetDatum(proc->mppSessionId);

		values[15] = BoolGetDatum(proc->mppIsWriter);

		values[16] = Int32GetDatum(Gp_segment);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);
		SRF_RETURN_NEXT(funcctx, result);
	}

	/*
	 * This loop only executes on the masterDB and only in dispatch mode, because that
	 * is the only time we dispatched to the segDBs.
	 */

	while (mystatus->currIdx >= lockData->nelements && mystatus->currIdx < lockData->nelements + mystatus->numSegLocks)
	{
		HeapTuple	tuple;
		Datum		result;
		Datum		values[17];
		bool		nulls[17];
		int i;
		int whichresultset = 0;
		int whichelement = mystatus->currIdx - lockData->nelements;
		int whichrow = whichelement;

		Assert(Gp_role == GP_ROLE_DISPATCH);

		/*
		 * Because we have one result set per segDB (rather than one big result set with everything),
		 * we need to figure out which result set we are on, and which row within that result set
		 * we are returning.
		 *
		 * So, we walk through all the result sets and all the rows in each one, in order.
		 */

		while(whichrow >= PQntuples(mystatus->segresults[whichresultset]))
		{
			whichrow -= PQntuples(mystatus->segresults[whichresultset]);
			whichresultset++;
			if (whichresultset >= mystatus->numsegresults)
				break;
		}

		/*
		 * If this condition is true, we have already sent everything back,
		 * and we just want to do the SRF_RETURN_DONE
		 */
		if (whichresultset >= mystatus->numsegresults)
			break;

		mystatus->currIdx++;

		/*
		 * Form tuple with appropriate data we got from the segDBs
		 */
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));

		/*
		 * For each column, extract out the value (which comes out in text).
		 * Convert it to the appropriate datatype to match our tupledesc,
		 * and put that in values.
		 * The columns look like this (from select statement earlier):
		 *
		 * "   (locktype text, database oid, relation oid, page int4, tuple int2,"
		 *	"   transactionid xid, classid oid, objid oid, objsubid int2,"
		 *	"    transaction xid, pid int4, mode text, granted boolean, "
		 *	"    mppSessionId int4, mppIsWriter boolean, gp_segment_id int4) ,"
		 */

		values[0] = CStringGetTextDatum(PQgetvalue(mystatus->segresults[whichresultset], whichrow, 0));
		values[1] = ObjectIdGetDatum(atoi(PQgetvalue(mystatus->segresults[whichresultset], whichrow, 1)));
		values[2] = ObjectIdGetDatum(atoi(PQgetvalue(mystatus->segresults[whichresultset], whichrow, 2)));
		values[3] = UInt32GetDatum(atoi(PQgetvalue(mystatus->segresults[whichresultset], whichrow, 3)));
		values[4] = UInt16GetDatum(atoi(PQgetvalue(mystatus->segresults[whichresultset], whichrow, 4)));

		values[5] = CStringGetTextDatum(PQgetvalue(mystatus->segresults[whichresultset], whichrow, 5));
		values[6] = TransactionIdGetDatum(atoi(PQgetvalue(mystatus->segresults[whichresultset], whichrow, 6)));
		values[7] = ObjectIdGetDatum(atoi(PQgetvalue(mystatus->segresults[whichresultset], whichrow, 7)));
		values[8] = ObjectIdGetDatum(atoi(PQgetvalue(mystatus->segresults[whichresultset], whichrow, 8)));
		values[9] = UInt16GetDatum(atoi(PQgetvalue(mystatus->segresults[whichresultset], whichrow, 9)));

		values[10] = CStringGetTextDatum(PQgetvalue(mystatus->segresults[whichresultset], whichrow, 10));
		values[11] = UInt32GetDatum(atoi(PQgetvalue(mystatus->segresults[whichresultset], whichrow, 11)));
		values[12] = CStringGetTextDatum(PQgetvalue(mystatus->segresults[whichresultset], whichrow, 12));
		values[13] = BoolGetDatum(strncmp(PQgetvalue(mystatus->segresults[whichresultset], whichrow,13),"t",1)==0);
		values[14] = Int32GetDatum(atoi(PQgetvalue(mystatus->segresults[whichresultset], whichrow,14)));
		values[15] = BoolGetDatum(strncmp(PQgetvalue(mystatus->segresults[whichresultset], whichrow,15),"t",1)==0);
		values[16] = Int32GetDatum(atoi(PQgetvalue(mystatus->segresults[whichresultset], whichrow,16)));

		/*
		 * Copy the null info over.  It should all match properly.
		 */
		for (i=0; i<17; i++)
		{
			nulls[i] = PQgetisnull(mystatus->segresults[whichresultset], whichrow, i);
		}

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);
		SRF_RETURN_NEXT(funcctx, result);
	}

	/*
	 * if we dispatched to the segDBs, free up the memory holding the result sets.
	 * Otherwise we might leak this memory each time we got called (does it automatically
	 * get freed by the pool being deleted?  Probably, but this is safer).
	 */
	if (mystatus->segresults != NULL)
	{
		int i;
		for (i = 0; i < mystatus->numsegresults; i++)
			PQclear(mystatus->segresults[i]);

		free(mystatus->segresults);
	}

	SRF_RETURN_DONE(funcctx);
}


/*
 * Functions for manipulating advisory locks
 *
 * We make use of the locktag fields as follows:
 *
 *	field1: MyDatabaseId ... ensures locks are local to each database
 *	field2: first of 2 int4 keys, or high-order half of an int8 key
 *	field3: second of 2 int4 keys, or low-order half of an int8 key
 *	field4: 1 if using an int8 key, 2 if using 2 int4 keys
 */
#define SET_LOCKTAG_INT64(tag, key64) \
	SET_LOCKTAG_ADVISORY(tag, \
						 MyDatabaseId, \
						 (uint32) ((key64) >> 32), \
						 (uint32) (key64), \
						 1)
#define SET_LOCKTAG_INT32(tag, key1, key2) \
	SET_LOCKTAG_ADVISORY(tag, MyDatabaseId, key1, key2, 2)

/*
 * pg_advisory_lock(int8) - acquire exclusive lock on an int8 key
 */
Datum
pg_advisory_lock_int8(PG_FUNCTION_ARGS)
{
	int64		key = PG_GETARG_INT64(0);
	LOCKTAG		tag;

	SET_LOCKTAG_INT64(tag, key);

	(void) LockAcquire(&tag, ExclusiveLock, true, false);

	PG_RETURN_VOID();
}

/*
 * pg_advisory_lock_shared(int8) - acquire share lock on an int8 key
 */
Datum
pg_advisory_lock_shared_int8(PG_FUNCTION_ARGS)
{
	int64		key = PG_GETARG_INT64(0);
	LOCKTAG		tag;

	SET_LOCKTAG_INT64(tag, key);

	(void) LockAcquire(&tag, ShareLock, true, false);

	PG_RETURN_VOID();
}

/*
 * pg_try_advisory_lock(int8) - acquire exclusive lock on an int8 key, no wait
 *
 * Returns true if successful, false if lock not available
 */
Datum
pg_try_advisory_lock_int8(PG_FUNCTION_ARGS)
{
	int64		key = PG_GETARG_INT64(0);
	LOCKTAG		tag;
	LockAcquireResult res;

	SET_LOCKTAG_INT64(tag, key);

	res = LockAcquire(&tag, ExclusiveLock, true, true);

	PG_RETURN_BOOL(res != LOCKACQUIRE_NOT_AVAIL);
}

/*
 * pg_try_advisory_lock_shared(int8) - acquire share lock on an int8 key, no wait
 *
 * Returns true if successful, false if lock not available
 */
Datum
pg_try_advisory_lock_shared_int8(PG_FUNCTION_ARGS)
{
	int64		key = PG_GETARG_INT64(0);
	LOCKTAG		tag;
	LockAcquireResult res;

	SET_LOCKTAG_INT64(tag, key);

	res = LockAcquire(&tag, ShareLock, true, true);

	PG_RETURN_BOOL(res != LOCKACQUIRE_NOT_AVAIL);
}

/*
 * pg_advisory_unlock(int8) - release exclusive lock on an int8 key
 *
 * Returns true if successful, false if lock was not held
*/
Datum
pg_advisory_unlock_int8(PG_FUNCTION_ARGS)
{
	int64		key = PG_GETARG_INT64(0);
	LOCKTAG		tag;
	bool		res;

	SET_LOCKTAG_INT64(tag, key);

	res = LockRelease(&tag, ExclusiveLock, true);

	PG_RETURN_BOOL(res);
}

/*
 * pg_advisory_unlock_shared(int8) - release share lock on an int8 key
 *
 * Returns true if successful, false if lock was not held
 */
Datum
pg_advisory_unlock_shared_int8(PG_FUNCTION_ARGS)
{
	int64		key = PG_GETARG_INT64(0);
	LOCKTAG		tag;
	bool		res;

	SET_LOCKTAG_INT64(tag, key);

	res = LockRelease(&tag, ShareLock, true);

	PG_RETURN_BOOL(res);
}

/*
 * pg_advisory_lock(int4, int4) - acquire exclusive lock on 2 int4 keys
 */
Datum
pg_advisory_lock_int4(PG_FUNCTION_ARGS)
{
	int32		key1 = PG_GETARG_INT32(0);
	int32		key2 = PG_GETARG_INT32(1);
	LOCKTAG		tag;

	SET_LOCKTAG_INT32(tag, key1, key2);

	(void) LockAcquire(&tag, ExclusiveLock, true, false);

	PG_RETURN_VOID();
}

/*
 * pg_advisory_lock_shared(int4, int4) - acquire share lock on 2 int4 keys
 */
Datum
pg_advisory_lock_shared_int4(PG_FUNCTION_ARGS)
{
	int32		key1 = PG_GETARG_INT32(0);
	int32		key2 = PG_GETARG_INT32(1);
	LOCKTAG		tag;

	SET_LOCKTAG_INT32(tag, key1, key2);

	(void) LockAcquire(&tag, ShareLock, true, false);

	PG_RETURN_VOID();
}

/*
 * pg_try_advisory_lock(int4, int4) - acquire exclusive lock on 2 int4 keys, no wait
 *
 * Returns true if successful, false if lock not available
 */
Datum
pg_try_advisory_lock_int4(PG_FUNCTION_ARGS)
{
	int32		key1 = PG_GETARG_INT32(0);
	int32		key2 = PG_GETARG_INT32(1);
	LOCKTAG		tag;
	LockAcquireResult res;

	SET_LOCKTAG_INT32(tag, key1, key2);

	res = LockAcquire(&tag, ExclusiveLock, true, true);

	PG_RETURN_BOOL(res != LOCKACQUIRE_NOT_AVAIL);
}

/*
 * pg_try_advisory_lock_shared(int4, int4) - acquire share lock on 2 int4 keys, no wait
 *
 * Returns true if successful, false if lock not available
 */
Datum
pg_try_advisory_lock_shared_int4(PG_FUNCTION_ARGS)
{
	int32		key1 = PG_GETARG_INT32(0);
	int32		key2 = PG_GETARG_INT32(1);
	LOCKTAG		tag;
	LockAcquireResult res;

	SET_LOCKTAG_INT32(tag, key1, key2);

	res = LockAcquire(&tag, ShareLock, true, true);

	PG_RETURN_BOOL(res != LOCKACQUIRE_NOT_AVAIL);
}

/*
 * pg_advisory_unlock(int4, int4) - release exclusive lock on 2 int4 keys
 *
 * Returns true if successful, false if lock was not held
*/
Datum
pg_advisory_unlock_int4(PG_FUNCTION_ARGS)
{
	int32		key1 = PG_GETARG_INT32(0);
	int32		key2 = PG_GETARG_INT32(1);
	LOCKTAG		tag;
	bool		res;

	SET_LOCKTAG_INT32(tag, key1, key2);

	res = LockRelease(&tag, ExclusiveLock, true);

	PG_RETURN_BOOL(res);
}

/*
 * pg_advisory_unlock_shared(int4, int4) - release share lock on 2 int4 keys
 *
 * Returns true if successful, false if lock was not held
 */
Datum
pg_advisory_unlock_shared_int4(PG_FUNCTION_ARGS)
{
	int32		key1 = PG_GETARG_INT32(0);
	int32		key2 = PG_GETARG_INT32(1);
	LOCKTAG		tag;
	bool		res;

	SET_LOCKTAG_INT32(tag, key1, key2);

	res = LockRelease(&tag, ShareLock, true);

	PG_RETURN_BOOL(res);
}

/*
 * pg_advisory_unlock_all() - release all advisory locks
 */
Datum
pg_advisory_unlock_all(PG_FUNCTION_ARGS)
{
	LockReleaseAll(USER_LOCKMETHOD, true);

	PG_RETURN_VOID();
}
