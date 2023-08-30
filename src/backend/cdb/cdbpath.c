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
#include "access/htup_details.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "nodes/makefuncs.h"	/* makeFuncExpr() */
#include "nodes/relation.h"		/* PlannerInfo, RelOptInfo */
#include "optimizer/cost.h"		/* cpu_tuple_cost */
#include "optimizer/pathnode.h" /* Path, pathnode_walker() */
#include "optimizer/paths.h"
#include "optimizer/planmain.h"

#include "parser/parse_expr.h"	/* exprType() */
#include "parser/parse_oper.h"

#include "utils/catcache.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

#include "cdb/cdbdef.h"			/* CdbSwap() */
#include "cdb/cdbhash.h"
#include "cdb/cdbpath.h"		/* me */
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"


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
 *      specified ordering or if path has parameterization relids,
 *      which generally may indicate we can't add motion;
 *      also NULL is returned if pathkeys is NIL
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
	int numsegments;

	Assert(cdbpathlocus_is_valid(locus) &&
		   cdbpathlocus_is_valid(subpath->locus));

	numsegments = CdbPathLocus_CommonSegments(subpath->locus, locus);
	Assert(numsegments > 0);

	/*
	 * Motion is to change path's locus, if target locus is the
	 * same as the subpath's, there is no need to add motion.
	 */
	if (cdbpathlocus_equal(subpath->locus, locus))
		return subpath;

	/* Moving subpath output to a single executor process (qDisp or qExec)? */
	if (CdbPathLocus_IsBottleneck(locus))
	{
		/* entry-->entry?  No motion needed. */
		if (CdbPathLocus_IsEntry(subpath->locus) &&
			CdbPathLocus_IsEntry(locus))
		{
			subpath->locus.numsegments = getgpsegmentCount();
			return subpath;
		}
		/* singleQE-->singleQE?  No motion needed. */
		if (CdbPathLocus_IsSingleQE(subpath->locus) &&
			CdbPathLocus_IsSingleQE(locus))
		{
			subpath->locus.numsegments = numsegments;
			return subpath;
		}

		/* entry-->singleQE?  Don't move.  Slice's QE will run on entry db. */
		if (CdbPathLocus_IsEntry(subpath->locus))
		{
			subpath->locus.numsegments = numsegments;
			return subpath;
		}

		/* No motion needed if subpath can run anywhere giving same output. */
		if (CdbPathLocus_IsGeneral(subpath->locus))
		{
			/*
			 * general-->(entry|singleqe), no motion is needed, can run
			 * directly on any of the common segments
			 */
			subpath->locus.numsegments = numsegments;
			return subpath;
		}

		/* Fail if caller refuses motion. */
		if (require_existing_order &&
			!pathkeys)
			return NULL;

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
			pathnode->path.locus = locus;
			pathnode->path.rows = subpath->rows;
			pathnode->path.pathkeys = pathkeys;
			pathnode->subpath = subpath;

			Assert(pathnode->path.locus.numsegments > 0);

			/* Costs, etc, are same as subpath. */
			pathnode->path.startup_cost = subpath->total_cost;
			pathnode->path.total_cost = subpath->total_cost;
			pathnode->path.memory = subpath->memory;
			pathnode->path.motionHazard = subpath->motionHazard;

			/* Motion nodes are never rescannable. */
			pathnode->path.rescannable = false;
			return (Path *) pathnode;
		}

		if (CdbPathLocus_IsSegmentGeneral(subpath->locus))
		{
			/*
			 * If the subpath requires parameters, we cannot generate Motion atop of it.
			 * See Github Issue 13532 for details.
			 */
			if (!bms_is_empty(PATH_REQ_OUTER(subpath)))
				return NULL;

			/*
			 * Data is only available on segments, to distingush it with
			 * CdbLocusType_General, adding a motion to indicated this
			 * slice must be executed on a singleton gang.
			 *
			 * This motion may be redundant for segmentGeneral --> singleQE
			 * if the singleQE is not promoted to executed on qDisp in the
			 * end, so in apply_motion_mutator(), we will omit it.
			 */
			pathnode = makeNode(CdbMotionPath);
			pathnode->path.pathtype = T_Motion;
			pathnode->path.parent = subpath->parent;
			pathnode->path.locus = locus;
			pathnode->path.rows = subpath->rows;
			pathnode->path.pathkeys = pathkeys;
			pathnode->subpath = subpath;

			Assert(pathnode->path.locus.numsegments > 0);

			/* Costs, etc, are same as subpath. */
			pathnode->path.startup_cost = subpath->total_cost;
			pathnode->path.total_cost = subpath->total_cost;
			pathnode->path.memory = subpath->memory;
			pathnode->path.motionHazard = subpath->motionHazard;

			/* Motion nodes are never rescannable. */
			pathnode->path.rescannable = false;
			return (Path *) pathnode;
		}

		/* replicated-->singleton would give redundant copies of the rows. */
		if (CdbPathLocus_IsReplicated(subpath->locus))
			goto invalid_motion_request;

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
			subpath->locus.numsegments = numsegments;
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
			subpath->locus.numsegments = numsegments;
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
		if (CdbPathLocus_IsHashed(locus) &&
			(CdbPathLocus_NumSegments(locus) >
			 CdbPathLocus_NumSegments(subpath->locus)))
		{
			pathkeys = subpath->pathkeys;
		}
		else if (CdbPathLocus_IsReplicated(locus))
		{
			/*
			 * Assume that this case only can be generated in
			 * UPDATE/DELETE statement
			 */
			if (root->upd_del_replicated_table == 0)
				goto invalid_motion_request;

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

	/* Create CdbMotionPath node. */
	pathnode = makeNode(CdbMotionPath);
	pathnode->path.pathtype = T_Motion;
	pathnode->path.parent = subpath->parent;
	pathnode->path.locus = locus;
	pathnode->path.rows = subpath->rows;
	pathnode->path.pathkeys = pathkeys;
	pathnode->subpath = subpath;

	/* Cost of motion */
	cdbpath_cost_motion(root, pathnode);

	/* Tell operators above us that slack may be needed for deadlock safety. */
	pathnode->path.motionHazard = true;
	pathnode->path.rescannable = false;

	return (Path *) pathnode;

	/* Unexpected source or destination locus. */
invalid_motion_request:
	Assert(0);
	elog(WARNING, "cdbpath_create_motion_path can't apply valid motion");
	return NULL;
}								/* cdbpath_create_motion_path */

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

	/*
	 * Callers of this functions must correct numsegments themselves
	 */

	CdbPathLocus_MakeHashed(a_locus, a_distkeys, GP_POLICY_INVALID_NUMSEGMENTS());
	if (b_distkeys)
		CdbPathLocus_MakeHashed(b_locus, b_distkeys, GP_POLICY_INVALID_NUMSEGMENTS());
	else
		*b_locus = *a_locus;
	return true;
}								/* cdbpath_distkeys_from_preds */


