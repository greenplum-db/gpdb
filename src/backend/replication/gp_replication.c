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
#include "replication/walsender_private.h"
#include "storage/lwlock.h"
#include "utils/builtins.h"

/* Set at database system is ready to accept connections */
extern pg_time_t PMAcceptingConnectionsStartTime;

/* Control array for gp_replication management */
GPReplicationCtlData *GPRepCtl = NULL;

/* Report shared-memory space needed by GPReplicationShmemInit */
Size
GPReplicationShmemSize(void)
{
	Size		size = 0;

	size = offsetof(GPReplicationCtlData, gp_replications);

	/*
	 * Since we only allow max_wal_senders walsender, a primiray's
	 * replication mirrors can not be more than this value.
	 */
	size = add_size(size, mul_size(max_wal_senders, sizeof(GPReplication)));

	return size;
}

/* Allocate and initialize GPReplication related shared memory */
void
GPReplicationShmemInit(void)
{
	bool		found;

	GPRepCtl = (GPReplicationCtlData *)
		ShmemInitStruct("GPReplication Ctl", GPReplicationShmemSize(), &found);

	if (!found)
	{
		int			i;

		/* First time through, so initialize */
		MemSet(GPRepCtl, 0, GPReplicationShmemSize());

		for (i = 0; i < max_wal_senders; i++)
		{
			GPReplication *slot = &GPRepCtl->gp_replications[i];

			/* everything else is zeroed by the memset above */
			SpinLockInit(&slot->mutex);
		}
	}
}

/*
 * GPReplicationCreateIfNotExist - Init a GPReplication for current replication
 * application. Use application_name to identify the primary-mirror pair.
 * If GPReplication for current application_name is already exist, skip
 * create.
 *
 * This function is called under walsender, walsender's application is used.
 */
void
GPReplicationCreateIfNotExist(void)
{
	int		i;
	GPReplication *gp_replication = NULL;

	/* GPRepCtl should be set already. */
	Assert(GPRepCtl != NULL);

	/* Use GPReplicationControlLock to protect concurrent create/drop */
	LWLockAcquire(GPReplicationControlLock, LW_EXCLUSIVE);

	for (i =0; i < max_wal_senders; i++)
	{
		GPReplication *slot = &GPRepCtl->gp_replications[i];

		if (slot->in_use)
		{
			if (strcmp(application_name, NameStr(slot->name)) == 0)
			{
				/* GPReplication for current application already exists */
				LWLockRelease(GPReplicationControlLock);
				return;
			}
		}
		else
		{
			/* Find a free slot */
			if (gp_replication == NULL)
				gp_replication = slot;
		}
	}

	/* If find a free slot, create a new GPReplication */
	if (gp_replication != NULL)
	{
		gp_replication->in_use = true;
		StrNCpy(NameStr(gp_replication->name), application_name, NAMEDATALEN);
		gp_replication->con_attempt_count = 0;
		gp_replication->replica_disconnected_at = 0;

		LWLockRelease(GPReplicationControlLock);
		return;
	}

	/* No need to release LWLock before fatal since abort will release it */
	ereport(FATAL,
			(errcode(ERRCODE_TOO_MANY_CONNECTIONS),
				errmsg("number of requested standby connections "
					   "exceeds max_wal_senders (currently %d)",
					   max_wal_senders)));
}

/*
 * GPReplicationDrop - Drop a GPReplication.
 *
 * This function is called in FTS probe process, so an app_name is used
 * to specify which GPReplication should be dropped.
 */
void
GPReplicationDrop(const char* app_name)
{
	int		i;

	/* GPRepCtl should be set already. */
	Assert(GPRepCtl != NULL);

	/* Use GPReplicationControlLock to protect concurrent create/drop */
	LWLockAcquire(GPReplicationControlLock, LW_EXCLUSIVE);
	for (i =0; i < max_wal_senders; i++)
	{
		GPReplication *slot = &GPRepCtl->gp_replications[i];

		if (slot->in_use &&
			strcmp(app_name, NameStr(slot->name)) == 0)
		{
			slot->in_use = false;
		}
	}
	LWLockRelease(GPReplicationControlLock);
}

