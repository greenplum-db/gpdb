#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "c.h"
#include "storage/bfz.h"

/* Ignore ereport */
#include "utils/elog.h"
#undef ereport
#undef errcode
#undef errmsg
#define ereport
#define errcode
#define errmsg

#include "../bfz.c"

/* ==================== bfz_scan_begin =================== */
/*
 * Tests that bfz->freeable_stuff->tot_bytes is initialized to 0
 * in bfz_scan_begin
 */
void
test__bfz_scan_begin_initbytes(void **state)
{

	bfz_t *bfz = palloc0(sizeof(bfz_t));
	bfz->compression_index = 0; /* compress_nothing */

	bfz->mode = BFZ_MODE_FREED;
	bfz->fd = 1;

	bfz_scan_begin(bfz);

	assert_true(bfz->mode == BFZ_MODE_SCAN);

	struct bfz_freeable_stuff *fs = bfz->freeable_stuff;
	assert_true(fs->buffer_pointer == fs->buffer_end);
	assert_true((void *) fs->buffer_end == (void *) fs->buffer);
	assert_true(fs->tot_bytes == 0L);

}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__bfz_scan_begin_initbytes)
	};

	MemoryContextInit();

	return run_tests(tests);
}
