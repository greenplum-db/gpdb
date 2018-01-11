/*-------------------------------------------------------------------------
 *
 * mcxt.c
 *	  POSTGRES memory context management code.
 *
 * This module handles context management operations that are independent
 * of the particular kind of context being operated on.  It calls
 * context-type-specific operations via the function pointers in a
 * context's MemoryContextMethods struct.
 *
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/utils/mmgr/mcxt.c,v 1.66 2009/01/01 17:23:53 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "miscadmin.h"                      /* MyProcPid */
#include "utils/memutils.h"
#include "utils/memaccounting.h"

#include "cdb/cdbvars.h"                    /* gp_process_memory_cutoff_bytes */
#include "inttypes.h"

#ifdef HAVE_STDINT_H
#include <stdint.h>                         /* SIZE_MAX (C99) */
#endif
#ifndef SIZE_MAX
#define SIZE_MAX ((Size)0-(Size)1)          /* for Solaris */
#endif

#ifdef CDB_PALLOC_CALLER_ID
#define CDB_MCXT_WHERE(context) (context)->callerFile, (context)->callerLine
#else
#define CDB_MCXT_WHERE(context) __FILE__, __LINE__
#endif

#if defined(CDB_PALLOC_TAGS) && !defined(CDB_PALLOC_CALLER_ID)
#error "If CDB_PALLOC_TAGS is defined, CDB_PALLOC_CALLER_ID must be defined too"
#endif

/* Maximum allowed length of the name of a context including the parent names prepended */
#define MAX_CONTEXT_NAME_SIZE 200

/*****************************************************************************
 *	  GLOBAL MEMORY															 *
 *****************************************************************************/

/*
 * CurrentMemoryContext
 *		Default memory context for allocations.
 */
MemoryContext CurrentMemoryContext = NULL;

/*
 * Standard top-level contexts. For a description of the purpose of each
 * of these contexts, refer to src/backend/utils/mmgr/README
 */
MemoryContext TopMemoryContext = NULL;
MemoryContext ErrorContext = NULL;
MemoryContext PostmasterContext = NULL;
MemoryContext CacheMemoryContext = NULL;
MemoryContext MessageContext = NULL;
MemoryContext TopTransactionContext = NULL;
MemoryContext CurTransactionContext = NULL;
MemoryContext MemoryAccountMemoryContext = NULL;

/* This is a transient link to the active portal's memory context: */
MemoryContext PortalContext = NULL;


/*****************************************************************************
 *	  EXPORTED ROUTINES														 *
 *****************************************************************************/


/*
 * MemoryContextInit
 *		Start up the memory-context subsystem.
 *
 * This must be called before creating contexts or allocating memory in
 * contexts.  TopMemoryContext and ErrorContext are initialized here;
 * other contexts must be created afterwards.
 *
 * In normal multi-backend operation, this is called once during
 * postmaster startup, and not at all by individual backend startup
 * (since the backends inherit an already-initialized context subsystem
 * by virtue of being forked off the postmaster).
 *
 * In a standalone backend this must be called during backend startup.
 */
void
MemoryContextInit(void)
{
	AssertState(TopMemoryContext == NULL);
	AssertState(CurrentMemoryContext == NULL);
	AssertState(MemoryAccountMemoryContext == NULL);

	/*
	 * Initialize TopMemoryContext as an AllocSetContext with slow growth rate
	 * --- we don't really expect much to be allocated in it.
	 *
	 * (There is special-case code in MemoryContextCreate() for this call.)
	 */
	TopMemoryContext = AllocSetContextCreate((MemoryContext) NULL,
											 "TopMemoryContext",
											 0,
											 8 * 1024,
											 8 * 1024);

	/*
	 * Not having any other place to point CurrentMemoryContext, make it point
	 * to TopMemoryContext.  Caller should change this soon!
	 */
	CurrentMemoryContext = TopMemoryContext;

	/*
	 * Initialize ErrorContext as an AllocSetContext with slow growth rate ---
	 * we don't really expect much to be allocated in it. More to the point,
	 * require it to contain at least 8K at all times. This is the only case
	 * where retained memory in a context is *essential* --- we want to be
	 * sure ErrorContext still has some memory even if we've run out
	 * elsewhere!
	 */
	ErrorContext = AllocSetContextCreate(TopMemoryContext,
										 "ErrorContext",
										 8 * 1024,
										 8 * 1024,
										 8 * 1024);

	MemoryAccounting_Reset();
}

/*
 * MemoryContextReset
 *		Release all space allocated within a context and its descendants,
 *		but don't delete the contexts themselves.
 *
 * The type-specific reset routine handles the context itself, but we
 * have to do the recursion for the children.
 */
void
MemoryContextReset(MemoryContext context)
{
	AssertArg(MemoryContextIsValid(context));

	/* save a function call in common case where there are no children */
	if (context->firstchild != NULL)
		MemoryContextResetChildren(context);

	(*context->methods.reset) (context);
}

/*
 * MemoryContextResetChildren
 *		Release all space allocated within a context's descendants,
 *		but don't delete the contexts themselves.  The named context
 *		itself is not touched.
 */
void
MemoryContextResetChildren(MemoryContext context)
{
	MemoryContext child;

	AssertArg(MemoryContextIsValid(context));

	for (child = context->firstchild; child != NULL; child = child->nextchild)
		MemoryContextReset(child);
}

