/*
 * execHHashagg.c
 *		GPDB additions to support hybrid hash aggregation algorithm.
 *		This file could be merged into nodeAgg.c.  The separation is
 *		only to help isolate Greenplum Database-only code from future merges with
 *		PG code.  Note, however, that nodeAgg.c is also modified to
 *		make use of this code.
 *
 * Portions Copyright (c) 2006-2007, Greenplum
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2005, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *    $Id$
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "miscadmin.h" /* work_mem */
#include "executor/executor.h"
#include "nodes/execnodes.h"
#include "executor/tuptable.h"
#include "executor/instrument.h"            /* Instrumentation */
#include "executor/execHHashagg.h"
#include "executor/execWorkfile.h"
#include "storage/bfz.h"
#include "utils/datum.h"
#include "utils/memutils.h"
#include "utils/lsyscache.h"
#include "utils/elog.h"
#include "cdb/memquota.h"
#include "utils/workfile_mgr.h"

#include "access/hash.h"

#include "cdb/cdbexplain.h"
#include "cdb/cdbvars.h"


#define HHA_MSG_LVL DEBUG2


/* Encapture data related to a batch file. */
struct BatchFileInfo
{
	int64 total_bytes;
	int64 ntuples;
	ExecWorkFile *wfile;
};

#define BATCHFILE_METADATA \
    (sizeof(BatchFileInfo) + sizeof(bfz_t) + sizeof(struct bfz_freeable_stuff))
#define FREEABLE_BATCHFILE_METADATA (sizeof(struct bfz_freeable_stuff))

/* Used for padding */
static char padding_dummy[MAXIMUM_ALIGNOF];

/*
 * Represent different types for input records to be inserted
 * into the hash table.
 */
typedef enum InputRecordType
{
	INPUT_RECORD_TUPLE = 0,
	INPUT_RECORD_GROUP_AND_AGGS,
} InputRecordType;

#define GET_BUFFER_SIZE(hashtable) \
	(mpool_total_bytes_allocated((hashtable)->group_buf))

#define GET_USED_BUFFER_SIZE(hashtable) \
	(mpool_bytes_used((hashtable)->group_buf))

#define SANITY_CHECK_METADATA_SIZE(hashtable) \
	do { \
		Assert((hashtable)->mem_for_metadata > 0); \
		Assert((hashtable)->mem_for_metadata > (hashtable)->nbuckets * OVERHEAD_PER_BUCKET); \
		if ((hashtable)->mem_for_metadata >= (hashtable)->max_mem) \
			ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), \
				errmsg(ERRMSG_GP_INSUFFICIENT_STATEMENT_MEMORY)));\
	} while (0)

#define GET_TOTAL_USED_SIZE(hashtable) \
		(GET_USED_BUFFER_SIZE(hashtable) + (hashtable)->mem_for_metadata)

#define AVAIL_MEM(hashtable) \
		(hashtable->max_mem - GET_TOTAL_USED_SIZE(hashtable))

#define HAVE_FREESPACE(hashtable) \
		(AVAIL_MEM(hashtable) > 0)

/* Actual memory needed per bucket = entry pointer + bloom value */
#define OVERHEAD_PER_BUCKET (sizeof(HashAggBucket) + sizeof(uint64))

#define BLOOMVAL(hashkey) ((uint64)1) << (((hashkey) >> 23) & 0x3f);

#define BUCKET_IDX(hashtable, hashkey) \
		(((hashkey) >> (hashtable)->pshift) & ((hashtable)->nbuckets - 1))

#define LOG2(x) (ceil(log((x)) / log(2)))

/* Methods that handle batch files */
static SpillSet *createSpillSet(unsigned branching_factor, unsigned parent_hash_bit);
static int closeSpillFile(AggState *aggstate, SpillSet *spill_set, int file_no);
static int closeSpillFiles(AggState *aggstate, SpillSet *spill_set);
static int suspendSpillFiles(SpillSet *spill_set);
static int32 writeHashEntry(AggState *aggstate,
							BatchFileInfo *file_info,
							HashAggEntry *entry);
static void *readHashEntry(AggState *aggstate,
						   BatchFileInfo *file_info,
						   HashKey *p_hashkey,
						   int32 *p_input_size);

/* Methods for hash table */
static uint32 calc_hash_value(AggState* aggstate, TupleTableSlot *inputslot);
static void spill_hash_table(AggState *aggstate);
static void expand_hash_table(AggState *aggstate);
static void init_agg_hash_iter(HashAggTable* ht);
static HashAggEntry *lookup_agg_hash_entry(AggState *aggstate, void *input_record,
										   InputRecordType input_type, int32 input_size,
										   uint32 hashkey, bool *p_isnew);
static void agg_hash_table_stat_upd(HashAggTable *ht);
static void reset_agg_hash_table(AggState *aggstate, int64 nentries);
static bool agg_hash_reload(AggState *aggstate);
static void reCalcNumberBatches(HashAggTable *hashtable, SpillFile *spill_file);
static inline void *mpool_cxt_alloc(void *manager, Size len);

static inline void *mpool_cxt_alloc(void *manager, Size len)
{
 	return mpool_alloc((MPool *)manager, len);
}

/* Function: calc_hash_value
 *
 * Calculate the hash value for the given input tuple.
 *
 * This based on but different from get_hash_value from the dynahash
 * API.  Use a different name to underline that we don't use dynahash.
 */
uint32
calc_hash_value(AggState* aggstate, TupleTableSlot *inputslot)
{
	Agg *agg;
	ExprContext *econtext;
	MemoryContext oldContext;
	int			i;
	FmgrInfo* info = aggstate->hashfunctions;
	HashAggTable *hashtable = aggstate->hhashtable;
	
	agg = (Agg*)aggstate->ss.ps.plan;
	econtext = aggstate->tmpcontext; /* short-lived, per-input-tuple */

	oldContext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

	for (i = 0; i < agg->numCols; i++, info++)
	{
		AttrNumber	att = agg->grpColIdx[i];
		bool isnull = false;
		Datum value = slot_getattr(inputslot, att, &isnull);

		if (!isnull)			/* treat nulls as having hash key 0 */
		{
			hashtable->hashkey_buf[i] = DatumGetUInt32(FunctionCall1(info, value));
		}
		
		else
			hashtable->hashkey_buf[i] = 0xdeadbeef;
	}

	MemoryContextSwitchTo(oldContext);
	return (uint32) hash_any((unsigned char *) hashtable->hashkey_buf, agg->numCols * sizeof(HashKey));
}

/* Function: adjustInputGroup
 *
 * Adjust the datum pointers stored in the byte array of an input group.
 */
static inline void
adjustInputGroup(AggState *aggstate, 
				 void *input_group)
{
	int32 tuple_size;
	void *datum;
	AggStatePerGroup pergroup;
	AggStatePerAgg peragg = aggstate->peragg;
	int aggno;
	
	tuple_size = memtuple_get_size((MemTuple)input_group);
	pergroup = (AggStatePerGroup) ((char *)input_group +
								   MAXALIGN(tuple_size));
	Assert(pergroup != NULL);
	datum = (char *)input_group + MAXALIGN(tuple_size) + 
		aggstate->numaggs * sizeof(AggStatePerGroupData);

	for (aggno = 0; aggno < aggstate->numaggs; aggno++)
	{
		AggStatePerAgg peraggstate = &peragg[aggno];
		AggStatePerGroup pergroupstate = &pergroup[aggno];
		
		if (!peraggstate->transtypeByVal &&
			!pergroupstate->transValueIsNull)
		{
			Size datum_size;
			pergroupstate->transValue = PointerGetDatum(datum);
			datum_size = datumGetSize(pergroupstate->transValue,
									  peraggstate->transtypeByVal,
									  peraggstate->transtypeLen);
			Assert(MAXALIGN(datum_size) - datum_size <= MAXIMUM_ALIGNOF);
			datum = (char *)datum + MAXALIGN(datum_size);
		}
	}
}

/* Function: getEmptyHashAggEntry
 *
 * Obtain a new empty HashAggEntry.
 */
static inline HashAggEntry *
getEmptyHashAggEntry(AggState *aggstate)
{
	return mpool_alloc(aggstate->hhashtable->group_buf, sizeof(HashAggEntry));
}

/* Function: makeHashAggEntryForInput
 *
 * Allocate a new hash agg entry for the given input tuple and hash key
 * of the given AggState. This includes installing the grouping key heap tuple.
 *
 * It is the caller's responsibility to link the entry into the hash table
 * and to initialize the per group data.
 *
 * If no enough memory is available, this function returns NULL.
 */
