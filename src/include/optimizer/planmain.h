/*-------------------------------------------------------------------------
 *
 * planmain.h
 *	  prototypes for various files in optimizer/plan
 *
 *
 * Portions Copyright (c) 2005-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/optimizer/planmain.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PLANMAIN_H
#define PLANMAIN_H

#include "nodes/pathnodes.h"
#include "nodes/plannodes.h"

/* GUC parameters */
#define DEFAULT_CURSOR_TUPLE_FRACTION 1.0 /* assume all rows will be fetched */
extern double cursor_tuple_fraction;

/* query_planner callback to compute query_pathkeys */
typedef void (*query_pathkeys_callback) (PlannerInfo *root, void *extra);

/*
 * prototypes for plan/planmain.c
 */
extern RelOptInfo *query_planner(PlannerInfo *root,
								 query_pathkeys_callback qp_callback, void *qp_extra);

/*
 * prototypes for plan/planagg.c
 */
extern void preprocess_minmax_aggregates(PlannerInfo *root);

/*
 * prototype for plan/plangroupexp.c
 */
extern Plan *make_distinctaggs_for_rollup(PlannerInfo *root, bool is_agg,
										  List *tlist, bool twostage, List *sub_tlist,
										  List *qual, AggStrategy aggstrategy,
										  int numGroupCols, AttrNumber *grpColIdx, Oid *grpOperators,
										  double numGroups, int *rollup_gs_times,
										  int numAggs, int transSpace,
										  double *p_dNumGroups,
										  List **p_current_pathkeys,
										  Plan *lefttree);

extern Plan *add_repeat_node(Plan *result_plan, int repeat_count, uint64 grouping);
extern bool contain_group_id(Node *node);

/*
 * prototypes for plan/createplan.c
 */
extern Plan *create_plan(PlannerInfo *root, Path *best_path, PlanSlice *curSlice);
extern Plan *create_plan_recurse(PlannerInfo *root, Path *best_path,
					int flags);
extern ForeignScan *make_foreignscan(List *qptlist, List *qpqual,
									 Index scanrelid, List *fdw_exprs, List *fdw_private,
									 List *fdw_scan_tlist, List *fdw_recheck_quals,
									 Plan *outer_plan);
extern Plan *change_plan_targetlist(Plan *subplan, List *tlist,
									bool tlist_parallel_safe);
extern Plan *materialize_finished_plan(PlannerInfo *root, Plan *subplan);
extern bool is_projection_capable_path(Path *path);
extern bool is_projection_capable_plan(Plan *plan);

extern List *reconstruct_group_clause(List *orig_groupClause, List *tlist,
						 AttrNumber *grpColIdx, int numcols);

extern Motion *make_motion(PlannerInfo *root, Plan *lefttree,
			int numSortCols, AttrNumber *sortColIdx,
			Oid *sortOperators, Oid *collations, bool *nullsFirst);

extern Material *make_material(Plan *lefttree);
extern Result *make_result(List *tlist, Node *resconstantqual, Plan *subplan);
extern Plan *add_sort_cost(PlannerInfo *root, Plan *input, 
						   double limit_tuples);
extern Plan *plan_pushdown_tlist(PlannerInfo *root, Plan *plan, List *tlist);      /*CDB*/

/* External use of these functions is deprecated: */
extern Sort *make_sort_from_pathkeys(Plan *lefttree, List *pathkeys, Relids relids);
extern Sort *make_sort_from_sortclauses(List *sortcls,
						   Plan *lefttree);
extern Agg *make_agg(List *tlist, List *qual,
					 AggStrategy aggstrategy, AggSplit aggsplit,
					 bool streaming,
					 int numGroupCols, AttrNumber *grpColIdx, Oid *grpOperators, Oid *grpCollations,
					 List *groupingSets, List *chain,
					 double dNumGroups, Plan *lefttree);
extern Limit *make_limit(Plan *lefttree, Node *limitOffset, Node *limitCount);
extern TupleSplit *make_tup_split(List *tlist, List *dqa_info_lst, int numGroupCols,
								  AttrNumber *grpColIdx, Plan *lefttree);

/*
 * prototypes for plan/initsplan.c
 */
extern int	from_collapse_limit;
extern int	join_collapse_limit;

extern void add_base_rels_to_query(PlannerInfo *root, Node *jtnode);
extern void add_other_rels_to_query(PlannerInfo *root);
extern void build_base_rel_tlists(PlannerInfo *root, List *final_tlist);
extern void add_vars_to_targetlist(PlannerInfo *root, List *vars,
								   Relids where_needed, bool create_new_ph);
extern void add_vars_to_targetlist_x(PlannerInfo *root, List *vars,
									 Relids where_needed, bool create_new_ph,
									 bool force);
extern void find_lateral_references(PlannerInfo *root);
extern void create_lateral_join_info(PlannerInfo *root);
extern List *deconstruct_jointree(PlannerInfo *root);
extern void distribute_restrictinfo_to_rels(PlannerInfo *root,
											RestrictInfo *restrictinfo);
extern void process_implied_equality(PlannerInfo *root,
									 Oid opno,
									 Oid collation,
									 Expr *item1,
									 Expr *item2,
									 Relids qualscope,
									 Relids nullable_relids,
									 Index security_level,
									 bool below_outer_join,
									 bool both_const);
extern RestrictInfo *build_implied_join_equality(Oid opno,
												 Oid collation,
												 Expr *item1,
												 Expr *item2,
												 Relids qualscope,
												 Relids nullable_relids,
												 Index security_level);
extern void match_foreign_keys_to_quals(PlannerInfo *root);

extern void check_mergejoinable(RestrictInfo *restrictinfo);
extern void check_hashjoinable(RestrictInfo *restrictinfo);
extern bool has_redistributable_clause(RestrictInfo *restrictinfo);

/*
 * prototypes for plan/analyzejoins.c
 */
extern List *remove_useless_joins(PlannerInfo *root, List *joinlist);
extern void reduce_unique_semijoins(PlannerInfo *root);
extern bool query_supports_distinctness(Query *query);
extern bool query_is_distinct_for(Query *query, List *colnos, List *opids);
extern bool innerrel_is_unique(PlannerInfo *root,
							   Relids joinrelids, Relids outerrelids, RelOptInfo *innerrel,
							   JoinType jointype, List *restrictlist, bool force_cache);

/*
 * prototypes for plan/setrefs.c
 */
extern Plan *set_plan_references(PlannerInfo *root, Plan *plan);
extern void record_plan_function_dependency(PlannerInfo *root, Oid funcid);
extern void record_plan_type_dependency(PlannerInfo *root, Oid typid);
extern bool extract_query_dependencies_walker(Node *node, PlannerInfo *root);

extern void cdb_extract_plan_dependencies(PlannerInfo *root, Plan *plan);

extern void add_proc_oids_for_dump(Oid funcid);

#endif							/* PLANMAIN_H */
