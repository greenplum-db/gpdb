/*-------------------------------------------------------------------------
 *
 * nodeTableFunction.c
 *	 Support routines for scans of enhanced table functions.
 *
 * DESCRIPTION
 *
 *   This code is distinct from ExecFunctionScan due to the nature of
 *   the plans.  A plain table function will be called without an input
 *   subquery, whereas the enhanced table function framework allows 
 *   table functions operating over table input.
 *
 *  Normal Table Function            Enhanced Table Function
 *
 *         (out)                              (out)
 *           |                                  |
 *     (FunctionScan)                  (TableFunctionScan)
 *                                              |
 *                                        (SubqueryScan)
 *
 * INTERFACE ROUTINES
 * 	 ExecTableFunctionScan			sequentially scans a relation.
 *	 ExecTableFunctionNext			retrieve next tuple in sequential order.
 *	 ExecInitTableFunctionScan		creates and initializes a externalscan node.
 *	 ExecEndTableFunctionScan		releases any storage allocated.
 *	 ExecTableFunctionReScan		rescans the relation
 *
 * Portions Copyright (c) 2011, EMC
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodeTableFunction.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "funcapi.h"
#include "tablefuncapi.h"

#include "cdb/cdbvars.h"
#include "executor/executor.h"
#include "executor/nodeTableFunction.h"
#include "nodes/nodeFuncs.h"
#include "parser/parsetree.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"


static TupleTableSlot *TableFunctionNext(TableFunctionState *node);

/* Private structure forward declared in tablefuncapi.h */
typedef struct AnyTableData
{
	ExprContext			*econtext;
	PlanState			*subplan;     /* subplan node */
	TupleDesc            subdesc;     /* tuple descriptor of subplan */
	JunkFilter          *junkfilter;  /* for projection of subplan tuple */
} AnyTableData;

/*
 * TableFunctionNext - ExecScan callback function for table function scans
 */
static TupleTableSlot *
TableFunctionNext(TableFunctionState *node)
{
	MemoryContext        oldcontext  = NULL;
	TupleTableSlot		*slot        = NULL;
	ExprContext			*econtext	 = node->ss.ps.ps_ExprContext;
	bool                 returns_set = node->flinfo.fn_retset;
	HeapTuple            tuple       = NULL;
	TupleDesc            resultdesc  = node->resultdesc;
	Datum                user_result;

	/* Clean up any per-tuple memory */
	ResetExprContext(econtext);

	/* Setup arguments on the first call */
	if (node->is_firstcall)
	{
		int			i = 0;
		ListCell   *lc;

		foreach(lc, node->args)
		{
			ExprState  *argstate = (ExprState *) lfirst(lc);

			if (!argstate)
			{
				/*
				 * The ANYTABLE argument is represented as a NULL in the
				 * arguments list.
				 */
				node->fcinfo->args[i].value = AnyTableGetDatum(node->inputscan);
				node->fcinfo->args[i].isnull = false;
			}
			else
				node->fcinfo->args[i].value = ExecEvalExpr(argstate,
														   econtext,
														   &node->fcinfo->args[i].isnull);
			i++;
		}
		node->is_firstcall = false;
	}

	/*
	 * If all results have been returned by the callback function then
	 * we are done. 
	 */
	if (node->rsinfo.isDone == ExprEndResult)
		return slot;  /* empty slot */

	/* Invoke the user supplied function */
	node->fcinfo->isnull = false;
	oldcontext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);
	user_result = FunctionCallInvoke(node->fcinfo);
	MemoryContextSwitchTo(oldcontext);
	if (node->rsinfo.returnMode != SFRM_ValuePerCall)
	{
		/* FIXME: should support both protocols */
		ereport(ERROR,
				(errcode(ERRCODE_E_R_I_E_SRF_PROTOCOL_VIOLATED),
				 errmsg("table functions must use SFRM_ValuePerCall protocol")));
	}
	if (node->rsinfo.isDone == ExprEndResult)
		return slot;  /* empty slot */

	/* Mark this the last value if the func doesn't return a set */
	if (!returns_set || node->rsinfo.isDone == ExprSingleResult)
		node->rsinfo.isDone = ExprEndResult;

	/* This would only error if the user violated the SRF calling convensions */
	if (returns_set && node->fcinfo->isnull)
	{
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
				 errmsg("function returning set of rows cannot return null value")));
	}

	/* Convert the Datum into tuple and store it into the scan slot */
	if (node->is_rowtype)
	{
		HeapTupleHeader  th;

		if (node->fcinfo->isnull)
		{
			int			i;
			Datum		values[MaxTupleAttributeNumber];
			bool		nulls[MaxTupleAttributeNumber];

			Assert(!returns_set);  /* checked above */
			Assert(resultdesc->natts <= MaxTupleAttributeNumber);
			for (i = 0; i < resultdesc->natts; i++)
				nulls[i] = true;

			/* 
			 * If we get a clean solution to the tuple allocation below we
			 * can use it here as well.  This is less an issue because there
			 * is only a single tuple in this case, so the overhead of a
			 * single palloc is not a big deal.
			 */
			oldcontext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);
			tuple = heap_form_tuple(resultdesc, values, nulls);
			MemoryContextSwitchTo(oldcontext);
		}
		else
		{

			/* Convert returned HeapTupleHeader into a HeapTuple */
			th	  = DatumGetHeapTupleHeader(user_result);
			tuple = &node->tuple;

			ItemPointerSetInvalid(&(tuple->t_self));
			tuple->t_len  = HeapTupleHeaderGetDatumLength(th);
			tuple->t_data = th;

			/* Double check that this tuple is of the expected form */
			if (resultdesc->tdtypeid != HeapTupleHeaderGetTypeId(th))
			{
				ereport(ERROR, 
						(errcode(ERRCODE_E_R_I_E_SRF_PROTOCOL_VIOLATED),
						 errmsg("invalid tuple returned from table function"),
						 errdetail("Returned tuple does not match output "
								   "tuple descriptor.")));
			}
		}
	}
	else
	{
		/* 
		 * TODO: This will allocate memory for each tuple, we should be able 
		 * to get away with fewer pallocs.
		 */
		oldcontext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);
		/* &user_result yields a singleton pointer - make sure only one is read */
		Assert(1 == resultdesc->natts);
		tuple = heap_form_tuple(resultdesc, 
								&user_result, 
								&node->fcinfo->isnull);
		MemoryContextSwitchTo(oldcontext);
	}

	/* 
	 * Store the tuple into the scan slot.
	 *
	 * Note: Tuple should be allocated in the per-row memory context, so they
	 * will be freed automatically when the context is freed, we cannot free
	 * them again here.
	 */
	Assert(tuple);
	slot = ExecStoreHeapTuple(tuple, 
							  node->ss.ss_ScanTupleSlot, 
							  false /* shouldFree */);
	Assert(!TupIsNull(slot));

	node->ss.ss_ScanTupleSlot = slot;
	return slot;
}

