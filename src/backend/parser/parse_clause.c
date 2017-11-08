/*-------------------------------------------------------------------------
 *
 * parse_clause.c
 *	  handle clauses in parser
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/parser/parse_clause.c,v 1.175 2008/08/07 01:11:51 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/heapam.h"
#include "catalog/heap.h"
#include "catalog/pg_exttable.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_type.h"
#include "catalog/pg_window.h"
#include "commands/defrem.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/tlist.h"
#include "optimizer/var.h"
#include "parser/analyze.h"
#include "parser/parsetree.h"
#include "parser/parse_agg.h"
#include "parser/parse_clause.h"
#include "parser/parse_coerce.h"
#include "parser/parse_expr.h"
#include "parser/parse_oper.h"
#include "parser/parse_relation.h"
#include "parser/parse_target.h"
#include "parser/parse_type.h"
#include "rewrite/rewriteManip.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/rel.h"

#include "cdb/cdbvars.h"
#include "cdb/cdbpartition.h"
#include "catalog/catalog.h"
#include "miscadmin.h"

/* clause types for findTargetlistEntrySQL92 */
#define ORDER_CLAUSE 0
#define GROUP_CLAUSE 1
#define DISTINCT_ON_CLAUSE 2

static char *clauseText[] = {"ORDER BY", "GROUP BY", "DISTINCT ON"};

static void extractRemainingColumns(List *common_colnames,
						List *src_colnames, List *src_colvars,
						List **res_colnames, List **res_colvars);
static Node *transformJoinUsingClause(ParseState *pstate,
						 List *leftVars, List *rightVars);
static Node *transformJoinOnClause(ParseState *pstate, JoinExpr *j,
					  RangeTblEntry *l_rte,
					  RangeTblEntry *r_rte,
					  List *relnamespace,
					  Relids containedRels);
static RangeTblEntry *transformTableEntry(ParseState *pstate, RangeVar *r);
static RangeTblEntry *transformCTEReference(ParseState *pstate, RangeVar *r,
					  CommonTableExpr *cte, Index levelsup);
static RangeTblEntry *transformRangeSubselect(ParseState *pstate,
						RangeSubselect *r);
static RangeTblEntry *transformRangeFunction(ParseState *pstate,
					   RangeFunction *r);
static Node *transformFromClauseItem(ParseState *pstate, Node *n,
						RangeTblEntry **top_rte, int *top_rti,
						List **relnamespace,
						Relids *containedRels);
static Node *buildMergedJoinVar(ParseState *pstate, JoinType jointype,
				   Var *l_colvar, Var *r_colvar);
static TargetEntry *findTargetlistEntrySQL92(ParseState *pstate, Node *node,
						 List **tlist, int clause);
static TargetEntry *findTargetlistEntrySQL99(ParseState *pstate, Node *node,
					List **tlist);
static List *findListTargetlistEntries(ParseState *pstate, Node *node,
									   List **tlist, bool in_grpext,
									   bool ignore_in_grpext,
                                       bool useSQL99);
static List *addTargetToGroupList(ParseState *pstate, TargetEntry *tle,
					 List *grouplist, List *targetlist, int location,
					 bool resolveUnknown);
static TargetEntry *getTargetBySortGroupRef(Index ref, List *tl);
static List *reorderGroupList(List *grouplist);
static List *transformRowExprToList(ParseState *pstate, RowExpr *rowexpr,
									List *groupsets, List *targetList);
static List *transformRowExprToGroupClauses(ParseState *pstate, RowExpr *rowexpr,
											List *groupsets, List *targetList);
static void freeGroupList(List *grouplist);
static WindowClause *findWindowClause(List *wclist, const char *name);
static Node *transformFrameOffset(ParseState *pstate, int frameOptions,
								  Node *clause,
								  List *orderClause, List *targetlist, bool isFollowing,
								  int location);
static SortGroupClause *make_group_clause(TargetEntry *tle, List *targetlist,
				  Oid eqop, Oid sortop, bool nulls_first);

typedef struct grouping_rewrite_ctx
{
	List *grp_tles;
	ParseState *pstate;
} grouping_rewrite_ctx;

typedef struct winref_check_ctx
{
	ParseState *pstate;
	Index winref;
	bool has_order;
	bool has_frame;
} winref_check_ctx;

/*
 * transformFromClause -
 *	  Process the FROM clause and add items to the query's range table,
 *	  joinlist, and namespaces.
 *
 * Note: we assume that pstate's p_rtable, p_joinlist, p_relnamespace, and
 * p_varnamespace lists were initialized to NIL when the pstate was created.
 * We will add onto any entries already present --- this is needed for rule
 * processing, as well as for UPDATE and DELETE.
 *
 * The range table may grow still further when we transform the expressions
 * in the query's quals and target list. (This is possible because in
 * POSTQUEL, we allowed references to relations not specified in the
 * from-clause.  PostgreSQL keeps this extension to standard SQL.)
 */
void
transformFromClause(ParseState *pstate, List *frmList)
{
	ListCell   *fl;

	/*
	 * The grammar will have produced a list of RangeVars, RangeSubselects,
	 * RangeFunctions, and/or JoinExprs. Transform each one (possibly adding
	 * entries to the rtable), check for duplicate refnames, and then add it
	 * to the joinlist and namespaces.
	 */
	foreach(fl, frmList)
	{
		Node	   *n = lfirst(fl);
		RangeTblEntry *rte = NULL;
		int			rtindex = 0;
		List	   *relnamespace = NULL;
		Relids		containedRels = NULL;

		n = transformFromClauseItem(pstate, n,
									&rte,
									&rtindex,
									&relnamespace,
									&containedRels);
		checkNameSpaceConflicts(pstate, pstate->p_relnamespace, relnamespace);
		pstate->p_joinlist = lappend(pstate->p_joinlist, n);
		pstate->p_relnamespace = list_concat(pstate->p_relnamespace,
											 relnamespace);
		pstate->p_varnamespace = lappend(pstate->p_varnamespace, rte);
		bms_free(containedRels);
	}
}

/*
 * winref_checkspec_walker
 */
static bool
winref_checkspec_walker(Node *node, void *ctx)
{
	winref_check_ctx *ref = (winref_check_ctx *)ctx;

	if (!node)
		return false;
	else if (IsA(node, WindowFunc))
	{
		WindowFunc *winref = (WindowFunc *) node;

		/*
		 * Look at functions pointing to the interesting spec only.
		 */
		if (winref->winref != ref->winref)
			return false;

		if (winref->windistinct)
		{
			if (ref->has_order)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("DISTINCT cannot be used with window specification containing an ORDER BY clause"),
						 parser_errposition(ref->pstate, winref->location)));

			if (ref->has_frame)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("DISTINCT cannot be used with window specification containing a framing clause"),
						 parser_errposition(ref->pstate, winref->location)));
		}

		/*
		 * Check compatibilities between function's requirement and
		 * window specification by looking up pg_window catalog.
		 */
		if (!ref->has_order || ref->has_frame)
		{
			HeapTuple		tuple;
			Form_pg_window	wf;

			tuple = SearchSysCache1(WINFNOID,
									ObjectIdGetDatum(winref->winfnoid));

			/*
			 * Check only "true" window function.
			 * Otherwise, it must be an aggregate.
			 */
			if (HeapTupleIsValid(tuple))
			{
				wf = (Form_pg_window) GETSTRUCT(tuple);
				if (wf->winrequireorder && !ref->has_order)
					ereport(ERROR,
							(errcode(ERRCODE_WRONG_OBJECT_TYPE),
							 errmsg("window function \"%s\" requires a window specification with an ordering clause",
									get_func_name(wf->winfnoid)),
								parser_errposition(ref->pstate, winref->location)));

				if (!wf->winallowframe && ref->has_frame)
					ereport(ERROR,
							(errcode(ERRCODE_WRONG_OBJECT_TYPE),
							 errmsg("window function \"%s\" cannot be used with a framed window specification",
									get_func_name(wf->winfnoid)),
								parser_errposition(ref->pstate, winref->location)));
				ReleaseSysCache(tuple);
			}
		}
	}

	return expression_tree_walker(node, winref_checkspec_walker, ctx);
}

/*
 * winref_checkspec
 *
 * See if any WindowFuncss using this spec are DISTINCT qualified.
 *
 * In addition, we're going to check winrequireorder / winallowframe.
 * You might want to do it in ParseFuncOrColumn,
 * but we need to do this here after all the transformations
 * (especially parent inheritance) was done.
 */
static bool
winref_checkspec(ParseState *pstate, List *targetlist, Index winref,
				 bool has_order, bool has_frame)
{
	winref_check_ctx ctx;

	ctx.pstate = pstate;
	ctx.winref = winref;
	ctx.has_order = has_order;
	ctx.has_frame = has_frame;

	return expression_tree_walker((Node *) targetlist,
								  winref_checkspec_walker, (void *) &ctx);
}

/*
 * setTargetTable
 *	  Add the target relation of INSERT/UPDATE/DELETE to the range table,
 *	  and make the special links to it in the ParseState.
 *
 *	  We also open the target relation and acquire a write lock on it.
 *	  This must be done before processing the FROM list, in case the target
 *	  is also mentioned as a source relation --- we want to be sure to grab
 *	  the write lock before any read lock.
 *
 *	  If alsoSource is true, add the target to the query's joinlist and
 *	  namespace.  For INSERT, we don't want the target to be joined to;
 *	  it's a destination of tuples, not a source.	For UPDATE/DELETE,
 *	  we do need to scan or join the target.  (NOTE: we do not bother
 *	  to check for namespace conflict; we assume that the namespace was
 *	  initially empty in these cases.)
 *
 *	  Finally, we mark the relation as requiring the permissions specified
 *	  by requiredPerms.
 *
 *	  Returns the rangetable index of the target relation.
 */
int
setTargetTable(ParseState *pstate, RangeVar *relation,
			   bool inh, bool alsoSource, AclMode requiredPerms)
{
	RangeTblEntry *rte;
	int			rtindex;
	ParseCallbackState pcbstate;

	/* Close old target; this could only happen for multi-action rules */
	if (pstate->p_target_relation != NULL)
		heap_close(pstate->p_target_relation, NoLock);

	/*
	 * Open target rel and grab suitable lock (which we will hold till end of
	 * transaction).
	 *
	 * free_parsestate() will eventually do the corresponding heap_close(),
	 * but *not* release the lock.
     *
	 * CDB: Acquire ExclusiveLock if it is a distributed relation and we are
	 * doing UPDATE or DELETE activity
	 */
	setup_parser_errposition_callback(&pcbstate, pstate, relation->location);
	if (pstate->p_is_insert && !pstate->p_is_update)
	{
		pstate->p_target_relation = heap_openrv(relation, RowExclusiveLock);
	}
	else
	{
    	pstate->p_target_relation = CdbOpenRelationRv(relation, 
													  RowExclusiveLock, 
													  false, NULL);
	}
	cancel_parser_errposition_callback(&pcbstate);
	
	/*
	 * Now build an RTE.
	 */
	rte = addRangeTableEntryForRelation(pstate, pstate->p_target_relation,
										relation->alias, inh, false);
	pstate->p_target_rangetblentry = rte;

	/* assume new rte is at end */
	rtindex = list_length(pstate->p_rtable);
	Assert(rte == rt_fetch(rtindex, pstate->p_rtable));

	/*
	 * Special check for DML on system relations,
	 * allow DML when:
	 * 	- in single user mode: initdb insert PIN entries to pg_depend,...
	 * 	- in maintenance mode, upgrade mode or
	 *  - allow_system_table_mods = dml
	 */
	if (IsUnderPostmaster && !allowSystemTableModsDML
		&& IsSystemRelation(pstate->p_target_relation))
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("permission denied: \"%s\" is a system catalog",
						 RelationGetRelationName(pstate->p_target_relation))));

	/* special check for DML on foreign relations */
	if(RelationIsForeign(pstate->p_target_relation))
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("foreign tables are read only. cannot change \"%s\"",
						 RelationGetRelationName(pstate->p_target_relation))));


	/* special check for DML on external relations */
	if(RelationIsExternal(pstate->p_target_relation))
	{
		if (requiredPerms != ACL_INSERT)
		{
			/* UPDATE/DELETE */
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("cannot update or delete from external relation \"%s\"",
							RelationGetRelationName(pstate->p_target_relation))));
		}
		else
		{
			/* INSERT */
			Oid reloid = RelationGetRelid(pstate->p_target_relation);
			ExtTableEntry* 	extentry;
			
			extentry = GetExtTableEntry(reloid);
			
			if(!extentry->iswritable)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("cannot change a readable external table \"%s\"",
								 RelationGetRelationName(pstate->p_target_relation))));

			pfree(extentry);
		}
	}
	
    /* MPP-21035: Directly modify a part of a partitioned table is disallowed */
    PartStatus targetRelPartStatus = rel_part_status(RelationGetRelid(pstate->p_target_relation));
    if(PART_STATUS_INTERIOR == targetRelPartStatus)
    {
    	ereport(ERROR,
    			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
    			 errmsg("Directly modifying intermediate part of a partitioned table is disallowed"),
    			 errhint("Modify either the root or a leaf partition instead")));
    }

	/*
	 * Override addRangeTableEntry's default ACL_SELECT permissions check, and
	 * instead mark target table as requiring exactly the specified
	 * permissions.
	 *
	 * If we find an explicit reference to the rel later during parse
	 * analysis, we will add the ACL_SELECT bit back again; see
	 * scanRTEForColumn (for simple field references), ExpandColumnRefStar
	 * (for foo.*) and ExpandAllTables (for *).
	 */
	rte->requiredPerms = requiredPerms;

	/*
	 * If UPDATE/DELETE, add table to joinlist and namespaces.
	 */
	if (alsoSource)
		addRTEtoQuery(pstate, rte, true, true, true);

	return rtindex;
}

