/*-------------------------------------------------------------------------
 *
 * nodeHash.c
 *	  Routines to hash relations for hashjoin
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/executor/nodeHash.c,v 1.121 2009/06/11 14:48:57 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *		MultiExecHash	- generate an in-memory hash table of the relation
 *		ExecInitHash	- initialize node and subnodes
 *		ExecEndHash		- shutdown node and subnodes
 */

#include "postgres.h"

#include <math.h>
#include <limits.h>

#include "access/hash.h"
#include "catalog/pg_statistic.h"
#include "commands/tablespace.h"
#include "executor/execdebug.h"
#include "executor/hashjoin.h"
#include "executor/instrument.h"
#include "executor/nodeHash.h"
#include "executor/nodeHashjoin.h"
#include "miscadmin.h"
#include "parser/parse_expr.h"
#include "utils/dynahash.h"
#include "utils/memutils.h"
#include "utils/lsyscache.h"
#include "utils/faultinjector.h"
#include "utils/syscache.h"

#include "cdb/cdbexplain.h"
#include "cdb/cdbvars.h"

static void ExecHashIncreaseNumBatches(HashJoinTable hashtable);
static void ExecHashBuildSkewHash(HashJoinTable hashtable, Hash *node,
					  int mcvsToUse);
static void ExecHashSkewTableInsert(HashState *hashState, HashJoinTable hashtable,
						TupleTableSlot *slot,
						uint32 hashvalue,
						int bucketNumber);
static void ExecHashRemoveNextSkewBucket(HashState *hashState, HashJoinTable hashtable);

static void ExecHashTableExplainEnd(PlanState *planstate, struct StringInfoData *buf);
static void
ExecHashTableExplainBatches(HashJoinTable   hashtable,
                            StringInfo      buf,
                            int             ibatch_begin,
                            int             ibatch_end,
                            const char     *title);

/* Amount of metadata memory required per bucket */
#define MD_MEM_PER_BUCKET (sizeof(HashJoinTuple) + sizeof(uint64))

/* ----------------------------------------------------------------
 *		ExecHash
 *
 *		stub for pro forma compliance
 * ----------------------------------------------------------------
 */
TupleTableSlot *
ExecHash(HashState *node)
{
	elog(ERROR, "Hash node does not support ExecProcNode call convention");
	return NULL;
}

/* ----------------------------------------------------------------
 *		MultiExecHash
 *
 *		build hash table for hashjoin, doing partitioning if more
 *		than one batch is required.
 * ----------------------------------------------------------------
 */
Node *
MultiExecHash(HashState *node)
{
	PlanState  *outerNode;
	List	   *hashkeys;
	HashJoinTable hashtable;
	TupleTableSlot *slot;
	ExprContext *econtext;
	uint32		hashvalue;

	/* must provide our own instrumentation support */
	if (node->ps.instrument)
		InstrStartNode(node->ps.instrument);

	/*
	 * get state info from node
	 */
	outerNode = outerPlanState(node);
	hashtable = node->hashtable;

	/*
	 * set expression context
	 */
	hashkeys = node->hashkeys;
	econtext = node->ps.ps_ExprContext;

	SIMPLE_FAULT_INJECTOR(MultiExecHashLargeVmem);

	/*
	 * get all inner tuples and insert into the hash table (or temp files)
	 */
	for (;;)
	{
		slot = ExecProcNode(outerNode);
		if (TupIsNull(slot))
			break;

		/* We have to compute the hash value */
		econtext->ecxt_innertuple = slot;
		bool hashkeys_null = false;

		if (ExecHashGetHashValue(node, hashtable, econtext, hashkeys, false,
								 node->hs_keepnull, &hashvalue, &hashkeys_null))
		{
			int			bucketNumber;

			bucketNumber = ExecHashGetSkewBucket(hashtable, hashvalue);
			if (bucketNumber != INVALID_SKEW_BUCKET_NO)
			{
				/* It's a skew tuple, so put it into that hash table */
				ExecHashSkewTableInsert(node, hashtable, slot, hashvalue,
										bucketNumber);
			}
			else
			{
				/* Not subject to skew optimization, so insert normally */
				ExecHashTableInsert(node, hashtable, slot, hashvalue);
			}
			hashtable->totalTuples += 1;
		}

		if (hashkeys_null)
		{
			node->hs_hashkeys_null = true;
			if (node->hs_quit_if_hashkeys_null)
			{
				ExecSquelchNode(outerNode);
				return NULL;
			}
		}
	}
	MemoryAccounting_DeclareDone();

	/* Now we have set up all the initial batches & primary overflow batches. */
	hashtable->nbatch_outstart = hashtable->nbatch;

	/* must provide our own instrumentation support */
	if (node->ps.instrument)
		InstrStopNode(node->ps.instrument, hashtable->totalTuples);

	/*
	 * We do not return the hash table directly because it's not a subtype of
	 * Node, and so would violate the MultiExecProcNode API.  Instead, our
	 * parent Hashjoin node is expected to know how to fish it out of our node
	 * state.  Ugly but not really worth cleaning up, since Hashjoin knows
	 * quite a bit more about Hash besides that.
	 */
	return NULL;
}

/* ----------------------------------------------------------------
 *		ExecInitHash
 *
 *		Init routine for Hash node
 * ----------------------------------------------------------------
 */
HashState *
ExecInitHash(Hash *node, EState *estate, int eflags)
{
	HashState  *hashstate;

	/* check for unsupported flags */
	Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

	/*
	 * create state structure
	 */
	hashstate = makeNode(HashState);
	hashstate->ps.plan = (Plan *) node;
	hashstate->ps.state = estate;
	hashstate->hashtable = NULL;
	hashstate->hashkeys = NIL;	/* will be set by parent HashJoin */

	/*
	 * Miscellaneous initialization
	 *
	 * create expression context for node
	 */
	ExecAssignExprContext(estate, &hashstate->ps);

#define HASH_NSLOTS 1

	/*
	 * initialize our result slot
	 */
	ExecInitResultTupleSlot(estate, &hashstate->ps);

	/*
	 * initialize child expressions
	 */
	hashstate->ps.targetlist = (List *)
		ExecInitExpr((Expr *) node->plan.targetlist,
					 (PlanState *) hashstate);
	hashstate->ps.qual = (List *)
		ExecInitExpr((Expr *) node->plan.qual,
					 (PlanState *) hashstate);

	/*
	 * initialize child nodes
	 */
	outerPlanState(hashstate) = ExecInitNode(outerPlan(node), estate, eflags);

	/*
	 * initialize tuple type. no need to initialize projection info because
	 * this node doesn't do projections
	 */
	ExecAssignResultTypeFromTL(&hashstate->ps);
	hashstate->ps.ps_ProjInfo = NULL;

	return hashstate;
}

int
ExecCountSlotsHash(Hash *node)
{
	return ExecCountSlotsNode(outerPlan(node)) +
		ExecCountSlotsNode(innerPlan(node)) +
		HASH_NSLOTS;
}

/* ---------------------------------------------------------------
 *		ExecEndHash
 *
 *		clean up routine for Hash node
 * ----------------------------------------------------------------
 */
void
ExecEndHash(HashState *node)
{
	PlanState  *outerPlan;

	/*
	 * free exprcontext
	 */
	ExecFreeExprContext(&node->ps);

	/*
	 * shut down the subplan
	 */
	outerPlan = outerPlanState(node);
	ExecEndNode(outerPlan);

	EndPlanStateGpmonPkt(&node->ps);
}

/* ----------------------------------------------------------------
 *		ExecHashTableCreate
 *
 *		create an empty hashtable data structure for hashjoin.
 * ----------------------------------------------------------------
 */
