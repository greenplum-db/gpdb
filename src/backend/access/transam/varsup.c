/*-------------------------------------------------------------------------
 *
 * varsup.c
 *	  postgres OID & XID variables support routines
 *
 * Copyright (c) 2000-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/access/transam/varsup.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/clog.h"
#include "access/commit_ts.h"
#include "access/subtrans.h"
#include "access/transam.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "commands/dbcommands.h"
#include "miscadmin.h"
#include "postmaster/autovacuum.h"
#include "storage/pmsignal.h"
#include "storage/proc.h"
#include "utils/faultinjector.h"
#include "utils/guc.h"
#include "utils/syscache.h"

#include "access/distributedlog.h"
#include "cdb/cdbvars.h"
#include "tcop/tcopprot.h"

/* Number of OIDs to prefetch (preallocate) per XLOG write */
#define VAR_OID_PREFETCH		8192

/* pointer to "variable cache" in shared memory (set up by shmem.c) */
VariableCache ShmemVariableCache = NULL;

int xid_stop_limit;
int xid_warn_limit;

/*
 * Allocate the next FullTransactionId for a new transaction or
 * subtransaction.
 *
 * The new XID is also stored into MyPgXact before returning.
 *
 * Note: when this is called, we are actually already inside a valid
 * transaction, since XIDs are now not allocated until the transaction
 * does something.  So it is safe to do a database lookup if we want to
 * issue a warning about XID wrap.
 */
