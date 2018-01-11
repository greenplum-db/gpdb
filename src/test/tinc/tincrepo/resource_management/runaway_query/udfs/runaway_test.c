
/*
 * runaway_test.c
 *   Auxiliary functionality to test Runaway Query Termination
 *
 * ---------------------------------------------------------------------
 *
 * The dynamically linked library created from this source can be reference by
 * creating a SQL function that references it. For example,
 *
 * CREATE OR REPLACE FUNCTION gp_workfile_cache_test()
 *   RETURNS bool
 *   AS '$libdir/runaway_termination.so', 'gp_allocate_palloc_test' LANGUAGE C STRICT;
 *
 */

#include "postgres.h"
#include "utils/builtins.h"
#include "fmgr.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "utils/vmem_tracker.h"
#include <unistd.h>

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

/* The types of faults supported by the gp_inject_fault_test UDF */
#define FAULT_TYPE_ERROR 1
#define FAULT_TYPE_FATAL 2
#define FAULT_TYPE_PANIC 3
#define FAULT_TYPE_SEGV 4
#define FAULT_USER_SEGV_CRITICAL 5
#define FAULT_USER_SEGV_LWLOCK 6
#define FAULT_USER_PROCEXIT 7
#define FAULT_USER_ABORT 8

/* GPDB variable needed */ 
extern int Gp_segment;

static char *ptrAllocation = NULL;

Datum gp_allocate_palloc_test(PG_FUNCTION_ARGS);
Datum gp_allocate_palloc_gradual_test(PG_FUNCTION_ARGS);
Datum gp_inject_fault_test(PG_FUNCTION_ARGS);
Datum gp_allocate_top_memory_ctxt_test(PG_FUNCTION_ARGS);
Datum gp_available_vmem_mb_test(PG_FUNCTION_ARGS);


PG_FUNCTION_INFO_V1(gp_allocate_palloc_test);
PG_FUNCTION_INFO_V1(gp_inject_fault_test);
PG_FUNCTION_INFO_V1(gp_allocate_palloc_gradual_test);
PG_FUNCTION_INFO_V1(gp_allocate_top_memory_ctxt_test);
PG_FUNCTION_INFO_V1(gp_available_vmem_mb_test);


/*
 * gp_allocate_palloc_test
 *    Allocates a specified amount of memory using a single palloc call on
 *    the specified segment
 *
 *   content: segid this is applicable to. segid <= -2 means all segments
 *   size: amount of memory to palloc
 *   sleep: number of seconds to sleep after the allocation (0 = disable)
 *   crit_section: allocate in critical section if true
 */
Datum
gp_allocate_palloc_test(PG_FUNCTION_ARGS)
{
	int segno = PG_GETARG_INT32(0);
	int size = PG_GETARG_INT32(1);
	int sleep_sec = PG_GETARG_INT32(2);
	bool crit_section = PG_GETARG_BOOL(3);

	int size_alloc = 0;

	if (segno < -1 || segno == Gp_segment)
	{
		elog(LOG, "Allocating %d bytes %s", size,
				crit_section ? " in critical section" : "");

		if (crit_section)
		{
			START_CRIT_SECTION();
		}

		ptrAllocation = (char *) palloc(size);
		size_alloc = size;

		if (crit_section)
		{
			/*
			 * For critical section allocation, we want to free it right away,
			 * so that we don't trigger the runaway detector.
			 */
			pfree(ptrAllocation);

			END_CRIT_SECTION();
		}

		/*
		 * This doesn't check for interrupts during sleep.
		 * Alternatively, we can use a loop with CHECK_FOR_INTERRUPTS
		 * if we want to check for interrupts.
		 */
		if (sleep_sec > 0)
		{
			elog(LOG, "Sleeping for %d seconds", sleep_sec);
			sleep(sleep_sec);
		}

	}

	PG_RETURN_INT32(size_alloc);

}

/*
 * gp_allocate_top_memory_ctxt_test
 *    Allocates a specified amount of memory using a single palloc call on
 *    the specified segment in the TopMemoryContext
 *
 *   content: segid this is applicable to. segid <= -2 means all segments
 *   size: amount of memory to palloc
 *   sleep: number of seconds to sleep after the allocation (0 = disable)
 */
