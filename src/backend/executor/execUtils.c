/*-------------------------------------------------------------------------
 *
 * execUtils.c
 *	  miscellaneous executor utility routines
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2016, PostgreSQL Global Development Group
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
#include "executor/executor.h"
#include "nodes/nodeFuncs.h"
#include "parser/parsetree.h"
#include "utils/memutils.h"
#include "utils/rel.h"

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
static void ShutdownExprContext(ExprContext *econtext, bool isCommit);
static List *flatten_logic_exprs(Node *node);


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
	MemoryContextDeclareAccountingRoot(qcontext);

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

	estate->currentSliceId = 0;
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
	 * overall targetlist's econtext.  GroupingFunc arguments are never
	 * evaluated at all.
	 */
	if (IsA(node, Aggref))
		return false;
	if (IsA(node, WindowFunc))
		return false;
	if (IsA(node, GroupingFunc))
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
		/* Keep this check in sync with InitPlan! */
		ExecRowMark *erm = ExecFindRowMark(estate, scanrelid, true);

		if (erm != NULL && erm->relation != NULL)
			lockmode = NoLock;
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

/*
 * flatten_logic_exprs
 * This function is only used by ExecPrefetchJoinQual.
 * ExecPrefetchJoinQual need to prefetch subplan in join
 * qual that contains motion to materialize it to avoid
 * motion deadlock. This function is going to flatten
 * the bool exprs to avoid shortcut of bool logic.
 * An example is:
 * (a and b or c) or (d or e and f or g) and (h and i or j)
 * will be transformed to
 * (a, b, c, d, e, f, g, h, i, j).
 */
static List *
flatten_logic_exprs(Node *node)
{
	if (node == NULL)
		return NIL;

	if (IsA(node, BoolExprState))
	{
		BoolExprState *be = (BoolExprState *) node;
		return flatten_logic_exprs((Node *) (be->args));
	}

	if (IsA(node, List))
	{
		List     *es = (List *) node;
		List     *result = NIL;
		ListCell *lc = NULL;

		foreach(lc, es)
		{
			Node *n = (Node *) lfirst(lc);
			result = list_concat(result,
								 flatten_logic_exprs(n));
		}

		return result;
	}

	return list_make1(node);
}

/*
 * fake_outer_params
 *   helper function to fake the nestloop's nestParams
 *   so that prefetch inner or prefetch joinqual will
 *   not encounter NULL pointer reference issue. It is
 *   only invoked in ExecNestLoop and ExecPrefetchJoinQual
 *   when the join is a nestloop join.
 */
