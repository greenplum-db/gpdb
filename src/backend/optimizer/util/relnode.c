/*-------------------------------------------------------------------------
 *
 * relnode.c
 *	  Relation-node lookup/construction routines
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/optimizer/util/relnode.c,v 1.91 2008/10/04 21:56:53 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "nodes/makefuncs.h"                /* makeVar() */
#include "catalog/gp_policy.h"
#include "optimizer/cost.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/plancat.h"
#include "optimizer/restrictinfo.h"
#include "optimizer/var.h"                  /* contain_vars_of_level_or_above */
#include "parser/parsetree.h"
#include "parser/parse_expr.h"              /* exprType(), exprTypmod() */
#include "utils/hsearch.h"
#include "utils/lsyscache.h"


typedef struct JoinHashEntry
{
	Relids		join_relids;	/* hash key --- MUST BE FIRST */
	RelOptInfo *join_rel;
} JoinHashEntry;

static List *build_joinrel_restrictlist(PlannerInfo *root,
						   RelOptInfo *joinrel,
						   RelOptInfo *outer_rel,
						   RelOptInfo *inner_rel);
static void build_joinrel_joinlist(RelOptInfo *joinrel,
					   RelOptInfo *outer_rel,
					   RelOptInfo *inner_rel);
static List *subbuild_joinrel_restrictlist(RelOptInfo *joinrel,
							  List *joininfo_list,
							  List *new_restrictlist);
static List *subbuild_joinrel_joinlist(RelOptInfo *joinrel,
						  List *joininfo_list,
						  List *new_joininfo);


/*
 * build_simple_rel
 *	  Construct a new RelOptInfo for a base relation or 'other' relation.
 */
RelOptInfo *
build_simple_rel(PlannerInfo *root, int relid, RelOptKind reloptkind)
{
	RelOptInfo *rel;
	RangeTblEntry *rte;

	/* Rel should not exist already */
	Assert(relid > 0 && relid < root->simple_rel_array_size);
	if (root->simple_rel_array[relid] != NULL)
		elog(ERROR, "rel %d already exists", relid);

	/* Fetch RTE for relation */
	rte = root->simple_rte_array[relid];
	Assert(rte != NULL);

    /* CDB: Rel isn't expected to have any pseudo columns yet. */
    Assert(!rte->pseudocols);

	rel = makeNode(RelOptInfo);
	rel->reloptkind = reloptkind;
	rel->relids = bms_make_singleton(relid);
	rel->rows = 0;
	rel->width = 0;
	rel->reltargetlist = NIL;
	rel->pathlist = NIL;
	rel->cheapest_startup_path = NULL;
	rel->cheapest_total_path = NULL;
    rel->dedup_info = NULL;
    rel->onerow = false;
	rel->relid = relid;
	rel->rtekind = rte->rtekind;
	/* min_attr, max_attr, attr_needed, attr_widths are set below */
	rel->indexlist = NIL;
	rel->pages = 0;
	rel->tuples = 0;
	rel->subplan = NULL;
	rel->subrtable = NIL;
	rel->urilocationlist = NIL;
	rel->execlocationlist = NIL;
	rel->execcommand = NULL;
	rel->fmttype = '\0';
	rel->fmtopts = NULL;
	rel->rejectlimit = -1;
	rel->rejectlimittype = '\0';
	rel->fmterrtbl = InvalidOid;
	rel->ext_encoding = -1;
	rel->writable = false;
	rel->baserestrictinfo = NIL;
	rel->baserestrictcost.startup = 0;
	rel->baserestrictcost.per_tuple = 0;
	rel->joininfo = NIL;
	rel->has_eclass_joins = false;
	rel->index_outer_relids = NULL;
	rel->index_inner_paths = NIL;

	/* Check type of rtable entry */
	switch (rte->rtekind)
	{
		case RTE_RELATION:
			/* Table --- retrieve statistics from the system catalogs */

			get_relation_info(root, rte->relid, rte->inh, rel);

			/* if we've been asked to, force the dist-policy to be partitioned-randomly. */
			if (rte->forceDistRandom)
			{
				rel->cdbpolicy = (GpPolicy *) palloc(sizeof(GpPolicy));
				rel->cdbpolicy->ptype = POLICYTYPE_PARTITIONED;
				rel->cdbpolicy->nattrs = 0;
				rel->cdbpolicy->attrs[0] = 1;

				/* Scribble the tuple number of rel to reflect the real size */
				rel->tuples = rel->tuples * planner_segment_count();
			}
			break;
		case RTE_SUBQUERY:
		case RTE_FUNCTION:
		case RTE_TABLEFUNCTION:
		case RTE_VALUES:
		case RTE_CTE:

			/*
			 * Subquery, function, or values list --- set up attr range and
			 * arrays
			 *
			 * Note: 0 is included in range to support whole-row Vars
			 */
            /* CDB: Allow internal use of sysattrs (<0) for subquery dedup. */
        	rel->min_attr = FirstLowInvalidHeapAttributeNumber + 1;     /*CDB*/
			rel->max_attr = list_length(rte->eref->colnames);
			rel->attr_needed = (Relids *)
				palloc0((rel->max_attr - rel->min_attr + 1) * sizeof(Relids));
			rel->attr_widths = (int32 *)
				palloc0((rel->max_attr - rel->min_attr + 1) * sizeof(int32));
			break;
		default:
			elog(ERROR, "unrecognized RTE kind: %d",
				 (int) rte->rtekind);
			break;
	}

	/* Save the finished struct in the query's simple_rel_array */
	root->simple_rel_array[relid] = rel;

	/*
	 * If this rel is an appendrel parent, recurse to build "other rel"
	 * RelOptInfos for its children.  They are "other rels" because they are
	 * not in the main join tree, but we will need RelOptInfos to plan access
	 * to them.
	 */
	if (rte->inh)
	{
		ListCell   *l;

		foreach(l, root->append_rel_list)
		{
			AppendRelInfo *appinfo = (AppendRelInfo *) lfirst(l);

			/* append_rel_list contains all append rels; ignore others */
			if (appinfo->parent_relid != relid)
				continue;

			(void) build_simple_rel(root, appinfo->child_relid,
									RELOPT_OTHER_MEMBER_REL);
		}
	}

	return rel;
}