static HashAggEntry *
makeHashAggEntryForInput(AggState *aggstate, TupleTableSlot *inputslot, uint32 hashvalue)
{
	HashAggEntry *entry;
	MemoryContext oldcxt;
	HashAggTable *hashtable = aggstate->hhashtable;
	TupleTableSlot *hashslot = aggstate->hashslot;
	Datum *values = slot_get_values(aggstate->hashslot); 
	bool *isnull = slot_get_isnull(aggstate->hashslot); 
	ListCell *lc;
	uint32 tup_len;
	uint32 aggs_len;
	uint32 len;

	/*
	 * Extract the grouping columns from the inputslot, and store them into
	 * hashslot. The first integer in aggstate->hash_needed is the largest
	 * Var number for all grouping columns. 
	 */
	foreach (lc, aggstate->hash_needed)
	{
		const int n = lfirst_int(lc);
		values[n-1] = slot_getattr(inputslot, n, &(isnull[n-1])); 
	}

	tup_len = 0;
	aggs_len = aggstate->numaggs * sizeof(AggStatePerGroupData);
	
	oldcxt = MemoryContextSwitchTo(hashtable->entry_cxt);

	entry = getEmptyHashAggEntry(aggstate);
	entry->tuple_and_aggs = NULL;
	entry->hashvalue = hashvalue;
	entry->is_primodial = !(hashtable->is_spilling);
	entry->next = NULL;

	/*
	 * Copy memtuple into group_buf. Remember to always allocate
	 * enough space before calling ExecCopySlotMemTupleTo() because
	 * this function will call palloc() to allocate bigger space if
	 * the given one is not big enough, which is what we want to avoid.
	 */
	entry->tuple_and_aggs = (void *)memtuple_form_to(hashslot->tts_mt_bind,
													 values,
													 isnull,
													 entry->tuple_and_aggs,
													 &tup_len, false);
	Assert(tup_len > 0 && entry->tuple_and_aggs == NULL);

	if (GET_TOTAL_USED_SIZE(hashtable) + MAXALIGN(MAXALIGN(tup_len) + aggs_len) >=
		hashtable->max_mem)
		return NULL;

	entry->tuple_and_aggs = mpool_alloc(hashtable->group_buf,
										MAXALIGN(MAXALIGN(tup_len) + aggs_len));
	len = tup_len;
	entry->tuple_and_aggs = (void *)memtuple_form_to(hashslot->tts_mt_bind,
													 values,
													 isnull,
													 entry->tuple_and_aggs,
													 &len, false);
	Assert(len == tup_len && entry->tuple_and_aggs != NULL);

	MemoryContextSwitchTo(oldcxt);
	return entry;
}

/*
 * Function: makeHashAggEntryForGroup
 *
 * Allocate a new hash agg entry for the given byte array representing
 * group keys and aggregate values. This function will initialize the
 * per group data by pointing to the data stored on the given byte
 * array.
 *
 * This function assumes that the given byte array contains both a
 * memtuple that represents grouping keys, and their aggregate values,
 * stored in the format defined in writeHashEntry().
 *
 * It is the caller's responsibility to link the entry into the hash table.
 *
 * If no enough memory is available, this function returns NULL.
 */
static HashAggEntry *
makeHashAggEntryForGroup(AggState *aggstate, void *tuple_and_aggs,
						 int32 input_size, uint32 hashvalue)
{
	HashAggEntry *entry;
	HashAggTable *hashtable = aggstate->hhashtable;
	void *copy_tuple_and_aggs;

	MemoryContext oldcxt;

	if (GET_TOTAL_USED_SIZE(hashtable) + input_size >= hashtable->max_mem)
		return NULL;

	copy_tuple_and_aggs = mpool_alloc(hashtable->group_buf, input_size);
	memcpy(copy_tuple_and_aggs, tuple_and_aggs, input_size);

	oldcxt = MemoryContextSwitchTo(hashtable->entry_cxt);
	
	entry = getEmptyHashAggEntry(aggstate);
	entry->hashvalue = hashvalue;
	entry->is_primodial = !(hashtable->is_spilling);
	entry->tuple_and_aggs = copy_tuple_and_aggs;
	entry->next = NULL;

	/* Initialize per group data */
	adjustInputGroup(aggstate, entry->tuple_and_aggs);

	MemoryContextSwitchTo(oldcxt);

	return entry;
}

/*
 * Function: setGroupAggs
 *
 * Set the groupaggs buffer in the hashtable to point to the right place
 * in the given hash entry.
 */
static inline void
setGroupAggs(HashAggTable *hashtable, HashAggEntry *entry)
{
	if (entry != NULL)
	{
		int tup_len = memtuple_get_size((MemTuple)entry->tuple_and_aggs);
		hashtable->groupaggs->tuple = (MemTuple)entry->tuple_and_aggs;
		hashtable->groupaggs->aggs = (AggStatePerGroup)
			((char *)entry->tuple_and_aggs + MAXALIGN(tup_len));
	}
}

/*
 * Function: lookup_agg_hash_entry
 *
 * Returns a pointer to the old or new hash table entry corresponding
 * to the input record, or NULL if there is no such an entry (and
 * the table is full).
 *
 * The input record can be one of the following two types:
 *   TableTupleSlot -- representing an input tuple
 *   a byte array -- representing a contiguous space that contains
 *                   both group keys and aggregate values.
 *                   'input_size' represents how many bytes this byte array has.
 *
 * If an entry is returned and isNew is non-NULL, (*p_isnew) is set to true
 * or false depending on whether the returned entry is new.  Note that
 * a new entry will have *initialized* per-group data (Aggref states).
 */
static HashAggEntry *
lookup_agg_hash_entry(AggState *aggstate,
					  void *input_record,
					  InputRecordType input_type, int32 input_size,
					  uint32 hashkey, bool *p_isnew)
{
	HashAggEntry *entry;
	HashAggTable *hashtable = aggstate->hhashtable;
	MemTupleBinding *mt_bind = aggstate->hashslot->tts_mt_bind;
	ExprContext *tmpcontext = aggstate->tmpcontext; /* per input tuple context */
	Agg *agg = (Agg*)aggstate->ss.ps.plan;
	MemoryContext oldcxt;
	unsigned int bucket_idx;
	uint64 bloomval;			/* bloom filter value */
   
	Assert(mt_bind != NULL);

	if (p_isnew != NULL)
		*p_isnew = false;

	oldcxt = MemoryContextSwitchTo(tmpcontext->ecxt_per_tuple_memory);

	bucket_idx = BUCKET_IDX(hashtable, hashkey);
	bloomval = BLOOMVAL(hashkey);
	entry = (0 == (hashtable->bloom[bucket_idx] & bloomval) ? NULL :
			 hashtable->buckets[bucket_idx]);

	/*
	 * Search entry chain for the bucket. If such an entry found in the
	 * chain, move it to the front of the chain. Otherwise, if there
	 * are any space left, create a new entry, and insert it in
	 * the front of the chain.
	 */
	while (entry != NULL)
	{
		MemTuple mtup = (MemTuple) entry->tuple_and_aggs;
		int i;
		bool match = true;

		if (hashkey != entry->hashvalue)
		{
			entry = entry->next;
			continue;
		}
		
		for (i = 0; match && i < agg->numCols; i++)
		{
			AttrNumber	att = agg->grpColIdx[i];
			Datum input_datum = 0;
			Datum entry_datum = 0;
			bool input_isNull = false;
			bool entry_isNull = false;
				
			switch(input_type)
			{
				case INPUT_RECORD_TUPLE:
					input_datum = slot_getattr((TupleTableSlot *)input_record, att, &input_isNull);
					break;
				case INPUT_RECORD_GROUP_AND_AGGS:
					input_datum = memtuple_getattr((MemTuple)input_record, mt_bind, att, &input_isNull);
					break;
				default:
					insist_log(false, "invalid record type %d", input_type);
			}

			entry_datum = memtuple_getattr(mtup, mt_bind, att, &entry_isNull);

			if ( !input_isNull && !entry_isNull &&
				 (DatumGetBool(FunctionCall2(&aggstate->eqfunctions[i],
											 input_datum,
											 entry_datum)) ) )
				continue; /* Both non-NULL and equal. */
			match = (input_isNull && entry_isNull);/* NULLs match in group keys. */
		}
		
		/* Break if found an existing matching entry. */
		if (match)
			break;

		entry = entry->next;
	}

	if (entry == NULL)
	{
		/* Entry not found! Create a new matching entry. */
		switch(input_type)
		{
			case INPUT_RECORD_TUPLE:
				entry = makeHashAggEntryForInput(aggstate, (TupleTableSlot *)input_record, hashkey);
				break;
			case INPUT_RECORD_GROUP_AND_AGGS:
				entry = makeHashAggEntryForGroup(aggstate, input_record, input_size, hashkey);
				break;
			default:
				insist_log(false, "invalid record type %d", input_type);
		}
			
		if (entry != NULL)
		{
			if (hashtable->expandable &&
					hashtable->num_entries >= (hashtable->nbuckets * gp_hashagg_groups_per_bucket))
			{
				/* The hashtable is denser than envisioned; increase the number of buckets */
				expand_hash_table(aggstate);
				/* Recompute bucket_idx in case nbuckets changed */
				bucket_idx = BUCKET_IDX(hashtable, hashkey);
			}

			entry->next = hashtable->buckets[bucket_idx];
			hashtable->buckets[bucket_idx] = entry;
			hashtable->bloom[bucket_idx] |= bloomval;
			
			++hashtable->num_ht_groups;
			++hashtable->num_entries;

			*p_isnew = true; /* created a new entry */
		}
		/*
		  else no matching entry, and no room to create one. 
		*/
	}

	(void) MemoryContextSwitchTo(oldcxt);

	return entry;
}

/*
 * Compute HHashTable entry size
 *
 * int numaggs    Est # of aggregate functions.
 * int keywidth   Est per entry size of hash key.
 * int transpace  Est per entry size of by-ref values.
 */
double
agg_hash_entrywidth(int numaggs, int keywidth, int transpace)
{
	return sizeof(HashAggEntry)
		+ numaggs * sizeof(AggStatePerGroupData)
		+ keywidth
		+ transpace;
}

/* Function: calcHashAggTableSizes
 *
 * Check if the current memory quota is enough to handle the aggregation
 * in the hash-based fashion.
 */
