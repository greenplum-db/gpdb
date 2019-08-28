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
#include "nodes/makefuncs.h"
#include "utils/memutils.h"

/* Define UNIT_TESTING so that the extension can skip declaring PG_MODULE_MAGIC */
#define UNIT_TESTING

/* include unit under test */
#include "../src/pxfheaders.c"

/* include mock files */
#include "mock/libchurl_mock.c"
#include "mock/pxfutils_mock.c"
#include "mock/pxffilters_mock.c"

static GPHDUri         *gphd_uri   = NULL;
static PxfInputData    *input_data = NULL;
static struct extvar_t mock_extvar;

/* helper functions */
static void expect_headers_append(CHURL_HEADERS headers_handle, const char *header_key, const char *header_value);

static void setup_gphd_uri(void);
static void setup_input_data(void);
static void setup_external_vars(void);
static void expect_external_vars(void);

static void
common_setup(void **state)
{
	setup_gphd_uri();
	setup_input_data();
	setup_external_vars();
}

/*
 * Common resource cleanup
 */
static void
common_teardown(void **state)
{
	if (input_data->rel != NULL)
		pfree(input_data->rel);
	pfree(input_data->headers);
	pfree(input_data);
	pfree(gphd_uri);
}

static void
setup_gphd_uri()
{
	gphd_uri = palloc0(sizeof(GPHDUri));
	gphd_uri->host = "there's a place you're always welcome";
	gphd_uri->port = "it's as nice as it can be";
	gphd_uri->data = "everyone can get in";
	gphd_uri->uri = "'cos it's absolutely free";
}

static void
setup_input_data()
{
	Relation      rel      = (Relation) palloc0(sizeof(RelationData));
	CHURL_HEADERS *headers = (CHURL_HEADERS) palloc0(sizeof(CHURL_HEADERS));
	input_data = palloc0(sizeof(PxfInputData));
	input_data->gphduri   = gphd_uri;
	input_data->headers   = headers;
	input_data->proj_info = NULL;
	input_data->quals     = NIL;
	input_data->rel       = rel;
}

static void
setup_external_vars()
{
	mock_extvar.GP_USER = "pxfuser";
	snprintf(mock_extvar.GP_SEGMENT_ID, sizeof(mock_extvar.GP_SEGMENT_ID), "SegId");
	snprintf(mock_extvar.GP_SEGMENT_COUNT, sizeof(mock_extvar.GP_SEGMENT_COUNT), "10");
	snprintf(mock_extvar.GP_XID, sizeof(mock_extvar.GP_XID), "20");
}

static void
expect_external_vars()
{
	expect_any(external_set_env_vars, extvar);
	expect_string(external_set_env_vars, uri, gphd_uri->uri);
	expect_value(external_set_env_vars, csv, false);
	expect_value(external_set_env_vars, escape, NULL);
	expect_value(external_set_env_vars, quote, NULL);
	expect_value(external_set_env_vars, header, false);
	expect_value(external_set_env_vars, scancounter, 0);
	will_assign_memory(external_set_env_vars, extvar, &mock_extvar, sizeof(extvar_t));
	will_be_called(external_set_env_vars);
}

