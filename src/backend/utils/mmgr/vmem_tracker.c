/*-------------------------------------------------------------------------
 *
 * vmem_tracker.c
 *	 Implementation of vmem tracking that provides a mechanism to enforcing
 *	 a limit on the total memory consumption per-segment. The vmem tracker
 *	 also collaborates with red zone handler and runaway cleaner to determine
 *	 sessions that consume excessive vmem and cleans up such sessions by forcing
 *	 them to release their memory.
 *
 * Copyright (c) 2014-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/mmgr/vmem_tracker.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "cdb/cdbvars.h"
#include "miscadmin.h"
#include "port/atomics.h"
#include "utils/faultinjection.h"
#include "utils/vmem_tracker.h"
#include "utils/resource_manager.h"
#include "utils/session_state.h"

/* External dependencies within the runaway cleanup framework */
extern void EventVersion_ShmemInit(void);
extern void RedZoneHandler_ShmemInit();
extern void IdleTracker_ShmemInit(void);
extern void RunawayCleaner_Init(void);
extern void IdleTracker_Init(void);
extern void IdleTracker_Shutdown(void);

/* Number of bits in one MB of memory */
#define SHMEM_AVAILABLE_VMEM "available vmem on the segment"

#define CHUNKS_TO_MB(chunks) ((chunks) << (VmemTracker_GetChunkSizeInBits() - BITS_IN_MB))
#define MB_TO_CHUNKS(mb) ((mb) >> (VmemTracker_GetChunkSizeInBits() - BITS_IN_MB))
#define CHUNKS_TO_BYTES(chunks) (((int64)chunks) << VmemTracker_GetChunkSizeInBits())
#define BYTES_TO_CHUNKS(bytes) ((bytes) >> VmemTracker_GetChunkSizeInBits())
#define BYTES_TO_MB(bytes) ((bytes) >> BITS_IN_MB)

/* Number of Vmem chunks tracked by this process */
static int32 trackedVmemChunks = 0;
/* Maximum number of vmem chunks tracked by this process */
static int32 maxVmemChunksTracked = 0;
/* Number of bytes tracked (i.e., allocated under the tutelage of vmem tracker) */
static int64 trackedBytes = 0;

/* Vmem quota in chunk unit */
static int32 vmemChunksQuota = 0;
/*
 * Chunk size in bits. By default a chunk is 1MB, but it can be larger
 * depending on the vmem quota.
 */
static int chunkSizeInBits = BITS_IN_MB;
/* Is vmem tracker was initialized? If not, then we don't track. */
bool vmemTrackerInited = false;

/*
 * A derived parameter from gp_vmem_limit_per_query in chunks unit,
 * considering the current chunk size.
 */
static int32 maxChunksPerQuery = 0;

/*
 * How many chunks are currently waived from checking (e.g., for error
 * handling)
 */
static int32 waivedChunks = 0;

/*
 * Consumed vmem on the segment.
 */
volatile int32 *segmentVmemChunks = NULL;

static void ReleaseAllVmemChunks(void);
static int32 VmemTracker_GetMaxChunksPerQuery(void);

/*
 * Initializes the shared memory states of the vmem tracker. This
 * will also initialize the shared memory states of event version
 * provider, red zone handler and idle tracker.
 */
void
VmemTracker_ShmemInit()
{
	Assert(!vmemTrackerInited);
	trackedVmemChunks = 0;
	maxVmemChunksTracked = 0;
	trackedBytes = 0;

	bool		alreadyInShmem = false;

	segmentVmemChunks = (int32 *)
								ShmemInitStruct(SHMEM_AVAILABLE_VMEM,
										sizeof(int32),
										&alreadyInShmem);
	Assert(alreadyInShmem || !IsUnderPostmaster);

	Assert(NULL != segmentVmemChunks);

	if(!IsUnderPostmaster)
	{
		Assert(chunkSizeInBits == BITS_IN_MB);

		vmemChunksQuota = gp_vmem_protect_limit;
		/*
		 * If vmem is larger than 16GB (i.e., 16K MB), we make the chunks bigger
		 * so that the vmem limit in chunks unit is not larger than 16K.
		 */
		while(vmemChunksQuota > (16 * 1024))
		{
			chunkSizeInBits++;
			vmemChunksQuota >>= 1;
		}

		/*
		 * gp_vmem_limit_per_query is in kB. So, first convert it to MB, and then shift it
		 * to adjust for cases where we enlarged our chunk size
		 */
		maxChunksPerQuery = ceil(gp_vmem_limit_per_query / (1024.0 * (1 << (chunkSizeInBits - BITS_IN_MB))));

		/* Initialize the sub-systems */
		EventVersion_ShmemInit();
		RedZoneHandler_ShmemInit();
		IdleTracker_ShmemInit();

		*segmentVmemChunks = 0;
	}
}

