#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

/* Define UNIT_TESTING so that the extension can skip declaring PG_MODULE_MAGIC */
#define UNIT_TESTING

/* include unit under test */
#include "../src/pxfbridge.c"

/* include mock files */
#include "mock/libchurl_mock.c"
#include "mock/pxfuriparser_mock.c"
#include "mock/pxfheaders_mock.c"
#include "mock/pxfutils_mock.c"
#include "../src/pxfbridge.h"

/* helper functions */
static void expect_set_headers_call(CHURL_HEADERS headers_handle, const char *header_key, const char *header_value);

static const char *AUTHORITY = "127.0.0.1";

void
test_gpbridge_cleanup(void **state)
{
	/* init data in context that will be cleaned up */
	gphadoop_context *context = palloc0(sizeof(gphadoop_context));

	context->churl_handle = (CHURL_HANDLE) palloc0(sizeof(CHURL_HANDLE));
	context->churl_headers = (CHURL_HEADERS) palloc0(sizeof(CHURL_HEADERS));
	context->gphd_uri = (GPHDUri *) palloc0(sizeof(GPHDUri));

	/* set mock behavior for churl cleanup */
	expect_value(churl_cleanup, handle, context->churl_handle);
	expect_value(churl_cleanup, after_error, false);
	will_be_called(churl_cleanup);

	expect_value(churl_headers_cleanup, headers, context->churl_headers);
	will_be_called(churl_headers_cleanup);

	expect_value(freeGPHDUri, uri, context->gphd_uri);
	will_be_called(freeGPHDUri);

	/* call function under test */
	gpbridge_cleanup(context);

	/* assert call results */
	assert_true(context->churl_handle == NULL);
	assert_true(context->churl_headers == NULL);
	assert_true(context->gphd_uri == NULL);

	/* cleanup */
	pfree(context);
}

void
test_gpbridge_import_start(void **state)
{
	/* init data in context that will be cleaned up */
	gphadoop_context *context = (gphadoop_context *) palloc0(sizeof(gphadoop_context));

	initStringInfo(&context->uri);

	/* setup list of fragments */
	FragmentData *fragment = (FragmentData *) palloc0(sizeof(FragmentData));

	fragment->authority = AUTHORITY;
	fragment->fragment_md = "md";
	fragment->index = "1";
	fragment->profile = NULL;
	fragment->source_name = "source";
	fragment->user_data = "user_data";

	context->gphd_uri = (GPHDUri *) palloc0(sizeof(GPHDUri));
	List	   *list = list_make1(fragment);

	context->gphd_uri->fragments = list;
	context->gphd_uri->profile = "profile";

	CHURL_HEADERS headers = (CHURL_HEADERS) palloc0(sizeof(CHURL_HEADERS));

	will_return(churl_headers_init, headers);

	expect_any(build_http_headers, input);
	/* might verify params later */
	will_be_called(build_http_headers);

	expect_set_headers_call(headers, "X-GP-DATA-DIR", fragment->source_name);
	expect_set_headers_call(headers, "X-GP-DATA-FRAGMENT", fragment->index);
	expect_set_headers_call(headers, "X-GP-FRAGMENT-METADATA", fragment->fragment_md);
	expect_set_headers_call(headers, "X-GP-FRAGMENT-INDEX", fragment->index);
	expect_set_headers_call(headers, "X-GP-FRAGMENT-USER-DATA", fragment->user_data);
	expect_set_headers_call(headers, "X-GP-PROFILE", context->gphd_uri->profile);

	CHURL_HANDLE handle = (CHURL_HANDLE) palloc0(sizeof(CHURL_HANDLE));

	expect_value(churl_init_download, url, context->uri.data);
	expect_value(churl_init_download, headers, headers);
	will_return(churl_init_download, handle);

	expect_value(churl_read_check_connectivity, handle, handle);
	will_be_called(churl_read_check_connectivity);

	/* call function under test */
	gpbridge_import_start(context);

	/* assert call results */
	assert_int_equal(context->current_fragment, list_head(context->gphd_uri->fragments));

	StringInfoData expected_uri;

	initStringInfo(&expected_uri);
	appendStringInfo(&expected_uri,
					 "http://%s/%s/%s/Bridge/",
					 AUTHORITY, PXF_SERVICE_PREFIX, PXF_VERSION);
	assert_string_equal(context->uri.data, expected_uri.data);
	assert_int_equal(context->churl_headers, headers);
	assert_int_equal(context->churl_handle, handle);

	/* cleanup */
	list_free_deep(list);
	pfree(handle);
	pfree(headers);
	pfree(context->gphd_uri);
	pfree(context);
}

