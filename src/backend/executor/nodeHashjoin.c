/*-------------------------------------------------------------------------
 *
 * nodeHashjoin.c
 *	  Routines to handle hash join nodes
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeHashjoin.c
 *
 * PARALLELISM
 *
 * Hash joins can participate in parallel query execution in several ways.  A
 * parallel-oblivious hash join is one where the node is unaware that it is
 * part of a parallel plan.  In this case, a copy of the inner plan is used to
 * build a copy of the hash table in every backend, and the outer plan could
 * either be built from a partial or complete path, so that the results of the
 * hash join are correspondingly either partial or complete.  A parallel-aware
 * hash join is one that behaves differently, coordinating work between
 * backends, and appears as Parallel Hash Join in EXPLAIN output.  A Parallel
 * Hash Join always appears with a Parallel Hash node.
 *
 * Parallel-aware hash joins use the same per-backend state machine to track
 * progress through the hash join algorithm as parallel-oblivious hash joins.
 * In a parallel-aware hash join, there is also a shared state machine that
 * co-operating backends use to synchronize their local state machines and
 * program counters.  The shared state machine is managed with a Barrier IPC
 * primitive.  When all attached participants arrive at a barrier, the phase
 * advances and all waiting participants are released.
 *
 * When a participant begins working on a parallel hash join, it must first
 * figure out how much progress has already been made, because participants
 * don't wait for each other to begin.  For this reason there are switch
 * statements at key points in the code where we have to synchronize our local
 * state machine with the phase, and then jump to the correct part of the
 * algorithm so that we can get started.
 *
 * One barrier called build_barrier is used to coordinate the hashing phases.
 * The phase is represented by an integer which begins at zero and increments
 * one by one, but in the code it is referred to by symbolic names as follows:
 *
 *   PHJ_BUILD_ELECTING              -- initial state
 *   PHJ_BUILD_ALLOCATING            -- one sets up the batches and table 0
 *   PHJ_BUILD_HASHING_INNER         -- all hash the inner rel
 *   PHJ_BUILD_HASHING_OUTER         -- (multi-batch only) all hash the outer
 *   PHJ_BUILD_DONE                  -- building done, probing can begin
 *
 * While in the phase PHJ_BUILD_HASHING_INNER a separate pair of barriers may
 * be used repeatedly as required to coordinate expansions in the number of
 * batches or buckets.  Their phases are as follows:
 *
 *   PHJ_GROW_BATCHES_ELECTING       -- initial state
 *   PHJ_GROW_BATCHES_ALLOCATING     -- one allocates new batches
 *   PHJ_GROW_BATCHES_REPARTITIONING -- all repartition
 *   PHJ_GROW_BATCHES_FINISHING      -- one cleans up, detects skew
 *
 *   PHJ_GROW_BUCKETS_ELECTING       -- initial state
 *   PHJ_GROW_BUCKETS_ALLOCATING     -- one allocates new buckets
 *   PHJ_GROW_BUCKETS_REINSERTING    -- all insert tuples
 *
 * If the planner got the number of batches and buckets right, those won't be
 * necessary, but on the other hand we might finish up needing to expand the
 * buckets or batches multiple times while hashing the inner relation to stay
 * within our memory budget and load factor target.  For that reason it's a
 * separate pair of barriers using circular phases.
 *
 * The PHJ_BUILD_HASHING_OUTER phase is required only for multi-batch joins,
 * because we need to divide the outer relation into batches up front in order
 * to be able to process batches entirely independently.  In contrast, the
 * parallel-oblivious algorithm simply throws tuples 'forward' to 'later'
 * batches whenever it encounters them while scanning and probing, which it
 * can do because it processes batches in serial order.
 *
 * Once PHJ_BUILD_DONE is reached, backends then split up and process
 * different batches, or gang up and work together on probing batches if there
 * aren't enough to go around.  For each batch there is a separate barrier
 * with the following phases:
 *
 *  PHJ_BATCH_ELECTING       -- initial state
 *  PHJ_BATCH_ALLOCATING     -- one allocates buckets
 *  PHJ_BATCH_LOADING        -- all load the hash table from disk
 *  PHJ_BATCH_PROBING        -- all probe
 *  PHJ_BATCH_DONE           -- end
 *
 * Batch 0 is a special case, because it starts out in phase
 * PHJ_BATCH_PROBING; populating batch 0's hash table is done during
 * PHJ_BUILD_HASHING_INNER so we can skip loading.
 *
 * Initially we try to plan for a single-batch hash join using the combined
 * work_mem of all participants to create a large shared hash table.  If that
 * turns out either at planning or execution time to be impossible then we
 * fall back to regular work_mem sized hash tables.
 *
 * To avoid deadlocks, we never wait for any barrier unless it is known that
 * all other backends attached to it are actively executing the node or have
 * already arrived.  Practically, that means that we never return a tuple
 * while attached to a barrier, unless the barrier has reached its final
 * state.  In the slightly special case of the per-batch barrier, we return
 * tuples while in PHJ_BATCH_PROBING phase, but that's OK because we use
 * BarrierArriveAndDetach() to advance it to PHJ_BATCH_DONE without waiting.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/htup_details.h"
#include "access/parallel.h"
#include "executor/executor.h"
#include "executor/hashjoin.h"
#include "executor/instrument.h"	/* Instrumentation */
#include "executor/nodeHash.h"
#include "executor/nodeHashjoin.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "utils/memutils.h"
#include "utils/sharedtuplestore.h"

#include "cdb/cdbvars.h"
#include "miscadmin.h"			/* work_mem */
#include "utils/faultinjector.h"

/*
 * States of the ExecHashJoin state machine
 */
#define HJ_BUILD_HASHTABLE		1
#define HJ_NEED_NEW_OUTER		2
#define HJ_SCAN_BUCKET			3
#define HJ_FILL_OUTER_TUPLE		4
#define HJ_FILL_INNER_TUPLES	5
#define HJ_NEED_NEW_BATCH		6

/* Returns true if doing null-fill on outer relation */
#define HJ_FILL_OUTER(hjstate)	((hjstate)->hj_NullInnerTupleSlot != NULL)
/* Returns true if doing null-fill on inner relation */
#define HJ_FILL_INNER(hjstate)	((hjstate)->hj_NullOuterTupleSlot != NULL)

extern bool Test_print_prefetch_joinqual;

static TupleTableSlot *ExecHashJoinOuterGetTuple(PlanState *outerNode,
												 HashJoinState *hjstate,
												 uint32 *hashvalue);
static TupleTableSlot *ExecParallelHashJoinOuterGetTuple(PlanState *outerNode,
														 HashJoinState *hjstate,
														 uint32 *hashvalue);
static TupleTableSlot *ExecHashJoinGetSavedTuple(HashJoinState *hjstate,
												 BufFile *file,
												 uint32 *hashvalue,
												 TupleTableSlot *tupleSlot);
static bool ExecHashJoinNewBatch(HashJoinState *hjstate);
static bool isNotDistinctJoin(List *qualList);
static bool ExecParallelHashJoinNewBatch(HashJoinState *hjstate);
static void ExecParallelHashJoinPartitionOuter(HashJoinState *node);

static void ReleaseHashTable(HashJoinState *node);

static void SpillCurrentBatch(HashJoinState *node);
static bool ExecHashJoinReloadHashTable(HashJoinState *hjstate);
static void ExecEagerFreeHashJoin(HashJoinState *node);

/* ----------------------------------------------------------------
 *		ExecHashJoinImpl
 *
 *		This function implements the Hybrid Hashjoin algorithm.  It is marked
 *		with an always-inline attribute so that ExecHashJoin() and
 *		ExecParallelHashJoin() can inline it.  Compilers that respect the
 *		attribute should create versions specialized for parallel == true and
 *		parallel == false with unnecessary branches removed.
 *
 *		Note: the relation we build hash table on is the "inner"
 *			  the other one is "outer".
 * ----------------------------------------------------------------
 */