/*
 * Initializes the vmem tracker per-process states. This will also initialize
 * the per-process states of runaway cleaner and idle tracker.
 */
void
VmemTracker_Init()
{
	Assert(NULL != MySessionState);
	Assert(!vmemTrackerInited);
	Assert(trackedVmemChunks == 0);
	Assert(maxVmemChunksTracked == 0);
	Assert(trackedBytes == 0);

	/*
	 * Even though asserts have passed, make sure that in production system
	 * we still have 0 values in our counters.
	 */
	trackedVmemChunks = 0;
	maxVmemChunksTracked = 0;
	trackedBytes = 0;

	Assert(0 < vmemChunksQuota || Gp_role != GP_ROLE_EXECUTE);
	Assert(gp_vmem_limit_per_query == 0 || (maxChunksPerQuery != 0 && maxChunksPerQuery < gp_vmem_limit_per_query));

	Assert(NULL != segmentVmemChunks);

	IdleTracker_Init();
	RunawayCleaner_Init();

	vmemTrackerInited = true;
}

/*
 * Disables vmem tracking and releases all the memory that this
 * (i.e., current process) tracker is tracking. This will also
 * shutdown the idle tracker.
 */
void
VmemTracker_Shutdown()
{
	vmemTrackerInited = false;

	ReleaseAllVmemChunks();
	Assert(*segmentVmemChunks >= 0);

	IdleTracker_Shutdown();
}

/*
 * Resets the maximum reserved vmem to the current reserved vmem
 */
void
VmemTracker_ResetMaxVmemReserved()
{
	maxVmemChunksTracked = trackedVmemChunks;
}

/*
 * Reserve 'num_chunks_to_reserve' number of chunks for current process. The
 * reservation is validated against segment level vmem quota.
 */
static MemoryAllocationStatus
VmemTracker_ReserveVmemChunks(int32 numChunksToReserve)
{
	Assert(vmemTrackerInited);
	Assert(NULL != MySessionState);

	Assert(0 < numChunksToReserve);
	int32 total = pg_atomic_add_fetch_u32((pg_atomic_uint32 *)&MySessionState->sessionVmem, numChunksToReserve);
	Assert(total > (int32) 0);


	/* We don't support vmem usage from non-owner thread */
	Assert(MemoryProtection_IsOwnerThread());

	bool waiverUsed = false;

	if (!ResGroupReserveMemory(numChunksToReserve, waivedChunks, &waiverUsed))
	{
		pg_atomic_sub_fetch_u32((pg_atomic_uint32 *)&MySessionState->sessionVmem, numChunksToReserve);
		return MemoryFailure_ResourceGroupMemoryExhausted;
	}

	/*
	 * Query vmem quota exhausted, so rollback the reservation and return error.
	 * For non-QE processes and processes in critical section, we don't enforce
	 * VMEM, but we do track the usage.
	 */
	int32 memLimitPerQuery = VmemTracker_GetMaxChunksPerQuery();

	if (memLimitPerQuery != 0 && total > memLimitPerQuery &&
			Gp_role == GP_ROLE_EXECUTE && CritSectionCount == 0)
	{
		if (total > memLimitPerQuery + waivedChunks)
		{
			/* Revert the reserved space, but don't revert the prev_alloc as we have already set the firstTime to false */
			pg_atomic_sub_fetch_u32((pg_atomic_uint32 *)&MySessionState->sessionVmem, numChunksToReserve);
			/* Revert resgroup memory reservation */
			ResGroupReleaseMemory(numChunksToReserve);
			return MemoryFailure_QueryMemoryExhausted;
		}
		waiverUsed = true;
	}

	/* Now reserve vmem at segment level */
	int32 new_vmem = pg_atomic_add_fetch_u32((pg_atomic_uint32 *)segmentVmemChunks, numChunksToReserve);
	int32 vmemLimitChunks = VmemTracker_GetVmemLimitChunks();

	/*
	 * If segment vmem is exhausted, rollback query level reservation. For non-QE
	 * processes and processes in critical section, we don't enforce VMEM, but we
	 * do track the usage.
	 */
	if (new_vmem > vmemLimitChunks &&
			Gp_role == GP_ROLE_EXECUTE && CritSectionCount == 0)
	{
		if (new_vmem > vmemLimitChunks + waivedChunks)
		{
			/* Revert query memory reservation */
			pg_atomic_sub_fetch_u32((pg_atomic_uint32 *)&MySessionState->sessionVmem, numChunksToReserve);
			/* Revert vmem reservation */
			pg_atomic_sub_fetch_u32((pg_atomic_uint32 *)segmentVmemChunks, numChunksToReserve);
			/* Revert resgroup memory reservation */
			ResGroupReleaseMemory(numChunksToReserve);

			return MemoryFailure_VmemExhausted;
		}
		waiverUsed = true;
	}

	/* The current process now owns additional vmem in this segment */
	trackedVmemChunks += numChunksToReserve;

	maxVmemChunksTracked = Max(maxVmemChunksTracked, trackedVmemChunks);

	if (waivedChunks > 0 && !waiverUsed)
	{
		/*
		 * We have sufficient free memory that we are no longer using the waiver.
		 * Therefore reset the waiver.
		 */
		waivedChunks = 0;
	}

	return MemoryAllocation_Success;
}

