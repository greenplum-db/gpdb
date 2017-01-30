/*-------------------------------------------------------------------------
 *
 * explain.c
 *	  Explain query execution plans
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994-5, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/commands/explain.c,v 1.169 2008/01/01 19:45:49 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/xact.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_type.h"
#include	 "commands/defrem.h"
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
#include "utils/resscheduler.h"
#include "utils/tuplesort.h"
#include "utils/xml.h"
#include "cdb/cdbdisp.h"                /* CheckDispatchResult() */
#include "cdb/cdbexplain.h"             /* cdbexplain_recvExecStats */
#include "cdb/cdbpartition.h"
#include "cdb/cdbpullup.h"              /* cdbpullup_targetlist() */
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbpathlocus.h"
#include "cdb/memquota.h"
#include "miscadmin.h"

#ifdef USE_ORCA
extern char *SzDXLPlan(Query *parse);
extern StringInfo OptVersion();
#endif


/* Hook for plugins to get control in ExplainOneQuery() */
ExplainOneQuery_hook_type ExplainOneQuery_hook = NULL;

/* Hook for plugins to get control in explain_get_index_name() */
explain_get_index_name_hook_type explain_get_index_name_hook = NULL;

/* OR-able flags for ExplainXMLTag() */
#define X_OPENING 0
#define X_CLOSING 1
#define X_CLOSE_IMMEDIATE 2
#define X_NOWHITESPACE 4

extern bool Test_print_direct_dispatch_info;

static void ExplainOneQuery(Query *query, ExplainState *es,
				const char *queryString, ParamListInfo params);
static void report_triggers(ResultRelInfo *rInfo, bool show_relname,
				ExplainState *es);

#ifdef USE_ORCA
static void ExplainDXL(Query *query, ExplainStmt *stmt,
							const char *queryString,
							ParamListInfo params, TupOutputState *tstate);
#endif
#ifdef USE_CODEGEN
static void ExplainCodegen(PlanState *planstate, TupOutputState *tstate);
#endif
static double elapsed_time(instr_time *starttime);
static void ExplainNode(Plan *plan, PlanState *planstate,
				Plan *outer_plan,
				const char *relationship, const char *plan_name,
				ExplainState *es);
static ErrorData *explain_defer_error(ExplainState *es);
static void show_plan_tlist(Plan *plan, ExplainState *es);
static void show_qual(List *qual, const char *qlabel, Plan *plan,
		  Plan *outer_plan,  bool useprefix, ExplainState *es);
static void show_scan_qual(List *qual, const char *qlabel,
			   Plan *scan_plan, Plan *outer_plan,
			   ExplainState *es);
static void show_upper_qual(List *qual, const char *qlabel, Plan *plan,
				ExplainState *es);
static void show_sort_keys(SortState *sortstate, ExplainState *es);
static void show_sort_info(SortState *sortstate, ExplainState *es);
static const char *explain_get_index_name(Oid indexId);
static void ExplainScanTarget(Scan *plan, ExplainState *es);

static void ExplainMemberNodes(List *plans, PlanState **planstate,
				   Plan *outer_plan, ExplainState *es);
static void ExplainSubPlans(List *plans, const char *relationship,
					ExplainState *es);
static void ExplainSubPlans(List *plans, const char *relationship,
							ExplainState *es);
static void ExplainPropertyList(const char *qlabel, List *data,
								ExplainState *es);
static void ExplainProperty(const char *qlabel, const char *value,
							bool numeric, ExplainState *es);
#define ExplainPropertyText(qlabel, value, es)  \
	ExplainProperty(qlabel, value, false, es)
static void ExplainPropertyInteger(const char *qlabel, int value,
								   ExplainState *es);
static void ExplainPropertyLong(const char *qlabel, long value,
								ExplainState *es);
static void ExplainPropertyFloat(const char *qlabel, double value, int ndigits,
								 ExplainState *es);
static void ExplainOpenGroup(const char *objtype, const char *labelname,
				 bool labeled, ExplainState *es);
static void ExplainCloseGroup(const char *objtype, const char *labelname,
				 bool labeled, ExplainState *es);
static void ExplainDummyGroup(const char *objtype, const char *labelname,
							  ExplainState *es);
static void ExplainBeginOutput(ExplainState *es);
static void ExplainEndOutput(ExplainState *es);
static void ExplainXMLTag(const char *tagname, int flags, ExplainState *es);
static void ExplainJSONLineEnding(ExplainState *es);
static void appendGangAndDirectDispatchInfo( PlanState *planstate, int sliceId, ExplainState *es);


static void
show_grouping_keys(Plan  *plan,
                   int          numCols,
                   AttrNumber  *subplanColIdx,
                   const char  *qlabel, ExplainState *es);
static void
show_motion_keys(Plan *plan, List *hashExpr, int nkeys, AttrNumber *keycols,
			     const char *qlabel, ExplainState *es);

static void
explain_partition_selector(PartitionSelector *ps, Plan *parent, ExplainState *es);


/*
 * ExplainQuery -
 *	  execute an EXPLAIN command
 */
void
ExplainQuery(ExplainStmt *stmt, const char *queryString,
			 ParamListInfo params, DestReceiver *dest)
{
	ExplainState es;
	Oid		   *param_types;
	int			num_params;
	TupOutputState *tstate;
	List	   *rewritten;
	ListCell   *lc;

	/* Initialize ExplainState. */
		ExplainInitState(&es);

		/* Parse options list. */
		foreach(lc, stmt->options)
		{
			DefElem *opt = (DefElem *) lfirst(lc);

			if (strcmp(opt->defname, "analyze") == 0)
				es.analyze = defGetBoolean(opt);
			else if (strcmp(opt->defname, "verbose") == 0)
				es.verbose = defGetBoolean(opt);
			else if (strcmp(opt->defname, "costs") == 0)
				es.costs = defGetBoolean(opt);
			else if (strcmp(opt->defname, "format") == 0)
			{
				char   *p = defGetString(opt);

				if (strcmp(p, "text") == 0)
					es.format = EXPLAIN_FORMAT_TEXT;
				else if (strcmp(p, "xml") == 0)
					es.format = EXPLAIN_FORMAT_XML;
				else if (strcmp(p, "json") == 0)
					es.format = EXPLAIN_FORMAT_JSON;
				else
					ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("unrecognized value for EXPLAIN option \"%s\": \"%s\"",
								opt->defname, p)));
			}

			else
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("unrecognized EXPLAIN option \"%s\"",
								opt->defname)));
		}
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

	/* emit opening boilerplate */
	ExplainBeginOutput(&es);

	if (rewritten == NIL)
	{
		/* In the case of an INSTEAD NOTHING, tell at least that . But in
		 * non-text format, the output is delimited, so this isn't necessary.
		 *
		 */
		if (es.format == EXPLAIN_FORMAT_TEXT)
			appendStringInfoString(es.str, "Query rewrites to nothing");
	}
	else
	{
		ListCell *l;

		/* Explain every plan */
		foreach(l, rewritten)
		{
			ExplainOneQuery((Query *) lfirst(l), &es, queryString, params);

			/* Separate plans with an appropriate separator */
			if (lnext(l) != NULL)
				ExplainSeparatePlans(&es);
		}
	}
	/* emit closing boilerplate */
	ExplainEndOutput(&es);
	Assert(es.indent == 0);

	/* output tuples */
	tstate = begin_tup_output_tupdesc(dest, ExplainResultDesc(stmt));
	if (es.format == EXPLAIN_FORMAT_TEXT)
		do_text_output_multiline(tstate, es.str->data);
	else
		do_text_output_oneline(tstate, es.str->data);
	end_tup_output(tstate);

	pfree(es.str->data);
}

void
ExplainInitState(ExplainState *es)
{
	/* set default options */
	memset(es, 0, sizeof(ExplainState));
	es->costs = true;
	/* Prepare output buffer */
	es->str = makeStringInfo();
}

/*
 * ExplainResultDesc -
 *	  construct the result tupledesc for an EXPLAIN
 */
