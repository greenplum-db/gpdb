/*-------------------------------------------------------------------------
 *
 * origin.c
 *	  Logical replication progress tracking support.
 *
 * Copyright (c) 2013-2015, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/replication/logical/origin.c
 *
 * NOTES
 *
 * This file provides the following:
 * * An infrastructure to name nodes in a replication setup
 * * A facility to efficiently store and persist replication progress in an
 *	 efficient and durable manner.
 *
 * Replication origin consist out of a descriptive, user defined, external
 * name and a short, thus space efficient, internal 2 byte one. This split
 * exists because replication origin have to be stored in WAL and shared
 * memory and long descriptors would be inefficient.  For now only use 2 bytes
 * for the internal id of a replication origin as it seems unlikely that there
 * soon will be more than 65k nodes in one replication setup; and using only
 * two bytes allow us to be more space efficient.
 *
 * Replication progress is tracked in a shared memory table
 * (ReplicationStates) that's dumped to disk every checkpoint. Entries
 * ('slots') in this table are identified by the internal id. That's the case
 * because it allows to increase replication progress during crash
 * recovery. To allow doing so we store the original LSN (from the originating
 * system) of a transaction in the commit record. That allows to recover the
 * precise replayed state after crash recovery; without requiring synchronous
 * commits. Allowing logical replication to use asynchronous commit is
 * generally good for performance, but especially important as it allows a
 * single threaded replay process to keep up with a source that has multiple
 * backends generating changes concurrently.  For efficiency and simplicity
 * reasons a backend can setup one replication origin that's from then used as
 * the source of changes produced by the backend, until reset again.
 *
 * This infrastructure is intended to be used in cooperation with logical
 * decoding. When replaying from a remote system the configured origin is
 * provided to output plugins, allowing prevention of replication loops and
 * other filtering.
 *
 * There are several levels of locking at work:
 *
 * * To create and drop replication origins an exclusive lock on
 *	 pg_replication_slot is required for the duration. That allows us to
 *	 safely and conflict free assign new origins using a dirty snapshot.
 *
 * * When creating an in-memory replication progress slot the ReplicationOirgin
 *	 LWLock has to be held exclusively; when iterating over the replication
 *	 progress a shared lock has to be held, the same when advancing the
 *	 replication progress of an individual backend that has not setup as the
 *	 session's replication origin.
 *
 * * When manipulating or looking at the remote_lsn and local_lsn fields of a
 *	 replication progress slot that slot's lwlock has to be held. That's
 *	 primarily because we do not assume 8 byte writes (the LSN) is atomic on
 *	 all our platforms, but it also simplifies memory ordering concerns
 *	 between the remote and local lsn. We use a lwlock instead of a spinlock
 *	 so it's less harmful to hold the lock over a WAL write
 *	 (c.f. AdvanceReplicationProgress).
 *
 * ---------------------------------------------------------------------------
 */

#include "postgres.h"

#include <unistd.h>
#include <sys/stat.h>

#include "funcapi.h"
#include "miscadmin.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/xact.h"

#include "catalog/indexing.h"

#include "nodes/execnodes.h"

#include "replication/origin.h"
#include "replication/logical.h"

#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/lmgr.h"
#include "storage/copydir.h"

#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/pg_lsn.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "utils/tqual.h"

/*
 * Replay progress of a single remote node.
 */
typedef struct ReplicationState
{
	/*
	 * Local identifier for the remote node.
	 */
	RepOriginId roident;

	/*
	 * Location of the latest commit from the remote side.
	 */
	XLogRecPtr	remote_lsn;

	/*
	 * Remember the local lsn of the commit record so we can XLogFlush() to it
	 * during a checkpoint so we know the commit record actually is safe on
	 * disk.
	 */
	XLogRecPtr	local_lsn;

	/*
	 * Slot is setup in backend?
	 */
	pid_t		acquired_by;

	/*
	 * Lock protecting remote_lsn and local_lsn.
	 */
	LWLock		lock;
} ReplicationState;

/*
 * On disk version of ReplicationState.
 */
typedef struct ReplicationStateOnDisk
{
	RepOriginId roident;
	XLogRecPtr	remote_lsn;
} ReplicationStateOnDisk;


typedef struct ReplicationStateCtl
{
	int			tranche_id;
	LWLockTranche tranche;
	ReplicationState states[FLEXIBLE_ARRAY_MEMBER];
} ReplicationStateCtl;

/* external variables */
RepOriginId replorigin_sesssion_origin = InvalidRepOriginId;	/* assumed identity */
XLogRecPtr	replorigin_sesssion_origin_lsn = InvalidXLogRecPtr;
TimestampTz replorigin_sesssion_origin_timestamp = 0;

/*
 * Base address into a shared memory array of replication states of size
 * max_replication_slots.
 *
 * XXX: Should we use a separate variable to size this rather than
 * max_replication_slots?
 */
static ReplicationState *replication_states;
static ReplicationStateCtl *replication_states_ctl;

/*
 * Backend-local, cached element from ReplicationStates for use in a backend
 * replaying remote commits, so we don't have to search ReplicationStates for
 * the backends current RepOriginId.
 */
static ReplicationState *session_replication_state = NULL;

/* Magic for on disk files. */
#define REPLICATION_STATE_MAGIC ((uint32) 0x1257DADE)

