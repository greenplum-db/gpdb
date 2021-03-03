/*-------------------------------------------------------------------------
 *
 * nodeSort.c
 *	  Routines to handle sorting of relations.
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeSort.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/parallel.h"
#include "executor/execdebug.h"
#include "executor/nodeSort.h"
#include "lib/stringinfo.h"             /* StringInfo */
#include "miscadmin.h"
#include "utils/tuplesort.h"
#include "cdb/cdbvars.h" /* CDB *//* gp_sort_flags */
#include "utils/workfile_mgr.h"
#include "executor/instrument.h"
#include "utils/faultinjector.h"

static void ExecSortExplainEnd(PlanState *planstate, struct StringInfoData *buf);
static void ExecEagerFreeSort(SortState *node);

/* ----------------------------------------------------------------
 *		ExecSort
 *
 *		Sorts tuples from the outer subtree of the node using tuplesort,
 *		which saves the results in a temporary file or memory. After the
 *		initial call, returns a tuple from the file with each call.
 *
 *		Conditions:
 *		  -- none.
 *
 *		Initial States:
 *		  -- the outer child is prepared to return the first tuple.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecSort(PlanState *pstate)
{
	SortState  *node = castNode(SortState, pstate);
	EState	   *estate;
	ScanDirection dir;
	Tuplesortstate *tuplesortstate;
	TupleTableSlot *slot = NULL;
	Sort 		*plannode = NULL;
	PlanState  *outerNode = NULL;
	TupleDesc	tupDesc = NULL;

	CHECK_FOR_INTERRUPTS();

	/*
	 * get state info from node
	 */
	SO1_printf("ExecSort: %s\n",
			   "entering routine");

	estate = node->ss.ps.state;
	dir = estate->es_direction;
	tuplesortstate = (Tuplesortstate *) node->tuplesortstate;

	/*
	 * In Window node, we might need to call ExecSort again even when
	 * the last tuple in the Sort has been retrieved. Since we might
	 * eager free the tuplestore, the tuplestorestate could be NULL.
	 * We simply return NULL in this case.
	 */
	if (node->sort_Done && tuplesortstate == NULL)
	{
		return NULL;
	}

	plannode = (Sort *) node->ss.ps.plan;


	/*
	 * If called for the first time, initialize tuplesort_state
	 */

	if (!node->sort_Done)
	{
		SO1_printf("ExecSort: %s\n",
				   "sorting subplan");

		/*
		 * Want to scan subplan in the forward direction while creating the
		 * sorted data.
		 */
		estate->es_direction = ForwardScanDirection;

		/*
		 * Initialize tuplesort module.
		 */
		SO1_printf("ExecSort: %s\n",
				   "calling tuplesort_begin");

		outerNode = outerPlanState(node);
		tupDesc = ExecGetResultType(outerNode);

		tuplesortstate = tuplesort_begin_heap(//&node->ss,
											  tupDesc,
											  plannode->numCols,
											  plannode->sortColIdx,
											  plannode->sortOperators,
											  plannode->collations,
											  plannode->nullsFirst,
											  PlanStateOperatorMemKB((PlanState *) node),
											  NULL,
											  node->randomAccess);

		if (node->bounded)
			tuplesort_set_bound(tuplesortstate, node->bound);
		node->tuplesortstate = (void *) tuplesortstate;

		/* CDB */
		/* GPDB_12_MERGE_FIXME: these optimizations are currently broken */
#if 0
		{
			int 		unique = 0;
			int 		sort_flags = gp_sort_flags; /* get the guc */
			int         maxdistinct = gp_sort_max_distinct; /* get the guc */

			if (node->noduplicates)
				unique = 1;
			
			cdb_tuplesort_init(tuplesortstate, unique, sort_flags, maxdistinct);
		}
#endif

		/* If EXPLAIN ANALYZE, share our Instrumentation object with sort. */
		/* GPDB_12_MERGE_FIXME: broken */
#if 0
		if (node->ss.ps.instrument && node->ss.ps.instrument->need_cdb)
			tuplesort_set_instrument(tuplesortstate,
									 node->ss.ps.instrument,
									 node->ss.ps.cdbexplainbuf);
#endif
	}

	/*
	 * If first time through,
	 * read all tuples from outer plan and pass them to
	 * tuplesort.c. Subsequent calls just fetch tuples from tuplesort.
	 */
	if (!node->sort_Done)
	{

		Assert(outerNode != NULL);

		/*
		 * Scan the subplan and feed all the tuples to tuplesort.
		 */

		for (;;)
		{
			slot = ExecProcNode(outerNode);

			if (TupIsNull(slot))
				break;

			tuplesort_puttupleslot(tuplesortstate, slot);
		}

		SIMPLE_FAULT_INJECTOR("execsort_before_sorting");

		/*
		 * Complete the sort.
		 */
		tuplesort_performsort(tuplesortstate);

		/*
		 * restore to user specified direction
		 */
		estate->es_direction = dir;

		/*
		 * finally set the sorted flag to true
		 */
		node->sort_Done = true;
		node->bounded_Done = node->bounded;
		node->bound_Done = node->bound;
		if (node->shared_info && node->am_worker)
		{
			TuplesortInstrumentation *si;

			Assert(IsParallelWorker());
			Assert(ParallelWorkerNumber <= node->shared_info->num_workers);
			si = &node->shared_info->sinstrument[ParallelWorkerNumber];
			tuplesort_get_stats(tuplesortstate, si);
		}
		SO1_printf("ExecSort: %s\n", "sorting done");
	}

	SO1_printf("ExecSort: %s\n",
			   "retrieving tuple from tuplesort");

	/*
	 * Get the first or next tuple from tuplesort. Returns NULL if no more
	 * tuples.  Note that we only rely on slot tuple remaining valid until the
	 * next fetch from the tuplesort.
	 */
	slot = node->ss.ps.ps_ResultTupleSlot;
	(void) tuplesort_gettupleslot(tuplesortstate,
								  ScanDirectionIsForward(dir),
								  false, slot, NULL);

	if (TupIsNull(slot) && !node->delayEagerFree)
	{
		ExecEagerFreeSort(node);
	}

	return slot;
}

