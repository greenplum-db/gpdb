/*-------------------------------------------------------------------------
 *
 * nodeFunctionscan.c
 *	  Support routines for scanning RangeFunctions (functions in rangetable).
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeFunctionscan.c
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *		ExecFunctionScan		scans a function.
 *		ExecFunctionNext		retrieve next tuple in sequential order.
 *		ExecInitFunctionScan	creates and initializes a functionscan node.
 *		ExecEndFunctionScan		releases any storage allocated.
 *		ExecReScanFunctionScan	rescans the function
 */
#include "postgres.h"

#include "catalog/pg_type.h"
#include "executor/nodeFunctionscan.h"
#include "funcapi.h"
#include "nodes/nodeFuncs.h"
#include "parser/parsetree.h"
#include "utils/builtins.h"

#include "cdb/cdbvars.h"
#include "cdb/memquota.h"
#include "executor/spi.h"
#include "optimizer/var.h"              /* CDB: contain_ctid_var_reference() */


/*
 * Runtime data for each function being scanned.
 */
typedef struct FunctionScanPerFuncState
{
	ExprState  *funcexpr;		/* state of the expression being evaluated */
	TupleDesc	tupdesc;		/* desc of the function result type */
	int			colcount;		/* expected number of result columns */
	Tuplestorestate *tstore;	/* holds the function result set */
	int64		rowcount;		/* # of rows in result set, -1 if not known */
	TupleTableSlot *func_slot;	/* function result slot (or NULL) */
} FunctionScanPerFuncState;

static TupleTableSlot *FunctionNext(FunctionScanState *node);
static void ExecFunctionScanExplainEnd(PlanState *planstate, struct StringInfoData *buf);


/* ----------------------------------------------------------------
 *						Scan Support
 * ----------------------------------------------------------------
 */
