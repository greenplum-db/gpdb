/*-------------------------------------------------------------------------
 *
 * nodeSplitUpdate.c
 *	  Implementation of nodeSplitUpdate.
 *
 * Portions Copyright (c) 2012, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodeSplitUpdate.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "miscadmin.h"

#include "cdb/cdbpartition.h"
#include "commands/tablecmds.h"
#include "executor/execDML.h"
#include "executor/instrument.h"
#include "executor/nodeSplitUpdate.h"

#include "utils/memutils.h"

/* Splits an update tuple into a DELETE/INSERT tuples. */
void
SplitTupleTableSlot(List *targetList, SplitUpdate *plannode, SplitUpdateState *node, Datum *values, bool *nulls);

/* Number of slots used by SplitUpdate node */
#define SPLITUPDATE_NSLOTS 3
/* Memory used by node */
#define SPLITUPDATE_MEM 1

/*
 * Estimated Memory Usage of Split DML Node.
 * */
void
ExecSplitUpdateExplainEnd(PlanState *planstate, struct StringInfoData *buf)
{
	/* Add memory size of context */
	planstate->instrument->execmemused += SPLITUPDATE_MEM;
}

/* Split TupleTableSlot into a DELETE and INSERT TupleTableSlot */
void
SplitTupleTableSlot(List *targetList, SplitUpdate *plannode, SplitUpdateState *node, Datum *values, bool *nulls)
{
	ListCell *element = NULL;
	ListCell *deleteAtt = plannode->deleteColIdx->head;
	ListCell *insertAtt = plannode->insertColIdx->head;

	Datum *delete_values = slot_get_values(node->deleteTuple);
	bool *delete_nulls = slot_get_isnull(node->deleteTuple);
	Datum *insert_values = slot_get_values(node->insertTuple);
	bool *insert_nulls = slot_get_isnull(node->insertTuple);

	/* Iterate through new TargetList and match old and new values. The action is also added in this containsTuple. */
	foreach (element, targetList)
	{
		TargetEntry *tle = lfirst(element);
		int resno = tle->resno-1;

		if (IsA(tle->expr, DMLActionExpr))
		{
			/* Set the corresponding action to the new tuples. */
			delete_values[resno] = Int32GetDatum((int)DML_DELETE);
			delete_nulls[resno] = false;

			insert_values[resno] = Int32GetDatum((int)DML_INSERT);
			insert_nulls[resno] = false;
		}
		else if (((int)tle->resno) < plannode->ctidColIdx)
		{
			/* Old and new values */
			delete_values[resno] = values[deleteAtt->data.int_value-1];
			delete_nulls[resno] = nulls[deleteAtt->data.int_value-1];

			insert_values[resno] = values[insertAtt->data.int_value-1];
			insert_nulls[resno] = nulls[insertAtt->data.int_value-1];

			deleteAtt = deleteAtt->next;
			insertAtt = insertAtt->next;
		}
		else
		{
			/* `Resjunk' values */
			delete_values[resno] = values[((Var *)tle->expr)->varattno-1];
			delete_nulls[resno] = nulls[((Var *)tle->expr)->varattno-1];

			insert_values[resno] = values[((Var *)tle->expr)->varattno-1];
			insert_nulls[resno] = nulls[((Var *)tle->expr)->varattno-1];
		}
	}
}

/**
 * Splits every TupleTableSlot into two TupleTableSlots: DELETE and INSERT.
 */
TupleTableSlot*
ExecSplitUpdate(SplitUpdateState *node)
{
	PlanState *outerNode = outerPlanState(node);
	SplitUpdate *plannode = (SplitUpdate *) node->ps.plan;

	TupleTableSlot *slot = NULL;
	TupleTableSlot *result = NULL;

	Assert(outerNode != NULL);

	/* Returns INSERT TupleTableSlot. */
	if (!node->processInsert)
	{
		result = node->insertTuple;

		node->processInsert = true;
	}
	else
	{
		/* Creates both TupleTableSlots. Returns DELETE TupleTableSlots.*/
		slot = ExecProcNode(outerNode);

		if (TupIsNull(slot))
		{
			return NULL;
		}

		/* `Split' update into delete and insert */
		slot_getallattrs(slot);
		Datum *values = slot_get_values(slot);
		bool *nulls = slot_get_isnull(slot);

		ExecStoreAllNullTuple(node->deleteTuple);
		ExecStoreAllNullTuple(node->insertTuple);

		SplitTupleTableSlot(plannode->plan.targetlist, plannode, node, values, nulls);

		result = node->deleteTuple;
		node->processInsert = false;

	}

	return result;
}

