#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"

#define Assert(condition) if (!condition) AssertFailed()

bool is_assert_failed = false;

void AssertFailed()
{
	is_assert_failed = true;
}

/* Actual function body */
#include "../gp_replication.c"

void
test_IsMirrorUp_Assert(void **state)
{
	/*
	 * Test the Assert
	 */
	max_wal_senders = 0;
	IsMirrorUp();
	assert_true(is_assert_failed);
}

static void
test_setup(WalSndCtlData *data, WalSndState state)
{
	max_wal_senders = 1;
	WalSndCtl = data;
	data->walsnds[0].pid = 1;
	data->walsnds[0].state = state;
}

void
test_IsMirrorUp_Pid_Zero(void **state)
{
	max_wal_senders = 1;
	WalSndCtlData data;
	WalSndCtl = &data;
	data.walsnds[0].pid = 0;
	assert_false(IsMirrorUp());
}

void
test_IsMirrorUp_WALSNDSTATE_STARTUP(void **state)
{
	WalSndCtlData data;
	test_setup(&data, WALSNDSTATE_STARTUP);
	assert_false(IsMirrorUp());
}

void
test_IsMirrorUp_WALSNDSTATE_BACKUP(void **state)
{
	WalSndCtlData data;
	test_setup(&data, WALSNDSTATE_BACKUP);
	assert_false(IsMirrorUp());
}

void
test_IsMirrorUp_WALSNDSTATE_CATCHUP(void **state)
{
	WalSndCtlData data;
	test_setup(&data, WALSNDSTATE_CATCHUP);
	assert_true(IsMirrorUp());
}

void
test_IsMirrorUp_WALSNDSTATE_STREAMING(void **state)
{
	WalSndCtlData data;
	test_setup(&data, WALSNDSTATE_STREAMING);
	assert_true(IsMirrorUp());
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_IsMirrorUp_Assert),
		unit_test(test_IsMirrorUp_Pid_Zero),
		unit_test(test_IsMirrorUp_WALSNDSTATE_STARTUP),
		unit_test(test_IsMirrorUp_WALSNDSTATE_BACKUP),
		unit_test(test_IsMirrorUp_WALSNDSTATE_CATCHUP),
		unit_test(test_IsMirrorUp_WALSNDSTATE_STREAMING)
	};
	return run_tests(tests);
}
