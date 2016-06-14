/*-------------------------------------------------------------------------
 *
 * pathkeys.c
 *	  Utilities for matching and building path keys
 *
 * See src/backend/optimizer/README for a great deal of information about
 * the nature and use of path keys.
 *
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/optimizer/path/pathkeys.c,v 1.83 2007/01/21 00:57:15 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/skey.h"
#include "nodes/makefuncs.h"
#include "nodes/plannodes.h"
#include "optimizer/clauses.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/planmain.h"
#include "optimizer/tlist.h"
#include "optimizer/var.h"
#include "optimizer/restrictinfo.h"
#include "parser/parsetree.h"
#include "parser/parse_expr.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h" /* for compatible_oper_opid() */
#include "utils/lsyscache.h"

#include "cdb/cdbdef.h"         /* CdbSwap() */
#include "cdb/cdbpullup.h"      /* cdbpullup_expr(), cdbpullup_make_var() */

/*
 * If an EC contains a const and isn't below-outer-join, any PathKey depending
 * on it must be redundant, since there's only one possible value of the key.
 */
#define MUST_BE_REDUNDANT(eclass)  \
	((eclass)->ec_has_const && !(eclass)->ec_below_outer_join)

static PathKey *make_canonical_pathkey(PlannerInfo *root,
					   EquivalenceClass *eclass, Oid opfamily,
					   int strategy, bool nulls_first);
static bool pathkey_is_redundant(PathKey *new_pathkey, List *pathkeys);
static PathKey *make_pathkey_from_sortinfo(PlannerInfo *root,
						   Expr *expr, Oid ordering_op,
						   bool nulls_first,
						   bool canonicalize);


/****************************************************************************
 *		PATHKEY CONSTRUCTION AND REDUNDANCY TESTING
 ****************************************************************************/

/*
 * makePathKey
 *		create a PathKey node
 *
 * This does not promise to create a canonical PathKey, it's merely a
 * convenience routine to build the specified node.
 */
PathKey *
makePathKey(EquivalenceClass *eclass, Oid opfamily,
			int strategy, bool nulls_first)
{
	PathKey	   *pk = makeNode(PathKey);

	pk->pk_eclass = eclass;
	pk->pk_opfamily = opfamily;
	pk->pk_strategy = strategy;
	pk->pk_nulls_first = nulls_first;

	return pk;
}

/**
 * replace_expression_mutator
 *
 * Copy an expression tree, but replace all occurrences of one node with
 *   another.
 *
 * The replacement is passed in the context as a pointer to
 *    ReplaceExpressionMutatorReplacement
 *
 * context should be ReplaceExpressionMutatorReplacement*
 */
Node *
replace_expression_mutator(Node *node, void *context)
{
    ReplaceExpressionMutatorReplacement * repl;
    if (node == NULL)
        return NULL;

    if ( IsA(node, RestrictInfo))
    {
        RestrictInfo *info = (RestrictInfo *) node;
        return replace_expression_mutator((Node*) info->clause, context);
    }

    repl = (ReplaceExpressionMutatorReplacement*) context;
    if ( equal(node, repl->replaceThis))
    {
        repl->numReplacementsDone++;
        return copyObject(repl->withThis);
    }
    return expression_tree_mutator(node, replace_expression_mutator, (void *) context);
}

/**
 * Do relations in qualscope fall on the nullable side of an outer join?
 */
static bool
isBelowNullableSideOfOuterJoin( PlannerInfo *root, Relids qualscope)
{
    ListCell *curOuterJoinInfo;

    foreach(curOuterJoinInfo, root->oj_info_list)
    {
        OuterJoinInfo * oj = (OuterJoinInfo *) lfirst(curOuterJoinInfo);
        if (bms_overlap(qualscope, oj->syn_righthand))
            return true;

        if (oj->join_type == JOIN_FULL &&
            bms_overlap(qualscope, oj->syn_lefthand))
            return true;
    }

    return false;
}

/**
 * Iterate on the given list of restrict infos, and build a new list that contains
 * only the elements that have outerjoin_delayed= false
 */
static List *remove_outer_join_restrict_infos(List *restrictinfolist)
{
	List *newlist = NIL;

	ListCell *lc = NULL;
	foreach (lc, restrictinfolist)
	{
		RestrictInfo *rinfo = (RestrictInfo *) lfirst(lc);
		if (!rinfo->outerjoin_delayed)
		{
			newlist = lappend(newlist, rinfo);
		}
	}

	return newlist;
}

/**
 * Given an equivalence class, extract out all the relevant known clauses.
 * This is done by finding all relations represented in the equivalence class.
 * Then, we extract out all the restrictinfos from these relations and find
 * the additional relations mentioned in these clauses. These restrictinfo
 * clauses are appended into one big list
 * Input:
 * 	root - planner information
 * 	eclass - equivalence class
 */
static List *relevant_known_clauses(PlannerInfo *root, EquivalenceClass *eclass)
{
	/**
	 * Find all the relevant relids from pathkey list and corresponding restrict
	 * infos.
	 */
	Relids relevant_relids;

	/**
	 * Find the restrictinfos for every relation that has an equivalence member
	 * and then extract out these relids as well
	 */
	int relid;
	Relids iter;

	relevant_relids = bms_copy(eclass->ec_relids);

	iter = bms_copy(eclass->ec_relids);
	while ((relid = bms_first_member(iter)) >= 0)
	{
		RelOptInfo *rel1 = find_base_rel(root, relid);

		List *restrictinfolist = list_union(rel1->baserestrictinfo, remove_outer_join_restrict_infos(rel1->joininfo));
		List *restrictlistclauses = list_union(
				extract_actual_clauses(restrictinfolist, true),
				extract_actual_clauses(restrictinfolist, false)
		);

		relevant_relids = bms_union(relevant_relids, pull_varnos((Node *) restrictlistclauses));
	}
	bms_free(iter);

	/**
	 * Build a list of clauses by iterating over all relevant relations
	 */
	List *relevant_clauses = NIL;
	while ((relid = bms_first_member(relevant_relids)) >= 0)
	{
		RelOptInfo *rel1 = find_base_rel(root, relid);

		List *restrictinfolist = list_union(rel1->baserestrictinfo, remove_outer_join_restrict_infos(rel1->joininfo));
		List *restrictlistclauses = list_union(
				extract_actual_clauses(restrictinfolist, true),
				extract_actual_clauses(restrictinfolist, false)
		);
		relevant_clauses = list_concat(relevant_clauses, restrictlistclauses);
	}
	bms_free(relevant_relids);

	return relevant_clauses;
}

/**
 * Generate implied qual
 * Input:
 * 	root - planner information
 * 	relevant_clauses - all previously known clauses
 * 	old_clause - old clause to infer from
 * 	old_pk - the pathkey item to be replaced
 * 	new_pk - new pathkey item replacing it
 * Output:
 *  New list of relevant clauses
 */
