/*-------------------------------------------------------------------------
 *
 * workfile_mgr_test.c
 *	 Unit tests for workfile manager and cache.
 *
 * Portions Copyright (c) 2012, EMC Corp.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/test/regress/workfile_mgr_test.c
 *
 *-------------------------------------------------------------------------
 */

#include <unistd.h>

#include "postgres.h"

#include "access/xact.h"
#include "cdb/cdbgang.h"
#include "cdb/cdbvars.h"
#include "miscadmin.h"
#include "port/atomics.h"
#include "storage/buffile.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/logtape.h"
#include "utils/memutils.h"
#include "utils/workfile_mgr.h"

/* Number of Workfiles created during the "stress" workfile test */
#define TEST_MAX_NUM_WORKFILES 100000

/* Used to specify created workfiles, default is TEST_MAX_NUM_WORKFILES */
int num_workfiles;

int tests_passed;
int tests_failed;
int tests_total;

/* Test definitions */
static bool execworkfile_buffile_test(void);
static bool fd_tests(void);
static bool buffile_size_test(void);
static bool buffile_large_file_test(void);
static bool logicaltape_test(void);
static bool fd_large_file_test(void);
static bool execworkfile_create_one_MB_file(void);
static bool workfile_fill_sharedcache(void);
static bool workfile_create_and_set_cleanup(void);
static bool workfile_create_and_individual_cleanup(void);
static bool workfile_made_in_temp_tablespace(void);
static bool workfile_create_and_individual_cleanup_with_pinned_workfile_set(void);

static bool atomic_test(void);

/* Unit tests helper */
static void unit_test_result(bool result);
static void unit_test_reset(void);
static bool unit_test_summary(void);

extern Datum gp_workfile_mgr_test_harness(PG_FUNCTION_ARGS);
extern Datum gp_workfile_mgr_create_workset(PG_FUNCTION_ARGS);

#define GET_STR(textp) DatumGetCString(DirectFunctionCall1(textout, PointerGetDatum(textp)))

typedef bool (*gp_workfile_mgr_test)(void);

typedef struct test_def
{
	char *test_name;
	gp_workfile_mgr_test test_func;
} test_def;

static test_def test_defns[] = {
		{"execworkfile_buffile_test", execworkfile_buffile_test},
		{"atomic_test", atomic_test},
		{"fd_tests", fd_tests},
		{"buffile_size_test", buffile_size_test},
		{"buffile_large_file_test", buffile_large_file_test},
		{"logicaltape_test", logicaltape_test},
		{"fd_large_file_test",fd_large_file_test},
		{"execworkfile_create_one_MB_file",execworkfile_create_one_MB_file},
		{"workfile_fill_sharedcache", workfile_fill_sharedcache},
		{"workfile_create_and_set_cleanup", workfile_create_and_set_cleanup},
		{"workfile_create_and_individual_cleanup", workfile_create_and_individual_cleanup},
		{"workfile_made_in_temp_tablespace", workfile_made_in_temp_tablespace},
		{"workfile_create_and_individual_cleanup_with_pinned_workfile_set", workfile_create_and_individual_cleanup_with_pinned_workfile_set},
		{NULL, NULL}, /* This has to be the last element of the array */
};

PG_FUNCTION_INFO_V1(gp_workfile_mgr_test_harness);
Datum
gp_workfile_mgr_test_harness(PG_FUNCTION_ARGS)
{
	bool result = true;

	Assert(PG_NARGS() == 2);

	char *test_name = GET_STR(PG_GETARG_TEXT_P(0));
	bool run_all_tests = strcasecmp(test_name, "all") == 0;
	bool ran_any_tests = false;
	num_workfiles = PG_GETARG_INT32(1);

	if (num_workfiles <= 0 || num_workfiles > TEST_MAX_NUM_WORKFILES)
		num_workfiles = TEST_MAX_NUM_WORKFILES;

	int crt_test = 0;
	while (NULL != test_defns[crt_test].test_name)
	{
		if (run_all_tests || (strcasecmp(test_name, test_defns[crt_test].test_name) == 0))
		{
			result = result && test_defns[crt_test].test_func();
			ran_any_tests = true;
		}
		crt_test++;
	}

	if (!ran_any_tests)
	{
		elog(LOG, "No tests match given name: %s", test_name);
	}

	PG_RETURN_BOOL(ran_any_tests && result);
}