FullTransactionId
GetNewTransactionId(bool isSubXact)
{
	FullTransactionId full_xid;
	TransactionId xid;

	/*
	 * Workers synchronize transaction state at the beginning of each parallel
	 * operation, so we can't account for new XIDs after that point.
	 */
	if (IsInParallelMode())
		elog(ERROR, "cannot assign TransactionIds during a parallel operation");

	/*
	 * During bootstrap initialization, we return the special bootstrap
	 * transaction id.
	 */
	if (IsBootstrapProcessingMode())
	{
		Assert(!isSubXact);
		MyPgXact->xid = BootstrapTransactionId;
		return FullTransactionIdFromEpochAndXid(0, BootstrapTransactionId);
	}

	/* safety check, we should never get this far in a HS standby */
	if (RecoveryInProgress())
		elog(ERROR, "cannot assign TransactionIds during recovery");

	LWLockAcquire(XidGenLock, LW_EXCLUSIVE);

	full_xid = ShmemVariableCache->nextFullXid;
	xid = XidFromFullTransactionId(full_xid);

	/*----------
	 * Check to see if it's safe to assign another XID.  This protects against
	 * catastrophic data loss due to XID wraparound.  The basic rules are:
	 *
	 * If we're past xidVacLimit, start trying to force autovacuum cycles.
	 * If we're past xidWarnLimit, start issuing warnings.
	 * If we're past xidStopLimit, refuse to execute transactions, unless
	 * we are running in single-user mode (which gives an escape hatch
	 * to the DBA who somehow got past the earlier defenses).
	 *
	 * Note that this coding also appears in GetNewMultiXactId.
	 *----------
	 */
	if (TransactionIdFollowsOrEquals(xid, ShmemVariableCache->xidVacLimit))
	{
		/*
		 * For safety's sake, we release XidGenLock while sending signals,
		 * warnings, etc.  This is not so much because we care about
		 * preserving concurrency in this situation, as to avoid any
		 * possibility of deadlock while doing get_database_name(). First,
		 * copy all the shared values we'll need in this path.
		 */
		TransactionId xidWarnLimit = ShmemVariableCache->xidWarnLimit;
		TransactionId xidStopLimit = ShmemVariableCache->xidStopLimit;
		TransactionId xidWrapLimit = ShmemVariableCache->xidWrapLimit;
		Oid			oldest_datoid = ShmemVariableCache->oldestXidDB;

		LWLockRelease(XidGenLock);

		/*
		 * To avoid swamping the postmaster with signals, we issue the autovac
		 * request only once per 64K transaction starts.  This still gives
		 * plenty of chances before we get into real trouble.
		 */
		if (IsUnderPostmaster && (xid % 65536) == 0)
			SendPostmasterSignal(PMSIGNAL_START_AUTOVAC_LAUNCHER);

		if (IsUnderPostmaster &&
			TransactionIdFollowsOrEquals(xid, xidStopLimit))
		{
			char	   *oldest_datname = get_database_name(oldest_datoid);

			 /*
			  * In GPDB, don't say anything about old prepared transactions, because the system
			  * only uses prepared transactions internally. PREPARE TRANSACTION is not available
			  * to users.
			  */

			/* complain even if that DB has disappeared */
			if (oldest_datname)
				ereport(ERROR,
						(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
						 errmsg("database is not accepting commands to avoid wraparound data loss in database \"%s\"",
								oldest_datname),
						 errhint("Shutdown Greenplum Database. Lower the xid_stop_limit GUC. Execute a database-wide VACUUM in that database. Reset the xid_stop_limit GUC."
								 )));
			else
				ereport(ERROR,
						(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
						 errmsg("database is not accepting commands to avoid wraparound data loss in database with OID %u",
								oldest_datoid),
						 errhint("Shutdown Greenplum Database. Lower the xid_stop_limit GUC. Execute a database-wide VACUUM in that database. Reset the xid_stop_limit GUC."
								 )));
		}
		else if (TransactionIdFollowsOrEquals(xid, xidWarnLimit))
		{
			char	   *oldest_datname = get_database_name(oldest_datoid);

			 /*
			  * In GPDB, don't say anything about old prepared transactions, because the system
			  * only uses prepared transactions internally. PREPARE TRANSACTION is not available
			  * to users.
			  */

			/* complain even if that DB has disappeared */
			if (oldest_datname)
				ereport(WARNING,
						(errmsg("database \"%s\" must be vacuumed within %u transactions",
								oldest_datname,
								xidWrapLimit - xid),
						 errhint("To avoid a database shutdown, execute a database-wide VACUUM in that database.\n"
								 "You might also need to commit or roll back old prepared transactions, or drop stale replication slots.")));
			else
				ereport(WARNING,
						(errmsg("database with OID %u must be vacuumed within %u transactions",
								oldest_datoid,
								xidWrapLimit - xid),
						 errhint("To avoid a database shutdown, execute a database-wide VACUUM in that database.\n"
								 "You might also need to commit or roll back old prepared transactions, or drop stale replication slots.")));
		}

		/* Re-acquire lock and start over */
		LWLockAcquire(XidGenLock, LW_EXCLUSIVE);
		full_xid = ShmemVariableCache->nextFullXid;
		xid = XidFromFullTransactionId(full_xid);
	}

	/*
	 * If we are allocating the first XID of a new page of the commit log,
	 * zero out that commit-log page before returning. We must do this while
	 * holding XidGenLock, else another xact could acquire and commit a later
	 * XID before we zero the page.  Fortunately, a page of the commit log
	 * holds 32K or more transactions, so we don't have to do this very often.
	 *
	 * Extend pg_subtrans and pg_commit_ts too.
	 */
	ExtendCLOG(xid);
	ExtendCommitTs(xid);
	ExtendSUBTRANS(xid);
	DistributedLog_Extend(xid);

	/*
	 * Now advance the nextFullXid counter.  This must not happen until after
	 * we have successfully completed ExtendCLOG() --- if that routine fails,
	 * we want the next incoming transaction to try it again.  We cannot
	 * assign more XIDs until there is CLOG space for them.
	 */
	FullTransactionIdAdvance(&ShmemVariableCache->nextFullXid);

	/*
	 * To aid testing, you can set the debug_burn_xids GUC, to consume XIDs
	 * faster. If set, we bump the XID counter to the next value divisible by
	 * 4096, minus one. The idea is to skip over "boring" XID ranges, but
	 * still step through XID wraparound, CLOG page boundaries etc. one XID
	 * at a time.
	 */
	if (Debug_burn_xids)
	{
		uint64 xx;
		uint32		r;

		/*
		 * Based on the minimum of ENTRIES_PER_PAGE (DistributedLog),
		 * SUBTRANS_XACTS_PER_PAGE, CLOG_XACTS_PER_PAGE.
		 */
		const uint64      page_extend_limit = 4 * 1024;

		xx = U64FromFullTransactionId(ShmemVariableCache->nextFullXid);

		r = xx % page_extend_limit;
		if (r > 1 && r < (page_extend_limit - 1))
		{
			xx += page_extend_limit - r - 1;
			ShmemVariableCache->nextFullXid.value = xx;
		}
	}

	/*
	 * We must store the new XID into the shared ProcArray before releasing
	 * XidGenLock.  This ensures that every active XID older than
	 * latestCompletedXid is present in the ProcArray, which is essential for
	 * correct OldestXmin tracking; see src/backend/access/transam/README.
	 *
	 * Note that readers of PGXACT xid fields should be careful to fetch the
	 * value only once, rather than assume they can read a value multiple
	 * times and get the same answer each time.  Note we are assuming that
	 * TransactionId and int fetch/store are atomic.
	 *
	 * The same comments apply to the subxact xid count and overflow fields.
	 *
	 * Use of a write barrier prevents dangerous code rearrangement in this
	 * function; other backends could otherwise e.g. be examining my subxids
	 * info concurrently, and we don't want them to see an invalid
	 * intermediate state, such as an incremented nxids before the array entry
	 * is filled.
	 *
	 * Other processes that read nxids should do so before reading xids
	 * elements with a pg_read_barrier() in between, so that they can be sure
	 * not to read an uninitialized array element; see
	 * src/backend/storage/lmgr/README.barrier.
	 *
	 * If there's no room to fit a subtransaction XID into PGPROC, set the
	 * cache-overflowed flag instead.  This forces readers to look in
	 * pg_subtrans to map subtransaction XIDs up to top-level XIDs. There is a
	 * race-condition window, in that the new XID will not appear as running
	 * until its parent link has been placed into pg_subtrans. However, that
	 * will happen before anyone could possibly have a reason to inquire about
	 * the status of the XID, so it seems OK.  (Snapshots taken during this
	 * window *will* include the parent XID, so they will deliver the correct
	 * answer later on when someone does have a reason to inquire.)
	 */
	if (!isSubXact)
		MyPgXact->xid = xid;	/* LWLockRelease acts as barrier */
	else
	{
		int			nxids = MyPgXact->nxids;

		if (nxids < PGPROC_MAX_CACHED_SUBXIDS)
		{
			MyProc->subxids.xids[nxids] = xid;
			pg_write_barrier();
			MyPgXact->nxids = nxids + 1;
		}
		else
		{
			MyPgXact->overflowed = true;
			ereportif (gp_log_suboverflow_statement, LOG,
						(errmsg("Statement caused suboverflow: %s", debug_query_string)));
		}
	}

	LWLockRelease(XidGenLock);

	return full_xid;
}

