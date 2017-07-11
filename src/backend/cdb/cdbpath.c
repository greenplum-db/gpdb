/*-------------------------------------------------------------------------
 *
 * cdbpath.c
 *
 * Copyright (c) 2005-2008, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"    /* CDB_PROC_TIDTOI8 */
#include "catalog/pg_type.h"    /* INT8OID */
#include "miscadmin.h"          /* work_mem */
#include "nodes/makefuncs.h"    /* makeFuncExpr() */
#include "nodes/relation.h"     /* PlannerInfo, RelOptInfo, CdbRelDedupInfo */
#include "optimizer/cost.h"     /* cpu_tuple_cost */
#include "optimizer/pathnode.h" /* Path, pathnode_walker() */
#include "optimizer/paths.h"    /* compare_pathkeys() */
#include "optimizer/planmain.h"

#include "parser/parse_expr.h"	/* exprType() */
#include "parser/parse_oper.h"

#include "utils/syscache.h"

#include "cdb/cdbdef.h"         /* CdbSwap() */
#include "cdb/cdbllize.h"       /* makeFlow() */
#include "cdb/cdbhash.h"        /* isGreenplumDbHashable() */

#include "cdb/cdbpath.h"        /* me */
#include "cdb/cdbvars.h"

#ifdef small                    /* <socket.h> might #define small */
#undef small                    /*  but I want it for a variable name */
#endif


/*
 * cdbpath_cost_motion
 *    Fills in the cost estimate fields in a MotionPath node.
 */
void
cdbpath_cost_motion(PlannerInfo *root, CdbMotionPath *motionpath)
{
    Path   *subpath = motionpath->subpath;
    Cost    cost_per_row;
    Cost    motioncost;
    double  recvrows;
    double  sendrows;

    cost_per_row = (gp_motion_cost_per_row > 0.0)
                    ? gp_motion_cost_per_row
                    : 2.0 * cpu_tuple_cost;
    sendrows = cdbpath_rows(root, subpath);
    recvrows = cdbpath_rows(root, (Path *)motionpath);
    motioncost = cost_per_row * 0.5 * (sendrows + recvrows);

    motionpath->path.total_cost = motioncost + subpath->total_cost;
    motionpath->path.startup_cost = subpath->startup_cost;
    motionpath->path.memory = subpath->memory;
}                               /* cdbpath_cost_motion */


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
cdbpath_create_motion_path(PlannerInfo     *root,
                           Path            *subpath,
                           List            *pathkeys,
                           bool             require_existing_order,
                           CdbPathLocus     locus)
{
    CdbMotionPath      *pathnode;

    UnusedArg(root);
    Assert(cdbpathlocus_is_valid(locus) &&
           cdbpathlocus_is_valid(subpath->locus));

    /* Moving subpath output to a single executor process (qDisp or qExec)? */
    if (CdbPathLocus_IsBottleneck(locus))
    {
        /* entry-->entry or singleQE-->singleQE?  No motion needed. */
        if (CdbPathLocus_IsEqual(subpath->locus, locus))
            return subpath;

        /* entry-->singleQE?  Don't move.  Slice's QE will run on entry db. */
        if (CdbPathLocus_IsEntry(subpath->locus))
            return subpath;

        /* singleQE-->entry?  Don't move.  Slice's QE will run on entry db. */
        if (CdbPathLocus_IsSingleQE(subpath->locus))
        {
            /* Create CdbMotionPath node to indicate that the slice must be
             * dispatched to a singleton gang running on the entry db.  We
             * merely use this node to note that the path has 'Entry' locus;
             * no corresponding Motion node will be created in the Plan tree.
             */
            Assert(CdbPathLocus_IsEntry(locus));

            pathnode = makeNode(CdbMotionPath);
            pathnode->path.pathtype = T_Motion;
            pathnode->path.parent = subpath->parent;
            pathnode->path.locus = locus;
            pathnode->path.pathkeys = pathkeys;
            pathnode->subpath = subpath;

            /* Costs, etc, are same as subpath. */
            pathnode->path.startup_cost = subpath->total_cost;
            pathnode->path.total_cost = subpath->total_cost;
            pathnode->path.memory = subpath->memory;
            pathnode->path.motionHazard = subpath->motionHazard;
            pathnode->path.rescannable = subpath->rescannable;
            return (Path *)pathnode;
        }

        /* No motion needed if subpath can run anywhere giving same output. */
        if (CdbPathLocus_IsGeneral(subpath->locus))
            return subpath;

        /* Fail if caller refuses motion. */
        if (require_existing_order &&
            !pathkeys)
            return NULL;

        /* replicated-->singleton would give redundant copies of the rows. */
        if (CdbPathLocus_IsReplicated(subpath->locus))
            goto invalid_motion_request;

        /* Must be partitioned-->singleton.
         *  If caller gave pathkeys, they'll be used for Merge Receive.
         *  If no pathkeys, Union Receive will arbitrarily interleave
         *  the rows from the subpath partitions in no special order.
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
            if (cdbpathlocus_compare(CdbPathLocus_Comparison_Equal, subpath->locus, locus))
                return subpath;
        }

        /* Must be partitioned-->replicated */
        else if (!CdbPathLocus_IsReplicated(locus))
            goto invalid_motion_request;

        /* Fail if caller insists on ordered result or no motion. */
        if (require_existing_order)
            return NULL;

        /* Output streams lose any ordering they had.
         * Only a qDisp or singleton qExec can merge sorted streams (for now).
         */
        pathkeys = NIL;
    }

    /* If subplan uses no tables, it can run on qDisp or a singleton qExec. */
    else if (CdbPathLocus_IsGeneral(subpath->locus))
    {
        /* No motion needed if general-->general or general-->replicated. */
        if (CdbPathLocus_IsGeneral(locus) ||
            CdbPathLocus_IsReplicated(locus))
            return subpath;

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
            return subpath;

        /* Other destinations aren't used or supported at present. */
        goto invalid_motion_request;
    }
    else
        goto invalid_motion_request;

    /* Don't materialize before motion. */
    if (IsA(subpath, MaterialPath))
        subpath = ((MaterialPath *)subpath)->subpath;

	/*
	 * MPP-3300: materialize *before* motion can never help us, motion
	 * pushes data. other nodes pull. We relieve motion deadlocks by
	 * adding materialize nodes on top of motion nodes
	 */

    /* Create CdbMotionPath node. */
    pathnode = makeNode(CdbMotionPath);
    pathnode->path.pathtype = T_Motion;
    pathnode->path.parent = subpath->parent;
    pathnode->path.locus = locus;
    pathnode->path.pathkeys = pathkeys;
    pathnode->subpath = subpath;

    /* Cost of motion */
    cdbpath_cost_motion(root, pathnode);

    /* Tell operators above us that slack may be needed for deadlock safety. */
    pathnode->path.motionHazard = true;
    pathnode->path.rescannable = false;

    return (Path *)pathnode;

    /* Unexpected source or destination locus. */
invalid_motion_request:
    Assert(0);
    return NULL;
}                               /* cdbpath_create_motion_path */

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
    PlannerInfo    *root;
    List           *mergeclause_list;
    CdbPathLocus    locus;
    CdbPathLocus   *colocus;
    bool            colocus_eq_locus;
} CdbpathMatchPredsContext;


