/*-------------------------------------------------------------------------
 *
 * preptlist.c
 *	  Routines to preprocess the parse tree target list
 *
 * For INSERT and UPDATE queries, the targetlist must contain an entry for
 * each attribute of the target relation in the correct order.  For UPDATE and
 * DELETE queries, it must also contain junk tlist entries needed to allow the
 * executor to identify the rows to be updated or deleted.  For all query
 * types, we may need to add junk tlist entries for Vars used in the RETURNING
 * list and row ID information needed for SELECT FOR UPDATE locking and/or
 * EvalPlanQual checking.
 *
 * The query rewrite phase also does preprocessing of the targetlist (see
 * rewriteTargetListIU).  The division of labor between here and there is
 * partially historical, but it's not entirely arbitrary.  In particular,
 * consider an UPDATE across an inheritance tree.  What rewriteTargetListIU
 * does need be done only once (because it depends only on the properties of
 * the parent relation).  What's done here has to be done over again for each
 * child relation, because it depends on the properties of the child, which
 * might be of a different relation type, or have more columns and/or a
 * different column order than the parent.
 *
 * The fact that rewriteTargetListIU sorts non-resjunk tlist entries by column
 * position, which expand_targetlist depends on, violates the above comment
 * because the sorting is only valid for the parent relation.  In inherited
 * UPDATE cases, adjust_inherited_tlist runs in between to take care of fixing
 * the tlists for child tables to keep expand_targetlist happy.  We do it like
 * that because it's faster in typical non-inherited cases.
 *
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/prep/preptlist.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/sysattr.h"
#include "access/table.h"
#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "optimizer/optimizer.h"
#include "optimizer/prep.h"
#include "optimizer/tlist.h"
#include "parser/parsetree.h"
#include "parser/parse_coerce.h"
#include "rewrite/rewriteHandler.h"
#include "utils/rel.h"

#include "catalog/gp_distribution_policy.h"     /* CDB: POLICYTYPE_PARTITIONED */
#include "catalog/pg_inherits.h"
#include "optimizer/plancat.h"
#include "parser/parse_relation.h"
#include "utils/lsyscache.h"

static List *expand_targetlist(PlannerInfo *root, List *tlist, int command_type,
							   Index result_relation, Relation rel);
static List *supplement_simply_updatable_targetlist(PlannerInfo *root,
													List *range_table,
													List *tlist);


/*
 * preprocess_targetlist
 *	  Driver for preprocessing the parse tree targetlist.
 *
 *	  Returns the new targetlist.
 *
 * As a side effect, if there's an ON CONFLICT UPDATE clause, its targetlist
 * is also preprocessed (and updated in-place).
 */