/*
 * find_base_rel
 *	  Find a base or other relation entry, which must already exist.
 */
RelOptInfo *
find_base_rel(PlannerInfo *root, int relid)
{
	RelOptInfo *rel;

	Assert(relid > 0);

	if (relid < root->simple_rel_array_size)
	{
		rel = root->simple_rel_array[relid];
		if (rel)
			return rel;
	}

	elog(ERROR, "no relation entry for relid %d", relid);

	return NULL;				/* keep compiler quiet */
}

/*
 * build_join_rel_hash
 *	  Construct the auxiliary hash table for join relations.
 */
static void
build_join_rel_hash(PlannerInfo *root)
{
	HTAB	   *hashtab;
	HASHCTL		hash_ctl;
	ListCell   *l;

	/* Create the hash table */
	MemSet(&hash_ctl, 0, sizeof(hash_ctl));
	hash_ctl.keysize = sizeof(Relids);
	hash_ctl.entrysize = sizeof(JoinHashEntry);
	hash_ctl.hash = bitmap_hash;
	hash_ctl.match = bitmap_match;
	hash_ctl.hcxt = CurrentMemoryContext;
	hashtab = hash_create("JoinRelHashTable",
						  256L,
						  &hash_ctl,
					HASH_ELEM | HASH_FUNCTION | HASH_COMPARE | HASH_CONTEXT);

	/* Insert all the already-existing joinrels */
	foreach(l, root->join_rel_list)
	{
		RelOptInfo *rel = (RelOptInfo *) lfirst(l);
		JoinHashEntry *hentry;
		bool		found;

		hentry = (JoinHashEntry *) hash_search(hashtab,
											   &(rel->relids),
											   HASH_ENTER,
											   &found);
		Assert(!found);
		hentry->join_rel = rel;
	}

	root->join_rel_hash = hashtab;
}

/*
 * find_join_rel
 *	  Returns relation entry corresponding to 'relids' (a set of RT indexes),
 *	  or NULL if none exists.  This is for join relations.
 */
