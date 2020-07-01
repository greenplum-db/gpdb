/*-------------------------------------------------------------------------
 *
 * gp_replication.c
 *
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/replication/gp_replication.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "pgtime.h"
#include "cdb/cdbvars.h"
#include "replication/gp_replication.h"
#include "replication/walreceiver.h"
#include "replication/walsender.h"
#include "storage/lwlock.h"
#include "utils/builtins.h"

/* Set at database system is ready to accept connections */
extern pg_time_t PMAcceptingConnectionsStartTime;

/*
 * FTSRepStatusCtl is only used for GPDB primary-mirror replication,
 * so set to a small value for now.
 */
uint32 max_replication_status = 1;

/*
 * Control array for replication status management which used in FTS.
 * The reason for not using WalSndCtl to track replication status is
 * the WalSnd is used to track walsender status. And when FTS probe
 * happens, the WalSnd for a replication may already get freed.
 */
FTSReplicationStatusCtlData *FTSRepStatusCtl = NULL;

static void FTSReplicationStatusClearDisconnectTime(FTSReplicationStatus *replication_status);
static void FTSReplicationStatusClearAttempts(FTSReplicationStatus *replication_status);
static uint32 FTSReplicationStatusRetrieveAttempts(FTSReplicationStatus *replication_status);
static pg_time_t FTSReplicationStatusRetrieveDisconnectTime(FTSReplicationStatus *replication_status);
static void FTSReplicationStatusMarkDisconnect(FTSReplicationStatus *replication_status);

/* Report shared-memory space needed by FTSReplicationStatusShmemInit */
Size
FTSReplicationStatusShmemSize(void)
{
	Size		size = 0;

	size = offsetof(FTSReplicationStatusCtlData, replications);
	size = add_size(size, mul_size(max_replication_status, sizeof(FTSReplicationStatus)));

	return size;
}

/* Allocate and initialize FTSReplicationStatus related shared memory */
void
FTSReplicationStatusShmemInit(void)
{
	bool		found;

	Assert(FTSReplicationStatusShmemSize() > 0);

	FTSRepStatusCtl = (FTSReplicationStatusCtlData *)
		ShmemInitStruct("FTSReplicationStatus Ctl", FTSReplicationStatusShmemSize(), &found);

	if (!found)
	{
		/* First time through, so initialize */
		MemSet(FTSRepStatusCtl, 0, FTSReplicationStatusShmemSize());

		for (int i = 0; i < max_replication_status; i++)
		{
			FTSReplicationStatus *slot = &FTSRepStatusCtl->replications[i];

			/* everything else is zeroed by the memset above */
			SpinLockInit(&slot->mutex);
		}
	}
}

/*
 * FTSReplicationStatusCreateIfNotExist - Init a FTSReplicationStatus for current
 * replication application. Use application_name to identify the primary-mirror pair.
 * If FTSReplicationStatus for current application_name already exist, skip
 * create.
 *
 * This function is called under walsender, walsender's application_name is used.
 */
void
FTSReplicationStatusCreateIfNotExist(const char *app_name)
{
	FTSReplicationStatus *replication_status = NULL;

	/* FTSRepStatusCtl should be set already. */
	Assert(FTSRepStatusCtl != NULL);

	/* Use FTSReplicationStatusLock to protect concurrent create/drop */
	LWLockAcquire(FTSReplicationStatusLock, LW_EXCLUSIVE);

	for (int i = 0; i < max_replication_status; i++)
	{
		FTSReplicationStatus *slot = &FTSRepStatusCtl->replications[i];

		if (slot->in_use)
		{
			if (strncmp(app_name, NameStr(slot->name), NAMEDATALEN) == 0)
			{
				/* FTSReplicationStatus for current application already exists */
				LWLockRelease(FTSReplicationStatusLock);
				return;
			}
		}
		else
		{
			/*
			 * Find a free slot, but this does not mean the slot for app_name is not exist.
			 * Cause the slot may get freed and reused.
			 */
			if (replication_status == NULL)
				replication_status = slot;
		}
	}

	/* If find a free slot, create a new FTSReplicationStatus */
	if (replication_status != NULL)
	{
		replication_status->in_use = true;
		StrNCpy(NameStr(replication_status->name), application_name, NAMEDATALEN);
		replication_status->con_attempt_count = 0;
		replication_status->replica_disconnected_at = 0;

		LWLockRelease(FTSReplicationStatusLock);
		return;
	}

	/* No need to release LWLock before fatal since abort will release it */
	ereport(FATAL,
			(errcode(ERRCODE_TOO_MANY_CONNECTIONS),
				errmsg("number of requested standby connections "
					   "exceeds max_replication_status (currently %d)",
					   max_replication_status)));
}