void
fake_outer_params(JoinState *node)
{
	ExprContext    *econtext = node->ps.ps_ExprContext;
	PlanState      *inner = innerPlanState(node);
	TupleTableSlot *outerTupleSlot = econtext->ecxt_outertuple;
	NestLoop       *nl = (NestLoop *) (node->ps.plan);
	ListCell       *lc = NULL;

	/* only nestloop contains nestParams */
	Assert(IsA(node->ps.plan, NestLoop));

	/* econtext->ecxt_outertuple must have been set fakely. */
	Assert(outerTupleSlot != NULL);
	/*
	 * fetch the values of any outer Vars that must be passed to the
	 * inner scan, and store them in the appropriate PARAM_EXEC slots.
	 */
	foreach(lc, nl->nestParams)
	{
		NestLoopParam *nlp = (NestLoopParam *) lfirst(lc);
		int			paramno = nlp->paramno;
		ParamExecData *prm;

		prm = &(econtext->ecxt_param_exec_vals[paramno]);
		/* Param value should be an OUTER_VAR var */
		Assert(IsA(nlp->paramval, Var));
		Assert(nlp->paramval->varno == OUTER_VAR);
		Assert(nlp->paramval->varattno > 0);
		prm->value = slot_getattr(outerTupleSlot,
								  nlp->paramval->varattno,
								  &(prm->isnull));
		/* Flag parameter value as changed */
		inner->chgParam = bms_add_member(inner->chgParam,
										 paramno);
	}
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
void
ExecPrefetchJoinQual(JoinState *node)
{
	EState	   *estate = node->ps.state;
	ExprContext *econtext = node->ps.ps_ExprContext;
	PlanState  *inner = innerPlanState(node);
	PlanState  *outer = outerPlanState(node);
	List	   *joinqual = node->joinqual;
	TupleTableSlot *innertuple = econtext->ecxt_innertuple;

	ListCell   *lc = NULL;
	List       *quals = NIL;

	Assert(joinqual);

	/* Outer tuples should not be fetched before us */
	Assert(econtext->ecxt_outertuple == NULL);

	/* Build fake inner & outer tuples */
	econtext->ecxt_innertuple = ExecInitNullTupleSlot(estate,
													  ExecGetResultType(inner));
	econtext->ecxt_outertuple = ExecInitNullTupleSlot(estate,
													  ExecGetResultType(outer));

	if (IsA(node->ps.plan, NestLoop))
	{
		NestLoop *nl = (NestLoop *) (node->ps.plan);
		if (nl->nestParams)
			fake_outer_params(node);
	}

	quals = flatten_logic_exprs((Node *) joinqual);

	/* Fetch subplan with the fake inner & outer tuples */
	foreach(lc, quals)
	{
		/*
		 * Force every joinqual is prefech because
		 * our target is to materialize motion node.
		 */
		ExprState  *clause = (ExprState *) lfirst(lc);
		(void) ExecQual(list_make1(clause), econtext, false);
	}

	/* Restore previous state */
	econtext->ecxt_innertuple = innertuple;
	econtext->ecxt_outertuple = NULL;
}

/* ----------------------------------------------------------------
 *		CDB Slice Table utilities
 * ----------------------------------------------------------------
 */


static void
FillSliceGangInfo(ExecSlice *slice, PlanSlice *ps)
{
	int numsegments = ps->numsegments;
	DirectDispatchInfo *dd = &ps->directDispatch;

	switch (slice->gangType)
	{
		case GANGTYPE_UNALLOCATED:
			/*
			 * It's either the root slice or an InitPlan slice that runs in
			 * the QD process, or really unused slice.
			 */
			slice->planNumSegments = 1;
			break;
		case GANGTYPE_PRIMARY_WRITER:
		case GANGTYPE_PRIMARY_READER:
			slice->planNumSegments = numsegments;
			if (dd->isDirectDispatch)
			{
				slice->segments = list_copy(dd->contentIds);
			}
			else
			{
				int i;
				slice->segments = NIL;
				for (i = 0; i < numsegments; i++)
					slice->segments = lappend_int(slice->segments, i);
			}
			break;
		case GANGTYPE_ENTRYDB_READER:
			slice->planNumSegments = 1;
			slice->segments = list_make1_int(-1);
			break;
		case GANGTYPE_SINGLETON_READER:
			slice->planNumSegments = 1;
			slice->segments = list_make1_int(ps->segindex);
			break;
		default:
			elog(ERROR, "unexpected gang type");
	}
}

/*
 * Create the executor slice table.
 *
 * The planner constructed a slice table, in plannedstmt->slices. Turn that
 * into an "executor slice table", with slightly more information. The gangs
 * to execute the slices will be set up later.
 */
SliceTable *
InitSliceTable(EState *estate, PlannedStmt *plannedstmt)
{
	SliceTable *table;
	int			i;
	int			numSlices;
	MemoryContext oldcontext;

	numSlices = plannedstmt->numSlices;
	Assert(numSlices > 0);

	if (gp_max_slices > 0 && numSlices > gp_max_slices)
		ereport(ERROR,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				 errmsg("at most %d slices are allowed in a query, current number: %d",
						gp_max_slices, numSlices),
				 errhint("rewrite your query or adjust GUC gp_max_slices")));

	oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

	table = makeNode(SliceTable);
	table->instrument_options = INSTRUMENT_NONE;
	table->hasMotions = false;

	/*
	 * Initialize the executor slice table.
	 *
	 * We have most of the information in the planner slice table. In addition to that,
	 * we set up the parent-child relationships.
	 */
	table->slices = palloc0(sizeof(ExecSlice) * numSlices);
	for (i = 0; i < numSlices; i++)
	{
		ExecSlice  *currExecSlice = &table->slices[i];
		PlanSlice  *currPlanSlice = &plannedstmt->slices[i];
		int			parentIndex;
		int			rootIndex;

		currExecSlice->sliceIndex = i;
		currExecSlice->planNumSegments = currPlanSlice->numsegments;
		currExecSlice->segments = NIL;
		currExecSlice->primaryGang = NULL;
		currExecSlice->primaryProcesses = NIL;

		parentIndex = currPlanSlice->parentIndex;
		if (parentIndex < -1 || parentIndex >= numSlices)
			elog(ERROR, "invalid parent slice index %d", currPlanSlice->parentIndex);

		if (parentIndex >= 0)
		{
			ExecSlice *parentExecSlice = &table->slices[parentIndex];
			int			counter;

			/* Sending slice is a child of recv slice */
			parentExecSlice->children = lappend_int(parentExecSlice->children, currPlanSlice->sliceIndex);

			/* Find the root slice */
			rootIndex = i;
			counter = 0;
			while (plannedstmt->slices[rootIndex].parentIndex >= 0)
			{
				rootIndex = plannedstmt->slices[rootIndex].parentIndex;

				if (counter++ > numSlices)
					elog(ERROR, "circular parent-child relationship in slice table");
			}
			table->hasMotions = true;
		}
		else
			rootIndex = i;

		/* find root of this slice. All the parents should be initialized already */

		currExecSlice->parentIndex = parentIndex;
		currExecSlice->rootIndex = rootIndex;
		currExecSlice->gangType = currPlanSlice->gangType;

		FillSliceGangInfo(currExecSlice, currPlanSlice);
	}
	table->numSlices = numSlices;

	/*
	 * For CTAS although the data is distributed on part of the
	 * segments, the catalog changes must be dispatched to all the
	 * segments, so a full gang is required.
	 */
	if ((plannedstmt->intoClause != NULL || plannedstmt->copyIntoClause != NULL || plannedstmt->refreshClause))
	{
		if (table->slices[0].gangType == GANGTYPE_PRIMARY_WRITER)
		{
			int			numsegments = getgpsegmentCount();

			table->slices[0].segments = NIL;
			for (i = 0; i < numsegments; i++)
				table->slices[0].segments = lappend_int(table->slices[0].segments, i);
		}
	}

	MemoryContextSwitchTo(oldcontext);

	return table;
}

