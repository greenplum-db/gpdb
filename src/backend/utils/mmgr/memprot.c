/* 
 * memprot.c
 *		Memory allocation under greenplum memory allocation.
 * 
 * Copyright(c) 2008, Greenplum Inc.
 * 
 * We wrap up calls to malloc/realloc/free with our own accounting
 * so that we will make sure a postgres process will not go beyond
 * its allowed quota
 */

#include "postgres.h"

#include <signal.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/time.h>

#ifdef HAVE_SYS_IPC_H
#include <sys/ipc.h>
#endif
#ifdef HAVE_SYS_SEM_H
#include <sys/sem.h>
#endif
#ifdef HAVE_KERNEL_OS_H
#include <kernel/OS.h>
#endif

#include "miscadmin.h"
#include "port/atomics.h"
#include "storage/pg_sema.h"
#include "storage/ipc.h"
#include "storage/shmem.h"
#include "utils/palloc.h"
#include "utils/memutils.h"

#include "cdb/cdbvars.h"
#include "utils/vmem_tracker.h"
#include "utils/session_state.h"
#include "utils/gp_alloc.h"
#include "utils/guc.h"

#define SHMEM_OOM_TIME "last vmem oom time"
#define MAX_REQUESTABLE_SIZE 0x7fffffff

static void gp_failed_to_alloc(MemoryAllocationStatus ec, int en, int sz);

/*
 * Last OOM time of a segment. Maintained in shared memory.
 */
volatile OOMTimeType *segmentOOMTime = 0;

/*
 * We don't report memory usage of current process multiple times
 * for a single OOM event. This variable saves the last time we reported
 * OOM. If this time is not greater than the segmentOOMTime, we don't
 * report memory usage.
 */
volatile OOMTimeType alreadyReportedOOMTime = 0;

/*
 * Time when we started tracking for OOM in this process.
 * If this time is greater than segmentOOMTime, we don't
 * consider this process as culpable for that OOM event.
 */
volatile OOMTimeType oomTrackerStartTime = 0;
/*
 * The thread that owns the memory protection. The owner thread is the
 * one that initializes the memory protection by calling GPMemoryProtect_Init().
 * This should be the main thread. No other thread is allowed to call gp_malloc
 * or any memory protection related functions.
 */
#ifndef _WIN32
static pthread_t memprotOwnerThread = (pthread_t)0;
#else
static pthread_t memprotOwnerThread = {0,0};
#endif
/* Is memory protection enabled? */
bool gp_mp_inited = false;

/*
 * UpdateTimeAtomically
 *
 * Updates a OOMTimeType variable atomically, using compare_and_swap_*
 */
void UpdateTimeAtomically(volatile OOMTimeType* time_var)
{
	bool updateCompleted = false;

	OOMTimeType newOOMTime;

	while (!updateCompleted)
	{
#if defined(__x86_64__)
		newOOMTime = GetCurrentTimestamp();
#else
		struct timeval curTime;
		gettimeofday(&curTime, NULL);

		newOOMTime = (uint32)curTime.tv_sec;
#endif
		OOMTimeType oldOOMTime = *time_var;

#if defined(__x86_64__)
		updateCompleted = pg_atomic_compare_exchange_u64((pg_atomic_uint64 *)time_var,
				(uint64 *)&oldOOMTime,
				(uint64)newOOMTime);
#else
		updateCompleted = pg_atomic_compare_exchange_u32((pg_atomic_uint32 *)time_var,
				(uint32 *)&oldOOMTime,
				(uint32)newOOMTime);
#endif
	}
}

/*
 * InitPerProcessOOMTracking
 *
 * Initializes per-process OOM tracking data structures.
 */
void InitPerProcessOOMTracking()
{
	Assert(NULL != segmentOOMTime);

	alreadyReportedOOMTime = 0;

#if defined(__x86_64__)
	oomTrackerStartTime = GetCurrentTimestamp();
#else
	struct timeval curTime;
	gettimeofday(&curTime, NULL);

	oomTrackerStartTime = (uint32)curTime.tv_sec;
#endif
}

/* Initializes shared memory data structures */
void GPMemoryProtect_ShmemInit()
{
	Assert(!gp_mp_inited);

	VmemTracker_ShmemInit();

	bool		isSegmentOOMTimeInShmem = false;

	segmentOOMTime = (OOMTimeType *)
						ShmemInitStruct(SHMEM_OOM_TIME,
								sizeof(OOMTimeType),
								&isSegmentOOMTimeInShmem);

	Assert(isSegmentOOMTimeInShmem || !IsUnderPostmaster);
	Assert(NULL != segmentOOMTime);

	if (!IsUnderPostmaster)
	{
		/*
		 * Initializing segmentOOMTime to 0 ensures that no
		 * process dumps memory usage, unless we hit an OOM
		 * and update segmentOOMTime to a proper value.
		 */
		*segmentOOMTime = 0;
	}
}

