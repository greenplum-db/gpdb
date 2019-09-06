#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "catalog/storage_tablespace.h"

static Oid unlink_called_with_tablespace_oid;
static bool unlink_called_with_redo;
static Oid NOT_CALLED_OID = -1000;


static void unlink_tablespace_directory(Oid tablespaceOid, bool isRedo) {
	unlink_called_with_tablespace_oid = tablespaceOid;
	unlink_called_with_redo = isRedo;
}

static void
setup()
{
	unlink_called_with_redo = false;
	unlink_called_with_tablespace_oid = NOT_CALLED_OID;

	TablespaceStorageInit(unlink_tablespace_directory);
}

/*
 * Tests
 */
static void
test_create_tablespace_storage_populates_the_pending_tablespace_deletes_list(
	void **state)
{
	setup();

	Oid someTablespaceOid = 17999;

	ScheduleTablespaceDirectoryDeletionForAbort(someTablespaceOid);

	Oid tablespaceForDeletion = GetPendingTablespaceForDeletionForAbort();

	assert_int_equal(17999, tablespaceForDeletion);

	setup();

	someTablespaceOid = 88888;

	ScheduleTablespaceDirectoryDeletionForAbort(someTablespaceOid);

	tablespaceForDeletion = GetPendingTablespaceForDeletionForAbort();

	assert_int_equal(88888, tablespaceForDeletion);
}

static void
test_get_pending_tablespace_for_deletion_returns_a_null_value_by_default(void **state)
{
	setup();

	Oid tablespaceForDeletion = GetPendingTablespaceForDeletionForAbort();

	assert_int_equal(InvalidOid, tablespaceForDeletion);
}

static void
test_DoPendingTablespaceDeletionForAbort_removes_the_pending_tablespace_for_deletion_so_it_is_not_deleted_by_the_next_transaction(
	void **state)
{
	setup();

	Oid someTablespaceOid = 99999;

	ScheduleTablespaceDirectoryDeletionForAbort(someTablespaceOid);

	DoPendingTablespaceDeletionForAbort();

	Oid tablespaceForDeletion = GetPendingTablespaceForDeletionForAbort();

	assert_int_equal(InvalidOid, tablespaceForDeletion);
}

static void
test_DoPendingTablespaceDeletionForAbort_calls_unlink(void **state)
{
	setup();

	ScheduleTablespaceDirectoryDeletionForAbort(99999);

	DoPendingTablespaceDeletionForAbort();

	assert_int_equal(unlink_called_with_tablespace_oid, 99999);
	assert_int_equal(unlink_called_with_redo, false);
}

static void
test_delete_called_when_invalid_tablespace_set_does_not_call_unlink(void **state) 
{
	setup();

	ScheduleTablespaceDirectoryDeletionForAbort(InvalidOid);

	DoPendingTablespaceDeletionForAbort();

	assert_int_equal(unlink_called_with_tablespace_oid, NOT_CALLED_OID);
}

static void
test_DoTablespaceDeletionForRedoXlog_calls_unlink_with_tablespace_oid_and_redo_flag(void **state) {
	setup();

	ScheduleTablespaceDirectoryDeletionForAbort(66666);

	DoTablespaceDeletionForRedoXlog(77777);

	assert_int_equal(unlink_called_with_tablespace_oid, 77777);
	assert_int_equal(unlink_called_with_redo, true);
}

static void
test_UnscheduleTablespaceDirectoryDeletionForAbort_removes_the_scheduled_tablespace_for_deletion(void **state)
{
	setup();

	ScheduleTablespaceDirectoryDeletionForAbort(66666);
	UnscheduleTablespaceDirectoryDeletionForAbort();

	Oid tablespaceForDeletion = GetPendingTablespaceForDeletionForAbort();

	assert_int_equal(tablespaceForDeletion, InvalidOid);

}

static void
test_an_UnscheduleTablespaceDirectoryDeletionForAbort_does_not_get_unlinked(void **state)
{
	setup();

	ScheduleTablespaceDirectoryDeletionForAbort(66666);
	UnscheduleTablespaceDirectoryDeletionForAbort();
	
	DoPendingTablespaceDeletionForAbort();

	assert_int_equal(unlink_called_with_tablespace_oid, NOT_CALLED_OID);
}

static void
test_a_tablespace_can_be_scheduled_for_deletion_on_commit(void **state)
{
	Oid pending_tablespace_for_deletion;
	setup();

	ScheduleTablespaceDirectoryDeletionForCommit(99999);

	pending_tablespace_for_deletion = GetPendingTablespaceForDeletionForCommit();
	
	assert_int_equal(pending_tablespace_for_deletion, 99999);
}

static void
test_a_tablespace_can_be_unscheduled_for_deletion_on_commit(void **state) 
{
	Oid pending_tablespace_for_deletion;
	setup();

	ScheduleTablespaceDirectoryDeletionForCommit(99999);

	UnscheduleTablespaceDirectoryDeletionForCommit();

	pending_tablespace_for_deletion = GetPendingTablespaceForDeletionForCommit();

	assert_int_equal(pending_tablespace_for_deletion, InvalidOid);
}

static void
test_a_tablespace_that_is_pending_is_deleted_on_commit(void **state)
{
	setup();

	ScheduleTablespaceDirectoryDeletionForCommit(77777);

	DoPendingTablespaceDeletionForCommit();

	assert_int_equal(unlink_called_with_tablespace_oid, 77777);
	assert_int_equal(unlink_called_with_redo, false);
}

static void
test_a_tablespace_that_has_been_deleted_on_commit_is_no_longer_pending(void **state)
{
	setup();

	ScheduleTablespaceDirectoryDeletionForCommit(77777);

	DoPendingTablespaceDeletionForCommit();

	Oid pending_tablespace_for_deletion = -1;
	
	pending_tablespace_for_deletion = GetPendingTablespaceForDeletionForCommit();
	
	assert_int_equal(pending_tablespace_for_deletion, InvalidOid);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(
			test_create_tablespace_storage_populates_the_pending_tablespace_deletes_list),
		unit_test(
			test_get_pending_tablespace_for_deletion_returns_a_null_value_by_default),
		unit_test(
			test_DoPendingTablespaceDeletionForAbort_removes_the_pending_tablespace_for_deletion_so_it_is_not_deleted_by_the_next_transaction
		),
		unit_test(
			test_DoPendingTablespaceDeletionForAbort_calls_unlink
		),
		unit_test(
			test_delete_called_when_invalid_tablespace_set_does_not_call_unlink
			),
		unit_test(
			test_DoTablespaceDeletionForRedoXlog_calls_unlink_with_tablespace_oid_and_redo_flag
			),
		unit_test(
			test_UnscheduleTablespaceDirectoryDeletionForAbort_removes_the_scheduled_tablespace_for_deletion
			),
		unit_test(
			test_an_UnscheduleTablespaceDirectoryDeletionForAbort_does_not_get_unlinked
			),
		unit_test(
			test_a_tablespace_can_be_scheduled_for_deletion_on_commit
			),
		unit_test(
			test_a_tablespace_can_be_unscheduled_for_deletion_on_commit
			),
		unit_test(
			test_a_tablespace_that_is_pending_is_deleted_on_commit
			),
		unit_test(
			test_a_tablespace_that_has_been_deleted_on_commit_is_no_longer_pending
			)
			
	};

	return run_tests(tests);
}
