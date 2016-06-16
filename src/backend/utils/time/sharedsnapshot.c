/*-------------------------------------------------------------------------
 *
 * sharedsnapshot.c
 *	  GPDB shared snapshot management.
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/time/sharedsnapshot.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/distributedlog.h"
#include "access/twophase.h"  /*max_prepared_xacts*/
#include "cdb/cdbtm.h"
#include "cdb/cdbvars.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "lib/stringinfo.h"
#include "storage/buffile.h"
#include "storage/procarray.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/sharedsnapshot.h"

/*
 * Distributed Snapshot that gets sent in from the QD to processes running
 * in EXECUTE mode.
 */
DtxContext DistributedTransactionContext = DTX_CONTEXT_LOCAL_ONLY;

DtxContextInfo QEDtxContextInfo = DtxContextInfo_StaticInit;

/* MPP Shared Snapshot. */
typedef struct SharedSnapshotStruct
{
	int 		numSlots;		/* number of valid Snapshot entries */
	int			maxSlots;		/* allocated size of sharedSnapshotArray */
	int 		nextSlot;		/* points to the next avail slot. */

	/*
	 * We now allow direct indexing into this array.
	 *
	 * We allocate the XIPS below.
	 *
	 * Be very careful when accessing fields inside here.
	 */
	SharedSnapshotSlot	   *slots;

	TransactionId	   *xips;		/* VARIABLE LENGTH ARRAY */
} SharedSnapshotStruct;

static volatile SharedSnapshotStruct *sharedSnapshotArray;

volatile SharedSnapshotSlot *SharedLocalSnapshotSlot = NULL;

static Size slotSize = 0;
static Size slotCount = 0;
static Size xipEntryCount = 0;


/*
 * File references to shared snapshot files open in this transaction.
 */
static List *shared_snapshot_files = NIL;

/* prototypes for internal functions */
static SharedSnapshotSlot *SharedSnapshotAdd(int4 slotId);
static SharedSnapshotSlot *SharedSnapshotLookup(int4 slotId);

/*
 * Report shared-memory space needed by CreateSharedSnapshot.
 */
Size
SharedSnapshotShmemSize(void)
{
	Size		size;

	xipEntryCount = MaxBackends + max_prepared_xacts;

	slotSize = sizeof(SharedSnapshotSlot);
	slotSize += mul_size(sizeof(TransactionId), (xipEntryCount));
	slotSize = MAXALIGN(slotSize);

	/*
	 * We only really need max_prepared_xacts; but for safety we
	 * multiply that by two (to account for slow de-allocation on
	 * cleanup, for instance).
	 */
	slotCount = 2 * max_prepared_xacts;

	size = offsetof(SharedSnapshotStruct, xips);
	size = add_size(size, mul_size(slotSize, slotCount));

	return MAXALIGN(size);
}

/*
 * Initialize the sharedSnapshot array.  This array is used to communicate
 * snapshots between qExecs that are segmates.
 */
