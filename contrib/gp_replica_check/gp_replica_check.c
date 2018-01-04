#include "postgres.h"

#include "access/heapam.h"
#include "access/nbtree.h"
#include "access/gist_private.h"
#include "access/gin.h"
#include "commands/sequence.h"
#include "postmaster/bgwriter.h"
#include "replication/walsender_private.h"
#include "replication/walsender.h"
#include "catalog/catalog.h"
#include "catalog/pg_tablespace.h"
#include "storage/fd.h"
#include "utils/builtins.h"
#include "utils/hsearch.h"

/*
 * If a file comparison fails, how many times to retry before admitting
 * that it really differs?
 */
#define NUM_RETRIES		3

/*
 * How many seconds to wait for checkpoint record to be applied in standby?
 */
#define NUM_CHECKPOINT_SYNC_TIMEOUT 60

/*
 * Not all the FSM and VM changes are WAL-logged and its OK if they are out of
 * date. So it is OK to skip them for consistency check.
 */

#define should_skip(filename)	(pg_strncasecmp(filename, "pg", 2) == 0 \
								|| pg_strncasecmp(filename, ".", 1) == 0 \
								|| pg_strncasecmp(filename + strlen(filename) - 4, "_fsm", 4) == 0 \
								|| pg_strncasecmp(filename + strlen(filename) - 3, "_vm", 3) == 0)

PG_MODULE_MAGIC;

extern Datum gp_replica_check(PG_FUNCTION_ARGS);

typedef struct RelfilenodeEntry
{
	Oid relfilenode;
	int relam;
	int relkind;
	char relstorage;
	List *segments;
} RelfilenodeEntry;

typedef struct RelationTypeData
{
	char *name;
	bool include;
} RelationTypeData;

#define MAX_INCLUDE_RELATION_TYPES 8

static RelationTypeData relation_types[MAX_INCLUDE_RELATION_TYPES] = {
	{"btree", false},
	{"hash", false},
	{"gist", false},
	{"gin", false},
	{"bitmap", false},
	{"heap", false},
	{"sequence", false},
	{"ao", false}
};

static void init_relation_types(char *include_relation_types);
static RelationTypeData get_relation_type_data(int relam, char relstorage, int relkind);
static void mask_block(char *pagedata, BlockNumber blkno, int relam, int relkind);
static bool compare_files(char* primaryfilepath, char* mirrorfilepath, RelfilenodeEntry *rentry);
static bool sync_wait(void);
static HTAB* get_relfilenode_map();
static RelfilenodeEntry* get_relfilenode_entry(char *relfilenode, HTAB *relfilenode_map);

static void
init_relation_types(char *include_relation_types)
{
	List *elemlist;
	ListCell *l;

	if (!SplitIdentifierString(include_relation_types, ',', &elemlist))
	{
		list_free(elemlist);

		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("List syntax is invalid.")));
	}

	foreach(l, elemlist)
	{
		char *tok = (char *) lfirst(l);
		bool found = false;
		int type;

		/* Check for 'all' */
		if (pg_strcasecmp(tok, "all") == 0)
		{
			for (type = 0; type < MAX_INCLUDE_RELATION_TYPES; type++)
				relation_types[type].include = true;

			found = true;
		}
		else
		{
			for (type = 0; type < MAX_INCLUDE_RELATION_TYPES; type++)
			{
				if (pg_strcasecmp(tok, relation_types[type].name) == 0)
				{
					relation_types[type].include = true;
					found = true;
					break;
				}
			}
		}

		if (!found)
		{
			list_free(elemlist);
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("Unrecognized key word: \"%s\".", tok)));
		}
	}

	list_free(elemlist);
}

static RelationTypeData
get_relation_type_data(int relam, char relstorage, int relkind)
{
	switch(relam)
	{
		case BTREE_AM_OID:
			return relation_types[0];
		case HASH_AM_OID:
			return relation_types[1];
		case GIST_AM_OID:
			return relation_types[2];
		case GIN_AM_OID:
			return relation_types[3];
		case BITMAP_AM_OID:
			return relation_types[4];
		default:
			if (relstorage == RELSTORAGE_HEAP)
				if (relkind == RELKIND_SEQUENCE)
					return relation_types[6];
				else
					return relation_types[5];
			else if (relstorage_is_ao(relstorage))
				return relation_types[7];
			else
				ereport(ERROR,
						(errmsg("invalid relam %d or relstorage %c",
								relam, relstorage)));
	}
}

