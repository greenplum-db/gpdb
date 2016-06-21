/*-------------------------------------------------------------------------
 *
 *  execHybridhashagg.h
 *		GPDB addition to support hybrid hash aggregation algorithm.
 *
 *
 * Portions Copyright (c) 2006-2008, Greenplum
 * Portions Copyright (c) 1996-2005, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *    $Id$
 *
 *-------------------------------------------------------------------------
 */
#ifndef EXECHYBRIDHASHAGG_H
#define EXECHYBRIDHASHAGG_H

#include "executor/nodeAgg.h" /* Must see AggStatePerGroupData. */
#include "cdb/cdbpublic.h"    /* CdbExplain_Agg / CdbCellBuf */
#include "utils/memutils.h"
#include "executor/execWorkfile.h"
#include "utils/workfile_mgr.h"

typedef uint32 HashKey;
typedef struct BatchFileInfo BatchFileInfo;

/* An entry in an Agg hash table.
 * 
 * Each such entry corresponds to a single group and includes the grouping
 * key value, the hash value of that key, and the transition data for each
 * aggregate being evaluated.
 *
 * When accounting for the memory used by a HashAggEntry, include the
 * size of HashAggEntry structure plus an AggStatePerGroupData for each
 * aggregate function plus the size of the MinimalTupleData used to hold the
 * value of the grouping key.  Additional space is used for and pass-by-
 * reference Datum values in the grouping key and in transValues
 * in the per-group structure.
 */
typedef struct HashAggEntry
{
	struct HashAggEntry	   *next; /* Next entry in chain. */
	void *tuple_and_aggs; /* point to a chunk that contains both grouping keys
						   * and aggregate values.
						   */
	HashKey	hashvalue;
	bool is_primodial; /* indicate if this entry is there before spilling. */
} HashAggEntry;

/* A SpillFile controls access to a temporary file used to hold  
 * transition tuples spilled from the hash table in order to free 
 * up space.
 *
 * SpillFiles are organized into a tree by SpillSets.  See comments
 * there.
 *
 * The life cycle of a SpillFile S is as follows:
 *
 * 1. S is allocated as one of the branches of a SpillSet.
 *    
 *    Fields (file, spill_set) == (NULL, NULL).
 *
 * 2. S begins to accept spilled transition tuples.  The file field 
 *    holds the BufFileInfo* of a temporary file open for writing.
 *    
 *    Fields (file, spill_set) == (non-NULL BufFileInfo*, NULL).
 *
 * 3. S is closed for writing and opened for reading as the input source
 *    for a reload of the hash table.  The temporary file is rewound.
 *    Field spill_set points to a new SpillSet for use in case future 
 *    spills occur within the hash key range of S.
 *    
 *    Fields (file, spill_set) == (non-NULL BufFileInfo*, non-NULL SpillSet*).
 *
 * 4. S is closed for reading.  The temporary file is closed and the
 *    field file is set to NULL.  Any SpillSet pointed to by the field
 *    spill_set remains again the possibility of future spills to 
 *    the range of S.
 *    
 *    Fields (file, spill_set) == (NULL, non-NULL SpillSet*).
 *
 * Spilling transition tuples to a SpillFile is a two-step process.
 * First, as hash entries whose tuples should spill are discovered,
 * they are linked to the transient queue (queue_spill and tail) of
 * the SpillFile.  Later, they are written sequentially to the file.
 *
 */
typedef struct SpillFile
{
	BatchFileInfo *file_info; /* Temporary file */
	struct SpillSet* spill_set; /* Non-null if file is for reading */
	
	/* Reflect our controlling spill set.  */
	struct SpillSet *parent_spill_set;
	unsigned index_in_parent;
	/* Number of bits to shift key before hashing for tuples in this batch */
	unsigned batch_hash_bit;
	/* Indicates if this spillfile could not fit in memory and was respilled to another SpillSet */
	bool respilled;
} SpillFile;

/* A SpillSet is sequence of SpillFiles that partition a range of
 * hash key values into smaller ranges.  When the range represented
 * by a SpillFile turns out to be too large, an additional SpillSet
 * is added below one of its SpillFiles.  This results in a tree 
 * structure rooted by at top-level SpillSet anchored in the HashTable.
 *
 * The number of SpillFiles in a SpillSet should be a power of two.
 */
typedef struct SpillSet
{
	/* The pointer to its parent spill file */
	SpillFile *parent_spill_file;
	int level;
	unsigned num_spill_files;
	SpillFile spill_files[1]; /* 1st of num_spill_file contiguous SpillFile. */
} SpillSet;

