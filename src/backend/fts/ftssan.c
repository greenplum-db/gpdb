/*-------------------------------------------------------------------------
 *
 * ftssan.c
 *	  Implementation of interface for FireRep-specific segment state machine
 *	  and transitions
 *
 * Copyright (c) 2005-2010, Greenplum Inc.
 * Copyright (c) 2011, EMC Corp.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "catalog/catalog.h"
#include "catalog/gp_san_config.h"
#include "catalog/pg_filespace.h"
#include "catalog/pg_filespace_entry.h"
#include "catalog/pg_tablespace.h"
#include "cdb/cdbfts.h"
#include "executor/spi.h"
#include "postmaster/fts.h"
#include "postmaster/primary_mirror_mode.h"
#include "utils/fmgroids.h"

#include "cdb/ml_ipc.h" /* gettime_elapsed_ms */


#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

#include <unistd.h>


/*
 * CONSTANTS
 */

/* buffer size for SQL command */
#define SQL_CMD_BUF_SIZE     1024

/* buffer size for system command */
#define SYS_CMD_BUF_SIZE     4096


/*
 * ENUMS
 */

/*
 * primary/mirror valid states for SAN;
 * we enumerate all possible combined states for two segments
 * before and after state transition, assuming that the first
 * is the old primary and the second is the old mirror;
 * each segment can be:
 *    1) (P)rimary or (M)irror
 *    2) (U)p or (D)own
 */
enum san_segment_pair_state_e
{
	SAN_PU_MU = 0,
	SAN_PU_MD,
	SAN_MD_PU,

	SAN_SENTINEL
};

#define IS_VALID_OLD_STATE_SAN(state) \
	((unsigned int)(state) <= SAN_PU_MD)

#define IS_VALID_NEW_STATE_SAN(state) \
	((unsigned int)(state) < SAN_SENTINEL)

/*
 * state machine matrix for SAN;
 * we assume that the first segment is the old primary
 * transition from "down" to "up" is excluded;
 */
const uint32 state_machine_san[][SAN_SENTINEL] =
{
/* new:   PU_MU,     PU_MD,     MD_PU */
	{         0, TRANS_U_D, TRANS_D_U },  /* old: PU_MU */
	{         0,         0,         0 },  /* old: PU_MD */
};


/*
 * FUNCTION PROTOTYPES
 */

static void getMountsForDbid(int dbid, List **mountp);
static char mountMaintenanceForMpid(int mpid);
static void startSegmentForDbid(int dbid);


/*
 * Get combined state of primary and mirror for SAN
 */
uint32
FtsGetPairStateSAN(CdbComponentDatabaseInfo *primary, CdbComponentDatabaseInfo *mirror)
{
	Assert(FTS_STATUS_ISALIVE(primary->dbid, ftsProbeInfo->fts_status));
	Assert(FTS_STATUS_ISPRIMARY(primary->dbid, ftsProbeInfo->fts_status));
	Assert(!FTS_STATUS_ISPRIMARY(mirror->dbid, ftsProbeInfo->fts_status));

	if (FTS_STATUS_ISALIVE(mirror->dbid, ftsProbeInfo->fts_status))
	{
		/* Primary: Up - Mirror: Up */

		if (gp_log_fts >= GPVARS_VERBOSITY_VERBOSE)
		{
			elog(LOG, "probePublishUpdate: last state: primary dbid=%d up, mirror dbid=%d up",
				 primary->dbid, mirror->dbid);
		}

		return SAN_PU_MU;
	}
	else
	{
		/* Primary: Up - Mirror: Down */
		if (gp_log_fts >= GPVARS_VERBOSITY_VERBOSE)
		{
			elog(LOG, "probePublishUpdate: last state: primary dbid=%d up, mirror dbid=%d dead",
				 primary->dbid, mirror->dbid);
		}

		return SAN_PU_MD;
	}
}


/*
 *  get new state for primary and mirror using SAN state machine
 */
uint32
FtsTransitionSAN(uint32 stateOld, uint32 trans)
{
	Assert(IS_VALID_OLD_STATE_SAN(stateOld));

	int i = 0;

	/* check state machine for transition */
	for (i = 0; i < SAN_SENTINEL; i++)
	{
		if (gp_log_fts >= GPVARS_VERBOSITY_DEBUG)
		{
			elog(LOG, "ftsProbe: state machine: row=%d column=%d val=%d trans=%d comp=%d",
				 stateOld, i, state_machine_san[stateOld][i], trans, state_machine_san[stateOld][i] & trans);
		}

		if ((state_machine_san[stateOld][i] & trans) > 0)
		{
			if (gp_log_fts >= GPVARS_VERBOSITY_VERBOSE)
			{
				elog(LOG, "ftsProbe: state machine: match found, new state: %d", i);
			}
			return i;
		}
	}

	return stateOld;
}