/*
 * RetrieveGPReplication - Get the GPReplication from GPRepCtl.
 *
 * GPReplicationControlLock should be take before call this function.
 */
static GPReplication *
RetrieveGPReplication(const char *app_name, bool skip_error)
{
	int		i;
	GPReplication *gp_replication = NULL;

	/* GPRepCtl should be set already. */
	Assert(GPRepCtl != NULL);

	for (i =0; i < max_wal_senders; i++)
	{
		GPReplication *slot = &GPRepCtl->gp_replications[i];

		if (slot->in_use &&
			strcmp(app_name, NameStr(slot->name)) == 0)
		{
			gp_replication = slot;
			break;
		}
	}

	if (!gp_replication && !skip_error)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("GPReplication \"%s\" does not exist", app_name)));
	return gp_replication;
}

/*
 * GPReplicationIncreaseAttempts - Increase current replication's continuously
 * connection attempts.
 *
 * This function is called under walsender to mark a start of replicaton attempt.
 * So the walsender's application_name is used to identify the GPReplication.
 */
void
GPReplicationIncreaseAttempts(void)
{
	GPReplication *slot;

	/* GPRepCtl should be set already. */
	Assert(GPRepCtl != NULL);

	/* Use GPReplicationControlLock to prevent concurrent create/drop */
	LWLockAcquire(GPReplicationControlLock, LW_SHARED);

	slot = RetrieveGPReplication(application_name, false);

	/* Since we need to modify the slot's value, lock the mutex. */
	SpinLockAcquire(&slot->mutex);

	slot->con_attempt_count += 1;

	elogif(gp_log_fts >= GPVARS_VERBOSITY_VERBOSE, LOG,
		   "GPReplication: Continuously start replication %d times, for application %s",
		   slot->con_attempt_count, application_name);

	SpinLockRelease(&slot->mutex);

	LWLockRelease(GPReplicationControlLock);
}


/*
 * GPReplicationClearAttempts - Clear current replication's continuously
 * connection attempts since the replication start steaming data.
 *
 * This function is called under walsender to mark the replication start
 * steaming. So the walsender's application_name is used.
 */
void
GPReplicationClearAttempts(void)
{
	GPReplication *slot;

	/* GPRepCtl should be set already. */
	Assert(GPRepCtl != NULL);

	/* Use GPReplicationControlLock to prevent concurrent create/drop */
	LWLockAcquire(GPReplicationControlLock, LW_SHARED);

	slot = RetrieveGPReplication(application_name, false);

	/* Since we need to modify the slot's value, lock the mutex. */
	SpinLockAcquire(&slot->mutex);

	slot->con_attempt_count = 0;
	elogif(gp_log_fts >= GPVARS_VERBOSITY_VERBOSE, LOG,
		   "GPReplication: Clear replication connection attempts, for application %s",
		   application_name);

	SpinLockRelease(&slot->mutex);

	LWLockRelease(GPReplicationControlLock);
}

/*
 * GPReplicationRetrieveAttempts - Retrieve the replication connection attempts
 * for a replication application.
 *
 * This function is called under FTS probe process.
 * So app_name is used to specify which GPReplication to retrieve.
 */
uint32
GPReplicationRetrieveAttempts(const char* app_name)
{
	GPReplication	   *slot;
	uint32			 	result;

	/* GPRepCtl should be set already. */
	Assert(GPRepCtl != NULL);

	/* Use GPReplicationControlLock to prevent concurrent create/drop */
	LWLockAcquire(GPReplicationControlLock, LW_SHARED);

	slot = RetrieveGPReplication(app_name, false);

	/* To prevent partial read, lock the mutex. */
	SpinLockAcquire(&slot->mutex);

	result = slot->con_attempt_count;

	SpinLockRelease(&slot->mutex);

	LWLockRelease(GPReplicationControlLock);

	return result;
}

/*
 * GPReplicationSetDisconnectTime - Set replication disconnect time.
 *
 * This function is called under walsender to update replication disconnect time.
 * So the walsender's application_name is used.
 */
