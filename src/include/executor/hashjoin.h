/*-------------------------------------------------------------------------
 *
 * hashjoin.h
 *	  internal structures for hash joins
 *
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/executor/hashjoin.h,v 1.48 2008/01/01 19:45:57 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef HASHJOIN_H
#define HASHJOIN_H

#include "fmgr.h"
#include "executor/execWorkfile.h"
#include "cdb/cdbpublic.h"                 /* CdbExplain_Agg */
#include "utils/workfile_mgr.h"

struct StringInfoData;                  /* #include "lib/stringinfo.h" */


/* ----------------------------------------------------------------
 *				hash-join hash table structures
 *
 * Each active hashjoin has a HashJoinTable control block, which is
 * palloc'd in the executor's per-query context.  All other storage needed
 * for the hashjoin is kept in private memory contexts, two for each hashjoin.
 * This makes it easy and fast to release the storage when we don't need it
 * anymore.  (Exception: data associated with the temp files lives in the
 * per-query context too, since we always call buffile.c in that context.)
 *
 * The hashtable contexts are made children of the per-query context, ensuring
 * that they will be discarded at end of statement even if the join is
 * aborted early by an error.  (Likewise, any temporary files we make will
 * be cleaned up by the virtual file manager in event of an error.)
 *
 * Storage that should live through the entire join is allocated from the
 * "hashCxt", while storage that is only wanted for the current batch is
 * allocated in the "batchCxt".  By resetting the batchCxt at the end of
 * each batch, we free all the per-batch storage reliably and without tedium.
 *
 * During first scan of inner relation, we get its tuples from executor.
 * If nbatch > 1 then tuples that don't belong in first batch get saved
 * into inner-batch temp files. The same statements apply for the
 * first scan of the outer relation, except we write tuples to outer-batch
 * temp files.	After finishing the first scan, we do the following for
 * each remaining batch:
 *	1. Read tuples from inner batch file, load into hash buckets.
 *	2. Read tuples from outer batch file, match to hash buckets and output.
 *
 * It is possible to increase nbatch on the fly if the in-memory hash table
 * gets too big.  The hash-value-to-batch computation is arranged so that this
 * can only cause a tuple to go into a later batch than previously thought,
 * never into an earlier batch.  When we increase nbatch, we rescan the hash
 * table and dump out any tuples that are now of a later batch to the correct
 * inner batch file.  Subsequently, while reading either inner or outer batch
 * files, we might find tuples that no longer belong to the current batch;
 * if so, we just dump them out to the correct batch file.
 * ----------------------------------------------------------------
 */

/* these are in nodes/execnodes.h: */
/* typedef struct HashJoinTupleData *HashJoinTuple; */
/* typedef struct HashJoinTableData *HashJoinTable; */

typedef struct HashJoinTupleData
{
	struct HashJoinTupleData *next;		/* link to next tuple in same bucket */
	uint32		hashvalue;		/* tuple's hash code */
	/* Tuple data, in MinimalTuple format, follows on a MAXALIGN boundary */
} HashJoinTupleData;

#define HJTUPLE_OVERHEAD  MAXALIGN(sizeof(HashJoinTupleData))
#define HJTUPLE_MINTUPLE(hjtup)  \
	((MemTuple) ((char *) (hjtup) + HJTUPLE_OVERHEAD))


/* Statistics collection workareas for EXPLAIN ANALYZE */
typedef struct HashJoinBatchStats
{
    uint64      outerfilesize;
    uint64      innerfilesize;
    uint64      irdbytes;           /* inner bytes read from workfile */
    uint64      ordbytes;           /* outer bytes read from workfile */
    uint64      iwrbytes;           /* inner bytes written (to later batches) */
    uint64      owrbytes;           /* outer bytes written (to later batches) */
    uint64      hashspace_final;    /* work_mem for tuples kept in hash table */
    uint64      spillspace_in;      /* work_mem from lower batches to this one */
    uint64      spillspace_out;     /* work_mem from this batch to higher ones */
    uint64      spillrows_out;      /* rows spilled from this batch to higher */
} HashJoinBatchStats;

