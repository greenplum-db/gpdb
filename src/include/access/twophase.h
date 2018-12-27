/*-------------------------------------------------------------------------
 *
 * twophase.h
 *	  Two-phase-commit related declarations.
 *
 *
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/access/twophase.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef TWOPHASE_H
#define TWOPHASE_H

#include "access/xlogdefs.h"
#include "datatype/timestamp.h"
#include "storage/lock.h"

#include "cdb/cdblocaldistribxact.h"

/*
 * Directory where two phase commit files reside within PGDATA
 */
#define TWOPHASE_DIR "pg_twophase"

/*
 * <KAS fill in comment here>
 */

typedef struct prpt_map
{
  TransactionId xid;
  XLogRecPtr    xlogrecptr;
} prpt_map;

typedef struct prepared_transaction_agg_state
{
    union
    {
      int count;
      int64 dummy;
    };
  prpt_map maps[0]; /* variable length */
} prepared_transaction_agg_state;

#define PREPARED_TRANSACTION_CHECKPOINT_BYTES(count) \
	(offsetof(prepared_transaction_agg_state, maps) + sizeof(prpt_map) * (count))

/*
 * GlobalTransactionData is defined in twophase.c; other places have no
 * business knowing the internal definition.
 */
typedef struct GlobalTransactionData *GlobalTransaction;

/* GUC variable */
extern PGDLLIMPORT int max_prepared_xacts;

extern Size TwoPhaseShmemSize(void);
extern void TwoPhaseShmemInit(void);

extern void AtAbort_Twophase(void);
extern void PostPrepare_Twophase(void);

extern PGPROC *TwoPhaseGetDummyProc(TransactionId xid);
extern BackendId TwoPhaseGetDummyBackendId(TransactionId xid);

extern GlobalTransaction MarkAsPreparing(TransactionId xid, 
				LocalDistribXactData *localDistribXactRef,
				const char *gid,
				TimestampTz prepared_at,
				Oid owner, Oid databaseid
			      , XLogRecPtr xlogrecptr);

extern void StartPrepare(GlobalTransaction gxact);
extern void EndPrepare(GlobalTransaction gxact);
extern bool StandbyTransactionIdIsPrepared(TransactionId xid);

extern TransactionId PrescanPreparedTransactions(TransactionId **xids_p,
							int *nxids_p);
extern void StandbyRecoverPreparedTransactions(bool overwriteOK);
extern void RecoverPreparedTransactions(void);

extern void RecreateTwoPhaseFile(TransactionId xid, void *content, int len, XLogRecPtr *xlogrecptr);
extern void RemoveTwoPhaseFile(TransactionId xid, bool giveWarning);

extern void CheckPointTwoPhase(XLogRecPtr redo_horizon);

extern bool FinishPreparedTransaction(const char *gid, bool isCommit, bool raiseErrorIfNotFound);

extern void TwoPhaseAddPreparedTransactionInit(
					        prepared_transaction_agg_state **ptas
					      , int                             *maxCount);

struct XLogRecData;
extern XLogRecPtr *getTwoPhaseOldestPreparedTransactionXLogRecPtr(struct XLogRecData *rdata);

extern void TwoPhaseAddPreparedTransaction(
                 prepared_transaction_agg_state **ptas
		 , int                           *maxCount
                 , TransactionId                  xid
		 , XLogRecPtr                    *xlogPtr);

extern void getTwoPhasePreparedTransactionData(prepared_transaction_agg_state **ptas);

extern void SetupCheckpointPreparedTransactionList(prepared_transaction_agg_state *ptas);

#endif   /* TWOPHASE_H */
