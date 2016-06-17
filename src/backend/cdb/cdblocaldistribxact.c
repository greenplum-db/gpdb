/*-------------------------------------------------------------------------
 *
 * cdblocaldistribxact.c
 *
 * Maintains state of current distributed transactions on each (local)
 * database instance.  Driven by added GP code in the xact.c module.
 *
 * Also support a cache of recently seen committed transactions found by the
 * visibility routines for better performance.  Used to avoid reading the
 * distributed log SLRU files too frequently.
 *
 * Copyright (c) 2007-2008, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "miscadmin.h"
#include "access/transam.h"
#include "access/twophase.h"
#include "cdb/cdblocaldistribxact.h"
#include "cdb/cdbvars.h"
#include "storage/proc.h"
#include "cdb/cdbshareddoublylinked.h"
#include "utils/hsearch.h"
#include "miscadmin.h"
#include "utils/memutils.h"
#include "cdb/cdbdoublylinked.h"
#include "cdb/cdbpersistentstore.h"

/*
 * Local information in the database instance (master or segment) on
 * a distributed transaction.
 */
typedef struct LocalDistribXactData
{
    /*
     * Number of outstanding LocalDistribXactRef
     * objects referencing this object.
     */
	int			referenceCount;

	/*
	 * The object index.  Useful for cross checking.
	 */
	int	index;

	/*
	 * All double links.
	 */
	SharedDoubleLinks	sortedLocalDoubleLinks;

	/*
	 * Current distributed transaction state.
	 */
	LocalDistribXactState		state;

	/*
	 * Distributed and local xids.
	 */
	DistributedTransactionTimeStamp	distribTimeStamp;
	DistributedTransactionId 		distribXid;
	TransactionId 					localXid;

} LocalDistribXactData;

typedef LocalDistribXactData* LocalDistribXact;

/*
 * The shared memory data structure containing shared global
 * information for this module plus all the LocalDistribXactData elements.
 */
typedef struct LocalDistribXactSharedData
{
	/*
	 * Sorted all elements list.
	 */
	SharedDoublyLinkedHead		sortedLocalList;

	/*
	 * Free list and shared list base data.
	 */
	int	maxElements;

	SharedDoublyLinkedHead		freeList;

	SharedListBase	sortedLocalBase;

	/*
	 * Keep so we can give an answer to gp_max_distributed_xid.
	 */
	DistributedTransactionId	maxDistributedTransactionId;

	/*
	 * LocalDistribXactData element data -- must be last.
	 */
	uint8	data[0];

} LocalDistribXactSharedData;

static LocalDistribXactSharedData* LocalDistribXactShared = NULL;

// *****************************************************************************

static char*
LocalDistribXactStateToString(LocalDistribXactState	state)
{
	switch (state)
	{
	case LOCALDISTRIBXACT_STATE_ACTIVE:
		return "Active";

	case LOCALDISTRIBXACT_STATE_COMMITDELIVERY:
		return "Commit Delivery";

	case LOCALDISTRIBXACT_STATE_COMMITTED:
		return "Committed";

	case LOCALDISTRIBXACT_STATE_ABORTDELIVERY:
		return "Abort Delivery";

	case LOCALDISTRIBXACT_STATE_ABORTED:
		return "Aborted";

	case LOCALDISTRIBXACT_STATE_PREPARED:
		return "Prepared";

	case LOCALDISTRIBXACT_STATE_COMMITPREPARED:
		return "Commit Prepared";

	case LOCALDISTRIBXACT_STATE_ABORTPREPARED:
		return "Abort Prepared";

	default:
		return "Unknown";
	}
}

void
LocalDistribXactRef_Init(
	LocalDistribXactRef	*ref)
{
	memcpy(ref->eyecatcher, LocalDistribXactRef_Eyecatcher, LocalDistribXactRef_EyecatcherLen);
	ref->index = -1;
}