bool
calcHashAggTableSizes(double memquota,	/* Memory quota in bytes. */
					  double ngroups,	/* Est # of groups. */
					  double entrywidth, /* calculcate from agg_hash_entrywidth */
					  bool force,      /* true => succeed even if work_mem too small */
					  HashAggTableSizes   *out_hats)
{
	double entrysize, nbuckets, nentries;

	/* Assume we don't need to spill */
	bool expectSpill = false;
	double nbatches = 0, batchfile_mem = 0, entries_mem = 0, buckets_mem = 0;

	Assert(ngroups >= 0);

	/* Estimate the overhead per entry in the hash table */
	entrysize = entrywidth + OVERHEAD_PER_BUCKET / (double) gp_hashagg_groups_per_bucket;

	elog(HHA_MSG_LVL, "HashAgg: ngroups = %g, memquota = %g, entrysize = %g",
		 ngroups, memquota, entrysize);

	/*
	 * When all groups can not fit in the memory, we compute
	 * the number of batches to store spilled groups. Currently, we always
	 * set the number of batches to gp_hashagg_default_nbatches.
	 */			
	if (memquota < ngroups*entrysize)
	{
		nbatches = gp_hashagg_default_nbatches;
		batchfile_mem = BATCHFILE_METADATA * (1 + nbatches);
		expectSpill = true;

		/*
		 * If the memory quota is smaller than the overhead for batch files,
		 * return false. Note that we will always keep at most (nbatches + 1)
		 * batches in the memory.
		 */
		if (memquota < batchfile_mem)
		{
			elog(HHA_MSG_LVL, "HashAgg: not enough memory for the overhead of batch files.");
			return false;
		}
	}

	/* Reserve memory for batch files (if any are expected) */
	memquota -= batchfile_mem;

	/* How many entries will memquota allow ? */
	nentries = floor(memquota / entrysize);

	/* Yet, allocate only as many as needed */
	nentries = Min(ngroups, nentries);

	/* but at least a few hash entries as required */
	nentries = Max(nentries, gp_hashagg_groups_per_bucket);
	entries_mem = nentries * entrywidth;

	/*
	 * If the memory quota is smaller than the minimum number of entries
	 * required, return false
	 */
	if ((memquota - entries_mem) <= 0)
	{
		elog(HHA_MSG_LVL, "HashAgg: not enough memory for the overhead of buckets.");
		return false;
	}

	memquota -= entries_mem;

	/* Determine the number of buckets */
	nbuckets = ceil(nentries / gp_hashagg_groups_per_bucket);

	/* Use only as many allowed by memory */
	nbuckets = Min(nbuckets, floor(memquota / OVERHEAD_PER_BUCKET));

	/* Set nbuckets to the next power of 2. */
	nbuckets = (((unsigned)1) << ((unsigned) LOG2(nbuckets)));

	if ((nbuckets * OVERHEAD_PER_BUCKET) > memquota)
	{
		/*
		 * If the current nentries and nbuckets will make us go OOM, nbuckets was
		 * rounded up too high. Reduce the nbuckets to a lower power of 2
		 */
		nbuckets = nbuckets / 2;
	}

	/*
	 * Always set nbuckets greater than gp_hashagg_default_nbatches since
	 * the spilling relies on this fact to choose which files to spill
	 * groups to.
	 * Note: gp_hashagg_default_nbatches must be a power of two
	 */
	nbuckets = Max(nbuckets, gp_hashagg_default_nbatches);
	buckets_mem = nbuckets * OVERHEAD_PER_BUCKET;

	/* Reserve memory for the entries + hash table */
	memquota -= buckets_mem;

	if (memquota < 0)
	{
		elog(HHA_MSG_LVL, "HashAgg: not enough memory for the hash table parameters chosen:");
		elog(HHA_MSG_LVL, "HashAgg: nbuckets = %d, nentries = %d, nbatches = %d",
			 (int)nbuckets, (int)nentries, (int)nbatches);
		elog(HHA_MSG_LVL, "HashAgg: ngroups = %d, gp_hashagg_groups_per_bucket = %d",
			 (int)ngroups, (int)gp_hashagg_groups_per_bucket);
		return false;
	}

	if (nbatches > UINT_MAX || nentries > UINT_MAX || nbuckets > UINT_MAX)
	{
		if (force)
		{
			insist_log(false, "too many passes or hash entries");
		}
		else
		{
			elog(HHA_MSG_LVL, "HashAgg: number of passes or hash entries bigger than int type!");
			return false; /* Too many groups. */
		}
	}

	if (out_hats)
	{
		out_hats->nbuckets = (unsigned)nbuckets;
		out_hats->nentries = (unsigned)nentries;
		out_hats->nbatches = (unsigned)nbatches;
		out_hats->hashentry_width = entrywidth;
		out_hats->spill = expectSpill;
		out_hats->workmem_initial = (unsigned)(batchfile_mem);
		out_hats->workmem_per_entry = (unsigned) entrysize;
	}
	
	elog(HHA_MSG_LVL, "HashAgg: nbuckets = %d, nentries = %d, nbatches = %d",
		 (int)nbuckets, (int)nentries, (int)nbatches);
	elog(HHA_MSG_LVL, "HashAgg: expected memory footprint = %d",
		(int)(batchfile_mem + buckets_mem + entries_mem));
	
	return true;
}

/* Function: est_hash_tuple_size
 *
 * Estimate the average memory requirement for the grouping key HeapTuple
 * in a hash table entry.
 *
 * This function purports to know (perhaps too much) about the format in
 * which a tuple representing a grouping key (non-aggregated attrbutes,
 * actually) will be stored.  But it's all guess work, so no sense being
 * too fussy.
 */
static Size est_hash_tuple_size(TupleTableSlot *hashslot, List *hash_needed)
{
	Form_pg_attribute *attrs;
	Form_pg_attribute attr;
	int natts;
	ListCell *lc;
	Size len;
	TupleDesc tupleDescriptor;
	
	tupleDescriptor = hashslot->tts_tupleDescriptor;
	attrs = tupleDescriptor->attrs;
	natts = tupleDescriptor->natts;
	
	len = offsetof(HeapTupleHeaderData, t_bits);
	len += BITMAPLEN(natts);
	
	if (tupleDescriptor->tdhasoid)
		len += sizeof(Oid);
	
	len = MAXALIGN(len);
	
	/* Add data sizes to len. */
	foreach( lc, hash_needed )
	{
		int varNumber = lfirst_int(lc) - 1;
		attr = attrs[varNumber];
		
		Assert( !attr->attisdropped );
		
		len = att_align_nominal(len, attr->attalign);
		len += get_typavgwidth(attr->atttypid, attr->atttypmod);
	}

	len = MAXALIGN(HEAPTUPLESIZE + len);

	elog(HHA_MSG_LVL, "HashAgg: estimated hash tuple size is %d", (int)len);
	
	return len;
}

/* Function: create_agg_hash_table
 *
 * Creates and initializes a hash table for the given AggState.  Should be
 * called after the rest of the AggState is initialized.  The resulting table
 * should be installed in the AggState.
 *
 * The main control structure for the hash table is allocated in the memory
 * context aggstate->aggcontext as is the bucket array, the hashtable and the items related
 * to overflow files.  
 */
HashAggTable *
create_agg_hash_table(AggState *aggstate)
{
	int keywidth;
	double entrywidth;
	HashAggTable *hashtable;
	Agg *agg = (Agg *)aggstate->ss.ps.plan;
	MemoryContext oldcxt;

	oldcxt = MemoryContextSwitchTo(aggstate->aggcontext);
	hashtable = (HashAggTable *) palloc0(sizeof(HashAggTable));

	hashtable->entry_cxt = AllocSetContextCreate(aggstate->aggcontext, 
												 "HashAggTableEntryContext",
												 ALLOCSET_DEFAULT_MINSIZE,
												 ALLOCSET_DEFAULT_INITSIZE,
												 ALLOCSET_DEFAULT_MAXSIZE);

	uint64 operatorMemKB = PlanStateOperatorMemKB( (PlanState *) aggstate);

	keywidth = est_hash_tuple_size(aggstate->ss.ss_ScanTupleSlot, aggstate->hash_needed);
	keywidth = Min(keywidth, agg->plan.plan_width);

	entrywidth = agg_hash_entrywidth(aggstate->numaggs, keywidth, agg->transSpace);

	if (!calcHashAggTableSizes(1024.0 * (double) operatorMemKB,
							agg->numGroups,
							entrywidth,
							true,
							&(hashtable->hats)))
	{
		elog(ERROR, ERRMSG_GP_INSUFFICIENT_STATEMENT_MEMORY);
	}

	/* Initialize the hash buckets */
	hashtable->nbuckets = hashtable->hats.nbuckets;
	hashtable->buckets = (HashAggBucket *) palloc0(hashtable->nbuckets * sizeof(HashAggBucket));
	hashtable->bloom = (uint64 *) palloc0(hashtable->nbuckets * sizeof(uint64));

	hashtable->pshift = 0;
	hashtable->expandable = true;

	MemoryContextSwitchTo(hashtable->entry_cxt);
	
	/* Initialize buffer for hash entries */
	hashtable->group_buf = mpool_create(hashtable->entry_cxt,
										"GroupsAndAggs Context");
	hashtable->groupaggs = (GroupKeysAndAggs *) palloc0(sizeof(GroupKeysAndAggs));

	/* Set hashagg's memory manager */
	aggstate->mem_manager.alloc = mpool_cxt_alloc;
	aggstate->mem_manager.free = NULL;
	aggstate->mem_manager.manager = hashtable->group_buf;
	aggstate->mem_manager.realloc_ratio = 2;

	MemoryContextSwitchTo(oldcxt);

	hashtable->max_mem = 1024.0 * operatorMemKB;
	hashtable->mem_for_metadata = sizeof(HashAggTable) +
			hashtable->nbuckets * OVERHEAD_PER_BUCKET +
			sizeof(GroupKeysAndAggs);
	hashtable->mem_wanted = hashtable->mem_for_metadata;
	hashtable->mem_used = hashtable->mem_for_metadata;

	hashtable->prev_slot = NULL;

	MemSet(padding_dummy, 0, MAXIMUM_ALIGNOF);
	
	init_agg_hash_iter(hashtable);

	Assert(hashtable->mem_for_metadata > 0);

	return hashtable;
}