/* ----------------------------------------------------------------
 *		ExecInitSort
 *
 *		Creates the run-time state information for the sort node
 *		produced by the planner and initializes its outer subtree.
 * ----------------------------------------------------------------
 */
SortState *
ExecInitSort(Sort *node, EState *estate, int eflags)
{
	SortState  *sortstate;

	SO1_printf("ExecInitSort: %s\n",
			   "initializing sort node");

	/*
	 * create state structure
	 */
	sortstate = makeNode(SortState);
	sortstate->ss.ps.plan = (Plan *) node;
	sortstate->ss.ps.state = estate;
	sortstate->ss.ps.ExecProcNode = ExecSort;

	/*
	 * We must have random access to the sort output to do backward scan or
	 * mark/restore.  We also prefer to materialize the sort output if we
	 * might be called on to rewind and replay it many times.
	 */
	sortstate->randomAccess = (eflags & (EXEC_FLAG_REWIND |
										 EXEC_FLAG_BACKWARD |
										 EXEC_FLAG_MARK)) != 0;

	sortstate->bounded = false;
	sortstate->sort_Done = false;
	sortstate->tuplesortstate = NULL;

	/* CDB */

	/* BUT:
	 * The LIMIT optimizations requires exprcontext in which to
	 * evaluate the limit/offset parameters.
	 */
	ExecAssignExprContext(estate, &sortstate->ss.ps);

	/* CDB */ /* evaluate a limit as part of the sort */
	{
		sortstate->noduplicates = node->noduplicates;
	}

	/*
	 * Miscellaneous initialization
	 *
	 * Sort nodes don't initialize their ExprContexts because they never call
	 * ExecQual or ExecProject.
	 */

	/*
	 * CDB: Offer extra info for EXPLAIN ANALYZE.
	 */
	if (estate->es_instrument && (estate->es_instrument & INSTRUMENT_CDB))
	{
		/* Allocate string buffer. */
		sortstate->ss.ps.cdbexplainbuf = makeStringInfo();

		/* Request a callback at end of query. */
		sortstate->ss.ps.cdbexplainfun = ExecSortExplainEnd;
	}

	/*
	 * If eflag contains EXEC_FLAG_REWIND or EXEC_FLAG_BACKWARD or EXEC_FLAG_MARK,
	 * then this node is not eager free safe.
	 */
	sortstate->delayEagerFree =
		((eflags & (EXEC_FLAG_REWIND | EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)) != 0);

	/*
	 * initialize child nodes
	 *
	 * We shield the child node from the need to support BACKWARD, or
	 * MARK/RESTORE.
	 */

	eflags &= ~(EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK);

	/*
	 * If Sort does not have any external parameters, then it
	 * can shield the child node from being rescanned as well, hence
	 * we can clear the EXEC_FLAG_REWIND as well. If there are parameters,
	 * don't clear the REWIND flag, as the child will be rewound.
	 */

	if (node->plan.allParam == NULL || node->plan.extParam == NULL)
	{
		eflags &= ~EXEC_FLAG_REWIND;
	}

	outerPlanState(sortstate) = ExecInitNode(outerPlan(node), estate, eflags);

	/*
	 * If the child node of a Material is a Motion, then this Material node is
	 * not eager free safe.
	 */
	if (IsA(outerPlan((Plan *)node), Motion))
	{
		sortstate->delayEagerFree = true;
	}

	/*
	 * Initialize scan slot and type.
	 */
	ExecCreateScanSlotFromOuterPlan(estate, &sortstate->ss, &TTSOpsVirtual);

	/*
	 * Initialize return slot and type. No need to initialize projection info
	 * because this node doesn't do projections.
	 */
	ExecInitResultTupleSlotTL(&sortstate->ss.ps, &TTSOpsMinimalTuple);
	sortstate->ss.ps.ps_ProjInfo = NULL;

	SO1_printf("ExecInitSort: %s\n",
			   "sort node initialized");

	return sortstate;
}