static void
replorigin_check_prerequisites(bool check_slots, bool recoveryOK)
{
	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("only superusers can query or manipulate replication origins")));

	if (check_slots && max_replication_slots == 0)
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("cannot query or manipulate replication origin when max_replication_slots = 0")));

	if (!recoveryOK && RecoveryInProgress())
		ereport(ERROR,
				(errcode(ERRCODE_READ_ONLY_SQL_TRANSACTION),
		   errmsg("cannot manipulate replication origins during recovery")));

}


/* ---------------------------------------------------------------------------
 * Functions for working with replication origins themselves.
 * ---------------------------------------------------------------------------
 */

/*
 * Check for a persistent replication origin identified by name.
 *
 * Returns InvalidOid if the node isn't known yet and missing_ok is true.
 */
RepOriginId
replorigin_by_name(char *roname, bool missing_ok)
{
	Form_pg_replication_origin ident;
	Oid			roident = InvalidOid;
	HeapTuple	tuple;
	Datum		roname_d;

	roname_d = CStringGetTextDatum(roname);

	tuple = SearchSysCache1(REPLORIGNAME, roname_d);
	if (HeapTupleIsValid(tuple))
	{
		ident = (Form_pg_replication_origin) GETSTRUCT(tuple);
		roident = ident->roident;
		ReleaseSysCache(tuple);
	}
	else if (!missing_ok)
		elog(ERROR, "cache lookup failed for replication origin '%s'",
			 roname);

	return roident;
}

/*
 * Create a replication origin.
 *
 * Needs to be called in a transaction.
 */
RepOriginId
replorigin_create(char *roname)
{
	Oid			roident;
	HeapTuple	tuple = NULL;
	Relation	rel;
	Datum		roname_d;
	SnapshotData SnapshotDirty;
	SysScanDesc scan;
	ScanKeyData key;

	roname_d = CStringGetTextDatum(roname);

	Assert(IsTransactionState());

	/*
	 * We need the numeric replication origin to be 16bit wide, so we cannot
	 * rely on the normal oid allocation. Instead we simply scan
	 * pg_replication_origin for the first unused id. That's not particularly
	 * efficient, but this should be a fairly infrequent operation - we can
	 * easily spend a bit more code on this when it turns out it needs to be
	 * faster.
	 *
	 * We handle concurrency by taking an exclusive lock (allowing reads!)
	 * over the table for the duration of the search. Because we use a "dirty
	 * snapshot" we can read rows that other in-progress sessions have
	 * written, even though they would be invisible with normal snapshots. Due
	 * to the exclusive lock there's no danger that new rows can appear while
	 * we're checking.
	 */
	InitDirtySnapshot(SnapshotDirty);

	rel = heap_open(ReplicationOriginRelationId, ExclusiveLock);

	for (roident = InvalidOid + 1; roident < PG_UINT16_MAX; roident++)
	{
		bool		nulls[Natts_pg_replication_origin];
		Datum		values[Natts_pg_replication_origin];
		bool		collides;

		CHECK_FOR_INTERRUPTS();

		ScanKeyInit(&key,
					Anum_pg_replication_origin_roident,
					BTEqualStrategyNumber, F_OIDEQ,
					ObjectIdGetDatum(roident));

		scan = systable_beginscan(rel, ReplicationOriginIdentIndex,
								  true /* indexOK */ ,
								  &SnapshotDirty,
								  1, &key);

		collides = HeapTupleIsValid(systable_getnext(scan));

		systable_endscan(scan);

		if (!collides)
		{
			/*
			 * Ok, found an unused roident, insert the new row and do a CCI,
			 * so our callers can look it up if they want to.
			 */
			memset(&nulls, 0, sizeof(nulls));

			values[Anum_pg_replication_origin_roident - 1] = ObjectIdGetDatum(roident);
			values[Anum_pg_replication_origin_roname - 1] = roname_d;

			tuple = heap_form_tuple(RelationGetDescr(rel), values, nulls);
			simple_heap_insert(rel, tuple);
			CatalogUpdateIndexes(rel, tuple);
			CommandCounterIncrement();
			break;
		}
	}

	/* now release lock again,	*/
	heap_close(rel, ExclusiveLock);

	if (tuple == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				 errmsg("no free replication oid could be found")));

	heap_freetuple(tuple);
	return roident;
}


/*
 * Drop replication origin.
 *
 * Needs to be called in a transaction.
 */
void
replorigin_drop(RepOriginId roident)
{
	HeapTuple	tuple = NULL;
	Relation	rel;
	int			i;

	Assert(IsTransactionState());

	rel = heap_open(ReplicationOriginRelationId, ExclusiveLock);

	/* cleanup the slot state info */
	LWLockAcquire(ReplicationOriginLock, LW_EXCLUSIVE);

	for (i = 0; i < max_replication_slots; i++)
	{
		ReplicationState *state = &replication_states[i];

		/* found our slot */
		if (state->roident == roident)
		{
			if (state->acquired_by != 0)
			{
				ereport(ERROR,
						(errcode(ERRCODE_OBJECT_IN_USE),
						 errmsg("cannot drop replication origin with oid %d, in use by pid %d",
								state->roident,
								state->acquired_by)));
			}

			/* first WAL log */
			{
				xl_replorigin_drop xlrec;

				xlrec.node_id = roident;
				XLogBeginInsert();
				XLogRegisterData((char *) (&xlrec), sizeof(xlrec));
				XLogInsert(RM_REPLORIGIN_ID, XLOG_REPLORIGIN_DROP);
			}

			/* then reset the in-memory entry */
			state->roident = InvalidRepOriginId;
			state->remote_lsn = InvalidXLogRecPtr;
			state->local_lsn = InvalidXLogRecPtr;
			break;
		}
	}
	LWLockRelease(ReplicationOriginLock);

	tuple = SearchSysCache1(REPLORIGIDENT, ObjectIdGetDatum(roident));
	simple_heap_delete(rel, &tuple->t_self);
	ReleaseSysCache(tuple);

	CommandCounterIncrement();

	/* now release lock again,	*/
	heap_close(rel, ExclusiveLock);
}


