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

#include "postgres.h"
#include "utils/memutils.h"

/* Define UNIT_TESTING so that the extension can skip declaring PG_MODULE_MAGIC */
#define UNIT_TESTING

/* include unit under test */
#include "../src/pxfutils.c"
#include "../src/pxfuriparser.c"

#include "mock/pxffragment_mock.c"

static void test_parseGPHDUri_helper(const char *uri, const char *message);

static char uri[] = "pxf://some/path/and/table.tbl?FRAGMENTER=SomeFragmenter&ACCESSOR=SomeAccessor&RESOLVER=SomeResolver&ANALYZER=SomeAnalyzer";

/*
 * Test parsing of valid uri as given in LOCATION in a PXF external table.
 */
static void
test_parseGPHDUri_ValidURI(void **state)
{
	GPHDUri    *parsed = parseGPHDUri(uri);

	assert_true(parsed != NULL);
	assert_string_equal(parsed->uri, uri);

	assert_string_equal(parsed->protocol, "pxf");
	assert_string_equal(parsed->host, PXF_DEFAULT_HOST);
	assert_string_equal(parsed->port, psprintf("%d", PXF_DEFAULT_PORT));
	assert_string_equal(parsed->data, "some/path/and/table.tbl");

	List	   *options = parsed->options;

	assert_int_equal(list_length(options), 4);

	ListCell   *cell = list_nth_cell(options, 0);
	OptionData *option = lfirst(cell);

	assert_string_equal(option->key, FRAGMENTER);
	assert_string_equal(option->value, "SomeFragmenter");

	cell = list_nth_cell(options, 1);
	option = lfirst(cell);
	assert_string_equal(option->key, ACCESSOR);
	assert_string_equal(option->value, "SomeAccessor");

	cell = list_nth_cell(options, 2);
	option = lfirst(cell);
	assert_string_equal(option->key, RESOLVER);
	assert_string_equal(option->value, "SomeResolver");

	cell = list_nth_cell(options, 3);
	option = lfirst(cell);
	assert_string_equal(option->key, ANALYZER);
	assert_string_equal(option->value, "SomeAnalyzer");

	assert_true(parsed->profile == NULL);

	freeGPHDUri(parsed);
}

/*
 * Negative test: parsing of uri without protocol delimiter "://"
 */
static void
test_parseGPHDUri_NegativeTestNoProtocol(void **state)
{
	char	   *uri = "pxf:/some/path/and/table.tbl?FRAGMENTER=HdfsDataFragmenter";

	test_parseGPHDUri_helper(uri, "");
}

/*
 * Negative test: parsing of uri without options part
 */
static void
test_parseGPHDUri_NegativeTestNoOptions(void **state)
{
	char	   *uri = "pxf://some/path/and/table.tbl";

	test_parseGPHDUri_helper(uri, ": missing options section");
}

/*
 * Negative test: parsing of a uri with a missing equal
 */
static void
test_parseGPHDUri_NegativeTestMissingEqual(void **state)
{
	char	   *uri = "pxf://some/path/and/table.tbl?FRAGMENTER";

	test_parseGPHDUri_helper(uri, ": option 'FRAGMENTER' missing '='");
}

/*
 * Negative test: parsing of a uri with a missing key
 */
static void
test_parseGPHDUri_NegativeTestMissingKey(void **state)
{
	char	   *uri = "pxf://some/path/and/table.tbl?=HdfsDataFragmenter";

	test_parseGPHDUri_helper(uri, ": option '=HdfsDataFragmenter' missing key before '='");
}

/*
 * Negative test: parsing of a uri with a missing value
 */
static void
test_parseGPHDUri_NegativeTestMissingValue(void **state)
{
	char	   *uri = "pxf://some/path/and/table.tbl?FRAGMENTER=";

	test_parseGPHDUri_helper(uri, ": option 'FRAGMENTER=' missing value after '='");
}

/*
 * Test GPHDUri_opt_exists to check if a specified option is in the URI
 */
static void
test_GPHDUri_opt_exists(void **state)
{
	char	   *uri_str = "xyz?FRAGMENTER=HdfsDataFragmenter&RESOLVER=SomeResolver";
	char	   *cursor = strstr(uri_str, "?");
	GPHDUri    *uri = (GPHDUri *) palloc0(sizeof(GPHDUri));

	GPHDUri_parse_options(uri, &cursor);

	bool		exists = GPHDUri_opt_exists(uri, "FRAGMENTER");

	assert_true(exists);
	exists = GPHDUri_opt_exists(uri, "RESOLVER");
	assert_true(exists);
	exists = GPHDUri_opt_exists(uri, "ACCESSOR");
	assert_false(exists);

	pfree(uri);
}

/*
 * Test GPHDUri_verify_no_duplicate_options to check that there are no duplicate options
 */