void
CreateSharedSnapshotArray(void)
{
	bool	found;
	int		i;
	TransactionId *xip_base=NULL;

	/* Create or attach to the SharedSnapshot shared structure */
	sharedSnapshotArray = (SharedSnapshotStruct *)
		ShmemInitStruct("Shared Snapshot", SharedSnapshotShmemSize(), &found);

	Assert(slotCount != 0);
	Assert(xipEntryCount != 0);

	if (!found)
	{
		/*
		 * We're the first - initialize.
		 */
		sharedSnapshotArray->numSlots = 0;

		/* TODO:  MaxBackends is only somewhat right.  What we really want here
		 *        is the MaxBackends value from the QD.  But this is at least
		 *		  safe since we know we dont need *MORE* than MaxBackends.  But
		 *        in general MaxBackends on a QE is going to be bigger than on a
		 *		  QE by a good bit.  or at least it should be.
		 *
		 * But really, max_prepared_transactions *is* what we want (it
		 * corresponds to the number of connections allowed on the
		 * master).
		 *
		 * slotCount is initialized in SharedSnapshotShmemSize().
		 */
		sharedSnapshotArray->maxSlots = slotCount;
		sharedSnapshotArray->nextSlot = 0;

		sharedSnapshotArray->slots = (SharedSnapshotSlot *)&sharedSnapshotArray->xips;

		/* xips start just after the last slot structure */
		xip_base = (TransactionId *)&sharedSnapshotArray->slots[sharedSnapshotArray->maxSlots];

		for (i=0; i < sharedSnapshotArray->maxSlots; i++)
		{
			SharedSnapshotSlot *tmpSlot = &sharedSnapshotArray->slots[i];

			tmpSlot->slotid = -1;
			tmpSlot->slotindex = i;

			/*
			 * Fixup xip array pointer reference space allocated after slot structs:
			 *
			 * Note: xipEntryCount is initialized in SharedSnapshotShmemSize().
			 * So each slot gets (MaxBackends + max_prepared_xacts) transaction-ids.
			 */
			tmpSlot->snapshot.xip = &xip_base[xipEntryCount];
			xip_base += xipEntryCount;
		}
	}

	shared_snapshot_files = NIL;
}

/*
 * Used to dump the internal state of the shared slots for debugging.
 */
char *
SharedSnapshotDump(void)
{
	StringInfoData str;
	volatile SharedSnapshotStruct *arrayP = sharedSnapshotArray;
	int			index;

	initStringInfo(&str);

	appendStringInfo(&str, "Local SharedSnapshot Slot Dump: currSlots: %d maxSlots: %d ",
					 arrayP->numSlots, arrayP->maxSlots);

	LWLockAcquire(SharedSnapshotLock, LW_EXCLUSIVE);

	for (index=0; index < arrayP->maxSlots; index++)
	{
		/* need to do byte addressing to find the right slot */
		SharedSnapshotSlot *testSlot = &arrayP->slots[index];

		if (testSlot->slotid != -1)
		{
			appendStringInfo(&str, "(SLOT index: %d slotid: %d QDxid: %u QDcid: %u pid: %u)",
							 testSlot->slotindex, testSlot->slotid, testSlot->QDxid, testSlot->QDcid, (int)testSlot->pid);
		}

	}

	LWLockRelease(SharedSnapshotLock);

	return str.data;
}

/* Acquires an available slot in the sharedSnapshotArray.  The slot is then
 * marked with the supplied slotId.  This slotId is what others will use to
 * find this slot.  This should only ever be called by the "writer" qExec.
 *
 * The slotId should be something that is unique amongst all the possible
 * "writer" qExecs active on a segment database at a given moment.  It also
 * will need to be communicated to the "reader" qExecs so that they can find
 * this slot.
 */
