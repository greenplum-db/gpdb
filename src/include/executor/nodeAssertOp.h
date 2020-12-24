/*-------------------------------------------------------------------------
 *
 * nodeAssertOp.h
 *	  Prototypes for nodeAssertOp.
 *
 * Portions Copyright (c) 2012, EMC Corp.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/executor/nodeAssertOp.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef NODEASSERTOP_H
#define NODEASSERTOP_H

extern TupleTableSlot* ExecAssertOp(AssertOpState *node);
extern AssertOpState* ExecInitAssertOp(AssertOp *node, EState *estate, int eflags);
extern void ExecEndAssertOp(AssertOpState *node);
extern void ExecReScanAssertOp(AssertOpState *node);

#endif   /* NODEASSERTOP_H */