/*
 * A forgiving slice table indexer that returns the indexed Slice* or NULL
 */
ExecSlice *
getCurrentSlice(EState *estate, int sliceIndex)
{
	SliceTable *sliceTable = estate->es_sliceTable;

    if (sliceTable &&
		sliceIndex >= 0 &&
		sliceIndex < sliceTable->numSlices)
		return &sliceTable->slices[sliceIndex];

    return NULL;
}

/* Should the slice run on the QD?
 *
 * N.B. Not the same as !sliceRunsOnQE(slice), when slice is NULL.
 */
bool
sliceRunsOnQD(ExecSlice *slice)
{
	return (slice != NULL && slice->gangType == GANGTYPE_UNALLOCATED);
}


/* Should the slice run on a QE?
 *
 * N.B. Not the same as !sliceRunsOnQD(slice), when slice is NULL.
 */
bool
sliceRunsOnQE(ExecSlice *slice)
{
	return (slice != NULL && slice->gangType != GANGTYPE_UNALLOCATED);
}

/* Forward declarations */
static void InventorySliceTree(CdbDispatcherState *ds, SliceTable *sliceTable, int sliceIndex);

/*
 * Function AssignGangs runs on the QD and finishes construction of the
 * global slice table for a plan by assigning gangs allocated by the
 * executor factory to the slices of the slice table.
 *
 * On entry, the executor slice table (at queryDesc->estate->es_sliceTable)
 * has been initialized and has correct (by InitSliceTable function)
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
	EState		*estate;
	int			rootIdx;

	estate = queryDesc->estate;
	sliceTable = estate->es_sliceTable;
	rootIdx = RootSliceIndex(queryDesc->estate);

	/* cleanup processMap because initPlan and main Plan share the same slice table */
	for (int i = 0; i < sliceTable->numSlices; i++)
		sliceTable->slices[i].processesMap = NULL;

	InventorySliceTree(ds, sliceTable, rootIdx);
}

/*
 * Helper for AssignGangs takes a simple inventory of the gangs required
 * by a slice tree.  Recursive.  Closely coupled with AssignGangs.	Not
 * generally useful.
 */
