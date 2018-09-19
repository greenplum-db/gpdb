/*-------------------------------------------------------------------------
 *
 * cdbpathtoplan.c
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbpathtoplan.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "optimizer/tlist.h"

#include "cdb/cdbpathlocus.h"
#include "cdb/cdbllize.h"		/* makeFlow() */
#include "cdb/cdbpathtoplan.h"	/* me */

/*
 * cdbpathtoplan_create_flow
 */
Flow *
cdbpathtoplan_create_flow(PlannerInfo *root,
						  CdbPathLocus locus,
						  Relids relids,
						  Plan *plan)
{
	Flow	   *flow = NULL;

	/* Distribution */
	if (CdbPathLocus_IsEntry(locus))
	{
		flow = makeFlow(FLOW_SINGLETON);
		flow->segindex = -1;
	}
	else if (CdbPathLocus_IsSingleQE(locus))
	{
		flow = makeFlow(FLOW_SINGLETON);
		flow->segindex = 0;
	}
	else if (CdbPathLocus_IsGeneral(locus))
	{
		flow = makeFlow(FLOW_SINGLETON);
		flow->segindex = 0;
	}
	else if (CdbPathLocus_IsSegmentGeneral(locus))
	{
		flow = makeFlow(FLOW_SINGLETON);
		flow->segindex = 0;
	}
	else if (CdbPathLocus_IsReplicated(locus))
	{
		flow = makeFlow(FLOW_REPLICATED);
	}
	else if (CdbPathLocus_IsHashed(locus) ||
			 CdbPathLocus_IsHashedOJ(locus))
	{
		flow = makeFlow(FLOW_PARTITIONED);
		flow->hashExpr = cdbpathlocus_get_partkey_exprs(locus,
														relids,
														plan->targetlist);

		/*
		 * hashExpr can be NIL if the rel is partitioned on columns that
		 * aren't projected (i.e. are not present in the result of this Path
		 * operator).
		 */
	}
	else if (CdbPathLocus_IsStrewn(locus))
		flow = makeFlow(FLOW_PARTITIONED);
	else
		Insist(0);

	flow->req_move = MOVEMENT_NONE;
	flow->locustype = locus.locustype;
	return flow;
}								/* cdbpathtoplan_create_flow */