/*
 * FTSReplicationStatusDrop - Drop a FTSReplicationStatus.
 *
 * This function is called in FTS probe process, so an app_name is used
 * to specify which FTSReplicationStatus should be dropped.
 */
void
FTSReplicationStatusDrop(const char* app_name)
{

	/* FTSRepStatusCtl should be set already. */
	Assert(FTSRepStatusCtl != NULL);

	/* Use FTSReplicationStatusLock to protect concurrent create/drop */
	LWLockAcquire(FTSReplicationStatusLock, LW_EXCLUSIVE);
	for (int i = 0; i < max_replication_status; i++)
	{
		FTSReplicationStatus *slot = &FTSRepStatusCtl->replications[i];

		if (slot->in_use &&
			strcmp(app_name, NameStr(slot->name)) == 0)
		{
			slot->in_use = false;
			MemSet(NameStr(slot->name), 0, NAMEDATALEN);
		}
	}
	LWLockRelease(FTSReplicationStatusLock);
}

/*
 * RetrieveFTSReplicationStatus - Get the FTSReplicationStatus from FTSRepStatusCtl.
 *
 * FTSReplicationStatusLock should be held before call this function.
 */
FTSReplicationStatus *
RetrieveFTSReplicationStatus(const char *app_name, bool skip_warn)
{
	/* FTSRepStatusCtl should be set already. */
	Assert(FTSRepStatusCtl != NULL);
	Assert(LWLockHeldByMe(FTSReplicationStatusLock));

	for (int i = 0; i < max_replication_status; i++)
	{
		FTSReplicationStatus *slot = &FTSRepStatusCtl->replications[i];

		if (slot->in_use &&
			strcmp(app_name, NameStr(slot->name)) == 0)
			return slot;
	}

	if (!skip_warn)
		ereport(WARNING,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("FTSReplicationStatus \"%s\" does not exist", app_name)));
	return NULL;
}

/*
 * FTSReplicationStatusMarkDisconnect - Mark current replication's disconnect by
 * increase the con_attempt_count and set current time as disconnect time.
 *
 * This function is called under walsender to mark wal replication disconnected.
 *
 * FTSReplicationStatusLock should be held before call this function.
 */
static void
FTSReplicationStatusMarkDisconnect(FTSReplicationStatus *replication_status)
{
	if (replication_status == NULL)
		return;

	/* FTSRepStatusCtl should be set already. */
	Assert(FTSRepStatusCtl != NULL);

	/* Use FTSReplicationStatusLock to prevent concurrent create/drop */
	Assert(LWLockHeldByMe(FTSReplicationStatusLock));

	/* Since we need to modify the slot's value, lock the mutex. */
	SpinLockAcquire(&replication_status->mutex);
	replication_status->con_attempt_count += 1;
	replication_status->replica_disconnected_at = (pg_time_t) time(NULL);
	SpinLockRelease(&replication_status->mutex);
	elogif(gp_log_fts >= GPVARS_VERBOSITY_VERBOSE, LOG,
		   "FTSReplicationStatus: Mark replication disconnected. "
		   "Current attempt count: %d, disconnect at %ld, for application %s",
		   replication_status->con_attempt_count,
		   replication_status->replica_disconnected_at,
		   NameStr(replication_status->name));
}

/*
 * FTSReplicationStatusUpdateForWalState - Update replication status for an replication
 * application and it's WalSndState.
 *
 * This function is called under walsender to update replication status.
 */