static SharedSnapshotSlot *
SharedSnapshotAdd(int4 slotId)
{
	SharedSnapshotSlot *slot;
	volatile SharedSnapshotStruct *arrayP = sharedSnapshotArray;
	int nextSlot = -1;
	int i;
	int retryCount = gp_snapshotadd_timeout * 10; /* .1 s per wait */

retry:
	LWLockAcquire(SharedSnapshotLock, LW_EXCLUSIVE);

	slot = NULL;

	for (i=0; i < arrayP->maxSlots; i++)
	{
		SharedSnapshotSlot *testSlot = &arrayP->slots[i];

		if (testSlot->slotindex > arrayP->maxSlots)
			elog(ERROR, "Shared Local Snapshots Array appears corrupted: %s", SharedSnapshotDump());

		if (testSlot->slotid == slotId)
		{
			slot = testSlot;
			break;
		}
	}

	if (slot != NULL)
	{
		elog(DEBUG1, "SharedSnapshotAdd: found existing entry for our session-id. id %d retry %d pid %u", slotId, retryCount, (int)slot->pid);
		LWLockRelease(SharedSnapshotLock);

		if (retryCount > 0)
		{
			retryCount--;

			pg_usleep(100000); /* 100ms, wait gp_snapshotadd_timeout seconds max. */
			goto retry;
		}
		else
		{
			insist_log(false, "writer segworker group shared snapshot collision on id %d", slotId);
		}
		/* not reached */
	}

	if (arrayP->numSlots >= arrayP->maxSlots || arrayP->nextSlot == -1)
	{
		/*
		 * Ooops, no room.  this shouldn't happen as something else should have
		 * complained if we go over MaxBackends.
		 */
		LWLockRelease(SharedSnapshotLock);
		ereport(FATAL,
				(errcode(ERRCODE_TOO_MANY_CONNECTIONS),
				 errmsg("sorry, too many clients already."),
				 errdetail("There are no more available slots in the sharedSnapshotArray."),
				 errhint("Another piece of code should have detected that we have too many clients."
						 " this probably means that someone isn't releasing their slot properly.")));
	}

	slot = &arrayP->slots[arrayP->nextSlot];

	slot->slotindex = arrayP->nextSlot;

	/*
	 * find the next available slot
	 */
	for (i=arrayP->nextSlot+1; i < arrayP->maxSlots; i++)
	{
		SharedSnapshotSlot *tmpSlot = &arrayP->slots[i];

		if (tmpSlot->slotid == -1)
		{
			nextSlot = i;
			break;
		}
	}

	/* mark that there isn't a nextSlot if the above loop didn't find one */
	if (nextSlot == arrayP->nextSlot)
		arrayP->nextSlot = -1;
	else
		arrayP->nextSlot = nextSlot;

	arrayP->numSlots += 1;

	/* initialize some things */
	slot->slotid = slotId;
	slot->xid = 0;
	slot->pid = 0;
	slot->cid = 0;
	slot->startTimestamp = 0;
	slot->QDxid = 0;
	slot->QDcid = 0;
	slot->segmateSync = 0;

	LWLockRelease(SharedSnapshotLock);

	return slot;
}

void
GetSlotTableDebugInfo(void **snapshotArray, int *maxSlots)
{
	*snapshotArray = (void *)sharedSnapshotArray;
	*maxSlots = sharedSnapshotArray->maxSlots;
}

/*
 * Used by "reader" qExecs to find the slot in the sharedsnapshotArray with the
 * specified slotId.  In general, we should always be able to find the specified
 * slot unless something unexpected.  If the slot is not found, then NULL is
 * returned.
 *
 * MPP-4599: retry in the same pattern as the writer.
 */
static SharedSnapshotSlot *
SharedSnapshotLookup(int4 slotId)
{
	SharedSnapshotSlot *slot = NULL;
	volatile SharedSnapshotStruct *arrayP = sharedSnapshotArray;
	int retryCount = gp_snapshotadd_timeout * 10; /* .1 s per wait */
	int index;

	for (;;)
	{
		CHECK_FOR_INTERRUPTS();

		LWLockAcquire(SharedSnapshotLock, LW_SHARED);

		for (index=0; index < arrayP->maxSlots; index++)
		{
			SharedSnapshotSlot *testSlot;

			testSlot = &arrayP->slots[index];

			if (testSlot->slotindex > arrayP->maxSlots)
			{
				LWLockRelease(SharedSnapshotLock);
				elog(ERROR, "Shared Local Snapshots Array appears corrupted: %s", SharedSnapshotDump());
			}

			if (testSlot->slotid == slotId)
			{
				slot = testSlot;
				break;
			}
		}

		LWLockRelease(SharedSnapshotLock);

		if (slot != NULL)
		{
			break;
		}
		else
		{
			if (retryCount > 0)
			{
				retryCount--;

				pg_usleep(100000); /* 100ms, wait gp_snapshotadd_timeout seconds max. */
			}
			else
			{
				break;
			}
		}
	}

	return slot;
}


/*
 * Used by the "writer" qExec to "release" the slot it had been using.
 *
 */
