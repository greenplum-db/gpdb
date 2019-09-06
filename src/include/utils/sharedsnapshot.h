/*-------------------------------------------------------------------------
 *
 * sharedsnapshot.h
 *	  GPDB shared snapshot management
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/utils/sharedsnapshot.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef SHAREDSNAPSHOT_H
#define SHAREDSNAPSHOT_H

#include "storage/proc.h"
#include "utils/combocid.h"
#include "utils/tqual.h"

/* MPP Shared Snapshot */
typedef struct SharedSnapshotSlot
{
	int32			slotindex;  /* where in the array this one is. */
	int32	 		slotid;
	pid_t	 		pid; /* pid of writer seg */
	PGPROC			*writer_proc;
	PGXACT			*writer_xact;
	TransactionId	xid;
	CommandId       cid;
	TimestampTz		startTimestamp;
	volatile TransactionId   QDxid;
	volatile CommandId		QDcid;
	volatile bool			ready;
	volatile uint32			segmateSync;
	uint32			combocidcnt;
	ComboCidKeyData combocids[MaxComboCids];
	SnapshotData	snapshot;
	LWLockId        slotLock;
} SharedSnapshotSlot;

extern volatile SharedSnapshotSlot *SharedLocalSnapshotSlot;

/* MPP Shared Snapshot */
extern Size SharedSnapshotShmemSize(void);
extern void CreateSharedSnapshotArray(void);
extern char *SharedSnapshotDump(void);

extern void SharedSnapshotRemove(volatile SharedSnapshotSlot *slot, char *creatorDescription);
extern void addSharedSnapshot(char *creatorDescription, int id);
extern void lookupSharedSnapshot(char *lookerDescription, char *creatorDescription, int id);

extern void dumpSharedLocalSnapshot_forCursor(void);
extern void readSharedLocalSnapshot_forCursor(Snapshot snapshot, DtxContext distributedTransactionContext);

extern void AtEOXact_SharedSnapshot(void);

#define NUM_SHARED_SNAPSHOT_SLOTS (2 * max_prepared_xacts)

#endif   /* SHAREDSNAPSHOT_H */
