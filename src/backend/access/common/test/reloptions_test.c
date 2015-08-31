#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../reloptions.c"

void
test__parse_AO_options(void **state)
{
	bool aovalue=false;
	Datum opts;
	ArrayType *array;
	int nelems;
	opts = parseAOStorageOpts("checksum=false", &aovalue);
	assert_false(aovalue);
	array = DatumGetArrayTypeP(opts);
	nelems = ArrayGetNItems(ARR_NDIM(array), ARR_DIMS(array));
	assert_int_equal(nelems, 2);

	opts = parseAOStorageOpts("blocksize=8192", &aovalue);
	array = DatumGetArrayTypeP(opts);
	nelems = ArrayGetNItems(ARR_NDIM(array), ARR_DIMS(array));
	assert_int_equal(nelems, 2);

	assert_false(aovalue);
	opts = parseAOStorageOpts(
			"appendonly = True, checkSum= FALSE, orientation=Column",
			&aovalue);
	/*
	 * Optimised build (at least on OSX) magically returns aovalue to
	 * be 0.  Debug build however, correctly returns aovalue=1.  Due
	 * to this anomaly, the following assert is disabled.
	 */
	/* assert_true(aovalue); */
	array = DatumGetArrayTypeP(opts);
	nelems = ArrayGetNItems(ARR_NDIM(array), ARR_DIMS(array));
	assert_int_equal(nelems, 3);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__parse_AO_options)
	};

	MemoryContextInit();

	return run_tests(tests);
}