/*
 * Simplify InhOption (yes/no/default) into boolean yes/no.
 *
 * The reason we do things this way is that we don't want to examine the
 * SQL_inheritance option flag until parse_analyze() is run.	Otherwise,
 * we'd do the wrong thing with query strings that intermix SET commands
 * with queries.
 */
bool
interpretInhOption(InhOption inhOpt)
{
	switch (inhOpt)
	{
		case INH_NO:
			return false;
		case INH_YES:
			return true;
		case INH_DEFAULT:
			return SQL_inheritance;
	}
	elog(ERROR, "bogus InhOption value: %d", inhOpt);
	return false;				/* keep compiler quiet */
}

/*
 * Given a relation-options list (of DefElems), return true iff the specified
 * table/result set should be created with OIDs. This needs to be done after
 * parsing the query string because the return value can depend upon the
 * default_with_oids GUC var.
 */
bool
interpretOidsOption(List *defList)
{
	ListCell   *cell;

	/* Scan list to see if OIDS was included */
	foreach(cell, defList)
	{
		DefElem    *def = (DefElem *) lfirst(cell);

		if (pg_strcasecmp(def->defname, "oids") == 0)
			return defGetBoolean(def);
	}

	/* OIDS option was not specified, so use default. */
	return default_with_oids;
}

/*
 * Extract all not-in-common columns from column lists of a source table
 */
static void
extractRemainingColumns(List *common_colnames,
						List *src_colnames, List *src_colvars,
						List **res_colnames, List **res_colvars)
{
	List	   *new_colnames = NIL;
	List	   *new_colvars = NIL;
	ListCell   *lnames,
			   *lvars;

	Assert(list_length(src_colnames) == list_length(src_colvars));

	forboth(lnames, src_colnames, lvars, src_colvars)
	{
		char	   *colname = strVal(lfirst(lnames));
		bool		match = false;
		ListCell   *cnames;

		foreach(cnames, common_colnames)
		{
			char	   *ccolname = strVal(lfirst(cnames));

			if (strcmp(colname, ccolname) == 0)
			{
				match = true;
				break;
			}
		}

		if (!match)
		{
			new_colnames = lappend(new_colnames, lfirst(lnames));
			new_colvars = lappend(new_colvars, lfirst(lvars));
		}
	}

	*res_colnames = new_colnames;
	*res_colvars = new_colvars;
}

/* transformJoinUsingClause()
 *	  Build a complete ON clause from a partially-transformed USING list.
 *	  We are given lists of nodes representing left and right match columns.
 *	  Result is a transformed qualification expression.
 */
static Node *
transformJoinUsingClause(ParseState *pstate, List *leftVars, List *rightVars)
{
	Node	   *result = NULL;
	ListCell   *lvars,
			   *rvars;

	/*
	 * We cheat a little bit here by building an untransformed operator tree
	 * whose leaves are the already-transformed Vars.  This is OK because
	 * transformExpr() won't complain about already-transformed subnodes.
	 */
	forboth(lvars, leftVars, rvars, rightVars)
	{
		Node	   *lvar = (Node *) lfirst(lvars);
		Node	   *rvar = (Node *) lfirst(rvars);
		A_Expr	   *e;

		e = makeSimpleA_Expr(AEXPR_OP, "=",
							 copyObject(lvar), copyObject(rvar),
							 -1);

		if (result == NULL)
			result = (Node *) e;
		else
		{
			A_Expr	   *a;

			a = makeA_Expr(AEXPR_AND, NIL, result, (Node *) e, -1);
			result = (Node *) a;
		}
	}

	/*
	 * Since the references are already Vars, and are certainly from the input
	 * relations, we don't have to go through the same pushups that
	 * transformJoinOnClause() does.  Just invoke transformExpr() to fix up
	 * the operators, and we're done.
	 */
	result = transformExpr(pstate, result);

	result = coerce_to_boolean(pstate, result, "JOIN/USING");

	return result;
}

/* transformJoinOnClause()
 *	  Transform the qual conditions for JOIN/ON.
 *	  Result is a transformed qualification expression.
 */
static Node *
transformJoinOnClause(ParseState *pstate, JoinExpr *j,
					  RangeTblEntry *l_rte,
					  RangeTblEntry *r_rte,
					  List *relnamespace,
					  Relids containedRels)
{
	Node	   *result;
	List	   *save_relnamespace;
	List	   *save_varnamespace;
	Relids		clause_varnos;
	int			varno;

	/*
	 * This is a tad tricky, for two reasons.  First, the namespace that the
	 * join expression should see is just the two subtrees of the JOIN plus
	 * any outer references from upper pstate levels.  So, temporarily set
	 * this pstate's namespace accordingly.  (We need not check for refname
	 * conflicts, because transformFromClauseItem() already did.) NOTE: this
	 * code is OK only because the ON clause can't legally alter the namespace
	 * by causing implicit relation refs to be added.
	 */
	save_relnamespace = pstate->p_relnamespace;
	save_varnamespace = pstate->p_varnamespace;

	pstate->p_relnamespace = relnamespace;
	pstate->p_varnamespace = list_make2(l_rte, r_rte);

	result = transformWhereClause(pstate, j->quals, "JOIN/ON");

	pstate->p_relnamespace = save_relnamespace;
	pstate->p_varnamespace = save_varnamespace;

	/*
	 * Second, we need to check that the ON condition doesn't refer to any
	 * rels outside the input subtrees of the JOIN.  It could do that despite
	 * our hack on the namespace if it uses fully-qualified names. So, grovel
	 * through the transformed clause and make sure there are no bogus
	 * references.	(Outer references are OK, and are ignored here.)
	 */
	clause_varnos = pull_varnos(result);
	clause_varnos = bms_del_members(clause_varnos, containedRels);
	if ((varno = bms_first_member(clause_varnos)) >= 0)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
		 errmsg("JOIN/ON clause refers to \"%s\", which is not part of JOIN",
				rt_fetch(varno, pstate->p_rtable)->eref->aliasname)));
	}
	bms_free(clause_varnos);

	return result;
}

/*
 * transformTableEntry --- transform a RangeVar (simple relation reference)
 */
static RangeTblEntry *
transformTableEntry(ParseState *pstate, RangeVar *r)
{
	RangeTblEntry *rte;

	/*
	 * mark this entry to indicate it comes from the FROM clause. In SQL, the
	 * target list can only refer to range variables specified in the from
	 * clause but we follow the more powerful POSTQUEL semantics and
	 * automatically generate the range variable if not specified. However
	 * there are times we need to know whether the entries are legitimate.
	 */
	rte = addRangeTableEntry(pstate, r, r->alias,
							 interpretInhOption(r->inhOpt), true);

	return rte;
}

/*
 * transformCTEReference --- transform a RangeVar that references a common
 * table expression (ie, a sub-SELECT defined in a WITH clause)
 */
static RangeTblEntry *
transformCTEReference(ParseState *pstate, RangeVar *r,
					  CommonTableExpr *cte, Index levelsup)
{
	RangeTblEntry *rte;

	rte = addRangeTableEntryForCTE(pstate, cte, levelsup, r->alias, true);

	return rte;
}

/*
 * transformRangeSubselect --- transform a sub-SELECT appearing in FROM
 */
static RangeTblEntry *
transformRangeSubselect(ParseState *pstate, RangeSubselect *r)
{
	Query	   *query;
	RangeTblEntry *rte;

	/*
	 * We require user to supply an alias for a subselect, per SQL92. To relax
	 * this, we'd have to be prepared to gin up a unique alias for an
	 * unlabeled subselect.
	 */
	if (r->alias == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("subquery in FROM must have an alias")));

	/*
	 * Analyze and transform the subquery.
	 */
	query = parse_sub_analyze(r->subquery, pstate);

	/*
	 * Check that we got something reasonable.	Many of these conditions are
	 * impossible given restrictions of the grammar, but check 'em anyway.
	 */
	if (!IsA(query, Query) ||
		query->commandType != CMD_SELECT ||
		query->utilityStmt != NULL)
		elog(ERROR, "unexpected non-SELECT command in subquery in FROM");
	if (query->intoClause)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("subquery in FROM cannot have SELECT INTO"),
				 parser_errposition(pstate,
								 exprLocation((Node *) query->intoClause))));

	/*
	 * The subquery cannot make use of any variables from FROM items created
	 * earlier in the current query.  Per SQL92, the scope of a FROM item does
	 * not include other FROM items.  Formerly we hacked the namespace so that
	 * the other variables weren't even visible, but it seems more useful to
	 * leave them visible and give a specific error message.
	 *
	 * XXX this will need further work to support SQL99's LATERAL() feature,
	 * wherein such references would indeed be legal.
	 *
	 * We can skip groveling through the subquery if there's not anything
	 * visible in the current query.  Also note that outer references are OK.
	 */
	if (pstate->p_relnamespace || pstate->p_varnamespace)
	{
		if (contain_vars_of_level((Node *) query, 1))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
					 errmsg("subquery in FROM cannot refer to other relations of same query level"),
					 parser_errposition(pstate,
								   locate_var_of_level((Node *) query, 1))));
	}

	/*
	 * OK, build an RTE for the subquery.
	 */
	rte = addRangeTableEntryForSubquery(pstate, query, r->alias, true);

	return rte;
}


/*
 * transformRangeFunction --- transform a function call appearing in FROM
 */
static RangeTblEntry *
transformRangeFunction(ParseState *pstate, RangeFunction *r)
{
	Node	   *funcexpr;
	char	   *funcname;
	RangeTblEntry *rte;

	/*
	 * Get function name for possible use as alias.  We use the same
	 * transformation rules as for a SELECT output expression.	For a FuncCall
	 * node, the result will be the function name, but it is possible for the
	 * grammar to hand back other node types.
	 */
	funcname = FigureColname(r->funccallnode);

	if (funcname)
	{
		if (pg_strncasecmp(funcname, GP_DIST_RANDOM_NAME, sizeof(GP_DIST_RANDOM_NAME)) == 0)
		{
			/* OK, now we need to check the arguments and generate a RTE */
			FuncCall *fc;
			RangeVar *rel;

			fc = (FuncCall *)r->funccallnode;

			if (list_length(fc->args) != 1)
				elog(ERROR, "Invalid %s syntax.", GP_DIST_RANDOM_NAME);

			if (IsA(linitial(fc->args), A_Const))
			{
				A_Const *arg_val;
				char *schemaname;
				char *tablename;

				arg_val = linitial(fc->args);
				if (!IsA(&arg_val->val, String))
				{
					elog(ERROR, "%s: invalid argument type, non-string in value", GP_DIST_RANDOM_NAME);
				}

				schemaname = strVal(&arg_val->val);
				tablename = strchr(schemaname, '.');
				if (tablename)
				{
					*tablename = 0;
					tablename++;
				}
				else
				{
					/* no schema */
					tablename = schemaname;
					schemaname = NULL;
				}

				/* Got the name of the table, now we need to build the RTE for the table. */
				rel = makeRangeVar(schemaname, tablename, arg_val->location);
				rel->location = arg_val->location;

				rte = addRangeTableEntry(pstate, rel, r->alias, false, true);

				/* Now we set our special attribute in the rte. */
				rte->forceDistRandom = true;

				return rte;
			}
			else
			{
				elog(ERROR, "%s: invalid argument type", GP_DIST_RANDOM_NAME);
			}
		}
	}

	/*
	 * Transform the raw expression.
	 */
	funcexpr = transformExpr(pstate, r->funccallnode);

	/*
	 * The function parameters cannot make use of any variables from other
	 * FROM items.	(Compare to transformRangeSubselect(); the coding is
	 * different though because we didn't parse as a sub-select with its own
	 * level of namespace.)
	 *
	 * XXX this will need further work to support SQL99's LATERAL() feature,
	 * wherein such references would indeed be legal.
	 */
	if (pstate->p_relnamespace || pstate->p_varnamespace)
	{
		if (contain_vars_of_level(funcexpr, 0))
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
					 errmsg("function expression in FROM cannot refer to other relations of same query level"),
					 parser_errposition(pstate,
										locate_var_of_level(funcexpr, 0))));
	}

	/*
	 * Disallow aggregate functions in the expression.	(No reason to postpone
	 * this check until parseCheckAggregates.)
	 */
	if (pstate->p_hasAggs)
	{
		if (checkExprHasAggs(funcexpr))
			ereport(ERROR,
					(errcode(ERRCODE_GROUPING_ERROR),
					 errmsg("cannot use aggregate function in function expression in FROM"),
					 parser_errposition(pstate,
										locate_agg_of_level(funcexpr, 0))));
	}

	/*
	 * OK, build an RTE for the function.
	 */
	rte = addRangeTableEntryForFunction(pstate, funcname, funcexpr,
										r, true);

	/*
	 * If a coldeflist was supplied, ensure it defines a legal set of names
	 * (no duplicates) and datatypes (no pseudo-types, for instance).
	 * addRangeTableEntryForFunction looked up the type names but didn't check
	 * them further than that.
	 */
	if (r->coldeflist)
	{
		TupleDesc	tupdesc;

		tupdesc = BuildDescFromLists(rte->eref->colnames,
									 rte->funccoltypes,
									 rte->funccoltypmods);
		CheckAttributeNamesTypes(tupdesc, RELKIND_COMPOSITE_TYPE);
	}

	return rte;
}