static void
mask_block(char *pagedata, BlockNumber blockno, int relam, int relkind)
{
	switch(relam)
	{
		case BTREE_AM_OID:
			btree_mask(pagedata, blockno);
			break;

		case GIST_AM_OID:
			gist_mask(pagedata, blockno);
			break;

		case GIN_AM_OID:
			gin_mask(pagedata, blockno);
			break;

		/* heap table */
		default:
			if (relkind == RELKIND_SEQUENCE)
				seq_mask(pagedata, blockno);
			else
				heap_mask(pagedata, blockno);

			break;
	}
}

/*
 * Perform a checkpoint, and wait for it to be sent to and applied in all
 * replicas.
 */
static bool
sync_wait(void)
{
	int			retry;
	XLogRecPtr	ckpt_lsn;

	CHECK_FOR_INTERRUPTS();

	/*
	 * Request a checkpoint on first call, to flush out data changes from
	 * shared buffer to disk.
	 */
	RequestCheckpoint(CHECKPOINT_IMMEDIATE | CHECKPOINT_FORCE | CHECKPOINT_WAIT);

	ckpt_lsn = GetRedoRecPtr();

	retry = 0;
	while (retry < NUM_CHECKPOINT_SYNC_TIMEOUT)
	{
		int			i;

		CHECK_FOR_INTERRUPTS();

		LWLockAcquire(SyncRepLock, LW_SHARED);
		for (i = 0; i < max_wal_senders; i++)
		{
			/* Ignore standbys that are not connected at the moment */
			if (WalSndCtl->walsnds[i].pid == 0)
				continue;

			if (WalSndCtl->walsnds[i].state != WALSNDSTATE_STREAMING
				|| !XLByteLE(ckpt_lsn, WalSndCtl->walsnds[i].apply))
				break;
		}
		LWLockRelease(SyncRepLock);

		/*
		 * The above loop did not break early on any wal sender so they should
		 * all be in sync.
		 */
		if (i == max_wal_senders)
			return true;

		pg_usleep(1000000 /* 1 second */);
		retry++;
	}

	return false;
}