/* Function: agg_hash_initial_pass
 *
 * Performs ExecAgg initialization for the first pass of the hashed case:
 * - reads the input tuples,
 * - builds a hash table with an entry per group,
 * - spills all groups in the hash table to several overflow batches
 *   to be processed during later passes.
 *
 * Note that overflowed groups are distributed to batches in such
 * a way that groups with matching grouping keys will be in the same
 * batch.
 *
 * When called, CurrentMemoryContext should be the per-query context.
 */
bool
agg_hash_initial_pass(AggState *aggstate)
{
	HashAggTable *hashtable = aggstate->hhashtable;
	ExprContext *tmpcontext = aggstate->tmpcontext; /* per input tuple context */
	TupleTableSlot *outerslot = NULL;
	bool streaming = ((Agg *) aggstate->ss.ps.plan)->streaming;
	bool tuple_remaining = true;

	Assert(hashtable);
	AssertImply(!streaming, aggstate->hashaggstatus == HASHAGG_BEFORE_FIRST_PASS);
	elog(HHA_MSG_LVL,
		 "HashAgg: initial pass -- beginning to load hash table");

	/*
	 * Check if an input tuple has been read, but not processed
	 * because of lack of space before streaming the results
	 * in the last call.
	 */
	if (aggstate->hashslot->tts_tupleDescriptor != NULL &&
		hashtable->prev_slot != NULL)
	{
		outerslot = hashtable->prev_slot;
		hashtable->prev_slot = NULL;
	}
	else
	{
		outerslot = ExecProcNode(outerPlanState(aggstate));
	}

	/*
	 * Process outer-plan tuples, until we exhaust the outer plan.
	 */
	while(true)
	{
		HashKey hashkey;
		bool isNew;
		HashAggEntry *entry;

		/* no more tuple. Done */
		if (TupIsNull(outerslot))
		{
			tuple_remaining = false;
			break;
		}

		if (aggstate->hashslot->tts_tupleDescriptor == NULL)
		{
			int size;
							
			/* Initialize hashslot by cloning input slot. */
			ExecSetSlotDescriptor(aggstate->hashslot, outerslot->tts_tupleDescriptor); 
			ExecStoreAllNullTuple(aggstate->hashslot);

			size = ((Agg *)aggstate->ss.ps.plan)->numCols * sizeof(HashKey);
			
			hashtable->hashkey_buf = (HashKey *)palloc0(size);
			hashtable->mem_for_metadata += size;
		}

		/* set up for advance_aggregates call */
		tmpcontext->ecxt_outertuple = outerslot;

		/* Find or (if there's room) build a hash table entry for the
		 * input tuple's group. */
		hashkey = calc_hash_value(aggstate, outerslot);
		entry = lookup_agg_hash_entry(aggstate, (void *)outerslot,
									  INPUT_RECORD_TUPLE, 0, hashkey, &isNew);
		
		if (entry == NULL)
		{
			if (GET_TOTAL_USED_SIZE(hashtable) > hashtable->mem_used)
				hashtable->mem_used = GET_TOTAL_USED_SIZE(hashtable);

			if (hashtable->num_ht_groups <= 1)
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
								 ERRMSG_GP_INSUFFICIENT_STATEMENT_MEMORY));
			
			/*
			 * If stream_bottom is on, we store outerslot into hashslot, so that
			 * we can process it later.
			 */
			if (streaming)
			{
				Assert(tuple_remaining);
				hashtable->prev_slot = outerslot;
				/* Stream existing entries instead of spilling */
				break;
			}

			if (!hashtable->is_spilling && aggstate->ss.ps.instrument)
			{
				/* Update in-memory hash table statistics before spilling. */
				agg_hash_table_stat_upd(hashtable);
			}

			spill_hash_table(aggstate);

			entry = lookup_agg_hash_entry(aggstate, (void *)outerslot,
										  INPUT_RECORD_TUPLE, 0, hashkey, &isNew);
		}

		setGroupAggs(hashtable, entry);
		
		if (isNew)
		{
			int tup_len = memtuple_get_size((MemTuple)entry->tuple_and_aggs);
			MemSet((char *)entry->tuple_and_aggs + MAXALIGN(tup_len), 0,
				   aggstate->numaggs * sizeof(AggStatePerGroupData));
			initialize_aggregates(aggstate, aggstate->peragg, hashtable->groupaggs->aggs,
								  &(aggstate->mem_manager));
		}
			
		/* Advance the aggregates */
		call_AdvanceAggregates(aggstate, hashtable->groupaggs->aggs, &(aggstate->mem_manager));
		
		hashtable->num_tuples++;

		/* Reset per-input-tuple context after each tuple */
		ResetExprContext(tmpcontext);

		if (streaming && !HAVE_FREESPACE(hashtable))
		{
			Assert(tuple_remaining);
			ExecClearTuple(aggstate->hashslot);
			/* Pause and stream entries before reading the next tuple */
			break;
		}

		/* Read the next tuple */
		outerslot = ExecProcNode(outerPlanState(aggstate));
	}

	if (GET_TOTAL_USED_SIZE(hashtable) > hashtable->mem_used)
		hashtable->mem_used = GET_TOTAL_USED_SIZE(hashtable);

	if (hashtable->is_spilling)
	{
		int freed_size = 0;

		/*
		 * Split out the rest of groups in the hashtable if spilling has already
		 * happened. This is because none of these groups can be immediately outputted
		 * any more.
		 */
		spill_hash_table(aggstate);
		freed_size = suspendSpillFiles(hashtable->spill_set);
		hashtable->mem_for_metadata -= freed_size;

		Assert(hashtable->mem_for_metadata > 0);

		if (aggstate->ss.ps.instrument)
		{
			aggstate->ss.ps.instrument->workfileCreated = true;
		}
	}

	if (!hashtable->is_spilling && aggstate->ss.ps.instrument)
	{
		/* Update in-memory hash table statistics if not already done when spilling */
		agg_hash_table_stat_upd(hashtable);
	}

	AssertImply(tuple_remaining, streaming);
	if(tuple_remaining) 
		elog(HHA_MSG_LVL, "HashAgg: streaming out the intermediate results.");

	return tuple_remaining;
}

/* Create a spill set for the given branching_factor (a power of two) 
 * and hash key range.
 *
 * Use the arguments to derive break values.  All but the first spill
 * file carries the minimum hash key value that may appear in the file.  
 * The minimum value of the first spill file in a set is unused, but is
 * set to the minimum of the spill set (debugging). The break values are 
 * chosen to distribute hash key values evenly across the given range.
 */
static SpillSet *
createSpillSet(unsigned branching_factor, unsigned parent_hash_bit)
{
	int i;
	SpillSet *spill_set;

	/* Allocate and initialize the SpillSet. */
	spill_set = palloc(sizeof(SpillSet) + (branching_factor-1) * sizeof (SpillFile));
	spill_set->parent_spill_file = NULL;
	spill_set->level = 0;
	spill_set->num_spill_files = branching_factor;
	
	/* Allocate and initialize its SpillFiles. */
	for ( i = 0; i < branching_factor; i++ )
	{
		SpillFile *spill_file = &spill_set->spill_files[i];
		
		spill_file->file_info = NULL;
		spill_file->spill_set = NULL;
		spill_file->parent_spill_set = spill_set;
		spill_file->index_in_parent = i;
		spill_file->respilled = false;
		spill_file->batch_hash_bit = parent_hash_bit;
	}

	elog(HHA_MSG_LVL, "HashAgg: created a new spill set with batch_hash_bit=%u, num_spill_files=%u",
	     parent_hash_bit, branching_factor);

	return spill_set;
}

/*
 * Free the space for a given SpillSet, and return the bytes that are freed.
 */
static inline int
freeSpillSet(SpillSet *spill_set)
{
	int freedspace = 0;
	if (spill_set == NULL)
	{
		return freedspace;
	}
	
	elog(gp_workfile_caching_loglevel, "freeing up SpillSet with %d files", spill_set->num_spill_files);

	freedspace += sizeof(SpillSet) + (spill_set->num_spill_files - 1) * sizeof (SpillFile);
	pfree(spill_set);

	return freedspace;
}


/* Get the spill file to which to spill a hash entry with the given key.
 *
 *
 * If the temporary file has not been created for this spill file, it
 * will be created in this function. The buffer space required for
 * this temporary file is returned through 'p_alloc_size'.
 */
static SpillFile *
getSpillFile(workfile_set *work_set, SpillSet *set, int file_no, int *p_alloc_size)
{
	SpillFile *spill_file;

	*p_alloc_size = 0;

	/* Find the right spill file */
	Assert(set != NULL);
	spill_file = &set->spill_files[file_no];

	if (spill_file->file_info == NULL)
	{

		spill_file->file_info = (BatchFileInfo *)palloc(sizeof(BatchFileInfo));
		spill_file->file_info->total_bytes = 0;
		spill_file->file_info->ntuples = 0;
		/* Initialize to NULL in case the create function below throws an exception */
		spill_file->file_info->wfile = NULL; 
		spill_file->file_info->wfile = workfile_mgr_create_file(work_set);

		elog(HHA_MSG_LVL, "HashAgg: create %d level batch file %d with compression %d",
			 set->level, file_no, work_set->metadata.bfz_compress_type);

		*p_alloc_size = BATCHFILE_METADATA;
	}

	return spill_file;
}

/*
 * suspendSpillFiles -- temporary suspend all spill files so that we
 * can have more space for the hash table.
 */