/*
 * resolve new SAN state for primary and mirror
 */
void
FtsResolveStateSAN(FtsSegmentPairState *pairState)
{
	Assert(IS_VALID_NEW_STATE_SAN(pairState->stateNew));

	switch (pairState->stateNew)
	{
		case (SAN_PU_MD):

			pairState->statePrimary = ftsProbeInfo->fts_status[pairState->primary->dbid];
			pairState->stateMirror = ftsProbeInfo->fts_status[pairState->mirror->dbid];

			/* mirror: down */
			pairState->stateMirror &= ~FTS_STATUS_ALIVE;

			break;

		case (SAN_MD_PU):

			Assert(FTS_STATUS_ISALIVE(pairState->mirror->dbid, ftsProbeInfo->fts_status));

			pairState->statePrimary = ftsProbeInfo->fts_status[pairState->primary->dbid];
			pairState->stateMirror = ftsProbeInfo->fts_status[pairState->mirror->dbid];

			/* primary: down, becomes mirror */
			pairState->statePrimary &= ~FTS_STATUS_PRIMARY;
			pairState->statePrimary &= ~FTS_STATUS_ALIVE;

			/* mirror: up, change-tracking, promoted to primary */
			pairState->stateMirror |= FTS_STATUS_PRIMARY;

			break;

		case (SAN_PU_MU):
			Assert(!"FTS is not responsible for bringing segments back to life");
			break;

		default:
			Assert(!"Invalid transition in filerep state machine");
	}
}


void
FtsFailoverSAN(FtsSegmentStatusChange *changes, int changeCount, char *time_str)
{
	char cmd[256];
	List *mps=NIL;
	ListCell *cell;
	int i;

	if (gp_log_fts >= GPVARS_VERBOSITY_DEBUG)
	{
		elog(LOG, "SAN_Failover: got %d changes", changeCount);

		FtsDumpChanges(changes, changeCount);
	}

	/*
	 * For each failed segment:
	 *   1) Find the mount-point.
	 *   2) Clean it up on the primary (if possible).
	 *   3) Mount it on the failover.
	 *   4) Start all segments using the mount-point on their failover location.
	 */
	for (i=0; i < changeCount; i++)
	{
		bool new_alive, old_alive, new_pri;

		new_alive = (changes[i].newStatus & FTS_STATUS_ALIVE ? true : false);
		old_alive = (changes[i].oldStatus & FTS_STATUS_ALIVE ? true : false);

		new_pri = (changes[i].newStatus & FTS_STATUS_PRIMARY ? true : false);

		if (!new_alive && old_alive)
		{
			/* dead marked as new primary means double-failure -- skip */
			if (new_pri)
				continue;

			getMountsForDbid(changes[i].dbid, &mps);
			/* mps is our deduped list of mountpoints */

			Assert(list_length(mps) > 0);
		}
	}

	/* fix up the mountpoints (the list has no duplicates) */
	foreach(cell, mps)
	{
		char	new_active_host;
		int		mpid = lfirst_int(cell);

		if (gp_log_fts >= GPVARS_VERBOSITY_DEBUG)
			elog(LOG, "mpid %d", mpid);

		new_active_host = mountMaintenanceForMpid(mpid);

		snprintf(cmd, sizeof(cmd), "update %s set active_host = '%c' where mountid=%d",
				 GpSanConfigRelationName, new_active_host, mpid);

		/* executeEntrySQL(entryConn, cmd); */

		snprintf(cmd, sizeof(cmd),
				 "insert into gp_configuration_history values ('%s', %d, 'FTSPROBE: changed active host for mount-id %d to %c')",
				 time_str, -1, mpid, new_active_host);

		/* executeEntrySQL(entryConn, cmd); */
	}

	/* Now we're ready to start up the failover segments */
	for (i=0; i < changeCount; i++)
	{
		bool new_alive, new_pri, old_pri;

		new_pri = (changes[i].newStatus & FTS_STATUS_PRIMARY ? true : false);
		old_pri = (changes[i].oldStatus & FTS_STATUS_PRIMARY ? true : false);

		new_alive = (changes[i].newStatus & FTS_STATUS_ALIVE ? true : false);

		/* dead marked as new primary means double-failure -- skip */
		if (new_pri && !new_alive)
			continue;

		if (new_pri && !old_pri)
		{
			startSegmentForDbid(changes[i].dbid);

			snprintf(cmd, sizeof(cmd),
					 "insert into gp_configuration_history values ('%s', %d, 'FTSPROBE: started segment moving mountpoints.')",
					 time_str, changes[i].dbid);
			/* executeEntrySQL(entryConn, cmd); */
		}
	}
}

