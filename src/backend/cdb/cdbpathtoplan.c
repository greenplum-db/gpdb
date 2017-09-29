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

#include "optimizer/pathnode.h" /* Path */
#include "optimizer/planmain.h" /* make_sort_from_pathkeys() */
#include "optimizer/tlist.h"

#include "cdb/cdbllize.h"		/* makeFlow() */
#include "cdb/cdbmutate.h"		/* make_*_motion() */
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"		/* gp_singleton_segindex */

#include "cdb/cdbpathtoplan.h"	/* me */

/*
 * cdbpathtoplan_create_flow
 */
Flow *
cdbpathtoplan_create_flow(PlannerInfo *root,
						  CdbPathLocus locus,
						  Relids relids,
						  List *pathkeys,
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


/*
 * cdbpathtoplan_create_motion_plan
 */
Motion *
cdbpathtoplan_create_motion_plan(PlannerInfo *root,
								 CdbMotionPath *path,
								 Plan *subplan)
{
	Motion	   *motion = NULL;
	Path	   *subpath = path->subpath;

	/* Send all tuples to a single process? */
	if (CdbPathLocus_IsBottleneck(path->path.locus))
	{
		int			destSegIndex = -1;	/* to dispatcher */

		if (CdbPathLocus_IsSingleQE(path->path.locus))
			destSegIndex = gp_singleton_segindex;	/* to singleton qExec */

		if (path->path.pathkeys)
		{
			/*
			 * Build a dummy Sort node.  We'll take its sort key info to
			 * define our Merge Receive keys.  Unchanged subplan ptr is
			 * returned to us if ordering is degenerate (all cols constant).
			 */
			Sort	   *sort = make_sort_from_pathkeys(root,
													   subplan,
													   path->path.pathkeys,
													   -1.0,
													   true);

			/* Merge Receive to preserve ordering */
			if (sort)
			{
				/* Result node might have been added below the Sort */
				subplan = sort->plan.lefttree;
				motion = make_sorted_union_motion(root,
												  subplan,
												  destSegIndex,
												  path->path.pathkeys,
												  false /* useExecutorVarFormat */
					);
			}

			/* Degenerate ordering... build unordered Union Receive */
			else
				motion = make_union_motion(subplan,
										   destSegIndex,
										   false	/* useExecutorVarFormat */
					);
		}

		/* Unordered Union Receive */
		else
			motion = make_union_motion(subplan,
									   destSegIndex,
									   false	/* useExecutorVarFormat */
				);
	}

	/* Send all of the tuples to all of the QEs in gang above... */
	else if (CdbPathLocus_IsReplicated(path->path.locus))
		motion = make_broadcast_motion(subplan,
									   false	/* useExecutorVarFormat */
			);

	/* Hashed redistribution to all QEs in gang above... */
	else if (CdbPathLocus_IsHashed(path->path.locus) ||
			 CdbPathLocus_IsHashedOJ(path->path.locus))
	{
		List	   *hashExpr = cdbpathlocus_get_partkey_exprs(path->path.locus,
															  path->path.parent->relids,
															  subplan->targetlist);

		Insist(hashExpr);

		/**
         * If there are subplans in the hashExpr, push it down to lower level.
         */
		if (contain_subplans((Node *) hashExpr))
		{
			/* make a Result node to do the projection if necessary */
			if (!is_projection_capable_plan(subplan))
			{
				List	   *tlist = copyObject(subplan->targetlist);

				subplan = (Plan *) make_result(root, tlist, NULL, subplan);
			}
			subplan->targetlist = add_to_flat_tlist(subplan->targetlist,
													hashExpr,
													true /* resjunk */ );
		}
		motion = make_hashed_motion(subplan,
									hashExpr,
									false /* useExecutorVarFormat */ );
	}
	else
		Insist(0);

	/*
	 * Decorate the subplan with a Flow node telling the plan slicer what kind
	 * of gang will be needed to execute the subplan.
	 */
	subplan->flow = cdbpathtoplan_create_flow(root,
											  subpath->locus,
											  subpath->parent
											  ? subpath->parent->relids
											  : NULL,
											  subpath->pathkeys,
											  subplan);

	/**
	 * If plan has a flow node, and its child is projection capable,
	 * then ensure all entries of hashExpr are in the targetlist.
	 */
	if (subplan->flow
		&& subplan->flow->hashExpr
		&& is_projection_capable_plan(subplan))
	{
		subplan->targetlist = add_to_flat_tlist(subplan->targetlist, subplan->flow->hashExpr, true /* resjunk */ );
	}

	return motion;
}								/* cdbpathtoplan_create_motion_plan */
