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
} FileTag;

#endif							/* SYNC_H */