TupleDesc
ExplainResultDesc(ExplainStmt *stmt)
{
	TupleDesc	tupdesc;
	ListCell   *lc;
	bool		xml = false;

	/* Check for XML format option */
	foreach(lc, stmt->options)
	{
		DefElem *opt = (DefElem *) lfirst(lc);

		if (strcmp(opt->defname, "format") == 0)
		{
			char   *p = defGetString(opt);

			xml = (strcmp(p, "xml") == 0);
		}
	}

	/* Need a tuple descriptor representing a single TEXT or XML column */
	tupdesc = CreateTemplateTupleDesc(1, false);
	TupleDescInitEntry(tupdesc, (AttrNumber) 1, "QUERY PLAN",
					   xml ? XMLOID : TEXTOID, -1, 0);
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
ExplainOneQuery(Query *query, ExplainState *es,
			const char *queryString, ParamListInfo params)
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
		ExplainOneUtility(query->utilityStmt, es, queryString, params);
		return;
	}

	/* if an advisor plugin is present, let it manage things */
	if (ExplainOneQuery_hook)
		(*ExplainOneQuery_hook) (query, es, queryString, params);
	else
	{
		PlannedStmt *plan;

		/* plan the query */
		plan = planner(query, 0, params);

		/* run it (if needed) and produce output */
		ExplainOnePlan(plan, es, queryString, params);
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
ExplainOneUtility(Node *utilityStmt, ExplainState *es,
				  const char *queryString, ParamListInfo params)
{
	if (utilityStmt == NULL)
		return;

	if (IsA(utilityStmt, ExecuteStmt))
		ExplainExecuteQuery((ExecuteStmt *) utilityStmt, es,
							queryString, params);
	else if (IsA(utilityStmt, NotifyStmt))
	{
		if (es->format == EXPLAIN_FORMAT_TEXT)
			appendStringInfoString(es->str, "NOTIFY\n");
		else
			ExplainDummyGroup("Notify", NULL, es);
	}
	else
	{
		if (es->format == EXPLAIN_FORMAT_TEXT)
			appendStringInfoString(es->str,
							   "Utility statements have no plan structure");
		else
			ExplainDummyGroup("Utility Statement", NULL, es);
	}
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
ExplainOnePlan(PlannedStmt *plannedstmt, ExplainState *es,
		const char *queryString, ParamListInfo params)
{
	QueryDesc  *queryDesc;
	instr_time	starttime;
	double		totaltime = 0;
	EState     *estate = NULL;
	int			eflags;
	int         nb;
	MemoryContext explaincxt = CurrentMemoryContext;


	/*
	 * Update snapshot command ID to ensure this query sees results of any
	 * previously executed queries.  (It's a bit cheesy to modify
	 * ActiveSnapshot without making a copy, but for the limited ways in which
	 * EXPLAIN can be invoked, I think it's OK, because the active snapshot
	 * shouldn't be shared with anything else anyway.)
	 */
	ActiveSnapshot->curcid = GetCurrentCommandId(false);

	/* Create a QueryDesc requesting no	*/
	 queryDesc = CreateQueryDesc(plannedstmt,
								queryString,
								ActiveSnapshot, InvalidSnapshot,
								None_Receiver, params, es->analyze);

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

    /*
     * Start timing.
     */
    INSTR_TIME_SET_CURRENT(starttime);

	/* If analyzing, we need to cope with queued triggers */
	if (es->analyze)
		AfterTriggerBeginQuery();

    /* Allocate workarea for summary stats. */
    if (es->analyze)
    {
        es->showstatctx = cdbexplain_showExecStatsBegin(queryDesc,
                                                        starttime);

        /* Attach workarea to QueryDesc so ExecSetParamPlan() can find it. */
        queryDesc->showstatctx = es->showstatctx;
    }

	/* Select execution options */
	if (es->analyze)
		eflags = 0;				/* default run-to-completion flags */
	else
		eflags = EXEC_FLAG_EXPLAIN_ONLY;

    if (gp_resqueue_memory_policy != RESQUEUE_MEMORY_POLICY_NONE)
    {
		if (superuser())
		{
			queryDesc->plannedstmt->query_mem = ResourceQueueGetSuperuserQueryMemoryLimit();			
		}
		else
		{
			queryDesc->plannedstmt->query_mem = ResourceQueueGetQueryMemoryLimit(queryDesc->plannedstmt, GetResQueueId());			
		}
	}

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
	if (es->analyze)
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

	es->pstmt = queryDesc->plannedstmt;
	es->rtable = queryDesc->plannedstmt->rtable;

	ExplainOpenGroup("Query", NULL, true, es);

	if (queryDesc->plannedstmt->planTree && estate->es_sliceTable)
	{
		Node   *saved_es_sliceTable;

		/* Little two-step to get EXPLAIN VERBOSE to show slice table. */
		saved_es_sliceTable = queryDesc->plannedstmt->planTree->sliceTable;		/* probably NULL */
		queryDesc->plannedstmt->planTree->sliceTable = (Node *) queryDesc->estate->es_sliceTable;
		//explain_outNode(es, queryDesc);
		queryDesc->plannedstmt->planTree->sliceTable = saved_es_sliceTable;
	}
	/*
     * Produce the EXPLAIN report into buf.  (Sometimes we get internal errors
     * while doing this; try to proceed with a partial report anyway.)
     */
    PG_TRY();
    {
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
			appendStringInfo(es->str, "%s", cmdName);

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
			appendStringInfo(es->str, " (slice%d; segments: %d)", sliceNum, numSegments);
			appendStringInfo(es->str, "  (rows=%.0f width=%d)\n", ceil(childPlan->plan_rows / numSegments), childPlan->plan_width);
			appendStringInfo(es->str, "  ->  ");

		}
    	ExplainPrintPlan(es, queryDesc);

    }
    PG_CATCH();
    {
		MemoryContextSwitchTo(explaincxt);
        es->deferredError = explain_defer_error(es);

        /* Keep a NUL at the end of the output buffer. */
        appendStringInfoChar(es->str, '\0');
    }
    PG_END_TRY();

	/*
	 * If we ran the command, run any AFTER triggers it queued.  (Note this
	 * will not include DEFERRED triggers; since those don't run until end of
	 * transaction, we can't measure them.)  Include into total runtime.
     * Skip triggers if there has been an error.
	 */
	if (es->analyze &&
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

		ExplainOpenGroup("Triggers", "Triggers", false, es);

		show_relname = (numrels > 1 || targrels != NIL);
		rInfo = queryDesc->estate->es_result_relations;
		for (nr = 0; nr < numrels; rInfo++, nr++)
			report_triggers(rInfo, show_relname, es);

		foreach(l, targrels)
		{
			rInfo = (ResultRelInfo *) lfirst(l);
			report_triggers(rInfo, show_relname, es);
		}

		ExplainCloseGroup("Triggers", "Triggers", false, es);
	}

    /*
     * Display per-slice and whole-query statistics.
     */
    if (es->analyze)
        cdbexplain_showExecStatsEnd(queryDesc->plannedstmt, es->showstatctx, es->str, estate);

    /*
     * Show non-default GUC settings that might have affected the plan.
     */
    List *gucs_to_show = gp_guc_list_show( PGC_S_DEFAULT, gp_guc_list_for_explain);

	if (length(gucs_to_show) )
	{
		ListCell *cell;
		if ( es->format == EXPLAIN_FORMAT_TEXT)
			{
				appendStringInfo(es->str, "Settings:  ");
				foreach(cell, gucs_to_show)
				{
					appendStringInfo(es->str, "%s=%s; ", ((NameValue *)(cell->data.ptr_value))->name, ((NameValue *)(cell->data.ptr_value))->value);
				}
				truncateStringInfo(es->str, es->str->len - 2);  /* drop final "; " */
				appendStringInfoChar(es->str, '\n');
			}
		else
		{
			ExplainOpenGroup("Settings", "Settings", false, es);
			foreach(cell, gucs_to_show)
			{
				ExplainPropertyText( ((NameValue *)(cell->data.ptr_value))->name, ((NameValue *)(cell->data.ptr_value))->value, es);
			}
			ExplainCloseGroup("Settings", "Settings", false, es);
		}
		list_free(gucs_to_show);
	}

#ifdef USE_ORCA
    /* Display optimizer status: either 'legacy query optimizer' or Orca version number */
    if (optimizer_explain_show_status)
    {
		appendStringInfo(&buf, "Optimizer status: ");
    	if (queryDesc->plannedstmt->planGen == PLANGEN_PLANNER)
    	{
			appendStringInfo(&buf, "legacy query optimizer\n");
    	}
    	else /* PLANGEN_OPTIMIZER */
    	{
    		StringInfo str = OptVersion();
			appendStringInfo(&buf, "PQO version %s\n", str->data);
			pfree(str->data);
			pfree(str);
    	}
    }
#endif

    /*
     * Display final elapsed time.
     */
	if (es->analyze)
	{
		if (es->format == EXPLAIN_FORMAT_TEXT)
			appendStringInfo(es->str, "Total runtime: %.3f ms\n",
						 1000.0 * totaltime);
		else
			ExplainPropertyFloat("Total Runtime", 1000.0 * totaltime,
					3, es);
	}

	ExplainCloseGroup("Query", NULL, true, es);

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
        //EndCommand("EXPLAIN", tstate->dest->mydest);

        /* Resume handling the error.  Clean up and send the NOTICE message. */
        es->deferredError = NULL;
        ReThrowError(edata);
    }

    FreeQueryDesc(queryDesc);

	/* We need a CCI just in case query expanded to multiple plans */
	if (es->analyze)
		CommandCounterIncrement();
}          /* ExplainOnePlan_internal */


/*
 * ExplainPrintPlan -
 *	  convert a QueryDesc's plan tree to text and append it to es->str
 *
 * The caller should have set up the options fields of *es, as well as
 * initializing the output buffer es->str.  Other fields in *es are
 * initialized here.
 *
 * NB: will not work on utility statements
 */
void
ExplainPrintPlan(ExplainState *es, QueryDesc *queryDesc)
{
	Assert(queryDesc->plannedstmt != NULL);
	es->pstmt = queryDesc->plannedstmt;
	es->rtable = queryDesc->plannedstmt->rtable;
	ExplainNode(queryDesc->plannedstmt->planTree, queryDesc->planstate,
				NULL, NULL, NULL, es);
}

/*
 * report_triggers -
 *		report execution stats for a single relation's triggers
 */
