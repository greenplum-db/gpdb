#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../autovacuum.c"

void
test__gp_database_needs_vacuum_when_not_connectable(void **state)
{
    bool needs_autovacuum;
    Form_pg_database dbform = palloc0(sizeof(Form_pg_database));
    TransactionId currentTransactionId = 10;
    dbform->datallowconn = false;
	dbform->datfrozenxid = 4;


	autovacuum_freeze_max_age = 2;

    needs_autovacuum = GpDatabaseNeedsAutovacuum(dbform, currentTransactionId);
	assert_true(needs_autovacuum);
}

void
test__gp_database_does_not_need_vacuum_when_not_connectable(void **state)
{
	bool needs_autovacuum;
	Form_pg_database dbform = palloc0(sizeof(Form_pg_database));
	TransactionId currentTransactionId = 5;
	dbform->datallowconn = false;
	dbform->datfrozenxid = 4;

	autovacuum_freeze_max_age = 2;

	needs_autovacuum = GpDatabaseNeedsAutovacuum(dbform, currentTransactionId);
	assert_false(needs_autovacuum);
}

void
test__gp_database_does_not_need_vacuum_when_connectable(void **state)
{
    bool needs_autovacuum;
	Form_pg_database dbform = palloc0(sizeof(Form_pg_database));
	TransactionId currentTransactionId;
	dbform->datallowconn = true;
	dbform->datfrozenxid = 4;

	autovacuum_freeze_max_age = 2;

	currentTransactionId = 5;
	needs_autovacuum = GpDatabaseNeedsAutovacuum(dbform, currentTransactionId);
	assert_false(needs_autovacuum);

	currentTransactionId = 10;
	needs_autovacuum = GpDatabaseNeedsAutovacuum(dbform, currentTransactionId);
	assert_false(needs_autovacuum);
}

void
test__gp_database_needs_vacuum_when_connectable_and_only_current_xid_has_wraped_around(void **state)
{
	bool needs_autovacuum;
	Form_pg_database dbform = palloc0(sizeof(Form_pg_database));
	dbform->datallowconn = false;
	dbform->datfrozenxid = MaxTransactionId;

	autovacuum_freeze_max_age = 2;

	needs_autovacuum = GpDatabaseNeedsAutovacuum(dbform, MaxTransactionId + 10);
	assert_true(needs_autovacuum);
}

void 
test__gp_autovacuum_is_enabled_for_non_connectable_databases(void **state) {
	Form_pg_database dbform = palloc0(sizeof(Form_pg_database));
	dbform->datallowconn = false;

	assert_true(GpAutovacuumEnabled(dbform));
}

void 
test__gp_autovacuum_is_not_enabled_for_connectable_databases(void **state) {
	Form_pg_database dbform = palloc0(sizeof(Form_pg_database));
	dbform->datallowconn = true;

	assert_false(GpAutovacuumEnabled(dbform));
}

int
main(int argc, char* argv[]) 
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
			unit_test(test__gp_database_needs_vacuum_when_not_connectable),
			unit_test(test__gp_database_does_not_need_vacuum_when_not_connectable),
			unit_test(test__gp_database_does_not_need_vacuum_when_connectable),
			unit_test(test__gp_database_needs_vacuum_when_connectable_and_only_current_xid_has_wraped_around),
			unit_test(test__gp_autovacuum_is_enabled_for_non_connectable_databases),
			unit_test(test__gp_autovacuum_is_not_enabled_for_connectable_databases)
	};

	MemoryContextInit();

	return run_tests(tests);
}
