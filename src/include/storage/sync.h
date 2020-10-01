/*-------------------------------------------------------------------------
 *
 * sync.h
 *	  File synchronization management code.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/storage/sync.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef SYNC_H
#define SYNC_H

#include "storage/relfilenode.h"

/*
 * Type of sync request.  These are used to manage the set of pending
 * requests to call a sync handler's sync or unlink functions at the next
 * checkpoint.
 */
typedef enum SyncRequestType
{
	SYNC_REQUEST,				/* schedule a call of sync function */
	SYNC_UNLINK_REQUEST,		/* schedule a call of unlink function */
	SYNC_FORGET_REQUEST,		/* forget all calls for a tag */
	SYNC_FILTER_REQUEST			/* forget all calls satisfying match fn */
} SyncRequestType;

/*
 * Which set of functions to use to handle a given request.  The values of
 * the enumerators must match the indexes of the function table in sync.c.
 */
typedef enum SyncRequestHandler
{
	SYNC_HANDLER_MD = 0,			/* md smgr */
	SYNC_HANDLER_AO = 1
} SyncRequestHandler;

/*
 * A tag identifying a file.  Currently it has the members required for md.c's
 * usage, but sync.c has no knowledge of the internal structure, and it is
 * liable to change as required by future handlers.
 */
typedef struct FileTag
{
	int16		handler;		/* SyncRequestHandler value, saving space */
	int16		forknum;		/* ForkNumber, saving space */
	RelFileNode rnode;
	uint32		segno;
	/*
	 * GPDB_12_MERGE_FIXME: Should the "is this AO table?" flag be put here?
	 * Do we need to keep track of whether this file backs an AO table? GPDB 6
	 * has logic in md/smgr layer to optimize operations on AO tables.
	 *
	 * For example:
	 *   1. delaying fsync on AO tables
	 *      (commit 6bd76f70450)
	 *   2. skipping heap specific functions like ForgetRelationFsyncRequests()
	 *      and DropRelFileNodeBuffers() when operating on AO tables
	 *      (commit 85fee7365d2)
	 *   3. efficient unlinking of AO tables
	 *      (commit 8838ac983c6)
	 *
	 * Seems like these features should come through the tableam interface.
	 * Let's try to do that and delete this diff from upstream.
	 */
	/*bool		is_ao_segno;*/
} FileTag;

#define INIT_FILETAG(a,xx_rnode,xx_forknum,xx_segno,xx_handler)	\
( \
	memset(&(a), 0, sizeof(FileTag)), \
	(a).handler = (xx_handler),	\
	(a).rnode = (xx_rnode), \
	(a).forknum = (xx_forknum), \
	(a).segno = (xx_segno) \
)

extern void InitSync(void);
extern void SyncPreCheckpoint(void);
extern void SyncPostCheckpoint(void);
extern void ProcessSyncRequests(void);
extern void RememberSyncRequest(const FileTag *ftag, SyncRequestType type);
extern void EnableSyncRequestForwarding(void);
extern bool RegisterSyncRequest(const FileTag *ftag, SyncRequestType type,
								bool retryOnError);

#endif							/* SYNC_H */
