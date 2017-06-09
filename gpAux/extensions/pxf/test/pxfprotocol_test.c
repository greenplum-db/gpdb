#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

/* Define TEST_CASE so that the extension can skip declaring PG_MODULE_MAGIC */
#define TEST_CASE

/* include unit under test */
#include "../src/pxfprotocol.c"

void
test_pxfprotocol_validate_urls(void **state)
{
    Datum d = pxfprotocol_validate_urls(NULL);
    assert_int_equal(DatumGetInt32(d), 0);
}

void
test_pxfprotocol_export(void **state)
{
    Datum d = pxfprotocol_export(NULL);
    assert_int_equal(DatumGetInt32(d), 0);
}

void
test_pxfprotocol_import_last_call(void **state)
{
    PG_FUNCTION_ARGS = palloc(sizeof(FunctionCallInfoData));
    fcinfo->context = palloc(sizeof(ExtProtocolData));
    fcinfo->context->type = T_ExtProtocolData;
    EXTPROTOCOL_SET_USER_CTX(fcinfo, NULL);
    EXTPROTOCOL_SET_LAST_CALL(fcinfo);

    Datum d = pxfprotocol_import(fcinfo);

    assert_int_equal(DatumGetInt32(d), 0);

    // TODO: check EXTPROTOCOL_GET_USER_CTX is set to null

    pfree(fcinfo->context);
    pfree(fcinfo);
}

int
main(int argc, char* argv[])
{
    cmockery_parse_arguments(argc, argv);

    const UnitTest tests[] = {
            unit_test(test_pxfprotocol_validate_urls),
            unit_test(test_pxfprotocol_export),
            unit_test(test_pxfprotocol_import_last_call)
    };

    MemoryContextInit();

    return run_tests(tests);
}