/*
 * Read nextFullXid but don't allocate it.
 */
FullTransactionId
ReadNextFullTransactionId(void)
{
	FullTransactionId fullXid;

	LWLockAcquire(XidGenLock, LW_SHARED);
	fullXid = ShmemVariableCache->nextFullXid;
	LWLockRelease(XidGenLock);

	return fullXid;
}

/*
 * Advance nextFullXid to the value after a given xid.  The epoch is inferred.
 * This must only be called during recovery or from two-phase start-up code.
 */
void
AdvanceNextFullTransactionIdPastXid(TransactionId xid)
{
	FullTransactionId newNextFullXid;
	TransactionId next_xid;
	uint32		epoch;

	/*
	 * It is safe to read nextFullXid without a lock, because this is only
	 * called from the startup process or single-process mode, meaning that no
	 * other process can modify it.
	 */
	Assert(AmStartupProcess() || !IsUnderPostmaster);

	/* Fast return if this isn't an xid high enough to move the needle. */
	next_xid = XidFromFullTransactionId(ShmemVariableCache->nextFullXid);
	if (!TransactionIdFollowsOrEquals(xid, next_xid))
		return;

	/*
	 * Compute the FullTransactionId that comes after the given xid.  To do
	 * this, we preserve the existing epoch, but detect when we've wrapped
	 * into a new epoch.  This is necessary because WAL records and 2PC state
	 * currently contain 32 bit xids.  The wrap logic is safe in those cases
	 * because the span of active xids cannot exceed one epoch at any given
	 * point in the WAL stream.
	 */
	TransactionIdAdvance(xid);
	epoch = EpochFromFullTransactionId(ShmemVariableCache->nextFullXid);
	if (unlikely(xid < next_xid))
		++epoch;
	newNextFullXid = FullTransactionIdFromEpochAndXid(epoch, xid);

	/*
	 * We still need to take a lock to modify the value when there are
	 * concurrent readers.
	 */
	LWLockAcquire(XidGenLock, LW_EXCLUSIVE);
	ShmemVariableCache->nextFullXid = newNextFullXid;
	LWLockRelease(XidGenLock);
}

