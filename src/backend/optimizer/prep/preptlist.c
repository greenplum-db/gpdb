/*-------------------------------------------------------------------------
 *
 * preptlist.c
 *	  Routines to preprocess the parse tree target list
 *
 * For INSERT and UPDATE queries, the targetlist must contain an entry for
 * each attribute of the target relation in the correct order.  For all query
 * types, we may need to add junk tlist entries for Vars used in the RETURNING
 * list and row ID information needed for SELECT FOR UPDATE locking and/or
 * EvalPlanQual checking.
 *
 * NOTE: the rewriter's rewriteTargetListIU and rewriteTargetListUD
 * routines also do preprocessing of the targetlist.  The division of labor
 * between here and there is a bit arbitrary and historical.
 *
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/prep/preptlist.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/heapam.h"
#include "access/sysattr.h"
#include "catalog/gp_policy.h"     /* CDB: POLICYTYPE_PARTITIONED */
#include "catalog/pg_inherits_fn.h"
#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "optimizer/plancat.h"
#include "optimizer/prep.h"
#include "optimizer/tlist.h"
#include "parser/parsetree.h"
#include "parser/parse_coerce.h"
#include "parser/parse_relation.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"


static List *expand_targetlist(PlannerInfo *root, List *tlist, int command_type,
				  Index result_relation, List *range_table);
static List *supplement_simply_updatable_targetlist(List *range_table,
													List *tlist);


/*
 * preprocess_targetlist
 *	  Driver for preprocessing the parse tree targetlist.
 *
 *	  Returns the new targetlist.
 */
List *
preprocess_targetlist(PlannerInfo *root, List *tlist)
{
	Query	   *parse = root->parse;
	int			result_relation = parse->resultRelation;
	List	   *range_table = parse->rtable;
	CmdType		command_type = parse->commandType;
	ListCell   *lc;

	/*
	 * Sanity check: if there is a result relation, it'd better be a real
	 * relation not a subquery.  Else parser or rewriter messed up.
	 */
	if (result_relation)
	{
		RangeTblEntry *rte = rt_fetch(result_relation, range_table);

		if (rte->subquery != NULL || rte->relid == InvalidOid)
			elog(ERROR, "subquery cannot be result relation");
	}

	/*
	 * for heap_form_tuple to work, the targetlist must match the exact order
	 * of the attributes. We also need to fill in any missing attributes. -ay
	 * 10/94
	 */
	if (command_type == CMD_INSERT || command_type == CMD_UPDATE)
		tlist = expand_targetlist(root, tlist, command_type,
								  result_relation, range_table);

	/* simply updatable cursors */
	if (root->glob->simplyUpdatable)
		tlist = supplement_simply_updatable_targetlist(range_table, tlist);

	/*
	 * Add necessary junk columns for rowmarked rels.  These values are needed
	 * for locking of rels selected FOR UPDATE/SHARE, and to do EvalPlanQual
	 * rechecking.  See comments for PlanRowMark in plannodes.h.
	 */
	foreach(lc, root->rowMarks)
	{
		PlanRowMark *rc = (PlanRowMark *) lfirst(lc);
		Var		   *var;
		char		resname[32];
		TargetEntry *tle;

		/* child rels use the same junk attrs as their parents */
		if (rc->rti != rc->prti)
			continue;

		if (rc->markType != ROW_MARK_COPY)
		{
			/* It's a regular table, so fetch its TID */
			var = makeVar(rc->rti,
						  SelfItemPointerAttributeNumber,
						  TIDOID,
						  -1,
						  InvalidOid,
						  0);
			snprintf(resname, sizeof(resname), "ctid%u", rc->rowmarkId);
			tle = makeTargetEntry((Expr *) var,
								  list_length(tlist) + 1,
								  pstrdup(resname),
								  true);
			tlist = lappend(tlist, tle);

			/* if parent of inheritance tree, need the tableoid too */
			if (rc->isParent)
			{
				var = makeVar(rc->rti,
							  TableOidAttributeNumber,
							  OIDOID,
							  -1,
							  InvalidOid,
							  0);
				snprintf(resname, sizeof(resname), "tableoid%u", rc->rowmarkId);
				tle = makeTargetEntry((Expr *) var,
									  list_length(tlist) + 1,
									  pstrdup(resname),
									  true);
				tlist = lappend(tlist, tle);
			}
		}
		else
		{
			/* Not a table, so we need the whole row as a junk var */
			var = makeWholeRowVar(rt_fetch(rc->rti, range_table),
								  rc->rti,
								  0,
								  false);
			snprintf(resname, sizeof(resname), "wholerow%u", rc->rowmarkId);
			tle = makeTargetEntry((Expr *) var,
								  list_length(tlist) + 1,
								  pstrdup(resname),
								  true);
			tlist = lappend(tlist, tle);
		}
	}

	/*
	 * If the query has a RETURNING list, add resjunk entries for any Vars
	 * used in RETURNING that belong to other relations.  We need to do this
	 * to make these Vars available for the RETURNING calculation.  Vars that
	 * belong to the result rel don't need to be added, because they will be
	 * made to refer to the actual heap tuple.
	 */
	if (parse->returningList && list_length(parse->rtable) > 1)
	{
		List	   *vars;
		ListCell   *l;

		vars = pull_var_clause((Node *) parse->returningList,
							   PVC_RECURSE_AGGREGATES,
							   PVC_INCLUDE_PLACEHOLDERS);
		foreach(l, vars)
		{
			Var		   *var = (Var *) lfirst(l);
			TargetEntry *tle;

			if (IsA(var, Var) &&
				var->varno == result_relation)
				continue;		/* don't need it */

			if (tlist_member((Node *) var, tlist))
				continue;		/* already got it */

			tle = makeTargetEntry((Expr *) var,
								  list_length(tlist) + 1,
								  NULL,
								  true);

			tlist = lappend(tlist, tle);
		}
		list_free(vars);
	}

	return tlist;
}

