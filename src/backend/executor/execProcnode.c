/*-------------------------------------------------------------------------
 *
 * execProcnode.c
 *	 contains dispatch functions which call the appropriate "initialize",
 *	 "get a tuple", and "cleanup" routines for the given node type.
 *	 If the node has children, then it will presumably call ExecInitNode,
 *	 ExecProcNode, or ExecEndNode on its subnodes and do the appropriate
 *	 processing.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/execProcnode.c
 *
 *-------------------------------------------------------------------------
 */
/*
 *	 NOTES
 *		This used to be three files.  It is now all combined into
 *		one file so that it is easier to keep the dispatch routines
 *		in sync when new nodes are added.
 *
 *	 EXAMPLE
 *		Suppose we want the age of the manager of the shoe department and
 *		the number of employees in that department.  So we have the query:
 *
 *				select DEPT.no_emps, EMP.age
 *				from DEPT, EMP
 *				where EMP.name = DEPT.mgr and
 *					  DEPT.name = "shoe"
 *
 *		Suppose the planner gives us the following plan:
 *
 *						Nest Loop (DEPT.mgr = EMP.name)
 *						/		\
 *					   /		 \
 *				   Seq Scan		Seq Scan
 *					DEPT		  EMP
 *				(name = "shoe")
 *
 *		ExecutorStart() is called first.
 *		It calls InitPlan() which calls ExecInitNode() on
 *		the root of the plan -- the nest loop node.
 *
 *	  * ExecInitNode() notices that it is looking at a nest loop and
 *		as the code below demonstrates, it calls ExecInitNestLoop().
 *		Eventually this calls ExecInitNode() on the right and left subplans
 *		and so forth until the entire plan is initialized.  The result
 *		of ExecInitNode() is a plan state tree built with the same structure
 *		as the underlying plan tree.
 *
 *	  * Then when ExecutorRun() is called, it calls ExecutePlan() which calls
 *		ExecProcNode() repeatedly on the top node of the plan state tree.
 *		Each time this happens, ExecProcNode() will end up calling
 *		ExecNestLoop(), which calls ExecProcNode() on its subplans.
 *		Each of these subplans is a sequential scan so ExecSeqScan() is
 *		called.  The slots returned by ExecSeqScan() may contain
 *		tuples which contain the attributes ExecNestLoop() uses to
 *		form the tuples it returns.
 *
 *	  * Eventually ExecSeqScan() stops returning tuples and the nest
 *		loop join ends.  Lastly, ExecutorEnd() calls ExecEndNode() which
 *		calls ExecEndNestLoop() which in turn calls ExecEndNode() on
 *		its subplans which result in ExecEndSeqScan().
 *
 *		This should show how the executor works by having
 *		ExecInitNode(), ExecProcNode() and ExecEndNode() dispatch
 *		their work to the appropriate node support routines which may
 *		in turn call these routines themselves on their subplans.
 */
#include "postgres.h"

#include "executor/executor.h"
#include "executor/nodeAgg.h"
#include "executor/nodeAppend.h"
#include "executor/nodeBitmapAnd.h"
#include "executor/nodeBitmapHeapscan.h"
#include "executor/nodeBitmapIndexscan.h"
#include "executor/nodeDynamicBitmapHeapscan.h"
#include "executor/nodeDynamicBitmapIndexscan.h"
#include "executor/nodeBitmapOr.h"
#include "executor/nodeCtescan.h"
#include "executor/nodeCustom.h"
#include "executor/nodeForeignscan.h"
#include "executor/nodeFunctionscan.h"
#include "executor/nodeGather.h"
#include "executor/nodeGatherMerge.h"
#include "executor/nodeHash.h"
#include "executor/nodeHashjoin.h"
#include "executor/nodeIndexonlyscan.h"
#include "executor/nodeIndexscan.h"
#include "executor/nodeLimit.h"
#include "executor/nodeLockRows.h"
#include "executor/nodeMaterial.h"
#include "executor/nodeMergeAppend.h"
#include "executor/nodeMergejoin.h"
#include "executor/nodeModifyTable.h"
#include "executor/nodeNamedtuplestorescan.h"
#include "executor/nodeNestloop.h"
#include "executor/nodeProjectSet.h"
#include "executor/nodeRecursiveunion.h"
#include "executor/nodeResult.h"
#include "executor/nodeSamplescan.h"
#include "executor/nodeSeqscan.h"
#include "executor/nodeSetOp.h"
#include "executor/nodeSort.h"
#include "executor/nodeSubplan.h"
#include "executor/nodeSubqueryscan.h"
#include "executor/nodeTableFuncscan.h"
#include "executor/nodeTidscan.h"
#include "executor/nodeTupleSplit.h"
#include "executor/nodeUnique.h"
#include "executor/nodeValuesscan.h"
#include "executor/nodeWindowAgg.h"
#include "executor/nodeWorktablescan.h"
#include "nodes/nodeFuncs.h"
#include "miscadmin.h"

#include "cdb/cdbvars.h"
#include "cdb/ml_ipc.h"			/* interconnect context */
#include "executor/nodeAssertOp.h"
#include "executor/nodeDynamicIndexscan.h"
#include "executor/nodeDynamicSeqscan.h"
#include "executor/nodeDynamicForeignscan.h"
#include "executor/nodeMotion.h"
#include "executor/nodePartitionSelector.h"
#include "executor/nodeSequence.h"
#include "executor/nodeShareInputScan.h"
#include "executor/nodeSplitUpdate.h"
#include "executor/nodeTableFunction.h"
#include "pg_trace.h"
#include "tcop/tcopprot.h"
#include "utils/memutils.h"
#include "utils/metrics_utils.h"

 /* flags bits for planstate walker */
#define PSW_IGNORE_INITPLAN    0x01

 /**
  * Forward declarations of static functions
  */
static CdbVisitOpt planstate_walk_node_extended(PlanState *planstate,
				 CdbVisitOpt (*walker) (PlanState *planstate, void *context),
							 void *context,
							 int flags);

