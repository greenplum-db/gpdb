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
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/execProcnode.c
 *
 *-------------------------------------------------------------------------
 */
/*
 *	 INTERFACE ROUTINES
 *		ExecInitNode	-		initialize a plan node and its subplans
 *		ExecProcNode	-		get a tuple by executing the plan node
 *		ExecEndNode		-		shut down a plan node and its subplans
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
 *		their work to the appopriate node support routines which may
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
#include "executor/nodeForeignscan.h"
#include "executor/nodeFunctionscan.h"
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
#include "executor/nodeNestloop.h"
#include "executor/nodeRecursiveunion.h"
#include "executor/nodeResult.h"
#include "executor/nodeSeqscan.h"
#include "executor/nodeSetOp.h"
#include "executor/nodeSort.h"
#include "executor/nodeSubplan.h"
#include "executor/nodeSubqueryscan.h"
#include "executor/nodeTidscan.h"
#include "executor/nodeUnique.h"
#include "executor/nodeValuesscan.h"
#include "executor/nodeWindowAgg.h"
#include "executor/nodeWorktablescan.h"
#include "miscadmin.h"

#include "cdb/cdbvars.h"
#include "cdb/ml_ipc.h"			/* interconnect context */
#include "executor/nodeAssertOp.h"
#include "executor/nodeDML.h"
#include "executor/nodeDynamicIndexscan.h"
#include "executor/nodeDynamicSeqscan.h"
#include "executor/nodeExternalscan.h"
#include "executor/nodeMotion.h"
#include "executor/nodePartitionSelector.h"
#include "executor/nodeRepeat.h"
#include "executor/nodeRowTrigger.h"
#include "executor/nodeSequence.h"
#include "executor/nodeShareInputScan.h"
#include "executor/nodeSplitUpdate.h"
#include "executor/nodeTableFunction.h"
#include "pg_trace.h"
#include "tcop/tcopprot.h"
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

/*
 * saveExecutorMemoryAccount saves an operator specific memory account
 * into the PlanState of that operator
 */
static inline void
saveExecutorMemoryAccount(PlanState *execState,
						  MemoryAccountIdType curMemoryAccountId)
{
	Assert(curMemoryAccountId != MEMORY_OWNER_TYPE_Undefined);
	Assert(MEMORY_OWNER_TYPE_Undefined == execState->memoryAccountId);
	execState->memoryAccountId = curMemoryAccountId;
}


/*
 * CREATE_EXECUTOR_MEMORY_ACCOUNT is a convenience macro to create a new
 * operator specific memory account *if* the operator will be executed in
 * the current slice, i.e., it is not part of some other slice (alien
 * plan node). We assign a shared AlienExecutorMemoryAccount for plan nodes
 * that will not be executed in current slice
 *
 * If the operator is a child of an 'X_NestedExecutor' account, the operator
 * is also assigned to the 'X_NestedExecutor' account, unless the
 * explain_memory_verbosity guc is set to 'debug' or above.
 */