PG_FUNCTION_INFO_V1(gp_workfile_mgr_create_workset);
Datum
gp_workfile_mgr_create_workset(PG_FUNCTION_ARGS)
{
	Assert(PG_NARGS() == 1 || PG_NARGS() == 4);
	bool emptySet = false;
	bool interXact = false;
	bool holdPin = false;
	bool closeFile = false;

	char *worksetName = GET_STR(PG_GETARG_TEXT_P(0));
	if (PG_NARGS() == 1)
		emptySet = true;
	else
	{
		interXact = PG_GETARG_BOOL(1);
		holdPin = PG_GETARG_BOOL(2);
		closeFile = PG_GETARG_BOOL(3);
	}
	
	BufFile *buffile;

	workfile_set *work_set = workfile_mgr_create_set(worksetName, NULL, holdPin);
	Assert(work_set->active);

	if (!emptySet)
	{
		buffile = BufFileCreateTempInSet("workfile_test", interXact, work_set);

		if (closeFile)
			BufFileClose(buffile);
	}

	PG_RETURN_VOID();
}

/*
 * Creates a StringInfo object holding n_chars characters.
 */
static StringInfo
create_text_stringinfo(int64 n_chars)
{
	StringInfo strInfo = makeStringInfo();
	char *text = "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789";

	int64 to_write = n_chars;
	while (to_write >= (int64) strlen(text))
	{
		appendStringInfo(strInfo, "%s", text);
		to_write -= (int64) strlen(text);
	}
	Assert(to_write >= 0 && to_write < (int64) strlen(text));

	/* Pad end */
	while (to_write > 0)
	{
		appendStringInfoChar(strInfo, 'P');
		to_write--;
	}

	return strInfo;
}

/*
 * Unit tests for new ExecWorkfile and WorkfileSegspace functionality
 *  with underlying Buffile files
 *
 * This test is only run when the per-segment limit GUC is non-zero.
 * If GUC is 0, then we don't keep track of the per-segment used size.
 *
 */
