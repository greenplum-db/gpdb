#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../ip.c"

test_pg_get_nameinfo_all_unix(bool log_hostname)
{
	int result = -1;
	char remote_host[NI_MAXHOST] = {'\0'};
	char remote_port[NI_MAXSERV] = {'\0'};
	struct sockaddr_storage *addr = (struct sockaddr_storage *) malloc(sizeof(struct sockaddr_storage));
	int salen = sizeof(struct sockaddr_storage);

	/* initialize UNIX socket parameters */
	addr->ss_family = AF_UNIX;
#if defined(__darwin__)
	strcpy(addr->__ss_pad1, "mock");
#else
	strcpy(addr->__ss_padding, "mock");
#endif

	/* This code is pretty much duplicated from postmaster.c:BackendInitialize() */
	result = pg_getnameinfo_all(addr, salen,
				    remote_host, sizeof(remote_host),
				    remote_port, sizeof(remote_port),
				    (log_hostname ? 0 : NI_NUMERICHOST) | NI_NUMERICSERV);

	assert_true(result != EAI_FAIL);
	assert_string_equal(remote_host, "[local]");
	assert_string_equal(remote_port, "mock");
}

void
test_pg_get_nameinfo_all_unix_log_hostname_false(void **state)
{
	test_pg_get_nameinfo_all_unix(false);
}

void
test_pg_get_nameinfo_all_unix_log_hostname_true(void **state)
{
	test_pg_get_nameinfo_all_unix(true);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_pg_get_nameinfo_all_unix_log_hostname_false),
		unit_test(test_pg_get_nameinfo_all_unix_log_hostname_true)
	};

	MemoryContextInit();

	return run_tests(tests);
}