static List *gen_implied_qual(PlannerInfo *root,
		List *relevant_clauses, /* This list may be modified */
		Node *old_clause,
		Node *old_pk,
		Node *new_pk
		)
{

	/* Expression types must match */
	Assert(exprType(old_pk) == exprType(new_pk)
			&& exprTypmod(old_pk) == exprTypmod(new_pk));

	/* clone the clause, replacing first node with
	 *    clone of second
	 */
	ReplaceExpressionMutatorReplacement ctx;
	ctx.replaceThis = old_pk;
	ctx.withThis = new_pk;
	ctx.numReplacementsDone = 0;
	Node *new_clause = (Node*) replace_expression_mutator(
			(Node*)old_clause, &ctx);

	Relids old_qualscope = pull_varnos(old_clause);
	Relids new_qualscope = pull_varnos(new_clause);

	bool inferrable = new_qualscope != NULL /* distribute_qual_to_rels doesn't accept pseudoconstants */
			&& (ctx.numReplacementsDone > 0)
			&& !list_member(relevant_clauses, new_clause)
			&& !subexpression_match((Expr *) new_pk, (Expr *) old_clause);

	/**
	 * No inferences may be performed across an outer join
	 */
	inferrable = inferrable
			&& !isBelowNullableSideOfOuterJoin(root, old_qualscope)
			&& !isBelowNullableSideOfOuterJoin(root, new_qualscope);

	if (inferrable)
	{
		distribute_qual_to_rels(root, new_clause,
				true, /* is_deduced */
				true, /* is_deduced_but_not_equijoin */
				false,
				new_qualscope, /* qualscope */
				NULL, /* ojscope */
				NULL, /* outerjoin_nonnullable */
				NULL /* postponed_qual_list */
		);
		relevant_clauses = lappend(relevant_clauses, new_clause);
	}

	return relevant_clauses;
}

/**
 * Generate all qualifications that are implied by the equivalence specified
 *   by the given pathkey list
 * Input:
 * - root: planner info structure
 * - lpkitems: list of equivalent pathkey items
 */
static void
gen_implied_quals_for_eclass(PlannerInfo *root, EquivalenceClass *eclass)
{
	List *relevant_clauses = relevant_known_clauses(root, eclass);

	/*
	 * For every triple (em1, clause, em2), we try to replace em1 in clause
	 * with em2 and add it as an inferred clause since em1 = em2
	 */
    ListCell     *lcem1;
    foreach(lcem1, eclass->ec_members)
    {
		EquivalenceMember *em1 = (EquivalenceMember *) lfirst(lcem1);

		/*
		 * Only look at equivalence members that correspond to a single relation
		 */
		if (bms_membership(em1->em_relids) == BMS_SINGLETON)
    	{
			/*
			 * Iterate over all clauses
			 */
			ListCell   *lcclause;
			foreach(lcclause, relevant_clauses)
    		{
    			Node *old_clause = (Node *) lfirst(lcclause);

				/*
				 * Is it safe to infer from old_clause?
				 */
				bool safe_to_infer =
					!contain_volatile_functions(old_clause)
					&& !contain_subplans(old_clause);

				if (safe_to_infer)
				{
					ListCell *lcem2;

					/* now try to apply to others in the equivalence class */
					foreach(lcem2, eclass->ec_members)
    				{
						EquivalenceMember *em2 = (EquivalenceMember *) lfirst(lcem2);

						if (exprType((Node *) em1->em_expr) == exprType((Node *) em2->em_expr)
							&& exprTypmod((Node *)em1->em_expr) == exprTypmod((Node *)em2->em_expr))
						{
							relevant_clauses = gen_implied_qual(root,
																relevant_clauses,
																old_clause,
																(Node *)em1->em_expr,
																(Node *)em2->em_expr);
						}
					} /* foreach lcem2 */
				} /* safe_to_infer */
			} /* foreach lcclause */
		} /* BMS_SINGLETON */
	} /* foreach lcem1 */
}

/* TODO:
 *
 * note that we require types to be the same.  We could try converting them
 *   (introducing relabel nodes) as long as the conversion is a widening
 *   conversion (clause on int4 can be applied to int2 type by widening the
 *   int2 to an int4 when creating the replicated clause)
 *   likewise, is varchar(10) vs varchar(50) an issue at this point?
 */
void
generate_implied_quals(PlannerInfo *root)
{
	ListCell *lc;

    if (!root->config->gp_enable_predicate_propagation)
        return;

    /* generate using the query-global equivalence classes */
	foreach(lc, root->eq_classes)
	{
		EquivalenceClass *ec = (EquivalenceClass *) lfirst(lc);

		Assert(ec->ec_merged == NULL);		/* else shouldn't be in list */
		Assert(!ec->ec_broken);				/* not yet anyway... */

		/* Single-member ECs won't generate any deductions */
		if (list_length(ec->ec_members) <= 1)
			continue;

        gen_implied_quals_for_eclass(root, ec);
	}
}

/*
 * make_canonical_pathkey
 *	  Given the parameters for a PathKey, find any pre-existing matching
 *	  pathkey in the query's list of "canonical" pathkeys.  Make a new
 *	  entry if there's not one already.
 *
 * Note that this function must not be used until after we have completed
 * merging EquivalenceClasses.
 */
static PathKey *
make_canonical_pathkey(PlannerInfo *root,
					   EquivalenceClass *eclass, Oid opfamily,
					   int strategy, bool nulls_first)
{
	PathKey	   *pk;
	ListCell   *lc;
	MemoryContext oldcontext;

	/* The passed eclass might be non-canonical, so chase up to the top */
	while (eclass->ec_merged)
		eclass = eclass->ec_merged;

	foreach(lc, root->canon_pathkeys)
	{
		pk = (PathKey *) lfirst(lc);
		if (eclass == pk->pk_eclass &&
			opfamily == pk->pk_opfamily &&
			strategy == pk->pk_strategy &&
			nulls_first == pk->pk_nulls_first)
			return pk;
	}

	/*
	 * Be sure canonical pathkeys are allocated in the main planning context.
	 * Not an issue in normal planning, but it is for GEQO.
	 */
	oldcontext = MemoryContextSwitchTo(root->planner_cxt);

	pk = makePathKey(eclass, opfamily, strategy, nulls_first);
	root->canon_pathkeys = lappend(root->canon_pathkeys, pk);

	MemoryContextSwitchTo(oldcontext);

	return pk;
}

/*
 * pathkey_is_redundant
 *	   Is a pathkey redundant with one already in the given list?
 *
 * Both the given pathkey and the list members must be canonical for this
 * to work properly.  We detect two cases:
 *
 * 1. If the new pathkey's equivalence class contains a constant, and isn't
 * below an outer join, then we can disregard it as a sort key.  An example:
 *			SELECT ... WHERE x = 42 ORDER BY x, y;
 * We may as well just sort by y.  Note that because of opfamily matching,
 * this is semantically correct: we know that the equality constraint is one
 * that actually binds the variable to a single value in the terms of any
 * ordering operator that might go with the eclass.  This rule not only lets
 * us simplify (or even skip) explicit sorts, but also allows matching index
 * sort orders to a query when there are don't-care index columns.
 *
 * 2. If the new pathkey's equivalence class is the same as that of any
 * existing member of the pathkey list, then it is redundant.  Some examples:
 *			SELECT ... ORDER BY x, x;
 *			SELECT ... ORDER BY x, x DESC;
 *			SELECT ... WHERE x = y ORDER BY x, y;
 * In all these cases the second sort key cannot distinguish values that are
 * considered equal by the first, and so there's no point in using it.
 * Note in particular that we need not compare opfamily (all the opfamilies
 * of the EC have the same notion of equality) nor sort direction.
 *
 * Because the equivclass.c machinery forms only one copy of any EC per query,
 * pointer comparison is enough to decide whether canonical ECs are the same.
 */
static bool
pathkey_is_redundant(PathKey *new_pathkey, List *pathkeys)
{
	EquivalenceClass *new_ec = new_pathkey->pk_eclass;
	ListCell   *lc;

	/* Assert we've been given canonical pathkeys */
	Assert(!new_ec->ec_merged);

	/* Check for EC containing a constant --- unconditionally redundant */
	if (MUST_BE_REDUNDANT(new_ec))
		return true;

	/* If same EC already used in list, then redundant */
	foreach(lc, pathkeys)
	{
		PathKey	   *old_pathkey = (PathKey *) lfirst(lc);

		/* Assert we've been given canonical pathkeys */
		Assert(!old_pathkey->pk_eclass->ec_merged);

		if (new_ec == old_pathkey->pk_eclass)
			return true;
	}

	return false;
}