static bool
execworkfile_buffile_test(void)
{
/* GPDB_12_MERGE_FIXME: Broken by the BufFile API changes */
#if 0
	int64 result = 0;
	bool success = false;
	int64 expected_size = 0;
	int64 final_size = 0;
	int64 current_size = 0;
	int64 initial_diskspace = WorkfileSegspace_GetSize();

	unit_test_reset();
	elog(LOG, "Running test: execworkfile_buffile_test");
	if (0 == gp_workfile_limit_per_segment)
	{
		elog(LOG, "Skipping test because the gp_workfile_limit_per_segment is 0");
		unit_test_result(true);
		return unit_test_summary();
	}

	/* Create file name */
	char *file_name = "test_execworkfile_buffile.dat";

	elog(LOG, "Running sub-test: Creating EWF/Buffile");

	BufFile *ewf = BufFileCreateNamedTemp(file_name,
										  false /* interXact */,
										  NULL /* work_set */);

	BufFile *write_ewf = ewf;

	unit_test_result(ewf != NULL);

	int nchars = 100000;
	StringInfo text = create_text_stringinfo(nchars);

	elog(LOG, "Running sub-test: Writing small amount data to EWF/Buffile and checking size");

	size_t written = BufFileWrite(ewf, text->data, 20);
	expected_size += 20;

	unit_test_result(written == 20 && expected_size == WorkfileSegspace_GetSize() - initial_diskspace);


	elog(LOG, "Running sub-test: Writing larger amount data (%d bytes) to EWF/Buffile and checking size", nchars);
	written = BufFileWrite(ewf, text->data, nchars);
	expected_size += nchars;

	unit_test_result(written == nchars && expected_size == WorkfileSegspace_GetSize() - initial_diskspace);

	elog(LOG, "Running sub-test: Writing to the middle of a EWF/Buffile and checking size");
	result = BufFileSeek(ewf, 0 /* fileno */, BufFileGetSize(ewf) / 2, SEEK_SET);
	Assert(result == 0);
	/* This write should not add to the size */
	success = BufFileWrite(ewf, text->data, BufFileGetSize(ewf) / 10);

	unit_test_result(success && expected_size == WorkfileSegspace_GetSize() - initial_diskspace);

	elog(LOG, "Running sub-test: Seeking past end and writing data to EWF/Buffile and checking size");
	int past_end_offset = 100;
	int past_end_write = 200;
	result = BufFileSeek(ewf, 0 /* fileno */, BufFileGetSize(ewf) + past_end_offset, SEEK_SET);
	Assert(result == 0);
	written = BufFileWrite(ewf, text->data, past_end_write);
	expected_size += past_end_offset + past_end_write;

	unit_test_result(written == past_end_write && expected_size == WorkfileSegspace_GetSize() - initial_diskspace);

	elog(LOG, "Running sub-test: Closing EWF/Buffile"); // keep it open
	final_size = BufFileGetSize(ewf);
	unit_test_result(final_size == expected_size);

	write_ewf = ewf;

	elog(LOG, "Running sub-test: Opening existing EWF/Buffile and checking size");

	ewf = BufFileOpenNamedTemp(file_name,
							   false /* interXact */);

	current_size = BufFileGetSize(ewf);
	unit_test_result(current_size == final_size);

	elog(LOG, "Running sub-test: Reading from reopened EWF/Buffile file");

	int buf_size = 100;
	char *buf = (char *) palloc(buf_size);

	result = BufFileRead(ewf, buf, buf_size);
	unit_test_result(result == buf_size);
	pfree(buf);

	elog(LOG, "Running sub-test: Closing EWF/Buffile");
	final_size = BufFileGetSize(ewf);
	BufFileClose(ewf);

	unit_test_result(final_size == current_size);

	elog(LOG, "Running sub-test: Closing and deleting file from disk");
	BufFileClose(write_ewf);

	unit_test_result(success);

	pfree(text->data);
	pfree(text);

	return unit_test_summary();
#else
	elog(ERROR, "broken test");
	return 0;
#endif
}

/*
 * Unit test for testing the fd.c FileDiskSize and other new capabilities
 */
static bool
fd_tests(void)
{
	unit_test_reset();
	elog(LOG, "Running test: fd_tests");

	elog(LOG, "Running sub-test: Creating fd file");

	File testFd = OpenNamedTemporaryFile("test_fd.dat",
			true /* create */,
			false /* delOnClose */,
			true /* closeAtEOXact */);

	unit_test_result(testFd > 0);

	elog(LOG, "Running sub-test: Reading size of open empty file");
	int64 fd_size = FileDiskSize(testFd);
	unit_test_result(fd_size == 0L);

	elog(LOG, "Running sub-test: Closing file");
	FileClose(testFd);
	unit_test_result(true);

	elog(LOG, "Running sub-test: Opening existing empty file and reading size");
	testFd = OpenNamedTemporaryFile("test_fd.dat",
			false /* create */,
			false /* delOnClose */,
			true /* closeAtEOXact */);

	fd_size = FileDiskSize(testFd);
	unit_test_result(fd_size == 0L);

	elog(LOG, "Running sub-test: Writing to existing open file, sync and read size");
	int nchars = 10000;
	StringInfo text = create_text_stringinfo(nchars);
	int len_to_write = 5000;
	Assert(len_to_write <= text->len);

	FileWrite(testFd, text->data, len_to_write, 0, 0);
	FileSync(testFd, 0);

	fd_size = FileDiskSize(testFd);
	unit_test_result(fd_size == len_to_write);

	elog(LOG, "Running sub-test: Closing file");
	FileClose(testFd);
	unit_test_result(true);

	pfree(text->data);
	pfree(text);

	return unit_test_summary();
}

/*
 * Unit tests for the buffile size functionality
 */