static int
suspendSpillFiles(SpillSet *spill_set)
{
	int file_no;
	int freed_size = 0;
	
	if (spill_set == NULL ||
		spill_set->num_spill_files == 0)
		return 0;

	for (file_no = 0; file_no < spill_set->num_spill_files; file_no++)
	{
		SpillFile *spill_file = &spill_set->spill_files[file_no];
	
		if (spill_file->file_info &&
			spill_file->file_info->wfile != NULL)
		{
			ExecWorkFile_Suspend(spill_file->file_info->wfile);

			freed_size += FREEABLE_BATCHFILE_METADATA;

			elog(HHA_MSG_LVL, "HashAgg: %s contains " INT64_FORMAT " entries ("
				 INT64_FORMAT " bytes)",
				 spill_file->file_info->wfile->fileName,
				 spill_file->file_info->ntuples, spill_file->file_info->total_bytes);
		}
	}

	return freed_size;
}

/*
 * closeSpillFile -- close a given spill file and return its freed buffer
 * space. All files under its spill_set are also closed.
 */
static int
closeSpillFile(AggState *aggstate, SpillSet *spill_set, int file_no)
{
	int freedspace = 0;
	SpillFile *spill_file;
	HashAggTable *hashtable = aggstate->hhashtable;
	
	Assert(spill_set != NULL && file_no < spill_set->num_spill_files);

	spill_file = &spill_set->spill_files[file_no];

	if (spill_file->spill_set != NULL)
	{
		freedspace += closeSpillFiles(aggstate, spill_file->spill_set);
	}
	
	if (spill_file->file_info &&
		spill_file->file_info->wfile != NULL)
	{
		workfile_mgr_close_file(hashtable->work_set, spill_file->file_info->wfile);
		spill_file->file_info->wfile = NULL;
		freedspace += (BATCHFILE_METADATA - sizeof(BatchFileInfo));
		
	}
	if (spill_file->file_info)
	{
		pfree(spill_file->file_info);
		spill_file->file_info = NULL;
		freedspace += sizeof(BatchFileInfo);
	}

	return freedspace;
}

/*
 * closeSpillFiles -- close all spill files for a given spill set to
 * save the buffer space, and return how much space freed.
 */
static int
closeSpillFiles(AggState *aggstate, SpillSet *spill_set)
{
	int file_no;
	int freedspace = 0;
	
	if (spill_set == NULL ||
		spill_set->num_spill_files == 0)
		return 0;

	for (file_no = 0; file_no < spill_set->num_spill_files; file_no++)
	{
		freedspace += closeSpillFile(aggstate, spill_set, file_no);
	}

	return freedspace;
}

/*
 * Obtain the spill set to which the overflown entries are spilled.
 *
 * If such a spill set does not exist, it is created here.
 *
 * The statistics in the hashtable is also updated.
 */
static SpillSet *
obtain_spill_set(HashAggTable *hashtable)
{
	SpillSet **p_spill_set;
	unsigned set_hash_bit = 0;
	
	if (hashtable->curr_spill_file != NULL)
	  {
		SpillSet *parent_spill_set;
		p_spill_set = &(hashtable->curr_spill_file->spill_set);
		parent_spill_set = hashtable->curr_spill_file->parent_spill_set;
		Assert(parent_spill_set != NULL);
		unsigned parent_hash_bit = hashtable->curr_spill_file->batch_hash_bit;
		set_hash_bit = parent_hash_bit +
			(unsigned) LOG2(parent_spill_set->num_spill_files);

	  }
	else
		p_spill_set = &(hashtable->spill_set);

	if (*p_spill_set == NULL)
	{
		/*
		 * The optimizer may estimate that there is no need for a spill. However,
		 * it is wrong in this case. We need to set nbatches to its rightful value.
		 */
		if (hashtable->hats.nbatches == 0)
		{
			elog(DEBUG2, "Not all groups fit into memory; writing to disk");
			hashtable->hats.nbatches = gp_hashagg_default_nbatches;
		}

		*p_spill_set = createSpillSet(hashtable->hats.nbatches, set_hash_bit);

		hashtable->num_overflows++;
		hashtable->mem_for_metadata +=
			sizeof(SpillSet) +
			(hashtable->hats.nbatches - 1) * sizeof(SpillFile);

		SANITY_CHECK_METADATA_SIZE(hashtable);

		if (hashtable->curr_spill_file != NULL)
		{
			hashtable->curr_spill_file->spill_set->level =
				hashtable->curr_spill_file->parent_spill_set->level + 1;
			hashtable->curr_spill_file->spill_set->parent_spill_file =
				hashtable->curr_spill_file;
		}
	}

	return *p_spill_set;
}

/* Spill all entries from the hash table to file in order to make room
 * for new hash entries.
 *
 * We want to maximize the sequential writes to each spill file. Since
 * the number of buckets and the number of batches (#batches) are the power of 2,
 * We simply write bucket 0, #batches, 2 * #batches, ... to the batch 0;
 * write bucket 1, (#batches + 1), (2 * #batches + 1), ... to the batch 1;
 * and etc.
 */
static void
spill_hash_table(AggState *aggstate)
{
	HashAggTable *hashtable = aggstate->hhashtable;
	elog(HHA_MSG_LVL, "Spilling hash table at %ld entries", hashtable->num_entries);
	SpillSet *spill_set;
	SpillFile *spill_file;
	int bucket_no;
	int file_no;
	MemoryContext oldcxt;
	uint64 old_num_spill_groups = hashtable->num_spill_groups;

	spill_set = obtain_spill_set(hashtable);

	oldcxt = MemoryContextSwitchTo(hashtable->entry_cxt);

	/* Spill set does not have a workfile_set. Use existing or create new one as needed */
	if (hashtable->work_set == NULL)
	{
		hashtable->work_set = workfile_mgr_create_set(BFZ, true /* can_be_reused */, &aggstate->ss.ps);
		hashtable->work_set->metadata.buckets = hashtable->nbuckets;
		//aggstate->workfiles_created = true;
	}

	/* Book keeping. */
	hashtable->is_spilling = true;

	Assert(hashtable->nbuckets > spill_set->num_spill_files);

	/*
	 * Write each spill file. Write the last spill file first, since it will
	 * be processed the last.
	 */
	for (file_no = spill_set->num_spill_files - 1; file_no >= 0; file_no--)
	{
		int alloc_size = 0;

		/*
		 * getSpillFile() requires a large amount of memory (BATCHFILE_METADATA =
		 * ~16 KB) per spill file it creates.  Since the hash table is already out
		 * of memory when spilling, it will have to overuse memory than is allowed.
		 * This could not have been pre-allocated that would have wasted memory,
		 * allowing us not to spill in the first place.
		 * FIXME : The amount of memory required at this point can be reduced
		 * substantially by suspending the file after it's written to (see
		 * suspendSpillFiles).
		 */
		spill_file = getSpillFile(hashtable->work_set, spill_set, file_no, &alloc_size);
		Assert(spill_file != NULL);
			
		hashtable->mem_for_metadata += alloc_size;
		if (alloc_size > 0)
		{
			SANITY_CHECK_METADATA_SIZE(hashtable);

			hashtable->num_batches++;
			
			CheckSendPlanStateGpmonPkt(&aggstate->ss.ps);
		}

		for (bucket_no = file_no; bucket_no < hashtable->nbuckets;
			 bucket_no += spill_set->num_spill_files)
		{
			HashAggEntry *entry = hashtable->buckets[bucket_no];
			
			/* Ignore empty chains. */
			if (entry == NULL) continue;
			
			/* Write all entries in the hash chain. */
			while ( entry  != NULL )
			{
				HashAggEntry *spill_entry = entry;
				entry = spill_entry->next;

				if (spill_entry != NULL)
				{
					int32 written_bytes;
					
					written_bytes = writeHashEntry(aggstate, spill_file->file_info, spill_entry);
					spill_file->file_info->ntuples++;
					spill_file->file_info->total_bytes += written_bytes;

					hashtable->num_spill_groups++;
				}
			}

			hashtable->buckets[bucket_no] = NULL;
			hashtable->bloom[bucket_no] = 0;
		}
	}

	/* Reset the buffer */
	mpool_reset(hashtable->group_buf);

	/* Reset in-memory entries count */
	hashtable->num_entries = 0;

	elog(HHA_MSG_LVL, "HashAgg: spill " INT64_FORMAT " groups",
		 hashtable->num_spill_groups - old_num_spill_groups);

	MemoryContextSwitchTo(oldcxt);
}

