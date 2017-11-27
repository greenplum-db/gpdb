/*-------------------------------------------------------------------------
 *
 * bufmgr.h
 *	  POSTGRES buffer manager definitions.
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/storage/bufmgr.h,v 1.119 2009/01/01 17:24:01 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef BUFMGR_H
#define BUFMGR_H

#include "cdb/cdbfilerepprimary.h"
#include "miscadmin.h"
#include "storage/block.h"
#include "storage/buf.h"
#include "storage/buf_internals.h"
#include "storage/bufpage.h"
#include "storage/relfilenode.h"
#include "storage/smgr.h"
#include "utils/relcache.h"

typedef void *Block;

/* Possible arguments for GetAccessStrategy() */
typedef enum BufferAccessStrategyType
{
	BAS_NORMAL,					/* Normal random access */
	BAS_BULKREAD,				/* Large read-only scan (hint bit updates are
								 * ok) */
	BAS_BULKWRITE,				/* Large multi-block write (e.g. COPY IN) */
	BAS_VACUUM					/* VACUUM */
} BufferAccessStrategyType;

/* Possible modes for ReadBufferExtended() */
typedef enum
{
	RBM_NORMAL,			/* Normal read */
	RBM_ZERO,			/* Don't read from disk, caller will initialize */
	RBM_ZERO_ON_ERROR	/* Read, but return an all-zeros page on error */
} ReadBufferMode;

/* in globals.c ... this duplicates miscadmin.h */
extern PGDLLIMPORT int NBuffers;

/* in bufmgr.c */

/* should not be accessed directly.  Use ShouldMemoryProtectBufferPool() instead */
extern bool memory_protect_buffer_pool;
extern bool zero_damaged_pages;
extern int	bgwriter_lru_maxpages;
extern double bgwriter_lru_multiplier;
extern bool bgwriter_flush_all_buffers;

extern PGDLLIMPORT bool IsUnderPostmaster; /* from utils/init/globals.c */
#define ShouldMemoryProtectBufferPool() (memory_protect_buffer_pool && IsUnderPostmaster)

/* in buf_init.c */
extern PGDLLIMPORT char *BufferBlocks;
extern PGDLLIMPORT int32 *PrivateRefCount;

/* in localbuf.c */
extern PGDLLIMPORT int NLocBuffer;
extern PGDLLIMPORT Block *LocalBufferBlockPointers;
extern PGDLLIMPORT int32 *LocalRefCount;

/* special block number for ReadBuffer() */
#define P_NEW	InvalidBlockNumber		/* grow the file to get a new page */

/*
 * Buffer content lock modes (mode argument for LockBuffer())
 */
#define BUFFER_LOCK_UNLOCK		0
#define BUFFER_LOCK_SHARE		1
#define BUFFER_LOCK_EXCLUSIVE	2

/*
 * These routines are beaten on quite heavily, hence the macroization.
 */

/*
 * BufferIsValid
 *		True iff the given buffer number is valid (either as a shared
 *		or local buffer).
 *
 * This is not quite the inverse of the BufferIsInvalid() macro, since this
 * adds sanity rangechecks on the buffer number.
 *
 * Note: For a long time this was defined the same as BufferIsPinned,
 * that is it would say False if you didn't hold a pin on the buffer.
 * I believe this was bogus and served only to mask logic errors.
 * Code should always know whether it has a buffer reference,
 * independently of the pin state.
 */
#define BufferIsValid(bufnum) \
( \
	(bufnum) != InvalidBuffer && \
	(bufnum) >= -NLocBuffer && \
	(bufnum) <= NBuffers \
)

/*
 * BufferIsPinned
 *		True iff the buffer is pinned (also checks for valid buffer number).
 *
 *		NOTE: what we check here is that *this* backend holds a pin on
 *		the buffer.  We do not care whether some other backend does.
 */
#define BufferIsPinned(bufnum) \
( \
	!BufferIsValid(bufnum) ? \
		false \
	: \
		BufferIsLocal(bufnum) ? \
			(LocalRefCount[-(bufnum) - 1] > 0) \
		: \
			(PrivateRefCount[(bufnum) - 1] > 0) \
)

/*
 * BufferGetBlock
 *		Returns a reference to a disk page image associated with a buffer.
 *
 * Note:
 *		Assumes buffer is valid.
 */
#define BufferGetBlock(buffer) \
( \
	AssertMacro(BufferIsValid(buffer)), \
	BufferIsLocal(buffer) ? \
		LocalBufferBlockPointers[-(buffer) - 1] \
	: \
		(Block) (BufferBlocks + ((Size) ((buffer) - 1)) * BLCKSZ) \
)

// Helper DEFINEs for MirroredLock within Buffer Pool Manager ONLY.
#ifdef USE_ASSERT_CHECKING

typedef struct MirroredLockBufMgrLocalVars
{
	bool mirroredLockIsHeldByMe;

	bool specialResyncManagerFlag;

	bool mirroredVariablesSet;
} MirroredLockBufMgrLocalVars;

