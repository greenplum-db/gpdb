/*-------------------------------------------------------------------------
 *
 * execUtils.c
 *	  miscellaneous executor utility routines
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/execUtils.c
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *		CreateExecutorState		Create/delete executor working state
 *		FreeExecutorState
 *		CreateExprContext
 *		CreateStandaloneExprContext
 *		FreeExprContext
 *		ReScanExprContext
 *
 *		ExecAssignExprContext	Common code for plan node init routines.
 *		ExecAssignResultType
 *		etc
 *
 *		ExecOpenScanRelation	Common code for scan node init routines.
 *		ExecCloseScanRelation
 *
 *		ExecOpenIndices			\
 *		ExecCloseIndices		 | referenced by InitPlan, EndPlan,
 *		ExecInsertIndexTuples	/  ExecInsert, ExecUpdate
 *
 *		RegisterExprContextCallback    Register function shutdown callback
 *		UnregisterExprContextCallback  Deregister function shutdown callback
 *
 *	 NOTES
 *		This file has traditionally been the place to stick misc.
 *		executor support stuff that doesn't really go anyplace else.
 */

#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/appendonlywriter.h"
#include "access/relscan.h"
#include "access/transam.h"
#include "catalog/index.h"
#include "executor/execdebug.h"
#include "executor/execUtils.h"
#include "nodes/nodeFuncs.h"
#include "parser/parsetree.h"
#include "storage/lmgr.h"
#include "utils/memutils.h"
#include "utils/tqual.h"

#include "nodes/primnodes.h"
#include "nodes/execnodes.h"

#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/ml_ipc.h"
#include "cdb/cdbmotion.h"
#include "cdb/cdbsreh.h"
#include "cdb/memquota.h"
#include "executor/instrument.h"
#include "executor/spi.h"
#include "utils/elog.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "storage/ipc.h"
#include "cdb/cdbllize.h"
#include "utils/guc.h"
#include "utils/workfile_mgr.h"
#include "utils/metrics_utils.h"

#include "cdb/memquota.h"

static bool get_last_attnums(Node *node, ProjectionInfo *projInfo);
static bool index_recheck_constraint(Relation index, Oid *constr_procs,
						 Datum *existing_values, bool *existing_isnull,
						 Datum *new_values);
static void ShutdownExprContext(ExprContext *econtext, bool isCommit);


/* ----------------------------------------------------------------
 *				 Executor state and memory management functions
 * ----------------------------------------------------------------
 */

/* ----------------
 *		CreateExecutorState
 *
 *		Create and initialize an EState node, which is the root of
 *		working storage for an entire Executor invocation.
 *
 * Principally, this creates the per-query memory context that will be
 * used to hold all working data that lives till the end of the query.
 * Note that the per-query context will become a child of the caller's
 * CurrentMemoryContext.
 * ----------------
 */
EState *
CreateExecutorState(void)
{
	EState	   *estate;
	MemoryContext qcontext;
	MemoryContext oldcontext;

	/*
	 * Create the per-query context for this Executor run.
	 */
	qcontext = AllocSetContextCreate(CurrentMemoryContext,
									 "ExecutorState",
									 ALLOCSET_DEFAULT_MINSIZE,
									 ALLOCSET_DEFAULT_INITSIZE,
									 ALLOCSET_DEFAULT_MAXSIZE);

	/*
	 * Make the EState node within the per-query context.  This way, we don't
	 * need a separate pfree() operation for it at shutdown.
	 */
	oldcontext = MemoryContextSwitchTo(qcontext);

	estate = makeNode(EState);

	/*
	 * Initialize dynamicTableScanInfo.
	 */
	estate->dynamicTableScanInfo = palloc0(sizeof(DynamicTableScanInfo));

	/*
	 * Initialize all fields of the Executor State structure
	 */
	estate->es_direction = ForwardScanDirection;
	estate->es_snapshot = InvalidSnapshot;		/* caller must initialize this */
	estate->es_crosscheck_snapshot = InvalidSnapshot;	/* no crosscheck */
	estate->es_range_table = NIL;
	estate->es_plannedstmt = NULL;

	estate->es_junkFilter = NULL;

	estate->es_output_cid = (CommandId) 0;

	estate->es_result_relations = NULL;
	estate->es_num_result_relations = 0;
	estate->es_result_relation_info = NULL;

	estate->es_trig_target_relations = NIL;
	estate->es_trig_tuple_slot = NULL;
	estate->es_trig_oldtup_slot = NULL;
	estate->es_trig_newtup_slot = NULL;

	estate->es_param_list_info = NULL;
	estate->es_param_exec_vals = NULL;

	estate->es_query_cxt = qcontext;

	estate->es_tupleTable = NIL;

	estate->es_rowMarks = NIL;

	estate->es_processed = 0;
	estate->es_lastoid = InvalidOid;

	estate->es_top_eflags = 0;
	estate->es_instrument = 0;
	estate->es_finished = false;

	estate->es_exprcontexts = NIL;

	estate->es_subplanstates = NIL;

	estate->es_auxmodifytables = NIL;

	estate->es_per_tuple_exprcontext = NULL;

	estate->es_epqTuple = NULL;
	estate->es_epqTupleSet = NULL;
	estate->es_epqScanDone = NULL;

	estate->es_sliceTable = NULL;
	estate->interconnect_context = NULL;
	estate->motionlayer_context = NULL;
	estate->es_interconnect_is_setup = false;
	estate->active_recv_id = -1;
	estate->es_got_eos = false;
	estate->cancelUnfinished = false;

	estate->dispatcherState = NULL;

	estate->currentSliceIdInPlan = 0;
	estate->currentExecutingSliceId = 0;
	estate->currentSubplanLevel = 0;
	estate->rootSliceId = 0;
	estate->eliminateAliens = false;

	/*
	 * Return the executor state structure
	 */
	MemoryContextSwitchTo(oldcontext);

	return estate;
}

/* ----------------
 *		FreeExecutorState
 *
 *		Release an EState along with all remaining working storage.
 *
 * Note: this is not responsible for releasing non-memory resources,
 * such as open relations or buffer pins.  But it will shut down any
 * still-active ExprContexts within the EState.  That is sufficient
 * cleanup for situations where the EState has only been used for expression
 * evaluation, and not to run a complete Plan.
 *
 * This can be called in any memory context ... so long as it's not one
 * of the ones to be freed.
 *
 * In Greenplum, this also clears the PartitionState, even though that's a
 * non-memory resource, as that can be allocated for expression evaluation even
 * when there is no Plan.
 * ----------------
 */
void
FreeExecutorState(EState *estate)
{
	/*
	 * Shut down and free any remaining ExprContexts.  We do this explicitly
	 * to ensure that any remaining shutdown callbacks get called (since they
	 * might need to release resources that aren't simply memory within the
	 * per-query memory context).
	 */
	while (estate->es_exprcontexts)
	{
		/*
		 * XXX: seems there ought to be a faster way to implement this than
		 * repeated list_delete(), no?
		 */
		FreeExprContext((ExprContext *) linitial(estate->es_exprcontexts),
						true);
		/* FreeExprContext removed the list link for us */
	}

	estate->dispatcherState = NULL;
	estate->dynamicTableScanInfo = NULL;

	/*
	 * Free the per-query memory context, thereby releasing all working
	 * memory, including the EState node itself.
	 */
	MemoryContextDelete(estate->es_query_cxt);
}

/* ----------------
 *		CreateExprContext
 *
 *		Create a context for expression evaluation within an EState.
 *
 * An executor run may require multiple ExprContexts (we usually make one
 * for each Plan node, and a separate one for per-output-tuple processing
 * such as constraint checking).  Each ExprContext has its own "per-tuple"
 * memory context.
 *
 * Note we make no assumption about the caller's memory context.
 * ----------------
 */
ExprContext *
CreateExprContext(EState *estate)
{
	ExprContext *econtext;
	MemoryContext oldcontext;

	/* Create the ExprContext node within the per-query memory context */
	oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

	econtext = makeNode(ExprContext);

	/* Initialize fields of ExprContext */
	econtext->ecxt_scantuple = NULL;
	econtext->ecxt_innertuple = NULL;
	econtext->ecxt_outertuple = NULL;

	econtext->ecxt_per_query_memory = estate->es_query_cxt;

	/*
	 * Create working memory for expression evaluation in this context.
	 */
	econtext->ecxt_per_tuple_memory =
		AllocSetContextCreate(estate->es_query_cxt,
							  "ExprContext",
							  ALLOCSET_DEFAULT_MINSIZE,
							  ALLOCSET_DEFAULT_INITSIZE,
							  ALLOCSET_DEFAULT_MAXSIZE);

	econtext->ecxt_param_exec_vals = estate->es_param_exec_vals;
	econtext->ecxt_param_list_info = estate->es_param_list_info;

	econtext->ecxt_aggvalues = NULL;
	econtext->ecxt_aggnulls = NULL;

	econtext->caseValue_datum = (Datum) 0;
	econtext->caseValue_isNull = true;

	econtext->domainValue_datum = (Datum) 0;
	econtext->domainValue_isNull = true;

	econtext->ecxt_estate = estate;

	econtext->ecxt_callbacks = NULL;

	/*
	 * Link the ExprContext into the EState to ensure it is shut down when the
	 * EState is freed.  Because we use lcons(), shutdowns will occur in
	 * reverse order of creation, which may not be essential but can't hurt.
	 */
	estate->es_exprcontexts = lcons(econtext, estate->es_exprcontexts);

	MemoryContextSwitchTo(oldcontext);

	return econtext;
}

/* ----------------
 *		CreateStandaloneExprContext
 *
 *		Create a context for standalone expression evaluation.
 *
 * An ExprContext made this way can be used for evaluation of expressions
 * that contain no Params, subplans, or Var references (it might work to
 * put tuple references into the scantuple field, but it seems unwise).
 *
 * The ExprContext struct is allocated in the caller's current memory
 * context, which also becomes its "per query" context.
 *
 * It is caller's responsibility to free the ExprContext when done,
 * or at least ensure that any shutdown callbacks have been called
 * (ReScanExprContext() is suitable).  Otherwise, non-memory resources
 * might be leaked.
 * ----------------
 */
