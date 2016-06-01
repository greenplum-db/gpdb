#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#define free our_free

static void our_free(void* ptr);

#include "../memprot.c"

#define EXPECT_EXCEPTION()     \
	expect_any(ExceptionalCondition,conditionName); \
	expect_any(ExceptionalCondition,errorType); \
	expect_any(ExceptionalCondition,fileName); \
	expect_any(ExceptionalCondition,lineNumber); \
    will_be_called_with_sideeffect(ExceptionalCondition, &_ExceptionalCondition, NULL);\

#define PG_RE_THROW() siglongjmp(*PG_exception_stack, 1)

static void* our_free_expected_pointer = NULL;
static void* our_free_input_pointer = NULL;

static void our_free(void* ptr)
{
	our_free_input_pointer = ptr;
	assert_true(ptr == our_free_expected_pointer);
}

/*
 * This method will emulate the real ExceptionalCondition
 * function by re-throwing the exception, essentially falling
 * back to the next available PG_CATCH();
 */
void
_ExceptionalCondition()
{
     PG_RE_THROW();
}

/*
 * This method sets up memprot.
 */
void MemProtTestSetup(void **state)
{
	gp_mp_inited = true;
	memprotOwnerThread = pthread_self();
}

/*
 * This method resets memprot state.
 */
void MemProtTestTeardown(void **state)
{
	gp_mp_inited = false;
}

size_t CalculateVmemSizeFromUserSize(size_t user_size)
{
	size_t chosen_vmem_size = sizeof(VmemHeader) + user_size;

#ifdef GP_ALLOC_DEBUG
	chosen_vmem_size += sizeof(FooterChecksumType);
#endif

	return chosen_vmem_size;
}

void* AllocateWithCheck(size_t size)
{
	size_t chosen_vmem_size = CalculateVmemSizeFromUserSize(size);

	if (chosen_vmem_size > 0x7fffffff)
	{
		EXPECT_EXCEPTION();
	}
	else
	{
		// The expect_value's chosen_vmem_size will check the size calculation with overhead
		expect_value(VmemTracker_ReserveVmem, newlyRequestedBytes, chosen_vmem_size);
		will_return(VmemTracker_ReserveVmem, MemoryAllocation_Success);
	}

	PG_TRY();
	{
		void *ptr = gp_malloc(size);

		if (chosen_vmem_size > 0x7fffffff)
		{
			assert_true(false);
		}

		size_t stored_size = *((size_t *)((((char *) ptr) - sizeof(VmemHeader)) + offsetof(VmemHeader, size)));
		assert_true(stored_size == size);

		return ptr;
	}
	PG_CATCH();
	{
	}
	PG_END_TRY();

	return NULL;
}

void* FreeWithCheck(void* ptr, size_t size)
{
	size_t stored_size = UserPtr_GetUserPtrSize(ptr);
	assert_true(stored_size == size);

	size_t vmem_size = CalculateVmemSizeFromUserSize(size);
	assert_true(vmem_size ==  UserPtr_GetVmemPtrSize(ptr));

	// The expect_value's chosen_vmem_size will check the size calculation with overhead
	expect_value(VmemTracker_ReleaseVmem, toBeFreedRequested, vmem_size);
	will_be_called(VmemTracker_ReleaseVmem);

	our_free_expected_pointer = ((char*)ptr - sizeof(VmemHeader));
	our_free_input_pointer = NULL;

	assert_true(our_free_expected_pointer != our_free_input_pointer);

	gp_free(ptr);

	assert_true(our_free_input_pointer == our_free_expected_pointer);
}

/*
 * Checks if the gp_malloc is storing size information in the header
 */
void
test__gp_malloc__stores_size_in_header(void **state)
{
	size_t sizes[] = {50, 1024, 1024L * 1024L * 1024L * 2L, 1024L * 1024L * 1024L * 5L};

	for (int idx = 0; idx < sizeof(sizes)/sizeof(sizes[0]); idx++)
	{
		size_t chosen_size = sizes[idx];
		AllocateWithCheck(chosen_size);
	}
}

/*
 * Checks if the gp_malloc is storing size information in the header
 */
void
test__gp_free__stores_size_in_header(void **state)
{
	size_t sizes[] = {50, 1024, 1024L * 1024L * 1024L * 2L, 1024L * 1024L * 1024L * 5L};

	for (int idx = 0; idx < sizeof(sizes)/sizeof(sizes[0]); idx++)
	{
		size_t chosen_size = sizes[idx];
		void *ptr = AllocateWithCheck(chosen_size);

		// For invalid size we will only have null pointer, which we don't try to free
		if (NULL != ptr)
		{
			FreeWithCheck(ptr, chosen_size);
		}
	}
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
			unit_test_setup_teardown(test__gp_malloc__stores_size_in_header, MemProtTestSetup, MemProtTestTeardown),
			unit_test_setup_teardown(test__gp_free__stores_size_in_header, MemProtTestSetup, MemProtTestTeardown),
	};

	return run_tests(tests);
}
