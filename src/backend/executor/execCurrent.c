/*-------------------------------------------------------------------------
 *
 * execCurrent.c
 *	  executor support for WHERE CURRENT OF cursor
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *	src/backend/executor/execCurrent.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/relscan.h"
#include "access/sysattr.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/portal.h"
#include "utils/rel.h"

#include "access/table.h"
#include "parser/parse_relation.h"
#include "parser/parsetree.h"
#include "cdb/cdbvars.h"


static char *fetch_cursor_param_value(ExprContext *econtext, int paramId);
static ScanState *search_plan_tree(PlanState *node, Oid table_oid,
								   bool *pending_rescan);

/*
 * execCurrentOf
 *
 * Given a CURRENT OF expression and the OID of a table, determine which row
 * of the table is currently being scanned by the cursor named by CURRENT OF,
 * and return the row's TID into *current_tid.
 *
 * Returns true if a row was identified.  Returns false if the cursor is valid
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
			cursor_name = fetch_cursor_param_value(econtext, cexpr->cursor_param);

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
	if (current_gp_segment_id == GpIdentity.segindex &&
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
 *
 * GPDB calls it before dispatching to make QEs get the same current position
 * of the cursor.
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

		cursor_name = fetch_cursor_param_value(econtext, cexpr->cursor_param);
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
	queryDesc = portal->queryDesc;
	if (queryDesc == NULL || queryDesc->estate == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_CURSOR_STATE),
				 errmsg("cursor \"%s\" is held from a previous transaction",
						cursor_name)));

	/*
	 * The referenced cursor must be simply updatable. This has already
	 * been discerned by parse/analyze for the DECLARE CURSOR of the given
	 * cursor. This flag assures us that gp_segment_id, ctid, and tableoid (if necessary)
	 * will be available as junk metadata, courtesy of preprocess_targetlist.
	 *
	 * Apply simply updatable check to ordinary tables. Refer to the issue:
	 * https://github.com/greenplum-db/gpdb/issues/9838.
	 */
	if (!OidIsValid(queryDesc->plannedstmt->simplyUpdatableRel))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_CURSOR_STATE),
						errmsg("cursor \"%s\" is not a simply updatable scan of table \"%s\"",
								cursor_name, table_name)));

	/*
	 * gpdb partition table routine is different with upstream
	 * so we hold private updatable check method.
	 */
	/* better hold a lock already since we're scanning it */
	Relation	rel = table_open(table_oid, NoLock);
	char		relkind = rel->rd_rel->relkind;
	bool		relispartition = rel->rd_rel->relispartition;
	table_close(rel, NoLock);

	if (relkind == RELKIND_PARTITIONED_TABLE ||
		relispartition ||
		get_rel_persistence(table_oid) == RELPERSISTENCE_TEMP)
	{
		/*
		 * The target relation must directly match the cursor's relation. This throws out
		 * the simple case in which a cursor is declared against table X and the update is
		 * issued against Y. Moreover, this disallows some subtler inheritance cases where
		 * Y inherits from X. While such cases could be implemented, it seems wiser to
		 * simply error out cleanly.
		 */
		if (table_oid != queryDesc->plannedstmt->simplyUpdatableRel)
			ereport(ERROR,
			        (errcode(ERRCODE_INVALID_CURSOR_STATE),
					        errmsg("cursor \"%s\" is not a simply updatable scan of table \"%s\"",
					               cursor_name, table_name)));
	}
	else
	{
		ScanState  *scanstate;
		bool		pending_rescan = false;

		/*
		 * Without FOR UPDATE, we dig through the cursor's plan to find the
		 * scan node.  Fail if it's not there or buried underneath
		 * aggregation.
		 */
		scanstate = search_plan_tree(queryDesc->planstate, table_oid,
									 &pending_rescan);
		if (!scanstate)
			ereport(ERROR,
			        (errcode(ERRCODE_INVALID_CURSOR_STATE),
					        errmsg("cursor \"%s\" is not a simply updatable scan of table \"%s\"",
					               cursor_name, table_name)));
	}

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
	 * We have two different strategies depending on whether the cursor uses
	 * FOR UPDATE/SHARE or not.  The reason for supporting both is that the
	 * FOR UPDATE code is able to identify a target table in many cases where
	 * the other code can't, while the non-FOR-UPDATE case allows use of WHERE
	 * CURRENT OF with an insensitive cursor.
	 *
	 * GPDB: Neither of those methods work in GPDB, however, because the scan
	 * is most likely below a Motion node, and belongs to a different slice
	 * than the top node. The slot of the scan node is empty, and the tuple
	 * has been received by a Motion node higher up in the tree instead. So
	 * we use a different approach.
	 */
