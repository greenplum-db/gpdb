/*-------------------------------------------------------------------------
 *
 * execMain.c
 *	  top level executor interface routines
 *
 * INTERFACE ROUTINES
 *	ExecutorStart()
 *	ExecutorRun()
 *	ExecutorFinish()
 *	ExecutorEnd()
 *
 *	These four procedures are the external interface to the executor.
 *	In each case, the query descriptor is required as an argument.
 *
 *	ExecutorStart must be called at the beginning of execution of any
 *	query plan and ExecutorEnd must always be called at the end of
 *	execution of a plan (unless it is aborted due to error).
 *
 *	ExecutorRun accepts direction and count arguments that specify whether
 *	the plan is to be executed forwards, backwards, and for how many tuples.
 *	In some cases ExecutorRun may be called multiple times to process all
 *	the tuples for a plan.  It is also acceptable to stop short of executing
 *	the whole plan (but only if it is a SELECT).
 *
 *	ExecutorFinish must be called after the final ExecutorRun call and
 *	before ExecutorEnd.  This can be omitted only in case of EXPLAIN,
 *	which should also omit ExecutorRun.
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2016, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/execMain.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/appendonlywriter.h"
#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/transam.h"
#include "access/xact.h"
#include "catalog/namespace.h"
#include "catalog/pg_tablespace.h"
#include "catalog/aoblkdir.h"
#include "catalog/aovisimap.h"
#include "catalog/catalog.h"
#include "catalog/oid_dispatch.h"
#include "catalog/pg_attribute_encoding.h"
#include "catalog/pg_type.h"
#include "cdb/cdbpartition.h"
#include "commands/copy.h"
#include "commands/createas.h"
#include "commands/matview.h"
#include "commands/tablespace.h"
#include "commands/trigger.h"
#include "executor/execdebug.h"
#include "executor/execUtils.h"
#include "executor/instrument.h"
#include "executor/nodeSubplan.h"
#include "foreign/fdwapi.h"
#include "libpq/pqformat.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "optimizer/clauses.h"
#include "parser/parsetree.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "tcop/utility.h"
#include "utils/acl.h"
#include "utils/builtins.h" /* dumpDynamicTableScanPidIndex() */
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rls.h"
#include "utils/snapmgr.h"
#include "utils/tqual.h"
#include "utils/metrics_utils.h"

#include "utils/ps_status.h"
#include "utils/snapmgr.h"
#include "utils/typcache.h"
#include "utils/workfile_mgr.h"
#include "utils/faultinjector.h"
#include "utils/resource_manager.h"

#include "catalog/pg_inherits_fn.h"
#include "catalog/pg_statistic.h"
#include "catalog/pg_class.h"

#include "tcop/tcopprot.h"

#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbaocsam.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbexplain.h"             /* cdbexplain_sendExecStats() */
#include "cdb/cdbplan.h"
#include "cdb/cdbsubplan.h"
#include "cdb/cdbvars.h"
#include "cdb/ml_ipc.h"
#include "cdb/cdbmotion.h"
#include "cdb/cdbtm.h"
#include "cdb/cdboidsync.h"
#include "cdb/cdbllize.h"
#include "cdb/memquota.h"
#include "cdb/cdbtargeteddispatch.h"
#include "cdb/cdbutil.h"


/* Hooks for plugins to get control in ExecutorStart/Run/Finish/End */
ExecutorStart_hook_type ExecutorStart_hook = NULL;
ExecutorRun_hook_type ExecutorRun_hook = NULL;
ExecutorFinish_hook_type ExecutorFinish_hook = NULL;
ExecutorEnd_hook_type ExecutorEnd_hook = NULL;

/* Hook for plugin to get control in ExecCheckRTPerms() */
ExecutorCheckPerms_hook_type ExecutorCheckPerms_hook = NULL;

/* decls for local routines only used within this module */
static void InitPlan(QueryDesc *queryDesc, int eflags);
static void CheckValidRowMarkRel(Relation rel, RowMarkType markType);
static void ExecPostprocessPlan(EState *estate);
static void ExecEndPlan(PlanState *planstate, EState *estate);
static void ExecutePlan(EState *estate, PlanState *planstate,
			bool use_parallel_mode,
			CmdType operation,
			bool sendTuples,
			uint64 numberTuples,
			ScanDirection direction,
			DestReceiver *dest);
static bool ExecCheckRTEPermsModified(Oid relOid, Oid userid,
						  Bitmapset *modifiedCols,
						  AclMode requiredPerms);
static void ExecCheckXactReadOnly(PlannedStmt *plannedstmt);
static char *ExecBuildSlotValueDescription(Oid reloid,
							  TupleTableSlot *slot,
							  TupleDesc tupdesc,
							  Bitmapset *modifiedCols,
							  int maxfieldlen);
static void EvalPlanQualStart(EPQState *epqstate, EState *parentestate,
				  Plan *planTree);

static PartitionNode *BuildPartitionNodeFromRoot(Oid relid);
static void InitializeQueryPartsMetadata(PlannedStmt *plannedstmt, EState *estate);
static void AdjustReplicatedTableCounts(EState *estate);

/*
 * Note that GetUpdatedColumns() also exists in commands/trigger.c.  There does
 * not appear to be any good header to put it into, given the structures that
 * it uses, so we let them be duplicated.  Be sure to update both if one needs
 * to be changed, however.
 */
#define GetInsertedColumns(relinfo, estate) \
	(rt_fetch((relinfo)->ri_RangeTableIndex, (estate)->es_range_table)->insertedCols)
#define GetUpdatedColumns(relinfo, estate) \
	(rt_fetch((relinfo)->ri_RangeTableIndex, (estate)->es_range_table)->updatedCols)

/* end of local decls */

/*
 * For a partitioned insert target only:  
 * This type represents an entry in the per-part hash table stored at
 * estate->es_partition_state->result_partition_hash.   The table maps 
 * part OID -> ResultRelInfo and avoids repeated calculation of the
 * result information.
 */
typedef struct ResultPartHashEntry 
{
	Oid			targetid; /* OID of part relation */
	ResultRelInfo resultRelInfo;
} ResultPartHashEntry;

/* ----------------------------------------------------------------
 *		ExecutorStart
 *
 *		This routine must be called at the beginning of any execution of any
 *		query plan
 *
 * Takes a QueryDesc previously created by CreateQueryDesc (which is separate
 * only because some places use QueryDescs for utility commands).  The tupDesc
 * field of the QueryDesc is filled in to describe the tuples that will be
 * returned, and the internal fields (estate and planstate) are set up.
 *
 * eflags contains flag bits as described in executor.h.
 *
 * NB: the CurrentMemoryContext when this is called will become the parent
 * of the per-query context used for this Executor invocation.
 *
 * We provide a function hook variable that lets loadable plugins
 * get control when ExecutorStart is called.  Such a plugin would
 * normally call standard_ExecutorStart().
 *
 * MPP: In here we take care of setting up all the necessary items that
 * will be needed to service the query, such as setting up interconnect,
 * and dispatching the query. Any other items in the future
 * must be added here.
 *
 * ----------------------------------------------------------------
 */
void
ExecutorStart(QueryDesc *queryDesc, int eflags)
{
	if (ExecutorStart_hook)
		(*ExecutorStart_hook) (queryDesc, eflags);
	else
		standard_ExecutorStart(queryDesc, eflags);
}

void
standard_ExecutorStart(QueryDesc *queryDesc, int eflags)
{
	EState	   *estate;
	MemoryContext oldcontext;
	GpExecIdentity exec_identity;
	bool		shouldDispatch;
	bool		needDtx;

	/* sanity checks: queryDesc must not be started already */
	Assert(queryDesc != NULL);
	Assert(queryDesc->estate == NULL);
	Assert(queryDesc->plannedstmt != NULL);

	Assert(queryDesc->plannedstmt->intoPolicy == NULL ||
		GpPolicyIsPartitioned(queryDesc->plannedstmt->intoPolicy) ||
		GpPolicyIsReplicated(queryDesc->plannedstmt->intoPolicy));

	/* GPDB hook for collecting query info */
	if (query_info_collect_hook)
		(*query_info_collect_hook)(METRICS_QUERY_START, queryDesc);

	/**
	 * Distribute memory to operators.
	 */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		if (!IsResManagerMemoryPolicyNone() &&
			LogResManagerMemory())
		{
			elog(GP_RESMANAGER_MEMORY_LOG_LEVEL, "query requested %.0fKB of memory",
				 (double) queryDesc->plannedstmt->query_mem / 1024.0);
		}

		/**
		 * There are some statements that do not go through the resource queue, so we cannot
		 * put in a strong assert here. Someday, we should fix resource queues.
		 */
		if (queryDesc->plannedstmt->query_mem > 0)
		{
			switch(*gp_resmanager_memory_policy)
			{
				case RESMANAGER_MEMORY_POLICY_AUTO:
					PolicyAutoAssignOperatorMemoryKB(queryDesc->plannedstmt,
													 queryDesc->plannedstmt->query_mem);
					break;
				case RESMANAGER_MEMORY_POLICY_EAGER_FREE:
					PolicyEagerFreeAssignOperatorMemoryKB(queryDesc->plannedstmt,
														  queryDesc->plannedstmt->query_mem);
					break;
				default:
					Assert(IsResManagerMemoryPolicyNone());
					break;
			}
		}
	}

	/*
	 * If the transaction is read-only, we need to check if any writes are
	 * planned to non-temporary tables.  EXPLAIN is considered read-only.
	 *
	 * Don't allow writes in parallel mode.  Supporting UPDATE and DELETE
	 * would require (a) storing the combocid hash in shared memory, rather
	 * than synchronizing it just once at the start of parallelism, and (b) an
	 * alternative to heap_update()'s reliance on xmax for mutual exclusion.
	 * INSERT may have no such troubles, but we forbid it to simplify the
	 * checks.
	 *
	 * We have lower-level defenses in CommandCounterIncrement and elsewhere
	 * against performing unsafe operations in parallel mode, but this gives a
	 * more user-friendly error message.
	 *
	 * In GPDB, we must call ExecCheckXactReadOnly() in the QD even if the
	 * transaction is not read-only, because ExecCheckXactReadOnly() also
	 * determines if two-phase commit is needed.
	 */
	if ((XactReadOnly || IsInParallelMode() || Gp_role == GP_ROLE_DISPATCH) &&
		!(eflags & EXEC_FLAG_EXPLAIN_ONLY))
		ExecCheckXactReadOnly(queryDesc->plannedstmt);

	/*
	 * Build EState, switch into per-query memory context for startup.
	 */
	estate = CreateExecutorState();
	queryDesc->estate = estate;

	oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

	/**
	 * Attached the plannedstmt from queryDesc
	 */
	estate->es_plannedstmt = queryDesc->plannedstmt;

	/*
	 * Fill in external parameters, if any, from queryDesc; and allocate
	 * workspace for internal parameters
	 */
	estate->es_param_list_info = queryDesc->params;

	if (queryDesc->plannedstmt->nParamExec > 0)
		estate->es_param_exec_vals = (ParamExecData *)
			palloc0(queryDesc->plannedstmt->nParamExec * sizeof(ParamExecData));

	/*
	 * If non-read-only query, set the command ID to mark output tuples with
	 */
	switch (queryDesc->operation)
	{
		case CMD_SELECT:

			/*
			 * SELECT FOR [KEY] UPDATE/SHARE and modifying CTEs need to mark
			 * tuples
			 */
			if (queryDesc->plannedstmt->rowMarks != NIL ||
				queryDesc->plannedstmt->hasModifyingCTE)
				estate->es_output_cid = GetCurrentCommandId(true);

			/*
			 * A SELECT without modifying CTEs can't possibly queue triggers,
			 * so force skip-triggers mode. This is just a marginal efficiency
			 * hack, since AfterTriggerBeginQuery/AfterTriggerEndQuery aren't
			 * all that expensive, but we might as well do it.
			 */
			if (!queryDesc->plannedstmt->hasModifyingCTE)
				eflags |= EXEC_FLAG_SKIP_TRIGGERS;
			break;

		case CMD_INSERT:
		case CMD_DELETE:
		case CMD_UPDATE:
			estate->es_output_cid = GetCurrentCommandId(true);
			break;

		default:
			elog(ERROR, "unrecognized operation code: %d",
				 (int) queryDesc->operation);
			break;
	}

	/*
	 * Copy other important information into the EState
	 */
	estate->es_snapshot = RegisterSnapshot(queryDesc->snapshot);
	estate->es_crosscheck_snapshot = RegisterSnapshot(queryDesc->crosscheck_snapshot);
	estate->es_top_eflags = eflags;
	estate->es_instrument = queryDesc->instrument_options;
	estate->showstatctx = queryDesc->showstatctx;

	/*
	 * Shared input info is needed when ROLE_EXECUTE or sequential plan
	 */
	estate->es_sharenode = NIL;

	/*
	 * Handling of the Slice table depends on context.
	 */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		/* Set up the slice table. */
		SliceTable *sliceTable;

		sliceTable = InitSliceTable(estate, queryDesc->plannedstmt);
		estate->es_sliceTable = sliceTable;

		if (sliceTable->slices[0].gangType != GANGTYPE_UNALLOCATED ||
			sliceTable->hasMotions)
		{
			if (queryDesc->ddesc == NULL)
			{
				queryDesc->ddesc = makeNode(QueryDispatchDesc);;
				queryDesc->ddesc->useChangedAOOpts = true;
			}

			/* Pass EXPLAIN ANALYZE flag to qExecs. */
			estate->es_sliceTable->instrument_options = queryDesc->instrument_options;

			/* set our global sliceid variable for elog. */
			currentSliceId = LocallyExecutingSliceIndex(estate);

			/* InitPlan() will acquire locks by walking the entire plan
			 * tree -- we'd like to avoid acquiring the locks until
			 * *after* we've set up the interconnect */
			if (estate->es_sliceTable->hasMotions)
				estate->motionlayer_context = createMotionLayerState(queryDesc->plannedstmt->numSlices - 1);

			shouldDispatch = !(eflags & EXEC_FLAG_EXPLAIN_ONLY);
		}
		else
		{
			/* QD-only query, no dispatching required */
			shouldDispatch = false;
		}
	}
	else if (Gp_role == GP_ROLE_EXECUTE)
	{
		QueryDispatchDesc *ddesc = queryDesc->ddesc;

		shouldDispatch = false;

		/* qDisp should have sent us a slice table via MPPEXEC */
		if (ddesc && ddesc->sliceTable != NULL)
		{
			SliceTable *sliceTable;
			ExecSlice  *slice;

			sliceTable = ddesc->sliceTable;
			Assert(IsA(sliceTable, SliceTable));
			slice = &sliceTable->slices[sliceTable->localSlice];

			estate->es_sliceTable = sliceTable;
			estate->es_cursorPositions = ddesc->cursorPositions;

			estate->currentSliceId = slice->rootIndex;

			/* set our global sliceid variable for elog. */
			currentSliceId = LocallyExecutingSliceIndex(estate);

			/* Should we collect statistics for EXPLAIN ANALYZE? */
			estate->es_instrument = sliceTable->instrument_options;
			queryDesc->instrument_options = sliceTable->instrument_options;

			/* InitPlan() will acquire locks by walking the entire plan
			 * tree -- we'd like to avoid acquiring the locks until
			 * *after* we've set up the interconnect */
			if (estate->es_sliceTable->hasMotions)
			{
				estate->motionlayer_context = createMotionLayerState(queryDesc->plannedstmt->numSlices - 1);

				PG_TRY();
				{
					/*
					 * Initialize the motion layer for this query.
					 */
					Assert(!estate->interconnect_context);
					SetupInterconnect(estate);
					UpdateMotionExpectedReceivers(estate->motionlayer_context, estate->es_sliceTable);

					SIMPLE_FAULT_INJECTOR("qe_got_snapshot_and_interconnect");
					Assert(estate->interconnect_context);
				}
				PG_CATCH();
				{
					mppExecutorCleanup(queryDesc);
					PG_RE_THROW();
				}
				PG_END_TRY();
			}
		}
		else
		{
			/* local query in QE. */
		}
	}
	else
		shouldDispatch = false;

	/*
	 * We don't eliminate aliens if we don't have an MPP plan
	 * or we are executing on master.
	 *
	 * TODO: eliminate aliens even on master, if not EXPLAIN ANALYZE
	 */
	estate->eliminateAliens = execute_pruned_plan && estate->es_sliceTable && estate->es_sliceTable->hasMotions && !IS_QUERY_DISPATCHER();

	/* If the interconnect has been set up; we need to catch any
	 * errors to shut it down -- so we have to wrap InitPlan in a PG_TRY() block. */
	PG_TRY();
	{
		/*
		 * Initialize the plan state tree
		 */
		Assert(CurrentMemoryContext == estate->es_query_cxt);
		InitPlan(queryDesc, eflags);

		Assert(queryDesc->planstate);

#ifdef USE_ASSERT_CHECKING
		AssertSliceTableIsValid(estate->es_sliceTable);
#endif

		if (Debug_print_slice_table && Gp_role == GP_ROLE_DISPATCH)
			elog_node_display(DEBUG3, "slice table", estate->es_sliceTable, true);

		/*
		 * If we're running as a QE and there's a slice table in our queryDesc,
		 * then we need to finish the EState setup we prepared for back in
		 * CdbExecQuery.
		 */
		if (Gp_role == GP_ROLE_EXECUTE && estate->es_sliceTable != NULL)
		{
			MotionState *motionstate = NULL;

			/*
			 * Note that, at this point on a QE, the estate is setup (based on the
			 * slice table transmitted from the QD via MPPEXEC) so that fields
			 * es_sliceTable, cur_root_idx and es_cur_slice_idx are correct for
			 * the QE.
			 *
			 * If responsible for a non-root slice, arrange to enter the plan at the
			 * slice's sending Motion node rather than at the top.
			 */
			if (LocallyExecutingSliceIndex(estate) != RootSliceIndex(estate))
			{
				motionstate = getMotionState(queryDesc->planstate, LocallyExecutingSliceIndex(estate));
				Assert(motionstate != NULL && IsA(motionstate, MotionState));
			}

			if (Debug_print_slice_table)
				elog_node_display(DEBUG3, "slice table", estate->es_sliceTable, true);

			if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
				elog(DEBUG1, "seg%d executing slice%d under root slice%d",
					 GpIdentity.segindex,
					 LocallyExecutingSliceIndex(estate),
					 RootSliceIndex(estate));
		}

		/*
		 * if in dispatch mode, time to serialize plan and query
		 * trees, and fire off cdb_exec command to each of the qexecs
		 */
		if (shouldDispatch)
		{
			/*
			 * MPP-2869: preprocess_initplans() may
			 * dispatch. (interacted with MPP-2859, which caused an
			 * initPlan to do a write which should have happened in
			 * main body of query) We need to call
			 * ExecutorSaysTransactionDoesWrites() before any dispatch
			 * work for this query.
			 */
			needDtx = ExecutorSaysTransactionDoesWrites();
			if (needDtx)
				setupDtxTransaction();

			if (queryDesc->ddesc != NULL)
			{
				queryDesc->ddesc->sliceTable = estate->es_sliceTable;
				queryDesc->ddesc->oidAssignments = GetAssignedOidsForDispatch();
			}

			/*
			 * First, pre-execute any initPlan subplans.
			 */
			if (queryDesc->plannedstmt->nParamExec > 0)
			{
				preprocess_initplans(queryDesc);
			}

			/*
			 * This call returns after launching the threads that send the
			 * plan to the appropriate segdbs.  It does not wait for them to
			 * finish unless an error is detected before all slices have been
			 * dispatched.
			 *
			 * Main plan is parallel, send plan to it.
			 */
			if (estate->es_sliceTable->slices[0].gangType != GANGTYPE_UNALLOCATED ||
				estate->es_sliceTable->slices[0].children)
			{
				CdbDispatchPlan(queryDesc,
								estate->es_param_exec_vals,
								needDtx, true);
			}
		}

		/*
		 * Get executor identity (who does the executor serve). we can assume
		 * Forward scan direction for now just for retrieving the identity.
		 */
		if (!(eflags & EXEC_FLAG_EXPLAIN_ONLY))
			exec_identity = getGpExecIdentity(queryDesc, ForwardScanDirection, estate);
		else
			exec_identity = GP_IGNORE;

		/*
		 * If we have no slice to execute in this process, mark currentSliceId as
		 * invalid.
		 */
		if (exec_identity == GP_IGNORE)
		{
			estate->currentSliceId = -1;
			currentSliceId = -1;
		}

#ifdef USE_ASSERT_CHECKING
		/* non-root on QE */
		if (exec_identity == GP_NON_ROOT_ON_QE)
		{
			MotionState *motionState = getMotionState(queryDesc->planstate, LocallyExecutingSliceIndex(estate));

			Assert(motionState);

			Assert(IsA(motionState->ps.plan, Motion));
		}
		else
#endif
		if (exec_identity == GP_ROOT_SLICE)
		{
			/* Run a root slice. */
			if (queryDesc->planstate != NULL &&
				estate->es_sliceTable &&
				estate->es_sliceTable->slices[0].gangType == GANGTYPE_UNALLOCATED &&
				estate->es_sliceTable->slices[0].children &&
				!estate->es_interconnect_is_setup)
			{
				Assert(!estate->interconnect_context);
				SetupInterconnect(estate);
				Assert(estate->interconnect_context);
				UpdateMotionExpectedReceivers(estate->motionlayer_context, estate->es_sliceTable);
			}
		}
		else if (exec_identity != GP_IGNORE)
		{
			/* should never happen */
			Assert(!"unsupported parallel execution strategy");
		}

		if(estate->es_interconnect_is_setup)
			Assert(estate->interconnect_context != NULL);

	}
	PG_CATCH();
	{
		mppExecutorCleanup(queryDesc);
		PG_RE_THROW();
	}
	PG_END_TRY();

	if (DEBUG1 >= log_min_messages)
	{
		char		msec_str[32];
		switch (check_log_duration(msec_str, false))
		{
			case 1:
			case 2:
				ereport(LOG, (errmsg("duration to ExecutorStart end: %s ms", msec_str)));
				break;
		}
	}

	/*
	 * Set up an AFTER-trigger statement context, unless told not to, or
	 * unless it's EXPLAIN-only mode (when ExecutorFinish won't be called).
	 */
	if (!(eflags & (EXEC_FLAG_SKIP_TRIGGERS | EXEC_FLAG_EXPLAIN_ONLY)))
		AfterTriggerBeginQuery();

	MemoryContextSwitchTo(oldcontext);
}

