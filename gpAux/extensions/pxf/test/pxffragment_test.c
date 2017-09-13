#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

/* Define UNIT_TESTING so that the extension can skip declaring PG_MODULE_MAGIC */
#define UNIT_TESTING

/* include unit under test */
#include "../src/pxffragment.c"

/* include mock files */
#include "mock/libchurl_mock.c"
#include "mock/pxfheaders_mock.c"
#include "mock/pxfuriparser_mock.c"
#include "mock/pxfutils_mock.c"
#include "../../../../src/include/nodes/pg_list.h"

#define ARRSIZE(x) (sizeof(x) / sizeof((x)[0]))

/* helper functions */
static List *prepare_fragment_list(int fragtotal, int sefgindex, int segtotal, int xid);
static void test_list(int segindex, int segtotal, int xid, int fragtotal, char *expected[], int expected_total);
static FragmentData *buildFragment(const char *index, const char *source, const char *userdata, const char *metadata, const char *profile);
static bool compareLists(List *list1, List *list2, bool (*compareType) (void *, void *));
static bool compareString(char *str1, char *str2);
static bool compareFragment(ListCell *fragment_cell1, ListCell *fragment_cell2);


void
test_filter_fragments_for_segment(void **state)
{
	/* --- 1 segment, all fragements should be processed by it */
	char	   *expected_1_1_0[1] = {"0"};

	/* 1 fragment */
	test_list(0, 1, 1, 1, expected_1_1_0, ARRSIZE(expected_1_1_0));
	/* xid = 1 */
	test_list(0, 1, 2, 1, expected_1_1_0, ARRSIZE(expected_1_1_0));
	/* xid = 2 */

	char	   *expected_1_2_0[2] = {"0", "1"};

	/* 2 fragments */

	test_list(0, 1, 1, 2, expected_1_2_0, ARRSIZE(expected_1_2_0));
	/* xid = 1 */
	test_list(0, 1, 2, 2, expected_1_2_0, ARRSIZE(expected_1_2_0));
	/* xid = 2 */

	char	   *expected_1_3_0[3] = {"0", "1", "2"};

	/* 3 fragments */
	test_list(0, 1, 1, 3, expected_1_3_0, ARRSIZE(expected_1_3_0));
	/* xid = 1 */
	test_list(0, 1, 2, 3, expected_1_3_0, ARRSIZE(expected_1_3_0));
	/* xid = 2 */

	/* --- 2 segments, each processes every other fragment, based on xid */
	/* 1 fragment, xid=1 */
	test_list(0, 2, 1, 1, NULL, 0);
	/* seg = 0 */
	char	   *expected_1_2_1_1[1] = {"0"};

	/* seg = 1 */
	test_list(1, 2, 1, 1, expected_1_2_1_1, ARRSIZE(expected_1_2_1_1));

	/* 1 fragment, xid=2 */
	char	   *expected_0_2_2_1[1] = {"0"};

	/* seg = 0 */
	test_list(0, 2, 2, 1, expected_0_2_2_1, ARRSIZE(expected_0_2_2_1));
	test_list(1, 2, 2, 1, NULL, 0);
	/* seg = 1 */

	/* 1 fragment, xid=3 */
	test_list(0, 2, 3, 1, NULL, 0);
	/* seg = 0 */
	char	   *expected_1_2_3_1[1] = {"0"};

	/* seg = 1 */
	test_list(1, 2, 3, 1, expected_1_2_3_1, ARRSIZE(expected_1_2_3_1));

	/* 2 fragments, xid=1 */
	char	   *expected_0_2_1_2[1] = {"1"};

	/* seg = 0 */
	test_list(0, 2, 1, 2, expected_0_2_1_2, ARRSIZE(expected_0_2_1_2));
	char	   *expected_1_2_1_2[1] = {"0"};

	/* seg = 1 */
	test_list(1, 2, 1, 2, expected_1_2_1_2, ARRSIZE(expected_1_2_1_2));

	/* 2 fragments, xid=2 */
	char	   *expected_0_2_2_2[1] = {"0"};

	/* seg = 0 */
	test_list(0, 2, 2, 2, expected_0_2_2_2, ARRSIZE(expected_0_2_2_2));
	char	   *expected_1_2_2_2[1] = {"1"};

	/* seg = 1 */
	test_list(1, 2, 2, 2, expected_1_2_2_2, ARRSIZE(expected_1_2_2_2));

	/* 2 fragments, xid=3 */
	char	   *expected_0_2_3_2[1] = {"1"};

	/* seg = 0 */
	test_list(0, 2, 3, 2, expected_0_2_3_2, ARRSIZE(expected_0_2_3_2));
	char	   *expected_1_2_3_2[1] = {"0"};

	/* seg = 1 */
	test_list(1, 2, 3, 2, expected_1_2_3_2, ARRSIZE(expected_1_2_3_2));

	/* 3 fragments, xid=1 */
	char	   *expected_0_2_1_3[1] = {"1"};

	/* seg = 0 */
	test_list(0, 2, 1, 3, expected_0_2_1_3, ARRSIZE(expected_0_2_1_3));
	char	   *expected_1_2_1_3[2] = {"0", "2"};

	/* seg = 1 */
	test_list(1, 2, 1, 3, expected_1_2_1_3, ARRSIZE(expected_1_2_1_3));

	/* 3 fragments, xid=2 */
	char	   *expected_0_2_2_3[2] = {"0", "2"};

	/* seg = 0 */
	test_list(0, 2, 2, 3, expected_0_2_2_3, ARRSIZE(expected_0_2_2_3));
	char	   *expected_1_2_2_3[1] = {"1"};

	/* seg = 1 */
	test_list(1, 2, 2, 3, expected_1_2_2_3, ARRSIZE(expected_1_2_2_3));

	/* 3 fragments, xid=3 */
	char	   *expected_0_2_3_3[1] = {"1"};

	/* seg = 0 */
	test_list(0, 2, 3, 3, expected_0_2_3_3, ARRSIZE(expected_0_2_3_3));
	char	   *expected_1_2_3_3[2] = {"0", "2"};

	/* seg = 1 */
	test_list(1, 2, 3, 3, expected_1_2_3_3, ARRSIZE(expected_1_2_3_3));

	/* special case -- no fragments */
	MemoryContext old_context = CurrentMemoryContext;

	PG_TRY();
	{
		test_list(0, 1, 1, 0, NULL, 0);
		assert_false("Expected Exception");
	}
	PG_CATCH();
	{
		MemoryContextSwitchTo(old_context);
		ErrorData  *edata = CopyErrorData();

		assert_true(edata->elevel == ERROR);
		char	   *expected_message = pstrdup("Parameter list is null in filter_fragments_for_segment");

		assert_string_equal(edata->message, expected_message);
		pfree(expected_message);
	}
	PG_END_TRY();

	/* special case -- invalid transaction id */
	old_context = CurrentMemoryContext;
	PG_TRY();
	{
		test_list(0, 1, 0, 1, NULL, 0);
		assert_false("Expected Exception");
	}
	PG_CATCH();
	{
		MemoryContextSwitchTo(old_context);
		ErrorData  *edata = CopyErrorData();

		assert_true(edata->elevel == ERROR);
		char	   *expected_message = pstrdup("Cannot get distributed transaction identifier in filter_fragments_for_segment");

		assert_string_equal(edata->message, expected_message);
		pfree(expected_message);
	}
	PG_END_TRY();
}