#if 0
	if (queryDesc->estate->es_rowmarks)
	{
		ExecRowMark *erm;
		Index		i;

		/*
		 * Here, the query must have exactly one FOR UPDATE/SHARE reference to
		 * the target table, and we dig the ctid info out of that.
		 */
		erm = NULL;
		for (i = 0; i < queryDesc->estate->es_range_table_size; i++)
		{
			ExecRowMark *thiserm = queryDesc->estate->es_rowmarks[i];

			if (thiserm == NULL ||
				!RowMarkRequiresRowShareLock(thiserm->markType))
				continue;		/* ignore non-FOR UPDATE/SHARE items */

			if (thiserm->relid == table_oid)
			{
				if (erm)
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_CURSOR_STATE),
							 errmsg("cursor \"%s\" has multiple FOR UPDATE/SHARE references to table \"%s\"",
									cursor_name, table_name)));
				erm = thiserm;
			}
		}

		if (erm == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_CURSOR_STATE),
					 errmsg("cursor \"%s\" does not have a FOR UPDATE/SHARE reference to table \"%s\"",
							cursor_name, table_name)));

		/*
		 * The cursor must have a current result row: per the SQL spec, it's
		 * an error if not.
		 */
		if (portal->atStart || portal->atEnd)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_CURSOR_STATE),
					 errmsg("cursor \"%s\" is not positioned on a row",
							cursor_name)));

		/* Return the currently scanned TID, if there is one */
		if (ItemPointerIsValid(&(erm->curCtid)))
		{
			*current_tid = erm->curCtid;
			return true;
		}

		/*
		 * This table didn't produce the cursor's current row; some other
		 * inheritance child of the same parent must have.  Signal caller to
		 * do nothing on this table.
		 */
		return false;
	}
	else
	{
		/*
		 * Without FOR UPDATE, we dig through the cursor's plan to find the
		 * scan node.  Fail if it's not there or buried underneath
		 * aggregation.
		 */
		ScanState  *scanstate;
		bool		pending_rescan = false;

		scanstate = search_plan_tree(queryDesc->planstate, table_oid,
									 &pending_rescan);
		if (!scanstate)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_CURSOR_STATE),
					 errmsg("cursor \"%s\" is not a simply updatable scan of table \"%s\"",
							cursor_name, table_name)));

		/*
		 * The cursor must have a current result row: per the SQL spec, it's
		 * an error if not.  We test this at the top level, rather than at the
		 * scan node level, because in inheritance cases any one table scan
		 * could easily not be on a row. We want to return false, not raise
		 * error, if the passed-in table OID is for one of the inactive scans.
		 */
		if (portal->atStart || portal->atEnd)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_CURSOR_STATE),
					 errmsg("cursor \"%s\" is not positioned on a row",
							cursor_name)));

		/*
		 * Now OK to return false if we found an inactive scan.  It is
		 * inactive either if it's not positioned on a row, or there's a
		 * rescan pending for it.
		 */
		if (TupIsNull(scanstate->ss_ScanTupleSlot) || pending_rescan)
			return false;

		/*
		 * Extract TID of the scan's current row.  The mechanism for this is
		 * in principle scan-type-dependent, but for most scan types, we can
		 * just dig the TID out of the physical scan tuple.
		 */
		if (IsA(scanstate, IndexOnlyScanState))
		{
			/*
			 * For IndexOnlyScan, the tuple stored in ss_ScanTupleSlot may be
			 * a virtual tuple that does not have the ctid column, so we have
			 * to get the TID from xs_ctup.t_self.
			 */
			IndexScanDesc scan = ((IndexOnlyScanState *) scanstate)->ioss_ScanDesc;

			*current_tid = scan->xs_heaptid;
		}
		else
		{
			/*
			 * Default case: try to fetch TID from the scan node's current
			 * tuple.  As an extra cross-check, verify tableoid in the current
			 * tuple.  If the scan hasn't provided a physical tuple, we have
			 * to fail.
			 */
			Datum		ldatum;
			bool		lisnull;
			ItemPointer tuple_tid;

#ifdef USE_ASSERT_CHECKING
			ldatum = slot_getsysattr(scanstate->ss_ScanTupleSlot,
									 TableOidAttributeNumber,
									 &lisnull);
			if (lisnull)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_CURSOR_STATE),
						 errmsg("cursor \"%s\" is not a simply updatable scan of table \"%s\"",
								cursor_name, table_name)));
			Assert(DatumGetObjectId(ldatum) == table_oid);
#endif

			ldatum = slot_getsysattr(scanstate->ss_ScanTupleSlot,
									 SelfItemPointerAttributeNumber,
									 &lisnull);
			if (lisnull)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_CURSOR_STATE),
						 errmsg("cursor \"%s\" is not a simply updatable scan of table \"%s\"",
								cursor_name, table_name)));
			tuple_tid = (ItemPointer) DatumGetPointer(ldatum);

			*current_tid = *tuple_tid;
		}

		Assert(ItemPointerIsValid(current_tid));

		return true;
	}
