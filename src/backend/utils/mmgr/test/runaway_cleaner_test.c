#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../runaway_cleaner.c"

#define EXPECT_ELOG(LOG_LEVEL)     \
    will_be_called(elog_start); \
	expect_any(elog_start, filename); \
	expect_any(elog_start, lineno); \
	expect_any(elog_start, funcname); \
	if (LOG_LEVEL < ERROR) \
	{ \
    	will_be_called(elog_finish); \
	} \
    else \
    { \
    	will_be_called_with_sideeffect(elog_finish, &_ExceptionalCondition, NULL);\
    } \
	expect_value(elog_finish, elevel, LOG_LEVEL); \
	expect_any(elog_finish, fmt); \

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

#define CHECK_FOR_RUNAWAY_CLEANUP_MEMORY_LOGGING() \
	will_be_called(write_stderr); \
	expect_any(write_stderr, fmt); \
	will_be_called(MemoryContextStats); \
	expect_any(MemoryContextStats, context); \

/* MySessionState will use the address of this global variable */
static SessionState fakeSessionState;

/* Prepares a fake MySessionState pointer for use in the vmem tracker */
static void
InitFakeSessionState(int activeProcessCount, int cleanupCountdown, RunawayStatus runawayStatus, int pinCount, int vmem)
{
	MySessionState = &fakeSessionState;

	MySessionState->activeProcessCount = activeProcessCount;
	MySessionState->cleanupCountdown = cleanupCountdown;
	MySessionState->runawayStatus = runawayStatus;
	MySessionState->next = NULL;
	MySessionState->pinCount = pinCount;
	MySessionState->sessionId = 1234;
	MySessionState->sessionVmem = vmem;
	MySessionState->spinLock = 0;
}

#undef  PG_RE_THROW
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

/*
 * Checks if RunawayCleaner_StartCleanup() does not start cleanup if
 * the current session is not a runaway
 */
static void
test__RunawayCleaner_StartCleanup__IgnoresNonRunaway(void **state)
{
	InitFakeSessionState(2 /* activeProcessCount */,
			CLEANUP_COUNTDOWN_BEFORE_RUNAWAY /* cleanupCountdown */,
			RunawayStatus_NotRunaway /* runawayStatus */, 2 /* pinCount */, 0 /* vmem */);

	EventVersion fakeLatestRunawayVersion = 10;
	latestRunawayVersion = &fakeLatestRunawayVersion;
	beginCleanupRunawayVersion = 0;

	RunawayCleaner_StartCleanup();

	/* Cleanup shouldn't have begun */
	assert_true(beginCleanupRunawayVersion != *latestRunawayVersion);
}

/*
 * Checks if RunawayCleaner_StartCleanup() does not execute a duplicate
 * cleanup for the same runaway event that it already started cleaning up
 */
static void
test__RunawayCleaner_StartCleanup__IgnoresDuplicateCleanup(void **state)
{
	InitFakeSessionState(2 /* activeProcessCount */,
			2 /* cleanupCountdown */,
			RunawayStatus_PrimaryRunawaySession /* runawayStatus */, 2 /* pinCount */, 0 /* vmem */);

	EventVersion fakeLatestRunawayVersion = 10;
	latestRunawayVersion = &fakeLatestRunawayVersion;
	beginCleanupRunawayVersion = *latestRunawayVersion;

	/*
	 * As we are not providing IsCommitInProgress, the call itself verifies
	 * that we are not attempting any cleanup
	 */
	RunawayCleaner_StartCleanup();
}

/*
 * Checks if RunawayCleaner_StartCleanup() starts the cleanup process if
 * all conditions are met (i.e., no commit is in progress and vmem tracker
 * is initialized) and runaway session is "primary"
 */