/*
 * transformFromClauseItem -
 *	  Transform a FROM-clause item, adding any required entries to the
 *	  range table list being built in the ParseState, and return the
 *	  transformed item ready to include in the joinlist and namespaces.
 *	  This routine can recurse to handle SQL92 JOIN expressions.
 *
 * The function return value is the node to add to the jointree (a
 * RangeTblRef or JoinExpr).  Additional output parameters are:
 *
 * *top_rte: receives the RTE corresponding to the jointree item.
 * (We could extract this from the function return node, but it saves cycles
 * to pass it back separately.)
 *
 * *top_rti: receives the rangetable index of top_rte.	(Ditto.)
 *
 * *relnamespace: receives a List of the RTEs exposed as relation names
 * by this item.
 *
 * *containedRels: receives a bitmap set of the rangetable indexes
 * of all the base and join relations represented in this jointree item.
 * This is needed for checking JOIN/ON conditions in higher levels.
 *
 * We do not need to pass back an explicit varnamespace value, because
 * in all cases the varnamespace contribution is exactly top_rte.
 */
static Node *
transformFromClauseItem(ParseState *pstate, Node *n,
						RangeTblEntry **top_rte, int *top_rti,
						List **relnamespace,
						Relids *containedRels)
{
    Node                   *result;

	if (IsA(n, RangeVar))
	{
		/* Plain relation reference, or perhaps a CTE reference */
		RangeVar *rv = (RangeVar *) n;
		RangeTblRef *rtr;
		RangeTblEntry *rte = NULL;
		int			rtindex;

		/*
		 * If it is an unqualified name, it might be a reference to some
		 * CTE visible in this or a parent query.
		 */
		if (!rv->schemaname)
		{
			ParseState *ps;
			Index	levelsup;

			for (ps = pstate, levelsup = 0;
				 ps != NULL;
				 ps = ps->parentParseState, levelsup++)
			{
				ListCell *lc;

				foreach(lc, ps->p_ctenamespace)
				{
					CommonTableExpr *cte = (CommonTableExpr *) lfirst(lc);

					if (strcmp(rv->relname, cte->ctename) == 0)
					{
						rte = transformCTEReference(pstate, rv, cte, levelsup);
						break;
					}
				}
				if (rte)
					break;
			}
		}

		/* if not found as a CTE, must be a table reference */
		if (!rte)
			rte = transformTableEntry(pstate, rv);

		/* assume new rte is at end */
		rtindex = list_length(pstate->p_rtable);
		Assert(rte == rt_fetch(rtindex, pstate->p_rtable));
		*top_rte = rte;
		*top_rti = rtindex;
		*relnamespace = list_make1(rte);
		*containedRels = bms_make_singleton(rtindex);
		rtr = makeNode(RangeTblRef);
		rtr->rtindex = rtindex;
		result = (Node *) rtr;
	}
	else if (IsA(n, RangeSubselect))
	{
		/* sub-SELECT is like a plain relation */
		RangeTblRef *rtr;
		RangeTblEntry *rte;
		int			rtindex;

		rte = transformRangeSubselect(pstate, (RangeSubselect *) n);
		/* assume new rte is at end */
		rtindex = list_length(pstate->p_rtable);
		Assert(rte == rt_fetch(rtindex, pstate->p_rtable));
		*top_rte = rte;
		*top_rti = rtindex;
		*relnamespace = list_make1(rte);
		*containedRels = bms_make_singleton(rtindex);
		rtr = makeNode(RangeTblRef);
		rtr->rtindex = rtindex;
		result = (Node *) rtr;
	}
	else if (IsA(n, RangeFunction))
	{
		/* function is like a plain relation */
		RangeTblRef *rtr;
		RangeTblEntry *rte;
		int			rtindex;

		rte = transformRangeFunction(pstate, (RangeFunction *) n);
		/* assume new rte is at end */
		rtindex = list_length(pstate->p_rtable);
		Assert(rte == rt_fetch(rtindex, pstate->p_rtable));
		*top_rte = rte;
		*top_rti = rtindex;
		*relnamespace = list_make1(rte);
		*containedRels = bms_make_singleton(rtindex);
		rtr = makeNode(RangeTblRef);
		rtr->rtindex = rtindex;
		result = (Node *) rtr;
	}
	else if (IsA(n, JoinExpr))
	{
		/* A newfangled join expression */
		JoinExpr   *j = (JoinExpr *) n;
		RangeTblEntry *l_rte;
		RangeTblEntry *r_rte;
		int			l_rtindex;
		int			r_rtindex;
		Relids		l_containedRels,
					r_containedRels,
					my_containedRels;
		List	   *l_relnamespace,
				   *r_relnamespace,
				   *my_relnamespace,
				   *l_colnames,
				   *r_colnames,
				   *res_colnames,
				   *l_colvars,
				   *r_colvars,
				   *res_colvars;
		RangeTblEntry *rte;

		/*
		 * Recursively process the left and right subtrees
		 */
		j->larg = transformFromClauseItem(pstate, j->larg,
										  &l_rte,
										  &l_rtindex,
										  &l_relnamespace,
										  &l_containedRels);
		j->rarg = transformFromClauseItem(pstate, j->rarg,
										  &r_rte,
										  &r_rtindex,
										  &r_relnamespace,
										  &r_containedRels);

		/*
		 * Check for conflicting refnames in left and right subtrees. Must do
		 * this because higher levels will assume I hand back a self-
		 * consistent namespace subtree.
		 */
		checkNameSpaceConflicts(pstate, l_relnamespace, r_relnamespace);

		/*
		 * Generate combined relation membership info for possible use by
		 * transformJoinOnClause below.
		 */
		my_relnamespace = list_concat(l_relnamespace, r_relnamespace);
		my_containedRels = bms_join(l_containedRels, r_containedRels);

		pfree(r_relnamespace);	/* free unneeded list header */

		/*
		 * Extract column name and var lists from both subtrees
		 *
		 * Note: expandRTE returns new lists, safe for me to modify
		 */
		expandRTE(l_rte, l_rtindex, 0, -1, false,
				  &l_colnames, &l_colvars);
		expandRTE(r_rte, r_rtindex, 0, -1, false,
				  &r_colnames, &r_colvars);

		/*
		 * Natural join does not explicitly specify columns; must generate
		 * columns to join. Need to run through the list of columns from each
		 * table or join result and match up the column names. Use the first
		 * table, and check every column in the second table for a match.
		 * (We'll check that the matches were unique later on.) The result of
		 * this step is a list of column names just like an explicitly-written
		 * USING list.
		 */
		if (j->isNatural)
		{
			List	   *rlist = NIL;
			ListCell   *lx,
					   *rx;

			Assert(j->usingClause == NIL);	/* shouldn't have USING() too */

			foreach(lx, l_colnames)
			{
				char	   *l_colname = strVal(lfirst(lx));
				Value	   *m_name = NULL;

				foreach(rx, r_colnames)
				{
					char	   *r_colname = strVal(lfirst(rx));

					if (strcmp(l_colname, r_colname) == 0)
					{
						m_name = makeString(l_colname);
						break;
					}
				}

				/* matched a right column? then keep as join column... */
				if (m_name != NULL)
					rlist = lappend(rlist, m_name);
			}

			j->usingClause = rlist;
		}

		/*
		 * Now transform the join qualifications, if any.
		 */
		res_colnames = NIL;
		res_colvars = NIL;

		if (j->usingClause)
		{
			/*
			 * JOIN/USING (or NATURAL JOIN, as transformed above). Transform
			 * the list into an explicit ON-condition, and generate a list of
			 * merged result columns.
			 */
			List	   *ucols = j->usingClause;
			List	   *l_usingvars = NIL;
			List	   *r_usingvars = NIL;
			ListCell   *ucol;

			Assert(j->quals == NULL);	/* shouldn't have ON() too */

			foreach(ucol, ucols)
			{
				char	   *u_colname = strVal(lfirst(ucol));
				ListCell   *col;
				int			ndx;
				int			l_index = -1;
				int			r_index = -1;
				Var		   *l_colvar,
						   *r_colvar;

				/* Check for USING(foo,foo) */
				foreach(col, res_colnames)
				{
					char	   *res_colname = strVal(lfirst(col));

					if (strcmp(res_colname, u_colname) == 0)
						ereport(ERROR,
								(errcode(ERRCODE_DUPLICATE_COLUMN),
								 errmsg("column name \"%s\" appears more than once in USING clause",
										u_colname)));
				}

				/* Find it in left input */
				ndx = 0;
				foreach(col, l_colnames)
				{
					char	   *l_colname = strVal(lfirst(col));

					if (strcmp(l_colname, u_colname) == 0)
					{
						if (l_index >= 0)
							ereport(ERROR,
									(errcode(ERRCODE_AMBIGUOUS_COLUMN),
									 errmsg("common column name \"%s\" appears more than once in left table",
											u_colname)));
						l_index = ndx;
					}
					ndx++;
				}
				if (l_index < 0)
					ereport(ERROR,
							(errcode(ERRCODE_UNDEFINED_COLUMN),
							 errmsg("column \"%s\" specified in USING clause does not exist in left table",
									u_colname)));

				/* Find it in right input */
				ndx = 0;
				foreach(col, r_colnames)
				{
					char	   *r_colname = strVal(lfirst(col));

					if (strcmp(r_colname, u_colname) == 0)
					{
						if (r_index >= 0)
							ereport(ERROR,
									(errcode(ERRCODE_AMBIGUOUS_COLUMN),
									 errmsg("common column name \"%s\" appears more than once in right table",
											u_colname)));
						r_index = ndx;
					}
					ndx++;
				}
				if (r_index < 0)
					ereport(ERROR,
							(errcode(ERRCODE_UNDEFINED_COLUMN),
							 errmsg("column \"%s\" specified in USING clause does not exist in right table",
									u_colname)));

				l_colvar = list_nth(l_colvars, l_index);
				l_usingvars = lappend(l_usingvars, l_colvar);
				r_colvar = list_nth(r_colvars, r_index);
				r_usingvars = lappend(r_usingvars, r_colvar);

				res_colnames = lappend(res_colnames, lfirst(ucol));
				res_colvars = lappend(res_colvars,
									  buildMergedJoinVar(pstate,
														 j->jointype,
														 l_colvar,
														 r_colvar));
			}

			j->quals = transformJoinUsingClause(pstate,
												l_usingvars,
												r_usingvars);
		}
		else if (j->quals)
		{
			/* User-written ON-condition; transform it */
			j->quals = transformJoinOnClause(pstate, j,
											 l_rte, r_rte,
											 my_relnamespace,
											 my_containedRels);
		}
		else
		{
			/* CROSS JOIN: no quals */
		}

		/* Add remaining columns from each side to the output columns */
		extractRemainingColumns(res_colnames,
								l_colnames, l_colvars,
								&l_colnames, &l_colvars);
		extractRemainingColumns(res_colnames,
								r_colnames, r_colvars,
								&r_colnames, &r_colvars);
		res_colnames = list_concat(res_colnames, l_colnames);
		res_colvars = list_concat(res_colvars, l_colvars);
		res_colnames = list_concat(res_colnames, r_colnames);
		res_colvars = list_concat(res_colvars, r_colvars);

		/*
		 * Check alias (AS clause), if any.
		 */
		if (j->alias)
		{
			if (j->alias->colnames != NIL)
			{
				if (list_length(j->alias->colnames) > list_length(res_colnames))
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("column alias list for \"%s\" has too many entries",
									j->alias->aliasname)));
			}
		}

		/*
		 * Now build an RTE for the result of the join
		 */
		rte = addRangeTableEntryForJoin(pstate,
										res_colnames,
										j->jointype,
										res_colvars,
										j->alias,
										true);

		/* assume new rte is at end */
		j->rtindex = list_length(pstate->p_rtable);
		Assert(rte == rt_fetch(j->rtindex, pstate->p_rtable));

		*top_rte = rte;
		*top_rti = j->rtindex;

		/*
		 * Prepare returned namespace list.  If the JOIN has an alias then it
		 * hides the contained RTEs as far as the relnamespace goes;
		 * otherwise, put the contained RTEs and *not* the JOIN into
		 * relnamespace.
		 */
		if (j->alias)
		{
			*relnamespace = list_make1(rte);
			list_free(my_relnamespace);
		}
		else
			*relnamespace = my_relnamespace;

		/*
		 * Include join RTE in returned containedRels set
		 */
		*containedRels = bms_add_member(my_containedRels, j->rtindex);

		result = (Node *) j;
	}
	else
    {
        result = NULL;
		elog(ERROR, "unrecognized node type: %d", (int) nodeTag(n));
    }

	return result;
}

/*
 * buildMergedJoinVar -
 *	  generate a suitable replacement expression for a merged join column
 */
