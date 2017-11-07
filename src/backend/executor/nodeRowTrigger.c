/*-------------------------------------------------------------------------
 *
 * nodeRowTriggerOp.c
 *	  Implementation of nodeRowTriggerOp.
 *
 * Portions Copyright (c) 2012, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodeRowTrigger.c
 *
 * The semantics of triggers is the following:
 * After triggers are executed after each tuple is processed, which
 * is not the case for DML statements going through the planner.
 * Before triggers are immediately executed after each tuple is processed.
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "miscadmin.h"

#include "cdb/cdbpartition.h"
#include "commands/tablecmds.h"
#include "executor/nodeRowTrigger.h"
#include "executor/instrument.h"
#include "commands/trigger.h"

/* Optimizer mapping for Row Triggers */
#define GPMD_TRIGGER_ROW 1
#define GPMD_TRIGGER_BEFORE 2
#define GPMD_TRIGGER_INSERT 4
#define GPMD_TRIGGER_DELETE 8
#define GPMD_TRIGGER_UPDATE 16

/* Number of slots used by node.*/
#define ROWTRIGGER_NSLOTS 4
/* Memory used by RowTrigger */
#define ROWTRIGGER_MEM 1

/* Executes the corresponding (INSERT/DELETE/UPDATE) ExecTrigger. */
static HeapTuple
ExecTriggers(EState *estate, ResultRelInfo *relinfo,
				HeapTuple originalTuple,
				HeapTuple finalTuple, int eventFlags, bool *processTuple);

/* Executes Before and After Update triggers. */
static HeapTuple
ExecUpdateTriggers(EState *estate, ResultRelInfo *relinfo,
				TriggerDesc *trigdesc, int ntrigs, int *tgindx, HeapTuple newtuple,
				HeapTuple trigtuple /*old*/, TriggerEvent eventFlags);

/* Executes Before and After Insert triggers. */
static HeapTuple
ExecInsertTriggers(EState *estate, ResultRelInfo *relinfo,
				TriggerDesc *trigdesc, int ntrigs, int *tgindx,  HeapTuple	trigtuple, TriggerEvent eventFlags);

/* Executes Before and After Delete triggers. */
static bool
ExecDeleteTriggers(EState *estate, ResultRelInfo *relinfo,
				TriggerDesc *trigdesc, int ntrigs, int *tgindx,  HeapTuple	trigtuple, TriggerEvent eventFlags);

/* Stores the matching attribute values in a TupleTableSlot.
 * This is used to generate a TupleTableSlot for the new and old values. */
static void
StoreTupleForTrigger(TupleTableSlot *slot, Datum *values, bool *nulls, ListCell *attr);

/* Constructs output TupleTableSlot */
static void
ConstructNewTupleTableSlot(HeapTuple newtuple, TupleTableSlot *triggerTuple, ListCell *attr, Datum *values, bool *nulls);

/* Obtains the Heap/Mem tuples to process during the trigger */
static void
AssignTuplesForTriggers(void **newTuple, void **oldTuple,  RowTrigger *plannode,
					RowTriggerState *node,  Relation relation);

/* Initializes event trigger flags and relation to TriggerData struct */
static void
InitTriggerData(TriggerData *triggerData, TriggerEvent eventFlags, Relation relation);


/*
 * Estimated Memory Usage of RowTrigger Node.
 * */
void
ExecRowTriggerExplainEnd(PlanState *planstate, struct StringInfoData *buf)
{
	planstate->instrument->execmemused += ROWTRIGGER_MEM;
}

/* Sets common initialization parameters across types of triggers. */
static void
InitTriggerData(TriggerData *triggerData, TriggerEvent eventFlags, Relation relation)
{
	triggerData->type = T_TriggerData;
	triggerData->tg_event = eventFlags;
	triggerData->tg_relation = relation;
}

/*
 * Executes Before and After Update Row Triggers.
 * Allows modifying the NEW values of an updated tuple.
 * This function allows raising exceptions.
 *
 * */
