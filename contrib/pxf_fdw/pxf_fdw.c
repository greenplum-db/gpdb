/*
 * pxf_fdw.c
 *		  Foreign-data wrapper for PXF (Platform Extension Framework)
 *
 * IDENTIFICATION
 *		  contrib/pxf_fdw/pxf_fdw.c
 */

#include "postgres.h"

#include "pxf_fdw.h"
#include "pxf_fragment.h"
#include "pxf_bridge.h"

#include "access/sysattr.h"
#include "access/reloptions.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_foreign_table.h"
#include "catalog/pg_user_mapping.h"
#include "catalog/pg_type.h"
#include "cdb/cdbsreh.h"
#include "cdb/cdbvars.h"
#include "commands/copy.h"
#include "commands/defrem.h"
#include "commands/explain.h"
#include "foreign/fdwapi.h"
#include "foreign/foreign.h"
#include "nodes/pg_list.h"
#include "nodes/makefuncs.h"
#include "optimizer/paths.h"
#include "optimizer/pathnode.h"
#include "optimizer/planmain.h"
#include "optimizer/restrictinfo.h"
#include "optimizer/var.h"
#include "parser/parsetree.h"
#include "utils/memutils.h"

PG_MODULE_MAGIC;

#define DEFAULT_PXF_FDW_STARTUP_COST   50000

extern Datum pxf_fdw_handler(PG_FUNCTION_ARGS);

/*
 * SQL functions
 */
PG_FUNCTION_INFO_V1(pxf_fdw_handler);

/*
 * FDW functions declarations
 */
static void pxfGetForeignRelSize(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid);
static void pxfGetForeignPaths(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid);
#if (PG_VERSION_NUM <= 90500)
static ForeignScan *pxfGetForeignPlan(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid, ForeignPath *best_path, List *tlist, List *scan_clauses);
#else
static ForeignScan *pxfGetForeignPlan(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid, ForeignPath *best_path, List *tlist, List *scan_clauses, Plan *outer_plan);
#endif
static void pxfExplainForeignScan(ForeignScanState *node, ExplainState *es);
static void pxfBeginForeignScan(ForeignScanState *node, int eflags);
static TupleTableSlot *pxfIterateForeignScan(ForeignScanState *node);
static void pxfReScanForeignScan(ForeignScanState *node);
static void pxfEndForeignScan(ForeignScanState *node);

/*
 * Helper functions
 */
static void InitCopyState(PxfFdwExecutionState * estate, Relation relation);
static int	PxfCallback(void *outbuf, int datasize, void *extra);
static HeapTuple direct_call(PxfFdwExecutionState * festate);


/*
 * Foreign-data wrapper handler functions:
 * returns a struct with pointers to the
 * pxf_fdw callback routines.
 */
Datum
pxf_fdw_handler(PG_FUNCTION_ARGS)
{
	FdwRoutine *fdw_routine = makeNode(FdwRoutine);

	/*
	 * foreign table scan support
	 */

	/* master - only */
	fdw_routine->GetForeignRelSize = pxfGetForeignRelSize;
	fdw_routine->GetForeignPaths = pxfGetForeignPaths;
	fdw_routine->GetForeignPlan = pxfGetForeignPlan;
	fdw_routine->ExplainForeignScan = pxfExplainForeignScan;

	/* segment - only when mpp_execute = segments */
	fdw_routine->BeginForeignScan = pxfBeginForeignScan;
	fdw_routine->IterateForeignScan = pxfIterateForeignScan;
	fdw_routine->ReScanForeignScan = pxfReScanForeignScan;
	fdw_routine->EndForeignScan = pxfEndForeignScan;

	PG_RETURN_POINTER(fdw_routine);
}

/*
 * GetForeignRelSize
 *		set relation size estimates for a foreign table
 */
