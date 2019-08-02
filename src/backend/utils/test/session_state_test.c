#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"

#include "../session_state.c"

/*
 * This sets up an expected exception that will be rethrown for
 * verification using PG_TRY(), PG_CATCH() and PG_END_TRY() macros
 */
#define EXPECT_EXCEPTION()     \
	expect_any(ExceptionalCondition,conditionName); \
	expect_any(ExceptionalCondition,errorType); \
	expect_any(ExceptionalCondition,fileName); \
	expect_any(ExceptionalCondition,lineNumber); \
    will_be_called_with_sideeffect(ExceptionalCondition, &_ExceptionalCondition, NULL);\

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
	free((void *)AllSessionStateEntries);
	AllSessionStateEntries = NULL;
}

/*
 * Acquires a SessionState entry for the specified sessionid. If an existing entry
 * is found, this method reuses that entry
 */
static SessionState *
AcquireSessionState(int sessionId, int loglevel)
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

	EXPECT_EREPORT(loglevel);
	SessionState_Init();
	return (SessionState *) MySessionState;
}

/* Releases a SessionState entry for the specified sessionId */
static void
ReleaseSessionState(int sessionId)
{
	/* We call shutdown twice */
	will_be_called_count(LWLockAcquire, 2);
	will_be_called_count(LWLockRelease, 2);

	expect_any_count(LWLockAcquire, l, 2);
	expect_any_count(LWLockAcquire, mode, 2);
	expect_any_count(LWLockRelease, l, 2);

	gp_session_id = sessionId;
	/* First find the previously allocated session state */
	SessionState *foundSessionState = AcquireSessionState(sessionId, gp_sessionstate_loglevel);

	assert_true(foundSessionState->sessionId == sessionId);
	/*
	 * It was pre-allocated and we incremented the pinCount
	 * for finding it
	 */
	assert_true(foundSessionState->pinCount > 1);
	/* Satisfy assertion */
	sessionStateInited = true;
	EXPECT_EREPORT(gp_sessionstate_loglevel);
	/* Undo for our search pinCount */
	SessionState_Shutdown();
	/* The pinCount should not drop to 0 as we just undid our own increment */
	assert_true(foundSessionState->pinCount >= 1);
	MySessionState = foundSessionState;
	sessionStateInited = true;
	/*
	 * If we are releasing this SessionState permanently, we need to ensure
	 * that RunawayCleaner_RunawayCleanupDoneForSession() will be called
	 */
	if (foundSessionState->pinCount == 1)
	{
		will_be_called(RunawayCleaner_RunawayCleanupDoneForSession);
	}

	EXPECT_EREPORT(gp_sessionstate_loglevel);
	/* Undo one more to truly undo previously acquired one */
	SessionState_Shutdown();
}

/*
 * Checks if SessionState_ShmemInit does nothing under postmaster.
 * Note, it is *only* expected to re-attach with an existing array.
 */
static void
test__SessionState_ShmemInit__NoOpUnderPostmaster(void **state)
{
	AllSessionStateEntries = NULL;
	IsUnderPostmaster = true;

	static SessionStateArray fakeSessionStateArray;
	/* Initilize with some non-zero values */
	fakeSessionStateArray.maxSession = 0;
	fakeSessionStateArray.numSession = 0;
	fakeSessionStateArray.freeList = NULL;
	fakeSessionStateArray.usedList = NULL;

	will_return(ShmemInitStruct, &fakeSessionStateArray);
	will_assign_value(ShmemInitStruct, foundPtr, true);

	expect_any_count(ShmemInitStruct, name, 1);
	expect_any_count(ShmemInitStruct, size, 1);
	expect_any_count(ShmemInitStruct, foundPtr, 1);

	SessionState_ShmemInit();

	/* The lookup should always work, whether under postmaster or not */
	assert_true(AllSessionStateEntries == &fakeSessionStateArray);
	/* All the struct properties should be unchanged */
	assert_true(AllSessionStateEntries->maxSession == 0);
	assert_true(AllSessionStateEntries->numSession == 0);
	assert_true(AllSessionStateEntries->freeList == NULL &&
			AllSessionStateEntries->usedList == NULL);

	/* Undo the assignment for next test */
	AllSessionStateEntries = NULL;
}