/*
 * canonicalize_pathkeys
 *	   Convert a not-necessarily-canonical pathkeys list to canonical form.
 *
 * Note that this function must not be used until after we have completed
 * merging EquivalenceClasses.
 */
List *
canonicalize_pathkeys(PlannerInfo *root, List *pathkeys)
{
	List	   *new_pathkeys = NIL;
	ListCell   *l;

	foreach(l, pathkeys)
	{
		PathKey	   *pathkey = (PathKey *) lfirst(l);
		EquivalenceClass *eclass;
		PathKey	   *cpathkey;

		/* Find the canonical (merged) EquivalenceClass */
		eclass = pathkey->pk_eclass;
		while (eclass->ec_merged)
			eclass = eclass->ec_merged;

		/*
		 * If we can tell it's redundant just from the EC, skip.
		 * pathkey_is_redundant would notice that, but we needn't even bother
		 * constructing the node...
		 */
		if (MUST_BE_REDUNDANT(eclass))
			continue;

		/* OK, build a canonicalized PathKey struct */
		cpathkey = make_canonical_pathkey(root,
										  eclass,
										  pathkey->pk_opfamily,
										  pathkey->pk_strategy,
										  pathkey->pk_nulls_first);

		/* Add to list unless redundant */
		if (!pathkey_is_redundant(cpathkey, new_pathkeys))
			new_pathkeys = lappend(new_pathkeys, cpathkey);
	}
	return new_pathkeys;
}

/*
 * make_pathkey_from_sortinfo
 *	  Given an expression, a sortop, and a nulls-first flag, create
 *	  a PathKey.  If canonicalize = true, the result is a "canonical"
 *	  PathKey, otherwise not.  (But note it might be redundant anyway.)
 *
 * canonicalize should always be TRUE after EquivalenceClass merging has
 * been performed, but FALSE if we haven't done EquivalenceClass merging yet.
 */
static PathKey *
make_pathkey_from_sortinfo(PlannerInfo *root,
						   Expr *expr, Oid ordering_op,
						   bool nulls_first,
						   bool canonicalize)
{
	Oid			opfamily,
				opcintype;
	int16		strategy;
	Oid			equality_op;
	List	   *opfamilies;
	EquivalenceClass *eclass;

	/*
	 * An ordering operator fully determines the behavior of its opfamily,
	 * so could only meaningfully appear in one family --- or perhaps two
	 * if one builds a reverse-sort opfamily, but there's not much point in
	 * that anymore.  But EquivalenceClasses need to contain opfamily lists
	 * based on the family membership of equality operators, which could
	 * easily be bigger.  So, look up the equality operator that goes with
	 * the ordering operator (this should be unique) and get its membership.
	 */

	/* Find the operator in pg_amop --- failure shouldn't happen */
	if (!get_ordering_op_properties(ordering_op,
									&opfamily, &opcintype, &strategy))
		elog(ERROR, "operator %u is not a valid ordering operator",
			 ordering_op);
	/* Get matching equality operator */
	equality_op = get_opfamily_member(opfamily,
									  opcintype,
									  opcintype,
									  BTEqualStrategyNumber);
	if (!OidIsValid(equality_op))		/* shouldn't happen */
		elog(ERROR, "could not find equality operator for ordering operator %u",
			 ordering_op);
	opfamilies = get_mergejoin_opfamilies(equality_op);
	if (!opfamilies)			/* certainly should find some */
		elog(ERROR, "could not find opfamilies for ordering operator %u",
			 ordering_op);

	/* Now find or create a matching EquivalenceClass */
	eclass = get_eclass_for_sort_expr(root, expr, opcintype, opfamilies);

	/* And finally we can find or create a PathKey node */
	if (canonicalize)
		return make_canonical_pathkey(root, eclass, opfamily,
									  strategy, nulls_first);
	else
		return makePathKey(eclass, opfamily, strategy, nulls_first);
}


/****************************************************************************
 *		PATHKEY COMPARISONS
 ****************************************************************************/

/*
 * compare_pathkeys
 *	  Compare two pathkeys to see if they are equivalent, and if not whether
 *	  one is "better" than the other.
 *
 *	  This function may only be applied to canonicalized pathkey lists.
 *	  In the canonical representation, pathkeys can be checked for equality
 *	  by simple pointer comparison.
 */
PathKeysComparison
compare_pathkeys(List *keys1, List *keys2)
{
	ListCell   *key1,
			   *key2;

	forboth(key1, keys1, key2, keys2)
	{
		PathKey	   *pathkey1 = (PathKey *) lfirst(key1);
		PathKey	   *pathkey2 = (PathKey *) lfirst(key2);

		/*
		 * XXX would like to check that we've been given canonicalized input,
		 * but PlannerInfo not accessible here...
		 */
#ifdef NOT_USED
		Assert(list_member_ptr(root->canon_pathkeys, pathkey1));
		Assert(list_member_ptr(root->canon_pathkeys, pathkey2));
#endif

		if (pathkey1 != pathkey2)
			return PATHKEYS_DIFFERENT;	/* no need to keep looking */
	}

	/*
	 * If we reached the end of only one list, the other is longer and
	 * therefore not a subset.
	 */
	if (key1 == NULL && key2 == NULL)
		return PATHKEYS_EQUAL;
	if (key1 != NULL)
		return PATHKEYS_BETTER1;	/* key1 is longer */
	return PATHKEYS_BETTER2;	/* key2 is longer */
}

/*
 * pathkeys_contained_in
 *	  Common special case of compare_pathkeys: we just want to know
 *	  if keys2 are at least as well sorted as keys1.
 */
bool
pathkeys_contained_in(List *keys1, List *keys2)
{
	switch (compare_pathkeys(keys1, keys2))
	{
		case PATHKEYS_EQUAL:
		case PATHKEYS_BETTER2:
			return true;
		default:
			break;
	}
	return false;
}

/*
 * get_cheapest_path_for_pathkeys
 *	  Find the cheapest path (according to the specified criterion) that
 *	  satisfies the given pathkeys.  Return NULL if no such path.
 *
 * 'paths' is a list of possible paths that all generate the same relation
 * 'pathkeys' represents a required ordering (already canonicalized!)
 * 'cost_criterion' is STARTUP_COST or TOTAL_COST
 */
Path *
get_cheapest_path_for_pathkeys(List *paths, List *pathkeys,
							   CostSelector cost_criterion)
{
	Path	   *matched_path = NULL;
	ListCell   *l;

	foreach(l, paths)
	{
		Path	   *path = (Path *) lfirst(l);

		/*
		 * Since cost comparison is a lot cheaper than pathkey comparison, do
		 * that first.	(XXX is that still true?)
		 */
		if (matched_path != NULL &&
			compare_path_costs(matched_path, path, cost_criterion) <= 0)
			continue;

		if (pathkeys_contained_in(pathkeys, path->pathkeys))
			matched_path = path;
	}
	return matched_path;
}

/*
 * get_cheapest_fractional_path_for_pathkeys
 *	  Find the cheapest path (for retrieving a specified fraction of all
 *	  the tuples) that satisfies the given pathkeys.
 *	  Return NULL if no such path.
 *
 * See compare_fractional_path_costs() for the interpretation of the fraction
 * parameter.
 *
 * 'paths' is a list of possible paths that all generate the same relation
 * 'pathkeys' represents a required ordering (already canonicalized!)
 * 'fraction' is the fraction of the total tuples expected to be retrieved
 */
