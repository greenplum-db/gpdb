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
 * Portions Copyright (c) 1996-2016, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/utils/memutils.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef MEMUTILS_H
#define MEMUTILS_H

#include "nodes/memnodes.h"


/*
 * MaxAllocSize, MaxAllocHugeSize
 *		Quasi-arbitrary limits on size of allocations.
 *
 * Note:
 *		There is no guarantee that smaller allocations will succeed, but
 *		larger requests will be summarily denied.
 *
 * palloc() enforces MaxAllocSize, chosen to correspond to the limiting size
 * of varlena objects under TOAST.  See VARSIZE_4B() and related macros in
 * postgres.h.  Many datatypes assume that any allocatable size can be
 * represented in a varlena header.  This limit also permits a caller to use
 * an "int" variable for an index into or length of an allocation.  Callers
 * careful to avoid these hazards can access the higher limit with
 * MemoryContextAllocHuge().  Both limits permit code to assume that it may
 * compute twice an allocation's size without overflow.
 */
#define MaxAllocSize	((Size) 0x3fffffff)		/* 1 gigabyte - 1 */

#define AllocSizeIsValid(size)	((Size) (size) <= MaxAllocSize)

#define MaxAllocHugeSize	(SIZE_MAX / 2)

#define AllocHugeSizeIsValid(size)	((Size) (size) <= MaxAllocHugeSize)

/*
 * All chunks allocated by any memory context manager are required to be
 * preceded by a StandardChunkHeader at a spacing of STANDARDCHUNKHEADERSIZE.
 * A currently-allocated chunk must contain a backpointer to its owning
 * context as well as the allocated size of the chunk.  The backpointer is
 * used by pfree() and repalloc() to find the context to call.  The allocated
 * size is not absolutely essential, but it's expected to be needed by any
 * reasonable implementation.
 */
typedef struct StandardChunkHeader
{
	MemoryContext context;		/* owning context */
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
extern PGDLLIMPORT MemoryContext DispatcherContext;
extern PGDLLIMPORT MemoryContext InterconnectContext;
extern PGDLLIMPORT MemoryContext OptimizerMemoryContext;

/* This is a transient link to the active portal's memory context: */
extern PGDLLIMPORT MemoryContext PortalContext;

/* Backwards compatibility macro */
#define MemoryContextResetAndDeleteChildren(ctx) MemoryContextReset(ctx)


/*
 * Memory-context-type-independent functions in mcxt.c
 */
extern void MemoryContextInit(void);
extern void MemoryContextReset(MemoryContext context);
extern void MemoryContextResetOnly(MemoryContext context);
extern void MemoryContextResetChildren(MemoryContext context);
extern void MemoryContextDeleteChildren(MemoryContext context);
extern void MemoryContextSetParent(MemoryContext context,
					   MemoryContext new_parent);
extern Size GetMemoryChunkSpace(void *pointer);
extern MemoryContext GetMemoryChunkContext(void *pointer);
extern MemoryContext MemoryContextGetParent(MemoryContext context);
extern bool MemoryContextIsEmpty(MemoryContext context);

/* Statistics */
extern void MemoryContextDeclareAccountingRoot(MemoryContext context);
extern Size MemoryContextGetCurrentSpace(MemoryContext context);
extern Size MemoryContextGetPeakSpace(MemoryContext context);
extern Size MemoryContextSetPeakSpace(MemoryContext context, Size nbytes);

#define MemoryContextDelete(context)    (MemoryContextDeleteImpl(context, __FILE__, PG_FUNCNAME_MACRO, __LINE__))
extern void MemoryContextDeleteImpl(MemoryContext context, const char* sfile, const char *func, int sline);

extern void MemoryContextStats(MemoryContext context);
extern void MemoryContextStatsDetail(MemoryContext context, int max_children);
extern void MemoryContextAllowInCriticalSection(MemoryContext context,
									bool allow);

#ifdef MEMORY_CONTEXT_CHECKING
extern void MemoryContextCheck(MemoryContext context);
#endif
extern bool MemoryContextContains(MemoryContext context, void *pointer);

extern void MemoryContextError(int errorcode, MemoryContext context,
                               const char *sfile, int sline,
                               const char *fmt, ...)
                              pg_attribute_noreturn()
                              pg_attribute_printf(5, 6);

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

/* this function should be only called by MemoryContextSetParent() */
extern void AllocSetTransferAccounting(MemoryContext context,
									   MemoryContext new_parent);

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
#define ALLOCSET_DEFAULT_SIZES \
	ALLOCSET_DEFAULT_MINSIZE, ALLOCSET_DEFAULT_INITSIZE, ALLOCSET_DEFAULT_MAXSIZE

/*
 * Recommended alloc parameters for "small" contexts that are never expected
 * to contain much data (for example, a context to contain a query plan).
 */
#define ALLOCSET_SMALL_MINSIZE	 0
#define ALLOCSET_SMALL_INITSIZE  (1 * 1024)
#define ALLOCSET_SMALL_MAXSIZE	 (8 * 1024)
#define ALLOCSET_SMALL_SIZES \
	ALLOCSET_SMALL_MINSIZE, ALLOCSET_SMALL_INITSIZE, ALLOCSET_SMALL_MAXSIZE

/*
 * Recommended alloc parameters for contexts that should start out small,
 * but might sometimes grow big.
 */
#define ALLOCSET_START_SMALL_SIZES \
	ALLOCSET_SMALL_MINSIZE, ALLOCSET_SMALL_INITSIZE, ALLOCSET_DEFAULT_MAXSIZE


/*
 * Threshold above which a request in an AllocSet context is certain to be
 * allocated separately (and thereby have constant allocation overhead).
 * Few callers should be interested in this, but tuplesort/tuplestore need
 * to know it.
 */
#define ALLOCSET_SEPARATE_THRESHOLD  8192

#endif   /* MEMUTILS_H */
