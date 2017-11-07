/*-------------------------------------------------------------------------
 *
 * execCurrent.c
 *	  executor support for WHERE CURRENT OF cursor
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *	$PostgreSQL: pgsql/src/backend/executor/execCurrent.c,v 1.7 2008/05/12 00:00:48 alvherre Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/sysattr.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/portal.h"

#include "parser/parse_relation.h"
#include "parser/parsetree.h"
#include "cdb/cdbvars.h"


static char *fetch_param_value(ExprContext *econtext, int paramId);
#ifdef NOT_USED
static ScanState *search_plan_tree(PlanState *node, Oid table_oid);
#endif /* NOT_USED */

/*
 * execCurrentOf
 *
 * Given a CURRENT OF expression and the OID of a table, determine which row
 * of the table is currently being scanned by the cursor named by CURRENT OF,
 * and return the row's TID into *current_tid.
 *
 * Returns TRUE if a row was identified.  Returns FALSE if the cursor is valid
 * for the table but is not currently scanning a row of the table (this is a
 * legal situation in inheritance cases).  Raises error if cursor is not a
 * valid updatable scan of the specified table.
 *
 * In GPDB, we also check that the tuple came from the current segment.
 */
bool
execCurrentOf(CurrentOfExpr *cexpr,
			  ExprContext *econtext,
			  Oid table_oid,
			  ItemPointer current_tid)
{
	int			current_gp_segment_id = -1;
	Oid			current_table_oid;

	/*
	 * In an executor node, the dispatcher should've included the current
	 * position of the cursor along with the query plan. Find and return it
	 * from there.
	 */
	if (Gp_role == GP_ROLE_EXECUTE)
	{
		ListCell   *lc;
		char	   *cursor_name;
		bool		found = false;

		/* Get the cursor name --- may have to look up a parameter reference */
		if (cexpr->cursor_name)
			cursor_name = cexpr->cursor_name;
		else
			cursor_name = fetch_param_value(econtext, cexpr->cursor_param);

		foreach (lc, econtext->ecxt_estate->es_cursorPositions)
		{
			CursorPosInfo *cpos = (CursorPosInfo *) lfirst(lc);

			if (strcmp(cpos->cursor_name, cursor_name) == 0)
			{
				current_gp_segment_id = cpos->gp_segment_id;
				current_table_oid = cpos->table_oid;
				ItemPointerCopy(&cpos->ctid, current_tid);
				found = true;
				break;
			}
		}

		/* Not found. Odd, the dispatcher should've checked for this already. */
		if (!found)
			elog(ERROR, "no cursor position information found for cursor \"%s\"",
				 cursor_name);
	}
	else
	{
		getCurrentOf(cexpr, econtext, table_oid, current_tid,
					 &current_gp_segment_id, &current_table_oid, NULL);
	}

	/*
	 * Found the cursor. Does the table and segment match?
	 */
	if (current_gp_segment_id == Gp_segment &&
		(current_table_oid == InvalidOid || current_table_oid == table_oid))
	{
		return true;
	}
	else
		return false;
}

/*
 * Return the current position of a cursor that a CURRENT OF expression
 * refers to.
 *
 * This checks that the cursor is valid for table specified by 'table_oid',
 * but it doesn't have to be scanning a row of that table (i.e. it can
 * be scanning a row of a different table in the same inheritance hierarchy).
 * The current table's oid is returned in *current_table_oid.
 */