/*
 * MemoryContextDelete
 *		Delete a context and its descendants, and release all space
 *		allocated therein.
 *
 * The type-specific delete routine removes all subsidiary storage
 * for the context, but we have to delete the context node itself,
 * as well as recurse to get the children.	We must also delink the
 * node from its parent, if it has one.
 */
void
MemoryContextDeleteImpl(MemoryContext context, const char* sfile, const char *func, int sline)
{
	AssertArg(MemoryContextIsValid(context));
	/* We had better not be deleting TopMemoryContext ... */
	Assert(context != TopMemoryContext);
	/* And not CurrentMemoryContext, either */
	Assert(context != CurrentMemoryContext);

#ifdef CDB_PALLOC_CALLER_ID
	context->callerFile = sfile;
	context->callerLine = sline;
#endif

	MemoryContextDeleteChildren(context);

	/*
	 * We delink the context from its parent before deleting it, so that if
	 * there's an error we won't have deleted/busted contexts still attached
	 * to the context tree.  Better a leak than a crash.
	 */
	if (context->parent)
	{
		MemoryContext parent = context->parent;

		if (context == parent->firstchild)
			parent->firstchild = context->nextchild;
		else
		{
			MemoryContext child;

			for (child = parent->firstchild; child; child = child->nextchild)
			{
				if (context == child->nextchild)
				{
					child->nextchild = context->nextchild;
					break;
				}
			}
		}
	}
	(*context->methods.delete_context)(context);
	pfree(context);
}

/*
 * MemoryContextDeleteChildren
 *		Delete all the descendants of the named context and release all
 *		space allocated therein.  The named context itself is not touched.
 */
void
MemoryContextDeleteChildren(MemoryContext context)
{
	AssertArg(MemoryContextIsValid(context));

	/*
	 * MemoryContextDelete will delink the child from me, so just iterate as
	 * long as there is a child.
	 */
	while (context->firstchild != NULL)
		MemoryContextDelete(context->firstchild);
}

/*
 * MemoryContextResetAndDeleteChildren
 *		Release all space allocated within a context and delete all
 *		its descendants.
 *
 * This is a common combination case where we want to preserve the
 * specific context but get rid of absolutely everything under it.
 */
void
MemoryContextResetAndDeleteChildren(MemoryContext context)
{
	AssertArg(MemoryContextIsValid(context));

	MemoryContextDeleteChildren(context);
	(*context->methods.reset) (context);
}

/*
 * GetMemoryChunkSpace
 *		Given a currently-allocated chunk, determine the total space
 *		it occupies (including all memory-allocation overhead).
 *
 * This is useful for measuring the total space occupied by a set of
 * allocated chunks.
 */
Size
GetMemoryChunkSpace(void *pointer)
{
	StandardChunkHeader *header;

	/*
	 * Try to detect bogus pointers handed to us, poorly though we can.
	 * Presumably, a pointer that isn't MAXALIGNED isn't pointing at an
	 * allocated chunk.
	 */
	Assert(pointer != NULL);
	Assert(pointer == (void *) MAXALIGN(pointer));

	/*
	 * OK, it's probably safe to look at the chunk header.
	 */
	header = (StandardChunkHeader *)
		((char *) pointer - STANDARDCHUNKHEADERSIZE);

	AssertArg(MemoryContextIsValid(header->sharedHeader->context));

	return (*header->sharedHeader->context->methods.get_chunk_space) (header->sharedHeader->context,
														 pointer);
}

/*
 * GetMemoryChunkContext
 *		Given a currently-allocated chunk, determine the context
 *		it belongs to.
 */
MemoryContext
GetMemoryChunkContext(void *pointer)
{
	StandardChunkHeader *header;

	/*
	 * Try to detect bogus pointers handed to us, poorly though we can.
	 * Presumably, a pointer that isn't MAXALIGNED isn't pointing at an
	 * allocated chunk.
	 */
	Assert(pointer != NULL);
	Assert(pointer == (void *) MAXALIGN(pointer));

	/*
	 * OK, it's probably safe to look at the chunk header.
	 */
	header = (StandardChunkHeader *)
		((char *) pointer - STANDARDCHUNKHEADERSIZE);

	AssertArg(MemoryContextIsValid(header->sharedHeader->context));

	return header->sharedHeader->context;
}

/*
 * MemoryContextIsEmpty
 *		Is a memory context empty of any allocated space?
 */
bool
MemoryContextIsEmpty(MemoryContext context)
{
	AssertArg(MemoryContextIsValid(context));

	/*
	 * For now, we consider a memory context nonempty if it has any children;
	 * perhaps this should be changed later.
	 */
	if (context->firstchild != NULL)
		return false;
	/* Otherwise use the type-specific inquiry */
	return (*context->methods.is_empty) (context);
}


/*
 * MemoryContextNoteAlloc
 *		Update lifetime cumulative statistics upon allocation from host mem mgr.
 *
 * Called by the context-type-specific memory manager upon successfully
 * obtaining a block of size 'nbytes' from its lower-level source (e.g. malloc).
 */