static void
report_triggers(ResultRelInfo *rInfo, bool show_relname, ExplainState *es)
{
	int			nt;

	if (!rInfo->ri_TrigDesc || !rInfo->ri_TrigInstrument)
		return;
	for (nt = 0; nt < rInfo->ri_TrigDesc->numtriggers; nt++)
	{
		Trigger    *trig = rInfo->ri_TrigDesc->triggers + nt;
		Instrumentation *instr = rInfo->ri_TrigInstrument + nt;
		char	   *relname;
		char	   *conname = NULL;

		/* Must clean up instrumentation state */
		InstrEndLoop(instr);

		/*
		 * We ignore triggers that were never invoked; they likely aren't
		 * relevant to the current query type.
		 */
		if (instr->ntuples == 0)
			continue;

		ExplainOpenGroup("Trigger", NULL, true, es);

		relname = RelationGetRelationName(rInfo->ri_RelationDesc);
		if (OidIsValid(trig->tgconstraint))
			conname = get_constraint_name(trig->tgconstraint);

		/*
		 * In text format, we avoid printing both the trigger name and the
		 * constraint name unless VERBOSE is specified.  In non-text
		 * formats we just print everything.
		 */
		if (es->format == EXPLAIN_FORMAT_TEXT)
		{
			if (es->verbose || conname == NULL)
				appendStringInfo(es->str, "Trigger %s", trig->tgname);
			else
				appendStringInfoString(es->str, "Trigger");
			if (conname)
				appendStringInfo(es->str, " for constraint %s", conname);
			if (show_relname)
				appendStringInfo(es->str, " on %s", relname);
			appendStringInfo(es->str, ": time=%.3f calls=%.0f\n",
							 1000.0 * instr->total, instr->ntuples);
		}
		else
		{
			ExplainPropertyText("Trigger Name", trig->tgname, es);
			if (conname)
				ExplainPropertyText("Constraint Name", conname, es);
			ExplainPropertyText("Relation", relname, es);
			ExplainPropertyFloat("Time", 1000.0 * instr->total, 3, es);
			ExplainPropertyFloat("Calls", instr->ntuples, 0, es);
		}

		if (conname)
			pfree(conname);

		ExplainCloseGroup("Trigger", NULL, true, es);
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
 * ExplainNode -
 *	  Appends a description of the Plan node to es->str
 *
 * planstate points to the executor state node corresponding to the plan node.
 * We need this to get at the instrumentation data (if any) as well as the
 * list of subplans.
 *
 * outer_plan, if not null, references another plan node that is the outer
 * side of a join with the current node.  This is only interesting for
 * deciphering runtime keys of an inner indexscan.
 *
 * relationship describes the relationship of this plan node to its parent
 * (eg, "Outer", "Inner"); it can be null at top level.  plan_name is an
 * optional name to be attached to the node.
 *
 * In text format, es->indent is controlled in this function since we only
 * want it to change at Plan-node boundaries.  In non-text formats, es->indent
 * corresponds to the nesting depth of logical output groups, and therefore
 * is controlled by ExplainOpenGroup/ExplainCloseGroup.
 */
static void
ExplainNode(Plan *plan, PlanState *planstate,
			Plan *outer_plan,
			const char *relationship, const char *plan_name,
			ExplainState *es)
{
	const char *pname;		/* node type name for text output */
	const char *sname; 		/* node type name for non-text output */
	const char *strategy =  NULL;
	const char *operation = NULL;

	int 			save_indent = es->indent;
	bool 			haschildren;
	Slice 		*currentSlice = es->currentSlice;
	int         nSenders = 0;
	int         nReceivers = 0;
	int		  sliceId = 0;

	float	scaleFactor = 1.0;

	Assert(plan);

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

	switch (nodeTag(plan))
	{
		case T_Result:
			pname = sname = "Result";
			break;
		case T_Repeat:
			pname = sname = "Repeat";
			break;
		case T_Append:
			pname = sname = "Append";
			break;
		case T_Sequence:
			pname =  sname = "Sequence";
			break;
		case T_BitmapAnd:
			pname = sname = "BitmapAnd";
			break;
		case T_BitmapOr:
			pname = sname = "BitmapOr";
			break;
		case T_NestLoop:
			pname =  sname = "Nested Loop";
			break;
		case T_MergeJoin:
			pname = "Merge";	/* "Join" gets added by jointype switch */
			sname = "Merge Join";
			break;
		case T_HashJoin:
			pname = "Hash";		/* "Join" gets added by jointype switch */
			sname = "Hash Join";
			break;
		case T_SeqScan:
			pname = sname = "Seq Scan";
			break;
		case T_AppendOnlyScan:
			pname = sname =  "Append-only Scan";
			break;
		case T_AOCSScan:
			pname = sname =  "Append-only Columnar Scan";
			break;
		case T_TableScan:
			pname = sname =  "Table Scan";
			break;
		case T_DynamicTableScan:
			pname = sname =  "Dynamic Table Scan";
			break;
		case T_ExternalScan:
			pname = sname =  "External Scan";
			break;
		case T_IndexScan:
			pname = sname = "Index Scan";
			break;
		case T_DynamicIndexScan:
			pname = sname =  "Dynamic Index Scan";
			break;
		case T_BitmapIndexScan:
			pname = sname = "Bitmap Index Scan";
			break;
		case T_BitmapHeapScan:
			pname = sname = "Bitmap Heap Scan";
			break;
		case T_BitmapAppendOnlyScan:
			if (((BitmapAppendOnlyScan *)plan)->isAORow)
				pname = sname =  "Bitmap Append-Only Row-Oriented Scan";
			else
				pname = sname =  "Bitmap Append-Only Column-Oriented Scan";
			break;
		case T_BitmapTableScan:
			pname = sname =  "Bitmap Table Scan";
			break;
		case T_TidScan:
			pname = sname = "Tid Scan";
			break;
		case T_SubqueryScan:
			pname = sname = "Subquery Scan";
			break;
		case T_FunctionScan:
			pname = sname = "Function Scan";
			break;
		case T_ValuesScan:
			pname = sname = "Values Scan";
			break;
		case T_ShareInputScan:
			{
				/* FIXME */
				ShareInputScan *sisc = (ShareInputScan *) plan;
				appendStringInfo(es->str, "Shared Scan (share slice:id %d:%d)",
						currentSlice ? currentSlice->sliceIndex : -1, sisc->share_id);
				pname = sname = "";
			}
			break;
		case T_CteScan:
			pname = sname = "CTE Scan";
			break;
		case T_Material:
			pname = sname = "Materialize";
			break;
		case T_Sort:
			pname = sname = "Sort";
			break;
		case T_Agg:
			switch (((Agg *) plan)->aggstrategy)
			{
				case AGG_PLAIN:
					pname = "Aggregate";
					strategy = "Plain";
					break;
				case AGG_SORTED:
					pname = "GroupAggregate";
					strategy = "Sorted";
					break;
				case AGG_HASHED:
					pname = "HashAggregate";
					strategy = "Hashed";
					break;
				default:
					pname = "Aggregate ???";
					strategy = "???";
					break;
			}
			break;
		case T_Unique:
			pname = sname = "Unique";
			break;
		case T_SetOp:
			switch (((SetOp *) plan)->cmd)
			{
				case SETOPCMD_INTERSECT:
					pname = "SetOp Intersect";
					strategy = "Intersect";
					break;
				case SETOPCMD_INTERSECT_ALL:
					pname = "SetOp Intersect All";
					strategy = "Intersect All";
					break;
				case SETOPCMD_EXCEPT:
					pname = "SetOp Except";
					strategy = "Except";
					break;
				case SETOPCMD_EXCEPT_ALL:
					pname = "SetOp Except All";
					strategy = "Except All";
					break;
				default:
					pname = "SetOp ???";
					strategy = "???";
					break;
			}
			break;
		case T_Limit:
			pname = sname = "Limit";
			break;
		case T_Hash:
			pname = sname = "Hash";
			break;
		case T_Motion:
			{
				Motion	   *pMotion = (Motion *) plan;
				SliceTable *sliceTable = planstate->state->es_sliceTable;
				Slice *slice = (Slice *)list_nth(sliceTable->slices, pMotion->motionID);

				nSenders = slice->numGangMembersToBeActive;
				nReceivers = 0;

				/* scale the number of rows by the number of segments sending data */
				scaleFactor = nSenders;

				switch (pMotion->motionType)
				{
					case MOTIONTYPE_HASH:
						nReceivers = pMotion->numOutputSegs;
						pname = sname = "Redistribute Motion";
						break;
					case MOTIONTYPE_FIXED:
						nReceivers = pMotion->numOutputSegs;
						if (nReceivers == 0)
						{
							pname =  sname ="Broadcast Motion";
							nReceivers = getgpsegmentCount();
						}
						else
						{
							scaleFactor = 1;
							pname = sname = "Gather Motion";
						}
						break;
					case MOTIONTYPE_EXPLICIT:
						nReceivers = getgpsegmentCount();
						pname = sname = "Explicit Redistribute Motion";
						break;
					default:
						pname = sname = "Motion ???";
						break;
				}

				if (es->format == EXPLAIN_FORMAT_TEXT)
				{
					appendStringInfo(es->str, "%s %d:%d", pname,
						nSenders, nReceivers);
					appendGangAndDirectDispatchInfo(planstate, pMotion->motionID, es);
				}
				else
				{
					sliceId = pMotion->motionID;
				}
				pname = "";

			}
			break;
		case T_DML:
			{
				switch (es->pstmt->commandType)
				{
					case CMD_INSERT:
						pname = operation = "Insert";
						break;
					case CMD_DELETE:
						pname = operation =  "Delete";
						break;
					case CMD_UPDATE:
						pname = operation =  "Update";
						break;
					default:
						pname = "DML ???";
						break;
				}
			}
			break;

		case T_SplitUpdate:
			pname = sname = "Split";
			break;
		case T_AssertOp:
			pname = sname =  "Assert";
			break;
		case T_PartitionSelector:
			pname = sname =  "Partition Selector";
			break;
		case T_RowTrigger:
			pname = sname =  "RowTrigger";
			break;
		default:
			pname = sname = "???";
			break;
	}

	ExplainOpenGroup("Plan",
				 relationship ? NULL : "Plan",
				 true, es);

	if (es->format == EXPLAIN_FORMAT_TEXT)
	{
		if (plan_name)
		{
			appendStringInfoSpaces(es->str, es->indent * 2);
			appendStringInfo(es->str, "%s\n", plan_name);
			es->indent++;
		}
		if (es->indent)
		{
			appendStringInfoSpaces(es->str, es->indent * 2);
			appendStringInfoString(es->str, "->  ");
			es->indent += 2;
		}
		appendStringInfoString(es->str, pname);
		es->indent++;
	}
	else
	{
		ExplainPropertyText("Node Type", sname, es);
		ExplainPropertyInteger("Senders", nSenders, es);
		ExplainPropertyInteger("Receivers", nReceivers, es);

		if (strategy)
			ExplainPropertyText("Strategy", strategy, es);
		if (relationship)
			ExplainPropertyText("Parent Relationship", relationship, es);
		if (plan_name)
			ExplainPropertyText("Subplan Name", plan_name, es);

		appendGangAndDirectDispatchInfo(planstate, sliceId, es);

	}

	switch (nodeTag(plan))
	{
		case T_IndexScan:

		{
			IndexScan *indexscan = (IndexScan *) plan;
			const char *indexname =
							explain_get_index_name(indexscan->indexid);

			if (es->format == EXPLAIN_FORMAT_TEXT)
			{
				if (ScanDirectionIsBackward(indexscan->indexorderdir))
					appendStringInfoString(es->str, " Backward");
					appendStringInfo(es->str, " using %s", indexname);
			}
			else
			{
				const char *scandir;

				switch (indexscan->indexorderdir)
				{
					case BackwardScanDirection:
						scandir = "Backward";
						break;
					case NoMovementScanDirection:
						scandir = "NoMovement";
						break;
					case ForwardScanDirection:
						scandir = "Forward";
						break;
					default:
						scandir = "???";
						break;
					}
					ExplainPropertyText("Scan Direction", scandir, es);
					ExplainPropertyText("Index Name", indexname, es);
			}
		}
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
		case T_SubqueryScan:
		case T_FunctionScan:
		case T_ValuesScan:
		case T_CteScan:
		case T_TableFunctionScan:
			ExplainScanTarget((Scan *) plan, es);
			break;

		case T_BitmapIndexScan:
			{
				BitmapIndexScan *bitmapindexscan = (BitmapIndexScan *) plan;
				const char *indexname =
					explain_get_index_name(bitmapindexscan->indexid);

				if (es->format == EXPLAIN_FORMAT_TEXT)
					appendStringInfo(es->str, " on %s", indexname);
				else
					ExplainPropertyText("Index Name", indexname, es);
			}
			break;
		case T_ShareInputScan:
			{
				ShareInputScan *sisc = (ShareInputScan *) plan;
				if (es->format == EXPLAIN_FORMAT_TEXT)
					appendStringInfo(es->str, "Shared Scan (share slice:id %d:%d)",
									currentSlice ? currentSlice->sliceIndex : -1, sisc->share_id);
				else
				{
					ExplainPropertyInteger("Slice", currentSlice ? currentSlice->sliceIndex : -1, es);
					ExplainPropertyInteger("Share", sisc->share_id, es);
				}
			}
			break;
		case T_PartitionSelector:
			{
				PartitionSelector *ps = (PartitionSelector *)plan;
				char *relname = get_rel_name(ps->relid);
				if ( es->format == EXPLAIN_FORMAT_TEXT)
				{
					appendStringInfo(es->str, " for %s", quote_identifier(relname));
					if (0 != ps->scanId)
					{
						appendStringInfo(es->str, " (dynamic scan id: %d)", ps->scanId);
					}
				}
				else
				{
					ExplainPropertyText("Partition Name", relname, es);
					if (0 != ps->scanId)
					{
						ExplainPropertyInteger("Dynamic Scan Id", ps->scanId, es);
					}
				}
			}
			break;
		case T_NestLoop:
		case T_MergeJoin:
		case T_HashJoin:
			{
				const char *jointype;

				switch (((Join *) plan)->jointype)
				{
					case JOIN_INNER:
						jointype = "Inner";
						break;
					case JOIN_LEFT:
						jointype = "Left";
						break;
					case JOIN_FULL:
						jointype = "Full";
						break;
					case JOIN_RIGHT:
						jointype = "Right";
						break;
					case JOIN_IN:
						jointype = "EXISTS";
						break;
					case JOIN_LASJ:
						jointype = "Left Anti Semi";
						break;
					case JOIN_LASJ_NOTIN:
						jointype = "Left Anti Semi (Not-in)";
						break;
					default:
						jointype = "???";
						break;
				}
				if (es->format == EXPLAIN_FORMAT_TEXT)
				{
					/*
					 * For historical reasons, the join type is interpolated
					 * into the node type name...
					 */
					if (((Join *) plan)->jointype != JOIN_INNER)
						appendStringInfo(es->str, " %s Join", jointype);
					else if (!IsA(plan, NestLoop))
						appendStringInfo(es->str, " Join");
				}
				else
					ExplainPropertyText("Join Type", jointype, es);
			}
			break;
		case T_SetOp:
			{
				const char *setopcmd;

				switch (((SetOp *) plan)->cmd)
				{
					case SETOPCMD_INTERSECT:
						setopcmd = "Intersect";
						break;
					case SETOPCMD_INTERSECT_ALL:
						setopcmd = "Intersect All";
						break;
					case SETOPCMD_EXCEPT:
						setopcmd = "Except";
						break;
					case SETOPCMD_EXCEPT_ALL:
						setopcmd = "Except All";
						break;
					default:
						setopcmd = "???";
						break;
				}
				if (es->format == EXPLAIN_FORMAT_TEXT)
					appendStringInfo(es->str, " %s", setopcmd);
				else
					ExplainPropertyText("Command", setopcmd, es);
			}
			break;
		case T_Material:
			pname = sname =  "Materialize";
			break;
		case T_Sort:
			pname = sname =  "Sort";
			break;

		default:
			break;
	}

	Assert(scaleFactor > 0.0)

	if (es->costs)
	{
		if (es->format == EXPLAIN_FORMAT_TEXT)
		{
			appendStringInfo(es->str, "  (cost=%.2f..%.2f rows=%.0f width=%d)",
							 plan->startup_cost, plan->total_cost,
							 plan->plan_rows, plan->plan_width);
		}
		else
		{
			ExplainPropertyFloat("Startup Cost", plan->startup_cost, 2, es);
			ExplainPropertyFloat("Total Cost", plan->total_cost, 2, es);
			ExplainPropertyFloat("Plan Rows", plan->plan_rows, 0, es);
			ExplainPropertyInteger("Plan Width", plan->plan_width, es);
		}
	}

	/*
	 * We have to forcibly clean up the instrumentation state because we
	 * haven't done ExecutorEnd yet.  This is pretty grotty ...
	 */
	if (planstate->instrument)
		InstrEndLoop(planstate->instrument);

	if (planstate->instrument && planstate->instrument->nloops > 0)
	{
		double		nloops = planstate->instrument->nloops;
		double		startup_sec = 1000.0 * planstate->instrument->startup / nloops;
		double		total_sec = 1000.0 * planstate->instrument->total / nloops;
		double		rows = planstate->instrument->ntuples / nloops;

		if (es->format == EXPLAIN_FORMAT_TEXT)
		{
			appendStringInfo(es->str,
							 " (actual time=%.3f..%.3f rows=%.0f loops=%.0f)",
							 startup_sec, total_sec, rows, nloops);
		}
		else
		{
			ExplainPropertyFloat("Actual Startup Time", startup_sec, 3, es);
			ExplainPropertyFloat("Actual Total Time", total_sec, 3, es);
			ExplainPropertyFloat("Actual Rows", rows, 0, es);
			ExplainPropertyFloat("Actual Loops", nloops, 0, es);
		}
	}
	else if (es->analyze)
	{
		if (es->format == EXPLAIN_FORMAT_TEXT)
			appendStringInfo(es->str, " (never executed)");
		else
		{
			ExplainPropertyFloat("Actual Startup Time", 0.0, 3, es);
			ExplainPropertyFloat("Actual Total Time", 0.0, 3, es);
			ExplainPropertyFloat("Actual Rows", 0.0, 0, es);
			ExplainPropertyFloat("Actual Loops", 0.0, 0, es);
		}
	}
	if (gp_resqueue_print_operator_memory_limits)
	{
		if (es->format == EXPLAIN_FORMAT_TEXT)
		{
			appendStringInfo(es->str, " (operatorMem=" UINT64_FORMAT "KB)",
						 PlanStateOperatorMemKB(planstate));
		}
		else
		{
			ExplainPropertyInteger("operatorMem", PlanStateOperatorMemKB(planstate), es);
		}
	}

	if (es->format == EXPLAIN_FORMAT_TEXT)
		appendStringInfoChar(es->str, '\n');

#ifdef DEBUG_EXPLAIN
	appendStringInfo(es->str, "plan->targetlist=%s\n", nodeToString(plan->targetlist));
#endif

	/* quals, sort keys, etc */
	switch (nodeTag(plan))
	{
		case T_IndexScan:
		case T_DynamicIndexScan:
			show_scan_qual(((IndexScan *) plan)->indexqualorig,
						   "Index Cond", plan, outer_plan, es);
			show_scan_qual(plan->qual, "Filter", plan, outer_plan, es);
			break;
		case T_BitmapIndexScan:
			show_scan_qual(((BitmapIndexScan *) plan)->indexqualorig,
						   "Index Cond", plan, outer_plan, es);
			break;
		case T_BitmapHeapScan:
		case T_BitmapAppendOnlyScan:
		case T_BitmapTableScan:
			/* XXX do we want to show this in production? */
			if (nodeTag(plan) == T_BitmapHeapScan)
			{
				show_scan_qual(((BitmapHeapScan *) plan)->bitmapqualorig,
							   "Recheck Cond", plan,
							   outer_plan, es);
			}
			else if (nodeTag(plan) == T_BitmapAppendOnlyScan)
			{
				show_scan_qual(((BitmapAppendOnlyScan *) plan)->bitmapqualorig,
							   "Recheck Cond", plan,
							   outer_plan, es);
			}
			else if (nodeTag(plan) == T_BitmapTableScan)
			{
				show_scan_qual(((BitmapTableScan *) plan)->bitmapqualorig,
							   "Recheck Cond",  plan,
							   outer_plan, es);
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
		case T_SubqueryScan:
			show_scan_qual(plan->qual,
					"Filter", plan,
					outer_plan, es);
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
						"TID Cond", plan,
						outer_plan, es);
				show_scan_qual(plan->qual,
						"Filter", plan,
						outer_plan, es);
			}
			break;
		case T_NestLoop:
			show_upper_qual(((NestLoop *) plan)->join.joinqual,
					"Join Filter", plan,  es);
			show_upper_qual(plan->qual,
					"Filter", plan,  es);
			break;
		case T_MergeJoin:
			show_upper_qual(((MergeJoin *) plan)->mergeclauses,
							"Merge Cond", plan, es);
			show_upper_qual(((MergeJoin *) plan)->join.joinqual,
							"Join Filter", plan,  es);
			show_upper_qual(plan->qual, "Filter", plan, es);
			break;
		case T_HashJoin:
			{
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
								"Hash Cond", plan, es);
				show_upper_qual(((HashJoin *) plan)->join.joinqual,
								"Join Filter", plan, es);
				show_upper_qual(plan->qual,
								"Filter", plan, es);
			}
			break;
		case T_Agg:
			show_upper_qual(plan->qual,
							"Filter", plan, es);
			show_grouping_keys(plan,
						       ((Agg *) plan)->numCols,
						       ((Agg *) plan)->grpColIdx,
						       "Group By", es);
			break;
		case T_Window:
			{
				Window *window = (Window *)plan;
				ListCell *cell;
				char orderKeyStr[32]; /* XXX big enough */
				int i;

				if ( window->numPartCols > 0 )
				{
					show_grouping_keys(plan,
									   window->numPartCols,
									   window->partColIdx,
									   "Partition By", es);
				}

				if (list_length(window->windowKeys) > 1)
					i = 0;
				else
					i = -1;

				foreach(cell, window->windowKeys)
				{
					if ( i < 0 )
						sprintf(orderKeyStr, "Order By");
					else
					{
						sprintf(orderKeyStr, "Order By (level %d)", ++i);
					}

					show_sort_keys((SortState *)planstate, es);
				}
				/* XXX don't show framing for now */
			}
			break;
		case T_TableFunctionScan:
			{
				show_scan_qual(plan->qual,
							   "Filter",
							   plan,
							   outer_plan, es);

				/* Partitioning and ordering information */

			}
			break;

		case T_Unique:
			show_motion_keys(plan,
								 NIL,
								 ((Unique *) plan)->numCols,
								 ((Unique *) plan)->uniqColIdx,
								 "Group By", es);
			break;
		case T_Sort:
			{
				bool bLimit = (((Sort *) plan)->limitCount
							   || ((Sort *) plan)->limitOffset);

				bool bNoDup = ((Sort *) plan)->noduplicates;

				char *SortKeystr = "Sort Key";

				if ((bLimit && bNoDup))
					SortKeystr = "Sort Key (Limit Distinct)";
				else if (bLimit)
					SortKeystr = "Sort Key (Limit)";
				else if (bNoDup)
					SortKeystr = "Sort Key (Distinct)";
				show_sort_keys((SortState*)planstate, es);
				show_sort_info((SortState *) planstate, es);
			}
			break;
		case T_Result:
			show_upper_qual((List *) ((Result *) plan)->resconstantqual,
							"One-Time Filter", plan, es);
			show_upper_qual(plan->qual, "Filter", plan, es);
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
							"Merge Key", es);

                /* Descending into a new slice. */
                if (sliceTable)
                    es->currentSlice = (Slice *)list_nth(sliceTable->slices,
                                                         pMotion->motionID);
			}
			break;
		case T_AssertOp:
			{
				show_upper_qual(plan->qual,
								"Assert Cond", plan, es);
			}
			break;
		case T_PartitionSelector:
			{
				/*
				 *  FIXME this is probably wrong
				 *  our code calls explain_outNode recursively
				 *  with the correct parentPlan
				 */

				explain_partition_selector((PartitionSelector *) plan, innerPlan(plan)?outerPlan(plan):NULL, es);
			}
			break;

		default:
			break;
	}

	/* Get ready to display the child plans */
	haschildren = plan->initPlan ||
		outerPlan(plan) ||
		innerPlan(plan) ||
		IsA(plan, Append) ||
		IsA(plan, BitmapAnd) ||
		IsA(plan, BitmapOr) ||
		IsA(plan, SubqueryScan) ||
		planstate->subPlan;
	if (haschildren)
		ExplainOpenGroup("Plans", "Plans", false, es);

	/* initPlan-s */
	if (plan->initPlan)
		ExplainSubPlans(planstate->initPlan, "InitPlan", es);

	/* lefttree */
	if (outerPlan(plan))
	{
		/*
		 * Ordinarily we don't pass down our own outer_plan value to our child
		 * nodes, but in bitmap scan trees we must, since the bottom
		 * BitmapIndexScan nodes may have outer references.
		 */
		ExplainNode(outerPlan(plan), outerPlanState(planstate),
					IsA(plan, BitmapHeapScan) ? outer_plan : NULL,
					"Outer", NULL, es);
	}

	/* righttree */
	if (innerPlan(plan))
	{
		ExplainNode(innerPlan(plan), innerPlanState(planstate),
					outerPlan(plan),
					"Inner", NULL, es);
	}

	/* special child plans */
	switch (nodeTag(plan))
	{
		case T_Append:
			ExplainMemberNodes(((Append *) plan)->appendplans,
							   ((AppendState *) planstate)->appendplans,
							   outer_plan, es);
			break;
		case T_BitmapAnd:
			ExplainMemberNodes(((BitmapAnd *) plan)->bitmapplans,
							   ((BitmapAndState *) planstate)->bitmapplans,
							   outer_plan, es);
			break;
		case T_BitmapOr:
			ExplainMemberNodes(((BitmapOr *) plan)->bitmapplans,
							   ((BitmapOrState *) planstate)->bitmapplans,
							   outer_plan, es);
			break;
		case T_SubqueryScan:
			{
				SubqueryScan *subqueryscan = (SubqueryScan *) plan;
				SubqueryScanState *subquerystate = (SubqueryScanState *) planstate;

				ExplainNode(subqueryscan->subplan, subquerystate->subplan,
							NULL,
							"Subquery", NULL, es);
			}
			break;
		default:
			break;
	}

	/* subPlan-s */
	if (planstate->subPlan)
		ExplainSubPlans(planstate->subPlan, "SubPlan", es);

	/* end of child plans */
	if (haschildren)
		ExplainCloseGroup("Plans", "Plans", false, es);

	/* in text format, undo whatever indentation we added */
	if (es->format == EXPLAIN_FORMAT_TEXT)
		es->indent = save_indent;

	ExplainCloseGroup("Plan",
					  relationship ? NULL : "Plan",
					  true, es);

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
appendGangAndDirectDispatchInfo( PlanState *planstate, int sliceId, ExplainState *es)
{
	SliceTable *sliceTable = planstate->state->es_sliceTable;
	Slice *slice = (Slice *)list_nth(sliceTable->slices, sliceId);

	switch (slice->gangType)
	{
		case GANGTYPE_UNALLOCATED:
		case GANGTYPE_ENTRYDB_READER:
			if (es->format == EXPLAIN_FORMAT_TEXT)
				appendStringInfo(es->str, "  (slice%d)", sliceId);
			else
				ExplainPropertyInteger("slice", sliceId, es);
			break;

		case GANGTYPE_PRIMARY_WRITER:
		case GANGTYPE_PRIMARY_READER:
		case GANGTYPE_SINGLETON_READER:
		{
			int numSegments;

			if (es->format == EXPLAIN_FORMAT_TEXT)
				appendStringInfo(es->str, "  (slice%d)", sliceId);
			else
				ExplainPropertyInteger("slice", sliceId, es);

			if (slice->directDispatch.isDirectDispatch)
			{
				Assert( list_length(slice->directDispatch.contentIds) == 1);
				numSegments = list_length(slice->directDispatch.contentIds);
			}
			else
			{
				numSegments = slice->numGangMembersToBeActive;
			}
			if (es->format == EXPLAIN_FORMAT_TEXT)
				appendStringInfo(es->str, " segments: %d)", numSegments);
			else
				ExplainPropertyInteger("segments", numSegments, es);

			break;
		}
	}
}

