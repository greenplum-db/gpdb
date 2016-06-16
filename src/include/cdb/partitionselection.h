/*--------------------------------------------------------------------------
 *
 * partitionselection.h
 *	 Definitions and API functions for partitionselection.c
 *
 * Copyright (c) Pivotal Inc.
 *
 *--------------------------------------------------------------------------
 */
#ifndef PARTITIONSELECTION_H
#define PARTITIONSELECTION_H

#include "executor/tuptable.h"
#include "nodes/relation.h"
#include "nodes/execnodes.h"

/* ----------------
 * PartitionConstraints node
 * used during PartitionSelector
 * ----------------
 */
typedef struct PartitionConstraints
{
	NodeTag type;
	PartitionRule * pRule;
	bool defaultPart;
	Const *lowerBound;
	Const *upperBound;
	bool lbInclusive;
	bool upInclusive;
	bool lbOpen;
	bool upOpen;
} PartitionConstraints;

/* ----------------
 * SelectedParts node
 * This is the result of partition selection. It has a list of leaf part oids
 * and the corresponding ScanIds to which they should be propagated
 * ----------------
 */
typedef struct SelectedParts
{
	List *partOids;
	List *scanIds;
} SelectedParts;

extern PartitionSelectorState *initPartitionSelection(bool isRunTime, PartitionSelector *node, EState *estate);
extern void getPartitionNodeAndAccessMethod(Oid rootOid, List *partsMetadata, MemoryContext memoryContext,
											PartitionNode **partsAndRules, PartitionAccessMethods **accessMethods);
extern SelectedParts *processLevel(PartitionSelectorState *node, int level, TupleTableSlot *inputTuple);
extern SelectedParts *static_part_selection(PartitionSelector *ps);

extern AttrNumber *varattnos_map(TupleDesc oldtd, TupleDesc newtd);
extern void change_varattnos_of_a_node(Node *node, const AttrNumber *newattno);
extern void change_varattnos_of_a_varno(Node *node, const AttrNumber *newattno, Index varno);

#endif   /* PARTITIONSELECTION_H */
