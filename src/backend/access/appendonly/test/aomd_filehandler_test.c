#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"


#include "postgres.h"
#include "utils/memutils.h"
#include "access/aomd.h"
#include "access/appendonlytid.h"
#include "access/appendonlywriter.h"
#include "catalog/pg_attribute_encoding.h"

/*
 * ACHTUNG  This module is trickier than you might initially have expected
 *  because not all combinations of present files are in fact valid GPDB
 *  ao tables.  See the comment in ao_foreach_extent_file() to understand
 *  what valid combinations are.  We do NOT attempt to test invalid
 *  combinations here as that is a higher-level test than this unit test.
 */

#define MAX_SEGNO_FILES (MAX_AOREL_CONCURRENCY * MaxFileNumber)
typedef struct {
	bool present[MAX_SEGNO_FILES];
	bool call_result[MAX_SEGNO_FILES];
	bool call_expected[MAX_SEGNO_FILES];
	int num_called;
} aomd_filehandler_callback_ctx;

static void
setup_test_structures(aomd_filehandler_callback_ctx *ctx)
{
	memset(ctx->present, false, sizeof(ctx->present));
	memset(ctx->call_result, false, sizeof(ctx->call_result));
	memset(ctx->call_expected, false, sizeof(ctx->call_expected));
	ctx->num_called = 0;

    /* these files get checked for presence in the foreach() */
    ctx->call_expected[AOTupleId_MultiplierSegmentFileNum] = true;
    ctx->call_expected[MaxHeapAttributeNumber * AOTupleId_MultiplierSegmentFileNum + AOTupleId_MultiplierSegmentFileNum] = true;
    ctx->call_expected[MaxHeapAttributeNumber * AOTupleId_MultiplierSegmentFileNum] = true;
    for (int segno = 1; segno < MAX_AOREL_CONCURRENCY; segno++)
    {
        ctx->call_expected[segno] = true;
        ctx->call_expected[MaxHeapAttributeNumber * AOTupleId_MultiplierSegmentFileNum + segno] = true;
    }
}

/*
 * This is meant to be called in sorted order from lowest segno to
 *  highest segno.  Since [.0,.127] is called by foreach(), we need
 *  not set call_expected on the called in segno, just the next one.
 *  This is because, for instance, .129 can only be called if .1 is present.
 */
static void
set_ctx_for_present_file(aomd_filehandler_callback_ctx *ctx, int segno)
{
	ctx->present[segno] = true;
	if (segno < (MAX_SEGNO_FILES - MAX_AOREL_CONCURRENCY))
    {
        ctx->call_expected[segno + MAX_AOREL_CONCURRENCY] = true;
        ctx->call_expected[MAX_AOREL_CONCURRENCY * (MaxHeapAttributeNumber + 1) + segno] = true;
    }
}

static int
compareSegnoFiles(bool *expected, bool *result)
{
	size_t numDiffer = 0;
	bool printedOneDiff = false;

	/*
	 * We had two choices for the initialization value of segno:
	 *   A). 0 since ao tables have a 0 segno file
	 *   B). 1 since the foreach() function we are testing here does not
	 *              actually touch segno 0.
	 * We opt for A). since callers of foreach() need to handle that case
	 *  properly, so we want our tests to model that.
	 */
	for (size_t segno=0;segno<MAX_SEGNO_FILES;segno++) {
		if (expected[segno] != result[segno]) {
			numDiffer++;
			/* print only the first error to avoid printing thousands of them */
			if (!printedOneDiff) {
				printedOneDiff = true;
				print_error("MISMATCH for segno: %ld expected: %d got: %d (%s)\n",
							(long)segno, expected[segno], result[segno],
							result[segno] ? "called but should not have been" :
							"not called but should have been");
			}
		}
	}

	return numDiffer;
}

static bool
file_callback(int segno, void *ctx) {
	aomd_filehandler_callback_ctx *myctx = ctx;

	assert_true(segno < MAX_SEGNO_FILES);

	myctx->num_called++;
	myctx->call_result[segno] = true;

	return myctx->present[segno];
}

