/*-------------------------------------------------------------------------
 *
 * explain.c
 *	  Explain query execution plans
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994-5, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/commands/explain.c,v 1.185 2009/04/05 19:59:39 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/xact.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_type.h"
#include "commands/explain.h"
#include "commands/prepare.h"
#include "commands/trigger.h"
#include "commands/queue.h"
#include "executor/execUtils.h"
#include "executor/instrument.h"
#include "nodes/pg_list.h"
#include "nodes/print.h"
#include "optimizer/clauses.h"
#include "optimizer/planner.h"
#include "optimizer/var.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteHandler.h"
#include "tcop/tcopprot.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/json.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"             /* AllocSetContextCreate() */
#include "utils/tuplesort.h"
#include "utils/snapmgr.h"

#include "cdb/cdbdisp.h"                /* CheckDispatchResult() */
#include "cdb/cdbexplain.h"             /* cdbexplain_recvExecStats */
#include "cdb/cdbpartition.h"
#include "cdb/cdbpullup.h"              /* cdbpullup_targetlist() */
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbpathlocus.h"
#include "cdb/memquota.h"
#include "miscadmin.h"
#include "utils/resscheduler.h"

#ifdef USE_ORCA
extern char *SzDXLPlan(Query *parse);
extern const char *OptVersion();
#endif


/* Hook for plugins to get control in ExplainOneQuery() */
ExplainOneQuery_hook_type ExplainOneQuery_hook = NULL;

/* Hook for plugins to get control in explain_get_index_name() */
explain_get_index_name_hook_type explain_get_index_name_hook = NULL;


typedef struct ExplainState
{
	/* options */
	bool		printAnalyze;	/* print actual times */
	/* other states */
	PlannedStmt *pstmt;			/* top of plan */
	List	   *rtable;			/* range table */

    /* CDB */
    struct CdbExplain_ShowStatCtx  *showstatctx;    /* EXPLAIN ANALYZE info */
    Slice          *currentSlice;   /* slice whose nodes we are visiting */
    ErrorData      *deferredError;  /* caught error to be re-thrown */
} ExplainState;

extern bool Test_print_direct_dispatch_info;

static void ExplainOneQuery(Query *query, ExplainStmt *stmt,
				const char *queryString,
				ParamListInfo params, TupOutputState *tstate);
static void report_triggers(ResultRelInfo *rInfo, bool show_relname,
				StringInfo buf);

#ifdef USE_ORCA
static void ExplainDXL(Query *query, ExplainStmt *stmt,
							const char *queryString,
							ParamListInfo params, TupOutputState *tstate);
#endif
#ifdef USE_CODEGEN
static void ExplainCodegen(PlanState *planstate, TupOutputState *tstate);
#endif
static double elapsed_time(instr_time *starttime);
static ErrorData *explain_defer_error(ExplainState *es);
static void explain_outNode(StringInfo str,
				Plan *plan, PlanState *planstate,
				Plan *outer_plan, Plan *parentPlan,
				int indent, ExplainState *es);
static void show_scan_qual(List *qual, const char *qlabel,
			   int scanrelid, Plan *scan_plan, Plan *outer_plan,
			   StringInfo str, int indent, ExplainState *es);
static void show_upper_qual(List *qual, const char *qlabel, Plan *plan,
				StringInfo str, int indent, ExplainState *es);
static void show_sort_keys(Plan *sortplan, int nkeys, AttrNumber *keycols,
			   const char *qlabel,
			   StringInfo str, int indent, ExplainState *es);
static const char *explain_get_index_name(Oid indexId);

static void
show_grouping_keys(Plan        *plan,
                   int          numCols,
                   AttrNumber  *subplanColIdx,
                   const char  *qlabel,
			       StringInfo str, int indent, ExplainState *es);
static void
show_motion_keys(Plan *plan, List *hashExpr, int nkeys, AttrNumber *keycols,
			     const char *qlabel,
                 StringInfo str, int indent, ExplainState *es);

static void
explain_partition_selector(PartitionSelector *ps, Plan *parent,
						   StringInfo str, int indent, ExplainState *es);

/*
 * ExplainQuery -
 *	  execute an EXPLAIN command
 */
void
ExplainQuery(ExplainStmt *stmt, const char *queryString,
			 ParamListInfo params, DestReceiver *dest)
{
	Oid		   *param_types;
	int			num_params;
	TupOutputState *tstate;
	List	   *rewritten;
	ListCell   *l;

	/* Convert parameter type data to the form parser wants */
	getParamListTypes(params, &param_types, &num_params);

	/*
	 * Run parse analysis and rewrite.	Note this also acquires sufficient
	 * locks on the source table(s).
	 *
	 * Because the parser and planner tend to scribble on their input, we make
	 * a preliminary copy of the source querytree.	This prevents problems in
	 * the case that the EXPLAIN is in a portal or plpgsql function and is
	 * executed repeatedly.  (See also the same hack in DECLARE CURSOR and
	 * PREPARE.)  XXX FIXME someday.
	 */
	rewritten = pg_analyze_and_rewrite((Node *) copyObject(stmt->query),
									   queryString, param_types, num_params);

	/* prepare for projection of tuples */
	tstate = begin_tup_output_tupdesc(dest, ExplainResultDesc(stmt));

	if (rewritten == NIL)
	{
		/* In the case of an INSTEAD NOTHING, tell at least that */
		do_text_output_oneline(tstate, "Query rewrites to nothing");
	}
	else
	{
		/* Explain every plan */
		foreach(l, rewritten)
		{
			ExplainOneQuery((Query *) lfirst(l), stmt,
							queryString, params, tstate);
			/* put a blank line between plans */
			if (lnext(l) != NULL)
				do_text_output_oneline(tstate, "");
		}
	}

	end_tup_output(tstate);
}

/*
 * ExplainResultDesc -
 *	  construct the result tupledesc for an EXPLAIN
 */
TupleDesc
ExplainResultDesc(ExplainStmt *stmt)
{
	TupleDesc	tupdesc;

	/* need a tuple descriptor representing a single TEXT column */
	tupdesc = CreateTemplateTupleDesc(1, false);
	TupleDescInitEntry(tupdesc, (AttrNumber) 1, "QUERY PLAN",
					   TEXTOID, -1, 0);
	return tupdesc;
}

#ifdef USE_ORCA
/*
 * ExplainDXL -
 *	  print out the execution plan for one Query in DXL format
 *	  this function implicitly uses optimizer
 */
static void
ExplainDXL(Query *query, ExplainStmt *stmt, const char *queryString,
				ParamListInfo params, TupOutputState *tstate)
{
	MemoryContext oldcxt = CurrentMemoryContext;
	ExplainState explainState;
	ExplainState *es = &explainState;
	StringInfoData buf;
	bool		save_enumerate;

	/* Initialize ExplainState structure. */
	memset(es, 0, sizeof(*es));
	es->showstatctx = NULL;
	es->deferredError = NULL;
	es->pstmt = NULL;

	initStringInfo(&buf);

	save_enumerate = optimizer_enumerate_plans;

	/* Do the EXPLAIN. */
	PG_TRY();
	{
		// enable plan enumeration before calling optimizer
		optimizer_enumerate_plans = true;

		// optimize query using optimizer and get generated plan in DXL format
		char *dxl = SzDXLPlan(query);

		// restore old value of enumerate plans GUC
		optimizer_enumerate_plans = save_enumerate;

		if (dxl == NULL)
			elog(NOTICE, "Optimizer failed to produce plan");
		else
		{
			do_text_output_multiline(tstate, dxl);
			do_text_output_oneline(tstate, ""); /* separator line */
			pfree(dxl);
		}

		/* Free the memory we used. */
		MemoryContextSwitchTo(oldcxt);
	}
	PG_CATCH();
	{
		// restore old value of enumerate plans GUC
		optimizer_enumerate_plans = save_enumerate;

		/* Exit to next error handler. */
		PG_RE_THROW();
	}
	PG_END_TRY();
}
#endif

/*
 * ExplainOneQuery -
 *	  print out the execution plan for one Query
 */
static void
ExplainOneQuery(Query *query, ExplainStmt *stmt, const char *queryString,
				ParamListInfo params, TupOutputState *tstate)
{
#ifdef USE_ORCA
    if (stmt->dxl)
    {
    	ExplainDXL(query, stmt, queryString, params, tstate);
    	return;
    }
#endif

	/* planner will not cope with utility statements */
	if (query->commandType == CMD_UTILITY)
	{
		ExplainOneUtility(query->utilityStmt, stmt,
						  queryString, params, tstate);
		return;
	}

	/* if an advisor plugin is present, let it manage things */
	if (ExplainOneQuery_hook)
		(*ExplainOneQuery_hook) (query, stmt, queryString, params, tstate);
	else
	{
		PlannedStmt *plan;

		/* plan the query */
		plan = planner(query, 0, params);

		/* run it (if needed) and produce output */
		ExplainOnePlan(plan, params, stmt, queryString, tstate);
	}
}

