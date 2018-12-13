/*-------------------------------------------------------------------------
 *
 * view.c
 *	  use rewrite rules to construct views
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/view.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "access/xact.h"
#include "catalog/namespace.h"
#include "catalog/oid_dispatch.h"
#include "catalog/pg_depend.h"
#include "commands/defrem.h"
#include "commands/tablecmds.h"
#include "commands/view.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "parser/analyze.h"
#include "parser/parse_relation.h"
#include "rewrite/rewriteDefine.h"
#include "rewrite/rewriteManip.h"
#include "rewrite/rewriteHandler.h"
#include "rewrite/rewriteSupport.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"


#include "cdb/cdbdisp_query.h"
#include "cdb/cdbvars.h"


static void checkViewTupleDesc(TupleDesc newdesc, TupleDesc olddesc);

/*---------------------------------------------------------------------
 * Validator for "check_option" reloption on views. The allowed values
 * are "local" and "cascaded".
 */
void
validateWithCheckOption(char *value)
{
	if (value == NULL ||
		(pg_strcasecmp(value, "local") != 0 &&
		 pg_strcasecmp(value, "cascaded") != 0))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid value for \"check_option\" option"),
				 errdetail("Valid values are \"local\", and \"cascaded\".")));
	}
}

/*---------------------------------------------------------------------
 * DefineVirtualRelation
 *
 * Create the "view" relation. `DefineRelation' does all the work,
 * we just provide the correct arguments ... at least when we're
 * creating a view.  If we're updating an existing view, we have to
 * work harder.
 *---------------------------------------------------------------------
 */
static Oid
DefineVirtualRelation(RangeVar *relation, List *tlist, bool replace,
					  List *options)
{
	Oid			viewOid;
	LOCKMODE	lockmode;
	CreateStmt *createStmt = makeNode(CreateStmt);
	List	   *attrList;
	ListCell   *t;

	createStmt->ownerid = GetUserId();

	/*
	 * create a list of ColumnDef nodes based on the names and types of the
	 * (non-junk) targetlist items from the view's SELECT list.
	 */
	attrList = NIL;
	foreach(t, tlist)
	{
		TargetEntry *tle = lfirst(t);

		if (!tle->resjunk)
		{
			ColumnDef  *def = makeNode(ColumnDef);

			def->colname = pstrdup(tle->resname);
			def->typeName = makeTypeNameFromOid(exprType((Node *) tle->expr),
											 exprTypmod((Node *) tle->expr));
			def->inhcount = 0;
			def->is_local = true;
			def->is_not_null = false;
			def->is_from_type = false;
			def->storage = 0;
			def->raw_default = NULL;
			def->cooked_default = NULL;
			def->collClause = NULL;
			def->collOid = exprCollation((Node *) tle->expr);
			def->location = -1;

			/*
			 * It's possible that the column is of a collatable type but the
			 * collation could not be resolved, so double-check.
			 */
			if (type_is_collatable(exprType((Node *) tle->expr)))
			{
				if (!OidIsValid(def->collOid))
					ereport(ERROR,
							(errcode(ERRCODE_INDETERMINATE_COLLATION),
							 errmsg("could not determine which collation to use for view column \"%s\"",
									def->colname),
							 errhint("Use the COLLATE clause to set the collation explicitly.")));
			}
			else
				Assert(!OidIsValid(def->collOid));
			def->constraints = NIL;

			attrList = lappend(attrList, def);
		}
	}

	if (attrList == NIL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("view must have at least one column")));

	/*
	 * Look up, check permissions on, and lock the creation namespace; also
	 * check for a preexisting view with the same name.  This will also set
	 * relation->relpersistence to RELPERSISTENCE_TEMP if the selected
	 * namespace is temporary.
	 */
	lockmode = replace ? AccessExclusiveLock : NoLock;
	(void) RangeVarGetAndCheckCreationNamespace(relation, lockmode, &viewOid);

	if (OidIsValid(viewOid) && replace)
	{
		Relation	rel;
		TupleDesc	descriptor;
		List	   *atcmds = NIL;
		AlterTableCmd *atcmd;

		/* Relation is already locked, but we must build a relcache entry. */
		rel = relation_open(viewOid, NoLock);

		/* Make sure it *is* a view. */
		if (rel->rd_rel->relkind != RELKIND_VIEW)
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("\"%s\" is not a view",
							RelationGetRelationName(rel))));

		/* Also check it's not in use already */
		CheckTableNotInUse(rel, "CREATE OR REPLACE VIEW");

		/*
		 * Due to the namespace visibility rules for temporary objects, we
		 * should only end up replacing a temporary view with another
		 * temporary view, and similarly for permanent views.
		 */
		Assert(relation->relpersistence == rel->rd_rel->relpersistence);

		/*
		 * Create a tuple descriptor to compare against the existing view, and
		 * verify that the old column list is an initial prefix of the new
		 * column list.
		 */
		descriptor = BuildDescForRelation(attrList);
		checkViewTupleDesc(descriptor, rel->rd_att);

		/*
		 * The new options list replaces the existing options list, even if
		 * it's empty.
		 */
		atcmd = makeNode(AlterTableCmd);
		atcmd->subtype = AT_ReplaceRelOptions;
		atcmd->def = (Node *) options;
		atcmds = lappend(atcmds, atcmd);

		/*
		 * If new attributes have been added, we must add pg_attribute entries
		 * for them.  It is convenient (although overkill) to use the ALTER
		 * TABLE ADD COLUMN infrastructure for this.
		 */
		if (list_length(attrList) > rel->rd_att->natts)
		{
			ListCell   *c;
			int			skip = rel->rd_att->natts;

			foreach(c, attrList)
			{
				if (skip > 0)
				{
					skip--;
					continue;
				}
				atcmd = makeNode(AlterTableCmd);
				atcmd->subtype = AT_AddColumnToView;
				atcmd->def = (Node *) lfirst(c);
				atcmds = lappend(atcmds, atcmd);
			}
		}

		/* OK, let's do it. */
		AlterTableInternal(viewOid, atcmds, true);

		/*
		 * Seems okay, so return the OID of the pre-existing view.
		 */
		relation_close(rel, NoLock);	/* keep the lock! */

		return viewOid;
	}
	else
	{
		Oid			relid;

		/*
		 * now set the parameters for keys/inheritance etc. All of these are
		 * uninteresting for views...
		 */
		createStmt->relation = relation;
		createStmt->tableElts = attrList;
		createStmt->inhRelations = NIL;
		createStmt->inhOids = NIL;
		createStmt->parentOidCount = 0;
		createStmt->constraints = NIL;
		createStmt->options = options;
		createStmt->oncommit = ONCOMMIT_NOOP;
		createStmt->tablespacename = NULL;
		createStmt->relKind = RELKIND_VIEW;
		createStmt->if_not_exists = false;

		/*
		 * finally create the relation (this will error out if there's an
		 * existing view, so we don't need more code to complain if "replace"
		 * is false).
		 */
		relid = DefineRelation(createStmt, RELKIND_VIEW, InvalidOid, RELSTORAGE_VIRTUAL, false, true, NULL);
		Assert(relid != InvalidOid);
		return relid;
	}
}

