/*-------------------------------------------------------------------------
 *
 * storage.h
 *	  prototypes for functions in backend/catalog/storage.c
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/catalog/storage.h,v 1.3 2009/01/01 17:23:58 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef STORAGE_H
#define STORAGE_H

#include "access/xlog.h"
#include "lib/stringinfo.h"
#include "storage/block.h"
#include "storage/relfilenode.h"
#include "utils/relcache.h"

extern void RelationCreateStorage(RelFileNode rnode, bool isLocalBuf,
					  char *relationName,
					  MirrorDataLossTrackingState mirrorDataLossTrackingState,
					  int64 mirrorDataLossTrackingSessionNum,
					  bool *mirrorDataLossOccurred);

extern void RelationDropStorage(RelFileNode *relFileNode,
					int32 segmentFileNum,
					PersistentFileSysRelStorageMgr relStorageMgr,
					bool isLocalBuf,
					char *relationName,
					ItemPointer persistentTid,
					int64 persistentSerialNum);
extern void RelationTruncate(Relation rel, BlockNumber nblocks, bool markPersistentAsPhysicallyTruncated);

/*
 * These functions used to be in storage/smgr/smgr.c, which explains the
 * naming
 */
extern void AtEOXact_smgr(bool isCommit);
extern int smgrGetPendingFileSysWork(EndXactRecKind endXactRecKind,
									 PersistentEndXactFileSysActionInfo **ptr);
extern void AtSubCommit_smgr(void);
extern void AtSubAbort_smgr(void);
extern void PostPrepare_smgr(void);

extern void smgr_redo(XLogRecPtr beginLoc, XLogRecPtr lsn, XLogRecord *record);
extern void smgr_desc(StringInfo buf, XLogRecPtr beginLoc, XLogRecord *record);

#endif   /* STORAGE_H */
