/*-------------------------------------------------------------------------
 *
 * nodeBitmapAppendOnlyscan.h
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 * Portions Copyright (c) 2008-2009, Greenplum Inc.
 *
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODEBITMAPAPPENDONLYSCAN_H
#define NODEBITMAPAPPENDONLYSCAN_H

#include "nodes/execnodes.h"

extern int	ExecCountSlotsBitmapAppendOnlyScan(BitmapAppendOnlyScan *node);
extern BitmapAppendOnlyScanState *ExecInitBitmapAppendOnlyScan(BitmapAppendOnlyScan *node, EState *estate, int eflags);
extern TupleTableSlot *ExecBitmapAppendOnlyScan(BitmapAppendOnlyScanState *node);
extern void ExecEndBitmapAppendOnlyScan(BitmapAppendOnlyScanState *node);
extern void ExecBitmapAppendOnlyReScan(BitmapAppendOnlyScanState *node, ExprContext *exprCtxt);
extern void ExecEagerFreeBitmapAppendOnlyScan(BitmapAppendOnlyScanState *node);

static inline gpmon_packet_t * GpmonPktFromBitmapAppendOnlyScanState(BitmapAppendOnlyScanState *node)
{
	return &node->ss.ps.gpmon_pkt;
}

#endif   /* NODEBITMAPAPPENDONLYSCAN_H */

