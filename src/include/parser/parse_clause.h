/*-------------------------------------------------------------------------
 *
 * parse_clause.h
 *	  handle clauses in parser
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/parser/parse_clause.h,v 1.50 2008/07/31 22:47:56 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef PARSE_CLAUSE_H
#define PARSE_CLAUSE_H

#include "parser/parse_node.h"

extern void transformFromClause(ParseState *pstate, List *frmList);
extern int setTargetTable(ParseState *pstate, RangeVar *relation,
			   bool inh, bool alsoSource, AclMode requiredPerms);
extern bool interpretInhOption(InhOption inhOpt);
extern bool interpretOidsOption(List *defList);

extern Node *transformWhereClause(ParseState *pstate, Node *clause,
					 const char *constructName);
extern Node *transformLimitClause(ParseState *pstate, Node *clause,
					 const char *constructName);
extern List *transformGroupClause(ParseState *pstate, List *grouplist,
					 List **targetlist, List *sortClause,
					 bool useSQL99);
extern List *transformSortClause(ParseState *pstate, List *orderlist,
                                 List **targetlist, bool resolveUnknown,
                                 bool useSQL99);

extern List *transformWindowDefinitions(ParseState *pstate,
						   List *windowdefs,
						   List **targetlist);

extern List *transformDistinctClause(ParseState *pstate, List *distinctlist,
						List **targetlist, List *sortClause, List **groupClause);
extern List *transformScatterClause(ParseState *pstate, List *scatterlist,
									List **targetlist);
extern void processExtendedGrouping(ParseState *pstate, Node *havingQual,
									List *windowClause, List *targetlist);

extern List *addTargetToSortList(ParseState *pstate, TargetEntry *tle,
					List *sortlist, List *targetlist,
					SortBy *sortby, bool resolveUnknown);
extern Index assignSortGroupRef(TargetEntry *tle, List *tlist);
extern bool targetIsInSortGroupList(TargetEntry *tle, Oid sortop, List *sortList);

#endif   /* PARSE_CLAUSE_H */