/*
 * Lookup replication origin via it's oid and return the name.
 *
 * The external name is palloc'd in the calling context.
 *
 * Returns true if the origin is known, false otherwise.
 */
bool
replorigin_by_oid(RepOriginId roident, bool missing_ok, char **roname)
{
	HeapTuple	tuple;
	Form_pg_replication_origin ric;

	Assert(OidIsValid((Oid) roident));
	Assert(roident != InvalidRepOriginId);
	Assert(roident != DoNotReplicateId);

	tuple = SearchSysCache1(REPLORIGIDENT,
							ObjectIdGetDatum((Oid) roident));

	if (HeapTupleIsValid(tuple))
	{
		ric = (Form_pg_replication_origin) GETSTRUCT(tuple);
		*roname = text_to_cstring(&ric->roname);
		ReleaseSysCache(tuple);

		return true;
	}
	else
	{
		*roname = NULL;

		if (!missing_ok)
			elog(ERROR, "cache lookup failed for replication origin with oid %u",
				 roident);

		return false;
	}
}


/* ---------------------------------------------------------------------------
 * Functions for handling replication progress.
 * ---------------------------------------------------------------------------
 */

Size
ReplicationOriginShmemSize(void)
{
	Size		size = 0;

	/*
	 * XXX: max_replication_slots is arguablethe wrong thing to use here, here
	 * we keep the replay state of *remote* transactions. But for now it seems
	 * sufficient to reuse it, lest we introduce a separate guc.
	 */
	if (max_replication_slots == 0)
		return size;

	size = add_size(size, offsetof(ReplicationStateCtl, states));

	size = add_size(size,
				  mul_size(max_replication_slots, sizeof(ReplicationState)));
	return size;
}

void
ReplicationOriginShmemInit(void)
{
	bool		found;

	if (max_replication_slots == 0)
		return;

	replication_states_ctl = (ReplicationStateCtl *)
		ShmemInitStruct("ReplicationOriginState",
						ReplicationOriginShmemSize(),
						&found);
	replication_states = replication_states_ctl->states;

	if (!found)
	{
		int			i;

		replication_states_ctl->tranche_id = LWLockNewTrancheId();
		replication_states_ctl->tranche.name = "ReplicationOrigins";
		replication_states_ctl->tranche.array_base =
			&replication_states[0].lock;
		replication_states_ctl->tranche.array_stride =
			sizeof(ReplicationState);

		MemSet(replication_states, 0, ReplicationOriginShmemSize());

		for (i = 0; i < max_replication_slots; i++)
			LWLockInitialize(&replication_states[i].lock,
							 replication_states_ctl->tranche_id);
	}

	LWLockRegisterTranche(replication_states_ctl->tranche_id,
						  &replication_states_ctl->tranche);
}

/* ---------------------------------------------------------------------------
 * Perform a checkpoint of each replication origin's progress with respect to
 * the replayed remote_lsn. Make sure that all transactions we refer to in the
 * checkpoint (local_lsn) are actually on-disk. This might not yet be the case
 * if the transactions were originally committed asynchronously.
 *
 * We store checkpoints in the following format:
 * +-------+------------------------+------------------+-----+--------+
 * | MAGIC | ReplicationStateOnDisk | struct Replic... | ... | CRC32C | EOF
 * +-------+------------------------+------------------+-----+--------+
 *
 * So its just the magic, followed by the statically sized
 * ReplicationStateOnDisk structs. Note that the maximum number of
 * ReplicationStates is determined by max_replication_slots.
 * ---------------------------------------------------------------------------
 */
