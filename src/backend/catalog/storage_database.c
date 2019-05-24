#include "postgres.h"

#include "access/xlogutils.h"
#include "catalog/pg_database.h"
#include "catalog/storage_database.h"
#include "common/relpath.h"
#include "utils/faultinjector.h"
#include "storage/lmgr.h"

typedef struct PendingDbDelete
{
	DbDirNode	dbDirNode;		/* database dir that needs to be deleted */
	bool		atCommit;		/* T=delete at commit; F=delete at abort */
	struct PendingDbDelete *next;		/* linked-list link */
} PendingDbDelete;

static void dropDatabaseDirectory(DbDirNode *deldb, bool isRedo);

static PendingDbDelete *pendingDbDeletes = NULL; /* head of linked list */
static Oid sessionLockMoveDbOid = InvalidOid;

/*
 * ScheduleDbDirDelete
 * 		Schedule dboid dir removal at transaction commit/abort
 */

void
ScheduleDbDirDelete(Oid db_id, Oid tablespace_oid, bool forCommit)
{
	PendingDbDelete *pending;

	/* Add the relation to the list of stuff to delete at commit */
	pending = (PendingDbDelete *)
		MemoryContextAlloc(TopMemoryContext, sizeof(PendingDbDelete));
	pending->atCommit = forCommit;
	pending->dbDirNode = (DbDirNode)
		{.tablespace = tablespace_oid, .database = db_id};
	pending->next = pendingDbDeletes;
	pendingDbDeletes = pending;
}

void
DoPendingDbDeletes(bool isCommit)
{
	PendingDbDelete *pending;
	PendingDbDelete *next;

	for (pending = pendingDbDeletes; pending != NULL; pending = next)
	{
		next = pending->next;
		/* unlink list entry first, so we don't retry on failure */
		pendingDbDeletes = next;
		/* do deletion if called for */
		if (pending->atCommit == isCommit)
			dropDatabaseDirectory(&pending->dbDirNode,
								  false);

		pfree(pending);
	}
}

void
MoveDbSessionLockAcquire(Oid db_id)
{
	Assert(sessionLockMoveDbOid == InvalidOid);
	LockSharedObjectForSession(DatabaseRelationId, db_id, 0,
							   AccessExclusiveLock);
	sessionLockMoveDbOid = db_id;
}

void
MoveDbSessionLockRelease()
{
	if (sessionLockMoveDbOid == InvalidOid)
		return;
	
	UnlockSharedObjectForSession(DatabaseRelationId, sessionLockMoveDbOid, 0,
							 AccessExclusiveLock);
	DatabaseStorageResetSessionLock();
}

void
DatabaseStorageResetSessionLock()
{
	sessionLockMoveDbOid = InvalidOid;
}

int
GetPendingDbDeletes(bool forCommit, DbDirNode **ptr)
{
	int			ndbs;
	DbDirNode	*dbptr;
	PendingDbDelete *pending;

	ndbs = 0;
	for (pending = pendingDbDeletes; pending != NULL; pending = pending->next)
	{
		if (pending->atCommit == forCommit)
			ndbs++;
	}
	if (ndbs == 0)
	{
		*ptr = NULL;
		return 0;
	}
	dbptr = (DbDirNode *) palloc(ndbs * sizeof(DbDirNode));
	*ptr = dbptr;
	for (pending = pendingDbDeletes; pending != NULL; pending = pending->next)
	{
		if (pending->atCommit == forCommit)
		{
			*dbptr = pending->dbDirNode;
			dbptr++;
		}
	}
	return ndbs;
}

/*
 * This functions contains non-catalog modifications to be performed for movedb().
 * Its called after successfully marking the transaction as committed via pending
 * deletes.
 */
void
DropDatabaseDirectories(DbDirNode *deldbs, int ndeldbs, bool isRedo)
{
	int i;
	for (i = 0; i < ndeldbs; i++)
	{
		dropDatabaseDirectory(&deldbs[i], isRedo);
	}

#ifdef FAULT_INJECTOR
	if(ndeldbs > 0)
		SIMPLE_FAULT_INJECTOR("after_drop_database_directories");
#endif
}

/*
 *	PostPrepare_DatabaseStorage -- Clean up after a successful PREPARE
 *
 * What we have to do here is throw away the in-memory state about pending
 * db deletes. It's all been recorded in the 2PC state file and
 * we no longer need to track it in memory.
 */
void
PostPrepare_DatabaseStorage()
{
	PendingDbDelete *pendingDbDelete;
	PendingDbDelete *nextDbDelete;

	for (pendingDbDelete = pendingDbDeletes; pendingDbDelete != NULL; pendingDbDelete = nextDbDelete)
	{
		nextDbDelete = pendingDbDelete->next;
		pendingDbDeletes = nextDbDelete;
		/* must explicitly free the list entry */
		pfree(pendingDbDelete);
	}

	DatabaseStorageResetSessionLock();
}

static void
dropDatabaseDirectory(DbDirNode *deldb, bool isRedo)
{
	char *dbpath = GetDatabasePath(deldb->database, deldb->tablespace);
	/*
	 * Remove files from the old tablespace
	 */
	if (!rmtree(dbpath, true))
		ereport(WARNING,
				(errmsg("some useless files may be left behind in old database directory \"%s\"",
						dbpath)));

	if (isRedo)
		XLogDropDatabase(deldb->database);
}