#define MIRROREDLOCK_BUFMGR_DECLARE \
	MirroredLockBufMgrLocalVars mirroredLockBufMgrLocalVars = {false, false, false};
#else

typedef struct MirroredLockBufMgrLocalVars
{
	bool mirroredLockIsHeldByMe;

	bool specialResyncManagerFlag;
} MirroredLockBufMgrLocalVars;

#define MIRROREDLOCK_BUFMGR_DECLARE \
	MirroredLockBufMgrLocalVars mirroredLockBufMgrLocalVars = {false, false};
#endif

#ifdef USE_ASSERT_CHECKING
#define MIRROREDLOCK_BUFMGR_LOCK \
	{ \
		mirroredLockBufMgrLocalVars.mirroredLockIsHeldByMe = LWLockHeldByMe(MirroredLock); \
		mirroredLockBufMgrLocalVars.specialResyncManagerFlag = FileRepPrimary_IsResyncManagerOrWorker(); \
		mirroredLockBufMgrLocalVars.mirroredVariablesSet = true; \
		\
		if (!mirroredLockBufMgrLocalVars.mirroredLockIsHeldByMe) \
		{ \
			if (!mirroredLockBufMgrLocalVars.specialResyncManagerFlag) \
			{ \
				LWLockAcquire(MirroredLock , LW_SHARED); \
			} \
			else \
			{ \
				HOLD_INTERRUPTS(); \
			} \
		} \
		\
		Assert(InterruptHoldoffCount > 0); \
	}
#else
#define MIRROREDLOCK_BUFMGR_LOCK \
	{ \
		mirroredLockBufMgrLocalVars.mirroredLockIsHeldByMe = LWLockHeldByMe(MirroredLock); \
		mirroredLockBufMgrLocalVars.specialResyncManagerFlag = FileRepPrimary_IsResyncManagerOrWorker(); \
		\
		if (!mirroredLockBufMgrLocalVars.mirroredLockIsHeldByMe) \
		{ \
			if (!mirroredLockBufMgrLocalVars.specialResyncManagerFlag) \
			{ \
				LWLockAcquire(MirroredLock , LW_SHARED); \
			} \
			else \
			{ \
				HOLD_INTERRUPTS(); \
			} \
		} \
	}
#endif

#ifdef USE_ASSERT_CHECKING
#define MIRROREDLOCK_BUFMGR_UNLOCK \
	{ \
		Assert(mirroredLockBufMgrLocalVars.mirroredVariablesSet); \
		Assert(InterruptHoldoffCount > 0); \
		\
		if (!mirroredLockBufMgrLocalVars.mirroredLockIsHeldByMe) \
		{ \
			if (!mirroredLockBufMgrLocalVars.specialResyncManagerFlag) \
			{ \
				LWLockRelease(MirroredLock); \
			} \
			else \
			{ \
				RESUME_INTERRUPTS(); \
			} \
		} \
	}
#else
#define MIRROREDLOCK_BUFMGR_UNLOCK \
	{ \
		if (!mirroredLockBufMgrLocalVars.mirroredLockIsHeldByMe) \
		{ \
			if (!mirroredLockBufMgrLocalVars.specialResyncManagerFlag) \
			{ \
				LWLockRelease(MirroredLock); \
			} \
			else \
			{ \
				RESUME_INTERRUPTS(); \
			} \
		} \
	}
#endif


#define MIRROREDLOCK_BUFMGR_MUST_ALREADY_BE_HELD \
	{ \
		bool alreadyMirroredLockIsHeldByMe; \
		bool alreadySpecialResyncManagerFlag; \
		\
		alreadyMirroredLockIsHeldByMe = LWLockHeldByMe(MirroredLock); \
		alreadySpecialResyncManagerFlag = FileRepPrimary_IsResyncManagerOrWorker(); \
		if (!alreadyMirroredLockIsHeldByMe && !alreadySpecialResyncManagerFlag) \
			elog(ERROR, "Mirrored lock must already be held"); \
	}

#define MIRROREDLOCK_BUFMGR_MUST_ALREADY_BE_HELD_BUF(buf) \
	{ \
		if (!BufferIsLocal(buf)) \
		{ \
			volatile BufferDesc *bufHdr; \
			bufHdr = &BufferDescriptors[buffer - 1]; \
			if (bufHdr->tag.forkNum == MAIN_FORKNUM) \
				MIRROREDLOCK_BUFMGR_MUST_ALREADY_BE_HELD; \
		} \
	}

#ifdef USE_ASSERT_CHECKING
#define MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_DECLARE \
	bool verifyMirroredLockIsHeldByMe = false; \
	bool verifyMirroredVariablesSet = false;
#else
#define MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_DECLARE \
	bool verifyMirroredLockIsHeldByMe = false;
#endif

#ifdef USE_ASSERT_CHECKING
#define MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_ENTER \
	{ \
		verifyMirroredLockIsHeldByMe = LWLockHeldByMe(MirroredLock); \
		verifyMirroredVariablesSet = true; \
	}
