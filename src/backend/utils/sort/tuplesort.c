/*-------------------------------------------------------------------------
 *
 * tuplesort.c
 *	  Generalized tuple sorting routines.
 *
 * This module handles sorting of heap tuples, index tuples, or single
 * Datums (and could easily support other kinds of sortable objects,
 * if necessary).  It works efficiently for both small and large amounts
 * of data.  Small amounts are sorted in-memory using qsort().	Large
 * amounts are sorted using temporary files and a standard external sort
 * algorithm.
 *
 * See Knuth, volume 3, for more than you want to know about the external
 * sorting algorithm.  We divide the input into sorted runs using replacement
 * selection, in the form of a priority tree implemented as a heap
 * (essentially his Algorithm 5.2.3H), then merge the runs using polyphase
 * merge, Knuth's Algorithm 5.4.2D.  The logical "tapes" used by Algorithm D
 * are implemented by logtape.c, which avoids space wastage by recycling
 * disk space as soon as each block is read from its "tape".
 *
 * We do not form the initial runs using Knuth's recommended replacement
 * selection data structure (Algorithm 5.4.1R), because it uses a fixed
 * number of records in memory at all times.  Since we are dealing with
 * tuples that may vary considerably in size, we want to be able to vary
 * the number of records kept in memory to ensure full utilization of the
 * allowed sort memory space.  So, we keep the tuples in a variable-size
 * heap, with the next record to go out at the top of the heap.  Like
 * Algorithm 5.4.1R, each record is stored with the run number that it
 * must go into, and we use (run number, key) as the ordering key for the
 * heap.  When the run number at the top of the heap changes, we know that
 * no more records of the prior run are left in the heap.
 *
 * The approximate amount of memory allowed for any one sort operation
 * is specified in kilobytes by the caller (most pass work_mem).  Initially,
 * we absorb tuples and simply store them in an unsorted array as long as
 * we haven't exceeded workMem.  If we reach the end of the input without
 * exceeding workMem, we sort the array using qsort() and subsequently return
 * tuples just by scanning the tuple array sequentially.  If we do exceed
 * workMem, we construct a heap using Algorithm H and begin to emit tuples
 * into sorted runs in temporary tapes, emitting just enough tuples at each
 * step to get back within the workMem limit.  Whenever the run number at
 * the top of the heap changes, we begin a new run with a new output tape
 * (selected per Algorithm D).	After the end of the input is reached,
 * we dump out remaining tuples in memory into a final run (or two),
 * then merge the runs using Algorithm D.
 *
 * When merging runs, we use a heap containing just the frontmost tuple from
 * each source run; we repeatedly output the smallest tuple and insert the
 * next tuple from its source tape (if any).  When the heap empties, the merge
 * is complete.  The basic merge algorithm thus needs very little memory ---
 * only M tuples for an M-way merge, and M is constrained to a small number.
 * However, we can still make good use of our full workMem allocation by
 * pre-reading additional tuples from each source tape.  Without prereading,
 * our access pattern to the temporary file would be very erratic; on average
 * we'd read one block from each of M source tapes during the same time that
 * we're writing M blocks to the output tape, so there is no sequentiality of
 * access at all, defeating the read-ahead methods used by most Unix kernels.
 * Worse, the output tape gets written into a very random sequence of blocks
 * of the temp file, ensuring that things will be even worse when it comes
 * time to read that tape.	A straightforward merge pass thus ends up doing a
 * lot of waiting for disk seeks.  We can improve matters by prereading from
 * each source tape sequentially, loading about workMem/M bytes from each tape
 * in turn.  Then we run the merge algorithm, writing but not reading until
 * one of the preloaded tuple series runs out.	Then we switch back to preread
 * mode, fill memory again, and repeat.  This approach helps to localize both
 * read and write accesses.
 *
 * When the caller requests random access to the sort result, we form
 * the final sorted run on a logical tape which is then "frozen", so
 * that we can access it randomly.	When the caller does not need random
 * access, we return from tuplesort_performsort() as soon as we are down
 * to one run per logical tape.  The final merge is then performed
 * on-the-fly as the caller repeatedly calls tuplesort_getXXX; this
 * saves one cycle of writing all the data out to disk and reading it in.
 *
 * Before Postgres 8.2, we always used a seven-tape polyphase merge, on the
 * grounds that 7 is the "sweet spot" on the tapes-to-passes curve according
 * to Knuth's figure 70 (section 5.4.2).  However, Knuth is assuming that
 * tape drives are expensive beasts, and in particular that there will always
 * be many more runs than tape drives.	In our implementation a "tape drive"
 * doesn't cost much more than a few Kb of memory buffers, so we can afford
 * to have lots of them.  In particular, if we can have as many tape drives
 * as sorted runs, we can eliminate any repeated I/O at all.  In the current
 * code we determine the number of tapes M on the basis of workMem: we want
 * workMem/M to be large enough that we read a fair amount of data each time
 * we preread from a tape, so as to maintain the locality of access described
 * above.  Nonetheless, with large workMem we can have many tapes.
 *
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/utils/sort/tuplesort.c,v 1.89 2009/01/01 17:23:53 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */

#define COMPILING_TUPLESORT_C


#include "postgres.h"

#include <limits.h>

#include "access/genam.h"
#include "access/nbtree.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_operator.h"
#include "commands/tablespace.h"
#include "miscadmin.h"
#include "pg_trace.h"
#include "utils/datum.h"
#include "utils/logtape.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/pg_rusage.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "utils/tuplesort.h"

#include "access/genam.h"
#include "cdb/cdbvars.h"
#include "executor/execWorkfile.h"
#include "executor/instrument.h"        /* Instrumentation */
#include "executor/nodeSort.h"          /* Gpmon */
#include "lib/stringinfo.h"             /* StringInfo */
#include "utils/dynahash.h"             /* my_log2 */

/* GUC variables */
#ifdef TRACE_SORT
bool		trace_sort = false;
#endif

#define HEAP_SORT	0
#define INDEX_SORT	1
#define DATUM_SORT	2

#ifdef DEBUG_BOUNDED_SORT
bool		optimize_bounded_sort = true;
#endif


/*
 * The objects we actually sort are SortTuple structs.	These contain
 * a pointer to the tuple proper (might be a MinimalTuple or IndexTuple),
 * which is a separate palloc chunk --- we assume it is just one chunk and
 * can be freed by a simple pfree().  SortTuples also contain the tuple's
 * first key column in Datum/nullflag format, and an index integer.
 *
 * Storing the first key column lets us save heap_getattr or index_getattr
 * calls during tuple comparisons.	We could extract and save all the key
 * columns not just the first, but this would increase code complexity and
 * overhead, and wouldn't actually save any comparison cycles in the common
 * case where the first key determines the comparison result.  Note that
 * for a pass-by-reference datatype, datum1 points into the "tuple" storage.
 *
 * When sorting single Datums, the data value is represented directly by
 * datum1/isnull1.	If the datatype is pass-by-reference and isnull1 is false,
 * then datum1 points to a separately palloc'd data value that is also pointed
 * to by the "tuple" pointer; otherwise "tuple" is NULL.
 *
 * While building initial runs, tupindex holds the tuple's run number.  During
 * merge passes, we re-use it to hold the input tape number that each tuple in
 * the heap was read from, or to hold the index of the next tuple pre-read
 * from the same tape in the case of pre-read entries.	tupindex goes unused
 * if the sort occurs entirely in memory.
 */
typedef struct
{
	MemTuple	tuple;			/* the tuple proper */
	Datum		datum1;			/* value of first key column */
	int			tupindex;		/* see notes above */
	bool		isnull1;		/* is first key column NULL? */
} SortTuple;


/*
 * Possible states of a Tuplesort object.  These denote the states that
 * persist between calls of Tuplesort routines.
 */
typedef enum
{
	TSS_INITIAL,				/* Loading tuples; still within memory limit */
	TSS_BOUNDED,				/* Loading tuples into bounded-size heap */
	TSS_BUILDRUNS,				/* Loading tuples; writing to tape */
	TSS_SORTEDINMEM,			/* Sort completed entirely in memory */
	TSS_SORTEDONTAPE,			/* Sort completed, final run is on tape */
	TSS_FINALMERGE				/* Performing final merge on-the-fly */
} TupSortStatus;

/*
 * Parameters for calculation of number of tapes to use --- see inittapes()
 * and tuplesort_merge_order().
 *
 * In this calculation we assume that each tape will cost us about 3 blocks
 * worth of buffer space (which is an underestimate for very large data
 * volumes, but it's probably close enough --- see logtape.c).
 *
 * MERGE_BUFFER_SIZE is how much data we'd like to read from each input
 * tape during a preread cycle (see discussion at top of file).
 */
#define MINORDER		6		/* minimum merge order */
#define MAXORDER		250		/* maximum merge order */
#define TAPE_BUFFER_OVERHEAD		(BLCKSZ * 3)
#define MERGE_BUFFER_SIZE			(BLCKSZ * 32)

/* 
 * Current postion of Tuplesort operation.
 */
struct TuplesortPos
{
	/*
	 * These variables are used after completion of sorting to keep track of
	 * the next tuple to return.  (In the tape case, the tape's current read
	 * position is also critical state.)
	 */
	int			current;		/* array index (only used if SORTEDINMEM) */
	bool		eof_reached;	/* reached EOF (needed for cursors) */

	/* markpos_xxx holds marked position for mark and restore */
	union {
		LogicalTapePos tapepos;
		long 		   mempos;
	} markpos;
	bool markpos_eof; 

	LogicalTape* 		cur_work_tape;      /* current tape that I am working on */
};


/*
 * Private state of a Tuplesort operation.
 */
struct Tuplesortstate
{
	/* MUST BE FIRST, to match switcheroo_Tuplesortstate */
	bool		is_mk_tuplesortstate;

	TupSortStatus status;		/* enumerated value as shown above */
	int			nKeys;			/* number of columns in sort key */
	bool		randomAccess;	/* did caller request random access? */
	bool		bounded;		/* did caller specify a maximum number of
								 * tuples to return? */
	bool		boundUsed;		/* true if we made use of a bounded heap */
	int			bound;			/* if bounded, the maximum number of tuples */
	long		availMem;		/* remaining memory available, in bytes */
	long        availMemMin;    /* CDB: availMem low water mark (bytes) */
	long        availMemMin01;  /* MPP-1559: initial low water mark */
	long		allowedMem;		/* total memory allowed, in bytes */
	int			maxTapes;		/* number of tapes (Knuth's T) */
	int			tapeRange;		/* maxTapes-1 (Knuth's P) */
	MemoryContext sortcontext;	/* memory context holding all sort data */
	LogicalTapeSet *tapeset;	/* logtape.c object for tapes in a temp file */

	/*
	 * These function pointers decouple the routines that must know what kind
	 * of tuple we are sorting from the routines that don't need to know it.
	 * They are set up by the tuplesort_begin_xxx routines.
	 *
	 * Function to compare two tuples; result is per qsort() convention, ie:
	 * <0, 0, >0 according as a<b, a=b, a>b.  The API must match
	 * qsort_arg_comparator.
	 */
	int			(*comparetup) (const SortTuple *a, const SortTuple *b,
										   Tuplesortstate *state);

	/*
	 * Function to copy a supplied input tuple into palloc'd space and set up
	 * its SortTuple representation (ie, set tuple/datum1/isnull1).  Also,
	 * state->availMem must be decreased by the amount of space used for the
	 * tuple copy (note the SortTuple struct itself is not counted).
	 */
	void		(*copytup) (Tuplesortstate *state, SortTuple *stup, void *tup);

	/*
	 * Function to write a stored tuple onto tape.	The representation of the
	 * tuple on tape need not be the same as it is in memory; requirements on
	 * the tape representation are given below.  After writing the tuple,
	 * pfree() the out-of-line data (not the SortTuple struct!), and increase
	 * state->availMem by the amount of memory space thereby released.
	 */
	void		(*writetup) (Tuplesortstate *state, LogicalTape *lt,
										 SortTuple *stup);

	/*
	 * Function to read a stored tuple from tape back into memory. 'len' is
	 * the already-read length of the stored tuple.  Create a palloc'd copy,
	 * initialize tuple/datum1/isnull1 in the target SortTuple struct, and
	 * decrease state->availMem by the amount of memory space consumed.
	 */
	void		(*readtup) (Tuplesortstate *state, TuplesortPos *pos, SortTuple *stup,
			LogicalTape *lt, uint32 len);

	/*
	 * Function to reverse the sort direction from its current state. (We
	 * could dispense with this if we wanted to enforce that all variants
	 * represent the sort key information alike.)
	 */
	void		(*reversedirection) (Tuplesortstate *state);

	/*
	 * This array holds the tuples now in sort memory.	If we are in state
	 * INITIAL, the tuples are in no particular order; if we are in state
	 * SORTEDINMEM, the tuples are in final sorted order; in states BUILDRUNS
	 * and FINALMERGE, the tuples are organized in "heap" order per Algorithm
	 * H.  (Note that memtupcount only counts the tuples that are part of the
	 * heap --- during merge passes, memtuples[] entries beyond tapeRange are
	 * never in the heap and are used to hold pre-read tuples.)  In state
	 * SORTEDONTAPE, the array is not used.
	 */
	SortTuple  *memtuples;		/* array of SortTuple structs */
	int			memtupcount;	/* number of tuples currently present */
	int			memtupsize;		/* allocated length of memtuples array */
	bool		memtupblimited; /* true when hit the limit */
	int64  		discardcount;   /* count of discards */ /*CDB*/
	int64  		totalNumTuples;    /* count of all input tuples */ /*CDB*/
	bool		noduplicates;   /* discard duplicate rows if true *//* CDB */
	int			mppsortflags;   /* special sort flags*//* CDB */
	int64  		gpmaxdistinct;  /* max number of distinct values */ /*CDB*/
	bool		standardsort;   /* do regular sort if true *//* CDB */

	/*
	 * A flag to indicate whether the stats for this tuplesort
	 * has been finalized.
	 */
	bool statsFinalized;

	/*
	 * While building initial runs, this is the current output run number
	 * (starting at 0).  Afterwards, it is the number of initial runs we made.
	 */
	int			currentRun;

	/*
	 * Unless otherwise noted, all pointer variables below are pointers to
	 * arrays of length maxTapes, holding per-tape data.
	 */

	/*
	 * These variables are only used during merge passes.  mergeactive[i] is
	 * true if we are reading an input run from (actual) tape number i and
	 * have not yet exhausted that run.  mergenext[i] is the memtuples index
	 * of the next pre-read tuple (next to be loaded into the heap) for tape
	 * i, or 0 if we are out of pre-read tuples.  mergelast[i] similarly
	 * points to the last pre-read tuple from each tape.  mergeavailslots[i]
	 * is the number of unused memtuples[] slots reserved for tape i, and
	 * mergeavailmem[i] is the amount of unused space allocated for tape i.
	 * mergefreelist and mergefirstfree keep track of unused locations in the
	 * memtuples[] array.  The memtuples[].tupindex fields link together
	 * pre-read tuples for each tape as well as recycled locations in
	 * mergefreelist. It is OK to use 0 as a null link in these lists, because
	 * memtuples[0] is part of the merge heap and is never a pre-read tuple.
	 */
	bool	   *mergeactive;	/* active input run source? */
	int		   *mergenext;		/* first preread tuple for each source */
	int		   *mergelast;		/* last preread tuple for each source */
	int		   *mergeavailslots;	/* slots left for prereading each tape */
	long	   *mergeavailmem;	/* availMem for prereading each tape */
	int			mergefreelist;	/* head of freelist of recycled slots */
	int			mergefirstfree; /* first slot never used in this merge */

