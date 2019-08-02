#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../redzone_handler.c"

#define EXPECT_EREPORT(LOG_LEVEL)     \
	expect_any(errstart, elevel); \
	expect_any(errstart, filename); \
	expect_any(errstart, lineno); \
	expect_any(errstart, funcname); \
	expect_any(errstart, domain); \
	if (LOG_LEVEL < ERROR) \
	{ \
    	will_return(errstart, false); \
	} \
    else \
    { \
    	will_return_with_sideeffect(errstart, false, &_ExceptionalCondition, NULL);\
    } \

/* isRunawayDetector assumes the address of this variable */
static uint32 fakeIsRunawayDetector = 0;
extern bool sessionStateInited;

#undef PG_RE_THROW
#define PG_RE_THROW() siglongjmp(*PG_exception_stack, 1)

/*
 * This method will emulate the real ExceptionalCondition
 * function by re-throwing the exception, essentially falling
 * back to the next available PG_CATCH();
 */
static void
_ExceptionalCondition()
{
     PG_RE_THROW();
}

/* Creates a SessionStateArray of the specified number of entry */
static void
CreateSessionStateArray(int numEntries)
{
	MaxBackends = numEntries;

	IsUnderPostmaster = false;

	assert_true(NULL == AllSessionStateEntries);

	SessionStateArray *fakeSessionStateArray = NULL;
	fakeSessionStateArray = malloc(SessionState_ShmemSize());

	will_return(ShmemInitStruct, fakeSessionStateArray);
	will_assign_value(ShmemInitStruct, foundPtr, false);

	expect_any_count(ShmemInitStruct, name, 1);
	expect_any_count(ShmemInitStruct, size, 1);
	expect_any_count(ShmemInitStruct, foundPtr, 1);

	SessionState_ShmemInit();

	/* The lookup should always work, whether under postmaster or not */
	assert_true(AllSessionStateEntries == fakeSessionStateArray);
}

/* Frees a previously created SessionStateArray */
static void
DestroySessionStateArray()
{
	assert_true(NULL != AllSessionStateEntries);
	free((void *) AllSessionStateEntries);
	AllSessionStateEntries = NULL;
}

/*
 * Acquires a SessionState entry for the specified sessionid. If an existing entry
 * is found, this method reuses that entry
 */
static SessionState *
AcquireSessionState(int sessionId, int vmem, int activeProcessCount)
{
	will_be_called_count(LWLockAcquire, 1);
	will_be_called_count(LWLockRelease, 1);
	expect_any_count(LWLockAcquire, l, 1);
	expect_any_count(LWLockAcquire, mode, 1);
	expect_any_count(LWLockRelease, l, 1);

	/* Keep the assertions happy */
	gp_session_id = sessionId;
	sessionStateInited = false;
	MySessionState = NULL;

	EXPECT_EREPORT(gp_sessionstate_loglevel);
	SessionState_Init();

	if (vmem >= 0)
	{
		MySessionState->sessionVmem = vmem;
	}

	if (activeProcessCount >= 0)
	{
		MySessionState->activeProcessCount = activeProcessCount;
	}

	return (SessionState *) MySessionState;
}
/*
 * Checks if RedZoneHandler_ShmemInit() properly initializes the global variables
 * as the postmaster
 */
