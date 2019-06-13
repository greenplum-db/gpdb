#include "catalog/storage_tablespace.h"


static Oid pending_tablespace_scheduled_for_deletion;
static void (*unlink_tablespace_dir)(Oid tablespace_for_unlink, bool is_redo);


static void
tablespace_storage_reset(void)
{
	pending_tablespace_scheduled_for_deletion = InvalidOid;
}

static void
perform_pending_tablespace_deletion_internal(Oid tablespace_oid_to_delete,
                                                         bool isRedo)
{
	if (tablespace_oid_to_delete == InvalidOid)
		return;

	unlink_tablespace_dir(tablespace_oid_to_delete, isRedo);
	tablespace_storage_reset();
} 

void
TablespaceStorageInit(void (*unlink_tablespace_dir_function)(Oid tablespace_oid, bool is_redo))
{
	unlink_tablespace_dir = unlink_tablespace_dir_function;

	tablespace_storage_reset();
}

void
ScheduleTablespaceDirectoryDeletion(Oid tablespace_oid)
{
	pending_tablespace_scheduled_for_deletion = tablespace_oid;
}

Oid
GetPendingTablespaceForDeletion(void)
{
	return pending_tablespace_scheduled_for_deletion;
}

void
DoPendingTablespaceDeletion(void)
{
	perform_pending_tablespace_deletion_internal(
		GetPendingTablespaceForDeletion(),
		false
		);
}

void
DoTablespaceDeletion(Oid tablespace_oid_to_delete)
{
	perform_pending_tablespace_deletion_internal(tablespace_oid_to_delete, true);
}

void
UnscheduleTablespaceDirectoryDeletion(void)
{
	tablespace_storage_reset();
}
