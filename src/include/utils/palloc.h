/*-------------------------------------------------------------------------
 *
 * palloc.h
 *	  POSTGRES memory allocator definitions.
 *
 * This file contains the basic memory allocation interface that is
 * needed by almost every backend module.  It is included directly by
 * postgres.h, so the definitions here are automatically available
 * everywhere.  Keep it lean!
 *
 * Memory allocation occurs within "contexts".  Every chunk obtained from
 * palloc()/MemoryContextAlloc() is allocated within a specific context.
 * The entire contents of a context can be freed easily and quickly by
 * resetting or deleting the context --- this is both faster and less
 * prone to memory-leakage bugs than releasing chunks individually.
 * We organize contexts into context trees to allow fine-grain control
 * over chunk lifetime while preserving the certainty that we will free
 * everything that should be freed.  See utils/mmgr/README for more info.
 *
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/utils/palloc.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PALLOC_H
#define PALLOC_H

/*
 * Optional #defines for debugging...
 *
 * If CDB_PALLOC_CALLER_ID is defined, MemoryContext error and warning
 * messages (such as "out of memory" and "invalid memory alloc request
 * size") will include the caller's source file name and line number.
 * This can be useful in optimized builds where the error handler's
 * stack trace doesn't accurately identify the call site.  Overhead
 * is minimal: two extra parameters to memory allocation functions,
 * and 8 to 16 bytes per context.
 *
 * If CDB_PALLOC_TAGS is defined, every allocation from a standard
 * memory context (aset.c) is tagged with an extra 16 to 32 bytes of
 * debugging info preceding the first byte of the area.  The added
 * header fields identify the allocation call site (source file name
 * and line number).  Also each context keeps a linked list of all
 * of its allocated areas.
 */

/*
#define CDB_PALLOC_CALLER_ID
*/

/*
 * GPDB_93_MERGE_FIXME: This mechanism got broken. If this is resurrected and
 * and made working the --enable-testutils invocations should be readded to
 * gpAux/Makefile. For reference to where, the commit adding this comment has
 * the removal so reverting this will get us back where we were before the
 * merge.
 */
#ifdef USE_ASSERT_CHECKING
//#define CDB_PALLOC_TAGS
#endif

/* CDB_PALLOC_TAGS implies CDB_PALLOC_CALLER_ID */
#if defined(CDB_PALLOC_TAGS) && !defined(CDB_PALLOC_CALLER_ID)
#define CDB_PALLOC_CALLER_ID
#endif

/*
 * We track last OOM time to identify culprit processes that
 * consume too much memory. For 64-bit platform we have high
 * precision time variable based on TimeStampTz. However, on
 * 32-bit platform we only have "per-second" precision.
 * OOMTimeType abstracts this different types.
 */
#if defined(__x86_64__)
typedef int64 OOMTimeType;
#else
typedef uint32 OOMTimeType;
#endif

/*
 * Type MemoryContextData is declared in nodes/memnodes.h.  Most users
 * of memory allocation should just treat it as an abstract type, so we
 * do not provide the struct contents here.
 */
typedef struct MemoryContextData *MemoryContext;

/*
 * A memory context can have callback functions registered on it.  Any such
 * function will be called once just before the context is next reset or
 * deleted.  The MemoryContextCallback struct describing such a callback
 * typically would be allocated within the context itself, thereby avoiding
 * any need to manage it explicitly (the reset/delete action will free it).
 */
typedef void (*MemoryContextCallbackFunction) (void *arg);

typedef struct MemoryContextCallback
{
	MemoryContextCallbackFunction func; /* function to call */
	void	   *arg;			/* argument to pass it */
	struct MemoryContextCallback *next; /* next in list of callbacks */
} MemoryContextCallback;

/*
 * CurrentMemoryContext is the default allocation context for palloc().
 * Avoid accessing it directly!  Instead, use MemoryContextSwitchTo()
 * to change the setting.
 */
extern PGDLLIMPORT MemoryContext CurrentMemoryContext;

extern bool gp_mp_inited;
extern volatile OOMTimeType* segmentOOMTime;
extern volatile OOMTimeType oomTrackerStartTime;
extern volatile OOMTimeType alreadyReportedOOMTime;

/*
 * Flags for MemoryContextAllocExtended.
 */
#define MCXT_ALLOC_HUGE			0x01	/* allow huge allocation (> 1 GB) */
#define MCXT_ALLOC_NO_OOM		0x02	/* no failure if out-of-memory */
#define MCXT_ALLOC_ZERO			0x04	/* zero allocated memory */