static void
test_build_http_headers(void **state)
{
	char		alignment[3];
	ExtTableEntry    ext_tbl;
	struct tupleDesc tuple;
	pg_ltoa(sizeof(char *), alignment);

	OptionData *option_data1 = (OptionData *) palloc0(sizeof(OptionData));
	option_data1->key   = "key1";
	option_data1->value = "value1";

	OptionData *option_data2 = (OptionData *) palloc0(sizeof(OptionData));
	option_data2->key   = "key2";
	option_data2->value = "value2";

	List *copyOpts = NIL;
	copyOpts = lappend(copyOpts, makeDefElem("format", (Node *)makeString("csv")));
	copyOpts = lappend(copyOpts, makeDefElem("delimiter", (Node *)makeString("|")));
	copyOpts = lappend(copyOpts, makeDefElem("null", (Node *)makeString("")));
	copyOpts = lappend(copyOpts, makeDefElem("escape", (Node *)makeString("\"")));
	copyOpts = lappend(copyOpts, makeDefElem("quote", (Node *)makeString("\"")));
	copyOpts = lappend(copyOpts, makeDefElem("encoding", (Node *)makeString("UTF8")));

	gphd_uri->options = list_make2(option_data1, option_data2);

	tuple.natts = 0;
	ext_tbl.fmtcode = 'c';
	ext_tbl.fmtopts = "delimiter '|' null '' escape '\"' quote '\"'";
	ext_tbl.encoding = 6;
	input_data->rel->rd_id = 56;
	input_data->rel->rd_att = &tuple;

	expect_external_vars();

	expect_value(GetExtTableEntry, relid, input_data->rel->rd_id);
	will_return(GetExtTableEntry, &ext_tbl);
	expect_value(parseCopyFormatString, rel, input_data->rel);
	expect_value(parseCopyFormatString, fmtstr, ext_tbl.fmtopts);
	expect_value(parseCopyFormatString, fmttype, ext_tbl.fmtcode);
	will_return(parseCopyFormatString, copyOpts);
	expect_value(appendCopyEncodingOption, copyFmtOpts, copyOpts);
	expect_value(appendCopyEncodingOption, encoding, ext_tbl.encoding);
	will_return(appendCopyEncodingOption, copyOpts);

	expect_string(normalize_key_name, key, "format");
	will_return(normalize_key_name, pstrdup("X-GP-OPTIONS-FORMAT"));
	expect_string(normalize_key_name, key, "delimiter");
	will_return(normalize_key_name, pstrdup("X-GP-OPTIONS-DELIMITER"));
	expect_string(normalize_key_name, key, "null");
	will_return(normalize_key_name, pstrdup("X-GP-OPTIONS-NULL"));
	expect_string(normalize_key_name, key, "escape");
	will_return(normalize_key_name, pstrdup("X-GP-OPTIONS-ESCAPE"));
	expect_string(normalize_key_name, key, "quote");
	will_return(normalize_key_name, pstrdup("X-GP-OPTIONS-QUOTE"));
	expect_string(normalize_key_name, key, "encoding");
	will_return(normalize_key_name, pstrdup("X-GP-OPTIONS-ENCODING"));

	expect_headers_append(input_data->headers, "X-GP-FORMAT", TextFormatName);
	expect_headers_append(input_data->headers, "X-GP-OPTIONS-FORMAT", "csv");
	expect_headers_append(input_data->headers, "X-GP-OPTIONS-DELIMITER", "|");
	expect_headers_append(input_data->headers, "X-GP-OPTIONS-NULL", "");
	expect_headers_append(input_data->headers, "X-GP-OPTIONS-ESCAPE", "\"");
	expect_headers_append(input_data->headers, "X-GP-OPTIONS-QUOTE", "\"");
	expect_headers_append(input_data->headers, "X-GP-OPTIONS-ENCODING", "UTF8");
	expect_headers_append(input_data->headers, "X-GP-ATTRS", "0");

	expect_headers_append(input_data->headers, "X-GP-USER", "pxfuser");
	expect_headers_append(input_data->headers, "X-GP-SEGMENT-ID", "SegId");
	expect_headers_append(input_data->headers, "X-GP-SEGMENT-COUNT", "10");
	expect_headers_append(input_data->headers, "X-GP-XID", "20");

	expect_headers_append(input_data->headers, "X-GP-ALIGNMENT", alignment);
	expect_headers_append(input_data->headers, "X-GP-URL-HOST", gphd_uri->host);
	expect_headers_append(input_data->headers, "X-GP-URL-PORT", gphd_uri->port);
	expect_headers_append(input_data->headers, "X-GP-DATA-DIR", gphd_uri->data);

	expect_string(normalize_key_name, key, "key1");
	will_return(normalize_key_name, pstrdup("X-GP-OPTIONS-KEY1"));
	expect_headers_append(input_data->headers, "X-GP-OPTIONS-KEY1", "value1");

	expect_string(normalize_key_name, key, "key2");
	will_return(normalize_key_name, pstrdup("X-GP-OPTIONS-KEY2"));
	expect_headers_append(input_data->headers, "X-GP-OPTIONS-KEY2", "value2");
	expect_headers_append(input_data->headers, "X-GP-URI", gphd_uri->uri);
	expect_headers_append(input_data->headers, "X-GP-HAS-FILTER", "0");

	/* call function under test */
	build_http_headers(input_data);

	/* no asserts as the function just calls to set headers */
}

static void
test_build_http_headers_no_user_error(void **state)
{
	pfree(input_data->rel);
	input_data->rel = NULL;
	mock_extvar.GP_USER = NULL;
	expect_external_vars();

	MemoryContext old_context = CurrentMemoryContext;
	PG_TRY();
	{
		build_http_headers(input_data);
		assert_false("Expected Exception");
	}
	PG_CATCH();
	{
		MemoryContextSwitchTo(old_context);
		ErrorData  *edata = CopyErrorData();

		assert_true(edata->elevel == ERROR);
		char	   *expected_message = pstrdup("user identity is unknown");

		assert_string_equal(edata->message, expected_message);
		pfree(expected_message);
	}
	PG_END_TRY();
}