/* ----------------------------------------------------------------
 *		ExecutorRun
 *
 *		This is the main routine of the executor module. It accepts
 *		the query descriptor from the traffic cop and executes the
 *		query plan.
 *
 *		ExecutorStart must have been called already.
 *
 *		If direction is NoMovementScanDirection then nothing is done
 *		except to start up/shut down the destination.  Otherwise,
 *		we retrieve up to 'count' tuples in the specified direction.
 *
 *		Note: count = 0 is interpreted as no portal limit, i.e., run to
 *		completion.  Also note that the count limit is only applied to
 *		retrieved tuples, not for instance to those inserted/updated/deleted
 *		by a ModifyTable plan node.
 *
 *		There is no return value, but output tuples (if any) are sent to
 *		the destination receiver specified in the QueryDesc; and the number
 *		of tuples processed at the top level can be found in
 *		estate->es_processed.
 *
 *		We provide a function hook variable that lets loadable plugins
 *		get control when ExecutorRun is called.  Such a plugin would
 *		normally call standard_ExecutorRun().
 *
 *		MPP: In here we must ensure to only run the plan and not call
 *		any setup/teardown items (unless in a CATCH block).
 *
 * ----------------------------------------------------------------
 */
void
ExecutorRun(QueryDesc *queryDesc,
			ScanDirection direction, uint64 count)
{
	if (ExecutorRun_hook)
		(*ExecutorRun_hook) (queryDesc, direction, count);
	else
		standard_ExecutorRun(queryDesc, direction, count);
}

void
standard_ExecutorRun(QueryDesc *queryDesc,
					 ScanDirection direction, uint64 count)
{
	EState	   *estate;
	CmdType		operation;
	DestReceiver *dest;
	bool		sendTuples;
	MemoryContext oldcontext;
	/*
	 * NOTE: Any local vars that are set in the PG_TRY block and examined in the
	 * PG_CATCH block should be declared 'volatile'. (setjmp shenanigans)
	 */
	ExecSlice  *currentSlice;
	GpExecIdentity		exec_identity;

	/* sanity checks */
	Assert(queryDesc != NULL);

	estate = queryDesc->estate;

	Assert(estate != NULL);
	Assert(!(estate->es_top_eflags & EXEC_FLAG_EXPLAIN_ONLY));

	/*
	 * Switch into per-query memory context
	 */
	oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

	/* Allow instrumentation of Executor overall runtime */
	if (queryDesc->totaltime)
		InstrStartNode(queryDesc->totaltime);

	/*
     * CDB: Update global slice id for log messages.
     */
    currentSlice = getCurrentSlice(estate, LocallyExecutingSliceIndex(estate));
    if (currentSlice)
    {
        if (Gp_role == GP_ROLE_EXECUTE ||
            sliceRunsOnQD(currentSlice))
            currentSliceId = currentSlice->sliceIndex;
    }

	/*
	 * extract information from the query descriptor and the query feature.
	 */
	operation = queryDesc->operation;
	dest = queryDesc->dest;

	/*
	 * startup tuple receiver, if we will be emitting tuples
	 */
	estate->es_processed = 0;
	estate->es_lastoid = InvalidOid;

	sendTuples = (queryDesc->tupDesc != NULL &&
				  (operation == CMD_SELECT ||
				   queryDesc->plannedstmt->hasReturning));

	if (sendTuples)
		(*dest->rStartup) (dest, operation, queryDesc->tupDesc);

	/*
	 * Need a try/catch block here so that if an ereport is called from
	 * within ExecutePlan, we can clean up by calling CdbCheckDispatchResult.
	 * This cleans up the asynchronous commands running through the threads launched from
	 * CdbDispatchCommand.
	 */
	PG_TRY();
	{
		/*
		 * Run the plan locally.  There are three ways;
		 *
		 * 1. Do nothing
		 * 2. Run a root slice
		 * 3. Run a non-root slice on a QE.
		 *
		 * Here we decide what is our identity -- root slice, non-root
		 * on QE or other (in which case we do nothing), and then run
		 * the plan if required. For more information see
		 * getGpExecIdentity() in execUtils.
		 */
		exec_identity = getGpExecIdentity(queryDesc, direction, estate);

		if (exec_identity == GP_IGNORE)
		{
			/* do nothing */
			estate->es_got_eos = true;
		}
		else if (exec_identity == GP_NON_ROOT_ON_QE)
		{
			/*
			 * Run a non-root slice on a QE.
			 *
			 * Since the top Plan node is a (Sending) Motion, run the plan
			 * forward to completion. The plan won't return tuples locally
			 * (tuples go out over the interconnect), so the destination is
			 * uninteresting.  The command type should be SELECT, however, to
			 * avoid other sorts of DML processing..
			 *
			 * This is the center of slice plan activity -- here we arrange to
			 * blunder into the middle of the plan rather than entering at the
			 * root.
			 */

			MotionState *motionState = getMotionState(queryDesc->planstate, LocallyExecutingSliceIndex(estate));

			Assert(motionState);

			ExecutePlan(estate,
						(PlanState *) motionState,
						queryDesc->plannedstmt->parallelModeNeeded,
						CMD_SELECT,
						sendTuples,
						0,
						ForwardScanDirection,
						dest);
		}
		else if (exec_identity == GP_ROOT_SLICE)
		{
			/*
			 * Run a root slice
			 * It corresponds to the "normal" path through the executor
			 * in that we enter the plan at the top and count on the
			 * motion nodes at the fringe of the top slice to return
			 * without ever calling nodes below them.
			 */
			ExecutePlan(estate,
						queryDesc->planstate,
						queryDesc->plannedstmt->parallelModeNeeded,
						operation,
						sendTuples,
						count,
						direction,
						dest);
		}
		else
		{
			/* should never happen */
			Assert(!"undefined parallel execution strategy");
		}
    }
	PG_CATCH();
	{
        /* If EXPLAIN ANALYZE, let qExec try to return stats to qDisp. */
        if (estate->es_sliceTable &&
            estate->es_sliceTable->instrument_options &&
            (estate->es_sliceTable->instrument_options & INSTRUMENT_CDB) &&
            Gp_role == GP_ROLE_EXECUTE)
        {
            PG_TRY();
            {
                cdbexplain_sendExecStats(queryDesc);
            }
            PG_CATCH();
            {
                /* Close down interconnect etc. */
				mppExecutorCleanup(queryDesc);
		        PG_RE_THROW();
            }
            PG_END_TRY();
        }

        /* Close down interconnect etc. */
		mppExecutorCleanup(queryDesc);
		PG_RE_THROW();
	}
	PG_END_TRY();


#ifdef FAULT_INJECTOR
	/*
	 * Allow testing of very high number of processed rows, without spending
	 * hours actually processing that many rows.
	 *
	 * Somewhat arbitrarily, only trigger this if more than 10000 rows were truly
	 * processed. This screens out some internal queries that the system might
	 * issue during planning.
	 */
	if (estate->es_processed >= 10000 && estate->es_processed <= 1000000)
	//if (estate->es_processed >= 10000)
	{
		if (FaultInjector_InjectFaultIfSet("executor_run_high_processed",
										   DDLNotSpecified,
										   "" /* databaseName */,
										   "" /* tableName */) == FaultInjectorTypeSkip)
		{
			/*
			 * For testing purposes, pretend that we have already processed
			 * almost 2^32 rows.
			 */
			estate->es_processed = UINT_MAX - 10;
		}
	}
#endif /* FAULT_INJECTOR */

	/*
	 * shutdown tuple receiver, if we started it
	 */
	if (sendTuples)
		(*dest->rShutdown) (dest);

	if (queryDesc->totaltime)
		InstrStopNode(queryDesc->totaltime, estate->es_processed);

	MemoryContextSwitchTo(oldcontext);
}

/* ----------------------------------------------------------------
 *		ExecutorFinish
 *
 *		This routine must be called after the last ExecutorRun call.
 *		It performs cleanup such as firing AFTER triggers.  It is
 *		separate from ExecutorEnd because EXPLAIN ANALYZE needs to
 *		include these actions in the total runtime.
 *
 *		We provide a function hook variable that lets loadable plugins
 *		get control when ExecutorFinish is called.  Such a plugin would
 *		normally call standard_ExecutorFinish().
 *
 * ----------------------------------------------------------------
 */
void
ExecutorFinish(QueryDesc *queryDesc)
{
	if (ExecutorFinish_hook)
		(*ExecutorFinish_hook) (queryDesc);
	else
		standard_ExecutorFinish(queryDesc);
}

void
standard_ExecutorFinish(QueryDesc *queryDesc)
{
	EState	   *estate;
	MemoryContext oldcontext;

	/* sanity checks */
	Assert(queryDesc != NULL);

	estate = queryDesc->estate;

	Assert(estate != NULL);
	Assert(!(estate->es_top_eflags & EXEC_FLAG_EXPLAIN_ONLY));

	/* This should be run once and only once per Executor instance */
	Assert(!estate->es_finished);

	/* Switch into per-query memory context */
	oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

	/* Allow instrumentation of Executor overall runtime */
	if (queryDesc->totaltime)
		InstrStartNode(queryDesc->totaltime);

	/* Run ModifyTable nodes to completion */
	ExecPostprocessPlan(estate);

	/* Execute queued AFTER triggers, unless told not to */
	if (!(estate->es_top_eflags & EXEC_FLAG_SKIP_TRIGGERS))
		AfterTriggerEndQuery(estate);

	if (queryDesc->totaltime)
		InstrStopNode(queryDesc->totaltime, 0);

	MemoryContextSwitchTo(oldcontext);

	estate->es_finished = true;
}

/* ----------------------------------------------------------------
 *		ExecutorEnd
 *
 *		This routine must be called at the end of execution of any
 *		query plan
 *
 *		We provide a function hook variable that lets loadable plugins
 *		get control when ExecutorEnd is called.  Such a plugin would
 *		normally call standard_ExecutorEnd().
 *
 * ----------------------------------------------------------------
 */
void
ExecutorEnd(QueryDesc *queryDesc)
{
	if (ExecutorEnd_hook)
		(*ExecutorEnd_hook) (queryDesc);
	else
		standard_ExecutorEnd(queryDesc);
}

void
standard_ExecutorEnd(QueryDesc *queryDesc)
{
	EState	   *estate;
	MemoryContext oldcontext;
	
	/* GPDB: whether this is a inner query for extension usage */
	bool		isInnerQuery;

	/* sanity checks */
	Assert(queryDesc != NULL);

	estate = queryDesc->estate;

	Assert(estate != NULL);

	/* GPDB: Save SPI flag first in case the memory context of plannedstmt is cleaned up*/
	isInnerQuery = estate->es_plannedstmt->metricsQueryType > TOP_LEVEL_QUERY;

	if (DEBUG1 >= log_min_messages)
	{
		char		msec_str[32];
		switch (check_log_duration(msec_str, false))
		{
			case 1:
			case 2:
				ereport(LOG, (errmsg("duration to ExecutorEnd starting: %s ms", msec_str)));
				break;
		}
	}

	if (gp_partitioning_dynamic_selection_log &&
		estate->dynamicTableScanInfo != NULL &&
		estate->dynamicTableScanInfo->numScans > 0)
	{
		for (int scanNo = 0; scanNo < estate->dynamicTableScanInfo->numScans; scanNo++)
		{
			dumpDynamicTableScanPidIndex(estate, scanNo);
		}
	}

	/*
	 * Check that ExecutorFinish was called, unless in EXPLAIN-only mode. This
	 * Assert is needed because ExecutorFinish is new as of 9.1, and callers
	 * might forget to call it.
	 */
	Assert(estate->es_finished ||
		   (estate->es_top_eflags & EXEC_FLAG_EXPLAIN_ONLY));

	/*
	 * Switch into per-query memory context to run ExecEndPlan
	 */
	oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

    /*
     * If EXPLAIN ANALYZE, qExec returns stats to qDisp now.
     */
    if (estate->es_sliceTable &&
        estate->es_sliceTable->instrument_options &&
        (estate->es_sliceTable->instrument_options & INSTRUMENT_CDB) &&
        Gp_role == GP_ROLE_EXECUTE)
        cdbexplain_sendExecStats(queryDesc);

	/*
	 * if needed, collect mpp dispatch results and tear down
	 * all mpp specific resources (e.g. interconnect).
	 */
	PG_TRY();
	{
		mppExecutorFinishup(queryDesc);
	}
	PG_CATCH();
	{
		/*
		 * we got an error. do all the necessary cleanup.
		 */
		mppExecutorCleanup(queryDesc);

		/*
		 * Remove our own query's motion layer.
		 */
		RemoveMotionLayer(estate->motionlayer_context);

		/*
		 * GPDB specific
		 * Clean the special resources created by INITPLAN.
		 * The resources have long life cycle and are used by the main plan.
		 * It's too early to clean them in preprocess_initplans.
		 */
		if (queryDesc->plannedstmt->nParamExec > 0)
		{
			postprocess_initplans(queryDesc);
		}

		/*
		 * Release EState and per-query memory context.
		 */
		FreeExecutorState(estate);

		PG_RE_THROW();
	}
	PG_END_TRY();

	/*
	 * GPDB specific
	 * Clean the special resources created by INITPLAN.
	 * The resources have long life cycle and are used by the main plan.
	 * It's too early to clean them in preprocess_initplans.
	 */
	if (queryDesc->plannedstmt->nParamExec > 0)
	{
		postprocess_initplans(queryDesc);
	}

    /*
     * If normal termination, let each operator clean itself up.
     * Otherwise don't risk it... an error might have left some
     * structures in an inconsistent state.
     */
	ExecEndPlan(queryDesc->planstate, estate);

	/*
	 * Remove our own query's motion layer.
	 */
	RemoveMotionLayer(estate->motionlayer_context);

	/* do away with our snapshots */
	UnregisterSnapshot(estate->es_snapshot);
	UnregisterSnapshot(estate->es_crosscheck_snapshot);

	/*
	 * Must switch out of context before destroying it
	 */
	MemoryContextSwitchTo(oldcontext);

	queryDesc->es_processed = estate->es_processed;
	queryDesc->es_lastoid = estate->es_lastoid;

	/*
	 * Release EState and per-query memory context
	 */
	FreeExecutorState(estate);
	
	/* GPDB hook for collecting query info */
	if (query_info_collect_hook)
		(*query_info_collect_hook)(isInnerQuery ? METRICS_INNER_QUERY_DONE : METRICS_QUERY_DONE, queryDesc);

	/* Reset queryDesc fields that no longer point to anything */
	queryDesc->tupDesc = NULL;
	queryDesc->estate = NULL;
	queryDesc->planstate = NULL;
	queryDesc->totaltime = NULL;

	if (DEBUG1 >= log_min_messages)
	{
		char		msec_str[32];
		switch (check_log_duration(msec_str, false))
		{
			case 1:
			case 2:
				ereport(LOG, (errmsg("duration to ExecutorEnd end: %s ms", msec_str)));
				break;
		}
	}

	ReportOOMConsumption();
}

/* ----------------------------------------------------------------
 *		ExecutorRewind
 *
 *		This routine may be called on an open queryDesc to rewind it
 *		to the start.
 * ----------------------------------------------------------------
 */
void
ExecutorRewind(QueryDesc *queryDesc)
{
	EState	   *estate;
	MemoryContext oldcontext;

	/* sanity checks */
	Assert(queryDesc != NULL);

	estate = queryDesc->estate;

	Assert(estate != NULL);

	/* It's probably not sensible to rescan updating queries */
	Assert(queryDesc->operation == CMD_SELECT);

	/*
	 * Switch into per-query memory context
	 */
	oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

	/*
	 * rescan plan
	 */
	ExecReScan(queryDesc->planstate);

	MemoryContextSwitchTo(oldcontext);
}


/*
 * ExecCheckRTPerms
 *		Check access permissions for all relations listed in a range table.
 *
 * Returns true if permissions are adequate.  Otherwise, throws an appropriate
 * error if ereport_on_violation is true, or simply returns false otherwise.
 *
 * Note that this does NOT address row level security policies (aka: RLS).  If
 * rows will be returned to the user as a result of this permission check
 * passing, then RLS also needs to be consulted (and check_enable_rls()).
 *
 * See rewrite/rowsecurity.c.
 */