HashJoinTable
ExecHashTableCreate(HashState *hashState, HashJoinState *hjstate, List *hashOperators, uint64 operatorMemKB)
{
	HashJoinTable hashtable;
	Plan	   *outerNode;
	int			nbuckets;
	int			nbatch;
	int			num_skew_mcvs;
	int			log2_nbuckets;
	int			nkeys;
	int			i;
	ListCell   *ho;
	MemoryContext oldcxt;

	START_MEMORY_ACCOUNT(hashState->ps.plan->memoryAccountId);
	{
	Hash *node = (Hash *) hashState->ps.plan;

	/*
	 * Get information about the size of the relation to be hashed (it's the
	 * "outer" subtree of this node, but the inner relation of the hashjoin).
	 * Compute the appropriate size of the hash table.
	 */
	outerNode = outerPlan(node);

	ExecChooseHashTableSize(outerNode->plan_rows, outerNode->plan_width,
							OidIsValid(node->skewTable),
							operatorMemKB,
							&nbuckets, &nbatch, &num_skew_mcvs);

#ifdef HJDEBUG
	printf("nbatch = %d, nbuckets = %d\n", nbatch, nbuckets);
#endif

	/* nbuckets must be a power of 2 */
	log2_nbuckets = my_log2(nbuckets);
	Assert(nbuckets == (1 << log2_nbuckets));

	/*
	 * Initialize the hash table control block.
	 *
	 * The hashtable control block is just palloc'd from the executor's
	 * per-query memory context.
	 */
	hashtable = (HashJoinTable) palloc0(sizeof(HashJoinTableData));
	hashtable->nbuckets = nbuckets;
	hashtable->log2_nbuckets = log2_nbuckets;
	hashtable->buckets = NULL;
	hashtable->skewEnabled = false;
	hashtable->skewBucket = NULL;
	hashtable->skewBucketLen = 0;
	hashtable->nSkewBuckets = 0;
	hashtable->skewBucketNums = NULL;
	hashtable->nbatch = nbatch;
	hashtable->curbatch = 0;
	hashtable->nbatch_original = nbatch;
	hashtable->nbatch_outstart = nbatch;
	hashtable->growEnabled = true;
	hashtable->totalTuples = 0;
	hashtable->innerBatchFile = NULL;
	hashtable->outerBatchFile = NULL;
	hashtable->work_set = NULL;
	hashtable->state_file = NULL;
	hashtable->spaceUsed = 0;
	hashtable->spaceAllowed = operatorMemKB * 1024L;
	hashtable->spaceUsedSkew = 0;
	hashtable->spaceAllowedSkew =
		hashtable->spaceAllowed * SKEW_WORK_MEM_PERCENT / 100;
	hashtable->stats = NULL;
	hashtable->eagerlyReleased = false;
	hashtable->hjstate = hjstate;
	hashtable->first_pass = true;

	/*
	 * Get info about the hash functions to be used for each hash key. Also
	 * remember whether the join operators are strict.
	 */
	nkeys = list_length(hashOperators);
	hashtable->outer_hashfunctions =
		(FmgrInfo *) palloc(nkeys * sizeof(FmgrInfo));
	hashtable->inner_hashfunctions =
		(FmgrInfo *) palloc(nkeys * sizeof(FmgrInfo));
	hashtable->hashStrict = (bool *) palloc(nkeys * sizeof(bool));
	i = 0;
	foreach(ho, hashOperators)
	{
		Oid			hashop = lfirst_oid(ho);
		Oid			left_hashfn;
		Oid			right_hashfn;

		if (!get_op_hash_functions(hashop, &left_hashfn, &right_hashfn))
			elog(ERROR, "could not find hash function for hash operator %u",
				 hashop);
		fmgr_info(left_hashfn, &hashtable->outer_hashfunctions[i]);
		fmgr_info(right_hashfn, &hashtable->inner_hashfunctions[i]);
		hashtable->hashStrict[i] = op_strict(hashop);
		i++;
	}

	/*
	 * Create temporary memory contexts in which to keep the hashtable working
	 * storage.  See notes in executor/hashjoin.h.
	 */
	hashtable->hashCxt = AllocSetContextCreate(CurrentMemoryContext,
											   "HashTableContext",
											   ALLOCSET_DEFAULT_MINSIZE,
											   ALLOCSET_DEFAULT_INITSIZE,
											   ALLOCSET_DEFAULT_MAXSIZE);

	hashtable->batchCxt = AllocSetContextCreate(hashtable->hashCxt,
												"HashBatchContext",
												ALLOCSET_DEFAULT_MINSIZE,
												ALLOCSET_DEFAULT_INITSIZE,
												ALLOCSET_DEFAULT_MAXSIZE);

	/* CDB */ /* track temp buf file allocations in separate context */
	hashtable->bfCxt = AllocSetContextCreate(CurrentMemoryContext,
											 "hbbfcxt",
											 ALLOCSET_DEFAULT_MINSIZE,
											 ALLOCSET_DEFAULT_INITSIZE,
											 ALLOCSET_DEFAULT_MAXSIZE);

	/* Allocate data that will live for the life of the hashjoin */
	oldcxt = MemoryContextSwitchTo(hashtable->hashCxt);

	if (nbatch > 1)
	{
		/*
		 * allocate and initialize the file arrays in hashCxt
		 */
		hashtable->innerBatchFile = (ExecWorkFile **)
			palloc0(nbatch * sizeof(ExecWorkFile *));
		hashtable->outerBatchFile = (ExecWorkFile **)
			palloc0(nbatch * sizeof(ExecWorkFile *));
		/* The files will not be opened until needed... */
		/* ... but make sure we have temp tablespaces established for them */
		PrepareTempTablespaces();
	}

	/*
	 * Prepare context for the first-scan space allocations; allocate the
	 * hashbucket array therein, and set each bucket "empty".
	 */
	MemoryContextSwitchTo(hashtable->batchCxt);

	hashtable->buckets = (HashJoinTuple *)
		palloc0(nbuckets * sizeof(HashJoinTuple));

	/*
	 * Set up for skew optimization, if possible and there's a need for more
	 * than one batch.	(In a one-batch join, there's no point in it.)
	 */
	if (nbatch > 1)
		ExecHashBuildSkewHash(hashtable, node, num_skew_mcvs);

	MemoryContextSwitchTo(oldcxt);
	}
	END_MEMORY_ACCOUNT();

	return hashtable;
}


/*
 * Compute appropriate size for hashtable given the estimated size of the
 * relation to be hashed (number of rows and average row width).
 *
 * This is exported so that the planner's costsize.c can use it.
 */

/* Target bucket loading (tuples per bucket) */
/*
 * CDB: we now use gp_hashjoin_tuples_per_bucket
 * #define NTUP_PER_BUCKET			10
 */

