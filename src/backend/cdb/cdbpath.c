/*-------------------------------------------------------------------------
 *
 * cdbpath.c
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbpath.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/hash.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_trigger.h"
#include "commands/trigger.h"
#include "nodes/makefuncs.h"	/* makeFuncExpr() */
#include "nodes/relation.h"		/* PlannerInfo, RelOptInfo */
#include "optimizer/clauses.h"
#include "optimizer/cost.h"		/* cpu_tuple_cost */
#include "optimizer/pathnode.h" /* Path, pathnode_walker() */
#include "optimizer/paths.h"
#include "optimizer/planmain.h"
#include "optimizer/tlist.h"
#include "parser/parsetree.h"
#include "parser/parse_expr.h"	/* exprType() */
#include "parser/parse_oper.h"
#include "utils/catcache.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

#include "cdb/cdbdef.h"			/* CdbSwap() */
#include "cdb/cdbhash.h"
#include "cdb/cdbpath.h"		/* me */
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"


typedef struct
{
	CdbPathLocus locus;
	CdbPathLocus move_to;
	double		bytes;
	Path	   *path;
	List       *pathkeys;
	bool		ok_to_replicate;
	bool		require_existing_order;
	bool		has_wts;		/* Does the rel have WorkTableScan? */
} CdbpathMfjRel;

static bool try_redistribute(PlannerInfo *root, CdbpathMfjRel *g,
							 CdbpathMfjRel *o, List *redistribution_clauses);

static SplitUpdatePath *make_splitupdate_path(PlannerInfo *root, Path *subpath, Index rti);

static bool can_elide_explicit_motion(PlannerInfo *root, Index rti, Path *subpath, GpPolicy *policy);
/*
 * cdbpath_cost_motion
 *    Fills in the cost estimate fields in a MotionPath node.
 */
void
cdbpath_cost_motion(PlannerInfo *root, CdbMotionPath *motionpath)
{
	Path	   *subpath = motionpath->subpath;
	Cost		cost_per_row;
	Cost		motioncost;
	double		recvrows;
	double		sendrows;

	if (CdbPathLocus_IsReplicated(motionpath->path.locus))
		motionpath->path.rows = subpath->rows * CdbPathLocus_NumSegments(motionpath->path.locus);
	else
		motionpath->path.rows = subpath->rows;

	cost_per_row = (gp_motion_cost_per_row > 0.0)
		? gp_motion_cost_per_row
		: 2.0 * cpu_tuple_cost;
	sendrows = subpath->rows;
	recvrows = motionpath->path.rows;
	motioncost = cost_per_row * 0.5 * (sendrows + recvrows);

	motionpath->path.total_cost = motioncost + subpath->total_cost;
	motionpath->path.startup_cost = subpath->startup_cost;
	motionpath->path.memory = subpath->memory;
}								/* cdbpath_cost_motion */


/*
 * cdbpath_create_motion_path
 *    Returns a Path that delivers the subpath result to the
 *    given locus, or NULL if it can't be done.
 *
 *    'pathkeys' must specify an ordering equal to or weaker than the
 *    subpath's existing ordering.
 *
 *    If no motion is needed, the caller's subpath is returned unchanged.
 *    Else if require_existing_order is true, NULL is returned if the
 *      motion would not preserve an ordering at least as strong as the
 *      specified ordering; also NULL is returned if pathkeys is NIL
 *      meaning the caller is just checking and doesn't want to add motion.
 *    Else a CdbMotionPath is returned having either the specified pathkeys
 *      (if given and the motion uses Merge Receive), or the pathkeys
 *      of the original subpath (if the motion is order-preserving), or no
 *      pathkeys otherwise (the usual case).
 */
