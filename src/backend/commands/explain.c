/*-------------------------------------------------------------------------
 *
 * explain.c
 *	  Explain query execution plans
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994-5, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/commands/explain.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/xact.h"
#include "catalog/pg_type.h"
#include "cdb/cdbpartition.h"
#include "commands/createas.h"
#include "commands/defrem.h"
#include "commands/prepare.h"
#include "commands/queue.h"
#include "executor/execUtils.h"
#include "executor/hashjoin.h"
#include "foreign/fdwapi.h"
#include "optimizer/clauses.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteHandler.h"
#include "storage/bufmgr.h"
#include "tcop/tcopprot.h"
#include "utils/builtins.h"
#include "utils/json.h"
#include "utils/lsyscache.h"
#include "utils/metrics_utils.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "utils/tuplesort.h"
#include "utils/xml.h"

#include "cdb/cdbgang.h"
#include "executor/execDynamicScan.h"

#ifdef USE_ORCA
extern char *SerializeDXLPlan(Query *parse);
#endif


/* Crude hack to avoid changing sizeof(ExplainState) in released branches */
#define grouping_stack extra->groupingstack
#define deparse_cxt extra->deparsecxt

/* Hook for plugins to get control in ExplainOneQuery() */
ExplainOneQuery_hook_type ExplainOneQuery_hook = NULL;

/* Hook for plugins to get control in explain_get_index_name() */
explain_get_index_name_hook_type explain_get_index_name_hook = NULL;


/* OR-able flags for ExplainXMLTag() */
#define X_OPENING 0
#define X_CLOSING 1
#define X_CLOSE_IMMEDIATE 2
#define X_NOWHITESPACE 4

static void ExplainOneQuery(Query *query, IntoClause *into, ExplainState *es,
				const char *queryString, ParamListInfo params);
static void report_triggers(ResultRelInfo *rInfo, bool show_relname,
				ExplainState *es);

#ifdef USE_ORCA
static void ExplainDXL(Query *query, ExplainState *es,
							const char *queryString,
							ParamListInfo params);
#endif
static double elapsed_time(instr_time *starttime);
static void ExplainPreScanNode(PlanState *planstate, Bitmapset **rels_used);
static void ExplainPreScanMemberNodes(List *plans, PlanState **planstates,
						  Bitmapset **rels_used);
static void ExplainPreScanSubPlans(List *plans, Bitmapset **rels_used);
static void ExplainNode(PlanState *planstate, List *ancestors,
			const char *relationship, const char *plan_name,
			ExplainState *es);
static void show_plan_tlist(PlanState *planstate, List *ancestors,
				ExplainState *es);
static void show_expression(Node *node, const char *qlabel,
				PlanState *planstate, List *ancestors,
				bool useprefix, ExplainState *es);
static void show_qual(List *qual, const char *qlabel,
		  PlanState *planstate, List *ancestors,
		  bool useprefix, ExplainState *es);
static void show_scan_qual(List *qual, const char *qlabel,
			   PlanState *planstate, List *ancestors,
			   ExplainState *es);
static void show_upper_qual(List *qual, const char *qlabel,
				PlanState *planstate, List *ancestors,
				ExplainState *es);
static void show_sort_keys(SortState *sortstate, List *ancestors,
			   ExplainState *es);
static void show_merge_append_keys(MergeAppendState *mstate, List *ancestors,
					   ExplainState *es);
static void show_agg_keys(AggState *astate, List *ancestors,
			  ExplainState *es);
static void show_sort_group_keys(PlanState *planstate, const char *qlabel,
					 int nkeys, AttrNumber *keycols,
					 List *ancestors, ExplainState *es);
static void show_sort_info(SortState *sortstate, ExplainState *es);
static void show_windowagg_keys(WindowAggState *waggstate, List *ancestors, ExplainState *es);
static void show_hash_info(HashState *hashstate, ExplainState *es);
static void show_tidbitmap_info(BitmapHeapScanState *planstate,
					ExplainState *es);
static void show_instrumentation_count(const char *qlabel, int which,
						   PlanState *planstate, ExplainState *es);
static void show_foreignscan_info(ForeignScanState *fsstate, ExplainState *es);
static const char *explain_get_index_name(Oid indexId);
static void ExplainIndexScanDetails(Oid indexid, ScanDirection indexorderdir,
						ExplainState *es);
static void ExplainScanTarget(Scan *plan, ExplainState *es);
static void ExplainModifyTarget(ModifyTable *plan, ExplainState *es);
static void ExplainTargetRel(Plan *plan, Index rti, ExplainState *es);
static void show_modifytable_info(ModifyTableState *mtstate, ExplainState *es);
static void ExplainMemberNodes(List *plans, PlanState **planstates,
				   List *ancestors, ExplainState *es);
static void ExplainSubPlans(List *plans, List *ancestors,
				const char *relationship, ExplainState *es, SliceTable *sliceTable);
static void ExplainProperty(const char *qlabel, const char *value,
				bool numeric, ExplainState *es);
static void ExplainPropertyStringInfo(const char *qlabel, ExplainState *es,
									  const char *fmt,...)
									  __attribute__((format(PG_PRINTF_ATTRIBUTE, 3, 4)));
static void ExplainDummyGroup(const char *objtype, const char *labelname,
				  ExplainState *es);
static void ExplainXMLTag(const char *tagname, int flags, ExplainState *es);
static void ExplainJSONLineEnding(ExplainState *es);
static void ExplainYAMLLineStarting(ExplainState *es);
static void escape_yaml(StringInfo buf, const char *str);

/* Include the Greenplum EXPLAIN extensions */
#include "explain_gp.c"


/*
 * ExplainQuery -
 *	  execute an EXPLAIN command
 */
