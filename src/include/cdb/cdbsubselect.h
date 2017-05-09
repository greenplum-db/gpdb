/*-------------------------------------------------------------------------
 *
 * cdbsubselect.c
 *	  Flattens subqueries, transforms them to joins.
 *
 * Copyright (c) 2007-2008, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBSUBSELECT_H
#define CDBSUBSELECT_H

struct Node;                            /* #include "nodes/nodes.h" */
struct PlannerInfo;                     /* #include "nodes/relation.h" */

extern void cdbsubselect_flatten_sublinks(struct PlannerInfo *root, struct Node *jtnode);

extern Node *convert_sublink_to_join(PlannerInfo *root, List** rtrlist_inout, SubLink *sublink);
extern Node *convert_EXPR_to_join(PlannerInfo *root, List** rtrlist_inout, OpExpr *opexp);

extern void cdbsubselect_drop_orderby(Query *subselect);
extern void cdbsubselect_drop_distinct(Query *subselect);
extern bool has_correlation_in_funcexpr_rte(List *rtable);
extern bool is_simple_subquery(PlannerInfo *root, Query *subquery);
extern Node *convert_IN_to_antijoin(PlannerInfo *root, SubLink *sublink);

#endif   /* CDBSUBSELECT_H */