void
ExecChooseHashTableSize(double ntuples, int tupwidth, bool useskew,
						uint64 operatorMemKB,
						int *numbuckets,
						int *numbatches,
						int *num_skew_mcvs)
{
	int			tupsize;
	double		inner_rel_bytes;
	long		hash_table_bytes;
	long		skew_table_bytes;
	long		max_pointers;
	int			nbatch;
	int			nbuckets;
	int			i;

	/* num tuples is a global number. We should be receiving only part of that */
	if (Gp_role == GP_ROLE_EXECUTE)
	{
		ntuples = ntuples / getgpsegmentCount();
	}

	/* Force a plausible relation size if no info */
	if (ntuples <= 0.0)
		ntuples = 1000.0;

	/*
	 * Estimate tupsize based on footprint of tuple in hashtable... note this
	 * does not allow for any palloc overhead.	The manipulations of spaceUsed
	 * don't count palloc overhead either.
	 */
	tupsize = ExecHashRowSize(tupwidth);
	inner_rel_bytes = ntuples * tupsize;

	/*
	 * Target in-memory hashtable size is work_mem kilobytes.
	 */
	hash_table_bytes = operatorMemKB * 1024L;

	/*
	 * If skew optimization is possible, estimate the number of skew buckets
	 * that will fit in the memory allowed, and decrement the assumed space
	 * available for the main hash table accordingly.
	 *
	 * We make the optimistic assumption that each skew bucket will contain
	 * one inner-relation tuple.  If that turns out to be low, we will recover
	 * at runtime by reducing the number of skew buckets.
	 *
	 * hashtable->skewBucket will have up to 8 times as many HashSkewBucket
	 * pointers as the number of MCVs we allow, since ExecHashBuildSkewHash
	 * will round up to the next power of 2 and then multiply by 4 to reduce
	 * collisions.
	 */
	if (useskew)
	{
		skew_table_bytes = hash_table_bytes * SKEW_WORK_MEM_PERCENT / 100;

		*num_skew_mcvs = skew_table_bytes / (
		/* size of a hash tuple */
											 tupsize +
		/* worst-case size of skewBucket[] per MCV */
											 (8 * sizeof(HashSkewBucket *)) +
		/* size of skewBucketNums[] entry */
											 sizeof(int) +
		/* size of skew bucket struct itself */
											 SKEW_BUCKET_OVERHEAD
			);

		if (*num_skew_mcvs > 0)
			hash_table_bytes -= skew_table_bytes;
	}
	else
		*num_skew_mcvs = 0;

	/*
	 * Set nbuckets to achieve an average bucket load of gp_hashjoin_tuples_per_bucket when
	 * memory is filled.  Set nbatch to the smallest power of 2 that appears
	 * sufficient.  The Min() steps limit the results so that the pointer
	 * arrays we'll try to allocate do not exceed work_mem.
	 */
	max_pointers = (operatorMemKB * 1024L) / sizeof(void *);
	/* also ensure we avoid integer overflow in nbatch and nbuckets */
	max_pointers = Min(max_pointers, INT_MAX / 2);

	if (inner_rel_bytes > hash_table_bytes)
	{
		/* We'll need multiple batches */
		long		lbuckets;
		double		dbatch;
		int			minbatch;

		lbuckets = (hash_table_bytes / tupsize) / gp_hashjoin_tuples_per_bucket;
		lbuckets = Min(lbuckets, max_pointers);

		nbuckets = (int) lbuckets;

		/*
		 * Both nbuckets and nbatch must be powers of 2 to make
		 * ExecHashGetBucketAndBatch fast.	We already fixed nbatch; now inflate
		 * nbuckets to the next larger power of 2.	We also force nbuckets to not
		 * be real small, by starting the search at 2^10.
		 */
		i = 10;
		while ((1 << i) < nbuckets)
			i++;
		nbuckets = (1 << i);

		dbatch = ceil(inner_rel_bytes / hash_table_bytes);
		dbatch = Min(dbatch, max_pointers);
		minbatch = (int) dbatch;
		nbatch = 2;
		while (nbatch < minbatch)
		{
			nbatch <<= 1;
		}

		/*
		 * Check to see if we're capping the number of workfiles we allow per
		 * query
		 */
		if (gp_workfile_limit_files_per_query > 0)
		{
			int			nbatch_lower = nbatch;

			/*
			 * We create two files per batch during spilling - one for outer
			 * and one of inner side. Lower the nbatch if necessary to fit
			 * under that limit. Don't go below two batches, because in that
			 * case we're basically disabling spilling.
			 */
			while ((nbatch_lower * 2 > gp_workfile_limit_files_per_query) && (nbatch_lower > 2))
			{
				nbatch_lower >>= 1;
			}

			Assert(nbatch_lower <= nbatch);
			if (nbatch_lower != nbatch)
			{
				elog(LOG, "HashJoin: Too many batches computed: nbatch=%d. gp_workfile_limit_files_per_query=%d, using nbatch=%d instead",
					 nbatch, gp_workfile_limit_files_per_query, nbatch_lower);
				nbatch = nbatch_lower;
			}
		}
	}
	else
	{
		/* We expect the hashtable to fit in memory, we want to use
		 * more buckets if we have memory to spare */
		double		dbuckets_lower;
		double		dbuckets_upper;
		double		dbuckets;

		/* divide our tuple row-count estimate by our the number of
		 * tuples we'd like in a bucket: this produces a small bucket
		 * count independent of our work_mem setting */
		dbuckets_lower = (double)ntuples / (double)gp_hashjoin_tuples_per_bucket;

		/* if we have work_mem to spare, we'd like to use it -- so
		 * divide up our memory evenly (see the spill case above) */
		dbuckets_upper = (double)hash_table_bytes / ((double)tupsize * gp_hashjoin_tuples_per_bucket);

		/* we'll use our "lower" work_mem independent guess as a lower
		 * limit; but if we've got memory to spare we'll take the mean
		 * of the lower-limit and the upper-limit */
		if (dbuckets_upper > dbuckets_lower)
			dbuckets = (dbuckets_lower + dbuckets_upper)/2.0;
		else
			dbuckets = dbuckets_lower;

		dbuckets = ceil(dbuckets);
		dbuckets = Min(dbuckets, max_pointers);

		nbuckets = (int) dbuckets;

		/*
		 * Both nbuckets and nbatch must be powers of 2 to make
		 * ExecHashGetBucketAndBatch fast.	We already fixed nbatch; now inflate
		 * nbuckets to the next larger power of 2.	We also force nbuckets to not
		 * be real small, by starting the search at 2^10.  (Note: above we made
		 * sure that nbuckets is not more than INT_MAX / 2, so this loop cannot
		 * overflow, nor can the final shift to recalculate nbuckets.)
		 */
		i = 10;
		while ((1 << i) < nbuckets)
			i++;
		nbuckets = (1 << i);

		nbatch = 1;
	}

	*numbuckets = nbuckets;
	*numbatches = nbatch;
}


/* ----------------------------------------------------------------
 *		ExecHashTableDestroy
 *
 *		destroy a hash table
 * ----------------------------------------------------------------
 */
void
ExecHashTableDestroy(HashState *hashState, HashJoinTable hashtable)
{
	int			i;

	Assert(hashtable);
	Assert(!hashtable->eagerlyReleased);

	START_MEMORY_ACCOUNT(hashState->ps.plan->memoryAccountId);
	{

	/*
	 * Make sure all the temp files are closed.
	 */
	if (hashtable->innerBatchFile)
	{
		for (i = 0; i < hashtable->nbatch; i++)
		{
			if (hashtable->innerBatchFile[i])
				workfile_mgr_close_file(hashtable->work_set, hashtable->innerBatchFile[i]);
			if (hashtable->outerBatchFile[i])
				workfile_mgr_close_file(hashtable->work_set, hashtable->outerBatchFile[i]);
			hashtable->innerBatchFile[i] = NULL;
			hashtable->outerBatchFile[i] = NULL;
		}
	}

	/* Close state file as well */
	if (hashtable->state_file != NULL)
	{
		workfile_mgr_close_file(hashtable->work_set, hashtable->state_file);
		hashtable->state_file = NULL;
	}

	if (hashtable->work_set != NULL)
	{
		workfile_mgr_close_set(hashtable->work_set);
		hashtable->work_set = NULL;
	}

	/* Release working memory (batchCxt is a child, so it goes away too) */
	MemoryContextDelete(hashtable->hashCxt);
	}
	END_MEMORY_ACCOUNT();
}

/*
 * ExecHashIncreaseNumBatches
 *		increase the original number of batches in order to reduce
 *		current memory consumption
 */