static void
test__RunawayCleaner_StartCleanup__StartsPrimaryCleanupIfPossible(void **state)
{
	InitFakeSessionState(2 /* activeProcessCount */,
			2 /* cleanupCountdown */,
			RunawayStatus_PrimaryRunawaySession /* runawayStatus */, 2 /* pinCount */, 12345 /* vmem */);

	EventVersion fakeLatestRunawayVersion = 10;
	latestRunawayVersion = &fakeLatestRunawayVersion;

	/*
	 * Set beginCleanupRunawayVersion to less than *latestRunawayVersion
	 * to trigger a cleanup
	 */
	beginCleanupRunawayVersion = 1;
	endCleanupRunawayVersion = 1;
	isProcessActive = true;

	/* Make sure the cleanup goes through */
	vmemTrackerInited = true;
	CritSectionCount = 0;
	InterruptHoldoffCount = 0;
	/* We need a valid gp_command_count to execute cleanup */
	gp_command_count = 1;
	will_return(IsTransactionState, true);

	EXPECT_EREPORT(ERROR);

	PG_TRY();
	{
		RunawayCleaner_StartCleanup();
		assert_false("Cleanup didn't throw error");
	}
	PG_CATCH();
	{

	}
	PG_END_TRY();

	assert_true(beginCleanupRunawayVersion == *latestRunawayVersion);
	/* We should not finish the cleanup as we errored out */
	assert_true(endCleanupRunawayVersion == 1);

	/* cleanupCountdown shouldn't change as we haven't finished cleanup */
	assert_true(MySessionState->cleanupCountdown == 2);

	/*
	 * If we call RunawayCleaner_StartCleanup again for the same runaway event,
	 * it should be a noop, therefore requiring no "will_be_called" setup
	 */
	RunawayCleaner_StartCleanup();
}

/*
 * Checks if RunawayCleaner_StartCleanup() starts the cleanup process if
 * all conditions are met (i.e., no commit is in progress and vmem tracker
 * is initialized) and runaway session is "secondary"
 */
static void
test__RunawayCleaner_StartCleanup__StartsSecondaryCleanupIfPossible(void **state)
{
	InitFakeSessionState(2 /* activeProcessCount */,
						 2 /* cleanupCountdown */,
						 RunawayStatus_SecondaryRunawaySession /* runawayStatus */, 2 /* pinCount */, 12345 /* vmem */);

	EventVersion fakeLatestRunawayVersion = 10;
	latestRunawayVersion = &fakeLatestRunawayVersion;

	/*
	 * Set beginCleanupRunawayVersion to less than *latestRunawayVersion
	 * to trigger a cleanup
	 */
	beginCleanupRunawayVersion = 1;
	endCleanupRunawayVersion = 1;
	isProcessActive = true;

	/* Make sure the cleanup goes through */
	vmemTrackerInited = true;
	CritSectionCount = 0;
	InterruptHoldoffCount = 0;
	/* We need a valid gp_command_count to execute cleanup */
	gp_command_count = 1;
	will_return(superuser, false);
	will_return(IsTransactionState, true);

	EXPECT_EREPORT(ERROR);

	PG_TRY();
	{
		RunawayCleaner_StartCleanup();
		assert_false("Cleanup didn't throw error");
	}
	PG_CATCH();
	{

	}
	PG_END_TRY();

	assert_true(beginCleanupRunawayVersion == *latestRunawayVersion);
	/* We should not finish the cleanup as we errored out */
	assert_true(endCleanupRunawayVersion == 1);

	/* cleanupCountdown shouldn't change as we haven't finished cleanup */
	assert_true(MySessionState->cleanupCountdown == 2);

	/*
	 * If we call RunawayCleaner_StartCleanup again for the same runaway event,
	 * it should be a noop, therefore requiring no "will_be_called" setup
	 */
	RunawayCleaner_StartCleanup();
}


/*
 * Checks if RunawayCleaner_StartCleanup() ignores cleanup if in critical section
 */
static void
test__RunawayCleaner_StartCleanup__IgnoresCleanupInCriticalSection(void **state)
{
	InitFakeSessionState(2 /* activeProcessCount */,
			2 /* cleanupCountdown */,
			RunawayStatus_PrimaryRunawaySession /* runawayStatus */, 2 /* pinCount */, 12345 /* vmem */);

	EventVersion fakeLatestRunawayVersion = 10;
	latestRunawayVersion = &fakeLatestRunawayVersion;

	/*
	 * Set beginCleanupRunawayVersion to less than *latestRunawayVersino
	 * to trigger a cleanup
	 */
	beginCleanupRunawayVersion = 1;
	endCleanupRunawayVersion = 1;

	/* Make sure the cleanup goes through */
	vmemTrackerInited = true;
	isProcessActive = true;

	CritSectionCount = 1;
	InterruptHoldoffCount = 0;

	CHECK_FOR_RUNAWAY_CLEANUP_MEMORY_LOGGING();
	RunawayCleaner_StartCleanup();

	assert_true(beginCleanupRunawayVersion == *latestRunawayVersion);
	/* Cleanup is done, without ever throwing an ERROR */
	assert_true(endCleanupRunawayVersion == beginCleanupRunawayVersion);

	/*
	 * cleanupCountdown is decremented by 1 as there was no error, and therefore
	 * the cleanup is done within the same call of RunawayCleaner_StartCleanup
	 */
	assert_true(MySessionState->cleanupCountdown == 1);

	CritSectionCount = 0;
}