bool
LocalDistribXactRef_IsNil(
	LocalDistribXactRef	*ref)
{
	if (strncmp(ref->eyecatcher, LocalDistribXactRef_Eyecatcher, LocalDistribXactRef_EyecatcherLen) != 0)
		elog(FATAL, "Distributed to local xact element not valid (eyecatcher)");
	return (ref->index == -1);
}

static void
LocalDistribXactRef_AssignMaster(
	LocalDistribXactRef	*ref,
	LocalDistribXact		ele)
{
	Assert(LocalDistribXactRef_IsNil(ref));
	Assert(ele->index >=0 );

	memcpy(ref->eyecatcher, LocalDistribXactRef_Eyecatcher, LocalDistribXactRef_EyecatcherLen);

	ref->index = ele->index;
	ele->referenceCount++;
}

static void
LocalDistribXactRef_AssignPgProc(
	LocalDistribXactRef	*ref,
	LocalDistribXact		ele)
{
	Assert(LocalDistribXactRef_IsNil(ref));
	Assert(ele->index >=0 );

	memcpy(ref->eyecatcher, LocalDistribXactRef_Eyecatcher, LocalDistribXactRef_EyecatcherLen);

	ref->index = ele->index;
	ele->referenceCount++;
}

static void
LocalDistribXactRef_AssignLocal(
	LocalDistribXactRef	*ref,
	LocalDistribXact		ele)
{
	Assert(LocalDistribXactRef_IsNil(ref));
	Assert(ele->index >=0 );

	memcpy(ref->eyecatcher, LocalDistribXactRef_Eyecatcher, LocalDistribXactRef_EyecatcherLen);

	ref->index = ele->index;
	ele->referenceCount++;
}


static void
LocalDistribXact_RemoveElement(
	LocalDistribXact	ele)
{
	Assert(ele != NULL);

	SharedDoubleLinks_Remove(
					&LocalDistribXactShared->sortedLocalBase,
					&LocalDistribXactShared->sortedLocalList,
					ele);
}

static void
LocalDistribXact_Reset(
	LocalDistribXact	ele)
{
	Assert(ele != NULL);
	Assert(ele->referenceCount == 0);

	ele->state = LOCALDISTRIBXACT_STATE_NONE;
	ele->distribXid = InvalidDistributedTransactionId;
	ele->localXid = InvalidTransactionId;
}

void
LocalDistribXactRef_ReleaseUnderLock(
	LocalDistribXactRef	*ref)
{
	LocalDistribXact ele;

	if (strncmp(ref->eyecatcher, LocalDistribXactRef_Eyecatcher, LocalDistribXactRef_EyecatcherLen) != 0)
		elog(FATAL, "Distributed to local xact element not valid");
	if (ref->index != -1)
	{
		ele = SharedListBase_ToElement(
							&LocalDistribXactShared->sortedLocalBase,
							ref->index);

		Assert(ele->referenceCount > 0);
		ele->referenceCount--;

		if (ele->referenceCount == 0)
		{
			LocalDistribXact_RemoveElement(ele);

			elog((Debug_print_full_dtm ? LOG : DEBUG5),
				 "Removing distributed transaction xid = %u (local xid = %u) in \"%s\" state for LocalDistribXact index %d",
				 ele->distribXid,
				 ele->localXid,
				 LocalDistribXactStateToString(ele->state),
				 ele->index);

			/*
			 * Return to free list.
			 */
			LocalDistribXact_Reset(ele);

			SharedDoublyLinkedHead_AddLast(
				&LocalDistribXactShared->sortedLocalBase,
				&LocalDistribXactShared->freeList,
				ele);
		}

		ref->index = -1;
	}
}

void
LocalDistribXactRef_Release(
	LocalDistribXactRef	*ref)
{
	LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

	LocalDistribXactRef_ReleaseUnderLock(ref);

	LWLockRelease(ProcArrayLock);
}