bool
ExecCheckRTPerms(List *rangeTable, bool ereport_on_violation)
{
	ListCell   *l;
	bool		result = true;

	foreach(l, rangeTable)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(l);

		result = ExecCheckRTEPerms(rte);
		if (!result)
		{
			Assert(rte->rtekind == RTE_RELATION);
			if (ereport_on_violation)
				aclcheck_error(ACLCHECK_NO_PRIV, ACL_KIND_CLASS,
							   get_rel_name(rte->relid));
			return false;
		}
	}

	if (ExecutorCheckPerms_hook)
		result = (*ExecutorCheckPerms_hook) (rangeTable,
											 ereport_on_violation);
	return result;
}

/*
 * ExecCheckRTEPerms
 *		Check access permissions for a single RTE.
 */
bool
ExecCheckRTEPerms(RangeTblEntry *rte)
{
	AclMode		requiredPerms;
	AclMode		relPerms;
	AclMode		remainingPerms;
	Oid			relOid;
	Oid			userid;

	/*
	 * Only plain-relation RTEs need to be checked here.  Function RTEs are
	 * checked by init_fcache when the function is prepared for execution.
	 * Join, subquery, and special RTEs need no checks.
	 */
	if (rte->rtekind != RTE_RELATION)
		return true;

	/*
	 * No work if requiredPerms is empty.
	 */
	requiredPerms = rte->requiredPerms;
	if (requiredPerms == 0)
		return true;

	relOid = rte->relid;

	/*
	 * userid to check as: current user unless we have a setuid indication.
	 *
	 * Note: GetUserId() is presently fast enough that there's no harm in
	 * calling it separately for each RTE.  If that stops being true, we could
	 * call it once in ExecCheckRTPerms and pass the userid down from there.
	 * But for now, no need for the extra clutter.
	 */
	userid = rte->checkAsUser ? rte->checkAsUser : GetUserId();

	/*
	 * We must have *all* the requiredPerms bits, but some of the bits can be
	 * satisfied from column-level rather than relation-level permissions.
	 * First, remove any bits that are satisfied by relation permissions.
	 */
	relPerms = pg_class_aclmask(relOid, userid, requiredPerms, ACLMASK_ALL);
	remainingPerms = requiredPerms & ~relPerms;
	if (remainingPerms != 0)
	{
		int			col = -1;

		/*
		 * If we lack any permissions that exist only as relation permissions,
		 * we can fail straight away.
		 */
		if (remainingPerms & ~(ACL_SELECT | ACL_INSERT | ACL_UPDATE))
			return false;

		/*
		 * Check to see if we have the needed privileges at column level.
		 *
		 * Note: failures just report a table-level error; it would be nicer
		 * to report a column-level error if we have some but not all of the
		 * column privileges.
		 */
		if (remainingPerms & ACL_SELECT)
		{
			/*
			 * When the query doesn't explicitly reference any columns (for
			 * example, SELECT COUNT(*) FROM table), allow the query if we
			 * have SELECT on any column of the rel, as per SQL spec.
			 */
			if (bms_is_empty(rte->selectedCols))
			{
				if (pg_attribute_aclcheck_all(relOid, userid, ACL_SELECT,
											  ACLMASK_ANY) != ACLCHECK_OK)
					return false;
			}

			while ((col = bms_next_member(rte->selectedCols, col)) >= 0)
			{
				/* bit #s are offset by FirstLowInvalidHeapAttributeNumber */
				AttrNumber	attno = col + FirstLowInvalidHeapAttributeNumber;

				if (attno == InvalidAttrNumber)
				{
					/* Whole-row reference, must have priv on all cols */
					if (pg_attribute_aclcheck_all(relOid, userid, ACL_SELECT,
												  ACLMASK_ALL) != ACLCHECK_OK)
						return false;
				}
				else
				{
					if (pg_attribute_aclcheck(relOid, attno, userid,
											  ACL_SELECT) != ACLCHECK_OK)
						return false;
				}
			}
		}

		/*
		 * Basically the same for the mod columns, for both INSERT and UPDATE
		 * privilege as specified by remainingPerms.
		 */
		if (remainingPerms & ACL_INSERT && !ExecCheckRTEPermsModified(relOid,
																	  userid,
														   rte->insertedCols,
																 ACL_INSERT))
			return false;

		if (remainingPerms & ACL_UPDATE && !ExecCheckRTEPermsModified(relOid,
																	  userid,
															rte->updatedCols,
																 ACL_UPDATE))
			return false;
	}
	return true;
}

/*
 * ExecCheckRTEPermsModified
 *		Check INSERT or UPDATE access permissions for a single RTE (these
 *		are processed uniformly).
 */
static bool
ExecCheckRTEPermsModified(Oid relOid, Oid userid, Bitmapset *modifiedCols,
						  AclMode requiredPerms)
{
	int			col = -1;

	/*
	 * When the query doesn't explicitly update any columns, allow the query
	 * if we have permission on any column of the rel.  This is to handle
	 * SELECT FOR UPDATE as well as possible corner cases in UPDATE.
	 */
	if (bms_is_empty(modifiedCols))
	{
		if (pg_attribute_aclcheck_all(relOid, userid, requiredPerms,
									  ACLMASK_ANY) != ACLCHECK_OK)
			return false;
	}

	while ((col = bms_next_member(modifiedCols, col)) >= 0)
	{
		/* bit #s are offset by FirstLowInvalidHeapAttributeNumber */
		AttrNumber	attno = col + FirstLowInvalidHeapAttributeNumber;

		if (attno == InvalidAttrNumber)
		{
			/* whole-row reference can't happen here */
			elog(ERROR, "whole-row update is not implemented");
		}
		else
		{
			if (pg_attribute_aclcheck(relOid, attno, userid,
									  requiredPerms) != ACLCHECK_OK)
				return false;
		}
	}
	return true;
}

/*
 * Check that the query does not imply any writes to non-temp tables;
 * unless we're in parallel mode, in which case don't even allow writes
 * to temp tables.
 *
 * This function is used to check if the current statement will perform any writes.
 * It is used to enforce:
 *  (1) read-only mode (both fts and transaction isolation level read only)
 *      as well as
 *  (2) to keep track of when a distributed transaction becomes
 *      "dirty" and will require 2pc.
 *
 * Note: in a Hot Standby slave this would need to reject writes to temp
 * tables just as we do in parallel mode; but an HS slave can't have created
 * any temp tables in the first place, so no need to check that.
 *
 * In GPDB, an important side-effect of this is to call
 * ExecutorMarkTransactionDoesWrites(), if the query is not read-only. That
 * ensures that we use two-phase commit for this transaction.
 */
static void
ExecCheckXactReadOnly(PlannedStmt *plannedstmt)
{
	ListCell   *l;
	int         rti;
	char		relstorage;

	/*
	 * CREATE TABLE AS or SELECT INTO?
	 *
	 * XXX should we allow this if the destination is temp?  Considering that
	 * it would still require catalog changes, probably not.
	 */
	if (plannedstmt->intoClause != NULL)
	{
		Assert(plannedstmt->intoClause->rel);
		if (plannedstmt->intoClause->rel->relpersistence == RELPERSISTENCE_TEMP)
			ExecutorMarkTransactionDoesWrites();
		else
			PreventCommandIfReadOnly(CreateCommandTag((Node *) plannedstmt));
	}

	/*
	 * Refresh matview will write xlog.
	 */
	if (plannedstmt->refreshClause != NULL)
	{
		PreventCommandIfReadOnly(CreateCommandTag((Node *) plannedstmt));
	}

    rti = 0;
	/*
	 * Fail if write permissions are requested in parallel mode for table
	 * (temp or non-temp), otherwise fail for any non-temp table.
	 */
	foreach(l, plannedstmt->rtable)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(l);

		rti++;

		if (rte->rtekind != RTE_RELATION)
			continue;

		if ((rte->requiredPerms & (~ACL_SELECT)) == 0)
			continue;

		/*
		 * External and foreign tables don't need two phase commit which is for
		 * local mpp tables
		 */
		relstorage = get_rel_relstorage(rte->relid);
		if (relstorage == RELSTORAGE_FOREIGN)
			continue;

		if (isTempNamespace(get_rel_namespace(rte->relid)))
		{
			ExecutorMarkTransactionDoesWrites();
			continue;
		}

        /* CDB: Allow SELECT FOR SHARE/UPDATE *
         *
         */
        if ((rte->requiredPerms & ~(ACL_SELECT | ACL_SELECT_FOR_UPDATE)) == 0)
        {
        	ListCell   *cell;
        	bool foundRTI = false;

        	foreach(cell, plannedstmt->rowMarks)
			{
				RowMarkClause *rmc = lfirst(cell);
				if( rmc->rti == rti )
				{
					foundRTI = true;
					break;
				}
			}

			if (foundRTI)
				continue;
        }

		PreventCommandIfReadOnly(CreateCommandTag((Node *) plannedstmt));
	}

	if (plannedstmt->commandType != CMD_SELECT || plannedstmt->hasModifyingCTE)
		PreventCommandIfParallelMode(CreateCommandTag((Node *) plannedstmt));
}


/* ----------------------------------------------------------------
 *		InitPlan
 *
 *		Initializes the query plan: open files, allocate storage
 *		and start up the rule manager
 * ----------------------------------------------------------------
 */
static void
InitPlan(QueryDesc *queryDesc, int eflags)
{
	CmdType		operation = queryDesc->operation;
	PlannedStmt *plannedstmt = queryDesc->plannedstmt;
	Plan	   *plan = plannedstmt->planTree;
	List	   *rangeTable = plannedstmt->rtable;
	EState	   *estate = queryDesc->estate;
	PlanState  *planstate;
	TupleDesc	tupType;
	ListCell   *l;

	Assert(plannedstmt->intoPolicy == NULL ||
		GpPolicyIsPartitioned(plannedstmt->intoPolicy) ||
		GpPolicyIsReplicated(plannedstmt->intoPolicy));

	if (DEBUG1 >= log_min_messages)
	{
		char msec_str[32];
		switch (check_log_duration(msec_str, false))
		{
			case 1:
			case 2:
				ereport(LOG, (errmsg("duration to InitPlan start: %s ms", msec_str)));
				break;
			default:
				/* do nothing */
				break;
		}
	}

	/*
	 * Do permissions checks
	 */
	if (operation != CMD_SELECT || Gp_role != GP_ROLE_EXECUTE)
	{
		ExecCheckRTPerms(rangeTable, true);
	}

	/*
	 * initialize the node's execution state
	 */
	estate->es_range_table = rangeTable;
	estate->es_plannedstmt = plannedstmt;

	/*
	 * initialize result relation stuff, and open/lock the result rels.
	 *
	 * We must do this before initializing the plan tree, else we might try to
	 * do a lock upgrade if a result rel is also a source rel.
	 *
	 * CDB: Note that we need this info even if we aren't the slice that will be doing
	 * the actual updating, since it's where we learn things, such as if the row needs to
	 * contain OIDs or not.
	 */
	if (plannedstmt->resultRelations)
	{
		List	   *resultRelations = plannedstmt->resultRelations;
		int			numResultRelations = list_length(resultRelations);
		ResultRelInfo *resultRelInfos;
		ResultRelInfo *resultRelInfo;
		bool        is_conflict_update;

		is_conflict_update = IsOnConflictUpdate(plannedstmt);

		/*
		 * MPP-2879: The QEs don't pass their MPPEXEC statements through
		 * the parse (where locks would ordinarily get acquired). So we
		 * need to take some care to pick them up here (otherwise we get
		 * some very strange interactions with QE-local operations (vacuum?
		 * utility-mode ?)).
		 *
		 * NOTE: There is a comment in lmgr.c which reads forbids use of
		 * heap_open/relation_open with "NoLock" followed by use of
		 * RelationOidLock/RelationLock with a stronger lock-mode:
		 * RelationOidLock/RelationLock expect a relation to already be
		 * locked.
		 *
		 * But we also need to serialize CMD_UPDATE && CMD_DELETE to preserve
		 * order on mirrors.
		 *
		 * So we're going to ignore the "NoLock" issue above.
		 */

		/* CDB: we must promote locks for UPDATE and DELETE operations for ao table. */
		LOCKMODE    lockmode;
		lockmode = (Gp_role != GP_ROLE_EXECUTE || Gp_is_writer) ? RowExclusiveLock : NoLock;

		resultRelInfos = (ResultRelInfo *)
			palloc(numResultRelations * sizeof(ResultRelInfo));
		resultRelInfo = resultRelInfos;
		foreach(l, plannedstmt->resultRelations)
		{
			Index		resultRelationIndex = lfirst_int(l);
			Oid			resultRelationOid;
			Relation	resultRelation;

			resultRelationOid = getrelid(resultRelationIndex, rangeTable);
			if (operation == CMD_UPDATE || operation == CMD_DELETE ||
				(operation == CMD_INSERT && is_conflict_update)) /* conflictUpdate should be treated as update */
			{
				/*
				 * On QD, the lock on the table has already been taken during parsing, so if it's a child
				 * partition, we don't need to take a lock. If there a deadlock GDD will come in place
				 * and resolve the deadlock. ORCA Update / Delete plans only contains the root relation, so
				 * no locks on leaf partition are taken here. The below changes makes planner as well to not
				 * take locks on leaf partitions with GDD on.
				 * Note: With GDD off, ORCA and planner both will acquire locks on the leaf partitions.
				 */
				if (Gp_role == GP_ROLE_DISPATCH && rel_is_child_partition(resultRelationOid) && gp_enable_global_deadlock_detector)
				{
					lockmode = NoLock;
				}
				resultRelation = CdbOpenRelation(resultRelationOid, lockmode, NULL); /* lockUpgraded */
			}
			else
			{
				resultRelation = heap_open(resultRelationOid, lockmode);
			}
			InitResultRelInfo(resultRelInfo,
							  resultRelation,
							  resultRelationIndex,
							  estate->es_instrument);
			resultRelInfo++;
		}
		estate->es_result_relations = resultRelInfos;
		estate->es_num_result_relations = numResultRelations;
		/* es_result_relation_info is NULL except when within ModifyTable */
		estate->es_result_relation_info = NULL;

		/*
		 * In some occasions when inserting data into a target relations we
		 * need to pass some specific information from the QD to the QEs.
		 * we do this information exchange here, via the parseTree. For now
		 * this is used for partitioned and append-only tables.
		 */
		if (Gp_role == GP_ROLE_EXECUTE)
		{
			estate->es_result_partitions = plannedstmt->result_partitions;
		}
		else
		{
			List          *resultRelations = plannedstmt->resultRelations;
			List          *all_relids = NIL;
			Oid            relid = getrelid(linitial_int(resultRelations), rangeTable);
			bool           containRoot = false;

			if (rel_is_child_partition(relid))
				relid = rel_partition_get_master(relid);
			else
			{
				/*
				 * If root partition is in result_partitions, it must be
				 * the first element.
				 */
				containRoot = true;
			}

			estate->es_result_partitions = BuildPartitionNodeFromRoot(relid);

			/*
			 * List all the relids that may take part in this insert operation.
			 * The logic here is that:
			 *   - If root partition is in the resultRelations, all_relids
			 *     contains the root and all its all_inheritors
			 *   - Otherwise, all_relids is a map of result_partitions to
			 *     get each element's relation oid.
			 */

			if (containRoot)
			{
				/*
				 * For partition tables, if GDD is off, any DML statement on root
				 * partition, must acquire locks on the leaf partitions to avoid
				 * deadlocks.
				 *
				 * Without locking the partition relations on QD when INSERT
				 * with Planner the following dead lock scenario may happen
				 * between INSERT and AppendOnly VACUUM drop phase on the
				 * partition table:
				 *
				 * 1. AO VACUUM drop on QD: acquired AccessExclusiveLock
				 * 2. INSERT on QE: acquired RowExclusiveLock
				 * 3. AO VACUUM drop on QE: waiting for AccessExclusiveLock
				 * 4. INSERT on QD: waiting for AccessShareLock at ExecutorEnd()
				 *
				 * 2 blocks 3, 1 blocks 4, 1 and 2 will not release their locks
				 * until 3 and 4 proceed. Hence INSERT needs to Lock the partition
				 * tables on QD here (before 2) to prevent this dead lock.
				 *
				 * Deadlock can also occur in case of DELETE as below
				 * Session1: BEGIN; delete from foo_1_prt_1 WHERE c = 999999; => Holds
				 * Exclusive lock on foo_1_prt_1 on QD and marks the tuple c updated by the
				 * current transaction;
				 * Session2: BEGIN; delete from foo WHERE c = 1; => Holds Exclusive lock
				 * on foo on QD and marks the tuple c = 1 updated by current transaction
				 * Session1: DELETE FROM foo_1_prt_1 WHERE c = 1; => This wait, as Session
				 * 2 has already taken the lock, Session1 will wait to acquire the
				 * transaction lock.
				 * Session2: DELETE FROM foo WHERE c = 999999; => This waits, as Session 1
				 * has already taken the lock, Session 2 will wait to acquire the
				 * transaction lock.
				 * This will cause a deadlock.
				 * Similar scenario apply for UPDATE as well.
				 */
				lockmode = NoLock;
				if ((operation == CMD_DELETE || operation == CMD_INSERT || operation == CMD_UPDATE) &&
					!gp_enable_global_deadlock_detector &&
					rel_is_partitioned(relid))
				{
					if (operation == CMD_INSERT)
						lockmode = RowExclusiveLock;
					else
						lockmode = ExclusiveLock;
				}
				all_relids = find_all_inheritors(relid, lockmode, NULL);
			}
			else
			{
				ListCell *lc;
				int       idx;

				foreach(lc, resultRelations)
				{
					idx = lfirst_int(lc);
					all_relids = lappend_oid(all_relids,
											 getrelid(idx, rangeTable));
				}
			}

			plannedstmt->result_partitions = estate->es_result_partitions;
		}
	}
	else
	{
		/*
		 * if no result relation, then set state appropriately
		 */
		estate->es_result_relations = NULL;
		estate->es_num_result_relations = 0;
		estate->es_result_relation_info = NULL;
		estate->es_result_partitions = NULL;
	}

	estate->es_partition_state = NULL;
	if (estate->es_result_partitions)
	{
		estate->es_partition_state = createPartitionState(estate->es_result_partitions,
														  estate->es_num_result_relations);
	}

	/*
	 * If there are partitions involved in the query, initialize partitioning metadata.
	 */
	InitializeQueryPartsMetadata(plannedstmt, estate);

	/*
	 * set the number of partition selectors for every dynamic scan id
	 */
	estate->dynamicTableScanInfo->numSelectorsPerScanId = plannedstmt->numSelectorsPerScanId;

	/*
	 * Similarly, we have to lock relations selected FOR [KEY] UPDATE/SHARE
	 * before we initialize the plan tree, else we'd be risking lock upgrades.
	 * While we are at it, build the ExecRowMark list.
	 */
	estate->es_rowMarks = NIL;
	foreach(l, plannedstmt->rowMarks)
	{
		PlanRowMark *rc = (PlanRowMark *) lfirst(l);
		Oid			relid;
		Relation	relation;
		ExecRowMark *erm;
		LOCKMODE    lm;

		/* ignore "parent" rowmarks; they are irrelevant at runtime */
		if (rc->isParent)
			continue;

		/* get relation's OID (will produce InvalidOid if subquery) */
		relid = getrelid(rc->rti, rangeTable);
		lm = rc->canOptSelectLockingClause ? RowShareLock : ExclusiveLock;

		/*
		 * If you change the conditions under which rel locks are acquired
		 * here, be sure to adjust ExecOpenScanRelation to match.
		 */
		switch (rc->markType)
		{
			/*
			 * Greenplum specific behavior:
			 * The implementation of select statement with locking clause
			 * (for update | no key update | share | key share) in postgres
			 * is to hold RowShareLock on tables during parsing stage, and
			 * generate a LockRows plan node for executor to lock the tuples.
			 * It is not easy to lock tuples in Greenplum database, since
			 * tuples may be fetched through motion nodes.
			 *
			 * But when Global Deadlock Detector is enabled, and the select
			 * statement with locking clause contains only one table, we are
			 * sure that there are no motions. For such simple cases, we could
			 * make the behavior just the same as Postgres.
			 */
			case ROW_MARK_EXCLUSIVE:
			case ROW_MARK_NOKEYEXCLUSIVE:
			case ROW_MARK_SHARE:
			case ROW_MARK_KEYSHARE:
			case ROW_MARK_REFERENCE:
				relation = heap_open(relid, lm);
				break;
			case ROW_MARK_COPY:
				/* no physical table access is required */
				relation = NULL;
				break;
			default:
				elog(ERROR, "unrecognized markType: %d", rc->markType);
				relation = NULL;	/* keep compiler quiet */
				break;
		}

		/*
		 * Check that relation is a legal target for marking.
		 *
		 * In most cases parser and/or planner should have noticed this
		 * already, but they don't cover all cases.
		 */
		if (relation)
		{
			switch (relation->rd_rel->relkind)
			{
				case RELKIND_RELATION:
				case RELKIND_MATVIEW:
					/* OK */
					break;
				case RELKIND_SEQUENCE:
					/* Must disallow this because we don't vacuum sequences */
					ereport(ERROR,
							(errcode(ERRCODE_WRONG_OBJECT_TYPE),
							 errmsg("cannot lock rows in sequence \"%s\"",
									RelationGetRelationName(relation))));
					break;
				case RELKIND_TOASTVALUE:
					/* This will be disallowed in 9.1, but for now OK */
					break;
				case RELKIND_VIEW:
					/* Should not get here */
					ereport(ERROR,
							(errcode(ERRCODE_WRONG_OBJECT_TYPE),
							 errmsg("cannot lock rows in view \"%s\"",
									RelationGetRelationName(relation))));
					break;
				default:
					ereport(ERROR,
							(errcode(ERRCODE_WRONG_OBJECT_TYPE),
							 errmsg("cannot lock rows in relation \"%s\"",
									RelationGetRelationName(relation))));
					break;
			}
		}

		/* Check that relation is a legal target for marking */
		if (relation)
			CheckValidRowMarkRel(relation, rc->markType);

		erm = (ExecRowMark *) palloc(sizeof(ExecRowMark));
		erm->relation = relation;
		erm->relid = relid;
		erm->rti = rc->rti;
		erm->prti = rc->prti;
		erm->rowmarkId = rc->rowmarkId;
		erm->markType = rc->markType;
		erm->strength = rc->strength;
		erm->waitPolicy = rc->waitPolicy;
		erm->ermActive = false;
		ItemPointerSetInvalid(&(erm->curCtid));
		erm->ermExtra = NULL;
		estate->es_rowMarks = lappend(estate->es_rowMarks, erm);
	}

	/*
	 * Initialize the executor's tuple table to empty.
	 */
	estate->es_tupleTable = NIL;
	estate->es_trig_tuple_slot = NULL;
	estate->es_trig_oldtup_slot = NULL;
	estate->es_trig_newtup_slot = NULL;

	/* mark EvalPlanQual not active */
	estate->es_epqTuple = NULL;
	estate->es_epqTupleSet = NULL;
	estate->es_epqScanDone = NULL;

	/*
	 * Initialize private state information for each SubPlan.  We must do this
	 * before running ExecInitNode on the main query tree, since
	 * ExecInitSubPlan expects to be able to find these entries.
	 */
	Assert(estate->es_subplanstates == NIL);
	Bitmapset *locallyExecutableSubplans;
	Plan *start_plan_node = plannedstmt->planTree;

	estate->currentSliceId = 0;

	/*
	 * If eliminateAliens is true then we extract the local Motion node
	 * and subplans for our current slice. This enables us to call ExecInitNode
	 * for only a subset of the plan tree.
	 */
	if (estate->eliminateAliens)
	{
		Motion *m = findSenderMotion(plannedstmt, LocallyExecutingSliceIndex(estate));

		/*
		 * We may not have any motion in the current slice, e.g., in insert query
		 * the root may not have any motion.
		 */
		if (NULL != m)
		{
			start_plan_node = (Plan *) m;
			ExecSlice *sendSlice = &estate->es_sliceTable->slices[m->motionID];
			estate->currentSliceId = sendSlice->parentIndex;
		}
		/* Compute SubPlans' root plan nodes for SubPlans reachable from this plan root */
		locallyExecutableSubplans = getLocallyExecutableSubplans(plannedstmt, start_plan_node);
	}
	else
		locallyExecutableSubplans = NULL;

	int			subplan_id = 1;
	foreach(l, plannedstmt->subplans)
	{
		PlanState  *subplanstate = NULL;
		int			sp_eflags = 0;

		/*
		 * Initialize only the subplans that are reachable from our local slice.
		 * If alien elimination is not turned on, then all subplans are considered
		 * reachable.
		 */
		if (!estate->eliminateAliens ||
			bms_is_member(subplan_id, locallyExecutableSubplans))
		{
			/*
			 * A subplan will never need to do BACKWARD scan nor MARK/RESTORE.
			 *
			 * GPDB: We always set the REWIND flag, to delay eagerfree.
			 */
			sp_eflags = eflags
				& (EXEC_FLAG_EXPLAIN_ONLY | EXEC_FLAG_WITH_NO_DATA);
			sp_eflags |= EXEC_FLAG_REWIND;

			/* set our global sliceid variable for elog. */
			int			save_currentSliceId = estate->currentSliceId;

			estate->currentSliceId = estate->es_plannedstmt->subplan_sliceIds[subplan_id - 1];

			Plan	   *subplan = (Plan *) lfirst(l);
			subplanstate = ExecInitNode(subplan, estate, sp_eflags);

			estate->currentSliceId = save_currentSliceId;
		}

		estate->es_subplanstates = lappend(estate->es_subplanstates, subplanstate);

		++subplan_id;
	}

	/*
	 * If this is a query that was dispatched from the QE, install precomputed
	 * parameter values from all init plans into our EState.
	 */
	if (Gp_role == GP_ROLE_EXECUTE && queryDesc->ddesc)
		InstallDispatchedExecParams(queryDesc->ddesc, estate);

	/*
	 * Initialize the private state information for all the nodes in the query
	 * tree.  This opens files, allocates storage and leaves us ready to start
	 * processing tuples.
	 */
	planstate = ExecInitNode(start_plan_node, estate, eflags);

	queryDesc->planstate = planstate;

	Assert(queryDesc->planstate);

	/* GPDB hook for collecting query info */
	if (query_info_collect_hook)
		(*query_info_collect_hook)(METRICS_PLAN_NODE_INITIALIZE, queryDesc);

	if (RootSliceIndex(estate) != LocallyExecutingSliceIndex(estate))
		return;

	/*
	 * Get the tuple descriptor describing the type of tuples to return.
	 */
	tupType = ExecGetResultType(planstate);

	/*
	 * Initialize the junk filter if needed.  SELECT queries need a filter if
	 * there are any junk attrs in the top-level tlist.
	 */
	if (operation == CMD_SELECT)
	{
		bool		junk_filter_needed = false;
		ListCell   *tlist;

		foreach(tlist, plan->targetlist)
		{
			TargetEntry *tle = (TargetEntry *) lfirst(tlist);

			if (tle->resjunk)
			{
				junk_filter_needed = true;
				break;
			}
		}

		if (junk_filter_needed)
		{
			JunkFilter *j;

			j = ExecInitJunkFilter(planstate->plan->targetlist,
								   tupType->tdhasoid,
								   ExecInitExtraTupleSlot(estate));
			estate->es_junkFilter = j;

			/* Want to return the cleaned tuple type */
			tupType = j->jf_cleanTupType;
		}
	}

	queryDesc->tupDesc = tupType;

	/*
	 * GPDB: Hack for CTAS/MatView:
	 *   Need to switch to IntoRelDest for CTAS.
	 *   Also need to create tables in advance.
	 */
	if (queryDesc->plannedstmt->intoClause != NULL)
		intorel_initplan(queryDesc, eflags);
	else if(queryDesc->plannedstmt->copyIntoClause != NULL)
	{
		queryDesc->dest = CreateCopyDestReceiver();
		((DR_copy*)queryDesc->dest)->queryDesc = queryDesc;
	}
	else if (queryDesc->plannedstmt->refreshClause != NULL && Gp_role == GP_ROLE_EXECUTE)
		transientrel_init(queryDesc);
	if (DEBUG1 >= log_min_messages)
			{
				char		msec_str[32];
				switch (check_log_duration(msec_str, false))
				{
					case 1:
					case 2:
						ereport(LOG, (errmsg("duration to InitPlan end: %s ms", msec_str)));
						break;
				}
			}
}

