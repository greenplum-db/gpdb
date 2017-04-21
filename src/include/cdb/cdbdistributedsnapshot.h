/*-------------------------------------------------------------------------
 *
 * cdbdistributedsnapshot.h
 *
 * Copyright (c) 2007-2008, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBDISTRIBUTEDSNAPSHOT_H
#define CDBDISTRIBUTEDSNAPSHOT_H

/* This is a shipped header, do not include other files under "cdb" */
#include "c.h"     /* DistributedTransactionId */


/*
 * Combine global and local information captured while scanning the
 * distributed transactions so they can be sorted as a unit.
 *
 * For sub-transaction same distribXid can be associated with multiple
 * localXid, but currently we cache only one localXid for distribXid, for
 * others we will consult the distributed_log. Opportunity exist though to
 * improve performance by caching every localXid corresponding to distribXid,
 * in inProgressEntryArray.
 */
typedef struct DistributedSnapshotMapEntry
{
	DistributedTransactionId 	distribXid;

	TransactionId 				localXid;

} DistributedSnapshotMapEntry;

#define DistributedSnapshotHeader_StaticInit {0,0,0,0,0,0}

typedef struct DistributedSnapshotHeader
{
	DistributedTransactionTimeStamp		distribTransactionTimeStamp;
										/*
										 * The unique timestamp for this
										 * start of the DTM.  It applies to
										 * all of the distributed transactions
										 * in this snapshot.
										 */

	DistributedTransactionId		xminAllDistributedSnapshots;
										/*
										 * The lowest distributed transaction
										 * being used for distributed snapshots.
										 */
	
	DistributedSnapshotId			distribSnapshotId;
										/* 
										 * Unique number identifying this
										 * particular distributed snapshot.
										 */
										   
	DistributedTransactionId 	xmin;	/* XID < xmin are visible to me */
	DistributedTransactionId 	xmax;	/* XID >= xmax are invisible to me */
	int32						count;	/* 
										 * Count of distributed transactions
										 * in inProgress*Array. 
										 */
	int32						maxCount;
										/*
										 * Max entry count for 
										 * inProgress*Array.
										 */
} DistributedSnapshotHeader;

#define DistributedSnapshotWithLocalMapping_StaticInit {DistributedSnapshotHeader_StaticInit,NULL}

/*
 * GP: Global information about which transactions are visible for a distributed
 * transaction.
 *
 * The changable aspects related to a transaction like sub-transactions and
 * command number are stored separately.
 */
typedef struct DistributedSnapshotWithLocalMapping
{
	DistributedSnapshotHeader		header;
	
	DistributedSnapshotMapEntry 	*inProgressEntryArray;
										/* 
										 * Array of distributed transactions
										 * in progress, optionally with the
										 * associated local xid.
										 */

} DistributedSnapshotWithLocalMapping;

#define DistributedSnapshot_StaticInit {DistributedSnapshotHeader_StaticInit,NULL}

typedef struct DistributedSnapshot
{
	DistributedSnapshotHeader		header;
		
	DistributedTransactionId 		*inProgressXidArray;
										/* 
										 * Array of distributed transactions
										 * in progress.
										 */

} DistributedSnapshot;

typedef enum
{
	DISTRIBUTEDSNAPSHOT_COMMITTED_NONE = 0,		
	DISTRIBUTEDSNAPSHOT_COMMITTED_INPROGRESS,
	DISTRIBUTEDSNAPSHOT_COMMITTED_VISIBLE,
	DISTRIBUTEDSNAPSHOT_COMMITTED_IGNORE
	
} DistributedSnapshotCommitted;

extern DistributedSnapshotCommitted DistributedSnapshotWithLocalMapping_CommittedTest(
	DistributedSnapshotWithLocalMapping		*dslm,
	TransactionId 							localXid);

extern void DistributedSnapshot_Reset(
	DistributedSnapshot *distributedSnapshot);

extern void DistributedSnapshot_Copy(
	DistributedSnapshot *target,
	DistributedSnapshot *source);

#endif   /* CDBDISTRIBUTEDSNAPSHOT_H */