void
GPReplicationSetDisconnectTime(pg_time_t disconn_time)
{
	GPReplication *slot;

	/* GPRepCtl should be set already. */
	Assert(GPRepCtl != NULL);

	/* Use GPReplicationControlLock to prevent concurrent create/drop */
	LWLockAcquire(GPReplicationControlLock, LW_SHARED);

	slot = RetrieveGPReplication(application_name, false);

	/* Since we need to modify the slot's value, lock the mutex. */
	SpinLockAcquire(&slot->mutex);

	slot->replica_disconnected_at = disconn_time;
	elogif(gp_log_fts >= GPVARS_VERBOSITY_VERBOSE, LOG,
		   "GPReplication: Set replication disconnect time %ld, for application %s",
		   slot->replica_disconnected_at, application_name);

	SpinLockRelease(&slot->mutex);

	LWLockRelease(GPReplicationControlLock);
}

/*
 * GPReplicationRetrieveDisconnectTime - Retrieve replication disconnect time.
 *
 * This function is called under FTS probe process to retrieve replication disconnect time.
 * So app_name is used to specify which GPReplication to retrieve.
 */
pg_time_t
GPReplicationRetrieveDisconnectTime(const char* app_name)
{
	GPReplication	   *slot;
	pg_time_t			disconn_time;

	/* GPRepCtl should be set already. */
	Assert(GPRepCtl != NULL);

	/* Use GPReplicationControlLock to prevent concurrent create/drop */
	LWLockAcquire(GPReplicationControlLock, LW_SHARED);

	slot = RetrieveGPReplication(app_name, false);

	/* Since we need to modify the slot's value, lock the mutex. */
	SpinLockAcquire(&slot->mutex);

	disconn_time = slot->replica_disconnected_at;

	SpinLockRelease(&slot->mutex);

	LWLockRelease(GPReplicationControlLock);

	return disconn_time;
}

/*
 * IsGPReplicationExists - Check whether GPReplication exist for
 * the app_name;
 */
static bool
IsGPReplicationExists(const char *app_name)
{
	GPReplication	   *slot;
	bool				result = false;

	/* GPRepCtl should be set already. */
	Assert(GPRepCtl != NULL);

	/* Use GPReplicationControlLock to prevent concurrent create/drop */
	LWLockAcquire(GPReplicationControlLock, LW_SHARED);
	slot = RetrieveGPReplication(app_name, true);
	result = slot != NULL;
	LWLockRelease(GPReplicationControlLock);

	return result;
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

/*
 * Check the WalSndCtl to obtain if mirror is up or down, if the wal sender is
 * in streaming, and if synchronous replication is enabled or not.
 */
void
GetMirrorStatus(FtsResponse *response)
{
	pg_time_t	walsender_replica_disconnected_at = 0;
	uint32		attempt_replication_times = 0;

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

	LWLockRelease(SyncRepLock);

	/*
	 * Detect the primary-mirror replication attempt count.
	 * If the replication keeps crash, we should consider mark
	 * mirror down directly. Since the walsender keeps resarting,
	 * walsender->replica_disconnected_at keeps updated.
	 * So ignore it.
	 *
	 * If the GPReplication for GP_WALRECEIVER_APPNAME is not exist,
	 * it means the replication has already been stopped.
	 */
	if (IsGPReplicationExists(GP_WALRECEIVER_APPNAME))
	{
		attempt_replication_times = GPReplicationRetrieveAttempts(GP_WALRECEIVER_APPNAME);
		if (attempt_replication_times <= gp_fts_replication_attempt_count)
			walsender_replica_disconnected_at = GPReplicationRetrieveDisconnectTime(GP_WALRECEIVER_APPNAME);
		else
		{
			ereport(LOG,
					(errmsg("Primary-mirror replication streaming already attempted %d times exceed"
					" limit gp_fts_replication_attempt_count %d",
					attempt_replication_times, gp_fts_replication_attempt_count)));
		}
	}

	if (!response->IsMirrorUp)
	{
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
			response->RequestRetry = true;
		}
	}

	response->IsSyncRepEnabled = WalSndCtl->sync_standbys_defined;
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

	GPReplicationDrop(GP_WALRECEIVER_APPNAME);
}

Datum
gp_replication_error(PG_FUNCTION_ARGS)
{
	PG_RETURN_TEXT_P(cstring_to_text(WalSndCtl->error == WALSNDERROR_WALREAD ? "walread" : "none"));
}