void
SharedSnapshotRemove(volatile SharedSnapshotSlot *slot, char *creatorDescription)
{
	int slotId = slot->slotid;

	LWLockAcquire(SharedSnapshotLock, LW_EXCLUSIVE);

	/* determine if we need to modify the next available slot to use.  we
	 * only do this is our slotindex is lower then the existing one.
	 */
	if (sharedSnapshotArray->nextSlot == -1 || slot->slotindex < sharedSnapshotArray->nextSlot)
	{
		if (slot->slotindex > sharedSnapshotArray->maxSlots)
			elog(ERROR, "Shared Local Snapshots slot has a bogus slotindex: %d. slot array dump: %s",
				 slot->slotindex, SharedSnapshotDump());

		sharedSnapshotArray->nextSlot = slot->slotindex;
	}

	/* reset the slotid which marks it as being unused. */
	slot->slotid = -1;
	slot->xid = 0;
	slot->pid = 0;
	slot->cid = 0;
	slot->startTimestamp = 0;
	slot->QDxid = 0;
	slot->QDcid = 0;
	slot->segmateSync = 0;

	sharedSnapshotArray->numSlots -= 1;

	LWLockRelease(SharedSnapshotLock);

	elog((Debug_print_full_dtm ? LOG : DEBUG5),"SharedSnapshotRemove removed slot for slotId = %d, creator = %s (address %p)",
		 slotId, creatorDescription, SharedLocalSnapshotSlot);
}

void
addSharedSnapshot(char *creatorDescription, int id)
{
	SharedSnapshotSlot *slot;

	slot = SharedSnapshotAdd(id);

	if (slot==NULL)
	{
		ereport(ERROR,
				(errmsg("%s could not set the Shared Local Snapshot!",
						creatorDescription),
				 errdetail("Tried to set the shared local snapshot slot with id: %d "
						   "and failed. Shared Local Snapshots dump: %s", id,
						   SharedSnapshotDump())));
	}
	SharedLocalSnapshotSlot = slot;

	elog((Debug_print_full_dtm ? LOG : DEBUG5),"%s added Shared Local Snapshot slot for gp_session_id = %d (address %p)",
		 creatorDescription, id, SharedLocalSnapshotSlot);
}

void
lookupSharedSnapshot(char *lookerDescription, char *creatorDescription, int id)
{
	SharedSnapshotSlot *slot;

	slot = SharedSnapshotLookup(id);

	if (slot == NULL)
	{
		ereport(ERROR,
				(errmsg("%s could not find Shared Local Snapshot!",
						lookerDescription),
				 errdetail("Tried to find a shared snapshot slot with id: %d "
						   "and found none. Shared Local Snapshots dump: %s", id,
						   SharedSnapshotDump()),
				 errhint("Either this %s was created before the %s or the %s died.",
						 lookerDescription, creatorDescription, creatorDescription)));
	}
	SharedLocalSnapshotSlot = slot;

	elog((Debug_print_full_dtm ? LOG : DEBUG5),"%s found Shared Local Snapshot slot for gp_session_id = %d created by %s (address %p)",
		 lookerDescription, id, creatorDescription, SharedLocalSnapshotSlot);
}

static char *
sharedLocalSnapshot_filename(TransactionId xid, CommandId cid, uint32 segmateSync)
{
	int pid;
	static char filename[MAXPGPATH];
	
	if (Gp_is_writer)
	{
		pid = MyProc->pid;
	}
	else
	{
		if (lockHolderProcPtr == NULL)
		{
			/* get lockholder! */
			elog(ERROR, "NO LOCK HOLDER POINTER.");
		}
		pid = lockHolderProcPtr->pid;
	}

	snprintf(filename, sizeof(filename), "sess%u_w%u_qdxid%u_qdcid%u_sync%u",
			 gp_session_id, pid, xid, cid, segmateSync);
	return filename;
}

/*
 * Dump the shared local snapshot, so that the readers can pick it up.
 *
 * BufFileCreateTemp_ReaderWriter(filename, iswriter)
 */