void
MemoryContextNoteAlloc(MemoryContext context, Size nbytes)
{
    Size            held;

    AssertArg(MemoryContextIsValid(context));

    for (;;)
    {
        Assert(context->allBytesAlloc >= context->allBytesFreed);
        Assert(context->allBytesAlloc - context->allBytesFreed < SIZE_MAX - nbytes);

        context->allBytesAlloc += nbytes;

        held = (Size)(context->allBytesAlloc - context->allBytesFreed);
        if (context->maxBytesHeld < held)
            context->maxBytesHeld = held;

        if (!context->parent)
            break;
        context = context->parent;
    }
}                               /* MemoryContextNoteAlloc */

/*
 * MemoryContextNoteFree
 *		Update lifetime cumulative statistics upon free to host memory manager.
 *
 * Called by the context-type-specific memory manager upon relinquishing a
 * block of size 'nbytes' back to its lower-level source (e.g. free()).
 */
void
MemoryContextNoteFree(MemoryContext context, Size nbytes)
{
    Size    held;

	AssertArg(MemoryContextIsValid(context));

    while (context)
    {
        Assert(context->allBytesAlloc >= context->allBytesFreed + nbytes);
        Assert(context->allBytesFreed + nbytes >= context->allBytesFreed);

        context->allBytesFreed += nbytes;

        held = (Size)(context->allBytesAlloc - context->allBytesFreed);
        if (context->localMinHeld > held)
            context->localMinHeld = held;

        context = context->parent;
    }
}                               /* MemoryContextNoteFree */

/*
 * MemoryContextError
 *		Report failure of a memory context operation.  Does not return.
 */
void
MemoryContextError(int errorcode, MemoryContext context,
		const char *sfile, int sline,
		const char *fmt, ...)
{
	va_list args;
	char    buf[200];

	/*
	 * Don't use elog, as we might have a malloc problem.
	 * Also, don't use write_log, as this method might be
	 * called from syslogger, which does not support
	 * write_log calls
	 */
	write_stderr("Logging memory usage for memory context error");

	MemoryAccounting_SaveToLog();
	MemoryContextStats(TopMemoryContext);

	if(coredump_on_memerror)
	{
		/*
		 * Turn memory context into a SIGSEGV, so will generate
		 * a core dump.
		 *
		 * XXX What is the right way of doing this?
		 */
		((void(*)()) NULL)();
	}

	if(errorcode != ERRCODE_OUT_OF_MEMORY && errorcode != ERRCODE_INTERNAL_ERROR)
	{
		Assert(!"Memory context error: unknown error code.");
	}

	/* Format caller's message. */
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf)-32, fmt, args);
	va_end(args);

	/*
	 * This might fail if we run out of memory at the system level
	 * (i.e., malloc returned null), and the system is running so
	 * low in memory that ereport cannot format its parameter.
	 * However, we already dumped our usage information using
	 * write_stderr, so we are gonna take a chance by calling ereport.
	 * If we fail, we at least have OOM message in the log. If we succeed,
	 * we will also have the detail error code and location of the error.
	 * Note, ereport should switch to ErrorContext which should have
	 * some preallocated memory to handle this message. Therefore,
	 * our chance of success is quite high
	 */
	ereport(ERROR, (errcode(errorcode),
				errmsg("%s (context '%s') (%s:%d)",
					buf,
					context->name,
					sfile ? sfile : "",
					sline)
		       ));

	/* not reached */
	abort();
}                               /* MemoryContextError */


/*
 * MemoryContextGetCurrentSpace
 *		Return the number of bytes currently occupied by the memory context.
 *
 * This is the amount of space obtained from the lower-level source of the
 * memory (e.g. malloc) and not yet released back to that source.  Includes
 * overhead and free space held and managed within this context by the
 * context-type-specific memory manager.
 */
Size
MemoryContextGetCurrentSpace(MemoryContext context)
{
	AssertArg(MemoryContextIsValid(context));
    Assert(context->allBytesAlloc >= context->allBytesFreed);
    Assert(context->allBytesAlloc - context->allBytesFreed < SIZE_MAX);

    return (Size)(context->allBytesAlloc - context->allBytesFreed);
}                               /* MemoryContextGetCurrentSpace */

/*
 * MemoryContextGetPeakSpace
 *		Return the peak number of bytes occupied by the memory context.
 *
 * This is the maximum value reached by MemoryContextGetCurrentSpace() since
 * the context was created, or since reset by MemoryContextSetPeakSpace().
 */
Size
MemoryContextGetPeakSpace(MemoryContext context)
{
	AssertArg(MemoryContextIsValid(context));
    return context->maxBytesHeld;
}                               /* MemoryContextGetPeakSpace */

/*
 * MemoryContextSetPeakSpace
 *		Resets the peak space statistic to the space currently occupied or
 *      the specified value, whichever is greater.  Returns the former peak
 *      space value.
 *
 * Can be used to observe local maximum usage over an interval and then to
 * restore the overall maximum.
 */
Size
MemoryContextSetPeakSpace(MemoryContext context, Size nbytes)
{
    Size    held;
    Size    oldpeak;

	AssertArg(MemoryContextIsValid(context));
    Assert(context->allBytesAlloc >= context->allBytesFreed);
    Assert(context->allBytesAlloc - context->allBytesFreed < SIZE_MAX);

    oldpeak = context->maxBytesHeld;

    held = (Size)(context->allBytesAlloc - context->allBytesFreed);
    context->maxBytesHeld = Max(held, nbytes);

    return oldpeak;
}                               /* MemoryContextSetPeakSpace */


