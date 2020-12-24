/*-------------------------------------------------------------------------
 *
 * nodeSplitUpdate.h
 *        Prototypes for nodeSplitUpdate.
 *
 * Portions Copyright (c) 2012, EMC Corp.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/executor/nodeSplitUpdate.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef NODESplitUpdate_H
#define NODESplitUpdate_H

extern SplitUpdateState* ExecInitSplitUpdate(SplitUpdate *node, EState *estate, int eflags);
extern void ExecEndSplitUpdate(SplitUpdateState *node);

#endif   /* NODESplitUpdate_H */