void
dumpSharedLocalSnapshot_forCursor(void)
{
	SharedSnapshotSlot *src = NULL;
	char* fname = NULL;
	BufFile *f = NULL;
	Size count=0;
	TransactionId *xids = NULL;
	int64 sub_size;
	int64 size_read;
	ResourceOwner oldowner;
	MemoryContext oldcontext;

	Assert(Gp_role == GP_ROLE_DISPATCH || (Gp_role == GP_ROLE_EXECUTE && Gp_is_writer));
	Assert(SharedLocalSnapshotSlot != NULL);

	src = (SharedSnapshotSlot *)SharedLocalSnapshotSlot;
	fname = sharedLocalSnapshot_filename(src->QDxid, src->QDcid, src->segmateSync);

	/*
	 * Create our dump-file. Hold the reference to it in
	 * the transaction's resource owner, so that it lives as long
	 * as the cursor we're declaring.
	 */
	oldowner = CurrentResourceOwner;
	CurrentResourceOwner = TopTransactionResourceOwner;
	oldcontext = MemoryContextSwitchTo(TopTransactionContext);
	f = BufFileCreateTemp_ReaderWriter(fname, true, false);

	/*
	 * Remember our file, so that we can close it at end of transaction.
	 * The resource owner mechanism would do it for us as a backstop, but it
	 * produces warnings at commit if some files haven't been closed.
	 */
	shared_snapshot_files = lappend(shared_snapshot_files, f);
	MemoryContextSwitchTo(oldcontext);
	CurrentResourceOwner = oldowner;

	/* we have our file. */

#define FileWriteOK(file, ptr, size) (BufFileWrite(file, ptr, size) == size)

#define FileWriteFieldWithCount(count, file, field) \
    if (BufFileWrite((file), &(field), sizeof(field)) != sizeof(field)) break; \
    count += sizeof(field);

	do
	{
		/* Write our length as zero. (we'll fix it later). */
		count = 0;

		/*
		 * We write two counts here: One is count of first part,
		 * second is size of subtransaction xids copied from
		 * SharedLocalSnapshotSlot. This can be a big number.
		 */
		FileWriteFieldWithCount(count, f, count);
		FileWriteFieldWithCount(count, f, src->total_subcnt);

		FileWriteFieldWithCount(count, f, src->pid);
		FileWriteFieldWithCount(count, f, src->xid);
		FileWriteFieldWithCount(count, f, src->cid);
		FileWriteFieldWithCount(count, f, src->startTimestamp);

		FileWriteFieldWithCount(count, f, src->combocidcnt);
		FileWriteFieldWithCount(count, f, src->combocids);
		FileWriteFieldWithCount(count, f, src->snapshot.xmin);
		FileWriteFieldWithCount(count, f, src->snapshot.xmax);
		FileWriteFieldWithCount(count, f, src->snapshot.xcnt);

		if (!FileWriteOK(f, &src->snapshot.xip, src->snapshot.xcnt * sizeof(TransactionId)))
			break;
		count += src->snapshot.xcnt * sizeof(TransactionId);

		FileWriteFieldWithCount(count, f, src->snapshot.curcid);

		/*
		 * THE STUFF IN THE SHARED LOCAL VERSION OF
		 * snapshot.distribSnapshotWithLocalMapping
		 * APPEARS TO *NEVER* BE USED, SO THERE IS
		 * NO POINT IN TRYING TO DUMP IT (IN FACT,
		 * IT'S ALLOCATION STRATEGY ISN'T SHMEM-FRIENDLY).
		 */

		/*
		 * THIS STUFF IS USED IN THE FILENAME
		 * SO THE READER ALREADY HAS IT.
		 *

		 dst->QDcid = src->QDcid;
		 dst->segmateSync = src->segmateSync;
		 dst->QDxid = src->QDxid;
		 dst->ready = src->ready;

		 *
		 */

		if (src->total_subcnt > src->inmemory_subcnt)
		{
			Assert(subxip_file != 0);

			xids = palloc(MAX_XIDBUF_SIZE);

			FileSeek(subxip_file, 0, SEEK_SET);
			sub_size = (src->total_subcnt - src->inmemory_subcnt)
				    * sizeof(TransactionId);
			while (sub_size > 0)
			{
				size_read = (sub_size > MAX_XIDBUF_SIZE) ?
						MAX_XIDBUF_SIZE : sub_size;
				if (size_read != FileRead(subxip_file, (char *)xids,
							  size_read))
				{
					elog(ERROR,
					     "Error in reading subtransaction file.");
				}

				if (!FileWriteOK(f, xids, sub_size))
				{
					break;
				}

				sub_size -= size_read;
			}

			pfree(xids);
			if (sub_size != 0)
				break;
		}

		if (src->inmemory_subcnt > 0)
		{
			sub_size = src->inmemory_subcnt * sizeof(TransactionId);
			if (!FileWriteOK(f, src->subxids, sub_size))
			{
				break;
			}
		}

		/*
		 * Now update our length field: seek to beginning and overwrite
		 * our original zero-length. count does not include
		 * subtransaction ids.
		 */
		if (BufFileSeek(f, 0 /* offset */, SEEK_SET) != 0)
			break;

		if (!FileWriteOK(f, &count, sizeof(count)))
			break;

		/* now flush and close. */
		BufFileFlush(f);
		/*
		 * Temp files get deleted on close!
		 *
		 * BufFileClose(f);
		 */

		return;
	}
	while (0);

	elog(ERROR, "Failed to write shared snapshot to temp-file");
}