void
LocalDistribXactRef_Transfer(
	LocalDistribXactRef	*target,
	LocalDistribXactRef	*source)
{
	if (strncmp(target->eyecatcher, LocalDistribXactRef_Eyecatcher, LocalDistribXactRef_EyecatcherLen) != 0)
		elog(FATAL, "Distributed to local xact element not valid");
	Assert (target->index == -1);
	if (strncmp(source->eyecatcher, LocalDistribXactRef_Eyecatcher, LocalDistribXactRef_EyecatcherLen) != 0)
		elog(FATAL, "Distributed to local xact element not valid");
	Assert (source->index != -1);

	target->index = source->index;
	source->index = -1;
}

void
LocalDistribXactRef_Clone(
	LocalDistribXactRef	*clone,
	LocalDistribXactRef	*source)
{
	LocalDistribXact 	ele;

	if (strncmp(clone->eyecatcher, LocalDistribXactRef_Eyecatcher, LocalDistribXactRef_EyecatcherLen) != 0)
		elog(FATAL, "Distributed to local xact element not valid");
	Assert (clone->index == -1);
	if (strncmp(source->eyecatcher, LocalDistribXactRef_Eyecatcher, LocalDistribXactRef_EyecatcherLen) != 0)
		elog(FATAL, "Distributed to local xact element not valid");
	Assert (source->index != -1);

	LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

	ele = SharedListBase_ToElement(
						&LocalDistribXactShared->sortedLocalBase,
						source->index);

	Assert(ele->referenceCount > 0);
	ele->referenceCount++;

	clone->index = source->index;

	LWLockRelease(ProcArrayLock);
}

// *****************************************************************************

DistributedTransactionId
LocalDistribXact_GetMaxDistributedXid(void)
{
	return (LocalDistribXactShared == NULL ? 0 :
					LocalDistribXactShared->maxDistributedTransactionId);

}

static void
LocalDistribXact_SetLocalPgProc(
	LocalDistribXact	ele)
{
	Assert(LocalDistribXactShared != NULL);
	Assert (MyProc != NULL);

	MyProc->xid = ele->localXid;
	LocalDistribXactRef_AssignPgProc(
								&MyProc->localDistribXactRef,
								ele);
}

/*
 * NOTE: The ProcArrayLock must already be held.
 */
static LocalDistribXact
LocalDistribXact_New(
	DistributedTransactionTimeStamp	newDistribTimeStamp,
	DistributedTransactionId 		newDistribXid,
	TransactionId					newLocalXid,
	LocalDistribXactState			state)
{
	LocalDistribXact 	ele;

	Assert(newDistribTimeStamp != 0);
	Assert(newDistribXid != InvalidDistributedTransactionId);
	Assert(newLocalXid != InvalidTransactionId);

	ele = SharedDoublyLinkedHead_RemoveFirst(
					&LocalDistribXactShared->sortedLocalBase,
					&LocalDistribXactShared->freeList);
	if (ele == NULL)
		return NULL;

	ele->distribTimeStamp = newDistribTimeStamp;
	ele->distribXid = newDistribXid;
	ele->localXid = newLocalXid;

	ele->state = state;

	if (newDistribXid > LocalDistribXactShared->maxDistributedTransactionId)
		LocalDistribXactShared->maxDistributedTransactionId = newDistribXid;

	/*
	 * Since we are under the ProcArrayLock, we will get local xid in
	 * increasing order.
	 */
	SharedDoublyLinkedHead_AddLast(
					&LocalDistribXactShared->sortedLocalBase,
					&LocalDistribXactShared->sortedLocalList,
					ele);

	return ele;
}


/*
 * NOTE: The ProcArrayLock must already be held.
 */