#define CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, planNode, NodeType)    \
	MemoryAccounting_CreateExecutorAccountWithType(                            \
		(isAlienPlanNode), (planNode), MEMORY_OWNER_TYPE_Exec_##NodeType)

static inline MemoryAccountIdType
MemoryAccounting_CreateExecutorAccountWithType(bool isAlienPlanNode,
											   Plan *node,
											   MemoryOwnerType ownerType)
{
	if (isAlienPlanNode)
		return MEMORY_OWNER_TYPE_Exec_AlienShared;
	else
		if (MemoryAccounting_IsUnderNestedExecutor() &&
			explain_memory_verbosity < EXPLAIN_MEMORY_VERBOSITY_DEBUG)
			return MemoryAccounting_GetOrCreateNestedExecutorAccount();
		else
		{
			long maxLimit =
				node->operatorMemKB == 0 ? work_mem : node->operatorMemKB;
			return MemoryAccounting_CreateAccount(maxLimit, ownerType);
		}
}

/*
 * setSubplanSliceId
 *	 Set the slice id info for the given subplan.
 */
static void
setSubplanSliceId(SubPlan *subplan, EState *estate)
{
	Assert(subplan != NULL && IsA(subplan, SubPlan) &&estate != NULL);

	estate->currentSliceIdInPlan = subplan->qDispSliceId;

	/*
	 * The slice that the initPlan will be running is the same as the root
	 * slice. Depending on the location of InitPlan in the plan, the root
	 * slice is the root slice of the whole plan, or the root slice of the
	 * parent subplan of this InitPlan.
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
	int			origSliceIdInPlan = estate->currentSliceIdInPlan;
	int			origExecutingSliceId = estate->currentExecutingSliceId;

	MemoryAccountIdType curMemoryAccountId;


	int localMotionId = LocallyExecutingSliceIndex(estate);

	/*
	 * For most plan nodes the ascendant motion is the parent motion
	 * node. However, subplans are different. They can be executed under
	 * different slices, although appearing in another slice. Other
	 * exception includes two stage agg where agg node on the master
	 * does not have any parent motion. Any time we see such null parent
	 * motion, we assume they are not alien. They either assume "citizen"
	 * status under a subplan, or they are the root of the execution on
	 * the master.
	 */
	Motion *parentMotion = (Motion *) node->motionNode;
	int parentMotionId = parentMotion != NULL ? parentMotion->motionID : UNSET_SLICE_ID;

	/*
	 * Is current plan node supposed to execute in current slice?
	 * Special case is sending motion node, which may be at the root
	 * and therefore parentless. We can sending motions motionId to
	 * determine its alien status.
	 *
	 * On master we don't do alien elimination because of EXPLAIN ANALYZE
	 * gathering stats from all slices.
	 */
	bool isAlienPlanNode = !((localMotionId == parentMotionId) || (parentMotionId == UNSET_SLICE_ID) ||
							 (nodeTag(node) == T_Motion && ((Motion*)node)->motionID == localMotionId)) && (Gp_role == GP_ROLE_EXECUTE);

	/* We cannot have alien nodes if we are eliminating aliens */
	AssertImply(estate->eliminateAliens, !isAlienPlanNode);

	switch (nodeTag(node))
	{
			/*
			 * control nodes
			 */
		case T_Result:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, Result);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitResult((Result *) node,
												  estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_ModifyTable:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, ModifyTable);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitModifyTable((ModifyTable *) node,
													   estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_Append:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, Append);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitAppend((Append *) node,
												  estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_Sequence:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, Sequence);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitSequence((Sequence *) node,
													estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_MergeAppend:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, MergeAppend);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitMergeAppend((MergeAppend *) node,
													   estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_RecursiveUnion:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, RecursiveUnion);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitRecursiveUnion((RecursiveUnion *) node,
														  estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_BitmapAnd:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, BitmapAnd);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{

			result = (PlanState *) ExecInitBitmapAnd((BitmapAnd *) node,
													 estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_BitmapOr:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, BitmapOr);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
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
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, SeqScan);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitSeqScan((SeqScan *) node,
													 estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_DynamicSeqScan:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, DynamicSeqScan);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitDynamicSeqScan((DynamicSeqScan *) node,
												   estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_ExternalScan:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, ExternalScan);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitExternalScan((ExternalScan *) node,
														estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_IndexScan:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, IndexScan);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitIndexScan((IndexScan *) node,
													 estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_DynamicIndexScan:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, DynamicIndexScan);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitDynamicIndexScan((DynamicIndexScan *) node,
													estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_IndexOnlyScan:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, IndexOnlyScan);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitIndexOnlyScan((IndexOnlyScan *) node,
														 estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_BitmapIndexScan:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, BitmapIndexScan);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
				result = (PlanState *) ExecInitBitmapIndexScan((BitmapIndexScan *) node,
															   estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_DynamicBitmapIndexScan:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, DynamicBitmapIndexScan);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
				result = (PlanState *) ExecInitDynamicBitmapIndexScan((DynamicBitmapIndexScan *) node,
																	  estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_BitmapHeapScan:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, BitmapHeapScan);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitBitmapHeapScan((BitmapHeapScan *) node,
														  estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_DynamicBitmapHeapScan:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, DynamicBitmapHeapScan);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitDynamicBitmapHeapScan((DynamicBitmapHeapScan *) node,
																 estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_TidScan:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, TidScan);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitTidScan((TidScan *) node,
												   estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_SubqueryScan:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, SubqueryScan);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitSubqueryScan((SubqueryScan *) node,
														estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_FunctionScan:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, FunctionScan);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitFunctionScan((FunctionScan *) node,
														estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_TableFunctionScan:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, TableFunctionScan);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitTableFunction((TableFunctionScan *) node,
														 estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_ValuesScan:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, ValuesScan);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitValuesScan((ValuesScan *) node,
													  estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_CteScan:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, CteScan);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitCteScan((CteScan *) node,
												   estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_WorkTableScan:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, WorkTableScan);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitWorkTableScan((WorkTableScan *) node,
														 estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_ForeignScan:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, ForeignScan);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitForeignScan((ForeignScan *) node,
														estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

			/*
			 * join nodes
			 */
		case T_NestLoop:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, NestLoop);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitNestLoop((NestLoop *) node,
													estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_MergeJoin:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, MergeJoin);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitMergeJoin((MergeJoin *) node,
													 estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_HashJoin:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, HashJoin);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
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
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, ShareInputScan);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitShareInputScan((ShareInputScan *) node, estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

			/*
			 * materialization nodes
			 */
		case T_Material:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, Material);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitMaterial((Material *) node,
													estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_Sort:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, Sort);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitSort((Sort *) node,
												estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_Agg:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, Agg);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitAgg((Agg *) node,
												 estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_WindowAgg:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, WindowAgg);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitWindowAgg((WindowAgg *) node,
													 estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_Unique:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, Unique);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitUnique((Unique *) node,
												  estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_Hash:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, Hash);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitHash((Hash *) node,
												estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_SetOp:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, SetOp);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitSetOp((SetOp *) node,
												 estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_LockRows:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, LockRows);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitLockRows((LockRows *) node,
													estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_Limit:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, Limit);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitLimit((Limit *) node,
												 estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_Motion:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, Motion);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitMotion((Motion *) node,
												  estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;

		case T_Repeat:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, Repeat);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitRepeat((Repeat *) node,
												  estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;
		case T_DML:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, DML);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitDML((DML *) node,
												  estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;
		case T_SplitUpdate:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, SplitUpdate);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
			result = (PlanState *) ExecInitSplitUpdate((SplitUpdate *) node,
												  estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;
		case T_AssertOp:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, AssertOp);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
 			result = (PlanState *) ExecInitAssertOp((AssertOp *) node,
 												  estate, eflags);
			}
			END_MEMORY_ACCOUNT();
			break;
		case T_RowTrigger:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, RowTrigger);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
			{
 			result = (PlanState *) ExecInitRowTrigger((RowTrigger *) node,
 												   estate, eflags);
			}
			END_MEMORY_ACCOUNT();
 			break;
		case T_PartitionSelector:
			curMemoryAccountId = CREATE_EXECUTOR_MEMORY_ACCOUNT(isAlienPlanNode, node, PartitionSelector);

			START_MEMORY_ACCOUNT(curMemoryAccountId);
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
		result->instrument = GpInstrAlloc(node, estate->es_instrument);

	/* Also set up gpmon counters */
	InitPlanNodeGpmonPkt(node, &result->gpmon_pkt, estate);

	if (result != NULL)
	{
		saveExecutorMemoryAccount(result, curMemoryAccountId);
	}
	return result;
}

