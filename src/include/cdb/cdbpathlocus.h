/*-------------------------------------------------------------------------
 *
 * cdbpathlocus.h
 *
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbpathlocus.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBPATHLOCUS_H
#define CDBPATHLOCUS_H

#include "nodes/pg_list.h"      /* List */
#include "nodes/bitmapset.h"    /* Bitmapset */

struct Path;                    /* defined in relation.h */
struct RelOptInfo;              /* defined in relation.h */
struct PlannerInfo;				/* defined in relation.h */
struct GpPolicy;				/* defined in gp_distribution_policy.h */
struct PathTarget;


/*
 * CdbLocusType
 *
 * The difference between SegmentGeneral and Replicated is that a path
 * with Replicated locus *must* be executed on all of the segments, whereas
 * a SegmentGeneral *may* be executed on all of the segments, or just one
 * of them, as is convenient. Replicated is used as the locus for
 * UPDATEs/DELETEs/INSERTs to replicated tables; it's important that the
 * plan gets executed on all segments so that all segments are updated.
 * SegmentGeneral is used when querying replicated tables.
 */
typedef enum CdbLocusType
{
    CdbLocusType_Null,
    CdbLocusType_Entry,         /* a single backend process on the entry db:
                                 * usually the qDisp itself, but could be a
                                 * qExec started by the entry postmaster.
                                 */
    CdbLocusType_SingleQE,      /* a single backend process on any db: the
                                 * qDisp itself, or a qExec started by a
                                 * segment postmaster or the entry postmaster.
                                 */
    CdbLocusType_General,       /* compatible with any locus (data is
                                 * self-contained in the query plan or
                                 * generally available in any qExec or qDisp) */
    CdbLocusType_SegmentGeneral,/* generally available in any qExec, but not
								 * available in qDisp */
	CdbLocusType_OuterQuery,	/* generally available in any qExec or qDisp, but
								 * contains correlated vars from outer query, so must
								 * not be redistributed */
    CdbLocusType_Replicated,    /* replicated over all qExecs of an N-gang */
    CdbLocusType_Hashed,        /* hash partitioned over all qExecs of N-gang */
    CdbLocusType_HashedOJ,      /* result of hash partitioned outer join, NULLs can be anywhere */
    CdbLocusType_Strewn,        /* partitioned on no known function */
    CdbLocusType_End            /* = last valid CdbLocusType + 1 */
} CdbLocusType;

#define CdbLocusType_IsValid(locustype)     \
    ((locustype) > CdbLocusType_Null &&     \
     (locustype) < CdbLocusType_End)

#define CdbLocusType_IsValidOrNull(locustype)   \
    ((locustype) >= CdbLocusType_Null &&        \
     (locustype) < CdbLocusType_End)

