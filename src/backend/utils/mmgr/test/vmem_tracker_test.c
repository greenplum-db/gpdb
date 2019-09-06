#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../vmem_tracker.c"

#define EXPECT_EXCEPTION()     \
	expect_any(ExceptionalCondition,conditionName); \
	expect_any(ExceptionalCondition,errorType); \
	expect_any(ExceptionalCondition,fileName); \
	expect_any(ExceptionalCondition,lineNumber); \
    will_be_called_with_sideeffect(ExceptionalCondition, &_ExceptionalCondition, NULL);\

#define SEGMENT_VMEM_CHUNKS_TEST_VALUE 100

#undef PG_RE_THROW
#define PG_RE_THROW() siglongjmp(*PG_exception_stack, 1)

/*
 * This method will emulate the real ExceptionalCondition
 * function by re-throwing the exception, essentially falling
 * back to the next available PG_CATCH();
 */
static
void _ExceptionalCondition()
{
     PG_RE_THROW();
}

/* segmentVmemChunks pointer uses the address of this global variable */
static int32 fakeSegmentVmemChunks = 0;

/*
 * A global variable to save the trackedBytes just before VmemTracker_ReserveVmem
 * is called. Can be used to check the sanity of trackedBytes during a
 * RedZoneHandler_DetectRunawaySession call
 */
static int64 preAllocTrackedBytes = 0;

/* MySessionState will use the address of this global variable */
static SessionState fakeSessionState;

/* segmentOOMTime pointer uses the address of this global variable */
static OOMTimeType fakeSegmentOOMTime = 0;

/* Prepares a fake MySessionState pointer for use in the vmem tracker */
static
void InitFakeSessionState()
{
	MySessionState = &fakeSessionState;

	MySessionState->activeProcessCount = 0;
	MySessionState->cleanupCountdown = -1;
	MySessionState->runawayStatus = RunawayStatus_NotRunaway;
	MySessionState->next = NULL;
	MySessionState->pinCount = 1;
	MySessionState->sessionId = 1234;
	MySessionState->sessionVmem = 0;
	MySessionState->spinLock = 0;
}

/* Resets the OOM event state */
static void OOMEventSetup()
{
	segmentOOMTime = &fakeSegmentOOMTime;

	*segmentOOMTime = 0;
	oomTrackerStartTime = 1;
	alreadyReportedOOMTime = 0;
}

/*
 * This method sets up necessary data structures such as
 * fake session state, segmentVmemChunks etc. for testing
 * vmem tracker.
 */
static
void VmemTrackerTestSetup(void **state)
{
	InitFakeSessionState();

	OOMEventSetup();

	/* 8GB default VMEM */
	gp_vmem_protect_limit = 8192;
	/* Disable runaway detector */
	runaway_detector_activation_percent = 100;

	will_return(ShmemInitStruct, &fakeSegmentVmemChunks);
	will_assign_value(ShmemInitStruct, foundPtr, false);

	expect_any_count(ShmemInitStruct, name, 1);
	expect_any_count(ShmemInitStruct, size, 1);
	expect_any_count(ShmemInitStruct, foundPtr, 1);

	will_be_called(EventVersion_ShmemInit);
	will_be_called(RedZoneHandler_ShmemInit);
	will_be_called(IdleTracker_ShmemInit);

	/* Keep the assertion happy and also initialize everything */
	IsUnderPostmaster = false;
	VmemTracker_ShmemInit();

	will_be_called(IdleTracker_Init);
	will_be_called(RunawayCleaner_Init);
	VmemTracker_Init();

	/* Make sure the VMEM enforcement would work */
	Gp_role = GP_ROLE_EXECUTE;
	CritSectionCount = 0;
	IsUnderPostmaster = true;
}

/*
 * This method cleans up vmem tracker data structures after a test run.
 */
static
void VmemTrackerTestTeardown(void **state)
{
#ifdef USE_ASSERT_CHECKING
	will_return(MemoryProtection_IsOwnerThread, true);
#endif

	will_be_called(IdleTracker_Shutdown);
	VmemTracker_Shutdown();
}

/*
 * Checks if the vmem tracker can be disabled (no further reservation)
 */