void
test_gpbridge_export_start(void **state)
{
	/* init data in context that will be cleaned up */
	gphadoop_context *context = (gphadoop_context *) palloc0(sizeof(gphadoop_context));
	initStringInfo(&context->uri);
	initStringInfo(&context->write_file_name);
	context->gphd_uri = (GPHDUri *) palloc0(sizeof(GPHDUri));
	context->gphd_uri->data = "path";
	context->gphd_uri->profile = "profile";

	/* expectations for creating file name for write */
	char		xid[TMGIDSIZE]="abcdefghijklmnopqrstu";

	GpIdentity.segindex = 3;
	char* expected_file_name = psprintf("/%s/%s_%d", "path", xid, 3);
	expect_any(getDistributedTransactionIdentifier, id);
	will_assign_memory(getDistributedTransactionIdentifier, id, xid, TMGIDSIZE);
	will_return(getDistributedTransactionIdentifier, true);

	/* expectation for remote uri construction */
	will_return(get_authority, "abc:123");
	char* expected_uri = psprintf("http://abc:123/pxf/v15/Writable/stream?path=%s", expected_file_name);

	CHURL_HEADERS headers = (CHURL_HEADERS) palloc0(sizeof(CHURL_HEADERS));
	will_return(churl_headers_init, headers);

	expect_any(build_http_headers, input);
	/* might verify params later */
	will_be_called(build_http_headers);

	CHURL_HANDLE handle = (CHURL_HANDLE) palloc0(sizeof(CHURL_HANDLE));
	expect_value(churl_init_upload, url, context->uri.data);
	expect_value(churl_init_upload, headers, headers);
	will_return(churl_init_upload, handle);

	/* call function under test */
	gpbridge_export_start(context);

	/* assert call results */
	assert_string_equal(context->write_file_name.data, expected_file_name);
	assert_string_equal(context->uri.data, expected_uri);
	assert_int_equal(context->churl_headers, headers);
	assert_int_equal(context->churl_handle, handle);

	/* cleanup */
	pfree(handle);
	pfree(headers);
	pfree(context->gphd_uri);
	pfree(context);
}

void
test_gpbridge_read_one_fragment_less_than_buffer(void **state)
{
	/* init data in context */
	gphadoop_context *context = (gphadoop_context *) palloc0(sizeof(gphadoop_context));

	context->gphd_uri = (GPHDUri *) palloc0(sizeof(GPHDUri));
	context->gphd_uri->fragments = (FragmentData *) palloc0(sizeof(FragmentData));
	CHURL_HANDLE handle = (CHURL_HANDLE) palloc0(sizeof(CHURL_HANDLE));

	context->churl_handle = handle;

	int			datalen = 10;
	char	   *databuf = (char *) palloc0(datalen);

	/* first call returns less bytes than buffer size (4 bytes read) */
	expect_value(churl_read, handle, context->churl_handle);
	expect_value(churl_read, buf, databuf);
	expect_value(churl_read, max_size, datalen);
	will_return(churl_read, 4);

	/* second call returns 0, indicating no more data */
	expect_value(churl_read, handle, context->churl_handle);
	expect_value(churl_read, buf, databuf + 4);
	expect_value(churl_read, max_size, datalen - 4);
	will_return(churl_read, 0);

	/* call function under test */
	int			bytes_read = gpbridge_read(context, databuf, datalen);

	/* assert call results */
	assert_int_equal(bytes_read, 4);

	/* cleanup */
	pfree(handle);
	pfree(databuf);
	pfree(context);
}