static LocalDistribXact
LocalDistribXact_Start(
	DistributedTransactionTimeStamp	newDistribTimeStamp,
	DistributedTransactionId 		newDistribXid)
{
	TransactionId		localXid;
	LocalDistribXact 	ele;

	Assert(newDistribTimeStamp != 0);
	Assert(newDistribXid != InvalidDistributedTransactionId);

	localXid = GetNewTransactionId(false, false);
								// NOT subtrans, DO NOT Set PROC struct xid;

	ele = LocalDistribXact_New(
						newDistribTimeStamp,
						newDistribXid,
						localXid,
						LOCALDISTRIBXACT_STATE_ACTIVE);

	return ele;
}

/*
 * NOTE: The ProcArrayLock must already be held.
 */
void
LocalDistribXact_StartOnMaster(
	DistributedTransactionTimeStamp	newDistribTimeStamp,
	DistributedTransactionId 		newDistribXid,
	TransactionId					*newLocalXid,
	LocalDistribXactRef				*masterLocalDistribXactRef)
{
	LocalDistribXact 	ele;

	Assert(LocalDistribXactShared != NULL);
	Assert(newDistribTimeStamp != 0);
	Assert(newDistribXid != InvalidDistributedTransactionId);
	Assert(newLocalXid != NULL);
	Assert(masterLocalDistribXactRef != NULL);

	ele = LocalDistribXact_Start(
							newDistribTimeStamp,
							newDistribXid);
	if (ele == NULL)
	{
		elog(FATAL, "Out of distributed transaction to local elements");
	}

	LocalDistribXact_SetLocalPgProc(ele);

	*newLocalXid = ele->localXid;

	LocalDistribXactRef_AssignMaster(
							masterLocalDistribXactRef,
							ele);
}

void
LocalDistribXact_StartOnSegment(
	DistributedTransactionTimeStamp	newDistribTimeStamp,
	DistributedTransactionId 		newDistribXid,
	TransactionId					*newLocalXid)
{
	LocalDistribXact 	ele;

	MIRRORED_LOCK_DECLARE;

	Assert(LocalDistribXactShared != NULL);
	Assert(newDistribTimeStamp != 0);
	Assert(newDistribXid != InvalidDistributedTransactionId);
	Assert(newLocalXid != NULL);

	MIRRORED_LOCK;
	LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

	ele = LocalDistribXact_Start(
							newDistribTimeStamp,
							newDistribXid);
	if (ele == NULL)
	{
		LWLockRelease(ProcArrayLock);
		elog(FATAL, "Out of distributed transaction to local elements");
	}

	LocalDistribXact_SetLocalPgProc(ele);

	*newLocalXid = ele->localXid;

	LWLockRelease(ProcArrayLock);
	MIRRORED_UNLOCK;

}

void
LocalDistribXact_CreateRedoPrepared(
	DistributedTransactionTimeStamp	redoDistribTimeStamp,
	DistributedTransactionId 		redoDistribXid,
	TransactionId					redoLocalXid,
	LocalDistribXactRef				*localDistribXactRef)
{
	LocalDistribXact 	ele;

	Assert(LocalDistribXactShared != NULL);
	Assert(redoDistribTimeStamp != 0);
	Assert(redoDistribXid != InvalidTransactionId);


	LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

	ele = LocalDistribXact_New(
							redoDistribTimeStamp,
							redoDistribXid,
							redoLocalXid,
							LOCALDISTRIBXACT_STATE_PREPARED);
	if (ele == NULL)
	{
		LWLockRelease(ProcArrayLock);
		elog(FATAL, "Out of distributed transaction to local elements");
	}

	LocalDistribXactRef_Init(localDistribXactRef);
	LocalDistribXactRef_AssignLocal(
							localDistribXactRef,
							ele);

	LWLockRelease(ProcArrayLock);

}

// *****************************************************************************