	/*
	 * Variables for Algorithm D.  Note that destTape is a "logical" tape
	 * number, ie, an index into the tp_xxx[] arrays.  Be careful to keep
	 * "logical" and "actual" tape numbers straight!
	 */
	int			Level;			/* Knuth's l */
	int			destTape;		/* current output tape (Knuth's j, less 1) */
	int		   *tp_fib;			/* Target Fibonacci run counts (A[]) */
	int		   *tp_runs;		/* # of real runs on each tape */
	int		   *tp_dummy;		/* # of dummy runs for each tape (D[]) */
	int		   *tp_tapenum;		/* Actual tape numbers (TAPE[]) */
	int			activeTapes;	/* # of active input tapes in merge pass */

	LogicalTape  *result_tape;	/* actual tape of finished output */
	TuplesortPos pos; 			/* current postion */
	/*
	 * These variables are specific to the MinimalTuple case; they are set by
	 * tuplesort_begin_heap and used only by the MinimalTuple routines.
	 */
	TupleDesc	tupDesc;
	ScanKey		scanKeys;		/* array of length nKeys */

	MemTupleBinding *mt_bind;
	/*
	 * These variables are specific to the IndexTuple case; they are set by
	 * tuplesort_begin_index_xxx and used only by the IndexTuple routines.
	 */
	Relation	indexRel;		/* index being built */

	/* These are specific to the index_btree subcase: */
	ScanKey		indexScanKey;
	bool		enforceUnique;	/* complain if we find duplicate tuples */

	/* These are specific to the index_hash subcase: */
	uint32		hash_mask;		/* mask for sortable part of hash code */

	/*
	 * These variables are specific to the Datum case; they are set by
	 * tuplesort_begin_datum and used only by the DatumTuple routines.
	 */
	Oid			datumType;
	FmgrInfo	sortOpFn;		/* cached lookup data for sortOperator */
	int			sortFnFlags;	/* equivalent to sk_flags */
	/* we need typelen and byval in order to know how to copy the Datums. */
	int			datumTypeLen;
	bool		datumTypeByVal;

    /*
     * CDB: EXPLAIN ANALYZE reporting interface and statistics.
     */
    struct Instrumentation *instrument;
    struct StringInfoData  *explainbuf;
    uint64 spilledBytes;
	uint64 memUsedBeforeSpill; /* memory that is used at the time of spilling */

	/*
	 * Resource snapshot for time of sort start.
	 */
	PGRUsage	ru_start;

	/* 
	 * File for dump/load logical tape set state.  Used by sharing sort across slice
	 */
	char       *pfile_rwfile_prefix;
	ExecWorkFile *pfile_rwfile_state;

	/* gpmon */
	gpmon_packet_t *gpmon_pkt;
	int *gpmon_sort_tick;
};

#define COMPARETUP(state,a,b)	((*(state)->comparetup) (a, b, state))
#define COPYTUP(state,stup,tup) ((*(state)->copytup) (state, stup, tup))
#define WRITETUP(state,tape,stup)	((*(state)->writetup) (state, tape, stup))
#define READTUP(state,pos,stup,tape,len) ((*(state)->readtup) (state, pos, stup, tape, len))
#define REVERSEDIRECTION(state) ((*(state)->reversedirection) (state))
#define LACKMEM(state)		((state)->availMem < 0)

static inline void USEMEM(Tuplesortstate *state, int amt)
{
	state->availMem -= amt;
}

static inline void FREEMEM(Tuplesortstate *state, int amt)
{
	if (state->availMemMin > state->availMem)
		state->availMemMin = state->availMem;
	state->availMem += amt;
}

/*
 * NOTES about on-tape representation of tuples:
 *
 * We require the first "unsigned int" of a stored tuple to be the total size
 * on-tape of the tuple, including itself (so it is never zero; an all-zero
 * unsigned int is used to delimit runs).  The remainder of the stored tuple
 * may or may not match the in-memory representation of the tuple ---
 * any conversion needed is the job of the writetup and readtup routines.
 *
 * If state->randomAccess is true, then the stored representation of the
 * tuple must be followed by another "unsigned int" that is a copy of the
 * length --- so the total tape space used is actually sizeof(unsigned int)
 * more than the stored length value.  This allows read-backwards.	When
 * randomAccess is not true, the write/read routines may omit the extra
 * length word.
 *
 * writetup is expected to write both length words as well as the tuple
 * data.  When readtup is called, the tape is positioned just after the
 * front length word; readtup must read the tuple data and advance past
 * the back length word (if present).
 *
 * The write/read routines can make use of the tuple description data
 * stored in the Tuplesortstate record, if needed.	They are also expected
 * to adjust state->availMem by the amount of memory space (not tape space!)
 * released or consumed.  There is no error return from either writetup
 * or readtup; they should ereport() on failure.
 *
 *
 * NOTES about memory consumption calculations:
 *
 * We count space allocated for tuples against the workMem limit, plus
 * the space used by the variable-size memtuples array.  Fixed-size space
 * is not counted; it's small enough to not be interesting.
 *
 * Note that we count actual space used (as shown by GetMemoryChunkSpace)
 * rather than the originally-requested size.  This is important since
 * palloc can add substantial overhead.  It's not a complete answer since
 * we won't count any wasted space in palloc allocation blocks, but it's
 * a lot better than what we were doing before 7.3.
 */


static Tuplesortstate *tuplesort_begin_common(int workMem, bool randomAccess, bool allocmemtuple);
static void puttuple_common(Tuplesortstate *state, SortTuple *tuple);
static void inittapes(Tuplesortstate *state, const char* rwfile_prefix);
static void selectnewtape(Tuplesortstate *state);
static void mergeruns(Tuplesortstate *state);
static void mergeonerun(Tuplesortstate *state);
static void beginmerge(Tuplesortstate *state);
static void mergepreread(Tuplesortstate *state);
static void mergeprereadone(Tuplesortstate *state, int srcTape);
static void dumptuples(Tuplesortstate *state, bool alltuples);
static void make_bounded_heap(Tuplesortstate *state);
static void sort_bounded_heap(Tuplesortstate *state);
static void tuplesort_heap_insert(Tuplesortstate *state, SortTuple *tuple,
					  int tupleindex, bool checkIndex);
static void tuplesort_heap_siftup(Tuplesortstate *state, bool checkIndex);
static unsigned int getlen(Tuplesortstate *state, TuplesortPos *pos, LogicalTape *lt, bool eofOK);
static void markrunend(Tuplesortstate *state, int tapenum);
static int comparetup_heap(const SortTuple *a, const SortTuple *b,
				Tuplesortstate *state);
static void copytup_heap(Tuplesortstate *state, SortTuple *stup, void *tup);
static void writetup_heap(Tuplesortstate *state, LogicalTape *lt, SortTuple *stup);
static void readtup_heap(Tuplesortstate *state, TuplesortPos *pos, SortTuple *stup,
			 LogicalTape *lt, unsigned int len);
static void reversedirection_heap(Tuplesortstate *state);
static int comparetup_index_btree(const SortTuple *a, const SortTuple *b,
				 Tuplesortstate *state);
static int comparetup_index_hash(const SortTuple *a, const SortTuple *b,
				 Tuplesortstate *state);
static void copytup_index(Tuplesortstate *state, SortTuple *stup, void *tup);
static void writetup_index(Tuplesortstate *state, LogicalTape *lt, SortTuple *stup);
static void readtup_index(Tuplesortstate *state, TuplesortPos *pos, SortTuple *stup,
			  LogicalTape *lt, unsigned int len);
static void reversedirection_index_btree(Tuplesortstate *state);
static void reversedirection_index_hash(Tuplesortstate *state);
static int comparetup_datum(const SortTuple *a, const SortTuple *b,
				 Tuplesortstate *state);
static void copytup_datum(Tuplesortstate *state, SortTuple *stup, void *tup);
static void writetup_datum(Tuplesortstate *state, LogicalTape *lt, SortTuple *stup);
static void readtup_datum(Tuplesortstate *state, TuplesortPos *pos, SortTuple *stup,
			  LogicalTape *lt, unsigned int len);
static void reversedirection_datum(Tuplesortstate *state);
static void free_sort_tuple(Tuplesortstate *state, SortTuple *stup);

static void tuplesort_sorted_insert(Tuplesortstate *state, SortTuple *tuple,
					  int tupleindex, bool checkIndex);

/*
 *		tuplesort_begin_xxx
 *
 * Initialize for a tuple sort operation.
 *
 * After calling tuplesort_begin, the caller should call tuplesort_putXXX
 * zero or more times, then call tuplesort_performsort when all the tuples
 * have been supplied.	After performsort, retrieve the tuples in sorted
 * order by calling tuplesort_getXXX until it returns false/NULL.  (If random
 * access was requested, rescan, markpos, and restorepos can also be called.)
 * Call tuplesort_end to terminate the operation and release memory/disk space.
 *
 * Each variant of tuplesort_begin has a workMem parameter specifying the
 * maximum number of kilobytes of RAM to use before spilling data to disk.
 * (The normal value of this parameter is work_mem, but some callers use
 * other values.)  Each variant also has a randomAccess parameter specifying
 * whether the caller needs non-sequential access to the sort result.
 *
 * CDB: During EXPLAIN ANALYZE, after tuplesort_begin_xxx() the caller should
 * use tuplesort_set_instrument() (q.v.) to enable statistical reporting.
 */

static Tuplesortstate *
tuplesort_begin_common(int workMem, bool randomAccess, bool allocmemtuple)
{
	Tuplesortstate *state;
	MemoryContext sortcontext;
	MemoryContext oldcontext;

	/*
	 * Create a working memory context for this sort operation. All data
	 * needed by the sort will live inside this context.
	 */
	sortcontext = AllocSetContextCreate(CurrentMemoryContext,
										"TupleSort",
										ALLOCSET_DEFAULT_MINSIZE,
										ALLOCSET_DEFAULT_INITSIZE,
										ALLOCSET_DEFAULT_MAXSIZE);

	/*
	 * Make the Tuplesortstate within the per-sort context.  This way, we
	 * don't need a separate pfree() operation for it at shutdown.
	 */
	oldcontext = MemoryContextSwitchTo(sortcontext);

	state = (Tuplesortstate *) palloc0(sizeof(Tuplesortstate));

	if (trace_sort)
		pg_rusage_init(&state->ru_start);

	state->status = TSS_INITIAL;
	state->randomAccess = randomAccess;
	state->bounded = false;
	state->boundUsed = false;
	state->allowedMem = workMem * 1024L;
	state->availMemMin = state->availMem = state->allowedMem;
	state->availMemMin01 = state->availMemMin;
	state->sortcontext = sortcontext;
	state->tapeset = NULL;

	state->memtupcount = 0;
	state->memtupsize = 1024;	/* initial guess */
	state->memtupblimited = false;
	state->discardcount = 0; /*CDB*/
	state->totalNumTuples  = 0; /*CDB*/
	state->mppsortflags = 0;   /* special sort flags*//* CDB */
	state->standardsort = true; /* normal sort *//* CDB */
	state->gpmaxdistinct = 20000; /* maximum distinct values *//* CDB */

	if(allocmemtuple)
	{
		state->memtuples = (SortTuple *) palloc(state->memtupsize * sizeof(SortTuple));
		USEMEM(state, GetMemoryChunkSpace(state->memtuples));

		/* workMem must be large enough for the minimal memtuples array */
		if (LACKMEM(state))
			elog(ERROR, "insufficient memory allowed for sort");
	} 
	else
	{
		state->memtuples = NULL;
	}
	

	state->currentRun = 0;

	/*
	 * maxTapes, tapeRange, and Algorithm D variables will be initialized by
	 * inittapes(), if needed
	 */

	state->result_tape = NULL;	/* flag that result tape has not been formed */
	state->pos.cur_work_tape = NULL; /* flag no work tape associated with pos */

	MemoryContextSwitchTo(oldcontext);

	Assert(!state->statsFinalized);

	return state;
}

Tuplesortstate *
tuplesort_begin_heap(ScanState *ss, TupleDesc tupDesc,
					 int nkeys, AttrNumber *attNums,
					 Oid *sortOperators, bool *nullsFirstFlags,
					 int workMem, bool randomAccess)
{
	Tuplesortstate *state = tuplesort_begin_common(workMem, randomAccess, true);
	MemoryContext oldcontext;
	int			i;

	oldcontext = MemoryContextSwitchTo(state->sortcontext);

	AssertArg(nkeys > 0);

	if (trace_sort)
		elog(LOG,
			 "begin tuple sort: nkeys = %d, workMem = %d, randomAccess = %c",
			 nkeys, workMem, randomAccess ? 't' : 'f');

	TRACE_POSTGRESQL_SORT_START(HEAP_SORT, false, nkeys, workMem, randomAccess);

	state->nKeys = nkeys;

	state->comparetup = comparetup_heap;
	state->copytup = copytup_heap;
	state->writetup = writetup_heap;
	state->readtup = readtup_heap;
	state->reversedirection = reversedirection_heap;

	state->tupDesc = tupDesc;	/* assume we need not copy tupDesc */
	state->mt_bind = create_memtuple_binding(tupDesc);

	state->scanKeys = (ScanKey) palloc0(nkeys * sizeof(ScanKeyData));

	for (i = 0; i < nkeys; i++)
	{
		Oid			sortFunction;
		bool		reverse;

		AssertArg(attNums[i] != 0);
		AssertArg(sortOperators[i] != 0);

		if (!get_compare_function_for_ordering_op(sortOperators[i],
												  &sortFunction, &reverse))
			elog(ERROR, "operator %u is not a valid ordering operator",
				 sortOperators[i]);

		/*
		 * We needn't fill in sk_strategy or sk_subtype since these scankeys
		 * will never be passed to an index.
		 */
		ScanKeyInit(&state->scanKeys[i],
					attNums[i],
					InvalidStrategy,
					sortFunction,
					(Datum) 0);

		/* However, we use btree's conventions for encoding directionality */
		if (reverse)
			state->scanKeys[i].sk_flags |= SK_BT_DESC;
		if (nullsFirstFlags[i])
			state->scanKeys[i].sk_flags |= SK_BT_NULLS_FIRST;
	}

	MemoryContextSwitchTo(oldcontext);

	return state;
}

Tuplesortstate *
tuplesort_begin_index_btree(Relation indexRel,
							bool enforceUnique,
							int workMem, bool randomAccess)
{
	Tuplesortstate *state = tuplesort_begin_common(workMem, randomAccess, true);
	MemoryContext oldcontext;

	oldcontext = MemoryContextSwitchTo(state->sortcontext);

	if (trace_sort)
		elog(LOG,
			 "begin index sort: unique = %c, workMem = %d, randomAccess = %c",
			 enforceUnique ? 't' : 'f',
			 workMem, randomAccess ? 't' : 'f');

	state->nKeys = RelationGetNumberOfAttributes(indexRel);

	TRACE_POSTGRESQL_SORT_START(INDEX_SORT, enforceUnique, state->nKeys, workMem, randomAccess);

	state->comparetup = comparetup_index_btree;
	state->copytup = copytup_index;
	state->writetup = writetup_index;
	state->readtup = readtup_index;
	state->reversedirection = reversedirection_index_btree;

	state->indexRel = indexRel;
	state->indexScanKey = _bt_mkscankey_nodata(indexRel);
	state->enforceUnique = enforceUnique;

	MemoryContextSwitchTo(oldcontext);

	return state;
}

