/*-------------------------------------------------------------------------
 *
 * procarray.h
 *	  POSTGRES process array definitions.
 *
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/storage/procarray.h,v 1.21 2008/03/26 16:20:48 alvherre Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef PROCARRAY_H
#define PROCARRAY_H

#include "storage/lock.h"
#include "utils/snapshot.h"

#include "cdb/cdbpublic.h"

struct DtxContextInfo;         /* cdb/cdbdtxcontextinfo.h */
struct SnapshotData;           /* utils/tqual.h */

extern Size ProcArrayShmemSize(void);
extern void CreateSharedProcArray(void);
extern void ProcArrayAdd(PGPROC *proc);
extern void ProcArrayRemove(PGPROC *proc, TransactionId latestXid);
extern bool ProcArrayEndTransaction(PGPROC *proc, TransactionId latestXid, bool isCommit);
extern void ProcArrayClearTransaction(PGPROC *proc, bool commit);
extern void ClearTransactionFromPgProc_UnderLock(PGPROC *proc, bool commit);

extern Snapshot GetSnapshotData(Snapshot snapshot, bool serializable);

extern bool TransactionIdIsInProgress(TransactionId xid);
extern bool TransactionIdIsActive(TransactionId xid);
extern TransactionId GetOldestXmin(bool allDbs, bool ignoreVacuum);

extern VirtualTransactionId *GetVirtualXIDsDelayingChkpt(int *nvxids);
extern bool HaveVirtualXIDsDelayingChkpt(VirtualTransactionId *vxids, int nvxids);

extern PGPROC *BackendPidGetProc(int pid);
extern int	BackendXidGetPid(TransactionId xid);
extern bool IsBackendPid(int pid);

extern VirtualTransactionId *GetCurrentVirtualXIDs(TransactionId limitXmin,
					  bool allDbs, int excludeVacuum);
extern int	CountActiveBackends(void);
extern int	CountDBBackends(Oid databaseid);
extern int	CountUserBackends(Oid roleid);
extern bool CheckOtherDBBackends(Oid databaseId);

extern void XidCacheRemoveRunningXids(TransactionId xid,
						  int nxids, const TransactionId *xids,
						  TransactionId latestXid);
						  
extern PGPROC *FindProcByGpSessionId(long gp_session_id);
extern void UpdateSerializableCommandId(void);

extern struct SnapshotData* GetSnapshotData(struct SnapshotData *snapshot, bool serializable);
extern void updateSharedLocalSnapshot(struct DtxContextInfo *dtxContextInfo, struct SnapshotData *snapshot, char* debugCaller);

extern void GetSlotTableDebugInfo(void **snapshotArray, int *maxSlots);

extern bool FindAndSignalProcess(int sessionId, int commandId);

#endif   /* PROCARRAY_H */
