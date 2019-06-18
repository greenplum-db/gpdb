/*-------------------------------------------------------------------------
 *
 * storage_tablespace.h
 *	  prototypes for tablespace support for backend/catalog/storage.c
 *
 * src/include/catalog/storage_tablespace.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef STORAGE_TABLESPACE_H
#define STORAGE_TABLESPACE_H


#include "postgres.h"


extern void TablespaceStorageInit(void (*unlink_tablespace_dir)(Oid tablespace_dir, bool isRedo));
extern void TablespaceCreateStorage(Oid tablespaceoid);
extern Oid smgrGetPendingTablespaceForDeletion(void);
extern void smgrDoPendingTablespaceDeletion(void);
extern void smgrDoTablespaceDeletion(Oid tablespace_to_delete);


#endif // STORAGE_TABLESPACE_H
