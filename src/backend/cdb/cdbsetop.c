/*-------------------------------------------------------------------------
 *
 * cdbsetop.c
 *	  Routines to aid in planning set-operation queries for parallel
 *    execution.  This is, essentially, an extension of the file
 *    optimizer/prep/prepunion.c, although some functions are not
 *    externalized.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbsetop.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "nodes/makefuncs.h"
#include "optimizer/planmain.h"

#include "cdb/cdbhash.h"
#include "cdb/cdbmutate.h"
#include "cdb/cdbpath.h"
#include "cdb/cdbsetop.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"


/*
 * Function: choose_setop_type
 *
 * Decide what type of plan to use for a set operation based on the loci of
 * the node list input to the set operation.
 *
 * See the comments in cdbsetop.h for discussion of types of setop plan.
 */
GpSetOpType
choose_setop_type(List *pathlist)
{
	ListCell   *cell;
	bool		ok_general = true;
	bool		ok_partitioned = true;
	bool		ok_single_qe = true;
	bool		has_partitioned = false;

	Assert(Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_UTILITY);

	foreach(cell, pathlist)
	{
		Path	   *subpath = (Path *) lfirst(cell);

		switch (subpath->locus.locustype)
		{
			case CdbLocusType_Hashed:
			case CdbLocusType_HashedOJ:
			case CdbLocusType_Strewn:
				ok_general = false;
				has_partitioned = true;
				break;

			case CdbLocusType_Entry:
				ok_general = ok_partitioned = ok_single_qe = false;
				break;

			case CdbLocusType_OuterQuery:
				return PSETOP_SEQUENTIAL_OUTERQUERY;

			case CdbLocusType_SingleQE:
				ok_general = false;
				break;

			case CdbLocusType_SegmentGeneral:
				ok_general = false;
				break;

			case CdbLocusType_General:
				break;

			case CdbLocusType_Replicated:
				break;

			case CdbLocusType_Null:
				elog(ERROR, "unexpected Null locus in set operation branch");
				break;
			default:
				elog(ERROR, "unexpected locus type in set operation branch");
				break;
		}
	}

	if (ok_general)
		return PSETOP_GENERAL;
	else if (ok_partitioned && has_partitioned)
		return PSETOP_PARALLEL_PARTITIONED;
	else if (ok_single_qe)
		return PSETOP_SEQUENTIAL_QE;

	return PSETOP_SEQUENTIAL_QD;
}


void
adjust_setop_arguments(PlannerInfo *root, List *pathlist, List *tlist_list, GpSetOpType setop_type)
{
	ListCell   *pathcell;
	ListCell   *tlistcell;
	Path	   *adjusted_path;
	CdbPathLocus locus;

	forboth(pathcell, pathlist, tlistcell, tlist_list)
	{
		Path	   *subpath = (Path *) lfirst(pathcell);
		List	   *subtlist = (List *) lfirst(tlistcell);

		adjusted_path = subpath;
		switch (setop_type)
		{
			case PSETOP_GENERAL:
				/* This only occurs when all arguments are general. */
				break;

			case PSETOP_PARALLEL_PARTITIONED:
				switch (subpath->locus.locustype)
				{
					case CdbLocusType_Hashed:
					case CdbLocusType_HashedOJ:
					case CdbLocusType_Strewn:
						break;
					case CdbLocusType_SingleQE:
					case CdbLocusType_General:
					case CdbLocusType_SegmentGeneral:
						/*
						 * The setop itself will run on an N-gang, so we need
						 * to arrange for the singleton input to be separately
						 * dispatched to a 1-gang and collect its result on
						 * one of our N QEs. Hence ...
						 */
						adjusted_path = make_motion_hash_all_targets(root, subpath, subtlist);
						break;
					case CdbLocusType_Null:
					case CdbLocusType_Entry:
					case CdbLocusType_Replicated:
					default:
						ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
										errmsg("unexpected argument locus to set operation")));
						break;
				}
				break;

			case PSETOP_SEQUENTIAL_QD:
				switch (subpath->locus.locustype)
				{
					case CdbLocusType_Hashed:
					case CdbLocusType_HashedOJ:
					case CdbLocusType_Strewn:
						CdbPathLocus_MakeEntry(&locus);
						adjusted_path = cdbpath_create_motion_path(root, subpath, NULL, false,
																   locus);
						break;

					case CdbLocusType_SingleQE:
						/*
						 * The input was focused on a single QE, but we need it in the QD.
						 * It's bit silly to add a Motion to just move the whole result from
						 * single QE to QD, it would be better to produce the result in the
						 * QD in the first place, and avoid the Motion. But it's too late
						 * to modify the subpath.
						 */
						CdbPathLocus_MakeEntry(&locus);
						adjusted_path = cdbpath_create_motion_path(root, subpath, NULL, false,
																   locus);
						break;

					case CdbLocusType_Entry:
					case CdbLocusType_General:
					case CdbLocusType_OuterQuery:
						break;

					case CdbLocusType_Null:
					case CdbLocusType_Replicated:
					default:
						ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
										errmsg("unexpected argument locus to set operation")));
						break;
				}
				break;

			case PSETOP_SEQUENTIAL_QE:
			case PSETOP_SEQUENTIAL_OUTERQUERY:
				switch (subpath->locus.locustype)
				{
					case CdbLocusType_Hashed:
					case CdbLocusType_HashedOJ:
					case CdbLocusType_Strewn:
						/* Gather to QE.  No need to keep ordering. */
						CdbPathLocus_MakeSingleQE(&locus, getgpsegmentCount());
						adjusted_path = cdbpath_create_motion_path(root, subpath, NULL, false,
																   locus);
						break;

					case CdbLocusType_SingleQE:
						break;

					case CdbLocusType_OuterQuery:
					case CdbLocusType_General:
						break;

					case CdbLocusType_SegmentGeneral:
						/* Gather to QE.  No need to keep ordering. */
						CdbPathLocus_MakeSingleQE(&locus, getgpsegmentCount());
						adjusted_path = cdbpath_create_motion_path(root, subpath, NULL, false,
																   locus);
						break;

					case CdbLocusType_Entry:
					case CdbLocusType_Null:
					case CdbLocusType_Replicated:
					default:
						ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
										errmsg("unexpected argument locus to set operation")));
						break;
				}
				break;

			default:
				/* Can't happen! */
				ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
								errmsg("unexpected arguments to set operation")));
				break;
		}

		/* If we made changes, inject them into the argument list. */
		if (subpath != adjusted_path)
		{
			subpath = adjusted_path;
			pathcell->data.ptr_value = subpath;
		}
	}

	return;
}

