/*-------------------------------------------------------------------------
 *
 * ftsmessagehandler.c
 *	  Implementation of handling of FTS messages
 *
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/fts/ftsmessagehandler.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <sys/stat.h>
#include <unistd.h>
#include <replication/slot.h>

#include "access/xlog.h"
#include "cdb/cdbvars.h"
#include "libpq/pqformat.h"
#include "libpq/libpq.h"
#include "postmaster/fts.h"
#include "postmaster/postmaster.h"
#include "utils/faultinjector.h"
#include "utils/guc.h"
#include "replication/gp_replication.h"
#include "storage/fd.h"

#define FTS_PROBE_FILE_NAME "fts_probe_file.bak"
#define FTS_PROBE_MAGIC_STRING "FtS PrObEr MaGiC StRiNg, pRoBiNg cHeCk......."

/*
 * Check if we can smoothly read and write to data directory.
 *
 * O_DIRECT flag requires buffer to be OS/FS block aligned.
 * Best to have it IO Block alligned henece using BLCKSZ
 */
static bool
checkIODataDirectory(void)
{
	int fd;
	int size = BLCKSZ + BLCKSZ;
	int magic_len = strlen(FTS_PROBE_MAGIC_STRING) + 1;
	char *data = palloc0(size);

	/*
	 * Buffer needs to be alligned to BLOCK_SIZE for reads and writes if using O_DIRECT
	 */
	char* dataAligned = (char *) TYPEALIGN(BLCKSZ, data);

	errno = 0;
	bool failure = false;

	fd = BasicOpenFile(FTS_PROBE_FILE_NAME, O_RDWR | PG_O_DIRECT | O_EXCL,
                                           S_IRUSR | S_IWUSR);
	do
	{
		if (fd < 0)
		{
			if (errno == ENOENT)
			{
				elog(LOG, "FTS: \"%s\" file doesn't exist, creating it once.", FTS_PROBE_FILE_NAME);
				fd = BasicOpenFile(FTS_PROBE_FILE_NAME, O_RDWR | O_CREAT | O_EXCL,
                                           S_IRUSR | S_IWUSR);
				if (fd < 0)
				{
					failure = true;
					ereport(LOG, (errcode_for_file_access(),
							errmsg("FTS: could not create file \"%s\": %m",
								FTS_PROBE_FILE_NAME)));
				}
				else
				{
					strncpy(dataAligned, FTS_PROBE_MAGIC_STRING, magic_len);
					if (write(fd, dataAligned, BLCKSZ) != BLCKSZ)
					{
						ereport(LOG, (errcode_for_file_access(),
									  errmsg("FTS: could not write file \"%s\" : %m",
											 FTS_PROBE_FILE_NAME)));
						failure = true;
					}
				}
			}
			else
			{
				/*
				 * Some other error
				 */
				failure = true;
				ereport(LOG, (errcode_for_file_access(),
						errmsg("FTS: could not open file \"%s\": %m",
							FTS_PROBE_FILE_NAME)));
			}
			break;
		}

		int len = read(fd, dataAligned, BLCKSZ);
		if (len != BLCKSZ)
		{
			ereport(LOG, (errcode_for_file_access(),
					errmsg("FTS: could not read file \"%s\" "
						"(actual bytes read %d, required: %d): %m",
						FTS_PROBE_FILE_NAME, len, BLCKSZ)));
			failure = true;
			break;
		}

		if (strncmp(dataAligned, FTS_PROBE_MAGIC_STRING, magic_len) != 0)
		{
			ereport(LOG, (errmsg("FTS: Read corrupted data from \"%s\" file", FTS_PROBE_FILE_NAME)));
			failure = true;
			break;
		}

		if (lseek(fd, (off_t) 0, SEEK_SET) < 0)
		{
			ereport(LOG, (errcode_for_file_access(),
					errmsg("FTS: could not seek in file \"%s\" to offset zero: %m",
					FTS_PROBE_FILE_NAME)));
			failure = true;
			break;
		}

		/*
		 * Read worked, lets overwrite what we read, to check if can write also
		 */
		if (write(fd, dataAligned, BLCKSZ) != BLCKSZ)
		{
			ereport(LOG, (errcode_for_file_access(),
					errmsg("FTS: could not write file \"%s\" : %m",
					FTS_PROBE_FILE_NAME)));
			failure = true;
			break;
		}
	} while (0);

	if (fd > 0)
	{
		close(fd);

		/*
		 * We are more concerned with IOs hanging than failures.
		 * Cleanup the file as detected the problem and reporting the same.
		 * This is done to cover for cases like:
		 * 1] FTS detects corruption/read failure on the file, reports to Master
		 * 2] Triggers failover to mirror
		 * 3] But if the file stays around, when it transitions back to Primary
		 *    would again detect this corrupted file and again trigger failover.
		 * To avoid such scenarios remove the file.
		 */
		if (failure)
		{
			if (unlink(FTS_PROBE_FILE_NAME) < 0)
				ereport(LOG,
				(errcode_for_file_access(),
				errmsg("could not unlink file \"%s\": %m", FTS_PROBE_FILE_NAME)));
		}
	}

	if (failure)
		ereport(ERROR,
				(errmsg("disk IO check during FTS probe failed")));

	pfree(data);
	return failure;
}

