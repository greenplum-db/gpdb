#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include "cmockery.h"

#include "postgres.h"

#include "miscadmin.h"
#include "storage/pg_shmem.h"
#include "storage/shmem.h"
#include "utils/memutils.h"
/*
 * This include is used to share the test between the real implementation
 * and the fake implementation tests
 */
#include "shmem_shared_tests.c"

int main(int argc, char *argv[]) {
    cmockery_parse_arguments(argc, argv);

    const UnitTest tests[] = {
            unit_test_with_prefix(fake_, test__ShmemAlloc_can_allocate_memory),
            unit_test_with_prefix(fake_, test__add_size__when_two_positive_sizes_are_passed_it_returns_the_sum_of_the_sizes),
            unit_test_with_prefix(fake_, test__mul_size__when_first_argument_is_zero_it_returns_zero),
            unit_test_with_prefix(fake_, test__mul_size__when_second_argument_is_zero_it_returns_zero),
            unit_test_with_prefix(fake_, test__mul_size__when_both_arguments_are_zero_it_returns_zero),
            unit_test_with_prefix(fake_, test__mul_size__when_both_arguments_are_not_zero_it_returns_the_mutiplication_of_sizes),
    };

    return run_tests(tests);
}