/*
 * CdbPathLocus
 *
 * Specifies a distribution of tuples across segments.
 *
 * If locustype is CdbLocusType_Hashed or CdbLocusType_HashedOJ:
 *      Rows are distributed based on values of a partitioning key.  The
 *      partitioning key is often called a "distribution key", to avoid
 *      confusion with table partitioning.
 *
 *      A tuple's partitioning key consists of one or more key columns.
 *      When CdbPathLocus represents the distribution of a table, the
 *      partitioning key corresponds to the columns listed in DISTRIBUTED BY
 *      of the table, but the planner can distribute intermediate results
 *      based on arbitrary expressions.
 *
 *      The partitioning key is represented by a List of DistributionKeys,
 *      one for each key column. Each DistributionKey contains a list of
 *      EquivalenceClasses, which contain expressions that can be used
 *      to compute the value for the key column. Any of the expressions
 *      can be used to compute the value, depending on what relations are
 *      available at that part of the plan.
 *
 *      For example, if the query contains a "WHERE a=b" clause, the planner
 *      would form an EquivalenceClass that contains two members, "a" and "b".
 *      Because of the WHERE clause, either "a" and "b" can be used to
 *      compute the hash value. Usually, a DistributionKey contains only one
 *      EquivalenceClass, because whenever there is an equijoin on two
 *      expressions, the planner puts them in the same EquivalenceClass.
 *      However, if there are FULL JOINs in the query, the FULL JOIN quals do
 *      not form equivalence classes with other quals, because the NULL
 *      handling is different. See src/backend/optimizer/README for
 *      discussion on "outerjoin delayed" equivalence classes.
 *
 *      When a path locus is constructed for a FULL JOIN, CdbLocusType_HashedOJ
 *      is used instead of CdbLocusType_Hashed. The important distinction
 *      between Hashed and HashedOJ is the semantics for NULLs. In a Hashed
 *      distribution, a NULL is hashed like any other value, and all NULLs are
 *      located on a particular segment, based on the hash value of a NULL
 *      datum. But with HashedOJ, NULL values can legitimately appear on any
 *      segment!
 *
 *      For join optimization, Hashed and HashedOJ can both be used. In an inner
 *      join on A=B, NULL rows won't match anyway. And for an OUTER JOIN, it
 *      doesn't matter which segment the NULL rows appear on, as long as we
 *      correctly mark the resulting locus also as HashedOJ. But for grouping,
 *      HashedOJ can not be used, because you might end up with multiple NULL
 *      NULL groups, one for each segment!
 *
 * If locustype == CdbLocusType_Strewn:
 *      Rows are distributed according to a criterion that is unknown or
 *      may depend on inputs that are unknown or unavailable in the present
 *      context.  The 'distkey' field is NIL.
 *
 * If the distribution is not partitioned, then the 'distkey' field is NIL.
 *
 * The numsegments attribute specifies how many segments the tuples are
 * distributed on, from segment 0 to segment `numsegments-1`.  In the future
 * we might further change it to a range or list so discontinuous segments
 * can be described.  This numsegments has different meaning for different
 * locustype:
 * - Null: numsegments is usually meaningless in Null locus as it will be
 *   remade to other locus types later.  But there is also cases that we set
 *   a valid numsegments in Null locus, this value will be kept when remade
 *   it to other locus types, and it becomes meaningful after that;
 * - Entry: numsegments in Entry locus specify the candidate segments to put
 *   the Entry node on, it's master and all the primary segments in current
 *   implementation;
 * - SingleQE: numsegments in SingleQE locus specify the candidate segments
 *   to put the SingleQE node on, although SingleQE is always executed on one
 *   segment but numsegments usually have a value > 1;
 * - General: similar with Entry and SingleQE;
 * - SegmentGeneral, Replicated, Hashed, HashedOJ, Strewn: numsegments in
 *   these locus types specify the segments that contain the tuples;
 */
typedef struct CdbPathLocus
{
	CdbLocusType locustype;
	List	   *distkey;		/* List of DistributionKeys */
	int			numsegments;
} CdbPathLocus;

#define CdbPathLocus_NumSegments(locus)         \
            ((locus).numsegments)

/*
 * CdbPathLocus_IsEqual
 *
 * Returns true if one CdbPathLocus is exactly the same as the other.
 * To test for logical equivalence, use cdbpathlocus_equal() instead.
 */
#define CdbPathLocus_IsEqual(a, b)              \
            ((a).locustype == (b).locustype &&  \
             (a).numsegments == (b).numsegments && \
             (a).distkey == (b).distkey)

#define CdbPathLocus_CommonSegments(a, b) \
            Min((a).numsegments, (b).numsegments)

/*
 * CdbPathLocus_IsBottleneck
 *
 * Returns true if the locus indicates confinement to a single process:
 * either a singleton qExec (1-gang) on a segment db or the entry db, or
 * the qDisp process.
 */
#define CdbPathLocus_IsBottleneck(locus)        \
            (CdbPathLocus_IsEntry(locus) ||     \
			 CdbPathLocus_IsOuterQuery(locus) ||     \
             CdbPathLocus_IsSingleQE(locus))

/*
 * CdbPathLocus_IsPartitioned
 *
 * Returns true if the locus indicates partitioning across an N-gang, such
 * that each qExec has a disjoint subset of the rows.
 */
#define CdbPathLocus_IsPartitioned(locus)       \
            (CdbPathLocus_IsHashed(locus) ||    \
             CdbPathLocus_IsHashedOJ(locus) ||  \
             CdbPathLocus_IsStrewn(locus))

#define CdbPathLocus_IsNull(locus)          \
            ((locus).locustype == CdbLocusType_Null)
#define CdbPathLocus_IsEntry(locus)         \
            ((locus).locustype == CdbLocusType_Entry)
