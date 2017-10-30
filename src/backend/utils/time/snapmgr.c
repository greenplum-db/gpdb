/*-------------------------------------------------------------------------
 * snapmgr.c
 *		PostgreSQL snapshot manager
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/utils/time/snapmgr.c,v 1.1 2008/03/26 18:48:59 alvherre Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/xact.h"
#include "access/transam.h"
#include "storage/procarray.h"
#include "utils/snapmgr.h"
#include "utils/tqual.h"

#include "cdb/cdbtm.h"
#include "utils/guc.h"


/*
 * These SnapshotData structs are static to simplify memory allocation
 * (see the hack in GetSnapshotData to avoid repeated malloc/free).
 */
static SnapshotData SerializableSnapshotData = {HeapTupleSatisfiesMVCC};
static SnapshotData LatestSnapshotData = {HeapTupleSatisfiesMVCC};

/* Externally visible pointers to valid snapshots: */
Snapshot	SerializableSnapshot = NULL;
Snapshot	LatestSnapshot = NULL;

/*
 * This pointer is not maintained by this module, but it's convenient
 * to declare it here anyway.  Callers typically assign a copy of
 * GetTransactionSnapshot's result to ActiveSnapshot.
 */
Snapshot	ActiveSnapshot = NULL;

/*
 * These are updated by GetSnapshotData.  We initialize them this way
 * for the convenience of TransactionIdIsInProgress: even in bootstrap
 * mode, we don't want it to say that BootstrapTransactionId is in progress.
 *
 * RecentGlobalXmin is initialized to InvalidTransactionId, to ensure that no
 * one tries to use a stale value.  Readers should ensure that it has been set
 * to something else before using it.
 */
TransactionId TransactionXmin = FirstNormalTransactionId;
TransactionId RecentXmin = FirstNormalTransactionId;
TransactionId RecentGlobalXmin = InvalidTransactionId;


/*
 * GetTransactionSnapshot
 *		Get the appropriate snapshot for a new query in a transaction.
 *
 * The SerializableSnapshot is the first one taken in a transaction.
 * In serializable mode we just use that one throughout the transaction.
 * In read-committed mode, we take a new snapshot each time we are called.
 *
 * Note that the return value points at static storage that will be modified
 * by future calls and by CommandCounterIncrement().  Callers should copy
 * the result with CopySnapshot() if it is to be used very long.
 */
Snapshot
GetTransactionSnapshot(void)
{
	/* First call in transaction? */
	if (SerializableSnapshot == NULL)
	{
		SerializableSnapshot = GetSnapshotData(&SerializableSnapshotData, true);
		return SerializableSnapshot;
	}

	if (IsXactIsoLevelSerializable)
	{
		elog((Debug_print_snapshot_dtm ? LOG : DEBUG5),"[Distributed Snapshot #%u] *Serializable Skip* (gxid = %u, '%s')",
			 (SerializableSnapshot == NULL ? 0 : SerializableSnapshot->distribSnapshotWithLocalMapping.ds.distribSnapshotId),
			 getDistributedTransactionId(),
			 DtxContextToString(DistributedTransactionContext));

		UpdateSerializableCommandId();

		return SerializableSnapshot;
	}

	elog((Debug_print_snapshot_dtm ? LOG : DEBUG5),"[Distributed Snapshot #%u] (gxid = %u, '%s')",
		 (LatestSnapshot == NULL ? 0 : LatestSnapshot->distribSnapshotWithLocalMapping.ds.distribSnapshotId),
		 getDistributedTransactionId(),
		 DtxContextToString(DistributedTransactionContext));

	LatestSnapshot = GetSnapshotData(&LatestSnapshotData, false);

	return LatestSnapshot;
}

/*
 * GetLatestSnapshot
 *		Get a snapshot that is up-to-date as of the current instant,
 *		even if we are executing in SERIALIZABLE mode.
 */
Snapshot
GetLatestSnapshot(void)
{
	/* Should not be first call in transaction */
	if (SerializableSnapshot == NULL)
		elog(ERROR, "no snapshot has been set");

	LatestSnapshot = GetSnapshotData(&LatestSnapshotData, false);

	return LatestSnapshot;
}

/*
 * CopySnapshot
 *		Copy the given snapshot.
 *
 * The copy is palloc'd in the current memory context.
 */
