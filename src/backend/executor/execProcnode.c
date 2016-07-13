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
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/executor/execProcnode.c,v 1.62 2008/01/01 19:45:49 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
/*
 *	 INTERFACE ROUTINES
 *		ExecCountSlotsNode -	count tuple slots needed by plan tree
 *		ExecInitNode	-		initialize a plan node and its subplans
 *		ExecProcNode	-		get a tuple by executing the plan node
 *		ExecEndNode		-		shut down a plan node and its subplans
 *		ExecSquelchNode		-	notify subtree that no more tuples are needed
 *		ExecStateTreeWalker -	call given function for each node of plan state
 *
 *	 NOTES
 *		This used to be three files.  It is now all combined into
 *		one file so that it is easier to keep ExecInitNode, ExecProcNode,
 *		and ExecEndNode in sync when new nodes are added.
 *
 *	 EXAMPLE
 *		Suppose we want the age of the manager of the shoe department and
 *		the number of employees in that department.  So we have the query:
 *
 *				select DEPT.no_emps, EMP.age
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
 *		and so forth until the entire plan is initialized.	The result
 *		of ExecInitNode() is a plan state tree built with the same structure
 *		as the underlying plan tree.
 *
 *	  * Then when ExecRun() is called, it calls ExecutePlan() which calls
 *		ExecProcNode() repeatedly on the top node of the plan state tree.
 *		Each time this happens, ExecProcNode() will end up calling
 *		ExecNestLoop(), which calls ExecProcNode() on its subplans.
 *		Each of these subplans is a sequential scan so ExecSeqScan() is
 *		called.  The slots returned by ExecSeqScan() may contain
 *		tuples which contain the attributes ExecNestLoop() uses to
 *		form the tuples it returns.
 *
 *	  * Eventually ExecSeqScan() stops returning tuples and the nest
 *		loop join ends.  Lastly, ExecEnd() calls ExecEndNode() which
 *		calls ExecEndNestLoop() which in turn calls ExecEndNode() on
 *		its subplans which result in ExecEndSeqScan().
 *
 *		This should show how the executor works by having
 *		ExecInitNode(), ExecProcNode() and ExecEndNode() dispatch
 *		their work to the appopriate node support routines which may
 *		in turn call these routines themselves on their subplans.
 */
#include "postgres.h"

#include "executor/executor.h"
#include "executor/instrument.h"
#include "executor/nodeAgg.h"
#include "executor/nodeAppend.h"
#include "executor/nodeAssertOp.h"
#include "executor/nodeSequence.h"
#include "executor/nodeBitmapAnd.h"
#include "executor/nodeBitmapHeapscan.h"
#include "executor/nodeBitmapIndexscan.h"
#include "executor/nodeBitmapTableScan.h"
#include "executor/nodeBitmapOr.h"
#include "executor/nodeBitmapAppendOnlyscan.h"
#include "executor/nodeExternalscan.h"
#include "executor/nodeTableScan.h"
#include "executor/nodeDML.h"
#include "executor/nodeDynamicIndexscan.h"
#include "executor/nodeDynamicTableScan.h"
#include "executor/nodeFunctionscan.h"
#include "executor/nodeHash.h"
#include "executor/nodeHashjoin.h"
#include "executor/nodeIndexscan.h"
#include "executor/nodeLimit.h"
#include "executor/nodeMaterial.h"
#include "executor/nodeMergejoin.h"
#include "executor/nodeMotion.h"
#include "executor/nodeNestloop.h"
#include "executor/nodeRepeat.h"
#include "executor/nodeResult.h"
#include "executor/nodeRowTrigger.h"
#include "executor/nodeSetOp.h"
#include "executor/nodeShareInputScan.h"
#include "executor/nodeSort.h"
#include "executor/nodeSplitUpdate.h"
#include "executor/nodeSubplan.h"
#include "executor/nodeSubqueryscan.h"
#include "executor/nodeTableFunction.h"
#include "executor/nodeTidscan.h"
#include "executor/nodeUnique.h"
#include "executor/nodeValuesscan.h"
#include "executor/nodeWindow.h"
#include "executor/nodePartitionSelector.h"
#include "miscadmin.h"
#include "tcop/tcopprot.h"
#include "cdb/cdbvars.h"

#include "cdb/ml_ipc.h" /* interconnect context */

#include "utils/debugbreak.h"
#include "pg_trace.h"

#include "codegen/codegen_wrapper.h"

#ifdef CDB_TRACE_EXECUTOR
#include "nodes/print.h"
static void ExecCdbTraceNode(PlanState *node, bool entry, TupleTableSlot *result);
#endif   /* CDB_TRACE_EXECUTOR */

 /* flags bits for planstate walker */
 #define PSW_IGNORE_INITPLAN	0x01

 /**
  * Forward declarations of static functions
  */
 static CdbVisitOpt
 planstate_walk_node_extended(PlanState      *planstate,
 			        CdbVisitOpt   (*walker)(PlanState *planstate, void *context),
 			        void           *context,
 			        int flags);

 static CdbVisitOpt
 planstate_walk_array(PlanState    **planstates,
                       int            nplanstate,
  			         CdbVisitOpt  (*walker)(PlanState *planstate, void *context),
  			         void          *context,
  			         int flags);

 static CdbVisitOpt
 planstate_walk_kids(PlanState      *planstate,
  			        CdbVisitOpt   (*walker)(PlanState *planstate, void *context),
  			        void           *context,
  			        int flags);

 static void
 EnrollQualList(PlanState* result, TupleTableSlot* slot);

 static void
 EnrollProjInfoTargetList(ProjectionInfo* ProjInfo, TupleTableSlot* slot, bool skip_var);

/*
 * setSubplanSliceId
 *   Set the slice id info for the given subplan.
 */