/*
 * Checks if RunawayCleaner_StartCleanup() ignores cleanup if interrupts are held off
 */
static void
test__RunawayCleaner_StartCleanup__IgnoresCleanupInHoldoffInterrupt(void **state)
{
	InitFakeSessionState(2 /* activeProcessCount */,
			2 /* cleanupCountdown */,
			RunawayStatus_PrimaryRunawaySession /* runawayStatus */, 2 /* pinCount */, 12345 /* vmem */);

	EventVersion fakeLatestRunawayVersion = 10;
	latestRunawayVersion = &fakeLatestRunawayVersion;

	/*
	 * Set beginCleanupRunawayVersion to less than *latestRunawayVersino
	 * to trigger a cleanup
	 */
	beginCleanupRunawayVersion = 1;
	endCleanupRunawayVersion = 1;

	/* Make sure the cleanup goes through */
	vmemTrackerInited = true;
	isProcessActive = true;

	CritSectionCount = 0;
	InterruptHoldoffCount = 1;

	CHECK_FOR_RUNAWAY_CLEANUP_MEMORY_LOGGING();
	RunawayCleaner_StartCleanup();

	assert_true(beginCleanupRunawayVersion == *latestRunawayVersion);
	/* Cleanup is done, without ever throwing an ERROR */
	assert_true(endCleanupRunawayVersion == beginCleanupRunawayVersion);

	/*
	 * cleanupCountdown is decremented by 1 as there was no error, and therefore
	 * the cleanup is done within the same call of RunawayCleaner_StartCleanup
	 */
	assert_true(MySessionState->cleanupCountdown == 1);

	InterruptHoldoffCount = 0;
}

/*
 * Checks if RunawayCleaner_StartCleanup() ignores cleanup if outside of any transaction
 */
static void
test__RunawayCleaner_StartCleanup__IgnoresCleanupOutsideAnyTransaction(void **state)
{
	InitFakeSessionState(2 /* activeProcessCount */,
						 2 /* cleanupCountdown */,
						 RunawayStatus_PrimaryRunawaySession /* runawayStatus */, 2 /* pinCount */, 12345 /* vmem */);

	EventVersion fakeLatestRunawayVersion = 10;
	latestRunawayVersion = &fakeLatestRunawayVersion;

	/*
	 * Set beginCleanupRunawayVersion to less than *latestRunawayVersino
	 * to trigger a cleanup
	 */
	beginCleanupRunawayVersion = 1;
	endCleanupRunawayVersion = 1;
	isProcessActive = true;

	/* Make sure the cleanup goes through */
	vmemTrackerInited = true;
	CritSectionCount = 0;
	InterruptHoldoffCount = 0;
	gp_command_count = 1;

	/* But it is called outside of a transaction */
	will_return(IsTransactionState, false);

	CHECK_FOR_RUNAWAY_CLEANUP_MEMORY_LOGGING();
	/* Should not call superuser() as that can cause a PANIC */
	RunawayCleaner_StartCleanup();

	assert_true(beginCleanupRunawayVersion == *latestRunawayVersion);
	/* Cleanup is done, without ever throwing an ERROR */
	assert_true(endCleanupRunawayVersion == beginCleanupRunawayVersion);

	/*
	 * cleanupCountdown is decremented by 1 as there was no error, and therefore
	 * the cleanup is done within the same call of RunawayCleaner_StartCleanup
	 */
	assert_true(MySessionState->cleanupCountdown == 1);

	InterruptHoldoffCount = 0;
}

/*
 * Checks if RunawayCleaner_RunawayCleanupDoneForProcess() ignores cleanupCountdown
 * if optional cleanup
 */
