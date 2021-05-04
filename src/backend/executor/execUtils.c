/*-------------------------------------------------------------------------
 *
 * execUtils.c
 *	  miscellaneous executor utility routines
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
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
 *		etc
 *
 *		ExecOpenScanRelation	Common code for scan node init routines.
 *
 *		ExecInitRangeTable		Set up executor's range-table-related data.
 *
 *		ExecGetRangeTableRelation		Fetch Relation for a rangetable entry.
 *
 *		executor_errposition	Report syntactic position of an error.
 *
 *		RegisterExprContextCallback    Register function shutdown callback
 *		UnregisterExprContextCallback  Deregister function shutdown callback
 *
 *		GetAttributeByName		Runtime extraction of columns from tuples.
 *		GetAttributeByNum
 *
 *	 NOTES
 *		This file has traditionally been the place to stick misc.
 *		executor support stuff that doesn't really go anyplace else.
 */

#include "postgres.h"
#include "pgstat.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/parallel.h"
#include "access/relscan.h"
#include "access/table.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "catalog/index.h"
#include "executor/execdebug.h"
#include "executor/execUtils.h"
#include "executor/executor.h"
#include "jit/jit.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "parser/parsetree.h"
#include "partitioning/partdesc.h"
#include "storage/lmgr.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/typcache.h"

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

static bool tlist_matches_tupdesc(PlanState *ps, List *tlist, Index varno, TupleDesc tupdesc);
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
									 ALLOCSET_DEFAULT_SIZES);
	MemoryContextDeclareAccountingRoot(qcontext);

	/*
	 * Make the EState node within the per-query context.  This way, we don't
	 * need a separate pfree() operation for it at shutdown.
	 */
	oldcontext = MemoryContextSwitchTo(qcontext);

	estate = makeNode(EState);

	/*
	 * Initialize all fields of the Executor State structure
	 */
	estate->es_direction = ForwardScanDirection;
	estate->es_snapshot = InvalidSnapshot;	/* caller must initialize this */
	estate->es_crosscheck_snapshot = InvalidSnapshot;	/* no crosscheck */
	estate->es_range_table = NIL;
	estate->es_range_table_array = NULL;
	estate->es_range_table_size = 0;
	estate->es_relations = NULL;
	estate->es_rowmarks = NULL;
	estate->es_plannedstmt = NULL;

	estate->es_junkFilter = NULL;

	estate->es_output_cid = (CommandId) 0;

	estate->es_result_relations = NULL;
	estate->es_num_result_relations = 0;
	estate->es_result_relation_info = NULL;

	estate->es_root_result_relations = NULL;
	estate->es_num_root_result_relations = 0;

	estate->es_tuple_routing_result_relations = NIL;

	estate->es_trig_target_relations = NIL;

	estate->es_param_list_info = NULL;
	estate->es_param_exec_vals = NULL;

	estate->es_queryEnv = NULL;

	estate->es_query_cxt = qcontext;

	estate->es_tupleTable = NIL;

	estate->es_processed = 0;

	estate->es_top_eflags = 0;
	estate->es_instrument = 0;
	estate->es_finished = false;

	estate->es_exprcontexts = NIL;

	estate->es_subplanstates = NIL;

	estate->es_auxmodifytables = NIL;

	estate->es_per_tuple_exprcontext = NULL;

	estate->es_sourceText = NULL;

	estate->es_use_parallel_mode = false;

	estate->es_jit_flags = 0;
	estate->es_jit = NULL;

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
 * Note: this is not responsible for releasing non-memory resources, such as
 * open relations or buffer pins.  But it will shut down any still-active
 * ExprContexts within the EState and deallocate associated JITed expressions.
 * That is sufficient cleanup for situations where the EState has only been
 * used for expression evaluation, and not to run a complete Plan.
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

	/* release JIT context, if allocated */
	if (estate->es_jit)
	{
		jit_release_context(estate->es_jit);
		estate->es_jit = NULL;
	}

	/* release partition directory, if allocated */
	if (estate->es_partition_directory)
	{
		DestroyPartitionDirectory(estate->es_partition_directory);
		estate->es_partition_directory = NULL;
	}

	/*
	 * Free the per-query memory context, thereby releasing all working
	 * memory, including the EState node itself.
	 */
	MemoryContextDelete(estate->es_query_cxt);
}

