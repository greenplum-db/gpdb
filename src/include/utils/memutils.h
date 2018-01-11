/*-------------------------------------------------------------------------
 *
 * memutils.h
 *	  This file contains declarations for memory allocation utility
 *	  functions.  These are functions that are not quite widely used
 *	  enough to justify going in utils/palloc.h, but are still part
 *	  of the API of the memory management subsystem.
 *
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/utils/memutils.h,v 1.65 2009/01/01 17:24:02 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef MEMUTILS_H
#define MEMUTILS_H

#include "nodes/memnodes.h"
#include "utils/memaccounting.h"

/*
 * MaxAllocSize
 *		Quasi-arbitrary limit on size of allocations.
 *
 * Note:
 *		There is no guarantee that allocations smaller than MaxAllocSize
 *		will succeed.  Allocation requests larger than MaxAllocSize will
 *		be summarily denied.
 *
 * XXX This is deliberately chosen to correspond to the limiting size
 * of varlena objects under TOAST.	See VARATT_MASK_SIZE in postgres.h.
 *
 * XXX Also, various places in aset.c assume they can compute twice an
 * allocation's size without overflow, so beware of raising this.
 */
#define MaxAllocSize	((Size) 0x3fffffff)		/* 1 gigabyte - 1 */

static inline bool AllocSizeIsValid(Size sz)
{
        return (sz < MaxAllocSize);
}

/*
 * Multiple chunks can share a SharedChunkHeader if their shared information
 * such as owning memory context, memoryAccount, memory account generation etc.
 * match. This sharing mechanism optimizes memory consumption by "refactoring"
 * common chunk properties.
 */
typedef struct SharedChunkHeader
{
	MemoryContext context;		/* owning context */
	MemoryAccountIdType memoryAccountId; /* Which account to charge for this memory. */

	/* Combined balance of all the chunks that are sharing this header */
	int64 balance;

	struct SharedChunkHeader *prev;
	struct SharedChunkHeader *next;
} SharedChunkHeader;

/*
 * All chunks allocated by any memory context manager are required to be
 * preceded by a StandardChunkHeader at a spacing of STANDARDCHUNKHEADERSIZE.
 * A currently-allocated chunk must contain a backpointer to its owning
 * context as well as the allocated size of the chunk.	The backpointer is
 * used by pfree() and repalloc() to find the context to call.	The allocated
 * size is not absolutely essential, but it's expected to be needed by any
 * reasonable implementation.
 */
typedef struct StandardChunkHeader
{
	 /*
	  * SharedChunkHeader stores all the "shared" details
	  * among multiple chunks, such as memoryAccount to charge,
	  * generation of memory account, memory context that owns this
	  * chunk etc.
	  */
	struct SharedChunkHeader* sharedHeader;
	Size		size;			/* size of data space allocated in chunk */

#ifdef MEMORY_CONTEXT_CHECKING
	/* when debugging memory usage, also store actual requested size */
	Size		requested_size;
#endif
#ifdef CDB_PALLOC_TAGS
	const char  *alloc_tag;
	int 		alloc_n;
	void *prev_chunk;
	void *next_chunk;
#endif
} StandardChunkHeader;

#define STANDARDCHUNKHEADERSIZE  MAXALIGN(sizeof(StandardChunkHeader))

/*--------------------
 * Chunk freelist k holds chunks of size 1 << (k + ALLOC_MINBITS),
 * for k = 0 .. ALLOCSET_NUM_FREELISTS-1.
 *
 * Note that all chunks in the freelists have power-of-2 sizes.  This
 * improves recyclability: we may waste some space, but the wasted space
 * should stay pretty constant as requests are made and released.
 *
 * A request too large for the last freelist is handled by allocating a
 * dedicated block from malloc().  The block still has a block header and
 * chunk header, but when the chunk is freed we'll return the whole block
 * to malloc(), not put it on our freelists.
 *
 * CAUTION: ALLOC_MINBITS must be large enough so that
 * 1<<ALLOC_MINBITS is at least MAXALIGN,
 * or we may fail to align the smallest chunks adequately.
 * 8-byte alignment is enough on all currently known machines.
 *
 * With the current parameters, request sizes up to 8K are treated as chunks,
 * larger requests go into dedicated blocks.  Change ALLOCSET_NUM_FREELISTS
 * to adjust the boundary point.
 *--------------------
 */

#define ALLOC_MINBITS		3	/* smallest chunk size is 8 bytes */
#define ALLOCSET_NUM_FREELISTS	11
#define ALLOC_CHUNK_LIMIT	(1 << (ALLOCSET_NUM_FREELISTS-1+ALLOC_MINBITS))
/* Size of largest chunk that we use a fixed size for */

typedef struct AllocBlockData *AllocBlock;		/* forward reference */
typedef struct AllocChunkData *AllocChunk;

