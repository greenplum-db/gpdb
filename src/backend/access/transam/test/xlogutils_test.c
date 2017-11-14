#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../xlogutils.c"

/*
 * Test that the invalid_page_tab hash table is properly used when invalid AO
 * segment files are encountered.
 */
void
test_ao_invalid_segment_file(void **state)
{
	RelFileNode relfilenode;
	int32 segmentFileNum;

	/* create mock relfilenode */
	relfilenode.spcNode = DEFAULTTABLESPACE_OID;
	relfilenode.dbNode = 12087 /* postgres database */;
	relfilenode.relNode = FirstNormalObjectId;

	segmentFileNum = 2;

	/* invalid_page_tab should not be initialized */
	assert_true(invalid_page_tab == NULL);

	/* initialize invalid_page_tab and log an invalid AO segment file */
	XLogAOSegmentFile(relfilenode, segmentFileNum);
	assert_false(invalid_page_tab == NULL);
	assert_int_equal(hash_get_num_entries(invalid_page_tab), 1);

	/* try again but should not put in same entry */
	XLogAOSegmentFile(relfilenode, segmentFileNum);
	assert_int_equal(hash_get_num_entries(invalid_page_tab), 1);

	/* forget the invalid AO segment file */
	XLogAODropSegmentFile(relfilenode, segmentFileNum);
	assert_int_equal(hash_get_num_entries(invalid_page_tab), 0);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_ao_invalid_segment_file)
	};
	return run_tests(tests);
}
