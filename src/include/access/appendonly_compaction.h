/*------------------------------------------------------------------------------
 *
 * appendonly_compaction
 *
 * Copyright (c) 2013-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/access/appendonly_compaction.h
 *
 *------------------------------------------------------------------------------
*/
#ifndef APPENDONLY_COMPACTION_H
#define APPENDONLY_COMPACTION_H

#include "nodes/pg_list.h"
#include "access/appendonly_visimap.h"
#include "utils/rel.h"
#include "access/memtup.h"
#include "executor/tuptable.h"

#define APPENDONLY_COMPACTION_SEGNO_INVALID (-1)

/*
 * Stats for progress reporting.
 * This is AO/AOCO counterpart of LVRelStats for Heap. It lives throughout
 * all phases of AO VACUUM.
 */
typedef struct AOVacuumRelStats
{
	int		nbytes_truncated;	/* current # of bytes truncated from segment file */
	int		num_dead_tuples;	/* current # of dead tuples */
	int		num_index_vacuumed; /* current # of indexes been vacuumed */
} AOVacuumRelStats;

extern Bitmapset *AppendOptimizedCollectDeadSegments(Relation aorel);
extern void AppendOptimizedDropDeadSegments(Relation aorel, Bitmapset *segnos, AOVacuumRelStats *vacrelstats);
extern void AppendOnlyCompact(Relation aorel,
							  int compaction_segno,
							  int *insert_segno,
							  bool isFull,
							  List *avoid_segnos,
							  AOVacuumRelStats *vacrelstats);
extern bool AppendOnlyCompaction_ShouldCompact(
								   Relation aoRelation,
								   int segno,
								   int64 segmentTotalTupcount,
								   bool isFull,
								   Snapshot appendOnlyMetaDataSnapshot);
extern void AppendOnlyThrowAwayTuple(Relation rel, TupleTableSlot *slot, MemTupleBinding *mt_bind);
extern void AppendOptimizedTruncateToEOF(Relation aorel, AOVacuumRelStats *vacrelstats);

#endif