/*
 * MemoryContextName
 *		Format the name of the memory context into the caller's buffer.
 *
 * Returns ptr to the name string within the supplied buffer.  (The string
 * is built at the tail of the buffer from right to left.)
 */
char *
MemoryContextName(MemoryContext context, MemoryContext relativeTo,
                  char *buf, int bufsize)
{
    MemoryContext   ctx;
    char           *cbp = buf + bufsize - 1;

	AssertArg(MemoryContextIsValid(context));

    if (bufsize <= 0)
        return buf;

    for (ctx = context; ctx && ctx != relativeTo; ctx = ctx->parent)
    {
        const char *name = ctx->name ? ctx->name : "";
        int         len = strlen(name);

        if (cbp - buf < len + 1)
        {
            len = Min(3, cbp - buf);
            cbp -= len;
            memcpy(cbp, "...", len);
            break;
        }
        if (ctx != context)
            *--cbp = '/';
        cbp -= len;
        memcpy(cbp, name, len);
    }

    if (buf < cbp)
    {
        if (!ctx)
            *--cbp = '/';
        else if (ctx == context)
            *--cbp = '.';
    }

    buf[bufsize-1] = '\0';
    return cbp;
}                               /* MemoryContextName */

/*
 * MemoryContext_LogContextStats
 *		Logs memory consumption details of a given context.
 *
 *	Parameters:
 *		siblingCount: number of sibling context of this context in the memory context tree
 *		allAllocated: total bytes allocated in this context
 *		allFreed: total bytes freed in this context
 *		curAvailable: bytes that are allocated in blocks but are not used in any chunks
 *		contextName: name of the context
 */
static void
MemoryContext_LogContextStats(uint64 siblingCount, uint64 allAllocated,
		uint64 allFreed, uint64 curAvailable, const char *contextName)
{
	write_stderr("context: " UINT64_FORMAT ", " UINT64_FORMAT ", " UINT64_FORMAT ", " UINT64_FORMAT ", " UINT64_FORMAT ", %s\n", \
	siblingCount, (allAllocated - allFreed), curAvailable, \
	allAllocated, allFreed, contextName);
}


/*
 * MemoryContextStats_recur
 *		Print statistics about the named context and all its descendants.
 *
 * This is just a debugging utility, so it's not fancy.  The statistics
 * are merely sent to stderr.
 *
 * Parameters:
 * 		topContext: the top of the sub-tree where we start our processing
 * 		rootContext: the root context of the entire tree that can be used
 * 		to generate a bread crumb like context name
 *
 * 		topContexName: the name of the top context
 * 		nameBuffer: a buffer to format the name of any future context
 *		nameBufferSize: size of the nameBuffer
 *		nBlocksTop: number of blocks in the top context
 *		nChunksTop: number of chunks in the top context
 *
 *		currentAvailableTop: free space across all blocks in the top context
 *
 *		allAllocatedTop: total bytes allocated in the top context, including
 *		blocks that are already dropped
 *
 *		allFreedTop: total bytes that were freed in the top context
 *		maxHeldTop: maximum bytes held in the top context
 */