static
void test__VmemTracker_ReserveVmem__IgnoreWhenUninitialized(void **state)
{
	vmemTrackerInited = false;
	int preAllocChunks = trackedVmemChunks;
	/* Allocate 10MB */
	VmemTracker_ReserveVmem(1024 * 1024 * 10);
	/* As vmemTracker is not initialized, new reservation should not happen */
	assert_true(preAllocChunks == trackedVmemChunks);
}

/* Checks if the reservation fails for negative size request */
static
void test__VmemTracker_ReserveVmem__FailForInvalidSize(void **state)
{
#ifdef USE_ASSERT_CHECKING
	EXPECT_EXCEPTION();
	PG_TRY();
	{
		/* The min size is 0. So, negative size should fail. */
		VmemTracker_ReserveVmem(-1);
		assert_true(false);
	}
	PG_CATCH();
	{
	}
	PG_END_TRY();
#endif
}

/*
 * Checks the effectiveness of the cache.
 *
 * This will test the following:
 *
 * 1. Cache satisfiable reservations use cache
 * 2. Insufficient cache triggers new reservation (i.e., increase cache)
 * 3. Freeing would leave unused cache, that can be reused later
 */
static
void test__VmemTracker_ReserveVmem__CacheSanity(void **state)
{
	/* GPDB Memory protection is enabled and initialized */
	gp_mp_inited = true;

	int64 oneChunkBytes = 1 << chunkSizeInBits;

	assert_true(0 == trackedVmemChunks);

#ifdef USE_ASSERT_CHECKING
	will_return_count(MemoryProtection_IsOwnerThread, true, 5);
#endif

	int64 prevTrackedBytes = trackedBytes;
	will_be_called(RedZoneHandler_DetectRunawaySession);
	/* This triggers one chunk allocation */
	VmemTracker_ReserveVmem(oneChunkBytes + 1);
	assert_true(1 == trackedVmemChunks);
	assert_true(prevTrackedBytes + oneChunkBytes + 1 == trackedBytes);

	int64 twoChunkBytes = 2 * oneChunkBytes;
	/*
	 * This will not trigger a new chunk reservation: previously we allocated
	 * (oneChunkBytes + 1) and up to (twoChunkBytes - 1) we will not need a
	 * new chunk (as we already had an extra chunk - 1). The difference is
	 * the amount up to which we can allocate without any new chunk reservation
	 */
	VmemTracker_ReserveVmem(twoChunkBytes - 1 - (oneChunkBytes + 1));
	assert_true(1 == trackedVmemChunks);

	will_be_called(RedZoneHandler_DetectRunawaySession);
	/* This will trigger a new chunk reservation */
	VmemTracker_ReserveVmem(1);
	assert_true(2 == trackedVmemChunks);

	/* This will satisfy from the cache */
	VmemTracker_ReserveVmem(oneChunkBytes - 1);
	assert_true(2 == trackedVmemChunks);

	will_be_called(RedZoneHandler_DetectRunawaySession);
	/*
	 * This will trigger three new chunk reservation: we exhausted previous reservation,
	 * and now we are asking for two new chunks + 1 extra bytes, which translates to 3
	 * chunks, where the last chunk only has 1 byte used.
	 */
	VmemTracker_ReserveVmem(twoChunkBytes + 1);
	assert_true(5 == trackedVmemChunks);

	/* Free one chunk. */
	VmemTracker_ReleaseVmem(oneChunkBytes);
	assert_true(4 == trackedVmemChunks);

	VmemTracker_ReserveVmem(oneChunkBytes / 2);
	assert_true(4 == trackedVmemChunks);

	/* Still not reserving any additional chunks */
	VmemTracker_ReserveVmem(oneChunkBytes / 2 - 1);
	assert_true(4 == trackedVmemChunks);

	will_be_called(RedZoneHandler_DetectRunawaySession);
	/* 1 more byte, and we need a new chunk */
	VmemTracker_ReserveVmem(1);
	assert_true(5 == trackedVmemChunks);
}

/*
 * Checks the sanity of the tracked bytes.
 *
 * This will test the following:
 *
 * 1. If within segment and session limit, the request will be honored and
 *    the tracked bytes will be increased accordingly
 * 2. If request exceeds segment or session limit, it will not be honored
 *    and the trackedBytes will be unchanged
 * 3. For failure due to exceeding either segment or session limit, we will
 *    error out with proper error type (i.e., if segment limit is smaller than
 *    session limit, we will hit segment limit first, or vice versa)
 * 4. For equal segment and session limit we will hit the session limit first.
 */
