/*-------------------------------------------------------------------------
 *
 * tuplesort_mk_details.h 
 *	  Tuplesort Multi Key.
 *
 * Portions Copyright (c) Greenplum Inc. 2008.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/tuplesort_mk_details.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef TUPLESORT_MK_DETAILS_H
#define TUPLESORT_MK_DETAILS_H

/* mk_heap: multi level key heap */
/* mk_qsort: multi level key quick sort */

/* flags, involved in cmp */
#define MKE_CF_NullFirst       1                      /* Null is smaller than non null */ 
#define MKE_CF_NotNull         2
#define MKE_CF_NullLast        3                      /* Null is bigger than non null  */
#define MKE_CF_NULLBITS        3                      

#define MKE_CF_Lv              0xFFFC                 /* Runs use 14 bits             */
#define MKE_CF_Run             0x3FFF0000             /* Lv use 14 bits               */
#define MKE_CF_Empty           0x40000000             /* Empty use 1 bit              */
#define MKE_CF_MAX             0x7FFFFFFF             /* max value. */

#define MKE_MAX_LV    0x3FFF
#define MKE_MAX_RUN   0x3FFF
#define MKE_RunShift  16 
#define MKE_LvShift   2 
/* Top bit is always unused */

/* flags, not comp */ 
#define MKE_F_RefCnt          1
#define MKE_F_Copied          2 
#define MKE_F_Reader          0xFFFC               
#define MKE_MAX_READER        0x3FFF
#define MKE_ReaderShift       2 

typedef struct MKEntry 
{
    int32 compflags;
    int32 flags;

    /**
     *  The datum for this key
     */
    Datum d;

    /**
     * Ptr to the tuple that contains this entry's key.  Is a void * to provide polymorphism: it could be a memtuple, heaptuple, or really anything that has multi-key behavior!
     *   Deciphering of this field is done by the functions that are passed when the multi-key heap is prepared
     */
    void *ptr;
} MKEntry;

/**
 * Set flags and compflags to their initial values
 */
static inline void mke_blank(MKEntry *e)
{
    e->compflags = (MKE_MAX_LV << MKE_LvShift);
    e->flags = 0;
}

static inline void i_clear_flag(int32 *p, int32 flag)
{
    *p &= ~flag;
}
static inline bool i_test_flag(int32 *p, int32 flag)
{
    return (*p & flag) != 0;
}
static inline void i_set_flag(int32 *p, int32 flag)
{
    *p |= flag;
}
static inline int32 i_get_flag(int32 *p, int32 flag, int32 shiftsz)
{
    return ((*p & flag) >> shiftsz);
}

/* Getter, Setter */
static inline void mke_set_null(MKEntry *e, bool nullfirst)
{
    i_clear_flag(&e->compflags, MKE_CF_NULLBITS);
    if (nullfirst)
        i_set_flag(&e->compflags, MKE_CF_NullFirst);
    else
        i_set_flag(&e->compflags, MKE_CF_NullLast);
}
static inline void mke_set_not_null(MKEntry *e)
{
    i_clear_flag(&e->compflags, MKE_CF_NULLBITS);
    i_set_flag(&e->compflags, MKE_CF_NotNull);
}
static inline bool mke_is_null(MKEntry *e)
{
    return (e->compflags & MKE_CF_NULLBITS) != MKE_CF_NotNull;
}
static inline int32 mke_get_nullbits(MKEntry *e)
{
    return (e->compflags & MKE_CF_NULLBITS);
}

static inline int32 mke_get_run(MKEntry *e)
{
    return i_get_flag(&e->compflags, MKE_CF_Run, MKE_RunShift);
}
static inline void mke_set_run(MKEntry *e, int32 run)
{
    Assert(run >= 0 && run <= MKE_MAX_RUN);
    /* If statement_mem is too small, we might end up generating 
       too many runs. */ 
    if (run > MKE_MAX_RUN)
    {
    	elog(ERROR, ERRMSG_GP_INSUFFICIENT_STATEMENT_MEMORY);
    } 
    i_clear_flag(&e->compflags, MKE_CF_Run);
    i_set_flag(&e->compflags, run << MKE_RunShift);
}

static inline int32 mke_get_lv(MKEntry *e)
{
    return (MKE_MAX_LV - i_get_flag(&e->compflags, MKE_CF_Lv, MKE_LvShift));
}
static inline void mke_set_lv(MKEntry *e, int32 lv)
{
    Assert(lv >= 0 && lv <= MKE_MAX_LV);
    /* If statement_mem is too small, we might end up generating 
       too many runs. */ 
    if (lv > MKE_MAX_LV) 
    {
		elog(ERROR, ERRMSG_GP_INSUFFICIENT_STATEMENT_MEMORY);
    }
    i_clear_flag(&e->compflags, MKE_CF_Lv);
    i_set_flag(&e->compflags, (MKE_MAX_LV - lv) << MKE_LvShift);
}