Path *
get_cheapest_fractional_path_for_pathkeys(List *paths,
										  List *pathkeys,
										  double fraction)
{
	Path	   *matched_path = NULL;
	ListCell   *l;

	foreach(l, paths)
	{
		Path	   *path = (Path *) lfirst(l);

		/*
		 * Since cost comparison is a lot cheaper than pathkey comparison, do
		 * that first.
		 */
		if (matched_path != NULL &&
			compare_fractional_path_costs(matched_path, path, fraction) <= 0)
			continue;

		if (pathkeys_contained_in(pathkeys, path->pathkeys))
			matched_path = path;
	}
	return matched_path;
}

/****************************************************************************
 *		NEW PATHKEY FORMATION
 ****************************************************************************/

/*
 * build_index_pathkeys
 *	  Build a pathkeys list that describes the ordering induced by an index
 *	  scan using the given index.  (Note that an unordered index doesn't
 *	  induce any ordering; such an index will have no sortop OIDS in
 *	  its sortops arrays, and we will return NIL.)
 *
 * If 'scandir' is BackwardScanDirection, attempt to build pathkeys
 * representing a backwards scan of the index.	Return NIL if can't do it.
 *
 * The result is canonical, meaning that redundant pathkeys are removed;
 * it may therefore have fewer entries than there are index columns.
 *
 * We generate the full pathkeys list whether or not all are useful for the
 * current query.  Caller should do truncate_useless_pathkeys().
 */
List *
build_index_pathkeys(PlannerInfo *root,
					 IndexOptInfo *index,
					 ScanDirection scandir)
{
	List	   *retval = NIL;
	ListCell   *indexprs_item = list_head(index->indexprs);
	int			i;

	for (i = 0; i < index->ncolumns; i++)
	{
		Oid			sortop;
		bool		nulls_first;
		int			ikey;
		Expr	   *indexkey;
		PathKey	   *cpathkey;

		if (ScanDirectionIsBackward(scandir))
		{
			sortop = index->revsortop[i];
			nulls_first = !index->nulls_first[i];
		}
		else
		{
			sortop = index->fwdsortop[i];
			nulls_first = index->nulls_first[i];
		}

		if (!OidIsValid(sortop))
			break;				/* no more orderable columns */

		ikey = index->indexkeys[i];
		if (ikey != 0)
		{
			/* simple index column */
			indexkey = (Expr *) find_indexkey_var(root, index->rel, ikey);
		}
		else
		{
			/* expression --- assume we need not copy it */
			if (indexprs_item == NULL)
				elog(ERROR, "wrong number of index expressions");
			indexkey = (Expr *) lfirst(indexprs_item);
			indexprs_item = lnext(indexprs_item);
		}

		/* OK, make a canonical pathkey for this sort key */
		cpathkey = make_pathkey_from_sortinfo(root,
											  indexkey,
											  sortop,
											  nulls_first,
											  true);

		/* Add to list unless redundant */
		if (!pathkey_is_redundant(cpathkey, retval))
			retval = lappend(retval, cpathkey);
	}

	return retval;
}

/*
 * Find or make a Var node for the specified attribute of the rel.
 *
 * We first look for the var in the rel's target list, because that's
 * easy and fast.  But the var might not be there (this should normally
 * only happen for vars that are used in WHERE restriction clauses,
 * but not in join clauses or in the SELECT target list).  In that case,
 * gin up a Var node the hard way.
 */
Var *
find_indexkey_var(PlannerInfo *root, RelOptInfo *rel, AttrNumber varattno)
{
	ListCell   *temp;
	Index		relid;
	Oid			reloid,
				vartypeid;
	int32		type_mod;

	foreach(temp, rel->reltargetlist)
	{
		Var		   *var = (Var *) lfirst(temp);

		if (IsA(var, Var) &&
			var->varattno == varattno)
			return var;
	}

	relid = rel->relid;
	reloid = getrelid(relid, root->parse->rtable);
	get_atttypetypmod(reloid, varattno, &vartypeid, &type_mod);

	return makeVar(relid, varattno, vartypeid, type_mod, 0);
}

/*
 * convert_subquery_pathkeys
 *	  Build a pathkeys list that describes the ordering of a subquery's
 *	  result, in the terms of the outer query.	This is essentially a
 *	  task of conversion.
 *
 * 'rel': outer query's RelOptInfo for the subquery relation.
 * 'subquery_pathkeys': the subquery's output pathkeys, in its terms.
 *
 * It is not necessary for caller to do truncate_useless_pathkeys(),
 * because we select keys in a way that takes usefulness of the keys into
 * account.
 */
List *
convert_subquery_pathkeys(PlannerInfo *root, RelOptInfo *rel,
						  List *subquery_pathkeys)
{
	List	   *retval = NIL;
	int			retvallen = 0;
	int			outer_query_keys = list_length(root->query_pathkeys);
	List	   *sub_tlist = rel->subplan->targetlist;
	ListCell   *i;

	foreach(i, subquery_pathkeys)
	{
		PathKey	   *sub_pathkey = (PathKey *) lfirst(i);
		EquivalenceClass *sub_eclass = sub_pathkey->pk_eclass;
		PathKey	   *best_pathkey = NULL;
		int			best_score = -1;
		ListCell   *j;

		/*
		 * The sub_pathkey's EquivalenceClass could contain multiple elements
		 * (representing knowledge that multiple items are effectively equal).
		 * Each element might match none, one, or more of the output columns
		 * that are visible to the outer query. This means we may have
		 * multiple possible representations of the sub_pathkey in the context
		 * of the outer query.  Ideally we would generate them all and put
		 * them all into an EC of the outer query, thereby propagating
		 * equality knowledge up to the outer query.  Right now we cannot do
		 * so, because the outer query's EquivalenceClasses are already frozen
		 * when this is called. Instead we prefer the one that has the highest
		 * "score" (number of EC peers, plus one if it matches the outer
		 * query_pathkeys). This is the most likely to be useful in the outer
		 * query.
		 */
		foreach(j, sub_eclass->ec_members)
		{
			EquivalenceMember *sub_member = (EquivalenceMember *) lfirst(j);
			Expr	   *sub_expr = sub_member->em_expr;
			Expr	   *rtarg;
			ListCell   *k;

			/*
			 * We handle two cases: the sub_pathkey key can be either an exact
			 * match for a targetlist entry, or a RelabelType of a targetlist
			 * entry.  (The latter case is worth extra code because it arises
			 * frequently in connection with varchar fields.)
			 */
			if (IsA(sub_expr, RelabelType))
				rtarg = ((RelabelType *) sub_expr)->arg;
			else
				rtarg = NULL;

			foreach(k, sub_tlist)
			{
				TargetEntry *tle = (TargetEntry *) lfirst(k);
				Expr	   *outer_expr;
				EquivalenceClass *outer_ec;
				PathKey	   *outer_pk;
				int			score;

				/* resjunk items aren't visible to outer query */
				if (tle->resjunk)
					continue;

				if (equal(tle->expr, sub_expr))
				{
					/* Exact match */
					outer_expr = (Expr *)
						makeVar(rel->relid,
								tle->resno,
								exprType((Node *) tle->expr),
								exprTypmod((Node *) tle->expr),
								0);
				}
				else if (rtarg && equal(tle->expr, rtarg))
				{
					/* Match after discarding RelabelType */
					outer_expr = (Expr *)
						makeVar(rel->relid,
								tle->resno,
								exprType((Node *) tle->expr),
								exprTypmod((Node *) tle->expr),
								0);
					outer_expr = (Expr *)
						makeRelabelType((Expr *) outer_expr,
										((RelabelType *) sub_expr)->resulttype,
									 ((RelabelType *) sub_expr)->resulttypmod,
								   ((RelabelType *) sub_expr)->relabelformat);
				}
				else
					continue;

				/* Found a representation for this sub_pathkey */
				outer_ec = get_eclass_for_sort_expr(root,
													outer_expr,
													sub_member->em_datatype,
													sub_eclass->ec_opfamilies);
				outer_pk = make_canonical_pathkey(root,
												  outer_ec,
												  sub_pathkey->pk_opfamily,
												  sub_pathkey->pk_strategy,
												  sub_pathkey->pk_nulls_first);
				/* score = # of equivalence peers */
				score = list_length(outer_ec->ec_members) - 1;
				/* +1 if it matches the proper query_pathkeys item */
				if (retvallen < outer_query_keys &&
					list_nth(root->query_pathkeys, retvallen) == outer_pk)
					score++;
				if (score > best_score)
				{
					best_pathkey = outer_pk;
					best_score = score;
				}
			}
		}

		/*
		 * If we couldn't find a representation of this sub_pathkey, we're
		 * done (we can't use the ones to its right, either).
		 */
		if (!best_pathkey)
			break;

		/*
		 * Eliminate redundant ordering info; could happen if outer query
		 * equivalences subquery keys...
		 */
		if (!pathkey_is_redundant(best_pathkey, retval))
		{
			retval = lappend(retval, best_pathkey);
			retvallen++;
		}
	}

	return retval;
}

