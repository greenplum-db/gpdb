/*-------------------------------------------------------------------------
 *
 * cdbpullup.h
 *    definitions for cdbpullup.c utilities
 *
 * Portions Copyright (c)2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbpullup.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef CDBPULLUP_H
#define CDBPULLUP_H

#include "nodes/pathnodes.h"     /* PathKey, Relids */

/*
 * cdbpullup_expr
 *
 * Suppose there is a Plan node 'P' whose projection is defined by
 * a targetlist 'TL', and which has subplan 'S'.  Given TL and an
 * expr 'X0' whose Var nodes reference the result columns of S, this
 * function returns a new expr 'X1' that is a copy of X0 with Var nodes
 * adjusted to reference the columns of S after their passage through P.
 *
 * Parameters:
 *      expr -> X0, the expr in terms of the subplan S's targetlist
 *      targetlist -> TL (a List of TargetEntry), the plan P's targetlist
 *              (or can be a List of Expr)
 *      newvarlist -> an optional List of Expr which may contain Var nodes
 *              referencing the result of the projection.  The Var nodes in
 *              the new expr are copied from ones in this list if possible,
 *              to get their varnoold and varoattno settings.
 *      newvarno = varno to be used in new Var nodes
 *
 * When calling this function on an expr which has NOT yet been transformed
 * by set_plan_references(), newvarno should be the RTE index assigned to
 * the result of the projection.
 *
 * When calling this function on an expr which HAS been transformed by
 * set_plan_references(), newvarno should usually be OUTER; or 0 if the
 * expr is to be used in the targetlist of an Agg or Group node.
 *
 * At present this function doesn't support pull-up from a subquery into a
 * containing query: there is no provision for adjusting the varlevelsup
 * field in Var nodes for outer references.  This could be added if needed.
 *
 * Returns X1, the expr recast in terms of the given targetlist; or
 * NULL if X0 references a column of S that is not projected in TL.
 */
Expr *
cdbpullup_expr(Expr *expr, List *targetlist, List *newvarlist, Index newvarno);

extern Expr *cdbpullup_findEclassInTargetList(EquivalenceClass *eclass, List *targetlist, Oid hashOpFamily);

extern List *cdbpullup_truncatePathKeysForTargetList(List *pathkeys, List *targetlist);

/*
 * cdbpullup_isExprCoveredByTargetlist
 *
 * Returns true if 'expr' is in 'targetlist', or if 'expr' contains no
 * Var node of the current query level that is not in 'targetlist'.
 *
 * If 'expr' is a List, returns false if the above condition is false for
 * some member of the list.
 *
 * 'targetlist' is a List of TargetEntry.
 *
 * NB:  A Var in the expr is considered as matching a Var in the targetlist
 * without regard for whether or not there is a RelabelType node atop the 
 * targetlist Var.
 *
 * See also: cdbpullup_missing_var_walker
 */
bool
cdbpullup_isExprCoveredByTargetlist(Expr *expr, List *targetlist);

#endif   /* CDBPULLUP_H */
