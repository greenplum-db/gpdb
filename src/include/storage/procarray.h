/*-------------------------------------------------------------------------
 *
 * procarray.h
 *	  POSTGRES process array definitions.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/storage/procarray.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef PROCARRAY_H
#define PROCARRAY_H

#include "storage/lock.h"
#include "storage/standby.h"
#include "utils/relcache.h"
#include "utils/snapshot.h"

#include "cdb/cdbpublic.h"
#include "cdb/cdbtm.h"

struct DtxContextInfo;         /* cdb/cdbdtxcontextinfo.h */

/*
 * These are to implement PROCARRAY_FLAGS_XXX
 *
 * Note: These flags are cloned from PROC_XXX flags in src/include/storage/proc.h
 * to avoid forcing to include proc.h when including procarray.h. So if you modify
 * PROC_XXX flags, you need to modify these flags.
 */
#define		PROCARRAY_VACUUM_FLAG			0x02	/* currently running lazy
													 * vacuum */
#define		PROCARRAY_ANALYZE_FLAG			0x04	/* currently running
													 * analyze */
#define		PROCARRAY_LOGICAL_DECODING_FLAG 0x10	/* currently doing logical
													 * decoding outside xact */

#define		PROCARRAY_SLOTS_XMIN			0x20	/* replication slot xmin,
													 * catalog_xmin */
/*
 * Only flags in PROCARRAY_PROC_FLAGS_MASK are considered when matching
 * PGXACT->vacuumFlags. Other flags are used for different purposes and
 * have no corresponding PROC flag equivalent.
 * GPDB doesn't use PROC_IN_VACUUM for now, see comment in vacuum_rel().
 */
#define		PROCARRAY_PROC_FLAGS_MASK	(PROCARRAY_ANALYZE_FLAG | PROCARRAY_LOGICAL_DECODING_FLAG)

/* Use the following flags as an input "flags" to GetOldestXmin function */
/* Consider all backends except for logical decoding ones which manage xmin separately */
#define		PROCARRAY_FLAGS_DEFAULT			PROCARRAY_LOGICAL_DECODING_FLAG
/* Ignore vacuum backends */
#define		PROCARRAY_FLAGS_VACUUM			PROCARRAY_FLAGS_DEFAULT | PROCARRAY_VACUUM_FLAG
/* Ignore analyze backends */
#define		PROCARRAY_FLAGS_ANALYZE			PROCARRAY_FLAGS_DEFAULT | PROCARRAY_ANALYZE_FLAG
/* Ignore both vacuum and analyze backends */
#define		PROCARRAY_FLAGS_VACUUM_ANALYZE	PROCARRAY_FLAGS_DEFAULT | PROCARRAY_VACUUM_FLAG | PROCARRAY_ANALYZE_FLAG

extern Size ProcArrayShmemSize(void);
extern void CreateSharedProcArray(void);
extern void ProcArrayAdd(PGPROC *proc);
extern void ProcArrayRemove(PGPROC *proc, TransactionId latestXid);
extern void ProcArrayEndTransaction(PGPROC *proc, TransactionId latestXid);
extern void ProcArrayEndGxact(TMGXACT *gxact);
extern void ProcArrayClearTransaction(PGPROC *proc);

extern void ProcArrayInitRecovery(TransactionId initializedUptoXID);
extern void ProcArrayApplyRecoveryInfo(RunningTransactions running);
extern void ProcArrayApplyXidAssignment(TransactionId topxid,
										int nsubxids, TransactionId *subxids);

extern void RecordKnownAssignedTransactionIds(TransactionId xid);
extern void ExpireTreeKnownAssignedTransactionIds(TransactionId xid,
												  int nsubxids, TransactionId *subxids,
												  TransactionId max_xid);
extern void ExpireAllKnownAssignedTransactionIds(void);
extern void ExpireOldKnownAssignedTransactionIds(TransactionId xid);

extern int	GetMaxSnapshotXidCount(void);
extern int	GetMaxSnapshotSubxidCount(void);

extern Snapshot GetSnapshotData(Snapshot snapshot, DtxContext distributedTransactionContext);

extern bool ProcArrayInstallImportedXmin(TransactionId xmin,
										 VirtualTransactionId *sourcevxid);
extern bool ProcArrayInstallRestoredXmin(TransactionId xmin, PGPROC *proc);

extern RunningTransactions GetRunningTransactionData(void);

extern bool TransactionIdIsInProgress(TransactionId xid);
extern bool TransactionIdIsActive(TransactionId xid);
extern TransactionId GetOldestXmin(Relation rel, int flags);
extern TransactionId GetLocalOldestXmin(Relation rel, int flags);
extern TransactionId GetOldestActiveTransactionId(void);
extern TransactionId GetOldestSafeDecodingTransactionId(bool catalogOnly);

extern VirtualTransactionId *GetVirtualXIDsDelayingChkpt(int *nvxids);
extern bool HaveVirtualXIDsDelayingChkpt(VirtualTransactionId *vxids, int nvxids);

extern PGPROC *BackendPidGetProc(int pid);
extern PGPROC *BackendPidGetProcWithLock(int pid);
extern int	BackendXidGetPid(TransactionId xid);
extern bool IsBackendPid(int pid);

extern VirtualTransactionId *GetCurrentVirtualXIDs(TransactionId limitXmin,
												   bool excludeXmin0, bool allDbs, int excludeVacuum,
												   int *nvxids);
extern VirtualTransactionId *GetConflictingVirtualXIDs(TransactionId limitXmin, Oid dbOid);
extern pid_t CancelVirtualTransaction(VirtualTransactionId vxid, ProcSignalReason sigmode);

extern bool MinimumActiveBackends(int min);
extern int	CountDBBackends(Oid databaseid);
extern int	CountDBConnections(Oid databaseid);
extern void CancelDBBackends(Oid databaseid, ProcSignalReason sigmode, bool conflictPending);
extern int	SignalMppBackends(int sig);
extern int	CountUserBackends(Oid roleid);
extern bool CountOtherDBBackends(Oid databaseId,
								 int *nbackends, int *nprepared);

extern void XidCacheRemoveRunningXids(TransactionId xid,
									  int nxids, const TransactionId *xids,
									  TransactionId latestXid);
						  
extern PGPROC *FindProcByGpSessionId(long gp_session_id);
extern void UpdateSerializableCommandId(CommandId curcid);

extern void updateSharedLocalSnapshot(struct DtxContextInfo *dtxContextInfo,
									  DtxContext distributedTransactionContext,
									  Snapshot snapshot,
									  char *debugCaller);

extern void GetSlotTableDebugInfo(void **snapshotArray, int *maxSlots);

extern void getDtxCheckPointInfo(char **result, int *result_size);

extern List *ListAllGxid(void);
extern int GetPidByGxid(DistributedTransactionId gxid);
extern bool IsDtxInProgress(DistributedTransactionId gxid);

extern void ProcArraySetReplicationSlotXmin(TransactionId xmin,
											TransactionId catalog_xmin, bool already_locked);

extern void ProcArrayGetReplicationSlotXmin(TransactionId *xmin,
											TransactionId *catalog_xmin);
extern DistributedTransactionId LocalXidGetDistributedXid(TransactionId xid);
extern int GetSessionIdByPid(int pid);
extern void ResGroupSignalMoveQuery(int sessionId, void *slot, Oid groupId);

#endif							/* PROCARRAY_H */