/*
 * A helper function to create a PathKey for an EquivalenceClass. This is used
 * to construct a PathKey in a hash locus, rather than sort ordering, so we
 * don't care about the particular opfamily being used, the strategy, nor
 * nulls_first.
 */
static PathKey *
makePathKeyForEC(EquivalenceClass *eclass)
{
	PathKey    *pk = makeNode(PathKey);

	pk->pk_eclass = eclass;
	pk->pk_opfamily = linitial_oid(eclass->ec_opfamilies);
	pk->pk_strategy = BTLessStrategyNumber;
	pk->pk_nulls_first = false;

	return pk;
}

static bool
cdbpath_match_preds_to_partkey_tail(CdbpathMatchPredsContext *ctx,
									ListCell *partkeycell)
{
	PathKey    *copathkey;
	ListCell   *rcell;

	/*----------------
	 * Is there a "<partkey item> = <constant expr>" predicate?
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
	copathkey = NULL;

	if (ctx->locus.locustype == CdbLocusType_Hashed)
	{
		PathKey    *pathkey = (PathKey *) lfirst(partkeycell);

		if (CdbPathkeyEqualsConstant(pathkey))
			copathkey = pathkey;
	}
	else if (ctx->locus.locustype == CdbLocusType_HashedOJ)
	{
		List	   *sublist = (List *) lfirst(partkeycell);
		ListCell   *cell;

		foreach(cell, sublist)
		{
			PathKey    *pathkey = (PathKey *) lfirst(cell);

			if (CdbPathkeyEqualsConstant(pathkey))
			{
				copathkey = pathkey;
				break;
			}
		}
	}
	else
		elog(ERROR, "unexpected locus type: %u", ctx->locus.locustype);
	/* Look for an equijoin comparison to the partkey item. */
	if (!copathkey)
	{
		foreach(rcell, ctx->mergeclause_list)
		{
			ListCell   *i;
			RestrictInfo *rinfo = (RestrictInfo *) lfirst(rcell);

			if (!rinfo->left_ec)
				cache_mergeclause_eclasses(ctx->root, rinfo);

			if (CdbPathLocus_IsHashed(ctx->locus))
			{
				PathKey    *pathkey = (PathKey *) lfirst(partkeycell);

				if (pathkey->pk_eclass == rinfo->left_ec)
					copathkey = makePathKeyForEC(rinfo->right_ec);
				else if (pathkey->pk_eclass == rinfo->right_ec)
					copathkey = makePathKeyForEC(rinfo->left_ec);
			}
			else if (CdbPathLocus_IsHashedOJ(ctx->locus))
			{
				List	   *sublist = (List *) lfirst(partkeycell);

				foreach(i, sublist)
				{
					PathKey    *pathkey = (PathKey *) lfirst(i);

					if (pathkey->pk_eclass == rinfo->left_ec)
						copathkey = makePathKeyForEC(rinfo->right_ec);
					else if (pathkey->pk_eclass == rinfo->right_ec)
						copathkey = makePathKeyForEC(rinfo->left_ec);
				}
			}

			if (copathkey)
				break;
		}

		/* Fail if didn't find a match for this partkey item. */
		if (!copathkey)
			return false;
	}

	/* Might need to build co-locus if locus is outer join source or result. */
	if (copathkey != lfirst(partkeycell))
		ctx->colocus_eq_locus = false;

	/* Match remaining partkey items. */
	partkeycell = lnext(partkeycell);
	if (partkeycell)
	{
		if (!cdbpath_match_preds_to_partkey_tail(ctx, partkeycell))
			return false;
	}

	/* Success!  Matched all items.  Return co-locus if requested. */
	if (ctx->colocus)
	{
		if (ctx->colocus_eq_locus)
			*ctx->colocus = ctx->locus;
		else if (!partkeycell)
			CdbPathLocus_MakeHashed(ctx->colocus, list_make1(copathkey));
		else
		{
			if (CdbPathLocus_IsHashed(*ctx->colocus))
				ctx->colocus->partkey_h = lcons(copathkey, ctx->colocus->partkey_h);
			else
			{
				Assert(CdbPathLocus_IsHashedOJ(*ctx->colocus));
				ctx->colocus->partkey_oj = lcons(copathkey, ctx->colocus->partkey_oj);
			}
			Assert(cdbpathlocus_is_valid(*ctx->colocus));
		}
	}
	return true;
}	/* cdbpath_match_preds_to_partkey_tail */



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
cdbpath_match_preds_to_partkey(PlannerInfo     *root,
                               List            *mergeclause_list,
                               CdbPathLocus     locus,
                               CdbPathLocus    *colocus)            /* OUT */
{
    CdbpathMatchPredsContext ctx;

    if (!CdbPathLocus_IsHashed(locus) &&
        !CdbPathLocus_IsHashedOJ(locus))
        return false;

    Assert(cdbpathlocus_is_valid(locus));

    ctx.root                = root;
    ctx.mergeclause_list    = mergeclause_list;
    ctx.locus               = locus;
    ctx.colocus             = colocus;
    ctx.colocus_eq_locus    = true;

	if (CdbPathLocus_IsHashed(locus))
		return cdbpath_match_preds_to_partkey_tail(&ctx, list_head(locus.partkey_h));
	else
		return cdbpath_match_preds_to_partkey_tail(&ctx, list_head(locus.partkey_oj));
}                               /* cdbpath_match_preds_to_partkey */


