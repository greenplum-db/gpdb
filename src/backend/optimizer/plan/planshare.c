/*-------------------------------------------------------------------------
 *
 * planshare.c
 *	  Plan shared plan
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/plan/planshare.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/planmain.h"
#include "optimizer/subselect.h"
#include "optimizer/planshare.h"

static ShareInputScan *
make_shareinputscan(PlannerInfo *root, Plan *inputplan)
{
	ShareInputScan *sisc;
	Path		sipath;

	sisc = makeNode(ShareInputScan);

	sisc->scan.plan.targetlist = copyObject(inputplan->targetlist);
	sisc->scan.plan.lefttree = inputplan;
	sisc->scan.plan.flow = copyObject(inputplan->flow);

	sisc->cross_slice = false;
	sisc->producer_slice_id = -1;
	sisc->this_slice_id = -1;
	sisc->nconsumers = 0;

	sisc->scan.plan.qual = NIL;
	sisc->scan.plan.righttree = NULL;

	cost_shareinputscan(&sipath, root, inputplan->total_cost, inputplan->plan_rows, inputplan->plan_width);

	sisc->scan.plan.startup_cost = sipath.startup_cost;
	sisc->scan.plan.total_cost = sipath.total_cost; 
	sisc->scan.plan.plan_rows = inputplan->plan_rows;
	sisc->scan.plan.plan_width = inputplan->plan_width;

	sisc->scan.plan.extParam = bms_copy(inputplan->extParam);
	sisc->scan.plan.allParam = bms_copy(inputplan->allParam);

	return sisc;
}

/*
 * Prepare a subplan for sharing. After this, you can call
 * share_prepared_plan() as many times as you want to share this plan.
 *
 * This doesn't do much at the moment. One little optimization is that
 * if the subplan is a ShareInputScan, we make the new ShareInputScans
 * be siblings of that, rather than creating a ShareInputScan on
 * top of a ShareInputScan.
 */
Plan *
prepare_plan_for_sharing(PlannerInfo *root, Plan *common)
{
	Plan *shared = common;

	if (IsA(common, ShareInputScan))
	{
		shared = common->lefttree;
	}

	return shared;
}

/*
 * Create a ShareInputScan to scan the given subplan. The subplan
 * must've been prepared for sharing by calling
 * prepare_plan_for_sharing().
 */
Plan *
share_prepared_plan(PlannerInfo *root, Plan *common)
{
	return (Plan *) make_shareinputscan(root, common);
}