/*
 * Checks if SessionState_ShmemInit initializes the SessionState entries
 * when postmaster
 */
static void
test__SessionState_ShmemInit__InitializesWhenPostmaster(void **state)
{
	IsUnderPostmaster = false;

	/* The intention is that MAX_BACKENDS here would match the value in guc.c */
#define MAX_BACKENDS 0x7fffff

	int allMaxBackends[] = {1, 100, MAX_BACKENDS};

	for (int i = 0; i < sizeof(allMaxBackends) / sizeof(int); i++)
	{
		CreateSessionStateArray(allMaxBackends[i]);

		/* All the struct properties should be unchanged */
		assert_true(AllSessionStateEntries->maxSession == MaxBackends);
		assert_true(AllSessionStateEntries->numSession == 0);
		assert_true(AllSessionStateEntries->freeList == AllSessionStateEntries->sessions &&
				AllSessionStateEntries->usedList == NULL);

		SessionState *prev = NULL;
		for (int j = 0; j < MaxBackends; j++)
		{
			SessionState *cur = (SessionState *) &AllSessionStateEntries->sessions[j];
			assert_true(cur->sessionId == INVALID_SESSION_ID);
			assert_true(cur->cleanupCountdown == CLEANUP_COUNTDOWN_BEFORE_RUNAWAY);
			assert_true(cur->runawayStatus == RunawayStatus_NotRunaway);
			assert_true(cur->pinCount == 0);
			assert_true(cur->activeProcessCount == 0);
			assert_true(cur->idle_start == 0);
			assert_true(cur->sessionVmem == 0);
			assert_true(cur->spinLock == 0);

			if (prev != NULL)
			{
				assert_true(prev->next == cur);
			}

			prev = cur;
		}

		assert_true(prev->next == NULL);

		DestroySessionStateArray();
	}
}

/*
 * Checks if SessionState_ShmemInit initializes the usedList and freeList
 * properly
 */
static void
test__SessionState_ShmemInit__LinkedListSanity(void **state)
{
	/* Only 3 entries to test the linked list sanity */
	CreateSessionStateArray(3);

	assert_true(AllSessionStateEntries->usedList == NULL &&
			AllSessionStateEntries->freeList ==  &AllSessionStateEntries->sessions[0] &&
			AllSessionStateEntries->sessions[0].next == &AllSessionStateEntries->sessions[1] &&
			AllSessionStateEntries->sessions[1].next == &AllSessionStateEntries->sessions[2] &&
			AllSessionStateEntries->sessions[2].next == NULL);

	DestroySessionStateArray();
}

/*
 * Checks if SessionState_Init initializes a SessionState entry after acquiring
 */
static void
test__SessionState_Init__AcquiresAndInitializes(void **state)
{
	/* Only 2 entry to test initialization */
	CreateSessionStateArray(1);

	SessionState *theEntry = AllSessionStateEntries->freeList;

	theEntry->activeProcessCount = 1234;
	theEntry->idle_start = 1234;
	theEntry->cleanupCountdown = 1234;
	theEntry->runawayStatus = RunawayStatus_PrimaryRunawaySession;
	theEntry->pinCount = 1234;
	theEntry->sessionId = 1234;
	theEntry->sessionVmem = 1234;
	/* Mark it as acquired and see if it is released */
	SpinLockAcquire(&theEntry->spinLock);
	assert_true(theEntry->spinLock == 1);

	/* These should be new */
	SessionState *first = AcquireSessionState(1, gp_sessionstate_loglevel);
	assert_true(first == theEntry);

	assert_true(theEntry->activeProcessCount == 0);
	assert_true(theEntry->idle_start == 0);
	assert_true(theEntry->cleanupCountdown == CLEANUP_COUNTDOWN_BEFORE_RUNAWAY);
	assert_true(theEntry->runawayStatus == RunawayStatus_NotRunaway);
	assert_true(theEntry->pinCount == 1);
	assert_true(theEntry->sessionId == 1);
	assert_true(theEntry->sessionVmem == 0);
	assert_true(theEntry->spinLock == 0);

	DestroySessionStateArray();
}


/*
 * Checks if SessionState_Init initializes the global variables
 * such as MySessionState and sessionStateInited properly
 */