/*
 * AllocSetContext is our standard implementation of MemoryContext.
 *
 * Note: isReset means there is nothing for AllocSetReset to do.  This is
 * different from the aset being physically empty (empty blocks list) because
 * we may still have a keeper block.  It's also different from the set being
 * logically empty, because we don't attempt to detect pfree'ing the last
 * active chunk.
 */
typedef struct AllocSetContext
{
	MemoryContextData header;	/* Standard memory-context fields */
	/* Info about storage allocated in this context: */
	AllocBlock	blocks;			/* head of list of blocks in this set */
	AllocChunk	freelist[ALLOCSET_NUM_FREELISTS];		/* free chunk lists */
	bool		isReset;		/* T = no space alloced since last reset */
	/* Allocation parameters for this context: */
	Size		initBlockSize;	/* initial block size */
	Size		maxBlockSize;	/* maximum block size */
	Size		nextBlockSize;	/* next block size to allocate */
	Size		allocChunkLimit; /* effective chunk size limit */
	AllocBlock	keeper;			/* if not NULL, keep this block over resets */

	/* Points to the head of the sharedHeaderList */
	SharedChunkHeader *sharedHeaderList;
	/* The memory account of this SharedChunkHeader is NULL */
	SharedChunkHeader *nullAccountHeader;

#ifdef CDB_PALLOC_TAGS
	/*
	 * allocList maintains a list of chunks (double linked list) that are
	 * currently allocated.
	 */
	AllocChunk  allocList;
#endif
} AllocSetContext;

typedef AllocSetContext *AllocSet;

/*
 * Standard top-level memory contexts.
 *
 * Only TopMemoryContext and ErrorContext are initialized by
 * MemoryContextInit() itself.
 */
extern PGDLLIMPORT MemoryContext TopMemoryContext;
extern PGDLLIMPORT MemoryContext ErrorContext;
extern PGDLLIMPORT MemoryContext PostmasterContext;
extern PGDLLIMPORT MemoryContext CacheMemoryContext;
extern PGDLLIMPORT MemoryContext MessageContext;
extern PGDLLIMPORT MemoryContext TopTransactionContext;
extern PGDLLIMPORT MemoryContext CurTransactionContext;
extern PGDLLIMPORT MemoryContext MemoryAccountMemoryContext;
extern PGDLLIMPORT MemoryContext MemoryAccountDebugContext;

/* This is a transient link to the active portal's memory context: */
extern PGDLLIMPORT MemoryContext PortalContext;


/*
 * Memory-context-type-independent functions in mcxt.c
 */
extern void MemoryContextInit(void);
extern void MemoryContextReset(MemoryContext context);
extern void MemoryContextResetChildren(MemoryContext context);
extern void MemoryContextDeleteChildren(MemoryContext context);
extern void MemoryContextResetAndDeleteChildren(MemoryContext context);
extern Size GetMemoryChunkSpace(void *pointer);
extern MemoryContext GetMemoryChunkContext(void *pointer);
extern bool MemoryContextIsEmpty(MemoryContext context);

/* Statistics */
extern Size MemoryContextGetCurrentSpace(MemoryContext context);
extern Size MemoryContextGetPeakSpace(MemoryContext context);
extern Size MemoryContextSetPeakSpace(MemoryContext context, Size nbytes);
extern char *MemoryContextName(MemoryContext context, MemoryContext relativeTo,
                               char *buf, int bufsize);

#define MemoryContextDelete(context)    (MemoryContextDeleteImpl(context, __FILE__, PG_FUNCNAME_MACRO, __LINE__))
extern void MemoryContextDeleteImpl(MemoryContext context, const char* sfile, const char *func, int sline);

#ifdef CDB_PALLOC_TAGS
extern void dump_memory_allocation(const char* fname);
extern void dump_memory_allocation_ctxt(FILE * ofile, void *ctxt);
#endif

#ifdef MEMORY_CONTEXT_CHECKING
extern void MemoryContextCheck(MemoryContext context);
#endif
extern bool MemoryContextContains(MemoryContext context, void *pointer);
extern bool MemoryContextContainsGenericAllocation(MemoryContext context, void *pointer);

/* Functions called only by context-type-specific memory managers... */
extern void MemoryContextNoteAlloc(MemoryContext context, Size nbytes);
extern void MemoryContextNoteFree(MemoryContext context, Size nbytes);
#ifdef _MSC_VER
__declspec(noreturn)
#endif
extern void MemoryContextError(int errorcode, MemoryContext context,
                               const char *sfile, int sline,
                               const char *fmt, ...)
                              __attribute__((__noreturn__));

/*
 * This routine handles the context-type-independent part of memory
 * context creation.  It's intended to be called from context-type-
 * specific creation routines, and noplace else.
 */
extern MemoryContext MemoryContextCreate(NodeTag tag, Size size,
					MemoryContextMethods *methods,
					MemoryContext parent,
					const char *name);


/*
 * Memory-context-type-specific functions
 */

