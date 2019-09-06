#include "catalog/storage_tablespace.h"


static Oid pending_tablespace_scheduled_for_deletion_on_abort;
static Oid pending_tablespace_scheduled_for_deletion_on_commit;
static void (*unlink_tablespace_dir)(Oid tablespace_for_unlink, bool is_redo);


static void
tablespace_storage_reset_on_abort(void)
{
	pending_tablespace_scheduled_for_deletion_on_abort = InvalidOid;
}

static void
tablespace_storage_reset_on_commit(void)
{
	pending_tablespace_scheduled_for_deletion_on_commit = InvalidOid;
}

static void
perform_pending_tablespace_deletion_internal(Oid tablespace_oid_to_delete,
                                                         bool isRedo)
{
	if (!OidIsValid(tablespace_oid_to_delete))
		return;

	unlink_tablespace_dir(tablespace_oid_to_delete, isRedo);
} 

void
TablespaceStorageInit(void (*unlink_tablespace_dir_function)(Oid tablespace_oid, bool is_redo))
{
	unlink_tablespace_dir = unlink_tablespace_dir_function;

	tablespace_storage_reset_on_abort();
	tablespace_storage_reset_on_commit();
}

/*
 * For abort:
 */
void
ScheduleTablespaceDirectoryDeletionForAbort(Oid tablespace_oid)
{
	pending_tablespace_scheduled_for_deletion_on_abort = tablespace_oid;
}

Oid
GetPendingTablespaceForDeletionForAbort(void)
{
	return pending_tablespace_scheduled_for_deletion_on_abort;
}

void
DoPendingTablespaceDeletionForAbort(void)
{
	perform_pending_tablespace_deletion_internal(
		GetPendingTablespaceForDeletionForAbort(),
		false
		);
	tablespace_storage_reset_on_abort();
}

void
UnscheduleTablespaceDirectoryDeletionForAbort(void)
{
	tablespace_storage_reset_on_abort();
}


/* 
 * For commit:
 */
void
ScheduleTablespaceDirectoryDeletionForCommit(Oid tablespaceoid)
{
	pending_tablespace_scheduled_for_deletion_on_commit = tablespaceoid;
}


void
UnscheduleTablespaceDirectoryDeletionForCommit(void)
{
	tablespace_storage_reset_on_commit();
}


Oid
GetPendingTablespaceForDeletionForCommit(void)
{
	return pending_tablespace_scheduled_for_deletion_on_commit;
}


void
DoPendingTablespaceDeletionForCommit(void)
{
	perform_pending_tablespace_deletion_internal(
		GetPendingTablespaceForDeletionForCommit(),
		false);

	tablespace_storage_reset_on_commit();
}


/*
 * For Xlog redo:
 *
 */
void
DoTablespaceDeletionForRedoXlog(Oid tablespace_oid_to_delete)
{
	perform_pending_tablespace_deletion_internal(tablespace_oid_to_delete, true);
}
