/*-------------------------------------------------------------------------
 *
 * gp_replication.h
 *
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
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
#include "storage/shmem.h"
#include "storage/spin.h"

/*
 * Each GPDB primary-mirror pair has a GPReplication in shared memory.
 *
 * Mainly used to track replication process for FTS purpose.
 *
 * This struct is protected by its 'mutex' spinlock field. The walsender
 * and FTS probe process will access this struct.
 */
typedef struct GPReplication
{
	/* The slot's identifier, ie. the replicaton application name */
	NameData    name;

	/* lock, on same cacheline as effective_xmin */
	slock_t		mutex;

	/* is this slot defined */
	bool		in_use;

	/*
	 * For GPDB FTS purpose, if the the primary, mirror replication keeps crash
	 * continuously and attempt to create replication connection too many times,
	 * FTS should mark the mirror down.
	 * If the connection established, clear the attempt count to 0.
	 */
	uint32      con_attempt_count;

	/*
	 * Records time, either during initialization or due to disconnection.
	 * This helps to detect time passed since mirror didn't connect.
	 */
	pg_time_t   replica_disconnected_at;
} GPReplication;

typedef struct GPReplicationCtlData
{
	/*
	 * This array should be declared [FLEXIBLE_ARRAY_MEMBER], but for some
	 * reason you can't do that in an otherwise-empty struct.
	 */
	GPReplication   gp_replications[1];
} GPReplicationCtlData;

extern GPReplicationCtlData *GPRepCtl;

extern Size GPReplicationShmemSize(void);
extern void GPReplicationShmemInit(void);
extern void GPReplicationCreateIfNotExist(const char *app_name);
extern void GPReplicationDrop(const char* app_name);

extern GPReplication *RetrieveGPReplication(const char *app_name, bool skip_error);
extern void GPReplicationMarkDisconnect(GPReplication *gp_replication);
extern void GPReplicationClearAttempts(GPReplication *gp_replication);
extern void GPReplicationClearDisconnectTime(GPReplication *gp_replication);
extern pg_time_t GPReplicationRetrieveDisconnectTime(GPReplication *gp_replication);

extern void GetMirrorStatus(FtsResponse *response);
extern void SetSyncStandbysDefined(void);
extern void UnsetSyncStandbysDefined(void);

extern Datum gp_replication_error(PG_FUNCTION_ARGS pg_attribute_unused() );

#endif