/* ----------------------------------------------------------------
 *		FunctionNext
 *
 *		This is a workhorse for ExecFunctionScan
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
FunctionNext(FunctionScanState *node)
{
	EState	   *estate;
	ScanDirection direction;
	TupleTableSlot *scanslot;
	bool		alldone;
	int64		oldpos;
	int			funcno;
	int			att;

	/*
	 * get information from the estate and scan state
	 */
	estate = node->ss.ps.state;
	direction = estate->es_direction;
	scanslot = node->ss.ss_ScanTupleSlot;

	if (node->simple)
	{
		/*
		 * Fast path for the trivial case: the function return type and scan
		 * result type are the same, so we fetch the function result straight
		 * into the scan result slot. No need to update ordinality or
		 * rowcounts either.
		 */
		Tuplestorestate *tstore = node->funcstates[0].tstore;

		/*
		 * If first time through, read all tuples from function and put them
		 * in a tuplestore. Subsequent calls just fetch tuples from
		 * tuplestore.
		 */
		if (tstore == NULL)
		{
			node->funcstates[0].tstore = tstore =
				ExecMakeTableFunctionResult(node->funcstates[0].funcexpr,
											node->ss.ps.ps_ExprContext,
											node->funcstates[0].tupdesc,
											node->eflags & EXEC_FLAG_BACKWARD,
											PlanStateOperatorMemKB( (PlanState *) node));

			/* CDB: Offer extra info for EXPLAIN ANALYZE. */
			if (node->ss.ps.instrument && node->ss.ps.instrument->need_cdb)
			{
				/* Let the tuplestore share our Instrumentation object. */
				tuplestore_set_instrument(tstore, node->ss.ps.instrument);

				/* Request a callback at end of query. */
				node->ss.ps.cdbexplainfun = ExecFunctionScanExplainEnd;
			}

			/*
			 * paranoia - cope if the function, which may have constructed the
			 * tuplestore itself, didn't leave it pointing at the start. This
			 * call is fast, so the overhead shouldn't be an issue.
			 */
			tuplestore_rescan(tstore);
		}

		/*
		 * Get the next tuple from tuplestore.
		 */
		(void) tuplestore_gettupleslot(tstore,
									   ScanDirectionIsForward(direction),
									   false,
									   scanslot);

		if (TupIsNull(scanslot) && !node->ss.ps.delayEagerFree)
		{
			ExecEagerFreeFunctionScan((FunctionScanState *)(&node->ss.ps));
		}

		return scanslot;
	}

	/*
	 * Increment or decrement ordinal counter before checking for end-of-data,
	 * so that we can move off either end of the result by 1 (and no more than
	 * 1) without losing correct count.  See PortalRunSelect for why we can
	 * assume that we won't be called repeatedly in the end-of-data state.
	 */
	oldpos = node->ordinal;
	if (ScanDirectionIsForward(direction))
		node->ordinal++;
	else
		node->ordinal--;

	/*
	 * Main loop over functions.
	 *
	 * We fetch the function results into func_slots (which match the function
	 * return types), and then copy the values to scanslot (which matches the
	 * scan result type), setting the ordinal column (if any) as well.
	 */
	ExecClearTuple(scanslot);
	att = 0;
	alldone = true;
	for (funcno = 0; funcno < node->nfuncs; funcno++)
	{
		FunctionScanPerFuncState *fs = &node->funcstates[funcno];
		int			i;

		/*
		 * If first time through, read all tuples from function and put them
		 * in a tuplestore. Subsequent calls just fetch tuples from
		 * tuplestore.
		 */
		if (fs->tstore == NULL)
		{
			fs->tstore =
				ExecMakeTableFunctionResult(fs->funcexpr,
											node->ss.ps.ps_ExprContext,
											fs->tupdesc,
											node->eflags & EXEC_FLAG_BACKWARD,
											PlanStateOperatorMemKB( (PlanState *) node));

			/* CDB: Offer extra info for EXPLAIN ANALYZE. */
			if (node->ss.ps.instrument && node->ss.ps.instrument->need_cdb)
			{
				/* Let the tuplestore share our Instrumentation object. */
				tuplestore_set_instrument(fs->tstore, node->ss.ps.instrument);

				/* Request a callback at end of query. */
				node->ss.ps.cdbexplainfun = ExecFunctionScanExplainEnd;
			}

			/*
			 * paranoia - cope if the function, which may have constructed the
			 * tuplestore itself, didn't leave it pointing at the start. This
			 * call is fast, so the overhead shouldn't be an issue.
			 */
			tuplestore_rescan(fs->tstore);
		}

		/*
		 * Get the next tuple from tuplestore.
		 *
		 * If we have a rowcount for the function, and we know the previous
		 * read position was out of bounds, don't try the read. This allows
		 * backward scan to work when there are mixed row counts present.
		 */
		if (fs->rowcount != -1 && fs->rowcount < oldpos)
			ExecClearTuple(fs->func_slot);
		else
			(void) tuplestore_gettupleslot(fs->tstore,
										   ScanDirectionIsForward(direction),
										   false,
										   fs->func_slot);

		if (TupIsNull(fs->func_slot))
		{
			/*
			 * If we ran out of data for this function in the forward
			 * direction then we now know how many rows it returned. We need
			 * to know this in order to handle backwards scans. The row count
			 * we store is actually 1+ the actual number, because we have to
			 * position the tuplestore 1 off its end sometimes.
			 */
			if (ScanDirectionIsForward(direction) && fs->rowcount == -1)
				fs->rowcount = node->ordinal;

			/*
			 * populate the result cols with nulls
			 */
			for (i = 0; i < fs->colcount; i++)
			{
				scanslot->PRIVATE_tts_values[att] = (Datum) 0;
				scanslot->PRIVATE_tts_isnull[att] = true;
				att++;
			}
		}
		else
		{
			/*
			 * we have a result, so just copy it to the result cols.
			 */
			slot_getallattrs(fs->func_slot);

			for (i = 0; i < fs->colcount; i++)
			{
				scanslot->PRIVATE_tts_values[att] = slot_getattr(fs->func_slot, i + 1,
														 &scanslot->PRIVATE_tts_isnull[att]);
				att++;
			}

			/* CDB: Label each row with a synthetic ctid for subquery dedup. */
			if (node->cdb_want_ctid)
			{
				HeapTuple   tuple = ExecFetchSlotHeapTuple(scanslot); 

				/* Increment 48-bit row count */
				node->cdb_fake_ctid.ip_posid++;
				if (node->cdb_fake_ctid.ip_posid == 0)
					ItemPointerSetBlockNumber(&node->cdb_fake_ctid,
											  1 + ItemPointerGetBlockNumber(&node->cdb_fake_ctid));

				tuple->t_self = node->cdb_fake_ctid;
			}

			/*
			 * We're not done until every function result is exhausted; we pad
			 * the shorter results with nulls until then.
			 */
			alldone = false;
		}
	}

	/*
	 * ordinal col is always last, per spec.
	 */
	if (node->ordinality)
	{
		scanslot->PRIVATE_tts_values[att] = Int64GetDatumFast(node->ordinal);
		scanslot->PRIVATE_tts_isnull[att] = false;
	}

	/*
	 * If alldone, we just return the previously-cleared scanslot.  Otherwise,
	 * finish creating the virtual tuple.
	 */
	if (!alldone)
		ExecStoreVirtualTuple(scanslot);

	if (TupIsNull(scanslot) && !node->ss.ps.delayEagerFree)
	{
		ExecEagerFreeFunctionScan((FunctionScanState *)(&node->ss.ps));
	}

	return scanslot;
}

