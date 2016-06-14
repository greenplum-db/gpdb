/*-------------------------------------------------------------------------
 *
 * tqual.h
 *	  POSTGRES "time qualification" definitions, ie, tuple visibility rules.
 *
 *	  Should be moved/renamed...	- vadim 07/28/98
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/utils/tqual.h,v 1.65 2007/01/05 22:19:59 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef TQUAL_H
#define TQUAL_H

#include "access/htup.h"
#include "storage/buf.h"
#include "utils/timestamp.h"  /* TimestampTz */
#include "access/xact.h"   /* MaxGpSavePoints */
#include "utils/combocid.h" /*MaxComboCids*/
#include "utils/rel.h"	/* Relation */


#include "cdb/cdbdistributedsnapshot.h"  /* DistributedSnapshotWithLocalMapping */

#define MAX_XIDBUF_SIZE (1024 * 1024)
#define MAX_XIDBUF_XIDS (MAX_XIDBUF_SIZE/sizeof(uint32))
#define MAX_XIDBUF_INIT_PAGES 16

/*
 * "Regular" snapshots are pointers to a SnapshotData structure.
 *
 * We also have some "special" snapshot values that have fixed meanings
 * and don't need any backing SnapshotData.  These are encoded by small
 * integer values, which of course is a gross violation of ANSI C, but
 * it works fine on all known platforms.
 *
 * SnapshotDirty is an even more special case: its semantics are fixed,
 * but there is a backing SnapshotData struct for it.  That struct is
 * actually used as *output* data from tqual.c, not input into it.
 * (But hey, SnapshotDirty ought to have a dirty implementation, no? ;-))
 */

typedef struct SnapshotData
{
	TransactionId xmin;			/* XID < xmin are visible to me */
	TransactionId xmax;			/* XID >= xmax are invisible to me */
	uint32		xcnt;			/* # of xact ids in xip[] */
	TransactionId *xip;			/* array of xact IDs in progress */
	/* note: all ids in xip[] satisfy xmin <= xip[i] < xmax */
	int32		subxcnt;		/* # of xact ids in subxip[], -1 if overflow */
	TransactionId *subxip;		/* array of subxact IDs in progress */
	/*
	 * note: all ids in subxip[] are >= xmin, but we don't bother filtering
	 * out any that are >= xmax
	 */
	CommandId	curcid;			/* in my xact, CID < curcid are visible */

	bool haveDistribSnapshot;
						/* True if this snapshot is distributed. */
								
	DistributedSnapshotWithLocalMapping	distribSnapshotWithLocalMapping;
								/*
								 * GP: Global information about which
								 * transactions are visible for a distributed
								 * transaction, with cached local xids
								 */
} SnapshotData;

typedef SnapshotData *Snapshot;

/* Special snapshot values: */
#define InvalidSnapshot				((Snapshot) 0x0)	/* same as NULL */
#define SnapshotNow					((Snapshot) 0x1)
#define SnapshotSelf				((Snapshot) 0x2)
#define SnapshotAny					((Snapshot) 0x3)
#define SnapshotToast				((Snapshot) 0x4)

extern PGDLLIMPORT Snapshot SnapshotDirty;

/* This macro encodes the knowledge of which snapshots are MVCC-safe */
#define IsMVCCSnapshot(snapshot)  \
	((snapshot) != SnapshotNow && \
	 (snapshot) != SnapshotSelf && \
	 (snapshot) != SnapshotAny && \
	 (snapshot) != SnapshotToast && \
	 (snapshot) != SnapshotDirty)


extern PGDLLIMPORT Snapshot SerializableSnapshot;
extern PGDLLIMPORT Snapshot LatestSnapshot;
extern PGDLLIMPORT Snapshot ActiveSnapshot;

extern TransactionId TransactionXmin;
extern TransactionId RecentXmin;
extern TransactionId RecentGlobalXmin;

/*
 * HeapTupleSatisfiesVisibility
 *		True iff heap tuple satisfies a time qual.
 *
 * Notes:
 *	Assumes heap tuple is valid.
 *	Beware of multiple evaluations of snapshot argument.
 *	Hint bits in the HeapTuple's t_infomask may be updated as a side effect.
 *
 *   GP: The added relation parameter helps us decide if we are going to set tuple hint
 *   bits.  If it is null, we ignore the gp_disable_tuple_hints GUC.
 *
 */
#define HeapTupleSatisfiesVisibility(relation, tuple, snapshot, buffer) \
((snapshot) == SnapshotNow ? \
	HeapTupleSatisfiesNow(relation, (tuple)->t_data, buffer) \
: \
	((snapshot) == SnapshotSelf ? \
		HeapTupleSatisfiesItself(relation, (tuple)->t_data, buffer) \
	: \
		((snapshot) == SnapshotAny ? \
			true \
		: \
			((snapshot) == SnapshotToast ? \
				HeapTupleSatisfiesToast(relation, (tuple)->t_data, buffer) \
			: \
				((snapshot) == SnapshotDirty ? \
					HeapTupleSatisfiesDirty(relation, (tuple)->t_data, buffer) \
				: \
					HeapTupleSatisfiesSnapshot(relation, (tuple)->t_data, snapshot, buffer) \
				) \
			) \
		) \
	) \
)