static Node *
buildMergedJoinVar(ParseState *pstate, JoinType jointype,
				   Var *l_colvar, Var *r_colvar)
{
	Oid			outcoltype;
	int32		outcoltypmod;
	Node	   *l_node,
			   *r_node,
			   *res_node;

	/*
	 * Choose output type if input types are dissimilar.
	 */
	outcoltype = l_colvar->vartype;
	outcoltypmod = l_colvar->vartypmod;
	if (outcoltype != r_colvar->vartype)
	{
		outcoltype = select_common_type(list_make2_oid(l_colvar->vartype,
													   r_colvar->vartype),
										"JOIN/USING");
		outcoltypmod = -1;		/* ie, unknown */
	}
	else if (outcoltypmod != r_colvar->vartypmod)
	{
		/* same type, but not same typmod */
		outcoltypmod = -1;		/* ie, unknown */
	}

	/*
	 * Insert coercion functions if needed.  Note that a difference in typmod
	 * can only happen if input has typmod but outcoltypmod is -1. In that
	 * case we insert a RelabelType to clearly mark that result's typmod is
	 * not same as input.  We never need coerce_type_typmod.
	 */
	if (l_colvar->vartype != outcoltype)
		l_node = coerce_type(pstate, (Node *) l_colvar, l_colvar->vartype,
							 outcoltype, outcoltypmod,
							 COERCION_IMPLICIT, COERCE_IMPLICIT_CAST,
							 -1);
	else if (l_colvar->vartypmod != outcoltypmod)
		l_node = (Node *) makeRelabelType((Expr *) l_colvar,
										  outcoltype, outcoltypmod,
										  COERCE_IMPLICIT_CAST);
	else
		l_node = (Node *) l_colvar;

	if (r_colvar->vartype != outcoltype)
		r_node = coerce_type(pstate, (Node *) r_colvar, r_colvar->vartype,
							 outcoltype, outcoltypmod,
							 COERCION_IMPLICIT, COERCE_IMPLICIT_CAST,
							 -1);
	else if (r_colvar->vartypmod != outcoltypmod)
		r_node = (Node *) makeRelabelType((Expr *) r_colvar,
										  outcoltype, outcoltypmod,
										  COERCE_IMPLICIT_CAST);
	else
		r_node = (Node *) r_colvar;

	/*
	 * Choose what to emit
	 */
	switch (jointype)
	{
		case JOIN_INNER:

			/*
			 * We can use either var; prefer non-coerced one if available.
			 */
			if (IsA(l_node, Var))
				res_node = l_node;
			else if (IsA(r_node, Var))
				res_node = r_node;
			else
				res_node = l_node;
			break;
		case JOIN_LEFT:
			/* Always use left var */
			res_node = l_node;
			break;
		case JOIN_RIGHT:
			/* Always use right var */
			res_node = r_node;
			break;
		case JOIN_FULL:
			{
				/*
				 * Here we must build a COALESCE expression to ensure that the
				 * join output is non-null if either input is.
				 */
				CoalesceExpr *c = makeNode(CoalesceExpr);

				c->coalescetype = outcoltype;
				c->args = list_make2(l_node, r_node);
				res_node = (Node *) c;
				break;
			}
		default:
			elog(ERROR, "unrecognized join type: %d", (int) jointype);
			res_node = NULL;	/* keep compiler quiet */
			break;
	}

	return res_node;
}


/*
 * transformWhereClause -
 *	  Transform the qualification and make sure it is of type boolean.
 *	  Used for WHERE and allied clauses.
 *
 * constructName does not affect the semantics, but is used in error messages
 */
Node *
transformWhereClause(ParseState *pstate, Node *clause,
					 const char *constructName)
{
	Node	   *qual;

	if (clause == NULL)
		return NULL;

	qual = transformExpr(pstate, clause);

	qual = coerce_to_boolean(pstate, qual, constructName);

	return qual;
}


/*
 * transformLimitClause -
 *	  Transform the expression and make sure it is of type bigint.
 *	  Used for LIMIT and allied clauses.
 *
 * Note: as of Postgres 8.2, LIMIT expressions are expected to yield int8,
 * rather than int4 as before.
 *
 * constructName does not affect the semantics, but is used in error messages
 */
Node *
transformLimitClause(ParseState *pstate, Node *clause,
					 const char *constructName)
{
	Node	   *qual;

	if (clause == NULL)
		return NULL;

	qual = transformExpr(pstate, clause);

	qual = coerce_to_specific_type(pstate, qual, INT8OID, constructName);

	/*
	 * LIMIT can't refer to any vars or aggregates of the current query
	 */
	if (contain_vars_of_level(qual, 0))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
		/* translator: %s is name of a SQL construct, eg LIMIT */
				 errmsg("argument of %s must not contain variables",
						constructName),
				 parser_errposition(pstate,
									locate_var_of_level(qual, 0))));
	}
	if (checkExprHasAggs(qual))
	{
		ereport(ERROR,
				(errcode(ERRCODE_GROUPING_ERROR),
		/* translator: %s is name of a SQL construct, eg LIMIT */
				 errmsg("argument of %s must not contain aggregates",
						constructName),
				 parser_errposition(pstate,
									locate_agg_of_level(qual, 0))));
	}

	return qual;
}

/*
 * findListTargetlistEntries -
 *   Returns a list of targetlist entries matching the given node that
 *   corresponds to a grouping clause.
 *
 * This is similar to findTargetlistEntry(), but works for all
 * grouping clauses, including both the ordinary grouping clauses and the
 * grouping extension clauses, including ROLLUP, CUBE, and GROUPING
 * SETS.
 *
 * All targets will be added in the order that they appear in the grouping
 * clause.
 *
 * Param 'in_grpext' represents if 'node' is immediately enclosed inside
 * a GROUPING SET clause. For example,
 *     GROUPING SETS ( (a,b), ( (c,d), e ) )
 * or
 *     ROLLUP ( (a,b), ( (c,d), e ) )
 * '(a,b)' is immediately inside a GROUPING SET clause, while '(c,d)'
 * is not. '(c,d)' is immediately inside '( (c,d), e )', which is
 * considered as an ordinary grouping set.
 *
 * Note that RowExprs are handled differently with other expressions.
 * RowExprs themselves are not added into targetlists and result
 * list. However, all of their arguments will be added into
 * the targetlist. They will also be appended into the return
 * list if ignore_in_grpext is set or RowExprs do not appear
 * immediate inside a grouping extension.
 */
static List *findListTargetlistEntries(ParseState *pstate, Node *node,
									   List **tlist, bool in_grpext,
									   bool ignore_in_grpext,
                                       bool useSQL99)
{
	List *result_list = NIL;

	/*
	 * In GROUP BY clauses, empty grouping set () is supported as 'NIL'
	 * in the list. If this is the case, we simply skip it.
	 */
	if (node == NULL)
		return result_list;

	if (IsA(node, GroupingClause))
	{
		ListCell *gl;
		GroupingClause *gc = (GroupingClause*)node;

		foreach(gl, gc->groupsets)
		{
			List *subresult_list;

			subresult_list = findListTargetlistEntries(pstate, lfirst(gl),
													   tlist, true, 
                                                       ignore_in_grpext,
                                                       useSQL99);

			result_list = list_concat(result_list, subresult_list);
		}
	}

	/*
	 * In GROUP BY clause, we handle RowExpr specially here. When
	 * RowExprs appears immediately inside a grouping extension, we do
	 * not want to append the target entries for their arguments into
	 * result_list. This is because we do not want the order of
	 * these target entries in the result list from transformGroupClause()
	 * to be affected by ORDER BY.
	 *
	 * However, if ignore_in_grpext is set, we will always append
	 * these target enties.
	 */
	else if (IsA(node, RowExpr))
	{
		List *args = ((RowExpr *)node)->args;
		ListCell *lc;

		foreach (lc, args)
		{
			Node *rowexpr_arg = lfirst(lc);
			TargetEntry *tle;

            if (useSQL99)
                tle = findTargetlistEntrySQL99(pstate, rowexpr_arg, tlist);
            else
                tle = findTargetlistEntrySQL92(pstate, rowexpr_arg, tlist,
                                               GROUP_CLAUSE);

			/* If RowExpr does not appear immediately inside a GROUPING SETS,
			 * we append its targetlit to the given targetlist.
			 */
			if (ignore_in_grpext || !in_grpext)
				result_list = lappend(result_list, tle);
		}
	}

	else
	{
		TargetEntry *tle;

        if (useSQL99)
            tle = findTargetlistEntrySQL99(pstate, node, tlist);
        else
            tle = findTargetlistEntrySQL92(pstate, node, tlist, GROUP_CLAUSE);

		result_list = lappend(result_list, tle);
	}

	return result_list;
}

/*
 *	findTargetlistEntrySQL92 -
 *	  Returns the targetlist entry matching the given (untransformed) node.
 *	  If no matching entry exists, one is created and appended to the target
 *	  list as a "resjunk" node.
 *
 * This function supports the old SQL92 ORDER BY interpretation, where the
 * expression is an output column name or number.  If we fail to find a
 * match of that sort, we fall through to the SQL99 rules.  For historical
 * reasons, Postgres also allows this interpretation for GROUP BY, though
 * the standard never did.  However, for GROUP BY we prefer a SQL99 match.
 * This function is *not* used for WINDOW definitions.
 *
 * node		the ORDER BY, GROUP BY, or DISTINCT ON expression to be matched
 * tlist	the target list (passed by reference so we can append to it)
 * clause	identifies clause type being processed
 */
static TargetEntry *
findTargetlistEntrySQL92(ParseState *pstate, Node *node, List **tlist,
						 int clause)
{
	ListCell   *tl;

	/*----------
	 * Handle two special cases as mandated by the SQL92 spec:
	 *
	 * 1. Bare ColumnName (no qualifier or subscripts)
	 *	  For a bare identifier, we search for a matching column name
	 *	  in the existing target list.	Multiple matches are an error
	 *	  unless they refer to identical values; for example,
	 *	  we allow	SELECT a, a FROM table ORDER BY a
	 *	  but not	SELECT a AS b, b FROM table ORDER BY b
	 *	  If no match is found, we fall through and treat the identifier
	 *	  as an expression.
	 *	  For GROUP BY, it is incorrect to match the grouping item against
	 *	  targetlist entries: according to SQL92, an identifier in GROUP BY
	 *	  is a reference to a column name exposed by FROM, not to a target
	 *	  list column.	However, many implementations (including pre-7.0
	 *	  PostgreSQL) accept this anyway.  So for GROUP BY, we look first
	 *	  to see if the identifier matches any FROM column name, and only
	 *	  try for a targetlist name if it doesn't.  This ensures that we
	 *	  adhere to the spec in the case where the name could be both.
	 *	  DISTINCT ON isn't in the standard, so we can do what we like there;
	 *	  we choose to make it work like ORDER BY, on the rather flimsy
	 *	  grounds that ordinary DISTINCT works on targetlist entries.
	 *
	 * 2. IntegerConstant
	 *	  This means to use the n'th item in the existing target list.
	 *	  Note that it would make no sense to order/group/distinct by an
	 *	  actual constant, so this does not create a conflict with our
	 *	  extension to order/group by an expression.
	 *	  GROUP BY column-number is not allowed by SQL92, but since
	 *	  the standard has no other behavior defined for this syntax,
	 *	  we may as well accept this common extension.
	 *
	 * Note that pre-existing resjunk targets must not be used in either case,
	 * since the user didn't write them in his SELECT list.
	 *
	 * If neither special case applies, fall through to treat the item as
	 * an expression per SQL99.
	 *----------
	 */
	if (IsA(node, ColumnRef) &&
		list_length(((ColumnRef *) node)->fields) == 1)
	{
		char	   *name = strVal(linitial(((ColumnRef *) node)->fields));
		int			location = ((ColumnRef *) node)->location;

		if (clause == GROUP_CLAUSE)
		{
			/*
			 * In GROUP BY, we must prefer a match against a FROM-clause
			 * column to one against the targetlist.  Look to see if there is
			 * a matching column.  If so, fall through to use SQL99 rules.
			 * NOTE: if name could refer ambiguously to more than one column
			 * name exposed by FROM, colNameToVar will ereport(ERROR). That's
			 * just what we want here.
			 *
			 * Small tweak for 7.4.3: ignore matches in upper query levels.
			 * This effectively changes the search order for bare names to (1)
			 * local FROM variables, (2) local targetlist aliases, (3) outer
			 * FROM variables, whereas before it was (1) (3) (2). SQL92 and
			 * SQL99 do not allow GROUPing BY an outer reference, so this
			 * breaks no cases that are legal per spec, and it seems a more
			 * self-consistent behavior.
			 */
			if (colNameToVar(pstate, name, true, location) != NULL)
				name = NULL;
		}

		if (name != NULL)
		{
			TargetEntry *target_result = NULL;

			foreach(tl, *tlist)
			{
				TargetEntry *tle = (TargetEntry *) lfirst(tl);

				if (!tle->resjunk &&
					strcmp(tle->resname, name) == 0)
				{
					if (target_result != NULL)
					{
						if (!equal(target_result->expr, tle->expr))
							ereport(ERROR,
									(errcode(ERRCODE_AMBIGUOUS_COLUMN),

							/*------
							  translator: first %s is name of a SQL construct, eg ORDER BY */
									 errmsg("%s \"%s\" is ambiguous",
											clauseText[clause], name),
									 parser_errposition(pstate, location)));
					}
					else
						target_result = tle;
					/* Stay in loop to check for ambiguity */
				}
			}
			if (target_result != NULL)
				return target_result;	/* return the first match */
		}
	}
	if (IsA(node, A_Const))
	{
		Value	   *val = &((A_Const *) node)->val;
		int			location = ((A_Const *) node)->location;
		int			targetlist_pos = 0;
		int			target_pos;

		if (!IsA(val, Integer))
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
			/* translator: %s is name of a SQL construct, eg ORDER BY */
					 errmsg("non-integer constant in %s",
							clauseText[clause]),
					 parser_errposition(pstate, location)));

		target_pos = intVal(val);
		foreach(tl, *tlist)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(tl);

			if (!tle->resjunk)
			{
				if (++targetlist_pos == target_pos)
					return tle; /* return the unique match */
			}
		}
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
		/* translator: %s is name of a SQL construct, eg ORDER BY */
				 errmsg("%s position %d is not in select list",
						clauseText[clause], target_pos),
				 parser_errposition(pstate, location)));
	}

	/*
	 * Otherwise, we have an expression, so process it per SQL99 rules.
	 */
	return findTargetlistEntrySQL99(pstate, node, tlist);
}