static void
test__SessionState_Init__TestSideffects(void **state)
{
	/* Only 2 entry to test initialization */
	CreateSessionStateArray(1);

	will_be_called_count(LWLockAcquire, 1);
	will_be_called_count(LWLockRelease, 1);
	expect_any_count(LWLockAcquire, l, 1);
	expect_any_count(LWLockAcquire, mode, 1);
	expect_any_count(LWLockRelease, l, 1);

	assert_true(MySessionState == NULL);
	assert_true(sessionStateInited == false);

	EXPECT_EREPORT(gp_sessionstate_loglevel);
	SessionState_Init();

	assert_true(NULL != MySessionState);
	assert_true(sessionStateInited);

	DestroySessionStateArray();
}


/*
 * Checks if SessionState_Init acquires a new entry as well as
 * reuse an existing entry whenever possible
 */
static void
test__SessionState_Init__AcquiresWithReuse(void **state)
{
	/* Only 3 entries to test the reuse */
	CreateSessionStateArray(3);

	/* These should be new */
	SessionState *first = AcquireSessionState(1, gp_sessionstate_loglevel);
	SessionState *second = AcquireSessionState(2, gp_sessionstate_loglevel);
	SessionState *third = AcquireSessionState(3, gp_sessionstate_loglevel);

	assert_true(first != second);
	assert_true(first != third);
	assert_true(second != third);

	SessionState *reuseFirst = AcquireSessionState(1, gp_sessionstate_loglevel);
	assert_true(reuseFirst == first);
	assert_true(reuseFirst->pinCount == 2);
	assert_true(AllSessionStateEntries->usedList == third &&
			third->next == second && second->next == first);

	DestroySessionStateArray();
}

/*
 * Checks if SessionState_Init fails when no more SessionState entry
 * is available to satisfy a new request
 */
static void
test__SessionState_Init__FailsIfNoFreeSessionStateEntry(void **state)
{
	/* Only 3 entries to exhaust the entries */
	CreateSessionStateArray(3);

	/* These should be new */
	AcquireSessionState(1, gp_sessionstate_loglevel);
	AcquireSessionState(2, gp_sessionstate_loglevel);
	AcquireSessionState(3, gp_sessionstate_loglevel);

	PG_TRY();
	{
		/* No more SessionState entry to satisfy this request */
		AcquireSessionState(4, FATAL);
		assert_false("No ereport(FATAL, ...) was called");
	}
	PG_CATCH();
	{

	}
	PG_END_TRY();

	DestroySessionStateArray();
}

/*
 * Checks if SessionState_Shutdown decrements pinCount and releases
 * SessionState entry as appropriate. The usedList, freeList and
 * sessions array are also checked for sanity
 */
static void
test__SessionState_Shutdown__ReleaseSessionEntry(void **state)
{
	/* Only 3 entries to test the reuse */
	CreateSessionStateArray(3);

	/* These should be new */
	SessionState *first = AcquireSessionState(1, gp_sessionstate_loglevel);
	SessionState *second = AcquireSessionState(2, gp_sessionstate_loglevel);
	SessionState *third = AcquireSessionState(3, gp_sessionstate_loglevel);

	assert_true(first != second);
	assert_true(second != third);

	SessionState *reuseFirst = AcquireSessionState(1, gp_sessionstate_loglevel);
	assert_true(reuseFirst == first);
	assert_true(reuseFirst->pinCount == 2);
	assert_true(AllSessionStateEntries->usedList == third &&
			third->next == second && second->next == first);

	/* The entire linked list has been reversed as we shift from freeList to usedList*/
	assert_true(AllSessionStateEntries->sessions[0].next == NULL &&
			AllSessionStateEntries->sessions[1].next == &AllSessionStateEntries->sessions[0] &&
			AllSessionStateEntries->sessions[2].next == &AllSessionStateEntries->sessions[1]);
	/* The last entry is at the head of the usedList */
	assert_true(AllSessionStateEntries->usedList == &AllSessionStateEntries->sessions[2] &&
			third == &AllSessionStateEntries->sessions[2]);
	/* All 3 entries are consumed */
	assert_true(AllSessionStateEntries->freeList == NULL);

	/* Release 1 entry */
	ReleaseSessionState(2);
	assert_true(AllSessionStateEntries->freeList == second);
	assert_true(AllSessionStateEntries->numSession == 2);

	/* Entry 1 had 2 pinCount. So, we need 2 release call. */
	ReleaseSessionState(1);
	assert_true(AllSessionStateEntries->numSession == 2);
	/* Only 1 free entry */
	assert_true(AllSessionStateEntries->freeList->next == NULL);
	ReleaseSessionState(1);
	assert_true(AllSessionStateEntries->numSession == 1);
	/* Only 1 used entry */
	assert_true(AllSessionStateEntries->usedList->next == NULL);

	/* Release entry for session 3 */
	ReleaseSessionState(3);
	assert_true(AllSessionStateEntries->numSession == 0);
	/*
	 * Based on free ordering, now we have session 2 at the tail of freeList, preceeded by
	 * session 1 and preceded by session 3. Note, the indexing starts at 0, while session
	 * id starts at 1
	 */
	assert_true(AllSessionStateEntries->sessions[1].next == NULL &&
			AllSessionStateEntries->sessions[0].next == &AllSessionStateEntries->sessions[1] &&
			AllSessionStateEntries->sessions[2].next == &AllSessionStateEntries->sessions[0]);
	assert_true(AllSessionStateEntries->freeList == &AllSessionStateEntries->sessions[2]);
	assert_true(AllSessionStateEntries->usedList == NULL);
	DestroySessionStateArray();
}

