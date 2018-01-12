#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "storage/pg_shmem.h"

#define Assert(condition) if (!condition) AssertFailed()

bool is_assert_failed = false;

void AssertFailed()
{
	is_assert_failed = true;
}

/* Actual function body */
#include "../gp_replication.c"

static void
expect_lwlock(LWLockMode lockmode)
{
	expect_value(LWLockAcquire, lockid, SyncRepLock);
	expect_value(LWLockAcquire, mode, lockmode);
	will_be_called(LWLockAcquire);

	expect_value(LWLockRelease, lockid, SyncRepLock);
	will_be_called(LWLockRelease);
}

static void
test_setup(WalSndCtlData *data, WalSndState state)
{
	max_wal_senders = 1;
	WalSndCtl = data;
	data->walsnds[0].pid = 1;
	data->walsnds[0].state = state;

	expect_lwlock(LW_SHARED);
}

void
test_GetMirrorStatus_Pid_Zero(void **state)
{
	FtsResponse response;
	WalSndCtlData data;

	max_wal_senders = 1;
	WalSndCtl = &data;
	data.walsnds[0].pid = 0;

	expect_lwlock(LW_SHARED);
	GetMirrorStatus(&response);

	assert_false(response.IsMirrorUp);
	assert_false(response.IsInSync);
}

void
test_GetMirrorStatus_WALSNDSTATE_STARTUP(void **state)
{
	FtsResponse response;
	WalSndCtlData data;

	test_setup(&data, WALSNDSTATE_STARTUP);
	GetMirrorStatus(&response);

	assert_false(response.IsMirrorUp);
	assert_false(response.IsInSync);
}

void
test_GetMirrorStatus_WALSNDSTATE_BACKUP(void **state)
{
	FtsResponse response;
	WalSndCtlData data;

	test_setup(&data, WALSNDSTATE_BACKUP);
	GetMirrorStatus(&response);

	assert_false(response.IsMirrorUp);
	assert_false(response.IsInSync);
}

void
test_GetMirrorStatus_WALSNDSTATE_CATCHUP(void **state)
{
	FtsResponse response;
	WalSndCtlData data;

	test_setup(&data, WALSNDSTATE_CATCHUP);
	GetMirrorStatus(&response);

	assert_true(response.IsMirrorUp);
	assert_false(response.IsInSync);
}

void
test_GetMirrorStatus_WALSNDSTATE_STREAMING(void **state)
{
	FtsResponse response;
	WalSndCtlData data;

	test_setup(&data, WALSNDSTATE_STREAMING);
	GetMirrorStatus(&response);

	assert_true(response.IsMirrorUp);
	assert_true(response.IsInSync);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_GetMirrorStatus_Pid_Zero),
		unit_test(test_GetMirrorStatus_WALSNDSTATE_STARTUP),
		unit_test(test_GetMirrorStatus_WALSNDSTATE_BACKUP),
		unit_test(test_GetMirrorStatus_WALSNDSTATE_CATCHUP),
		unit_test(test_GetMirrorStatus_WALSNDSTATE_STREAMING)
	};
	return run_tests(tests);
}
