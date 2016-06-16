/*
 * execIndexscan.c
 * 	Define common routines that are used by IndexScan, BitmapIndexScan, and DynamicIndexScan nodes.
 *
 * Copyright (c) 2013 - present, EMC/Greenplum
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/nbtree.h"
#include "executor/execIndexscan.h"
#include "executor/executor.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "utils/array.h"
#include "utils/lsyscache.h"

/*
 * InitCommonIndexScanState
 *   Initialize the scan state that is common in IndexScan and DynamicIndexScan.
 */
void
InitCommonIndexScanState(IndexScanState *indexstate, IndexScan *node, EState *estate, int eflags)
{
	Assert(IsA(indexstate, IndexScanState) ||
		IsA(indexstate, DynamicIndexScanState));
	
	indexstate->ss.ps.plan = (Plan *)node;
	indexstate->ss.ps.state = estate;

	/*
	 * Miscellaneous initialization
	 *
	 * create expression context for node
	 */
	ExecAssignExprContext(estate, &indexstate->ss.ps);

	/*
	 * initialize child expressions
	 *
	 * Note: we don't initialize all of the indexqual expression, only the
	 * sub-parts corresponding to runtime keys (see below).  The indexqualorig
	 * expression is always initialized even though it will only be used in
	 * some uncommon cases --- would be nice to improve that.  (Problem is
	 * that any SubPlans present in the expression must be found now...)
	 */
	indexstate->ss.ps.targetlist = (List *)
		ExecInitExpr((Expr *) node->scan.plan.targetlist,
					 (PlanState *) indexstate);
	indexstate->ss.ps.qual = (List *)
		ExecInitExpr((Expr *) node->scan.plan.qual,
					 (PlanState *) indexstate);
	indexstate->indexqualorig = (List *)
		ExecInitExpr((Expr *) node->indexqualorig,
					 (PlanState *) indexstate);

	/*
	 * tuple table initialization
	 */
	ExecInitResultTupleSlot(estate, &indexstate->ss.ps);
	ExecInitScanTupleSlot(estate, &indexstate->ss);
	/* Initialize result tuple type [JIRA: MPP-24151]. */
	ExecAssignResultTypeFromTL((PlanState *)indexstate);

	/*
	 * If eflag contains EXEC_FLAG_REWIND or EXEC_FLAG_BACKWARD or EXEC_FLAG_MARK,
	 * then this node is not eager free safe.
	 */
	indexstate->ss.ps.delayEagerFree =
		((eflags & (EXEC_FLAG_REWIND | EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)) != 0);
}

/*
 * OpenIndexRelation
 *    Open the index relation of the given index oid.
 *
 * If the parent table is one of the target relations of the query, then
 * InitPlan already opened and write-locked the index, so we can avoid
 * taking another lock here.  Otherwise we need a normal reader's lock.
 *
 * The parent table is specified through the range table index.
 */ 
Relation
OpenIndexRelation(EState *estate, Oid indexOid, Index tableRtIndex)
{
	LOCKMODE lockMode = AccessShareLock;
	
	if (ExecRelationIsTargetRelation(estate, tableRtIndex))
	{
		lockMode = NoLock;
	}

	return index_open(indexOid, lockMode);
}

