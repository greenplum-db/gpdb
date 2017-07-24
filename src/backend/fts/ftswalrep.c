/*-------------------------------------------------------------------------
 *
 * ftswalrep.c
 *	  Implementation of interface for WALrep-specific segment state machine
 *	  and transitions
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "gp-libpq-fe.h"
#include "gp-libpq-int.h"
#include "cdb/cdbfts.h"
#include "cdb/cdbvars.h"
#include "libpq/ip.h"
#include "executor/spi.h"
#include "postmaster/fts.h"
#include "postmaster/primary_mirror_mode.h"
#include "postmaster/primary_mirror_transition_client.h"

#include "cdb/ml_ipc.h" /* gettime_elapsed_ms */


/*
 * CONSTANTS
 */

/* buffer size for system command */
#define SYS_CMD_BUF_SIZE     4096


/*
 * ENUMS
 */

/*
 * primary/mirror valid states for WALrep;
 * we enumerate all possible combined states for two segments
 * before and after state transition, assuming that the first
 * is the old primary and the second is the old mirror;
 * each segment can be:
 *    1) (P)rimary or (M)irror
 *    2) (U)p or (D)own
 *    3) in (S)treaming, (C)atch-up or (A)rchiving mode -- see WALrep's PrimaryState
 */
enum walrep_segment_pair_state_e
{
	WALREP_PUS_MUS = 0,
	WALREP_PUC_MUC,
	WALREP_PUA_MDX,
	WALREP_MDX_PUA,
	WALREP_SENTINEL
};

 /* we always assume that primary is up */
#define IS_VALID_OLD_STATE_WALREP(state) \
	((unsigned int)(state) <= WALREP_PUA_MDX)

#define IS_VALID_NEW_STATE_WALREP(state) \
	((unsigned int)(state) < WALREP_SENTINEL)

/*
 * state machine matrix for walrep;
 * we assume that the first segment is the old primary;
 * transitions from "down" to "resync" and from "resync" to "sync" are excluded;
 */
const uint32 state_machine_walrep[][WALREP_SENTINEL] =
{
/* new: PUS_MUS,   PUC_MUC,               PUA_MDX,   MDX_PUA */
	{         0,         0,             TRANS_U_D, TRANS_D_U },  /* old: PUS_MUS */
	{         0,         0,             TRANS_U_D,         0 },  /* old: PUC_MUC */
	{         0,         0,                     0,         0 },  /* old: PUA_MDX */
};

/* WALrep transition type */
typedef enum WALrepModeUpdateLoggingEnum
{
	WALrepModeUpdateLoggingEnum_MirrorToArchiving,
	WALrepModeUpdateLoggingEnum_PrimaryToArchiving,
} WALrepModeUpdateLoggingEnum;


/*
 * STATIC VARIABLES
 */

/* struct holding segment configuration */
static CdbComponentDatabases *cdb_component_dbs = NULL;


/*
 * FUNCTION PROTOTYPES
 */

static void modeUpdate(int dbid, char *mode, char status, WALrepModeUpdateLoggingEnum logMsgToSend);
static void getHostsByDbid(int dbid,
						   char **hostname_p,
						   int *host_port_p,
						   char **peer_name_p,
						   int *peer_pm_port_p);


/*
 * Get combined state of primary and mirror for WALrep
 */