Path *
cdbpath_create_motion_path(PlannerInfo *root,
						   Path *subpath,
						   List *pathkeys,
						   bool require_existing_order,
						   CdbPathLocus locus)
{
	CdbMotionPath *pathnode;

	Assert(cdbpathlocus_is_valid(locus) &&
		   cdbpathlocus_is_valid(subpath->locus));

	/*
	 * Motion is to change path's locus, if target locus is the
	 * same as the subpath's, there is no need to add motion.
	 */
	if (cdbpathlocus_equal(subpath->locus, locus))
		return subpath;

	/* Moving subpath output to a single executor process (qDisp or qExec)? */
	if (CdbPathLocus_IsOuterQuery(locus))
	{
		/* Outer -> Outer is a no-op */
		if (CdbPathLocus_IsOuterQuery(subpath->locus))
		{
			return subpath;
		}

		if (CdbPathLocus_IsGeneral(subpath->locus))
		{
			/* XXX: this is a bit bogus. We just change the subpath's locus. */
			subpath->locus = locus;
			return subpath;
		}

		if (CdbPathLocus_IsEntry(subpath->locus) ||
			CdbPathLocus_IsSingleQE(subpath->locus))
		{
			/*
			 * XXX: this is a bit bogus. We just change the subpath's locus.
			 *
			 * This is also bogus, because the outer query might need to run
			 * in segments.
			 */
			subpath->locus = locus;
			return subpath;
		}
	}
	else if (CdbPathLocus_IsBottleneck(locus))
	{
		/* entry-->entry?  No motion needed. */
		if (CdbPathLocus_IsEntry(subpath->locus) &&
			CdbPathLocus_IsEntry(locus))
		{
			return subpath;
		}
		/* singleQE-->singleQE?  No motion needed. */
		if (CdbPathLocus_IsSingleQE(subpath->locus) &&
			CdbPathLocus_IsSingleQE(locus))
		{
			subpath->locus.numsegments = CdbPathLocus_CommonSegments(subpath->locus, locus);
			return subpath;
		}

		/* entry-->singleQE?  Don't move.  Slice's QE will run on entry db. */
		if (CdbPathLocus_IsEntry(subpath->locus))
		{
			return subpath;
		}

		/* outerquery-->entry?  No motion needed. */
		if (CdbPathLocus_IsOuterQuery(subpath->locus) &&
			CdbPathLocus_IsEntry(locus))
		{
			return subpath;
		}

		/* singleQE-->entry?  Don't move.  Slice's QE will run on entry db. */
		if (CdbPathLocus_IsSingleQE(subpath->locus))
		{
			/*
			 * Create CdbMotionPath node to indicate that the slice must be
			 * dispatched to a singleton gang running on the entry db.  We
			 * merely use this node to note that the path has 'Entry' locus;
			 * no corresponding Motion node will be created in the Plan tree.
			 */
			Assert(CdbPathLocus_IsEntry(locus));

			pathnode = makeNode(CdbMotionPath);
			pathnode->path.pathtype = T_Motion;
			pathnode->path.parent = subpath->parent;
			/* Motion doesn't project, so use source path's pathtarget */
			pathnode->path.pathtarget = subpath->pathtarget;
			pathnode->path.locus = locus;
			pathnode->path.rows = subpath->rows;

			/* GPDB_96_MERGE_FIXME: When is a Motion path parallel-safe? I tried
			 * setting this to 'false' initially, to play it safe, but somehow
			 * the Paths with motions ended up in gather plans anyway, and tripped
			 * assertion failures.
			 */
			pathnode->path.parallel_aware = false;
			pathnode->path.parallel_safe = subpath->parallel_safe;
			pathnode->path.parallel_workers = subpath->parallel_workers;
			pathnode->path.pathkeys = pathkeys;

			pathnode->subpath = subpath;

			/* Costs, etc, are same as subpath. */
			pathnode->path.startup_cost = subpath->total_cost;
			pathnode->path.total_cost = subpath->total_cost;
			pathnode->path.memory = subpath->memory;
			pathnode->path.motionHazard = subpath->motionHazard;

			/* Motion nodes are never rescannable. */
			pathnode->path.rescannable = false;
			return (Path *) pathnode;
		}

		if (CdbPathLocus_IsSegmentGeneral(subpath->locus) ||
			CdbPathLocus_IsReplicated(subpath->locus))
		{
			/*
			 * Data is only available on segments, to distingush it with
			 * CdbLocusType_General, adding a motion to indicated this
			 * slice must be executed on a singleton gang.
			 *
			 * This motion may be redundant for segmentGeneral --> singleQE
			 * if the singleQE is not promoted to executed on qDisp in the
			 * end, so in cdbllize_fix_outer_query_motions(), we will omit it.
			 */
			pathnode = makeNode(CdbMotionPath);
			pathnode->path.pathtype = T_Motion;
			pathnode->path.parent = subpath->parent;
			/* Motion doesn't project, so use source path's pathtarget */
			pathnode->path.pathtarget = subpath->pathtarget;
			pathnode->path.locus = locus;
			pathnode->path.rows = subpath->rows;
			pathnode->path.pathkeys = pathkeys;

			/* GPDB_96_MERGE_FIXME: When is a Motion path parallel-safe? I tried
			 * setting this to 'false' initially, to play it safe, but somehow
			 * the Paths with motions ended up in gather plans anyway, and tripped
			 * assertion failures.
			 */
			pathnode->path.parallel_aware = false;
			pathnode->path.parallel_safe = subpath->parallel_safe;
			pathnode->path.parallel_workers = subpath->parallel_workers;

			pathnode->subpath = subpath;

			/* Costs, etc, are same as subpath. */
			pathnode->path.startup_cost = subpath->total_cost;
			pathnode->path.total_cost = subpath->total_cost;
			pathnode->path.memory = subpath->memory;
			pathnode->path.motionHazard = subpath->motionHazard;

			/* Motion nodes are never rescannable. */
			pathnode->path.rescannable = false;
			return (Path *) pathnode;
		}

		/* No motion needed if subpath can run anywhere giving same output. */
		if (CdbPathLocus_IsGeneral(subpath->locus))
		{
			/*
			 * general-->(entry|singleqe), no motion is needed, can run
			 * directly on any of the common segments
			 */
			return subpath;
		}

		/* Fail if caller refuses motion. */
		if (require_existing_order &&
			!pathkeys)
			return NULL;

		/*
		 * Must be partitioned-->singleton. If caller gave pathkeys, they'll
		 * be used for Merge Receive. If no pathkeys, Union Receive will
		 * arbitrarily interleave the rows from the subpath partitions in no
		 * special order.
		 */
		if (!CdbPathLocus_IsPartitioned(subpath->locus))
			goto invalid_motion_request;
	}

	/* Output from a single process to be distributed over a gang? */
	else if (CdbPathLocus_IsBottleneck(subpath->locus))
	{
		/* Must be bottleneck-->partitioned or bottleneck-->replicated */
		if (!CdbPathLocus_IsPartitioned(locus) &&
			!CdbPathLocus_IsReplicated(locus))
			goto invalid_motion_request;

		/* Fail if caller disallows motion. */
		if (require_existing_order &&
			!pathkeys)
			return NULL;

		/* Each qExec receives a subset of the rows, with ordering preserved. */
		pathkeys = subpath->pathkeys;
	}

	/* Redistributing partitioned subpath output from one gang to another? */
	else if (CdbPathLocus_IsPartitioned(subpath->locus))
	{
		/* partitioned-->partitioned? */
		if (CdbPathLocus_IsPartitioned(locus))
		{
			/* No motion if subpath partitioning matches caller's request. */
			if (cdbpathlocus_equal(subpath->locus, locus))
				return subpath;
		}

		/* Must be partitioned-->replicated */
		else if (!CdbPathLocus_IsReplicated(locus))
			goto invalid_motion_request;

		/* Fail if caller insists on ordered result or no motion. */
		if (require_existing_order)
			return NULL;

		/*
		 * Output streams lose any ordering they had. Only a qDisp or
		 * singleton qExec can merge sorted streams (for now).
		 */
		pathkeys = NIL;
	}

	/* If subplan uses no tables, it can run on qDisp or a singleton qExec. */
	else if (CdbPathLocus_IsGeneral(subpath->locus))
	{
		/*
		 * No motion needed if general-->general or general-->replicated or
		 * general-->segmentGeneral
		 */
		if (CdbPathLocus_IsGeneral(locus) ||
			CdbPathLocus_IsReplicated(locus) ||
			CdbPathLocus_IsSegmentGeneral(locus))
		{
			return subpath;
		}

		/* Must be general-->partitioned. */
		if (!CdbPathLocus_IsPartitioned(locus))
			goto invalid_motion_request;

		/* Fail if caller wants no motion. */
		if (require_existing_order &&
			!pathkeys)
			return NULL;

		/* Since the motion is 1-to-many, the rows remain in the same order. */
		pathkeys = subpath->pathkeys;
	}

	/* Does subpath produce same multiset of rows on every qExec of its gang? */
	else if (CdbPathLocus_IsReplicated(subpath->locus))
	{
		/* No-op if replicated-->replicated. */
		if (CdbPathLocus_IsReplicated(locus))
		{
			Assert(CdbPathLocus_NumSegments(locus) <=
				   CdbPathLocus_NumSegments(subpath->locus));
			subpath->locus.numsegments = CdbPathLocus_NumSegments(locus);
			return subpath;
		}

		/* Other destinations aren't used or supported at present. */
		goto invalid_motion_request;
	}

	/* Most motions from SegmentGeneral (replicated table) are disallowed */
	else if (CdbPathLocus_IsSegmentGeneral(subpath->locus))
	{
		/*
		 * The only allowed case is a SegmentGeneral to Hashed motion,
		 * and SegmentGeneral's numsegments is smaller than Hashed's.
		 * In such a case we redistribute SegmentGeneral to Hashed.
		 *
		 * FIXME: HashedOJ?
		 */
		if (CdbPathLocus_IsPartitioned(locus))
		{
			pathkeys = subpath->pathkeys;
		}
		else if (CdbPathLocus_IsReplicated(locus))
		{
			if (CdbPathLocus_NumSegments(locus) <= CdbPathLocus_NumSegments(subpath->locus))
			{
				subpath->locus.numsegments = CdbPathLocus_NumSegments(locus);
				return subpath;
			}
			else
			{
				pathkeys = subpath->pathkeys;
			}
		}
		else if (CdbPathLocus_IsSegmentGeneral(locus))
		{
			subpath->locus.numsegments = Min(subpath->locus.numsegments, locus.numsegments);
			return subpath;
		}
		else
			goto invalid_motion_request;
	}
	else
		goto invalid_motion_request;

	/* Don't materialize before motion. */
	if (IsA(subpath, MaterialPath))
		subpath = ((MaterialPath *) subpath)->subpath;

	/*
	 * MPP-3300: materialize *before* motion can never help us, motion pushes
	 * data. other nodes pull. We relieve motion deadlocks by adding
	 * materialize nodes on top of motion nodes
	 */


    if(IsA(subpath,SubqueryScanPath)
       && CdbPathLocus_IsBottleneck(locus)
       && !subpath->pathkeys
       && ((SubqueryScanPath *)subpath)->subpath->pathkeys)
    {
        /*
         * In gpdb, when there is a Gather Motion on top of a SubqueryScan,
         * it is hard to keep the sort order information. The subquery's
         * path key cannot be pulled up, if the parent query doesn't have
         * an equivalence class corresponding to the subquery's sort order.
         *
         * e.g. create a view with an ORDER BY:
         *
         * CREATE VIEW v AS SELECT va, vn FROM sourcetable ORDER BY vn;
         *
         * and query it:
         *
         * SELECT va FROM v_sourcetable;
         *
         * So, push down the Gather Motion if the SubqueryScan dose not 
         * have pathkey but the SubqueryScan's subpath does.
         *
         */
        SubqueryScanPath *subqueryScanPath = (SubqueryScanPath *)subpath;
        SubqueryScanPath *newSubqueryScanPath = NULL;
        Path *motionPath = NULL;

        subpath = subqueryScanPath->subpath;

        motionPath = cdbpath_create_motion_path(root,
                                                subpath,
                                                subpath->pathkeys,
                                                true,
                                                locus);

        newSubqueryScanPath = create_subqueryscan_path(root,
                                                       subqueryScanPath->path.parent,
                                                       motionPath,
                                                       subqueryScanPath->path.pathkeys,
                                                       locus,
                                                       subqueryScanPath->required_outer);

	/* apply final path target */
        newSubqueryScanPath = (SubqueryScanPath *)apply_projection_to_path(root,
                                                                           subqueryScanPath->path.parent,
                                                                           (Path *) newSubqueryScanPath,
                                                                           subqueryScanPath->path.pathtarget);

        return (Path *) newSubqueryScanPath;
    }

	/* Create CdbMotionPath node. */
	pathnode = makeNode(CdbMotionPath);
	pathnode->path.pathtype = T_Motion;
	pathnode->path.parent = subpath->parent;
	/* Motion doesn't project, so use source path's pathtarget */
	pathnode->path.pathtarget = subpath->pathtarget;
	pathnode->path.locus = locus;
	pathnode->path.rows = subpath->rows;
	pathnode->path.pathkeys = pathkeys;

	/* GPDB_96_MERGE_FIXME: When is a Motion path parallel-safe? I tried
	 * setting this to 'false' initially, to play it safe, but somehow
	 * the Paths with motions ended up in gather plans anyway, and tripped
	 * assertion failures.
	 */
	pathnode->path.parallel_aware = false;
	pathnode->path.parallel_safe = subpath->parallel_safe;
	pathnode->path.parallel_workers = subpath->parallel_workers;

	pathnode->subpath = subpath;
	pathnode->is_explicit_motion = false;

	/* Cost of motion */
	cdbpath_cost_motion(root, pathnode);

	/* Tell operators above us that slack may be needed for deadlock safety. */
	pathnode->path.motionHazard = true;
	pathnode->path.rescannable = false;

	/*
	 * A motion to bring data to the outer query's locus needs a Material node
	 * on top, to shield the Motion node from rescanning, when the SubPlan
	 * is rescanned.
	 */
	if (CdbPathLocus_IsOuterQuery(locus))
	{
		return (Path *) create_material_path(root, subpath->parent,
											 &pathnode->path);
	}

	return (Path *) pathnode;

	/* Unexpected source or destination locus. */
invalid_motion_request:
	elog(ERROR, "could not build Motion path");
	return NULL;
}								/* cdbpath_create_motion_path */

Path *
cdbpath_create_explicit_motion_path(PlannerInfo *root,
									Path *subpath,
									CdbPathLocus locus)
{
	CdbMotionPath *pathnode;

	/* Create CdbMotionPath node. */
	pathnode = makeNode(CdbMotionPath);
	pathnode->path.pathtype = T_Motion;
	pathnode->path.parent = subpath->parent;
	/* Motion doesn't project, so use source path's pathtarget */
	pathnode->path.pathtarget = subpath->pathtarget;
	pathnode->path.locus = locus;
	pathnode->path.rows = subpath->rows;
	pathnode->path.pathkeys = NIL;

	/* GPDB_96_MERGE_FIXME: When is a Motion path parallel-safe? I tried
	 * setting this to 'false' initially, to play it safe, but somehow
	 * the Paths with motions ended up in gather plans anyway, and tripped
	 * assertion failures.
	 */
	pathnode->path.parallel_aware = false;
	pathnode->path.parallel_safe = subpath->parallel_safe;
	pathnode->path.parallel_workers = subpath->parallel_workers;

	pathnode->subpath = subpath;
	pathnode->is_explicit_motion = true;

	/* Cost of motion */
	cdbpath_cost_motion(root, pathnode);

	/* Tell operators above us that slack may be needed for deadlock safety. */
	pathnode->path.motionHazard = true;
	pathnode->path.rescannable = false;

	return (Path *) pathnode;
}

Path *
cdbpath_create_broadcast_motion_path(PlannerInfo *root,
									 Path *subpath,
									 int numsegments)
{
	CdbMotionPath *pathnode;

	/* Create CdbMotionPath node. */
	pathnode = makeNode(CdbMotionPath);
	pathnode->path.pathtype = T_Motion;
	pathnode->path.parent = subpath->parent;
	/* Motion doesn't project, so use source path's pathtarget */
	pathnode->path.pathtarget = subpath->pathtarget;
	CdbPathLocus_MakeReplicated(&pathnode->path.locus, numsegments);
	pathnode->path.rows = subpath->rows;
	pathnode->path.pathkeys = NIL;

	/* GPDB_96_MERGE_FIXME: When is a Motion path parallel-safe? I tried
	 * setting this to 'false' initially, to play it safe, but somehow
	 * the Paths with motions ended up in gather plans anyway, and tripped
	 * assertion failures.
	 */
	pathnode->path.parallel_aware = false;
	pathnode->path.parallel_safe = subpath->parallel_safe;
	pathnode->path.parallel_workers = subpath->parallel_workers;

	pathnode->subpath = subpath;
	pathnode->is_explicit_motion = false;

	/* Cost of motion */
	cdbpath_cost_motion(root, pathnode);

	/* Tell operators above us that slack may be needed for deadlock safety. */
	pathnode->path.motionHazard = true;
	pathnode->path.rescannable = false;

	return (Path *) pathnode;
}

/*
 */
