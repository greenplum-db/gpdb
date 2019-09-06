/*-------------------------------------------------------------------------
 *
 * storage_tablespace.h
 *	  prototypes for tablespace support for backend/catalog/storage_tablespace.c
 *
 * src/include/catalog/storage_tablespace.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef STORAGE_TABLESPACE_H
#define STORAGE_TABLESPACE_H


#include "postgres.h"


extern void TablespaceStorageInit(void (*unlink_tablespace_dir)(Oid tablespace_dir, bool isRedo));

extern void ScheduleTablespaceDirectoryDeletionForAbort(Oid tablespaceoid);
extern void UnscheduleTablespaceDirectoryDeletionForAbort(void);
extern Oid GetPendingTablespaceForDeletionForAbort(void);
extern void DoPendingTablespaceDeletionForAbort(void);

extern void ScheduleTablespaceDirectoryDeletionForCommit(Oid tablespaceoid);
extern void UnscheduleTablespaceDirectoryDeletionForCommit(void);
extern Oid GetPendingTablespaceForDeletionForCommit(void);
extern void DoPendingTablespaceDeletionForCommit(void);


extern void DoTablespaceDeletionForRedoXlog(Oid tablespace_oid_to_delete);


#endif // STORAGE_TABLESPACE_H