ExprContext *
CreateStandaloneExprContext(void)
{
	ExprContext *econtext;

	/* Create the ExprContext node within the caller's memory context */
	econtext = makeNode(ExprContext);

	/* Initialize fields of ExprContext */
	econtext->ecxt_scantuple = NULL;
	econtext->ecxt_innertuple = NULL;
	econtext->ecxt_outertuple = NULL;

	econtext->ecxt_per_query_memory = CurrentMemoryContext;

	/*
	 * Create working memory for expression evaluation in this context.
	 */
	econtext->ecxt_per_tuple_memory =
		AllocSetContextCreate(CurrentMemoryContext,
							  "ExprContext",
							  ALLOCSET_DEFAULT_MINSIZE,
							  ALLOCSET_DEFAULT_INITSIZE,
							  ALLOCSET_DEFAULT_MAXSIZE);

	econtext->ecxt_param_exec_vals = NULL;
	econtext->ecxt_param_list_info = NULL;

	econtext->ecxt_aggvalues = NULL;
	econtext->ecxt_aggnulls = NULL;

	econtext->caseValue_datum = (Datum) 0;
	econtext->caseValue_isNull = true;

	econtext->domainValue_datum = (Datum) 0;
	econtext->domainValue_isNull = true;

	econtext->ecxt_estate = NULL;

	econtext->ecxt_callbacks = NULL;

	return econtext;
}

/* ----------------
 *		FreeExprContext
 *
 *		Free an expression context, including calling any remaining
 *		shutdown callbacks.
 *
 * Since we free the temporary context used for expression evaluation,
 * any previously computed pass-by-reference expression result will go away!
 *
 * If isCommit is false, we are being called in error cleanup, and should
 * not call callbacks but only release memory.  (It might be better to call
 * the callbacks and pass the isCommit flag to them, but that would require
 * more invasive code changes than currently seems justified.)
 *
 * Note we make no assumption about the caller's memory context.
 * ----------------
 */
void
FreeExprContext(ExprContext *econtext, bool isCommit)
{
	EState	   *estate;

	/* Call any registered callbacks */
	ShutdownExprContext(econtext, isCommit);
	/* And clean up the memory used */
	MemoryContextDelete(econtext->ecxt_per_tuple_memory);
	/* Unlink self from owning EState, if any */
	estate = econtext->ecxt_estate;
	if (estate)
		estate->es_exprcontexts = list_delete_ptr(estate->es_exprcontexts,
												  econtext);
	/* And delete the ExprContext node */
	pfree(econtext);
}

/*
 * ReScanExprContext
 *
 *		Reset an expression context in preparation for a rescan of its
 *		plan node.  This requires calling any registered shutdown callbacks,
 *		since any partially complete set-returning-functions must be canceled.
 *
 * Note we make no assumption about the caller's memory context.
 */
void
ReScanExprContext(ExprContext *econtext)
{
	/* Call any registered callbacks */
	ShutdownExprContext(econtext, true);
	/* And clean up the memory used */
	MemoryContextReset(econtext->ecxt_per_tuple_memory);
}

/*
 * Build a per-output-tuple ExprContext for an EState.
 *
 * This is normally invoked via GetPerTupleExprContext() macro,
 * not directly.
 */
ExprContext *
MakePerTupleExprContext(EState *estate)
{
	if (estate->es_per_tuple_exprcontext == NULL)
		estate->es_per_tuple_exprcontext = CreateExprContext(estate);

	return estate->es_per_tuple_exprcontext;
}


/* ----------------------------------------------------------------
 *				 miscellaneous node-init support functions
 *
 * Note: all of these are expected to be called with CurrentMemoryContext
 * equal to the per-query memory context.
 * ----------------------------------------------------------------
 */

/* ----------------
 *		ExecAssignExprContext
 *
 *		This initializes the ps_ExprContext field.  It is only necessary
 *		to do this for nodes which use ExecQual or ExecProject
 *		because those routines require an econtext. Other nodes that
 *		don't have to evaluate expressions don't need to do this.
 * ----------------
 */
void
ExecAssignExprContext(EState *estate, PlanState *planstate)
{
	planstate->ps_ExprContext = CreateExprContext(estate);
}

/* ----------------
 *		ExecAssignResultType
 * ----------------
 */
void
ExecAssignResultType(PlanState *planstate, TupleDesc tupDesc)
{
	TupleTableSlot *slot = planstate->ps_ResultTupleSlot;

	ExecSetSlotDescriptor(slot, tupDesc);
}

/* ----------------
 *		ExecAssignResultTypeFromTL
 * ----------------
 */
void
ExecAssignResultTypeFromTL(PlanState *planstate)
{
	bool		hasoid;
	TupleDesc	tupDesc;

	if (ExecContextForcesOids(planstate, &hasoid))
	{
		/* context forces OID choice; hasoid is now set correctly */
	}
	else
	{
		/* given free choice, don't leave space for OIDs in result tuples */
		hasoid = false;
	}

	/*
	 * ExecTypeFromTL needs the parse-time representation of the tlist, not a
	 * list of ExprStates.  This is good because some plan nodes don't bother
	 * to set up planstate->targetlist ...
	 */
	tupDesc = ExecTypeFromTL(planstate->plan->targetlist, hasoid);
	ExecAssignResultType(planstate, tupDesc);
}

/* ----------------
 *		ExecGetResultType
 * ----------------
 */
TupleDesc
ExecGetResultType(PlanState *planstate)
{
	TupleTableSlot *slot = planstate->ps_ResultTupleSlot;

	return slot->tts_tupleDescriptor;
}

/* ----------------
 *		ExecBuildProjectionInfo
 *
 * Build a ProjectionInfo node for evaluating the given tlist in the given
 * econtext, and storing the result into the tuple slot.  (Caller must have
 * ensured that tuple slot has a descriptor matching the tlist!)  Note that
 * the given tlist should be a list of ExprState nodes, not Expr nodes.
 *
 * inputDesc can be NULL, but if it is not, we check to see whether simple
 * Vars in the tlist match the descriptor.  It is important to provide
 * inputDesc for relation-scan plan nodes, as a cross check that the relation
 * hasn't been changed since the plan was made.  At higher levels of a plan,
 * there is no need to recheck.
 * ----------------
 */
ProjectionInfo *
ExecBuildProjectionInfo(List *targetList,
						ExprContext *econtext,
						TupleTableSlot *slot,
						TupleDesc inputDesc)
{
	ProjectionInfo *projInfo = makeNode(ProjectionInfo);
	int			len = ExecTargetListLength(targetList);
	int		   *workspace;
	int		   *varSlotOffsets;
	int		   *varNumbers;
	int		   *varOutputCols;
	List	   *exprlist;
	int			numSimpleVars;
	bool		directMap;
	ListCell   *tl;

	projInfo->pi_exprContext = econtext;
	projInfo->pi_slot = slot;
	/* since these are all int arrays, we need do just one palloc */
	workspace = (int *) palloc(len * 3 * sizeof(int));
	projInfo->pi_varSlotOffsets = varSlotOffsets = workspace;
	projInfo->pi_varNumbers = varNumbers = workspace + len;
	projInfo->pi_varOutputCols = varOutputCols = workspace + len * 2;
	projInfo->pi_lastInnerVar = 0;
	projInfo->pi_lastOuterVar = 0;
	projInfo->pi_lastScanVar = 0;

	/*
	 * We separate the target list elements into simple Var references and
	 * expressions which require the full ExecTargetList machinery.  To be a
	 * simple Var, a Var has to be a user attribute and not mismatch the
	 * inputDesc.  (Note: if there is a type mismatch then ExecEvalScalarVar
	 * will probably throw an error at runtime, but we leave that to it.)
	 */
	exprlist = NIL;
	numSimpleVars = 0;
	directMap = true;
	foreach(tl, targetList)
	{
		GenericExprState *gstate = (GenericExprState *) lfirst(tl);
		Var		   *variable = (Var *) gstate->arg->expr;
		bool		isSimpleVar = false;

		if (variable != NULL &&
			IsA(variable, Var) &&
			variable->varattno > 0)
		{
			if (!inputDesc)
				isSimpleVar = true;		/* can't check type, assume OK */
			else if (variable->varattno <= inputDesc->natts)
			{
				Form_pg_attribute attr;

				attr = inputDesc->attrs[variable->varattno - 1];
				if (!attr->attisdropped && variable->vartype == attr->atttypid)
					isSimpleVar = true;
			}
		}

		if (isSimpleVar)
		{
			TargetEntry *tle = (TargetEntry *) gstate->xprstate.expr;
			AttrNumber	attnum = variable->varattno;

			varNumbers[numSimpleVars] = attnum;
			varOutputCols[numSimpleVars] = tle->resno;
			if (tle->resno != numSimpleVars + 1)
				directMap = false;

			switch (variable->varno)
			{
				case INNER_VAR:
					varSlotOffsets[numSimpleVars] = offsetof(ExprContext,
															 ecxt_innertuple);
					if (projInfo->pi_lastInnerVar < attnum)
						projInfo->pi_lastInnerVar = attnum;
					break;

				case OUTER_VAR:
					varSlotOffsets[numSimpleVars] = offsetof(ExprContext,
															 ecxt_outertuple);
					if (projInfo->pi_lastOuterVar < attnum)
						projInfo->pi_lastOuterVar = attnum;
					break;

					/* INDEX_VAR is handled by default case */

				default:
					varSlotOffsets[numSimpleVars] = offsetof(ExprContext,
															 ecxt_scantuple);
					if (projInfo->pi_lastScanVar < attnum)
						projInfo->pi_lastScanVar = attnum;
					break;
			}
			numSimpleVars++;
		}
		else
		{
			/* Not a simple variable, add it to generic targetlist */
			exprlist = lappend(exprlist, gstate);
			/* Examine expr to include contained Vars in lastXXXVar counts */
			get_last_attnums((Node *) variable, projInfo);
		}
	}
	projInfo->pi_targetlist = exprlist;
	projInfo->pi_numSimpleVars = numSimpleVars;
	projInfo->pi_directMap = directMap;

	if (exprlist == NIL)
		projInfo->pi_itemIsDone = NULL; /* not needed */
	else
		projInfo->pi_itemIsDone = (ExprDoneCond *)
			palloc(len * sizeof(ExprDoneCond));

	return projInfo;
}

/*
 * get_last_attnums: expression walker for ExecBuildProjectionInfo
 *
 *	Update the lastXXXVar counts to be at least as large as the largest
 *	attribute numbers found in the expression
 */