/*
 *	findTargetlistEntrySQL99 -
 *	  Returns the targetlist entry matching the given (untransformed) node.
 *	  If no matching entry exists, one is created and appended to the target
 *	  list as a "resjunk" node.
 *
 * This function supports the SQL99 interpretation, wherein the expression
 * is just an ordinary expression referencing input column names.
 *
 * node		the ORDER BY, GROUP BY, etc expression to be matched
 * tlist	the target list (passed by reference so we can append to it)
 */
static TargetEntry *
findTargetlistEntrySQL99(ParseState *pstate, Node *node, List **tlist)
{
	TargetEntry *target_result;
	ListCell   *tl;
	Node	   *expr;

	/*
	 * Convert the untransformed node to a transformed expression, and search
	 * for a match in the tlist.  NOTE: it doesn't really matter whether there
	 * is more than one match.	Also, we are willing to match an existing
	 * resjunk target here, though the SQL92 cases above must ignore resjunk
	 * targets.
	 */
	expr = transformExpr(pstate, node);

	foreach(tl, *tlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(tl);

		if (equal(expr, tle->expr))
			return tle;
	}

	/*
	 * If no matches, construct a new target entry which is appended to the
	 * end of the target list.	This target is given resjunk = TRUE so that it
	 * will not be projected into the final tuple.
	 */
	target_result = transformTargetEntry(pstate, node, expr, NULL, true);

	*tlist = lappend(*tlist, target_result);

	return target_result;
}

/*
 * make_grouping_clause -
 *     Generate a new GroupingClause object from a given one.
 *
 * The given GroupingClause object generated by the parser contain either
 * GroupingClauses or expressions. The RowExpr expressions are handled
 * differently with other expressions -- they are transformed into a list
 * GroupingcClauses or GroupClauses, which is appended into the 'groupsets'
 * in the returning GroupingClause as a whole.
 *
 * The 'groupsets' in the returning GroupingClause may contain GroupClause,
 * GroupingClause, or List.
 *
 * Note that RowExprs are not added into the final targetlist.
 */
static GroupingClause *
make_grouping_clause(ParseState *pstate, GroupingClause *grpcl, List* targetList)
{
	GroupingClause *result;
	ListCell* gc;

	result = makeNode(GroupingClause);
	result->groupType = grpcl->groupType;
	result->groupsets = NIL;

	foreach (gc, grpcl->groupsets)
	{
		Node *node = (Node*)lfirst(gc);

		if (node == NULL)
		{
			result->groupsets =
				lappend(result->groupsets, list_make1(NIL));
		}

		else if (IsA(node, GroupingClause))
		{
			result->groupsets =
				lappend(result->groupsets,
						make_grouping_clause(pstate,
											 (GroupingClause*)node, targetList));
		}

		else if (IsA(node, RowExpr))
		{
			/*
			 * Since this RowExpr is immediately inside a GROUPING SETS, we convert it
			 * into a list of GroupClauses, which will be considered as a single
			 * grouping set in the planner.
			 */
			result->groupsets =
				transformRowExprToList(pstate, (RowExpr *)node,
									   result->groupsets, targetList);
		}

		else
		{
			TargetEntry *tle = findTargetlistEntrySQL92(pstate, node,
                                                        &targetList, GROUP_CLAUSE);
			Oid			sortop;
			Oid			eqop;

			/* Unlike ordinary grouping sets, we will create duplicate
			 * expression entries. For example, rollup(a,a) consists
			 * of three grouping sets "(a,a), (a), ()".
			 */
			get_sort_group_operators(exprType((Node *) tle->expr),
									 true, true, false,
									 &sortop, &eqop, NULL);
			result->groupsets =
				lappend(result->groupsets,
						make_group_clause(tle, targetList, eqop, sortop, false));
		}
	}

	return result;
}

static bool
grouping_rewrite_walker(Node *node, void *context)
{
	grouping_rewrite_ctx *ctx = (grouping_rewrite_ctx *)context;

	if (node == NULL)
		return false;

	if (IsA(node, A_Const))
	{
		return false;
	}
	else if(IsA(node, A_Expr))
	{
		/* could be seen inside an untransformed window clause */
		return false;
	}
	else if(IsA(node, ColumnRef))
	{
		/* could be seen inside an untransformed window clause */
		return false;
	}
	else if (IsA(node, TypeCast))
	{
		return false;
	}
	else if (IsA(node, GroupingFunc))
	{
		GroupingFunc *gf = (GroupingFunc *)node;
		ListCell *arg_lc;
		List *newargs = NIL;

		gf->ngrpcols = list_length(ctx->grp_tles);

		/*
		 * For each argument in gf->args, find its position in grp_tles,
		 * and increment its counts. Note that this is a O(n^2) algorithm,
		 * but it should not matter that much.
		 */
		foreach (arg_lc, gf->args)
		{
			long i = 0;
			Node *node = lfirst(arg_lc);
			ListCell *grp_lc = NULL;

			foreach (grp_lc, ctx->grp_tles)
			{
				TargetEntry *grp_tle = (TargetEntry *)lfirst(grp_lc);

				if (equal(grp_tle->expr, node))
					break;
				i++;
			}

			/* Find a column not in GROUP BY clause */
			if (grp_lc == NULL)
			{
				RangeTblEntry *rte;
				const char *attname;
				Var *var = (Var *) node;

				/* Do not allow expressions inside a grouping function. */
				if (IsA(node, RowExpr))
					ereport(ERROR,
							(errcode(ERRCODE_GROUPING_ERROR),
							 errmsg("row type can not be used inside a grouping function.")));

				if (!IsA(node, Var))
					ereport(ERROR,
							(errcode(ERRCODE_GROUPING_ERROR),
							 errmsg("expression in a grouping fuction does not appear in GROUP BY.")));

				Assert(IsA(node, Var));
				Assert(var->varno > 0);
				Assert(var->varno <= list_length(ctx->pstate->p_rtable));

				rte = rt_fetch(var->varno, ctx->pstate->p_rtable);
				attname = get_rte_attribute_name(rte, var->varattno);

				ereport(ERROR,
						(errcode(ERRCODE_GROUPING_ERROR),
						 errmsg("column \"%s\".\"%s\" is not in GROUP BY",
								rte->eref->aliasname, attname)));
			}

			newargs = lappend(newargs, makeInteger(i));
		}

		/* Free gf->args since we do not need it any more. */
		list_free_deep(gf->args);
		gf->args = newargs;
	}
	else if(IsA(node, SortBy))
	{
		/*
		 * When WindowClause leaves the main parser, partition and order
		 * clauses will be lists of SortBy structures. Process them here to
		 * avoid muddying up the expression_tree_walker().
		 */
		SortBy *s = (SortBy *)node;
		return grouping_rewrite_walker(s->node, context);
	}
	return expression_tree_walker(node, grouping_rewrite_walker, context);
}


/*
 *
 * create_group_clause
 * 	Order group clauses based on equivalent sort clauses to allow plans
 * 	with sort-based grouping implementation,
 *
 * 	given a list of a GROUP-BY tle's, return a list of group clauses in the same order
 * 	of matching ORDER-BY tle's
 *
 *  the remaining GROUP-BY tle's are stored in tlist_remainder
 *
 *
 */
static List *
create_group_clause(List *tlist_group, List *targetlist,
					List *sortClause, List **tlist_remainder)
{
	List	   *result = NIL;
	ListCell   *l;
	List *tlist = list_copy(tlist_group);

	/*
	 * Iterate through the ORDER BY clause. If we find a grouping element
	 * that matches the ORDER BY element, append the grouping element to the
	 * result set immediately. Otherwise, stop iterating. The effect of this
	 * is to look for a prefix of the ORDER BY list in the grouping clauses,
	 * and to move that prefix to the front of the GROUP BY.
	 */
	foreach(l, sortClause)
	{
		SortGroupClause *sc = (SortGroupClause *) lfirst(l);
		ListCell   *prev = NULL;
		ListCell   *tl = NULL;
		bool		found = false;

		foreach(tl, tlist)
		{
			Node        *node = (Node*)lfirst(tl);

			if (IsA(node, TargetEntry))
			{
				TargetEntry *tle = (TargetEntry *) lfirst(tl);

				if (!tle->resjunk &&
					sc->tleSortGroupRef == tle->ressortgroupref)
				{
					SortGroupClause *gc;

					tlist = list_delete_cell(tlist, tl, prev);

					/* Use the sort clause's sorting information */
					gc = make_group_clause(tle, targetlist,
										   sc->eqop, sc->sortop, sc->nulls_first);
					result = lappend(result, gc);
					found = true;
					break;
				}

				prev = tl;
			}
		}

		/* As soon as we've failed to match an ORDER BY element, stop */
		if (!found)
			break;
	}

	/* Save remaining GROUP-BY tle's */
	*tlist_remainder = tlist;

	return result;
}

static SortGroupClause *
make_group_clause(TargetEntry *tle, List *targetlist,
				  Oid eqop, Oid sortop, bool nulls_first)
{
	SortGroupClause *result;

	result = makeNode(SortGroupClause);
	result->tleSortGroupRef = assignSortGroupRef(tle, targetlist);
	result->eqop = eqop;
	result->sortop = sortop;
	result->nulls_first = nulls_first;
	return result;
}

/*
 * transformGroupClause -
 *	  transform a GROUP BY clause
 *
 * The given GROUP BY clause can contain both GroupClauses and
 * GroupingClauses.
 *
 * GROUP BY items will be added to the targetlist (as resjunk columns)
 * if not already present, so the targetlist must be passed by reference.
 */
List *
transformGroupClause(ParseState *pstate, List *grouplist,
					 List **targetlist, List *sortClause,
                     bool useSQL99)
{
	List	   *result = NIL;
	List	   *tle_list = NIL;
	ListCell   *l;
	List       *reorder_grouplist = NIL;

	/* Preprocess the grouping clause, lookup TLEs */
	foreach(l, grouplist)
	{
		List        *tl;
		ListCell    *tl_cell;
		TargetEntry *tle;
		Oid			restype;
		Node        *node;

		node = (Node*)lfirst(l);
		tl = findListTargetlistEntries(pstate, node, targetlist, false, false, 
                                       useSQL99);

		foreach(tl_cell, tl)
		{
			tle = (TargetEntry*)lfirst(tl_cell);

			/* if tlist item is an UNKNOWN literal, change it to TEXT */
			restype = exprType((Node *) tle->expr);

			if (restype == UNKNOWNOID)
				tle->expr = (Expr *) coerce_type(pstate, (Node *) tle->expr,
												 restype, TEXTOID, -1,
												 COERCION_IMPLICIT,
												 COERCE_IMPLICIT_CAST,
												 -1);

			/*
			 * The tle_list will be used to match with the ORDER by element below.
			 * We only append the tle to tle_list when node is not a
			 * GroupingClause or tle->expr is not a RowExpr.
			 */
			 if (node != NULL &&
				 !IsA(node, GroupingClause) &&
				 !IsA(tle->expr, RowExpr))
				 tle_list = lappend(tle_list, tle);
		}
	}

	/* create first group clauses based on sort clauses */
	List *tle_list_remainder = NIL;
	result = create_group_clause(tle_list,
								*targetlist,
								sortClause,
								&tle_list_remainder);

	/*
	 * Now add all remaining elements of the GROUP BY list to the result list.
	 * The result list is a list of GroupClauses and/or GroupingClauses.
	 * In each grouping set, all GroupClauses will appear in front of
	 * GroupingClauses. See the following GROUP BY clause:
	 *
	 *   GROUP BY ROLLUP(b,c),a, ROLLUP(e,d)
	 *
	 * the result list can be roughly represented as follows.
	 *
	 *    GroupClause(a),
	 *    GroupingClause(ROLLUP,groupsets(GroupClause(b),GroupClause(c))),
	 *    GroupingClause(CUBE,groupsets(GroupClause(e),GroupClause(d)))
	 *
	 *   XXX: the above transformation doesn't make sense -gs
	 */
	reorder_grouplist = reorderGroupList(grouplist);

	foreach(l, reorder_grouplist)
	{
		Node *node = (Node*) lfirst(l);

		if (node == NULL) /* the empty grouping set */
			result = list_concat(result, list_make1(NIL));

		else if (IsA(node, GroupingClause))
		{
			GroupingClause *tmp = make_grouping_clause(pstate,
													   (GroupingClause*)node,
													   *targetlist);
			result = lappend(result, tmp);
		}

		else if (IsA(node, RowExpr))
		{
			/* The top level RowExprs are handled differently with other expressions.
			 * We convert each argument into GroupClause and append them
			 * one by one into 'result' list.
			 *
			 * Note that RowExprs are not added into the final targetlist.
			 */
			result =
				transformRowExprToGroupClauses(pstate, (RowExpr *)node,
											   result, *targetlist);
		}

		else
		{
			TargetEntry *tle;
			SortGroupClause *gc;
			Oid			sortop;
			Oid			eqop;

			if (useSQL99)
				tle = findTargetlistEntrySQL99(pstate, node, targetlist);
			else
				tle = findTargetlistEntrySQL92(pstate, node, targetlist,
											   GROUP_CLAUSE);

			/*
			 * Avoid making duplicate grouplist entries.  Note that we don't
			 * enforce a particular sortop here.  Along with the copying of sort
			 * information above, this means that if you write something like
			 * "GROUP BY foo ORDER BY foo USING <<<", the GROUP BY operation
			 * silently takes on the equality semantics implied by the ORDER BY.
			 */
			if (targetIsInSortList(tle, InvalidOid, result))
				continue;

			get_sort_group_operators(exprType((Node *) tle->expr),
									 true, true, false,
									 &sortop, &eqop, NULL);
			gc = make_group_clause(tle, *targetlist, eqop, sortop, false);
			result = lappend(result, gc);
		}
	}

	/* We're doing extended grouping for both ordinary grouping and grouping
	 * extensions.
	 */
	{
		ListCell *lc;

		/*
		 * Find all unique target entries appeared in reorder_grouplist.
		 * We stash them in the ParseState, to be processed later by
		 * processExtendedGrouping().
		 */
		foreach (lc, reorder_grouplist)
		{
			pstate->p_grp_tles = list_concat_unique(
				pstate->p_grp_tles,
				findListTargetlistEntries(pstate, lfirst(lc),
										  targetlist, false, true, useSQL99));
		}
	}

	list_free(tle_list);
	list_free(tle_list_remainder);
	freeGroupList(reorder_grouplist);

	return result;
}

