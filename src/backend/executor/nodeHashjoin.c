/*-------------------------------------------------------------------------
 *
 * nodeHashjoin.c
 *	  Routines to handle hash join nodes
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/executor/nodeHashjoin.c,v 1.94 2008/08/14 18:47:58 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "executor/executor.h"
#include "executor/hashjoin.h"
#include "executor/instrument.h"	/* Instrumentation */
#include "executor/nodeHash.h"
#include "executor/nodeHashjoin.h"
#include "utils/faultinjector.h"
#include "utils/memutils.h"

/* Returns true for JOIN_LEFT, JOIN_ANTI and JOIN_LASJ_NOTIN jointypes */
#define HASHJOIN_IS_OUTER(hjstate)  ((hjstate)->hj_NullInnerTupleSlot != NULL)

#include "cdb/cdbvars.h"
#include "miscadmin.h"			/* work_mem */

static TupleTableSlot *ExecHashJoinOuterGetTuple(PlanState *outerNode,
						  HashJoinState *hjstate,
						  uint32 *hashvalue);
static TupleTableSlot *ExecHashJoinGetSavedTuple(HashJoinState *hjstate,
						  HashJoinBatchSide *side,
						  uint32 *hashvalue,
						  TupleTableSlot *tupleSlot);
static int	ExecHashJoinNewBatch(HashJoinState *hjstate);
static bool isNotDistinctJoin(List *qualList);

static void ReleaseHashTable(HashJoinState *node);
static bool isHashtableEmpty(HashJoinTable hashtable);

static void SpillCurrentBatch(HashJoinState *node);
static bool ExecHashJoinReloadHashTable(HashJoinState *hjstate);

/* ----------------------------------------------------------------
 *		ExecHashJoin
 *
 *		This function implements the Hybrid Hashjoin algorithm.
 *
 *		Note: the relation we build hash table on is the "inner"
 *			  the other one is "outer".
 * ----------------------------------------------------------------
 */
