/*-------------------------------------------------------------------------
 *
 * execMain.c
 *	  top level executor interface routines
 *
 * INTERFACE ROUTINES
 *	ExecutorStart()
 *	ExecutorRun()
 *	ExecutorEnd()
 *
 *	The old ExecutorMain() has been replaced by ExecutorStart(),
 *	ExecutorRun() and ExecutorEnd()
 *
 *	These three procedures are the external interfaces to the executor.
 *	In each case, the query descriptor is required as an argument.
 *
 *	ExecutorStart() must be called at the beginning of execution of any
 *	query plan and ExecutorEnd() should always be called at the end of
 *	execution of a plan.
 *
 *	ExecutorRun accepts direction and count arguments that specify whether
 *	the plan is to be executed forwards, backwards, and for how many tuples.
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2010, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/executor/execMain.c,v 1.349 2010/04/28 16:10:42 heikki Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/aosegfiles.h"
#include "access/appendonlywriter.h"
#include "access/fileam.h"
#include "access/reloptions.h"
#include "access/sysattr.h"
#include "access/transam.h"
#include "access/xact.h"
#include "catalog/heap.h"
#include "catalog/namespace.h"
#include "catalog/pg_tablespace.h"
#include "catalog/toasting.h"
#include "catalog/aoseg.h"
#include "catalog/aoblkdir.h"
#include "catalog/aovisimap.h"
#include "catalog/catalog.h"
#include "catalog/pg_attribute_encoding.h"
#include "catalog/pg_type.h"
#include "cdb/cdbpartition.h"
#include "commands/tablecmds.h" /* XXX: temp for get_parts() */
#include "commands/tablespace.h"
#include "commands/trigger.h"
#include "executor/execDML.h"
#include "executor/execdebug.h"
#include "executor/execUtils.h"
#include "executor/instrument.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "optimizer/clauses.h"
#include "parser/parse_clause.h"
#include "parser/parsetree.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "storage/smgr.h"
#include "tcop/utility.h"
#include "utils/acl.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"
#include "utils/tqual.h"
#include "utils/metrics_utils.h"

#include "utils/builtins.h"
#include "utils/ps_status.h"
#include "utils/snapmgr.h"
#include "utils/typcache.h"
#include "utils/workfile_mgr.h"
#include "utils/faultinjector.h"

#include "catalog/pg_statistic.h"
#include "catalog/pg_class.h"

#include "tcop/tcopprot.h"

#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbaocsam.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbexplain.h"             /* cdbexplain_sendExecStats() */
#include "cdb/cdbplan.h"
#include "cdb/cdbsrlz.h"
#include "cdb/cdbsubplan.h"
#include "cdb/cdbvars.h"
#include "cdb/ml_ipc.h"
#include "cdb/cdbmotion.h"
#include "cdb/cdbtm.h"
#include "cdb/cdboidsync.h"
#include "cdb/cdbllize.h"
#include "cdb/memquota.h"
#include "cdb/cdbtargeteddispatch.h"

extern bool cdbpathlocus_querysegmentcatalogs;

/* Hooks for plugins to get control in ExecutorStart/Run/End() */
ExecutorStart_hook_type ExecutorStart_hook = NULL;
ExecutorRun_hook_type ExecutorRun_hook = NULL;
ExecutorEnd_hook_type ExecutorEnd_hook = NULL;

/* decls for local routines only used within this module */
static void InitPlan(QueryDesc *queryDesc, int eflags);
static void ExecEndPlan(PlanState *planstate, EState *estate);
static void ExecutePlan(EState *estate, PlanState *planstate,
			CmdType operation,
			bool sendTuples,
			long numberTuples,
			ScanDirection direction,
			DestReceiver *dest);
static void ExecCheckXactReadOnly(PlannedStmt *plannedstmt);
static void EvalPlanQualStart(EPQState *epqstate, EState *parentestate,
				  Plan *planTree);
static void OpenIntoRel(QueryDesc *queryDesc);
static void CloseIntoRel(QueryDesc *queryDesc);
static void intorel_startup(DestReceiver *self, int operation, TupleDesc typeinfo);
static void intorel_receive(TupleTableSlot *slot, DestReceiver *self);
static void intorel_shutdown(DestReceiver *self);
static void intorel_destroy(DestReceiver *self);

static void FillSliceTable(EState *estate, PlannedStmt *stmt);

void ExecCheckRTPerms(List *rangeTable);
void ExecCheckRTEPerms(RangeTblEntry *rte);

static PartitionNode *BuildPartitionNodeFromRoot(Oid relid);
static void InitializeQueryPartsMetadata(PlannedStmt *plannedstmt, EState *estate);
static void AdjustReplicatedTableCounts(EState *estate);
static bool intoRelIsReplicatedTable(QueryDesc *queryDesc);

/*
 * For a partitioned insert target only:  
 * This type represents an entry in the per-part hash table stored at
 * estate->es_partition_state->result_partition_hash.   The table maps 
 * part OID -> ResultRelInfo and avoids repeated calculation of the
 * result information.
 */
typedef struct ResultPartHashEntry 
{
	Oid targetid; /* OID of part relation */
	int offset; /* Index ResultRelInfo in es_result_partitions */
} ResultPartHashEntry;


typedef struct CopyDirectDispatchToSliceContext
{
	plan_tree_base_prefix	base; /* Required prefix for plan_tree_walker/mutator */
	EState					*estate; /* EState instance */
} CopyDirectDispatchToSliceContext;

static bool CopyDirectDispatchFromPlanToSliceTableWalker( Node *node, CopyDirectDispatchToSliceContext *context);

static void
CopyDirectDispatchToSlice( Plan *ddPlan, int sliceId, CopyDirectDispatchToSliceContext *context)
{
	EState	*estate = context->estate;
	Slice *slice = (Slice *)list_nth(estate->es_sliceTable->slices, sliceId);

	Assert( ! slice->directDispatch.isDirectDispatch );	/* should not have been set by some other process */
	Assert(ddPlan != NULL);

	if ( ddPlan->directDispatch.isDirectDispatch)
	{
		slice->directDispatch.isDirectDispatch = true;
		slice->directDispatch.contentIds = list_copy(ddPlan->directDispatch.contentIds);
	}
}

static bool
CopyDirectDispatchFromPlanToSliceTableWalker( Node *node, CopyDirectDispatchToSliceContext *context)
{
	int sliceId = -1;
	Plan *ddPlan = NULL;

	if (node == NULL)
		return false;

	if (IsA(node, Motion))
	{
		Motion *motion = (Motion *) node;

		ddPlan = (Plan*)node;
		sliceId = motion->motionID;
	}

	if (ddPlan != NULL)
	{
		CopyDirectDispatchToSlice(ddPlan, sliceId, context);
	}
	return plan_tree_walker(node, CopyDirectDispatchFromPlanToSliceTableWalker, context);
}

static void
CopyDirectDispatchFromPlanToSliceTable(PlannedStmt *stmt, EState *estate)
{
	CopyDirectDispatchToSliceContext context;
	exec_init_plan_tree_base(&context.base, stmt);
	context.estate = estate;
	CopyDirectDispatchToSlice( stmt->planTree, 0, &context);
	CopyDirectDispatchFromPlanToSliceTableWalker((Node *) stmt->planTree, &context);
}

