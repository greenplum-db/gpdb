#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "c.h"
#include "postgres.h"
#include "nodes/nodes.h"

#include "../event_version.c"

/*
 * Checks if EventVersion_ShmemInit attaches the global pointers and initializes
 * the versions as a postmaster
 */
static void
test__EventVersion_ShmemInit__AttachesPointersAndInitializesValuesWhenPostmaster(void **state)
{
	vmemTrackerInited = false;

	CurrentVersion = NULL;
	latestRunawayVersion = NULL;

	static EventVersion fakeCurrentVersion = 123;
	static EventVersion fakeLatestRunawayVersion = 123;

	will_return(ShmemInitStruct, &fakeCurrentVersion);
	will_return(ShmemInitStruct, &fakeLatestRunawayVersion);

	/* Simulate Postmaster */
	IsUnderPostmaster = false;
	will_assign_value(ShmemInitStruct, foundPtr, false);
	will_assign_value(ShmemInitStruct, foundPtr, false);

	expect_any_count(ShmemInitStruct, name, 2);
	expect_any_count(ShmemInitStruct, size, 2);
	expect_any_count(ShmemInitStruct, foundPtr, 2);

	EventVersion_ShmemInit();

	/*
	 * The pointers should always be attached to the share memory area
	 * no matter whether its postmaster or under postmaster
	 */
	assert_true(CurrentVersion == &fakeCurrentVersion);
	assert_true(latestRunawayVersion == &fakeLatestRunawayVersion);

	/* As we are postmaster, we should also initialize the versions */
	assert_true(*CurrentVersion == 1);
	assert_true(*latestRunawayVersion == 0);
}

/*
 * Checks if EventVersion_ShmemInit attaches the global pointers and but does not
 * initialize the versions when under postmaster
 */
static void
test__EventVersion_ShmemInit__AttachesPointersWhenUnderPostmaster(void **state)
{
	vmemTrackerInited = false;

	CurrentVersion = NULL;
	latestRunawayVersion = NULL;

	static EventVersion fakeCurrentVersion = 123;
	static EventVersion fakeLatestRunawayVersion = 123;

	will_return(ShmemInitStruct, &fakeCurrentVersion);
	will_return(ShmemInitStruct, &fakeLatestRunawayVersion);

	/* Simulate Postmaster */
	IsUnderPostmaster = true;
	will_assign_value(ShmemInitStruct, foundPtr, true);
	will_assign_value(ShmemInitStruct, foundPtr, true);

	expect_any_count(ShmemInitStruct, name, 2);
	expect_any_count(ShmemInitStruct, size, 2);
	expect_any_count(ShmemInitStruct, foundPtr, 2);

	EventVersion_ShmemInit();

	/*
	 * The pointers should always be attached to the share memory area
	 * no matter whether its postmaster or under postmaster
	 */
	assert_true(CurrentVersion == &fakeCurrentVersion);
	assert_true(latestRunawayVersion == &fakeLatestRunawayVersion);

	/* As we are under postmaster, we don't re-initialize the versions */
	assert_true(*CurrentVersion == 123);
	assert_true(*latestRunawayVersion == 123);
}

int
main(int argc, char* argv[])
{
        cmockery_parse_arguments(argc, argv);

        const UnitTest tests[] = {
            	unit_test(test__EventVersion_ShmemInit__AttachesPointersAndInitializesValuesWhenPostmaster),
            	unit_test(test__EventVersion_ShmemInit__AttachesPointersWhenUnderPostmaster),
        };
        return run_tests(tests);
}