static pg_attribute_always_inline TupleTableSlot *
ExecHashJoinImpl(PlanState *pstate, bool parallel)
{
	HashJoinState *node = castNode(HashJoinState, pstate);
	PlanState  *outerNode;
	HashState  *hashNode;
	ExprState  *joinqual;
	ExprState  *otherqual;
	ExprContext *econtext;
	HashJoinTable hashtable;
	TupleTableSlot *outerTupleSlot;
	uint32		hashvalue;
	int			batchno;
	ParallelHashJoinState *parallel_state;
	EState	   *estate;

	/*
	 * get information from HashJoin node
	 */
	estate = node->js.ps.state;
	joinqual = node->js.joinqual;
	otherqual = node->js.ps.qual;
	hashNode = (HashState *) innerPlanState(node);
	outerNode = outerPlanState(node);
	hashtable = node->hj_HashTable;
	econtext = node->js.ps.ps_ExprContext;
	parallel_state = hashNode->parallel_state;

	/*
	 * Reset per-tuple memory context to free any expression evaluation
	 * storage allocated in the previous tuple cycle.
	 */
	ResetExprContext(econtext);

	/*
	 * run the hash join state machine
	 */
	for (;;)
	{
		/* We must never use an eagerly released hash table */
		Assert(hashtable == NULL || !hashtable->eagerlyReleased);
		/*
		 * It's possible to iterate this loop many times before returning a
		 * tuple, in some pathological cases such as needing to move much of
		 * the current batch to a later batch.  So let's check for interrupts
		 * each time through.
		 */
		CHECK_FOR_INTERRUPTS();

		switch (node->hj_JoinState)
		{
			case HJ_BUILD_HASHTABLE:

				/*
				 * First time through: build hash table for inner relation.
				 */
				Assert(hashtable == NULL);

				/*
				 * MPP-4165: My fix for MPP-3300 was correct in that we avoided
				 * the *deadlock* but had very unexpected (and painful)
				 * performance characteristics: we basically de-pipeline and
				 * de-parallelize execution of any query which has motion below
				 * us.
				 *
				 * So now prefetch_inner is set (see createplan.c) if we have *any* motion
				 * below us. If we don't have any motion, it doesn't matter.
				 *
				 * See motion_sanity_walker() for details on how a deadlock may occur.
				 */
				if (!node->prefetch_inner)
				{
					/*
					 * If the outer relation is completely empty, and it's not
					 * right/full join, we can quit without building the hash
					 * table.  However, for an inner join it is only a win to
					 * check this when the outer relation's startup cost is less
					 * than the projected cost of building the hash table.
					 * Otherwise it's best to build the hash table first and see
					 * if the inner relation is empty.  (When it's a left join, we
					 * should always make this check, since we aren't going to be
					 * able to skip the join on the strength of an empty inner
					 * relation anyway.)
					 *
					 * If we are rescanning the join, we make use of information
					 * gained on the previous scan: don't bother to try the
					 * prefetch if the previous scan found the outer relation
					 * nonempty. This is not 100% reliable since with new
					 * parameters the outer relation might yield different
					 * results, but it's a good heuristic.
					 *
					 * The only way to make the check is to try to fetch a tuple
					 * from the outer plan node.  If we succeed, we have to stash
					 * it away for later consumption by ExecHashJoinOuterGetTuple.
					 */
					if (HJ_FILL_INNER(node))
					{
						/* no chance to not build the hash table */
						node->hj_FirstOuterTupleSlot = NULL;
					}
					else if (parallel)
					{
						/*
						 * The empty-outer optimization is not implemented for
						 * shared hash tables, because no one participant can
						 * determine that there are no outer tuples, and it's not
						 * yet clear that it's worth the synchronization overhead
						 * of reaching consensus to figure that out.  So we have
						 * to build the hash table.
						 */
						node->hj_FirstOuterTupleSlot = NULL;
					}
					else if (HJ_FILL_OUTER(node) ||
						 (outerNode->plan->startup_cost < hashNode->ps.plan->total_cost &&
						  !node->hj_OuterNotEmpty))
					{
						node->hj_FirstOuterTupleSlot = ExecProcNode(outerNode);
						if (TupIsNull(node->hj_FirstOuterTupleSlot))
						{
							node->hj_OuterNotEmpty = false;
							return NULL;
						}
						else
							node->hj_OuterNotEmpty = true;
					}
					else
						node->hj_FirstOuterTupleSlot = NULL;
				}
				else
				{
					/* see MPP-989 comment above, for now we assume that we have
					* at least one row on the outer. */
					node->hj_FirstOuterTupleSlot = NULL;
				}

				/*
				 * Create the hash table.  If using Parallel Hash, then
				 * whoever gets here first will create the hash table and any
				 * later arrivals will merely attach to it.
				 */
				hashtable = ExecHashTableCreate(hashNode,
												node,
												node->hj_HashOperators,
												node->hj_Collations,
				/*
				 * hashNode->hs_keepnull is required to support using IS NOT DISTINCT FROM as hash condition
				 * For example, in ORCA, `explain SELECT t2.a FROM t2 INTERSECT (SELECT t1.a FROM t1);`
				 */
												HJ_FILL_INNER(node) || hashNode->hs_keepnull,
												PlanStateOperatorMemKB((PlanState *) hashNode));
				node->hj_HashTable = hashtable;

				/*
				 * CDB: Offer extra info for EXPLAIN ANALYZE.
				 */
				if ((estate->es_instrument & INSTRUMENT_CDB))
					ExecHashTableExplainInit(hashNode, node, hashtable);

				/*
				 * Only if doing a LASJ_NOTIN join, we want to quit as soon as we find
				 * a NULL key on the inner side
				 */
				hashNode->hs_quit_if_hashkeys_null = (node->js.jointype == JOIN_LASJ_NOTIN);

				/*
				 * Execute the Hash node, to build the hash table.  If using
				 * Parallel Hash, then we'll try to help hashing unless we
				 * arrived too late.
				 */
				hashNode->hashtable = hashtable;
				(void) MultiExecProcNode((PlanState *) hashNode);

#ifdef HJDEBUG
				elog(gp_workfile_caching_loglevel, "HashJoin built table with %.1f tuples by executing subplan for batch 0", hashtable->totalTuples);
#endif

				/**
				 * If LASJ_NOTIN and a null was found on the inner side, then clean out.
				 */
				if (node->js.jointype == JOIN_LASJ_NOTIN && hashNode->hs_hashkeys_null)
					return NULL;

				/*
				 * If the inner relation is completely empty, and we're not
				 * doing a left outer join, we can quit without scanning the
				 * outer relation.
				 */
				if (hashtable->totalTuples == 0 && !HJ_FILL_OUTER(node))
					return NULL;

				/*
				 * Prefetch JoinQual to prevent motion hazard.
				 *
				 * See ExecPrefetchJoinQual() for details.
				 */
				if (node->prefetch_joinqual)
				{
					ExecPrefetchJoinQual(&node->js);
					node->prefetch_joinqual = false;
				}

				/*
				 * We just scanned the entire inner side and built the hashtable
				 * (and its overflow batches). Check here and remember if the inner
				 * side is empty.
				 */
				node->hj_InnerEmpty = (hashtable->totalTuples == 0);

				/*
				 * need to remember whether nbatch has increased since we
				 * began scanning the outer relation
				 */
				hashtable->nbatch_outstart = hashtable->nbatch;

				/*
				 * Reset OuterNotEmpty for scan.  (It's OK if we fetched a
				 * tuple above, because ExecHashJoinOuterGetTuple will
				 * immediately set it again.)
				 */
				node->hj_OuterNotEmpty = false;

				if (parallel)
				{
					Barrier    *build_barrier;

					build_barrier = &parallel_state->build_barrier;
					Assert(BarrierPhase(build_barrier) == PHJ_BUILD_HASHING_OUTER ||
						   BarrierPhase(build_barrier) == PHJ_BUILD_DONE);
					if (BarrierPhase(build_barrier) == PHJ_BUILD_HASHING_OUTER)
					{
						/*
						 * If multi-batch, we need to hash the outer relation
						 * up front.
						 */
						if (hashtable->nbatch > 1)
							ExecParallelHashJoinPartitionOuter(node);
						BarrierArriveAndWait(build_barrier,
											 WAIT_EVENT_HASH_BUILD_HASHING_OUTER);
					}
					Assert(BarrierPhase(build_barrier) == PHJ_BUILD_DONE);

					/* Each backend should now select a batch to work on. */
					hashtable->curbatch = -1;
					node->hj_JoinState = HJ_NEED_NEW_BATCH;

					continue;
				}
				else
					node->hj_JoinState = HJ_NEED_NEW_OUTER;

				/* FALL THRU */

			case HJ_NEED_NEW_OUTER:

				/* For a rescannable hash table we might need to reload batch 0 during rescan */
				if (hashtable->curbatch == -1 && !hashtable->first_pass)
				{
					hashtable->curbatch = 0;
					if (!ExecHashJoinReloadHashTable(node))
						return NULL;
				}

				/*
				 * We don't have an outer tuple, try to get the next one
				 */
				if (parallel)
					outerTupleSlot =
						ExecParallelHashJoinOuterGetTuple(outerNode, node,
														  &hashvalue);
				else
					outerTupleSlot =
						ExecHashJoinOuterGetTuple(outerNode, node, &hashvalue);

				if (TupIsNull(outerTupleSlot))
				{
					/* end of batch, or maybe whole join */
					if (HJ_FILL_INNER(node))
					{
						/* set up to scan for unmatched inner tuples */
						ExecPrepHashTableForUnmatched(node);
						node->hj_JoinState = HJ_FILL_INNER_TUPLES;
					}
					else
						node->hj_JoinState = HJ_NEED_NEW_BATCH;
					continue;
				}

				econtext->ecxt_outertuple = outerTupleSlot;
				node->hj_MatchedOuter = false;

				/*
				 * Find the corresponding bucket for this tuple in the main
				 * hash table or skew hash table.
				 */
				node->hj_CurHashValue = hashvalue;
				ExecHashGetBucketAndBatch(hashtable, hashvalue,
										  &node->hj_CurBucketNo, &batchno);
				node->hj_CurSkewBucketNo = ExecHashGetSkewBucket(hashtable,
																 hashvalue);
				node->hj_CurTuple = NULL;

				/*
				 * The tuple might not belong to the current batch (where
				 * "current batch" includes the skew buckets if any).
				 */
				if (batchno != hashtable->curbatch &&
					node->hj_CurSkewBucketNo == INVALID_SKEW_BUCKET_NO)
				{
					bool		shouldFree;
					MinimalTuple mintuple = ExecFetchSlotMinimalTuple(outerTupleSlot,
																	  &shouldFree);

					/*
					 * Need to postpone this outer tuple to a later batch.
					 * Save it in the corresponding outer-batch file.
					 */
					Assert(parallel_state == NULL);
					Assert(batchno > hashtable->curbatch);
					ExecHashJoinSaveTuple(&node->js.ps, mintuple,
										  hashvalue,
										  hashtable,
										  &hashtable->outerBatchFile[batchno],
										  hashtable->bfCxt);

					if (shouldFree)
						pfree(mintuple);

					/* Loop around, staying in HJ_NEED_NEW_OUTER state */
					continue;
				}

				/* OK, let's scan the bucket for matches */
				node->hj_JoinState = HJ_SCAN_BUCKET;

				/* FALL THRU */

			case HJ_SCAN_BUCKET:

				/*
				 * OPT-3325: Handle NULLs in the outer side of LASJ_NOTIN
				 *  - if tuple is NULL and inner is not empty, drop outer tuple
				 *  - if tuple is NULL and inner is empty, keep going as we'll
				 *    find no match for this tuple in the inner side
				 */
				if (node->js.jointype == JOIN_LASJ_NOTIN &&
					!node->hj_InnerEmpty &&
					isJoinExprNull(node->hj_OuterHashKeys,econtext))
				{
					node->hj_MatchedOuter = true;
					node->hj_JoinState = HJ_NEED_NEW_OUTER;
					continue;
				}

				/*
				 * Scan the selected hash bucket for matches to current outer
				 */
				if (parallel)
				{
					if (!ExecParallelScanHashBucket(hashNode, node, econtext))
					{
						/* out of matches; check for possible outer-join fill */
						node->hj_JoinState = HJ_FILL_OUTER_TUPLE;
						continue;
					}
				}
				else
				{
					if (!ExecScanHashBucket(hashNode, node, econtext))
					{
						/* out of matches; check for possible outer-join fill */
						node->hj_JoinState = HJ_FILL_OUTER_TUPLE;
						continue;
					}
				}

				/*
				 * We've got a match, but still need to test non-hashed quals.
				 * ExecScanHashBucket already set up all the state needed to
				 * call ExecQual.
				 *
				 * If we pass the qual, then save state for next call and have
				 * ExecProject form the projection, store it in the tuple
				 * table, and return the slot.
				 *
				 * Only the joinquals determine tuple match status, but all
				 * quals must pass to actually return the tuple.
				 */
				if (joinqual == NULL || ExecQual(joinqual, econtext))
				{
					node->hj_MatchedOuter = true;
					HeapTupleHeaderSetMatch(HJTUPLE_MINTUPLE(node->hj_CurTuple));

					/* In an antijoin, we never return a matched tuple */
					if (node->js.jointype == JOIN_ANTI ||
						node->js.jointype == JOIN_LASJ_NOTIN)
					{
						node->hj_JoinState = HJ_NEED_NEW_OUTER;
						continue;
					}

					/*
					 * If we only need to join to the first matching inner
					 * tuple, then consider returning this one, but after that
					 * continue with next outer tuple.
					 */
					if (node->js.single_match)
						node->hj_JoinState = HJ_NEED_NEW_OUTER;

					if (otherqual == NULL || ExecQual(otherqual, econtext))
						return ExecProject(node->js.ps.ps_ProjInfo);
					else
						InstrCountFiltered2(node, 1);
				}
				else
					InstrCountFiltered1(node, 1);
				break;

			case HJ_FILL_OUTER_TUPLE:

				/*
				 * The current outer tuple has run out of matches, so check
				 * whether to emit a dummy outer-join tuple.  Whether we emit
				 * one or not, the next state is NEED_NEW_OUTER.
				 */
				node->hj_JoinState = HJ_NEED_NEW_OUTER;

				if (!node->hj_MatchedOuter &&
					HJ_FILL_OUTER(node))
				{
					/*
					 * Generate a fake join tuple with nulls for the inner
					 * tuple, and return it if it passes the non-join quals.
					 */
					econtext->ecxt_innertuple = node->hj_NullInnerTupleSlot;

					if (otherqual == NULL || ExecQual(otherqual, econtext))
						return ExecProject(node->js.ps.ps_ProjInfo);
					else
						InstrCountFiltered2(node, 1);
				}
				break;

			case HJ_FILL_INNER_TUPLES:

				/*
				 * We have finished a batch, but we are doing right/full join,
				 * so any unmatched inner tuples in the hashtable have to be
				 * emitted before we continue to the next batch.
				 */
				if (!ExecScanHashTableForUnmatched(node, econtext))
				{
					/* no more unmatched tuples */
					node->hj_JoinState = HJ_NEED_NEW_BATCH;
					continue;
				}

				/*
				 * Generate a fake join tuple with nulls for the outer tuple,
				 * and return it if it passes the non-join quals.
				 */
				econtext->ecxt_outertuple = node->hj_NullOuterTupleSlot;

				if (otherqual == NULL || ExecQual(otherqual, econtext))
					return ExecProject(node->js.ps.ps_ProjInfo);
				else
					InstrCountFiltered2(node, 1);
				break;

			case HJ_NEED_NEW_BATCH:

				/*
				 * Try to advance to next batch.  Done if there are no more.
				 */
				if (parallel)
				{
					if (!ExecParallelHashJoinNewBatch(node))
						return NULL;	/* end of parallel-aware join */
				}
				else
				{
					if (!ExecHashJoinNewBatch(node))
						return NULL;	/* end of parallel-oblivious join */
				}
				node->hj_JoinState = HJ_NEED_NEW_OUTER;
				break;

			default:
				elog(ERROR, "unrecognized hashjoin state: %d",
					 (int) node->hj_JoinState);
		}
	}
}