RelOptInfo *
find_join_rel(PlannerInfo *root, Relids relids)
{
	/*
	 * Switch to using hash lookup when list grows "too long".	The threshold
	 * is arbitrary and is known only here.
	 */
	if (!root->join_rel_hash && list_length(root->join_rel_list) > 32)
		build_join_rel_hash(root);

	/*
	 * Use either hashtable lookup or linear search, as appropriate.
	 *
	 * Note: the seemingly redundant hashkey variable is used to avoid taking
	 * the address of relids; unless the compiler is exceedingly smart, doing
	 * so would force relids out of a register and thus probably slow down the
	 * list-search case.
	 */
	if (root->join_rel_hash)
	{
		Relids		hashkey = relids;
		JoinHashEntry *hentry;

		hentry = (JoinHashEntry *) hash_search(root->join_rel_hash,
											   &hashkey,
											   HASH_FIND,
											   NULL);
		if (hentry)
			return hentry->join_rel;
	}
	else
	{
		ListCell   *l;

		foreach(l, root->join_rel_list)
		{
			RelOptInfo *rel = (RelOptInfo *) lfirst(l);

			if (bms_equal(rel->relids, relids))
				return rel;
		}
	}

	return NULL;
}

/*
 * build_join_rel
 *	  Returns relation entry corresponding to the union of two given rels,
 *	  creating a new relation entry if none already exists.
 *
 * 'joinrelids' is the Relids set that uniquely identifies the join
 * 'outer_rel' and 'inner_rel' are relation nodes for the relations to be
 *		joined
 * 'sjinfo': join context info
 * 'restrictlist_ptr': result variable.  If not NULL, *restrictlist_ptr
 *		receives the list of RestrictInfo nodes that apply to this
 *		particular pair of joinable relations.
 *
 * restrictlist_ptr makes the routine's API a little grotty, but it saves
 * duplicated calculation of the restrictlist...
 */
RelOptInfo *
build_join_rel(PlannerInfo *root,
			   Relids joinrelids,
			   RelOptInfo *outer_rel,
			   RelOptInfo *inner_rel,
			   SpecialJoinInfo *sjinfo,
			   List **restrictlist_ptr)
{
	RelOptInfo *joinrel;
	List	   *restrictlist;

	/*
	 * See if we already have a joinrel for this set of base rels.
	 */
	joinrel = find_join_rel(root, joinrelids);

	if (joinrel)
	{
		/*
		 * Yes, so we only need to figure the restrictlist for this particular
		 * pair of component relations.
		 */
		if (restrictlist_ptr)
			*restrictlist_ptr = build_joinrel_restrictlist(root,
														   joinrel,
														   outer_rel,
														   inner_rel);

        /* CDB: Join between single-row inputs produces a single-row joinrel. */
        Assert(joinrel->onerow == (outer_rel->onerow && inner_rel->onerow));

		return joinrel;
	}

	/*
	 * Nope, so make one.
	 */
	joinrel = makeNode(RelOptInfo);
	joinrel->reloptkind = RELOPT_JOINREL;
	joinrel->relids = bms_copy(joinrelids);
	joinrel->rows = 0;
	joinrel->width = 0;
	joinrel->reltargetlist = NIL;
	joinrel->pathlist = NIL;
	joinrel->cheapest_startup_path = NULL;
	joinrel->cheapest_total_path = NULL;
    joinrel->dedup_info = NULL;
    joinrel->onerow = false;
	joinrel->relid = 0;			/* indicates not a baserel */
	joinrel->rtekind = RTE_JOIN;
	joinrel->min_attr = 0;
	joinrel->max_attr = 0;
	joinrel->attr_needed = NULL;
	joinrel->attr_widths = NULL;
	joinrel->indexlist = NIL;
	joinrel->pages = 0;
	joinrel->tuples = 0;
	joinrel->subplan = NULL;
	joinrel->subrtable = NIL;
	joinrel->baserestrictinfo = NIL;
	joinrel->baserestrictcost.startup = 0;
	joinrel->baserestrictcost.per_tuple = 0;
	joinrel->joininfo = NIL;
	joinrel->has_eclass_joins = false;
	joinrel->index_outer_relids = NULL;
	joinrel->index_inner_paths = NIL;

	/* CDB: Join between single-row inputs produces a single-row joinrel. */
	if (outer_rel->onerow && inner_rel->onerow)
		joinrel->onerow = true;

	/*
	 * Create a new tlist containing just the vars that need to be output from
	 * this join (ie, are needed for higher joinclauses or final output).
	 *
	 * NOTE: the tlist order for a join rel will depend on which pair of outer
	 * and inner rels we first try to build it from.  But the contents should
	 * be the same regardless.
	 */
	build_joinrel_tlist(root, joinrel, outer_rel->reltargetlist);
	build_joinrel_tlist(root, joinrel, inner_rel->reltargetlist);

	/* cap width of output row by sum of its inputs */
	joinrel->width = Min(joinrel->width, outer_rel->width + inner_rel->width);

	/*
	 * Construct restrict and join clause lists for the new joinrel. (The
	 * caller might or might not need the restrictlist, but I need it anyway
	 * for set_joinrel_size_estimates().)
	 */
	restrictlist = build_joinrel_restrictlist(root, joinrel,
											  outer_rel, inner_rel);
	if (restrictlist_ptr)
		*restrictlist_ptr = restrictlist;
	build_joinrel_joinlist(joinrel, outer_rel, inner_rel);

	/*
	 * CDB: Attach subquery duplicate suppression info if needed.
	 */
	if (root->join_info_list)
		joinrel->dedup_info = cdb_make_rel_dedup_info(root, joinrel);

	/*
	 * This is also the right place to check whether the joinrel has any
	 * pending EquivalenceClass joins.
	 */
	joinrel->has_eclass_joins = has_relevant_eclass_joinclause(root, joinrel);

	/*
	 * Set estimates of the joinrel's size.
	 */
	set_joinrel_size_estimates(root, joinrel, outer_rel, inner_rel,
							   sjinfo, restrictlist);

	/*
	 * Add the joinrel to the query's joinrel list, and store it into the
	 * auxiliary hashtable if there is one.  NB: GEQO requires us to append
	 * the new joinrel to the end of the list!
	 */
	root->join_rel_list = lappend(root->join_rel_list, joinrel);

	if (root->join_rel_hash)
	{
		JoinHashEntry *hentry;
		bool		found;

		hentry = (JoinHashEntry *) hash_search(root->join_rel_hash,
											   &(joinrel->relids),
											   HASH_ENTER,
											   &found);
		Assert(!found);
		hentry->join_rel = joinrel;
	}

	return joinrel;
}