void
FTSReplicationStatusUpdateForWalState(const char *app_name, WalSndState state)
{
	FTSReplicationStatus *replication_status;

	LWLockAcquire(FTSReplicationStatusLock, LW_SHARED);

	replication_status = RetrieveFTSReplicationStatus(app_name, false /*skip_warn*/);
	/* replication_status must exist */
	Assert(replication_status);

	if (state == WALSNDSTATE_CATCHUP || state == WALSNDSTATE_STREAMING)
	{
		/*
		 * We can clear the disconnect time once the connection established.
		 * We only clean the failure count when the wal start streaming, since
		 * although the connection established, and start to send wal, but there
		 * still chance to fail. Since the blocked transaction will get released
		 * only when wal start streaming. More details, see SyncRepReleaseWaiters.
		 */
		FTSReplicationStatusClearDisconnectTime(replication_status);
		/* If current replication start streaming, clear the failure attempt count */
		if (state == WALSNDSTATE_STREAMING)
			FTSReplicationStatusClearAttempts(replication_status);
	}
	else if (FTSReplicationStatusRetrieveDisconnectTime(replication_status) == 0)
	{
		/*
		 * Mark the replication failure if's it the first time set the failure
		 * WalSndState. Since if the disconnect time is not 0, we already mark
		 * the replication failure.
		 */
		FTSReplicationStatusMarkDisconnect(replication_status);
	}

	LWLockRelease(FTSReplicationStatusLock);
}

/*
 * FTSReplicationStatusMarkDisconnectForReplication - Mark a replication disconnected
 * base on replication's application name.
 *
 * This function is called under walsender to mark wal replication disconnected.
 */
void
FTSReplicationStatusMarkDisconnectForReplication(const char *app_name)
{
	FTSReplicationStatus *replication_status;

	LWLockAcquire(FTSReplicationStatusLock, LW_SHARED);

	replication_status = RetrieveFTSReplicationStatus(app_name, false /* skip_warn */);

	/* replication_status must exist  */
	Assert(replication_status);
	FTSReplicationStatusMarkDisconnect(replication_status);

	LWLockRelease(FTSReplicationStatusLock);
}

/*
 * FTSReplicationStatusClearAttempts - Clear current replication's continuously
 * connection attempts since the replication start steaming data.
 *
 * This function is called under walsender to mark the replication start
 * steaming.
 *
 * FTSReplicationStatusLock should be held before call this function.
 */
static void
FTSReplicationStatusClearAttempts(FTSReplicationStatus *replication_status)
{
	if (replication_status == NULL)
		return;
	/* FTSRepStatusCtl should be set already. */
	Assert(FTSRepStatusCtl != NULL);

	/* Use FTSReplicationStatusLock to prevent concurrent create/drop */
	Assert(LWLockHeldByMe(FTSReplicationStatusLock));

	/* Since we need to modify the slot's value, lock the mutex. */
	SpinLockAcquire(&replication_status->mutex);
	replication_status->con_attempt_count = 0;
	SpinLockRelease(&replication_status->mutex);
	elogif(gp_log_fts >= GPVARS_VERBOSITY_VERBOSE, LOG,
		   "FTSReplicationStatus: Clear replication connection attempts, for application %s",
		   NameStr(replication_status->name));
}

/*
 * FTSReplicationStatusRetrieveAttempts - Retrieve the replication connection attempts
 * for a replication application.
 *
 * This function is called under FTS probe process.
 *
 * FTSReplicationStatusLock should be held before call this function.
 */
static uint32
FTSReplicationStatusRetrieveAttempts(FTSReplicationStatus *replication_status)
{
	uint32			 	result;

	if (replication_status == NULL)
		return 0;

	/* FTSRepStatusCtl should be set already. */
	Assert(FTSRepStatusCtl != NULL);

	/* Use FTSReplicationStatusLock to prevent concurrent create/drop */
	Assert(LWLockHeldByMe(FTSReplicationStatusLock));

	/* To prevent partial read, lock the mutex. */
	SpinLockAcquire(&replication_status->mutex);
	result = replication_status->con_attempt_count;
	SpinLockRelease(&replication_status->mutex);

	return result;
}

/*
 * FTSReplicationStatusClearDisconnectTime - Clear replication disconnect time.
 *
 * This function is called under walsender to clear replication disconnect time.
 *
 * FTSReplicationStatusLock should be held before call this function.
 */