static bool
get_last_attnums(Node *node, ProjectionInfo *projInfo)
{
	if (node == NULL)
		return false;
	if (IsA(node, Var))
	{
		Var		   *variable = (Var *) node;
		AttrNumber	attnum = variable->varattno;

		switch (variable->varno)
		{
			case INNER_VAR:
				if (projInfo->pi_lastInnerVar < attnum)
					projInfo->pi_lastInnerVar = attnum;
				break;

			case OUTER_VAR:
				if (projInfo->pi_lastOuterVar < attnum)
					projInfo->pi_lastOuterVar = attnum;
				break;

				/* INDEX_VAR is handled by default case */

			default:
				if (projInfo->pi_lastScanVar < attnum)
					projInfo->pi_lastScanVar = attnum;
				break;
		}
		return false;
	}

	/*
	 * Don't examine the arguments or filters of Aggrefs or WindowFuncs,
	 * because those do not represent expressions to be evaluated within the
	 * overall targetlist's econtext.
	 */
	if (IsA(node, Aggref))
		return false;
	if (IsA(node, WindowFunc))
		return false;
	return expression_tree_walker(node, get_last_attnums,
								  (void *) projInfo);
}

/* ----------------
 *		ExecAssignProjectionInfo
 *
 * forms the projection information from the node's targetlist
 *
 * Notes for inputDesc are same as for ExecBuildProjectionInfo: supply it
 * for a relation-scan node, can pass NULL for upper-level nodes
 * ----------------
 */
void
ExecAssignProjectionInfo(PlanState *planstate,
						 TupleDesc inputDesc)
{
	ProjectionInfo* pi = planstate->ps_ProjInfo;
	if (NULL != pi)
	{
		/*
		 * Note that pi->pi_varSlotOffsets, pi->pi_varNumbers, and
		 * pi->pi_varOutputCols are all pointers into the same allocation.
		 */
		if (NULL != pi->pi_varSlotOffsets)
		{
			pfree(pi->pi_varSlotOffsets);
		}
		if (NULL != pi->pi_itemIsDone)
		{
			pfree(pi->pi_itemIsDone);
		}
		pfree(pi);
	}

	planstate->ps_ProjInfo =
		ExecBuildProjectionInfo(planstate->targetlist,
								planstate->ps_ExprContext,
								planstate->ps_ResultTupleSlot,
								inputDesc);
}


/* ----------------
 *		ExecFreeExprContext
 *
 * A plan node's ExprContext should be freed explicitly during executor
 * shutdown because there may be shutdown callbacks to call.  (Other resources
 * made by the above routines, such as projection info, don't need to be freed
 * explicitly because they're just memory in the per-query memory context.)
 *
 * However ... there is no particular need to do it during ExecEndNode,
 * because FreeExecutorState will free any remaining ExprContexts within
 * the EState.  Letting FreeExecutorState do it allows the ExprContexts to
 * be freed in reverse order of creation, rather than order of creation as
 * will happen if we delete them here, which saves O(N^2) work in the list
 * cleanup inside FreeExprContext.
 * ----------------
 */
void
ExecFreeExprContext(PlanState *planstate)
{
	/*
	 * Per above discussion, don't actually delete the ExprContext. We do
	 * unlink it from the plan node, though.
	 */
	planstate->ps_ExprContext = NULL;
}

/* ----------------------------------------------------------------
 *		the following scan type support functions are for
 *		those nodes which are stubborn and return tuples in
 *		their Scan tuple slot instead of their Result tuple
 *		slot..  luck fur us, these nodes do not do projections
 *		so we don't have to worry about getting the ProjectionInfo
 *		right for them...  -cim 6/3/91
 * ----------------------------------------------------------------
 */

/* ----------------
 *		ExecGetScanType
 * ----------------
 */
TupleDesc
ExecGetScanType(ScanState *scanstate)
{
	TupleTableSlot *slot = scanstate->ss_ScanTupleSlot;

	return slot->tts_tupleDescriptor;
}

/* ----------------
 *		ExecAssignScanType
 * ----------------
 */
void
ExecAssignScanType(ScanState *scanstate, TupleDesc tupDesc)
{
	TupleTableSlot *slot = scanstate->ss_ScanTupleSlot;

	ExecSetSlotDescriptor(slot, tupDesc);
}

/* ----------------
 *		ExecAssignScanTypeFromOuterPlan
 * ----------------
 */
void
ExecAssignScanTypeFromOuterPlan(ScanState *scanstate)
{
	PlanState  *outerPlan;
	TupleDesc	tupDesc;

	outerPlan = outerPlanState(scanstate);
	tupDesc = ExecGetResultType(outerPlan);

	ExecAssignScanType(scanstate, tupDesc);
}


/* ----------------------------------------------------------------
 *				  Scan node support
 * ----------------------------------------------------------------
 */

/* ----------------------------------------------------------------
 *		ExecRelationIsTargetRelation
 *
 *		Detect whether a relation (identified by rangetable index)
 *		is one of the target relations of the query.
 * ----------------------------------------------------------------
 */
bool
ExecRelationIsTargetRelation(EState *estate, Index scanrelid)
{
	ResultRelInfo *resultRelInfos;
	int			i;

	resultRelInfos = estate->es_result_relations;
	for (i = 0; i < estate->es_num_result_relations; i++)
	{
		if (resultRelInfos[i].ri_RangeTableIndex == scanrelid)
			return true;
	}
	return false;
}

/* ----------------------------------------------------------------
 *		ExecOpenScanRelation
 *
 *		Open the heap relation to be scanned by a base-level scan plan node.
 *		This should be called during the node's ExecInit routine.
 *
 * By default, this acquires AccessShareLock on the relation.  However,
 * if the relation was already locked by InitPlan, we don't need to acquire
 * any additional lock.  This saves trips to the shared lock manager.
 * ----------------------------------------------------------------
 */
Relation
ExecOpenScanRelation(EState *estate, Index scanrelid, int eflags)
{
	Relation	rel;
	Oid			reloid;
	LOCKMODE	lockmode;

	/*
	 * Determine the lock type we need.  First, scan to see if target relation
	 * is a result relation.  If not, check if it's a FOR UPDATE/FOR SHARE
	 * relation.  In either of those cases, we got the lock already.
	 */
	lockmode = AccessShareLock;
	if (ExecRelationIsTargetRelation(estate, scanrelid))
		lockmode = NoLock;
	else
	{
		ListCell   *l;

		foreach(l, estate->es_rowMarks)
		{
			ExecRowMark *erm = lfirst(l);

			/* Keep this check in sync with InitPlan! */
			if (erm->rti == scanrelid &&
				erm->relation != NULL)
			{
				lockmode = NoLock;
				break;
			}
		}
	}

	/* Open the relation and acquire lock as needed */
	reloid = getrelid(scanrelid, estate->es_range_table);
	rel = heap_open(reloid, lockmode);

	/*
	 * Complain if we're attempting a scan of an unscannable relation, except
	 * when the query won't actually be run.  This is a slightly klugy place
	 * to do this, perhaps, but there is no better place.
	 */
	if ((eflags & (EXEC_FLAG_EXPLAIN_ONLY | EXEC_FLAG_WITH_NO_DATA)) == 0 &&
		!RelationIsScannable(rel))
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("materialized view \"%s\" has not been populated",
						RelationGetRelationName(rel)),
				 errhint("Use the REFRESH MATERIALIZED VIEW command.")));

	return rel;
}

/*
 * same as above, but for external table scans
 */
Relation
ExecOpenScanExternalRelation(EState *estate, Index scanrelid)
{
	RangeTblEntry *rtentry;
	Oid			reloid;
	LOCKMODE	lockmode;

	lockmode = NoLock;

	rtentry = rt_fetch(scanrelid, estate->es_range_table);
	reloid = rtentry->relid;

	return relation_open(reloid, NoLock);
}

/* ----------------------------------------------------------------
 *		ExecCloseScanRelation
 *
 *		Close the heap relation scanned by a base-level scan plan node.
 *		This should be called during the node's ExecEnd routine.
 *
 * Currently, we do not release the lock acquired by ExecOpenScanRelation.
 * This lock should be held till end of transaction.  (There is a faction
 * that considers this too much locking, however.)
 *
 * If we did want to release the lock, we'd have to repeat the logic in
 * ExecOpenScanRelation in order to figure out what to release.
 * ----------------------------------------------------------------
 */
void
ExecCloseScanRelation(Relation scanrel)
{
	heap_close(scanrel, NoLock);
}


/* ----------------------------------------------------------------
 *				  ExecInsertIndexTuples support
 * ----------------------------------------------------------------
 */

/* ----------------------------------------------------------------
 *		ExecOpenIndices
 *
 *		Find the indices associated with a result relation, open them,
 *		and save information about them in the result ResultRelInfo.
 *
 *		At entry, caller has already opened and locked
 *		resultRelInfo->ri_RelationDesc.
 * ----------------------------------------------------------------
 */
void
ExecOpenIndices(ResultRelInfo *resultRelInfo)
{
	Relation	resultRelation = resultRelInfo->ri_RelationDesc;
	List	   *indexoidlist;
	ListCell   *l;
	int			len,
				i;
	RelationPtr relationDescs;
	IndexInfo **indexInfoArray;

	resultRelInfo->ri_NumIndices = 0;

	/* fast path if no indexes */
	if (!RelationGetForm(resultRelation)->relhasindex)
		return;

	/*
	 * Get cached list of index OIDs
	 */
	indexoidlist = RelationGetIndexList(resultRelation);
	len = list_length(indexoidlist);
	if (len == 0)
		return;

	/*
	 * allocate space for result arrays
	 */
	relationDescs = (RelationPtr) palloc(len * sizeof(Relation));
	indexInfoArray = (IndexInfo **) palloc(len * sizeof(IndexInfo *));

	resultRelInfo->ri_NumIndices = len;
	resultRelInfo->ri_IndexRelationDescs = relationDescs;
	resultRelInfo->ri_IndexRelationInfo = indexInfoArray;

	/*
	 * For each index, open the index relation and save pg_index info. We
	 * acquire RowExclusiveLock, signifying we will update the index.
	 *
	 * Note: we do this even if the index is not IndexIsReady; it's not worth
	 * the trouble to optimize for the case where it isn't.
	 */
	i = 0;
	foreach(l, indexoidlist)
	{
		Oid			indexOid = lfirst_oid(l);
		Relation	indexDesc;
		IndexInfo  *ii;

		indexDesc = index_open(indexOid, RowExclusiveLock);

		/* extract index key information from the index's pg_index info */
		ii = BuildIndexInfo(indexDesc);

		relationDescs[i] = indexDesc;
		indexInfoArray[i] = ii;
		i++;
	}

	list_free(indexoidlist);
}

/* ----------------------------------------------------------------
 *		ExecCloseIndices
 *
 *		Close the index relations stored in resultRelInfo
 * ----------------------------------------------------------------
 */
