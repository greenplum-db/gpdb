/*-------------------------------------------------------------------------
 *
 * storage_tablespace_twophase.c
 *
 *	  implement hooks for twophase and tablespace storage
 *
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	  src/backend/catalog/storage_tablespace_twophase.c
 *
 *-------------------------------------------------------------------------
 */
#include "catalog/storage_tablespace.h"
#include "access/twophase_storage_tablespace.h"


void
AtTwoPhaseCommit_TablespaceStorage()
{
	DoPendingTablespaceDeletionForCommit();
	UnscheduleTablespaceDirectoryDeletionForAbort();
}


void
AtTwoPhaseAbort_TablespaceStorage()
{
	DoPendingTablespaceDeletionForAbort();
	UnscheduleTablespaceDirectoryDeletionForCommit();
}
