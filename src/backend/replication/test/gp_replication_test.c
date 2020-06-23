#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "storage/pg_shmem.h"

/* Actual function body */
#include "../gp_replication.c"

static void
expect_lwlock(LWLockMode lockmode, int count)
{
	expect_value(LWLockAcquire, lock, SyncRepLock);
	expect_value(LWLockAcquire, mode, lockmode);
	will_return(LWLockAcquire, true);

	expect_value(LWLockRelease, lock, SyncRepLock);
	will_be_called(LWLockRelease);

	if (count > 0)
	{
		expect_value(LWLockAcquire, lock, FTSReplicationStatusLock);
		expect_value(LWLockAcquire, mode, lockmode);
		will_return(LWLockAcquire, true);

#ifdef USE_ASSERT_CHECKING
		expect_value_count(LWLockHeldByMe, l, FTSReplicationStatusLock, count);
		will_return_count(LWLockHeldByMe, true, count);
#endif

		expect_value(LWLockRelease, lock, FTSReplicationStatusLock);
		will_be_called(LWLockRelease);
	}
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

static FTSReplicationStatusCtlData *
test_setup(int pid, WalSndState state, int count)
{
	max_wal_senders = 1;
	WalSndCtl = (WalSndCtlData *)malloc(WalSndShmemSize());
	MemSet(WalSndCtl, 0, WalSndShmemSize());

	WalSndCtl->walsnds[0].pid = pid;
	WalSndCtl->walsnds[0].state = state;
	WalSndCtl->walsnds[0].is_for_gp_walreceiver = true;
	SpinLockInit(&WalSndCtl->walsnds[0].mutex);

	FTSRepStatusCtl = (FTSReplicationStatusCtlData *)malloc(FTSReplicationStatusShmemSize());
	MemSet(FTSRepStatusCtl, 0, FTSReplicationStatusShmemSize());

	FTSRepStatusCtl->replications[0].in_use = true;
	StrNCpy(NameStr(FTSRepStatusCtl->replications[0].name), GP_WALRECEIVER_APPNAME, NAMEDATALEN);
	SpinLockInit(&FTSRepStatusCtl->replications[0].mutex);

	expect_lwlock(LW_SHARED, count);
	return FTSRepStatusCtl;
}

static void
test_GetMirrorStatus_Pid_Zero(void **state)
{
	FtsResponse response = { .IsMirrorUp = false };
	FTSReplicationStatusCtlData *data;
	data = test_setup(0, WALSNDSTATE_STARTUP, 3);

	/*
	 * This would make sure Mirror is reported as DOWN, as grace period
	 * duration is taken into account.
	 */
	data->replications[0].replica_disconnected_at =
		((pg_time_t) time(NULL)) - gp_fts_mark_mirror_down_grace_period;

	/*
	 * Ensure the recovery finished before wal sender died.
	 */
	PMAcceptingConnectionsStartTime = data->replications[0].replica_disconnected_at - 1;

	GetMirrorStatus(&response);

	assert_false(response.RequestRetry);
	assert_false(response.IsMirrorUp);
	assert_false(response.IsInSync);
}

static void
test_GetMirrorStatus_RequestRetry(void **state)
{
	FtsResponse response = { .IsMirrorUp = false };
	FTSReplicationStatusCtlData *data;

	data = test_setup(0, WALSNDSTATE_STARTUP, 3);

	/*
	 * Make the pid zero time within the grace period.
	 */
	data->replications[0].replica_disconnected_at =
		((pg_time_t) time(NULL)) - gp_fts_mark_mirror_down_grace_period/2;

	/*
	 * Ensure recovery finished before wal sender died.
	 */
	PMAcceptingConnectionsStartTime = data->replications[0].replica_disconnected_at - gp_fts_mark_mirror_down_grace_period;

	expect_ereport();
	GetMirrorStatus(&response);

	assert_true(response.RequestRetry);
}

/*
 * If the replication keeps crash and continuously retry more than
 * gp_fts_replication_attempt_count, FTS should not retry and mark
 * mirror down directly.
 */
static void
test_GetMirrorStatus_Exceed_ContinuouslyReplicationAttempt(void **state)
{
	FtsResponse response = { .IsMirrorUp = false };
	FTSReplicationStatusCtlData *data;

	data = test_setup(0, WALSNDSTATE_STARTUP, 2);

	/*
	 * Make the current replication attempt count exceeds the max attempt count.
	 */
	data->replications[0].con_attempt_count = gp_fts_replication_attempt_count + 1;

	/*
	 * Make the pid zero time within the grace period.
	 */
	data->replications[0].replica_disconnected_at =
		((pg_time_t) time(NULL)) - gp_fts_mark_mirror_down_grace_period/2;

	/*
	 * Ensure recovery finished before wal sender died.
	 */
	PMAcceptingConnectionsStartTime = data->replications[0].replica_disconnected_at - gp_fts_mark_mirror_down_grace_period;

	expect_ereport();
	GetMirrorStatus(&response);

	assert_false(response.RequestRetry);
}

/*
 * Verify the logic the grace period will exclude the recovery time.
 */
static void
test_GetMirrorStatus_Delayed_AcceptingConnectionsStartTime(void **state)
{
	FtsResponse response = { .IsMirrorUp = false };
	FTSReplicationStatusCtlData *data;

	data = test_setup(0, WALSNDSTATE_STARTUP, 3);

	/*
	 * wal sender pid zero time over the grace period
	 * Mirror will be marked down, and no retry.
	 */
	data->replications[0].replica_disconnected_at =
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
	FTSReplicationStatusCtlData *data;

	data = test_setup(0, WALSNDSTATE_STARTUP, 3);

	/*
	 * This would make sure Mirror is reported as DOWN, as grace period
	 * duration is taken into account.
	 */
	data->replications[0].replica_disconnected_at = INT64_MAX;
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
	FTSReplicationStatusCtlData *data;

	data = test_setup(1, WALSNDSTATE_STARTUP, 3);

	/*
	 * This would make sure Mirror is not reported as DOWN, as still in grace
	 * period.
	 */
	data->replications[0].replica_disconnected_at = ((pg_time_t) time(NULL));
	PMAcceptingConnectionsStartTime = data->replications[0].replica_disconnected_at;

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
	FTSReplicationStatusCtlData *data;

	data = test_setup(1, WALSNDSTATE_BACKUP, 3);

	/*
	 * This would make sure Mirror is reported as DOWN, as grace period
	 * duration is taken into account.
	 */
	data->replications[0].replica_disconnected_at =
		((pg_time_t) time(NULL)) - gp_fts_mark_mirror_down_grace_period;

	/*
	 * Ensure the recovery finished before wal sender died.
	 */
	PMAcceptingConnectionsStartTime = data->replications[0].replica_disconnected_at - 1;

	GetMirrorStatus(&response);

	assert_false(response.RequestRetry);
	assert_false(response.IsMirrorUp);
	assert_false(response.IsInSync);
}

static void
test_GetMirrorStatus_WALSNDSTATE_CATCHUP(void **state)
{
	FtsResponse response = { .IsMirrorUp = false };
	FTSReplicationStatusCtlData *data;

	data = test_setup(1, WALSNDSTATE_CATCHUP, 0);

	GetMirrorStatus(&response);

	assert_true(response.IsMirrorUp);
	assert_false(response.IsInSync);
}

static void
test_GetMirrorStatus_WALSNDSTATE_STREAMING(void **state)
{
	FtsResponse response = { .IsMirrorUp = false };
	FTSReplicationStatusCtlData *data;

	data = test_setup(1, WALSNDSTATE_STREAMING, 0);

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
		unit_test(test_GetMirrorStatus_Exceed_ContinuouslyReplicationAttempt),
		unit_test(test_GetMirrorStatus_Delayed_AcceptingConnectionsStartTime),
		unit_test(test_GetMirrorStatus_Overflow),
		unit_test(test_GetMirrorStatus_WALSNDSTATE_STARTUP),
		unit_test(test_GetMirrorStatus_WALSNDSTATE_BACKUP),
		unit_test(test_GetMirrorStatus_WALSNDSTATE_CATCHUP),
		unit_test(test_GetMirrorStatus_WALSNDSTATE_STREAMING)
	};
	return run_tests(tests);
}
