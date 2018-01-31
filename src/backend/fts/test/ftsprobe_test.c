#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "utils/timestamp.h"

/* redefine pg_usleep to mock */
#undef pg_usleep
#define pg_usleep pg_usleep_mock

int pg_usleep_called = 0;
static void pg_usleep_mock(long microsec)
{
	/*
	 * An uninitialized role and mode will result in extra 2 second sleep to
	 * give primary segment more time to finish startup. Make sure 2 seconds
	 * is given to pg_usleep mock.
	 */
	assert_int_equal(microsec, 2 * USECS_PER_SEC);

	pg_usleep_called++;
}

/* Actual function body */
#include "../ftsprobe.c"

static void write_log_will_be_called()
{
	expect_any(write_log, fmt);
	will_be_called(write_log);
}

void
test_probeProcessResponse_uninitialized_role_and_mode(void **state)
{
	ProbeConnectionInfo probeInfo;
	uint32 n32;
	bool result;

	memset(&probeInfo, 0, sizeof(ProbeConnectionInfo));

	/* mock a primary probe response with uninitialized role and mode */
	n32 = htonl((uint32) PROBE_RESPONSE_LEN);
	memcpy(probeInfo.response, &n32, 4);
	n32 = htonl((uint32) PMModeUninitialized);
	memcpy(probeInfo.response+4, &n32, 4);
	n32 = htonl((uint32) SegmentStateNotInitialized);
	memcpy(probeInfo.response+8, &n32, 4);
	n32 = htonl((uint32) DataStateNotInitialized);
	memcpy(probeInfo.response+12, &n32, 4);
	n32 = htonl((uint32) FaultTypeNotInitialized);
	memcpy(probeInfo.response+16, &n32, 4);

	probeInfo.segmentStatus = PROBE_DEAD;
	probeInfo.role = 'p';
	probeInfo.mode = 's';

	/* reset pg_usleep counter */
	pg_usleep_called = 0;

	write_log_will_be_called();

	/* run the test */
	result = probeProcessResponse(&probeInfo);

	/* should have returned false and marked probe dead for probe retry */
	assert_false(result);
	assert_true(probeInfo.segmentStatus == PROBE_DEAD);
	assert_int_equal(pg_usleep_called, 1);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_probeProcessResponse_uninitialized_role_and_mode)
	};
	return run_tests(tests);
}