static void
SendFtsResponse(FtsResponse *response, const char *messagetype)
{
	StringInfoData buf;

	BeginCommand(messagetype, DestRemote);

	pq_beginmessage(&buf, 'T');
	pq_sendint(&buf, Natts_fts_message_response, 2); /* # of columns */

	pq_sendstring(&buf, "is_mirror_up");
	pq_sendint(&buf, 0, 4);		/* table oid */
	pq_sendint(&buf, Anum_fts_message_response_is_mirror_up, 2);		/* attnum */
	pq_sendint(&buf, BOOLOID, 4);		/* type oid */
	pq_sendint(&buf, 1, 2);	/* typlen */
	pq_sendint(&buf, -1, 4);		/* typmod */
	pq_sendint(&buf, 0, 2);		/* format code */

	pq_sendstring(&buf, "is_in_sync");
	pq_sendint(&buf, 0, 4);		/* table oid */
	pq_sendint(&buf, Anum_fts_message_response_is_in_sync, 2);		/* attnum */
	pq_sendint(&buf, BOOLOID, 4);		/* type oid */
	pq_sendint(&buf, 1, 2);	/* typlen */
	pq_sendint(&buf, -1, 4);		/* typmod */
	pq_sendint(&buf, 0, 2);		/* format code */

	pq_sendstring(&buf, "is_syncrep_enabled");
	pq_sendint(&buf, 0, 4);		/* table oid */
	pq_sendint(&buf, Anum_fts_message_response_is_syncrep_enabled, 2);		/* attnum */
	pq_sendint(&buf, BOOLOID, 4);		/* type oid */
	pq_sendint(&buf, 1, 2);	/* typlen */
	pq_sendint(&buf, -1, 4);		/* typmod */
	pq_sendint(&buf, 0, 2);		/* format code */

	pq_sendstring(&buf, "is_role_mirror");
	pq_sendint(&buf, 0, 4);		/* table oid */
	pq_sendint(&buf, Anum_fts_message_response_is_role_mirror, 2);		/* attnum */
	pq_sendint(&buf, BOOLOID, 4);		/* type oid */
	pq_sendint(&buf, 1, 2);	/* typlen */
	pq_sendint(&buf, -1, 4);		/* typmod */
	pq_sendint(&buf, 0, 2);		/* format code */

	pq_sendstring(&buf, "request_retry");
	pq_sendint(&buf, 0, 4);		/* table oid */
	pq_sendint(&buf, Anum_fts_message_response_request_retry, 2);		/* attnum */
	pq_sendint(&buf, BOOLOID, 4);		/* type oid */
	pq_sendint(&buf, 1, 2);	/* typlen */
	pq_sendint(&buf, -1, 4);		/* typmod */
	pq_sendint(&buf, 0, 2);		/* format code */

	pq_endmessage(&buf);

	/* Send a DataRow message */
	pq_beginmessage(&buf, 'D');
	pq_sendint(&buf, Natts_fts_message_response, 2);		/* # of columns */

	pq_sendint(&buf, 1, 4); /* col1 len */
	pq_sendint(&buf, response->IsMirrorUp, 1);

	pq_sendint(&buf, 1, 4); /* col2 len */
	pq_sendint(&buf, response->IsInSync, 1);

	pq_sendint(&buf, 1, 4); /* col3 len */
	pq_sendint(&buf, response->IsSyncRepEnabled, 1);

	pq_sendint(&buf, 1, 4); /* col4 len */
	pq_sendint(&buf, response->IsRoleMirror, 1);

	pq_sendint(&buf, 1, 4); /* col5 len */
	pq_sendint(&buf, response->RequestRetry, 1);

	pq_endmessage(&buf);
	EndCommand(messagetype, DestRemote);
	pq_flush();
}

