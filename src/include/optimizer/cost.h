/*-------------------------------------------------------------------------
 *
 * cost.h
 *	  prototypes for costsize.c and clausesel.c.
 *
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/optimizer/cost.h,v 1.97 2009/06/11 14:49:11 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef COST_H
#define COST_H

#include "nodes/plannodes.h"
#include "nodes/relation.h"


/* defaults for costsize.c's Cost parameters */
/* NB: cost-estimation code should use the variables, not these constants! */
/* If you change these, update backend/utils/misc/postgresql.sample.conf */
#define DEFAULT_SEQ_PAGE_COST  1.0
#define DEFAULT_RANDOM_PAGE_COST  100.0
#define DEFAULT_CPU_TUPLE_COST	0.01
#define DEFAULT_CPU_INDEX_TUPLE_COST 0.005
#define DEFAULT_CPU_OPERATOR_COST  0.0025

#define DEFAULT_EFFECTIVE_CACHE_SIZE  16384		/* measured in pages */

typedef enum
{
	CONSTRAINT_EXCLUSION_OFF,	/* do not use c_e */
	CONSTRAINT_EXCLUSION_ON,	/* apply c_e to all rels */
	CONSTRAINT_EXCLUSION_PARTITION		/* apply c_e to otherrels only */
} ConstraintExclusionType;


/*
 * clamp_row_est
 *		Force a row-count estimate to a sane value.
 */
static inline double
clamp_row_est(double nrows)
{
	/*
	 * Force estimate to be at least one row, to make explain output look
	 * better and to avoid possible divide-by-zero when interpolating costs.
     * CDB: Don't round to integer.
	 */
    return (nrows < 1.0) ? 1.0 : nrows;
}


/*
 * prototypes for costsize.c
 *	  routines to compute costs and sizes
 */

/* parameter variables and flags */
extern PGDLLIMPORT double seq_page_cost;
extern PGDLLIMPORT double random_page_cost;
extern PGDLLIMPORT double cpu_tuple_cost;
extern PGDLLIMPORT double cpu_index_tuple_cost;
extern PGDLLIMPORT double cpu_operator_cost;
extern PGDLLIMPORT int effective_cache_size;
extern Cost disable_cost;
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
extern int	constraint_exclusion;

extern bool gp_enable_hashjoin_size_heuristic;          /*CDB*/
extern bool gp_enable_predicate_propagation;

extern double index_pages_fetched(double tuples_fetched, BlockNumber pages,
					double index_pages, PlannerInfo *root);
extern void cost_seqscan(Path *path, PlannerInfo *root, RelOptInfo *baserel);
extern void cost_externalscan(ExternalPath *path, PlannerInfo *root, RelOptInfo *baserel);
extern void cost_appendonlyscan(AppendOnlyPath *path, PlannerInfo *root, RelOptInfo *baserel);
extern void cost_aocsscan(AOCSPath *path, PlannerInfo *root, RelOptInfo *baserel);
extern void cost_index(IndexPath *path, PlannerInfo *root, IndexOptInfo *index,
		   List *indexQuals, RelOptInfo *outer_rel);
extern void cost_bitmap_heap_scan(Path *path, PlannerInfo *root, RelOptInfo *baserel,
					  Path *bitmapqual, RelOptInfo *outer_rel);
extern void cost_bitmap_appendonly_scan(Path *path, PlannerInfo *root, RelOptInfo *baserel,
					  Path *bitmapqual, RelOptInfo *outer_rel);
extern void cost_bitmap_table_scan(Path *path, PlannerInfo *root, RelOptInfo *baserel,
					  Path *bitmapqual, RelOptInfo *outer_rel);
extern void cost_bitmap_and_node(BitmapAndPath *path, PlannerInfo *root);
extern void cost_bitmap_or_node(BitmapOrPath *path, PlannerInfo *root);
extern void cost_bitmap_tree_node(Path *path, Cost *cost, Selectivity *selec);
extern void cost_tidscan(Path *path, PlannerInfo *root,
			 RelOptInfo *baserel, List *tidquals);