void
processExtendedGrouping(ParseState *pstate,
						Node *havingQual,
						List *windowClause,
						List *targetlist)
{
	grouping_rewrite_ctx ctx;

	/*
	 * For each GROUPING function, check if its argument(s) appear in the
	 * GROUP BY clause. We also set ngrpcols, nargs and grpargs values for
	 * each GROUPING function here. These values are used together with
	 * GROUPING_ID to calculate the final value for each GROUPING function
	 * in the executor.
	 */
	ctx.pstate = pstate;
	ctx.grp_tles = pstate->p_grp_tles;
	pstate->p_grp_tles = NIL;

	expression_tree_walker((Node *) targetlist, grouping_rewrite_walker,
						   (void *)&ctx);

	/*
	 * The expression might be present in a window clause as well
	 * so process those.
	 */
	expression_tree_walker((Node *) windowClause,
						   grouping_rewrite_walker, (void *)&ctx);

	/*
	 * The expression might be present in the having clause as well.
	 */
	expression_tree_walker(havingQual,
						   grouping_rewrite_walker, (void *)&ctx);
}

/*
 * transformSortClause -
 *	  transform an ORDER BY clause
 *
 * ORDER BY items will be added to the targetlist (as resjunk columns)
 * if not already present, so the targetlist must be passed by reference.
 *
 * This is also used for window and aggregate ORDER BY clauses (which act
 * almost the same, but are always interpreted per SQL99 rules).
 */
List *
transformSortClause(ParseState *pstate,
					List *orderlist,
					List **targetlist,
					bool resolveUnknown,
					bool useSQL99)
{
	List	   *sortlist = NIL;
	ListCell   *olitem;

	foreach(olitem, orderlist)
	{
		SortBy	   *sortby = (SortBy *) lfirst(olitem);
		TargetEntry *tle;

		if (useSQL99)
			tle = findTargetlistEntrySQL99(pstate, sortby->node, targetlist);
		else
			tle = findTargetlistEntrySQL92(pstate, sortby->node, targetlist,
										   ORDER_CLAUSE);

		sortlist = addTargetToSortList(pstate, tle,
									   sortlist, *targetlist, sortby,
									   resolveUnknown);
	}

	return sortlist;
}

/*
 * transformWindowDefinitions -
 *		transform window definitions (WindowDef to WindowClause)
 *
 * There's a fair bit to do here: column references in the PARTITION and
 * ORDER clauses must be valid; ORDER clause must present if the function
 * requires; the frame clause must be checked to ensure that the function
 * supports framing, that the framed column is of the right type, that the
 * offset is sane, that the start and end of the frame are sane.
 * Then we translate it to use the proper parse nodes for the respective
 * part of the clause.
 */
List *
transformWindowDefinitions(ParseState *pstate,
						   List *windowdefs,
						   List **targetlist)
{
	List	   *result = NIL;
	Index		winref = 0;
	ListCell   *lc;

	/*
	 * We have two lists of window specs: one in the ParseState -- put there
	 * when we find the OVER(...) clause in the targetlist and the other
	 * is windowClause, a list of named window clauses. So, we concatenate
	 * them together.
	 *
	 * Note that we're careful place those found in the target list at
	 * the end because the spec might refer to a named clause and we'll
	 * after to know about those first.
	 */
	foreach(lc, windowdefs)
	{
		WindowDef  *windef = lfirst(lc);
		WindowClause *refwc = NULL;
		List	   *partitionClause;
		List	   *orderClause;
		WindowClause *wc;

		winref++;

		/*
		 * Check for duplicate window names.
		 */
		if (windef->name &&
			findWindowClause(result, windef->name) != NULL)
			ereport(ERROR,
					(errcode(ERRCODE_WINDOWING_ERROR),
					 errmsg("window \"%s\" is already defined", windef->name),
					 parser_errposition(pstate, windef->location)));

		/*
		 * If it references a previous window, look that up.
		 */
		if (windef->refname)
		{
			refwc = findWindowClause(result, windef->refname);
			if (refwc == NULL)
				ereport(ERROR,
						(errcode(ERRCODE_UNDEFINED_OBJECT),
						 errmsg("window \"%s\" does not exist",
								windef->refname),
						 parser_errposition(pstate, windef->location)));
		}

		/*
		 * Transform PARTITION and ORDER specs, if any.  These are treated
		 * almost exactly like top-level GROUP BY and ORDER BY clauses,
		 * including the special handling of nondefault operator semantics.
		 */
		orderClause =
			transformSortClause(pstate,
								windef->orderClause,
								targetlist,
								true /* fix unknowns */ ,
								true /* force SQL99 rules */ );
		partitionClause =
			transformSortClause(pstate,
								windef->partitionClause,
								targetlist,
								true /* fix unknowns */ ,
								true /* force SQL99 rules */ );

		/*
		 * And prepare the new WindowClause.
		 */
		wc = makeNode(WindowClause);
		wc->name = windef->name;
		wc->refname = windef->refname;

		/*
		 * Per spec, a windowdef that references a previous one copies the
		 * previous partition clause (and mustn't specify its own).  It can
		 * specify its own ordering clause, but only if the previous one had
		 * none.  It always specifies its own frame clause, and the previous
		 * one must not have a frame clause.  Yeah, it's bizarre that each of
		 * these cases works differently, but SQL:2008 says so; see 7.11
		 * <window clause> syntax rule 10 and general rule 1.  The frame
		 * clause rule is especially bizarre because it makes "OVER foo"
		 * different from "OVER (foo)", and requires the latter to throw an
		 * error if foo has a nondefault frame clause.  Well, ours not to
		 * reason why, but we do go out of our way to throw a useful error
		 * message for such cases.
		 */
		if (refwc)
		{
			if (partitionClause)
				ereport(ERROR,
						(errcode(ERRCODE_WINDOWING_ERROR),
						 errmsg("cannot override PARTITION BY clause of window \"%s\"",
								windef->refname),
						 parser_errposition(pstate, windef->location)));
			wc->partitionClause = copyObject(refwc->partitionClause);
		}
		else
			wc->partitionClause = partitionClause;
		if (refwc)
		{
			if (orderClause && refwc->orderClause)
				ereport(ERROR,
						(errcode(ERRCODE_WINDOWING_ERROR),
						 errmsg("cannot override ORDER BY clause of window \"%s\"",
								windef->refname),
						 parser_errposition(pstate, windef->location)));
			if (orderClause)
			{
				wc->orderClause = orderClause;
				wc->copiedOrder = false;
			}
			else
			{
				wc->orderClause = copyObject(refwc->orderClause);
				wc->copiedOrder = true;
			}
		}
		else
		{
			wc->orderClause = orderClause;
			wc->copiedOrder = false;
		}
		if (refwc && refwc->frameOptions != FRAMEOPTION_DEFAULTS)
		{
			/*
			 * Use this message if this is a WINDOW clause, or if it's an OVER
			 * clause that includes ORDER BY or framing clauses.  (We already
			 * rejected PARTITION BY above, so no need to check that.)
			 */
			if (windef->name ||
				orderClause || windef->frameOptions != FRAMEOPTION_DEFAULTS)
				ereport(ERROR,
						(errcode(ERRCODE_WINDOWING_ERROR),
						 errmsg("cannot copy window \"%s\" because it has a frame clause",
								windef->refname),
						 parser_errposition(pstate, windef->location)));
			/* Else this clause is just OVER (foo), so say this: */
			ereport(ERROR,
					(errcode(ERRCODE_WINDOWING_ERROR),
					 errmsg("cannot copy window \"%s\" because it has a frame clause",
							windef->refname),
					 errhint("Omit the parentheses in this OVER clause."),
					 parser_errposition(pstate, windef->location)));
		}

		/*
		 * Finally, process the framing clause. parseProcessWindFunc() will
		 * have picked up window functions that do not support framing.
		 *
		 * What we do need to do is the following:
		 * - If BETWEEN has been specified, the trailing bound is not
		 *   UNBOUNDED FOLLOWING; the leading bound is not UNBOUNDED
		 *   PRECEDING; if the first bound specifies CURRENT ROW, the
		 *   second bound shall not specify a PRECEDING bound; if the
		 *   first bound specifies a FOLLOWING bound, the second bound
		 *   shall not specify a PRECEDING or CURRENT ROW bound.
		 *
		 * - If the user did not specify BETWEEN, the bound is assumed to be
		 *   a trailing bound and the leading bound is set to CURRENT ROW.
		 *   We're careful not to set is_between here because the user did not
		 *   specify it.
		 *
		 * - If RANGE is specified: the ORDER BY clause of the window spec
		 *   may specify only one column; the type of that column must support
		 *   +/- <integer> operations and must be merge-joinable.
		 */

		wc->frameOptions = windef->frameOptions;
		wc->winref = winref;
		/* Process frame offset expressions */
		if ((windef->frameOptions & FRAMEOPTION_NONDEFAULT) != 0)
		{
			/*
			 * Framing is only supported on specifications with an ordering
			 * clause.
			 */
			if (!wc->orderClause)
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("window specifications with a framing clause must have an ORDER BY clause"),
						 parser_errposition(pstate, windef->location)));

			wc->startOffset = transformFrameOffset(pstate, wc->frameOptions,
												   windef->startOffset, wc->orderClause,
												   *targetlist,
												   (windef->frameOptions & FRAMEOPTION_START_VALUE_FOLLOWING) != 0,
												   windef->location);
			wc->endOffset = transformFrameOffset(pstate, wc->frameOptions,
												 windef->endOffset, wc->orderClause,
												 *targetlist,
												 (windef->frameOptions & FRAMEOPTION_END_VALUE_FOLLOWING) != 0,
												 windef->location);
		}

		/* finally, check function restriction with this spec. */
		winref_checkspec(pstate, *targetlist, winref,
						 PointerIsValid(wc->orderClause),
						 wc->frameOptions != FRAMEOPTION_DEFAULTS);

		result = lappend(result, wc);
	}

	/* If there are no window functions in the targetlist,
	 * forget the window clause.
	 */
	if (!pstate->p_hasWindowFuncs)
		pstate->p_windowdefs = NIL;

	return result;
}

/*
 * transformDistinctToGroupBy
 *
 * 		transform DISTINCT clause to GROUP-BY clause
 */
List *
transformDistinctToGroupBy(ParseState *pstate, List **targetlist,
							List **sortClause, List **groupClause)
{
	List *group_tlist = list_copy(*targetlist);

	/*
	 * create first group clauses based on matching sort clauses, if any
	 */
	List *group_tlist_remainder = NIL;
	List *group_clause_list = create_group_clause(group_tlist,
												*targetlist,
												*sortClause,
												&group_tlist_remainder);

	if (list_length(group_tlist_remainder) > 0)
	{
		/*
		 * append remaining group clauses to the end of group clause list
		 */
		ListCell *lc = NULL;

		foreach(lc, group_tlist_remainder)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(lc);
			if (!tle->resjunk)
			{
				SortBy sortby;

				sortby.type = T_SortBy;
				sortby.sortby_dir = SORTBY_DEFAULT;
				sortby.sortby_nulls = SORTBY_NULLS_DEFAULT;
				sortby.useOp = NIL;
				sortby.location = -1;
				sortby.node = (Node *) tle->expr;
				group_clause_list = addTargetToSortList(pstate, tle,
														group_clause_list, *targetlist,
														&sortby, true);
			}
		}
	}

	*groupClause = group_clause_list;

	list_free(group_tlist);
	list_free(group_tlist_remainder);

	/*
	 * return empty distinct list, since we have created a grouping clause to do the job
	 */
	return NIL;
}


/*
 * transformDistinctClause -
 *	  transform a DISTINCT clause
 *
 * Since we may need to add items to the query's targetlist, that list
 * is passed by reference.
 *
 * As with GROUP BY, we absorb the sorting semantics of ORDER BY as much as
 * possible into the distinctClause.  This avoids a possible need to re-sort,
 * and allows the user to choose the equality semantics used by DISTINCT,
 * should she be working with a datatype that has more than one equality
 * operator.
 */
