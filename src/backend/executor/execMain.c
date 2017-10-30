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
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/executor/execMain.c,v 1.313 2008/08/25 22:42:32 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "gpmon/gpmon.h"

#include "access/heapam.h"
#include "access/aosegfiles.h"
#include "access/appendonlywriter.h"
#include "access/fileam.h"
#include "access/reloptions.h"
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
#include "executor/nodeSubplan.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h" /* temporary */
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "parser/parse_clause.h"
#include "parser/parsetree.h"
#include "storage/smgr.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/tqual.h"

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
#include "cdb/cdbmirroredbufferpool.h"
#include "cdb/cdbpersistentstore.h"
#include "cdb/cdbpersistentfilesysobj.h"
#include "cdb/cdbllize.h"
#include "cdb/memquota.h"
#include "cdb/cdbtargeteddispatch.h"

extern bool cdbpathlocus_querysegmentcatalogs;


typedef struct evalPlanQual
{
	Index		rti;
	EState	   *estate;
	PlanState  *planstate;
	struct evalPlanQual *next;	/* stack of active PlanQual plans */
	struct evalPlanQual *free;	/* list of free PlanQual plans */
} evalPlanQual;

/* decls for local routines only used within this module */
static void InitPlan(QueryDesc *queryDesc, int eflags);
static void ExecCheckPlanOutput(Relation resultRel, List *targetList);
static void ExecEndPlan(PlanState *planstate, EState *estate);
static TupleTableSlot *ExecutePlan(EState *estate, PlanState *planstate,
			CmdType operation,
			long numberTuples,
			ScanDirection direction,
			DestReceiver *dest);
static void ExecSelect(TupleTableSlot *slot,
		   DestReceiver *dest, EState *estate);

static TupleTableSlot *EvalPlanQualNext(EState *estate);
static void EndEvalPlanQual(EState *estate);
static void ExecCheckXactReadOnly(PlannedStmt *plannedstmt);
static void EvalPlanQualStart(evalPlanQual *epq, EState *estate,
				  evalPlanQual *priorepq);
static void EvalPlanQualStop(evalPlanQual *epq);
static void OpenIntoRel(QueryDesc *queryDesc);
static void CloseIntoRel(QueryDesc *queryDesc);
static void intorel_startup(DestReceiver *self, int operation, TupleDesc typeinfo);
static void intorel_receive(TupleTableSlot *slot, DestReceiver *self);
static void intorel_shutdown(DestReceiver *self);
static void intorel_destroy(DestReceiver *self);

static void FillSliceTable(EState *estate, PlannedStmt *stmt);

void ExecCheckRTPerms(List *rangeTable);
void ExecCheckRTEPerms(RangeTblEntry *rte);

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
 * MPP: In here we take care of setting up all the necessary items that
 * will be needed to service the query, such as setting up interconnect,
 * and dispatching the query. Any other items in the future
 * must be added here.
 * ----------------------------------------------------------------
 */
void
ExecutorStart(QueryDesc *queryDesc, int eflags)
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

	Assert(queryDesc->plannedstmt->intoPolicy == NULL
		   || queryDesc->plannedstmt->intoPolicy->ptype == POLICYTYPE_PARTITIONED);

	/**
	 * Perfmon related stuff.
	 */
	if (gp_enable_gpperfmon
		&& Gp_role == GP_ROLE_DISPATCH
		&& queryDesc->gpmon_pkt)
	{
		gpmon_qlog_query_start(queryDesc->gpmon_pkt);
	}

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
	 * Fill in parameters, if any, from queryDesc
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
	estate->es_snapshot = queryDesc->snapshot;
	estate->es_crosscheck_snapshot = queryDesc->crosscheck_snapshot;
	estate->es_instrument = queryDesc->doInstrument;
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
		estate->es_sliceTable->doInstrument = queryDesc->doInstrument;

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
			estate->es_instrument = sliceTable->doInstrument;
			queryDesc->doInstrument = sliceTable->doInstrument;
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
			/* Assign gang descriptions to the root slices of the slice forest. */
			InitRootSlices(queryDesc);

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
 *		MPP: In here we must ensure to only run the plan and not call
 *		any setup/teardown items (unless in a CATCH block).
 *
 * ----------------------------------------------------------------
 */
TupleTableSlot *
ExecutorRun(QueryDesc *queryDesc,
			ScanDirection direction, long count)
{
	EState	   *estate;
	CmdType		operation;
	DestReceiver *dest;
	bool		sendTuples;
	TupleTableSlot *result = NULL;
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
				   queryDesc->plannedstmt->returningLists));

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
			result = NULL;
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

			result = ExecutePlan(estate,
								 (PlanState *) motionState,
								 CMD_SELECT,
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
			result = ExecutePlan(estate,
								 queryDesc->planstate,
								 operation,
								 count,
								 direction,
								 dest);
		}
		else
		{
			/* should never happen */
			Assert(!"undefined parallel execution strategy");
		}

		/*
		 * if result is null we got end-of-stream. We need to mark it
		 * since with a cursor end-of-stream will only be received with
		 * the fetch that returns the last tuple. ExecutorEnd needs to
		 * know if EOS was received in order to do the right cleanup.
		 */
		if(result == NULL)
			estate->es_got_eos = true;
    }
	PG_CATCH();
	{
        /* If EXPLAIN ANALYZE, let qExec try to return stats to qDisp. */
        if (estate->es_sliceTable &&
            estate->es_sliceTable->doInstrument &&
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

	/*
	 * shutdown tuple receiver, if we started it
	 */
	if (sendTuples)
		(*dest->rShutdown) (dest);

	MemoryContextSwitchTo(oldcontext);
	END_MEMORY_ACCOUNT();

	return result;
}

/* ----------------------------------------------------------------
 *		ExecutorEnd
 *
 *		This routine must be called at the end of execution of any
 *		query plan
 * ----------------------------------------------------------------
 */
void
ExecutorEnd(QueryDesc *queryDesc)
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
        estate->es_sliceTable->doInstrument &&
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
	 * Close the SELECT INTO relation if any
	 */
	if (estate->es_select_into)
		CloseIntoRel(queryDesc);

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

	/* Reset queryDesc fields that no longer point to anything */
	queryDesc->tupDesc = NULL;
	queryDesc->estate = NULL;
	queryDesc->planstate = NULL;
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
	Oid			relOid;
	Oid			userid;

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
	 * We must have *all* the requiredPerms bits, so use aclmask not aclcheck.
	 */
	if (pg_class_aclmask(relOid, userid, requiredPerms, ACLMASK_ALL)
		!= requiredPerms)
	{
		/*
		 * If the table is a partition, return an error message that includes
		 * the name of the parent table.
		 */
		const char *rel_name = get_rel_name_partition(relOid);
		aclcheck_error(ACLCHECK_NO_PRIV, ACL_KIND_CLASS, rel_name);
	}
}

/*
 * This function is used to check if the current statement will perform any writes.
 * It is used to enforce:
 *  (1) read-only mode (both fts and transcation isolation level read only)
 *      as well as
 *  (2) to keep track of when a distributed transaction becomes
 *      "dirty" and will require 2pc.
 Check that the query does not imply any writes to non-temp tables.
 */
static void
ExecCheckXactReadOnly(PlannedStmt *plannedstmt)
{
	ListCell   *l;
    int         rti;
    bool		changesTempTables = false;

	/*
	 * CREATE TABLE AS or SELECT INTO?
	 *
	 * XXX should we allow this if the destination is temp?
	 */
	if (plannedstmt->intoClause != NULL)
	{
		Assert(plannedstmt->intoClause->rel);
		if (plannedstmt->intoClause->rel->istemp)
			changesTempTables = true;
		else
			goto fail;
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
			changesTempTables = true;
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

		goto fail;
	}
	if (changesTempTables)
		ExecutorMarkTransactionDoesWrites();
	return;

fail:
	if (XactReadOnly)
		ereport(ERROR,
				(errcode(ERRCODE_READ_ONLY_SQL_TRANSACTION),
				 errmsg("transaction is read-only")));
	else
		ExecutorMarkTransactionDoesWrites();
}

/*
 * BuildPartitionNodeFromRoot
 *   Build PartitionNode for the root partition of a given partition oid.
 */
