/*-------------------------------------------------------------------------
 *
 * cdbpullup.h
 *    definitions for cdbpullup.c utilities
 *
 * Portions Copyright (c)2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbpullup.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef CDBPULLUP_H
#define CDBPULLUP_H

#include "nodes/relation.h"     /* PathKey, Relids */

struct Plan;                    /* #include "nodes/plannodes.h" */

/*
 * cdbpullup_colIdx
 *
 * Given a [potentially] projecting Plan operator and a vector
 * of attribute numbers in the plan's single subplan, create a
 * vector of attribute numbers in the plan's targetlist.
 *
 * Parameters:
 *      plan -> the Plan node
 *      subplan -> the Plan node's subplan
 *      numCols = number of colIdx array elements
 *      subplanColIdx (IN) -> array [0..numCols-1] of AttrNumber, designating
 *          entries in the targetlist of the Plan operator's subplan.
 *     *pProjectedColIdx (OUT) -> array [0..n-1] of AttrNumber, palloc'd
 *          by this function, where n is the function's return value; or
 *          NULL when n == 0.
 *
 * Returns the number of leading subplanColIdx entries for which matching
 * Var nodes were found in the plan's targetlist.
 */
int
cdbpullup_colIdx(struct Plan   *plan,
                 struct Plan   *subplan,
                 int            numCols,
                 AttrNumber    *subplanColIdx,
                 AttrNumber   **pProjectedColIdx);


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


/*
 * cdbpullup_exprHasSubplanRef
 *
 * Returns true if the expr's first Var is a reference to an item in the
 * targetlist of the associated Plan node's subplan.  If so, it can be
 * assumed that the Plan node and associated exprs have been processed
 * by set_plan_references(), and its Var nodes are in executor format.
 *
 * Returns false otherwise, which implies no conclusion about whether or
 * not set_plan_references() has been done.
 *
 * Note that a Var node that belongs to a Scan operator and refers to the
 * Scan's source relation or index, doesn't have its varno changed by
 * set_plan_references() to OUTER/INNER/0.  Think twice about using this
 * unless you know that the expr comes from an upper Plan node.
 */
bool
cdbpullup_exprHasSubplanRef(Expr *expr);


extern Expr *cdbpullup_findPathKeyExprInTargetList(PathKey *item, List *targetlist);

extern List *cdbpullup_truncatePathKeysForTargetList(List *pathkeys, List *targetlist);


/*
 * cdbpullup_findSubplanRefInTargetList
 *
 * Given a targetlist, returns ptr to first TargetEntry whose expr is a
 * Var node having the specified varattno, and having its varno in executor
 * format (varno is OUTER, INNER, or 0) as set by set_plan_references().
 * Returns NULL if no such TargetEntry is found.
 */
TargetEntry *
cdbpullup_findSubplanRefInTargetList(AttrNumber varattno, List *targetlist);


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

/*
 * cdbpullup_makeVar
 *
 * Returns a new Var node with given 'varno' and 'varattno', and varlevelsup=0.
 *
 * The caller must provide an 'oldexpr' from which we obtain the vartype and
 * vartypmod for the new Var node.  If 'oldexpr' is a Var node, all fields are
 * copied except for varno, varattno and varlevelsup.
 *
 * The parameter modifyOld determines if varnoold and varoattno are modified or
 * not. Rule of thumb is to use modifyOld = false if called before setrefs.
 *
 * Copying an existing Var node preserves its varnoold and varoattno fields,
 * which are used by EXPLAIN to display the table and column name.
 * Also these fields are tested by equal(), so they may need to be set
 * correctly for successful lookups by list_member(), tlist_member(),
 * make_canonical_pathkey(), etc.
 */
Expr *
cdbpullup_make_expr(Index varno, AttrNumber varattno, Expr *oldexpr, bool modifyOld);


/*
 * cdbpullup_missingVarWalker
 *
 * Returns true if some Var in expr is not in targetlist.
 * 'targetlist' is either a List of TargetEntry, or a plain List of Expr.
 *
 * NB:  A Var in the expr is considered as matching a Var in the targetlist
 * without regard for whether or not there is a RelabelType node atop the 
 * targetlist Var.
 *
 * See also: cdbpullup_isExprCoveredByTargetlist
 */
bool
cdbpullup_missingVarWalker(Node *node, void *targetlist);


/*
 * cdbpullup_targetentry
 *
 * Given a TargetEntry from a subplan's targetlist, this function returns a
 * new TargetEntry that can be used to pull the result value up into the
 * parent plan's projection.
 *
 * Parameters:
 *      subplan_targetentry is an item in the targetlist of the subplan S.
 *      newresno is the attribute number of the new TargetEntry (its
 *          position in P's targetlist).
 *      newvarno should be OUTER; except it should be 0 if P is an Agg or
 *          Group node.  It is ignored if useExecutorVarFormat is false.
 *      useExecutorVarFormat must be true if called after optimization,
 *          when set_plan_references() has been called and Var nodes have
 *          been adjusted to refer to items in the subplan's targetlist.
 *          It must be false before the end of optimization, when Var
 *          nodes are still in parser format, referring to RTE items.
 */
TargetEntry *
cdbpullup_targetentry(TargetEntry  *subplan_targetentry,
                      AttrNumber    newresno,
                      Index         newvarno,
                      bool          useExecutorVarFormat);

/*
 * cdbpullup_targetlist
 *
 * Return a targetlist that can be attached to a parent plan yielding
 * the same projection as the given subplan.
 */
List *
cdbpullup_targetlist(struct Plan *subplan, bool useExecutorVarFormat);


#endif   /* CDBPULLUP_H */