static void
test__RedZoneHandler_ShmemInit__InitializesGlobalVarsWhenPostmaster(void **state)
{
	vmemTrackerInited = false;
	IsUnderPostmaster = false;

	/* Assign weird value to test the re-initialization */
	fakeIsRunawayDetector = 1234;
	isRunawayDetector = NULL;

	expect_any_count(ShmemInitStruct, name, 2);
	expect_any_count(ShmemInitStruct, size, 2);
	expect_any_count(ShmemInitStruct, foundPtr, 2);
	will_assign_value(ShmemInitStruct, foundPtr, false);
	will_assign_value(ShmemInitStruct, foundPtr, false);
	will_return_count(ShmemInitStruct, &fakeIsRunawayDetector, 2);

	/*
	 * When vmem limit is not activated or runaway_detector_activation_percent is
	 * set to 0,, red zone should be very high (i.e., red-zone will be disabled).
	 * Note, it doesn't matter what runaway_detector_activation_percent is set for
	 * this test, as the VmemTracker_ConvertVmemMBToChunks is returning 0.
	 */
	will_return(VmemTracker_ConvertVmemMBToChunks, 0);
	expect_any(VmemTracker_ConvertVmemMBToChunks, mb);

	RedZoneHandler_ShmemInit();

	assert_true(isRunawayDetector == &fakeIsRunawayDetector);
	assert_true(redZoneChunks == INT32_MAX);
	assert_true(*isRunawayDetector == 0);

	/*
	 * When the activation percent is set to 100, we will not even attempt calculating
	 * the redZoneChunks and instead assign INT32_MAX directly. Note, we don't even
	 * call VmemTracker_ConvertVmemMBToChunks()
	 */
	runaway_detector_activation_percent = 100;
	redZoneChunks = 0;
	RedZoneHandler_ShmemInit();
	assert_true(redZoneChunks == INT32_MAX);
}

/*
 * Checks if RedZoneHandler_ShmemInit() properly initializes the global variables
 * when under postmaster
 */
static void
test__RedZoneHandler_ShmemInit__InitializesUnderPostmaster(void **state)
{
	vmemTrackerInited = false;
	IsUnderPostmaster = true;

	/* Assign weird value to test the re-initialization */
	fakeIsRunawayDetector = 1234;
	isRunawayDetector = NULL;

	expect_any(ShmemInitStruct, name);
	expect_any(ShmemInitStruct, size);
	expect_any(ShmemInitStruct, foundPtr);
	will_assign_value(ShmemInitStruct, foundPtr, true);
	will_return(ShmemInitStruct, &fakeIsRunawayDetector);

	/* For testing that we don't change this value */
	redZoneChunks = 1234;
	RedZoneHandler_ShmemInit();

	assert_true(isRunawayDetector == &fakeIsRunawayDetector);
	assert_true(redZoneChunks == 1234);
	assert_true(*isRunawayDetector == 1234);
}

/*
 * Checks if RedZoneHandler_IsVmemRedZone() properly identifies red zone
 */
static void
test__RedZoneHandler_IsVmemRedZone__ProperlyIdentifiesRedZone(void **state)
{
	vmemTrackerInited = false;

	/* No red zone detection if vmem tracker is not initialized */
	assert_false(RedZoneHandler_IsVmemRedZone());

	vmemTrackerInited = true;

	static int32 fakeSegmentVmemChunks = 0;
	segmentVmemChunks = &fakeSegmentVmemChunks;

	redZoneChunks = INT32_MAX;
	*segmentVmemChunks = INT32_MAX;
	/* Both segment vmem and red zone is INT32_MAX. It's not a red-zone */
	assert_false(RedZoneHandler_IsVmemRedZone());

	/* 100 chunks */
	*segmentVmemChunks = 100;
	redZoneChunks = 80;
	/* segmentVmemChunks exceeds redZoneChunks. So, should be red zone */
	assert_true(RedZoneHandler_IsVmemRedZone());

	vmemTrackerInited = false;
	/*
	 * segmentVmemChunks exceeds redZoneChunks. But vmem tracker is not
	 * initialized. Therefore, no red zone detection
	 */
	assert_false(RedZoneHandler_IsVmemRedZone());
}

/*
 * Checks if RedZoneHandler_FlagTopConsumer() allows only one detector
 * at a time
 */
static void
test__RedZoneHandler_FlagTopConsumer__SingletonDetector(void **state)
{
	/* Make sure the code is exercised */
	vmemTrackerInited = true;

	/* Ensure non-null MySessionState */
	MySessionState = (SessionState *) 0x1234;

	static uint32 fakeIsRunawayDetector = 0;
	isRunawayDetector = &fakeIsRunawayDetector;

	/* We already have a runaway detector */
	*isRunawayDetector = 1;

	/*
	 * This will return without attempting to detecting any runaway session.
	 * This is tested from the fact that it is not trying to call LWLocAcquire
	 */
	RedZoneHandler_FlagTopConsumer();
}