/*
 * Advance the cluster-wide value for the oldest valid clog entry.
 *
 * We must acquire XactTruncationLock to advance the oldestClogXid. It's not
 * necessary to hold the lock during the actual clog truncation, only when we
 * advance the limit, as code looking up arbitrary xids is required to hold
 * XactTruncationLock from when it tests oldestClogXid through to when it
 * completes the clog lookup.
 */
void
AdvanceOldestClogXid(TransactionId oldest_datfrozenxid)
{
	LWLockAcquire(XactTruncationLock, LW_EXCLUSIVE);
	if (TransactionIdPrecedes(ShmemVariableCache->oldestClogXid,
							  oldest_datfrozenxid))
	{
		ShmemVariableCache->oldestClogXid = oldest_datfrozenxid;
	}
	LWLockRelease(XactTruncationLock);
}

/*
 * Determine the last safe XID to allocate using the currently oldest
 * datfrozenxid (ie, the oldest XID that might exist in any database
 * of our cluster), and the OID of the (or a) database with that value.
 */
void
SetTransactionIdLimit(TransactionId oldest_datfrozenxid, Oid oldest_datoid)
{
	TransactionId xidVacLimit;
	TransactionId xidWarnLimit;
	TransactionId xidStopLimit;
	TransactionId xidWrapLimit;
	TransactionId curXid;

	Assert(TransactionIdIsNormal(oldest_datfrozenxid));

	/*
	 * The place where we actually get into deep trouble is halfway around
	 * from the oldest potentially-existing XID.  (This calculation is
	 * probably off by one or two counts, because the special XIDs reduce the
	 * size of the loop a little bit.  But we throw in plenty of slop below,
	 * so it doesn't matter.)
	 */
	xidWrapLimit = oldest_datfrozenxid + (MaxTransactionId >> 1);
	if (xidWrapLimit < FirstNormalTransactionId)
		xidWrapLimit += FirstNormalTransactionId;

	/*
	 * We'll refuse to continue assigning XIDs in interactive mode once we get
	 * within xid_stop_limit transactions of data loss.  This leaves lots of
	 * room for the DBA to fool around fixing things in a standalone backend,
	 * while not being significant compared to total XID space. (Note that since
	 * vacuuming requires one transaction per table cleaned, we had better be
	 * sure there's lots of XIDs left...)
	 */
	xidStopLimit = xidWrapLimit - (TransactionId)xid_stop_limit;
	if (xidStopLimit < FirstNormalTransactionId)
		xidStopLimit -= FirstNormalTransactionId;

	/*
	 * We'll start complaining loudly when we get within xid_warn_limit of
	 * the stop point.  This is kind of arbitrary, but if you let your gas
	 * gauge get down to 1% of full, would you be looking for the next gas
	 * station?  We need to be fairly liberal about this number because there
	 * are lots of scenarios where most transactions are done by automatic
	 * clients that won't pay attention to warnings. (No, we're not gonna make
	 * this configurable.  If you know enough to configure it, you know enough
	 * to not get in this kind of trouble in the first place.)
	 */
	xidWarnLimit = xidStopLimit  - (TransactionId)xid_warn_limit;
	if (xidWarnLimit < FirstNormalTransactionId)
		xidWarnLimit -= FirstNormalTransactionId;

	/*
	 * We'll start trying to force autovacuums when oldest_datfrozenxid gets
	 * to be more than autovacuum_freeze_max_age transactions old.
	 *
	 * Note: guc.c ensures that autovacuum_freeze_max_age is in a sane range,
	 * so that xidVacLimit will be well before xidWarnLimit.
	 *
	 * Note: autovacuum_freeze_max_age is a PGC_POSTMASTER parameter so that
	 * we don't have to worry about dealing with on-the-fly changes in its
	 * value.  It doesn't look practical to update shared state from a GUC
	 * assign hook (too many processes would try to execute the hook,
	 * resulting in race conditions as well as crashes of those not connected
	 * to shared memory).  Perhaps this can be improved someday.  See also
	 * SetMultiXactIdLimit.
	 */
	xidVacLimit = oldest_datfrozenxid + autovacuum_freeze_max_age;
	if (xidVacLimit < FirstNormalTransactionId)
		xidVacLimit += FirstNormalTransactionId;

	/* Grab lock for just long enough to set the new limit values */
	LWLockAcquire(XidGenLock, LW_EXCLUSIVE);
	ShmemVariableCache->oldestXid = oldest_datfrozenxid;
	ShmemVariableCache->xidVacLimit = xidVacLimit;
	ShmemVariableCache->xidWarnLimit = xidWarnLimit;
	ShmemVariableCache->xidStopLimit = xidStopLimit;
	ShmemVariableCache->xidWrapLimit = xidWrapLimit;
	ShmemVariableCache->oldestXidDB = oldest_datoid;
	curXid = XidFromFullTransactionId(ShmemVariableCache->nextFullXid);
	LWLockRelease(XidGenLock);

	/* Log the info */
	ereport(DEBUG1,
			(errmsg("transaction ID wrap limit is %u, limited by database with OID %u",
					xidWrapLimit, oldest_datoid)));

	/*
	 * If past the autovacuum force point, immediately signal an autovac
	 * request.  The reason for this is that autovac only processes one
	 * database per invocation.  Once it's finished cleaning up the oldest
	 * database, it'll call here, and we'll signal the postmaster to start
	 * another iteration immediately if there are still any old databases.
	 */
	if (TransactionIdFollowsOrEquals(curXid, xidVacLimit) &&
		IsUnderPostmaster && !InRecovery)
		SendPostmasterSignal(PMSIGNAL_START_AUTOVAC_LAUNCHER);

	/* Give an immediate warning if past the wrap warn point */
	if (TransactionIdFollowsOrEquals(curXid, xidWarnLimit) && !InRecovery)
	{
		char	   *oldest_datname;

		/*
		 * We can be called when not inside a transaction, for example during
		 * StartupXLOG().  In such a case we cannot do database access, so we
		 * must just report the oldest DB's OID.
		 *
		 * Note: it's also possible that get_database_name fails and returns
		 * NULL, for example because the database just got dropped.  We'll
		 * still warn, even though the warning might now be unnecessary.
		 */
		if (IsTransactionState())
			oldest_datname = get_database_name(oldest_datoid);
		else
			oldest_datname = NULL;

		if (oldest_datname)
			ereport(WARNING,
					(errmsg("database \"%s\" must be vacuumed within %u transactions",
							oldest_datname,
							xidWrapLimit - curXid),
					 errhint("To avoid a database shutdown, execute a database-wide VACUUM in that database.\n"
							 "You might also need to commit or roll back old prepared transactions, or drop stale replication slots.")));
		else
			ereport(WARNING,
					(errmsg("database with OID %u must be vacuumed within %u transactions",
							oldest_datoid,
							xidWrapLimit - curXid),
					 errhint("To avoid a database shutdown, execute a database-wide VACUUM in that database.\n"
							 "You might also need to commit or roll back old prepared transactions, or drop stale replication slots.")));
	}
}


