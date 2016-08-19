/*------------------------------------------------------------------------------
 *
 * aocs_compaction
 *
 * Copyright (c) 2013, Pivotal.
 *
 *------------------------------------------------------------------------------
*/
#ifndef AOCS_COMPACTION_H
#define AOCS_COMPACTION_H

#include "postgres.h"
#include "nodes/pg_list.h"
#include "utils/rel.h"

extern void AOCSDrop(Relation aorel, int compaction_segno);
extern void AOCSCompact(Relation aorel, 
		int compaction_segno,
		int insert_segno,
		bool isFull);
extern void AOCSTruncateToEOF(Relation aorel);
#endif