static void
test_build_http_headers_empty_user_error(void **state)
{
	pfree(input_data->rel);
	input_data->rel = NULL;
	mock_extvar.GP_USER = "";
	expect_external_vars();

	MemoryContext old_context = CurrentMemoryContext;
	PG_TRY();
	{
		build_http_headers(input_data);
		assert_false("Expected Exception");
	}
	PG_CATCH();
	{
		MemoryContextSwitchTo(old_context);
		ErrorData  *edata = CopyErrorData();

		assert_true(edata->elevel == ERROR);
		char	   *expected_message = pstrdup("user identity is unknown");

		assert_string_equal(edata->message, expected_message);
		pfree(expected_message);
	}
	PG_END_TRY();
}

/**
 * Query has valid projection info,
 * but some of expression in WHERE clause is not supported
 * Make sure we are not sending any projection information at all,
 * to avoid incorrect results.
 */
static void
test__build_http_header__where_is_not_supported(void **state)
{
	char		alignment[3];
	pg_ltoa(sizeof(char *), alignment);

	input_data->proj_info = palloc0(sizeof(ProjectionInfo));
	input_data->quals = list_make1("unsupported quals");

	pfree(input_data->rel);
	input_data->rel = NULL;

	expect_external_vars();

	expect_value(extractPxfAttributes, quals, input_data->quals);
//	expect_any(extractPxfAttributes, qualsAreSupported);
	will_return(extractPxfAttributes, NULL);

	expect_headers_append(input_data->headers, "X-GP-USER", "pxfuser");
	expect_headers_append(input_data->headers, "X-GP-SEGMENT-ID", mock_extvar.GP_SEGMENT_ID);
	expect_headers_append(input_data->headers, "X-GP-SEGMENT-COUNT", mock_extvar.GP_SEGMENT_COUNT);
	expect_headers_append(input_data->headers, "X-GP-XID", mock_extvar.GP_XID);
	expect_headers_append(input_data->headers, "X-GP-ALIGNMENT", alignment);
	expect_headers_append(input_data->headers, "X-GP-URL-HOST", gphd_uri->host);
	expect_headers_append(input_data->headers, "X-GP-URL-PORT", gphd_uri->port);
	expect_headers_append(input_data->headers, "X-GP-DATA-DIR", gphd_uri->data);
	expect_headers_append(input_data->headers, "X-GP-URI", gphd_uri->uri);
	expect_headers_append(input_data->headers, "X-GP-HAS-FILTER", "0");

	build_http_headers(input_data);

	pfree(input_data->proj_info);
}