TupleTableSlot *				/* return: a tuple or NULL */
ExecHashJoin(HashJoinState *node)
{
	EState	   *estate;
	PlanState  *outerNode;
	HashState  *hashNode;
	List	   *joinqual;
	List	   *otherqual;
	TupleTableSlot *inntuple;
	ExprContext *econtext;
	HashJoinTable hashtable;
	HashJoinTuple curtuple;
	TupleTableSlot *outerTupleSlot;
	uint32		hashvalue;
	int			batchno;

	/*
	 * get information from HashJoin node
	 */
	estate = node->js.ps.state;
	joinqual = node->js.joinqual;
	otherqual = node->js.ps.qual;
	hashNode = (HashState *) innerPlanState(node);
	outerNode = outerPlanState(node);

	/*
	 * get information from HashJoin state
	 */
	hashtable = node->hj_HashTable;
	econtext = node->js.ps.ps_ExprContext;

	/*
	 * Reset per-tuple memory context to free any expression evaluation
	 * storage allocated in the previous tuple cycle.  Note this can't happen
	 * until we're done projecting out tuples from a join tuple.
	 */
	ResetExprContext(econtext);

	/*
	 * if this is the first call, build the hash table for inner relation
	 */
	if (hashtable == NULL)
	{
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
			 * If the outer relation is completely empty, we can quit without
			 * building the hash table.  However, for an inner join it is only a
			 * win to check this when the outer relation's startup cost is less
			 * than the projected cost of building the hash table.	Otherwise it's
			 * best to build the hash table first and see if the inner relation is
			 * empty.  (When it's an outer join, we should always make this check,
			 * since we aren't going to be able to skip the join on the strength
			 * of an empty inner relation anyway.)
			 *
			 * If we are rescanning the join, we make use of information gained on
			 * the previous scan: don't bother to try the prefetch if the previous
			 * scan found the outer relation nonempty.	This is not 100% reliable
			 * since with new parameters the outer relation might yield different
			 * results, but it's a good heuristic.
			 *
			 * The only way to make the check is to try to fetch a tuple from the
			 * outer plan node.  If we succeed, we have to stash it away for later
			 * consumption by ExecHashJoinOuterGetTuple.
			 */
			if ((HASHJOIN_IS_OUTER(node)) ||
					(outerNode->plan->startup_cost < hashNode->ps.plan->total_cost &&
						!node->hj_OuterNotEmpty))
			{
				TupleTableSlot *slot;

				slot = ExecProcNode(outerNode);

				node->hj_FirstOuterTupleSlot = slot;
				if (TupIsNull(node->hj_FirstOuterTupleSlot))
				{
					node->hj_OuterNotEmpty = false;

					/* CDB: Tell inner subtree that its data will not be needed. */
					ExecSquelchNode((PlanState *)hashNode);

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
		 * create the hash table
		 */
		hashtable = ExecHashTableCreate(hashNode,
										node,
										node->hj_HashOperators,
										PlanStateOperatorMemKB((PlanState *) hashNode));
		node->hj_HashTable = hashtable;

		/*
		 * CDB: Offer extra info for EXPLAIN ANALYZE.
		 */
		if (estate->es_instrument && (estate->es_instrument & INSTRUMENT_CDB))
			ExecHashTableExplainInit(hashNode, node, hashtable);


		/*
		 * execute the Hash node, to build the hash table
		 */
		hashNode->hashtable = hashtable;

		/*
		 * Only if doing a LASJ_NOTIN join, we want to quit as soon as we find
		 * a NULL key on the inner side
		 */
		hashNode->hs_quit_if_hashkeys_null = (node->js.jointype == JOIN_LASJ_NOTIN);

		/*
		 * Store pointer to the HashJoinState in the hashtable, as we will
		 * need the HashJoin plan when creating the spill file set
		 */
		hashtable->hjstate = node;

		/* Execute the Hash node and build the hashtable */
		(void) MultiExecProcNode((PlanState *) hashNode);

#ifdef HJDEBUG
		elog(gp_workfile_caching_loglevel, "HashJoin built table with %.1f tuples by executing subplan for batch 0", hashtable->totalTuples);
#endif

		/**
		 * If LASJ_NOTIN and a null was found on the inner side, then clean out.
		 */
		if (node->js.jointype == JOIN_LASJ_NOTIN && hashNode->hs_hashkeys_null)
		{
			/*
			 * CDB: We'll read no more from outer subtree. To keep sibling QEs
			 * from being starved, tell source QEs not to clog up the pipeline
			 * with our never-to-be-consumed data.
			 */
			ExecSquelchNode(outerNode);
			/* end of join */

			ExecEagerFreeHashJoin(node);

			return NULL;
		}

		/*
		 * We just scanned the entire inner side and built the hashtable
		 * (and its overflow batches). Check here and remember if the inner
		 * side is empty.
		 */
		node->hj_InnerEmpty = isHashtableEmpty(hashtable);

		/*
		 * If the inner relation is completely empty, and we're not doing an
		 * outer join, we can quit without scanning the outer relation.
		 */
		if (!HASHJOIN_IS_OUTER(node)
			&& node->hj_InnerEmpty)
		{
			/*
			 * CDB: We'll read no more from outer subtree. To keep sibling QEs
			 * from being starved, tell source QEs not to clog up the pipeline
			 * with our never-to-be-consumed data.
			 */
			ExecSquelchNode(outerNode);
			/* end of join */

			ExecEagerFreeHashJoin(node);

			return NULL;
		}

		/*
		 * Reset OuterNotEmpty for scan.  (It's OK if we fetched a tuple
		 * above, because ExecHashJoinOuterGetTuple will immediately set it
		 * again.)
		 */
		node->hj_OuterNotEmpty = false;
	}

	/* For a rescannable hash table we might need to reload batch 0 during rescan */
	if (hashtable->curbatch == -1 && !hashtable->first_pass)
	{
		hashtable->curbatch = 0;
		if (!ExecHashJoinReloadHashTable(node))
		{
			return NULL;
		}
	}

	/*
	 * run the hash join process
	 */
	for (;;)
	{
		/* We must never use an eagerly released hash table */
		Assert(!hashtable->eagerlyReleased);

		/*
		 * If we don't have an outer tuple, get the next one
		 */
		if (node->hj_NeedNewOuter)
		{
			outerTupleSlot = ExecHashJoinOuterGetTuple(outerNode,
													   node,
													   &hashvalue);
			if (TupIsNull(outerTupleSlot))
			{
				/* end of join */
				if (!node->reuse_hashtable)
					ExecEagerFreeHashJoin(node);

				return NULL;
			}

			node->js.ps.ps_OuterTupleSlot = outerTupleSlot;
			econtext->ecxt_outertuple = outerTupleSlot;
			node->hj_NeedNewOuter = false;
			node->hj_MatchedOuter = false;

			/*
			 * now we have an outer tuple, find the corresponding bucket for
			 * this tuple from the hash table
			 */
			node->hj_CurHashValue = hashvalue;
			ExecHashGetBucketAndBatch(hashtable, hashvalue,
									  &node->hj_CurBucketNo, &batchno);
			node->hj_CurTuple = NULL;

			/*
			 * Now we've got an outer tuple and the corresponding hash bucket,
			 * but this tuple may not belong to the current batch.
			 */
			if (batchno != hashtable->curbatch)
			{
				/*
				 * Need to postpone this outer tuple to a later batch. Save it
				 * in the corresponding outer-batch file.
				 */
				Assert(batchno != 0);
				Assert(batchno > hashtable->curbatch);
				ExecHashJoinSaveTuple(&node->js.ps, ExecFetchSlotMemTuple(outerTupleSlot, false),
									  hashvalue,
									  hashtable,
									  &hashtable->batches[batchno]->outerside,
									  hashtable->bfCxt);
				node->hj_NeedNewOuter = true;
				continue;		/* loop around for a new outer tuple */
			}
		}

		/*
		 * OK, scan the selected hash bucket for matches
		 */
		for (;;)
		{
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
				break;		/* loop around for a new outer tuple */
			}

			curtuple = ExecScanHashBucket(hashNode, node, econtext);
			if (curtuple == NULL)
				break;			/* out of matches */

			/*
			 * we've got a match, but still need to test non-hashed quals
			 */
			inntuple = ExecStoreMinimalTuple(HJTUPLE_MINTUPLE(curtuple),
											 node->hj_HashTupleSlot,
											 false);	/* don't pfree */
			econtext->ecxt_innertuple = inntuple;

			/* reset temp memory each time to avoid leaks from qual expr */
			ResetExprContext(econtext);

			/*
			 * if we pass the qual, then save state for next call and have
			 * ExecProject form the projection, store it in the tuple table,
			 * and return the slot.
			 *
			 * Only the joinquals determine MatchedOuter status, but all quals
			 * must pass to actually return the tuple.
			 */
			if (joinqual == NIL || ExecQual(joinqual, econtext, false))
			{
				node->hj_MatchedOuter = true;

				/* In an antijoin, we never return a matched tuple */
				if (node->js.jointype == JOIN_LASJ_NOTIN || node->js.jointype == JOIN_ANTI)
				{
					node->hj_NeedNewOuter = true;
					break;		/* out of loop over hash bucket */
				}
				else
				{
					/*
					 * In a semijoin, we'll consider returning the first match,
					 * but after that we're done with this outer tuple.
					 */
					if (node->js.jointype == JOIN_SEMI)
						node->hj_NeedNewOuter = true;

					if (otherqual == NIL || ExecQual(otherqual, econtext, false))
						return ExecProject(node->js.ps.ps_ProjInfo, NULL);

					/*
					 * If semijoin and we didn't return the tuple, we're still
					 * done with this outer tuple.
					 */
					if (node->js.jointype == JOIN_SEMI)
						break;		/* out of loop over hash bucket */
				}
			}
		}

		/*
		 * Now the current outer tuple has run out of matches, so check
		 * whether to emit a dummy outer-join tuple. If not, loop around to
		 * get a new outer tuple.
		 */
		node->hj_NeedNewOuter = true;

		if (!node->hj_MatchedOuter &&
			(HASHJOIN_IS_OUTER(node)))
		{
			/*
			 * We are doing an outer join and there were no join matches for
			 * this outer tuple.  Generate a fake join tuple with nulls for
			 * the inner tuple, and return it if it passes the non-join quals.
			 */
			econtext->ecxt_innertuple = node->hj_NullInnerTupleSlot;

			if (otherqual == NIL || ExecQual(otherqual, econtext, false))
				return ExecProject(node->js.ps.ps_ProjInfo, NULL);
		}
	}
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
	List	   *hoperators;
	ListCell   *l;

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
	 * Miscellaneous initialization
	 *
	 * create expression context for node
	 */
	ExecAssignExprContext(estate, &hjstate->js.ps);

	if (node->hashqualclauses != NIL)
	{
		/* CDB: This must be an IS NOT DISTINCT join!  */
		Insist(isNotDistinctJoin(node->hashqualclauses));
		hjstate->hj_nonequijoin = true;
	}
	else
		hjstate->hj_nonequijoin = false;

	/*
	 * initialize child expressions
	 */
	hjstate->js.ps.targetlist = (List *)
		ExecInitExpr((Expr *) node->join.plan.targetlist,
					 (PlanState *) hjstate);
	hjstate->js.ps.qual = (List *)
		ExecInitExpr((Expr *) node->join.plan.qual,
					 (PlanState *) hjstate);
	hjstate->js.jointype = node->join.jointype;
	hjstate->js.joinqual = (List *)
		ExecInitExpr((Expr *) node->join.joinqual,
					 (PlanState *) hjstate);
	hjstate->hashclauses = (List *)
		ExecInitExpr((Expr *) node->hashclauses,
					 (PlanState *) hjstate);

	if (node->hashqualclauses != NIL)
	{
		hjstate->hashqualclauses = (List *)
			ExecInitExpr((Expr *) node->hashqualclauses,
						 (PlanState *) hjstate);
	}
	else
	{
		hjstate->hashqualclauses = hjstate->hashclauses;
	}

	/*
	 * MPP-3300, we only pre-build hashtable if we need to (this is relaxing
	 * the fix to MPP-989)
	 */
	hjstate->prefetch_inner = node->join.prefetch_inner;

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
	((HashState *) innerPlanState(hjstate))->hs_keepnull = hjstate->hj_nonequijoin;

	outerPlanState(hjstate) = ExecInitNode(outerNode, estate, eflags);

#define HASHJOIN_NSLOTS 3

	/*
	 * tuple table initialization
	 */
	ExecInitResultTupleSlot(estate, &hjstate->js.ps);
	hjstate->hj_OuterTupleSlot = ExecInitExtraTupleSlot(estate);

	switch (node->join.jointype)
	{
		case JOIN_INNER:
		case JOIN_SEMI:
			break;
		case JOIN_LEFT:
		case JOIN_ANTI:
		case JOIN_LASJ_NOTIN:
			hjstate->hj_NullInnerTupleSlot =
				ExecInitNullTupleSlot(estate,
								 ExecGetResultType(innerPlanState(hjstate)));
			break;
		default:
			elog(ERROR, "unrecognized join type: %d",
				 (int) node->join.jointype);
	}

	/*
	 * now for some voodoo.  our temporary tuple slot is actually the result
	 * tuple slot of the Hash node (which is our inner plan).  we do this
	 * because Hash nodes don't return tuples via ExecProcNode() -- instead
	 * the hash join node uses ExecScanHashBucket() to get at the contents of
	 * the hash table.	-cim 6/9/91
	 */
	{
		HashState  *hashstate = (HashState *) innerPlanState(hjstate);
		TupleTableSlot *slot = hashstate->ps.ps_ResultTupleSlot;

		hjstate->hj_HashTupleSlot = slot;
	}

	/*
	 * initialize tuple type and projection info
	 */
	ExecAssignResultTypeFromTL(&hjstate->js.ps);
	ExecAssignProjectionInfo(&hjstate->js.ps, NULL);

	ExecSetSlotDescriptor(hjstate->hj_OuterTupleSlot,
						  ExecGetResultType(outerPlanState(hjstate)));

	/*
	 * initialize hash-specific info
	 */
	hjstate->hj_HashTable = NULL;
	hjstate->hj_FirstOuterTupleSlot = NULL;

	hjstate->hj_CurHashValue = 0;
	hjstate->hj_CurBucketNo = 0;
	hjstate->hj_CurTuple = NULL;

	/*
	 * Deconstruct the hash clauses into outer and inner argument values, so
	 * that we can evaluate those subexpressions separately.  Also make a list
	 * of the hash operator OIDs, in preparation for looking up the hash
	 * functions to use.
	 */
	lclauses = NIL;
	rclauses = NIL;
	hoperators = NIL;
	foreach(l, hjstate->hashclauses)
	{
		FuncExprState *fstate = (FuncExprState *) lfirst(l);
		OpExpr	   *hclause;

		Assert(IsA(fstate, FuncExprState));
		hclause = (OpExpr *) fstate->xprstate.expr;
		Assert(IsA(hclause, OpExpr));
		lclauses = lappend(lclauses, linitial(fstate->args));
		rclauses = lappend(rclauses, lsecond(fstate->args));
		hoperators = lappend_oid(hoperators, hclause->opno);
	}
	hjstate->hj_OuterHashKeys = lclauses;
	hjstate->hj_InnerHashKeys = rclauses;
	hjstate->hj_HashOperators = hoperators;
	/* child Hash node needs to evaluate inner hash keys, too */
	((HashState *) innerPlanState(hjstate))->hashkeys = rclauses;

	hjstate->js.ps.ps_OuterTupleSlot = NULL;
	hjstate->hj_NeedNewOuter = true;
	hjstate->hj_MatchedOuter = false;
	hjstate->hj_OuterNotEmpty = false;

	return hjstate;
}

int
ExecCountSlotsHashJoin(HashJoin *node)
{
	return ExecCountSlotsNode(outerPlan(node)) +
		ExecCountSlotsNode(innerPlan(node)) +
		HASHJOIN_NSLOTS;
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

	EndPlanStateGpmonPkt(&node->js.ps);
}

/*
 * ExecHashJoinOuterGetTuple
 *
 *		get the next outer tuple for hashjoin: either by
 *		executing a plan node in the first pass, or from
 *		the temp files for the hashjoin batches.
 *
 * Returns a null slot if no more outer tuples.  On success, the tuple's
 * hash value is stored at *hashvalue --- this is either originally computed,
 * or re-read from the temp file.
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
		for (;;)
		{
			/*
			 * Check to see if first outer tuple was already fetched by
			 * ExecHashJoin() and not used yet.
			 */
			slot = hjstate->hj_FirstOuterTupleSlot;
			if (!TupIsNull(slot))
				hjstate->hj_FirstOuterTupleSlot = NULL;
			else
			{
				slot = ExecProcNode(outerNode);
			}

			if (TupIsNull(slot))
				break;

			/*
			 * We have to compute the tuple's hash value.
			 */
			econtext = hjstate->js.ps.ps_ExprContext;
			econtext->ecxt_outertuple = slot;

			bool hashkeys_null = false;
			bool keep_nulls = (HASHJOIN_IS_OUTER(hjstate))||
					hjstate->hj_nonequijoin;
			if (ExecHashGetHashValue(hashState, hashtable, econtext,
									 hjstate->hj_OuterHashKeys,
									 true,		/* outer tuple */
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
		}

		/*
		 * We have just reached the end of the first pass. Try to switch to a
		 * saved batch.
		 */

		/* SFR: This can cause re-spill! */
		curbatch = ExecHashJoinNewBatch(hjstate);


#ifdef HJDEBUG
		elog(gp_workfile_caching_loglevel, "HashJoin built table with %.1f tuples for batch %d", hashtable->totalTuples, curbatch);
#endif

	} /* if (curbatch == 0) */

	/*
	 * Try to read from a temp file. Loop allows us to advance to new batches
	 * as needed.  NOTE: nbatch could increase inside ExecHashJoinNewBatch, so
	 * don't try to optimize this loop.
	 */
	while (curbatch < hashtable->nbatch)
	{
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
										 &hashtable->batches[curbatch]->outerside,
										 hashvalue,
										 hjstate->hj_OuterTupleSlot);
		if (!TupIsNull(slot))
			return slot;
		curbatch = ExecHashJoinNewBatch(hjstate);

#ifdef HJDEBUG
		elog(gp_workfile_caching_loglevel, "HashJoin built table with %.1f tuples for batch %d", hashtable->totalTuples, curbatch);
#endif

		CheckSendPlanStateGpmonPkt(&hjstate->js.ps);
	}

	/* Out of batches... */
	return NULL;
}

