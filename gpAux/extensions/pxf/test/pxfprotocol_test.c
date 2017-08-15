/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

/* Define UNIT_TESTING so that the extension can skip declaring PG_MODULE_MAGIC */
#define UNIT_TESTING

/* include unit under test */
#include "../src/pxfprotocol.c"

/* include mock files */
#include "mock/pxfbridge_mock.c"
#include "mock/pxfuriparser_mock.c"

const char* uri_param = "pxf://localhost:51200/tmp/dummy1";
const char* uri_param_segwork = "pxf://localhost:51200/tmp/dummy1&segwork=46@127.0.0.1@51200@tmp/dummy1.1@0@ZnJhZ21lbnQx@@@";

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
    PG_FUNCTION_ARGS = palloc(sizeof(FunctionCallInfoData));
    fcinfo->context = palloc(sizeof(ExtProtocolData));
    fcinfo->context->type = T_ExtProtocolData;
    EXTPROTOCOL_GET_DATALEN(fcinfo) = 100;
    EXTPROTOCOL_GET_DATABUF(fcinfo) = palloc0(EXTPROTOCOL_GET_DATALEN(fcinfo));
    ((ExtProtocolData*) fcinfo->context)->prot_last_call = false;
    ((ExtProtocolData*) fcinfo->context)->prot_url = uri_param;

    Relation relation = (Relation) palloc0(sizeof(Relation));
    ((ExtProtocolData*) fcinfo->context)->prot_relation = relation;

    /* set mock behavior for uri parsing */
    GPHDUri* gphd_uri = palloc0(sizeof(GPHDUri));
    expect_string(parseGPHDUri, uri_str, uri_param_segwork);
    will_return(parseGPHDUri, gphd_uri);

    /* set mock behavior for bridge import start -- nothing here */
    expect_any(gpbridge_import_start, context);
    will_be_called(gpbridge_import_start);

    /* set mock behavior for bridge read */
    const int EXPECTED_SIZE = 31; // expected number of bytes read from the bridge
    expect_any(gpbridge_read, context);
    expect_value(gpbridge_read, databuf, EXTPROTOCOL_GET_DATABUF(fcinfo));
    expect_value(gpbridge_read, datalen, EXTPROTOCOL_GET_DATALEN(fcinfo));
    will_return(gpbridge_read, EXPECTED_SIZE);

    Datum d = pxfprotocol_import(fcinfo);

    assert_int_equal(DatumGetInt32(d), EXPECTED_SIZE);     // return number of bytes read from the bridge
    gphadoop_context* context = (gphadoop_context *) EXTPROTOCOL_GET_USER_CTX(fcinfo);
    assert_true(context != NULL); // context has been created
    assert_true(context->gphd_uri != NULL); // gphduri has been parsed
    assert_true(context->uri.data != NULL); // uri has been initialized
    assert_int_equal(context->write_file_name.len, 0); // no write file name for import case
    assert_true(context->relation != NULL);
    assert_int_equal(context->relation, relation); // relation pointer is copied

    /* cleanup */
    pfree(relation);
    pfree(gphd_uri);
    pfree(EXTPROTOCOL_GET_USER_CTX(fcinfo));
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
    gphadoop_context *call_context = palloc0(sizeof(gphadoop_context));
    EXTPROTOCOL_SET_USER_CTX(fcinfo, call_context);

    /* set mock behavior for bridge read */
    const int EXPECTED_SIZE = 0; // expected number of bytes read from the bridge
    expect_value(gpbridge_read, context, call_context);
    expect_value(gpbridge_read, databuf, EXTPROTOCOL_GET_DATABUF(fcinfo));
    expect_value(gpbridge_read, datalen, EXTPROTOCOL_GET_DATALEN(fcinfo));
    will_return(gpbridge_read, EXPECTED_SIZE);

    Datum d = pxfprotocol_import(fcinfo);

    assert_int_equal(DatumGetInt32(d), EXPECTED_SIZE);             // return number of bytes read from the bridge
    assert_true(EXTPROTOCOL_GET_USER_CTX(fcinfo) == call_context); // context is still the same

    /* cleanup */
    pfree(call_context);
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
    gphadoop_context *call_context = palloc(sizeof(gphadoop_context));
    EXTPROTOCOL_SET_USER_CTX(fcinfo, call_context);
    EXTPROTOCOL_SET_LAST_CALL(fcinfo);

    /* init data in context that will be cleaned up */
    initStringInfo(&call_context->uri);
    initStringInfo(&call_context->write_file_name);

    /* set mock behavior for bridge cleanup */
    expect_value(gpbridge_cleanup, context, call_context);
    will_be_called(gpbridge_cleanup);

    Datum d = pxfprotocol_import(fcinfo);

    assert_int_equal(DatumGetInt32(d), 0);                 // 0 is returned from function
    assert_true(EXTPROTOCOL_GET_USER_CTX(fcinfo) == NULL); // call context is cleaned up

    /* cleanup */
    pfree(fcinfo->context);
    pfree(fcinfo);
}

/* test setup and teardown methods */
void
before_test(void)
{
    // set global variables
    GpIdentity.segindex = 0;
}

void
after_test(void)
{
    // no-op, but the teardown seems to be required when the test fails, otherwise CMockery issues a mismatch error
}

int
main(int argc, char* argv[])
{
    cmockery_parse_arguments(argc, argv);

    const UnitTest tests[] = {
            unit_test(test_pxfprotocol_validate_urls),
            unit_test(test_pxfprotocol_export),
            unit_test_setup_teardown(test_pxfprotocol_import_first_call, before_test, after_test),
            unit_test_setup_teardown(test_pxfprotocol_import_second_call, before_test, after_test),
            unit_test_setup_teardown(test_pxfprotocol_import_last_call, before_test, after_test)
    };

    MemoryContextInit();

    return run_tests(tests);
}