/*-------------------------------------------------------------------------
 *
 * analyzeutils.c
 *
 *	  Provides utils functions for analyze.c
 *
 * Copyright (c) 2015, Pivotal Inc.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_statistic.h"
#include "cdb/cdbhash.h"
#include "cdb/cdbpartition.h"
#include "commands/analyzeutils.h"
#include "commands/vacuum.h"
#include "lib/binaryheap.h"
#include "miscadmin.h"
#include "parser/parse_oper.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/hsearch.h"


typedef struct MCVFreqEntry
{
	MCVFreqPair *entry;
} MCVFreqEntry;

typedef struct PartDatum
{
	int			partId;			/* id of the partition histogram where the
								 * datum is from */
	Datum		datum;
} PartDatum;

static Datum *buildMCVArrayForStatsEntry(MCVFreqPair **mcvpairArray, int *nEntries, float4 ndistinct, float4 samplerows);
static float4 *buildFreqArrayForStatsEntry(MCVFreqPair **mcvpairArray, int nEntries, float4 reltuples);
static int	datumHashTableMatch(const void *keyPtr1, const void *keyPtr2, Size keysize);
static uint32 datumHashTableHash(const void *keyPtr, Size keysize);
static HTAB *createDatumHashTable(unsigned int nEntries);
static MCVFreqPair *MCVFreqPairCopy(MCVFreqPair *mcvFreqPair);
static bool containsDatum(HTAB *datumHash, MCVFreqPair *mcvFreqPair);
static void addLeafPartitionMCVsToHashTable(HTAB *datumHash, HeapTuple heaptupleStats,
			 float4 partReltuples, TypInfo *typInfo
);
static void addMCVToHashTable(HTAB *datumHash, MCVFreqPair *mcvFreqPair);
static int	mcvpair_cmp(const void *a, const void *b);

static void initTypInfo(TypInfo *typInfo, Oid typOid);
static int	DatumHeapComparator(Datum lhs, Datum rhs, void *context);
static void advanceCursor(int pid, int *cursors, AttStatsSlot * *histSlots);
static Datum getMinBound(AttStatsSlot * *histSlots, int *cursors, int nParts, Oid ltFuncOid);
static Datum getMaxBound(AttStatsSlot * *histSlots, int nParts, Oid ltFuncOid);
static void
			getHistogramHeapTuple(AttStatsSlot * *histSlots, HeapTuple *heaptupleStats, int *numNotNullParts, int nParts);
static void initDatumHeap(binaryheap *hp, AttStatsSlot * *histSlots, int *cursors, int nParts);

static float4 getBucketSizes(const HeapTuple *heaptupleStats, const float4 *relTuples, int nParts,
			   MCVFreqPair **mcvPairRemaining, int rem_mcv,
			   float4 *eachBucket);

float4
get_rel_reltuples(Oid relid)
{
	float4		relTuples = 0.0;
	HeapTuple	tp;

	tp = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_class reltup = (Form_pg_class) GETSTRUCT(tp);

		relTuples = reltup->reltuples;
		ReleaseSysCache(tp);
	}

	return relTuples;
}

int32
get_rel_relpages(Oid relid)
{
	int32		relPages = 0.0;
	HeapTuple	tp;

	tp = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
	if (HeapTupleIsValid(tp))
	{
		Form_pg_class reltup = (Form_pg_class) GETSTRUCT(tp);

		relPages = reltup->relpages;
		ReleaseSysCache(tp);
	}

	return relPages;
}

/*
 * Given column stats of an attribute, build an MCVFreqPair and add it to the hash table.
 * If the MCV to be added already exist in the hash table, we increment its count value.
 * Input:
 * 	- datumHash: hash table
 * 	- partOid: Oid of current partition
 *  - partReltuples: Number of tuples in that partition
 * 	- typInfo: type information
 * Output:
 *  - partReltuples: the number of tuples in this partition
 */
static void
addLeafPartitionMCVsToHashTable (HTAB *datumHash, HeapTuple heaptupleStats,
								 float4 partReltuples, TypInfo * typInfo)
{
	AttStatsSlot mcvSlot;

	get_attstatsslot(&mcvSlot, heaptupleStats, STATISTIC_KIND_MCV,
					 InvalidOid, ATTSTATSSLOT_VALUES | ATTSTATSSLOT_NUMBERS);

	Assert(mcvSlot.nvalues == mcvSlot.nnumbers);
	for (int i = 0; i < mcvSlot.nvalues; i++)
	{
		Datum		mcv = mcvSlot.values[i];
		float4		count = partReltuples * mcvSlot.numbers[i];
		MCVFreqPair *mcvFreqPair = (MCVFreqPair *) palloc(sizeof(MCVFreqPair));

		mcvFreqPair->mcv = mcv;
		mcvFreqPair->count = count;
		mcvFreqPair->typinfo = typInfo;
		addMCVToHashTable(datumHash, mcvFreqPair);
		pfree(mcvFreqPair);
	}
	free_attstatsslot(&mcvSlot);
}