void
ExplainQuery(ExplainStmt *stmt, const char *queryString,
			 ParamListInfo params, DestReceiver *dest)
{
	ExplainState es;
	TupOutputState *tstate;
	List	   *rewritten;
	ListCell   *lc;
	bool		timing_set = false;

	/* Initialize ExplainState. */
	ExplainInitState(&es);

	/* Parse options list. */
	foreach(lc, stmt->options)
	{
		DefElem    *opt = (DefElem *) lfirst(lc);

		if (strcmp(opt->defname, "analyze") == 0)
			es.analyze = defGetBoolean(opt);
		else if (strcmp(opt->defname, "verbose") == 0)
			es.verbose = defGetBoolean(opt);
		else if (strcmp(opt->defname, "costs") == 0)
			es.costs = defGetBoolean(opt);
		else if (strcmp(opt->defname, "buffers") == 0)
			es.buffers = defGetBoolean(opt);
		else if (strcmp(opt->defname, "timing") == 0)
		{
			timing_set = true;
			es.timing = defGetBoolean(opt);
		}
		else if (strcmp(opt->defname, "format") == 0)
		{
			char	   *p = defGetString(opt);

			if (strcmp(p, "text") == 0)
				es.format = EXPLAIN_FORMAT_TEXT;
			else if (strcmp(p, "xml") == 0)
				es.format = EXPLAIN_FORMAT_XML;
			else if (strcmp(p, "json") == 0)
				es.format = EXPLAIN_FORMAT_JSON;
			else if (strcmp(p, "yaml") == 0)
				es.format = EXPLAIN_FORMAT_YAML;
			else
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("unrecognized value for EXPLAIN option \"%s\": \"%s\"",
					   opt->defname, p)));
		}
		else if (strcmp(opt->defname, "dxl") == 0)
			es.dxl = defGetBoolean(opt);
		else
			ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("unrecognized EXPLAIN option \"%s\"",
							opt->defname)));
	}

	if (es.buffers && !es.analyze)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("EXPLAIN option BUFFERS requires ANALYZE")));

	/* if the timing was not set explicitly, set default value */
	es.timing = (timing_set) ? es.timing : es.analyze;

	/* check that timing is used with EXPLAIN ANALYZE */
	if (es.timing && !es.analyze)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("EXPLAIN option TIMING requires ANALYZE")));

	/* currently, summary option is not exposed to users; just set it */
	es.summary = es.analyze;

	/*
	 * Parse analysis was done already, but we still have to run the rule
	 * rewriter.  We do not do AcquireRewriteLocks: we assume the query either
	 * came straight from the parser, or suitable locks were acquired by
	 * plancache.c.
	 *
	 * Because the rewriter and planner tend to scribble on the input, we make
	 * a preliminary copy of the source querytree.  This prevents problems in
	 * the case that the EXPLAIN is in a portal or plpgsql function and is
	 * executed repeatedly.  (See also the same hack in DECLARE CURSOR and
	 * PREPARE.)  XXX FIXME someday.
	 */
	Assert(IsA(stmt->query, Query));
	rewritten = QueryRewrite((Query *) copyObject(stmt->query));

	/* emit opening boilerplate */
	ExplainBeginOutput(&es);

	if (rewritten == NIL)
	{
		/*
		 * In the case of an INSTEAD NOTHING, tell at least that.  But in
		 * non-text format, the output is delimited, so this isn't necessary.
		 */
		if (es.format == EXPLAIN_FORMAT_TEXT)
			appendStringInfoString(es.str, "Query rewrites to nothing\n");
	}
	else
	{
		ListCell   *l;

		/* Explain every plan */
		foreach(l, rewritten)
		{
			ExplainOneQuery((Query *) lfirst(l), NULL, &es,
							queryString, params);

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

/*
 * Initialize ExplainState.
 */
void
ExplainInitState(ExplainState *es)
{
	/* Set default options. */
	memset(es, 0, sizeof(ExplainState));
	es->costs = true;
	/* Prepare output buffer. */
	es->str = makeStringInfo();
	/* Kluge to avoid changing sizeof(ExplainState) in released branches. */
	es->extra = (ExplainStateExtra *) palloc0(sizeof(ExplainStateExtra));
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
	Oid			result_type = TEXTOID;

	/* Check for XML format option */
	foreach(lc, stmt->options)
	{
		DefElem    *opt = (DefElem *) lfirst(lc);

		if (strcmp(opt->defname, "format") == 0)
		{
			char	   *p = defGetString(opt);

			if (strcmp(p, "xml") == 0)
				result_type = XMLOID;
			else if (strcmp(p, "json") == 0)
				result_type = JSONOID;
			else
				result_type = TEXTOID;
			/* don't "break", as ExplainQuery will use the last value */
		}
	}

	/* Need a tuple descriptor representing a single TEXT or XML column */
	tupdesc = CreateTemplateTupleDesc(1, false);
	TupleDescInitEntry(tupdesc, (AttrNumber) 1, "QUERY PLAN",
					   result_type, -1, 0);
	return tupdesc;
}

#ifdef USE_ORCA
/*
 * ExplainDXL -
 *	  print out the execution plan for one Query in DXL format
 *	  this function implicitly uses optimizer
 */
static void
ExplainDXL(Query *query, ExplainState *es, const char *queryString,
				ParamListInfo params)
{
	MemoryContext oldcxt = CurrentMemoryContext;
	bool		save_enumerate;
	char	   *dxl = NULL;

	save_enumerate = optimizer_enumerate_plans;

	/* Do the EXPLAIN. */

	/* enable plan enumeration before calling optimizer */
	optimizer_enumerate_plans = true;

	/* optimize query using optimizer and get generated plan in DXL format */
	dxl = SerializeDXLPlan(query);

	/* restore old value of enumerate plans GUC */
	optimizer_enumerate_plans = save_enumerate;

	if (dxl == NULL)
		elog(NOTICE, "Optimizer failed to produce plan");
	else
	{
		appendStringInfoString(es->str, dxl);
		appendStringInfoChar(es->str, '\n'); /* separator line */
		pfree(dxl);
	}

	/* Free the memory we used. */
	MemoryContextSwitchTo(oldcxt);
}
#endif

/*
 * ExplainOneQuery -
 *	  print out the execution plan for one Query
 *
 * "into" is NULL unless we are explaining the contents of a CreateTableAsStmt.
 */
static void
ExplainOneQuery(Query *query, IntoClause *into, ExplainState *es,
				const char *queryString, ParamListInfo params)
{
#ifdef USE_ORCA
	if (es->dxl)
	{
		ExplainDXL(query, es, queryString, params);
		return;
	}
#endif

	/* planner will not cope with utility statements */
	if (query->commandType == CMD_UTILITY)
	{
		ExplainOneUtility(query->utilityStmt, into, es, queryString, params);
		return;
	}

	/* if an advisor plugin is present, let it manage things */
	if (ExplainOneQuery_hook)
		(*ExplainOneQuery_hook) (query, into, es, queryString, params);
	else
	{
		PlannedStmt *plan;
		instr_time	planstart,
					planduration;

		INSTR_TIME_SET_CURRENT(planstart);

		/* plan the query */
		plan = pg_plan_query(query, 0, params);

		INSTR_TIME_SET_CURRENT(planduration);
		INSTR_TIME_SUBTRACT(planduration, planstart);

		/*
		 * GPDB_92_MERGE_FIXME: it really should be an optimizer's responsibility
		 * to correctly set the into-clause and into-policy of the PlannedStmt.
		 */
		if (into != NULL)
			plan->intoClause = copyObject(into);

		/* run it (if needed) and produce output */
		ExplainOnePlan(plan, into, es, queryString, params, &planduration, 0);
	}
}

/*
 * ExplainOneUtility -
 *	  print out the execution plan for one utility statement
 *	  (In general, utility statements don't have plans, but there are some
 *	  we treat as special cases)
 *
 * "into" is NULL unless we are explaining the contents of a CreateTableAsStmt.
 *
 * This is exported because it's called back from prepare.c in the
 * EXPLAIN EXECUTE case.
 */
void
ExplainOneUtility(Node *utilityStmt, IntoClause *into, ExplainState *es,
				  const char *queryString, ParamListInfo params)
{
	if (utilityStmt == NULL)
		return;

	if (IsA(utilityStmt, CreateTableAsStmt))
	{
		/*
		 * We have to rewrite the contained SELECT and then pass it back to
		 * ExplainOneQuery.  It's probably not really necessary to copy the
		 * contained parsetree another time, but let's be safe.
		 */
		CreateTableAsStmt *ctas = (CreateTableAsStmt *) utilityStmt;
		List	   *rewritten;

		Assert(IsA(ctas->query, Query));
		rewritten = QueryRewrite((Query *) copyObject(ctas->query));
		Assert(list_length(rewritten) == 1);
		ExplainOneQuery((Query *) linitial(rewritten), ctas->into, es,
						queryString, params);
	}
	else if (IsA(utilityStmt, ExecuteStmt))
		ExplainExecuteQuery((ExecuteStmt *) utilityStmt, into, es,
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
							  "Utility statements have no plan structure\n");
		else
			ExplainDummyGroup("Utility Statement", NULL, es);
	}
}

/*
 * ExplainOnePlan -
 *		given a planned query, execute it if needed, and then print
 *		EXPLAIN output
 *
 * "into" is NULL unless we are explaining the contents of a CreateTableAsStmt,
 * in which case executing the query should result in creating that table.
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
ExplainOnePlan(PlannedStmt *plannedstmt, IntoClause *into, ExplainState *es,
			   const char *queryString, ParamListInfo params,
			   const instr_time *planduration, int cursorOptions)
{
	DestReceiver *dest;
	QueryDesc  *queryDesc;
	instr_time	starttime;
	double		totaltime = 0;
	int			eflags;
	int			instrument_option = 0;

	if (es->analyze && es->timing)
		instrument_option |= INSTRUMENT_TIMER;
	else if (es->analyze)
		instrument_option |= INSTRUMENT_ROWS;

	if (es->buffers)
		instrument_option |= INSTRUMENT_BUFFERS;

	if (es->analyze)
		instrument_option |= INSTRUMENT_CDB;

	/*
	 * We always collect timing for the entire statement, even when node-level
	 * timing is off, so we don't look at es->timing here.  (We could skip
	 * this if !es->summary, but it's hardly worth the complication.)
	 */
	INSTR_TIME_SET_CURRENT(starttime);

	/*
	 * Use a snapshot with an updated command ID to ensure this query sees
	 * results of any previously executed queries.
	 */
	PushCopiedSnapshot(GetActiveSnapshot());
	UpdateActiveSnapshotCommandId();

	/*
	 * Normally we discard the query's output, but if explaining CREATE TABLE
	 * AS, we'd better use the appropriate tuple receiver.
	 */
	if (into)
		dest = CreateIntoRelDestReceiver(into);
	else
		dest = None_Receiver;

	/* Create a QueryDesc for the query */
	queryDesc = CreateQueryDesc(plannedstmt, queryString,
								GetActiveSnapshot(), InvalidSnapshot,
								dest, params, instrument_option);

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

	/* GPDB hook for collecting query info */
	if (query_info_collect_hook)
		(*query_info_collect_hook)(METRICS_QUERY_SUBMIT, queryDesc);

    /* Allocate workarea for summary stats. */
    if (es->analyze)
    {
        /* Attach workarea to QueryDesc so ExecSetParamPlan() can find it. */
        queryDesc->showstatctx = cdbexplain_showExecStatsBegin(queryDesc,
															   starttime);
    }
	else
		queryDesc->showstatctx = NULL;

	/* Select execution options */
	if (es->analyze)
		eflags = 0;				/* default run-to-completion flags */
	else
		eflags = EXEC_FLAG_EXPLAIN_ONLY;
	if (into)
		eflags |= GetIntoRelEFlags(into);

	queryDesc->plannedstmt->query_mem =
		ResourceManagerGetQueryMemoryLimit(queryDesc->plannedstmt);

	/* call ExecutorStart to prepare the plan for execution */
	ExecutorStart(queryDesc, eflags);

	/* Execute the plan for statistics if asked for */
	if (es->analyze)
	{
		ScanDirection dir;

		/* EXPLAIN ANALYZE CREATE TABLE AS WITH NO DATA is weird */
		if (into && into->skipData)
			dir = NoMovementScanDirection;
		else
			dir = ForwardScanDirection;

		/* run the plan */
		ExecutorRun(queryDesc, dir, 0L);

		/* Wait for completion of all qExec processes. */
		if (queryDesc->estate->dispatcherState && queryDesc->estate->dispatcherState->primaryResults)
		{
			cdbdisp_checkDispatchResult(queryDesc->estate->dispatcherState, DISPATCH_WAIT_NONE);
			/*
			 * If some QE throw errors, we might not receive stats from QEs,
			 * In ExecutorEnd we will reThrow QE's error, In this situation,
			 * there is no need to execute ExplainPrintPlan. reThrow error in advance.
			 */
			ErrorData  *qeError = NULL;
			cdbdisp_getDispatchResults(queryDesc->estate->dispatcherState, &qeError);
			if (qeError)
			{
				FlushErrorState();
				ReThrowError(qeError);
			}
		}

		/* run cleanup too */
		ExecutorFinish(queryDesc);

		/* We can't run ExecutorEnd 'till we're done printing the stats... */
		totaltime += elapsed_time(&starttime);
	}

	ExplainOpenGroup("Query", NULL, true, es);

	/* Create textual dump of plan tree */
	ExplainPrintPlan(es, queryDesc);

	if (cursorOptions & CURSOR_OPT_PARALLEL_RETRIEVE)
		ExplainParallelRetrieveCursor(es, queryDesc);

	if (es->summary && planduration)
	{
		double		plantime = INSTR_TIME_GET_DOUBLE(*planduration);

		if (es->format == EXPLAIN_FORMAT_TEXT)
			appendStringInfo(es->str, "Planning time: %.3f ms\n",
							 1000.0 * plantime);
		else
			ExplainPropertyFloat("Planning Time", 1000.0 * plantime, 3, es);
	}

	/* Print info about runtime of triggers */
	if (es->analyze)
		ExplainPrintTriggers(es, queryDesc);

	/*
	 * Display per-slice and whole-query statistics.
	 */
	if (es->analyze)
		cdbexplain_showExecStatsEnd(queryDesc->plannedstmt, queryDesc->showstatctx,
									queryDesc->estate, es);

	/*
	 * Show non-default GUC settings that might have affected the plan as well
	 * as optimizer settings etc.
	 */
	ExplainOpenGroup("Settings", "Settings", true, es);

	if (queryDesc->plannedstmt->planGen == PLANGEN_PLANNER)
		ExplainProperty("Optimizer", "Postgres query optimizer", false, es);
#ifdef USE_ORCA
	else
		ExplainPropertyStringInfo("Optimizer", es, "Pivotal Optimizer (GPORCA)");
#endif

	/* We only list the non-default GUCs in verbose mode */
	if (es->verbose)
	{
		List	*settings;

		settings = gp_guc_list_show(PGC_S_DEFAULT, gp_guc_list_for_explain);

		if (list_length(settings) > 0)
			ExplainPropertyList("Settings", settings, es);
	}

	ExplainCloseGroup("Settings", "Settings", true, es);

	/*
	 * Close down the query and free resources.  Include time for this in the
	 * total execution time (although it should be pretty minimal).
	 */
	INSTR_TIME_SET_CURRENT(starttime);

	ExecutorEnd(queryDesc);

	FreeQueryDesc(queryDesc);

	PopActiveSnapshot();

	/* We need a CCI just in case query expanded to multiple plans */
	if (es->analyze)
		CommandCounterIncrement();

	totaltime += elapsed_time(&starttime);

	if (es->summary)
	{
		if (es->format == EXPLAIN_FORMAT_TEXT)
			appendStringInfo(es->str, "Execution time: %.3f ms\n",
							 1000.0 * totaltime);
		else
			ExplainPropertyFloat("Execution Time", 1000.0 * totaltime,
								 3, es);
	}

	ExplainCloseGroup("Query", NULL, true, es);
}

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
	EState     *estate = queryDesc->estate;
	Bitmapset  *rels_used = NULL;

	Assert(queryDesc->plannedstmt != NULL);
	es->pstmt = queryDesc->plannedstmt;
	es->rtable = queryDesc->plannedstmt->rtable;
	es->showstatctx = queryDesc->showstatctx;

	/* CDB: Find slice table entry for the root slice. */
	es->currentSlice = getCurrentSlice(estate, LocallyExecutingSliceIndex(estate));

	/*
	 * Get local stats if root slice was executed here in the qDisp, as long
	 * as we haven't already gathered the statistics. This can happen when an
	 * executor hook generates EXPLAIN output.
	 */
	if (es->analyze && !es->showstatctx->stats_gathered)
	{
		if (Gp_role != GP_ROLE_EXECUTE && (!es->currentSlice || sliceRunsOnQD(es->currentSlice)))
			cdbexplain_localExecStats(queryDesc->planstate, es->showstatctx);

        /* Fill in the plan's Instrumentation with stats from qExecs. */
        if (estate->dispatcherState && estate->dispatcherState->primaryResults)
            cdbexplain_recvExecStats(queryDesc->planstate,
                                     estate->dispatcherState->primaryResults,
                                     LocallyExecutingSliceIndex(estate),
                                     es->showstatctx);
	}

	ExplainPreScanNode(queryDesc->planstate, &rels_used);
	es->rtable_names = select_rtable_names_for_explain(es->rtable, rels_used);
	es->deparse_cxt = deparse_context_for_plan_rtable(es->rtable,
													  es->rtable_names);
	ExplainNode(queryDesc->planstate, NIL, NULL, NULL, es);
}

/*
 * ExplainPrintTriggers -

 *	  convert a QueryDesc's trigger statistics to text and append it to
 *	  es->str
 *
 * The caller should have set up the options fields of *es, as well as
 * initializing the output buffer es->str.  Other fields in *es are
 * initialized here.
 */