List *
transformDistinctClause(ParseState *pstate,
						List **targetlist, List *sortClause)
{
	List	   *result = NIL;
	ListCell   *slitem;
	ListCell   *tlitem;

	/*
	 * The distinctClause should consist of all ORDER BY items followed by all
	 * other non-resjunk targetlist items.  There must not be any resjunk
	 * ORDER BY items --- that would imply that we are sorting by a value that
	 * isn't necessarily unique within a DISTINCT group, so the results
	 * wouldn't be well-defined.  This construction ensures we follow the rule
	 * that sortClause and distinctClause match; in fact the sortClause will
	 * always be a prefix of distinctClause.
	 *
	 * Note a corner case: the same TLE could be in the ORDER BY list multiple
	 * times with different sortops.  We have to include it in the
	 * distinctClause the same way to preserve the prefix property. The net
	 * effect will be that the TLE value will be made unique according to both
	 * sortops.
	 */
	foreach(slitem, sortClause)
	{
		SortGroupClause *scl = (SortGroupClause *) lfirst(slitem);
		TargetEntry *tle = get_sortgroupclause_tle(scl, *targetlist);

		if (tle->resjunk)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
					 errmsg("for SELECT DISTINCT, ORDER BY expressions must appear in select list"),
					 parser_errposition(pstate,
										exprLocation((Node *) tle->expr))));
		result = lappend(result, copyObject(scl));
	}

	/*
	 * Now add any remaining non-resjunk tlist items, using default sort/group
	 * semantics for their data types.
	 */
	foreach(tlitem, *targetlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(tlitem);

		if (tle->resjunk)
			continue;			/* ignore junk */
		result = addTargetToGroupList(pstate, tle,
									  result, *targetlist,
									  exprLocation((Node *) tle->expr),
									  true);
	}

	return result;
}

/*
 * transformDistinctOnClause -
 *	  transform a DISTINCT ON clause
 *
 * Since we may need to add items to the query's targetlist, that list
 * is passed by reference.
 *
 * As with GROUP BY, we absorb the sorting semantics of ORDER BY as much as
 * possible into the distinctClause.  This avoids a possible need to re-sort,
 * and allows the user to choose the equality semantics used by DISTINCT,
 * should she be working with a datatype that has more than one equality
 * operator.
 */
List *
transformDistinctOnClause(ParseState *pstate, List *distinctlist,
						  List **targetlist, List *sortClause)
{
	List	   *result = NIL;
	List	   *sortgrouprefs = NIL;
	bool		skipped_sortitem;
	ListCell   *lc;
	ListCell   *lc2;

	/*
	 * Add all the DISTINCT ON expressions to the tlist (if not already
	 * present, they are added as resjunk items).  Assign sortgroupref numbers
	 * to them, and make a list of these numbers.  (NB: we rely below on the
	 * sortgrouprefs list being one-for-one with the original distinctlist.
	 * Also notice that we could have duplicate DISTINCT ON expressions and
	 * hence duplicate entries in sortgrouprefs.)
	 */
	foreach(lc, distinctlist)
	{
		Node	   *dexpr = (Node *) lfirst(lc);
		int			sortgroupref;
		TargetEntry *tle;

		tle = findTargetlistEntrySQL92(pstate, dexpr, targetlist,
									   DISTINCT_ON_CLAUSE);
		sortgroupref = assignSortGroupRef(tle, *targetlist);
		sortgrouprefs = lappend_int(sortgrouprefs, sortgroupref);
	}

	/*
	 * If the user writes both DISTINCT ON and ORDER BY, adopt the sorting
	 * semantics from ORDER BY items that match DISTINCT ON items, and also
	 * adopt their column sort order.  We insist that the distinctClause and
	 * sortClause match, so throw error if we find the need to add any more
	 * distinctClause items after we've skipped an ORDER BY item that wasn't
	 * in DISTINCT ON.
	 */
	skipped_sortitem = false;
	foreach(lc, sortClause)
	{
		SortGroupClause *scl = (SortGroupClause *) lfirst(lc);

		if (list_member_int(sortgrouprefs, scl->tleSortGroupRef))
		{
			if (skipped_sortitem)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
						 errmsg("SELECT DISTINCT ON expressions must match initial ORDER BY expressions")));
			else
				result = lappend(result, copyObject(scl));
		}
		else
			skipped_sortitem = true;
	}

	/*
	 * Now add any remaining DISTINCT ON items, using default sort/group
	 * semantics for their data types.  (Note: this is pretty questionable; if
	 * the ORDER BY list doesn't include all the DISTINCT ON items and more
	 * besides, you certainly aren't using DISTINCT ON in the intended way,
	 * and you probably aren't going to get consistent results.  It might be
	 * better to throw an error or warning here.  But historically we've
	 * allowed it, so keep doing so.)
	 */
	forboth(lc, distinctlist, lc2, sortgrouprefs)
	{
		Node	   *dexpr = (Node *) lfirst(lc);
		int			sortgroupref = lfirst_int(lc2);
		TargetEntry *tle = get_sortgroupref_tle(sortgroupref, *targetlist);

		if (targetIsInSortList(tle, InvalidOid, result))
			continue;			/* already in list (with some semantics) */
		if (skipped_sortitem)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_COLUMN_REFERENCE),
					 errmsg("SELECT DISTINCT ON expressions must match initial ORDER BY expressions"),
					 parser_errposition(pstate, exprLocation(dexpr))));
		result = addTargetToGroupList(pstate, tle,
									  result, *targetlist,
									  exprLocation(dexpr),
									  true);
	}

	return result;
}

/*
 * transformScatterClause -
 *	  transform a SCATTER BY clause
 *
 * SCATTER BY items will be added to the targetlist (as resjunk columns)
 * if not already present, so the targetlist must be passed by reference.
 *
 */
List *
transformScatterClause(ParseState *pstate,
					   List *scatterlist,
					   List **targetlist)
{
	List	   *outlist = NIL;
	ListCell   *olitem;

	/* Special case handling for SCATTER RANDOMLY */
	if (list_length(scatterlist) == 1 && linitial(scatterlist) == NULL)
		return list_make1(NULL);
	
	/* preprocess the scatter clause, lookup TLEs */
	foreach(olitem, scatterlist)
	{
		Node			*node = lfirst(olitem);
		TargetEntry		*tle;

		tle = findTargetlistEntrySQL99(pstate, node, targetlist);

		/* coerce unknown to text */
		if (exprType((Node *) tle->expr) == UNKNOWNOID)
		{
			tle->expr = (Expr *) coerce_type(pstate, (Node *) tle->expr,
											 UNKNOWNOID, TEXTOID, -1,
											 COERCION_IMPLICIT,
											 COERCE_IMPLICIT_CAST,
											 -1);
		}

		outlist = lappend(outlist, tle->expr);
	}
	return outlist;
}

/*
 * addTargetToSortList
 *		If the given targetlist entry isn't already in the SortGroupClause
 *		list, add it to the end of the list, using the given sort ordering
 *		info.
 *
 * If resolveUnknown is TRUE, convert TLEs of type UNKNOWN to TEXT.  If not,
 * do nothing (which implies the search for a sort operator will fail).
 * pstate should be provided if resolveUnknown is TRUE, but can be NULL
 * otherwise.
 *
 * Returns the updated SortGroupClause list.
 */
List *
addTargetToSortList(ParseState *pstate, TargetEntry *tle,
					List *sortlist, List *targetlist, SortBy *sortby,
					bool resolveUnknown)
{
	Oid			restype = exprType((Node *) tle->expr);
	Oid			sortop;
	Oid			eqop;
	bool		reverse;
	int			location;
	ParseCallbackState pcbstate;

	/* if tlist item is an UNKNOWN literal, change it to TEXT */
	if (restype == UNKNOWNOID && resolveUnknown)
	{
		tle->expr = (Expr *) coerce_type(pstate, (Node *) tle->expr,
										 restype, TEXTOID, -1,
										 COERCION_IMPLICIT,
										 COERCE_IMPLICIT_CAST,
										 exprLocation((Node *) tle->expr));
		restype = TEXTOID;
	}

	/*
	 * Rather than clutter the API of get_sort_group_operators and the other
	 * functions we're about to use, make use of error context callback to
	 * mark any error reports with a parse position.  We point to the operator
	 * location if present, else to the expression being sorted.  (NB: use
	 * the original untransformed expression here; the TLE entry might well
	 * point at a duplicate expression in the regular SELECT list.)
	 */
	location = sortby->location;
	if (location < 0)
		location = exprLocation(sortby->node);
	setup_parser_errposition_callback(&pcbstate, pstate, location);

	/* determine the sortop */
	switch (sortby->sortby_dir)
	{
		case SORTBY_DEFAULT:
		case SORTBY_ASC:
			get_sort_group_operators(restype,
									 true, true, false,
									 &sortop, &eqop, NULL);
			reverse = false;
			break;
		case SORTBY_DESC:
			get_sort_group_operators(restype,
									 false, true, true,
									 NULL, &eqop, &sortop);
			reverse = true;
			break;
		case SORTBY_USING:
			Assert(sortby->useOp != NIL);
			sortop = compatible_oper_opid(sortby->useOp,
										  restype,
										  restype,
										  false);

			/*
			 * Verify it's a valid ordering operator, fetch the corresponding
			 * equality operator, and determine whether to consider it like
			 * ASC or DESC.
			 */
			eqop = get_equality_op_for_ordering_op(sortop, &reverse);
			if (!OidIsValid(eqop))
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					   errmsg("operator %s is not a valid ordering operator",
							  strVal(llast(sortby->useOp))),
						 parser_errposition(pstate, sortby->location),
						 errhint("Ordering operators must be \"<\" or \">\" members of btree operator families.")));
			break;
		default:
			elog(ERROR, "unrecognized sortby_dir: %d", sortby->sortby_dir);
			sortop = InvalidOid;	/* keep compiler quiet */
			eqop = InvalidOid;
			reverse = false;
			break;
	}

	cancel_parser_errposition_callback(&pcbstate);

	/* avoid making duplicate sortlist entries */
	if (!targetIsInSortList(tle, sortop, sortlist))
	{
		SortGroupClause *sortcl = makeNode(SortGroupClause);

		sortcl->tleSortGroupRef = assignSortGroupRef(tle, targetlist);

		sortcl->eqop = eqop;
		sortcl->sortop = sortop;

		switch (sortby->sortby_nulls)
		{
			case SORTBY_NULLS_DEFAULT:
				/* NULLS FIRST is default for DESC; other way for ASC */
				sortcl->nulls_first = reverse;
				break;
			case SORTBY_NULLS_FIRST:
				sortcl->nulls_first = true;
				break;
			case SORTBY_NULLS_LAST:
				sortcl->nulls_first = false;
				break;
			default:
				elog(ERROR, "unrecognized sortby_nulls: %d",
					 sortby->sortby_nulls);
				break;
		}

		sortlist = lappend(sortlist, sortcl);
	}

	return sortlist;
}

/*
 * addTargetToGroupList
 *		If the given targetlist entry isn't already in the SortGroupClause
 *		list, add it to the end of the list, using default sort/group
 *		semantics.
 *
 * This is very similar to addTargetToSortList, except that we allow the
 * case where only a grouping (equality) operator can be found, and that
 * the TLE is considered "already in the list" if it appears there with any
 * sorting semantics.
 *
 * If resolveUnknown is TRUE, convert TLEs of type UNKNOWN to TEXT.  If not,
 * do nothing (which implies the search for an equality operator will fail).
 * pstate should be provided if resolveUnknown is TRUE, but can be NULL
 * otherwise.
 *
 * Returns the updated SortGroupClause list.
 */
static List *
addTargetToGroupList(ParseState *pstate, TargetEntry *tle,
					 List *grouplist, List *targetlist, int location,
					 bool resolveUnknown)
{
	Oid			restype = exprType((Node *) tle->expr);
	Oid			sortop;
	Oid			eqop;

	/* if tlist item is an UNKNOWN literal, change it to TEXT */
	if (restype == UNKNOWNOID && resolveUnknown)
	{
		tle->expr = (Expr *) coerce_type(pstate, (Node *) tle->expr,
										 restype, TEXTOID, -1,
										 COERCION_IMPLICIT,
										 COERCE_IMPLICIT_CAST,
										 -1);
		restype = TEXTOID;
	}

	/* avoid making duplicate grouplist entries */
	if (!targetIsInSortList(tle, InvalidOid, grouplist))
	{
		SortGroupClause *grpcl = makeNode(SortGroupClause);
		ParseCallbackState pcbstate;

		setup_parser_errposition_callback(&pcbstate, pstate, location);

		/* determine the eqop and optional sortop */
		get_sort_group_operators(restype,
								 false, true, false,
								 &sortop, &eqop, NULL);

		cancel_parser_errposition_callback(&pcbstate);

		grpcl->tleSortGroupRef = assignSortGroupRef(tle, targetlist);
		grpcl->eqop = eqop;
		grpcl->sortop = sortop;
		grpcl->nulls_first = false;		/* OK with or without sortop */

		grouplist = lappend(grouplist, grpcl);
	}

	return grouplist;
}

/*
 * assignSortGroupRef
 *	  Assign the targetentry an unused ressortgroupref, if it doesn't
 *	  already have one.  Return the assigned or pre-existing refnumber.
 *
 * 'tlist' is the targetlist containing (or to contain) the given targetentry.
 */
Index
assignSortGroupRef(TargetEntry *tle, List *tlist)
{
	Index		maxRef;
	ListCell   *l;

	if (tle->ressortgroupref)	/* already has one? */
		return tle->ressortgroupref;

	/* easiest way to pick an unused refnumber: max used + 1 */
	maxRef = 0;
	foreach(l, tlist)
	{
		Index		ref = ((TargetEntry *) lfirst(l))->ressortgroupref;

		if (ref > maxRef)
			maxRef = ref;
	}
	tle->ressortgroupref = maxRef + 1;
	return tle->ressortgroupref;
}

