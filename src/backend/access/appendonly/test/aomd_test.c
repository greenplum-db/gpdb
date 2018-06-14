#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "access/appendonlywriter.h"
#include "catalog/pg_tablespace.h"

#define PATH_TO_DATA_FILE "/tmp/md_test/1234"
/*
 * file_present is a 1-based array used to determine return values for
 * access/unlink.
 */
static bool file_present[MAX_AOREL_CONCURRENCY * MaxHeapAttributeNumber + 2];
static int num_unlink_called = 0;
static bool unlink_passing = true;

static void
setup_test_structures()
{
	num_unlink_called = 0;
	memset(file_present, false, sizeof(file_present));
	unlink_passing = true;
}

/*
 *******************************************************************************
 * Mocking access and unlink for unittesting
 *******************************************************************************
 */
#undef unlink
#define unlink mock_unlink

int mock_unlink(const char * path)
{
	int ec = 0;
	u_int segfile = 0; /* parse the path */
	char *tmp_path = path + strlen(PATH_TO_DATA_FILE) + 1;
	if (strcmp(tmp_path, "") != 0)
	{
		segfile = atoi(tmp_path);
	}

	if (!file_present[segfile])
	{
		ec = -1;
		errno = ENOENT;
	}
	else
	{
		num_unlink_called++;
	}

#if 0
	elog(WARNING, "UNLINK %s %d num_times_called=%d unlink_passing %d\n",
		path, segfile, num_unlink_called, unlink_passing);
#endif
	return ec;
}
/*
 *******************************************************************************
 */

#include "../aomd.c"

void
test__AOSegmentFilePathNameLen(void **state)
{
	RelationData reldata;
	char	   *basepath = "base/21381/123";

	reldata.rd_node.relNode = 123;
	reldata.rd_node.spcNode = DEFAULTTABLESPACE_OID;
	reldata.rd_node.dbNode = 21381;
	reldata.rd_backend = -1;

	int			r = AOSegmentFilePathNameLen(&reldata);

	assert_in_range(r, strlen(basepath) + 3, strlen(basepath) + 10);
}

void
test__FormatAOSegmentFileName(void **state)
{
	char	   *basepath = "base/21381/123";
	int32		fileSegNo;
	char		filepathname[256];

	/* seg 0, no columns */
	FormatAOSegmentFileName(basepath, 0, -1, &fileSegNo, filepathname);
	assert_string_equal(filepathname, "base/21381/123");
	assert_int_equal(fileSegNo, 0);

	/* seg 1, no columns */
	FormatAOSegmentFileName(basepath, 1, -1, &fileSegNo, filepathname);
	assert_string_equal(filepathname, "base/21381/123.1");
	assert_int_equal(fileSegNo, 1);

	/* seg 0, column 1 */
	FormatAOSegmentFileName(basepath, 0, 1, &fileSegNo, filepathname);
	assert_string_equal(filepathname, "base/21381/123.128");
	assert_int_equal(fileSegNo, 128);

	/* seg 1, column 1 */
	FormatAOSegmentFileName(basepath, 1, 1, &fileSegNo, filepathname);
	assert_string_equal(filepathname, "base/21381/123.129");
	assert_int_equal(fileSegNo, 129);

	/* seg 0, column 2 */
	FormatAOSegmentFileName(basepath, 0, 2, &fileSegNo, filepathname);
	assert_string_equal(filepathname, "base/21381/123.256");
	assert_int_equal(fileSegNo, 256);
}


void
test__MakeAOSegmentFileName(void **state)
{
	char	   *basepath = "base/21381/123";
	int32		fileSegNo;
	char		filepathname[256];
	RelationData reldata;

	reldata.rd_node.relNode = 123;
	reldata.rd_node.spcNode = DEFAULTTABLESPACE_OID;
	reldata.rd_node.dbNode = 21381;
	reldata.rd_backend = -1;

	/* seg 0, no columns */
	MakeAOSegmentFileName(&reldata, 0, -1, &fileSegNo, filepathname);
	assert_string_equal(filepathname, "base/21381/123");
	assert_int_equal(fileSegNo, 0);

	/* seg 1, no columns */
	MakeAOSegmentFileName(&reldata, 1, -1, &fileSegNo, filepathname);
	assert_string_equal(filepathname, "base/21381/123.1");
	assert_int_equal(fileSegNo, 1);

	/* seg 0, column 1 */
	MakeAOSegmentFileName(&reldata, 0, 1, &fileSegNo, filepathname);
	assert_string_equal(filepathname, "base/21381/123.128");
	assert_int_equal(fileSegNo, 128);

	/* seg 1, column 1 */
	MakeAOSegmentFileName(&reldata, 1, 1, &fileSegNo, filepathname);
	assert_string_equal(filepathname, "base/21381/123.129");
	assert_int_equal(fileSegNo, 129);

	/* seg 0, column 2 */
	MakeAOSegmentFileName(&reldata, 0, 2, &fileSegNo, filepathname);
	assert_string_equal(filepathname, "base/21381/123.256");
	assert_int_equal(fileSegNo, 256);
}