/* ----------------------------------------------------------------
 *		ExecutorStart
 *
 *		This routine must be called at the beginning of any execution of any
 *		query plan
 *
 * Takes a QueryDesc previously created by CreateQueryDesc (it's not real
 * clear why we bother to separate the two functions, but...).	The tupDesc
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
	bool		needDtxTwoPhase;
	QueryDispatchDesc *ddesc;

	/* sanity checks: queryDesc must not be started already */
	Assert(queryDesc != NULL);
	Assert(queryDesc->estate == NULL);
	Assert(queryDesc->plannedstmt != NULL);

	PlannedStmt *plannedStmt = queryDesc->plannedstmt;

	if (MEMORY_OWNER_TYPE_Undefined == plannedStmt->memoryAccountId)
	{
		plannedStmt->memoryAccountId = MemoryAccounting_CreateAccount(0, MEMORY_OWNER_TYPE_EXECUTOR);
	}

	START_MEMORY_ACCOUNT(plannedStmt->memoryAccountId);

	Assert(plannedStmt->intoPolicy == NULL ||
		GpPolicyIsPartitioned(plannedStmt->intoPolicy) ||
		GpPolicyIsReplicated(plannedStmt->intoPolicy));

	/**
	 * Perfmon related stuff.
	 */
	if (gp_enable_gpperfmon
		&& Gp_role == GP_ROLE_DISPATCH
		&& queryDesc->gpmon_pkt)
	{
		gpmon_qlog_query_start(queryDesc->gpmon_pkt);
	}

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
	 * In GPDB, we must call ExecCheckXactReadOnly() in the QD even if the
	 * transaction is not read-only, because ExecCheckXactReadOnly() also
	 * determines if two-phase commit is needed.
	 */
	if ((XactReadOnly || Gp_role == GP_ROLE_DISPATCH) && !(eflags & EXEC_FLAG_EXPLAIN_ONLY))
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
			/* SELECT INTO and SELECT FOR UPDATE/SHARE need to mark tuples */
			if (queryDesc->plannedstmt->intoClause != NULL ||
				queryDesc->plannedstmt->rowMarks != NIL)
				estate->es_output_cid = GetCurrentCommandId(true);
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
	estate->es_instrument = queryDesc->instrument_options;
	estate->showstatctx = queryDesc->showstatctx;

	/*
	 * Shared input info is needed when ROLE_EXECUTE or sequential plan
	 */
	estate->es_sharenode = (List **) palloc0(sizeof(List *));

	/*
	 * Initialize the motion layer for this query.
	 *
	 * NOTE: need to be in estate->es_query_cxt before the call.
	 */
	initMotionLayerStructs((MotionLayerState **)&estate->motionlayer_context);

	/* Reset workfile disk full flag */
	WorkfileDiskspace_SetFull(false /* isFull */);
	/* Initialize per-query resource (diskspace) tracking */
	WorkfileQueryspace_InitEntry(gp_session_id, gp_command_count);

	/*
	 * Handling of the Slice table depends on context.
	 */
	if (Gp_role == GP_ROLE_DISPATCH && queryDesc->plannedstmt->planTree->dispatch == DISPATCH_PARALLEL)
	{
		ddesc = makeNode(QueryDispatchDesc);
		queryDesc->ddesc = ddesc;

		if (queryDesc->dest->mydest == DestIntoRel)
			queryDesc->ddesc->validate_reloptions = false;
		else
			queryDesc->ddesc->validate_reloptions = true;

		/*
		 * If this is an extended query (normally cursor or bind/exec) - before
		 * starting the portal, we need to make sure that the shared snapshot is
		 * already set by a writer gang, or the cursor query readers will
		 * timeout waiting for one that may not exist (in some cases). Therefore
		 * we insert a small hack here and dispatch a SET query that will do it
		 * for us. (This is also done in performOpenCursor() for the simple
		 * query protocol).
		 *
		 * MPP-7504/MPP-7448: We also call this down inside the dispatcher after
		 * the pre-dispatch evaluator has run.
		 */
		if (queryDesc->extended_query)
		{
			verify_shared_snapshot_ready();
		}

		/* Set up blank slice table to be filled in during InitPlan. */
		InitSliceTable(estate, queryDesc->plannedstmt->nMotionNodes, queryDesc->plannedstmt->nInitPlans);

		/**
		 * Copy direct dispatch decisions out of the plan and into the slice table.  Must be done after slice table is built.
		 * Note that this needs to happen whether or not the plan contains direct dispatch decisions. This
		 * is because the direct dispatch partially forgets some of the decisions it has taken.
		 **/
		if (gp_enable_direct_dispatch)
		{
			CopyDirectDispatchFromPlanToSliceTable(queryDesc->plannedstmt, estate );
		}

		/* Pass EXPLAIN ANALYZE flag to qExecs. */
		estate->es_sliceTable->instrument_options = queryDesc->instrument_options;

		/* set our global sliceid variable for elog. */
		currentSliceId = LocallyExecutingSliceIndex(estate);

		/* Determine OIDs for into relation, if any */
		if (queryDesc->plannedstmt->intoClause != NULL)
		{
			IntoClause *intoClause = queryDesc->plannedstmt->intoClause;
			Oid         reltablespace;

			cdb_sync_oid_to_segments();

			/* MPP-10329 - must always dispatch the tablespace */
			if (intoClause->tableSpaceName)
			{
				reltablespace = get_tablespace_oid(intoClause->tableSpaceName, false);
				ddesc->intoTableSpaceName = intoClause->tableSpaceName;
			}
			else
			{
				reltablespace = GetDefaultTablespace(intoClause->rel->istemp);

				/* Need the real tablespace id for dispatch */
				if (!OidIsValid(reltablespace))
					reltablespace = MyDatabaseTableSpace;

				ddesc->intoTableSpaceName = get_tablespace_name(reltablespace);
			}
		}
	}
	else if (Gp_role == GP_ROLE_EXECUTE)
	{
		ddesc = queryDesc->ddesc;

		/* qDisp should have sent us a slice table via MPPEXEC */
		if (ddesc && ddesc->sliceTable != NULL)
		{
			SliceTable *sliceTable;
			Slice	   *slice;

			sliceTable = queryDesc->ddesc->sliceTable;
			Assert(IsA(sliceTable, SliceTable));
			slice = (Slice *)list_nth(sliceTable->slices, sliceTable->localSlice);
			Assert(IsA(slice, Slice));

			estate->es_sliceTable = sliceTable;
			estate->es_cursorPositions = queryDesc->ddesc->cursorPositions;

			estate->currentSliceIdInPlan = slice->rootIndex;
			estate->currentExecutingSliceId = slice->rootIndex;

			/* set our global sliceid variable for elog. */
			currentSliceId = LocallyExecutingSliceIndex(estate);

			/* Should we collect statistics for EXPLAIN ANALYZE? */
			estate->es_instrument = sliceTable->instrument_options;
			queryDesc->instrument_options = sliceTable->instrument_options;
		}

		/* InitPlan() will acquire locks by walking the entire plan
		 * tree -- we'd like to avoid acquiring the locks until
		 * *after* we've set up the interconnect */
		if (queryDesc->plannedstmt->nMotionNodes > 0)
		{
			int			i;

			PG_TRY();
			{
				for (i=1; i <= queryDesc->plannedstmt->nMotionNodes; i++)
				{
					InitMotionLayerNode(estate->motionlayer_context, i);
				}

				estate->es_interconnect_is_setup = true;

				Assert(!estate->interconnect_context);
				SetupInterconnect(estate);

				SIMPLE_FAULT_INJECTOR(QEGotSnapshotAndInterconnect);
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

	/*
	 * We don't eliminate aliens if we don't have an MPP plan
	 * or we are executing on master.
	 *
	 * TODO: eliminate aliens even on master, if not EXPLAIN ANALYZE
	 */
	estate->eliminateAliens = execute_pruned_plan && queryDesc->plannedstmt->nMotionNodes > 0 && Gp_segment != -1;

	/*
	 * Assign a Motion Node to every Plan Node. This makes it
	 * easy to identify which slice any Node belongs to
	 */
	AssignParentMotionToPlanNodes(queryDesc->plannedstmt);

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

		if (Gp_role == GP_ROLE_DISPATCH &&
			queryDesc->plannedstmt->planTree->dispatch == DISPATCH_PARALLEL)
		{
			if (!(eflags & EXEC_FLAG_EXPLAIN_ONLY))
			{
				/*
				 * Since we intend to execute the plan, inventory the slice tree,
				 * allocate gangs, and associate them with slices.
				 *
				 * For now, always use segment 'gp_singleton_segindex' for
				 * singleton gangs.
				 *
				 * On return, gangs have been allocated and CDBProcess lists have
				 * been filled in in the slice table.)
				 */
				AssignGangs(queryDesc);
			}
		}

#ifdef USE_ASSERT_CHECKING
		AssertSliceTableIsValid((struct SliceTable *) estate->es_sliceTable, queryDesc->plannedstmt);
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

				/* Patch Motion node so it looks like a top node. */
				motionstate->ps.plan->nMotionNodes = estate->es_sliceTable->nMotions;
			}

			if (Debug_print_slice_table)
				elog_node_display(DEBUG3, "slice table", estate->es_sliceTable, true);

			if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
				elog(DEBUG1, "seg%d executing slice%d under root slice%d",
					 Gp_segment,
					 LocallyExecutingSliceIndex(estate),
					 RootSliceIndex(estate));
		}

		/*
		 * Are we going to dispatch this plan parallel?  Only if we're running as
		 * a QD and the plan is a parallel plan.
		 */
		if (Gp_role == GP_ROLE_DISPATCH &&
			queryDesc->plannedstmt->planTree->dispatch == DISPATCH_PARALLEL &&
			!(eflags & EXEC_FLAG_EXPLAIN_ONLY))
		{
			shouldDispatch = true;
		}
		else
		{
			shouldDispatch = false;
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
			needDtxTwoPhase = ExecutorSaysTransactionDoesWrites();
			dtmPreCommand("ExecutorStart", "(none)", queryDesc->plannedstmt,
						  needDtxTwoPhase, true /* wantSnapshot */, queryDesc->extended_query );

			queryDesc->ddesc->sliceTable = estate->es_sliceTable;

			queryDesc->ddesc->oidAssignments = GetAssignedOidsForDispatch();

			/*
			 * First, see whether we need to pre-execute any initPlan subplans.
			 */
			if (queryDesc->plannedstmt->nParamExec > 0)
			{
				ParamListInfoData *pli = queryDesc->params;

				/*
				 * First, use paramFetch to fetch any "lazy" parameters, so that
				 * they are dispatched along with the queries. The QE nodes cannot
				 * call the callback function on their own.
				 */
				if (pli && pli->paramFetch)
				{
					int			iparam;

					for (iparam = 0; iparam < queryDesc->params->numParams; iparam++)
					{
						ParamExternData *prm = &pli->params[iparam];

						if (!OidIsValid(prm->ptype))
							(*pli->paramFetch) (pli, iparam + 1);
					}
				}

				preprocess_initplans(queryDesc);

				/*
				 * Copy the values of the preprocessed subplans to the
				 * external parameters.
				 */
				queryDesc->params = addRemoteExecParamsToParamList(queryDesc->plannedstmt,
																   queryDesc->params,
																   queryDesc->estate->es_param_exec_vals);
			}

			/*
			 * This call returns after launching the threads that send the
			 * plan to the appropriate segdbs.  It does not wait for them to
			 * finish unless an error is detected before all slices have been
			 * dispatched.
			 */
			CdbDispatchPlan(queryDesc, needDtxTwoPhase, true, estate->dispatcherState);
		}

		/*
		 * Get executor identity (who does the executor serve). we can assume
		 * Forward scan direction for now just for retrieving the identity.
		 */
		if (!(eflags & EXEC_FLAG_EXPLAIN_ONLY))
			exec_identity = getGpExecIdentity(queryDesc, ForwardScanDirection, estate);
		else
			exec_identity = GP_IGNORE;

		/* non-root on QE */
		if (exec_identity == GP_NON_ROOT_ON_QE)
		{
			MotionState *motionState = getMotionState(queryDesc->planstate, LocallyExecutingSliceIndex(estate));

			Assert(motionState);

			Assert(IsA(motionState->ps.plan, Motion));

			/* update the connection information, if needed */
			if (((PlanState *) motionState)->plan->nMotionNodes > 0)
			{
				ExecUpdateTransportState((PlanState *)motionState,
										 estate->interconnect_context);
			}
		}
		else if (exec_identity == GP_ROOT_SLICE)
		{
			/* Run a root slice. */
			if (queryDesc->planstate != NULL &&
				queryDesc->planstate->plan->nMotionNodes > 0 && !estate->es_interconnect_is_setup)
			{
				estate->es_interconnect_is_setup = true;

				Assert(!estate->interconnect_context);
				SetupInterconnect(estate);
				Assert(estate->interconnect_context);
			}
			if (estate->es_interconnect_is_setup)
			{
				ExecUpdateTransportState(queryDesc->planstate,
										 estate->interconnect_context);
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

	END_MEMORY_ACCOUNT();

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
 *		completion.
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
			ScanDirection direction, long count)
{
	if (ExecutorRun_hook)
		(*ExecutorRun_hook) (queryDesc, direction, count);
	else
		standard_ExecutorRun(queryDesc, direction, count);
}

void
standard_ExecutorRun(QueryDesc *queryDesc,
					 ScanDirection direction, long count)
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
	Slice              *currentSlice;
	GpExecIdentity		exec_identity;

	/* sanity checks */
	Assert(queryDesc != NULL);

	estate = queryDesc->estate;

	Assert(estate != NULL);

	Assert(NULL != queryDesc->plannedstmt && MEMORY_OWNER_TYPE_Undefined != queryDesc->plannedstmt->memoryAccountId);

	START_MEMORY_ACCOUNT(queryDesc->plannedstmt->memoryAccountId);

	/*
	 * Switch into per-query memory context
	 */
	oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

	/* Allow instrumentation of ExecutorRun overall runtime */
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
		if (FaultInjector_InjectFaultIfSet(ExecutorRunHighProcessed,
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
	END_MEMORY_ACCOUNT();
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

	/* sanity checks */
	Assert(queryDesc != NULL);

	estate = queryDesc->estate;

	Assert(estate != NULL);

	Assert(NULL != queryDesc->plannedstmt && MEMORY_OWNER_TYPE_Undefined != queryDesc->plannedstmt->memoryAccountId);

	START_MEMORY_ACCOUNT(queryDesc->plannedstmt->memoryAccountId);

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
	 * all mpp specific resources (interconnect, seq server).
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
		RemoveMotionLayer(estate->motionlayer_context, true);

		/*
		 * Release EState and per-query memory context.
		 */
		FreeExecutorState(estate);

		PG_RE_THROW();
	}
	PG_END_TRY();

    /*
     * If normal termination, let each operator clean itself up.
     * Otherwise don't risk it... an error might have left some
     * structures in an inconsistent state.
     */
	ExecEndPlan(queryDesc->planstate, estate);

	WorkfileQueryspace_ReleaseEntry();

	/*
	 * Release any gangs we may have assigned.
	 */
	if (Gp_role == GP_ROLE_DISPATCH && queryDesc->plannedstmt->planTree->dispatch == DISPATCH_PARALLEL)
		ReleaseGangs(queryDesc);

	/*
	 * Remove our own query's motion layer.
	 */
	RemoveMotionLayer(estate->motionlayer_context, true);

	/*
	 * Adjust SELECT INTO count
	 * Close the SELECT INTO relation if any
	 */
	if (estate->es_select_into)
	{
		if (Gp_role == GP_ROLE_DISPATCH &&
			intoRelIsReplicatedTable(queryDesc))
			estate->es_processed = estate->es_processed / getgpsegmentCount();
		CloseIntoRel(queryDesc);
	}

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
	
	/**
	 * Perfmon related stuff.
	 */
	if (gp_enable_gpperfmon 
			&& Gp_role == GP_ROLE_DISPATCH
			&& queryDesc->gpmon_pkt)
	{			
		gpmon_qlog_query_end(queryDesc->gpmon_pkt);
		queryDesc->gpmon_pkt = NULL;
	}

	/* GPDB hook for collecting query info */
	if (query_info_collect_hook)
		(*query_info_collect_hook)(METRICS_QUERY_DONE, queryDesc);

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
	END_MEMORY_ACCOUNT();

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

	Assert(NULL != queryDesc->plannedstmt && MEMORY_OWNER_TYPE_Undefined != queryDesc->plannedstmt->memoryAccountId);

	START_MEMORY_ACCOUNT(queryDesc->plannedstmt->memoryAccountId);

	/* It's probably not sensible to rescan updating queries */
	Assert(queryDesc->operation == CMD_SELECT);

	/*
	 * Switch into per-query memory context
	 */
	oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

	/*
	 * rescan plan
	 */
	ExecReScan(queryDesc->planstate, NULL);

	MemoryContextSwitchTo(oldcontext);

	END_MEMORY_ACCOUNT();
}


/*
 * ExecCheckRTPerms
 *		Check access permissions for all relations listed in a range table.
 */
void
ExecCheckRTPerms(List *rangeTable)
{
	ListCell   *l;

	foreach(l, rangeTable)
	{
		ExecCheckRTEPerms((RangeTblEntry *) lfirst(l));
	}
}

/*
 * ExecCheckRTEPerms
 *		Check access permissions for a single RTE.
 */
void
ExecCheckRTEPerms(RangeTblEntry *rte)
{
	AclMode		requiredPerms;
	AclMode		relPerms;
	AclMode		remainingPerms;
	Oid			relOid;
	Oid			userid;
	Bitmapset  *tmpset;
	int			col;

	/*
	 * Only plain-relation RTEs need to be checked here.  Function RTEs are
	 * checked by init_fcache when the function is prepared for execution.
	 * Join, subquery, and special RTEs need no checks.
	 */
	if (rte->rtekind != RTE_RELATION)
		return;

	/*
	 * No work if requiredPerms is empty.
	 */
	requiredPerms = rte->requiredPerms;
	if (requiredPerms == 0)
		return;

	relOid = rte->relid;

	/*
	 * userid to check as: current user unless we have a setuid indication.
	 *
	 * Note: GetUserId() is presently fast enough that there's no harm in
	 * calling it separately for each RTE.	If that stops being true, we could
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
		/*
		 * If we lack any permissions that exist only as relation permissions,
		 * we can fail straight away.
		 */
		if (remainingPerms & ~(ACL_SELECT | ACL_INSERT | ACL_UPDATE))
			aclcheck_error(ACLCHECK_NO_PRIV, ACL_KIND_CLASS,
						   get_rel_name_partition(relOid));

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
					aclcheck_error(ACLCHECK_NO_PRIV, ACL_KIND_CLASS,
								   get_rel_name_partition(relOid));
			}

			tmpset = bms_copy(rte->selectedCols);
			while ((col = bms_first_member(tmpset)) >= 0)
			{
				/* remove the column number offset */
				col += FirstLowInvalidHeapAttributeNumber;
				if (col == InvalidAttrNumber)
				{
					/* Whole-row reference, must have priv on all cols */
					if (pg_attribute_aclcheck_all(relOid, userid, ACL_SELECT,
												  ACLMASK_ALL) != ACLCHECK_OK)
						aclcheck_error(ACLCHECK_NO_PRIV, ACL_KIND_CLASS,
									   get_rel_name_partition(relOid));
				}
				else
				{
					if (pg_attribute_aclcheck(relOid, col, userid, ACL_SELECT)
						!= ACLCHECK_OK)
						aclcheck_error(ACLCHECK_NO_PRIV, ACL_KIND_CLASS,
									   get_rel_name_partition(relOid));
				}
			}
			bms_free(tmpset);
		}

		/*
		 * Basically the same for the mod columns, with either INSERT or
		 * UPDATE privilege as specified by remainingPerms.
		 */
		remainingPerms &= ~ACL_SELECT;
		if (remainingPerms != 0)
		{
			/*
			 * When the query doesn't explicitly change any columns, allow the
			 * query if we have permission on any column of the rel.  This is
			 * to handle SELECT FOR UPDATE as well as possible corner cases in
			 * INSERT and UPDATE.
			 */
			if (bms_is_empty(rte->modifiedCols))
			{
				if (pg_attribute_aclcheck_all(relOid, userid, remainingPerms,
											  ACLMASK_ANY) != ACLCHECK_OK)
					aclcheck_error(ACLCHECK_NO_PRIV, ACL_KIND_CLASS,
								   get_rel_name_partition(relOid));
			}

			tmpset = bms_copy(rte->modifiedCols);
			while ((col = bms_first_member(tmpset)) >= 0)
			{
				/* remove the column number offset */
				col += FirstLowInvalidHeapAttributeNumber;
				if (col == InvalidAttrNumber)
				{
					/* whole-row reference can't happen here */
					elog(ERROR, "whole-row update is not implemented");
				}
				else
				{
					if (pg_attribute_aclcheck(relOid, col, userid, remainingPerms)
						!= ACLCHECK_OK)
						aclcheck_error(ACLCHECK_NO_PRIV, ACL_KIND_CLASS,
									   get_rel_name_partition(relOid));
				}
			}
			bms_free(tmpset);
		}
	}
}