static void
expand_hash_table(AggState *aggstate)
{
	unsigned mem_needed, old_nbuckets, bucket_idx, new_bucket_idx;
	uint32 hashkey;
	uint64 bloomval;
	HashAggEntry *entry, *nextentry;
	HashAggTable *hashtable = aggstate->hhashtable;

#ifdef USE_ASSERT_CHECKING
	unsigned nentries = 0;
#endif

	Assert(hashtable);
	old_nbuckets = hashtable->nbuckets;

	/* Make sure there is memory available for additional buckets */
	mem_needed = old_nbuckets * OVERHEAD_PER_BUCKET;
	if (mem_needed > AVAIL_MEM(hashtable) || hashtable->nbuckets > (UINT_MAX / 2))
	{
		/* Cannot double the buckets if there is not enough space */
		elog(HHA_MSG_LVL, "HashAgg: cannot grow the number of buckets!");
		elog(HHA_MSG_LVL, "HashAgg: mem needed = %d available = %d; nbuckets = %d",
				mem_needed, (unsigned) AVAIL_MEM(hashtable), hashtable->nbuckets);
		hashtable->expandable = false;
		return;
	}
	elog(HHA_MSG_LVL, "Growing the hash table to %d buckets with %ld entries",
			hashtable->nbuckets * 2, hashtable->num_entries);

	Assert(hashtable->nbuckets > 1);

	/* OK, do it */

	hashtable->nbuckets = hashtable->nbuckets * 2;
	hashtable->mem_for_metadata += old_nbuckets * OVERHEAD_PER_BUCKET;
	hashtable->mem_wanted = Max(hashtable->mem_wanted, hashtable->mem_for_metadata);

	Assert(GET_TOTAL_USED_SIZE(hashtable) < hashtable->max_mem);

	hashtable->buckets = (HashAggBucket *) repalloc(hashtable->buckets,
		hashtable->nbuckets * sizeof(HashAggBucket));
	hashtable->bloom =  (uint64 *) repalloc(hashtable->bloom,
		hashtable->nbuckets * sizeof(uint64));

	memset(hashtable->bloom, 0, hashtable->nbuckets * sizeof(uint64));
	memset(hashtable->buckets + old_nbuckets, 0, old_nbuckets * sizeof(HashAggBucket));

	/* Iterate all the entries from the hashtable move them as needed */
	for(bucket_idx=0; bucket_idx < old_nbuckets; ++bucket_idx)
	{
		entry = hashtable->buckets[bucket_idx];
		hashtable->buckets[bucket_idx] = NULL;

		while(entry != NULL)
		{
			nextentry = entry->next;
			hashkey = entry->hashvalue;
			bloomval = BLOOMVAL(hashkey);

			new_bucket_idx = BUCKET_IDX(hashtable, hashkey);

			Assert(new_bucket_idx == bucket_idx ||
					new_bucket_idx == bucket_idx + old_nbuckets);

			/* Insert this at the head of the bucket */
			entry->next = hashtable->buckets[new_bucket_idx];
			hashtable->buckets[new_bucket_idx] = entry;
			hashtable->bloom[new_bucket_idx] |= bloomval;

			entry = nextentry;
#ifdef USE_ASSERT_CHECKING
			++nentries;
#endif
		}
	}
	hashtable->num_expansions++;
	Assert(hashtable->mem_for_metadata > 0);
	Assert(nentries == hashtable->num_entries);
}

/*
 * writeHashEntry -- write an hash entry to a batch file.
 *
 * The hash entry is serialized here, including the tuple that contains
 * grouping keys and aggregate values.
 *
 * readHashEntry() should expect to retrieve hash entries in the
 * format defined in this function.
 */
static int32
writeHashEntry(AggState *aggstate, BatchFileInfo *file_info,
			   HashAggEntry *entry)
{
	int32 tuple_agg_size = 0;
	int32 total_size = 0;
	AggStatePerGroup pergroup;
	int aggno;
	AggStatePerAgg peragg = aggstate->peragg;

	Assert(file_info != NULL);
	Assert(file_info->wfile != NULL);

	ExecWorkFile_Write(file_info->wfile, (void *)(&(entry->hashvalue)), sizeof(entry->hashvalue));

	tuple_agg_size = memtuple_get_size((MemTuple)entry->tuple_and_aggs);
	pergroup = (AggStatePerGroup) ((char *)entry->tuple_and_aggs + MAXALIGN(tuple_agg_size));
	tuple_agg_size = MAXALIGN(tuple_agg_size) +
		aggstate->numaggs * sizeof(AggStatePerGroupData);
	total_size = MAXALIGN(tuple_agg_size);

	for (aggno = 0; aggno < aggstate->numaggs; aggno++)
	{
		AggStatePerAgg peraggstate = &peragg[aggno];
		AggStatePerGroup pergroupstate = &pergroup[aggno];
		
		if (!peraggstate->transtypeByVal &&
			!pergroupstate->transValueIsNull)
		{
			Size datum_size = datumGetSize(pergroupstate->transValue,
										   peraggstate->transtypeByVal,
										   peraggstate->transtypeLen);
			total_size += MAXALIGN(datum_size);
		}
	}

	ExecWorkFile_Write(file_info->wfile, (char *)&total_size, sizeof(total_size));
	ExecWorkFile_Write(file_info->wfile, entry->tuple_and_aggs, tuple_agg_size);
	Assert(MAXALIGN(tuple_agg_size) - tuple_agg_size <= MAXIMUM_ALIGNOF);
	if (MAXALIGN(tuple_agg_size) - tuple_agg_size > 0)
	{
		ExecWorkFile_Write(file_info->wfile, padding_dummy, MAXALIGN(tuple_agg_size) - tuple_agg_size);
	}

	for (aggno = 0; aggno < aggstate->numaggs; aggno++)
	{
		AggStatePerAgg peraggstate = &peragg[aggno];
		AggStatePerGroup pergroupstate = &pergroup[aggno];
		
		if (!peraggstate->transtypeByVal &&
			!pergroupstate->transValueIsNull)
		{
			Size datum_size = datumGetSize(pergroupstate->transValue,
										   peraggstate->transtypeByVal,
										   peraggstate->transtypeLen);
			ExecWorkFile_Write(file_info->wfile,
						DatumGetPointer(pergroupstate->transValue), datum_size);
			Assert(MAXALIGN(datum_size) - datum_size <= MAXIMUM_ALIGNOF);
			if (MAXALIGN(datum_size) - datum_size > 0)
			{
				ExecWorkFile_Write(file_info->wfile,
						padding_dummy, MAXALIGN(datum_size) - datum_size);
			}
		}
	}

	return (total_size + sizeof(total_size) + sizeof(entry->hashvalue));
}

/*
 * agg_hash_table_stat_upd
 *   Collect buckets and hash chain statistics of the in-memory hash table for
 *   EXPLAIN ANALYZE
 */
static void
agg_hash_table_stat_upd(HashAggTable *hashtable)
{
	unsigned int	i;

	for (i = 0; i < hashtable->nbuckets; i++)
	{
		HashAggEntry   *entry = hashtable->buckets[i];
		int             chainlength = 0;

		if (entry)
		{
			for (chainlength = 0; entry; chainlength++)
				entry = entry->next;
			cdbexplain_agg_upd(&hashtable->chainlength, chainlength, i);
		}
	}

	hashtable->total_buckets += hashtable->nbuckets;

	/* Cannot use more buckets than have been created */
	Assert(hashtable->chainlength.vcnt <= hashtable->total_buckets);
}

/* Function: init_agg_hash_iter
 *
 * Initialize the HashAggTable's (one and only) entry iterator. */
void init_agg_hash_iter(HashAggTable* hashtable)
{
	Assert( hashtable != NULL && hashtable->buckets != NULL && hashtable->nbuckets > 0 );
	
	hashtable->curr_bucket_idx = -1;
	hashtable->next_entry = NULL;
}

/* Function: agg_hash_iter
 *
 * Returns a pointer to the next HashAggEntry on the given HashAggTable's
 * iterator and advances the iterator.  Returns NULL when there are no more
 * entries.  Be sure to call init_agg_hash_iter before the first call here.
 *
 * During the iteration, this function also writes out entries that belong
 * to batch files which will be processed later.
 */
HashAggEntry *
agg_hash_iter(AggState *aggstate)
{
	HashAggTable* hashtable = aggstate->hhashtable;
	HashAggEntry *entry = hashtable->next_entry;
	SpillSet *spill_set = hashtable->spill_set;
	MemoryContext oldcxt;

	Assert( hashtable != NULL && hashtable->buckets != NULL && hashtable->nbuckets > 0 );

	if (hashtable->curr_spill_file != NULL)
		spill_set = hashtable->curr_spill_file->spill_set;
	
	oldcxt = MemoryContextSwitchTo(hashtable->entry_cxt);

	while (entry == NULL &&
		   hashtable->nbuckets > ++ hashtable->curr_bucket_idx)
	{
		entry = hashtable->buckets[hashtable->curr_bucket_idx];
		if (entry != NULL)
		{
			Assert(entry->is_primodial);
			break;
		}
	}

	if (entry != NULL)
	{
		hashtable->num_output_groups++;
		hashtable->next_entry = entry->next;
		entry->next = NULL;
	}

	MemoryContextSwitchTo(oldcxt);

	return entry;
}

/*
 * Read the serialized from of a hash entry from the given batch file.
 *
 * This function returns a byte array starting with a MemTuple which
 * represents grouping keys, and being followed by its aggregate values.
 * The complete format can be found at writeHashEntry(). The size
 * of the byte array is also returned.
 *
 * The byte array is allocated inside the per-tuple memory context.
 */
static void *
readHashEntry(AggState *aggstate, BatchFileInfo *file_info,
			  HashKey *p_hashkey, int32 *p_input_size)
{
	void *tuple_and_aggs = NULL;
	MemoryContext oldcxt;

	Assert(file_info != NULL && file_info->wfile != NULL);

	*p_input_size = 0;

	if (ExecWorkFile_Read(file_info->wfile, (char *)p_hashkey, sizeof(HashKey)) != sizeof(HashKey))
	{
		return NULL;
	}
	
	if (ExecWorkFile_Read(file_info->wfile, (char *)p_input_size, sizeof(int32)) !=
			sizeof(int32))
	{
		ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
						errmsg("could not read from temporary file: %m")));
	}

	tuple_and_aggs = ExecWorkFile_ReadFromBuffer(file_info->wfile, *p_input_size);
	if (tuple_and_aggs == NULL)
	{
		oldcxt = MemoryContextSwitchTo(aggstate->tmpcontext->ecxt_per_tuple_memory);
		tuple_and_aggs = palloc(*p_input_size);
		int32 read_size = ExecWorkFile_Read(file_info->wfile, tuple_and_aggs, *p_input_size);
		if (read_size != *p_input_size)
			ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
					errmsg("could not read from temporary file, requesting %d bytes, read %d bytes: %m",
							*p_input_size, read_size)));
		MemoryContextSwitchTo(oldcxt);
	}

	return tuple_and_aggs;
}