/*
 * Generate a list of the mountpoints that we have to work on.
 * Duplicates are eliminated by using list_append_unique_int()
 */
static void
getMountsForDbid(int dbid, List **mountp)
{
	Relation	conf_rel;
	HeapTuple	conf_tup;
	HeapScanDesc conf_scan;
	bool	isNull=true;

	ScanKeyData key[1];

	Assert(dbid > 0);

	ScanKeyInit(&key[0],
				Anum_gp_segment_configuration_dbid,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum((int16)dbid));

	conf_rel = heap_open(GpSegmentConfigRelationId, AccessShareLock);
	conf_scan = heap_beginscan(conf_rel, SnapshotNow, 1, key);

	while (HeapTupleIsValid(conf_tup = heap_getnext(conf_scan, ForwardScanDirection)))
	{
		int i;
		int2vector *san_mounts_v;
		Datum	tup_dbid;
		Datum	conf_datum;

		tup_dbid = heap_getattr(conf_tup, Anum_gp_segment_configuration_dbid, conf_rel->rd_att, &isNull);
		Assert(!isNull); /* primary key better not be null ! */

		if ((int)DatumGetInt16(tup_dbid) != dbid)
		{
			elog(ERROR, "getMounts_forDbid: failed dbid lookup in segment configuration");
		}

		/* got our dbid, get our array of mountpoints. */
		conf_datum = heap_getattr(conf_tup, Anum_gp_segment_configuration_san_mounts, conf_rel->rd_att, &isNull);

		if (isNull)
			continue; /* Can this happen ? */

		san_mounts_v = DatumGetPointer(conf_datum);
		Assert(san_mounts_v != NULL);

		if (gp_log_fts >= GPVARS_VERBOSITY_DEBUG)
			elog(LOG, "Got our vector of mountpoints length %d", san_mounts_v->dim1);
		for (i=0; i < san_mounts_v->dim1; i++)
		{
			*mountp = list_append_unique_int(*mountp, san_mounts_v->values[i]);
		}
		break;
	}

	heap_endscan(conf_scan);
	heap_close(conf_rel, AccessShareLock);
}