void
test_gpbridge_read_one_fragment_buffer(void **state)
{
	/* init data in context */
	gphadoop_context *context = (gphadoop_context *) palloc0(sizeof(gphadoop_context));

	context->gphd_uri = (GPHDUri *) palloc0(sizeof(GPHDUri));
	context->gphd_uri->fragments = (FragmentData *) palloc0(sizeof(FragmentData));
	CHURL_HANDLE handle = (CHURL_HANDLE) palloc0(sizeof(CHURL_HANDLE));

	context->churl_handle = handle;

	int			datalen = 10;
	char	   *databuf = (char *) palloc0(datalen);

	/*
	 * first call returns as many bytes as buffer size (10 bytes read), only 1
	 * call to read
	 */
	expect_value(churl_read, handle, context->churl_handle);
	expect_value(churl_read, buf, databuf);
	expect_value(churl_read, max_size, datalen);
	will_return(churl_read, 10);

	/* call function under test */
	int			bytes_read = gpbridge_read(context, databuf, datalen);

	/* assert call results */
	assert_int_equal(bytes_read, 10);

	/* cleanup */
	pfree(handle);
	pfree(databuf);
	pfree(context);
}

void
test_gpbridge_read_next_fragment_buffer(void **state)
{
	/* init data in context */
	gphadoop_context *context = (gphadoop_context *) palloc0(sizeof(gphadoop_context));
	CHURL_HANDLE handle = (CHURL_HANDLE) palloc0(sizeof(CHURL_HANDLE));

	context->churl_handle = handle;
	CHURL_HEADERS headers = (CHURL_HEADERS) palloc0(sizeof(CHURL_HEADERS));

	context->churl_headers = headers;

	initStringInfo(&context->uri);

	/* setup list of fragments */
	FragmentData *prev_fragment = (FragmentData *) palloc0(sizeof(FragmentData));
	FragmentData *fragment = (FragmentData *) palloc0(sizeof(FragmentData));

	fragment->authority = AUTHORITY;
	fragment->fragment_md = "md";
	fragment->index = "1";
	fragment->profile = NULL;
	fragment->source_name = "source";
	fragment->user_data = "user_data";

	List	   *list = list_make2(prev_fragment, fragment);

	context->current_fragment = list_head(list);

	context->gphd_uri = (GPHDUri *) palloc0(sizeof(GPHDUri));
	context->gphd_uri->profile = "profile";
	context->gphd_uri->fragments = (FragmentData *) palloc0(sizeof(FragmentData));

	int			datalen = 10;
	char	   *databuf = (char *) palloc0(datalen);

	/* first call for current fragment returns 0 as no more data available */
	expect_value(churl_read, handle, context->churl_handle);
	expect_value(churl_read, buf, databuf);
	expect_value(churl_read, max_size, datalen);
	will_return(churl_read, 0);

	expect_value(churl_read_check_connectivity, handle, handle);
	will_be_called(churl_read_check_connectivity);

	expect_set_headers_call(headers, "X-GP-DATA-DIR", fragment->source_name);
	expect_set_headers_call(headers, "X-GP-DATA-FRAGMENT", fragment->index);
	expect_set_headers_call(headers, "X-GP-FRAGMENT-METADATA", fragment->fragment_md);
	expect_set_headers_call(headers, "X-GP-FRAGMENT-INDEX", fragment->index);
	expect_set_headers_call(headers, "X-GP-FRAGMENT-USER-DATA", fragment->user_data);
	expect_set_headers_call(headers, "X-GP-PROFILE", context->gphd_uri->profile);

	expect_value(churl_download_restart, handle, handle);
	expect_value(churl_download_restart, url, context->uri.data);
	expect_value(churl_download_restart, headers, headers);
	will_be_called(churl_download_restart);

	expect_value(churl_read_check_connectivity, handle, handle);
	will_be_called(churl_read_check_connectivity);

	/*
	 * call returns as many bytes as buffer size (10 bytes read), only 1 call
	 * to read
	 */
	expect_value(churl_read, handle, context->churl_handle);
	expect_value(churl_read, buf, databuf);
	expect_value(churl_read, max_size, datalen);
	will_return(churl_read, 10);

	/* call function under test */
	int			bytes_read = gpbridge_read(context, databuf, datalen);

	/* assert call results */
	assert_int_equal(bytes_read, 10);
	assert_int_equal(context->current_fragment, lnext(list_head((list))));
	/* second fragment became current */

	/* cleanup */
	list_free_deep(list);
	pfree(handle);
	pfree(headers);
	pfree(databuf);
	pfree(context->gphd_uri);
	pfree(context);
}