/*
 * make_motion_hash_all_targets
 *		Add a Motion node atop the given subplath to hash collocate
 *      tuples non-distinct on the non-junk attributes.  This motion
 *      should only be applied to a non-replicated, non-root subpath.
 *
 * This will align with the sort attributes used as input to a SetOp
 * or Unique operator. This is used in path for UNION and other
 * set-operations that implicitly do a DISTINCT on the whole target
 * list.
 */
Path *
make_motion_hash_all_targets(PlannerInfo *root, Path *subpath, List *tlist)
{
	ListCell   *cell;
	List	   *hashexprs = NIL;
	List	   *hashopfamilies = NIL;
	List	   *hashsortrefs = NIL;
	CdbPathLocus locus;

	foreach(cell, tlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(cell);
		Oid			opfamily;

		if (tle->resjunk)
			continue;

		opfamily = cdb_default_distribution_opfamily_for_type(exprType((Node *) tle->expr));
		if (!opfamily)
			continue;		/* not hashable */

		hashexprs = lappend(hashexprs, copyObject(tle->expr));
		hashopfamilies = lappend_oid(hashopfamilies, opfamily);
		hashsortrefs = lappend_int(hashsortrefs, tle->ressortgroupref);
	}

	if (hashexprs)
	{
		/* Distribute to ALL to maximize parallelism */
		locus = cdbpathlocus_from_exprs(root,
										subpath->parent,
										hashexprs,
										hashopfamilies,
										hashsortrefs,
										getgpsegmentCount());
	}
	else
	{
		/*
		 * Degenerate case, where none of the columns are hashable.
		 *
		 * (If the caller knew this, it probably would have been better to
		 * produce a different plan, with Sorts in the segments, and an
		 * order-preserving gather on the top.)
		 */
		CdbPathLocus_MakeSingleQE(&locus, getgpsegmentCount());
	}

	return cdbpath_create_motion_path(root, subpath, subpath->pathkeys,
									  false, locus);
}

/*
 *     Marks an Append plan with its locus based on the set operation
 *     type determined during examination of the arguments.
 */
void
mark_append_locus(Path *path, GpSetOpType optype)
{
	switch (optype)
	{
		case PSETOP_GENERAL:
			CdbPathLocus_MakeGeneral(&path->locus);
			break;
		case PSETOP_PARALLEL_PARTITIONED:
			CdbPathLocus_MakeStrewn(&path->locus, getgpsegmentCount());
			break;
		case PSETOP_SEQUENTIAL_QD:
			CdbPathLocus_MakeEntry(&path->locus);
			break;
		case PSETOP_SEQUENTIAL_QE:
			CdbPathLocus_MakeSingleQE(&path->locus, getgpsegmentCount());
			break;
		case PSETOP_SEQUENTIAL_OUTERQUERY:
			CdbPathLocus_MakeOuterQuery(&path->locus);
			break;
		case PSETOP_NONE:
			break;
	}
}