/*
 * build_join_pathkeys
 *	  Build the path keys for a join relation constructed by mergejoin or
 *	  nestloop join.  This is normally the same as the outer path's keys.
 *
 *	  EXCEPTION: in a FULL or RIGHT join, we cannot treat the result as
 *	  having the outer path's path keys, because null lefthand rows may be
 *	  inserted at random points.  It must be treated as unsorted.
 *
 *	  We truncate away any pathkeys that are uninteresting for higher joins.
 *
 * 'joinrel' is the join relation that paths are being formed for
 * 'jointype' is the join type (inner, left, full, etc)
 * 'outer_pathkeys' is the list of the current outer path's path keys
 *
 * Returns the list of new path keys.
 */
List *
build_join_pathkeys(PlannerInfo *root,
					RelOptInfo *joinrel,
					JoinType jointype,
					List *outer_pathkeys)
{
	if (jointype == JOIN_FULL || jointype == JOIN_RIGHT)
		return NIL;

	/*
	 * This used to be quite a complex bit of code, but now that all pathkey
	 * sublists start out life canonicalized, we don't have to do a darn thing
	 * here!
	 *
	 * We do, however, need to truncate the pathkeys list, since it may
	 * contain pathkeys that were useful for forming this joinrel but are
	 * uninteresting to higher levels.
	 */
	return truncate_useless_pathkeys(root, joinrel, outer_pathkeys);
}


/****************************************************************************
 *		PATHKEYS FOR DISTRIBUTED QUERIES
 ****************************************************************************/

/*
 * cdb_make_pathkey_for_expr_non_canonical
 *	  Returns a a non canonicalized pathkey item.
 *
 *    The caller specifies the name of the equality operator thus:
 *          list_make1(makeString("="))
 *
 *    The 'sortop' field of the expr's PathKey node is filled
 *    with the Oid of the sort operator that would be used for a
 *    merge join with another expr of the same data type, using the
 *    equality operator whose name is given.  Partitioning doesn't
 *    itself use the sort operator, but its Oid is needed to
 *    associate the PathKey with the same equivalence class
 *    (canonical pathkey) as any other expressions to which
 *    our expr is constrained by compatible merge-joinable
 *    equality operators.  (We assume, in what may be a temporary
 *    excess of optimism, that our hashed partitioning function
 *    implements the same notion of equality as these operators.)
 */
PathKey *
cdb_make_pathkey_for_expr_non_canonical(PlannerInfo *root, Node *expr, List *eqopname)
{
	return cdb_make_pathkey_for_expr(root, expr, eqopname, false);
}

/*
 * cdb_make_pathkey_for_expr
 *	  Returns a canonicalized PathKey which represents an equivalence
 *    class of expressions that must be equal to the given expression.
 *
 *    The caller specifies the name of the equality operator thus:
 *          list_make1(makeString("="))
 *
 *    The 'opfamily' field of resulting PathKey is filled with the operator
 *    family that would be used for a merge join with another expr of the
 *    same data type, using the equality operator whose name is given.
 *    Partitioning doesn't itself use the sort operator, but its Oid is
 *    needed to associate the PathKey with the same equivalence class
 *    (canonical pathkey) as any other expressions to which
 *    our expr is constrained by compatible merge-joinable
 *    equality operators.  (We assume, in what may be a temporary
 *    excess of optimism, that our hashed partitioning function
 *    implements the same notion of equality as these operators.)
 */
PathKey *
cdb_make_pathkey_for_expr(PlannerInfo    *root,
					      Node     *expr,
                          List     *eqopname,
						  bool	canonical)
{
    Oid             opfamily = InvalidOid;
    Oid             typeoid = InvalidOid;
    Oid	            eqopoid = InvalidOid;
    PathKey        *pk = NULL;
	List           *mergeopfamilies;
	EquivalenceClass *eclass;
	int             strategy;
	ListCell       *lc;

    /* Get the expr's data type. */
    typeoid = exprType(expr);

    /* Get Oid of the equality operator applied to two values of that type. */
    eqopoid = compatible_oper_opid(eqopname, typeoid, typeoid, true);

    /*
	 * Get Oid of the sort operator that would be used for a
     * sort-merge equijoin on a pair of exprs of the same type.
     */
	if (eqopoid == InvalidOid || !op_mergejoinable(eqopoid))
		elog(ERROR, "could not find mergejoinable = operator for type %u", typeoid);

	mergeopfamilies = get_mergejoin_opfamilies(eqopoid);
	foreach(lc, mergeopfamilies)
	{
		opfamily = lfirst_oid(lc);
		strategy = get_op_opfamily_strategy(eqopoid, opfamily);
		if (strategy)
			break;
	}
	eclass = get_eclass_for_sort_expr(root, (Expr *) expr, typeoid, mergeopfamilies);
	if (!canonical)
		pk = makePathKey(eclass, opfamily, strategy, false);
	else
		pk = make_canonical_pathkey(root, eclass, opfamily, strategy, false);

	return pk;
}

/*
 * cdb_pull_up_pathkey
 *
 * Given a pathkey, finds a PathKey whose key expr can be projected
 * thru a given targetlist.  If found, builds the transformed key expr
 * and returns the canonical pathkey representing its equivalence class.
 *
 * Returns NULL if the given pathkey does not have a PathKey whose
 * key expr can be rewritten in terms of the projected output columns.
 *
 * Note that this function does not unite the pre- and post-projection
 * equivalence classes.  Equivalences known on one side of the projection
 * are not made known on the other side.  Although that might be useful,
 * it would have to be done at an earlier point in the planner.
 *
 * At present this function doesn't support pull-up from a subquery into a
 * containing query: there is no provision for adjusting the varlevelsup
 * field in Var nodes for outer references.  This could be added if needed.
 *
 * 'pathkey' is a List of PathKey.
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
 *
 * NB: We ignore the presence or absence of a RelabelType node atop either
 * expr in determining whether a PathKey expr matches a targetlist expr.
 */