/*
 * Main function for aggregating leaf partition MCV/Freq to compute
 * root or interior partition MCV/Freq
 *
 * Input:
 * 	- relationOid: Oid of root or interior partition
 * 	- attnum: column number
 *  - numPartitions: # of elements in heaptupleStats and relTuples arrays
 *  - heaptupleStats: pg_statistics tuples for each partition
 *  - relTuples: number of tuples in each partition (pg_class.reltuples)
 * 	- nEntries: target number of MCVs/Freqs to be collected, the real number of
 * 	MCVs/Freqs returned may be less
 *
 * Output:
 * 	- result: two dimensional arrays of MCVs and Freqs
 */
MCVFreqPair **
aggregate_leaf_partition_MCVs(Oid relationOid,
							  AttrNumber attnum,
							  int numPartitions,
							  HeapTuple *heaptupleStats,
							  float4 *relTuples,
							  unsigned int nEntries,
							  double ndistinct,
							  int *num_mcv,
							  int *rem_mcv,
							  void **result)
{
	Oid			typoid = get_atttype(relationOid, attnum);
	TypInfo    *typInfo = (TypInfo *) palloc(sizeof(TypInfo));

	initTypInfo(typInfo, typoid);

	/* Hash table for storing combined MCVs */
	HTAB	   *datumHash = createDatumHashTable(nEntries);
	float4		sumReltuples = 0;

	for (int i = 0; i < numPartitions; i++)
	{
		if (!HeapTupleIsValid(heaptupleStats[i]))
			continue;

		addLeafPartitionMCVsToHashTable(datumHash, heaptupleStats[i], relTuples[i],
										typInfo);
		sumReltuples += relTuples[i];
	}

	*rem_mcv = hash_get_num_entries(datumHash);
	if (0 == *rem_mcv)
	{
		/* in the unlikely event of an empty hash table, return early */
		*result = NULL;
		result++;
		*result = NULL;
		hash_destroy(datumHash);
		return NULL;
	}

	int			i = 0;
	HASH_SEQ_STATUS hash_seq;
	MCVFreqEntry *mcvfreq;

	MCVFreqPair **mcvpairArray = palloc((*rem_mcv) * sizeof(MCVFreqPair *));

	/* put MCVFreqPairs in an array in order to sort */
	hash_seq_init(&hash_seq, datumHash);
	while ((mcvfreq = hash_seq_search(&hash_seq)) != NULL)
	{
		mcvpairArray[i++] = mcvfreq->entry;
	}
	/* sort MCVFreqPairs in descending order of frequency */
	qsort(mcvpairArray, i, sizeof(MCVFreqPair *), mcvpair_cmp);

	/* prepare returning MCV and Freq arrays */
	*num_mcv = Min(i, nEntries);
	*result = (void *) buildMCVArrayForStatsEntry(mcvpairArray, num_mcv,
												  ndistinct, sumReltuples);
	if (*result == NULL)
	{
		*num_mcv = 0;
		return mcvpairArray;
	}

	result++;					/* now switch to frequency array (result[1]) */
	*result = (void *) buildFreqArrayForStatsEntry(mcvpairArray, *num_mcv,
												   sumReltuples);

	hash_destroy(datumHash);
	pfree(typInfo);

	*rem_mcv -= *num_mcv;
	return mcvpairArray;
}

/*
 * Return an array of MCVs from the resultant MCVFreqPair array
 * Input:
 * 	- mcvpairArray: contains MCVs and corresponding counts in desc order
 * 	- nEntries: number of MCVs to be returned
 * 	- typoid: type oid of the MCV datum
 */
static Datum *
buildMCVArrayForStatsEntry(MCVFreqPair **mcvpairArray,
						   int *nEntries,
						   float4 ndistinct,
						   float4 samplerows)
{
	Assert(mcvpairArray);
	Assert(*nEntries > 0);

	Datum	   *out = palloc(sizeof(Datum) * (*nEntries));
	double		mincount = -1.0;

	if (*nEntries == (int) ndistinct &&
		ndistinct > 0 &&
		*nEntries <= default_statistics_target)
	{
		/* Track list includes all values seen, and all will fit */
	}
	else
	{
		double		avgcount,
					maxmincount;

		/* estimate # of occurrences in sample of a typical value */
		avgcount = (double) samplerows / ndistinct;
		/* set minimum threshold count to store a value */
		mincount = avgcount * 1.25;
		if (mincount < 2)
			mincount = 2;
		/* don't let threshold exceed 1/K, however */
		maxmincount = (double) samplerows / (double) default_statistics_target;
		if (mincount > maxmincount)
			mincount = maxmincount;

	}
	for (int i = 0; i < *nEntries; i++)
	{
		if ((mcvpairArray[i])->count < mincount)
		{
			if (i == 0)
			{
				pfree(out);
				return NULL;
			}
			else
			{
				*nEntries = i;
				break;
			}
		}
		Datum		mcv = (mcvpairArray[i])->mcv;

		out[i] = mcv;
	}

	return out;
}