void
CheckPointReplicationOrigin(void)
{
	const char *tmppath = "pg_logical/replorigin_checkpoint.tmp";
	const char *path = "pg_logical/replorigin_checkpoint";
	int			tmpfd;
	int			i;
	uint32		magic = REPLICATION_STATE_MAGIC;
	pg_crc32c	crc;

	if (max_replication_slots == 0)
		return;

	INIT_CRC32C(crc);

	/* make sure no old temp file is remaining */
	if (unlink(tmppath) < 0 && errno != ENOENT)
		ereport(PANIC,
				(errcode_for_file_access(),
				 errmsg("could not remove file \"%s\": %m",
						path)));

	/*
	 * no other backend can perform this at the same time, we're protected by
	 * CheckpointLock.
	 */
	tmpfd = OpenTransientFile((char *) tmppath,
							  O_CREAT | O_EXCL | O_WRONLY | PG_BINARY,
							  S_IRUSR | S_IWUSR);
	if (tmpfd < 0)
		ereport(PANIC,
				(errcode_for_file_access(),
				 errmsg("could not create file \"%s\": %m",
						tmppath)));

	/* write magic */
	if ((write(tmpfd, &magic, sizeof(magic))) != sizeof(magic))
	{
		CloseTransientFile(tmpfd);
		ereport(PANIC,
				(errcode_for_file_access(),
				 errmsg("could not write to file \"%s\": %m",
						tmppath)));
	}
	COMP_CRC32C(crc, &magic, sizeof(magic));

	/* prevent concurrent creations/drops */
	LWLockAcquire(ReplicationOriginLock, LW_SHARED);

	/* write actual data */
	for (i = 0; i < max_replication_slots; i++)
	{
		ReplicationStateOnDisk disk_state;
		ReplicationState *curstate = &replication_states[i];
		XLogRecPtr	local_lsn;

		if (curstate->roident == InvalidRepOriginId)
			continue;

		LWLockAcquire(&curstate->lock, LW_SHARED);

		disk_state.roident = curstate->roident;

		disk_state.remote_lsn = curstate->remote_lsn;
		local_lsn = curstate->local_lsn;

		LWLockRelease(&curstate->lock);

		/* make sure we only write out a commit that's persistent */
		XLogFlush(local_lsn);

		if ((write(tmpfd, &disk_state, sizeof(disk_state))) !=
			sizeof(disk_state))
		{
			CloseTransientFile(tmpfd);
			ereport(PANIC,
					(errcode_for_file_access(),
					 errmsg("could not write to file \"%s\": %m",
							tmppath)));
		}

		COMP_CRC32C(crc, &disk_state, sizeof(disk_state));
	}

	LWLockRelease(ReplicationOriginLock);

	/* write out the CRC */
	FIN_CRC32C(crc);
	if ((write(tmpfd, &crc, sizeof(crc))) != sizeof(crc))
	{
		CloseTransientFile(tmpfd);
		ereport(PANIC,
				(errcode_for_file_access(),
				 errmsg("could not write to file \"%s\": %m",
						tmppath)));
	}

	/* fsync the temporary file */
	if (pg_fsync(tmpfd) != 0)
	{
		CloseTransientFile(tmpfd);
		ereport(PANIC,
				(errcode_for_file_access(),
				 errmsg("could not fsync file \"%s\": %m",
						tmppath)));
	}

	CloseTransientFile(tmpfd);

	/* rename to permanent file, fsync file and directory */
	if (rename(tmppath, path) != 0)
	{
		ereport(PANIC,
				(errcode_for_file_access(),
				 errmsg("could not rename file \"%s\" to \"%s\": %m",
						tmppath, path)));
	}

	fsync_fname((char *) path, false);
	fsync_fname("pg_logical", true);
}

/*
 * Recover replication replay status from checkpoint data saved earlier by
 * CheckPointReplicationOrigin.
 *
 * This only needs to be called at startup and *not* during every checkpoint
 * read during recovery (e.g. in HS or PITR from a base backup) afterwards. All
 * state thereafter can be recovered by looking at commit records.
 */
void
StartupReplicationOrigin(void)
{
	const char *path = "pg_logical/replorigin_checkpoint";
	int			fd;
	int			readBytes;
	uint32		magic = REPLICATION_STATE_MAGIC;
	int			last_state = 0;
	pg_crc32c	file_crc;
	pg_crc32c	crc;

	/* don't want to overwrite already existing state */
#ifdef USE_ASSERT_CHECKING
	static bool already_started = false;

	Assert(!already_started);
	already_started = true;
#endif

	if (max_replication_slots == 0)
		return;

	INIT_CRC32C(crc);

	elog(DEBUG2, "starting up replication origin progress state");

	fd = OpenTransientFile((char *) path, O_RDONLY | PG_BINARY, 0);

	/*
	 * might have had max_replication_slots == 0 last run, or we just brought
	 * up a standby.
	 */
	if (fd < 0 && errno == ENOENT)
		return;
	else if (fd < 0)
		ereport(PANIC,
				(errcode_for_file_access(),
				 errmsg("could not open file \"%s\": %m",
						path)));

	/* verify magic, thats written even if nothing was active */
	readBytes = read(fd, &magic, sizeof(magic));
	if (readBytes != sizeof(magic))
		ereport(PANIC,
				(errmsg("could not read file \"%s\": %m",
						path)));
	COMP_CRC32C(crc, &magic, sizeof(magic));

	if (magic != REPLICATION_STATE_MAGIC)
		ereport(PANIC,
		   (errmsg("replication checkpoint has wrong magic %u instead of %u",
				   magic, REPLICATION_STATE_MAGIC)));

	/* we can skip locking here, no other access is possible */

	/* recover individual states, until there are no more to be found */
	while (true)
	{
		ReplicationStateOnDisk disk_state;

		readBytes = read(fd, &disk_state, sizeof(disk_state));

		/* no further data */
		if (readBytes == sizeof(crc))
		{
			/* not pretty, but simple ... */
			file_crc = *(pg_crc32c *) &disk_state;
			break;
		}

		if (readBytes < 0)
		{
			ereport(PANIC,
					(errcode_for_file_access(),
					 errmsg("could not read file \"%s\": %m",
							path)));
		}

		if (readBytes != sizeof(disk_state))
		{
			ereport(PANIC,
					(errcode_for_file_access(),
					 errmsg("could not read file \"%s\": read %d of %zu",
							path, readBytes, sizeof(disk_state))));
		}

		COMP_CRC32C(crc, &disk_state, sizeof(disk_state));

		if (last_state == max_replication_slots)
			ereport(PANIC,
					(errcode(ERRCODE_CONFIGURATION_LIMIT_EXCEEDED),
					 errmsg("no free replication state could be found, increase max_replication_slots")));

		/* copy data to shared memory */
		replication_states[last_state].roident = disk_state.roident;
		replication_states[last_state].remote_lsn = disk_state.remote_lsn;
		last_state++;

		elog(LOG, "recovered replication state of node %u to %X/%X",
			 disk_state.roident,
			 (uint32) (disk_state.remote_lsn >> 32),
			 (uint32) disk_state.remote_lsn);
	}

	/* now check checksum */
	FIN_CRC32C(crc);
	if (file_crc != crc)
		ereport(PANIC,
				(errcode(ERRCODE_CONFIGURATION_LIMIT_EXCEEDED),
				 errmsg("replication_slot_checkpoint has wrong checksum %u, expected %u",
						crc, file_crc)));

	CloseTransientFile(fd);
}

