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
#include "../src/libchurl.c"

/* include mock files */
#include "mock/curl_mock.c"

/* test strings */
const char *uri_param = "pxf://localhost:5888/tmp/dummy1";
char *read_string = "Hello World!\0";

static void
curl_easy_setopt_test_helper(CURL *curl_handle, CURLoption in_option)
{
	expect_value(curl_easy_setopt, curl, curl_handle);
	expect_value(curl_easy_setopt, option, in_option);
	will_return(curl_easy_setopt, CURLE_OK);
}

static void
curl_slist_append_test_helper(struct curl_slist *slist, char *header_string)
{
	expect_any(curl_slist_append, list);
	expect_string(curl_slist_append, string, header_string);
	will_return(curl_slist_append, slist);
}

static void
test_set_curl_option(void **state)
{
	/* set up context with a curl_handle */
	churl_context *context = palloc0(sizeof(churl_context));
	/* mock a curl_easy_init call which returns a calloced CURL handle */
	context->curl_handle = palloc0(1);

	/* set mock behavior for curl_easy_setopt */
	curl_easy_setopt_test_helper(context->curl_handle, CURLOPT_URL);

	/* function call */
	set_curl_option(context, CURLOPT_URL, uri_param);

	/* cleanup */
	pfree(context->curl_handle);
	pfree(context);
}


static CURL *
test_churl_init()
{
	/*
	 * Set mock behavior for curl handle initialization. curl_easy_init will
	 * return a calloced CURL handle, but since the CURL struct is opaque we
	 * cannot allocate a sizeof(CURL) chunk here. Since these tests never use
	 * the mocked handle for anything but non-NULL checks, settle for
	 * allocating a small buffer.
	 */
	CURL	   *mock_curl_handle = palloc0(1);
	will_return(curl_easy_init, mock_curl_handle);

	/* set mock behavior for all the curl_easy_setopt calls */
#ifdef CURLOPT_RESOLVE
	curl_easy_setopt_test_helper(mock_curl_handle, CURLOPT_RESOLVE);
	curl_slist_append_test_helper(NULL, "localhost:5888:127.0.0.1");
#endif
	curl_easy_setopt_test_helper(mock_curl_handle, CURLOPT_URL);
	curl_easy_setopt_test_helper(mock_curl_handle, CURLOPT_VERBOSE);
	curl_easy_setopt_test_helper(mock_curl_handle, CURLOPT_ERRORBUFFER);
	curl_easy_setopt_test_helper(mock_curl_handle, CURLOPT_IPRESOLVE);
	curl_easy_setopt_test_helper(mock_curl_handle, CURLOPT_WRITEFUNCTION);
	curl_easy_setopt_test_helper(mock_curl_handle, CURLOPT_WRITEDATA);
	curl_easy_setopt_test_helper(mock_curl_handle, CURLOPT_HEADERFUNCTION);
	curl_easy_setopt_test_helper(mock_curl_handle, CURLOPT_HEADERDATA);
	curl_easy_setopt_test_helper(mock_curl_handle, CURLOPT_HTTPHEADER);

	return mock_curl_handle;
}

static void
test_churl_init_upload(void **state)
{
	CHURL_HEADERS headers = palloc0(sizeof(CHURL_HEADERS));
	CURL	   *mock_curl_handle = test_churl_init();

	curl_easy_setopt_test_helper(mock_curl_handle, CURLOPT_POST);
	curl_easy_setopt_test_helper(mock_curl_handle, CURLOPT_READFUNCTION);
	curl_easy_setopt_test_helper(mock_curl_handle, CURLOPT_READDATA);

	struct curl_slist *mock_curl_slist = palloc0(sizeof(struct curl_slist));
	curl_slist_append_test_helper(mock_curl_slist, "Content-Type: application/octet-stream");
	curl_slist_append_test_helper(mock_curl_slist, "Transfer-Encoding: chunked");
	curl_slist_append_test_helper(mock_curl_slist, "Expect: 100-continue");

	/*
	 * setup_multi_handle mock setup. curl_multi_init will return a calloced
	 * CURLM handle, but since the CURL struct is opaque we cannot allocate a
	 * sizeof(CURLM) chunk here. Since these tests never use the mocked handle
	 * for anything but non-NULL checks, settle for allocating a small buffer.
	 */
	CURLM	   *mock_multi_handle = palloc0(1);
	will_return(curl_multi_init, mock_multi_handle);

	expect_value(curl_multi_add_handle, multi_handle, mock_multi_handle);
	expect_value(curl_multi_add_handle, curl_handle, mock_curl_handle);
	will_return(curl_multi_add_handle, CURLM_OK);

	expect_value(curl_multi_perform, multi_handle, mock_multi_handle);
	expect_any(curl_multi_perform, running_handles);
	will_return(curl_multi_perform, CURLM_OK);

	/* function call */
	CHURL_HANDLE handle = churl_init_upload(uri_param, headers);
	churl_context *context = (churl_context *) handle;

	assert_true(context->upload == true);
	assert_true(context->curl_error_buffer[0] == 0);
	assert_true(context->download_buffer != NULL);
	assert_true(context->upload_buffer != NULL);
	assert_true(context->curl_handle != NULL);
	assert_true(context->multi_handle != NULL);
	assert_true(context->curl_still_running == 0);

	/* tear down */
	pfree(mock_curl_handle);
	pfree(mock_multi_handle);
	pfree(headers);
	pfree(handle);
}