static
void test__VmemTracker_ReserveVmem__TrackedBytesSanity(void **state)
{
	/* GPDB Memory protection is enabled and initialized */
	gp_mp_inited = true;

	/* Session quota is disabled */
	assert_true(0 == maxChunksPerQuery);

#ifdef USE_ASSERT_CHECKING
	will_return_count(MemoryProtection_IsOwnerThread, true, 5);
#endif

	will_be_called(RedZoneHandler_DetectRunawaySession);
	MemoryAllocationStatus status = VmemTracker_ReserveVmem(CHUNKS_TO_BYTES(vmemChunksQuota));
	assert_true(status == MemoryAllocation_Success);
	assert_true(trackedVmemChunks == vmemChunksQuota);
	assert_true(trackedBytes == CHUNKS_TO_BYTES(vmemChunksQuota));
	int chunksReserved = trackedVmemChunks;

	status = VmemTracker_ReserveVmem(CHUNKS_TO_BYTES(1) - 1);
	/* No new chunk should have been reserved */
	assert_true(chunksReserved == trackedVmemChunks);

	/* This will be over the vmem limit */
	will_be_called(RedZoneHandler_DetectRunawaySession);
	/* 1 more byte, and we need a new chunk */
	status = VmemTracker_ReserveVmem(1);
	assert_true(status == MemoryFailure_VmemExhausted);
	assert_true(trackedVmemChunks == vmemChunksQuota);
	assert_true(trackedBytes == CHUNKS_TO_BYTES(vmemChunksQuota + 1) - 1);

	VmemTracker_ReleaseVmem(trackedBytes);
	assert_true(0 == trackedBytes);

	/* Enable session quota and set it to vmemChunksQuota */
	maxChunksPerQuery = vmemChunksQuota;

	will_be_called(RedZoneHandler_DetectRunawaySession);
	/* This will first hit the session limit */
	status = VmemTracker_ReserveVmem(CHUNKS_TO_BYTES(vmemChunksQuota + 1));
	assert_true(status == MemoryFailure_QueryMemoryExhausted);
	assert_true(trackedVmemChunks == 0);
	assert_true(trackedBytes == 0);

	/* Enable session quota and set it to more than vmemChunksQuota */
	maxChunksPerQuery = vmemChunksQuota + 1;

	will_be_called(RedZoneHandler_DetectRunawaySession);
	/* This will first hit the vmem limit */
	/* 1 more byte, and we need a new chunk */
	status = VmemTracker_ReserveVmem(CHUNKS_TO_BYTES(vmemChunksQuota + 1));
	assert_true(status == MemoryFailure_VmemExhausted);
	assert_true(trackedVmemChunks == 0);
	assert_true(trackedBytes == 0);
}

/*
 * Helper method to check if trackedBytes is rolled back before
 * calling RedZoneHandler_DetectRunawaySession.
 *
 * This method assumes a non-zero trackedBytes. So, the caller must
 * pre-allocate some memory before attempting to verify rollback of
 * trackedBytes during runaway detector for a particular reservation.
 */
static
void RedZoneHandler_DetectRunawaySession_TrackedBytesSanity()
{
	assert_true(0 != trackedBytes && trackedBytes == preAllocTrackedBytes);
}

/*
 * Checks if we undo tracked bytes before calling red-zone detector and whether
 * we redo tracked bytes once the call returns
 */
static
void test__VmemTracker_ReserveVmem__TrackedBytesSanityForRedzoneDetection(void **state)
{
	/* GPDB Memory protection is enabled and initialized */
	gp_mp_inited = true;

#ifdef USE_ASSERT_CHECKING
	will_return_count(MemoryProtection_IsOwnerThread, true, 2);
#endif
	will_be_called(RedZoneHandler_DetectRunawaySession);
	/* This will first hit the vmem limit */
	/* 1 more byte, and we need a new chunk */
	VmemTracker_ReserveVmem(CHUNKS_TO_BYTES(2) + 1);
	assert_true(2 == trackedVmemChunks);

	will_be_called_with_sideeffect(RedZoneHandler_DetectRunawaySession, &RedZoneHandler_DetectRunawaySession_TrackedBytesSanity, NULL);
	preAllocTrackedBytes = trackedBytes;
	VmemTracker_ReserveVmem(CHUNKS_TO_BYTES(1));
	assert_true(3 == trackedVmemChunks);
	assert_true(preAllocTrackedBytes + CHUNKS_TO_BYTES(1) == trackedBytes);
}