/* convenience macro for sucking catalog info out of our table */
#define get_heap_attr(datum, prefix, ATTR, tuple, rel)					\
	{																	\
		bool isNull=true;												\
																		\
		datum = heap_getattr((tuple), prefix ## ATTR , (rel)->rd_att, &isNull); \
		if (isNull)														\
			elog(FATAL, "Fault reponse error: %s IS NULL", #ATTR);		\
	}

#define get_config_attr(datum, ATTR, tuple, rel) get_heap_attr(datum, Anum_gp_segment_configuration_, ATTR, tuple, rel)

static void
startSegmentForDbid(int dbid)
{
	Relation	conf_rel, fse_rel;
	HeapTuple	conf_tup, fse_tup;
	HeapScanDesc conf_scan, fse_scan;
	ScanKeyData  fse_scankey[2];

	ScanKeyData key[1];

	Assert(dbid > 0);

	ScanKeyInit(&key[0],
				Anum_gp_segment_configuration_dbid,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum((int16)dbid));

	conf_rel = heap_open(GpSegmentConfigRelationId, AccessShareLock);
	fse_rel  = heap_open(FileSpaceEntryRelationId, AccessShareLock);
	conf_scan = heap_beginscan(conf_rel, SnapshotNow, 1, key);

	while (HeapTupleIsValid(conf_tup = heap_getnext(conf_scan, ForwardScanDirection)))
	{
		Datum	conf_datum;
		char	*gphome_env=NULL;
		char	*host;
		char	*dir;
		int4	port;
		int2	content_id;
		char	cmd[1024];

		get_config_attr(conf_datum, dbid, conf_tup, conf_rel);

		if ((int)DatumGetInt16(conf_datum) != dbid)
		{
			elog(ERROR, "startSegment_forDbid: failed dbid lookup in segment configuration");
		}

		/* got our dbid, get our array of mountpoints. */
		get_config_attr(conf_datum, address, conf_tup, conf_rel);
		host = DatumGetCString(DirectFunctionCall1(textout, conf_datum));

		get_config_attr(conf_datum, content, conf_tup, conf_rel);
		content_id = DatumGetInt16(conf_datum);

		get_config_attr(conf_datum, port, conf_tup, conf_rel);
		port = DatumGetInt32(conf_datum);

		/* Find the set of data-directories for all filespaces */
		/* Note: currently only fetches the pg_system filespace */
		ScanKeyInit(&fse_scankey[0], 
					Anum_pg_filespace_entry_fsedbid,
					BTEqualStrategyNumber, F_INT4EQ,
					Int16GetDatum((int2)dbid));  /* Current dbid */
		ScanKeyInit(&fse_scankey[1], 
					Anum_pg_filespace_entry_fsefsoid,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(SYSTEMFILESPACE_OID));  /* pg_system filespace */
		fse_scan = heap_beginscan(fse_rel, SnapshotNow, 2, fse_scankey);
		fse_tup = heap_getnext(fse_scan, ForwardScanDirection);

		if (!HeapTupleIsValid(fse_tup))
			elog(ERROR, "filespace lookup failure for dbid %d", dbid);

		get_heap_attr(conf_datum, Anum_pg_filespace_entry_, fselocation, fse_tup, fse_rel);
		dir = DatumGetCString(DirectFunctionCall1(textout, conf_datum));

		heap_endscan(fse_scan);

		/* Startup the segment */
		if (gp_log_fts >= GPVARS_VERBOSITY_VERBOSE)
			elog(LOG, "Starting %s:%s:%d", host, dir, port);

		/*
		 * NOTE: we expect every segment to have GPHOME set to the
		 * same directory as the master -- this is also assumed by the
		 * management scripts.
		 */
		/*
		  GPHOME=os.environ.get('GPHOME')
		  SRC_GPPATH=". %s/greenplum_path.sh;" % GPHOME
		*/
		gphome_env = getenv("GPHOME");

		if (gphome_env == NULL)
		{
			elog(LOG, "Failed to find GPHOME, sending our pg_ctl without path.");
			/* we don't have "gphome" ? let's try this anyhow and hope for the best. */
			snprintf(cmd, sizeof(cmd), "ssh %s \"pg_ctl -w -D %s -l %s/pg_log/startup.log -o '-i -p %d --silent-mode=true -M mirrorless -b %d -C %d -z %d' start 2>&1 \" ",
					 host, dir, dir, port, dbid, content_id, GpIdentity.numsegments);
		}
		else
		{
			snprintf(cmd, sizeof(cmd), "ssh %s \"source %s/greenplum_path.sh; %s/bin/pg_ctl -w -D %s -l %s/pg_log/startup.log -o '-i -p %d --silent-mode=true -M mirrorless -b %d -C %d -z %d' start 2>&1 \" ",
					 host, gphome_env, gphome_env, dir, dir, port, dbid, content_id, GpIdentity.numsegments);
		}

		if (gp_log_fts >= GPVARS_VERBOSITY_DEBUG)
			elog(LOG, "command is [%s]", cmd);
		system(cmd);

		ereport(LOG,
			(errmsg("SAN mirror segment on %s:%d is taking over as primary.",
				host, port ),
			 errSendAlert(true)));

		break;
	}

	heap_endscan(conf_scan);
	heap_close(conf_rel, AccessShareLock);
}

static char
mountMaintenanceForMpid(int mpid)
{
	char		ret='p';
	Relation	mount_rel;
	HeapTuple	mount_tup;
	HeapScanDesc mount_scan;

	ScanKeyData key[1];

	ScanKeyInit(&key[0],
				Anum_gp_san_configuration_mountid,
				BTEqualStrategyNumber, F_INT2EQ,
				Int16GetDatum((int16)mpid));

	mount_rel = heap_open(GpSanConfigRelationId, AccessShareLock);
	mount_scan = heap_beginscan(mount_rel, SnapshotNow, 1, key);

	while (HeapTupleIsValid(mount_tup = heap_getnext(mount_scan, ForwardScanDirection)))
	{
		Datum	mount_datum;

		char	active_host;
		char	san_type;

		char	*p_host, *p_mp, *p_dev;
		char	*m_host, *m_mp, *m_dev;

#define get_mount_attr(datum, ATTR, tuple, rel) get_heap_attr(datum, Anum_gp_san_configuration_, ATTR, tuple, rel)

		get_mount_attr(mount_datum, mountid, mount_tup, mount_rel);
		if ((int)DatumGetInt16(mount_datum) != mpid)
		{
			elog(ERROR, "getMounts_forDbid: failed dbid lookup in segment configuration");
		}

		/* got our row, now pull out our columns */
		get_mount_attr(mount_datum, active_host, mount_tup, mount_rel);
		active_host = DatumGetChar(mount_datum);

		get_mount_attr(mount_datum, san_type, mount_tup, mount_rel);
		san_type = DatumGetChar(mount_datum);

		/* get info for the primary */
		get_mount_attr(mount_datum, primary_host, mount_tup, mount_rel);
		p_host = DatumGetCString(DirectFunctionCall1(textout, mount_datum));

		get_mount_attr(mount_datum, primary_mountpoint, mount_tup, mount_rel);
		p_mp = DatumGetCString(DirectFunctionCall1(textout, mount_datum));

		get_mount_attr(mount_datum, primary_device, mount_tup, mount_rel);
		p_dev = DatumGetCString(DirectFunctionCall1(textout, mount_datum));

		/* get info for the mirror */
		get_mount_attr(mount_datum, mirror_host, mount_tup, mount_rel);
		m_host = DatumGetCString(DirectFunctionCall1(textout, mount_datum));

		get_mount_attr(mount_datum, mirror_mountpoint, mount_tup, mount_rel);
		m_mp = DatumGetCString(DirectFunctionCall1(textout, mount_datum));

		get_mount_attr(mount_datum, mirror_device, mount_tup, mount_rel);
		m_dev = DatumGetCString(DirectFunctionCall1(textout, mount_datum));

		elog(LOG, "active %c type %c primary %s:%s:'%s' mirror %s:%s:'%s'", active_host, san_type,
			 p_host, p_mp, p_dev, m_host, m_mp, m_dev);

		{
			char	*gphome_env=NULL;
			char	cmd[1024];
			char	args[8][256];
			int		cmd_status;

			/*
			 * NOTE: we expect every segment to have GPHOME set to the
			 * same directory as the master -- this is also assumed by the
			 * management scripts.
			 */
			/*
			  GPHOME=os.environ.get('GPHOME')
			  SRC_GPPATH=". %s/greenplum_path.sh;" % GPHOME
			*/
			gphome_env = getenv("GPHOME");

			if (gphome_env == NULL)
				snprintf(cmd, sizeof(cmd), "gp_mount_agent");
			else
				snprintf(cmd, sizeof(cmd), "%s/bin/gp_mount_agent", gphome_env);

			snprintf(args[0], sizeof(args[0]), "--agent -t %c", san_type);
			snprintf(args[1], sizeof(args[1]), "-a %c", active_host);
			snprintf(args[2], sizeof(args[2]), "-p %s", p_host);
			snprintf(args[3], sizeof(args[3]), "-d %s", p_dev);
			snprintf(args[4], sizeof(args[4]), "-m %s", p_mp);
			snprintf(args[5], sizeof(args[5]), "-q %s", m_host);
			snprintf(args[6], sizeof(args[6]), "-e %s", m_dev);
			snprintf(args[7], sizeof(args[7]), "-n %s", m_mp);

			elog(LOG, "Mount agent command is [%s %s %s %s %s %s %s %s %s]",
				 cmd, args[0], args[1], args[2], args[3], args[4], args[5],
				 args[6], args[7]);

			cmd_status = execlp(cmd, "gp_mount_agent", args[0], args[1],
								args[2], args[3], args[4], args[5],  args[6],
								args[7], NULL);

			if (cmd_status == -1)
			{
				elog(ERROR, "Could not issue command: %m");
				/* not reached */
			}
			else if (cmd_status != 0)
			{
				elog(ERROR, "gp_mount_agent failed!");
				/* not reached */
			}
		}

		ret = (active_host == 'p') ? 'm' : 'p';

		break;
	}

	heap_endscan(mount_scan);
	heap_close(mount_rel, AccessShareLock);

	return ret;
}


/* EOF */