static CdbMotionPath *
make_motion_path(PlannerInfo *root, Path *subpath,
				 CdbPathLocus locus,
				 bool is_explicit_motion,
				 GpPolicy *policy)
{
	CdbMotionPath *pathnode;

	/* Create CdbMotionPath node. */
	pathnode = makeNode(CdbMotionPath);
	pathnode->path.pathtype = T_Motion;
	pathnode->path.parent = subpath->parent;
	/* Motion doesn't project, so use source path's pathtarget */
	pathnode->path.pathtarget = subpath->pathtarget;
	pathnode->path.locus = locus;
	pathnode->path.rows = subpath->rows;
	pathnode->path.pathkeys = NIL;

	/* GPDB_96_MERGE_FIXME: When is a Motion path parallel-safe? I tried
	 * setting this to 'false' initially, to play it safe, but somehow
	 * the Paths with motions ended up in gather plans anyway, and tripped
	 * assertion failures.
	 */
	pathnode->path.parallel_aware = false;
	pathnode->path.parallel_safe = subpath->parallel_safe;
	pathnode->path.parallel_workers = subpath->parallel_workers;

	pathnode->subpath = subpath;

	pathnode->is_explicit_motion = is_explicit_motion;
	pathnode->policy = policy;

	/* Cost of motion */
	cdbpath_cost_motion(root, pathnode);

	/* Tell operators above us that slack may be needed for deadlock safety. */
	pathnode->path.motionHazard = true;
	pathnode->path.rescannable = false;

	return pathnode;
}

/*
 * cdbpath_match_preds_to_partkey_tail
 *
 * Returns true if all of the locus's partitioning key expressions are
 * found as comparands of equijoin predicates in the mergeclause_list.
 *
 * NB: for mergeclause_list and pathkey structure assumptions, see:
 *          select_mergejoin_clauses() in joinpath.c
 *          find_mergeclauses_for_pathkeys() in pathkeys.c
 */

typedef struct
{
	PlannerInfo *root;
	List	   *mergeclause_list;
	Path       *path;
	CdbPathLocus locus;
	CdbPathLocus *colocus;
	bool		colocus_eq_locus;
} CdbpathMatchPredsContext;


/*
 * A helper function to create a DistributionKey for an EquivalenceClass.
 */
static DistributionKey *
makeDistributionKeyForEC(EquivalenceClass *eclass, Oid opfamily)
{
	DistributionKey *dk = makeNode(DistributionKey);

	Assert(OidIsValid(opfamily));

	dk->dk_eclasses = list_make1(eclass);
	dk->dk_opfamily = opfamily;

	return dk;
}

/*
 * cdbpath_eclass_constant_is_hashable
 *
 * Iterates through a list of equivalence class members and determines if
 * expression in pseudoconstant is hashable under the given hash opfamily.
 *
 * If there are multiple constants in the equivalence class, it is sufficient
 * that one of them is usable.
 */
static bool
cdbpath_eclass_constant_is_hashable(EquivalenceClass *ec, Oid hashOpFamily)
{
	ListCell   *j;

	foreach(j, ec->ec_members)
	{
		EquivalenceMember *em = (EquivalenceMember *) lfirst(j);

		if (!em->em_is_const)
			continue;

		if (get_opfamily_member(hashOpFamily, em->em_datatype, em->em_datatype,
								HTEqualStrategyNumber))
			return true;
	}

	return false;
}

static bool
cdbpath_match_preds_to_distkey_tail(CdbpathMatchPredsContext *ctx,
									ListCell *distkeycell)
{
	DistributionKey *distkey = (DistributionKey *) lfirst(distkeycell);
	DistributionKey *codistkey;
	ListCell   *cell;
	ListCell   *rcell;

	Assert(CdbPathLocus_IsHashed(ctx->locus) ||
		   CdbPathLocus_IsHashedOJ(ctx->locus));

	/*----------------
	 * Is there a "<distkey item> = <constant expr>" predicate?
	 *
	 * If table T is distributed on cols (C,D,E) and query contains preds
	 *		T.C = U.A AND T.D = <constant expr> AND T.E = U.B
	 * then we would like to report a match and return the colocus
	 * 		(U.A, <constant expr>, U.B)
	 * so the caller can join T and U by redistributing only U.
	 * (Note that "T.D = <constant expr>" won't be in the mergeclause_list
	 * because it isn't a join pred.)
	 *----------------
	 */
	codistkey = NULL;

	foreach(cell, distkey->dk_eclasses)
	{
		EquivalenceClass *ec = (EquivalenceClass *) lfirst(cell);

		if (CdbEquivClassIsConstant(ec) &&
			cdbpath_eclass_constant_is_hashable(ec, distkey->dk_opfamily))
		{
			codistkey = distkey;
			break;
		}
	}

	/* Look for an equijoin comparison to the distkey item. */
	if (!codistkey)
	{
		foreach(rcell, ctx->mergeclause_list)
		{
			EquivalenceClass *a_ec; /* Corresponding to ctx->path. */
			EquivalenceClass *b_ec;
			ListCell   *i;
			RestrictInfo *rinfo = (RestrictInfo *) lfirst(rcell);

			update_mergeclause_eclasses(ctx->root, rinfo);

			if (bms_is_subset(rinfo->right_relids, ctx->path->parent->relids))
			{
				a_ec = rinfo->right_ec;
				b_ec = rinfo->left_ec;
			}
			else
			{
				a_ec = rinfo->left_ec;
				b_ec = rinfo->right_ec;
				Assert(bms_is_subset(rinfo->left_relids, ctx->path->parent->relids));
			}

			foreach(i, distkey->dk_eclasses)
			{
				EquivalenceClass *dk_eclass = (EquivalenceClass *) lfirst(i);

				if (dk_eclass == a_ec)
					codistkey = makeDistributionKeyForEC(b_ec, distkey->dk_opfamily); /* break earlier? */
			}

			if (codistkey)
				break;
		}
	}

	/* Fail if didn't find a match for this distkey item. */
	if (!codistkey)
		return false;

	/* Might need to build co-locus if locus is outer join source or result. */
	if (codistkey != lfirst(distkeycell))
		ctx->colocus_eq_locus = false;

	/* Match remaining partkey items. */
	distkeycell = lnext(distkeycell);
	if (distkeycell)
	{
		if (!cdbpath_match_preds_to_distkey_tail(ctx, distkeycell))
			return false;
	}

	/* Success!  Matched all items.  Return co-locus if requested. */
	if (ctx->colocus)
	{
		if (ctx->colocus_eq_locus)
			*ctx->colocus = ctx->locus;
		else if (!distkeycell)
			CdbPathLocus_MakeHashed(ctx->colocus, list_make1(codistkey),
									CdbPathLocus_NumSegments(ctx->locus));
		else
		{
			ctx->colocus->distkey = lcons(codistkey, ctx->colocus->distkey);
			Assert(cdbpathlocus_is_valid(*ctx->colocus));
		}
	}
	return true;
}								/* cdbpath_match_preds_to_partkey_tail */



/*
 * cdbpath_match_preds_to_partkey
 *
 * Returns true if an equijoin predicate is found in the mergeclause_list
 * for each item of the locus's partitioning key.
 *
 * (Also, a partkey item that is equal to a constant is treated as a match.)
 *
 * Readers may refer also to these related functions:
 *          select_mergejoin_clauses() in joinpath.c
 *          find_mergeclauses_for_pathkeys() in pathkeys.c
 */
static bool
cdbpath_match_preds_to_distkey(PlannerInfo *root,
							   List *mergeclause_list,
							   Path *path,
							   CdbPathLocus locus,
							   CdbPathLocus *colocus)	/* OUT */
{
	CdbpathMatchPredsContext ctx;

	if (!CdbPathLocus_IsHashed(locus) &&
		!CdbPathLocus_IsHashedOJ(locus))
		return false;

	Assert(cdbpathlocus_is_valid(locus));

	ctx.root = root;
	ctx.mergeclause_list = mergeclause_list;
	ctx.path = path;
	ctx.locus = locus;
	ctx.colocus = colocus;
	ctx.colocus_eq_locus = true;

	return cdbpath_match_preds_to_distkey_tail(&ctx, list_head(locus.distkey));
}


/*
 * cdbpath_match_preds_to_both_distkeys
 *
 * Returns true if the mergeclause_list contains equijoin
 * predicates between each item of the outer_locus distkey and
 * the corresponding item of the inner_locus distkey.
 *
 * Readers may refer also to these related functions:
 *          select_mergejoin_clauses() in joinpath.c
 *          find_mergeclauses_for_pathkeys() in pathkeys.c
 */
static bool
cdbpath_match_preds_to_both_distkeys(PlannerInfo *root,
									 List *mergeclause_list,
									 CdbPathLocus outer_locus,
									 CdbPathLocus inner_locus)
{
	ListCell   *outercell;
	ListCell   *innercell;
	List	   *outer_distkey;
	List	   *inner_distkey;

	if (!mergeclause_list ||
		CdbPathLocus_NumSegments(outer_locus) != CdbPathLocus_NumSegments(inner_locus) ||
		outer_locus.distkey == NIL || inner_locus.distkey == NIL ||
		list_length(outer_locus.distkey) != list_length(inner_locus.distkey))
		return false;

	Assert(CdbPathLocus_IsHashed(outer_locus) ||
		   CdbPathLocus_IsHashedOJ(outer_locus));
	Assert(CdbPathLocus_IsHashed(inner_locus) ||
		   CdbPathLocus_IsHashedOJ(inner_locus));

	outer_distkey = outer_locus.distkey;
	inner_distkey = inner_locus.distkey;

	forboth(outercell, outer_distkey, innercell, inner_distkey)
	{
		DistributionKey *outer_dk = (DistributionKey *) lfirst(outercell);
		DistributionKey *inner_dk = (DistributionKey *) lfirst(innercell);
		ListCell   *rcell;

		if (outer_dk->dk_opfamily != inner_dk->dk_opfamily)
			return false;	/* incompatible hashing scheme */

		foreach(rcell, mergeclause_list)
		{
			bool		not_found = false;
			RestrictInfo *rinfo = (RestrictInfo *) lfirst(rcell);
			ListCell   *i;

			update_mergeclause_eclasses(root, rinfo);

			/* Skip predicate if neither side matches outer distkey item. */
			foreach(i, outer_dk->dk_eclasses)
			{
				EquivalenceClass *outer_ec = (EquivalenceClass *) lfirst(i);

				if (outer_ec != rinfo->left_ec && outer_ec != rinfo->right_ec)
				{
					not_found = true;
					break;
				}
			}
			if (not_found)
				continue;

			/* Skip predicate if neither side matches inner distkey item. */
			if (inner_dk != outer_dk)
			{
				ListCell   *i;

				foreach(i, inner_dk->dk_eclasses)
				{
					EquivalenceClass *inner_ec = (EquivalenceClass *) lfirst(i);

					if (inner_ec != rinfo->left_ec && inner_ec != rinfo->right_ec)
					{
						not_found = true;
						break;
					}
				}
				if (not_found)
					continue;
			}

			/* Found equijoin between outer distkey item & inner distkey item */
			break;
		}

		/* Fail if didn't find equijoin between this pair of distkey items. */
		if (!rcell)
			return false;
	}
	return true;
}								/* cdbpath_match_preds_to_both_distkeys */