static bool
buffile_size_test(void)
{
/* GPDB_12_MERGE_FIXME: Broken by the BufFile API changes */
#if 0
	unit_test_reset();
	elog(LOG, "Running test: buffile_size_test");

	elog(LOG, "Running sub-test: Creating buffile");

	/* Create file name */
	char *file_name = "test_buffile.dat";

	BufFile *testBf = BufFileCreateNamedTemp(file_name,
											 false /* interXact */,
											 NULL /* workfile_set */);

	unit_test_result(NULL != testBf);

	elog(LOG, "Running sub-test: Size of newly created buffile");
	int64 test_size = BufFileGetSize(testBf);
	unit_test_result(test_size == 0);

	elog(LOG, "Running sub-test: Writing to new buffile and reading size < bufsize");
	int nchars = 10000;
	int expected_size = nchars;
	StringInfo text = create_text_stringinfo(nchars);
	BufFileWrite(testBf, text->data, nchars);
	pfree(text->data);
	pfree(text);
	test_size = BufFileGetSize(testBf);

	unit_test_result(test_size == expected_size);

	elog(LOG, "Running sub-test: Writing to new buffile and reading size > bufsize");
	nchars = 1000000;
	expected_size += nchars;
	text = create_text_stringinfo(nchars);
	BufFileWrite(testBf, text->data, nchars);
	test_size = BufFileGetSize(testBf);

	unit_test_result(test_size == expected_size);

	elog(LOG, "Running sub-test: seeking back and writing then testing size");
	BufFileSeek(testBf, 0 /* fileno */, expected_size/2, SEEK_SET);
	/* This write should not add to the size */
	BufFileWrite(testBf, text->data, expected_size / 10);
	test_size = BufFileGetSize(testBf);

	unit_test_result(test_size == expected_size);

	elog(LOG, "Running sub-test: Opening existing and testing size");
	BufFile *testBf1 = BufFileOpenNamedTemp(file_name,
								  false /*interXact */);
	test_size = BufFileGetSize(testBf1);

	unit_test_result(test_size == expected_size);

	elog(LOG, "Running sub-test: Seek past end, appending and testing size");
	int past_end_offset = 100;
	int past_end_write = 200;
	BufFileSeek(testBf1, 0 /* fileno */, expected_size + past_end_offset, SEEK_SET);
	BufFileWrite(testBf1, text->data, past_end_write);
	expected_size += past_end_offset + past_end_write;
	test_size = BufFileGetSize(testBf1);

	unit_test_result(test_size == expected_size);

	elog(LOG, "Running sub-test: Closing buffile");
	BufFileClose(testBf1);
	BufFileClose(testBf);
	unit_test_result(true);

	pfree(text->data);
	pfree(text);

	return unit_test_summary();
#else
	elog(ERROR, "broken test");
	return 0;
#endif
}

/*
 * Unit test for the atomic functions
 *
 * These are functional tests, they only test for correctness with no concurrency
 *
 */