static void
setSubplanSliceId(SubPlan *subplan, EState *estate)
{
	Assert(subplan!= NULL && IsA(subplan, SubPlan) && estate != NULL);
	
	estate->currentSliceIdInPlan = subplan->qDispSliceId;
	/*
	 * The slice that the initPlan will be running
	 * is the same as the root slice. Depending on
	 * the location of InitPlan in the plan, the root slice is
	 * the root slice of the whole plan, or the root slice
	 * of the parent subplan of this InitPlan.
	 */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		estate->currentExecutingSliceId = RootSliceIndex(estate);
	}
	else
	{
		estate->currentExecutingSliceId = estate->rootSliceId;
	}
}


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

	/*
	 * do nothing when we get to the end of a leaf on tree.
	 */
	if (node == NULL)
		return NULL;

	Assert(estate != NULL);
	int origSliceIdInPlan = estate->currentSliceIdInPlan;
	int origExecutingSliceId = estate->currentExecutingSliceId;

	MemoryAccount* curMemoryAccount = NULL;

	StringInfo codegenManagerName = makeStringInfo();
	appendStringInfo(codegenManagerName, "%s-%d-%d", "execProcnode", node->plan_node_id, node->type);
	void* CodegenManager = CodeGeneratorManagerCreate(codegenManagerName->data);
	START_CODE_GENERATOR_MANAGER(CodegenManager);
	{


	/*
	 * Is current plan node supposed to execute in current slice?
	 * Special case is sending motion node, which is supposed to
	 * update estate->currentSliceIdInPlan inside ExecInitMotion,
	 * but wouldn't get a chance to do so until called in the code
	 * below. But, we want to set up a memory account for sender
	 * motion before we call ExecInitMotion to make sure we don't
	 * miss its allocation memory
	 */
	bool isAlienPlanNode = !((currentSliceId == origSliceIdInPlan) ||
			(nodeTag(node) == T_Motion && ((Motion*)node)->motionID == currentSliceId));

	/*
	 * As of 03/28/2014, there is no support for BitmapTableScan
	 * in the planner/optimizer. Therefore, for testing purpose
	 * we treat Bitmap Heap/AO/AOCO as BitmapTableScan, if the guc
	 * force_bitmap_table_scan is true.
	 *
	 * TODO rahmaf2 04/01/2014: remove all "fake" BitmapTableScan
	 * once the planner/optimizer is capable of generating BitmapTableScan
	 * nodes. [JIRA: MPP-23177]
	 */
	if (force_bitmap_table_scan)
	{
		if (IsA(node, BitmapHeapScan) ||
				IsA(node, BitmapAppendOnlyScan))
		{
			node->type = T_BitmapTableScan;
		}
	}

	switch (nodeTag(node))
	{
			/*
			 * control nodes
			 */
		case T_Result:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, Result);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitResult((Result *) node,
												  estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_Append:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, Append);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitAppend((Append *) node,
												  estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_Sequence:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, Sequence);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitSequence((Sequence *) node,
													estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_BitmapAnd:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, BitmapAnd);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{

			result = (PlanState *) ExecInitBitmapAnd((BitmapAnd *) node,
													 estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_BitmapOr:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, BitmapOr);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitBitmapOr((BitmapOr *) node,
													estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

			/*
			 * scan nodes
			 */
		case T_SeqScan:
		case T_AppendOnlyScan:
		case T_AOCSScan:
		case T_TableScan:
			/* SeqScan, AppendOnlyScan and AOCSScan are defunct */
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, TableScan);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitTableScan((TableScan *) node,
													 estate, eflags);
			/*
			 * Enroll targetlist & quals' expression evaluation functions
			 * in codegen_manager
			 */
			EnrollQualList(result, ((ScanState*) result)->ss_ScanTupleSlot);
			if (NULL !=result)
			{
			  EnrollProjInfoTargetList(result->ps_ProjInfo,
			                           ((ScanState*) result)->ss_ScanTupleSlot,
			                           true /* skip_var */);
			}
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_DynamicTableScan:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, DynamicTableScan);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitDynamicTableScan((DynamicTableScan *) node,
													 estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_ExternalScan:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, ExternalScan);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitExternalScan((ExternalScan *) node,
														estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_IndexScan:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, IndexScan);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitIndexScan((IndexScan *) node,
													 estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_DynamicIndexScan:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, DynamicIndexScan);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitDynamicIndexScan((DynamicIndexScan *) node,
													estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_BitmapIndexScan:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, BitmapIndexScan);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitBitmapIndexScan((BitmapIndexScan *) node,
														   estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_BitmapHeapScan:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, BitmapHeapScan);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitBitmapHeapScan((BitmapHeapScan *) node,
														  estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_BitmapAppendOnlyScan:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, BitmapAppendOnlyScan);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitBitmapAppendOnlyScan((BitmapAppendOnlyScan*) node,
														        estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_BitmapTableScan:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, BitmapTableScan);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitBitmapTableScan((BitmapTableScan*) node,
														        estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_TidScan:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, TidScan);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitTidScan((TidScan *) node,
												   estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_SubqueryScan:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, SubqueryScan);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitSubqueryScan((SubqueryScan *) node,
														estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_FunctionScan:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, FunctionScan);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitFunctionScan((FunctionScan *) node,
														estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_TableFunctionScan:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, TableFunctionScan);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitTableFunction((TableFunctionScan *) node,
														 estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_ValuesScan:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, ValuesScan);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitValuesScan((ValuesScan *) node,
													  estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_NestLoop:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, NestLoop);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitNestLoop((NestLoop *) node,
													estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_MergeJoin:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, MergeJoin);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitMergeJoin((MergeJoin *) node,
													 estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_HashJoin:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, HashJoin);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitHashJoin((HashJoin *) node,
													estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

			/*
			 * share input nodes
			 */
		case T_ShareInputScan:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, ShareInputScan);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitShareInputScan((ShareInputScan *) node, estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

			/*
			 * materialization nodes
			 */
		case T_Material:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, Material);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitMaterial((Material *) node,
													estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_Sort:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, Sort);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitSort((Sort *) node,
												estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_Agg:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, Agg);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitAgg((Agg *) node,
											   estate, eflags);
			/*
			 * Enroll targetlist & quals' expression evaluation functions
			 * in codegen_manager
			 */
			EnrollQualList(result, ((ScanState*) result)->ss_ScanTupleSlot);
			if (NULL != result)
			{
			  AggState* aggstate = (AggState*)result;
			  for (int aggno = 0; aggno < aggstate->numaggs; aggno++)
			  {
			    AggStatePerAgg peraggstate = &aggstate->peragg[aggno];
			    EnrollProjInfoTargetList(peraggstate->evalproj,
			                             ((ScanState*) result)->ss_ScanTupleSlot,
			                             false /* skip_var */);
			  }
			}
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_Window:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, Window);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitWindow((Window *) node,
											   estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_Unique:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, Unique);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitUnique((Unique *) node,
												  estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_Hash:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, Hash);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitHash((Hash *) node,
												estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_SetOp:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, SetOp);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitSetOp((SetOp *) node,
												 estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_Limit:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, Limit);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitLimit((Limit *) node,
												 estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_Motion:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, Motion);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitMotion((Motion *) node,
												  estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_Repeat:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, Repeat);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitRepeat((Repeat *) node,
												  estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;
		case T_DML:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, DML);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitDML((DML *) node,
												  estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;
		case T_SplitUpdate:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, SplitUpdate);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitSplitUpdate((SplitUpdate *) node,
												  estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;
		case T_AssertOp:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, AssertOp);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
 			result = (PlanState *) ExecInitAssertOp((AssertOp *) node,
 												  estate, eflags);
			}
			END_MEMORY_ACCOUNT();
 			break;
		case T_RowTrigger:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, RowTrigger);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
 			result = (PlanState *) ExecInitRowTrigger((RowTrigger *) node,
 												   estate, eflags);
			}
			END_MEMORY_ACCOUNT();
 			break;
		case T_PartitionSelector:
			curMemoryAccount = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, PartitionSelector);

			START_MEMORY_ACCOUNT(curMemoryAccount);
			{
			result = (PlanState *) ExecInitPartitionSelector((PartitionSelector *) node,
															estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;
		default:
			elog(ERROR, "unrecognized node type: %d", (int) nodeTag(node));
			result = NULL;		/* keep compiler quiet */
			break;
	}

	estate->currentSliceIdInPlan = origSliceIdInPlan;
	estate->currentExecutingSliceId = origExecutingSliceId;

	/*
	 * Initialize any initPlans present in this node.  The planner put them in
	 * a separate list for us.
	 */
	subps = NIL;
	foreach(l, node->initPlan)
	{
		SubPlan    *subplan = (SubPlan *) lfirst(l);
		SubPlanState *sstate;

		Assert(IsA(subplan, SubPlan));

		setSubplanSliceId(subplan, estate);

		sstate = ExecInitSubPlan(subplan, result);
		subps = lappend(subps, sstate);
	}
	if (result != NULL)
		result->initPlan = subps;

	estate->currentSliceIdInPlan = origSliceIdInPlan;
	estate->currentExecutingSliceId = origExecutingSliceId;

	/* Set up instrumentation for this node if requested */
	if (estate->es_instrument && result != NULL)
		result->instrument = InstrAlloc(1);

	if (result != NULL)
	{
		SAVE_EXECUTOR_MEMORY_ACCOUNT(result, curMemoryAccount);
		result->CodegenManager = CodegenManager;
		CodeGeneratorManagerGenerateCode(CodegenManager);
		CodeGeneratorManagerPrepareGeneratedFunctions(CodegenManager);
	}
	}
	END_CODE_GENERATOR_MANAGER();

	return result;
}