Snapshot
CopySnapshot(Snapshot snapshot)
{
	Snapshot	newsnap;
	Size		subxipoff;
	Size		dsoff = 0;
	Size		size;

	if (!IsMVCCSnapshot(snapshot))
		return snapshot;

	/* We allocate any XID arrays needed in the same palloc block. */
	size = subxipoff = sizeof(SnapshotData) +
		snapshot->xcnt * sizeof(TransactionId);
	if (snapshot->subxcnt > 0)
		size += snapshot->subxcnt * sizeof(TransactionId);

	if (snapshot->haveDistribSnapshot &&
		snapshot->distribSnapshotWithLocalMapping.ds.count > 0)
	{
		dsoff = size;
		size += snapshot->distribSnapshotWithLocalMapping.ds.count *
			sizeof(DistributedTransactionId);
		size += snapshot->distribSnapshotWithLocalMapping.currentLocalXidsCount *
			sizeof(TransactionId);
	}

	newsnap = (Snapshot) palloc(size);
	memcpy(newsnap, snapshot, sizeof(SnapshotData));

	/* setup XID array */
	if (snapshot->xcnt > 0)
	{
		newsnap->xip = (TransactionId *) (newsnap + 1);
		memcpy(newsnap->xip, snapshot->xip,
			   snapshot->xcnt * sizeof(TransactionId));
	}
	else
		newsnap->xip = NULL;

	/* setup subXID array */
	if (snapshot->subxcnt > 0)
	{
		newsnap->subxip = (TransactionId *) ((char *) newsnap + subxipoff);
		memcpy(newsnap->subxip, snapshot->subxip,
			   snapshot->subxcnt * sizeof(TransactionId));
	}
	else
		newsnap->subxip = NULL;

	if (snapshot->haveDistribSnapshot &&
		snapshot->distribSnapshotWithLocalMapping.ds.count > 0)
	{
		newsnap->distribSnapshotWithLocalMapping.ds.inProgressXidArray =
			(DistributedTransactionId*) ((char *) newsnap + dsoff);
		memcpy(newsnap->distribSnapshotWithLocalMapping.ds.inProgressXidArray,
			   snapshot->distribSnapshotWithLocalMapping.ds.inProgressXidArray,
			   snapshot->distribSnapshotWithLocalMapping.ds.count *
			   sizeof(DistributedTransactionId));

		/* Store -1 as the maxCount, to indicate that the array was not malloc'd */
		newsnap->distribSnapshotWithLocalMapping.ds.maxCount = -1;

		/*
		 * Increment offset to point to next chunk of memory allocated for
		 * cache.
		 */
		dsoff +=snapshot->distribSnapshotWithLocalMapping.ds.count *
			sizeof(DistributedTransactionId);

		/* Copy the local xid cache */
		newsnap->distribSnapshotWithLocalMapping.inProgressMappedLocalXids =
			(TransactionId*) ((char *) newsnap + dsoff);
		memcpy(newsnap->distribSnapshotWithLocalMapping.inProgressMappedLocalXids,
			   snapshot->distribSnapshotWithLocalMapping.inProgressMappedLocalXids,
			   snapshot->distribSnapshotWithLocalMapping.currentLocalXidsCount *
			   sizeof(TransactionId));

		newsnap->distribSnapshotWithLocalMapping.maxLocalXidsCount =
			snapshot->distribSnapshotWithLocalMapping.currentLocalXidsCount;
	}
	else
	{
		newsnap->distribSnapshotWithLocalMapping.ds.inProgressXidArray = NULL;
	}

	return newsnap;
}

/*
 * FreeSnapshot
 *		Free a snapshot previously copied with CopySnapshot.
 *
 * This is currently identical to pfree, but is provided for cleanliness.
 *
 * Do *not* apply this to the results of GetTransactionSnapshot or
 * GetLatestSnapshot, since those are just static structs.
 */
void
FreeSnapshot(Snapshot snapshot)
{
	if (!IsMVCCSnapshot(snapshot))
		return;

	pfree(snapshot);
}

/*
 * FreeXactSnapshot
 *		Free snapshot(s) at end of transaction.
 */
void
FreeXactSnapshot(void)
{
	/*
	 * We do not free the xip arrays for the static snapshot structs; they
	 * will be reused soon. So this is now just a state change to prevent
	 * outside callers from accessing the snapshots.
	 */
	SerializableSnapshot = NULL;
	LatestSnapshot = NULL;
	ActiveSnapshot = NULL;		/* just for cleanliness */
}