/* aset.c */
extern MemoryContext AllocSetContextCreate(MemoryContext parent,
					  const char *name,
					  Size minContextSize,
					  Size initBlockSize,
					  Size maxBlockSize);

/* mpool.c */
typedef struct MPool MPool;
extern MPool *mpool_create(MemoryContext parent,
						   const char *name);
extern void *mpool_alloc(MPool *mpool, Size size);
extern void mpool_reset(MPool *mpool);
extern void mpool_delete(MPool *mpool);
extern uint64 mpool_total_bytes_allocated(MPool *mpool);
extern uint64 mpool_bytes_used(MPool *mpool);

/*
 * Recommended default alloc parameters, suitable for "ordinary" contexts
 * that might hold quite a lot of data.
 */
#define ALLOCSET_DEFAULT_MINSIZE   0
#define ALLOCSET_DEFAULT_INITSIZE  (8 * 1024)
#define ALLOCSET_DEFAULT_MAXSIZE   (8 * 1024 * 1024)

/*
 * Recommended alloc parameters for "small" contexts that are not expected
 * to contain much data (for example, a context to contain a query plan).
 */
#define ALLOCSET_SMALL_MINSIZE	 0
#define ALLOCSET_SMALL_INITSIZE  (1 * 1024)
#define ALLOCSET_SMALL_MAXSIZE	 (8 * 1024)

/*
 * Threshold above which a request in an AllocSet context is certain to be
 * allocated separately (and thereby have constant allocation overhead).
 * Few callers should be interested in this, but tuplesort/tuplestore need
 * to know it.
 */
#define ALLOCSET_SEPARATE_THRESHOLD  8192

typedef struct SwitchedMemoryContext
{
	MemoryContext oldContext;
	MemoryContext newContext;
}
SwitchedMemoryContext;

/**
 * Shorthand for doing an AllocSetContextCreate and then switching to the new context,
 *   using DEFAULT memory values
 */
static inline SwitchedMemoryContext
AllocSetCreateDefaultContextInCurrentAndSwitchTo(const char *name)
{
	SwitchedMemoryContext res;
	res.newContext = AllocSetContextCreate(CurrentMemoryContext, name,
									   ALLOCSET_DEFAULT_MINSIZE,
									   ALLOCSET_DEFAULT_INITSIZE,
									   ALLOCSET_DEFAULT_MAXSIZE);

   	res.oldContext = MemoryContextSwitchTo(res.newContext);
   	return res;
}

/**
 * Shorthand for doing an AllocSetContextCreate and then switching to the new context,
 *   using SMALL memory values
 */
static inline SwitchedMemoryContext
AllocSetCreateSmallContextInCurrentAndSwitchTo(const char *name)
{
	SwitchedMemoryContext res;
	res.newContext = AllocSetContextCreate(CurrentMemoryContext, name,
									   ALLOCSET_SMALL_MINSIZE,
									   ALLOCSET_SMALL_INITSIZE,
									   ALLOCSET_SMALL_MAXSIZE);

   	res.oldContext = MemoryContextSwitchTo(res.newContext);
   	return res;
}

static inline void
DeleteAndRestoreSwitchedMemoryContext(SwitchedMemoryContext context)
{
	MemoryContextSwitchTo(context.oldContext);
	MemoryContextDelete(context.newContext);
}

/*
 * floor_log2_Size
 *
 * Returns the largest integer i such that 2**i <= sz
 */
int floor_log2_Size(Size sz);   /* in utils/mmgr/mcxt.c */

static inline int
floor_log2_Size_inline(Size sz)
{
    unsigned    u;
    int         shift;

    Assert(sz > 0);

    if (sizeof(sz) > 4 &&
        sz > (Size)0xffffffff)
    {
        u = (unsigned)((sz >> 16) >> 16);
        shift = 32;
    }
    else
    {
        u = (unsigned)sz;
        shift = 0;
    }

    if (u > 0xffff)
    {
        u >>= 16;
        shift += 16;
    }
    if (u > 0xff)
    {
        u >>= 8;
        shift += 8;
    }
    if (u > 0xf)
    {
        u >>= 4;
        shift += 4;
    }
    if (u > 3)
    {
        u >>= 2;
        shift += 2;
    }
    if (u > 1)
        shift += 1;

    Assert(sz >> shift == (Size)1);
    return shift;
}                               /* floor_log2_Size_inline */

/*
 * ceil_log2_Size
 *
 * Returns the smallest integer i such that sz <= 2**i
 */
int ceil_log2_Size(Size sz);    /* in utils/mmgr/mcxt.c */

static inline int
ceil_log2_Size_inline(Size sz)
{
    int     shift = floor_log2_Size_inline(sz);

    if (sz > (Size)1 << shift)
        shift++;

    Assert(sz <= (Size)1 << shift);
    return shift;
}                               /* ceil_log2_Size_inline */


#endif   /* MEMUTILS_H */
