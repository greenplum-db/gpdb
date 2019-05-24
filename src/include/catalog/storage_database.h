/*-------------------------------------------------------------------------
 *
 * storage_database.h
 *	  prototypes for functions in backend/catalog/storage_database.c
 *
 * src/include/catalog/storage_database.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef STORAGE_DATABASE_H
#define STORAGE_DATABASE_H

#include "postgres.h"
#include "postgres_ext.h"
#include "storage/dbdirnode.h"

extern void ScheduleDbDirDelete(Oid db_id, Oid tablespace_oid, bool forCommit);
extern void DoPendingDbDeletes(bool isCommit);
extern int	GetPendingDbDeletes(bool forCommit, DbDirNode **ptr);
extern void DropDatabaseDirectories(DbDirNode *deldbs, int ndeldbs, bool isRedo);
extern void PostPrepare_DatabaseStorage(void);
extern void MoveDbSessionLockAcquire(Oid db_id);
extern void MoveDbSessionLockRelease(void);
extern void DatabaseStorageResetSessionLock(void);
#endif   /* STORAGE_DATABASE_H */