/*
 * Checks if SessionState_Shutdown marks the session clean when the pinCount
 * drops to 0 (i.e., releasing the entry back to the freeList)
 */
static void
test__SessionState_Shutdown__MarksSessionCleanUponRelease(void **state)
{
	/* Only 3 entries to test the reuse */
	CreateSessionStateArray(1);

	/* These should be new */
	SessionState *first = AcquireSessionState(1, gp_sessionstate_loglevel);
	SessionState *reuseFirst = AcquireSessionState(1, gp_sessionstate_loglevel);
	SessionState *reuseAgain = AcquireSessionState(1, gp_sessionstate_loglevel);

	assert_true(reuseFirst == first && reuseAgain == first);
	assert_true(reuseFirst->pinCount == 3);

	/* Entry 1 had 2 pinCount. So, we need 3 release call. */
	ReleaseSessionState(1);
	assert_true(reuseFirst->pinCount == 2 && AllSessionStateEntries->numSession == 1);
	ReleaseSessionState(1);
	assert_true(reuseFirst->pinCount == 1 && AllSessionStateEntries->numSession == 1);

	will_be_called_count(LWLockAcquire, 1);
	will_be_called_count(LWLockRelease, 1);
	expect_any_count(LWLockAcquire, l, 1);
	expect_any_count(LWLockAcquire, mode, 1);
	expect_any_count(LWLockRelease, l, 1);

	/* Bypass assertion */
	MySessionState = first;
	sessionStateInited = true;
	will_be_called(RunawayCleaner_RunawayCleanupDoneForSession);
	EXPECT_EREPORT(gp_sessionstate_loglevel);
	/* This will finally release the entry */
	SessionState_Shutdown();

	assert_true(AllSessionStateEntries->numSession == 0);

	DestroySessionStateArray();
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);
	/*
	 * Initializing the gp_sessionstate_loglevel to make sure it is not
	 * set to some random value
	 */
	gp_sessionstate_loglevel = LOG;

	const UnitTest tests[] = {
		unit_test(test__SessionState_ShmemInit__NoOpUnderPostmaster),
		unit_test(test__SessionState_ShmemInit__InitializesWhenPostmaster),
		unit_test(test__SessionState_ShmemInit__LinkedListSanity),
		unit_test(test__SessionState_Init__TestSideffects),
		unit_test(test__SessionState_Init__AcquiresWithReuse),
		unit_test(test__SessionState_Init__AcquiresAndInitializes),
		unit_test(test__SessionState_Init__FailsIfNoFreeSessionStateEntry),
		unit_test(test__SessionState_Shutdown__ReleaseSessionEntry),
		unit_test(test__SessionState_Shutdown__MarksSessionCleanUponRelease),
	};

	return run_tests(tests);
}