void
readSharedLocalSnapshot_forCursor(Snapshot snapshot)
{
	BufFile *f;
	char *fname=NULL;
	Size count=0, sanity;
	uint8 *p, *buffer=NULL;

	pid_t writerPid;
	TransactionId localXid;
	CommandId localCid;
	TimestampTz localXactStartTimestamp;

	uint32 combocidcnt;
	ComboCidKeyData tmp_combocids[MaxComboCids];
	uint32 sub_size;
	uint32 read_size;
	int64 subcnt;
	TransactionId *subxids = NULL;

	Assert(Gp_role == GP_ROLE_EXECUTE);
	Assert(!Gp_is_writer);
	Assert(SharedLocalSnapshotSlot != NULL);
	Assert(snapshot->xip != NULL);
	Assert(snapshot->subxip != NULL);

	/*
	 * Open our dump-file, this will either return a valid file, or
	 * throw an error.
	 *
	 * NOTE: this is always run *after* the dump by the writer is
	 * guaranteed to have completed.
	 */
	fname = sharedLocalSnapshot_filename(QEDtxContextInfo.distributedXid,
		QEDtxContextInfo.curcid, QEDtxContextInfo.segmateSync);

	f = BufFileCreateTemp_ReaderWriter(fname, false, false);
	/* we have our file. */

#define FileReadOK(file, ptr, size) (BufFileRead(file, ptr, size) == size)

	/* Read the file-length info */
	if (!FileReadOK(f, &count, sizeof(count)))
		elog(ERROR, "Cursor snapshot: failed to read size");

	elog(DEBUG1, "Reading in cursor-snapshot %u bytes",
		     (unsigned int)count);

	buffer = palloc(count);

	/*
	 * Seek back to the beginning:
	 * We're going to read this all in one go, the size
	 * of this buffer should be more than a few hundred bytes.
	 */
	if (BufFileSeek(f, 0 /* offset */, SEEK_SET) != 0)
		elog(ERROR, "Cursor snapshot: failed to seek.");

	if (!FileReadOK(f, buffer, count))
		elog(ERROR, "Cursor snapshot: failed to read content");

	/* we've got the entire snapshot read into our buffer. */
	p = buffer;

	/* sanity check count */
	memcpy(&sanity, p, sizeof(sanity));
	if (sanity != count)
		elog(ERROR, "cursor snapshot failed sanity %u != %u",
			    (unsigned int)sanity, (unsigned int)count);
	p += sizeof(sanity);

	memcpy(&sub_size, p, sizeof(uint32));
	p += sizeof(uint32);

	/* see dumpSharedLocalSnapshot_forCursor() for the correct order here */

	memcpy(&writerPid, p, sizeof(writerPid));
	p += sizeof(writerPid);

	memcpy(&localXid, p, sizeof(localXid));
	p += sizeof(localXid);

	memcpy(&localCid, p, sizeof(localCid));
	p += sizeof(localCid);

	memcpy(&localXactStartTimestamp, p, sizeof(localXactStartTimestamp));
	p += sizeof(localXactStartTimestamp);

	memcpy(&combocidcnt, p, sizeof(combocidcnt));
	p += sizeof(combocidcnt);

	memcpy(tmp_combocids, p, sizeof(tmp_combocids));
	p += sizeof(tmp_combocids);

	/* handle the combocid stuff (same as in GetSnapshotData()) */
	if (usedComboCids != combocidcnt)
	{
		if (usedComboCids == 0)
		{
			MemoryContext oldCtx =  MemoryContextSwitchTo(TopTransactionContext);
			comboCids = palloc(combocidcnt * sizeof(ComboCidKeyData));
			MemoryContextSwitchTo(oldCtx);
		}
		else
			repalloc(comboCids, combocidcnt * sizeof(ComboCidKeyData));
	}
	memcpy(comboCids, tmp_combocids, combocidcnt * sizeof(ComboCidKeyData));
	usedComboCids = ((combocidcnt < MaxComboCids) ? combocidcnt : MaxComboCids);

	memcpy(&snapshot->xmin, p, sizeof(snapshot->xmin));
	p += sizeof(snapshot->xmin);

	memcpy(&snapshot->xmax, p, sizeof(snapshot->xmax));
	p += sizeof(snapshot->xmax);

	memcpy(&snapshot->xcnt, p, sizeof(snapshot->xcnt));
	p += sizeof(snapshot->xcnt);

	memcpy(snapshot->xip, p, snapshot->xcnt * sizeof(TransactionId));
	p += snapshot->xcnt * sizeof(TransactionId);

	/* zero out the slack in the xip-array */
	memset(snapshot->xip + snapshot->xcnt, 0, (xipEntryCount - snapshot->xcnt)*sizeof(TransactionId));

	memcpy(&snapshot->curcid, p, sizeof(snapshot->curcid));

	/* Now we're done with the buffer */
	pfree(buffer);

	/*
	 * Now read the subtransaction ids. This can be a big number, so cannot
	 * allocate memory all at once.
	 */
	sub_size *= sizeof(TransactionId);

	ResetXidBuffer(&subxbuf);

	if (sub_size)
	{
		subxids = palloc(MAX_XIDBUF_SIZE);
	}

	while (sub_size > 0)
	{
		read_size = sub_size > MAX_XIDBUF_SIZE ? MAX_XIDBUF_SIZE : sub_size;
		if (!FileReadOK(f, (char *)subxids, read_size))
		{
			elog(ERROR, "Error in Reading Subtransaction file.");
		}
		subcnt = read_size/sizeof(TransactionId);
		AddSortedToXidBuffer(&subxbuf, subxids, subcnt);
		sub_size -= read_size;
	}

	if (subxids)
	{
		pfree(subxids);
	}

	/* we're done with file. */
	BufFileClose(f);

	SetSharedTransactionId_reader(localXid, snapshot->curcid);

	return;
}