Tuplesortstate *
tuplesort_begin_index_hash(Relation indexRel,
						   uint32 hash_mask,
						   int workMem, bool randomAccess)
{
	Tuplesortstate *state = tuplesort_begin_common(workMem, randomAccess, true);
	MemoryContext oldcontext;

	oldcontext = MemoryContextSwitchTo(state->sortcontext);

#ifdef TRACE_SORT
	if (trace_sort)
		elog(LOG,
			 "begin index sort: hash_mask = 0x%x, workMem = %d, randomAccess = %c",
			 hash_mask,
			 workMem, randomAccess ? 't' : 'f');
#endif

	state->nKeys = 1;			/* Only one sort column, the hash code */

	state->comparetup = comparetup_index_hash;
	state->copytup = copytup_index;
	state->writetup = writetup_index;
	state->readtup = readtup_index;
	state->reversedirection = reversedirection_index_hash;

	state->indexRel = indexRel;

	state->hash_mask = hash_mask;

	MemoryContextSwitchTo(oldcontext);

	return state;
}

Tuplesortstate *
tuplesort_begin_datum(ScanState *ss, Oid datumType,
					  Oid sortOperator, bool nullsFirstFlag,
					  int workMem, bool randomAccess)
{
	Tuplesortstate *state = tuplesort_begin_common(workMem, randomAccess, true);
	MemoryContext oldcontext;
	Oid			sortFunction;
	bool		reverse;
	int16		typlen;
	bool		typbyval;

	oldcontext = MemoryContextSwitchTo(state->sortcontext);

	if (trace_sort)
		elog(LOG,
			 "begin datum sort: workMem = %d, randomAccess = %c",
			 workMem, randomAccess ? 't' : 'f');

	TRACE_POSTGRESQL_SORT_START(DATUM_SORT, false, 1, workMem, randomAccess);

	state->nKeys = 1;			/* always a one-column sort */

	state->comparetup = comparetup_datum;
	state->copytup = copytup_datum;
	state->writetup = writetup_datum;
	state->readtup = readtup_datum;
	state->reversedirection = reversedirection_datum;

	state->datumType = datumType;

	/* lookup the ordering function */
	if (!get_compare_function_for_ordering_op(sortOperator,
											  &sortFunction, &reverse))
		elog(ERROR, "operator %u is not a valid ordering operator",
			 sortOperator);
	fmgr_info(sortFunction, &state->sortOpFn);

	/* set ordering flags */
	state->sortFnFlags = reverse ? SK_BT_DESC : 0;
	if (nullsFirstFlag)
		state->sortFnFlags |= SK_BT_NULLS_FIRST;

	/* lookup necessary attributes of the datum type */
	get_typlenbyval(datumType, &typlen, &typbyval);
	state->datumTypeLen = typlen;
	state->datumTypeByVal = typbyval;

	MemoryContextSwitchTo(oldcontext);

	return state;
}

/*
 * tuplesort_set_bound
 *
 *	Advise tuplesort that at most the first N result tuples are required.
 *
 * Must be called before inserting any tuples.	(Actually, we could allow it
 * as long as the sort hasn't spilled to disk, but there seems no need for
 * delayed calls at the moment.)
 *
 * This is a hint only. The tuplesort may still return more tuples than
 * requested.
 */
void
tuplesort_set_bound(Tuplesortstate *state, int64 bound)
{
	/* Assert we're called before loading any tuples */
	Assert(state->status == TSS_INITIAL);
	Assert(state->memtupcount == 0);
	Assert(!state->bounded);

#ifdef DEBUG_BOUNDED_SORT
	/* Honor GUC setting that disables the feature (for easy testing) */
	if (!optimize_bounded_sort)
		return;
#endif

	/* We want to be able to compute bound * 2, so limit the setting */
	if (bound > (int64) (INT_MAX / 2))
		return;

	state->bounded = true;
	state->bound = (int) bound;
}

/*
 * tuplesort_end
 *
 *	Release resources and clean up.
 *
 * NOTE: after calling this, any pointers returned by tuplesort_getXXX are
 * pointing to garbage.  Be careful not to attempt to use or free such
 * pointers afterwards!
 */
void
tuplesort_end(Tuplesortstate *state)
{
	/* context swap probably not needed, but let's be safe */
	MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);

	long		spaceUsed;

	if (state->tapeset)
		spaceUsed = LogicalTapeSetBlocks(state->tapeset);
	else
		spaceUsed = (state->allowedMem - state->availMem + 1023) / 1024;

	/*
	 * Call before state->tapeset is closed.
	 */
	tuplesort_finalize_stats(state);

	/*
	 * Delete temporary "tape" files, if any.
	 *
	 * Note: want to include this in reported total cost of sort, hence need
	 * for two #ifdef TRACE_SORT sections.
	 */
	if (state->tapeset)
	{
		LogicalTapeSetClose(state->tapeset, NULL /* workset */);

		if (state->pfile_rwfile_state)
        {
			workfile_mgr_close_file(NULL /* workset */, state->pfile_rwfile_state);
        }
	}

	if (trace_sort)
	{
		if (state->tapeset)
			elog(LOG, "external sort ended, %ld disk blocks used: %s",
				 spaceUsed, pg_rusage_show(&state->ru_start));
		else
			elog(LOG, "internal sort ended, %ld KB used: %s",
				 spaceUsed, pg_rusage_show(&state->ru_start));
	}

	TRACE_POSTGRESQL_SORT_DONE(state->tapeset,
			(state->tapeset ? LogicalTapeSetBlocks(state->tapeset) :
			(state->allowedMem - state->availMem + 1023) / 1024));


	MemoryContextSwitchTo(oldcontext);

	/*
	 * Free the per-sort memory context, thereby releasing all working memory,
	 * including the Tuplesortstate struct itself.
	 */
	MemoryContextDelete(state->sortcontext);
}

/*
 * Grow the memtuples[] array, if possible within our memory constraint.
 * Return TRUE if able to enlarge the array, FALSE if not.
 *
 * At each increment we double the size of the array.  When we are short
 * on memory we could consider smaller increases, but because availMem
 * moves around with tuple addition/removal, this might result in thrashing.
 * Small increases in the array size are likely to be pretty inefficient.
 */
static bool
grow_memtuples(Tuplesortstate *state)
{
	/*
	 * We need to be sure that we do not cause LACKMEM to become true, else
	 * the space management algorithm will go nuts.  We assume here that the
	 * memory chunk overhead associated with the memtuples array is constant
	 * and so there will be no unexpected addition to what we ask for.	(The
	 * minimum array size established in tuplesort_begin_common is large
	 * enough to force palloc to treat it as a separate chunk, so this
	 * assumption should be good.  But let's check it.)
	 */
	if (state->availMem <= (long) (state->memtupsize * sizeof(SortTuple)))
		return false;

	/*
	 * On a 64-bit machine, allowedMem could be high enough to get us into
	 * trouble with MaxAllocSize, too.
	 */
	if ((Size) (state->memtupsize * 2) >= MaxAllocSize / sizeof(SortTuple))
		return false;

	FREEMEM(state, GetMemoryChunkSpace(state->memtuples));
	state->memtupsize *= 2;
	state->memtuples = (SortTuple *)
		repalloc(state->memtuples,
				 state->memtupsize * sizeof(SortTuple));
	USEMEM(state, GetMemoryChunkSpace(state->memtuples));
	if (LACKMEM(state))
		elog(ERROR, "unexpected out-of-memory situation during sort");
	return true;
}

/*
 * Accept one tuple while collecting input data for sort.
 *
 * Note that the input data is always copied; the caller need not save it.
 */
void
tuplesort_puttupleslot(Tuplesortstate *state, TupleTableSlot *slot)
{
	MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);
	SortTuple	stup;

	/*
	 * Copy the given tuple into memory we control, and decrease availMem.
	 * Then call the common code.
	 */
	COPYTUP(state, &stup, (void *) slot);

	puttuple_common(state, &stup);

	MemoryContextSwitchTo(oldcontext);
}

/*
 * Accept one index tuple while collecting input data for sort.
 *
 * Note that the input tuple is always copied; the caller need not save it.
 */
void
tuplesort_putindextuple(Tuplesortstate *state, IndexTuple tuple)
{
	MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);
	SortTuple	stup;

	/*
	 * Copy the given tuple into memory we control, and decrease availMem.
	 * Then call the common code.
	 */
	COPYTUP(state, &stup, (void *) tuple);

	puttuple_common(state, &stup);

	MemoryContextSwitchTo(oldcontext);
}

/*
 * Accept one Datum while collecting input data for sort.
 *
 * If the Datum is pass-by-ref type, the value will be copied.
 */
void
tuplesort_putdatum(Tuplesortstate *state, Datum val, bool isNull)
{
	MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);
	SortTuple	stup;

	/*
	 * If it's a pass-by-reference value, copy it into memory we control, and
	 * decrease availMem.  Then call the common code.
	 */
	if (isNull || state->datumTypeByVal)
	{
		stup.datum1 = val;
		stup.isnull1 = isNull;
		stup.tuple = NULL;		/* no separate storage */
	}
	else
	{
		stup.datum1 = datumCopy(val, false, state->datumTypeLen);
		stup.isnull1 = false;
		stup.tuple = (MemTuple) DatumGetPointer(stup.datum1);
		USEMEM(state, GetMemoryChunkSpace(DatumGetPointer(stup.datum1))); 
	}

	puttuple_common(state, &stup);

	MemoryContextSwitchTo(oldcontext);
}

/*
 * Shared code for tuple and datum cases.
 */
static void
puttuple_common(Tuplesortstate *state, SortTuple *tuple)
{
	int do_standardsort = (state->memtupcount == 0) || state->standardsort;

	state->totalNumTuples++;

	switch (state->status)
	{
		case TSS_INITIAL:

			/*
			 * Save the tuple into the unsorted array.	First, grow the array
			 * as needed.  Note that we try to grow the array when there is
			 * still one free slot remaining --- if we fail, there'll still be
			 * room to store the incoming tuple, and then we'll switch to
			 * tape-based operation.
			 */
			if (state->memtupcount >= state->memtupsize - 1)
			{
				(void) grow_memtuples(state);
				Assert(state->memtupcount < state->memtupsize);
			}

			if (do_standardsort)
			{
				tuple->tupindex = 0; /* MPP-1561: always safe to set
									  * the index to zero, which
									  * matches the behavior of
									  * tuplesort_sorted_insert and
									  * inittapes */
				state->memtuples[state->memtupcount++] = *tuple;
			}
			else
			{
				/* sorting LIMIT or noduplicates: only one run */
				tuplesort_sorted_insert(state, tuple, 0, false);

				/* check if have sufficient discards to justify
				 * fullsort 
				 */

                /*
                 * don't bother if mppsortflags set to "always on"
                 */
                if ((state->mppsortflags > 1) &&
					(state->totalNumTuples >= state->mppsortflags))
                {
                    int64 discard_ratio =
                        (state->discardcount * 100)/state->totalNumTuples;
                    if (discard_ratio < 50)
                    {
                         state->standardsort = true;
                    }

					/* MPP-1342: limit the number of distinct values */
					if (state->memtupcount > state->gpmaxdistinct)
                    {
                         state->standardsort = true;
                    }

                }
			}

			/*
			 * Check if it's time to switch over to a bounded heapsort. We do
			 * so if the input tuple count exceeds twice the desired tuple
			 * count (this is a heuristic for where heapsort becomes cheaper
			 * than a quicksort), or if we've just filled workMem and have
			 * enough tuples to meet the bound.
			 *
			 * Note that once we enter TSS_BOUNDED state we will always try to
			 * complete the sort that way.	In the worst case, if later input
			 * tuples are larger than earlier ones, this might cause us to
			 * exceed workMem significantly.
			 */
			if (state->bounded &&
				(state->memtupcount > state->bound * 2 ||
				 (state->memtupcount > state->bound && LACKMEM(state))))
			{
#ifdef TRACE_SORT
				if (trace_sort)
					elog(LOG, "switching to bounded heapsort at %d tuples: %s",
						 state->memtupcount,
						 pg_rusage_show(&state->ru_start));
#endif
				make_bounded_heap(state);
				return;
			}

			/*
			 * Done if we still fit in available memory and have array slots.
			 */
			if (state->memtupcount < state->memtupsize && !LACKMEM(state))
				return;

			state->memUsedBeforeSpill = MemoryContextGetPeakSpace(state->sortcontext);

			/*
			 * Nope; time to switch to tape-based operation.
			 */
			inittapes(state, state->pfile_rwfile_state ? state->pfile_rwfile_prefix : NULL);

			/*
			 * Dump tuples until we are back under the limit.
			 */
			dumptuples(state, false);

			if (state->instrument)
			{
				state->instrument->workfileCreated = true;
			}

			break;

		case TSS_BOUNDED:

			/*
			 * We don't want to grow the array here, so check whether the new
			 * tuple can be discarded before putting it in.  This should be a
			 * good speed optimization, too, since when there are many more
			 * input tuples than the bound, most input tuples can be discarded
			 * with just this one comparison.  Note that because we currently
			 * have the sort direction reversed, we must check for <= not >=.
			 */
			if (COMPARETUP(state, tuple, &state->memtuples[0]) <= 0)
			{
				/* new tuple <= top of the heap, so we can discard it */
				free_sort_tuple(state, tuple);
			}
			else
			{
				/* discard top of heap, sift up, insert new tuple */
				free_sort_tuple(state, &state->memtuples[0]);
				tuplesort_heap_siftup(state, false);
				tuplesort_heap_insert(state, tuple, 0, false);
			}
			break;

		case TSS_BUILDRUNS:

			/*
			 * Insert the tuple into the heap, with run number currentRun if
			 * it can go into the current run, else run number currentRun+1.
			 * The tuple can go into the current run if it is >= the first
			 * not-yet-output tuple.  (Actually, it could go into the current
			 * run if it is >= the most recently output tuple ... but that
			 * would require keeping around the tuple we last output, and it's
			 * simplest to let writetup free each tuple as soon as it's
			 * written.)
			 *
			 * Note there will always be at least one tuple in the heap at
			 * this point; see dumptuples.
			 */
			Assert(state->memtupcount > 0);
			if (COMPARETUP(state, tuple, &state->memtuples[0]) >= 0)
				tuplesort_heap_insert(state, tuple, state->currentRun, true);
			else
				tuplesort_heap_insert(state, tuple, state->currentRun + 1, true);

			/*
			 * If we are over the memory limit, dump tuples till we're under.
			 */
			dumptuples(state, false);
			break;

		default:
			elog(ERROR, "invalid tuplesort state");
			break;
	}
}

/*
 * All tuples have been provided; finish the sort.
 */