/*
 * build_joinrel_tlist
 *	  Builds a join relation's target list.
 *
 * The join's targetlist includes all Vars of its member relations that
 * will still be needed above the join.  This subroutine adds all such
 * Vars from the specified input rel's tlist to the join rel's tlist.
 *
 * We also compute the expected width of the join's output, making use
 * of data that was cached at the baserel level by set_rel_width().
 */
void
build_joinrel_tlist(PlannerInfo *root, RelOptInfo *joinrel,
					List *input_tlist)
{
	Relids		relids = joinrel->relids;
	ListCell   *vars;

	foreach(vars, input_tlist)
	{
		Var		   *origvar = (Var *) lfirst(vars);
		Var		   *var;
		RelOptInfo *baserel;
		int			ndx;

		/*
		 * We can't run into any child RowExprs here, but we could find a
		 * whole-row Var with a ConvertRowtypeExpr atop it.
		 */
		var = origvar;
		while (!IsA(var, Var))
		{
			if (IsA(var, ConvertRowtypeExpr))
				var = (Var *) ((ConvertRowtypeExpr *) var)->arg;
			else
				elog(ERROR, "unexpected node type in reltargetlist: %d",
					 (int) nodeTag(var));
		}

        /* Pseudo column? */
        if (var->varattno <= FirstLowInvalidHeapAttributeNumber)
        {
            CdbRelColumnInfo   *rci = cdb_find_pseudo_column(root, var);

		    if (bms_nonempty_difference(rci->where_needed, relids))
		    {
			    joinrel->reltargetlist = lappend(joinrel->reltargetlist, origvar);
			    joinrel->width += rci->attr_width;
		    }
            continue;
        }

		/* Get the Var's original base rel */
		baserel = find_base_rel(root, var->varno);

        /* System-defined attribute, whole row, or user-defined attribute */
        Assert(var->varattno >= baserel->min_attr &&
               var->varattno <= baserel->max_attr);

		/* Is it still needed above this joinrel? */
		ndx = var->varattno - baserel->min_attr;
		if (bms_nonempty_difference(baserel->attr_needed[ndx], relids))
		{
			/* Yup, add it to the output */
			joinrel->reltargetlist = lappend(joinrel->reltargetlist, origvar);
			joinrel->width += baserel->attr_widths[ndx];
		}
	}
}

