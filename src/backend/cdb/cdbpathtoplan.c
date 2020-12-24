/*-------------------------------------------------------------------------
 *
 * cdbpathtoplan.c
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbpathtoplan.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "cdb/cdbpathlocus.h"
#include "cdb/cdbllize.h"		/* makeFlow() */
#include "cdb/cdbpathtoplan.h"	/* me */
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"
#include "nodes/pathnodes.h"

/*
 * cdbpathtoplan_create_flow
 */
Flow *
cdbpathtoplan_create_flow(PlannerInfo *root,
						  CdbPathLocus locus)
{
	Flow	   *flow = NULL;

	/* Distribution */
	if (CdbPathLocus_IsEntry(locus))
	{
		flow = makeFlow(FLOW_SINGLETON, 1);
		flow->segindex = -1;
	}
	else if (CdbPathLocus_IsSingleQE(locus))
	{
		flow = makeFlow(FLOW_SINGLETON, locus.numsegments);
		flow->segindex = 0;
	}
	else if (CdbPathLocus_IsGeneral(locus))
	{
		if (Gp_role == GP_ROLE_DISPATCH)
			flow = makeFlow(FLOW_SINGLETON, getgpsegmentCount());
		else
			flow = makeFlow(FLOW_SINGLETON, 1); /* dummy flow */
		flow->segindex = 0;
	}
	else if (CdbPathLocus_IsSegmentGeneral(locus))
	{
		flow = makeFlow(FLOW_SINGLETON, locus.numsegments);
		flow->segindex = 0;
	}
	else if (CdbPathLocus_IsReplicated(locus))
	{
		flow = makeFlow(FLOW_REPLICATED, locus.numsegments);
	}
	else if (CdbPathLocus_IsHashed(locus) ||
			 CdbPathLocus_IsHashedOJ(locus))
	{
		flow = makeFlow(FLOW_PARTITIONED, locus.numsegments);

		/*
		 * hashExpr can be NIL if the rel is partitioned on columns that
		 * aren't projected (i.e. are not present in the result of this Path
		 * operator).
		 */
	}
	else if (CdbPathLocus_IsStrewn(locus))
		flow = makeFlow(FLOW_PARTITIONED, locus.numsegments);
	else if (CdbPathLocus_IsOuterQuery(locus))
	{
		/*
		 * A plan that's attached to the outer query later on will run on
		 * the same segments as the outer query. We don't know what the
		 * locus of the outer query is yet, so just mark the plan with a
		 * dummy Flow. It's good enough for the rest of the planner.
		 */
		flow = makeFlow(FLOW_SINGLETON, 1);
	}
	else
		elog(ERROR, "incorrect locus type %d to create flow", locus.locustype);

	flow->locustype = locus.locustype;
	return flow;
}								/* cdbpathtoplan_create_flow */