#ifdef XXX
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
explain_outNode(Plan *plan, PlanState *planstate,
				Plan *outer_plan, Plan *parentPlan,
				const char * relationship, ExplainState *es)
{
	const char	   *pname = NULL;
	const char	   *sname = NULL;
	const char 	   *strategy = NULL;
	const char 	   *operation = NULL;

    Slice      *currentSlice = es->currentSlice;    /* save */
	bool		skip_outer=false;
	char       *skip_outer_msg = NULL;
	float		scaleFactor = 1.0; /* we will divide planner estimates by this factor to produce
									  per-segment estimates */

	Assert(plan);

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
/*
 * FIXME
 */
	if (plan == NULL)
	{
		appendStringInfoChar(es->str, '\n');
		return;
	}

	switch (nodeTag(plan))
	{
		case T_Result:
			pname = sname = "Result";
			break;
		case T_Repeat:
			pname = sname = "Repeat";
			break;
		case T_Append:
			pname =  sname = "Append";
			break;
		case T_Sequence:
			pname =  sname = "Sequence";
			break;
		case T_BitmapAnd:
			pname =  sname = "BitmapAnd";
			break;
		case T_BitmapOr:
			pname =  sname = "BitmapOr";
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
					pname =  sname =  "Nested Loop";
					break;
				case JOIN_LEFT:
					pname =  sname = "Nested Loop Left Join";
					break;
				case JOIN_FULL:
					pname =  sname = "Nested Loop Full Join";
					break;
				case JOIN_RIGHT:
					pname =  sname = "Nested Loop Right Join";
					break;
				case JOIN_IN:
					pname =  sname = "Nested Loop EXISTS Join";
					break;
				case JOIN_LASJ:
					pname =  sname = "Nested Loop Left Anti Semi Join";
					break;
				case JOIN_LASJ_NOTIN:
					pname =  sname = "Nested Loop Left Anti Semi Join (Not-In)";
					break;
				default:
					pname =  sname = "Nested Loop ??? Join";
					break;
			}
			break;
		case T_MergeJoin:
			switch (((MergeJoin *) plan)->join.jointype)
			{
				case JOIN_INNER:
					pname = "Merge";
					sname = 	"Merge Join";
					break;
				case JOIN_LEFT:
					pname = "Merge";
					sname = "Merge Left Join";
					break;
				case JOIN_FULL:
					pname = "Merge";
					sname = "Merge Full Join";
					break;
				case JOIN_RIGHT:
					pname = "Merge";
					sname = "Merge Right Join";
					break;
				case JOIN_IN:
					pname = "Merge";
					sname = "Merge EXISTS Join";
					break;
				case JOIN_LASJ:
					pname = "Merge";
					sname = "Merge Left Anti Semi Join";
					break;
				case JOIN_LASJ_NOTIN:
					pname = "Merge";
					sname = "Merge Left Anti Semi Join (Not-In)";
					break;
				default:
					pname = "Merge";
					sname = "Merge ??? Join";
					break;
			}
			break;
		case T_HashJoin:
			switch (((HashJoin *) plan)->join.jointype)
			{
				case JOIN_INNER:
					pname = "Hash";
					sname = "Hash Join";
					break;
				case JOIN_LEFT:
					pname = "Hash";
					sname = "Hash Left Join";
					break;
				case JOIN_FULL:
					pname = "Hash";
					sname = "Hash Full Join";
					break;
				case JOIN_RIGHT:
					pname = "Hash";
					sname = "Hash Right Join";
					break;
				case JOIN_IN:
					pname = "Hash";
					sname = "Hash EXISTS Join";
					break;
				case JOIN_LASJ:
					pname = "Hash";
					sname = "Hash Left Anti Semi Join";
					break;
				case JOIN_LASJ_NOTIN:
					pname = "Hash";
					sname = "Hash Left Anti Semi Join (Not-In)";
					break;
				default:
					pname = "Hash";
					sname = "Hash ??? Join";
					break;
			}
			break;
		case T_SeqScan:
			pname = sname = "Seq Scan";
			break;
		case T_AppendOnlyScan:
			pname = sname =  "Append-only Scan";
			break;
		case T_AOCSScan:
			pname = sname =  "Append-only Columnar Scan";
			break;
		case T_TableScan:
			pname = sname =  "Table Scan";
			break;
		case T_DynamicTableScan:
			pname = sname =  "Dynamic Table Scan";
			break;
		case T_ExternalScan:
			pname = sname =  "External Scan";
			break;
		case T_IndexScan:
			pname = sname =  "Index Scan";
			break;
		case T_DynamicIndexScan:
			pname = sname =  "Dynamic Index Scan";
			break;
		case T_BitmapIndexScan:
			pname = sname =  "Bitmap Index Scan";
			break;
		case T_BitmapHeapScan:
			pname = sname =  "Bitmap Heap Scan";
			break;
		case T_BitmapAppendOnlyScan:
			if (((BitmapAppendOnlyScan *)plan)->isAORow)
				pname = sname =  "Bitmap Append-Only Row-Oriented Scan";
			else
				pname = sname =  "Bitmap Append-Only Column-Oriented Scan";
			break;
		case T_BitmapTableScan:
			pname = sname =  "Bitmap Table Scan";
			break;
		case T_TidScan:
			pname = sname =  "Tid Scan";
			break;
		case T_SubqueryScan:
			pname = sname =  "Subquery Scan";
			break;
		case T_FunctionScan:
			pname = sname =  "Function Scan";
			break;
		case T_ValuesScan:
			pname = sname =  "Values Scan";
			break;
		case T_ShareInputScan:
			{
				/* FIXME */
				ShareInputScan *sisc = (ShareInputScan *) plan;
				appendStringInfo(es->str, "Shared Scan (share slice:id %d:%d)",
						currentSlice ? currentSlice->sliceIndex : -1, sisc->share_id);
				pname = sname = "";
			}
			break;
		case T_Material:
			pname = sname = "Materialize";
			break;
		case T_Sort:
			pname = sname = "Sort";
			break;
		case T_Agg:
			switch (((Agg *) plan)->aggstrategy)
			{
				case AGG_PLAIN:
					pname = "Aggregate";
					strategy = "Plain";
					break;
				case AGG_SORTED:
					pname = "GroupAggregate";
					strategy = "sorted";
					break;
				case AGG_HASHED:
					pname = "HashAggregate";
					strategy = "Hashed";
					break;
				default:
					pname = "Aggregate ???";
					strategy = "???";
					break;
			}
			break;
		case T_Window:
			pname = sname = "Window";
			break;
		case T_TableFunctionScan:
			pname = sname = "Table Function Scan";
			break;
		case T_Unique:
			pname = sname = "Unique";
			break;
		case T_SetOp:
			switch (((SetOp *) plan)->cmd)
			{
				case SETOPCMD_INTERSECT:
					pname = "SetOp Intersect";
					strategy = "Intersect";
					break;
				case SETOPCMD_INTERSECT_ALL:
					pname = "SetOp Intersect All";
					strategy = "Intersect All";
					break;
				case SETOPCMD_EXCEPT:
					pname = "SetOp Except";
					strategy = "Except";
					break;
				case SETOPCMD_EXCEPT_ALL:
					pname = "SetOp Except All";
					strategy = "Except All";
					break;
				default:
					pname = "SetOp ???";
					strategy = "???";
					break;
			}
			break;
		case T_Limit:
			pname =  sname = "Limit";
			break;
		case T_Hash:
			pname = sname = "Hash";
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
						pname = sname = "Redistribute Motion";
						break;
					case MOTIONTYPE_FIXED:
						nReceivers = pMotion->numOutputSegs;
						if (nReceivers == 0)
						{
							pname =  sname ="Broadcast Motion";
							nReceivers = getgpsegmentCount();
						}
						else
						{
							scaleFactor = 1;
							pname = sname = "Gather Motion";
						}
						break;
					case MOTIONTYPE_EXPLICIT:
						nReceivers = getgpsegmentCount();
						pname = sname = "Explicit Redistribute Motion";
						break;
					default:
						pname = sname = "Motion ???";
						break;
				}

				if (es->format == EXPLAIN_FORMAT_TEXT)
				{
					appendStringInfo(es->str, "%s %d:%d", pname,
						nSenders, nReceivers);
				}
				else
				{
					ExplainPropertyText("Node Type", sname, es);
					if (strategy)
						ExplainPropertyText("Strategy", strategy, es);

				}
				appendGangAndDirectDispatchInfo(planstate, pMotion->motionID, es);
				pname = "";

			}
			break;
		case T_DML:
			{
				switch (es->pstmt->commandType)
				{
					case CMD_INSERT:
						pname = operation = "Insert";
						break;
					case CMD_DELETE:
						pname = operation =  "Delete";
						break;
					case CMD_UPDATE:
						pname = operation =  "Update";
						break;
					default:
						pname = "DML ???";
						break;
				}
			}
			break;
		case T_SplitUpdate:
			pname = sname = "Split";
			break;
		case T_AssertOp:
			pname = sname =  "Assert";
			break;
		case T_PartitionSelector:
			pname = sname =  "Partition Selector";
			break;
		case T_RowTrigger:
 			pname = sname =  "RowTrigger";
 			break;
		default:
			pname = sname =  "???";
			break;
	}

	ExplainOpenGroup("Plan",
			relationship ? NULL : "Plan",
			true, es);

	if (es->format == EXPLAIN_FORMAT_TEXT)
	{
		if (es->indent)
		{
			appendStringInfoSpaces(es->str, es->indent * 2);
			appendStringInfoString(es->str, "->  ");
			es->indent += 2;
		}
		appendStringInfoString(es->str, pname);
		es->indent++;
	}
	else
	{
		ExplainPropertyText("Node Type", sname, es);
		if (strategy)
			ExplainPropertyText("Strategy", strategy, es);
		if (operation)
			ExplainPropertyText("Operation", operation, es);
		if (relationship)
			ExplainPropertyText("Parent Relationship", relationship, es);
	}
	switch (nodeTag(plan))
	{
		case T_IndexScan:
			if (ScanDirectionIsBackward(((IndexScan *) plan)->indexorderdir))
				appendStringInfoString(es->str, " Backward");
			appendStringInfo(es->str, " using %s",
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

				appendStringInfo(es->str, " on %s",
								 quote_identifier(relname));
				if (strcmp(rte->eref->aliasname, relname) != 0)
					appendStringInfo(es->str, " %s",
									 quote_identifier(rte->eref->aliasname));

				/* Print dynamic scan id for dytnamic scan operators */
				if (isDynamicScan((Scan *)plan))
				{
					appendStringInfo(es->str, " (dynamic scan id: %d)",
									 ((Scan *)plan)->partIndexPrintable);
				}
			}
			break;
		case T_BitmapIndexScan:
			appendStringInfo(es->str, " on %s",
				explain_get_index_name(((BitmapIndexScan *) plan)->indexid));
			break;
		case T_SubqueryScan:
			if (((Scan *) plan)->scanrelid > 0)
			{
				RangeTblEntry *rte = rt_fetch(((Scan *) plan)->scanrelid,
											  es->rtable);

				appendStringInfo(es->str, " %s",
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
				appendStringInfo(es->str, " on %s", quote_identifier(proname));
				if (strcmp(rte->eref->aliasname, proname) != 0)
					appendStringInfo(es->str, " %s",
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

				appendStringInfo(es->str, " on %s",
								 quote_identifier(proname));
				if (strcmp(rte->eref->aliasname, proname) != 0)
					appendStringInfo(es->str, " %s",
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

				appendStringInfo(es->str, " on %s",
								 quote_identifier(valsname));
			}
			break;
		case T_PartitionSelector:
			{
				PartitionSelector *ps = (PartitionSelector *)plan;
				char *relname = get_rel_name(ps->relid);
				appendStringInfo(es->str, " for %s", quote_identifier(relname));
				if (0 != ps->scanId)
				{
					appendStringInfo(es->str, " (dynamic scan id: %d)", ps->scanId);
				}
			}
			break;
		default:
			break;
	}

	Assert(scaleFactor > 0.0);

	if (es->costs)
	{
		if (es->format == EXPLAIN_FORMAT_TEXT)
		{
			appendStringInfo(es->str, "  (cost=%.2f..%.2f rows=%.0f width=%d)",
					 plan->startup_cost, plan->total_cost,
					 ceil(plan->plan_rows / scaleFactor), plan->plan_width);
		}
		else
		{
			ExplainPropertyFloat("Startup Cost", plan->startup_cost, 2, es);
			ExplainPropertyFloat("Total Cost", plan->total_cost, 2, es);
			ExplainPropertyFloat("Plan Rows", plan->plan_rows, 0, es);
			ExplainPropertyInteger("Plan Width", plan->plan_width, es);
		}
	}
	if (gp_resqueue_print_operator_memory_limits)
	{
		if (es->format == EXPLAIN_FORMAT_TEXT)
		{
			appendStringInfo(es->str, " (operatorMem=" UINT64_FORMAT "KB)",
						 PlanStateOperatorMemKB(planstate));
		}
		else
		{
			ExplainPropertyInteger("operatorMem", PlanStateOperatorMemKB(planstate), es);
		}
	}

	if (es->format == EXPLAIN_FORMAT_TEXT)
		appendStringInfoChar(es->str, '\n');

#ifdef DEBUG_EXPLAIN
	appendStringInfo(es->str, "plan->targetlist=%s\n", nodeToString(plan->targetlist));
#endif

	/* quals, sort keys, etc */
	switch (nodeTag(plan))
	{
		case T_IndexScan:
		case T_DynamicIndexScan:
			show_scan_qual(((IndexScan *) plan)->indexqualorig,
						   "Index Cond",plan,
						   outer_plan, es);
			show_scan_qual(plan->qual,
						   "Filter", plan,
						   outer_plan, es);
			break;
		case T_BitmapIndexScan:
			show_scan_qual(((BitmapIndexScan *) plan)->indexqualorig,
						   "Index Cond", plan,
						   outer_plan, es);
			break;
		case T_BitmapHeapScan:
		case T_BitmapAppendOnlyScan:
		case T_BitmapTableScan:
			/* XXX do we want to show this in production? */
			if (nodeTag(plan) == T_BitmapHeapScan)
			{
				show_scan_qual(((BitmapHeapScan *) plan)->bitmapqualorig,
							   "Recheck Cond", plan,
							   outer_plan, es);
			}
			else if (nodeTag(plan) == T_BitmapAppendOnlyScan)
			{
				show_scan_qual(((BitmapAppendOnlyScan *) plan)->bitmapqualorig,
							   "Recheck Cond", plan,
							   outer_plan, es);
			}
			else if (nodeTag(plan) == T_BitmapTableScan)
			{
				show_scan_qual(((BitmapTableScan *) plan)->bitmapqualorig,
							   "Recheck Cond",  plan,
							   outer_plan, es);
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
			show_scan_qual(plan->qual,
						   "Filter", plan,
						   outer_plan, es);
			break;
		case T_SubqueryScan:
			show_scan_qual(plan->qual,
						   "Filter", plan,
						   outer_plan, es);
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
							   "TID Cond", plan,
							   outer_plan, es);
				show_scan_qual(plan->qual,
							   "Filter", plan,
							   outer_plan, es);
			}
			break;
		case T_NestLoop:
			show_upper_qual(((NestLoop *) plan)->join.joinqual,
							"Join Filter", plan, es);
			show_upper_qual(plan->qual,
							"Filter", plan, es);
			break;
		case T_MergeJoin:
			show_upper_qual(((MergeJoin *) plan)->mergeclauses,
							"Merge Cond", plan, es);
			show_upper_qual(((MergeJoin *) plan)->join.joinqual,
							"Join Filter", plan, es);
			show_upper_qual(plan->qual,
							"Filter", plan, es);
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
							"Hash Cond", plan, es);
			show_upper_qual(((HashJoin *) plan)->join.joinqual,
							"Join Filter", plan, es);
			show_upper_qual(plan->qual,
							"Filter", plan, es);
			break;
		}
		case T_Agg:
			show_upper_qual(plan->qual,
							"Filter", plan, es);
			show_grouping_keys(plan,
						       ((Agg *) plan)->numCols,
						       ((Agg *) plan)->grpColIdx,
						       "Group By", es);
			break;
		case T_Window:
			{
				Window *window = (Window *)plan;
				ListCell *cell;
				char orderKeyStr[32]; /* XXX big enough */
				int i;

				if ( window->numPartCols > 0 )
				{
					show_grouping_keys(plan,
									   window->numPartCols,
									   window->partColIdx,
									   "Partition By", es);
				}

				if (list_length(window->windowKeys) > 1)
					i = 0;
				else
					i = -1;

				foreach(cell, window->windowKeys)
				{
					if ( i < 0 )
						sprintf(orderKeyStr, "Order By");
					else
					{
						sprintf(orderKeyStr, "Order By (level %d)", ++i);
					}

					show_sort_keys((SortState *)planstate, es);
				}
				/* XXX don't show framing for now */
			}
			break;
		case T_TableFunctionScan:
		{
			show_scan_qual(plan->qual,
						   "Filter",
						   plan,
						   outer_plan, es);

			/* Partitioning and ordering information */
			
		}
		break;

		case T_Unique:
			show_motion_keys(plan,
                             NIL,
						     ((Unique *) plan)->numCols,
						     ((Unique *) plan)->uniqColIdx,
						     "Group By", es);
			break;
		case T_Sort:
		{
			bool bLimit = (((Sort *) plan)->limitCount
						   || ((Sort *) plan)->limitOffset);

			bool bNoDup = ((Sort *) plan)->noduplicates;

			char *SortKeystr = "Sort Key";

			if ((bLimit && bNoDup))
				SortKeystr = "Sort Key (Limit Distinct)";
			else if (bLimit)
				SortKeystr = "Sort Key (Limit)";
			else if (bNoDup)
				SortKeystr = "Sort Key (Distinct)";

			show_sort_keys((SortState *)planstate, es);
			show_sort_info((SortState *) planstate, es);
		}
			break;
		case T_Result:
			show_upper_qual((List *) ((Result *) plan)->resconstantqual,
							"One-Time Filter", plan, es);
			show_upper_qual(plan->qual,
							"Filter", plan, es);
			break;
		case T_Repeat:
			show_upper_qual(plan->qual,
							"Filter", plan, es);
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
							"Merge Key", es);

                /* Descending into a new slice. */
                if (sliceTable)
                    es->currentSlice = (Slice *)list_nth(sliceTable->slices,
                                                         pMotion->motionID);
			}
			break;
		case T_AssertOp:
			{
				show_upper_qual(plan->qual,
								"Assert Cond", plan, es);
			}
			break;
		case T_PartitionSelector:
			{
				explain_partition_selector((PartitionSelector *) plan, parentPlan, es);
			}
			break;
		default:
			break;
	}

    /* CDB: Show actual row count, etc. */
	if (planstate->instrument)
	{
        cdbexplain_showExecStats(planstate, es->showstatctx, es);
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


			appendStringInfoSpaces(es->str, es->indent*2);
		    appendStringInfoString(es->str, "  InitPlan");

            /* Subplan might have its own root slice */
            if (sliceTable &&
                sp->qDispSliceId > 0)
            {
                es->currentSlice = (Slice *)list_nth(sliceTable->slices,
                                                     sp->qDispSliceId);
    		    appendGangAndDirectDispatchInfo(planstate, sp->qDispSliceId, es);
            }
            else
            {
                /*
                 * CDB TODO: In non-parallel query, all qDispSliceId's are 0.
                 * Should fill them in properly before ExecutorStart(), but
                 * for now, just omit the slice id.
                 */
            }

            ExplainSeparatePlans(es);
			if (es->format == EXPLAIN_FORMAT_TEXT)
			{
				appendStringInfoSpaces(es->str, es->indent);
				appendStringInfo(es->str, "    ->  ");
			}

			explain_outNode(exec_subplan_get_plan(es->pstmt, sp),
							sps->planstate,
							NULL, plan, "InitPlan",es);
		}
        es->currentSlice = saved_slice;
	}

	/* lefttree */
	if (outerPlan(plan) && !skip_outer)
	{
		appendStringInfoSpaces(es->str, es->indent);
		appendStringInfo(es->str, "  ->  ");

		/*
		 * Ordinarily we don't pass down our own outer_plan value to our child
		 * nodes, but in bitmap scan trees we must, since the bottom
		 * BitmapIndexScan nodes may have outer references.
		 */
		explain_outNode(outerPlan(plan),
						outerPlanState(planstate),
						(IsA(plan, BitmapHeapScan) |
						 IsA(plan, BitmapAppendOnlyScan) |
						 IsA(plan, BitmapTableScan)) ? outer_plan : NULL,
						plan, "Outer", es);
	}
    else if (skip_outer)
    {
		appendStringInfoSpaces(es->str, es->indent);
		appendStringInfo(es->str, "  ->  ");
		appendStringInfoString(es->str, skip_outer_msg);
        ExplainSeparatePlans(es);
    }

	/* righttree */
	if (innerPlan(plan))
	{
		appendStringInfoSpaces(es->str, es->indent);
		appendStringInfo(es->str, "  ->  ");
		explain_outNode(innerPlan(plan),
						innerPlanState(planstate),
						outerPlan(plan),
						plan, "Inner", es);
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

			appendStringInfoSpaces(es->str, es->indent);
			appendStringInfo(es->str, "  ->  ");

			/*
			 * Ordinarily we don't pass down our own outer_plan value to our
			 * child nodes, but in an Append we must, since we might be
			 * looking at an appendrel indexscan with outer references from
			 * the member scans.
			 */
			explain_outNode(subnode,
							appendstate->appendplans[j],
							outer_plan,
							(Plan *) appendplan, "Member", es);
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

			appendStringInfoSpaces(es->str, es->indent);
			appendStringInfo(es->str, "  ->  ");

			explain_outNode( subnode,
							sequenceState->subplans[j],
							outer_plan,
							plan, "Member", es);
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

			appendStringInfoSpaces(es->str, es->indent);
			appendStringInfo(es->str, "  ->  ");

			explain_outNode(subnode,
							bitmapandstate->bitmapplans[j],
							outer_plan, /* pass down same outer plan */
							plan, "Member", es);
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
			if (es->format == EXPLAIN_FORMAT_TEXT)
			{

				appendStringInfoSpaces(es->str, es->indent);
				appendStringInfo(es->str, "  ->  ");
			}
			explain_outNode(subnode,
							bitmaporstate->bitmapplans[j],
							outer_plan, /* pass down same outer plan */
							plan,"Member", es);
			j++;
		}
	}

	if (IsA(plan, SubqueryScan))
	{
		SubqueryScan *subqueryscan = (SubqueryScan *) plan;
		SubqueryScanState *subquerystate = (SubqueryScanState *) planstate;
		Plan	   *subnode = subqueryscan->subplan;

		if (es->format == EXPLAIN_FORMAT_TEXT)
		{
			appendStringInfoSpaces(es->str, es->indent);
			appendStringInfo(es->str, "  ->  ");
		}
		explain_outNode(subnode,
						subquerystate->subplan,
						NULL,
						plan, "Member", es);
	}

	/* subPlan-s */
	if (planstate->subPlan)
	{
		ListCell   *lst;

		foreach(lst, planstate->subPlan)
		{
			SubPlanState *sps = (SubPlanState *) lfirst(lst);
			SubPlan    *sp = (SubPlan *) sps->xprstate.expr;

			if (es->format == EXPLAIN_FORMAT_TEXT)
			{
				appendStringInfoSpaces(es->str, es->indent);
				appendStringInfo(es->str, "  %s\n", sp->plan_name);

				appendStringInfoSpaces(es->str, es->indent);
				appendStringInfo(es->str, "    ->  ");
			}
			explain_outNode(exec_subplan_get_plan(es->pstmt, sp),
							sps->planstate,
							NULL,
							plan, "SubPlan", es);
		}
	}

    es->currentSlice = currentSlice;    /* restore */
}                               /* explain_outNode */
#endif
#ifdef XXX
/*
 * Show a qualifier expression for a scan plan node
 *
 * Note: outer_plan is the referent for any OUTER vars in the scan qual;
 * this would be the outer side of a nestloop plan.  inner_plan should be
 * NULL except for a SubqueryScan plan node, where it should be the subplan.
 */