static bool
atomic_test(void)
{
	unit_test_reset();
	elog(LOG, "Running test: atomic_test");

	{
		elog(LOG, "Running sub-test: pg_atomic_compare_exchange_u64");
		uint64 dest = 5;
		uint64 old = 5;
		uint64 new = 6;

		elog(LOG, "Before: dest=%d, old=%d, new=%d", (uint32) dest, (uint32) old, (uint32) new);
		int32 result = pg_atomic_compare_exchange_u64((pg_atomic_uint64 *)&dest, &old, new);
		elog(LOG, "After: dest=%d, old=%d, new=%d, result=%d", (uint32) dest, (uint32) old, (uint32) new, (uint32) result);
		unit_test_result(dest == new);
	}

	{
		elog(LOG, "Running sub-test: pg_atomic_add_fetch_u64 small addition");

		int64 base = 25;
		int64 inc = 3;
		int64 result = 0;
		int64 expected_result = base + inc;
		elog(DEBUG1, "Before: base=%lld, inc=%lld, result=%lld", (long long int) base, (long long int) inc, (long long int) result);
		result = pg_atomic_add_fetch_u64((pg_atomic_uint64 *)&base, inc);
		elog(DEBUG1, "After: base=%lld, inc=%lld, result=%lld", (long long int) base, (long long int) inc, (long long int) result);
		unit_test_result(result == expected_result && base == expected_result);

		elog(LOG, "Running sub-test: pg_atomic_sub_fetch_u64 small subtraction");

		inc = 4;
		result = 0;
		expected_result = base + inc;
		elog(DEBUG1, "Before: base=%lld, inc=%lld, result=%lld", (long long int) base, (long long int) inc, (long long int) result);
		result = pg_atomic_sub_fetch_u64((pg_atomic_uint64 *)&base, inc);
		elog(DEBUG1, "After: base=%lld, inc=%lld, result=%lld", (long long int) base, (long long int) inc, (long long int) result);
		unit_test_result(result == expected_result && base == expected_result);

		elog(LOG, "Running sub-test: pg_atomic_add_fetch_u64 huge addition");
		base = 37421634719307;
		inc  = 738246483234;
		result = 0;
		expected_result = base + inc;
		elog(DEBUG1, "Before: base=%lld, inc=%lld, result=%lld", (long long int) base, (long long int) inc, (long long int) result);
		result = pg_atomic_add_fetch_u64((pg_atomic_uint64 *)&base, inc);
		elog(DEBUG1, "After: base=%lld, inc=%lld, result=%lld", (long long int) base, (long long int) inc, (long long int) result);
		unit_test_result(result == expected_result && base == expected_result);


		elog(LOG, "Running sub-test: pg_atomic_sub_fetch_u64 huge subtraction");
		inc  = 32738246483234;
		result = 0;
		expected_result = base + inc;
		elog(DEBUG1, "Before: base=%lld, inc=%lld, result=%lld", (long long int) base, (long long int) inc, (long long int) result);
		result = pg_atomic_sub_fetch_u64((pg_atomic_uint64 *)&base, inc);
		elog(DEBUG1, "After: base=%lld, inc=%lld, result=%lld", (long long int) base, (long long int) inc, (long long int) result);
		unit_test_result(result == expected_result && base == expected_result);
	}


	return unit_test_summary();
}

/*
 * Unit test for BufFile support of large files (greater than 4 GB).
 *
 */
static bool
buffile_large_file_test(void)
{
/* GPDB_12_MERGE_FIXME: Broken by the BufFile API changes */
#if 0
	unit_test_reset();
	elog(LOG, "Running test: buffile_large_file_test");
	char *file_name = "Test_large_buff.dat";

	BufFile *bfile = BufFileCreateNamedTemp(file_name,
											true /* interXact */,
											NULL /* workfile_set */);

	int nchars = 100000;
	/* 4.5 GBs */
	int total_entries = 48319;
	/* Entry that requires an int64 seek */
	int test_entry = 45000;

	StringInfo test_string = create_text_stringinfo(nchars);

	elog(LOG, "Running sub-test: Creating file %s", file_name);

	for (int i = 0; i < total_entries; i++)
	{
		if (test_entry == i)
		{
			BufFileWrite(bfile, test_string->data , nchars*sizeof(char));
		}
		else
		{
			StringInfo text = create_text_stringinfo(nchars);

			BufFileWrite(bfile, text->data , nchars*sizeof(char));

			pfree(text->data);
			pfree(text);
		}
	}
	elog(LOG, "Running sub-test: Reading record %s", file_name);

	char *buffer = palloc(nchars * sizeof(char));

	BufFileSeek(bfile, 0 /* fileno */, (int64) ((int64)test_entry * (int64) nchars), SEEK_SET);

	int nread = BufFileRead(bfile, buffer, nchars*sizeof(char));

	BufFileClose(bfile);

	/* Verify correct size of the inserted record and content */
	unit_test_result (nread == nchars &&
					 (strncmp(test_string->data, buffer, test_string->len) == 0));

	pfree(test_string->data);
	pfree(test_string);

	return unit_test_summary();
#else
	elog(ERROR, "broken test");
	return 0;
#endif
}

/*
 * Unit test for logical tape's support for large spill files.
 */