static void
test__RunawayCleaner_RunawayCleanupDoneForProcess__IgnoresCleanupIfNotRequired(void **state)
{
#define CLEANUP_COUNTDOWN 2
	InitFakeSessionState(2 /* activeProcessCount */,
			CLEANUP_COUNTDOWN /* cleanupCountdown */,
			RunawayStatus_PrimaryRunawaySession /* runawayStatus */, 2 /* pinCount */, 12345 /* vmem */);

	EventVersion fakeLatestRunawayVersion = 10;
	latestRunawayVersion = &fakeLatestRunawayVersion;

	/*
	 * Set beginCleanupRunawayVersion to less than *latestRunawayVersino
	 * to trigger a cleanup
	 */
	beginCleanupRunawayVersion = 1;
	endCleanupRunawayVersion = 1;

	/* Make sure the cleanup is not ignored for vmem initialization */
	vmemTrackerInited = true;
	/* Simulate a deactivation before the runaway */
	deactivationVersion = *latestRunawayVersion - 1;
	activationVersion = *latestRunawayVersion - 1;
	isProcessActive = false;

	CritSectionCount = 0;
	InterruptHoldoffCount = 0;

	RunawayCleaner_StartCleanup();

	/* The cleanup shouldn't even start as the QE was deactivated at the time of the runaway*/
	assert_true(beginCleanupRunawayVersion == 1);
	assert_true(endCleanupRunawayVersion == 1);

	/*
	 * cleanupCountdown should not be decremented as this was an optional cleanup
	 */
	assert_true(MySessionState->cleanupCountdown == CLEANUP_COUNTDOWN);
	assert_true(MySessionState->runawayStatus == RunawayStatus_PrimaryRunawaySession);

	/*
	 * Now simulate a scenario where the we activated but the runaway happened
	 * before the activation
	 */
	beginCleanupRunawayVersion = 2;
	endCleanupRunawayVersion = 2;

	/* Another runaway happened after the last cleanup */
	*latestRunawayVersion = beginCleanupRunawayVersion + 2;
	activationVersion = 5;
	deactivationVersion = 3;
	isProcessActive = true;

	RunawayCleaner_StartCleanup();

	/* The cleanup shouldn't even start as the runaway event happened before the QE became active */
	assert_true(beginCleanupRunawayVersion == 2);
	assert_true(endCleanupRunawayVersion == 2);

	/*
	 * cleanupCountdown should not be decremented as this was an optional cleanup
	 */
	assert_true(MySessionState->cleanupCountdown == CLEANUP_COUNTDOWN);
	assert_true(MySessionState->runawayStatus == RunawayStatus_PrimaryRunawaySession);
}

/*
 * Checks if RunawayCleaner_RunawayCleanupDoneForProcess ignores duplicate cleanup
 * for a single runaway event
 */
static void
test__RunawayCleaner_RunawayCleanupDoneForProcess__IgnoresDuplicateCalls(void **state)
{
	InitFakeSessionState(2 /* activeProcessCount */,
			2 /* cleanupCountdown */,
			RunawayStatus_PrimaryRunawaySession /* runawayStatus */, 2 /* pinCount */, 12345 /* vmem */);

	EventVersion fakeLatestRunawayVersion = 10;
	latestRunawayVersion = &fakeLatestRunawayVersion;
	/*
	 * Set beginCleanupRunawayVersion and endCleanupRunawayVersion to
	 * latestRunawayVersion which should make the function call a no-op
	 */
	beginCleanupRunawayVersion = *latestRunawayVersion;
	endCleanupRunawayVersion = *latestRunawayVersion;

	/* Make sure the cleanup goes through */
	vmemTrackerInited = true;
	isProcessActive = true;

	RunawayCleaner_RunawayCleanupDoneForProcess(false /* ignoredCleanup */);
	/* Nothing got cleaned */
	assert_true(MySessionState->cleanupCountdown == 2);
}

/*
 * Checks if RunawayCleaner_RunawayCleanupDoneForProcess prevents multiple cleanup
 * for a single runaway event by properly updating beginCleanupRunawayVersion and
 * endCleanupRunawayVersion
 */