/*
 * build_joinrel_restrictlist
 * build_joinrel_joinlist
 *	  These routines build lists of restriction and join clauses for a
 *	  join relation from the joininfo lists of the relations it joins.
 *
 *	  These routines are separate because the restriction list must be
 *	  built afresh for each pair of input sub-relations we consider, whereas
 *	  the join list need only be computed once for any join RelOptInfo.
 *	  The join list is fully determined by the set of rels making up the
 *	  joinrel, so we should get the same results (up to ordering) from any
 *	  candidate pair of sub-relations.	But the restriction list is whatever
 *	  is not handled in the sub-relations, so it depends on which
 *	  sub-relations are considered.
 *
 *	  If a join clause from an input relation refers to base rels still not
 *	  present in the joinrel, then it is still a join clause for the joinrel;
 *	  we put it into the joininfo list for the joinrel.  Otherwise,
 *	  the clause is now a restrict clause for the joined relation, and we
 *	  return it to the caller of build_joinrel_restrictlist() to be stored in
 *	  join paths made from this pair of sub-relations.	(It will not need to
 *	  be considered further up the join tree.)
 *
 *	  In many case we will find the same RestrictInfos in both input
 *	  relations' joinlists, so be careful to eliminate duplicates.
 *	  Pointer equality should be a sufficient test for dups, since all
 *	  the various joinlist entries ultimately refer to RestrictInfos
 *	  pushed into them by distribute_restrictinfo_to_rels().
 *
 * 'joinrel' is a join relation node
 * 'outer_rel' and 'inner_rel' are a pair of relations that can be joined
 *		to form joinrel.
 *
 * build_joinrel_restrictlist() returns a list of relevant restrictinfos,
 * whereas build_joinrel_joinlist() stores its results in the joinrel's
 * joininfo list.  One or the other must accept each given clause!
 *
 * NB: Formerly, we made deep(!) copies of each input RestrictInfo to pass
 * up to the join relation.  I believe this is no longer necessary, because
 * RestrictInfo nodes are no longer context-dependent.	Instead, just include
 * the original nodes in the lists made for the join relation.
 */
static List *
build_joinrel_restrictlist(PlannerInfo *root,
						   RelOptInfo *joinrel,
						   RelOptInfo *outer_rel,
						   RelOptInfo *inner_rel)
{
	List	   *result;

	/*
	 * Collect all the clauses that syntactically belong at this level,
	 * eliminating any duplicates (important since we will see many of the
	 * same clauses arriving from both input relations).
	 */
	result = subbuild_joinrel_restrictlist(joinrel, outer_rel->joininfo, NIL);
	result = subbuild_joinrel_restrictlist(joinrel, inner_rel->joininfo, result);

	/*
	 * Add on any clauses derived from EquivalenceClasses.	These cannot be
	 * redundant with the clauses in the joininfo lists, so don't bother
	 * checking.
	 */
	result = list_concat(result,
						 generate_join_implied_equalities(root,
														  joinrel,
														  outer_rel,
														  inner_rel));

	return result;
}

static void
build_joinrel_joinlist(RelOptInfo *joinrel,
					   RelOptInfo *outer_rel,
					   RelOptInfo *inner_rel)
{
	List	   *result;

	/*
	 * Collect all the clauses that syntactically belong above this level,
	 * eliminating any duplicates (important since we will see many of the
	 * same clauses arriving from both input relations).
	 */
	result = subbuild_joinrel_joinlist(joinrel, outer_rel->joininfo, NIL);
	result = subbuild_joinrel_joinlist(joinrel, inner_rel->joininfo, result);

	joinrel->joininfo = result;
}

