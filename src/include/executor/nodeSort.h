/*-------------------------------------------------------------------------
 *
 * nodeSort.h
 *
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/executor/nodeSort.h,v 1.24 2008/01/01 19:45:57 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODESORT_H
#define NODESORT_H

#include "nodes/execnodes.h"

extern int	ExecCountSlotsSort(Sort *node);
extern SortState *ExecInitSort(Sort *node, EState *estate, int eflags);
extern struct TupleTableSlot *ExecSort(SortState *node);
extern void ExecEndSort(SortState *node);
extern void ExecSortMarkPos(SortState *node);
extern void ExecSortRestrPos(SortState *node);
extern void ExecReScanSort(SortState *node, ExprContext *exprCtxt);
extern void ExecEagerFreeSort(SortState *node);

enum
{
	GPMON_SORT_MEMORY_BYTE = GPMON_QEXEC_M_NODE_START,
	GPMON_SORT_SPILLTUPLE,
	GPMON_SORT_SPILLBYTE,
	GPMON_SORT_SPILLPASS,
	GPMON_SORT_CURRSPILLPASS_TUPLE,
	GPMON_SORT_CURRSPILLPASS_BYTE,
	GPMON_SORT_TOTAL,
};

static inline gpmon_packet_t * GpmonPktFromSortState(SortState *node)
{
	return &node->ss.ps.gpmon_pkt;
}

#endif   /* NODESORT_H */
