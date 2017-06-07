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
#include "../src/pxfutils.c"

/* include mock files */
#include "mock/libchurl_mock.c"

void
test_are_ips_equal(void **state)
{
    assert_false(are_ips_equal(NULL, "ip"));
    assert_false(are_ips_equal("ip", NULL));
    assert_false(are_ips_equal("ip", "pi"));
    assert_false(are_ips_equal("IP", "ip"));
    assert_true(are_ips_equal("ip", "ip"));
}

void
test_port_to_str(void **state)
{

    char* port = pstrdup("123");
    port_to_str(&port, 65535);
    assert_string_equal(port, "65535");

    /* test null original port */
    MemoryContext old_context = CurrentMemoryContext;
    PG_TRY();
    {
        port = pstrdup("123");
        port_to_str(NULL, 65535);
        assert_false("Expected Exception");
    }
    PG_CATCH();
    {
        MemoryContextSwitchTo(old_context);
        ErrorData *edata = CopyErrorData();
        assert_true(edata->elevel == ERROR);
        char* expected_message = pstrdup("unexpected internal error in pxfutils.c");
        assert_string_equal(edata->message, expected_message);
        pfree(expected_message);
        pfree(port);
    }
    PG_END_TRY();
}

void
test_normalize_key_name(void **state)
{
    char* replaced = normalize_key_name("bar");
    assert_string_equal(replaced, "X-GP-BAR");
    pfree(replaced);

    replaced = normalize_key_name("FOO");
    assert_string_equal(replaced, "X-GP-FOO");
    pfree(replaced);

    /* test null string */
    MemoryContext old_context = CurrentMemoryContext;
    PG_TRY();
    {
        replaced = normalize_key_name(NULL);
        assert_false("Expected Exception");
    }
    PG_CATCH();
    {
        MemoryContextSwitchTo(old_context);
        ErrorData *edata = CopyErrorData();
        assert_true(edata->elevel == ERROR);
        char* expected_message = pstrdup("internal error in pxfutils.c:normalize_key_name. Parameter key is null or empty.");
        assert_string_equal(edata->message, expected_message);
        pfree(expected_message);
    }
    PG_END_TRY();

    /* test empty string */
    PG_TRY();
    {
        replaced = normalize_key_name("");
        assert_false("Expected Exception");
    }
    PG_CATCH();
    {
        MemoryContextSwitchTo(old_context);
        ErrorData *edata = CopyErrorData();
        assert_true(edata->elevel == ERROR);
        char* expected_message = pstrdup("internal error in pxfutils.c:normalize_key_name. Parameter key is null or empty.");
        assert_string_equal(edata->message, expected_message);
        pfree(expected_message);
    }
    PG_END_TRY();

}

void
test_call_rest(void **state)
{
    GPHDUri *hadoop_uri = (GPHDUri*) palloc0(sizeof(GPHDUri));
    hadoop_uri->host = "host";
    hadoop_uri->port = "123";


    ClientContext *client_context = (ClientContext*) palloc0(sizeof(ClientContext));
    client_context->http_headers = (CHURL_HEADERS) palloc0(sizeof(CHURL_HEADERS));
    initStringInfo(&(client_context->the_rest_buf));

    CHURL_HANDLE handle = (CHURL_HANDLE) palloc0(sizeof(CHURL_HANDLE));

    char *rest_msg = "http://%s:%s/%s/%s";

    expect_value(print_http_headers, headers, client_context->http_headers);
    will_be_called(print_http_headers);(client_context->http_headers);


    StringInfoData expected_url;
    initStringInfo(&expected_url);
    appendStringInfo(&expected_url, "http://host:123/%s/%s", PXF_SERVICE_PREFIX, PXF_VERSION);

    expect_string(churl_init_download, url, expected_url.data);
    expect_value(churl_init_download, headers, client_context->http_headers);
    will_return(churl_init_download, handle);

    expect_value(churl_read_check_connectivity, handle, handle);
    will_be_called(churl_read_check_connectivity);

    /* first call to read should return Hello */
    expect_value(churl_read, handle, handle);
    expect_any(churl_read, buf);
    expect_any(churl_read, max_size);

    char* str = pstrdup("Hello ");

    //will_assign_memory(churl_read, buf, str, 7);
    will_return(churl_read, 7);

    /* second call to read should return World */
    expect_value(churl_read, handle, handle);
    expect_any(churl_read, buf);
    expect_any(churl_read, max_size);
    //will_assign_memory(churl_read, buf, "World", 6);
    will_return(churl_read, 6);

    /* third call will return nothing */
    expect_value(churl_read, handle, handle);
    expect_any(churl_read, buf);
    expect_any(churl_read, max_size);
    will_return(churl_read, 0);

    expect_value(churl_cleanup, handle, handle);
    expect_value(churl_cleanup, after_error, false);
    will_be_called(churl_cleanup);

    call_rest(hadoop_uri, client_context, rest_msg);

    //TODO: debug this, seems will_assign_memory is not quite working
    //assert_string_equal(client_context->the_rest_buf.data, "Hello World");

    pfree(expected_url.data);
    pfree(client_context->http_headers);
    pfree(client_context);
    pfree(hadoop_uri);
}

int
main(int argc, char* argv[])
{
    cmockery_parse_arguments(argc, argv);

    const UnitTest tests[] = {
            unit_test(test_are_ips_equal),
            unit_test(test_port_to_str),
            unit_test(test_normalize_key_name),
            unit_test(test_call_rest)
    };

    MemoryContextInit();

    return run_tests(tests);
}
