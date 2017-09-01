/*-------------------------------------------------------------------------
 *
 * simex.c
 * 		Implementation of interface for simulating Exceptional Situations (ES).
 *
 * The Simulator of Exceptions (SimEx) framework controls when an ES will be
 * injected and what ES subclass will be used.
 *
 * The ES class to be simulated is set at startup and is associated with a set
 * of ES subclasses. SimEx uses bit vectors (one per ES subclass) to track
 * which stacks have been examined. On each call, a hash value is calculating
 * over the frame addresses in the stack. Then, the corresponding bit is
 * checked in all bit vectors. The first bit vector that has the bit unset
 * gives the ES subclass to be injected.
 *
 * After identifying the ES subclass, SimEx logs the prominent ES injection.
 * Logged information includes the ES class-subclass names, the file and the
 * line where injection will occur and the current stack trace.
 *
 * Bit vectors are stored in a shared memory segment; the postmaster is
 * responsible for initializing, reseting and deleting it. All backends access
 * the same bit vectors through this shmem segment.
 *
 * Portions Copyright (c) 2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/misc/simex.c
 *
 *-------------------------------------------------------------------------
 */

#include <dlfcn.h>

#include "postgres.h"
#include "lib/stringinfo.h"
#include "utils/elog.h"
#include "utils/simex.h"

#ifdef USE_TEST_UTILS

/* associate an ES subclass with its class */
typedef struct SimExESClassAssociate
{
	SimExESClass class;
	SimExESSubClass subclass;
	const char *ESName; 		// format: "class_subclass"
} SimExESClassAssociate;

/*
 * Table holding ES class/subclass associations
 *
 * NOTICE: This table needs to be updated when a new ES class or subclass
 *         is introduced, see simex.h.
 */
const SimExESClassAssociate SimExESClassAssociateTable[] =
{
	{SimExESClass_OOM, SimExESSubClass_OOM_ReturnNull, "OOM_ReturnNull"},
	{SimExESClass_Cancel, SimExESSubClass_Cancel_QueryCancel, "Cancel_QueryCancel"},
	{SimExESClass_Cancel, SimExESSubClass_Cancel_ProcDie, "Cancel_ProcDie"},
	{SimExESClass_CacheInvalidation, SimExESSubClass_CacheInvalidation, "Cache_Invalidation"},
	{SimExESClass_SysError, SimExESSubClass_SysError_Net, "SysError_Net"},
	{SimExESClass_ProcKill, SimExESSubClass_ProcKill_BgWriter, "Send SIGKILL to writer process"},
};
#define SimExESClassAssociateTable_Size (sizeof(SimExESClassAssociateTable) / sizeof(SimExESClassAssociate))

#define SIMEX_ES_SUBCLASS_MAX_COUNT 	(SYNC_BITVECTOR_MAX_COUNT)	/* one bit vector per ES subclass */
#define SIMEX_MAX_FRAME_DEPTH 			64							/* number of frames used to compute a stack's hash value,
																	 * set to low value to prevent multiple ES injection for a
																	 * repeating function sequence.
																	 */
#define OFFSET_NOT_FOUND				-1
#define PERCENT							100
#define RAND_ACCURACY					1000 /* percentage with three digits accuracy */

/* SimEx bit vector container */
static SyncBitvectorContainer bitvectorContainer = NULL;

/* maps bit vector offsets to ES subclasses */
static int16 SimExESSubclassOffset[SIMEX_ES_SUBCLASS_MAX_COUNT];
static int32 SimExESSubclassCount = 0;

/* holds return addresses of frames in stack */
static void *SimExStackAddresses[SIMEX_MAX_FRAME_DEPTH];

/* stack depth */
static uint32 SimExStackDepth = 0;

/* static functions */
static void check_associate_table(void);
static uint64 simex_get_fingerprint(void);
static int32 simex_check_fingerprint(uint64 fingerprint);
static SimExESSubClass
simex_log_and_return(int32 associateOffset, const char *file, int32 line, uint64 fingerprint);

/*
 * Initializes SimEx. This must run before initializing the shared memory segment.
 *
 * Checks if the ES class (as set by GUC gp_simex_class) is valid and if there
 * are valid associated ES subclasses to it.
 * Stores the used offsets of SimExESClassAssociateTable in SimExESSubclassOffset
 * and sets the number of ES subclasses to be examined.
 */
void simex_init(void)
{
	check_associate_table();

/* check if the build allows SimEx */
#ifndef USE_TEST_UTILS
	gp_simex_init = false;
#endif

/* only x86 is currently supported */
#if !defined(__i386) && !defined(__x86_64__)
	gp_simex_init = false;
#endif

	if (!gp_simex_init)
	{
		return;
	}

	SimExESClass class = SimExESClass_Base;
	int32 i;
	const int32 size = SimExESClassAssociateTable_Size;

	SimExESClass_CheckValid(gp_simex_class);
	class = (SimExESClass) gp_simex_class;
	SimExESSubclassCount = 0;

	for (i = 0; i < size; i++)
	{
		if (SimExESClassAssociateTable[i].class == class)
		{
			SimExESSubclassOffset[SimExESSubclassCount] = i;
			SimExESSubclassCount++;
		}
	}

	SimExStackDepth = 0;
}

/*
 * Checks if the entries in the ES class/subclass association table
 * are consistent with the enum values for ES class/subclass.
 */