static void
MemoryContextStats_recur(MemoryContext topContext, MemoryContext rootContext,
                         char *topContextName, char *nameBuffer, int nameBufferSize,
                         uint64 nBlocksTop, uint64 nChunksTop,
                         uint64 currentAvailableTop, uint64 allAllocatedTop,
                         uint64 allFreedTop, uint64 maxHeldTop)
{
	MemoryContext   child;
    char*           name;

	AssertArg(MemoryContextIsValid(topContext));

	uint64 nBlocks = 0;
	uint64 nChunks = 0;
	uint64 currentAvailable = 0;
	uint64 allAllocated = 0;
	uint64 allFreed = 0;
	uint64 maxHeld = 0;

	/*
	 * The top context is always supposed to have children contexts. Therefore, it is not
	 * collapse-able with other siblings. So, the siblingCount is set to 1.
	 */
	MemoryContext_LogContextStats(1 /* siblingCount */, allAllocatedTop, allFreedTop, currentAvailableTop, topContextName);

    uint64 cumBlocks = 0;
    uint64 cumChunks = 0;
    uint64 cumCurAvailable = 0;
    uint64 cumAllAllocated = 0;
    uint64 cumAllFreed = 0;
    uint64 cumMaxHeld = 0;

    char prevChildName[MAX_CONTEXT_NAME_SIZE] = "";

    uint64 siblingCount = 0;

	for (child = topContext->firstchild; child != NULL; child = child->nextchild)
	{
		/* Get name and ancestry of this MemoryContext */
		name = MemoryContextName(child, rootContext, nameBuffer, nameBufferSize);

		(*child->methods.stats)(child, &nBlocks, &nChunks, &currentAvailable, &allAllocated, &allFreed, &maxHeld);

		if (child->firstchild == NULL)
		{
			/* To qualify for sibling collapsing the context must not have any child context */

			if (strcmp(name, prevChildName) == 0)
			{
				cumBlocks += nBlocks;
				cumChunks += nChunks;
				cumCurAvailable += currentAvailable;
				cumAllAllocated += allAllocated;
				cumAllFreed += allFreed;
				cumMaxHeld = Max(cumMaxHeld, maxHeld);

				siblingCount++;
			}
			else
			{
				if (siblingCount != 0)
				{
					/*
					 * Output the previous cumulative stat, and start a new run. Note: don't just
					 * pass the new one to MemoryContextStats_recur, as the new one might be the
					 * start of another run of duplicate contexts
					 */

					MemoryContext_LogContextStats(siblingCount, cumAllAllocated, cumAllFreed, cumCurAvailable, prevChildName);
				}

				cumBlocks = nBlocks;
				cumChunks = nChunks;
				cumCurAvailable = currentAvailable;
				cumAllAllocated = allAllocated;
				cumAllFreed = allFreed;
				cumMaxHeld = maxHeld;

				/* Move new name into previous name */
				strncpy(prevChildName, name, MAX_CONTEXT_NAME_SIZE - 1);

				/* The current one is the sole sibling */
				siblingCount = 1;
			}
		}
		else
		{
			/* Does not qualify for sibling collapsing as the context has child context */

			if (siblingCount != 0)
			{
				/*
				 * We have previously collapsed (one or more siblings with empty children) context
				 * stats that we want to print here. Output the previous cumulative stat.
				 */

				MemoryContext_LogContextStats(siblingCount, cumAllAllocated, cumAllFreed, cumCurAvailable, prevChildName);
			}

			MemoryContextStats_recur(child, rootContext, name, nameBuffer, nameBufferSize, nBlocks,
					nChunks, currentAvailable, allAllocated, allFreed, maxHeld);

			/*
			 * We just traversed a child node, so we need to make sure we don't carry over
			 * any child name from previous matching siblings. So, we reset prevChildName,
			 * and all cumulative stats
			 */
			prevChildName[0] = '\0';

			cumBlocks = 0;
			cumChunks = 0;
			cumCurAvailable = 0;
			cumAllAllocated = 0;
			cumAllFreed = 0;
			cumMaxHeld = 0;

			/*
			 * The current one doesn't qualify for collapsing, and we already
			 * printed it and its children by calling MemoryContextStats_recur
			 */
			siblingCount = 0;
		}
	}

	if (siblingCount != 0)
	{
		/* Output any unprinted cumulative stats */

		MemoryContext_LogContextStats(siblingCount, cumAllAllocated, cumAllFreed, cumCurAvailable, prevChildName);
	}
}

/*
 * MemoryContextStats
 *		Prints the usage details of a context.
 *
 * Parameters:
 * 		context: the context of interest.
 */
void
MemoryContextStats(MemoryContext context)
{
    char*     name;
    char      namebuf[MAX_CONTEXT_NAME_SIZE];

	AssertArg(MemoryContextIsValid(context));

    name = MemoryContextName(context, NULL, namebuf, sizeof(namebuf));
    write_stderr("pid %d: Memory statistics for %s/\n", MyProcPid, name);
    write_stderr("context: occurrences_count, currently_allocated, currently_available, total_allocated, total_freed, name\n");

	uint64 nBlocks = 0;
	uint64 nChunks = 0;
	uint64 currentAvailable = 0;
	uint64 allAllocated = 0;
	uint64 allFreed = 0;
	uint64 maxHeld = 0;
	int namebufsize = sizeof(namebuf);

	/* Get the root context's stat and pass it to the MemoryContextStats_recur for printing */
	(*context->methods.stats)(context, &nBlocks, &nChunks, &currentAvailable, &allAllocated, &allFreed, &maxHeld);
	name = MemoryContextName(context, context, namebuf, namebufsize);

    MemoryContextStats_recur(context, context, name, namebuf, namebufsize, nBlocks, nChunks,
    		currentAvailable, allAllocated, allFreed, maxHeld);
}

/*
 * MemoryContextCheck
 *		Check all chunks in the named context.
 *
 * This is just a debugging utility, so it's not fancy.
 */
#ifdef MEMORY_CONTEXT_CHECKING
void
MemoryContextCheck(MemoryContext context)
{
	MemoryContext child;

	AssertArg(MemoryContextIsValid(context));

	(*context->methods.check) (context);
	for (child = context->firstchild; child != NULL; child = child->nextchild)
		MemoryContextCheck(child);
}
#endif

/*
 * MemoryContextContains
 *		Detect whether an allocated chunk of memory belongs to a given
 *		context or not.
 *
 * Note: this test assumes that the pointer was allocated using palloc.
 * If unsure, please use the generic version (MemoryContextContainsGenericAllocation).
 *
 * Caution: this test is reliable as long as the 'pointer' does point to
 * a chunk of memory allocated from *some* context.  If 'pointer' points
 * at memory obtained in some other way, there is a small chance of a
 * false-positive result since the bits right before it might look like
 * a valid chunk header by chance. In the latter case (when the memory
 * was not palloc'ed), we are more likely to crash. Please use the generic
 * version of this method if you have any doubt that the tested memory
 * region may not be palloc'ed.
 */