static void
HandleFtsWalRepProbe(void)
{
	FtsResponse response = {
		false, /* IsMirrorUp */
		false, /* IsInSync */
		false, /* IsSyncRepEnabled */
		false, /* IsRoleMirror */
		false, /* RequestRetry */
	};

	if (am_mirror)
	{
		response.IsRoleMirror = true;
		elog(LOG, "received probe message while acting as mirror");
	}
	else
	{
		GetMirrorStatus(&response);

		/*
		 * We check response.IsSyncRepEnabled even though syncrep is again checked
		 * later in the set function to avoid acquiring the SyncRepLock again.
		 */
		if (response.IsMirrorUp && !response.IsSyncRepEnabled)
		{
			SetSyncStandbysDefined();
			/* Syncrep is enabled now, so respond accordingly. */
			response.IsSyncRepEnabled = true;
		}
	}

	/*
	 * Perform basic sanity check for disk IO on segment. Without this check
	 * in many situations FTS didn't detect the problem and hence didn't
	 * trigger failover to mirror. It caused extended data unavailable
	 * situations. Hence performing some read-write as part of FTS probe
	 * helped detect and trigger failover.
	 */
	checkIODataDirectory();
	SendFtsResponse(&response, FTS_MSG_PROBE);
}

static void
HandleFtsWalRepSyncRepOff(void)
{
	FtsResponse response = {
		false, /* IsMirrorUp */
		false, /* IsInSync */
		false, /* IsSyncRepEnabled */
		false, /* IsRoleMirror */
		false, /* RequestRetry */
	};

	ereport(LOG,
			(errmsg("turning off synchronous wal replication due to FTS request")));
	UnsetSyncStandbysDefined();
	GetMirrorStatus(&response);

	SendFtsResponse(&response, FTS_MSG_SYNCREP_OFF);
}

static void
CreateReplicationSlotOnPromote(const char *name)
{
	int             i;

	Assert(MyReplicationSlot == NULL);

	/*
	 * Check for name collision, and identify an allocatable slot.  We need to
	 * hold ReplicationSlotControlLock in shared mode for this, so that nobody
	 * else can change the in_use flags while we're looking at them.
	 */
	LWLockAcquire(ReplicationSlotControlLock, LW_SHARED);
	for (i = 0; i < max_replication_slots; i++)
	{
		ReplicationSlot *s = &ReplicationSlotCtl->replication_slots[i];

		if (s->in_use && strcmp(name, NameStr(s->data.name)) == 0)
			MyReplicationSlot = s;
	}
	LWLockRelease(ReplicationSlotControlLock);

	if (MyReplicationSlot == NULL)
	{
		ereport(LOG, (errmsg("creating replication slot %s", name)));
		ReplicationSlotCreate(name, false, RS_PERSISTENT);
	}
	else
		ereport(LOG, (errmsg("replication slot %s exists", name)));

	/*
	 * Only on promote signal replication slot is created on mirror. If
	 * node was acting as mirror, no replication slot should exists on it.
	 * Hence, no-zero restart_lsn means was set by previous attempt on promote
	 * signal and hence no need to overwrite the same.
	 */
	if (MyReplicationSlot->data.restart_lsn == 0)
	{
		/* Starting reserving WAL right away for pg_rewind to work later */
		ReplicationSlotReserveWal();
		/* Write this slot to disk */
		ReplicationSlotMarkDirty();
		ReplicationSlotSave();
		if (MyReplicationSlot->active)
			ReplicationSlotRelease();
	}

	MyReplicationSlot = NULL;
}