/* Checks if we call OOM logger before reserving any new VMEM */
static
void test__VmemTracker_ReserveVmem__OOMLoggingBeforeReservation(void **state)
{
	gp_mp_inited = true;
	OOMTimeType tempOOMtime;
	segmentOOMTime = &tempOOMtime;
	/* Make *segmentOOMTime > oomTrackerStartTime and alreadyReportedOOMTime */
	*segmentOOMTime = 1;
	oomTrackerStartTime = 0;
	alreadyReportedOOMTime = 0;

	/* Verify that we are actually trying to log OOM */
	expect_value(UpdateTimeAtomically, time_var, &alreadyReportedOOMTime);
	will_be_called(UpdateTimeAtomically);
	expect_any_count(write_stderr, fmt, -1);
	will_be_called(write_stderr);
	will_be_called(MemoryAccounting_SaveToLog);
	expect_any(MemoryContextStats, context);
	will_be_called(MemoryContextStats);

	/* 1 for OOM logging and 1 for VmemTracker_ReserveVmemChunks() */
#ifdef USE_ASSERT_CHECKING
	will_return_count(MemoryProtection_IsOwnerThread, true, 2);
#endif
	will_be_called(RedZoneHandler_DetectRunawaySession);
	VmemTracker_ReserveVmem(CHUNKS_TO_BYTES(2) + 1);
	assert_true(2 == trackedVmemChunks);
}

/*
 * Checks if we attach segmentVmemLimit properly, without changing the value
 * of the limit, when under postmaster
 */
static
void test__VmemTracker_ShmemInit__InitSegmentVmemLimitUnderPostmaster(void **state)
{
	/* Keep the assertions happy */
	vmemTrackerInited = false;
	chunkSizeInBits = BITS_IN_MB;

	static int32 tempSegmentVmemChunks = SEGMENT_VMEM_CHUNKS_TEST_VALUE;
	expect_any(ShmemInitStruct, name);
	expect_any(ShmemInitStruct, size);
	expect_any(ShmemInitStruct, foundPtr);
	will_assign_value(ShmemInitStruct, foundPtr, true);
	will_return(ShmemInitStruct, &tempSegmentVmemChunks);

	assert_true(segmentVmemChunks == &fakeSegmentVmemChunks);

	/*
	 * When under postmaster, we don't reinitialize the segmentVmemChunks, we just
	 * attach to it
	 */
	IsUnderPostmaster = true;
	VmemTracker_ShmemInit();
	assert_true(segmentVmemChunks == &tempSegmentVmemChunks &&
			segmentVmemChunks != &fakeSegmentVmemChunks &&
			*segmentVmemChunks == SEGMENT_VMEM_CHUNKS_TEST_VALUE);

	Assert(!vmemTrackerInited);
}

/*
 * Checks if we attach segmentVmemLimit properly and initialize it to 0.
 */
static
void test__VmemTracker_ShmemInit__InitSegmentVmemLimitInPostmaster(void **state)
{
	/* Keep the assertions happy */
	vmemTrackerInited = false;
	chunkSizeInBits = BITS_IN_MB;

	static int32 tempSegmentVmemChunks = SEGMENT_VMEM_CHUNKS_TEST_VALUE;
	expect_any(ShmemInitStruct, name);
	expect_any(ShmemInitStruct, size);
	expect_any(ShmemInitStruct, foundPtr);
	will_assign_value(ShmemInitStruct, foundPtr, false);
	will_return(ShmemInitStruct, &tempSegmentVmemChunks);

	assert_true(segmentVmemChunks == &fakeSegmentVmemChunks);

	will_be_called(EventVersion_ShmemInit);
	will_be_called(RedZoneHandler_ShmemInit);
	will_be_called(IdleTracker_ShmemInit);

	/*
	 * When not under postmaster, we don't reinitialize the segmentVmemChunks, we just
	 * attach to it
	 */
	IsUnderPostmaster = false;
	VmemTracker_ShmemInit();
	assert_true(segmentVmemChunks == &tempSegmentVmemChunks &&
			segmentVmemChunks != &fakeSegmentVmemChunks &&
			*segmentVmemChunks != SEGMENT_VMEM_CHUNKS_TEST_VALUE &&
			*segmentVmemChunks == 0);

	Assert(!vmemTrackerInited);
}

