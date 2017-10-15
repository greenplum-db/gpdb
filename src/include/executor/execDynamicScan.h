/*--------------------------------------------------------------------------
 *
 * execDynamicScan.h
 *	 Definitions and API functions for execDynamicScan.c
 *
 * Copyright (c) 2014-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/executor/execDynamicScan.h
 *
 *--------------------------------------------------------------------------
 */
#ifndef EXECDYNAMICSCAN_H
#define EXECDYNAMICSCAN_H

#include "access/attnum.h"
#include "nodes/execnodes.h"
#include "executor/tuptable.h"

typedef void (PartitionInitMethod)(ScanState *scanState, AttrNumber *attMap);
typedef void (PartitionEndMethod)(ScanState *scanState);
typedef void (PartitionReScanMethod)(ScanState *scanState);
typedef TupleTableSlot * (PartitionScanTupleMethod)(ScanState *scanState);

extern void
DynamicScan_Begin(ScanState *scanState, Plan *plan, EState *estate, int eflags);

extern void
DynamicScan_End(ScanState *scanState, PartitionEndMethod *partitionEndMethod);

extern void
DynamicScan_ReScan(ScanState *scanState, ExprContext *exprCtxt);

extern TupleTableSlot *
DynamicScan_GetNextTuple(ScanState *scanState, PartitionInitMethod *partitionInitMethod,
		PartitionEndMethod *partitionEndMethod, PartitionReScanMethod *partitionReScanMethod,
		PartitionScanTupleMethod *partitionScanTupleMethod);

extern MemoryContext
DynamicScan_GetPartitionMemoryContext(ScanState *scanState);

extern Oid
DynamicScan_GetTableOid(ScanState *scanState);

extern bool
DynamicScan_RemapExpression(ScanState *scanState, AttrNumber *attMap, Node *expr);

#endif   /* EXECDYNAMICSCAN_H */
