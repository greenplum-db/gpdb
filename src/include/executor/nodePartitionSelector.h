/*-------------------------------------------------------------------------
 *
 * nodePartitionSelector.h
 *	  implement the execution of PartitionSelector for selecting partition
 *	  Oids based on a given set of predicates. It works for both constant
 *	  partition elimination and join partition elimination
 *
 * Copyright (c) 2014-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/executor/nodePartitionSelector.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef NODEPARTITIONSELECTOR_H
#define NODEPARTITIONSELECTOR_H

extern TupleTableSlot* ExecPartitionSelector(PartitionSelectorState *node);
extern PartitionSelectorState* ExecInitPartitionSelector(PartitionSelector *node, EState *estate, int eflags);
extern void ExecEndPartitionSelector(PartitionSelectorState *node);
extern void ExecReScanPartitionSelector(PartitionSelectorState *node, ExprContext *exprCtxt);
extern int ExecCountSlotsPartitionSelector(PartitionSelector *node);
extern void initGpmonPktForPartitionSelector(Plan *planNode, gpmon_packet_t *gpmon_pkt, EState *estate);

#endif   /* NODEPARTITIONSELECTOR_H */