static void
ExecHashIncreaseNumBatches(HashJoinTable hashtable)
{
	int			oldnbatch = hashtable->nbatch;
	int			curbatch = hashtable->curbatch;
	int			nbatch;
	int			i;
	MemoryContext oldcxt;
	long		ninmemory;
	long		nfreed;
	Size		spaceUsedBefore = hashtable->spaceUsed;
	Size		spaceFreed = 0;
	HashJoinTableStats *stats = hashtable->stats;

	/* do nothing if we've decided to shut off growth */
	if (!hashtable->growEnabled)
		return;

	/* safety check to avoid overflow */
	if (oldnbatch > Min(INT_MAX / 2, MaxAllocSize / (sizeof(void *) * 2)))
		return;

	/* A reusable hash table can only respill during first pass */
	AssertImply(hashtable->hjstate->reuse_hashtable, hashtable->first_pass);

	nbatch = oldnbatch * 2;
	Assert(nbatch > 1);

#ifdef HJDEBUG
	printf("Increasing nbatch to %d because space = %lu\n",
		   nbatch, (unsigned long) hashtable->spaceUsed);
#endif

	oldcxt = MemoryContextSwitchTo(hashtable->hashCxt);

	if (hashtable->innerBatchFile == NULL)
	{
		/* we had no file arrays before */
		hashtable->innerBatchFile = (ExecWorkFile **)
			palloc0(nbatch * sizeof(ExecWorkFile *));
		hashtable->outerBatchFile = (ExecWorkFile **)
			palloc0(nbatch * sizeof(ExecWorkFile *));
		/* time to establish the temp tablespaces, too */
		PrepareTempTablespaces();
	}
	else
	{
		/* enlarge arrays and zero out added entries */
		hashtable->innerBatchFile = (ExecWorkFile **)
			repalloc(hashtable->innerBatchFile, nbatch * sizeof(ExecWorkFile *));
		hashtable->outerBatchFile = (ExecWorkFile **)
			repalloc(hashtable->outerBatchFile, nbatch * sizeof(ExecWorkFile *));
		MemSet(hashtable->innerBatchFile + oldnbatch, 0,
			   (nbatch - oldnbatch) * sizeof(ExecWorkFile *));
		MemSet(hashtable->outerBatchFile + oldnbatch, 0,
			   (nbatch - oldnbatch) * sizeof(ExecWorkFile *));
	}

	/* EXPLAIN ANALYZE batch statistics */
	if (stats && stats->nbatchstats < nbatch)
	{
		Size		sz = nbatch * sizeof(stats->batchstats[0]);

		stats->batchstats =
			(HashJoinBatchStats *) repalloc(stats->batchstats, sz);
		sz = (nbatch - stats->nbatchstats) * sizeof(stats->batchstats[0]);
		memset(stats->batchstats + stats->nbatchstats, 0, sz);
		stats->nbatchstats = nbatch;
	}

	MemoryContextSwitchTo(oldcxt);

	hashtable->nbatch = nbatch;

	/*
	 * Scan through the existing hash table entries and dump out any that are
	 * no longer of the current batch.
	 */
	ninmemory = nfreed = 0;

	for (i = 0; i < hashtable->nbuckets; i++)
	{
		HashJoinTuple prevtuple;
		HashJoinTuple tuple;

		prevtuple = NULL;
		tuple = hashtable->buckets[i];

		while (tuple != NULL)
		{
			/* save link in case we delete */
			HashJoinTuple nexttuple = tuple->next;
			int			bucketno;
			int			batchno;

			ninmemory++;
			ExecHashGetBucketAndBatch(hashtable, tuple->hashvalue,
									  &bucketno, &batchno);
			Assert(bucketno == i);
			if (batchno == curbatch)
			{
				/* keep tuple */
				prevtuple = tuple;
			}
			else
			{
				Size    spaceTuple;

				/* dump it out */
				Assert(batchno > curbatch);
				ExecHashJoinSaveTuple(NULL, HJTUPLE_MINTUPLE(tuple),
									  tuple->hashvalue,
									  hashtable,
									  &hashtable->innerBatchFile[batchno],
									  hashtable->bfCxt);
				/* and remove from hash table */
				if (prevtuple)
					prevtuple->next = nexttuple;
				else
					hashtable->buckets[i] = nexttuple;
				/* prevtuple doesn't change */
				spaceTuple = HJTUPLE_OVERHEAD + memtuple_get_size(HJTUPLE_MINTUPLE(tuple));
				hashtable->spaceUsed -= spaceTuple;
				spaceFreed += spaceTuple;
				if (stats)
					stats->batchstats[batchno].spillspace_in += spaceTuple;

				pfree(tuple);
				nfreed++;
			}

			tuple = nexttuple;
		}
	}

#ifdef HJDEBUG
	printf("Freed %ld of %ld tuples, space now %lu\n",
		   nfreed, ninmemory, (unsigned long) hashtable->spaceUsed);
#endif

	/* Update work_mem high-water mark and amount spilled. */
	if (stats)
	{
		stats->workmem_max = Max(stats->workmem_max, spaceUsedBefore);
		stats->batchstats[curbatch].spillspace_out += spaceFreed;
		stats->batchstats[curbatch].spillrows_out += nfreed;
	}

	/*
	 * If we dumped out either all or none of the tuples in the table, disable
	 * further expansion of nbatch.  This situation implies that we have
	 * enough tuples of identical hashvalues to overflow spaceAllowed.
	 * Increasing nbatch will not fix it since there's no way to subdivide the
	 * group any more finely. We have to just gut it out and hope the server
	 * has enough RAM.
	 */
	if (nfreed == 0 || nfreed == ninmemory)
	{
		hashtable->growEnabled = false;
		elog(LOG, "HJ: Disabling further increase of nbatch");
	}

}

/*
 * ExecHashTableInsert
 *		insert a tuple into the hash table depending on the hash value
 *		it may just go to a temp file for later batches
 *
 * Note: the passed TupleTableSlot may contain a regular, minimal, or virtual
 * tuple; the minimal case in particular is certain to happen while reloading
 * tuples from batch files.  We could save some cycles in the regular-tuple
 * case by not forcing the slot contents into minimal form; not clear if it's
 * worth the messiness required.
 *
 * Returns true if the tuple belonged to this batch and was inserted to
 * the in-memory hash table, or false if it belonged to a later batch and
 * was pushed to a temp file.
 */
bool
ExecHashTableInsert(HashState *hashState, HashJoinTable hashtable,
					TupleTableSlot *slot,
					uint32 hashvalue)
{
	MemTuple tuple = ExecFetchSlotMemTuple(slot, false);
	int			bucketno;
	int			batchno;
	int			hashTupleSize;

	START_MEMORY_ACCOUNT(hashState->ps.plan->memoryAccountId);
	{
	PlanState *ps = &hashState->ps;

	ExecHashGetBucketAndBatch(hashtable, hashvalue,
							  &bucketno, &batchno);

	hashTupleSize = HJTUPLE_OVERHEAD + memtuple_get_size(tuple);

	/*
	 * decide whether to put the tuple in the hash table or a temp file
	 */
	if (batchno == hashtable->curbatch)
	{
		/*
		 * put the tuple in hash table
		 */
		HashJoinTuple hashTuple;

		hashTuple = (HashJoinTuple) MemoryContextAlloc(hashtable->batchCxt,
													   hashTupleSize);
		hashTuple->hashvalue = hashvalue;
		memcpy(HJTUPLE_MINTUPLE(hashTuple), tuple, memtuple_get_size(tuple));
		hashTuple->next = hashtable->buckets[bucketno];
		hashtable->buckets[bucketno] = hashTuple;
		hashtable->spaceUsed += hashTupleSize;

		/* Double the number of batches when too much data in hash table. */
		if (hashtable->spaceUsed > hashtable->spaceAllowed)
		{
			ExecHashIncreaseNumBatches(hashtable);

			if (ps && ps->instrument)
			{
				ps->instrument->workfileCreated = true;
			}

			/* Gpmon stuff */
			if(ps)
			{
				CheckSendPlanStateGpmonPkt(ps);
			}
		}
	}
	else
	{
		/*
		 * put the tuple into a temp file for later batches
		 */
		Assert(batchno > hashtable->curbatch);
		ExecHashJoinSaveTuple(ps, tuple,
							  hashvalue,
							  hashtable,
							  &hashtable->innerBatchFile[batchno],
							  hashtable->bfCxt);
	}
	}
	END_MEMORY_ACCOUNT();

	return (batchno == hashtable->curbatch);
}

/*
 * ExecHashGetHashValue
 *		Compute the hash value for a tuple
 *
 * The tuple to be tested must be in either econtext->ecxt_outertuple or
 * econtext->ecxt_innertuple.  Vars in the hashkeys expressions reference
 * either OUTER or INNER.
 *
 * A TRUE result means the tuple's hash value has been successfully computed
 * and stored at *hashvalue.  A FALSE result means the tuple cannot match
 * because it contains a null attribute, and hence it should be discarded
 * immediately.  (If keep_nulls is true then FALSE is never returned.)
 * Found_null indicates all the hashkeys are null.
 */