void
tuplesort_performsort(Tuplesortstate *state)
{
	MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);

	if (trace_sort)
		elog(LOG, "performsort starting: %s",
			 pg_rusage_show(&state->ru_start));

	/*
	 * If we the output needs to be shared by other readers, we need to
	 * materialize it on a tape, even if it fit in memory.
	 */
	if (state->status == TSS_INITIAL && NULL != state->pfile_rwfile_state)
		inittapes(state, state->pfile_rwfile_prefix);

	switch (state->status)
	{
		case TSS_INITIAL:
			/*
			 * We were able to accumulate all the tuples within the allowed
			 * amount of memory.  Just qsort 'em and we're done.
			 */
			if ((state->memtupcount > 1)
				&& state->standardsort)
				qsort_arg((void *) state->memtuples,
						  state->memtupcount,
						  sizeof(SortTuple),
						  (qsort_arg_comparator) state->comparetup,
						  (void *) state);
			state->pos.current = 0;
			state->pos.eof_reached = false;
			state->pos.markpos.mempos = 0;
			state->pos.markpos_eof = false;
			state->status = TSS_SORTEDINMEM;
			break;

		case TSS_BOUNDED:

			/*
			 * We were able to accumulate all the tuples required for output
			 * in memory, using a heap to eliminate excess tuples.	Now we
			 * have to transform the heap to a properly-sorted array.
			 */
			sort_bounded_heap(state);
			state->pos.current = 0;
			state->pos.eof_reached = false;
			state->pos.markpos.tapepos.blkNum = 0L; 
			state->pos.markpos.tapepos.offset = 0;
			state->pos.markpos_eof = false;
			state->status = TSS_SORTEDINMEM;
			break;

		case TSS_BUILDRUNS:

			/*
			 * Finish tape-based sort.	First, flush all tuples remaining in
			 * memory out to tape; then merge until we have a single remaining
			 * run (or, if !randomAccess, one run per tape). Note that
			 * mergeruns sets the correct state->status.
			 */
			dumptuples(state, true);

			/* CDB: How much work_mem would be enough for in-memory sort? */
			if (state->instrument)
			{
				/*
				 * The workmemwanted is summed up of the following:
				 * (1) metadata: Tuplesortstate, tuple array
				 * (2) the total bytes for all tuples.
				 */
				int64   workmemwanted = 
					sizeof(Tuplesortstate) +
					((uint64)(1 << my_log2(state->totalNumTuples))) * sizeof(SortTuple) +
					state->spilledBytes;

				state->instrument->workmemwanted =
					Max(state->instrument->workmemwanted, workmemwanted);
			}

			mergeruns(state);
			state->pos.eof_reached = false;
			state->pos.markpos.tapepos.blkNum = 0L; 
			state->pos.markpos.tapepos.offset = 0;
			state->pos.markpos_eof = false;
			break;

		default:
			elog(ERROR, "invalid tuplesort state");
			break;
	}

	if (trace_sort)
	{
		if (state->status == TSS_FINALMERGE)
			elog(LOG, "performsort done (except %d-way final merge): %s",
				 state->activeTapes,
				 pg_rusage_show(&state->ru_start));
		else
			elog(LOG, "performsort done: %s",
				 pg_rusage_show(&state->ru_start));
	}

	/* MPP-1559 */
	state->availMemMin01 = state->availMemMin;

	MemoryContextSwitchTo(oldcontext);
}

/*
 * Internal routine to fetch the next tuple in either forward or back
 * direction into *stup.  Returns FALSE if no more tuples.
 * If *should_free is set, the caller must pfree stup.tuple when done with it.
 */
static bool
tuplesort_gettuple_common_pos(Tuplesortstate *state, TuplesortPos *pos,
		bool forward, SortTuple *stup, bool *should_free)
{
	unsigned int tuplen;
	LogicalTape *work_tape;
	bool fOK;

	switch (state->status)
	{
		case TSS_SORTEDINMEM:
			Assert(forward || state->randomAccess);
			*should_free = false;
			if (forward)
			{
				if (pos->current < state->memtupcount)
				{
					*stup = state->memtuples[pos->current++];
					return true;
				}
				pos->eof_reached = true;

				/*
				 * Complain if caller tries to retrieve more tuples than
				 * originally asked for in a bounded sort.	This is because
				 * returning EOF here might be the wrong thing.
				 */
				if (state->bounded && state->pos.current >= state->bound)
					elog(ERROR, "retrieved too many tuples in a bounded sort");

				return false;
			}
			else
			{
				if (pos->current <= 0)
					return false;

				/*
				 * if all tuples are fetched already then we return last
				 * tuple, else - tuple before last returned.
				 */
				if (pos->eof_reached)
					pos->eof_reached = false;
				else
				{
					pos->current--;	/* last returned tuple */
					if (pos->current <= 0)
						return false;
				}
				*stup = state->memtuples[pos->current - 1];
				return true;
			}
			break;

		case TSS_SORTEDONTAPE:
			AssertEquivalent((pos == &state->pos), (pos->cur_work_tape == NULL));
			Assert(forward || state->randomAccess);
			*should_free = true;
			work_tape =	pos->cur_work_tape == NULL ? state->result_tape : pos->cur_work_tape; 

			if (forward)
			{
				if (pos->eof_reached)
					return false;
				if ((tuplen = getlen(state, pos, work_tape, true)) != 0) 
				{
					READTUP(state, pos, stup, work_tape, tuplen);
					return true;
				}
				else
				{
					pos->eof_reached = true;
					return false;
				}
			}

			/*
			 * Backward.
			 *
			 * if all tuples are fetched already then we return last tuple,
			 * else - tuple before last returned.
			 */
			/*
			 * Seek position is pointing just past the zero tuplen at the
			 * end of file; back up to fetch last tuple's ending length
			 * word.  If seek fails we must have a completely empty file.
			 */
			fOK = LogicalTapeBackspace(state->tapeset, work_tape,  2*sizeof(unsigned));
			if(!fOK)
				return false;

			if (pos->eof_reached)
			{
				pos->eof_reached = false;
			}
			else
			{
				tuplen = getlen(state, pos, work_tape, false);

				/*
				 * Back up to get ending length word of tuple before it.
				 */
				fOK = LogicalTapeBackspace(state->tapeset, work_tape, tuplen + 2 *sizeof(unsigned));

				if (!fOK)
				{
					/*
					 * If that fails, presumably the prev tuple is the first
					 * in the file.  Back up so that it becomes next to read
					 * in forward direction (not obviously right, but that is
					 * what in-memory case does).
					 */
					fOK = LogicalTapeBackspace(state->tapeset, work_tape, tuplen + 2 *sizeof(unsigned));
					if(!fOK)
						elog(ERROR, "bogus tuple length in backward scan");

					return false;
				}
			}

			tuplen = getlen(state, pos, work_tape, false);

			/*
			 * Now we have the length of the prior tuple, back up and read it.
			 * Note: READTUP expects we are positioned after the initial
			 * length word of the tuple, so back up to that point.
			 */
			fOK = LogicalTapeBackspace(state->tapeset, work_tape, tuplen);
			if (!fOK) 
				elog(ERROR, "bogus tuple length in backward scan");

			READTUP(state, pos, stup, work_tape, tuplen);
			return true;

		case TSS_FINALMERGE:
			Assert(forward);
			Assert(pos == &state->pos && pos->cur_work_tape == NULL);

			*should_free = true;

			/*
			 * This code should match the inner loop of mergeonerun().
			 */
			if (state->memtupcount > 0)
			{
				int			srcTape = state->memtuples[0].tupindex;
				Size		tuplen;
				int			tupIndex;
				SortTuple  *newtup;

				*stup = state->memtuples[0];
				/* returned tuple is no longer counted in our memory space */
				if (stup->tuple)
				{
					tuplen = GetMemoryChunkSpace(stup->tuple);
					FREEMEM(state, tuplen);
					state->mergeavailmem[srcTape] += tuplen;
				}
				tuplesort_heap_siftup(state, false);
				if ((tupIndex = state->mergenext[srcTape]) == 0)
				{
					/*
					 * out of preloaded data on this tape, try to read more
					 *
					 * Unlike mergeonerun(), we only preload from the single
					 * tape that's run dry.  See mergepreread() comments.
					 */
					mergeprereadone(state, srcTape);

					/*
					 * if still no data, we've reached end of run on this tape
					 */
					if ((tupIndex = state->mergenext[srcTape]) == 0)
						return true;
				}
				/* pull next preread tuple from list, insert in heap */
				newtup = &state->memtuples[tupIndex];
				state->mergenext[srcTape] = newtup->tupindex;
				if (state->mergenext[srcTape] == 0)
					state->mergelast[srcTape] = 0;
				tuplesort_heap_insert(state, newtup, srcTape, false);
				/* put the now-unused memtuples entry on the freelist */
				newtup->tupindex = state->mergefreelist;
				state->mergefreelist = tupIndex;
				state->mergeavailslots[srcTape]++;
				return true;
			}
			return false;

		default:
			elog(ERROR, "invalid tuplesort state");
			return false;		/* keep compiler quiet */
	}
}

/*
 * Fetch the next tuple in either forward or back direction.
 * If successful, put tuple in slot and return TRUE; else, clear the slot
 * and return FALSE.
 */
bool
tuplesort_gettupleslot(Tuplesortstate *state, bool forward,
					   TupleTableSlot *slot)
{
	return tuplesort_gettupleslot_pos(state, &state->pos, forward, slot, state->sortcontext);
}

bool
tuplesort_gettupleslot_pos(Tuplesortstate *state, TuplesortPos *pos,
		bool forward, TupleTableSlot *slot, MemoryContext mcontext)
{
	MemoryContext oldcontext = MemoryContextSwitchTo(mcontext);
	SortTuple	stup;
	bool		should_free = false;

	if (!tuplesort_gettuple_common_pos(state, pos, forward, &stup, &should_free))
		stup.tuple = NULL;

	MemoryContextSwitchTo(oldcontext);

	if (stup.tuple)
	{
		ExecStoreMinimalTuple(stup.tuple, slot, should_free);
		if (state->gpmon_pkt)
			Gpmon_Incr_Rows_Out(state->gpmon_pkt);
		return true;
	}
	else
	{
		ExecClearTuple(slot);
		return false;
	}
}

/*
 * Fetch the next index tuple in either forward or back direction.
 * Returns NULL if no more tuples.	If *should_free is set, the
 * caller must pfree the returned tuple when done with it.
 */
IndexTuple
tuplesort_getindextuple(Tuplesortstate *state, bool forward,
						bool *should_free)
{
	MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);
	SortTuple	stup;

	if (!tuplesort_gettuple_common_pos(state, &state->pos, forward, &stup, should_free))
		stup.tuple = NULL;

	MemoryContextSwitchTo(oldcontext);

	return (IndexTuple) stup.tuple;
}

/*
 * Fetch the next Datum in either forward or back direction.
 * Returns FALSE if no more datums.
 *
 * If the Datum is pass-by-ref type, the returned value is freshly palloc'd
 * and is now owned by the caller.
 */
bool
tuplesort_getdatum(Tuplesortstate *state, bool forward,
				   Datum *val, bool *isNull)
{
	MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);
	SortTuple	stup;
	bool		should_free = false;

	if (!tuplesort_gettuple_common_pos(state, &state->pos, forward, &stup, &should_free))
	{
		MemoryContextSwitchTo(oldcontext);
		return false;
	}

	if (stup.isnull1 || state->datumTypeByVal)
	{
		*val = stup.datum1;
		*isNull = stup.isnull1;
	}
	else
	{
		if (should_free)
			*val = stup.datum1;
		else
			*val = datumCopy(stup.datum1, false, state->datumTypeLen);
		*isNull = false;
	}

	MemoryContextSwitchTo(oldcontext);

	return true;
}

/*
 * Advance over N tuples in either forward or back direction,
 * without returning any data.  N==0 is a no-op.
 * Returns TRUE if successful, FALSE if ran out of tuples.
 */
bool
tuplesort_skiptuples(Tuplesortstate *state, int64 ntuples, bool forward)
{
	/*
	 * We don't actually support backwards skip yet, because no callers need
	 * it.	The API is designed to allow for that later, though.
	 */
	Assert(forward);
	Assert(ntuples >= 0);

	switch (state->status)
	{
		case TSS_SORTEDINMEM:
			if (state->memtupcount - state->pos.current >= ntuples)
			{
				state->pos.current += ntuples;
				return true;
			}
			state->pos.current = state->memtupcount;
			state->pos.eof_reached = true;

			/*
			 * Complain if caller tries to retrieve more tuples than
			 * originally asked for in a bounded sort.	This is because
			 * returning EOF here might be the wrong thing.
			 */
			if (state->bounded && state->pos.current >= state->bound)
				elog(ERROR, "retrieved too many tuples in a bounded sort");

			return false;

		case TSS_SORTEDONTAPE:
		case TSS_FINALMERGE:

			/*
			 * We could probably optimize these cases better, but for now it's
			 * not worth the trouble.
			 */
			while (ntuples-- > 0)
			{
				SortTuple	stup;
				bool		should_free;

				if (!tuplesort_gettuple_common_pos(state, &state->pos, forward,
											   &stup, &should_free))
					return false;
				if (should_free)
					pfree(stup.tuple);
				CHECK_FOR_INTERRUPTS();
			}
			return true;

		default:
			elog(ERROR, "invalid tuplesort state");
			return false;		/* keep compiler quiet */
	}
}

/*
 * tuplesort_merge_order - report merge order we'll use for given memory
 * (note: "merge order" just means the number of input tapes in the merge).
 *
 * This is exported for use by the planner.  allowedMem is in bytes.
 */
int
tuplesort_merge_order(long allowedMem)
{
	int			mOrder;

	/*
	 * We need one tape for each merge input, plus another one for the output,
	 * and each of these tapes needs buffer space.  In addition we want
	 * MERGE_BUFFER_SIZE workspace per input tape (but the output tape doesn't
	 * count).
	 *
	 * Note: you might be thinking we need to account for the memtuples[]
	 * array in this calculation, but we effectively treat that as part of the
	 * MERGE_BUFFER_SIZE workspace.
	 */
	mOrder = (allowedMem - TAPE_BUFFER_OVERHEAD) /
		(MERGE_BUFFER_SIZE + TAPE_BUFFER_OVERHEAD);

	mOrder = Max(mOrder, MINORDER);
	mOrder = Min(mOrder, MAXORDER);

	return mOrder;
}

/*
 * inittapes - initialize for tape sorting.
 *
 * This is called only if we have found we don't have room to sort in memory.
 */
static void
inittapes(Tuplesortstate *state, const char* rwfile_prefix)
{
	int			maxTapes,
				ntuples,
				j;
	long		tapeSpace;

	/* Compute number of tapes to use: merge order plus 1 */
	maxTapes = tuplesort_merge_order(state->allowedMem) + 1;

	/*
	 * We must have at least 2*maxTapes slots in the memtuples[] array, else
	 * we'd not have room for merge heap plus preread.  It seems unlikely that
	 * this case would ever occur, but be safe.
	 */
	maxTapes = Min(maxTapes, state->memtupsize / 2);
/* XXX XXX: with losers, only need 1x slots because we don't need a merge heap */

	state->maxTapes = maxTapes;
	state->tapeRange = maxTapes - 1;

	if (trace_sort)
		elog(LOG, "switching to external sort with %d tapes: %s",
			 maxTapes, pg_rusage_show(&state->ru_start));

	/*
	 * Decrease availMem to reflect the space needed for tape buffers; but
	 * don't decrease it to the point that we have no room for tuples. (That
	 * case is only likely to occur if sorting pass-by-value Datums; in all
	 * other scenarios the memtuples[] array is unlikely to occupy more than
	 * half of allowedMem.	In the pass-by-value case it's not important to
	 * account for tuple space, so we don't care if LACKMEM becomes
	 * inaccurate.)
	 */
	tapeSpace = maxTapes * TAPE_BUFFER_OVERHEAD;
	if (tapeSpace + GetMemoryChunkSpace(state->memtuples) < state->allowedMem)
		USEMEM(state, tapeSpace);

	/*
	 * Make sure that the temp file(s) underlying the tape set are created in
	 * suitable temp tablespaces.
	 */
	PrepareTempTablespaces();

	/*
	 * Create the tape set and allocate the per-tape data arrays.
	 */
	if(!rwfile_prefix){
		state->tapeset = LogicalTapeSetCreate(maxTapes, true /* del_on_close */);
	}
	else
	{
		/* We are shared XSLICE, use given prefix to create files so that consumers can find them */
    	ExecWorkFile *tape_file = ExecWorkFile_Create(rwfile_prefix,
    			BUFFILE,
    			true /* delOnClose */,
    			0 /* compressType */);

		state->tapeset = LogicalTapeSetCreate_File(tape_file, maxTapes);
	}

	state->mergeactive = (bool *) palloc0(maxTapes * sizeof(bool));
	state->mergenext = (int *) palloc0(maxTapes * sizeof(int));
	state->mergelast = (int *) palloc0(maxTapes * sizeof(int));
	state->mergeavailslots = (int *) palloc0(maxTapes * sizeof(int));
	state->mergeavailmem = (long *) palloc0(maxTapes * sizeof(long));
	state->tp_fib = (int *) palloc0(maxTapes * sizeof(int));
	state->tp_runs = (int *) palloc0(maxTapes * sizeof(int));
	state->tp_dummy = (int *) palloc0(maxTapes * sizeof(int));
	state->tp_tapenum = (int *) palloc0(maxTapes * sizeof(int));

	/*
	 * Convert the unsorted contents of memtuples[] into a heap. Each tuple is
	 * marked as belonging to run number zero.
	 *
	 * NOTE: we pass false for checkIndex since there's no point in comparing
	 * indexes in this step, even though we do intend the indexes to be part
	 * of the sort key...
	 */
	if (state->standardsort)
	{
		ntuples = state->memtupcount;
		state->memtupcount = 0;		/* make the heap empty */
		for (j = 0; j < ntuples; j++)
		{
			/* Must copy source tuple to avoid possible overwrite */
			SortTuple	stup = state->memtuples[j];

			tuplesort_heap_insert(state, &stup, 0, false);
		}
		Assert(state->memtupcount == ntuples);
	}
	state->currentRun = 0;

	/*
	 * Initialize variables of Algorithm D (step D1).
	 */
	for (j = 0; j < maxTapes; j++)
	{
		state->tp_fib[j] = 1;
		state->tp_runs[j] = 0;
		state->tp_dummy[j] = 1;
		state->tp_tapenum[j] = j;
	}
	state->tp_fib[state->tapeRange] = 0;
	state->tp_dummy[state->tapeRange] = 0;

	state->Level = 1;
	state->destTape = 0;

	state->status = TSS_BUILDRUNS;
}

