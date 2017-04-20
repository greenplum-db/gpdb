/*-------------------------------------------------------------------------
 *
 * ftsfilerep.c
 *	  Implementation of interface for FireRep-specific segment state machine
 *	  and transitions
 *
 * Copyright (c) 2005-2010, Greenplum Inc.
 * Copyright (c) 2011, EMC Corp.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "cdb/cdbfts.h"
#include "executor/spi.h"
#include "postmaster/fts.h"
#include "postmaster/primary_mirror_mode.h"

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
 * primary/mirror valid states for filerep;
 * we enumerate all possible combined states for two segments
 * before and after state transition, assuming that the first
 * is the old primary and the second is the old mirror;
 * each segment can be:
 *    1) (P)rimary or (M)irror
 *    2) (U)p or (D)own
 *    3) in (S)ync, (R)esync or (C)hangetracking mode -- see filerep's DataState_e
 */
enum filerep_segment_pair_state_e
{
	FILEREP_PUS_MUS = 0,
	FILEREP_PUR_MUR,
	FILEREP_PUC_MDX,
	FILEREP_MDX_PUC,

	FILEREP_SENTINEL
};

 /* we always assume that primary is up */
#define IS_VALID_OLD_STATE_FILEREP(state) \
	((unsigned int)(state) <= FILEREP_PUC_MDX)

#define IS_VALID_NEW_STATE_FILEREP(state) \
	((unsigned int)(state) < FILEREP_SENTINEL)

/*
 * state machine matrix for filerep;
 * we assume that the first segment is the old primary;
 * transitions from "down" to "resync" and from "resync" to "sync" are excluded;
 */
const uint32 state_machine_filerep[][FILEREP_SENTINEL] =
{
/* new: PUS_MUS,   PUR_MUR,               PUC_MDX,   MDX_PUC */
	{         0,         0,             TRANS_U_D, TRANS_D_U },  /* old: PUS_MUS */
	{         0,         0,             TRANS_U_D,         0 },  /* old: PUR_MUR */
	{         0,         0,                     0,         0 },  /* old: PUC_MDX */
};

/* filerep transition type */
typedef enum FilerepModeUpdateLoggingEnum
{
	FilerepModeUpdateLoggingEnum_MirrorToChangeTracking,
	FilerepModeUpdateLoggingEnum_PrimaryToChangeTracking,
} FilerepModeUpdateLoggingEnum;


/*
 * STATIC VARIABLES
 */

/* struct holding segment configuration */
static CdbComponentDatabases *cdb_component_dbs = NULL;


/*
 * FUNCTION PROTOTYPES
 */

static void modeUpdate(int dbid, char *mode, char status, FilerepModeUpdateLoggingEnum logMsgToSend);
static void getHostsByDbid
	(
	int dbid,
	char **hostname_p,
	int *host_port_p,
	int *host_filerep_port_p,
	char **peer_name_p,
	int *peer_pm_port_p,
	int *peer_filerep_port_p
	)
	;


/*
 * Get combined state of primary and mirror for filerep
 */
uint32
FtsGetPairStateFilerep(CdbComponentDatabaseInfo *primary, CdbComponentDatabaseInfo *mirror)
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

		return FILEREP_PUS_MUS;
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

		return FILEREP_PUC_MDX;
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

		return FILEREP_PUR_MUR;
	}

	/* segments are in inconsistent state */
	FtsRequestPostmasterShutdown(primary, mirror);
	return FILEREP_SENTINEL;
}


/*
 * Get new state for primary and mirror using filerep state machine.
 * In case of an invalid old state, log the old state and do not transition.
 */
uint32
FtsTransitionFilerep(uint32 stateOld, uint32 trans)
{
	int i = 0;

	if (!(IS_VALID_OLD_STATE_FILEREP(stateOld)))
		elog(ERROR, "FTS: invalid old state for transition: %d", stateOld);

	/* check state machine for transition */
	for (i = 0; i < FILEREP_SENTINEL; i++)
	{
		if (gp_log_fts >= GPVARS_VERBOSITY_DEBUG)
		{
			elog(LOG, "FTS: state machine: row=%d column=%d val=%d trans=%d comp=%d.",
				 stateOld,
				 i,
				 state_machine_filerep[stateOld][i],
				 trans,
				 state_machine_filerep[stateOld][i] & trans);
		}

		if ((state_machine_filerep[stateOld][i] & trans) > 0)
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
 * resolve new filerep state for primary and mirror
 */
void
FtsResolveStateFilerep(FtsSegmentPairState *pairState)
{
	Assert(IS_VALID_NEW_STATE_FILEREP(pairState->stateNew));

	switch (pairState->stateNew)
	{
		case (FILEREP_PUC_MDX):

			pairState->statePrimary = ftsProbeInfo->fts_status[pairState->primary->dbid];
			pairState->stateMirror = ftsProbeInfo->fts_status[pairState->mirror->dbid];

			/* primary: up, change-tracking */
			pairState->statePrimary &= ~FTS_STATUS_SYNCHRONIZED;
			pairState->statePrimary |= FTS_STATUS_CHANGELOGGING;

			/* mirror: down */
			pairState->stateMirror &= ~FTS_STATUS_ALIVE;

			break;

		case (FILEREP_MDX_PUC):

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

		case (FILEREP_PUS_MUS):
		case (FILEREP_PUR_MUR):
			Assert(!"FTS is not responsible for bringing segments back to life");
			break;

		default:
			Assert(!"Invalid transition in filerep state machine");
	}
}


/*
 * pre-process probe results to take into account some special
 * state-changes that Filerep uses: when the segments have completed
 * re-sync, or when they report an explicit fault.
 *
 * NOTE: we examine pairs of primary-mirror segments; this is requiring
 * for reasoning about state changes.
 */
void
FtsPreprocessProbeResultsFilerep(CdbComponentDatabases *dbs, uint8 *probe_results)
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
		Assert(mirror != NULL && "mirrors should always be there in filerep mode");

		/* peer segments have completed re-sync if primary reports completion */
		if (PROBE_IS_RESYNC_COMPLETE(primary))
		{
			/* update configuration */
			FtsMarkSegmentsInSync(primary, mirror);
		}

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
FtsFailoverFilerep(FtsSegmentStatusChange *changes, int changeCount)
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
				modeUpdate(changes[i].dbid, "primary", 'c', FilerepModeUpdateLoggingEnum_MirrorToChangeTracking);
			}
			else if (old_pri && new_pri)
			{
				/* convert primary to changetracking */
				modeUpdate(changes[i].dbid, "primary", 'c', FilerepModeUpdateLoggingEnum_PrimaryToChangeTracking);
			}
		}
	}
}