static void
pxfGetForeignRelSize(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid)
{
	/* Bitmapset  *attrsUsed = NULL; */

	elog(DEBUG5, "pxf_fdw: pxfGetForeignRelSize starts");

	/* Collect all the attributes needed for joins or final output. */

	/*
	 * pull_varattnos((Node *) baserel->reltargetlist, baserel->relid,
	 * &attrsUsed);
	 */

/*	Relation relation = relation_open(baserel->relid, NoLock);
	getFragmentList(options, relation, NULL, NULL); */

	baserel->rows = 1000;

	elog(DEBUG5, "pxf_fdw: pxfGetForeignRelSize ends");
}

/*
 * GetForeignPaths
 *		create access path for a scan on the foreign table
 */
static void
pxfGetForeignPaths(PlannerInfo *root,
				   RelOptInfo *baserel,
				   Oid foreigntableid)
{
	ForeignPath *path = NULL;
	int			total_cost = DEFAULT_PXF_FDW_STARTUP_COST;


	elog(DEBUG5, "pxf_fdw: pxfGetForeignPaths starts");

	path = create_foreignscan_path(root, baserel,
#if PG_VERSION_NUM >= 90600
								   NULL,	/* default pathtarget */
#endif
								   baserel->rows,
								   DEFAULT_PXF_FDW_STARTUP_COST,
								   total_cost,
								   NIL, /* no pathkeys */
								   NULL,	/* no outer rel either */
#if PG_VERSION_NUM >= 90500
								   NULL,	/* no extra plan */
#endif
								   NIL);



	/*
	 * Create a ForeignPath node and add it as only possible path.
	 */
	add_path(baserel, (Path *) path);

	elog(DEBUG5, "pxf_fdw: pxfGetForeignPaths ends");
}

/*
 * GetForeignPlan
 *		create a ForeignScan plan node
 */
#if PG_VERSION_NUM >= 90500
static ForeignScan *
pxfGetForeignPlan(PlannerInfo *root,
				  RelOptInfo *baserel,
				  Oid foreigntableid,
				  ForeignPath *best_path,
				  List *tlist,
				  List *scan_clauses,
				  Plan *outer_plan)
#else
static ForeignScan *
pxfGetForeignPlan(PlannerInfo *root,
				  RelOptInfo *baserel,
				  Oid foreigntableid,
				  ForeignPath *best_path,
				  List *tlist,
				  List *scan_clauses)
#endif
{
	Index		scan_relid = baserel->relid;

	elog(DEBUG5, "pxf_fdw: pxfGetForeignPlan starts");

	/*
	 * We have no native ability to evaluate restriction clauses, so we just
	 * put all the scan_clauses into the plan node's qual list for the
	 * executor to check.  So all we have to do here is strip RestrictInfo
	 * nodes from the clauses and ignore pseudoconstants (which will be
	 * handled elsewhere).
	 */
	scan_clauses = extract_actual_clauses(scan_clauses, false);

	elog(DEBUG5, "pxf_fdw: pxfGetForeignPlan ends");

	return make_foreignscan(tlist,
							scan_clauses,
							scan_relid,
							NIL,	/* no expressions to evaluate */
							NIL
#if PG_VERSION_NUM >= 90500
							,NIL
							,remote_exprs
							,outer_plan
#endif
		);

}

/*
 * pxfExplainForeignScan
 *		Produce extra output for EXPLAIN of a ForeignScan on a foreign table
 */
static void
pxfExplainForeignScan(ForeignScanState *node, ExplainState *es)
{
	elog(DEBUG5, "pxf_fdw: pxfExplainForeignScan starts on segment: %d", PXF_SEGMENT_ID);
	elog(DEBUG5, "pxf_fdw: pxfExplainForeignScan ends on segment: %d", PXF_SEGMENT_ID);
}

/*
 * BeginForeignScan
 *   called during executor startup. perform any initialization
 *   needed, but not start the actual scan.
 */