/* ----------------------------------------------------------------
 *		ExecSliceDependencyNode
 *
 *		Exec dependency, block till slice dependency are met
 * ----------------------------------------------------------------
 */
void
ExecSliceDependencyNode(PlanState *node)
{
	CHECK_FOR_INTERRUPTS();

	if (node == NULL)
		return;

	if (nodeTag(node) == T_ShareInputScanState)
		ExecSliceDependencyShareInputScan((ShareInputScanState *) node);
	else if (nodeTag(node) == T_SubqueryScanState)
	{
		SubqueryScanState *subq = (SubqueryScanState *) node;

		ExecSliceDependencyNode(subq->subplan);
	}
	else if (nodeTag(node) == T_AppendState)
	{
		int			i = 0;
		AppendState *app = (AppendState *) node;

		for (; i < app->as_nplans; ++i)
			ExecSliceDependencyNode(app->appendplans[i]);
	}
	else if (nodeTag(node) == T_SequenceState)
	{
		int			i = 0;
		SequenceState *ss = (SequenceState *) node;

		for (; i < ss->numSubplans; ++i)
			ExecSliceDependencyNode(ss->subplans[i]);
	}
	else if (nodeTag(node) == T_ModifyTableState)
	{
		int			i = 0;
		ModifyTableState *mt = (ModifyTableState *) node;

		for (; i < mt->mt_nplans; ++i)
			ExecSliceDependencyNode(mt->mt_plans[i]);
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

	START_MEMORY_ACCOUNT(node->memoryAccountId);
	{

	CHECK_FOR_INTERRUPTS();

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

	if (node->chgParam != NULL) /* something changed */
		ExecReScan(node);		/* let ReScan handle this */

	if (node->squelched)
		elog(ERROR, "cannot execute squelched plan node of type: %d",
			 (int) nodeTag(node));

	if (node->instrument)
		InstrStartNode(node->instrument);

	if(!node->fHadSentGpmon)
		CheckSendPlanStateGpmonPkt(node);

	if(!node->fHadSentNodeStart)
	{
		/* GPDB hook for collecting query info */
		if (query_info_collect_hook)
			(*query_info_collect_hook)(METRICS_PLAN_NODE_EXECUTING, node);
		node->fHadSentNodeStart = true;
	}

	switch (nodeTag(node))
	{
			/*
			 * control nodes
			 */
		case T_ResultState:
			result = ExecResult((ResultState *) node);
			break;

		case T_ModifyTableState:
			result = ExecModifyTable((ModifyTableState *) node);
			break;

		case T_AppendState:
			result = ExecAppend((AppendState *) node);
			break;

		case T_MergeAppendState:
			result = ExecMergeAppend((MergeAppendState *) node);
			break;

		case T_RecursiveUnionState:
			result = ExecRecursiveUnion((RecursiveUnionState *) node);
			break;

		case T_SequenceState:
			result = ExecSequence((SequenceState *) node);
			break;

			/* BitmapAndState does not yield tuples */

			/* BitmapOrState does not yield tuples */

			/*
			 * scan nodes
			 */
		case T_SeqScanState:
			result = ExecSeqScan((SeqScanState *)node);
			break;

		case T_DynamicSeqScanState:
			result = ExecDynamicSeqScan((DynamicSeqScanState *) node);
			break;

		case T_ExternalScanState:
			result = ExecExternalScan((ExternalScanState *) node);
			break;

		case T_IndexScanState:
			result = ExecIndexScan((IndexScanState *) node);
			break;

		case T_DynamicIndexScanState:
			result = ExecDynamicIndexScan((DynamicIndexScanState *) node);
			break;

		case T_IndexOnlyScanState:
			result = ExecIndexOnlyScan((IndexOnlyScanState *) node);
			break;

			/* BitmapIndexScanState does not yield tuples */

		case T_BitmapHeapScanState:
			result = ExecBitmapHeapScan((BitmapHeapScanState *) node);
			break;

		case T_DynamicBitmapHeapScanState:
			result = ExecDynamicBitmapHeapScan((DynamicBitmapHeapScanState *) node);
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

		case T_CteScanState:
			result = ExecCteScan((CteScanState *) node);
			break;

		case T_WorkTableScanState:
			result = ExecWorkTableScan((WorkTableScanState *) node);
			break;

		case T_ForeignScanState:
			result = ExecForeignScan((ForeignScanState *) node);
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
			 * materialization nodes
			 */
		case T_MaterialState:
			result = ExecMaterial((MaterialState *) node);
			break;

		case T_SortState:
			result = ExecSort((SortState *) node);
			break;

		case T_AggState:
			result = ExecAgg((AggState *) node);
			break;

		case T_WindowAggState:
			result = ExecWindowAgg((WindowAggState *) node);
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

		case T_LockRowsState:
			result = ExecLockRows((LockRowsState *) node);
			break;

		case T_LimitState:
			result = ExecLimit((LimitState *) node);
			break;

		case T_MotionState:
			result = ExecMotion((MotionState *) node);
			break;

		case T_ShareInputScanState:
			result = ExecShareInputScan((ShareInputScanState *) node);
			break;

		case T_RepeatState:
			result = ExecRepeat((RepeatState *) node);
			break;

		case T_DMLState:
			result = ExecDML((DMLState *) node);
			break;

		case T_SplitUpdateState:
			result = ExecSplitUpdate((SplitUpdateState *) node);
			break;

		case T_RowTriggerState:
			result = ExecRowTrigger((RowTriggerState *) node);
			break;

		case T_AssertOpState:
			result = ExecAssertOp((AssertOpState *) node);
			break;

		case T_PartitionSelectorState:
			result = ExecPartitionSelector((PartitionSelectorState *) node);
			break;

		default:
			elog(ERROR, "unrecognized node type: %d", (int) nodeTag(node));
			result = NULL;
			break;
	}

	if (!TupIsNull(result))
	{
		Gpmon_Incr_Rows_Out(&node->gpmon_pkt);
		CheckSendPlanStateGpmonPkt(node);
	}

	if (node->instrument)
		InstrStopNode(node->instrument, TupIsNull(result) ? 0 : 1);

	if (node->plan)
		TRACE_POSTGRESQL_EXECPROCNODE_EXIT(GpIdentity.segindex, currentSliceId, nodeTag(node), node->plan->plan_node_id);
	}
	END_MEMORY_ACCOUNT();
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

	START_MEMORY_ACCOUNT(node->memoryAccountId);
{
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
}
	END_MEMORY_ACCOUNT();
	return result;
}

static CdbVisitOpt
transportUpdateNodeWalker(PlanState *node, void *context)
{
	/*
	 * For motion nodes, we just transfer the context information established
	 * during SetupInterconnect
	 */
	if (IsA(node, MotionState))
	{
		node->state->interconnect_context = (ChunkTransportState *) context;
		/* visit subtree */
	}

	return CdbVisit_Walk;
}	/* transportUpdateNodeWalker */

void
ExecUpdateTransportState(PlanState *node, ChunkTransportState * state)
{
	Assert(node);
	Assert(state);
	planstate_walk_node(node, transportUpdateNodeWalker, state);
}	/* ExecUpdateTransportState */


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

	EState	   *estate = node->state;

	Assert(estate != NULL);
	int			origSliceIdInPlan = estate->currentSliceIdInPlan;
	int			origExecutingSliceId = estate->currentExecutingSliceId;

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

		case T_IndexScanState:
			ExecEndIndexScan((IndexScanState *) node);
			break;

		case T_DynamicIndexScanState:
			ExecEndDynamicIndexScan((DynamicIndexScanState *) node);
			break;

		case T_ExternalScanState:
			ExecEndExternalScan((ExternalScanState *) node);
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

		case T_ValuesScanState:
			ExecEndValuesScan((ValuesScanState *) node);
			break;

		case T_CteScanState:
			ExecEndCteScan((CteScanState *) node);
			break;

		case T_WorkTableScanState:
			ExecEndWorkTableScan((WorkTableScanState *) node);
			break;

		case T_ForeignScanState:
			ExecEndForeignScan((ForeignScanState *) node);
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
	/* GPDB hook for collecting query info */
	if (query_info_collect_hook)
		(*query_info_collect_hook)(METRICS_PLAN_NODE_FINISHED, node);

	estate->currentSliceIdInPlan = origSliceIdInPlan;
	estate->currentExecutingSliceId = origExecutingSliceId;
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