/*
 * Return an array of frequencies from the resultant MCVFreqPair array
 * Input:
 * 	- mcvpairArray: contains MCVs and corresponding counts in desc order
 * 	- nEntries: number of frequencies to be returned
 * 	- reltuples: number of tuples of the root or interior partition (all leaf partitions combined)
 */
static float4 *
buildFreqArrayForStatsEntry(MCVFreqPair **mcvpairArray,
							int nEntries,
							float4 reltuples)
{
	Assert(mcvpairArray);
	Assert(nEntries > 0);
	Assert(reltuples > 0);		/* otherwise ANALYZE will not collect stats */

	float4	   *out = (float *) palloc(sizeof(float4) * nEntries);

	for (int i = 0; i < nEntries; i++)
	{
		float4		freq = mcvpairArray[i]->count / reltuples;

		out[i] = freq;
	}

	return out;
}

/*
 * Comparison function to sort an array of MCVFreqPairs in desc order
 */
static int
mcvpair_cmp(const void *a, const void *b)
{
	Assert(a);
	Assert(b);

	MCVFreqPair *mcvFreqPair1 = *(MCVFreqPair **) a;
	MCVFreqPair *mcvFreqPair2 = *(MCVFreqPair **) b;

	if (mcvFreqPair1->count > mcvFreqPair2->count)
		return -1;
	if (mcvFreqPair1->count < mcvFreqPair2->count)
		return 1;
	else
		return 0;
}

/**
 * Add an MCVFreqPair to the hash table, if the same datum already exists
 * in the hash table, update its count
 * Input:
 * 	datumHash - hash table
 * 	mcvFreqPair - MCVFreqPair to be added
 * 	typbyval - whether the datum inside is passed by value
 * 	typlen - pg_type.typlen of the datum type
 */
static void
addMCVToHashTable(HTAB *datumHash, MCVFreqPair *mcvFreqPair)
{
	Assert(datumHash);
	Assert(mcvFreqPair);

	MCVFreqEntry *mcvfreq;
	bool		found = false;	/* required by hash_search */

	if (!containsDatum(datumHash, mcvFreqPair))
	{
		/* create a deep copy of MCVFreqPair and put it in the hash table */
		MCVFreqPair *key = MCVFreqPairCopy(mcvFreqPair);

		mcvfreq = hash_search(datumHash, &key, HASH_ENTER, &found);
		mcvfreq->entry = key;
	}
	else
	{
		mcvfreq = hash_search(datumHash, &mcvFreqPair, HASH_FIND, &found);
		Assert(mcvfreq);
		mcvfreq->entry->count += mcvFreqPair->count;
	}

	return;
}

/**
 * Copy function for MCVFreqPair
 * Input:
 * 	mcvFreqPair - input MCVFreqPair
 * 	typbyval - whether the datum inside is passed by value
 * 	typlen - pg_type.typlen of the datum type
 * Output:
 * 	result - a deep copy of input MCVFreqPair
 */
static MCVFreqPair *
MCVFreqPairCopy(MCVFreqPair *mcvFreqPair)
{
	MCVFreqPair *result = (MCVFreqPair *) palloc(sizeof(MCVFreqPair));

	result->count = mcvFreqPair->count;
	result->typinfo = mcvFreqPair->typinfo;
	result->mcv = datumCopy(mcvFreqPair->mcv,
							mcvFreqPair->typinfo->typbyval,
							mcvFreqPair->typinfo->typlen);

	return result;
}

/**
 * Test whether an MCVFreqPair is in the hash table
 * Input:
 * 	datumHash - hash table
 * 	mcvFreqPair - pointer to an MCVFreqPair
 * Output:
 * 	found - whether the MCVFreqPair is found
 */
static bool
containsDatum(HTAB *datumHash, MCVFreqPair *mcvFreqPair)
{
	bool		found = false;

	if (datumHash != NULL)
		hash_search(datumHash, &mcvFreqPair, HASH_FIND, &found);

	return found;
}

/**
 * Create a hash table with both hash key and hash entry as a pointer
 * to a MCVFreqPair struct
 * Input:
 * 	nEntries - estimated number of elements in the hash table, the size
 * 	of the hash table can grow dynamically
 * Output:
 * 	a pointer to the created hash table
 */
static HTAB *
createDatumHashTable(unsigned int nEntries)
{
	HASHCTL		hash_ctl;

	MemSet(&hash_ctl, 0, sizeof(hash_ctl));

	hash_ctl.keysize = sizeof(MCVFreqPair *);
	hash_ctl.entrysize = sizeof(MCVFreqEntry);
	hash_ctl.hash = datumHashTableHash;
	hash_ctl.match = datumHashTableMatch;

	return hash_create("DatumHashTable", nEntries, &hash_ctl,
					   HASH_ELEM | HASH_FUNCTION | HASH_COMPARE);
}