static LocalDistribXact
LocalDistribXact_GetFromRef(
	TransactionId				localXid,
	LocalDistribXactRef			*localDistribXactRef)
{
	LocalDistribXact 	ele;

	Assert(LocalDistribXactShared != NULL);

	Assert(!LocalDistribXactRef_IsNil(localDistribXactRef));

	ele = SharedListBase_ToElement(
						&LocalDistribXactShared->sortedLocalBase,
						localDistribXactRef->index);
	if (ele == NULL)
		elog(FATAL, "Distributed transaction element not found for local xid = %u",
			 localXid);

	if (ele->localXid != localXid)
		elog(FATAL, "Local xid %u does not match expected local xid %u (distributed xid = %u)",
		     ele->localXid,
		     localXid,
		     ele->distribXid);

	return ele;
}

void
LocalDistribXact_ChangeStateUnderLock(
	TransactionId				localXid,
	LocalDistribXactRef			*localDistribXactRef,
	LocalDistribXactState		newState)
{
	LocalDistribXact 			ele;
	LocalDistribXactState		oldState;

	Assert(LocalDistribXactShared != NULL);

	Assert(!LocalDistribXactRef_IsNil(localDistribXactRef));

	ele = LocalDistribXact_GetFromRef(
								localXid,
								localDistribXactRef);

	/*
	 * Validate current state given new state.
	 */
	switch (newState)
	{
	case LOCALDISTRIBXACT_STATE_COMMITDELIVERY:
	case LOCALDISTRIBXACT_STATE_ABORTDELIVERY:
	case LOCALDISTRIBXACT_STATE_PREPARED:
		if (ele->state != LOCALDISTRIBXACT_STATE_ACTIVE)
			elog(PANIC,
			     "Expected distributed transaction xid = %u to local element to be in state \"Active\" and "
			     "found state \"%s\"",
			     ele->distribXid,
			     LocalDistribXactStateToString(ele->state));
		break;

	case LOCALDISTRIBXACT_STATE_COMMITPREPARED:
	case LOCALDISTRIBXACT_STATE_ABORTPREPARED:
		if (ele->state != LOCALDISTRIBXACT_STATE_PREPARED)
			elog(PANIC,
			     "Expected distributed transaction xid = %u to local element to be in state \"Prepared\" and "
			     "found state \"%s\"",
			     ele->distribXid,
			     LocalDistribXactStateToString(ele->state));
		break;

	case LOCALDISTRIBXACT_STATE_COMMITTED:
		if (ele->state != LOCALDISTRIBXACT_STATE_ACTIVE &&
			ele->state != LOCALDISTRIBXACT_STATE_COMMITDELIVERY)
			elog(PANIC,
			     "Expected distributed transaction xid = %u to local element to be in state \"Active\" or \"Commit Delivery\" and "
			     "found state \"%s\"",
			     ele->distribXid,
			     LocalDistribXactStateToString(ele->state));
		break;

	case LOCALDISTRIBXACT_STATE_ABORTED:
		if (ele->state != LOCALDISTRIBXACT_STATE_ACTIVE &&
			ele->state != LOCALDISTRIBXACT_STATE_ABORTDELIVERY)
			elog(PANIC,
			     "Expected distributed transaction xid = %u to local element to be in state \"Active\" or \"Abort Delivery\" and "
			     "found state \"%s\"",
			     ele->distribXid,
			     LocalDistribXactStateToString(ele->state));
		break;

	case LOCALDISTRIBXACT_STATE_ACTIVE:
		elog(PANIC, "Unexpected distributed to local transaction new state: '%s'",
			LocalDistribXactStateToString(newState));
		break;

	default:
		elog(PANIC, "Unrecognized distributed to local transaction state: %d",
			(int) newState);
	}

	oldState = ele->state;
	ele->state = newState;

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "Moved distributed transaction xid = %u (local xid = %u) from \"%s\" to \"%s\" for LocalDistribXact index %d",
		 ele->distribXid,
		 ele->localXid,
		 LocalDistribXactStateToString(oldState),
		 LocalDistribXactStateToString(newState),
		 ele->index);
}