void
ExecCloseIndices(ResultRelInfo *resultRelInfo)
{
	int			i;
	int			numIndices;
	RelationPtr indexDescs;

	numIndices = resultRelInfo->ri_NumIndices;
	indexDescs = resultRelInfo->ri_IndexRelationDescs;

	for (i = 0; i < numIndices; i++)
	{
		if (indexDescs[i] == NULL)
			continue;			/* shouldn't happen? */

		/* Drop lock acquired by ExecOpenIndices */
		index_close(indexDescs[i], RowExclusiveLock);
	}

	/*
	 * XXX should free indexInfo array here too?  Currently we assume that
	 * such stuff will be cleaned up automatically in FreeExecutorState.
	 */
}

/* ----------------------------------------------------------------
 *		ExecInsertIndexTuples
 *
 *		This routine takes care of inserting index tuples
 *		into all the relations indexing the result relation
 *		when a heap tuple is inserted into the result relation.
 *		Much of this code should be moved into the genam
 *		stuff as it only exists here because the genam stuff
 *		doesn't provide the functionality needed by the
 *		executor.. -cim 9/27/89
 *
 *		This returns a list of index OIDs for any unique or exclusion
 *		constraints that are deferred and that had
 *		potential (unconfirmed) conflicts.
 *
 *		CAUTION: this must not be called for a HOT update.
 *		We can't defend against that here for lack of info.
 *		Should we change the API to make it safer?
 * ----------------------------------------------------------------
 */
List *
ExecInsertIndexTuples(TupleTableSlot *slot,
					  ItemPointer tupleid,
					  EState *estate)
{
	List	   *result = NIL;
	ResultRelInfo *resultRelInfo;
	int			i;
	int			numIndices;
	RelationPtr relationDescs;
	Relation	heapRelation;
	IndexInfo **indexInfoArray;
	ExprContext *econtext;
	Datum		values[INDEX_MAX_KEYS];
	bool		isnull[INDEX_MAX_KEYS];

	/*
	 * Get information from the result relation info structure.
	 */
	resultRelInfo = estate->es_result_relation_info;
	numIndices = resultRelInfo->ri_NumIndices;
	relationDescs = resultRelInfo->ri_IndexRelationDescs;
	indexInfoArray = resultRelInfo->ri_IndexRelationInfo;
	heapRelation = resultRelInfo->ri_RelationDesc;

	/*
	 * We will use the EState's per-tuple context for evaluating predicates
	 * and index expressions (creating it if it's not already there).
	 */
	econtext = GetPerTupleExprContext(estate);

	/* Arrange for econtext's scan tuple to be the tuple under test */
	econtext->ecxt_scantuple = slot;

	/*
	 * for each index, form and insert the index tuple
	 */
	for (i = 0; i < numIndices; i++)
	{
		Relation	indexRelation = relationDescs[i];
		IndexInfo  *indexInfo;
		IndexUniqueCheck checkUnique;
		bool		satisfiesConstraint;

		if (indexRelation == NULL)
			continue;

		indexInfo = indexInfoArray[i];

		/* If the index is marked as read-only, ignore it */
		if (!indexInfo->ii_ReadyForInserts)
			continue;

		/* Check for partial index */
		if (indexInfo->ii_Predicate != NIL)
		{
			List	   *predicate;

			/*
			 * If predicate state not set up yet, create it (in the estate's
			 * per-query context)
			 */
			predicate = indexInfo->ii_PredicateState;
			if (predicate == NIL)
			{
				predicate = (List *)
					ExecPrepareExpr((Expr *) indexInfo->ii_Predicate,
									estate);
				indexInfo->ii_PredicateState = predicate;
			}

			/* Skip this index-update if the predicate isn't satisfied */
			if (!ExecQual(predicate, econtext, false))
				continue;
		}

		/*
		 * FormIndexDatum fills in its values and isnull parameters with the
		 * appropriate values for the column(s) of the index.
		 */
		FormIndexDatum(indexInfo,
					   slot,
					   estate,
					   values,
					   isnull);

		/*
		 * The index AM does the actual insertion, plus uniqueness checking.
		 *
		 * For an immediate-mode unique index, we just tell the index AM to
		 * throw error if not unique.
		 *
		 * For a deferrable unique index, we tell the index AM to just detect
		 * possible non-uniqueness, and we add the index OID to the result
		 * list if further checking is needed.
		 */
		if (!indexRelation->rd_index->indisunique)
			checkUnique = UNIQUE_CHECK_NO;
		else if (indexRelation->rd_index->indimmediate)
			checkUnique = UNIQUE_CHECK_YES;
		else
			checkUnique = UNIQUE_CHECK_PARTIAL;

		satisfiesConstraint =
			index_insert(indexRelation, /* index relation */
						 values,	/* array of index Datums */
						 isnull,	/* null flags */
						 tupleid,		/* tid of heap tuple */
						 heapRelation,	/* heap relation */
						 checkUnique);	/* type of uniqueness check to do */

		/*
		 * If the index has an associated exclusion constraint, check that.
		 * This is simpler than the process for uniqueness checks since we
		 * always insert first and then check.  If the constraint is deferred,
		 * we check now anyway, but don't throw error on violation; instead
		 * we'll queue a recheck event.
		 *
		 * An index for an exclusion constraint can't also be UNIQUE (not an
		 * essential property, we just don't allow it in the grammar), so no
		 * need to preserve the prior state of satisfiesConstraint.
		 */
		if (indexInfo->ii_ExclusionOps != NULL)
		{
			bool		errorOK = !indexRelation->rd_index->indimmediate;

			satisfiesConstraint =
				check_exclusion_constraint(heapRelation,
										   indexRelation, indexInfo,
										   tupleid, values, isnull,
										   estate, false, errorOK);
		}

		if ((checkUnique == UNIQUE_CHECK_PARTIAL ||
			 indexInfo->ii_ExclusionOps != NULL) &&
			!satisfiesConstraint)
		{
			/*
			 * The tuple potentially violates the uniqueness or exclusion
			 * constraint, so make a note of the index so that we can re-check
			 * it later.
			 */
			result = lappend_oid(result, RelationGetRelid(indexRelation));
		}
	}

	return result;
}

/*
 * Check for violation of an exclusion constraint
 *
 * heap: the table containing the new tuple
 * index: the index supporting the exclusion constraint
 * indexInfo: info about the index, including the exclusion properties
 * tupleid: heap TID of the new tuple we have just inserted
 * values, isnull: the *index* column values computed for the new tuple
 * estate: an EState we can do evaluation in
 * newIndex: if true, we are trying to build a new index (this affects
 *		only the wording of error messages)
 * errorOK: if true, don't throw error for violation
 *
 * Returns true if OK, false if actual or potential violation
 *
 * When errorOK is true, we report violation without waiting to see if any
 * concurrent transaction has committed or not; so the violation is only
 * potential, and the caller must recheck sometime later.  This behavior
 * is convenient for deferred exclusion checks; we need not bother queuing
 * a deferred event if there is definitely no conflict at insertion time.
 *
 * When errorOK is false, we'll throw error on violation, so a false result
 * is impossible.
 */
bool
check_exclusion_constraint(Relation heap, Relation index, IndexInfo *indexInfo,
						   ItemPointer tupleid, Datum *values, bool *isnull,
						   EState *estate, bool newIndex, bool errorOK)
{
	Oid		   *constr_procs = indexInfo->ii_ExclusionProcs;
	uint16	   *constr_strats = indexInfo->ii_ExclusionStrats;
	Oid		   *index_collations = index->rd_indcollation;
	int			index_natts = index->rd_index->indnatts;
	IndexScanDesc index_scan;
	HeapTuple	tup;
	ScanKeyData scankeys[INDEX_MAX_KEYS];
	SnapshotData DirtySnapshot;
	int			i;
	bool		conflict;
	bool		found_self;
	ExprContext *econtext;
	TupleTableSlot *existing_slot;
	TupleTableSlot *save_scantuple;

	/*
	 * If any of the input values are NULL, the constraint check is assumed to
	 * pass (i.e., we assume the operators are strict).
	 */
	for (i = 0; i < index_natts; i++)
	{
		if (isnull[i])
			return true;
	}

	/*
	 * Search the tuples that are in the index for any violations, including
	 * tuples that aren't visible yet.
	 */
	InitDirtySnapshot(DirtySnapshot);

	for (i = 0; i < index_natts; i++)
	{
		ScanKeyEntryInitialize(&scankeys[i],
							   0,
							   i + 1,
							   constr_strats[i],
							   InvalidOid,
							   index_collations[i],
							   constr_procs[i],
							   values[i]);
	}

	/*
	 * Need a TupleTableSlot to put existing tuples in.
	 *
	 * To use FormIndexDatum, we have to make the econtext's scantuple point
	 * to this slot.  Be sure to save and restore caller's value for
	 * scantuple.
	 */
	existing_slot = MakeSingleTupleTableSlot(RelationGetDescr(heap));

	econtext = GetPerTupleExprContext(estate);
	save_scantuple = econtext->ecxt_scantuple;
	econtext->ecxt_scantuple = existing_slot;

	/*
	 * May have to restart scan from this point if a potential conflict is
	 * found.
	 */
retry:
	conflict = false;
	found_self = false;
	index_scan = index_beginscan(heap, index, &DirtySnapshot, index_natts, 0);
	index_rescan(index_scan, scankeys, index_natts, NULL, 0);

	while ((tup = index_getnext(index_scan,
								ForwardScanDirection)) != NULL)
	{
		TransactionId xwait;
		ItemPointerData ctid_wait;
		Datum		existing_values[INDEX_MAX_KEYS];
		bool		existing_isnull[INDEX_MAX_KEYS];
		char	   *error_new;
		char	   *error_existing;

		/*
		 * Ignore the entry for the tuple we're trying to check.
		 */
		if (ItemPointerEquals(tupleid, &tup->t_self))
		{
			if (found_self)		/* should not happen */
				elog(ERROR, "found self tuple multiple times in index \"%s\"",
					 RelationGetRelationName(index));
			found_self = true;
			continue;
		}

		/*
		 * Extract the index column values and isnull flags from the existing
		 * tuple.
		 */
		ExecStoreHeapTuple(tup,	existing_slot, InvalidBuffer, false);
		FormIndexDatum(indexInfo, existing_slot, estate,
					   existing_values, existing_isnull);

		/* If lossy indexscan, must recheck the condition */
		if (index_scan->xs_recheck)
		{
			if (!index_recheck_constraint(index,
										  constr_procs,
										  existing_values,
										  existing_isnull,
										  values))
				continue;		/* tuple doesn't actually match, so no
								 * conflict */
		}

		/*
		 * At this point we have either a conflict or a potential conflict. If
		 * we're not supposed to raise error, just return the fact of the
		 * potential conflict without waiting to see if it's real.
		 */
		if (errorOK)
		{
			conflict = true;
			break;
		}

		/*
		 * If an in-progress transaction is affecting the visibility of this
		 * tuple, we need to wait for it to complete and then recheck.  For
		 * simplicity we do rechecking by just restarting the whole scan ---
		 * this case probably doesn't happen often enough to be worth trying
		 * harder, and anyway we don't want to hold any index internal locks
		 * while waiting.
		 */
		xwait = TransactionIdIsValid(DirtySnapshot.xmin) ?
			DirtySnapshot.xmin : DirtySnapshot.xmax;

		if (TransactionIdIsValid(xwait))
		{
			ctid_wait = tup->t_data->t_ctid;
			index_endscan(index_scan);
			XactLockTableWait(xwait, heap, &ctid_wait,
							  XLTW_RecheckExclusionConstr);
			goto retry;
		}

		/*
		 * We have a definite conflict.  Report it.
		 */
		error_new = BuildIndexValueDescription(index, values, isnull);
		error_existing = BuildIndexValueDescription(index, existing_values,
													existing_isnull);
		if (newIndex)
			ereport(ERROR,
					(errcode(ERRCODE_EXCLUSION_VIOLATION),
					 errmsg("could not create exclusion constraint \"%s\"",
							RelationGetRelationName(index)),
					 error_new && error_existing ?
						errdetail("Key %s conflicts with key %s.",
								  error_new, error_existing) :
						errdetail("Key conflicts exist."),
					 errtableconstraint(heap,
										RelationGetRelationName(index))));
		else
			ereport(ERROR,
					(errcode(ERRCODE_EXCLUSION_VIOLATION),
					 errmsg("conflicting key value violates exclusion constraint \"%s\"",
							RelationGetRelationName(index)),
					 error_new && error_existing ?
						errdetail("Key %s conflicts with existing key %s.",
								  error_new, error_existing) :
						errdetail("Key conflicts with existing key."),
					 errtableconstraint(heap,
										RelationGetRelationName(index))));
	}

	index_endscan(index_scan);

	/*
	 * Ordinarily, at this point the search should have found the originally
	 * inserted tuple, unless we exited the loop early because of conflict.
	 * However, it is possible to define exclusion constraints for which that
	 * wouldn't be true --- for instance, if the operator is <>. So we no
	 * longer complain if found_self is still false.
	 */

	econtext->ecxt_scantuple = save_scantuple;

	ExecDropSingleTupleTableSlot(existing_slot);

	return !conflict;
}