/*
 * ExplainOneUtility -
 *	  print out the execution plan for one utility statement
 *	  (In general, utility statements don't have plans, but there are some
 *	  we treat as special cases)
 *
 * This is exported because it's called back from prepare.c in the
 * EXPLAIN EXECUTE case
 */
void
ExplainOneUtility(Node *utilityStmt, ExplainStmt *stmt,
				  const char *queryString, ParamListInfo params,
				  TupOutputState *tstate)
{
	if (utilityStmt == NULL)
		return;

	if (IsA(utilityStmt, ExecuteStmt))
		ExplainExecuteQuery((ExecuteStmt *) utilityStmt, stmt,
							queryString, params, tstate);
	else if (IsA(utilityStmt, NotifyStmt))
		do_text_output_oneline(tstate, "NOTIFY");
	else
		do_text_output_oneline(tstate,
							   "Utility statements have no plan structure");
}

#ifdef USE_CODEGEN
/*
 * ExplainCodegen -
 * 		given a PlanState tree, traverse its nodes, collect any accumulated
 * 		explain strings from the state's CodegenManager, and print to EXPLAIN
 * 		output
 * 		NB: This method does not recurse into sub plans at this point.
 */
static void
ExplainCodegen(PlanState *planstate, TupOutputState *tstate) {
	if (NULL == planstate) {
		return;
	}

	Assert(NULL != tstate);

	ExplainCodegen(planstate->lefttree, tstate);

	char* str = CodeGeneratorManagerGetExplainString(planstate->CodegenManager);
	Assert(NULL != str);
	do_text_output_oneline(tstate, str);

	ExplainCodegen(planstate->righttree, tstate);
}
#endif

/*
 * ExplainOnePlan -
 *		given a planned query, execute it if needed, and then print
 *		EXPLAIN output
 *
 * Since we ignore any DeclareCursorStmt that might be attached to the query,
 * if you say EXPLAIN ANALYZE DECLARE CURSOR then we'll actually run the
 * query.  This is different from pre-8.3 behavior but seems more useful than
 * not running the query.  No cursor will be created, however.
 *
 * This is exported because it's called back from prepare.c in the
 * EXPLAIN EXECUTE case, and because an index advisor plugin would need
 * to call it.
 */
void
ExplainOnePlan(PlannedStmt *plannedstmt, ParamListInfo params,
			   ExplainStmt *stmt, const char *queryString, TupOutputState *tstate)
{
	QueryDesc  *queryDesc;
	instr_time	starttime;
	double		totaltime = 0;
	ExplainState *es;
	StringInfoData buf;
	EState     *estate = NULL;
	int			eflags;
	char	   *settings;
	MemoryContext explaincxt = CurrentMemoryContext;
	int			instrument_option = 0;

	if (stmt->analyze)
		instrument_option |= INSTRUMENT_TIMER;

	/*
	 * Update snapshot command ID to ensure this query sees results of any
	 * previously executed queries.  (It's a bit cheesy to modify
	 * ActiveSnapshot without making a copy, but for the limited ways in which
	 * EXPLAIN can be invoked, I think it's OK, because the active snapshot
	 * shouldn't be shared with anything else anyway.)
	 */
	ActiveSnapshot->curcid = GetCurrentCommandId(false);

	/* Create a QueryDesc requesting no output */
	queryDesc = CreateQueryDesc(plannedstmt,
								queryString,
								ActiveSnapshot, InvalidSnapshot,
								None_Receiver, params,
								instrument_option);

	if (gp_enable_gpperfmon && Gp_role == GP_ROLE_DISPATCH)
	{
		Assert(queryString);
		gpmon_qlog_query_submit(queryDesc->gpmon_pkt);
		gpmon_qlog_query_text(queryDesc->gpmon_pkt,
				queryString,
				application_name,
				GetResqueueName(GetResQueueId()),
				GetResqueuePriority(GetResQueueId()));
	}

	/* Initialize ExplainState structure. */
	es = (ExplainState *) palloc0(sizeof(ExplainState));
	es->pstmt = queryDesc->plannedstmt;

    /*
     * Start timing.
     */
    INSTR_TIME_SET_CURRENT(starttime);

	/* If analyzing, we need to cope with queued triggers */
	if (stmt->analyze)
		AfterTriggerBeginQuery();

    /* Allocate workarea for summary stats. */
    if (stmt->analyze)
    {
        es->showstatctx = cdbexplain_showExecStatsBegin(queryDesc,
                                                        starttime);

        /* Attach workarea to QueryDesc so ExecSetParamPlan() can find it. */
        queryDesc->showstatctx = es->showstatctx;
    }

	/* Select execution options */
	if (stmt->analyze)
		eflags = 0;				/* default run-to-completion flags */
	else
		eflags = EXEC_FLAG_EXPLAIN_ONLY;

	queryDesc->plannedstmt->query_mem = ResourceManagerGetQueryMemoryLimit(queryDesc->plannedstmt);

#ifdef USE_CODEGEN
	if (stmt->codegen && codegen && Gp_segment == -1) {
		eflags |= EXEC_FLAG_EXPLAIN_CODEGEN;
	}
#endif

	/* call ExecutorStart to prepare the plan for execution */
	ExecutorStart(queryDesc, eflags);

#ifdef USE_CODEGEN
	if (stmt->codegen && codegen && Gp_segment == -1) {
		ExplainCodegen(queryDesc->planstate, tstate);
	}
#endif

    estate = queryDesc->estate;

    /* CDB: Find slice table entry for the root slice. */
    es->currentSlice = getCurrentSlice(estate, LocallyExecutingSliceIndex(estate));

	/* Execute the plan for statistics if asked for */
	/* In GPDB, we attempt to proceed with our report even if there is an error.
     */
	if (stmt->analyze)
	{
		/* run the plan */
        PG_TRY();
        {
		    ExecutorRun(queryDesc, ForwardScanDirection, 0L);
        }
        PG_CATCH();
        {
			MemoryContextSwitchTo(explaincxt);
			es->deferredError = explain_defer_error(es);
        }
        PG_END_TRY();

        /* Wait for completion of all qExec processes. */
        PG_TRY();
        {
            if (estate->dispatcherState && estate->dispatcherState->primaryResults)
			{
				CdbCheckDispatchResult(estate->dispatcherState,
									   DISPATCH_WAIT_NONE);
			}
        }
        PG_CATCH();
        {
			MemoryContextSwitchTo(explaincxt);
            es->deferredError = explain_defer_error(es);
        }
        PG_END_TRY();

        /* Suspend timing. */
	    totaltime += elapsed_time(&starttime);

        /* Get local stats if root slice was executed here in the qDisp. */
        if (!es->currentSlice ||
            sliceRunsOnQD(es->currentSlice))
            cdbexplain_localExecStats(queryDesc->planstate, es->showstatctx);

        /* Fill in the plan's Instrumentation with stats from qExecs. */
        if (estate->dispatcherState && estate->dispatcherState->primaryResults)
            cdbexplain_recvExecStats(queryDesc->planstate,
                                     estate->dispatcherState->primaryResults,
                                     LocallyExecutingSliceIndex(estate),
                                     es->showstatctx);
	}

	es->printAnalyze = stmt->analyze;
	es->pstmt = queryDesc->plannedstmt;
	es->rtable = queryDesc->plannedstmt->rtable;

    if (stmt->verbose)
	{
		char	   *s;
		char	   *f;

		if (queryDesc->plannedstmt->planTree && estate->es_sliceTable)
		{
        	Node   *saved_es_sliceTable;

			/* Little two-step to get EXPLAIN VERBOSE to show slice table. */
			saved_es_sliceTable = queryDesc->plannedstmt->planTree->sliceTable;		/* probably NULL */
			queryDesc->plannedstmt->planTree->sliceTable = (Node *) queryDesc->estate->es_sliceTable;
			s = nodeToString(queryDesc->plannedstmt);
			queryDesc->plannedstmt->planTree->sliceTable = saved_es_sliceTable;
		}
		else
		{
			s = nodeToString(queryDesc->plannedstmt);
		}

		if (s)
		{
			if (Explain_pretty_print)
				f = pretty_format_node_dump(s);
			else
				f = format_node_dump(s);
			pfree(s);
			do_text_output_multiline(tstate, f);
			pfree(f);
			do_text_output_oneline(tstate, ""); /* separator line */
		}
	}

	initStringInfo(&buf);

    /*
     * Produce the EXPLAIN report into buf.  (Sometimes we get internal errors
     * while doing this; try to proceed with a partial report anyway.)
     */
    PG_TRY();
    {
     	int indent = 0;
    	CmdType cmd = queryDesc->plannedstmt->commandType;
    	Plan *childPlan = queryDesc->plannedstmt->planTree;

    	if ( (cmd == CMD_DELETE || cmd == CMD_INSERT || cmd == CMD_UPDATE) &&
    		  queryDesc->plannedstmt->planGen == PLANGEN_PLANNER )
    	{
    	   	/* Set sliceNum to the slice number of the outer-most query plan node */
    	   	int sliceNum = 0;
    	   	int numSegments = getgpsegmentCount();
	    	char *cmdName = NULL;

   			switch (cmd)
			{
				case CMD_DELETE:
					cmdName = "Delete";
					break;
				case CMD_INSERT:
					cmdName = "Insert";
					break;
				case CMD_UPDATE:
					cmdName = "Update";
					break;
				default:
					/* This should never be reached */
					Assert(!"Unexpected statement type");
					break;
			}
			appendStringInfo(&buf, "%s", cmdName);

			if (IsA(childPlan, Motion))
			{
				Motion	   *pMotion = (Motion *) childPlan;
				if (pMotion->motionType == MOTIONTYPE_FIXED && pMotion->numOutputSegs != 0)
				{
					numSegments = 1;
				}
				/* else: other motion nodes execute on all segments */
			}
			else if ((childPlan->directDispatch).isDirectDispatch)
			{
				numSegments = 1;
			}
			appendStringInfo(&buf, " (slice%d; segments: %d)", sliceNum, numSegments);
			appendStringInfo(&buf, "  (rows=%.0f width=%d)\n", ceil(childPlan->plan_rows / numSegments), childPlan->plan_width);
			appendStringInfo(&buf, "  ->  ");
			indent = 3;
		}
	    explain_outNode(&buf, childPlan, queryDesc->planstate,
					    NULL, NULL, indent, es);
    }
    PG_CATCH();
    {
		MemoryContextSwitchTo(explaincxt);
        es->deferredError = explain_defer_error(es);

        /* Keep a NUL at the end of the output buffer. */
        buf.data[Min(buf.len, buf.maxlen-1)] = '\0';
    }
    PG_END_TRY();

	/*
	 * If we ran the command, run any AFTER triggers it queued.  (Note this
	 * will not include DEFERRED triggers; since those don't run until end of
	 * transaction, we can't measure them.)  Include into total runtime.
     * Skip triggers if there has been an error.
	 */
	if (es->printAnalyze &&
        !es->deferredError)
	{
		ResultRelInfo *rInfo;
		bool		show_relname;
		int			numrels = queryDesc->estate->es_num_result_relations;
		List	   *targrels = queryDesc->estate->es_trig_target_relations;
		int			nr;
		ListCell   *l;


		INSTR_TIME_SET_CURRENT(starttime);
		AfterTriggerEndQuery(queryDesc->estate);
		totaltime += elapsed_time(&starttime);

		show_relname = (numrels > 1 || targrels != NIL);
		rInfo = queryDesc->estate->es_result_relations;
		for (nr = 0; nr < numrels; rInfo++, nr++)
			report_triggers(rInfo, show_relname, &buf);

		foreach(l, targrels)
		{
			rInfo = (ResultRelInfo *) lfirst(l);
			report_triggers(rInfo, show_relname, &buf);
		}
	}

    /*
     * Display per-slice and whole-query statistics.
     */
    if (stmt->analyze)
        cdbexplain_showExecStatsEnd(queryDesc->plannedstmt, es->showstatctx, &buf, estate);

    /*
     * Show non-default GUC settings that might have affected the plan.
     */
	settings = gp_guc_list_show(PGC_S_DEFAULT, gp_guc_list_for_explain);
	if (*settings)
		appendStringInfo(&buf, "Settings:  %s\n", settings);
	pfree(settings);

    /* Display optimizer status: either 'legacy query optimizer' or Orca version number */
	appendStringInfo(&buf, "Optimizer status: ");
	if (queryDesc->plannedstmt->planGen == PLANGEN_PLANNER)
	{
		appendStringInfo(&buf, "legacy query optimizer\n");
	}
#ifdef USE_ORCA
	else /* PLANGEN_OPTIMIZER */
	{
		appendStringInfo(&buf, "PQO version %s\n", OptVersion());
	}
#endif

    /*
     * Display final elapsed time.
     */
	if (stmt->analyze)
		appendStringInfo(&buf, "Total runtime: %.3f ms\n",
						 1000.0 * totaltime);

    /*
     * Send EXPLAIN report to client.  Some might have been sent already
     * by explain_outNode().
     */
    if (buf.len > 0)
        do_text_output_multiline(tstate, buf.data);

    /*
	 * Close down the query and free resources.
     *
     * For EXPLAIN ANALYZE, if a qExec failed or gave an error, ExecutorEnd()
     * will reissue the error locally at this point.  Intercept any such error
     * and reduce it to a NOTICE so it won't interfere with our output.
	 */
    PG_TRY();
    {
	    ExecutorEnd(queryDesc);
    }
    PG_CATCH();
    {
		MemoryContextSwitchTo(explaincxt);
        es->deferredError = explain_defer_error(es);
    }
    PG_END_TRY();

    /*
     * If we intercepted an error, now's the time to re-throw it.
     * Although we have marked it as a NOTICE instead of an ERROR,
     * it will still get the same error handling and cleanup treatment.
     *
     * We must call EndCommand() to send a successful completion response;
     * otherwise libpq clients just discard the nice report they have received.
     * Oddly, the NOTICE will be sent *after* the success response; that
     * should be good enough for now.
     */
    if (es->deferredError)
    {
        ErrorData  *edata = es->deferredError;

        /* Tell client the command ended successfully. */
        EndCommand("EXPLAIN", tstate->dest->mydest);

        /* Resume handling the error.  Clean up and send the NOTICE message. */
        es->deferredError = NULL;
        ReThrowError(edata);
    }

    FreeQueryDesc(queryDesc);

	/* We need a CCI just in case query expanded to multiple plans */
	if (stmt->analyze)
		CommandCounterIncrement();
}                               /* ExplainOnePlan_internal */

