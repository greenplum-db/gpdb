/*-------------------------------------------------------------------------
 *
 * cdblocaldistribxact.h
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdblocaldistribxact.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBLOCALDISTRIBXACT_H
#define CDBLOCALDISTRIBXACT_H

typedef enum
{
	LOCALDISTRIBXACT_STATE_NONE = 0,
	LOCALDISTRIBXACT_STATE_ACTIVE,
	LOCALDISTRIBXACT_STATE_COMMITTED,
	LOCALDISTRIBXACT_STATE_ABORTED,
	LOCALDISTRIBXACT_STATE_PREPARED
} LocalDistribXactState;

/*
 * Local information in the database instance (master or segment) on
 * a distributed transaction.
 */
typedef struct LocalDistribXactData
{
	/*
	 * Current distributed transaction state.
	 */
	LocalDistribXactState		state;

	/*
	 * Distributed xid and the master's restart timestamp.
	 */
	DistributedTransactionTimeStamp	distribTimeStamp;
	DistributedTransactionId 		distribXid;

} LocalDistribXactData;

extern void LocalDistribXact_ChangeState(int pgprocno,
	LocalDistribXactState		newState);

extern char* LocalDistribXact_DisplayString(int pgprocno);

extern bool LocalDistribXactCache_CommittedFind(
	TransactionId						localXid,
	DistributedTransactionId			*distribXid);

extern void LocalDistribXactCache_AddCommitted(
	TransactionId						localXid,
	DistributedTransactionId			distribXid);

extern void LocalDistribXactCache_ShowStats(char *nameStr);

#endif   /* CDBLOCALDISTRIBXACT_H */