static void
test_list(int segindex, int segtotal, int xid, int fragtotal, char *expected[], int expected_total)
{
	/* prepare the input list */
	List	   *list = prepare_fragment_list(fragtotal, segindex, segtotal, xid);

	/* filter the list */
	List	   *filtered = filter_fragments_for_segment(list);

	/* assert results */
	if (expected_total > 0)
	{
		assert_int_equal(filtered->length, expected_total);

		ListCell   *cell;
		int			i;

		foreach_with_count(cell, filtered, i)
		{
			assert_true(compareString(((FragmentData *) lfirst(cell))->index, expected[i]));
		}
	}
	else
	{
		assert_true(filtered == NIL);
	}
}

static List *
prepare_fragment_list(int fragtotal, int segindex, int segtotal, int xid)
{
	GpIdentity.segindex = segindex;
	GpIdentity.numsegments = segtotal;

	if (fragtotal > 0)
		will_return(getDistributedTransactionId, xid);

	List	   *result = NIL;

	for (int i = 0; i < fragtotal; i++)
	{
		FragmentData *fragment = palloc0(sizeof(FragmentData));
		char		index[2];

		sprintf(index, "%d", i);
		fragment->index = pstrdup(index);
		result = lappend(result, fragment);
	}
	return result;
}

static FragmentData *
buildFragment(const char *index, const char *source, const char *userdata, const char *metadata, const char *profile)
{
	FragmentData *fragment = (FragmentData *) palloc0(sizeof(FragmentData));

	fragment->index = index;
	fragment->source_name = source;
	fragment->profile = profile;
	fragment->fragment_md = metadata;
	fragment->user_data = userdata;
	return fragment;
}