static PartitionNode *
BuildPartitionNodeFromRoot(Oid relid)
{
	PartitionNode *partitionNode = NULL;
	
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
 * InitializeResultRelations
 *   Initialize result relation relevant information
 *
 * CDB: Note that we need this info even if we aren't the slice that will be doing
 * the actual updating, since it's where we learn things, such as if the row needs to
 * contain OIDs or not.
 */
static void
InitializeResultRelations(PlannedStmt *plannedstmt, EState *estate, CmdType operation)
{
	List	   *rangeTable = estate->es_range_table;
	ListCell   *l;
	LOCKMODE    lockmode;
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
	lockmode = (Gp_role != GP_ROLE_EXECUTE || Gp_is_writer) ? RowExclusiveLock : NoLock;
	
	Assert(plannedstmt != NULL && estate != NULL);

	if (plannedstmt->resultRelations)
	{
		resultRelInfos = (ResultRelInfo *)
			palloc(numResultRelations * sizeof(ResultRelInfo));
		resultRelInfo = resultRelInfos;
		foreach(l, resultRelations)
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
		/* Initialize to first or only result rel */
		estate->es_result_relation_info = resultRelInfos;
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

		return;
	}

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
	
	estate->es_partition_state = NULL;
	if (estate->es_result_partitions)
	{
		estate->es_partition_state = createPartitionState(estate->es_result_partitions,
														  estate->es_num_result_relations);
	}
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

	Assert(plannedstmt->intoPolicy == NULL
		   || plannedstmt->intoPolicy->ptype == POLICYTYPE_PARTITIONED);

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
	 * Do permissions checks.
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

	/*
	 * initialize result relation stuff
	 *
	 * CDB: Note that we need this info even if we aren't the slice that will be doing
	 * the actual updating, since it's where we learn things, such as if the row needs to
	 * contain OIDs or not.
	 */
	InitializeResultRelations(plannedstmt, estate, operation);

	/*
	 * If there are partitions involved in the query, initialize partitioning metadata.
	 */
	InitializeQueryPartsMetadata(plannedstmt, estate);

	/*
	 * set the number of partition selectors for every dynamic scan id
	 */
	estate->dynamicTableScanInfo->numSelectorsPerScanId = plannedstmt->numSelectorsPerScanId;

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
	 * Have to lock relations selected FOR UPDATE/FOR SHARE before we
	 * initialize the plan tree, else we'd be doing a lock upgrade. While we
	 * are at it, build the ExecRowMark list.
	 */
	estate->es_rowMarks = NIL;
	foreach(l, plannedstmt->rowMarks)
	{
		RowMarkClause *rc = (RowMarkClause *) lfirst(l);
		Oid			relid = getrelid(rc->rti, rangeTable);
		Relation	relation;
		LOCKMODE    lockmode;
		bool        lockUpgraded;
		ExecRowMark *erm;

        /* CDB: On QD, lock whole table in S or X mode, if distributed. */
		lockmode = rc->forUpdate ? RowExclusiveLock : RowShareLock;
		relation = CdbOpenRelation(relid, lockmode, rc->noWait, &lockUpgraded);
		if (lockUpgraded)
		{
            heap_close(relation, NoLock);
            continue;
        }

		/*
		 * Check that relation is a legal target for marking.
		 *
		 * In most cases parser and/or planner should have noticed this
		 * already, but they don't cover all cases.
		 */
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

		erm = (ExecRowMark *) palloc(sizeof(ExecRowMark));
		erm->relation = relation;
		erm->rti = rc->rti;
		erm->forUpdate = rc->forUpdate;
		erm->noWait = rc->noWait;
		/* We'll set up ctidAttno below */
		erm->ctidAttNo = InvalidAttrNumber;
		estate->es_rowMarks = lappend(estate->es_rowMarks, erm);
	}

	/*
	 * Initialize the executor's tuple table.  Also, if it's not a SELECT,
	 * set up a tuple table slot for use for trigger output tuples.
	 */
	estate->es_tupleTable = NIL;
	if (operation != CMD_SELECT)
		estate->es_trig_tuple_slot = ExecInitExtraTupleSlot(estate);

	/* mark EvalPlanQual not active */
	estate->es_plannedstmt = plannedstmt;
	estate->es_evalPlanQual = NULL;
	estate->es_evTupleNull = NULL;
	estate->es_evTuple = NULL;
	estate->es_useEvalPlan = false;

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

	if (RootSliceIndex(estate) != LocallyExecutingSliceIndex(estate))
		return;
	
	/*
	 * Get the tuple descriptor describing the type of tuples to return. (this
	 * is especially important if we are creating a relation with "SELECT
	 * INTO")
	 */
	tupType = ExecGetResultType(planstate);

	/*
	 * Initialize the junk filter if needed.  SELECT and INSERT queries need a
	 * filter if there are any junk attrs in the tlist.  INSERT and SELECT
	 * INTO also need a filter if the plan may return raw disk tuples (else
	 * heap_insert will be scribbling on the source relation!). UPDATE and
	 * DELETE always need a filter, since there's always a junk 'ctid'
	 * attribute present --- no need to look first.
	 *
	 * This section of code is also a convenient place to verify that the
	 * output of an INSERT or UPDATE matches the target table(s).
	 */
	{
		bool		junk_filter_needed = false;
		ListCell   *tlist;

		switch (operation)
		{
			case CMD_SELECT:
			case CMD_INSERT:
				foreach(tlist, plan->targetlist)
				{
					TargetEntry *tle = (TargetEntry *) lfirst(tlist);

					if (tle->resjunk)
					{
						junk_filter_needed = true;
						break;
					}
				}
				if (!junk_filter_needed &&
					(operation == CMD_INSERT || estate->es_select_into) &&
					ExecMayReturnRawTuples(planstate))
					junk_filter_needed = true;
				break;
			case CMD_UPDATE:
			case CMD_DELETE:
				junk_filter_needed = true;
				break;
			default:
				break;
		}

		if (junk_filter_needed)
		{
			/*
			 * If there are multiple result relations, each one needs its own
			 * junk filter.  Note this is only possible for UPDATE/DELETE, so
			 * we can't be fooled by some needing a filter and some not.
			 */
			if (list_length(plannedstmt->resultRelations) > 1)
			{
				List *appendplans;
				int			as_nplans;
				ResultRelInfo *resultRelInfo;
				ListCell *lc;

				/* Top plan had better be an Append here. */
				Assert(IsA(plannedstmt->planTree, Append));
				Assert(((Append *) plannedstmt->planTree)->isTarget);
				Assert(IsA(planstate, AppendState));
				appendplans = ((Append *) plannedstmt->planTree)->appendplans;
				as_nplans = list_length(appendplans);
				Assert(as_nplans == estate->es_num_result_relations);
				resultRelInfo = estate->es_result_relations;
				foreach(lc, appendplans)
				{
					Plan  *subplan = (Plan *)lfirst(lc);
					JunkFilter *j;

					if (operation == CMD_UPDATE && PLANGEN_PLANNER == plannedstmt->planGen)
						ExecCheckPlanOutput(resultRelInfo->ri_RelationDesc,
											subplan->targetlist);

					j = ExecInitJunkFilter(subplan->targetlist,
								   resultRelInfo->ri_RelationDesc->rd_att->tdhasoid,
							       ExecInitExtraTupleSlot(estate));
					/*
					 * Since it must be UPDATE/DELETE, there had better be a
					 * "ctid" junk attribute in the tlist ... but ctid could
					 * be at a different resno for each result relation. We
					 * look up the ctid resnos now and save them in the
					 * junkfilters.
					 */
					j->jf_junkAttNo = ExecFindJunkAttribute(j, "ctid");
					if (!AttributeNumberIsValid(j->jf_junkAttNo))
						elog(ERROR, "could not find junk ctid column");
					resultRelInfo->ri_junkFilter = j;
					resultRelInfo++;
				}

				/*
				 * Set active junkfilter too; at this point ExecInitAppend has
				 * already selected an active result relation...
				 */
				estate->es_junkFilter =
					estate->es_result_relation_info->ri_junkFilter;

				/*
				 * We currently can't support rowmarks in this case, because
				 * the associated junk CTIDs might have different resnos in
				 * different subplans.
				 */
				if (estate->es_rowMarks)
					ereport(ERROR,
							(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
							 errmsg("SELECT FOR UPDATE/SHARE is not supported within a query with multiple result relations")));
			}
			else
			{

				/* Normal case with just one JunkFilter */
				JunkFilter *j;

				if (PLANGEN_PLANNER == plannedstmt->planGen && (operation == CMD_INSERT || operation == CMD_UPDATE))
					ExecCheckPlanOutput(estate->es_result_relation_info->ri_RelationDesc,
										planstate->plan->targetlist);

				j = ExecInitJunkFilter(planstate->plan->targetlist,
						       tupType->tdhasoid,
						       ExecInitExtraTupleSlot(estate));
				estate->es_junkFilter = j;
				if (estate->es_result_relation_info)
					estate->es_result_relation_info->ri_junkFilter = j;

				if (operation == CMD_SELECT)
				{
					/* For SELECT, want to return the cleaned tuple type */
					tupType = j->jf_cleanTupType;
				}
				else if (operation == CMD_UPDATE || operation == CMD_DELETE)
				{
					/* For UPDATE/DELETE, find the ctid junk attr now */
					j->jf_junkAttNo = ExecFindJunkAttribute(j, "ctid");
					if (!AttributeNumberIsValid(j->jf_junkAttNo))
						elog(ERROR, "could not find junk ctid column");
				}

				/* For SELECT FOR UPDATE/SHARE, find the ctid attrs now */
				foreach(l, estate->es_rowMarks)
				{
					ExecRowMark *erm = (ExecRowMark *) lfirst(l);
					char		resname[32];

					/* CDB: CTIDs were not fetched for distributed relation. */
					Relation relation = erm->relation;
					if (relation->rd_cdbpolicy &&
						relation->rd_cdbpolicy->ptype == POLICYTYPE_PARTITIONED)
						continue;

					snprintf(resname, sizeof(resname), "ctid%u", erm->rti);
					erm->ctidAttNo = ExecFindJunkAttribute(j, resname);
					if (!AttributeNumberIsValid(erm->ctidAttNo))
						elog(ERROR, "could not find junk \"%s\" column",
							 resname);
				}
			}
		}
		else
		{
			/* The planner requires that top node of the target list has the same
			 * number of columns than the output relation. This is not a requirement
			 * of the Optimizer. */
			if (operation == CMD_INSERT
					&& plannedstmt->planGen == PLANGEN_PLANNER)
			{
				ExecCheckPlanOutput(estate->es_result_relation_info->ri_RelationDesc,
									planstate->plan->targetlist);
			}

			estate->es_junkFilter = NULL;
			/*
			 * In Postgres the optimizer (planner) would add a junk
			 * TID column for each table in the rowMarks. And the
			 * executor will lock the tuples when executing the
			 * root node in the plan.
			 *
			 * The below check seems dormant even in Postgres: to
			 * actually cover the `elog(ERROR)`, the code path
			 * needs to have
			 *   1. No junk TID columns
			 *   2. A non-empty rowMarks list
			 *
			 * In Postgres the above conditions are all-or-nothing.
			 * So the check feels more like an `Assert` rather than
			 * an `elog(ERROR)`, and it seems to be dead code.
			 *
			 * In Greenplum, the above condition does not hold.
			 * Greenplum has a different (but compliant)
			 * implementation, where we place a lock on each table
			 * in the rowMarks.  Consequently, we do *not* generate
			 * the junk column in the plan (except for tables that
			 * are only on the master). See preprocess_targetlist
			 * in preptlist.c This is reasonable because the root
			 * node is most likely executing in a different slice
			 * from the leaf nodes (which are most likely SCAN's):
			 * by the time a (hypothetical) TID reaches the root
			 * node, there's no guarantee that the tuple has not
			 * passed through a motion node, hence it makes no
			 * sense locking it.
			 */
#if 0
			if (estate->es_rowMarks)
				elog(ERROR, "SELECT FOR UPDATE/SHARE, but no junk columns");
#endif
		}
	}

	/*
	 * Initialize RETURNING projections if needed.
	 */
	if (plannedstmt->returningLists)
	{
		TupleTableSlot *slot;
		ExprContext *econtext;
		ResultRelInfo *resultRelInfo;

		/*
		 * We set QueryDesc.tupDesc to be the RETURNING rowtype in this case.
		 * We assume all the sublists will generate the same output tupdesc.
		 */
		tupType = ExecTypeFromTL((List *) linitial(plannedstmt->returningLists),
								 false);

		/* Set up a slot for the output of the RETURNING projection(s) */
		slot = ExecInitExtraTupleSlot(estate);
		ExecSetSlotDescriptor(slot, tupType);
		/* Need an econtext too */
		econtext = CreateExprContext(estate);

		/*
		 * Build a projection for each result rel.	Note that any SubPlans in
		 * the RETURNING lists get attached to the topmost plan node.
		 */
		Assert(list_length(plannedstmt->returningLists) == estate->es_num_result_relations);
		resultRelInfo = estate->es_result_relations;
		foreach(l, plannedstmt->returningLists)
		{
			List	   *rlist = (List *) lfirst(l);
			List	   *rliststate;

			rliststate = (List *) ExecInitExpr((Expr *) rlist, planstate);
			resultRelInfo->ri_projectReturning =
				ExecBuildProjectionInfo(rliststate, econtext, slot,
									 resultRelInfo->ri_RelationDesc->rd_att);
			resultRelInfo++;
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
				  bool doInstrument)
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
		if (doInstrument)
			resultRelInfo->ri_TrigInstrument = InstrAlloc(n);
		else
			resultRelInfo->ri_TrigInstrument = NULL;
	}
	else
	{
		resultRelInfo->ri_TrigFunctions = NULL;
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
 * We assume that estate->es_result_relation_info is already set up to
 * describe the target relation.  Note that in an UPDATE that spans an
 * inheritance tree, some of the target relations may have OIDs and some not.
 * We have to make the decisions on a per-relation basis as we initialize
 * each of the child plans of the topmost Append plan.
 *
 * SELECT INTO is even uglier, because we don't have the INTO relation's
 * descriptor available when this code runs; we have to look aside at a
 * flag set by InitPlan().
 */
bool
ExecContextForcesOids(PlanState *planstate, bool *hasoids)
{
	if (planstate->state->es_select_into)
	{
		*hasoids = planstate->state->es_into_oids;
		return true;
	}
	else
	{
		ResultRelInfo *ri = planstate->state->es_result_relation_info;

		if (ri != NULL)
		{
			Relation	rel = ri->ri_RelationDesc;

			if (rel != NULL)
			{
				*hasoids = rel->rd_rel->relhasoids;
				return true;
			}
		}
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
	 * shut down any PlanQual processing we were doing
	 */
	if (estate->es_evalPlanQual != NULL)
		EndEvalPlanQual(estate);

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
	 * releasing buffer pins and tupdesc refcounts; there's no need to
	 * pfree the TupleTableSlots, since the containing memory context
	 * is about to go away anyway.
	 */
	ExecResetTupleTable(estate->es_tupleTable, false);

	/* Report how many tuples we may have inserted into AO tables */
	SendAOTupCounts(estate);

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
		ExecRowMark *erm = lfirst(l);

		heap_close(erm->relation, NoLock);
	}
}

/*
 * Verify that the tuples to be produced by INSERT or UPDATE match the
 * target relation's rowtype
 *
 * We do this to guard against stale plans.  If plan invalidation is
 * functioning properly then we should never get a failure here, but better
 * safe than sorry.  Note that this is called after we have obtained lock
 * on the target rel, so the rowtype can't change underneath us.
 *
 * The plan output is represented by its targetlist, because that makes
 * handling the dropped-column case easier.
 */
static void
ExecCheckPlanOutput(Relation resultRel, List *targetList)
{
	TupleDesc	resultDesc = RelationGetDescr(resultRel);
	int			attno = 0;
	ListCell   *lc;

	/*
	 * Don't do this during dispatch because the plan is not suitable
	 * structured to meet these tests
	 */
	if (Gp_role == GP_ROLE_DISPATCH)
		return;

	foreach(lc, targetList)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(lc);
		Form_pg_attribute attr;

		if (tle->resjunk)
			continue;			/* ignore junk tlist items */

		if (attno >= resultDesc->natts)
			ereport(ERROR,
					(errcode(ERRCODE_DATATYPE_MISMATCH),
					 errmsg("table row type and query-specified row type do not match"),
					 errdetail("Query has too many columns.")));
		attr = resultDesc->attrs[attno++];

		if (!attr->attisdropped)
		{
			/* Normal case: demand type match */
			if (exprType((Node *) tle->expr) != attr->atttypid)
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
						 errmsg("table row type and query-specified row type do not match"),
						 errdetail("Table has type %s at ordinal position %d, but query expects %s.",
								   format_type_be(attr->atttypid),
								   attno,
								   format_type_be(exprType((Node *) tle->expr)))));
		}
		else
		{
			/*
			 * For a dropped column, we can't check atttypid (it's likely 0).
			 * In any case the planner has most likely inserted an INT4 null.
			 * What we insist on is just *some* NULL constant.
			 */
			if (!IsA(tle->expr, Const) ||
				!((Const *) tle->expr)->constisnull)
				ereport(ERROR,
						(errcode(ERRCODE_DATATYPE_MISMATCH),
						 errmsg("table row type and query-specified row type do not match"),
						 errdetail("Query provides a value for a dropped column at ordinal position %d.",
								   attno)));
		}
	}
	if (attno != resultDesc->natts)
		ereport(ERROR,
				(errcode(ERRCODE_DATATYPE_MISMATCH),
				 errmsg("table row type and query-specified row type do not match"),
				 errdetail("Query has too few columns.")));
}