/*
 * Check that a proposed result relation is a legal target for the operation
 *
 * Generally the parser and/or planner should have noticed any such mistake
 * already, but let's make sure.
 *
 * Note: when changing this function, you probably also need to look at
 * CheckValidRowMarkRel.
 */
void
CheckValidResultRel(Relation resultRel, CmdType operation)
{
	TriggerDesc *trigDesc = resultRel->trigdesc;
	FdwRoutine *fdwroutine;

	switch (resultRel->rd_rel->relkind)
	{
		case RELKIND_RELATION:
			/* OK */
			break;
		case RELKIND_SEQUENCE:
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot change sequence \"%s\"",
							RelationGetRelationName(resultRel))));
			break;
		case RELKIND_TOASTVALUE:
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot change TOAST relation \"%s\"",
							RelationGetRelationName(resultRel))));
			break;
		case RELKIND_VIEW:

			/*
			 * Okay only if there's a suitable INSTEAD OF trigger.  Messages
			 * here should match rewriteHandler.c's rewriteTargetView, except
			 * that we omit errdetail because we haven't got the information
			 * handy (and given that we really shouldn't get here anyway, it's
			 * not worth great exertion to get).
			 */
			/*
			 * GPDB_91_MERGE_FIXME: In Greenplum, views are treated as non
			 * partitioned relations, gp_distribution_policy contains no entry
			 * for views.  Consequently, flow of a ModifyTable node for a view
			 * is determined such that it is not dispatched to segments.
			 * Things get confused if the DML statement has a where clause that
			 * results in a direct dispatch to one segment.  Underlying scan
			 * nodes have direct dispatch set but when it's time to commit, the
			 * direct dispatch information is not passed on to the DTM and it
			 * sends PREPARE to all segments, causing "Distributed transaction
			 * ... not found" error.  Until this is fixed, INSTEAD OF triggers
			 * and DML on views need to be disabled.
			 */
			ereport(ERROR,
					(errcode(ERRCODE_GP_FEATURE_NOT_YET),
					 errmsg("cannot change view \"%s\"",
							RelationGetRelationName(resultRel)),
					 errhint("changing views is not supported in Greenplum")));

			switch (operation)
			{
				case CMD_INSERT:
					if (!trigDesc || !trigDesc->trig_insert_instead_row)
						ereport(ERROR,
						  (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
						   errmsg("cannot insert into view \"%s\"",
								  RelationGetRelationName(resultRel)),
						   errhint("To enable inserting into the view, provide an INSTEAD OF INSERT trigger or an unconditional ON INSERT DO INSTEAD rule.")));
					break;
				case CMD_UPDATE:
					if (!trigDesc || !trigDesc->trig_update_instead_row)
						ereport(ERROR,
						  (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
						   errmsg("cannot update view \"%s\"",
								  RelationGetRelationName(resultRel)),
						   errhint("To enable updating the view, provide an INSTEAD OF UPDATE trigger or an unconditional ON UPDATE DO INSTEAD rule.")));
					break;
				case CMD_DELETE:
					if (!trigDesc || !trigDesc->trig_delete_instead_row)
						ereport(ERROR,
						  (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
						   errmsg("cannot delete from view \"%s\"",
								  RelationGetRelationName(resultRel)),
						   errhint("To enable deleting from the view, provide an INSTEAD OF DELETE trigger or an unconditional ON DELETE DO INSTEAD rule.")));
					break;
				default:
					elog(ERROR, "unrecognized CmdType: %d", (int) operation);
					break;
			}
			break;
		case RELKIND_MATVIEW:
			if (!MatViewIncrementalMaintenanceIsEnabled())
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("cannot change materialized view \"%s\"",
								RelationGetRelationName(resultRel))));
			break;
		case RELKIND_FOREIGN_TABLE:
			/* Okay only if the FDW supports it */
			fdwroutine = GetFdwRoutineForRelation(resultRel, false);
			switch (operation)
			{
				case CMD_INSERT:
					if (fdwroutine->ExecForeignInsert == NULL)
						ereport(ERROR,
								(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							errmsg("cannot insert into foreign table \"%s\"",
								   RelationGetRelationName(resultRel))));
					if (fdwroutine->IsForeignRelUpdatable != NULL &&
						(fdwroutine->IsForeignRelUpdatable(resultRel) & (1 << CMD_INSERT)) == 0)
						ereport(ERROR,
						  (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
						errmsg("foreign table \"%s\" does not allow inserts",
							   RelationGetRelationName(resultRel))));
					break;
				case CMD_UPDATE:
					if (fdwroutine->ExecForeignUpdate == NULL)
						ereport(ERROR,
								(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
								 errmsg("cannot update foreign table \"%s\"",
										RelationGetRelationName(resultRel))));
					if (fdwroutine->IsForeignRelUpdatable != NULL &&
						(fdwroutine->IsForeignRelUpdatable(resultRel) & (1 << CMD_UPDATE)) == 0)
						ereport(ERROR,
						  (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
						errmsg("foreign table \"%s\" does not allow updates",
							   RelationGetRelationName(resultRel))));
					break;
				case CMD_DELETE:
					if (fdwroutine->ExecForeignDelete == NULL)
						ereport(ERROR,
								(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							errmsg("cannot delete from foreign table \"%s\"",
								   RelationGetRelationName(resultRel))));
					if (fdwroutine->IsForeignRelUpdatable != NULL &&
						(fdwroutine->IsForeignRelUpdatable(resultRel) & (1 << CMD_DELETE)) == 0)
						ereport(ERROR,
						  (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
						errmsg("foreign table \"%s\" does not allow deletes",
							   RelationGetRelationName(resultRel))));
					break;
				default:
					elog(ERROR, "unrecognized CmdType: %d", (int) operation);
					break;
			}
			break;

		/* GPDB additions */
		case RELKIND_AOSEGMENTS:
			if (!allowSystemTableMods)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("cannot change AO segment listing relation \"%s\"",
								RelationGetRelationName(resultRel))));
			break;
		case RELKIND_AOBLOCKDIR:
			if (!allowSystemTableMods)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("cannot change AO block directory relation \"%s\"",
								RelationGetRelationName(resultRel))));
			break;
		case RELKIND_AOVISIMAP:
			if (!allowSystemTableMods)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("cannot change AO visibility map relation \"%s\"",
								RelationGetRelationName(resultRel))));
			break;

		default:
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot change relation \"%s\"",
							RelationGetRelationName(resultRel))));
			break;
	}
}

/*
 * Check that a proposed rowmark target relation is a legal target
 *
 * In most cases parser and/or planner should have noticed this already, but
 * they don't cover all cases.
 */
static void
CheckValidRowMarkRel(Relation rel, RowMarkType markType)
{
	FdwRoutine *fdwroutine;

	switch (rel->rd_rel->relkind)
	{
		case RELKIND_RELATION:
			/* OK */
			break;
		case RELKIND_SEQUENCE:
			/* Must disallow this because we don't vacuum sequences */
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot lock rows in sequence \"%s\"",
							RelationGetRelationName(rel))));
			break;
		case RELKIND_TOASTVALUE:
			/* We could allow this, but there seems no good reason to */
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot lock rows in TOAST relation \"%s\"",
							RelationGetRelationName(rel))));
			break;
		case RELKIND_VIEW:
			/* Should not get here; planner should have expanded the view */
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot lock rows in view \"%s\"",
							RelationGetRelationName(rel))));
			break;
		case RELKIND_MATVIEW:
			/* Allow referencing a matview, but not actual locking clauses */
			if (markType != ROW_MARK_REFERENCE)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					   errmsg("cannot lock rows in materialized view \"%s\"",
							  RelationGetRelationName(rel))));
			break;
		case RELKIND_FOREIGN_TABLE:
			/* Okay only if the FDW supports it */
			fdwroutine = GetFdwRoutineForRelation(rel, false);
			if (fdwroutine->RefetchForeignRow == NULL)
				ereport(ERROR,
						(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						 errmsg("cannot lock rows in foreign table \"%s\"",
								RelationGetRelationName(rel))));
			break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot lock rows in relation \"%s\"",
							RelationGetRelationName(rel))));
			break;
	}
}

/*
 * Initialize ResultRelInfo data for one result relation
 *
 * Caution: before Postgres 9.1, this function included the relkind checking
 * that's now in CheckValidResultRel, and it also did ExecOpenIndices if
 * appropriate.  Be sure callers cover those needs.
 */
