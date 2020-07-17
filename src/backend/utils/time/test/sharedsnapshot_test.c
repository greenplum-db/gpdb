#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "utils/memutils.h"
#include "utils/resowner.h"
#include "utils/faultinjector.h"
#include "utils/snapshot.h"
#include "storage/fd.h"

#include "../sharedsnapshot.c"

static void
test_boundaries_of_CreateSharedSnapshotArray(void **state)
{
	/*
	 * MaxBackends is used to calculate NUM_SHARED_SNAPSHOT_SLOTS. Make
	 * it non-zero so that we actually allocate some local snapshot slots.
	 */
	MaxBackends = 2;

	SharedSnapshotStruct 	*fakeSharedSnapshotArray = NULL;
	LWLockPadded 			*fakeLockBase = NULL;

	expect_string(RequestNamedLWLockTranche, tranche_name, "SharedSnapshotLocks");
	expect_value(RequestNamedLWLockTranche, num_lwlocks, NUM_SHARED_SNAPSHOT_SLOTS);
	will_be_called(RequestNamedLWLockTranche);
	Size sharedSnapshotShmemSize = SharedSnapshotShmemSize();
	fakeSharedSnapshotArray = malloc(sharedSnapshotShmemSize);

	will_return(ShmemInitStruct, fakeSharedSnapshotArray);
	will_assign_value(ShmemInitStruct, foundPtr, false);
	expect_any_count(ShmemInitStruct, name, 1);
	expect_any_count(ShmemInitStruct, size, 1);
	expect_any_count(ShmemInitStruct, foundPtr, 1);

	expect_string(RequestNamedLWLockTranche, tranche_name, "SharedSnapshotLocks");
	expect_value(RequestNamedLWLockTranche, num_lwlocks, NUM_SHARED_SNAPSHOT_SLOTS);
	will_be_called(RequestNamedLWLockTranche);
	fakeLockBase = malloc(NUM_SHARED_SNAPSHOT_SLOTS * sizeof(LWLockPadded));
	will_return(GetNamedLWLockTranche, fakeLockBase);
	expect_any(GetNamedLWLockTranche, tranche_name);

	CreateSharedSnapshotArray();

	for (int i=0; i<sharedSnapshotArray->maxSlots; i++)
	{
		SharedSnapshotSlot *s = &sharedSnapshotArray->slots[i];

		/*
		 * Assert that each slot in SharedSnapshotStruct has an associated
		 * dynamically allocated LWLock.
		 */
		s->snapshot.satisfies = HeapTupleSatisfiesMVCC;
		assert_true(s->slotLock == &fakeLockBase[i].lock);
		/*
		 * Assert that every slot xip array falls inside the boundaries of the
		 * allocated shared snapshot.
		 */
		assert_true((uint8_t *)s->snapshot.xip > (uint8_t *)fakeSharedSnapshotArray);
		assert_true((uint8_t *)s->snapshot.xip < (((uint8_t *)fakeSharedSnapshotArray) +
												sharedSnapshotShmemSize));
	}
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_boundaries_of_CreateSharedSnapshotArray)
	};
	MemoryContextInit();
	InitFileAccess();
	return run_tests(tests);
}
