/*-------------------------------------------------------------------------
 *
 * nodeSplitUpdate.h
 *        Prototypes for nodeSplitUpdate.
 *
 * Portions Copyright (c) 2012, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/executor/nodeSplitUpdate.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef NODESplitUpdate_H
#define NODESplitUpdate_H

extern void ExecSplitUpdateExplainEnd(PlanState *planstate, struct StringInfoData *buf);
extern TupleTableSlot* ExecSplitUpdate(SplitUpdateState *node);
extern SplitUpdateState* ExecInitSplitUpdate(SplitUpdate *node, EState *estate, int eflags);
extern void ExecEndSplitUpdate(SplitUpdateState *node);
extern int ExecCountSlotsSplitUpdate(SplitUpdate *node);

extern void initGpmonPktForSplitUpdate(Plan *planNode, gpmon_packet_t *gpmon_pkt, EState *estate);

#endif   /* NODESplitUpdate_H */