bool
ExecHashGetHashValue(HashState *hashState, HashJoinTable hashtable,
					 ExprContext *econtext,
					 List *hashkeys,
					 bool outer_tuple,
					 bool keep_nulls,
					 uint32 *hashvalue,
					 bool *hashkeys_null)
{
	uint32		hashkey = 0;
	FmgrInfo   *hashfunctions;
	ListCell   *hk;
	int			i = 0;
	MemoryContext oldContext;
	bool		result = true;

	START_MEMORY_ACCOUNT(hashState->ps.plan->memoryAccountId);
	{

	Assert(hashkeys_null);

	(*hashkeys_null) = true;

	/*
	 * We reset the eval context each time to reclaim any memory leaked in the
	 * hashkey expressions.
	 */
	ResetExprContext(econtext);

	oldContext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

	if (outer_tuple)
		hashfunctions = hashtable->outer_hashfunctions;
	else
		hashfunctions = hashtable->inner_hashfunctions;

	foreach(hk, hashkeys)
	{
		ExprState  *keyexpr = (ExprState *) lfirst(hk);
		Datum		keyval;
		bool		isNull = false;

		/* rotate hashkey left 1 bit at each step */
		hashkey = (hashkey << 1) | ((hashkey & 0x80000000) ? 1 : 0);

		/*
		 * Get the join attribute value of the tuple
		 */
		keyval = ExecEvalExpr(keyexpr, econtext, &isNull, NULL);

		if (!isNull)
		{
			*hashkeys_null = false;
		}

		/*
		 * If the attribute is NULL, and the join operator is strict, then
		 * this tuple cannot pass the join qual so we can reject it
		 * immediately (unless we're scanning the outside of an outer join, in
		 * which case we must not reject it).  Otherwise we act like the
		 * hashcode of NULL is zero (this will support operators that act like
		 * IS NOT DISTINCT, though not any more-random behavior).  We treat
		 * the hash support function as strict even if the operator is not.
		 *
		 * Note: currently, all hashjoinable operators must be strict since
		 * the hash index AM assumes that.	However, it takes so little extra
		 * code here to allow non-strict that we may as well do it.
		 */
		if (isNull)
		{
			if (hashtable->hashStrict[i] && !keep_nulls)
			{
				result = false;
			}
			/* else, leave hashkey unmodified, equivalent to hashcode 0 */
		}
		else if (result)
		{
			/* Compute the hash function */
			uint32		hkey;

			hkey = DatumGetUInt32(FunctionCall1(&hashfunctions[i], keyval));
			hashkey ^= hkey;
		}

		i++;
	}

	MemoryContextSwitchTo(oldContext);

	*hashvalue = hashkey;
	}
	END_MEMORY_ACCOUNT();
	return result;
}

/*
 * ExecHashGetBucketAndBatch
 *		Determine the bucket number and batch number for a hash value
 *
 * Note: on-the-fly increases of nbatch must not change the bucket number
 * for a given hash code (since we don't move tuples to different hash
 * chains), and must only cause the batch number to remain the same or
 * increase.  Our algorithm is
 *		bucketno = hashvalue MOD nbuckets
 *		batchno = (hashvalue DIV nbuckets) MOD nbatch
 * where nbuckets and nbatch are both expected to be powers of 2, so we can
 * do the computations by shifting and masking.  (This assumes that all hash
 * functions are good about randomizing all their output bits, else we are
 * likely to have very skewed bucket or batch occupancy.)
 *
 * nbuckets doesn't change over the course of the join.
 *
 * nbatch is always a power of 2; we increase it only by doubling it.  This
 * effectively adds one more bit to the top of the batchno.
 */
void
ExecHashGetBucketAndBatch(HashJoinTable hashtable,
						  uint32 hashvalue,
						  int *bucketno,
						  int *batchno)
{
	uint32		nbuckets = (uint32) hashtable->nbuckets;
	uint32		nbatch = (uint32) hashtable->nbatch;

	if (nbatch > 1)
	{
		/* we can do MOD by masking, DIV by shifting */
		*bucketno = hashvalue & (nbuckets - 1);
		*batchno = (hashvalue >> hashtable->log2_nbuckets) & (nbatch - 1);
	}
	else
	{
		*bucketno = hashvalue & (nbuckets - 1);
		*batchno = 0;
	}
}

/*
 * ExecScanHashBucket
 *		scan a hash bucket for matches to the current outer tuple
 *
 * The current outer tuple must be stored in econtext->ecxt_outertuple.
 */
HashJoinTuple
ExecScanHashBucket(HashState *hashState, HashJoinState *hjstate,
				   ExprContext *econtext)
{
	List	   *hjclauses = hjstate->hashqualclauses;
	HashJoinTable hashtable = hjstate->hj_HashTable;
	HashJoinTuple hashTuple = hjstate->hj_CurTuple;
	uint32		hashvalue = hjstate->hj_CurHashValue;

	START_MEMORY_ACCOUNT(hashState->ps.plan->memoryAccountId);
	{
	/*
	 * hj_CurTuple is the address of the tuple last returned from the current
	 * bucket, or NULL if it's time to start scanning a new bucket.
	 *
	 * If the tuple hashed to a skew bucket then scan the skew bucket
	 * otherwise scan the standard hashtable bucket.
	 */
	if (hashTuple != NULL)
		hashTuple = hashTuple->next;
	else if (hjstate->hj_CurSkewBucketNo != INVALID_SKEW_BUCKET_NO)
		hashTuple = hashtable->skewBucket[hjstate->hj_CurSkewBucketNo]->tuples;
	else
		hashTuple = hashtable->buckets[hjstate->hj_CurBucketNo];

	while (hashTuple != NULL)
	{
		if (hashTuple->hashvalue == hashvalue)
		{
			TupleTableSlot *inntuple;

			/* insert hashtable's tuple into exec slot so ExecQual sees it */
			inntuple = ExecStoreMinimalTuple(HJTUPLE_MINTUPLE(hashTuple),
											 hjstate->hj_HashTupleSlot,
											 false);	/* do not pfree */
			econtext->ecxt_innertuple = inntuple;

			/* reset temp memory each time to avoid leaks from qual expr */
			ResetExprContext(econtext);

			if (ExecQual(hjclauses, econtext, false))
			{
				hjstate->hj_CurTuple = hashTuple;
				return hashTuple;
			}
		}

		hashTuple = hashTuple->next;
	}
	}
	END_MEMORY_ACCOUNT();

	/*
	 * no match
	 */
	return NULL;
}

/*
 * ExecHashTableReset
 *
 *		reset hash table header for new batch
 */
void
ExecHashTableReset(HashState *hashState, HashJoinTable hashtable)
{
	MemoryContext oldcxt;
	int			nbuckets = hashtable->nbuckets;

	START_MEMORY_ACCOUNT(hashState->ps.plan->memoryAccountId);
	{
	Assert(!hashtable->eagerlyReleased);

	/*
	 * Release all the hash buckets and tuples acquired in the prior pass, and
	 * reinitialize the context for a new pass.
	 */
	MemoryContextReset(hashtable->batchCxt);
	oldcxt = MemoryContextSwitchTo(hashtable->batchCxt);

	/* Reallocate and reinitialize the hash bucket headers. */
	hashtable->buckets = (HashJoinTuple *)
		palloc0(nbuckets * sizeof(HashJoinTuple));

	hashtable->spaceUsed = 0;
	hashtable->totalTuples = 0;

	MemoryContextSwitchTo(oldcxt);
	}
	END_MEMORY_ACCOUNT();
}

void
ExecReScanHash(HashState *node, ExprContext *exprCtxt)
{
	/*
	 * if chgParam of subnode is not null then plan will be re-scanned by
	 * first ExecProcNode.
	 */
	if (((PlanState *) node)->lefttree->chgParam == NULL)
		ExecReScan(((PlanState *) node)->lefttree, exprCtxt);
}


/*
 * ExecHashTableExplainInit
 *      Called after ExecHashTableCreate to set up EXPLAIN ANALYZE reporting.
 */
void
ExecHashTableExplainInit(HashState *hashState, HashJoinState *hjstate,
						 HashJoinTable hashtable)
{
	MemoryContext oldcxt;
	int			nbatch = Max(hashtable->nbatch, 1);

    START_MEMORY_ACCOUNT(hashState->ps.plan->memoryAccountId);
    {
    /* Switch to a memory context that survives until ExecutorEnd. */
    oldcxt = MemoryContextSwitchTo(hjstate->js.ps.state->es_query_cxt);

    /* Request a callback at end of query. */
    hjstate->js.ps.cdbexplainfun = ExecHashTableExplainEnd;

    /* Create workarea and attach it to the HashJoinTable. */
    hashtable->stats = (HashJoinTableStats *)palloc0(sizeof(*hashtable->stats));
    hashtable->stats->endedbatch = -1;

    /* Create per-batch statistics array. */
    hashtable->stats->batchstats =
        (HashJoinBatchStats *)palloc0(nbatch * sizeof(hashtable->stats->batchstats[0]));
    hashtable->stats->nbatchstats = nbatch;

    /* Restore caller's memory context. */
    MemoryContextSwitchTo(oldcxt);
    }
    END_MEMORY_ACCOUNT();
}                               /* ExecHashTableExplainInit */


/*
 * ExecHashTableExplainEnd
 *      Called before ExecutorEnd to finish EXPLAIN ANALYZE reporting.
 */