/*
 * selectnewtape -- select new tape for new initial run.
 *
 * This is called after finishing a run when we know another run
 * must be started.  This implements steps D3, D4 of Algorithm D.
 */
static void
selectnewtape(Tuplesortstate *state)
{
	int			j;
	int			a;

	/* Step D3: advance j (destTape) */
	if (state->tp_dummy[state->destTape] < state->tp_dummy[state->destTape + 1])
	{
		state->destTape++;
		return;
	}
	if (state->tp_dummy[state->destTape] != 0)
	{
		state->destTape = 0;
		return;
	}

	/* Step D4: increase level */
	state->Level++;
	a = state->tp_fib[0];
	for (j = 0; j < state->tapeRange; j++)
	{
		state->tp_dummy[j] = a + state->tp_fib[j + 1] - state->tp_fib[j];
		state->tp_fib[j] = a + state->tp_fib[j + 1];
	}
	state->destTape = 0;
}

/*
 * mergeruns -- merge all the completed initial runs.
 *
 * This implements steps D5, D6 of Algorithm D.  All input data has
 * already been written to initial runs on tape (see dumptuples).
 */
static void
mergeruns(Tuplesortstate *state)
{
	int			tapenum,
				svTape,
				svRuns,
				svDummy;
	LogicalTape *lt = NULL;

	Assert(state->status == TSS_BUILDRUNS);

	/*
	 * If we produced only one initial run (quite likely if the total data
	 * volume is between 1X and 2X workMem), we can just use that tape as the
	 * finished output, rather than doing a useless merge.	(This obvious
	 * optimization is not in Knuth's algorithm.)
	 */
	if (state->currentRun == 1)
	{
		state->result_tape = LogicalTapeSetGetTape(state->tapeset, state->tp_tapenum[state->destTape]);
		/* must freeze and rewind the finished output tape */
		LogicalTapeFreeze(state->tapeset, state->result_tape);
		state->status = TSS_SORTEDONTAPE;
		return;
	}

	/* End of step D2: rewind all output tapes to prepare for merging */
	for (tapenum = 0; tapenum < state->tapeRange; tapenum++)
	{
		lt = LogicalTapeSetGetTape(state->tapeset, tapenum);
		LogicalTapeRewind(state->tapeset, lt, false);
	}

	for (;;)
	{
		/*
		 * At this point we know that tape[T] is empty.  If there's just one
		 * (real or dummy) run left on each input tape, then only one merge
		 * pass remains.  If we don't have to produce a materialized sorted
		 * tape, we can stop at this point and do the final merge on-the-fly.
		 */
		if (!state->randomAccess)
		{
			bool		allOneRun = true;

			Assert(state->tp_runs[state->tapeRange] == 0);
			for (tapenum = 0; tapenum < state->tapeRange; tapenum++)
			{
				if (state->tp_runs[tapenum] + state->tp_dummy[tapenum] != 1)
				{
					allOneRun = false;
					break;
				}
			}
			if (allOneRun)
			{
				/* Tell logtape.c we won't be writing anymore */
				LogicalTapeSetForgetFreeSpace(state->tapeset);
				/* Initialize for the final merge pass */
				beginmerge(state);
				state->status = TSS_FINALMERGE;
				return;
			}
		}

		/* Step D5: merge runs onto tape[T] until tape[P] is empty */
		while (state->tp_runs[state->tapeRange - 1] ||
			   state->tp_dummy[state->tapeRange - 1])
		{
			bool		allDummy = true;

			for (tapenum = 0; tapenum < state->tapeRange; tapenum++)
			{
				if (state->tp_dummy[tapenum] == 0)
				{
					allDummy = false;
					break;
				}
			}

			if (allDummy)
			{
				state->tp_dummy[state->tapeRange]++;
				for (tapenum = 0; tapenum < state->tapeRange; tapenum++)
					state->tp_dummy[tapenum]--;
			}
			else
				mergeonerun(state);
		}

		/* Step D6: decrease level */
		if (--state->Level == 0)
			break;

		/* rewind output tape T to use as new input */
		lt = LogicalTapeSetGetTape(state->tapeset, state->tp_tapenum[state->tapeRange]);
		LogicalTapeRewind(state->tapeset, lt, false);

		/* rewind used-up input tape P, and prepare it for write pass */
		lt = LogicalTapeSetGetTape(state->tapeset, state->tp_tapenum[state->tapeRange - 1]);
		LogicalTapeRewind(state->tapeset, lt, true);

		state->tp_runs[state->tapeRange - 1] = 0;

		/*
		 * reassign tape units per step D6; note we no longer care about A[]
		 */
		svTape = state->tp_tapenum[state->tapeRange];
		svDummy = state->tp_dummy[state->tapeRange];
		svRuns = state->tp_runs[state->tapeRange];
		for (tapenum = state->tapeRange; tapenum > 0; tapenum--)
		{
			state->tp_tapenum[tapenum] = state->tp_tapenum[tapenum - 1];
			state->tp_dummy[tapenum] = state->tp_dummy[tapenum - 1];
			state->tp_runs[tapenum] = state->tp_runs[tapenum - 1];
		}
		state->tp_tapenum[0] = svTape;
		state->tp_dummy[0] = svDummy;
		state->tp_runs[0] = svRuns;
	}

	/*
	 * Done.  Knuth says that the result is on TAPE[1], but since we exited
	 * the loop without performing the last iteration of step D6, we have not
	 * rearranged the tape unit assignment, and therefore the result is on
	 * TAPE[T].  We need to do it this way so that we can freeze the final
	 * output tape while rewinding it.	The last iteration of step D6 would be
	 * a waste of cycles anyway...
	 */
	state->result_tape = LogicalTapeSetGetTape(state->tapeset, state->tp_tapenum[state->tapeRange]);
	LogicalTapeFreeze(state->tapeset, state->result_tape);
	state->status = TSS_SORTEDONTAPE;
}

/*
 * Merge one run from each input tape, except ones with dummy runs.
 *
 * This is the inner loop of Algorithm D step D5.  We know that the
 * output tape is TAPE[T].
 */
static void
mergeonerun(Tuplesortstate *state)
{
	int			destTape = state->tp_tapenum[state->tapeRange];
	int			srcTape;
	int			tupIndex;
	SortTuple  *tup;
	long		priorAvail,
				spaceFreed;

	LogicalTape *lt = NULL;

	/*
	 * Start the merge by loading one tuple from each active source tape into
	 * the heap.  We can also decrease the input run/dummy run counts.
	 */
	beginmerge(state);

	/*
	 * Execute merge by repeatedly extracting lowest tuple in heap, writing it
	 * out, and replacing it with next tuple from same tape (if there is
	 * another one).
	 */
	lt = LogicalTapeSetGetTape(state->tapeset, destTape);

	while (state->memtupcount > 0)
	{
		/* write the tuple to destTape */
		priorAvail = state->availMem;
		srcTape = state->memtuples[0].tupindex;
		WRITETUP(state, lt, &state->memtuples[0]);

		/* writetup adjusted total free space, now fix per-tape space */
		spaceFreed = state->availMem - priorAvail;
		state->mergeavailmem[srcTape] += spaceFreed;

		/* compact the heap */
		tuplesort_heap_siftup(state, false);
		if ((tupIndex = state->mergenext[srcTape]) == 0)
		{
			/* out of preloaded data on this tape, try to read more */
			mergepreread(state);
			/* if still no data, we've reached end of run on this tape */
			if ((tupIndex = state->mergenext[srcTape]) == 0)
				continue;
		}
		/* pull next preread tuple from list, insert in heap */
		tup = &state->memtuples[tupIndex];
		state->mergenext[srcTape] = tup->tupindex;
		if (state->mergenext[srcTape] == 0)
			state->mergelast[srcTape] = 0;
		tuplesort_heap_insert(state, tup, srcTape, false);
		/* put the now-unused memtuples entry on the freelist */
		tup->tupindex = state->mergefreelist;
		state->mergefreelist = tupIndex;
		state->mergeavailslots[srcTape]++;
	}

	/*
	 * When the heap empties, we're done.  Write an end-of-run marker on the
	 * output tape, and increment its count of real runs.
	 */
	markrunend(state, destTape);
	state->tp_runs[state->tapeRange]++;

	if (trace_sort)
		elog(LOG, "finished %d-way merge step: %s", state->activeTapes,
			 pg_rusage_show(&state->ru_start));
}

/*
 * beginmerge - initialize for a merge pass
 *
 * We decrease the counts of real and dummy runs for each tape, and mark
 * which tapes contain active input runs in mergeactive[].	Then, load
 * as many tuples as we can from each active input tape, and finally
 * fill the merge heap with the first tuple from each active tape.
 */
static void
beginmerge(Tuplesortstate *state)
{
	int			activeTapes;
	int			tapenum;
	int			srcTape;
	int			slotsPerTape;
	long		spacePerTape;

	/* Heap should be empty here */
	Assert(state->memtupcount == 0);

	/* Adjust run counts and mark the active tapes */
	memset(state->mergeactive, 0,
		   state->maxTapes * sizeof(*state->mergeactive));
	activeTapes = 0;
	for (tapenum = 0; tapenum < state->tapeRange; tapenum++)
	{
		if (state->tp_dummy[tapenum] > 0)
			state->tp_dummy[tapenum]--;
		else
		{
			Assert(state->tp_runs[tapenum] > 0);
			state->tp_runs[tapenum]--;
			srcTape = state->tp_tapenum[tapenum];
			state->mergeactive[srcTape] = true;
			activeTapes++;
		}
	}
	state->activeTapes = activeTapes;

	/* Clear merge-pass state variables */
	memset(state->mergenext, 0,
		   state->maxTapes * sizeof(*state->mergenext));
	memset(state->mergelast, 0,
		   state->maxTapes * sizeof(*state->mergelast));
	state->mergefreelist = 0;	/* nothing in the freelist */
	state->mergefirstfree = activeTapes;		/* 1st slot avail for preread */

	/*
	 * Initialize space allocation to let each active input tape have an equal
	 * share of preread space.
	 */
	Assert(activeTapes > 0);
	slotsPerTape = (state->memtupsize - state->mergefirstfree) / activeTapes;
	Assert(slotsPerTape > 0);
	spacePerTape = state->availMem / activeTapes;
	for (srcTape = 0; srcTape < state->maxTapes; srcTape++)
	{
		if (state->mergeactive[srcTape])
		{
			state->mergeavailslots[srcTape] = slotsPerTape;
			state->mergeavailmem[srcTape] = spacePerTape;
		}
	}

	/*
	 * Preread as many tuples as possible (and at least one) from each active
	 * tape
	 */
	mergepreread(state);

	/* Load the merge heap with the first tuple from each input tape */
	for (srcTape = 0; srcTape < state->maxTapes; srcTape++)
	{
		int			tupIndex = state->mergenext[srcTape];
		SortTuple  *tup;

		if (tupIndex)
		{
			tup = &state->memtuples[tupIndex];
			state->mergenext[srcTape] = tup->tupindex;
			if (state->mergenext[srcTape] == 0)
				state->mergelast[srcTape] = 0;
			tuplesort_heap_insert(state, tup, srcTape, false);
			/* put the now-unused memtuples entry on the freelist */
			tup->tupindex = state->mergefreelist;
			state->mergefreelist = tupIndex;
			state->mergeavailslots[srcTape]++;
		}
	}
}

/*
 * mergepreread - load tuples from merge input tapes
 *
 * This routine exists to improve sequentiality of reads during a merge pass,
 * as explained in the header comments of this file.  Load tuples from each
 * active source tape until the tape's run is exhausted or it has used up
 * its fair share of available memory.	In any case, we guarantee that there
 * is at least one preread tuple available from each unexhausted input tape.
 *
 * We invoke this routine at the start of a merge pass for initial load,
 * and then whenever any tape's preread data runs out.  Note that we load
 * as much data as possible from all tapes, not just the one that ran out.
 * This is because logtape.c works best with a usage pattern that alternates
 * between reading a lot of data and writing a lot of data, so whenever we
 * are forced to read, we should fill working memory completely.
 *
 * In FINALMERGE state, we *don't* use this routine, but instead just preread
 * from the single tape that ran dry.  There's no read/write alternation in
 * that state and so no point in scanning through all the tapes to fix one.
 * (Moreover, there may be quite a lot of inactive tapes in that state, since
 * we might have had many fewer runs than tapes.  In a regular tape-to-tape
 * merge we can expect most of the tapes to be active.)
 */
static void
mergepreread(Tuplesortstate *state)
{
	int			srcTape;

	for (srcTape = 0; srcTape < state->maxTapes; srcTape++)
		mergeprereadone(state, srcTape);
}

/*
 * mergeprereadone - load tuples from one merge input tape
 *
 * Read tuples from the specified tape until it has used up its free memory
 * or array slots; but ensure that we have at least one tuple, if any are
 * to be had.
 */
static void
mergeprereadone(Tuplesortstate *state, int srcTape)
{
	unsigned int tuplen;
	SortTuple	stup;
	int			tupIndex;
	long		priorAvail,
				spaceUsed;

	LogicalTape *srclt = NULL;

	if (!state->mergeactive[srcTape])
		return;					/* tape's run is already exhausted */

	priorAvail = state->availMem;
	state->availMem = state->mergeavailmem[srcTape];

	srclt = LogicalTapeSetGetTape(state->tapeset, srcTape);

	while ((state->mergeavailslots[srcTape] > 0 && !LACKMEM(state)) ||
		   state->mergenext[srcTape] == 0)
	{
		/* read next tuple, if any */
		Assert(state->pos.cur_work_tape == NULL);


		if ((tuplen = getlen(state, &state->pos, srclt, true)) == 0)
		{
			state->mergeactive[srcTape] = false;
			break;
		}
		READTUP(state, &state->pos, &stup, srclt, tuplen);

		/* find a free slot in memtuples[] for it */
		tupIndex = state->mergefreelist;
		if (tupIndex)
			state->mergefreelist = state->memtuples[tupIndex].tupindex;
		else
		{
			tupIndex = state->mergefirstfree++;
			Assert(tupIndex < state->memtupsize);
		}
		state->mergeavailslots[srcTape]--;
		/* store tuple, append to list for its tape */
		stup.tupindex = 0;
		state->memtuples[tupIndex] = stup;
		if (state->mergelast[srcTape])
			state->memtuples[state->mergelast[srcTape]].tupindex = tupIndex;
		else
			state->mergenext[srcTape] = tupIndex;
		state->mergelast[srcTape] = tupIndex;
	}
	/* update per-tape and global availmem counts */
	spaceUsed = state->mergeavailmem[srcTape] - state->availMem;
	state->mergeavailmem[srcTape] = state->availMem;
	state->availMem = priorAvail - spaceUsed;
}