static void
test_add_tuple_desc_httpheader(void **state)
{
	/* setup mock data and expectations */
	CHURL_HEADERS headers = (CHURL_HEADERS) palloc0(sizeof(CHURL_HEADERS));
	Relation	rel = (Relation) palloc0(sizeof(RelationData));

	struct tupleDesc tuple;

	tuple.natts = 4;
	rel->rd_id = 56;
	rel->rd_att = &tuple;
	expect_headers_append(headers, "X-GP-ATTRS", "4");

	FormData_pg_attribute attrs[4];
	Form_pg_attribute attrs_ptr[4];

	tuple.attrs = attrs_ptr;

	/* define first attribute */
	attrs_ptr[0] = &attrs[0];
	char		data0[10] = "name0";

	snprintf(NameStr(attrs[0].attname), sizeof(data0), "%s", data0);
	char		typename0[12] = "NUMERICOID";

	attrs[0].atttypid = NUMERICOID;
	attrs[0].atttypmod = 10;
	char		typecode0[10];

	pg_ltoa(attrs[0].atttypid, typecode0);
	expect_value(TypeOidGetTypename, typid, NUMERICOID);
	will_return(TypeOidGetTypename, &typename0);
	expect_headers_append(headers, "X-GP-ATTR-NAME0", data0);
	expect_headers_append(headers, "X-GP-ATTR-TYPECODE0", typecode0);
	expect_headers_append(headers, "X-GP-ATTR-TYPENAME0", typename0);
	expect_headers_append(headers, "X-GP-ATTR-TYPEMOD0-COUNT", "2");
	char		typemod00[10];

	pg_ltoa((attrs[0].atttypmod >> 16) & 0xffff, typemod00);
	expect_headers_append(headers, "X-GP-ATTR-TYPEMOD0-0", typemod00);
	char		typemod01[10];

	pg_ltoa((attrs[0].atttypmod - VARHDRSZ) & 0xffff, typemod01);
	expect_headers_append(headers, "X-GP-ATTR-TYPEMOD0-1", typemod01);

	/* define second attribute */
	attrs_ptr[1] = &attrs[1];
	char		data1[10] = "name1";

	snprintf(NameStr(attrs[1].attname), sizeof(data1), "%s", data1);
	char		typename1[12] = "CHAROID";

	attrs[1].atttypid = CHAROID;
	attrs[1].atttypmod = 10;
	char		typecode1[10];

	pg_ltoa(attrs[1].atttypid, typecode1);
	expect_value(TypeOidGetTypename, typid, CHAROID);
	will_return(TypeOidGetTypename, &typename1);
	expect_headers_append(headers, "X-GP-ATTR-NAME1", data1);
	expect_headers_append(headers, "X-GP-ATTR-TYPECODE1", typecode1);
	expect_headers_append(headers, "X-GP-ATTR-TYPENAME1", typename1);
	expect_headers_append(headers, "X-GP-ATTR-TYPEMOD1-COUNT", "1");
	char		typemod10[10];

	pg_ltoa((attrs[1].atttypmod - VARHDRSZ), typemod10);
	expect_headers_append(headers, "X-GP-ATTR-TYPEMOD1-0", typemod10);

	/* define third attribute */
	attrs_ptr[2] = &attrs[2];
	char		data2[10] = "name2";

	snprintf(NameStr(attrs[2].attname), sizeof(data2), "%s", data2);
	char		typename2[12] = "TIMEOID";

	attrs[2].atttypid = TIMEOID;
	attrs[2].atttypmod = 10;
	char		typecode2[10];

	pg_ltoa(attrs[2].atttypid, typecode2);
	expect_value(TypeOidGetTypename, typid, TIMEOID);
	will_return(TypeOidGetTypename, &typename2);
	expect_headers_append(headers, "X-GP-ATTR-NAME2", data2);
	expect_headers_append(headers, "X-GP-ATTR-TYPECODE2", typecode2);
	expect_headers_append(headers, "X-GP-ATTR-TYPENAME2", typename2);
	expect_headers_append(headers, "X-GP-ATTR-TYPEMOD2-COUNT", "1");
	char		typemod20[10];

	pg_ltoa(attrs[2].atttypmod, typemod20);
	expect_headers_append(headers, "X-GP-ATTR-TYPEMOD2-0", typemod20);

	/* define fourth attribute */
	attrs_ptr[3] = &attrs[3];
	char		data3[10] = "name3";

	snprintf(NameStr(attrs[3].attname), sizeof(data3), "%s", data3);
	char		typename3[12] = "INTERVALOID";

	attrs[3].atttypid = INTERVALOID;
	attrs[3].atttypmod = 10;
	char		typecode3[10];

	pg_ltoa(attrs[3].atttypid, typecode3);
	expect_value(TypeOidGetTypename, typid, INTERVALOID);
	will_return(TypeOidGetTypename, &typename3);
	expect_headers_append(headers, "X-GP-ATTR-NAME3", data3);
	expect_headers_append(headers, "X-GP-ATTR-TYPECODE3", typecode3);
	expect_headers_append(headers, "X-GP-ATTR-TYPENAME3", typename3);
	expect_headers_append(headers, "X-GP-ATTR-TYPEMOD3-COUNT", "1");
	char		typemod30[10];

	pg_ltoa(INTERVAL_PRECISION(attrs[3].atttypmod), typemod30);
	expect_headers_append(headers, "X-GP-ATTR-TYPEMOD3-0", typemod30);

	/* call function under test */
	add_tuple_desc_httpheader(headers, rel);

	/* no asserts as the function just calls to set headers */

	/* cleanup */
	pfree(rel);
	pfree(headers);
}

static void
test_get_format_name(void **state)
{
	char	   *formatName = get_format_name('t');

	assert_string_equal(formatName, TextFormatName);

	formatName = get_format_name('c');
	assert_string_equal(formatName, TextFormatName);

	formatName = get_format_name('b');
	assert_string_equal(formatName, GpdbWritableFormatName);

	MemoryContext old_context = CurrentMemoryContext;

	PG_TRY();
	{
		formatName = get_format_name('x');
		assert_false("Expected Exception");
	}
	PG_CATCH();
	{
		ErrorData  *edata;

		MemoryContextSwitchTo(old_context);
		edata = CopyErrorData();
		FlushErrorState();
		assert_string_equal(edata->message, "unable to get format name for format code: x");
	}
	PG_END_TRY();
}

static void
expect_headers_append(CHURL_HEADERS headers_handle, const char *header_key, const char *header_value)
{
	expect_value(churl_headers_append, headers, headers_handle);
	expect_string(churl_headers_append, key, header_key);
	expect_string(churl_headers_append, value, header_value);
	will_be_called(churl_headers_append);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test(test_get_format_name),
		unit_test_setup_teardown(test_build_http_headers, common_setup, common_teardown),
		unit_test_setup_teardown(test_build_http_headers_no_user_error, common_setup, common_teardown),
		unit_test_setup_teardown(test_build_http_headers_empty_user_error, common_setup, common_teardown),
		unit_test_setup_teardown(test__build_http_header__where_is_not_supported, common_setup, common_teardown),
		unit_test(test_add_tuple_desc_httpheader)
	};

	MemoryContextInit();

	return run_tests(tests);
}