/*
 * Verify that tupledesc associated with proposed new view definition
 * matches tupledesc of old view.  This is basically a cut-down version
 * of equalTupleDescs(), with code added to generate specific complaints.
 * Also, we allow the new tupledesc to have more columns than the old.
 */
static void
checkViewTupleDesc(TupleDesc newdesc, TupleDesc olddesc)
{
	int			i;

	if (newdesc->natts < olddesc->natts)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("cannot drop columns from view")));
	/* we can ignore tdhasoid */

	for (i = 0; i < olddesc->natts; i++)
	{
		Form_pg_attribute newattr = newdesc->attrs[i];
		Form_pg_attribute oldattr = olddesc->attrs[i];

		/* XXX msg not right, but we don't support DROP COL on view anyway */
		if (newattr->attisdropped != oldattr->attisdropped)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("cannot drop columns from view")));

		if (strcmp(NameStr(newattr->attname), NameStr(oldattr->attname)) != 0)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("cannot change name of view column \"%s\" to \"%s\"",
						NameStr(oldattr->attname),
						NameStr(newattr->attname))));
		/* XXX would it be safe to allow atttypmod to change?  Not sure */
		if (newattr->atttypid != oldattr->atttypid ||
			newattr->atttypmod != oldattr->atttypmod)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
					 errmsg("cannot change data type of view column \"%s\" from %s to %s",
							NameStr(oldattr->attname),
							format_type_with_typemod(oldattr->atttypid,
													 oldattr->atttypmod),
							format_type_with_typemod(newattr->atttypid,
													 newattr->atttypmod))));
		/* We can ignore the remaining attributes of an attribute... */
	}

	/*
	 * We ignore the constraint fields.  The new view desc can't have any
	 * constraints, and the only ones that could be on the old view are
	 * defaults, which we are happy to leave in place.
	 */
}

static void
DefineViewRules(Oid viewOid, Query *viewParse, bool replace)
{
	/*
	 * Set up the ON SELECT rule.  Since the query has already been through
	 * parse analysis, we use DefineQueryRewrite() directly.
	 */
	DefineQueryRewrite(pstrdup(ViewSelectRuleName),
					   viewOid,
					   NULL,
					   CMD_SELECT,
					   true,
					   replace,
					   list_make1(viewParse));

	/*
	 * Someday: automatic ON INSERT, etc
	 */
}

