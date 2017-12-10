/*-------------------------------------------------------------------------
 *
 * heapam.h
 *	  POSTGRES heap access method definitions.
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/access/heapam.h,v 1.143 2009/06/11 14:49:08 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef HEAPAM_H
#define HEAPAM_H

#include "access/htup.h"
#include "access/sdir.h"
#include "access/skey.h"
#include "access/xlog.h"
#include "nodes/primnodes.h"
#include "storage/bufpage.h"
#include "storage/lock.h"
#include "utils/relcache.h"
#include "utils/relationnode.h"
#include "utils/snapshot.h"
#include "utils/tqual.h"

/* "options" flag bits for heap_insert */
#define HEAP_INSERT_SKIP_WAL	0x0001
#define HEAP_INSERT_SKIP_FSM	0x0002

typedef struct BulkInsertStateData *BulkInsertState;

// UNDONE: Temporarily.
extern void RelationFetchGpRelationNodeForXLog_Index(Relation relation);

/*
 * Check if we have the persistent TID and serial number for a relation.
 */
static inline bool
RelationNeedToFetchGpRelationNodeForXLog(Relation relation)
{
	if (!InRecovery && !GpPersistent_SkipXLogInfo(relation->rd_id))
	{
		return true;
	}
	else
		return false;
}

/*
 * Fetch the persistent TID and serial number for a relation from the gp_relation_node
 * if needed to put in the XLOG record header.
 */
static inline void
RelationFetchGpRelationNodeForXLog(Relation relation)
{
	if (RelationNeedToFetchGpRelationNodeForXLog(relation))
	{
		if (relation->rd_rel->relkind == RELKIND_INDEX )
		{
			// UNDONE: Temporarily.
			RelationFetchGpRelationNodeForXLog_Index(relation);
			return;
		}
		RelationFetchSegFile0GpRelationNode(relation);
	}
}

extern bool
RelationAllowedToGenerateXLogRecord(Relation relation);

typedef enum
{
	LockTupleShared,
	LockTupleExclusive
} LockTupleMode;


/* ----------------
 *		function prototypes for heap access method
 *
 * heap_create, heap_create_with_catalog, and heap_drop_with_catalog
 * are declared in catalog/heap.h
 * ----------------
 */


typedef enum
{
	LockTupleWait,		/* wait for lock until it's acquired */
	LockTupleNoWait,	/* if can't get lock right away, report error */
	LockTupleIfNotLocked/* if can't get lock right away, give up. no error */
} LockTupleWaitType;

inline static void xl_heaptid_set(
	struct xl_heaptid	*heaptid,
	Relation rel,
	ItemPointer tid)
{
	heaptid->node = rel->rd_node;
	RelationGetPTInfo(rel, &heaptid->persistentTid, &heaptid->persistentSerialNum);
	heaptid->tid = *tid;
}

inline static void xl_heapnode_set(
	struct xl_heapnode	*heapnode,

	Relation rel)
{
	heapnode->node = rel->rd_node;
	RelationGetPTInfo(rel, &heapnode->persistentTid, &heapnode->persistentSerialNum);
}

/* in heap/heapam.c */
extern Relation relation_open(Oid relationId, LOCKMODE lockmode);
extern Relation try_relation_open(Oid relationId, LOCKMODE lockmode, 
								  bool noWait);
extern Relation relation_openrv(const RangeVar *relation, LOCKMODE lockmode);
extern Relation try_relation_openrv(const RangeVar *relation, LOCKMODE lockmode,
									bool noWait);

extern void relation_close(Relation relation, LOCKMODE lockmode);

extern Relation heap_open(Oid relationId, LOCKMODE lockmode);
extern Relation heap_openrv(const RangeVar *relation, LOCKMODE lockmode);
extern Relation try_heap_open(Oid relationId, LOCKMODE lockmode, bool noWait);
extern Relation try_heap_openrv(const RangeVar *relation, LOCKMODE lockmode);

#define heap_close(r,l)  relation_close(r,l)

/* CDB */
extern Relation CdbOpenRelation(Oid relid, LOCKMODE reqmode, bool noWait, 
								bool *lockUpgraded);
extern Relation CdbTryOpenRelation(Oid relid, LOCKMODE reqmode, bool noWait, 
								   bool *lockUpgraded);
extern Relation CdbOpenRelationRv(const RangeVar *relation, LOCKMODE reqmode, 
								  bool noWait, bool *lockUpgraded);

/* struct definition appears in relscan.h */
typedef struct HeapScanDescData *HeapScanDesc;

/*
 * HeapScanIsValid
 *		True iff the heap scan is valid.
 */
#define HeapScanIsValid(scan) PointerIsValid(scan)

extern HeapScanDesc heap_beginscan(Relation relation, Snapshot snapshot,
			   int nkeys, ScanKey key);
extern HeapScanDesc heap_beginscan_strat(Relation relation, Snapshot snapshot,
					 int nkeys, ScanKey key,
					 bool allow_strat, bool allow_sync);
extern HeapScanDesc heap_beginscan_bm(Relation relation, Snapshot snapshot,
				  int nkeys, ScanKey key);
extern void heap_rescan(HeapScanDesc scan, ScanKey key);
extern void heap_endscan(HeapScanDesc scan);
extern HeapTuple heap_getnext(HeapScanDesc scan, ScanDirection direction);
extern void heap_getnextx(HeapScanDesc scan, ScanDirection direction,
			  HeapTupleData tdata[], int *tdatacnt,
			  int *seen_EOS);

