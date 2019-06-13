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
extern void ScheduleTablespaceDirectoryDeletion(Oid tablespaceoid);
extern void UnscheduleTablespaceDirectoryDeletion(void);

extern Oid GetPendingTablespaceForDeletion(void);
extern void DoPendingTablespaceDeletion(void);
extern void DoTablespaceDeletion(Oid tablespace_to_delete);


#endif // STORAGE_TABLESPACE_H