/* ----------------------------------------------------------------
 *		ExecHashJoin
 *
 *		Parallel-oblivious version.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *			/* return: a tuple or NULL */
ExecHashJoin(PlanState *pstate)
{
	TupleTableSlot *result;

	/*
	 * On sufficiently smart compilers this should be inlined with the
	 * parallel-aware branches removed.
	 */
	result = ExecHashJoinImpl(pstate, false);

	if (TupIsNull(result) && !((HashJoinState *) pstate)->reuse_hashtable)
	{
		/*
		 * CDB: We'll read no more from inner subtree. To keep our
		 * sibling QEs from being starved, tell source QEs not to
		 * clog up the pipeline with our never-to-be-consumed
		 * data.
		 */
		ExecSquelchNode(pstate);
	}

	return result;
}

/* ----------------------------------------------------------------
 *		ExecParallelHashJoin
 *
 *		Parallel-aware version.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *			/* return: a tuple or NULL */
ExecParallelHashJoin(PlanState *pstate)
{
	TupleTableSlot *result;

	/*
	 * On sufficiently smart compilers this should be inlined with the
	 * parallel-oblivious branches removed.
	 */
	result = ExecHashJoinImpl(pstate, true);

	if (TupIsNull(result) && !((HashJoinState *) pstate)->reuse_hashtable)
	{
		/*
		 * CDB: We'll read no more from inner subtree. To keep our
		 * sibling QEs from being starved, tell source QEs not to
		 * clog up the pipeline with our never-to-be-consumed
		 * data.
		 */
		ExecSquelchNode(pstate);
	}

	return result;
}

/* ----------------------------------------------------------------
 *		ExecInitHashJoin
 *
 *		Init routine for HashJoin node.
 * ----------------------------------------------------------------
 */
