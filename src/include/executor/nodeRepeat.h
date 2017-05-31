/*-------------------------------------------------------------------------
 *
 * nodeRepeat.h
 *
 * Copyright (c) 2008 - present, Greenplum Inc.
 *
 * IDENTIFICATION:
 *     $Id$
 *
 * $File$
 * $Change$
 * $Author$
 * $DateTime$
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

static inline gpmon_packet_t * GpmonPktFromRepeatState(RepeatState *node)
{
	return &node->ps.gpmon_pkt;
}

#endif