static inline void mke_set_empty(MKEntry *e)
{
    e->compflags = MKE_CF_Empty; 
    e->flags = 0;
	e->d = 0;
	e->ptr = 0;
}
static inline bool mke_is_empty(MKEntry *e)
{
    return i_test_flag(&e->compflags, MKE_CF_Empty); 
}

static inline void mke_set_comp_max(MKEntry *e)
{
    e->compflags = MKE_CF_MAX;
}

static inline void mke_set_refc(MKEntry *e)
{
    i_set_flag(&e->flags, MKE_F_RefCnt);
}
static inline void mke_clear_refc(MKEntry *e)
{
    i_clear_flag(&e->flags, MKE_F_RefCnt);
}
static inline bool mke_is_refc(MKEntry *e)
{
    return i_test_flag(&e->flags, MKE_F_RefCnt);
}

static inline void mke_set_copied(MKEntry *e)
{
    i_set_flag(&e->flags, MKE_F_Copied);
}
static inline void mke_clear_copied(MKEntry *e)
{
    i_clear_flag(&e->flags, MKE_F_Copied);
}
static inline bool mke_is_copied(MKEntry *e)
{
    return i_test_flag(&e->flags, MKE_F_Copied);
}

static inline void mke_clear_refc_copied(MKEntry *e)
{
    i_clear_flag(&e->flags, MKE_F_RefCnt | MKE_F_Copied);
}

static inline int32 mke_get_reader(MKEntry *e)
{
    return i_get_flag(&e->flags, MKE_F_Reader, MKE_ReaderShift);
}
static inline void mke_set_reader(MKEntry *e, int32 r)
{
    Assert(r >= 0 && r <= MKE_MAX_READER);
    i_clear_flag(&e->flags, MKE_F_Reader);
    i_set_flag(&e->flags, r << MKE_ReaderShift);
}

struct MKContext;
struct MKLvContext;

typedef int32 (*MKCompare) (MKEntry *v1, MKEntry *v2, struct MKLvContext *lvctxt, struct MKContext *mkctxt);
typedef void (*MKCopyFree) (MKEntry *dst, MKEntry *src, struct MKLvContext *lvctxt);
typedef Datum (*MKFetchDatumForPrepare) (MKEntry *a, struct MKContext *mkctxt, struct MKLvContext *lvctxt, bool *isNullOut);
typedef void (*MKFreeTuple) (MKEntry *e);

extern void tupsort_prepare(MKEntry *a, struct MKContext *ctxt, int lv);

/*
 * Our impl. of multi-key sort and heap only handles the following data type.
 */
typedef enum MKLvType
{
    MKLV_TYPE_NONE,  /* this level has not yet been assigned a type: todo: verify meaning */
    MKLV_TYPE_INT32, /* this level contains int32 values */
    MKLV_TYPE_CHAR,  /* this level contains char (blank padded) values */
    MKLV_TYPE_TEXT,  /* this level contains text values */
} MKLvType;

typedef struct MKLvContext
{
	/* Is the type of datums in this level passed by value instead of reference */
    bool typByVal;

    /* what is the length field of datums in this level */
    int  typLen;

    /* type of datums in this level, converted to our MKLvType enumeration */
    MKLvType lvtype;

	ScanKeyData	scanKey;

    int16 attno;

    /* the mk heap context that this level context belongs to */
    struct MKContext *mkctxt;
} MKLvContext;

typedef struct MKContext {

	/* how many levels are there?  This should correspond to the # of keys in the multi-key value */
	int total_lv;

	/* the levels themselves */
    MKLvContext *lvctxt;

    /* callback capable of fetching a datum from the MKEntry data */
    MKFetchDatumForPrepare fetchForPrep;

    /* Callback capable of copying prepared data from one MKEntry to another (freeing the dest MKEntry).
     * It can also be called with a NULL src so that the dst is simply freed.
     */
    MKCopyFree  cpfr;

    /**
     * MUST be set
     *
     * pfree() the out-of-line data (not the SortTuple struct!), and increase
     * state->availMem by the amount of memory space thereby released.
     */
    MKFreeTuple freeTup;


    /* as calculated by estimateExtraSpace.  This is only valid before we've switched to buildruns mode
     *   It is only calculate when the fetchForPrep fn is set as well
     */
    long estimatedExtraForPrep;

    /* strxfrm's scale factor (multiplies by length), depends on the current collation */
    int strxfrmScaleFactor;

    /* strxfrm's constant factor, depends on the current collation */
    int strxfrmConstantFactor;

    TupleDesc tupdesc;
    Relation indexRel;
    MemTupleBinding *mt_bind;

    bool		bounded;		/* did caller specify a maximum number of tuples to return? */
    bool		boundUsed;		/* true if we made use of a bounded heap */
    int			bound;			/* if bounded, the maximum number of tuples */

    /* Bit trick: this mask will be set so it can be used to negate a return value.  See comments on usage */
    int32 limitmask;

    /* Unique sort: should the sort discard duplicate values?  */
    bool unique;

    /* enforce Unique, for index build */
    bool enforceUnique;

	/* Name of the index we're building, if any. Used for error messages. */
	char	   *indexname;
} MKContext;