/*
 * Releases "reduction" number of chunks to the session and segment vmem counter.
 */
static void
VmemTracker_ReleaseVmemChunks(int reduction)
{
	Assert(0 <= reduction);
	/* We don't support vmem usage from non-owner thread */
	Assert(MemoryProtection_IsOwnerThread());

	pg_atomic_sub_fetch_u32((pg_atomic_uint32 *) segmentVmemChunks, reduction);

	Assert(*segmentVmemChunks >= 0);
	Assert(NULL != MySessionState);
	pg_atomic_sub_fetch_u32((pg_atomic_uint32 *)&MySessionState->sessionVmem, reduction);
	ResGroupReleaseMemory(reduction);
	Assert(0 <= MySessionState->sessionVmem);
	trackedVmemChunks -= reduction;
}

/*
 * Releases all vmem reserved by this process.
 */
static void
ReleaseAllVmemChunks()
{
	VmemTracker_ReleaseVmemChunks(trackedVmemChunks);
	Assert(0 == trackedVmemChunks);
	trackedBytes = 0;
}

/*
 * Returns the available VMEM in "chunks" unit. If the available chunks
 * is less than 0, it return 0.
 */
static int32
VmemTracker_GetNonNegativeAvailableVmemChunks()
{
	int32 usedChunks = *segmentVmemChunks;
	int32 vmemLimitChunks = VmemTracker_GetVmemLimitChunks();
	if (vmemLimitChunks > usedChunks)
	{
		return vmemLimitChunks - usedChunks;
	}
	else
	{
		return 0;
	}
}

/*
 * Returns the available query chunks. If the available chunks
 * is less than 0, it return 0.
 */
static int32
VmemTracker_GetNonNegativeAvailableQueryChunks()
{
	int32 curSessionVmem = MySessionState->sessionVmem;
	int32 memLimitPerQuery = VmemTracker_GetMaxChunksPerQuery();
	if (memLimitPerQuery > curSessionVmem)
	{
		return memLimitPerQuery - curSessionVmem;
	}
	else
	{
		return 0;
	}
}

/* Converts chunks to MB */
int32
VmemTracker_ConvertVmemChunksToMB(int chunks)
{
	return CHUNKS_TO_MB(chunks);
}

/* Converts MB to chunks */
int32
VmemTracker_ConvertVmemMBToChunks(int mb)
{
	return MB_TO_CHUNKS(mb);
}

/* Converts chunks to bytes */
int64
VmemTracker_ConvertVmemChunksToBytes(int chunks)
{
	return CHUNKS_TO_BYTES(chunks);
}

/*
 * Returns the maximum vmem consumed by current process in "chunks" unit.
 */
int64
VmemTracker_GetMaxReservedVmemChunks(void)
{
	Assert(maxVmemChunksTracked >= trackedVmemChunks);
	return maxVmemChunksTracked;
}

/*
 * Returns the maximum vmem consumed by current process in "MB" unit.
 */
int64
VmemTracker_GetMaxReservedVmemMB(void)
{
	Assert(maxVmemChunksTracked >= trackedVmemChunks);
	return CHUNKS_TO_MB(maxVmemChunksTracked);
}

/*
 * Returns the maximum vmem consumed by current process in "bytes" unit.
 */
int64
VmemTracker_GetMaxReservedVmemBytes(void)
{
	Assert(maxVmemChunksTracked >= trackedVmemChunks);
	return CHUNKS_TO_BYTES(maxVmemChunksTracked);
}

/*
 * Returns the vmem limit in "bytes" unit.
 */
