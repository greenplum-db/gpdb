/*-------------------------------------------------------------------------
 *
 * appendonlywriter.h
 *	  Definitions for appendonlywriter.c
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/include/access/appendonlywriter.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef APPENDONLYWRITER_H
#define APPENDONLYWRITER_H

#include "access/aosegfiles.h"
#include "nodes/plannodes.h"
#include "nodes/relation.h"
#include "storage/lock.h"
#include "tcop/dest.h"

/*
 * Maximum concurrent number of writes into a single append only table.
 * TODO: may want to make this a guc instead (can only be set at gpinit time).
 */
#define MAX_AOREL_CONCURRENCY 128

/*
 * This segfile number may only be used for special case write operations.
 * These include operations that bypass the qd -OR- that need not worry about
 * concurrency. Currently these are:
 *
 *   - UTILITY mode operations, for example, gp_restore.
 *   - Table rewrites that are sometimes done as a part of ALTER TABLE.
 *   - CTAS, which runs on an exclusively locked destination table.
 *
 * Note that in some cases (such as CTAS) the aoseg table on the QD will not
 * be updated for segment entry 0 after the operation completes. This is ok
 * as the concurrency module never cares about the RESERVED segno anyway. When
 * wanting to sync the aoseg table on the master after such operations you need
 * to call gp_update_ao_master_stats(), which gp_restore does automatically.
 */
#define RESERVED_SEGNO 0

/*
 * GUC variables
 */
extern int	MaxAppendOnlyTables;	/* Max # of concurrently used AO rels */

/*
 * The valid states w.r.t. inuse, incompaction and awaiting drop are
 *   | use | compa. | awaiting | description
 * ---------------------------------------------------------------------
 *   |     |        |          | Default initial state
 * ---------------------------------------------------------------------
 *   |     |        |    x     | Initial state if the last compaction drop
 *   |     |        |          | transaction on the segment file was aborted.
 *   |     |        |          | The segment file should not be used for inserts.
 * ---------------------------------------------------------------------
 *   | x   |        |          | Segment file is assigned to an insert or
 *   |     |        |          | compaction transaction for insertion.
 * ---------------------------------------------------------------------
 *   | x   |    x   |          | Segment file is assigned to a compaction transaction.
 *   |     |        |          | The segment file is in the compaction phase.
 * ---------------------------------------------------------------------
 *   | x   |    x   |    x     | The segment file is assigned to be dropped in
 *   |     |        |          | a compaction drop transaction (state 6) or
 *   |     |        |          | is in a pseudo compaction transaction (state 7)
 *   |     |        |          | after a fault recovery.
 * ---------------------------------------------------------------------
 *   |     |    x   |    x     | INVALID STATE
 * ---------------------------------------------------------------------
 *   | x   |        |    x     | The segment file has been compacted by the
 *   |     |        |          | current vacuum run and will be dropped.
 * ---------------------------------------------------------------------
 *   |     |    x   |          | INVALID STATE
 * ---------------------------------------------------------------------
 *
 * Awaiting drop means: if true - the segment file is awaiting
 * drop in at least one of the segment
 * nodes. The segment file should not
 * be used for inserts or compaction inserts.
 *  The segment file should be compacted before
 * any other segment file. At most one segment
 * file can awaiting a drop.
 *
 * The valid state transitions are:
 * 1 -> 3 (normal insertion or compact insertion)
 * 1 -> 4 (compaction)
 * 2 -> 7 (pseudo compaction transaction started)
 * 3 -> 1 (insertion committed or aborted)
 * 4 -> 6 (compaction transaction committed)
 * 4 -> 1 (compaction transaction aborted)
 * 5 -> 1 (drop transaction committed)
 * 5 -> 2 (drop transaction aborted)
 * 6 -> 5 (drop transaction started)
 * 6 -> 7 (pseudo compaction transaction started)
 * 6 -> 8 (drop transaction skipped)
 * 7 -> 6 (pseudo compaction committed)
 * 7 -> 2 (pseudo compaction aborted)
 * 8 -> 2 (drop transaction skipped (commit or aborted))
 */
