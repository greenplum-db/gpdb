#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

//#include "../src/pxfprotocol.c"

void
test_pxfprotocol_export(void **state)
{
    //assert_int_equal(pxfprotocol_export(), 0);
}

int
main(int argc, char* argv[])
{
    cmockery_parse_arguments(argc, argv);

    const UnitTest tests[] = {
            unit_test(test_pxfprotocol_export)
    };

    MemoryContextInit();

    return run_tests(tests);
}