/*
 * report_triggers -
 *		report execution stats for a single relation's triggers
 */
static void
report_triggers(ResultRelInfo *rInfo, bool show_relname, StringInfo buf)
{
	int			nt;

	if (!rInfo->ri_TrigDesc || !rInfo->ri_TrigInstrument)
		return;
	for (nt = 0; nt < rInfo->ri_TrigDesc->numtriggers; nt++)
	{
		Trigger    *trig = rInfo->ri_TrigDesc->triggers + nt;
		Instrumentation *instr = rInfo->ri_TrigInstrument + nt;
		char	   *conname;

		/* Must clean up instrumentation state */
		InstrEndLoop(instr);

		/*
		 * We ignore triggers that were never invoked; they likely aren't
		 * relevant to the current query type.
		 */
		if (instr->ntuples == 0)
			continue;

		if (OidIsValid(trig->tgconstraint) &&
			(conname = get_constraint_name(trig->tgconstraint)) != NULL)
		{
			appendStringInfo(buf, "Trigger for constraint %s", conname);
			pfree(conname);
		}
		else
			appendStringInfo(buf, "Trigger %s", trig->tgname);

		if (show_relname)
			appendStringInfo(buf, " on %s",
							 RelationGetRelationName(rInfo->ri_RelationDesc));

		appendStringInfo(buf, ": time=%.3f calls=%.0f\n",
						 1000.0 * instr->total, instr->ntuples);
	}
}

/* Compute elapsed time in seconds since given timestamp */
static double
elapsed_time(instr_time *starttime)
{
	instr_time	endtime;

	INSTR_TIME_SET_CURRENT(endtime);
	INSTR_TIME_SUBTRACT(endtime, *starttime);
	return INSTR_TIME_GET_DOUBLE(endtime);
}


/*
 * explain_defer_error
 *    Called within PG_CATCH handler to demote and save the current error.
 *
 * We'll try to postpone the error cleanup until after we have produced
 * the EXPLAIN ANALYZE report, and then reflect the error to the client as
 * merely a NOTICE (because an ERROR causes libpq clients to discard the
 * report).
 *
 * If successful, upon return we fall thru the bottom of the PG_CATCH
 * handler and continue sequentially.  Otherwise we re-throw to the
 * next outer error handler.
 */
static ErrorData *
explain_defer_error(ExplainState *es)
{
    ErrorData  *edata;

    /* Already saved an earlier error?  Rethrow it now. */
    if (es->deferredError)
        ReThrowError(es->deferredError);    /* does not return */

    /* Try to downgrade the error to a NOTICE.  Rethrow if disallowed. */
    if (!elog_demote(NOTICE))
        PG_RE_THROW();

    /* Save the error info and expunge it from the error system. */
    edata = CopyErrorData();
    FlushErrorState();

    /* Caller must eventually ReThrowError() for proper cleanup. */
    return edata;
}                               /* explain_defer_error */