static bool
logicaltape_test(void)
{
/* GPDB_12_MERGE_FIXME: tuplesort and logtape.c were replaced with upstream versions
 * which broke this
 */
#if 0
	unit_test_reset();
	elog(LOG, "Running test: logicaltape_test");

	int max_tapes = 10;
	int nchars = 100000;
	/* 4.5 GBs */
	int max_entries = 48319;

	/* Target record values */
	int test_tape = 5;
	int test_entry = 45000;
	LogicalTapePos entryPos;

	LogicalTapeSet *tape_set = LogicalTapeSetCreate(max_tapes);

	LogicalTape *work_tape = NULL;

	StringInfo test_string = create_text_stringinfo(nchars);

	elog(LOG, "Running sub-test: Creating LogicalTape");

	/* Fill LogicalTapeSet */
	for (int i = 0; i < max_tapes; i++)
	{
		work_tape = LogicalTapeSetGetTape(tape_set, i);

		/* Create large SpillFile for LogicalTape */
		if (test_tape == i)
		{
			elog(LOG, "Running sub-test: Fill LogicalTape");
			for (int j = 0; j < max_entries; j++)
			{
				if ( j == test_entry)
				{
					/* Keep record position of target record in LogicalTape */
					LogicalTapeUnfrozenTell(tape_set, work_tape, &entryPos);

					LogicalTapeWrite(tape_set, work_tape, test_string->data, (size_t)test_string->len);
				}
				else
				{
					/* Add additional records */
					StringInfo text = create_text_stringinfo(nchars);

					LogicalTapeWrite(tape_set, work_tape, text->data, (size_t)text->len);

					pfree(text->data);
					pfree(text);
				}
			}
		}
		else
		{
			/* Add additional records */
			StringInfo text = create_text_stringinfo(nchars);

			LogicalTapeWrite(tape_set, work_tape, text->data, (size_t)text->len);

			pfree(text->data);
			pfree(text);
		}

	}

	/* Set target LogicalTape */
	work_tape = LogicalTapeSetGetTape(tape_set, test_tape);
	char *buffer = palloc(nchars * sizeof(char));

	elog(LOG, "Running sub-test: Freeze LogicalTape");
	LogicalTapeFreeze(tape_set, work_tape);

	elog(LOG, "Running sub-test: Seek in LogicalTape");
	LogicalTapeSeek(tape_set, work_tape, &entryPos);

	elog(LOG, "Running sub-test: Reading from LogicalTape");
	LogicalTapeRead(tape_set, work_tape, buffer, (size_t)(nchars*sizeof(char)));

	LogicalTapeSetClose(tape_set, NULL /* work_set */);

	unit_test_result (strncmp(test_string->data, buffer, test_string->len) == 0);

	return unit_test_summary();
#else
	elog(ERROR, "broken test");
	return 0;
#endif
}

/*
 * Unit test for testing large fd file.
 * This unit test verifies that the file's size on disk is as expected.
 */
static bool
fd_large_file_test(void)
{
	off_t		offset;

	unit_test_reset();
	elog(LOG, "Running test: fd_large_file_test");

	elog(LOG, "Running sub-test: Creating fd file");

	/* Create file name */
	char *file_name = "test_large_fd.dat";

	File testFd = OpenNamedTemporaryFile(file_name,
			true /* create */,
			true /* delOnClose */,
			true /* closeAtEOXact */);

	unit_test_result(testFd > 0);

	elog(LOG, "Running sub-test: Writing to existing open file, sync and read size");
	int nchars = 100000;
	/* 4.5 GBs */
	int total_entries = 48319;

	StringInfo text = create_text_stringinfo(nchars);

	offset = 0;
	for (int i = 0; i < total_entries; i++)
	{
		int len = strlen(text->data);
		FileWrite(testFd, text->data, len, offset, 0);
		offset += len;
		FileSync(testFd, 0);
	}

	pfree(text->data);
	pfree(text);

	int64 fd_size = FileDiskSize(testFd);
	unit_test_result(fd_size == (int64) nchars * sizeof(char) * (int64) total_entries);

	elog(LOG, "Running sub-test: Closing file");
	FileClose(testFd);
	unit_test_result(true);

	return unit_test_summary();
}

/*
 * Unit test for writing a one MB execworkfile.
 *
 */