static void
modeUpdate(int dbid, char *mode, char status, FilerepModeUpdateLoggingEnum logMsgToSend)
{
	char cmd[SYS_CMD_BUF_SIZE];
	int cmd_status;

	char *seg_addr = NULL;
	char *peer_addr = NULL;
	int seg_pm_port = -1;
	int seg_rep_port = -1;
	int peer_rep_port = -1;
	int peer_pm_port = -1;

	char runInBg = '&';

	Assert(dbid >= 0);
	Assert(mode != NULL);

	getHostsByDbid(dbid, &seg_addr, &seg_pm_port, &seg_rep_port, &peer_addr, &peer_pm_port, &peer_rep_port);

	Assert(seg_addr != NULL);
	Assert(peer_addr != NULL);
	Assert(seg_pm_port > 0);
	Assert(seg_rep_port > 0);
	Assert(peer_pm_port > 0);
	Assert(peer_rep_port > 0);

	switch (logMsgToSend)
	{
		case FilerepModeUpdateLoggingEnum_MirrorToChangeTracking:
			ereport(LOG,
					(errmsg("FTS: mirror (dbid=%d) on %s:%d taking over as primary in change-tracking mode.",
							dbid, seg_addr, seg_pm_port ),
					 errSendAlert(true)));
			break;
		case FilerepModeUpdateLoggingEnum_PrimaryToChangeTracking:
			ereport(LOG,
					(errmsg("FTS: primary (dbid=%d) on %s:%d transitioning to change-tracking mode, mirror marked as down.",
							dbid, seg_addr, seg_pm_port ),
					 errSendAlert(true)));
			break;
	}

	/* check if parallel segment transition is activated */
	if (!gp_fts_transition_parallel)
	{
		runInBg = ' ';
	}

	/* issue the command to change modes */
	(void) snprintf
		(
		cmd, sizeof(cmd),
		"gp_primarymirror -m %s -s %c -H %s -P %d -R %d -h %s -p %d -r %d -n %d -t %d %c",
		mode,
		status,
		seg_addr,
		seg_pm_port,
		seg_rep_port,
		peer_addr,
		peer_pm_port,
		peer_rep_port,
		gp_fts_transition_retries,
		gp_fts_transition_timeout,
		runInBg
		)
		;

	Assert(strlen(cmd) < sizeof(cmd) - 1);

	if (gp_log_fts >= GPVARS_VERBOSITY_DEBUG)
		elog(LOG, "FTS: gp_primarymirror command is [%s].", cmd);

	cmd_status = system(cmd);

	if (cmd_status == -1)
	{
		elog(ERROR, "FTS: failed to execute command: %s (%m).", cmd);
	}
	else if (cmd_status != 0)
	{
		elog(ERROR, "FTS: segment transition failed: %s (exit code %d).", cmd, cmd_status);
	}
}


static void
getHostsByDbid(int dbid, char **hostname_p, int *host_port_p, int *host_filerep_port_p, char **peer_name_p,
			int *peer_pm_port_p, int *peer_filerep_port_p)
{
	bool found;
	int i;
	int content_id=-1;

	Assert(dbid >= 0);
	Assert(hostname_p != NULL);
	Assert(host_port_p != NULL);
	Assert(host_filerep_port_p != NULL);
	Assert(peer_name_p != NULL);
	Assert(peer_pm_port_p != NULL);
	Assert(peer_filerep_port_p != NULL);

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
			*host_filerep_port_p = segInfo->filerep_port;
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
			*peer_filerep_port_p = segInfo->filerep_port;
			found = true;
			break;
		}
	}

	if (!found)
	{
		elog(LOG, "FTS: could not find mirror-peer for dbid %d.", dbid);
		*peer_name_p = NULL;
		*peer_pm_port_p = -1;
		*peer_filerep_port_p = -1;
	}
}


/* EOF */