void
replorigin_redo(XLogReaderState *record)
{
	uint8		info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;

	switch (info)
	{
		case XLOG_REPLORIGIN_SET:
			{
				xl_replorigin_set *xlrec =
				(xl_replorigin_set *) XLogRecGetData(record);

				replorigin_advance(xlrec->node_id,
								   xlrec->remote_lsn, record->EndRecPtr,
								   xlrec->force /* backward */ ,
								   false /* WAL log */ );
				break;
			}
		case XLOG_REPLORIGIN_DROP:
			{
				xl_replorigin_drop *xlrec;
				int			i;

				xlrec = (xl_replorigin_drop *) XLogRecGetData(record);

				for (i = 0; i < max_replication_slots; i++)
				{
					ReplicationState *state = &replication_states[i];

					/* found our slot */
					if (state->roident == xlrec->node_id)
					{
						/* reset entry */
						state->roident = InvalidRepOriginId;
						state->remote_lsn = InvalidXLogRecPtr;
						state->local_lsn = InvalidXLogRecPtr;
						break;
					}
				}
				break;
			}
		default:
			elog(PANIC, "replorigin_redo: unknown op code %u", info);
	}
}


/*
 * Tell the replication origin progress machinery that a commit from 'node'
 * that originated at the LSN remote_commit on the remote node was replayed
 * successfully and that we don't need to do so again. In combination with
 * setting up replorigin_sesssion_origin_lsn and replorigin_sesssion_origin that ensures we
 * won't loose knowledge about that after a crash if the transaction had a
 * persistent effect (think of asynchronous commits).
 *
 * local_commit needs to be a local LSN of the commit so that we can make sure
 * uppon a checkpoint that enough WAL has been persisted to disk.
 *
 * Needs to be called with a RowExclusiveLock on pg_replication_origin,
 * unless running in recovery.
 */
void
replorigin_advance(RepOriginId node,
				   XLogRecPtr remote_commit, XLogRecPtr local_commit,
				   bool go_backward, bool wal_log)
{
	int			i;
	ReplicationState *replication_state = NULL;
	ReplicationState *free_state = NULL;

	Assert(node != InvalidRepOriginId);

	/* we don't track DoNotReplicateId */
	if (node == DoNotReplicateId)
		return;

	/*
	 * XXX: For the case where this is called by WAL replay, it'd be more
	 * efficient to restore into a backend local hashtable and only dump into
	 * shmem after recovery is finished. Let's wait with implementing that
	 * till it's shown to be a measurable expense
	 */

	/* Lock exclusively, as we may have to create a new table entry. */
	LWLockAcquire(ReplicationOriginLock, LW_EXCLUSIVE);

	/*
	 * Search for either an existing slot for the origin, or a free one we can
	 * use.
	 */
	for (i = 0; i < max_replication_slots; i++)
	{
		ReplicationState *curstate = &replication_states[i];

		/* remember where to insert if necessary */
		if (curstate->roident == InvalidRepOriginId &&
			free_state == NULL)
		{
			free_state = curstate;
			continue;
		}

		/* not our slot */
		if (curstate->roident != node)
		{
			continue;
		}

		/* ok, found slot */
		replication_state = curstate;

		LWLockAcquire(&replication_state->lock, LW_EXCLUSIVE);

		/* Make sure it's not used by somebody else */
		if (replication_state->acquired_by != 0)
		{
			ereport(ERROR,
					(errcode(ERRCODE_OBJECT_IN_USE),
					 errmsg("replication origin with oid %d is already active for pid %d",
							replication_state->roident,
							replication_state->acquired_by)));
		}

		break;
	}

	if (replication_state == NULL && free_state == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_CONFIGURATION_LIMIT_EXCEEDED),
				 errmsg("no free replication state slot could be found for replication origin with oid %u",
						node),
				 errhint("Increase max_replication_slots and try again.")));

	if (replication_state == NULL)
	{
		/* initialize new slot */
		LWLockAcquire(&free_state->lock, LW_EXCLUSIVE);
		replication_state = free_state;
		Assert(replication_state->remote_lsn == InvalidXLogRecPtr);
		Assert(replication_state->local_lsn == InvalidXLogRecPtr);
		replication_state->roident = node;
	}

	Assert(replication_state->roident != InvalidRepOriginId);

	/*
	 * If somebody "forcefully" sets this slot, WAL log it, so it's durable
	 * and the standby gets the message. Primarily this will be called during
	 * WAL replay (of commit records) where no WAL logging is necessary.
	 */
	if (wal_log)
	{
		xl_replorigin_set xlrec;

		xlrec.remote_lsn = remote_commit;
		xlrec.node_id = node;
		xlrec.force = go_backward;

		XLogBeginInsert();
		XLogRegisterData((char *) (&xlrec), sizeof(xlrec));

		XLogInsert(RM_REPLORIGIN_ID, XLOG_REPLORIGIN_SET);
	}

	/*
	 * Due to - harmless - race conditions during a checkpoint we could see
	 * values here that are older than the ones we already have in memory.
	 * Don't overwrite those.
	 */
	if (go_backward || replication_state->remote_lsn < remote_commit)
		replication_state->remote_lsn = remote_commit;
	if (local_commit != InvalidXLogRecPtr &&
		(go_backward || replication_state->local_lsn < local_commit))
		replication_state->local_lsn = local_commit;
	LWLockRelease(&replication_state->lock);

	/*
	 * Release *after* changing the LSNs, slot isn't acquired and thus could
	 * otherwise be dropped anytime.
	 */
	LWLockRelease(ReplicationOriginLock);
}