bool
MemoryContextContains(MemoryContext context, void *pointer)
{
	StandardChunkHeader *header;

	/*
	 * Try to detect bogus pointers handed to us, poorly though we can.
	 * Presumably, a pointer that isn't MAXALIGNED isn't pointing at an
	 * allocated chunk.
	 */
	if (pointer == NULL || pointer != (void *) MAXALIGN(pointer))
	{
		return false;
	}

	/*
	 * OK, it's probably safe to look at the chunk header.
	 */
	header = (StandardChunkHeader *)
		((char *) pointer - STANDARDCHUNKHEADERSIZE);

	if (header->sharedHeader == NULL || (void*)header->sharedHeader != (void *) MAXALIGN(header->sharedHeader) || !AllocSizeIsValid(header->size))
	{
		return false;
	}

	SharedChunkHeader *sharedHeader = (SharedChunkHeader *)header->sharedHeader;

	/*
	 * If the context link doesn't match then we certainly have a non-member
	 * chunk.  Also check for a reasonable-looking size as extra guard against
	 * being fooled by bogus pointers.
	 */
	if (sharedHeader->context == context)
	{
		return true;
	}
	return false;
}

/*
 * MemoryContextContainsGenericAllocation
 *		Detects whether a generic (may or may not be allocated by
 *		palloc) chunk of memory belongs to a given context or not.
 *		Note, the "generic" means it will be ready to
 *		handle chunks not allocated using palloc.
 *
 * Caution: this test has the same problem as MemoryContextContains
 * 		where it can falsely detect a chunk belonging to a context,
 * 		while it does not. In addition, it can also falsely conclude
 * 		that a chunk does *not* belong to a context, while in reality
 * 		it does. The latter weakness stems from its versatility to
 * 		handle non-palloc'ed chunks.
 */
bool
MemoryContextContainsGenericAllocation(MemoryContext context, void *pointer)
{
	StandardChunkHeader *header;

	/*
	 * Try to detect bogus pointers handed to us, poorly though we can.
	 * Presumably, a pointer that isn't MAXALIGNED isn't pointing at an
	 * allocated chunk.
	 */
	if (pointer == NULL || pointer != (void *) MAXALIGN(pointer))
	{
		return false;
	}

	/*
	 * OK, it's probably safe to look at the chunk header.
	 */
	header = (StandardChunkHeader *)
		((char *) pointer - STANDARDCHUNKHEADERSIZE);

	AllocSet set = (AllocSet)context;

	if (header->sharedHeader == set->sharedHeaderList ||
			(set->sharedHeaderList != NULL && set->sharedHeaderList->next == header->sharedHeader) ||
			(set->sharedHeaderList != NULL && set->sharedHeaderList->next != NULL && set->sharedHeaderList->next->next == header->sharedHeader))
	{
		/*
		 * At this point we know that one of the sharedHeader pointers of the
		 * provided context (AllocSet) is the same as the sharedHeader
		 * pointer of the provided chunk. Therefore, the chunk should
		 * belong to the AllocSet (with a false positive chance coming
		 * from some third party allocated memory region having the
		 * same value as the sharedHeaderList pointer address
		 */
		return true;
	}

	/*
	 * We might falsely conclude that the chunk does not belong
	 * to the context, if we fail to match the chunk's sharedHeader
	 * pointer with one of the leading sharedHeader pointers in the
	 * context's sharedHeaderList.
	 */
	return false;
}

/*--------------------
 * MemoryContextCreate
 *		Context-type-independent part of context creation.
 *
 * This is only intended to be called by context-type-specific
 * context creation routines, not by the unwashed masses.
 *
 * The context creation procedure is a little bit tricky because
 * we want to be sure that we don't leave the context tree invalid
 * in case of failure (such as insufficient memory to allocate the
 * context node itself).  The procedure goes like this:
 *	1.	Context-type-specific routine first calls MemoryContextCreate(),
 *		passing the appropriate tag/size/methods values (the methods
 *		pointer will ordinarily point to statically allocated data).
 *		The parent and name parameters usually come from the caller.
 *	2.	MemoryContextCreate() attempts to allocate the context node,
 *		plus space for the name.  If this fails we can ereport() with no
 *		damage done.
 *	3.	We fill in all of the type-independent MemoryContext fields.
 *	4.	We call the type-specific init routine (using the methods pointer).
 *		The init routine is required to make the node minimally valid
 *		with zero chance of failure --- it can't allocate more memory,
 *		for example.
 *	5.	Now we have a minimally valid node that can behave correctly
 *		when told to reset or delete itself.  We link the node to its
 *		parent (if any), making the node part of the context tree.
 *	6.	We return to the context-type-specific routine, which finishes
 *		up type-specific initialization.  This routine can now do things
 *		that might fail (like allocate more memory), so long as it's
 *		sure the node is left in a state that delete will handle.
 *
 * This protocol doesn't prevent us from leaking memory if step 6 fails
 * during creation of a top-level context, since there's no parent link
 * in that case.  However, if you run out of memory while you're building
 * a top-level context, you might as well go home anyway...
 *
 * Normally, the context node and the name are allocated from
 * TopMemoryContext (NOT from the parent context, since the node must
 * survive resets of its parent context!).	However, this routine is itself
 * used to create TopMemoryContext!  If we see that TopMemoryContext is NULL,
 * we assume we are creating TopMemoryContext and use malloc() to allocate
 * the node.
 *
 * Note that the name field of a MemoryContext does not point to
 * separately-allocated storage, so it should not be freed at context
 * deletion.
 *--------------------
 */
