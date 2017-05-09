/*-------------------------------------------------------------------------
 *
 * pathnode.h
 *	  prototypes for pathnode.c, relnode.c.
 *
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/optimizer/pathnode.h,v 1.79 2008/10/04 21:56:55 tgl Exp $
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
extern void set_cheapest(PlannerInfo *root, RelOptInfo *parent_rel);    /*CDB*/
extern void add_path(PlannerInfo *root, RelOptInfo *parent_rel, Path *new_path);

extern Path *create_seqscan_path(PlannerInfo *root, RelOptInfo *rel);
extern ExternalPath *create_external_path(PlannerInfo *root, RelOptInfo *rel);
extern AppendOnlyPath *create_appendonly_path(PlannerInfo *root, RelOptInfo *rel);
extern AOCSPath *create_aocs_path(PlannerInfo *root, RelOptInfo *rel);
extern IndexPath *create_index_path(PlannerInfo *root,
				  IndexOptInfo *index,
				  List *clause_groups,
				  List *pathkeys,
				  ScanDirection indexscandir,
				  RelOptInfo *outer_rel);
extern BitmapHeapPath *create_bitmap_heap_path(PlannerInfo *root,
						RelOptInfo *rel,
						Path *bitmapqual,
						RelOptInfo *outer_rel);
extern BitmapAppendOnlyPath *create_bitmap_appendonly_path(PlannerInfo *root,
														   RelOptInfo *rel,
														   Path *bitmapqual,
														   RelOptInfo *outer_rel,
														   bool isAORow);
extern BitmapTableScanPath *create_bitmap_table_scan_path(PlannerInfo *root,
						RelOptInfo *rel,
						Path *bitmapqual,
						RelOptInfo *outer_rel);
extern BitmapAndPath *create_bitmap_and_path(PlannerInfo *root,
					   RelOptInfo *rel,
					   List *bitmapquals);
extern BitmapOrPath *create_bitmap_or_path(PlannerInfo *root,
					  RelOptInfo *rel,
					  List *bitmapquals);
extern TidPath *create_tidscan_path(PlannerInfo *root, RelOptInfo *rel,
					List *tidquals);
extern AppendPath *create_append_path(PlannerInfo *root, RelOptInfo *rel, List *subpaths);
extern ResultPath *create_result_path(List *quals);
extern MaterialPath *create_material_path(PlannerInfo *root, RelOptInfo *rel, Path *subpath);
extern UniquePath *create_unique_path(PlannerInfo *root,
		Path        *subpath,
		List        *distinct_on_exprs,
		List		   *distinct_on_operators,
		Relids       distinct_on_rowid_relids);
extern Path *create_subqueryscan_path(PlannerInfo *root, RelOptInfo *rel, List *pathkeys);
extern Path *create_functionscan_path(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
extern Path *create_tablefunction_path(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
extern Path *create_valuesscan_path(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);

extern Path *create_ctescan_path(PlannerInfo *root, RelOptInfo *rel, List *pathkeys);
extern Path *create_worktablescan_path(PlannerInfo *root, RelOptInfo *rel);

extern bool path_contains_inner_index(Path *path);
extern NestPath *create_nestloop_path(PlannerInfo *root,
					 RelOptInfo *joinrel,
					 JoinType jointype,
					 SpecialJoinInfo *sjinfo,
					 Path *outer_path,
					 Path *inner_path,
					 List *restrict_clauses,
					 List *mergeclause_list,    /*CDB*/
					 List *pathkeys);

extern MergePath *create_mergejoin_path(PlannerInfo *root,
					  RelOptInfo *joinrel,
					  JoinType jointype,
					  SpecialJoinInfo *sjinfo,
					  Path *outer_path,
					  Path *inner_path,
					  List *restrict_clauses,
					  List *pathkeys,
					  List *mergeclauses,
                      List *allmergeclauses,    /*CDB*/
					  List *outersortkeys,
					  List *innersortkeys);

extern HashPath *create_hashjoin_path(PlannerInfo *root,
					 RelOptInfo *joinrel,
					 JoinType jointype,
					 SpecialJoinInfo *sjinfo,
					 Path *outer_path,
					 Path *inner_path,
					 List *restrict_clauses,
                     List *mergeclause_list,    /*CDB*/
					 List *hashclauses);

/*
 * prototypes for relnode.c
 */
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
extern void build_joinrel_tlist(PlannerInfo *root, RelOptInfo *joinrel, List *input_tlist);

extern List *build_relation_tlist(RelOptInfo *rel);

extern Var *cdb_define_pseudo_column(PlannerInfo   *root,
                         RelOptInfo    *rel,
                         const char    *colname,
                         Expr          *defexpr,
                         int32          width);

extern CdbRelColumnInfo *cdb_find_pseudo_column(PlannerInfo *root, Var *var);

extern CdbRelColumnInfo *cdb_rte_find_pseudo_column(RangeTblEntry *rte, AttrNumber attno);

extern CdbRelDedupInfo *cdb_make_rel_dedup_info(PlannerInfo *root, RelOptInfo *rel);

#endif   /* PATHNODE_H */