void
getCurrentOf(CurrentOfExpr *cexpr,
			 ExprContext *econtext,
			 Oid table_oid,
			 ItemPointer current_tid,
			 int *current_gp_segment_id,
			 Oid *current_table_oid,
			 char **p_cursor_name)
{
	char	   *cursor_name;
	char	   *table_name;
	Portal		portal;
	QueryDesc  *queryDesc;
	TupleTableSlot *slot;
	AttrNumber	gp_segment_id_attno;
	AttrNumber	ctid_attno;
	AttrNumber	tableoid_attno;
	bool		isnull;
	Datum		value;

	/*
	 * In an executor node, execCurrentOf() is supposed to use the cursor
	 * position information received from the dispatcher, and we shouldn't
	 * get here.
	 */
	if (Gp_role == GP_ROLE_EXECUTE)
		elog(ERROR, "getCurrentOf called in executor node");

	/* Get the cursor name --- may have to look up a parameter reference */
	if (cexpr->cursor_name)
		cursor_name = cexpr->cursor_name;
	else
	{
		if (!econtext->ecxt_param_list_info)
			elog(ERROR, "no cursor name information found");

		cursor_name = fetch_param_value(econtext, cexpr->cursor_param);
	}

	/* Fetch table name for possible use in error messages */
	table_name = get_rel_name(table_oid);
	if (table_name == NULL)
		elog(ERROR, "cache lookup failed for relation %u", table_oid);

	/* Find the cursor's portal */
	portal = GetPortalByName(cursor_name);
	if (!PortalIsValid(portal))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_CURSOR),
				 errmsg("cursor \"%s\" does not exist", cursor_name)));

	/*
	 * We have to watch out for non-SELECT queries as well as held cursors,
	 * both of which may have null queryDesc.
	 */
	if (portal->strategy != PORTAL_ONE_SELECT)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_CURSOR_STATE),
				 errmsg("cursor \"%s\" is not a SELECT query",
						cursor_name)));
	queryDesc = PortalGetQueryDesc(portal);
	if (queryDesc == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_CURSOR_STATE),
				 errmsg("cursor \"%s\" is held from a previous transaction",
						cursor_name)));

	/*
	 * The referenced cursor must be simply updatable. This has already
	 * been discerned by parse/analyze for the DECLARE CURSOR of the given
	 * cursor. This flag assures us that gp_segment_id, ctid, and tableoid (if necessary)
	 * will be available as junk metadata, courtesy of preprocess_targetlist.
	 */
	if (!queryDesc->plannedstmt->simplyUpdatable)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_CURSOR_STATE),
				 errmsg("cursor \"%s\" is not a simply updatable scan of table \"%s\"",
						cursor_name, table_name)));

	/*
	 * The target relation must directly match the cursor's relation. This throws out
	 * the simple case in which a cursor is declared against table X and the update is
	 * issued against Y. Moreover, this disallows some subtler inheritance cases where
	 * Y inherits from X. While such cases could be implemented, it seems wiser to
	 * simply error out cleanly.
	 */
	Index varno = extractSimplyUpdatableRTEIndex(queryDesc->plannedstmt->rtable);
	Oid cursor_relid = getrelid(varno, queryDesc->plannedstmt->rtable);
	if (table_oid != cursor_relid)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_CURSOR_STATE),
				 errmsg("cursor \"%s\" is not a simply updatable scan of table \"%s\"",
						cursor_name, table_name)));

	/*
	 * The cursor must have a current result row: per the SQL spec, it's an
	 * error if not.  We test this at the top level, rather than at the scan
	 * node level, because in inheritance cases any one table scan could
	 * easily not be on a row.	We want to return false, not raise error, if
	 * the passed-in table OID is for one of the inactive scans.
	 */
	if (portal->atStart || portal->atEnd)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_CURSOR_STATE),
				 errmsg("cursor \"%s\" is not positioned on a row",
						cursor_name)));

	/*
	 * In PostgreSQL, we extract the current tuple's TID from the scan node
	 * we dug above. That doesn't work in GPDB, however, because the scan is
	 * most likely below a Motion node, and belongs to a different slice
	 * than the top node. The slot of the scan node is empty, and the tuple
	 * has been received by a Motion node higher up in the tree instead. So
	 * we use a different approach:
	 *
	 * The planner should've made the gp_segment_id, ctid, and tableoid
	 * available as junk columns at the top of the plan. To retrieve this
	 * junk metadata, we leverage the EState's junkfilter against the raw
	 * tuple yielded by the top node in the plan.
	 */

	slot = queryDesc->planstate->ps_ResultTupleSlot;
	Insist(!TupIsNull(slot));
	Assert(queryDesc->estate->es_junkFilter);

	/* extract gp_segment_id metadata */
	gp_segment_id_attno = ExecFindJunkAttribute(queryDesc->estate->es_junkFilter, "gp_segment_id");
	if (!AttributeNumberIsValid(gp_segment_id_attno))
		elog(ERROR, "could not find junk gp_segment_id column");

	value = ExecGetJunkAttribute(slot, gp_segment_id_attno, &isnull);
	if (isnull)
		elog(ERROR, "gp_segment_id is NULL");
	*current_gp_segment_id = DatumGetInt32(value);

	/* extract ctid metadata */
	ctid_attno = ExecFindJunkAttribute(queryDesc->estate->es_junkFilter, "ctid");
	if (!AttributeNumberIsValid(ctid_attno))
		elog(ERROR, "could not find junk ctid column");
	value = ExecGetJunkAttribute(slot, ctid_attno, &isnull);
	if (isnull)
		elog(ERROR, "ctid is NULL");
	ItemPointerCopy(DatumGetItemPointer(value), current_tid);

	/*
	 * extract tableoid metadata
	 *
	 * DECLARE CURSOR planning only includes tableoid metadata when
	 * scrolling a partitioned table. Otherwise gp_segment_id and ctid alone
	 * are sufficient to uniquely identify a tuple.
	 */
	tableoid_attno = ExecFindJunkAttribute(queryDesc->estate->es_junkFilter,
										   "tableoid");
	if (AttributeNumberIsValid(tableoid_attno))
	{
		value = ExecGetJunkAttribute(slot, tableoid_attno, &isnull);
		if (isnull)
			elog(ERROR, "tableoid is NULL");
		*current_table_oid = DatumGetObjectId(value);

		/*
		 * This is our last opportunity to verify that the physical table given
		 * by tableoid is, indeed, simply updatable.
		 */
		if (!isSimplyUpdatableRelation(*current_table_oid, true /* noerror */))
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("%s is not updatable",
							get_rel_name_partition(*current_table_oid))));
	}

	if (p_cursor_name)
		*p_cursor_name = pstrdup(cursor_name);
}