/*
 * Internal implementation for CreateExprContext() and CreateWorkExprContext()
 * that allows control over the AllocSet parameters.
 */
static ExprContext *
CreateExprContextInternal(EState *estate, Size minContextSize,
						  Size initBlockSize, Size maxBlockSize)
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
							  minContextSize,
							  initBlockSize,
							  maxBlockSize);

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
	return CreateExprContextInternal(estate, ALLOCSET_DEFAULT_SIZES);
}


/* ----------------
 *		CreateWorkExprContext
 *
 * Like CreateExprContext, but specifies the AllocSet sizes to be reasonable
 * in proportion to work_mem. If the maximum block allocation size is too
 * large, it's easy to skip right past work_mem with a single allocation.
 * ----------------
 */
ExprContext *
CreateWorkExprContext(EState *estate)
{
	Size minContextSize = ALLOCSET_DEFAULT_MINSIZE;
	Size initBlockSize = ALLOCSET_DEFAULT_INITSIZE;
	Size maxBlockSize = ALLOCSET_DEFAULT_MAXSIZE;

	/* choose the maxBlockSize to be no larger than 1/16 of work_mem */
	while (16 * maxBlockSize > work_mem * 1024L)
		maxBlockSize >>= 1;

	if (maxBlockSize < ALLOCSET_DEFAULT_INITSIZE)
		maxBlockSize = ALLOCSET_DEFAULT_INITSIZE;

	return CreateExprContextInternal(estate, minContextSize,
									 initBlockSize, maxBlockSize);
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
							  ALLOCSET_DEFAULT_SIZES);

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
 *		ExecGetResultType
 * ----------------
 */
TupleDesc
ExecGetResultType(PlanState *planstate)
{
	return planstate->ps_ResultTupleDesc;
}

/*
 * ExecGetResultSlotOps - information about node's type of result slot
 */
const TupleTableSlotOps *
ExecGetResultSlotOps(PlanState *planstate, bool *isfixed)
{
	if (planstate->resultopsset && planstate->resultops)
	{
		if (isfixed)
			*isfixed = planstate->resultopsfixed;
		return planstate->resultops;
	}

	if (isfixed)
	{
		if (planstate->resultopsset)
			*isfixed = planstate->resultopsfixed;
		else if (planstate->ps_ResultTupleSlot)
			*isfixed = TTS_FIXED(planstate->ps_ResultTupleSlot);
		else
			*isfixed = false;
	}

	if (!planstate->ps_ResultTupleSlot)
		return &TTSOpsVirtual;

	return planstate->ps_ResultTupleSlot->tts_ops;
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
	planstate->ps_ProjInfo =
		ExecBuildProjectionInfo(planstate->plan->targetlist,
								planstate->ps_ExprContext,
								planstate->ps_ResultTupleSlot,
								planstate,
								inputDesc);
}


/* ----------------
 *		ExecConditionalAssignProjectionInfo
 *
 * as ExecAssignProjectionInfo, but store NULL rather than building projection
 * info if no projection is required
 * ----------------
 */
void
ExecConditionalAssignProjectionInfo(PlanState *planstate, TupleDesc inputDesc,
									Index varno)
{
	if (tlist_matches_tupdesc(planstate,
							  planstate->plan->targetlist,
							  varno,
							  inputDesc))
	{
		planstate->ps_ProjInfo = NULL;
		planstate->resultopsset = planstate->scanopsset;
		planstate->resultopsfixed = planstate->scanopsfixed;
		planstate->resultops = planstate->scanops;
	}
	else
	{
		if (!planstate->ps_ResultTupleSlot)
		{
			ExecInitResultSlot(planstate, &TTSOpsVirtual);
			planstate->resultops = &TTSOpsVirtual;
			planstate->resultopsfixed = true;
			planstate->resultopsset = true;
		}
		ExecAssignProjectionInfo(planstate, inputDesc);
	}
}

