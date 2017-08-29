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

int
main(int argc, char* argv[])
{
    cmockery_parse_arguments(argc, argv);

    const UnitTest tests[] = {
            unit_test(test_normalize_key_name)
    };

    MemoryContextInit();

    return run_tests(tests);
}