static void
show_scan_qual(List *qual, const char *qlabel,
			   Plan *outer_plan, Plan *inner_plan,
			    int indent, ExplainState *es)
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
	context = deparse_context_for_plan((Node *) outer_plan,
									   (Node *) inner_plan,
									   es->rtable);
	useprefix = (outer_plan != NULL || inner_plan != NULL);

	/* Deparse the expression */
	exprstr = deparse_expr_sweet(node, context, useprefix, false);

	/* And add to str */
	for (i = 0; i < indent; i++)
		appendStringInfo(es->str, "  ");
	appendStringInfo(es->str, "  %s: %s\n", qlabel, exprstr);
}
#endif  /* XXX */
#ifdef XXX
/*
 * Show a qualifier expression for an upper-level plan node
 */
static void
show_upper_qual(List *qual, const char *qlabel, Plan *plan,
				 int indent, ExplainState *es)
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
	context = deparse_context_for_plan((Node *) outerPlan(plan),
									   (Node *) innerPlan(plan),
									   es->rtable);
	useprefix = list_length(es->rtable) > 1;

	/* Deparse the expression */
	node = (Node *) make_ands_explicit(qual);
	exprstr = deparse_expr_sweet(node, context, useprefix, false);

	/* And add to str */
	for (i = 0; i < indent; i++)
		appendStringInfo(es->str, "  ");
	appendStringInfo(es->str, "  %s: %s\n", qlabel, exprstr);
}
#endif