/*
 * TableFunctionRecheck -- access method routine to recheck a tuple in EvalPlanQual
 */
static bool
TableFunctionRecheck(TableFunctionState *node, TupleTableSlot *slot)
{
	/* nothing to check */
	return true;
}

/*
 * ExecTableFunction - wrapper around TableFunctionNext
 */
static TupleTableSlot *
ExecTableFunction(PlanState *pstate)
{
	TableFunctionState *node = castNode(TableFunctionState, pstate);

	return ExecScan(&node->ss,
					(ExecScanAccessMtd) TableFunctionNext,
					(ExecScanRecheckMtd) TableFunctionRecheck);
}


/*
 * ExecInitTableFunction - Setup the table function executor 
 */
TableFunctionState *
ExecInitTableFunction(TableFunctionScan *node, EState *estate, int eflags)
{
	TableFunctionState	*scanstate;
	PlanState           *subplan;
	Oid					 funcrettype;
	TypeFuncClass		 functypclass;
	RangeTblFunction *rtfunc;
	FuncExpr            *func;
	ExprContext         *econtext;
	TupleDesc            inputdesc  = NULL;
	TupleDesc			 resultdesc = NULL;
	ListCell   *lc;
	List	   *argstates;

	/* Inner plan is not used, outer plan must be present */
	Assert(innerPlan(node) == NULL);
	Assert(outerPlan(node) != NULL);

	/* Forward scan only */
	Assert(!(eflags & (EXEC_FLAG_MARK | EXEC_FLAG_BACKWARD)));

	/*
	 * Create state structure.
	 */
	scanstate = makeNode(TableFunctionState);
	scanstate->ss.ps.plan  = (Plan *)node;
	scanstate->ss.ps.state = estate;
	scanstate->ss.ps.ExecProcNode = ExecTableFunction;
	scanstate->inputscan   = palloc0(sizeof(AnyTableData));
	scanstate->is_firstcall = true;

	/* Create expression context for the node. */
	ExecAssignExprContext(estate, &scanstate->ss.ps);
	econtext = scanstate->ss.ps.ps_ExprContext;

	/* Initialize child nodes */
	outerPlanState(scanstate) = ExecInitNode(outerPlan(node), estate, eflags);
	subplan   = outerPlanState(scanstate);
	inputdesc = CreateTupleDescCopy(ExecGetResultType(subplan));

	/* 
	 * The funcexpr must be a function call.  This check is to verify that
	 * the planner didn't try to perform constant folding or other inlining
	 * on a function invoked as a table function.
	 */
	rtfunc = node->function;
	if (!rtfunc->funcexpr || !IsA(rtfunc->funcexpr, FuncExpr))
	{
		/* should not be possible */
		elog(ERROR, "table function expression is not a function expression");
	}
	func = (FuncExpr *) rtfunc->funcexpr;
	functypclass = get_expr_result_type((Node*) func, &funcrettype, &resultdesc);
	
	switch (functypclass)
	{
		case TYPEFUNC_COMPOSITE:
		{
			/* Composite data type: copy the typcache entry for safety */
			Assert(resultdesc);
			resultdesc = CreateTupleDescCopy(resultdesc);
			scanstate->is_rowtype = true;
			break;
		}

		case TYPEFUNC_RECORD:
		{
			/* Record data type: Construct tuple desc based on rangeTable */
			resultdesc = BuildDescFromLists(rtfunc->funccolnames,
											rtfunc->funccoltypes,
											rtfunc->funccoltypmods,
											rtfunc->funccolcollations);
			scanstate->is_rowtype = true;
			break;
		}

		case TYPEFUNC_SCALAR:
		{
			/* Scalar data type: Construct a tuple descriptor manually */
			char	   *attname;

			if (rtfunc->funccolnames)
				attname = strVal(linitial(rtfunc->funccolnames));
			else
				attname = NULL;

			resultdesc = CreateTemplateTupleDesc(1);
			TupleDescInitEntry(resultdesc,
							   (AttrNumber) 1,
							   attname,
							   funcrettype,
							   -1,
							   0);
			TupleDescInitEntryCollation(resultdesc,
										(AttrNumber) 1,
										exprCollation(rtfunc->funcexpr));
			scanstate->is_rowtype = false;
			break;
		}

		default:
		{
			/* This should not be possible, it should be caught by parser. */
			elog(ERROR, "table function has unsupported return type");
		}
	}

	/*
	 * For RECORD results, make sure a typmod has been assigned.  (The
	 * function should do this for itself, but let's cover things in case it
	 * doesn't.)
	 */
	BlessTupleDesc(resultdesc);

	/* Initialize scan slot and type */
	ExecInitScanTupleSlot(estate, &scanstate->ss, resultdesc,
						  &TTSOpsHeapTuple);
	scanstate->resultdesc = resultdesc;

	/* Initialize result tuple type and projection info */
	ExecInitResultTypeTL(&scanstate->ss.ps);
	ExecAssignScanProjectionInfo(&scanstate->ss);

	/* Initialize child expressions */
	scanstate->ss.ps.qual =
		ExecInitQual(node->scan.plan.qual, (PlanState *) scanstate);

	/*
	 * Other node-specific setup
	 */
	scanstate->rsinfo.type		   = T_ReturnSetInfo;
	scanstate->rsinfo.econtext	   = econtext;
	scanstate->rsinfo.expectedDesc = resultdesc;
	scanstate->rsinfo.allowedModes = (int) (SFRM_ValuePerCall);
	scanstate->rsinfo.returnMode   = (int) (SFRM_ValuePerCall);
	scanstate->rsinfo.isDone	   = ExprSingleResult;
	scanstate->rsinfo.setResult    = NULL;
	scanstate->rsinfo.setDesc	   = NULL;

	scanstate->userdata = rtfunc->funcuserdata;

	/* setup the AnyTable input */
	scanstate->inputscan->econtext = econtext;
	scanstate->inputscan->subplan  = subplan;
	scanstate->inputscan->subdesc  = inputdesc;

	/* Determine projection information for subplan */
	scanstate->inputscan->junkfilter =
		ExecInitJunkFilter(subplan->plan->targetlist, 
						   NULL  /* slot */);
	BlessTupleDesc(scanstate->inputscan->junkfilter->jf_cleanTupType);

	/*
	 * Prepare for execution of the function.
	 */
	fmgr_info(func->funcid, &scanstate->flinfo);
	if (scanstate->flinfo.fn_nargs != list_length(func->args))
		elog(ERROR, "number of arguments between TableFunctionScan and Fmgrinfo don't match");
	scanstate->fcinfo = palloc0(SizeForFunctionCallInfo(scanstate->flinfo.fn_nargs));

	InitFunctionCallInfoData(*scanstate->fcinfo,		/* Fcinfo  */
							 &scanstate->flinfo,		/* Flinfo  */
							 list_length(func->args), /* Nargs   */
							 InvalidOid,					  /* input_collation */
							 (Node *) scanstate,                   /* Context */
							 (Node *) &(scanstate->rsinfo));       /* ResultInfo */

	/*
	 * Initialize ExprStates for all arguments.
	 */
	int			count = 0;
	argstates = NIL;
	foreach(lc, func->args)
	{
		Expr	   *arg = (Expr *) lfirst(lc);
		ExprState  *argstate;

		if (IsA(arg, TableValueExpr))
		{
			count++;
			argstate = NULL;
		}
		else
		{
			argstate = ExecInitExpr(arg, &scanstate->ss.ps);
		}
		argstates = lappend(argstates, argstate);
	}

	/*
	 * Currently we don't allow table functions with more than one table value
	 * expression arguments, and if they don't have at least one they will be
	 * planned as nodeFunctionScan instead of nodeTableFunctionScan.
	 * Therefore we should have found exactly 1 TableValueExpr above.
	 */
	if (count != 1)
		elog(ERROR, "table functions over multiple TABLE value expressions not yet supported");

	scanstate->args = argstates;

	return scanstate;
}