HashJoinState *
ExecInitHashJoin(HashJoin *node, EState *estate, int eflags)
{
	HashJoinState *hjstate;
	Plan	   *outerNode;
	Hash	   *hashNode;
	List	   *lclauses;
	List	   *rclauses;
	List	   *rhclauses;
	List	   *hoperators;
	List	   *hcollations;
	TupleDesc	outerDesc,
				innerDesc;
	ListCell   *l;
	const TupleTableSlotOps *ops;

	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	/*
	 * create state structure
	 */
	hjstate = makeNode(HashJoinState);
	hjstate->js.ps.plan = (Plan *) node;
	hjstate->js.ps.state = estate;
	hjstate->reuse_hashtable = (eflags & EXEC_FLAG_REWIND) != 0;

	/*
	 * See ExecHashJoinInitializeDSM() and ExecHashJoinInitializeWorker()
	 * where this function may be replaced with a parallel version, if we
	 * managed to launch a parallel query.
	 */
	hjstate->js.ps.ExecProcNode = ExecHashJoin;
	hjstate->js.jointype = node->join.jointype;

	/*
	 * Miscellaneous initialization
	 *
	 * create expression context for node
	 */
	ExecAssignExprContext(estate, &hjstate->js.ps);

	if (node->hashqualclauses != NIL)
	{
		/* CDB: This must be an IS NOT DISTINCT join!  */
		Assert(isNotDistinctJoin(node->hashqualclauses));
		hjstate->hj_nonequijoin = true;
	}
	else
		hjstate->hj_nonequijoin = false;

	/*
	 * MPP-3300, we only pre-build hashtable if we need to (this is relaxing
	 * the fix to MPP-989)
	 */
	hjstate->prefetch_inner = node->join.prefetch_inner;
	hjstate->prefetch_joinqual = node->join.prefetch_joinqual;

	if (Test_print_prefetch_joinqual && hjstate->prefetch_joinqual)
		elog(NOTICE,
			 "prefetch join qual in slice %d of plannode %d",
			 currentSliceId, ((Plan *) node)->plan_node_id);

	/*
	 * initialize child nodes
	 *
	 * Note: we could suppress the REWIND flag for the inner input, which
	 * would amount to betting that the hash will be a single batch.  Not
	 * clear if this would be a win or not.
	 */
	hashNode = (Hash *) innerPlan(node);
	outerNode = outerPlan(node);

	/*
	 * XXX The following order are significant.  We init Hash first, then the outerNode
	 * this is the same order as we execute (in the sense of the first exec called).
	 * Until we have a better way to uncouple, share input needs this to be true.  If the
	 * order is wrong, when both hash and outer node have share input and (both ?) have 
	 * a subquery node, share input will fail because the estate of the nodes can not be
	 * set up correctly.
	 */
    innerPlanState(hjstate) = ExecInitNode((Plan *) hashNode, estate, eflags);
    innerDesc = ExecGetResultType(innerPlanState(hjstate));
	((HashState *) innerPlanState(hjstate))->hs_keepnull = hjstate->hj_nonequijoin;

	outerPlanState(hjstate) = ExecInitNode(outerNode, estate, eflags);
	outerDesc = ExecGetResultType(outerPlanState(hjstate));

	/*
	 * Initialize result slot, type and projection.
	 */
	ExecInitResultTupleSlotTL(&hjstate->js.ps, &TTSOpsVirtual);
	ExecAssignProjectionInfo(&hjstate->js.ps, NULL);

	/*
	 * tuple table initialization
	 */
	ops = ExecGetResultSlotOps(outerPlanState(hjstate), NULL);
	hjstate->hj_OuterTupleSlot = ExecInitExtraTupleSlot(estate, outerDesc,
														ops);

	/*
	 * detect whether we need only consider the first matching inner tuple
	 */
	hjstate->js.single_match = (node->join.inner_unique ||
								node->join.jointype == JOIN_SEMI);

	/* set up null tuples for outer joins, if needed */
	switch (node->join.jointype)
	{
		case JOIN_INNER:
		case JOIN_SEMI:
			break;
		case JOIN_LEFT:
		case JOIN_ANTI:
		case JOIN_LASJ_NOTIN:
			hjstate->hj_NullInnerTupleSlot =
				ExecInitNullTupleSlot(estate, innerDesc, &TTSOpsVirtual);
			break;
		case JOIN_RIGHT:
			hjstate->hj_NullOuterTupleSlot =
				ExecInitNullTupleSlot(estate, outerDesc, &TTSOpsVirtual);
			break;
		case JOIN_FULL:
			hjstate->hj_NullOuterTupleSlot =
				ExecInitNullTupleSlot(estate, outerDesc, &TTSOpsVirtual);
			hjstate->hj_NullInnerTupleSlot =
				ExecInitNullTupleSlot(estate, innerDesc, &TTSOpsVirtual);
			break;
		default:
			elog(ERROR, "unrecognized join type: %d",
				 (int) node->join.jointype);
	}

	/*
	 * now for some voodoo.  our temporary tuple slot is actually the result
	 * tuple slot of the Hash node (which is our inner plan).  we can do this
	 * because Hash nodes don't return tuples via ExecProcNode() -- instead
	 * the hash join node uses ExecScanHashBucket() to get at the contents of
	 * the hash table.  -cim 6/9/91
	 */
	{
		HashState  *hashstate = (HashState *) innerPlanState(hjstate);
		TupleTableSlot *slot = hashstate->ps.ps_ResultTupleSlot;

		hjstate->hj_HashTupleSlot = slot;
	}

	/*
	 * initialize child expressions
	 */
	hjstate->js.ps.qual =
		ExecInitQual(node->join.plan.qual, (PlanState *) hjstate);
	hjstate->js.joinqual =
		ExecInitQual(node->join.joinqual, (PlanState *) hjstate);
	hjstate->hashclauses =
		ExecInitQual(node->hashclauses, (PlanState *) hjstate);

	if (node->hashqualclauses != NIL)
	{
		hjstate->hashqualclauses =
			ExecInitQual(node->hashqualclauses, (PlanState *) hjstate);
	}
	else
	{
		hjstate->hashqualclauses = hjstate->hashclauses;
	}

	/*
	 * initialize hash-specific info
	 */
	hjstate->hj_HashTable = NULL;
	hjstate->hj_FirstOuterTupleSlot = NULL;

	hjstate->hj_CurHashValue = 0;
	hjstate->hj_CurBucketNo = 0;
	hjstate->hj_CurSkewBucketNo = INVALID_SKEW_BUCKET_NO;
	hjstate->hj_CurTuple = NULL;

	/*
	 * Deconstruct the hash clauses into outer and inner argument values, so
	 * that we can evaluate those subexpressions separately.  Also make a list
	 * of the hash operator OIDs, in preparation for looking up the hash
	 * functions to use.
	 */
	lclauses = NIL;
	rclauses = NIL;
	rhclauses = NIL;
	hoperators = NIL;
	hcollations = NIL;
	foreach(l, node->hashclauses)
	{
		OpExpr	   *hclause = lfirst_node(OpExpr, l);

		lclauses = lappend(lclauses, ExecInitExpr(linitial(hclause->args),
												  (PlanState *) hjstate));
		rclauses = lappend(rclauses, ExecInitExpr(lsecond(hclause->args),
												  (PlanState *) hjstate));
		rhclauses = lappend(rhclauses, ExecInitExpr(lsecond(hclause->args),
													innerPlanState(hjstate)));
		hoperators = lappend_oid(hoperators, hclause->opno);
		hcollations = lappend_oid(hcollations, hclause->inputcollid);
	}
	hjstate->hj_OuterHashKeys = lclauses;
	hjstate->hj_InnerHashKeys = rclauses;
	hjstate->hj_HashOperators = hoperators;
	hjstate->hj_Collations = hcollations;
	/* child Hash node needs to evaluate inner hash keys, too */
	((HashState *) innerPlanState(hjstate))->hashkeys = rhclauses;

	hjstate->hj_JoinState = HJ_BUILD_HASHTABLE;
	hjstate->hj_MatchedOuter = false;
	hjstate->hj_OuterNotEmpty = false;

	return hjstate;
}

/* ----------------------------------------------------------------
 *		ExecEndHashJoin
 *
 *		clean up routine for HashJoin node
 * ----------------------------------------------------------------
 */
void
ExecEndHashJoin(HashJoinState *node)
{
	/*
	 * Free hash table
	 */
	if (node->hj_HashTable)
	{
		if (!node->hj_HashTable->eagerlyReleased)
		{
			HashState  *hashState = (HashState *) innerPlanState(node);

			ExecHashTableDestroy(hashState, node->hj_HashTable);
		}
		pfree(node->hj_HashTable);
		node->hj_HashTable = NULL;
	}

	/*
	 * Free the exprcontext
	 */
	ExecFreeExprContext(&node->js.ps);

	/*
	 * clean out the tuple table
	 */
	ExecClearTuple(node->js.ps.ps_ResultTupleSlot);
	ExecClearTuple(node->hj_OuterTupleSlot);
	ExecClearTuple(node->hj_HashTupleSlot);

	/*
	 * clean up subtrees
	 */
	ExecEndNode(outerPlanState(node));
	ExecEndNode(innerPlanState(node));
}

/*
 * ExecHashJoinOuterGetTuple
 *
 *		get the next outer tuple for a parallel oblivious hashjoin: either by
 *		executing the outer plan node in the first pass, or from the temp
 *		files for the hashjoin batches.
 *
 * Returns a null slot if no more outer tuples (within the current batch).
 *
 * On success, the tuple's hash value is stored at *hashvalue --- this is
 * either originally computed, or re-read from the temp file.
 */
