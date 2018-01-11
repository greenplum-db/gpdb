/*-------------------------------------------------------------------------
 *
 * nodeRepeat.h
 *
 * Portions Copyright (c) 2008 - present, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/executor/nodeRepeat.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef NODEREPEAT_H
#define NODEREPEAT_H

#include "nodes/execnodes.h"

extern TupleTableSlot *ExecRepeat(RepeatState *repeatstate);
extern RepeatState *ExecInitRepeat(Repeat *node, EState *estate, int eflags);
extern int ExecCountSlotsRepeat(Repeat *node);
extern void ExecEndRepeat(RepeatState *node);
extern void ExecReScanRepeat(RepeatState *node, ExprContext *exprCtxt);

#endif
