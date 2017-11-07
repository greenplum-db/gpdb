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
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/sort/tuplesort_mk.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/heapam.h"
#include "access/nbtree.h"
#include "access/tuptoaster.h"
#include "catalog/pg_type.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_operator.h"
#include "executor/instrument.h"	/* Instrumentation */
#include "lib/stringinfo.h"		/* StringInfo */
#include "executor/nodeSort.h"	/* gpmon */
#include "miscadmin.h"
#include "pg_trace.h"
#include "utils/datum.h"
#include "executor/execWorkfile.h"
#include "utils/logtape.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/pg_rusage.h"
#include "utils/syscache.h"
#include "utils/tuplesort.h"
#include "utils/pg_locale.h"
#include "utils/builtins.h"
#include "utils/tuplesort_mk.h"
#include "utils/tuplesort_mk_details.h"
#include "utils/string_wrapper.h"
#include "utils/faultinjector.h"

#include "cdb/cdbvars.h"

/*
 * Possible states of a Tuplesort object.  These denote the states that
 * persist between calls of Tuplesort routines.
 */
typedef enum
{
	TSS_INITIAL,				/* Loading tuples; still within memory limit */
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

// #define PRINT_SPILL_AND_MEMORY_MESSAGES

/* 
 * Current position of Tuplesort operation.
 */
struct TuplesortPos_mk
{
	/*
	 * These variables are used after completion of sorting to keep track of
	 * the next tuple to return.  (In the tape case, the tape's current read
	 * position is also critical state.)
	 */
	int			current;		/* array index (only used if SORTEDINMEM) */
	bool		eof_reached;	/* reached EOF (needed for cursors) */

	/* markpos_xxx holds marked position for mark and restore */
	union
	{
		LogicalTapePos tapepos;
		long		mempos;
	}			markpos;
	bool		markpos_eof;

	LogicalTape *cur_work_tape; /* current tape that I am working on */
};

/* Merge reader (read back from runs) context for mk_heap */
typedef struct TupsortMergeReadCtxt
{
	Tuplesortstate_mk *tsstate;
	TuplesortPos_mk pos;

	/* Buffer for preread */
	MKEntry    *p;
	int			allocsz;
	int			cnt;
	int			cur;

	int64		mem_allowed;
	int64		mem_used;
} TupsortMergeReadCtxt;

/*
 * Private state of a Tuplesort operation.
 */
struct Tuplesortstate_mk
{
	/* MUST BE FIRST, to match switcheroo_Tuplesortstate */
	bool		is_mk_tuplesortstate;

	TupSortStatus status;		/* enumerated value as shown above */
	int			nKeys;			/* number of columns in sort key */
	bool		randomAccess;	/* did caller request random access? */

	int64		memAllowed;

	int			maxTapes;		/* number of tapes (Knuth's T) */
	int			tapeRange;		/* maxTapes-1 (Knuth's P) */
	MemoryContext sortcontext;	/* memory context holding all sort data */
	LogicalTapeSet *tapeset;	/* logtape.c object for tapes in a temp file */

	ScanState  *ss;

	/* Representation of all spill file names, for spill file reuse */
	workfile_set *work_set;

	/*
	 * MUST be set
	 *
	 * Function to copy a supplied input tuple into palloc'd space and set up
	 * its SortTuple representation (ie, set tuple/datum1/isnull1).  Also,
	 * state->availMem must be decreased by the amount of space used for the
	 * tuple copy (note the SortTuple struct itself is not counted).
	 */
	void		(*copytup) (Tuplesortstate_mk *state, MKEntry *e, void *tup);

	/*
	 * MUST be set
	 *
	 * Function to write a stored tuple onto tape.  The representation of the
	 * tuple on tape need not be the same as it is in memory; requirements on
	 * the tape representation are given below.  After writing the tuple,
	 * pfree() the out-of-line data (not the SortTuple struct!), and increase
	 * state->availMem by the amount of memory space thereby released.
	 */
	long		(*writetup) (Tuplesortstate_mk *state, LogicalTape *lt, MKEntry *e);

	/*
	 * MUST be set
	 *
	 * Function to read a stored tuple from tape back into memory. 'len' is
	 * the already-read length of the stored tuple.  Create a palloc'd copy,
	 * initialize tuple/datum1/isnull1 in the target SortTuple struct, and
	 * decrease state->availMem by the amount of memory space consumed.
	 */
	void		(*readtup) (Tuplesortstate_mk *state, TuplesortPos_mk *pos, MKEntry *e,
										LogicalTape *lt, uint32 len);

	/*
	 * This array holds the tuples now in sort memory.  If we are in state
	 * INITIAL, the tuples are in no particular order; if we are in state
	 * SORTEDINMEM, the tuples are in final sorted order;
	 */
	MKEntry    *entries;
	long		entry_allocsize;
	long		entry_count;

	/*
	 * BUILDRUNS and FINALMERGE: Heap for producing and merging runs
	 */
	MKHeap	   *mkheap;
	MKHeapReader *mkhreader;
	TupsortMergeReadCtxt *mkhreader_ctxt;
	int			mkhreader_allocsize;

	/*
	 * MK context, holds info needed by compare/prepare functions
	 */
	MKContext	mkctxt;

	/*
	 * A flag to indicate whether the stats for this tuplesort has been
	 * finalized.
	 */
	bool		statsFinalized;

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

	LogicalTape *result_tape;	/* actual tape of finished output */
	TuplesortPos_mk pos;		/* current postion */

	/*
	 * Tuple desc and binding, for MemTuple.
	 */
	TupleDesc	tupDesc;
	MemTupleBinding *mt_bind;
	ScanKey		cmpScanKey;

	/*
	 * These variables are specific to the IndexTuple case; they are set by
	 * tuplesort_begin_index and used only by the IndexTuple routines.
	 */
	Relation	indexRel;

	/*
	 * These variables are specific to the Datum case; they are set by
	 * tuplesort_begin_datum and used only by the DatumTuple routines.
	 */
	Oid			datumType;
	Oid			sortOperator;
	bool		nullfirst;

	/* we need typelen and byval in order to know how to copy the Datums. */
	int			datumTypeLen;
	bool		datumTypeByVal;

	/*
	 * CDB: EXPLAIN ANALYZE reporting interface and statistics.
	 */
	struct Instrumentation *instrument;
	struct StringInfoData *explainbuf;
	uint64		totalTupleBytes;
	uint64		totalNumTuples;
	uint64		numTuplesInMem;
	uint64		memUsedBeforeSpill;		/* memory that is used by Sort at the
										 * time of spilling */
	long		arraySizeBeforeSpill;	/* the value for entry_allocsize at
										 * the time of spilling */

	/*
	 * File for dump/load logical tape set.  Used by sharing sort across slice
	 */
	char	   *tapeset_file_prefix;

	/*
	 * State file used to load a logical tape set. Used by sharing sort across
	 * slice
	 */
	ExecWorkFile *tapeset_state_file;

	/* Gpmon */
	gpmon_packet_t *gpmon_pkt;
	int		   *gpmon_sort_tick;
};

static void tuplesort_get_stats_mk(Tuplesortstate_mk* state, const char **sortMethod, const char **spaceType, long *spaceUsed);

static bool
is_sortstate_rwfile(Tuplesortstate_mk *state)
{
	return state->tapeset_state_file != NULL;
}
#ifdef USE_ASSERT_CHECKING
static bool
is_under_sort_or_exec_ctxt(Tuplesortstate_mk *state)
{
	/*
	 * Check if this is executed under sortcontext (most cases) or under
	 * es_query_cxt (when under a SharedInputScan)
	 */
	return CurrentMemoryContext == state->sortcontext ||
		CurrentMemoryContext == state->ss->ps.state->es_query_cxt;
}
#endif

/**
 * Any strings that are STRXFRM_INPUT_LENGTH_LIMIT or larger will store only the
 *	  first STRXFRM_INPUT_LENGTH_LIMIT bytes of the transformed string.
 *
 * Note that this is actually less transformed data in some cases than if the string were
 *	 just a little smaller than STRXFRM_INPUT_LENGTH_LIMIT.  We can probably make the
 *	 transition more gradual but will still want this fading off -- for long strings
 *	 that differ within the first STRXFRM_INPUT_LENGTH_LIMIT bytes then the prefix
 *	 will be sufficient for all but equal cases -- in which case a longer prefix does
 *	 not help (we must resort to datum for comparison)
 *
 * If less than the whole transformed size is stored then the transformed string itself is also
 *	 not copied -- so for large strings we must resort to datum-based comparison.
 */
#define STRXFRM_INPUT_LENGTH_LIMIT (512)
#define COPYTUP(state,stup,tup) ((*(state)->copytup) (state, stup, tup))
#define WRITETUP(state,tape,stup)	((*(state)->writetup) (state, tape, stup))
#define READTUP(state,pos,stup,tape,len) ((*(state)->readtup) (state, pos, stup, tape, len))

static inline bool
LACKMEM_WITH_ESTIMATE(Tuplesortstate_mk *state)
{
	/*
	 * Assert: lackmem is only effective during initial run build because we
	 * don't maintain the estimate after that.
	 */
	Assert(state->status == TSS_INITIAL);

	return MemoryContextGetCurrentSpace(state->sortcontext) + state->mkctxt.estimatedExtraForPrep > state->memAllowed;
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
 * stored in the Tuplesortstate_mk record, if needed.	They are also expected
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

static Tuplesortstate_mk *tuplesort_begin_common(ScanState *ss, int workMem, bool randomAccess, bool allocmemtuple);
static void puttuple_common(Tuplesortstate_mk *state, MKEntry *e);
static void selectnewtape_mk(Tuplesortstate_mk *state);
static void mergeruns(Tuplesortstate_mk *state);
static void beginmerge(Tuplesortstate_mk *state);
static uint32 getlen(Tuplesortstate_mk *state, TuplesortPos_mk *pos, LogicalTape *lt, bool eofOK);
static void markrunend(Tuplesortstate_mk *state, int tapenum);

static void copytup_heap(Tuplesortstate_mk *state, MKEntry *e, void *tup);
static long writetup_heap(Tuplesortstate_mk *state, LogicalTape *lt, MKEntry *e);
static void freetup_heap(MKEntry *e);
static void readtup_heap(Tuplesortstate_mk *state, TuplesortPos_mk *pos, MKEntry *e,
			 LogicalTape *lt, uint32 len);

static void copytup_index(Tuplesortstate_mk *state, MKEntry *e, void *tup);
static long writetup_index(Tuplesortstate_mk *state, LogicalTape *lt, MKEntry *e);
static void freetup_index(MKEntry *e);
static void readtup_index(Tuplesortstate_mk *state, TuplesortPos_mk *pos, MKEntry *e,
			  LogicalTape *lt, uint32 len);

static void freetup_noop(MKEntry *e);

static void copytup_datum(Tuplesortstate_mk *state, MKEntry *e, void *tup);
static long writetup_datum(Tuplesortstate_mk *state, LogicalTape *lt, MKEntry *e);
static void freetup_datum(MKEntry *e);
static void readtup_datum(Tuplesortstate_mk *state, TuplesortPos_mk *pos, MKEntry *e,
			  LogicalTape *lt, uint32 len);

static void tupsort_prepare_char(MKEntry *a, bool isChar);
static int	tupsort_compare_char(MKEntry *v1, MKEntry *v2, MKLvContext *lvctxt, MKContext *mkContext);

static Datum tupsort_fetch_datum_mtup(MKEntry *a, MKContext *mkctxt, MKLvContext *lvctxt, bool *isNullOut);
static Datum tupsort_fetch_datum_itup(MKEntry *a, MKContext *mkctxt, MKLvContext *lvctxt, bool *isNullOut);

static int32 estimateMaxPrepareSizeForEntry(MKEntry *a, struct MKContext *mkctxt);
static int32 estimatePrepareSpaceForChar(struct MKContext *mkContext, MKEntry *e, Datum d, bool isCHAR);

static void tuplesort_inmem_limit_insert(Tuplesortstate_mk *state, MKEntry *e);
static void tuplesort_inmem_nolimit_insert(Tuplesortstate_mk *state, MKEntry *e);
static void tuplesort_heap_insert(Tuplesortstate_mk *state, MKEntry *e);
static void tuplesort_limit_sort(Tuplesortstate_mk *state);

static void tupsort_refcnt(void *vp, int ref);

/* Declare the following as extern so that older dtrace will not complain */
extern void inittapes_mk(Tuplesortstate_mk *state, const char *rwfile_prefix);
extern void dumptuples_mk(Tuplesortstate_mk *state, bool alltuples);
extern void mergeonerun_mk(Tuplesortstate_mk *state);

extern void mkheap_verify_heap(MKHeap *heap, int top);

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

static Tuplesortstate_mk *
tuplesort_begin_common(ScanState *ss, int workMem, bool randomAccess, bool allocmemtuple)
{
	Tuplesortstate_mk *state;
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
	 * Make the Tuplesortstate_mk within the per-sort context.  This way, we
	 * don't need a separate pfree() operation for it at shutdown.
	 */
	oldcontext = MemoryContextSwitchTo(sortcontext);

	/*
	 * palloc0, need to zero out memeories, so we do not need to set every var
	 * of the tuplestate to 0 below.
	 */
	state = (Tuplesortstate_mk *) palloc0(sizeof(Tuplesortstate_mk));

	state->status = TSS_INITIAL;
	state->randomAccess = randomAccess;
	state->mkctxt.bounded = false;
	state->mkctxt.boundUsed = false;
	state->memAllowed = workMem * 1024L;

	state->work_set = NULL;
	state->ss = ss;

	state->sortcontext = sortcontext;

	if (allocmemtuple)
	{
		int			i;

		state->entry_allocsize = 1024;
		state->entries = (MKEntry *) palloc0(state->entry_allocsize * sizeof(MKEntry));

		for (i = 0; i < state->entry_allocsize; ++i)
			mke_blank(state->entries + i);

		/* workMem must be large enough for the minimal memtuples array */
		if (LACKMEM_WITH_ESTIMATE(state))
			elog(ERROR, "insufficient memory allowed for sort");
	}

	/*
	 * maxTapes, tapeRange, and Algorithm D variables will be initialized by
	 * inittapes(), if needed
	 */
	MemoryContextSwitchTo(oldcontext);

	Assert(!state->statsFinalized);

	return state;
}


/*
 * Initialize some extra CDB attributes for the sort, including limit
 * and uniqueness.  Should do this after begin_heap.
 *
 */
void
cdb_tuplesort_init_mk(Tuplesortstate_mk *state, int unique, int sort_flags,
					  int64 maxdistinct)
{
	UnusedArg(sort_flags);
	UnusedArg(maxdistinct);

	if (unique)
		state->mkctxt.unique = true;
}

/* make a copy of current state pos */
void
tuplesort_begin_pos_mk(Tuplesortstate_mk *st, TuplesortPos_mk ** pos)
{
	TuplesortPos_mk *st_pos;

	Assert(st);

	st_pos = (TuplesortPos_mk *) palloc(sizeof(TuplesortPos_mk));
	memcpy(st_pos, &(st->pos), sizeof(TuplesortPos_mk));

	if (st->result_tape)
	{
		st_pos->cur_work_tape = LogicalTapeSetDuplicateTape(st->tapeset, st->result_tape);
	}
	else
	{
		/*
		 * sort did not finish completely due to QueryFinishPending so pretend
		 * that there are no tuples
		 */
		st_pos->eof_reached = true;
	}
	*pos = st_pos;
}

void
create_mksort_context(
					  MKContext *mkctxt,
					  int nkeys, AttrNumber *attNums,
					  Oid *sortOperators, bool *nullsFirstFlags,
					  ScanKey sk,
				MKFetchDatumForPrepare fetchForPrep, MKFreeTuple freeTupleFn,
					  TupleDesc tupdesc, bool tbyv, int tlen)
{
	int			i;

	Assert(mkctxt);
	mkctxt->total_lv = nkeys;
	mkctxt->lvctxt = (MKLvContext *) palloc0(sizeof(MKLvContext) * nkeys);

	AssertEquivalent(sortOperators == NULL, sk != NULL);
	AssertEquivalent(fetchForPrep == NULL, tupdesc == NULL);

	mkctxt->fetchForPrep = fetchForPrep;
	mkctxt->tupdesc = tupdesc;
	if (tupdesc)
		mkctxt->mt_bind = create_memtuple_binding(tupdesc);

	mkctxt->cpfr = tupsort_cpfr;
	mkctxt->freeTup = freeTupleFn;
	mkctxt->estimatedExtraForPrep = 0;

	lc_guess_strxfrm_scaling_factor(&mkctxt->strxfrmScaleFactor, &mkctxt->strxfrmConstantFactor);

	for (i = 0; i < nkeys; ++i)
	{
		Oid			sortFunction;
		bool		reverse;
		MKLvContext *sinfo = mkctxt->lvctxt + i;

		if (sortOperators)
		{
			Assert(sortOperators[i] != 0);

			/* Select a sort function */
			if (!get_compare_function_for_ordering_op(sortOperators[i],
													&sortFunction, &reverse))
				elog(ERROR, "operator %u is not a valid ordering operator",
					 sortOperators[i]);

			/*
			 * We needn't fill in sk_strategy or sk_subtype since these
			 * scankeys will never be passed to an index.
			 */
			ScanKeyInit(&sinfo->scanKey,
						attNums ? attNums[i] : i + 1,
						InvalidStrategy,
						sortFunction,
						(Datum) 0);

			/* However, we use btree's conventions for encoding directionality */
			if (reverse)
				sinfo->scanKey.sk_flags |= SK_BT_DESC;
			if (nullsFirstFlags[i])
				sinfo->scanKey.sk_flags |= SK_BT_NULLS_FIRST;
		}
		else
		{
			sinfo->scanKey = sk[i];		/* structural copy */
		}

		AssertImply(attNums, attNums[i] != 0);
		/* Per lv context info.  Need later for prep */
		sinfo->attno = attNums ? attNums[i] : i + 1;

		sinfo->lvtype = MKLV_TYPE_NONE;

		if (tupdesc)
		{
			sinfo->typByVal = tupdesc->attrs[sinfo->attno - 1]->attbyval;
			sinfo->typLen = tupdesc->attrs[sinfo->attno - 1]->attlen;

			if (sinfo->scanKey.sk_func.fn_addr == btint4cmp)
				sinfo->lvtype = MKLV_TYPE_INT32;
			if (!lc_collate_is_c())
			{
				if (sinfo->scanKey.sk_func.fn_addr == bpcharcmp)
					sinfo->lvtype = MKLV_TYPE_CHAR;
				else if (sinfo->scanKey.sk_func.fn_addr == bttextcmp)
					sinfo->lvtype = MKLV_TYPE_TEXT;
			}
		}
		else
		{
			sinfo->typByVal = tbyv;
			sinfo->typLen = tlen;
		}
		sinfo->mkctxt = mkctxt;
	}
}

Tuplesortstate_mk *
tuplesort_begin_heap_mk(ScanState *ss,
						TupleDesc tupDesc,
						int nkeys, AttrNumber *attNums,
						Oid *sortOperators, bool *nullsFirstFlags,
						int workMem, bool randomAccess)
{
	Tuplesortstate_mk *state = tuplesort_begin_common(ss, workMem, randomAccess, true);
	MemoryContext oldcontext;

	oldcontext = MemoryContextSwitchTo(state->sortcontext);

	AssertArg(nkeys > 0);

	TRACE_POSTGRESQL_TUPLESORT_BEGIN(nkeys, workMem, randomAccess);

	state->nKeys = nkeys;
	state->copytup = copytup_heap;
	state->writetup = writetup_heap;
	state->readtup = readtup_heap;

	state->tupDesc = tupDesc;	/* assume we need not copy tupDesc */
	state->mt_bind = create_memtuple_binding(tupDesc);
	state->cmpScanKey = NULL;

	create_mksort_context(
						  &state->mkctxt,
						  nkeys, attNums,
						  sortOperators, nullsFirstFlags,
						  NULL,
						  tupsort_fetch_datum_mtup,
						  freetup_heap,
						  tupDesc, 0, 0);

	MemoryContextSwitchTo(oldcontext);

	return state;
}

Tuplesortstate_mk *
tuplesort_begin_heap_file_readerwriter_mk(ScanState *ss,
									const char *rwfile_prefix, bool isWriter,
										  TupleDesc tupDesc,
										  int nkeys, AttrNumber *attNums,
								   Oid *sortOperators, bool *nullsFirstFlags,
										  int workMem, bool randomAccess)
{
	Tuplesortstate_mk *state;
	char		statedump[MAXPGPATH];
	char		full_prefix[MAXPGPATH];

	Assert(randomAccess);

	int			len = snprintf(statedump, sizeof(statedump), "%s/%s_sortstate",
							   PG_TEMP_FILES_DIR,
							   rwfile_prefix);

	insist_log(len <= MAXPGPATH - 1, "could not generate temporary file name");

	len = snprintf(full_prefix, sizeof(full_prefix), "%s/%s",
				   PG_TEMP_FILES_DIR,
				   rwfile_prefix);
	insist_log(len <= MAXPGPATH - 1, "could not generate temporary file name");


	if (isWriter)
	{
		/*
		 * Writer is a ordinary tuplesort, except the underlying buf file are
		 * named by rwfile_prefix.
		 */
		state = tuplesort_begin_heap_mk(ss, tupDesc, nkeys, attNums,
										sortOperators, nullsFirstFlags,
										workMem, randomAccess);

		state->tapeset_file_prefix = MemoryContextStrdup(state->sortcontext, full_prefix);

		state->tapeset_state_file = ExecWorkFile_Create(statedump,
														BUFFILE,
													  true /* delOnClose */ ,
													  0 /* compressType */ );
		Assert(state->tapeset_state_file != NULL);

		return state;
	}
	else
	{
		/*
		 * For reader, we really don't know anything about sort op, attNums,
		 * etc. All the readers cares are the data on the logical tape set.
		 * The state of the logical tape set has been dumped, so we load it
		 * back and that is it.
		 */
		MemoryContext oldctxt;

		state = tuplesort_begin_common(ss, workMem, randomAccess, false);
		state->status = TSS_SORTEDONTAPE;
		state->randomAccess = true;

		state->readtup = readtup_heap;

		oldctxt = MemoryContextSwitchTo(state->sortcontext);

		state->tapeset_file_prefix = MemoryContextStrdup(state->sortcontext, full_prefix);

		state->tapeset_state_file = ExecWorkFile_Open(statedump,
													  BUFFILE,
													  false /* delOnClose */ ,
													  0 /* compressType */ );
		ExecWorkFile *tapefile = ExecWorkFile_Open(full_prefix,
												   BUFFILE,
												   false /* delOnClose */ ,
												   0 /* compressType */ );

		state->tapeset = LoadLogicalTapeSetState(state->tapeset_state_file, tapefile);
		state->currentRun = 0;
		state->result_tape = LogicalTapeSetGetTape(state->tapeset, 0);

		state->pos.eof_reached = false;
		state->pos.markpos.tapepos.blkNum = 0;
		state->pos.markpos.tapepos.offset = 0;
		state->pos.markpos.mempos = 0;
		state->pos.markpos_eof = false;
		state->pos.cur_work_tape = NULL;

		MemoryContextSwitchTo(oldctxt);
		return state;
	}
}

Tuplesortstate_mk *
tuplesort_begin_index_mk(Relation indexRel,
						 bool enforceUnique,
						 int workMem, bool randomAccess)
{
	Tuplesortstate_mk *state = tuplesort_begin_common(NULL, workMem, randomAccess, true);
	MemoryContext oldcontext;
	TupleDesc	tupdesc;

	oldcontext = MemoryContextSwitchTo(state->sortcontext);

	TRACE_POSTGRESQL_TUPLESORT_BEGIN(enforceUnique, workMem, randomAccess);

	state->nKeys = RelationGetNumberOfAttributes(indexRel);
	tupdesc = RelationGetDescr(indexRel);

	state->copytup = copytup_index;
	state->writetup = writetup_index;
	state->readtup = readtup_index;

	state->indexRel = indexRel;
	state->cmpScanKey = _bt_mkscankey_nodata(indexRel);

	create_mksort_context(
						  &state->mkctxt,
						  state->nKeys, NULL,
						  NULL, NULL,
						  state->cmpScanKey,
						  tupsort_fetch_datum_itup,
						  freetup_index,
						  tupdesc, 0, 0);

	state->mkctxt.enforceUnique = enforceUnique;
	state->mkctxt.indexname = RelationGetRelationName(indexRel);
	state->mkctxt.indexRel = indexRel;
	MemoryContextSwitchTo(oldcontext);

	return state;
}

Tuplesortstate_mk *
tuplesort_begin_datum_mk(ScanState *ss,
						 Oid datumType,
						 Oid sortOperator, bool nullsFirstFlag,
						 int workMem, bool randomAccess)
{
	Tuplesortstate_mk *state = tuplesort_begin_common(ss, workMem, randomAccess, true);
	MemoryContext oldcontext;
	int16		typlen;
	bool		typbyval;

	oldcontext = MemoryContextSwitchTo(state->sortcontext);

	TRACE_POSTGRESQL_TUPLESORT_BEGIN(datumType, workMem, randomAccess);

	state->nKeys = 1;			/* always a one-column sort */

	state->copytup = copytup_datum;
	state->writetup = writetup_datum;
	state->readtup = readtup_datum;

	state->datumType = datumType;

	/* lookup necessary attributes of the datum type */
	get_typlenbyval(datumType, &typlen, &typbyval);
	state->datumTypeLen = typlen;
	state->datumTypeByVal = typbyval;

	state->sortOperator = sortOperator;
	state->cmpScanKey = NULL;
	state->nullfirst = nullsFirstFlag;
	create_mksort_context(
						  &state->mkctxt,
						  1, NULL,
						  &sortOperator, &nullsFirstFlag,
						  NULL,
						  NULL, /* tupsort_prepare_datum, */
						  typbyval ? freetup_noop : freetup_datum,
						  NULL, typbyval, typlen);

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
tuplesort_set_bound_mk(Tuplesortstate_mk *state, int64 bound)
{
	Assert(!state->mkctxt.bounded);

#ifdef DEBUG_BOUNDED_SORT
	/* Honor GUC setting that disables the feature (for easy testing) */
	if (!optimize_bounded_sort)
		return;
#endif

	/* We want to be able to compute bound * 2, so limit the setting */
	if (bound > (int64) (INT_MAX / 2))
		return;

	state->mkctxt.bounded = true;
	state->mkctxt.bound = (int) bound;
	state->mkctxt.limitmask = -1;
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
tuplesort_end_mk(Tuplesortstate_mk *state)
{
	long		spaceUsed;

	if (state->tapeset)
		spaceUsed = LogicalTapeSetBlocks(state->tapeset) * (BLCKSZ / 1024);
	else
		spaceUsed = (MemoryContextGetCurrentSpace(state->sortcontext) + 1024) / 1024;

	/*
	 * Call before state->tapeset is closed.
	 */
	tuplesort_finalize_stats_mk(state);

	/*
	 * Delete temporary "tape" files, if any.
	 *
	 * Note: want to include this in reported total cost of sort, hence need
	 * for two #ifdef TRACE_SORT sections.
	 */
	if (state->tapeset)
	{
		LogicalTapeSetClose(state->tapeset, state->work_set);
		state->tapeset = NULL;

		if (state->tapeset_state_file)
		{
			workfile_mgr_close_file(state->work_set, state->tapeset_state_file);
		}
	}

	if (state->work_set)
	{
		workfile_mgr_close_set(state->work_set);
	}


	TRACE_POSTGRESQL_TUPLESORT_END(state->tapeset ? 1 : 0, spaceUsed);

	/*
	 * Free the per-sort memory context, thereby releasing all working memory,
	 * including the Tuplesortstate_mk struct itself.
	 */
	MemoryContextDelete(state->sortcontext);
}

/*
 * tuplesort_finalize_stats_mk
 *
 * Finalize the EXPLAIN ANALYZE stats.
 */
void
tuplesort_finalize_stats_mk(Tuplesortstate_mk *state)
{
	if (state->instrument && state->instrument->need_cdb && !state->statsFinalized)
	{
		Size		maxSpaceUsedOnSort = MemoryContextGetPeakSpace(state->sortcontext);

		/* Report executor memory used by our memory context. */
		state->instrument->execmemused += (double) maxSpaceUsedOnSort;

		if (state->instrument->workmemused < maxSpaceUsedOnSort)
		{
			state->instrument->workmemused = maxSpaceUsedOnSort;
		}

		if (state->numTuplesInMem < state->totalNumTuples)
		{
			uint64		mem_for_metadata = sizeof(Tuplesortstate_mk) +
			state->arraySizeBeforeSpill * sizeof(MKEntry);
			double		tupleRatio = ((double) state->totalNumTuples) / ((double) state->numTuplesInMem);

			/*
			 * The memwanted is summed up of the following: (1) metadata (2)
			 * the array size (3) the prorated number of bytes for all tuples,
			 * estimated from the memUsedBeforeSpill. Note that because of our
			 * memory allocation algorithm, the used memory for tuples may be
			 * much larger than the actual bytes needed for tuples. (4) the
			 * prorated number of bytes for extra space needed.
			 */
			uint64		memwanted =
			sizeof(Tuplesortstate_mk) /* (1) */ +
			state->totalNumTuples * sizeof(MKEntry) /* (2) */ +
			(uint64) (tupleRatio * (double) (state->memUsedBeforeSpill - mem_for_metadata)) /* (3) */ +
			(uint64) (tupleRatio * (double) state->mkctxt.estimatedExtraForPrep) /* (4) */ ;

			state->instrument->workmemwanted =
				Max(state->instrument->workmemwanted, memwanted);
		}

		state->statsFinalized = true;
		tuplesort_get_stats_mk(state, &state->instrument->sortMethod, &state->instrument->sortSpaceType, &state->instrument->sortSpaceUsed);
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
 * valid for the life of the Tuplesortstate_mk object; or if they are to be
 * freed early, disconnect them by calling again with NULL pointers.
 */
void
tuplesort_set_instrument_mk(Tuplesortstate_mk *state,
							struct Instrumentation *instrument,
							struct StringInfoData *explainbuf)
{
	state->instrument = instrument;
	state->explainbuf = explainbuf;
}	/* tuplesort_set_instrument */

/*
 * Accept one tuple while collecting input data for sort.
 *
 * Note that the input data is always copied; the caller need not save it.
 */
void
tuplesort_puttupleslot_mk(Tuplesortstate_mk *state, TupleTableSlot *slot)
{
	MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);
	MKEntry		e;

	mke_blank(&e);

	COPYTUP(state, &e, (void *) slot);
	puttuple_common(state, &e);

	MemoryContextSwitchTo(oldcontext);
}

/*
 * Accept one index tuple while collecting input data for sort.
 *
 * Note that the input tuple is always copied; the caller need not save it.
 */
void
tuplesort_putindextuple_mk(Tuplesortstate_mk *state, IndexTuple tuple)
{
	MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);
	MKEntry		e;

	mke_blank(&e);

	COPYTUP(state, &e, (void *) tuple);
	puttuple_common(state, &e);

	MemoryContextSwitchTo(oldcontext);
}

/*
 * Accept one Datum while collecting input data for sort.
 *
 * If the Datum is pass-by-ref type, the value will be copied.
 */
void
tuplesort_putdatum_mk(Tuplesortstate_mk *state, Datum val, bool isNull)
{
	MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);
	MKEntry		e;

	mke_blank(&e);

	/*
	 * If it's a pass-by-reference value, copy it into memory we control, and
	 * decrease availMem.  Then call the common code.
	 */
	if (isNull || state->datumTypeByVal)
	{
		e.d = val;
		if (isNull)
			mke_set_null(&e, state->nullfirst);
		else
			mke_set_not_null(&e);
	}
	else
	{
		mke_set_not_null(&e);
		e.d = datumCopy(val, false, state->datumTypeLen);
		state->totalTupleBytes += state->datumTypeLen;
	}

	puttuple_common(state, &e);
	MemoryContextSwitchTo(oldcontext);
}

/*
 * grow_unsorted_array
 *	 Grow the unsorted array to allow more entries to be inserted later.

 * If there are no enough memory available for the growth, this function
 * returns false. Otherwsie, returns true.
 */
static bool
grow_unsorted_array(Tuplesortstate_mk *state)
{
	/*
	 * We want to grow the array twice as large as the previous one. However,
	 * when we are close to the memory limit, we do not want to do so.
	 * Otherwise, too much memory got allocated for the metadata, not to the
	 * tuple itself. We estimate the maximum number of entries that is
	 * possible under the current memory limit by considering both metadata
	 * and tuple size.
	 */
	if (state->memAllowed < MemoryContextGetCurrentSpace(state->sortcontext))
		return false;

	uint64		availMem = state->memAllowed - MemoryContextGetCurrentSpace(state->sortcontext);
	uint64		avgTupSize = (uint64) (((double) state->totalTupleBytes) / ((double) state->totalNumTuples));
	uint64		avgExtraForPrep = (uint64) (((double) state->mkctxt.estimatedExtraForPrep) / ((double) state->totalNumTuples));

	if ((availMem / (sizeof(MKEntry) + avgTupSize + avgExtraForPrep)) == 0)
		return false;

	int			maxNumEntries = state->entry_allocsize + (availMem / (sizeof(MKEntry) + avgTupSize + avgExtraForPrep));
	int			newNumEntries = Min(maxNumEntries, state->entry_allocsize * 2);

	state->entries = (MKEntry *) repalloc(state->entries, newNumEntries * sizeof(MKEntry));
	for (int entryNo = state->entry_allocsize; entryNo < newNumEntries; entryNo++)
		mke_blank(state->entries + entryNo);

	state->entry_allocsize = newNumEntries;

	return true;
}


/*
 * Shared code for tuple and datum cases.
 */
static void
puttuple_common(Tuplesortstate_mk *state, MKEntry *e)
{
	Assert(is_under_sort_or_exec_ctxt(state));
	state->totalNumTuples++;

	bool		growSucceed = true;

	switch (state->status)
	{
		case TSS_INITIAL:

			/*
			 * Save the tuple into the unsorted array.  First, grow the array
			 * as needed.  Note that we try to grow the array when there is
			 * still one free slot remaining --- if we fail, there'll still be
			 * room to store the incoming tuple, and then we'll switch to
			 * tape-based operation.
			 */
			if (!state->mkheap && state->entry_count >= state->entry_allocsize - 1)
			{
				growSucceed = grow_unsorted_array(state);
			}

			/* full sort? */
			if (!state->mkctxt.bounded)
			{
				tuplesort_inmem_nolimit_insert(state, e);
			}
			else
			{
				/* Limit sort insert */
				tuplesort_inmem_limit_insert(state, e);
			}

			/* If out of work_mem, switch to diskmode */
			if (!growSucceed)
			{
				if (state->mkheap != NULL)
				{
					/*
					 * Corner case. LIMIT == amount of entries in memory. In
					 * this case, we failed to grow array, but we just created
					 * the heap. No need to spill in this case, we'll just use
					 * the in-memory heap.
					 */
					Assert(state->mkctxt.bounded);
					Assert(state->numTuplesInMem == state->mkheap->count);
				}
				else
				{
					Assert(state->mkheap == NULL);
					Assert(state->entry_count > 0);
					state->arraySizeBeforeSpill = state->entry_allocsize;
					state->memUsedBeforeSpill = MemoryContextGetPeakSpace(state->sortcontext);
					inittapes_mk(state, is_sortstate_rwfile(state) ? state->tapeset_file_prefix : NULL);
					Assert(state->status == TSS_BUILDRUNS);
				}

				if (state->instrument)
				{
					state->instrument->workfileCreated = true;
				}
			}

			break;

		case TSS_BUILDRUNS:

			/*
			 * Insert the tuple into the heap
			 */
			Assert(state->mkheap);
			tuplesort_heap_insert(state, e);
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
tuplesort_performsort_mk(Tuplesortstate_mk *state)
{
	MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);

	TRACE_POSTGRESQL_TUPLESORT_PERFORM_SORT();

	switch (state->status)
	{
		case TSS_INITIAL:

			/*
			 * We were able to accumulate all the tuples within the allowed
			 * amount of memory.  Just qsort 'em and we're done.
			 */
			if (!state->mkctxt.bounded)
				mk_qsort(state->entries, state->entry_count, &state->mkctxt);
			else
				tuplesort_limit_sort(state);

			state->pos.current = 0;
			state->pos.eof_reached = false;
			state->pos.markpos.mempos = 0;
			state->pos.markpos_eof = false;
			state->status = TSS_SORTEDINMEM;
			if (state->mkctxt.bounded)
			{
				state->mkctxt.boundUsed = true;
			}

			/* Not shareinput sort, we are done. */
			if (!is_sortstate_rwfile(state))
				break;

			/* Shareinput sort, need to put this stuff onto disk */
			inittapes_mk(state, state->tapeset_file_prefix);
			/* Fall through */
		case TSS_BUILDRUNS:

#ifdef PRINT_SPILL_AND_MEMORY_MESSAGES
			elog(INFO, "Done building runs.  Mem peak is now %ld", (long) MemoryContextGetPeakSpace(state->sortcontext));
#endif   /* // PRINT_SPILL_AND_MEMORY_MESSAGES */

			dumptuples_mk(state, true);

#ifdef PRINT_SPILL_AND_MEMORY_MESSAGES
			elog(INFO, "Done tuple dump.  Mem peak is now %ld", (long) MemoryContextGetPeakSpace(state->sortcontext));
#endif   /* // PRINT_SPILL_AND_MEMORY_MESSAGES */

			HOLD_INTERRUPTS();

			/*
			 * MPP-18288: Do not change this log message, it is used to test
			 * mksort query cancellation
			 */
			elog(DEBUG1, "ExecSort: mksort starting merge runs  >>======== ");
			RESUME_INTERRUPTS();

			mergeruns(state);

			HOLD_INTERRUPTS();

			/*
			 * MPP-18288: Do not change this log message, it is used to test
			 * mksort query cancellation
			 */
			elog(DEBUG1, "ExecSort: mksort finished merge runs  ++++++++>> ");
			RESUME_INTERRUPTS();

			state->pos.eof_reached = false;
			state->pos.markpos.tapepos.blkNum = 0;
			state->pos.markpos.tapepos.offset = 0;
			state->pos.markpos_eof = false;

			break;

		default:
			elog(ERROR, "Invalid tuplesort state");
			break;
	}
	MemoryContextSwitchTo(oldcontext);
}

void
tuplesort_flush_mk(Tuplesortstate_mk *state)
{
	Assert(state->status == TSS_SORTEDONTAPE);
	Assert(state->tapeset && state->tapeset_state_file);
	Assert(state->pos.cur_work_tape == NULL);
	elog(gp_workfile_caching_loglevel, "tuplesort_mk: writing logical tape state to file");
	LogicalTapeFlush(state->tapeset, state->result_tape, state->tapeset_state_file);
	ExecWorkFile_Flush(state->tapeset_state_file);
}

/*
 * Internal routine to fetch the next tuple in either forward or back
 * direction into *stup.  Returns FALSE if no more tuples.
 * If *should_free is set, the caller must pfree stup.tuple when done with it.
 */
static bool
tuplesort_gettuple_common_pos(Tuplesortstate_mk *state, TuplesortPos_mk *pos,
							  bool forward, MKEntry *e, bool *should_free)
{
	uint32		tuplen;
	LogicalTape *work_tape;
	bool		fOK;

	Assert(is_under_sort_or_exec_ctxt(state));

	/*
	 * No output if we are told to finish execution.
	 */
	if (QueryFinishPending)
	{
		*should_free = false;
		return false;
	}

	switch (state->status)
	{
		case TSS_SORTEDINMEM:
			Assert(forward || state->randomAccess);
			*should_free = false;

			if (forward)
			{
				if (state->mkctxt.unique)
				{
					/**
					  * request to skip duplicates.  qsort has already replaced all but one of each
					  *   values with empty so skip the empties
					  * When limit heap sort was used, the limit heap sort should have done this
					  */
					while (pos->current < state->entry_count &&
						   mke_is_empty(&state->entries[pos->current]))
					{
						pos->current++;
					}
				}

				if (pos->current < state->entry_count)
				{
					*e = state->entries[pos->current];
					pos->current++;
					return true;
				}
				pos->eof_reached = true;
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
					pos->current--;		/* pos->current points to one above
										 * the last returned tuple */
					if (pos->current <= 0)
						return false;
				}

				if (state->mkctxt.unique)
				{
					/**
					 * request to skip duplicates.  qsort has already replaced all but one of each
					 *	 values with empty so skip the empties
					 * When limit heap sort was used, the limit heap sort should have done this
					 */
					while (pos->current - 1 >= 0 &&
						   mke_is_empty(&state->entries[pos->current - 1]))
					{
						pos->current--;
					}
					if (pos->current <= 0)
						return false;
				}

				*e = state->entries[pos->current - 1];

				return true;
			}
			break;

		case TSS_SORTEDONTAPE:
			AssertEquivalent((pos == &state->pos), (pos->cur_work_tape == NULL));
			Assert(forward || state->randomAccess);
			*should_free = true;
			work_tape = pos->cur_work_tape == NULL ? state->result_tape : pos->cur_work_tape;

			if (forward)
			{
				if (pos->eof_reached)
					return false;
				if ((tuplen = getlen(state, pos, work_tape, true)) != 0)
				{
					READTUP(state, pos, e, work_tape, tuplen);
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
			 * Seek position is pointing just past the zero tuplen at the end
			 * of file; back up to fetch last tuple's ending length word.  If
			 * seek fails we must have a completely empty file.
			 */
			fOK = LogicalTapeBackspace(state->tapeset, work_tape, 2 * sizeof(uint32));
			if (!fOK)
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
				fOK = LogicalTapeBackspace(state->tapeset, work_tape, tuplen + 2 * sizeof(uint32));

				if (!fOK)
				{
					/*
					 * If that fails, presumably the prev tuple is the first
					 * in the file.  Back up so that it becomes next to read
					 * in forward direction (not obviously right, but that is
					 * what in-memory case does).
					 */
					fOK = LogicalTapeBackspace(state->tapeset, work_tape, tuplen + 2 * sizeof(uint32));
					if (!fOK)
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

			READTUP(state, pos, e, work_tape, tuplen);
			return true;

		case TSS_FINALMERGE:
			Assert(forward);
			Assert(pos == &state->pos && pos->cur_work_tape == NULL);

			*should_free = true;
			mkheap_putAndGet(state->mkheap, e);
			return !mke_is_empty(e);

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
tuplesort_gettupleslot_mk(Tuplesortstate_mk *state, bool forward,
						  TupleTableSlot *slot)
{
	return tuplesort_gettupleslot_pos_mk(state, &state->pos, forward, slot, state->sortcontext);
}

bool
tuplesort_gettupleslot_pos_mk(Tuplesortstate_mk *state, TuplesortPos_mk *pos,
				  bool forward, TupleTableSlot *slot, MemoryContext mcontext)
{
	MemoryContext oldcontext = MemoryContextSwitchTo(mcontext);
	MKEntry		e;

	bool		should_free = false;
	bool		fOK;

	mke_set_empty(&e);
	fOK = tuplesort_gettuple_common_pos(state, pos, forward, &e, &should_free);

	MemoryContextSwitchTo(oldcontext);

	if (fOK)
	{
		Assert(!mke_is_empty(&e));
		ExecStoreMinimalTuple(e.ptr, slot, should_free);

#ifdef USE_ASSERT_CHECKING
		if (should_free && state->mkheap != NULL && state->mkheap->count > 0)
		{
			Assert(e.ptr != NULL &&
				   (e.ptr != state->mkheap->lvtops->ptr));
		}

#endif

		if (state->gpmon_pkt)
			Gpmon_Incr_Rows_Out(state->gpmon_pkt);

		return true;
	}

	ExecClearTuple(slot);
	return false;
}

/*
 * Fetch the next index tuple in either forward or back direction.
 * Returns NULL if no more tuples.  If *should_free is set, the
 * caller must pfree the returned tuple when done with it.
 */
IndexTuple
tuplesort_getindextuple_mk(Tuplesortstate_mk *state, bool forward,
						   bool *should_free)
{
	MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);
	MKEntry		e;
	bool		fOK = tuplesort_gettuple_common_pos(state, &state->pos, forward, &e, should_free);

	MemoryContextSwitchTo(oldcontext);

	if (fOK)
		return (IndexTuple) (e.ptr);
	return NULL;
}

/*
 * Fetch the next Datum in either forward or back direction.
 * Returns FALSE if no more datums.
 *
 * If the Datum is pass-by-ref type, the returned value is freshly palloc'd
 * and is now owned by the caller.
 */
bool
tuplesort_getdatum_mk(Tuplesortstate_mk *state, bool forward,
					  Datum *val, bool *isNull)
{
	MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);
	MKEntry		e;
	bool		should_free = false;

	bool		fOK = tuplesort_gettuple_common_pos(state, &state->pos, forward, &e, &should_free);

	if (fOK)
	{
		*isNull = mke_is_null(&e);
		if (*isNull || state->datumTypeByVal || should_free)
			*val = e.d;
		else
			*val = datumCopy(e.d, false, state->datumTypeLen);
	}

	MemoryContextSwitchTo(oldcontext);
	return fOK;
}

/*
 * inittapes - initialize for tape sorting.
 *
 * This is called only if we have found we don't have room to sort in memory.
 */
void
inittapes_mk(Tuplesortstate_mk *state, const char *rwfile_prefix)
{
	int			maxTapes;
	int			j;
	long		tapeSpace;

	Assert(is_under_sort_or_exec_ctxt(state));

	/* Compute number of tapes to use: merge order plus 1 */
	maxTapes = tuplesort_merge_order(state->memAllowed) + 1;

#ifdef PRINT_SPILL_AND_MEMORY_MESSAGES
	elog(INFO, "Spilling after %d", (int) state->entry_count);
#endif   /* // PRINT_SPILL_AND_MEMORY_MESSAGES */

	/*
	 * We must have at least 2*maxTapes slots in the memtuples[] array, else
	 * we'd not have room for merge heap plus preread.  It seems unlikely that
	 * this case would ever occur, but be safe.
	 */
	maxTapes = Min(maxTapes, state->entry_count / 2);
	maxTapes = Max(maxTapes, 1);

#ifdef PRINT_SPILL_AND_MEMORY_MESSAGES
	elog(INFO, "     maxtapes %d Mem peak is now %ld", maxTapes, (long) MemoryContextGetPeakSpace(state->sortcontext));
#endif   /* // PRINT_SPILL_AND_MEMORY_MESSAGES */

	/*
	 * XXX XXX: with losers, only need 1x slots because we don't need a merge
	 * heap
	 */

	state->maxTapes = maxTapes;
	state->tapeRange = maxTapes - 1;

	TRACE_POSTGRESQL_TUPLESORT_SWITCH_EXTERNAL(maxTapes);

	/*
	 * Decrease availMem to reflect the space needed for tape buffers; but
	 * don't decrease it to the point that we have no room for tuples. (That
	 * case is only likely to occur if sorting pass-by-value Datums; in all
	 * other scenarios the memtuples[] array is unlikely to occupy more than
	 * half of allowedMem.  In the pass-by-value case it's not important to
	 * account for tuple space, so we don't care if LACKMEM becomes
	 * inaccurate.)
	 */
	tapeSpace = maxTapes * TAPE_BUFFER_OVERHEAD;
	Assert(state->work_set == NULL);

	/*
	 * Create the tape set and allocate the per-tape data arrays.
	 */
	if (!rwfile_prefix)
	{
		state->work_set = workfile_mgr_create_set(BUFFILE, false /* can_be_reused */ , NULL /* ps */ );
		state->tapeset_state_file = workfile_mgr_create_fileno(state->work_set, WORKFILE_NUM_MKSORT_METADATA);

		ExecWorkFile *tape_file = workfile_mgr_create_fileno(state->work_set, WORKFILE_NUM_MKSORT_TAPESET);

		state->tapeset = LogicalTapeSetCreate_File(tape_file, maxTapes);
	}
	else
	{
		/*
		 * We are shared XSLICE, use given prefix to create files so that
		 * consumers can find them
		 */
		ExecWorkFile *tape_file = ExecWorkFile_Create(rwfile_prefix,
													  BUFFILE,
													  true /* delOnClose */ ,
													  0 /* compressType */ );

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
	Assert(state->mkheap == NULL);
	Assert(state->status == TSS_INITIAL || state->status == TSS_SORTEDINMEM);

#ifdef PRINT_SPILL_AND_MEMORY_MESSAGES
	elog(INFO, "     about to make heap mem peak is now %ld", (long) MemoryContextGetPeakSpace(CurrentMemoryContext));
#endif   /* // PRINT_SPILL_AND_MEMORY_MESSAGES */

	if (state->status == TSS_INITIAL)
	{
		/*
		 * We are now building heap from array for run-building using
		 * replacement-selection algorithm. Such run-building heap need to be
		 * a MIN-HEAP, but for limit sorting, we use a MAX-HEAP. The way we
		 * convert MIN-HEAP to MAX-HEAP is by setting the limitmask to -1, and
		 * then using the limitmask in mkheap_compare() in tuplesort_mkheap.c.
		 * So, to restore the MAX-HEAP to a MIN-HEAP, we can just revert the
		 * limitmask to 0. This is needed for MPP-19310 and MPP-19857
		 */
		state->mkctxt.limitmask = 0;

		state->mkheap = mkheap_from_array(state->entries, state->entry_allocsize, state->entry_count, &state->mkctxt);
		state->entries = NULL;
		state->entry_allocsize = 0;
		state->entry_count = 0;
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
#ifdef PRINT_SPILL_AND_MEMORY_MESSAGES
	elog(INFO, "     build-run ready mem peak is now %ld", (long) MemoryContextGetPeakSpace(state->sortcontext));
#endif   /* // PRINT_SPILL_AND_MEMORY_MESSAGES */

}

/*
 * selectnewtape -- select new tape for new initial run.
 *
 * This is called after finishing a run when we know another run
 * must be started.  This implements steps D3, D4 of Algorithm D.
 */
static void
selectnewtape_mk(Tuplesortstate_mk *state)
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
mergeruns(Tuplesortstate_mk *state)
{
	int			tapenum,
				svTape,
				svRuns,
				svDummy;
	LogicalTape *lt = NULL;

	Assert(state->status == TSS_BUILDRUNS);


#ifdef FAULT_INJECTOR

	/*
	 * MPP-18288: We're injecting an interrupt here. We have to hold
	 * interrupts while we're injecting it to make sure the interrupt is not
	 * handled within the fault injector itself.
	 */
	HOLD_INTERRUPTS();
	FaultInjector_InjectFaultIfSet(
								   ExecSortMKSortMergeRuns,
								   DDLNotSpecified,
								   "", //databaseName
								   "");
	//tableName
		RESUME_INTERRUPTS();
#endif

	if (QueryFinishPending)
	{
		state->status = TSS_SORTEDONTAPE;
		return;
	}

	/*
	 * If we produced only one initial run (quite likely if the total data
	 * volume is between 1X and 2X workMem), we can just use that tape as the
	 * finished output, rather than doing a useless merge.  (This obvious
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

			/*
			 * We stop processing the input if we are asked to finish.
			 * Although we haven't finished on each tape yet, we will safely
			 * clean them up.  Make sure we don't get into this code path
			 * again.
			 */
			if (QueryFinishPending)
			{
				/* pretend we are done */
				state->status = TSS_SORTEDONTAPE;
				return;
			}

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
				mergeonerun_mk(state);
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
	 * output tape while rewinding it.  The last iteration of step D6 would be
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
void
mergeonerun_mk(Tuplesortstate_mk *state)
{
	int			destTape = state->tp_tapenum[state->tapeRange];
	MKEntry		e;

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

	Assert(state->mkheap);

	while (mkheap_putAndGet(state->mkheap, &e) >= 0)
		WRITETUP(state, lt, &e);

	/*
	 * When the heap empties, we're done.  Write an end-of-run marker on the
	 * output tape, and increment its count of real runs.
	 */
	markrunend(state, destTape);
	state->tp_runs[state->tapeRange]++;

	TRACE_POSTGRESQL_TUPLESORT_MERGEONERUN(state->activeTapes);
}

static bool
tupsort_preread(TupsortMergeReadCtxt * ctxt)
{
	uint32		tuplen;

	Assert(ctxt->mem_allowed > 0);

	if (!ctxt->pos.cur_work_tape || ctxt->pos.eof_reached)
		return false;

	ctxt->mem_used = 0;

	Assert(ctxt->p && ctxt->allocsz > 0);
	Assert(ctxt->mem_allowed > 0);

	for (ctxt->cnt = 0;
		 ctxt->cnt < ctxt->allocsz && ctxt->mem_used < ctxt->mem_allowed;
		 ++ctxt->cnt)
	{
		tuplen = getlen(ctxt->tsstate, &ctxt->pos, ctxt->pos.cur_work_tape, true);
		if (tuplen != 0)
		{
			MKEntry    *e = ctxt->p + ctxt->cnt;

			READTUP(ctxt->tsstate, &ctxt->pos, e, ctxt->pos.cur_work_tape, tuplen);
			ctxt->mem_used += tuplen;
		}
		else
		{
			ctxt->pos.eof_reached = true;
			break;
		}
	}

	ctxt->cur = 0;
	return ctxt->cnt > ctxt->cur;
}

static bool
tupsort_mergeread(void *pvctxt, MKEntry *e)
{
	TupsortMergeReadCtxt *ctxt = (TupsortMergeReadCtxt *) pvctxt;

	Assert(ctxt->mem_allowed > 0);

	if (ctxt->cur < ctxt->cnt)
	{
		*e = ctxt->p[ctxt->cur++];
		return true;
	}

	if (!tupsort_preread(ctxt))
		return false;

	Assert(ctxt->cur == 0 && ctxt->cnt > 0);
	*e = ctxt->p[ctxt->cur++];
	return true;
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
beginmerge(Tuplesortstate_mk *state)
{
	int			activeTapes;
	int			tapenum;
	int			srcTape;
	int			totalSlots;
	int			slotsPerTape;
	int64		spacePerTape;

	int			i;

	MemoryContext oldctxt;

	/* Heap should be empty here */
	Assert(mkheap_empty(state->mkheap));

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
	totalSlots = (state->mkheap == NULL) ? state->entry_allocsize : state->mkheap->alloc_size;
	slotsPerTape = (totalSlots - state->mergefirstfree) / activeTapes;
	slotsPerTape = Max(slotsPerTape, 128);
	spacePerTape = state->memAllowed / activeTapes;


	oldctxt = MemoryContextSwitchTo(state->sortcontext);
	if (state->mkheap)
	{
		mkheap_destroy(state->mkheap);
		state->mkheap = NULL;
	}

	if (state->mkhreader)
	{
		Assert(state->mkhreader_ctxt);
		for (i = 0; i < state->mkhreader_allocsize; ++i)
		{
			TupsortMergeReadCtxt *mkhr_ctxt = state->mkhreader_ctxt + i;

			AssertEquivalent(mkhr_ctxt->p != NULL, mkhr_ctxt->allocsz > 0);
			if (mkhr_ctxt->p)
				pfree(mkhr_ctxt->p);
		}

		pfree(state->mkhreader_ctxt);
		pfree(state->mkhreader);
	}

	state->mkhreader = palloc0(sizeof(MKHeapReader) * activeTapes);
	state->mkhreader_ctxt = palloc0(sizeof(TupsortMergeReadCtxt) * activeTapes);

	for (i = 0; i < activeTapes; ++i)
	{
		TupsortMergeReadCtxt *mkhr_ctxt = state->mkhreader_ctxt + i;

		state->mkhreader[i].reader = tupsort_mergeread;
		state->mkhreader[i].mkhr_ctxt = mkhr_ctxt;
		mkhr_ctxt->tsstate = state;
	}

	state->mkhreader_allocsize = activeTapes;

	for (i = 0, srcTape = 0; srcTape < state->maxTapes; srcTape++)
	{
		if (state->mergeactive[srcTape])
		{
			TupsortMergeReadCtxt *mkhr_ctxt = state->mkhreader_ctxt + i;

			mkhr_ctxt->pos.cur_work_tape = LogicalTapeSetGetTape(state->tapeset, srcTape);
			mkhr_ctxt->pos.eof_reached = false;
			mkhr_ctxt->mem_allowed = spacePerTape;
			Assert(mkhr_ctxt->mem_allowed > 0);
			mkhr_ctxt->mem_used = 0;
			mkhr_ctxt->cur = 0;
			mkhr_ctxt->cnt = 0;

			Assert(mkhr_ctxt->p == NULL);
			Assert(slotsPerTape > 0);
			mkhr_ctxt->p = (MKEntry *) palloc(sizeof(MKEntry) * slotsPerTape);
			mkhr_ctxt->allocsz = slotsPerTape;

			++i;
		}
	}

	Assert(i == activeTapes);

	state->mkheap = mkheap_from_reader(state->mkhreader, i, &state->mkctxt);
	Assert(state->mkheap);

	MemoryContextSwitchTo(oldctxt);
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
void
dumptuples_mk(Tuplesortstate_mk *state, bool alltuples)
{
	int			i;
	int			bDumped = 0;
	LogicalTape *lt = NULL;
	MKEntry		e;

	Assert(is_under_sort_or_exec_ctxt(state));

	if (alltuples && !state->mkheap)
	{
		/* Dump a sorted array.  Must be shared input that sends up here */

		/*
		 * ShareInput or sort: The sort may sort nothing, we still need to
		 * handle it here
		 */
		if (state->entry_count != 0)
		{
			lt = LogicalTapeSetGetTape(state->tapeset, state->tp_tapenum[state->destTape]);

			if (state->mkctxt.unique)
			{
				for (i = 0; i < state->entry_count; ++i)
					if (!mke_is_empty(&state->entries[i]))		/* can be empty because
																 * the qsort may have
																 * marked duplicates */
						WRITETUP(state, lt, &state->entries[i]);
			}
			else
			{
				for (i = 0; i < state->entry_count; ++i)
					WRITETUP(state, lt, &state->entries[i]);
			}
		}

		markrunend(state, state->tp_tapenum[state->destTape]);
		state->currentRun++;
		state->tp_runs[state->destTape]++;
		state->tp_dummy[state->destTape]--;		/* per Alg D step D2 */
		return;
	}

	/* OK.  Normal case */
	Assert(state->mkheap);
	while (1)
	{
		/*
		 * Done if we heap is empty, or pretend so if we are told to finish
		 * execution.
		 */
		if (mkheap_empty(state->mkheap) || QueryFinishPending)
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
		Assert(!mkheap_empty(state->mkheap));
		lt = LogicalTapeSetGetTape(state->tapeset, state->tp_tapenum[state->destTape]);

		mke_set_empty(&e);
		mke_set_run(&e, state->currentRun + 1);
		mkheap_putAndGet(state->mkheap, &e);

		Assert(!mke_is_empty(&e));

		WRITETUP(state, lt, &e);

		/*
		 * If the heap is empty *or* top run number has changed, we've
		 * finished the current run.
		 */
		if (mkheap_empty(state->mkheap) || !mkheap_run_match(state->mkheap, state->currentRun))
		{
#ifdef USE_ASSERT_CHECKING
			mkheap_verify_heap(state->mkheap, 0);
#endif
			markrunend(state, state->tp_tapenum[state->destTape]);
			state->currentRun++;
			state->tp_runs[state->destTape]++;
			state->tp_dummy[state->destTape]--; /* per Alg D step D2 */

			TRACE_POSTGRESQL_TUPLESORT_DUMPTUPLES(state->entry_count, state->currentRun, state->destTape);

			/*
			 * Done if heap is empty, else prepare for new run.
			 */
			if (mkheap_empty(state->mkheap))
				break;

			Assert(mkheap_run_match(state->mkheap, state->currentRun));
			selectnewtape_mk(state);
		}
	}

	if (state->gpmon_pkt)
		tuplesort_checksend_gpmonpkt(state->gpmon_pkt, state->gpmon_sort_tick);
}

/*
 * Put pos at the begining of the tuplesort.  Create pos->work_tape if necessary
 */
void
tuplesort_rescan_pos_mk(Tuplesortstate_mk *state, TuplesortPos_mk *pos)
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
			if (NULL == state->result_tape)
			{
				Assert(pos->eof_reached);
				pos->markpos.tapepos.blkNum = 0L;
				pos->markpos.tapepos.offset = 0;
				pos->markpos_eof = true;

				break;
			}

			if (pos == &state->pos)
			{
				Assert(pos->cur_work_tape == NULL);
				LogicalTapeRewind(state->tapeset, state->result_tape, false);
			}
			else
			{
				if (pos->cur_work_tape == NULL)
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
 * tuplesort_rescan		- rewind and replay the scan
 */

void
tuplesort_rescan_mk(Tuplesortstate_mk *state)
{
	tuplesort_rescan_pos_mk(state, &state->pos);
}

/*
 * tuplesort_markpos	- saves current position in the merged sort file
 */
void
tuplesort_markpos_pos_mk(Tuplesortstate_mk *state, TuplesortPos_mk *pos)
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

void
tuplesort_markpos_mk(Tuplesortstate_mk *state)
{
	tuplesort_markpos_pos_mk(state, &state->pos);
}

/*
 * tuplesort_restorepos - restores current position in merged sort file to
 *						  last saved position
 */
void
tuplesort_restorepos_pos_mk(Tuplesortstate_mk *state, TuplesortPos_mk *pos)
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
				bool		fSeekOK = LogicalTapeSeek(state->tapeset, work_tape, &pos->markpos.tapepos);

				if (!fSeekOK)
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

void
tuplesort_restorepos_mk(Tuplesortstate_mk *state)
{
	tuplesort_restorepos_pos_mk(state, &state->pos);
}


/*
 * tuplesort_get_stats_mk - extract summary statistics
 *
 * This can be called after tuplesort_performsort_mk() finishes to obtain
 * printable summary information about how the sort was performed.
 * spaceUsed is measured in kilobytes.
 */
static void
tuplesort_get_stats_mk(Tuplesortstate_mk* state,
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
		*spaceUsed = (MemoryContextGetCurrentSpace(state->sortcontext) + 1024) / 1024;
	}

	switch (state->status)
	{
	case TSS_SORTEDINMEM:
		if (state->mkctxt.boundUsed)
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
	}
	return;
}

/*
 * Tape interface routines
 */
static uint32
getlen(Tuplesortstate_mk *state, TuplesortPos_mk *pos, LogicalTape *lt, bool eofOK)
{
	uint32		len;
	size_t		readSize;

	Assert(lt);
	readSize = LogicalTapeRead(state->tapeset, lt, (void *) &len, sizeof(len));

	if (readSize != sizeof(len))
	{
		Assert(!"Catch me");
		elog(ERROR, "unexpected end of tape");
	}

	if (len == 0 && !eofOK)
		elog(ERROR, "unexpected end of data");
	return len;
}

static void
markrunend(Tuplesortstate_mk *state, int tapenum)
{
	uint32		len = 0;
	LogicalTape *lt = LogicalTapeSetGetTape(state->tapeset, tapenum);

	LogicalTapeWrite(state->tapeset, lt, (void *) &len, sizeof(len));
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

static void
copytup_heap(Tuplesortstate_mk *state, MKEntry *e, void *tup)
{
	/*
	 * We expect the passed "tup" to be a TupleTableSlot, and form a
	 * MinimalTuple using the exported interface for that.
	 */
	TupleTableSlot *slot = (TupleTableSlot *) tup;

	slot_getallattrs(slot);
	e->ptr = (void *) memtuple_form_to(state->mt_bind,
									   slot_get_values(slot),
									   slot_get_isnull(slot),
									   NULL, NULL, false
		);

	state->totalTupleBytes += memtuple_get_size((MemTuple) e->ptr);

	Assert(state->mt_bind);
}

/*
 * Since MinimalTuple already has length in its first word, we don't need
 * to write that separately.
 */
static long
writetup_heap(Tuplesortstate_mk *state, LogicalTape *lt, MKEntry *e)
{
	uint32		tuplen = memtuple_get_size(e->ptr);
	long		ret = tuplen;

	LogicalTapeWrite(state->tapeset, lt, e->ptr, tuplen);

	if (state->randomAccess)	/* need trailing length word? */
	{
		LogicalTapeWrite(state->tapeset, lt, (void *) &tuplen, sizeof(tuplen));
		ret += sizeof(tuplen);
	}

	pfree(e->ptr);
	e->ptr = NULL;

	return ret;
}

static void
freetup_heap(MKEntry *e)
{
	pfree(e->ptr);
	e->ptr = NULL;
}

static void
readtup_heap(Tuplesortstate_mk *state, TuplesortPos_mk *pos, MKEntry *e, LogicalTape *lt, uint32 len)
{
	uint32		tuplen;
	size_t		readSize;

	Assert(is_under_sort_or_exec_ctxt(state));

	MemSet(e, 0, sizeof(MKEntry));
	e->ptr = palloc(memtuple_size_from_uint32(len));

	memtuple_set_mtlen((MemTuple) e->ptr, len);

	Assert(lt);
	readSize = LogicalTapeRead(state->tapeset, lt,
							   (void *) ((char *) e->ptr + sizeof(uint32)),
							memtuple_size_from_uint32(len) - sizeof(uint32));

	if (readSize != (size_t) (memtuple_size_from_uint32(len) - sizeof(uint32)))
		elog(ERROR, "unexpected end of data");

	if (state->randomAccess)	/* need trailing length word? */
	{
		readSize = LogicalTapeRead(state->tapeset, lt, (void *) &tuplen, sizeof(tuplen));

		if (readSize != sizeof(tuplen))
			elog(ERROR, "unexpected end of data");
	}

	/*
	 * For shareinput on sort, the reader will not set mt_bind.  In this case,
	 * we will not call compare.
	 */
	AssertImply(!state->mt_bind, state->status == TSS_SORTEDONTAPE);
}

static void
copytup_index(Tuplesortstate_mk *state, MKEntry *e, void *tup)
{
	IndexTuple	tuple = (IndexTuple) tup;
	uint32		tuplen = IndexTupleSize(tuple);

	/* copy the tuple into sort storage */
	e->ptr = palloc(tuplen);
	memcpy(e->ptr, tuple, tuplen);

	state->totalTupleBytes += tuplen;
}

static long
writetup_index(Tuplesortstate_mk *state, LogicalTape *lt, MKEntry *e)
{
	IndexTuple	tuple = (IndexTuple) e->ptr;
	uint32		tuplen = IndexTupleSize(tuple) + sizeof(tuplen);
	long		ret = tuplen;

	LogicalTapeWrite(state->tapeset, lt, (void *) &tuplen, sizeof(tuplen));
	LogicalTapeWrite(state->tapeset, lt, (void *) tuple, IndexTupleSize(tuple));

	if (state->randomAccess)	/* need trailing length word? */
	{
		LogicalTapeWrite(state->tapeset, lt, (void *) &tuplen, sizeof(tuplen));
		ret += sizeof(tuplen);
	}

	pfree(tuple);
	e->ptr = NULL;
	return ret;
}

static void
freetup_index(MKEntry *e)
{
	pfree(e->ptr);
	e->ptr = NULL;
}

static void
readtup_index(Tuplesortstate_mk *state, TuplesortPos_mk *pos, MKEntry *e,
			  LogicalTape *lt, uint32 len)
{
	uint32		tuplen = len - sizeof(uint32);
	IndexTuple	tuple = (IndexTuple) palloc(tuplen);
	size_t		readSize;

	Assert(lt);

	MemSet(e, 0, sizeof(MKEntry));
	readSize = LogicalTapeRead(state->tapeset, lt, (void *) tuple, tuplen);

	if (readSize != tuplen)
		elog(ERROR, "unexpected end of data");

	if (state->randomAccess)	/* need trailing length word? */
	{
		readSize = LogicalTapeRead(state->tapeset, lt, (void *) &tuplen, sizeof(tuplen));

		if (readSize != sizeof(tuplen))
			elog(ERROR, "unexpected end of data");
	}
	e->ptr = (void *) tuple;
}


static void
copytup_datum(Tuplesortstate_mk *state, MKEntry *e, void *tup)
{
	/* Not currently needed */
	elog(ERROR, "copytup_datum() should not be called");
}

static long
writetup_datum(Tuplesortstate_mk *state, LogicalTape *lt, MKEntry *e)
{
	void	   *waddr;
	uint32		tuplen;
	uint32		writtenlen;
	bool		needfree = false;
	long		ret;

	if (mke_is_null(e))
	{
		waddr = NULL;
		tuplen = 0;
	}
	else if (state->datumTypeByVal)
	{
		waddr = &e->d;
		tuplen = sizeof(Datum);
	}
	else
	{
		waddr = DatumGetPointer(e->d);
		tuplen = datumGetSize(e->d, false, state->datumTypeLen);
		needfree = true;
		Assert(tuplen != 0);
	}

	writtenlen = tuplen + sizeof(uint32);
	ret = writtenlen;

	LogicalTapeWrite(state->tapeset, lt, (void *) &writtenlen, sizeof(writtenlen));
	LogicalTapeWrite(state->tapeset, lt, waddr, tuplen);

	if (state->randomAccess)	/* need trailing length word? */
	{
		LogicalTapeWrite(state->tapeset, lt, (void *) &writtenlen, sizeof(writtenlen));
		ret += sizeof(writtenlen);
	}

	if (needfree)
		pfree(waddr);

	return ret;
}

/**
 * No-op free: does nothing!
 */
static void
freetup_noop(MKEntry *e)
{
}

/**
 * Free the datum if non-null, should only be used for by-reference datums
 */
static void
freetup_datum(MKEntry *e)
{
	if (!mke_is_null(e))
	{
		void	   *waddr = DatumGetPointer(e->d);

		pfree(waddr);
	}
}


static void
readtup_datum(Tuplesortstate_mk *state, TuplesortPos_mk *pos, MKEntry *e,
			  LogicalTape *lt, uint32 len)
{
	size_t		readSize;
	uint32		tuplen = len - sizeof(uint32);

	Assert(lt);

	MemSet(e, 0, sizeof(MKEntry));

	if (tuplen == 0)
		mke_set_null(e, state->nullfirst);
	else if (state->datumTypeByVal)
	{
		mke_set_not_null(e);

		Assert(tuplen == sizeof(Datum));
		readSize = LogicalTapeRead(state->tapeset, lt, (void *) &e->d, tuplen);

		if (readSize != tuplen)
			elog(ERROR, "unexpected end of data");
	}
	else
	{
		void	   *raddr = palloc(tuplen);

		readSize = LogicalTapeRead(state->tapeset, lt, raddr, tuplen);

		if (readSize != tuplen)
			elog(ERROR, "unexpected end of data");

		mke_set_not_null(e);
		e->d = PointerGetDatum(raddr);
	}

	if (state->randomAccess)	/* need trailing length word? */
	{
		readSize = LogicalTapeRead(state->tapeset, lt, (void *) &tuplen, sizeof(tuplen));

		if (readSize != sizeof(tuplen))
			elog(ERROR, "unexpected end of data");
	}
}

typedef struct refcnt_locale_str
{
	int			ref;
	short		xfrm_pos;
	char		isPrefixOnly;
	char		data[1];
} refcnt_locale_str;

typedef struct refcnt_locale_str_k
{
	int			ref;
	short		xfrm_pos;
	char		isPrefixOnly;
	char		data[16000];
} refcnt_locale_str_k;

/**
 * Compare the datums for the given level.  It is assumed that each entry has been prepared
 *	 for the given level.
 */
int
tupsort_compare_datum(MKEntry *v1, MKEntry *v2, MKLvContext *lvctxt, MKContext *context)
{
	Assert(!mke_is_null(v1));
	Assert(!mke_is_null(v2));

	switch (lvctxt->lvtype)
	{
		case MKLV_TYPE_NONE:
			return inlineApplySortFunction(&lvctxt->scanKey.sk_func, lvctxt->scanKey.sk_flags,
										   v1->d, false,
										   v2->d, false
				);
		case MKLV_TYPE_INT32:
			{
				int32		i1 = DatumGetInt32(v1->d);
				int32		i2 = DatumGetInt32(v2->d);
				int			result = (i1 < i2) ? -1 : ((i1 == i2) ? 0 : 1);

				return ((lvctxt->scanKey.sk_flags & SK_BT_DESC) != 0) ? -result : result;
			}
		default:
			return tupsort_compare_char(v1, v2, lvctxt, context);
	}

	Assert(!"Never reach here");
	return 0;
}

void
tupsort_cpfr(MKEntry *dst, MKEntry *src, MKLvContext *lvctxt)
{
	Assert(dst);
	if (mke_is_refc(dst))
	{
		Assert(dst->d != 0);
		Assert(!mke_is_copied(dst));
		tupsort_refcnt(DatumGetPointer(dst->d), -1);
	}

	if (mke_is_copied(dst))
	{
		Assert(dst->d != 0);
		Assert(!mke_is_refc(dst));
		pfree(DatumGetPointer(dst->d));
	}

	mke_clear_refc_copied(dst);

	if (src)
	{
		*dst = *src;
		if (!mke_is_null(src))
		{
			if (mke_is_refc(src))
				tupsort_refcnt(DatumGetPointer(dst->d), 1);
			else if (!lvctxt->typByVal)
			{
				Assert(src->d != 0);
				dst->d = datumCopy(src->d, lvctxt->typByVal, lvctxt->typLen);
				mke_set_copied(dst);
			}
		}
	}
}

static void
tupsort_refcnt(void *vp, int ref)
{
	refcnt_locale_str *p = (refcnt_locale_str *) vp;

	Assert(p && p->ref > 0);
	Assert(ref == 1 || ref == -1);

	if (ref == 1)
		++p->ref;
	else
	{
		if (--p->ref == 0)
			pfree(p);
	}
}

static int
tupsort_compare_char(MKEntry *v1, MKEntry *v2, MKLvContext *lvctxt, MKContext *mkContext)
{
	int			result = 0;

	refcnt_locale_str *p1 = (refcnt_locale_str *) DatumGetPointer(v1->d);
	refcnt_locale_str *p2 = (refcnt_locale_str *) DatumGetPointer(v2->d);

	Assert(!mke_is_null(v1));
	Assert(!mke_is_null(v2));

	Assert(!lc_collate_is_c());
	Assert(mkContext->fetchForPrep);

	Assert(p1->ref > 0 && p2->ref > 0);

	if (p1 == p2)
	{
		Assert(p1->ref >= 2);
		result = 0;
	}
	else
	{
		result = strcmp(p1->data + p1->xfrm_pos, p2->data + p2->xfrm_pos);

		if (result == 0)
		{
			if (p1->isPrefixOnly || p2->isPrefixOnly)
			{
				/*
				 * only prefixes were equal so we must compare more of the
				 * strings
				 *
				 * do this by getting the true datum and calling the built-in
				 * comparison function
				 */
				Datum		p1Original,
							p2Original;
				bool		p1IsNull,
							p2IsNull;

				p1Original = (mkContext->fetchForPrep) (v1, mkContext, lvctxt, &p1IsNull);
				p2Original = (mkContext->fetchForPrep) (v2, mkContext, lvctxt, &p2IsNull);

				Assert(!p1IsNull);		/* should not have been prepared if
										 * null */
				Assert(!p2IsNull);

				result = inlineApplySortFunction(&lvctxt->scanKey.sk_func, lvctxt->scanKey.sk_flags,
									   p1Original, false, p2Original, false);
			}
			else
			{
				/*
				 * See varstr_cmp for comment on comparing str with locale.
				 * Essentially, for some locale, strcoll may return eq even if
				 * original str are different.
				 */
				result = strcmp(p1->data, p2->data);
			}
		}

		/* The values were equal -- de-dupe them */
		if (result == 0)
		{
			if (p1->ref >= p2->ref)
			{
				v2->d = v1->d;
				tupsort_refcnt(p1, 1);
				tupsort_refcnt(p2, -1);
			}
			else
			{
				v1->d = v2->d;
				tupsort_refcnt(p2, 1);
				tupsort_refcnt(p1, -1);
			}
		}
	}

	return ((lvctxt->scanKey.sk_flags & SK_BT_DESC) != 0) ? -result : result;
}

static int32
estimateMaxPrepareSizeForEntry(MKEntry *a, struct MKContext *mkContext)
{
	int			result = 0;
	int			lv;

	Assert(mkContext->fetchForPrep != NULL);

	for (lv = 0; lv < mkContext->total_lv; lv++)
	{
		MKLvContext *level = mkContext->lvctxt + lv;
		MKLvType	levelType = level->lvtype;

		if (levelType == MKLV_TYPE_CHAR ||
			levelType == MKLV_TYPE_TEXT)
		{
			bool		isnull;
			Datum		d = (mkContext->fetchForPrep) (a, mkContext, level, &isnull);

			if (!isnull)
			{
				int			amountThisDatum = estimatePrepareSpaceForChar(mkContext, a, d, levelType == MKLV_TYPE_CHAR);

				if (amountThisDatum > result)
					result = amountThisDatum;
			}
		}
	}

	return result;
}

static Datum
tupsort_fetch_datum_mtup(MKEntry *a, MKContext *mkctxt, MKLvContext *lvctxt, bool *isNullOut)
{
	Datum		d = memtuple_getattr(
									 (MemTuple) a->ptr,
									 mkctxt->mt_bind,
									 lvctxt->attno,
									 isNullOut
	);

	return d;
}

static Datum
tupsort_fetch_datum_itup(MKEntry *a, MKContext *mkctxt, MKLvContext *lvctxt, bool *isNullOut)
{
	Datum		d = index_getattr(
								  (IndexTuple) a->ptr,
								  lvctxt->attno,
								  mkctxt->tupdesc,
								  isNullOut
	);

	return d;
}

void
tupsort_prepare(MKEntry *a, MKContext *mkctxt, int lv)
{
	MKLvContext *lvctxt = mkctxt->lvctxt + lv;
	bool		isnull;

	Assert(mkctxt->fetchForPrep != NULL);

	tupsort_cpfr(a, NULL, lvctxt);

	a->d = (mkctxt->fetchForPrep) (a, mkctxt, lvctxt, &isnull);

	if (!isnull)
		mke_set_not_null(a);
	else
		mke_set_null(a, (lvctxt->scanKey.sk_flags & SK_BT_NULLS_FIRST) != 0);

	if (lvctxt->lvtype == MKLV_TYPE_CHAR)
		tupsort_prepare_char(a, true);
	else if (lvctxt->lvtype == MKLV_TYPE_TEXT)
		tupsort_prepare_char(a, false);
}

/* "True" length (not counting trailing blanks) of a BpChar */
static inline int
bcTruelen(char *p, int len)
{
	int			i;

	for (i = len - 1; i >= 0; i--)
	{
		if (p[i] != ' ')
			break;
	}
	return i + 1;
}

/**
 * should only be called for non-null Datum (caller must check the isnull flag from the fetch)
 */
static int32
estimatePrepareSpaceForChar(struct MKContext *mkContext, MKEntry *e, Datum d, bool isCHAR)
{
	int			len,
				transformedLength,
				retlen;

	if (isCHAR)
	{
		char	   *p;
		void	   *toFree;

		varattrib_untoast_ptr_len(d, &p, &len, &toFree);
		len = bcTruelen(p, len);

		if (toFree)
			pfree(toFree);
	}
	else
	{
		/*
		 * since we don't need the data for checking the true length, just
		 * unpack enough to get the length this may avoid decompression
		 */
		len = varattrib_untoast_len(d);
	}

	/* figure out how much space transformed version will take */
	transformedLength = len * mkContext->strxfrmScaleFactor + mkContext->strxfrmConstantFactor;
	if (len > STRXFRM_INPUT_LENGTH_LIMIT)
	{
		if (transformedLength > STRXFRM_INPUT_LENGTH_LIMIT)
			transformedLength = STRXFRM_INPUT_LENGTH_LIMIT;

		/*
		 * we do not store the raw data for long input strings (in part
		 * because of compression producing a long input string from a much
		 * smaller datum)
		 */
		len = 0;
	}

	retlen = offsetof(refcnt_locale_str, data) + len + 1 + transformedLength + 1;
	return retlen;
}

/**
 * Prepare a character string by copying the datum out to a null-terminated string and
 *	 then also keeping a strxfrmed copy of it.
 *
 * If the string is long then the copied out value is not kept and only the prefix of the strxfrm
 *	 is kept.
 *
 * Note that this must be in sync with the estimation function.
 */
static void
tupsort_prepare_char(MKEntry *a, bool isCHAR)
{
	char	   *p;
	void	   *tofree = NULL;
	int			len;
	int			lenToStore;
	int			transformedLenToStore;
	int			retlen;

	refcnt_locale_str *ret;
	refcnt_locale_str_k kstr;
	int			avail = sizeof(kstr.data);
	bool		storePrefixOnly;

	Assert(!lc_collate_is_c());

	if (mke_is_null(a))
		return;

	varattrib_untoast_ptr_len(a->d, &p, &len, &tofree);

	if (isCHAR)
		len = bcTruelen(p, len);

	if (len > STRXFRM_INPUT_LENGTH_LIMIT)
	{
		/*
		 * too long?  store prefix of strxfrm only and DON'T store copy of
		 * original string
		 */
		storePrefixOnly = true;
		lenToStore = 0;
	}
	else
	{
		storePrefixOnly = false;
		lenToStore = len;
	}

	/*
	 * This assertion is true because avail is larger than
	 * STRXFRM_INPUT_LENGTH_LIMIT, which is the max of lenToStore ...
	 */
	Assert(lenToStore <= avail - 2);
	Assert(lenToStore < 32768); /* so it will fit in a short (xfrm_pos field) */
	Assert(STRXFRM_INPUT_LENGTH_LIMIT < 32768); /* so it will fit in a short
												 * (xfrm_pos field) */

	kstr.ref = 1;
	kstr.xfrm_pos = lenToStore + 1;
	kstr.isPrefixOnly = storePrefixOnly ? 1 : 0;
	memcpy(kstr.data, p, lenToStore);
	kstr.data[lenToStore] = '\0';

	avail -= lenToStore + 1;
	Assert(avail > 0);

	/*
	 * String transformation.
	 */
	if (storePrefixOnly)
	{
		/*
		 * prefix only: we haven't copied from p into a null-terminated string
		 * so do that now
		 */
		char	   *possibleStr = kstr.data + kstr.xfrm_pos + STRXFRM_INPUT_LENGTH_LIMIT + 2;
		char	   *str;

		if (avail >= STRXFRM_INPUT_LENGTH_LIMIT + 1 + len + 1)
		{
			/*
			 * will segment this so first part is xformed string and next is
			 * the throw-away string to be passed to strxfrm
			 */
			str = possibleStr;
		}
		else
		{
			str = palloc(len + 1);
		}
		memcpy(str, p, len);
		str[len] = '\0';

		/*
		 * transform, but limit length of transformed string
		 */
		Assert(avail >= STRXFRM_INPUT_LENGTH_LIMIT + 1);
		transformedLenToStore = (int) gp_strxfrm(kstr.data + kstr.xfrm_pos, str, STRXFRM_INPUT_LENGTH_LIMIT + 1);

		if (transformedLenToStore > STRXFRM_INPUT_LENGTH_LIMIT)
		{
			transformedLenToStore = STRXFRM_INPUT_LENGTH_LIMIT;

			/*
			 * this is required for linux -- when there is not enough room
			 * then linux does NOT write the \0
			 */
			kstr.data[kstr.xfrm_pos + transformedLenToStore] = '\0';
		}

		if (str != possibleStr)
			pfree(str);
	}
	else
		transformedLenToStore = (int) gp_strxfrm(kstr.data + kstr.xfrm_pos, kstr.data, avail);

	/*
	 * Copy or transform into the result as needed
	 */
	retlen = offsetof(refcnt_locale_str, data) + lenToStore + 1 + transformedLenToStore + 1;

	ret = (refcnt_locale_str *) palloc(retlen);

	if (transformedLenToStore < avail)
	{
		memcpy(ret, &kstr, retlen);
		Assert(ret->ref == 1);
		Assert(ret->data[ret->xfrm_pos - 1] == '\0');
		Assert(ret->data[ret->xfrm_pos + transformedLenToStore] == '\0');
	}
	else
	{
		/*
		 * note that when avail (determined by refcnt_locale_str_k.data) is
		 * much larger than STRXFRM_INPUT_LENGTH_LIMIT then this code won't be
		 * hit.
		 */
		memcpy(ret, &kstr, offsetof(refcnt_locale_str, data) + lenToStore + 1);
		avail = retlen - offsetof(refcnt_locale_str, data) - lenToStore - 1;
		Assert(avail > transformedLenToStore);
		Assert(ret->ref == 1);
		Assert(ret->xfrm_pos == len + 1);
		Assert(ret->data[len] == '\0');

		transformedLenToStore = (int) gp_strxfrm(ret->data + ret->xfrm_pos, kstr.data, avail);
		Assert(transformedLenToStore < avail);
		Assert(ret->data[ret->xfrm_pos + transformedLenToStore] == '\0');
	}

	if (tofree)
		pfree(tofree);

	/*
	 * data string is length zero in this case (could just not even store it
	 * and have xfrm_pos == 0 but that complicates code more)
	 */
	AssertImply(storePrefixOnly, ret->data[0] == '\0');

	/*
	 * finalize result
	 */
	a->d = PointerGetDatum(ret);
	mke_set_refc(a);
}

/*
 * tuplesort_inmem_limit_insert
 *	 Adds a tuple for sorting when we are doing LIMIT sort and we (still) fit in memory
 *	 If we're in here, it means we haven't exceeded memory yet.
 * Three cases:
 *	 - we're below LIMIT tuples. Then add to unsorted array, increase memory
 *	 - we just hit LIMIT tuples. Switch to using a heap instead of array, so
 *	   we only keep top LIMIT tuples.
 *	 - we're above LIMIT tuples. Then add to heap, don't increase memory usage
 */
static void
tuplesort_inmem_limit_insert(Tuplesortstate_mk *state, MKEntry *entry)
{
	Assert(state->mkctxt.bounded);
	Assert(is_under_sort_or_exec_ctxt(state));
	Assert(!mke_is_empty(entry));

	Assert(state->status == TSS_INITIAL);

	if (state->mkheap == NULL)
	{
		Assert(state->entry_count < state->entry_allocsize);

		/*
		 * We haven't created the heap yet. First add element to unsorted
		 * array so we don't lose it. Then, we have two cases: A. if we're
		 * under limit, we're done, iterate. B. We just hit the limit. Create
		 * heap. Add to heap.
		 */

		state->entries[state->entry_count++] = *entry;
		state->numTuplesInMem++;

		if (state->entry_count == state->mkctxt.bound)
		{
			/* B: just hit limit. Create heap from array */
			state->mkheap = mkheap_from_array(state->entries,
											  state->entry_count,
											  state->entry_count,
											  &state->mkctxt);
			state->entries = NULL;
			state->entry_allocsize = 0;
			state->entry_count = 0;
		}
	}
	else
	{
		/*
		 * We already have the heap (and it fits in memory). Simply add to the
		 * heap, keeping only top LIMIT elements.
		 */
		Assert(state->mkheap && !mkheap_empty(state->mkheap));
		Assert(state->mkheap->count == state->numTuplesInMem);
		Assert(state->entry_count == 0);
		(void) mkheap_putAndGet(state->mkheap, entry);

		Assert(!mke_is_empty(entry));
		Assert(entry->ptr);
		pfree(entry->ptr);
		entry->ptr = NULL;
	}
}

/*
 * tuplesort_inmem_nolimit_insert
 *	 Adds a tuple for sorting when we are regular (no LIMIT) sort and we (still) fit in memory
 *
 *	 Simply add to the unsorted in-memory array. We'll either qsort or spill this later.
 */
static void
tuplesort_inmem_nolimit_insert(Tuplesortstate_mk *state, MKEntry *entry)
{
	Assert(state->status == TSS_INITIAL);
	Assert(!state->mkctxt.bounded);
	Assert(is_under_sort_or_exec_ctxt(state));
	Assert(!mke_is_empty(entry));
	Assert(state->entry_count < state->entry_allocsize);

	state->entries[state->entry_count] = *entry;
	if (state->mkctxt.fetchForPrep)
	{
		state->mkctxt.estimatedExtraForPrep += estimateMaxPrepareSizeForEntry(&state->entries[state->entry_count],
															 &state->mkctxt);
	}
	state->entry_count++;
	state->numTuplesInMem++;

}

static void
tuplesort_heap_insert(Tuplesortstate_mk *state, MKEntry *e)
{
	int			ins;

	Assert(state->mkheap);
	Assert(is_under_sort_or_exec_ctxt(state));

	ins = mkheap_putAndGet_run(state->mkheap, e, state->currentRun);
	tupsort_cpfr(e, NULL, &state->mkctxt.lvctxt[mke_get_lv(e)]);

	Assert(ins >= 0);

	{
		LogicalTape *lt = LogicalTapeSetGetTape(state->tapeset, state->tp_tapenum[state->destTape]);

		WRITETUP(state, lt, e);

		if (!mkheap_run_match(state->mkheap, state->currentRun))
		{
			markrunend(state, state->tp_tapenum[state->destTape]);
			state->currentRun++;
			state->tp_runs[state->destTape]++;
			state->tp_dummy[state->destTape]--;

			selectnewtape_mk(state);
		}
	}
}

static void
tuplesort_limit_sort(Tuplesortstate_mk *state)
{
	Assert(state->mkctxt.bounded);
	Assert(is_under_sort_or_exec_ctxt(state));

	if (!state->mkheap)
	{
		Assert(state->entry_count <= state->mkctxt.bound);
		mk_qsort(state->entries, state->entry_count, &state->mkctxt);
		return;
	}
	else
	{
		int			i;

#ifdef USE_ASSERT_CHECKING
		mkheap_verify_heap(state->mkheap, 0);
#endif
		state->entry_allocsize = mkheap_cnt(state->mkheap);
		state->entries = palloc(state->entry_allocsize * sizeof(MKEntry));

		/* Put these things to minimum */
		for (i = 0; i < state->entry_allocsize; ++i)
			mke_set_comp_max(state->entries + i);

		for (i = state->entry_allocsize - 1; i >= 0; --i)
		{
			if (QueryFinishPending)
				break;
			mkheap_putAndGet(state->mkheap, state->entries + i);
		}

		Assert(QueryFinishPending || (mkheap_cnt(state->mkheap) == 0));
		state->entry_count = state->entry_allocsize;
	}

	mkheap_destroy(state->mkheap);
	state->mkheap = NULL;
}

void
tuplesort_set_gpmon_mk(Tuplesortstate_mk *state, gpmon_packet_t *gpmon_pkt, int *gpmon_tick)
{
	state->gpmon_pkt = gpmon_pkt;
	state->gpmon_sort_tick = gpmon_tick;
}

/* EOF */