/*
 * cdbpath_match_preds_to_both_partkeys
 *
 * Returns true if the mergeclause_list contains equijoin
 * predicates between each item of the outer_locus partkey and
 * the corresponding item of the inner_locus partkey.
 *
 * Readers may refer also to these related functions:
 *          select_mergejoin_clauses() in joinpath.c
 *          find_mergeclauses_for_pathkeys() in pathkeys.c
 */
static bool
cdbpath_match_preds_to_both_partkeys(PlannerInfo   *root,
                                     List          *mergeclause_list,
                                     CdbPathLocus   outer_locus,
                                     CdbPathLocus   inner_locus)
{
    ListCell   *outercell;
    ListCell   *innercell;
	List	   *outer_partkey;
	List	   *inner_partkey;

    if (!mergeclause_list ||
		CdbPathLocus_Degree(outer_locus) == 0 || CdbPathLocus_Degree(inner_locus) == 0 ||
        CdbPathLocus_Degree(outer_locus) != CdbPathLocus_Degree(inner_locus))
        return false;

    Assert(CdbPathLocus_IsHashed(outer_locus) ||
           CdbPathLocus_IsHashedOJ(outer_locus));
    Assert(CdbPathLocus_IsHashed(inner_locus) ||
           CdbPathLocus_IsHashedOJ(inner_locus));

	if (CdbPathLocus_IsHashed(outer_locus))
		outer_partkey = outer_locus.partkey_h;
	else
		outer_partkey = outer_locus.partkey_oj;

	if (CdbPathLocus_IsHashed(inner_locus))
		inner_partkey = inner_locus.partkey_h;
	else
		inner_partkey = inner_locus.partkey_oj;

    forboth(outercell, outer_partkey, innercell, inner_partkey)
    {
        List        *outersublist = (List *)lfirst(outercell);
        List        *innersublist = (List *)lfirst(innercell);
        ListCell    *rcell;
        foreach(rcell, mergeclause_list)
        {
			bool not_found = false;
            RestrictInfo   *rinfo = (RestrictInfo *)lfirst(rcell);

            if (!rinfo->left_ec)
                cache_mergeclause_eclasses(root, rinfo);

            /* Skip predicate if neither side matches outer partkey item. */
            if (CdbPathLocus_IsHashed(outer_locus))
            {
				PathKey *pathkey = (PathKey *) outersublist;
				if (pathkey->pk_eclass != rinfo->left_ec && pathkey->pk_eclass != rinfo->right_ec)
					continue;
            }
            else
            {
				Assert(CdbPathLocus_IsHashedOJ(outer_locus));
				ListCell *i;
				foreach(i, outersublist)
				{
					PathKey *pathkey = (PathKey *) lfirst(i);
					if (pathkey->pk_eclass != rinfo->left_ec && pathkey->pk_eclass != rinfo->right_ec)
					{
						not_found = true;
						break;
					}
				}
				if (not_found)
					continue;
            }

            /* Skip predicate if neither side matches inner partkey item. */
            if (innersublist == outersublist)
            {}                  /* do nothing */
            else if (CdbPathLocus_IsHashed(inner_locus))
            {
				PathKey *pathkey = (PathKey *) innersublist;
				if (pathkey->pk_eclass != rinfo->left_ec && pathkey->pk_eclass != rinfo->right_ec)
					continue;
            }
            else
            {
                Assert(CdbPathLocus_IsHashedOJ(inner_locus));
				ListCell *i;
				foreach(i, innersublist)
				{
					PathKey *pathkey = (PathKey *) lfirst(i);
					if (pathkey->pk_eclass != rinfo->left_ec && pathkey->pk_eclass != rinfo->right_ec)
					{
						not_found = true;
						break;
					}
				}
				if (not_found)
					continue;
            }

            /* Found equijoin between outer partkey item & inner partkey item */
            break;
        }

        /* Fail if didn't find equijoin between this pair of partkey items. */
        if (!rcell)
            return false;
    }
    return true;
}                               /* cdbpath_match_preds_to_both_partkeys */