void
test_parse_get_fragments_response(void **state)
{
	List	   *expected_data_fragments = NIL;

	expected_data_fragments = lappend(expected_data_fragments, buildFragment("0", "demo/text2.csv", "dummyuserdata1", "metadatavalue1", "DemoProfile"));
	expected_data_fragments = lappend(expected_data_fragments, buildFragment("1", "demo/text_csv.csv", "dummyuserdata2", "metadatavalue2", "DemoProfile"));
	List	   *data_fragments = NIL;
	StringInfoData frag_json;

	initStringInfo(&frag_json);
	appendStringInfo(&frag_json, "{\"PXFFragments\":[{\"index\":0,\"sourceName\":\"demo/text2.csv\",\"userData\":\"dummyuserdata1\",\"metadata\":\"metadatavalue1\",\"replicas\":[\"localhost\",\"localhost\",\"localhost\"],\"profile\":\"DemoProfile\"},{\"index\":1,\"sourceName\":\"demo/text_csv.csv\",\"userData\":\"dummyuserdata2\",\"metadata\":\"metadatavalue2\",\"replicas\":[\"localhost\",\"localhost\",\"localhost\"],\"profile\":\"DemoProfile\"}]}");
	data_fragments = parse_get_fragments_response(data_fragments, &frag_json);
	assert_true(compareLists(data_fragments, expected_data_fragments, compareFragment));
}

void
test_parse_get_fragments_response_bad_index(void **state)
{
	List	   *expected_data_fragments = NIL;

	expected_data_fragments = lappend(expected_data_fragments, buildFragment("4", "demo/text2.csv", "dummyuserdata1", "metadatavalue1", NULL));
	expected_data_fragments = lappend(expected_data_fragments, buildFragment("1", "demo/text_csv.csv", "dummyuserdata2", "metadatavalue2", NULL));
	List	   *data_fragments = NIL;
	StringInfoData frag_json;

	initStringInfo(&frag_json);
	appendStringInfo(&frag_json, "{\"PXFFragments\":[{\"index\":0,\"sourceName\":\"demo/text2.csv\",\"userData\":\"dummyuserdata1\",\"metadata\":\"metadatavalue1\",\"replicas\":[\"localhost\",\"localhost\",\"localhost\"]},{\"index\":1,\"sourceName\":\"demo/text_csv.csv\",\"userData\":\"dummyuserdata2\",\"metadata\":\"metadatavalue2\",\"replicas\":[\"localhost\",\"localhost\",\"localhost\"]}]}");
	data_fragments = parse_get_fragments_response(data_fragments, &frag_json);
	assert_false(compareLists(data_fragments, expected_data_fragments, compareFragment));
}

void
test_parse_get_fragments_response_bad_metadata(void **state)
{
	List	   *expected_data_fragments = NIL;

	expected_data_fragments = lappend(expected_data_fragments, buildFragment("0", "demo/text2.csv", "dummyuserdata1", "metadatavalue5", NULL));
	List	   *data_fragments = NIL;
	StringInfoData frag_json;

	initStringInfo(&frag_json);
	appendStringInfo(&frag_json, "{\"PXFFragments\":[{\"index\":0,\"sourceName\":\"demo/text2.csv\",\"userData\":\"dummyuserdata1\",\"metadata\":\"metadatavalue1\",\"replicas\":[\"localhost\",\"localhost\",\"localhost\"]}]}");
	data_fragments = parse_get_fragments_response(data_fragments, &frag_json);
	assert_false(compareLists(data_fragments, expected_data_fragments, compareFragment));
}

void
test_parse_get_fragments_response_nulluserdata(void **state)
{
	List	   *data_fragments = NIL;
	List	   *expected_data_fragments = NIL;

	expected_data_fragments = lappend(expected_data_fragments, buildFragment("1", "demo/text2.csv", NULL, "metadatavalue1", "DemoProfile"));
	StringInfoData frag_json;

	initStringInfo(&frag_json);
	appendStringInfo(&frag_json, "{\"PXFFragments\":[{\"index\":1,\"userData\":null,\"sourceName\":\"demo/text2.csv\",\"metadata\":\"metadatavalue1\",\"replicas\":[\"localhost\",\"localhost\",\"localhost\"],\"profile\":\"DemoProfile\"}]}");
	data_fragments = parse_get_fragments_response(data_fragments, &frag_json);
	assert_true(compareLists(data_fragments, expected_data_fragments, compareFragment));
}

