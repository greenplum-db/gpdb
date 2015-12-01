#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"


/*
#include "../mdlite_utils.c"
#include "../mdlite_global_generation.c"
#include "../mdlite_local_generation.c"
#include "../mdlite_inv_translator.c"
 */

#include "../../../../include/utils/mdlite.h"

/* Provide specialized mock implementations for memory allocation functions */
#define palloc0 mdlite_palloc0_mock
/*
 * This is a mocked version of palloc0.
 * It asserts that it is executed in the TopMemoryContext.
 */
void *
mdlite_palloc0_mock(Size size)
{
    assert_int_equal(CurrentMemoryContext, TopMemoryContext);
    return MemoryContextAllocZero(CurrentMemoryContext, size);
}

/* 
 * Test mdlite_global_generation.c functions 
 */

 /* Testing global cache generation memory for default value */
void
test__mdlite_shmem_init__NULL_memory(void** state)
{
    assert_true(NULL == mdlite_global_generation);
}

/* Testing size of shared memory */
void 
test__mdlite_shmem_size__uint64(void** state)
{
    assert_true(sizeof(uint64) == mdlite_shmem_size());
}

/*
 * Test mdlite_local_generations.c functions
 */

/* Testing default value for newly created local cache generations objects */
void 
test__mdlite_create_local__default_value(void** state)
{
    mdlite_local* local_mdlite = mdlite_create_local();
    assert_true(local_mdlite->bump_cmd_id == DEFAULT_BUMP_CMD_ID &&
                local_mdlite->local_generation == 0);
    pfree(local_mdlite);
}

/* Testing setting bump id functionality */
void 
test__mdlite_local_bump_cmd_id(void** state)
{
    mdlite_local local;
    will_return(GetCurrentLocalMDLite, &local);
    will_return(GetCurrentCommandId, (CommandId)10);
    mdlite_local_bump_cmd_id();
    assert_true(local.bump_cmd_id == 10);
}

/*
 * Test mdlite_inv_translator.c functions
 */


/* Testing invalidate translator functionality*/
void 
test__mdlite_inv_translator__set_bmp(void** state)
{
    mdlite_local local;
    Relation relation;
    expect_any(IsAoSegmentRelation, relation);
    will_return(IsAoSegmentRelation, false);
    will_return(mdlite_enabled, true);
    will_return(GetCurrentLocalMDLite, &local);
    will_return(GetCurrentCommandId, (CommandId)10);
    mdlite_inv_translator(relation);
    printf("%d", local.bump_cmd_id);
    assert_true(local.bump_cmd_id == 10);
    
}

int
main(int argc, char* argv[])
{
    cmockery_parse_arguments(argc, argv);
    
    const UnitTest tests[] = {
		unit_test(test__mdlite_shmem_init__NULL_memory),
                unit_test(test__mdlite_shmem_size__uint64),
                unit_test(test__mdlite_create_local__default_value),
                unit_test(test__mdlite_local_bump_cmd_id),
                unit_test(test__mdlite_inv_translator__set_bmp)
	};

    return run_tests(tests);
}