/* ----------------------------------------------------------------
 *    EnrollQualList
 *
 *    Enroll Qual List's expr state from PlanState for codegen.
 * ----------------------------------------------------------------
 */
void
EnrollQualList(PlanState* result, TupleTableSlot* slot)
{
#ifdef USE_CODEGEN
	if (NULL == result ||
	    NULL == result->qual)
	{
		return;
	}

	ListCell *l;
	foreach(l, result->qual)
	{
	  ExprState *exprstate = (ExprState*) lfirst(l);
	  enroll_ExecEvalExpr_codegen(exprstate->evalfunc,
	                              &exprstate->evalfunc,
	                              exprstate,
	                              result->ps_ExprContext,
	                              slot);
	}

#endif
}

/* ----------------------------------------------------------------
 *    EnrollProjInfoTargetList
 *
 *    Enroll Targetlist from ProjectionInfo to Codegen
 * ----------------------------------------------------------------
 */
void
EnrollProjInfoTargetList(ProjectionInfo* ProjInfo,
                         TupleTableSlot* slot,
                         bool skip_var)
{
#ifdef USE_CODEGEN
  if (NULL == ProjInfo ||
      NULL == ProjInfo->pi_targetlist)
  {
    return;
  }
  ListCell *l;
  foreach(l, ProjInfo->pi_targetlist)
  {
    GenericExprState *gstate = (GenericExprState *) lfirst(l);
    if (NULL == gstate->arg ||
        NULL == gstate->arg->evalfunc)
    {
      continue;
    }
    if (skip_var &&
        T_Var == nodeTag(gstate->arg->expr))
    {
      continue;
    }
    enroll_ExecEvalExpr_codegen(gstate->arg->evalfunc,
                                &gstate->arg->evalfunc,
                                gstate->arg,
                                ProjInfo->pi_exprContext,
                                slot);

  }
#endif
}


/* ----------------------------------------------------------------
 *		ExecSliceDependencyNode
 *
 *	 	Exec dependency, block till slice dependency are met
 * ----------------------------------------------------------------
 */
void
ExecSliceDependencyNode(PlanState *node)
{
	CHECK_FOR_INTERRUPTS();

	if(node == NULL)
		return;

	if(nodeTag(node) == T_ShareInputScanState)
		ExecSliceDependencyShareInputScan((ShareInputScanState *) node);
	else if(nodeTag(node) == T_SubqueryScanState)
	{
		SubqueryScanState *subq = (SubqueryScanState *) node;
		ExecSliceDependencyNode(subq->subplan);
	}
	else if(nodeTag(node) == T_AppendState)
	{
		int i=0;
		AppendState *app = (AppendState *) node;

		for(; i<app->as_nplans; ++i)
			ExecSliceDependencyNode(app->appendplans[i]);
	}
	else if(nodeTag(node) == T_SequenceState)
	{
		int i=0;
		SequenceState *ss = (SequenceState *) node;

		for(; i<ss->numSubplans; ++i)
			ExecSliceDependencyNode(ss->subplans[i]);
	}

	ExecSliceDependencyNode(outerPlanState(node));
	ExecSliceDependencyNode(innerPlanState(node));
}
    
/* ----------------------------------------------------------------
 *		ExecProcNode
 *
 *		Execute the given node to return a(nother) tuple.
 * ----------------------------------------------------------------
 */