/* Result codes for HeapTupleSatisfiesUpdate */
typedef enum
{
	HeapTupleMayBeUpdated,
	HeapTupleInvisible,
	HeapTupleSelfUpdated,
	HeapTupleUpdated,
	HeapTupleBeingUpdated
} HTSU_Result;

/* Result codes for HeapTupleSatisfiesVacuum */
typedef enum
{
	HEAPTUPLE_DEAD,				/* tuple is dead and deletable */
	HEAPTUPLE_LIVE,				/* tuple is live (committed, no deleter) */
	HEAPTUPLE_RECENTLY_DEAD,	/* tuple is dead, but not deletable yet */
	HEAPTUPLE_INSERT_IN_PROGRESS,		/* inserting xact is still in progress */
	HEAPTUPLE_DELETE_IN_PROGRESS	/* deleting xact is still in progress */
} HTSV_Result;

/* Result codes for TupleTransactionStatus */
typedef enum TupleTransactionStatus
{
	TupleTransactionStatus_None,
	TupleTransactionStatus_Frozen,
	TupleTransactionStatus_HintCommitted,
	TupleTransactionStatus_HintAborted,
	TupleTransactionStatus_CLogInProgress,
	TupleTransactionStatus_CLogCommitted,
	TupleTransactionStatus_CLogAborted,
	TupleTransactionStatus_CLogSubCommitted,
} TupleTransactionStatus;

/* Result codes for TupleVisibilityStatus */
typedef enum TupleVisibilityStatus
{
	TupleVisibilityStatus_Unknown,
	TupleVisibilityStatus_InProgress,
	TupleVisibilityStatus_Aborted,
	TupleVisibilityStatus_Past,
	TupleVisibilityStatus_Now,
} TupleVisibilityStatus;

typedef struct TupleVisibilitySummary
{
	ItemPointerData				tid;
	int16						infomask;
	int16						infomask2;
	ItemPointerData				updateTid;
	TransactionId				xmin;
	TupleTransactionStatus      xminStatus;
	TransactionId				xmax;
	TupleTransactionStatus      xmaxStatus;
	CommandId					cid;			/* inserting or deleting command ID, or both */
	TupleVisibilityStatus		visibilityStatus;
} TupleVisibilitySummary;

extern bool HeapTupleSatisfiesItself(Relation relation, HeapTupleHeader tuple, Buffer buffer);
extern bool HeapTupleSatisfiesNow(Relation relation, HeapTupleHeader tuple, Buffer buffer);
extern bool HeapTupleSatisfiesDirty(Relation relation, HeapTupleHeader tuple, Buffer buffer);
extern bool HeapTupleSatisfiesToast(Relation relation, HeapTupleHeader tuple, Buffer buffer);
extern bool HeapTupleSatisfiesSnapshot(Relation relation, HeapTupleHeader tuple,
						   Snapshot snapshot, Buffer buffer);
extern HTSU_Result HeapTupleSatisfiesUpdate(Relation relation, HeapTupleHeader tuple,
						 CommandId curcid, Buffer buffer);
extern HTSV_Result HeapTupleSatisfiesVacuum(HeapTupleHeader tuple,
						 TransactionId OldestXmin, Buffer buffer);

extern Snapshot GetTransactionSnapshot(void);
extern Snapshot GetLatestSnapshot(void);
extern Snapshot CopySnapshot(Snapshot snapshot);
extern void FreeSnapshot(Snapshot snapshot);
extern void FreeXactSnapshot(void);
extern void LogDistributedSnapshotInfo(Snapshot snapshot, const char *prefix);
extern void GetTupleVisibilitySummary(
	HeapTuple				tuple,
	TupleVisibilitySummary	*tupleVisibilitySummary);


// 0  gp_tid                    			TIDOID
// 1  gp_xmin                  			INT4OID
// 2  gp_xmin_status        			TEXTOID
// 3  gp_xmin_commit_distrib_id		TEXTOID
// 4  gp_xmax		          		INT4OID
// 5  gp_xmax_status       			TEXTOID
// 6  gp_xmax_distrib_id    			TEXTOID
// 7  gp_command_id	    			INT4OID
// 8  gp_infomask    	   			TEXTOID
// 9  gp_update_tid         			TIDOID
// 10 gp_visibility             			TEXTOID

extern void GetTupleVisibilitySummaryDatums(
	Datum		*values,
	bool		*nulls,
	TupleVisibilitySummary	*tupleVisibilitySummary);

extern char *GetTupleVisibilitySummaryString(
	TupleVisibilitySummary	*tupleVisibilitySummary);

#endif   /* TQUAL_H */