#endif
	{
		/*
		 * GPDB method:
		 *
		 * The planner should've made the gp_segment_id, ctid, and tableoid
		 * available as junk columns at the top of the plan. To retrieve this
		 * junk metadata, we leverage the EState's junkfilter against the raw
		 * tuple yielded by the top node in the plan.
		 */
		slot = queryDesc->planstate->ps_ResultTupleSlot;
		if (TupIsNull(slot))
			elog(ERROR, "TupleTableslot is empty");
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
			(void) isSimplyUpdatableRelation(*current_table_oid, false /* noerror */);
		}

		if (p_cursor_name)
			*p_cursor_name = pstrdup(cursor_name);
	}
}

/*
 * fetch_cursor_param_value
 *
 * Fetch the string value of a param, verifying it is of type REFCURSOR.
 */
static char *
fetch_cursor_param_value(ExprContext *econtext, int paramId)
{
	ParamListInfo paramInfo = econtext->ecxt_param_list_info;

	if (paramInfo &&
		paramId > 0 && paramId <= paramInfo->numParams)
	{
		ParamExternData *prm;
		ParamExternData prmdata;

		/* give hook a chance in case parameter is dynamic */
		if (paramInfo->paramFetch != NULL)
			prm = paramInfo->paramFetch(paramInfo, paramId, false, &prmdata);
		else
			prm = &paramInfo->params[paramId - 1];

		if (OidIsValid(prm->ptype) && !prm->isnull)
		{
			/* safety check in case hook did something unexpected */
			if (prm->ptype != REFCURSOROID)
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
						 errmsg("type of parameter %d (%s) does not match that when preparing the plan (%s)",
								paramId,
								format_type_be(prm->ptype),
								format_type_be(REFCURSOROID))));

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
 *
 * If a candidate is found, set *pending_rescan to true if that candidate
 * or any node above it has a pending rescan action, i.e. chgParam != NULL.
 * That indicates that we shouldn't consider the node to be positioned on a
 * valid tuple, even if its own state would indicate that it is.  (Caller
 * must initialize *pending_rescan to false, and should not trust its state
 * if multiple candidates are found.)
 */
static ScanState *
search_plan_tree(PlanState *node, Oid table_oid,
				 bool *pending_rescan)
{
	ScanState  *result = NULL;

	if (node == NULL)
		return NULL;
	switch (nodeTag(node))
	{
		/*
		 * Relation scan nodes can all be treated alike
		 */
		case T_SeqScanState:
		case T_SampleScanState:
		case T_IndexScanState:
		case T_IndexOnlyScanState:
		case T_BitmapHeapScanState:
		case T_TidScanState:
		case T_ForeignScanState:
		case T_CustomScanState:
			{
				ScanState  *sstate = (ScanState *) node;

				if (RelationGetRelid(sstate->ss_currentRelation) == table_oid)
					result = sstate;
				break;
			}

			/*
			 * For Append, we must look through the members; watch out for
			 * multiple matches (possible if it was from UNION ALL)
			 */
		case T_AppendState:
			{
				AppendState *astate = (AppendState *) node;
				int			i;

				for (i = 0; i < astate->as_nplans; i++)
				{
					ScanState  *elem = search_plan_tree(astate->appendplans[i],
														table_oid,
														pending_rescan);

					if (!elem)
						continue;
					if (result)
						return NULL;	/* multiple matches */
					result = elem;
				}
				break;
			}

			/*
			 * Similarly for MergeAppend
			 */
		case T_MergeAppendState:
			{
				MergeAppendState *mstate = (MergeAppendState *) node;
				int			i;

				for (i = 0; i < mstate->ms_nplans; i++)
				{
					ScanState  *elem = search_plan_tree(mstate->mergeplans[i],
														table_oid,
														pending_rescan);

					if (!elem)
						continue;
					if (result)
						return NULL;	/* multiple matches */
					result = elem;
				}
				break;
			}

			/*
			 * Result and Limit can be descended through (these are safe
			 * because they always return their input's current row)
			 */
		case T_ResultState:
		case T_LimitState:
			result = search_plan_tree(node->lefttree,
									  table_oid,
									  pending_rescan);
			break;

			/*
			 * SubqueryScan too, but it keeps the child in a different place
			 */
		case T_SubqueryScanState:
			result = search_plan_tree(((SubqueryScanState *) node)->subplan,
									  table_oid,
									  pending_rescan);
			break;

		case T_MotionState:
			result = search_plan_tree(node->lefttree, table_oid, pending_rescan);
			break;

		default:
			/* Otherwise, assume we can't descend through it */
			break;
	}

	/*
	 * If we found a candidate at or below this node, then this node's
	 * chgParam indicates a pending rescan that will affect the candidate.
	 */
	if (result && node->chgParam != NULL)
		*pending_rescan = true;

	return result;
}