static void
appendGangAndDirectDispatchInfo(StringInfo str, PlanState *planstate, int sliceId)
{
	SliceTable *sliceTable = planstate->state->es_sliceTable;
	Slice *slice = (Slice *)list_nth(sliceTable->slices, sliceId);

	switch (slice->gangType)
	{
		case GANGTYPE_UNALLOCATED:
		case GANGTYPE_ENTRYDB_READER:
			appendStringInfo(str, "  (slice%d)", sliceId);
			break;

		case GANGTYPE_PRIMARY_WRITER:
		case GANGTYPE_PRIMARY_READER:
		case GANGTYPE_SINGLETON_READER:
		{
			int numSegments;
			appendStringInfo(str, "  (slice%d;", sliceId);

			if (slice->directDispatch.isDirectDispatch)
			{
				Assert( list_length(slice->directDispatch.contentIds) == 1);
				numSegments = list_length(slice->directDispatch.contentIds);
			}
			else
			{
				numSegments = slice->numGangMembersToBeActive;
			}
			appendStringInfo(str, " segments: %d)", numSegments);
			break;
		}
	}
}

/*
 * explain_outNode -
 *	  converts a Plan node into ascii string and appends it to 'str'
 *
 * planstate points to the executor state node corresponding to the plan node.
 * We need this to get at the instrumentation data (if any) as well as the
 * list of subplans.
 *
 * outer_plan, if not null, references another plan node that is the outer
 * side of a join with the current node.  This is only interesting for
 * deciphering runtime keys of an inner indexscan.
 *
 * parentPlan points to the parent plan node and can be used by PartitionSelector
 * to deparse its printablePredicate.
 */