static List *
subbuild_joinrel_restrictlist(RelOptInfo *joinrel,
							  List *joininfo_list,
							  List *new_restrictlist)
{
	ListCell   *l;

	foreach(l, joininfo_list)
	{
		RestrictInfo *rinfo = (RestrictInfo *) lfirst(l);

		if (bms_is_subset(rinfo->required_relids, joinrel->relids))
		{
			/*
			 * This clause becomes a restriction clause for the joinrel, since
			 * it refers to no outside rels.  Add it to the list, being
			 * careful to eliminate duplicates. (Since RestrictInfo nodes in
			 * different joinlists will have been multiply-linked rather than
			 * copied, pointer equality should be a sufficient test.)
			 */
			new_restrictlist = list_append_unique_ptr(new_restrictlist, rinfo);
		}
		else
		{
			/*
			 * This clause is still a join clause at this level, so we ignore
			 * it in this routine.
			 */
		}
	}

	return new_restrictlist;
}

static List *
subbuild_joinrel_joinlist(RelOptInfo *joinrel,
						  List *joininfo_list,
						  List *new_joininfo)
{
	ListCell   *l;

	foreach(l, joininfo_list)
	{
		RestrictInfo *rinfo = (RestrictInfo *) lfirst(l);

		if (bms_is_subset(rinfo->required_relids, joinrel->relids))
		{
			/*
			 * This clause becomes a restriction clause for the joinrel, since
			 * it refers to no outside rels.  So we can ignore it in this
			 * routine.
			 */
		}
		else
		{
			/*
			 * This clause is still a join clause at this level, so add it to
			 * the new joininfo list, being careful to eliminate duplicates.
			 * (Since RestrictInfo nodes in different joinlists will have been
			 * multiply-linked rather than copied, pointer equality should be
			 * a sufficient test.)
			 */
			new_joininfo = list_append_unique_ptr(new_joininfo, rinfo);
		}
	}

	return new_joininfo;
}

/*
 * cdb_define_pseudo_column
 *
 * Add a pseudo column definition to a baserel or appendrel.  Returns
 * a Var node referencing the new column.
 *
 * This function does not add the new Var node to the targetlist.  The
 * caller should do that, if needed, by calling add_vars_to_targetlist().
 *
 * A pseudo column is defined by an expression which is to be evaluated
 * in targetlist and/or qual expressions of the baserel's scan operator in
 * the Plan tree.
 *
 * A pseudo column is referenced by means of Var nodes in which varno = relid
 * and varattno = FirstLowInvalidHeapAttributeNumber minus the 0-based position
 * of the CdbRelColumnInfo node in the rte->pseudocols list.
 *
 * The special Var nodes will later be eliminated during set_plan_references().
 * Those in the scan or append operator's targetlist and quals will be replaced
 * by copies of the defining expression.  Those further downstream will be
 * replaced by ordinary Var nodes referencing the appropriate targetlist item.
 *
 * A pseudo column defined in an appendrel is merely a placeholder for a
 * column produced by the subpaths, allowing the column to be referenced
 * by downstream nodes.  Its defining expression is never evaluated because
 * the Append targetlist is not executed.  It is the caller's responsibility
 * to make corresponding changes to the targetlists of the appendrel and its
 * subpaths so that they all match.
 *
 * Note that a joinrel can't define a pseudo column because, lacking a
 * relid, there's no way for a Var node to reference such a column.
 */
Var *
cdb_define_pseudo_column(PlannerInfo   *root,
                         RelOptInfo    *rel,
                         const char    *colname,
                         Expr          *defexpr,
                         int32          width)
{
    CdbRelColumnInfo   *rci = makeNode(CdbRelColumnInfo);
    RangeTblEntry      *rte = rt_fetch(rel->relid, root->parse->rtable);
    ListCell           *cell;
    Var                *var;
    int                 i;

    Assert(colname && strlen(colname) < sizeof(rci->colname)-10);
    Assert(rel->reloptkind == RELOPT_BASEREL ||
           rel->reloptkind == RELOPT_OTHER_MEMBER_REL);

    rci->defexpr = defexpr;
    rci->where_needed = NULL;

    /* Assign attribute number. */
    rci->pseudoattno = FirstLowInvalidHeapAttributeNumber - list_length(rte->pseudocols);

    /* Make a Var node which upper nodes can copy to reference the column. */
    var = makeVar(rel->relid, rci->pseudoattno,
                  exprType((Node *)defexpr), exprTypmod((Node *)defexpr),
                  0);

    /* Note the estimated number of bytes for a value of this type. */
    if (width < 0)
		width = get_typavgwidth(var->vartype, var->vartypmod);
    rci->attr_width = width;

    /* If colname isn't unique, add suffix "_2", "_3", etc. */
    StrNCpy(rci->colname, colname, sizeof(rci->colname));
    for (i = 1;;)
    {
        CdbRelColumnInfo   *rci2;
        Value              *val;

        /* Same as the name of a regular column? */
        foreach(cell, rte->eref ? rte->eref->colnames : NULL)
        {
            val = (Value *)lfirst(cell);
            Assert(IsA(val, String));
            if (0 == strcmp(strVal(val), rci->colname))
                break;
        }

        /* Same as the name of an already defined pseudo column? */
        if (!cell)
        {
            foreach(cell, rte->pseudocols)
            {
                rci2 = (CdbRelColumnInfo *)lfirst(cell);
                Assert(IsA(rci2, CdbRelColumnInfo));
                if (0 == strcmp(rci2->colname, rci->colname))
                    break;
            }
        }

        if (!cell)
            break;
        Insist(i <= list_length(rte->eref->colnames) + list_length(rte->pseudocols));
        snprintf(rci->colname, sizeof(rci->colname), "%s_%d", colname, ++i);
    }

    /* Add to the RTE's pseudo column list. */
    rte->pseudocols = lappend(rte->pseudocols, rci);

    return var;
}                               /* cdb_define_pseudo_column */


