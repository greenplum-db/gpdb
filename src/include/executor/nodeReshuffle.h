/*-------------------------------------------------------------------------
 *
 * nodeReshuffle.h
 *
 *
 *
 * Portions Copyright (c) 2012, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 * src/include/executor/nodeReshuffle.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODERESHUFFLE_H
#define NODERESHUFFLE_H

#include "nodes/execnodes.h"

extern TupleTableSlot *ExecReshuffle(ReshuffleState *node);
extern ReshuffleState *ExecInitReshuffle(Reshuffle *node, EState *estate, int eflags);
extern void ExecEndReshuffle(ReshuffleState *node);
extern void ExecReScanReshuffle(ReshuffleState *node);

#endif   /* NODERESHUFFLE_H */