/*
 * Check existing tuple's index values to see if it really matches the
 * exclusion condition against the new_values.  Returns true if conflict.
 */
static bool
index_recheck_constraint(Relation index, Oid *constr_procs,
						 Datum *existing_values, bool *existing_isnull,
						 Datum *new_values)
{
	int			index_natts = index->rd_index->indnatts;
	int			i;

	for (i = 0; i < index_natts; i++)
	{
		/* Assume the exclusion operators are strict */
		if (existing_isnull[i])
			return false;

		if (!DatumGetBool(OidFunctionCall2Coll(constr_procs[i],
											   index->rd_indcollation[i],
											   existing_values[i],
											   new_values[i])))
			return false;
	}

	return true;
}

/*
 * ExecUpdateAOtupCount
 *		Update the tuple count on the master for an append only relation segfile.
 */
static void
ExecUpdateAOtupCount(ResultRelInfo *result_rels,
					 Snapshot shapshot,
					 int num_result_rels,
					 EState* estate,
					 uint64 tupadded)
{
	int		i;

	Assert(Gp_role == GP_ROLE_DISPATCH);

	bool was_delete = estate && estate->es_plannedstmt &&
		(estate->es_plannedstmt->commandType == CMD_DELETE);

	for (i = num_result_rels; i > 0; i--)
	{
		if(RelationIsAppendOptimized(result_rels->ri_RelationDesc))
		{
			Assert(result_rels->ri_aosegno != InvalidFileSegNumber);

			if (was_delete && tupadded > 0)
			{
				/* Touch the ao seg info */
				UpdateMasterAosegTotals(result_rels->ri_RelationDesc,
									result_rels->ri_aosegno,
									0,
									1);
			} 
			else if (!was_delete)
			{
				UpdateMasterAosegTotals(result_rels->ri_RelationDesc,
									result_rels->ri_aosegno,
									tupadded,
									1);
			}
		}

		result_rels++;
	}
}

/*
 * UpdateChangedParamSet
 *		Add changed parameters to a plan node's chgParam set
 */
void
UpdateChangedParamSet(PlanState *node, Bitmapset *newchg)
{
	Bitmapset  *parmset;

	/*
	 * The plan node only depends on params listed in its allParam set. Don't
	 * include anything else into its chgParam set.
	 */
	parmset = bms_intersect(node->plan->allParam, newchg);

	/*
	 * Keep node->chgParam == NULL if there's not actually any members; this
	 * allows the simplest possible tests in executor node files.
	 */
	if (!bms_is_empty(parmset))
		node->chgParam = bms_join(node->chgParam, parmset);
	else
		bms_free(parmset);
}

/*
 * Register a shutdown callback in an ExprContext.
 *
 * Shutdown callbacks will be called (in reverse order of registration)
 * when the ExprContext is deleted or rescanned.  This provides a hook
 * for functions called in the context to do any cleanup needed --- it's
 * particularly useful for functions returning sets.  Note that the
 * callback will *not* be called in the event that execution is aborted
 * by an error.
 */
void
RegisterExprContextCallback(ExprContext *econtext,
							ExprContextCallbackFunction function,
							Datum arg)
{
	ExprContext_CB *ecxt_callback;

	/* Save the info in appropriate memory context */
	ecxt_callback = (ExprContext_CB *)
		MemoryContextAlloc(econtext->ecxt_per_query_memory,
						   sizeof(ExprContext_CB));

	ecxt_callback->function = function;
	ecxt_callback->arg = arg;

	/* link to front of list for appropriate execution order */
	ecxt_callback->next = econtext->ecxt_callbacks;
	econtext->ecxt_callbacks = ecxt_callback;
}

/*
 * Deregister a shutdown callback in an ExprContext.
 *
 * Any list entries matching the function and arg will be removed.
 * This can be used if it's no longer necessary to call the callback.
 */
void
UnregisterExprContextCallback(ExprContext *econtext,
							  ExprContextCallbackFunction function,
							  Datum arg)
{
	ExprContext_CB **prev_callback;
	ExprContext_CB *ecxt_callback;

	prev_callback = &econtext->ecxt_callbacks;

	while ((ecxt_callback = *prev_callback) != NULL)
	{
		if (ecxt_callback->function == function && ecxt_callback->arg == arg)
		{
			*prev_callback = ecxt_callback->next;
			pfree(ecxt_callback);
		}
		else
			prev_callback = &ecxt_callback->next;
	}
}

/*
 * Call all the shutdown callbacks registered in an ExprContext.
 *
 * The callback list is emptied (important in case this is only a rescan
 * reset, and not deletion of the ExprContext).
 *
 * If isCommit is false, just clean the callback list but don't call 'em.
 * (See comment for FreeExprContext.)
 */
static void
ShutdownExprContext(ExprContext *econtext, bool isCommit)
{
	ExprContext_CB *ecxt_callback;
	MemoryContext oldcontext;

	/* Fast path in normal case where there's nothing to do. */
	if (econtext->ecxt_callbacks == NULL)
		return;

	/*
	 * Call the callbacks in econtext's per-tuple context.  This ensures that
	 * any memory they might leak will get cleaned up.
	 */
	oldcontext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

	/*
	 * Call each callback function in reverse registration order.
	 */
	while ((ecxt_callback = econtext->ecxt_callbacks) != NULL)
	{
		econtext->ecxt_callbacks = ecxt_callback->next;
		if (isCommit)
			(*ecxt_callback->function) (ecxt_callback->arg);
		pfree(ecxt_callback);
	}

	MemoryContextSwitchTo(oldcontext);
}


/* ---------------------------------------------------------------
 * 		Share Input utilities
 * ---------------------------------------------------------------
 */
ShareNodeEntry *
ExecGetShareNodeEntry(EState* estate, int shareidx, bool fCreate)
{
	Assert(shareidx >= 0);
	Assert(estate->es_sharenode != NULL);

	if(!fCreate)
	{
		if(shareidx >= list_length(*estate->es_sharenode))
			return NULL;
	}
	else
	{
		while(list_length(*estate->es_sharenode) <= shareidx)
		{
			ShareNodeEntry *n = makeNode(ShareNodeEntry);
			n->sharePlan = NULL;
			n->shareState = NULL;

			*estate->es_sharenode = lappend(*estate->es_sharenode, n);
		}
	}

	return (ShareNodeEntry *) list_nth(*estate->es_sharenode, shareidx);
}

/*
 * Prefetch JoinQual to prevent motion hazard.
 *
 * A motion hazard is a deadlock between motions, a classic motion hazard in a
 * join executor is formed by its inner and outer motions, it can be prevented
 * by prefetching the inner plan, refer to motion_sanity_check() for details.
 *
 * A similar motion hazard can be formed by the outer motion and the join qual
 * motion.  A join executor fetches a outer tuple, filters it with the join
 * qual, then repeat the process on all the outer tuples.  When there are
 * motions in both outer plan and the join qual then below state is possible:
 *
 * 0. processes A and B belong to the join slice, process C belongs to the
 *    outer slice, process D belongs to the JoinQual slice;
 * 1. A has read the first outer tuple and is fetching tuples from D;
 * 2. D is waiting for ACK from B;
 * 3. B is fetching the first outer tuple from C;
 * 4. C is waiting for ACK from A;
 *
 * So a deadlock is formed A->D->B->C->A.  We can prevent it also by
 * prefetching the join qual.
 *
 * An example is demonstrated and explained in test case
 * src/test/regress/sql/deadlock2.sql.
 *
 * Return true if the JoinQual is prefetched.
 */