static bool
execworkfile_create_one_MB_file(void)
{
/* GPDB_12_MERGE_FIXME: Broken by the BufFile API changes */
#if 0
	unit_test_reset();
	elog(LOG, "Running test: execworkfile_one_MB_file_test");

	StringInfo filename = makeStringInfo();

	appendStringInfo(filename,
					 "%s",
					 "Test_buffile_one_MB_file_test.dat");

	BufFile *ewf = BufFileCreateNamedTemp(filename->data,
										  false /* interXact */,
										  NULL /* work_set */);

	/* Number of characters in a MB */
	int nchars = (int)((1<<20)/sizeof(char));

	elog(LOG, "Running sub-test: Creating file %s", filename->data);

	StringInfo text = create_text_stringinfo(nchars);

	BufFileWrite(ewf, text->data, nchars*sizeof(char));

	pfree(text->data);
	pfree(text);

	elog(LOG, "Running sub-test: Closing file %s", filename->data);

	int64 final_size = BufFileGetSize(ewf);

	BufFileClose(ewf);

	/* Verify correct size of the created file */
	unit_test_result (final_size == (int64)nchars*sizeof(char) );

	pfree(filename->data);
	pfree(filename);

	return unit_test_summary();
#else
	elog(ERROR, "broken test");
	return 0;
#endif
}

/*
 * Unit test that inserts many entries in the workfile mgr shared cache
 */
static bool
workfile_fill_sharedcache(void)
{
	bool success = true;

	unit_test_reset();
	elog(LOG, "Running test: workfile_fill_sharedcache");

	int n_entries = gp_workfile_max_entries + 1;

	elog(LOG, "Running sub-test: Creating %d empty workfile sets", n_entries);

	int crt_entry = 0;
	for (crt_entry = 0; crt_entry < n_entries; crt_entry++)
	{
		workfile_set *work_set = workfile_mgr_create_set("workfile_test", NULL, false /* hold pin */);
		if (NULL == work_set)
		{
			success = false;
			break;
		}
		if (crt_entry >= gp_workfile_max_entries - 2)
		{
			/* Pause between adding extra ones so we can test from other sessions */
			elog(LOG, "Added %d entries out of %d, pausing for 30 seconds before proceeding", crt_entry + 1, n_entries);
			sleep(30);
		}

	}

	unit_test_result(success);

	return unit_test_summary();

}

/*
 * Unit test that creates very many workfiles, and then cleans them up
 */
static bool
workfile_create_and_set_cleanup(void)
{
	bool success = true;

	unit_test_reset();
	elog(LOG, "Running test: workfile_create_and_set_cleanup");

	elog(LOG, "Running sub-test: Create Workset");

	workfile_set *work_set = workfile_mgr_create_set("workfile_test", NULL, false /* hold pin */);

	unit_test_result(NULL != work_set);

	elog(LOG, "Running sub-test: Create %d workfiles", num_workfiles);

	BufFile **ewfiles = (BufFile **) palloc(num_workfiles * sizeof(BufFile *));

	for (int i=0; i < num_workfiles; i++)
	{
		ewfiles[i] = BufFileCreateTempInSet("workfile_test", false /* interXact */, work_set);

		if (ewfiles[i] == NULL)
		{
			success = false;
			break;
		}

		if (i % 1000 == 999)
		{
			elog(LOG, "Created %d workfiles so far", i);
		}
	}
	unit_test_result(success);

	elog(LOG, "Running sub-test: Closing Workset");
	workfile_mgr_close_set(work_set);

	unit_test_result(!work_set->active);

	return unit_test_summary();
}

static bool
workfile_made_in_temp_tablespace(void)
{
	const char *bufFilePath;
	BufFile *bufFile;
	bool		success = true;

	unit_test_reset();

	/*
	 * Set temp_tablespaces at a session level to simulate what a user does
	 */
	SetConfigOption("temp_tablespaces", "work_file_test_ts", PGC_INTERNAL, PGC_S_SESSION);

	workfile_set *work_set = workfile_mgr_create_set("workfile_test", NULL, false /* hold pin */);

	/*
	 * BufFileCreateTempInSet calls PrepareTempTablespaces
	 * which parses the temp_tablespaces value and BufFileCreateTempInSet
	 * uses that value as the location for workfile created
	 */
	bufFile = BufFileCreateTempInSet("workfile_test", false, work_set);

	if (bufFile == NULL)
		success = false;

	bufFilePath = BufFileGetFilename(bufFile);

	char *expectedPathPrefix = "pg_tblspc/";

	/*
	 * Expect workfile to be created in the temp tablespace specified above,
	 * which will have the prefix, "pg_tblspc". By default,
	 * workfiles are created in data directory having prefix, "base"
	 */
	if(0 != strncmp(bufFilePath, expectedPathPrefix, strlen(expectedPathPrefix)))
		success = false;

	BufFileClose(bufFile);

	unit_test_result(!work_set->active);

	return unit_test_summary();
}