/*
 * dumptuples - remove tuples from heap and write to tape
 *
 * This is used during initial-run building, but not during merging.
 *
 * When alltuples = false, dump only enough tuples to get under the
 * availMem limit (and leave at least one tuple in the heap in any case,
 * since puttuple assumes it always has a tuple to compare to).  We also
 * insist there be at least one free slot in the memtuples[] array.
 *
 * When alltuples = true, dump everything currently in memory.
 * (This case is only used at end of input data.)
 *
 * If we empty the heap, close out the current run and return (this should
 * only happen at end of input data).  If we see that the tuple run number
 * at the top of the heap has changed, start a new run.
 */
static void
dumptuples(Tuplesortstate *state, bool alltuples)
{
	int     bDumped = 0;
	long    spilledBytes = state->availMem;
	LogicalTape *lt = NULL;

	while (alltuples ||
		   (LACKMEM(state) && state->memtupcount > 1) ||
		   state->memtupcount >= state->memtupsize)
	{
		/* ShareInput or sort: The sort may sort nothing, we still
		 * need to handle it here 
		 */
		if (state->memtupcount == 0)
		{
			markrunend(state, state->tp_tapenum[state->destTape]);
			state->currentRun++;
			state->tp_runs[state->destTape]++;
			state->tp_dummy[state->destTape]--; /* per Alg D step D2 */

			break;
		}

		if (!bDumped) 
			bDumped = 1;
		/*
		 * Dump the heap's frontmost entry, and sift up to remove it from the
		 * heap.
		 */
		Assert(state->memtupcount > 0);

		lt = LogicalTapeSetGetTape(state->tapeset, state->tp_tapenum[state->destTape]);
		WRITETUP(state, lt, &state->memtuples[0]);
		tuplesort_heap_siftup(state, true);

		/*
		 * If the heap is empty *or* top run number has changed, we've
		 * finished the current run.
		 */
		if (state->memtupcount == 0 ||
			state->currentRun != state->memtuples[0].tupindex)
		{
			markrunend(state, state->tp_tapenum[state->destTape]);
			state->currentRun++;
			state->tp_runs[state->destTape]++;
			state->tp_dummy[state->destTape]--; /* per Alg D step D2 */

			if (trace_sort)
				elog(LOG, "finished writing%s run %d to tape %d: %s",
					 (state->memtupcount == 0) ? " final" : "",
					 state->currentRun, state->destTape,
					 pg_rusage_show(&state->ru_start));

			/*
			 * Done if heap is empty, else prepare for new run.
			 */
			if (state->memtupcount == 0)
				break;
			Assert(state->currentRun == state->memtuples[0].tupindex);
			selectnewtape(state);
		}
	}

	/* CDB: Accumulate total size of spilled tuples. */
	spilledBytes = state->availMem - spilledBytes;
	if (spilledBytes > 0)
		state->spilledBytes += spilledBytes;

	if(state->gpmon_pkt)
		tuplesort_checksend_gpmonpkt(state->gpmon_pkt, state->gpmon_sort_tick);
}

/*
 * tuplesort_rescan		- rewind and replay the scan
 */
void
tuplesort_rescan(Tuplesortstate *state)
{
	tuplesort_rescan_pos(state, &state->pos);
}

/*
 * Put pos at the begining of the tuplesort.  Create pos->work_tape if necessary
 */
void
tuplesort_rescan_pos(Tuplesortstate *state, TuplesortPos *pos)
{
	MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);

	Assert(state->randomAccess);

	switch (state->status)
	{
		case TSS_SORTEDINMEM:
			pos->current = 0;
			pos->eof_reached = false;
			pos->markpos.mempos = 0; 
			pos->markpos_eof = false;
			pos->cur_work_tape = NULL;
			break;
		case TSS_SORTEDONTAPE:
			if(pos == &state->pos)
			{
				Assert(pos->cur_work_tape == NULL);
				LogicalTapeRewind(state->tapeset, state->result_tape, false);
			}
			else
			{
				if(pos->cur_work_tape == NULL)
					pos->cur_work_tape = state->result_tape;

				LogicalTapeRewind(state->tapeset, pos->cur_work_tape, false);
			}

			pos->eof_reached = false;
			pos->markpos.tapepos.blkNum = 0L; 
			pos->markpos.tapepos.offset = 0;
			pos->markpos_eof = false;
			break;
		default:
			elog(ERROR, "invalid tuplesort state");
			break;
	}

	MemoryContextSwitchTo(oldcontext);
}

/*
 * tuplesort_markpos	- saves current position in the merged sort file
 */
void
tuplesort_markpos(Tuplesortstate *state)
{
	tuplesort_markpos_pos(state, &state->pos);
}

void
tuplesort_markpos_pos(Tuplesortstate *state, TuplesortPos *pos)
{
	LogicalTape *work_tape = NULL;
	MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);

	Assert(state->randomAccess);

	switch (state->status)
	{
		case TSS_SORTEDINMEM:
			pos->markpos.mempos = pos->current;
			pos->markpos_eof = pos->eof_reached;
			break;
		case TSS_SORTEDONTAPE:
			AssertEquivalent(pos == &state->pos, pos->cur_work_tape == NULL);	
			work_tape = pos->cur_work_tape == NULL ? state->result_tape : pos->cur_work_tape;
			LogicalTapeTell(state->tapeset, work_tape, &pos->markpos.tapepos);
			pos->markpos_eof = pos->eof_reached;
			break;
		default:
			elog(ERROR, "invalid tuplesort state");
			break;
	}

	MemoryContextSwitchTo(oldcontext);
}

/*
 * tuplesort_restorepos - restores current position in merged sort file to
 *						  last saved position
 */
void
tuplesort_restorepos(Tuplesortstate *state)
{
	tuplesort_restorepos_pos(state, &state->pos);
}

void
tuplesort_restorepos_pos(Tuplesortstate *state, TuplesortPos *pos)
{
	MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);

	Assert(state->randomAccess);

	switch (state->status)
	{
		case TSS_SORTEDINMEM:
			pos->current = pos->markpos.mempos; 
			pos->eof_reached = pos->markpos_eof;
			break;
		case TSS_SORTEDONTAPE:
			AssertEquivalent(pos == &state->pos, pos->cur_work_tape == NULL);	
			{
				LogicalTape *work_tape = pos->cur_work_tape == NULL ? state->result_tape : pos->cur_work_tape;
				bool fSeekOK = LogicalTapeSeek(state->tapeset, work_tape, &pos->markpos.tapepos);
				
				if(!fSeekOK)
					elog(ERROR, "tuplesort_restorepos failed");
				pos->eof_reached = pos->markpos_eof;
			}
			break;
		default:
			elog(ERROR, "invalid tuplesort state");
			break;
	}

	MemoryContextSwitchTo(oldcontext);
}

/*
 * tuplesort_get_stats - extract summary statistics
 *
 * This can be called after tuplesort_performsort() finishes to obtain
 * printable summary information about how the sort was performed.
 * spaceUsed is measured in kilobytes.
 */
static void
tuplesort_get_stats(Tuplesortstate *state,
					const char **sortMethod,
					const char **spaceType,
					long *spaceUsed)
{
	/*
	 * Note: it might seem we should provide both memory and disk usage for a
	 * disk-based sort.  However, the current code doesn't track memory space
	 * accurately once we have begun to return tuples to the caller (since we
	 * don't account for pfree's the caller is expected to do), so we cannot
	 * rely on availMem in a disk sort.  This does not seem worth the overhead
	 * to fix.	Is it worth creating an API for the memory context code to
	 * tell us how much is actually used in sortcontext?
	 */
	if (state->tapeset)
	{
		*spaceType = "Disk";
		*spaceUsed = LogicalTapeSetBlocks(state->tapeset) * (BLCKSZ / 1024);
	}
	else
	{
		*spaceType = "Memory";
		*spaceUsed = (state->allowedMem - state->availMem + 1023) / 1024;
	}

	switch (state->status)
	{
		case TSS_SORTEDINMEM:
			if (state->boundUsed)
				*sortMethod = "top-N heapsort";
			else
				*sortMethod = "quicksort";
			break;
		case TSS_SORTEDONTAPE:
			*sortMethod = "external sort";
			break;
		case TSS_FINALMERGE:
			*sortMethod = "external merge";
			break;
		default:
			*sortMethod = "still in progress";
			break;
	}
}


/*
 * Heap manipulation routines, per Knuth's Algorithm 5.2.3H.
 *
 * Compare two SortTuples.	If checkIndex is true, use the tuple index
 * as the front of the sort key; otherwise, no.
 */

#define HEAPCOMPARE(tup1,tup2) \
	(checkIndex && ((tup1)->tupindex != (tup2)->tupindex) ? \
	 ((tup1)->tupindex) - ((tup2)->tupindex) : \
	 COMPARETUP(state, tup1, tup2))

/*
 * Convert the existing unordered array of SortTuples to a bounded heap,
 * discarding all but the smallest "state->bound" tuples.
 *
 * When working with a bounded heap, we want to keep the largest entry
 * at the root (array entry zero), instead of the smallest as in the normal
 * sort case.  This allows us to discard the largest entry cheaply.
 * Therefore, we temporarily reverse the sort direction.
 *
 * We assume that all entries in a bounded heap will always have tupindex
 * zero; it therefore doesn't matter that HEAPCOMPARE() doesn't reverse
 * the direction of comparison for tupindexes.
 */
static void
make_bounded_heap(Tuplesortstate *state)
{
	int			tupcount = state->memtupcount;
	int			i;

	Assert(state->status == TSS_INITIAL);
	Assert(state->bounded);
	Assert(tupcount >= state->bound);

	/* Reverse sort direction so largest entry will be at root */
	REVERSEDIRECTION(state);

	state->memtupcount = 0;		/* make the heap empty */
	for (i = 0; i < tupcount; i++)
	{
		if (state->memtupcount >= state->bound &&
		  COMPARETUP(state, &state->memtuples[i], &state->memtuples[0]) <= 0)
		{
			/* New tuple would just get thrown out, so skip it */
			free_sort_tuple(state, &state->memtuples[i]);
		}
		else
		{
			/* Insert next tuple into heap */
			/* Must copy source tuple to avoid possible overwrite */
			SortTuple	stup = state->memtuples[i];

			tuplesort_heap_insert(state, &stup, 0, false);

			/* If heap too full, discard largest entry */
			if (state->memtupcount > state->bound)
			{
				free_sort_tuple(state, &state->memtuples[0]);
				tuplesort_heap_siftup(state, false);
			}
		}
	}

	Assert(state->memtupcount == state->bound);
	state->status = TSS_BOUNDED;
}

/*
 * Convert the bounded heap to a properly-sorted array
 */
static void
sort_bounded_heap(Tuplesortstate *state)
{
	int			tupcount = state->memtupcount;

	Assert(state->status == TSS_BOUNDED);
	Assert(state->bounded);
	Assert(tupcount == state->bound);

	/*
	 * We can unheapify in place because each sift-up will remove the largest
	 * entry, which we can promptly store in the newly freed slot at the end.
	 * Once we're down to a single-entry heap, we're done.
	 */
	while (state->memtupcount > 1)
	{
		SortTuple	stup = state->memtuples[0];

		/* this sifts-up the next-largest entry and decreases memtupcount */
		tuplesort_heap_siftup(state, false);
		state->memtuples[state->memtupcount] = stup;
	}
	state->memtupcount = tupcount;

	/*
	 * Reverse sort direction back to the original state.  This is not
	 * actually necessary but seems like a good idea for tidiness.
	 */
	REVERSEDIRECTION(state);

	state->status = TSS_SORTEDINMEM;
	state->boundUsed = true;
}

/*
 * Insert a new tuple into an empty or existing heap, maintaining the
 * heap invariant.	Caller is responsible for ensuring there's room.
 *
 * Note: we assume *tuple is a temporary variable that can be scribbled on.
 * For some callers, tuple actually points to a memtuples[] entry above the
 * end of the heap.  This is safe as long as it's not immediately adjacent
 * to the end of the heap (ie, in the [memtupcount] array entry) --- if it
 * is, it might get overwritten before being moved into the heap!
 */
static void
tuplesort_heap_insert(Tuplesortstate *state, SortTuple *tuple,
					  int tupleindex, bool checkIndex)
{
	SortTuple  *memtuples;
	int			j;
	int			comparestat;

	/*
	 * Save the tupleindex --- see notes above about writing on *tuple. It's a
	 * historical artifact that tupleindex is passed as a separate argument
	 * and not in *tuple, but it's notationally convenient so let's leave it
	 * that way.
	 */
	tuple->tupindex = tupleindex;

	memtuples = state->memtuples;
	Assert(state->memtupcount < state->memtupsize);

	/*
	 * Sift-up the new entry, per Knuth 5.2.3 exercise 16. Note that Knuth is
	 * using 1-based array indexes, not 0-based.
	 */
	j = state->memtupcount++;
	while (j > 0)
	{
		int			i = (j - 1) >> 1;

		comparestat = HEAPCOMPARE(tuple, &memtuples[i]);
		if (comparestat >= 0)
			break;
		memtuples[j] = memtuples[i];
		j = i;
	}

	memtuples[j] = *tuple;
}

/*
 * The tuple at state->memtuples[0] has been removed from the heap.
 * Decrement memtupcount, and sift up to maintain the heap invariant.
 */
static void
tuplesort_heap_siftup(Tuplesortstate *state, bool checkIndex)
{
	SortTuple  *memtuples = state->memtuples;
	SortTuple  *tuple;
	int			i,
				n;

	if (--state->memtupcount <= 0)
		return;
	n = state->memtupcount;
	tuple = &memtuples[n];		/* tuple that must be reinserted */
	i = 0;						/* i is where the "hole" is */
	for (;;)
	{
		int			j = 2 * i + 1;

		if (j >= n)
			break;
		if (j + 1 < n &&
			HEAPCOMPARE(&memtuples[j], &memtuples[j + 1]) > 0)
			j++;
		if (HEAPCOMPARE(tuple, &memtuples[j]) <= 0)
			break;
		memtuples[i] = memtuples[j];
		i = j;
	}
	memtuples[i] = *tuple;
}


/*
 * Tape interface routines
 */

static unsigned int
getlen(Tuplesortstate *state, TuplesortPos *pos, LogicalTape *lt, bool eofOK)
{
	unsigned int len;
	size_t readSize;

	Assert(lt);
	readSize = LogicalTapeRead(state->tapeset, lt, (void *)&len, sizeof(len));

	if (readSize != sizeof(len))
	{
		elog(ERROR, "unexpected end of tape");
	}

	if (len == 0 && !eofOK)
		elog(ERROR, "unexpected end of data");
	return len;
}

static void
markrunend(Tuplesortstate *state, int tapenum)
{
	unsigned int len = 0;
	LogicalTape *lt = LogicalTapeSetGetTape(state->tapeset, tapenum);

	LogicalTapeWrite(state->tapeset, lt, (void *) &len, sizeof(len));
}