/*
 * ExecIndexBuildScanKeys
 *		Build the index scan keys from the index qualification expressions
 *
 * The index quals are passed to the index AM in the form of a ScanKey array.
 * This routine sets up the ScanKeys, fills in all constant fields of the
 * ScanKeys, and prepares information about the keys that have non-constant
 * comparison values.  We divide index qual expressions into five types:
 *
 * 1. Simple operator with constant comparison value ("indexkey op constant").
 * For these, we just fill in a ScanKey containing the constant value.
 *
 * 2. Simple operator with non-constant value ("indexkey op expression").
 * For these, we create a ScanKey with everything filled in except the
 * expression value, and set up an IndexRuntimeKeyInfo struct to drive
 * evaluation of the expression at the right times.
 *
 * 3. RowCompareExpr ("(indexkey, indexkey, ...) op (expr, expr, ...)").
 * For these, we create a header ScanKey plus a subsidiary ScanKey array,
 * as specified in access/skey.h.  The elements of the row comparison
 * can have either constant or non-constant comparison values.
 *
 * 4. ScalarArrayOpExpr ("indexkey op ANY (array-expression)").  For these,
 * we create a ScanKey with everything filled in except the comparison value,
 * and set up an IndexArrayKeyInfo struct to drive processing of the qual.
 * (Note that we treat all array-expressions as requiring runtime evaluation,
 * even if they happen to be constants.)
 *
 * 5. NullTest ("indexkey IS NULL").  We just fill in the ScanKey properly.
 *
 * Input params are:
 *
 * planstate: executor state node we are working for
 * index: the index we are building scan keys for
 * quals: indexquals expressions
 * strategies: associated operator strategy numbers
 * subtypes: associated operator subtype OIDs
 *
 * (Any elements of the strategies and subtypes lists that correspond to
 * RowCompareExpr quals are not used here; instead we look up the info
 * afresh.)
 *
 * Output params are:
 *
 * *scanKeys: receives ptr to array of ScanKeys
 * *numScanKeys: receives number of scankeys
 * *runtimeKeys: receives ptr to array of IndexRuntimeKeyInfos, or NULL if none
 * *numRuntimeKeys: receives number of runtime keys
 * *arrayKeys: receives ptr to array of IndexArrayKeyInfos, or NULL if none
 * *numArrayKeys: receives number of array keys
 *
 * Caller may pass NULL for arrayKeys and numArrayKeys to indicate that
 * ScalarArrayOpExpr quals are not supported.
 */

