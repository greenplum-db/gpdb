#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "utils/memutils.h"
#include "utils/palloc.h"

#include "catalog/storage_tablespace.h"
#include <stdio.h>


static Oid unlink_called_with_tablespace_oid;
static bool unlink_called_with_redo;
static Oid NOT_CALLED_OID = -1000;


static void unlink_tablespace_directory(Oid tablespaceOid, bool isRedo) {
	unlink_called_with_tablespace_oid = tablespaceOid;
	unlink_called_with_redo = isRedo;
}


void
setup()
{
	unlink_called_with_redo = false;
	unlink_called_with_tablespace_oid = NOT_CALLED_OID;
	TablespaceStorageInit(unlink_tablespace_directory);
}


void
test__create_tablespace_storage_populates_the_pending_tablespace_deletes_list(
	void **state)
{
	setup();

	Oid someTablespaceOid = 17999;

	TablespaceCreateStorage(someTablespaceOid);

	Oid tablespaceForDeletion = smgrGetPendingTablespaceForDeletion();

	assert_int_equal(17999, tablespaceForDeletion);

	setup();

	someTablespaceOid = 88888;

	TablespaceCreateStorage(someTablespaceOid);

	tablespaceForDeletion = smgrGetPendingTablespaceForDeletion();

	assert_int_equal(88888, tablespaceForDeletion);
}

void
test__get_pending_tablespace_for_deletion_returns_a_null_value_by_default(void **state)
{
	setup();

	Oid tablespaceForDeletion = smgrGetPendingTablespaceForDeletion();

	assert_int_equal(InvalidOid, tablespaceForDeletion);
}

void
test__smgrDoPendingTablespaceDeletion_removes_the_pending_tablespace_for_deletion_so_it_is_not_deleted_by_the_next_transaction(
	void **state)
{
	setup();

	Oid someTablespaceOid = 99999;

	TablespaceCreateStorage(someTablespaceOid);

	smgrDoPendingTablespaceDeletion();

	Oid tablespaceForDeletion = smgrGetPendingTablespaceForDeletion();

	assert_int_equal(InvalidOid, tablespaceForDeletion);
}

void
test__smgrDoPendingTablespaceDeletion_calls_unlink(void **state)
{
	setup();

	TablespaceCreateStorage(99999);

	smgrDoPendingTablespaceDeletion();

	assert_int_equal(unlink_called_with_tablespace_oid, 99999);
	assert_int_equal(unlink_called_with_redo, false);
}

void
test__delete_called_when_invalid_tablespace_set_does_not_call_unlink(void **state) 
{
	setup();

	TablespaceCreateStorage(InvalidOid);

	smgrDoPendingTablespaceDeletion();

	assert_int_equal(unlink_called_with_tablespace_oid, NOT_CALLED_OID);
}

void
test__smgrDoTablespaceDeletion_calls_unlink_with_tablespace_oid_and_redo_flag(void **state) {
	setup();

	TablespaceCreateStorage(66666);

	smgrDoTablespaceDeletion(77777);

	assert_int_equal(unlink_called_with_tablespace_oid, 77777);
	assert_int_equal(unlink_called_with_redo, true);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(
			test__create_tablespace_storage_populates_the_pending_tablespace_deletes_list),
		unit_test(
			test__get_pending_tablespace_for_deletion_returns_a_null_value_by_default),
		unit_test(
			test__smgrDoPendingTablespaceDeletion_removes_the_pending_tablespace_for_deletion_so_it_is_not_deleted_by_the_next_transaction
		),
		unit_test(
			test__smgrDoPendingTablespaceDeletion_calls_unlink
		),
		unit_test(
			test__delete_called_when_invalid_tablespace_set_does_not_call_unlink
			),
		unit_test(
			test__smgrDoTablespaceDeletion_calls_unlink_with_tablespace_oid_and_redo_flag
			)
	};

	return run_tests(tests);
}