void
ExecHashTableExplainEnd(PlanState *planstate, struct StringInfoData *buf)
{
    HashJoinState      *hjstate = (HashJoinState *)planstate;
    HashJoinTable       hashtable = hjstate->hj_HashTable;
    HashJoinTableStats *stats;
    Instrumentation    *jinstrument = hjstate->js.ps.instrument;
    int                 total_buckets;
    int                 i;

    if (!hashtable ||
        !hashtable->stats ||
        hashtable->nbatch < 1 ||
        !jinstrument)
        return;

    stats = hashtable->stats;

	/* Check batchstats not null: If nodeHash failed to palloc batchstats, it will
	 * throw.  Posgres will catch and handle it, but no matter what, postgres will 
	 * try to get some explain results.  We must check here in this case or we will
	 * segv.
	 */
	if(stats->batchstats == NULL)
		return;

	if (!hashtable->eagerlyReleased)
	{		
		HashState *hashState = (HashState *) innerPlanState(hjstate);

		/* Report on batch in progress, in case the join is being ended early. */
		ExecHashTableExplainBatchEnd(hashState, hashtable);

		/* Report executor memory used by our memory context. */
		jinstrument->execmemused +=
				(double)MemoryContextGetPeakSpace(hashtable->hashCxt);
	}
	else
	{
		/*
		 * Memory has been eagerly released. We can't get statistics
		 * from the memory context. We approximate from stats structure.
		 */

		jinstrument->execmemused += (double) stats->workmem_max;
	}
	
    /* Report actual work_mem high water mark. */
    jinstrument->workmemused = Max(jinstrument->workmemused, stats->workmem_max);

    /* How much work_mem would suffice to hold all inner tuples in memory? */
    if (hashtable->nbatch > 1)
    {
        uint64  workmemwanted = 0;

        /* Space actually taken by hash rows in completed batches... */
        for (i = 0; i <= stats->endedbatch; i++)
            workmemwanted += stats->batchstats[i].hashspace_final;

        /* ... plus workfile size for original batches not reached, plus... */
        for (; i < hashtable->nbatch_original; i++)
            workmemwanted += stats->batchstats[i].innerfilesize;

        /* ... rows spilled to unreached oflo batches, in case quitting early */
        for (; i < stats->nbatchstats; i++)
            workmemwanted += stats->batchstats[i].spillspace_in;

        /*
         * Sometimes workfiles are used even though all the data would fit
         * in work_mem.  For example, if the planner overestimated the inner
         * rel size, it might have instructed us to use more initial batches
         * than were actually needed, causing unnecessary workfile I/O.  To
         * avoid this I/O, the user would have to increase work_mem based on
         * the planner's estimate rather than our runtime observations.  For
         * now, we don't try to second-guess the planner; just keep quiet.
         */
        if (workmemwanted > PlanStateOperatorMemKB(planstate) * 1024L)
            jinstrument->workmemwanted =
                Max(jinstrument->workmemwanted, workmemwanted);
    }

    /* Report workfile I/O statistics. */
    if (hashtable->nbatch > 1)
    {
    	ExecHashTableExplainBatches(hashtable, buf, 0, 1, "Initial");
    	ExecHashTableExplainBatches(hashtable,
    			buf,
				1,
				hashtable->nbatch_original,
				"Initial");
    	ExecHashTableExplainBatches(hashtable,
    			buf,
				hashtable->nbatch_original,
				hashtable->nbatch_outstart,
				"Overflow");
    	ExecHashTableExplainBatches(hashtable,
    			buf,
				hashtable->nbatch_outstart,
				hashtable->nbatch,
				"Secondary Overflow");
    }

    /* Report hash chain statistics. */
    total_buckets = stats->nonemptybatches * hashtable->nbuckets;
    if (total_buckets > 0)
    {
        appendStringInfo(buf,
                         "Hash chain length"
                         " %.1f avg, %.0f max, using %d of %d buckets.  ",
                         cdbexplain_agg_avg(&stats->chainlength),
                         stats->chainlength.vmax,
                         stats->chainlength.vcnt,
                         total_buckets);
        if (hashtable->nbatch > stats->nonemptybatches)
            appendStringInfo(buf,
                             "Skipped %d empty batches.",
                             hashtable->nbatch - stats->nonemptybatches);
        appendStringInfoChar(buf, '\n');
    }
}                               /* ExecHashTableExplainEnd */


/*
 * ExecHashTableExplainBatches
 *      Report summary of EXPLAIN ANALYZE stats for a set of batches.
 */
void
ExecHashTableExplainBatches(HashJoinTable   hashtable,
                            StringInfo      buf,
                            int             ibatch_begin,
                            int             ibatch_end,
                            const char     *title)
{
    HashJoinTableStats *stats = hashtable->stats;
    CdbExplain_Agg      irdbytes;
    CdbExplain_Agg      iwrbytes;
    CdbExplain_Agg      ordbytes;
    CdbExplain_Agg      owrbytes;
    int                 i;

    if (ibatch_begin >= ibatch_end)
        return;

    Assert(ibatch_begin >= 0 &&
           ibatch_end <= hashtable->nbatch &&
           hashtable->nbatch <= stats->nbatchstats &&
           stats->batchstats != NULL);

    cdbexplain_agg_init0(&irdbytes);
    cdbexplain_agg_init0(&iwrbytes);
    cdbexplain_agg_init0(&ordbytes);
    cdbexplain_agg_init0(&owrbytes);

    /* Add up the batch stats. */
    for (i = ibatch_begin; i < ibatch_end; i++)
    {
        HashJoinBatchStats *bs = &stats->batchstats[i];

        cdbexplain_agg_upd(&irdbytes, (double)bs->irdbytes, i);
        cdbexplain_agg_upd(&iwrbytes, (double)bs->iwrbytes, i);
        cdbexplain_agg_upd(&ordbytes, (double)bs->ordbytes, i);
        cdbexplain_agg_upd(&owrbytes, (double)bs->owrbytes, i);
    }

    if (iwrbytes.vcnt + irdbytes.vcnt + owrbytes.vcnt + ordbytes.vcnt > 0)
    {
        if (ibatch_begin == ibatch_end - 1)
            appendStringInfo(buf,
                             "%s batch %d:\n",
                             title,
                             ibatch_begin);
        else
            appendStringInfo(buf,
                             "%s batches %d..%d:\n",
                             title,
                             ibatch_begin,
                             ibatch_end - 1);
    }

    /* Inner bytes read from workfile */
    if (irdbytes.vcnt > 0)
    {
        appendStringInfo(buf,
                         "  Read %.0fK bytes from inner workfile",
                         ceil(irdbytes.vsum / 1024));
        if (irdbytes.vcnt > 1)
            appendStringInfo(buf,
                             ": %.0fK avg x %d nonempty batches"
                             ", %.0fK max",
                             ceil(cdbexplain_agg_avg(&irdbytes)/1024),
                             irdbytes.vcnt,
                             ceil(irdbytes.vmax / 1024));
        appendStringInfoString(buf, ".\n");
    }

    /* Inner rel bytes spilled to workfile */
    if (iwrbytes.vcnt > 0)
    {
        appendStringInfo(buf,
                         "  Wrote %.0fK bytes to inner workfile",
                         ceil(iwrbytes.vsum / 1024));
        if (iwrbytes.vcnt > 1)
            appendStringInfo(buf,
                             ": %.0fK avg x %d overflowing batches"
                             ", %.0fK max",
                             ceil(cdbexplain_agg_avg(&iwrbytes)/1024),
                             iwrbytes.vcnt,
                             ceil(iwrbytes.vmax / 1024));
        appendStringInfoString(buf, ".\n");
    }

    /* Outer bytes read from workfile */
    if (ordbytes.vcnt > 0)
    {
        appendStringInfo(buf,
                         "  Read %.0fK bytes from outer workfile",
                         ceil(ordbytes.vsum / 1024));
        if (ordbytes.vcnt > 1)
            appendStringInfo(buf,
                             ": %.0fK avg x %d nonempty batches"
                             ", %.0fK max",
                             ceil(cdbexplain_agg_avg(&ordbytes)/1024),
                             ordbytes.vcnt,
                             ceil(ordbytes.vmax / 1024));
        appendStringInfoString(buf, ".\n");
    }

    /* Outer rel bytes spilled to workfile */
    if (owrbytes.vcnt > 0)
    {
        appendStringInfo(buf,
                         "  Wrote %.0fK bytes to outer workfile",
                         ceil(owrbytes.vsum / 1024));
        if (owrbytes.vcnt > 1)
            appendStringInfo(buf,
                             ": %.0fK avg x %d overflowing batches"
                             ", %.0fK max",
                             ceil(cdbexplain_agg_avg(&owrbytes)/1024),
                             owrbytes.vcnt,
                             ceil(owrbytes.vmax / 1024));
        appendStringInfoString(buf, ".\n");
    }
}                               /* ExecHashTableExplainBatches */