/*
 * Init SplitUpdate Node. A memory context is created to hold Split Tuples.
 * */
SplitUpdateState*
ExecInitSplitUpdate(SplitUpdate *node, EState *estate, int eflags)
{
	/* Check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK | EXEC_FLAG_REWIND)));

	bool    has_oids = false;

	SplitUpdateState *splitupdatestate;

	splitupdatestate = makeNode(SplitUpdateState);
	splitupdatestate->ps.plan = (Plan *)node;
	splitupdatestate->ps.state = estate;
	splitupdatestate->processInsert = true;

	/*
	 * then initialize outer plan
	 */
	Plan *outerPlan = outerPlan(node);
	outerPlanState(splitupdatestate) = ExecInitNode(outerPlan, estate, eflags);

	ExecInitResultTupleSlot(estate, &splitupdatestate->ps);

	splitupdatestate->insertTuple = ExecInitExtraTupleSlot(estate);
	splitupdatestate->deleteTuple = ExecInitExtraTupleSlot(estate);

	/* New TupleDescriptor for output TupleTableSlots (old_values + new_values, ctid, gp_segment, action).*/
	if (estate->es_result_relation_info != NULL &&
		estate->es_result_relation_info->ri_RelationDesc != NULL &&
		estate->es_result_relation_info->ri_RelationDesc->rd_rel != NULL &&
		estate->es_result_relation_info->ri_RelationDesc->rd_rel->relhasoids)
	{
		has_oids = true;
	}

	TupleDesc tupDesc = ExecTypeFromTL(node->plan.targetlist, has_oids);
	ExecSetSlotDescriptor(splitupdatestate->insertTuple, tupDesc);
	ExecSetSlotDescriptor(splitupdatestate->deleteTuple, tupDesc);

	/*
	 * DML nodes do not project.
	 */
	ExecAssignResultTypeFromTL(&splitupdatestate->ps);
	ExecAssignProjectionInfo(&splitupdatestate->ps, NULL);

	if (estate->es_instrument)
	{
			splitupdatestate->ps.cdbexplainbuf = makeStringInfo();

			/* Request a callback at end of query. */
			splitupdatestate->ps.cdbexplainfun = ExecSplitUpdateExplainEnd;
	}

	initGpmonPktForSplitUpdate((Plan *)node, &splitupdatestate->ps.gpmon_pkt, estate);

	return splitupdatestate;
}

/* Release Resources Requested by SplitUpdate node. */
void
ExecEndSplitUpdate(SplitUpdateState *node)
{
	ExecFreeExprContext(&node->ps);
	ExecClearTuple(node->ps.ps_ResultTupleSlot);
	ExecClearTuple(node->insertTuple);
	ExecClearTuple(node->deleteTuple);
	ExecEndNode(outerPlanState(node));
	EndPlanStateGpmonPkt(&node->ps);
}

/* Return number of TupleTableSlots used by SplitUpdate node.*/
int
ExecCountSlotsSplitUpdate(SplitUpdate *node)
{
	return ExecCountSlotsNode(outerPlan(node)) + SPLITUPDATE_NSLOTS;
}

/* Tracing execution for GP Monitor. */
void
initGpmonPktForSplitUpdate(Plan *planNode, gpmon_packet_t *gpmon_pkt, EState *estate)
{
	Assert(planNode != NULL && gpmon_pkt != NULL && IsA(planNode, SplitUpdate));

	InitPlanNodeGpmonPkt(planNode, gpmon_pkt, estate);
}