/*
 * Set up for an external caller of ApplySortFunction.	This function
 * basically just exists to localize knowledge of the encoding of sk_flags
 * used in this module.
 */
void
SelectSortFunction(Oid sortOperator,
				   bool nulls_first,
				   Oid *sortFunction,
				   int *sortFlags)
{
	bool		reverse;

	if (!get_compare_function_for_ordering_op(sortOperator,
											  sortFunction, &reverse))
		elog(ERROR, "operator %u is not a valid ordering operator",
			 sortOperator);

	*sortFlags = reverse ? SK_BT_DESC : 0;
	if (nulls_first)
		*sortFlags |= SK_BT_NULLS_FIRST;
}

/*
 * Inline-able copy of FunctionCall2() to save some cycles in sorting.
 */
static inline Datum
myFunctionCall2(FmgrInfo *flinfo, Datum arg1, Datum arg2)
{
	FunctionCallInfoData fcinfo;
	Datum		result;

	InitFunctionCallInfoData(fcinfo, flinfo, 2, NULL, NULL);

	fcinfo.arg[0] = arg1;
	fcinfo.arg[1] = arg2;
	fcinfo.argnull[0] = false;
	fcinfo.argnull[1] = false;

	result = FunctionCallInvoke(&fcinfo);

	/* Check for null result, since caller is clearly not expecting one */
	if (fcinfo.isnull)
		elog(ERROR, "function %u returned NULL", fcinfo.flinfo->fn_oid);

	return result;
}

/*
 * Apply a sort function (by now converted to fmgr lookup form)
 * and return a 3-way comparison result.  This takes care of handling
 * reverse-sort and NULLs-ordering properly.  We assume that DESC and
 * NULLS_FIRST options are encoded in sk_flags the same way btree does it.
 */
static inline int32
inlineApplySortFunction(FmgrInfo *sortFunction, int sk_flags,
						Datum datum1, bool isNull1,
						Datum datum2, bool isNull2)
{
	int32		compare;

	if (isNull1)
	{
		if (isNull2)
			compare = 0;		/* NULL "=" NULL */
		else if (sk_flags & SK_BT_NULLS_FIRST)
			compare = -1;		/* NULL "<" NOT_NULL */
		else
			compare = 1;		/* NULL ">" NOT_NULL */
	}
	else if (isNull2)
	{
		if (sk_flags & SK_BT_NULLS_FIRST)
			compare = 1;		/* NOT_NULL ">" NULL */
		else
			compare = -1;		/* NOT_NULL "<" NULL */
	}
	else
	{
		compare = DatumGetInt32(myFunctionCall2(sortFunction,
												datum1, datum2));

		if (sk_flags & SK_BT_DESC)
			compare = -compare;
	}

	return compare;
}

/*
 * Non-inline ApplySortFunction() --- this is needed only to conform to
 * C99's brain-dead notions about how to implement inline functions...
 */
int32
ApplySortFunction(FmgrInfo *sortFunction, int sortFlags,
				  Datum datum1, bool isNull1,
				  Datum datum2, bool isNull2)
{
	return inlineApplySortFunction(sortFunction, sortFlags,
								   datum1, isNull1,
								   datum2, isNull2);
}


/*
 * Routines specialized for HeapTuple (actually MinimalTuple) case
 */

static int
comparetup_heap(const SortTuple *a, const SortTuple *b, Tuplesortstate *state)
{
	ScanKey		scanKey = state->scanKeys;
	int			nkey;
	int32		compare;

	/* Allow interrupting long sorts */
	CHECK_FOR_INTERRUPTS();

	Assert(state->mt_bind);
	/* Compare the leading sort key */
	compare = inlineApplySortFunction(&scanKey->sk_func, scanKey->sk_flags,
									  a->datum1, a->isnull1,
									  b->datum1, b->isnull1);
	if (compare != 0)
		return compare;

	scanKey++;
	for (nkey = 1; nkey < state->nKeys; nkey++, scanKey++)
	{
		AttrNumber	attno = scanKey->sk_attno;
		Datum		datum1,
					datum2;
		bool		isnull1,
					isnull2;

		datum1 = memtuple_getattr(a->tuple, state->mt_bind, attno, &isnull1); 
		datum2 = memtuple_getattr(b->tuple, state->mt_bind, attno, &isnull2);

		compare = inlineApplySortFunction(&scanKey->sk_func, scanKey->sk_flags,
										  datum1, isnull1,
										  datum2, isnull2);

		if (compare != 0)
			return compare;
	}

	/* CDB JC XXX XXX - need to merge aggregates or discard duplicates here */

	return 0;
}

static void
copytup_heap(Tuplesortstate *state, SortTuple *stup, void *tup)
{
	/*
	 * We expect the passed "tup" to be a TupleTableSlot, and form a
	 * MinimalTuple using the exported interface for that.
	 */
	TupleTableSlot *slot = (TupleTableSlot *) tup;

	slot_getallattrs(slot);
	stup->tuple = memtuple_form_to(state->mt_bind, 
			slot_get_values(slot),
			slot_get_isnull(slot),
			NULL, NULL, false
			);
	USEMEM(state, GetMemoryChunkSpace(stup->tuple));

	Assert(state->mt_bind);
	stup->datum1 = memtuple_getattr(stup->tuple, state->mt_bind, state->scanKeys[0].sk_attno, &stup->isnull1);
}

static void
writetup_heap(Tuplesortstate *state, LogicalTape *lt, SortTuple *stup)
{
	uint32 tuplen = memtuple_get_size(stup->tuple);

	LogicalTapeWrite(state->tapeset, lt, (void *) stup->tuple, tuplen);

	if (state->randomAccess)	/* need trailing length word? */
		LogicalTapeWrite(state->tapeset, lt, (void *) &tuplen, sizeof(tuplen));

	FREEMEM(state, GetMemoryChunkSpace(stup->tuple));
	pfree(stup->tuple);
}

static void
readtup_heap(Tuplesortstate *state, TuplesortPos *pos, SortTuple *stup,
			 LogicalTape *lt, uint32 len)
{
	uint32 tuplen;
	size_t readSize;

	stup->tuple = (MemTuple) palloc(memtuple_size_from_uint32(len));
	USEMEM(state, GetMemoryChunkSpace(stup->tuple));
	memtuple_set_mtlen(stup->tuple, len);

	Assert(lt);
	readSize = LogicalTapeRead(state->tapeset, lt,
				(void *) ((char *)stup->tuple + sizeof(uint32)),
				memtuple_size_from_uint32(len) - sizeof(uint32));

	if (readSize != (size_t) (memtuple_size_from_uint32(len) - sizeof(uint32))) 
		elog(ERROR, "unexpected end of data");

	if (state->randomAccess)	/* need trailing length word? */
	{
		readSize = LogicalTapeRead(state->tapeset, lt, (void *)&tuplen, sizeof(tuplen));

		if(readSize != sizeof(tuplen))
			elog(ERROR, "unexpected end of data");
	}

	/* For shareinput on sort, the reader will not set mt_bind.  In this case,
	 * we will not call compare.
	 */
	AssertImply(!state->mt_bind, state->status == TSS_SORTEDONTAPE);
	if(state->mt_bind)
		stup->datum1 = memtuple_getattr(stup->tuple, state->mt_bind, state->scanKeys[0].sk_attno, &stup->isnull1);
}

static void
reversedirection_heap(Tuplesortstate *state)
{
	ScanKey		scanKey = state->scanKeys;
	int			nkey;

	for (nkey = 0; nkey < state->nKeys; nkey++, scanKey++)
	{
		scanKey->sk_flags ^= (SK_BT_DESC | SK_BT_NULLS_FIRST);
	}
}


/*
 * Routines specialized for IndexTuple case
 *
 * The btree and hash cases require separate comparison functions, but the
 * IndexTuple representation is the same so the copy/write/read support
 * functions can be shared.
 */

static int
comparetup_index_btree(const SortTuple *a, const SortTuple *b,
					   Tuplesortstate *state)
{
	/*
	 * This is similar to _bt_tuplecompare(), but we have already done the
	 * index_getattr calls for the first column, and we need to keep track of
	 * whether any null fields are present.  Also see the special treatment
	 * for equal keys at the end.
	 */
	ScanKey		scanKey = state->indexScanKey;
	IndexTuple	tuple1;
	IndexTuple	tuple2;
	int			keysz;
	TupleDesc	tupDes;
	bool		equal_hasnull = false;
	int			nkey;
	int32		compare;

	/* Allow interrupting long sorts */
	CHECK_FOR_INTERRUPTS();

	/* Compare the leading sort key */
	compare = inlineApplySortFunction(&scanKey->sk_func, scanKey->sk_flags,
									  a->datum1, a->isnull1,
									  b->datum1, b->isnull1);
	if (compare != 0)
		return compare;

	/* they are equal, so we only need to examine one null flag */
	if (a->isnull1)
		equal_hasnull = true;

	/* Compare additional sort keys */
	tuple1 = (IndexTuple) a->tuple;
	tuple2 = (IndexTuple) b->tuple;
	keysz = state->nKeys;
	tupDes = RelationGetDescr(state->indexRel);
	scanKey++;
	for (nkey = 2; nkey <= keysz; nkey++, scanKey++)
	{
		Datum		datum1,
					datum2;
		bool		isnull1,
					isnull2;

		datum1 = index_getattr(tuple1, nkey, tupDes, &isnull1);
		datum2 = index_getattr(tuple2, nkey, tupDes, &isnull2);

		compare = inlineApplySortFunction(&scanKey->sk_func, scanKey->sk_flags,
										  datum1, isnull1,
										  datum2, isnull2);
		if (compare != 0)
			return compare;		/* done when we find unequal attributes */

		/* they are equal, so we only need to examine one null flag */
		if (isnull1)
			equal_hasnull = true;
	}

	/*
	 * If btree has asked us to enforce uniqueness, complain if two equal
	 * tuples are detected (unless there was at least one NULL field).
	 *
	 * It is sufficient to make the test here, because if two tuples are equal
	 * they *must* get compared at some stage of the sort --- otherwise the
	 * sort algorithm wouldn't have checked whether one must appear before the
	 * other.
	 *
	 * Some rather brain-dead implementations of qsort will sometimes call the
	 * comparison routine to compare a value to itself.  (At this writing only
	 * QNX 4 is known to do such silly things; we don't support QNX anymore,
	 * but perhaps the behavior still exists elsewhere.)  Don't raise a bogus
	 * error in that case.
	 */
	if (state->enforceUnique && !equal_hasnull && tuple1 != tuple2)
	{
		Datum	values[INDEX_MAX_KEYS];
		bool	isnull[INDEX_MAX_KEYS];

		index_deform_tuple(tuple1, tupDes, values, isnull);
		ereport(ERROR,
				(errcode(ERRCODE_UNIQUE_VIOLATION),
				 errmsg("could not create unique index \"%s\"",
						RelationGetRelationName(state->indexRel)),
				 errdetail("Key %s is duplicated.",
						   BuildIndexValueDescription(state->indexRel,
													  values, isnull))));
	}

	/*
	 * If key values are equal, we sort on ItemPointer.  This does not affect
	 * validity of the finished index, but it offers cheap insurance against
	 * performance problems with bad qsort implementations that have trouble
	 * with large numbers of equal keys.
	 */
	{
		BlockNumber blk1 = ItemPointerGetBlockNumber(&tuple1->t_tid);
		BlockNumber blk2 = ItemPointerGetBlockNumber(&tuple2->t_tid);

		if (blk1 != blk2)
			return (blk1 < blk2) ? -1 : 1;
	}
	{
		OffsetNumber pos1 = ItemPointerGetOffsetNumber(&tuple1->t_tid);
		OffsetNumber pos2 = ItemPointerGetOffsetNumber(&tuple2->t_tid);

		if (pos1 != pos2)
			return (pos1 < pos2) ? -1 : 1;
	}

	return 0;
}

static int
comparetup_index_hash(const SortTuple *a, const SortTuple *b,
					  Tuplesortstate *state)
{
	uint32		hash1;
	uint32		hash2;
	IndexTuple	tuple1;
	IndexTuple	tuple2;

	/* Allow interrupting long sorts */
	CHECK_FOR_INTERRUPTS();

	/*
	 * Fetch hash keys and mask off bits we don't want to sort by.
	 * We know that the first column of the index tuple is the hash key.
	 */
	Assert(!a->isnull1);
	hash1 = DatumGetUInt32(a->datum1) & state->hash_mask;
	Assert(!b->isnull1);
	hash2 = DatumGetUInt32(b->datum1) & state->hash_mask;

	if (hash1 > hash2)
		return 1;
	else if (hash1 < hash2)
		return -1;

	/*
	 * If hash values are equal, we sort on ItemPointer.  This does not affect
	 * validity of the finished index, but it offers cheap insurance against
	 * performance problems with bad qsort implementations that have trouble
	 * with large numbers of equal keys.
	 */
	tuple1 = (IndexTuple) a->tuple;
	tuple2 = (IndexTuple) b->tuple;

	{
		BlockNumber blk1 = ItemPointerGetBlockNumber(&tuple1->t_tid);
		BlockNumber blk2 = ItemPointerGetBlockNumber(&tuple2->t_tid);

		if (blk1 != blk2)
			return (blk1 < blk2) ? -1 : 1;
	}
	{
		OffsetNumber pos1 = ItemPointerGetOffsetNumber(&tuple1->t_tid);
		OffsetNumber pos2 = ItemPointerGetOffsetNumber(&tuple2->t_tid);

		if (pos1 != pos2)
			return (pos1 < pos2) ? -1 : 1;
	}

	return 0;
}

static void
copytup_index(Tuplesortstate *state, SortTuple *stup, void *tup)
{
	IndexTuple	tuple = (IndexTuple) tup;
	unsigned int tuplen = IndexTupleSize(tuple);
	IndexTuple	newtuple;

	/* copy the tuple into sort storage */
	newtuple = (IndexTuple) palloc(tuplen);
	memcpy(newtuple, tuple, tuplen);
	USEMEM(state, GetMemoryChunkSpace(newtuple));
	stup->tuple = (void *) newtuple;
	/* set up first-column key value */
	stup->datum1 = index_getattr(newtuple,
								 1,
								 RelationGetDescr(state->indexRel),
								 &stup->isnull1);
}

static void
writetup_index(Tuplesortstate *state, LogicalTape *lt, SortTuple *stup)
{
	IndexTuple	tuple = (IndexTuple) stup->tuple;
	unsigned int tuplen;

	tuplen = IndexTupleSize(tuple) + sizeof(tuplen);

	LogicalTapeWrite(state->tapeset, lt,
					 (void *) &tuplen, sizeof(tuplen));
	LogicalTapeWrite(state->tapeset, lt,
					 (void *) tuple, IndexTupleSize(tuple));

	if (state->randomAccess)	/* need trailing length word? */
		LogicalTapeWrite(state->tapeset, lt,
						 (void *) &tuplen, sizeof(tuplen));

	FREEMEM(state, GetMemoryChunkSpace(tuple));
	pfree(tuple);
}