#define CdbPathLocus_IsSingleQE(locus)      \
            ((locus).locustype == CdbLocusType_SingleQE)
#define CdbPathLocus_IsGeneral(locus)       \
            ((locus).locustype == CdbLocusType_General)
#define CdbPathLocus_IsReplicated(locus)    \
            ((locus).locustype == CdbLocusType_Replicated)
#define CdbPathLocus_IsHashed(locus)        \
            ((locus).locustype == CdbLocusType_Hashed)
#define CdbPathLocus_IsHashedOJ(locus)      \
            ((locus).locustype == CdbLocusType_HashedOJ)
#define CdbPathLocus_IsStrewn(locus)        \
            ((locus).locustype == CdbLocusType_Strewn)
#define CdbPathLocus_IsSegmentGeneral(locus)        \
            ((locus).locustype == CdbLocusType_SegmentGeneral)
#define CdbPathLocus_IsOuterQuery(locus)        \
            ((locus).locustype == CdbLocusType_OuterQuery)

#define CdbPathLocus_MakeSimple(plocus, _locustype, numsegments_) \
    do {                                                \
        CdbPathLocus *_locus = (plocus);                \
        _locus->locustype = (_locustype);               \
        _locus->numsegments = (numsegments_);                        \
        _locus->distkey = NIL;                        \
    } while (0)

#define CdbPathLocus_MakeNull(plocus)                   \
            CdbPathLocus_MakeSimple((plocus), CdbLocusType_Null, -1)
#define CdbPathLocus_MakeEntry(plocus)                  \
            CdbPathLocus_MakeSimple((plocus), CdbLocusType_Entry, -1)
#define CdbPathLocus_MakeSingleQE(plocus, numsegments_)               \
            CdbPathLocus_MakeSimple((plocus), CdbLocusType_SingleQE, (numsegments_))
#define CdbPathLocus_MakeGeneral(plocus)                \
            CdbPathLocus_MakeSimple((plocus), CdbLocusType_General, -1)
#define CdbPathLocus_MakeSegmentGeneral(plocus, numsegments_)                \
            CdbPathLocus_MakeSimple((plocus), CdbLocusType_SegmentGeneral, (numsegments_))
#define CdbPathLocus_MakeReplicated(plocus, numsegments_)             \
            CdbPathLocus_MakeSimple((plocus), CdbLocusType_Replicated, (numsegments_))
#define CdbPathLocus_MakeHashed(plocus, distkey_, numsegments_)       \
    do {                                                \
        CdbPathLocus *_locus = (plocus);                \
        _locus->locustype = CdbLocusType_Hashed;		\
        _locus->numsegments = (numsegments_);           \
        _locus->distkey = (distkey_);					\
        Assert(cdbpathlocus_is_valid(*_locus));         \
    } while (0)
#define CdbPathLocus_MakeHashedOJ(plocus, distkey_, numsegments_)     \
    do {                                                \
        CdbPathLocus *_locus = (plocus);                \
        _locus->locustype = CdbLocusType_HashedOJ;		\
        _locus->numsegments = (numsegments_);           \
        _locus->distkey = (distkey_);					\
        Assert(cdbpathlocus_is_valid(*_locus));         \
    } while (0)
#define CdbPathLocus_MakeStrewn(plocus, numsegments_)                 \
            CdbPathLocus_MakeSimple((plocus), CdbLocusType_Strewn, (numsegments_))

#define CdbPathLocus_MakeOuterQuery(plocus)                 \
	CdbPathLocus_MakeSimple((plocus), CdbLocusType_OuterQuery, -1)

/************************************************************************/

extern bool cdbpathlocus_equal(CdbPathLocus a, CdbPathLocus b);

/************************************************************************/

extern CdbPathLocus cdbpathlocus_for_insert(struct PlannerInfo *root,
											struct GpPolicy *policy,
											struct PathTarget *pathtarget);

CdbPathLocus
cdbpathlocus_from_policy(struct PlannerInfo *root, Index rti, struct GpPolicy *policy);
CdbPathLocus
cdbpathlocus_from_baserel(struct PlannerInfo   *root,
                          struct RelOptInfo    *rel);
CdbPathLocus
cdbpathlocus_from_exprs(struct PlannerInfo     *root,
						struct RelOptInfo *rel,
                        List                   *hash_on_exprs,
						List *hash_opclasses,
						List *hash_sortrefs,
                        int                     numsegments);
