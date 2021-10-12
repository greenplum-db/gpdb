#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../mcxt.c"

static void
test__MemoryContextContainsGenericAllocation_DoesContain(void **state)
{
	MemoryContext mcxt = AllocSetContextCreate(ErrorContext,
													 "TestContext",
													 ALLOCSET_DEFAULT_MINSIZE,
													 ALLOCSET_DEFAULT_INITSIZE,
													 ALLOCSET_DEFAULT_MAXSIZE);
	MemoryContextSwitchTo(mcxt);

	// Most common case, where pointer points to beginning of a chunk allocated
	//  in the correct context  (true positive)
	char *pointer = palloc(500);
	bool contains = MemoryContextContainsGenericAllocation(mcxt, pointer);
	assert_true(contains);

	// Make sure this also works for pointers into an offset of a chunk from
	//   the right context, and misaligned
	contains = MemoryContextContainsGenericAllocation(mcxt, pointer + 499);
	assert_true(contains);
}

static void
test__MemoryContextContainsGenericAllocation_DoesNotContainAllocationFromAnotherMemoryContext(void **state)
{
	MemoryContext mcxt = AllocSetContextCreate(ErrorContext,
													 "A memory context",
													 ALLOCSET_DEFAULT_MINSIZE,
													 ALLOCSET_DEFAULT_INITSIZE,
													 ALLOCSET_DEFAULT_MAXSIZE);
	MemoryContext another_mcxt = AllocSetContextCreate(ErrorContext,
													 "Another memory context",
													 ALLOCSET_DEFAULT_MINSIZE,
													 ALLOCSET_DEFAULT_INITSIZE,
													 ALLOCSET_DEFAULT_MAXSIZE);

	MemoryContextSwitchTo(mcxt);

	bool contains;
	void *pointer = palloc(sizeof(char));

	contains = MemoryContextContainsGenericAllocation(another_mcxt, pointer);
	assert_false(contains);
}

static void
test__MemoryContextContainsGenericAllocation_FalsePositive(void **state)
{
	MemoryContext mcxt = AllocSetContextCreate(ErrorContext,
													 "A memory context",
													 ALLOCSET_DEFAULT_MINSIZE,
													 ALLOCSET_DEFAULT_INITSIZE,
													 ALLOCSET_DEFAULT_MAXSIZE);
	MemoryContext another_mcxt = AllocSetContextCreate(ErrorContext,
													 "Another memory context",
													 ALLOCSET_DEFAULT_MINSIZE,
													 ALLOCSET_DEFAULT_INITSIZE,
													 ALLOCSET_DEFAULT_MAXSIZE);

	MemoryContextSwitchTo(another_mcxt);
	char *pointer = palloc(500);
	bool contains;

	//  With the previous MemoryContextContains() function, you could get a
	//  false positive here if the pointer points into the middle of a palloc'd
	//  region (which does happen in some window functions, such as LAG()
	//  where the pointer can point to columns within previously allocated memtuples)
	//  instead of the beginning, where the memory just before the pointer happens
	//  to match the address of the memory context.  This is rare in gpdb7, but
	//  we were seeing it happen in gpdb6 when the preceeding memory was all 0's
	pointer += 200;

	*(MemoryContext *) (((char *) pointer) - sizeof(void *)) = NULL;
	// check for false positive when memory just before pointer contains
	// a null pointer -- this was happening with LAG() function in gpdb6 before
	contains = MemoryContextContainsGenericAllocation(mcxt, pointer);
	assert_false(contains);

	*(MemoryContext *) (((char *) pointer) - sizeof(void *)) = mcxt;

	// check for false positive when memory just before pointer happens
	// to contain the address it's looking for
	contains = MemoryContextContainsGenericAllocation(mcxt, pointer);
	assert_false(contains);
}

static void
test__MemoryContextContainsGenericAllocation_FalseNegative(void **state)
{
	MemoryContext mcxt = AllocSetContextCreate(ErrorContext,
													 "A memory context",
													 ALLOCSET_DEFAULT_MINSIZE,
													 ALLOCSET_DEFAULT_INITSIZE,
													 ALLOCSET_DEFAULT_MAXSIZE);
	MemoryContext another_mcxt = AllocSetContextCreate(ErrorContext,
													 "Another memory context",
													 ALLOCSET_DEFAULT_MINSIZE,
													 ALLOCSET_DEFAULT_INITSIZE,
													 ALLOCSET_DEFAULT_MAXSIZE);
	MemoryContextSwitchTo(mcxt);
	char *pointer = palloc(500);
	pointer += 200;

	*(MemoryContext *) (((char *) pointer) - sizeof(void *)) = another_mcxt;

	// check for a false negative when memory just before pointer contains
	// an address it's not looking for
	bool contains = MemoryContextContainsGenericAllocation(mcxt, pointer);
	assert_true(contains);
}

static void
test__MemoryContextContainsGenericAllocation_MultipleAllocations(void **state) {
	MemoryContext mcxt = AllocSetContextCreate(ErrorContext,
													 "A memory context",
													 ALLOCSET_DEFAULT_MINSIZE,
													 ALLOCSET_DEFAULT_INITSIZE,
													 ALLOCSET_DEFAULT_MAXSIZE);

	MemoryContextSwitchTo(mcxt);

	for (Size s = 1; s < 5 * ALLOCSET_DEFAULT_INITSIZE; s *= 16) {
		assert_true(MemoryContextContainsGenericAllocation(mcxt, palloc(s)));
	}
}

static void
test__MemoryContextContainsGenericAllocation_DoesNotContainNullPointer(void **state)
{
	MemoryContext mcxt = AllocSetContextCreate(ErrorContext,
													 "TestContext",
													 ALLOCSET_DEFAULT_MINSIZE,
													 ALLOCSET_DEFAULT_INITSIZE,
													 ALLOCSET_DEFAULT_MAXSIZE);
	bool contains = MemoryContextContainsGenericAllocation(mcxt, NULL);
	assert_false(contains);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__MemoryContextContainsGenericAllocation_DoesContain),
		unit_test(test__MemoryContextContainsGenericAllocation_DoesNotContainAllocationFromAnotherMemoryContext),
		unit_test(test__MemoryContextContainsGenericAllocation_MultipleAllocations),
		unit_test(test__MemoryContextContainsGenericAllocation_FalsePositive),
		unit_test(test__MemoryContextContainsGenericAllocation_FalseNegative),
		unit_test(test__MemoryContextContainsGenericAllocation_DoesNotContainNullPointer)
	};

	return run_tests(tests);
}