static void
explain_outNode(StringInfo str,
				Plan *plan, PlanState *planstate,
				Plan *outer_plan, Plan *parentPlan,
				int indent, ExplainState *es)
{
	const char	   *pname = NULL;
    Slice      *currentSlice = es->currentSlice;    /* save */
	int			i;
	bool		skip_outer=false;
	char       *skip_outer_msg = NULL;
	float		scaleFactor = 1.0; /* we will divide planner estimates by this factor to produce
									  per-segment estimates */

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		/**
		 * Estimates will have to be scaled down to be per-segment (except in a few cases).
		 */
		if ((plan->directDispatch).isDirectDispatch)
		{
			scaleFactor = 1.0;
		}
		else if (plan->flow != NULL && CdbPathLocus_IsBottleneck(*(plan->flow)))
		{
			/**
			 * Data is unified in one place (singleQE or QD), or executed on a single segment.
			 * We scale up estimates to make it global.
			 * We will later amend this for Motion nodes.
			 */
			scaleFactor = 1.0;
		}
		else
		{
			/* the plan node is executed on multiple nodes, so scale down the number of rows seen by each segment */
			scaleFactor = getgpsegmentCount();
		}
	}

	if (plan == NULL)
	{
		appendStringInfoChar(str, '\n');
		return;
	}

	switch (nodeTag(plan))
	{
		case T_Result:
			pname = "Result";
			break;
		case T_Repeat:
			pname = "Repeat";
			break;
		case T_Append:
			pname = "Append";
			break;
		case T_RecursiveUnion:
			pname = "Recursive Union";
			break;
		case T_Sequence:
			pname = "Sequence";
			break;
		case T_BitmapAnd:
			pname = "BitmapAnd";
			break;
		case T_BitmapOr:
			pname = "BitmapOr";
			break;
		case T_NestLoop:
			if (((NestLoop *)plan)->shared_outer)
			{
				skip_outer = true;
				skip_outer_msg = "See first subplan of Hash Join";
			}

			switch (((NestLoop *) plan)->join.jointype)
			{
				case JOIN_INNER:
					pname = "Nested Loop";
					break;
				case JOIN_LEFT:
					pname = "Nested Loop Left Join";
					break;
				case JOIN_FULL:
					pname = "Nested Loop Full Join";
					break;
				case JOIN_RIGHT:
					pname = "Nested Loop Right Join";
					break;
				case JOIN_SEMI:
					pname = "Nested Loop Semi Join";
					break;
				case JOIN_ANTI:
					pname = "Nested Loop Anti Join";
					break;
				case JOIN_LASJ_NOTIN:
					pname = "Nested Loop Left Anti Semi Join (Not-In)";
					break;
				default:
					pname = "Nested Loop ??? Join";
					break;
			}
			break;
		case T_MergeJoin:
			switch (((MergeJoin *) plan)->join.jointype)
			{
				case JOIN_INNER:
					pname = "Merge Join";
					break;
				case JOIN_LEFT:
					pname = "Merge Left Join";
					break;
				case JOIN_FULL:
					pname = "Merge Full Join";
					break;
				case JOIN_RIGHT:
					pname = "Merge Right Join";
					break;
				case JOIN_SEMI:
					pname = "Merge Semi Join";
					break;
				case JOIN_ANTI:
					pname = "Merge Anti Join";
					break;
				case JOIN_LASJ_NOTIN:
					pname = "Merge Left Anti Semi Join (Not-In)";
					break;
				default:
					pname = "Merge ??? Join";
					break;
			}
			break;
		case T_HashJoin:
			switch (((HashJoin *) plan)->join.jointype)
			{
				case JOIN_INNER:
					pname = "Hash Join";
					break;
				case JOIN_LEFT:
					pname = "Hash Left Join";
					break;
				case JOIN_FULL:
					pname = "Hash Full Join";
					break;
				case JOIN_RIGHT:
					pname = "Hash Right Join";
					break;
				case JOIN_SEMI:
					pname = "Hash Semi Join";
					break;
				case JOIN_ANTI:
					pname = "Hash Anti Join";
					break;
				case JOIN_LASJ_NOTIN:
					pname = "Hash Left Anti Semi Join (Not-In)";
					break;
				default:
					pname = "Hash ??? Join";
					break;
			}
			break;
		case T_SeqScan:
			pname = "Seq Scan";
			break;
		case T_AppendOnlyScan:
			pname = "Append-only Scan";
			break;
		case T_AOCSScan:
			pname = "Append-only Columnar Scan";
			break;
		case T_TableScan:
			pname = "Table Scan";
			break;
		case T_DynamicTableScan:
			pname = "Dynamic Table Scan";
			break;
		case T_ExternalScan:
			pname = "External Scan";
			break;
		case T_IndexScan:
			pname = "Index Scan";
			break;
		case T_DynamicIndexScan:
			pname = "Dynamic Index Scan";
			break;
		case T_BitmapIndexScan:
			pname = "Bitmap Index Scan";
			break;
		case T_BitmapHeapScan:
			pname = "Bitmap Heap Scan";
			break;
		case T_BitmapAppendOnlyScan:
			if (((BitmapAppendOnlyScan *)plan)->isAORow)
				pname = "Bitmap Append-Only Row-Oriented Scan";
			else
				pname = "Bitmap Append-Only Column-Oriented Scan";
			break;
		case T_BitmapTableScan:
			pname = "Bitmap Table Scan";
			break;
		case T_TidScan:
			pname = "Tid Scan";
			break;
		case T_SubqueryScan:
			pname = "Subquery Scan";
			break;
		case T_FunctionScan:
			pname = "Function Scan";
			break;
		case T_ValuesScan:
			pname = "Values Scan";
			break;
		case T_CteScan:
			pname = "CTE Scan";
			break;
		case T_WorkTableScan:
			pname = "WorkTable Scan";
			break;
		case T_ShareInputScan:
			{
				ShareInputScan *sisc = (ShareInputScan *) plan;
				appendStringInfo(str, "Shared Scan (share slice:id %d:%d)",
						currentSlice ? currentSlice->sliceIndex : -1, sisc->share_id);
				pname = "";
			}
			break;
		case T_Material:
			pname = "Materialize";
			break;
		case T_Sort:
			pname = "Sort";
			break;
		case T_Agg:
			switch (((Agg *) plan)->aggstrategy)
			{
				case AGG_PLAIN:
					pname = "Aggregate";
					break;
				case AGG_SORTED:
					pname = "GroupAggregate";
					break;
				case AGG_HASHED:
					pname = "HashAggregate";
					break;
				default:
					pname = "Aggregate ???";
					break;
			}
			break;
		case T_WindowAgg:
			pname = "WindowAgg";
			break;
		case T_TableFunctionScan:
			pname = "Table Function Scan";
			break;
		case T_Unique:
			pname = "Unique";
			break;
		case T_SetOp:
			switch (((SetOp *) plan)->cmd)
			{
				case SETOPCMD_INTERSECT:
					pname = "SetOp Intersect";
					break;
				case SETOPCMD_INTERSECT_ALL:
					pname = "SetOp Intersect All";
					break;
				case SETOPCMD_EXCEPT:
					pname = "SetOp Except";
					break;
				case SETOPCMD_EXCEPT_ALL:
					pname = "SetOp Except All";
					break;
				default:
					pname = "SetOp ???";
					break;
			}
			break;
		case T_Limit:
			pname = "Limit";
			break;
		case T_Hash:
			pname = "Hash";
			break;
		case T_Motion:
			{
				Motion	   *pMotion = (Motion *) plan;
				SliceTable *sliceTable = planstate->state->es_sliceTable;
				Slice *slice = (Slice *)list_nth(sliceTable->slices, pMotion->motionID);

                int         nSenders = slice->numGangMembersToBeActive;
				int         nReceivers = 0;

				/* scale the number of rows by the number of segments sending data */
				scaleFactor = nSenders;

				switch (pMotion->motionType)
				{
					case MOTIONTYPE_HASH:
						nReceivers = pMotion->numOutputSegs;
						pname = "Redistribute Motion";
						break;
					case MOTIONTYPE_FIXED:
						nReceivers = pMotion->numOutputSegs;
						if (nReceivers == 0)
						{
							pname = "Broadcast Motion";
							nReceivers = getgpsegmentCount();
						}
						else
						{
							scaleFactor = 1;
							pname = "Gather Motion";
						}
						break;
					case MOTIONTYPE_EXPLICIT:
						nReceivers = getgpsegmentCount();
						pname = "Explicit Redistribute Motion";
						break;
					default:
						pname = "Motion ???";
						break;
				}

				appendStringInfo(str, "%s %d:%d", pname,
						nSenders, nReceivers);

				appendGangAndDirectDispatchInfo(str, planstate, pMotion->motionID);
				pname = "";

			}
			break;
		case T_DML:
			{
				switch (es->pstmt->commandType)
				{
					case CMD_INSERT:
						pname = "Insert";
						break;
					case CMD_DELETE:
						pname = "Delete";
						break;
					case CMD_UPDATE:
						pname = "Update";
						break;
					default:
						pname = "DML ???";
						break;
				}
			}
			break;
		case T_SplitUpdate:
			pname = "Split";
			break;
		case T_AssertOp:
			pname = "Assert";
			break;
		case T_PartitionSelector:
			pname = "Partition Selector";
			break;
		case T_RowTrigger:
 			pname = "RowTrigger";
 			break;
		default:
			pname = "???";
			break;
	}

	appendStringInfoString(str, pname);
	switch (nodeTag(plan))
	{
		case T_IndexScan:
			if (ScanDirectionIsBackward(((IndexScan *) plan)->indexorderdir))
				appendStringInfoString(str, " Backward");
			appendStringInfo(str, " using %s",
					  explain_get_index_name(((IndexScan *) plan)->indexid));
			/* FALL THRU */
		case T_SeqScan:
		case T_ExternalScan:
		case T_AppendOnlyScan:
		case T_AOCSScan:
		case T_TableScan:
		case T_DynamicTableScan:
		case T_DynamicIndexScan:
		case T_BitmapHeapScan:
		case T_BitmapAppendOnlyScan:
		case T_BitmapTableScan:
		case T_TidScan:
			if (((Scan *) plan)->scanrelid > 0)
			{
				RangeTblEntry *rte = rt_fetch(((Scan *) plan)->scanrelid,
											  es->rtable);
				char	   *relname;

				/* Assume it's on a real relation */
				Assert(rte->rtekind == RTE_RELATION);

				/* We only show the rel name, not schema name */
				relname = get_rel_name(rte->relid);

				appendStringInfo(str, " on %s",
								 quote_identifier(relname));
				if (strcmp(rte->eref->aliasname, relname) != 0)
					appendStringInfo(str, " %s",
									 quote_identifier(rte->eref->aliasname));

				/* Print dynamic scan id for dytnamic scan operators */
				if (isDynamicScan((Scan *)plan))
				{
					appendStringInfo(str, " (dynamic scan id: %d)",
									 ((Scan *)plan)->partIndexPrintable);
				}
			}
			break;
		case T_BitmapIndexScan:
			appendStringInfo(str, " on %s",
				explain_get_index_name(((BitmapIndexScan *) plan)->indexid));
			break;
		case T_SubqueryScan:
			if (((Scan *) plan)->scanrelid > 0)
			{
				RangeTblEntry *rte = rt_fetch(((Scan *) plan)->scanrelid,
											  es->rtable);

				appendStringInfo(str, " %s",
								 quote_identifier(rte->eref->aliasname));
			}
			break;
		case T_TableFunctionScan:
			{
				RangeTblEntry	*rte;
				FuncExpr		*funcexpr;
				char			*proname;

				/* Get the range table, it should be a TableFunction */
				rte = rt_fetch(((Scan *) plan)->scanrelid, es->rtable);
				Assert(rte->rtekind == RTE_TABLEFUNCTION);

				/*
				 * Lookup the function name.
				 *
				 * Unlike RTE_FUNCTION there should be no cases where the
				 * optimizer could have evaluated away the function call.
				 */
				Insist(rte->funcexpr && IsA(rte->funcexpr, FuncExpr));
				funcexpr = (FuncExpr *) rte->funcexpr;
				proname	 = get_func_name(funcexpr->funcid);

				/* Build the output description */
				appendStringInfo(str, " on %s", quote_identifier(proname));
				if (strcmp(rte->eref->aliasname, proname) != 0)
					appendStringInfo(str, " %s",
									 quote_identifier(rte->eref->aliasname));

				/* might be nice to add order by and scatter by info */

			}
			break;
		case T_FunctionScan:
			if (((Scan *) plan)->scanrelid > 0)
			{
				RangeTblEntry *rte = rt_fetch(((Scan *) plan)->scanrelid,
											  es->rtable);
				Node	   *funcexpr;
				char	   *proname;

				/* Assert it's on a RangeFunction */
				Assert(rte->rtekind == RTE_FUNCTION);

				/*
				 * If the expression is still a function call, we can get the
				 * real name of the function.  Otherwise, punt (this can
				 * happen if the optimizer simplified away the function call,
				 * for example).
				 */
				funcexpr = ((FunctionScan *) plan)->funcexpr;
				if (funcexpr && IsA(funcexpr, FuncExpr))
				{
					Oid			funcid = ((FuncExpr *) funcexpr)->funcid;

					/* We only show the func name, not schema name */
					proname = get_func_name(funcid);
				}
				else
					proname = rte->eref->aliasname;

				appendStringInfo(str, " on %s",
								 quote_identifier(proname));
				if (strcmp(rte->eref->aliasname, proname) != 0)
					appendStringInfo(str, " %s",
									 quote_identifier(rte->eref->aliasname));
			}
			break;
		case T_ValuesScan:
			if (((Scan *) plan)->scanrelid > 0)
			{
				RangeTblEntry *rte = rt_fetch(((Scan *) plan)->scanrelid,
											  es->rtable);
				char	   *valsname;

				/* Assert it's on a values rte */
				Assert(rte->rtekind == RTE_VALUES);

				valsname = rte->eref->aliasname;

				appendStringInfo(str, " on %s",
								 quote_identifier(valsname));
			}
			break;
		case T_CteScan:
			if (((Scan *) plan)->scanrelid > 0)
			{
				RangeTblEntry *rte = rt_fetch(((Scan *) plan)->scanrelid,
											  es->rtable);

				/* Assert it's on a non-self-reference CTE */
				Assert(rte->rtekind == RTE_CTE);
				Assert(!rte->self_reference);

				appendStringInfo(str, " on %s",
								 quote_identifier(rte->ctename));
				if (strcmp(rte->eref->aliasname, rte->ctename) != 0)
					appendStringInfo(str, " %s",
									 quote_identifier(rte->eref->aliasname));
			}
			break;
		case T_WorkTableScan:
			if (((Scan *) plan)->scanrelid > 0)
			{
				RangeTblEntry *rte = rt_fetch(((Scan *) plan)->scanrelid,
											  es->rtable);

				/* Assert it's on a self-reference CTE */
				Assert(rte->rtekind == RTE_CTE);
				Assert(rte->self_reference);

				appendStringInfo(str, " on %s",
								 quote_identifier(rte->ctename));
				if (strcmp(rte->eref->aliasname, rte->ctename) != 0)
					appendStringInfo(str, " %s",
									 quote_identifier(rte->eref->aliasname));
			}
			break;
		case T_PartitionSelector:
			{
				PartitionSelector *ps = (PartitionSelector *)plan;
				char *relname = get_rel_name(ps->relid);
				appendStringInfo(str, " for %s", quote_identifier(relname));
				if (0 != ps->scanId)
				{
					appendStringInfo(str, " (dynamic scan id: %d)", ps->scanId);
				}
			}
			break;
		default:
			break;
	}

	Assert(scaleFactor > 0.0);

	appendStringInfo(str, "  (cost=%.2f..%.2f rows=%.0f width=%d)",
					 plan->startup_cost, plan->total_cost,
					 ceil(plan->plan_rows / scaleFactor), plan->plan_width);

	if (ResManagerPrintOperatorMemoryLimits())
	{
		appendStringInfo(str, " (operatorMem=" UINT64_FORMAT "KB)",
						 PlanStateOperatorMemKB(planstate));
	}

	appendStringInfoChar(str, '\n');

