#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../aomd.c"

#include "catalog/pg_tablespace.h"

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


int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test(test__AOSegmentFilePathNameLen),
		unit_test(test__FormatAOSegmentFileName),
		unit_test(test__MakeAOSegmentFileName)
	};

	MemoryContextInit();

	return run_tests(tests);
}