void
LocalDistribXact_ChangeState(
	TransactionId				localXid,
	LocalDistribXactRef			*localDistribXactRef,
	LocalDistribXactState		newState)
{
	LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);

	LocalDistribXact_ChangeStateUnderLock(
										localXid,
										localDistribXactRef,
										newState);

	LWLockRelease(ProcArrayLock);
}

#define MAX_LOCAL_DISTRIB_DISPLAY_BUFFER 100
static char LocalDistribDisplayBuffer[MAX_LOCAL_DISTRIB_DISPLAY_BUFFER];

char*
LocalDistribXact_DisplayString(
	LocalDistribXactRef		*localDistribXactRef)
{
	LocalDistribXact 	ele;
	int 				snprintfResult;

	if (LocalDistribXactRef_IsNil(localDistribXactRef))
		return "distributed transaction is nil";

	ele = SharedListBase_ToElement(
						&LocalDistribXactShared->sortedLocalBase,
						localDistribXactRef->index);
	snprintfResult =
		snprintf(
			LocalDistribDisplayBuffer,
			MAX_LOCAL_DISTRIB_DISPLAY_BUFFER,
		    "distributed transaction {timestamp %u, xid %u} for local xid %u",
		    ele->distribTimeStamp,
		    ele->distribXid,
		    ele->localXid);

	Assert(snprintfResult >= 0);
	Assert(snprintfResult < MAX_LOCAL_DISTRIB_DISPLAY_BUFFER);

	return LocalDistribDisplayBuffer;
}

// *****************************************************************************

static int
LocalDistribXact_MaxElements(void)
{
	// UNDONE: Figure out a good safe number and one place to calculate...
	return 3 * (MaxBackends + max_prepared_xacts);
}

/*
 * Report shared-memory space needed by CreateSharedLocalDistribXact.
 */
Size
LocalDistribXact_ShmemSize(void)
{
	Size		size;
	int			count;

	count = LocalDistribXact_MaxElements();

	size = offsetof(LocalDistribXactSharedData, data);
	size = add_size(size, mul_size(sizeof(LocalDistribXactData),count));

	elog((Debug_print_full_dtm ? LOG : DEBUG5),
		 "LocalDistribXactShmemSize return %d bytes",
		 (int)size);

	return size;
}

/*
 * Initialize the shared LocalDistribXact array during postmaster startup.
 */
void
LocalDistribXact_ShmemCreate(void)
{
	bool		found;

	/* Create or attach to the LocalDistribXactShared shared structure */
	LocalDistribXactShared = (LocalDistribXactSharedData *)
		ShmemInitStruct("LocalDistrib", LocalDistribXact_ShmemSize(), &found);

	if (!found)
	{
		int i;
		int maxElements;

		/*
		 * We're the first - initialize.
		 */

		SharedDoublyLinkedHead_Init(&LocalDistribXactShared->sortedLocalList);

		SharedListBase_Init(
					&LocalDistribXactShared->sortedLocalBase,
					&LocalDistribXactShared->data,
					sizeof(LocalDistribXactData),
					offsetof(LocalDistribXactData, sortedLocalDoubleLinks));

		maxElements = LocalDistribXact_MaxElements();
		LocalDistribXactShared->maxElements = maxElements;

		SharedDoublyLinkedHead_Init(&LocalDistribXactShared->freeList);

		LocalDistribXactShared->maxDistributedTransactionId = FirstDistributedTransactionId;

		/*
		 * Initalize the array of LocalDistribXact structs.
		 */
		for (i = 0; i < maxElements; i++)
		{
			LocalDistribXact ele;

			ele = (LocalDistribXact)
						SharedListBase_ToElement(
								&LocalDistribXactShared->sortedLocalBase, i);

			ele->referenceCount = 0;

			ele->index = i;

			SharedDoubleLinks_Init(
				&ele->sortedLocalDoubleLinks, i);

			ele->distribXid = InvalidDistributedTransactionId;
			ele->localXid = InvalidTransactionId;

			SharedDoublyLinkedHead_AddLast(
				&LocalDistribXactShared->sortedLocalBase,
				&LocalDistribXactShared->freeList,
				ele);
		}

		elog((Debug_print_full_dtm ? LOG : DEBUG5),
			 "Initialized %d LocalDistribXact elements",
			 maxElements);
	}
}