XLogRecPtr
replorigin_get_progress(RepOriginId node, bool flush)
{
	int			i;
	XLogRecPtr	local_lsn = InvalidXLogRecPtr;
	XLogRecPtr	remote_lsn = InvalidXLogRecPtr;

	/* prevent slots from being concurrently dropped */
	LWLockAcquire(ReplicationOriginLock, LW_SHARED);

	for (i = 0; i < max_replication_slots; i++)
	{
		ReplicationState *state;

		state = &replication_states[i];

		if (state->roident == node)
		{
			LWLockAcquire(&state->lock, LW_SHARED);

			remote_lsn = state->remote_lsn;
			local_lsn = state->local_lsn;

			LWLockRelease(&state->lock);

			break;
		}
	}

	LWLockRelease(ReplicationOriginLock);

	if (flush && local_lsn != InvalidXLogRecPtr)
		XLogFlush(local_lsn);

	return remote_lsn;
}

/*
 * Tear down a (possibly) configured session replication origin during process
 * exit.
 */
static void
ReplicationOriginExitCleanup(int code, Datum arg)
{
	LWLockAcquire(ReplicationOriginLock, LW_EXCLUSIVE);

	if (session_replication_state != NULL &&
		session_replication_state->acquired_by == MyProcPid)
	{
		session_replication_state->acquired_by = 0;
		session_replication_state = NULL;
	}

	LWLockRelease(ReplicationOriginLock);
}

/*
 * Setup a replication origin in the shared memory struct if it doesn't
 * already exists and cache access to the specific ReplicationSlot so the
 * array doesn't have to be searched when calling
 * replorigin_session_advance().
 *
 * Obviously only one such cached origin can exist per process and the current
 * cached value can only be set again after the previous value is torn down
 * with replorigin_session_reset().
 */
void
replorigin_session_setup(RepOriginId node)
{
	static bool registered_cleanup;
	int			i;
	int			free_slot = -1;

	if (!registered_cleanup)
	{
		on_shmem_exit(ReplicationOriginExitCleanup, 0);
		registered_cleanup = true;
	}

	Assert(max_replication_slots > 0);

	if (session_replication_state != NULL)
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
		errmsg("cannot setup replication origin when one is already setup")));

	/* Lock exclusively, as we may have to create a new table entry. */
	LWLockAcquire(ReplicationOriginLock, LW_EXCLUSIVE);

	/*
	 * Search for either an existing slot for the origin, or a free one we can
	 * use.
	 */
	for (i = 0; i < max_replication_slots; i++)
	{
		ReplicationState *curstate = &replication_states[i];

		/* remember where to insert if necessary */
		if (curstate->roident == InvalidRepOriginId &&
			free_slot == -1)
		{
			free_slot = i;
			continue;
		}

		/* not our slot */
		if (curstate->roident != node)
			continue;

		else if (curstate->acquired_by != 0)
		{
			ereport(ERROR,
					(errcode(ERRCODE_OBJECT_IN_USE),
			 errmsg("replication identiefer %d is already active for pid %d",
					curstate->roident, curstate->acquired_by)));
		}

		/* ok, found slot */
		session_replication_state = curstate;
	}


	if (session_replication_state == NULL && free_slot == -1)
		ereport(ERROR,
				(errcode(ERRCODE_CONFIGURATION_LIMIT_EXCEEDED),
				 errmsg("no free replication state slot could be found for replication origin with oid %u",
						node),
				 errhint("Increase max_replication_slots and try again.")));
	else if (session_replication_state == NULL)
	{
		/* initialize new slot */
		session_replication_state = &replication_states[free_slot];
		Assert(session_replication_state->remote_lsn == InvalidXLogRecPtr);
		Assert(session_replication_state->local_lsn == InvalidXLogRecPtr);
		session_replication_state->roident = node;
	}


	Assert(session_replication_state->roident != InvalidRepOriginId);

	session_replication_state->acquired_by = MyProcPid;

	LWLockRelease(ReplicationOriginLock);
}

/*
 * Reset replay state previously setup in this session.
 *
 * This function may only be called if an origin was setup with
 * replorigin_session_setup().
 */
void
replorigin_session_reset(void)
{
	Assert(max_replication_slots != 0);

	if (session_replication_state == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("no replication origin is configured")));

	LWLockAcquire(ReplicationOriginLock, LW_EXCLUSIVE);

	session_replication_state->acquired_by = 0;
	session_replication_state = NULL;

	LWLockRelease(ReplicationOriginLock);
}