/*
 * cdbpath_distkeys_from_preds
 *
 * Makes a CdbPathLocus for repartitioning, driven by
 * the equijoin predicates in the mergeclause_list (a List of RestrictInfo).
 * Returns true if successful, or false if no usable equijoin predicates.
 *
 * Readers may refer also to these related functions:
 *      select_mergejoin_clauses() in joinpath.c
 *      make_pathkeys_for_mergeclauses() in pathkeys.c
 */
static bool
cdbpath_distkeys_from_preds(PlannerInfo *root,
							List *mergeclause_list,
							Path *a_path,
							int numsegments,
							CdbPathLocus *a_locus,	/* OUT */
							CdbPathLocus *b_locus)	/* OUT */
{
	List	   *a_distkeys = NIL;
	List	   *b_distkeys = NIL;
	ListCell   *rcell;

	foreach(rcell, mergeclause_list)
	{
		RestrictInfo *rinfo = (RestrictInfo *) lfirst(rcell);
		Oid			lhs_opno;
		Oid			rhs_opno;
		Oid			opfamily;

		update_mergeclause_eclasses(root, rinfo);

		/*
		 * skip non-hashable keys
		 */
		if (!rinfo->hashjoinoperator)
			continue;

		/*
		 * look up a hash operator family that is compatible for the left and right datatypes
		 * of the hashjoin = operator
		 */
		if (!get_compatible_hash_operators_and_family(rinfo->hashjoinoperator,
													  &lhs_opno, &rhs_opno, &opfamily))
			continue;

		/* Left & right pathkeys are usually the same... */
		if (!b_distkeys && rinfo->left_ec == rinfo->right_ec)
		{
			ListCell   *i;
			bool        found = false;

			foreach(i, a_distkeys)
			{
				DistributionKey *distkey = (DistributionKey *) lfirst(i);
				EquivalenceClass *dk_eclass;

				/*
				 * we only create Hashed DistributionKeys with a single eclass
				 * in this function.
				 */
				Assert(list_length(distkey->dk_eclasses) == 1);
				dk_eclass = (EquivalenceClass *) linitial(distkey->dk_eclasses);

				if (dk_eclass == rinfo->left_ec)
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				DistributionKey *a_dk = makeDistributionKeyForEC(rinfo->left_ec, opfamily);
				a_distkeys = lappend(a_distkeys, a_dk);
			}
		}

		/* ... except in outer join ON-clause. */
		else
		{
			EquivalenceClass *a_ec;
			EquivalenceClass *b_ec;
			ListCell   *i;
			bool		found = false;

			if (bms_is_subset(rinfo->right_relids, a_path->parent->relids))
			{
				a_ec = rinfo->right_ec;
				b_ec = rinfo->left_ec;
			}
			else
			{
				a_ec = rinfo->left_ec;
				b_ec = rinfo->right_ec;
				Assert(bms_is_subset(rinfo->left_relids, a_path->parent->relids));
			}

			if (!b_ec)
				b_ec = a_ec;

			/*
			 * Convoluted logic to ensure that (a_ec not in a_distkeys) AND
			 * (b_ec not in b_distkeys)
			 */
			found = false;
			foreach(i, a_distkeys)
			{
				DistributionKey *distkey = (DistributionKey *) lfirst(i);
				EquivalenceClass *dk_eclass;

				/*
				 * we only create Hashed DistributionKeys with a single eclass
				 * in this function.
				 */
				Assert(list_length(distkey->dk_eclasses) == 1);
				dk_eclass = (EquivalenceClass *) linitial(distkey->dk_eclasses);

				if (dk_eclass == a_ec)
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				foreach(i, b_distkeys)
				{
					DistributionKey *distkey = (DistributionKey *) lfirst(i);
					EquivalenceClass *dk_eclass;

					/*
					 * we only create Hashed DistributionKeys with a single eclass
					 * in this function.
					 */
					Assert(list_length(distkey->dk_eclasses) == 1);
					dk_eclass = (EquivalenceClass *) linitial(distkey->dk_eclasses);

					if (dk_eclass == b_ec)
					{
						found = true;
						break;
					}
				}
			}

			if (!found)
			{
				DistributionKey *a_dk = makeDistributionKeyForEC(a_ec, opfamily);
				DistributionKey *b_dk = makeDistributionKeyForEC(b_ec, opfamily);

				a_distkeys = lappend(a_distkeys, a_dk);
				b_distkeys = lappend(b_distkeys, b_dk);
			}
		}

		if (list_length(a_distkeys) >= 20)
			break;
	}

	if (!a_distkeys)
		return false;

	CdbPathLocus_MakeHashed(a_locus, a_distkeys, numsegments);
	if (b_distkeys)
		CdbPathLocus_MakeHashed(b_locus, b_distkeys, numsegments);
	else
		*b_locus = *a_locus;
	return true;
}								/* cdbpath_distkeys_from_preds */

/*
 * Add a RowIdExpr to the target list of 'path'. Returns the ID
 * of the generated rowid expression in *rowidexpr_id.
 */
static Path *
add_rowid_to_path(PlannerInfo *root, Path *path, int *rowidexpr_id)
{
	RowIdExpr *rowidexpr;
	PathTarget *newpathtarget;

	/*
	 * 'last_rowidexpr_id' is used to generate a unique ID for the RowIdExpr
	 * node that we generate. It only needs to be unique within this query
	 * plan, and the simplest way to achieve that is to just have a global
	 * counter. (Actually, it isn't really needed at the moment because the
	 * deduplication is always done immediately on top of the join, so two
	 * different RowIdExprs should never appear in the same part of the plan
	 * tree. But it might come handy when debugging, if nothing else.
	 * XXX: If we start to rely on it for something important, consider
	 * overflow behavior more carefully.)
	 */
	static uint32 last_rowidexpr_id = 0;

	rowidexpr = makeNode(RowIdExpr);
	last_rowidexpr_id++;

	*rowidexpr_id = rowidexpr->rowidexpr_id = (int) last_rowidexpr_id;

	newpathtarget = copy_pathtarget(path->pathtarget);
	add_column_to_pathtarget(newpathtarget, (Expr *) rowidexpr, 0);

	return (Path *) create_projection_path(root, path->parent, path, newpathtarget);
}

/*
 * cdbpath_motion_for_join
 *
 * Decides where a join should be done.  Adds Motion operators atop
 * the subpaths if needed to deliver their results to the join locus.
 * Returns the join locus if ok, or a null locus otherwise. If
 * jointype is JOIN_SEMI_DEDUP or JOIN_SEMI_DEDUP_REVERSE, this also
 * tacks a RowIdExpr on one side of the join, and *p_rowidexpr_id is
 * set to the ID of that. The caller is expected to uniquefy
 * the result after the join, passing the rowidexpr_id to
 * create_unique_rowid_path().
 *
 * mergeclause_list is a List of RestrictInfo.  Its members are
 * the equijoin predicates between the outer and inner rel.
 * It comes from select_mergejoin_clauses() in joinpath.c.
 */
