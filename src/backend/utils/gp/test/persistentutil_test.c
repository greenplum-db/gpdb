#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "../persistentutil.c"

typedef struct dirent dirent;

void
test__strToRelfilenode_converts_larger_than_max_signed_int(void **state)
{
	/* Purposely adding an extra dot to make the strToRelfilenode believe that
	 * we don't have correct segument number to return false */
	char * d_name = "2147483648.1.1"; /*2147483647 is the
										MAX_INT limit for a
										signed int */
	Oid relfilenode = InvalidOid;
	int32 segmentnum = -1;
	bool ret = strToRelfilenode(d_name, &relfilenode, &segmentnum);

	assert_true(relfilenode == 2147483648);
	assert_true(segmentnum == 1);
	assert_false(ret);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);
	const UnitTest tests[] = {
		unit_test(test__strToRelfilenode_converts_larger_than_max_signed_int),
	};

	return run_tests(tests);
}
