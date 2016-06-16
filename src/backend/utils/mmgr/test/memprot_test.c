#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#define free our_free

static void our_free(void* ptr);

#include "../memprot.c"

static bool is_assert_checking =
#ifdef USE_ASSERT_CHECKING
		true;
#else
		false;
#endif

#define EXPECT_EXCEPTION()     \
	expect_any(ExceptionalCondition,conditionName); \
	expect_any(ExceptionalCondition,errorType); \
	expect_any(ExceptionalCondition,fileName); \
	expect_any(ExceptionalCondition,lineNumber); \
    will_be_called_with_sideeffect(ExceptionalCondition, &_ExceptionalCondition, NULL);\

#define PG_RE_THROW() siglongjmp(*PG_exception_stack, 1)

/*
 * Saves the expected input of our_free function to check if we correctly free
 * vmem pointer, instead of the user pointer
 */
static void* our_free_expected_pointer = NULL;
/* Facilitates verification that gp_free actually called free */
static void* our_free_input_pointer = NULL;

/*
 * An overriden method for "free" (using macro substitution) to verify if free was called
 * with proper vmem pointer
 */
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

#define TEST_MEMORY_ACCOUNT_ADDR 0x65656565
#define TEST_MEMORY_ACCOUNT_GENERATION 0x2

/*
 * This method sets up memprot.
 */
void MemProtTestSetup(void **state)
{
	gp_mp_inited = true;
	memprotOwnerThread = pthread_self();
	ActiveMemoryAccount = (MemoryAccount*) TEST_MEMORY_ACCOUNT_ADDR;
	MemoryAccountingCurrentGeneration = TEST_MEMORY_ACCOUNT_GENERATION;
}

/*
 * This method resets memprot state.
 */
void MemProtTestTeardown(void **state)
{
	gp_mp_inited = false;
	ActiveMemoryAccount = NULL;
	MemoryAccountingCurrentGeneration = 0;
}

static size_t CalculateVmemSizeFromUserSize(size_t user_size)
{
	size_t chosen_vmem_size = sizeof(VmemHeader) + user_size;

#ifdef GP_ALLOC_DEBUG
	chosen_vmem_size += sizeof(FooterChecksumType);
#endif

	return chosen_vmem_size;
}

static size_t CalculateAccountedAllocSizeFromUserSize(size_t user_size)
{
	return CalculateVmemSizeFromUserSize(sizeof(AccountedAllocHeader) + user_size);
}

/*
 * This method calculates a new size of a pointer given a ratio to the original size.
 * The returned size will be between the maximum requestable size and zero
 */
static size_t CalculateReallocSize(size_t original_size, float ratio)
{
	assert_true(ratio >= 0.0);

	size_t new_size = ((float)original_size) * ratio;

	/* Overflow or exceeding the max allowed allocation size */
	if ((ratio > 1.0 && new_size < original_size) || (new_size > MAX_REQUESTABLE_SIZE))
	{
		return MAX_REQUESTABLE_SIZE;
	}

	return new_size;
}

/* Verifies that the stored size is same as the expected size of the usable memory */
static void VerifyStoredSize(void* ptr, size_t expected_user_size)
{
	size_t stored_size = *((size_t *)((((char *) ptr) - sizeof(VmemHeader)) + offsetof(VmemHeader, size)));
	assert_true(stored_size == expected_user_size);
}

