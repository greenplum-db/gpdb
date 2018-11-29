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

extern TableScanState *ExecInitTableScan(TableScan *node, EState *estate, int eflags);
extern TupleTableSlot *ExecTableScan(TableScanState *node);
extern void ExecEndTableScan(TableScanState *node);
extern void ExecReScanTable(TableScanState *node);
extern void ExecEagerFreeTableScan(TableScanState *node);

#endif