void
InitResultRelInfo(ResultRelInfo *resultRelInfo,
				  Relation resultRelationDesc,
				  Index resultRelationIndex,
				  int instrument_options)
{
	MemSet(resultRelInfo, 0, sizeof(ResultRelInfo));
	resultRelInfo->type = T_ResultRelInfo;
	resultRelInfo->ri_RangeTableIndex = resultRelationIndex;
	resultRelInfo->ri_RelationDesc = resultRelationDesc;
	resultRelInfo->ri_NumIndices = 0;
	resultRelInfo->ri_IndexRelationDescs = NULL;
	resultRelInfo->ri_IndexRelationInfo = NULL;
	/* make a copy so as not to depend on relcache info not changing... */
	resultRelInfo->ri_TrigDesc = CopyTriggerDesc(resultRelationDesc->trigdesc);
	if (resultRelInfo->ri_TrigDesc)
	{
		int			n = resultRelInfo->ri_TrigDesc->numtriggers;

		resultRelInfo->ri_TrigFunctions = (FmgrInfo *)
			palloc0(n * sizeof(FmgrInfo));
		resultRelInfo->ri_TrigWhenExprs = (List **)
			palloc0(n * sizeof(List *));
		if (instrument_options)
			resultRelInfo->ri_TrigInstrument = InstrAlloc(n, instrument_options);
	}
	else
	{
		resultRelInfo->ri_TrigFunctions = NULL;
		resultRelInfo->ri_TrigWhenExprs = NULL;
		resultRelInfo->ri_TrigInstrument = NULL;
	}
	if (resultRelationDesc->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
		resultRelInfo->ri_FdwRoutine = GetFdwRoutineForRelation(resultRelationDesc, true);
	else
		resultRelInfo->ri_FdwRoutine = NULL;
	resultRelInfo->ri_FdwState = NULL;
	resultRelInfo->ri_usesFdwDirectModify = false;
	resultRelInfo->ri_ConstraintExprs = NULL;
	resultRelInfo->ri_junkFilter = NULL;
	resultRelInfo->ri_segid_attno = InvalidAttrNumber;
	resultRelInfo->ri_action_attno = InvalidAttrNumber;
	resultRelInfo->ri_tupleoid_attno = InvalidAttrNumber;
	resultRelInfo->ri_projectReturning = NULL;
	resultRelInfo->ri_aoInsertDesc = NULL;
	resultRelInfo->ri_aocsInsertDesc = NULL;
	resultRelInfo->ri_deleteDesc = NULL;
	resultRelInfo->ri_updateDesc = NULL;
	resultRelInfo->ri_aosegno = InvalidFileSegNumber;
	resultRelInfo->bufferedTuplesSize = 0;
	resultRelInfo->nBufferedTuples = 0;
	resultRelInfo->bufferedTuples = NULL;
	resultRelInfo->biState = GetBulkInsertState();
}


void
CloseResultRelInfo(ResultRelInfo *resultRelInfo)
{
	/* end (flush) the INSERT operation in the access layer */
	if (resultRelInfo->ri_aoInsertDesc)
		appendonly_insert_finish(resultRelInfo->ri_aoInsertDesc);
	if (resultRelInfo->ri_aocsInsertDesc)
		aocs_insert_finish(resultRelInfo->ri_aocsInsertDesc);

	if (resultRelInfo->ri_deleteDesc != NULL)
	{
		if (RelationIsAoRows(resultRelInfo->ri_RelationDesc))
			appendonly_delete_finish(resultRelInfo->ri_deleteDesc);
		else
		{
			Assert(RelationIsAoCols(resultRelInfo->ri_RelationDesc));
			aocs_delete_finish(resultRelInfo->ri_deleteDesc);
		}
		if (!resultRelInfo->ri_aoInsertDesc && !resultRelInfo->ri_aocsInsertDesc)
			AORelIncrementModCount(resultRelInfo->ri_RelationDesc);
		resultRelInfo->ri_deleteDesc = NULL;
	}
	if (resultRelInfo->ri_updateDesc != NULL)
	{
		if (RelationIsAoRows(resultRelInfo->ri_RelationDesc))
			appendonly_update_finish(resultRelInfo->ri_updateDesc);
		else
		{
			Assert(RelationIsAoCols(resultRelInfo->ri_RelationDesc));
			aocs_update_finish(resultRelInfo->ri_updateDesc);
		}
		/*
		 * No need to update modcount here, because UPDATE works like
		 * delete+insert, and the insertion shows up in the tupcount.
		 */
		//AORelIncrementModCount(resultRelInfo->ri_RelationDesc);
		resultRelInfo->ri_updateDesc = NULL;
	}

	if (resultRelInfo->ri_resultSlot)
	{
		ExecDropSingleTupleTableSlot(resultRelInfo->ri_resultSlot);
		resultRelInfo->ri_resultSlot = NULL;
	}

	if (resultRelInfo->ri_PartitionParentSlot)
	{
		ExecDropSingleTupleTableSlot(resultRelInfo->ri_PartitionParentSlot);
		resultRelInfo->ri_PartitionParentSlot = NULL;
	}

	/* Close indices and then the relation itself */
	ExecCloseIndices(resultRelInfo);
	heap_close(resultRelInfo->ri_RelationDesc, NoLock);

	if (resultRelInfo->biState != NULL)
	{
		FreeBulkInsertState(resultRelInfo->biState);
		resultRelInfo->biState = NULL;
	}

	/* Recurse into partitions */
	/* Examine each hash table entry. */
	if (resultRelInfo->ri_partition_hash)
	{
		HASH_SEQ_STATUS hash_seq_status;
		ResultPartHashEntry *entry;

		hash_freeze(resultRelInfo->ri_partition_hash);
		hash_seq_init(&hash_seq_status, resultRelInfo->ri_partition_hash);
		while ((entry = hash_seq_search(&hash_seq_status)) != NULL)
		{
			CloseResultRelInfo(&entry->resultRelInfo);
		}
		/* No need for hash_seq_term() since we iterated to end. */
	}
}

/*
 * ResultRelInfoChooseSegno
 *
 * based on a list of relid->segno mapping, look for our own resultRelInfo
 * relid in the mapping and find the segfile number that this resultrel should
 * use if it is inserting into an AO relation. for any non AO relation this is
 * irrelevant and will return early.
 *
 * Note that we rely on the fact that the caller has a well constructed mapping
 * and that it includes all the relids of *any* AO relation that may insert
 * data during this transaction. For non partitioned tables the mapping list
 * will have only one element - our table. for partitioning it may have
 * multiple (depending on how many partitions are AO).
 */
void
ResultRelInfoChooseSegno(ResultRelInfo *resultRelInfo)
{
	if (resultRelInfo->ri_aosegno != InvalidFileSegNumber)
		return;		/* a target was chosen already */

	/* only relevant for AO relations */
	if(!relstorage_is_ao(RelinfoGetStorage(resultRelInfo)))
		return;

	Assert(resultRelInfo->ri_RelationDesc);

	resultRelInfo->ri_aosegno = ChooseSegnoForWrite(resultRelInfo->ri_RelationDesc);

	Assert(resultRelInfo->ri_aosegno != InvalidFileSegNumber);
}

/*
 *		ExecGetTriggerResultRel
 *
 * Get a ResultRelInfo for a trigger target relation.  Most of the time,
 * triggers are fired on one of the result relations of the query, and so
 * we can just return a member of the es_result_relations array.  (Note: in
 * self-join situations there might be multiple members with the same OID;
 * if so it doesn't matter which one we pick.)  However, it is sometimes
 * necessary to fire triggers on other relations; this happens mainly when an
 * RI update trigger queues additional triggers on other relations, which will
 * be processed in the context of the outer query.  For efficiency's sake,
 * we want to have a ResultRelInfo for those triggers too; that can avoid
 * repeated re-opening of the relation.  (It also provides a way for EXPLAIN
 * ANALYZE to report the runtimes of such triggers.)  So we make additional
 * ResultRelInfo's as needed, and save them in es_trig_target_relations.
 */
ResultRelInfo *
ExecGetTriggerResultRel(EState *estate, Oid relid)
{
	ResultRelInfo *rInfo;
	int			nr;
	ListCell   *l;
	Relation	rel;
	MemoryContext oldcontext;

	/* First, search through the query result relations */
	rInfo = estate->es_result_relations;
	nr = estate->es_num_result_relations;
	while (nr > 0)
	{
		if (RelationGetRelid(rInfo->ri_RelationDesc) == relid)
			return rInfo;
		rInfo++;
		nr--;
	}
	/* Nope, but maybe we already made an extra ResultRelInfo for it */
	foreach(l, estate->es_trig_target_relations)
	{
		rInfo = (ResultRelInfo *) lfirst(l);
		if (RelationGetRelid(rInfo->ri_RelationDesc) == relid)
			return rInfo;
	}
	/* Nope, so we need a new one */

	/*
	 * Open the target relation's relcache entry.  We assume that an
	 * appropriate lock is still held by the backend from whenever the trigger
	 * event got queued, so we need take no new lock here.  Also, we need not
	 * recheck the relkind, so no need for CheckValidResultRel.
	 */
	rel = heap_open(relid, NoLock);

	/*
	 * Make the new entry in the right context.
	 */
	oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);
	rInfo = makeNode(ResultRelInfo);
	InitResultRelInfo(rInfo,
					  rel,
					  0,		/* dummy rangetable index */
					  estate->es_instrument);
	estate->es_trig_target_relations =
		lappend(estate->es_trig_target_relations, rInfo);
	MemoryContextSwitchTo(oldcontext);

	/*
	 * Currently, we don't need any index information in ResultRelInfos used
	 * only for triggers, so no need to call ExecOpenIndices.
	 */

	return rInfo;
}

/*
 *		ExecContextForcesOids
 *
 * This is pretty grotty: when doing INSERT, UPDATE, or CREATE TABLE AS,
 * we need to ensure that result tuples have space for an OID iff they are
 * going to be stored into a relation that has OIDs.  In other contexts
 * we are free to choose whether to leave space for OIDs in result tuples
 * (we generally don't want to, but we do if a physical-tlist optimization
 * is possible).  This routine checks the plan context and returns TRUE if the
 * choice is forced, FALSE if the choice is not forced.  In the TRUE case,
 * *hasoids is set to the required value.
 *
 * One reason this is ugly is that all plan nodes in the plan tree will emit
 * tuples with space for an OID, though we really only need the topmost node
 * to do so.  However, node types like Sort don't project new tuples but just
 * return their inputs, and in those cases the requirement propagates down
 * to the input node.  Eventually we might make this code smart enough to
 * recognize how far down the requirement really goes, but for now we just
 * make all plan nodes do the same thing if the top level forces the choice.
 *
 * We assume that if we are generating tuples for INSERT or UPDATE,
 * estate->es_result_relation_info is already set up to describe the target
 * relation.  Note that in an UPDATE that spans an inheritance tree, some of
 * the target relations may have OIDs and some not.  We have to make the
 * decisions on a per-relation basis as we initialize each of the subplans of
 * the ModifyTable node, so ModifyTable has to set es_result_relation_info
 * while initializing each subplan.
 *
 * CREATE TABLE AS is even uglier, because we don't have the target relation's
 * descriptor available when this code runs; we have to look aside at the
 * flags passed to ExecutorStart().
 */
bool
ExecContextForcesOids(PlanState *planstate, bool *hasoids)
{
	/*
	 * In PostgreSQL, we check the "currently active" result relation,
	 * es_result_relation_info. In GPDB, however, the node that produces
	 * the tuple can be in a different slice than the ModifyTable node,
	 * and thanks to "alien elimination" in InitPlan, we might not have
	 * initialized the ModifyTable node at all in this process. Therefore,
	 * force OIDs if there are any result relations that need OIDs.
	 */
	int			i;

	for (i = 0; i < planstate->state->es_num_result_relations; i++)
	{
		ResultRelInfo *ri = &planstate->state->es_result_relations[i];
		Relation	rel = ri->ri_RelationDesc;

		if (rel != NULL)
		{
			*hasoids = rel->rd_rel->relhasoids;
			return true;
		}
	}

	if (planstate->state->es_top_eflags & EXEC_FLAG_WITH_OIDS)
	{
		*hasoids = true;
		return true;
	}
	if (planstate->state->es_top_eflags & EXEC_FLAG_WITHOUT_OIDS)
	{
		*hasoids = false;
		return true;
	}

	return false;
}

/* ----------------------------------------------------------------
 *		ExecPostprocessPlan
 *
 *		Give plan nodes a final chance to execute before shutdown
 * ----------------------------------------------------------------
 */
static void
ExecPostprocessPlan(EState *estate)
{
	ListCell   *lc;

	/*
	 * Make sure nodes run forward.
	 */
	estate->es_direction = ForwardScanDirection;

	/*
	 * Run any secondary ModifyTable nodes to completion, in case the main
	 * query did not fetch all rows from them.  (We do this to ensure that
	 * such nodes have predictable results.)
	 */
	foreach(lc, estate->es_auxmodifytables)
	{
		PlanState  *ps = (PlanState *) lfirst(lc);

		for (;;)
		{
			TupleTableSlot *slot;

			/* Reset the per-output-tuple exprcontext each time */
			ResetPerTupleExprContext(estate);

			slot = ExecProcNode(ps);

			if (TupIsNull(slot))
				break;
		}
	}
}

/* ----------------------------------------------------------------
 *		ExecEndPlan
 *
 *		Cleans up the query plan -- closes files and frees up storage
 *
 * NOTE: we are no longer very worried about freeing storage per se
 * in this code; FreeExecutorState should be guaranteed to release all
 * memory that needs to be released.  What we are worried about doing
 * is closing relations and dropping buffer pins.  Thus, for example,
 * tuple tables must be cleared or dropped to ensure pins are released.
 * ----------------------------------------------------------------
 */
void
ExecEndPlan(PlanState *planstate, EState *estate)
{
	ResultRelInfo *resultRelInfo;
	int			i;
	ListCell   *l;

	/*
	 * shut down the node-type-specific query processing
	 */
	if (planstate != NULL)
		ExecEndNode(planstate);

	/*
	 * for subplans too
	 */
	foreach(l, estate->es_subplanstates)
	{
		PlanState  *subplanstate = (PlanState *) lfirst(l);
		if (subplanstate != NULL)
		{
			ExecEndNode(subplanstate);
		}
	}

	/*
	 * destroy the executor's tuple table.  Actually we only care about
	 * releasing buffer pins and tupdesc refcounts; there's no need to pfree
	 * the TupleTableSlots, since the containing memory context is about to go
	 * away anyway.
	 */
	ExecResetTupleTable(estate->es_tupleTable, false);

	/* Adjust INSERT/UPDATE/DELETE count for replicated table ON QD */
	AdjustReplicatedTableCounts(estate);

	/*
	 * close the result relation(s) if any, but hold locks until xact commit.
	 */
	resultRelInfo = estate->es_result_relations;
	for (i = 0; i < estate->es_num_result_relations; i++)
	{
		CloseResultRelInfo(resultRelInfo);
		resultRelInfo++;
	}

	/*
	 * likewise close any trigger target relations
	 */
	foreach(l, estate->es_trig_target_relations)
	{
		resultRelInfo = (ResultRelInfo *) lfirst(l);
		/* Close indices and then the relation itself */
		ExecCloseIndices(resultRelInfo);
		heap_close(resultRelInfo->ri_RelationDesc, NoLock);
	}

	/*
	 * close any relations selected FOR [KEY] UPDATE/SHARE, again keeping
	 * locks
	 */
	foreach(l, estate->es_rowMarks)
	{
		ExecRowMark *erm = (ExecRowMark *) lfirst(l);

		if (erm->relation)
			heap_close(erm->relation, NoLock);
	}
}

/* ----------------------------------------------------------------
 *		ExecutePlan
 *
 *		Processes the query plan until we have retrieved 'numberTuples' tuples,
 *		moving in the specified direction.
 *
 *		Runs to completion if numberTuples is 0
 *
 * Note: the ctid attribute is a 'junk' attribute that is removed before the
 * user can see it
 * ----------------------------------------------------------------
 */
static void
ExecutePlan(EState *estate,
			PlanState *planstate,
			bool use_parallel_mode,
			CmdType operation,
			bool sendTuples,
			uint64 numberTuples,
			ScanDirection direction,
			DestReceiver *dest)
{
	TupleTableSlot *slot;
	uint64		current_tuple_count;

	/*
	 * For holdable cursor, the plan is executed without rewinding on gpdb. We
	 * need to quit if the executor has already emitted all tuples.
	 */
	if (estate->es_got_eos)
		return;

	/*
	 * initialize local variables
	 */
	current_tuple_count = 0;

	/*
	 * Set the direction.
	 */
	estate->es_direction = direction;

	/*
	 * If a tuple count was supplied, we must force the plan to run without
	 * parallelism, because we might exit early.
	 */
	if (numberTuples)
		use_parallel_mode = false;

	/*
	 * If a tuple count was supplied, we must force the plan to run without
	 * parallelism, because we might exit early.
	 */
	if (use_parallel_mode)
		EnterParallelMode();

	/*
	 * Loop until we've processed the proper number of tuples from the plan.
	 */
	for (;;)
	{
		/* Reset the per-output-tuple exprcontext */
		ResetPerTupleExprContext(estate);

		/*
		 * Execute the plan and obtain a tuple
		 */
		slot = ExecProcNode(planstate);

		/*
		 * if the tuple is null, then we assume there is nothing more to
		 * process so we just end the loop...
		 */
		if (TupIsNull(slot))
		{
			/*
			 * We got end-of-stream. We need to mark it since with a cursor
			 * end-of-stream will only be received with the fetch that
			 * returns the last tuple. ExecutorEnd needs to know if EOS was
			 * received in order to do the right cleanup.
			 */
			estate->es_got_eos = true;
			/* Allow nodes to release or shut down resources. */
			(void) ExecShutdownNode(planstate);
			break;
		}

		/*
		 * If we have a junk filter, then project a new tuple with the junk
		 * removed.
		 *
		 * Store this new "clean" tuple in the junkfilter's resultSlot.
		 * (Formerly, we stored it back over the "dirty" tuple, which is WRONG
		 * because that tuple slot has the wrong descriptor.)
		 */
		if (estate->es_junkFilter != NULL)
			slot = ExecFilterJunk(estate->es_junkFilter, slot);

		if (operation != CMD_SELECT && Gp_role == GP_ROLE_EXECUTE && !Gp_is_writer)
		{
			elog(ERROR, "INSERT/UPDATE/DELETE must be executed by a writer segworker group");
		}

		/*
		 * If we are supposed to send the tuple somewhere, do so. (In
		 * practice, this is probably always the case at this point.)
		 */
		if (sendTuples)
		{
			/*
			 * If we are not able to send the tuple, we assume the destination
			 * has closed and no more tuples can be sent. If that's the case,
			 * end the loop.
			 */
			if (!((*dest->receiveSlot) (slot, dest)))
				break;
		}

		/*
		 * Count tuples processed, if this is a SELECT.  (For other operation
		 * types, the ModifyTable plan node must count the appropriate
		 * events.)
		 */
		if (operation == CMD_SELECT)
		{
			(estate->es_processed)++;

#ifdef FAULT_INJECTOR
			/*
			 * bump es_processed using the fault injector, but only if the number rows is in a certain range
			 * this avoids bumping the counter every time after we bumped it once
			 */
			if (estate->es_processed >= 10000 && estate->es_processed <= 1000000)
			{
				if (FaultInjector_InjectFaultIfSet("executor_run_high_processed",
												   DDLNotSpecified,
												   "" /* databaseName */,
												   "" /* tableName */) == FaultInjectorTypeSkip)
				{
					/*
					 * For testing purposes, pretend that we have already processed
					 * almost 2^32 rows.
					 */
					estate->es_processed = UINT_MAX - 10;
				}
			}
#endif /* FAULT_INJECTOR */
		}

		/*
		 * check our tuple count.. if we've processed the proper number then
		 * quit, else loop again and process more tuples.  Zero numberTuples
		 * means no limit.
		 */
		current_tuple_count++;
		if (numberTuples && numberTuples == current_tuple_count)
			break;
	}

	if (use_parallel_mode)
		ExitParallelMode();
}


/*
 * ExecRelCheck --- check that tuple meets constraints for result relation
 *
 * Returns NULL if OK, else name of failed check constraint
 */
static const char *
ExecRelCheck(ResultRelInfo *resultRelInfo,
			 TupleTableSlot *slot, EState *estate)
{
	Relation	rel = resultRelInfo->ri_RelationDesc;
	int			ncheck = rel->rd_att->constr->num_check;
	ConstrCheck *check = rel->rd_att->constr->check;
	ExprContext *econtext;
	MemoryContext oldContext;
	List	   *qual;
	int			i;

	/*
	 * If first time through for this result relation, build expression
	 * nodetrees for rel's constraint expressions.  Keep them in the per-query
	 * memory context so they'll survive throughout the query.
	 */
	if (resultRelInfo->ri_ConstraintExprs == NULL)
	{
		oldContext = MemoryContextSwitchTo(estate->es_query_cxt);
		resultRelInfo->ri_ConstraintExprs =
			(List **) palloc(ncheck * sizeof(List *));
		for (i = 0; i < ncheck; i++)
		{
			/* ExecQual wants implicit-AND form */
			qual = make_ands_implicit(stringToNode(check[i].ccbin));
			resultRelInfo->ri_ConstraintExprs[i] = (List *)
				ExecPrepareExpr((Expr *) qual, estate);
		}
		MemoryContextSwitchTo(oldContext);
	}

	/*
	 * We will use the EState's per-tuple context for evaluating constraint
	 * expressions (creating it if it's not already there).
	 */
	econtext = GetPerTupleExprContext(estate);

	/* Arrange for econtext's scan tuple to be the tuple under test */
	econtext->ecxt_scantuple = slot;

	/* And evaluate the constraints */
	for (i = 0; i < ncheck; i++)
	{
		qual = resultRelInfo->ri_ConstraintExprs[i];

		/*
		 * NOTE: SQL specifies that a NULL result from a constraint expression
		 * is not to be treated as a failure.  Therefore, tell ExecQual to
		 * return TRUE for NULL.
		 */
		if (!ExecQual(qual, econtext, true))
			return check[i].ccname;
	}

	/* NULL result means no error */
	return NULL;
}