/*
 * cdb_find_pseudo_column
 *
 * Return the CdbRelColumnInfo node which defines a pseudo column.
 */
CdbRelColumnInfo *
cdb_find_pseudo_column(PlannerInfo *root, Var *var)
{
    CdbRelColumnInfo   *rci;
    RangeTblEntry      *rte;
    const char         *rtename;

    Assert(IsA(var, Var) &&
           var->varno > 0 &&
           var->varno <= list_length(root->parse->rtable));

    rte = rt_fetch(var->varno, root->parse->rtable);
    rci = cdb_rte_find_pseudo_column(rte, var->varattno);
    if (!rci)
    {
        rtename = (rte->eref && rte->eref->aliasname) ? rte->eref->aliasname
                                                      : "*BOGUS*";
        ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
                        errmsg_internal("invalid varattno %d for rangetable entry %s",
                                        var->varattno, rtename) ));
    }
    return rci;
}                               /* cdb_find_pseudo_column */


/*
 * cdb_rte_find_pseudo_column
 *
 * Return the CdbRelColumnInfo node which defines a pseudo column; or
 * NULL if didn't find a pseudo column with the given attno.
 */
CdbRelColumnInfo *
cdb_rte_find_pseudo_column(RangeTblEntry *rte, AttrNumber attno)
{
    int                 ndx = FirstLowInvalidHeapAttributeNumber - attno;
    CdbRelColumnInfo   *rci;

    Assert(IsA(rte, RangeTblEntry));

    if (attno > FirstLowInvalidHeapAttributeNumber ||
        ndx >= list_length(rte->pseudocols))
        return NULL;

    rci = (CdbRelColumnInfo *)list_nth(rte->pseudocols, ndx);

    Assert(IsA(rci, CdbRelColumnInfo));
    Insist(rci->pseudoattno == attno);
    return rci;
}                               /* cdb_rte_find_pseudo_column */


/*
 * cdb_make_rel_dedup_info
 *
 * When a subquery from a search condition has been flattened into a join,
 * the join may need some special mojo.  A row of the main query must not
 * join with more than one row of the subquery, or in case it does, the
 * consequent multiple result rows must be collapsed to a single row
 * afterwards.
 *
 * This function creates and returns a CdbRelDedupInfo structure for
 * planning subquery duplicate suppression.  Returns NULL if the rel
 * doesn't need a CdbRelOptInfo at this time.
 *
 * The reltargetlist should be complete before calling this function.
 */