/*
 * Free any shared snapshot files.
 */
void
AtEOXact_SharedSnapshot(void)
{
	ListCell *lc;
	List *oldlist;

	oldlist = shared_snapshot_files;
	shared_snapshot_files = NIL;

	foreach(lc, oldlist)
	{
		BufFileClose((BufFile * ) lfirst(lc));
	}
	list_free(oldlist);
}

/*
 * LogDistributedSnapshotInfo
 *   Log the distributed snapshot info in a given snapshot.
 *
 * The 'prefix' is used to prefix the log message.
 */
void
LogDistributedSnapshotInfo(Snapshot snapshot, const char *prefix)
{
	static const int MESSAGE_LEN = 500;

	if (!IsMVCCSnapshot(snapshot))
		return;

	DistributedSnapshotWithLocalMapping *mapping =
		&(snapshot->distribSnapshotWithLocalMapping);

	char message[MESSAGE_LEN];
	snprintf(message, MESSAGE_LEN, "%s Distributed snapshot info: "
			 "xminAllDistributedSnapshots=%d, distribSnapshotId=%d"
			 ", xmin=%d, xmax=%d, count=%d",
			 prefix,
			 mapping->header.xminAllDistributedSnapshots,
			 mapping->header.distribSnapshotId,
			 mapping->header.xmin,
			 mapping->header.xmax,
			 mapping->header.count);

	snprintf(message, MESSAGE_LEN, "%s, In progress array: {",
			 message);

	for (int no = 0; no < mapping->header.count; no++)
	{
		if (no != 0)
			snprintf(message, MESSAGE_LEN, "%s, (%d,%d)",
					 message, mapping->inProgressEntryArray[no].distribXid,
					 mapping->inProgressEntryArray[no].localXid);
		else
			snprintf(message, MESSAGE_LEN, "%s (%d,%d)",
					 message, mapping->inProgressEntryArray[no].distribXid,
					 mapping->inProgressEntryArray[no].localXid);
	}

	elog(LOG, "%s}", message);
}