/*
 * Do the same work replorigin_advance() does, just on the session's
 * configured origin.
 *
 * This is noticeably cheaper than using replorigin_advance().
 */
void
replorigin_session_advance(XLogRecPtr remote_commit, XLogRecPtr local_commit)
{
	Assert(session_replication_state != NULL);
	Assert(session_replication_state->roident != InvalidRepOriginId);

	LWLockAcquire(&session_replication_state->lock, LW_EXCLUSIVE);
	if (session_replication_state->local_lsn < local_commit)
		session_replication_state->local_lsn = local_commit;
	if (session_replication_state->remote_lsn < remote_commit)
		session_replication_state->remote_lsn = remote_commit;
	LWLockRelease(&session_replication_state->lock);
}

/*
 * Ask the machinery about the point up to which we successfully replayed
 * changes from an already setup replication origin.
 */
XLogRecPtr
replorigin_session_get_progress(bool flush)
{
	XLogRecPtr	remote_lsn;
	XLogRecPtr	local_lsn;

	Assert(session_replication_state != NULL);

	LWLockAcquire(&session_replication_state->lock, LW_SHARED);
	remote_lsn = session_replication_state->remote_lsn;
	local_lsn = session_replication_state->local_lsn;
	LWLockRelease(&session_replication_state->lock);

	if (flush && local_lsn != InvalidXLogRecPtr)
		XLogFlush(local_lsn);

	return remote_lsn;
}



/* ---------------------------------------------------------------------------
 * SQL functions for working with replication origin.
 *
 * These mostly should be fairly short wrappers around more generic functions.
 * ---------------------------------------------------------------------------
 */

/*
 * Create replication origin for the passed in name, and return the assigned
 * oid.
 */
Datum
pg_replication_origin_create(PG_FUNCTION_ARGS)
{
	char	   *name;
	RepOriginId roident;

	replorigin_check_prerequisites(false, false);

	name = text_to_cstring((text *) DatumGetPointer(PG_GETARG_DATUM(0)));
	roident = replorigin_create(name);

	pfree(name);

	PG_RETURN_OID(roident);
}

/*
 * Drop replication origin.
 */
Datum
pg_replication_origin_drop(PG_FUNCTION_ARGS)
{
	char	   *name;
	RepOriginId roident;

	replorigin_check_prerequisites(false, false);

	name = text_to_cstring((text *) DatumGetPointer(PG_GETARG_DATUM(0)));

	roident = replorigin_by_name(name, false);
	Assert(OidIsValid(roident));

	replorigin_drop(roident);

	pfree(name);

	PG_RETURN_VOID();
}

/*
 * Return oid of a replication origin.
 */
Datum
pg_replication_origin_oid(PG_FUNCTION_ARGS)
{
	char	   *name;
	RepOriginId roident;

	replorigin_check_prerequisites(false, false);

	name = text_to_cstring((text *) DatumGetPointer(PG_GETARG_DATUM(0)));
	roident = replorigin_by_name(name, true);

	pfree(name);

	if (OidIsValid(roident))
		PG_RETURN_OID(roident);
	PG_RETURN_NULL();
}

/*
 * Setup a replication origin for this session.
 */
Datum
pg_replication_origin_session_setup(PG_FUNCTION_ARGS)
{
	char	   *name;
	RepOriginId origin;

	replorigin_check_prerequisites(true, false);

	name = text_to_cstring((text *) DatumGetPointer(PG_GETARG_DATUM(0)));
	origin = replorigin_by_name(name, false);
	replorigin_session_setup(origin);

	replorigin_sesssion_origin = origin;

	pfree(name);

	PG_RETURN_VOID();
}

/*
 * Reset previously setup origin in this session
 */
Datum
pg_replication_origin_session_reset(PG_FUNCTION_ARGS)
{
	replorigin_check_prerequisites(true, false);

	replorigin_session_reset();

	/* FIXME */
	replorigin_sesssion_origin = InvalidRepOriginId;
	replorigin_sesssion_origin_lsn = InvalidXLogRecPtr;
	replorigin_sesssion_origin_timestamp = 0;

	PG_RETURN_VOID();
}

/*
 * Has a replication origin been setup for this session.
 */
Datum
pg_replication_origin_session_is_setup(PG_FUNCTION_ARGS)
{
	replorigin_check_prerequisites(false, false);

	PG_RETURN_BOOL(replorigin_sesssion_origin != InvalidRepOriginId);
}


/*
 * Return the replication progress for origin setup in the current session.
 *
 * If 'flush' is set to true it is ensured that the returned value corresponds
 * to a local transaction that has been flushed. this is useful if asychronous
 * commits are used when replaying replicated transactions.
 */
Datum
pg_replication_origin_session_progress(PG_FUNCTION_ARGS)
{
	XLogRecPtr	remote_lsn = InvalidXLogRecPtr;
	bool		flush = PG_GETARG_BOOL(0);

	replorigin_check_prerequisites(true, false);

	if (session_replication_state == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("no replication origin is configured")));

	remote_lsn = replorigin_session_get_progress(flush);

	if (remote_lsn == InvalidXLogRecPtr)
		PG_RETURN_NULL();

	PG_RETURN_LSN(remote_lsn);
}