/*
 * Check that the query does not imply any writes to non-temp tables.
 *
 * This function is used to check if the current statement will perform any writes.
 * It is used to enforce:
 *  (1) read-only mode (both fts and transcation isolation level read only)
 *      as well as
 *  (2) to keep track of when a distributed transaction becomes
 *      "dirty" and will require 2pc.
 *
 * Note: in a Hot Standby slave this would need to reject writes to temp
 * tables as well; but an HS slave can't have created any temp tables
 * in the first place, so no need to check that.
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

	/*
	 * CREATE TABLE AS or SELECT INTO?
	 *
	 * XXX should we allow this if the destination is temp?  Considering that
	 * it would still require catalog changes, probably not.
	 */
	if (plannedstmt->intoClause != NULL)
	{
		Assert(plannedstmt->intoClause->rel);
		if (plannedstmt->intoClause->rel->istemp)
			ExecutorMarkTransactionDoesWrites();
		else
			PreventCommandIfReadOnly(CreateCommandTag((Node *) plannedstmt));
	}

	/* Fail if write permissions are requested on any non-temp table */
    rti = 0;
	foreach(l, plannedstmt->rtable)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(l);

		rti++;

		if (rte->rtekind != RTE_RELATION)
			continue;

		if ((rte->requiredPerms & (~ACL_SELECT)) == 0)
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
	bool		shouldDispatch = Gp_role == GP_ROLE_DISPATCH && plannedstmt->planTree->dispatch == DISPATCH_PARALLEL;

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
	if (operation != CMD_SELECT ||
		(Gp_role != GP_ROLE_EXECUTE &&
		 !(shouldDispatch && cdbpathlocus_querysegmentcatalogs)))
	{
		ExecCheckRTPerms(rangeTable);
	}
	else
	{
		/*
		 * We don't check the rights here, so we can query pg_statistic even if we are a non-privileged user.
		 * This shouldn't cause a problem, because "cdbpathlocus_querysegmentcatalogs" can only be true if we
		 * are doing special catalog queries for ANALYZE.  Otherwise, the QD will execute the normal access right
		 * check.  This does open a security hole, as it's possible for a hacker to connect to a segdb with GP_ROLE_EXECUTE,
		 * (at least, in theory, although it isn't easy) and then do a query.  But all they can see is
		 * pg_statistic and pg_class, and pg_class is normally readable by everyone.
		 */

		ListCell *lc = NULL;

		foreach(lc, rangeTable)
		{
			RangeTblEntry *rte = lfirst(lc);

			if (rte->rtekind != RTE_RELATION)
				continue;

			if (rte->requiredPerms == 0)
				continue;

			/*
			 * Ignore access rights check on pg_statistic and pg_class, so
			 * the QD can retrieve the statistics from the QEs.
			 */
			if (rte->relid != StatisticRelationId && rte->relid != RelationRelationId)
			{
				ExecCheckRTEPerms(rte);
			}
		}
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
		/* CDB: we must promote locks for UPDATE and DELETE operations. */
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
			if (operation == CMD_UPDATE || operation == CMD_DELETE)
			{
				resultRelation = CdbOpenRelation(resultRelationOid,
													 lockmode,
													 false, /* noWait */
													 NULL); /* lockUpgraded */
			}
			else
			{
				resultRelation = heap_open(resultRelationOid, lockmode);
			}
			InitResultRelInfo(resultRelInfo,
							  resultRelation,
							  resultRelationIndex,
							  operation,
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
			estate->es_result_aosegnos = plannedstmt->result_aosegnos;
		}
		else
		{
			List	   *resultRelations = plannedstmt->resultRelations;
			int			numResultRelations = list_length(resultRelations);
			List 	*all_relids = NIL;
			Oid		 relid = getrelid(linitial_int(plannedstmt->resultRelations), rangeTable);

			if (rel_is_child_partition(relid))
			{
				relid = rel_partition_get_master(relid);
			}

			estate->es_result_partitions = BuildPartitionNodeFromRoot(relid);

			/*
			 * list all the relids that may take part in this insert operation
			 */
			all_relids = lappend_oid(all_relids, relid);
			all_relids = list_concat(all_relids,
									 all_partition_relids(estate->es_result_partitions));

			/*
			 * We also assign a segno for a deletion operation.
			 * That segno will later be touched to ensure a correct
			 * incremental backup.
			 */
			estate->es_result_aosegnos = assignPerRelSegno(all_relids);

			plannedstmt->result_partitions = estate->es_result_partitions;
			plannedstmt->result_aosegnos = estate->es_result_aosegnos;

			/* Set any QD resultrels segno, just in case. The QEs set their own in ExecInsert(). */
			int relno = 0;
			ResultRelInfo* relinfo;
			for (relno = 0; relno < numResultRelations; relno ++)
			{
				relinfo = &(resultRelInfos[relno]);
				ResultRelInfoSetSegno(relinfo, estate->es_result_aosegnos);
			}
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
		estate->es_result_aosegnos = NIL;
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
	 * Similarly, we have to lock relations selected FOR UPDATE/FOR SHARE
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

		/* ignore "parent" rowmarks; they are irrelevant at runtime */
		if (rc->isParent)
			continue;

		switch (rc->markType)
		{
				/* CDB: On QD, lock whole table in X mode, if distributed. */
			case ROW_MARK_TABLE_EXCLUSIVE:
				relid = getrelid(rc->rti, rangeTable);
				relation = heap_open(relid, ExclusiveLock);
				break;
			case ROW_MARK_TABLE_SHARE:
				/* CDB: On QD, lock whole table in S mode, if distributed. */
				relid = getrelid(rc->rti, rangeTable);
				relation = heap_open(relid, RowShareLock);
				break;
			case ROW_MARK_EXCLUSIVE:
			case ROW_MARK_SHARE:
				relid = getrelid(rc->rti, rangeTable);
				relation = heap_open(relid, RowShareLock);
				break;
			case ROW_MARK_REFERENCE:
				relid = getrelid(rc->rti, rangeTable);
				relation = heap_open(relid, AccessShareLock);
				break;
			case ROW_MARK_COPY:
				/* there's no real table here ... */
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

		erm = (ExecRowMark *) palloc(sizeof(ExecRowMark));
		erm->relation = relation;
		erm->rti = rc->rti;
		erm->prti = rc->prti;
		erm->markType = rc->markType;
		erm->noWait = rc->noWait;
		erm->ctidAttNo = rc->ctidAttNo;
		erm->toidAttNo = rc->toidAttNo;
		erm->wholeAttNo = rc->wholeAttNo;
		ItemPointerSetInvalid(&(erm->curCtid));
		estate->es_rowMarks = lappend(estate->es_rowMarks, erm);
	}

	/*
	 * Detect whether we're doing SELECT INTO.  If so, set the es_into_oids
	 * flag appropriately so that the plan tree will be initialized with the
	 * correct tuple descriptors.  (Other SELECT INTO stuff comes later.)
	 */
	estate->es_select_into = false;
	if (operation == CMD_SELECT && plannedstmt->intoClause != NULL)
	{
		estate->es_select_into = true;
		estate->es_into_oids = interpretOidsOption(plannedstmt->intoClause->options);
	}

	/*
	 * Initialize the executor's tuple table to empty.
	 */
	estate->es_tupleTable = NIL;
	estate->es_trig_tuple_slot = NULL;
	estate->es_trig_oldtup_slot = NULL;

	/* mark EvalPlanQual not active */
	estate->es_epqTuple = NULL;
	estate->es_epqTupleSet = NULL;
	estate->es_epqScanDone = NULL;

	/*
	 * Initialize the slice table.
	 */
	if (Gp_role == GP_ROLE_DISPATCH)
		FillSliceTable(estate, plannedstmt);

	/*
	 * Initialize private state information for each SubPlan.  We must do this
	 * before running ExecInitNode on the main query tree, since
	 * ExecInitSubPlan expects to be able to find these entries.
	 */
	Assert(estate->es_subplanstates == NIL);
	Bitmapset *locallyExecutableSubplans = NULL;
	Plan *start_plan_node = plannedstmt->planTree;

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
		}
		/* Compute SubPlans' root plan nodes for SubPlans reachable from this plan root */
		locallyExecutableSubplans = getLocallyExecutableSubplans(plannedstmt, start_plan_node);
	}

	int subplan_idx = 0;
	foreach(l, plannedstmt->subplans)
	{
		PlanState  *subplanstate = NULL;
		int			sp_eflags = 0;

		/*
		 * Initialize only the subplans that are reachable from our local slice.
		 * If alien elimination is not turned on, then all subplans are considered
		 * reachable.
		 */
		if (!estate->eliminateAliens || bms_is_member(subplan_idx, locallyExecutableSubplans))
		{
			/*
			 * A subplan will never need to do BACKWARD scan nor MARK/RESTORE.
			 *
			 * GPDB: We always set the REWIND flag, to delay eagerfree.
			 */
			sp_eflags = eflags & EXEC_FLAG_EXPLAIN_ONLY;
			sp_eflags |= EXEC_FLAG_REWIND;

			Plan	   *subplan = (Plan *) lfirst(l);
			subplanstate = ExecInitNode(subplan, estate, sp_eflags);
		}

		estate->es_subplanstates = lappend(estate->es_subplanstates, subplanstate);

		++subplan_idx;
	}

	/* No more use for locallyExecutableSubplans */
	bms_free(locallyExecutableSubplans);

	/* Extract all precomputed parameters from init plans */
	ExtractParamsFromInitPlans(plannedstmt, plannedstmt->planTree, estate);

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
	 * Get the tuple descriptor describing the type of tuples to return. (this
	 * is especially important if we are creating a relation with "SELECT
	 * INTO")
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
	 * If doing SELECT INTO, initialize the "into" relation.  We must wait
	 * till now so we have the "clean" result tuple type to create the new
	 * table from.
	 *
	 * If EXPLAIN, skip creating the "into" relation.
	 */
	if (estate->es_select_into && !(eflags & EXEC_FLAG_EXPLAIN_ONLY) &&
			/* Only create the table if root slice */
	        (Gp_role != GP_ROLE_EXECUTE || Gp_is_writer) )
		OpenIntoRel(queryDesc);


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
 * Initialize ResultRelInfo data for one result relation
 */
void
InitResultRelInfo(ResultRelInfo *resultRelInfo,
				  Relation resultRelationDesc,
				  Index resultRelationIndex,
				  CmdType operation,
				  int instrument_options)
{
	/*
	 * Check valid relkind ... parser and/or planner should have noticed this
	 * already, but let's make sure.
	 */
	switch (resultRelationDesc->rd_rel->relkind)
	{
		case RELKIND_RELATION:
			/* OK */
			break;
		case RELKIND_SEQUENCE:
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot change sequence \"%s\"",
							RelationGetRelationName(resultRelationDesc))));
			break;
		case RELKIND_TOASTVALUE:
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot change TOAST relation \"%s\"",
							RelationGetRelationName(resultRelationDesc))));
			break;
		case RELKIND_AOSEGMENTS:
			if (!allowSystemTableModsDML)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("cannot change AO segment listing relation \"%s\"",
								RelationGetRelationName(resultRelationDesc))));
			break;
		case RELKIND_AOBLOCKDIR:
			if (!allowSystemTableModsDML)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("cannot change AO block directory relation \"%s\"",
								RelationGetRelationName(resultRelationDesc))));
			break;
		case RELKIND_AOVISIMAP:
			if (!allowSystemTableModsDML)
				ereport(ERROR,
						(errcode(ERRCODE_WRONG_OBJECT_TYPE),
						 errmsg("cannot change AO visibility map relation \"%s\"",
								RelationGetRelationName(resultRelationDesc))));
			break;

		case RELKIND_VIEW:
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot change view \"%s\"",
							RelationGetRelationName(resultRelationDesc))));
			break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_WRONG_OBJECT_TYPE),
					 errmsg("cannot change relation \"%s\"",
							RelationGetRelationName(resultRelationDesc))));
			break;
	}

	/* OK, fill in the node */
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
	resultRelInfo->ri_ConstraintExprs = NULL;
	resultRelInfo->ri_junkFilter = NULL;
	resultRelInfo->ri_projectReturning = NULL;
	resultRelInfo->ri_aoInsertDesc = NULL;
	resultRelInfo->ri_aocsInsertDesc = NULL;
	resultRelInfo->ri_extInsertDesc = NULL;
	resultRelInfo->ri_deleteDesc = NULL;
	resultRelInfo->ri_updateDesc = NULL;
	resultRelInfo->ri_aosegno = InvalidFileSegNumber;

	/*
	 * If there are indices on the result relation, open them and save
	 * descriptors in the result relation info, so that we can add new index
	 * entries for the tuples we add/update.  We need not do this for a
	 * DELETE, however, since deletion doesn't affect indexes.
	 */
	if (Gp_role != GP_ROLE_EXECUTE || Gp_is_writer) /* only needed by the root slice who will do the actual updating */
	{
		if (resultRelationDesc->rd_rel->relhasindex &&
			operation != CMD_DELETE)
			ExecOpenIndices(resultRelInfo);
	}
}

