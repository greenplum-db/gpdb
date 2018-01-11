/*-------------------------------------------------------------------------
 *
 * execIndexScan.h
 *
 * Portions Copyright (c) 2013 - present, EMC/Greenplum
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/executor/execIndexscan.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef EXECINDEXSCAN_H
#define EXECINDEXSCAN_H

#include "nodes/execnodes.h"


extern void
InitCommonIndexScanState(IndexScanState *indexstate, IndexScan *node, EState *estate, int eflags);

extern Relation
OpenIndexRelation(EState *estate, Oid indexOid, Index tableRtIndex);

extern void
InitRuntimeKeysContext(IndexScanState *indexstate);

extern void
FreeRuntimeKeysContext(IndexScanState *indexstate);

extern void
ExecIndexBuildScanKeys(PlanState *planstate, Relation index,
					List *quals, ScanKey *scanKeys, int *numScanKeys,
					IndexRuntimeKeyInfo **runtimeKeys, int *numRuntimeKeys,
					IndexArrayKeyInfo **arrayKeys, int *numArrayKeys);


#endif