/* Initializes per-process data structures and enables memory protection */
void GPMemoryProtect_Init()
{
	Assert(!gp_mp_inited);

	if (NULL == MySessionState)
	{
		/* Only database sessions have memory protection enabled */
		return;
	}

	/*
	 * Lock in the current thread that is initializing the memory protection
	 * so that no other thread can use memory protection later on.
	 */
	memprotOwnerThread = pthread_self();

	VmemTracker_Init();

	Assert(NULL != segmentOOMTime);

	InitPerProcessOOMTracking();

	gp_mp_inited = true;
}

/* Disables memory protection */
void GPMemoryProtect_Shutdown()
{
	if (NULL == MySessionState)
	{
		Assert(!gp_mp_inited);
		/* Only database sessions have memory protection enabled */
		return;
	}

	VmemTracker_UnregisterStartupMemory();

	gp_mp_inited = false;

	VmemTracker_Shutdown();
}

/*
 * Add the per-process startup committed memory to vmem tracker.
 *
 * Postgresql suggests setting vm.overcommit_memory to 2, so system level OOM
 * is triggered by committed memory size.  Each postgres process has several MB
 * committed memory since it being forked, in practice the size can be 6~16 MB,
 * depends on build-time and runtime configurations.
 *
 * These memory were not tracked by vmem tracker however, as a result an idle
 * (just gets launched, or just finished execution) QE process has 0 chunks in
 * track, but it might have 16MB committed memory.  The vmem tracker protect
 * limit will never be reached no matter how many such processes there are, but
 * when there are enough such processes the system level OOM will be triggered,
 * which will lead to unpredictable failures on not only postgres but also all
 * the other processes on the system.
 *
 * To prevent this worst case we add a startup cost to the vmem tracker, so the
 * vmem tracker OOM will happen instead of the system one, this is less harmful
 * and easier to handle.  The startup cost, however, is a hard coded value from
 * practice for now, we may want to make a better estimation at runtime in the
 * future.  Another thing to improve is that this startup cost should only be
 * added when vm.overcommit_memory is 2.
 */
void
GPMemoryProtect_TrackStartupMemory(void)
{
	MemoryAllocationStatus status;
	int64		bytes = 0;

	Assert(gp_mp_inited);

	/*
	 * When compile without ORCA a postgress process will commit 6MB, this
	 * includes the memory allocated before vmem tracker initialization.
	 */
	bytes += 6L << BITS_IN_MB;

#ifdef USE_ORCA
	/* When compile with ORCA it will commit 6MB more */
	bytes += 6L << BITS_IN_MB;

	/*
	 * When optimizer_use_gpdb_allocators is on, at least 2MB of above will be
	 * tracked by vmem tracker later, so do not recount them.  This GUC is not
	 * available until gpdb 5.1.0 .
	 */
#if GP_VERSION_NUM >= 50100
	if (optimizer_use_gpdb_allocators)
		bytes -= 2L << BITS_IN_MB;
#endif  /* GP_VERSION_NUM */
#endif  /* USE_ORCA */

	/* Leave some buffer for extensions like metrics_collector */
	bytes += 2L << BITS_IN_MB;

	/* Register the startup memory */
	status = VmemTracker_RegisterStartupMemory(bytes);
	if (status != MemoryAllocation_Success)
		gp_failed_to_alloc(status, 0, bytes);
}

/*
 *  Returns true if the current thread is the owner thread, i.e.,
 * the thread that initialized the memory protection subsystem
 * by calling GPMemoryProtect_Init()
 */
bool
MemoryProtection_IsOwnerThread()
{
	return pthread_equal(memprotOwnerThread, pthread_self());
}

/*
 * gp_failed_to_alloc is called upon an OOM. We can have either a VMEM
 * limited OOM (i.e., the system still has memory, but we ran out of either
 * per-query VMEM limit or segment VMEM limit) or a true OOM, where the
 * malloc returns a NULL pointer.
 *
 * This function logs OOM details, such as memory allocation/deallocation/peak.
 * It also updates segment OOM time by calling UpdateTimeAtomically().
 *
 * Parameters:
 *
 * 		ec: error code; indicates what type of OOM event happend (system, VMEM, per-query VMEM)
 * 		en: the last seen error number as retrieved by calling __error() or similar function
 * 		sz: the requested allocation size for which we reached OOM
 */
