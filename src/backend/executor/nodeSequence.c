/*
 * nodeSequence.c
 *   Routines to handle Sequence node.
 *
 * Portions Copyright (c) 2012 - present, EMC/Greenplum
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodeSequence.c
 *
 * Sequence node contains a list of subplans, which will be processed in the 
 * order of left-to-right. Result tuples from the last subplan will be outputted
 * as the results of the Sequence node.
 *
 * Sequence does not make use of its left and right subtrees, and instead it
 * maintains a list of subplans explicitly.
 */

#include "postgres.h"

#include "executor/nodeSequence.h"
#include "executor/executor.h"
#include "miscadmin.h"

SequenceState *
ExecInitSequence(Sequence *node, EState *estate, int eflags)
{
	SequenceState *sequenceState;
	PlanState  *lastPlanState;

	/* Check for unsupported flags */
	Assert(!(eflags & EXEC_FLAG_MARK));

	/* Sequence should not contain 'qual'. */
	Assert(node->plan.qual == NIL);

	sequenceState = makeNode(SequenceState);
	sequenceState->ps.plan = (Plan *)node;
	sequenceState->ps.state = estate;
	sequenceState->ps.ExecProcNode = ExecSequence;

	int numSubplans = list_length(node->subplans);
	Assert(numSubplans >= 1);
	sequenceState->subplans = (PlanState **)palloc0(numSubplans * sizeof(PlanState *));
	sequenceState->numSubplans = numSubplans;
	
	/* Initialize subplans */
	ListCell *lc;
	int no = 0;
	foreach (lc, node->subplans)
	{
		Plan *subplan = (Plan *)lfirst(lc);
		Assert(subplan != NULL);
		Assert(no < numSubplans);
		
		sequenceState->subplans[no] = ExecInitNode(subplan, estate, eflags);
		no++;
	}

	sequenceState->initState = true;
	
	/* Sequence does not need projection. */
	sequenceState->ps.ps_ProjInfo = NULL;

	/*
	 * Initialize result type. We will pass through the last child slot.
	 */
	lastPlanState = sequenceState->subplans[numSubplans - 1];
	ExecInitResultTypeTL(&sequenceState->ps);
	sequenceState->ps.resultopsset = true;
	sequenceState->ps.resultops = ExecGetResultSlotOps(lastPlanState,
													   &lastPlanState->resultopsfixed);

	return sequenceState;
}

/*
 * completeSubplan
 *   Execute a given subplan to completion.
 *
 * The outputs from the given subplan will be discarded.
 */
static void
completeSubplan(PlanState *subplan)
{
	while (ExecProcNode(subplan) != NULL)
	{
	}
}

TupleTableSlot *
ExecSequence(PlanState *pstate)
{
	SequenceState *node = castNode(SequenceState, pstate);
	/*
	 * If no subplan has been executed yet, execute them here, except for
	 * the last subplan.
	 */
	if (node->initState)
	{
		for(int no = 0; no < node->numSubplans - 1; no++)
		{
			completeSubplan(node->subplans[no]);

			CHECK_FOR_INTERRUPTS();
		}

		node->initState = false;
	}

	Assert(!node->initState);

	PlanState *lastPlan = node->subplans[node->numSubplans - 1];
	TupleTableSlot *result = ExecProcNode(lastPlan);

	/*
	 * Return the tuple as returned by the subplan as-is. We do
	 * NOT make use of the result slot that was set up in
	 * ExecInitSequence, because there's no reason to.
	 */
	return result;
}

void
ExecEndSequence(SequenceState *node)
{
	/* shutdown subplans */
	for(int no = 0; no < node->numSubplans; no++)
	{
		Assert(node->subplans[no] != NULL);
		ExecEndNode(node->subplans[no]);
	}
}

void
ExecReScanSequence(SequenceState *node)
{
	for (int i = 0; i < node->numSubplans; i++)
	{
		PlanState  *subnode = node->subplans[i];

		/*
		 * ExecReScan doesn't know about my subplans, so I have to do
		 * changed-parameter signaling myself.
		 */
		if (node->ps.chgParam != NULL)
		{
			UpdateChangedParamSet(subnode, node->ps.chgParam);
		}

		/*
		 * Always rescan the inputs immediately, to ensure we can pass down
		 * any outer tuple that might be used in index quals.
		 */
		ExecReScan(subnode);
	}

	node->initState = true;
}

void
ExecSquelchSequence(SequenceState *node)
{
	for (int i = 0; i < node->numSubplans; i++)
		ExecSquelchNode(node->subplans[i]);
}