void
ExecConstraints(ResultRelInfo *resultRelInfo,
				TupleTableSlot *slot, EState *estate)
{
	Relation	rel = resultRelInfo->ri_RelationDesc;
	TupleDesc	tupdesc = RelationGetDescr(rel);
	TupleConstr *constr = tupdesc->constr;
	Bitmapset  *modifiedCols;
	Bitmapset  *insertedCols;
	Bitmapset  *updatedCols;

	Assert(constr);

	if (constr->has_not_null)
	{
		int			natts = tupdesc->natts;
		int			attrChk;

		for (attrChk = 1; attrChk <= natts; attrChk++)
		{
			if (tupdesc->attrs[attrChk - 1]->attnotnull &&
				slot_attisnull(slot, attrChk))
			{
				char	   *val_desc;
				insertedCols = GetInsertedColumns(resultRelInfo, estate);
				updatedCols = GetUpdatedColumns(resultRelInfo, estate);
				modifiedCols = bms_union(insertedCols, updatedCols);
				val_desc = ExecBuildSlotValueDescription(RelationGetRelid(rel),
														 slot,
														 tupdesc,
														 modifiedCols,
														 64);

				ereport(ERROR,
						(errcode(ERRCODE_NOT_NULL_VIOLATION),
						 errmsg("null value in column \"%s\" violates not-null constraint",
							  NameStr(tupdesc->attrs[attrChk - 1]->attname)),
						 val_desc ? errdetail("Failing row contains %s.", val_desc) : 0,
						 errtablecol(rel, attrChk)));
			}
		}
	}

	if (constr->num_check > 0)
	{
		const char *failed;

		if ((failed = ExecRelCheck(resultRelInfo, slot, estate)) != NULL)
		{
			char	   *val_desc;
			insertedCols = GetInsertedColumns(resultRelInfo, estate);
			updatedCols = GetUpdatedColumns(resultRelInfo, estate);
			modifiedCols = bms_union(insertedCols, updatedCols);
			val_desc = ExecBuildSlotValueDescription(RelationGetRelid(rel),
													 slot,
													 tupdesc,
													 modifiedCols,
													 64);
			ereport(ERROR,
					(errcode(ERRCODE_CHECK_VIOLATION),
					 errmsg("new row for relation \"%s\" violates check constraint \"%s\"",
							RelationGetRelationName(rel), failed),
			  val_desc ? errdetail("Failing row contains %s.", val_desc) : 0,
					 errtableconstraint(rel, failed)));
		}
	}
}

/*
 * ExecWithCheckOptions -- check that tuple satisfies any WITH CHECK OPTIONs
 * of the specified kind.
 *
 * Note that this needs to be called multiple times to ensure that all kinds of
 * WITH CHECK OPTIONs are handled (both those from views which have the WITH
 * CHECK OPTION set and from row level security policies).  See ExecInsert()
 * and ExecUpdate().
 */
void
ExecWithCheckOptions(WCOKind kind, ResultRelInfo *resultRelInfo,
					 TupleTableSlot *slot, EState *estate)
{
	Relation	rel = resultRelInfo->ri_RelationDesc;
	TupleDesc	tupdesc = RelationGetDescr(rel);
	ExprContext *econtext;
	ListCell   *l1,
			   *l2;

	/*
	 * We will use the EState's per-tuple context for evaluating constraint
	 * expressions (creating it if it's not already there).
	 */
	econtext = GetPerTupleExprContext(estate);

	/* Arrange for econtext's scan tuple to be the tuple under test */
	econtext->ecxt_scantuple = slot;

	/* Check each of the constraints */
	forboth(l1, resultRelInfo->ri_WithCheckOptions,
			l2, resultRelInfo->ri_WithCheckOptionExprs)
	{
		WithCheckOption *wco = (WithCheckOption *) lfirst(l1);
		ExprState  *wcoExpr = (ExprState *) lfirst(l2);

		/*
		 * Skip any WCOs which are not the kind we are looking for at this
		 * time.
		 */
		if (wco->kind != kind)
			continue;

		/*
		 * WITH CHECK OPTION checks are intended to ensure that the new tuple
		 * is visible (in the case of a view) or that it passes the
		 * 'with-check' policy (in the case of row security). If the qual
		 * evaluates to NULL or FALSE, then the new tuple won't be included in
		 * the view or doesn't pass the 'with-check' policy for the table.  We
		 * need ExecQual to return FALSE for NULL to handle the view case (the
		 * opposite of what we do above for CHECK constraints).
		 */
		if (!ExecQual((List *) wcoExpr, econtext, false))
		{
			char	   *val_desc;
			Bitmapset  *modifiedCols;
			Bitmapset  *insertedCols;
			Bitmapset  *updatedCols;

			switch (wco->kind)
			{
					/*
					 * For WITH CHECK OPTIONs coming from views, we might be
					 * able to provide the details on the row, depending on
					 * the permissions on the relation (that is, if the user
					 * could view it directly anyway).  For RLS violations, we
					 * don't include the data since we don't know if the user
					 * should be able to view the tuple as that depends on
					 * the USING policy.
					 */
				case WCO_VIEW_CHECK:
					insertedCols = GetInsertedColumns(resultRelInfo, estate);
					updatedCols = GetUpdatedColumns(resultRelInfo, estate);
					modifiedCols = bms_union(insertedCols, updatedCols);
					val_desc = ExecBuildSlotValueDescription(RelationGetRelid(rel),
															 slot,
															 tupdesc,
															 modifiedCols,
															 64);

					ereport(ERROR,
							(errcode(ERRCODE_WITH_CHECK_OPTION_VIOLATION),
					  errmsg("new row violates check option for view \"%s\"",
							 wco->relname),
							 val_desc ? errdetail("Failing row contains %s.",
												  val_desc) : 0));
					break;
				case WCO_RLS_INSERT_CHECK:
				case WCO_RLS_UPDATE_CHECK:
					if (wco->polname != NULL)
						ereport(ERROR,
								(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
								 errmsg("new row violates row-level security policy \"%s\" for table \"%s\"",
										wco->polname, wco->relname)));
					else
						ereport(ERROR,
								(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
								 errmsg("new row violates row-level security policy for table \"%s\"",
										wco->relname)));
					break;
				case WCO_RLS_CONFLICT_CHECK:
					if (wco->polname != NULL)
						ereport(ERROR,
								(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
								 errmsg("new row violates row-level security policy \"%s\" (USING expression) for table \"%s\"",
										wco->polname, wco->relname)));
					else
						ereport(ERROR,
								(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
								 errmsg("new row violates row-level security policy (USING expression) for table \"%s\"",
										wco->relname)));
					break;
				default:
					elog(ERROR, "unrecognized WCO kind: %u", wco->kind);
					break;
			}
		}
	}
}

/*
 * ExecBuildSlotValueDescription -- construct a string representing a tuple
 *
 * This is intentionally very similar to BuildIndexValueDescription, but
 * unlike that function, we truncate long field values (to at most maxfieldlen
 * bytes).  That seems necessary here since heap field values could be very
 * long, whereas index entries typically aren't so wide.
 *
 * Also, unlike the case with index entries, we need to be prepared to ignore
 * dropped columns.  We used to use the slot's tuple descriptor to decode the
 * data, but the slot's descriptor doesn't identify dropped columns, so we
 * now need to be passed the relation's descriptor.
 *
 * Note that, like BuildIndexValueDescription, if the user does not have
 * permission to view any of the columns involved, a NULL is returned.  Unlike
 * BuildIndexValueDescription, if the user has access to view a subset of the
 * column involved, that subset will be returned with a key identifying which
 * columns they are.
 */
static char *
ExecBuildSlotValueDescription(Oid reloid,
							  TupleTableSlot *slot,
							  TupleDesc tupdesc,
							  Bitmapset *modifiedCols,
							  int maxfieldlen)
{
	StringInfoData buf;
	StringInfoData collist;
	bool		write_comma = false;
	bool		write_comma_collist = false;
	int			i;
	AclResult	aclresult;
	bool		table_perm = false;
	bool		any_perm = false;

	/*
	 * Check if RLS is enabled and should be active for the relation; if so,
	 * then don't return anything.  Otherwise, go through normal permission
	 * checks.
	 */
	if (check_enable_rls(reloid, InvalidOid, true) == RLS_ENABLED)
		return NULL;

	initStringInfo(&buf);

	appendStringInfoChar(&buf, '(');

	/*
	 * Check if the user has permissions to see the row.  Table-level SELECT
	 * allows access to all columns.  If the user does not have table-level
	 * SELECT then we check each column and include those the user has SELECT
	 * rights on.  Additionally, we always include columns the user provided
	 * data for.
	 */
	aclresult = pg_class_aclcheck(reloid, GetUserId(), ACL_SELECT);
	if (aclresult != ACLCHECK_OK)
	{
		/* Set up the buffer for the column list */
		initStringInfo(&collist);
		appendStringInfoChar(&collist, '(');
	}
	else
		table_perm = any_perm = true;

	/* Make sure the tuple is fully deconstructed */
	slot_getallattrs(slot);

	for (i = 0; i < tupdesc->natts; i++)
	{
		bool		column_perm = false;
		char	   *val;
		int			vallen;

		/* ignore dropped columns */
		if (tupdesc->attrs[i]->attisdropped)
			continue;

		if (!table_perm)
		{
			/*
			 * No table-level SELECT, so need to make sure they either have
			 * SELECT rights on the column or that they have provided the data
			 * for the column.  If not, omit this column from the error
			 * message.
			 */
			aclresult = pg_attribute_aclcheck(reloid, tupdesc->attrs[i]->attnum,
											  GetUserId(), ACL_SELECT);
			if (bms_is_member(tupdesc->attrs[i]->attnum - FirstLowInvalidHeapAttributeNumber,
							  modifiedCols) || aclresult == ACLCHECK_OK)
			{
				column_perm = any_perm = true;

				if (write_comma_collist)
					appendStringInfoString(&collist, ", ");
				else
					write_comma_collist = true;

				appendStringInfoString(&collist, NameStr(tupdesc->attrs[i]->attname));
			}
		}

		if (table_perm || column_perm)
		{
			if (slot->PRIVATE_tts_isnull[i])
				val = "null";
			else
			{
				Oid			foutoid;
				bool		typisvarlena;

				getTypeOutputInfo(tupdesc->attrs[i]->atttypid,
								  &foutoid, &typisvarlena);
				val = OidOutputFunctionCall(foutoid, slot->PRIVATE_tts_values[i]);
			}

			if (write_comma)
				appendStringInfoString(&buf, ", ");
			else
				write_comma = true;

			/* truncate if needed */
			vallen = strlen(val);
			if (vallen <= maxfieldlen)
				appendStringInfoString(&buf, val);
			else
			{
				vallen = pg_mbcliplen(val, vallen, maxfieldlen);
				appendBinaryStringInfo(&buf, val, vallen);
				appendStringInfoString(&buf, "...");
			}
		}
	}

	/* If we end up with zero columns being returned, then return NULL. */
	if (!any_perm)
		return NULL;

	appendStringInfoChar(&buf, ')');

	if (!table_perm)
	{
		appendStringInfoString(&collist, ") = ");
		appendStringInfoString(&collist, buf.data);

		return collist.data;
	}

	return buf.data;
}


/*
 * ExecUpdateLockMode -- find the appropriate UPDATE tuple lock mode for a
 * given ResultRelInfo
 */
LockTupleMode
ExecUpdateLockMode(EState *estate, ResultRelInfo *relinfo)
{
	Bitmapset  *keyCols;
	Bitmapset  *updatedCols;

	/*
	 * Compute lock mode to use.  If columns that are part of the key have not
	 * been modified, then we can use a weaker lock, allowing for better
	 * concurrency.
	 */
	updatedCols = GetUpdatedColumns(relinfo, estate);
	keyCols = RelationGetIndexAttrBitmap(relinfo->ri_RelationDesc,
										 INDEX_ATTR_BITMAP_KEY);

	if (bms_overlap(keyCols, updatedCols))
		return LockTupleExclusive;

	return LockTupleNoKeyExclusive;
}

/*
 * ExecFindRowMark -- find the ExecRowMark struct for given rangetable index
 *
 * If no such struct, either return NULL or throw error depending on missing_ok
 */
ExecRowMark *
ExecFindRowMark(EState *estate, Index rti, bool missing_ok)
{
	ListCell   *lc;

	foreach(lc, estate->es_rowMarks)
	{
		ExecRowMark *erm = (ExecRowMark *) lfirst(lc);

		if (erm->rti == rti)
			return erm;
	}
	if (!missing_ok)
		elog(ERROR, "failed to find ExecRowMark for rangetable index %u", rti);
	return NULL;
}

/*
 * ExecBuildAuxRowMark -- create an ExecAuxRowMark struct
 *
 * Inputs are the underlying ExecRowMark struct and the targetlist of the
 * input plan node (not planstate node!).  We need the latter to find out
 * the column numbers of the resjunk columns.
 */
ExecAuxRowMark *
ExecBuildAuxRowMark(ExecRowMark *erm, List *targetlist)
{
	ExecAuxRowMark *aerm = (ExecAuxRowMark *) palloc0(sizeof(ExecAuxRowMark));
	char		resname[32];

	aerm->rowmark = erm;

	/* Look up the resjunk columns associated with this rowmark */
	if (erm->markType != ROW_MARK_COPY)
	{
		/* need ctid for all methods other than COPY */
		snprintf(resname, sizeof(resname), "ctid%u", erm->rowmarkId);
		aerm->ctidAttNo = ExecFindJunkAttributeInTlist(targetlist,
													   resname);
		if (!AttributeNumberIsValid(aerm->ctidAttNo))
			elog(ERROR, "could not find junk %s column", resname);
	}
	else
	{
		/* need wholerow if COPY */
		snprintf(resname, sizeof(resname), "wholerow%u", erm->rowmarkId);
		aerm->wholeAttNo = ExecFindJunkAttributeInTlist(targetlist,
														resname);
		if (!AttributeNumberIsValid(aerm->wholeAttNo))
			elog(ERROR, "could not find junk %s column", resname);
	}

	/* if child rel, need tableoid */
	if (erm->rti != erm->prti)
	{
		snprintf(resname, sizeof(resname), "tableoid%u", erm->rowmarkId);
		aerm->toidAttNo = ExecFindJunkAttributeInTlist(targetlist,
													   resname);
		if (!AttributeNumberIsValid(aerm->toidAttNo))
			elog(ERROR, "could not find junk %s column", resname);
	}

	return aerm;
}


/*
 * EvalPlanQual logic --- recheck modified tuple(s) to see if we want to
 * process the updated version under READ COMMITTED rules.
 *
 * See backend/executor/README for some info about how this works.
 */


/*
 * Check a modified tuple to see if we want to process its updated version
 * under READ COMMITTED rules.
 *
 *	estate - outer executor state data
 *	epqstate - state for EvalPlanQual rechecking
 *	relation - table containing tuple
 *	rti - rangetable index of table containing tuple
 *	lockmode - requested tuple lock mode
 *	*tid - t_ctid from the outdated tuple (ie, next updated version)
 *	priorXmax - t_xmax from the outdated tuple
 *
 * *tid is also an output parameter: it's modified to hold the TID of the
 * latest version of the tuple (note this may be changed even on failure)
 *
 * Returns a slot containing the new candidate update/delete tuple, or
 * NULL if we determine we shouldn't process the row.
 *
 * Note: properly, lockmode should be declared as enum LockTupleMode,
 * but we use "int" to avoid having to include heapam.h in executor.h.
 */
TupleTableSlot *
EvalPlanQual(EState *estate, EPQState *epqstate,
			 Relation relation, Index rti, int lockmode,
			 ItemPointer tid, TransactionId priorXmax)
{
	TupleTableSlot *slot;
	HeapTuple	copyTuple;

	Assert(rti > 0);

	/*
	 * Get and lock the updated version of the row; if fail, return NULL.
	 */
	copyTuple = EvalPlanQualFetch(estate, relation, lockmode, LockWaitBlock,
								  tid, priorXmax);

	if (copyTuple == NULL)
		return NULL;

	/*
	 * For UPDATE/DELETE we have to return tid of actual row we're executing
	 * PQ for.
	 */
	*tid = copyTuple->t_self;

	/*
	 * Need to run a recheck subquery.  Initialize or reinitialize EPQ state.
	 */
	EvalPlanQualBegin(epqstate, estate);

	/*
	 * Free old test tuple, if any, and store new tuple where relation's scan
	 * node will see it
	 */
	EvalPlanQualSetTuple(epqstate, rti, copyTuple);

	/*
	 * Fetch any non-locked source rows
	 */
	EvalPlanQualFetchRowMarks(epqstate);

	/*
	 * Run the EPQ query.  We assume it will return at most one tuple.
	 */
	slot = EvalPlanQualNext(epqstate);

	/*
	 * If we got a tuple, force the slot to materialize the tuple so that it
	 * is not dependent on any local state in the EPQ query (in particular,
	 * it's highly likely that the slot contains references to any pass-by-ref
	 * datums that may be present in copyTuple).  As with the next step, this
	 * is to guard against early re-use of the EPQ query.
	 */
	if (!TupIsNull(slot))
		(void) ExecMaterializeSlot(slot);

	/*
	 * Clear out the test tuple.  This is needed in case the EPQ query is
	 * re-used to test a tuple for a different relation.  (Not clear that can
	 * really happen, but let's be safe.)
	 */
	EvalPlanQualSetTuple(epqstate, rti, NULL);

	return slot;
}

/*
 * Fetch a copy of the newest version of an outdated tuple
 *
 *	estate - executor state data
 *	relation - table containing tuple
 *	lockmode - requested tuple lock mode
 *	wait_policy - requested lock wait policy
 *	*tid - t_ctid from the outdated tuple (ie, next updated version)
 *	priorXmax - t_xmax from the outdated tuple
 *
 * Returns a palloc'd copy of the newest tuple version, or NULL if we find
 * that there is no newest version (ie, the row was deleted not updated).
 * We also return NULL if the tuple is locked and the wait policy is to skip
 * such tuples.
 *
 * If successful, we have locked the newest tuple version, so caller does not
 * need to worry about it changing anymore.
 *
 * Note: properly, lockmode should be declared as enum LockTupleMode,
 * but we use "int" to avoid having to include heapam.h in executor.h.
 */