HeapTuple
ExecUpdateTriggers(EState *estate, ResultRelInfo *relinfo,
				TriggerDesc *trigdesc, int ntrigs, int *tgindx, HeapTuple newtuple,
				HeapTuple trigtuple /*old*/, TriggerEvent eventFlags)
{
	TriggerData triggerData;

	HeapTuple	oldtuple;
	HeapTuple	intuple = newtuple;

	InitTriggerData(&triggerData, eventFlags, relinfo->ri_RelationDesc);

	/* Executes all update triggers one by one. The resulting tuple from a
	* trigger is given to the following one */
	for (int i = 0; i < ntrigs; i++)
	{
		Trigger    *trigger = &trigdesc->triggers[tgindx[i]];

		if (!trigger->tgenabled)
		{
			continue;
		}

		triggerData.tg_trigtuple = trigtuple;
		triggerData.tg_newtuple = oldtuple = newtuple;
		triggerData.tg_trigtuplebuf = InvalidBuffer;
		triggerData.tg_newtuplebuf = InvalidBuffer;
		triggerData.tg_trigger = trigger;

		newtuple = ExecCallTriggerFunc(&triggerData,
									   tgindx[i],
									   relinfo->ri_TrigFunctions,
									   relinfo->ri_TrigInstrument,
									   GetPerTupleMemoryContext(estate));

		if (oldtuple != newtuple && oldtuple != intuple)
		{
			heap_freetuple(oldtuple);
		}

		if (newtuple == NULL)
		{
			break;
		}
	}

	return newtuple;
}

/*
 * Executes Before and After Insert Row Triggers.
 * Allows modifying the NEW values of an inserted tuple.
 * This function allows raising exceptions.
 * */
HeapTuple
ExecInsertTriggers(EState *estate, ResultRelInfo *relinfo,
				TriggerDesc *trigdesc, int ntrigs, int *tgindx,
				HeapTuple	trigtuple, TriggerEvent eventFlags)
{
	HeapTuple	newtuple = trigtuple;
	HeapTuple	oldtuple;
	TriggerData triggerData;

	InitTriggerData(&triggerData, eventFlags, relinfo->ri_RelationDesc);

	triggerData.tg_newtuple = NULL;
	triggerData.tg_newtuplebuf = InvalidBuffer;

	/* Executes all insert triggers one by one. The resulting tuple from a
	 * trigger is given to the following one */
	for (int i = 0; i < ntrigs; i++)
	{
		Trigger    *trigger = &trigdesc->triggers[tgindx[i]];

		if (!trigger->tgenabled)
		{
			continue;
		}

		triggerData.tg_trigtuple = oldtuple = newtuple;
		triggerData.tg_trigtuplebuf = InvalidBuffer;
		triggerData.tg_trigger = trigger;

		newtuple = ExecCallTriggerFunc(&triggerData,
									   tgindx[i],
									   relinfo->ri_TrigFunctions,
									   relinfo->ri_TrigInstrument,
									   GetPerTupleMemoryContext(estate));

		if (oldtuple != newtuple && oldtuple != trigtuple)
		{
			heap_freetuple(oldtuple);
		}

		if (newtuple == NULL)
		{
			break;
		}
	}
	return newtuple;
}

/*
 * Executes Before and After Delete Row Triggers.
 * This trigger can only cancel the deletion of a tuple.
 *
 * If any of the triggers return NULL tuple, this method
 * will return false to indicate that this tuple should not
 * be deleted.
 */
bool
ExecDeleteTriggers(EState *estate, ResultRelInfo *relinfo,
				TriggerDesc *trigdesc, int ntrigs, int *tgindx,
				HeapTuple	trigtuple, TriggerEvent eventFlags)
{
	TriggerData triggerData;
	HeapTuple	newtuple;

	if (trigtuple == NULL)
	{
		return false;
	}

	InitTriggerData(&triggerData, eventFlags, relinfo->ri_RelationDesc);
	triggerData.tg_newtuple = NULL;
	triggerData.tg_newtuplebuf = InvalidBuffer;

	/* Executes all triggers one by one. If a predicate fails the execution
	 * of the rest of the triggers is suspended. */
	for (int i = 0; i < ntrigs; i++)
	{
		Trigger    *trigger = &trigdesc->triggers[tgindx[i]];

		if (!trigger->tgenabled)
		{
			continue;
		}

		triggerData.tg_trigtuple = trigtuple;
		triggerData.tg_trigtuplebuf = InvalidBuffer;
		triggerData.tg_trigger = trigger;

		newtuple = ExecCallTriggerFunc(&triggerData,
									   tgindx[i],
									   relinfo->ri_TrigFunctions,
									   relinfo->ri_TrigInstrument,
									   GetPerTupleMemoryContext(estate));
		if (newtuple == NULL)
		{
			return false;
		}

		if (newtuple != trigtuple)
		{
			heap_freetuple(newtuple);
		}
	}

	return true;
}