CdbPathLocus
cdbpath_motion_for_join(PlannerInfo *root,
						JoinType jointype,	/* JOIN_INNER/FULL/LEFT/RIGHT/IN */
						Path **p_outer_path,	/* INOUT */
						Path **p_inner_path,	/* INOUT */
						int *p_rowidexpr_id,	/* OUT */
						List *redistribution_clauses, /* equijoin RestrictInfo list */
						List *restrict_clauses,
						List *outer_pathkeys,
						List *inner_pathkeys,
						bool outer_require_existing_order,
						bool inner_require_existing_order)
{
	CdbpathMfjRel outer;
	CdbpathMfjRel inner;
	int			numsegments;
	bool		join_quals_contain_outer_references;
	ListCell   *lc;

	*p_rowidexpr_id = 0;

	outer.pathkeys = outer_pathkeys;
	inner.pathkeys = inner_pathkeys;
	outer.path = *p_outer_path;
	inner.path = *p_inner_path;
	outer.locus = outer.path->locus;
	inner.locus = inner.path->locus;
	CdbPathLocus_MakeNull(&outer.move_to);
	CdbPathLocus_MakeNull(&inner.move_to);

	Assert(cdbpathlocus_is_valid(outer.locus));
	Assert(cdbpathlocus_is_valid(inner.locus));

	/*
	 * Does the join quals contain references to outer query? If so, we must
	 * evaluate them in the outer query's locus. That means pulling both
	 * inputs to outer locus, and performing the join there.
	 *
	 * XXX: If there are pseudoconstant quals, they will be executed by a
	 * gating Result with a One-Time Filter. In that case, the join's inputs
	 * wouldn't need to be brought to the outer locus. We could execute the
	 * join normally, and bring the result to the outer locus and put the
	 * gating Result above the Motion, instead. But for now, we're not smart
	 * like that.
	 */
	join_quals_contain_outer_references = false;
	foreach(lc, restrict_clauses)
	{
		RestrictInfo *rinfo = (RestrictInfo *) lfirst(lc);

		if (rinfo->contain_outer_query_references)
		{
			join_quals_contain_outer_references = true;
			break;
		}
	}

	/*
	 * Locus type Replicated can only be generated by join operation.
	 * And in the function cdbpathlocus_join there is a rule:
	 * <any locus type> join <Replicated> => any locus type
	 * Proof by contradiction, it shows that when code arrives here,
	 * is is impossible that any of the two input paths' locus
	 * is Replicated. So we add two asserts here.
	 */
	Assert(!CdbPathLocus_IsReplicated(outer.locus));
	Assert(!CdbPathLocus_IsReplicated(inner.locus));

	if (CdbPathLocus_IsReplicated(outer.locus) ||
		CdbPathLocus_IsReplicated(inner.locus))
		goto fail;

	outer.has_wts = cdbpath_contains_wts(outer.path);
	inner.has_wts = cdbpath_contains_wts(inner.path);

	/* For now, inner path should not contain WorkTableScan */
	Assert(!inner.has_wts);

	/*
	 * If outer rel contains WorkTableScan and inner rel is hash distributed,
	 * unfortunately we have to pretend that inner is randomly distributed,
	 * otherwise we may end up with redistributing outer rel.
	 */
	if (outer.has_wts && inner.locus.distkey != NIL)
		CdbPathLocus_MakeStrewn(&inner.locus,
								CdbPathLocus_NumSegments(inner.locus));

	/*
	 * Caller can specify an ordering for each source path that is the same as
	 * or weaker than the path's existing ordering. Caller may insist that we
	 * do not add motion that would lose the specified ordering property;
	 * otherwise the given ordering is preferred but not required. A required
	 * NIL ordering means no motion is allowed for that path.
	 */
	outer.require_existing_order = outer_require_existing_order;
	inner.require_existing_order = inner_require_existing_order;

	/*
	 * Don't consider replicating the preserved rel of an outer join, or the
	 * current-query rel of a join between current query and subquery.
	 *
	 * Path that contains WorkTableScan cannot be replicated.
	 */
	/* ok_to_replicate means broadcast */
	outer.ok_to_replicate = !outer.has_wts;
	inner.ok_to_replicate = true;
	switch (jointype)
	{
		case JOIN_INNER:
		case JOIN_UNIQUE_OUTER:
		case JOIN_UNIQUE_INNER:
			break;
		case JOIN_SEMI:
		case JOIN_ANTI:
		case JOIN_LEFT:
		case JOIN_LASJ_NOTIN:
			outer.ok_to_replicate = false;
			break;
		case JOIN_RIGHT:
			inner.ok_to_replicate = false;
			break;
		case JOIN_FULL:
			outer.ok_to_replicate = false;
			inner.ok_to_replicate = false;
			break;

		case JOIN_DEDUP_SEMI:

			/*
			 * In this plan type, we generate a unique row ID on the outer
			 * side of the join, perform the join, possibly broadcasting the
			 * outer side, and remove duplicates after the join, so that only
			 * one row for each input outer row remains.
			 *
			 * If the outer input is General or SegmentGeneral, it's available
			 * in all the segments, but we cannot reliably generate a row ID
			 * to distinguish each logical row in that case. So force the
			 * input to a single node first in that case.
			 *
			 * In previous Greenplum versions, we assumed that we can generate
			 * a unique row ID for General paths, by generating the same
			 * sequence of numbers on each segment. That works as long as the
			 * rows are in the same order on each segment, but it seemed like
			 * a risky assumption. And it didn't work on SegmentGeneral paths
			 * (i.e. replicated tables) anyway.
			 */
			if (!CdbPathLocus_IsPartitioned(inner.locus))
				goto fail;

			if (CdbPathLocus_IsPartitioned(outer.locus) ||
				CdbPathLocus_IsBottleneck(outer.locus))
			{
				/* ok */
			}
			else if (CdbPathLocus_IsGeneral(outer.locus))
			{
				CdbPathLocus_MakeSingleQE(&outer.locus,
										  CdbPathLocus_NumSegments(inner.locus));
				outer.path->locus = outer.locus;

			}
			else if (CdbPathLocus_IsSegmentGeneral(outer.locus))
			{
				CdbPathLocus_MakeSingleQE(&outer.locus,
										  CdbPathLocus_CommonSegments(inner.locus,
																	  outer.locus));
				outer.path->locus = outer.locus;
			}
			else
				goto fail;
			inner.ok_to_replicate = false;
			outer.path = add_rowid_to_path(root, outer.path, p_rowidexpr_id);
			*p_outer_path = outer.path;
			break;

		case JOIN_DEDUP_SEMI_REVERSE:
			/* same as JOIN_DEDUP_SEMI, but with inner and outer reversed */
			if (!CdbPathLocus_IsPartitioned(outer.locus))
				goto fail;
			if (CdbPathLocus_IsPartitioned(inner.locus) ||
				CdbPathLocus_IsBottleneck(inner.locus))
			{
				/* ok */
			}
			else if (CdbPathLocus_IsGeneral(inner.locus))
			{
				CdbPathLocus_MakeSingleQE(&inner.locus,
										  CdbPathLocus_NumSegments(outer.locus));
				inner.path->locus = inner.locus;
			}
			else if (CdbPathLocus_IsSegmentGeneral(inner.locus))
			{
				CdbPathLocus_MakeSingleQE(&inner.locus,
										  CdbPathLocus_CommonSegments(outer.locus,
																	  inner.locus));
				inner.path->locus = inner.locus;
			}
			else
				goto fail;
			outer.ok_to_replicate = false;
			inner.path = add_rowid_to_path(root, inner.path, p_rowidexpr_id);
			*p_inner_path = inner.path;
			break;

		default:

			/*
			 * The caller should already have transformed
			 * JOIN_UNIQUE_INNER/OUTER into JOIN_INNER
			 */
			elog(ERROR, "unexpected join type %d", jointype);
	}

	/* Get rel sizes. */
	outer.bytes = outer.path->rows * outer.path->pathtarget->width;
	inner.bytes = inner.path->rows * inner.path->pathtarget->width;

	if (join_quals_contain_outer_references)
	{
		if (CdbPathLocus_IsOuterQuery(outer.locus) &&
			CdbPathLocus_IsOuterQuery(inner.locus))
			return outer.locus;

		if (!CdbPathLocus_IsOuterQuery(outer.locus))
			CdbPathLocus_MakeOuterQuery(&outer.move_to);
		if (!CdbPathLocus_IsOuterQuery(inner.locus))
			CdbPathLocus_MakeOuterQuery(&inner.move_to);
	}
	else if (CdbPathLocus_IsOuterQuery(outer.locus) ||
			 CdbPathLocus_IsOuterQuery(inner.locus))
	{
		/*
		 * If one side of the join has "outer query" locus, must bring the
		 * other side there too.
		 */
		if (CdbPathLocus_IsOuterQuery(outer.locus) &&
			CdbPathLocus_IsOuterQuery(inner.locus))
			return outer.locus;

		if (CdbPathLocus_IsOuterQuery(outer.locus))
			inner.move_to = outer.locus;
		else
			outer.move_to = inner.locus;
	}
	else if (CdbPathLocus_IsGeneral(outer.locus) ||
			 CdbPathLocus_IsGeneral(inner.locus))
	{
		/*
		 * Motion not needed if either source is everywhere (e.g. a constant).
		 *
		 * But if a row is everywhere and is preserved in an outer join, we don't
		 * want to preserve it in every qExec process where it is unmatched,
		 * because that would produce duplicate null-augmented rows. So in that
		 * case, bring all the partitions to a single qExec to be joined. CDB
		 * TODO: Can this case be handled without introducing a bottleneck?
		 */
		/*
		 * The logic for the join result's locus is (outer's locus is general):
		 *   1. if outer is ok to replicated, then result's locus is the same
		 *      as inner's locus
		 *   2. if outer is not ok to replicated (like left join or wts cases)
		 *      2.1 if inner's locus is hashed or hashOJ, we try to redistribute
		 *          outer as the inner, if fails, make inner singleQE
		 *      2.2 if inner's locus is strewn, we try to redistribute
		 *          outer and inner, if fails, make inner singleQE
		 *      2.3 just return the inner's locus, no motion is needed
		 */
		CdbpathMfjRel *general = &outer;
		CdbpathMfjRel *other = &inner;

		/*
		 * both are general, the result is general
		 */
		if (CdbPathLocus_IsGeneral(outer.locus) &&
			CdbPathLocus_IsGeneral(inner.locus))
			return outer.locus;

		if (CdbPathLocus_IsGeneral(inner.locus))
		{
			general = &inner;
			other = &outer;
		}

		/* numsegments of General locus is always -1 */
		Assert(CdbPathLocus_NumSegments(general->locus) == -1);

		/*
		 * If general can happen everywhere (ok_to_replicate)
		 * then it acts like identity: 
		 *     General join other_locus => other_locus
		 */
		if (general->ok_to_replicate)
			return other->locus;

		if (!CdbPathLocus_IsPartitioned(other->locus))
		{
			/*
			 * If general is not ok_to_replicate, for example,
			 * generate_series(1, 10) left join xxxx, only for
			 * some specific locus types general can act as
			 * identity:
			 *    General join other_locus => other_locus, if and only if
			 *    other_locus in (singleQE, Entry).
			 * Here other's locus:
			 *    - cannot be general (it has already handled)
			 *    - cannot be replicated (assert at the beginning of the function)
			 */
			Assert(CdbPathLocus_IsBottleneck(other->locus) ||
				   CdbPathLocus_IsSegmentGeneral(other->locus));
			return other->locus;
		}
		/*
		 * If other's locus is partitioned, we first try to
		 * add redistribute motion, if fails, we gather other
		 * to singleQE.
		 */
		else if (!try_redistribute(root, general, other, redistribution_clauses))
		{
			/*
			 * FIXME: do we need test other's movable?
			 */
			CdbPathLocus_MakeSingleQE(&other->move_to,
									  CdbPathLocus_NumSegments(other->locus));
		}
	}
	else if (CdbPathLocus_IsSegmentGeneral(outer.locus) ||
			 CdbPathLocus_IsSegmentGeneral(inner.locus))
	{
		/*
		 * the whole branch handles the case that at least
		 * one of the two locus is SegmentGeneral. The logic
		 * is:
		 *   - if both are SegmentGeneral:
		 *       1. if both locus are equal, no motion needed, simply return
		 *       2. For update cases. If resultrelation
		 *          is SegmentGeneral, the update must execute
		 *          on each segment of the resultrelation, if resultrelation's
		 *          numsegments is larger, the only solution is to broadcast
		 *          other
		 *       3. no motion is needed, change both numsegments to common
		 *   - if only one of them is SegmentGeneral :
		 *       1. consider update case, if resultrelation is SegmentGeneral,
		 *          the only solution is to broadcast the other
		 *       2. if other's locus is singleQE or entry, make SegmentGeneral
		 *          to other's locus
		 *       3. the remaining possibility of other's locus is partitioned
		 *          3.1 if SegmentGeneral is not ok_to_replicate, try to
		 *              add redistribute motion, if fails gather each to
		 *              singleQE
		 *          3.2 if SegmentGeneral's numsegments is larger, just return
		 *              other's locus
		 *          3.3 try to add redistribute motion, if fails, gather each
		 *              to singleQE
		 */
		CdbpathMfjRel *segGeneral;
		CdbpathMfjRel *other;

		if (CdbPathLocus_IsSegmentGeneral(outer.locus) &&
			CdbPathLocus_IsSegmentGeneral(inner.locus))
		{
			/*
			 * use_common to indicate whether we should
			 * return a segmentgeneral locus with common
			 * numsegments.
			 */
			bool use_common = true;
			/*
			 * Handle the case two same locus
			 */
			if (CdbPathLocus_NumSegments(outer.locus) ==
				CdbPathLocus_NumSegments(inner.locus))
				return inner.locus;
			/*
			 * Now, two locus' numsegments not equal
			 * We should consider update resultrelation
			 * if update,
			 *   - resultrelation's numsegments larger, then
			 *     we should broadcast the other
			 *   - otherwise, results is common
			 * else:
			 *   common
			 */
			if (root->upd_del_replicated_table > 0)
			{
				if ((CdbPathLocus_NumSegments(outer.locus) >
					 CdbPathLocus_NumSegments(inner.locus)) &&
					bms_is_member(root->upd_del_replicated_table,
								  outer.path->parent->relids))
				{
					/*
					 * the updated resultrelation is replicated table
					 * and its numsegments is larger, we should broadcast
					 * the other path
					 */
					if (!inner.ok_to_replicate)
						goto fail;

					/*
					 * FIXME: do we need to test inner's movable?
					 */
					CdbPathLocus_MakeReplicated(&inner.move_to,
												CdbPathLocus_NumSegments(outer.locus));
					use_common = false;
				}
				else if ((CdbPathLocus_NumSegments(outer.locus) <
						  CdbPathLocus_NumSegments(inner.locus)) &&
						 bms_is_member(root->upd_del_replicated_table,
									   inner.path->parent->relids))
				{
					/*
					 * the updated resultrelation is replicated table
					 * and its numsegments is larger, we should broadcast
					 * the other path
					 */
					if (!outer.ok_to_replicate)
						goto fail;

					/*
					 * FIXME: do we need to test outer's movable?
					 */
					CdbPathLocus_MakeReplicated(&outer.move_to,
												CdbPathLocus_NumSegments(inner.locus));
					use_common = false;
				}
			}
			
			if (use_common)
			{
				/*
				 * The statement is not update a replicated table.
				 * Just return the segmentgeneral with a smaller numsegments.
				 */
				numsegments = CdbPathLocus_CommonSegments(inner.locus,
														  outer.locus);
				outer.locus.numsegments = numsegments;
				inner.locus.numsegments = numsegments;

				return inner.locus;
			}
		}
		else
		{
			if (CdbPathLocus_IsSegmentGeneral(outer.locus))
			{
				segGeneral = &outer;
				other = &inner;
			}
			else
			{
				segGeneral = &inner;
				other = &outer;
			}

			Assert(CdbPathLocus_IsBottleneck(other->locus) ||
				   CdbPathLocus_IsPartitioned(other->locus));
			
			/*
			 * For UPDATE/DELETE, replicated table can't guarantee a logic row has
			 * same ctid or item pointer on each copy. If we broadcast matched tuples
			 * to all segments, the segments may update the wrong tuples or can't
			 * find a valid tuple according to ctid or item pointer.
			 *
			 * So For UPDATE/DELETE on replicated table, we broadcast other path so
			 * all target tuples can be selected on all copys and then be updated
			 * locally.
			 */
			if (root->upd_del_replicated_table > 0 &&
				bms_is_member(root->upd_del_replicated_table,
							  segGeneral->path->parent->relids))
			{
				/*
				 * For UPDATE on a replicated table, we have to do it
				 * everywhere so that for each segment, we have to collect
				 * all the information of other that is we should broadcast it
				 */
				
				/*
				 * FIXME: do we need to test other's movable?
				 */
				CdbPathLocus_MakeReplicated(&other->move_to,
											CdbPathLocus_NumSegments(segGeneral->locus));
			}
			else if (CdbPathLocus_IsBottleneck(other->locus))
			{
				/*
				 * if the locus type is equal and segment count is unequal,
				 * we will dispatch the one on more segments to the other
				 */
				numsegments = CdbPathLocus_CommonSegments(segGeneral->locus,
														  other->locus);
				segGeneral->move_to = other->locus;
				segGeneral->move_to.numsegments = numsegments;
			}
			else
			{
				/*
				 * This branch handles for partitioned other locus
				 * hashed, hashoj, strewn
				 */
				Assert(CdbPathLocus_IsPartitioned(other->locus));
				
				if (!segGeneral->ok_to_replicate)
				{
					if (!try_redistribute(root, segGeneral,
										  other, redistribution_clauses))
					{
						/*
						 * FIXME: do we need to test movable?
						 */
						CdbPathLocus_MakeSingleQE(&segGeneral->move_to,
												  CdbPathLocus_NumSegments(segGeneral->locus));
						CdbPathLocus_MakeSingleQE(&other->move_to,
												  CdbPathLocus_NumSegments(other->locus));
					}
				}
				else
				{
					/*
					 * If all other's segments have segGeneral stored, then no motion
					 * is needed.
					 *
					 * A sql to reach here:
					 *     select * from d2 a join r1 b using (c1);
					 * where d2 is a replicated table on 2 segment,
					 *       r1 is a random table on 1 segments.
					 */
					if (CdbPathLocus_NumSegments(segGeneral->locus) >=
						CdbPathLocus_NumSegments(other->locus))
						return other->locus;
					else
					{
						if (!try_redistribute(root, segGeneral,
											  other, redistribution_clauses))
						{
							numsegments = CdbPathLocus_CommonSegments(segGeneral->locus,
																	  other->locus);
							/*
							 * FIXME: do we need to test movable?
							 */
							CdbPathLocus_MakeSingleQE(&segGeneral->move_to, numsegments);
							CdbPathLocus_MakeSingleQE(&other->move_to, numsegments);
						}
					}
				}
			}
		}
	}
	/*
	 * Is either source confined to a single process? NB: Motion to a single
	 * process (qDisp or qExec) is the only motion in which we may use Merge
	 * Receive to preserve an existing ordering.
	 */
	else if (CdbPathLocus_IsBottleneck(outer.locus) ||
			 CdbPathLocus_IsBottleneck(inner.locus))
	{							/* singleQE or entry db */
		CdbpathMfjRel *single = &outer;
		CdbpathMfjRel *other = &inner;
		bool		single_immovable = (outer.require_existing_order &&
										!outer_pathkeys) || outer.has_wts;
		bool		other_immovable = inner.require_existing_order &&
		!inner_pathkeys;

		/*
		 * If each of the sources has a single-process locus, then assign both
		 * sources and the join to run in the same process, without motion.
		 * The slice will be run on the entry db if either source requires it.
		 */
		if (CdbPathLocus_IsEntry(single->locus))
		{
			if (CdbPathLocus_IsBottleneck(other->locus))
				return single->locus;
		}
		else if (CdbPathLocus_IsSingleQE(single->locus))
		{
			if (CdbPathLocus_IsBottleneck(other->locus))
			{
				/*
				 * Can join directly on one of the common segments.
				 */
				numsegments = CdbPathLocus_CommonSegments(outer.locus,
														  inner.locus);

				other->locus.numsegments = numsegments;
				return other->locus;
			}
		}

		/* Let 'single' be the source whose locus is singleQE or entry. */
		else
		{
			CdbSwap(CdbpathMfjRel *, single, other);
			CdbSwap(bool, single_immovable, other_immovable);
		}

		Assert(CdbPathLocus_IsBottleneck(single->locus));
		Assert(CdbPathLocus_IsPartitioned(other->locus));

		/* If the bottlenecked rel can't be moved, bring the other rel to it. */
		if (single_immovable)
		{
			if (other_immovable)
				goto fail;
			else
				other->move_to = single->locus;
		}

		/* Redistribute single rel if joining on other rel's partitioning key */
		else if (cdbpath_match_preds_to_distkey(root,
												redistribution_clauses,
												other->path,
												other->locus,
												&single->move_to))	/* OUT */
		{
			AssertEquivalent(CdbPathLocus_NumSegments(other->locus),
							 CdbPathLocus_NumSegments(single->move_to));
		}

		/* Replicate single rel if cheaper than redistributing both rels. */
		else if (single->ok_to_replicate &&
				 (single->bytes * CdbPathLocus_NumSegments(other->locus) <
				  single->bytes + other->bytes))
			CdbPathLocus_MakeReplicated(&single->move_to,
										CdbPathLocus_NumSegments(other->locus));

		/*
		 * Redistribute both rels on equijoin cols.
		 *
		 * Redistribute both to the same segments, here we choose the
		 * same segments with other.
		 */
		else if (!other_immovable &&
				 cdbpath_distkeys_from_preds(root,
											 redistribution_clauses,
											 single->path,
											 CdbPathLocus_NumSegments(other->locus),
											 &single->move_to,	/* OUT */
											 &other->move_to))	/* OUT */
		{
			/* ok */
		}

		/* Broadcast single rel for below cases. */
		else if (single->ok_to_replicate &&
				 (other_immovable ||
				  single->bytes < other->bytes ||
				  other->has_wts))
			CdbPathLocus_MakeReplicated(&single->move_to,
										CdbPathLocus_NumSegments(other->locus));

		/* Last resort: If possible, move all partitions of other rel to single QE. */
		else if (!other_immovable)
			other->move_to = single->locus;
		else
			goto fail;
	}							/* singleQE or entry */

	/*
	 * No motion if partitioned alike and joining on the partitioning keys.
	 */
	else if (cdbpath_match_preds_to_both_distkeys(root, redistribution_clauses,
												  outer.locus, inner.locus))
		return cdbpathlocus_join(jointype, outer.locus, inner.locus);

	/*
	 * Both sources are partitioned.  Redistribute or replicate one or both.
	 */
	else
	{							/* partitioned */
		CdbpathMfjRel *large_rel = &outer;
		CdbpathMfjRel *small_rel = &inner;

		/* Which rel is bigger? */
		if (large_rel->bytes < small_rel->bytes)
			CdbSwap(CdbpathMfjRel *, large_rel, small_rel);

		/* Both side are distribued in 1 segment, it can join without motion. */
		if (CdbPathLocus_NumSegments(large_rel->locus) == 1 &&
			CdbPathLocus_NumSegments(small_rel->locus) == 1)
			return large_rel->locus;

		/* If joining on larger rel's partitioning key, redistribute smaller. */
		if (!small_rel->require_existing_order &&
			cdbpath_match_preds_to_distkey(root,
										   redistribution_clauses,
										   large_rel->path,
										   large_rel->locus,
										   &small_rel->move_to))	/* OUT */
		{
			AssertEquivalent(CdbPathLocus_NumSegments(large_rel->locus),
							 CdbPathLocus_NumSegments(small_rel->move_to));
		}

		/*
		 * Replicate smaller rel if cheaper than redistributing larger rel.
		 * But don't replicate a rel that is to be preserved in outer join.
		 */
		else if (!small_rel->require_existing_order &&
				 small_rel->ok_to_replicate &&
				 (small_rel->bytes * CdbPathLocus_NumSegments(large_rel->locus) <
				  large_rel->bytes))
			CdbPathLocus_MakeReplicated(&small_rel->move_to,
										CdbPathLocus_NumSegments(large_rel->locus));

		/*
		 * Replicate larger rel if cheaper than redistributing smaller rel.
		 * But don't replicate a rel that is to be preserved in outer join.
		 */
		else if (!large_rel->require_existing_order &&
				 large_rel->ok_to_replicate &&
				 (large_rel->bytes * CdbPathLocus_NumSegments(small_rel->locus) <
				  small_rel->bytes))
			CdbPathLocus_MakeReplicated(&large_rel->move_to,
										CdbPathLocus_NumSegments(small_rel->locus));

		/* If joining on smaller rel's partitioning key, redistribute larger. */
		else if (!large_rel->require_existing_order &&
				 cdbpath_match_preds_to_distkey(root,
												redistribution_clauses,
												small_rel->path,
												small_rel->locus,
												&large_rel->move_to))	/* OUT */
		{
			AssertEquivalent(CdbPathLocus_NumSegments(small_rel->locus),
							 CdbPathLocus_NumSegments(large_rel->move_to));
		}

		/* Replicate smaller rel if cheaper than redistributing both rels. */
		else if (!small_rel->require_existing_order &&
				 small_rel->ok_to_replicate &&
				 (small_rel->bytes * CdbPathLocus_NumSegments(large_rel->locus) <
				  small_rel->bytes + large_rel->bytes))
			CdbPathLocus_MakeReplicated(&small_rel->move_to,
										CdbPathLocus_NumSegments(large_rel->locus));

		/* Replicate largeer rel if cheaper than redistributing both rels. */
		else if (!large_rel->require_existing_order &&
				 large_rel->ok_to_replicate &&
				 (large_rel->bytes * CdbPathLocus_NumSegments(small_rel->locus) <
				  large_rel->bytes + small_rel->bytes))
			CdbPathLocus_MakeReplicated(&large_rel->move_to,
										CdbPathLocus_NumSegments(small_rel->locus));

		/*
		 * Redistribute both rels on equijoin cols.
		 *
		 * the two results should all be distributed on the same segments,
		 * here we make them the same with common segments for safe
		 * TODO: how about distribute them both to ALL segments?
		 */
		else if (!small_rel->require_existing_order &&
				 !small_rel->has_wts &&
				 !large_rel->require_existing_order &&
				 !large_rel->has_wts &&
				 cdbpath_distkeys_from_preds(root,
											 redistribution_clauses,
											 large_rel->path,
											 CdbPathLocus_CommonSegments(large_rel->locus,
																		 small_rel->locus),
											 &large_rel->move_to,
											 &small_rel->move_to))
		{
			/* ok */
		}

		/*
		 * No usable equijoin preds, or couldn't consider the preferred
		 * motion. Replicate one rel if possible. MPP TODO: Consider number of
		 * seg dbs per host.
		 */
		else if (!small_rel->require_existing_order &&
				 small_rel->ok_to_replicate)
			CdbPathLocus_MakeReplicated(&small_rel->move_to,
										CdbPathLocus_NumSegments(large_rel->locus));
		else if (!large_rel->require_existing_order &&
				 large_rel->ok_to_replicate)
			CdbPathLocus_MakeReplicated(&large_rel->move_to,
										CdbPathLocus_NumSegments(small_rel->locus));

		/* Last resort: Move both rels to a single qExec. */
		else
		{
			int numsegments = CdbPathLocus_CommonSegments(outer.locus,
														  inner.locus);
			CdbPathLocus_MakeSingleQE(&outer.move_to, numsegments);
			CdbPathLocus_MakeSingleQE(&inner.move_to, numsegments);
		}
	}							/* partitioned */

	/*
	 * Move outer.
	 */
	if (!CdbPathLocus_IsNull(outer.move_to))
	{
		outer.path = cdbpath_create_motion_path(root,
												outer.path,
												outer_pathkeys,
												outer.require_existing_order,
												outer.move_to);
		if (!outer.path)		/* fail if outer motion not feasible */
			goto fail;

		if (IsA(outer.path, MaterialPath) && !root->config->may_rescan)
		{
			/*
			 * If we are the outer path and can never be rescanned,
			 * we could remove the materialize path.
			 */
			MaterialPath *mpath = (MaterialPath *) outer.path;
			outer.path = mpath->subpath;
		}
	}

	/*
	 * Move inner.
	 */
	if (!CdbPathLocus_IsNull(inner.move_to))
	{
		inner.path = cdbpath_create_motion_path(root,
												inner.path,
												inner_pathkeys,
												inner.require_existing_order,
												inner.move_to);
		if (!inner.path)		/* fail if inner motion not feasible */
			goto fail;
	}

	/*
	 * Ok to join.  Give modified subpaths to caller.
	 */
	*p_outer_path = outer.path;
	*p_inner_path = inner.path;

	/* Tell caller where the join will be done. */
	return cdbpathlocus_join(jointype, outer.path->locus, inner.path->locus);

fail:							/* can't do this join */
	CdbPathLocus_MakeNull(&outer.move_to);
	return outer.move_to;
}								/* cdbpath_motion_for_join */

