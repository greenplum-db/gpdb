#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"

#define Assert(condition) if (!condition) AssertFailed()

bool is_assert_failed = false;

void AssertFailed()
{
	is_assert_failed = true;
}

/* Actual function body */
#include "../ftsprobehandler.c"

void
test_HandleFtsWalRepProbe(void **state)
{
	bool expected_mirror_state = true;

	will_return(IsMirrorUp, expected_mirror_state);

	expect_any(initStringInfo, str);
	will_be_called(initStringInfo);

	expect_any(pq_beginmessage, buf);
	expect_value(pq_beginmessage, msgtype, '\0');
	will_be_called(pq_beginmessage);

	ProbeResponse response;
	response.IsMirrorUp = expected_mirror_state;

	expect_any(pq_sendbytes, buf);
	expect_memory(pq_sendbytes, data, &response, sizeof(response));
	expect_value(pq_sendbytes, datalen, sizeof(ProbeResponse));
	will_be_called(pq_sendbytes);

	expect_any(pq_endmessage, buf);
	will_be_called(pq_endmessage);

	will_be_called(pq_flush);

	HandleFtsWalRepProbe();
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_HandleFtsWalRepProbe)
	};
	return run_tests(tests);
}