TupleTableSlot *
ExecProcNode(PlanState *node)
{
	TupleTableSlot *result = NULL;

	START_CODE_GENERATOR_MANAGER(node->CodegenManager);
	{
	START_MEMORY_ACCOUNT(node->memoryAccount);
	{

#ifndef WIN32
	static void *ExecJmpTbl[] = {
		&&Exec_Jmp_Result,
		&&Exec_Jmp_Append,
		&&Exec_Jmp_Sequence,
		&&Exec_Jmp_BitmapAnd,
		&&Exec_Jmp_BitmapOr,
		&&Exec_Jmp_TableScan,
		&&Exec_Jmp_TableScan,
		&&Exec_Jmp_TableScan,
		&&Exec_Jmp_TableScan,
		&&Exec_Jmp_DynamicTableScan,
		&&Exec_Jmp_ExternalScan,
		&&Exec_Jmp_IndexScan,
		&&Exec_Jmp_DynamicIndexScan,
		&&Exec_Jmp_BitmapIndexScan,
		&&Exec_Jmp_BitmapHeapScan,
		&&Exec_Jmp_BitmapAppendOnlyScan,
		&&Exec_Jmp_BitmapTableScan,
		&&Exec_Jmp_TidScan,
		&&Exec_Jmp_SubqueryScan,
		&&Exec_Jmp_FunctionScan,
		&&Exec_Jmp_TableFunctionScan,
		&&Exec_Jmp_ValuesScan,
		&&Exec_Jmp_NestLoop,
		&&Exec_Jmp_MergeJoin,
		&&Exec_Jmp_HashJoin,
		&&Exec_Jmp_Material,
		&&Exec_Jmp_Sort,
		&&Exec_Jmp_Agg,
		&&Exec_Jmp_Unique,
		&&Exec_Jmp_Hash,
		&&Exec_Jmp_SetOp,
		&&Exec_Jmp_Limit,
		&&Exec_Jmp_Motion,
		&&Exec_Jmp_ShareInputScan,
		&&Exec_Jmp_Window,
		&&Exec_Jmp_Repeat,
		&&Exec_Jmp_DML,
		&&Exec_Jmp_SplitUpdate,
		&&Exec_Jmp_RowTrigger,
		&&Exec_Jmp_AssertOp,
		&&Exec_Jmp_PartitionSelector
	};

	COMPILE_ASSERT((T_Plan_End - T_Plan_Start) == (T_PlanState_End - T_PlanState_Start));
	COMPILE_ASSERT(ARRAY_SIZE(ExecJmpTbl) == (T_PlanState_End - T_PlanState_Start));

	CHECK_FOR_INTERRUPTS();

	/*
	 * Even if we are requested to finish query, Motion has to do its work
	 * to tell End of Stream message to upper slice.  He will probably get
	 * NULL tuple from underlying operator by calling another ExecProcNode,
	 * so one additional operator execution should not be a big hit.
	 */
	if (QueryFinishPending && !IsA(node, MotionState))
		return NULL;

#ifdef CDB_TRACE_EXECUTOR
	ExecCdbTraceNode(node, true, NULL);
#endif   /* CDB_TRACE_EXECUTOR */

	if(node->plan)
		PG_TRACE5(execprocnode__enter, Gp_segment, currentSliceId, nodeTag(node), node->plan->plan_node_id, node->plan->plan_parent_node_id);

	if (node->chgParam != NULL) /* something changed */
		ExecReScan(node, NULL); /* let ReScan handle this */

	if (node->instrument)
		InstrStartNode(node->instrument);

	if(!node->fHadSentGpmon)
		CheckSendPlanStateGpmonPkt(node);

	Assert(nodeTag(node) >= T_PlanState_Start && nodeTag(node) < T_PlanState_End);
	goto *ExecJmpTbl[nodeTag(node) - T_PlanState_Start];

Exec_Jmp_Result:
	result = ExecResult((ResultState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_Append:
	result = ExecAppend((AppendState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_Sequence:
	result = ExecSequence((SequenceState *) node);
	goto Exec_Jmp_Done;

	/* These two does not yield tuple */
Exec_Jmp_BitmapAnd:
Exec_Jmp_BitmapOr:
	goto Exec_Jmp_Done;

Exec_Jmp_TableScan:
	result = ExecTableScan((TableScanState *)node);
	goto Exec_Jmp_Done;

Exec_Jmp_DynamicTableScan:
	result = ExecDynamicTableScan((DynamicTableScanState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_ExternalScan:
	result = ExecExternalScan((ExternalScanState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_IndexScan:
	result = ExecIndexScan((IndexScanState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_DynamicIndexScan:
	result = ExecDynamicIndexScan((DynamicIndexScanState *) node);
	goto Exec_Jmp_Done;
	/* BitmapIndexScanState does not yield tuples */
Exec_Jmp_BitmapIndexScan:
	goto Exec_Jmp_Done;

Exec_Jmp_BitmapHeapScan:
	result = ExecBitmapHeapScan((BitmapHeapScanState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_BitmapAppendOnlyScan:
	result = ExecBitmapAppendOnlyScan((BitmapAppendOnlyScanState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_BitmapTableScan:
	result = ExecBitmapTableScan((BitmapTableScanState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_TidScan:
	result = ExecTidScan((TidScanState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_SubqueryScan:
	result = ExecSubqueryScan((SubqueryScanState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_FunctionScan:
	result = ExecFunctionScan((FunctionScanState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_TableFunctionScan:
	result = ExecTableFunction((TableFunctionState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_ValuesScan:
	result = ExecValuesScan((ValuesScanState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_NestLoop:
	result = ExecNestLoop((NestLoopState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_MergeJoin:
	result = ExecMergeJoin((MergeJoinState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_HashJoin:
	result = ExecHashJoin((HashJoinState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_Material:
	result = ExecMaterial((MaterialState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_Sort:
	result = ExecSort((SortState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_Agg:
	result = ExecAgg((AggState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_Unique:
	result = ExecUnique((UniqueState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_Hash:
	result = ExecHash((HashState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_SetOp:
	result = ExecSetOp((SetOpState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_Limit:
	result = ExecLimit((LimitState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_Motion:
	result = ExecMotion((MotionState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_ShareInputScan:
	result = ExecShareInputScan((ShareInputScanState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_Window:
	result = ExecWindow((WindowState *) node);
	goto Exec_Jmp_Done;
Exec_Jmp_Repeat:
	result = ExecRepeat((RepeatState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_DML:
	result = ExecDML((DMLState *) node);
	goto Exec_Jmp_Done;
	
Exec_Jmp_SplitUpdate:
	result = ExecSplitUpdate((SplitUpdateState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_RowTrigger:
	result = ExecRowTrigger((RowTriggerState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_AssertOp:
	result = ExecAssertOp((AssertOpState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_PartitionSelector:
	result = ExecPartitionSelector((PartitionSelectorState *) node);
	goto Exec_Jmp_Done;

Exec_Jmp_Done:
	if (node->instrument)
		InstrStopNode(node->instrument, TupIsNull(result) ? 0.0 : 1.0);

	if(node->plan)
		PG_TRACE5(execprocnode__exit, Gp_segment, currentSliceId, nodeTag(node), node->plan->plan_node_id, node->plan->plan_parent_node_id);
#else

	CHECK_FOR_INTERRUPTS();

	if (QueryFinishPending && !IsA(node, MotionState))
		return NULL;

#ifdef CDB_TRACE_EXECUTOR
	ExecCdbTraceNode(node, true, NULL);
#endif   /* CDB_TRACE_EXECUTOR */

	if (node->chgParam != NULL) /* something changed */
		ExecReScan(node, NULL); /* let ReScan handle this */

	if (node->instrument)
		InstrStartNode(node->instrument);

	switch (nodeTag(node))
	{
			/*
			 * control nodes
			 */
		case T_ResultState:
			result = ExecResult((ResultState *) node);
			break;

		case T_AppendState:
			result = ExecAppend((AppendState *) node);
			break;

			/* BitmapAndState does not yield tuples */

			/* BitmapOrState does not yield tuples */

			/*
			 * scan nodes
			 */
		case T_SeqScanState:
		case T_AppendOnlyScanState:
		case T_AOCSScanState:
			insist_log(false, "SeqScan/AppendOnlyScan/AOCSScan are defunct");
			break;

		case T_IndexScanState:
			result = ExecIndexScan((IndexScanState *) node);
			break;

		case T_ExternalScanState:
			result = ExecExternalScan((ExternalScanState *) node);
			break;
			
			/* BitmapIndexScanState does not yield tuples */

		case T_BitmapHeapScanState:
			result = ExecBitmapHeapScan((BitmapHeapScanState *) node);
			break;

		case T_TidScanState:
			result = ExecTidScan((TidScanState *) node);
			break;

		case T_SubqueryScanState:
			result = ExecSubqueryScan((SubqueryScanState *) node);
			break;

		case T_FunctionScanState:
			result = ExecFunctionScan((FunctionScanState *) node);
			break;

		case T_TableFunctionState:
			result = ExecTableFunction((TableFunctionState *) node);
			break;

		case T_ValuesScanState:
			result = ExecValuesScan((ValuesScanState *) node);
			break;

		case T_BitmapAppendOnlyScanState:
			result = ExecBitmapAppendOnlyScan((BitmapAppendOnlyScanState *) node);
			break;
			
			/*
			 * join nodes
			 */
		case T_NestLoopState:
			result = ExecNestLoop((NestLoopState *) node);
			break;

		case T_MergeJoinState:
			result = ExecMergeJoin((MergeJoinState *) node);
			break;

		case T_HashJoinState:
			result = ExecHashJoin((HashJoinState *) node);
			break;

			/*
			 * shareinput nodes
			 */
		case T_ShareInputScanState:
			result = ExecShareInputScan((ShareInputScanState *) node);
			break;

			/*
			 * materialization nodes
			 */
		case T_MaterialState:
			result = ExecMaterial((MaterialState *) node);
			break;

		case T_SortState:
			result = ExecSort((SortState *) node);
			break;

		case T_GroupState:
			result = ExecGroup((GroupState *) node);
			break;

		case T_AggState:
			result = ExecAgg((AggState *) node);
			break;

		case T_WindowState:
			result = ExecWindow((WindowState *) node);
			break;

		case T_UniqueState:
			result = ExecUnique((UniqueState *) node);
			break;

		case T_HashState:
			result = ExecHash((HashState *) node);
			break;

		case T_SetOpState:
			result = ExecSetOp((SetOpState *) node);
			break;

		case T_LimitState:
			result = ExecLimit((LimitState *) node);
			break;

		case T_MotionState:
			result = ExecMotion((MotionState *) node);
			break;

		default:
			elog(ERROR, "unrecognized node type: %d", (int) nodeTag(node));
			result = NULL;
			break;
	}

	if (node->instrument)
		InstrStopNode(node->instrument, TupIsNull(result) ? 0.0 : 1.0);
#endif 
#ifdef CDB_TRACE_EXECUTOR
	ExecCdbTraceNode(node, false, result);
#endif   /* CDB_TRACE_EXECUTOR */

	/*
	 * Eager free and squelch the subplans, unless it's a nested subplan.
	 * In that case we cannot free or squelch, because it will be re-executed.
	 */
	if (TupIsNull(result))
	{
		ListCell *subp;
		foreach(subp, node->subPlan)
		{
			SubPlanState *subplanState = (SubPlanState *)lfirst(subp);
			Assert(subplanState != NULL &&
				   subplanState->planstate != NULL);

			bool subplanAtTopNestLevel = (node->state->currentSubplanLevel == 0);

			if (subplanAtTopNestLevel)
			{
				ExecSquelchNode(subplanState->planstate);
				ExecEagerFreeChildNodes(subplanState->planstate, subplanAtTopNestLevel);
				ExecEagerFree(subplanState->planstate);
			}
		}
	}

	}
	END_MEMORY_ACCOUNT();
	}
	END_CODE_GENERATOR_MANAGER();
	return result;
}


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

	CHECK_FOR_INTERRUPTS();

	Assert(NULL != node->plan);

	START_MEMORY_ACCOUNT(node->memoryAccount);
	{
		PG_TRACE5(execprocnode__enter, Gp_segment, currentSliceId, nodeTag(node), node->plan->plan_node_id, node->plan->plan_parent_node_id);

		if (node->chgParam != NULL) /* something changed */
			ExecReScan(node, NULL); /* let ReScan handle this */

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

		PG_TRACE5(execprocnode__exit, Gp_segment, currentSliceId, nodeTag(node), node->plan->plan_node_id, node->plan->plan_parent_node_id);
	}
	END_MEMORY_ACCOUNT();
	return result;
}


/*
 * ExecCountSlotsNode - count up the number of tuple table slots needed
 *
 * Note that this scans a Plan tree, not a PlanState tree, because we
 * haven't built the PlanState tree yet ...
 */
int
ExecCountSlotsNode(Plan *node)
{
	if (node == NULL)
		return 0;

	switch (nodeTag(node))
	{
			/*
			 * control nodes
			 */
		case T_Result:
			return ExecCountSlotsResult((Result *) node);

		case T_Append:
			return ExecCountSlotsAppend((Append *) node);

		case T_Sequence:
			return ExecCountSlotsSequence((Sequence *) node);

		case T_BitmapAnd:
			return ExecCountSlotsBitmapAnd((BitmapAnd *) node);

		case T_BitmapOr:
			return ExecCountSlotsBitmapOr((BitmapOr *) node);

			/*
			 * scan nodes
			 */
		case T_SeqScan:
		case T_AppendOnlyScan:
		case T_AOCSScan:
		case T_TableScan:
			return ExecCountSlotsTableScan((TableScan *) node);

		case T_DynamicTableScan:
			return ExecCountSlotsDynamicTableScan((DynamicTableScan *) node);

		case T_ExternalScan:
			return ExecCountSlotsExternalScan((ExternalScan *) node);

		case T_IndexScan:
			return ExecCountSlotsIndexScan((IndexScan *) node);

		case T_DynamicIndexScan:
			return ExecCountSlotsDynamicIndexScan((DynamicIndexScan *) node);

		case T_BitmapIndexScan:
			return ExecCountSlotsBitmapIndexScan((BitmapIndexScan *) node);

		case T_BitmapHeapScan:
			return ExecCountSlotsBitmapHeapScan((BitmapHeapScan *) node);

		case T_BitmapAppendOnlyScan:
			return ExecCountSlotsBitmapAppendOnlyScan((BitmapAppendOnlyScan*) node);
			
		case T_BitmapTableScan:
			return ExecCountSlotsBitmapTableScan((BitmapTableScan *) node);

		case T_TidScan:
			return ExecCountSlotsTidScan((TidScan *) node);

		case T_SubqueryScan:
			return ExecCountSlotsSubqueryScan((SubqueryScan *) node);

		case T_FunctionScan:
			return ExecCountSlotsFunctionScan((FunctionScan *) node);

		case T_TableFunctionScan:
			return ExecCountSlotsTableFunction((TableFunctionScan *) node);

		case T_ValuesScan:
			return ExecCountSlotsValuesScan((ValuesScan *) node);

			/*
			 * join nodes
			 */
		case T_NestLoop:
			return ExecCountSlotsNestLoop((NestLoop *) node);

		case T_MergeJoin:
			return ExecCountSlotsMergeJoin((MergeJoin *) node);

		case T_HashJoin:
			return ExecCountSlotsHashJoin((HashJoin *) node);

		/*
		 * share input nodes
		 */
		case T_ShareInputScan:
			return ExecCountSlotsShareInputScan((ShareInputScan *) node);

			/*
			 * materialization nodes
			 */
		case T_Material:
			return ExecCountSlotsMaterial((Material *) node);

		case T_Sort:
			return ExecCountSlotsSort((Sort *) node);

		case T_Agg:
			return ExecCountSlotsAgg((Agg *) node);

		case T_Window:
			return ExecCountSlotsWindow((Window *) node);

		case T_Unique:
			return ExecCountSlotsUnique((Unique *) node);

		case T_Hash:
			return ExecCountSlotsHash((Hash *) node);

		case T_SetOp:
			return ExecCountSlotsSetOp((SetOp *) node);

		case T_Limit:
			return ExecCountSlotsLimit((Limit *) node);

		case T_Motion:
			return ExecCountSlotsMotion((Motion *) node);

		case T_Repeat:
			return ExecCountSlotsRepeat((Repeat *) node);

		case T_DML:
			return ExecCountSlotsDML((DML *) node);

		case T_SplitUpdate:
			return ExecCountSlotsSplitUpdate((SplitUpdate *) node);

		case T_AssertOp:
 			return ExecCountSlotsAssertOp((AssertOp *) node);

		case T_RowTrigger:
 			return ExecCountSlotsRowTrigger((RowTrigger *) node);

		case T_PartitionSelector:
			return ExecCountSlotsPartitionSelector((PartitionSelector *) node);

		default:
			elog(ERROR, "unrecognized node type: %d", (int) nodeTag(node));
			break;
	}

	return 0;
}

/* ----------------------------------------------------------------
 *		ExecSquelchNode
 *
 *		When a node decides that it will not consume any more
 *		input tuples from a subtree that has not yet returned
 *		end-of-data, it must call ExecSquelchNode() on the subtree.
 * ----------------------------------------------------------------
 */

static CdbVisitOpt
squelchNodeWalker(PlanState *node,
				  void *context)
{
	if (IsA(node, MotionState))
	{
		ExecStopMotion((MotionState *) node);
		return CdbVisit_Skip;	/* don't visit subtree */
	}
	else if (IsA(node, ExternalScanState))
	{
		ExecStopExternalScan((ExternalScanState *) node);
		/* ExternalScan nodes are expected to be leaf nodes (without subplans) */
	}

	return CdbVisit_Walk;
}	/* squelchNodeWalker */


void
ExecSquelchNode(PlanState *node)
{
	/*
	 * If parameters have changed, then node can be part of subquery execution.
	 * In this case we cannot squelch node, otherwise next subquery invocations
	 * will receive no tuples from lower motion nodes (MPP-13921).
	 */
	if (node->chgParam == NULL)
	{
		planstate_walk_node_extended(node, squelchNodeWalker, NULL, PSW_IGNORE_INITPLAN);
	}
}	                            /* ExecSquelchNode */


static CdbVisitOpt
transportUpdateNodeWalker(PlanState *node, void *context)
{

	/* For motion nodes, we just transfer the context information established during SetupInterconnect */
	if (IsA(node, MotionState))
	{
		((MotionState *)node)->ps.state->interconnect_context = (ChunkTransportState *)context;
		/* visit subtree */
	}

	return CdbVisit_Walk;
}	/* transportUpdateNodeWalker */

void
ExecUpdateTransportState(PlanState *node, ChunkTransportState *state)
{
	Assert(node);
	Assert(state);
	planstate_walk_node(node, transportUpdateNodeWalker, state);
}	                            /* ExecUpdateTransportState */


/* ----------------------------------------------------------------
 *		ExecEndNode
 *
 *		Recursively cleans up all the nodes in the plan rooted
 *		at 'node'.
 *
 *		After this operation, the query plan will not be able to
 *		processed any further.	This should be called only after
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

	EState *estate = node->state;
	Assert(estate != NULL);
	int origSliceIdInPlan = estate->currentSliceIdInPlan;
	int origExecutingSliceId = estate->currentExecutingSliceId;

	estate->currentSliceIdInPlan = origSliceIdInPlan;
	estate->currentExecutingSliceId = origExecutingSliceId;

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

		case T_AppendState:
			ExecEndAppend((AppendState *) node);
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
		case T_AppendOnlyScanState:
		case T_AOCSScanState:
			insist_log(false, "SeqScan/AppendOnlyScan/AOCSScan are defunct");
			break;
			
		case T_TableScanState:
			ExecEndTableScan((TableScanState *) node);
			break;
			
		case T_DynamicTableScanState:
			ExecEndDynamicTableScan((DynamicTableScanState *) node);
			break;

		case T_IndexScanState:
			ExecEndIndexScan((IndexScanState *) node);
			break;

		case T_DynamicIndexScanState:
			ExecEndDynamicIndexScan((DynamicIndexScanState *) node);
			break;

		case T_ExternalScanState:
			ExecEndExternalScan((ExternalScanState *) node);
			break;

		case T_BitmapIndexScanState:
			ExecEndBitmapIndexScan((BitmapIndexScanState *) node);
			break;

		case T_BitmapHeapScanState:
			ExecEndBitmapHeapScan((BitmapHeapScanState *) node);
			break;

		case T_BitmapAppendOnlyScanState:
			ExecEndBitmapAppendOnlyScan((BitmapAppendOnlyScanState *) node);
			break;

		case T_BitmapTableScanState:
			ExecEndBitmapTableScan((BitmapTableScanState *) node);
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

		case T_ValuesScanState:
			ExecEndValuesScan((ValuesScanState *) node);
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

		case T_WindowState:
			ExecEndWindow((WindowState *) node);
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

		case T_LimitState:
			ExecEndLimit((LimitState *) node);
			break;

		case T_MotionState:
			ExecEndMotion((MotionState *) node);
			break;

		case T_RepeatState:
			ExecEndRepeat((RepeatState *) node);
			break;
			/*
			 * DML nodes
			 */
		case T_DMLState:
			ExecEndDML((DMLState *) node);
			break;
		case T_SplitUpdateState:
			ExecEndSplitUpdate((SplitUpdateState *) node);
			break;
		case T_AssertOpState:
 			ExecEndAssertOp((AssertOpState *) node);
 			break;
		case T_RowTriggerState:
 			ExecEndRowTrigger((RowTriggerState *) node);
 			break;
		case T_PartitionSelectorState:
			ExecEndPartitionSelector((PartitionSelectorState *) node);
			break;

		default:
			elog(ERROR, "unrecognized node type: %d", (int) nodeTag(node));
			break;
	}

	/*
	 * if codegen guc is true, then assert if CodegenManager is NULL
	 */
	AssertImply(codegen, NULL != node->CodegenManager);
	CodeGeneratorManagerDestroy(node->CodegenManager);
	node->CodegenManager = NULL;

	estate->currentSliceIdInPlan = origSliceIdInPlan;
	estate->currentExecutingSliceId = origExecutingSliceId;
}


#ifdef CDB_TRACE_EXECUTOR
/* ----------------------------------------------------------------
 *	ExecCdbTraceNode
 *
 *	Trace entry and exit from ExecProcNode on an executor node.
 * ----------------------------------------------------------------
 */
void
ExecCdbTraceNode(PlanState *node, bool entry, TupleTableSlot *result)
{
	bool		willReScan = FALSE;
	bool		willReturnTuple = FALSE;
	Plan	   *plan = NULL;
	const char *nameTag = NULL;
	const char *extraTag = "";
	char		extraTagBuffer[20];

	/*
	 * Don't trace NULL nodes..
	 */
	if (node == NULL)
		return;

	plan = node->plan;
	Assert(plan != NULL);
	Assert(result == NULL || !entry);
	willReScan = (entry && node->chgParam != NULL);
	willReturnTuple = (!entry && !TupIsNull(result));

	switch (nodeTag(node))
	{
			/*
			 * control nodes
			 */
		case T_ResultState:
			nameTag = "Result";
			break;

		case T_AppendState:
			nameTag = "Append";
			break;

		case T_SequenceState:
			nameTag = "Sequence";
			break;

			/*
			 * scan nodes
			 */
		case T_SeqScanState:
			nameTag = "SeqScan";
			break;

		case T_TableScanState:
			nameTag = "TableScan";
			break;

		case T_DynamicTableScanState:
			nameTag = "DynamicTableScan";
			break;

		case T_IndexScanState:
			nameTag = "IndexScan";
			break;

		case T_BitmapIndexScanState:
			nameTag = "BitmapIndexScan";
			break;

		case T_BitmapHeapScanState:
			nameTag = "BitmapHeapScan";
			break;

		case T_BitmapAppendOnlyScanState:
			nameTag = "BitmapAppendOnlyScan";
			break;

		case T_TidScanState:
			nameTag = "TidScan";
			break;

		case T_SubqueryScanState:
			nameTag = "SubqueryScan";
			break;

		case T_FunctionScanState:
			nameTag = "FunctionScan";
			break;

		case T_TableFunctionState:
			nameTag = "TableFunctionScan";
			break;

		case T_ValuesScanState:
			nameTag = "ValuesScan";
			break;
			
			/*
			 * join nodes
			 */
		case T_NestLoopState:
			nameTag = "NestLoop";
			break;

		case T_MergeJoinState:
			nameTag = "MergeJoin";
			break;

		case T_HashJoinState:
			nameTag = "HashJoin";
			break;

			/*
			 * share inpt nodess
			 */
		case T_ShareInputScanState:
			nameTag = "ShareInputScan";
			break;

			/*
			 * materialization nodes
			 */
		case T_MaterialState:
			nameTag = "Material";
			break;

		case T_SortState:
			nameTag = "Sort";
			break;

		case T_GroupState:
			nameTag = "Group";
			break;

		case T_AggState:
			nameTag = "Agg";
			break;

		case T_WindowState:
			nameTag = "Window";
			break;

		case T_UniqueState:
			nameTag = "Unique";
			break;

		case T_HashState:
			nameTag = "Hash";
			break;

		case T_SetOpState:
			nameTag = "SetOp";
			break;

		case T_LimitState:
			nameTag = "Limit";
			break;

		case T_MotionState:
			nameTag = "Motion";
			{
				snprintf(extraTagBuffer, sizeof extraTagBuffer, " %d", ((Motion *) plan)->motionID);
				extraTag = &extraTagBuffer[0];
			}
			break;

		case T_RepeatState:
			nameTag = "Repeat";
			break;
			/*
			 * DML nodes
			 */
		case T_DMLState:
			ExecEndDML((DMLState *) node);
			break;
		case T_SplitUpdateState:
			nameTag = "SplitUpdate";
			break;
		case T_AssertOp:
 			nameTag = "AssertOp";
 			break;
 		case T_RowTriggerState:
 			nameTag = "RowTrigger";
 			break;
		default:
			nameTag = "*unknown*";
			break;
	}

	if (entry)
	{
		elog(DEBUG4, "CDB_TRACE_EXECUTOR: Exec %s%s%s", nameTag, extraTag, willReScan ? " (will ReScan)." : ".");
	}
	else
	{
		elog(DEBUG4, "CDB_TRACE_EXECUTOR: Return from %s%s with %s tuple.", nameTag, extraTag, willReturnTuple ? "a" : "no");
		if ( willReturnTuple )
			print_slot(result);
	}

	return;
}
#endif   /* CDB_TRACE_EXECUTOR */


/* -----------------------------------------------------------------------
 *                      PlanState Tree Walking Functions
 * -----------------------------------------------------------------------
 *
 * planstate_walk_node
 *    Calls a 'walker' function for the given PlanState node; or returns
 *    CdbVisit_Walk if 'planstate' is NULL.
 *
 *    If 'walker' returns CdbVisit_Walk, then this function calls
 *    planstate_walk_kids() to visit the node's children, and returns
 *    the result.
 *
 *    If 'walker' returns CdbVisit_Skip, then this function immediately
 *    returns CdbVisit_Walk and does not visit the node's children.
 *
 *    If 'walker' returns CdbVisit_Stop or another value, then this function
 *    immediately returns that value and does not visit the node's children.
 *
 * planstate_walk_array
 *    Calls planstate_walk_node() for each non-NULL PlanState ptr in
 *    the given array of pointers to PlanState objects.
 *
 *    Quits if the result of planstate_walk_node() is CdbVisit_Stop or another
 *    value other than CdbVisit_Walk, and returns that result without visiting
 *    any more nodes.
 *
 *    Returns CdbVisit_Walk if 'planstates' is NULL, or if all of the
 *    subtrees return CdbVisit_Walk.
 *
 *    Note that this function never returns CdbVisit_Skip to its caller.
 *    Only the caller's 'walker' function can return CdbVisit_Skip.
 *
 * planstate_walk_list
 *    Calls planstate_walk_node() for each PlanState node in the given List.
 *
 *    Quits if the result of planstate_walk_node() is CdbVisit_Stop or another
 *    value other than CdbVisit_Walk, and returns that result without visiting
 *    any more nodes.
 *
 *    Returns CdbVisit_Walk if all of the subtrees return CdbVisit_Walk, or
 *    if the list is empty.
 *
 *    Note that this function never returns CdbVisit_Skip to its caller.
 *    Only the caller's 'walker' function can return CdbVisit_Skip.
 *
 * planstate_walk_kids
 *    Calls planstate_walk_node() for each child of the given PlanState node.
 *
 *    Quits if the result of planstate_walk_node() is CdbVisit_Stop or another
 *    value other than CdbVisit_Walk, and returns that result without visiting
 *    any more nodes.
 *
 *    Returns CdbVisit_Walk if the given planstate node ptr is NULL, or if
 *    all of the children return CdbVisit_Walk, or if there are no children.
 *
 *    Note that this function never returns CdbVisit_Skip to its caller.
 *    Only the 'walker' can return CdbVisit_Skip.
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
planstate_walk_node(PlanState      *planstate,
			        CdbVisitOpt   (*walker)(PlanState *planstate, void *context),
			        void           *context)
{
	return planstate_walk_node_extended(planstate, walker, context, 0);
}

/**
 * Workhorse walker that uses flags.
 */
CdbVisitOpt
planstate_walk_node_extended(PlanState      *planstate,
			        CdbVisitOpt   (*walker)(PlanState *planstate, void *context),
			        void           *context,
			        int flags)
{
    CdbVisitOpt     whatnext;

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
}	                            /* planstate_walk_node */

CdbVisitOpt
planstate_walk_array(PlanState    **planstates,
                     int            nplanstate,
			         CdbVisitOpt  (*walker)(PlanState *planstate, void *context),
			         void          *context,
			         int flags)
{
    CdbVisitOpt     whatnext = CdbVisit_Walk;
    int             i;

    if (planstates == NULL)
        return CdbVisit_Walk;

    for (i = 0; i < nplanstate && whatnext == CdbVisit_Walk; i++)
        whatnext = planstate_walk_node_extended(planstates[i], walker, context, flags);

    return whatnext;
}	                            /* planstate_walk_array */

CdbVisitOpt
planstate_walk_kids(PlanState      *planstate,
			        CdbVisitOpt   (*walker)(PlanState *planstate, void *context),
			        void           *context,
			        int flags)
{
    CdbVisitOpt v;

    if (planstate == NULL)
        return CdbVisit_Walk;

	switch (nodeTag(planstate))
	{
        case T_NestLoopState:
        {
            NestLoopState  *nls = (NestLoopState *)planstate;

            /* Don't visit left subtree of NJ if it is shared with brother HJ */
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
			AppendState *as = (AppendState *)planstate;

            v = planstate_walk_array(as->appendplans, as->as_nplans, walker, context, flags);
            Assert(!planstate->lefttree && !planstate->righttree);
			break;
		}

        case T_SequenceState:
		{
			SequenceState *ss = (SequenceState *)planstate;

            v = planstate_walk_array(ss->subplans, ss->numSubplans, walker, context, flags);
            Assert(!planstate->lefttree && !planstate->righttree);
			break;
		}
  
        case T_BitmapAndState:
        {
            BitmapAndState *bas = (BitmapAndState *)planstate;

            v = planstate_walk_array(bas->bitmapplans, bas->nplans, walker, context, flags);
            Assert(!planstate->lefttree && !planstate->righttree);
			break;
        }
        case T_BitmapOrState:
        {
            BitmapOrState  *bos = (BitmapOrState *)planstate;

            v = planstate_walk_array(bos->bitmapplans, bos->nplans, walker, context, flags);
            Assert(!planstate->lefttree && !planstate->righttree);
			break;
        }

        case T_SubqueryScanState:
            v = planstate_walk_node_extended(((SubqueryScanState *)planstate)->subplan, walker, context, flags);
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
		ListCell *lc = NULL;
		CdbVisitOpt v1 = v;
		foreach (lc, planstate->initPlan)
		{
			SubPlanState *sps = (SubPlanState *) lfirst(lc);
			PlanState *ips = sps->planstate;
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
		ListCell *lc = NULL;
		CdbVisitOpt v1 = v;
		foreach (lc, planstate->subPlan)
		{
			SubPlanState *sps = (SubPlanState *) lfirst(lc);
			PlanState *ips = sps->planstate;
			Assert(ips);
			if (v1 == CdbVisit_Walk)
			{
				v1 = planstate_walk_node_extended(ips, walker, context, flags);
			}
		}

	}

    return v;
}	                            /* planstate_walk_kids */