/*
 * cdb_pathkey_isGreenplumDbHashable
 *
 * Iterates through a list of equivalence class members and determines if all
 * of them are GreenplumDbHashable.
 */
static bool
cdbpath_eclass_isGreenplumDbHashable(EquivalenceClass *ec)
{
	ListCell *j;

	foreach(j, ec->ec_members)
	{
		EquivalenceMember *em = (EquivalenceMember *) lfirst(j);

		/* Fail on non-hashable expression types */
		if (!isGreenplumDbHashable(exprType((Node *) em->em_expr)))
				return false;
	}

	return true;
}


/*
 * cdbpath_partkeys_from_preds
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
cdbpath_partkeys_from_preds(PlannerInfo    *root,
                            List           *mergeclause_list,
                            Path           *a_path,
                            CdbPathLocus   *a_locus,            /* OUT */
                            CdbPathLocus   *b_locus)            /* OUT */
{
    List       *a_partkey = NIL;
    List       *b_partkey = NIL;
    ListCell   *rcell;

    foreach(rcell, mergeclause_list)
    {
        RestrictInfo *rinfo = (RestrictInfo *) lfirst(rcell);

        if (!rinfo->left_ec)
        {
            cache_mergeclause_eclasses(root, rinfo);
            Assert(rinfo->left_ec);
        }

        /*
         * skip non-hashable keys
         */
        if (!cdbpath_eclass_isGreenplumDbHashable(rinfo->left_ec) ||
            !cdbpath_eclass_isGreenplumDbHashable(rinfo->right_ec))
        {
            continue;
        }

        /* Left & right pathkeys are usually the same... */
        if (!b_partkey && rinfo->left_ec == rinfo->right_ec)
        {
			ListCell *i;

			foreach(i, a_partkey)
			{
				PathKey *pathkey = (PathKey *) lfirst(i);
				if (pathkey->pk_eclass == rinfo->left_ec)
					a_partkey = lappend(a_partkey, rinfo->left_ec);
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
			 * Convoluted logic to ensure that (a_ec not in a_partkey) AND (b_ec not in b_partkey)
			 */
			found = false;
			foreach(i, a_partkey)
			{
				PathKey *pathkey = (PathKey *) lfirst(i);
				if (pathkey->pk_eclass == a_ec)
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				foreach(i, b_partkey)
				{
					PathKey *pathkey = (PathKey *) lfirst(i);
					if (pathkey->pk_eclass == b_ec)
					{
						found = true;
						break;
					}
				}
			}

			if (!found)
            {
				PathKey *a_pk = makePathKeyForEC(a_ec);
				PathKey *b_pk = makePathKeyForEC(b_ec);

                a_partkey = lappend(a_partkey, a_pk);
                b_partkey = lappend(b_partkey, b_pk);
            }
        }

        if (list_length(a_partkey) >= 20)
            break;
    }

    if (!a_partkey)
        return false;

    CdbPathLocus_MakeHashed(a_locus, a_partkey);
    if (b_partkey)
        CdbPathLocus_MakeHashed(b_locus, b_partkey);
    else
        *b_locus = *a_locus;
    return true;
}                               /* cdbpath_partkeys_from_preds */


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
 */

typedef struct
{
        CdbPathLocus    locus;
        CdbPathLocus    move_to;
        double          bytes;
        Path           *path;
        bool            ok_to_replicate;
        bool            require_existing_order;
} CdbpathMfjRel;

CdbPathLocus
cdbpath_motion_for_join(PlannerInfo    *root,
                        JoinType        jointype,           /* JOIN_INNER/FULL/LEFT/RIGHT/IN */
                        Path          **p_outer_path,       /* INOUT */
                        Path          **p_inner_path,       /* INOUT */
                        List           *mergeclause_list,   /* equijoin RestrictInfo list */
                        List           *outer_pathkeys,
                        List           *inner_pathkeys,
                        bool            outer_require_existing_order,
                        bool            inner_require_existing_order)
{
    CdbpathMfjRel   outer;
    CdbpathMfjRel   inner;

    outer.path  = *p_outer_path;
    inner.path  = *p_inner_path;
    outer.locus = outer.path->locus;
    inner.locus = inner.path->locus;
    CdbPathLocus_MakeNull(&outer.move_to);
    CdbPathLocus_MakeNull(&inner.move_to);

    Assert(cdbpathlocus_is_valid(outer.locus) &&
           cdbpathlocus_is_valid(inner.locus));

    /* Caller can specify an ordering for each source path that is
     * the same as or weaker than the path's existing ordering.
     * Caller may insist that we do not add motion that would
     * lose the specified ordering property; otherwise the given
     * ordering is preferred but not required.
     * A required NIL ordering means no motion is allowed for that path.
     */
    outer.require_existing_order = outer_require_existing_order;
    inner.require_existing_order = inner_require_existing_order;

    /* Don't consider replicating the preserved rel of an outer join, or
     * the current-query rel of a join between current query and subquery.
     */
    outer.ok_to_replicate = true;
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
            Assert(0);
    }

    /* Get rel sizes. */
    outer.bytes = cdbpath_rows(root, outer.path) * outer.path->parent->width;
    inner.bytes = cdbpath_rows(root, inner.path) * inner.path->parent->width;

    /*
     * Motion not needed if either source is everywhere (e.g. a constant).
     *
     * But if a row is everywhere and is preserved in an outer join, we
     * don't want to preserve it in every qExec process where it is
     * unmatched, because that would produce duplicate null-augmented rows.
     * So in that case, bring all the partitions to a single qExec to be joined.
     * CDB TODO: Can this case be handled without introducing a bottleneck?
     */
    if (CdbPathLocus_IsGeneral(outer.locus))
    {
        if (!outer.ok_to_replicate &&
            CdbPathLocus_IsPartitioned(inner.locus))
            CdbPathLocus_MakeSingleQE(&inner.move_to);
        else
            return inner.locus;
    }
    else if (CdbPathLocus_IsGeneral(inner.locus))
    {
        if (!inner.ok_to_replicate &&
            CdbPathLocus_IsPartitioned(outer.locus))
            CdbPathLocus_MakeSingleQE(&outer.move_to);
        else
            return outer.locus;
    }

    /*
     * Is either source confined to a single process?
     *   NB: Motion to a single process (qDisp or qExec) is the only motion
     *   in which we may use Merge Receive to preserve an existing ordering.
     */
    else if (CdbPathLocus_IsBottleneck(outer.locus) ||
             CdbPathLocus_IsBottleneck(inner.locus))
    {                                       /* singleQE or entry db */
        CdbpathMfjRel  *single = &outer;
        CdbpathMfjRel  *other = &inner;
        bool            single_immovable = outer.require_existing_order &&
                                           !outer_pathkeys;
        bool            other_immovable = inner.require_existing_order &&
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
                return other->locus;
        }

        /* Let 'single' be the source whose locus is singleQE or entry. */
        else
        {
            CdbSwap(CdbpathMfjRel*, single, other);
            CdbSwap(bool, single_immovable, other_immovable);
        }
        Assert(CdbPathLocus_IsBottleneck(single->locus));
        Assert(CdbPathLocus_IsPartitioned(other->locus));

        /* If the bottlenecked rel can't be moved, bring the other rel to it. */
        if (single_immovable)
            other->move_to = single->locus;

        /* Redistribute single rel if joining on other rel's partitioning key */
        else if (cdbpath_match_preds_to_partkey(root,
                                                mergeclause_list,
                                                other->locus,
                                                &single->move_to))  /* OUT */
        {}

        /* Replicate single rel if cheaper than redistributing both rels. */
        else if (single->ok_to_replicate &&
                 single->bytes * root->config->cdbpath_segments < single->bytes + other->bytes)
            CdbPathLocus_MakeReplicated(&single->move_to);

        /* Redistribute both rels on equijoin cols. */
        else if (!other->require_existing_order &&
                 cdbpath_partkeys_from_preds(root,
                                             mergeclause_list,
                                             single->path,
                                             &single->move_to,  /* OUT */
                                             &other->move_to))  /* OUT */
        {}

        /* No usable equijoin preds, or caller imposed restrictions on motion.
         * Replicate single rel if cheaper than bottlenecking other rel.
         */
        else if (single->ok_to_replicate &&
                 single->bytes < other->bytes)
            CdbPathLocus_MakeReplicated(&single->move_to);

        /* Last resort: Move all partitions of other rel to single QE. */
        else
            other->move_to = single->locus;
    }                                       /* singleQE or entry */

    /*
     * Replicated paths shouldn't occur loose, for now.
     */
    else if (CdbPathLocus_IsReplicated(outer.locus) ||
             CdbPathLocus_IsReplicated(inner.locus))
    {
        Assert(false);
        goto fail;
    }

    /*
     * No motion if partitioned alike and joining on the partitioning keys.
     */
    else if (cdbpath_match_preds_to_both_partkeys(root, mergeclause_list,
                                                  outer.locus, inner.locus))
        return cdbpathlocus_join(outer.locus, inner.locus);

    /*
     * Kludge used internally for querying catalogs on segment dbs.
     * Each QE will join the catalogs that are local to its own segment.
     * The catalogs don't have partitioning keys.  No motion needed.
     */
    else if (CdbPathLocus_IsStrewn(outer.locus) &&
             CdbPathLocus_IsStrewn(inner.locus) &&
             cdbpathlocus_querysegmentcatalogs)
        return outer.locus;

    /*
     * Both sources are partitioned.  Redistribute or replicate one or both.
     */
    else
    {                                       /* partitioned */
        CdbpathMfjRel  *large = &outer;
        CdbpathMfjRel  *small = &inner;

        /* Which rel is bigger? */
        if (large->bytes < small->bytes)
            CdbSwap(CdbpathMfjRel*, large, small);

        /* If joining on larger rel's partitioning key, redistribute smaller. */
        if (!small->require_existing_order &&
            cdbpath_match_preds_to_partkey(root,
                                           mergeclause_list,
                                           large->locus,
                                           &small->move_to))    /* OUT */
        {}

        /* Replicate smaller rel if cheaper than redistributing larger rel.
         * But don't replicate a rel that is to be preserved in outer join.
         */
        else if (!small->require_existing_order &&
                 small->ok_to_replicate &&
                 small->bytes * root->config->cdbpath_segments < large->bytes)
            CdbPathLocus_MakeReplicated(&small->move_to);

        /* If joining on smaller rel's partitioning key, redistribute larger. */
        else if (!large->require_existing_order &&
                 cdbpath_match_preds_to_partkey(root,
                                                mergeclause_list,
                                                small->locus,
                                                &large->move_to))   /* OUT */
        {}

        /* Replicate smaller rel if cheaper than redistributing both rels. */
        else if (!small->require_existing_order &&
                 small->ok_to_replicate &&
                 small->bytes * root->config->cdbpath_segments < large->bytes + small->bytes)
            CdbPathLocus_MakeReplicated(&small->move_to);

        /* Redistribute both rels on equijoin cols. */
        else if (!small->require_existing_order &&
                 !large->require_existing_order &&
                 cdbpath_partkeys_from_preds(root,
                                             mergeclause_list,
                                             large->path,
                                             &large->move_to,
                                             &small->move_to))
        {}

        /* No usable equijoin preds, or couldn't consider the preferred motion.
         * Replicate one rel if possible.
         * MPP TODO: Consider number of seg dbs per host.
         */
        else if (!small->require_existing_order &&
                 small->ok_to_replicate)
            CdbPathLocus_MakeReplicated(&small->move_to);
        else if (!large->require_existing_order &&
                 large->ok_to_replicate)
            CdbPathLocus_MakeReplicated(&large->move_to);

        /* Last resort: Move both rels to a single qExec. */
        else
        {
            CdbPathLocus_MakeSingleQE(&outer.move_to);
            CdbPathLocus_MakeSingleQE(&inner.move_to);
        }
    }                                       /* partitioned */

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
        if (!outer.path)            /* fail if outer motion not feasible */
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
        if (!inner.path)            /* fail if inner motion not feasible */
            goto fail;
    }

    /*
     * Ok to join.  Give modified subpaths to caller.
     */
    *p_outer_path = outer.path;
    *p_inner_path = inner.path;

    /* Tell caller where the join will be done. */
    return cdbpathlocus_join(outer.path->locus, inner.path->locus);