static void gp_failed_to_alloc(MemoryAllocationStatus ec, int en, int sz)
{
	/*
	 * A per-query vmem overflow shouldn't trigger a segment-wide
	 * OOM reporting.
	 */
	if (MemoryFailure_QueryMemoryExhausted != ec)
	{
		UpdateTimeAtomically(segmentOOMTime);
	}

	UpdateTimeAtomically(&alreadyReportedOOMTime);

	/* Request 1 MB of waiver for processing error */
	VmemTracker_RequestWaiver(1024 * 1024);

	Assert(MemoryProtection_IsOwnerThread());
	if (ec == MemoryFailure_QueryMemoryExhausted)
	{
		elog(LOG, "Logging memory usage for reaching per-query memory limit");
	}
	else if (ec == MemoryFailure_VmemExhausted)
	{
		elog(LOG, "Logging memory usage for reaching Vmem limit");
	}
	else if (ec == MemoryFailure_SystemMemoryExhausted)
	{
		/*
		 * The system memory is exhausted and malloc returned a null pointer.
		 * Although elog switches to ErrorContext, which already
		 * has pre-allocated space, we are not risking any new allocation until
		 * we dump the memory context and memory accounting tree. We are therefore
		 * printing the log message header using write_stderr.
		 */
		write_stderr("Logging memory usage for reaching system memory limit");
	}
	else if (ec == MemoryFailure_ResourceGroupMemoryExhausted)
	{
		elog(LOG, "Logging memory usage for reaching resource group limit");
	}
	else
		elog(ERROR, "Unknown memory failure error code");

	RedZoneHandler_LogVmemUsageOfAllSessions();
	MemoryContextStats(TopMemoryContext);

	if (coredump_on_memerror)
	{
		/*
		 * Generate a core dump by writing to NULL pointer
		 */
		*(volatile int *) NULL = ec;
	}

	if (ec == MemoryFailure_VmemExhausted)
	{
		/* Hit MOP limit */
		ereport(ERROR, (errcode(ERRCODE_GP_MEMPROT_KILL),
				errmsg("Out of memory"),
				errdetail("Vmem limit reached, failed to allocate %d bytes from tracker, which has %d MB available",
						sz, VmemTracker_GetAvailableVmemMB()
				)
		));
	}
	else if (ec == MemoryFailure_QueryMemoryExhausted)
	{
		/* Hit MOP limit */
		ereport(ERROR, (errcode(ERRCODE_GP_MEMPROT_KILL),
				errmsg("Out of memory"),
				errdetail("Per-query memory limit reached: current limit is %d kB, requested %d bytes, has %d MB available for this query",
						gp_vmem_limit_per_query, sz, VmemTracker_GetAvailableQueryVmemMB()
				)
		));
	}
	else if (ec == MemoryFailure_SystemMemoryExhausted)
	{
		ereport(ERROR, (errcode(ERRCODE_GP_MEMPROT_KILL),
				errmsg("Out of memory"),
				errdetail("System memory limit reached, failed to allocate %d bytes from system", sz)
		));
	}
	else if (ec == MemoryFailure_ResourceGroupMemoryExhausted)
	{
		ereport(ERROR, (errcode(ERRCODE_GP_MEMPROT_KILL),
				errmsg("Out of memory"),
				errdetail("Resource group memory limit reached")));
	}
	else
	{
		/* SemOp error.  */
		ereport(ERROR, (errcode(ERRCODE_GP_MEMPROT_KILL),
				errmsg("Failed to allocate memory under virtual memory protection"),
				errdetail("Error %d, errno %d, %s", ec, en, strerror(en))
		));
	}
}

/*
 * malloc the requested "size" and additional memory for metadata and store header and/or
 * footer metadata information. Caller is in charge to update Vmem counter accordingly.
 */
static void *malloc_and_store_metadata(size_t size)
{
	size_t malloc_size = UserPtrSize_GetVmemPtrSize(size);
	void *malloc_pointer = malloc(malloc_size);
	if (NULL == malloc_pointer)
	{
		/*
		 * A NULL pointer from the underlying allocator will be returned as-is
		 * and the caller is supposed to convert it to error or other ways it
		 * sees fit.
		 */
		return NULL;
	}
	VmemPtr_Initialize((VmemHeader*) malloc_pointer, size);
	return VmemPtrToUserPtr((VmemHeader*) malloc_pointer);
}

/*
 * realloc the requested "size" and additional memory for metadata and store header and/or
 * footer metadata information. Caller is in charge to update Vmem counter accordingly.
 */