/*
 * Checks if RedZoneHandler_FlagTopConsumer() finds the top consumer
 */
static void
test__RedZoneHandler_FlagTopConsumer__FindsTopConsumer(void **state)
{
	/* Make sure the RedZoneHandler_FlagTopConsumer code is exercised */
	vmemTrackerInited = true;

	CreateSessionStateArray(4);

	/* Make sure MySessionState is valid */
	SessionState *one = AcquireSessionState(1 /* sessionId */, 100 /* vmem */, 1 /* activeProcessCount */);
	SessionState *two = AcquireSessionState(2, 101, 1);
	SessionState *three = AcquireSessionState(3, 101, 1);
	SessionState *four = AcquireSessionState(4, 99, 1);

	/* Ensure we can detect runaway sessions */
	*isRunawayDetector = 0;

	will_be_called_count(LWLockAcquire, 1);
	will_be_called_count(LWLockRelease, 1);
	expect_any_count(LWLockAcquire, l, 1);
	expect_any_count(LWLockAcquire, mode, 1);
	expect_any_count(LWLockRelease, l, 1);

	static EventVersion fakeLatestRunawayVersion = 0;
	static EventVersion fakeCurrentVersion = 1;
	latestRunawayVersion = &fakeLatestRunawayVersion;
	CurrentVersion = &fakeCurrentVersion;

	RedZoneHandler_FlagTopConsumer();

	assert_true(one->runawayStatus == RunawayStatus_NotRunaway &&
			two->runawayStatus  == RunawayStatus_NotRunaway /* three is tied with two. So, won't be flagged */ &&
			three->runawayStatus  == RunawayStatus_PrimaryRunawaySession /* First detected max consumer is
			flagged (note the usedList is reversed, so "three" will be ahead of "two") */ &&
			four->runawayStatus == RunawayStatus_NotRunaway);

	DestroySessionStateArray();
}

/*
 * Checks if RedZoneHandler_FlagTopConsumer() ignores the idle sessions
 * even if they are the top consumer
 */
static void
test__RedZoneHandler_FlagTopConsumer__IgnoresIdleSession(void **state)
{
	/* Make sure the RedZoneHandler_FlagTopConsumer code is exercised */
	vmemTrackerInited = true;

	CreateSessionStateArray(4);

	/* Make sure MySessionState is valid */
	SessionState *one = AcquireSessionState(1 /* sessionId */, 100 /* vmem */, 1 /* activeProcessCount */);
	SessionState *two = AcquireSessionState(2, 101, 0);
	SessionState *three = AcquireSessionState(3, 100, 0);
	SessionState *four = AcquireSessionState(4, 99, 1);

	/* Ensure we can detect runaway sessions */
	*isRunawayDetector = 0;

	will_be_called_count(LWLockAcquire, 1);
	will_be_called_count(LWLockRelease, 1);
	expect_any_count(LWLockAcquire, l, 1);
	expect_any_count(LWLockAcquire, mode, 1);
	expect_any_count(LWLockRelease, l, 1);

	static EventVersion fakeLatestRunawayVersion = 0;
	static EventVersion fakeCurrentVersion = 1;
	latestRunawayVersion = &fakeLatestRunawayVersion;
	CurrentVersion = &fakeCurrentVersion;

	RedZoneHandler_FlagTopConsumer();

	assert_true(one->runawayStatus  == RunawayStatus_SecondaryRunawaySession &&
			two->runawayStatus == RunawayStatus_NotRunaway &&
			three->runawayStatus  == RunawayStatus_NotRunaway /* We will encounter three first, but it
				doesn't have active process. So, RDT will ignore it. */ &&
			four->runawayStatus == RunawayStatus_NotRunaway);

	DestroySessionStateArray();
}

/*
 * Checks if RedZoneHandler_FlagTopConsumer() reactivates the runaway detector
 * if there is no active session
 */
