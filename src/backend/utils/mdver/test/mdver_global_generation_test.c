#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../mdver_global_generation.c"


 /*
  *test__mdver_shmem_init__NULL_memory
  * 	Testing global cache generation memory for default value
  */
void
test__mdver_shmem_init__NULL_memory(void** state)
{
    assert_true(NULL == mdver_global_generation);
}

 /*
  * test__mdver_shmem_init__No_NULL
  * 	Testing global cache generation memory by mocking ShmemInitStruct for memory
  */
void
test__mdver_shmem_init__No_NULL(void** state)
{
    uint64 global_cache_generation = 0;
    expect_any(ShmemInitStruct, name);
    expect_any(ShmemInitStruct, size);
    expect_any(ShmemInitStruct, foundPtr);
    will_return(ShmemInitStruct, &global_cache_generation);
    mdver_shmem_init();
    assert_true(global_cache_generation = mdver_global_generation && 0 == *mdver_global_generation);
}

/*
 * test__mdver_shmem_size__uint64
 * 		Testing size of shared memory
 */
void 
test__mdver_shmem_size__uint64(void** state)
{
	GpIdentity.segindex = MASTER_CONTENT_ID;
    assert_true(sizeof(uint64) == mdver_shmem_size());

    GpIdentity.segindex = 42;
    assert_true(0 == mdver_shmem_size());
}

/* test__mdver_bump_global_generation__inc
 * 		Testing bumping global generation
 */
void
test__mdver_bump_global_generation__inc(void** state)
{
	mdver_local local;
    uint64 global_cache_generation = 0;
    expect_any(ShmemInitStruct, name);
    expect_any(ShmemInitStruct, size);
    expect_any(ShmemInitStruct, foundPtr);
    will_return(ShmemInitStruct, &global_cache_generation);
    mdver_shmem_init();

    /* If transaction_dirty is false, leave global generation unchanged */
    local.transaction_dirty = false;
    will_return(mdver_enabled, true);
    mdver_bump_global_generation(&local);
    assert_true(NULL != mdver_global_generation && 0 == *mdver_global_generation);

    local.transaction_dirty = true;
    will_return(mdver_enabled, true);
    mdver_bump_global_generation(&local);
    assert_true(NULL != mdver_global_generation && 1 == *mdver_global_generation);
}

int
main(int argc, char* argv[])
{
    cmockery_parse_arguments(argc, argv);
    
    const UnitTest tests[] = {
    		unit_test(test__mdver_shmem_init__NULL_memory),
			unit_test(test__mdver_shmem_init__No_NULL),
			unit_test(test__mdver_shmem_size__uint64),
			unit_test(test__mdver_bump_global_generation__inc)
    };

    return run_tests(tests);
}