/*
 * ExecHashJoinNewBatch
 *		switch to a new hashjoin batch
 *
 * Returns the number of the new batch (1..nbatch-1), or nbatch if no more.
 * We will never return a batch number that has an empty outer batch file.
 */
static int
ExecHashJoinNewBatch(HashJoinState *hjstate)
{
	HashJoinTable hashtable = hjstate->hj_HashTable;
	HashJoinBatchData *batch;
	int			nbatch;
	int			curbatch;

	SIMPLE_FAULT_INJECTOR(FaultExecHashJoinNewBatch);

	HashState  *hashState = (HashState *) innerPlanState(hjstate);

start_over:
	nbatch = hashtable->nbatch;
	curbatch = hashtable->curbatch;

	if (curbatch >= nbatch)
		return nbatch;

	if (curbatch >= 0 && hashtable->stats)
		ExecHashTableExplainBatchEnd(hashState, hashtable);

	if (curbatch > 0)
	{
		/*
		 * We no longer need the previous outer batch file; close it right
		 * away to free disk space.
		 */
		batch = hashtable->batches[curbatch];
		if (batch->outerside.workfile != NULL)
		{
			workfile_mgr_close_file(hashtable->work_set, batch->outerside.workfile);
		}
		batch->outerside.workfile = NULL;
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
		hashtable->batches[curbatch]->innerside.workfile == NULL)
	{
		SpillCurrentBatch(hjstate);
	}

	/*
	 * We can always skip over any batches that are completely empty on both
	 * sides.  We can sometimes skip over batches that are empty on only one
	 * side, but there are exceptions:
	 *
	 * 1. In an outer join, we have to process outer batches even if the inner
	 * batch is empty.
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
		   (hashtable->batches[curbatch]->outerside.workfile == NULL ||
			hashtable->batches[curbatch]->innerside.workfile == NULL))

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

		batch = hashtable->batches[curbatch];
		if (batch->outerside.workfile != NULL &&
			HASHJOIN_IS_OUTER(hjstate))
			break;				/* must process due to rule 1 */
		if (batch->innerside.workfile != NULL &&
			nbatch != hashtable->nbatch_original)
			break;				/* must process due to rule 2 */
		if (batch->outerside.workfile != NULL &&
			nbatch != hashtable->nbatch_outstart)
			break;				/* must process due to rule 3 */
		/* We can ignore this batch. */
		/* Release associated temp files right away. */
		if (batch->innerside.workfile != NULL && !hjstate->reuse_hashtable)
		{
			workfile_mgr_close_file(hashtable->work_set, batch->innerside.workfile);
			batch->innerside.workfile = NULL;
		}

		if (batch->outerside.workfile != NULL)
		{
			workfile_mgr_close_file(hashtable->work_set, batch->outerside.workfile);
		}
		batch->outerside.workfile = NULL;

		curbatch++;
	}

	hashtable->curbatch = curbatch;		/* CDB: upd before return, even if no
										 * more data, so stats logic can see
										 * whether join was run to completion */

	if (curbatch >= nbatch)
		return curbatch;		/* no more batches */

	batch = hashtable->batches[curbatch];

	if (!ExecHashJoinReloadHashTable(hjstate))
	{
		/* We no longer continue as we couldn't load the batch */
		return nbatch;
	}
	/*
	 * If there's no outer batch file, advance to next batch.
	 */
	if (batch->outerside.workfile == NULL)
		goto start_over;

	/*
	 * Rewind outer batch file, so that we can start reading it.
	 */
	bool		result = ExecWorkFile_Rewind(batch->outerside.workfile);

	if (!result)
	{
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not access temporary file")));
	}

	return curbatch;
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
ExecHashJoinSaveTuple(PlanState *ps, MemTuple tuple, uint32 hashvalue,
					  HashJoinTable hashtable, struct HashJoinBatchSide *batchside,
					  MemoryContext bfCxt)
{
	if (hashtable->work_set == NULL)
	{
		hashtable->hjstate->workfiles_created = true;
		if (hashtable->hjstate->js.ps.instrument)
		{
			hashtable->hjstate->js.ps.instrument->workfileCreated = true;
		}

		MemoryContext oldcxt;

		oldcxt = MemoryContextSwitchTo(bfCxt);

		hashtable->work_set = workfile_mgr_create_set(gp_workfile_type_hashjoin,
				true, /* can_be_reused */
				&hashtable->hjstate->js.ps);

		/*
		 * First time spilling. Before creating any spill files, create a
		 * metadata file
		 */
		hashtable->state_file = workfile_mgr_create_fileno(hashtable->work_set, WORKFILE_NUM_HASHJOIN_METADATA);
		elog(gp_workfile_caching_loglevel, "created state file %s", ExecWorkFile_GetFileName(hashtable->state_file));

		MemoryContextSwitchTo(oldcxt);
	}

	if (batchside->workfile == NULL)
	{
		MemoryContext oldcxt;

		oldcxt = MemoryContextSwitchTo(bfCxt);

		/* First write to this batch file, so create it */
		Assert(hashtable->work_set != NULL);
		batchside->workfile = workfile_mgr_create_file(hashtable->work_set);

		elog(gp_workfile_caching_loglevel, "create batch file %s with gp_workfile_compress_algorithm=%d",
			 ExecWorkFile_GetFileName(batchside->workfile),
			 hashtable->work_set->metadata.bfz_compress_type);

		MemoryContextSwitchTo(oldcxt);
	}

	if (!ExecWorkFile_Write(batchside->workfile, (void *) &hashvalue, sizeof(uint32)))
	{
		workfile_mgr_report_error();
	}

	if (!ExecWorkFile_Write(batchside->workfile, (void *) tuple, memtuple_get_size(tuple)))
	{
		workfile_mgr_report_error();
	}

	batchside->total_tuples++;

	if (ps)
	{
		CheckSendPlanStateGpmonPkt(ps);
	}
}