/*
 * fetch_param_value
 *
 * Fetch the string value of a param, verifying it is of type REFCURSOR.
 */
static char *
fetch_param_value(ExprContext *econtext, int paramId)
{
	ParamListInfo paramInfo = econtext->ecxt_param_list_info;

	if (paramInfo &&
		paramId > 0 && paramId <= paramInfo->numParams)
	{
		ParamExternData *prm = &paramInfo->params[paramId - 1];

		if (OidIsValid(prm->ptype) && !prm->isnull)
		{
			Assert(prm->ptype == REFCURSOROID);
			/* We know that refcursor uses text's I/O routines */
			return TextDatumGetCString(prm->value);
		}
	}

	ereport(ERROR,
			(errcode(ERRCODE_UNDEFINED_OBJECT),
			 errmsg("no value found for parameter %d", paramId)));
	return NULL;
}

/*
 * search_plan_tree
 *
 * Search through a PlanState tree for a scan node on the specified table.
 * Return NULL if not found or multiple candidates.
 */
#ifdef NOT_USED
static ScanState *
search_plan_tree(PlanState *node, Oid table_oid)
{
	if (node == NULL)
		return NULL;
	switch (nodeTag(node))
	{
			/*
			 * scan nodes can all be treated alike
			 */
		case T_SeqScanState:
		case T_AppendOnlyScanState:
		case T_AOCSScanState:
		case T_TableScanState:
		case T_DynamicTableScanState:
		case T_IndexScanState:
		case T_BitmapHeapScanState:
		case T_BitmapAppendOnlyScanState:
		case T_BitmapTableScanState:
		case T_TidScanState:
			{
				ScanState  *sstate = (ScanState *) node;

				if (RelationGetRelid(sstate->ss_currentRelation) == table_oid)
					return sstate;
				break;
			}

			/*
			 * For Append, we must look through the members; watch out for
			 * multiple matches (possible if it was from UNION ALL)
			 */
		case T_AppendState:
			{
				AppendState *astate = (AppendState *) node;
				ScanState  *result = NULL;
				int			i;

				for (i = 0; i < astate->as_nplans; i++)
				{
					ScanState  *elem = search_plan_tree(astate->appendplans[i],
														table_oid);

					if (!elem)
						continue;
					if (result)
						return NULL;	/* multiple matches */
					result = elem;
				}
				return result;
			}

			/*
			 * Result and Limit can be descended through (these are safe
			 * because they always return their input's current row)
			 */
		case T_ResultState:
		case T_LimitState:
		case T_MotionState:
			return search_plan_tree(node->lefttree, table_oid);

			/*
			 * SubqueryScan too, but it keeps the child in a different place
			 */
		case T_SubqueryScanState:
			return search_plan_tree(((SubqueryScanState *) node)->subplan,
									table_oid);

		default:
			/* Otherwise, assume we can't descend through it */
			break;
	}
	return NULL;
}
#endif /* NOT_USED */