typedef enum AOSegfileState
{
	AVAILABLE = 1,
	AWAITING_DROP_READY = 2,
	INSERT_USE = 3,
	COMPACTION_USE = 4,
	DROP_USE = 5,
	COMPACTED_AWAITING_DROP = 6,
	PSEUDO_COMPACTION_USE = 7,
	COMPACTED_DROP_SKIPPED = 8
} AOSegfileState;

/*
 * Describes the status of a particular file segment number accross an entire
 * AO relation.
 *
 * This information is kept in a hash table and kept up to date. It represents
 * the global status of this segment number accross the whole array, for example
 * 'tupcount' for segno #4 will show the total rows in all file segments with
 * number 4 for this specific table.
 *
 * This data structure is accessible by the master node and it is used to make
 * decisions regarding file segment allocations for data writes. Note that it
 * is not used for reads. Durind reads each segdb scanner reads its local
 * catalog.
 *
 * Note that 'isfull' tries to guess if any of the file segments is full. Since
 * there may be some skew in the data we use a threshold that is a bit lower
 * than the max tuples allowed per segment.
 *

 */
typedef struct AOSegfileStatus
{
	/*
	 * Total tupcount for this segno across all segdbs. This includes
	 * invisible tuples
	 */
	int64		total_tupcount;

	/*
	 * Num tuples added in this segno across  all segdbs in the current
	 * transaction
	 */
	int64		tupsadded;

	/*
	 * the inserting transaction id
	 */
	TransactionId xid;

	/*
	 * the latest committed inserting transaction id
	 */
	DistributedTransactionId latestWriteXid;

	AOSegfileState state;

	int16		formatversion;

	/* if true - never insert into this segno anymore */
	bool		isfull;


	/*
	 * Flag to indicate if the current transaction has been aborted. It is set
	 * by AtAbort_AppendOnly to ensure the correct state transition in
	 * AtEOXact_AppendOnly_Relation. Outside this transition, the value is
	 * false.
	 */
	bool		aborted;
} AOSegfileStatus;

/*
 * Describes the status of all file segments of an AO relation in the system.
 * This data structure is kept in a hash table on the master and kept up to
 * date.
 *
 * 'relid' stands for the AO relation this entry serves.
 * 'txns_using_rel' stands for the number of transactions that are currently
 * inserting into this relation. if equals zero it is safe to remove this
 * entry from the hash table (when needed).
 */
typedef struct AORelHashEntryData
{
	Oid			relid;
	int			txns_using_rel;
	AOSegfileStatus relsegfiles[MAX_AOREL_CONCURRENCY];

} AORelHashEntryData;
typedef AORelHashEntryData *AORelHashEntry;


typedef struct AppendOnlyWriterData
{
	int			num_existing_aorels;	/* Current # of recorded entries for
										 * AO relations */
} AppendOnlyWriterData;
extern AppendOnlyWriterData *AppendOnlyWriter;

/*
 * functions in appendonlywriter.c
 */
extern Size AppendOnlyWriterShmemSize(void);
extern void InitAppendOnlyWriter(void);
extern Size AppendOnlyWriterShmemSize(void);
extern int	SetSegnoForWrite(Relation rel, int existingsegno);
extern void RegisterSegnoForCompactionDrop(Oid relid, List *compactedSegmentFileList);
extern void DeregisterSegnoForCompactionDrop(Oid relid, List *compactedSegmentFileList);
extern List *SetSegnoForCompaction(Relation rel, List *compactedSegmentFileList,
					  List *insertedSegmentFileList, bool *isdrop);
extern int SetSegnoForCompactionInsert(Relation rel, List *compacted_segno,
							List *compactedSegmentFileList,
							List *insertedSegmentFileList);
extern List *assignPerRelSegno(List *all_rels);
extern void UpdateMasterAosegTotals(Relation parentrel,
						int segno,
						int64 tupcount,
						int64 modcount_added);
extern void UpdateMasterAosegTotalsFromSegments(Relation parentrel,
									Snapshot appendOnlyMetaDataSnapshot, List *segmentNumList,
									int64 modcount_added);
extern bool AORelRemoveHashEntry(Oid relid);
extern void AtCommit_AppendOnly(void);
extern void AtAbort_AppendOnly(void);
extern void AtEOXact_AppendOnly(void);

#endif							/* APPENDONLYWRITER_H */