/* ----------------------------------------------------------------
 *		ExecutePlan
 *
 *		processes the query plan to retrieve 'numberTuples' tuples in the
 *		direction specified.
 *
 *		Retrieves all tuples if numberTuples is 0
 *
 *		result is either a slot containing the last tuple in the case
 *		of a SELECT or NULL otherwise.
 *
 * Note: the ctid attribute is a 'junk' attribute that is removed before the
 * user can see it
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecutePlan(EState *estate,
			PlanState *planstate,
			CmdType operation,
			long numberTuples,
			ScanDirection direction,
			DestReceiver *dest)
{
	JunkFilter *junkfilter;
	TupleTableSlot *planSlot;
	TupleTableSlot *slot;
	ItemPointer tupleid = NULL;
	ItemPointerData tuple_ctid;
	long		current_tuple_count;
	TupleTableSlot *result;

	/*
	 * initialize local variables
	 */
	current_tuple_count = 0;
	result = NULL;

	/*
	 * Set the direction.
	 */
	estate->es_direction = direction;

	/*
	 * Process BEFORE EACH STATEMENT triggers
	 */
if (Gp_role != GP_ROLE_EXECUTE || Gp_is_writer)
{
	switch (operation)
	{
		case CMD_UPDATE:
			ExecBSUpdateTriggers(estate, estate->es_result_relation_info);
			break;
		case CMD_DELETE:
			ExecBSDeleteTriggers(estate, estate->es_result_relation_info);
			break;
		case CMD_INSERT:
			ExecBSInsertTriggers(estate, estate->es_result_relation_info);
			break;
		default:
			/* do nothing */
			break;
	}
}

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
lnext:	;
		if (estate->es_useEvalPlan)
		{
			planSlot = EvalPlanQualNext(estate);
			if (TupIsNull(planSlot))
				planSlot = ExecProcNode(planstate);
		}
		else
			planSlot = ExecProcNode(planstate);

		/*
		 * if the tuple is null, then we assume there is nothing more to
		 * process so we just return null...
		 */
		if (TupIsNull(planSlot))
		{
			result = NULL;
			break;
		}

		/* ORCA plans of UPDATES/DELETES/INSERTS are handled elsewhere. */
		if (estate->es_plannedstmt->planGen != PLANGEN_PLANNER && operation != CMD_SELECT)
			goto executed;

		slot = planSlot;

		/*
		 * If we have a junk filter, then project a new tuple with the junk
		 * removed.
		 *
		 * Store this new "clean" tuple in the junkfilter's resultSlot.
		 * (Formerly, we stored it back over the "dirty" tuple, which is WRONG
		 * because that tuple slot has the wrong descriptor.)
		 *
		 * But first, extract all the junk information we need.
		 */
		if ((junkfilter = estate->es_junkFilter) != NULL)
		{
			/*
			 * Process any FOR UPDATE or FOR SHARE locking requested.
			 */
			if (estate->es_rowMarks != NIL)
			{
				ListCell   *l;

		lmark:	;
				foreach(l, estate->es_rowMarks)
				{
					ExecRowMark *erm = lfirst(l);
					Datum		datum;
					bool		isNull;
					HeapTupleData tuple;
					Buffer		buffer;
					ItemPointerData update_ctid;
					TransactionId update_xmax;
					TupleTableSlot *newSlot;
					LockTupleMode lockmode;
					HTSU_Result test;

					/* CDB: CTIDs were not fetched for distributed relation. */
					Relation relation = erm->relation;
					if (relation->rd_cdbpolicy &&
						relation->rd_cdbpolicy->ptype == POLICYTYPE_PARTITIONED)
						continue;

					datum = ExecGetJunkAttribute(slot,
												 erm->ctidAttNo,
												 &isNull);
					/* shouldn't ever get a null result... */
					if (isNull)
						elog(ERROR, "ctid is NULL");

					tuple.t_self = *((ItemPointer) DatumGetPointer(datum));

					if (erm->forUpdate)
						lockmode = LockTupleExclusive;
					else
						lockmode = LockTupleShared;

					test = heap_lock_tuple(erm->relation, &tuple, &buffer,
										   &update_ctid, &update_xmax,
										   estate->es_output_cid,
										   lockmode,
										   (erm->noWait ? LockTupleNoWait : LockTupleWait));
					ReleaseBuffer(buffer);
					switch (test)
					{
						case HeapTupleSelfUpdated:
							/* treat it as deleted; do not process */
							goto lnext;

						case HeapTupleMayBeUpdated:
							break;

						case HeapTupleUpdated:
							if (IsXactIsoLevelSerializable)
								ereport(ERROR,
								 (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE),
								  errmsg("could not serialize access due to concurrent update")));
							if (!ItemPointerEquals(&update_ctid,
												   &tuple.t_self))
							{
								/* updated, so look at updated version */
								newSlot = EvalPlanQual(estate,
													   erm->rti,
													   &update_ctid,
													   update_xmax);
								if (!TupIsNull(newSlot))
								{
									slot = planSlot = newSlot;
									estate->es_useEvalPlan = true;
									goto lmark;
								}
							}

							/*
							 * if tuple was deleted or PlanQual failed for
							 * updated tuple - we must not return this tuple!
							 */
							goto lnext;

						default:
							elog(ERROR, "unrecognized heap_lock_tuple status: %u",
								 test);
							return NULL;
					}
				}
			}

			/*
			 * extract the 'ctid' junk attribute.
			 */
			if (operation == CMD_UPDATE || operation == CMD_DELETE)
			{
				Datum		datum;
				bool		isNull;

				datum = ExecGetJunkAttribute(slot, junkfilter->jf_junkAttNo,
											 &isNull);
				/* shouldn't ever get a null result... */
				if (isNull)
					elog(ERROR, "ctid is NULL");

				tupleid = (ItemPointer) DatumGetPointer(datum);
				tuple_ctid = *tupleid;	/* make sure we don't free the ctid!! */
				tupleid = &tuple_ctid;
			}

			/*
			 * Create a new "clean" tuple with all junk attributes removed. We
			 * don't need to do this for DELETE, however (there will in fact
			 * be no non-junk attributes in a DELETE!)
			 */
			if (operation != CMD_DELETE)
				slot = ExecFilterJunk(junkfilter, slot);
		}

		if (operation != CMD_SELECT && Gp_role == GP_ROLE_EXECUTE && !Gp_is_writer)
		{
			elog(ERROR, "INSERT/UPDATE/DELETE must be executed by a writer segworker group");
		}

		/*
		 * Based on the operation, a tuple is either
		 * returned it to the user (SELECT) or inserted, deleted, or updated.
		 */
		switch (operation)
		{
			case CMD_SELECT:
				ExecSelect(slot, dest, estate);
				result = slot;
				break;

			case CMD_INSERT:
				ExecInsert(slot, dest, estate, PLANGEN_PLANNER, false /* isUpdate */);
				result = NULL;
				break;

			case CMD_DELETE:
				ExecDelete(tupleid, planSlot, dest, estate, PLANGEN_PLANNER, false /* isUpdate */);
				result = NULL;
				break;

			case CMD_UPDATE:
				ExecUpdate(slot, tupleid, planSlot, dest, estate);
				result = NULL;
				break;

			default:
				elog(ERROR, "unrecognized operation code: %d",
					 (int) operation);
				result = NULL;
				break;
		}

		/*
		 * check our tuple count.. if we've processed the proper number then
		 * quit, else loop again and process more tuples.  Zero numberTuples
		 * means no limit.
		 */
	executed:
		current_tuple_count++;
		if (numberTuples && numberTuples == current_tuple_count)
			break;
	}

	/*
	 * Process AFTER EACH STATEMENT triggers
	 */
if (Gp_role != GP_ROLE_EXECUTE || Gp_is_writer)
{
	switch (operation)
	{
		case CMD_UPDATE:
			ExecASUpdateTriggers(estate, estate->es_result_relation_info);
			break;
		case CMD_DELETE:
			ExecASDeleteTriggers(estate, estate->es_result_relation_info);
			break;
		case CMD_INSERT:
			ExecASInsertTriggers(estate, estate->es_result_relation_info);
			break;
		default:
			/* do nothing */
			break;
	}
}

	/*
	 * here, result is either a slot containing a tuple in the case of a
	 * SELECT or NULL otherwise.
	 */
	return result;
}

