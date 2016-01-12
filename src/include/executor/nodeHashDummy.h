/*-------------------------------------------------------------------------
 *
 * nodeHashDummyDummy.h
 *	  prototypes for nodeHashDummyDummy.c
 *
 * Portions Copyright (c) 2015 Pivotal Inc.
 *-------------------------------------------------------------------------
 */
#ifndef NODEHASHDUMMY_H
#define NODEHASHDUMMY_H

#include "nodes/execnodes.h"
#include "executor/hashjoin.h"  /* for HJTUPLE_OVERHEAD */
#include "access/memtup.h"

extern int	ExecCountSlotsHashDummy(HashDummy *node);
extern HashDummyState *ExecInitHashDummy(HashDummy *node, EState *estate, int eflags);
extern struct TupleTableSlot *ExecHashDummy(HashDummyState *node);
extern void ExecEndHashDummy(HashDummyState *node);
extern void ExecReScanHashDummy(HashDummyState *node, ExprContext *exprCtxt);

#endif   /* NODEHASHDUMMY_H */