/* Helper method to set the segment and session vmem limit to desired values */
static
void SetVmemLimit(int32 newSegmentVmemLimitMB, int32 newSessionVmemLimitMB)
{
	static int32 tempSegmentVmemChunks = 0;

	/* Keep the assertions happy */
	vmemTrackerInited = false;
	chunkSizeInBits = BITS_IN_MB;

	expect_any(ShmemInitStruct, name);
	expect_any(ShmemInitStruct, size);
	expect_any(ShmemInitStruct, foundPtr);
	will_assign_value(ShmemInitStruct, foundPtr, false);
	will_return(ShmemInitStruct, &tempSegmentVmemChunks);

	will_be_called(EventVersion_ShmemInit);
	will_be_called(RedZoneHandler_ShmemInit);
	will_be_called(IdleTracker_ShmemInit);

	IsUnderPostmaster = false;

	gp_vmem_protect_limit = newSegmentVmemLimitMB;
	/* Session vmem limit is in kB unit */
	gp_vmem_limit_per_query = newSessionVmemLimitMB * 1024;
	VmemTracker_ShmemInit();

	assert_true(chunkSizeInBits >= BITS_IN_MB && vmemChunksQuota == MB_TO_CHUNKS(newSegmentVmemLimitMB));
	assert_true(chunkSizeInBits >= BITS_IN_MB && maxChunksPerQuery == MB_TO_CHUNKS(newSessionVmemLimitMB));
}

/*
 * Checks if we correctly calculate segment and session vmem limit.
 * Also checks the chunk size adjustment.
 */
static
void test__VmemTracker_ShmemInit__QuotaCalculation(void **state)
{
	SetVmemLimit(0, 1024);
	assert_true(chunkSizeInBits == BITS_IN_MB);
	assert_true(vmemChunksQuota == gp_vmem_protect_limit);
	assert_true(maxChunksPerQuery == (gp_vmem_limit_per_query / 1024));

	SetVmemLimit(1024 * 8, 1024);
	assert_true(chunkSizeInBits == BITS_IN_MB);
	assert_true(vmemChunksQuota == gp_vmem_protect_limit);
	assert_true(maxChunksPerQuery == (gp_vmem_limit_per_query / 1024));

	SetVmemLimit(1024 * 16 + 1, 1024);
	assert_true(chunkSizeInBits == BITS_IN_MB + 1);
	assert_true(vmemChunksQuota == gp_vmem_protect_limit / 2);
	assert_true(maxChunksPerQuery == (gp_vmem_limit_per_query / (1024 * 2)));

	SetVmemLimit(1024 * 32, 1024);
	assert_true(chunkSizeInBits == BITS_IN_MB + 1);
	assert_true(vmemChunksQuota == gp_vmem_protect_limit / 2);
	assert_true(maxChunksPerQuery == (gp_vmem_limit_per_query / (1024 * 2)));

	/*
	 * After one integer division of (32GB + 1) by 2 would still be 16GB, so
	 * only 1 shifting would happen
	 */
	SetVmemLimit(1024 * 32 + 1, 1024);
	assert_true(chunkSizeInBits == BITS_IN_MB + 1);
	assert_true(vmemChunksQuota == gp_vmem_protect_limit / 2);
	assert_true(maxChunksPerQuery == (gp_vmem_limit_per_query / (1024 * 2)));

	/*
	 * After one integer division of (32GB + 2) by 2 would still be 16GB + 1, so
	 * 2 shifting would happen
	 */
	SetVmemLimit(1024 * 32 + 2, 1024);
	assert_true(chunkSizeInBits == BITS_IN_MB + 2);
	assert_true(vmemChunksQuota == gp_vmem_protect_limit / 4);
	assert_true(maxChunksPerQuery == (gp_vmem_limit_per_query / (1024 * 4)));

	SetVmemLimit(1024 * 64 + 4, 1024);
	assert_true(chunkSizeInBits == BITS_IN_MB + 3);
	assert_true(vmemChunksQuota == gp_vmem_protect_limit / 8);
	assert_true(maxChunksPerQuery == (gp_vmem_limit_per_query / (1024 * 8)));

	/* Reset to default for future test sanity */
	SetVmemLimit(8 * 1024, 0);
}

