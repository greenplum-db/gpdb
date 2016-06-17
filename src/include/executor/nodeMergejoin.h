/*-------------------------------------------------------------------------
 *
 * nodeMergejoin.h
 *
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/executor/nodeMergejoin.h,v 1.26 2008/01/01 19:45:57 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODEMERGEJOIN_H
#define NODEMERGEJOIN_H

#include "nodes/execnodes.h"

extern int	ExecCountSlotsMergeJoin(MergeJoin *node);
extern MergeJoinState *ExecInitMergeJoin(MergeJoin *node, EState *estate, int eflags);
extern TupleTableSlot *ExecMergeJoin(MergeJoinState *node);
extern void ExecEndMergeJoin(MergeJoinState *node);
extern void ExecReScanMergeJoin(MergeJoinState *node, ExprContext *exprCtxt);
extern void ExecEagerFreeMergeJoin(MergeJoinState *node);

enum {
	GPMON_MERGEJOIN_INNERTUPLE = GPMON_QEXEC_M_NODE_START,
	GPMON_MERGEJOIN_OUTERTUPLE,
	GPMON_MERGEJOIN_TOTAL,
};

static inline gpmon_packet_t *GpmonPktFromMergeJoinState(MergeJoinState *node)
{
	return &node->js.ps.gpmon_pkt;
}
#endif   /* NODEMERGEJOIN_H */