/**
 * This prepares entries AND sets their level.
 */
static inline void mk_prepare_array(MKEntry *a, int i, int j, int32 lv, struct MKContext *ctxt)
{
    MKEntry *last = a+j;
    MKEntry *cur = a+i;

    Assert(a && j>=0);
    do {
        if (ctxt->fetchForPrep)
            tupsort_prepare(cur, ctxt, lv);
        mke_set_lv(cur, lv);
    } while (++cur <= last);
}

extern void tupsort_cpfr(MKEntry *dst, MKEntry *src, MKLvContext *ctxt);
extern int tupsort_compare_datum(MKEntry *v1, MKEntry *v2, MKLvContext *ctxt, MKContext *mkContext);

extern void create_mksort_context(
        MKContext *mkctxt,
        int nkeys, AttrNumber *attNums,
        Oid *sortOperators, bool *nullsFirstFlags,
		ScanKey sk,
        MKFetchDatumForPrepare fetchForPrep, MKFreeTuple freeTup, TupleDesc tupdesc, bool tbyv, int tlen);

/* MK quicksort stuff */
extern void mk_qsort_impl(MKEntry *a, int left, int right, int lv, bool lvdown, MKContext *ctxt, bool seenNull);
static inline void mk_qsort(MKEntry* a, int n, MKContext *ctxt)
{
    mk_qsort_impl(a, 0, n-1, 0, true, ctxt, false);
}

/* MK Heap stuff */
typedef bool (*MKFlagPtrReader) (void *ctxt, MKEntry *e);
typedef struct MKHeapReader
{
    MKFlagPtrReader reader;
    void *mkhr_ctxt;
} MKHeapReader;

/**
 * The heap itself
 */
typedef struct MKHeap 
{
	/* Configuration data for the heap, indicating how to interpret values and how to perform the sort */
    MKContext *mkctxt;

    MKEntry *lvtops;

    /* the top of the heap */
    MKEntry *p;

    /* the number of non-empty values in the heap */
    int count;

    /* the first maxentry values of p represent the heap, which contains cnt # of non-empty values.  cnt <= maxentry
     *
     * todo: rename this to numEntries (or maxentryplusone)
     */
    int maxentry;

    /* the allocated size of p, can be used to check for overruns */
    int alloc_size;

    /* if 0 then we have an instantiated heap
     * If non-zero then we have a heap which contains only the most-recent values from each reader.  When a
     *   value is removed from the heap it will be replenished, if possible, by a value from the reader
     *   that produced the value.
     */
    int nreader;

    /* the readers themselves -- see nreader field */
    MKHeapReader *readers;
} MKHeap;


/* 
 * Create a heap from an array.  Once the heap is created, it owns the array. 
 */
extern MKHeap *mkheap_from_array(MKEntry *entries, int alloc_size, int count, MKContext *mkctxt); 

/* 
 * Create a heap from an array of readers.  MKHeap never owns the reader.  Readers
 * are assumed to have a longer life cycle than the mkheap. 
 */
extern MKHeap *mkheap_from_reader(MKHeapReader *readers, int nreader, MKContext *mkctxt);

extern void mkheap_destroy(MKHeap *mkheap);

/* 
 * Insert into heap.  
 * NOTE: Unusual interfaces.
 * 		insert an flagptr.  
 *		return value < 0 means fail to insert because the value is smaller than the smallest entry in heap.
 *		return value = 0 means fail to insert because the value equals to smallest entry in heap.
 *		return value > 0 means successfully inserted into the heap.
 *
 * This means, we cannot insert anything smaller than current smallest entry, however, we will see this
 * is enough for our use in external sort or limit sort.  More general way of doing this is definitely 
 * doable, but not needed.
 *
 * On output, in cases where the value is >= the min element, *e will be filled in with a copy of the min value of the heap.
 */
extern int mkheap_putAndGet(MKHeap *mkheap, MKEntry *e);
extern int mkheap_putAndGet_run(MKHeap *mkheap, MKEntry *e, int16 run);
extern int mkheap_putAndGet_reader(MKHeap *mkheap, MKEntry *e);

/* 
 * Remove and return smallest elements from the heap.
 */
extern MKEntry *mkheap_peek(MKHeap *mkheap);

static inline bool mkheap_run_match(MKHeap *mkheap, int32 run)
{
    MKEntry *e = mkheap_peek(mkheap);

    if(e)
        return (run == mke_get_run(e)); 
    else
        return false;
}

static inline bool mkheap_empty(MKHeap *mkheap)
{
    return mkheap->count == 0;
}
static inline int64 mkheap_cnt(MKHeap *mkheap)
{
    return mkheap->count;
}

#endif  // TUPLESORT_MK_DETAILS_H