static bool
compare_files(char *primaryfilepath, char *mirrorfilepath, RelfilenodeEntry *rentry)
{
	File		primaryFile = -1;
	File		mirrorFile = -1;
	BlockNumber blockno;
	int			attempts = 0;
	bool		any_retries = false;
	bool		primaryFileExists;
	bool		mirrorFileExists;

	blockno = 0;

	/*
	 * If there's any discrepancy between the files below, we will loop back
	 * here. If NUM_RETRIES is reached, return error.
	 */
retry:
	CHECK_FOR_INTERRUPTS();

	/* If the files were still open from previous attempt, close them first. */
	if (primaryFile != -1)
	{
		FileClose(primaryFile);
		primaryFile = -1;
	}
	if (mirrorFile != -1)
	{
		FileClose(mirrorFile);
		mirrorFile = -1;
	}

	if (attempts == NUM_RETRIES)
	{
		ereport(WARNING,
				(errmsg("%s files \"%s\" and \"%s\" mismatch at blockno %d, gave up after %d retries",
						get_relation_type_data(rentry->relam, rentry->relstorage, rentry->relkind).name,
						primaryfilepath, mirrorfilepath, blockno, attempts)));
		return false;
	}
	attempts++;

	if (attempts > 1)
	{
		any_retries = true;
		/*
		 * Request a checkpoint on first call, to flush out data changes from
		 * shared buffer to disk.
		 */
		if (!sync_wait())
			return false;
	}

	/*
	 * Attempt to open both files.
	 */
	primaryFileExists = true;
	primaryFile = PathNameOpenFile(primaryfilepath, O_RDONLY | PG_BINARY, S_IRUSR);
	if (primaryFile < 0)
	{
		if (errno == ENOENT)
			primaryFileExists = false;
		else
			elog(WARNING, "could not open file \"%s\": %m", primaryfilepath);
	}

	mirrorFileExists = true;
	mirrorFile = PathNameOpenFile(mirrorfilepath, O_RDONLY | PG_BINARY, S_IRUSR);
	if (mirrorFile < 0)
	{
		if (errno == ENOENT)
			mirrorFileExists = false;
		else
			elog(WARNING, "could not open file \"%s\": %m", mirrorfilepath);
	}

	/* Did it succeed? Neither one, just one of them, or both? */
	if (!primaryFileExists && !mirrorFileExists)
	{
		elog(NOTICE, "file \"%s\" was concurrently deleted on primary and mirror", primaryfilepath);
		return true;
	}

	if (!primaryFileExists && mirrorFileExists)
	{
		elog(NOTICE, "file \"%s\" was concurrently deleted on primary", primaryfilepath);
		goto retry;
	}

	if (primaryFileExists && !mirrorFileExists)
	{
		elog(NOTICE, "file \"%s\" was concurrently deleted on mirror", mirrorfilepath);
		goto retry;
	}

	/*
	 * Otherwise, both files were opened successfully. Compare them block-by block.
	 *
	 * Note: if this is not the first attempt, we keep the block number across attempts,
	 * rather than always starting from the beginning of the file.
	 */
	while (true)
	{
		char		primaryFileBuf[BLCKSZ];
		char		mirrorFileBuf[BLCKSZ];
		int			primaryFileBytesRead;
		int			mirrorFileBytesRead;

		CHECK_FOR_INTERRUPTS();

		if (FileSeek(primaryFile, (int64) blockno * BLCKSZ, SEEK_SET) < 0)
		{
			elog(NOTICE, "seek in file \"%s\" failed: %m", primaryfilepath);
			goto retry;
		}
		if (FileSeek(mirrorFile, (int64) blockno * BLCKSZ, SEEK_SET) < 0)
		{
			elog(NOTICE, "seek in file \"%s\" failed: %m", mirrorFileBuf);
			goto retry;
		}

		primaryFileBytesRead = FileRead(primaryFile, primaryFileBuf, sizeof(primaryFileBuf));
		if (primaryFileBytesRead < 0)
		{
			elog(NOTICE, "could not read from file \"%s\", block %u: %m", primaryfilepath, blockno);
			goto retry;
		}
		mirrorFileBytesRead = FileRead(mirrorFile, mirrorFileBuf, sizeof(mirrorFileBuf));
		if (mirrorFileBytesRead < 0)
		{
			elog(NOTICE, "could not read from file \"%s\", block %u: %m", mirrorfilepath, blockno);
			goto retry;
		}

		if (primaryFileBytesRead != mirrorFileBytesRead)
		{
			/* length mismatch */
			ereport(NOTICE,
					(errmsg("%s files \"%s\" and \"%s\" mismatch at blockno %u, primary length: %i, mirror length: %i",
							get_relation_type_data(rentry->relam, rentry->relstorage, rentry->relkind).name,
							primaryfilepath, mirrorfilepath, blockno,
							primaryFileBytesRead, mirrorFileBytesRead)));
			goto retry;
		}

		if (primaryFileBytesRead == 0)
			break; /* reached EOF */

		if (rentry->relstorage == RELSTORAGE_HEAP)
		{
			if (primaryFileBytesRead != BLCKSZ)
			{
				elog(NOTICE, "short read of %d bytes from heap file \"%s\", block %u: %m", primaryFileBytesRead, primaryfilepath, blockno);
				goto retry;
			}
			/*
			 * Perform some basic sanity checks before handing the block to
			 * mask_block(). It might throw a hard ERROR on a bogus block,
			 * so we better catch that here so we can retry.
			 */
			if (!PageIsVerified(primaryFileBuf, blockno))
			{
				elog(NOTICE, "invalid page header or checksum in heap file \"%s\", block %u: %m", primaryfilepath, blockno);
				goto retry;
			}
			if (!PageIsVerified(mirrorFileBuf, blockno))
			{
				elog(NOTICE, "invalid page header or checksum in heap file \"%s\", block %u: %m", mirrorfilepath, blockno);
				goto retry;
			}

			if (!PageIsNew(primaryFileBuf) && !PageIsNew(mirrorFileBuf))
			{
				mask_block(primaryFileBuf, blockno, rentry->relam, rentry->relkind);
				mask_block(mirrorFileBuf, blockno, rentry->relam, rentry->relkind);
			}
		}

		if (memcmp(primaryFileBuf, mirrorFileBuf, primaryFileBytesRead) != 0)
		{
			/* different contents */
			ereport(NOTICE,
					(errmsg("%s files \"%s\" and \"%s\" mismatch at blockno %u",
							get_relation_type_data(rentry->relam, rentry->relstorage, rentry->relkind).name,
							primaryfilepath, mirrorfilepath, blockno)));
			goto retry;
		}

		/* Success! Advance to next block, and reset the retry-counter */
		attempts = 1;
		blockno++;
	}

	/* Reached end of file successfully! */

	if (primaryFile != -1)
		FileClose(primaryFile);
	if (mirrorFile != -1)
		FileClose(mirrorFile);

	/*
	 * The NOTICEs about differences can make the user think that something's
	 * wrong, even though they are normal if there is any concurrent activity.
	 * So if we emitted those NOTICEs, emit another NOTICE to reassure the
	 * user it was all right in the end.
	 *
	 * (It's next to impossible to quiesce the cluster so well that there would be
	 * no activity. Hint bits can be set even by read-only queries, for example.)
	 */
	if (any_retries)
		elog(NOTICE, "succeeded after retrying");

	return true;
}

