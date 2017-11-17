#include "../resource_manager.c"

#include <utils/memutils.h>
#include "cmockery.h"

void
test__DefaultResourceOwner_sets_when_CurrentResourceOwner_is_NULL(void **state)
{
	CurrentResourceOwner = NULL;
	ResourceOwner owner = DefaultResourceOwnerAcquire("dummy");
	assert_true(CurrentResourceOwner == owner);
	DefaultResourceOwnerRelease(owner);
	assert_true(CurrentResourceOwner == NULL);
}

void
test__DefaultResourceOwner_does_nothing_when_CurrentResourceOwner_is_set(void **state)
{
	const ResourceOwner initialOwner =
			ResourceOwnerCreate(NULL, "initial resource owner");
	CurrentResourceOwner = initialOwner;

	ResourceOwner owner = DefaultResourceOwnerAcquire("dummy");
	assert_true(CurrentResourceOwner == initialOwner);
	DefaultResourceOwnerRelease(owner);
	assert_true(CurrentResourceOwner == initialOwner);

	ResourceOwnerDelete(initialOwner);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	MemoryContextInit();
	TopTransactionContext =
			AllocSetContextCreate(TopMemoryContext,
			                      "mock TopTransactionContext",
			                      ALLOCSET_DEFAULT_MINSIZE,
			                      ALLOCSET_DEFAULT_INITSIZE,
			                      ALLOCSET_DEFAULT_MAXSIZE);

	const UnitTest tests[] = {
			unit_test(test__DefaultResourceOwner_sets_when_CurrentResourceOwner_is_NULL),
			unit_test(test__DefaultResourceOwner_does_nothing_when_CurrentResourceOwner_is_set),
	};

	run_tests(tests);
}
