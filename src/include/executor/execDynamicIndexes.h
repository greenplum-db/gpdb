/*--------------------------------------------------------------------
 * execDynamicIndexes.h
 *
 * Copyright (C) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
 *
 * IDENTIFICATION
 *		src/include/executor/execDynamicIndexes.h
 *--------------------------------------------------------------------
 */

#ifndef EXECDYNAMICINDEXES_H
#define EXECDYNAMICINDEXES_H

#include "nodes/execnodes.h"

extern AttrNumber *GetColumnMapping(Oid oldOid, Oid newOid);
extern TupleTableSlot *ExecNextDynamicIndexScan(DynamicIndexScanState *node);
extern void ExecEndDynamicIndexScan(DynamicIndexScanState *node);
extern void ExecReScanDynamicIndex(DynamicIndexScanState *node);

#endif							/* EXECDYNAMICINDEXES_H */