static CdbVisitOpt planstate_walk_array(PlanState **planstates,
					 int nplanstate,
				 CdbVisitOpt (*walker) (PlanState *planstate, void *context),
					 void *context,
					 int flags);

static CdbVisitOpt planstate_walk_kids(PlanState *planstate,
				 CdbVisitOpt (*walker) (PlanState *planstate, void *context),
					void *context,
					int flags);

static TupleTableSlot *ExecProcNodeFirst(PlanState *node);
#if 0
static TupleTableSlot *ExecProcNodeInstr(PlanState *node);
#endif
static TupleTableSlot *ExecProcNodeGPDB(PlanState *node);


/* ------------------------------------------------------------------------
 *		ExecInitNode
 *
 *		Recursively initializes all the nodes in the plan tree rooted
 *		at 'node'.
 *
 *		Inputs:
 *		  'node' is the current node of the plan produced by the query planner
 *		  'estate' is the shared execution state for the plan tree
 *		  'eflags' is a bitwise OR of flag bits described in executor.h
 *
 *		Returns a PlanState node corresponding to the given Plan node.
 * ------------------------------------------------------------------------
 */
PlanState *
ExecInitNode(Plan *node, EState *estate, int eflags)
{
	PlanState  *result;
	List	   *subps;
	ListCell   *l;
	MemoryContext nodecxt = NULL;
	MemoryContext oldcxt = NULL;

	/*
	 * do nothing when we get to the end of a leaf on tree.
	 */
	if (node == NULL)
		return NULL;

	/*
	 * Make sure there's enough stack available. Need to check here, in
	 * addition to ExecProcNode() (via ExecProcNodeFirst()), to ensure the
	 * stack isn't overrun while initializing the node tree.
	 */
	check_stack_depth();

	/*
	 * If per-node memory usage was requested
	 * (explain_memory_verbosity=detail), create a separate memory context
	 * for every node, so that we can attribute memory usage to each node.
	 * Otherwise, everything is allocated in the per-query ExecutorState
	 * context. The extra memory contexts consume some memory on their
	 * own, and prevent reusing memory allocated in one node in another
	 * node, so we only want to do this if the level of detail is needed.
	 */
	if ((estate->es_instrument & INSTRUMENT_MEMORY_DETAIL) != 0)
	{
		nodecxt = AllocSetContextCreate(CurrentMemoryContext,
										"executor node",
										ALLOCSET_SMALL_SIZES);
		MemoryContextDeclareAccountingRoot(nodecxt);
		oldcxt = MemoryContextSwitchTo(nodecxt);
	}

	switch (nodeTag(node))
	{
			/*
			 * control nodes
			 */
		case T_Result:
			result = (PlanState *) ExecInitResult((Result *) node,
												  estate, eflags);
			break;

		case T_ProjectSet:
			result = (PlanState *) ExecInitProjectSet((ProjectSet *) node,
													  estate, eflags);
			break;

		case T_ModifyTable:
			result = (PlanState *) ExecInitModifyTable((ModifyTable *) node,
													   estate, eflags);
			break;

		case T_Append:
			result = (PlanState *) ExecInitAppend((Append *) node,
												  estate, eflags);
			break;

		case T_Sequence:
			result = (PlanState *) ExecInitSequence((Sequence *) node,
													estate, eflags);
			break;

		case T_MergeAppend:
			result = (PlanState *) ExecInitMergeAppend((MergeAppend *) node,
													   estate, eflags);
			break;

		case T_RecursiveUnion:
			result = (PlanState *) ExecInitRecursiveUnion((RecursiveUnion *) node,
														  estate, eflags);
			break;

		case T_BitmapAnd:
			result = (PlanState *) ExecInitBitmapAnd((BitmapAnd *) node,
													 estate, eflags);
			break;

		case T_BitmapOr:
			result = (PlanState *) ExecInitBitmapOr((BitmapOr *) node,
													estate, eflags);
			break;

			/*
			 * scan nodes
			 */
		case T_SeqScan:
			result = (PlanState *) ExecInitSeqScan((SeqScan *) node,
													 estate, eflags);
			break;

		case T_DynamicSeqScan:
			result = (PlanState *) ExecInitDynamicSeqScan((DynamicSeqScan *) node,
												   estate, eflags);
			break;

		case T_SampleScan:
			result = (PlanState *) ExecInitSampleScan((SampleScan *) node,
													  estate, eflags);
			break;

		case T_IndexScan:
			result = (PlanState *) ExecInitIndexScan((IndexScan *) node,
													 estate, eflags);
			break;

		case T_DynamicIndexScan:
			result = (PlanState *) ExecInitDynamicIndexScan((DynamicIndexScan *) node,
													estate, eflags);
			break;

		case T_IndexOnlyScan:
			result = (PlanState *) ExecInitIndexOnlyScan((IndexOnlyScan *) node,
														 estate, eflags);
			break;

		case T_BitmapIndexScan:
			result = (PlanState *) ExecInitBitmapIndexScan((BitmapIndexScan *) node,
														   estate, eflags);
			break;

		case T_DynamicBitmapIndexScan:
			result = (PlanState *) ExecInitDynamicBitmapIndexScan((DynamicBitmapIndexScan *) node,
																  estate, eflags);
			break;

		case T_BitmapHeapScan:
			result = (PlanState *) ExecInitBitmapHeapScan((BitmapHeapScan *) node,
														  estate, eflags);
			break;

		case T_DynamicBitmapHeapScan:
			result = (PlanState *) ExecInitDynamicBitmapHeapScan((DynamicBitmapHeapScan *) node,
																 estate, eflags);
			break;

		case T_TidScan:
			result = (PlanState *) ExecInitTidScan((TidScan *) node,
												   estate, eflags);
			break;

		case T_SubqueryScan:
			result = (PlanState *) ExecInitSubqueryScan((SubqueryScan *) node,
														estate, eflags);
			break;

		case T_FunctionScan:
			result = (PlanState *) ExecInitFunctionScan((FunctionScan *) node,
														estate, eflags);
			break;

		case T_TableFunctionScan:
			result = (PlanState *) ExecInitTableFunction((TableFunctionScan *) node,
														 estate, eflags);
			break;

		case T_TableFuncScan:
			result = (PlanState *) ExecInitTableFuncScan((TableFuncScan *) node,
														 estate, eflags);
			break;

		case T_ValuesScan:
			result = (PlanState *) ExecInitValuesScan((ValuesScan *) node,
													  estate, eflags);
			break;

		case T_CteScan:
			result = (PlanState *) ExecInitCteScan((CteScan *) node,
												   estate, eflags);
			break;

		case T_NamedTuplestoreScan:
			result = (PlanState *) ExecInitNamedTuplestoreScan((NamedTuplestoreScan *) node,
															   estate, eflags);
			break;

		case T_WorkTableScan:
			result = (PlanState *) ExecInitWorkTableScan((WorkTableScan *) node,
														 estate, eflags);
			break;

		case T_ForeignScan:
			result = (PlanState *) ExecInitForeignScan((ForeignScan *) node,
														estate, eflags);
			break;

		case T_DynamicForeignScan:
			result = (PlanState *) ExecInitDynamicForeignScan((DynamicForeignScan *) node,
												   estate, eflags);
			break;

		case T_CustomScan:
			result = (PlanState *) ExecInitCustomScan((CustomScan *) node,
													  estate, eflags);
			break;

			/*
			 * join nodes
			 */
		case T_NestLoop:
			result = (PlanState *) ExecInitNestLoop((NestLoop *) node,
													estate, eflags);
			break;

		case T_MergeJoin:
			result = (PlanState *) ExecInitMergeJoin((MergeJoin *) node,
													 estate, eflags);
			break;

		case T_HashJoin:
			result = (PlanState *) ExecInitHashJoin((HashJoin *) node,
													estate, eflags);
			break;

			/*
			 * share input nodes
			 */
		case T_ShareInputScan:
			result = (PlanState *) ExecInitShareInputScan((ShareInputScan *) node, estate, eflags);
			break;

			/*
			 * materialization nodes
			 */
		case T_Material:
			result = (PlanState *) ExecInitMaterial((Material *) node,
													estate, eflags);
			break;

		case T_Sort:
			result = (PlanState *) ExecInitSort((Sort *) node,
												estate, eflags);
			break;

		case T_Agg:
			result = (PlanState *) ExecInitAgg((Agg *) node,
												 estate, eflags);
			break;

		case T_TupleSplit:
			result = (PlanState *) ExecInitTupleSplit((TupleSplit *) node,
													  estate, eflags);
			break;

		case T_WindowAgg:
			result = (PlanState *) ExecInitWindowAgg((WindowAgg *) node,
													 estate, eflags);
			break;

		case T_Unique:
			result = (PlanState *) ExecInitUnique((Unique *) node,
												  estate, eflags);
			break;

		case T_Gather:
			result = (PlanState *) ExecInitGather((Gather *) node,
												  estate, eflags);
			break;

		case T_GatherMerge:
			result = (PlanState *) ExecInitGatherMerge((GatherMerge *) node,
													   estate, eflags);
			break;

		case T_Hash:
			result = (PlanState *) ExecInitHash((Hash *) node,
												estate, eflags);
			break;

		case T_SetOp:
			result = (PlanState *) ExecInitSetOp((SetOp *) node,
												 estate, eflags);
			break;

		case T_LockRows:
			result = (PlanState *) ExecInitLockRows((LockRows *) node,
													estate, eflags);
			break;

		case T_Limit:
			result = (PlanState *) ExecInitLimit((Limit *) node,
												 estate, eflags);
			break;

		case T_Motion:
			result = (PlanState *) ExecInitMotion((Motion *) node,
												  estate, eflags);
			break;

		case T_SplitUpdate:
			result = (PlanState *) ExecInitSplitUpdate((SplitUpdate *) node,
												  estate, eflags);
			break;
		case T_AssertOp:
 			result = (PlanState *) ExecInitAssertOp((AssertOp *) node,
 												  estate, eflags);
			break;
		case T_PartitionSelector:
			result = (PlanState *) ExecInitPartitionSelector((PartitionSelector *) node,
															estate, eflags);
			break;
		default:
			elog(ERROR, "unrecognized node type: %d", (int) nodeTag(node));
			result = NULL;		/* keep compiler quiet */
			break;
	}

	ExecSetExecProcNode(result, result->ExecProcNode);

	if ((estate->es_instrument & INSTRUMENT_MEMORY_DETAIL) != 0)
	{
		Assert(CurrentMemoryContext == nodecxt);
		result->node_context = nodecxt;
		MemoryContextSwitchTo(oldcxt);
	}

	/*
	 * Initialize any initPlans present in this node.  The planner put them in
	 * a separate list for us.
	 */
	subps = NIL;
	foreach(l, node->initPlan)
	{
		SubPlan    *subplan = (SubPlan *) lfirst(l);
		SubPlanState *sstate;
		int			origSliceId = estate->currentSliceId;

		Assert(IsA(subplan, SubPlan));

		estate->currentSliceId = estate->es_plannedstmt->subplan_sliceIds[subplan->plan_id - 1];

		sstate = ExecInitSubPlan(subplan, result);
		subps = lappend(subps, sstate);

		estate->currentSliceId = origSliceId;
	}
	if (result != NULL)
		result->initPlan = subps;

	/* Set up instrumentation for this node if requested */
	if (estate->es_instrument && result != NULL)
		result->instrument = GpInstrAlloc(node, estate->es_instrument);

	return result;
}