/**
 * Hash function for MCVFreqPair struct pointer.
 * Input:
 * 	keyPtr - pointer to hash key
 * 	keysize - not used, hash function must have this signature
 * Output:
 * 	result - hash value as an unsigned integer
 */
static uint32
datumHashTableHash(const void *keyPtr, Size keysize)
{
	uint32		result;
	MCVFreqPair *mcvFreqPair = *((MCVFreqPair **)keyPtr);
	FmgrInfo   *hashfunc = &mcvFreqPair->typinfo->hashfunc;

	result = DatumGetUInt32(FunctionCall1(hashfunc, mcvFreqPair->mcv));

	return result;
}

/**
 * Match function for MCVFreqPair struct pointer.
 * Input:
 * 	keyPtr1, keyPtr2 - pointers to two hash keys
 * 	keysize - not used, hash function must have this signature
 * Output:
 * 	0 if two hash keys match, 1 otherwise
 */
static int
datumHashTableMatch(const void *keyPtr1, const void *keyPtr2, Size keysize)
{
	Assert(keyPtr1);
	Assert(keyPtr2);

	MCVFreqPair *left = *((MCVFreqPair **) keyPtr1);
	MCVFreqPair *right = *((MCVFreqPair **) keyPtr2);

	Assert(left->typinfo->typOid == right->typinfo->typOid);

	return datumCompare(left->mcv, right->mcv, left->typinfo->eqFuncOp) ? 0 : 1;
}

/*
 * Initialize type information
 * Input:
 * 	typOid - Oid of the type
 * Output:
 *  members of typInfo are initialized
 */
static void
initTypInfo(TypInfo *typInfo, Oid typOid)
{
	Oid			ltOpr;
	Oid			eqOpr;
	Oid			hashFunc;

	typInfo->typOid = typOid;
	get_typlenbyval(typOid, &typInfo->typlen, &typInfo->typbyval);

	get_sort_group_operators(typOid, false, true, false, &ltOpr, &eqOpr, NULL, NULL);
	typInfo->ltFuncOp = get_opcode(ltOpr);
	typInfo->eqFuncOp = get_opcode(eqOpr);

	if (!get_op_hash_functions(eqOpr, &hashFunc, NULL))
		elog(ERROR, "could not find hash function for hash operator %u", eqOpr);
	fmgr_info(hashFunc, &typInfo->hashfunc);
}

/*
 * Comparator function of heap element PartDatum
 * Input:
 * 	lhs, rhs - pointers to heap elements
 * 	context - pointer to comparison context
 * Output:
 *  -1 if lhs < rhs
 *  0 if lhs == rhs
 *  1 if lhs > rhs
 */
static int
DatumHeapComparator(Datum lhs, Datum rhs, void *context)
{
	Datum		d1 = ((PartDatum *) DatumGetPointer(lhs))->datum;
	Datum		d2 = ((PartDatum *) DatumGetPointer(rhs))->datum;
	TypInfo    *typInfo = (TypInfo *) context;

	if (datumCompare(d1, d2, typInfo->ltFuncOp))
	{
		return 1;
	}

	if (datumCompare(d1, d2, typInfo->eqFuncOp))
	{
		return 0;
	}

	return -1;
}

/* Advance the cursor of a partition by 1, set to -1 if the end is reached
 * Input:
 * 	pid - partition id
 * 	cursors - cursor vector
 * 	nBounds - array of the number of bounds
 * */
static void
advanceCursor(int pid, int *cursors, AttStatsSlot * *histSlots)
{
	cursors[pid]++;
	if (cursors[pid] >= histSlots[pid]->nvalues)
	{
		cursors[pid] = -1;
	}
}

/*
 * Get the minimum bound of all partition bounds. Only need to iterate over
 * the first bound of each partition since the bounds in a histogram are ordered.
 */
static Datum
getMinBound(AttStatsSlot * *histSlots, int *cursors, int nParts, Oid ltFuncOid)
{
	Assert(histSlots);
	Assert(histSlots[0]);
	Assert(cursors);
	Assert(nParts > 0);

	Datum		minDatum = histSlots[0]->values[0];

	for (int pid = 0; pid < nParts; pid++)
	{
		if (datumCompare(histSlots[pid]->values[0], minDatum, ltFuncOid))
		{
			minDatum = histSlots[pid]->values[0];
		}
		advanceCursor(pid, cursors, histSlots);
	}

	return minDatum;
}

/*
 * Get the maximum bound of all partition bounds. Only need to iterate over
 * the last bound of each partition since the bounds in a histogram are ordered.
 */