/*
 * This is the "base" number of the calls we expect when there's no file present (i.e. they are always called).
 * This consists of these number groups:
 *   1. The pair number of the base reilfilenode (i.e. no extension), which is AOTupleId_MultiplierSegmentFileNum * MaxHeapAttributeNumber
 *   2. Numbers in the first concurrency level which are 1 ... 127.
 *   3. The pair numbers of the first concurrency level, which are (AOTupleId_MultiplierSegmentFileNum * MaxHeapAttributeNumber +1 ... +127). 
 *   4. The first number and its pair number in the second concurrency level, which are 128 and AOTupleId_MultiplierSegmentFileNum * MaxHeapAttributeNumber + 128.
 *		Since neither of these numbers are present, we will stop here.
 * Adding them together: 1 + 127 + 127 + 2 = 257
 */
#define EXPECTED_BASE_NUM_CALLS 	257

/* no file present */
static void
test_no_files_present(void **state)
{
    aomd_filehandler_callback_ctx ctx;
    setup_test_structures(&ctx);

    ao_foreach_extent_file(file_callback, &ctx);

    assert_int_equal(ctx.num_called, EXPECTED_BASE_NUM_CALLS);
    assert_int_equal(compareSegnoFiles(ctx.call_expected, ctx.call_result), 0);
}

/* concurrency = 1 num_columns = 1 */
static void
test_co_1_column_1_concurrency(void **state)
{
    aomd_filehandler_callback_ctx ctx;
    setup_test_structures(&ctx);

    for (int col = 0; col < 1; col++)
        set_ctx_for_present_file(&ctx, 1 + col * AOTupleId_MultiplierSegmentFileNum);

    ao_foreach_extent_file(file_callback, &ctx);

    assert_int_equal(ctx.num_called, EXPECTED_BASE_NUM_CALLS + 2 * 1 * 1);
    assert_int_equal(compareSegnoFiles(ctx.call_expected, ctx.call_result), 0);
}

/* concurrency = 1 num_columns = 4 */
static void
test_co_4_columns_1_concurrency(void **state)
{
    aomd_filehandler_callback_ctx ctx;
    setup_test_structures(&ctx);

    for (int col = 0; col < 4; col++)
        set_ctx_for_present_file(&ctx, 1 + col * AOTupleId_MultiplierSegmentFileNum);

    ao_foreach_extent_file(file_callback, &ctx);

    assert_int_equal(ctx.num_called, EXPECTED_BASE_NUM_CALLS + 2 * 4 * 1);
    assert_int_equal(compareSegnoFiles(ctx.call_expected, ctx.call_result), 0);
}

/* concurrency = 1,5 num_columns = 3 */
static void
test_co_3_columns_2_concurrency(void **state)
{
    aomd_filehandler_callback_ctx ctx;
    setup_test_structures(&ctx);

    for (int col = 0; col < 3; col++)
    {
        set_ctx_for_present_file(&ctx, 1 + col * AOTupleId_MultiplierSegmentFileNum);
        set_ctx_for_present_file(&ctx, 5 + col * AOTupleId_MultiplierSegmentFileNum);
    }

    ao_foreach_extent_file(file_callback, &ctx);

    assert_int_equal(ctx.num_called, EXPECTED_BASE_NUM_CALLS + 2 * 3 * 2);
    assert_int_equal(compareSegnoFiles(ctx.call_expected, ctx.call_result), 0);
}

/* concurrency = [1,127] num_columns = 1 */
static void
test_co_1_column_127_concurrency(void **state)
{
    aomd_filehandler_callback_ctx ctx;
    setup_test_structures(&ctx);

    for (int concurrency = 1; concurrency < MAX_AOREL_CONCURRENCY; concurrency++)
        set_ctx_for_present_file(&ctx, concurrency);

    ao_foreach_extent_file(file_callback, &ctx);

    assert_int_equal(ctx.num_called, EXPECTED_BASE_NUM_CALLS + 2 * 1 * 127);
    assert_int_equal(compareSegnoFiles(ctx.call_expected, ctx.call_result), 0);
}