/*
 * targetIsInSortList
 *		Is the given target item already in the sortlist?
 *		If sortop is not InvalidOid, also test for a match to the sortop.
 *
 * It is not an oversight that this function ignores the nulls_first flag.
 * We check sortop when determining if an ORDER BY item is redundant with
 * earlier ORDER BY items, because it's conceivable that "ORDER BY
 * foo USING <, foo USING <<<" is not redundant, if <<< distinguishes
 * values that < considers equal.  We need not check nulls_first
 * however, because a lower-order column with the same sortop but
 * opposite nulls direction is redundant.  Also, we can consider
 * ORDER BY foo ASC, foo DESC redundant, so check for a commutator match.
 *
 * Works for both ordering and grouping lists (sortop would normally be
 * InvalidOid when considering grouping).  Note that the main reason we need
 * this routine (and not just a quick test for nonzeroness of ressortgroupref)
 * is that a TLE might be in only one of the lists.
 *
 * Any GroupingClauses in the list will be skipped during comparison.
 */
bool
targetIsInSortList(TargetEntry *tle, Oid sortop, List *sortgroupList)
{
	Index		ref = tle->ressortgroupref;
	ListCell   *l;

	/* no need to scan list if tle has no marker */
	if (ref == 0)
		return false;

	foreach(l, sortgroupList)
	{
		Node *node = (Node *) lfirst(l);

		/* Skip the empty grouping set */
		if (node == NULL)
			continue;

		if (IsA(node, SortGroupClause))
		{
			SortGroupClause *scl = (SortGroupClause *) node;

			if (scl->tleSortGroupRef == ref &&
				(sortop == InvalidOid ||
				 sortop == scl->sortop ||
				 sortop == get_commutator(scl->sortop)))
				return true;
		}
	}
	return false;
}

/*
 * Given a sort group reference, find the TargetEntry for it.
 */

static TargetEntry *
getTargetBySortGroupRef(Index ref, List *tl)
{
	ListCell *tmp;

	foreach(tmp, tl)
	{
		TargetEntry *te = (TargetEntry *)lfirst(tmp);

		if (te->ressortgroupref == ref)
			return te;
	}
	return NULL;
}

/*
 * Re-order entries in a given GROUP BY list, which includes expressions or
 * grouping extension clauses, such as ROLLUP, CUBE, GROUPING_SETS.
 *
 * In each grouping set level, all non grouping extension clauses (or
 * expressions) will appear in front of grouping extension clauses.
 * See the following GROUP BY clause:
 *
 *   GROUP BY ROLLUP(b,c),a, ROLLUP(e,d)
 *
 * The re-ordered list is like below:
 *
 *   a,ROLLUP(b,c), ROLLUP(e,d)
 *
 * We make a fresh copy for each entries in the result list. The caller
 * needs to free the list eventually.
 */
static List *
reorderGroupList(List *grouplist)
{
	List *result = NIL;
	ListCell *gl;
	List *sub_list = NIL;

	foreach(gl, grouplist)
	{
		Node *node = (Node*)lfirst(gl);

		if (node == NULL)
		{
			/* Append an empty set. */
			result = list_concat(result, list_make1(NIL));
		}

		else if (IsA(node, GroupingClause))
		{
			GroupingClause *gc = (GroupingClause *)node;
			GroupingClause *new_gc = makeNode(GroupingClause);

			new_gc->groupType = gc->groupType;
			new_gc->groupsets = reorderGroupList(gc->groupsets);

			sub_list = lappend(sub_list, new_gc);
		}
		else
		{
			Node *new_node = (Node *)copyObject(node);
			result = lappend(result, new_node);
		}
	}

	result = list_concat(result, sub_list);
	return result;
}

/*
 * Free all the cells of the group list, the list itself and all the
 * objects pointed-by the cells of the list. The element in
 * the group list can be NULL.
 */
static void
freeGroupList(List *grouplist)
{
	ListCell *gl;

	if (grouplist == NULL)
		return;

	foreach (gl, grouplist)
	{
		Node *node = (Node *)lfirst(gl);
		if (node == NULL)
			continue;
		if (IsA(node, GroupingClause))
		{
			GroupingClause *gc = (GroupingClause *)node;
			freeGroupList(gc->groupsets);
			pfree(gc);
		}

		else
		{
			pfree(node);
		}
	}

	pfree(grouplist);
}

/*
 * Transform a RowExp into a list of GroupClauses, and store this list
 * as a whole into a given List. The new list is returned.
 *
 * This function should be used when a RowExpr is immediately inside
 * a grouping extension clause. For example,
 *    GROUPING SETS ( ( (a,b), c ), (c,d) )
 * or
 *    ROLLUP ( ( (a,b), c ), (c,d) )
 *
 * '(c,d)' is immediately inside a grouping extension clause,
 * while '(a,b)' is not.
 */
static List *
transformRowExprToList(ParseState *pstate, RowExpr *rowexpr,
					   List *groupsets, List *targetList)
{
	List *args = rowexpr->args;
	List *grping_set = NIL;
	ListCell *arglc;

	foreach (arglc, args)
	{
		Node *node = lfirst(arglc);

		if (IsA(node, RowExpr))
		{
			groupsets =
				transformRowExprToGroupClauses(pstate, (RowExpr *)node,
											   groupsets, targetList);
		}

		else
		{
			/* Find the TargetEntry for this argument. This should have been
			 * generated in findListTargetlistEntries().
			 */
			TargetEntry *arg_tle =
				findTargetlistEntrySQL92(pstate, node, &targetList, GROUP_CLAUSE);
			Oid			sortop;
			Oid			eqop;

			get_sort_group_operators(exprType((Node *) arg_tle->expr),
									 true, true, false,
									 &sortop, &eqop, NULL);
			grping_set = lappend(grping_set,
								 make_group_clause(arg_tle, targetList, eqop, sortop, false));
		}
	}
	groupsets = lappend (groupsets, grping_set);

	return groupsets;
}

/*
 * Transform a RowExpr into a list of GroupClauses, and append these
 * GroupClausesone by one into 'groupsets'. The new list is returned.
 *
 * This function should be used when a RowExpr is not immediately inside
 * a grouping extension clause. For example,
 *    GROUPING SETS ( ( (a,b), c ), (c,d) )
 * or
 *    ROLLUP ( ( (a,b), c ), (c,d) )
 *
 * '(c,d)' is immediately inside a grouping extension clause,
 * while '(a,b)' is not.
 */
static List *
transformRowExprToGroupClauses(ParseState *pstate, RowExpr *rowexpr,
							   List *groupsets, List *targetList)
{
	List *args = rowexpr->args;
	ListCell *arglc;

	foreach (arglc, args)
	{
		Node *node = lfirst(arglc);

		if (IsA(node, RowExpr))
		{
			transformRowExprToGroupClauses(pstate, (RowExpr *)node,
										   groupsets, targetList);
		}

		else
		{
			/* Find the TargetEntry for this argument. This should have been
			 * generated in findListTargetlistEntries().
			 */
			TargetEntry *arg_tle =
				findTargetlistEntrySQL92(pstate, node, &targetList, GROUP_CLAUSE);
			Oid			sortop;
			Oid			eqop;

			get_sort_group_operators(exprType((Node *) arg_tle->expr),
									 true, true, false,
									 &sortop, &eqop, NULL);

			/* avoid making duplicate expression entries */
			if (targetIsInSortList(arg_tle, sortop, groupsets))
				continue;

			groupsets = lappend(groupsets,
								make_group_clause(arg_tle, targetList, eqop, sortop, false));
		}
	}

	return groupsets;
}

/*
 * findWindowClause
 *		Find the named WindowClause in the list, or return NULL if not there
 */
static WindowClause *
findWindowClause(List *wclist, const char *name)
{
	ListCell   *l;

	foreach(l, wclist)
	{
		WindowClause *wc = (WindowClause *) lfirst(l);

		if (wc->name && strcmp(wc->name, name) == 0)
			return wc;
	}

	return NULL;
}

/*
 * transformFrameOffset
 *		Process a window frame offset expression
 */
static Node *
transformFrameOffset(ParseState *pstate, int frameOptions, Node *clause,
					 List *orderClause, List *targetlist, bool isFollowing,
					 int location)
{
	const char *constructName = NULL;
	Node	   *node;

	/* Quick exit if no offset expression */
	if (clause == NULL)
		return NULL;

	if (frameOptions & FRAMEOPTION_ROWS)
	{
		/* Transform the raw expression tree */
		node = transformExpr(pstate, clause);

		/*
		 * Like LIMIT clause, simply coerce to int8
		 */
		constructName = "ROWS";
		node = coerce_to_specific_type(pstate, node, INT8OID, constructName);
	}
	else if (frameOptions & FRAMEOPTION_RANGE)
	{
		TargetEntry *te;
		Oid			otype;
		Oid			rtype;
		Oid			newrtype;
		SortGroupClause *sort;
		Oid			oprresult;
		List	   *oprname;
		Operator	tup;
		int32		typmod;

		/* Transform the raw expression tree */
		node = transformExpr(pstate, clause);

		/*
		 * this needs a lot of thought to decide how to support in the context
		 * of Postgres' extensible datatype framework
		 */
		constructName = "RANGE";

		/* caller should've checked this already, but better safe than sorry */
		if (list_length(orderClause) != 1)
		{
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("only one ORDER BY column may be specified when RANGE is used in a window specification"),
					 parser_errposition(pstate, location)));
		}

		typmod = exprTypmod(node);

		if (IsA(node, Const))
		{
			Const *con = (Const *) node;

			if (con->constisnull)
				ereport(ERROR,
						(errcode(ERROR_INVALID_WINDOW_FRAME_PARAMETER),
						 errmsg("RANGE parameter cannot be NULL"),
						 parser_errposition(pstate, con->location)));
		}

		sort = (SortGroupClause *) linitial(orderClause);
		te = getTargetBySortGroupRef(sort->tleSortGroupRef,
									 targetlist);
		otype = exprType((Node *)te->expr);
		rtype = exprType(node);

		/* XXX: Reverse these if user specified DESC */
		if (isFollowing)
			oprname = lappend(NIL, makeString("+"));
		else
			oprname = lappend(NIL, makeString("-"));

		tup = oper(pstate, oprname, otype, rtype, true, 0);

		if (!HeapTupleIsValid(tup))
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("window specification RANGE parameter type must be coercible to ORDER BY column type")));

		oprresult = ((Form_pg_operator)GETSTRUCT(tup))->oprresult;
		newrtype = ((Form_pg_operator)GETSTRUCT(tup))->oprright;
		ReleaseSysCache(tup);
		list_free_deep(oprname);

		if (rtype != newrtype)
		{
			/*
			 * We have to coerce the RHS to the new type so that we'll be
			 * able to trivially find an operator later on.
			 */

			/* XXX: we assume that the typmod for the new RHS type
			 * is the same as before... is that safe?
			 */
			Expr *expr =
				(Expr *)coerce_to_target_type(NULL,
											  node,
											  rtype,
											  newrtype, typmod,
											  COERCION_EXPLICIT,
											  COERCE_IMPLICIT_CAST,
											  -1);
			if (!PointerIsValid(expr))
			{
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("type mismatch between ORDER BY and RANGE "
								"parameter in window specification"),
						 errhint("Operations between window specification "
								 "the ORDER BY column and RANGE parameter "
								 "must result in a data type which can be "
								 "cast back to the ORDER BY column type"),
						 parser_errposition(pstate, exprLocation((Node *) expr))));
			}

			node = (Node *) expr;
		}

		if (oprresult != otype)
		{
			/*
			 * See if it can be coerced. The point of this is to just
			 * throw an error if the coercion is not possible. The
			 * actual coercion will be done later, in the executor.
			 *
			 * XXX: Why not do it here?
			 */
			if (!can_coerce_type(1, &oprresult, &otype, COERCION_EXPLICIT))
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("invalid RANGE parameter"),
						 errhint("Operations between window specification "
								 "the ORDER BY column and RANGE parameter "
								 "must result in a data type which can be "
								 "cast back to the ORDER BY column type")));
		}

		if (IsA(node, Const))
		{
			/* see if RANGE parameter is negative */
			Const *con = (Const *) node;
			Oid			sortop;

			get_sort_group_operators(newrtype,
									 false, false, false,
									 &sortop, NULL, NULL);

			if (OidIsValid(sortop))
			{
				Type typ = typeidType(newrtype);
				Oid funcoid = get_opcode(sortop);
				Datum zero;
				Datum result;

				zero = stringTypeDatum(typ, "0", exprTypmod(node));

				/*
				 * As we know the value is a const and since transformExpr()
				 * will have parsed the type into its internal format, we can
				 * just poke directly into the Const structure.
				 */
				result = OidFunctionCall2(funcoid, con->constvalue, zero);

				if (result)
					ereport(ERROR,
							(errcode(ERROR_INVALID_WINDOW_FRAME_PARAMETER),
							 errmsg("RANGE parameter cannot be negative"),
							 parser_errposition(pstate, con->location)));

				ReleaseSysCache(typ);
			}
		}
	}
	else
	{
		Assert(false);
		node = NULL;
	}

	/* In GPDB, we allow this. */
#if 0
	/* Disallow variables in frame offsets */
	checkExprIsVarFree(pstate, node, constructName);
#endif

	return node;
}
