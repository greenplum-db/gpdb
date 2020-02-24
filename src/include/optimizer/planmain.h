/*-------------------------------------------------------------------------
 *
 * planmain.h
 *	  prototypes for various files in optimizer/plan
 *
 *
 * Portions Copyright (c) 2005-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2016, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/optimizer/planmain.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PLANMAIN_H
#define PLANMAIN_H

#include "nodes/plannodes.h"
#include "nodes/relation.h"

/* possible values for force_parallel_mode */
typedef enum
{
	FORCE_PARALLEL_OFF,
	FORCE_PARALLEL_ON,
	FORCE_PARALLEL_REGRESS
}	ForceParallelMode;

/* GUC parameters */
#define DEFAULT_CURSOR_TUPLE_FRACTION 1.0 /* assume all rows will be fetched */
extern double cursor_tuple_fraction;
extern int	force_parallel_mode;

/* query_planner callback to compute query_pathkeys */
typedef void (*query_pathkeys_callback) (PlannerInfo *root, void *extra);

/*
 * prototypes for plan/planmain.c
 */
extern RelOptInfo *query_planner(PlannerInfo *root, List *tlist,
			  query_pathkeys_callback qp_callback, void *qp_extra);

/*
 * prototypes for plan/planagg.c
 */
extern void preprocess_minmax_aggregates(PlannerInfo *root, List *tlist);

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
extern SubqueryScan *make_subqueryscan(List *qptlist, List *qpqual,
				  Index scanrelid, Plan *subplan);
extern ForeignScan *make_foreignscan(List *qptlist, List *qpqual,
				 Index scanrelid, List *fdw_exprs, List *fdw_private,
				 List *fdw_scan_tlist, List *fdw_recheck_quals,
				 Plan *outer_plan);
extern List *reconstruct_group_clause(List *orig_groupClause, List *tlist,
						 AttrNumber *grpColIdx, int numcols);

extern Motion *make_motion(PlannerInfo *root, Plan *lefttree,
			int numSortCols, AttrNumber *sortColIdx,
			Oid *sortOperators, Oid *collations, bool *nullsFirst);

extern HashJoin *make_hashjoin(List *tlist,
			  List *joinclauses, List *otherclauses,
			  List *hashclauses, List *hashqualclauses,
			  Plan *lefttree, Plan *righttree,
			  JoinType jointype);
extern Hash *make_hash(Plan *lefttree,
		  Oid skewTable,
		  AttrNumber skewColumn,
		  bool skewInherit,
		  Oid skewColType,
		  int32 skewColTypmod);
extern NestLoop *make_nestloop(List *tlist,
			  List *joinclauses, List *otherclauses, List *nestParams,
			  Plan *lefttree, Plan *righttree,
			  JoinType jointype);
extern MergeJoin *make_mergejoin(List *tlist,
			   List *joinclauses, List *otherclauses,
			   List *mergeclauses,
			   Oid *mergefamilies,
			   Oid *mergecollations,
			   int *mergestrategies,
			   bool *mergenullsfirst,
			   Plan *lefttree, Plan *righttree,
			   JoinType jointype);
extern Material *make_material(Plan *lefttree);
extern Plan *materialize_finished_plan(PlannerInfo *root, Plan *subplan);
extern Result *make_result(List *tlist, Node *resconstantqual, Plan *subplan);
extern bool is_projection_capable_path(Path *path);
extern bool is_projection_capable_plan(Plan *plan);
extern Plan *add_sort_cost(PlannerInfo *root, Plan *input, 
						   double limit_tuples);
extern Plan *plan_pushdown_tlist(PlannerInfo *root, Plan *plan, List *tlist);      /*CDB*/

/* External use of these functions is deprecated: */
extern Sort *make_sort_from_pathkeys(Plan *lefttree, List *pathkeys);
extern Sort *make_sort_from_sortclauses(List *sortcls,
						   Plan *lefttree);
extern Sort *make_sort_from_groupcols(List *groupcls, AttrNumber *grpColIdx,
									  Plan *lefttree);
extern Agg *make_agg(List *tlist, List *qual,
		 AggStrategy aggstrategy, AggSplit aggsplit,
		 bool streaming,
		 int numGroupCols, AttrNumber *grpColIdx, Oid *grpOperators,
		 List *groupingSets, List *chain,
		 double dNumGroups, Plan *lefttree);
extern Limit *make_limit(Plan *lefttree, Node *limitOffset, Node *limitCount);
extern TupleSplit *make_tup_split(List *tlist,
								  int numDQAs, Bitmapset **dqas_ref_bms,
								  int numGroupCols, AttrNumber *grpColIdx,
								  Plan *lefttree);

/*
 * prototypes for plan/initsplan.c
 */
extern int	from_collapse_limit;
extern int	join_collapse_limit;

extern void add_base_rels_to_query(PlannerInfo *root, Node *jtnode);
extern void build_base_rel_tlists(PlannerInfo *root, List *final_tlist);
extern void add_vars_to_targetlist(PlannerInfo *root, List *vars,
					   Relids where_needed, bool create_new_ph);
extern void add_vars_to_targetlist_x(PlannerInfo *root, List *vars,
						 Relids where_needed, bool create_new_ph, bool force);
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
						 bool below_outer_join,
						 bool both_const);
extern RestrictInfo *build_implied_join_equality(Oid opno,
							Oid collation,
							Expr *item1,
							Expr *item2,
							Relids qualscope,
							Relids nullable_relids);
extern void match_foreign_keys_to_quals(PlannerInfo *root);

extern void check_mergejoinable(RestrictInfo *restrictinfo);
extern void check_hashjoinable(RestrictInfo *restrictinfo);
extern bool has_redistributable_clause(RestrictInfo *restrictinfo);

/*
 * prototypes for plan/analyzejoins.c
 */
extern List *remove_useless_joins(PlannerInfo *root, List *joinlist);
extern bool query_supports_distinctness(Query *query);
extern bool query_is_distinct_for(Query *query, List *colnos, List *opids);

/*
 * prototypes for plan/setrefs.c
 */
extern Plan *set_plan_references(PlannerInfo *root, Plan *plan);
extern void record_plan_function_dependency(PlannerInfo *root, Oid funcid);
extern void extract_query_dependencies(Node *query,
						   List **relationOids,
						   List **invalItems,
						   bool *hasRowSecurity);
extern void cdb_extract_plan_dependencies(PlannerInfo *root, Plan *plan);

extern void add_proc_oids_for_dump(Oid funcid);

#endif   /* PLANMAIN_H */