void
ExecEndTableFunction(TableFunctionState *node)
{
	/* Free the ExprContext */
	ExecFreeExprContext(&node->ss.ps);
	
	/* Clean out the tuple table */
	if (node->ss.ps.ps_ResultTupleSlot)
		ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
	ExecClearTuple(node->ss.ss_ScanTupleSlot);

	/* End the subplans */
	ExecEndNode(outerPlanState(node));
}

void
ExecReScanTableFunction(TableFunctionState *node)
{
	/* TableFunction Planner marks TableFunction nodes as not rescannable */
	if (!node->is_firstcall)
		elog(ERROR, "invalid rescan of TableFunctionScan");
}


/* Callback functions exposed to the user */
TupleDesc 
AnyTable_GetTupleDesc(AnyTable t)
{
	if (t == NULL)
	{
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid null value for anytable type")));
	}
	Assert(t->junkfilter && IsA(t->junkfilter, JunkFilter));

	/* Return the projected tuple descriptor */
	return t->junkfilter->jf_cleanTupType;
}

HeapTuple
AnyTable_GetNextTuple(AnyTable t)
{
	MemoryContext oldcontext;
	TupleTableSlot *slot;

	if (t == NULL)
	{
		ereport(ERROR, 
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid null value for anytable type")));
	}

	/* Fetch the next tuple into the tuple slot */
	oldcontext = MemoryContextSwitchTo(t->econtext->ecxt_per_query_memory);
	t->econtext->ecxt_outertuple = ExecProcNode(t->subplan);
	MemoryContextSwitchTo(oldcontext);
	if (TupIsNull(t->econtext->ecxt_outertuple))
	{
		return (HeapTuple) NULL;
	}

	/* ----------------------------------------
	 * 1) Fetch the tuple from the tuple slot
	 * 2) apply resjunk filtering
	 * 3) copy result into a HeapTuple
	 * ----------------------------------------
	 */

	/* GPDB_91_MERGE_FIXME: We used to call  ExecRemoveJunk here, but it
	 * was removed in the upstream. I copied the implementation of
	 * ExecRemoveJunk here, but based on the commit message (2e852e541c),
	 * I don't think we should be doing this either
	 */
	slot = ExecFilterJunk(t->junkfilter, t->econtext->ecxt_outertuple);
	return ExecCopySlotHeapTuple(slot);
}

/*
 * tf_set_userdata_internal
 * This is the entity of TF_SET_USERDATA() API. Sets bytea datum to
 * RangeTblEntry, which is transported to project function via serialized
 * plan tree.
 */
void
tf_set_userdata_internal(FunctionCallInfo fcinfo, bytea *userdata)
{
	RangeTblFunction *rtfunc;

	if (!fcinfo->context || !IsA(fcinfo->context, RangeTblFunction))
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("expected RangeTblFunction node, found %d",
				 fcinfo->context ? nodeTag(fcinfo->context) : 0)));
	rtfunc = (RangeTblFunction *) fcinfo->context;

	/* Make sure it gets detoasted, but packed is allowed */
	rtfunc->funcuserdata =
		userdata ? pg_detoast_datum_packed(userdata) : NULL;
}

/*
 * tf_get_userdata_internal
 * This is the entity of TF_GET_USERDATA() API. Extracts userdata from
 * its scan node which was transported via serialized plan tree.
 */
bytea *
tf_get_userdata_internal(FunctionCallInfo fcinfo)
{
	bytea	   *userdata;

	if (!fcinfo->context || !IsA(fcinfo->context, TableFunctionState))
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("expected TableFunctionState node, found %d",
				 fcinfo->context ? nodeTag(fcinfo->context) : 0)));

	userdata = ((TableFunctionState *) fcinfo->context)->userdata;
	if (!userdata)
		return NULL;

	/* unpack, just in case */
	return pg_detoast_datum(userdata);
}
