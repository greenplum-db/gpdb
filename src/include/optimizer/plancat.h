/*-------------------------------------------------------------------------
 *
 * plancat.h
 *	  prototypes for plancat.c.
 *
 *
 * Portions Copyright (c) 2006-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/optimizer/plancat.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PLANCAT_H
#define PLANCAT_H

#include "nodes/pathnodes.h"
#include "utils/relcache.h"

/* Hook for plugins to get control in get_relation_info() */
typedef void (*get_relation_info_hook_type) (PlannerInfo *root,
											 Oid relationObjectId,
											 bool inhparent,
											 RelOptInfo *rel);
extern PGDLLIMPORT get_relation_info_hook_type get_relation_info_hook;


extern void get_relation_info(PlannerInfo *root, Oid relationObjectId,
							  bool inhparent, RelOptInfo *rel);

extern List *infer_arbiter_indexes(PlannerInfo *root);

extern void estimate_rel_size(Relation rel, int32 *attr_widths,
							  BlockNumber *pages, double *tuples, double *allvisfrac);

extern void cdb_estimate_rel_size(RelOptInfo   *relOptInfo,
							  Relation      rel,
							  int32        *attr_widths,
							  BlockNumber  *pages,
							  double       *tuples,
							  double       *allvisfrac);
extern double cdb_estimate_partitioned_numtuples(Relation rel);

extern int32 get_rel_data_width(Relation rel, int32 *attr_widths);
extern int32 get_relation_data_width(Oid relid, int32 *attr_widths);

extern bool relation_excluded_by_constraints(PlannerInfo *root,
											 RelOptInfo *rel, RangeTblEntry *rte);

extern List *build_physical_tlist(PlannerInfo *root, RelOptInfo *rel);

extern bool has_unique_index(RelOptInfo *rel, AttrNumber attno);

extern Selectivity restriction_selectivity(PlannerInfo *root,
										   Oid operatorid,
										   List *args,
										   Oid inputcollid,
										   int varRelid);

extern Selectivity join_selectivity(PlannerInfo *root,
									Oid operatorid,
									List *args,
									Oid inputcollid,
									JoinType jointype,
									SpecialJoinInfo *sjinfo);

extern Selectivity function_selectivity(PlannerInfo *root,
										Oid funcid,
										List *args,
										Oid inputcollid,
										bool is_join,
										int varRelid,
										JoinType jointype,
										SpecialJoinInfo *sjinfo);

extern void add_function_cost(PlannerInfo *root, Oid funcid, Node *node,
							  QualCost *cost);

extern double get_function_rows(PlannerInfo *root, Oid funcid, Node *node);

extern bool has_row_triggers(PlannerInfo *root, Index rti, CmdType event);

extern bool has_stored_generated_columns(PlannerInfo *root, Index rti);

#define DEFAULT_EXTERNAL_TABLE_PAGES 1000
#define DEFAULT_EXTERNAL_TABLE_TUPLES 1000000

#define DEFAULT_INTERNAL_TABLE_PAGES 100

#endif							/* PLANCAT_H */