/*
 * If a node wants to change its ExecProcNode function after ExecInitNode()
 * has finished, it should do so with this function.  That way any wrapper
 * functions can be reinstalled, without the node having to know how that
 * works.
 */
void
ExecSetExecProcNode(PlanState *node, ExecProcNodeMtd function)
{
	/*
	 * Add a wrapper around the ExecProcNode callback that checks stack depth
	 * during the first execution and maybe adds an instrumentation wrapper.
	 * When the callback is changed after execution has already begun that
	 * means we'll superfluously execute ExecProcNodeFirst, but that seems ok.
	 */
	node->ExecProcNodeReal = function;
	node->ExecProcNode = ExecProcNodeFirst;
}


/*
 * ExecProcNode wrapper that performs some one-time checks, before calling
 * the relevant node method (possibly via an instrumentation wrapper).
 */
static TupleTableSlot *
ExecProcNodeFirst(PlanState *node)
{
	/*
	 * Perform stack depth check during the first execution of the node.  We
	 * only do so the first time round because it turns out to not be cheap on
	 * some common architectures (eg. x86).  This relies on the assumption
	 * that ExecProcNode calls for a given plan node will always be made at
	 * roughly the same stack depth.
	 */
	check_stack_depth();

	/*
	 * If instrumentation is required, change the wrapper to one that just
	 * does instrumentation.  Otherwise we can dispense with all wrappers and
	 * have ExecProcNode() directly call the relevant function from now on.
	 *
	 * GPDB: Unfortunately, GPDB has a bunch of extra stuff that we need
	 * to do on every node, so we cannot make use of this upstream optimization.
	 * ExecProcNodeGPDB() is a wrapper that does all the Greenplum-specific
	 * extra stuff.
	 */
#if 0
	if (node->instrument)
		node->ExecProcNode = ExecProcNodeInstr;
	else
		node->ExecProcNode = node->ExecProcNodeReal;
#endif
	node->ExecProcNode = ExecProcNodeGPDB;

	return node->ExecProcNode(node);
}

