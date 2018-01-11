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
#include "../src/pxfheaders.c"

/* include mock files */
#include "mock/libchurl_mock.c"
#include "mock/pxfutils_mock.c"

/* helper functions */
static void expect_headers_append(CHURL_HEADERS headers_handle, const char *header_key, const char *header_value);

void
test_build_http_headers(void **state)
{

	/* setup mock data and expectations */
	PxfInputData *input = (PxfInputData *) palloc0(sizeof(PxfInputData));
	CHURL_HEADERS headers = (CHURL_HEADERS) palloc0(sizeof(CHURL_HEADERS));
	GPHDUri    *gphd_uri = (GPHDUri *) palloc0(sizeof(GPHDUri));
	Relation	rel = (Relation) palloc0(sizeof(RelationData));

	ExtTableEntry ext_tbl;
	struct tupleDesc tuple;

	input->headers = headers;
	input->gphduri = gphd_uri;
	input->rel = rel;

	gphd_uri->host = "testhost";
	gphd_uri->port = "101";
	gphd_uri->data = "this is test data";
	gphd_uri->uri = "testuri";

	OptionData *option_data1 = (OptionData *) palloc0(sizeof(OptionData));

	option_data1->key = "option1-key";
	option_data1->value = "option1-value";
	OptionData *option_data2 = (OptionData *) palloc0(sizeof(OptionData));

	option_data2->key = "option2-key";
	option_data2->value = "option2-value";
	gphd_uri->options = list_make2(option_data1, option_data2);

	tuple.natts = 0;
	ext_tbl.fmtcode = 'c';
	rel->rd_id = 56;
	rel->rd_att = &tuple;

	expect_value(GetExtTableEntry, relid, rel->rd_id);
	will_return(GetExtTableEntry, &ext_tbl);
	expect_headers_append(headers, "X-GP-FORMAT", TextFormatName);
	expect_headers_append(headers, "X-GP-ATTRS", "0");

	expect_any(external_set_env_vars, extvar);
	expect_string(external_set_env_vars, uri, gphd_uri->uri);
	expect_value(external_set_env_vars, csv, false);
	expect_value(external_set_env_vars, escape, NULL);
	expect_value(external_set_env_vars, quote, NULL);
	expect_value(external_set_env_vars, header, false);
	expect_value(external_set_env_vars, scancounter, 0);

	struct extvar_t mock_extvar;
	mock_extvar.GP_USER = "user";
	snprintf(mock_extvar.GP_SEGMENT_ID, sizeof(mock_extvar.GP_SEGMENT_ID), "SegId");
	snprintf(mock_extvar.GP_SEGMENT_COUNT, sizeof(mock_extvar.GP_SEGMENT_COUNT), "10");
	snprintf(mock_extvar.GP_XID, sizeof(mock_extvar.GP_XID), "20");
	will_assign_memory(external_set_env_vars, extvar, &mock_extvar, sizeof(extvar_t));
	will_be_called(external_set_env_vars);

	expect_headers_append(headers, "X-GP-USER", "user");
	expect_headers_append(headers, "X-GP-SEGMENT-ID", "SegId");
	expect_headers_append(headers, "X-GP-SEGMENT-COUNT", "10");
	expect_headers_append(headers, "X-GP-XID", "20");

	char		alignment[3];

	pg_ltoa(sizeof(char *), alignment);
	expect_headers_append(headers, "X-GP-ALIGNMENT", alignment);
	expect_headers_append(headers, "X-GP-URL-HOST", gphd_uri->host);
	expect_headers_append(headers, "X-GP-URL-PORT", gphd_uri->port);
	expect_headers_append(headers, "X-GP-DATA-DIR", gphd_uri->data);


	expect_string(normalize_key_name, key, "option1-key");
	will_return(normalize_key_name, pstrdup("X-GP-OPTION1-KEY"));
	expect_headers_append(headers, "X-GP-OPTION1-KEY", "option1-value");

	expect_string(normalize_key_name, key, "option2-key");
	will_return(normalize_key_name, pstrdup("X-GP-OPTION2-KEY"));
	expect_headers_append(headers, "X-GP-OPTION2-KEY", "option2-value");

	expect_headers_append(headers, "X-GP-URI", gphd_uri->uri);
	expect_headers_append(headers, "X-GP-HAS-FILTER", "0");

	/* call function under test */
	build_http_headers(input);

	/* no asserts as the function just calls to set headers */

	/* cleanup */
	pfree(rel);
	pfree(gphd_uri);
	pfree(headers);
	pfree(input);
}

