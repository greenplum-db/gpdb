/*------------------------------------------------------------------------------
 *
 * aocs_compaction.h
 *
 * Copyright (c) 2013-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/access/aocs_compaction.h
 *
 *------------------------------------------------------------------------------
 */
#ifndef AOCS_COMPACTION_H
#define AOCS_COMPACTION_H

#include "nodes/pg_list.h"
#include "utils/rel.h"

struct AOCSVPInfo;
struct AOVacuumRelStats;
extern void AOCSSegmentFileTruncateToEOF(Relation aorel, int segno, struct AOCSVPInfo *vpinfo, struct AOVacuumRelStats *vacrelstats);
extern void AOCSCompaction_DropSegmentFile(Relation aorel, int segno, struct AOVacuumRelStats *vacrelstats);
extern void AOCSCompact(Relation aorel,
						List *compaction_segnos,
						int insert_segno,
						struct AOVacuumRelStats *vacrelstats);

#endif