/* ----------------------------------------------------------------
 *		ExecSelect
 *
 *		SELECTs are easy.. we just pass the tuple to the appropriate
 *		output function.
 * ----------------------------------------------------------------
 */
static void
ExecSelect(TupleTableSlot *slot,
		   DestReceiver *dest,
		   EState *estate)
{
	(*dest->receiveSlot) (slot, dest);
	(estate->es_processed)++;
}

/* ----------------------------------------------------------------
 *		ExecInsert
 *
 *		INSERTs are trickier.. we have to insert the tuple into
 *		the base relation and insert appropriate tuples into the
 *		index relations.
 *		Insert can be part of an update operation when
 *		there is a preceding SplitUpdate node. 
 * ----------------------------------------------------------------
 */
void
ExecInsert(TupleTableSlot *slot,
		   DestReceiver *dest,
		   EState *estate,
		   PlanGenerator planGen,
		   bool isUpdate)
{
	void	   *tuple;
	ResultRelInfo *resultRelInfo;
	Relation	resultRelationDesc;
	Oid			newId;
	TupleTableSlot *partslot = NULL;

	AOTupleId	aoTupleId = AOTUPLEID_INIT;

	bool		rel_is_heap = false;
	bool 		rel_is_aorows = false;
	bool		rel_is_aocols = false;
	bool		rel_is_external = false;

	/*
	 * get information on the (current) result relation
	 */
	if (estate->es_result_partitions)
	{
		resultRelInfo = slot_get_partition(slot, estate);

		/* Check whether the user provided the correct leaf part only if required */
		if (!dml_ignore_target_partition_check)
		{
			Assert(NULL != estate->es_result_partitions->part &&
					NULL != resultRelInfo->ri_RelationDesc);

			List *resultRelations = estate->es_plannedstmt->resultRelations;
			/*
			 * Only inheritance can generate multiple result relations and inheritance
			 * is not compatible with partitions. As we are in inserting in partitioned
			 * table, we should not have more than one resultRelation
			 */
			Assert(list_length(resultRelations) == 1);
			/* We only have one resultRelations entry where the user originally intended to insert */
			int rteIdxForUserRel = linitial_int(resultRelations);
			Assert (rteIdxForUserRel > 0);
			Oid userProvidedRel = InvalidOid;

			if (1 == rteIdxForUserRel)
			{
				/* Optimization for typical case */
				userProvidedRel = ((RangeTblEntry *) estate->es_plannedstmt->rtable->head->data.ptr_value)->relid;
			}
			else
			{
				userProvidedRel = getrelid(rteIdxForUserRel, estate->es_plannedstmt->rtable);
			}

			/* Error out if user provides a leaf partition that does not match with our calculated partition */
			if (userProvidedRel != estate->es_result_partitions->part->parrelid &&
				userProvidedRel != resultRelInfo->ri_RelationDesc->rd_id)
			{
				ereport(ERROR,
						(errcode(ERRCODE_CHECK_VIOLATION),
						 errmsg("Trying to insert row into wrong partition"),
						 errdetail("Expected partition: %s, provided partition: %s",
							resultRelInfo->ri_RelationDesc->rd_rel->relname.data,
							estate->es_result_relation_info->ri_RelationDesc->rd_rel->relname.data)));
			}
		}
		estate->es_result_relation_info = resultRelInfo;
	}
	else
	{
		resultRelInfo = estate->es_result_relation_info;
	}

	Assert (!resultRelInfo->ri_projectReturning);

	resultRelationDesc = resultRelInfo->ri_RelationDesc;

	rel_is_heap = RelationIsHeap(resultRelationDesc);
	rel_is_aocols = RelationIsAoCols(resultRelationDesc);
	rel_is_aorows = RelationIsAoRows(resultRelationDesc);
	rel_is_external = RelationIsExternal(resultRelationDesc);

	partslot = reconstructMatchingTupleSlot(slot, resultRelInfo);
	if (rel_is_heap)
	{
		tuple = ExecFetchSlotHeapTuple(partslot);
	}
	else if (rel_is_aorows)
	{
		tuple = ExecFetchSlotMemTuple(partslot, false);
	}
	else if (rel_is_external) 
	{
		if (estate->es_result_partitions && 
			estate->es_result_partitions->part->parrelid != 0)
		{
			ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				errmsg("Insert into external partitions not supported.")));			
			return;
		}
		else
		{
			/*
			 * Make a modifiable copy, since external_insert() takes the
			 * liberty to modify the tuple.
			 */
			tuple = ExecCopySlotHeapTuple(partslot);
		}
	}
	else
	{
		Assert(rel_is_aocols);
		tuple = ExecFetchSlotMemTuple(partslot, true);
	}

	Assert(partslot != NULL && tuple != NULL);

	/* Execute triggers in Planner-generated plans */
	if (planGen == PLANGEN_PLANNER)
	{
		/* BEFORE ROW INSERT Triggers */
		if (resultRelInfo->ri_TrigDesc &&
			resultRelInfo->ri_TrigDesc->n_before_row[TRIGGER_EVENT_INSERT] > 0)
		{
			HeapTuple	newtuple;

			/* NYI */
			if(rel_is_aocols)
				elog(ERROR, "triggers are not supported on tables that use column-oriented storage");

			newtuple = ExecBRInsertTriggers(estate, resultRelInfo, tuple);

			if (newtuple == NULL)	/* "do nothing" */
			{
				return;
			}

			if (newtuple != tuple)	/* modified by Trigger(s) */
			{
				/*
				 * Put the modified tuple into a slot for convenience of routines
				 * below.  We assume the tuple was allocated in per-tuple memory
				 * context, and therefore will go away by itself. The tuple table
				 * slot should not try to clear it.
				 */
				TupleTableSlot *newslot = estate->es_trig_tuple_slot;

				if (newslot->tts_tupleDescriptor != partslot->tts_tupleDescriptor)
					ExecSetSlotDescriptor(newslot, partslot->tts_tupleDescriptor);
				ExecStoreGenericTuple(newtuple, newslot, false);
				newslot->tts_tableOid = partslot->tts_tableOid; /* for constraints */
				tuple = newtuple;
				partslot = newslot;
			}
		}
	}

	/*
	 * Check the constraints of the tuple
	 */
	if (resultRelationDesc->rd_att->constr)
		ExecConstraints(resultRelInfo, partslot, estate);

	/*
	 * insert the tuple
	 *
	 * Note: heap_insert returns the tid (location) of the new tuple in the
	 * t_self field.
	 *
	 * NOTE: for append-only relations we use the append-only access methods.
	 */
	if (rel_is_aorows)
	{
		if (resultRelInfo->ri_aoInsertDesc == NULL)
		{
			/* Set the pre-assigned fileseg number to insert into */
			ResultRelInfoSetSegno(resultRelInfo, estate->es_result_aosegnos);

			resultRelInfo->ri_aoInsertDesc =
				appendonly_insert_init(resultRelationDesc,
									   resultRelInfo->ri_aosegno,
									   false);

		}

		appendonly_insert(resultRelInfo->ri_aoInsertDesc, tuple, &newId, &aoTupleId);
	}
	else if (rel_is_aocols)
	{
		if (resultRelInfo->ri_aocsInsertDesc == NULL)
		{
			ResultRelInfoSetSegno(resultRelInfo, estate->es_result_aosegnos);
			resultRelInfo->ri_aocsInsertDesc = aocs_insert_init(resultRelationDesc, 
																resultRelInfo->ri_aosegno, false);
		}

		newId = aocs_insert(resultRelInfo->ri_aocsInsertDesc, partslot);
		aoTupleId = *((AOTupleId*)slot_get_ctid(partslot));
	}
	else if (rel_is_external)
	{
		/* Writable external table */
		if (resultRelInfo->ri_extInsertDesc == NULL)
			resultRelInfo->ri_extInsertDesc = external_insert_init(resultRelationDesc);

		newId = external_insert(resultRelInfo->ri_extInsertDesc, tuple);
	}
	else
	{
		Insist(rel_is_heap);

		newId = heap_insert(resultRelationDesc,
							tuple,
							estate->es_output_cid,
							true, true, GetCurrentTransactionId());
	}

	(estate->es_processed)++;
	(resultRelInfo->ri_aoprocessed)++;
	estate->es_lastoid = newId;

	partslot->tts_tableOid = RelationGetRelid(resultRelationDesc);

	if (rel_is_aorows || rel_is_aocols)
	{
		/*
		 * insert index entries for AO Row-Store tuple
		 */
		if (resultRelInfo->ri_NumIndices > 0)
			ExecInsertIndexTuples(partslot, (ItemPointer)&aoTupleId, estate, false);
	}
	else
	{
		/* Use parttuple for index update in case this is an indexed heap table. */
		TupleTableSlot *xslot = partslot;
		void *xtuple = tuple;

		setLastTid(&(((HeapTuple) xtuple)->t_self));

		/*
		 * insert index entries for tuple
		 */
		if (resultRelInfo->ri_NumIndices > 0)
			ExecInsertIndexTuples(xslot, &(((HeapTuple) xtuple)->t_self), estate, false);

		if (planGen == PLANGEN_PLANNER)
		{
			/* AFTER ROW INSERT Triggers */
			ExecARInsertTriggers(estate, resultRelInfo, tuple);
		}
	}
}

/* ----------------------------------------------------------------
 *		ExecDelete
 *
 *		DELETE is like UPDATE, except that we delete the tuple and no
 *		index modifications are needed.
 *		DELETE can be part of an update operation when
 *		there is a preceding SplitUpdate node. 
 *
 * ----------------------------------------------------------------
 */
