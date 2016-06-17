/*-------------------------------------------------------------------------
 *
 * cdblocaldistribxact.h
 *
 * Copyright (c) 2007-2008, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBLOCALDISTRIBXACT_H
#define CDBLOCALDISTRIBXACT_H

#include "storage/lock.h"

#include "cdb/cdbpublic.h"   /* LocalDistribXactRef */

typedef enum
{
	LOCALDISTRIBXACT_STATE_NONE = 0,		
	LOCALDISTRIBXACT_STATE_ACTIVE,
	LOCALDISTRIBXACT_STATE_COMMITDELIVERY,
	LOCALDISTRIBXACT_STATE_COMMITTED,
	LOCALDISTRIBXACT_STATE_ABORTDELIVERY,
	LOCALDISTRIBXACT_STATE_ABORTED,
	LOCALDISTRIBXACT_STATE_PREPARED,
	LOCALDISTRIBXACT_STATE_COMMITPREPARED,
	LOCALDISTRIBXACT_STATE_ABORTPREPARED,
	
} LocalDistribXactState;

extern Size LocalDistribXact_ShmemSize(void);
extern void LocalDistribXact_ShmemCreate(void);

extern void LocalDistribXactRef_Init(
	LocalDistribXactRef	*ref);
extern bool LocalDistribXactRef_IsNil(
	LocalDistribXactRef	*ref);
extern void LocalDistribXactRef_ReleaseUnderLock(
	LocalDistribXactRef	*ref);
extern void LocalDistribXactRef_Release(
	LocalDistribXactRef	*ref);
extern void LocalDistribXactRef_Transfer(
	LocalDistribXactRef	*target,
	LocalDistribXactRef	*source);
extern void  LocalDistribXactRef_Clone(
	LocalDistribXactRef	*clone,
	LocalDistribXactRef	*source);

extern void LocalDistribXact_StartOnMaster(
	DistributedTransactionTimeStamp	newDistribTimeStamp,
	DistributedTransactionId 		newDistribXid,
	TransactionId					*newLocalXid,
	LocalDistribXactRef			*masterLocalDistribXactRef);

extern void LocalDistribXact_StartOnSegment(
	DistributedTransactionTimeStamp	newDistribTimeStamp,
	DistributedTransactionId 		newDistribXid,
	TransactionId					*newLocalXid);

extern void LocalDistribXact_CreateRedoPrepared(
	DistributedTransactionTimeStamp	redoDistribTimeStamp,
	DistributedTransactionId 		redoDistribXid,
	TransactionId					redoLocalXid,
	LocalDistribXactRef				*localDistribXactRef);

extern void LocalDistribXact_ChangeStateUnderLock(
	TransactionId				localXid,
	LocalDistribXactRef			*localDistribXactRef,
	LocalDistribXactState		newState);

extern void LocalDistribXact_ChangeState(
	TransactionId				localXid,
	LocalDistribXactRef			*localDistribXactRef,
	LocalDistribXactState		newState);

extern DistributedTransactionId LocalDistribXact_GetMaxDistributedXid(void);

extern void LocalDistribXact_GetDistributedXid(
	TransactionId					localXid,
	LocalDistribXactRef				*localDistribXactRef,
	DistributedTransactionTimeStamp *distribTimeStamp,
	DistributedTransactionId 		*distribXid);

extern char* LocalDistribXact_DisplayString(
	LocalDistribXactRef		*localDistribXactRef);

extern bool LocalDistribXactCache_CommittedFind(
	TransactionId						localXid,
	DistributedTransactionTimeStamp		distribTransactionTimeStamp,
	DistributedTransactionId			*distribXid);

extern void LocalDistribXactCache_AddCommitted(
	TransactionId						localXid,
	DistributedTransactionTimeStamp		distribTransactionTimeStamp,
	DistributedTransactionId			distribXid);

extern void LocalDistribXactCache_ShowStats(char *nameStr);

#endif   /* CDBLOCALDISTRIBXACT_H */