static TupleTableSlot *
ExecProcNodeGPDB(PlanState *node)
{
	TupleTableSlot *result;
	MemoryContext oldcxt = NULL;

	/*
	 * Even if we are requested to finish query, Motion has to do its work
	 * to tell End of Stream message to upper slice.  He will probably get
	 * NULL tuple from underlying operator by calling another ExecProcNode,
	 * so one additional operator execution should not be a big hit.
	 */
	if (QueryFinishPending && !IsA(node, MotionState))
		return NULL;

	if (node->plan)
		TRACE_POSTGRESQL_EXECPROCNODE_ENTER(GpIdentity.segindex, currentSliceId, nodeTag(node), node->plan->plan_node_id);

	if (node->squelched)
		elog(ERROR, "cannot execute squelched plan node of type: %d",
			 (int) nodeTag(node));

	if (!node->fHadSentNodeStart)
	{
		/* GPDB hook for collecting query info */
		if (query_info_collect_hook)
			(*query_info_collect_hook)(METRICS_PLAN_NODE_EXECUTING, node);
		node->fHadSentNodeStart = true;
	}

	if (node->instrument)
		InstrStartNode(node->instrument);

	if ((node->state->es_instrument & INSTRUMENT_MEMORY_DETAIL) != 0)
		oldcxt = MemoryContextSwitchTo(node->node_context);

	result = node->ExecProcNodeReal(node);

	if ((node->state->es_instrument & INSTRUMENT_MEMORY_DETAIL) != 0)
	{
		Assert(CurrentMemoryContext == node->node_context);
		MemoryContextSwitchTo(oldcxt);
	}

	if (node->instrument)
		InstrStopNode(node->instrument, TupIsNull(result) ? 0.0 : 1.0);

	if (node->plan)
		TRACE_POSTGRESQL_EXECPROCNODE_EXIT(GpIdentity.segindex, currentSliceId, nodeTag(node), node->plan->plan_node_id);

	return result;
}

/*
 * ExecProcNode wrapper that performs instrumentation calls.  By keeping
 * this a separate function, we avoid overhead in the normal case where
 * no instrumentation is wanted.
 */
#if 0
static TupleTableSlot *
ExecProcNodeInstr(PlanState *node)
{
	TupleTableSlot *result;

	InstrStartNode(node->instrument);

	result = node->ExecProcNodeReal(node);

	InstrStopNode(node->instrument, TupIsNull(result) ? 0.0 : 1.0);

	return result;
}
#endif

/* ----------------------------------------------------------------
 *		MultiExecProcNode
 *
 *		Execute a node that doesn't return individual tuples
 *		(it might return a hashtable, bitmap, etc).  Caller should
 *		check it got back the expected kind of Node.
 *
 * This has essentially the same responsibilities as ExecProcNode,
 * but it does not do InstrStartNode/InstrStopNode (mainly because
 * it can't tell how many returned tuples to count).  Each per-node
 * function must provide its own instrumentation support.
 * ----------------------------------------------------------------
 */
Node *
MultiExecProcNode(PlanState *node)
{
	Node	   *result;

	check_stack_depth();

	CHECK_FOR_INTERRUPTS();

	Assert(NULL != node->plan);

	TRACE_POSTGRESQL_EXECPROCNODE_ENTER(GpIdentity.segindex, currentSliceId, nodeTag(node), node->plan->plan_node_id);
	
	if (!node->fHadSentNodeStart)
	{
		/* GPDB hook for collecting query info */
		if (query_info_collect_hook)
			(*query_info_collect_hook)(METRICS_PLAN_NODE_EXECUTING, node);
		node->fHadSentNodeStart = true;
	}

	if (node->chgParam != NULL) /* something changed */
		ExecReScan(node);		/* let ReScan handle this */

	switch (nodeTag(node))
	{
			/*
			 * Only node types that actually support multiexec will be listed
			 */

		case T_HashState:
			result = MultiExecHash((HashState *) node);
			break;

		case T_BitmapIndexScanState:
			result = MultiExecBitmapIndexScan((BitmapIndexScanState *) node);
			break;

		case T_DynamicBitmapIndexScanState:
			result = MultiExecDynamicBitmapIndexScan((DynamicBitmapIndexScanState *) node);
			break;

		case T_BitmapAndState:
			result = MultiExecBitmapAnd((BitmapAndState *) node);
			break;

		case T_BitmapOrState:
			result = MultiExecBitmapOr((BitmapOrState *) node);
			break;

		default:
			elog(ERROR, "unrecognized node type: %d", (int) nodeTag(node));
			result = NULL;
			break;
	}

	TRACE_POSTGRESQL_EXECPROCNODE_EXIT(GpIdentity.segindex, currentSliceId, nodeTag(node), node->plan->plan_node_id);

	return result;
}