/*
 * This function executes the appropriate triggers determined by the given flags.
 * This function also returns the resulting tuple from the trigger.
 * In a delete operation, the value of insertTuple is modified to
 * specify the action to take (DELETE or IGNORE) about that current tuple.
 * */
HeapTuple
ExecTriggers(EState *estate, ResultRelInfo *relinfo,
				HeapTuple originalTuple,
				HeapTuple finalTuple, int eventFlags, bool *processTuple)
{
	TriggerDesc *trigdesc = relinfo->ri_TrigDesc;

	Assert( NULL != trigdesc);

	/* Since GPMD_TRIGGER_INSERT is equal to zero. The result should be negated.
	 * This is not needed for the rest of the flags.*/
	int insert = !(eventFlags & GPMD_TRIGGER_INSERT) ? TRIGGER_EVENT_INSERT:0;
	int delete = (eventFlags & GPMD_TRIGGER_DELETE) ? TRIGGER_EVENT_DELETE:0;
	int update = (eventFlags & GPMD_TRIGGER_UPDATE) ? TRIGGER_EVENT_UPDATE:0;
	int before = (eventFlags & GPMD_TRIGGER_BEFORE) ? TRIGGER_EVENT_BEFORE:0;


	int			ntrigs = 0;
	int		   *tgindx = NULL;

	TriggerEvent triggerType = insert | delete | update;
	TriggerEvent triggerFlags = triggerType | TRIGGER_EVENT_ROW | before;

	if (before)
	{
		ntrigs = trigdesc->n_before_row[triggerType];
		tgindx = trigdesc->tg_before_row[triggerType];
	}
	else
	{
		ntrigs = trigdesc->n_after_row[triggerType];
		tgindx = trigdesc->tg_after_row[triggerType];
	}

	if (delete)
	{
		*processTuple = ExecDeleteTriggers(estate, relinfo, trigdesc, ntrigs, tgindx,
										originalTuple, triggerFlags);
	}
	else if (update)
	{
		originalTuple = ExecUpdateTriggers(estate, relinfo, trigdesc, ntrigs, tgindx,
										originalTuple, finalTuple, triggerFlags);

		if (NULL == originalTuple)
		{
			*processTuple = false;
		}
	}
	else if (!insert)
	{
		originalTuple = ExecInsertTriggers(estate, relinfo, trigdesc, ntrigs, tgindx,
										originalTuple, triggerFlags);
	}
	else
	{
		insist_log(false, "unrecognized trigger type");
	}

	return originalTuple;
}

/* Build new TupleTableSlot given a List of attributes and a slot. */
void
StoreTupleForTrigger(TupleTableSlot *slot, Datum *values, bool *nulls, ListCell *attr)
{

	Assert(slot);
	ExecStoreAllNullTuple(slot);

	slot_getallattrs(slot);
	Datum *new_values = slot_get_values(slot);
	bool *new_nulls = slot_get_isnull(slot);

	for (int attno = 0; attno < slot->tts_tupleDescriptor->natts; attno++)
	{
		new_values[attno] = values[attr->data.int_value-1];
		new_nulls[attno] = nulls[attr->data.int_value-1];

		attr = attr->next;
	}
}
/* Store results from trigger to output tuple */
void
ConstructNewTupleTableSlot(HeapTuple newtuple, TupleTableSlot *triggerTuple, ListCell *attr, Datum *values, bool *nulls)
{
	ExecStoreHeapTuple(newtuple , triggerTuple, InvalidBuffer, true);
	slot_getallattrs(triggerTuple);

	Datum *new_values = slot_get_values(triggerTuple);
	bool *new_nulls = slot_get_isnull(triggerTuple);

	for(int attno = 0; attno < triggerTuple->tts_tupleDescriptor->natts; attno++)
	{
		values[attr->data.int_value-1] = new_values[attno];
		nulls[attr->data.int_value-1] = new_nulls[attno];

		attr = attr->next;
	}
}