extern bool heap_fetch(Relation relation, Snapshot snapshot,
		   HeapTuple tuple, Buffer *userbuf, bool keep_buf,
		   Relation stats_relation);
extern bool heap_hot_search_buffer(Relation rel, ItemPointer tid, Buffer buffer,
					   Snapshot snapshot, bool *all_dead);
extern bool heap_hot_search(ItemPointer tid, Relation relation,
				Snapshot snapshot, bool *all_dead);

extern void heap_get_latest_tid(Relation relation, Snapshot snapshot,
					ItemPointer tid);
extern void setLastTid(const ItemPointer tid);

extern BulkInsertState GetBulkInsertState(void);
extern void FreeBulkInsertState(BulkInsertState);

extern Oid heap_insert(Relation relation, HeapTuple tup, CommandId cid,
					   int options, BulkInsertState bistate, TransactionId xid);
extern HTSU_Result heap_delete(Relation relation, ItemPointer tid,
			ItemPointer ctid, TransactionId *update_xmax,
			CommandId cid, Snapshot crosscheck, bool wait);
extern HTSU_Result heap_update(Relation relation, ItemPointer otid,
			HeapTuple newtup,
			ItemPointer ctid, TransactionId *update_xmax,
			CommandId cid, Snapshot crosscheck, bool wait);
extern HTSU_Result heap_lock_tuple(Relation relation, HeapTuple tuple,
				Buffer *buffer, ItemPointer ctid,
				TransactionId *update_xmax, CommandId cid,
				LockTupleMode mode, LockTupleWaitType waittype);
extern void heap_inplace_update(Relation relation, HeapTuple tuple);
extern void frozen_heap_inplace_update(Relation relation, HeapTuple tuple);
extern bool heap_freeze_tuple(HeapTupleHeader tuple, TransactionId *cutoff_xid,
							  Buffer buf, bool xlog_replay);

extern Oid	simple_heap_insert(Relation relation, HeapTuple tup);
extern Oid frozen_heap_insert(Relation relation, HeapTuple tup);
extern Oid frozen_heap_insert_directed(Relation relation, HeapTuple tup, BlockNumber blockNum);
extern void simple_heap_delete(Relation relation, ItemPointer tid);
extern void simple_heap_delete_xid(Relation relation, ItemPointer tid, TransactionId xid);
extern void simple_heap_update(Relation relation, ItemPointer otid,
				   HeapTuple tup);

extern void heap_markpos(HeapScanDesc scan);
extern void heap_markposx(HeapScanDesc scan, HeapTuple tuple);
extern void heap_restrpos(HeapScanDesc scan);

extern void heap_sync(Relation relation);

extern void heap_redo(XLogRecPtr beginLoc, XLogRecPtr lsn, XLogRecord *rptr);
extern void heap_desc(StringInfo buf, XLogRecPtr beginLoc, XLogRecord *record);
extern bool heap_getrelfilenode(
	XLogRecord 		*record,
	RelFileNode		*relFileNode);
extern void heap2_redo(XLogRecPtr beginLoc, XLogRecPtr lsn, XLogRecord *rptr);
extern void heap2_desc(StringInfo buf, XLogRecPtr beginLoc, XLogRecord *record);
extern void heap_mask(char *pagedata, BlockNumber blkno);

extern void log_heap_newpage(Relation rel, 
							 Page page,
							 BlockNumber bno);
extern XLogRecPtr log_heap_move(Relation reln, Buffer oldbuf,
			  ItemPointerData from,
			  Buffer newbuf, HeapTuple newtup,
			  bool all_visible_cleared, bool new_all_visible_cleared);
extern XLogRecPtr log_heap_clean(Relation reln, Buffer buffer,
			   OffsetNumber *redirected, int nredirected,
			   OffsetNumber *nowdead, int ndead,
			   OffsetNumber *nowunused, int nunused,
			   bool redirect_move);
extern XLogRecPtr log_heap_freeze(Relation reln, Buffer buffer,
				TransactionId cutoff_xid,
				OffsetNumber *offsets, int offcnt);

extern XLogRecPtr log_newpage_rel(Relation rel, ForkNumber forkNum, BlockNumber blkno,
								  Page page);

extern XLogRecPtr log_newpage_relFileNode(RelFileNode *relFileNode,
										  ForkNumber forkNum,
										  BlockNumber blkno, Page page,
										  ItemPointer persistentTid,
										  int64 persistentSerialNum);

/* in heap/pruneheap.c */
extern void heap_page_prune_opt(Relation relation, Buffer buffer,
					TransactionId OldestXmin);
extern int heap_page_prune(Relation relation, Buffer buffer,
				TransactionId OldestXmin,
				bool redirect_move, bool report_stats);
extern void heap_page_prune_execute(Buffer buffer,
						OffsetNumber *redirected, int nredirected,
						OffsetNumber *nowdead, int ndead,
						OffsetNumber *nowunused, int nunused,
						bool redirect_move);
extern void heap_get_root_tuples(Page page, OffsetNumber *root_offsets);

/* in heap/syncscan.c */
extern void ss_report_location(Relation rel, BlockNumber location);
extern BlockNumber ss_get_location(Relation rel, BlockNumber relnblocks);
extern void SyncScanShmemInit(void);
extern Size SyncScanShmemSize(void);

#endif   /* HEAPAM_H */
