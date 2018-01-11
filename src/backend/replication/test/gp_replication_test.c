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

void
test_SetSyncStandbysDefined(void **state)
{
	WalSndCtlData data;
	WalSndCtl = &data;
	data.sync_standbys_defined = false;

	expect_lwlock(LW_EXCLUSIVE);
#ifdef USE_ASSERT_CHECKING
	expect_value(LWLockHeldByMe, lockid, SyncRepLock);
	will_return(LWLockHeldByMe, true);
#endif

	/*
	 * set_gp_replication_config() should only be called once when mirror first
	 * comes up to set synchronous wal rep state
	 */
	expect_string_count(set_gp_replication_config, name, "synchronous_standby_names", 1);
	expect_string_count(set_gp_replication_config, value, "*", 1);
	will_be_called(set_gp_replication_config);

	/* simulate first call when mirror first comes up */
	assert_false(WalSndCtl->sync_standbys_defined);
	assert_true(SyncRepStandbyNames == NULL);
	SetSyncStandbysDefined();

	/* relative variables should have updated */
	assert_true(WalSndCtl->sync_standbys_defined);
	assert_true(strcmp(SyncRepStandbyNames, "*") == 0);

	expect_lwlock(LW_EXCLUSIVE);

	/* simulate second call after sync state is set which should do nothing */
	SetSyncStandbysDefined();
}

void
test_UnsetSyncStandbysDefined(void **state)
{
	int shmqueuenext_calls;
	WalSndCtlData data;
	WalSndCtl = &data;
	data.sync_standbys_defined = true;

	/* mock SHMQueueNext to skip the SyncRepWakeQueue */
#ifdef USE_ASSERT_CHECKING
	shmqueuenext_calls = 4;
#else
	shmqueuenext_calls = 2;
#endif
	expect_any_count(SHMQueueNext, queue, shmqueuenext_calls);
	expect_any_count(SHMQueueNext, curElem, shmqueuenext_calls);
	expect_any_count(SHMQueueNext, linkOffset, shmqueuenext_calls);
	will_return_count(SHMQueueNext, NULL, shmqueuenext_calls);

	expect_lwlock(LW_EXCLUSIVE);
#ifdef USE_ASSERT_CHECKING
	expect_value(LWLockHeldByMe, lockid, SyncRepLock);
	will_return(LWLockHeldByMe, true);
#endif

	/* unset the GUC in-memory */
	expect_string_count(set_gp_replication_config, name, "synchronous_standby_names", 1);
	expect_string_count(set_gp_replication_config, value, "", 1);
	will_be_called(set_gp_replication_config);

	/* simulate call when primary is in synchronous replication */
	assert_true(WalSndCtl->sync_standbys_defined);
	assert_true(strcmp(SyncRepStandbyNames, "*") == 0);

	/* call the function we are testing */
	UnsetSyncStandbysDefined();

	/* relative variables should have updated */
	assert_false(WalSndCtl->sync_standbys_defined);
	assert_true(strcmp(SyncRepStandbyNames, "") == 0);
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
		unit_test(test_GetMirrorStatus_WALSNDSTATE_STREAMING),
		unit_test(test_SetSyncStandbysDefined),
		unit_test(test_UnsetSyncStandbysDefined)
	};
	return run_tests(tests);
}