Datum
pg_replication_origin_xact_setup(PG_FUNCTION_ARGS)
{
	XLogRecPtr	location = PG_GETARG_LSN(0);

	replorigin_check_prerequisites(true, false);

	if (session_replication_state == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("no replication origin is configured")));

	replorigin_sesssion_origin_lsn = location;
	replorigin_sesssion_origin_timestamp = PG_GETARG_TIMESTAMPTZ(1);

	PG_RETURN_VOID();
}

Datum
pg_replication_origin_xact_reset(PG_FUNCTION_ARGS)
{
	replorigin_check_prerequisites(true, false);

	replorigin_sesssion_origin_lsn = InvalidXLogRecPtr;
	replorigin_sesssion_origin_timestamp = 0;

	PG_RETURN_VOID();
}


Datum
pg_replication_origin_advance(PG_FUNCTION_ARGS)
{
	text	   *name = PG_GETARG_TEXT_P(0);
	XLogRecPtr	remote_commit = PG_GETARG_LSN(1);
	RepOriginId node;

	replorigin_check_prerequisites(true, false);

	/* lock to prevent the replication origin from vanishing */
	LockRelationOid(ReplicationOriginRelationId, RowExclusiveLock);

	node = replorigin_by_name(text_to_cstring(name), false);

	/*
	 * Can't sensibly pass a local commit to be flushed at checkpoint - this
	 * xact hasn't committed yet. This is why this function should be used to
	 * set up the initial replication state, but not for replay.
	 */
	replorigin_advance(node, remote_commit, InvalidXLogRecPtr,
					   true /* go backward */ , true /* wal log */ );

	UnlockRelationOid(ReplicationOriginRelationId, RowExclusiveLock);

	PG_RETURN_VOID();
}


/*
 * Return the replication progress for an individual replication origin.
 *
 * If 'flush' is set to true it is ensured that the returned value corresponds
 * to a local transaction that has been flushed. this is useful if asychronous
 * commits are used when replaying replicated transactions.
 */
Datum
pg_replication_origin_progress(PG_FUNCTION_ARGS)
{
	char	   *name;
	bool		flush;
	RepOriginId roident;
	XLogRecPtr	remote_lsn = InvalidXLogRecPtr;

	replorigin_check_prerequisites(true, true);

	name = text_to_cstring((text *) DatumGetPointer(PG_GETARG_DATUM(0)));
	flush = PG_GETARG_BOOL(1);

	roident = replorigin_by_name(name, false);
	Assert(OidIsValid(roident));

	remote_lsn = replorigin_get_progress(roident, flush);

	if (remote_lsn == InvalidXLogRecPtr)
		PG_RETURN_NULL();

	PG_RETURN_LSN(remote_lsn);
}


Datum
pg_show_replication_origin_status(PG_FUNCTION_ARGS)
{
	ReturnSetInfo *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
	TupleDesc	tupdesc;
	Tuplestorestate *tupstore;
	MemoryContext per_query_ctx;
	MemoryContext oldcontext;
	int			i;
#define REPLICATION_ORIGIN_PROGRESS_COLS 4

	/* we we want to return 0 rows if slot is set to zero */
	replorigin_check_prerequisites(false, true);

	if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("set-valued function called in context that cannot accept a set")));
	if (!(rsinfo->allowedModes & SFRM_Materialize))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("materialize mode required, but it is not allowed in this context")));
	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	if (tupdesc->natts != REPLICATION_ORIGIN_PROGRESS_COLS)
		elog(ERROR, "wrong function definition");

	per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
	oldcontext = MemoryContextSwitchTo(per_query_ctx);

	tupstore = tuplestore_begin_heap(true, false, work_mem);
	rsinfo->returnMode = SFRM_Materialize;
	rsinfo->setResult = tupstore;
	rsinfo->setDesc = tupdesc;

	MemoryContextSwitchTo(oldcontext);


	/* prevent slots from being concurrently dropped */
	LWLockAcquire(ReplicationOriginLock, LW_SHARED);

	/*
	 * Iterate through all possible replication_states, display if they are
	 * filled. Note that we do not take any locks, so slightly corrupted/out
	 * of date values are a possibility.
	 */
	for (i = 0; i < max_replication_slots; i++)
	{
		ReplicationState *state;
		Datum		values[REPLICATION_ORIGIN_PROGRESS_COLS];
		bool		nulls[REPLICATION_ORIGIN_PROGRESS_COLS];
		char	   *roname;

		state = &replication_states[i];

		/* unused slot, nothing to display */
		if (state->roident == InvalidRepOriginId)
			continue;

		memset(values, 0, sizeof(values));
		memset(nulls, 1, sizeof(nulls));

		values[0] = ObjectIdGetDatum(state->roident);
		nulls[0] = false;

		/*
		 * We're not preventing the origin to be dropped concurrently, so
		 * silently accept that it might be gone.
		 */
		if (replorigin_by_oid(state->roident, true,
							  &roname))
		{
			values[1] = CStringGetTextDatum(roname);
			nulls[1] = false;
		}

		LWLockAcquire(&state->lock, LW_SHARED);

		values[2] = LSNGetDatum(state->remote_lsn);
		nulls[2] = false;

		values[3] = LSNGetDatum(state->local_lsn);
		nulls[3] = false;

		LWLockRelease(&state->lock);

		tuplestore_putvalues(tupstore, tupdesc, values, nulls);
	}

	tuplestore_donestoring(tupstore);

	LWLockRelease(ReplicationOriginLock);

#undef REPLICATION_ORIGIN_PROGRESS_COLS

	return (Datum) 0;
}