/*
 * ForceTransactionIdLimitUpdate -- does the XID wrap-limit data need updating?
 *
 * We primarily check whether oldestXidDB is valid.  The cases we have in
 * mind are that that database was dropped, or the field was reset to zero
 * by pg_resetwal.  In either case we should force recalculation of the
 * wrap limit.  Also do it if oldestXid is old enough to be forcing
 * autovacuums or other actions; this ensures we update our state as soon
 * as possible once extra overhead is being incurred.
 */
bool
ForceTransactionIdLimitUpdate(void)
{
	TransactionId nextXid;
	TransactionId xidVacLimit;
	TransactionId oldestXid;
	Oid			oldestXidDB;

	/* Locking is probably not really necessary, but let's be careful */
	LWLockAcquire(XidGenLock, LW_SHARED);
	nextXid = XidFromFullTransactionId(ShmemVariableCache->nextFullXid);
	xidVacLimit = ShmemVariableCache->xidVacLimit;
	oldestXid = ShmemVariableCache->oldestXid;
	oldestXidDB = ShmemVariableCache->oldestXidDB;
	LWLockRelease(XidGenLock);

	if (!TransactionIdIsNormal(oldestXid))
		return true;			/* shouldn't happen, but just in case */
	if (!TransactionIdIsValid(xidVacLimit))
		return true;			/* this shouldn't happen anymore either */
	if (TransactionIdFollowsOrEquals(nextXid, xidVacLimit))
		return true;			/* past VacLimit, don't delay updating */
	if (!SearchSysCacheExists1(DATABASEOID, ObjectIdGetDatum(oldestXidDB)))
		return true;			/* could happen, per comments above */
	return false;
}

