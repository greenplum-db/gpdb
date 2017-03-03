#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "c.h"
#include "postgres.h"

/* Ignore elog */
#include "utils/elog.h"
#undef elog
#define elog


/* Provide specialized mock implementations for memory allocation functions */

#undef palloc
#define palloc getSpillFile_palloc_mock
void *getSpillFile_palloc_mock(Size size);

#include "../execHHashagg.c"


/*
 * This is a mocked version of palloc to be used in getSpillFile().
 * It returns allocated memory padded with 0x7f pattern.
 */
void *
getSpillFile_palloc_mock(Size size)
{
	void *ptr = MemoryContextAlloc(CurrentMemoryContext, size);
	MemSetAligned(ptr, 0x7f, size);
	return ptr;
}


/* ==================== getSpillFile ==================== */
/*
 * Test that the spill_file->file_info->wfile field is allocated
 * and initialized during normal execution (no exception thrown).
 */
void
test__getSpillFile__Initialize_wfile_success(void **state)
{
	int alloc_size = 0;
	int file_no = 0;
	int branching_factor = 32;
	ExecWorkFile *ewfile = (ExecWorkFile *) palloc0(sizeof(ExecWorkFile));
	workfile_set *work_set = (workfile_set *) palloc0(sizeof(workfile_set));
	SpillSet *spill_set = (SpillSet *) palloc0(sizeof(SpillSet) + (branching_factor-1) * sizeof (SpillFile));

	SpillFile *spill_file = &spill_set->spill_files[file_no];
	spill_file->file_info = NULL;

	expect_value(workfile_mgr_create_file, work_set, work_set);
	will_return(workfile_mgr_create_file, ewfile);

	getSpillFile(work_set, spill_set, file_no, &alloc_size);

	assert_true(spill_file->file_info != NULL);
	assert_int_equal(spill_file->file_info->total_bytes, 0);
	assert_int_equal(spill_file->file_info->ntuples, 0);
	assert_int_equal(alloc_size, BATCHFILE_METADATA);

	/*
	 * During normal execution, wfile should be initialized with
	 * the result of workfile_mgr_create_wfile, a valid ExecWorkFile pointer
	 */
	assert_true(spill_file->file_info->wfile == ewfile);
}

/*
 * Function used a side effect to simulate throwing exception
 * by a certain function.
 */
void
throw_exception_side_effect()
{
	PG_RE_THROW();
}

/* ==================== getSpillFile ==================== */
/*
 * Test that the spill_file->file_info->wfile field is initialized to NULL
 * when creating a workfile throws an exception.
 */
void
test__getSpillFile__Initialize_wfile_exception(void **state)
{
	int alloc_size = 0;
	int file_no = 0;
	int branching_factor = 32;
	ExecWorkFile *ewfile = (ExecWorkFile *) palloc0(sizeof(ExecWorkFile));
	workfile_set *work_set = (workfile_set *) palloc0(sizeof(workfile_set));
	SpillSet *spill_set = (SpillSet *) palloc0(sizeof(SpillSet) + (branching_factor-1) * sizeof (SpillFile));

	volatile SpillFile *spill_file = &spill_set->spill_files[0];
	spill_file->file_info = NULL;

	/* Make workfile_mgr_create_file throw an exception, using the side effect function */
	expect_value(workfile_mgr_create_file, work_set, work_set);
	will_return_with_sideeffect(workfile_mgr_create_file, ewfile, &throw_exception_side_effect, NULL);

	PG_TRY();
	{
		/* This function will throw an exception, and we'll catch it below */
		getSpillFile(work_set, spill_set, file_no, &alloc_size);
	}
	PG_CATCH();
	{
		assert_true(spill_file->file_info != NULL);
		assert_int_equal(spill_file->file_info->total_bytes, 0);
		assert_int_equal(spill_file->file_info->ntuples, 0);
		assert_int_equal(alloc_size, 0);

		/*
		 * This is the main test: We must initialize this pointer to NULL, even
		 * if an exception is thrown
		 */
		assert_true(spill_file->file_info->wfile == NULL);
		return;
	}
	PG_END_TRY();

	/* We shouldn't get here, the getSpillFile should throw an exception */
	assert_true(false);
}

/* ==================== destroy_agg_hash_table ==================== */
/*
 * Test that the destroy_agg_hash_table frees all the memory.
 */
test__destroy_agg_hash_table__check_for_leaks(void **state)
{
	/* Dummy agg state to test for leaks */
	AggState aggState;

	MemoryContext testContext = AllocSetContextCreate(CurrentMemoryContext,
			  "LeakTestContext",
			  ALLOCSET_DEFAULT_MINSIZE,
			  ALLOCSET_DEFAULT_INITSIZE,
			  ALLOCSET_DEFAULT_MAXSIZE);

	/*
	 * We don't switch to testContext as we want to control all the allocations.
	 * Instead we use MemoryContextAllocZero to explicitly allocate in that context.
	 */

	/* Create all the mocked objects */
	HashAggTable *ht = MemoryContextAllocZero(testContext, sizeof(HashAggTable));
	aggState.hhashtable = ht;
	Agg *agg = MemoryContextAllocZero(testContext, sizeof(Agg));
	agg->aggstrategy = AGG_HASHED;
	aggState.ss.ps.plan = agg;

	aggState.aggcontext =
		AllocSetContextCreate(TopMemoryContext,
							  "AggContext",
							  ALLOCSET_DEFAULT_MINSIZE,
							  ALLOCSET_DEFAULT_INITSIZE,
							  ALLOCSET_DEFAULT_MAXSIZE);

	ht->entry_cxt = AllocSetContextCreate(aggState.aggcontext,
			 "HashAggTableContext",
			 ALLOCSET_DEFAULT_MINSIZE,
			 ALLOCSET_DEFAULT_INITSIZE,
			 ALLOCSET_DEFAULT_MAXSIZE);

	ht->group_buf = mpool_create(ht->entry_cxt,
			"GroupsAndAggs Context");

#define NUM_SPILL_FILES 3
#define NUM_BUCKETS 1024

	ht->buckets = MemoryContextAllocZero(testContext, sizeof(HashAggEntry) * NUM_BUCKETS);
	ht->bloom = MemoryContextAllocZero(testContext, NUM_BUCKETS / (sizeof(uint64) * 8 /* bits */));

	SpillSet *spill_set = createSpillSet(NUM_SPILL_FILES, 0 /* parent_hash_bit */);
	ht->spill_set = spill_set;
	spill_set->num_spill_files = NUM_SPILL_FILES;
	for (int i = 0; i < NUM_SPILL_FILES; i++)
	{
		spill_set->spill_files[i].file_info = MemoryContextAllocZero(testContext, sizeof(BatchFileInfo));
	}

#ifdef CDB_PALLOC_TAGS
	/* Make sure our testContext had some allocations */
	assert_true(((AllocSet)testContext)->allocList != NULL);
#endif

	/* Invoke the method of interest */
	destroy_agg_hash_table(&aggState);
	/* Free our fake plan node before checking that the testContext is completely empty */
	pfree(agg);

	/* Nothing should be allocated anymore */
#ifdef CDB_PALLOC_TAGS
	assert_true(((AllocSet)testContext)->allocList == NULL);
#endif
	assert_true(aggState.hhashtable == NULL);
}

/* ==================== main ==================== */
int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__getSpillFile__Initialize_wfile_success),
		unit_test(test__getSpillFile__Initialize_wfile_exception),
		unit_test(test__destroy_agg_hash_table__check_for_leaks)
	};

	MemoryContextInit();

	return run_tests(tests);
}
