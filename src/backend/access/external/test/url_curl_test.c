#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../url_curl.c"

static void
test__make_url(void **state)
{
	const char	   *url1 = "http://[:0]/foo/bar";
	const char	   *url2 = "http://[:0]:8080/foo/bar";
	int				result;
	char			outbuf[1024];

	/* test ipv6 with no port, NULL outbuf */
	expect_string(getDnsAddress, hostname, ":0");
	expect_value(getDnsAddress, port, 80);
	expect_value(getDnsAddress, elevel, ERROR);
	will_return(getDnsAddress, ":0");

	result = make_url(url1, NULL, true);

	assert_int_equal(result, strlen(url1));

	/* test ipv6 with no port, valid outbuf */
	expect_string(getDnsAddress, hostname, ":0");
	expect_value(getDnsAddress, port, 80);
	expect_value(getDnsAddress, elevel, ERROR);
	will_return(getDnsAddress, ":0");

	result = make_url(url1, outbuf, true);

	assert_string_equal(url1, outbuf);
	assert_int_equal(result, strlen(url1));

	/* test ipv6 with explicit port */
	expect_string(getDnsAddress, hostname, ":0");
	expect_value(getDnsAddress, port, 8080);
	expect_value(getDnsAddress, elevel, ERROR);
	will_return(getDnsAddress, ":0");

	result = make_url(url2, outbuf, true);

	assert_string_equal(url2, outbuf);
	assert_int_equal(result, strlen(url2));
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__make_url)
	};

	MemoryContextInit();

	return run_tests(tests);
}