static HTAB*
get_relfilenode_map()
{
	Relation pg_class;
	HeapScanDesc scan;
	HeapTuple tup = NULL;

	HTAB *relfilenodemap;
	HASHCTL relfilenodectl;
	int hash_flags;
	MemSet(&relfilenodectl, 0, sizeof(relfilenodectl));
	relfilenodectl.keysize = sizeof(Oid);
	relfilenodectl.entrysize = sizeof(RelfilenodeEntry);
	relfilenodectl.hash = oid_hash;
	hash_flags = (HASH_ELEM | HASH_FUNCTION);

	relfilenodemap = hash_create("relfilenode map", 50000, &relfilenodectl, hash_flags);

	pg_class = heap_open(RelationRelationId, AccessShareLock);
	scan = heap_beginscan(pg_class, SnapshotNow, 0, NULL);
	while((tup = heap_getnext(scan, ForwardScanDirection)) != NULL)
	{
		Form_pg_class classtuple = (Form_pg_class) GETSTRUCT(tup);
		if ((classtuple->relkind == RELKIND_UNCATALOGED
			 || classtuple->relkind == RELKIND_VIEW
			 || classtuple->relkind == RELKIND_COMPOSITE_TYPE)
			|| (classtuple->relstorage != RELSTORAGE_HEAP
				&& !relstorage_is_ao(classtuple->relstorage)))
			continue;

		RelfilenodeEntry *rentry;
		int rnode = classtuple->relfilenode;
		rentry = hash_search(relfilenodemap, (void *)&rnode, HASH_ENTER, NULL);
		rentry->relfilenode = classtuple->relfilenode;
		rentry->relam = classtuple->relam;
		rentry->relkind = classtuple->relkind;
		rentry->relstorage = classtuple->relstorage;
	}
	heap_endscan(scan);
	heap_close(pg_class, AccessShareLock);

	return relfilenodemap;
}

static RelfilenodeEntry*
get_relfilenode_entry(char *relfilenode, HTAB *relfilenode_map)
{
	bool found;

	int rnode = DatumGetObjectId(DirectFunctionCall1(oidin, CStringGetDatum(relfilenode)));
	RelfilenodeEntry *rentry = hash_search(relfilenode_map, (void *)&rnode, HASH_FIND, &found);

	if (found)
		return rentry;

	return NULL;
}

PG_FUNCTION_INFO_V1(gp_replica_check);