/*
 * Does the path contain WorkTableScan?
 */
bool
cdbpath_contains_wts(Path *path)
{
	JoinPath   *joinPath;
	AppendPath *appendPath;
	ListCell   *lc;

	if (IsJoinPath(path))
	{
		joinPath = (JoinPath *) path;
		if (cdbpath_contains_wts(joinPath->outerjoinpath)
			|| cdbpath_contains_wts(joinPath->innerjoinpath))
			return true;
		else
			return false;
	}
	else if (IsA(path, AppendPath))
	{
		appendPath = (AppendPath *) path;
		foreach(lc, appendPath->subpaths)
		{
			if (cdbpath_contains_wts((Path *) lfirst(lc)))
				return true;
		}
		return false;
	}

	return path->pathtype == T_WorkTableScan;
}


/*
 * has_redistributable_clause
 *	  If the restrictinfo's clause is redistributable, return true.
 */
bool
has_redistributable_clause(RestrictInfo *restrictinfo)
{
	return restrictinfo->hashjoinoperator != InvalidOid;
}

/*
 * try_redistribute
 *     helper function for A join B when
 *     - A's locus is general or segmentgeneral
 *     - B's locus is partitioned
 *     it tries to redistribute A to B's locus
 *     or redistribute both A and B to the same
 *     partitioned locus.
 *
 *     return values:
 *     - true: redistributed motion has been added for A
 *     - false: cannot add redistributed motion, caller should
 *       continue to find other solutions.
 */