int64
VmemTracker_GetVmemLimitBytes(void)
{
	return CHUNKS_TO_BYTES(VmemTracker_GetVmemLimitChunks());
}

/*
 * Returns the vmem limit in "chunks" unit.
 */
int32
VmemTracker_GetVmemLimitChunks(void)
{
	/*
	 * For backend who has vmem tracker initialized and resource
	 * group enabled, the vmem limit is not expected to be used
	 * until resource group is activated, otherwise, there might
	 * be an inconsistency about the vmem limit.
	 */
	AssertImply(vmemTrackerInited && IsResGroupEnabled(),
				IsResGroupActivated());

	return IsResGroupEnabled() ?
		ResGroupGetVmemLimitChunks() : vmemChunksQuota;
}

/*
 * Return the chunk size in bits.
 */
int32
VmemTracker_GetChunkSizeInBits(void)
{
	/*
	 * TODO:
	 * For backend who has vmem tracker initialized and resource
	 * group enabled, the chunk size is not expected to be used
	 * until resource group is activated, otherwise, there might
	 * be an inconsistency about the chunk size.
	 */
	return IsResGroupEnabled() ?
		ResGroupGetVmemChunkSizeInBits() : chunkSizeInBits;
}

/*
 * Returns the vmem  in "chunks" unit.
 */
static int32
VmemTracker_GetMaxChunksPerQuery(void)
{
	return IsResGroupEnabled() ?
		ResGroupGetMaxChunksPerQuery() : maxChunksPerQuery;
}

/*
 * Returns the vmem usage of current process in "chunks" unit.
 */
int32
VmemTracker_GetReservedVmemChunks(void)
{
	return trackedVmemChunks;
}

/*
 * Returns the vmem usage of current process in "bytes" unit.
 */
int64
VmemTracker_GetReservedVmemBytes(void)
{
	return CHUNKS_TO_BYTES(trackedVmemChunks);
}

/*
 * Returns the available VMEM in "bytes" unit
 */
int64
VmemTracker_GetAvailableVmemBytes()
{
	return CHUNKS_TO_BYTES(VmemTracker_GetNonNegativeAvailableVmemChunks());
}

/*
 * Returns the available VMEM in "MB" unit
 */
int32
VmemTracker_GetAvailableVmemMB()
{
	return CHUNKS_TO_MB(VmemTracker_GetNonNegativeAvailableVmemChunks());
}

/*
 * Returns the available per-query VMEM in "MB" unit
 */
int32
VmemTracker_GetAvailableQueryVmemMB()
{
	return CHUNKS_TO_MB(VmemTracker_GetNonNegativeAvailableQueryChunks());
}

/*
 * Reserve newly_requested bytes from the vmem system.
 *
 * For performance reason, this method only reserves in chunk units and if the new
 * request can be met from previous chunk reservation, it does not try to reserve a new
 * chunk.
 */
MemoryAllocationStatus
VmemTracker_ReserveVmem(int64 newlyRequestedBytes)
{
	if (!VmemTrackerIsActivated())
	{
		Assert(0 == trackedVmemChunks);
		return MemoryAllocation_Success;
	}

	Assert(gp_mp_inited);
	Assert(newlyRequestedBytes >= 0);

	trackedBytes += newlyRequestedBytes;

	int32 newszChunk = trackedBytes >> VmemTracker_GetChunkSizeInBits();

	MemoryAllocationStatus status = MemoryAllocation_Success;

	if(newszChunk > trackedVmemChunks)
	{
		/*
		 * Undo trackedBytes, as the VmemTracker_TerminateRunawayQuery() may
		 * not return
		 */
		trackedBytes -= newlyRequestedBytes;

		/*
		 * Detect a runaway session. Moreover, if the current session is deemed
		 * as runaway, start cleanup.
		 *
		 * Caution: this method may not return as it has the potential to call
		 * elog(ERROR, ...).
		 */
		RedZoneHandler_DetectRunawaySession();

		/*
		 * Before reserving further VMEM, check if the current session has a pending
		 * query cancellation or other pending interrupts. This ensures more responsive
		 * interrupt processing, including query cancellation requests without depending
		 * on CHECK_FOR_INTERRUPTS(). In a sense, this is a lightweight CHECK_FOR_INTERRUPTS
		 * as we don't execute BackoffBackendTick() and some runaway detection code.
		 */
		if (vmem_process_interrupt && InterruptPending)
		{
			/* ProcessInterrupts should check for InterruptHoldoffCount and CritSectionCount */
			ProcessInterrupts(__FILE__, __LINE__);
		}

		/*
		 * Redo, as we returned from VmemTracker_TerminateRunawayQuery and
		 * we are successfully reserving this vmem
		 */
		trackedBytes += newlyRequestedBytes;

		/*
		 * Before attempting to reserve vmem, we check if there was any OOM
		 * situation, and report our consumption if there was any. This accurately
		 * tells us our share of fault in an OOM situation.
		 */
		ReportOOMConsumption();

		int32 needChunk = newszChunk - trackedVmemChunks;
		status = VmemTracker_ReserveVmemChunks(needChunk);
	}

	/* Failed to reserve vmem chunks. Revert changes to trackedBytes */
	if (MemoryAllocation_Success != status)
	{
		trackedBytes -= newlyRequestedBytes;
	}

	return status;
}