/*
 * ExecHashJoinGetSavedTuple
 *		read the next tuple from a batch file.	Return NULL if no more.
 *
 * On success, *hashvalue is set to the tuple's hash value, and the tuple
 * itself is stored in the given slot.
 */
static TupleTableSlot *
ExecHashJoinGetSavedTuple(HashJoinState *hjstate,
						  HashJoinBatchSide *batchside,
						  uint32 *hashvalue,
						  TupleTableSlot *tupleSlot)
{
	uint32		header[2];
	size_t		nread;
	MemTuple	tuple;

	/*
	 * Since both the hash value and the MinimalTuple length word are uint32,
	 * we can read them both in one BufFileRead() call without any type
	 * cheating.
	 */
	nread = ExecWorkFile_Read(batchside->workfile, (void *) header, sizeof(header));
	if (nread != sizeof(header))				/* end of file */
	{
		ExecClearTuple(tupleSlot);
		return NULL;
	}

	*hashvalue = header[0];
	tuple = (MemTuple) palloc(memtuple_size_from_uint32(header[1]));
	memtuple_set_mtlen(tuple, header[1]);

	nread = ExecWorkFile_Read(batchside->workfile,
							  (void *) ((char *) tuple + sizeof(uint32)),
							  memtuple_size_from_uint32(header[1]) - sizeof(uint32));
	
	if (nread != memtuple_size_from_uint32(header[1]) - sizeof(uint32))
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not read from hash-join temporary file")));
	return ExecStoreMinimalTuple(tuple, tupleSlot, true);
}


