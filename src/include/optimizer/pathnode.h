/*-------------------------------------------------------------------------
 *
 * pathnode.h
 *	  prototypes for pathnode.c, relnode.c.
 *
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2016, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/optimizer/pathnode.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PATHNODE_H
#define PATHNODE_H

#include "nodes/relation.h"
#include "cdb/cdbdef.h"                 /* CdbVisitOpt */


/*
 * prototypes for pathnode.c
 */

extern CdbVisitOpt pathnode_walk_node(Path *path,
			       CdbVisitOpt (*walker)(Path *path, void *context),
			       void *context);

extern int compare_path_costs(Path *path1, Path *path2,
				   CostSelector criterion);
extern int compare_fractional_path_costs(Path *path1, Path *path2,
							  double fraction);
extern void set_cheapest(RelOptInfo *parent_rel);
extern void add_path(RelOptInfo *parent_rel, Path *new_path);
extern void cdb_add_join_path(PlannerInfo *root, RelOptInfo *parent_rel, JoinType orig_jointype,
				  Relids required_outer, JoinPath *new_path);
extern Path *create_seqscan_path(PlannerInfo *root, RelOptInfo *rel,
					Relids required_outer, int parallel_workers);
extern ExternalPath *create_external_path(PlannerInfo *root, RelOptInfo *rel,
					Relids required_outer);
extern AppendOnlyPath *create_appendonly_path(PlannerInfo *root, RelOptInfo *rel,
					Relids required_outer);
extern AOCSPath *create_aocs_path(PlannerInfo *root, RelOptInfo *rel,
					Relids required_outer);
extern bool add_path_precheck(RelOptInfo *parent_rel,
				  Cost startup_cost, Cost total_cost,
				  List *pathkeys, Relids required_outer);
extern void add_partial_path(RelOptInfo *parent_rel, Path *new_path);
extern bool add_partial_path_precheck(RelOptInfo *parent_rel,
						  Cost total_cost, List *pathkeys);

extern Path *create_samplescan_path(PlannerInfo *root, RelOptInfo *rel,
					   Relids required_outer);
extern IndexPath *create_index_path(PlannerInfo *root,
				  IndexOptInfo *index,
				  List *indexclauses,
				  List *indexclausecols,
				  List *indexorderbys,
				  List *indexorderbycols,
				  List *pathkeys,
				  ScanDirection indexscandir,
				  bool indexonly,
				  Relids required_outer,
				  double loop_count);
extern BitmapHeapPath *create_bitmap_heap_path(PlannerInfo *root,
						RelOptInfo *rel,
						Path *bitmapqual,
						Relids required_outer,
						double loop_count);
extern BitmapAndPath *create_bitmap_and_path(PlannerInfo *root,
					   RelOptInfo *rel,
					   List *bitmapquals);
extern BitmapOrPath *create_bitmap_or_path(PlannerInfo *root,
					  RelOptInfo *rel,
					  List *bitmapquals);
extern TidPath *create_tidscan_path(PlannerInfo *root, RelOptInfo *rel,
					List *tidquals, Relids required_outer);
extern AppendPath *create_append_path(PlannerInfo *root, RelOptInfo *rel,
				   List *subpaths,
				   Relids required_outer, int parallel_workers);
extern MergeAppendPath *create_merge_append_path(PlannerInfo *root,
						 RelOptInfo *rel,
						 List *subpaths,
						 List *pathkeys,
						 Relids required_outer);
extern ResultPath *create_result_path(PlannerInfo *root, RelOptInfo *rel,
				   PathTarget *target, List *resconstantqual);
extern MaterialPath *create_material_path(PlannerInfo *root, RelOptInfo *rel, Path *subpath);
extern UniquePath *create_unique_path(PlannerInfo *root, RelOptInfo *rel,
				   Path *subpath, SpecialJoinInfo *sjinfo);
extern UniquePath *create_unique_rowid_path(PlannerInfo *root,
						 RelOptInfo *rel,
                         Path        *subpath,
                         Relids       distinct_relids,
						 Relids       required_outer);
extern GatherPath *create_gather_path(PlannerInfo *root,
				   RelOptInfo *rel, Path *subpath, PathTarget *target,
				   Relids required_outer, double *rows);
extern SubqueryScanPath *create_subqueryscan_path(PlannerInfo *root,
						 RelOptInfo *rel, Path *subpath,
						 List *pathkeys, CdbPathLocus locus, Relids required_outer);
extern Path *create_functionscan_path(PlannerInfo *root, RelOptInfo *rel,
						 RangeTblEntry *rte,
						 List *pathkeys, Relids required_outer);