fail:                           /* can't do this join */
    CdbPathLocus_MakeNull(&outer.move_to);
    return outer.move_to;
}                               /* cdbpath_motion_for_join */


/*
 * cdbpath_dedup_fixup
 *      Modify path to support unique rowid operation for subquery preds.
 */

typedef struct CdbpathDedupFixupContext
{
    PlannerInfo    *root;
    Relids          distinct_on_rowid_relids;
    List           *rowid_vars;
    int32           subplan_id;
    bool            need_subplan_id;
    bool            need_segment_id;
} CdbpathDedupFixupContext;

static CdbVisitOpt
cdbpath_dedup_fixup_walker(Path *path, void *context);


/* Drop Var nodes from a List unless they belong to a given set of relids. */
static List *
cdbpath_dedup_pickvars(List *vars, Relids relids_to_keep)
{
    ListCell       *cell;
    ListCell       *nextcell;
    ListCell       *prevcell = NULL;
    Var            *var;

    for (cell = list_head(vars); cell; cell = nextcell)
    {
        nextcell = lnext(cell);
        var = (Var *)lfirst(cell);
        Assert(IsA(var, Var));
        if (!bms_is_member(var->varno, relids_to_keep))
            vars = list_delete_cell(vars, cell, prevcell);
        else
            prevcell = cell;
    }
    return vars;
}                               /* cdbpath_dedup_pickvars */