CdbRelDedupInfo *
cdb_make_rel_dedup_info(PlannerInfo *root, RelOptInfo *rel)
{
    CdbRelDedupInfo    *dedup;
    ListCell           *cell;
    Relids              prejoin_dedup_subqrelids;
    Relids              spent_subqrelids;
    SpecialJoinInfo     *join_unique_ininfo;
    bool                partial;
    bool                try_postjoin_dedup;
    int                 subqueries_unfinished;

    /* Return NULL if rel has no tables from flattened subqueries. */
    if (bms_is_subset(rel->relids, root->currlevel_relids))
        return NULL;

    /*
     * Does rel include any subquery tables which are not referenced
     * downstream?
     *
     * When the columns of the inner rel of a join are not needed by
     * downstream operators, and all tables of the inner rel come from
     * flattened subqueries, then the JOIN_SEMI jointype can be used,
     * telling the executor to produce only the first matching inner row
     * for each outer row.
     */
    spent_subqrelids = bms_difference(rel->relids, root->currlevel_relids);
    foreach(cell, rel->reltargetlist)
    {
        Var    *var = (Var *)lfirst(cell);

        Assert(IsA(var, Var) && var->varlevelsup == 0);
        spent_subqrelids = bms_del_member(spent_subqrelids, var->varno);
    }
    if (bms_is_empty(spent_subqrelids))
    {
        bms_free(spent_subqrelids);
        spent_subqrelids = NULL;
    }

    /*
     * Determine set of flattened subqueries whose inputs are all included in
     * this rel.
     *
     * (A subquery can be identified by its set of relids: the righthand relids
     * in its SpecialJoinInfo.)
     *
     * Post-join duplicate removal can be applied to a rel that contains the
     * sublink's lefthand relids, the subquery's own tables (the sublink's
     * righthand relids), and the relids of outer references.  For subqueries
     * in search conditions, it is sufficient to test whether the sublink's
     * righthand relids are a subset of spent_subqrelids.
     */
    prejoin_dedup_subqrelids = NULL;
    join_unique_ininfo = NULL;
    partial = false;
    try_postjoin_dedup = false;
    subqueries_unfinished = list_length(root->join_info_list);
    foreach(cell, root->join_info_list)
    {
        SpecialJoinInfo   *sjinfo = (SpecialJoinInfo *)lfirst(cell);
        if(!sjinfo->consider_dedup)
            continue;

        /* Got all of the subquery's own tables? */
        if (bms_is_subset(sjinfo->syn_righthand, rel->relids))
        {
            /* Early dedup (JOIN_UNIQUE, JOIN_SEMI) can be applied to this rel. */
            prejoin_dedup_subqrelids =
                bms_add_members(prejoin_dedup_subqrelids, sjinfo->syn_righthand);

            /* Got all the correlating and left-hand relids too? */
            if (bms_is_subset(sjinfo->syn_righthand, spent_subqrelids))
            {
                try_postjoin_dedup = true;
                subqueries_unfinished--;
            }
            else
                partial = true;

            /* Does rel have exactly the relids of uncorrelated "= ANY" subq? */
            if (sjinfo->try_join_unique &&
                bms_equal(sjinfo->syn_righthand, rel->relids))
                join_unique_ininfo = sjinfo;
        }
        else if (bms_overlap(sjinfo->syn_righthand, rel->relids))
            partial = true;
    }

    /* Exit if didn't find anything interesting. */
    if (!spent_subqrelids &&
        !prejoin_dedup_subqrelids)
        return NULL;

    /*
     * A heuristic to avoid placing more than one Unique op in series.
     *
     * If rel includes some but not all of a sublink's required inputs,
     * that subquery will become eligible for late (post-join) dedup at a
     * later stage, after the missing tables are joined.  There may also be
     * another sublink whose required inputs are all present; in which case
     * we refrain from considering late dedup until both sublinks have been
     * fully evaluated and can be deduped together.
     *
     * (Also, this allows cdp_is_path_deduped() to assume that a Unique op will
     * dedup all of the sublinks whose righthand relids are covered by the rel.)
     */
    if (partial)
        try_postjoin_dedup = false;

    /*
     * Create CdbRelDedupInfo.
     */
    dedup = makeNode(CdbRelDedupInfo);

    dedup->prejoin_dedup_subqrelids = prejoin_dedup_subqrelids;
    dedup->spent_subqrelids = spent_subqrelids;
    dedup->try_postjoin_dedup = try_postjoin_dedup;
    dedup->no_more_subqueries = (subqueries_unfinished == 0);
    dedup->join_unique_ininfo = join_unique_ininfo;
    dedup->later_dedup_pathlist = NIL;
    dedup->cheapest_startup_path = NULL;
    dedup->cheapest_total_path = NULL;

    return dedup;
}                               /* cdb_make_rel_dedup_info */

