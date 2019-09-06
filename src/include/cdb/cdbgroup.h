/*-------------------------------------------------------------------------
 *
 * cdbgroup.h
 *	  prototypes for cdbgroup.c.
 *
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbgroup.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBGROUP_H
#define CDBGROUP_H

#include "nodes/params.h"
#include "nodes/parsenodes.h"
#include "nodes/plannodes.h"
#include "nodes/relation.h"

#include "optimizer/clauses.h" /* AggClauseCosts */
#include "optimizer/planmain.h"   /* GroupContext */

extern Plan *cdb_grouping_planner(PlannerInfo* root,
					 AggClauseCosts *agg_costs,
					 GroupContext *group_context);

extern bool cdbpathlocus_collocates_pathkeys(PlannerInfo *root, CdbPathLocus locus, List *pathkeys, bool exact_match);
extern bool cdbpathlocus_collocates_expressions(PlannerInfo *root, CdbPathLocus locus, List *exprs,
											   bool exact_match);

extern CdbPathLocus cdbpathlocus_from_flow(Flow *flow);
extern List *generate_subquery_tlist(Index varno, List *input_tlist,
									 bool keep_resjunk, int **p_resno_map);
extern List *reconstruct_pathkeys(PlannerInfo *root, List *pathkeys, int *resno_map,
								  List *orig_tlist, List *new_tlist);
extern List *augment_subplan_tlist(List *tlist, List *exprs, int *pnum, AttrNumber **pcols, bool return_resno);

extern Plan *within_agg_planner(PlannerInfo *root, AggClauseCosts *agg_costs,
								GroupContext *group_context);

extern void UpdateScatterClause(Query *query, List *newtlist);

#endif   /* CDBGROUP_H */