static TupleTableSlot *
ExecHashJoinOuterGetTuple(PlanState *outerNode,
						  HashJoinState *hjstate,
						  uint32 *hashvalue)
{
	HashJoinTable hashtable = hjstate->hj_HashTable;
	int			curbatch = hashtable->curbatch;
	TupleTableSlot *slot;
	ExprContext *econtext;
	HashState  *hashState = (HashState *) innerPlanState(hjstate);

	/* Read tuples from outer relation only if it's the first batch */
	if (curbatch == 0)
	{
		/*
		 * Check to see if first outer tuple was already fetched by
		 * ExecHashJoin() and not used yet.
		 */
		slot = hjstate->hj_FirstOuterTupleSlot;
		if (!TupIsNull(slot))
			hjstate->hj_FirstOuterTupleSlot = NULL;
		else
			slot = ExecProcNode(outerNode);

		while (!TupIsNull(slot))
		{
			/*
			 * We have to compute the tuple's hash value.
			 */
			econtext = hjstate->js.ps.ps_ExprContext;
			econtext->ecxt_outertuple = slot;

			bool hashkeys_null = false;
			bool keep_nulls = HJ_FILL_OUTER(hjstate) ||
					hjstate->hj_nonequijoin;
			if (ExecHashGetHashValue(hashState, hashtable, econtext,
									 hjstate->hj_OuterHashKeys,
									 true,	/* outer tuple */
									 keep_nulls,
									 hashvalue,
									 &hashkeys_null))
			{
				/* remember outer relation is not empty for possible rescan */
				hjstate->hj_OuterNotEmpty = true;

				return slot;
			}

			/*
			 * That tuple couldn't match because of a NULL, so discard it and
			 * continue with the next one.
			 */
			slot = ExecProcNode(outerNode);
		}

#ifdef HJDEBUG
		elog(gp_workfile_caching_loglevel, "HashJoin built table with %.1f tuples for batch %d", hashtable->totalTuples, curbatch);
#endif
	}
	else if (curbatch < hashtable->nbatch)
	{
		BufFile	   *file = hashtable->outerBatchFile[curbatch];

		/*
		 * In outer-join cases, we could get here even though the batch file
		 * is empty.
		 */
		if (file == NULL)
			return NULL;

		/*
		 * For batches > 0, we can be reading many many outer tuples from disk
		 * and probing them against the hashtable. If we don't find any
		 * matches, we'll keep coming back here to read tuples from disk and
		 * returning them (MPP-23213). Break this long tight loop here.
		 */
		CHECK_FOR_INTERRUPTS();

		if (QueryFinishPending)
			return NULL;

		slot = ExecHashJoinGetSavedTuple(hjstate,
										 file,
										 hashvalue,
										 hjstate->hj_OuterTupleSlot);
		if (!TupIsNull(slot))
			return slot;

#ifdef HJDEBUG
		elog(gp_workfile_caching_loglevel, "HashJoin built table with %.1f tuples for batch %d", hashtable->totalTuples, curbatch);
#endif
	}

	/* End of this batch */
	return NULL;
}

/*
 * ExecHashJoinOuterGetTuple variant for the parallel case.
 */
static TupleTableSlot *
ExecParallelHashJoinOuterGetTuple(PlanState *outerNode,
								  HashJoinState *hjstate,
								  uint32 *hashvalue)
{
	HashJoinTable hashtable = hjstate->hj_HashTable;
	int			curbatch = hashtable->curbatch;
	TupleTableSlot *slot;
	HashState  *hashState = (HashState *) innerPlanState(hjstate);

	/*
	 * In the Parallel Hash case we only run the outer plan directly for
	 * single-batch hash joins.  Otherwise we have to go to batch files, even
	 * for batch 0.
	 */
	if (curbatch == 0 && hashtable->nbatch == 1)
	{
		slot = ExecProcNode(outerNode);

		while (!TupIsNull(slot))
		{
			ExprContext *econtext = hjstate->js.ps.ps_ExprContext;
			bool		hashkeys_null = false;
			bool		keep_nulls;

			keep_nulls = HJ_FILL_OUTER(hjstate) ||
				hjstate->hj_nonequijoin;

			econtext->ecxt_outertuple = slot;
			if (ExecHashGetHashValue(hashState,
									 hashtable, econtext,
									 hjstate->hj_OuterHashKeys,
									 true,	/* outer tuple */
									 keep_nulls,
									 hashvalue,
									 &hashkeys_null))
				return slot;

			/*
			 * That tuple couldn't match because of a NULL, so discard it and
			 * continue with the next one.
			 */
			slot = ExecProcNode(outerNode);
		}
	}
	else if (curbatch < hashtable->nbatch)
	{
		MinimalTuple tuple;

		tuple = sts_parallel_scan_next(hashtable->batches[curbatch].outer_tuples,
									   hashvalue);
		if (tuple != NULL)
		{
			ExecForceStoreMinimalTuple(tuple,
									   hjstate->hj_OuterTupleSlot,
									   false);
			slot = hjstate->hj_OuterTupleSlot;
			return slot;
		}
		else
			ExecClearTuple(hjstate->hj_OuterTupleSlot);
	}

	/* End of this batch */
	return NULL;
}

/*
 * ExecHashJoinNewBatch
 *		switch to a new hashjoin batch
 *
 * Returns true if successful, false if there are no more batches.
 */
static bool
ExecHashJoinNewBatch(HashJoinState *hjstate)
{
	HashJoinTable hashtable = hjstate->hj_HashTable;
	int			nbatch;
	int			curbatch;

	SIMPLE_FAULT_INJECTOR("exec_hashjoin_new_batch");

	HashState  *hashState = (HashState *) innerPlanState(hjstate);

	nbatch = hashtable->nbatch;
	curbatch = hashtable->curbatch;

	if (curbatch >= nbatch)
		return false;

	if (curbatch >= 0 && hashtable->stats)
		ExecHashTableExplainBatchEnd(hashState, hashtable);

	if (curbatch > 0)
	{
		/*
		 * We no longer need the previous outer batch file; close it right
		 * away to free disk space.
		 */
		if (hashtable->outerBatchFile[curbatch])
			BufFileClose(hashtable->outerBatchFile[curbatch]);
		hashtable->outerBatchFile[curbatch] = NULL;
	}
	else	/* we just finished the first batch */
	{
		/*
		 * Reset some of the skew optimization state variables, since we no
		 * longer need to consider skew tuples after the first batch. The
		 * memory context reset we are about to do will release the skew
		 * hashtable itself.
		 */
		hashtable->skewEnabled = false;
		hashtable->skewBucket = NULL;
		hashtable->skewBucketNums = NULL;
		hashtable->nSkewBuckets = 0;
		hashtable->spaceUsedSkew = 0;
	}

	/*
	 * If we want to keep the hash table around, for re-scan, then write
	 * the current batch's state to disk before moving to the next one.
	 * It's possible that we increase the number of batches later, so that
	 * by the time we reload this file, some of the tuples we wrote here
	 * will logically belong to a later file. ExecHashJoinReloadHashTable
	 * will move such tuples when the file is reloaded.
	 *
	 * If we have already re-scanned, we might still have the old file
	 * around, in which case there's no need to write it again.
	 * XXX: Currently, we actually always re-create it, see comments in
	 * ExecHashJoinReloadHashTable.
	 */
	if (nbatch > 1 && hjstate->reuse_hashtable &&
		hashtable->innerBatchFile[curbatch] == NULL)
	{
		SpillCurrentBatch(hjstate);
	}

	/*
	 * We can always skip over any batches that are completely empty on both
	 * sides.  We can sometimes skip over batches that are empty on only one
	 * side, but there are exceptions:
	 *
	 * 1. In a left/full outer join, we have to process outer batches even if
	 * the inner batch is empty.  Similarly, in a right/full outer join, we
	 * have to process inner batches even if the outer batch is empty.
	 *
	 * 2. If we have increased nbatch since the initial estimate, we have to
	 * scan inner batches since they might contain tuples that need to be
	 * reassigned to later inner batches.
	 *
	 * 3. Similarly, if we have increased nbatch since starting the outer
	 * scan, we have to rescan outer batches in case they contain tuples that
	 * need to be reassigned.
	 */
	curbatch++;
	while (curbatch < nbatch &&
		   (hashtable->outerBatchFile[curbatch] == NULL ||
			hashtable->innerBatchFile[curbatch] == NULL))

	{
		/*
		 * For rescannable we must complete respilling on first batch
		 *
		 * Consider case 2: the inner workfile is not null. We are on the first pass
		 * (before ReScan was called). I.e., we are processing a join for the base
		 * case of a recursive CTE. If the base case does not have tuples for batch
		 * k (i.e., the outer workfile for batch k is null), and we never increased
		 * the initial number of batches, then we will skip the inner batchfile (case 2).
		 *
		 * However, one iteration of recursive CTE is no guarantee that the future outer
		 * batch will also not match batch k on the inner. Therefore, we may have a
		 * non-null outer batch k on some future iteration.
		 *
		 * If during loading batch k inner workfile for future iteration triggers a re-spill
		 * we will be forced to increase number of batches. This will result in wrong result
		 * as we will not write any inner tuples (we consider inner workfiles read-only after
		 * a rescan call).
		 *
		 * So, to produce wrong result, without this guard, the following conditions have
		 * to be true:
		 *
		 * 1. Outer batchfile for batch k is null
		 * 2. Inner batchfile for batch k not null
		 * 3. No resizing of nbatch for batch (0...(k-1))
		 * 4. Inner batchfile for batch k is too big to fit in memory
		 */
		if (hjstate->reuse_hashtable)
			break;

		if (hashtable->outerBatchFile[curbatch] &&
			HJ_FILL_OUTER(hjstate))
			break;				/* must process due to rule 1 */
		if (hashtable->innerBatchFile[curbatch] &&
			HJ_FILL_INNER(hjstate))
			break;				/* must process due to rule 1 */
		if (hashtable->innerBatchFile[curbatch] &&
			nbatch != hashtable->nbatch_original)
			break;				/* must process due to rule 2 */
		if (hashtable->outerBatchFile[curbatch] &&
			nbatch != hashtable->nbatch_outstart)
			break;				/* must process due to rule 3 */
		/* We can ignore this batch. */
		/* Release associated temp files right away. */
		if (hashtable->innerBatchFile[curbatch] && !hjstate->reuse_hashtable)
			BufFileClose(hashtable->innerBatchFile[curbatch]);
		hashtable->innerBatchFile[curbatch] = NULL;
		if (hashtable->outerBatchFile[curbatch])
			BufFileClose(hashtable->outerBatchFile[curbatch]);
		hashtable->outerBatchFile[curbatch] = NULL;

		curbatch++;
	}

	hashtable->curbatch = curbatch;		/* CDB: upd before return, even if no
										 * more data, so stats logic can see
										 * whether join was run to completion */

	if (curbatch >= nbatch)
		return false;			/* no more batches */

	if (!ExecHashJoinReloadHashTable(hjstate))
	{
		/* We no longer continue as we couldn't load the batch */
		return false;
	}

	/*
	 * Rewind outer batch file (if present), so that we can start reading it.
	 */
	if (hashtable->outerBatchFile[curbatch] != NULL)
	{
		if (BufFileSeek(hashtable->outerBatchFile[curbatch], 0, 0, SEEK_SET) != 0)
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not rewind hash-join temporary file: %m")));
	}

	return true;
}