/* ----------------------------------------------------------------
 *		ExecEndSort(node)
 * ----------------------------------------------------------------
 */
void
ExecEndSort(SortState *node)
{
	SO1_printf("ExecEndSort: %s\n",
			   "shutting down sort node");

	ExecEagerFreeSort(node);

	/*
	 * shut down the subplan
	 */
	ExecEndNode(outerPlanState(node));

	SO1_printf("ExecEndSort: %s\n",
			   "sort node shutdown");
}

/* ----------------------------------------------------------------
 *		ExecSortMarkPos
 *
 *		Calls tuplesort to save the current position in the sorted file.
 * ----------------------------------------------------------------
 */
void
ExecSortMarkPos(SortState *node)
{
	/*
	 * if we haven't sorted yet, just return
	 */
	if (!node->sort_Done)
		return;

	tuplesort_markpos((Tuplesortstate *) node->tuplesortstate);
}

/* ----------------------------------------------------------------
 *		ExecSortRestrPos
 *
 *		Calls tuplesort to restore the last saved sort file position.
 * ----------------------------------------------------------------
 */
void
ExecSortRestrPos(SortState *node)
{
	/*
	 * if we haven't sorted yet, just return.
	 */
	if (!node->sort_Done)
		return;

	/*
	 * restore the scan to the previously marked position
	 */
	tuplesort_restorepos((Tuplesortstate *) node->tuplesortstate);
}

void
ExecReScanSort(SortState *node)
{
	PlanState  *outerPlan = outerPlanState(node);

	/*
	 * If we haven't sorted yet, just return. If outerplan's chgParam is not
	 * NULL then it will be re-scanned by ExecProcNode, else no reason to
	 * re-scan it at all.
	 */
	if (!node->sort_Done)
		return;

	/* must drop pointer to sort result tuple */
	ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);

	/*
	 * If subnode is to be rescanned then we forget previous sort results; we
	 * have to re-read the subplan and re-sort.  Also must re-sort if the
	 * bounded-sort parameters changed or we didn't select randomAccess.
	 *
	 * Otherwise we can just rewind and rescan the sorted output.
	 */
	if (outerPlan->chgParam != NULL ||
		node->bounded != node->bounded_Done ||
		node->bound != node->bound_Done ||
		!node->randomAccess ||
		(node->tuplesortstate == NULL))
	{
		node->sort_Done = false;

		if (node->tuplesortstate != NULL)
		{
			tuplesort_end((Tuplesortstate *) node->tuplesortstate);
		}

		/*
		 * if chgParam of subnode is not null then plan will be re-scanned by
		 * first ExecProcNode.
		 */
		if (outerPlan->chgParam == NULL)
			ExecReScan(outerPlan);
	}
	else
		tuplesort_rescan((Tuplesortstate *) node->tuplesortstate);
}


/*
 * ExecSortExplainEnd
 *      Called before ExecutorEnd to finish EXPLAIN ANALYZE reporting.
 */