static Datum
getMaxBound(AttStatsSlot * *histSlots, int nParts, Oid ltFuncOid)
{
	Assert(histSlots);
	Assert(histSlots[0]);
	Assert(nParts > 0);

	Datum		maxDatum = histSlots[0]->values[histSlots[0]->nvalues - 1];

	for (int pid = 0; pid < nParts; pid++)
	{
		if (datumCompare(maxDatum, histSlots[pid]->values[histSlots[pid]->nvalues - 1], ltFuncOid))
		{
			maxDatum = histSlots[pid]->values[histSlots[pid]->nvalues - 1];
		}
	}

	return maxDatum;
}

/*
 * Preparing the output array of histogram bounds, removing any duplicates
 * Input:
 * 	ldatum - list of pointers to the aggregated bounds, may contain duplicates
 * 	typInfo - type information
 * Output:
 *  an array containing the aggregated histogram bounds
 */
static Datum *
buildHistogramEntryForStats(List *ldatum, TypInfo *typInfo, int *num_hist)
{
	Assert(ldatum);
	Assert(typInfo);

	Datum	   *histArray = (Datum *) palloc(sizeof(Datum) * list_length(ldatum));

	ListCell   *lc;
	Datum	   *prevDatum = (Datum *) linitial(ldatum);
	int			idx = 0;

	*num_hist = 0;

	foreach_with_count(lc, ldatum, idx)
	{
		Datum	   *pdatum = (Datum *) lfirst(lc);

		/* remove duplicate datum in the list, starting from the second datum */
		if (datumCompare(*pdatum, *prevDatum, typInfo->eqFuncOp) && idx > 0)
		{
			continue;
		}

		histArray[*num_hist] = *pdatum;
		*num_hist = *num_hist + 1;
		*prevDatum = *pdatum;
	}

	return histArray;
}


/*
 * Obtain all histogram bounds from every partition and store them in a 2D array (histData)
 * Input:
 * 	lRelOids - list of part Oids
 * 	typInfo - type info
 * 	attnum - attribute number
 * Output:
 * 	histData - 2D array of all histogram bounds from every partition
 * 	nBounds - array of the number of histogram bounds (from each partition)
 * 	partsReltuples - array of the number of tuples (from each partition)
 * 	sumReltuples - sum of number of tuples in all partitions
 */
static void
getHistogramHeapTuple(AttStatsSlot * *histSlots, HeapTuple *heaptupleStats,
					  int *numNotNullParts, int nParts)
{
	int			pid = 0;

	for (int i = 0; i < nParts; i++)
	{
		if (!HeapTupleIsValid(heaptupleStats[i]))
		{
			continue;
		}
		histSlots[pid] = (AttStatsSlot *) palloc(sizeof(AttStatsSlot));
		get_attstatsslot(histSlots[pid], heaptupleStats[i], STATISTIC_KIND_HISTOGRAM, InvalidOid, ATTSTATSSLOT_VALUES);

		if (histSlots[pid]->nvalues > 0)
		{
			pid++;
		}
	}
	*numNotNullParts = pid;
}

/*
 * Obtain all histogram bounds from every partition and store them in a 2D array (histData)
 * Input:
 * 	lRelOids - list of part Oids
 * 	typInfo - type info
 * 	attnum - attribute number
 * Output:
 * 	histData - 2D array of all histogram bounds from every partition
 * 	nBounds - array of the number of histogram bounds (from each partition)
 * 	partsReltuples - array of the number of tuples (from each partition)
 * 	sumReltuples - sum of number of tuples in all partitions
 */
static void
getHistogramMCVTuple(AttStatsSlot * *histSlots, MCVFreqPair **mcvRemaining,
					 int start_idx, int rem_mcv)
{
	for (int i = 0; i < rem_mcv; i++)
	{
		histSlots[start_idx + i] = (AttStatsSlot *) palloc(sizeof(AttStatsSlot));
		histSlots[start_idx + i]->nvalues = 2;
		histSlots[start_idx + i]->values = (Datum *) palloc(sizeof(Datum) * 2);
		histSlots[start_idx + i]->values[0] = mcvRemaining[i]->mcv;
		histSlots[start_idx + i]->values[1] = mcvRemaining[i]->mcv;
	}
}

/*
 * Initialize heap by inserting the second histogram bound from each partition histogram.
 * Input:
 * 	hp - heap
 * 	histData - all histogram bounds from each part
 * 	cursors - cursor vector
 * 	nParts - number of partitions
 */
static void
initDatumHeap(binaryheap *hp, AttStatsSlot * *histSlots, int *cursors, int nParts)
{
	PartDatum *pds = (PartDatum *) palloc(nParts * sizeof(PartDatum));

	for (int pid = 0; pid < nParts; pid++)
	{
		/* do nothing if part histogram only has one element */
		if (cursors[pid] > 0)
		{
			pds[pid].partId = pid;
			pds[pid].datum = histSlots[pid]->values[cursors[pid]];
			binaryheap_add_unordered(hp, PointerGetDatum(&pds[pid]));
		}
	}
	binaryheap_build(hp);
}

