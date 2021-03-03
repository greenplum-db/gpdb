/*-------------------------------------------------------------------------
 *
 * nodePartitionSelector.h
 *	  implement the execution of PartitionSelector for selecting partition
 *	  Oids based on a given set of predicates. It works for both constant
 *	  partition elimination and join partition elimination
 *
 * Copyright (c) 2014-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/executor/nodePartitionSelector.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef NODEPARTITIONSELECTOR_H
#define NODEPARTITIONSELECTOR_H

extern PartitionSelectorState* ExecInitPartitionSelector(PartitionSelector *node, EState *estate, int eflags);
extern void ExecEndPartitionSelector(PartitionSelectorState *node);
extern void ExecReScanPartitionSelector(PartitionSelectorState *node);

#endif   /* NODEPARTITIONSELECTOR_H */