static void
test_churl_init_download(void **state)
{
	CHURL_HEADERS headers = palloc0(sizeof(CHURL_HEADERS));
	CURL	   *mock_curl_handle = test_churl_init();

	/*
	 * setup_multi_handle mock setup. curl_multi_init will return a calloced
	 * CURLM handle, but since the CURL struct is opaque we cannot allocate a
	 * sizeof(CURLM) chunk here. Since these tests never use the mocked handle
	 * for anything but non-NULL checks, settle for allocating a small buffer.
	 */
	CURLM	   *mock_multi_handle = palloc0(1);
	will_return(curl_multi_init, mock_multi_handle);

	expect_value(curl_multi_add_handle, multi_handle, mock_multi_handle);
	expect_value(curl_multi_add_handle, curl_handle, mock_curl_handle);
	will_return(curl_multi_add_handle, CURLM_OK);

	expect_value(curl_multi_perform, multi_handle, mock_multi_handle);
	expect_any(curl_multi_perform, running_handles);
	will_return(curl_multi_perform, CURLM_OK);

	/* function call */
	CHURL_HANDLE handle = churl_init_download(uri_param, headers);
	churl_context *context = (churl_context *) handle;

	/* test assertions */
	assert_true(context->upload == false);
	assert_true(context->curl_error_buffer[0] == 0);
	assert_true(context->download_buffer != NULL);
	assert_true(context->upload_buffer != NULL);
	assert_true(context->curl_handle != NULL);
	assert_true(context->multi_handle != NULL);

	/* tear down */
	pfree(mock_curl_handle);
	pfree(mock_multi_handle);
	pfree(headers);
	pfree(handle);
}

/*  wrapper function to enable sideeffect testing with multiple parameters */
static void
write_callback_wrapper(void *ptr)
{
	churl_context * user_context = (churl_context *) ptr;
	write_callback(read_string, sizeof(char), strlen(read_string) + 1, user_context);
}

static void
test_churl_read(void **state)
{
	/* context setup */
	CHURL_HANDLE handle = palloc0(sizeof(CHURL_HANDLE));
	churl_context *context = (churl_context *) handle;
	/* mock curl_multi_init */
	CURLM	   *mock_multi_handle = palloc0(1);

	/* buffer to read into */
	int			READ_LEN = 32;
	char		buf[READ_LEN];

	context->download_buffer = palloc0(sizeof(churl_buffer));
	context->multi_handle = mock_multi_handle;
	context->curl_still_running = 1;
	context->upload = 0;
	context->download_buffer->top = 0;
	context->download_buffer->bot = 0;
	context->download_buffer->ptr = palloc0(sizeof(READ_LEN));
	context->download_buffer->max = READ_LEN;

	/* set up curl_multi_fdset mock behavior */
	int fd = 1;
	expect_value(curl_multi_fdset, multi_handle, mock_multi_handle);
	expect_any(curl_multi_fdset, read_fd_set);
	expect_any(curl_multi_fdset, write_fd_set);
	expect_any(curl_multi_fdset, exc_fd_set);
	expect_any(curl_multi_fdset, max_fd);
	will_assign_value(curl_multi_fdset, max_fd, fd);
	will_return(curl_multi_fdset, CURLM_OK);

	/* set up select mock behavior */
	expect_value(select, nfds, fd + 1);
	expect_any(select, read_fds);
	expect_any(select, write_fds);
	expect_any(select, except_fds);
	expect_any(select, timeout);
	will_return(select, 1);

	/* set up curl_multi_perform mock behavior */
	/* this function invokes callback functions which are handled by */
	/* write_callback_wrapper */
	expect_value(curl_multi_perform, multi_handle, mock_multi_handle);
	expect_any(curl_multi_perform, running_handles);
	will_assign_value(curl_multi_perform, running_handles, 0);
	will_return_with_sideeffect(curl_multi_perform, CURLM_OK, write_callback_wrapper, context);

	/* function call */
	size_t	buffer_offset = churl_read(handle, buf, READ_LEN);

	/* test assertions */
	assert_true(context->curl_still_running == 0);
	assert_true(strcmp(buf, read_string) == 0);
	assert_true(buffer_offset == 13);
	assert_true(context->download_buffer->bot == 13);
	assert_true(context->download_buffer->bot == context->download_buffer->top);

	pfree(context->download_buffer);
	pfree(handle);
}


int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test(test_set_curl_option),
		unit_test(test_churl_init_upload),
		unit_test(test_churl_init_download),
		unit_test(test_churl_read)
	};

	MemoryContextInit();

	return run_tests(tests);
}