/* ----------------------------------------------------------------
 *		ExecEndNode
 *
 *		Recursively cleans up all the nodes in the plan rooted
 *		at 'node'.
 *
 *		After this operation, the query plan will not be able to be
 *		processed any further.  This should be called only after
 *		the query plan has been fully executed.
 * ----------------------------------------------------------------
 */
void
ExecEndNode(PlanState *node)
{
	/*
	 * do nothing when we get to the end of a leaf on tree.
	 */
	if (node == NULL)
		return;

	/*
	 * Make sure there's enough stack available. Need to check here, in
	 * addition to ExecProcNode() (via ExecProcNodeFirst()), because it's not
	 * guaranteed that ExecProcNode() is reached for all nodes.
	 */
	check_stack_depth();

	if (node->chgParam != NULL)
	{
		bms_free(node->chgParam);
		node->chgParam = NULL;
	}

	/* Free EXPLAIN ANALYZE buffer */
	if (node->cdbexplainbuf)
	{
		if (node->cdbexplainbuf->data)
			pfree(node->cdbexplainbuf->data);
		pfree(node->cdbexplainbuf);
		node->cdbexplainbuf = NULL;
	}

	switch (nodeTag(node))
	{
			/*
			 * control nodes
			 */
		case T_ResultState:
			ExecEndResult((ResultState *) node);
			break;

		case T_ProjectSetState:
			ExecEndProjectSet((ProjectSetState *) node);
			break;

		case T_ModifyTableState:
			ExecEndModifyTable((ModifyTableState *) node);
			break;

		case T_AppendState:
			ExecEndAppend((AppendState *) node);
			break;

		case T_MergeAppendState:
			ExecEndMergeAppend((MergeAppendState *) node);
			break;

		case T_RecursiveUnionState:
			ExecEndRecursiveUnion((RecursiveUnionState *) node);
			break;

		case T_SequenceState:
			ExecEndSequence((SequenceState *) node);
			break;

		case T_BitmapAndState:
			ExecEndBitmapAnd((BitmapAndState *) node);
			break;

		case T_BitmapOrState:
			ExecEndBitmapOr((BitmapOrState *) node);
			break;

			/*
			 * scan nodes
			 */
		case T_SeqScanState:
			ExecEndSeqScan((SeqScanState *) node);
			break;

		case T_DynamicSeqScanState:
			ExecEndDynamicSeqScan((DynamicSeqScanState *) node);
			break;

		case T_SampleScanState:
			ExecEndSampleScan((SampleScanState *) node);
			break;

		case T_GatherState:
			ExecEndGather((GatherState *) node);
			break;

		case T_GatherMergeState:
			ExecEndGatherMerge((GatherMergeState *) node);
			break;

		case T_IndexScanState:
			ExecEndIndexScan((IndexScanState *) node);
			break;

		case T_DynamicIndexScanState:
			ExecEndDynamicIndexScan((DynamicIndexScanState *) node);
			break;

		case T_IndexOnlyScanState:
			ExecEndIndexOnlyScan((IndexOnlyScanState *) node);
			break;

		case T_BitmapIndexScanState:
			ExecEndBitmapIndexScan((BitmapIndexScanState *) node);
			break;

		case T_DynamicBitmapIndexScanState:
			ExecEndDynamicBitmapIndexScan((DynamicBitmapIndexScanState *) node);
			break;

		case T_BitmapHeapScanState:
			ExecEndBitmapHeapScan((BitmapHeapScanState *) node);
			break;

		case T_DynamicBitmapHeapScanState:
			ExecEndDynamicBitmapHeapScan((DynamicBitmapHeapScanState *) node);
			break;

		case T_TidScanState:
			ExecEndTidScan((TidScanState *) node);
			break;

		case T_SubqueryScanState:
			ExecEndSubqueryScan((SubqueryScanState *) node);
			break;

		case T_FunctionScanState:
			ExecEndFunctionScan((FunctionScanState *) node);
			break;

		case T_TableFunctionState:
			ExecEndTableFunction((TableFunctionState *) node);
			break;

		case T_TableFuncScanState:
			ExecEndTableFuncScan((TableFuncScanState *) node);
			break;

		case T_ValuesScanState:
			ExecEndValuesScan((ValuesScanState *) node);
			break;

		case T_CteScanState:
			ExecEndCteScan((CteScanState *) node);
			break;

		case T_NamedTuplestoreScanState:
			ExecEndNamedTuplestoreScan((NamedTuplestoreScanState *) node);
			break;

		case T_WorkTableScanState:
			ExecEndWorkTableScan((WorkTableScanState *) node);
			break;

		case T_ForeignScanState:
			ExecEndForeignScan((ForeignScanState *) node);
			break;

		case T_DynamicForeignScanState:
			ExecEndDynamicForeignScan((DynamicForeignScanState *) node);
			break;

		case T_CustomScanState:
			ExecEndCustomScan((CustomScanState *) node);
			break;

			/*
			 * join nodes
			 */
		case T_NestLoopState:
			ExecEndNestLoop((NestLoopState *) node);
			break;

		case T_MergeJoinState:
			ExecEndMergeJoin((MergeJoinState *) node);
			break;

		case T_HashJoinState:
			ExecEndHashJoin((HashJoinState *) node);
			break;

			/*
			 * ShareInput nodes
			 */
		case T_ShareInputScanState:
			ExecEndShareInputScan((ShareInputScanState *) node);
			break;

			/*
			 * materialization nodes
			 */
		case T_MaterialState:
			ExecEndMaterial((MaterialState *) node);
			break;

		case T_SortState:
			ExecEndSort((SortState *) node);
			break;

		case T_AggState:
			ExecEndAgg((AggState *) node);
			break;

		case T_TupleSplitState:
			ExecEndTupleSplit((TupleSplitState *) node);
			break;

		case T_WindowAggState:
			ExecEndWindowAgg((WindowAggState *) node);
			break;

		case T_UniqueState:
			ExecEndUnique((UniqueState *) node);
			break;

		case T_HashState:
			ExecEndHash((HashState *) node);
			break;

		case T_SetOpState:
			ExecEndSetOp((SetOpState *) node);
			break;

		case T_LockRowsState:
			ExecEndLockRows((LockRowsState *) node);
			break;

		case T_LimitState:
			ExecEndLimit((LimitState *) node);
			break;

		case T_MotionState:
			ExecEndMotion((MotionState *) node);
			break;

			/*
			 * DML nodes
			 */
		case T_SplitUpdateState:
			ExecEndSplitUpdate((SplitUpdateState *) node);
			break;
		case T_AssertOpState:
			ExecEndAssertOp((AssertOpState *) node);
			break;
		case T_PartitionSelectorState:
			ExecEndPartitionSelector((PartitionSelectorState *) node);
			break;

		default:
			elog(ERROR, "unrecognized node type: %d", (int) nodeTag(node));
			break;
	}
	/* GPDB hook for collecting query info */
	if (query_info_collect_hook)
		(*query_info_collect_hook)(METRICS_PLAN_NODE_FINISHED, node);
}