uint32
FtsGetPairStateWalRep(CdbComponentDatabaseInfo *primary, CdbComponentDatabaseInfo *mirror)
{
	/* check for inconsistent segment state */
	if (!FTS_STATUS_ISALIVE(primary->dbid, ftsProbeInfo->fts_status) ||
	    !FTS_STATUS_ISPRIMARY(primary->dbid, ftsProbeInfo->fts_status) ||
	    FTS_STATUS_ISPRIMARY(mirror->dbid, ftsProbeInfo->fts_status) ||
	    FTS_STATUS_IS_CHANGELOGGING(mirror->dbid, ftsProbeInfo->fts_status))
	{
		FtsRequestPostmasterShutdown(primary, mirror);
	}

	if (FTS_STATUS_IS_SYNCED(primary->dbid, ftsProbeInfo->fts_status) &&
	    !FTS_STATUS_IS_CHANGELOGGING(primary->dbid, ftsProbeInfo->fts_status) &&
	    FTS_STATUS_ISALIVE(mirror->dbid, ftsProbeInfo->fts_status) &&
	    FTS_STATUS_IS_SYNCED(mirror->dbid, ftsProbeInfo->fts_status))
	{
		/* Primary: Up, Sync - Mirror: Up, Sync */
		if (gp_log_fts > GPVARS_VERBOSITY_VERBOSE)
		{
			elog(LOG, "FTS: last state: primary (dbid=%d) sync, mirror (dbid=%d) sync.",
				 primary->dbid, mirror->dbid);
		}

		return WALREP_PUS_MUS;
	}

	if (!FTS_STATUS_IS_SYNCED(primary->dbid, ftsProbeInfo->fts_status) &&
	    FTS_STATUS_IS_CHANGELOGGING(primary->dbid, ftsProbeInfo->fts_status) &&
	    !FTS_STATUS_ISALIVE(mirror->dbid, ftsProbeInfo->fts_status))
	{
		/* Primary: Up, Changetracking - Mirror: Down */
		if (gp_log_fts >= GPVARS_VERBOSITY_VERBOSE)
		{
			elog(LOG, "FTS: last state: primary (dbid=%d) change-tracking, mirror (dbid=%d) down.",
				 primary->dbid, mirror->dbid);
		}

		return WALREP_PUA_MDX;
	}

	if (!FTS_STATUS_IS_SYNCED(primary->dbid, ftsProbeInfo->fts_status) &&
	    !FTS_STATUS_IS_CHANGELOGGING(primary->dbid, ftsProbeInfo->fts_status) &&
	    FTS_STATUS_ISALIVE(mirror->dbid, ftsProbeInfo->fts_status) &&
	    !FTS_STATUS_IS_SYNCED(mirror->dbid, ftsProbeInfo->fts_status))
	{
		/* Primary: Up, Resync - Mirror: Up, Resync */
		if (gp_log_fts >= GPVARS_VERBOSITY_VERBOSE)
		{
			elog(LOG, "FTS: last state: primary (dbid=%d) resync, mirror (dbid=%d) resync.",
				 primary->dbid, mirror->dbid);
		}

		return WALREP_PUC_MUC;
	}

	/* segments are in inconsistent state */
	FtsRequestPostmasterShutdown(primary, mirror);
	return WALREP_SENTINEL;
}


/*
 * Get new state for primary and mirror using WALrep state machine.
 * In case of an invalid old state, log the old state and do not transition.
 */
uint32
FtsTransitionWalRep(uint32 stateOld, uint32 trans)
{
	int i = 0;

	if (!(IS_VALID_OLD_STATE_WALREP(stateOld)))
		elog(ERROR, "FTS: invalid old state for transition: %d", stateOld);

	/* check state machine for transition */
	for (i = 0; i < WALREP_SENTINEL; i++)
	{
		if (gp_log_fts >= GPVARS_VERBOSITY_DEBUG)
		{
			elog(LOG, "FTS: state machine: row=%d column=%d val=%d trans=%d comp=%d.",
				 stateOld,
				 i,
				 state_machine_walrep[stateOld][i],
				 trans,
				 state_machine_walrep[stateOld][i] & trans);
		}

		if ((state_machine_walrep[stateOld][i] & trans) > 0)
		{
			if (gp_log_fts >= GPVARS_VERBOSITY_VERBOSE)
			{
				elog(LOG, "FTS: state machine: match found, new state: %d.", i);
			}
			return i;
		}
	}

	return stateOld;
}


/*
 * resolve new WALrep state for primary and mirror
 */
void
FtsResolveStateWalRep(FtsSegmentPairState *pairState)
{
	Assert(IS_VALID_NEW_STATE_WALREP(pairState->stateNew));

	switch (pairState->stateNew)
	{
		case (WALREP_PUA_MDX):

			pairState->statePrimary = ftsProbeInfo->fts_status[pairState->primary->dbid];
			pairState->stateMirror = ftsProbeInfo->fts_status[pairState->mirror->dbid];

			/* primary: up, change-tracking */
			pairState->statePrimary &= ~FTS_STATUS_SYNCHRONIZED;
			pairState->statePrimary |= FTS_STATUS_CHANGELOGGING;

			/* mirror: down */
			pairState->stateMirror &= ~FTS_STATUS_ALIVE;

			break;

		case (WALREP_MDX_PUA):

			Assert(FTS_STATUS_ISALIVE(pairState->mirror->dbid, ftsProbeInfo->fts_status));
			Assert(FTS_STATUS_IS_SYNCED(pairState->mirror->dbid, ftsProbeInfo->fts_status));

			pairState->statePrimary = ftsProbeInfo->fts_status[pairState->primary->dbid];
			pairState->stateMirror = ftsProbeInfo->fts_status[pairState->mirror->dbid];

			/* primary: down, becomes mirror */
			pairState->statePrimary &= ~FTS_STATUS_PRIMARY;
			pairState->statePrimary &= ~FTS_STATUS_ALIVE;

			/* mirror: up, change-tracking, promoted to primary */
			pairState->stateMirror |= FTS_STATUS_PRIMARY;
			pairState->stateMirror &= ~FTS_STATUS_SYNCHRONIZED;
			pairState->stateMirror |= FTS_STATUS_CHANGELOGGING;

			break;

		case (WALREP_PUS_MUS):
		case (WALREP_PUC_MUC):
			Assert(!"FTS is not responsible for bringing segments back to life");
			break;

		default:
			Assert(!"Invalid transition in WALrep state machine");
	}
}


