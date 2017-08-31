/*-------------------------------------------------------------------------
 *
 * nodeRowTrigger.h
 *	  Prototypes for nodeRowTriggerOp.
 *
 * Portions Copyright (c) 2012, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/executor/nodeRowTrigger.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef NODEROWTRIGGER_H
#define NODEROWTRIGGER_H

extern void ExecRowTriggerExplainEnd(PlanState *planstate, struct StringInfoData *buf);
extern TupleTableSlot* ExecRowTrigger(RowTriggerState *node);
extern RowTriggerState* ExecInitRowTrigger(RowTrigger *node, EState *estate, int eflags);
extern void ExecEndRowTrigger(RowTriggerState *node);
extern int ExecCountSlotsRowTrigger(RowTrigger *node);

extern void initGpmonPktForRowTrigger(Plan *planNode, gpmon_packet_t *gpmon_pkt, EState *estate);

#endif   /* NODEROWTRIGGER_H */