typedef struct HashAggTableSizes
{
    unsigned             nbuckets;   /* Calculated # of hash buckets. */
    unsigned             nentries;   /* Calculated # of hash entries. */
    unsigned             nbatches;   /* Calculated # of passes. */
	double               hashentry_width; /* Estimated hash entry size */
    unsigned             workmem_initial;    /* Estimated work_mem bytes at #entries=0 */
    unsigned             workmem_per_entry;  /* Additional work_mem bytes per entry */
    bool                 spill;      /* Do we expect to spill ? */
} HashAggTableSizes;

/*
 * A higher level structure to earily reference grouping keys and their
 * aggregate values.
 */
typedef struct GroupKeysAndAggs
{
	struct MemTupleData *tuple; /* tuple that contains grouping keys */
	AggStatePerGroup aggs; /* the location for the first aggregate values. */
} GroupKeysAndAggs;

typedef enum HashAggState
{
	HASHAGG_BEFORE_FIRST_PASS,
	HASHAGG_IN_A_PASS,
	HASHAGG_BETWEEN_PASSES,
	HASHAGG_STREAMING,
	HASHAGG_END_OF_PASSES
} HashAggState;

/* An Agg hash table with associated overflow batches and processing
 * state.  
 * 
 * The table holds information specific to hybrid hash aggregate, but
 * relies on the parent AggState or grandparent Agg nodes for many items,
 * e.g., description of input and output tuples, tuple slots, expression 
 * and memory contexts, grouping key information (including hash and 
 * equality functions), etc. Thus it is very tightly coupled with them.
 */
typedef struct HashAggTable
{
	/* Hash table */
	MemoryContext   entry_cxt;	/* memory context for hash table entries */

	unsigned nbuckets;
	HashAggEntry  **buckets;
	uint64 *bloom;

	/* Overflow batches */
	SpillSet       *spill_set;
	/* Representation of all workfile names, used by the workfile manager */
	workfile_set *work_set;
	/* Metadata file containing information required to restore the state
	 * from a cached workfile for reuse */
	ExecWorkFile *state_file;

	/* The batch file is currently being processed. */
	SpillFile *curr_spill_file;
	int curr_spill_level;

	HashAggState		state;	/* state of processing */
	unsigned			pass;	/* current pass (0 is initial) */

	/*
	 * The space to buffer the free hash entries and AggStatePerGroups. Using this,
	 * we can reduce palloc/pfree calls.
	 */
	CdbCellBuf entry_buf;
	MPool *group_buf;

	/* Shared temporary structure */
	GroupKeysAndAggs   *groupaggs;

	/* Variables during iteration */
	int curr_bucket_idx;
	HashAggEntry *next_entry;

	/* buffer for calculating the hashkey */
	HashKey *hashkey_buf;

	/* GPDB: Statistics for EXPLAIN ANALYZE */
	HashAggTableSizes   hats;

	double max_mem; /* Maximum available memory */
	double mem_for_metadata; /* Current memory usage for metadata */
	double mem_wanted; /* The desirable work_mem */
	double mem_used; /* The maxinum amount of used memory. */
	
	uint32 num_reloads; /* number of times reloading a batch file */
	uint32 num_batches; /* number of batch files */
	uint64 num_tuples; /* Total input tuples so far*/
	uint64 num_output_groups; /* Total output groups */
	uint64 num_ht_groups; /* number of groups in the hash table */
	uint64 num_spill_groups; /* number of spilled groups */
	uint32 num_overflows; /* number of times hash table overflows */
	uint64 total_buckets; /* total number of buckets allocated */
	bool is_spilling; /* indicate that spilling happened for this batch. */
	struct TupleTableSlot *prev_slot; /* a slot that is read previously. */
    CdbExplain_Agg      chainlength;
} HashAggTable;

extern HashAggTable *create_agg_hash_table(AggState *aggstate);
extern bool agg_hash_initial_pass(AggState *aggstate);
extern bool agg_hash_stream(AggState *aggstate);
extern bool agg_hash_next_pass(AggState *aggstate);
extern bool agg_hash_continue_pass(AggState *aggstate);
extern void destroy_agg_hash_table(AggState *aggstate);

extern HashAggEntry *agg_hash_iter(AggState *aggstate);

extern bool 
calcHashAggTableSizes(double memquota,	/* Memory quota in bytes. */
					   double ngroups,	/* Est # of groups. */
					   int numaggs,		/* Est # of aggregate functions */
					   int keywidth,	/* Est per entry size of hash key. */
					   int transpace,	/* Est per entry size of by-ref values. */
                       bool force,      /* true => succeed even if work_mem too small */
                       HashAggTableSizes   *out_hats);
#endif