/*
 * Comparison function for two datums
 * Input:
 * 	d1, d2 - datums
 * 	opFuncOid - oid of the function for comparison operator of this datum type
 */
bool
datumCompare(Datum d1, Datum d2, Oid opFuncOid)
{
	FmgrInfo	ltproc;

	fmgr_info(opFuncOid, &ltproc);
	return DatumGetBool(FunctionCall2Coll(&ltproc, DEFAULT_COLLATION_OID, d1, d2));
}

/*
 * Main function for aggregating leaf partition histogram to compute
 * root or interior partition histogram
 * Input:
 * 	- relationOid: Oid of root or interior partition
 * 	- attnum: column number
 *  - nParts: # of elements in heaptupleStats and relTuples arrays
 *  - heaptupleStats: pg_statistics tuples for each partition
 * 	- nEntries: target number of histogram bounds to be collected, the real number of
 * 	histogram bounds returned may be less
 * Output:
 * 	- result: an array of aggregated histogram bounds
 * Algorithm:
 *
 * 	We use the following example to explain how the aggregating algorithm works.

	Suppose a parent table 'lineitem' has 3 partitions 'lineitem_prt_1', 'lineitem_prt_2',
	'lineitem_prt_3'. The histograms of the column of interest of the parts are:

	hist(prt_1): {0,19,38,59}
	hist(prt_2): {2,18,40,62}
	hist(prt_3): {1,22,39,61}

	Note the histograms are equi-depth, which implies each bucket should contain the same number of tuples.

	The number of tuples in each part is:

	nTuples(prt_1) = 300
	nTuples(prt_2) = 270
	nTuples(prt_3) = 330

	Some notation:

	hist(agg): the aggregated histogram
	hist(parts): the histograms of the partitions, i.e., {hist(prt_1), hist(prt_2), hist(prt_3)}
	nEntries: the target number of histogram buckets in hist(agg). Usually this is the same as in the partitions. In this example, nEntries = 3.
	nParts: the number of partitions. nParts = 3 in this example.

	Since we know the target number of tuples in each bucket of hist(agg), the basic idea is to fill the buckets of hist(agg) using the buckets in hist(parts). And once a bucket in hist(agg) is filled up, we look at which bucket from hist(parts) is the current bucket, and use its bound as the bucket bound in hist(agg).
	Continue with our example we have,

	bucketSize(prt_1) = 300/3 = 100
	bucketSize(prt_2) = 270/3 = 90
	bucketSize(prt_3) = 330/3 = 110
	bucketSize(agg) = (300+270+330)/3 = 300

	Now, to begin with, we find the minimum of the first boundary point across hist(parts) and use it as the first boundary of hist(agg), i.e.,
	hist(agg) = {min({0,2,1})} = {0}

	We need to maintain a priority queue in order to decide on the next bucket from hist(parts) to work with.
	Each element in the queue is a (Datum, partID) pair, where Datum is a boundary from hist(parts) and partID is the ID of the part the Datum comes from.
	Each time we dequeue(), we get the minimum datum in the queue as the next datum we will work on.
	The priority queue contains up to nParts entries. In our example, we first enqueue
	the second boundary across hist(parts), i.e., 19, 18, 22, along with their part ID.

	Continue with filling the bucket of hist(agg), we dequeue '18' from the queue and fill in
	the first bucket (having 90 tuples). Since bucketSize(agg) = 300, we need more buckets
	from hist(parts) to fill it. At the same time, we dequeue 18 and enqueue the next bound (which is 40).
	The first bucket of hist(agg) will be filled up by '22' (90+100+110 >= 300), at this time we put '22' as the next boundary value in hist(agg), i.e.
	hist(agg) = {0,22}

	Continue with the iteration, we will finally fill all the buckets
	hist(agg) = {0,22,40,62}
 *
 */