static CdbVisitOpt
cdbpath_dedup_fixup_unique(UniquePath *uniquePath, CdbpathDedupFixupContext *ctx)
{
    Relids      downstream_relids = ctx->distinct_on_rowid_relids;
    List       *ctid_exprs;
	List	   *ctid_operators;
    List       *other_vars = NIL;
    List       *other_operators = NIL;
    List       *partkey = NIL;
    List       *eq = NIL;
    ListCell   *cell;
    bool        save_need_segment_id = ctx->need_segment_id;

    Assert(!ctx->rowid_vars);

    /*
     * Leave this node unchanged unless it removes duplicates by row id.
     *
     * NB. If ctx->distinct_on_rowid_relids is nonempty, row id vars
     * could be added to our rel's targetlist while visiting the child
     * subtree.  Any such added columns should pass on safely through this
     * Unique op because they aren't added to the distinct_on_exprs list.
     */
    if (bms_is_empty(uniquePath->distinct_on_rowid_relids))
        return CdbVisit_Walk;   /* onward to visit the kids */

    /* No action needed if data is trivially unique. */
    if (uniquePath->umethod == UNIQUE_PATH_NOOP ||
        uniquePath->umethod == UNIQUE_PATH_LIMIT1)
        return CdbVisit_Walk;   /* onward to visit the kids */

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

    /* CDB TODO: we share kid's targetlist at present, so our tlist could
     * contain rowid vars which are no longer needed downstream.
     */

    /*
     * Build DISTINCT ON key for UniquePath, putting the ctid columns first
     * because those are usually more distinctive than the segment ids.
     * Also build repartitioning key if needed, using only the ctid columns.
     */
	ctid_exprs = NIL;
	ctid_operators = NIL;
	foreach(cell, ctx->rowid_vars)
    {
        Var        *var = (Var *)lfirst(cell);

        Assert(IsA(var, Var) &&
               bms_is_member(var->varno, ctx->distinct_on_rowid_relids));

        /* Skip vars which aren't part of the row id for this Unique op. */
        if (!bms_is_member(var->varno, uniquePath->distinct_on_rowid_relids))
            continue;

        /* ctid? */
        if (var->varattno == SelfItemPointerAttributeNumber)
        {
            /*
             * The tid type has a full set of comparison operators, but
             * oddly its "=" operator is not marked hashable.  So 'ctid'
             * is directly usable for sorted duplicate removal; but we
             * cast it to 64-bit integer for hashed duplicate removal.
             */
            if (uniquePath->umethod == UNIQUE_PATH_HASH)
			{
                ctid_exprs = lappend(ctid_exprs,
                                     makeFuncExpr(CDB_PROC_TIDTOI8, INT8OID,
                                                  list_make1(var),
                                                  COERCE_EXPLICIT_CAST));
				ctid_operators = lappend_oid(ctid_operators, Int8EqualOperator);
			}
            else
			{
                ctid_exprs = lappend(ctid_exprs, var);
				ctid_operators = lappend_oid(ctid_operators, TIDEqualOperator);
			}

			/* Add to repartitioning key.  Can use tid type without coercion. */
			if (uniquePath->must_repartition)
			{
				PathKey    *cpathkey;

				if (!eq)
					eq = list_make1(makeString("="));
				cpathkey = cdb_make_pathkey_for_expr(ctx->root, (Node *) var, eq, false);
				partkey = lappend(partkey, cpathkey);
			}
        }

        /* other uniqueifiers such as gp_segment_id */
        else
		{
			Operator	optup;
			Oid			eqop;

			other_vars = lappend(other_vars, var);

			optup = equality_oper(exprType((Node *) var), false);
			eqop = oprid(optup);
			ReleaseSysCache(optup);

			other_operators = lappend_oid(other_operators, eqop);
		}
    }

    uniquePath->distinct_on_exprs = list_concat(ctid_exprs, other_vars);
	uniquePath->distinct_on_eq_operators = list_concat(ctid_operators, other_operators);

    /* To repartition, add a MotionPath below this UniquePath. */
    if (uniquePath->must_repartition)
    {
        CdbPathLocus    locus;

        Assert(partkey);
        CdbPathLocus_MakeHashed(&locus, partkey);

        uniquePath->subpath = cdbpath_create_motion_path(ctx->root,
                                                         uniquePath->subpath,
                                                         NIL,
                                                         false,
                                                         locus);
        Insist(uniquePath->subpath);
        uniquePath->path.locus = uniquePath->subpath->locus;
        uniquePath->path.motionHazard = uniquePath->subpath->motionHazard;
        uniquePath->path.rescannable = uniquePath->subpath->rescannable;
        list_free_deep(eq);
    }

    /* Prune row id var list to remove items not needed downstream. */
    ctx->rowid_vars = cdbpath_dedup_pickvars(ctx->rowid_vars, downstream_relids);

    bms_free(ctx->distinct_on_rowid_relids);
    ctx->distinct_on_rowid_relids = downstream_relids;
    return CdbVisit_Skip;       /* we visited kids already; done with subtree */
}                               /* cdbpath_dedup_fixup_unique */

