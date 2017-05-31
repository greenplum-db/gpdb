/*
 * nodeSequence.h
 *    header file for nodeSequence.c.
 *
 * Copyright (c) 2012 - present, EMC/Greenplum
 */
#ifndef NODESEQUENCE_H
#define NODESEQUENCE_H

#include "executor/tuptable.h"
#include "nodes/execnodes.h"

extern SequenceState *ExecInitSequence(Sequence *node, EState *estate, int eflags);
extern TupleTableSlot *ExecSequence(SequenceState *node);
extern void ExecReScanSequence(SequenceState *node, ExprContext *exprCtxt);
extern void ExecEndSequence(SequenceState *node);
extern int ExecCountSlotsSequence(Sequence *node);

static inline gpmon_packet_t * GpmonPktFromSequenceState(SequenceState *node)
{
	return &node->ps.gpmon_pkt;
}

#endif