/*
 * FunctionRecheck -- access method routine to recheck a tuple in EvalPlanQual
 */
static bool
FunctionRecheck(FunctionScanState *node, TupleTableSlot *slot)
{
	/* nothing to check */
	return true;
}

/* ----------------------------------------------------------------
 *		ExecFunctionScan(node)
 *
 *		Scans the function sequentially and returns the next qualifying
 *		tuple.
 *		We call the ExecScan() routine and pass it the appropriate
 *		access method functions.
 * ----------------------------------------------------------------
 */
TupleTableSlot *
ExecFunctionScan(FunctionScanState *node)
{
	return ExecScan(&node->ss,
					(ExecScanAccessMtd) FunctionNext,
					(ExecScanRecheckMtd) FunctionRecheck);
}

/* ----------------------------------------------------------------
 *		ExecInitFunctionScan
 * ----------------------------------------------------------------
 */
FunctionScanState *
ExecInitFunctionScan(FunctionScan *node, EState *estate, int eflags)
{
	FunctionScanState *scanstate;
	RangeTblEntry *rte = rt_fetch(node->scan.scanrelid,
								  estate->es_range_table);
	int			nfuncs = list_length(node->functions);
	TupleDesc	scan_tupdesc;
	int			i,
				natts;
	ListCell   *lc;

	/* check for unsupported flags */
	Assert(!(eflags & EXEC_FLAG_MARK));

	/*
	 * FunctionScan should not have any children.
	 */
	Assert(outerPlan(node) == NULL);
	Assert(innerPlan(node) == NULL);

	/*
	 * create new ScanState for node
	 */
	scanstate = makeNode(FunctionScanState);
	scanstate->ss.ps.plan = (Plan *) node;
	scanstate->ss.ps.state = estate;
	scanstate->eflags = eflags;

	/*
	 * are we adding an ordinality column?
	 */
	scanstate->ordinality = node->funcordinality;

	scanstate->nfuncs = nfuncs;
	if (nfuncs == 1 && !node->funcordinality)
		scanstate->simple = true;
	else
		scanstate->simple = false;

	/*
	 * Ordinal 0 represents the "before the first row" position.
	 *
	 * We need to track ordinal position even when not adding an ordinality
	 * column to the result, in order to handle backwards scanning properly
	 * with multiple functions with different result sizes. (We can't position
	 * any individual function's tuplestore any more than 1 place beyond its
	 * end, so when scanning backwards, we need to know when to start
	 * including the function in the scan again.)
	 */
	scanstate->ordinal = 0;

	/*
	 * Miscellaneous initialization
	 *
	 * create expression context for node
	 */
	ExecAssignExprContext(estate, &scanstate->ss.ps);

	/*
	 * tuple table initialization
	 */
	ExecInitResultTupleSlot(estate, &scanstate->ss.ps);
	ExecInitScanTupleSlot(estate, &scanstate->ss);

	/*
	 * initialize child expressions
	 */
	scanstate->ss.ps.targetlist = (List *)
		ExecInitExpr((Expr *) node->scan.plan.targetlist,
					 (PlanState *) scanstate);
	scanstate->ss.ps.qual = (List *)
		ExecInitExpr((Expr *) node->scan.plan.qual,
					 (PlanState *) scanstate);

	scanstate->funcstates = palloc(nfuncs * sizeof(FunctionScanPerFuncState));

	natts = 0;
	i = 0;
	foreach(lc, node->functions)
	{
		RangeTblFunction *rtfunc = (RangeTblFunction *) lfirst(lc);
		Node	   *funcexpr = rtfunc->funcexpr;
		int			colcount = rtfunc->funccolcount;
		FunctionScanPerFuncState *fs = &scanstate->funcstates[i];
		TypeFuncClass functypclass;
		Oid			funcrettype;
		TupleDesc	tupdesc;

		fs->funcexpr = ExecInitExpr((Expr *) funcexpr, (PlanState *) scanstate);

		/*
		 * Don't allocate the tuplestores; the actual calls to the functions
		 * do that.  NULL means that we have not called the function yet (or
		 * need to call it again after a rescan).
		 */
		fs->tstore = NULL;
		fs->rowcount = -1;

		/*
		 * Now determine if the function returns a simple or composite type,
		 * and build an appropriate tupdesc.  Note that in the composite case,
		 * the function may now return more columns than it did when the plan
		 * was made; we have to ignore any columns beyond "colcount".
		 */
		functypclass = get_expr_result_type(funcexpr,
											&funcrettype,
											&tupdesc);

		if (functypclass == TYPEFUNC_COMPOSITE)
		{
			/* Composite data type, e.g. a table's row type */
			Assert(tupdesc);
			Assert(tupdesc->natts >= colcount);
			/* Must copy it out of typcache for safety */
			tupdesc = CreateTupleDescCopy(tupdesc);
		}
		else if (functypclass == TYPEFUNC_SCALAR)
		{
			/* Base data type, i.e. scalar */
			tupdesc = CreateTemplateTupleDesc(1, false);
			TupleDescInitEntry(tupdesc,
							   (AttrNumber) 1,
							   NULL,	/* don't care about the name here */
							   funcrettype,
							   -1,
							   0);
			TupleDescInitEntryCollation(tupdesc,
										(AttrNumber) 1,
										exprCollation(funcexpr));
		}
		else if (functypclass == TYPEFUNC_RECORD)
		{
			tupdesc = BuildDescFromLists(rtfunc->funccolnames,
										 rtfunc->funccoltypes,
										 rtfunc->funccoltypmods,
										 rtfunc->funccolcollations);

			/*
			 * For RECORD results, make sure a typmod has been assigned.  (The
			 * function should do this for itself, but let's cover things in
			 * case it doesn't.)
			 */
			BlessTupleDesc(tupdesc);
		}
		else
		{
			/* crummy error message, but parser should have caught this */
			elog(ERROR, "function in FROM has unsupported return type");
		}

		fs->tupdesc = tupdesc;
		fs->colcount = colcount;

		/*
		 * We only need separate slots for the function results if we are
		 * doing ordinality or multiple functions; otherwise, we'll fetch
		 * function results directly into the scan slot.
		 */
		if (!scanstate->simple)
		{
			fs->func_slot = ExecInitExtraTupleSlot(estate);
			ExecSetSlotDescriptor(fs->func_slot, fs->tupdesc);
		}
		else
			fs->func_slot = NULL;

		natts += colcount;
		i++;
	}

	/* Check if targetlist or qual contains a var node referencing the ctid column */
	scanstate->cdb_want_ctid = contain_ctid_var_reference(&node->scan);

    ItemPointerSet(&scanstate->cdb_fake_ctid, 0, 0);
    ItemPointerSet(&scanstate->cdb_mark_ctid, 0, 0);

	/*
	 * Create the combined TupleDesc
	 *
	 * If there is just one function without ordinality, the scan result
	 * tupdesc is the same as the function result tupdesc --- except that we
	 * may stuff new names into it below, so drop any rowtype label.
	 */
	if (scanstate->simple)
	{
		scan_tupdesc = CreateTupleDescCopy(scanstate->funcstates[0].tupdesc);
		scan_tupdesc->tdtypeid = RECORDOID;
		scan_tupdesc->tdtypmod = -1;
	}
	else
	{
		AttrNumber	attno = 0;

		if (node->funcordinality)
			natts++;

		scan_tupdesc = CreateTemplateTupleDesc(natts, false);

		for (i = 0; i < nfuncs; i++)
		{
			TupleDesc	tupdesc = scanstate->funcstates[i].tupdesc;
			int			colcount = scanstate->funcstates[i].colcount;
			int			j;

			for (j = 1; j <= colcount; j++)
				TupleDescCopyEntry(scan_tupdesc, ++attno, tupdesc, j);
		}

		/* If doing ordinality, add a column of type "bigint" at the end */
		if (node->funcordinality)
		{
			TupleDescInitEntry(scan_tupdesc,
							   ++attno,
							   NULL,	/* don't care about the name here */
							   INT8OID,
							   -1,
							   0);
		}

		Assert(attno == natts);
	}

	/*
	 * Make sure the scan result tupdesc has the column names the query
	 * expects.  This affects the output of constructs like row_to_json which
	 * read the column names from the passed-in tupdesc.
	 */
	i = 0;
	foreach(lc, rte->eref->colnames)
	{
		char	   *attname = strVal(lfirst(lc));

		if (i >= scan_tupdesc->natts)
			break;				/* shouldn't happen, but just in case */
		namestrcpy(&(scan_tupdesc->attrs[i]->attname), attname);
		i++;
	}

	ExecAssignScanType(&scanstate->ss, scan_tupdesc);

	/*
	 * Initialize result tuple type and projection info.
	 */
	ExecAssignResultTypeFromTL(&scanstate->ss.ps);
	ExecAssignScanProjectionInfo(&scanstate->ss);
	
	if (!IsResManagerMemoryPolicyNone())
	{
		SPI_ReserveMemory(((Plan *)node)->operatorMemKB * 1024L);
	}

	/*
	 * If eflag contains EXEC_FLAG_REWIND or EXEC_FLAG_BACKWARD or EXEC_FLAG_MARK,
	 * then this node is not eager free safe.
	 */
	scanstate->ss.ps.delayEagerFree =
		((eflags & (EXEC_FLAG_REWIND | EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)) != 0);

	/*
	 * Also don't free eagerly, if there are multiple functions. If a ROWS
	 * FROM() expression with multiple functions is used with a nested loop,
	 * when the param changes, we might need to rescan some, but not all, of
	 * the functions, depending on which function's arguments refer to the
	 * params. In that case, we prefer to avoid the rescans for those
	 * functions that we can.
	 *
	 * (In a nested loop with no params at all, the EXEC_FLAG_REWIND flag is
	 * set. And in the case of a single function, the param must be used as
	 * an arugment of that function, and we'll need to always rescan it.)
	 */
	if (nfuncs > 0)
		scanstate->ss.ps.delayEagerFree = true;

	return scanstate;
}