/* Function: agg_hash_stream
 *
 * Call agg_hash_initial_pass (again) to load more input tuples
 * into the hash table.  Used only for streaming lower phase
 * of a multiphase hashed aggregation to avoid spilling to 
 * file.
 *
 * Return true, if all input tuples have been consumed, else
 * return false (call me again).
 */
bool
agg_hash_stream(AggState *aggstate)
{
	Assert( ((Agg *) aggstate->ss.ps.plan)->streaming );
	
	elog(HHA_MSG_LVL,
		"HashAgg: streaming");

	reset_agg_hash_table(aggstate, 0 /* don't reallocate buckets */);
	
	return agg_hash_initial_pass(aggstate);
}

/*
 * Function: agg_hash_load
 *
 * Load spilled groups into the hash table. Similar to the initial
 * pass, when the hash table does not have space for new groups, all
 * groups in the hash table are spilled to overflown batch files.
 */
static bool
agg_hash_reload(AggState *aggstate)
{
	HashAggTable *hashtable = aggstate->hhashtable;
	ExprContext *tmpcontext = aggstate->tmpcontext; /* per input tuple context */
	bool has_tuples = false;
	SpillFile *spill_file = hashtable->curr_spill_file;

	Assert(spill_file && spill_file->file_info);

	reset_agg_hash_table(aggstate, spill_file->file_info->ntuples);
	reCalcNumberBatches(hashtable, spill_file);

	/*
	 * Record the start value for mem_for_metadata, since its value
	 * has already been accumulated into mem_wanted. Any more memory
	 * added to mem_for_metadata after this point will be added to
	 * mem_wanted at the end of loading hashtable.
	 */
	Assert(hashtable->mem_for_metadata > 0);
	double start_mem_for_metadata = hashtable->mem_for_metadata;

	Assert(spill_file != NULL && spill_file->parent_spill_set != NULL);

	hashtable->is_spilling = false;
	hashtable->num_reloads++;

	hashtable->pshift = spill_file->batch_hash_bit +
		(unsigned) LOG2(spill_file->parent_spill_set->num_spill_files);


	if (spill_file->file_info->wfile != NULL)
	{
		ExecWorkFile_Restart(spill_file->file_info->wfile);
		hashtable->mem_for_metadata  += FREEABLE_BATCHFILE_METADATA;
	}

	while(true)
	{
		HashKey hashkey;
		HashAggEntry *entry;
		bool isNew = false;
		int input_size = 0;
		
		void *input = readHashEntry(aggstate, spill_file->file_info, &hashkey, &input_size);

		if (input != NULL)
		{
			spill_file->file_info->ntuples--;
			Assert(spill_file->parent_spill_set != NULL);
			/* The following asserts the mapping between a hashkey bucket and the index in parent. */
			Assert((hashkey >> spill_file->batch_hash_bit) %
				   spill_file->parent_spill_set->num_spill_files == 
				   spill_file->index_in_parent);
		}

		else
		{
			/* Check we processed all tuples, only when not reading from disk */
			break;
		}

		has_tuples = true;

		/* set up for advance_aggregates call */
		tmpcontext->ecxt_outertuple = aggstate->hashslot;

		entry = lookup_agg_hash_entry(aggstate, input, INPUT_RECORD_GROUP_AND_AGGS, input_size,
									  hashkey, &isNew);
		
		if (entry == NULL)
		{
			Assert(hashtable->curr_spill_file != NULL);
			Assert(hashtable->curr_spill_file->parent_spill_set != NULL);
			
			if (GET_TOTAL_USED_SIZE(hashtable) > hashtable->mem_used)
				hashtable->mem_used = GET_TOTAL_USED_SIZE(hashtable);

			if (hashtable->num_ht_groups <= 1)
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
								 ERRMSG_GP_INSUFFICIENT_STATEMENT_MEMORY));

			elog(gp_workfile_caching_loglevel, "HashAgg: respill occurring in agg_hash_reload while loading batch data");

			if (!hashtable->is_spilling && aggstate->ss.ps.instrument)
			{
				/* Update in-memory hash table statistics before spilling. */
				agg_hash_table_stat_upd(hashtable);
			}

			spill_hash_table(aggstate);

			entry = lookup_agg_hash_entry(aggstate, input, INPUT_RECORD_GROUP_AND_AGGS, input_size,
										  hashkey, &isNew);
		}

		if (!isNew)
		{
			int aggno;
			AggStatePerGroup input_pergroupstate = (AggStatePerGroup)
				((char *)input + MAXALIGN(memtuple_get_size((MemTuple) input)));

			setGroupAggs(hashtable, entry);

			adjustInputGroup(aggstate, input);
			
			/* Advance the aggregates for the group by applying preliminary function. */
			for (aggno = 0; aggno < aggstate->numaggs; aggno++)
			{
				AggStatePerAgg peraggstate = &aggstate->peragg[aggno];
				AggStatePerGroup pergroupstate = &hashtable->groupaggs->aggs[aggno];
				FunctionCallInfoData fcinfo;

				/* Set the input aggregate values */
				fcinfo.arg[1] = input_pergroupstate[aggno].transValue;
				fcinfo.argnull[1] = input_pergroupstate[aggno].transValueIsNull;

				pergroupstate->transValue =
					invoke_agg_trans_func(aggstate,
										  peraggstate,
										  &(peraggstate->prelimfn),
										  peraggstate->prelimfn.fn_nargs - 1,
										  pergroupstate->transValue,
										  &(pergroupstate->noTransValue),
										  &(pergroupstate->transValueIsNull),
										  peraggstate->transtypeByVal,
										  peraggstate->transtypeLen,
										  &fcinfo, (void *)aggstate,
										  aggstate->tmpcontext->ecxt_per_tuple_memory,
										  &(aggstate->mem_manager));
				Assert(peraggstate->transtypeByVal ||
				       (pergroupstate->transValueIsNull ||
					PointerIsValid(DatumGetPointer(pergroupstate->transValue))));
				       
			}
		}
		
		/* Reset per-input-tuple context after each tuple */
		ResetExprContext(tmpcontext);
	}

	CheckSendPlanStateGpmonPkt(&aggstate->ss.ps);

	if (hashtable->is_spilling)
	{
        int freed_size = 0;

		/*
		 * Split out the rest of groups in the hashtable if spilling has already
		 * happened. This is because none of these groups can be immediately outputted
		 * any more.
		 */
		spill_hash_table(aggstate);
        freed_size = suspendSpillFiles(hashtable->curr_spill_file->spill_set);
        hashtable->mem_for_metadata -= freed_size;
        elog(gp_workfile_caching_loglevel, "loaded hashtable from file %s and then respilled. we should delete file from work_set now",
        		hashtable->curr_spill_file->file_info->wfile->fileName);
        hashtable->curr_spill_file->respilled = true;
	}

	else
	{
		/*
		 * Update the workmenwanted value when the hashtable is not spilling.
		 * At this point, we know that groups in this hashtable will not appear
		 * at later time.
		 */
		Assert(hashtable->mem_for_metadata >= start_mem_for_metadata);
		hashtable->mem_wanted += 
			((hashtable->mem_for_metadata - start_mem_for_metadata) +
			 GET_BUFFER_SIZE(hashtable));
	}

	if (!hashtable->is_spilling && aggstate->ss.ps.instrument)
	{
		/* Update in-memory hash table statistics if not already done when spilling */
		agg_hash_table_stat_upd(hashtable);
	}

	return has_tuples;
}

/*
 * Function: reCalcNumberBatches
 *
 * Recalculate the number of batches based on the statistics we collected
 * for a given spill file. This function limits the maximum number of
 * batches to the default one -- gp_hashagg_default_nbatches.
 *
 * Note that we may over-estimate the number of batches, but it is still
 * better than under-estimate it.
 */
static void
reCalcNumberBatches(HashAggTable *hashtable, SpillFile *spill_file)
{
	unsigned nbatches;
	double metadata_size;
	uint64 total_bytes;
	
	Assert(spill_file->file_info != NULL);
	Assert(hashtable->max_mem > hashtable->mem_for_metadata);
		
	total_bytes = spill_file->file_info->total_bytes +
		spill_file->file_info->ntuples * sizeof(HashAggEntry);
	
	nbatches =
		(total_bytes - 1) / 
		(hashtable->max_mem - hashtable->mem_for_metadata) + 1;

	/* Also need to deduct the batch file buffer size */
	metadata_size = hashtable->mem_for_metadata +
		nbatches * BATCHFILE_METADATA;
	
	if (metadata_size < hashtable->max_mem)
		nbatches = (total_bytes - 1) / 
			(hashtable->max_mem - metadata_size) + 1;
	else
		nbatches = 4;

	/* We never respill to a single batch file. */
	if (nbatches == 1)
	    nbatches = 4;

	/* Set the number batches to the power of 2 */
	nbatches = (((unsigned)1) << (unsigned) LOG2(nbatches));

	/*
	 * We limit the number of batches to the default one.
	 */
	if (nbatches > gp_hashagg_default_nbatches)
		nbatches = gp_hashagg_default_nbatches;
	
	if (hashtable->mem_for_metadata +
		nbatches * BATCHFILE_METADATA > hashtable->max_mem)
		ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
				 ERRMSG_GP_INSUFFICIENT_STATEMENT_MEMORY));
	
	hashtable->hats.nbatches = nbatches;
}

/*
 * Fucntion: agg_hash_next_pass
 *
 * This function identifies a batch file to be processed next.
 *
 * When there are no batch files left, this function returns false.
 */