/* Wrapper around gp_malloc that executes basic sets of tests during allocations */
static void* AllocateWithCheck(size_t size)
{
	size_t chosen_vmem_size = CalculateVmemSizeFromUserSize(size);

	/* Too big allocation should fail assert checking */
	if (is_assert_checking && chosen_vmem_size > MAX_REQUESTABLE_SIZE)
	{
		EXPECT_EXCEPTION();
	}
	else
	{
		/* The expect_value's chosen_vmem_size will check the size calculation with overhead */
		expect_value(VmemTracker_ReserveVmem, newlyRequestedBytes, chosen_vmem_size);
		will_return(VmemTracker_ReserveVmem, MemoryAllocation_Success);
	}

	PG_TRY();
	{
		void *ptr = gp_malloc(size);

		/* size limit is only checked in assert build */
		if (is_assert_checking && chosen_vmem_size > MAX_REQUESTABLE_SIZE)
		{
			assert_true(false);
		}

		size_t stored_size = *((size_t *)((((char *) ptr) - sizeof(VmemHeader)) + offsetof(VmemHeader, size)));
		assert_true(stored_size == size);
		/* Also check for correctness of UserPtr_GetEndPtr as an API */
		assert_true(UserPtr_GetEndPtr(ptr) == ((char*)ptr + size));

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
static void* ReallocateWithCheck(void* ptr, size_t requested_size)
{
	/*
	 * Nothing to check for a null pointer in optimized build as we will just crash
	 * and this is consistent with existing behavior of repalloc.
	 */
	if (NULL == ptr)
	{
		if (is_assert_checking)
		{
			/* The gp_realloc should fail assert for null pointer check */
			EXPECT_EXCEPTION();
			PG_TRY();
			{
				void * new_ptr = gp_realloc(ptr, requested_size);
				assert_true(false);
			}
			PG_CATCH();
			{

			}
			PG_END_TRY();
		}

		/*
		 * For release build or for debug build after failing the assertion in gp_realloc,
		 * we don't have any further execution to check
		 */
		return NULL;
	}

	size_t orig_user_size = UserPtr_GetUserPtrSize(ptr);
	size_t orig_vmem_size = CalculateVmemSizeFromUserSize(orig_user_size);
	size_t size_difference = requested_size - orig_user_size;

	/* If the size change is negative, we release vmem */
	if (requested_size > orig_user_size)
	{
		expect_value(VmemTracker_ReserveVmem, newlyRequestedBytes, requested_size - orig_user_size);
		will_return(VmemTracker_ReserveVmem, MemoryAllocation_Success);
	}
	else if (requested_size < orig_user_size)
	{
		/* ReleaseVmem will release the size difference */
		expect_value(VmemTracker_ReleaseVmem, toBeFreedRequested, abs(size_difference));
		will_be_called(VmemTracker_ReleaseVmem);
	}

	void *realloc_ptr =	gp_realloc(ptr, requested_size);
	assert_true(ptr == realloc_ptr || requested_size != orig_user_size);

	assert_true(requested_size == UserPtr_GetUserPtrSize(realloc_ptr));

	/* Check vmem size has been recalculated */
	size_t realloc_vmem_size = orig_vmem_size + size_difference;
	assert_true(realloc_vmem_size == UserPtr_GetVmemPtrSize(realloc_ptr));
	assert_true(UserPtr_GetEndPtr(realloc_ptr) == ((char*)realloc_ptr + requested_size));

	return realloc_ptr;
}

/*
 * Frees a user pointer, checking against the original user size
 */
static void FreeWithCheck(void* ptr, size_t size)
{
	size_t vmem_size = 0;

	/*
	 * Nothing to check for a null pointer in optimized build as we will just crash
	 * and this is consistent with existing behavior of pfree.
	 */
	if (NULL == ptr)
	{
		if (is_assert_checking)
		{
			/* The gp_free should fail assert for null pointer check */
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

		/*
		 * For release build or for debug build after failing the assertion in gp_free,
		 * we don't have any further execution to check
		 */
		return;
	}

	size_t stored_size = UserPtr_GetUserPtrSize(ptr);
	assert_true(stored_size == size);

	vmem_size = CalculateVmemSizeFromUserSize(size);
	assert_true(vmem_size ==  UserPtr_GetVmemPtrSize(ptr));

	our_free_expected_pointer = ((char*)ptr - sizeof(VmemHeader));

	/* Set the our_free_input_pointer to null to detect if free was actually called */
	our_free_input_pointer = NULL;

	if (!is_assert_checking || size != 0)
	{
		/* The expect_value's chosen_vmem_size will check the size calculation with overhead */
		expect_value(VmemTracker_ReleaseVmem, toBeFreedRequested, vmem_size);
		will_be_called(VmemTracker_ReleaseVmem);
	}

	/*
	 * Verify that the input pointer (supposed to be saved inside our_free to indicate
	 * that it was called) and the expected pointer are not same, before calling gp_free
	 */
	assert_true(our_free_expected_pointer != our_free_input_pointer);

	if (is_assert_checking && size == 0)
	{
		EXPECT_EXCEPTION();
	}

	PG_TRY();
	{
		gp_free(ptr);

		if (size == 0)
		{
			assert_true(!is_assert_checking || false);
		}

		/* For non-zero size, we expect the free (and our_free) to be called */
		assert_true(size == 0 || our_free_input_pointer == our_free_expected_pointer);
	}
	PG_CATCH();
	{

	}
	PG_END_TRY();

}

static void CheckAccountedAllocHeader(void* ptr)
{
	assert_true(NULL != ptr);

	char* stored_header = ((char*) ptr) - sizeof(AccountedAllocHeader);
	MemoryAccount* stored_memory_account = *(MemoryAccount **)(stored_header + offsetof(AccountedAllocHeader, memoryAccount));
	uint16 stored_memory_account_generation = *(uint16 *)(stored_header + offsetof(AccountedAllocHeader, memoryAccountGeneration));

	assert_true(stored_memory_account == (MemoryAccount*) TEST_MEMORY_ACCOUNT_ADDR);
	assert_true(stored_memory_account_generation == (uint16) TEST_MEMORY_ACCOUNT_GENERATION);
}

/* Wrapper around gp_allocated_malloc that executes basic sets of tests during allocations */
static void* AccountedAllocateWithCheck(size_t size)
{
	size_t chosen_vmem_size = CalculateAccountedAllocSizeFromUserSize(size);

	/* Too big allocation should fail assert checking */
	if (is_assert_checking && chosen_vmem_size > 0x7fffffff)
	{
		EXPECT_EXCEPTION();
	}
	else
	{

		expect_value(VmemTracker_ReserveVmem, newlyRequestedBytes, chosen_vmem_size);
		will_return(VmemTracker_ReserveVmem, MemoryAllocation_Success);

		expect_value(MemoryAccounting_Allocate, memoryAccount, TEST_MEMORY_ACCOUNT_ADDR);
		expect_value(MemoryAccounting_Allocate, allocatedSize, chosen_vmem_size);
		will_return(MemoryAccounting_Allocate, true);
	}
	PG_TRY();
	{
		void *ptr = gp_accounted_malloc(size);

		CheckAccountedAllocHeader(ptr);
		return ptr;
	}
	PG_CATCH();
	{
	}
	PG_END_TRY();

	return NULL;
}

/*
 * Frees a user pointer, checking against the original user size
 */
static void AccountedFreeWithCheck(void* ptr, size_t size)
{
	size_t vmem_size = 0;

	/*
	 * Nothing to check for a null pointer in optimized build as we will just crash
	 * and this is consistent with existing behavior of pfree.
	 */
	if (NULL == ptr)
	{
		if (is_assert_checking)
		{
			/* The gp_free should fail assert for null pointer check */
			EXPECT_EXCEPTION();
			PG_TRY();
			{
				gp_accounted_free(ptr);
				assert_true(false);
			}
			PG_CATCH();
			{

			}
			PG_END_TRY();
		}

		/*
		 * For release build or for debug build after failing the assertion in gp_free,
		 * we don't have any further execution to check
		 */
		return;
	}

	vmem_size = CalculateAccountedAllocSizeFromUserSize(size);

	if (!is_assert_checking || size != 0)
	{
		/* The expect_value's chosen_vmem_size will check the size calculation with overhead */
		expect_value(VmemTracker_ReleaseVmem, toBeFreedRequested, vmem_size);
		will_be_called(VmemTracker_ReleaseVmem);

		expect_value(MemoryAccounting_Free, memoryAccount, TEST_MEMORY_ACCOUNT_ADDR);
		expect_value(MemoryAccounting_Free, memoryAccountGeneration, TEST_MEMORY_ACCOUNT_GENERATION);
		expect_value(MemoryAccounting_Free, allocatedSize, vmem_size);
		will_return(MemoryAccounting_Free, true);
	}

	if (is_assert_checking && size == 0)
	{
		EXPECT_EXCEPTION();
	}

	PG_TRY();
	{
		our_free_expected_pointer = ((char*)ptr - sizeof(AccountedAllocHeader) - sizeof(VmemHeader));
		gp_accounted_free(ptr);

		if (size == 0)
		{
			assert_true(!is_assert_checking || false);
		}
	}
	PG_CATCH();
	{
	}
	PG_END_TRY();
}

static void* AccountedReallocateWithCheck(void* ptr, size_t original_size, size_t requested_size)
{
	/*
	 * Nothing to check for a null pointer in optimized build as we will just crash
	 * and this is consistent with existing behavior of repalloc.
	 */
	if (NULL == ptr)
	{
		if (is_assert_checking)
		{
			/* The gp_realloc should fail assert for null pointer check */
			EXPECT_EXCEPTION();
			PG_TRY();
			{
				void * new_ptr = gp_accounted_realloc(ptr, requested_size);
				assert_true(false);
			}
			PG_CATCH();
			{

			}
			PG_END_TRY();
		}

		/*
		 * For release build or for debug build after failing the assertion in gp_realloc,
		 * we don't have any further execution to check
		 */
		return NULL;
	}

	size_t original_vmem_size = CalculateAccountedAllocSizeFromUserSize(original_size);
	size_t requested_vmem_size = CalculateAccountedAllocSizeFromUserSize(requested_size);

	/* If the size change is negative, we release vmem */
	if (requested_size > original_size)
	{
		expect_value(VmemTracker_ReserveVmem, newlyRequestedBytes, requested_size - original_size);
		will_return(VmemTracker_ReserveVmem, MemoryAllocation_Success);
	}
	else if (requested_size < original_size)
	{
		/* ReleaseVmem will release the size difference */
		expect_value(VmemTracker_ReleaseVmem, toBeFreedRequested, original_size - requested_size);
		will_be_called(VmemTracker_ReleaseVmem);
	}

	expect_value(MemoryAccounting_Free, memoryAccount, TEST_MEMORY_ACCOUNT_ADDR);
	expect_value(MemoryAccounting_Free, memoryAccountGeneration, TEST_MEMORY_ACCOUNT_GENERATION);
	expect_value(MemoryAccounting_Free, allocatedSize, original_vmem_size);
	will_return(MemoryAccounting_Free, true);

	expect_value(MemoryAccounting_Allocate, memoryAccount, TEST_MEMORY_ACCOUNT_ADDR);
	expect_value(MemoryAccounting_Allocate, allocatedSize, requested_vmem_size);
	will_return(MemoryAccounting_Allocate, true);

	void *realloc_ptr =	gp_accounted_realloc(ptr, requested_size);
	assert_true(ptr == realloc_ptr || requested_size != original_size);

	CheckAccountedAllocHeader(realloc_ptr);

	return realloc_ptr;
}


/* Checks if we bypass vmem tracker when mp_init is false */
void
test__gp_malloc_no_vmem_tracker_when_mp_init_false(void **state)
{
	gp_mp_inited = false;

	const size_t alloc_size = 10;
	/* No expected calls to VMEM tracker as the mp is not initialized */
	void * alloc = gp_malloc(alloc_size);
	assert_true(alloc != NULL);
	VerifyStoredSize(alloc, alloc_size);
}

/* Checks if we call vmem tracker when mp_init is true */
void
test__gp_malloc_calls_vmem_tracker_when_mp_init_true(void **state)
{
	gp_mp_inited = true;
	const size_t alloc_size = 10;

	expect_value(VmemTracker_ReserveVmem, newlyRequestedBytes, CalculateVmemSizeFromUserSize(alloc_size));
	will_return(VmemTracker_ReserveVmem, MemoryAllocation_Success);

	/* No expected calls to VMEM tracker as the mp is not initialized */
	void * alloc = gp_malloc(alloc_size);
	assert_true(alloc != NULL);
	VerifyStoredSize(alloc, alloc_size);
}

/*
 * Tests the basic functionality of gp_malloc and gp_free
 */
void
test__gp_malloc_and_free__basic_tests(void **state)
{
	size_t sizes[] = {50, 1024, MAX_REQUESTABLE_SIZE - sizeof(VmemHeader) - FOOTER_CHECKSUM_SIZE,
			1024L * 1024L * 1024L * 2L, 1024L * 1024L * 1024L * 5L};

	for (int idx = 0; idx < sizeof(sizes)/sizeof(sizes[0]); idx++)
	{
		size_t chosen_size = sizes[idx];
		void *ptr = AllocateWithCheck(chosen_size);
		FreeWithCheck(ptr, chosen_size);
	}
}

/*
 * Tests the basic functionality of gp_realloc
 */
void
test__gp_realloc__basic_tests(void **state)
{
	size_t sizes[] = {50, 1024, MAX_REQUESTABLE_SIZE - sizeof(VmemHeader) - FOOTER_CHECKSUM_SIZE};
	/* Ratio of new size to original size for realloc calls */
	float fractions[] = {0, 0.1, 0.5, 1, 1.5, 2};

	for (int idx = 0; idx < sizeof(sizes)/sizeof(sizes[0]); idx++)
	{
		for (int didx = 0; didx < sizeof(fractions)/sizeof(fractions[0]); didx++)
		{
			size_t original_size = sizes[idx];
			float chosen_fraction = fractions[didx];
			size_t requested_size = CalculateReallocSize(original_size, chosen_fraction);

			void *ptr = AllocateWithCheck(original_size);
			ptr = ReallocateWithCheck(ptr, requested_size);
			FreeWithCheck(ptr, requested_size);
		}
	}
}

/*
 * Tests basic functionality of gp_accounted_malloc and gp_accounted_free
 */
void
test__gp_accounted_malloc_and_free__basic_tests(void **state)
{
	size_t sizes[] = { 50, 1024, 1024L * 1024L * 1024L * 2L, 1024L * 1024L * 1024L * 5L,
			MAX_REQUESTABLE_SIZE - sizeof(AccountedAllocHeader) - sizeof(VmemHeader) - FOOTER_CHECKSUM_SIZE};

	for (int idx = 0; idx < sizeof(sizes)/sizeof(sizes[0]); idx++)
	{
		size_t chosen_size = sizes[idx];
		void* ptr = AccountedAllocateWithCheck(chosen_size);
		AccountedFreeWithCheck(ptr, chosen_size);
	}
}

/*
 * Tests the basic functionality of gp_realloc
 */
void
test__gp_accounted_realloc__basic_tests(void **state)
{
	size_t sizes[] = {50, 1024, MAX_REQUESTABLE_SIZE - sizeof(VmemHeader) - FOOTER_CHECKSUM_SIZE};
	/* Ratio of new size to original size for realloc calls */
	float fractions[] = {0 };//, 0.1, 0.5, 1, 1.5, 2};

	for (int idx = 0; idx < sizeof(sizes)/sizeof(sizes[0]); idx++)
	{
		for (int didx = 0; didx < sizeof(fractions)/sizeof(fractions[0]); didx++)
		{
			size_t original_size = sizes[idx];
			float chosen_fraction = fractions[didx];
			size_t realloced_size = CalculateReallocSize(original_size, chosen_fraction);

			void *ptr = AccountedAllocateWithCheck(original_size);
			ptr = AccountedReallocateWithCheck(ptr, original_size, realloced_size);
			AccountedFreeWithCheck(ptr, realloced_size);
		}
	}
}


int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test_setup_teardown(test__gp_malloc_no_vmem_tracker_when_mp_init_false, MemProtTestSetup, MemProtTestTeardown),
		unit_test_setup_teardown(test__gp_malloc_calls_vmem_tracker_when_mp_init_true, MemProtTestSetup, MemProtTestTeardown),
		unit_test_setup_teardown(test__gp_malloc_and_free__basic_tests, MemProtTestSetup, MemProtTestTeardown),
		unit_test_setup_teardown(test__gp_realloc__basic_tests, MemProtTestSetup, MemProtTestTeardown),

		unit_test_setup_teardown(test__gp_accounted_malloc_and_free__basic_tests, MemProtTestSetup, MemProtTestTeardown),
		unit_test_setup_teardown(test__gp_accounted_realloc__basic_tests, MemProtTestSetup, MemProtTestTeardown),
	};

	return run_tests(tests);
}