Datum
gp_replica_check(PG_FUNCTION_ARGS)
{
	char *primarydirpath = TextDatumGetCString(PG_GETARG_DATUM(0));
	char *mirrordirpath = TextDatumGetCString(PG_GETARG_DATUM(1));
	char *relation_types = TextDatumGetCString(PG_GETARG_DATUM(2));
	struct dirent *dent = NULL;
	bool dir_equal = true;

	init_relation_types(relation_types);

	/* TODO: Currently, we only scan the default tablespace */
	primarydirpath = psprintf("%s/%s",
							  primarydirpath,
							  GetDatabasePath(MyDatabaseId, DEFAULTTABLESPACE_OID));
	mirrordirpath = psprintf("%s/%s",
							 mirrordirpath,
							 GetDatabasePath(MyDatabaseId, DEFAULTTABLESPACE_OID));

	DIR *primarydir = AllocateDir(primarydirpath);
	DIR *mirrordir = AllocateDir(mirrordirpath);

	/*
	 * Checkpoint and wait until the standby has replayed it.
	 *
	 * XXX: There is currently no guarantee or wait that the standby has
	 * performed a restartpoint based on the checkpoint record, so this is
	 * not very robust.
	 */
	if (!sync_wait())
	{
		ereport(WARNING,
				(errmsg("unable to obtain start synced LSN values between primary and mirror")));
		PG_RETURN_BOOL(false);
	}

	/* Store information from pg_class for each relfilenode */
	HTAB *relfilenode_map = get_relfilenode_map();

	/*
	 * For each relfilenode in primary, if it is of type specified from user
	 * input, do comparison with its corresponding file on the mirror
	 */
	while ((dent = ReadDir(primarydir, primarydirpath)) != NULL)
	{
		char primaryfilename[MAXPGPATH] = {'\0'};
		char mirrorfilename[MAXPGPATH] = {'\0'};
		char *d_name_copy;
		char *relfilenode;
		bool match;

		if (should_skip(dent->d_name))
			continue;

		d_name_copy = pstrdup(dent->d_name);
		relfilenode = strtok(d_name_copy, ".");
		RelfilenodeEntry *rentry = get_relfilenode_entry(relfilenode, relfilenode_map);

		/* not a valid relfilenode */
		if (rentry == NULL)
		{
			ereport(WARNING,
					(errmsg("relfilenode %s not present in primary's pg_class",
							relfilenode)));
			continue;
		}

		/* skip if relation type not requested by user input */
		if (!get_relation_type_data(rentry->relam, rentry->relstorage, rentry->relkind).include)
			continue;

		d_name_copy = strtok(NULL, ".");
		if (d_name_copy != NULL)
			rentry->segments = lappend_int(rentry->segments, atoi(d_name_copy));

		snprintf(primaryfilename, MAXPGPATH, "%s/%s", primarydirpath, dent->d_name);
		snprintf(mirrorfilename, MAXPGPATH, "%s/%s", mirrordirpath, dent->d_name);

		/* do the file comparison */
		match = compare_files(primaryfilename, mirrorfilename, rentry);
		dir_equal = dir_equal && match;
	}
	FreeDir(primarydir);

	/* Open up mirrordirpath and verify each mirror file exist in the primary hash table */
	while ((dent = ReadDir(mirrordir, mirrordirpath)) != NULL)
	{
		char *d_name_copy;
		char *relfilenode;

		CHECK_FOR_INTERRUPTS();

		if (should_skip(dent->d_name))
			continue;

		d_name_copy = pstrdup(dent->d_name);
		relfilenode = strtok(d_name_copy, ".");
		RelfilenodeEntry *rentry = get_relfilenode_entry(relfilenode, relfilenode_map);

		if (rentry != NULL)
		{
			d_name_copy = strtok(NULL, ".");
			if (d_name_copy != NULL)
			{
				ListCell *l;
				bool found = false;
				foreach (l, rentry->segments)
				{
					if (lfirst_int(l) == atoi(d_name_copy))
					{
						found = true;
						break;
					}
				}

				if (!found && get_relation_type_data(rentry->relam, rentry->relstorage, rentry->relkind).include)
					ereport(WARNING,
							(errmsg("found extra %s file on mirror: %s/%s",
									get_relation_type_data(rentry->relam, rentry->relstorage, rentry->relkind).name,
									mirrordirpath,
									dent->d_name)));
			}
		}
		else
			ereport(WARNING,
					(errmsg("found extra unknown file on mirror: %s/%s",
							mirrordirpath, dent->d_name)));
	}
	FreeDir(mirrordir);
	hash_destroy(relfilenode_map);

	PG_RETURN_BOOL(dir_equal);
}