static void
FTSReplicationStatusClearDisconnectTime(FTSReplicationStatus *replication_status)
{
	if (replication_status == NULL)
		return;

	/* FTSRepStatusCtl should be set already. */
	Assert(FTSRepStatusCtl != NULL);

	/* Use FTSReplicationStatusLock to prevent concurrent create/drop */
	Assert(LWLockHeldByMe(FTSReplicationStatusLock));

	/* Since we need to modify the slot's value, lock the mutex. */
	SpinLockAcquire(&replication_status->mutex);
	replication_status->replica_disconnected_at = (pg_time_t) 0;
	SpinLockRelease(&replication_status->mutex);
	elogif(gp_log_fts >= GPVARS_VERBOSITY_VERBOSE, LOG,
		   "FTSReplicationStatus: Clear replication disconnect time, for application %s",
		   NameStr(replication_status->name));
}

/*
 * FTSReplicationStatusRetrieveDisconnectTime - Retrieve replication disconnect time.
 *
 * This function is called under FTS probe process to retrieve replication disconnect time.
 *
 * FTSReplicationStatusLock should be held before call this function.
 */
static pg_time_t
FTSReplicationStatusRetrieveDisconnectTime(FTSReplicationStatus *replication_status)
{
	pg_time_t			disconn_time;

	if (replication_status == NULL)
		return 0;

	/* FTSRepStatusCtl should be set already. */
	Assert(FTSRepStatusCtl != NULL);

	/* Use FTSReplicationStatusLock to prevent concurrent create/drop */
	Assert(LWLockHeldByMe(FTSReplicationStatusLock));

	/* Since we need to modify the slot's value, lock the mutex. */
	SpinLockAcquire(&replication_status->mutex);

	disconn_time = replication_status->replica_disconnected_at;

	SpinLockRelease(&replication_status->mutex);

	return disconn_time;
}

/* FTSGetReplicationDisconnectTime - used in FTS probe process.
 *
 * Detect the primary-mirror replication attempt count.
 * If the replication keeps crash, we should consider mark
 * mirror down directly. Since the walsender keeps resarting,
 * walsender->replica_disconnected_at keeps updated.
 * So ignore it.
 *
 * The reason why we want mark mirror down for this case is because,
 * when current situation happens, and a transaction try to sync wal
 * to mirror at the same time, the transaction will block. If walsender
 * keeps fail, the transaction will block forever.
 * Please see more details in SyncRepWaitForLSN and SyncRepReleaseWaiters.
 *
 * If the FTSReplicationStatus for GP_WALRECEIVER_APPNAME is not exist,
 * it means the replication has already been stopped.
 */
pg_time_t
FTSGetReplicationDisconnectTime(const char *app_name)
{
	pg_time_t			walsender_replica_disconnected_at = 0;
	uint32				attempt_replication_times = 0;
	FTSReplicationStatus	   *replication_status = NULL;

	LWLockAcquire(FTSReplicationStatusLock, LW_SHARED);
	replication_status = RetrieveFTSReplicationStatus(app_name, true /* skip_warn */);
	if (replication_status)
	{
		attempt_replication_times = FTSReplicationStatusRetrieveAttempts(replication_status);
		if (attempt_replication_times <= gp_fts_replication_attempt_count)
			walsender_replica_disconnected_at = FTSReplicationStatusRetrieveDisconnectTime(replication_status);
		else
		{
			ereport(LOG,
					(errmsg("Primary-mirror replication streaming already attempted %d times exceed"
					" limit gp_fts_replication_attempt_count %d",
					attempt_replication_times, gp_fts_replication_attempt_count)));
		}
	}
	LWLockRelease(FTSReplicationStatusLock);

	return walsender_replica_disconnected_at;
}

static bool
is_mirror_up(WalSnd *walsender)
{
	Assert(walsender->is_for_gp_walreceiver);
	bool walsender_has_pid = walsender->pid != 0;

	/*
	 * WalSndSetState() resets replica_disconnected_at for
	 * below states. If modifying below states then be sure
	 * to update corresponding logic in WalSndSetState() as
	 * well.
	 */
	bool is_communicating_with_mirror = walsender->state == WALSNDSTATE_CATCHUP ||
		walsender->state == WALSNDSTATE_STREAMING;

	return walsender_has_pid && is_communicating_with_mirror;
}