// *****************************************************************************

/* Memory context for long-lived local-distributed commit pairs. */
static MemoryContext LocalDistribCacheMemCxt = NULL;

/* Hash table for the long-lived local-distributed commit pairs. */
static HTAB	   		*LocalDistribCacheHtab;

/*
 * A cached local-distributed transaction pair.
 *
 * We also cache just local-only transactions, so in that case distribXid
 * will be InvalidDistributedTransactionId.
 */
typedef struct LocalDistribXactCacheEntry
{
	/*
	 * Distributed and local xids.
	 */
	TransactionId 					localXid;
										/* MUST BE FIRST: Hash table key. */

	DistributedTransactionId 		distribXid;

	int64							visits;

	DoubleLinks						lruDoubleLinks;
										/* list link for LRU */

}	LocalDistribXactCacheEntry;

/*
 * Globals for local-distributed cache.
 */
static struct LocalDistribXactCache
{
	int32			count;

	DoublyLinkedHead 		lruDoublyLinkedHead;

	int64		hitCount;
	int64		totalCount;
	int64		addCount;
	int64		removeCount;

}	LocalDistribXactCache = {0,{NULL,NULL},0,0,0,0};


bool
LocalDistribXactCache_CommittedFind(
	TransactionId						localXid,
	DistributedTransactionTimeStamp		distribTransactionTimeStamp,
	DistributedTransactionId			*distribXid)
{
	LocalDistribXactCacheEntry*	entry;
	bool						found;

	// Before doing anything, see if we are enabled.
	if (gp_max_local_distributed_cache == 0)
		return false;

	if (LocalDistribCacheMemCxt == NULL)
	{
		HASHCTL		hash_ctl;

		/* Create the memory context where cross-transaction state is stored */
		LocalDistribCacheMemCxt = AllocSetContextCreate(TopMemoryContext,
											  "Local-distributed commit cache context",
											  ALLOCSET_DEFAULT_MINSIZE,
											  ALLOCSET_DEFAULT_INITSIZE,
											  ALLOCSET_DEFAULT_MAXSIZE);

		Assert(LocalDistribCacheHtab == NULL);

		/* Create the hashtable proper */
		MemSet(&hash_ctl, 0, sizeof(hash_ctl));
		hash_ctl.keysize = sizeof(TransactionId);
		hash_ctl.entrysize = sizeof(LocalDistribXactCacheEntry);
		hash_ctl.hash = tag_hash;
		hash_ctl.hcxt = LocalDistribCacheMemCxt;
		LocalDistribCacheHtab = hash_create("Local-distributed commit cache",
									 25,	/* start small and extend */
									 &hash_ctl,
									 HASH_ELEM | HASH_FUNCTION | HASH_CONTEXT);

		MemSet(&LocalDistribXactCache, 0, sizeof(LocalDistribXactCache));
		DoublyLinkedHead_Init(&LocalDistribXactCache.lruDoublyLinkedHead);

	}

	entry = (LocalDistribXactCacheEntry*) hash_search(
													LocalDistribCacheHtab,
													&localXid,
													HASH_FIND,
													&found);

	if (found)
	{
		/*
		 * Maintain LRU ordering.
		 */
		DoubleLinks_Remove(
					offsetof(LocalDistribXactCacheEntry, lruDoubleLinks),
					&LocalDistribXactCache.lruDoublyLinkedHead,
					entry);
		DoublyLinkedHead_AddFirst(
						offsetof(LocalDistribXactCacheEntry, lruDoubleLinks),
						&LocalDistribXactCache.lruDoublyLinkedHead,
						entry);

		*distribXid = entry->distribXid;

		entry->visits++;

		LocalDistribXactCache.hitCount++;
	}

	LocalDistribXactCache.totalCount++;

	return found;
}

