/*-------------------------------------------------------------------------
 *
 * paths.h
 *	  prototypes for various files in optimizer/path
 *
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/optimizer/paths.h,v 1.93.2.2 2007/05/22 01:40:42 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef PATHS_H
#define PATHS_H

#include "nodes/relation.h"


/*
 * allpaths.c
 */
extern bool enable_seqscan;
extern bool enable_indexscan;
extern bool enable_bitmapscan;
extern bool enable_tidscan;
extern bool enable_sort;
extern bool enable_hashagg;
extern bool enable_groupagg;
extern bool enable_nestloop;
extern bool enable_mergejoin;
extern bool enable_hashjoin;
extern bool gp_enable_hashjoin_size_heuristic;          /*CDB*/
extern bool gp_enable_fallback_plan;
extern bool gp_enable_predicate_propagation;

extern RelOptInfo *make_one_rel(PlannerInfo *root, List *joinlist);

#ifdef OPTIMIZER_DEBUG
extern void debug_print_rel(PlannerInfo *root, RelOptInfo *rel);
#endif

/*
 * indxpath.c
 *	  routines to generate index paths
 */
typedef enum
{
	/* Whether to use ScalarArrayOpExpr to build index qualifications */
	SAOP_FORBID,				/* Do not use ScalarArrayOpExpr */
	SAOP_ALLOW,					/* OK to use ScalarArrayOpExpr */
	SAOP_REQUIRE				/* Require ScalarArrayOpExpr */
} SaOpControl;

extern void create_index_paths(PlannerInfo *root, RelOptInfo *rel,
							   char relstorage,
                               List **pindexpathlist, List **pbitmappathlist);
extern List *generate_bitmap_or_paths(PlannerInfo *root, RelOptInfo *rel,
						 List *clauses, List *outer_clauses,
						 RelOptInfo *outer_rel);
extern void best_inner_indexscan(PlannerInfo *root, RelOptInfo *rel,
					 RelOptInfo *outer_rel, JoinType jointype,
					 Path **cheapest_startup, Path **cheapest_total);
extern List *group_clauses_by_indexkey(IndexOptInfo *index,
						  List *clauses, List *outer_clauses,
						  Relids outer_relids,
						  SaOpControl saop_control,
						  bool *found_clause);
extern bool match_index_to_operand(Node *operand, int indexcol,
					   IndexOptInfo *index);
extern List *expand_indexqual_conditions(IndexOptInfo *index,
							List *clausegroups);
extern void check_partial_indexes(PlannerInfo *root, RelOptInfo *rel);
extern List *flatten_clausegroups_list(List *clausegroups);

/*
 * orindxpath.c
 *	  additional routines for indexable OR clauses
 */
extern bool create_or_index_quals(PlannerInfo *root, RelOptInfo *rel);

/*
 * tidpath.h
 *	  routines to generate tid paths
 */
extern void create_tidscan_paths(PlannerInfo *root, RelOptInfo *rel, List** ppathlist);

/*
 * joinpath.c
 *	   routines to create join paths
 */
extern void add_paths_to_joinrel(PlannerInfo *root, RelOptInfo *joinrel,
					 RelOptInfo *outerrel,
					 RelOptInfo *innerrel,
					 JoinType jointype,
					 List *restrictlist);
List *
hashclauses_for_join(List *restrictlist,
					 RelOptInfo *outerrel,
					 RelOptInfo *innerrel,
					 JoinType jointype);
List *select_mergejoin_clauses(RelOptInfo *joinrel,
						 RelOptInfo *outerrel,
						 RelOptInfo *innerrel,
						 List *restrictlist,
						 JoinType jointype);
extern void build_mergejoin_strat_lists(List *mergeclauses,
							List **mergefamilies, List **mergestrategies);

/*
 * joinrels.c
 *	  routines to determine which relations to join
 */
extern List *make_rels_by_joins(PlannerInfo *root, int level, List **joinrels);
extern RelOptInfo *make_join_rel(PlannerInfo *root,
			  RelOptInfo *rel1, RelOptInfo *rel2);