/*
 * ExecFunctionScanExplainEnd
 *      Called before ExecutorEnd to finish EXPLAIN ANALYZE reporting.
 *
 * The cleanup that ordinarily would occur during ExecutorEnd() needs to be 
 * done earlier in order to report statistics to EXPLAIN ANALYZE.  Note that 
 * ExecEndFunctionScan() will be called for a second time during ExecutorEnd().
 */
void
ExecFunctionScanExplainEnd(PlanState *planstate, struct StringInfoData *buf __attribute__((unused)))
{
	ExecEagerFreeFunctionScan((FunctionScanState *) planstate);
}                               /* ExecFunctionScanExplainEnd */

/* ----------------------------------------------------------------
 *		ExecEndFunctionScan
 *
 *		frees any storage allocated through C routines.
 * ----------------------------------------------------------------
 */
void
ExecEndFunctionScan(FunctionScanState *node)
{
	/*
	 * Free the exprcontext
	 */
	ExecFreeExprContext(&node->ss.ps);

	/*
	 * clean out the tuple table
	 */
	ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
	ExecClearTuple(node->ss.ss_ScanTupleSlot);

	ExecEagerFreeFunctionScan(node);

	EndPlanStateGpmonPkt(&node->ss.ps);
}

/* ----------------------------------------------------------------
 *		ExecReScanFunctionScan
 *
 *		Rescans the relation.
 * ----------------------------------------------------------------
 */