MemoryContext
MemoryContextCreate(NodeTag tag, Size size,
					MemoryContextMethods *methods,
					MemoryContext parent,
					const char *name)
{
	MemoryContext node;
	Size		needed = size + strlen(name) + 1;

	/* Get space for node and name */
	if (TopMemoryContext != NULL)
	{
		/* Normal case: allocate the node in TopMemoryContext */
		node = (MemoryContext) MemoryContextAlloc(TopMemoryContext,
												  needed);
	}
	else
	{
		/* Special case for startup: use good ol' malloc */
		node = (MemoryContext) malloc(needed);
		if(!node)
			ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY),
				errmsg("Failed to create memory context: out of memory")
				));
	}

	/* Initialize the node as best we can */
	MemSet(node, 0, size);
	node->type = tag;
	node->methods = *methods;
	node->parent = parent;
	node->firstchild = NULL;
	node->nextchild = NULL;
	node->name = ((char *) node) + size;
	strcpy(node->name, name);

	/* Type-specific routine finishes any other essential initialization */
	(*node->methods.init) (node);

	/* OK to link node to parent (if any) */
	if (parent)
	{
		node->nextchild = parent->firstchild;
		parent->firstchild = node;
	}

	/* Return to type-specific creation routine to finish up */
	return node;
}

/*
 * MemoryContextAlloc
 *		Allocate space within the specified context.
 *
 * This could be turned into a macro, but we'd have to import
 * nodes/memnodes.h into postgres.h which seems a bad idea.
 */
void *
MemoryContextAllocImpl(MemoryContext context, Size size, const char* sfile, const char *sfunc, int sline)
{
	void *ret;

#ifdef PGTRACE_ENABLED
	StandardChunkHeader *header;
#endif
	AssertArg(MemoryContextIsValid(context));

#ifdef CDB_PALLOC_CALLER_ID
	context->callerFile = sfile;
	context->callerLine = sline;
#endif

	if (!AllocSizeIsValid(size))
		MemoryContextError(ERRCODE_INTERNAL_ERROR,
				context, CDB_MCXT_WHERE(context),
				"invalid memory alloc request size %lu",
				(unsigned long)size);

	ret = (*context->methods.alloc) (context, size);
#ifdef PGTRACE_ENABLED
	header = (StandardChunkHeader *)
		((char *) ret - STANDARDCHUNKHEADERSIZE);
	PG_TRACE5(memctxt__alloc, size, header->size, 0, 0, (long) context->name);
#endif

	return ret;
}

/*
 * MemoryContextAllocZero
 *		Like MemoryContextAlloc, but clears allocated memory
 *
 *	We could just call MemoryContextAlloc then clear the memory, but this
 *	is a very common combination, so we provide the combined operation.
 */
void *
MemoryContextAllocZeroImpl(MemoryContext context, Size size, const char* sfile, const char *sfunc, int sline)
{
	void	   *ret;

#ifdef PGTRACE_ENABLED
	StandardChunkHeader *header;
#endif
	AssertArg(MemoryContextIsValid(context));

#ifdef CDB_PALLOC_CALLER_ID
	context->callerFile = sfile;
	context->callerLine = sline;
#endif

	if (!AllocSizeIsValid(size))
		MemoryContextError(ERRCODE_INTERNAL_ERROR,
				context, CDB_MCXT_WHERE(context),
				"invalid memory alloc request size %lu",
				(unsigned long)size);

	ret = (*context->methods.alloc) (context, size);

	MemSetAligned(ret, 0, size);

#ifdef PGTRACE_ENABLED
	header = (StandardChunkHeader *)
		((char *) ret - STANDARDCHUNKHEADERSIZE);
	PG_TRACE5(memctxt__alloc, size, header->size, 0, 0, (long) context->name);
#endif

	return ret;
}

/*
 * MemoryContextAllocZeroAligned
 *		MemoryContextAllocZero where length is suitable for MemSetLoop
 *
 *	This might seem overly specialized, but it's not because newNode()
 *	is so often called with compile-time-constant sizes.
 */
void *
MemoryContextAllocZeroAlignedImpl(MemoryContext context, Size size, const char* sfile, const char *sfunc, int sline)
{
	void	   *ret;

#ifdef PGTRACE_ENABLED
	StandardChunkHeader *header;
#endif

	AssertArg(MemoryContextIsValid(context));

#ifdef CDB_PALLOC_CALLER_ID
	context->callerFile = sfile;
	context->callerLine = sline;
#endif

	if (!AllocSizeIsValid(size))
		MemoryContextError(ERRCODE_INTERNAL_ERROR,
				context, CDB_MCXT_WHERE(context),
				"invalid memory alloc request size %lu",
				(unsigned long)size);

	ret = (*context->methods.alloc) (context, size);

	MemSetLoop(ret, 0, size);

#ifdef PGTRACE_ENABLED
	header = (StandardChunkHeader *)
		((char *) ret - STANDARDCHUNKHEADERSIZE);
	PG_TRACE5(memctxt__alloc, size, header->size, 0, 0, (long) context->name);
#endif

	return ret;
}

