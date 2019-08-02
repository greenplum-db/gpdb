#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "storage/pg_shmem.h"

/* Actual function body */
#include "../gp_replication.c"

static void
expect_lwlock(LWLockMode lockmode)
{
	expect_value(LWLockAcquire, l, SyncRepLock);
	expect_value(LWLockAcquire, mode, lockmode);
	will_return(LWLockAcquire, true);

	expect_value(LWLockRelease, l, SyncRepLock);
	will_be_called(LWLockRelease);
}

static void
expect_ereport()
{
	expect_any(errstart, elevel);
	expect_any(errstart, filename);
	expect_any(errstart, lineno);
	expect_any(errstart, funcname);
	expect_any(errstart, domain);

	will_be_called(errstart);
}

static void
test_setup(WalSndCtlData *data, int pid, WalSndState state)
{
	max_wal_senders = 1;
	WalSndCtl = data;

	data->walsnds[0].pid = pid;
	data->walsnds[0].state = state;
	data->walsnds[0].is_for_gp_walreceiver = true;
	SpinLockInit(&data->walsnds[0].mutex);

	expect_lwlock(LW_SHARED);
}

static void
test_GetMirrorStatus_Pid_Zero(void **state)
{
	FtsResponse response = { .IsMirrorUp = false };
	WalSndCtlData data = { .walsnds[0].pid = 0 };

	test_setup(&data, 0, WALSNDSTATE_STARTUP);

	/*
	 * This would make sure Mirror is reported as DOWN, as grace period
	 * duration is taken into account.
	 */
	data.walsnds[0].replica_disconnected_at =
		((pg_time_t) time(NULL)) - gp_fts_mark_mirror_down_grace_period;

	/*
	 * Ensure the recovery finished before wal sender died.
	 */
	PMAcceptingConnectionsStartTime = data.walsnds[0].replica_disconnected_at - 1;

	GetMirrorStatus(&response);

	assert_false(response.RequestRetry);
	assert_false(response.IsMirrorUp);
	assert_false(response.IsInSync);
}

static void
test_GetMirrorStatus_RequestRetry(void **state)
{
	FtsResponse response = { .IsMirrorUp = false };
	WalSndCtlData data = { .walsnds[0].pid = 0 };

	test_setup(&data, 0, WALSNDSTATE_STARTUP);

	/*
	 * Make the pid zero time within the grace period.
	 */
	data.walsnds[0].replica_disconnected_at =
		((pg_time_t) time(NULL)) - gp_fts_mark_mirror_down_grace_period/2;

	/*
	 * Ensure recovery finished before wal sender died.
	 */
	PMAcceptingConnectionsStartTime = data.walsnds[0].replica_disconnected_at - gp_fts_mark_mirror_down_grace_period;

	expect_ereport();
	GetMirrorStatus(&response);

	assert_true(response.RequestRetry);
}

/*
 * Verify the logic the grace period will exclude the recovery time.
 */
static void
test_GetMirrorStatus_Delayed_AcceptingConnectionsStartTime(void **state)
{
	FtsResponse response = { .IsMirrorUp = false };
	WalSndCtlData data = { .walsnds[0].pid = 0 };

	test_setup(&data, 0, WALSNDSTATE_STARTUP);

	/*
	 * wal sender pid zero time over the grace period
	 * Mirror will be marked down, and no retry.
	 */
	data.walsnds[0].replica_disconnected_at =
		((pg_time_t) time(NULL)) - gp_fts_mark_mirror_down_grace_period;

	/*
	 * However the database was in recovery, hence
	 * we are still within the grace period, and
	 * we should retry.
	 */
	PMAcceptingConnectionsStartTime = ((pg_time_t) time(NULL)) - gp_fts_mark_mirror_down_grace_period/2;

	expect_ereport();
	GetMirrorStatus(&response);

	assert_true(response.RequestRetry);
}

static void
test_GetMirrorStatus_Overflow(void **state)
{
	FtsResponse response = { .IsMirrorUp = false };
	WalSndCtlData data = { .walsnds[0].pid = 0 };

	test_setup(&data, 0, WALSNDSTATE_STARTUP);

	/*
	 * This would make sure Mirror is reported as DOWN, as grace period
	 * duration is taken into account.
	 */
	data.walsnds[0].replica_disconnected_at = INT64_MAX;
	PMAcceptingConnectionsStartTime = ((pg_time_t) time(NULL));

	GetMirrorStatus(&response);

	assert_false(response.RequestRetry);
	assert_false(response.IsMirrorUp);
	assert_false(response.IsInSync);
}

static void
test_GetMirrorStatus_WALSNDSTATE_STARTUP(void **state)
{
	FtsResponse response = { .IsMirrorUp = false };
	WalSndCtlData data = { .walsnds[0].pid = 0 };

	test_setup(&data, 1, WALSNDSTATE_STARTUP);

	/*
	 * This would make sure Mirror is not reported as DOWN, as still in grace
	 * period.
	 */
	data.walsnds[0].replica_disconnected_at = ((pg_time_t) time(NULL));
	PMAcceptingConnectionsStartTime = data.walsnds[0].replica_disconnected_at;

	expect_ereport();
	GetMirrorStatus(&response);

	assert_true(response.RequestRetry);
	assert_false(response.IsMirrorUp);
	assert_false(response.IsInSync);
}

static void
test_GetMirrorStatus_WALSNDSTATE_BACKUP(void **state)
{
	FtsResponse response = { .IsMirrorUp = false };
	WalSndCtlData data = { .walsnds[0].pid = 0 };

	test_setup(&data, 1, WALSNDSTATE_BACKUP);

	/*
	 * This would make sure Mirror is reported as DOWN, as grace period
	 * duration is taken into account.
	 */
	data.walsnds[0].replica_disconnected_at =
		((pg_time_t) time(NULL)) - gp_fts_mark_mirror_down_grace_period;

	/*
	 * Ensure the recovery finished before wal sender died.
	 */
	PMAcceptingConnectionsStartTime = data.walsnds[0].replica_disconnected_at - 1;

	GetMirrorStatus(&response);

	assert_false(response.RequestRetry);
	assert_false(response.IsMirrorUp);
	assert_false(response.IsInSync);
}

static void
test_GetMirrorStatus_WALSNDSTATE_CATCHUP(void **state)
{
	FtsResponse response = { .IsMirrorUp = false };
	WalSndCtlData data = { .walsnds[0].pid = 0 };

	test_setup(&data, 1, WALSNDSTATE_CATCHUP);

	GetMirrorStatus(&response);

	assert_true(response.IsMirrorUp);
	assert_false(response.IsInSync);
}

static void
test_GetMirrorStatus_WALSNDSTATE_STREAMING(void **state)
{
	FtsResponse response = { .IsMirrorUp = false };
	WalSndCtlData data = { .walsnds[0].pid = 0 };

	test_setup(&data, 1, WALSNDSTATE_STREAMING);

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
		unit_test(test_GetMirrorStatus_RequestRetry),
		unit_test(test_GetMirrorStatus_Delayed_AcceptingConnectionsStartTime),
		unit_test(test_GetMirrorStatus_Overflow),
		unit_test(test_GetMirrorStatus_WALSNDSTATE_STARTUP),
		unit_test(test_GetMirrorStatus_WALSNDSTATE_BACKUP),
		unit_test(test_GetMirrorStatus_WALSNDSTATE_CATCHUP),
		unit_test(test_GetMirrorStatus_WALSNDSTATE_STREAMING)
	};
	return run_tests(tests);
}
