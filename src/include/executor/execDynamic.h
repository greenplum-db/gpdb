/*--------------------------------------------------------------------
 * execPartition.h
 *		POSTGRES partitioning executor interface
 *
 * Copyright (C) 2023 VMware, Inc. or its affiliates. All Rights Reserved.
 *
 * IDENTIFICATION
 *		src/include/executor/execPartition.h
 *--------------------------------------------------------------------
 */

#ifndef EXECDYNAMIC_H
#define EXECDYNAMIC_H

#include "nodes/execnodes.h"

extern AttrNumber *GetColumnMapping(Oid oldOid, Oid newOid);
extern TupleTableSlot *ExecNextDynamicIndexScan(DynamicIndexScanState *node);
extern void ExecEndDynamicIndexScan(DynamicIndexScanState *node);
extern void ExecReScanDynamicIndex(DynamicIndexScanState *node);

#endif							/* EXECDYNAMIC_H */