/*
 * cdbpath_motion_for_join
 *
 * Decides where a join should be done.  Adds Motion operators atop
 * the subpaths if needed to deliver their results to the join locus.
 * Returns the join locus if ok, or a null locus otherwise.
 *
 * mergeclause_list is a List of RestrictInfo.  Its members are
 * the equijoin predicates between the outer and inner rel.
 * It comes from select_mergejoin_clauses() in joinpath.c.
 *
 * Besides ordering logic,
 * outer_require_existing_order and inner_require_existing_order
 * indicates path has parameterization relids, so we can decide
 * can we add a motion node atop of processing node
 */

typedef struct
{
	CdbPathLocus locus;
	CdbPathLocus move_to;
	double		bytes;
	Path	   *path;
	bool		ok_to_replicate;
	bool		require_existing_order;
	bool		has_wts;		/* Does the rel have WorkTableScan? */
} CdbpathMfjRel;

CdbPathLocus
cdbpath_motion_for_join(PlannerInfo *root,
						JoinType jointype,	/* JOIN_INNER/FULL/LEFT/RIGHT/IN */
						Path **p_outer_path,	/* INOUT */
						Path **p_inner_path,	/* INOUT */
						List *redistribution_clauses, /* equijoin RestrictInfo list */
						List *outer_pathkeys,
						List *inner_pathkeys,
						bool outer_require_existing_order,
						bool inner_require_existing_order)
{
	CdbpathMfjRel outer;
	CdbpathMfjRel inner;
	int			numsegments;

	outer.path = *p_outer_path;
	inner.path = *p_inner_path;
	outer.locus = outer.path->locus;
	inner.locus = inner.path->locus;
	CdbPathLocus_MakeNull(&outer.move_to,
						  CdbPathLocus_NumSegments(outer.path->locus));
	CdbPathLocus_MakeNull(&inner.move_to,
						  CdbPathLocus_NumSegments(inner.path->locus));

	Assert(cdbpathlocus_is_valid(outer.locus));
	Assert(cdbpathlocus_is_valid(inner.locus));

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
		default:
			/*
			 * The caller should already have transformed JOIN_UNIQUE_INNER/OUTER
			 * and JOIN_DEDUP_SEMI/SEMI_REVERSE into JOIN_INNER
			 */
			elog(ERROR, "unexpected join type %d", jointype);
	}

	/* Get rel sizes. */
	outer.bytes = outer.path->rows * outer.path->parent->width;
	inner.bytes = inner.path->rows * inner.path->parent->width;

	/*
	 * Motion not needed if either source is everywhere (e.g. a constant).
	 *
	 * But if a row is everywhere and is preserved in an outer join, we don't
	 * want to preserve it in every qExec process where it is unmatched,
	 * because that would produce duplicate null-augmented rows. So in that
	 * case, bring all the partitions to a single qExec to be joined. CDB
	 * TODO: Can this case be handled without introducing a bottleneck?
	 */
	if (CdbPathLocus_IsGeneral(outer.locus))
	{
		if (!outer.ok_to_replicate &&
			CdbPathLocus_IsPartitioned(inner.locus))
			CdbPathLocus_MakeSingleQE(&inner.move_to,
									  CdbPathLocus_NumSegments(inner.locus));
		else if (!outer.ok_to_replicate &&
			CdbPathLocus_IsSegmentGeneral(inner.locus))
			CdbPathLocus_MakeSingleQE(&inner.move_to,
									  CdbPathLocus_NumSegments(inner.locus));
		else
			return inner.locus;
	}
	else if (CdbPathLocus_IsGeneral(inner.locus))
	{
		if (!inner.ok_to_replicate &&
			CdbPathLocus_IsPartitioned(outer.locus))
			CdbPathLocus_MakeSingleQE(&outer.move_to,
									  CdbPathLocus_NumSegments(outer.locus));
		else if (!inner.ok_to_replicate &&
			CdbPathLocus_IsSegmentGeneral(outer.locus))
			CdbPathLocus_MakeSingleQE(&outer.move_to,
									  CdbPathLocus_NumSegments(outer.locus));
		else
			return outer.locus;
	}
	else if (CdbPathLocus_IsSegmentGeneral(outer.locus) ||
			 CdbPathLocus_IsSegmentGeneral(inner.locus))
	{
		CdbpathMfjRel *segGeneral;
		CdbpathMfjRel *other;

		if (CdbPathLocus_IsEqual(outer.locus, inner.locus))
			return inner.locus;
		else if (CdbPathLocus_IsSegmentGeneral(outer.locus) &&
				 CdbPathLocus_IsSegmentGeneral(inner.locus))
		{
			/*
			 * In the UPDATE/DELETE statement, if the result(target) relation
			 * is replicated table, we can not generate a sub-plan as
			 * SELECT statement, becuase we have to UPDATE/DELETE the tuple
			 * on all segments
			 *
			 * The JOIN operator is generated by 'UPDATE/DELETE ... FROM ...',
			 * so we can assume that the join type is inner-join:
			 *   a) if the outer table have 'wts', it can not add broadcast motion
			 *     directly.
			 *   b) We can sure that inner table have no 'wts'.
			 */
			if (root->upd_del_replicated_table > 0 &&
				bms_is_member(root->upd_del_replicated_table, inner.path->parent->relids) &&
				CdbPathLocus_NumSegments(inner.locus) > CdbPathLocus_NumSegments(outer.locus))
			{
				if (!outer.ok_to_replicate)
					goto fail;

				CdbPathLocus_MakeReplicated(&outer.move_to,
											CdbPathLocus_NumSegments(inner.locus));
			}
			else if (root->upd_del_replicated_table > 0 &&
					 bms_is_member(root->upd_del_replicated_table, outer.path->parent->relids) &&
					 CdbPathLocus_NumSegments(inner.locus) < CdbPathLocus_NumSegments(outer.locus))
			{
				Assert(inner.ok_to_replicate);

				CdbPathLocus_MakeReplicated(&inner.move_to,
											CdbPathLocus_NumSegments(outer.locus));
			}
			else
			{
				/*
				 * Can join directly on one of the common segments.
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

			if (CdbPathLocus_IsReplicated(other->locus))
			{
				Assert(root->upd_del_replicated_table > 0);

				/*
				 * It only appear when we UPDATE a replicated table.
				 * All the segment which replicated table storaged must execute
				 * the plan to delete tuple on himself, so if the segments count
				 * of broadcast(locus is Replicated) if less than the replicated
				 * table, we can not execute the plan correctly.
				 *
				 * TODO:Can we modify(or add) the broadcast motion for this case?
				 */
				Assert(CdbPathLocus_NumSegments(segGeneral->locus) <=
					   CdbPathLocus_NumSegments(other->locus));

				/*
				 * Only need to broadcast other to the segments of the
				 * replicated table.
				 */
				if (CdbPathLocus_NumSegments(segGeneral->locus) <
					CdbPathLocus_NumSegments(other->locus))
				{
					other->locus.numsegments =
							CdbPathLocus_NumSegments(segGeneral->locus);
				}

				return other->locus;
			}

			Assert(CdbPathLocus_IsBottleneck(other->locus) ||
				   CdbPathLocus_IsSegmentGeneral(other->locus) ||
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
				bms_is_member(root->upd_del_replicated_table, segGeneral->path->parent->relids))
			{
				CdbPathLocus_MakeReplicated(&other->move_to,
											CdbPathLocus_NumSegments(segGeneral->locus));
			}
			/*
			 * other is bottleneck, move inner to other
			 */
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
			else if (!segGeneral->ok_to_replicate)
			{
				int numsegments = CdbPathLocus_CommonSegments(segGeneral->locus,
															  other->locus);
				/* put both inner and outer to single QE */
				CdbPathLocus_MakeSingleQE(&segGeneral->move_to, numsegments);
				CdbPathLocus_MakeSingleQE(&other->move_to, numsegments);
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
				{
					return other->locus;
				}

				/*
				 * Otherwise there is some segments where other is on but
				 * segGeneral is not, in such a case motions are needed.
				 */

				/*
				 * For the case that other is a Hashed table and redistribute
				 * clause matches other's distribute keys, we could redistribute
				 * segGeneral to other.
				 */
				if (CdbPathLocus_IsHashed(other->locus) &&
					cdbpath_match_preds_to_distkey(root,
												   redistribution_clauses,
												   other->path,
												   other->locus,
												   &segGeneral->move_to))    /* OUT */
				{
					/* the result is distributed on the same segments with other */
					AssertEquivalent(CdbPathLocus_NumSegments(other->locus),
									 CdbPathLocus_NumSegments(segGeneral->move_to));
				}
				/*
				 * Otherwise gather both of them to a SingleQE, this is not usually
				 * a best choice as the SingleQE might be on QD, so although the
				 * overall cost is low it increases the load on QD.
				 *
				 * FIXME: is it possible to only gather other to segGeneral?
				 */
				else
				{
					int numsegments = CdbPathLocus_NumSegments(segGeneral->locus);

					Assert(CdbPathLocus_NumSegments(segGeneral->locus) <
						   CdbPathLocus_NumSegments(other->locus));

					CdbPathLocus_MakeSingleQE(&segGeneral->move_to, numsegments);
					CdbPathLocus_MakeSingleQE(&other->move_to, numsegments);
				}
			}
		}
	}
	/*
	 * Replicated paths shouldn't occur except UPDATE/DELETE on replicated table.
	 */
	else if (CdbPathLocus_IsReplicated(outer.locus))
	{
		if (root->upd_del_replicated_table > 0)
			CdbPathLocus_MakeReplicated(&inner.move_to,
										CdbPathLocus_NumSegments(outer.locus));
		else
		{
			Assert(false);
			goto fail;
		}
	}
	else if (CdbPathLocus_IsReplicated(inner.locus))
	{
		if (root->upd_del_replicated_table > 0)
			CdbPathLocus_MakeReplicated(&outer.move_to,
										CdbPathLocus_NumSegments(inner.locus));
		else
		{
			Assert(false);
			goto fail;
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

		/* Redistribute both rels on equijoin cols. */
		else if (!other_immovable &&
				 cdbpath_distkeys_from_preds(root,
											 redistribution_clauses,
											 single->path,
											 &single->move_to,	/* OUT */
											 &other->move_to))	/* OUT */
		{
			/*
			 * Redistribute both to the same segments, here we choose the
			 * same segments with other.
			 */
			single->move_to.numsegments = CdbPathLocus_NumSegments(other->locus);
			other->move_to.numsegments = CdbPathLocus_NumSegments(other->locus);
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

		/* Redistribute both rels on equijoin cols. */
		else if (!small_rel->require_existing_order &&
				 !small_rel->has_wts &&
				 !large_rel->require_existing_order &&
				 !large_rel->has_wts &&
				 cdbpath_distkeys_from_preds(root,
											 redistribution_clauses,
											 large_rel->path,
											 &large_rel->move_to,
											 &small_rel->move_to))
		{
			/*
			 * the two results should all be distributed on the same segments,
			 * here we make them the same with common segments for safe
			 * TODO: how about distribute them both to ALL segments?
			 */
			numsegments = CdbPathLocus_CommonSegments(large_rel->locus,
													  small_rel->locus);

			large_rel->move_to.numsegments = numsegments;
			small_rel->move_to.numsegments = numsegments;
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

		/* Last resort: Move both rels to a single qExec
		 * only if there is no wts on either rels*/
		else if (!outer.has_wts && !inner.has_wts)
		{
			int numsegments = CdbPathLocus_CommonSegments(outer.locus,
														  inner.locus);
			CdbPathLocus_MakeSingleQE(&outer.move_to, numsegments);
			CdbPathLocus_MakeSingleQE(&inner.move_to, numsegments);
		}
		else
			goto fail;
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
	CdbPathLocus_MakeNull(&outer.move_to, GP_POLICY_INVALID_NUMSEGMENTS());
	return outer.move_to;
}								/* cdbpath_motion_for_join */


/*
 * cdbpath_dedup_fixup
 *      Modify path to support unique rowid operation for subquery preds.
 */

typedef struct CdbpathDedupFixupContext
{
	PlannerInfo *root;
	Relids		distinct_on_rowid_relids;
	List	   *rowid_vars;
	int32		subplan_id;
	bool		need_subplan_id;
	bool		need_segment_id;
} CdbpathDedupFixupContext;

static CdbVisitOpt
			cdbpath_dedup_fixup_walker(Path *path, void *context);


/* Drop Var nodes from a List unless they belong to a given set of relids. */
static List *
cdbpath_dedup_pickvars(List *vars, Relids relids_to_keep)
{
	ListCell   *cell;
	ListCell   *nextcell;
	ListCell   *prevcell = NULL;
	Var		   *var;

	for (cell = list_head(vars); cell; cell = nextcell)
	{
		nextcell = lnext(cell);
		var = (Var *) lfirst(cell);
		Assert(IsA(var, Var));
		if (!bms_is_member(var->varno, relids_to_keep))
			vars = list_delete_cell(vars, cell, prevcell);
		else
			prevcell = cell;
	}
	return vars;
}								/* cdbpath_dedup_pickvars */

static CdbVisitOpt
cdbpath_dedup_fixup_unique(UniquePath *uniquePath, CdbpathDedupFixupContext *ctx)
{
	Relids		downstream_relids = ctx->distinct_on_rowid_relids;
	List	   *ctid_exprs;
	List	   *ctid_operators;
	List	   *other_vars = NIL;
	List	   *other_operators = NIL;
	List	   *distkeys = NIL;
	ListCell   *cell;
	bool		save_need_segment_id = ctx->need_segment_id;

	Assert(!ctx->rowid_vars);

	/*
	 * Leave this node unchanged unless it removes duplicates by row id.
	 *
	 * NB. If ctx->distinct_on_rowid_relids is nonempty, row id vars could be
	 * added to our rel's targetlist while visiting the child subtree.  Any
	 * such added columns should pass on safely through this Unique op because
	 * they aren't added to the distinct_on_exprs list.
	 */
	if (bms_is_empty(uniquePath->distinct_on_rowid_relids))
		return CdbVisit_Walk;	/* onward to visit the kids */

	/* No action needed if data is trivially unique. */
	if (uniquePath->umethod == UNIQUE_PATH_NOOP)
		return CdbVisit_Walk;	/* onward to visit the kids */

	/* Find set of relids for which subpath must produce row ids. */
	ctx->distinct_on_rowid_relids = bms_union(ctx->distinct_on_rowid_relids,
											  uniquePath->distinct_on_rowid_relids);

	/* Tell join ops below that row ids mustn't be left out of targetlists. */
	ctx->distinct_on_rowid_relids = bms_add_member(ctx->distinct_on_rowid_relids, 0);

	/* Notify descendants if we're going to insert a MotionPath below. */
	if (uniquePath->must_repartition)
		ctx->need_segment_id = true;

	/* Visit descendants to get list of row id vars and add to targetlists. */
	pathnode_walk_node(uniquePath->subpath, cdbpath_dedup_fixup_walker, ctx);

	/* Restore saved flag. */
	ctx->need_segment_id = save_need_segment_id;

	/*
	 * CDB TODO: we share kid's targetlist at present, so our tlist could
	 * contain rowid vars which are no longer needed downstream.
	 */

	/*
	 * Build DISTINCT ON key for UniquePath, putting the ctid columns first
	 * because those are usually more distinctive than the segment ids. Also
	 * build repartitioning key if needed, using only the ctid columns.
	 */
	ctid_exprs = NIL;
	ctid_operators = NIL;
	foreach(cell, ctx->rowid_vars)
	{
		Var		   *var = (Var *) lfirst(cell);

		Assert(IsA(var, Var) &&
			   bms_is_member(var->varno, ctx->distinct_on_rowid_relids));

		/* Skip vars which aren't part of the row id for this Unique op. */
		if (!bms_is_member(var->varno, uniquePath->distinct_on_rowid_relids))
			continue;

		/* ctid? */
		if (var->varattno == SelfItemPointerAttributeNumber)
		{
			Assert(var->vartype == TIDOID);

			ctid_exprs = lappend(ctid_exprs, var);
			ctid_operators = lappend_oid(ctid_operators, TIDEqualOperator);

			/* Add to repartitioning key. */
			if (uniquePath->must_repartition)
			{
				DistributionKey *cdistkey;
				Oid			opfamily;

				opfamily = get_compatible_hash_opfamily(TIDEqualOperator);

				cdistkey = cdb_make_distkey_for_expr(ctx->root,_(Node *) var,
													 opfamily);
				distkeys = lappend(distkeys, cdistkey);
			}
		}

		/* other uniqueifiers such as gp_segment_id */
		else
		{
			Oid			eqop;

			other_vars = lappend(other_vars, var);

			get_sort_group_operators(exprType((Node *) var),
									 false, true, false,
									 NULL, &eqop, NULL, NULL);

			other_operators = lappend_oid(other_operators, eqop);
		}
	}

	uniquePath->uniq_exprs = list_concat(ctid_exprs, other_vars);
	uniquePath->in_operators = list_concat(ctid_operators, other_operators);

	/* To repartition, add a MotionPath below this UniquePath. */
	if (uniquePath->must_repartition)
	{
		CdbPathLocus locus;

		Assert(distkeys);
		CdbPathLocus_MakeHashed(&locus, distkeys,
								CdbPathLocus_NumSegments(uniquePath->subpath->locus));

		uniquePath->subpath = cdbpath_create_motion_path(ctx->root,
														 uniquePath->subpath,
														 NIL,
														 false,
														 locus);
		Insist(uniquePath->subpath);
		uniquePath->path.locus = uniquePath->subpath->locus;
		uniquePath->path.motionHazard = uniquePath->subpath->motionHazard;
		uniquePath->path.rescannable = uniquePath->subpath->rescannable;
	}

	/* Prune row id var list to remove items not needed downstream. */
	ctx->rowid_vars = cdbpath_dedup_pickvars(ctx->rowid_vars, downstream_relids);

	bms_free(ctx->distinct_on_rowid_relids);
	ctx->distinct_on_rowid_relids = downstream_relids;
	return CdbVisit_Skip;		/* we visited kids already; done with subtree */
}								/* cdbpath_dedup_fixup_unique */

static void
cdbpath_dedup_fixup_baserel(Path *path, CdbpathDedupFixupContext *ctx)
{
	RelOptInfo *rel = path->parent;
	List	   *rowid_vars = NIL;
	Const	   *con;
	Var		   *var;

	Assert(!ctx->rowid_vars);

	/* Find or make a Var node referencing our 'ctid' system attribute. */
	var = find_indexkey_var(ctx->root, rel, SelfItemPointerAttributeNumber);
	rowid_vars = lappend(rowid_vars, var);

	/*
	 * If below a Motion operator, make a Var node for our 'gp_segment_id'
	 * attr.
	 *
	 * Omit if the data is known to come from just one segment, or consists
	 * only of constants (e.g. values scan) or immutable function results.
	 */
	if (ctx->need_segment_id)
	{
		if (!CdbPathLocus_IsBottleneck(path->locus) &&
			!CdbPathLocus_IsGeneral(path->locus))
		{
			var = find_indexkey_var(ctx->root, rel, GpSegmentIdAttributeNumber);
			rowid_vars = lappend(rowid_vars, var);
		}
	}

	/*
	 * If below an Append, add 'gp_subplan_id' pseudo column to the
	 * targetlist.
	 *
	 * set_plan_references() will later replace the pseudo column Var node in
	 * our rel's targetlist with a copy of its defining expression, i.e. the
	 * Const node built here.
	 */
	if (ctx->need_subplan_id)
	{
		/* Make a Const node containing the current subplan id. */
		con = makeConst(INT4OID, -1, InvalidOid, sizeof(int32),
						Int32GetDatum(ctx->subplan_id),
						false, true);

		/* Set up a pseudo column whose value will be the constant. */
		var = cdb_define_pseudo_column(ctx->root, rel, "gp_subplan_id",
									   (Expr *) con, sizeof(int32));

		/* Give downstream operators a Var referencing the pseudo column. */
		rowid_vars = lappend(rowid_vars, var);
	}

	/* Add these vars to the rel's list of result columns. */
	add_vars_to_targetlist(ctx->root, rowid_vars, ctx->distinct_on_rowid_relids, false);

	/* Recalculate width of the rel's result rows. */
	set_rel_width(ctx->root, rel);

	/*
	 * Tell caller to add our vars to the DISTINCT ON key of the ancestral
	 * UniquePath, and to the targetlists of any intervening ancestors.
	 */
	ctx->rowid_vars = rowid_vars;
}								/* cdbpath_dedup_fixup_baserel */

static void
cdbpath_dedup_fixup_joinrel(JoinPath *joinpath, CdbpathDedupFixupContext *ctx)
{
	RelOptInfo *rel = joinpath->path.parent;

	Assert(!ctx->rowid_vars);

	/*
	 * CDB TODO: Subpath id isn't needed from both outer and inner. Don't
	 * request row id vars from rhs of EXISTS join.
	 */

	/* Get row id vars from outer subpath. */
	if (joinpath->outerjoinpath)
		pathnode_walk_node(joinpath->outerjoinpath, cdbpath_dedup_fixup_walker, ctx);

	/* Get row id vars from inner subpath. */
	if (joinpath->innerjoinpath)
	{
		List	   *outer_rowid_vars = ctx->rowid_vars;

		ctx->rowid_vars = NIL;
		pathnode_walk_node(joinpath->innerjoinpath, cdbpath_dedup_fixup_walker, ctx);

		/* Which rel has more rows?  Put its row id vars in front. */
		if (outer_rowid_vars &&
			ctx->rowid_vars &&
			joinpath->outerjoinpath->rows >= joinpath->innerjoinpath->rows)
			ctx->rowid_vars = list_concat(outer_rowid_vars, ctx->rowid_vars);
		else
			ctx->rowid_vars = list_concat(ctx->rowid_vars, outer_rowid_vars);
	}

	/* Update joinrel's targetlist and adjust row width. */
	if (ctx->rowid_vars)
		build_joinrel_tlist(ctx->root, rel, ctx->rowid_vars);
}								/* cdbpath_dedup_fixup_joinrel */

static void
cdbpath_dedup_fixup_motion(CdbMotionPath *motionpath, CdbpathDedupFixupContext *ctx)
{
	bool		save_need_segment_id = ctx->need_segment_id;

	/*
	 * Motion could bring together rows which happen to have the same ctid but
	 * are actually from different segments.  They must not be treated as
	 * duplicates.  To distinguish them, let each row be labeled with its
	 * originating segment id.
	 */
	ctx->need_segment_id = true;

	/* Visit the upstream nodes. */
	pathnode_walk_node(motionpath->subpath, cdbpath_dedup_fixup_walker, ctx);

	/* Restore saved flag. */
	ctx->need_segment_id = save_need_segment_id;
}								/* cdbpath_dedup_fixup_motion */

static void
cdbpath_dedup_fixup_append(AppendPath *appendPath, CdbpathDedupFixupContext *ctx)
{
	Relids		save_distinct_on_rowid_relids = ctx->distinct_on_rowid_relids;
	List	   *appendrel_rowid_vars;
	ListCell   *cell;
	int			ncol;
	bool		save_need_subplan_id = ctx->need_subplan_id;

	/*
	 * The planner creates dummy AppendPaths with no subplans, if it can
	 * eliminate a relation altogther with constraint exclusion. We have
	 * nothing to do for those.
	 */
	if (appendPath->subpaths == NIL)
		return;

	Assert(!ctx->rowid_vars);

	/* Make a working copy of the set of relids for which row ids are needed. */
	ctx->distinct_on_rowid_relids = bms_copy(ctx->distinct_on_rowid_relids);

	/*
	 * Append could bring together rows which happen to have the same ctid but
	 * are actually from different tables or different branches of a UNION
	 * ALL.  They must not be treated as duplicates.  To distinguish them, let
	 * each row be labeled with an integer which will be different for each
	 * branch of the Append.
	 */
	ctx->need_subplan_id = true;

	/* Assign a dummy subplan id (not actually used) for the appendrel. */
	ctx->subplan_id++;

	/* Add placeholder columns to the appendrel's targetlist. */
	cdbpath_dedup_fixup_baserel((Path *) appendPath, ctx);
	ncol = list_length(appendPath->path.parent->reltargetlist);

	appendrel_rowid_vars = ctx->rowid_vars;
	ctx->rowid_vars = NIL;

	/* Update the parent and child rels. */
	foreach(cell, appendPath->subpaths)
	{
		Path	   *subpath = (Path *) lfirst(cell);

		if (!subpath)
			continue;

		/* Assign a subplan id to this branch of the Append. */
		ctx->subplan_id++;

		/* Tell subpath to produce row ids. */
		ctx->distinct_on_rowid_relids =
			bms_add_members(ctx->distinct_on_rowid_relids,
							subpath->parent->relids);

		/* Process one subpath. */
		pathnode_walk_node(subpath, cdbpath_dedup_fixup_walker, ctx);

		/*
		 * Subpath and appendrel should have same number of result columns.
		 * CDB TODO: Add dummy columns to other subpaths to keep their
		 * targetlists in sync.
		 */
		if (list_length(subpath->parent->reltargetlist) != ncol)
			ereport(ERROR, (errcode(ERRCODE_GP_FEATURE_NOT_YET),
							errmsg("The query is not yet supported in "
								   "this version of " PACKAGE_NAME "."),
							errdetail("Unsupported combination of "
									  "UNION ALL of joined tables "
									  "with subquery.")
							));

		/* Don't need subpath's rowid_vars. */
		list_free(ctx->rowid_vars);
		ctx->rowid_vars = NIL;
	}

	/* Provide appendrel's row id vars to downstream operators. */
	ctx->rowid_vars = appendrel_rowid_vars;

	/* Restore saved values. */
	bms_free(ctx->distinct_on_rowid_relids);
	ctx->distinct_on_rowid_relids = save_distinct_on_rowid_relids;
	ctx->need_subplan_id = save_need_subplan_id;
}								/* cdbpath_dedup_fixup_append */

static CdbVisitOpt
cdbpath_dedup_fixup_walker(Path *path, void *context)
{
	CdbpathDedupFixupContext *ctx = (CdbpathDedupFixupContext *) context;

	Assert(!ctx->rowid_vars);

	/* Watch for a UniquePath node calling for removal of dups by row id. */
	if (path->pathtype == T_Unique)
		return cdbpath_dedup_fixup_unique((UniquePath *) path, ctx);

	/* Leave node unchanged unless a downstream Unique op needs row ids. */
	if (!bms_overlap(path->parent->relids, ctx->distinct_on_rowid_relids))
		return CdbVisit_Walk;	/* visit descendants */

	/* Alter this node to produce row ids for an ancestral Unique operator. */
	switch (path->pathtype)
	{
		case T_Append:
			cdbpath_dedup_fixup_append((AppendPath *) path, ctx);
			break;

		case T_SeqScan:
		case T_ExternalScan:
		case T_IndexScan:
		case T_BitmapHeapScan:
		case T_TidScan:
		case T_SubqueryScan:
		case T_FunctionScan:
		case T_ValuesScan:
		case T_CteScan:
		case T_ForeignScan:
			cdbpath_dedup_fixup_baserel(path, ctx);
			break;

		case T_HashJoin:
		case T_MergeJoin:
		case T_NestLoop:
			cdbpath_dedup_fixup_joinrel((JoinPath *) path, ctx);
			break;

		case T_Result:
		case T_Material:
			/* These nodes share child's RelOptInfo and don't need fixup. */
			return CdbVisit_Walk;	/* visit descendants */

		case T_Motion:
			cdbpath_dedup_fixup_motion((CdbMotionPath *) path, ctx);
			break;

		default:
			elog(ERROR, "cannot create a unique ID for path type: %d", path->pathtype);
	}
	return CdbVisit_Skip;		/* already visited kids, don't revisit them */
}								/* cdbpath_dedup_fixup_walker */

void
cdbpath_dedup_fixup(PlannerInfo *root, Path *path)
{
	CdbpathDedupFixupContext context;

	memset(&context, 0, sizeof(context));

	context.root = root;

	pathnode_walk_node(path, cdbpath_dedup_fixup_walker, &context);

	Assert(bms_is_empty(context.distinct_on_rowid_relids) &&
		   !context.rowid_vars &&
		   !context.need_segment_id &&
		   !context.need_subplan_id);
}								/* cdbpath_dedup_fixup */

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
 * turn_volatile_seggen_to_singleqe
 *
 * This function is the key tool to build correct plan
 * for general or segmentgeneral locus paths that contain
 * volatile functions.
 *
 * If we find such a pattern:
 *    1. if we are update or delete statement on replicated table
 *       simply reject the query
 *    2. if it is general locus, simply change it to singleQE
 *    3. if it is segmentgeneral, use a motion to bring it to
 *       singleQE and then create a projection path
 *
 * If we do not find the pattern, simply return the input path.
 *
 * The last parameter of this function is the part that we want to
 * check volatile functions.
 */
Path *
turn_volatile_seggen_to_singleqe(PlannerInfo *root, Path *path, Node *node)
{
	if ((CdbPathLocus_IsSegmentGeneral(path->locus) || CdbPathLocus_IsGeneral(path->locus)) &&
		(contain_volatile_functions(node)))
	{
		CdbPathLocus     singleQE;
		Path            *mpath;
		ProjectionPath  *ppath;

		if (root->upd_del_replicated_table > 0 &&
			bms_is_member(root->upd_del_replicated_table,
						  path->parent->relids))
			elog(ERROR, "could not devise a plan");

		if (CdbPathLocus_IsGeneral(path->locus))
		{
			CdbPathLocus_MakeSingleQE(&(path->locus),
									  getgpsegmentCount());
			return path;
		}

		CdbPathLocus_MakeSingleQE(&singleQE,
								  CdbPathLocus_NumSegments(path->locus));
		mpath = cdbpath_create_motion_path(root, path, NIL, false, singleQE);
		/*
		 * mpath might be NULL, like path contain outer Params
		 * See Github Issue 13532 for details.
		 */
		if (mpath == NULL)
			return path;

		ppath =  create_projection_path_with_quals(root, mpath->parent, mpath, NIL);
		ppath->force = true;
		return (Path *) ppath;
	}
	else
		return path;
}