static bool
tlist_matches_tupdesc(PlanState *ps, List *tlist, Index varno, TupleDesc tupdesc)
{
	int			numattrs = tupdesc->natts;
	int			attrno;
	ListCell   *tlist_item = list_head(tlist);

	/* Check the tlist attributes */
	for (attrno = 1; attrno <= numattrs; attrno++)
	{
		Form_pg_attribute att_tup = TupleDescAttr(tupdesc, attrno - 1);
		Var		   *var;

		if (tlist_item == NULL)
			return false;		/* tlist too short */
		var = (Var *) ((TargetEntry *) lfirst(tlist_item))->expr;
		if (!var || !IsA(var, Var))
			return false;		/* tlist item not a Var */
		/* if these Asserts fail, planner messed up */
		Assert(var->varno == varno);
		Assert(var->varlevelsup == 0);
		if (var->varattno != attrno)
			return false;		/* out of order */
		if (att_tup->attisdropped)
			return false;		/* table contains dropped columns */
		if (att_tup->atthasmissing)
			return false;		/* table contains cols with missing values */

		/*
		 * Note: usually the Var's type should match the tupdesc exactly, but
		 * in situations involving unions of columns that have different
		 * typmods, the Var may have come from above the union and hence have
		 * typmod -1.  This is a legitimate situation since the Var still
		 * describes the column, just not as exactly as the tupdesc does. We
		 * could change the planner to prevent it, but it'd then insert
		 * projection steps just to convert from specific typmod to typmod -1,
		 * which is pretty silly.
		 */
		if (var->vartype != att_tup->atttypid ||
			(var->vartypmod != att_tup->atttypmod &&
			 var->vartypmod != -1))
			return false;		/* type mismatch */

		tlist_item = lnext(tlist_item);
	}

	if (tlist_item)
		return false;			/* tlist too long */

	return true;
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
 *				  Scan node support
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
 *		ExecCreateScanSlotFromOuterPlan
 * ----------------
 */
void
ExecCreateScanSlotFromOuterPlan(EState *estate,
								ScanState *scanstate,
								const TupleTableSlotOps *tts_ops)
{
	PlanState  *outerPlan;
	TupleDesc	tupDesc;

	outerPlan = outerPlanState(scanstate);
	tupDesc = ExecGetResultType(outerPlan);

	ExecInitScanTupleSlot(estate, scanstate, tupDesc, tts_ops);
}

/* ----------------------------------------------------------------
 *		ExecRelationIsTargetRelation
 *
 *		Detect whether a relation (identified by rangetable index)
 *		is one of the target relations of the query.
 *
 * Note: This is currently no longer used in core.  We keep it around
 * because FDWs may wish to use it to determine if their foreign table
 * is a target relation.
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
 * ----------------------------------------------------------------
 */
Relation
ExecOpenScanRelation(EState *estate, Index scanrelid, int eflags)
{
	Relation	rel;

	/* Open the relation. */
	rel = ExecGetRangeTableRelation(estate, scanrelid);

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
 * ExecInitRangeTable
 *		Set up executor's range-table-related data
 *
 * We build an array from the range table list to allow faster lookup by RTI.
 * (The es_range_table field is now somewhat redundant, but we keep it to
 * avoid breaking external code unnecessarily.)
 * This is also a convenient place to set up the parallel es_relations array.
 */
void
ExecInitRangeTable(EState *estate, List *rangeTable)
{
	Index		rti;
	ListCell   *lc;

	/* Remember the range table List as-is */
	estate->es_range_table = rangeTable;

	/* Set up the equivalent array representation */
	estate->es_range_table_size = list_length(rangeTable);
	estate->es_range_table_array = (RangeTblEntry **)
		palloc(estate->es_range_table_size * sizeof(RangeTblEntry *));
	rti = 0;
	foreach(lc, rangeTable)
	{
		estate->es_range_table_array[rti++] = lfirst_node(RangeTblEntry, lc);
	}

	/*
	 * Allocate an array to store an open Relation corresponding to each
	 * rangetable entry, and initialize entries to NULL.  Relations are opened
	 * and stored here as needed.
	 */
	estate->es_relations = (Relation *)
		palloc0(estate->es_range_table_size * sizeof(Relation));

	/*
	 * es_rowmarks is also parallel to the es_range_table_array, but it's
	 * allocated only if needed.
	 */
	estate->es_rowmarks = NULL;
}

/*
 * ExecGetRangeTableRelation
 *		Open the Relation for a range table entry, if not already done
 *
 * The Relations will be closed again in ExecEndPlan().
 */
Relation
ExecGetRangeTableRelation(EState *estate, Index rti)
{
	Relation	rel;

	Assert(rti > 0 && rti <= estate->es_range_table_size);


	rel = estate->es_relations[rti - 1];
	if (rel == NULL)
	{
		/* First time through, so open the relation */
		RangeTblEntry *rte = exec_rt_fetch(rti, estate);

		Assert(rte->rtekind == RTE_RELATION);

		/* GPDB: a QE process is not holding the locks yet, same as a parallel worker. */
		if (!IsParallelWorker() && Gp_role != GP_ROLE_EXECUTE)
		{
			/*
			 * In a normal query, we should already have the appropriate lock,
			 * but verify that through an Assert.  Since there's already an
			 * Assert inside table_open that insists on holding some lock, it
			 * seems sufficient to check this only when rellockmode is higher
			 * than the minimum.
			 */
			/* GPDB_12_MERGE_FIXME: if GDD is not enabled, we acquire a stronger
			 * lock earlier. What would be a good way to formulate this check?
			 * For now, just pass orstronger=true.
			 */
			rel = table_open(rte->relid, NoLock);
			Assert(rte->rellockmode == AccessShareLock ||
				   CheckRelationLockedByMe(rel, rte->rellockmode,
										   true /* orstronger */));
		}
		else
		{
			/*
			 * If we are a parallel worker, we need to obtain our own local
			 * lock on the relation.  This ensures sane behavior in case the
			 * parent process exits before we do.
			 */
			rel = table_open(rte->relid, rte->rellockmode);
		}

		estate->es_relations[rti - 1] = rel;
	}

	return rel;
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
 * executor_errposition
 *		Report an execution-time cursor position, if possible.
 *
 * This is expected to be used within an ereport() call.  The return value
 * is a dummy (always 0, in fact).
 *
 * The locations stored in parsetrees are byte offsets into the source string.
 * We have to convert them to 1-based character indexes for reporting to
 * clients.  (We do things this way to avoid unnecessary overhead in the
 * normal non-error case: computing character indexes would be much more
 * expensive than storing token offsets.)
 */
void
executor_errposition(EState *estate, int location)
{
	int			pos;

	/* No-op if location was not provided */
	if (location < 0)
		return;
	/* Can't do anything if source text is not available */
	if (estate == NULL || estate->es_sourceText == NULL)
		return;
	/* Convert offset to character number */
	pos = pg_mbstrlen_with_len(estate->es_sourceText, location) + 1;
	/* And pass it to the ereport mechanism */
	errposition(pos);
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
			ecxt_callback->function(ecxt_callback->arg);
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

	if (IsA(node, BoolExpr))
	{
		BoolExpr *be = (BoolExpr *) node;
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
 * Prefetch JoinQual or NonJoinQual to prevent motion hazard.
 *
 * A motion hazard is a deadlock between motions, a classic motion hazard in a
 * join executor is formed by its inner and outer motions, it can be prevented
 * by prefetching the inner plan, refer to motion_sanity_check() for details.
 *
 * A similar motion hazard can be formed by the outer motion and the join qual
 * motion(or non join qual motion).  A join executor fetches a outer tuple,
 * filters it with the qual, then repeat the process on all the outer tuples.
 * When there are motions in both outer plan and the join qual then below state
 * is possible:
 *
 * 0. processes A and B belong to the join slice, process C belongs to the
 *    outer slice, process D belongs to the JoinQual(NonJoinQual) slice;
 * 1. A has read the first outer tuple and is fetching tuples from D;
 * 2. D is waiting for ACK from B;
 * 3. B is fetching the first outer tuple from C;
 * 4. C is waiting for ACK from A;
 *
 * So a deadlock is formed A->D->B->C->A.  We can prevent it also by
 * prefetching the join qual or non join qual
 *
 * An example is demonstrated and explained in test case
 * src/test/regress/sql/deadlock2.sql.
 *
 * Return true if the JoinQual or NonJoinQual is prefetched.
 */
void
ExecPrefetchQual(JoinState *node, bool isJoinQual)
{
	EState	   *estate = node->ps.state;
	ExprContext *econtext = node->ps.ps_ExprContext;
	PlanState  *inner = innerPlanState(node);
	PlanState  *outer = outerPlanState(node);
	TupleTableSlot *innertuple = econtext->ecxt_innertuple;

	ListCell   *lc = NULL;
	List       *quals = NIL;

	ExprState  *qual;

	if (isJoinQual)
		qual = node->joinqual;
	else
		qual = node->ps.qual;

	Assert(qual);

	/* Outer tuples should not be fetched before us */
	Assert(econtext->ecxt_outertuple == NULL);

	/* Build fake inner & outer tuples */
	econtext->ecxt_innertuple = ExecInitNullTupleSlot(estate,
													  ExecGetResultType(inner),
													  &TTSOpsVirtual);
	econtext->ecxt_outertuple = ExecInitNullTupleSlot(estate,
													  ExecGetResultType(outer),
													  &TTSOpsVirtual);

	if (IsA(node->ps.plan, NestLoop))
	{
		NestLoop *nl = (NestLoop *) (node->ps.plan);
		if (nl->nestParams)
			fake_outer_params(node);
	}

	quals = flatten_logic_exprs((Node *) qual);

	/* Fetch subplan with the fake inner & outer tuples */
	foreach(lc, quals)
	{
		/*
		 * Force every qual is prefech because
		 * our target is to materialize motion node.
		 */
		ExprState  *clause = (ExprState *) lfirst(lc);
		(void) ExecQual(clause, econtext);
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
					slice->segments = lappend_int(slice->segments, i % getgpsegmentCount());
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
static bool AssignWriterGangFirst(CdbDispatcherState *ds, SliceTable *sliceTable, int sliceIndex);
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

	AssignWriterGangFirst(ds, sliceTable, rootIdx);
	InventorySliceTree(ds, sliceTable, rootIdx);
}

/*
 * AssignWriterGangFirst() - Try to assign writer gang first.
 *
 * For the gang allocation, our current implementation required the first
 * allocated gang must be the writer gang.
 * This has several reasons:
 * - For lock holding, Because of our MPP structure, we assign a LockHolder
 *   for each segment when executing a query. lockHolder is the gang member that
 *   should hold and manage locks for this transaction. On the QEs, it should
 *   normally be the Writer gang member. More details please refer to
 *   lockHolderProcPtr in lock.c.
 * - For SharedSnapshot among session's gang processes on a particular segment.
 *   During initPostgres(), reader QE will try to lookup the shared slot written
 *   by writer QE. More details please reger to sharedsnapshot.c.
 *
 * Normally, the writer slice will be assign writer gang first when iterate the
 * slice table. But this is not true for writable CTE (with only one writer gang).
 * For below statement:
 *
 * WITH updated AS (update tbl set rank = 6 where id = 5 returning rank)
 * select * from tbl where rank in (select rank from updated);
 *                                           QUERY PLAN
 * ----------------------------------------------------------------------------------------------
 *  Gather Motion 3:1  (slice1; segments: 3)
 *    ->  Seq Scan on tbl
 *          Filter: (hashed SubPlan 1)
 *          SubPlan 1
 *            ->  Broadcast Motion 1:3  (slice2; segments: 1)
 *                  ->  Update on tbl
 *                        ->  Seq Scan on tbl
 *                              Filter: (id = 5)
 *  Slice 0: Dispatcher; root 0; parent -1; gang size 0
 *  Slice 1: Reader; root 0; parent 0; gang size 3
 *  Slice 2: Primary Writer; root 0; parent 1; gang size 1
 *
 * If we sill assign writer gang to Slice 1 here, the writer process will execute
 * on reader gang. So, find the writer slice and assign writer gang first.
 */
static bool
AssignWriterGangFirst(CdbDispatcherState *ds, SliceTable *sliceTable, int sliceIndex)
{
	ExecSlice	   *slice = &sliceTable->slices[sliceIndex];

	if (slice->gangType == GANGTYPE_PRIMARY_WRITER)
	{
		Assert(slice->primaryGang == NULL);
		Assert(slice->segments != NIL);
		slice->primaryGang = AllocateGang(ds, slice->gangType, slice->segments);
		setupCdbProcessList(slice);
		return true;
	}
	else
	{
		ListCell *cell;
		foreach(cell, slice->children)
		{
			int			childIndex = lfirst_int(cell);
			if (AssignWriterGangFirst(ds, sliceTable, childIndex))
				return true;
		}
	}
	return false;
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
	else if (!slice->primaryGang)
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
	int			primaryWriterSliceIndex;

	/* caller must have switched into per-query memory context already */
	estate = queryDesc->estate;

	currentSlice = getCurrentSlice(estate, LocallyExecutingSliceIndex(estate));
	primaryWriterSliceIndex = PrimaryWriterSliceIndex(estate);

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

		/* collect pgstat from QEs for current transaction level */
		pgstat_combine_from_qe(pr, primaryWriterSliceIndex);

		/* get num of rows processed from writer QEs. */
		estate->es_processed +=
			cdbdisp_sumCmdTuples(pr, primaryWriterSliceIndex);

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

/*
 *		GetAttributeByName
 *		GetAttributeByNum
 *
 *		These functions return the value of the requested attribute
 *		out of the given tuple Datum.
 *		C functions which take a tuple as an argument are expected
 *		to use these.  Ex: overpaid(EMP) might call GetAttributeByNum().
 *		Note: these are actually rather slow because they do a typcache
 *		lookup on each call.
 */
Datum
GetAttributeByName(HeapTupleHeader tuple, const char *attname, bool *isNull)
{
	AttrNumber	attrno;
	Datum		result;
	Oid			tupType;
	int32		tupTypmod;
	TupleDesc	tupDesc;
	HeapTupleData tmptup;
	int			i;

	if (attname == NULL)
		elog(ERROR, "invalid attribute name");

	if (isNull == NULL)
		elog(ERROR, "a NULL isNull pointer was passed");

	if (tuple == NULL)
	{
		/* Kinda bogus but compatible with old behavior... */
		*isNull = true;
		return (Datum) 0;
	}

	tupType = HeapTupleHeaderGetTypeId(tuple);
	tupTypmod = HeapTupleHeaderGetTypMod(tuple);
	tupDesc = lookup_rowtype_tupdesc(tupType, tupTypmod);

	attrno = InvalidAttrNumber;
	for (i = 0; i < tupDesc->natts; i++)
	{
		Form_pg_attribute att = TupleDescAttr(tupDesc, i);

		if (namestrcmp(&(att->attname), attname) == 0)
		{
			attrno = att->attnum;
			break;
		}
	}

	if (attrno == InvalidAttrNumber)
		elog(ERROR, "attribute \"%s\" does not exist", attname);

	/*
	 * heap_getattr needs a HeapTuple not a bare HeapTupleHeader.  We set all
	 * the fields in the struct just in case user tries to inspect system
	 * columns.
	 */
	tmptup.t_len = HeapTupleHeaderGetDatumLength(tuple);
	ItemPointerSetInvalid(&(tmptup.t_self));
	tmptup.t_tableOid = InvalidOid;
	tmptup.t_data = tuple;

	result = heap_getattr(&tmptup,
						  attrno,
						  tupDesc,
						  isNull);

	ReleaseTupleDesc(tupDesc);

	return result;
}

Datum
GetAttributeByNum(HeapTupleHeader tuple,
				  AttrNumber attrno,
				  bool *isNull)
{
	Datum		result;
	Oid			tupType;
	int32		tupTypmod;
	TupleDesc	tupDesc;
	HeapTupleData tmptup;

	if (!AttributeNumberIsValid(attrno))
		elog(ERROR, "invalid attribute number %d", attrno);

	if (isNull == NULL)
		elog(ERROR, "a NULL isNull pointer was passed");

	if (tuple == NULL)
	{
		/* Kinda bogus but compatible with old behavior... */
		*isNull = true;
		return (Datum) 0;
	}

	tupType = HeapTupleHeaderGetTypeId(tuple);
	tupTypmod = HeapTupleHeaderGetTypMod(tuple);
	tupDesc = lookup_rowtype_tupdesc(tupType, tupTypmod);

	/*
	 * heap_getattr needs a HeapTuple not a bare HeapTupleHeader.  We set all
	 * the fields in the struct just in case user tries to inspect system
	 * columns.
	 */
	tmptup.t_len = HeapTupleHeaderGetDatumLength(tuple);
	ItemPointerSetInvalid(&(tmptup.t_self));
	tmptup.t_tableOid = InvalidOid;
	tmptup.t_data = tuple;

	result = heap_getattr(&tmptup,
						  attrno,
						  tupDesc,
						  isNull);

	ReleaseTupleDesc(tupDesc);

	return result;
}

/*
 * Number of items in a tlist (including any resjunk items!)
 */
int
ExecTargetListLength(List *targetlist)
{
	/* This used to be more complex, but fjoins are dead */
	return list_length(targetlist);
}

/*
 * Number of items in a tlist, not including any resjunk items
 */
int
ExecCleanTargetListLength(List *targetlist)
{
	int			len = 0;
	ListCell   *tl;

	foreach(tl, targetlist)
	{
		TargetEntry *curTle = lfirst_node(TargetEntry, tl);

		if (!curTle->resjunk)
			len++;
	}
	return len;
}

/*
 * Return a relInfo's tuple slot for a trigger's OLD tuples.
 */
TupleTableSlot *
ExecGetTriggerOldSlot(EState *estate, ResultRelInfo *relInfo)
{
	if (relInfo->ri_TrigOldSlot == NULL)
	{
		Relation	rel = relInfo->ri_RelationDesc;
		MemoryContext oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

		relInfo->ri_TrigOldSlot =
			ExecInitExtraTupleSlot(estate,
								   RelationGetDescr(rel),
								   table_slot_callbacks(rel));

		MemoryContextSwitchTo(oldcontext);
	}

	return relInfo->ri_TrigOldSlot;
}

/*
 * Return a relInfo's tuple slot for a trigger's NEW tuples.
 */
TupleTableSlot *
ExecGetTriggerNewSlot(EState *estate, ResultRelInfo *relInfo)
{
	if (relInfo->ri_TrigNewSlot == NULL)
	{
		Relation	rel = relInfo->ri_RelationDesc;
		MemoryContext oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

		relInfo->ri_TrigNewSlot =
			ExecInitExtraTupleSlot(estate,
								   RelationGetDescr(rel),
								   table_slot_callbacks(rel));

		MemoryContextSwitchTo(oldcontext);
	}

	return relInfo->ri_TrigNewSlot;
}

/*
 * Return a relInfo's tuple slot for processing returning tuples.
 */
TupleTableSlot *
ExecGetReturningSlot(EState *estate, ResultRelInfo *relInfo)
{
	if (relInfo->ri_ReturningSlot == NULL)
	{
		Relation	rel = relInfo->ri_RelationDesc;
		MemoryContext oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

		relInfo->ri_ReturningSlot =
			ExecInitExtraTupleSlot(estate,
								   RelationGetDescr(rel),
								   table_slot_callbacks(rel));

		MemoryContextSwitchTo(oldcontext);
	}

	return relInfo->ri_ReturningSlot;
}