/*
 * pfree
 *		Release an allocated chunk.
 */
void
MemoryContextFreeImpl(void *pointer, const char *sfile, const char *sfunc, int sline)
{
	/*
	 * Try to detect bogus pointers handed to us, poorly though we can.
	 * Presumably, a pointer that isn't MAXALIGNED isn't pointing at an
	 * allocated chunk.
	 */
	Assert(pointer != NULL);
	Assert(pointer == (void *) MAXALIGN(pointer));

	/*
	 * OK, it's probably safe to look at the chunk header.
	 */
	StandardChunkHeader* header = (StandardChunkHeader *)
		((char *) pointer - STANDARDCHUNKHEADERSIZE);

	AssertArg(MemoryContextIsValid(header->sharedHeader->context));

#ifdef PGTRACE_ENABLED
	PG_TRACE5(memctxt__free, 0, 0, 
#ifdef MEMORY_CONTEXT_CHECKING
		header->requested_size, header->size,
#else
		0, header->size, 
#endif
		(long) header->sharedHeader->context->name);
#endif

#ifdef CDB_PALLOC_CALLER_ID
	header->sharedHeader->context->callerFile = sfile;
	header->sharedHeader->context->callerLine = sline;
#endif

	if (header->sharedHeader->context->methods.free_p)
		(*header->sharedHeader->context->methods.free_p) (header->sharedHeader->context, pointer);
	else
		Assert(header);   /* this assert never fails. Just here so we can set breakpoint in debugger. */
}

/*
 * repalloc
 *		Adjust the size of a previously allocated chunk.
 */
void *
MemoryContextReallocImpl(void *pointer, Size size, const char *sfile, const char *sfunc, int sline)
{
	StandardChunkHeader *header;
	void *ret;

#ifdef PGTRACE_ENABLED 
	long old_reqsize;
	long old_size;
#endif

	/*
	 * Try to detect bogus pointers handed to us, poorly though we can.
	 * Presumably, a pointer that isn't MAXALIGNED isn't pointing at an
	 * allocated chunk.
	 */
	Assert(pointer != NULL);
	Assert(pointer == (void *) MAXALIGN(pointer));

	/*
	 * OK, it's probably safe to look at the chunk header.
	 */
	header = (StandardChunkHeader *)
		((char *) pointer - STANDARDCHUNKHEADERSIZE);

	AssertArg(MemoryContextIsValid(header->sharedHeader->context));

#ifdef PGTRACE_ENABLED
#ifdef MEMORY_CONTEXT_CHECKING
	old_reqsize = header->requested_size;
#else
	old_reqsize = 0;
#endif
	old_size = header->size;
#endif

#ifdef CDB_PALLOC_CALLER_ID
	header->sharedHeader->context->callerFile = sfile;
	header->sharedHeader->context->callerLine = sline;
#endif

	if (!AllocSizeIsValid(size))
		MemoryContextError(ERRCODE_INTERNAL_ERROR,
				header->sharedHeader->context, CDB_MCXT_WHERE(header->sharedHeader->context),
				"invalid memory alloc request size %lu",
				(unsigned long)size);

	ret = (*header->sharedHeader->context->methods.realloc) (header->sharedHeader->context, pointer, size);

#ifdef PGTRACE_ENABLED
	header = (StandardChunkHeader *)
		((char *) ret - STANDARDCHUNKHEADERSIZE);
	PG_TRACE5(memctxt__realloc, size, header->size, old_reqsize, old_size, (long) header->sharedHeader->context->name);
#endif

	return ret;
}


/*
 * MemoryContextStrdup
 *		Like strdup(), but allocate from the specified context
 */
char *
MemoryContextStrdup(MemoryContext context, const char *string)
{
	char	   *nstr;
	Size		len = strlen(string) + 1;

	nstr = (char *) MemoryContextAlloc(context, len);

	memcpy(nstr, string, len);

	return nstr;
}

/*
 * pnstrdup
 *		Like pstrdup(), but append null byte to a
 *		not-necessarily-null-terminated input string.
 */
char *
pnstrdup(const char *in, Size len)
{
	char	   *out = palloc(len + 1);

	memcpy(out, in, len);
	out[len] = '\0';
	return out;
}


/*
 * floor_log2_Size
 */
int floor_log2_Size(Size sz)
{
    return floor_log2_Size_inline(sz);
}

/*
 * ceil_log2_Size
 */
int ceil_log2_Size(Size sz)
{
    return ceil_log2_Size_inline(sz);
}


#if defined(WIN32) || defined(__CYGWIN__)
/*
 *	Memory support routines for libpgport on Win32
 *
 *	Win32 can't load a library that PGDLLIMPORTs a variable
 *	if the link object files also PGDLLIMPORT the same variable.
 *	For this reason, libpgport can't reference CurrentMemoryContext
 *	in the palloc macro calls.
 *
 *	To fix this, we create several functions here that allow us to
 *	manage memory without doing the inline in libpgport.
 */
void *
pgport_palloc(Size sz)
{
	return palloc(sz);
}


char *
pgport_pstrdup(const char *str)
{
	return pstrdup(str);
}


/* Doesn't reference a PGDLLIMPORT variable, but here for completeness. */
void
pgport_pfree(void *pointer)
{
	pfree(pointer);
}

#endif