/*
 * Choose a batch to work on, and attach to it.  Returns true if successful,
 * false if there are no more batches.
 */
static bool
ExecParallelHashJoinNewBatch(HashJoinState *hjstate)
{
	HashJoinTable hashtable = hjstate->hj_HashTable;
	int			start_batchno;
	int			batchno;

	/*
	 * If we started up so late that the batch tracking array has been freed
	 * already by ExecHashTableDetach(), then we are finished.  See also
	 * ExecParallelHashEnsureBatchAccessors().
	 */
	if (hashtable->batches == NULL)
		return false;

	/*
	 * If we were already attached to a batch, remember not to bother checking
	 * it again, and detach from it (possibly freeing the hash table if we are
	 * last to detach).
	 */
	if (hashtable->curbatch >= 0)
	{
		hashtable->batches[hashtable->curbatch].done = true;
		ExecHashTableDetachBatch(hashtable);
	}

	/*
	 * Search for a batch that isn't done.  We use an atomic counter to start
	 * our search at a different batch in every participant when there are
	 * more batches than participants.
	 */
	batchno = start_batchno =
		pg_atomic_fetch_add_u32(&hashtable->parallel_state->distributor, 1) %
		hashtable->nbatch;
	do
	{
		uint32		hashvalue;
		MinimalTuple tuple;
		TupleTableSlot *slot;

		if (!hashtable->batches[batchno].done)
		{
			SharedTuplestoreAccessor *inner_tuples;
			Barrier    *batch_barrier =
			&hashtable->batches[batchno].shared->batch_barrier;

			switch (BarrierAttach(batch_barrier))
			{
				case PHJ_BATCH_ELECTING:

					/* One backend allocates the hash table. */
					if (BarrierArriveAndWait(batch_barrier,
											 WAIT_EVENT_HASH_BATCH_ELECTING))
						ExecParallelHashTableAlloc(hashtable, batchno);
					/* Fall through. */

				case PHJ_BATCH_ALLOCATING:
					/* Wait for allocation to complete. */
					BarrierArriveAndWait(batch_barrier,
										 WAIT_EVENT_HASH_BATCH_ALLOCATING);
					/* Fall through. */

				case PHJ_BATCH_LOADING:
					/* Start (or join in) loading tuples. */
					ExecParallelHashTableSetCurrentBatch(hashtable, batchno);
					inner_tuples = hashtable->batches[batchno].inner_tuples;
					sts_begin_parallel_scan(inner_tuples);
					while ((tuple = sts_parallel_scan_next(inner_tuples,
														   &hashvalue)))
					{
						ExecForceStoreMinimalTuple(tuple,
												   hjstate->hj_HashTupleSlot,
												   false);
						slot = hjstate->hj_HashTupleSlot;
						ExecParallelHashTableInsertCurrentBatch(hashtable, slot,
																hashvalue);
					}
					sts_end_parallel_scan(inner_tuples);
					BarrierArriveAndWait(batch_barrier,
										 WAIT_EVENT_HASH_BATCH_LOADING);
					/* Fall through. */

				case PHJ_BATCH_PROBING:

					/*
					 * This batch is ready to probe.  Return control to
					 * caller. We stay attached to batch_barrier so that the
					 * hash table stays alive until everyone's finished
					 * probing it, but no participant is allowed to wait at
					 * this barrier again (or else a deadlock could occur).
					 * All attached participants must eventually call
					 * BarrierArriveAndDetach() so that the final phase
					 * PHJ_BATCH_DONE can be reached.
					 */
					ExecParallelHashTableSetCurrentBatch(hashtable, batchno);
					sts_begin_parallel_scan(hashtable->batches[batchno].outer_tuples);
					return true;

				case PHJ_BATCH_DONE:

					/*
					 * Already done.  Detach and go around again (if any
					 * remain).
					 */
					BarrierDetach(batch_barrier);
					hashtable->batches[batchno].done = true;
					hashtable->curbatch = -1;
					break;

				default:
					elog(ERROR, "unexpected batch phase %d",
						 BarrierPhase(batch_barrier));
			}
		}
		batchno = (batchno + 1) % hashtable->nbatch;
	} while (batchno != start_batchno);

	return false;
}

/*
 * ExecHashJoinSaveTuple
 *		save a tuple to a batch file.
 *
 * The data recorded in the file for each tuple is its hash value,
 * then the tuple in MinimalTuple format.
 *
 * Note: it is important always to call this in the regular executor
 * context, not in a shorter-lived context; else the temp file buffers
 * will get messed up.
 */
void
ExecHashJoinSaveTuple(PlanState *ps, MinimalTuple tuple, uint32 hashvalue,
					  HashJoinTable hashtable, BufFile **fileptr,
					  MemoryContext bfCxt)
{
	BufFile	   *file = *fileptr;
	size_t		written;

	if (hashtable->work_set == NULL)
	{
		/*
		 * First time spilling.
		 */
		if (hashtable->hjstate->js.ps.instrument)
		{
			hashtable->hjstate->js.ps.instrument->workfileCreated = true;
		}

		MemoryContext oldcxt;

		oldcxt = MemoryContextSwitchTo(bfCxt);
		hashtable->work_set = workfile_mgr_create_set("HashJoin", NULL, true /* hold pin */);
		MemoryContextSwitchTo(oldcxt);
	}

	if (file == NULL)
	{
		MemoryContext oldcxt;

		oldcxt = MemoryContextSwitchTo(bfCxt);

		/* First write to this batch file, so create it */
		Assert(hashtable->work_set != NULL);
		file = BufFileCreateTempInSet("HashJoin", false /* interXact */,
									  hashtable->work_set);
		BufFilePledgeSequential(file);	/* allow compression */
		*fileptr = file;

		elog(gp_workfile_caching_loglevel, "create batch file %s",
			 BufFileGetFilename(file));

		MemoryContextSwitchTo(oldcxt);
	}

	written = BufFileWrite(file, (void *) &hashvalue, sizeof(uint32));
	if (written != sizeof(uint32))
	{
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not write to temporary file: %m")));
	}

	written = BufFileWrite(file, (void *) tuple, tuple->t_len);
	if (written != tuple->t_len)
	{
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not write to temporary file: %m")));
	}
}