/* Fetches Heap Tuples from TupleTableSlots.
 * Updates require extracting the Heap Tuple from the old
 * and new tuple.
 * INSERT and DELETES modify the value of newTuple.
 */
void
AssignTuplesForTriggers(void **newTuple, void **oldTuple,  RowTrigger *plannode,
					RowTriggerState *node,  Relation relation)
{
	/* Check for valid tuples. */
	bool rel_is_heap = RelationIsHeap(relation);
	bool rel_is_aorows = RelationIsAoCols(relation);
	bool rel_is_aocols = RelationIsAoRows(relation);
	bool rel_is_external = RelationIsExternal(relation);

	if (rel_is_aocols || rel_is_external)
	{
		elog(ERROR, "Triggers are not supported on column-oriented or external tables");
	}

	if (rel_is_aorows && !(plannode->eventFlags & GPMD_TRIGGER_INSERT))
	{
		elog(ERROR, "Append only tables only support INSERT triggers");
	}

	if (rel_is_heap)
	{
		if (plannode->newValuesColIdx != NIL)
		{
			*newTuple = ExecFetchSlotHeapTuple(node->newTuple);
		}

		if (plannode->oldValuesColIdx != NIL)
		{
			*oldTuple = ExecFetchSlotHeapTuple(node->oldTuple);
		}
	}
	else
	{
		if (plannode->newValuesColIdx != NIL)
		{
			*newTuple = ExecFetchSlotMemTuple(node->newTuple, !rel_is_aorows);
		}

		if (plannode->oldValuesColIdx != NIL)
		{
			*oldTuple = ExecFetchSlotMemTuple(node->oldTuple, !rel_is_aorows);
		}
	}

	if ( plannode->newValuesColIdx == NIL &&
			plannode->oldValuesColIdx )
	{
		*newTuple = *oldTuple;
		*oldTuple = NULL;
	}
}

/*
 * Exec Function for After and Before Row Triggers.
 * INSERT and UPDATE triggers allow modifying the NEW values of the
 * inserted/modified tuple. All the functions allow raising exceptions
 * or messages.
 * Before DELETE triggers ignore tuples that do not meet
 * a user-defined predicate.
 *
 * TODO: Support After Trigger Update.
 * */
TupleTableSlot*
ExecRowTrigger(RowTriggerState *node)
{
	PlanState *outerNode = outerPlanState(node);
	RowTrigger *plannode = (RowTrigger *) node->ps.plan;

	Assert(NULL != outerNode);

	ResultRelInfo *resultRelInfo = outerNode->state->es_result_relation_info;

	Assert(NULL != resultRelInfo);

	bool processTuple = true;
	TupleTableSlot *slot = NULL;

	/* Loop if DELETE skips a tuple */
	do
	{
		 slot = ExecProcNode(outerNode);

		if (TupIsNull(slot))
		{
			return NULL;
		}

		slot_getallattrs(slot);
		Datum *values = slot_get_values(slot);
		bool *nulls = slot_get_isnull(slot);

		/* Build new TupleTableSlots */
		if (plannode->newValuesColIdx != NIL)
		{
			StoreTupleForTrigger(node->newTuple, values, nulls, plannode->newValuesColIdx->head);
		}

		if (plannode->oldValuesColIdx != NIL)
		{
			StoreTupleForTrigger(node->oldTuple, values, nulls, plannode->oldValuesColIdx->head);
		}

		void *tuple = NULL;
		void *oldTuple = NULL;

		AssignTuplesForTriggers(&tuple, &oldTuple, plannode, node, resultRelInfo->ri_RelationDesc);

		Assert(tuple);

		processTuple = true;

		HeapTuple newtuple = ExecTriggers(outerNode->state, resultRelInfo, tuple,
								oldTuple, plannode->eventFlags, &processTuple);

		/* A tuple does not get processed if a predicate is not satisfied. */
		if (processTuple)
		{
			if (newtuple == NULL)
			{	
				/* Request next tuple to insert if a null new tuple is provided. */
				if (plannode->eventFlags & GPMD_TRIGGER_INSERT)
				{
					slot = NULL;
					processTuple = false;
				}
			}
			else
			{
				/* Update output tuple modified by the trigger. We are only interested in a modified
			 	* tuple for before triggers. */
				if (newtuple != tuple &&
					(plannode->eventFlags & GPMD_TRIGGER_BEFORE))
				{
					/* UPDATE triggers require oldValues */
					List *colidx = plannode->newValuesColIdx;
					if (plannode->eventFlags & GPMD_TRIGGER_UPDATE)
					{
				    		colidx = plannode->oldValuesColIdx;
					}
					ConstructNewTupleTableSlot(newtuple, node->triggerTuple, colidx->head, values, nulls);
				}
			}
		}
	} while (!processTuple);

	return slot;
}

