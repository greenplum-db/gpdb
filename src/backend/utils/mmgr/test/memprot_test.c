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

/*
 * This method calculates a new size of a pointer given a ratio to the original size.
 * The returned size will be between he maximum requestable size and zero
 */
size_t CalculateReallocSize(size_t original_size, float delta)
{
	assert_true(delta >= 0.0);

	size_t new_size = ((float)original_size) * delta;

	// overflow
	if (delta > 1.0 && new_size < original_size)
	{
		return MAX_REQUESTABLE_SIZE;
	}

	if (new_size > MAX_REQUESTABLE_SIZE)
	{
		return MAX_REQUESTABLE_SIZE;
	}

	return new_size;
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

/*
 * Calls gp_realloc, testing that the pointer is properly resized. Returns
 * the newly resized pointer.
 */
void* ReallocateWithCheck(void* ptr, size_t requested_size)
{
	size_t orig_user_size = UserPtr_GetUserPtrSize(ptr);
	size_t orig_vmem_size = CalculateVmemSizeFromUserSize(orig_user_size);

	// If the size change is negative, we release vmem
	if (requested_size > orig_user_size)
	{
		expect_value(VmemTracker_ReserveVmem, newlyRequestedBytes, requested_size - orig_user_size);
		will_return(VmemTracker_ReserveVmem, MemoryAllocation_Success);
	}
	else
	{
		// ReleaseVmem will release the size difference
		//expect_value(VmemTracker_ReleaseVmem, toBeFreedRequested, -1 * size_difference);
	}

	void *realloc_ptr =	gp_realloc(ptr, requested_size);
	assert_true(ptr == realloc_ptr || requested_size != orig_user_size);

	assert_true(requested_size == UserPtr_GetUserPtrSize(realloc_ptr));


	// Check vmem size has been recalculated
	size_t realloc_vmem_size = orig_vmem_size + (requested_size - orig_user_size);
	assert_true(realloc_vmem_size == UserPtr_GetVmemPtrSize(realloc_ptr));

	return realloc_ptr;
}

/*
 * Frees a user pointer, checking against the original user size
 */
void FreeWithCheck(void* ptr, size_t size)
{
	bool debug = false;
#ifdef USE_ASSERT_CHECKING
	debug = true;
#endif
	size_t vmem_size = 0;

	/*
	 * Nothing to check for a null pointer in optimized build as we will just crash
	 * and this is consistent with existing behavior of pfree.
	 */
	if (NULL == ptr)
	{
		if (debug)
		{
			EXPECT_EXCEPTION();
			PG_TRY();
			{
				gp_free(ptr);
				assert_true(false);
			}
			PG_CATCH();
			{

			}
			PG_END_TRY();
		}

		return;
	}

	size_t stored_size = UserPtr_GetUserPtrSize(ptr);
	assert_true(stored_size == size);

	vmem_size = CalculateVmemSizeFromUserSize(size);
	assert_true(vmem_size ==  UserPtr_GetVmemPtrSize(ptr));

	our_free_expected_pointer = ((char*)ptr - sizeof(VmemHeader));
	our_free_input_pointer = NULL;

	if (!debug || size != 0)
	{
		// The expect_value's chosen_vmem_size will check the size calculation with overhead
		expect_value(VmemTracker_ReleaseVmem, toBeFreedRequested, vmem_size);
		will_be_called(VmemTracker_ReleaseVmem);
	}

	assert_true(our_free_expected_pointer != our_free_input_pointer);

	if (debug && size == 0)
	{
		EXPECT_EXCEPTION();
	}

	PG_TRY();
	{
		gp_free(ptr);

		if (size == 0)
		{
			assert_true(false);
		}
		assert_true(size == 0 || our_free_input_pointer == our_free_expected_pointer);
	}
	PG_CATCH();
	{

	}
	PG_END_TRY();
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
 * Checks if the gp_realloc is storing size information in the header
 */
void
test__gp_realloc__stores_size_in_header(void **state)
{
	size_t sizes[] = {50, 1024, MAX_REQUESTABLE_SIZE - sizeof(VmemHeader) - sizeof(FooterChecksumType)};
	// Ratio of new size to original size for realloc calls
	float deltas[] = {0, 0.1, 0.5, 1, 1.5, 2};

	for (int idx = 0; idx < sizeof(sizes)/sizeof(sizes[0]); idx++)
	{
		for (int didx = 0; didx < sizeof(deltas)/sizeof(deltas[0]); didx++)
		{
			size_t original_size = sizes[idx];
			float chosen_delta = deltas[didx];
			size_t requested_size = CalculateReallocSize(original_size, chosen_delta);

			void *ptr = AllocateWithCheck(original_size);
			if (NULL != ptr)
			{
				ptr = ReallocateWithCheck(ptr, requested_size);
				FreeWithCheck(ptr, requested_size);
			}
		}
	}
}

/*
 * Checks if the gp_malloc is storing size information in the header
 */
void
test__gp_free__FreesAndReleasesVmem(void **state)
{
	size_t sizes[] = {50, 1024, 1024L * 1024L * 1024L * 2L, 1024L * 1024L * 1024L * 5L};

	for (int idx = 0; idx < sizeof(sizes)/sizeof(sizes[0]); idx++)
	{
		size_t chosen_size = sizes[idx];
		void *ptr = AllocateWithCheck(chosen_size);

		// For invalid size we will only have null pointer, which we don't try to free
		FreeWithCheck(ptr, chosen_size);
	}
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
			unit_test_setup_teardown(test__gp_malloc__stores_size_in_header, MemProtTestSetup, MemProtTestTeardown),
			unit_test_setup_teardown(test__gp_realloc__stores_size_in_header, MemProtTestSetup, MemProtTestTeardown),
			unit_test_setup_teardown(test__gp_free__FreesAndReleasesVmem, MemProtTestSetup, MemProtTestTeardown),
	};

	return run_tests(tests);
}