static void
test__RunawayCleaner_RunawayCleanupDoneForProcess__PreventsDuplicateCleanup(void **state)
{
	InitFakeSessionState(2 /* activeProcessCount */,
			2 /* cleanupCountdown */,
			RunawayStatus_PrimaryRunawaySession /* runawayStatus */, CLEANUP_COUNTDOWN /* pinCount */, 12345 /* vmem */);

	EventVersion fakeLatestRunawayVersion = 10;
	latestRunawayVersion = &fakeLatestRunawayVersion;

	/*
	 * Some imaginary cleanup begin/end event version. The idea is to ensure
	 * that once the RunawayCleaner_RunawayCleanupDoneForProcess call returns
	 * we will have both set to latestRunawayVersion
	 */
	beginCleanupRunawayVersion = *latestRunawayVersion;
	endCleanupRunawayVersion = 0;

	/* Make sure the cleanup goes through */
	vmemTrackerInited = true;
	isProcessActive = true;

	CHECK_FOR_RUNAWAY_CLEANUP_MEMORY_LOGGING();
	RunawayCleaner_RunawayCleanupDoneForProcess(false /* ignoredCleanup */);
	/* cleanupCountdown should be adjusted */
	assert_true(MySessionState->cleanupCountdown == CLEANUP_COUNTDOWN - 1);

	assert_true(beginCleanupRunawayVersion == endCleanupRunawayVersion);
	assert_true(beginCleanupRunawayVersion == *latestRunawayVersion);

	/* Second call shouldn't change anything */
	RunawayCleaner_RunawayCleanupDoneForProcess(false /* ignoredCleanup */);
	/* cleanupCountdown is unchanged */
	assert_true(MySessionState->cleanupCountdown == CLEANUP_COUNTDOWN - 1);
}

/*
 * Checks if RunawayCleaner_RunawayCleanupDoneForProcess reactivates a process
 * if the deactivation process triggers cleanup for a pending runaway event
 */
static void
test__RunawayCleaner_RunawayCleanupDoneForProcess__UndoDeactivation(void **state)
{
	InitFakeSessionState(2 /* activeProcessCount */,
			2 /* cleanupCountdown */,
			RunawayStatus_PrimaryRunawaySession /* runawayStatus */, 2 /* pinCount */, 12345 /* vmem */);

	EventVersion fakeLatestRunawayVersion = 10;
	latestRunawayVersion = &fakeLatestRunawayVersion;

	/*
	 * Set beginCleanupRunawayVersion to latestRunawayVersion and endCleanupRunawayVersion
	 * to a smaller value to simulate an ongoing cleanup
	 */
	beginCleanupRunawayVersion = *latestRunawayVersion;
	endCleanupRunawayVersion = 1;

	/* Valid isRunawayDetector is necessary for Assert */
	static uint32 fakeIsRunawayDetector = 1;
	isRunawayDetector = &fakeIsRunawayDetector;

	/* Make sure we became idle after a pending runaway event */
	activationVersion = 1;
	deactivationVersion = *latestRunawayVersion + 1;

	/* Make sure the cleanup goes through */
	vmemTrackerInited = true;
	isProcessActive = false;

	/* We must undo the idle state */
	will_be_called(IdleTracker_ActivateProcess);

	CHECK_FOR_RUNAWAY_CLEANUP_MEMORY_LOGGING();
	RunawayCleaner_RunawayCleanupDoneForProcess(false /* ignoredCleanup */);
	/* The cleanupCountdown must be decremented as we cleaned up */
	assert_true(MySessionState->cleanupCountdown == 1);
	/* We updated the endCleanupRunawayVersion to indicate that we finished cleanup */
	assert_true(endCleanupRunawayVersion == beginCleanupRunawayVersion);
}

/*
 * Checks if RunawayCleaner_RunawayCleanupDoneForProcess reactivates the runaway detector
 * once all the processes of the runaway session are done cleaning
 */