/**
 * Init RowTrigger, which initializes the trigger tuples.
 * */
RowTriggerState*
ExecInitRowTrigger(RowTrigger *node, EState *estate, int eflags)
{
	RowTriggerState *rowTriggerState;
	Plan *outerPlan;
	
	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK | EXEC_FLAG_REWIND)));
	
	rowTriggerState = makeNode(RowTriggerState);
	rowTriggerState->ps.plan = (Plan *)node;
	rowTriggerState->ps.state = estate;

	ExecInitResultTupleSlot(estate, &rowTriggerState->ps);

	rowTriggerState->ps.targetlist = (List *)
		ExecInitExpr((Expr *) node->plan.targetlist,
					 (PlanState *) rowTriggerState);

	/*
	 * then initialize outer plan
	 */
	outerPlan = outerPlan(node);
	outerPlanState(rowTriggerState) = ExecInitNode(outerPlan, estate, eflags);
	
	/*
	 * RowTrigger nodes do not project.
	 */
	ExecAssignResultTypeFromTL(&rowTriggerState->ps);
	ExecAssignProjectionInfo(&rowTriggerState->ps, NULL);

	rowTriggerState->newTuple  = ExecInitExtraTupleSlot(estate);
	rowTriggerState->oldTuple  = ExecInitExtraTupleSlot(estate);
	rowTriggerState->triggerTuple = ExecInitExtraTupleSlot(estate);

	TupleDesc tupDesc =
			estate->es_result_relation_info->ri_RelationDesc->rd_att;

	ExecSetSlotDescriptor(rowTriggerState->newTuple, tupDesc);
	ExecSetSlotDescriptor(rowTriggerState->oldTuple, tupDesc);
	ExecSetSlotDescriptor(rowTriggerState->triggerTuple, tupDesc);

	if (estate->es_instrument && (estate->es_instrument & INSTRUMENT_CDB))
	{
	        rowTriggerState->ps.cdbexplainbuf = makeStringInfo();

	        /* Request a callback at end of query. */
	        rowTriggerState->ps.cdbexplainfun = ExecRowTriggerExplainEnd;
	}

	initGpmonPktForRowTrigger((Plan *)node, &rowTriggerState->ps.gpmon_pkt, estate);

	return rowTriggerState;
}

/* Free Resources Requested by RowTrigger node. */
void
ExecEndRowTrigger(RowTriggerState *node)
{
	ExecFreeExprContext(&node->ps);
	ExecEndNode(outerPlanState(node));
	EndPlanStateGpmonPkt(&node->ps);
}

/* Return number of TupleTableSlots used by RowTrigger node.*/
int
ExecCountSlotsRowTrigger(RowTrigger *node)
{
	return ExecCountSlotsNode(outerPlan(node)) + ROWTRIGGER_NSLOTS;
}

/* Tracing execution for GP Monitor. */
void
initGpmonPktForRowTrigger(Plan *planNode, gpmon_packet_t *gpmon_pkt, EState *estate)
{
	Assert(planNode != NULL && gpmon_pkt != NULL && IsA(planNode, RowTrigger));

	InitPlanNodeGpmonPkt(planNode, gpmon_pkt, estate);
}
