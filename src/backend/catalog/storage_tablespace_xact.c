/*-------------------------------------------------------------------------
 *
 * storage_tablespace_xact.c
 *
 *	  implement hooks for transactions and tablespace storage
 *
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	  src/backend/catalog/storage_tablespace_xact.c
 *
 *-------------------------------------------------------------------------
 */


#include "access/xact_storage_tablespace.h"
#include "catalog/storage_tablespace.h"


/*
 * AtCommit_TablespaceStorage:
 *
 * Needs to happen before locks are released to ensure that no
 * concurrent sessions are using the tablespace storage.
 *
 */
void
AtCommit_TablespaceStorage(void)
{
	DoPendingTablespaceDeletionForCommit();
	UnscheduleTablespaceDirectoryDeletionForAbort();
}


/*
 * AtAbort_TablespaceStorage:
 *
 * Needs to happen before locks are released to ensure that no
 * concurrent sessions are using the tablespace storage.
 *
 */
void
AtAbort_TablespaceStorage(void)
{
	DoPendingTablespaceDeletionForAbort();
	UnscheduleTablespaceDirectoryDeletionForCommit();
}
