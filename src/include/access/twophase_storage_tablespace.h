/*-------------------------------------------------------------------------
 *
 * twophase_storage_tablespace.h
 *
 *	  Hooks to be implemented for twophase for tablespace storage:
 *
 *
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 * src/include/access/twophase_storage_tablespace.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef ACCESS_TWOPHASE_STORAGE_TABLESPACE_H
#define ACCESS_TWOPHASE_STORAGE_TABLESPACE_H


void AtTwoPhaseCommit_TablespaceStorage(void);
void AtTwoPhaseAbort_TablespaceStorage(void);


#endif //ACCESS_TWOPHASE_STORAGE_TABLESPACE_H
