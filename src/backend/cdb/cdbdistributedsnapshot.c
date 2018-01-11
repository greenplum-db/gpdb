/*-------------------------------------------------------------------------
 *
 * cdbdistributedsnapshot.c
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbdistributedsnapshot.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "cdb/cdbdistributedsnapshot.h"
#include "cdb/cdblocaldistribxact.h"
#include "access/distributedlog.h"
#include "miscadmin.h"
#include "access/transam.h"
#include "cdb/cdbvars.h"
#include "utils/snapmgr.h"

/*
 * Purpose of this function is on pretty same lines as
 * HeapTupleSatisfiesVacuum() just more from distributed perspective.
 *
 * Helps to determine the status of tuples for VACUUM, PagePruning and
 * FREEZING purposes. Here, what we mainly want to know is:
 * - if a tuple is potentially visible to *any* running transaction GLOBALLY
 * in cluster. If so, it can't be removed yet by VACUUM.
 * - also, if a tuple is visible to *all* current and future transactions,
 *   then it can be freezed by VACUUM.
 *
 * xminAllDistributedSnapshots is a cutoff XID (obtained from distributed
 * snapshot). Tuples deleted by dxids >= xminAllDistributedSnapshots are
 * deemed "recently dead"; they might still be visible to some open
 * transaction globally, so we can't remove them, even if we see that the
 * deleting transaction has committed and even if locally its lower than
 * OldestXmin.
 *
 * Function is coded with conservative mind-set, to make sure tuples are
 * deleted or freezed only if can be evaluated and guaranteed to be known
 * meeting above mentioned criteria. So, any scenarios in which global
 * snapshot can't be checked it returns to not do anything to the tuple. For
 * example running vacuum in utility mode for particular QE directly, in which
 * case don't have distributed snapshot to check against, it will not allow
 * marking tuples DEAD just based on local information.
 */
bool
localXidSatisfiesAnyDistributedSnapshot(TransactionId localXid)
{
	DistributedSnapshotWithLocalMapping *dslm;


	Assert(TransactionIdIsNormal(localXid));

	/*
	 * For single user mode operation like initdb time, let the vacuum
	 * cleanout and freeze tuples.
	 */
	if (!IsUnderPostmaster || !IsNormalProcessingMode())
		return false;

	dslm = GetCurrentDistributedSnapshotWithLocalMapping();

	/* Only if we have distributed snapshot, evaluate against it */
	if (dslm)
	{
		DistributedSnapshotCommitted distributedSnapshotCommitted =
			DistributedSnapshotWithLocalMapping_CommittedTest(dslm, localXid, true);

		switch (distributedSnapshotCommitted)
		{
			case DISTRIBUTEDSNAPSHOT_COMMITTED_INPROGRESS:
				return true;

			case DISTRIBUTEDSNAPSHOT_COMMITTED_IGNORE:
				return false;

			default:
				elog(ERROR,
					 "unrecognized distributed committed test result: %d for localXid %u",
					 (int) distributedSnapshotCommitted, localXid);
				break;
		}
	}

	/*
	 * If don't have snapshot or distributed snapshot, can't check the global
	 * visibility and hence convey not to clean-up the tuple.
	 */
	return true;
}

/*
 * DistributedSnapshotWithLocalMapping_CommittedTest
 *		Is the given XID still-in-progress according to the
 *      distributed snapshot?  Or, is the transaction strictly local
 *      and needs to be tested with the local snapshot?
 *
 * The caller should've checked that the XID is committed (in clog),
 * otherwise the result of this function is undefined.
 */
