/*-------------------------------------------------------------------------
 *
 * nodeBitmapIndexscan.h
 *
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/executor/nodeBitmapIndexscan.h,v 1.5 2008/01/01 19:45:57 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODEBITMAPINDEXSCAN_H
#define NODEBITMAPINDEXSCAN_H

#include "nodes/execnodes.h"

extern int	ExecCountSlotsBitmapIndexScan(BitmapIndexScan *node);
extern BitmapIndexScanState *ExecInitBitmapIndexScan(BitmapIndexScan *node, EState *estate, int eflags);
extern Node *MultiExecBitmapIndexScan(BitmapIndexScanState *node);
extern void ExecEndBitmapIndexScan(BitmapIndexScanState *node);
extern void ExecBitmapIndexReScan(BitmapIndexScanState *node, ExprContext *exprCtxt);

enum
{
    GPMON_BITMAPINDEXSCAN_RESCAN = GPMON_QEXEC_M_NODE_START,
    GPMON_BITMAPINDEXSCAN_TOTAL
};

static inline gpmon_packet_t * GpmonPktFromBitmapIndexScanState(BitmapIndexScanState *node)
{
	return &((IndexScanState*)node)->ss.ps.gpmon_pkt;
}

#endif   /* NODEBITMAPINDEXSCAN_H */
