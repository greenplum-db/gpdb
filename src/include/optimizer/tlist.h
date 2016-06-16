/*-------------------------------------------------------------------------
 *
 * tlist.h
 *	  prototypes for tlist.c.
 *
 *
 * Portions Copyright (c) 2007-2009, Greenplum inc
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/optimizer/tlist.h,v 1.49 2008/01/01 19:45:58 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef TLIST_H
#define TLIST_H

#include "nodes/relation.h"


// return the first target entries that match the node expression
extern TargetEntry *tlist_member(Node *node, List *targetlist);
extern TargetEntry *tlist_member_ignore_relabel(Node *node, List *targetlist);

// return a list a target entries that match the node expression
extern List *tlist_members(Node *node, List *targetlist);

extern TargetEntry *tlist_member_ignoring_RelabelType(Expr *expr, List *targetlist);

extern List *flatten_tlist(List *tlist);
extern List *add_to_flat_tlist(List *tlist, List *vars, bool resjunk);

extern TargetEntry *get_sortgroupref_tle(Index sortref,
					 List *targetList);
extern TargetEntry *get_sortgroupclause_tle(SortClause *sortClause,
						List *targetList);
extern Node *get_sortgroupclause_expr(SortClause *sortClause,
						 List *targetList);
extern List *get_sortgrouplist_exprs(List *sortClauses,
						List *targetList);
extern void get_grouplist_colidx(List *sortClauses,
								 List *targetList, int *numCols,
								 AttrNumber **colIdx, Oid **sortops);

extern List *get_grouplist_exprs(List *groupClause, List *targetList);
extern void get_sortgroupclauses_tles(List *clauses, List *targetList,
									  List **tles, List **sortops);

extern bool tlist_same_datatypes(List *tlist, List *colTypes, bool junkOK);

extern Index maxSortGroupRef(List *targetlist, bool include_orderedagg);

extern int get_row_width(List *tlist);

/* check that two target lists are aligned */
extern void insist_target_lists_aligned(List *tlistFst, List *tlistSnd);

#endif   /* TLIST_H */