static void
cdbpath_dedup_fixup_baserel(Path *path, CdbpathDedupFixupContext *ctx)
{
    RelOptInfo *rel = path->parent;
    List       *rowid_vars = NIL;
    Const      *con;
    Var        *var;

    Assert(!ctx->rowid_vars);

    /* Find or make a Var node referencing our 'ctid' system attribute. */
    var = find_indexkey_var(ctx->root, rel, SelfItemPointerAttributeNumber);
    rowid_vars = lappend(rowid_vars, var);

    /*
     * If below a Motion operator, make a Var node for our 'gp_segment_id' attr.
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
     * If below an Append, add 'gp_subplan_id' pseudo column to the targetlist.
     *
     * set_plan_references() will later replace the pseudo column Var node
     * in our rel's targetlist with a copy of its defining expression, i.e.
     * the Const node built here.
     */
    if (ctx->need_subplan_id)
    {
        /* Make a Const node containing the current subplan id. */
        con = makeConst(INT4OID, -1, sizeof(int32), Int32GetDatum(ctx->subplan_id),
                        false, true);

        /* Set up a pseudo column whose value will be the constant. */
        var = cdb_define_pseudo_column(ctx->root, rel, "gp_subplan_id",
                                       (Expr *)con, sizeof(int32));

        /* Give downstream operators a Var referencing the pseudo column. */
        rowid_vars = lappend(rowid_vars, var);
    }

    /* Add these vars to the rel's list of result columns. */
    add_vars_to_targetlist(ctx->root, rowid_vars, ctx->distinct_on_rowid_relids);

    /* Recalculate width of the rel's result rows. */
    set_rel_width(ctx->root, rel);

    /*
     * Tell caller to add our vars to the DISTINCT ON key of the ancestral
     * UniquePath, and to the targetlists of any intervening ancestors.
     */
    ctx->rowid_vars = rowid_vars;
}                               /* cdbpath_dedup_fixup_baserel */