void
ExecReScanFunctionScan(FunctionScanState *node)
{
	FunctionScan *scan = (FunctionScan *) node->ss.ps.plan;
	int			i;
	Bitmapset  *chgparam = node->ss.ps.chgParam;

	ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
	for (i = 0; i < node->nfuncs; i++)
	{
		FunctionScanPerFuncState *fs = &node->funcstates[i];

		if (fs->func_slot)
			ExecClearTuple(fs->func_slot);
	}

	ExecScanReScan(&node->ss);

	ItemPointerSet(&node->cdb_fake_ctid, 0, 0);

	/*
	 * Here we have a choice whether to drop the tuplestores (and recompute
	 * the function outputs) or just rescan them.  We must recompute if an
	 * expression contains changed parameters, else we rescan.
	 *
	 * XXX maybe we should recompute if the function is volatile?  But in
	 * general the executor doesn't conditionalize its actions on that.
	 */
	if (chgparam)
	{
		ListCell   *lc;

		i = 0;
		foreach(lc, scan->functions)
		{
			RangeTblFunction *rtfunc = (RangeTblFunction *) lfirst(lc);

			if (bms_overlap(chgparam, rtfunc->funcparams))
			{
				if (node->funcstates[i].tstore != NULL)
				{
					tuplestore_end(node->funcstates[i].tstore);
					node->funcstates[i].tstore = NULL;
				}
				node->funcstates[i].rowcount = -1;
			}
			i++;
		}
	}

	/* Reset ordinality counter */
	node->ordinal = 0;

	/* Make sure we rewind any remaining tuplestores */
	for (i = 0; i < node->nfuncs; i++)
	{
		if (node->funcstates[i].tstore != NULL)
			tuplestore_rescan(node->funcstates[i].tstore);
	}
}

void
ExecEagerFreeFunctionScan(FunctionScanState *node)
{
	int			i;

	/*
	 * Release slots and tuplestore resources
	 */
	for (i = 0; i < node->nfuncs; i++)
	{
		FunctionScanPerFuncState *fs = &node->funcstates[i];

		if (fs->func_slot)
			ExecClearTuple(fs->func_slot);

		if (fs->tstore != NULL)
		{
			tuplestore_end(node->funcstates[i].tstore);
			fs->tstore = NULL;
		}
	}
}
