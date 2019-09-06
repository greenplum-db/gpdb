/*-------------------------------------------------------------------------
 *
 * xact_storage_tablespace.h
 *
 *	  Hooks to be implemented for transactions for tablespace storage:
 *
 *
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 * src/include/access/xact_storage_tablespace.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef ACCESS_XACT_STORAGE_TABLESPACE_H
#define ACCESS_XACT_STORAGE_TABLESPACE_H


void AtCommit_TablespaceStorage(void);
void AtAbort_TablespaceStorage(void);


#endif // ACCESS_XACT_STORAGE_TABLESPACE_H