static bool
try_redistribute(PlannerInfo *root, CdbpathMfjRel *g, CdbpathMfjRel *o,
				 List *redistribution_clauses)
{
	bool g_immovable;
	bool o_immovable;

	Assert(CdbPathLocus_IsGeneral(g->locus) ||
		   CdbPathLocus_IsSegmentGeneral(g->locus));
	Assert(CdbPathLocus_IsPartitioned(o->locus));

	/*
	 * we cannot add motion if requiring order.
	 * has_wts can be true only for general locus
	 * otherwise, it is false and not impact the
	 * value of <x>_immovable.
	 */
	g_immovable = (g->require_existing_order &&
				   !g->pathkeys) || g->has_wts;

	/*
	 * if g cannot be added motion on,
	 * we should return immediately.
	 */
	if (g_immovable)
		return false;
	
	o_immovable = (o->require_existing_order &&
				   !o->pathkeys) || o->has_wts;

	if (CdbPathLocus_IsHashed(o->locus) ||
		CdbPathLocus_IsHashedOJ(o->locus))
	{
		/*
		 * first try to only redistribute g as o's locus
		 * if fails then try to redistribute both g and o
		 */
		if (cdbpath_match_preds_to_distkey(root,
										   redistribution_clauses,
										   o->path,
										   o->locus,
										   &g->move_to))
			return true;
		else
		{
			/*
			 * both g and o can be added motion on,
			 * we should try each possible case.
			 */
			int			numsegments;

			if (CdbPathLocus_IsGeneral(g->locus))
				numsegments = CdbPathLocus_NumSegments(o->locus);
			else
				numsegments = CdbPathLocus_CommonSegments(o->locus, g->locus);

			if(cdbpath_distkeys_from_preds(root,
										   redistribution_clauses,
										   o->path,
										   numsegments,
										   &o->move_to,
										   &g->move_to))
			{
				return true;
			}
		}
	}
	else
	{
		/*
		 * the only possible solution is to
		 * redistributed both g and o, so
		 * both g and o should be movable.
		 */
		int			numsegments;

		if (CdbPathLocus_IsGeneral(g->locus))
			numsegments = CdbPathLocus_NumSegments(o->locus);
		else
			numsegments = CdbPathLocus_CommonSegments(o->locus, g->locus);

		if (!o_immovable &&
			cdbpath_distkeys_from_preds(root,
										redistribution_clauses,
										o->path,
										numsegments,
										&o->move_to,
										&g->move_to))
		{
			return true;
		}
	}

	/*
	 * fail to redistribute, return false
	 * to let caller know.
	 */
	return false;
}

/*
 * Add a suitable Motion Path so that the input tuples from 'subpath' are
 * distributed correctly for insertion into target table.
 */
Path *
create_motion_path_for_ctas(PlannerInfo *root, GpPolicy *policy, Path *subpath)
{
	GpPolicyType	policyType = policy->ptype;

	if (policyType == POLICYTYPE_PARTITIONED && policy->nattrs == 0)
	{
		/*
		 * If the target table is DISTRIBUTED RANDOMLY, and the input data
		 * is already partitioned, we could let the insertions happen where
		 * they are. But to ensure more random distribution, redistribute.
		 * This is different from create_motion_path_for_insert().
		 */
		CdbPathLocus targetLocus;

		CdbPathLocus_MakeStrewn(&targetLocus, policy->numsegments);
		return cdbpath_create_motion_path(root, subpath, NIL, false, targetLocus);
	}
	else
		return create_motion_path_for_insert(root, policy, subpath);
}

/*
 * Add a suitable Motion Path so that the input tuples from 'subpath' are
 * distributed correctly for insertion into target table.
 */
