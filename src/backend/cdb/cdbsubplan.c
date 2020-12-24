/*-------------------------------------------------------------------------
 *
 * cdbsubplan.c
 *	  Provides routines for preprocessing initPlan subplans
 *		and executing queries with initPlans
 *
 * Portions Copyright (c) 2003-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
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
#include "utils/tuplestore.h"

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
	int			nParamExec;
	int			i;
	EState	   *estate = queryDesc->estate;
	int			originalSlice,
				rootIndex;

	nParamExec = list_length(queryDesc->plannedstmt->paramExecTypes);
	if (nParamExec == 0)
		return;

	originalSlice = LocallyExecutingSliceIndex(queryDesc->estate);
	Assert(originalSlice == 0); /* Original slice being executed is slice 0 */

	/*
	 * Loop through the estate->es_param_exec_vals. This array has an element
	 * for each PARAM_EXEC (internal) param, and a pointer to the SubPlanState
	 * to execute to evaluate it. It seems that they are created in the proper
	 * order, i.e. if a subplan x has a sublan y, then y will come before x in
	 * the es_param_exec_vals array.
	 */
	for (i = 0; i < nParamExec; i++)
	{
		ParamExecData *prm;
		SubPlanState *sps;

		prm = &estate->es_param_exec_vals[i];
		sps = (SubPlanState *) prm->execPlan;

		if (isParamExecutableNow(sps, estate->es_param_exec_vals))
		{
			SubPlan    *subplan = sps->subplan;
			int			qDispSliceId;

			Assert(IsA(subplan, SubPlan));

			if (queryDesc->plannedstmt->subplan_sliceIds)
			{
				qDispSliceId = queryDesc->plannedstmt->subplan_sliceIds[subplan->plan_id - 1];
				if (queryDesc->plannedstmt->slices[qDispSliceId].parentIndex != -1)
					qDispSliceId = 0;		/* not an init plan */
			}
			else
				qDispSliceId = 0;

			if (qDispSliceId > 0)
			{
				/*
				 * Adjust for the slice to execute on the QD.
				 */
				rootIndex = qDispSliceId;
				queryDesc->estate->es_sliceTable->localSlice = rootIndex;

				/* set our global sliceid variable for elog. */
				currentSliceId = rootIndex;

				/*
				 * Use ExprContext to set the param. If ExprContext is not
				 * initialized, create a new one here. (see MPP-3511)
				 */
				if (sps->planstate->ps_ExprContext == NULL)
					sps->planstate->ps_ExprContext = CreateExprContext(estate);

				/* MPP-12048: Set the right slice index before execution. */
				Assert(qDispSliceId < queryDesc->plannedstmt->numSlices);

				Assert(LocallyExecutingSliceIndex(sps->planstate->state) == qDispSliceId);

				/*
				 * Dispatch this initplan query. ExecSetParamPlan() calls
				 * CdbDispatchPlan() to dispatch it.
				 *
				 * Note that this will rebuild the array of dispatched PARAM_EXEC
				 * values on every iteration. That's important because the set
				 * of valid PARAM_EXEC values grows on every iteration, and
				 * later Init plans can depend on previous ones.
				 */
				ExecSetParamPlan(sps, sps->planstate->ps_ExprContext, queryDesc);
			}
		}

		queryDesc->estate->es_sliceTable->localSlice = originalSlice;
		currentSliceId = originalSlice;
	}
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
	EState	   *estate = queryDesc->estate;
	int			nParamExec;
	ParamExecData *prm;
	SubPlanState *sps;
	int			i;

	nParamExec = list_length(queryDesc->plannedstmt->paramExecTypes);

	/* clean ntuplestore used by INITPLAN function */
	for (i = 0; i < nParamExec; i++)
	{
		prm = &estate->es_param_exec_vals[i];
		sps = (SubPlanState *) prm->execPlan;
		if (sps && sps->ts_state)
		{
			tuplestore_end(sps->ts_state);
			sps->ts_state = NULL;
		}
	}
}

static bool
isParamExecutableNow(SubPlanState *spstate, ParamExecData *prmList)
{
	Assert(prmList);
	if (!spstate)
		return false;

	List	   *extParam = spstate->subplan->extParam;

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