extern TableFunctionScanPath *create_tablefunction_path(PlannerInfo *root,
						  RelOptInfo *rel, Path *subpath,
						  List *pathkeys, Relids required_outer);
extern Path *create_valuesscan_path(PlannerInfo *root, RelOptInfo *rel,
					   RangeTblEntry *rte,
					   Relids required_outer);
extern Path *create_ctescan_path(PlannerInfo *root, RelOptInfo *rel,
					Path *subpath, CdbPathLocus locus,
					List *pathkeys,
					Relids required_outer);
extern Path *create_worktablescan_path(PlannerInfo *root, RelOptInfo *rel,
						  CdbPathLocus ctelocus,
						  Relids required_outer);
extern ForeignPath *create_foreignscan_path(PlannerInfo *root, RelOptInfo *rel,
						PathTarget *target,
						double rows, Cost startup_cost, Cost total_cost,
						List *pathkeys,
						Relids required_outer,
						Path *fdw_outerpath,
						List *fdw_private);

extern Relids calc_nestloop_required_outer(Path *outer_path, Path *inner_path);
extern Relids calc_non_nestloop_required_outer(Path *outer_path, Path *inner_path);

extern bool path_contains_inner_index(Path *path);
extern NestPath *create_nestloop_path(PlannerInfo *root,
					 RelOptInfo *joinrel,
					 JoinType jointype,
					 JoinCostWorkspace *workspace,
					 SpecialJoinInfo *sjinfo,
					 SemiAntiJoinFactors *semifactors,
					 Path *outer_path,
					 Path *inner_path,
					 List *restrict_clauses,
					 List *redistribution_clauses,    /*CDB*/
					 List *pathkeys,
					 Relids required_outer);

extern MergePath *create_mergejoin_path(PlannerInfo *root,
					  RelOptInfo *joinrel,
					  JoinType jointype,
					  JoinCostWorkspace *workspace,
					  SpecialJoinInfo *sjinfo,
					  Path *outer_path,
					  Path *inner_path,
					  List *restrict_clauses,
					  List *pathkeys,
					  Relids required_outer,
					  List *mergeclauses,
                      List *redistribution_clauses,    /*CDB*/
					  List *outersortkeys,
					  List *innersortkeys);

extern HashPath *create_hashjoin_path(PlannerInfo *root,
					 RelOptInfo *joinrel,
					 JoinType jointype,
					 JoinCostWorkspace *workspace,
					 SpecialJoinInfo *sjinfo,
					 SemiAntiJoinFactors *semifactors,
					 Path *outer_path,
					 Path *inner_path,
					 List *restrict_clauses,
					 Relids required_outer,
                     List *redistribution_clauses,    /*CDB*/
					 List *hashclauses);

extern ProjectionPath *create_projection_path(PlannerInfo *root,
					   RelOptInfo *rel,
					   Path *subpath,
					   PathTarget *target);
extern ProjectionPath *create_projection_path_with_quals(PlannerInfo *root,
					   RelOptInfo *rel,
					   Path *subpath,
					   PathTarget *target,
					   List *restrict_clauses);
extern Path *apply_projection_to_path(PlannerInfo *root,
						 RelOptInfo *rel,
						 Path *path,
						 PathTarget *target);
extern SortPath *create_sort_path(PlannerInfo *root,
				 RelOptInfo *rel,
				 Path *subpath,
				 List *pathkeys,
				 double limit_tuples);
extern GroupPath *create_group_path(PlannerInfo *root,
				  RelOptInfo *rel,
				  Path *subpath,
				  PathTarget *target,
				  List *groupClause,
				  List *qual,
				  double numGroups);
extern UpperUniquePath *create_upper_unique_path(PlannerInfo *root,
						 RelOptInfo *rel,
						 Path *subpath,
						 int numCols,
						 double numGroups);
struct HashAggTableSizes; /* defined in execHHashagg.h */
extern AggPath *create_agg_path(PlannerInfo *root,
				RelOptInfo *rel,
				Path *subpath,
				PathTarget *target,
				AggStrategy aggstrategy,
				AggSplit aggsplit,
				bool streaming,
				List *groupClause,
				List *qual,
				const AggClauseCosts *aggcosts,
				double numGroups,
				struct HashAggTableSizes *hash_info);
extern GroupingSetsPath *create_groupingsets_path(PlannerInfo *root,
						 RelOptInfo *rel,
						 Path *subpath,
						 PathTarget *target,
						 AggSplit aggsplit,
						 List *having_qual,
						 List *rollup_lists,
						 List *rollup_groupclauses,
						 const AggClauseCosts *agg_costs,
						 double numGroups);