/* concurrency = 0 num_columns = MaxHeapAttributeNumber  */
static void
test_co_max_columns_0th_concurrency(void **state)
{
    aomd_filehandler_callback_ctx ctx;
    setup_test_structures(&ctx);

    for (int col = 0; col < MaxHeapAttributeNumber; col++)
        set_ctx_for_present_file(&ctx, col * MAX_AOREL_CONCURRENCY);

    ao_foreach_extent_file(file_callback, &ctx);

    /* 0th file already acccounted for, hence the -1 */
    assert_int_equal(ctx.num_called, (EXPECTED_BASE_NUM_CALLS - 1) + (MaxHeapAttributeNumber * 2 - 1) - 1);
    assert_int_equal(compareSegnoFiles(ctx.call_expected, ctx.call_result), 0);
}

/* concurrency = 1 num_columns = (MaxHeapAttributeNumber + 1) */
static void
test_co_max_columns_0_1_concurrency(void **state)
{
    aomd_filehandler_callback_ctx ctx;
    setup_test_structures(&ctx);

    for (int col = 0; col < MaxHeapAttributeNumber; col++) {
        set_ctx_for_present_file(&ctx, col * MAX_AOREL_CONCURRENCY);
        set_ctx_for_present_file(&ctx, col * MAX_AOREL_CONCURRENCY + 1);
    }

    ao_foreach_extent_file(file_callback, &ctx);

    /* 0th file already acccount for, hence the -1 */
    assert_int_equal(ctx.num_called, EXPECTED_BASE_NUM_CALLS + 2*(MAX_AOREL_CONCURRENCY -1 + MaxHeapAttributeNumber*2 - 1) - 1);
    assert_int_equal(compareSegnoFiles(ctx.call_expected, ctx.call_result), 0);
}

static void
test_different_number_of_columns_per_concurrency_level(void **state)
{
    aomd_filehandler_callback_ctx ctx;
    setup_test_structures(&ctx);

    set_ctx_for_present_file(&ctx, 1);
    set_ctx_for_present_file(&ctx, 129);
    set_ctx_for_present_file(&ctx, 2);
    set_ctx_for_present_file(&ctx, 130);
    set_ctx_for_present_file(&ctx, 258);

    ao_foreach_extent_file(file_callback, &ctx);

    assert_int_equal(ctx.num_called, EXPECTED_BASE_NUM_CALLS + 10);
    assert_int_equal(compareSegnoFiles(ctx.call_expected, ctx.call_result), 0);
}

static void
test_all_files_present(void **state)
{
	aomd_filehandler_callback_ctx ctx;
	setup_test_structures(&ctx);

	memset(ctx.present, true, sizeof(ctx.present));
	memset(ctx.call_expected, true, sizeof(ctx.call_expected));

	ctx.call_expected[0] = false;  /* caller must deal with .0 file */
	ao_foreach_extent_file(file_callback, &ctx);

	assert_int_equal(ctx.num_called, MAX_SEGNO_FILES - 1);
	assert_int_equal(compareSegnoFiles(ctx.call_expected, ctx.call_result), 0);

	return;
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test(test_no_files_present),
        unit_test(test_co_1_column_1_concurrency),
        unit_test(test_co_4_columns_1_concurrency),
        unit_test(test_co_3_columns_2_concurrency),
        unit_test(test_co_1_column_127_concurrency),
        unit_test(test_co_max_columns_0th_concurrency),
        unit_test(test_co_max_columns_0_1_concurrency),
        unit_test(test_different_number_of_columns_per_concurrency_level),
		unit_test(test_all_files_present),
	};

	MemoryContextInit();

	return run_tests(tests);
}
