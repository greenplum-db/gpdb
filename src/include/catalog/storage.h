/*-------------------------------------------------------------------------
 *
 * storage.h
 *	  prototypes for functions in backend/catalog/storage.c
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/catalog/storage.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef STORAGE_H
#define STORAGE_H

#include "access/xlog.h"
#include "storage/block.h"
#include "storage/relfilenode.h"
#include "storage/smgr.h"
#include "utils/relcache.h"

/* Pending delete node linked to xact it created */
typedef struct PendingRelXactDelete
{
	RelFileNodePendingDelete relnode;
	TransactionId xid;
}			PendingRelXactDelete;


typedef struct PendingRelXactDeleteArray
{
	size_t		count;
	PendingRelXactDelete array[FLEXIBLE_ARRAY_MEMBER];
}			PendingRelXactDeleteArray;

extern Size PendingDeleteShmemSize(void);
extern void PendingDeleteShmemInit(void);
extern XLogRecPtr PendingDeleteXLogInsert(void);
extern void PendingDeleteRedoRecord(XLogReaderState *record);
extern void PendingDeleteRedoRemove(TransactionId xid);
extern void PendingDeleteRedoDropFiles(void);

extern SMgrRelation RelationCreateStorage(RelFileNode rnode,
										  char relpersistence,
										  SMgrImpl smgr_which);
extern void RelationDropStorage(Relation rel);
extern void RelationPreserveStorage(RelFileNode rnode, bool atCommit);
extern void RelationTruncate(Relation rel, BlockNumber nblocks);
extern void RelationCopyStorage(SMgrRelation src, SMgrRelation dst,
								ForkNumber forkNum, char relpersistence);

/*
 * These functions used to be in storage/smgr/smgr.c, which explains the
 * naming
 */
extern void smgrDoPendingDeletes(bool isCommit);
extern int	smgrGetPendingDeletes(bool forCommit, RelFileNodePendingDelete **ptr);
extern void AtSubCommit_smgr(void);
extern void AtSubAbort_smgr(void);
extern void PostPrepare_smgr(void);

#endif							/* STORAGE_H */