void
ExecDelete(ItemPointer tupleid,
		   TupleTableSlot *planSlot,
		   DestReceiver *dest,
		   EState *estate,
		   PlanGenerator planGen,
		   bool isUpdate)
{
	ResultRelInfo *resultRelInfo;
	Relation	resultRelationDesc;
	HTSU_Result result;
	ItemPointerData update_ctid;
	TransactionId update_xmax = InvalidTransactionId;

	/*
	 * get information on the (current) result relation
	 */
	if (estate->es_result_partitions && planGen == PLANGEN_OPTIMIZER)
	{
		Assert(estate->es_result_partitions->part->parrelid);

#ifdef USE_ASSERT_CHECKING
		Oid parent = estate->es_result_partitions->part->parrelid;
#endif

		/* Obtain part for current tuple. */
		resultRelInfo = slot_get_partition(planSlot, estate);
		estate->es_result_relation_info = resultRelInfo;

#ifdef USE_ASSERT_CHECKING
		Oid part = RelationGetRelid(resultRelInfo->ri_RelationDesc);
#endif

		Assert(parent != part);
	}
	else
	{
		resultRelInfo = estate->es_result_relation_info;
	}
	resultRelationDesc = resultRelInfo->ri_RelationDesc;

	Assert (!resultRelInfo->ri_projectReturning);

	if (planGen == PLANGEN_PLANNER)
	{
		/* BEFORE ROW DELETE Triggers */
		if (resultRelInfo->ri_TrigDesc &&
			resultRelInfo->ri_TrigDesc->n_before_row[TRIGGER_EVENT_DELETE] > 0)
		{
			bool		dodelete;

			dodelete = ExecBRDeleteTriggers(estate, resultRelInfo, tupleid);

			if (!dodelete)			/* "do nothing" */
				return;
		}
	}

	bool isHeapTable = RelationIsHeap(resultRelationDesc);
	bool isAORowsTable = RelationIsAoRows(resultRelationDesc);
	bool isAOColsTable = RelationIsAoCols(resultRelationDesc);
	bool isExternalTable = RelationIsExternal(resultRelationDesc);

	if (isExternalTable && estate->es_result_partitions && 
		estate->es_result_partitions->part->parrelid != 0)
	{
		ereport(ERROR,
			(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
			errmsg("Delete from external partitions not supported.")));			
		return;
	}
	/*
	 * delete the tuple
	 *
	 * Note: if es_crosscheck_snapshot isn't InvalidSnapshot, we check that
	 * the row to be deleted is visible to that snapshot, and throw a can't-
	 * serialize error if not.	This is a special-case behavior needed for
	 * referential integrity updates in serializable transactions.
	 */
ldelete:;
	if (isHeapTable)
	{
		result = heap_delete(resultRelationDesc, tupleid,
						 &update_ctid, &update_xmax,
						 estate->es_output_cid,
						 estate->es_crosscheck_snapshot,
						 true /* wait for commit */ );
	}
	else if (isAORowsTable)
	{
		if (IsXactIsoLevelSerializable)
		{
			if (!isUpdate)
				ereport(ERROR,
					   (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("Deletes on append-only tables are not supported in serializable transactions.")));		
			else
				ereport(ERROR,
					   (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("Updates on append-only tables are not supported in serializable transactions.")));	
		}

		if (resultRelInfo->ri_deleteDesc == NULL)
		{
			resultRelInfo->ri_deleteDesc = 
				appendonly_delete_init(resultRelationDesc, ActiveSnapshot);
		}

		AOTupleId* aoTupleId = (AOTupleId*)tupleid;
		result = appendonly_delete(resultRelInfo->ri_deleteDesc, aoTupleId);
	} 
	else if (isAOColsTable)
	{
		if (IsXactIsoLevelSerializable)
		{
			if (!isUpdate)
				ereport(ERROR,
					   (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("Deletes on append-only tables are not supported in serializable transactions.")));		
			else
				ereport(ERROR,
					   (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("Updates on append-only tables are not supported in serializable transactions.")));		
		}

		if (resultRelInfo->ri_deleteDesc == NULL)
		{
			resultRelInfo->ri_deleteDesc = 
				aocs_delete_init(resultRelationDesc);
		}

		AOTupleId* aoTupleId = (AOTupleId*)tupleid;
		result = aocs_delete(resultRelInfo->ri_deleteDesc, aoTupleId);
	}
	else
	{
		Insist(0);
	}
	switch (result)
	{
		case HeapTupleSelfUpdated:
			/* already deleted by self; nothing to do */
		
			/*
			 * In an scenario in which R(a,b) and S(a,b) have 
			 *        R               S
			 *    ________         ________
			 *     (1, 1)           (1, 2)
			 *                      (1, 7)
 			 *
   			 *  An update query such as:
 			 *   UPDATE R SET a = S.b  FROM S WHERE R.b = S.a;
 			 *   
 			 *  will have an non-deterministic output. The tuple in R 
			 * can be updated to (2,1) or (7,1).
 			 * Since the introduction of SplitUpdate, these queries will 
			 * send multiple requests to delete the same tuple. Therefore, 
			 * in order to avoid a non-deterministic output, 
			 * an error is reported in such scenario.
 			 */
			if (isUpdate)
			{

				ereport(ERROR,
					(errcode(ERRCODE_IN_FAILED_SQL_TRANSACTION ),
					errmsg("multiple updates to a row by the same query is not allowed")));
			}

			return;

		case HeapTupleMayBeUpdated:
			break;

		case HeapTupleUpdated:
			if (IsXactIsoLevelSerializable)
				ereport(ERROR,
						(errcode(ERRCODE_T_R_SERIALIZATION_FAILURE),
						 errmsg("could not serialize access due to concurrent update")));
			else if (!ItemPointerEquals(tupleid, &update_ctid))
			{
				TupleTableSlot *epqslot;

				Assert(update_xmax != InvalidTransactionId);

				epqslot = EvalPlanQual(estate,
									   resultRelInfo->ri_RangeTableIndex,
									   &update_ctid,
									   update_xmax);
				if (!TupIsNull(epqslot))
				{
					*tupleid = update_ctid;
					goto ldelete;
				}
			}
			/* tuple already deleted; nothing to do */
			return;

		default:
			elog(ERROR, "unrecognized heap_delete status: %u", result);
			return;
	}

	if (!isUpdate)
	{
		(estate->es_processed)++;
		/*
		 * To notify master if tuples deleted or not, to update mod_count.
		 */
		(resultRelInfo->ri_aoprocessed)++;
	}

	/*
	 * Note: Normally one would think that we have to delete index tuples
	 * associated with the heap tuple now...
	 *
	 * ... but in POSTGRES, we have no need to do this because VACUUM will
	 * take care of it later.  We can't delete index tuples immediately
	 * anyway, since the tuple is still visible to other transactions.
	 */


	if (!(isAORowsTable || isAOColsTable) && planGen == PLANGEN_PLANNER)
	{
		/* AFTER ROW DELETE Triggers */
		ExecARDeleteTriggers(estate, resultRelInfo, tupleid);
	}
}


/*
 * Check if the tuple being updated will stay in the same part and throw ERROR
 * if not.  This check is especially necessary for default partition that has
 * no constraint on it.  partslot is the tuple being updated, and
 * resultRelInfo is the target relation of this update.  Call this only
 * estate has valid es_result_partitions.
 */
static void
checkPartitionUpdate(EState *estate, TupleTableSlot *partslot,
					 ResultRelInfo *resultRelInfo)
{
	Relation	resultRelationDesc = resultRelInfo->ri_RelationDesc;
	AttrNumber	max_attr;
	Datum	   *values = NULL;
	bool	   *nulls = NULL;
	TupleDesc	tupdesc = NULL;
	Oid			parentRelid;
	Oid			targetid;

	Assert(estate->es_partition_state != NULL &&
		   estate->es_partition_state->accessMethods != NULL);
	if (!estate->es_partition_state->accessMethods->part_cxt)
		estate->es_partition_state->accessMethods->part_cxt =
			GetPerTupleExprContext(estate)->ecxt_per_tuple_memory;

	Assert(PointerIsValid(estate->es_result_partitions));

	/*
	 * As opposed to INSERT, resultRelation here is the same child part
	 * as scan origin.  However, the partition selection is done with the
	 * parent partition's attribute numbers, so if this result (child) part
	 * has physically-different attribute numbers due to dropped columns,
	 * we should map the child attribute numbers to the parent's attribute
	 * numbers to perform the partition selection.
	 * EState doesn't have the parent relation information at the moment,
	 * so we have to do a hard job here by opening it and compare the
	 * tuple descriptors.  If we find we need to map attribute numbers,
	 * max_partition_attr could also be bogus for this child part,
	 * so we end up materializing the whole columns using slot_getallattrs().
	 * The purpose of this code is just to prevent the tuple from
	 * incorrectly staying in default partition that has no constraint
	 * (parts with constraint will throw an error if the tuple is changing
	 * partition keys to out of part value anyway.)  It's a bit overkill
	 * to do this complicated logic just for this purpose, which is necessary
	 * with our current partitioning design, but I hope some day we can
	 * change this so that we disallow phyisically-different tuple descriptor
	 * across partition.
	 */
	parentRelid = estate->es_result_partitions->part->parrelid;

	/*
	 * I don't believe this is the case currently, but we check the parent relid
	 * in case the updating partition has changed since the last time we opened it.
	 */
	if (resultRelInfo->ri_PartitionParent &&
		parentRelid != RelationGetRelid(resultRelInfo->ri_PartitionParent))
	{
		resultRelInfo->ri_PartCheckTupDescMatch = 0;
		if (resultRelInfo->ri_PartCheckMap != NULL)
			pfree(resultRelInfo->ri_PartCheckMap);
		if (resultRelInfo->ri_PartitionParent)
			relation_close(resultRelInfo->ri_PartitionParent, AccessShareLock);
	}

	/*
	 * Check this at the first pass only to avoid repeated catalog access.
	 */
	if (resultRelInfo->ri_PartCheckTupDescMatch == 0 &&
		parentRelid != RelationGetRelid(resultRelInfo->ri_RelationDesc))
	{
		Relation	parentRel;
		TupleDesc	resultTupdesc, parentTupdesc;

		/*
		 * We are on a child part, let's see the tuple descriptor looks like
		 * the parent's one.  Probably this won't cause deadlock because
		 * DML should have opened the parent table with appropriate lock.
		 */
		parentRel = relation_open(parentRelid, AccessShareLock);
		resultTupdesc = RelationGetDescr(resultRelationDesc);
		parentTupdesc = RelationGetDescr(parentRel);
		if (!equalTupleDescs(resultTupdesc, parentTupdesc, false))
		{
			AttrMap		   *map;
			MemoryContext	oldcontext;

			/* Tuple looks different.  Construct attribute mapping. */
			oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);
			map_part_attrs(resultRelationDesc, parentRel, &map, true);
			MemoryContextSwitchTo(oldcontext);

			/* And save it for later use. */
			resultRelInfo->ri_PartCheckMap = map;

			resultRelInfo->ri_PartCheckTupDescMatch = -1;
		}
		else
			resultRelInfo->ri_PartCheckTupDescMatch = 1;

		resultRelInfo->ri_PartitionParent = parentRel;
		/* parentRel will be closed as part of ResultRelInfo cleanup */
	}

	if (resultRelInfo->ri_PartCheckMap != NULL)
	{
		Datum	   *parent_values;
		bool	   *parent_nulls;
		Relation	parentRel = resultRelInfo->ri_PartitionParent;
		TupleDesc	parentTupdesc;
		AttrMap	   *map;

		Assert(parentRel != NULL);
		parentTupdesc = RelationGetDescr(parentRel);

		/*
		 * We need to map the attribute numbers to parent's one, to
		 * select the would-be destination relation, since all partition
		 * rules are based on the parent relation's tuple descriptor.
		 * max_partition_attr can be bogus as well, so don't use it.
		 */
		slot_getallattrs(partslot);
		values = slot_get_values(partslot);
		nulls = slot_get_isnull(partslot);
		parent_values = palloc(parentTupdesc->natts * sizeof(Datum));
		parent_nulls = palloc0(parentTupdesc->natts * sizeof(bool));

		map = resultRelInfo->ri_PartCheckMap;
		reconstructTupleValues(map, values, nulls, partslot->tts_tupleDescriptor->natts,
							   parent_values, parent_nulls, parentTupdesc->natts);

		/* Now we have values/nulls in parent's view. */
		values = parent_values;
		nulls = parent_nulls;
		tupdesc = RelationGetDescr(parentRel);
	}
	else
	{
		/*
		 * map == NULL means we can just fetch values/nulls from the
		 * current slot.
		 */
		Assert(nulls == NULL && tupdesc == NULL);
		max_attr = estate->es_partition_state->max_partition_attr;
		slot_getsomeattrs(partslot, max_attr);
		/* values/nulls pointing to partslot's array. */
		values = slot_get_values(partslot);
		nulls = slot_get_isnull(partslot);
		tupdesc = partslot->tts_tupleDescriptor;
	}

	/* And select the destination relation that this tuple would go to. */
	targetid = selectPartition(estate->es_result_partitions, values,
							   nulls, tupdesc,
							   estate->es_partition_state->accessMethods);

	/* Free up if we allocated mapped attributes. */
	if (values != slot_get_values(partslot))
	{
		Assert(nulls != slot_get_isnull(partslot));
		pfree(values);
		pfree(nulls);
	}

	if (!OidIsValid(targetid))
		ereport(ERROR,
				(errcode(ERRCODE_NO_PARTITION_FOR_PARTITIONING_KEY),
				 errmsg("no partition for partitioning key")));

	if (RelationGetRelid(resultRelationDesc) != targetid)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("moving tuple from partition \"%s\" to "
						"partition \"%s\" not supported",
						get_rel_name(RelationGetRelid(resultRelationDesc)),
						get_rel_name(targetid))));
}