/*
 * Checks if we call *_Init on required sub-modules.
 */
static
void test__VmemTracker_Init__InitializesOthers(void **state)
{
	/* Keep the assertion happy */
	vmemTrackerInited = false;

	will_be_called(IdleTracker_Init);
	will_be_called(RunawayCleaner_Init);
	VmemTracker_Init();

	/* Now the vmemTrackerInited should be true */
	assert_true(vmemTrackerInited);
	/* All local vmem tracking counters should be set to 0 */
	assert_true(trackedVmemChunks == 0);
	assert_true(maxVmemChunksTracked == 0);
	assert_true(trackedBytes == 0);
}

/*
 * Checks if we release all vmem during shutdown.
 */
static
void test__VmemTracker_Shutdown__ReleasesAllVmem(void **state)
{
#ifdef USE_ASSERT_CHECKING
	will_return_count(MemoryProtection_IsOwnerThread, true, 2);
#endif

	will_be_called(RedZoneHandler_DetectRunawaySession);
	MemoryAllocationStatus status = VmemTracker_ReserveVmem(CHUNKS_TO_BYTES(vmemChunksQuota));
	assert_true(status == MemoryAllocation_Success);
	assert_true(trackedVmemChunks == vmemChunksQuota);
	assert_true(0 < vmemChunksQuota && trackedBytes == CHUNKS_TO_BYTES(vmemChunksQuota));

	assert_true(vmemTrackerInited);
	assert_true(trackedVmemChunks > 0);
	assert_true(maxVmemChunksTracked > 0);
	assert_true(trackedBytes > 0);

	will_be_called(IdleTracker_Shutdown);
	VmemTracker_Shutdown();

	assert_false(vmemTrackerInited);
	assert_true(trackedVmemChunks == 0);
	/* maxVmemChunksTracked should not be affected */
	assert_true(maxVmemChunksTracked > 0);
	assert_true(trackedBytes == 0);
}

/*
 * Checks if waiver works after we exhaust vmem quota. Also checks the resetting
 * of the waiver once the vmem usage falls below vmem quota and a new chunk
 * can be reserved from vmem quota.
 */