/*
 * ExecHashTableExplainBatchEnd
 *      Called at end of each batch to collect statistics for EXPLAIN ANALYZE.
 */
void
ExecHashTableExplainBatchEnd(HashState *hashState, HashJoinTable hashtable)
{
    int                 curbatch = hashtable->curbatch;
    HashJoinTableStats *stats = hashtable->stats;
    HashJoinBatchStats *batchstats = &stats->batchstats[curbatch];
    int                 i;
    
    START_MEMORY_ACCOUNT(hashState->ps.plan->memoryAccountId);
    {
    Assert(!hashtable->eagerlyReleased);

    /* Already reported on this batch? */
    if ( stats->endedbatch == curbatch 
			|| curbatch >= hashtable->nbatch || !hashtable->first_pass)
        return;
    stats->endedbatch = curbatch;

    /* Update high-water mark for work_mem actually used at one time. */
    if (stats->workmem_max < hashtable->spaceUsed)
        stats->workmem_max = hashtable->spaceUsed;

    /* Final size of hash table for this batch */
    batchstats->hashspace_final = hashtable->spaceUsed;

    /* Collect workfile I/O statistics. */
    if (hashtable->nbatch > 1)
    {
        uint64      owrbytes = 0;
        uint64      iwrbytes = 0;

        Assert(stats->batchstats &&
               hashtable->nbatch <= stats->nbatchstats);

        /* How much was read from inner workfile for current batch? */
        batchstats->irdbytes = batchstats->innerfilesize;

        /* How much was read from outer workfiles for current batch? */
		if (hashtable->outerBatchFile &&
			hashtable->outerBatchFile[curbatch] != NULL)
            batchstats->ordbytes =
				ExecWorkFile_Tell64(hashtable->outerBatchFile[curbatch]);

		/*
		 * How much was written to workfiles for the remaining batches?
		 */
		for (i = curbatch + 1; i < hashtable->nbatch; i++)
		{
			HashJoinBatchStats *bs = &stats->batchstats[i];
			uint64              filebytes = 0;

			if (hashtable->outerBatchFile &&
				hashtable->outerBatchFile[i] != NULL)
				filebytes = ExecWorkFile_Tell64(hashtable->outerBatchFile[i]);

			Assert(filebytes >= bs->outerfilesize);
			owrbytes += filebytes - bs->outerfilesize;
			bs->outerfilesize = filebytes;

			filebytes = 0;

			if (hashtable->innerBatchFile &&
				hashtable->innerBatchFile[i])
				filebytes = ExecWorkFile_Tell64(hashtable->innerBatchFile[i]);

			Assert(filebytes >= bs->innerfilesize);
			iwrbytes += filebytes - bs->innerfilesize;
			bs->innerfilesize = filebytes;
		}
		batchstats->owrbytes = owrbytes;
		batchstats->iwrbytes = iwrbytes;
    }                           /* give workfile I/O statistics */

	/* Collect hash chain statistics. */
	stats->nonemptybatches++;
	for (i = 0; i < hashtable->nbuckets; i++)
	{
		HashJoinTuple   hashtuple = hashtable->buckets[i];
		int             chainlength;

		if (hashtuple)
		{
			for (chainlength = 0; hashtuple; hashtuple = hashtuple->next)
				chainlength++;
			cdbexplain_agg_upd(&stats->chainlength, chainlength, i);
		}
	}

    }
    END_MEMORY_ACCOUNT();
}                               /* ExecHashTableExplainBatchEnd */


/*
 * ExecHashBuildSkewHash
 *
 *		Set up for skew optimization if we can identify the most common values
 *		(MCVs) of the outer relation's join key.  We make a skew hash bucket
 *		for the hash value of each MCV, up to the number of slots allowed
 *		based on available memory.
 */
static void
ExecHashBuildSkewHash(HashJoinTable hashtable, Hash *node, int mcvsToUse)
{
	HeapTupleData *statsTuple;
	Datum	   *values;
	int			nvalues;
	float4	   *numbers;
	int			nnumbers;

	/* Do nothing if planner didn't identify the outer relation's join key */
	if (!OidIsValid(node->skewTable))
		return;
	/* Also, do nothing if we don't have room for at least one skew bucket */
	if (mcvsToUse <= 0)
		return;

	/*
	 * Try to find the MCV statistics for the outer relation's join key.
	 */
	statsTuple = SearchSysCache(STATRELATT,
								ObjectIdGetDatum(node->skewTable),
								Int16GetDatum(node->skewColumn),
								0, 0);
	if (!HeapTupleIsValid(statsTuple))
		return;

	if (get_attstatsslot(statsTuple, node->skewColType, node->skewColTypmod,
						 STATISTIC_KIND_MCV, InvalidOid,
						 &values, &nvalues,
						 &numbers, &nnumbers))
	{
		double		frac;
		int			nbuckets;
		FmgrInfo   *hashfunctions;
		int			i;

		if (mcvsToUse > nvalues)
			mcvsToUse = nvalues;

		/*
		 * Calculate the expected fraction of outer relation that will
		 * participate in the skew optimization.  If this isn't at least
		 * SKEW_MIN_OUTER_FRACTION, don't use skew optimization.
		 */
		frac = 0;
		for (i = 0; i < mcvsToUse; i++)
			frac += numbers[i];
		if (frac < SKEW_MIN_OUTER_FRACTION)
		{
			free_attstatsslot(node->skewColType,
							  values, nvalues, numbers, nnumbers);
			ReleaseSysCache(statsTuple);
			return;
		}

		/*
		 * Okay, set up the skew hashtable.
		 *
		 * skewBucket[] is an open addressing hashtable with a power of 2 size
		 * that is greater than the number of MCV values.  (This ensures there
		 * will be at least one null entry, so searches will always
		 * terminate.)
		 *
		 * Note: this code could fail if mcvsToUse exceeds INT_MAX/8, but that
		 * is not currently possible since we limit pg_statistic entries to
		 * much less than that.
		 */
		nbuckets = 2;
		while (nbuckets <= mcvsToUse)
			nbuckets <<= 1;
		/* use two more bits just to help avoid collisions */
		nbuckets <<= 2;

		hashtable->skewEnabled = true;
		hashtable->skewBucketLen = nbuckets;

		/*
		 * We allocate the bucket memory in the hashtable's batch context. It
		 * is only needed during the first batch, and this ensures it will be
		 * automatically removed once the first batch is done.
		 */
		hashtable->skewBucket = (HashSkewBucket **)
			MemoryContextAllocZero(hashtable->batchCxt,
								   nbuckets * sizeof(HashSkewBucket *));
		hashtable->skewBucketNums = (int *)
			MemoryContextAllocZero(hashtable->batchCxt,
								   mcvsToUse * sizeof(int));

		hashtable->spaceUsed += nbuckets * sizeof(HashSkewBucket *)
			+ mcvsToUse * sizeof(int);
		hashtable->spaceUsedSkew += nbuckets * sizeof(HashSkewBucket *)
			+ mcvsToUse * sizeof(int);

		/*
		 * Create a skew bucket for each MCV hash value.
		 *
		 * Note: it is very important that we create the buckets in order of
		 * decreasing MCV frequency.  If we have to remove some buckets, they
		 * must be removed in reverse order of creation (see notes in
		 * ExecHashRemoveNextSkewBucket) and we want the least common MCVs to
		 * be removed first.
		 */
		hashfunctions = hashtable->outer_hashfunctions;

		for (i = 0; i < mcvsToUse; i++)
		{
			uint32		hashvalue;
			int			bucket;

			hashvalue = DatumGetUInt32(FunctionCall1(&hashfunctions[0],
													 values[i]));

			/*
			 * While we have not hit a hole in the hashtable and have not hit
			 * the desired bucket, we have collided with some previous hash
			 * value, so try the next bucket location.	NB: this code must
			 * match ExecHashGetSkewBucket.
			 */
			bucket = hashvalue & (nbuckets - 1);
			while (hashtable->skewBucket[bucket] != NULL &&
				   hashtable->skewBucket[bucket]->hashvalue != hashvalue)
				bucket = (bucket + 1) & (nbuckets - 1);

			/*
			 * If we found an existing bucket with the same hashvalue, leave
			 * it alone.  It's okay for two MCVs to share a hashvalue.
			 */
			if (hashtable->skewBucket[bucket] != NULL)
				continue;

			/* Okay, create a new skew bucket for this hashvalue. */
			hashtable->skewBucket[bucket] = (HashSkewBucket *)
				MemoryContextAlloc(hashtable->batchCxt,
								   sizeof(HashSkewBucket));
			hashtable->skewBucket[bucket]->hashvalue = hashvalue;
			hashtable->skewBucket[bucket]->tuples = NULL;
			hashtable->skewBucketNums[hashtable->nSkewBuckets] = bucket;
			hashtable->nSkewBuckets++;
			hashtable->spaceUsed += SKEW_BUCKET_OVERHEAD;
			hashtable->spaceUsedSkew += SKEW_BUCKET_OVERHEAD;
		}

		free_attstatsslot(node->skewColType,
						  values, nvalues, numbers, nnumbers);
	}

	ReleaseSysCache(statsTuple);
}