bool
agg_hash_next_pass(AggState *aggstate)
{
	HashAggTable *hashtable = aggstate->hhashtable;
	SpillSet *spill_set;
	int file_no;
	bool more = false;

	Assert( !((Agg *) aggstate->ss.ps.plan)->streaming );
	
	if (hashtable->spill_set == NULL)
		return false;

	elog(HHA_MSG_LVL, "HashAgg: outputed " INT64_FORMAT " groups.", hashtable->num_output_groups);

	if (hashtable->curr_spill_file == NULL)
	{
		spill_set = hashtable->spill_set;
		file_no = 0;
	}
	else if (hashtable->curr_spill_file->spill_set != NULL)
	{
		spill_set = hashtable->curr_spill_file->spill_set;
		file_no = 0;
	}
	else
	{
		spill_set = hashtable->curr_spill_file->parent_spill_set;
		file_no = hashtable->curr_spill_file->index_in_parent;

		/*
		 * Close the current spill file since it is finished, and its buffer space
		 * can be freed to use.
		 */
		hashtable->mem_for_metadata -= closeSpillFile(aggstate, spill_set, file_no);
		file_no++;
	}

	Assert(hashtable->mem_for_metadata > 0);

	/* Find the next SpillSet */
	while (spill_set != NULL)
	{
		SpillFile *parent_spillfile;
		int freespace = 0;
		
		while (file_no < spill_set->num_spill_files)
		{
			if (spill_set->spill_files[file_no].file_info == NULL)
			{
				/* Gap in spill_files array, skip it */
				file_no++;
				continue;
			}

			Assert(spill_set->spill_files[file_no].file_info != NULL);
			if (spill_set->spill_files[file_no].file_info->ntuples == 0)
			{
				/* Batch file with no tuples in it, close it and skip it */
				Assert(spill_set->spill_files[file_no].file_info->total_bytes == 0);
				elog(HHA_MSG_LVL, "Skipping and closing empty batch file HashAgg_Slice%d_Batch_l%d_f%d",
										 currentSliceId,
										 spill_set->level, file_no);
				/*
				 * Close this spill file since it is empty, and its buffer space
				 * can be freed to use.
				 */
				hashtable->mem_for_metadata -= closeSpillFile(aggstate, spill_set, file_no);

				file_no++;
				continue;
			}

			/* Valid spill file, we're done */
			break;
		}

		Assert(hashtable->mem_for_metadata > 0);

		if (file_no < spill_set->num_spill_files)
			break;

		/* If this spill set is the root, break the loop. */
		if (spill_set->parent_spill_file == NULL)
		{
			freespace = freeSpillSet(spill_set);
			hashtable->mem_for_metadata -= freespace;
			spill_set = NULL;
			break;
		}

		parent_spillfile = spill_set->parent_spill_file;
		spill_set = spill_set->parent_spill_file->parent_spill_set;
		
		freespace += freeSpillSet(parent_spillfile->spill_set);
		hashtable->mem_for_metadata -= freespace;
		parent_spillfile->spill_set = NULL;
		
		file_no = parent_spillfile->index_in_parent + 1;

		/* Close parent_spillfile */
		hashtable->mem_for_metadata -= closeSpillFile(aggstate, spill_set,
													  parent_spillfile->index_in_parent);
		Assert(hashtable->mem_for_metadata > 0);
	}
	
	if (spill_set != NULL)
	{
		Assert(file_no < spill_set->num_spill_files);
		
		hashtable->curr_spill_file = &(spill_set->spill_files[file_no]);

		
		elog(HHA_MSG_LVL, "HashAgg: processing %d level batch file %d",
			 spill_set->level, file_no);

		more = agg_hash_reload(aggstate);
	}
	else
	{
		hashtable->curr_spill_file = NULL;
		hashtable->spill_set = NULL;
	}
	
	/* Report statistics for EXPLAIN ANALYZE. */
	if (!more && aggstate->ss.ps.instrument)
	{
		Instrumentation    *instr = aggstate->ss.ps.instrument;

		instr->workmemwanted = Max(instr->workmemwanted, hashtable->mem_wanted);
		instr->workmemused = hashtable->mem_used;
	}

	return more;
}

/*
 * Append hashtable statistics to explain buffer for EXPLAIN ANALYZE
 */
void
agg_hash_explain(AggState *aggstate)
{
	HashAggTable *hashtable = aggstate->hhashtable;
	StringInfo hbuf = aggstate->ss.ps.cdbexplainbuf;

	/* If the hash table spilled */
	if (hashtable->num_spill_groups > 0 )
	{
		appendStringInfo(hbuf,
				INT64_FORMAT " groups total in %d batches",
				hashtable->num_output_groups,
				hashtable->num_batches);
		
		appendStringInfo(hbuf,
				"; %d overflows"
				"; " INT64_FORMAT " spill groups",
				hashtable->num_overflows,
				hashtable->num_spill_groups);

		appendStringInfo(hbuf, ".\n");
	}

	/* Hash chain statistics */
	if (hashtable->chainlength.vcnt > 0)
	{
		appendStringInfo(hbuf,
				"Hash chain length %.1f avg, %.0f max,"
				" using %d of " INT64_FORMAT " buckets"
				"; total %d expansions.\n",
				cdbexplain_agg_avg(&hashtable->chainlength),
				hashtable->chainlength.vmax,
				hashtable->chainlength.vcnt,
				hashtable->total_buckets,
				hashtable->num_expansions);
	}
}

/* Resets all gpmon states for this agg and sends an updated gpmon packet */
static void
Gpmon_ResetAggHashTable(AggState* aggstate)
{
	CheckSendPlanStateGpmonPkt(&aggstate->ss.ps);
}

/* Function: reset_agg_hash_table
 *
 * Clear the hash table content anchored by the bucket array.
 */
void reset_agg_hash_table(AggState *aggstate, int64 nentries)
{
	HashAggTable *hashtable = aggstate->hhashtable;
	bool reallocate_buckets = true;
	double old_nbuckets;
	HashAggTableSizes hats;
	MemoryContext oldcxt;
	
	elog(HHA_MSG_LVL,
		"HashAgg: resetting " INT64_FORMAT "-entry hash table",
		hashtable->num_ht_groups);

	Assert(hashtable->buckets && hashtable->bloom);

	/*
	 * Determine whether to reallocate buckets. Especially avoid re-allocation if
	 * already the right size
	 */
	reallocate_buckets = nentries > 0 &&
		calcHashAggTableSizes(hashtable->max_mem,
			nentries,
			hashtable->hats.hashentry_width,
			true,
			&hats) &&
		(hats.nbuckets != hashtable->nbuckets);

	if (reallocate_buckets)
	{
		Assert(hats.nbuckets > 0);
		old_nbuckets = hashtable->nbuckets;
		oldcxt = MemoryContextSwitchTo(aggstate->aggcontext);

		Assert(hashtable->mem_for_metadata > hashtable->nbuckets * OVERHEAD_PER_BUCKET);

		/* Recalculate memory used with the increase/decrease in nbuckets */
		hashtable->mem_for_metadata +=
			((hats.nbuckets - old_nbuckets) * OVERHEAD_PER_BUCKET);
		hashtable->nbuckets = hats.nbuckets;

		/* Copy relevant stats into the hashtable */
		hashtable->hats.nbuckets = hats.nbuckets;
		hashtable->hats.nentries = hats.nentries;

		pfree(hashtable->buckets);
		pfree(hashtable->bloom);

		hashtable->buckets = (HashAggBucket *) palloc0(hashtable->nbuckets * sizeof(HashAggBucket));
		hashtable->bloom = (uint64 *) palloc0(hashtable->nbuckets * sizeof(uint64));

		hashtable->expandable = true;

		MemoryContextSwitchTo(oldcxt);

		Assert(AVAIL_MEM(hashtable) > 0);
		elog(HHA_MSG_LVL, "Resetting with %d buckets for %d entries",
				hashtable->nbuckets, hats.nentries);
	}
	else
	{
		/* No need to reallocated buckets. Reset to zero. */
		MemSet(hashtable->buckets, 0, hashtable->nbuckets * sizeof(HashAggBucket));
		MemSet(hashtable->bloom, 0, hashtable->nbuckets * sizeof(uint64));
	}

	Assert(hashtable->mem_for_metadata > 0);

	hashtable->num_ht_groups = 0;
	hashtable->num_entries = 0;

	hashtable->pshift = 0;

	mpool_reset(hashtable->group_buf);

	init_agg_hash_iter(hashtable);

	Gpmon_ResetAggHashTable(aggstate);
}

/* Function: destroy_agg_hash_table
 *
 * Give back the resources anchored by the hash table.  Ok to call, even
 * if the hash table isn't set up.
 */
void destroy_agg_hash_table(AggState *aggstate)
{
	Agg *agg = (Agg*)aggstate->ss.ps.plan;
	
	if ( agg->aggstrategy == AGG_HASHED && aggstate->hhashtable != NULL )
	{

		elog(HHA_MSG_LVL,
			"HashAgg: destroying hash table -- ngroup=" INT64_FORMAT " ntuple=" INT64_FORMAT,
			aggstate->hhashtable->num_ht_groups,
			aggstate->hhashtable->num_tuples);
		
		Gpmon_ResetAggHashTable(aggstate);

		/* destroy_batches(aggstate->hhashtable); */
		pfree(aggstate->hhashtable->buckets);
		pfree(aggstate->hhashtable->bloom);
		if (aggstate->hhashtable->hashkey_buf)
			pfree(aggstate->hhashtable->hashkey_buf);

		closeSpillFiles(aggstate, aggstate->hhashtable->spill_set);

		if (NULL != aggstate->hhashtable->spill_set)
		{
			pfree(aggstate->hhashtable->spill_set);
			aggstate->hhashtable->spill_set = NULL;
		}

		if (NULL != aggstate->hhashtable->work_set)
		{
			workfile_mgr_close_set(aggstate->hhashtable->work_set);
		}

		mpool_delete(aggstate->hhashtable->group_buf);

		pfree(aggstate->hhashtable);
		aggstate->hhashtable = NULL;
	}
}

/* EOF */
