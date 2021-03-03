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

extern void AppendOnlyRecycleDeadSegments(Relation aorel);
extern void AppendOnlyCompact(Relation aorel,
							  int compaction_segno,
							  int *insert_segno,
							  bool isFull,
							  List *avoid_segnos);
extern bool AppendOnlyCompaction_ShouldCompact(
								   Relation aoRelation,
								   int segno,
								   int64 segmentTotalTupcount,
								   bool isFull,
								   Snapshot appendOnlyMetaDataSnapshot);
extern void AppendOnlyThrowAwayTuple(Relation rel, TupleTableSlot *slot);
extern void AppendOnlyTruncateToEOF(Relation aorel);

#endif