DistributedSnapshotCommitted
DistributedSnapshotWithLocalMapping_CommittedTest(
												  DistributedSnapshotWithLocalMapping *dslm,
												  TransactionId localXid,
												  bool isVacuumCheck)
{
	DistributedSnapshot *ds = &dslm->ds;
	uint32		i;
	DistributedTransactionId distribXid = InvalidDistributedTransactionId;

	/*
	 * Return early if local xid is not normal as it cannot have distributed
	 * xid associated with it.
	 */
	if (!TransactionIdIsNormal(localXid))
		return DISTRIBUTEDSNAPSHOT_COMMITTED_IGNORE;

	/*
	 * Checking the distributed committed log can be expensive, so make a scan
	 * through our cache in distributed snapshot looking for a possible
	 * corresponding local xid only if it has value in checking.
	 */
	if (dslm->currentLocalXidsCount)
	{
		Assert(TransactionIdIsNormal(dslm->minCachedLocalXid));
		Assert(TransactionIdIsNormal(dslm->maxCachedLocalXid));

		if (TransactionIdEquals(localXid, dslm->minCachedLocalXid) ||
			TransactionIdEquals(localXid, dslm->maxCachedLocalXid))
		{
			return DISTRIBUTEDSNAPSHOT_COMMITTED_INPROGRESS;
		}

		if (TransactionIdFollows(localXid, dslm->minCachedLocalXid) &&
			TransactionIdPrecedes(localXid, dslm->maxCachedLocalXid))
		{
			for (i = 0; i < dslm->currentLocalXidsCount; i++)
			{
				Assert(dslm->inProgressMappedLocalXids != NULL);
				Assert(TransactionIdIsValid(dslm->inProgressMappedLocalXids[i]));

				if (TransactionIdEquals(localXid, dslm->inProgressMappedLocalXids[i]))
					return DISTRIBUTEDSNAPSHOT_COMMITTED_INPROGRESS;
			}
		}
	}

	/*
	 * Is this local xid in a process-local cache we maintain?
	 */
	if (LocalDistribXactCache_CommittedFind(localXid,
											ds->distribTransactionTimeStamp,
											&distribXid))
	{
		/*
		 * We cache local-only committed transactions for better performance,
		 * too.
		 */
		if (distribXid == InvalidDistributedTransactionId)
			return DISTRIBUTEDSNAPSHOT_COMMITTED_IGNORE;

		/*
		 * Fall below and evaluate the committed distributed transaction
		 * against the distributed snapshot.
		 */
	}
	else
	{
		DistributedTransactionTimeStamp checkDistribTimeStamp;

		/*
		 * Ok, now we must consult the distributed log.
		 */
		if (DistributedLog_CommittedCheck(localXid,
										  &checkDistribTimeStamp,
										  &distribXid))
		{
			/*
			 * We found it in the distributed log.
			 */
			Assert(checkDistribTimeStamp != 0);
			Assert(distribXid != InvalidDistributedTransactionId);

			/*
			 * Committed distributed transactions from other DTM starts are
			 * weeded out.
			 */
			if (checkDistribTimeStamp != ds->distribTransactionTimeStamp)
				return DISTRIBUTEDSNAPSHOT_COMMITTED_IGNORE;

			/*
			 * We have a distributed committed xid that corresponds to the
			 * local xid.
			 */
			Assert(distribXid != InvalidDistributedTransactionId);

			/*
			 * Since we did not find it in our process local cache, add it.
			 */
			LocalDistribXactCache_AddCommitted(
											   localXid,
											   ds->distribTransactionTimeStamp,
											   distribXid);
		}
		else
		{
			/*
			 * Since the local xid is committed (as determined by the
			 * visibility routine) and distributedlog doesn't know of the
			 * transaction, it must be local-only.
			 */
			LocalDistribXactCache_AddCommitted(localXid,
											   ds->distribTransactionTimeStamp,
											    /* distribXid */ InvalidDistributedTransactionId);

			return DISTRIBUTEDSNAPSHOT_COMMITTED_IGNORE;
		}
	}

	Assert(ds->xminAllDistributedSnapshots != InvalidDistributedTransactionId);

	/*
	 * If this distributed transaction is older than all the distributed
	 * snapshots, then we can ignore it from now on.
	 */
	Assert(ds->xmin >= ds->xminAllDistributedSnapshots);

	if (distribXid < ds->xminAllDistributedSnapshots)
		return DISTRIBUTEDSNAPSHOT_COMMITTED_IGNORE;

	/*
	 * If called to check for purpose of vacuum, in-progress is not
	 * interesting to check and hence just return.
	 */
	if (isVacuumCheck)
		return DISTRIBUTEDSNAPSHOT_COMMITTED_INPROGRESS;

	/* Any xid < xmin is not in-progress */
	if (distribXid < ds->xmin)
		return DISTRIBUTEDSNAPSHOT_COMMITTED_VISIBLE;

	/*
	 * Any xid >= xmax is in-progress, distributed xmax points to the
	 * committer, so it must be visible, so ">" instead of ">="
	 */
	if (distribXid > ds->xmax)
	{
		elog((Debug_print_snapshot_dtm ? LOG : DEBUG5),
			 "distributedsnapshot committed but invisible: distribXid %d dxmax %d dxmin %d distribSnapshotId %d",
			 distribXid, ds->xmax, ds->xmin, ds->distribSnapshotId);

		return DISTRIBUTEDSNAPSHOT_COMMITTED_INPROGRESS;
	}

	for (i = 0; i < ds->count; i++)
	{
		if (distribXid == ds->inProgressXidArray[i])
		{
			/*
			 * Save the relationship to the local xid so we may avoid checking
			 * the distributed committed log in a subsequent check. We can
			 * only record local xids till cache size permits.
			 */
			if (dslm->currentLocalXidsCount < dslm->maxLocalXidsCount)
			{
				Assert(dslm->inProgressMappedLocalXids != NULL);

				dslm->inProgressMappedLocalXids[dslm->currentLocalXidsCount++] =
					localXid;

				if (!TransactionIdIsValid(dslm->minCachedLocalXid) ||
					TransactionIdPrecedes(localXid, dslm->minCachedLocalXid))
				{
					dslm->minCachedLocalXid = localXid;
				}

				if (!TransactionIdIsValid(dslm->maxCachedLocalXid) ||
					TransactionIdFollows(localXid, dslm->maxCachedLocalXid))
				{
					dslm->maxCachedLocalXid = localXid;
				}
			}

			return DISTRIBUTEDSNAPSHOT_COMMITTED_INPROGRESS;
		}

		/*
		 * Leverage the fact that ds->inProgressXidArray is sorted in
		 * ascending order based on distribXid while creating the snapshot in
		 * CreateDistributedSnapshot(). So, can fail fast once known are
		 * lower than rest of them.
		 */
		if (distribXid < ds->inProgressXidArray[i])
			break;
	}

	/*
	 * Not in-progress, therefore visible.
	 */
	return DISTRIBUTEDSNAPSHOT_COMMITTED_VISIBLE;
}