/*****************************************************************************
 *
 *		TARGETLIST EXPANSION
 *
 *****************************************************************************/

/*
 * expand_targetlist
 *	  Given a target list as generated by the parser and a result relation,
 *	  add targetlist entries for any missing attributes, and ensure the
 *	  non-junk attributes appear in proper field order.
 */
static List *
expand_targetlist(PlannerInfo *root, List *tlist, int command_type,
				  Index result_relation, List *range_table)
{
	List	   *new_tlist = NIL;
	ListCell   *tlist_item;
	Relation	rel;
	int			attrno,
				numattrs;
	Bitmapset  *changed_cols = NULL;

	tlist_item = list_head(tlist);

	/*
	 * The rewriter should have already ensured that the TLEs are in correct
	 * order; but we have to insert TLEs for any missing attributes.
	 *
	 * Scan the tuple description in the relation's relcache entry to make
	 * sure we have all the user attributes in the right order.  We assume
	 * that the rewriter already acquired at least AccessShareLock on the
	 * relation, so we need no lock here.
	 */
	rel = heap_open(getrelid(result_relation, range_table), NoLock);

	numattrs = RelationGetNumberOfAttributes(rel);

	for (attrno = 1; attrno <= numattrs; attrno++)
	{
		Form_pg_attribute att_tup = rel->rd_att->attrs[attrno - 1];
		TargetEntry *new_tle = NULL;

		if (tlist_item != NULL)
		{
			TargetEntry *old_tle = (TargetEntry *) lfirst(tlist_item);

			if (!old_tle->resjunk && old_tle->resno == attrno)
			{
				new_tle = old_tle;
				tlist_item = lnext(tlist_item);
			}
		}

		/*
		 * GPDB: If it's an UPDATE, keep track of which columns are being
		 * updated, and which ones are just passed through from old relation.
		 * We need that information later, to determine whether this UPDATE
		 * can move tuples from one segment to another.
		 */
		if (new_tle && command_type == CMD_UPDATE)
		{
			bool		col_changed = true;

			/*
			 * The column is unchanged, if the new value is a Var that refers
			 * directly to the same attribute in the same table.
			 */
			if (IsA(new_tle->expr, Var))
			{
				Var		   *var = (Var *) new_tle->expr;

				if (var->varno == result_relation && var->varattno == attrno)
					col_changed = false;
			}

			if (col_changed)
				changed_cols = bms_add_member(changed_cols, attrno);
		}

		if (new_tle == NULL)
		{
			/*
			 * Didn't find a matching tlist entry, so make one.
			 *
			 * For INSERT, generate a NULL constant.  (We assume the rewriter
			 * would have inserted any available default value.) Also, if the
			 * column isn't dropped, apply any domain constraints that might
			 * exist --- this is to catch domain NOT NULL.
			 *
			 * For UPDATE, generate a Var reference to the existing value of
			 * the attribute, so that it gets copied to the new tuple. But
			 * generate a NULL for dropped columns (we want to drop any old
			 * values).
			 *
			 * When generating a NULL constant for a dropped column, we label
			 * it INT4 (any other guaranteed-to-exist datatype would do as
			 * well). We can't label it with the dropped column's datatype
			 * since that might not exist anymore.  It does not really matter
			 * what we claim the type is, since NULL is NULL --- its
			 * representation is datatype-independent.  This could perhaps
			 * confuse code comparing the finished plan to the target
			 * relation, however.
			 */
			Oid			atttype = att_tup->atttypid;
			int32		atttypmod = att_tup->atttypmod;
			Oid			attcollation = att_tup->attcollation;
			Node	   *new_expr;

			switch (command_type)
			{
				case CMD_INSERT:
					if (!att_tup->attisdropped)
					{
						new_expr = (Node *) makeConst(atttype,
													  -1,
													  attcollation,
													  att_tup->attlen,
													  (Datum) 0,
													  true,		/* isnull */
													  att_tup->attbyval);
						new_expr = coerce_to_domain(new_expr,
													InvalidOid, -1,
													atttype,
													COERCE_IMPLICIT_CAST,
													-1,
													false,
													false);
					}
					else
					{
						/* Insert NULL for dropped column */
						new_expr = (Node *) makeConst(INT4OID,
													  -1,
													  InvalidOid,
													  sizeof(int32),
													  (Datum) 0,
													  true,		/* isnull */
													  true /* byval */ );
					}
					break;
				case CMD_UPDATE:
					if (!att_tup->attisdropped)
					{
						new_expr = (Node *) makeVar(result_relation,
													attrno,
													atttype,
													atttypmod,
													attcollation,
													0);
					}
					else
					{
						/* Insert NULL for dropped column */
						new_expr = (Node *) makeConst(INT4OID,
													  -1,
													  InvalidOid,
													  sizeof(int32),
													  (Datum) 0,
													  true,		/* isnull */
													  true /* byval */ );
					}
					break;
				default:
					elog(ERROR, "unrecognized command_type: %d",
						 (int) command_type);
					new_expr = NULL;	/* keep compiler quiet */
					break;
			}

			new_tle = makeTargetEntry((Expr *) new_expr,
									  attrno,
									  pstrdup(NameStr(att_tup->attname)),
									  false);
		}

		new_tlist = lappend(new_tlist, new_tle);
	}


	/*
	 * If an UPDATE can move the tuples from one segment to another, we will
	 * need to create a Split Update node for it. The node is created later
	 * in the planning, but if it's needed, we must ensure that the target
	 * list contains all the original values of each distribution key column,
	 * because the Split Update needs them as input. The old distribution
	 * key columns come in the target list after all the new values, and
	 * before the 'ctid' and other resjunk columns. (The logic in
	 * process_targetlist_for_splitupdate() relies on that order.)
	 */
	if (command_type == CMD_UPDATE)
	{
		GpPolicy   *targetPolicy;
		bool		key_col_updated = false;

		/* Was any distribution key column among the changed columns? */
		targetPolicy = GpPolicyFetch(RelationGetRelid(rel));
		if (targetPolicy->ptype == POLICYTYPE_PARTITIONED)
		{
			int			i;

			for (i = 0; i < targetPolicy->nattrs; i++)
			{
				if (bms_is_member(targetPolicy->attrs[i], changed_cols))
				{
					key_col_updated = true;
					break;
				}
			}
		}

		if (key_col_updated)
		{
			/*
			 * Yes, this is a split update.
			 * Updating a hash column is a split update, of course.
			 *
			 * For each column that was changed, add the original column value
			 * to the target list, if it's not there already.
			 */
			int			i;

			for (i = 0; i < targetPolicy->nattrs; i++)
			{
				AttrNumber	keycolidx = targetPolicy->attrs[i];
				Var		   *origvar;
				Form_pg_attribute att_tup = rel->rd_att->attrs[keycolidx - 1];

				origvar = makeVar(result_relation,
								  keycolidx,
								  att_tup->atttypid,
								  att_tup->atttypmod,
								  att_tup->attcollation,
								  0);
				TargetEntry *new_tle = makeTargetEntry((Expr *) origvar,
													   attrno,
													   NameStr(att_tup->attname),
													   true);
				new_tlist = lappend(new_tlist, new_tle);
				attrno++;
			}

			/* Also add the old OID to the tlist, if the table has OIDs. */
			if (rel->rd_rel->relhasoids)
			{
				TargetEntry *new_tle;
				Var		   *oidvar;

				oidvar = makeVar(result_relation,
								 ObjectIdAttributeNumber,
								 OIDOID,
								 -1,
								 InvalidOid,
								 0);
				new_tle = makeTargetEntry((Expr *) oidvar,
										  attrno,
										  "oid",
										  true);
				new_tlist = lappend(new_tlist, new_tle);
				attrno++;
			}

			/*
			 * Since we just went through a lot of work to determine whether a
			 * Split Update is needed, memorize that in the PlannerInfo, so that
			 * we don't need redo all that work later in the planner, when it's
			 * time to actually create the ModifyTable, and SplitUpdate, node.
			 */
			root->is_split_update = true;
		}
	}

	/*
	 * The remaining tlist entries should be resjunk; append them all to the
	 * end of the new tlist, making sure they have resnos higher than the last
	 * real attribute.  (Note: although the rewriter already did such
	 * renumbering, we have to do it again here in case we are doing an UPDATE
	 * in a table with dropped columns, or an inheritance child table with
	 * extra columns.)
	 */
	while (tlist_item)
	{
		TargetEntry *old_tle = (TargetEntry *) lfirst(tlist_item);

		if (!old_tle->resjunk)
			elog(ERROR, "targetlist is not sorted correctly");
		/* Get the resno right, but don't copy unnecessarily */
		if (old_tle->resno != attrno)
		{
			old_tle = flatCopyTargetEntry(old_tle);
			old_tle->resno = attrno;
		}
		new_tlist = lappend(new_tlist, old_tle);
		attrno++;
		tlist_item = lnext(tlist_item);
	}

	heap_close(rel, NoLock);

	return new_tlist;
}