/*
 * CDB: Show GROUP BY keys for an Agg or Group node.
 */
void
show_grouping_keys(Plan  *plan,
                   int          numCols,
                   AttrNumber  *subplanColIdx,
                   const char  *qlabel,
			       ExplainState *es)
{
    Plan       *subplan = plan->lefttree;
    List	   *context;
    char	   *exprstr;
    bool		useprefix = list_length(es->rtable) > 1;
    int			keyno;
	int         num_null_cols = 0;
	int         rollup_gs_times = 0;

    if (numCols <= 0)
        return;

	appendStringInfoSpaces(es->str, es->indent);
    appendStringInfo(es->str, "  %s: ", qlabel);

    Node *outerPlan = (Node *) outerPlan(subplan);
    Node *innerPlan = (Node *) innerPlan(subplan);

    /*
     * For Append we cannot obtain outerPlan as the lefttree
     * is set to NULL. So, we extract the first child from the
     * list of appendplans
     */
    if (IsA(subplan, Append))
    {
    	Assert(NULL == outerPlan);
    	Assert(NULL == innerPlan);

    	Append *append = (Append *) subplan;

    	/*
    	 * Append node with no children is legal, at least when mark_dummy_join()
    	 * produces such a node.
    	 */
    	if (NULL != append->appendplans)
    	{
    		outerPlan = list_nth(append->appendplans, 0);
    		Assert(NULL != outerPlan);
    	}
    }

	/* Set up deparse context */
	context = deparse_context_for_plan(outerPlan,
									   innerPlan,
										   es->rtable);

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
			appendStringInfoString(es->str, ", ");
		appendStringInfoString(es->str, exprstr);
    }

	if (rollup_gs_times > 1)
		appendStringInfo(es->str, " (%d times)", rollup_gs_times);

    appendStringInfoChar(es->str, '\n');
}                               /* show_grouping_keys */


