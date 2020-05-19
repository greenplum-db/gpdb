/*-------------------------------------------------------------------------
 *
 * cdbsubplan.c
 *	  Provides routines for preprocessing initPlan subplans
 *		and executing queries with initPlans
 *
 * Portions Copyright (c) 2003-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbsubplan.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "executor/nodeSubplan.h"	/* For ExecSetParamPlan */
#include "executor/executor.h"	/* For CreateExprContext */
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbplan.h"
#include "cdb/cdbllize.h"
#include "cdb/cdbsubplan.h"
#include "cdb/cdbvars.h"		/* currentSliceId */
#include "cdb/ml_ipc.h"
#include "utils/tuplestorenew.h"

typedef struct ParamWalkerContext
{
	plan_tree_base_prefix base; /* Required prefix for
								 * plan_tree_walker/mutator */
	List	   *params;
	Bitmapset  *wtParams;
	Bitmapset  *epqParams;
} ParamWalkerContext;

static bool param_walker(Node *node, ParamWalkerContext *context);
static Oid	findParamType(List *params, int paramid);
static bool isParamExecutableNow(SubPlanState *spstate, ParamExecData *prmList);

/*
 * Function preprocess_initplans() is called from ExecutorRun running a
 * parallel plan on the QD.  The call happens prior to dispatch of the
 * main plan, and only if there are some initplans.
 *
 * Argument queryDesc is the one passed in to ExecutorRun.
 *
 * The function loops through the estate->es_param_exec_vals array, which
 * has plan->nParamExec elements.  Each element is a ParamExecData struct,
 * and the index of the element in the array is the paramid of the Param
 * node in the Plan that corresponds to the result of the subquery.
 *
 * The execPlan member points to a SubPlanState struct for the
 * subquery.  The value and isnull members hold the result
 * of executing the SubPlan.
 * I think that the order of the elements in this array guarantees
 * that for a subplan X within a subplan Y, X will come before Y in the array.
 * If a subplan returns multiple columns (like a MULTIEXPR_SUBLINK), each will be
 * a separate entry in the es_param_exec_vals array, but they will all have
 * the same value for execPlan.
 * In order to evaluate a subplan, we call ExecSetParamPlan.
 * This is a postgres function, but has been modified from its original form
 * to parallelize subplans. Inside ExecSetParamPlan, the
 * datum result(s) of the subplan are stuffed into the value field
 * of the ParamExecData struct(s).	It finds the proper one based on the
 * setParam list in the SubPlan node.
 * In order to handle SubPlans of SubPlans, we pass in the values of the
 * estate->es_param_exec_vals as ParamListInfo structs to the ExecSetParamPlan call.
 * These are then serialized into the mppexec all as parameters.  In this manner, the
 * result of a SubPlan of a SubPlan is available.
 */
void
preprocess_initplans(QueryDesc *queryDesc)
{
	ParamListInfo originalPli,
				augmentedPli;
	int			i;
	EState	   *estate = queryDesc->estate;
	int			originalSlice,
				rootIndex;

	if (queryDesc->plannedstmt->nParamExec == 0)
		return;

	originalPli = queryDesc->params;

	originalSlice = LocallyExecutingSliceIndex(queryDesc->estate);
	Assert(originalSlice == 0); /* Original slice being executed is slice 0 */

	/*
	 * Loop through the estate->es_param_exec_vals. This array has an element
	 * for each PARAM_EXEC (internal) param, and a pointer to the SubPlanState
	 * to execute to evaluate it. It seems that they are created in the proper
	 * order, i.e. if a subplan x has a sublan y, then y will come before x in
	 * the es_param_exec_vals array.
	 */
	for (i = 0; i < queryDesc->plannedstmt->nParamExec; i++)
	{
		ParamExecData *prm;
		SubPlanState *sps;

		prm = &estate->es_param_exec_vals[i];
		sps = (SubPlanState *) prm->execPlan;

		/*
		 * Append all the es_param_exec_vals datum values on to the external
		 * parameter list so they can be serialized in the mppexec call to the
		 * QEs.  Do this inside the loop since later initplans may depend on
		 * the results of earlier ones.
		 *
		 * TODO Some of the work of addRemoteExecParamsToParmList could be
		 * factored out of the loop.
		 */
		augmentedPli = addRemoteExecParamsToParamList(queryDesc->plannedstmt,
													  originalPli,
													  estate->es_param_exec_vals);

		if (isParamExecutableNow(sps, estate->es_param_exec_vals))
		{
			SubPlan    *subplan = (SubPlan *) sps->xprstate.expr;

			Assert(IsA(subplan, SubPlan));

			if (subplan->qDispSliceId > 0)
			{
				/*
				 * Adjust for the slice to execute on the QD.
				 */
				rootIndex = subplan->qDispSliceId;
				queryDesc->estate->es_sliceTable->localSlice = rootIndex;

				/* set our global sliceid variable for elog. */
				currentSliceId = rootIndex;

				/*
				 * This runs the SubPlan and puts the answer back into
				 * prm->value.
				 */
				queryDesc->params = augmentedPli;

				/*
				 * Use ExprContext to set the param. If ExprContext is not
				 * initialized, create a new one here. (see MPP-3511)
				 */
				if (sps->planstate->ps_ExprContext == NULL)
					sps->planstate->ps_ExprContext = CreateExprContext(estate);

				/* MPP-12048: Set the right slice index before execution. */
				Assert((subplan->qDispSliceId > queryDesc->plannedstmt->nMotionNodes) &&
					   (subplan->qDispSliceId <=
						(queryDesc->plannedstmt->nMotionNodes
						 + queryDesc->plannedstmt->nInitPlans)));

				Assert(LocallyExecutingSliceIndex(sps->planstate->state) == subplan->qDispSliceId);

				/*
				 * sps->planstate->state->es_cur_slice_idx =
				 * subplan->qDispSliceId;
				 */

				ExecSetParamPlan(sps, sps->planstate->ps_ExprContext, queryDesc);

				/*
				 * We dispatched, and have returned. We may have used the
				 * interconnect; so let's bump the interconnect-id.
				 */
				queryDesc->estate->es_sliceTable->ic_instance_id = ++gp_interconnect_id;
			}
		}

		queryDesc->params = originalPli;
		queryDesc->estate->es_sliceTable->localSlice = originalSlice;
		currentSliceId = originalSlice;

		pfree(augmentedPli);
	}
}