/*---------------------------------------------------------------
 * UpdateRangeTableOfViewParse
 *
 * Update the range table of the given parsetree.
 * This update consists of adding two new entries IN THE BEGINNING
 * of the range table (otherwise the rule system will die a slow,
 * horrible and painful death, and we do not want that now, do we?)
 * one for the OLD relation and one for the NEW one (both of
 * them refer in fact to the "view" relation).
 *
 * Of course we must also increase the 'varnos' of all the Var nodes
 * by 2...
 *
 * These extra RT entries are not actually used in the query,
 * except for run-time permission checking.
 *---------------------------------------------------------------
 */
static Query *
UpdateRangeTableOfViewParse(Oid viewOid, Query *viewParse)
{
	Relation	viewRel;
	List	   *new_rt;
	RangeTblEntry *rt_entry1,
			   *rt_entry2;

	/*
	 * Make a copy of the given parsetree.  It's not so much that we don't
	 * want to scribble on our input, it's that the parser has a bad habit of
	 * outputting multiple links to the same subtree for constructs like
	 * BETWEEN, and we mustn't have OffsetVarNodes increment the varno of a
	 * Var node twice.  copyObject will expand any multiply-referenced subtree
	 * into multiple copies.
	 */
	viewParse = (Query *) copyObject(viewParse);

	/* need to open the rel for addRangeTableEntryForRelation */
	viewRel = relation_open(viewOid, AccessShareLock);

	/*
	 * Create the 2 new range table entries and form the new range table...
	 * OLD first, then NEW....
	 */
	rt_entry1 = addRangeTableEntryForRelation(NULL, viewRel,
											  makeAlias("old", NIL),
											  false, false);
	rt_entry2 = addRangeTableEntryForRelation(NULL, viewRel,
											  makeAlias("new", NIL),
											  false, false);
	/* Must override addRangeTableEntry's default access-check flags */
	rt_entry1->requiredPerms = 0;
	rt_entry2->requiredPerms = 0;

	new_rt = lcons(rt_entry1, lcons(rt_entry2, viewParse->rtable));

	viewParse->rtable = new_rt;

	/*
	 * Now offset all var nodes by 2, and jointree RT indexes too.
	 */
	OffsetVarNodes((Node *) viewParse, 2, 0);

	relation_close(viewRel, AccessShareLock);

	return viewParse;
}

/*
 * DefineView
 *		Execute a CREATE VIEW command.
 */