/*
 * ResultRelInfoSetSegno
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
 *
 */
void
ResultRelInfoSetSegno(ResultRelInfo *resultRelInfo, List *mapping)
{
   	ListCell *relid_to_segno;
   	bool	  found = false;

	/* only relevant for AO relations */
	if(!relstorage_is_ao(RelinfoGetStorage(resultRelInfo)))
		return;

	Assert(mapping);
	Assert(resultRelInfo->ri_RelationDesc);

   	/* lookup the segfile # to write into, according to my relid */

   	foreach(relid_to_segno, mapping)
   	{
		SegfileMapNode *n = (SegfileMapNode *)lfirst(relid_to_segno);
		Oid myrelid = RelationGetRelid(resultRelInfo->ri_RelationDesc);
		if(n->relid == myrelid)
		{
			Assert(n->segno != InvalidFileSegNumber);
			resultRelInfo->ri_aosegno = n->segno;

			elogif(Debug_appendonly_print_insert, LOG,
				"Appendonly: setting pre-assigned segno %d in result "
				"relation with relid %d", n->segno, n->relid);

			found = true;
		}
	}

	Assert(found);
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
 * be processed in the context of the outer query.	For efficiency's sake,
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
	 * event got queued, so we need take no new lock here.
	 */
	rel = heap_open(relid, NoLock);

	/*
	 * Make the new entry in the right context.  Currently, we don't need any
	 * index information in ResultRelInfos used only for triggers, so tell
	 * InitResultRelInfo it's a DELETE.
	 */
	oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);
	rInfo = makeNode(ResultRelInfo);
	InitResultRelInfo(rInfo,
					  rel,
					  0,		/* dummy rangetable index */
					  CMD_DELETE,
					  estate->es_instrument);
	estate->es_trig_target_relations =
		lappend(estate->es_trig_target_relations, rInfo);
	MemoryContextSwitchTo(oldcontext);

	return rInfo;
}