/*
 * Function: addRemoteExecParamsToParamList()
 *
 * Creates a new ParamListInfo array from the existing one by appending
 * the array of ParamExecData (internal parameter) values as a new Param
 * type: PARAM_EXEC_REMOTE.
 *
 * When the query eventually runs (on the QD or a QE), it will have access
 * to the augmented ParamListInfo (locally or through serialization).
 *
 * Then, rather than lazily-evaluating the SubPlan to get its value (as for
 * an internal parameter), the plan will just look up the param value in the
 * ParamListInfo (as for an external parameter).
 */
ParamListInfo
addRemoteExecParamsToParamList(PlannedStmt *stmt, ParamListInfo extPrm, ParamExecData *intPrm)
{
	ParamListInfo augPrm;
	ParamWalkerContext context;
	int			i;
	int			nParams;
	Plan	   *plan = stmt->planTree;
	int			nIntPrm = stmt->nParamExec;

	if (nIntPrm == 0)
		return extPrm;
	Assert(intPrm != NULL);		/* So there must be some internal parameters. */

	/* Count existing external parameters. */
	nParams = (extPrm == NULL ? 0 : extPrm->numParams);

	/* Allocate space for augmented array and set final sentinel. */
	augPrm = palloc0(sizeof(ParamListInfoData) + (nParams + nIntPrm) * sizeof(ParamExternData));

	/* Copy in the existing external parameter entries. */
	for (i = 0; i < nParams; i++)
	{
		memcpy(&augPrm->params[i], &extPrm->params[i], sizeof(ParamExternData));
	}

	/*
	 * Walk the plan, looking for Param nodes of kind PARAM_EXEC, i.e.,
	 * executor internal parameters.
	 *
	 * We need these for their paramtype field, which isn't available in
	 * either the ParamExecData struct or the SubPlan struct.
	 */

	exec_init_plan_tree_base(&context.base, stmt);
	context.params = NIL;
	context.wtParams = NULL;
	context.epqParams = NULL;
	param_walker((Node *) plan, &context);

	/*
	 * mpp-25490: subplanX may is within subplan Y, try to param_walker the
	 * subplans list
	 */
	if (list_length(context.params) < nIntPrm)
	{
		ListCell   *sb;

		foreach(sb, stmt->subplans)
		{
			Node	   *subplan = lfirst(sb);

			param_walker((Node *) subplan, &context);
		}
	}

	/*
	 * Now initialize the ParamListInfo elements corresponding to the
	 * initplans we're "parameterizing".  Use the datatype info harvested
	 * above.
	 */
	int			j = nParams;

	for (i = 0; i < nIntPrm; i++)
	{
		Oid			paramType = findParamType(context.params, i);

		if (InvalidOid == paramType)
		{
			/* param of id i not found. Likely have been removed because of */
			/* constant folding */
			j++;
			continue;
		}

		augPrm->params[j].ptype = paramType;
		augPrm->params[j].isnull = intPrm[i].isnull;
		augPrm->params[j].pflags = 0;
		augPrm->params[j].value = intPrm[i].value;

		j++;
	}

	/*
	 * Set number of params. Note that we don't copy the hook functions from
	 * the original ParamListInfo, on purpose. The code that set the hooks
	 * might get confused, if we called the hook with a different ParamListInfo
	 * struct, with more parameters, than it originally set the hook on.
	 * (PL/pgSQL has an assertion for the number of params, at least.)
	 * This function is used just before serializing all the parameters,
	 * and the caller has already "fetched" any parameters it can, so we don't
	 * really need the hook anymore, anyway.
	 */
	augPrm->numParams = j;

	list_free(context.params);

	return augPrm;
}