/*
 * Reset all fields except maxCount and the malloc'd pointer for
 * inProgressXidArray.
 */
void
DistributedSnapshot_Reset(DistributedSnapshot *distributedSnapshot)
{
	distributedSnapshot->distribTransactionTimeStamp = 0;
	distributedSnapshot->xminAllDistributedSnapshots = InvalidDistributedTransactionId;
	distributedSnapshot->distribSnapshotId = 0;
	distributedSnapshot->xmin = InvalidDistributedTransactionId;
	distributedSnapshot->xmax = InvalidDistributedTransactionId;
	distributedSnapshot->count = 0;

	/* maxCount and inProgressXidArray left untouched */
	if (distributedSnapshot->maxCount < 0)
		elog(ERROR, "cannot reset a copied distributed snapshot");
}

/*
 * Make a copy of a DistributedSnapshot, allocating memory for the in-progress
 * array if necessary.
 */
void
DistributedSnapshot_Copy(DistributedSnapshot *target,
						 DistributedSnapshot *source)
{
	DistributedSnapshot_Reset(target);

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "DistributedSnapshot_Copy target maxCount %d, inProgressXidArray %p, and "
		 "source maxCount %d, count %d, inProgressXidArray %p",
		 target->maxCount,
		 target->inProgressXidArray,
		 source->maxCount,
		 source->count,
		 source->inProgressXidArray);

	/*
	 * If we have allocated space for the in-progress distributed
	 * transactions, check against that space.  Otherwise, use the source
	 * maxCount as guide in allocating space.
	 */
	if (target->inProgressXidArray)
	{
		if (target->maxCount < source->count)
		{
			free(target->inProgressXidArray);
			target->maxCount = 0;
			target->inProgressXidArray = NULL;
		}
	}

	/*
	 * Allocate the XID array if necessary. Make it large enough to hold
	 * the snapshot we're copying, plus a little headroom to make it more
	 * likely that the space can be reused on next call.
	 */
	if (target->inProgressXidArray == NULL)
	{
#define EXTRA_XID_ARRAY_HEADROOM 10
		int			maxCount = source->count + EXTRA_XID_ARRAY_HEADROOM;

		target->inProgressXidArray =
			(DistributedTransactionId *)
			malloc(maxCount * sizeof(DistributedTransactionId));
		if (target->inProgressXidArray == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_OUT_OF_MEMORY),
					 errmsg("out of memory")));
		target->maxCount = maxCount;
	}

	target->distribTransactionTimeStamp = source->distribTransactionTimeStamp;
	target->xminAllDistributedSnapshots = source->xminAllDistributedSnapshots;
	target->distribSnapshotId = source->distribSnapshotId;

	target->xmin = source->xmin;
	target->xmax = source->xmax;
	target->count = source->count;

	memcpy(target->inProgressXidArray,
		   source->inProgressXidArray,
		   source->count * sizeof(DistributedTransactionId));
}