/*
 * pre-process probe results to take into account some special
 * state-changes that WALrep uses: when the segments have completed
 * re-sync, or when they report an explicit fault.
 *
 * NOTE: we examine pairs of primary-mirror segments; this is requiring
 * for reasoning about state changes.
 */
void
FtsPreprocessProbeResultsWalRep(CdbComponentDatabases *dbs, uint8 *probe_results)
{
	int i = 0;
	cdb_component_dbs = dbs;
	Assert(cdb_component_dbs != NULL);

	for (i=0; i < cdb_component_dbs->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *segInfo = &cdb_component_dbs->segment_db_info[i];
		CdbComponentDatabaseInfo *primary = NULL, *mirror = NULL;

		if (!SEGMENT_IS_ACTIVE_PRIMARY(segInfo))
		{
			continue;
		}

		primary = segInfo;
		mirror = FtsGetPeerSegment(primary->segindex, primary->dbid);
		Assert(mirror != NULL && "mirrors should always be there in WALrep mode");

		/*
		 * Decide which segments to consider "down"
		 *
		 * There are a few possibilities here:
		 *    1) primary in crash fault
		 *             primary considered dead
		 *    2) mirror in crash fault
		 *             mirror considered dead
		 *    3) primary in networking fault, mirror has no fault or mirroring fault
		 *             primary considered dead, mirror considered alive
		 *    4) primary in mirroring fault
		 *             primary considered alive, mirror considered dead
		 */
		if (PROBE_HAS_FAULT_CRASH(primary))
		{
			elog(LOG, "FTS: primary (dbid=%d) reported crash, considered to be down.",
				 primary->dbid);

			/* consider primary dead -- case (1) */
			probe_results[primary->dbid] &= ~PROBE_ALIVE;
		}

		if (PROBE_HAS_FAULT_CRASH(mirror))
		{
			elog(LOG, "FTS: mirror (dbid=%d) reported crash, considered to be down.",
				 mirror->dbid);

			/* consider mirror dead -- case (2) */
			probe_results[mirror->dbid] &= ~PROBE_ALIVE;
		}

		if (PROBE_HAS_FAULT_NET(primary))
		{
			if (PROBE_IS_ALIVE(mirror) && !PROBE_HAS_FAULT_NET(mirror))
			{
				elog(LOG, "FTS: primary (dbid=%d) reported networking fault "
						  "while mirror (dbid=%d) is accessible, "
						  "primary considered to be down.",
					 primary->dbid, mirror->dbid);

				/* consider primary dead -- case (3) */
				probe_results[primary->dbid] &= ~PROBE_ALIVE;
			}
			else
			{
				if (PROBE_IS_ALIVE(primary))
				{
					elog(LOG, "FTS: primary (dbid=%d) reported networking fault "
							  "while mirror (dbid=%d) is unusable, "
							  "mirror considered to be down.",
						 primary->dbid, mirror->dbid);

					/* mirror cannot be used, consider mirror dead -- case (2) */
					probe_results[mirror->dbid] &= ~PROBE_ALIVE;
				}
			}
		}

		if (PROBE_IS_ALIVE(primary) && PROBE_HAS_FAULT_MIRROR(primary))
		{
			elog(LOG, "FTS: primary (dbid=%d) reported mirroring fault with mirror (dbid=%d), "
					  "mirror considered to be down.",
				 primary->dbid, mirror->dbid);

			/* consider mirror dead -- case (4) */
			probe_results[mirror->dbid] &= ~PROBE_ALIVE;
		}

		/*
		 * clear resync and fault flags as they aren't needed any further
		 */
		probe_results[primary->dbid] &= ~PROBE_FAULT_CRASH;
		probe_results[primary->dbid] &= ~PROBE_FAULT_MIRROR;
		probe_results[primary->dbid] &= ~PROBE_FAULT_NET;
		probe_results[primary->dbid] &= ~PROBE_RESYNC_COMPLETE;
		probe_results[mirror->dbid] &= ~PROBE_FAULT_CRASH;
		probe_results[mirror->dbid] &= ~PROBE_FAULT_MIRROR;
		probe_results[mirror->dbid] &= ~PROBE_FAULT_NET;
		probe_results[mirror->dbid] &= ~PROBE_RESYNC_COMPLETE;
	}
}



