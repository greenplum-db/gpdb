/*-------------------------------------------------------------------------
 *
 * nodeResult.h
 *
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/executor/nodeResult.h,v 1.24 2007/02/15 03:07:13 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODERESULT_H
#define NODERESULT_H

#include "nodes/execnodes.h"

extern int	ExecCountSlotsResult(Result *node);
extern ResultState *ExecInitResult(Result *node, EState *estate, int eflags);
extern TupleTableSlot *ExecResult(ResultState *node);
extern void ExecEndResult(ResultState *node);
extern void ExecResultMarkPos(ResultState *node);
extern void ExecResultRestrPos(ResultState *node);
extern void ExecReScanResult(ResultState *node, ExprContext *exprCtxt);

enum {
	GPMON_RESULT_TOTAL = GPMON_QEXEC_M_NODE_START,
};

static inline gpmon_packet_t * GpmonPktFromResultState(ResultState *node)
{
	return &node->ps.gpmon_pkt;
}
#endif   /* NODERESULT_H */