/* -----------------------------------------------------------------------
 *						PlanState Tree Walking Functions
 * -----------------------------------------------------------------------
 *
 * planstate_walk_node
 *	  Calls a 'walker' function for the given PlanState node; or returns
 *	  CdbVisit_Walk if 'planstate' is NULL.
 *
 *	  If 'walker' returns CdbVisit_Walk, then this function calls
 *	  planstate_walk_kids() to visit the node's children, and returns
 *	  the result.
 *
 *	  If 'walker' returns CdbVisit_Skip, then this function immediately
 *	  returns CdbVisit_Walk and does not visit the node's children.
 *
 *	  If 'walker' returns CdbVisit_Stop or another value, then this function
 *	  immediately returns that value and does not visit the node's children.
 *
 * planstate_walk_array
 *	  Calls planstate_walk_node() for each non-NULL PlanState ptr in
 *	  the given array of pointers to PlanState objects.
 *
 *	  Quits if the result of planstate_walk_node() is CdbVisit_Stop or another
 *	  value other than CdbVisit_Walk, and returns that result without visiting
 *	  any more nodes.
 *
 *	  Returns CdbVisit_Walk if 'planstates' is NULL, or if all of the
 *	  subtrees return CdbVisit_Walk.
 *
 *	  Note that this function never returns CdbVisit_Skip to its caller.
 *	  Only the caller's 'walker' function can return CdbVisit_Skip.
 *
 * planstate_walk_list
 *	  Calls planstate_walk_node() for each PlanState node in the given List.
 *
 *	  Quits if the result of planstate_walk_node() is CdbVisit_Stop or another
 *	  value other than CdbVisit_Walk, and returns that result without visiting
 *	  any more nodes.
 *
 *	  Returns CdbVisit_Walk if all of the subtrees return CdbVisit_Walk, or
 *	  if the list is empty.
 *
 *	  Note that this function never returns CdbVisit_Skip to its caller.
 *	  Only the caller's 'walker' function can return CdbVisit_Skip.
 *
 * planstate_walk_kids
 *	  Calls planstate_walk_node() for each child of the given PlanState node.
 *
 *	  Quits if the result of planstate_walk_node() is CdbVisit_Stop or another
 *	  value other than CdbVisit_Walk, and returns that result without visiting
 *	  any more nodes.
 *
 *	  Returns CdbVisit_Walk if the given planstate node ptr is NULL, or if
 *	  all of the children return CdbVisit_Walk, or if there are no children.
 *
 *	  Note that this function never returns CdbVisit_Skip to its caller.
 *	  Only the 'walker' can return CdbVisit_Skip.
 *
 * NB: All CdbVisitOpt values other than CdbVisit_Walk or CdbVisit_Skip are
 * treated as equivalent to CdbVisit_Stop.  Thus the walker can break out
 * of a traversal and at the same time return a smidgen of information to the
 * caller, perhaps to indicate the reason for termination.  For convenience,
 * a couple of alternative stopping codes are predefined for walkers to use at
 * their discretion: CdbVisit_Failure and CdbVisit_Success.
 *
 * NB: We do not visit the left subtree of a NestLoopState node (NJ) whose
 * 'shared_outer' flag is set.  This occurs when the NJ is the left child of
 * an AdaptiveNestLoopState (AJ); the AJ's right child is a HashJoinState (HJ);
 * and both the NJ and HJ point to the same left subtree.  This way we avoid
 * visiting the common subtree twice when descending through the AJ node.
 * The caller's walker function can handle the NJ as a special case to
 * override this behavior if there is a need to always visit both subtrees.
 *
 * NB: Use PSW_* flags to skip walking certain parts of the planstate tree.
 * -----------------------------------------------------------------------
 */


/**
 * Version of walker that uses no flags.
 */
CdbVisitOpt
planstate_walk_node(PlanState *planstate,
				 CdbVisitOpt (*walker) (PlanState *planstate, void *context),
					void *context)
{
	return planstate_walk_node_extended(planstate, walker, context, 0);
}

/**
 * Workhorse walker that uses flags.
 */
