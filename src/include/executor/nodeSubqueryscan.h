/*-------------------------------------------------------------------------
 *
 * nodeSubqueryscan.h
 *
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/executor/nodeSubqueryscan.h,v 1.15 2008/01/01 19:45:57 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODESUBQUERYSCAN_H
#define NODESUBQUERYSCAN_H

#include "nodes/execnodes.h"

extern int	ExecCountSlotsSubqueryScan(SubqueryScan *node);
extern SubqueryScanState *ExecInitSubqueryScan(SubqueryScan *node, EState *estate, int eflags);
extern TupleTableSlot *ExecSubqueryScan(SubqueryScanState *node);
extern void ExecEndSubqueryScan(SubqueryScanState *node);
extern void ExecSubqueryReScan(SubqueryScanState *node, ExprContext *exprCtxt);

enum {
	GPMON_SUBQUERYSCAN_RESCAN = GPMON_QEXEC_M_NODE_START, 
	GPMON_SUBQUERYSCAN_TOTAL
};

static inline gpmon_packet_t * GpmonPktFromSubqueryScanState(SubqueryScanState *node)
{
	return &node->ss.ps.gpmon_pkt;
}


#endif   /* NODESUBQUERYSCAN_H */
