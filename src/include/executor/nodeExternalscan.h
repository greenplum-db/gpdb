/*-------------------------------------------------------------------------
 *
 * nodeExternalscan.h
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/executor/nodeExternalscan.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODEEXTERNALSCAN_H
#define NODEEXTERNALSCAN_H

#include "nodes/execnodes.h"

extern int	ExecCountSlotsExternalScan(ExternalScan *node);
extern ExternalScanState *ExecInitExternalScan(ExternalScan *node, EState *estate, int eflags);
extern TupleTableSlot *ExecExternalScan(ExternalScanState *node);
extern void ExecEndExternalScan(ExternalScanState *node);
extern void ExecStopExternalScan(ExternalScanState *node);
extern void ExecExternalReScan(ExternalScanState *node, ExprContext *exprCtxt);
extern void ExecEagerFreeExternalScan(ExternalScanState *node);

#endif   /* NODEEXTERNALSCAN_H */