/*
 * CDB: Show the hash and merge keys for a Motion node.
 */
void
show_motion_keys(Plan *plan, List *hashExpr, int nkeys, AttrNumber *keycols,
			     const char *qlabel, ExplainState *es)
{
	List	   *context;
	char	   *exprstr;
	bool		useprefix = list_length(es->rtable) > 1;
	int			keyno;

	if (!nkeys && !hashExpr)
		return;

	/* Set up deparse context */
	context = deparse_context_for_plan((Node *) outerPlan(plan),
									   NULL,	/* Motion has no innerPlan */
									   es->rtable);

    /* Merge Receive ordering key */
    if (nkeys > 0)
    {
		appendStringInfoSpaces(es->str, es->indent);
        appendStringInfo(es->str, "  %s: ", qlabel);

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
			    appendStringInfoString(es->str, ", ");
		    appendStringInfoString(es->str, exprstr);
	    }

	    appendStringInfoChar(es->str, '\n');
    }

    /* Hashed repartitioning key */
    if (hashExpr)
    {
	    /* Deparse the expression */
	    exprstr = deparse_expr_sweet((Node *)hashExpr, context, useprefix, true);

	    /* And add to str */
		appendStringInfoSpaces(es->str, es->indent);
	    appendStringInfo(es->str, "  %s: %s\n", "Hash Key", exprstr);
    }
}                               /* show_motion_keys */

/*
 * Explain a partition selector node, including partition elimination expression
 * and number of statically selected partitions, if available.
 */
static void
explain_partition_selector(PartitionSelector *ps, Plan *parent, ExplainState *es)
{
	if (ps->printablePredicate)
	{
		List	   *context;
		bool		useprefix;
		char	   *exprstr;

		/* Set up deparsing context */
		context = deparse_context_for_plan((Node *) parent,
										   (Node *) parent,
										   es->rtable);
		useprefix = list_length(es->rtable) > 1;

		/* Deparse the expression */
		exprstr = deparse_expr_sweet(ps->printablePredicate, context, useprefix, false);

		/* And add to str */
		appendStringInfoSpaces(es->str, es->indent);
		appendStringInfo(es->str, "  %s: %s\n", "Filter", exprstr);
	}

	if (ps->staticSelection)
	{
		int nPartsSelected = list_length(ps->staticPartOids);
		int nPartsTotal = countLeafPartTables(ps->relid);
		appendStringInfoSpaces(es->str, es->indent);
		appendStringInfo(es->str, "  Partitions selected: %d (out of %d)\n", nPartsSelected, nPartsTotal);
	}
}

/*
 * Show the targetlist of a plan node
 */
static void
show_plan_tlist(Plan *plan, ExplainState *es)
{
	List	   *context;
	List		*result = NIL;
	bool		useprefix;
	ListCell   *lc;
	int			i;

	/* No work if empty tlist (this occurs eg in bitmap indexscans) */
	if (plan->targetlist == NIL)
		return;
	/* The tlist of an Append isn't real helpful, so suppress it */
	if (IsA(plan, Append))
		return;

	/* Set up deparsing context */
	context = deparse_context_for_plan((Node *) plan,
									   NULL, es->rtable);
	useprefix = list_length(es->rtable) > 1;

	/* Deparse each non-junk result column */
	i = 0;
	foreach(lc, plan->targetlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(lc);

		if (tle->resjunk)
			continue;
		result = lappend(result,
					     deparse_expression((Node *) tle->expr, context,
												  useprefix, false));
	}

	/* Print results */
	ExplainPropertyList("Output", result, es);
}


/*
 * Show a qualifier expression
 *
 * Note: outer_plan is the referent for any OUTER vars in the scan qual;
 * this would be the outer side of a nestloop plan.  Pass NULL if none.
 */
static void
show_qual(List *qual, const char *qlabel, Plan *plan, Plan *outer_plan,
		  bool useprefix, ExplainState *es)
{
	List	   *context;
	Node	   *node;
	char	   *exprstr;

	/* No work if empty qual */
	if (qual == NIL)
		return;

	/* Convert AND list to explicit AND */
	node = (Node *) make_ands_explicit(qual);

	/* Set up deparsing context */
	context = deparse_context_for_plan((Node *) plan,
									   (Node *) outer_plan,
									   es->rtable);

	/* Deparse the expression */
	exprstr = deparse_expression(node, context, useprefix, false);

	/* And add to es->str */
	ExplainPropertyText(qlabel, exprstr, es);
}

/*
 * Show a qualifier expression for a scan plan node
 */
static void
show_scan_qual(List *qual, const char *qlabel,
			   Plan *scan_plan, Plan *outer_plan,
			    ExplainState *es)
{
	bool		useprefix;

	useprefix = (outer_plan != NULL || IsA(scan_plan, SubqueryScan) ||
			es->verbose);
	show_qual(qual, qlabel, scan_plan, outer_plan, useprefix, es);
}

/*
 * Show a qualifier expression for an upper-level plan node
 */
static void
show_upper_qual(List *qual, const char *qlabel, Plan *plan, ExplainState *es)
{
	bool		useprefix;

	useprefix = (list_length(es->rtable) > 1 || es->verbose);
	show_qual(qual, qlabel, plan, NULL, useprefix, es);
}

/*
 * Show the sort keys for a Sort node.
 */
static void
show_sort_keys(SortState *sortstate, ExplainState *es)
{
	List	   *context;
	List		*result = NIL;
	bool		useprefix;
	int		nkeys;
	int		keyno;
	char	   *exprstr;
	AttrNumber *keycols;

	Sort * plan = (Sort *) sortstate->ss.ps.plan;


	keycols = plan->sortColIdx;
	nkeys = plan->numCols;

	if (nkeys <= 0)
		return;

	useprefix = list_length(es->rtable) > 1;    /*CDB*/

	appendStringInfoSpaces(es->str, es->indent);
	appendStringInfo(es->str, "  %s: ", "Sort Key");

	/* Set up deparsing context */
	context = deparse_context_for_plan((Node *) outerPlan(plan),
									   NULL,	/* Sort has no innerPlan */
									   es->rtable);
	useprefix = list_length(es->rtable) > 1;

	for (keyno = 0; keyno < nkeys; keyno++)
	{
		/* find key expression in tlist */
		AttrNumber	keyresno = keycols[keyno];
		TargetEntry *target = get_tle_by_resno(((Plan *)plan)->targetlist, keyresno);

		if (!target)
			elog(ERROR, "no tlist entry for key %d", keyresno);
		/* Deparse the expression, showing any top-level cast */
		exprstr = deparse_expr_sweet((Node *) target->expr, context,
									 useprefix, true);
		result = lappend(result, exprstr);
	}

	ExplainPropertyList("Sort Key", result, es);
}

/*
 * If it's EXPLAIN ANALYZE, show tuplesort stats for a sort node
 */
