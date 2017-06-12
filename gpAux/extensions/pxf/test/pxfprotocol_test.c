#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

/* Define UNIT_TESTING so that the extension can skip declaring PG_MODULE_MAGIC */
#define UNIT_TESTING

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
test_pxfprotocol_import_first_call(void **state)
{
    /* setup call info with no call context */
    GpIdentity.segindex = 31;
    const char *EXPECTED_TUPLE = "31,hello world 31";

    PG_FUNCTION_ARGS = palloc(sizeof(FunctionCallInfoData));
    fcinfo->context = palloc(sizeof(ExtProtocolData));
    fcinfo->context->type = T_ExtProtocolData;
    EXTPROTOCOL_GET_DATALEN(fcinfo) = 100;
    EXTPROTOCOL_GET_DATABUF(fcinfo) = palloc0(EXTPROTOCOL_GET_DATALEN(fcinfo));
    ((ExtProtocolData*) fcinfo->context)->prot_last_call = false;

    Datum d = pxfprotocol_import(fcinfo);

    assert_int_equal(DatumGetInt32(d), strlen(EXTPROTOCOL_GET_DATABUF(fcinfo)));
    assert_string_equal(EXTPROTOCOL_GET_DATABUF(fcinfo), EXPECTED_TUPLE);

    context_t *call_context = EXTPROTOCOL_GET_USER_CTX(fcinfo);
    assert_true(call_context != NULL);
    assert_int_equal(call_context->row_count, 1);

    pfree(EXTPROTOCOL_GET_DATABUF(fcinfo));
    pfree(fcinfo->context);
    pfree(fcinfo);
}

void
test_pxfprotocol_import_second_call(void **state)
{
    /* setup call info with call context */
    PG_FUNCTION_ARGS = palloc(sizeof(FunctionCallInfoData));
    fcinfo->context = palloc(sizeof(ExtProtocolData));
    fcinfo->context->type = T_ExtProtocolData;
    EXTPROTOCOL_GET_DATALEN(fcinfo) = 100;
    EXTPROTOCOL_GET_DATABUF(fcinfo) = palloc0(EXTPROTOCOL_GET_DATALEN(fcinfo));
    ((ExtProtocolData*) fcinfo->context)->prot_last_call = false;

    Datum d = pxfprotocol_import(fcinfo);

    assert_int_equal(DatumGetInt32(d), 0);
    assert_int_equal(strlen(EXTPROTOCOL_GET_DATABUF(fcinfo)), 0);

    context_t *call_context = EXTPROTOCOL_GET_USER_CTX(fcinfo);
    assert_true(call_context != NULL);
    assert_int_equal(call_context->row_count, 1);

    pfree(EXTPROTOCOL_GET_DATABUF(fcinfo));
    pfree(fcinfo->context);
    pfree(fcinfo);
}

void
test_pxfprotocol_import_last_call(void **state)
{
    /* setup call info with a call context and last call indicator */
    PG_FUNCTION_ARGS = palloc(sizeof(FunctionCallInfoData));
    fcinfo->context = palloc(sizeof(ExtProtocolData));
    fcinfo->context->type = T_ExtProtocolData;
    context_t *call_context = palloc(sizeof(context_t));
    EXTPROTOCOL_SET_USER_CTX(fcinfo, call_context);
    EXTPROTOCOL_SET_LAST_CALL(fcinfo);

    Datum d = pxfprotocol_import(fcinfo);

    assert_int_equal(DatumGetInt32(d), 0);
    assert_true(EXTPROTOCOL_GET_USER_CTX(fcinfo) == NULL);

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
            unit_test(test_pxfprotocol_import_first_call),
            unit_test(test_pxfprotocol_import_second_call),
            unit_test(test_pxfprotocol_import_last_call)
    };

    MemoryContextInit();

    return run_tests(tests);
}