bool
ExecPrefetchJoinQual(JoinState *node)
{
	EState	   *estate = node->ps.state;
	ExprContext *econtext = node->ps.ps_ExprContext;
	PlanState  *inner = innerPlanState(node);
	PlanState  *outer = outerPlanState(node);
	List	   *joinqual = node->joinqual;
	TupleTableSlot *innertuple = econtext->ecxt_innertuple;

	if (!joinqual)
		return false;

	/* Outer tuples should not be fetched before us */
	Assert(econtext->ecxt_outertuple == NULL);

	/* Build fake inner & outer tuples */
	econtext->ecxt_innertuple = ExecInitNullTupleSlot(estate,
													  ExecGetResultType(inner));
	econtext->ecxt_outertuple = ExecInitNullTupleSlot(estate,
													  ExecGetResultType(outer));

	/* Fetch subplan with the fake inner & outer tuples */
	ExecQual(joinqual, econtext, false);

	/* Restore previous state */
	econtext->ecxt_innertuple = innertuple;
	econtext->ecxt_outertuple = NULL;

	return true;
}

/*
 * Decide if should prefetch joinqual.
 *
 * Joinqual should be prefetched when both outer and joinqual contain motions.
 * In create_*join_plan() functions we set prefetch_joinqual according to the
 * outer motions, now we detect for joinqual motions to make the final
 * decision.
 *
 * See ExecPrefetchJoinQual() for details.
 *
 * This function should be called in ExecInit*Join() functions.
 *
 * Return true if JoinQual should be prefetched.
 */
bool
ShouldPrefetchJoinQual(EState *estate, Join *join)
{
	return (join->prefetch_joinqual &&
			findSenderMotion(estate->es_plannedstmt,
							 estate->currentSliceIdInPlan));
}

/* ----------------------------------------------------------------
 *		CDB Slice Table utilities
 * ----------------------------------------------------------------
 */

/* Attach a slice table to the given Estate structure.	It should
 * consist of blank slices, one for the root plan, one for each
 * Motion node (which roots a slice with a send node), and one for
 * each subplan (which acts as an initplan root node).
 */
void
InitSliceTable(EState *estate, int nMotions, int nSubplans)
{
	SliceTable *table;
	Slice	   *slice;
	int			i,
				n;
	MemoryContext oldcontext;

	n = 1 + nMotions + nSubplans;

	if (gp_max_slices > 0 && n > gp_max_slices)
		ereport(ERROR,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				 errmsg("at most %d slices are allowed in a query, current number: %d", gp_max_slices, n),
				 errhint("rewrite your query or adjust GUC gp_max_slices")));

	oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

	table = makeNode(SliceTable);
	table->nMotions = nMotions;
	table->nInitPlans = nSubplans;
	table->slices = NIL;
	table->instrument_options = INSTRUMENT_NONE;

	/* Each slice table has a unique-id. */
	table->ic_instance_id = ++gp_interconnect_id;

	for (i = 0; i < n; i++)
	{
		slice = makeNode(Slice);

		slice->sliceIndex = i;
        slice->rootIndex = (i > 0 && i <= nMotions) ? -1 : i;
		slice->gangType = GANGTYPE_UNALLOCATED;
		slice->gangSize = 0;
		slice->segments = NIL;
		slice->directDispatch.isDirectDispatch = false;
		slice->directDispatch.contentIds = NIL;
		slice->primaryGang = NULL;
		slice->parentIndex = -1;
		slice->children = NIL;
		slice->primaryProcesses = NIL;

		table->slices = lappend(table->slices, slice);
	}

	estate->es_sliceTable = table;

	MemoryContextSwitchTo(oldcontext);
}

/*
 * A forgiving slice table indexer that returns the indexed Slice* or NULL
 */
Slice *
getCurrentSlice(EState *estate, int sliceIndex)
{
	SliceTable *sliceTable = estate->es_sliceTable;

    if (sliceTable &&
        sliceIndex >= 0 &&
        sliceIndex < list_length(sliceTable->slices))
	    return (Slice *)list_nth(sliceTable->slices, sliceIndex);

    return NULL;
}

/* Should the slice run on the QD?
 *
 * N.B. Not the same as !sliceRunsOnQE(slice), when slice is NULL.
 */
bool
sliceRunsOnQD(Slice * slice)
{
	return (slice != NULL && slice->gangType == GANGTYPE_UNALLOCATED);
}


/* Should the slice run on a QE?
 *
 * N.B. Not the same as !sliceRunsOnQD(slice), when slice is NULL.
 */
bool
sliceRunsOnQE(Slice * slice)
{
	return (slice != NULL && slice->gangType != GANGTYPE_UNALLOCATED);
}

/**
 * Calculate the number of sending processes that should in be a slice.
 */
int
sliceCalculateNumSendingProcesses(Slice *slice)
{
	switch(slice->gangType)
	{
		case GANGTYPE_UNALLOCATED:
			return 0; /* does not send */

		case GANGTYPE_ENTRYDB_READER:
			return 1; /* on master */

		case GANGTYPE_SINGLETON_READER:
			return 1; /* on segment */

		case GANGTYPE_PRIMARY_WRITER:
		case GANGTYPE_PRIMARY_READER:
			if (slice->directDispatch.isDirectDispatch)
				return list_length(slice->directDispatch.contentIds);
			else
				return slice->gangSize;

		default:
			Insist(false);
			return -1;
	}
}

/* Forward declarations */
static void InventorySliceTree(CdbDispatcherState *ds, List * slices, int sliceIndex);

/*
 * Function AssignGangs runs on the QD and finishes construction of the
 * global slice table for a plan by assigning gangs allocated by the
 * executor factory to the slices of the slice table.
 *
 * On entry, the slice table (at queryDesc->estate->es_sliceTable) has
 * the correct structure (established by InitSliceTable) and has correct
 * gang types (established by function FillSliceTable).
 *
 * Gang assignment involves taking an inventory of the requirements of
 * each slice tree in the slice table, asking the executor factory to
 * allocate a minimal set of gangs that can satisfy any of the slice trees,
 * and associating the allocated gangs with slices in the slice table.
 *
 * On successful exit, the CDBProcess lists (primaryProcesses, mirrorProcesses)
 * and the Gang pointers (primaryGang, mirrorGang) are set correctly in each
 * slice in the slice table.
 */
void
AssignGangs(CdbDispatcherState *ds, QueryDesc *queryDesc)
{
	SliceTable	*sliceTable;
	ListCell  	*cell;
	Slice		*slice;
	EState		*estate;
	int			rootIdx;

	estate = queryDesc->estate;
	sliceTable = estate->es_sliceTable;
	rootIdx = RootSliceIndex(queryDesc->estate);

	/* cleanup processMap because initPlan and main Plan share the same slice table */
	foreach(cell, sliceTable->slices)
	{
		slice = (Slice *) lfirst(cell);
		slice->processesMap = NULL;
	}

	InventorySliceTree(ds, sliceTable->slices, rootIdx);
}

/*
 * Helper for AssignGangs takes a simple inventory of the gangs required
 * by a slice tree.  Recursive.  Closely coupled with AssignGangs.	Not
 * generally useful.
 */
void
InventorySliceTree(CdbDispatcherState *ds, List *slices, int sliceIndex)
{
	ListCell *cell;
	int childIndex;
	Slice *slice = list_nth(slices, sliceIndex);

	if (slice->gangType == GANGTYPE_UNALLOCATED)
	{
		slice->primaryGang = NULL;
		slice->primaryProcesses = getCdbProcessesForQD(true);
	}
	else
	{
		Assert(slice->segments != NIL);
		slice->primaryGang = AllocateGang(ds, slice->gangType, slice->segments);
		setupCdbProcessList(slice);
	}

	foreach(cell, slice->children)
	{
		childIndex = lfirst_int(cell);
		InventorySliceTree(ds, slices, childIndex);
	}
}

/*
 * Choose the execution identity (who does this executor serve?).
 * There are types:
 *
 * 1. No-Op (ignore) -- this occurs when the specified direction is
 *	 NoMovementScanDirection or when Gp_role is GP_ROLE_DISPATCH
 *	 and the current slice belongs to a QE.
 *
 * 2. Executor serves a Root Slice -- this occurs when Gp_role is
 *   GP_ROLE_UTILITY or the current slice is a root.  It corresponds
 *   to the "normal" path through the executor in that we enter the plan
 *   at the top and count on the motion nodes at the fringe of the top
 *   slice to return without ever calling nodes below them.
 *
 * 3. Executor serves a Non-Root Slice on a QE -- this occurs when
 *   Gp_role is GP_ROLE_EXECUTE and the current slice is not a root
 *   slice. It corresponds to a QE running a slice with a motion node on
 *	 top.  The call, thus, returns no tuples (since they all go out
 *	 on the interconnect to the receiver version of the motion node),
 *	 but it does execute the indicated slice down to any fringe
 *	 motion nodes (as in case 2).
 */
GpExecIdentity
getGpExecIdentity(QueryDesc *queryDesc,
				  ScanDirection direction,
				  EState	   *estate)
{
	Slice *currentSlice;

	currentSlice = getCurrentSlice(estate, LocallyExecutingSliceIndex(estate));
	if (currentSlice)
    {
        if (Gp_role == GP_ROLE_EXECUTE ||
            sliceRunsOnQD(currentSlice))
            currentSliceId = currentSlice->sliceIndex;
    }

	/* select the strategy */
	if (direction == NoMovementScanDirection)
	{
		return GP_IGNORE;
	}
	else if (Gp_role == GP_ROLE_DISPATCH && sliceRunsOnQE(currentSlice))
	{
		return GP_IGNORE;
	}
	else if (Gp_role == GP_ROLE_EXECUTE && LocallyExecutingSliceIndex(estate) != RootSliceIndex(estate))
	{
		return GP_NON_ROOT_ON_QE;
	}
	else
	{
		return GP_ROOT_SLICE;
	}
}

/*
 * End the gp-specific part of the executor.
 *
 * In here we collect the dispatch results if there are any, tear
 * down the interconnect if it is set-up.
 */
