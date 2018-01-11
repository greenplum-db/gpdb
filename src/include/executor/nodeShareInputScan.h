/*-------------------------------------------------------------------------
 *
 * nodeShareInputScan.h
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/include/executor/nodeShareInputScan.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODESHAREINPUTSCAN_H
#define NODESHAREINPUTSCAN_H

#include "nodes/execnodes.h"
extern int ExecCountSlotsShareInputScan(ShareInputScan* node);
extern ShareInputScanState *ExecInitShareInputScan(ShareInputScan *node, EState *estate, int eflags);
extern TupleTableSlot *ExecShareInputScan(ShareInputScanState *node);
extern void ExecEndShareInputScan(ShareInputScanState *node);
extern void ExecShareInputScanReScan(ShareInputScanState *node, ExprContext *exprCtxt);
extern void ExecEagerFreeShareInputScan(ShareInputScanState *node);

extern void ExecSliceDependencyShareInputScan(ShareInputScanState *node);

#endif   /* NODESHAREINPUTSCAN_H */