/*
 * ExecHashJoinGetSavedTuple
 *		read the next tuple from a batch file.  Return NULL if no more.
 *
 * On success, *hashvalue is set to the tuple's hash value, and the tuple
 * itself is stored in the given slot.
 */
static TupleTableSlot *
ExecHashJoinGetSavedTuple(HashJoinState *hjstate,
						  BufFile *file,
						  uint32 *hashvalue,
						  TupleTableSlot *tupleSlot)
{
	uint32		header[2];
	size_t		nread;
	MinimalTuple tuple;

	/*
	 * We check for interrupts here because this is typically taken as an
	 * alternative code path to an ExecProcNode() call, which would include
	 * such a check.
	 */
	CHECK_FOR_INTERRUPTS();

	/*
	 * Since both the hash value and the MinimalTuple length word are uint32,
	 * we can read them both in one BufFileRead() call without any type
	 * cheating.
	 */
	nread = BufFileRead(file, (void *) header, sizeof(header));
	if (nread != sizeof(header))				/* end of file */
	{
		ExecClearTuple(tupleSlot);
		return NULL;
	}

	*hashvalue = header[0];
	tuple = (MinimalTuple) palloc(header[1]);
	tuple->t_len = header[1];
	nread = BufFileRead(file,
						(void *) ((char *) tuple + sizeof(uint32)),
						header[1] - sizeof(uint32));
	if (nread != header[1] - sizeof(uint32))
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not read from hash-join temporary file: %m")));
	ExecForceStoreMinimalTuple(tuple, tupleSlot, true);
	return tupleSlot;
}


void
ExecReScanHashJoin(HashJoinState *node)
{
	/*
	 * In a multi-batch join, we currently have to do rescans the hard way,
	 * primarily because batch temp files may have already been released. But
	 * if it's a single-batch join, and there is no parameter change for the
	 * inner subnode, then we can just re-use the existing hash table without
	 * rebuilding it.
	 */
	if (node->hj_HashTable != NULL)
	{
		node->hj_HashTable->first_pass = false;

		if (node->js.ps.righttree->chgParam == NULL &&
			!node->hj_HashTable->eagerlyReleased)
		{
			/*
			 * Okay to reuse the hash table; needn't rescan inner, either.
			 *
			 * However, if it's a right/full join, we'd better reset the
			 * inner-tuple match flags contained in the table.
			 */
			if (HJ_FILL_INNER(node))
				ExecHashTableResetMatchFlags(node->hj_HashTable);

			/*
			 * Also, we need to reset our state about the emptiness of the
			 * outer relation, so that the new scan of the outer will update
			 * it correctly if it turns out to be empty this time. (There's no
			 * harm in clearing it now because ExecHashJoin won't need the
			 * info.  In the other cases, where the hash table doesn't exist
			 * or we are destroying it, we leave this state alone because
			 * ExecHashJoin will need it the first time through.)
			 */
			node->hj_OuterNotEmpty = false;

			/* ExecHashJoin can skip the BUILD_HASHTABLE step */
			node->hj_JoinState = HJ_NEED_NEW_OUTER;

			if (node->hj_HashTable->nbatch > 1)
			{
				/* Force reloading batch 0 upon next ExecHashJoin */
				node->hj_HashTable->curbatch = -1;
			}
			else
			{
				/* MPP-1600: reset the batch number */
				node->hj_HashTable->curbatch = 0;
			}
		}
		else
		{
			/* must destroy and rebuild hash table */
			if (!node->hj_HashTable->eagerlyReleased)
			{
				HashState  *hashState = (HashState *) innerPlanState(node);

				ExecHashTableDestroy(hashState, node->hj_HashTable);
			}
			pfree(node->hj_HashTable);
			node->hj_HashTable = NULL;
			node->hj_JoinState = HJ_BUILD_HASHTABLE;

			/*
			 * if chgParam of subnode is not null then plan will be re-scanned
			 * by first ExecProcNode.
			 */
			if (node->js.ps.righttree->chgParam == NULL)
				ExecReScan(node->js.ps.righttree);
		}
	}

	/* Always reset intra-tuple state */
	node->hj_CurHashValue = 0;
	node->hj_CurBucketNo = 0;
	node->hj_CurSkewBucketNo = INVALID_SKEW_BUCKET_NO;
	node->hj_CurTuple = NULL;

	node->hj_MatchedOuter = false;
	node->hj_FirstOuterTupleSlot = NULL;

	/*
	 * if chgParam of subnode is not null then plan will be re-scanned by
	 * first ExecProcNode.
	 */
	if (node->js.ps.lefttree->chgParam == NULL)
		ExecReScan(node->js.ps.lefttree);
}

/**
 * This method releases the hash table's memory. It maintains some of the other
 * aspects of the hash table like memory usage statistics. These may be required
 * during an explain analyze. A hash table that has been released cannot perform
 * any useful function anymore.
 */
static void
ReleaseHashTable(HashJoinState *node)
{
	if (node->hj_HashTable)
	{
		HashState *hashState = (HashState *) innerPlanState(node);

		/* This hashtable should not have been released already! */
		Assert(!node->hj_HashTable->eagerlyReleased);
		if (node->hj_HashTable->stats)
		{
			/* Report on batch in progress. */
			ExecHashTableExplainBatchEnd(hashState, node->hj_HashTable);
		}
		ExecHashTableDestroy(hashState, node->hj_HashTable);
		node->hj_HashTable->eagerlyReleased = true;
	}

	/* Always reset intra-tuple state */
	node->hj_CurHashValue = 0;
	node->hj_CurBucketNo = 0;
	node->hj_CurTuple = NULL;

	node->hj_JoinState = HJ_NEED_NEW_OUTER;
	node->hj_MatchedOuter = false;
	node->hj_FirstOuterTupleSlot = NULL;

}

/* Is this an IS-NOT-DISTINCT-join qual list (as opposed the an equijoin)?
 *
 * XXX We perform an abbreviated test based on the assumptions that 
 *     these are the only possibilities and that all conjuncts are 
 *     alike in this regard.
 */
bool
isNotDistinctJoin(List *qualList)
{
	ListCell   *lc;

	foreach(lc, qualList)
	{
		BoolExpr   *bex = (BoolExpr *) lfirst(lc);
		DistinctExpr *dex;

		if (IsA(bex, BoolExpr) &&bex->boolop == NOT_EXPR)
		{
			dex = (DistinctExpr *) linitial(bex->args);

			if (IsA(dex, DistinctExpr))
				return true;	/* We assume the rest follow suit! */
		}
	}
	return false;
}

static void
ExecEagerFreeHashJoin(HashJoinState *node)
{
	if (node->hj_HashTable != NULL && !node->hj_HashTable->eagerlyReleased)
	{
		ReleaseHashTable(node);
	}
}

void
ExecSquelchHashJoin(HashJoinState *node)
{
	ExecEagerFreeHashJoin(node);
	ExecSquelchNode(outerPlanState(node));
	ExecSquelchNode(innerPlanState(node));
}


/*
 * In our hybrid hash join we either spill when we increase number of batches
 * or when we re-spill. As we go, we normally destroy the batch file of the
 * batch that we have already processed. But if we need to support re-scanning
 * of the outer tuples, without also re-scanning the inner side, we need to
 * save the current hash for the next re-scan, instead.
 */
static void
SpillCurrentBatch(HashJoinState *node)
{
	HashJoinTable hashtable = node->hj_HashTable;
	int			curbatch = hashtable->curbatch;
	HashJoinTuple tuple;
	int			i;

	Assert(hashtable->innerBatchFile[curbatch] == NULL);

	for (i = 0; i < hashtable->nbuckets; i++)
	{
		/* GPDB_12_MERGE_FIXME: this only looks at 'unshared'. I think this is
		 * broken for parallel hashjoins
		 */
		tuple = hashtable->buckets.unshared[i];

		while (tuple != NULL)
		{
			ExecHashJoinSaveTuple(NULL, HJTUPLE_MINTUPLE(tuple),
								  tuple->hashvalue,
								  hashtable,
								  &hashtable->innerBatchFile[curbatch],
								  hashtable->bfCxt);
			tuple = tuple->next.unshared;
		}
	}
}