static void
readtup_index(Tuplesortstate *state, TuplesortPos *pos, SortTuple *stup,
			  LogicalTape *lt, unsigned int len)
{
	unsigned int tuplen = len - sizeof(unsigned int);
	IndexTuple	tuple = (IndexTuple) palloc(tuplen);
	size_t readSize;

	Assert(lt); 
	USEMEM(state, GetMemoryChunkSpace(tuple));

	readSize = LogicalTapeRead(state->tapeset, lt, (void *)tuple, tuplen);

	if(readSize != tuplen)
		elog(ERROR, "unexpected end of data");

	if (state->randomAccess)	/* need trailing length word? */
	{
		readSize = LogicalTapeRead(state->tapeset, lt, (void *)&tuplen, sizeof(tuplen));

		if (readSize != sizeof(tuplen))
			elog(ERROR, "unexpected end of data");
	}

	stup->tuple = (void *) tuple;
	/* set up first-column key value */
	stup->datum1 = index_getattr(tuple,
								 1,
								 RelationGetDescr(state->indexRel),
								 &stup->isnull1);
}

static void
reversedirection_index_btree(Tuplesortstate *state)
{
	ScanKey		scanKey = state->indexScanKey;
	int			nkey;

	for (nkey = 0; nkey < state->nKeys; nkey++, scanKey++)
	{
		scanKey->sk_flags ^= (SK_BT_DESC | SK_BT_NULLS_FIRST);
	}
}

static void
reversedirection_index_hash(Tuplesortstate *state)
{
	/* We don't support reversing direction in a hash index sort */
	elog(ERROR, "reversedirection_index_hash is not implemented");
}


/*
 * Routines specialized for DatumTuple case
 */

static int
comparetup_datum(const SortTuple *a, const SortTuple *b, Tuplesortstate *state)
{
	/* Allow interrupting long sorts */
	CHECK_FOR_INTERRUPTS();

	return inlineApplySortFunction(&state->sortOpFn, state->sortFnFlags,
								   a->datum1, a->isnull1,
								   b->datum1, b->isnull1);
}

static void
copytup_datum(Tuplesortstate *state, SortTuple *stup, void *tup)
{
	/* Not currently needed */
	elog(ERROR, "copytup_datum() should not be called");
}

static void
writetup_datum(Tuplesortstate *state, LogicalTape *lt, SortTuple *stup)
{
	void	   *waddr;
	unsigned int tuplen;
	unsigned int writtenlen;

	if (stup->isnull1)
	{
		waddr = NULL;
		tuplen = 0;
	}
	else if (state->datumTypeByVal)
	{
		waddr = &stup->datum1;
		tuplen = sizeof(Datum);
	}
	else
	{
		waddr = DatumGetPointer(stup->datum1);
		tuplen = datumGetSize(stup->datum1, false, state->datumTypeLen);
		Assert(tuplen != 0);
	}

	writtenlen = tuplen + sizeof(unsigned int);

	LogicalTapeWrite(state->tapeset, lt,
					 (void *) &writtenlen, sizeof(writtenlen));
	LogicalTapeWrite(state->tapeset, lt,
					 waddr, tuplen);

	if (state->randomAccess)	/* need trailing length word? */
		LogicalTapeWrite(state->tapeset, lt,
						 (void *) &writtenlen, sizeof(writtenlen));

	if (stup->tuple)
	{
		FREEMEM(state, GetMemoryChunkSpace(stup->tuple));
		pfree(stup->tuple);
	}
}

static void
readtup_datum(Tuplesortstate *state, TuplesortPos *pos, SortTuple *stup,
			  LogicalTape *lt, unsigned int len)
{
	size_t readSize;
	unsigned int tuplen = len - sizeof(unsigned int);

	Assert(lt); 

	if (tuplen == 0)
	{
		/* it's NULL */
		stup->datum1 = (Datum) 0;
		stup->isnull1 = true;
		stup->tuple = NULL;
	}
	else if (state->datumTypeByVal)
	{
		Assert(tuplen == sizeof(Datum));
		readSize = LogicalTapeRead(state->tapeset, lt, (void *)&stup->datum1, tuplen);

		if (readSize != tuplen) 
			elog(ERROR, "unexpected end of data");
		stup->isnull1 = false;
		stup->tuple = NULL;
	}
	else
	{
		void	   *raddr = palloc(tuplen);

		readSize = LogicalTapeRead(state->tapeset, lt, raddr, tuplen);

		if(readSize != tuplen)
			elog(ERROR, "unexpected end of data");

		stup->datum1 = PointerGetDatum(raddr);
		stup->isnull1 = false;
		stup->tuple = raddr;
		USEMEM(state, GetMemoryChunkSpace(raddr));
	}

	if (state->randomAccess)	/* need trailing length word? */
	{
		readSize = LogicalTapeRead(state->tapeset, lt, (void *)&tuplen, sizeof(tuplen));

		if (readSize != sizeof(tuplen)) 
			elog(ERROR, "unexpected end of data");
	}
}

static void
reversedirection_datum(Tuplesortstate *state)
{
	state->sortFnFlags ^= (SK_BT_DESC | SK_BT_NULLS_FIRST);
}

/*
 * Convenience routine to free a tuple previously loaded into sort memory
 */
static void
free_sort_tuple(Tuplesortstate *state, SortTuple *stup)
{
	FREEMEM(state, GetMemoryChunkSpace(stup->tuple));
	pfree(stup->tuple);
}


/*-------------------------------------------------------------------------
 * GPDB specific method implementations
 *-------------------------------------------------------------------------
 */


/*
 * Initialize some extra CDB attributes for the sort, including limit
 * and uniqueness.  Should do this after begin_heap.
 */
void
cdb_tuplesort_init(Tuplesortstate *state, int unique, int sort_flags,
				   int64 maxdistinct)
{
	if (unique)
		state->noduplicates = true;

	state->mppsortflags = sort_flags;

	state->gpmaxdistinct = maxdistinct;

	/* do a standard sort unless performing limit or duplicate
	 * elimination */

	state->standardsort =
			(state->mppsortflags == 0) ||
			((!state->bounded)
			 && (!state->noduplicates)); 
}


/* make a copy of current state pos - used by SharedInputScan */
void
tuplesort_begin_pos(Tuplesortstate *st, TuplesortPos **pos)
{
	TuplesortPos *st_pos;
	Assert(st);

	st_pos = (TuplesortPos *) palloc0(sizeof(TuplesortPos));
	memcpy(st_pos, &(st->pos), sizeof(TuplesortPos));

	if(st->tapeset)
		st_pos->cur_work_tape = LogicalTapeSetDuplicateTape(st->tapeset, st->result_tape);

	*pos = st_pos;
}


Tuplesortstate *
tuplesort_begin_heap_file_readerwriter(ScanState *ss,
		const char *rwfile_prefix, bool isWriter,
		TupleDesc tupDesc,
		int nkeys, AttrNumber *attNums,
		Oid *sortOperators, bool *nullsFirstFlags,
		int workMem, bool randomAccess)
{
	Tuplesortstate *state;
	char statedump[MAXPGPATH];
	char full_prefix[MAXPGPATH];

	Assert(randomAccess);

	int len = snprintf(statedump, sizeof(statedump), "%s/%s_sortstate", PG_TEMP_FILES_DIR, rwfile_prefix);
	insist_log(len <= MAXPGPATH - 1, "could not generate temporary file name");

	len = snprintf(full_prefix, sizeof(full_prefix), "%s/%s",
			PG_TEMP_FILES_DIR,
			rwfile_prefix);
	insist_log(len <= MAXPGPATH - 1, "could not generate temporary file name");

	if(isWriter)
	{
		/*
		 * Writer is a oridinary tuplesort, except the underlying buf file are named by
		 * rwfile_prefix.
		 */
		state = tuplesort_begin_heap(NULL, tupDesc, nkeys, attNums,
									 sortOperators, nullsFirstFlags,
									 workMem, randomAccess);

		state->pfile_rwfile_prefix = MemoryContextStrdup(state->sortcontext, full_prefix);
		state->pfile_rwfile_state = ExecWorkFile_Create(statedump,
				BUFFILE,
				true /* delOnClose */ ,
				0 /* compressType */ );
		Assert(state->pfile_rwfile_state != NULL);

		return state;
	}
	else
	{

		/*
		 * For reader, we really don't know anything about sort op, attNums, etc.
		 * All the readers cares are the data on the logical tape set.  The state
		 * of the logical tape set has been dumped, so we load it back and that is
		 * it.
		 */
		state = tuplesort_begin_common(workMem, randomAccess, false);
		state->status = TSS_SORTEDONTAPE;
		state->randomAccess = true;

		state->readtup = readtup_heap;

		state->pfile_rwfile_prefix = MemoryContextStrdup(state->sortcontext, full_prefix);
		state->pfile_rwfile_state = ExecWorkFile_Open(statedump,
				BUFFILE,
				false /* delOnClose */,
				0 /* compressType */);

		ExecWorkFile *tapefile = ExecWorkFile_Open(full_prefix,
				BUFFILE,
				false /* delOnClose */,
				0 /* compressType */);

		state->tapeset = LoadLogicalTapeSetState(state->pfile_rwfile_state, tapefile);
		state->currentRun = 0;
		state->result_tape = LogicalTapeSetGetTape(state->tapeset, 0);

		state->pos.eof_reached =false;
		state->pos.markpos.tapepos.blkNum = 0;
		state->pos.markpos.tapepos.offset = 0;
		state->pos.markpos.mempos = 0;
		state->pos.markpos_eof = false;
		state->pos.cur_work_tape = NULL;

		return state;
	}
}

void tuplesort_flush(Tuplesortstate *state)
{
	Assert(state->status == TSS_SORTEDONTAPE);
	Assert(state->tapeset && state->pfile_rwfile_state);
	Assert(state->pos.cur_work_tape == NULL);
	LogicalTapeFlush(state->tapeset, state->result_tape, state->pfile_rwfile_state);
	ExecWorkFile_Flush(state->pfile_rwfile_state);
}

static void
tuplesort_sorted_insert(Tuplesortstate *state, SortTuple *tuple,
						int tupleindex, bool checkIndex)
{
	SortTuple  *memtuples;
	SortTuple  *freetup;
	int			comparestat;
	int			j;
	int         searchpoint = 0;

	/*
	 * Save the tupleindex --- see notes above about writing on *tuple. It's a
	 * historical artifact that tupleindex is passed as a separate argument
	 * and not in *tuple, but it's notationally convenient so let's leave it
	 * that way.
	 */
	tuple->tupindex = tupleindex;

	memtuples = state->memtuples;
	Assert(state->memtupcount < state->memtupsize);

	/* compare to the last value first */
	comparestat =
			HEAPCOMPARE(tuple, &memtuples[state->memtupcount - 1]);

	if (state->memtupblimited)
	{
		/* discard the last tuple if it exceeds the LIMIT */
		if (comparestat >= 0)
		{
			freetup = tuple;
			goto L_freetup;
		}
		else
		{
			state->memtupblimited = false;
		}
	}

	if (state->noduplicates && (0 == comparestat))
	{
		freetup = tuple;
		goto L_freetup;
	}

	/* j is the last array index to memtuples, and memtupcount is the
	 * number of values
	 */
	j = state->memtupcount++;

	if (comparestat >= 0)
	{
		memtuples[j] = *tuple;
		goto L_checklimit;
	}

	if (j > 1)
	{
		int lefty  = 0;
		int righty = j-1;

		righty--;

        while (1)
        {
			int middle;

			if (lefty > righty)
					break;

			middle = (lefty + righty) / 2;

			comparestat =
					HEAPCOMPARE(tuple, &memtuples[middle]);

            if (comparestat < 0)
            {
				/* if key < current entry then keep moving left
				 * (eliminate the right interval)
				 */
                righty = middle - 1;
            }
            else
            {
				/* check if can free a duplicate */
				if (state->noduplicates && (0 == comparestat))
				{
					state->memtupcount--;
					freetup = tuple;
					goto L_freetup;
				}

				/* if key >= current entry then keep moving right
                 (eliminate the left interval).  Note that the return
                 value for the estimate gets bumped up to the current
                 position, because we can start a linear scan from
                 this location */

                searchpoint = middle;
                lefty  = middle + 1;
            }
        } /* end while */
	}

	/* Note: only go to position j-1, because we haven't filled in
	 * memtuples[j] yet
	 */
	for (; searchpoint < j; searchpoint++)
	{
		comparestat =
				HEAPCOMPARE(tuple, &memtuples[searchpoint]);
		if (comparestat < 0)
		{
			break;
		}
		if (searchpoint >= (j-1))
			break;
	}

	/*
	   if only a single tuple, and new tuple < memtuple[0], then
	   searchpoint = 0, so memtuple[0] is moved to memtuple[1].

	   If new tuple is less than the last tuple, then searchpoint is
	   set to j-1, so memtuple[j-1] is moved to memtuple[j], which is
	   correct
	*/

	/* check if can free a duplicate */
	if (state->noduplicates && (0 == comparestat))
	{
		state->memtupcount--;
		freetup = tuple;
		goto L_freetup;
	}

	/* move the other elements over by one */
	{
		void	*src = &memtuples[searchpoint];
		void	*dst = &memtuples[searchpoint+1];
		size_t	len = (j-searchpoint) * sizeof(SortTuple);
		memmove(dst, src, len);

		memtuples[searchpoint] = *tuple;
	}

 L_checklimit:
	/* CDB: always add new value if never dumped or no limit, else
	 * drop the last value if it exceeds the limit 
	 */
	if ((state->bounded)
		&& (state->memtupcount > state->bound))
	{
		/* set blimited true if have limit and memtuples are sorted */
		state->memtupblimited = true;

		freetup = &memtuples[--state->memtupcount];
		goto L_freetup;
	}
	return;

	/* free up tuples if necessary */
 L_freetup:
	if (freetup->tuple != NULL)
	{
		FREEMEM(state, GetMemoryChunkSpace(freetup->tuple));
		pfree(freetup->tuple);
	}

	state->discardcount++;
}


/*
 * tuplesort_finalize_stats
 *
 * Finalize the EXPLAIN ANALYZE stats.
 */
void
tuplesort_finalize_stats(Tuplesortstate *state)
{
    if (state->instrument && !state->statsFinalized)
    {
        double  workmemused;

        /* How close did we come to the work_mem limit? */
        FREEMEM(state, 0);              /* update low-water mark */
        workmemused = MemoryContextGetPeakSpace(state->sortcontext);
        if (state->instrument->workmemused < workmemused)
            state->instrument->workmemused = workmemused;

        /* Report executor memory used by our memory context. */
        state->instrument->execmemused +=
            (double)MemoryContextGetPeakSpace(state->sortcontext);

		state->statsFinalized = true;
		tuplesort_get_stats(state,
				&state->instrument->sortMethod,
				&state->instrument->sortSpaceType,
				&state->instrument->sortSpaceUsed);
    }
}

/*
 * tuplesort_set_instrument
 *
 * May be called after tuplesort_begin_xxx() to enable reporting of
 * statistics and events for EXPLAIN ANALYZE.
 *
 * The 'instr' and 'explainbuf' ptrs are retained in the 'state' object for
 * possible use anytime during the sort, up to and including tuplesort_end().
 * The caller must ensure that the referenced objects remain allocated and
 * valid for the life of the Tuplesortstate object; or if they are to be
 * freed early, disconnect them by calling again with NULL pointers.
 */
void
tuplesort_set_instrument(Tuplesortstate            *state,
                         struct Instrumentation    *instrument,
                         struct StringInfoData     *explainbuf)
{
    state->instrument = instrument;
    state->explainbuf = explainbuf;
}                               /* tuplesort_set_instrument */

void
tuplesort_set_gpmon(Tuplesortstate *state, gpmon_packet_t *pkt, int *tick)
{
	state->gpmon_pkt = pkt;
	state->gpmon_sort_tick = tick;
}

void tuplesort_checksend_gpmonpkt(gpmon_packet_t *pkt, int *tick)
{
	if(!pkt)
		return;

	if(gp_enable_gpperfmon)
	{
		if(*tick != gpmon_tick)
			gpmon_send(pkt);

		*tick = gpmon_tick;
	}
}