List *
preprocess_targetlist(PlannerInfo *root)
{
	Query	   *parse = root->parse;
	int			result_relation = parse->resultRelation;
	List	   *range_table = parse->rtable;
	CmdType		command_type = parse->commandType;
	RangeTblEntry *target_rte = NULL;
	Relation	target_relation = NULL;
	List	   *tlist;
	ListCell   *lc;

	/*
	 * If there is a result relation, open it so we can look for missing
	 * columns and so on.  We assume that previous code already acquired at
	 * least AccessShareLock on the relation, so we need no lock here.
	 */
	if (result_relation)
	{
		target_rte = rt_fetch(result_relation, range_table);

		/*
		 * Sanity check: it'd better be a real relation not, say, a subquery.
		 * Else parser or rewriter messed up.
		 */
		if (target_rte->rtekind != RTE_RELATION)
			elog(ERROR, "result relation must be a regular relation");

		target_relation = table_open(target_rte->relid, NoLock);
	}
	else
		Assert(command_type == CMD_SELECT);

	/*
	 * For UPDATE/DELETE, add any junk column(s) needed to allow the executor
	 * to identify the rows to be updated or deleted.  Note that this step
	 * scribbles on parse->targetList, which is not very desirable, but we
	 * keep it that way to avoid changing APIs used by FDWs.
	 */
	if (command_type == CMD_UPDATE || command_type == CMD_DELETE)
		rewriteTargetListUD(parse, target_rte, target_relation);

	/*
	 * for heap_form_tuple to work, the targetlist must match the exact order
	 * of the attributes. We also need to fill in any missing attributes. -ay
	 * 10/94
	 */
	tlist = parse->targetList;
	if (command_type == CMD_INSERT || command_type == CMD_UPDATE)
		tlist = expand_targetlist(root, tlist, command_type,
								  result_relation, target_relation);

	/* simply updatable cursors */
	if (root->glob->simplyUpdatableRel != InvalidOid)
		tlist = supplement_simply_updatable_targetlist(root, range_table, tlist);

	/*
	 * Add necessary junk columns for rowmarked rels.  These values are needed
	 * for locking of rels selected FOR UPDATE/SHARE, and to do EvalPlanQual
	 * rechecking.  See comments for PlanRowMark in plannodes.h.  If you
	 * change this stanza, see also expand_inherited_rtentry(), which has to
	 * be able to add on junk columns equivalent to these.
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

		if (rc->allMarkTypes & ~(1 << ROW_MARK_COPY))
		{
			/* Need to fetch TID */
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
		}
		if (rc->allMarkTypes & (1 << ROW_MARK_COPY))
		{
			/* Need the whole row as a junk var */
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

		/* If parent of inheritance tree, always fetch the tableoid too. */
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
							   PVC_RECURSE_AGGREGATES |
							   PVC_RECURSE_WINDOWFUNCS |
							   PVC_INCLUDE_PLACEHOLDERS);
		foreach(l, vars)
		{
			Var		   *var = (Var *) lfirst(l);
			TargetEntry *tle;

			if (IsA(var, Var) &&
				var->varno == result_relation)
				continue;		/* don't need it */

			if (tlist_member((Expr *) var, tlist))
				continue;		/* already got it */

			tle = makeTargetEntry((Expr *) var,
								  list_length(tlist) + 1,
								  NULL,
								  true);

			tlist = lappend(tlist, tle);
		}
		list_free(vars);
	}

	/*
	 * If there's an ON CONFLICT UPDATE clause, preprocess its targetlist too
	 * while we have the relation open.
	 */
	if (parse->onConflict)
		parse->onConflict->onConflictSet =
			expand_targetlist(root, parse->onConflict->onConflictSet,
							  CMD_UPDATE,
							  result_relation,
							  target_relation);

	if (target_relation)
		table_close(target_relation, NoLock);

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
				  Index result_relation, Relation rel)
{
	List	   *new_tlist = NIL;
	ListCell   *tlist_item;
	int			attrno,
				numattrs;
	Bitmapset  *changed_cols = NULL;

	tlist_item = list_head(tlist);

	/*
	 * The rewriter should have already ensured that the TLEs are in correct
	 * order; but we have to insert TLEs for any missing attributes.
	 *
	 * Scan the tuple description in the relation's relcache entry to make
	 * sure we have all the user attributes in the right order.
	 */
	numattrs = RelationGetNumberOfAttributes(rel);

	for (attrno = 1; attrno <= numattrs; attrno++)
	{
		Form_pg_attribute att_tup = TupleDescAttr(rel->rd_att, attrno - 1);
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
													  true, /* isnull */
													  att_tup->attbyval);
						new_expr = coerce_to_domain(new_expr,
													InvalidOid, -1,
													atttype,
													COERCION_IMPLICIT,
													COERCE_IMPLICIT_CAST,
													-1,
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
													  true, /* isnull */
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
													  true, /* isnull */
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
	 * in the planning, but if it's needed, and the table has OIDs, we must
	 * ensure that the target list contains the old OID so that the Split
	 * Update can copy it to the new tuple.
	 *
	 * GPDB_96_MERGE_FIXME: we used to copy all old distribution key columns,
	 * but we only need this for the OID now. Can we desupport Split Updates
	 * on tables with OIDs, and get rid of this?
	 *
	 * GPDB_12_MERGE_FIXME: Tables with special OIDS is now gone. We can
	 * definitely get rid of this now.
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
supplement_simply_updatable_targetlist(PlannerInfo *root, List *range_table, List *tlist)
{
	/*
	 * We determined that this is simply updatable earlier already. Simply
	 * updatable implies that there is exactly one range table entry.
	 * (More might be added later by expanding partitioned tables, but not
	 * yet.) So we should not get here.
	 */
	if (list_length(range_table) != 1)
	{
		Assert(false);
		root->glob->simplyUpdatableRel = InvalidOid;
	}
	Index varno = 1;

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
	reloid = rt_fetch(varno, range_table)->relid;
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
