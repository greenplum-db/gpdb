/*-------------------------------------------------------------------------
 *
 * tqual.h
 *	  POSTGRES "time qualification" definitions, ie, tuple visibility rules.
 *
 *	  Should be moved/renamed...	- vadim 07/28/98
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/utils/tqual.h,v 1.74 2009/01/01 17:24:02 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef TQUAL_H
#define TQUAL_H

#include "utils/rel.h"	/* Relation */
#include "utils/snapshot.h"


/* Static variables representing various special snapshot semantics */
extern PGDLLIMPORT SnapshotData SnapshotNowData;
extern PGDLLIMPORT SnapshotData SnapshotSelfData;
extern PGDLLIMPORT SnapshotData SnapshotAnyData;
extern PGDLLIMPORT SnapshotData SnapshotToastData;

#define SnapshotNow			(&SnapshotNowData)
#define SnapshotSelf		(&SnapshotSelfData)
#define SnapshotAny			(&SnapshotAnyData)
#define SnapshotToast		(&SnapshotToastData)

/*
 * We don't provide a static SnapshotDirty variable because it would be
 * non-reentrant.  Instead, users of that snapshot type should declare a
 * local variable of type SnapshotData, and initialize it with this macro.
 */
#define InitDirtySnapshot(snapshotdata)  \
	((snapshotdata).satisfies = HeapTupleSatisfiesDirty)

/* This macro encodes the knowledge of which snapshots are MVCC-safe */
#define IsMVCCSnapshot(snapshot)  \
	((snapshot)->satisfies == HeapTupleSatisfiesMVCC)

/*
 * HeapTupleSatisfiesVisibility
 *		True iff heap tuple satisfies a time qual.
 *
 * Notes:
 *	Assumes heap tuple is valid.
 *	Beware of multiple evaluations of snapshot argument.
 *	Hint bits in the HeapTuple's t_infomask may be updated as a side effect;
 *	if so, the indicated buffer is marked dirty.
 *
 *   GP: The added relation parameter helps us decide if we are going to set tuple hint
 *   bits.  If it is null, we ignore the gp_disable_tuple_hints GUC.
 */
#define HeapTupleSatisfiesVisibility(rel, tuple, snapshot, buffer)	\
	((*(snapshot)->satisfies) (rel, (tuple)->t_data, snapshot, buffer))

/* Result codes for HeapTupleSatisfiesVacuum */
typedef enum
{
	HEAPTUPLE_DEAD,				/* tuple is dead and deletable */
	HEAPTUPLE_LIVE,				/* tuple is live (committed, no deleter) */
	HEAPTUPLE_RECENTLY_DEAD,	/* tuple is dead, but not deletable yet */
	HEAPTUPLE_INSERT_IN_PROGRESS,		/* inserting xact is still in progress */
	HEAPTUPLE_DELETE_IN_PROGRESS	/* deleting xact is still in progress */
} HTSV_Result;

/* These are the "satisfies" test routines for the various snapshot types */
extern bool HeapTupleSatisfiesMVCC(Relation relation, HeapTupleHeader tuple,
					   Snapshot snapshot, Buffer buffer);
extern bool HeapTupleSatisfiesNow(Relation relation, HeapTupleHeader tuple,
					  Snapshot snapshot, Buffer buffer);
extern bool HeapTupleSatisfiesSelf(Relation relation, HeapTupleHeader tuple,
					   Snapshot snapshot, Buffer buffer);
extern bool HeapTupleSatisfiesAny(Relation relation, HeapTupleHeader tuple,
					  Snapshot snapshot, Buffer buffer);
extern bool HeapTupleSatisfiesToast(Relation relation, HeapTupleHeader tuple,
						Snapshot snapshot, Buffer buffer);
extern bool HeapTupleSatisfiesDirty(Relation relation, HeapTupleHeader tuple,
						Snapshot snapshot, Buffer buffer);

/* Special "satisfies" routines with different APIs */
extern HTSU_Result HeapTupleSatisfiesUpdate(Relation relation, HeapTupleHeader tuple,
						 CommandId curcid, Buffer buffer);
extern HTSV_Result HeapTupleSatisfiesVacuum(Relation relation, HeapTupleHeader tuple,
						 TransactionId OldestXmin, Buffer buffer);

extern void HeapTupleSetHintBits(HeapTupleHeader tuple, Buffer buffer, Relation rel,
					 uint16 infomask, TransactionId xid);

#endif   /* TQUAL_H */