struct mpp_xid_map_entry {
	pid_t				pid;
	TransactionId		global;
	TransactionId		local;
};

struct mpp_xid_map {
	int			size;
	int			cur;
	struct mpp_xid_map_entry map[1];
};

/*
 * mpp_global_xid_map
 */
Datum
mpp_global_xid_map(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	struct mpp_xid_map *ctx;
	bool	nulls[3];
	int			i, j;

	if (SRF_IS_FIRSTCALL())
	{
		TupleDesc	tupdesc;
		MemoryContext oldcontext;
		volatile SharedSnapshotStruct *arrayP = sharedSnapshotArray;

		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/*
		 * switch to memory context appropriate for multiple function calls
		 */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/* build tupdesc for result tuples */
		/* this had better match pg_locks view in system_views.sql */
		tupdesc = CreateTemplateTupleDesc(3, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "pid",
						   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "localxid",
						   XIDOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "globalxid",
						   XIDOID, -1, 0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		/* figure out how many slots we need */

		ctx = (struct mpp_xid_map *)palloc0(sizeof(struct mpp_xid_map) +
											arrayP->maxSlots * sizeof(struct mpp_xid_map_entry));

		j = 0;
		LWLockAcquire(SharedSnapshotLock, LW_EXCLUSIVE);
		for (i = 0; i < arrayP->maxSlots; i++)
		{
			SharedSnapshotSlot *testSlot = &arrayP->slots[i];

			if (testSlot->slotid != -1)
			{
				ctx->map[j].pid = testSlot->pid;
				ctx->map[j].local = testSlot->xid;
				ctx->map[j].global = testSlot->QDxid;
				j++;
			}
		}
		LWLockRelease(SharedSnapshotLock);
		ctx->size = j;

		funcctx->user_fctx = (void *)ctx;

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	ctx = (struct mpp_xid_map *)funcctx->user_fctx;

	MemSet(nulls, false, sizeof(nulls));
	while (ctx->cur < ctx->size)
	{
		Datum		values[3];

		HeapTuple	tuple;
		Datum		result;

		/*
		 * Form tuple with appropriate data.
		 */
		MemSet(values, 0, sizeof(values));

		values[0] = Int32GetDatum(ctx->map[ctx->cur].pid);
		values[1] = TransactionIdGetDatum(ctx->map[ctx->cur].local);
		values[2] = TransactionIdGetDatum(ctx->map[ctx->cur].global);

		ctx->cur++;

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);
		SRF_RETURN_NEXT(funcctx, result);
	}

	SRF_RETURN_DONE(funcctx);
}
