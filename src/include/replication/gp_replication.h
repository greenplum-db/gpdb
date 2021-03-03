/*-------------------------------------------------------------------------
 *
 * gp_replication.h
 *
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/replication/gp_replication.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef GPDB_GP_REPLICATION_H
#define GPDB_GP_REPLICATION_H

#include "fmgr.h"

#include "postmaster/fts.h"
#include "replication/walsender_private.h"
#include "storage/shmem.h"
#include "storage/spin.h"


/*
 * Each GPDB primary-mirror pair has a FTSReplicationStatus in shared memory.
 *
 * Mainly used to track replication process for FTS purpose.
 *
 * This struct is protected by its 'mutex' spinlock field. The walsender
 * and FTS probe process will access this struct.
 */
typedef struct FTSReplicationStatus
{
	NameData    name;			/* The slot's identifier, ie. the replicaton application name */
	slock_t		mutex;			/* lock, on same cacheline as effective_xmin */
	bool		in_use;			/* is this slot defined */

	/*
	 * For GPDB FTS purpose, if the the primary, mirror replication keeps crash
	 * continuously and attempt to create replication connection too many times,
	 * FTS should mark the mirror down.
	 * If the connection established, clear the attempt count to 0.
	 * See more details in FTSGetReplicationDisconnectTime.
	 */
	uint32      con_attempt_count;

	/*
	 * Records time, either during initialization or due to disconnection.
	 * This helps to detect time passed since mirror didn't connect.
	 */
	pg_time_t   replica_disconnected_at;
} FTSReplicationStatus;

typedef struct FTSReplicationStatusCtlData
{
	/*
	 * This array should be declared [FLEXIBLE_ARRAY_MEMBER], but for some
	 * reason you can't do that in an otherwise-empty struct.
	 */
	FTSReplicationStatus   replications[1];
} FTSReplicationStatusCtlData;

extern FTSReplicationStatusCtlData *FTSRepStatusCtl;

extern Size FTSReplicationStatusShmemSize(void);
extern void FTSReplicationStatusShmemInit(void);
extern void FTSReplicationStatusCreateIfNotExist(const char *app_name);
extern void FTSReplicationStatusDrop(const char* app_name);

extern FTSReplicationStatus *RetrieveFTSReplicationStatus(const char *app_name, bool skip_warn);
extern void FTSReplicationStatusUpdateForWalState(const char *app_name, WalSndState state);
extern void FTSReplicationStatusMarkDisconnectForReplication(const char *app_name);
extern pg_time_t FTSGetReplicationDisconnectTime(const char *app_name);

extern void GetMirrorStatus(FtsResponse *response);
extern void SetSyncStandbysDefined(void);
extern void UnsetSyncStandbysDefined(void);

extern Datum gp_replication_error(PG_FUNCTION_ARGS pg_attribute_unused() );

#endif
