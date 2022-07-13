/*------------------------------------------------------------------------------
 *
 * appendonly_compaction
 *
 * Copyright (c) 2013-Present Pivotal Software, Inc.
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

extern void AppendOptimizedDropDeadSegments(Relation aorel, Bitmapset *segnos);
extern void AppendOnlyDrop(Relation aorel,
			   List *compaction_segno,
			   Bitmapset **collect_dead_segs);
extern void AppendOnlyCompact(Relation aorel,
				  List *compaction_segno_list,
				  int insert_segno,
				  bool isFull);
extern bool AppendOnlyCompaction_ShouldCompact(
								   Relation aoRelation,
								   int segno,
								   int64 segmentTotalTupcount,
								   bool isFull,
								   Snapshot appendOnlyMetaDataSnapshot);
extern void AppendOnlyThrowAwayTuple(Relation rel,
						 TupleTableSlot *slot, MemTupleBinding *mt_bind);
extern void AppendOnlyTruncateToEOF(Relation aorel);
extern bool HasLockForSegmentFileDrop(Relation aorel);
extern bool AppendOnlyCompaction_IsRelationEmpty(Relation aorel);

#endif
