#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../mdver_inv_translator.c"

/*
 * test__mdver_inv_translator__set_bmp
 * 		Testing invalidate translator functionality
 */
void 
test__mdver_inv_translator__set_bmp(void** state)
{
	mdver_local local = {0, false};
	Relation relation;

	/* When relation is AoSegmentRelation, ignore the event */
	local.transaction_dirty = false;
	mdver_dirty_mdcache = false;
	will_return(mdver_enabled, true);
	will_return(GetCurrentLocalMDVer, &local);
	expect_any(IsAoSegmentRelation, relation);
	will_return(IsAoSegmentRelation, true);
	mdver_inv_translator(relation);
	assert_false(local.transaction_dirty);
	assert_false(mdver_dirty_mdcache);

	/* A real invalidation, dirty flag for command and transaction set */
	local.transaction_dirty = false;
	mdver_dirty_mdcache = false;
	will_return(mdver_enabled, true);
	will_return(GetCurrentLocalMDVer, &local);
	expect_any(IsAoSegmentRelation, relation);
	will_return(IsAoSegmentRelation, false);
	mdver_inv_translator(relation);
	assert_true(local.transaction_dirty);
	assert_true(mdver_dirty_mdcache);
}

/* Testing that the global dirty flag is set to true */
void
test__mdver_mark_dirty_mdcache(void **state)
{
	mdver_dirty_mdcache = false;
	mdver_mark_dirty_mdcache();
	assert_true(mdver_dirty_mdcache);
}

int
main(int argc, char* argv[])
{
    cmockery_parse_arguments(argc, argv);
    
    const UnitTest tests[] = {
		unit_test(test__mdver_inv_translator__set_bmp),
		unit_test(test__mdver_mark_dirty_mdcache)
	};

    return run_tests(tests);
}