static bool
ExecHashJoinReloadHashTable(HashJoinState *hjstate)
{
	HashState  *hashState = (HashState *) innerPlanState(hjstate);
	HashJoinTable hashtable = hjstate->hj_HashTable;
	TupleTableSlot *slot;
	uint32		hashvalue;
	int			curbatch = hashtable->curbatch;
	int			nmoved = 0;
#if 0
	int			orignbatch = hashtable->nbatch;
#endif

	/*
	 * Reload the hash table with the new inner batch (which could be empty)
	 */
	ExecHashTableReset(hashState, hashtable);

	if (hashtable->innerBatchFile[curbatch] != NULL)
	{
		/* Rewind batch file */
		if (BufFileSeek(hashtable->innerBatchFile[curbatch], 0, 0, SEEK_SET) != 0)
		{
			ereport(ERROR, (errcode_for_file_access(),
							errmsg("could not access temporary file")));
		}

		for (;;)
		{
			CHECK_FOR_INTERRUPTS();

			if (QueryFinishPending)
				return false;

			slot = ExecHashJoinGetSavedTuple(hjstate,
											 hashtable->innerBatchFile[curbatch],
											 &hashvalue,
											 hjstate->hj_HashTupleSlot);
			if (!slot)
				break;

			/*
			 * NOTE: some tuples may be sent to future batches.  Also, it is
			 * possible for hashtable->nbatch to be increased here!
			 */
			if (!ExecHashTableInsert(hashState, hashtable, slot, hashvalue))
				nmoved++;
		}

		/*
		 * after we build the hash table, the inner batch file is no longer
		 * needed
		 */
		if (hjstate->js.ps.instrument && hjstate->js.ps.instrument->need_cdb)
		{
			Assert(hashtable->stats);
			hashtable->stats->batchstats[curbatch].innerfilesize =
				BufFileSize(hashtable->innerBatchFile[curbatch]);
		}

		SIMPLE_FAULT_INJECTOR("workfile_hashjoin_failure");

		/*
		 * If we want to re-use the hash table after a re-scan, don't
		 * delete it yet. But if we did not load the batch file into memory as is,
		 * because some tuples were sent to later batches, then delete it now, so
		 * that it will be recreated with just the remaining tuples, after processing
		 * this batch.
		 *
		 * XXX: Currently, we actually always close the file, and recreate it
		 * afterwards, even if there are no changes. That's because the workfile
		 * API doesn't support appending to a file that's already been read from.
		 * FIXME: could fix that now
		 */
#if 0
		if (!hjstate->reuse_hashtable || nmoved > 0 || hashtable->nbatch != orignbatch)
#endif
		{
			BufFileClose(hashtable->innerBatchFile[curbatch]);
			hashtable->innerBatchFile[curbatch] = NULL;
		}
	}

	return true;
}

void
ExecShutdownHashJoin(HashJoinState *node)
{
	if (node->hj_HashTable)
	{
		/*
		 * Detach from shared state before DSM memory goes away.  This makes
		 * sure that we don't have any pointers into DSM memory by the time
		 * ExecEndHashJoin runs.
		 */
		ExecHashTableDetachBatch(node->hj_HashTable);
		ExecHashTableDetach(node->hj_HashTable);
	}
}

static void
ExecParallelHashJoinPartitionOuter(HashJoinState *hjstate)
{
	PlanState  *outerState = outerPlanState(hjstate);
	ExprContext *econtext = hjstate->js.ps.ps_ExprContext;
	HashJoinTable hashtable = hjstate->hj_HashTable;
	TupleTableSlot *slot;
	uint32		hashvalue;
	int			i;
	HashState  *hashState = (HashState *) innerPlanState(hjstate);

	Assert(hjstate->hj_FirstOuterTupleSlot == NULL);

	/* Execute outer plan, writing all tuples to shared tuplestores. */
	for (;;)
	{
		slot = ExecProcNode(outerState);
		if (TupIsNull(slot))
			break;
		econtext->ecxt_outertuple = slot;

		bool		hashkeys_null = false;
		bool		keep_nulls = HJ_FILL_OUTER(hjstate) ||
			hjstate->hj_nonequijoin;
		if (ExecHashGetHashValue(hashState, hashtable, econtext,
								 hjstate->hj_OuterHashKeys,
								 true,	/* outer tuple */
								 keep_nulls,
								 &hashvalue,
								 &hashkeys_null))
		{
			int			batchno;
			int			bucketno;
			bool		shouldFree;
			MinimalTuple mintup = ExecFetchSlotMinimalTuple(slot, &shouldFree);

			ExecHashGetBucketAndBatch(hashtable, hashvalue, &bucketno,
									  &batchno);
			sts_puttuple(hashtable->batches[batchno].outer_tuples,
						 &hashvalue, mintup);

			if (shouldFree)
				heap_free_minimal_tuple(mintup);
		}
		CHECK_FOR_INTERRUPTS();
	}

	/* Make sure all outer partitions are readable by any backend. */
	for (i = 0; i < hashtable->nbatch; ++i)
		sts_end_write(hashtable->batches[i].outer_tuples);
}

void
ExecHashJoinEstimate(HashJoinState *state, ParallelContext *pcxt)
{
	shm_toc_estimate_chunk(&pcxt->estimator, sizeof(ParallelHashJoinState));
	shm_toc_estimate_keys(&pcxt->estimator, 1);
}

void
ExecHashJoinInitializeDSM(HashJoinState *state, ParallelContext *pcxt)
{
	int			plan_node_id = state->js.ps.plan->plan_node_id;
	HashState  *hashNode;
	ParallelHashJoinState *pstate;

	/*
	 * Disable shared hash table mode if we failed to create a real DSM
	 * segment, because that means that we don't have a DSA area to work with.
	 */
	if (pcxt->seg == NULL)
		return;

	ExecSetExecProcNode(&state->js.ps, ExecParallelHashJoin);

	/*
	 * Set up the state needed to coordinate access to the shared hash
	 * table(s), using the plan node ID as the toc key.
	 */
	pstate = shm_toc_allocate(pcxt->toc, sizeof(ParallelHashJoinState));
	shm_toc_insert(pcxt->toc, plan_node_id, pstate);

	/*
	 * Set up the shared hash join state with no batches initially.
	 * ExecHashTableCreate() will prepare at least one later and set nbatch
	 * and space_allowed.
	 */
	pstate->nbatch = 0;
	pstate->space_allowed = 0;
	pstate->batches = InvalidDsaPointer;
	pstate->old_batches = InvalidDsaPointer;
	pstate->nbuckets = 0;
	pstate->growth = PHJ_GROWTH_OK;
	pstate->chunk_work_queue = InvalidDsaPointer;
	pg_atomic_init_u32(&pstate->distributor, 0);
	pstate->nparticipants = pcxt->nworkers + 1;
	pstate->total_tuples = 0;
	LWLockInitialize(&pstate->lock,
					 LWTRANCHE_PARALLEL_HASH_JOIN);
	BarrierInit(&pstate->build_barrier, 0);
	BarrierInit(&pstate->grow_batches_barrier, 0);
	BarrierInit(&pstate->grow_buckets_barrier, 0);

	/* Set up the space we'll use for shared temporary files. */
	SharedFileSetInit(&pstate->fileset, pcxt->seg);

	/* Initialize the shared state in the hash node. */
	hashNode = (HashState *) innerPlanState(state);
	hashNode->parallel_state = pstate;
}

/* ----------------------------------------------------------------
 *		ExecHashJoinReInitializeDSM
 *
 *		Reset shared state before beginning a fresh scan.
 * ----------------------------------------------------------------
 */
void
ExecHashJoinReInitializeDSM(HashJoinState *state, ParallelContext *cxt)
{
	int			plan_node_id = state->js.ps.plan->plan_node_id;
	ParallelHashJoinState *pstate =
	shm_toc_lookup(cxt->toc, plan_node_id, false);

	/*
	 * It would be possible to reuse the shared hash table in single-batch
	 * cases by resetting and then fast-forwarding build_barrier to
	 * PHJ_BUILD_DONE and batch 0's batch_barrier to PHJ_BATCH_PROBING, but
	 * currently shared hash tables are already freed by now (by the last
	 * participant to detach from the batch).  We could consider keeping it
	 * around for single-batch joins.  We'd also need to adjust
	 * finalize_plan() so that it doesn't record a dummy dependency for
	 * Parallel Hash nodes, preventing the rescan optimization.  For now we
	 * don't try.
	 */

	/* Detach, freeing any remaining shared memory. */
	if (state->hj_HashTable != NULL)
	{
		ExecHashTableDetachBatch(state->hj_HashTable);
		ExecHashTableDetach(state->hj_HashTable);
	}

	/* Clear any shared batch files. */
	SharedFileSetDeleteAll(&pstate->fileset);

	/* Reset build_barrier to PHJ_BUILD_ELECTING so we can go around again. */
	BarrierInit(&pstate->build_barrier, 0);
}

void
ExecHashJoinInitializeWorker(HashJoinState *state,
							 ParallelWorkerContext *pwcxt)
{
	HashState  *hashNode;
	int			plan_node_id = state->js.ps.plan->plan_node_id;
	ParallelHashJoinState *pstate =
	shm_toc_lookup(pwcxt->toc, plan_node_id, false);

	/* Attach to the space for shared temporary files. */
	SharedFileSetAttach(&pstate->fileset, pwcxt->seg);

	/* Attach to the shared state in the hash node. */
	hashNode = (HashState *) innerPlanState(state);
	hashNode->parallel_state = pstate;

	ExecSetExecProcNode(&state->js.ps, ExecParallelHashJoin);
}
