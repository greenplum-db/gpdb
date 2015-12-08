#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../mdver_local_generation.c"
extern int gp_command_count;

/*
 * test__mdver_create_local__default_value
 * 		Testing default value for newly created local cache generations objects
 */
void 
test__mdver_create_local__default_value(void** state)
{
    will_return(mdver_get_global_generation, 42);
    mdver_local* local_mdver = mdver_create_local();
    assert_true(local_mdver->transaction_dirty == false &&
                local_mdver->local_generation == 42);
    pfree(local_mdver);
}

/*
 * test__mdver_mark_dirty_xact__set_flag
 * 		Testing that mdver_mark_dirty_xact sets the flag in the local context
 */
void 
test__mdver_mark_dirty_xact__set_flag(void** state)
{
	mdver_local local;
	local.transaction_dirty = false;
    mdver_mark_dirty_xact(&local);
    assert_true(local.transaction_dirty);
}

/*
 * test__mdver_command_begin__set_local_generation
 *  Testing that at command begin, we correctly check the local and global
 *  generation and take action:
 *    - if LG == GG, mdver_dirty_mdcache = false nothing to do, return false
 *    - if LG == GG, mdver_dirty_mdcache = true set LG = GG and return true
 *    - if LG != GG, then set LG = GG and return true
 */
void
test__mdver_command_begin__set_local_generation(void **state)
{
	mdver_local local_mdver = {0, false};
	bool result = false;

	/* local_generation == global_generation == 10, mdver_dirty_mdcache = false */
	mdver_dirty_mdcache = false;
	local_mdver.local_generation = 10;
	will_return(mdver_enabled, true);
	will_return(mdver_get_global_generation, 10);
	will_return(GetCurrentLocalMDVer, &local_mdver);

	result = mdver_command_begin();
	assert_false(result);
	assert_true(local_mdver.local_generation == 10);
	assert_false(local_mdver.transaction_dirty);
	assert_false(mdver_dirty_mdcache);

	/* local_generation == global_generation == 10, mdver_dirty_mdcache = false */
	mdver_dirty_mdcache = true;
	local_mdver.local_generation = 10;
	will_return(mdver_enabled, true);
	will_return(mdver_get_global_generation, 10);
	will_return(GetCurrentLocalMDVer, &local_mdver);

	result = mdver_command_begin();
	assert_true(result);
	assert_true(local_mdver.local_generation == 10);
	assert_false(local_mdver.transaction_dirty);
	assert_false(mdver_dirty_mdcache);

	/* local_generation = 10, global_generation = 15. mdver_dirty_mdcache = true */
	mdver_dirty_mdcache = true;
	local_mdver.local_generation = 10;
	will_return(mdver_enabled, true);
	will_return(mdver_get_global_generation, 15);
	will_return(GetCurrentLocalMDVer, &local_mdver);

	result = mdver_command_begin();

	assert_true(result);
	assert_true(local_mdver.local_generation == 15);
	assert_false(local_mdver.transaction_dirty);
	assert_false(mdver_dirty_mdcache);

	/* local_generation = 10, global_generation = 15. mdver_dirty_mdcache = false */
	mdver_dirty_mdcache = false;
	local_mdver.local_generation = 10;
	will_return(mdver_enabled, true);
	will_return(mdver_get_global_generation, 15);
	will_return(GetCurrentLocalMDVer, &local_mdver);

	result = mdver_command_begin();

	assert_true(result);
	assert_true(local_mdver.local_generation == 15);
	assert_false(local_mdver.transaction_dirty);
	assert_false(mdver_dirty_mdcache);
}

int
main(int argc, char* argv[])
{
    cmockery_parse_arguments(argc, argv);
    
    const UnitTest tests[] = {
    		unit_test(test__mdver_create_local__default_value),
			unit_test(test__mdver_mark_dirty_xact__set_flag),
			unit_test(test__mdver_command_begin__set_local_generation)
	};

    return run_tests(tests);
}
