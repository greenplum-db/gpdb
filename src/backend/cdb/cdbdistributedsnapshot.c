/*-------------------------------------------------------------------------
 *
 * cdbdistributedsnapshot.c
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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
#include "utils/guc.h"
#include "utils/snapmgr.h"
#include "storage/procarray.h"

/*
 * DistributedSnapshotWithLocalMapping_CommittedTest
 *		Is the given XID still-in-progress according to the
 *      distributed snapshot?  Or, is the transaction strictly local
 *      and needs to be tested with the local snapshot?
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

	Assert(!IS_QUERY_DISPATCHER());

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
	if (dslm->currentLocalXidsCount > 0)
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
			LocalDistribXactCache_AddCommitted(localXid,
											   distribXid);
		}
		else
		{
			/*
			 * The distributedlog doesn't know of the transaction. It can be
			 * local-only, or still in-progress. The caller will proceed to do
			 * a local visibility check, which will determine which it is.
			 */
			return DISTRIBUTEDSNAPSHOT_COMMITTED_UNKNOWN;
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
	 * latestCompletedDxid + 1.
	 */
	if (distribXid >= ds->xmax)
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
			if (dslm->currentLocalXidsCount < ds->count)
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
	if (distributedSnapshot->inProgressXidArray == NULL)
	{
		distributedSnapshot->inProgressXidArray =
			(DistributedTransactionId*) malloc(GetMaxSnapshotXidCount() * sizeof(DistributedTransactionId));
		if (distributedSnapshot->inProgressXidArray == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_OUT_OF_MEMORY),
					 errmsg("out of memory")));
	}
}

/*
 * Make a copy of a DistributedSnapshot, allocating memory for the in-progress
 * array if necessary.
 *
 * Note: 'target' should be from a static variable, like the argument of GetSnapshotData()
 */
void
DistributedSnapshot_Copy(DistributedSnapshot *target,
						 DistributedSnapshot *source)
{
	DistributedSnapshot_Reset(target);

	Assert(source->xminAllDistributedSnapshots);
	Assert(source->xminAllDistributedSnapshots <= source->xmin);

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "DistributedSnapshot_Copy target inProgressXidArray %p, and "
		 "source count %d, inProgressXidArray %p",
		 target->inProgressXidArray,
		 source->count,
		 source->inProgressXidArray);

	target->distribTransactionTimeStamp = source->distribTransactionTimeStamp;
	target->xminAllDistributedSnapshots = source->xminAllDistributedSnapshots;
	target->distribSnapshotId = source->distribSnapshotId;
	target->xmin = source->xmin;
	target->xmax = source->xmax;
	target->count = source->count;

	if (source->count == 0)
		return;

	if (target->inProgressXidArray == NULL)
	{
		target->inProgressXidArray =
			(DistributedTransactionId*) malloc(GetMaxSnapshotXidCount() * sizeof(DistributedTransactionId));
		if (target->inProgressXidArray == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_OUT_OF_MEMORY),
					 errmsg("out of memory")));
	}

	Assert(source->count <= GetMaxSnapshotXidCount());
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
	memcpy(&ds->count, p, sizeof(int32));
	p += sizeof(int32);

	if (ds->count > 0)
	{
		int xipsize = sizeof(DistributedTransactionId) * ds->count;

		if (ds->inProgressXidArray == NULL)
		{
			ds->inProgressXidArray =
				(DistributedTransactionId *)malloc(sizeof(DistributedTransactionId) * GetMaxSnapshotXidCount());
			if (ds->inProgressXidArray == NULL)
				ereport(ERROR,
						(errcode(ERRCODE_OUT_OF_MEMORY),
						 errmsg("out of memory")));
		}

		memcpy(ds->inProgressXidArray, p, xipsize);
		p += xipsize;
	}

	Assert((p - buf) == DistributedSnapshot_SerializeSize(ds));
	return (p - buf);
}