HeapTuple
EvalPlanQualFetch(EState *estate, Relation relation, int lockmode,
				  LockWaitPolicy wait_policy,
				  ItemPointer tid, TransactionId priorXmax)
{
	HeapTuple	copyTuple = NULL;
	HeapTupleData tuple;
	SnapshotData SnapshotDirty;

	/*
	 * fetch target tuple
	 *
	 * Loop here to deal with updated or busy tuples
	 */
	InitDirtySnapshot(SnapshotDirty);
	tuple.t_self = *tid;
	for (;;)
	{
		Buffer		buffer;

		if (heap_fetch(relation, &SnapshotDirty, &tuple, &buffer, true, NULL))
		{
			HTSU_Result test;
			HeapUpdateFailureData hufd;

			/*
			 * If xmin isn't what we're expecting, the slot must have been
			 * recycled and reused for an unrelated tuple.  This implies that
			 * the latest version of the row was deleted, so we need do
			 * nothing.  (Should be safe to examine xmin without getting
			 * buffer's content lock.  We assume reading a TransactionId to be
			 * atomic, and Xmin never changes in an existing tuple, except to
			 * invalid or frozen, and neither of those can match priorXmax.)
			 */
			if (!TransactionIdEquals(HeapTupleHeaderGetXmin(tuple.t_data),
									 priorXmax))
			{
				ReleaseBuffer(buffer);
				return NULL;
			}

			/* otherwise xmin should not be dirty... */
			if (TransactionIdIsValid(SnapshotDirty.xmin))
				elog(ERROR, "t_xmin is uncommitted in tuple to be updated");

			/*
			 * If tuple is being updated by other transaction then we have to
			 * wait for its commit/abort, or die trying.
			 */
			if (TransactionIdIsValid(SnapshotDirty.xmax))
			{
				ReleaseBuffer(buffer);
				switch (wait_policy)
				{
					case LockWaitBlock:
						XactLockTableWait(SnapshotDirty.xmax,
										  relation, &tuple.t_self,
										  XLTW_FetchUpdated);
						break;
					case LockWaitSkip:
						if (!ConditionalXactLockTableWait(SnapshotDirty.xmax))
							return NULL;		/* skip instead of waiting */
						break;
					case LockWaitError:
						if (!ConditionalXactLockTableWait(SnapshotDirty.xmax))
							ereport(ERROR,
									(errcode(ERRCODE_LOCK_NOT_AVAILABLE),
									 errmsg("could not obtain lock on row in relation \"%s\"",
										RelationGetRelationName(relation))));
						break;
				}
				continue;		/* loop back to repeat heap_fetch */
			}

			/*
			 * If tuple was inserted by our own transaction, we have to check
			 * cmin against es_output_cid: cmin >= current CID means our
			 * command cannot see the tuple, so we should ignore it. Otherwise
			 * heap_lock_tuple() will throw an error, and so would any later
			 * attempt to update or delete the tuple.  (We need not check cmax
			 * because HeapTupleSatisfiesDirty will consider a tuple deleted
			 * by our transaction dead, regardless of cmax.) We just checked
			 * that priorXmax == xmin, so we can test that variable instead of
			 * doing HeapTupleHeaderGetXmin again.
			 */
			if (TransactionIdIsCurrentTransactionId(priorXmax) &&
				HeapTupleHeaderGetCmin(tuple.t_data) >= estate->es_output_cid)
			{
				ReleaseBuffer(buffer);
				return NULL;
			}

			/*
			 * This is a live tuple, so now try to lock it.
			 */
			test = heap_lock_tuple(relation, &tuple,
								   estate->es_output_cid,
								   lockmode, wait_policy,
								   false, &buffer, &hufd);
			/* We now have two pins on the buffer, get rid of one */
			ReleaseBuffer(buffer);

			switch (test)
			{
				case HeapTupleSelfUpdated:

					/*
					 * The target tuple was already updated or deleted by the
					 * current command, or by a later command in the current
					 * transaction.  We *must* ignore the tuple in the former
					 * case, so as to avoid the "Halloween problem" of
					 * repeated update attempts.  In the latter case it might
					 * be sensible to fetch the updated tuple instead, but
					 * doing so would require changing heap_update and
					 * heap_delete to not complain about updating "invisible"
					 * tuples, which seems pretty scary (heap_lock_tuple will
					 * not complain, but few callers expect
					 * HeapTupleInvisible, and we're not one of them).  So for
					 * now, treat the tuple as deleted and do not process.
					 */
					ReleaseBuffer(buffer);
					return NULL;

				case HeapTupleMayBeUpdated:
					/* successfully locked */
					break;

				case HeapTupleUpdated:
					ReleaseBuffer(buffer);
					if (IsolationUsesXactSnapshot())
						ereport(ERROR,
								(errcode(ERRCODE_T_R_SERIALIZATION_FAILURE),
								 errmsg("could not serialize access due to concurrent update")));

					/* Should not encounter speculative tuple on recheck */
					Assert(!HeapTupleHeaderIsSpeculative(tuple.t_data));
					if (!ItemPointerEquals(&hufd.ctid, &tuple.t_self))
					{
						/* it was updated, so look at the updated version */
						tuple.t_self = hufd.ctid;
						/* updated row should have xmin matching this xmax */
						priorXmax = hufd.xmax;
						continue;
					}
					/* tuple was deleted, so give up */
					return NULL;

				case HeapTupleWouldBlock:
					ReleaseBuffer(buffer);
					return NULL;

				case HeapTupleInvisible:
					elog(ERROR, "attempted to lock invisible tuple");
					break;

				default:
					ReleaseBuffer(buffer);
					elog(ERROR, "unrecognized heap_lock_tuple status: %u",
						 test);
					return NULL;	/* keep compiler quiet */
			}

			/*
			 * We got tuple - now copy it for use by recheck query.
			 */
			copyTuple = heap_copytuple(&tuple);
			ReleaseBuffer(buffer);
			break;
		}

		/*
		 * If the referenced slot was actually empty, the latest version of
		 * the row must have been deleted, so we need do nothing.
		 */
		if (tuple.t_data == NULL)
		{
			ReleaseBuffer(buffer);
			return NULL;
		}

		/*
		 * As above, if xmin isn't what we're expecting, do nothing.
		 */
		if (!TransactionIdEquals(HeapTupleHeaderGetXmin(tuple.t_data),
								 priorXmax))
		{
			ReleaseBuffer(buffer);
			return NULL;
		}

		/*
		 * If we get here, the tuple was found but failed SnapshotDirty.
		 * Assuming the xmin is either a committed xact or our own xact (as it
		 * certainly should be if we're trying to modify the tuple), this must
		 * mean that the row was updated or deleted by either a committed xact
		 * or our own xact.  If it was deleted, we can ignore it; if it was
		 * updated then chain up to the next version and repeat the whole
		 * process.
		 *
		 * As above, it should be safe to examine xmax and t_ctid without the
		 * buffer content lock, because they can't be changing.
		 */
		if (ItemPointerEquals(&tuple.t_self, &tuple.t_data->t_ctid))
		{
			/* deleted, so forget about it */
			ReleaseBuffer(buffer);
			return NULL;
		}

		/* updated, so look at the updated row */
		tuple.t_self = tuple.t_data->t_ctid;
		/* updated row should have xmin matching this xmax */
		priorXmax = HeapTupleHeaderGetUpdateXid(tuple.t_data);
		ReleaseBuffer(buffer);
		/* loop back to fetch next in chain */
	}

	/*
	 * Return the copied tuple
	 */
	return copyTuple;
}

/*
 * EvalPlanQualInit -- initialize during creation of a plan state node
 * that might need to invoke EPQ processing.
 *
 * Note: subplan/auxrowmarks can be NULL/NIL if they will be set later
 * with EvalPlanQualSetPlan.
 */
void
EvalPlanQualInit(EPQState *epqstate, EState *estate,
				 Plan *subplan, List *auxrowmarks, int epqParam)
{
	/* Mark the EPQ state inactive */
	epqstate->estate = NULL;
	epqstate->planstate = NULL;
	epqstate->origslot = NULL;
	/* ... and remember data that EvalPlanQualBegin will need */
	epqstate->plan = subplan;
	epqstate->arowMarks = auxrowmarks;
	epqstate->epqParam = epqParam;
}

/*
 * EvalPlanQualSetPlan -- set or change subplan of an EPQState.
 *
 * We need this so that ModifyTable can deal with multiple subplans.
 */
void
EvalPlanQualSetPlan(EPQState *epqstate, Plan *subplan, List *auxrowmarks)
{
	/* If we have a live EPQ query, shut it down */
	EvalPlanQualEnd(epqstate);
	/* And set/change the plan pointer */
	epqstate->plan = subplan;
	/* The rowmarks depend on the plan, too */
	epqstate->arowMarks = auxrowmarks;
}

/*
 * Install one test tuple into EPQ state, or clear test tuple if tuple == NULL
 *
 * NB: passed tuple must be palloc'd; it may get freed later
 */
void
EvalPlanQualSetTuple(EPQState *epqstate, Index rti, HeapTuple tuple)
{
	EState	   *estate = epqstate->estate;

	Assert(rti > 0);

	/*
	 * free old test tuple, if any, and store new tuple where relation's scan
	 * node will see it
	 */
	if (estate->es_epqTuple[rti - 1] != NULL)
		heap_freetuple(estate->es_epqTuple[rti - 1]);
	estate->es_epqTuple[rti - 1] = tuple;
	estate->es_epqTupleSet[rti - 1] = true;
}

/*
 * Fetch back the current test tuple (if any) for the specified RTI
 */
HeapTuple
EvalPlanQualGetTuple(EPQState *epqstate, Index rti)
{
	EState	   *estate = epqstate->estate;

	Assert(rti > 0);

	return estate->es_epqTuple[rti - 1];
}

/*
 * Fetch the current row values for any non-locked relations that need
 * to be scanned by an EvalPlanQual operation.  origslot must have been set
 * to contain the current result row (top-level row) that we need to recheck.
 */
void
EvalPlanQualFetchRowMarks(EPQState *epqstate)
{
	ListCell   *l;

	Assert(epqstate->origslot != NULL);

	foreach(l, epqstate->arowMarks)
	{
		ExecAuxRowMark *aerm = (ExecAuxRowMark *) lfirst(l);
		ExecRowMark *erm = aerm->rowmark;
		Datum		datum;
		bool		isNull;
		HeapTupleData tuple;

		if (RowMarkRequiresRowShareLock(erm->markType))
			elog(ERROR, "EvalPlanQual doesn't support locking rowmarks");

		/* clear any leftover test tuple for this rel */
		EvalPlanQualSetTuple(epqstate, erm->rti, NULL);

		/* if child rel, must check whether it produced this row */
		if (erm->rti != erm->prti)
		{
			Oid			tableoid;

			datum = ExecGetJunkAttribute(epqstate->origslot,
										 aerm->toidAttNo,
										 &isNull);
			/* non-locked rels could be on the inside of outer joins */
			if (isNull)
				continue;
			tableoid = DatumGetObjectId(datum);

			Assert(OidIsValid(erm->relid));
			if (tableoid != erm->relid)
			{
				/* this child is inactive right now */
				continue;
			}
		}

		if (erm->markType == ROW_MARK_REFERENCE)
		{
			HeapTuple	copyTuple;

			Assert(erm->relation != NULL);

			/* fetch the tuple's ctid */
			datum = ExecGetJunkAttribute(epqstate->origslot,
										 aerm->ctidAttNo,
										 &isNull);
			/* non-locked rels could be on the inside of outer joins */
			if (isNull)
				continue;

			/* fetch requests on foreign tables must be passed to their FDW */
			if (erm->relation->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
			{
				FdwRoutine *fdwroutine;
				bool		updated = false;

				fdwroutine = GetFdwRoutineForRelation(erm->relation, false);
				/* this should have been checked already, but let's be safe */
				if (fdwroutine->RefetchForeignRow == NULL)
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						   errmsg("cannot lock rows in foreign table \"%s\"",
								  RelationGetRelationName(erm->relation))));
				copyTuple = fdwroutine->RefetchForeignRow(epqstate->estate,
														  erm,
														  datum,
														  &updated);
				if (copyTuple == NULL)
					elog(ERROR, "failed to fetch tuple for EvalPlanQual recheck");

				/*
				 * Ideally we'd insist on updated == false here, but that
				 * assumes that FDWs can track that exactly, which they might
				 * not be able to.  So just ignore the flag.
				 */
			}
			else
			{
				/* ordinary table, fetch the tuple */
				Buffer		buffer;

				tuple.t_self = *((ItemPointer) DatumGetPointer(datum));
				if (!heap_fetch(erm->relation, SnapshotAny, &tuple, &buffer,
								false, NULL))
					elog(ERROR, "failed to fetch tuple for EvalPlanQual recheck");

				/* successful, copy tuple */
				copyTuple = heap_copytuple(&tuple);
				ReleaseBuffer(buffer);
			}

			/* store tuple */
			EvalPlanQualSetTuple(epqstate, erm->rti, copyTuple);
		}
		else
		{
			HeapTupleHeader td;

			Assert(erm->markType == ROW_MARK_COPY);

			/* fetch the whole-row Var for the relation */
			datum = ExecGetJunkAttribute(epqstate->origslot,
										 aerm->wholeAttNo,
										 &isNull);
			/* non-locked rels could be on the inside of outer joins */
			if (isNull)
				continue;
			td = DatumGetHeapTupleHeader(datum);

			/* build a temporary HeapTuple control structure */
			tuple.t_len = HeapTupleHeaderGetDatumLength(td);
			tuple.t_data = td;
			/* also copy t_ctid in case there's valid data there */
			tuple.t_self = td->t_ctid;

			/* copy and store tuple */
			EvalPlanQualSetTuple(epqstate, erm->rti,
								 heap_copytuple(&tuple));
		}
	}
}

/*
 * Fetch the next row (if any) from EvalPlanQual testing
 *
 * (In practice, there should never be more than one row...)
 */
TupleTableSlot *
EvalPlanQualNext(EPQState *epqstate)
{
	MemoryContext oldcontext;
	TupleTableSlot *slot;

	oldcontext = MemoryContextSwitchTo(epqstate->estate->es_query_cxt);
	slot = ExecProcNode(epqstate->planstate);
	MemoryContextSwitchTo(oldcontext);

	return slot;
}

/*
 * Initialize or reset an EvalPlanQual state tree
 */
void
EvalPlanQualBegin(EPQState *epqstate, EState *parentestate)
{
	EState	   *estate = epqstate->estate;

	if (estate == NULL)
	{
		/* First time through, so create a child EState */
		EvalPlanQualStart(epqstate, parentestate, epqstate->plan);
	}
	else
	{
		/*
		 * We already have a suitable child EPQ tree, so just reset it.
		 */
		int			rtsize = list_length(parentestate->es_range_table);
		PlanState  *planstate = epqstate->planstate;

		MemSet(estate->es_epqScanDone, 0, rtsize * sizeof(bool));

		/* Recopy current values of parent parameters */
		if (parentestate->es_plannedstmt->nParamExec > 0)
		{
			int			i;

			/*
			 * Force evaluation of any InitPlan outputs that could be needed
			 * by the subplan, just in case they got reset since
			 * EvalPlanQualStart (see comments therein).
			 */
			ExecSetParamPlanMulti(planstate->plan->extParam,
								  GetPerTupleExprContext(parentestate),
								  NULL);

			i = parentestate->es_plannedstmt->nParamExec;

			while (--i >= 0)
			{
				/* copy value if any, but not execPlan link */
				estate->es_param_exec_vals[i].value =
					parentestate->es_param_exec_vals[i].value;
				estate->es_param_exec_vals[i].isnull =
					parentestate->es_param_exec_vals[i].isnull;
			}
		}

		/*
		 * Mark child plan tree as needing rescan at all scan nodes.  The
		 * first ExecProcNode will take care of actually doing the rescan.
		 */
		planstate->chgParam = bms_add_member(planstate->chgParam,
											 epqstate->epqParam);
	}
}

/*
 * Start execution of an EvalPlanQual plan tree.
 *
 * This is a cut-down version of ExecutorStart(): we copy some state from
 * the top-level estate rather than initializing it fresh.
 */
static void
EvalPlanQualStart(EPQState *epqstate, EState *parentestate, Plan *planTree)
{
	EState	   *estate;
	int			rtsize;
	MemoryContext oldcontext;
	ListCell   *l;

	rtsize = list_length(parentestate->es_range_table);

	epqstate->estate = estate = CreateExecutorState();

	oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

	/*
	 * Child EPQ EStates share the parent's copy of unchanging state such as
	 * the snapshot, rangetable, result-rel info, and external Param info.
	 * They need their own copies of local state, including a tuple table,
	 * es_param_exec_vals, etc.
	 *
	 * The ResultRelInfo array management is trickier than it looks.  We
	 * create a fresh array for the child but copy all the content from the
	 * parent.  This is because it's okay for the child to share any
	 * per-relation state the parent has already created --- but if the child
	 * sets up any ResultRelInfo fields, such as its own junkfilter, that
	 * state must *not* propagate back to the parent.  (For one thing, the
	 * pointed-to data is in a memory context that won't last long enough.)
	 */
	estate->es_direction = ForwardScanDirection;
	estate->es_snapshot = parentestate->es_snapshot;
	estate->es_crosscheck_snapshot = parentestate->es_crosscheck_snapshot;
	estate->es_range_table = parentestate->es_range_table;
	estate->es_plannedstmt = parentestate->es_plannedstmt;
	estate->es_junkFilter = parentestate->es_junkFilter;
	estate->es_output_cid = parentestate->es_output_cid;
	if (parentestate->es_num_result_relations > 0)
	{
		int			numResultRelations = parentestate->es_num_result_relations;
		ResultRelInfo *resultRelInfos;

		resultRelInfos = (ResultRelInfo *)
			palloc(numResultRelations * sizeof(ResultRelInfo));
		memcpy(resultRelInfos, parentestate->es_result_relations,
			   numResultRelations * sizeof(ResultRelInfo));
		estate->es_result_relations = resultRelInfos;
		estate->es_num_result_relations = numResultRelations;
	}
	/* es_result_relation_info must NOT be copied */
	/* es_trig_target_relations must NOT be copied */
	estate->es_rowMarks = parentestate->es_rowMarks;
	estate->es_top_eflags = parentestate->es_top_eflags;
	estate->es_instrument = parentestate->es_instrument;
	/* es_auxmodifytables must NOT be copied */

	/*
	 * The external param list is simply shared from parent.  The internal
	 * param workspace has to be local state, but we copy the initial values
	 * from the parent, so as to have access to any param values that were
	 * already set from other parts of the parent's plan tree.
	 */
	estate->es_param_list_info = parentestate->es_param_list_info;
	if (parentestate->es_plannedstmt->nParamExec > 0)
	{
		int			i;

		/*
		 * Force evaluation of any InitPlan outputs that could be needed by
		 * the subplan.  (With more complexity, maybe we could postpone this
		 * till the subplan actually demands them, but it doesn't seem worth
		 * the trouble; this is a corner case already, since usually the
		 * InitPlans would have been evaluated before reaching EvalPlanQual.)
		 *
		 * This will not touch output params of InitPlans that occur somewhere
		 * within the subplan tree, only those that are attached to the
		 * ModifyTable node or above it and are referenced within the subplan.
		 * That's OK though, because the planner would only attach such
		 * InitPlans to a lower-level SubqueryScan node, and EPQ execution
		 * will not descend into a SubqueryScan.
		 *
		 * The EState's per-output-tuple econtext is sufficiently short-lived
		 * for this, since it should get reset before there is any chance of
		 * doing EvalPlanQual again.
		 */
		ExecSetParamPlanMulti(planTree->extParam,
							  GetPerTupleExprContext(parentestate),
							  NULL);

		/* now make the internal param workspace ... */
		i = parentestate->es_plannedstmt->nParamExec;
		estate->es_param_exec_vals = (ParamExecData *)
			palloc0(i * sizeof(ParamExecData));
		/* ... and copy down all values, whether really needed or not */
		while (--i >= 0)
		{
			/* copy value if any, but not execPlan link */
			estate->es_param_exec_vals[i].value =
				parentestate->es_param_exec_vals[i].value;
			estate->es_param_exec_vals[i].isnull =
				parentestate->es_param_exec_vals[i].isnull;
		}
	}

	/*
	 * Each EState must have its own es_epqScanDone state, but if we have
	 * nested EPQ checks they should share es_epqTuple arrays.  This allows
	 * sub-rechecks to inherit the values being examined by an outer recheck.
	 */
	estate->es_epqScanDone = (bool *) palloc0(rtsize * sizeof(bool));
	if (parentestate->es_epqTuple != NULL)
	{
		estate->es_epqTuple = parentestate->es_epqTuple;
		estate->es_epqTupleSet = parentestate->es_epqTupleSet;
	}
	else
	{
		estate->es_epqTuple = (HeapTuple *)
			palloc0(rtsize * sizeof(HeapTuple));
		estate->es_epqTupleSet = (bool *)
			palloc0(rtsize * sizeof(bool));
	}

	/*
	 * Each estate also has its own tuple table.
	 */
	estate->es_tupleTable = NIL;

	/*
	 * Initialize private state information for each SubPlan.  We must do this
	 * before running ExecInitNode on the main query tree, since
	 * ExecInitSubPlan expects to be able to find these entries. Some of the
	 * SubPlans might not be used in the part of the plan tree we intend to
	 * run, but since it's not easy to tell which, we just initialize them
	 * all.
	 */
	Assert(estate->es_subplanstates == NIL);
	foreach(l, parentestate->es_plannedstmt->subplans)
	{
		Plan	   *subplan = (Plan *) lfirst(l);
		PlanState  *subplanstate;

		subplanstate = ExecInitNode(subplan, estate, 0);
		estate->es_subplanstates = lappend(estate->es_subplanstates,
										   subplanstate);
	}

	/*
	 * Initialize the private state information for all the nodes in the part
	 * of the plan tree we need to run.  This opens files, allocates storage
	 * and leaves us ready to start processing tuples.
	 */
	epqstate->planstate = ExecInitNode(planTree, estate, 0);

	MemoryContextSwitchTo(oldcontext);
}