int
aggregate_leaf_partition_histograms(Oid relationOid,
									AttrNumber attnum,
									int nParts,
									HeapTuple *heaptupleStats,
									float4 *relTuples,
									unsigned int nEntries,
									MCVFreqPair **mcvpairArray,
									int rem_mcv,
									void **result)
{
	AssertImply(rem_mcv != 0, mcvpairArray != NULL);
	Assert(nParts > 0);

	/* get type information */
	TypInfo		typInfo;
	Oid			typOid = get_atttype(relationOid, attnum);

	initTypInfo(&typInfo, typOid);

	AttStatsSlot **histSlots = (AttStatsSlot * *) palloc0((nParts + rem_mcv) * sizeof(AttStatsSlot *));
	float4		sumReltuples = 0;

	int			numNotNullParts = 0;

	/* populate histData, nBounds, partsReltuples and sumReltuples */
	float4	   *eachBucket = palloc0((nParts + rem_mcv) * sizeof(float4));	/* the number of data
																			 * points in each bucket
																			 * for each histogram */

	getHistogramHeapTuple(histSlots, heaptupleStats, &numNotNullParts, nParts);
	if (0 == numNotNullParts + rem_mcv)
	{
		/* if all the parts histograms are empty, we return nothing */
		result = NULL;
		return 0;
	}
	getHistogramMCVTuple(histSlots, mcvpairArray, numNotNullParts, rem_mcv);
	sumReltuples = getBucketSizes(heaptupleStats, relTuples, nParts, mcvpairArray, rem_mcv, eachBucket);

	/* reset nParts to the number of non-null parts */
	nParts = numNotNullParts + rem_mcv;

	/* now define the state variables needed for the aggregation loop */
	float4		bucketSize = sumReltuples / nEntries;	/* target bucket size in
														 * the aggregated
														 * histogram */
	float4		nTuplesToFill = bucketSize; /* remaining number of tuples to
											 * fill in the current bucket of
											 * the aggregated histogram, reset
											 * to bucketSize when a new bucket
											 * is added */
	int		   *cursors = palloc0(nParts * sizeof(int));	/* the index of current
															 * bucket for each
															 * histogram, set to -1
															 * after the histogram
															 * has been traversed */
	float4	   *remainingSize = palloc0(nParts * sizeof(float4));	/* remaining number of
																	 * tuples in the current
																	 * bucket of a part */

	/* initialize eachBucket[] and remainingSize[] */
	for (int i = 0; i < nParts; i++)
	{
		if (1 < histSlots[i]->nvalues)
		{
			remainingSize[i] = eachBucket[i];
		}
	}

	/* we maintain a priority queue (min heap) of PartDatum */
	binaryheap *dhp = binaryheap_allocate(nParts,
										  DatumHeapComparator,
										  &typInfo);

	List	   *ldatum = NIL;	/* list of pointers to the selected bounds */

	/*
	 * the first bound in the aggregated histogram will be the minimum of the
	 * first bounds of all parts
	 */
	Datum		minBound = getMinBound(histSlots, cursors, nParts, typInfo.ltFuncOp);

	ldatum = lappend(ldatum, &minBound);

	/*
	 * continue filling the aggregated histogram, starting from the second
	 * bound
	 */
	initDatumHeap(dhp, histSlots, cursors, nParts);

	/*
	 * loop continues when DatumHeap is not empty yet and the number of
	 * histogram boundaries has not reached nEntries
	 */
	while (!binaryheap_empty(dhp) && list_length(ldatum) < nEntries)
	{
		PartDatum  *pd = (PartDatum *) DatumGetPointer(binaryheap_first(dhp));
		int			pid = pd->partId;

		if (remainingSize[pid] < nTuplesToFill)
		{
			nTuplesToFill -= remainingSize[pid];
			advanceCursor(pid, cursors, histSlots);
			remainingSize[pid] = eachBucket[pid];
			if (cursors[pid] > 0)
			{
				pd->datum = histSlots[pid]->values[cursors[pid]];
				binaryheap_replace_first(dhp, PointerGetDatum(pd));
			}
			else
				(void) binaryheap_remove_first(dhp);
		}
		else
		{
			ldatum = lappend(ldatum, &histSlots[pid]->values[cursors[pid]]);
			remainingSize[pid] -= nTuplesToFill;
			nTuplesToFill = bucketSize;
		}
	}

	/*
	 * adding the max boundary across all histograms to the aggregated
	 * histogram
	 */
	Datum		maxBound = getMaxBound(histSlots, nParts, typInfo.ltFuncOp);

	ldatum = lappend(ldatum, &maxBound);

	/* now ldatum contains the resulting boundaries */
	int			num_hist;
	Datum	   *out = buildHistogramEntryForStats(ldatum, &typInfo, &num_hist);

	/* clean up */
	binaryheap_free(dhp);

	*result = out;

	return num_hist;
}