static void
show_sort_info(SortState *sortstate, ExplainState *es)
{
	Assert(IsA(sortstate, SortState));
	if (es->analyze && sortstate->sort_Done &&
		sortstate->tuplesortstate != NULL &&
		(gp_enable_mk_sort ?
		 (void *) sortstate->tuplesortstate->sortstore_mk : (void *) sortstate->tuplesortstate->sortstore) != NULL)
	{
		Tuplesortstate	*state = (Tuplesortstate *) sortstate->tuplesortstate;
		const char *sortMethod;
		const char *spaceType;
		long		spaceUsed;

		if (gp_enable_mk_sort)
			tuplesort_get_stats_mk(sortstate->tuplesortstate->sortstore_mk,
					&sortMethod, &spaceType, &spaceUsed);
		else
			tuplesort_get_stats(state, &sortMethod, &spaceType, &spaceUsed);
		if (es->format == EXPLAIN_FORMAT_TEXT)
		{
			appendStringInfoSpaces(es->str, es->indent * 2);
			appendStringInfo(es->str, "Sort Method:  %s  %s: %ldkB\n",
							 sortMethod, spaceType, spaceUsed);
		}
		else
		{
			ExplainPropertyText("Sort Method", sortMethod, es);
			ExplainPropertyLong("Sort Space Used", spaceUsed, es);
			ExplainPropertyText("Sort Space Type", spaceType, es);
		}
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


/*
 * Show the target of a Scan node
 */
static void
ExplainScanTarget(Scan *plan, ExplainState *es)
{
	char	   *objectname = NULL;
	char	   *namespace = NULL;
	const char *objecttag = NULL;

	RangeTblEntry *rte;

	if (plan->scanrelid <= 0)	/* Is this still possible? */
		return;
	rte = rt_fetch(plan->scanrelid, es->rtable);

/*
 * FIXME may need to change this to align with line 1959 of old code
 *
 */
	switch (nodeTag(plan))
	{
		case T_SeqScan:
		case T_IndexScan:
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
			/* Assert it's on a real relation */
			Assert(rte->rtekind == RTE_RELATION);
			objectname = get_rel_name(rte->relid);
			if (es->verbose)
				namespace = get_namespace_name(get_rel_namespace(rte->relid));
			objecttag = "Relation Name";
			break;
		case T_FunctionScan:
			{
				Node	   *funcexpr;

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

					objectname = get_func_name(funcid);
					if (es->verbose)
						namespace =
							get_namespace_name(get_func_namespace(funcid));
				}
				objecttag = "Function Name";
			}
			break;
		case T_ValuesScan:
			Assert(rte->rtekind == RTE_VALUES);
			break;
		case T_CteScan:
			/* Assert it's on a non-self-reference CTE */
			Assert(rte->rtekind == RTE_CTE);
			Assert(!rte->self_reference);
			objectname = rte->ctename;
			objecttag		= "CTE Name";
			break;
#ifdef T_WorkTableScan
		case T_WorkTableScan:
			/* Assert it's on a self-reference CTE */
			Assert(rte->rtekind == RTE_CTE);
			Assert(rte->self_reference);
			objectname = rte->ctename;
			break;
#endif
		case T_TableFunctionScan:
			Assert(rte->rtekind == RTE_TABLEFUNCTION);
			/*
			 * Lookup the function name.
			 *
			 * Unlike RTE_FUNCTION there should be no cases where the
			 * optimizer could have evaluated away the function call.
			 */
			Insist(rte->funcexpr && IsA(rte->funcexpr, FuncExpr));
			FuncExpr *funcexpr = (FuncExpr *) rte->funcexpr;
			objectname	 = get_func_name(funcexpr->funcid);
			objecttag		 = "Table Function Name";
			break;
		default:
			break;
	}

	if (es->format == EXPLAIN_FORMAT_TEXT)
	{
		appendStringInfoString(es->str, " on");
		if (namespace != NULL)
			appendStringInfo(es->str, " %s.%s", quote_identifier(namespace),
							 quote_identifier(objectname));
		else if (objectname != NULL)
			appendStringInfo(es->str, " %s", quote_identifier(objectname));
		if (objectname == NULL ||
			strcmp(rte->eref->aliasname, objectname) != 0)
			appendStringInfo(es->str, " %s",
							 quote_identifier(rte->eref->aliasname));
		/* Print dynamic scan id for dynamic scan operators */
		if (isDynamicScan((Scan *)plan))
		{
			appendStringInfo(es->str, " (dynamic scan id: %d)",
							 ((Scan *)plan)->partIndexPrintable);
		}
	}
	else
	{
		if (objecttag != NULL && objectname != NULL)
			ExplainPropertyText(objecttag, objectname, es);
		if (namespace != NULL)
			ExplainPropertyText("Schema", namespace, es);
		ExplainPropertyText("Alias", rte->eref->aliasname, es);
		/* Print dynamic scan id for dynamic scan operators */
		if (isDynamicScan((Scan *)plan))
			ExplainPropertyInteger("dynamic scan id",
					((Scan *)plan)->partIndexPrintable, es);

	}
}

/*
 * Explain the constituent plans of an Append, BitmapAnd, or BitmapOr node.
 *
 * Ordinarily we don't pass down outer_plan to our child nodes, but in these
 * cases we must, since the node could be an "inner indexscan" in which case
 * outer references can appear in the child nodes.
 */
static void
ExplainMemberNodes(List *plans, PlanState **planstate, Plan *outer_plan,
		           ExplainState *es)
{
	ListCell   *lst;
	int			j = 0;

	foreach(lst, plans)
	{
		Plan	   *subnode = (Plan *) lfirst(lst);

		ExplainNode(subnode, planstate[j],
					outer_plan,
					"Member", NULL,
					es);
		j++;
	}
}


/*
 * Explain a list of SubPlans (or initPlans, which also use SubPlan nodes).
 */
static void
ExplainSubPlans(List *plans, const char *relationship, ExplainState *es)
{
	ListCell   *lst;

	foreach(lst, plans)
	{
		SubPlanState *sps = (SubPlanState *) lfirst(lst);
		SubPlan    *sp = (SubPlan *) sps->xprstate.expr;

		ExplainNode(exec_subplan_get_plan(es->pstmt, sp),
					sps->planstate,
					NULL,
					relationship, sp->plan_name,
					es);
	}
}

/*
 * Explain a property, such as sort keys or targets, that takes the form of
 * a list of unlabeled items.  "data" is a list of C strings.
 */
static void
ExplainPropertyList(const char *qlabel, List *data, ExplainState *es)
{
	ListCell   *lc;
	bool		first = true;

	switch (es->format)
	{
		case EXPLAIN_FORMAT_TEXT:
			appendStringInfoSpaces(es->str, es->indent * 2);
			appendStringInfo(es->str, "%s: ", qlabel);
			foreach(lc, data)
			{
				if (!first)
					appendStringInfoString(es->str, ", ");
				appendStringInfoString(es->str, (const char *) lfirst(lc));
				first = false;
			}
			appendStringInfoChar(es->str, '\n');
			break;

		case EXPLAIN_FORMAT_XML:
			ExplainXMLTag(qlabel, X_OPENING, es);
			foreach(lc, data)
			{
				char   *str;

				appendStringInfoSpaces(es->str, es->indent * 2 + 2);
				appendStringInfoString(es->str, "<Item>");
				str = escape_xml((const char *) lfirst(lc));
				appendStringInfoString(es->str, str);
				appendStringInfoString(es->str, "</Item>\n");
			}
			ExplainXMLTag(qlabel, X_CLOSING, es);
			break;

		case EXPLAIN_FORMAT_JSON:
			ExplainJSONLineEnding(es);
			appendStringInfoSpaces(es->str, es->indent * 2);
			escape_json(es->str, qlabel);
			appendStringInfoString(es->str, ": [");
			foreach(lc, data)
			{
				if (!first)
					appendStringInfoString(es->str, ", ");
				escape_json(es->str, (const char *) lfirst(lc));
				first = false;
			}
			appendStringInfoChar(es->str, ']');
			break;
	}
}

/*
 * Explain a simple property.
 *
 * If "numeric" is true, the value is a number (or other value that
 * doesn't need quoting in JSON).
 *
 * This usually should not be invoked directly, but via one of the datatype
 * specific routines ExplainPropertyText, ExplainPropertyInteger, etc.
 */
static void
ExplainProperty(const char *qlabel, const char *value, bool numeric,
				ExplainState *es)
{
	switch (es->format)
	{
		case EXPLAIN_FORMAT_TEXT:
			appendStringInfoSpaces(es->str, es->indent * 2);
			appendStringInfo(es->str, "%s: %s\n", qlabel, value);
			break;

		case EXPLAIN_FORMAT_XML:
			{
				char   *str;

				appendStringInfoSpaces(es->str, es->indent * 2);
				ExplainXMLTag(qlabel, X_OPENING | X_NOWHITESPACE, es);
				str = escape_xml(value);
				appendStringInfoString(es->str, str);
				ExplainXMLTag(qlabel, X_CLOSING | X_NOWHITESPACE, es);
				appendStringInfoChar(es->str, '\n');
			}
			break;

		case EXPLAIN_FORMAT_JSON:
			ExplainJSONLineEnding(es);
			appendStringInfoSpaces(es->str, es->indent * 2);
			escape_json(es->str, qlabel);
			appendStringInfoString(es->str, ": ");
			if (numeric)
				appendStringInfoString(es->str, value);
			else
				escape_json(es->str, value);
			break;
	}
}

/*
 * Explain an integer-valued property.
 */
static void
ExplainPropertyInteger(const char *qlabel, int value, ExplainState *es)
{
	char	buf[32];

	snprintf(buf, sizeof(buf), "%d", value);
	ExplainProperty(qlabel, buf, true, es);
}

/*
 * Explain a long-integer-valued property.
 */
static void
ExplainPropertyLong(const char *qlabel, long value, ExplainState *es)
{
	char	buf[32];

	snprintf(buf, sizeof(buf), "%ld", value);
	ExplainProperty(qlabel, buf, true, es);
}

/*
 * Explain a float-valued property, using the specified number of
 * fractional digits.
 */
static void
ExplainPropertyFloat(const char *qlabel, double value, int ndigits,
					 ExplainState *es)
{
	char	buf[256];

	snprintf(buf, sizeof(buf), "%.*f", ndigits, value);
	ExplainProperty(qlabel, buf, true, es);
}

/*
 * Open a group of related objects.
 *
 * objtype is the type of the group object, labelname is its label within
 * a containing object (if any).
 *
 * If labeled is true, the group members will be labeled properties,
 * while if it's false, they'll be unlabeled objects.
 */
static void
ExplainOpenGroup(const char *objtype, const char *labelname,
				 bool labeled, ExplainState *es)
{
	switch (es->format)
	{
		case EXPLAIN_FORMAT_TEXT:
			/* nothing to do */
			break;

		case EXPLAIN_FORMAT_XML:
			ExplainXMLTag(objtype, X_OPENING, es);
			es->indent++;
			break;

		case EXPLAIN_FORMAT_JSON:
			ExplainJSONLineEnding(es);
			appendStringInfoSpaces(es->str, 2 * es->indent);
			if (labelname)
			{
				escape_json(es->str, labelname);
				appendStringInfoString(es->str, ": ");
			}
			appendStringInfoChar(es->str, labeled ? '{' : '[');

			/*
			 * In JSON format, the grouping_stack is an integer list.  0 means
			 * we've emitted nothing at this grouping level, 1 means we've
			 * emitted something (and so the next item needs a comma).
			 * See ExplainJSONLineEnding().
			 */
			es->grouping_stack = lcons_int(0, es->grouping_stack);
			es->indent++;
			break;
	}
}

/*
 * Close a group of related objects.
 * Parameters must match the corresponding ExplainOpenGroup call.
 */
static void
ExplainCloseGroup(const char *objtype, const char *labelname,
				  bool labeled, ExplainState *es)
{
	switch (es->format)
	{
		case EXPLAIN_FORMAT_TEXT:
			/* nothing to do */
			break;

		case EXPLAIN_FORMAT_XML:
			es->indent--;
			ExplainXMLTag(objtype, X_CLOSING, es);
			break;

		case EXPLAIN_FORMAT_JSON:
			es->indent--;
			appendStringInfoChar(es->str, '\n');
			appendStringInfoSpaces(es->str, 2 * es->indent);
			appendStringInfoChar(es->str, labeled ? '}' : ']');
			es->grouping_stack = list_delete_first(es->grouping_stack);
			break;
	}
}

/*
 * Emit a "dummy" group that never has any members.
 *
 * objtype is the type of the group object, labelname is its label within
 * a containing object (if any).
 */
static void
ExplainDummyGroup(const char *objtype, const char *labelname, ExplainState *es)
{
	switch (es->format)
	{
		case EXPLAIN_FORMAT_TEXT:
			/* nothing to do */
			break;

		case EXPLAIN_FORMAT_XML:
			ExplainXMLTag(objtype, X_CLOSE_IMMEDIATE, es);
			break;

		case EXPLAIN_FORMAT_JSON:
			ExplainJSONLineEnding(es);
			appendStringInfoSpaces(es->str, 2 * es->indent);
			if (labelname)
			{
				escape_json(es->str, labelname);
				appendStringInfoString(es->str, ": ");
			}
			escape_json(es->str, objtype);
			break;
	}
}

/*
 * Emit the start-of-output boilerplate.
 *
 * This is just enough different from processing a subgroup that we need
 * a separate pair of subroutines.
 */
static void
ExplainBeginOutput(ExplainState *es)
{
	switch (es->format)
	{
		case EXPLAIN_FORMAT_TEXT:
			/* nothing to do */
			break;

		case EXPLAIN_FORMAT_XML:
			appendStringInfoString(es->str,
								   "<explain xmlns=\"http://www.postgresql.org/2009/explain\">\n");
			es->indent++;
			break;

		case EXPLAIN_FORMAT_JSON:
			/* top-level structure is an array of plans */
			appendStringInfoChar(es->str, '[');
			es->grouping_stack = lcons_int(0, es->grouping_stack);
			es->indent++;
			break;
	}
}

/*
 * Emit the end-of-output boilerplate.
 */
static void
ExplainEndOutput(ExplainState *es)
{
	switch (es->format)
	{
		case EXPLAIN_FORMAT_TEXT:
			/* nothing to do */
			break;

		case EXPLAIN_FORMAT_XML:
			es->indent--;
			appendStringInfoString(es->str, "</explain>");
			break;

		case EXPLAIN_FORMAT_JSON:
			es->indent--;
			appendStringInfoString(es->str, "\n]");
			es->grouping_stack = list_delete_first(es->grouping_stack);
			break;
	}
}

/*
 * Put an appropriate separator between multiple plans
 */
void
ExplainSeparatePlans(ExplainState *es)
{
	switch (es->format)
	{
		case EXPLAIN_FORMAT_TEXT:
			/* add a blank line */
			appendStringInfoChar(es->str, '\n');
			break;

		case EXPLAIN_FORMAT_XML:
			/* nothing to do */
			break;

		case EXPLAIN_FORMAT_JSON:
			/* must have a comma between array elements */
			appendStringInfoChar(es->str, ',');
			break;
	}
}

/*
 * Emit opening or closing XML tag.
 *
 * "flags" must contain X_OPENING, X_CLOSING, or X_CLOSE_IMMEDIATE.
 * Optionally, OR in X_NOWHITESPACE to suppress the whitespace we'd normally
 * add.
 *
 * XML tag names can't contain white space, so we replace any spaces in
 * "tagname" with dashes.
 */
static void
ExplainXMLTag(const char *tagname, int flags, ExplainState *es)
{
	const char *s;

	if ((flags & X_NOWHITESPACE) == 0)
		appendStringInfoSpaces(es->str, 2 * es->indent);
	appendStringInfoCharMacro(es->str, '<');
	if ((flags & X_CLOSING) != 0)
		appendStringInfoCharMacro(es->str, '/');
	for (s = tagname; *s; s++)
		appendStringInfoCharMacro(es->str, (*s == ' ') ? '-' : *s);
	if ((flags & X_CLOSE_IMMEDIATE) != 0)
		appendStringInfoString(es->str, " /");
	appendStringInfoCharMacro(es->str, '>');
	if ((flags & X_NOWHITESPACE) == 0)
		appendStringInfoCharMacro(es->str, '\n');
}

/*
 * Emit a JSON line ending.
 *
 * JSON requires a comma after each property but the last.  To facilitate this,
 * in JSON format, the text emitted for each property begins just prior to the
 * preceding line-break (and comma, if applicable).
 */
static void
ExplainJSONLineEnding(ExplainState *es)
{
	Assert(es->format == EXPLAIN_FORMAT_JSON);
	if (linitial_int(es->grouping_stack) != 0)
		appendStringInfoChar(es->str, ',');
	else
		linitial_int(es->grouping_stack) = 1;
	appendStringInfoChar(es->str, '\n');
}
#ifdef REMOVEME
/*
 * Produce a JSON string literal, properly escaping characters in the text.
 */
static void
escape_json(StringInfo buf, const char *str)
{
	const char *p;

	appendStringInfoCharMacro(buf, '\"');
	for (p = str; *p; p++)
	{
		switch (*p)
		{
			case '\b':
				appendStringInfoString(buf, "\\b");
				break;
			case '\f':
				appendStringInfoString(buf, "\\f");
				break;
			case '\n':
				appendStringInfoString(buf, "\\n");
				break;
			case '\r':
				appendStringInfoString(buf, "\\r");
				break;
			case '\t':
				appendStringInfoString(buf, "\\t");
				break;
			case '"':
				appendStringInfoString(buf, "\\\"");
				break;
			case '\\':
				appendStringInfoString(buf, "\\\\");
				break;
			default:
				if ((unsigned char) *p < ' ')
					appendStringInfo(buf, "\\u%04x", (int) *p);
				else
					appendStringInfoCharMacro(buf, *p);
				break;
		}
	}
	appendStringInfoCharMacro(buf, '\"');
}
#endif /* REMOVEME */