static void
pxfBeginForeignScan(ForeignScanState *node, int eflags)
{
	List	   *quals = node->ss.ps.qual;
	Oid			foreigntableid = RelationGetRelid(node->ss.ss_currentRelation);
	ProjectionInfo *proj_info = node->ss.ps.ps_ProjInfo;
	PxfFdwExecutionState *festate = NULL;
	PxfOptions *options = NULL;
	Relation	relation = node->ss.ss_currentRelation;

	elog(DEBUG5, "pxf_fdw: pxfBeginForeignScan starts on segment: %d", PXF_SEGMENT_ID);

	/*
	 * Do nothing in EXPLAIN (no ANALYZE) case.  node->fdw_state stays NULL.
	 */
	if (eflags & EXEC_FLAG_EXPLAIN_ONLY)
		return;

	options = PxfGetOptions(foreigntableid);

	/*
	 * Save state in node->fdw_state.  We must save enough information to call
	 * BeginCopyFrom() again.
	 */
	festate = (PxfFdwExecutionState *) palloc(sizeof(PxfFdwExecutionState));
	festate->options = options;
	festate->proj_info = proj_info;
	festate->quals = quals;

	InitCopyState(festate, relation);
	node->fdw_state = (void *) festate;

	elog(DEBUG5, "pxf_fdw: pxfBeginForeignScan ends on segment: %d", PXF_SEGMENT_ID);
}

/*
 * IterateForeignScan
 *		Retrieve next row from the result set, or clear tuple slot to indicate
 *		EOF.
 *   Fetch one row from the foreign source, returning it in a tuple table slot
 *    (the node's ScanTupleSlot should be used for this purpose).
 *  Return NULL if no more rows are available.
 */
static TupleTableSlot *
pxfIterateForeignScan(ForeignScanState *node)
{
	elog(DEBUG5, "pxf_fdw: pxfIterateForeignScan Executing on segment: %d",
		 PXF_SEGMENT_ID);

	PxfFdwExecutionState *pxfestate = (PxfFdwExecutionState *) node->fdw_state;
	TupleTableSlot *slot = node->ss.ss_ScanTupleSlot;
	ErrorContextCallback errcallback;
	bool		found;


	/* Set up callback to identify error line number. */
	errcallback.callback = CopyFromErrorCallback;
	errcallback.arg = (void *) pxfestate->cstate;
	errcallback.previous = error_context_stack;
	error_context_stack = &errcallback;

	/*
	 * The protocol for loading a virtual tuple into a slot is first
	 * ExecClearTuple, then fill the values/isnull arrays, then
	 * ExecStoreVirtualTuple.  If we don't find another row in the file, we
	 * just skip the last step, leaving the slot empty as required.
	 *
	 * We can pass ExprContext = NULL because we read all columns from the
	 * file, so no need to evaluate default expressions.
	 *
	 * We can also pass tupleOid = NULL because we don't allow oids for
	 * foreign tables.
	 */
	ExecClearTuple(slot);

	found = NextCopyFrom(pxfestate->cstate,
						 NULL,
						 slot_get_values(slot),
						 slot_get_isnull(slot),
						 NULL);
	if (found)
	{
		if (pxfestate->cstate->cdbsreh)
		{
			/*
			 * If NextCopyFrom failed, the processed row count will have
			 * already been updated, but we need to update it in a successful
			 * case.
			 *
			 * GPDB_91_MERGE_FIXME: this is almost certainly not the right
			 * place for this, but row counts are currently scattered all over
			 * the place. Consolidate.
			 */
			pxfestate->cstate->cdbsreh->processed++;
		}

		ExecStoreVirtualTuple(slot);
	}

	/* Remove error callback. */
	error_context_stack = errcallback.previous;

	return slot;
}

/*
 * ReScanForeignScan
 *		Restart the scan from the beginning
 */
static void
pxfReScanForeignScan(ForeignScanState *node)
{
	elog(DEBUG5, "pxf_fdw: pxfReScanForeignScan starts on segment: %d", PXF_SEGMENT_ID);
	elog(DEBUG5, "pxf_fdw: pxfReScanForeignScan ends on segment: %d", PXF_SEGMENT_ID);
}

