/*-------------------------------------------------------------------------
 *
 * cdbsubselect.c
 *	  Flattens subqueries, transforms them to joins.
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbsubselect.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBSUBSELECT_H
#define CDBSUBSELECT_H

struct Node;                            /* #include "nodes/nodes.h" */
struct PlannerInfo;                     /* #include "nodes/relation.h" */

extern JoinExpr *convert_EXPR_to_join(PlannerInfo *root, OpExpr *opexp);
extern void cdbsubselect_drop_orderby(Query *subselect);
extern void cdbsubselect_drop_distinct(Query *subselect);
extern bool has_correlation_in_funcexpr_rte(List *rtable);
extern bool is_simple_subquery(PlannerInfo *root, Query *subquery, RangeTblEntry *rte,
							   JoinExpr *lowest_outer_join);
extern JoinExpr *convert_IN_to_antijoin(PlannerInfo *root, SubLink *sublink, Relids available_rels);

#endif   /* CDBSUBSELECT_H */