#else
#define MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_ENTER \
	{ \
		verifyMirroredLockIsHeldByMe = LWLockHeldByMe(MirroredLock); \
	}
#endif

#ifdef USE_ASSERT_CHECKING
#define MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_EXIT \
	{ \
		bool currentMirroredLockIsHeldByMe; \
		\
		Assert(verifyMirroredVariablesSet); \
		currentMirroredLockIsHeldByMe = LWLockHeldByMe(MirroredLock); \
		if (verifyMirroredLockIsHeldByMe != currentMirroredLockIsHeldByMe) \
			elog(ERROR, "Exiting with mirrored lock held / not held different (enter %s, exit %s)", \
				 (verifyMirroredLockIsHeldByMe ? "true" : "false"), \
				 (currentMirroredLockIsHeldByMe ? "true" : "false")); \
	}
#else
#define MIRROREDLOCK_BUFMGR_VERIFY_NO_LOCK_LEAK_EXIT \
	{ \
		bool currentMirroredLockIsHeldByMe; \
		\
		currentMirroredLockIsHeldByMe = LWLockHeldByMe(MirroredLock); \
		if (verifyMirroredLockIsHeldByMe != currentMirroredLockIsHeldByMe) \
			elog(ERROR, "Exiting with mirrored lock held / not held different (enter %s, exit %s)", \
				 (verifyMirroredLockIsHeldByMe ? "true" : "false"), \
				 (currentMirroredLockIsHeldByMe ? "true" : "false")); \
	}
#endif

/*
 * BufferGetPageSize
 *		Returns the page size within a buffer.
 *
 * Notes:
 *		Assumes buffer is valid.
 *
 *		The buffer can be a raw disk block and need not contain a valid
 *		(formatted) disk page.
 */
/* XXX should dig out of buffer descriptor */
#define BufferGetPageSize(buffer) \
( \
	AssertMacro(BufferIsValid(buffer)), \
	(Size)BLCKSZ \
)

/*
 * BufferGetPage
 *		Returns the page associated with a buffer.
 */
#define BufferGetPage(buffer) ((Page)BufferGetBlock(buffer))

/*
 * prototypes for functions in bufmgr.c
 */
extern Buffer ReadBuffer(Relation reln, BlockNumber blockNum);
extern Buffer ReadBufferExtended(Relation reln, ForkNumber forkNum,
								 BlockNumber blockNum, ReadBufferMode mode,
								 BufferAccessStrategy strategy);
extern Buffer ReadBuffer_Resync(SMgrRelation reln, BlockNumber blockNum);
extern Buffer ReadBufferWithoutRelcache(RelFileNode rnode, bool isLocalBuf,
						ForkNumber forkNum, BlockNumber blockNum,
						ReadBufferMode mode, BufferAccessStrategy strategy);
extern void ReleaseBuffer(Buffer buffer);
extern void UnlockReleaseBuffer(Buffer buffer);
extern void MarkBufferDirty(Buffer buffer);
extern void IncrBufferRefCount(Buffer buffer);
extern Buffer ReleaseAndReadBuffer(Buffer buffer, Relation relation,
					 BlockNumber blockNum);
extern void InitBufferPool(void);
extern void InitBufferPoolAccess(void);
extern void InitBufferPoolBackend(void);
extern char *ShowBufferUsage(void);
extern void ResetBufferUsage(void);
extern void AtEOXact_Buffers(bool isCommit);
extern void PrintBufferLeakWarning(Buffer buffer);
extern void CheckPointBuffers(int flags);
extern BlockNumber BufferGetBlockNumber(Buffer buffer);
extern BlockNumber RelationGetNumberOfBlocks(Relation relation);
extern void FlushRelationBuffers(Relation rel);
extern void FlushDatabaseBuffers(Oid dbid);
extern void DropRelFileNodeBuffers(RelFileNode rnode, ForkNumber forkNum,
					   bool istemp, BlockNumber firstDelBlock);
extern void DropDatabaseBuffers(Oid tbpoid, Oid dbid);
extern XLogRecPtr BufferGetLSNAtomic(Buffer buffer);

#ifdef NOT_USED
extern void PrintPinnedBufs(void);
#endif
extern Size BufferShmemSize(void);
extern void BufferGetTag(Buffer buffer, RelFileNode *rnode, 
						 ForkNumber *forknum, BlockNumber *blknum);

extern void MarkBufferDirtyHint(Buffer buffer, Relation relation);

extern void UnlockBuffers(void);
extern void LockBuffer(Buffer buffer, int mode);
extern bool ConditionalLockBuffer(Buffer buffer);
extern void LockBufferForCleanup(Buffer buffer);
extern bool ConditionalLockBufferForCleanup(Buffer buffer);

extern void AbortBufferIO(void);

extern void BufmgrCommit(void);
extern void BgBufferSync(void);

extern void AtProcExit_LocalBuffers(void);

/* in freelist.c */
extern BufferAccessStrategy GetAccessStrategy(BufferAccessStrategyType btype);
extern void FreeAccessStrategy(BufferAccessStrategy strategy);

#endif