/*
 * ExecHashGetSkewBucket
 *
 *		Returns the index of the skew bucket for this hashvalue,
 *		or INVALID_SKEW_BUCKET_NO if the hashvalue is not
 *		associated with any active skew bucket.
 */
int
ExecHashGetSkewBucket(HashJoinTable hashtable, uint32 hashvalue)
{
	int			bucket;

	/*
	 * Always return INVALID_SKEW_BUCKET_NO if not doing skew optimization (in
	 * particular, this happens after the initial batch is done).
	 */
	if (!hashtable->skewEnabled)
		return INVALID_SKEW_BUCKET_NO;

	/*
	 * Since skewBucketLen is a power of 2, we can do a modulo by ANDing.
	 */
	bucket = hashvalue & (hashtable->skewBucketLen - 1);

	/*
	 * While we have not hit a hole in the hashtable and have not hit the
	 * desired bucket, we have collided with some other hash value, so try the
	 * next bucket location.
	 */
	while (hashtable->skewBucket[bucket] != NULL &&
		   hashtable->skewBucket[bucket]->hashvalue != hashvalue)
		bucket = (bucket + 1) & (hashtable->skewBucketLen - 1);

	/*
	 * Found the desired bucket?
	 */
	if (hashtable->skewBucket[bucket] != NULL)
		return bucket;

	/*
	 * There must not be any hashtable entry for this hash value.
	 */
	return INVALID_SKEW_BUCKET_NO;
}

/*
 * ExecHashSkewTableInsert
 *
 *		Insert a tuple into the skew hashtable.
 *
 * This should generally match up with the current-batch case in
 * ExecHashTableInsert.
 */
static void
ExecHashSkewTableInsert(HashState *hashState,
						HashJoinTable hashtable,
						TupleTableSlot *slot,
						uint32 hashvalue,
						int bucketNumber)
{
	MemTuple tuple = ExecFetchSlotMemTuple(slot, false);
	HashJoinTuple hashTuple;
	int			hashTupleSize;

	/* Create the HashJoinTuple */
	hashTupleSize = HJTUPLE_OVERHEAD + memtuple_get_size(tuple);
	hashTuple = (HashJoinTuple) MemoryContextAlloc(hashtable->batchCxt,
												   hashTupleSize);
	hashTuple->hashvalue = hashvalue;
	memcpy(HJTUPLE_MINTUPLE(hashTuple), tuple, memtuple_get_size(tuple));

	/* Push it onto the front of the skew bucket's list */
	hashTuple->next = hashtable->skewBucket[bucketNumber]->tuples;
	hashtable->skewBucket[bucketNumber]->tuples = hashTuple;

	/* Account for space used, and back off if we've used too much */
	hashtable->spaceUsed += hashTupleSize;
	hashtable->spaceUsedSkew += hashTupleSize;
	while (hashtable->spaceUsedSkew > hashtable->spaceAllowedSkew)
		ExecHashRemoveNextSkewBucket(hashState, hashtable);

	/* Check we are not over the total spaceAllowed, either */
	if (hashtable->spaceUsed > hashtable->spaceAllowed)
		ExecHashIncreaseNumBatches(hashtable);
}

/*
 *		ExecHashRemoveNextSkewBucket
 *
 *		Remove the least valuable skew bucket by pushing its tuples into
 *		the main hash table.
 */
static void
ExecHashRemoveNextSkewBucket(HashState *hashState, HashJoinTable hashtable)
{
	PlanState *ps = &hashState->ps;
	int			bucketToRemove;
	HashSkewBucket *bucket;
	uint32		hashvalue;
	int			bucketno;
	int			batchno;
	HashJoinTuple hashTuple;

	/* Locate the bucket to remove */
	bucketToRemove = hashtable->skewBucketNums[hashtable->nSkewBuckets - 1];
	bucket = hashtable->skewBucket[bucketToRemove];

	/*
	 * Calculate which bucket and batch the tuples belong to in the main
	 * hashtable.  They all have the same hash value, so it's the same for all
	 * of them.  Also note that it's not possible for nbatch to increase while
	 * we are processing the tuples.
	 */
	hashvalue = bucket->hashvalue;
	ExecHashGetBucketAndBatch(hashtable, hashvalue, &bucketno, &batchno);

	/* Process all tuples in the bucket */
	hashTuple = bucket->tuples;
	while (hashTuple != NULL)
	{
		HashJoinTuple nextHashTuple = hashTuple->next;
		MemTuple	tuple;
		Size		tupleSize;

		/*
		 * This code must agree with ExecHashTableInsert.  We do not use
		 * ExecHashTableInsert directly as ExecHashTableInsert expects a
		 * TupleTableSlot while we already have HashJoinTuples.
		 */
		tuple = HJTUPLE_MINTUPLE(hashTuple);
		tupleSize = HJTUPLE_OVERHEAD + memtuple_get_size(tuple);

		/* Decide whether to put the tuple in the hash table or a temp file */
		if (batchno == hashtable->curbatch)
		{
			/* Move the tuple to the main hash table */
			hashTuple->next = hashtable->buckets[bucketno];
			hashtable->buckets[bucketno] = hashTuple;
			/* We have reduced skew space, but overall space doesn't change */
			hashtable->spaceUsedSkew -= tupleSize;
		}
		else
		{
			/* Put the tuple into a temp file for later batches */
			Assert(batchno > hashtable->curbatch);
			ExecHashJoinSaveTuple(ps, tuple,
								  hashvalue,
								  hashtable,
								  &hashtable->innerBatchFile[batchno], hashtable->bfCxt);
			pfree(hashTuple);
			hashtable->spaceUsed -= tupleSize;
			hashtable->spaceUsedSkew -= tupleSize;
		}

		hashTuple = nextHashTuple;
	}

	/*
	 * Free the bucket struct itself and reset the hashtable entry to NULL.
	 *
	 * NOTE: this is not nearly as simple as it looks on the surface, because
	 * of the possibility of collisions in the hashtable.  Suppose that hash
	 * values A and B collide at a particular hashtable entry, and that A was
	 * entered first so B gets shifted to a different table entry.	If we were
	 * to remove A first then ExecHashGetSkewBucket would mistakenly start
	 * reporting that B is not in the hashtable, because it would hit the NULL
	 * before finding B.  However, we always remove entries in the reverse
	 * order of creation, so this failure cannot happen.
	 */
	hashtable->skewBucket[bucketToRemove] = NULL;
	hashtable->nSkewBuckets--;
	pfree(bucket);
	hashtable->spaceUsed -= SKEW_BUCKET_OVERHEAD;
	hashtable->spaceUsedSkew -= SKEW_BUCKET_OVERHEAD;

	/*
	 * If we have removed all skew buckets then give up on skew optimization.
	 * Release the arrays since they aren't useful any more.
	 */
	if (hashtable->nSkewBuckets == 0)
	{
		hashtable->skewEnabled = false;
		pfree(hashtable->skewBucket);
		pfree(hashtable->skewBucketNums);
		hashtable->skewBucket = NULL;
		hashtable->skewBucketNums = NULL;
		hashtable->spaceUsed -= hashtable->spaceUsedSkew;
		hashtable->spaceUsedSkew = 0;
	}
}