void
test_build_http_headers_no_user_error(void **state) {

	/* setup mock data and expectations */
	PxfInputData *input = (PxfInputData *) palloc0(sizeof(PxfInputData));
	CHURL_HEADERS headers = (CHURL_HEADERS) palloc0(sizeof(CHURL_HEADERS));
	GPHDUri *gphd_uri = (GPHDUri *) palloc0(sizeof(GPHDUri));
	Relation rel = (Relation) palloc0(sizeof(RelationData));

	ExtTableEntry ext_tbl;
	struct tupleDesc tuple;

	input->headers = headers;
	input->gphduri = gphd_uri;
	input->rel = NULL;

	gphd_uri->uri = "testuri";

	expect_any(external_set_env_vars, extvar);
	expect_string(external_set_env_vars, uri, gphd_uri->uri);
	expect_value(external_set_env_vars, csv, false);
	expect_value(external_set_env_vars, escape, NULL);
	expect_value(external_set_env_vars, quote, NULL);
	expect_value(external_set_env_vars, header, false);
	expect_value(external_set_env_vars, scancounter, 0);

	struct extvar_t mock_extvar;
	mock_extvar.GP_USER = NULL;
	snprintf(mock_extvar.GP_SEGMENT_ID, sizeof(mock_extvar.GP_SEGMENT_ID), "SegId");
	snprintf(mock_extvar.GP_SEGMENT_COUNT, sizeof(mock_extvar.GP_SEGMENT_COUNT), "10");
	snprintf(mock_extvar.GP_XID, sizeof(mock_extvar.GP_XID), "20");
	will_assign_memory(external_set_env_vars, extvar, &mock_extvar, sizeof(extvar_t));
	will_be_called(external_set_env_vars);

	MemoryContext old_context = CurrentMemoryContext;
	PG_TRY();
	{
		build_http_headers(input);
		assert_false("Expected Exception");
	}
	PG_CATCH();
	{
		MemoryContextSwitchTo(old_context);
		ErrorData  *edata = CopyErrorData();

		assert_true(edata->elevel == ERROR);
		char	   *expected_message = pstrdup("User identity is unknown");

		assert_string_equal(edata->message, expected_message);
		pfree(expected_message);
	}
	PG_END_TRY();
}

void
test_build_http_headers_empty_user_error(void **state) {

	/* setup mock data and expectations */
	PxfInputData *input = (PxfInputData *) palloc0(sizeof(PxfInputData));
	CHURL_HEADERS headers = (CHURL_HEADERS) palloc0(sizeof(CHURL_HEADERS));
	GPHDUri *gphd_uri = (GPHDUri *) palloc0(sizeof(GPHDUri));
	Relation rel = (Relation) palloc0(sizeof(RelationData));

	ExtTableEntry ext_tbl;
	struct tupleDesc tuple;

	input->headers = headers;
	input->gphduri = gphd_uri;
	input->rel = NULL;

	gphd_uri->uri = "testuri";

	expect_any(external_set_env_vars, extvar);
	expect_string(external_set_env_vars, uri, gphd_uri->uri);
	expect_value(external_set_env_vars, csv, false);
	expect_value(external_set_env_vars, escape, NULL);
	expect_value(external_set_env_vars, quote, NULL);
	expect_value(external_set_env_vars, header, false);
	expect_value(external_set_env_vars, scancounter, 0);

	struct extvar_t mock_extvar;
	mock_extvar.GP_USER = "";
	snprintf(mock_extvar.GP_SEGMENT_ID, sizeof(mock_extvar.GP_SEGMENT_ID), "SegId");
	snprintf(mock_extvar.GP_SEGMENT_COUNT, sizeof(mock_extvar.GP_SEGMENT_COUNT), "10");
	snprintf(mock_extvar.GP_XID, sizeof(mock_extvar.GP_XID), "20");
	will_assign_memory(external_set_env_vars, extvar, &mock_extvar, sizeof(extvar_t));
	will_be_called(external_set_env_vars);

	MemoryContext old_context = CurrentMemoryContext;
	PG_TRY();
	{
		build_http_headers(input);
		assert_false("Expected Exception");
	}
	PG_CATCH();
	{
		MemoryContextSwitchTo(old_context);
		ErrorData  *edata = CopyErrorData();

		assert_true(edata->elevel == ERROR);
		char	   *expected_message = pstrdup("User identity is unknown");

		assert_string_equal(edata->message, expected_message);
		pfree(expected_message);
	}
	PG_END_TRY();
}

void
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

	snprintf(NameStr(attrs[0].attname), sizeof(data0), data0);
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

	snprintf(NameStr(attrs[1].attname), sizeof(data1), data1);
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

	snprintf(NameStr(attrs[2].attname), sizeof(data2), data2);
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

	snprintf(NameStr(attrs[3].attname), sizeof(data3), data3);
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

void
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
		assert_string_equal(edata->message, "Unable to get format name for format code: x");
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
		unit_test(test_build_http_headers),
		unit_test(test_build_http_headers_no_user_error),
		unit_test(test_build_http_headers_empty_user_error),
		unit_test(test_add_tuple_desc_httpheader)
	};

	MemoryContextInit();

	return run_tests(tests);
}