static void
cdbpath_dedup_fixup_joinrel(JoinPath *joinpath, CdbpathDedupFixupContext *ctx)
{
    RelOptInfo *rel = joinpath->path.parent;

    Assert(!ctx->rowid_vars);

    /* CDB TODO: Subpath id isn't needed from both outer and inner.
     * Don't request row id vars from rhs of EXISTS join.
     */

    /* Get row id vars from outer subpath. */
    if (joinpath->outerjoinpath)
        pathnode_walk_node(joinpath->outerjoinpath, cdbpath_dedup_fixup_walker, ctx);

    /* Get row id vars from inner subpath. */
    if (joinpath->innerjoinpath)
    {
        List   *outer_rowid_vars = ctx->rowid_vars;

        ctx->rowid_vars = NIL;
        pathnode_walk_node(joinpath->innerjoinpath, cdbpath_dedup_fixup_walker, ctx);

        /* Which rel has more rows?  Put its row id vars in front. */
        if (outer_rowid_vars &&
            ctx->rowid_vars &&
            cdbpath_rows(ctx->root, joinpath->outerjoinpath) >= cdbpath_rows(ctx->root, joinpath->innerjoinpath))
            ctx->rowid_vars = list_concat(outer_rowid_vars, ctx->rowid_vars);
        else
            ctx->rowid_vars = list_concat(ctx->rowid_vars, outer_rowid_vars);
    }

    /* Update joinrel's targetlist and adjust row width. */
    if (ctx->rowid_vars)
        build_joinrel_tlist(ctx->root, rel, ctx->rowid_vars);
}                               /* cdbpath_dedup_fixup_joinrel */

static void
cdbpath_dedup_fixup_motion(CdbMotionPath *motionpath, CdbpathDedupFixupContext *ctx)
{
    bool        save_need_segment_id = ctx->need_segment_id;

    /*
     * Motion could bring together rows which happen to have the same ctid
     * but are actually from different segments.  They must not be treated
     * as duplicates.  To distinguish them, let each row be labeled with
     * its originating segment id.
     */
    ctx->need_segment_id = true;

    /* Visit the upstream nodes. */
    pathnode_walk_node(motionpath->subpath, cdbpath_dedup_fixup_walker, ctx);

    /* Restore saved flag. */
    ctx->need_segment_id = save_need_segment_id;
}                               /* cdbpath_dedup_fixup_motion */

static void
cdbpath_dedup_fixup_append(AppendPath *appendPath, CdbpathDedupFixupContext *ctx)
{
    Relids      save_distinct_on_rowid_relids = ctx->distinct_on_rowid_relids;
    List       *appendrel_rowid_vars;
    ListCell   *cell;
    int         ncol;
    bool        save_need_subplan_id = ctx->need_subplan_id;

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
     * Append could bring together rows which happen to have the same ctid
     * but are actually from different tables or different branches of a
     * UNION ALL.  They must not be treated as duplicates.  To distinguish
     * them, let each row be labeled with an integer which will be different
     * for each branch of the Append.
     */
    ctx->need_subplan_id = true;

    /* Assign a dummy subplan id (not actually used) for the appendrel. */
    ctx->subplan_id++;

    /* Add placeholder columns to the appendrel's targetlist. */
    cdbpath_dedup_fixup_baserel((Path *)appendPath, ctx);
    ncol = list_length(appendPath->path.parent->reltargetlist);

    appendrel_rowid_vars = ctx->rowid_vars;
    ctx->rowid_vars = NIL;

    /* Update the parent and child rels. */
    foreach(cell, appendPath->subpaths)
    {
        Path       *subpath = (Path *)lfirst(cell);

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
            ereport(ERROR, (errcode(ERRCODE_CDB_FEATURE_NOT_YET),
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
}                               /* cdbpath_dedup_fixup_append */

	static CdbVisitOpt
cdbpath_dedup_fixup_walker(Path *path, void *context)
{
	CdbpathDedupFixupContext   *ctx = (CdbpathDedupFixupContext *)context;

	Assert(!ctx->rowid_vars);

	/* Watch for a UniquePath node calling for removal of dups by row id. */
	if (path->pathtype == T_Unique)
		return cdbpath_dedup_fixup_unique((UniquePath *)path, ctx);

	/* Leave node unchanged unless a downstream Unique op needs row ids. */
	if (!bms_overlap(path->parent->relids, ctx->distinct_on_rowid_relids))
		return CdbVisit_Walk;       /* visit descendants */

	/* Alter this node to produce row ids for an ancestral Unique operator. */
	switch (path->pathtype)
	{
		case T_Append:
			cdbpath_dedup_fixup_append((AppendPath *)path, ctx);
			break;

		case T_SeqScan:
		case T_ExternalScan:
		case T_AppendOnlyScan:
		case T_AOCSScan:
		case T_IndexScan:
		case T_BitmapHeapScan:
		case T_BitmapAppendOnlyScan:
		case T_BitmapTableScan:
		case T_TidScan:
		case T_SubqueryScan:
		case T_FunctionScan:
		case T_ValuesScan:
		case T_CteScan:
			cdbpath_dedup_fixup_baserel(path, ctx);
			break;

		case T_HashJoin:
		case T_MergeJoin:
		case T_NestLoop:
			cdbpath_dedup_fixup_joinrel((JoinPath *)path, ctx);
			break;

		case T_Result:
		case T_Material:
			/* These nodes share child's RelOptInfo and don't need fixup. */
			return CdbVisit_Walk;           /* visit descendants */

		case T_Motion:
			cdbpath_dedup_fixup_motion((CdbMotionPath *)path, ctx);
			break;

		default:
			Insist(0);
	}
	return CdbVisit_Skip;       /* already visited kids, don't revisit them */
}                               /* cdbpath_dedup_fixup_walker */

void
cdbpath_dedup_fixup(PlannerInfo *root, Path *path)
{
    CdbpathDedupFixupContext    context;

    memset(&context, 0, sizeof(context));

    context.root = root;

    pathnode_walk_node(path, cdbpath_dedup_fixup_walker, &context);

    Assert(bms_is_empty(context.distinct_on_rowid_relids) &&
           !context.rowid_vars &&
           !context.need_segment_id &&
           !context.need_subplan_id);
}                               /* cdbpath_dedup_fixup */