static
void check_associate_table()
{
	/* counts ES subclasses per class */
	int32 subclassCount[SimExESClass_Max];
	int32 i = 0;

	/* true is a subclass is associated to a class */
	bool subclassAssociated[SimExESSubClass_Max];

	/* reset */
	for (i = 0; i < SimExESClass_Max; i++)
	{
		subclassCount[i] = 0;
	}

	for (i = 0; i < SimExESSubClass_Max; i++)
	{
		subclassAssociated[i] = false;
	}

	/* check association table */
	for (i = 0; i < SimExESClassAssociateTable_Size; i++)
	{
		SimExESClassAssociate associate = SimExESClassAssociateTable[i];
		Assert(SimExESClass_IsValid(associate.class));
		Assert(SimExESSubClass_IsValid(associate.subclass));

		subclassCount[associate.class]++;
		Assert(subclassCount[associate.class] <= SIMEX_ES_SUBCLASS_MAX_COUNT);

		subclassAssociated[associate.subclass] = true;
	}

	/* check if all classes are associated to subclasses */
	for (i = 0; i < SimExESClass_Max; i++)
	{
		Assert(subclassCount[i] > 0);
	}

	/* check if all subclasses are associated to classes */
	for (i = 1; i < SimExESSubClass_Max; i++)
	{
		Assert(subclassAssociated[i]);
	}
}

/*
 * Returns the number of ES subclasses associated to the specified ES class.
 */
int32 simex_get_subclass_count(void)
{
	if (SimExESSubclassCount == 0)
	{
		simex_init();
	}
	Assert(SimExESSubclassCount > 0);
	return SimExESSubclassCount;
}

/*
 * Sets the SimEx bit vector container.
 */
void simex_set_sync_bitvector_container(SyncBitvectorContainer container)
{
	bitvectorContainer = container;
}

/*
 * Checks which ES subclass will be injected (if any).
 * Logs ES injection to follow.
 */
SimExESSubClass simex_check(const char *file, int32 line)
{

/* check if the build allows SimEx */
#ifndef USE_TEST_UTILS
	return SimExESSubClass_OK;
#endif

	if (!gp_simex_init || !gp_simex_run)
	{
		return SimExESSubClass_OK;
	}

	Assert(file && "No source file is specified");
	Assert(line > 0 && "Line number is invalid");
	Assert(bitvectorContainer && "Bit vector container is NULL");
	Assert(SimExESSubclassCount > 0 && "No ES subclass has been set");

	/* disable SimEx to prevent infinitive loops */
	gp_simex_run = false;

	int64 fingerprint = simex_get_fingerprint();
	int32 associateOffset = simex_check_fingerprint(fingerprint);
	SimExESSubClass subclass =
			simex_log_and_return(associateOffset, file, line, fingerprint);

	/* re-enable SimEx before returning */
	gp_simex_run = true;

	return subclass;
}

/*
 * Returns a hash value as the fingerprint of the current stack.
 * The hash value is calculated by iterating over the frame
 * addresses in the stack.
 */
static
uint64 simex_get_fingerprint(void)
{
	uint64 hash = 0;

	/* get frame addresses */
	SimExStackDepth = gp_backtrace(SimExStackAddresses, SIMEX_MAX_FRAME_DEPTH);

	Assert(SIMEX_MAX_FRAME_DEPTH >= SimExStackDepth && "Stack size exceeds maximum");

	/* consider the first SIMEX_MAX_FRAME_DEPTH frames only, below the stack base pointer */
	for (uint32 depth = 0; depth < SimExStackDepth; depth++)
	{
		/*
		 * compute a hash -- xor with multiple of height of stack to make sure
		 * multiple recursive calls are distinguished from each other.
		 */
		hash ^= (*(uintptr_t*)SimExStackAddresses[depth]) ^ (depth * 23);
	}

	return hash;
}

/*
 * Checks if a ES subclass has not been examined for the
 * specific stack fingerprint. Returns the ES subclass offset
 * in SimExESClassAssociateTable if such a subclass is found,
 * else OFFSET_NOT_FOUND.
 */
static
int32 simex_check_fingerprint(uint64 fingerprint)
{
	// check for randomized operation
	if (gp_simex_rand < PERCENT)
	{
		Assert(gp_simex_rand * RAND_ACCURACY >= 1);

		uint64_t rand = random();

		/* check if random number between 0.001 and 100 is less than the threshold percentage */
		if (rand % (PERCENT * RAND_ACCURACY) <	(gp_simex_rand * RAND_ACCURACY))
		{
			/* randomly pick one of the ES subclasses to inject */
			return SimExESSubclassOffset[rand % SimExESSubclassCount];
		}

		/* no injection */
		return OFFSET_NOT_FOUND;
	}

	int32 i = 0;
	for (i = 0; i < SimExESSubclassCount; i++)
	{
		if (SyncBitVector_TestTestSetBit(bitvectorContainer, i, fingerprint))
		{
			int32 offset = SimExESSubclassOffset[i];
			Assert(offset >= 0);
			return offset;
		}
	}

	return OFFSET_NOT_FOUND;
}

/*
 * Logs prominent ES injection and returns the specified ES subclass.
 * Log information includes ES class-subclass names, file, line and current stack trace.
 */
static
SimExESSubClass simex_log_and_return(int32 associateOffset, const char *file, int32 line, uint64 fingerprint)
{
	if (associateOffset == OFFSET_NOT_FOUND)
	{
		return SimExESSubClass_OK;
	}

	Assert(associateOffset < SimExESClassAssociateTable_Size && "Invalid association offset");

	char *stackTrace = gp_stacktrace(SimExStackAddresses, SimExStackDepth);

	/* log stack trace */
	ereport(LOG,
			(errmsg("SimEx inject: class='%s', file='%s', line=%d, hash=%lu, stack trace: \n%s\n",
					SimExESClassAssociateTable[associateOffset].ESName,
					file,
					line,
					(unsigned long int) fingerprint,
					stackTrace)));

	return SimExESClassAssociateTable[associateOffset].subclass;
}

#endif /* USE_TEST_UTILS */