PathKey *
cdb_pull_up_pathkey(PlannerInfo    *root,
                    PathKey        *pathkey,
                    Relids          relids,
                    List           *targetlist,
                    List           *newvarlist,
                    Index           newrelid)
{
	Expr	   *sub_pathkeyexpr;
	EquivalenceClass *outer_ec;
	Expr	   *newexpr = NULL;

    Assert(pathkey);
    Assert(!newvarlist ||
           list_length(newvarlist) == list_length(targetlist));

	/* Find an expr that we can rewrite to use the projected columns. */
	sub_pathkeyexpr = cdbpullup_findPathKeyExprInTargetList(pathkey, targetlist);

	/* Replace expr's Var nodes with new ones referencing the targetlist. */
	if (sub_pathkeyexpr)
	{
		newexpr = cdbpullup_expr(sub_pathkeyexpr,
								 targetlist,
								 newvarlist,
								 newrelid);
	}
	/* If not found, see if the equiv class contains a constant expr. */
	else if (CdbPathkeyEqualsConstant(pathkey))
	{
		ListCell *lc;

		foreach (lc, pathkey->pk_eclass->ec_members)
		{
			EquivalenceMember *em = lfirst(lc);

			if (em->em_is_const)
			{
				newexpr = (Expr *) copyObject(em->em_expr);
				break;
			}
		}
	}
    /* Fail if no usable expr. */
    else
        return NULL;

	if (!newexpr)
		elog(ERROR, "could not pull up path key using projected target list");

	outer_ec = get_eclass_for_sort_expr(root,
										newexpr,
										exprType((Node *) newexpr),
										pathkey->pk_eclass->ec_opfamilies);

    /* Find or create the equivalence class for the transformed expr. */
    return make_canonical_pathkey(root,
								  outer_ec,
								  pathkey->pk_opfamily,
								  pathkey->pk_strategy,
								  false);
}



/****************************************************************************
 *		PATHKEYS AND SORT CLAUSES
 ****************************************************************************/

/*
 * make_pathkeys_for_sortclauses
 *		Generate a pathkeys list that represents the sort order specified
 *		by a list of SortClauses (GroupClauses will work too!)
 *
 * If canonicalize is TRUE, the resulting PathKeys are all in canonical form;
 * otherwise not.  canonicalize should always be TRUE after EquivalenceClass
 * merging has been performed, but FALSE if we haven't done EquivalenceClass
 * merging yet.  (We provide this option because grouping_planner() needs to
 * be able to represent requested pathkeys before the equivalence classes have
 * been created for the query.)
 *
 * 'sortclauses' is a list of SortClause or GroupClause nodes
 * 'tlist' is the targetlist to find the referenced tlist entries in
 */
List *
make_pathkeys_for_sortclauses(PlannerInfo *root,
							  List *sortclauses,
							  List *tlist,
							  bool canonicalize)
{
	List	   *pathkeys = NIL;
	ListCell   *l;

	foreach(l, sortclauses)
	{
		SortClause *sortcl = (SortClause *) lfirst(l);
		Expr	   *sortkey;
		PathKey	   *pathkey;

		sortkey = (Expr *) get_sortgroupclause_expr(sortcl, tlist);
		pathkey = make_pathkey_from_sortinfo(root,
											 sortkey,
											 sortcl->sortop,
											 sortcl->nulls_first,
											 canonicalize);

		/* Canonical form eliminates redundant ordering keys */
		if (canonicalize)
		{
			if (!pathkey_is_redundant(pathkey, pathkeys))
				pathkeys = lappend(pathkeys, pathkey);
		}
		else
			pathkeys = lappend(pathkeys, pathkey);
	}
	return pathkeys;
}

/****************************************************************************
 *      PATHKEYS AND GROUPCLAUSES AND GROUPINGCLAUSE
 ***************************************************************************/

/*
 * make_pathkeys_for_groupclause
 *   Generate a pathkeys list that represents the sort order specified by
 *   a list of GroupClauses or GroupingClauses.
 *
 * Note: similar to make_pathkeys_for_sortclauses, the result is NOT in
 * canonical form.
 */
List *
make_pathkeys_for_groupclause(PlannerInfo *root,
							  List *groupclause,
							  List *tlist)
{
	List *pathkeys = NIL;
	ListCell *l;

	List *sub_pathkeys = NIL;

	foreach(l, groupclause)
	{
		Expr	   *sortkey;
		PathKey    *pathkey;

		Node *node = lfirst(l);

		if (node == NULL)
			continue;

		if (IsA(node, GroupClause))
		{
			GroupClause *gc = (GroupClause*) node;
			sortkey = (Expr *) get_sortgroupclause_expr(gc, tlist);
			pathkey = make_pathkey_from_sortinfo(root, sortkey, gc->sortop, gc->nulls_first, false);

			/*
			 * Similar to SortClauses, the pathkey becomes a one-elment sublist.
			 * canonicalize_pathkeys() might replace it with a longer sublist later.
			 */
			pathkeys = lappend(pathkeys, pathkey);
		}
		else if (IsA(node, List))
		{
			pathkeys = list_concat(pathkeys,
								   make_pathkeys_for_groupclause(root, (List *)node,
																 tlist));
		}
		else if (IsA(node, GroupingClause))
		{
			sub_pathkeys =
				list_concat(sub_pathkeys,
							make_pathkeys_for_groupclause(root, ((GroupingClause*)node)->groupsets,
															 tlist));
		}
	}

	pathkeys = list_concat(pathkeys, sub_pathkeys);

	return pathkeys;
}

/****************************************************************************
 *		PATHKEYS AND MERGECLAUSES
 ****************************************************************************/

/*
 * cache_mergeclause_eclasses
 *		Make the cached EquivalenceClass links valid in a mergeclause
 *		restrictinfo.
 *
 * RestrictInfo contains fields in which we may cache pointers to
 * EquivalenceClasses for the left and right inputs of the mergeclause.
 * (If the mergeclause is a true equivalence clause these will be the
 * same EquivalenceClass, otherwise not.)
 */
void
cache_mergeclause_eclasses(PlannerInfo *root, RestrictInfo *restrictinfo)
{
	Assert(restrictinfo->mergeopfamilies != NIL);

	/* the cached values should be either both set or both not */
	if (restrictinfo->left_ec == NULL)
	{
		Expr	   *clause = restrictinfo->clause;
		Oid			lefttype,
					righttype;

		/* Need the declared input types of the operator */
		op_input_types(((OpExpr *) clause)->opno, &lefttype, &righttype);

		/* Find or create a matching EquivalenceClass for each side */
		restrictinfo->left_ec =
			get_eclass_for_sort_expr(root,
									 (Expr *) get_leftop(clause),
									 lefttype,
									 restrictinfo->mergeopfamilies);
		restrictinfo->right_ec =
			get_eclass_for_sort_expr(root,
									 (Expr *) get_rightop(clause),
									 righttype,
									 restrictinfo->mergeopfamilies);
	}
	else
		Assert(restrictinfo->right_ec != NULL);
}

/*
 * find_mergeclauses_for_pathkeys
 *	  This routine attempts to find a set of mergeclauses that can be
 *	  used with a specified ordering for one of the input relations.
 *	  If successful, it returns a list of mergeclauses.
 *
 * 'pathkeys' is a pathkeys list showing the ordering of an input path.
 * 'outer_keys' is TRUE if these keys are for the outer input path,
 *			FALSE if for inner.
 * 'restrictinfos' is a list of mergejoinable restriction clauses for the
 *			join relation being formed.
 *
 * The restrictinfos must be marked (via outer_is_left) to show which side
 * of each clause is associated with the current outer path.  (See
 * select_mergejoin_clauses())
 *
 * The result is NIL if no merge can be done, else a maximal list of
 * usable mergeclauses (represented as a list of their restrictinfo nodes).
 */