static bool
is_probe_retry_needed()
{
	pg_time_t			walsender_replica_disconnected_at = 0;

	/*
	 * Get the walsender disconnect time, if current replication failed too
	 * many times continuously, the walsender_replica_disconnected_at should
	 * not take into consider. See more details in FTSGetReplicationDisconnectTime.
	 */
	walsender_replica_disconnected_at = FTSGetReplicationDisconnectTime(GP_WALRECEIVER_APPNAME);

	/*
	 * PMAcceptingConnectionStartTime is process-local variable, set in
	 * postmaster process and inherited by the FTS handler child
	 * process. This works because the timestamp is set only once by
	 * postmaster, and is guaranteed to be set before FTS handler child
	 * processes can be spawned.
	 */
	Assert(PMAcceptingConnectionsStartTime);
	pg_time_t delta = ((pg_time_t) time(NULL)) -
		Max(walsender_replica_disconnected_at, PMAcceptingConnectionsStartTime);

	/*
	 * Report mirror as down, only if it didn't connect for below
	 * grace period to primary. This helps to avoid marking mirror
	 * down unnecessarily when restarting primary or due to small n/w
	 * glitch. During this period, request FTS to probe again.
	 *
	 * If the delta is negative, then it's overflowed, meaning it's
	 * over gp_fts_mark_mirror_down_grace_period since either last
	 * database accepting connections or last time wal sender
	 * died. Then, we can safely mark the mirror is down.
	 */
	if (delta < gp_fts_mark_mirror_down_grace_period && delta >= 0)
	{
		ereport(LOG,
				(errmsg("requesting fts retry as mirror didn't connect yet but in grace period: " INT64_FORMAT, delta),
					errdetail("pid zero at time: " INT64_FORMAT " accept connections start time: " INT64_FORMAT,
							walsender_replica_disconnected_at, PMAcceptingConnectionsStartTime)));
		return true;
	}
	return false;
}

/*
 * Check the WalSndCtl to obtain if mirror is up or down, if the wal sender is
 * in streaming, and if synchronous replication is enabled or not.
 */
void
GetMirrorStatus(FtsResponse *response)
{
	response->IsMirrorUp = false;
	response->IsInSync = false;
	response->RequestRetry = false;

	LWLockAcquire(SyncRepLock, LW_SHARED);

	for (int i = 0; i < max_wal_senders; i++)
	{
		bool is_up;
		bool is_streaming;
		WalSnd *walsender = &WalSndCtl->walsnds[i];

		SpinLockAcquire(&walsender->mutex);
		if (!walsender->is_for_gp_walreceiver)
		{
			SpinLockRelease(&walsender->mutex);
			continue;
		}

		is_up = is_mirror_up(walsender);
		is_streaming = (walsender->state == WALSNDSTATE_STREAMING);

		response->IsMirrorUp = is_up;
		response->IsInSync = (is_up && is_streaming);
		SpinLockRelease(&walsender->mutex);
		break;
	}

	response->IsSyncRepEnabled = WalSndCtl->sync_standbys_defined;

	LWLockRelease(SyncRepLock);

	if (!response->IsMirrorUp)
		response->RequestRetry = is_probe_retry_needed();
}

/*
 * Set WalSndCtl->sync_standbys_defined to true to enable synchronous segment
 * WAL replication and insert synchronous_standby_names="*" into the
 * gp_replication.conf to persist this state in case of segment crash.
 */
void
SetSyncStandbysDefined(void)
{
	if (!WalSndCtl->sync_standbys_defined)
	{
		set_gp_replication_config("synchronous_standby_names", "*");

		/* Signal a reload to the postmaster. */
		elog(LOG, "signaling configuration reload: setting synchronous_standby_names to '*'");
		DirectFunctionCall1(pg_reload_conf, PointerGetDatum(NULL) /* unused */);
	}
}

void
UnsetSyncStandbysDefined(void)
{
	if (WalSndCtl->sync_standbys_defined)
	{
		set_gp_replication_config("synchronous_standby_names", "");

		/* Signal a reload to the postmaster. */
		elog(LOG, "signaling configuration reload: setting synchronous_standby_names to ''");
		DirectFunctionCall1(pg_reload_conf, PointerGetDatum(NULL) /* unused */);
	}

	FTSReplicationStatusDrop(GP_WALRECEIVER_APPNAME);
}

Datum
gp_replication_error(PG_FUNCTION_ARGS)
{
	PG_RETURN_TEXT_P(cstring_to_text(WalSndCtl->error == WALSNDERROR_WALREAD ? "walread" : "none"));
}
