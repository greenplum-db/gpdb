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

	expect_value(BeginCommand, commandTag, FTS_MSG_TYPE_PROBE);
	expect_value(BeginCommand, dest, DestRemote);
	will_be_called(BeginCommand);

	expect_any(initStringInfo, str);
	will_be_called(initStringInfo);

	/* schema message */
	expect_any(pq_beginmessage, buf);
	expect_value(pq_beginmessage, msgtype, 'T');
	will_be_called(pq_beginmessage);

	expect_any(pq_endmessage, buf);
	will_be_called(pq_endmessage);

	/* data message */
	expect_any(pq_beginmessage, buf);
	expect_value(pq_beginmessage, msgtype, 'D');
	will_be_called(pq_beginmessage);

	expect_any(pq_endmessage, buf);
	will_be_called(pq_endmessage);

	expect_any_count(pq_sendint, buf, -1);
	expect_any_count(pq_sendint, i, -1);
	expect_any_count(pq_sendint, b, -1);
	will_be_called_count(pq_sendint, -1);

	expect_any_count(pq_sendstring, buf, -1);
	expect_any_count(pq_sendstring, str, -1);
	will_be_called_count(pq_sendstring, -1);

	will_be_called(pq_flush);

	expect_value(EndCommand, commandTag, FTS_MSG_TYPE_PROBE);
	expect_value(EndCommand, dest, DestRemote);
	will_be_called(EndCommand);

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