CdbPathLocus
cdbpathlocus_from_subquery(struct PlannerInfo  *root,
						   struct RelOptInfo   *rel,
						   struct Path         *subpath);

CdbPathLocus
cdbpathlocus_join(JoinType jointype, CdbPathLocus a, CdbPathLocus b);

/************************************************************************/

/*
 * cdbpathlocus_get_distkey_exprs
 *
 * Returns a List with one Expr for each distkey column.  Each item either is
 * in the given targetlist, or has no Var nodes that are not in the targetlist;
 * and uses only rels in the given set of relids.  Returns NIL if the
 * distkey cannot be expressed in terms of the given relids and targetlist.
 */
extern void cdbpathlocus_get_distkey_exprs(CdbPathLocus locus,
							   Bitmapset *relids,
							   List *targetlist,
							   List **exprs_p,
							   List **opfamilies_p);

/*
 * cdbpathlocus_pull_above_projection
 *
 * Given a targetlist, and a locus evaluable before the projection is
 * applied, returns an equivalent locus evaluable after the projection.
 * Returns a strewn locus if the necessary inputs are not projected.
 *
 * 'relids' is the set of relids that may occur in the targetlist exprs.
 * 'targetlist' specifies the projection.  It is a List of TargetEntry
 *      or merely a List of Expr.
 * 'newvarlist' is an optional List of Expr, in 1-1 correspondence with
 *      'targetlist'.  If specified, instead of creating a Var node to
 *      reference a targetlist item, we plug in a copy of the corresponding
 *      newvarlist item.
 * 'newrelid' is the RTE index of the projected result, for finding or
 *      building Var nodes that reference the projected columns.
 *      Ignored if 'newvarlist' is specified.
 */
CdbPathLocus
cdbpathlocus_pull_above_projection(struct PlannerInfo  *root,
                                   CdbPathLocus         locus,
                                   Bitmapset           *relids,
                                   List                *targetlist,
                                   List                *newvarlist,
                                   Index                newrelid);

/************************************************************************/

/*
 * cdbpathlocus_is_hashed_on_exprs
 *
 * This function tests whether grouping on a given set of exprs can be done
 * in place without motion.
 *
 * For a hashed locus, returns false if the distkey has a column whose
 * equivalence class contains no expr belonging to the given list.
 *
 * If 'ignore_constants' is true, any constants in the locus are ignored.
 */
bool
cdbpathlocus_is_hashed_on_exprs(CdbPathLocus locus, List *exprlist, bool ignore_constants);

/*
 * cdbpathlocus_is_hashed_on_eclasses
 *
 * This function tests whether grouping on a given set of exprs can be done
 * in place without motion.
 *
 * For a hashed locus, returns false if the distkey has any column whose
 * equivalence class is not in 'eclasses' list.
 *
 * If 'ignore_constants' is true, any constants in the locus are ignored.
 */
bool
cdbpathlocus_is_hashed_on_eclasses(CdbPathLocus locus, List *eclasses, bool ignore_constants);

bool
cdbpathlocus_is_hashed_on_tlist(CdbPathLocus locus, List *tlist, bool ignore_constants);

/*
 * cdbpathlocus_is_hashed_on_relids
 *
 * Used when a subquery predicate (such as "x IN (SELECT ...)") has been
 * flattened into a join with the tables of the current query.  A row of
 * the cross-product of the current query's tables might join with more
 * than one row from the subquery and thus be duplicated.  Removing such
 * duplicates after the join is called deduping.  If a row's duplicates
 * might occur in more than one partition, a Motion operator will be
 * needed to bring them together.  This function tests whether the join
 * result (whose locus is given) can be deduped in place without motion.
 *
 * For a hashed locus, returns false if the distkey has a column whose
 * equivalence class contains no Var node belonging to the given set of
 * relids.  Caller should specify the relids of the non-subquery tables.
 */
bool
cdbpathlocus_is_hashed_on_relids(CdbPathLocus locus, Bitmapset *relids);

/*
 * cdbpathlocus_is_valid
 *
 * Returns true if locus appears structurally valid.
 */
bool
cdbpathlocus_is_valid(CdbPathLocus locus);

#endif   /* CDBPATHLOCUS_H */