/* ----------------------------------------------------------------
 *		ExecUpdate
 *
 *		note: we can't run UPDATE queries with transactions
 *		off because UPDATEs are actually INSERTs and our
 *		scan will mistakenly loop forever, updating the tuple
 *		it just inserted..	This should be fixed but until it
 *		is, we don't want to get stuck in an infinite loop
 *		which corrupts your database..
 * ----------------------------------------------------------------
 */
void
ExecUpdate(TupleTableSlot *slot,
		   ItemPointer tupleid,
		   TupleTableSlot *planSlot,
		   DestReceiver *dest,
		   EState *estate)
{
	void*	tuple;
	ResultRelInfo *resultRelInfo;
	Relation	resultRelationDesc;
	HTSU_Result result;
	ItemPointerData update_ctid;
	TransactionId update_xmax = InvalidTransactionId;
	AOTupleId	aoTupleId = AOTUPLEID_INIT;
	TupleTableSlot *partslot = NULL;

	/*
	 * abort the operation if not running transactions
	 */
	if (IsBootstrapProcessingMode())
		elog(ERROR, "cannot UPDATE during bootstrap");

	/*
	 * get information on the (current) result relation
	 */
	resultRelInfo = estate->es_result_relation_info;
	resultRelationDesc = resultRelInfo->ri_RelationDesc;

	bool		rel_is_heap = RelationIsHeap(resultRelationDesc);
	bool 		rel_is_aorows = RelationIsAoRows(resultRelationDesc);
	bool		rel_is_aocols = RelationIsAoCols(resultRelationDesc);
	bool		rel_is_external = RelationIsExternal(resultRelationDesc);

	/*
	 * get the heap tuple out of the tuple table slot, making sure we have a
	 * writable copy
	 */
	if (rel_is_heap)
	{
		partslot = slot;
		tuple = ExecFetchSlotHeapTuple(partslot);
	}
	else if (rel_is_aorows || rel_is_aocols)
	{
		/*
		 * It is necessary to reconstruct a logically compatible tuple to
		 * a physically compatible tuple.  The slot's tuple descriptor comes
		 * from the projection target list, which doesn't indicate dropped
		 * columns, and MemTuple cannot deal with cases without converting
		 * the target list back into the original relation's tuple desc.
		 */
		partslot = reconstructMatchingTupleSlot(slot, resultRelInfo);

		/*
		 * We directly inline toasted columns here as update with toasted columns
		 * would create two references to the same toasted value.
		 */
		tuple = ExecFetchSlotMemTuple(partslot, true);
	}
	else if (rel_is_external) 
	{
		if (estate->es_result_partitions && 
			estate->es_result_partitions->part->parrelid != 0)
		{
			ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				errmsg("Update external partitions not supported.")));			
			return;
		}
		else
		{
			partslot = slot;
			tuple = ExecFetchSlotHeapTuple(partslot);
		}
	}
	else 
	{
		Insist(false);
	}

	/* see if this update would move the tuple to a different partition */
	if (estate->es_result_partitions)
		checkPartitionUpdate(estate, partslot, resultRelInfo);

	/* BEFORE ROW UPDATE Triggers */
	if (resultRelInfo->ri_TrigDesc &&
		resultRelInfo->ri_TrigDesc->n_before_row[TRIGGER_EVENT_UPDATE] > 0)
	{
		slot = ExecBRUpdateTriggers(estate, resultRelInfo,
									tupleid, slot);

		if (slot == NULL)		/* "do nothing" */
			return;

		/* trigger might have changed tuple */
		tuple = ExecFetchSlotHeapTuple(slot);
	}

	/*
	 * Check the constraints of the tuple
	 *
	 * If we generate a new candidate tuple after EvalPlanQual testing, we
	 * must loop back here and recheck constraints.  (We don't need to redo
	 * triggers, however.  If there are any BEFORE triggers then trigger.c
	 * will have done heap_lock_tuple to lock the correct tuple, so there's no
	 * need to do them again.)
	 */