/*
 * EvalPlanQualEnd -- shut down at termination of parent plan state node,
 * or if we are done with the current EPQ child.
 *
 * This is a cut-down version of ExecutorEnd(); basically we want to do most
 * of the normal cleanup, but *not* close result relations (which we are
 * just sharing from the outer query).  We do, however, have to close any
 * trigger target relations that got opened, since those are not shared.
 * (There probably shouldn't be any of the latter, but just in case...)
 */
void
EvalPlanQualEnd(EPQState *epqstate)
{
	EState	   *estate = epqstate->estate;
	MemoryContext oldcontext;
	ListCell   *l;

	if (estate == NULL)
		return;					/* idle, so nothing to do */

	oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

	ExecEndNode(epqstate->planstate);

	foreach(l, estate->es_subplanstates)
	{
		PlanState  *subplanstate = (PlanState *) lfirst(l);

		ExecEndNode(subplanstate);
	}

	/* throw away the per-estate tuple table */
	ExecResetTupleTable(estate->es_tupleTable, false);

	/* close any trigger target relations attached to this EState */
	foreach(l, estate->es_trig_target_relations)
	{
		ResultRelInfo *resultRelInfo = (ResultRelInfo *) lfirst(l);

		/* Close indices and then the relation itself */
		ExecCloseIndices(resultRelInfo);
		heap_close(resultRelInfo->ri_RelationDesc, NoLock);
	}

	MemoryContextSwitchTo(oldcontext);

	FreeExecutorState(estate);

	/* Mark EPQState idle */
	epqstate->estate = NULL;
	epqstate->planstate = NULL;
	epqstate->origslot = NULL;
}

ResultRelInfo *
targetid_get_partition(Oid targetid, EState *estate, bool openIndices)
{
	ResultRelInfo *parentInfo = estate->es_result_relations;
	ResultRelInfo *childInfo = estate->es_result_relations;
	ResultPartHashEntry *entry;
	bool		found;

	/*
	 * If the resolved target is the table/partition that was named as the
	 * update target itself, don't create a new ResultRelInfo entry for it.
	 */
	if (targetid == RelationGetRelid(parentInfo->ri_RelationDesc))
		return parentInfo;

	if (parentInfo->ri_partition_hash == NULL)
	{
		HASHCTL ctl;

		ctl.keysize = sizeof(Oid);
		ctl.entrysize = sizeof(*entry);
		ctl.hash = oid_hash;

		parentInfo->ri_partition_hash =
			hash_create("Partition Result Relation Hash",
						10,
						&ctl,
						HASH_ELEM | HASH_FUNCTION);
	}

	entry = hash_search(parentInfo->ri_partition_hash,
						&targetid,
						HASH_ENTER,
						&found);

	childInfo = &entry->resultRelInfo;
	if (found)
	{
		Assert(RelationGetRelid(childInfo->ri_RelationDesc) == targetid);
	}
	else
	{
		int			natts;
		Relation	resultRelation;

		natts = parentInfo->ri_RelationDesc->rd_att->natts; /* in base relation */

		resultRelation = heap_open(targetid, RowExclusiveLock);
		InitResultRelInfo(childInfo,
						  resultRelation,
						  1,
						  estate->es_instrument);

		if (openIndices)
			ExecOpenIndices(childInfo, false);

		childInfo->ri_partInsertMap =
			convert_tuples_by_name(RelationGetDescr(parentInfo->ri_RelationDesc),
								   RelationGetDescr(childInfo->ri_RelationDesc));
	}
	return childInfo;
}

/*
 * Calculate the part to use for the given key, then find or calculate
 * and cache required information about that part in the hash table
 * anchored in estate.
 *
 * Return a ResultRelInfo for that partition, an entry in the table
 * estate->es_result_relations. The first entry in this table is for the
 * partitioned table itself.
 *
 * NB: The entire table may be reallocated, changing the addresses of its
 * entries. Thus, it is important to avoid holding long-lived pointers to
 * table entries, such as the pointer returned from this function!
 */
ResultRelInfo *
slot_get_partition(TupleTableSlot *slot, EState *estate, bool openIndices)
{
	AttrNumber	max_attr;
	Datum	   *values;
	bool	   *nulls;
	Oid			targetid;

	Assert(PointerIsValid(estate->es_result_partitions));

	max_attr = estate->es_partition_state->max_partition_attr;

	slot_getsomeattrs(slot, max_attr);
	values = slot_get_values(slot);
	nulls = slot_get_isnull(slot);

	/* add a short term memory context if one wasn't assigned already */
	Assert(estate->es_partition_state != NULL &&
		estate->es_partition_state->accessMethods != NULL);
	if (!estate->es_partition_state->accessMethods->part_cxt)
		estate->es_partition_state->accessMethods->part_cxt =
			GetPerTupleExprContext(estate)->ecxt_per_tuple_memory;

	targetid = selectPartition(estate->es_result_partitions, values, nulls,
							   slot->tts_tupleDescriptor,
							   estate->es_partition_state->accessMethods);

	if (!OidIsValid(targetid))
		ereport(ERROR,
				(errcode(ERRCODE_NO_PARTITION_FOR_PARTITIONING_KEY),
				 errmsg("no partition for partitioning key")));

	return targetid_get_partition(targetid, estate, openIndices);
}


/* Use the given attribute map to convert an attribute number in the
 * base relation to an attribute number in the other relation.  Forgive
 * out-of-range attributes by mapping them to zero.  Treat null as
 * the identity map.
 */
AttrNumber
attrMap(TupleConversionMap *map, AttrNumber anum)
{
	if (map == NULL)
		return anum;

	if (0 < anum && anum <= map->indesc->natts)
	{
		for (int i = 0; i < map->outdesc->natts; i++)
		{
			if (map->attrMap[i] == anum)
				return i + 1;
		}
	}
	return 0;
}

/* For attrMapExpr below.
 *
 * Mutate Var nodes in an expression using the given attribute map.
 * Insist the Var nodes have varno == 1 and the that the mapping
 * yields a live attribute number (non-zero).
 */
static Node *apply_attrmap_mutator(Node *node, TupleConversionMap *map)
{
	if ( node == NULL )
		return NULL;
	
	if (IsA(node, Var) )
	{
		Var		   *var = (Var *) node;
		AttrNumber anum;

		Assert(var->varno == 1); /* in CHECK constraints */
		anum = attrMap(map, var->varattno);

		if (anum == 0)
		{
			/* Should never happen, but best caught early. */
			elog(ERROR, "attribute map discrepancy");
		}
		else if (anum != var->varattno)
		{
			var = copyObject(var);
			var->varattno = anum;
		}
		return (Node *) var;
	}
	return expression_tree_mutator(node, apply_attrmap_mutator, (void *) map);
}

/* Apply attrMap over the Var nodes in an expression. */
Node *
attrMapExpr(TupleConversionMap *map, Node *expr)
{
	return apply_attrmap_mutator(expr, map);
}


/* Check compatibility of the attributes of the given partitioned
 * table and part for purposes of INSERT (through the partitioned
 * table) or EXCHANGE (of the part into the partitioned table).
 * Don't use any partitioning catalogs, because this must run
 * on the segment databases as well as on the entry database.
 *
 * Also make a vector mapping the attribute
 * numbers of the partitioned table to corresponding attribute 
 * numbers in the part.  Represent the "unneeded" identity map
 * as null.
 *
 * base -- the partitioned table
 * part -- the part table
 * map_ptr -- where to store a pointer to the result.
 * throw -- whether to throw an error in case of incompatibility
 *
 * The return value indicates whether the part is compatible
 * with the base relation.  If the throw argument is true, however,
 * an error is issued rather than returning false.
 *
 * Be sure to call this in the memory context in which the result
 * vector ought to be stored.
 *
 * Though some error checking is done, it is not comprehensive.
 * If internal assumptions about possible tuple formats are
 * correct, errors should not occur.  Still, the downside is
 * incorrect data, so use errors (not assertions) for the case.
 *
 * Checks include same number of non-dropped attributes in all 
 * parts of a partitioned table, non-dropped attributes in 
 * corresponding relative positions must match in name, type
 * and alignment, attribute numbers must agree with their
 * position in tuple, etc.
 */
bool 
map_part_attrs(Relation base, Relation part, TupleConversionMap **map_ptr, bool throw)
{
	AttrNumber i = 1;
	AttrNumber n = base->rd_att->natts;
	FormData_pg_attribute *battr = NULL;
	
	AttrNumber j = 1;
	AttrNumber m = part->rd_att->natts;
	FormData_pg_attribute *pattr = NULL;

	bool is_identical = TRUE;
	bool is_compatible = TRUE;
	
	/* For each matched pair of attributes ... */
	while ( i <= n && j <= m )
	{
		battr = base->rd_att->attrs[i-1];
		pattr = part->rd_att->attrs[j-1];
		
		/* Skip dropped attributes. */
		
		if ( battr->attisdropped )
		{
			i++;
			continue;
		}
		
		if ( pattr->attisdropped )
		{
			j++;
			continue;
		}
		
		/* Check attribute conformability requirements. */
		
		/* -- Names must match. */
		if ( strncmp(NameStr(battr->attname), NameStr(pattr->attname), NAMEDATALEN) != 0 )
		{
			if ( throw )
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("relation \"%s\" must have the same "
								"column names and column order as \"%s\"",
								RelationGetRelationName(part),
								RelationGetRelationName(base))));
			is_compatible = FALSE;
			break;
		}
		
		/* -- Types must match. */
		if (battr->atttypid != pattr->atttypid)
		{
			if ( throw )
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("type mismatch for attribute \"%s\"",
								NameStr((battr->attname)))));
			is_compatible = FALSE;
			break;
		}
		
		/* -- Alignment should match, but check just to be safe. */
		if (battr->attalign != pattr->attalign )
		{
			if ( throw )
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("alignment mismatch for attribute \"%s\"",
								NameStr((battr->attname)))));
			is_compatible = FALSE;
			break;
		}
		
		/* -- Attribute numbers must match position (+1) in tuple. 
		 *    This is a hard requirement so always throw.  This could
		 *    be an assertion, except that we want to fail even in a 
		 *    distribution build.
		 */
		if ( battr->attnum != i || pattr->attnum != j )
			elog(ERROR,
				 "attribute numbers out of order");
		
		/* Note any attribute number difference. */
		if ( i != j )
			is_identical = FALSE;

		i++;
		j++;
	}

	if ( is_compatible )
	{
		/* Any excess attributes in parent better be marked dropped */
		for ( ; i <= n; i++ )
		{
			if ( !base->rd_att->attrs[i-1]->attisdropped )
			{
				if ( throw )
				/* the partitioned table has more columns than the part */
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("relation \"%s\" must have the same number columns as relation \"%s\"",
									RelationGetRelationName(part),
									RelationGetRelationName(base))));
				is_compatible = FALSE;
			}
		}

		/* Any excess attributes in part better be marked dropped */
		for ( ; j <= m; j++ )
		{
			if ( !part->rd_att->attrs[j-1]->attisdropped )
			{
				if ( throw )
				/* the partitioned table has fewer columns than the part */
					ereport(ERROR,
							(errcode(ERRCODE_SYNTAX_ERROR),
							 errmsg("relation \"%s\" must have the same number columns as relation \"%s\"",
									RelationGetRelationName(part),
									RelationGetRelationName(base))));
				is_compatible = FALSE;
			}
		}
	}

	/* Identical tuple descriptors should have the same number of columns */
	if (n != m)
	{
		is_identical = FALSE;
	}

	if ( !is_compatible )
	{
		Assert( !throw );
		return FALSE;
	}

	*map_ptr = convert_tuples_by_name(RelationGetDescr(base),
									  RelationGetDescr(part));
	return TRUE;
}

/*
 * BuildPartitionNodeFromRoot
 *   Build PartitionNode for the root partition of a given partition oid.
 */
static PartitionNode *
BuildPartitionNodeFromRoot(Oid relid)
{
	PartitionNode *partitionNode;

	if (rel_is_child_partition(relid))
	{
		relid = rel_partition_get_master(relid);
	}

	partitionNode = RelationBuildPartitionDescByOid(relid, false /* inctemplate */);

	return partitionNode;
}

/*
 * createPartitionAccessMethods
 *   Create a PartitionAccessMethods object.
 *
 * Note that the memory context for the access method is not set at this point. It will
 * be set during execution.
 */
static PartitionAccessMethods *
createPartitionAccessMethods(int numLevels)
{
	PartitionAccessMethods *accessMethods = palloc(sizeof(PartitionAccessMethods));;
	accessMethods->partLevels = numLevels;
	accessMethods->cmpfuncs = (FmgrInfo **) palloc0(numLevels * sizeof(FmgrInfo *));
	accessMethods->part_cxt = NULL;

	return accessMethods;
}

/*
 * createPartitionState
 *   Create a PartitionState object.
 *
 * Note that the memory context for the access method is not set at this point. It will
 * be set during execution.
 */
PartitionState *
createPartitionState(PartitionNode *partsAndRules,
					 int resultPartSize)
{
	Assert(partsAndRules != NULL);

	PartitionState *partitionState = makeNode(PartitionState);
	partitionState->accessMethods = createPartitionAccessMethods(num_partition_levels(partsAndRules));
	partitionState->max_partition_attr = max_partition_attr(partsAndRules);

	return partitionState;
}

/*
 * InitializeQueryPartsMetadata
 *   Initialize partitioning metadata for all partitions involved in the query.
 */
static void
InitializeQueryPartsMetadata(PlannedStmt *plannedstmt, EState *estate)
{
	Assert(plannedstmt != NULL && estate != NULL);

	if (plannedstmt->queryPartOids == NIL)
	{
		plannedstmt->queryPartsMetadata = NIL;
		return;
	}

	if (Gp_role != GP_ROLE_EXECUTE)
	{
		/*
		 * Non-QEs populate the partitioning metadata for all
		 * relevant partitions in the query.
		 */
		plannedstmt->queryPartsMetadata = NIL;
		ListCell *lc = NULL;
		foreach (lc, plannedstmt->queryPartOids)
		{
			Oid relid = (Oid)lfirst_oid(lc);
			PartitionNode *partitionNode = BuildPartitionNodeFromRoot(relid);
			Assert(partitionNode != NULL);
			plannedstmt->queryPartsMetadata =
				lappend(plannedstmt->queryPartsMetadata, partitionNode);
		}
	}

	/* Populate the partitioning metadata to EState */
	Assert(estate->dynamicTableScanInfo != NULL);

	MemoryContext oldContext = MemoryContextSwitchTo(estate->es_query_cxt);

	ListCell *lc = NULL;
	foreach(lc, plannedstmt->queryPartsMetadata)
	{
		PartitionNode *partsAndRules = (PartitionNode *)lfirst(lc);

		PartitionMetadata *metadata = palloc(sizeof(PartitionMetadata));
		metadata->partsAndRules = partsAndRules;
		Assert(metadata->partsAndRules != NULL);
		metadata->accessMethods = createPartitionAccessMethods(num_partition_levels(metadata->partsAndRules));
		estate->dynamicTableScanInfo->partsMetadata =
			lappend(estate->dynamicTableScanInfo->partsMetadata, metadata);
	}

	MemoryContextSwitchTo(oldContext);
}

/*
 * InitializePartsMetadata
 *   Initialize partitioning metadata for the given partitioned table oid
 */
List *
InitializePartsMetadata(Oid rootOid)
{
	PartitionMetadata *metadata = palloc(sizeof(PartitionMetadata));
	metadata->partsAndRules = BuildPartitionNodeFromRoot(rootOid);
	Assert(metadata->partsAndRules != NULL);

	metadata->accessMethods = createPartitionAccessMethods(num_partition_levels(metadata->partsAndRules));
	return list_make1(metadata);
}

/*
 * Adjust INSERT/UPDATE/DELETE count for replicated table ON QD
 */
static void
AdjustReplicatedTableCounts(EState *estate)
{
	int i;
	ResultRelInfo *resultRelInfo;
	bool containReplicatedTable = false;
	int			numsegments =  1;

	if (Gp_role != GP_ROLE_DISPATCH)
		return;

	/* check if result_relations contain replicated table*/
	for (i = 0; i < estate->es_num_result_relations; i++)
	{
		resultRelInfo = estate->es_result_relations + i;

		if (!resultRelInfo->ri_RelationDesc->rd_cdbpolicy)
			continue;

		if (GpPolicyIsReplicated(resultRelInfo->ri_RelationDesc->rd_cdbpolicy))
		{
			containReplicatedTable = true;
			numsegments = resultRelInfo->ri_RelationDesc->rd_cdbpolicy->numsegments;
		}
		else if (containReplicatedTable)
		{
			/*
			 * If one is replicated table, assert that other
			 * tables are also replicated table.
 			 */
			Insist(0);
		}
	}

	if (containReplicatedTable)
		estate->es_processed = estate->es_processed / numsegments;
}