/*
 * Requires OidGenLock to be held by caller.
 */
static Oid
GetNewObjectIdUnderLock(void)
{
	Oid			result;

	/* safety check, we should never get this far in a HS standby */
	if (RecoveryInProgress())
		elog(ERROR, "cannot assign OIDs during recovery");

	Assert(LWLockHeldByMe(OidGenLock));

	/*
	 * Check for wraparound of the OID counter.  We *must* not return 0
	 * (InvalidOid), and in normal operation we mustn't return anything below
	 * FirstNormalObjectId since that range is reserved for initdb (see
	 * IsCatalogRelationOid()).  Note we are relying on unsigned comparison.
	 *
	 * During initdb, we start the OID generator at FirstBootstrapObjectId, so
	 * we only wrap if before that point when in bootstrap or standalone mode.
	 * The first time through this routine after normal postmaster start, the
	 * counter will be forced up to FirstNormalObjectId.  This mechanism
	 * leaves the OIDs between FirstBootstrapObjectId and FirstNormalObjectId
	 * available for automatic assignment during initdb, while ensuring they
	 * will never conflict with user-assigned OIDs.
	 */
	if (ShmemVariableCache->nextOid < ((Oid) FirstNormalObjectId))
	{
		if (IsPostmasterEnvironment)
		{
			/* wraparound, or first post-initdb assignment, in normal mode */
			ShmemVariableCache->nextOid = FirstNormalObjectId;
			ShmemVariableCache->oidCount = 0;
		}
		else
		{
			/* we may be bootstrapping, so don't enforce the full range */
			if (ShmemVariableCache->nextOid < ((Oid) FirstBootstrapObjectId))
			{
				/* wraparound in standalone mode (unlikely but possible) */
				ShmemVariableCache->nextOid = FirstNormalObjectId;
				ShmemVariableCache->oidCount = 0;
			}
		}
	}

	/* If we run out of logged for use oids then we must log more */
	if (ShmemVariableCache->oidCount == 0)
	{
		XLogPutNextOid(ShmemVariableCache->nextOid + VAR_OID_PREFETCH);
		ShmemVariableCache->oidCount = VAR_OID_PREFETCH;
	}

	result = ShmemVariableCache->nextOid;

	(ShmemVariableCache->nextOid)++;
	(ShmemVariableCache->oidCount)--;

#ifdef FAULT_INJECTOR
	if (SIMPLE_FAULT_INJECTOR("bump_oid") == FaultInjectorTypeSkip)
	{
		/*
		 * CDB: we encounter high oid issues several times, we should
		 * have some test-utils to verify logic under larger oid.
		 */
		if (result <= PG_INT32_MAX) {
			result = PG_INT32_MAX + result % (PG_UINT32_MAX - PG_INT32_MAX) + 1;
		}
	}
#endif

	return result;
}