/*
 * Locate PlanRowMark for given RT index, or return NULL if none
 *
 * This probably ought to be elsewhere, but there's no very good place
 */
PlanRowMark *
get_plan_rowmark(List *rowmarks, Index rtindex)
{
	ListCell   *l;

	foreach(l, rowmarks)
	{
		PlanRowMark *rc = (PlanRowMark *) lfirst(l);

		if (rc->rti == rtindex)
			return rc;
	}
	return NULL;
}


/*
 * supplement_simply_updatable_targetlist
 * 
 * For a simply updatable cursor, we supplement the targetlist with junk
 * metadata for gp_segment_id, ctid, and tableoid. The handling of a CURRENT OF
 * invocation will rely on this junk information, in execCurrentOf(). Thus, in
 * a nutshell, it is the responsibility of this routine to ensure whatever
 * information needed to uniquely identify the currently positioned tuple is
 * available in the tuple itself.
 */
static List *
supplement_simply_updatable_targetlist(List *range_table, List *tlist)
{
	Index varno = extractSimplyUpdatableRTEIndex(range_table);

	/* ctid */
	Var         *varCtid = makeVar(varno,
								   SelfItemPointerAttributeNumber,
								   TIDOID,
								   -1,
								   InvalidOid,
								   0);
	TargetEntry *tleCtid = makeTargetEntry((Expr *) varCtid,
										   list_length(tlist) + 1,   /* resno */
										   pstrdup("ctid"),          /* resname */
										   true);                    /* resjunk */
	tlist = lappend(tlist, tleCtid);

	/* gp_segment_id */
	Oid         reloid 		= InvalidOid,
				vartypeid 	= InvalidOid;
	int32       type_mod 	= -1;
	Oid			type_coll	= InvalidOid;
	reloid = getrelid(varno, range_table);
	get_atttypetypmodcoll(reloid, GpSegmentIdAttributeNumber,
						  &vartypeid, &type_mod, &type_coll);
	Var         *varSegid = makeVar(varno,
									GpSegmentIdAttributeNumber,
									vartypeid,
									type_mod,
									type_coll,
									0);
	TargetEntry *tleSegid = makeTargetEntry((Expr *) varSegid,
											list_length(tlist) + 1,   /* resno */
											pstrdup("gp_segment_id"), /* resname */
											true);                    /* resjunk */

	tlist = lappend(tlist, tleSegid);

	/*
	 * tableoid is only needed in the case of inheritance, in order to supplement 
	 * our ability to uniquely identify a tuple. Without inheritance, we omit tableoid
	 * to avoid the overhead of carrying tableoid for each tuple in the result set.
	 */
	if (find_inheritance_children(reloid, NoLock) != NIL)
	{
		Var         *varTableoid = makeVar(varno,
										   TableOidAttributeNumber,
										   OIDOID,
										   -1,
										   InvalidOid,
										   0);
		TargetEntry *tleTableoid = makeTargetEntry((Expr *) varTableoid,
												   list_length(tlist) + 1,  /* resno */
												   pstrdup("tableoid"),     /* resname */
												   true);                   /* resjunk */
		tlist = lappend(tlist, tleTableoid);
	}
	
	return tlist;
}