#ifdef DEBUG_EXPLAIN
	appendStringInfo(str, "plan->targetlist=%s\n", nodeToString(plan->targetlist));
#endif

	/* quals, sort keys, etc */
	switch (nodeTag(plan))
	{
		case T_IndexScan:
		case T_DynamicIndexScan:
			show_scan_qual(((IndexScan *) plan)->indexqualorig,
						   "Index Cond",
						   ((Scan *) plan)->scanrelid,
						   plan, outer_plan,
						   str, indent, es);
			show_scan_qual(plan->qual,
						   "Filter",
						   ((Scan *) plan)->scanrelid,
						   plan, outer_plan,
						   str, indent, es);
			break;
		case T_BitmapIndexScan:
			show_scan_qual(((BitmapIndexScan *) plan)->indexqualorig,
						   "Index Cond",
						   ((Scan *) plan)->scanrelid,
						   plan, outer_plan,
						   str, indent, es);
			break;
		case T_BitmapHeapScan:
		case T_BitmapAppendOnlyScan:
		case T_BitmapTableScan:
			/* XXX do we want to show this in production? */
			if (nodeTag(plan) == T_BitmapHeapScan)
			{
				show_scan_qual(((BitmapHeapScan *) plan)->bitmapqualorig,
							   "Recheck Cond",
							   ((Scan *) plan)->scanrelid,
							   plan, outer_plan,
							   str, indent, es);
			}
			else if (nodeTag(plan) == T_BitmapAppendOnlyScan)
			{
				show_scan_qual(((BitmapAppendOnlyScan *) plan)->bitmapqualorig,
							   "Recheck Cond",
							   ((Scan *) plan)->scanrelid,
							   plan, outer_plan,
							   str, indent, es);
			}
			else if (nodeTag(plan) == T_BitmapTableScan)
			{
				show_scan_qual(((BitmapTableScan *) plan)->bitmapqualorig,
							   "Recheck Cond",
							   ((Scan *) plan)->scanrelid,
							   plan, outer_plan,
							   str, indent, es);
			}
			/* FALL THRU */
		case T_SeqScan:
		case T_ExternalScan:
		case T_AppendOnlyScan:
		case T_AOCSScan:
		case T_TableScan:
		case T_DynamicTableScan:
		case T_FunctionScan:
		case T_ValuesScan:
		case T_CteScan:
		case T_WorkTableScan:
			show_scan_qual(plan->qual,
						   "Filter",
						   ((Scan *) plan)->scanrelid,
						   plan, outer_plan,
						   str, indent, es);
			break;
		case T_SubqueryScan:
			show_scan_qual(plan->qual,
						   "Filter",
						   ((Scan *) plan)->scanrelid,
						   plan, outer_plan,
						   str, indent, es);
			break;
		case T_TidScan:
			{
				/*
				 * The tidquals list has OR semantics, so be sure to show it
				 * as an OR condition.
				 */
				List	   *tidquals = ((TidScan *) plan)->tidquals;

				if (list_length(tidquals) > 1)
					tidquals = list_make1(make_orclause(tidquals));
				show_scan_qual(tidquals,
							   "TID Cond",
							   ((Scan *) plan)->scanrelid,
							   plan, outer_plan,
							   str, indent, es);
				show_scan_qual(plan->qual,
							   "Filter",
							   ((Scan *) plan)->scanrelid,
							   plan, outer_plan,
							   str, indent, es);
			}
			break;
		case T_NestLoop:
			show_upper_qual(((NestLoop *) plan)->join.joinqual,
							"Join Filter", plan,
							str, indent, es);
			show_upper_qual(plan->qual,
							"Filter", plan,
							str, indent, es);
			break;
		case T_MergeJoin:
			show_upper_qual(((MergeJoin *) plan)->mergeclauses,
							"Merge Cond", plan,
							str, indent, es);
			show_upper_qual(((MergeJoin *) plan)->join.joinqual,
							"Join Filter", plan,
							str, indent, es);
			show_upper_qual(plan->qual,
							"Filter", plan,
							str, indent, es);
			break;
		case T_HashJoin: {
			HashJoin *hash_join = (HashJoin *) plan;
			/*
			 * In the case of an "IS NOT DISTINCT" condition, we display
			 * hashqualclauses instead of hashclauses.
			 */
			List *cond_to_show = hash_join->hashclauses;
			if (list_length(hash_join->hashqualclauses) > 0) {
				cond_to_show = hash_join->hashqualclauses;
			}
			show_upper_qual(cond_to_show,
							"Hash Cond", plan,
							str, indent, es);
			show_upper_qual(((HashJoin *) plan)->join.joinqual,
							"Join Filter", plan,
							str, indent, es);
			show_upper_qual(plan->qual,
							"Filter", plan,
							str, indent, es);
			break;
		}
		case T_Agg:
			show_upper_qual(plan->qual,
							"Filter", plan,
							str, indent, es);
			show_grouping_keys(plan,
						       ((Agg *) plan)->numCols,
						       ((Agg *) plan)->grpColIdx,
						       "Group By",
						       str, indent, es);
			break;
		case T_WindowAgg:
			{
				WindowAgg *window = (WindowAgg *) plan;

				if ( window->partNumCols > 0 )
				{
					show_grouping_keys(plan,
									   window->partNumCols,
									   window->partColIdx,
									   "Partition By",
									   str, indent, es);
				}

				show_sort_keys(outerPlan(plan),
							   window->ordNumCols,
							   window->ordColIdx,
							   "Order By",
							   str, indent, es);
				/* XXX don't show framing for now */
			}
			break;
		case T_TableFunctionScan:
		{
			show_scan_qual(plan->qual,
						   "Filter",
						   ((Scan *) plan)->scanrelid,
						   plan, outer_plan,
						   str, indent, es);

			/* Partitioning and ordering information */

		}
		break;

		case T_Unique:
			show_motion_keys(plan,
                             NIL,
						     ((Unique *) plan)->numCols,
						     ((Unique *) plan)->uniqColIdx,
						     "Group By",
						     str, indent, es);
			break;
		case T_Sort:
		{
			bool bNoDup = ((Sort *) plan)->noduplicates;

			char *SortKeystr = "Sort Key";

			if (bNoDup)
				SortKeystr = "Sort Key (Distinct)";

			show_sort_keys(plan,
						   ((Sort *) plan)->numCols,
						   ((Sort *) plan)->sortColIdx,
						   SortKeystr,
						   str, indent, es);
		}
			break;
		case T_Result:
			show_upper_qual((List *) ((Result *) plan)->resconstantqual,
							"One-Time Filter", plan,
							str, indent, es);
			show_upper_qual(plan->qual,
							"Filter", plan,
							str, indent, es);
			break;
		case T_Repeat:
			show_upper_qual(plan->qual,
							"Filter", plan,
							str, indent, es);
			break;
		case T_Motion:
			{
				Motion	   *pMotion = (Motion *) plan;
                SliceTable *sliceTable = planstate->state->es_sliceTable;

				if (pMotion->sendSorted || pMotion->motionType == MOTIONTYPE_HASH)
					show_motion_keys(plan,
							pMotion->hashExpr,
							pMotion->numSortCols,
							pMotion->sortColIdx,
							"Merge Key",
							str, indent, es);

                /* Descending into a new slice. */
                if (sliceTable)
                    es->currentSlice = (Slice *)list_nth(sliceTable->slices,
                                                         pMotion->motionID);
			}
			break;
		case T_AssertOp:
			{
				show_upper_qual(plan->qual,
								"Assert Cond", plan,
								str, indent, es);
			}
			break;
		case T_PartitionSelector:
			{
				explain_partition_selector((PartitionSelector *) plan, parentPlan,
						str, indent, es);
			}
			break;
		default:
			break;
	}

    /* CDB: Show actual row count, etc. */
	if (planstate->instrument)
	{
        cdbexplain_showExecStats(planstate,
                                 str,
                                 indent+1,
                                 es->showstatctx);
	}
	/* initPlan-s */
	if (plan->initPlan)
	{
        Slice      *saved_slice = es->currentSlice;
		ListCell   *lst;

		foreach(lst, planstate->initPlan)
		{
			SubPlanState *sps = (SubPlanState *) lfirst(lst);
			SubPlan    *sp = (SubPlan *) sps->xprstate.expr;
            SliceTable *sliceTable = planstate->state->es_sliceTable;

			appendStringInfoFill(str, 2*indent, ' ');
			appendStringInfo(str, "  %s", sp->plan_name);

            /* Subplan might have its own root slice */
            if (sliceTable &&
                sp->qDispSliceId > 0)
            {
                es->currentSlice = (Slice *)list_nth(sliceTable->slices,
                                                     sp->qDispSliceId);
    		    appendGangAndDirectDispatchInfo(str, planstate, sp->qDispSliceId );
            }
            else
            {
                /*
                 * CDB TODO: In non-parallel query, all qDispSliceId's are 0.
                 * Should fill them in properly before ExecutorStart(), but
                 * for now, just omit the slice id.
                 */
            }

            appendStringInfoChar(str, '\n');
			for (i = 0; i < indent; i++)
				appendStringInfo(str, "  ");
			appendStringInfo(str, "    ->  ");
			explain_outNode(str,
							exec_subplan_get_plan(es->pstmt, sp),
							sps->planstate,
							NULL, plan,
							indent + 4, es);
		}
        es->currentSlice = saved_slice;
	}

	/* lefttree */
	if (outerPlan(plan) && !skip_outer)
	{
		for (i = 0; i < indent; i++)
			appendStringInfo(str, "  ");
		appendStringInfo(str, "  ->  ");

		/*
		 * Ordinarily we don't pass down our own outer_plan value to our child
		 * nodes, but in bitmap scan trees we must, since the bottom
		 * BitmapIndexScan nodes may have outer references.
		 */
		explain_outNode(str, outerPlan(plan),
						outerPlanState(planstate),
						(IsA(plan, BitmapHeapScan) |
						 IsA(plan, BitmapAppendOnlyScan) |
						 IsA(plan, BitmapTableScan)) ? outer_plan : NULL,
						plan,
						indent + 3, es);
	}
    else if (skip_outer)
    {
		for (i = 0; i < indent; i++)
			appendStringInfo(str, "  ");
		appendStringInfo(str, "  ->  ");
		appendStringInfoString(str, skip_outer_msg);
		appendStringInfo(str, "\n");
    }

	/* righttree */
	if (innerPlan(plan))
	{
		for (i = 0; i < indent; i++)
			appendStringInfo(str, "  ");
		appendStringInfo(str, "  ->  ");
		explain_outNode(str, innerPlan(plan),
						innerPlanState(planstate),
						outerPlan(plan),
						plan,
						indent + 3, es);
	}

	if (IsA(plan, Append))
	{
		Append	   *appendplan = (Append *) plan;
		AppendState *appendstate = (AppendState *) planstate;
		ListCell   *lst;
		int			j;

		j = 0;
		foreach(lst, appendplan->appendplans)
		{
			Plan	   *subnode = (Plan *) lfirst(lst);

			for (i = 0; i < indent; i++)
				appendStringInfo(str, "  ");
			appendStringInfo(str, "  ->  ");

			/*
			 * Ordinarily we don't pass down our own outer_plan value to our
			 * child nodes, but in an Append we must, since we might be
			 * looking at an appendrel indexscan with outer references from
			 * the member scans.
			 */
			explain_outNode(str, subnode,
							appendstate->appendplans[j],
							outer_plan,
							(Plan *) appendplan,
							indent + 3, es);
			j++;
		}
	}

	if (IsA(plan, Sequence))
	{
		Sequence *sequence = (Sequence *) plan;
		SequenceState *sequenceState = (SequenceState *) planstate;
		ListCell *lc;
		int j = 0;
		foreach(lc, sequence->subplans)
		{
			Plan *subnode = (Plan *) lfirst(lc);

			for (i = 0; i < indent; i++)
				appendStringInfo(str, "  ");

			appendStringInfo(str, "  ->  ");

			explain_outNode(str, subnode,
							sequenceState->subplans[j],
							outer_plan,
							plan,
							indent + 3, es);
			j++;
		}
	}

	if (IsA(plan, BitmapAnd))
	{
		BitmapAnd  *bitmapandplan = (BitmapAnd *) plan;
		BitmapAndState *bitmapandstate = (BitmapAndState *) planstate;
		ListCell   *lst;
		int			j;

		j = 0;
		foreach(lst, bitmapandplan->bitmapplans)
		{
			Plan	   *subnode = (Plan *) lfirst(lst);

			for (i = 0; i < indent; i++)
				appendStringInfo(str, "  ");
			appendStringInfo(str, "  ->  ");

			explain_outNode(str, subnode,
							bitmapandstate->bitmapplans[j],
							outer_plan, /* pass down same outer plan */
							plan,
							indent + 3, es);
			j++;
		}
	}

	if (IsA(plan, BitmapOr))
	{
		BitmapOr   *bitmaporplan = (BitmapOr *) plan;
		BitmapOrState *bitmaporstate = (BitmapOrState *) planstate;
		ListCell   *lst;
		int			j;

		j = 0;
		foreach(lst, bitmaporplan->bitmapplans)
		{
			Plan	   *subnode = (Plan *) lfirst(lst);

			for (i = 0; i < indent; i++)
				appendStringInfo(str, "  ");
			appendStringInfo(str, "  ->  ");

			explain_outNode(str, subnode,
							bitmaporstate->bitmapplans[j],
							outer_plan, /* pass down same outer plan */
							plan,
							indent + 3, es);
			j++;
		}
	}

	if (IsA(plan, SubqueryScan))
	{
		SubqueryScan *subqueryscan = (SubqueryScan *) plan;
		SubqueryScanState *subquerystate = (SubqueryScanState *) planstate;
		Plan	   *subnode = subqueryscan->subplan;

		for (i = 0; i < indent; i++)
			appendStringInfo(str, "  ");
		appendStringInfo(str, "  ->  ");

		explain_outNode(str, subnode,
						subquerystate->subplan,
						NULL,
						plan,
						indent + 3, es);
	}

	/* subPlan-s */
	if (planstate->subPlan)
	{
		ListCell   *lst;

		foreach(lst, planstate->subPlan)
		{
			SubPlanState *sps = (SubPlanState *) lfirst(lst);
			SubPlan    *sp = (SubPlan *) sps->xprstate.expr;

			for (i = 0; i < indent; i++)
				appendStringInfo(str, "  ");
			appendStringInfo(str, "  %s\n", sp->plan_name);
			for (i = 0; i < indent; i++)
				appendStringInfo(str, "  ");
			appendStringInfo(str, "    ->  ");
			explain_outNode(str,
							exec_subplan_get_plan(es->pstmt, sp),
							sps->planstate,
							NULL,
							plan,
							indent + 4, es);
		}
	}
	es->currentSlice = currentSlice;    /* restore */
}