extern MinMaxAggPath *create_minmaxagg_path(PlannerInfo *root,
					  RelOptInfo *rel,
					  PathTarget *target,
					  List *mmaggregates,
					  List *quals);
extern WindowAggPath *create_windowagg_path(PlannerInfo *root,
					  RelOptInfo *rel,
					  Path *subpath,
					  PathTarget *target,
					  List *windowFuncs,
					  WindowClause *winclause,
					  List *winpathkeys);
extern SetOpPath *create_setop_path(PlannerInfo *root,
				  RelOptInfo *rel,
				  Path *subpath,
				  SetOpCmd cmd,
				  SetOpStrategy strategy,
				  List *distinctList,
				  AttrNumber flagColIdx,
				  int firstFlag,
				  double numGroups,
				  double outputRows);
extern RecursiveUnionPath *create_recursiveunion_path(PlannerInfo *root,
						   RelOptInfo *rel,
						   Path *leftpath,
						   Path *rightpath,
						   PathTarget *target,
						   List *distinctList,
						   int wtParam,
						   double numGroups);
extern LockRowsPath *create_lockrows_path(PlannerInfo *root, RelOptInfo *rel,
					 Path *subpath, List *rowMarks, int epqParam);
extern ModifyTablePath *create_modifytable_path(PlannerInfo *root,
						RelOptInfo *rel,
						CmdType operation, bool canSetTag,
						Index nominalRelation,
						List *resultRelations, List *subpaths,
						List *subroots,
						List *withCheckOptionLists, List *returningLists,
						List *is_split_updates,
						List *rowMarks, OnConflictExpr *onconflict,
						int epqParam);
extern LimitPath *create_limit_path(PlannerInfo *root, RelOptInfo *rel,
				  Path *subpath,
				  Node *limitOffset, Node *limitCount,
				  int64 offset_est, int64 count_est);

extern Path *reparameterize_path(PlannerInfo *root, Path *path,
					Relids required_outer,
					double loop_count);

/*
 * prototypes for relnode.c
 */
extern void setup_simple_rel_arrays(PlannerInfo *root);
extern RelOptInfo *build_simple_rel(PlannerInfo *root, int relid,
				 RelOptKind reloptkind);
extern RelOptInfo *find_base_rel(PlannerInfo *root, int relid);
extern RelOptInfo *find_join_rel(PlannerInfo *root, Relids relids);
extern RelOptInfo *build_join_rel(PlannerInfo *root,
			   Relids joinrelids,
			   RelOptInfo *outer_rel,
			   RelOptInfo *inner_rel,
			   SpecialJoinInfo *sjinfo,
			   List **restrictlist_ptr);
extern Relids min_join_parameterization(PlannerInfo *root,
						  Relids joinrelids,
						  RelOptInfo *outer_rel,
						  RelOptInfo *inner_rel);
extern RelOptInfo *build_empty_join_rel(PlannerInfo *root);
extern void build_joinrel_tlist(PlannerInfo *root, RelOptInfo *joinrel, List *input_tlist);

extern Var *cdb_define_pseudo_column(PlannerInfo   *root,
                         RelOptInfo    *rel,
                         const char    *colname,
                         Expr          *defexpr,
                         int32          width);

extern CdbRelColumnInfo *cdb_find_pseudo_column(PlannerInfo *root, Var *var);

extern CdbRelColumnInfo *cdb_rte_find_pseudo_column(RangeTblEntry *rte, AttrNumber attno);

extern RelOptInfo *fetch_upper_rel(PlannerInfo *root, UpperRelationKind kind,
				Relids relids);
extern AppendRelInfo *find_childrel_appendrelinfo(PlannerInfo *root,
							RelOptInfo *rel);
extern RelOptInfo *find_childrel_top_parent(PlannerInfo *root, RelOptInfo *rel);
extern Relids find_childrel_parents(PlannerInfo *root, RelOptInfo *rel);
extern ParamPathInfo *get_baserel_parampathinfo(PlannerInfo *root,
						  RelOptInfo *baserel,
						  Relids required_outer);
extern ParamPathInfo *get_joinrel_parampathinfo(PlannerInfo *root,
						  RelOptInfo *joinrel,
						  Path *outer_path,
						  Path *inner_path,
						  SpecialJoinInfo *sjinfo,
						  Relids required_outer,
						  List **restrict_clauses);
extern ParamPathInfo *get_appendrel_parampathinfo(RelOptInfo *appendrel,
							Relids required_outer);

#endif   /* PATHNODE_H */