typedef struct HashJoinTableStats
{
    struct StringInfoData  *joinexplainbuf; /* Join operator's report buf */
    HashJoinBatchStats     *batchstats;     /* -> array[0..nbatchstats-1] */
    int                     nbatchstats;    /* num of batchstats slots */
    int                     endedbatch;     /* index of last batch ended */

    /* These statistics are cumulative over all nontrivial batches... */
    int                     nonemptybatches;    /* num of nontrivial batches */
    Size                    workmem_max;        /* work_mem high water mark */
    CdbExplain_Agg          chainlength;        /* hash chain length stats */
} HashJoinTableStats;


/*
 * HashJoinBatchSide
 *
 * State of the outer or inner side of one batch.
 */
typedef struct HashJoinBatchSide
{
	/*
	 * A file is opened only when we first write a tuple into it
	 * (otherwise its pointer remains NULL).  Note that the zero'th
	 * batch never has files, since we will process rather than dump
	 * out any tuples of batch zero.
	 */
	ExecWorkFile *workfile;
	int total_tuples;
} HashJoinBatchSide;


/*
 * HashJoinBatchData
 *
 * State of one batch.
 */
typedef struct HashJoinBatchData
{
    Size                innerspace;     /* work_mem bytes for inner tuples */
    unsigned            innertuples;    /* inner number of tuples */

    HashJoinBatchSide   innerside;
    HashJoinBatchSide   outerside;
} HashJoinBatchData;


/*
 * HashJoinTableData
 */
typedef struct HashJoinTableData
{
	int			nbuckets;		/* # buckets in the in-memory hash table */
	int			log2_nbuckets;	/* its log2 (nbuckets must be a power of 2) */

	/* buckets[i] is head of list of tuples in i'th in-memory bucket */
	struct HashJoinTupleData **buckets;
	uint64     				  *bloom; /* bloom[i] is bloomfilter for buckets[i] */
	/* buckets array is per-batch storage, as are all the tuples */

	int			nbatch;			/* number of batches */
	int			curbatch;		/* current batch #; 0 during 1st pass */

	int			nbatch_original;	/* nbatch when we started inner scan */
	int			nbatch_outstart;	/* nbatch when we started outer scan */

	bool		growEnabled;	/* flag to shut off nbatch increases */

	uint64		totalTuples;	/* # tuples obtained from inner plan */

	HashJoinBatchData **batches;    /* array [0..nbatch-1] of ptr to HJBD */

	/* Representation of all spill file names, for spill file reuse */
	workfile_set * work_set;

	ExecWorkFile * state_file;

	/*
	 * Info about the datatype-specific hash functions for the datatypes being
	 * hashed. These are arrays of the same length as the number of hash join
	 * clauses (hash keys).
	 */
	FmgrInfo   *outer_hashfunctions;	/* lookup data for hash functions */
	FmgrInfo   *inner_hashfunctions;	/* lookup data for hash functions */
	bool	   *hashStrict;		/* is each hash join operator strict? */

	Size		spaceAllowed;	/* upper limit for space used */

	MemoryContext hashCxt;		/* context for whole-hash-join storage */
	MemoryContext batchCxt;		/* context for this-batch-only storage */
	MemoryContext bfCxt;		/* CDB */ /* context for temp buf file */

    HashJoinTableStats *stats;  /* statistics workarea for EXPLAIN ANALYZE */
    bool		eagerlyReleased; /* Has this hash-table been eagerly released? */

    HashJoinState * hjstate; /* reference to the enclosing HashJoinState */
    bool first_pass; /* Is this the first pass (pre-rescan) */
} HashJoinTableData;

#endif   /* HASHJOIN_H */