/*
 * transition segment to new state
 */
void
FtsFailoverWalRep(FtsSegmentStatusChange *changes, int changeCount)
{
	int i;

	if (gp_log_fts >= GPVARS_VERBOSITY_VERBOSE)
		FtsDumpChanges(changes, changeCount);

	/*
	 * For each failed primary segment:
	 *   1) Convert the mirror into primary.
	 *
	 * For each failed mirror segment:
	 *   1) Convert the primary into change-logging.
	 */
	for (i=0; i < changeCount; i++)
	{
		bool new_alive, old_alive;
		bool new_pri, old_pri;

		new_alive = (changes[i].newStatus & FTS_STATUS_ALIVE ? true : false);
		old_alive = (changes[i].oldStatus & FTS_STATUS_ALIVE ? true : false);

		new_pri = (changes[i].newStatus & FTS_STATUS_PRIMARY ? true : false);
		old_pri = (changes[i].oldStatus & FTS_STATUS_PRIMARY ? true : false);

		if (!new_alive && old_alive)
		{
			/*
			 * this is a segment that went down, nothing to tell it, it doesn't
			 * matter whether it was primary or mirror here.
			 *
			 * Nothing to do here.
			 */
			if (new_pri)
			{
				elog(LOG, "FTS: failed segment (dbid=%d) was marked as the new primary.", changes[i].dbid);
				FtsDumpChanges(changes, changeCount);
				/* NOTE: we don't apply the state change here. */
			}
		}
		else if (new_alive && !old_alive)
		{
			/* this is a segment that came back up ? Nothing to do here. */
			if (old_pri)
			{
				elog(LOG, "FTS: failed segment (dbid=%d) is alive and marked as the old primary.", changes[i].dbid);
				FtsDumpChanges(changes, changeCount);
				/* NOTE: we don't apply the state change here. */
			}
		}
		else if (new_alive && old_alive)
		{
			Assert(changes[i].newStatus & FTS_STATUS_CHANGELOGGING);

			/* this is a segment that may require a mode-change */
			if (old_pri && !new_pri)
			{
				/* demote primary to mirror ?! Nothing to do here. */
			}
			else if (new_pri && !old_pri)
			{
				/* promote mirror to primary */
				modeUpdate(changes[i].dbid, "primary", 'c', WALrepModeUpdateLoggingEnum_MirrorToArchiving);
			}
			else if (old_pri && new_pri)
			{
				/* convert primary to changetracking */
				modeUpdate(changes[i].dbid, "primary", 'c', WALrepModeUpdateLoggingEnum_PrimaryToArchiving);
			}
		}
	}
}

static bool
gpCheckForNeedToExitFn(void)
{
	return false;
}

static void
gpMirrorErrorLogFunction(char *str)
{
	elog(LOG, "%s\n", str);
}

static void
gpMirrorReceivedDataCallbackFunction(char *buf)
{
	elog(LOG, "%s\n", buf);
}

/**
 * *addrList will be filled in with the address(es) of the host/port when true is returned
 *
 * host/port may not be NULL
 */
static bool
determineTargetHost( struct addrinfo **addrList, char *host, int port)
{
	struct addrinfo hint;
	int			ret;

	*addrList = NULL;

	/* Initialize hint structure */
	MemSet(&hint, 0, sizeof(hint));
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_family = AF_UNSPEC;

	/* Using pghost, so we have to look-up the hostname */
	hint.ai_family = AF_UNSPEC;

	/* convert port to string */
	char port_string[64];
	sprintf(port_string, "%d", port);

	/* Use pg_getaddrinfo_all() to resolve the address */
	ret = pg_getaddrinfo_all(host, port_string, &hint, addrList);
	if (ret || ! *addrList)
	{
		fprintf(stderr,"could not translate host name \"%s\" to address: %s\n", host, gai_strerror(ret));
		return false;
	}
	return true;
}

/* buffer size for message to segment */
#define SEGMENT_MSG_BUF_SIZE     4096