static
void test__VmemTracker_RequestWaiver__WaiveEnforcement(void **state)
{
#ifdef USE_ASSERT_CHECKING
	will_return_count(MemoryProtection_IsOwnerThread, true, 7);
#endif

	will_be_called_count(RedZoneHandler_DetectRunawaySession, 5);
	/* Exhaust everything */
	MemoryAllocationStatus status = VmemTracker_ReserveVmem(CHUNKS_TO_BYTES(vmemChunksQuota + 1) - 1);
	assert_true(status == MemoryAllocation_Success);
	assert_true(trackedVmemChunks == vmemChunksQuota);
	assert_true(trackedBytes == CHUNKS_TO_BYTES(vmemChunksQuota + 1) - 1);

	int oldTrackedVmemChunks = trackedVmemChunks;
	int64 oldTrackedBytes = trackedBytes;
	int oldVmemChunksQuota = vmemChunksQuota;
	int oldWaivedChunks = waivedChunks;


	/* 1 more byte will fail */
	status = VmemTracker_ReserveVmem(1);
	assert_true(status == MemoryFailure_VmemExhausted);
	/* No new chunk should have been reserved */
	assert_true(oldTrackedVmemChunks == trackedVmemChunks);

	/* Requesting just 1 byte should give a whole chunk */
	VmemTracker_RequestWaiver(1);
	assert_true(trackedBytes == oldTrackedBytes);
	/* We don't touch vemChunksQuota directly */
	assert_true(vmemChunksQuota == oldVmemChunksQuota);
	/* 1 extra waived chunks */
	assert_true(oldWaivedChunks + 1 == waivedChunks);

	/* A whole chunk should now succeed using the additional waiver */
	status = VmemTracker_ReserveVmem(CHUNKS_TO_BYTES(1));
	assert_true(MemoryAllocation_Success == status);
	assert_true(trackedBytes == oldTrackedBytes + CHUNKS_TO_BYTES(1));

	oldTrackedVmemChunks = trackedVmemChunks;
	oldTrackedBytes = trackedBytes;
	oldWaivedChunks = waivedChunks;
	oldVmemChunksQuota = vmemChunksQuota;

	/* This will be over the vmem limit + waived chunks, therefore will fail */
	will_be_called(RedZoneHandler_DetectRunawaySession);
	/* 1 more byte, and we need a new chunk */
	status = VmemTracker_ReserveVmem(1);
	assert_true(status == MemoryFailure_VmemExhausted);
	assert_true(trackedVmemChunks == oldTrackedVmemChunks);
	assert_true(trackedBytes == oldTrackedBytes);
	assert_true(waivedChunks == 1 && oldWaivedChunks == waivedChunks);

	/* Enlarge the waived chunks */
	VmemTracker_RequestWaiver(CHUNKS_TO_BYTES(2));
	assert_true(waivedChunks == 2);
	/* Enlarged waivedChunks should now suffice this reservation request */
	status = VmemTracker_ReserveVmem(CHUNKS_TO_BYTES(1));
	assert_true(status == MemoryAllocation_Success);

	/* No reduction in waivedChunks is permitted */
	VmemTracker_RequestWaiver(CHUNKS_TO_BYTES(1));
	/* Should be unchanged to the maximum reserved waiver */
	assert_true(waivedChunks == 2);

	/* We allocated two additional waiver chunks after exhausting vmemChunksQuota */
	assert_true(trackedBytes == CHUNKS_TO_BYTES(vmemChunksQuota + 3) - 1);

	/*
	 * We allocated two chunks in the waiver zone after exhausting the quota.
	 * We free those, and 1 additional chunk to test resetting of waiver
	 */
	VmemTracker_ReleaseVmem(CHUNKS_TO_BYTES(3));
	/* We don't change waiver unless we are able to satisfy the request from the vmemChunksQuota */
	assert_true(waivedChunks == 2);
	assert_true(vmemChunksQuota == oldVmemChunksQuota);
	assert_true(trackedVmemChunks == vmemChunksQuota - 1);

	/* This should reset the waivedChunks as a new chunk will be allocated within the vmem limit */
	VmemTracker_ReserveVmem(CHUNKS_TO_BYTES(1));
	assert_true(waivedChunks == 0);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test_setup_teardown(test__VmemTracker_ReserveVmem__IgnoreWhenUninitialized, VmemTrackerTestSetup, VmemTrackerTestTeardown),
		unit_test_setup_teardown(test__VmemTracker_ReserveVmem__FailForInvalidSize, VmemTrackerTestSetup, VmemTrackerTestTeardown),
		unit_test_setup_teardown(test__VmemTracker_ReserveVmem__CacheSanity, VmemTrackerTestSetup, VmemTrackerTestTeardown),
		unit_test_setup_teardown(test__VmemTracker_ReserveVmem__TrackedBytesSanity, VmemTrackerTestSetup, VmemTrackerTestTeardown),
		unit_test_setup_teardown(test__VmemTracker_ReserveVmem__TrackedBytesSanityForRedzoneDetection, VmemTrackerTestSetup, VmemTrackerTestTeardown),
		unit_test_setup_teardown(test__VmemTracker_ReserveVmem__OOMLoggingBeforeReservation, VmemTrackerTestSetup, VmemTrackerTestTeardown),
		unit_test_setup_teardown(test__VmemTracker_ShmemInit__InitSegmentVmemLimitUnderPostmaster, VmemTrackerTestSetup, VmemTrackerTestTeardown),
		unit_test_setup_teardown(test__VmemTracker_ShmemInit__InitSegmentVmemLimitInPostmaster, VmemTrackerTestSetup, VmemTrackerTestTeardown),
		unit_test_setup_teardown(test__VmemTracker_ShmemInit__QuotaCalculation, VmemTrackerTestSetup, VmemTrackerTestTeardown),
		unit_test_setup_teardown(test__VmemTracker_Init__InitializesOthers, VmemTrackerTestSetup, VmemTrackerTestTeardown),
		unit_test_setup_teardown(test__VmemTracker_Shutdown__ReleasesAllVmem, VmemTrackerTestSetup, VmemTrackerTestTeardown),
		unit_test_setup_teardown(test__VmemTracker_RequestWaiver__WaiveEnforcement, VmemTrackerTestSetup, VmemTrackerTestTeardown),
	};

	return run_tests(tests);
}
