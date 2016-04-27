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
#include "storage/pg_sema.h"
#include "storage/ipc.h"
#include "utils/palloc.h"
#include "utils/memutils.h"

#include "cdb/cdbvars.h"
#include "utils/debugbreak.h"
#include "utils/simex.h"
#include "utils/vmem_tracker.h"
#include "utils/session_state.h"
#include "utils/gp_atomic.h"

#define SHMEM_OOM_TIME "last vmem oom time"

/*
 * Last OOM time of a segment. Maintained in shared memory.
 */
volatile OOMTimeType* segmentOOMTime = 0;

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

	gp_mp_inited = false;

	VmemTracker_Shutdown();
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

	Insist(MemoryProtection_IsOwnerThread());
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
	else
	{
		Assert(!"Unknown memory failure error code");
	}

	RedZoneHandler_LogVmemUsageOfAllSessions();
	MemoryAccounting_SaveToLog();
	MemoryContextStats(TopMemoryContext);

	if(coredump_on_memerror)
	{
		/*
		 * Generate a core dump by writing to NULL pointer
		 */
		*(int *) NULL = ec;
	}

	if (ec == MemoryFailure_VmemExhausted)
	{
		/* Hit MOP limit */
		ereport(ERROR, (errcode(ERRCODE_GP_MEMPROT_KILL),
				errmsg("Out of memory"),
				errdetail("VM Protect failed to allocate %d bytes, %d MB available",
						sz, VmemTracker_GetAvailableVmemMB()
				)
		));
	}
	else if (ec == MemoryFailure_QueryMemoryExhausted)
	{
		/* Hit MOP limit */
		ereport(ERROR, (errcode(ERRCODE_GP_MEMPROT_KILL),
				errmsg("Out of memory"),
				errdetail("Per-query VM protect limit reached: current limit is %d kB, requested %d bytes, available %d MB",
						gp_vmem_limit_per_query, sz, VmemTracker_GetAvailableQueryVmemMB()
				)
		));
	}
	else if (ec == MemoryFailure_SystemMemoryExhausted)
	{
		ereport(ERROR, (errcode(ERRCODE_GP_MEMPROT_KILL),
				errmsg("Out of memory"),
				errdetail("VM protect failed to allocate %d bytes from system, VM Protect %d MB available",
						sz, VmemTracker_GetAvailableVmemMB()
				)
		));
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

#define UsablePointerOfMalloc(malloc_start)	\
					((void*)(((char *)(malloc_start)) + sizeof(size_t)))

#define MallocStartFromUsablePointer(usable_pointer)	\
					((void*)(((char *)(usable_pointer)) - sizeof(size_t)))

#define GetUsableSize(malloc_pointer)	\
					(*(size_t *)(malloc_pointer))

#define SetUsableSize(malloc_pointer, size)	\
					(*((size_t*)malloc_pointer) = size)

#define MallocSizeFromUsableSize(size) \
					(size + sizeof(size_t))


static void* malloc_and_store_size(size_t size)
{
	size_t malloc_size = MallocSizeFromUsableSize(size);
	void* malloc_pointer = malloc(malloc_size);
	if (NULL == malloc_pointer)
	{
		return NULL;
	}

	SetUsableSize(malloc_pointer, size);
	return UsablePointerOfMalloc(malloc_pointer);
}

static void* realloc_and_store_size(void* usable_pointer, size_t new_usable_size)
{
	void* realloc_pointer = realloc(MallocStartFromUsablePointer(usable_pointer), MallocSizeFromUsableSize(new_usable_size));

	if (NULL == realloc_pointer)
	{
		return NULL;
	}
	SetUsableSize(realloc_pointer, new_usable_size);
	return UsablePointerOfMalloc(realloc_pointer);
}

static void free_with_stored_size(void *usable_pointer)
{
	void* malloc_pointer = MallocStartFromUsablePointer(usable_pointer);
	size_t usable_size = GetUsableSize(malloc_pointer);
	Assert(usable_size > 0);
	free(malloc_pointer);
	VmemTracker_ReleaseVmem(MallocSizeFromUsableSize(usable_size));
}

/* Reserves vmem from vmem tracker and allocates memory by calling malloc/calloc */
static void *gp_malloc_internal(int64 requested_size)
{
	void *usable_pointer = NULL;

	size_t size_with_overhead = MallocSizeFromUsableSize(requested_size);

	Assert(size_with_overhead >= 0 && size_with_overhead <= 0x7fffffff);

	MemoryAllocationStatus stat = VmemTracker_ReserveVmem(size_with_overhead);
	if (MemoryAllocation_Success == stat)
	{
		usable_pointer = malloc_and_store_size(requested_size);
		Assert(GetUsableSize(MallocStartFromUsablePointer(usable_pointer)) == requested_size);

#ifdef USE_TEST_UTILS
		if (gp_simex_init && gp_simex_run && gp_simex_class == SimExESClass_OOM && usable_pointer)
		{
			SimExESSubClass subclass = SimEx_CheckInject();
			if (subclass == SimExESSubClass_OOM_ReturnNull)
			{
				free(MallocStartFromUsablePointer(usable_pointer));
				usable_pointer = NULL;
			}
		}
#endif

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

	ret = malloc_and_store_size(sz);
	return ret;
}

/* Reallocates memory, respecting vmem protection, if enabled */
void *gp_realloc(void *ptr, int64 new_size)
{
	Assert(!gp_mp_inited || MemoryProtection_IsOwnerThread());

	void *ret = NULL;

	if(!gp_mp_inited)
	{
		ret = realloc_and_store_size(ptr, new_size);
		return ret;
	}

	size_t old_size = GetUsableSize(ptr);
	int64 size_diff = (new_size - old_size);

	if(new_size <= old_size || MemoryAllocation_Success == VmemTracker_ReserveVmem(size_diff))
	{
		ret = realloc_and_store_size(ptr, new_size);

#ifdef USE_TEST_UTILS
		if (gp_simex_init && gp_simex_run && gp_simex_class == SimExESClass_OOM && ret)
		{
			SimExESSubClass subclass = SimEx_CheckInject();
			if (subclass == SimExESSubClass_OOM_ReturnNull)
			{
				free(MallocStartFromUsablePointer(ret));
				ret = NULL;
			}
		}
#endif

		if(!ret)
		{
			Assert(0 < size_diff);
			VmemTracker_ReleaseVmem(size_diff);

			gp_failed_to_alloc(MemoryFailure_SystemMemoryExhausted, 0, old_size);
			return NULL;
		}

		return ret;
	}

	return NULL; 
}

/* Frees memory and releases vmem accordingly */
void gp_free(void *ptr)
{
	Assert(!gp_mp_inited || MemoryProtection_IsOwnerThread());
	free_with_stored_size(ptr);
}
