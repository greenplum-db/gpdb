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

#define INVALID_SEGMATE 0
#define IS_VALID_SEGMATE(x)  ((x != INVALID_SEGMATE))

typedef struct SharedSnapshotLockSlot
{
	int32			slotindex;  /* where in the array this one is. */
	int32	 		session_id;
	LWLock		   *lock;
}SharedSnapshotLockSlot;

typedef struct SnapshotDump
{
	uint32 segmateSync;
	dsm_handle  handle;
	dsm_segment *segment;
} SnapshotDump;

#define SNAPSHOTDUMPARRAYSZ 32

/* MPP Shared Snapshot */
typedef struct SharedSnapshotDesc
{
	PGPROC			*writer_proc;
	PGXACT			*writer_xact;
	volatile uint32	segmateSync;
	volatile int    cur_dump_id;
	SnapshotData   snapshot;

	volatile SnapshotDump    dump[SNAPSHOTDUMPARRAYSZ];
	/* for debugging only */
	TransactionId	xid;
	TimestampTz		startTimestamp;
} SharedSnapshotDesc;

typedef struct SharedSnapshotData
{
	Snapshot                snapshot;
	volatile SharedSnapshotLockSlot *lockSlot;
	SharedSnapshotDesc      *desc;
}SharedSnapshotData;

extern struct SharedSnapshotData SharedSnapshot;

/* MPP Shared Snapshot */
extern Size SharedSnapshotShmemSize(void);
extern void CreateSharedSnapshotArray(void);
extern char *SharedSnapshotDump(void);

extern void SharedSnapshotRemove(char *creatorDescription);
extern void addSharedSnapshot(char *creatorDescription, int sessionid);
extern void lookupSharedSnapshot(char *lookerDescription, char *creatorDescription, int sessionid);

extern void publishSharedSnapshot(uint32 segmateSync, Snapshot snapshot, bool for_cursor);
extern void syncSharedSnapshot(uint32 segmateSync, bool for_cursor);

extern void AtEOXact_SharedSnapshot(void);

#define NUM_SHARED_SNAPSHOT_SLOTS (2 * max_prepared_xacts)

#endif   /* SHAREDSNAPSHOT_H */