void
test_parse_get_fragments_response_nullfragment(void **state)
{
	List	   *data_fragments = NIL;
	StringInfoData frag_json;

	initStringInfo(&frag_json);
	appendStringInfo(&frag_json, "{}");
	data_fragments = parse_get_fragments_response(data_fragments, &frag_json);
	assert_true(data_fragments == NULL);
}


void
test_parse_get_fragments_response_emptyfragment(void **state)
{
	List	   *data_fragments = NIL;
	StringInfoData frag_json;

	initStringInfo(&frag_json);
	appendStringInfo(&frag_json, "{\"PXFFragments\":[]}");
	data_fragments = parse_get_fragments_response(data_fragments, &frag_json);
	assert_true(data_fragments == NULL);
}

void
test_call_rest(void **state)
{
	GPHDUri    *hadoop_uri = (GPHDUri *) palloc0(sizeof(GPHDUri));

	hadoop_uri->host = "host";
	hadoop_uri->port = "123";


	ClientContext *client_context = (ClientContext *) palloc0(sizeof(ClientContext));

	client_context->http_headers = (CHURL_HEADERS) palloc0(sizeof(CHURL_HEADERS));
	initStringInfo(&(client_context->the_rest_buf));

	CHURL_HANDLE handle = (CHURL_HANDLE) palloc0(sizeof(CHURL_HANDLE));

	char	   *rest_msg = "http://%s:%s/%s/%s";

	expect_value(print_http_headers, headers, client_context->http_headers);
	will_be_called(print_http_headers);
	(client_context->http_headers);


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

	char	   *str = pstrdup("Hello ");

	/* will_assign_memory(churl_read, buf, str, 7); */
	will_return(churl_read, 7);

	/* second call to read should return World */
	expect_value(churl_read, handle, handle);
	expect_any(churl_read, buf);
	expect_any(churl_read, max_size);
	/* will_assign_memory(churl_read, buf, "World", 6); */
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

	/* TODO: debug this, seems will_assign_memory is not quite working */
	/* assert_string_equal(client_context->the_rest_buf.data, "Hello World"); */

	pfree(expected_url.data);
	pfree(client_context->http_headers);
	pfree(client_context);
	pfree(hadoop_uri);
}

static bool
compareLists(List *list1, List *list2, bool (*compareType) (void *, void *))
{
	if ((list1 && !list2) || (list2 && !list1) || (list1->length != list2->length))
		return false;

	int			outerIndex = 0;
	ListCell   *object1 = NULL;
	ListCell   *object2 = NULL;

	foreach(object1, list1)
	{
		bool		isExists = false;
		int			innerIndex = 0;

		foreach(object2, list2)
		{
			innerIndex++;
			if ((*compareType) (object1, object2))
			{
				isExists = true;
				break;
			}
		}
		outerIndex++;
		if (!isExists)
			return false;
	}

	return true;
}

static bool
compareString(char *str1, char *str2)
{
	if (((str1 == NULL) && (str2 == NULL)) || (strcmp(str1, str2) == 0))
		return true;
	else
		return false;
}

static bool
compareFragment(ListCell *fragment_cell1, ListCell *fragment_cell2)
{
	FragmentData *fragment1 = (FragmentData *) lfirst(fragment_cell1);
	FragmentData *fragment2 = (FragmentData *) lfirst(fragment_cell2);

	return (compareString(fragment1->index, fragment2->index) &&
			compareString(fragment1->source_name, fragment2->source_name) &&
			compareString(fragment1->fragment_md, fragment2->fragment_md) &&
			compareString(fragment1->user_data, fragment2->user_data) &&
			compareString(fragment1->profile, fragment2->profile));
}


int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test(test_filter_fragments_for_segment),
		unit_test(test_parse_get_fragments_response),
		unit_test(test_parse_get_fragments_response_bad_metadata),
		unit_test(test_parse_get_fragments_response_bad_index),
		unit_test(test_parse_get_fragments_response_nullfragment),
		unit_test(test_parse_get_fragments_response_emptyfragment),
		unit_test(test_parse_get_fragments_response_nulluserdata),
		unit_test(test_call_rest)
	};

	MemoryContextInit();

	return run_tests(tests);
}