static void *realloc_and_store_metadata(void *usable_pointer, size_t new_usable_size)
{
	Assert(UserPtr_GetVmemPtr(usable_pointer)->checksum == VMEM_HEADER_CHECKSUM);
	Assert(*VmemPtr_GetPointerToFooterChecksum(UserPtr_GetVmemPtr(usable_pointer)) == VMEM_FOOTER_CHECKSUM);

	void *realloc_pointer = realloc(UserPtr_GetVmemPtr(usable_pointer), UserPtrSize_GetVmemPtrSize(new_usable_size));

	if (NULL == realloc_pointer)
	{
		return NULL;
	}
	VmemPtr_Initialize((VmemHeader*) realloc_pointer, new_usable_size);
	return VmemPtrToUserPtr((VmemHeader*) realloc_pointer);
}

/* Reserves vmem from vmem tracker and allocates memory by calling malloc/calloc */
static void *gp_malloc_internal(int64 requested_size)
{
	void *usable_pointer = NULL;

	Assert(requested_size > 0);
	size_t size_with_overhead = UserPtrSize_GetVmemPtrSize(requested_size);

	Assert(size_with_overhead <= MAX_REQUESTABLE_SIZE);

	MemoryAllocationStatus stat = VmemTracker_ReserveVmem(size_with_overhead);
	if (MemoryAllocation_Success == stat)
	{
		usable_pointer = malloc_and_store_metadata(requested_size);
		Assert(usable_pointer == NULL || VmemPtr_GetUserPtrSize(UserPtr_GetVmemPtr(usable_pointer)) == requested_size);

		if(!usable_pointer)
		{
			VmemTracker_ReleaseVmem(size_with_overhead);
			gp_failed_to_alloc(MemoryFailure_SystemMemoryExhausted, 0, size_with_overhead);

			return NULL;
		}

		return usable_pointer;
	}

	gp_failed_to_alloc(stat, 0, size_with_overhead);

	return NULL;
}

/*
 * Allocates sz bytes. If memory protection is enabled, this method
 * uses gp_malloc_internal to reserve vmem and allocate memory.
 */
void *gp_malloc(int64 sz)
{
	Assert(!gp_mp_inited || MemoryProtection_IsOwnerThread());

	void *ret;

	if(gp_mp_inited)
	{
		return gp_malloc_internal(sz);
	}

	ret = malloc_and_store_metadata(sz);
	return ret;
}

/* Reallocates memory, respecting vmem protection, if enabled */
void *gp_realloc(void *ptr, int64 new_size)
{
	Assert(!gp_mp_inited || MemoryProtection_IsOwnerThread());
	Assert(NULL != ptr);

	void *ret = NULL;

	if(!gp_mp_inited)
	{
		ret = realloc_and_store_metadata(ptr, new_size);
		return ret;
	}

	size_t old_size = UserPtr_GetUserPtrSize(ptr);
	int64 size_diff = (new_size - old_size);

	MemoryAllocationStatus stat = MemoryAllocation_Success;
	if(new_size <= old_size || MemoryAllocation_Success == (stat =  VmemTracker_ReserveVmem(size_diff)))
	{
		ret = realloc_and_store_metadata(ptr, new_size);

		if(!ret)
		{
			/*
			 * There is no guarantee that realloc would not fail during a shrinkage.
			 * But, we haven't touched Vmem at all for a shrinkage. So, nothing to undo.
			 */
			if (size_diff > 0)
			{
				VmemTracker_ReleaseVmem(size_diff);
			}

			gp_failed_to_alloc(MemoryFailure_SystemMemoryExhausted, 0, new_size);
			return NULL;
		}

		if (size_diff < 0)
		{
			/*
			 * As there is no guarantee that a shrinkage during realloc would not fail,
			 * we follow a lazy approach of adjusting VMEM during shrinkage. Upon a
			 * successful realloc, we finally release a VMEM, which should virtually never fail.
			 */
			VmemTracker_ReleaseVmem(-1 * size_diff);
		}

		return ret;
	}

	Assert(MemoryAllocation_Success != stat);

	gp_failed_to_alloc(stat, 0, new_size);
	return NULL; 
}

/* Frees memory and releases vmem accordingly */
void gp_free(void *user_pointer)
{
	Assert(!gp_mp_inited || MemoryProtection_IsOwnerThread());
	Assert(NULL != user_pointer);

	void *malloc_pointer = UserPtr_GetVmemPtr(user_pointer);
	size_t usable_size = VmemPtr_GetUserPtrSize((VmemHeader*) malloc_pointer);
	Assert(usable_size > 0);
	UserPtr_VerifyChecksum(user_pointer);
	free(malloc_pointer);
	VmemTracker_ReleaseVmem(UserPtrSize_GetVmemPtrSize(usable_size));
}