int
DistributedSnapshot_SerializeSize(DistributedSnapshot *ds)
{
	return sizeof(DistributedTransactionTimeStamp) +
		sizeof(DistributedSnapshotId) +
	/* xminAllDistributedSnapshots, xmin, xmax */
		3 * sizeof(DistributedTransactionId) +
	/* count */
		sizeof(int32) +
	/* Size of inProgressXidArray */
		sizeof(DistributedTransactionId) * ds->count;
}

int
DistributedSnapshot_Serialize(DistributedSnapshot *ds, char *buf)
{
	char	   *p = buf;

	memcpy(p, &ds->distribTransactionTimeStamp, sizeof(DistributedTransactionTimeStamp));
	p += sizeof(DistributedTransactionTimeStamp);
	memcpy(p, &ds->xminAllDistributedSnapshots, sizeof(DistributedTransactionId));
	p += sizeof(DistributedTransactionId);
	memcpy(p, &ds->distribSnapshotId, sizeof(DistributedSnapshotId));
	p += sizeof(DistributedSnapshotId);
	memcpy(p, &ds->xmin, sizeof(DistributedTransactionId));
	p += sizeof(DistributedTransactionId);
	memcpy(p, &ds->xmax, sizeof(DistributedTransactionId));
	p += sizeof(DistributedTransactionId);
	memcpy(p, &ds->count, sizeof(int32));
	p += sizeof(int32);

	memcpy(p, ds->inProgressXidArray, sizeof(DistributedTransactionId) * ds->count);
	p += sizeof(DistributedTransactionId) * ds->count;

	Assert((p - buf) == DistributedSnapshot_SerializeSize(ds));

	return (p - buf);
}

int
DistributedSnapshot_Deserialize(const char *buf, DistributedSnapshot *ds)
{
	const char *p = buf;
	int32		count;

	memcpy(&ds->distribTransactionTimeStamp, p, sizeof(DistributedTransactionTimeStamp));
	p += sizeof(DistributedTransactionTimeStamp);
	memcpy(&ds->xminAllDistributedSnapshots, p, sizeof(DistributedTransactionId));
	p += sizeof(DistributedTransactionId);
	memcpy(&ds->distribSnapshotId, p, sizeof(DistributedSnapshotId));
	p += sizeof(DistributedSnapshotId);
	memcpy(&ds->xmin, p, sizeof(DistributedTransactionId));
	p += sizeof(DistributedTransactionId);
	memcpy(&ds->xmax, p, sizeof(DistributedTransactionId));
	p += sizeof(DistributedTransactionId);
	memcpy(&count, p, sizeof(int32));
	p += sizeof(int32);

	/*
	 * If we have allocated space for the in-progress distributed
	 * transactions, check against that space.  Otherwise, use the received
	 * maxCount as guide in allocating space.
	 */
	if (ds->inProgressXidArray)
	{
		if (ds->maxCount <= 0)
			elog(ERROR, "Bad allocation of in-progress array");

		if (ds->maxCount < count)
		{
			free(ds->inProgressXidArray);
			ds->maxCount = 0;
			ds->inProgressXidArray = NULL;
		}
	}

	if (ds->inProgressXidArray == NULL)
	{
		int			maxCount = count + 10;

		ds->inProgressXidArray = (DistributedTransactionId *)
			malloc(maxCount * sizeof(DistributedTransactionId));
		if (ds->inProgressXidArray == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_OUT_OF_MEMORY),
					 errmsg("out of memory")));
		ds->maxCount = maxCount;
	}

	if (count > 0)
	{
		int			xipsize;

		Assert(ds->inProgressXidArray != NULL);

		xipsize = sizeof(DistributedTransactionId) * count;
		memcpy(ds->inProgressXidArray, p, xipsize);
		p += xipsize;
	}
	ds->count = count;

	Assert((p - buf) == DistributedSnapshot_SerializeSize(ds));

	return (p - buf);
}