void
LocalDistribXactCache_AddCommitted(
	TransactionId						localXid,
	DistributedTransactionTimeStamp		distribTransactionTimeStamp,
	DistributedTransactionId			distribXid)
{
	LocalDistribXactCacheEntry*	entry;
	bool						found;

	// Before doing anything, see if we are enabled.
	if (gp_max_local_distributed_cache == 0)
		return;

	Assert (LocalDistribCacheMemCxt != NULL);
	Assert (LocalDistribCacheHtab != NULL);

	if (LocalDistribXactCache.count >= gp_max_local_distributed_cache)
	{
		LocalDistribXactCacheEntry*	lastEntry;
		LocalDistribXactCacheEntry*	removedEntry;

		Assert(LocalDistribXactCache.count == gp_max_local_distributed_cache);

		/*
		 * Remove oldest.
		 */
		lastEntry = (LocalDistribXactCacheEntry*)
							DoublyLinkedHead_RemoveLast(
											offsetof(LocalDistribXactCacheEntry, lruDoubleLinks),
											&LocalDistribXactCache.lruDoublyLinkedHead);
		Assert(lastEntry != NULL);

		removedEntry = (LocalDistribXactCacheEntry*)
			hash_search(LocalDistribCacheHtab, &lastEntry->localXid,
						HASH_REMOVE, NULL);
		Assert(lastEntry == removedEntry);

		LocalDistribXactCache.count--;

		LocalDistribXactCache.removeCount++;
	}

	/* Now we can add entry to hash table */
	entry = (LocalDistribXactCacheEntry*) hash_search(
													LocalDistribCacheHtab,
													&localXid,
													HASH_ENTER,
													&found);
	if (found)
	{
		elog(ERROR, "Add should not have found local xid = %x", localXid);
	}

	DoubleLinks_Init(&entry->lruDoubleLinks);
	DoublyLinkedHead_AddFirst(
					offsetof(LocalDistribXactCacheEntry, lruDoubleLinks),
					&LocalDistribXactCache.lruDoublyLinkedHead,
					entry);

	entry->localXid = localXid;
	entry->distribXid = distribXid;
	entry->visits = 1;

	LocalDistribXactCache.count++;

	LocalDistribXactCache.addCount++;

}

void
LocalDistribXactCache_ShowStats(char *nameStr)
{
		elog(LOG, "%s: Local-distributed cache counts "
			 "(hits " INT64_FORMAT ", total " INT64_FORMAT ", adds " INT64_FORMAT ", removes " INT64_FORMAT ")",
			 nameStr,
			 LocalDistribXactCache.hitCount,
			 LocalDistribXactCache.totalCount,
			 LocalDistribXactCache.addCount,
			 LocalDistribXactCache.removeCount);
}

void
LocalDistribXact_GetDistributedXid(
	TransactionId					localXid,
	LocalDistribXactRef				*localDistribXactRef,
	DistributedTransactionTimeStamp *distribTimeStamp,
	DistributedTransactionId 		*distribXid)
{
	LocalDistribXact 			ele;

	Assert(LocalDistribXactShared != NULL);

	Assert(!LocalDistribXactRef_IsNil(localDistribXactRef));
	Assert(distribTimeStamp != NULL);
	Assert(distribXid != NULL);

	ele = LocalDistribXact_GetFromRef(
								localXid,
								localDistribXactRef);

	*distribTimeStamp = ele->distribTimeStamp;
	*distribXid = ele->distribXid;

}