static int
sendTransitionCommand(char *mode, char status, char *seg_addr, int seg_pm_port, int seg_rep_port,
					  char *peer_addr, int peer_pm_port, int peer_rep_port)
{
	struct addrinfo *addrList = NULL;
	int msgLen = 0;
	char msgBuffer[SEGMENT_MSG_BUF_SIZE];
	char *msg = NULL;

	/* build message */
	msgLen = snprintf(
			msgBuffer, sizeof(msgBuffer),
			"%s\n%c\n%s\n%d\n%s\n%d\n%d\n",
			mode,
			status,
			seg_addr,
			seg_rep_port,
			peer_addr,
			peer_rep_port,
			peer_pm_port
			);

	msg = msgBuffer;

	/* find the target machine */
	if (!determineTargetHost(&addrList, seg_addr, seg_pm_port))
	{
		return TRANS_ERRCODE_ERROR_HOST_LOOKUP_FAILED;
	}

	/* check for errors while building the message */
	if (msg == NULL || msgLen >= sizeof(msgBuffer))
	{
		return TRANS_ERRCODE_ERROR_READING_INPUT;
	}

	/* send the message */
	PrimaryMirrorTransitionClientInfo client;
	client.receivedDataCallbackFn = gpMirrorReceivedDataCallbackFunction;
	client.errorLogFn = gpMirrorErrorLogFunction;
	client.checkForNeedToExitFn = gpCheckForNeedToExitFn;
	return sendTransitionMessage(&client, addrList, msg, msgLen, gp_fts_transition_retries, gp_fts_transition_timeout);
}

static void
modeUpdate(int dbid, char *mode, char status, WALrepModeUpdateLoggingEnum logMsgToSend)
{
	char *seg_addr = NULL;
	char *peer_addr = NULL;
	int seg_pm_port = -1;
	int seg_rep_port = -1;
	int peer_rep_port = -1;
	int peer_pm_port = -1;

	Assert(dbid >= 0);
	Assert(mode != NULL);

	getHostsByDbid(dbid, &seg_addr, &seg_pm_port, &peer_addr, &peer_pm_port);

	Assert(seg_addr != NULL);
	Assert(peer_addr != NULL);
	Assert(seg_pm_port > 0);
	Assert(peer_pm_port > 0);

	switch (logMsgToSend)
	{
		case WALrepModeUpdateLoggingEnum_MirrorToArchiving:
			ereport(LOG,
					(errmsg("FTS: mirror (dbid=%d) on %s:%d taking over as primary in archiving mode.",
							dbid, seg_addr, seg_pm_port ),
					 errSendAlert(true)));
			break;
		case WALrepModeUpdateLoggingEnum_PrimaryToArchiving:
			ereport(LOG,
					(errmsg("FTS: primary (dbid=%d) on %s:%d transitioning to archiving mode, mirror marked as down.",
							dbid, seg_addr, seg_pm_port ),
					 errSendAlert(true)));
			break;
	}

	int return_code;
	return_code = sendTransitionCommand(mode, status, seg_addr, seg_pm_port, seg_rep_port, peer_addr, peer_pm_port, peer_rep_port);

	if (return_code == -1)
	{
		elog(ERROR, "FTS: failed to execute");
	}
	else if (return_code != 0)
	{
		elog(ERROR, "FTS: segment transition failed");
	}
}


static void
getHostsByDbid(int dbid, char **hostname_p, int *host_port_p, char **peer_name_p,
			int *peer_pm_port_p)
{
	bool found;
	int i;
	int content_id=-1;

	Assert(dbid >= 0);
	Assert(hostname_p != NULL);
	Assert(host_port_p != NULL);
	Assert(peer_name_p != NULL);
	Assert(peer_pm_port_p != NULL);

	found = false;

	/*
	 * We're going to scan the segment-dbs array twice.
	 *
	 * On the first pass we get our dbid, on the second pass we get our mirror-peer.
	 */
	for (i=0; i < cdb_component_dbs->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *segInfo = &cdb_component_dbs->segment_db_info[i];

		if (segInfo->dbid == dbid)
		{
			*hostname_p = segInfo->address;
			*host_port_p = segInfo->port;
			content_id = segInfo->segindex;
			found = true;
			break;
		}
	}

	if (!found)
	{
		elog(FATAL, "FTS: could not find entry for dbid %d.", dbid);
	}

	found = false;

	/* second pass, find the mirror-peer */
	for (i=0; i < cdb_component_dbs->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *segInfo = &cdb_component_dbs->segment_db_info[i];

		if (segInfo->segindex == content_id && segInfo->dbid != dbid)
		{
			*peer_name_p = segInfo->address;
			*peer_pm_port_p = segInfo->port;
			found = true;
			break;
		}
	}

	if (!found)
	{
		elog(LOG, "FTS: could not find mirror-peer for dbid %d.", dbid);
		*peer_name_p = NULL;
		*peer_pm_port_p = -1;
	}
}


/* EOF */