/*
 * EndForeignScan
 *		End the scan and release resources.
 */
static void
pxfEndForeignScan(ForeignScanState *node)
{
	elog(DEBUG5, "pxf_fdw: pxfEndForeignScan starts on segment: %d", PXF_SEGMENT_ID);

	ForeignScan *foreignScan = (ForeignScan *) node->ss.ps.plan;
	PxfFdwExecutionState *pxfestate = (PxfFdwExecutionState *) node->fdw_state;

	/* Release resources */
	if (foreignScan->fdw_private)
	{
		elog(DEBUG5, "Freeing fdw_private");
		pfree(foreignScan->fdw_private);
	}

	/* if pxfestate is NULL, we are in EXPLAIN; nothing to do */
	if (pxfestate)
	{
		if (pxfestate->cstate->data_source_cb_extra)
		{
			elog(DEBUG5, "Freeing data_source_cb_extra");

			pfree(pxfestate->cstate->data_source_cb_extra);
		}

		EndCopyFrom(pxfestate->cstate);
		elog(DEBUG5, "Freeing pxfestate");
		pfree(pxfestate);
	}

	elog(DEBUG5, "pxf_fdw: pxfEndForeignScan ends on segment: %d", PXF_SEGMENT_ID);
}

/*
 * Callback function invoked during pxfIterateForeignScan to retrieve data from PXF
 */
static int
PxfCallback(void *outbuf, int datasize, void *extra)
{
	return PxfBridgeRead(extra, outbuf, datasize);
}

/*
 * Creates a context for the PxfCallback function
 */
static void
InitCopyState(PxfFdwExecutionState * estate, Relation relation)
{
	List	   *fragments;
	PxfBridgeContext *context;
	List	   *copy_options;
	CopyState	cstate;

	fragments = getFragmentList(estate->options, relation, NULL, estate->proj_info, estate->quals);
	/* set context */
	context = palloc0(sizeof(PxfBridgeContext));

	initStringInfo(&context->uri);
	context->fragments = fragments;
	context->relation = relation;
	context->filterstr = NULL;
	context->options = estate->options;

	copy_options = estate->options->copy_options;

	PxfBridgeImportStart(context);

	/*
	 * Create CopyState from FDW options.  We always acquire all columns, so
	 * as to match the expected ScanTupleSlot signature.
	 */
	cstate = BeginCopyFrom(relation,
						   NULL,
						   false,	/* is_program */
						   &PxfCallback,	/* data_source_cb */
						   context, /* data_source_cb_extra */
						   NIL, /* attnamelist */
						   copy_options,	/* copy options */
						   NIL);	/* ao_segnos */


	if (estate->options->reject_limit == -1)
	{
		/* Default error handling - "all-or-nothing" */
		cstate->cdbsreh = NULL; /* no SREH */
		cstate->errMode = ALL_OR_NOTHING;
	}
	else
	{
		/* XXX: no error log for now */
		cstate->errMode = SREH_IGNORE;
		cstate->cdbsreh = makeCdbSreh(estate->options->reject_limit,
									  estate->options->is_reject_limit_rows,
									  estate->options->resource,
									  (char *) cstate->cur_relname,
									  false /* logerrors */ );

		cstate->cdbsreh->relid = RelationGetRelid(relation);
	}

	/* and 'fe_mgbuf' */
	cstate->fe_msgbuf = makeStringInfo();

	/*
	 * Create a temporary memory context that we can reset once per row to
	 * recover palloc'd memory.  This avoids any problems with leaks inside
	 * datatype input or output routines, and should be faster than retail
	 * pfree's anyway.
	 */
	cstate->rowcontext = AllocSetContextCreate(CurrentMemoryContext,
											   "PxfFdwMemCxt",
											   ALLOCSET_DEFAULT_MINSIZE,
											   ALLOCSET_DEFAULT_INITSIZE,
											   ALLOCSET_DEFAULT_MAXSIZE);

	estate->cstate = cstate;
}