void
ExplainPrintTriggers(ExplainState *es, QueryDesc *queryDesc)
{
	ResultRelInfo *rInfo;
	bool		show_relname;
	int			numrels = queryDesc->estate->es_num_result_relations;
	List	   *targrels = queryDesc->estate->es_trig_target_relations;
	int			nr;
	ListCell   *l;

	/*
	 * GPDB_91_MERGE_FIXME: If the target is a partitioned table, we
	 * should also report information on the triggers in the partitions.
	 * I.e. we should scan the 'ri_partition_hash' of each
	 * ResultRelInfo as well. This is somewhat academic, though, as long
	 * as we don't support triggers in GPDB in general..
	 */

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
 * ExplainQueryText -
 *	  add a "Query Text" node that contains the actual text of the query
 *
 * The caller should have set up the options fields of *es, as well as
 * initializing the output buffer es->str.
 *
 */
void
ExplainQueryText(ExplainState *es, QueryDesc *queryDesc)
{
	if (queryDesc->sourceText)
		ExplainPropertyText("Query Text", queryDesc->sourceText, es);
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
		 * constraint name unless VERBOSE is specified.  In non-text formats
		 * we just print everything.
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
			if (es->timing)
				appendStringInfo(es->str, ": time=%.3f calls=%.ld\n",
								 1000.0 * instr->total, instr->ntuples);
			else
				appendStringInfo(es->str, ": calls=%.ld\n", instr->ntuples);
		}
		else
		{
			ExplainPropertyText("Trigger Name", trig->tgname, es);
			if (conname)
				ExplainPropertyText("Constraint Name", conname, es);
			ExplainPropertyText("Relation", relname, es);
			if (es->timing)
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

static void
show_dispatch_info(Slice *slice, ExplainState *es, Plan *plan)
{
	int			segments;

	/*
	 * In non-parallel query, there is no slice information.
	 */
	if (!slice)
		return;

	switch (slice->gangType)
	{
		case GANGTYPE_UNALLOCATED:
		case GANGTYPE_ENTRYDB_READER:
			segments = 0;
			break;

		case GANGTYPE_PRIMARY_WRITER:
		case GANGTYPE_PRIMARY_READER:
		case GANGTYPE_SINGLETON_READER:
		{
			if (slice->directDispatch.isDirectDispatch)
			{
				segments = list_length(slice->directDispatch.contentIds);
			}
			else if (es->pstmt->planGen == PLANGEN_PLANNER)
			{
				/*
				 * - for motion nodes we want to display the sender segments
				 *   count, it can be fetched from lefttree;
				 * - for non-motion nodes the segments count can be fetched
				 *   from either lefttree or plan itself, they should be the
				 *   same;
				 * - there are also nodes like Hash that might have NULL
				 *   plan->flow but non-NULL lefttree->flow, so we can use
				 *   whichever that's available.
				 */
				Plan	   *fplan;

				fplan = plan;

				for (;;)
				{
					if (IsA(fplan, Motion))
					{
						Assert(fplan->lefttree);
						fplan = fplan->lefttree;
						continue;
					}

					if (fplan->flow)
						break;

					/* No flow on this node. Dig into child. */
					if (fplan->lefttree)
					{
						fplan = fplan->lefttree;
						continue;
					}
					if (IsA(fplan, Append))
					{
						Append	   *aplan = (Append *) fplan;

						if (aplan->appendplans)
						{
							fplan = (Plan *) linitial(aplan->appendplans);
							continue;
						}
					}

					/*
					 * No flow, and no subplan. Shouldn't happen, but let's not
					 * panic if it does.
					 *
					 * This shouldn't happen, but just in case the planner
					 * failed to decorate a node with a flow, don't panic
					 * in production. Not all nodes need to be marked with a
					 * flow, as long as the Motions, and the nodes immediately
					 * below a Motion are, so this just leads to incorrect
					 * information in EXPLAIN output. We'd still like to fix
					 * such cases, though so Assert so that we catch them
					 * during development.
					 */
					elog(WARNING, "could not find flow for node of type %d", fplan->type);
					break;
				}

				if (!fplan->flow)
				{
					/* no flow and no subplan; shouldn't happen */
					segments = 1;
				}
				else if (fplan->flow->flotype == FLOW_SINGLETON)
					segments = 1;
				else
					segments = fplan->flow->numsegments;
			}
			else
			{
				segments = slice->gangSize;
			}
			break;
		}

		default:
			segments = 0;		/* keep compiler happy */
			Assert(false);
			break;
	}

	if (es->format == EXPLAIN_FORMAT_TEXT)
	{
		if (segments == 0)
			appendStringInfo(es->str, "  (slice%d)", slice->sliceIndex);
		else if (slice->primaryGang && gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
			/*
			 * In gpdb 5 there was a unique gang_id for each gang, this was
			 * retired since gpdb 6, so we use the qe identifier from the first
			 * segment of the gang to identify each gang.
			 */
			appendStringInfo(es->str, "  (slice%d; gang%d; segments: %d)",
							 slice->sliceIndex,
							 slice->primaryGang->db_descriptors[0]->identifier,
							 segments);
		else
			appendStringInfo(es->str, "  (slice%d; segments: %d)",
							 slice->sliceIndex, segments);
	}
	else
	{
		ExplainPropertyInteger("Slice", slice->sliceIndex, es);
		if (slice->primaryGang && gp_log_gang >= GPVARS_VERBOSITY_DEBUG)
			ExplainPropertyInteger("Gang", slice->primaryGang->db_descriptors[0]->identifier, es);
		ExplainPropertyInteger("Segments", segments, es);
		ExplainPropertyText("Gang Type", gangTypeToString(slice->gangType), es);
	}
}

/*
 * ExplainPreScanNode -
 *	  Prescan the planstate tree to identify which RTEs are referenced
 *
 * Adds the relid of each referenced RTE to *rels_used.  The result controls
 * which RTEs are assigned aliases by select_rtable_names_for_explain.
 * This ensures that we don't confusingly assign un-suffixed aliases to RTEs
 * that never appear in the EXPLAIN output (such as inheritance parents).
 */
static void
ExplainPreScanNode(PlanState *planstate, Bitmapset **rels_used)
{
	Plan	   *plan = planstate->plan;

	switch (nodeTag(plan))
	{
		case T_SeqScan:
		case T_DynamicSeqScan:
		case T_ExternalScan:
		case T_DynamicIndexScan:
		case T_ShareInputScan:
		case T_IndexScan:
		case T_IndexOnlyScan:
		case T_BitmapHeapScan:
		case T_DynamicBitmapHeapScan:
		case T_TidScan:
		case T_SubqueryScan:
		case T_FunctionScan:
		case T_ValuesScan:
		case T_CteScan:
		case T_WorkTableScan:
		case T_ForeignScan:
			*rels_used = bms_add_member(*rels_used,
										((Scan *) plan)->scanrelid);
			break;
		case T_ModifyTable:
			/* cf ExplainModifyTarget */
			*rels_used = bms_add_member(*rels_used,
					  linitial_int(((ModifyTable *) plan)->resultRelations));
			break;
		default:
			break;
	}

	/* initPlan-s */
	if (planstate->initPlan)
		ExplainPreScanSubPlans(planstate->initPlan, rels_used);

	/* lefttree */
	if (outerPlanState(planstate))
		ExplainPreScanNode(outerPlanState(planstate), rels_used);

	/* righttree */
	if (innerPlanState(planstate))
		ExplainPreScanNode(innerPlanState(planstate), rels_used);

	/* special child plans */
	switch (nodeTag(plan))
	{
		case T_ModifyTable:
			ExplainPreScanMemberNodes(((ModifyTable *) plan)->plans,
								  ((ModifyTableState *) planstate)->mt_plans,
									  rels_used);
			break;
		case T_Append:
			ExplainPreScanMemberNodes(((Append *) plan)->appendplans,
									((AppendState *) planstate)->appendplans,
									  rels_used);
			break;
		case T_MergeAppend:
			ExplainPreScanMemberNodes(((MergeAppend *) plan)->mergeplans,
								((MergeAppendState *) planstate)->mergeplans,
									  rels_used);
			break;
		case T_Sequence:
			ExplainPreScanMemberNodes(((Sequence *) plan)->subplans,
									((SequenceState *) planstate)->subplans,
									  rels_used);
			break;
		case T_BitmapAnd:
			ExplainPreScanMemberNodes(((BitmapAnd *) plan)->bitmapplans,
								 ((BitmapAndState *) planstate)->bitmapplans,
									  rels_used);
			break;
		case T_BitmapOr:
			ExplainPreScanMemberNodes(((BitmapOr *) plan)->bitmapplans,
								  ((BitmapOrState *) planstate)->bitmapplans,
									  rels_used);
			break;
		case T_SubqueryScan:
			ExplainPreScanNode(((SubqueryScanState *) planstate)->subplan,
							   rels_used);
			break;
		default:
			break;
	}

	/* subPlan-s */
	if (planstate->subPlan)
		ExplainPreScanSubPlans(planstate->subPlan, rels_used);
}

/*
 * Prescan the constituent plans of a ModifyTable, Append, MergeAppend,
 * BitmapAnd, or BitmapOr node.
 *
 * Note: we don't actually need to examine the Plan list members, but
 * we need the list in order to determine the length of the PlanState array.
 */
static void
ExplainPreScanMemberNodes(List *plans, PlanState **planstates,
						  Bitmapset **rels_used)
{
	int			nplans = list_length(plans);
	int			j;

	for (j = 0; j < nplans; j++)
		ExplainPreScanNode(planstates[j], rels_used);
}

/*
 * Prescan a list of SubPlans (or initPlans, which also use SubPlan nodes).
 */
static void
ExplainPreScanSubPlans(List *plans, Bitmapset **rels_used)
{
	ListCell   *lst;

	foreach(lst, plans)
	{
		SubPlanState *sps = (SubPlanState *) lfirst(lst);

		ExplainPreScanNode(sps->planstate, rels_used);
	}
}

/*
 * ExplainNode -
 *	  Appends a description of a plan tree to es->str
 *
 * planstate points to the executor state node for the current plan node.
 * We need to work from a PlanState node, not just a Plan node, in order to
 * get at the instrumentation data (if any) as well as the list of subplans.
 *
 * ancestors is a list of parent PlanState nodes, most-closely-nested first.
 * These are needed in order to interpret PARAM_EXEC Params.
 *
 * relationship describes the relationship of this plan node to its parent
 * (eg, "Outer", "Inner"); it can be null at top level.  plan_name is an
 * optional name to be attached to the node.
 *
 * In text format, es->indent is controlled in this function since we only
 * want it to change at plan-node boundaries.  In non-text formats, es->indent
 * corresponds to the nesting depth of logical output groups, and therefore
 * is controlled by ExplainOpenGroup/ExplainCloseGroup.
 *
 * es->parentPlanState points to the parent planstate node and can be used by
 * PartitionSelector to deparse its printablePredicate. (This is passed in
 * ExplainState rather than as a normal argument, to avoid changing the
 * function signature from upstream.)
 */
static void
ExplainNode(PlanState *planstate, List *ancestors,
			const char *relationship, const char *plan_name,
			ExplainState *es)
{
	Plan	   *plan = planstate->plan;
	PlanState  *parentplanstate;
    Slice      *save_currentSlice = es->currentSlice;    /* save */
	const char *pname;			/* node type name for text output */
	const char *sname;			/* node type name for non-text output */
	const char *strategy = NULL;
	const char *operation = NULL;
	int			save_indent = es->indent;
	bool		haschildren;
	bool		skip_outer=false;
	char       *skip_outer_msg = NULL;
	int			motion_recv;
	int			motion_snd;
	float		scaleFactor = 1.0; /* we will divide planner estimates by this factor to produce
									  per-segment estimates */
	Slice		*parentSlice = NULL;

	/* Remember who called us. */
	parentplanstate = es->parentPlanState;
	es->parentPlanState = planstate;

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		/*
		 * Estimates will have to be scaled down to be per-segment (except in a
		 * few cases).
		 */
		if ((plan->directDispatch).isDirectDispatch)
		{
			scaleFactor = 1.0;
		}
		else if (plan->flow != NULL && CdbPathLocus_IsBottleneck(*(plan->flow)))
		{
			/*
			 * Data is unified in one place (singleQE or QD), or executed on a
			 * single segment.  We scale up estimates to make it global.  We
			 * will later amend this for Motion nodes.
			 */
			scaleFactor = 1.0;
		}
		else if (plan->flow != NULL && CdbPathLocus_IsSegmentGeneral(*(plan->flow)))
		{
			/* Replicated table has full data on every segment */
			scaleFactor = 1.0;
		}
		else if (plan->flow != NULL && es->pstmt->planGen == PLANGEN_PLANNER)
		{
			/*
			 * The plan node is executed on multiple nodes, so scale down the
			 * number of rows seen by each segment
			 */
			scaleFactor = CdbPathLocus_NumSegments(*(plan->flow));
		}
		else
		{
			/*
			 * The plan node is executed on multiple nodes, so scale down the
			 * number of rows seen by each segment
			 */
			scaleFactor = getgpsegmentCount();
		}
	}

	/*
	 * If this is a Motion node, we're descending into a new slice.
	 */
	if (IsA(plan, Motion))
	{
		Motion	   *pMotion = (Motion *) plan;
		SliceTable *sliceTable = planstate->state->es_sliceTable;

		if (sliceTable)
		{
			es->currentSlice = (Slice *) list_nth(sliceTable->slices,
												  pMotion->motionID);
			parentSlice = es->currentSlice->parentIndex == -1 ? NULL :
						  (Slice *) list_nth(sliceTable->slices,
											 es->currentSlice->parentIndex);
		}
	}

	switch (nodeTag(plan))
	{
		case T_Result:
			pname = sname = "Result";
			break;
		case T_ModifyTable:
			sname = "ModifyTable";
			switch (((ModifyTable *) plan)->operation)
			{
				case CMD_INSERT:
					pname = operation = "Insert";
					break;
				case CMD_UPDATE:
					pname = operation = "Update";
					break;
				case CMD_DELETE:
					pname = operation = "Delete";
					break;
				default:
					pname = "???";
					break;
			}
			break;
		case T_Repeat:
			pname = sname = "Repeat";
			break;
		case T_Append:
			pname = sname = "Append";
			break;
		case T_MergeAppend:
			pname = sname = "Merge Append";
			break;
		case T_RecursiveUnion:
			pname = sname = "Recursive Union";
			break;
		case T_Sequence:
			pname = sname = "Sequence";
			break;
		case T_BitmapAnd:
			pname = sname = "BitmapAnd";
			break;
		case T_BitmapOr:
			pname = sname = "BitmapOr";
			break;
		case T_NestLoop:
			pname = sname = "Nested Loop";
			if (((NestLoop *)plan)->shared_outer)
			{
				skip_outer = true;
				skip_outer_msg = "See first subplan of Hash Join";
			}
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
		case T_DynamicSeqScan:
			pname = sname = "Dynamic Seq Scan";
			break;
		case T_ExternalScan:
			pname = sname = "External Scan";
			break;
		case T_IndexScan:
			pname = sname = "Index Scan";
			break;
		case T_DynamicIndexScan:
			pname = sname = "Dynamic Index Scan";
			break;
		case T_IndexOnlyScan:
			pname = sname = "Index Only Scan";
			break;
		case T_BitmapIndexScan:
			pname = sname = "Bitmap Index Scan";
			break;
		case T_DynamicBitmapIndexScan:
			pname = sname = "Dynamic Bitmap Index Scan";
			break;
		case T_BitmapHeapScan:
			/*
			 * We print "Bitmap Heap Scan", even for AO tables. It's a bit
			 * confusing, but that's what the plan node is called, regardless
			 * of the table type.
			 */
			pname = sname = "Bitmap Heap Scan";
			break;
		case T_DynamicBitmapHeapScan:
			pname = sname = "Dynamic Bitmap Heap Scan";
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
		case T_CteScan:
			pname = sname = "CTE Scan";
			break;
		case T_WorkTableScan:
			pname = sname = "WorkTable Scan";
			break;
		case T_ForeignScan:
			pname = sname = "Foreign Scan";
		break;
		case T_ShareInputScan:
			pname = sname = "Shared Scan";
			break;
		case T_Material:
			pname = sname = "Materialize";
			break;
		case T_Sort:
			pname = sname = "Sort";
			break;
		case T_Agg:
			sname = "Aggregate";
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
		case T_WindowAgg:
			pname = sname = "WindowAgg";
			break;
		case T_TableFunctionScan:
			pname = sname = "Table Function Scan";
			break;
		case T_Unique:
			pname = sname = "Unique";
			break;
		case T_SetOp:
			sname = "SetOp";
			switch (((SetOp *) plan)->strategy)
			{
				case SETOP_SORTED:
					pname = "SetOp";
					strategy = "Sorted";
					break;
				case SETOP_HASHED:
					pname = "HashSetOp";
					strategy = "Hashed";
					break;
				default:
					pname = "SetOp ???";
					strategy = "???";
					break;
			}
			break;
		case T_LockRows:
			pname = sname = "LockRows";
			break;
		case T_Limit:
			pname = sname = "Limit";
			break;
		case T_Hash:
			pname = sname = "Hash";
			break;
		case T_Motion:
			{
				Motion		*pMotion = (Motion *) plan;

				Assert(plan->lefttree);
				Assert(plan->lefttree->flow);

				motion_snd = es->currentSlice->gangSize;
				motion_recv = (parentSlice == NULL ? 1 : parentSlice->gangSize);

				/* scale the number of rows by the number of segments sending data */
				scaleFactor = motion_snd;

				switch (pMotion->motionType)
				{
					case MOTIONTYPE_HASH:
						sname = "Redistribute Motion";
						break;
					case MOTIONTYPE_FIXED:
						if (pMotion->isBroadcast)
						{
							sname = "Broadcast Motion";
						}
						else if (plan->lefttree->flow->locustype == CdbLocusType_Replicated)
						{
							sname = "Explicit Gather Motion";
							scaleFactor = 1;
							motion_recv = 1;
						}
						else
						{
							sname = "Gather Motion";
							scaleFactor = 1;
							motion_recv = 1;
						}
						break;
					case MOTIONTYPE_EXPLICIT:
						sname = "Explicit Redistribute Motion";
						motion_recv = getgpsegmentCount();
						break;
					default:
						sname = "???";
						break;
				}

				if (es->pstmt->planGen == PLANGEN_PLANNER)
				{
					Slice	   *slice = es->currentSlice;

					if (slice->directDispatch.isDirectDispatch)
					{
						/* Special handling on direct dispatch */
						motion_snd = list_length(slice->directDispatch.contentIds);
					}
					else if (plan->lefttree->flow->flotype == FLOW_SINGLETON)
					{
						/* For SINGLETON we always display sender size as 1 */
						motion_snd = 1;
					}
					else
					{
						/* Otherwise find out sender size from outer plan */
						motion_snd = plan->lefttree->flow->numsegments;
					}

					if (pMotion->motionType == MOTIONTYPE_FIXED &&
						!pMotion->isBroadcast)
					{
						/* In Gather Motion always display receiver size as 1 */
						motion_recv = 1;
					}
					else
					{
						/* Otherwise find out receiver size from plan */
						motion_recv = plan->flow->numsegments;
					}
				}

				pname = psprintf("%s %d:%d", sname, motion_snd, motion_recv);
			}
			break;
		case T_DML:
			{
				sname = "DML";
				switch (es->pstmt->commandType)
				{
					case CMD_INSERT:
						pname = operation = "Insert";
						break;
					case CMD_DELETE:
						pname = operation = "Delete";
						break;
					case CMD_UPDATE:
						pname = operation = "Update";
						break;
					default:
						pname = operation = "DML ???";
						break;
				}
			}
			break;
		case T_SplitUpdate:
			pname = sname = "Split";
			break;
		case T_AssertOp:
			pname = sname = "Assert";
			break;
		case T_PartitionSelector:
			pname = sname = "Partition Selector";
			break;
		case T_RowTrigger:
			pname = sname = "RowTrigger";
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
			appendStringInfo(es->str, "%s", plan_name);

			/*
			 * Show slice information after the plan name.
			 *
			 * Note: If the top node was a Motion node, we print the slice
			 * *above* the Motion here. We will print the slice below the
			 * Motion, below.
			 */
			show_dispatch_info(save_currentSlice, es, plan);
			appendStringInfoChar(es->str, '\n');
			es->indent++;
		}
		if (es->indent)
		{
			appendStringInfoSpaces(es->str, es->indent * 2);
			appendStringInfoString(es->str, "->  ");
			es->indent += 2;
		}
		appendStringInfoString(es->str, pname);

		/*
		 * Print information about the current slice. In order to not make
		 * the output too verbose, only print it at the slice boundaries,
		 * ie. at Motion nodes. (We already switched the "current slice"
		 * to the slice below the Motion.)
		 */
		if (IsA(plan, Motion))
			show_dispatch_info(es->currentSlice, es, plan);

		es->indent++;
	}
	else
	{
		ExplainPropertyText("Node Type", sname, es);
		if (nodeTag(plan) == T_Motion)
		{
			ExplainPropertyInteger("Senders", motion_snd, es);
			ExplainPropertyInteger("Receivers", motion_recv, es);
		}
		if (strategy)
			ExplainPropertyText("Strategy", strategy, es);
		if (operation)
			ExplainPropertyText("Operation", operation, es);
		if (relationship)
			ExplainPropertyText("Parent Relationship", relationship, es);
		if (plan_name)
			ExplainPropertyText("Subplan Name", plan_name, es);

		show_dispatch_info(es->currentSlice, es, plan);
	}

	switch (nodeTag(plan))
	{
		case T_SeqScan:
		case T_DynamicSeqScan:
		case T_ExternalScan:
		case T_DynamicIndexScan:
		case T_BitmapHeapScan:
		case T_DynamicBitmapHeapScan:
		case T_TidScan:
		case T_SubqueryScan:
		case T_FunctionScan:
		case T_TableFunctionScan:
		case T_ValuesScan:
		case T_CteScan:
		case T_WorkTableScan:
		case T_ForeignScan:
			ExplainScanTarget((Scan *) plan, es);
			break;
		case T_IndexScan:
			{
				IndexScan  *indexscan = (IndexScan *) plan;

				ExplainIndexScanDetails(indexscan->indexid,
										indexscan->indexorderdir,
										es);
				ExplainScanTarget((Scan *) indexscan, es);
			}
			break;
		case T_IndexOnlyScan:
			{
				IndexOnlyScan *indexonlyscan = (IndexOnlyScan *) plan;

				ExplainIndexScanDetails(indexonlyscan->indexid,
										indexonlyscan->indexorderdir,
										es);
				ExplainScanTarget((Scan *) indexonlyscan, es);
			}
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
		case T_DynamicBitmapIndexScan:
			{
				BitmapIndexScan *bitmapindexscan = (BitmapIndexScan *) plan;
				Oid indexoid = bitmapindexscan->indexid;
				Oid parentOid = rel_partition_get_root(indexoid);
				while (parentOid != InvalidOid)
				{
					indexoid = parentOid;
					parentOid = rel_partition_get_root(indexoid);
				}

				const char *indexname =
				explain_get_index_name(indexoid);

				if (es->format == EXPLAIN_FORMAT_TEXT)
					appendStringInfo(es->str, " on %s", indexname);
				else
					ExplainPropertyText("Index Name", indexname, es);
			}
			break;
		case T_ModifyTable:
			ExplainModifyTarget((ModifyTable *) plan, es);
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
					case JOIN_SEMI:
						jointype = "Semi";
						break;
					case JOIN_ANTI:
						jointype = "Anti";
						break;
					case JOIN_LASJ_NOTIN:
						jointype = "Left Anti Semi (Not-In)";
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
						appendStringInfoString(es->str, " Join");
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
		case T_ShareInputScan:
			{
				ShareInputScan *sisc = (ShareInputScan *) plan;
				int				slice_id = -1;

				if (es->currentSlice)
					slice_id = es->currentSlice->sliceIndex;

				if (es->format == EXPLAIN_FORMAT_TEXT)
					appendStringInfo(es->str, " (share slice:id %d:%d)",
									 slice_id, sisc->share_id);
				else
				{
					ExplainPropertyInteger("Share ID", sisc->share_id, es);
					ExplainPropertyInteger("Slice ID", slice_id, es);
				}
			}
			break;
		case T_PartitionSelector:
			{
				PartitionSelector  *ps = (PartitionSelector *)plan;
				char			   *relname = get_rel_name(ps->relid);

				if (es->format == EXPLAIN_FORMAT_TEXT)
				{
					if (ps->scanId != 0)
						appendStringInfo(es->str, " for %s (dynamic scan id: %d)",
										 quote_identifier(relname),
										 ps->scanId);
					else
						appendStringInfo(es->str, " for %s", quote_identifier(relname));
				}
				else
				{
					ExplainPropertyText("Relation", relname, es);
					if (ps->scanId != 0)
						ExplainPropertyInteger("Dynamic Scan Id", ps->scanId, es);
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

	if (ResManagerPrintOperatorMemoryLimits())
	{
		ExplainPropertyInteger("operatorMem", PlanStateOperatorMemKB(planstate), es);
	}
	/*
	 * We have to forcibly clean up the instrumentation state because we
	 * haven't done ExecutorEnd yet.  This is pretty grotty ...
	 *
	 * Note: contrib/auto_explain could cause instrumentation to be set up
	 * even though we didn't ask for it here.  Be careful not to print any
	 * instrumentation results the user didn't ask for.  But we do the
	 * InstrEndLoop call anyway, if possible, to reduce the number of cases
	 * auto_explain has to contend with.
	 */
	if (planstate->instrument)
		InstrEndLoop(planstate->instrument);

	/* GPDB_90_MERGE_FIXME: In GPDB, these are printed differently. But does that work
	 * with the new XML/YAML EXPLAIN output */
	if (es->analyze &&
		planstate->instrument && planstate->instrument->nloops > 0)
	{
 		double		nloops = planstate->instrument->nloops;
		double		startup_sec = 1000.0 * planstate->instrument->startup / nloops;
		double		total_sec = 1000.0 * planstate->instrument->total / nloops;
		double		rows = planstate->instrument->ntuples / nloops;

		if (es->format == EXPLAIN_FORMAT_TEXT)
		{
			if (es->timing)
				appendStringInfo(es->str,
							" (actual time=%.3f..%.3f rows=%.0f loops=%.0f)",
								 startup_sec, total_sec, rows, nloops);
			else
				appendStringInfo(es->str,
								 " (actual rows=%.0f loops=%.0f)",
								 rows, nloops);
		}
		else
		{
			if (es->timing)
			{
				ExplainPropertyFloat("Actual Startup Time", startup_sec, 3, es);
				ExplainPropertyFloat("Actual Total Time", total_sec, 3, es);
			}
			ExplainPropertyFloat("Actual Rows", rows, 0, es);
			ExplainPropertyFloat("Actual Loops", nloops, 0, es);
		}
	}
	else if (es->analyze)
	{
		if (es->format == EXPLAIN_FORMAT_TEXT)
			appendStringInfoString(es->str, " (never executed)");
		else
		{
			if (es->timing)
			{
				ExplainPropertyFloat("Actual Startup Time", 0.0, 3, es);
				ExplainPropertyFloat("Actual Total Time", 0.0, 3, es);
			}
			ExplainPropertyFloat("Actual Rows", 0.0, 0, es);
			ExplainPropertyFloat("Actual Loops", 0.0, 0, es);
		}
	}

	/* in text format, first line ends here */
	if (es->format == EXPLAIN_FORMAT_TEXT)
		appendStringInfoChar(es->str, '\n');

	/* target list */
	if (es->verbose)
		show_plan_tlist(planstate, ancestors, es);

	/* quals, sort keys, etc */
	switch (nodeTag(plan))
	{
		case T_IndexScan:
		case T_DynamicIndexScan:
			show_scan_qual(((IndexScan *) plan)->indexqualorig,
						   "Index Cond", planstate, ancestors, es);
			if (((IndexScan *) plan)->indexqualorig)
				show_instrumentation_count("Rows Removed by Index Recheck", 2,
										   planstate, es);
			show_scan_qual(((IndexScan *) plan)->indexorderbyorig,
						   "Order By", planstate, ancestors, es);
			show_scan_qual(plan->qual, "Filter", planstate, ancestors, es);
			if (plan->qual)
				show_instrumentation_count("Rows Removed by Filter", 1,
										   planstate, es);
			break;
		case T_IndexOnlyScan:
			show_scan_qual(((IndexOnlyScan *) plan)->indexqual,
						   "Index Cond", planstate, ancestors, es);
			if (((IndexOnlyScan *) plan)->indexqual)
				show_instrumentation_count("Rows Removed by Index Recheck", 2,
										   planstate, es);
			show_scan_qual(((IndexOnlyScan *) plan)->indexorderby,
						   "Order By", planstate, ancestors, es);
			show_scan_qual(plan->qual, "Filter", planstate, ancestors, es);
			if (plan->qual)
				show_instrumentation_count("Rows Removed by Filter", 1,
										   planstate, es);
			if (es->analyze)
				ExplainPropertyLong("Heap Fetches",
				   ((IndexOnlyScanState *) planstate)->ioss_HeapFetches, es);
			break;
		case T_BitmapIndexScan:
		case T_DynamicBitmapIndexScan:
			show_scan_qual(((BitmapIndexScan *) plan)->indexqualorig,
						   "Index Cond", planstate, ancestors, es);
			break;
		case T_BitmapHeapScan:
		case T_DynamicBitmapHeapScan:
		{
			List		*bitmapqualorig;

			bitmapqualorig = ((BitmapHeapScan *) plan)->bitmapqualorig;

			show_scan_qual(bitmapqualorig,
						   "Recheck Cond", planstate, ancestors, es);

			if (bitmapqualorig)
				show_instrumentation_count("Rows Removed by Index Recheck", 2,
										   planstate, es);
			show_scan_qual(plan->qual, "Filter", planstate, ancestors, es);
			if (plan->qual)
				show_instrumentation_count("Rows Removed by Filter", 1,
										   planstate, es);
			if (es->analyze)
				show_tidbitmap_info((BitmapHeapScanState *) planstate, es);
			break;
		}
		case T_SeqScan:
		case T_DynamicSeqScan:
		case T_ExternalScan:
		case T_ValuesScan:
		case T_CteScan:
		case T_WorkTableScan:
		case T_SubqueryScan:
			show_scan_qual(plan->qual, "Filter", planstate, ancestors, es);
			if (plan->qual)
				show_instrumentation_count("Rows Removed by Filter", 1,
										   planstate, es);
			break;
		case T_FunctionScan:
			if (es->verbose)
			{
				List	   *fexprs = NIL;
				ListCell   *lc;

				foreach(lc, ((FunctionScan *) plan)->functions)
				{
					RangeTblFunction *rtfunc = (RangeTblFunction *) lfirst(lc);

					fexprs = lappend(fexprs, rtfunc->funcexpr);
				}
				/* We rely on show_expression to insert commas as needed */
				show_expression((Node *) fexprs,
								"Function Call", planstate, ancestors,
								es->verbose, es);
			}
			show_scan_qual(plan->qual, "Filter", planstate, ancestors, es);
			if (plan->qual)
				show_instrumentation_count("Rows Removed by Filter", 1,
										   planstate, es);
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
				show_scan_qual(tidquals, "TID Cond", planstate, ancestors, es);
				show_scan_qual(plan->qual, "Filter", planstate, ancestors, es);
				if (plan->qual)
					show_instrumentation_count("Rows Removed by Filter", 1,
											   planstate, es);
			}
			break;
		case T_ForeignScan:
			show_scan_qual(plan->qual, "Filter", planstate, ancestors, es);
			if (plan->qual)
				show_instrumentation_count("Rows Removed by Filter", 1,
										   planstate, es);
			show_foreignscan_info((ForeignScanState *) planstate, es);
			break;
		case T_NestLoop:
			show_upper_qual(((NestLoop *) plan)->join.joinqual,
							"Join Filter", planstate, ancestors, es);
			if (((NestLoop *) plan)->join.joinqual)
				show_instrumentation_count("Rows Removed by Join Filter", 1,
										   planstate, es);
			show_upper_qual(plan->qual, "Filter", planstate, ancestors, es);
			if (plan->qual)
				show_instrumentation_count("Rows Removed by Filter", 2,
										   planstate, es);
			break;
		case T_MergeJoin:
			show_upper_qual(((MergeJoin *) plan)->mergeclauses,
							"Merge Cond", planstate, ancestors, es);
			show_upper_qual(((MergeJoin *) plan)->join.joinqual,
							"Join Filter", planstate, ancestors, es);
			if (((MergeJoin *) plan)->join.joinqual)
				show_instrumentation_count("Rows Removed by Join Filter", 1,
										   planstate, es);
			show_upper_qual(plan->qual, "Filter", planstate, ancestors, es);
			if (plan->qual)
				show_instrumentation_count("Rows Removed by Filter", 2,
										   planstate, es);
			break;
		case T_HashJoin:
		{
			HashJoin *hash_join = (HashJoin *) plan;
			/*
			 * In the case of an "IS NOT DISTINCT" condition, we display
			 * hashqualclauses instead of hashclauses.
			 */
			List *cond_to_show = hash_join->hashclauses;
			if (list_length(hash_join->hashqualclauses) > 0)
				cond_to_show = hash_join->hashqualclauses;

			show_upper_qual(cond_to_show,
							"Hash Cond", planstate, ancestors, es);
			show_upper_qual(((HashJoin *) plan)->join.joinqual,
							"Join Filter", planstate, ancestors, es);
			if (((HashJoin *) plan)->join.joinqual)
				show_instrumentation_count("Rows Removed by Join Filter", 1,
										   planstate, es);
			show_upper_qual(plan->qual, "Filter", planstate, ancestors, es);
			if (plan->qual)
				show_instrumentation_count("Rows Removed by Filter", 2,
										   planstate, es);
			break;
		}
		case T_Agg:
			show_agg_keys((AggState *) planstate, ancestors, es);
			show_upper_qual(plan->qual, "Filter", planstate, ancestors, es);
			if (plan->qual)
				show_instrumentation_count("Rows Removed by Filter", 1,
										   planstate, es);
			break;
#if 0 /* Group node has been disabled in GPDB */
		case T_Group:
			show_group_keys((GroupState *) planstate, ancestors, es);
			show_upper_qual(plan->qual, "Filter", planstate, ancestors, es);
			if (plan->qual)
				show_instrumentation_count("Rows Removed by Filter", 1,
										   planstate, es);
			break;
#endif
		case T_WindowAgg:
			show_windowagg_keys((WindowAggState *) planstate, ancestors, es);
			break;
		case T_TableFunctionScan:
			show_scan_qual(plan->qual, "Filter", planstate, ancestors, es);
			/* TODO: Partitioning and ordering information */
			break;
		case T_Unique:
			show_motion_keys(planstate,
                             NIL,
						     ((Unique *) plan)->numCols,
						     ((Unique *) plan)->uniqColIdx,
						     "Group Key",
						     ancestors, es);
			break;
		case T_Sort:
			show_sort_keys((SortState *) planstate, ancestors, es);
			show_sort_info((SortState *) planstate, es);
			break;
		case T_MergeAppend:
			show_merge_append_keys((MergeAppendState *) planstate,
								   ancestors, es);
			break;
		case T_Result:
			show_upper_qual((List *) ((Result *) plan)->resconstantqual,
							"One-Time Filter", planstate, ancestors, es);
			show_upper_qual(plan->qual, "Filter", planstate, ancestors, es);
			if (plan->qual)
				show_instrumentation_count("Rows Removed by Filter", 1,
										   planstate, es);
			break;
		case T_ModifyTable:
			show_modifytable_info((ModifyTableState *) planstate, es);
			break;
		case T_Hash:
			show_hash_info((HashState *) planstate, es);
			break;
		case T_Repeat:
			show_upper_qual(plan->qual, "Filter", planstate, ancestors, es);
			break;
		case T_Motion:
			{
				Motion	   *pMotion = (Motion *) plan;

				if (pMotion->sendSorted || pMotion->motionType == MOTIONTYPE_HASH)
					show_motion_keys(planstate,
									 pMotion->hashExprs,
									 pMotion->numSortCols,
									 pMotion->sortColIdx,
									 "Merge Key",
									 ancestors, es);
			}
			break;
		case T_AssertOp:
			show_upper_qual(plan->qual, "Assert Cond", planstate, ancestors, es);
			break;
		case T_PartitionSelector:
			explain_partition_selector((PartitionSelector *) plan,
									   parentplanstate, ancestors, es);
			break;
		default:
			break;
	}

    /* Show executor statistics */
	if (planstate->instrument && planstate->instrument->need_cdb)
		cdbexplain_showExecStats(planstate, es);

	/* Show buffer usage */
	if (es->buffers && planstate->instrument)
	{
		const BufferUsage *usage = &planstate->instrument->bufusage;

		if (es->format == EXPLAIN_FORMAT_TEXT)
		{
			bool		has_shared = (usage->shared_blks_hit > 0 ||
									  usage->shared_blks_read > 0 ||
									  usage->shared_blks_dirtied > 0 ||
									  usage->shared_blks_written > 0);
			bool		has_local = (usage->local_blks_hit > 0 ||
									 usage->local_blks_read > 0 ||
									 usage->local_blks_dirtied > 0 ||
									 usage->local_blks_written > 0);
			bool		has_temp = (usage->temp_blks_read > 0 ||
									usage->temp_blks_written > 0);
			bool		has_timing = (!INSTR_TIME_IS_ZERO(usage->blk_read_time) ||
								 !INSTR_TIME_IS_ZERO(usage->blk_write_time));

			/* Show only positive counter values. */
			if (has_shared || has_local || has_temp)
			{
				appendStringInfoSpaces(es->str, es->indent * 2);
				appendStringInfoString(es->str, "Buffers:");

				if (has_shared)
				{
					appendStringInfoString(es->str, " shared");
					if (usage->shared_blks_hit > 0)
						appendStringInfo(es->str, " hit=%ld",
										 usage->shared_blks_hit);
					if (usage->shared_blks_read > 0)
						appendStringInfo(es->str, " read=%ld",
										 usage->shared_blks_read);
					if (usage->shared_blks_dirtied > 0)
						appendStringInfo(es->str, " dirtied=%ld",
										 usage->shared_blks_dirtied);
					if (usage->shared_blks_written > 0)
						appendStringInfo(es->str, " written=%ld",
										 usage->shared_blks_written);
					if (has_local || has_temp)
						appendStringInfoChar(es->str, ',');
				}
				if (has_local)
				{
					appendStringInfoString(es->str, " local");
					if (usage->local_blks_hit > 0)
						appendStringInfo(es->str, " hit=%ld",
										 usage->local_blks_hit);
					if (usage->local_blks_read > 0)
						appendStringInfo(es->str, " read=%ld",
										 usage->local_blks_read);
					if (usage->local_blks_dirtied > 0)
						appendStringInfo(es->str, " dirtied=%ld",
										 usage->local_blks_dirtied);
					if (usage->local_blks_written > 0)
						appendStringInfo(es->str, " written=%ld",
										 usage->local_blks_written);
					if (has_temp)
						appendStringInfoChar(es->str, ',');
				}
				if (has_temp)
				{
					appendStringInfoString(es->str, " temp");
					if (usage->temp_blks_read > 0)
						appendStringInfo(es->str, " read=%ld",
										 usage->temp_blks_read);
					if (usage->temp_blks_written > 0)
						appendStringInfo(es->str, " written=%ld",
										 usage->temp_blks_written);
				}
				appendStringInfoChar(es->str, '\n');
			}

			/* As above, show only positive counter values. */
			if (has_timing)
			{
				appendStringInfoSpaces(es->str, es->indent * 2);
				appendStringInfoString(es->str, "I/O Timings:");
				if (!INSTR_TIME_IS_ZERO(usage->blk_read_time))
					appendStringInfo(es->str, " read=%0.3f",
							  INSTR_TIME_GET_MILLISEC(usage->blk_read_time));
				if (!INSTR_TIME_IS_ZERO(usage->blk_write_time))
					appendStringInfo(es->str, " write=%0.3f",
							 INSTR_TIME_GET_MILLISEC(usage->blk_write_time));
				appendStringInfoChar(es->str, '\n');
			}
		}
		else
		{
			ExplainPropertyLong("Shared Hit Blocks", usage->shared_blks_hit, es);
			ExplainPropertyLong("Shared Read Blocks", usage->shared_blks_read, es);
			ExplainPropertyLong("Shared Dirtied Blocks", usage->shared_blks_dirtied, es);
			ExplainPropertyLong("Shared Written Blocks", usage->shared_blks_written, es);
			ExplainPropertyLong("Local Hit Blocks", usage->local_blks_hit, es);
			ExplainPropertyLong("Local Read Blocks", usage->local_blks_read, es);
			ExplainPropertyLong("Local Dirtied Blocks", usage->local_blks_dirtied, es);
			ExplainPropertyLong("Local Written Blocks", usage->local_blks_written, es);
			ExplainPropertyLong("Temp Read Blocks", usage->temp_blks_read, es);
			ExplainPropertyLong("Temp Written Blocks", usage->temp_blks_written, es);
			if (track_io_timing)
			{
				ExplainPropertyFloat("I/O Read Time", INSTR_TIME_GET_MILLISEC(usage->blk_read_time), 3, es);
				ExplainPropertyFloat("I/O Write Time", INSTR_TIME_GET_MILLISEC(usage->blk_write_time), 3, es);
			}
		}
	}

	/* Get ready to display the child plans */
	haschildren = planstate->initPlan ||
		outerPlanState(planstate) ||
		innerPlanState(planstate) ||
		IsA(plan, ModifyTable) ||
		IsA(plan, Append) ||
		IsA(plan, MergeAppend) ||
		IsA(plan, Sequence) ||
		IsA(plan, BitmapAnd) ||
		IsA(plan, BitmapOr) ||
		IsA(plan, SubqueryScan) ||
		planstate->subPlan;
	if (haschildren)
	{
		ExplainOpenGroup("Plans", "Plans", false, es);
		/* Pass current PlanState as head of ancestors list for children */
		ancestors = lcons(planstate, ancestors);
	}

	/* initPlan-s */
	if (plan->initPlan)
		ExplainSubPlans(planstate->initPlan, ancestors, "InitPlan", es, planstate->state->es_sliceTable);

	/* lefttree */
	if (outerPlan(plan) && !skip_outer)
	{
		ExplainNode(outerPlanState(planstate), ancestors,
					"Outer", NULL, es);
	}
    else if (skip_outer)
    {
		appendStringInfoSpaces(es->str, es->indent * 2);
		appendStringInfo(es->str, "  ->  ");
		appendStringInfoString(es->str, skip_outer_msg);
		appendStringInfo(es->str, "\n");
    }

	/* righttree */
	if (innerPlanState(planstate))
		ExplainNode(innerPlanState(planstate), ancestors,
					"Inner", NULL, es);

	/* special child plans */
	switch (nodeTag(plan))
	{
		case T_ModifyTable:
			ExplainMemberNodes(((ModifyTable *) plan)->plans,
							   ((ModifyTableState *) planstate)->mt_plans,
							   ancestors, es);
			break;
		case T_Append:
			ExplainMemberNodes(((Append *) plan)->appendplans,
							   ((AppendState *) planstate)->appendplans,
							   ancestors, es);
			break;
		case T_MergeAppend:
			ExplainMemberNodes(((MergeAppend *) plan)->mergeplans,
							   ((MergeAppendState *) planstate)->mergeplans,
							   ancestors, es);
			break;
		case T_Sequence:
			ExplainMemberNodes(((Sequence *) plan)->subplans,
							   ((SequenceState *) planstate)->subplans,
							   ancestors, es);
			break;
		case T_BitmapAnd:
			ExplainMemberNodes(((BitmapAnd *) plan)->bitmapplans,
							   ((BitmapAndState *) planstate)->bitmapplans,
							   ancestors, es);
			break;
		case T_BitmapOr:
			ExplainMemberNodes(((BitmapOr *) plan)->bitmapplans,
							   ((BitmapOrState *) planstate)->bitmapplans,
							   ancestors, es);
			break;
		case T_SubqueryScan:
			ExplainNode(((SubqueryScanState *) planstate)->subplan, ancestors,
						"Subquery", NULL, es);
			break;
		default:
			break;
	}

	/* subPlan-s */
	if (planstate->subPlan)
		ExplainSubPlans(planstate->subPlan, ancestors, "SubPlan", es, NULL);

	/* end of child plans */
	if (haschildren)
	{
		ancestors = list_delete_first(ancestors);
		ExplainCloseGroup("Plans", "Plans", false, es);
	}

	/* in text format, undo whatever indentation we added */
	if (es->format == EXPLAIN_FORMAT_TEXT)
		es->indent = save_indent;

	ExplainCloseGroup("Plan",
					  relationship ? NULL : "Plan",
					  true, es);

	es->currentSlice = save_currentSlice;
}

/*
 * Show the targetlist of a plan node
 */
static void
show_plan_tlist(PlanState *planstate, List *ancestors, ExplainState *es)
{
	Plan	   *plan = planstate->plan;
	List	   *context;
	List	   *result = NIL;
	bool		useprefix;
	ListCell   *lc;

	/* No work if empty tlist (this occurs eg in bitmap indexscans) */
	if (plan->targetlist == NIL)
		return;
	/* The tlist of an Append isn't real helpful, so suppress it */
	if (IsA(plan, Append))
		return;
	/* Likewise for MergeAppend and RecursiveUnion */
	if (IsA(plan, MergeAppend))
		return;
	if (IsA(plan, RecursiveUnion))
		return;

	/* Set up deparsing context */
	context = set_deparse_context_planstate(es->deparse_cxt,
											(Node *) planstate,
											ancestors);
	useprefix = list_length(es->rtable) > 1;

	/* Deparse each result column (we now include resjunk ones) */
	foreach(lc, plan->targetlist)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(lc);

		result = lappend(result,
						 deparse_expression((Node *) tle->expr, context,
											useprefix, false));
	}

	/* Print results */
	ExplainPropertyList("Output", result, es);
}

/*
 * Show a generic expression
 */
static void
show_expression(Node *node, const char *qlabel,
				PlanState *planstate, List *ancestors,
				bool useprefix, ExplainState *es)
{
	List	   *context;
	char	   *exprstr;

	/* Set up deparsing context */
	context = set_deparse_context_planstate(es->deparse_cxt,
											(Node *) planstate,
											ancestors);

	/* Deparse the expression */
	exprstr = deparse_expression(node, context, useprefix, false);

	/* And add to es->str */
	ExplainPropertyText(qlabel, exprstr, es);
}

/*
 * Show a qualifier expression (which is a List with implicit AND semantics)
 */
static void
show_qual(List *qual, const char *qlabel,
		  PlanState *planstate, List *ancestors,
		  bool useprefix, ExplainState *es)
{
	Node	   *node;

	/* No work if empty qual */
	if (qual == NIL)
		return;

	/* Convert AND list to explicit AND */
	node = (Node *) make_ands_explicit(qual);

	/* And show it */
	show_expression(node, qlabel, planstate, ancestors, useprefix, es);
}

/*
 * Show a qualifier expression for a scan plan node
 */
static void
show_scan_qual(List *qual, const char *qlabel,
			   PlanState *planstate, List *ancestors,
			   ExplainState *es)
{
	bool		useprefix;

	useprefix = (IsA(planstate->plan, SubqueryScan) ||es->verbose);
	show_qual(qual, qlabel, planstate, ancestors, useprefix, es);
}

/*
 * Show a qualifier expression for an upper-level plan node
 */
static void
show_upper_qual(List *qual, const char *qlabel,
				PlanState *planstate, List *ancestors,
				ExplainState *es)
{
	bool		useprefix;

	useprefix = (list_length(es->rtable) > 1 || es->verbose);
	show_qual(qual, qlabel, planstate, ancestors, useprefix, es);
}

/*
 * Show the sort keys for a Sort node.
 */
static void
show_sort_keys(SortState *sortstate, List *ancestors, ExplainState *es)
{
	Sort	   *plan = (Sort *) sortstate->ss.ps.plan;
	const char *SortKeystr;

	if (sortstate->noduplicates)
		SortKeystr = "Sort Key (Distinct)";
	else
		SortKeystr = "Sort Key";

	show_sort_group_keys((PlanState *) sortstate, SortKeystr,
						 plan->numCols, plan->sortColIdx,
						 ancestors, es);
}

static void
show_windowagg_keys(WindowAggState *waggstate, List *ancestors, ExplainState *es)
{
	WindowAgg *window = (WindowAgg *) waggstate->ss.ps.plan;

	/* The key columns refer to the tlist of the child plan */
	ancestors = lcons(window, ancestors);
	if ( window->partNumCols > 0 )
	{
		show_sort_group_keys((PlanState *) outerPlanState(waggstate), "Partition By",
							 window->partNumCols, window->partColIdx,
							 ancestors, es);
	}

	show_sort_group_keys((PlanState *) outerPlanState(waggstate), "Order By",
						 window->ordNumCols, window->ordColIdx,
						 ancestors, es);
	ancestors = list_delete_first(ancestors);

	/* XXX don't show framing for now */
}



/*
 * Likewise, for a MergeAppend node.
 */
static void
show_merge_append_keys(MergeAppendState *mstate, List *ancestors,
					   ExplainState *es)
{
	MergeAppend *plan = (MergeAppend *) mstate->ps.plan;

	show_sort_group_keys((PlanState *) mstate, "Sort Key",
						 plan->numCols, plan->sortColIdx,
						 ancestors, es);
}

/*
 * Show the grouping keys for an Agg node.
 */
static void
show_agg_keys(AggState *astate, List *ancestors,
			  ExplainState *es)
{
	Agg		   *plan = (Agg *) astate->ss.ps.plan;

	if (plan->numCols > 0)
	{
		/* The key columns refer to the tlist of the child plan */
		ancestors = lcons(astate, ancestors);
		show_sort_group_keys(outerPlanState(astate), "Group Key",
							 plan->numCols, plan->grpColIdx,
							 ancestors, es);
		ancestors = list_delete_first(ancestors);
	}
}

/*
 * Show the grouping keys for a Group node.
 */
#if 0
static void
show_group_keys(GroupState *gstate, List *ancestors,
				ExplainState *es)
{
	Group	   *plan = (Group *) gstate->ss.ps.plan;

	/* The key columns refer to the tlist of the child plan */
	ancestors = lcons(gstate, ancestors);
	show_sort_group_keys(outerPlanState(gstate), "Group Key",
						 plan->numCols, plan->grpColIdx,
						 ancestors, es);
	ancestors = list_delete_first(ancestors);
}
#endif

/*
 * Common code to show sort/group keys, which are represented in plan nodes
 * as arrays of targetlist indexes
 */
static void
show_sort_group_keys(PlanState *planstate, const char *qlabel,
					 int nkeys, AttrNumber *keycols,
					 List *ancestors, ExplainState *es)
{
	Plan	   *plan = planstate->plan;
	List	   *context;
	List	   *result = NIL;
	bool		useprefix;
	int			keyno;
	char	   *exprstr;

	if (nkeys <= 0)
		return;

	/* Set up deparsing context */
	context = set_deparse_context_planstate(es->deparse_cxt,
											(Node *) planstate,
											ancestors);
	useprefix = (list_length(es->rtable) > 1 || es->verbose);

	for (keyno = 0; keyno < nkeys; keyno++)
	{
		/* find key expression in tlist */
		AttrNumber	keyresno = keycols[keyno];
		TargetEntry *target = get_tle_by_resno(plan->targetlist,
											   keyresno);

		if (!target)
			elog(ERROR, "no tlist entry for key %d", keyresno);
		/* Deparse the expression, showing any top-level cast */
		exprstr = deparse_expression((Node *) target->expr, context,
									 useprefix, true);
		result = lappend(result, exprstr);
	}

	ExplainPropertyList(qlabel, result, es);

	/*
	 * GPDB_90_MERGE_FIXME: handle rollup times printing
	 * if (rollup_gs_times > 1)
	 *	appendStringInfo(es->str, " (%d times)", rollup_gs_times);
	 */
}

/*
 * If it's EXPLAIN ANALYZE, show tuplesort stats for a sort node
 *
 * GPDB_90_MERGE_FIXME: The sort statistics are stored quite differently from
 * upstream, it would be nice to rewrite this to avoid looping over all the
 * sort methods and instead have a _get_stats() function as in upstream.
 */
static void
show_sort_info(SortState *sortstate, ExplainState *es)
{
	CdbExplain_NodeSummary *ns;
	int			i;

	if (!es->analyze)
		return;

	ns = ((PlanState *) sortstate)->instrument->cdbNodeSummary;
	/* Might not have received stats from qExecs if they hit errors. */
	if (!ns)
		return;
	for (i = 0; i < NUM_SORT_METHOD; i++)
	{
		CdbExplain_Agg	*agg;
		const char *sortMethod;
		const char *spaceType;

		sortMethod = sort_method_enum_str[i];

		/*
		 * Memory and disk usage statistics are saved separately in GPDB so
		 * need to pull out the one in question first
		 */
		spaceType = "Memory";
		agg = &ns->sortSpaceUsed[MEMORY_SORT_SPACE_TYPE - 1][i];
		if (agg->vcnt > 0)
			spaceType = "Memory";
		else
		{
			spaceType = "Disk";
			agg = &ns->sortSpaceUsed[DISK_SORT_SPACE_TYPE - 1][i];
		}

		/*
		 * If the current sort method in question hasn't been used, skip to
		 * next one
		 */
		if (agg->vcnt  == 0)
			continue;

		if (es->format == EXPLAIN_FORMAT_TEXT)
		{
			appendStringInfoSpaces(es->str, es->indent * 2);
			appendStringInfo(es->str, "Sort Method:  %s  %s: %ldkB",
				sortMethod, spaceType, (long) agg->vsum);
			if (es->verbose)
			{
				appendStringInfo(es->str, "  Max Memory: %ldkB  Avg Memory: %ldkb (%d segments)",
								 (long) agg->vmax,
								 (long) (agg->vsum / agg->vcnt),
								 agg->vcnt);
			}
			appendStringInfo(es->str, "\n");
		}
		else
		{
			ExplainPropertyText("Sort Method", sortMethod, es);
			ExplainPropertyLong("Sort Space Used", (long) agg->vsum, es);
			ExplainPropertyText("Sort Space Type", spaceType, es);
			if (es->verbose)
			{
				ExplainPropertyLong("Sort Max Segment Memory", (long) agg->vmax, es);
				ExplainPropertyLong("Sort Avg Segment Memory", (long) (agg->vsum / agg->vcnt), es);
				ExplainPropertyInteger("Sort Segments", agg->vcnt, es);
			}
		}
	}
}

/*
 * Show information on hash buckets/batches.
 */
static void
show_hash_info(HashState *hashstate, ExplainState *es)
{
	HashJoinTable hashtable;

	Assert(IsA(hashstate, HashState));
	hashtable = hashstate->hashtable;

	if (hashtable)
	{
		long		spacePeakKb = (hashtable->spacePeak + 1023) / 1024;

		if (es->format != EXPLAIN_FORMAT_TEXT)
		{
			ExplainPropertyLong("Hash Buckets", hashtable->nbuckets, es);
			ExplainPropertyLong("Hash Batches", hashtable->nbatch, es);
			ExplainPropertyLong("Original Hash Batches",
								hashtable->nbatch_original, es);
			ExplainPropertyLong("Peak Memory Usage", spacePeakKb, es);
		}
		else if (hashtable->nbatch_original != hashtable->nbatch)
		{
			appendStringInfoSpaces(es->str, es->indent * 2);
			appendStringInfo(es->str,
			"Buckets: %d  Batches: %d (originally %d)  Memory Usage: %ldkB\n",
							 hashtable->nbuckets, hashtable->nbatch,
							 hashtable->nbatch_original, spacePeakKb);
		}
		else
		{
			appendStringInfoSpaces(es->str, es->indent * 2);
			appendStringInfo(es->str,
						   "Buckets: %d  Batches: %d  Memory Usage: %ldkB\n",
							 hashtable->nbuckets, hashtable->nbatch,
							 spacePeakKb);
		}
	}
}

/*
 * If it's EXPLAIN ANALYZE, show exact/lossy pages for a BitmapHeapScan node
 */
static void
show_tidbitmap_info(BitmapHeapScanState *planstate, ExplainState *es)
{
	if (es->format != EXPLAIN_FORMAT_TEXT)
	{
		ExplainPropertyLong("Exact Heap Blocks", planstate->exact_pages, es);
		ExplainPropertyLong("Lossy Heap Blocks", planstate->lossy_pages, es);
	}
	else
	{
		if (planstate->exact_pages > 0 || planstate->lossy_pages > 0)
		{
			appendStringInfoSpaces(es->str, es->indent * 2);
			appendStringInfoString(es->str, "Heap Blocks:");
			if (planstate->exact_pages > 0)
				appendStringInfo(es->str, " exact=%ld", planstate->exact_pages);
			if (planstate->lossy_pages > 0)
				appendStringInfo(es->str, " lossy=%ld", planstate->lossy_pages);
			appendStringInfoChar(es->str, '\n');
		}
	}
}

/*
 * If it's EXPLAIN ANALYZE, show instrumentation information for a plan node
 *
 * "which" identifies which instrumentation counter to print
 */
static void
show_instrumentation_count(const char *qlabel, int which,
						   PlanState *planstate, ExplainState *es)
{
	double		nfiltered;
	double		nloops;

	if (!es->analyze || !planstate->instrument)
		return;

	if (which == 2)
		nfiltered = planstate->instrument->nfiltered2;
	else
		nfiltered = planstate->instrument->nfiltered1;
	nloops = planstate->instrument->nloops;

	/* In text mode, suppress zero counts; they're not interesting enough */
	if (nfiltered > 0 || es->format != EXPLAIN_FORMAT_TEXT)
	{
		if (nloops > 0)
			ExplainPropertyFloat(qlabel, nfiltered / nloops, 0, es);
		else
			ExplainPropertyFloat(qlabel, 0.0, 0, es);
	}
}

/*
 * Show extra information for a ForeignScan node.
 */
static void
show_foreignscan_info(ForeignScanState *fsstate, ExplainState *es)
{
	FdwRoutine *fdwroutine = fsstate->fdwroutine;

	/* Let the FDW emit whatever fields it wants */
	if (fdwroutine->ExplainForeignScan != NULL)
		fdwroutine->ExplainForeignScan(fsstate, es);
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
 * Add some additional details about an IndexScan or IndexOnlyScan
 */
static void
ExplainIndexScanDetails(Oid indexid, ScanDirection indexorderdir,
						ExplainState *es)
{
	const char *indexname = explain_get_index_name(indexid);

	if (es->format == EXPLAIN_FORMAT_TEXT)
	{
		if (ScanDirectionIsBackward(indexorderdir))
			appendStringInfoString(es->str, " Backward");
		appendStringInfo(es->str, " using %s", indexname);
	}
	else
	{
		const char *scandir;

		switch (indexorderdir)
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

/*
 * Show the target of a Scan node
 */
static void
ExplainScanTarget(Scan *plan, ExplainState *es)
{
	ExplainTargetRel((Plan *) plan, plan->scanrelid, es);
}

/*
 * Show the target of a ModifyTable node
 */
static void
ExplainModifyTarget(ModifyTable *plan, ExplainState *es)
{
	Index		rti;

	/*
	 * We show the name of the first target relation.  In multi-target-table
	 * cases this should always be the parent of the inheritance tree.
	 */
	Assert(plan->resultRelations != NIL);
	rti = linitial_int(plan->resultRelations);

	ExplainTargetRel((Plan *) plan, rti, es);
}

/*
 * Show the target relation of a scan or modify node
 */
static void
ExplainTargetRel(Plan *plan, Index rti, ExplainState *es)
{
	char	   *objectname = NULL;
	char	   *namespace = NULL;
	const char *objecttag = NULL;
	RangeTblEntry *rte;
	char	   *refname;
	int			dynamicScanId = 0;

	rte = rt_fetch(rti, es->rtable);
	refname = (char *) list_nth(es->rtable_names, rti - 1);
	if (refname == NULL)
		refname = rte->eref->aliasname;

	switch (nodeTag(plan))
	{
		case T_SeqScan:
		case T_DynamicSeqScan:
		case T_IndexScan:
		case T_DynamicIndexScan:
		case T_IndexOnlyScan:
		case T_BitmapHeapScan:
		case T_DynamicBitmapHeapScan:
		case T_TidScan:
		case T_ForeignScan:
		case T_ModifyTable:
		case T_ExternalScan:
			/* Assert it's on a real relation */
			Assert(rte->rtekind == RTE_RELATION);
			objectname = get_rel_name(rte->relid);
			if (es->verbose)
				namespace = get_namespace_name(get_rel_namespace(rte->relid));
			objecttag = "Relation Name";

			/* Print dynamic scan id for dynamic scan operators */
			if (isDynamicScan(plan))
			{
				dynamicScanId = DynamicScan_GetDynamicScanIdPrintable(plan);
			}

			break;
		case T_FunctionScan:
			{
				FunctionScan *fscan = (FunctionScan *) plan;

				/* Assert it's on a RangeFunction */
				Assert(rte->rtekind == RTE_FUNCTION);

				/*
				 * If the expression is still a function call of a single
				 * function, we can get the real name of the function.
				 * Otherwise, punt.  (Even if it was a single function call
				 * originally, the optimizer could have simplified it away.)
				 */
				if (list_length(fscan->functions) == 1)
				{
					RangeTblFunction *rtfunc = (RangeTblFunction *) linitial(fscan->functions);

					if (IsA(rtfunc->funcexpr, FuncExpr))
					{
						FuncExpr   *funcexpr = (FuncExpr *) rtfunc->funcexpr;
						Oid			funcid = funcexpr->funcid;

						objectname = get_func_name(funcid);
						if (es->verbose)
							namespace =
								get_namespace_name(get_func_namespace(funcid));
					}
				}
				objecttag = "Function Name";
			}
			break;
		case T_TableFunctionScan:
			{
				TableFunctionScan *fscan = (TableFunctionScan *) plan;

				/* Assert it's on a RangeFunction */
				Assert(rte->rtekind == RTE_TABLEFUNCTION);

				/*
				 * Unlike in a FunctionScan, in a TableFunctionScan the call
				 * should always be a function call of a single function.
				 * Get the real name of the function.
				 */
				{
					RangeTblFunction *rtfunc = fscan->function;

					if (IsA(rtfunc->funcexpr, FuncExpr))
					{
						FuncExpr   *funcexpr = (FuncExpr *) rtfunc->funcexpr;
						Oid			funcid = funcexpr->funcid;

						objectname = get_func_name(funcid);
						if (es->verbose)
							namespace =
								get_namespace_name(get_func_namespace(funcid));
					}
				}
				objecttag = "Function Name";

				/* might be nice to add order by and scatter by info, if it's a TableFunctionScan */
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
			objecttag = "CTE Name";
			break;
		case T_WorkTableScan:
			/* Assert it's on a self-reference CTE */
			Assert(rte->rtekind == RTE_CTE);
			Assert(rte->self_reference);
			objectname = rte->ctename;
			objecttag = "CTE Name";
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
		if (objectname == NULL || strcmp(refname, objectname) != 0)
			appendStringInfo(es->str, " %s", quote_identifier(refname));

		if (dynamicScanId != 0)
			appendStringInfo(es->str, " (dynamic scan id: %d)",
							 dynamicScanId);
	}
	else
	{
		if (objecttag != NULL && objectname != NULL)
			ExplainPropertyText(objecttag, objectname, es);
		if (namespace != NULL)
			ExplainPropertyText("Schema", namespace, es);
		ExplainPropertyText("Alias", refname, es);

		if (dynamicScanId != 0)
			ExplainPropertyInteger("Dynamic Scan Id", dynamicScanId, es);
	}
}

/*
 * Show extra information for a ModifyTable node
 */
static void
show_modifytable_info(ModifyTableState *mtstate, ExplainState *es)
{
	FdwRoutine *fdwroutine = mtstate->resultRelInfo->ri_FdwRoutine;

	/*
	 * If the first target relation is a foreign table, call its FDW to
	 * display whatever additional fields it wants to.  For now, we ignore the
	 * possibility of other targets being foreign tables, although the API for
	 * ExplainForeignModify is designed to allow them to be processed.
	 */
	if (fdwroutine != NULL &&
		fdwroutine->ExplainForeignModify != NULL)
	{
		ModifyTable *node = (ModifyTable *) mtstate->ps.plan;
		List	   *fdw_private = (List *) linitial(node->fdwPrivLists);

		fdwroutine->ExplainForeignModify(mtstate,
										 mtstate->resultRelInfo,
										 fdw_private,
										 0,
										 es);
	}
}

/*
 * Explain the constituent plans of a ModifyTable, Append, MergeAppend,
 * BitmapAnd, or BitmapOr node.
 *
 * The ancestors list should already contain the immediate parent of these
 * plans.
 *
 * Note: we don't actually need to examine the Plan list members, but
 * we need the list in order to determine the length of the PlanState array.
 */
static void
ExplainMemberNodes(List *plans, PlanState **planstates,
				   List *ancestors, ExplainState *es)
{
	int			nplans = list_length(plans);
	int			j;

	for (j = 0; j < nplans; j++)
		ExplainNode(planstates[j], ancestors,
					"Member", NULL, es);
}

/*
 * Explain a list of SubPlans (or initPlans, which also use SubPlan nodes).
 *
 * The ancestors list should already contain the immediate parent of these
 * SubPlanStates.
 */
static void
ExplainSubPlans(List *plans, List *ancestors,
				const char *relationship, ExplainState *es,
				SliceTable *sliceTable)
{
	ListCell   *lst;
	Slice      *saved_slice = es->currentSlice;

	foreach(lst, plans)
	{
		SubPlanState *sps = (SubPlanState *) lfirst(lst);
		SubPlan    *sp = (SubPlan *) sps->xprstate.expr;

		/* Subplan might have its own root slice */
		if (sliceTable && sp->qDispSliceId > 0)
		{
			es->currentSlice = (Slice *)list_nth(sliceTable->slices,
												 sp->qDispSliceId);
		}

		ExplainNode(sps->planstate, ancestors,
					relationship, sp->plan_name, es);
	}

	es->currentSlice = saved_slice;
}

/*
 * Explain a property, such as sort keys or targets, that takes the form of
 * a list of unlabeled items.  "data" is a list of C strings.
 */
void
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
				char	   *str;

				appendStringInfoSpaces(es->str, es->indent * 2 + 2);
				appendStringInfoString(es->str, "<Item>");
				str = escape_xml((const char *) lfirst(lc));
				appendStringInfoString(es->str, str);
				pfree(str);
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

		case EXPLAIN_FORMAT_YAML:
			ExplainYAMLLineStarting(es);
			appendStringInfo(es->str, "%s: ", qlabel);
			foreach(lc, data)
			{
				appendStringInfoChar(es->str, '\n');
				appendStringInfoSpaces(es->str, es->indent * 2 + 2);
				appendStringInfoString(es->str, "- ");
				escape_yaml(es->str, (const char *) lfirst(lc));
			}
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
				char	   *str;

				appendStringInfoSpaces(es->str, es->indent * 2);
				ExplainXMLTag(qlabel, X_OPENING | X_NOWHITESPACE, es);
				str = escape_xml(value);
				appendStringInfoString(es->str, str);
				pfree(str);
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

		case EXPLAIN_FORMAT_YAML:
			ExplainYAMLLineStarting(es);
			appendStringInfo(es->str, "%s: ", qlabel);
			if (numeric)
				appendStringInfoString(es->str, value);
			else
				escape_yaml(es->str, value);
			break;
	}
}

static void
ExplainPropertyStringInfo(const char *qlabel, ExplainState *es, const char *fmt,...)
{
	StringInfoData buf;

	initStringInfo(&buf);

	for (;;)
	{
		va_list		args;
		int			needed;

		/* Try to format the data. */
		va_start(args, fmt);
		needed = appendStringInfoVA(&buf, fmt, args);
		va_end(args);

		if (needed == 0)
			break;

		/* Double the buffer size and try again. */
		enlargeStringInfo(&buf, needed);
	}

	ExplainPropertyText(qlabel, buf.data, es);
	pfree(buf.data);
}

/*
 * Explain a string-valued property.
 */
void
ExplainPropertyText(const char *qlabel, const char *value, ExplainState *es)
{
	ExplainProperty(qlabel, value, false, es);
}

/*
 * Explain an integer-valued property.
 */
void
ExplainPropertyInteger(const char *qlabel, int value, ExplainState *es)
{
	char		buf[32];

	snprintf(buf, sizeof(buf), "%d", value);
	ExplainProperty(qlabel, buf, true, es);
}

/*
 * Explain a long-integer-valued property.
 */
void
ExplainPropertyLong(const char *qlabel, long value, ExplainState *es)
{
	char		buf[32];

	snprintf(buf, sizeof(buf), "%ld", value);
	ExplainProperty(qlabel, buf, true, es);
}

/*
 * Explain a float-valued property, using the specified number of
 * fractional digits.
 */
void
ExplainPropertyFloat(const char *qlabel, double value, int ndigits,
					 ExplainState *es)
{
	char		buf[256];

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
void
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
			 * emitted something (and so the next item needs a comma). See
			 * ExplainJSONLineEnding().
			 */
			es->grouping_stack = lcons_int(0, es->grouping_stack);
			es->indent++;
			break;

		case EXPLAIN_FORMAT_YAML:

			/*
			 * In YAML format, the grouping stack is an integer list.  0 means
			 * we've emitted nothing at this grouping level AND this grouping
			 * level is unlabelled and must be marked with "- ".  See
			 * ExplainYAMLLineStarting().
			 */
			ExplainYAMLLineStarting(es);
			if (labelname)
			{
				appendStringInfo(es->str, "%s: ", labelname);
				es->grouping_stack = lcons_int(1, es->grouping_stack);
			}
			else
			{
				appendStringInfoString(es->str, "- ");
				es->grouping_stack = lcons_int(0, es->grouping_stack);
			}
			es->indent++;
			break;
	}
}

/*
 * Close a group of related objects.
 * Parameters must match the corresponding ExplainOpenGroup call.
 */
void
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

		case EXPLAIN_FORMAT_YAML:
			es->indent--;
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

		case EXPLAIN_FORMAT_YAML:
			ExplainYAMLLineStarting(es);
			if (labelname)
			{
				escape_yaml(es->str, labelname);
				appendStringInfoString(es->str, ": ");
			}
			else
			{
				appendStringInfoString(es->str, "- ");
			}
			escape_yaml(es->str, objtype);
			break;
	}
}

/*
 * Emit the start-of-output boilerplate.
 *
 * This is just enough different from processing a subgroup that we need
 * a separate pair of subroutines.
 */
void
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

		case EXPLAIN_FORMAT_YAML:
			es->grouping_stack = lcons_int(0, es->grouping_stack);
			break;
	}
}