static void
test__RedZoneHandler_FlagTopConsumer__ReactivatesDetectorIfNoActiveSession(void **state)
{
	/* Make sure the RedZoneHandler_FlagTopConsumer code is exercised */
	vmemTrackerInited = true;

	CreateSessionStateArray(4);

	/* Make sure MySessionState is valid */
	SessionState *one = AcquireSessionState(1 /* sessionId */, 100 /* vmem */, 0 /* activeProcessCount */);
	SessionState *two = AcquireSessionState(2, 101, 0);
	SessionState *three = AcquireSessionState(3, 100, 0);
	SessionState *four = AcquireSessionState(4, 99, 0);

	/* Ensure we can detect runaway sessions */
	*isRunawayDetector = 0;

	will_be_called_count(LWLockAcquire, 1);
	will_be_called_count(LWLockRelease, 1);
	expect_any_count(LWLockAcquire, l, 1);
	expect_any_count(LWLockAcquire, mode, 1);
	expect_any_count(LWLockRelease, l, 1);

	static EventVersion fakeLatestRunawayVersion = 0;
	static EventVersion fakeCurrentVersion = 1;
	latestRunawayVersion = &fakeLatestRunawayVersion;
	CurrentVersion = &fakeCurrentVersion;

	RedZoneHandler_FlagTopConsumer();

	/* None of them could be detected as runaway as all of them are inactive sessions */
	assert_true(one->runawayStatus == RunawayStatus_NotRunaway && two->runawayStatus == RunawayStatus_NotRunaway &&
			three->runawayStatus == RunawayStatus_NotRunaway && four->runawayStatus == RunawayStatus_NotRunaway);

	assert_true(*isRunawayDetector == 0);

	DestroySessionStateArray();
}

/*
 * Checks if RedZoneHandler_FlagTopConsumer() updates the CurrentVersion and
 * latestRunawayVersion
 */
static void
test__RedZoneHandler_FlagTopConsumer__UpdatesEventVersions(void **state)
{
	/* Make sure the RedZoneHandler_FlagTopConsumer code is exercised */
	vmemTrackerInited = true;

	CreateSessionStateArray(1);

	/* Make sure MySessionState is valid */
	SessionState *one = AcquireSessionState(1 /* sessionId */, 100 /* vmem */, 1 /* activeProcessCount */);

	/* Ensure we can detect runaway sessions */
	*isRunawayDetector = 0;

	will_be_called_count(LWLockAcquire, 1);
	will_be_called_count(LWLockRelease, 1);
	expect_any_count(LWLockAcquire, l, 1);
	expect_any_count(LWLockAcquire, mode, 1);
	expect_any_count(LWLockRelease, l, 1);

	static EventVersion fakeLatestRunawayVersion = 0;
	static EventVersion fakeCurrentVersion = 1;
	latestRunawayVersion = &fakeLatestRunawayVersion;
	CurrentVersion = &fakeCurrentVersion;

	RedZoneHandler_FlagTopConsumer();

	assert_true(one->runawayStatus == RunawayStatus_PrimaryRunawaySession);
	/* Verify that the event versions were properly updated */
	assert_true(*CurrentVersion == 3 && *latestRunawayVersion == 2);

	DestroySessionStateArray();
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__RedZoneHandler_ShmemInit__InitializesGlobalVarsWhenPostmaster),
		unit_test(test__RedZoneHandler_ShmemInit__InitializesUnderPostmaster),
		unit_test(test__RedZoneHandler_IsVmemRedZone__ProperlyIdentifiesRedZone),
		unit_test(test__RedZoneHandler_FlagTopConsumer__SingletonDetector),
		unit_test(test__RedZoneHandler_FlagTopConsumer__FindsTopConsumer),
		unit_test(test__RedZoneHandler_FlagTopConsumer__IgnoresIdleSession),
		unit_test(test__RedZoneHandler_FlagTopConsumer__ReactivatesDetectorIfNoActiveSession),
		unit_test(test__RedZoneHandler_FlagTopConsumer__UpdatesEventVersions),
	};

	return run_tests(tests);
}