Oid
DefineView(ViewStmt *stmt, const char *queryString)
{
	Query	   *viewParse_orig;
	Query	   *viewParse;
	Oid			viewOid;
	RangeVar   *view;
	ListCell   *cell;
	bool		check_option;

	/*
	 * Run parse analysis to convert the raw parse tree to a Query.  Note this
	 * also acquires sufficient locks on the source table(s).
	 *
	 * Since parse analysis scribbles on its input, copy the raw parse tree;
	 * this ensures we don't corrupt a prepared statement, for example.
	 *
	 * GPDB: Parse analysis is only performed in the dispatcher, the segments
	 * receive an already-analysed version from the dispatcher.
	 */
	if (Gp_role != GP_ROLE_EXECUTE)
		viewParse = parse_analyze((Node *) copyObject(stmt->query),
								  queryString, NULL, 0);
	else
		viewParse = (Query *) stmt->query;
	viewParse_orig = copyObject(viewParse);

	/*
	 * The grammar should ensure that the result is a single SELECT Query.
	 * However, it doesn't forbid SELECT INTO, so we have to check for that.
	 */
	if (!IsA(viewParse, Query))
		elog(ERROR, "unexpected parse analysis result");
	if (viewParse->utilityStmt != NULL &&
		IsA(viewParse->utilityStmt, CreateTableAsStmt))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("views must not contain SELECT INTO")));
	if (viewParse->commandType != CMD_SELECT ||
		viewParse->utilityStmt != NULL)
		elog(ERROR, "unexpected parse analysis result");

	/*
	 * Check for unsupported cases.  These tests are redundant with ones in
	 * DefineQueryRewrite(), but that function will complain about a bogus ON
	 * SELECT rule, and we'd rather the message complain about a view.
	 */
	if (viewParse->hasModifyingCTE)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		errmsg("views must not contain data-modifying statements in WITH")));

	/*
	 * Don't allow creating a view that contains dynamically typed functions.
	 * We cannot guarantee that the future return type would be the same when
	 * the view was used, as what it was now.
	 */
	if (viewParse->hasDynamicFunctions)
		ereport(ERROR,
				(errcode(ERRCODE_INDETERMINATE_DATATYPE),
				 errmsg("CREATE VIEW statements cannot include calls to "
						"dynamically typed function")));

	/*
	 * If the user specified the WITH CHECK OPTION, add it to the list of
	 * reloptions.
	 */
	if (stmt->withCheckOption == LOCAL_CHECK_OPTION)
		stmt->options = lappend(stmt->options,
								makeDefElem("check_option",
											(Node *) makeString("local")));
	else if (stmt->withCheckOption == CASCADED_CHECK_OPTION)
		stmt->options = lappend(stmt->options,
								makeDefElem("check_option",
											(Node *) makeString("cascaded")));

	/*
	 * Check that the view is auto-updatable if WITH CHECK OPTION was
	 * specified.
	 */
	check_option = false;

	foreach(cell, stmt->options)
	{
		DefElem    *defel = (DefElem *) lfirst(cell);

		if (pg_strcasecmp(defel->defname, "check_option") == 0)
			check_option = true;
	}

	/*
	 * If the check option is specified, look to see if the view is actually
	 * auto-updatable or not.
	 */
	if (check_option)
	{
		const char *view_updatable_error =
		view_query_is_auto_updatable(viewParse, true);

		if (view_updatable_error)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("WITH CHECK OPTION is supported only on auto-updatable views"),
					 errhint("%s", view_updatable_error)));
	}

	/*
	 * If a list of column names was given, run through and insert these into
	 * the actual query tree. - thomas 2000-03-08
	 */
	if (stmt->aliases != NIL)
	{
		ListCell   *alist_item = list_head(stmt->aliases);
		ListCell   *targetList;

		foreach(targetList, viewParse->targetList)
		{
			TargetEntry *te = (TargetEntry *) lfirst(targetList);

			Assert(IsA(te, TargetEntry));
			/* junk columns don't get aliases */
			if (te->resjunk)
				continue;
			te->resname = pstrdup(strVal(lfirst(alist_item)));
			alist_item = lnext(alist_item);
			if (alist_item == NULL)
				break;			/* done assigning aliases */
		}

		if (alist_item != NULL)
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("CREATE VIEW specifies more column "
							"names than columns")));
	}

	/* Unlogged views are not sensible. */
	if (stmt->view->relpersistence == RELPERSISTENCE_UNLOGGED)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
		errmsg("views cannot be unlogged because they do not have storage")));

	/*
	 * If the user didn't explicitly ask for a temporary view, check whether
	 * we need one implicitly.  We allow TEMP to be inserted automatically as
	 * long as the CREATE command is consistent with that --- no explicit
	 * schema name.
	 */
	view = copyObject(stmt->view);		/* don't corrupt original command */
	if (view->relpersistence == RELPERSISTENCE_PERMANENT
		&& isQueryUsingTempRelation(viewParse))
	{
		view->relpersistence = RELPERSISTENCE_TEMP;
		if (Gp_role != GP_ROLE_EXECUTE)
			ereport(NOTICE,
					(errmsg("view \"%s\" will be a temporary view",
							view->relname)));
	}

	/*
	 * Create the view relation
	 *
	 * NOTE: if it already exists and replace is false, the xact will be
	 * aborted.
	 */
	viewOid = DefineVirtualRelation(view, viewParse->targetList,
									stmt->replace, stmt->options);

	/*
	 * The relation we have just created is not visible to any other commands
	 * running with the same transaction & command id. So, increment the
	 * command id counter (but do NOT pfree any memory!!!!)
	 */
	CommandCounterIncrement();

	StoreViewQuery(viewOid, viewParse, stmt->replace);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		ViewStmt *dispatchStmt = (ViewStmt *) copyObject(stmt);
		dispatchStmt->query = (Node *) viewParse_orig;
		CdbDispatchUtilityStatement((Node *) dispatchStmt,
									DF_CANCEL_ON_ERROR|
									DF_WITH_SNAPSHOT|
									DF_NEED_TWO_PHASE,
									GetAssignedOidsForDispatch(),
									NULL);
	}

	return viewOid;
}

/*
 * Use the rules system to store the query for the view.
 */
void
StoreViewQuery(Oid viewOid, Query *viewParse, bool replace)
{
	/*
	 * The range table of 'viewParse' does not contain entries for the "OLD"
	 * and "NEW" relations. So... add them!
	 */
	viewParse = UpdateRangeTableOfViewParse(viewOid, viewParse);

	/*
	 * Now create the rules associated with the view.
	 */
	DefineViewRules(viewOid, viewParse, replace);
}
