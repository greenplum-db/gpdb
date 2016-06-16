/*-------------------------------------------------------------------------
 *
 * nodeSeqscan.h
 *
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/executor/nodeSeqscan.h,v 1.26 2008/01/01 19:45:57 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODESEQSCAN_H
#define NODESEQSCAN_H

#include "nodes/execnodes.h"

extern int	ExecCountSlotsSeqScan(SeqScan *node);
extern SeqScanState *ExecInitSeqScan(SeqScan *node, EState *estate, int eflags);
extern TupleTableSlot *ExecSeqScan(SeqScanState *node);
extern void ExecEndSeqScan(SeqScanState *node);
extern void ExecSeqMarkPos(SeqScanState *node);
extern void ExecSeqRestrPos(SeqScanState *node);
extern void ExecSeqReScan(SeqScanState *node, ExprContext *exprCtxt);
extern void ExecEagerFreeSeqScan(SeqScanState *node);

/* Gpmon stuff */
enum 
{
	GPMON_SEQSCAN_PAGE = GPMON_QEXEC_M_NODE_START,
	GPMON_SEQSCAN_RESTOREPOS,
	GPMON_SEQSCAN_RESCAN,
	GPMON_SEQSCAN_TOTAL,
};

static inline gpmon_packet_t * GpmonPktFromSeqScanState(SeqScanState *node)
{
	return &node->ps.gpmon_pkt;
}

#endif   /* NODESEQSCAN_H */
