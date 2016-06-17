/*-------------------------------------------------------------------------
 *
 * nodeUnique.h
 *
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/executor/nodeUnique.h,v 1.24 2008/01/01 19:45:57 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODEUNIQUE_H
#define NODEUNIQUE_H

#include "nodes/execnodes.h"

extern int	ExecCountSlotsUnique(Unique *node);
extern UniqueState *ExecInitUnique(Unique *node, EState *estate, int eflags);
extern TupleTableSlot *ExecUnique(UniqueState *node);
extern void ExecEndUnique(UniqueState *node);
extern void ExecReScanUnique(UniqueState *node, ExprContext *exprCtxt);

enum {
	GPMON_UNIQUE_TOTAL = GPMON_QEXEC_M_NODE_START,
};

static inline gpmon_packet_t * GpmonPktFromUniqueState(UniqueState *node) 
{
	return &node->ps.gpmon_pkt;
}
#endif   /* NODEUNIQUE_H */