/*
 * GetNewObjectId -- allocate a new OID
 *
 * OIDs are generated by a cluster-wide counter. Since they are only
 * 32 bits wide, counter wraparound will occur eventually, and
 * therefore it is unwise to assume they are unique unless precautions
 * are taken to make them so. Hence, this routine should generally not
 * be used directly. The only direct callers should be GetNewOid() and
 * GetNewOidWithIndex() in catalog/catalog.c. It's also called from
 * cdb_sync_oid_to_segments() in cdb/cdboidsync.c to synchronize the
 * OID counter on the QD with its QEs.
 */
Oid
GetNewObjectId(void)
{
	Oid			result;

	LWLockAcquire(OidGenLock, LW_EXCLUSIVE);

	result = GetNewObjectIdUnderLock();

	LWLockRelease(OidGenLock);

	return result;
}

/*
 * AdvanceObjectId -- advance object id counter for QD and QE nodes
 *
 * When advancing the Oid counter of a QD, it should only be for the purpose
 * of syncing Oid counters logically compared with the numeric maximum Oid
 * counter value among the primary segments.
 *
 * When advancing the Oid counter of a QE, the QD provides the preassigned OID
 * to the QE nodes which will be used as the relation's OID. QE nodes do not
 * use this OID as the relfilenode value anymore so the OID counter is not
 * incremented. This function forcefully increments the QE node's OID counter
 * to be about the same as the OID provided by the QD node.
 */
void
AdvanceObjectId(Oid newOid)
{
	LWLockAcquire(OidGenLock, LW_EXCLUSIVE);

	if (OidFollowsNextOid(newOid))
	{
		int32 nextOidDifference = (int32)(newOid - ShmemVariableCache->nextOid);

		/*
		 * We directly set the nextOid counter to the given OID instead of
		 * doing incremental calls to GetNewObjectIdUnderLock(). Update the
		 * oidCount to VAR_OID_PREFETCH and create an xlog if we have
		 * exhausted the current oidCount. We should always be moving forward
		 * and never backwards.
		 */
		ShmemVariableCache->nextOid = newOid;
		if (nextOidDifference >= ShmemVariableCache->oidCount)
		{
			XLogPutNextOid(ShmemVariableCache->nextOid + VAR_OID_PREFETCH);
			ShmemVariableCache->oidCount = VAR_OID_PREFETCH;
		}
		else
			ShmemVariableCache->oidCount -= nextOidDifference;
	}

	LWLockRelease(OidGenLock);
}

/*
 * Requires RelfilenodeGenLock to be held by caller.
 */
static Oid
GetNewSegRelfilenodeUnderLock(void)
{
	Oid result;

	Assert(LWLockHeldByMe(RelfilenodeGenLock));

	if (ShmemVariableCache->nextRelfilenode < ((Oid) FirstNormalObjectId) &&
		IsPostmasterEnvironment)
	{
		/* wraparound in normal environment */
		ShmemVariableCache->nextRelfilenode = FirstNormalObjectId;
		ShmemVariableCache->relfilenodeCount = 0;
	}

	if (ShmemVariableCache->relfilenodeCount == 0)
	{
		XLogPutNextRelfilenode(ShmemVariableCache->nextRelfilenode + VAR_OID_PREFETCH);
		ShmemVariableCache->relfilenodeCount = VAR_OID_PREFETCH;
	}

	result = ShmemVariableCache->nextRelfilenode;

	(ShmemVariableCache->nextRelfilenode)++;
	(ShmemVariableCache->relfilenodeCount)--;

	return result;
}

/*
 * GetNewSegRelfilenode -- allocate a new relfilenode value
 *
 * Similar to GetNewObjectId but for relfilenodes. This function has its own
 * separate counter and is used to allocate relfilenode values instead of
 * trying to use the newly generated OIDs (QD) or preassigned OIDs (QE) as the
 * relfilenode.
 */
Oid
GetNewSegRelfilenode(void)
{
	Oid result;

	LWLockAcquire(RelfilenodeGenLock, LW_EXCLUSIVE);

	result = GetNewSegRelfilenodeUnderLock();

	LWLockRelease(RelfilenodeGenLock);

	return result;
}

/*
 * Is the given Oid logically > ShmemVariableCache->nextOid?
 */
bool
OidFollowsNextOid(Oid id)
{
	int32		diff;

	diff = (int32) (id - ShmemVariableCache->nextOid);
	return (diff > 0);
}