void
test_mdunlink_co_no_file_exists(void **state)
{
	setup_test_structures();

	mdunlink_ao(PATH_TO_DATA_FILE);

	// called 1 time checking column
	assert_true(num_unlink_called == 0);
	return;
}

/* concurrency = 1 max_column = 4 */
void
test_mdunlink_co_4_columns_1_concurrency(void **state)
{
	setup_test_structures();

	/* concurrency 1 exists */
	file_present[1] = true;

	/* max column exists */
	file_present[(1*AOTupleId_MultiplierSegmentFileNum) + 1] = true;
	file_present[(2*AOTupleId_MultiplierSegmentFileNum) + 1] = true;
	file_present[(3*AOTupleId_MultiplierSegmentFileNum) + 1] = true;

	mdunlink_ao(PATH_TO_DATA_FILE);

	assert_true(num_unlink_called == 4);
	assert_true(unlink_passing);
	return;
}

/* concurrency = 1,5 max_column = 3 */
void
test_mdunlink_co_3_columns_2_concurrency(void **state)
{
	setup_test_structures();

	/* concurrency 1,5 exists */
	file_present[1] = true;
	file_present[5] = true;

	/* Concurrency 1 files */
	file_present[(1*AOTupleId_MultiplierSegmentFileNum) + 1] = true;
	file_present[(2*AOTupleId_MultiplierSegmentFileNum) + 1] = true;

	/* Concurrency 5 files */
	file_present[(1*AOTupleId_MultiplierSegmentFileNum) + 5] = true;
	file_present[(2*AOTupleId_MultiplierSegmentFileNum) + 5] = true;

	mdunlink_ao(PATH_TO_DATA_FILE);
	assert_true(num_unlink_called == 6);
	assert_true(unlink_passing);
	return;
}

void
test_mdunlink_co_all_columns_full_concurrency(void **state)
{
	setup_test_structures();

	memset(file_present, true, sizeof(file_present));
	file_present[MAX_AOREL_CONCURRENCY * MaxHeapAttributeNumber + 1] = false;

	mdunlink_ao(PATH_TO_DATA_FILE);

	assert_true(num_unlink_called == MaxHeapAttributeNumber * MAX_AOREL_CONCURRENCY);
	assert_true(unlink_passing);
	return;
}

void
test_mdunlink_co_one_columns_one_concurrency(void **state)
{
	setup_test_structures();

	file_present[1] = true;

	mdunlink_ao(PATH_TO_DATA_FILE);
	assert_true(num_unlink_called == 1);
	assert_true(unlink_passing);
	return;
}

void
test_mdunlink_co_one_columns_full_concurrency(void **state)
{
	setup_test_structures();

	for (int filenum=1; filenum < MAX_AOREL_CONCURRENCY; filenum++)
		file_present[filenum] = true;

	mdunlink_ao(PATH_TO_DATA_FILE);
	assert_true(num_unlink_called == 127);
	assert_true(unlink_passing);
	return;
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test(test__AOSegmentFilePathNameLen),
		unit_test(test__FormatAOSegmentFileName),
		unit_test(test__MakeAOSegmentFileName),
		unit_test(test_mdunlink_co_one_columns_full_concurrency),
		unit_test(test_mdunlink_co_one_columns_one_concurrency),
		unit_test(test_mdunlink_co_all_columns_full_concurrency),
		unit_test(test_mdunlink_co_3_columns_2_concurrency),
		unit_test(test_mdunlink_co_4_columns_1_concurrency),
		unit_test(test_mdunlink_co_no_file_exists)
	};

	MemoryContextInit();

	return run_tests(tests);
}