void
test_gpbridge_read_last_fragment_finished(void **state)
{
	/* init data in context */
	gphadoop_context *context = (gphadoop_context *) palloc0(sizeof(gphadoop_context));

	context->gphd_uri = (GPHDUri *) palloc0(sizeof(GPHDUri));
	context->gphd_uri->fragments = (FragmentData *) palloc0(sizeof(FragmentData));
	CHURL_HANDLE handle = (CHURL_HANDLE) palloc0(sizeof(CHURL_HANDLE));

	context->churl_handle = handle;

	initStringInfo(&context->uri);

	/* setup list of fragments */
	FragmentData *fragment = (FragmentData *) palloc0(sizeof(FragmentData));

	fragment->authority = AUTHORITY;
	fragment->fragment_md = "md";
	fragment->index = "1";
	fragment->profile = NULL;
	fragment->source_name = "source";
	fragment->user_data = "user_data";

	List	   *list = list_make1(fragment);

	context->current_fragment = list_head(list);

	int			datalen = 10;
	char	   *databuf = (char *) palloc0(datalen);

	/* first call for current fragment returns 0 as no more data available */
	expect_value(churl_read, handle, context->churl_handle);
	expect_value(churl_read, buf, databuf);
	expect_value(churl_read, max_size, datalen);
	will_return(churl_read, 0);

	expect_value(churl_read_check_connectivity, handle, handle);
	will_be_called(churl_read_check_connectivity);

	/* then since the next fragment is not available, reading is completed */

	/* call function under test */
	int			bytes_read = gpbridge_read(context, databuf, datalen);

	/* assert call results */
	assert_int_equal(bytes_read, 0);
	assert_int_equal(context->current_fragment, NULL);
	/* second fragment became current */

	/* cleanup */
	list_free_deep(list);
	pfree(handle);
	pfree(databuf);
	pfree(context);
}

void
test_gpbridge_write_data(void **state) {
	/* init data in context */
	gphadoop_context *context = (gphadoop_context *) palloc0(sizeof(gphadoop_context));
	initStringInfo(&context->write_file_name);
	CHURL_HANDLE handle = (CHURL_HANDLE) palloc0(sizeof(CHURL_HANDLE));
	context->churl_handle = handle;

	/* set mock behavior */
	char*	databuf = "foo";
	int		datalen = 3;
	expect_value(churl_write, handle, context->churl_handle);
	expect_value(churl_write, buf, databuf);
	expect_value(churl_write, bufsize, datalen);
	will_return(churl_write, 3);

	/* call function under test */
	int bytes_written = gpbridge_write(context, databuf, datalen);

	/* assert call results */
	assert_int_equal(bytes_written, 3);

	/* cleanup */
	pfree(handle);
	pfree(context);
}

void
test_gpbridge_write_no_data(void **state) {
	/* init data in context */
	gphadoop_context *context = (gphadoop_context *) palloc0(sizeof(gphadoop_context));

	/* call function under test */
	char*	databuf = NULL;
	int		datalen = 0;
	int bytes_written = gpbridge_write(context, databuf, datalen);

	/* assert call results */
	assert_int_equal(bytes_written, 0);

	/* cleanup */
	pfree(context);
}

static void
expect_set_headers_call(CHURL_HEADERS headers_handle, const char *header_key, const char *header_value)
{
	expect_string(churl_headers_override, headers, headers_handle);
	expect_string(churl_headers_override, key, header_key);
	expect_string(churl_headers_override, value, header_value);
	will_be_called(churl_headers_override);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test(test_gpbridge_cleanup),
		unit_test(test_gpbridge_import_start),
		unit_test(test_gpbridge_read_one_fragment_less_than_buffer),
		unit_test(test_gpbridge_read_one_fragment_buffer),
		unit_test(test_gpbridge_read_next_fragment_buffer),
		unit_test(test_gpbridge_read_last_fragment_finished),
		unit_test(test_gpbridge_export_start),
		unit_test(test_gpbridge_write_data),
		unit_test(test_gpbridge_write_no_data)
	};

	MemoryContextInit();

	return run_tests(tests);
}