void mppExecutorFinishup(QueryDesc *queryDesc)
{
	EState	   *estate;
	Slice      *currentSlice;

	/* caller must have switched into per-query memory context already */
	estate = queryDesc->estate;

	currentSlice = getCurrentSlice(estate, LocallyExecutingSliceIndex(estate));

	/*
	 * If QD, wait for QEs to finish and check their results.
	 */
	if (estate->dispatcherState && estate->dispatcherState->primaryResults)
	{
		CdbDispatchResults *pr = NULL;
		CdbDispatcherState *ds = estate->dispatcherState;
		DispatchWaitMode waitMode = DISPATCH_WAIT_NONE;
		ErrorData *qeError = NULL;
		HTAB *aopartcounts = NULL;

		/*
		 * If we are finishing a query before all the tuples of the query
		 * plan were fetched we must call ExecSquelchNode before checking
		 * the dispatch results in order to tell the nodes below we no longer
		 * need any more tuples.
		 */
		if (!estate->es_got_eos)
		{
			ExecSquelchNode(queryDesc->planstate);
		}

		/*
		 * Wait for completion of all QEs.  We send a "graceful" query
		 * finish, not cancel signal.  Since the query has succeeded,
		 * don't confuse QEs by sending erroneous message.
		 */
		if (estate->cancelUnfinished)
			waitMode = DISPATCH_WAIT_FINISH;

		cdbdisp_checkDispatchResult(ds, waitMode);

		pr = cdbdisp_getDispatchResults(ds, &qeError);		

		if (qeError)
		{
			estate->dispatcherState = NULL;
			FlushErrorState();
			ReThrowError(qeError);
		}

		/* If top slice was delegated to QEs, get num of rows processed. */
		int primaryWriterSliceIndex = PrimaryWriterSliceIndex(estate);
		//if (sliceRunsOnQE(currentSlice))
		{
			estate->es_processed +=
				cdbdisp_sumCmdTuples(pr, primaryWriterSliceIndex);
			estate->es_lastoid =
				cdbdisp_maxLastOid(pr, primaryWriterSliceIndex);

			if (estate->es_result_partitions)
				aopartcounts = cdbdisp_sumAoPartTupCount(pr);
		}

		/* sum up rejected rows if any (single row error handling only) */
		cdbdisp_sumRejectedRows(pr);

		/* sum up inserted rows into any AO relation */
		if (aopartcounts)
		{
			/* counts from a partitioned AO table */

			ListCell *lc;

			foreach(lc, estate->es_result_aosegnos)
			{
				SegfileMapNode *map = lfirst(lc);
				struct {
					Oid relid;
			   		int64 tupcount;
				} *entry;
				bool found;

				entry = hash_search(aopartcounts,
									&(map->relid),
									HASH_FIND,
									&found);

				/*
				 * Must update the mod count only for segfiles where actual tuples were touched 
				 * (added/deleted) based on entry->tupcount.
				 */
				if (found && entry->tupcount)
				{
					bool was_delete = estate->es_plannedstmt && (estate->es_plannedstmt->commandType == CMD_DELETE);

					Relation r = heap_open(map->relid, AccessShareLock);
					if (was_delete)
					{
						UpdateMasterAosegTotals(r, map->segno, 0, 1);
					}
					else
					{
						UpdateMasterAosegTotals(r, map->segno, entry->tupcount, 1);	
					}
					heap_close(r, NoLock);
				}
			}
		}
		else
		{
			/* counts from a (non partitioned) AO table */

			ExecUpdateAOtupCount(estate->es_result_relations,
								 estate->es_snapshot,
								 estate->es_num_result_relations,
								 estate,
								 estate->es_processed);
		}

		/*
		 * Check and free the results of all gangs. If any QE had an
		 * error, report it and exit to our error handler via PG_THROW.
		 * NB: This call doesn't wait, because we already waited above.
		 */
		estate->dispatcherState = NULL;
		cdbdisp_destroyDispatcherState(ds);
	}

	/* Teardown the Interconnect */
	if (estate->es_interconnect_is_setup)
	{
		/*
		 * MPP-3413: If we got here during cancellation of a cursor,
		 * we need to set the "forceEos" argument correctly --
		 * otherwise we potentially hang (cursors cancel on the QEs,
		 * mark the estate to "cancelUnfinished" and then try to do a
		 * normal interconnect teardown).
		 */
		TeardownInterconnect(estate->interconnect_context, estate->cancelUnfinished, false);
		estate->es_interconnect_is_setup = false;
	}
}

/*
 * Cleanup the gp-specific parts of the query executor.
 *
 * Will normally be called after an error from within a CATCH block.
 */
void mppExecutorCleanup(QueryDesc *queryDesc)
{
	CdbDispatcherState *ds;
	EState	   *estate;

	/* caller must have switched into per-query memory context already */
	estate = queryDesc->estate;
	ds = estate->dispatcherState;

	/* GPDB hook for collecting query info */
	if (query_info_collect_hook && QueryCancelCleanup)
		(*query_info_collect_hook)(METRICS_QUERY_CANCELING, queryDesc);

	/*
	 * If this query is being canceled, record that when the gpperfmon
	 * is enabled.
	 */
	if (gp_enable_gpperfmon &&
		Gp_role == GP_ROLE_DISPATCH &&
		queryDesc->gpmon_pkt &&
		QueryCancelCleanup)
	{			
		gpmon_qlog_query_canceling(queryDesc->gpmon_pkt);
	}

	/*
	 * Request any commands still executing on qExecs to stop.
	 * Wait for them to finish and clean up the dispatching structures.
	 * Replace current error info with QE error info if more interesting.
	 */
	if (ds)
	{
		estate->dispatcherState = NULL;
		CdbDispatchHandleError(ds);
	}

	/* Clean up the interconnect. */
	if (estate->es_interconnect_is_setup)
	{
		TeardownInterconnect(estate->interconnect_context, true /* force EOS */, true);
		estate->es_interconnect_is_setup = false;
	}

	/* GPDB hook for collecting query info */
	if (query_info_collect_hook)
		(*query_info_collect_hook)(QueryCancelCleanup ? METRICS_QUERY_CANCELED : METRICS_QUERY_ERROR, queryDesc);
	
	/**
	 * Perfmon related stuff.
	 */
	if (gp_enable_gpperfmon 
			&& Gp_role == GP_ROLE_DISPATCH
			&& queryDesc->gpmon_pkt)
	{			
		gpmon_qlog_query_error(queryDesc->gpmon_pkt);
		pfree(queryDesc->gpmon_pkt);
		queryDesc->gpmon_pkt = NULL;
	}

	ReportOOMConsumption();

	/**
	 * Since there was an error, clean up the function scan stack.
	 */
	if (!IsResManagerMemoryPolicyNone())
	{
		SPI_InitMemoryReservation();
	}
}

void ResetExprContext(ExprContext *econtext)
{
	MemoryContext memctxt = econtext->ecxt_per_tuple_memory;
	if(memctxt->allBytesAlloc - memctxt->allBytesFreed > 50000)
		MemoryContextReset(memctxt);
}

/**
 * This method is used to determine how much memory a specific operator
 * is supposed to use (in KB). 
 */
uint64 PlanStateOperatorMemKB(const PlanState *ps)
{
	Assert(ps);
	Assert(ps->plan);
	uint64 result = 0;
	if (ps->plan->operatorMemKB == 0)
	{
		/**
		 * There are some statements that do not go through the resource queue and these
		 * plans dont get decorated with the operatorMemKB. Someday, we should fix resource queues.
		 */
		result = work_mem;
	}
	else
	{
		if (IsA(ps, AggState))
		{
			/* Retrieve all relinquished memory (quota the other node not using) */
			result = ps->plan->operatorMemKB + (MemoryAccounting_RequestQuotaIncrease() >> 10);
		}
		else
			result = ps->plan->operatorMemKB;
	}
	
	return result;
}

/**
 * Methods to find motionstate object within a planstate tree given a motion id (which is the same as slice index)
 */
typedef struct MotionStateFinderContext
{
	int motionId; /* Input */
	MotionState *motionState; /* Output */
} MotionStateFinderContext;

/**
 * Walker method that finds motion state node within a planstate tree.
 */
static CdbVisitOpt
MotionStateFinderWalker(PlanState *node,
				  void *context)
{
	Assert(context);
	MotionStateFinderContext *ctx = (MotionStateFinderContext *) context;

	if (IsA(node, MotionState))
	{
		MotionState *ms = (MotionState *) node;
		Motion *m = (Motion *) ms->ps.plan;
		if (m->motionID == ctx->motionId)
		{
			Assert(ctx->motionState == NULL);
			ctx->motionState = ms;
			return CdbVisit_Skip;	/* don't visit subtree */
		}
	}

	/* Continue walking */
	return CdbVisit_Walk;
}

/**
 * Given a slice index, find the motionstate that corresponds to this slice index. This will iterate over the planstate tree
 * to get the right node.
 */
MotionState *
getMotionState(struct PlanState *ps, int sliceIndex)
{
	Assert(ps);
	Assert(sliceIndex > -1);

	MotionStateFinderContext ctx;
	ctx.motionId = sliceIndex;
	ctx.motionState = NULL;
	planstate_walk_node(ps, MotionStateFinderWalker, &ctx);

	if (ctx.motionState == NULL)
		elog(ERROR, "could not find MotionState in executor tree");

	return ctx.motionState;
}

typedef struct MotionFinderContext
{
	plan_tree_base_prefix base; /* Required prefix for plan_tree_walker/mutator */
	int motionId; /* Input */
	Motion *motion; /* Output */
} MotionFinderContext;

/*
 * Walker to find a motion node that matches a particular motionID
 */
static bool
MotionFinderWalker(Plan *node,
				  void *context)
{
	Assert(context);
	MotionFinderContext *ctx = (MotionFinderContext *) context;


	if (node == NULL)
		return false;

	if (IsA(node, Motion))
	{
		Motion *m = (Motion *) node;
		if (m->motionID == ctx->motionId)
		{
			ctx->motion = m;
			return true;	/* found our node; no more visit */
		}
	}

	/* Continue walking */
	return plan_tree_walker((Node*)node, MotionFinderWalker, ctx);
}

/*
 * Given the Plan and a Slice index, find the motion node that is the root of the slice's subtree.
 */
Motion *findSenderMotion(PlannedStmt *plannedstmt, int sliceIndex)
{
	Assert(sliceIndex > -1);

	Plan *planTree = plannedstmt->planTree;
	MotionFinderContext ctx;
	ctx.base.node = (Node*)plannedstmt;
	ctx.motionId = sliceIndex;
	ctx.motion = NULL;
	MotionFinderWalker(planTree, &ctx);
	return ctx.motion;
}

typedef struct SubPlanFinderContext
{
	plan_tree_base_prefix base; /* Required prefix for plan_tree_walker/mutator */
	Bitmapset *bms_subplans; /* Bitmapset for relevant subplans in current slice */
} SubPlanFinderContext;

/*
 * Walker to find all the subplans in a PlanTree between 'node' and the next motion node
 */
static bool
SubPlanFinderWalker(Plan *node,
				  void *context)
{
	Assert(context);
	SubPlanFinderContext *ctx = (SubPlanFinderContext *) context;

	if (node == NULL || IsA(node, Motion))
	{
		return false;	/* don't visit subtree */
	}

	if (IsA(node, SubPlan))
	{
		SubPlan *subplan = (SubPlan *) node;
		int i = subplan->plan_id - 1;
		if (!bms_is_member(i, ctx->bms_subplans))
			ctx->bms_subplans = bms_add_member(ctx->bms_subplans, i);
		else
			return false;
	 }

	/* Continue walking */
	return plan_tree_walker((Node*)node, SubPlanFinderWalker, ctx);
}