/*
 * Show a qualifier expression for a scan plan node
 *
 * Note: outer_plan is the referent for any OUTER vars in the scan qual;
 * this would be the outer side of a nestloop plan.  Pass NULL if none.
 */
static void
show_scan_qual(List *qual, const char *qlabel,
			   int scanrelid, Plan *scan_plan, Plan *outer_plan,
			   StringInfo str, int indent, ExplainState *es)
{
	List	   *context;
	bool		useprefix;
	Node	   *node;
	char	   *exprstr;
	int			i;

	/* No work if empty qual */
	if (qual == NIL)
		return;

	/* Convert AND list to explicit AND */
	node = (Node *) make_ands_explicit(qual);

	/* Set up deparsing context */
	context = deparse_context_for_plan((Node *) scan_plan,
									   (Node *) outer_plan,
									   es->rtable,
									   es->pstmt->subplans);
	useprefix = (outer_plan != NULL || IsA(scan_plan, SubqueryScan));

	/* Deparse the expression */
	exprstr = deparse_expr_sweet(node, context, useprefix, false);

	/* And add to str */
	for (i = 0; i < indent; i++)
		appendStringInfo(str, "  ");
	appendStringInfo(str, "  %s: %s\n", qlabel, exprstr);
}

/*
 * Show a qualifier expression for an upper-level plan node
 */