List *
find_mergeclauses_for_pathkeys(PlannerInfo *root,
							   List *pathkeys,
							   bool outer_keys,
							   List *restrictinfos)
{
	List	   *mergeclauses = NIL;
	ListCell   *i;

	/* make sure we have eclasses cached in the clauses */
	foreach(i, restrictinfos)
	{
		RestrictInfo *rinfo = (RestrictInfo *) lfirst(i);

		cache_mergeclause_eclasses(root, rinfo);
	}

	foreach(i, pathkeys)
	{
		PathKey	   *pathkey = (PathKey *) lfirst(i);
		EquivalenceClass *pathkey_ec = pathkey->pk_eclass;
		List	   *matched_restrictinfos = NIL;
		ListCell   *j;

		/*----------
		 * A mergejoin clause matches a pathkey if it has the same EC.
		 * If there are multiple matching clauses, take them all.  In plain
		 * inner-join scenarios we expect only one match, because
		 * equivalence-class processing will have removed any redundant
		 * mergeclauses.  However, in outer-join scenarios there might be
		 * multiple matches.  An example is
		 *
		 *	select * from a full join b
		 *		on a.v1 = b.v1 and a.v2 = b.v2 and a.v1 = b.v2;
		 *
		 * Given the pathkeys ({a.v1}, {a.v2}) it is okay to return all three
		 * clauses (in the order a.v1=b.v1, a.v1=b.v2, a.v2=b.v2) and indeed
		 * we *must* do so or we will be unable to form a valid plan.
		 *
		 * We expect that the given pathkeys list is canonical, which means
		 * no two members have the same EC, so it's not possible for this
		 * code to enter the same mergeclause into the result list twice.
		 *
		 * XXX it's possible that multiple matching clauses might have
		 * different ECs on the other side, in which case the order we put
		 * them into our result makes a difference in the pathkeys required
		 * for the other input path.  However this routine hasn't got any info
		 * about which order would be best, so for now we disregard that case
		 * (which is probably a corner case anyway).
		 *----------
		 */
		foreach(j, restrictinfos)
		{
			RestrictInfo *rinfo = (RestrictInfo *) lfirst(j);
			EquivalenceClass *clause_ec;

			if (outer_keys)
				clause_ec = rinfo->outer_is_left ?
					rinfo->left_ec : rinfo->right_ec;
			else
				clause_ec = rinfo->outer_is_left ?
					rinfo->right_ec : rinfo->left_ec;
			if (clause_ec == pathkey_ec)
				matched_restrictinfos = lappend(matched_restrictinfos, rinfo);
		}

		/*
		 * If we didn't find a mergeclause, we're done --- any additional
		 * sort-key positions in the pathkeys are useless.	(But we can still
		 * mergejoin if we found at least one mergeclause.)
		 */
		if (matched_restrictinfos == NIL)
			break;

		/*
		 * If we did find usable mergeclause(s) for this sort-key position,
		 * add them to result list.
		 */
		mergeclauses = list_concat(mergeclauses, matched_restrictinfos);
	}

	return mergeclauses;
}

/*
 * select_outer_pathkeys_for_merge
 *	  Builds a pathkey list representing a possible sort ordering
 *	  that can be used with the given mergeclauses.
 *
 * 'mergeclauses' is a list of RestrictInfos for mergejoin clauses
 *			that will be used in a merge join.
 * 'joinrel' is the join relation we are trying to construct.
 *
 * The restrictinfos must be marked (via outer_is_left) to show which side
 * of each clause is associated with the current outer path.  (See
 * select_mergejoin_clauses())
 *
 * Returns a pathkeys list that can be applied to the outer relation.
 *
 * Since we assume here that a sort is required, there is no particular use
 * in matching any available ordering of the outerrel.  (joinpath.c has an
 * entirely separate code path for considering sort-free mergejoins.)  Rather,
 * it's interesting to try to match the requested query_pathkeys so that a
 * second output sort may be avoided; and failing that, we try to list "more
 * popular" keys (those with the most unmatched EquivalenceClass peers)
 * earlier, in hopes of making the resulting ordering useful for as many
 * higher-level mergejoins as possible.
 */
List *
select_outer_pathkeys_for_merge(PlannerInfo *root,
								List *mergeclauses,
								RelOptInfo *joinrel)
{
	List	   *pathkeys = NIL;
	int			nClauses = list_length(mergeclauses);
	EquivalenceClass **ecs;
	int		   *scores;
	int			necs;
	ListCell   *lc;
	int			j;

	/* Might have no mergeclauses */
	if (nClauses == 0)
		return NIL;

	/*
	 * Make arrays of the ECs used by the mergeclauses (dropping any
	 * duplicates) and their "popularity" scores.
	 */
	ecs = (EquivalenceClass **) palloc(nClauses * sizeof(EquivalenceClass *));
	scores = (int *) palloc(nClauses * sizeof(int));
	necs = 0;

	foreach(lc, mergeclauses)
	{
		RestrictInfo *rinfo = (RestrictInfo *) lfirst(lc);
		EquivalenceClass *oeclass;
		int			score;
		ListCell   *lc2;

		/* get the outer eclass */
		cache_mergeclause_eclasses(root, rinfo);

		if (rinfo->outer_is_left)
			oeclass = rinfo->left_ec;
		else
			oeclass = rinfo->right_ec;

		/* reject duplicates */
		for (j = 0; j < necs; j++)
		{
			if (ecs[j] == oeclass)
				break;
		}
		if (j < necs)
			continue;

		/* compute score */
		score = 0;
		foreach(lc2, oeclass->ec_members)
		{
			EquivalenceMember *em = (EquivalenceMember *) lfirst(lc2);

			/* Potential future join partner? */
			if (!em->em_is_const && !em->em_is_child &&
				!bms_overlap(em->em_relids, joinrel->relids))
				score++;
		}

		ecs[necs] = oeclass;
		scores[necs] = score;
		necs++;
	}

	/*
	 * Find out if we have all the ECs mentioned in query_pathkeys; if so
	 * we can generate a sort order that's also useful for final output.
	 * There is no percentage in a partial match, though, so we have to
	 * have 'em all.
	 */
	if (root->query_pathkeys)
	{
		foreach(lc, root->query_pathkeys)
		{
			PathKey	   *query_pathkey = (PathKey *) lfirst(lc);
			EquivalenceClass *query_ec = query_pathkey->pk_eclass;

			for (j = 0; j < necs; j++)
			{
				if (ecs[j] == query_ec)
					break;		/* found match */
			}
			if (j >= necs)
				break;			/* didn't find match */
		}
		/* if we got to the end of the list, we have them all */
		if (lc == NULL)
		{
			/* copy query_pathkeys as starting point for our output */
			pathkeys = list_copy(root->query_pathkeys);
			/* mark their ECs as already-emitted */
			foreach(lc, root->query_pathkeys)
			{
				PathKey	   *query_pathkey = (PathKey *) lfirst(lc);
				EquivalenceClass *query_ec = query_pathkey->pk_eclass;

				for (j = 0; j < necs; j++)
				{
					if (ecs[j] == query_ec)
					{
						scores[j] = -1;
						break;
					}
				}
			}
		}
	}

	/*
	 * Add remaining ECs to the list in popularity order, using a default
	 * sort ordering.  (We could use qsort() here, but the list length is
	 * usually so small it's not worth it.)
	 */
	for (;;)
	{
		int		best_j;
		int		best_score;
		EquivalenceClass *ec;
		PathKey *pathkey;

		best_j = 0;
		best_score = scores[0];
		for (j = 1; j < necs; j++)
		{
			if (scores[j] > best_score)
			{
				best_j = j;
				best_score = scores[j];
			}
		}
		if (best_score < 0)
			break;				/* all done */
		ec = ecs[best_j];
		scores[best_j] = -1;
		pathkey = make_canonical_pathkey(root,
										 ec,
										 linitial_oid(ec->ec_opfamilies),
										 BTLessStrategyNumber,
										 false);
		/* can't be redundant because no duplicate ECs */
		Assert(!pathkey_is_redundant(pathkey, pathkeys));
		pathkeys = lappend(pathkeys, pathkey);
	}

	pfree(ecs);
	pfree(scores);

	return pathkeys;
}