/*
 *		ExecContextForcesOids
 *
 * This is pretty grotty: when doing INSERT, UPDATE, or SELECT INTO,
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
 * SELECT INTO is even uglier, because we don't have the INTO relation's
 * descriptor available when this code runs; we have to look aside at a
 * flag set by InitPlan().
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

	if (planstate->state->es_select_into)
	{
		*hasoids = planstate->state->es_into_oids;
		return true;
	}

	return false;
}

void
SendAOTupCounts(EState *estate)
{
	/*
	 * If we're inserting into partitions, send tuple counts for
	 * AO tables back to the QD.
	 */
	if (Gp_role == GP_ROLE_EXECUTE && estate->es_result_partitions)
	{
		StringInfoData buf;
		ResultRelInfo *resultRelInfo;
		int aocount = 0;
		int i;

		resultRelInfo = estate->es_result_relations;
		for (i = 0; i < estate->es_num_result_relations; i++)
		{
			if (relstorage_is_ao(RelinfoGetStorage(resultRelInfo)))
				aocount++;

			resultRelInfo++;
		}

		if (aocount)
		{
			if (Debug_appendonly_print_insert)
				ereport(LOG,(errmsg("QE sending tuple counts of %d partitioned "
									"AO relations... ", aocount)));

			pq_beginmessage(&buf, 'o');
			pq_sendint(&buf, aocount, 4);

			resultRelInfo = estate->es_result_relations;
			for (i = 0; i < estate->es_num_result_relations; i++)
			{
				if (relstorage_is_ao(RelinfoGetStorage(resultRelInfo)))
				{
					Oid relid = RelationGetRelid(resultRelInfo->ri_RelationDesc);
					uint64 tupcount = resultRelInfo->ri_aoprocessed;

					pq_sendint(&buf, relid, 4);
					pq_sendint64(&buf, tupcount);

					if (Debug_appendonly_print_insert)
						ereport(LOG,(errmsg("sent tupcount " INT64_FORMAT " for "
											"relation %d", tupcount, relid)));

				}
				resultRelInfo++;
			}
			pq_endmessage(&buf);
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

	/* Report how many tuples we may have inserted into AO tables */
	SendAOTupCounts(estate);

	/* Adjust INSERT/UPDATE/DELETE count for replicated table ON QD */
	AdjustReplicatedTableCounts(estate);

	/*
	 * close the result relation(s) if any, but hold locks until xact commit.
	 */
	resultRelInfo = estate->es_result_relations;
	for (i = 0; i < estate->es_num_result_relations; i++)
	{
		/* end (flush) the INSERT operation in the access layer */
		if (resultRelInfo->ri_aoInsertDesc)
			appendonly_insert_finish(resultRelInfo->ri_aoInsertDesc);
		if (resultRelInfo->ri_aocsInsertDesc)
			aocs_insert_finish(resultRelInfo->ri_aocsInsertDesc);
		if (resultRelInfo->ri_extInsertDesc)
			external_insert_finish(resultRelInfo->ri_extInsertDesc);

		if (resultRelInfo->ri_deleteDesc != NULL)
		{
			if (RelationIsAoRows(resultRelInfo->ri_RelationDesc))
				appendonly_delete_finish(resultRelInfo->ri_deleteDesc);
			else
			{
				Assert(RelationIsAoCols(resultRelInfo->ri_RelationDesc));
				aocs_delete_finish(resultRelInfo->ri_deleteDesc);
			}
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
			resultRelInfo->ri_updateDesc = NULL;
		}
		
		if (resultRelInfo->ri_resultSlot)
		{
			Assert(resultRelInfo->ri_resultSlot->tts_tupleDescriptor);
			ReleaseTupleDesc(resultRelInfo->ri_resultSlot->tts_tupleDescriptor);
			ExecClearTuple(resultRelInfo->ri_resultSlot);
		}

		if (resultRelInfo->ri_PartitionParent)
			relation_close(resultRelInfo->ri_PartitionParent, AccessShareLock);

		/* Close indices and then the relation itself */
		ExecCloseIndices(resultRelInfo);
		heap_close(resultRelInfo->ri_RelationDesc, NoLock);
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
	 * close any relations selected FOR UPDATE/FOR SHARE, again keeping locks
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
 *		Processes the query plan until we have processed 'numberTuples' tuples,
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
			CmdType operation,
			bool sendTuples,
			long numberTuples,
			ScanDirection direction,
			DestReceiver *dest)
{
	TupleTableSlot *slot;
	long		current_tuple_count;

	/*
	 * initialize local variables
	 */
	current_tuple_count = 0;

	/*
	 * Set the direction.
	 */
	estate->es_direction = direction;

	/*
	 * Make sure slice dependencies are met
	 */
	ExecSliceDependencyNode(planstate);

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
			(*dest->receiveSlot) (slot, dest);

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
				if (FaultInjector_InjectFaultIfSet(ExecutorRunHighProcessed,
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
}


/*
 * ExecRelCheck --- check that tuple meets constraints for result relation
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
		 * NOTE: SQL92 specifies that a NULL result from a constraint
		 * expression is not to be treated as a failure.  Therefore, tell
		 * ExecQual to return TRUE for NULL.
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
	TupleConstr *constr = rel->rd_att->constr;

	Assert(constr);

	if (constr->has_not_null)
	{
		int			natts = rel->rd_att->natts;
		int			attrChk;

		for (attrChk = 1; attrChk <= natts; attrChk++)
		{
			if (rel->rd_att->attrs[attrChk - 1]->attnotnull &&
				slot_attisnull(slot, attrChk))
				ereport(ERROR,
						(errcode(ERRCODE_NOT_NULL_VIOLATION),
						 errmsg("null value in column \"%s\" violates not-null constraint",
						NameStr(rel->rd_att->attrs[attrChk - 1]->attname))));
		}
	}

	if (constr->num_check > 0)
	{
		const char *failed;

		if ((failed = ExecRelCheck(resultRelInfo, slot, estate)) != NULL)
			ereport(ERROR,
					(errcode(ERRCODE_CHECK_VIOLATION),
					 errmsg("new row for relation \"%s\" violates check constraint \"%s\"",
							RelationGetRelationName(rel), failed)));
	}
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
 *	*tid - t_ctid from the outdated tuple (ie, next updated version)
 *	priorXmax - t_xmax from the outdated tuple
 *
 * *tid is also an output parameter: it's modified to hold the TID of the
 * latest version of the tuple (note this may be changed even on failure)
 *
 * Returns a slot containing the new candidate update/delete tuple, or
 * NULL if we determine we shouldn't process the row.
 */
TupleTableSlot *
EvalPlanQual(EState *estate, EPQState *epqstate,
			 Relation relation, Index rti,
			 ItemPointer tid, TransactionId priorXmax)
{
	TupleTableSlot *slot;
	HeapTuple	copyTuple;

	Assert(rti > 0);

	/*
	 * Get and lock the updated version of the row; if fail, return NULL.
	 */
	copyTuple = EvalPlanQualFetch(estate, relation, LockTupleExclusive,
								  tid, priorXmax);

	if (copyTuple == NULL)
		return NULL;

	/*
	 * For UPDATE/DELETE we have to return tid of actual row we're executing
	 * PQ for.
	 */
	*tid = copyTuple->t_self;

	/*
	 * Need to run a recheck subquery.	Initialize or reinitialize EPQ state.
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
 *	*tid - t_ctid from the outdated tuple (ie, next updated version)
 *	priorXmax - t_xmax from the outdated tuple
 *
 * Returns a palloc'd copy of the newest tuple version, or NULL if we find
 * that there is no newest version (ie, the row was deleted not updated).
 * If successful, we have locked the newest tuple version, so caller does not
 * need to worry about it changing anymore.
 *
 * Note: properly, lockmode should be declared as enum LockTupleMode,
 * but we use "int" to avoid having to include heapam.h in executor.h.
 */
HeapTuple
EvalPlanQualFetch(EState *estate, Relation relation, int lockmode,
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
			ItemPointerData update_ctid;
			TransactionId update_xmax;

			/*
			 * If xmin isn't what we're expecting, the slot must have been
			 * recycled and reused for an unrelated tuple.	This implies that
			 * the latest version of the row was deleted, so we need do
			 * nothing.  (Should be safe to examine xmin without getting
			 * buffer's content lock, since xmin never changes in an existing
			 * tuple.)
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
			 * wait for its commit/abort.
			 */
			if (TransactionIdIsValid(SnapshotDirty.xmax))
			{
				ReleaseBuffer(buffer);
				XactLockTableWait(SnapshotDirty.xmax);
				continue;		/* loop back to repeat heap_fetch */
			}

			/*
			 * If tuple was inserted by our own transaction, we have to check
			 * cmin against es_output_cid: cmin >= current CID means our
			 * command cannot see the tuple, so we should ignore it.  Without
			 * this we are open to the "Halloween problem" of indefinitely
			 * re-updating the same tuple. (We need not check cmax because
			 * HeapTupleSatisfiesDirty will consider a tuple deleted by our
			 * transaction dead, regardless of cmax.)  We just checked that
			 * priorXmax == xmin, so we can test that variable instead of
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
			test = heap_lock_tuple(relation, &tuple, &buffer,
								   &update_ctid, &update_xmax,
								   estate->es_output_cid,
								   lockmode, false);
			/* We now have two pins on the buffer, get rid of one */
			ReleaseBuffer(buffer);

			switch (test)
			{
				case HeapTupleSelfUpdated:
					/* treat it as deleted; do not process */
					ReleaseBuffer(buffer);
					return NULL;

				case HeapTupleMayBeUpdated:
					/* successfully locked */
					break;

				case HeapTupleUpdated:
					ReleaseBuffer(buffer);
					if (IsXactIsoLevelSerializable)
						ereport(ERROR,
								(errcode(ERRCODE_T_R_SERIALIZATION_FAILURE),
								 errmsg("could not serialize access due to concurrent update")));
					if (!ItemPointerEquals(&update_ctid, &tuple.t_self))
					{
						/* it was updated, so look at the updated version */
						tuple.t_self = update_ctid;
						/* updated row should have xmin matching this xmax */
						priorXmax = update_xmax;
						continue;
					}
					/* tuple was deleted, so give up */
					return NULL;

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
		priorXmax = HeapTupleHeaderGetXmax(tuple.t_data);
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
 * Note: subplan can be NULL if it will be set later with EvalPlanQualSetPlan.
 */
void
EvalPlanQualInit(EPQState *epqstate, EState *estate,
				 Plan *subplan, int epqParam)
{
	/* Mark the EPQ state inactive */
	epqstate->estate = NULL;
	epqstate->planstate = NULL;
	epqstate->origslot = NULL;
	/* ... and remember data that EvalPlanQualBegin will need */
	epqstate->plan = subplan;
	epqstate->rowMarks = NIL;
	epqstate->epqParam = epqParam;
}

/*
 * EvalPlanQualSetPlan -- set or change subplan of an EPQState.
 *
 * We need this so that ModifyTuple can deal with multiple subplans.
 */
void
EvalPlanQualSetPlan(EPQState *epqstate, Plan *subplan)
{
	/* If we have a live EPQ query, shut it down */
	EvalPlanQualEnd(epqstate);
	/* And set/change the plan pointer */
	epqstate->plan = subplan;
}

/*
 * EvalPlanQualAddRowMark -- add an ExecRowMark that EPQ needs to handle.
 *
 * Currently, only non-locking RowMarks are supported.
 */
void
EvalPlanQualAddRowMark(EPQState *epqstate, ExecRowMark *erm)
{
	if (RowMarkRequiresRowShareLock(erm->markType))
		elog(ERROR, "EvalPlanQual doesn't support locking rowmarks");
	epqstate->rowMarks = lappend(epqstate->rowMarks, erm);
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
 * to be scanned by an EvalPlanQual operation.	origslot must have been set
 * to contain the current result row (top-level row) that we need to recheck.
 */
void
EvalPlanQualFetchRowMarks(EPQState *epqstate)
{
	ListCell   *l;

	Assert(epqstate->origslot != NULL);

	foreach(l, epqstate->rowMarks)
	{
		ExecRowMark *erm = (ExecRowMark *) lfirst(l);
		Datum		datum;
		bool		isNull;
		HeapTupleData tuple;

		/* clear any leftover test tuple for this rel */
		EvalPlanQualSetTuple(epqstate, erm->rti, NULL);

		if (erm->relation)
		{
			Buffer		buffer;

			Assert(erm->markType == ROW_MARK_REFERENCE);

			/* if child rel, must check whether it produced this row */
			if (erm->rti != erm->prti)
			{
				Oid			tableoid;

				datum = ExecGetJunkAttribute(epqstate->origslot,
											 erm->toidAttNo,
											 &isNull);
				/* non-locked rels could be on the inside of outer joins */
				if (isNull)
					continue;
				tableoid = DatumGetObjectId(datum);

				if (tableoid != RelationGetRelid(erm->relation))
				{
					/* this child is inactive right now */
					continue;
				}
			}

			/* fetch the tuple's ctid */
			datum = ExecGetJunkAttribute(epqstate->origslot,
										 erm->ctidAttNo,
										 &isNull);
			/* non-locked rels could be on the inside of outer joins */
			if (isNull)
				continue;
			tuple.t_self = *((ItemPointer) DatumGetPointer(datum));

			/* okay, fetch the tuple */
			if (!heap_fetch(erm->relation, SnapshotAny, &tuple, &buffer,
							false, NULL))
				elog(ERROR, "failed to fetch tuple for EvalPlanQual recheck");

			/* successful, copy and store tuple */
			EvalPlanQualSetTuple(epqstate, erm->rti,
								 heap_copytuple(&tuple));
			ReleaseBuffer(buffer);
		}
		else
		{
			HeapTupleHeader td;

			Assert(erm->markType == ROW_MARK_COPY);

			/* fetch the whole-row Var for the relation */
			datum = ExecGetJunkAttribute(epqstate->origslot,
										 erm->wholeAttNo,
										 &isNull);
			/* non-locked rels could be on the inside of outer joins */
			if (isNull)
				continue;
			td = DatumGetHeapTupleHeader(datum);

			/* build a temporary HeapTuple control structure */
			tuple.t_len = HeapTupleHeaderGetDatumLength(td);
			ItemPointerSetInvalid(&(tuple.t_self));
			tuple.t_data = td;

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
			int			i = parentestate->es_plannedstmt->nParamExec;

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
	 */
	estate->es_direction = ForwardScanDirection;
	estate->es_snapshot = parentestate->es_snapshot;
	estate->es_crosscheck_snapshot = parentestate->es_crosscheck_snapshot;
	estate->es_range_table = parentestate->es_range_table;
	estate->es_plannedstmt = parentestate->es_plannedstmt;
	estate->es_junkFilter = parentestate->es_junkFilter;
	estate->es_output_cid = parentestate->es_output_cid;
	estate->es_result_relations = parentestate->es_result_relations;
	estate->es_num_result_relations = parentestate->es_num_result_relations;
	estate->es_result_relation_info = parentestate->es_result_relation_info;
	/* es_trig_target_relations must NOT be copied */
	estate->es_rowMarks = parentestate->es_rowMarks;
	estate->es_instrument = parentestate->es_instrument;
	estate->es_select_into = parentestate->es_select_into;
	estate->es_into_oids = parentestate->es_into_oids;

	/*
	 * The external param list is simply shared from parent.  The internal
	 * param workspace has to be local state, but we copy the initial values
	 * from the parent, so as to have access to any param values that were
	 * already set from other parts of the parent's plan tree.
	 */
	estate->es_param_list_info = parentestate->es_param_list_info;
	if (parentestate->es_plannedstmt->nParamExec > 0)
	{
		int			i = parentestate->es_plannedstmt->nParamExec;

		estate->es_param_exec_vals = (ParamExecData *)
			palloc0(i * sizeof(ParamExecData));
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
	 * nested EPQ checks they should share es_epqTuple arrays.	This allows
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
 * just sharing from the outer query).	We do, however, have to close any
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

/*
 * Support for SELECT INTO (a/k/a CREATE TABLE AS)
 *
 * We implement SELECT INTO by diverting SELECT's normal output with
 * a specialized DestReceiver type.
 */

typedef struct
{
	DestReceiver pub;			/* publicly-known function pointers */
	EState	   *estate;			/* EState we are working with */
	Relation	rel;			/* Relation to write to */
	int			hi_options;		/* heap_insert performance options */
	BulkInsertState bistate;	/* bulk insert state */

	struct AppendOnlyInsertDescData *ao_insertDesc; /* descriptor to AO tables */
	struct AOCSInsertDescData *aocs_ins;           /* descriptor for aocs */

	ItemPointerData last_heap_tid;

} DR_intorel;

/*
 * OpenIntoRel --- actually create the SELECT INTO target relation
 *
 * This also replaces QueryDesc->dest with the special DestReceiver for
 * SELECT INTO.  We assume that the correct result tuple type has already
 * been placed in queryDesc->tupDesc.
 */
static void
OpenIntoRel(QueryDesc *queryDesc)
{
	IntoClause *into = queryDesc->plannedstmt->intoClause;
	EState	   *estate = queryDesc->estate;
	Relation	intoRelationDesc;
	char	   *intoName;
	char		relkind = RELKIND_RELATION;
	char		relstorage;
	Oid			namespaceId;
	Oid			tablespaceId;
	Datum		reloptions;
	StdRdOptions *stdRdOptions;
	AclResult	aclresult;
	Oid			intoRelationId;
	TupleDesc	tupdesc;
	DR_intorel *myState;
	static char *validnsps[] = HEAP_RELOPT_NAMESPACES;
	char	   *intoTableSpaceName;
    GpPolicy   *targetPolicy;
	bool		use_wal;
	bool		validate_reloptions;

	RelFileNode relFileNode;
	
	targetPolicy = queryDesc->plannedstmt->intoPolicy;

	Assert(into);

	/*
	 * XXX This code needs to be kept in sync with DefineRelation(). Maybe we
	 * should try to use that function instead.
	 */

	/*
	 * Check consistency of arguments
	 */
	if (into->onCommit != ONCOMMIT_NOOP && !into->rel->istemp)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TABLE_DEFINITION),
				 errmsg("ON COMMIT can only be used on temporary tables")));

	/*
	 * Security check: disallow creating temp tables from security-restricted
	 * code.  This is needed because calling code might not expect untrusted
	 * tables to appear in pg_temp at the front of its search path.
	 */
	if (into->rel->istemp && InSecurityRestrictedOperation())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("cannot create temporary table within security-restricted operation")));

	/*
	 * Find namespace to create in, check its permissions
	 */
	intoName = into->rel->relname;
	namespaceId = RangeVarGetCreationNamespace(into->rel);

	aclresult = pg_namespace_aclcheck(namespaceId, GetUserId(),
									  ACL_CREATE);
	if (aclresult != ACLCHECK_OK)
		aclcheck_error(aclresult, ACL_KIND_NAMESPACE,
					   get_namespace_name(namespaceId));

	/*
	 * Select tablespace to use.  If not specified, use default tablespace
	 * (which may in turn default to database's default).
	 *
	 * In PostgreSQL, we resolve default tablespace here. In GPDB, that's
	 * done earlier, because we need to dispatch the final tablespace name,
	 * after resolving any defaults, to the segments. (Otherwise, we would
	 * rely on the assumption that default_tablespace GUC is kept in sync
	 * in all segment connections. That actually seems to be the case, as of
	 * this writing, but better to not rely on it.) So usually, we already
	 * have the fully-resolved tablespace name stashed in queryDesc->ddesc->
	 * intoTableSpaceName. In the dispatcher, we filled it in earlier, and
	 * in executor nodes, we received it from the dispatcher along with the
	 * query. In utility mode, however, queryDesc->ddesc is not set at all,
	 * and we follow the PostgreSQL codepath, resolving the defaults here.
	 */
	if (queryDesc->ddesc)
		intoTableSpaceName = queryDesc->ddesc->intoTableSpaceName;
	else
		intoTableSpaceName = into->tableSpaceName;

	if (intoTableSpaceName)
	{
		tablespaceId = get_tablespace_oid(intoTableSpaceName, false);
	}
	else
	{
		tablespaceId = GetDefaultTablespace(into->rel->istemp);
		/* note InvalidOid is OK in this case */
	}

	/* Check permissions except when using the database's default space */
	if (OidIsValid(tablespaceId) && tablespaceId != MyDatabaseTableSpace)
	{
		AclResult	aclresult;

		aclresult = pg_tablespace_aclcheck(tablespaceId, GetUserId(),
										   ACL_CREATE);

		if (aclresult != ACLCHECK_OK)
			aclcheck_error(aclresult, ACL_KIND_TABLESPACE,
						   get_tablespace_name(tablespaceId));
	}

	/* In all cases disallow placing user relations in pg_global */
	if (tablespaceId == GLOBALTABLESPACE_OID)
		ereport(ERROR,
		        (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
		         errmsg("only shared relations can be placed in pg_global tablespace")));

	/* Parse and validate any reloptions */
	reloptions = transformRelOptions((Datum) 0,
									 into->options,
									 NULL,
									 validnsps,
									 true,
									 false);

	/* get the relstorage (heap or AO tables) */
	if (queryDesc->ddesc)
		validate_reloptions = queryDesc->ddesc->validate_reloptions;
	else
		validate_reloptions = true;

	stdRdOptions = (StdRdOptions*) heap_reloptions(relkind, reloptions, validate_reloptions);
	if(stdRdOptions->appendonly)
		relstorage = stdRdOptions->columnstore ? RELSTORAGE_AOCOLS : RELSTORAGE_AOROWS;
	else
		relstorage = RELSTORAGE_HEAP;

	/* Copy the tupdesc because heap_create_with_catalog modifies it */
	tupdesc = CreateTupleDescCopy(queryDesc->tupDesc);

	/* MPP-8405: disallow OIDS on partitioned tables */
	if (tupdesc->tdhasoid && IsNormalProcessingMode() && Gp_role == GP_ROLE_DISPATCH)
	{
		if (relstorage == RELSTORAGE_AOCOLS)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("OIDS=TRUE is not allowed on tables that use column-oriented storage. Use OIDS=FALSE")));
		else
			ereport(NOTICE,
					(errmsg("OIDS=TRUE is not recommended for user-created tables. Use OIDS=FALSE to prevent wrap-around of the OID counter")));
	}

	/*
	 * We can skip WAL-logging the insertions for FileRep on segments, but not on
	 * master since we are using the WAL based physical replication.
	 *
	 * GPDP does not support PITR.
	 */
	use_wal = XLogIsNeeded();

	/* Now we can actually create the new relation */
	intoRelationId = heap_create_with_catalog(intoName,
											  namespaceId,
											  tablespaceId,
											  InvalidOid,
											  InvalidOid,
											  InvalidOid,
											  GetUserId(),
											  tupdesc,
											  NIL,
											  /* relam */ InvalidOid,
											  relkind,
											  relstorage,
											  false,
											  false,
											  true,
											  0,
											  into->onCommit,
											  targetPolicy,  	/* MPP */
											  reloptions,
											  true,
											  allowSystemTableModsDDL,
											  /* valid_opts */ !validate_reloptions);

	FreeTupleDesc(tupdesc);

	/*
	 * Advance command counter so that the newly-created relation's catalog
	 * tuples will be visible to heap_open.
	 */
	CommandCounterIncrement();

	/*
	 * If necessary, create a TOAST table for the new relation, or an Append
	 * Only segment table. Note that AlterTableCreateXXXTable ends with
	 * CommandCounterIncrement(), so that the new tables will be visible for
	 * insertion.
	 */
	reloptions = transformRelOptions((Datum) 0,
									 into->options,
									 "toast",
									 validnsps,
									 true,
									 false);

	(void) heap_reloptions(RELKIND_TOASTVALUE, reloptions, true);
	AlterTableCreateToastTable(intoRelationId, reloptions, false);
	AlterTableCreateAoSegTable(intoRelationId, false);
	AlterTableCreateAoVisimapTable(intoRelationId, false);

    /* don't create AO block directory here, it'll be created when needed */
	/*
	 * And open the constructed table for writing.
	 */
	intoRelationDesc = heap_open(intoRelationId, AccessExclusiveLock);
	
	/*
	 * Add column encoding entries based on the WITH clause.
	 *
	 * NOTE:  we could also do this expansion during parse analysis, by
	 * expanding the IntoClause options field into some attr_encodings field
	 * (cf. CreateStmt and transformCreateStmt()). As it stands, there's no real
	 * benefit for doing that from a code complexity POV. In fact, it would mean
	 * more code. If, however, we supported column encoding syntax during CTAS,
	 * it would be a good time to relocate this code.
	 */
	AddDefaultRelationAttributeOptions(intoRelationDesc,
									   into->options);

	/*
	 * Now replace the query's DestReceiver with one for SELECT INTO
	 */
	queryDesc->dest = CreateDestReceiver(DestIntoRel);
	myState = (DR_intorel *) queryDesc->dest;
	Assert(myState->pub.mydest == DestIntoRel);
	myState->estate = estate;
	myState->rel = intoRelationDesc;

	/*
	 * We can skip WAL-logging the insertions, unless PITR or streaming
	 * replication is in use. We can skip the FSM in any case.
	 */
	myState->hi_options = HEAP_INSERT_SKIP_FSM |
		(use_wal ? 0 : HEAP_INSERT_SKIP_WAL);
	myState->bistate = GetBulkInsertState();

	/* Not using WAL requires smgr_targblock be initially invalid */
	Assert(RelationGetTargetBlock(intoRelationDesc) == InvalidBlockNumber);

	relFileNode.spcNode = tablespaceId;
	relFileNode.dbNode = MyDatabaseId;
	relFileNode.relNode = intoRelationId;
}

/*
 * CloseIntoRel --- clean up SELECT INTO at ExecutorEnd time
 */
static void
CloseIntoRel(QueryDesc *queryDesc)
{
	DR_intorel *myState = (DR_intorel *) queryDesc->dest;

	/* Partition with SELECT INTO is not supported */
	Assert(!PointerIsValid(queryDesc->estate->es_result_partitions));

	/* OpenIntoRel might never have gotten called */
	if (myState && myState->pub.mydest == DestIntoRel && myState->rel)
	{
		Relation	rel = myState->rel;

		FreeBulkInsertState(myState->bistate);

		/*
		 * APPEND_ONLY is closed in the intorel_shutdown.
		 * If we skipped using WAL, must heap_sync before commit.
		 */
		if (RelationIsHeap(rel) && (myState->hi_options & HEAP_INSERT_SKIP_WAL) != 0)
		{
			FlushRelationBuffers(rel);
			/* FlushRelationBuffers will have opened rd_smgr */
			smgrimmedsync(rel->rd_smgr, MAIN_FORKNUM);
		}

		/* close rel, but keep lock until commit */
		heap_close(rel, NoLock);

		myState->rel = NULL;
	}
}

/*
 * Get the OID of the relation created for SELECT INTO or CREATE TABLE AS.
 *
 * To be called between ExecutorStart and ExecutorEnd.
 */
Oid
GetIntoRelOid(QueryDesc *queryDesc)
{
	DR_intorel *myState = (DR_intorel *) queryDesc->dest;

	if (myState && myState->pub.mydest == DestIntoRel && myState->rel)
		return RelationGetRelid(myState->rel);
	else
		return InvalidOid;
}

/*
 * CreateIntoRelDestReceiver -- create a suitable DestReceiver object
 */
DestReceiver *
CreateIntoRelDestReceiver(void)
{
	DR_intorel *self = (DR_intorel *) palloc0(sizeof(DR_intorel));

	self->pub.receiveSlot = intorel_receive;
	self->pub.rStartup = intorel_startup;
	self->pub.rShutdown = intorel_shutdown;
	self->pub.rDestroy = intorel_destroy;
	self->pub.mydest = DestIntoRel;

	self->estate = NULL;
	self->ao_insertDesc = NULL;
	self->aocs_ins = NULL;

	/* private fields will be set by OpenIntoRel */

	return (DestReceiver *) self;
}

/*
 * intorel_startup --- executor startup
 */
static void
intorel_startup(DestReceiver *self, int operation, TupleDesc typeinfo)
{
	/* no-op */
}

/*
 * intorel_receive --- receive one tuple
 */
static void
intorel_receive(TupleTableSlot *slot, DestReceiver *self)
{
	DR_intorel *myState = (DR_intorel *) self;
	Relation	into_rel = myState->rel;

	Assert(myState->estate->es_result_partitions == NULL);

	if (RelationIsAoRows(into_rel))
	{
		MemTuple	tuple = ExecCopySlotMemTuple(slot);
		AOTupleId	aoTupleId;

		if (myState->ao_insertDesc == NULL)
			myState->ao_insertDesc = appendonly_insert_init(into_rel, RESERVED_SEGNO, false);

		appendonly_insert(myState->ao_insertDesc, tuple, InvalidOid, &aoTupleId);
		pfree(tuple);
	}
	else if (RelationIsAoCols(into_rel))
	{
		if(myState->aocs_ins == NULL)
			myState->aocs_ins = aocs_insert_init(into_rel, RESERVED_SEGNO, false);

		aocs_insert(myState->aocs_ins, slot);
	}
	else
	{
		HeapTuple	tuple;

		/*
		 * get the heap tuple out of the tuple table slot, making sure we have a
		 * writable copy
		 */
		tuple = ExecMaterializeSlot(slot);

		/*
		 * force assignment of new OID (see comments in ExecInsert)
		 */
		if (myState->rel->rd_rel->relhasoids)
			HeapTupleSetOid(tuple, InvalidOid);

		heap_insert(into_rel,
					tuple,
					myState->estate->es_output_cid,
					myState->hi_options,
					myState->bistate,
					GetCurrentTransactionId());

		myState->last_heap_tid = tuple->t_self;
	}

	/* We know this is a newly created relation, so there are no indexes */
}

/*
 * intorel_shutdown --- executor end
 */
static void
intorel_shutdown(DestReceiver *self)
{
	/* If target was append only, finalise */
	DR_intorel *myState = (DR_intorel *) self;
	Relation	into_rel = myState->rel;


	if (RelationIsAoRows(into_rel) && myState->ao_insertDesc)
		appendonly_insert_finish(myState->ao_insertDesc);
	else if (RelationIsAoCols(into_rel) && myState->aocs_ins)
        aocs_insert_finish(myState->aocs_ins);
}

/*
 * intorel_destroy --- release DestReceiver object
 */
static void
intorel_destroy(DestReceiver *self)
{
	pfree(self);
}

/*
 * Calculate the part to use for the given key, then find or calculate
 * and cache required information about that part in the hash table
 * anchored in estate.
 * 
 * Return a pointer to the information, an entry in the table
 * estate->es_result_relations.  Note that the first entry in this
 * table is for the partitioned table itself and that the entire table
 * may be reallocated, changing the addresses of its entries.  
 *
 * Thus, it is important to avoid holding long-lived pointers to 
 * table entries (such as the pointer returned from this function).
 */
static ResultRelInfo *
get_part(EState *estate, Datum *values, bool *isnull, TupleDesc tupdesc)
{
	ResultRelInfo *resultRelInfo;
	Oid targetid;
	bool found;
	ResultPartHashEntry *entry;

	/* add a short term memory context if one wasn't assigned already */
	Assert(estate->es_partition_state != NULL &&
		estate->es_partition_state->accessMethods != NULL);
	if (!estate->es_partition_state->accessMethods->part_cxt)
		estate->es_partition_state->accessMethods->part_cxt =
			GetPerTupleExprContext(estate)->ecxt_per_tuple_memory;

	targetid = selectPartition(estate->es_result_partitions, values,
							   isnull, tupdesc, estate->es_partition_state->accessMethods);

	if (!OidIsValid(targetid))
		ereport(ERROR,
				(errcode(ERRCODE_NO_PARTITION_FOR_PARTITIONING_KEY),
				 errmsg("no partition for partitioning key")));

	if (estate->es_partition_state->result_partition_hash == NULL)
	{
		HASHCTL ctl;
		long num_buckets;

		/* reasonable assumption? */
		num_buckets =
			list_length(all_partition_relids(estate->es_result_partitions));
		num_buckets /= num_partition_levels(estate->es_result_partitions);

		ctl.keysize = sizeof(Oid);
		ctl.entrysize = sizeof(*entry);
		ctl.hash = oid_hash;

		estate->es_partition_state->result_partition_hash =
			hash_create("Partition Result Relation Hash",
						num_buckets,
						&ctl,
						HASH_ELEM | HASH_FUNCTION);
	}

	entry = hash_search(estate->es_partition_state->result_partition_hash,
						&targetid,
						HASH_ENTER,
						&found);

	if (found)
	{
		resultRelInfo = estate->es_result_relations;
		resultRelInfo += entry->offset;
		Assert(RelationGetRelid(resultRelInfo->ri_RelationDesc) == targetid);
	}
	else
	{
		int result_array_size =
			estate->es_partition_state->result_partition_array_size;
		int natts;
		Relation	resultRelation;

		if (estate->es_num_result_relations + 1 >= result_array_size)
		{
			int32 sz = result_array_size * 2;

			/* we shouldn't be able to overflow */
			Insist((int)sz > result_array_size);

			estate->es_result_relation_info = estate->es_result_relations =
					(ResultRelInfo *)repalloc(estate->es_result_relations,
										 	  sz * sizeof(ResultRelInfo));
			estate->es_partition_state->result_partition_array_size = (int)sz;
		}

		resultRelInfo = estate->es_result_relations;
		natts = resultRelInfo->ri_RelationDesc->rd_att->natts; /* in base relation */
		resultRelInfo += estate->es_num_result_relations;
		entry->offset = estate->es_num_result_relations;

		estate->es_num_result_relations++;

		resultRelation = heap_open(targetid, RowExclusiveLock);
		InitResultRelInfo(resultRelInfo,
						  resultRelation,
						  1,
						  CMD_INSERT,
						  estate->es_instrument);
		
		map_part_attrs(estate->es_result_relations->ri_RelationDesc, 
					   resultRelInfo->ri_RelationDesc,
					   &(resultRelInfo->ri_partInsertMap),
					   TRUE); /* throw on error, so result not needed */

		if (resultRelInfo->ri_partInsertMap)
			resultRelInfo->ri_partSlot = 
				MakeSingleTupleTableSlot(resultRelInfo->ri_RelationDesc->rd_att);
	}
	return resultRelInfo;
}

ResultRelInfo *
values_get_partition(Datum *values, bool *nulls, TupleDesc tupdesc,
					 EState *estate)
{
	ResultRelInfo *relinfo;

	Assert(PointerIsValid(estate->es_result_partitions));

	relinfo = get_part(estate, values, nulls, tupdesc);

	return relinfo;
}

/*
 * Find the partition we want and get the ResultRelInfo for the
 * partition.
 */
ResultRelInfo *
slot_get_partition(TupleTableSlot *slot, EState *estate)
{
	ResultRelInfo *resultRelInfo;
	AttrNumber max_attr;
	Datum *values;
	bool *nulls;

	Assert(PointerIsValid(estate->es_result_partitions));

	max_attr = estate->es_partition_state->max_partition_attr;

	slot_getsomeattrs(slot, max_attr);
	values = slot_get_values(slot);
	nulls = slot_get_isnull(slot);

	resultRelInfo = get_part(estate, values, nulls, slot->tts_tupleDescriptor);

	return resultRelInfo;
}

/* Wrap an attribute map (presumably from base partitioned table to part
 * as created by map_part_attrs in execMain.c) with an AttrMap. The new
 * AttrMap will contain a copy of the argument map.  The caller retains
 * the responsibility to dispose of the argument map eventually.
 *
 * If the input AttrNumber vector is empty or null, it is taken as an
 * identity map, i.e., a null AttrMap.
 */
AttrMap *makeAttrMap(int base_count, AttrNumber *base_map)
{
	int i, n, p;
	AttrMap *map;
	
	if ( base_count < 1 || base_map == NULL )
		return NULL;
	
	map = palloc0(sizeof(AttrMap) + base_count * sizeof(AttrNumber));
	
	for ( i = n = p = 0; i <= base_count; i++ )
	{
		map->attr_map[i] = base_map[i];
		
		if ( map->attr_map[i] != 0 ) 
		{
			if ( map->attr_map[i] > p ) p = map->attr_map[i];
			n++;
		}
	}	
	
	map->live_count = n;
	map->attr_max = p;
	map->attr_count = base_count;
	
	return map;
}

/* Use the given attribute map to convert an attribute number in the
 * base relation to an attribute number in the other relation.  Forgive
 * out-of-range attributes by mapping them to zero.  Treat null as
 * the identity map.
 */
AttrNumber attrMap(AttrMap *map, AttrNumber anum)
{
	if ( map == NULL )
		return anum;
	if ( 0 < anum && anum <= map->attr_count )
		return map->attr_map[anum];
	return 0;
}

/* For attrMapExpr below.
 *
 * Mutate Var nodes in an expression using the given attribute map.
 * Insist the Var nodes have varno == 1 and the that the mapping
 * yields a live attribute number (non-zero).
 */
static Node *apply_attrmap_mutator(Node *node, AttrMap *map)
{
	if ( node == NULL )
		return NULL;
	
	if (IsA(node, Var) )
	{
		AttrNumber anum = 0;
		Var *var = (Var*)node;
		Assert(var->varno == 1); /* in CHECK constraints */
		anum = attrMap(map, var->varattno);
		
		if ( anum == 0 )
		{
			/* Should never happen, but best caught early. */
			elog(ERROR, "attribute map discrepancy");
		}
		else if ( anum != var->varattno )
		{
			var = copyObject(var);
			var->varattno = anum;
		}
		return (Node *)var;
	}
	return expression_tree_mutator(node, apply_attrmap_mutator, (void *)map);
}

/* Apply attrMap over the Var nodes in an expression.
 */
Node *attrMapExpr(AttrMap *map, Node *expr)
{
	return apply_attrmap_mutator(expr, map);
}


/* Check compatibility of the attributes of the given partitioned
 * table and part for purposes of INSERT (through the partitioned
 * table) or EXCHANGE (of the part into the partitioned table).
 * Don't use any partitioning catalogs, because this must run
 * on the segment databases as well as on the entry database.
 *
 * If requested and needed, make a vector mapping the attribute
 * numbers of the partitioned table to corresponding attribute 
 * numbers in the part.  Represent the "unneeded" identity map
 * as null.
 *
 * base -- the partitioned table
 * part -- the part table
 * map_ptr -- where to store a pointer to the result, or NULL
 * throw -- whether to throw an error in case of incompatibility
 *
 * The implicit result is a vector one longer than the number
 * of attributes (existing or not) in the base relation.
 * It is returned through the map_ptr argument, if that argument
 * is non-null.
 *
 * The explicit result indicates whether the part is compatible
 * with the base relation.  If the throw argument is true, however,
 * an error is issued rather than returning false.
 *
 * Note that, in the map, element 0 is wasted and is always zero, 
 * so the vector is indexed by attribute number (origin 1).
 *
 * The i-th element of the map is the attribute number in 
 * the part relation that corresponds to attribute i of the  
 * base relation, or it is zero to indicate that attribute 
 * i of the base relation doesn't exist (has been dropped).
 *
 * This is a handy map for renumbering attributes for use with
 * part relations that may have a different configuration of 
 * "holes" than the partitioned table in which they occur.
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
map_part_attrs(Relation base, Relation part, AttrMap **map_ptr, bool throw)
{	
	AttrNumber i = 1;
	AttrNumber n = base->rd_att->natts;
	FormData_pg_attribute *battr = NULL;
	
	AttrNumber j = 1;
	AttrNumber m = part->rd_att->natts;
	FormData_pg_attribute *pattr = NULL;
	
	AttrNumber *v = NULL;
	
	/* If we might want a map, allocate one. */
	if ( map_ptr != NULL )
	{
		v = palloc0(sizeof(AttrNumber)*(n+1));
		*map_ptr = NULL;
	}
	
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
		
		/* If we're building a map, update it. */
		if ( v != NULL )
			v[i] = j;
		
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
		if ( v != NULL )
			pfree(v);
		return FALSE;
	}

	/* If parent and part are the same, don't use a map */
	if ( is_identical && v != NULL )
	{
		pfree(v);
		v = NULL;
	}
	
	if ( map_ptr != NULL && v != NULL )
	{
		*map_ptr = makeAttrMap(n, v);
		pfree(v);
	}
	return TRUE;
}

/*
 * Clear any partition state held in the argument EState node.  This is
 * called during ExecEndPlan and is not, itself, recursive.
 *
 * At present, the only required cleanup is to decrement reference counts
 * in any tuple descriptors held in slots in the partition state.
 */
void
ClearPartitionState(EState *estate)
{
	PartitionState *pstate = estate->es_partition_state;
	HASH_SEQ_STATUS hash_seq_status;
	ResultPartHashEntry *entry;

	if ( pstate == NULL || pstate->result_partition_hash == NULL )
		return;

	/* Examine each hash table entry. */
	hash_freeze(pstate->result_partition_hash);
	hash_seq_init(&hash_seq_status, pstate->result_partition_hash);
	while ( (entry = hash_seq_search(&hash_seq_status)) )
	{
		ResultPartHashEntry *part = (ResultPartHashEntry*)entry;
		ResultRelInfo *info = &estate->es_result_relations[part->offset];
		if ( info->ri_partSlot )
		{
			Assert( info->ri_partInsertMap ); /* paired with slot */
			ExecDropSingleTupleTableSlot(info->ri_partSlot);
		}
	}
	/* No need for hash_seq_term() since we iterated to end. */
	hash_destroy(pstate->result_partition_hash);
	pstate->result_partition_hash = NULL;
}

#if 0 /* for debugging purposes only */
char *
DumpSliceTable(SliceTable *table)
{
	StringInfoData buf;
	ListCell *lc;

	if (!table)
		return "No slice table";

	initStringInfo(&buf);

	foreach(lc, table->slices)
	{
		Slice *slice = (Slice *) lfirst(lc);

		appendStringInfo(&buf, "Slice %d: rootIndex %d gangType %d parent %d\n",
						 slice->sliceIndex, slice->rootIndex, slice->gangType, slice->parentIndex);
	}
	return buf.data;
}
#endif

typedef struct
{
	plan_tree_base_prefix prefix;
	EState	   *estate;
	int			currentSliceId;
} FillSliceTable_cxt;

static bool
FillSliceTable_walker(Node *node, void *context)
{
	FillSliceTable_cxt *cxt = (FillSliceTable_cxt *) context;
	PlannedStmt *stmt = (PlannedStmt *) cxt->prefix.node;
	EState	   *estate = cxt->estate;
	SliceTable *sliceTable = estate->es_sliceTable;
	int			parentSliceIndex = cxt->currentSliceId;
	bool		result;

	if (node == NULL)
		return false;

	/*
	 * When we encounter a ModifyTable node, mark the slice it's in as the
	 * primary writer slice. (There should be only one.)
	 */
	if (IsA(node, ModifyTable))
	{
		ModifyTable *mt = (ModifyTable *) node;

		if (list_length(mt->resultRelations) > 0)
		{
			ListCell   *lc = list_head(mt->resultRelations);
			int			idx = lfirst_int(lc);
			Oid			reloid = getrelid(idx, stmt->rtable);
			GpPolicyType policyType;

			policyType = GpPolicyFetch(CurrentMemoryContext, reloid)->ptype;

#ifdef USE_ASSERT_CHECKING
			{
				lc = lc->next;
				for (; lc != NULL; lc = lnext(lc))
				{
					idx = lfirst_int(lc);
					reloid = getrelid(idx, stmt->rtable);

					if (policyType != GpPolicyFetch(CurrentMemoryContext, reloid)->ptype)
						elog(ERROR, "ModifyTable mixes distributed and entry-only tables");

				}
			}
#endif

			if (policyType != POLICYTYPE_ENTRY)
			{
				Slice	   *currentSlice = (Slice *) list_nth(sliceTable->slices, cxt->currentSliceId);

				currentSlice->gangType = GANGTYPE_PRIMARY_WRITER;
				currentSlice->gangSize = getgpsegmentCount();
			}
		}
	}
	/* A DML node is the same as a ModifyTable node, in ORCA plans. */
	if (IsA(node, DML))
	{
		DML		   *dml = (DML *) node;
		int			idx = dml->scanrelid;
		Oid			reloid = getrelid(idx, stmt->rtable);
		GpPolicyType policyType;

		policyType = GpPolicyFetch(CurrentMemoryContext, reloid)->ptype;

		if (policyType != POLICYTYPE_ENTRY)
		{
			Slice	   *currentSlice = (Slice *) list_nth(sliceTable->slices, cxt->currentSliceId);

			currentSlice->gangType = GANGTYPE_PRIMARY_WRITER;
			currentSlice->gangSize = getgpsegmentCount();
		}
	}

	if (IsA(node, Motion))
	{
		Motion	   *motion = (Motion *) node;
		MemoryContext oldcxt = MemoryContextSwitchTo(estate->es_query_cxt);
		Flow	   *sendFlow;
		Slice	   *sendSlice;
		Slice	   *recvSlice;

		/* Top node of subplan should have a Flow node. */
		Insist(motion->plan.lefttree && motion->plan.lefttree->flow);
		sendFlow = motion->plan.lefttree->flow;

		/* Look up the sending gang's slice table entry. */
		sendSlice = (Slice *) list_nth(sliceTable->slices, motion->motionID);

		/* Look up the receiving (parent) gang's slice table entry. */
		recvSlice = (Slice *)list_nth(sliceTable->slices, parentSliceIndex);

		Assert(IsA(recvSlice, Slice));
		Assert(recvSlice->sliceIndex == parentSliceIndex);
		Assert(recvSlice->rootIndex == 0 ||
			   (recvSlice->rootIndex > sliceTable->nMotions &&
				recvSlice->rootIndex < list_length(sliceTable->slices)));

		/* Sending slice become a children of recv slice */
		recvSlice->children = lappend_int(recvSlice->children, sendSlice->sliceIndex);
		sendSlice->parentIndex = parentSliceIndex;
		sendSlice->rootIndex = recvSlice->rootIndex;

		/* The gang beneath a Motion will be a reader. */
		sendSlice->gangType = GANGTYPE_PRIMARY_READER;

		if (sendFlow->flotype != FLOW_SINGLETON)
		{
			sendSlice->gangSize = getgpsegmentCount();
			sendSlice->gangType = GANGTYPE_PRIMARY_READER;
		}
		else
		{
			sendSlice->gangSize = 1;
			sendSlice->gangType =
				sendFlow->segindex == -1 ?
				GANGTYPE_ENTRYDB_READER : GANGTYPE_SINGLETON_READER;
		}

		sendSlice->numGangMembersToBeActive =
			sliceCalculateNumSendingProcesses(sendSlice);

		MemoryContextSwitchTo(oldcxt);

		/* recurse into children */
		cxt->currentSliceId = motion->motionID;
		result = plan_tree_walker(node, FillSliceTable_walker, cxt);
		cxt->currentSliceId = parentSliceIndex;
		return result;
	}

	if (IsA(node, SubPlan))
	{
		SubPlan *subplan = (SubPlan *) node;

		if (subplan->is_initplan)
		{
			cxt->currentSliceId = subplan->qDispSliceId;
			result = plan_tree_walker(node, FillSliceTable_walker, cxt);
			cxt->currentSliceId = parentSliceIndex;
			return result;
		}
	}

	return plan_tree_walker(node, FillSliceTable_walker, cxt);
}

/*
 * Set up the parent-child relationships in the slice table.
 *
 * We used to do this as part of ExecInitMotion(), but because ExecInitNode()
 * no longer recurses into subplans, at SubPlan nodes, we cannot easily track
 * the parent-child slice relationships across SubPlan nodes at that phase
 * anymore. We now do this separate walk of the whole plantree, recursing
 * into SubPlan nodes, to do the same.
 */
static void
FillSliceTable(EState *estate, PlannedStmt *stmt)
{
	FillSliceTable_cxt cxt;
	SliceTable *sliceTable = estate->es_sliceTable;

	if (!sliceTable)
		return;

	cxt.prefix.node = (Node *) stmt;
	cxt.estate = estate;
	cxt.currentSliceId = 0;

	if (stmt->intoClause)
	{
		Slice	   *currentSlice = (Slice *) linitial(sliceTable->slices);

		currentSlice->gangType = GANGTYPE_PRIMARY_WRITER;
		currentSlice->gangSize = getgpsegmentCount();
	}

	/*
	 * NOTE: We depend on plan_tree_walker() to recurse into subplans of
	 * SubPlan nodes.
	 */
	FillSliceTable_walker((Node *) stmt->planTree, &cxt);
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
	accessMethods->amstate = palloc0(numLevels * sizeof(void *));
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
	partitionState->result_partition_array_size = resultPartSize;

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

	if (Gp_role != GP_ROLE_DISPATCH)
		return;

	/* check if result_relations contain replicated table*/
	for (i = 0; i < estate->es_num_result_relations; i++)
	{
		GpPolicy *policy = NULL;
		resultRelInfo = estate->es_result_relations + i;

		policy = resultRelInfo->ri_RelationDesc->rd_cdbpolicy;
		if (!policy)
			continue;

		if (GpPolicyIsReplicated(policy))
			containReplicatedTable = true;
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
		estate->es_processed = estate->es_processed / getgpsegmentCount();
}

static bool
intoRelIsReplicatedTable(QueryDesc *queryDesc)
{
	DR_intorel *into = (DR_intorel *) queryDesc->dest;

	if (into && into->pub.mydest == DestIntoRel &&
		into->rel && GpPolicyIsReplicated(into->rel->rd_cdbpolicy))
		return true;

	return false;
}
