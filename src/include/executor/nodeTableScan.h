/*-------------------------------------------------------------------------
 *
 * nodeTableScan.h
 *
 * Portions Copyright (c) 2012 - present, EMC/Greenplum
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/executor/nodeTableScan.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODETABLESCAN_H
#define NODETABLESCAN_H

#include "nodes/execnodes.h"

extern int	ExecCountSlotsTableScan(TableScan *node);
extern TableScanState *ExecInitTableScan(TableScan *node, EState *estate, int eflags);
extern TupleTableSlot *ExecTableScan(TableScanState *node);
extern void ExecEndTableScan(TableScanState *node);
extern void ExecTableMarkPos(TableScanState *node);
extern void ExecTableRestrPos(TableScanState *node);
extern void ExecTableReScan(TableScanState *node, ExprContext *exprCtxt);
extern void ExecEagerFreeTableScan(TableScanState *node);

#endif