Datum
gp_allocate_top_memory_ctxt_test(PG_FUNCTION_ARGS)
{
	int segno = PG_GETARG_INT32(0);
	int size = PG_GETARG_INT32(1);
	int sleep_sec = PG_GETARG_INT32(2);

	int size_alloc = 0;

	if (segno < -1 || segno == Gp_segment)
	{
		elog(LOG, "Allocating %d bytes in TopMemoryContext", size);

		ptrAllocation = (char *) MemoryContextAlloc(TopMemoryContext, size);
		size_alloc = size;

		/*
		 * This doesn't check for interrupts during sleep.
		 * Alternatively, we can use a loop with CHECK_FOR_INTERRUPTS
		 * if we want to check for interrupts.
		 */
		if (sleep_sec > 0)
		{
			elog(LOG, "Sleeping for %d seconds", sleep_sec);
			sleep(sleep_sec);
		}

	}

	PG_RETURN_INT32(size_alloc);

}

/*
 * gp_allocate_palloc_gradual_test
 *    Allocates a specified amount of memory using many palloc calls of 10MB
 *    on the specified segment
 *
 *
 *   content: segid this is applicable to. segid <= -2 means all segments
 *   size: amount of memory to palloc
 *   sleep: number of seconds to sleep after the allocation (0 = disable)
 */
Datum
gp_allocate_palloc_gradual_test(PG_FUNCTION_ARGS)
{
	int segno = PG_GETARG_INT32(0);
	int size = PG_GETARG_INT32(1);
	int sleep_sec = PG_GETARG_INT32(2);

	int size_alloc = 0;

	/* Allocate in chunks of 10MB */
	int quanta = 10 * 1024 * 1024;

	if (segno < -1 || segno == Gp_segment)
	{
		elog(LOG, "Gradually allocating %d bytes in chunks of %dMB",
				size, (quanta / 2^20));

		int size_done = 0;
		while (size_done + quanta < size)
		{
			ptrAllocation = (char *) palloc(quanta);
			size_done += quanta;
			elog(LOG, "Done allocating %dMB", size_done / (1024 * 1024));
		}

		/* Allocate the last chunk if needed */
		if (size - size_done > 0)
		{
			ptrAllocation = (char *) palloc(size - size_done);
		}

		/*
		 * This doesn't check for interrupts during sleep.
		 * Alternatively, we can use a loop with CHECK_FOR_INTERRUPTS
		 * if we want to check for interrupts.
		 */
		if (sleep_sec > 0)
		{
			elog(LOG, "Sleeping for %d seconds", sleep_sec);
			sleep(sleep_sec);
		}

		size_alloc = size;

	}

	PG_RETURN_INT32(size_alloc);
}

/*
 * gp_inject_fault_test
 *    Allocates a specified amount of memory using many palloc calls of 10MB
 *    on the specified segment
 *
 *
 *   content: segid this is applicable to. segid <= -2 means all segments
 *   fault_type: type of fault to inject
 *   arg: optional argument to the fault type
 */
Datum
gp_inject_fault_test(PG_FUNCTION_ARGS)
{
	int segno = PG_GETARG_INT32(0);
	int fault_type = PG_GETARG_INT32(1);
	int arg = PG_GETARG_INT32(2);


	if (segno >= -1 && segno != Gp_segment)
	{
		PG_RETURN_BOOL(false);
	}

	switch(fault_type)
	{
	case FAULT_TYPE_ERROR:
		elog(ERROR, "User fault injection raised error");
		break;

	case FAULT_TYPE_FATAL:
		elog(FATAL, "User fault injection raised fatal");
		break;

	case FAULT_TYPE_PANIC:
		elog(PANIC, "User fault injection raised panic");
		break;

	case FAULT_TYPE_SEGV:
		*(int *) 0 = 1234;
		break;

	case FAULT_USER_SEGV_CRITICAL:
		START_CRIT_SECTION();
		*(int *) 0 = 1234;
		END_CRIT_SECTION();
		break;

	case FAULT_USER_SEGV_LWLOCK:
		LWLockAcquire(WALInsertLock, LW_EXCLUSIVE);
		*(int *) 0 = 1234;
		break;

	case FAULT_USER_PROCEXIT:
		{
			extern void proc_exit(int);
			proc_exit((int) arg);
		}

	case FAULT_USER_ABORT:
		abort();

	default:
		elog(ERROR, "Invalid user fault injection code");

	}


	PG_RETURN_BOOL(true);
}

/*
 * gp_available_vmem_mb_test
 *    Returns the available Vmem for the segment (in MB)
 *    The format of the returned text is the following:
 *       "segid=N,vmem_mb=X"
 *     where N and X are integer values.
 *
 */
Datum
gp_available_vmem_mb_test(PG_FUNCTION_ARGS)
{

	StringInfo resultStr = makeStringInfo();
	appendStringInfo(resultStr, "segid=%d,vmem_mb=%d",
			Gp_segment, VmemTracker_GetAvailableVmemMB());

	text *resultTxt = cstring_to_text(resultStr->data);
	pfree(resultStr->data);

	PG_RETURN_TEXT_P(resultTxt);

}