extern bool have_join_order_restriction(PlannerInfo *root,
							RelOptInfo *rel1, RelOptInfo *rel2);

/*
 * pathkeys.c
 *	  utilities for matching and building path keys
 */
typedef enum
{
	PATHKEYS_EQUAL,				/* pathkeys are identical */
	PATHKEYS_BETTER1,			/* pathkey 1 is a superset of pathkey 2 */
	PATHKEYS_BETTER2,			/* vice versa */
	PATHKEYS_DIFFERENT			/* neither pathkey includes the other */
} PathKeysComparison;

extern void add_equijoined_keys(PlannerInfo *root, RestrictInfo *restrictinfo);
extern void add_equijoined_keys_to_list(List **ptrToList,
            RestrictInfo *restrictinfo);

extern bool exprs_known_equal(PlannerInfo *root, Node *item1, Node *item2);
extern void generate_implied_equalities(PlannerInfo *root);


typedef struct
{
    Node *replaceThis;
    Node *withThis;
    int numReplacementsDone;
} ReplaceExpressionMutatorReplacement;

/* context is ReplaceExpressionMutatorReplacement pointer */
extern Node * replace_expression_mutator(Node *node, void *context);
extern void generate_implied_quals(PlannerInfo *root);

extern List *canonicalize_pathkeys(PlannerInfo *root, List *pathkeys);
extern PathKeysComparison compare_pathkeys(List *keys1, List *keys2);
extern bool pathkeys_contained_in(List *keys1, List *keys2);
extern Path *get_cheapest_path_for_pathkeys(List *paths, List *pathkeys,
							   CostSelector cost_criterion);
extern Path *get_cheapest_fractional_path_for_pathkeys(List *paths,
										  List *pathkeys,
										  double fraction);
extern List *build_index_pathkeys(PlannerInfo *root, IndexOptInfo *index,
					 ScanDirection scandir, bool canonical);

Var *
find_indexkey_var(PlannerInfo *root, RelOptInfo *rel, AttrNumber varattno);

extern List *convert_subquery_pathkeys(PlannerInfo *root, RelOptInfo *rel,
						  List *subquery_pathkeys);
extern List *build_join_pathkeys(PlannerInfo *root,
					RelOptInfo *joinrel,
					JoinType jointype,
					List *outer_pathkeys);

extern PathKeyItem*
cdb_make_pathkey_for_expr_non_canonical(PlannerInfo    *root,
					      Node     *expr,
                          List     *eqopname);

List *
cdb_make_pathkey_for_expr(PlannerInfo  *root,
                          Node     *expr,
                          List     *eqopname);
List *
cdb_pull_up_pathkey(PlannerInfo    *root,
                    List           *pathkey,
                    Relids          relids,
                    List           *targetlist,
                    List           *newvarlist,
                    Index           newrelid);

extern List *make_pathkeys_for_sortclauses(List *sortclauses,
							  List *tlist);
extern List *make_pathkeys_for_groupclause(List *groupclause,
										   List *tlist);
extern void cache_mergeclause_pathkeys(PlannerInfo *root,
						   RestrictInfo *restrictinfo);
extern List *find_mergeclauses_for_pathkeys(PlannerInfo *root,
							   List *pathkeys,
							   List *restrictinfos);
extern List *make_pathkeys_for_mergeclauses(PlannerInfo *root,
							   List *mergeclauses,
							   RelOptInfo *rel);
extern int pathkeys_useful_for_merging(PlannerInfo *root,
							RelOptInfo *rel,
							List *pathkeys);
extern int	pathkeys_useful_for_ordering(PlannerInfo *root, List *pathkeys);
extern List *truncate_useless_pathkeys(PlannerInfo *root,
						  RelOptInfo *rel,
						  List *pathkeys);
extern List *construct_equivalencekey_list(List *equi_key_list,
										   int *resno_map,
										   List *orig_tlist,
										   List *new_tlist);
extern List *remove_pathkey_item(List *equi_key_list,
								 Node *key);

#endif   /* PATHS_H */
