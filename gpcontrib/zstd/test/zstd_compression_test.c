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

#define UNIT_TESTING

/* Include unit under test */
#include "../zstd_compression.c"

#include "mock/zstd_mock.c"


/*
 * Test compression failure when compressed data is larger than uncompressed
 */
void
test_uncompressed_sz_equals_compressed_sz(void **state)
{
    int32 uncompressed_size = 42;
    int32 compressed_size = 0;

    DirectFunctionCall6(
        zstd_compress,
        PointerGetDatum(NULL),
        Int32GetDatum(uncompressed_size),
        PointerGetDatum(NULL),
        Int32GetDatum(uncompressed_size),
        PointerGetDatum(&compressed_size),
        PointerGetDatum(NULL)
    );

    assert_int_equal(compressed_size, uncompressed_size);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_uncompressed_sz_equals_compressed_sz),
	};

	MemoryContextInit();

	return run_tests(tests);
}