extern void cost_subqueryscan(Path *path, RelOptInfo *baserel);
extern void cost_functionscan(Path *path, PlannerInfo *root,
				  RelOptInfo *baserel);
extern void cost_tablefunction(Path *path, PlannerInfo *root,
							   RelOptInfo *baserel);
extern void cost_valuesscan(Path *path, PlannerInfo *root,
				RelOptInfo *baserel);
extern void cost_ctescan(Path *path, PlannerInfo *root, RelOptInfo *baserel);
extern void cost_recursive_union(Plan *runion, Plan *nrterm, Plan *rterm);
extern void cost_sort(Path *path, PlannerInfo *root,
		  List *pathkeys, Cost input_cost, double tuples, int width,
		  double limit_tuples);
extern bool sort_exceeds_work_mem(Sort *sort);
extern void cost_material(Path *path, PlannerInfo *root,
			  Cost input_cost, double tuples, int width);
extern void cost_agg(Path *path, PlannerInfo *root,
					 AggStrategy aggstrategy, int numAggs,
					 int numGroupCols, double numGroups,
					 Cost input_startup_cost, Cost input_total_cost,
					 double input_tuples, double input_width, double hash_batches,
					 double hashentry_width, bool hash_streaming);
extern void cost_windowagg(Path *path, PlannerInfo *root,
			   int numWindowFuncs, int numPartCols, int numOrderCols,
			   Cost input_startup_cost, Cost input_total_cost,
			   double input_tuples);
extern void cost_group(Path *path, PlannerInfo *root,
		   int numGroupCols, double numGroups,
		   Cost input_startup_cost, Cost input_total_cost,
		   double input_tuples);
extern void cost_shareinputscan(Path *path, PlannerInfo *root, Cost sharecost, double ntuples, int width);
extern void cost_nestloop(NestPath *path, PlannerInfo *root,
			  SpecialJoinInfo *sjinfo);
extern void cost_mergejoin(MergePath *path, PlannerInfo *root,
			   SpecialJoinInfo *sjinfo);
extern void cost_hashjoin(HashPath *path, PlannerInfo *root,
			  SpecialJoinInfo *sjinfo);
extern void cost_subplan(PlannerInfo *root, SubPlan *subplan, Plan *plan);
extern void cost_qual_eval(QualCost *cost, List *quals, PlannerInfo *root);
extern void cost_qual_eval_node(QualCost *cost, Node *qual, PlannerInfo *root);
extern void set_baserel_size_estimates(PlannerInfo *root, RelOptInfo *rel);
extern void set_joinrel_size_estimates(PlannerInfo *root, RelOptInfo *rel,
						   RelOptInfo *outer_rel,
						   RelOptInfo *inner_rel,
						   SpecialJoinInfo *sjinfo,
						   List *restrictlist);
extern void set_function_size_estimates(PlannerInfo *root, RelOptInfo *rel);
extern void set_table_function_size_estimates(PlannerInfo *root, RelOptInfo *rel);
extern void set_rel_width(PlannerInfo *root, RelOptInfo *rel);
extern void set_values_size_estimates(PlannerInfo *root, RelOptInfo *rel);
extern void set_cte_size_estimates(PlannerInfo *root, RelOptInfo *rel,
					   Plan *cteplan);

/* Additional costsize.c prototypes for CDB incremental cost functions. */
extern Cost incremental_hashjoin_cost(double rows, 
									  int inner_width, int outer_width, 
									  List *hashclauses,
									  PlannerInfo *root);
extern Cost incremental_mergejoin_cost(double rows, List *mergeclauses, PlannerInfo *root);

/*
 * prototypes for clausesel.c
 *	  routines to compute clause selectivities
 */
extern Selectivity clauselist_selectivity(PlannerInfo *root,
					   List *clauses,
					   int varRelid,
					   JoinType jointype,
					   SpecialJoinInfo *sjinfo,
					   bool use_damping);
extern Selectivity clause_selectivity(PlannerInfo *root,
				   Node *clause,
				   int varRelid,
				   JoinType jointype,
				   SpecialJoinInfo *sjinfo,
				   bool use_damping);
extern int planner_segment_count(void);
extern double global_work_mem(PlannerInfo *root);

#endif   /* COST_H */