/*
 * Emit the end-of-output boilerplate.
 */
void
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

		case EXPLAIN_FORMAT_YAML:
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
		case EXPLAIN_FORMAT_JSON:
		case EXPLAIN_FORMAT_YAML:
			/* nothing to do */
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
 * XML restricts tag names more than our other output formats, eg they can't
 * contain white space or slashes.  Replace invalid characters with dashes,
 * so that for example "I/O Read Time" becomes "I-O-Read-Time".
 */
static void
ExplainXMLTag(const char *tagname, int flags, ExplainState *es)
{
	const char *s;
	const char *valid = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.";

	if ((flags & X_NOWHITESPACE) == 0)
		appendStringInfoSpaces(es->str, 2 * es->indent);
	appendStringInfoCharMacro(es->str, '<');
	if ((flags & X_CLOSING) != 0)
		appendStringInfoCharMacro(es->str, '/');
	for (s = tagname; *s; s++)
		appendStringInfoChar(es->str, strchr(valid, *s) ? *s : '-');
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

/*
 * Indent a YAML line.
 *
 * YAML lines are ordinarily indented by two spaces per indentation level.
 * The text emitted for each property begins just prior to the preceding
 * line-break, except for the first property in an unlabelled group, for which
 * it begins immediately after the "- " that introduces the group.  The first
 * property of the group appears on the same line as the opening "- ".
 */
static void
ExplainYAMLLineStarting(ExplainState *es)
{
	Assert(es->format == EXPLAIN_FORMAT_YAML);
	if (linitial_int(es->grouping_stack) == 0)
	{
		linitial_int(es->grouping_stack) = 1;
	}
	else
	{
		appendStringInfoChar(es->str, '\n');
		appendStringInfoSpaces(es->str, es->indent * 2);
	}
}

/*
 * YAML is a superset of JSON; unfortunately, the YAML quoting rules are
 * ridiculously complicated -- as documented in sections 5.3 and 7.3.3 of
 * http://yaml.org/spec/1.2/spec.html -- so we chose to just quote everything.
 * Empty strings, strings with leading or trailing whitespace, and strings
 * containing a variety of special characters must certainly be quoted or the
 * output is invalid; and other seemingly harmless strings like "0xa" or
 * "true" must be quoted, lest they be interpreted as a hexadecimal or Boolean
 * constant rather than a string.
 */
static void
escape_yaml(StringInfo buf, const char *str)
{
	escape_json(buf, str);
}