void
ExecSortExplainEnd(PlanState *planstate, struct StringInfoData *buf)
{
	SortState *sortstate = (SortState *) planstate;

	if (sortstate->tuplesortstate)
	{
		tuplesort_get_stats(sortstate->tuplesortstate,
							&sortstate->sortstats);

		if (planstate->instrument)
		{
			planstate->instrument->workfileCreated = (sortstate->sortstats.spaceType == SORT_SPACE_TYPE_DISK);
			planstate->instrument->workmemused = sortstate->sortstats.workmemused;
		}
	}
}                               /* ExecSortExplainEnd */

static void
ExecEagerFreeSort(SortState *node)
{
	/* clean out the tuple table */
	ExecClearTuple(node->ss.ss_ScanTupleSlot);

	/* must drop pointer to sort result tuple */
	ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);

	if (node->tuplesortstate != NULL)
	{
		/*
		 * Save stats like in ExecSortExplainEnd, so that we can display
		 * them later in EXPLAIN ANALYZE.
		 */
		tuplesort_get_stats(node->tuplesortstate,
							&node->sortstats);
		if (node->ss.ps.instrument)
		{
			node->ss.ps.instrument->workfileCreated = (node->sortstats.spaceType == SORT_SPACE_TYPE_DISK);
			node->ss.ps.instrument->workmemused = node->sortstats.workmemused;
		}

		tuplesort_end((Tuplesortstate *) node->tuplesortstate);
		node->tuplesortstate = NULL;
	}
}

void
ExecSquelchSort(SortState *node)
{
	ExecEagerFreeSort(node);
	ExecSquelchNode(outerPlanState(node));
}

/* ----------------------------------------------------------------
 *						Parallel Query Support
 * ----------------------------------------------------------------
 */

/* ----------------------------------------------------------------
 *		ExecSortEstimate
 *
 *		Estimate space required to propagate sort statistics.
 * ----------------------------------------------------------------
 */
void
ExecSortEstimate(SortState *node, ParallelContext *pcxt)
{
	Size		size;

	/* don't need this if not instrumenting or no workers */
	if (!node->ss.ps.instrument || pcxt->nworkers == 0)
		return;

	size = mul_size(pcxt->nworkers, sizeof(TuplesortInstrumentation));
	size = add_size(size, offsetof(SharedSortInfo, sinstrument));
	shm_toc_estimate_chunk(&pcxt->estimator, size);
	shm_toc_estimate_keys(&pcxt->estimator, 1);
}

/* ----------------------------------------------------------------
 *		ExecSortInitializeDSM
 *
 *		Initialize DSM space for sort statistics.
 * ----------------------------------------------------------------
 */
void
ExecSortInitializeDSM(SortState *node, ParallelContext *pcxt)
{
	Size		size;

	/* don't need this if not instrumenting or no workers */
	if (!node->ss.ps.instrument || pcxt->nworkers == 0)
		return;

	size = offsetof(SharedSortInfo, sinstrument)
		+ pcxt->nworkers * sizeof(TuplesortInstrumentation);
	node->shared_info = shm_toc_allocate(pcxt->toc, size);
	/* ensure any unfilled slots will contain zeroes */
	memset(node->shared_info, 0, size);
	node->shared_info->num_workers = pcxt->nworkers;
	shm_toc_insert(pcxt->toc, node->ss.ps.plan->plan_node_id,
				   node->shared_info);
}

/* ----------------------------------------------------------------
 *		ExecSortInitializeWorker
 *
 *		Attach worker to DSM space for sort statistics.
 * ----------------------------------------------------------------
 */
void
ExecSortInitializeWorker(SortState *node, ParallelWorkerContext *pwcxt)
{
	node->shared_info =
		shm_toc_lookup(pwcxt->toc, node->ss.ps.plan->plan_node_id, true);
	node->am_worker = true;
}

/* ----------------------------------------------------------------
 *		ExecSortRetrieveInstrumentation
 *
 *		Transfer sort statistics from DSM to private memory.
 * ----------------------------------------------------------------
 */
void
ExecSortRetrieveInstrumentation(SortState *node)
{
	Size		size;
	SharedSortInfo *si;

	if (node->shared_info == NULL)
		return;

	size = offsetof(SharedSortInfo, sinstrument)
		+ node->shared_info->num_workers * sizeof(TuplesortInstrumentation);
	si = palloc(size);
	memcpy(si, node->shared_info, size);
	node->shared_info = si;
}