static void
show_upper_qual(List *qual, const char *qlabel, Plan *plan,
				StringInfo str, int indent, ExplainState *es)
{
	List	   *context;
	bool		useprefix;
	Node	   *node;
	char	   *exprstr;
	int			i;

	/* No work if empty qual */
	if (qual == NIL)
		return;

	/* Set up deparsing context */
	context = deparse_context_for_plan((Node *) plan,
									   NULL,
									   es->rtable,
									   es->pstmt->subplans);
	useprefix = list_length(es->rtable) > 1;

	/* Deparse the expression */
	node = (Node *) make_ands_explicit(qual);
	exprstr = deparse_expr_sweet(node, context, useprefix, false);

	/* And add to str */
	for (i = 0; i < indent; i++)
		appendStringInfo(str, "  ");
	appendStringInfo(str, "  %s: %s\n", qlabel, exprstr);
}

/*
 * CDB: Show GROUP BY keys for an Agg or Group node.
 */
void
show_grouping_keys(Plan        *plan,
                   int          numCols,
                   AttrNumber  *subplanColIdx,
                   const char  *qlabel,
			       StringInfo str, int indent, ExplainState *es)
{
    Plan       *subplan = plan->lefttree;
    List	   *context;
    char	   *exprstr;
    bool		useprefix = list_length(es->rtable) > 1;
    int			keyno;
    int			i;
	int         num_null_cols = 0;
	int         rollup_gs_times = 0;

    if (numCols <= 0)
        return;

    for (i = 0; i < indent; i++)
        appendStringInfoString(str, "  ");
    appendStringInfo(str, "  %s: ", qlabel);

    Node *outerPlan = (Node *) outerPlan(subplan);

	/*
	 * Dig the child nodes of the subplan. This logic should match that in
	 * push_plan function, in ruleutils.c!
	 */
	if (IsA(subplan, Append))
		outerPlan = linitial(((Append *) subplan)->appendplans);
	else if (IsA(subplan, Sequence))
		outerPlan = (Node *) llast(((Sequence *) subplan)->subplans);

	/* Set up deparse context */
	context = deparse_context_for_plan((Node *) subplan,
									   outerPlan,
									   es->rtable,
									   es->pstmt->subplans);

	if (IsA(plan, Agg))
	{
		num_null_cols = ((Agg*)plan)->numNullCols;
		rollup_gs_times = ((Agg*)plan)->rollupGSTimes;
	}

    for (keyno = 0; keyno < numCols - num_null_cols; keyno++)
    {
	    /* find key expression in tlist */
	    AttrNumber      keyresno = subplanColIdx[keyno];
	    TargetEntry    *target = get_tle_by_resno(subplan->targetlist, keyresno);
		char grping_str[50];

	    if (!target)
		    elog(ERROR, "no tlist entry for key %d", keyresno);

		if (IsA(target->expr, Grouping))
		{
			sprintf(grping_str, "grouping");
			/* Append "grouping" explicitly. */
			exprstr = grping_str;
		}

		else if (IsA(target->expr, GroupId))
		{
			sprintf(grping_str, "groupid");
			/* Append "groupid" explicitly. */
			exprstr = grping_str;
		}

		else
			/* Deparse the expression, showing any top-level cast */
			exprstr = deparse_expr_sweet((Node *) target->expr, context,
										 useprefix, true);

		/* And add to str */
		if (keyno > 0)
			appendStringInfoString(str, ", ");
		appendStringInfoString(str, exprstr);
    }

	if (rollup_gs_times > 1)
		appendStringInfo(str, " (%d times)", rollup_gs_times);

    appendStringInfoChar(str, '\n');
}                               /* show_grouping_keys */


/*
 * Show the sort keys for a Sort node.
 */
static void
show_sort_keys(Plan *sortplan, int nkeys, AttrNumber *keycols,
			   const char *qlabel,
			   StringInfo str, int indent, ExplainState *es)
{
	List	   *context;
	bool		useprefix;
	int			keyno;
	char	   *exprstr;
	int			i;

	if (nkeys <= 0)
		return;

	useprefix = list_length(es->rtable) > 1;    /*CDB*/

	for (i = 0; i < indent; i++)
		appendStringInfo(str, "  ");
	appendStringInfo(str, "  %s: ", qlabel);

	/* Set up deparsing context */
	context = deparse_context_for_plan((Node *) sortplan,
									   NULL,
									   es->rtable,
									   es->pstmt->subplans);
	useprefix = list_length(es->rtable) > 1;

	for (keyno = 0; keyno < nkeys; keyno++)
	{
		/* find key expression in tlist */
		AttrNumber	keyresno = keycols[keyno];
		TargetEntry *target = get_tle_by_resno(sortplan->targetlist, keyresno);

		if (!target)
			elog(ERROR, "no tlist entry for key %d", keyresno);
		/* Deparse the expression, showing any top-level cast */
		exprstr = deparse_expr_sweet((Node *) target->expr, context,
									 useprefix, true);
		/* And add to str */
		if (keyno > 0)
			appendStringInfo(str, ", ");
		appendStringInfoString(str, exprstr);
	}

	appendStringInfo(str, "\n");
}


/*
 * CDB: Show the hash and merge keys for a Motion node.
 */
void
show_motion_keys(Plan *plan, List *hashExpr, int nkeys, AttrNumber *keycols,
			     const char *qlabel,
                 StringInfo str, int indent, ExplainState *es)
{
	List	   *context;
	char	   *exprstr;
	bool		useprefix = list_length(es->rtable) > 1;
	int			keyno;
	int			i;

	if (!nkeys && !hashExpr)
		return;

	/* Set up deparse context */
	context = deparse_context_for_plan((Node *) plan, (Node *) outerPlan(plan),
									   es->rtable,
									   es->pstmt->subplans);

    /* Merge Receive ordering key */
    if (nkeys > 0)
    {
        for (i = 0; i < indent; i++)
            appendStringInfoString(str, "  ");
        appendStringInfo(str, "  %s: ", qlabel);

	    for (keyno = 0; keyno < nkeys; keyno++)
	    {
		    /* find key expression in tlist */
		    AttrNumber	keyresno = keycols[keyno];
		    TargetEntry *target = get_tle_by_resno(plan->targetlist, keyresno);

		    /* Deparse the expression, showing any top-level cast */
		    if (target)
		        exprstr = deparse_expr_sweet((Node *) target->expr, context,
									         useprefix, true);
            else
            {
                elog(WARNING, "Gather Motion %s error: no tlist item %d",
                     qlabel, keyresno);
                exprstr = "*BOGUS*";
            }

		    /* And add to str */
		    if (keyno > 0)
			    appendStringInfoString(str, ", ");
		    appendStringInfoString(str, exprstr);
	    }

	    appendStringInfoChar(str, '\n');
    }

    /* Hashed repartitioning key */
    if (hashExpr)
    {
	    /* Deparse the expression */
	    exprstr = deparse_expr_sweet((Node *)hashExpr, context, useprefix, true);

	    /* And add to str */
	    for (i = 0; i < indent; i++)
		    appendStringInfoString(str, "  ");
	    appendStringInfo(str, "  %s: %s\n", "Hash Key", exprstr);
    }
}                               /* show_motion_keys */

/*
 * Explain a partition selector node, including partition elimination expression
 * and number of statically selected partitions, if available.
 */
static void
explain_partition_selector(PartitionSelector *ps, Plan *parent,
						   StringInfo str, int indent, ExplainState *es)
{
	if (ps->printablePredicate)
	{
		List	   *context;
		bool		useprefix;
		char	   *exprstr;
		int			i;

		/* Set up deparsing context */
		context = deparse_context_for_plan((Node *) parent,
										   (Node *) outerPlan(parent),
										   es->rtable,
										   es->pstmt->subplans);
		useprefix = list_length(es->rtable) > 1;

		/* Deparse the expression */
		exprstr = deparse_expr_sweet(ps->printablePredicate, context, useprefix, false);

		/* And add to str */
		for (i = 0; i < indent; i++)
			appendStringInfo(str, "  ");
		appendStringInfo(str, "  %s: %s\n", "Filter", exprstr);
	}

	if (ps->staticSelection)
	{
		int nPartsSelected = list_length(ps->staticPartOids);
		int nPartsTotal = countLeafPartTables(ps->relid);
		for (int i = 0; i < indent; i++)
		{
			appendStringInfoString(str, "  ");
		}

		appendStringInfo(str, "  Partitions selected: %d (out of %d)\n", nPartsSelected, nPartsTotal);
	}
}

/*
 * Fetch the name of an index in an EXPLAIN
 *
 * We allow plugins to get control here so that plans involving hypothetical
 * indexes can be explained.
 */
static const char *
explain_get_index_name(Oid indexId)
{
	const char *result;

	if (explain_get_index_name_hook)
		result = (*explain_get_index_name_hook) (indexId);
	else
		result = NULL;
	if (result == NULL)
	{
		/* default behavior: look in the catalogs and quote it */
		result = get_rel_name(indexId);
		if (result == NULL)
			elog(ERROR, "cache lookup failed for index %u", indexId);
		result = quote_identifier(result);
	}
	return result;
}