static void
HandleFtsWalRepPromote(void)
{
	FtsResponse response = {
		false, /* IsMirrorUp */
		false, /* IsInSync */
		false, /* IsSyncRepEnabled */
		am_mirror,  /* IsRoleMirror */
		false, /* RequestRetry */
	};

	ereport(LOG,
			(errmsg("promoting mirror to primary due to FTS request")));

	/*
	 * FTS sends promote message to a mirror.  The mirror may be undergoing
	 * promotion.  Promote messages should therefore be handled in an
	 * idempotent way.
	 */
	DBState state = GetCurrentDBState();
	if (state == DB_IN_ARCHIVE_RECOVERY)
	{
		/*
		 * Reset sync_standby_names on promotion. This is to avoid commits
		 * hanging/waiting for replication till next FTS probe. Next FTS probe
		 * will detect this node to be not in sync and reset the same which
		 * can take a min. Since we know on mirror promotion its marked as not
		 * in sync in gp_segment_configuration, best to right away clean the
		 * sync_standby_names.
		 */
		UnsetSyncStandbysDefined();

		CreateReplicationSlotOnPromote(INTERNAL_WAL_REPLICATION_SLOT_NAME);

		SignalPromote();
	}
	else
	{
		elog(LOG, "ignoring promote request, walreceiver not running,"
			 " DBState = %d", state);
	}

	SendFtsResponse(&response, FTS_MSG_PROMOTE);
}

void
HandleFtsMessage(const char* query_string)
{
	int dbid;
	int contid;
	char message_type[FTS_MSG_MAX_LEN];
	int error_level;

	if (sscanf(query_string, FTS_MSG_FORMAT,
			   message_type, &dbid, &contid) != 3)
	{
		ereport(ERROR,
				(errmsg("received invalid FTS query: %s", query_string)));
	}

#ifdef USE_ASSERT_CHECKING
	error_level = PANIC;
#else
	error_level = WARNING;
#endif

	if (dbid != GpIdentity.dbid)
		ereport(error_level,
				(errmsg("message type: %s received dbid:%d doesn't match this segments configured dbid:%d",
						message_type, dbid, GpIdentity.dbid)));

	if (contid != GpIdentity.segindex)
		ereport(error_level,
				(errmsg("message type: %s received contentid:%d doesn't match this segments configured contentid:%d",
						message_type, contid, GpIdentity.segindex)));

	SIMPLE_FAULT_INJECTOR("fts_handle_message");

	if (strncmp(query_string, FTS_MSG_PROBE,
				strlen(FTS_MSG_PROBE)) == 0)
		HandleFtsWalRepProbe();
	else if (strncmp(query_string, FTS_MSG_SYNCREP_OFF,
					 strlen(FTS_MSG_SYNCREP_OFF)) == 0)
		HandleFtsWalRepSyncRepOff();
	else if (strncmp(query_string, FTS_MSG_PROMOTE,
					 strlen(FTS_MSG_PROMOTE)) == 0)
		HandleFtsWalRepPromote();
	else
		ereport(ERROR,
				(errmsg("received unknown FTS query: %s", query_string)));
}