void
ExecIndexBuildScanKeys(PlanState *planstate, Relation index,
					   List *quals, List *strategies, List *subtypes,
					   ScanKey *scanKeys, int *numScanKeys,
					   IndexRuntimeKeyInfo **runtimeKeys, int *numRuntimeKeys,
					   IndexArrayKeyInfo **arrayKeys, int *numArrayKeys)
{
	ListCell   *qual_cell;
	ListCell   *strategy_cell;
	ListCell   *subtype_cell;
	ScanKey		scan_keys;
	IndexRuntimeKeyInfo *runtime_keys;
	IndexArrayKeyInfo *array_keys;
	int			n_scan_keys;
	int			extra_scan_keys;
	int			n_runtime_keys;
	int			n_array_keys;
	int			j;

	/*
	 * If there are any RowCompareExpr quals, we need extra ScanKey entries
	 * for them, and possibly extra runtime-key entries.  Count up what's
	 * needed.	(The subsidiary ScanKey arrays for the RowCompareExprs could
	 * be allocated as separate chunks, but we have to count anyway to make
	 * runtime_keys large enough, so might as well just do one palloc.)
	 */
	n_scan_keys = list_length(quals);
	extra_scan_keys = 0;
	foreach(qual_cell, quals)
	{
		if (IsA(lfirst(qual_cell), RowCompareExpr))
			extra_scan_keys +=
				list_length(((RowCompareExpr *) lfirst(qual_cell))->opnos);
	}
	scan_keys = (ScanKey)
		palloc((n_scan_keys + extra_scan_keys) * sizeof(ScanKeyData));
	/* Allocate these arrays as large as they could possibly need to be */
	runtime_keys = (IndexRuntimeKeyInfo *)
		palloc((n_scan_keys + extra_scan_keys) * sizeof(IndexRuntimeKeyInfo));
	array_keys = (IndexArrayKeyInfo *)
		palloc0(n_scan_keys * sizeof(IndexArrayKeyInfo));
	n_runtime_keys = 0;
	n_array_keys = 0;

	/*
	 * Below here, extra_scan_keys is index of first cell to use for next
	 * RowCompareExpr
	 */
	extra_scan_keys = n_scan_keys;

	/*
	 * for each opclause in the given qual, convert each qual's opclause into
	 * a single scan key
	 */
	qual_cell = list_head(quals);
	strategy_cell = list_head(strategies);
	subtype_cell = list_head(subtypes);

	for (j = 0; j < n_scan_keys; j++)
	{
		ScanKey		this_scan_key = &scan_keys[j];
		Expr	   *clause;		/* one clause of index qual */
		RegProcedure opfuncid;	/* operator proc id used in scan */
		StrategyNumber strategy;	/* op's strategy number */
		Oid			subtype;	/* op's strategy subtype */
		Expr	   *leftop;		/* expr on lhs of operator */
		Expr	   *rightop;	/* expr on rhs ... */
		AttrNumber	varattno;	/* att number used in scan */

		/*
		 * extract clause information from the qualification
		 */
		clause = (Expr *) lfirst(qual_cell);
		qual_cell = lnext(qual_cell);
		strategy = lfirst_int(strategy_cell);
		strategy_cell = lnext(strategy_cell);
		subtype = lfirst_oid(subtype_cell);
		subtype_cell = lnext(subtype_cell);

		if (IsA(clause, OpExpr))
		{
			/* indexkey op const or indexkey op expression */
			int			flags = 0;
			Datum		scanvalue;

			opfuncid = ((OpExpr *) clause)->opfuncid;

			/*
			 * leftop should be the index key Var, possibly relabeled
			 */
			leftop = (Expr *) get_leftop(clause);

			if (leftop && IsA(leftop, RelabelType))
				leftop = ((RelabelType *) leftop)->arg;

			Assert(leftop != NULL);

			if (!(IsA(leftop, Var) &&
				  var_is_rel((Var *) leftop)))
				insist_log(false,"indexqual doesn't have key on left side");

			varattno = ((Var *) leftop)->varattno;

			/*
			 * rightop is the constant or variable comparison value
			 */
			rightop = (Expr *) get_rightop(clause);

			if (rightop && IsA(rightop, RelabelType))
				rightop = ((RelabelType *) rightop)->arg;

			Assert(rightop != NULL);

			if (IsA(rightop, Const))
			{
				/* OK, simple constant comparison value */
				scanvalue = ((Const *) rightop)->constvalue;
				if (((Const *) rightop)->constisnull)
					flags |= SK_ISNULL;
			}
			else
			{
				/* Need to treat this one as a runtime key */
				runtime_keys[n_runtime_keys].scan_key = this_scan_key;
				runtime_keys[n_runtime_keys].key_expr =
					ExecInitExpr(rightop, planstate);
				n_runtime_keys++;
				scanvalue = (Datum) 0;
			}

			/*
			 * initialize the scan key's fields appropriately
			 */
			ScanKeyEntryInitialize(this_scan_key,
								   flags,
								   varattno,	/* attribute number to scan */
								   strategy,	/* op's strategy */
								   subtype,		/* strategy subtype */
								   opfuncid,	/* reg proc to use */
								   scanvalue);	/* constant */
		}
		else if (IsA(clause, RowCompareExpr))
		{
			/* (indexkey, indexkey, ...) op (expression, expression, ...) */
			RowCompareExpr *rc = (RowCompareExpr *) clause;
			ListCell   *largs_cell = list_head(rc->largs);
			ListCell   *rargs_cell = list_head(rc->rargs);
			ListCell   *opnos_cell = list_head(rc->opnos);
			ScanKey		first_sub_key = &scan_keys[extra_scan_keys];

			/* Scan RowCompare columns and generate subsidiary ScanKey items */
			while (opnos_cell != NULL)
			{
				ScanKey		this_sub_key = &scan_keys[extra_scan_keys];
				int			flags = SK_ROW_MEMBER;
				Datum		scanvalue;
				Oid			opno;
				Oid			opfamily;
				int			op_strategy;
				Oid			op_lefttype;
				Oid			op_righttype;
				bool		op_recheck;

				/*
				 * leftop should be the index key Var, possibly relabeled
				 */
				leftop = (Expr *) lfirst(largs_cell);
				largs_cell = lnext(largs_cell);

				if (leftop && IsA(leftop, RelabelType))
					leftop = ((RelabelType *) leftop)->arg;

				Assert(leftop != NULL);

				if (!(IsA(leftop, Var) &&
					  var_is_rel((Var *) leftop)))
					insist_log(false,"indexqual doesn't have key on left side");

				varattno = ((Var *) leftop)->varattno;

				/*
				 * rightop is the constant or variable comparison value
				 */
				rightop = (Expr *) lfirst(rargs_cell);
				rargs_cell = lnext(rargs_cell);

				if (rightop && IsA(rightop, RelabelType))
					rightop = ((RelabelType *) rightop)->arg;

				Assert(rightop != NULL);

				if (IsA(rightop, Const))
				{
					/* OK, simple constant comparison value */
					scanvalue = ((Const *) rightop)->constvalue;
					if (((Const *) rightop)->constisnull)
						flags |= SK_ISNULL;
				}
				else
				{
					/* Need to treat this one as a runtime key */
					runtime_keys[n_runtime_keys].scan_key = this_sub_key;
					runtime_keys[n_runtime_keys].key_expr =
						ExecInitExpr(rightop, planstate);
					n_runtime_keys++;
					scanvalue = (Datum) 0;
				}

				/*
				 * We have to look up the operator's associated btree support
				 * function
				 */
				opno = lfirst_oid(opnos_cell);
				opnos_cell = lnext(opnos_cell);

				if (index->rd_rel->relam != BTREE_AM_OID ||
					varattno < 1 || varattno > index->rd_index->indnatts)
					insist_log(false, "bogus RowCompare index qualification");
				opfamily = index->rd_opfamily[varattno - 1];

				get_op_opfamily_properties(opno, opfamily,
										   &op_strategy, &op_lefttype, &op_righttype, &op_recheck);

				insist_log(op_strategy == rc->rctype, "RowCompare index qualification contains wrong operator");

				opfuncid = get_opfamily_proc(opfamily, op_lefttype, op_righttype, BTORDER_PROC);

				/*
				 * initialize the subsidiary scan key's fields appropriately
				 */
				ScanKeyEntryInitialize(this_sub_key,
									   flags,
									   varattno,		/* attribute number */
									   op_strategy,		/* op's strategy */
									   op_lefttype,		/* strategy subtype */
									   opfuncid,		/* reg proc to use */
									   scanvalue);		/* constant */
				extra_scan_keys++;
			}

			/* Mark the last subsidiary scankey correctly */
			scan_keys[extra_scan_keys - 1].sk_flags |= SK_ROW_END;

			/*
			 * We don't use ScanKeyEntryInitialize for the header because it
			 * isn't going to contain a valid sk_func pointer.
			 */
			MemSet(this_scan_key, 0, sizeof(ScanKeyData));
			this_scan_key->sk_flags = SK_ROW_HEADER;
			this_scan_key->sk_attno = first_sub_key->sk_attno;
			this_scan_key->sk_strategy = rc->rctype;
			/* sk_subtype, sk_func not used in a header */
			this_scan_key->sk_argument = PointerGetDatum(first_sub_key);
		}
		else if (IsA(clause, ScalarArrayOpExpr))
		{
			/* indexkey op ANY (array-expression) */
			ScalarArrayOpExpr *saop = (ScalarArrayOpExpr *) clause;

			Assert(saop->useOr);
			opfuncid = saop->opfuncid;

			/*
			 * leftop should be the index key Var, possibly relabeled
			 */
			leftop = (Expr *) linitial(saop->args);

			if (leftop && IsA(leftop, RelabelType))
				leftop = ((RelabelType *) leftop)->arg;

			Assert(leftop != NULL);

			if (!(IsA(leftop, Var) &&
				  var_is_rel((Var *) leftop)))
				insist_log(false,"indexqual doesn't have key on left side");

			varattno = ((Var *) leftop)->varattno;

			/*
			 * rightop is the constant or variable array value
			 */
			rightop = (Expr *) lsecond(saop->args);

			if (rightop && IsA(rightop, RelabelType))
				rightop = ((RelabelType *) rightop)->arg;

			Assert(rightop != NULL);

			array_keys[n_array_keys].scan_key = this_scan_key;
			array_keys[n_array_keys].array_expr =
				ExecInitExpr(rightop, planstate);
			/* the remaining fields were zeroed by palloc0 */
			n_array_keys++;

			/*
			 * initialize the scan key's fields appropriately
			 */
			ScanKeyEntryInitialize(this_scan_key,
								   0,	/* flags */
								   varattno,	/* attribute number to scan */
								   strategy,	/* op's strategy */
								   subtype,		/* strategy subtype */
								   opfuncid,	/* reg proc to use */
								   (Datum) 0);	/* constant */
		}
		else if (IsA(clause, NullTest))
		{
			/* indexkey IS NULL */
			Assert(((NullTest *) clause)->nulltesttype == IS_NULL);

			/*
			 * argument should be the index key Var, possibly relabeled
			 */
			leftop = ((NullTest *) clause)->arg;

			if (leftop && IsA(leftop, RelabelType))
				leftop = ((RelabelType *) leftop)->arg;

			Assert(leftop != NULL);

			if (!(IsA(leftop, Var) &&
				  var_is_rel((Var *) leftop)))
				elog(ERROR, "NullTest indexqual has wrong key");

			varattno = ((Var *) leftop)->varattno;

			/*
			 * initialize the scan key's fields appropriately
			 */
			ScanKeyEntryInitialize(this_scan_key,
								   SK_ISNULL | SK_SEARCHNULL,
								   varattno,	/* attribute number to scan */
								   strategy,	/* op's strategy */
								   subtype,		/* strategy subtype */
								   InvalidOid,	/* no reg proc for this */
								   (Datum) 0);	/* constant */
		}
		else
			insist_log(false, "unsupported indexqual type: %d",
				 (int) nodeTag(clause));
	}

	/* Get rid of any unused arrays */
	if (n_runtime_keys == 0)
	{
		pfree(runtime_keys);
		runtime_keys = NULL;
	}
	if (n_array_keys == 0)
	{
		pfree(array_keys);
		array_keys = NULL;
	}

	/*
	 * Return info to our caller.
	 */
	*scanKeys = scan_keys;
	*numScanKeys = n_scan_keys;
	*runtimeKeys = runtime_keys;
	*numRuntimeKeys = n_runtime_keys;
	if (arrayKeys)
	{
		*arrayKeys = array_keys;
		*numArrayKeys = n_array_keys;
	}
	else if (n_array_keys != 0)
		insist_log(false, "ScalarArrayOpExpr index qual found where not allowed");
}

/*
 * InitRuntimeKeysContext
 *   Initialize the context for runtime keys.
 */
void
InitRuntimeKeysContext(IndexScanState *indexstate)
{
	EState *estate = indexstate->ss.ps.state;
	Assert(estate != NULL);

	if (indexstate->iss_RuntimeContext == NULL)
	{
		ExprContext *stdecontext = indexstate->ss.ps.ps_ExprContext;

		ExecAssignExprContext(estate, &indexstate->ss.ps);
		indexstate->iss_RuntimeContext = indexstate->ss.ps.ps_ExprContext;
		indexstate->ss.ps.ps_ExprContext = stdecontext;
	}
}

/*
 * FreeRuntimeKeysContext
 *   Frees the expression context for runtime keys.
 */
void
FreeRuntimeKeysContext(IndexScanState *indexstate)
{
	if (indexstate->iss_RuntimeContext != NULL)
	{
		FreeExprContext(indexstate->iss_RuntimeContext);
		indexstate->iss_RuntimeContext = NULL;
	}
}

/* EOF */