Path *
create_motion_path_for_insert(PlannerInfo *root, GpPolicy *policy,
							  Path *subpath)
{
	GpPolicyType	policyType = policy->ptype;
	CdbPathLocus	targetLocus;

	if (policyType == POLICYTYPE_PARTITIONED)
	{
		/*
		 * A query to reach here: INSERT INTO t1 VALUES(1).
		 * There is no need to add a motion from General, we could
		 * simply put General on the same segments with target table.
		 */
		/* FIXME: also do this for other targetPolicyType? */
		/* FIXME: also do this for all the subplans */
		if (CdbPathLocus_IsGeneral(subpath->locus))
		{
			subpath->locus.numsegments = policy->numsegments;
		}

		targetLocus = cdbpathlocus_for_insert(root, policy, subpath->pathtarget);

		if (policy->nattrs == 0 && CdbPathLocus_IsPartitioned(subpath->locus))
		{
			/*
			 * If the target table is DISTRIBUTED RANDOMLY, we can insert the
			 * rows anywhere. So if the input path is already partitioned, let
			 * the insertions happen where they are.
			 *
			 * If you `explain` the query insert into tab_random select * from tab_partition
			 * there is not Motion node in plan. However, it is not means that the query only
			 * execute in entry db. It is dispatched to QE and do everything well as we expect.
			 *
			 * But, we need to grant a Motion node if target locus' segnumber is different with
			 * subpath.
			 */
			if(targetLocus.numsegments != subpath->locus.numsegments)
			{
				CdbPathLocus_MakeStrewn(&targetLocus, policy->numsegments);
				subpath = cdbpath_create_motion_path(root, subpath, NIL, false, targetLocus);
			}
		}
		else if (CdbPathLocus_IsNull(targetLocus))
		{
			/* could not create DistributionKeys to represent the distribution keys. */
			CdbPathLocus_MakeStrewn(&targetLocus, policy->numsegments);

			subpath = (Path *) make_motion_path(root, subpath, targetLocus, false, policy);
		}
		else
		{
			subpath = cdbpath_create_motion_path(root, subpath, NIL, false, targetLocus);
		}
	}
	else if (policyType == POLICYTYPE_ENTRY)
	{
		/*
		 * Query result needs to be brought back to the QD.
		 */
		CdbPathLocus_MakeEntry(&targetLocus);
		subpath = cdbpath_create_motion_path(root, subpath, NIL, false, targetLocus);
	}
	else if (policyType == POLICYTYPE_REPLICATED)
	{
		/* try to optimize insert with no motion introduced into */
		if (optimizer_replicated_table_insert &&
			!contain_volatile_functions((Node *)subpath->pathtarget->exprs))
		{
			/*
			 * CdbLocusType_SegmentGeneral is only used by replicated table
			 * right now, so if both input and target are replicated table,
			 * no need to add a motion.
			 *
			 * Also, to expand a replicated table to new segments, gpexpand
			 * force a data reorganization by a query like:
			 * CREATE TABLE tmp_tab AS SELECT * FROM source_table DISTRIBUTED REPLICATED
			 * Obviously, tmp_tab in new segments can't get data if we don't
			 * add a broadcast here.
			 */
			if(CdbPathLocus_IsSegmentGeneral(subpath->locus) &&
					subpath->locus.numsegments >= policy->numsegments)
			{
				/*
				 * A query to reach here:
				 *     INSERT INTO d1 SELECT * FROM d1;
				 * There is no need to add a motion from General, we
				 * could simply put General on the same segments with
				 * target table.
				 *
				 * Otherwise a broadcast motion is needed otherwise d2 will
				 * only have data on segment 0.
				 */
				subpath->locus.numsegments = policy->numsegments;
				return subpath;
			}

			/* plan's data are available on all segment, no motion needed */
			if(CdbPathLocus_IsGeneral(subpath->locus))
			{
				/*
				 * A query to reach here: INSERT INTO d1 VALUES(1).
				 * There is no need to add a motion from General, we
				 * could simply put General on the same segments with
				 * target table.
				 */
				subpath->locus.numsegments = Min(subpath->locus.numsegments,policy->numsegments) ;
				return subpath;
			}

		}
		subpath = cdbpath_create_broadcast_motion_path(root, subpath, policy->numsegments);
	}
	else
		elog(ERROR, "unrecognized policy type %u", policyType);
	return subpath;
}

/*
 * Add a suitable Motion Path for delete and update. If the UPDATE
 * modifies the distribution key columns, use create_split_update_path()
 * instead.
 */
Path *
create_motion_path_for_upddel(PlannerInfo *root, Index rti, GpPolicy *policy,
							  Path *subpath)
{
	GpPolicyType	policyType = policy->ptype;
	CdbPathLocus	targetLocus;

	if (policyType == POLICYTYPE_PARTITIONED)
	{
		if (can_elide_explicit_motion(root, rti, subpath, policy))
			return subpath;
		else
		{
			/* GPDB_96_MERGE_FIXME: avoid creating the Explicit Motion in
			 * simple cases, where all the input data is already on the
			 * same segment.
			 *
			 * Is "strewn" correct here? Can we do better?
			 */
			CdbPathLocus_MakeStrewn(&targetLocus, policy->numsegments);
			subpath = cdbpath_create_explicit_motion_path(root,
														  subpath,
														  targetLocus);
		}
	}
	else if (policyType == POLICYTYPE_ENTRY)
	{
		/* Master-only table */
		CdbPathLocus_MakeEntry(&targetLocus);
		subpath = cdbpath_create_motion_path(root, subpath, NIL, false, targetLocus);
	}
	else if (policyType == POLICYTYPE_REPLICATED)
	{
	}
	else
		elog(ERROR, "unrecognized policy type %u", policyType);

	return subpath;
}

/*
 * In Postgres planner, we add a SplitUpdate node at top so that updating on
 * distribution columns could be handled. The SplitUpdate will split each
 * update into delete + insert.
 *
 * There are several important points should be highlighted:
 *
 * First, in order to split each update operation into two operations,
 * delete + insert, we need several junk columns in the subplan's targetlist,
 * in addition to the row's new values:
 *
 * ctid            the tuple id used for deletion
 *
 * gp_segment_id   the segment that the row originates from. Usually the
 *                 current segment where the SplitUpdate runs, but not
 *                 necessarily, if there are multiple joins involved and the
 *                 planner decided redistribute the data.
 *
 * oid             if result relation has oids, the old OID, so that it can be
 *                 preserved in the new row.
 *
 * We will add one more column to the output, the "action". It's an integer
 * that indicates for each row, whether it represents the DELETE or the INSERT
 * of that row. It is generated by the Split Update node.
 *
 * Second, current GPDB executor don't support statement-level update triggers
 * and will skip row-level update triggers because a split-update is actually
 * consist of a delete and insert. So, if the result relation has update
 * triggers, we should reject and error out because it's not functional.
 *
 * GPDB_96_MERGE_FIXME: the below comment is obsolete. Nowadays, SplitUpdate
 * computes the new row's hash, and the corresponding. target segment. The
 * old segment comes from the gp_segment_id junk column. But ORCA still
 * does it the old way!
 *
 * Third, to support deletion, and hash delete operation to correct segment,
 * we need to get attributes of OLD tuple. The old attributes must therefore
 * be present in the subplan's target list. That is handled earlier in the
 * planner, in expand_targetlist().
 *
 * For example, a typical plan would be as following for statement:
 * update foo set id = l.v + 1 from dep l where foo.v = l.id:
 *
 * |-- join ( targetlist: [ l.v + 1, foo.v, foo.id, foo.ctid, foo.gp_segment_id ] )
 *       |
 *       |-- motion ( targetlist: [l.id, l.v] )
 *       |    |
 *       |    |-- seqscan on dep ....
 *       |
 *       |-- hash (targetlist [ v, foo.ctid, foo.gp_segment_id ] )
 *            |
 *            |-- seqscan on foo (targetlist: [ v, foo.id, foo.ctid, foo.gp_segment_id ] )
 *
 * From the plan above, the target foo.id is assigned as l.v + 1, and expand_targetlist()
 * ensured that the old value of id, is also available, even though it would not otherwise
 * be needed.
 *
 * 'rti' is the UPDATE target relation.
 */
Path *
create_split_update_path(PlannerInfo *root, Index rti, GpPolicy *policy, Path *subpath)
{
	GpPolicyType	policyType = policy->ptype;
	CdbPathLocus	targetLocus;

	if (policyType == POLICYTYPE_PARTITIONED)
	{
		/*
		 * If any of the distribution key columns are being changed,
		 * the UPDATE might move tuples from one segment to another.
		 * Create a Split Update node to deal with that.
		 *
		 * If the input is a dummy plan that cannot return any rows,
		 * e.g. because the input was eliminated by constraint
		 * exclusion, we can skip it.
		 */
		targetLocus = cdbpathlocus_for_insert(root, policy, subpath->pathtarget);

		subpath = (Path *) make_splitupdate_path(root, subpath, rti);
		subpath = cdbpath_create_explicit_motion_path(root,
													  subpath,
													  targetLocus);
	}
	else if (policyType == POLICYTYPE_ENTRY)
	{
		/* Master-only table */
		CdbPathLocus_MakeEntry(&targetLocus);
		subpath = cdbpath_create_motion_path(root, subpath, NIL, false, targetLocus);
	}
	else if (policyType == POLICYTYPE_REPLICATED)
	{
	}
	else
		elog(ERROR, "unrecognized policy type %u", policyType);
	return subpath;
}


static SplitUpdatePath *
make_splitupdate_path(PlannerInfo *root, Path *subpath, Index rti)
{
	RangeTblEntry  *rte;
	PathTarget		*splitUpdatePathTarget;
	SplitUpdatePath	*splitupdatepath;
	DMLActionExpr	*actionExpr;

	/* Suppose we already hold locks before caller */
	rte = planner_rt_fetch(rti, root);

	/* GPDB_96_MERGE_FIXME: Why is this not allowed? */
	if (has_update_triggers(rte->relid))
		ereport(ERROR,
				(errcode(ERRCODE_GP_FEATURE_NOT_YET),
				 errmsg("UPDATE on distributed key column not allowed on relation with update triggers")));

	/* Add action column at the end of targetlist */
	actionExpr = makeNode(DMLActionExpr);
	splitUpdatePathTarget = copy_pathtarget(subpath->pathtarget);
	add_column_to_pathtarget(splitUpdatePathTarget, (Expr *) actionExpr, 0);

	/* populate information generated above into splitupdate node */
	splitupdatepath = makeNode(SplitUpdatePath);
	splitupdatepath->path.pathtype = T_SplitUpdate;
	splitupdatepath->path.parent = subpath->parent;
	splitupdatepath->path.pathtarget = splitUpdatePathTarget;
	splitupdatepath->path.param_info = NULL;
	splitupdatepath->path.parallel_aware = false;
	splitupdatepath->path.parallel_safe = subpath->parallel_safe;
	splitupdatepath->path.parallel_workers = subpath->parallel_workers;
	splitupdatepath->path.rows = 2 * subpath->rows;
	splitupdatepath->path.startup_cost = subpath->startup_cost;
	splitupdatepath->path.total_cost = subpath->total_cost;
	splitupdatepath->path.pathkeys = subpath->pathkeys;
	splitupdatepath->path.locus = subpath->locus;
	splitupdatepath->subpath = subpath;
	splitupdatepath->resultRelation = rti;

	return splitupdatepath;
}

static bool
can_elide_explicit_motion(PlannerInfo *root, Index rti, Path *subpath,
						  GpPolicy *policy)
{
	/*
	 * If there are no Motions between scan of the target relation and here,
	 * no motion is required.
	 */
	if (bms_is_member(rti, subpath->sameslice_relids))
		return true;

	if (!CdbPathLocus_IsStrewn(subpath->locus))
	{
		CdbPathLocus    resultrelation_locus = cdbpathlocus_from_policy(root, rti, policy);
		return cdbpathlocus_equal(subpath->locus, resultrelation_locus);
	}

	return false;
}