/*
 * Fundamental memory-allocation operations (more are in utils/memutils.h)
 */
extern void *MemoryContextAlloc(MemoryContext context, Size size);
extern void *MemoryContextAllocZero(MemoryContext context, Size size);
extern void *MemoryContextAllocZeroAligned(MemoryContext context, Size size);
extern void *MemoryContextAllocExtended(MemoryContext context,
										Size size, int flags);

extern void *palloc(Size size);
extern void *palloc0(Size size);
extern void *palloc_extended(Size size, int flags);
extern void *repalloc(void *pointer, Size size);
extern void pfree(void *pointer);

/*
 * The result of palloc() is always word-aligned, so we can skip testing
 * alignment of the pointer when deciding which MemSet variant to use.
 * Note that this variant does not offer any advantage, and should not be
 * used, unless its "sz" argument is a compile-time constant; therefore, the
 * issue that it evaluates the argument multiple times isn't a problem in
 * practice.
 */
#define palloc0fast(sz) \
	( MemSetTest(0, sz) ? \
		MemoryContextAllocZeroAligned(CurrentMemoryContext, sz) : \
		MemoryContextAllocZero(CurrentMemoryContext, sz) )

/* Higher-limit allocators. */
extern void *MemoryContextAllocHuge(MemoryContext context, Size size);
extern void *repalloc_huge(void *pointer, Size size);

/*
 * Although this header file is nominally backend-only, certain frontend
 * programs like pg_controldata include it via postgres.h.  For some compilers
 * it's necessary to hide the inline definition of MemoryContextSwitchTo in
 * this scenario; hence the #ifndef FRONTEND.
 */

#ifndef FRONTEND
static inline MemoryContext
MemoryContextSwitchTo(MemoryContext context)
{
	MemoryContext old = CurrentMemoryContext;

	CurrentMemoryContext = context;
	return old;
}
#endif							/* FRONTEND */

/* Registration of memory context reset/delete callbacks */
extern void MemoryContextRegisterResetCallback(MemoryContext context,
											   MemoryContextCallback *cb);

/*
 * These are like standard strdup() except the copied string is
 * allocated in a context, not with malloc().
 */
extern char *MemoryContextStrdup(MemoryContext context, const char *string);
extern char *pstrdup(const char *in);
extern char *pnstrdup(const char *in, Size len);

extern char *pchomp(const char *in);

/* sprintf into a palloc'd buffer --- these are in psprintf.c */
extern char *psprintf(const char *fmt,...) pg_attribute_printf(1, 2);
extern size_t pvsnprintf(char *buf, size_t len, const char *fmt, va_list args) pg_attribute_printf(3, 0);

#if defined(WIN32) || defined(__CYGWIN__)
extern void *pgport_palloc(Size sz);
extern char *pgport_pstrdup(const char *str);
extern void pgport_pfree(void *pointer);
#endif


/* Mem Protection */
extern int max_chunks_per_query;

extern PGDLLIMPORT MemoryContext TopMemoryContext;

extern bool MemoryProtection_IsOwnerThread(void);
extern void InitPerProcessOOMTracking(void);
extern void GPMemoryProtect_ShmemInit(void);
extern void GPMemoryProtect_Init(void);
extern void GPMemoryProtect_Shutdown(void);
extern void GPMemoryProtect_TrackStartupMemory(void);
extern void UpdateTimeAtomically(volatile OOMTimeType* time_var);

/*
 * moved here from memutils.h, so that it can be used in the ReportOOMConsumption()
 * macro below
 */
extern void MemoryContextStats(MemoryContext context);

/*
 * ReportOOMConsumption
 *
 * Checks if there was any new OOM event in this segment.
 * In case of a new OOM, it reports the memory consumption
 * of the current process.
 */
#define ReportOOMConsumption()\
{\
	if (gp_mp_inited && *segmentOOMTime >= oomTrackerStartTime && *segmentOOMTime > alreadyReportedOOMTime)\
	{\
		Assert(MemoryProtection_IsOwnerThread());\
		UpdateTimeAtomically(&alreadyReportedOOMTime);\
		write_stderr("One or more query execution processes ran out of memory on this segment. Logging memory usage.");\
		MemoryContextStats(TopMemoryContext);\
	}\
}

#endif							/* PALLOC_H */