static void
test_GPHDUri_verify_no_duplicate_options(void **state)
{
	/* No duplicates */
	char	   *uri_no_dup_opts = "xyz?FRAGMENTER=HdfsDataFragmenter&RESOLVER=SomeResolver";
	char	   *cursor = strstr(uri_no_dup_opts, "?");
	GPHDUri    *uri = (GPHDUri *) palloc0(sizeof(GPHDUri));

	GPHDUri_parse_options(uri, &cursor);
	GPHDUri_verify_no_duplicate_options(uri);
	pfree(uri);

	/* Expect error if duplicate options specified */
	char	   *uri_dup_opts = "xyz?FRAGMENTER=HdfsDataFragmenter&FRAGMENTER=SomeFragmenter";

	cursor = strstr(uri_dup_opts, "?");
	uri = (GPHDUri *) palloc0(sizeof(GPHDUri));
	GPHDUri_parse_options(uri, &cursor);

	MemoryContext old_context = CurrentMemoryContext;

	PG_TRY();
	{
		GPHDUri_verify_no_duplicate_options(uri);
		assert_false("Expected Exception");
	}
	PG_CATCH();
	{
		MemoryContextSwitchTo(old_context);
		ErrorData  *edata = CopyErrorData();

		FlushErrorState();

		/* Validate the type of expected error */
		assert_true(edata->sqlerrcode == ERRCODE_SYNTAX_ERROR);
		assert_true(edata->elevel == ERROR);
		StringInfoData expected_message;

		initStringInfo(&expected_message);
		appendStringInfo(&expected_message, "invalid URI %s: Duplicate option(s): %s", uri->uri, "FRAGMENTER");

		assert_string_equal(edata->message, expected_message.data);
		pfree(expected_message.data);
		elog_dismiss(INFO);
	}
	PG_END_TRY();

	pfree(uri);
}

/*
 * Test GPHDUri_verify_core_options_exist to check that all options in the expected list are present
 */
static void
test_GPHDUri_verify_core_options_exist(void **state)
{
	List	   *coreOptions = list_make3("FRAGMENTER", "ACCESSOR", "RESOLVER");

	/* Check for presence of options in the above list */
	char	   *uri_core_opts = "xyz?FRAGMENTER=HdfsDataFragmenter&ACCESSOR=SomeAccesor&RESOLVER=SomeResolver";
	char	   *cursor = strstr(uri_core_opts, "?");
	GPHDUri    *uri = (GPHDUri *) palloc0(sizeof(GPHDUri));

	GPHDUri_parse_options(uri, &cursor);
	GPHDUri_verify_core_options_exist(uri, coreOptions);
	pfree(uri);

	/* Option RESOLVER is missing. Expect validation error */
	char	   *uri_miss_core_opts = "xyz?FRAGMENTER=HdfsDataFragmenter&ACCESSOR=SomeAccesor";

	cursor = strstr(uri_miss_core_opts, "?");
	uri = (GPHDUri *) palloc0(sizeof(GPHDUri));
	GPHDUri_parse_options(uri, &cursor);

	MemoryContext old_context = CurrentMemoryContext;

	PG_TRY();
	{
		GPHDUri_verify_core_options_exist(uri, coreOptions);
		assert_false("Expected Exception");
	}
	PG_CATCH();
	{
		MemoryContextSwitchTo(old_context);
		ErrorData  *edata = CopyErrorData();

		FlushErrorState();

		/* Validate the type of expected error */
		assert_true(edata->sqlerrcode == ERRCODE_SYNTAX_ERROR);
		assert_true(edata->elevel == ERROR);
		StringInfoData expected_message;

		initStringInfo(&expected_message);
		appendStringInfo(&expected_message, "invalid URI %s: %s option(s) missing", uri->uri, "RESOLVER");

		assert_string_equal(edata->message, expected_message.data);
		pfree(expected_message.data);
		elog_dismiss(INFO);
	}
	PG_END_TRY();

	pfree(uri);
}

/*
 * Helper function for parse uri test cases
 */
static void
test_parseGPHDUri_helper(const char *uri, const char *message)
{
	/* Setting the test -- code omitted -- */
	MemoryContext old_context = CurrentMemoryContext;

	PG_TRY();
	{
		/* This will throw a ereport(ERROR). */
		parseGPHDUri(uri);

		assert_false("Expected Exception");
	}
	PG_CATCH();
	{
		MemoryContextSwitchTo(old_context);
		ErrorData  *edata = CopyErrorData();

		FlushErrorState();

		/* Validate the type of expected error */
		assert_true(edata->sqlerrcode == ERRCODE_SYNTAX_ERROR);
		assert_true(edata->elevel == ERROR);
		StringInfoData expected_message;

		initStringInfo(&expected_message);
		appendStringInfo(&expected_message, "invalid URI %s%s", uri, message);

		assert_string_equal(edata->message, expected_message.data);
		pfree(expected_message.data);
		elog_dismiss(INFO);
	}
	PG_END_TRY();
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test(test_parseGPHDUri_ValidURI),
		unit_test(test_parseGPHDUri_NegativeTestNoProtocol),
		unit_test(test_parseGPHDUri_NegativeTestNoOptions),
		unit_test(test_parseGPHDUri_NegativeTestMissingEqual),
		unit_test(test_parseGPHDUri_NegativeTestMissingKey),
		unit_test(test_parseGPHDUri_NegativeTestMissingValue),
		unit_test(test_GPHDUri_opt_exists),
		unit_test(test_GPHDUri_verify_no_duplicate_options),
		unit_test(test_GPHDUri_verify_core_options_exist),
	};

	MemoryContextInit();

	return run_tests(tests);
}