static void
InventorySliceTree(CdbDispatcherState *ds, SliceTable *sliceTable, int sliceIndex)
{
	ExecSlice *slice = &sliceTable->slices[sliceIndex];
	ListCell *cell;

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
		int			childIndex = lfirst_int(cell);

		InventorySliceTree(ds, sliceTable, childIndex);
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
	ExecSlice *currentSlice;

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
	ExecSlice  *currentSlice;

	/* caller must have switched into per-query memory context already */
	estate = queryDesc->estate;

	currentSlice = getCurrentSlice(estate, LocallyExecutingSliceIndex(estate));

	/* Teardown the Interconnect */
	if (estate->es_interconnect_is_setup)
	{
		TeardownInterconnect(estate->interconnect_context, false);
		estate->interconnect_context = NULL;
		estate->es_interconnect_is_setup = false;
	}

	/*
	 * If QD, wait for QEs to finish and check their results.
	 */
	if (estate->dispatcherState && estate->dispatcherState->primaryResults)
	{
		CdbDispatchResults *pr = NULL;
		CdbDispatcherState *ds = estate->dispatcherState;
		DispatchWaitMode waitMode = DISPATCH_WAIT_NONE;
		ErrorData *qeError = NULL;

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
		}

		/* sum up rejected rows if any (single row error handling only) */
		cdbdisp_sumRejectedRows(pr);

		/*
		 * Check and free the results of all gangs. If any QE had an
		 * error, report it and exit to our error handler via PG_THROW.
		 * NB: This call doesn't wait, because we already waited above.
		 */
		estate->dispatcherState = NULL;
		cdbdisp_destroyDispatcherState(ds);
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
	/* Clean up the interconnect. */
	if (estate->es_interconnect_is_setup)
	{
		TeardownInterconnect(estate->interconnect_context, true);
		estate->es_interconnect_is_setup = false;
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

	/* GPDB hook for collecting query info */
	if (query_info_collect_hook)
		(*query_info_collect_hook)(QueryCancelCleanup ? METRICS_QUERY_CANCELED : METRICS_QUERY_ERROR, queryDesc);
	
	ReportOOMConsumption();

	/**
	 * Since there was an error, clean up the function scan stack.
	 */
	if (!IsResManagerMemoryPolicyNone())
	{
		SPI_InitMemoryReservation();
	}
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
		elog(ERROR, "could not find MotionState for slice %d in executor tree", sliceIndex);

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
	return plan_tree_walker((Node*)node, MotionFinderWalker, ctx, true);
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
SubPlanFinderWalker(Node *node, void *context)
{
	SubPlanFinderContext *ctx = (SubPlanFinderContext *) context;

	if (node == NULL)
		return false;

	/* don't recurse into other slices */
	if (IsA(node, Motion))
		return false;

	if (IsA(node, SubPlan))
	{
		SubPlan *subplan = (SubPlan *) node;
		int		plan_id = subplan->plan_id;

		if (!bms_is_member(plan_id, ctx->bms_subplans))
			ctx->bms_subplans = bms_add_member(ctx->bms_subplans, plan_id);
		else
			return false;
	}

	/* Continue walking */
	return plan_tree_walker(node, SubPlanFinderWalker, ctx, true);
}

/*
 * Given a plan and a root motion node find all the subplans
 * between 'root' and the next motion node in the tree
 */
Bitmapset *
getLocallyExecutableSubplans(PlannedStmt *plannedstmt, Plan *root_plan)
{
	SubPlanFinderContext ctx;

	ctx.base.node = (Node *) plannedstmt;
	ctx.bms_subplans = NULL;

	/*
	 * Note that we begin with plan_tree_walker(root_plan), not
	 * SubPlanFinderWalker(root_plan). SubPlanFinderWalker() will stop
	 * at a Motion, but a slice typically has a Motion at the top. We want
	 * to recurse into the children of the top Motion, as well as any
	 * initPlans, targetlist, and other fields on the Motion itself. They
	 * are all considered part of the sending slice.
	 */
	(void) plan_tree_walker((Node *) root_plan, SubPlanFinderWalker, &ctx, true);

	return ctx.bms_subplans;
}

/*
 * Copy PARAM_EXEC parameter values that were received from the QD into
 * our EState.
 */
void
InstallDispatchedExecParams(QueryDispatchDesc *ddesc, EState *estate)
{
	Assert(Gp_role == GP_ROLE_EXECUTE);

	if (ddesc->paramInfo == NULL)
		return;

	for (int i = 0; i < ddesc->paramInfo->nExecParams; i++)
	{
		SerializedParamExecData *sprm = &ddesc->paramInfo->execParams[i];
		ParamExecData *prmExec = &estate->es_param_exec_vals[i];

		if (!sprm->isvalid)
			continue;	/* not dispatched */

		prmExec->execPlan = NULL;
		prmExec->value = sprm->value;
		prmExec->isnull = sprm->isnull;
	}
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
int
PrimaryWriterSliceIndex(EState *estate)
{
	if (!estate->es_sliceTable)
		return 0;

	for (int i = 0; i < estate->es_sliceTable->numSlices; i++)
	{
		ExecSlice  *slice = &estate->es_sliceTable->slices[i];

		if (slice->gangType == GANGTYPE_PRIMARY_WRITER)
			return slice->sliceIndex;
	}

	return 0;
}

/**
 * Provide root slice of locally executing slice.
 */
int
RootSliceIndex(EState *estate)
{
	int			result = 0;

	if (estate->es_sliceTable)
	{
		ExecSlice *localSlice = &estate->es_sliceTable->slices[LocallyExecutingSliceIndex(estate)];

		result = localSlice->rootIndex;

		Assert(result >= 0 && estate->es_sliceTable->numSlices);
	}

	return result;
}


#ifdef USE_ASSERT_CHECKING
/**
 * Assert that slicetable is valid. Must be called after ExecInitMotion, which sets up the slice table
 */
void AssertSliceTableIsValid(SliceTable *st)
{
	if (!st)
		return;

	for (int i = 0; i < st->numSlices; i++)
	{
		ExecSlice *s = &st->slices[i];

		/* The n-th slice entry has sliceIndex of n */
		Assert(s->sliceIndex == i && "slice index incorrect");

		/*
		 * FIXME: Sometimes the planner produces a plan with unused SubPlans, which
		 * might contain Motion nodes. We remove unused SubPlans as part cdbllize(), but
		 * there is a scenario with Append nodes where they still occur.
		 * adjust_appendrel_attrs() makes copies of any SubPlans it encounters, which
		 * happens early in the planning, leaving any SubPlans in target list of the
		 * Append node to point to the original plan_id. The scan in cdbllize() doesn't
		 * eliminate such SubPlans. But set_plan_references() will replace any SubPlans
		 * in the Append's targetlist with references to the outputs of the child nodes,
		 * leaving the original SubPlan unused.
		 *
		 * For now, just tolerate unused slices.
		 */
		if (s->rootIndex == -1 && s->parentIndex == -1 && s->gangType == GANGTYPE_UNALLOCATED)
			continue;

		/* Parent slice index */
		if (s->sliceIndex == s->rootIndex)
		{
			/* Current slice is a root slice. It will have parent index -1.*/
			Assert(s->parentIndex == -1 && "expecting parent index of -1");
		}
		else
		{
			/* All other slices must have a valid parent index */
			Assert(s->parentIndex >= 0 && s->parentIndex < st->numSlices && "slice's parent index out of range");
		}

		/* Current slice's children must consider it the parent */
		ListCell *lc1 = NULL;
		foreach (lc1, s->children)
		{
			int childIndex = lfirst_int(lc1);
			Assert(childIndex >= 0 && childIndex < st->numSlices && "invalid child slice");
			ExecSlice *sc = &st->slices[childIndex];
			Assert(sc->parentIndex == s->sliceIndex && "slice's child does not consider it the parent");
		}

		/* Current slice must be in its parent's children list */
		if (s->parentIndex >= 0)
		{
			ExecSlice *sp = &st->slices[s->parentIndex];

			bool found = false;
			foreach (lc1, sp->children)
			{
				int childIndex = lfirst_int(lc1);
				Assert(childIndex >= 0 && childIndex < st->numSlices && "invalid child slice");
				ExecSlice *sc = &st->slices[childIndex];

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