CdbVisitOpt
planstate_walk_node_extended(PlanState *planstate,
				 CdbVisitOpt (*walker) (PlanState *planstate, void *context),
							 void *context,
							 int flags)
{
	CdbVisitOpt whatnext;

	if (planstate == NULL)
		whatnext = CdbVisit_Walk;
	else
	{
		whatnext = walker(planstate, context);
		if (whatnext == CdbVisit_Walk)
			whatnext = planstate_walk_kids(planstate, walker, context, flags);
		else if (whatnext == CdbVisit_Skip)
			whatnext = CdbVisit_Walk;
	}
	Assert(whatnext != CdbVisit_Skip);
	return whatnext;
}	/* planstate_walk_node */

CdbVisitOpt
planstate_walk_array(PlanState **planstates,
					 int nplanstate,
				 CdbVisitOpt (*walker) (PlanState *planstate, void *context),
					 void *context,
					 int flags)
{
	CdbVisitOpt whatnext = CdbVisit_Walk;
	int			i;

	if (planstates == NULL)
		return CdbVisit_Walk;

	for (i = 0; i < nplanstate && whatnext == CdbVisit_Walk; i++)
		whatnext = planstate_walk_node_extended(planstates[i], walker, context, flags);

	return whatnext;
}	/* planstate_walk_array */

CdbVisitOpt
planstate_walk_kids(PlanState *planstate,
				 CdbVisitOpt (*walker) (PlanState *planstate, void *context),
					void *context,
					int flags)
{
	CdbVisitOpt v;

	if (planstate == NULL)
		return CdbVisit_Walk;

	switch (nodeTag(planstate))
	{
		case T_NestLoopState:
			{
				NestLoopState *nls = (NestLoopState *) planstate;

				/*
				 * Don't visit left subtree of NJ if it is shared with brother
				 * HJ
				 */
				if (nls->shared_outer)
					v = CdbVisit_Walk;
				else
					v = planstate_walk_node_extended(planstate->lefttree, walker, context, flags);

				/* Right subtree */
				if (v == CdbVisit_Walk)
					v = planstate_walk_node_extended(planstate->righttree, walker, context, flags);
				break;
			}

		case T_AppendState:
			{
				AppendState *as = (AppendState *) planstate;

				v = planstate_walk_array(as->appendplans, as->as_nplans, walker, context, flags);
				Assert(!planstate->lefttree && !planstate->righttree);
				break;
			}

		case T_MergeAppendState:
			{
				MergeAppendState *ms = (MergeAppendState *) planstate;

				v = planstate_walk_array(ms->mergeplans, ms->ms_nplans, walker, context, flags);
				Assert(!planstate->lefttree && !planstate->righttree);
				break;
			}

		case T_ModifyTableState:
			{
				ModifyTableState *mts = (ModifyTableState *) planstate;

				v = planstate_walk_array(mts->mt_plans, mts->mt_nplans, walker, context, flags);
				Assert(!planstate->lefttree && !planstate->righttree);
				break;
			}

		case T_SequenceState:
			{
				SequenceState *ss = (SequenceState *) planstate;

				v = planstate_walk_array(ss->subplans, ss->numSubplans, walker, context, flags);
				Assert(!planstate->lefttree && !planstate->righttree);
				break;
			}

		case T_BitmapAndState:
			{
				BitmapAndState *bas = (BitmapAndState *) planstate;

				v = planstate_walk_array(bas->bitmapplans, bas->nplans, walker, context, flags);
				Assert(!planstate->lefttree && !planstate->righttree);
				break;
			}
		case T_BitmapOrState:
			{
				BitmapOrState *bos = (BitmapOrState *) planstate;

				v = planstate_walk_array(bos->bitmapplans, bos->nplans, walker, context, flags);
				Assert(!planstate->lefttree && !planstate->righttree);
				break;
			}

		case T_SubqueryScanState:
			v = planstate_walk_node_extended(((SubqueryScanState *) planstate)->subplan, walker, context, flags);
			Assert(!planstate->lefttree && !planstate->righttree);
			break;

		default:
			/* Left subtree */
			v = planstate_walk_node_extended(planstate->lefttree, walker, context, flags);

			/* Right subtree */
			if (v == CdbVisit_Walk)
				v = planstate_walk_node_extended(planstate->righttree, walker, context, flags);
			break;
	}

	/* Init plan subtree */
	if (!(flags & PSW_IGNORE_INITPLAN)
		&& (v == CdbVisit_Walk))
	{
		ListCell   *lc = NULL;
		CdbVisitOpt v1 = v;

		foreach(lc, planstate->initPlan)
		{
			SubPlanState *sps = (SubPlanState *) lfirst(lc);
			PlanState  *ips = sps->planstate;

			Assert(ips);
			if (v1 == CdbVisit_Walk)
			{
				v1 = planstate_walk_node_extended(ips, walker, context, flags);
			}
		}
	}

	/* Sub plan subtree */
	if (v == CdbVisit_Walk)
	{
		ListCell   *lc = NULL;
		CdbVisitOpt v1 = v;

		foreach(lc, planstate->subPlan)
		{
			SubPlanState *sps = (SubPlanState *) lfirst(lc);
			PlanState  *ips = sps->planstate;

			if (!ips)
				elog(ERROR, "subplan has no planstate");
			if (v1 == CdbVisit_Walk)
			{
				v1 = planstate_walk_node_extended(ips, walker, context, flags);
			}
		}

	}

	return v;
}	/* planstate_walk_kids */

/*
 * ExecShutdownNode
 *
 * Give execution nodes a chance to stop asynchronous resource consumption
 * and release any resources still held.
 */