void
ExecReScanHashJoin(HashJoinState *node, ExprContext *exprCtxt)
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

		if (((PlanState *) node)->righttree->chgParam == NULL
			&& !node->hj_HashTable->eagerlyReleased)
		{
			/*
			 * okay to reuse the hash table; needn't rescan inner, either.
			 *
			 * What we do need to do is reset our state about the emptiness of
			 * the outer relation, so that the new scan of the outer will
			 * update it correctly if it turns out to be empty this time.
			 * (There's no harm in clearing it now because ExecHashJoin won't
			 * need the info.  In the other cases, where the hash table
			 * doesn't exist or we are destroying it, we leave this state
			 * alone because ExecHashJoin will need it the first time
			 * through.)
			 */
			node->hj_OuterNotEmpty = false;

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

			/*
			 * if chgParam of subnode is not null then plan will be re-scanned
			 * by first ExecProcNode.
			 */
			if (((PlanState *) node)->righttree->chgParam == NULL)
				ExecReScan(((PlanState *) node)->righttree, exprCtxt);
		}
	}

	/* Always reset intra-tuple state */
	node->hj_CurHashValue = 0;
	node->hj_CurBucketNo = 0;
	node->hj_CurTuple = NULL;

	node->js.ps.ps_OuterTupleSlot = NULL;
	node->hj_NeedNewOuter = true;
	node->hj_MatchedOuter = false;
	node->hj_FirstOuterTupleSlot = NULL;

	/*
	 * if chgParam of subnode is not null then plan will be re-scanned by
	 * first ExecProcNode.
	 */
	if (((PlanState *) node)->lefttree->chgParam == NULL)
		ExecReScan(((PlanState *) node)->lefttree, exprCtxt);
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

	node->js.ps.ps_OuterTupleSlot = NULL;
	node->hj_NeedNewOuter = true;
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