lreplace:;
	if (resultRelationDesc->rd_att->constr)
		ExecConstraints(resultRelInfo, partslot, estate);

	if (!GpPersistent_IsPersistentRelation(resultRelationDesc->rd_id))
	{
		/*
		 * Normal UPDATE path.
		 */

		/*
		 * replace the heap tuple
		 *
		 * Note: if es_crosscheck_snapshot isn't InvalidSnapshot, we check that
		 * the row to be updated is visible to that snapshot, and throw a can't-
		 * serialize error if not.	This is a special-case behavior needed for
		 * referential integrity updates in serializable transactions.
		 */
		if (rel_is_heap)
		{
			result = heap_update(resultRelationDesc, tupleid, tuple,
							 &update_ctid, &update_xmax,
							 estate->es_output_cid,
							 estate->es_crosscheck_snapshot,
							 true /* wait for commit */ );
		} 
		else if (rel_is_aorows)
		{
			if (IsXactIsoLevelSerializable)
			{
				ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					errmsg("Updates on append-only tables are not supported in serializable transactions.")));			
			}

			if (resultRelInfo->ri_updateDesc == NULL)
			{
				ResultRelInfoSetSegno(resultRelInfo, estate->es_result_aosegnos);
				resultRelInfo->ri_updateDesc = (AppendOnlyUpdateDesc)
					appendonly_update_init(resultRelationDesc, ActiveSnapshot, resultRelInfo->ri_aosegno);
			}
			result = appendonly_update(resultRelInfo->ri_updateDesc,
								 tuple, (AOTupleId *) tupleid, &aoTupleId);
		}
		else if (rel_is_aocols)
		{
			if (IsXactIsoLevelSerializable)
			{
				ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					errmsg("Updates on append-only tables are not supported in serializable transactions.")));			
			}

			if (resultRelInfo->ri_updateDesc == NULL)
			{
				ResultRelInfoSetSegno(resultRelInfo, estate->es_result_aosegnos);
				resultRelInfo->ri_updateDesc = (AppendOnlyUpdateDesc)
					aocs_update_init(resultRelationDesc, resultRelInfo->ri_aosegno);
			}
			result = aocs_update(resultRelInfo->ri_updateDesc,
								 partslot, (AOTupleId *) tupleid, &aoTupleId);
		}
		else
		{
			Assert(!"We should not be here");
		}
		switch (result)
		{
			case HeapTupleSelfUpdated:
				/* already deleted by self; nothing to do */
				return;

			case HeapTupleMayBeUpdated:
				break;

			case HeapTupleUpdated:
				if (IsXactIsoLevelSerializable)
					ereport(ERROR,
							(errcode(ERRCODE_T_R_SERIALIZATION_FAILURE),
							 errmsg("could not serialize access due to concurrent update")));
				else if (!ItemPointerEquals(tupleid, &update_ctid))
				{
					TupleTableSlot *epqslot;

					Assert(update_xmax != InvalidTransactionId);

					epqslot = EvalPlanQual(estate,
										   resultRelInfo->ri_RangeTableIndex,
										   &update_ctid,
										   update_xmax);
					if (!TupIsNull(epqslot))
					{
						*tupleid = update_ctid;
						partslot = ExecFilterJunk(estate->es_junkFilter, epqslot);
						tuple = ExecFetchSlotHeapTuple(partslot);
						goto lreplace;
					}
				}
				/* tuple already deleted; nothing to do */
				return;

			default:
				elog(ERROR, "unrecognized heap_update status: %u", result);
				return;
		}
	}
	else
	{
		HeapTuple persistentTuple;

		/*
		 * Persistent metadata path.
		 */
		persistentTuple = heap_copytuple(tuple);
		persistentTuple->t_self = *tupleid;

		frozen_heap_inplace_update(resultRelationDesc, persistentTuple);

		heap_freetuple(persistentTuple);
	}

	(estate->es_processed)++;
	(resultRelInfo->ri_aoprocessed)++;

	/*
	 * Note: instead of having to update the old index tuples associated with
	 * the heap tuple, all we do is form and insert new index tuples. This is
	 * because UPDATEs are actually DELETEs and INSERTs, and index tuple
	 * deletion is done later by VACUUM (see notes in ExecDelete).	All we do
	 * here is insert new index tuples.  -cim 9/27/89
	 */

	/*
	 * insert index entries for tuple
	 *
	 * Note: heap_update returns the tid (location) of the new tuple in the
	 * t_self field.
	 *
	 * If it's a HOT update, we mustn't insert new index entries.
	 */
	if (rel_is_aorows || rel_is_aocols)
	{
		if (resultRelInfo->ri_NumIndices > 0)
			ExecInsertIndexTuples(partslot, (ItemPointer)&aoTupleId, estate, false);
	}
	else
	{
		if (resultRelInfo->ri_NumIndices > 0 && !HeapTupleIsHeapOnly((HeapTuple) tuple))
			ExecInsertIndexTuples(partslot, &(((HeapTuple) tuple)->t_self), estate, false);

		/* AFTER ROW UPDATE Triggers */
		ExecARUpdateTriggers(estate, resultRelInfo, tupleid, tuple);
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
 * Check a modified tuple to see if we want to process its updated version
 * under READ COMMITTED rules.
 *
 * See backend/executor/README for some info about how this works.
 *
 *	estate - executor state data
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
EvalPlanQual(EState *estate, Index rti,
			 ItemPointer tid, TransactionId priorXmax)
{
	evalPlanQual *epq;
	EState	   *epqstate;
	Relation	relation;
	HeapTupleData tuple;
	HeapTuple	copyTuple = NULL;
	SnapshotData SnapshotDirty;
	bool		endNode;

	Assert(rti != 0);

	/*
	 * find relation containing target tuple
	 */
	if (estate->es_result_relation_info != NULL &&
		estate->es_result_relation_info->ri_RangeTableIndex == rti)
		relation = estate->es_result_relation_info->ri_RelationDesc;
	else
	{
		ListCell   *l;

		relation = NULL;
		foreach(l, estate->es_rowMarks)
		{
			if (((ExecRowMark *) lfirst(l))->rti == rti)
			{
				relation = ((ExecRowMark *) lfirst(l))->relation;
				break;
			}
		}
		if (relation == NULL)
			elog(ERROR, "could not find RowMark for RT index %u", rti);
	}

	/*
	 * fetch tid tuple
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
		 * test.
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
	 * For UPDATE/DELETE we have to return tid of actual row we're executing
	 * PQ for.
	 */
	*tid = tuple.t_self;

	/*
	 * Need to run a recheck subquery.	Find or create a PQ stack entry.
	 */
	epq = estate->es_evalPlanQual;
	endNode = true;

	if (epq != NULL && epq->rti == 0)
	{
		/* Top PQ stack entry is idle, so re-use it */
		Assert(!(estate->es_useEvalPlan) && epq->next == NULL);
		epq->rti = rti;
		endNode = false;
	}

	/*
	 * If this is request for another RTE - Ra, - then we have to check wasn't
	 * PlanQual requested for Ra already and if so then Ra' row was updated
	 * again and we have to re-start old execution for Ra and forget all what
	 * we done after Ra was suspended. Cool? -:))
	 */
	if (epq != NULL && epq->rti != rti &&
		epq->estate->es_evTuple[rti - 1] != NULL)
	{
		do
		{
			evalPlanQual *oldepq;

			/* stop execution */
			EvalPlanQualStop(epq);
			/* pop previous PlanQual from the stack */
			oldepq = epq->next;
			Assert(oldepq && oldepq->rti != 0);
			/* push current PQ to freePQ stack */
			oldepq->free = epq;
			epq = oldepq;
			estate->es_evalPlanQual = epq;
		} while (epq->rti != rti);
	}

	/*
	 * If we are requested for another RTE then we have to suspend execution
	 * of current PlanQual and start execution for new one.
	 */
	if (epq == NULL || epq->rti != rti)
	{
		/* try to reuse plan used previously */
		evalPlanQual *newepq = (epq != NULL) ? epq->free : NULL;

		if (newepq == NULL)		/* first call or freePQ stack is empty */
		{
			newepq = (evalPlanQual *) palloc0(sizeof(evalPlanQual));
			newepq->free = NULL;
			newepq->estate = NULL;
			newepq->planstate = NULL;
		}
		else
		{
			/* recycle previously used PlanQual */
			Assert(newepq->estate == NULL);
			epq->free = NULL;
		}
		/* push current PQ to the stack */
		newepq->next = epq;
		epq = newepq;
		estate->es_evalPlanQual = epq;
		epq->rti = rti;
		endNode = false;
	}

	Assert(epq->rti == rti);

	/*
	 * Ok - we're requested for the same RTE.  Unfortunately we still have to
	 * end and restart execution of the plan, because ExecReScan wouldn't
	 * ensure that upper plan nodes would reset themselves.  We could make
	 * that work if insertion of the target tuple were integrated with the
	 * Param mechanism somehow, so that the upper plan nodes know that their
	 * children's outputs have changed.
	 *
	 * Note that the stack of free evalPlanQual nodes is quite useless at the
	 * moment, since it only saves us from pallocing/releasing the
	 * evalPlanQual nodes themselves.  But it will be useful once we implement
	 * ReScan instead of end/restart for re-using PlanQual nodes.
	 */
	if (endNode)
	{
		/* stop execution */
		EvalPlanQualStop(epq);
	}

	/*
	 * Initialize new recheck query.
	 *
	 * Note: if we were re-using PlanQual plans via ExecReScan, we'd need to
	 * instead copy down changeable state from the top plan (including
	 * es_result_relation_info, es_junkFilter) and reset locally changeable
	 * state in the epq (including es_param_exec_vals, es_evTupleNull).
	 */
	EvalPlanQualStart(epq, estate, epq->next);

	/*
	 * free old RTE' tuple, if any, and store target tuple where relation's
	 * scan node will see it
	 */
	epqstate = epq->estate;
	if (epqstate->es_evTuple[rti - 1] != NULL)
		heap_freetuple(epqstate->es_evTuple[rti - 1]);
	epqstate->es_evTuple[rti - 1] = copyTuple;

	return EvalPlanQualNext(estate);
}

static TupleTableSlot *
EvalPlanQualNext(EState *estate)
{
	evalPlanQual *epq = estate->es_evalPlanQual;
	MemoryContext oldcontext;
	TupleTableSlot *slot;

	Assert(epq->rti != 0);

lpqnext:;
	oldcontext = MemoryContextSwitchTo(epq->estate->es_query_cxt);
	slot = ExecProcNode(epq->planstate);
	MemoryContextSwitchTo(oldcontext);

	/*
	 * No more tuples for this PQ. Continue previous one.
	 */
	if (TupIsNull(slot))
	{
		evalPlanQual *oldepq;

		/* stop execution */
		EvalPlanQualStop(epq);
		/* pop old PQ from the stack */
		oldepq = epq->next;
		if (oldepq == NULL)
		{
			/* this is the first (oldest) PQ - mark as free */
			epq->rti = 0;
			estate->es_useEvalPlan = false;
			/* and continue Query execution */
			return NULL;
		}
		Assert(oldepq->rti != 0);
		/* push current PQ to freePQ stack */
		oldepq->free = epq;
		epq = oldepq;
		estate->es_evalPlanQual = epq;
		goto lpqnext;
	}

	return slot;
}

static void
EndEvalPlanQual(EState *estate)
{
	evalPlanQual *epq = estate->es_evalPlanQual;

	if (epq->rti == 0)			/* plans already shutdowned */
	{
		Assert(epq->next == NULL);
		return;
	}

	for (;;)
	{
		evalPlanQual *oldepq;

		/* stop execution */
		EvalPlanQualStop(epq);
		/* pop old PQ from the stack */
		oldepq = epq->next;
		if (oldepq == NULL)
		{
			/* this is the first (oldest) PQ - mark as free */
			epq->rti = 0;
			estate->es_useEvalPlan = false;
			break;
		}
		Assert(oldepq->rti != 0);
		/* push current PQ to freePQ stack */
		oldepq->free = epq;
		epq = oldepq;
		estate->es_evalPlanQual = epq;
	}
}

/*
 * Start execution of one level of PlanQual.
 *
 * This is a cut-down version of ExecutorStart(): we copy some state from
 * the top-level estate rather than initializing it fresh.
 */
static void
EvalPlanQualStart(evalPlanQual *epq, EState *estate, evalPlanQual *priorepq)
{
	EState	   *epqstate;
	int			rtsize;
	MemoryContext oldcontext;
	ListCell   *l;

	rtsize = list_length(estate->es_range_table);

	epq->estate = epqstate = CreateExecutorState();

	oldcontext = MemoryContextSwitchTo(epqstate->es_query_cxt);

	/*
	 * The epqstates share the top query's copy of unchanging state such as
	 * the snapshot, rangetable, result-rel info, and external Param info.
	 * They need their own copies of local state, including a tuple table,
	 * es_param_exec_vals, etc.
	 */
	epqstate->es_direction = ForwardScanDirection;
	epqstate->es_snapshot = estate->es_snapshot;
	epqstate->es_crosscheck_snapshot = estate->es_crosscheck_snapshot;
	epqstate->es_range_table = estate->es_range_table;
	epqstate->es_output_cid = estate->es_output_cid;
	epqstate->es_result_relations = estate->es_result_relations;
	epqstate->es_num_result_relations = estate->es_num_result_relations;
	epqstate->es_result_relation_info = estate->es_result_relation_info;
	epqstate->es_junkFilter = estate->es_junkFilter;
	/* es_trig_target_relations must NOT be copied */
	epqstate->es_into_relation_descriptor = estate->es_into_relation_descriptor;
	epqstate->es_into_relation_is_bulkload = estate->es_into_relation_is_bulkload;
	epqstate->es_into_relation_last_heap_tid = estate->es_into_relation_last_heap_tid;

	epqstate->es_into_relation_bulkloadinfo = (struct MirroredBufferPoolBulkLoadInfo *) palloc0(sizeof(MirroredBufferPoolBulkLoadInfo));
	memcpy(epqstate->es_into_relation_bulkloadinfo, estate->es_into_relation_bulkloadinfo, sizeof(MirroredBufferPoolBulkLoadInfo));

	epqstate->es_param_list_info = estate->es_param_list_info;
	if (estate->es_plannedstmt->nParamExec > 0)
		epqstate->es_param_exec_vals = (ParamExecData *)
			palloc0(estate->es_plannedstmt->nParamExec * sizeof(ParamExecData));
	epqstate->es_rowMarks = estate->es_rowMarks;
	epqstate->es_instrument = estate->es_instrument;
	epqstate->es_select_into = estate->es_select_into;
	epqstate->es_into_oids = estate->es_into_oids;
	epqstate->es_plannedstmt = estate->es_plannedstmt;

	/*
	 * Each epqstate must have its own es_evTupleNull state, but all the stack
	 * entries share es_evTuple state.	This allows sub-rechecks to inherit
	 * the value being examined by an outer recheck.
	 */
	epqstate->es_evTupleNull = (bool *) palloc0(rtsize * sizeof(bool));
	if (priorepq == NULL)
		/* first PQ stack entry */
		epqstate->es_evTuple = (HeapTuple *)
			palloc0(rtsize * sizeof(HeapTuple));
	else
		/* later stack entries share the same storage */
		epqstate->es_evTuple = priorepq->estate->es_evTuple;

	/*
	 * Each epqstate also has its own tuple table.
	 */
	epqstate->es_tupleTable = NIL;

	/*
	 * Initialize private state information for each SubPlan.  We must do this
	 * before running ExecInitNode on the main query tree, since
	 * ExecInitSubPlan expects to be able to find these entries.
	 */
	Assert(epqstate->es_subplanstates == NIL);
	foreach(l, estate->es_plannedstmt->subplans)
	{
		Plan	   *subplan = (Plan *) lfirst(l);
		PlanState  *subplanstate;

		subplanstate = ExecInitNode(subplan, epqstate, 0);

		epqstate->es_subplanstates = lappend(epqstate->es_subplanstates,
											 subplanstate);
	}

	/*
	 * Initialize the private state information for all the nodes in the query
	 * tree.  This opens files, allocates storage and leaves us ready to start
	 * processing tuples.
	 */
	epq->planstate = ExecInitNode(estate->es_plannedstmt->planTree, epqstate, 0);

	MemoryContextSwitchTo(oldcontext);
}

/*
 * End execution of one level of PlanQual.
 *
 * This is a cut-down version of ExecutorEnd(); basically we want to do most
 * of the normal cleanup, but *not* close result relations (which we are
 * just sharing from the outer query).	We do, however, have to close any
 * trigger target relations that got opened, since those are not shared.
 */
static void
EvalPlanQualStop(evalPlanQual *epq)
{
	EState	   *epqstate = epq->estate;
	MemoryContext oldcontext;
	ListCell   *l;

	oldcontext = MemoryContextSwitchTo(epqstate->es_query_cxt);

	ExecEndNode(epq->planstate);

	foreach(l, epqstate->es_subplanstates)
	{
		PlanState  *subplanstate = (PlanState *) lfirst(l);

		ExecEndNode(subplanstate);
	}

	/* throw away the per-epqstate tuple table completely */
	ExecResetTupleTable(epqstate->es_tupleTable, true);
	epqstate->es_tupleTable = NIL;

	if (epqstate->es_evTuple[epq->rti - 1] != NULL)
	{
		heap_freetuple(epqstate->es_evTuple[epq->rti - 1]);
		epqstate->es_evTuple[epq->rti - 1] = NULL;
	}

	foreach(l, epqstate->es_trig_target_relations)
	{
		ResultRelInfo *resultRelInfo = (ResultRelInfo *) lfirst(l);

		/* Close indices and then the relation itself */
		ExecCloseIndices(resultRelInfo);
		heap_close(resultRelInfo->ri_RelationDesc, NoLock);
	}

	MemoryContextSwitchTo(oldcontext);

	/*
	 * Reset the sharing fields of this EState with main query's EState
	 * to NULL before calling FreeExecutorState, to avoid duplicate cleanup.
	 * This should be symmetric to the setup in EvalPlanQualStart().
	 */
	epqstate->es_snapshot = NULL;
	epqstate->es_crosscheck_snapshot = NULL;
	epqstate->es_range_table = NIL;
	epqstate->es_result_relations = NULL;
	epqstate->es_result_relation_info = NULL;
	epqstate->es_junkFilter = NULL;
	epqstate->es_into_relation_descriptor = NULL;
	epqstate->es_param_list_info = NULL;
	epqstate->es_rowMarks = NIL;
	epqstate->es_plannedstmt = NULL;

	FreeExecutorState(epqstate);

	epq->estate = NULL;
	epq->planstate = NULL;
}

/*
 * ExecGetActivePlanTree --- get the active PlanState tree from a QueryDesc
 *
 * Ordinarily this is just the one mentioned in the QueryDesc, but if we
 * are looking at a row returned by the EvalPlanQual machinery, we need
 * to look at the subsidiary state instead.
 */
PlanState *
ExecGetActivePlanTree(QueryDesc *queryDesc)
{
	EState	   *estate = queryDesc->estate;

	if (estate && estate->es_useEvalPlan && estate->es_evalPlanQual != NULL)
		return estate->es_evalPlanQual->planstate;
	else
		return queryDesc->planstate;
}


/*
 * Support for SELECT INTO (a/k/a CREATE TABLE AS)
 *
 * We implement SELECT INTO by diverting SELECT's normal output with
 * a specialized DestReceiver type.
 *
 * TODO: remove some of the INTO-specific cruft from EState, and keep
 * it in the DestReceiver instead.
 */

typedef struct
{
	DestReceiver pub;			/* publicly-known function pointers */
	EState	   *estate;			/* EState we are working with */
	AppendOnlyInsertDescData *ao_insertDesc; /* descriptor to AO tables */
        AOCSInsertDescData *aocs_ins;           /* descriptor for aocs */
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
	char	   *intoTableSpaceName;
    GpPolicy   *targetPolicy;
	bool		bufferPoolBulkLoad;
	bool		validate_reloptions;

	RelFileNode relFileNode;
	
	ItemPointerData persistentTid;
	int64			persistentSerialNum;
	
	targetPolicy = queryDesc->plannedstmt->intoPolicy;

	Assert(into);

	/*
	 * XXX This code needs to be kept in sync with DefineRelation().
	 * Maybe we should try to use that function instead.
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

	/* have to copy the actual tupdesc to get rid of any constraints */
	tupdesc = CreateTupleDescCopy(queryDesc->tupDesc);

	/*
	 * We can skip WAL-logging the insertions for FileRep on segments, but not on
	 * master since we are using the old WAL based physical replication.
	 *
	 * GPDP does not support PITR.
	 */
	bufferPoolBulkLoad = 
		(relstorage_is_buffer_pool(relstorage) ?
									XLog_CanBypassWal() : false);

	/* Now we can actually create the new relation */
	intoRelationId = heap_create_with_catalog(intoName,
											  namespaceId,
											  tablespaceId,
											  InvalidOid,
											  GetUserId(),
											  tupdesc,
											  /* relam */ InvalidOid,
											  relkind,
											  relstorage,
											  false,
											  true,
											  bufferPoolBulkLoad,
											  0,
											  into->onCommit,
											  targetPolicy,  	/* MPP */
											  reloptions,
											  allowSystemTableModsDDL,
											  /* valid_opts */ !validate_reloptions,
						 					  &persistentTid,
						 					  &persistentSerialNum);

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
	AlterTableCreateToastTable(intoRelationId, false);
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

	/* use_wal off requires rd_targblock be initially invalid */
	Assert(intoRelationDesc->rd_targblock == InvalidBlockNumber);

	estate->es_into_relation_is_bulkload = bufferPoolBulkLoad;
	estate->es_into_relation_descriptor = intoRelationDesc;

	relFileNode.spcNode = tablespaceId;
	relFileNode.dbNode = MyDatabaseId;
	relFileNode.relNode = intoRelationId;
	if (estate->es_into_relation_is_bulkload)
	{
		MirroredBufferPool_BeginBulkLoad(
								&relFileNode,
								&persistentTid,
								persistentSerialNum,
								estate->es_into_relation_bulkloadinfo);
	}
	else
	{
		/*
		 * Save this information for tracing in CloseIntoRel.
		 */
		estate->es_into_relation_bulkloadinfo->relFileNode = relFileNode;
		estate->es_into_relation_bulkloadinfo->persistentTid = persistentTid;
		estate->es_into_relation_bulkloadinfo->persistentSerialNum = persistentSerialNum;

		if (Debug_persistent_print)
		{
			elog(Persistent_DebugPrintLevel(),
				 "OpenIntoRel %u/%u/%u: not bypassing the WAL -- not using bulk load, persistent serial num " INT64_FORMAT ", TID %s",
				 relFileNode.spcNode,
				 relFileNode.dbNode,
				 relFileNode.relNode,
				 persistentSerialNum,
				 ItemPointerToString(&persistentTid));
		}
	}
	
	/*
	 * Now replace the query's DestReceiver with one for SELECT INTO
	 */
	queryDesc->dest = CreateDestReceiver(DestIntoRel, NULL);
	myState = (DR_intorel *) queryDesc->dest;
	Assert(myState->pub.mydest == DestIntoRel);
	myState->estate = estate;
}

/*
 * CloseIntoRel --- clean up SELECT INTO at ExecutorEnd time
 */
static void
CloseIntoRel(QueryDesc *queryDesc)
{
	EState	   *estate = queryDesc->estate;
	Relation	rel = estate->es_into_relation_descriptor;

	/* Partition with SELECT INTO is not supported */
	Assert(!PointerIsValid(estate->es_result_partitions));

	/* OpenIntoRel might never have gotten called */
	if (rel)
	{
		/* APPEND_ONLY is closed in the intorel_shutdown */
		if (!(RelationIsAoRows(rel) || RelationIsAoCols(rel)))
		{
			int32 numOfBlocks;

			/* If we skipped using WAL, must heap_sync before commit */
			if (estate->es_into_relation_is_bulkload)
			{
				FlushRelationBuffers(rel);
				/* FlushRelationBuffers will have opened rd_smgr */
				smgrimmedsync(rel->rd_smgr);
			}

			if (PersistentStore_IsZeroTid(&estate->es_into_relation_last_heap_tid))
				numOfBlocks = 0;
			else
				numOfBlocks = ItemPointerGetBlockNumber(&estate->es_into_relation_last_heap_tid) + 1;

			if (estate->es_into_relation_is_bulkload)
			{
				bool mirrorDataLossOccurred;

				/*
				 * We may have to catch-up the mirror since bulk loading of data is
				 * ignored by resynchronize.
				 */
				while (true)
				{
					bool bulkLoadFinished;

					bulkLoadFinished = 
						MirroredBufferPool_EvaluateBulkLoadFinish(
									estate->es_into_relation_bulkloadinfo);

					if (bulkLoadFinished)
					{
						/*
						 * The flush was successful to the mirror (or the mirror is
						 * not configured).
						 *
						 * We have done a state-change from 'Bulk Load Create Pending'
						 * to 'Create Pending'.
						 */
						break;
					}

					/*
					 * Copy primary data to mirror and flush.
					 */
					MirroredBufferPool_CopyToMirror(
							&estate->es_into_relation_bulkloadinfo->relFileNode,
							estate->es_into_relation_descriptor->rd_rel->relname.data,
							&estate->es_into_relation_bulkloadinfo->persistentTid,
							estate->es_into_relation_bulkloadinfo->persistentSerialNum,
							estate->es_into_relation_bulkloadinfo->mirrorDataLossTrackingState,
							estate->es_into_relation_bulkloadinfo->mirrorDataLossTrackingSessionNum,
							numOfBlocks,
							&mirrorDataLossOccurred);
				}
			}
			else
			{
				if (Debug_persistent_print)
				{
					elog(Persistent_DebugPrintLevel(),
						 "CloseIntoRel %u/%u/%u: did not bypass the WAL -- did not use bulk load, persistent serial num " INT64_FORMAT ", TID %s",
						 estate->es_into_relation_bulkloadinfo->relFileNode.spcNode,
						 estate->es_into_relation_bulkloadinfo->relFileNode.dbNode,
						 estate->es_into_relation_bulkloadinfo->relFileNode.relNode,
						 estate->es_into_relation_bulkloadinfo->persistentSerialNum,
						 ItemPointerToString(&estate->es_into_relation_bulkloadinfo->persistentTid));
				}
			}
		}

		/* close rel, but keep lock until commit */
		heap_close(rel, NoLock);

		rel = NULL;
	}
}

/*
 * CreateIntoRelDestReceiver -- create a suitable DestReceiver object
 *
 * Since CreateDestReceiver doesn't accept the parameters we'd need,
 * we just leave the private fields empty here.  OpenIntoRel will
 * fill them in.
 */
DestReceiver *
CreateIntoRelDestReceiver(void)
{
	DR_intorel *self = (DR_intorel *) palloc(sizeof(DR_intorel));

	self->pub.receiveSlot = intorel_receive;
	self->pub.rStartup = intorel_startup;
	self->pub.rShutdown = intorel_shutdown;
	self->pub.rDestroy = intorel_destroy;
	self->pub.mydest = DestIntoRel;

	self->estate = NULL;
	self->ao_insertDesc = NULL;
        self->aocs_ins = NULL;

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
	EState	   *estate = myState->estate;
	Relation	into_rel = estate->es_into_relation_descriptor;

	Assert(estate->es_result_partitions == NULL);

	if (RelationIsAoRows(into_rel))
	{
		MemTuple	tuple = ExecCopySlotMemTuple(slot);
		Oid			tupleOid;
		AOTupleId	aoTupleId;

		if (myState->ao_insertDesc == NULL)
			myState->ao_insertDesc = appendonly_insert_init(into_rel, RESERVED_SEGNO, false);

		appendonly_insert(myState->ao_insertDesc, tuple, &tupleOid, &aoTupleId);
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
		HeapTuple	tuple = ExecCopySlotHeapTuple(slot);

		heap_insert(into_rel,
					tuple,
					estate->es_output_cid,
					!estate->es_into_relation_is_bulkload,
					false, /* never any point in using FSM */
					GetCurrentTransactionId());

		estate->es_into_relation_last_heap_tid = tuple->t_self;

		heap_freetuple(tuple);
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
	EState	   *estate = myState->estate;
	Relation	into_rel = estate->es_into_relation_descriptor;


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
	EState	   *estate = cxt->estate;
	SliceTable *sliceTable = estate->es_sliceTable;
	int			parentSliceIndex = cxt->currentSliceId;
	bool		result;

	if (node == NULL)
		return false;

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

	cxt.prefix.node = (Node *) stmt;
	cxt.estate = estate;
	cxt.currentSliceId = 0;

	/*
	 * NOTE: We depend on plan_tree_walker() to recurse into subplans of
	 * SubPlan nodes.
	 */
	FillSliceTable_walker((Node *) stmt->planTree, &cxt);
}