bool
ExecShutdownNode(PlanState *node)
{
	if (node == NULL)
		return false;

	check_stack_depth();

	/*
	 * Treat the node as running while we shut it down, but only if it's run
	 * at least once already.  We don't expect much CPU consumption during
	 * node shutdown, but in the case of Gather or Gather Merge, we may shut
	 * down workers at this stage.  If so, their buffer usage will get
	 * propagated into pgBufferUsage at this point, and we want to make sure
	 * that it gets associated with the Gather node.  We skip this if the node
	 * has never been executed, so as to avoid incorrectly making it appear
	 * that it has.
	 */
	if (node->instrument && node->instrument->running)
		InstrStartNode(node->instrument);

	planstate_tree_walker(node, ExecShutdownNode, NULL);

	switch (nodeTag(node))
	{
		case T_GatherState:
			ExecShutdownGather((GatherState *) node);
			break;
		case T_ForeignScanState:
			ExecShutdownForeignScan((ForeignScanState *) node);
			break;
		case T_CustomScanState:
			ExecShutdownCustomScan((CustomScanState *) node);
			break;
		case T_GatherMergeState:
			ExecShutdownGatherMerge((GatherMergeState *) node);
			break;
		case T_HashState:
			ExecShutdownHash((HashState *) node);
			break;
		case T_HashJoinState:
			ExecShutdownHashJoin((HashJoinState *) node);
			break;
		default:
			break;
	}

	/* Stop the node if we started it above, reporting 0 tuples. */
	if (node->instrument && node->instrument->running)
		InstrStopNode(node->instrument, 0);

	return false;
}

/*
 * ExecSetTupleBound
 *
 * Set a tuple bound for a planstate node.  This lets child plan nodes
 * optimize based on the knowledge that the maximum number of tuples that
 * their parent will demand is limited.  The tuple bound for a node may
 * only be changed between scans (i.e., after node initialization or just
 * before an ExecReScan call).
 *
 * Any negative tuples_needed value means "no limit", which should be the
 * default assumption when this is not called at all for a particular node.
 *
 * Note: if this is called repeatedly on a plan tree, the exact same set
 * of nodes must be updated with the new limit each time; be careful that
 * only unchanging conditions are tested here.
 */
void
ExecSetTupleBound(int64 tuples_needed, PlanState *child_node)
{
	/*
	 * Since this function recurses, in principle we should check stack depth
	 * here.  In practice, it's probably pointless since the earlier node
	 * initialization tree traversal would surely have consumed more stack.
	 */

	if (IsA(child_node, SortState))
	{
		/*
		 * If it is a Sort node, notify it that it can use bounded sort.
		 *
		 * Note: it is the responsibility of nodeSort.c to react properly to
		 * changes of these parameters.  If we ever redesign this, it'd be a
		 * good idea to integrate this signaling with the parameter-change
		 * mechanism.
		 */
		SortState  *sortState = (SortState *) child_node;

		if (tuples_needed < 0)
		{
			/* make sure flag gets reset if needed upon rescan */
			sortState->bounded = false;
		}
		else
		{
			sortState->bounded = true;
			sortState->bound = tuples_needed;
		}
	}
	else if (IsA(child_node, AppendState))
	{
		/*
		 * If it is an Append, we can apply the bound to any nodes that are
		 * children of the Append, since the Append surely need read no more
		 * than that many tuples from any one input.
		 */
		AppendState *aState = (AppendState *) child_node;
		int			i;

		for (i = 0; i < aState->as_nplans; i++)
			ExecSetTupleBound(tuples_needed, aState->appendplans[i]);
	}
	else if (IsA(child_node, MergeAppendState))
	{
		/*
		 * If it is a MergeAppend, we can apply the bound to any nodes that
		 * are children of the MergeAppend, since the MergeAppend surely need
		 * read no more than that many tuples from any one input.
		 */
		MergeAppendState *maState = (MergeAppendState *) child_node;
		int			i;

		for (i = 0; i < maState->ms_nplans; i++)
			ExecSetTupleBound(tuples_needed, maState->mergeplans[i]);
	}
	else if (IsA(child_node, ResultState))
	{
		/*
		 * Similarly, for a projecting Result, we can apply the bound to its
		 * child node.
		 *
		 * If Result supported qual checking, we'd have to punt on seeing a
		 * qual.  Note that having a resconstantqual is not a showstopper: if
		 * that condition succeeds it affects nothing, while if it fails, no
		 * rows will be demanded from the Result child anyway.
		 */

		/*
		* GPDB: The second condition ensures qual is empty.
		* In case the RESULT node has a filter,
		* we do not want to push down the tuple bound.
		* This is because the bound will be applied before the filter,
		* and could lead to a subset of the actual result being returned.
		* Postgres doesn't create RESULT node with quals,
		* whereas GPDB can, hence the deviation.
		* See ExecResult() and create_projection_path_with_quals().
		*/

		if (outerPlanState(child_node) && child_node->plan->qual == NULL)
			ExecSetTupleBound(tuples_needed, outerPlanState(child_node));
	}
	else if (IsA(child_node, SubqueryScanState))
	{
		/*
		 * We can also descend through SubqueryScan, but only if it has no
		 * qual (otherwise it might discard rows).
		 */
		SubqueryScanState *subqueryState = (SubqueryScanState *) child_node;

		if (subqueryState->ss.ps.qual == NULL)
			ExecSetTupleBound(tuples_needed, subqueryState->subplan);
	}
	else if (IsA(child_node, GatherState))
	{
		/*
		 * A Gather node can propagate the bound to its workers.  As with
		 * MergeAppend, no one worker could possibly need to return more
		 * tuples than the Gather itself needs to.
		 *
		 * Note: As with Sort, the Gather node is responsible for reacting
		 * properly to changes to this parameter.
		 */
		GatherState *gstate = (GatherState *) child_node;

		gstate->tuples_needed = tuples_needed;

		/* Also pass down the bound to our own copy of the child plan */
		ExecSetTupleBound(tuples_needed, outerPlanState(child_node));
	}
	else if (IsA(child_node, GatherMergeState))
	{
		/* Same comments as for Gather */
		GatherMergeState *gstate = (GatherMergeState *) child_node;

		gstate->tuples_needed = tuples_needed;

		ExecSetTupleBound(tuples_needed, outerPlanState(child_node));
	}

	/*
	 * In principle we could descend through any plan node type that is
	 * certain not to discard or combine input rows; but on seeing a node that
	 * can do that, we can't propagate the bound any further.  For the moment
	 * it's unclear that any other cases are worth checking here.
	 */
}