/*
 * Unit test that creates very many workfiles, and then closes them one by one
 */
static bool
workfile_create_and_individual_cleanup(void)
{
	bool success = true;

	unit_test_reset();
	elog(LOG, "Running test: workfile_create_and_individual_cleanup");

	elog(LOG, "Running sub-test: Create Workset");

	workfile_set *work_set = workfile_mgr_create_set("workfile_test", NULL, false /* hold pin */);

	unit_test_result(NULL != work_set);

	elog(LOG, "Running sub-test: Create %d workfiles", num_workfiles);

	BufFile **ewfiles = (BufFile **) palloc(num_workfiles * sizeof(BufFile *));

	for (int i=0; i < num_workfiles; i++)
	{
		ewfiles[i] = BufFileCreateTempInSet("workfile_test", false /* interXact */, work_set);

		if (ewfiles[i] == NULL)
		{
			success = false;
			break;
		}

		if (i % 1000 == 999)
		{
			elog(LOG, "Created %d workfiles so far", i);
		}
	}
	unit_test_result(success);

	elog(LOG, "Running sub-test: Closing %d workfiles", num_workfiles);

	for (int i=0; i < num_workfiles; i++)
	{
		BufFileClose(ewfiles[i]);

		if (i % 1000 == 999)
		{
			elog(LOG, "Closed %d workfiles so far", i);
		}
	}
	unit_test_result(success);

	/* the workfile_set should be freed since all it's files are closed */
	unit_test_result(!work_set->active);

	return unit_test_summary();
}

/*
 * Unit test that creates very many workfiles with pinned workfile_set, and
 * then closes them one by one
 */
static bool
workfile_create_and_individual_cleanup_with_pinned_workfile_set(void)
{
	bool success = true;

	unit_test_reset();
	elog(LOG, "Running test: workfile_create_and_individual_cleanup");

	elog(LOG, "Running sub-test: Create Workset");

	workfile_set *work_set = workfile_mgr_create_set("workfile_test", NULL, true /* hold pin */);

	unit_test_result(NULL != work_set);

	elog(LOG, "Running sub-test: Create %d workfiles", num_workfiles);

	BufFile **ewfiles = (BufFile **) palloc(num_workfiles * sizeof(BufFile *));

	for (int i=0; i < num_workfiles; i++)
	{
		ewfiles[i] = BufFileCreateTempInSet("workfile_test", false /* interXact */, work_set);

		if (ewfiles[i] == NULL)
		{
			success = false;
			break;
		}

		if (i % 1000 == 999)
		{
			elog(LOG, "Created %d workfiles so far", i);
		}
	}
	unit_test_result(success);

	elog(LOG, "Running sub-test: Closing %d workfiles", num_workfiles);

	for (int i=0; i < num_workfiles; i++)
	{
		BufFileClose(ewfiles[i]);

		if (i % 1000 == 999)
		{
			elog(LOG, "Closed %d workfiles so far", i);
		}
	}
	unit_test_result(success);

	/* the workfile_set should not be freed since it gets pinned */
	unit_test_result(work_set->active);

	/* free the workfile_set */
	workfile_mgr_close_set(work_set);

	unit_test_result(!work_set->active);

	return unit_test_summary();
}

static bool
unit_test_summary(void)
{
	elog(LOG, "Unit tests summary: PASSED: %d/%d, FAILED: %d/%d",
			tests_passed, tests_total,
			tests_failed, tests_total);
	return tests_failed == 0;
}

static void
unit_test_reset()
{
	tests_passed = tests_failed = tests_total = 0;
}

static void
unit_test_result(bool result)
{
	tests_total++;

	if (result)
	{
		tests_passed++;
		elog(LOG, "====== PASS ======");
	}
	else
	{
		tests_failed++;
		elog(LOG, "!!!!!! FAIL !!!!!!");
	}
}