void
ExecEagerFreeHashJoin(HashJoinState *node)
{
	if (node->hj_HashTable != NULL && !node->hj_HashTable->eagerlyReleased)
	{
		ReleaseHashTable(node);
	}
}

/*
 * isHashtableEmpty
 *
 *	After populating the hashtable with all the tuples from the innerside,
 *	scan all the batches and return true if the hashtable is completely empty
 *
 */
static bool
isHashtableEmpty(HashJoinTable hashtable)
{
	int			i;
	bool		isEmpty = true;

	/* Is there a nonempty batch? */
	for (i = 0; i < hashtable->nbatch; i++)
	{
		/*
		 * For batch 0, the number of inner tuples is stored in
		 * batches[i].innertuples. For batches on disk (1 and above), the
		 * batches[i].innertuples is 0, but batches[i].innerside.workfile is
		 * non-NULL if any tuples were written to disk. Check both here.
		 */
		if ((hashtable->batches[i]->innertuples > 0) ||
			(NULL != hashtable->batches[i]->innerside.workfile))
		{
			/* Found a non-empty batch, stop the search */
			isEmpty = false;
			break;
		}
	}

	return isEmpty;
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
	HashJoinBatchData *fullbatch = hashtable->batches[hashtable->curbatch];
	HashJoinTuple tuple;
	int			i;

	Assert(fullbatch->innerside.workfile == NULL);

	for (i = 0; i < hashtable->nbuckets; i++)
	{
		tuple = hashtable->buckets[i];

		while (tuple != NULL)
		{
			ExecHashJoinSaveTuple(NULL, HJTUPLE_MINTUPLE(tuple),
								  tuple->hashvalue,
								  hashtable,
								  &fullbatch->innerside,
								  hashtable->bfCxt);
			tuple = tuple->next;
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
	int curbatch = hashtable->curbatch;
	HashJoinBatchData *batch = hashtable->batches[curbatch];
	int			nmoved = 0;
#if 0
	int			orignbatch = hashtable->nbatch;
#endif

	/*
	 * Reload the hash table with the new inner batch (which could be empty)
	 */
	ExecHashTableReset(hashState, hashtable);

	if (batch->innerside.workfile != NULL)
	{
		/* Rewind batch file */
		bool		result = ExecWorkFile_Rewind(batch->innerside.workfile);

		if (!result)
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
											 &batch->innerside,
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
				ExecWorkFile_Tell64(hashtable->batches[curbatch]->innerside.workfile);
		}

		SIMPLE_FAULT_INJECTOR(WorkfileHashJoinFailure);

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
		 */
#if 0
		if (!hjstate->reuse_hashtable || nmoved > 0 || hashtable->nbatch != orignbatch)
#endif
		{
			workfile_mgr_close_file(hashtable->work_set, batch->innerside.workfile);
			batch->innerside.workfile = NULL;
		}
	}

	return true;
}

/* EOF */