/*
 * CDB: Post processing INITPLAN to clean up resource with long life cycle
 * 
 * INITPLAN usually communicate with main plan through scalar PARAM, but in some case,
 * the main plan need to get more data from INITPLAN which long life cycle resource like
 * temp file will be used.
 * Take INITPLAN function case as an example, INITPLAN will store its result into
 * tuplestore, which will be read by entryDB in main plan. Tuplestore and corresponding
 * files should not be cleaned before the main plan finished.
 *
 * postprocess_initplans is used to clean these resources in ExecutorEnd of main plan.
 */
void
postprocess_initplans(QueryDesc *queryDesc)
{
	EState *estate = queryDesc->estate;
	ParamExecData *prm;
	SubPlanState *sps;
	int	i;

	/* clean ntuplestore used by INITPLAN function */
	for (i = 0; i < queryDesc->plannedstmt->nParamExec; i++)
	{
		prm = &estate->es_param_exec_vals[i];
		sps = (SubPlanState *) prm->execPlan;
		if(sps && sps->ts_pos)
			ntuplestore_destroy_accessor((NTupleStoreAccessor *) sps->ts_pos);
		if(sps && sps->ts_state && sps->ts_state->matstore)
			ntuplestore_destroy(sps->ts_state->matstore);
	}
}

static bool
isParamExecutableNow(SubPlanState *spstate, ParamExecData *prmList)
{
	Assert(prmList);
	if (!spstate)
		return false;

	List	   *extParam = ((SubPlan *) spstate->xprstate.expr)->extParam;

	if (extParam == NIL)
		return true;

	ListCell   *lc = NULL;
	ParamExecData *prmData = NULL;
	int			paramId = 0;

	foreach(lc, extParam)
	{
		paramId = lfirst_int(lc);
		prmData = &prmList[paramId];

		/*
		 * preprocess_initplans assumes that the params in the
		 * es_param_exec_vals are in order, so the not-evaluated depending
		 * params yet imply that they cannot be preprocessed, neither do this
		 * current param.
		 */
		if (prmData->execPlan)
			return false;
		else if (prmData->value == 0 && !prmData->isnull)
		{
			return false;
		}
	}
	return true;
}

/*
 * Helper function param_walker() walks a plan and adds any Param nodes
 * to a list in the ParamWalkerContext.
 *
 * This list is input to the function findParamType(), which loops over the
 * list looking for a specific paramid, and returns its type.
 */
static bool
param_walker(Node *node, ParamWalkerContext *context)
{
	if (node == NULL)
		return false;

	if (nodeTag(node) == T_Param)
	{
		Param	   *param = (Param *) node;

		if (param->paramkind == PARAM_EXEC)
		{
			context->params = lappend(context->params, param);
			return false;
		}
	}
	else if (IsA(node, WorkTableScan))
	{
		WorkTableScan *wt = (WorkTableScan *) node;

		context->wtParams = bms_add_member(context->wtParams, wt->wtParam);
	}
	else if (IsA(node, ModifyTable))
	{
		ModifyTable	*mt	= (ModifyTable *) node;
		context->epqParams = bms_add_member(context->epqParams, mt->epqParam);
	}
	else if (IsA(node, LockRows))
	{
		LockRows	*lr = (LockRows*) node;
		context->epqParams = bms_add_member(context->epqParams, lr->epqParam);
	}
	return plan_tree_walker(node, param_walker, context);
}

/*
 * Helper function findParamType() iterates over a list of Param nodes,
 * trying to match on the passed-in paramid. Returns the paramtype of a
 * match, else error.
 */
static Oid
findParamType(List *params, int paramid)
{
	ListCell   *l;

	foreach(l, params)
	{
		Param	   *p = lfirst(l);

		if (p->paramid == paramid)
			return p->paramtype;
	}

	return InvalidOid;
}