/*
 * make_inner_pathkeys_for_merge
 *	  Builds a pathkey list representing the explicit sort order that
 *	  must be applied to an inner path to make it usable with the
 *	  given mergeclauses.
 *
 * 'mergeclauses' is a list of RestrictInfos for mergejoin clauses
 *			that will be used in a merge join.
 * 'outer_pathkeys' are the already-known canonical pathkeys for the outer
 *			side of the join.
 *
 * The restrictinfos must be marked (via outer_is_left) to show which side
 * of each clause is associated with the current outer path.  (See
 * select_mergejoin_clauses())
 *
 * Returns a pathkeys list that can be applied to the inner relation.
 *
 * Note that it is not this routine's job to decide whether sorting is
 * actually needed for a particular input path.  Assume a sort is necessary;
 * just make the keys, eh?
 */
List *
make_inner_pathkeys_for_merge(PlannerInfo *root,
							  List *mergeclauses,
							  List *outer_pathkeys)
{
	List	   *pathkeys = NIL;
	EquivalenceClass *lastoeclass;
	PathKey	   *opathkey;
	ListCell   *lc;
	ListCell   *lop;

	lastoeclass = NULL;
	opathkey = NULL;
	lop = list_head(outer_pathkeys);

	foreach(lc, mergeclauses)
	{
		RestrictInfo *rinfo = (RestrictInfo *) lfirst(lc);
		EquivalenceClass *oeclass;
		EquivalenceClass *ieclass;
		PathKey	   *pathkey;

		cache_mergeclause_eclasses(root, rinfo);

		if (rinfo->outer_is_left)
		{
			oeclass = rinfo->left_ec;
			ieclass = rinfo->right_ec;
		}
		else
		{
			oeclass = rinfo->right_ec;
			ieclass = rinfo->left_ec;
		}

		/* outer eclass should match current or next pathkeys */
		/* we check this carefully for debugging reasons */
		if (oeclass != lastoeclass)
		{
			if (!lop)
				elog(ERROR, "too few pathkeys for mergeclauses");
			opathkey = (PathKey *) lfirst(lop);
			lop = lnext(lop);
			lastoeclass = opathkey->pk_eclass;
			if (oeclass != lastoeclass)
				elog(ERROR, "outer pathkeys do not match mergeclause");
		}

		/*
		 * Often, we'll have same EC on both sides, in which case the outer
		 * pathkey is also canonical for the inner side, and we can skip a
		 * useless search.
		 */
		if (ieclass == oeclass)
			pathkey = opathkey;
		else
			pathkey = make_canonical_pathkey(root,
											 ieclass,
											 opathkey->pk_opfamily,
											 opathkey->pk_strategy,
											 opathkey->pk_nulls_first);

		/*
		 * Don't generate redundant pathkeys (can happen if multiple
		 * mergeclauses refer to same EC).
		 */
		if (!pathkey_is_redundant(pathkey, pathkeys))
			pathkeys = lappend(pathkeys, pathkey);
	}

	return pathkeys;
}

/****************************************************************************
 *		PATHKEY USEFULNESS CHECKS
 *
 * We only want to remember as many of the pathkeys of a path as have some
 * potential use, either for subsequent mergejoins or for meeting the query's
 * requested output ordering.  This ensures that add_path() won't consider
 * a path to have a usefully different ordering unless it really is useful.
 * These routines check for usefulness of given pathkeys.
 ****************************************************************************/

/*
 * pathkeys_useful_for_merging
 *		Count the number of pathkeys that may be useful for mergejoins
 *		above the given relation.
 *
 * We consider a pathkey potentially useful if it corresponds to the merge
 * ordering of either side of any joinclause for the rel.  This might be
 * overoptimistic, since joinclauses that require different other relations
 * might never be usable at the same time, but trying to be exact is likely
 * to be more trouble than it's worth.
 */
int
pathkeys_useful_for_merging(PlannerInfo *root, RelOptInfo *rel, List *pathkeys)
{
	int			useful = 0;
	ListCell   *i;

	foreach(i, pathkeys)
	{
		PathKey	   *pathkey = (PathKey *) lfirst(i);
		bool		matched = false;
		ListCell   *j;

		/*
		 * First look into the EquivalenceClass of the pathkey, to see if
		 * there are any members not yet joined to the rel.  If so, it's
		 * surely possible to generate a mergejoin clause using them.
		 */
		if (rel->has_eclass_joins &&
			eclass_useful_for_merging(pathkey->pk_eclass, rel))
			matched = true;
		else
		{
			/*
			 * Otherwise search the rel's joininfo list, which contains
			 * non-EquivalenceClass-derivable join clauses that might
			 * nonetheless be mergejoinable.
			 */
			foreach(j, rel->joininfo)
			{
				RestrictInfo *restrictinfo = (RestrictInfo *) lfirst(j);

				if (restrictinfo->mergeopfamilies == NIL)
					continue;
				cache_mergeclause_eclasses(root, restrictinfo);

				if (pathkey->pk_eclass == restrictinfo->left_ec ||
					pathkey->pk_eclass == restrictinfo->right_ec)
				{
					matched = true;
					break;
				}
			}
		}

		/*
		 * If we didn't find a mergeclause, we're done --- any additional
		 * sort-key positions in the pathkeys are useless.	(But we can still
		 * mergejoin if we found at least one mergeclause.)
		 */
		if (matched)
			useful++;
		else
			break;
	}

	return useful;
}

/*
 * pathkeys_useful_for_ordering
 *		Count the number of pathkeys that are useful for meeting the
 *		query's requested output ordering.
 *
 * Unlike merge pathkeys, this is an all-or-nothing affair: it does us
 * no good to order by just the first key(s) of the requested ordering.
 * So the result is always either 0 or list_length(root->query_pathkeys).
 */
int
pathkeys_useful_for_ordering(PlannerInfo *root, List *pathkeys)
{
	if (root->query_pathkeys == NIL)
		return 0;				/* no special ordering requested */

	if (pathkeys == NIL)
		return 0;				/* unordered path */

	if (pathkeys_contained_in(root->query_pathkeys, pathkeys))
	{
		/* It's useful ... or at least the first N keys are */
		return list_length(root->query_pathkeys);
	}

	return 0;					/* path ordering not useful */
}

/*
 * truncate_useless_pathkeys
 *		Shorten the given pathkey list to just the useful pathkeys.
 */
List *
truncate_useless_pathkeys(PlannerInfo *root,
						  RelOptInfo *rel,
						  List *pathkeys)
{
	int			nuseful;
	int			nuseful2;

	nuseful = pathkeys_useful_for_merging(root, rel, pathkeys);
	nuseful2 = pathkeys_useful_for_ordering(root, pathkeys);
	if (nuseful2 > nuseful)
		nuseful = nuseful2;

	/*
	 * Note: not safe to modify input list destructively, but we can avoid
	 * copying the list if we're not actually going to change it
	 */
	if (nuseful == list_length(pathkeys))
		return pathkeys;
	else
		return list_truncate(list_copy(pathkeys), nuseful);
}