static void
test__RunawayCleaner_RunawayCleanupDoneForProcess__ReactivatesRunawayDetection(void **state)
{
	InitFakeSessionState(2 /* activeProcessCount */,
			2 /* cleanupCountdown */,
			RunawayStatus_PrimaryRunawaySession /* runawayStatus */, 2 /* pinCount */, 12345 /* vmem */);

	EventVersion fakeLatestRunawayVersion = 10;
	latestRunawayVersion = &fakeLatestRunawayVersion;

	/*
	 * Set beginCleanupRunawayVersion to latestRunawayVersion and endCleanupRunawayVersion
	 * to a smaller value to simulate an ongoing cleanup
	 */
	beginCleanupRunawayVersion = *latestRunawayVersion;
	endCleanupRunawayVersion = 1;

	/* Valid isRunawayDetector is necessary for Assert */
	static uint32 fakeIsRunawayDetector = 1;
	isRunawayDetector = &fakeIsRunawayDetector;

	/* Just an active process that never became idle */
	activationVersion = 0;
	deactivationVersion = 0;
	isProcessActive = true;

	/* Make sure the cleanup goes through */
	vmemTrackerInited = true;

	CHECK_FOR_RUNAWAY_CLEANUP_MEMORY_LOGGING();
	RunawayCleaner_RunawayCleanupDoneForProcess(false /* ignoredCleanup */);
	/* The cleanupCountdown must be decremented as we cleaned up */
	assert_true(MySessionState->cleanupCountdown == 1);
	/* We updated the endCleanupRunawayVersion to indicate that we finished cleanup */
	assert_true(endCleanupRunawayVersion == beginCleanupRunawayVersion);

	/* The runaway detector promotion should be disabled as we still have 1 QE unclean */
	assert_true(*isRunawayDetector == 1);

	/*
	 * Fake a ongoing cleanup by making endCleanupRunawayVersion < beginCleanupRunawayVersion
	 * so that we can execute cleanup one more time, marking all QEs clean
	 */
	endCleanupRunawayVersion = 1;

	CHECK_FOR_RUNAWAY_CLEANUP_MEMORY_LOGGING();
	/*
	 * cleanupCountdown should reach 0, and immediately afterwards should be set to
	 * CLEANUP_COUNTDOWN_BEFORE_RUNAWAY
	 */
	RunawayCleaner_RunawayCleanupDoneForProcess(false /* ignoredCleanup */);
	assert_true(MySessionState->cleanupCountdown == CLEANUP_COUNTDOWN_BEFORE_RUNAWAY);
	/* Runaway detector should be re-enabled */
	assert_true(*isRunawayDetector == 0);
}

/*
 * Checks if RunawayCleaner_RunawayCleanupDoneForSession reactivates the runaway detector
 */
static void
test__RunawayCleaner_RunawayCleanupDoneForSession__ResetsRunawayFlagAndReactivateRunawayDetector(void **state)
{
	InitFakeSessionState(2 /* activeProcessCount */,
			2 /* cleanupCountdown */,
			RunawayStatus_PrimaryRunawaySession /* runawayStatus */, 2 /* pinCount */, 12345 /* vmem */);

	EventVersion fakeLatestRunawayVersion = 10;
	latestRunawayVersion = &fakeLatestRunawayVersion;

	/*
	 * Satisfy asserts
	 */
	beginCleanupRunawayVersion = *latestRunawayVersion;
	endCleanupRunawayVersion = beginCleanupRunawayVersion;
	MySessionState->cleanupCountdown = CLEANUP_COUNTDOWN_BEFORE_RUNAWAY;

	*isRunawayDetector = 1;
	MySessionState->runawayStatus = RunawayStatus_PrimaryRunawaySession;
	RunawayCleaner_RunawayCleanupDoneForSession();

	assert_true(MySessionState->runawayStatus == RunawayStatus_NotRunaway);
	/* Runaway detector should be re-enabled */
	assert_true(*isRunawayDetector == 0);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__RunawayCleaner_StartCleanup__IgnoresNonRunaway),
		unit_test(test__RunawayCleaner_StartCleanup__IgnoresDuplicateCleanup),
		unit_test(test__RunawayCleaner_StartCleanup__StartsPrimaryCleanupIfPossible),
		unit_test(test__RunawayCleaner_StartCleanup__StartsSecondaryCleanupIfPossible),
		unit_test(test__RunawayCleaner_StartCleanup__IgnoresCleanupInCriticalSection),
		unit_test(test__RunawayCleaner_StartCleanup__IgnoresCleanupInHoldoffInterrupt),
		unit_test(test__RunawayCleaner_StartCleanup__IgnoresCleanupOutsideAnyTransaction),
		unit_test(test__RunawayCleaner_RunawayCleanupDoneForProcess__IgnoresCleanupIfNotRequired),
		unit_test(test__RunawayCleaner_RunawayCleanupDoneForProcess__IgnoresDuplicateCalls),
		unit_test(test__RunawayCleaner_RunawayCleanupDoneForProcess__PreventsDuplicateCleanup),
		unit_test(test__RunawayCleaner_RunawayCleanupDoneForProcess__UndoDeactivation),
		unit_test(test__RunawayCleaner_RunawayCleanupDoneForProcess__ReactivatesRunawayDetection),
		unit_test(test__RunawayCleaner_RunawayCleanupDoneForSession__ResetsRunawayFlagAndReactivateRunawayDetector),
	};

	return run_tests(tests);
}