static float4
getBucketSizes(const HeapTuple *heaptupleStats, const float4 *relTuples, int nParts,
			   MCVFreqPair **mcvPairRemaining, int rem_mcv,
			   float4 *eachBucket)
{
	float4	   *total = palloc(nParts * sizeof(float4));
	float4		sumTotal = 0;
	int			pid = 0;

	Assert(total != NULL);
	for (int i = 0; i < nParts; ++i)
	{
		AttStatsSlot mcvSlot;

		total[i] = relTuples[i];
		if (heaptupleStats[i] == NULL)
			continue;

		Form_pg_statistic stat = (Form_pg_statistic) GETSTRUCT(heaptupleStats[i]);

		if (get_attstatsslot(&mcvSlot, heaptupleStats[i], STATISTIC_KIND_MCV,
							 InvalidOid,
							 ATTSTATSSLOT_VALUES | ATTSTATSSLOT_NUMBERS))
		{
			Assert(mcvSlot.nvalues == mcvSlot.nnumbers);

			for (int j = 0; j < mcvSlot.nnumbers; ++j)
			{
				total[i] -= relTuples[i] * mcvSlot.numbers[j];
			}
		}
		total[i] -= relTuples[i] * stat->stanullfrac;
		if (total[i] < 0.0)		/* will this happen? */
		{
			total[i] = 0.0;
		}

		/* We assume eachBucket[i] is initialized to 0.0 */
		if (get_attstatsslot(&mcvSlot, heaptupleStats[i],
							 STATISTIC_KIND_HISTOGRAM, InvalidOid,
							 ATTSTATSSLOT_VALUES))
		{
			eachBucket[pid] = total[i] / (mcvSlot.nvalues - 1);
			pid++;
		}

		sumTotal += total[i];
	}

	for (int i = pid; i < pid + rem_mcv; ++i)
	{
		eachBucket[i] = mcvPairRemaining[i - pid]->count;
		sumTotal += eachBucket[i];
	}
	pfree(total);
	return sumTotal;
}

/*
 *	needs_sample() -- checks if the analyze requires sampling the actual data
 */
bool
needs_sample(VacAttrStats **vacattrstats, int attr_cnt)
{
	Assert(vacattrstats != NULL);
	int			i;

	for (i = 0; i < attr_cnt; i++)
	{
		Assert(vacattrstats[i] != NULL);
		if (!vacattrstats[i]->merge_stats)
			return true;
	}
	return false;
}

/*
 *	leaf_parts_analyzed() -- checks if all the leaf partitions are analyzed
 *                           for each requested column to be analyzed
 *
 *	We use this function to determine if all the leaf partitions are analyzed
 *  for the requested columns and the statistics are in place to be able to
 *  merge and generate meaningful statistics for the root partition. If any
 *  partition is analyzed and the attstattarget is set to collect stats, but
 *  there are no statistics for the partition in pg_statistics, root
 *  statistics will be bogus if we continue merging.
 *  0. A requested column in a single partition is not analyzed - return FALSE
 *  1. All partitions are analyzed
 *	  1.1. All partitions are empty - return FALSE
 *    1.2. Some empty & rest have stats - return TRUE
 *    1.3. Some empty & at least one don't have stats - return FALSE
 *    1.4. None empty & at least one don't have stats - return FALSE
 *    1.5. None empty & all have stats - return TRUE
 *
 *
 *  attrelid - the relation id of the root table
 *  relid_exclude - it is the relid that is excluded to check for the stats.
 *  It is used when we are asked to auto merge statistics when analyzing a
 *  single leaf partition. As we are going to produce stats for that
 *  specific leaf partition, we should not check its stats availability.
 *  va_cols - column attnum list to be analyzed from root table's perspective.
 *  These attnum's needs to be translated for each leaf table as the attnums
 *  for different columns might be different due to the dropped columns and
 *  split partitions.
 */
bool
leaf_parts_analyzed(Oid attrelid, Oid relid_exclude, List *va_cols, int elevel)
{
	PartitionNode *pn = get_parts(attrelid,
								  0 /* level */ ,
								  0 /* parent */ ,
								  false /* inctemplate */ ,
								  true /* includesubparts */ );

	Assert(pn);

	List	   *oid_list = all_leaf_partition_relids(pn);	/* all leaves */
	bool		all_parts_empty = true;
	ListCell   *lc,
			   *lc_col;

	foreach(lc, oid_list)
	{
		Oid			partRelid = lfirst_oid(lc);

		if (partRelid == relid_exclude)
			continue;

		float4		relTuples = get_rel_reltuples(partRelid);
		int32		relpages = get_rel_relpages(partRelid);

		/* Partition is analyzed and we detect it is empty */
		if (relTuples == 0.0 && relpages > 0)
			continue;

		all_parts_empty = false;

		foreach(lc_col, va_cols)
		{
			/*
			 * Check stats availability for each column that asked to be
			 * analyzed.
			 */
			AttrNumber	attnum = lfirst_int(lc_col);
			const char *attname = get_relid_attribute_name(attrelid, attnum);
			AttrNumber	child_attno = get_attnum(partRelid, attname);

			HeapTuple	heaptupleStats = get_att_stats(partRelid, child_attno);

			/* if there is no colstats */
			if (!HeapTupleIsValid(heaptupleStats) || relpages == 0)
			{
				if (relid_exclude == InvalidOid)
					ereport(elevel,
							(errmsg("column %s of partition %s is not analyzed, so ANALYZE will collect sample for stats calculation",
									attname, get_rel_name(partRelid))));
				else
					ereport(elevel,
							(errmsg("auto merging of leaf partition stats to calculate root partition stats is not possible because column %s of partition %s is not analyzed",
									attname, get_rel_name(partRelid))));
				return false;
			}
			heap_freetuple(heaptupleStats);
		}
	}

	return !all_parts_empty;
}