/*
 * Releases toBeFreedRequested bytes from the vmem system.
 *
 * For performance reason this method accumulates free requests until it has
 * enough bytes to free a whole chunk.
 */
void
VmemTracker_ReleaseVmem(int64 toBeFreedRequested)
{
	if (!vmemTrackerInited ||
	   (IsResGroupEnabled() &&
		!IsResGroupActivated()))
	{
		Assert(0 == trackedVmemChunks);
		return;
	}

	Assert(gp_mp_inited);

	/*
	 * We need this adjustment as GPDB may request to free more VMEM than it reserved, apparently
	 * because a bug somewhere that tries to release vmem for allocations made before the vmem
	 * system was initialized.
	 */
	int64 toBeFreed = Min(trackedBytes, toBeFreedRequested);
	if (0 == toBeFreed)
	{
		Assert(0 == trackedVmemChunks);
		return;
	}

	trackedBytes -= toBeFreed;

	int newszChunk = trackedBytes >> VmemTracker_GetChunkSizeInBits();

	if (newszChunk < trackedVmemChunks)
	{
		int reduction = trackedVmemChunks - newszChunk;

		VmemTracker_ReleaseVmemChunks(reduction);
	}
}

/*
 * Request additional VMEM bytes beyond per-session or system vmem limit for
 * OOM error handling.
 *
 * Note, the waiver_bytes are converted to chunk unit, and ceiled. This
 * means asking for 1 byte would result in 1 chunk, which can be multiples
 * of 1 MB.
 *
 * This method does nothing if the previously requested waiver is at least
 * as large as the newly requested waiver.
 */
void
VmemTracker_RequestWaiver(int64 waiver_bytes)
{
	Assert(gp_mp_inited);
	int chunks = BYTES_TO_CHUNKS(waiver_bytes);

	/* Handle ceiling */
	if (waiver_bytes > CHUNKS_TO_BYTES(chunks))
	{
		chunks += 1;

		Assert(waiver_bytes < CHUNKS_TO_BYTES(chunks));
	}

	waivedChunks = Max(chunks, waivedChunks);
}

/*
 * Reset requested waiver to zero.
 */
void
VmemTracker_ResetWaiver(void)
{
	waivedChunks = 0;
}

/*
 * Returns a bunch of different vmem usage stats such as max vmem usage,
 * current vmem usage, available vmem etc.
 *
 * Parameters:
 * 		reason: The type of stat to return.
 * 		arg: Currently unused, but may be used as input parameter when
 * 			 this method is used as "setter"
 */
int64
VmemTracker_Fault(int32 reason, int64 arg)
{
	switch(reason)
	{
	case GP_FAULT_USER_MP_CONFIG:
		return (int64) gp_vmem_protect_limit;
	case GP_FAULT_USER_MP_ALLOC:
		return (int64) (BYTES_TO_MB(VmemTracker_GetReservedVmemBytes()));
	case GP_FAULT_USER_MP_HIGHWM:
		return VmemTracker_GetMaxReservedVmemMB();
	case GP_FAULT_SEG_AVAILABLE:
		return VmemTracker_GetAvailableVmemMB();
	case GP_FAULT_SEG_SET_VMEMMAX:
		Assert(!"Not yet implemented");
		return -1;
	case GP_FAULT_SEG_GET_VMEMMAX:
		return VmemTracker_GetAvailableVmemMB();
	default:
		elog(ERROR, "GP MP Fault Invalid fault code");
	}

	return -1;
}

bool
VmemTrackerIsActivated(void)
{
	/*
	 * If resource group is enabled, the segment vmem limit
	 * and chunk size are undetermined until resource group
	 * is activated, so before that, the vmem tracker should
	 * stay unactivated.
	 */
	if (IsResGroupEnabled())
	{
		return vmemTrackerInited &&
			IsResGroupActivated();	
	}
	else
	{
		return vmemTrackerInited;
	}
}