/*
 * Given a plan and a root motion node find all the subplans
 * between 'root' and the next motion node in the tree
 */
Bitmapset *getLocallyExecutableSubplans(PlannedStmt *plannedstmt, Plan *root)
{
	SubPlanFinderContext ctx;
	Plan* root_plan = root;
	if (IsA(root, Motion))
	{
		root_plan = outerPlan(root);
	}
	ctx.base.node = (Node*)plannedstmt;
	ctx.bms_subplans = NULL;
	SubPlanFinderWalker(root_plan, &ctx);
	return ctx.bms_subplans;
}

typedef struct ParamExtractorContext
{
	plan_tree_base_prefix base; /* Required prefix for plan_tree_walker/mutator */
	EState *estate;
} ParamExtractorContext;

/*
 * Given a subplan determine if it is an initPlan (subplan->is_initplan) then copy its params
 * from estate-> es_param_list_info to estate->es_param_exec_vals.
 */
static void ExtractSubPlanParam(SubPlan *subplan, EState *estate)
{
	/*
	 * If this plan is un-correlated or undirect correlated one and want to
	 * set params for parent plan then mark parameters as needing evaluation.
	 *
	 * Note that in the case of un-correlated subqueries we don't care about
	 * setting parent->chgParam here: indices take care about it, for others -
	 * it doesn't matter...
	 */
	if (subplan->setParam != NIL)
	{
		ListCell   *lst;

		foreach(lst, subplan->setParam)
		{
			int			paramid = lfirst_int(lst);
			ParamExecData *prmExec = &(estate->es_param_exec_vals[paramid]);

			/**
			 * Has this parameter been already
			 * evaluated as part of preprocess_initplan()? If so,
			 * we shouldn't re-evaluate it. If it has been evaluated,
			 * we will simply substitute the actual value from
			 * the external parameters.
			 */
			if (Gp_role == GP_ROLE_EXECUTE && subplan->is_initplan)
			{
				ParamListInfo paramInfo = estate->es_param_list_info;
				ParamExternData *prmExt = NULL;
				int extParamIndex = -1;

				Assert(paramInfo);
				Assert(paramInfo->numParams > 0);

				/*
				 * To locate the value of this pre-evaluated parameter, we need to find
				 * its location in the external parameter list.
				 */
				extParamIndex = paramInfo->numParams - estate->es_plannedstmt->nParamExec + paramid;
				prmExt = &paramInfo->params[extParamIndex];

				/* Make sure the types are valid */
				if (!OidIsValid(prmExt->ptype))
				{
					prmExec->execPlan = NULL;
					prmExec->isnull = true;
					prmExec->value = (Datum) 0;
				}
				else
				{
					/** Hurray! Copy value from external parameter and don't bother setting up execPlan. */
					prmExec->execPlan = NULL;
					prmExec->isnull = prmExt->isnull;
					prmExec->value = prmExt->value;
				}
			}
		}
	}
}

/*
 * Walker to extract all the precomputer InitPlan params in a plan tree.
 */
static bool
ParamExtractorWalker(Plan *node,
				  void *context)
{
	Assert(context);
	ParamExtractorContext *ctx = (ParamExtractorContext *) context;

	/* Assuming InitPlan always runs on the master */
	if (node == NULL)
	{
		return false;	/* don't visit subtree */
	}

	if (IsA(node, SubPlan))
	{
		SubPlan *sub_plan = (SubPlan *) node;
		ExtractSubPlanParam(sub_plan, ctx->estate);
	}

	/* Continue walking */
	return plan_tree_walker((Node*)node, ParamExtractorWalker, ctx);
}

/*
 * Find and extract all the InitPlan setParams in a root node's subtree.
 */
void ExtractParamsFromInitPlans(PlannedStmt *plannedstmt, Plan *root, EState *estate)
{
	ParamExtractorContext ctx;
	ctx.base.node = (Node*)plannedstmt;
	ctx.estate = estate;

	/* If gather motion shows up at top, we still need to find master only init plan */
	if (IsA(root, Motion))
	{
		root = outerPlan(root);
	}
	ParamExtractorWalker(root, &ctx);
}

typedef struct MotionAssignerContext
{
	plan_tree_base_prefix base; /* Required prefix for plan_tree_walker/mutator */
	List *motStack; /* Motion Stack */
} MotionAssignerContext;

/*
 * Walker to set plan->motionNode for every Plan node to its corresponding parent
 * motion node.
 *
 * This function maintains a stack of motion nodes. When we encounter a motion node
 * we push it on to the stack, walk its subtree, and then pop it off the stack.
 * When we encounter any plan node (motion nodes included) we assign its plan->motionNode
 * to the top of the stack.
 *
 * NOTE: Motion nodes will have their motionNode value set to the previous motion node
 * we encountered while walking the subtree.
 */
static bool
MotionAssignerWalker(Plan *node,
				  void *context)
{
	if (node == NULL) return false;

	Assert(context);
	MotionAssignerContext *ctx = (MotionAssignerContext *) context;

	if (is_plan_node((Node*)node))
	{
		Plan *plan = (Plan *) node;
		/*
		 * TODO: For cached plan we may be assigning multiple times.
		 * The eventual goal is to relocate it to planner. For now,
		 * ignore already assigned nodes.
		 */
		if (NULL != plan->motionNode)
			return true;
		plan->motionNode = ctx->motStack != NIL ? (Plan *) lfirst(list_head(ctx->motStack)) : NULL;
	}

	/*
	 * Subplans get dynamic motion assignment as they can be executed from
	 * arbitrary expressions. So, we don't assign any motion to these nodes.
	 */
	if (IsA(node, SubPlan))
	{
		return false;
	}

	if (IsA(node, Motion))
	{
		ctx->motStack = lcons(node, ctx->motStack);
		plan_tree_walker((Node *)node, MotionAssignerWalker, ctx);
		ctx->motStack = list_delete_first(ctx->motStack);

		return false;
	}

	/* Continue walking */
	return plan_tree_walker((Node*)node, MotionAssignerWalker, ctx);
}

/*
 * Assign every node in plannedstmt->planTree its corresponding
 * parent Motion Node if it has one
 *
 * NOTE: Some plans may not be rooted by a motion on the segment so
 * this function does not guarantee that every node will have a non-NULL
 * motionNode value.
 */
void AssignParentMotionToPlanNodes(PlannedStmt *plannedstmt)
{
	MotionAssignerContext ctx;
	ctx.base.node = (Node*)plannedstmt;
	ctx.motStack = NIL;

	MotionAssignerWalker(plannedstmt->planTree, &ctx);
	/* The entire motion stack should have been unwounded */
	Assert(ctx.motStack == NIL);
}

/**
 * Provide index of locally executing slice
 */
int LocallyExecutingSliceIndex(EState *estate)
{
	Assert(estate);
	return (!estate->es_sliceTable ? 0 : estate->es_sliceTable->localSlice);
}

/**
 * Provide index of slice being executed on the primary writer gang
 */
int PrimaryWriterSliceIndex(EState *estate)
{
	ListCell   *lc;

	Assert(estate);

	if (!estate->es_sliceTable)
		return 0;

	foreach (lc, estate->es_sliceTable->slices)
	{
		Slice	   *slice = (Slice *) lfirst(lc);

		if (slice->gangType == GANGTYPE_PRIMARY_WRITER)
			return slice->sliceIndex;
	}

	return 0;
}

/**
 * Provide root slice of locally executing slice.
 */
int RootSliceIndex(EState *estate)
{
	Assert(estate);
	int result = 0;

	if (estate->es_sliceTable)
	{
		Slice *localSlice = list_nth(estate->es_sliceTable->slices, LocallyExecutingSliceIndex(estate));
		result = localSlice->rootIndex;
	}

	return result;
}


#ifdef USE_ASSERT_CHECKING
/**
 * Assert that slicetable is valid. Must be called after ExecInitMotion, which sets up the slice table
 */
void AssertSliceTableIsValid(SliceTable *st, struct PlannedStmt *pstmt)
{
	if (!st)
		return;

	Assert(pstmt);

	Assert(pstmt->nMotionNodes == st->nMotions);
	Assert(pstmt->nInitPlans == st->nInitPlans);

	ListCell *lc = NULL;
	int i = 0;

	int maxIndex = st->nMotions + st->nInitPlans + 1;

	Assert(maxIndex == list_length(st->slices));

	foreach_with_count(lc, st->slices, i)
	{
		Slice *s = (Slice *) lfirst(lc);

		/* The n-th slice entry has sliceIndex of n */
		Assert(s->sliceIndex == i && "slice index incorrect");

		/* The root index of a slice is either 0 or is a slice corresponding to an init plan */
		Assert((s->rootIndex == 0) || (s->rootIndex > st->nMotions && s->rootIndex < maxIndex));

		/* Parent slice index */
		if (s->sliceIndex == s->rootIndex)
		{
			/* Current slice is a root slice. It will have parent index -1.*/
			Assert(s->parentIndex == -1 && "expecting parent index of -1");
		}
		else
		{
			/* All other slices must have a valid parent index */
			Assert(s->parentIndex >= 0 && s->parentIndex < maxIndex && "slice's parent index out of range");
		}

		/* Current slice's children must consider it the parent */
		ListCell *lc1 = NULL;
		foreach (lc1, s->children)
		{
			int childIndex = lfirst_int(lc1);
			Assert(childIndex >= 0 && childIndex < maxIndex && "invalid child slice");
			Slice *sc = (Slice *) list_nth(st->slices, childIndex);
			Assert(sc->parentIndex == s->sliceIndex && "slice's child does not consider it the parent");
		}

		/* Current slice must be in its parent's children list */
		if (s->parentIndex >= 0)
		{
			Slice *sp = (Slice *) list_nth(st->slices, s->parentIndex);

			bool found = false;
			foreach (lc1, sp->children)
			{
				int childIndex = lfirst_int(lc1);
				Assert(childIndex >= 0 && childIndex < maxIndex && "invalid child slice");
				Slice *sc = (Slice *) list_nth(st->slices, childIndex);

				if (sc->sliceIndex == s->sliceIndex)
				{
					found = true;
					break;
				}
			}

			Assert(found && "slice's parent does not consider it a child");
		}
	}
}
#endif
