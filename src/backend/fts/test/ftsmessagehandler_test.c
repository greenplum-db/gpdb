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
#include "../ftsmessagehandler.c"

static void
mockSendFtsResponse(const char *messagetype)
{
	expect_value(BeginCommand, commandTag, messagetype);
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

	expect_value(EndCommand, commandTag, messagetype);
	expect_value(EndCommand, dest, DestRemote);
	will_be_called(EndCommand);

	will_be_called(pq_flush);
}
void
test_HandleFtsWalRepProbe(void **state)
{
	FtsResponse mockresponse;
	mockresponse.IsMirrorUp = true;
	mockresponse.IsInSync = true;
	mockresponse.IsSyncRepEnabled = false;

	expect_any(GetMirrorStatus, response);
	will_assign_memory(GetMirrorStatus, response, &mockresponse, sizeof(FtsResponse));
	will_be_called(GetMirrorStatus);

	will_be_called(SetSyncStandbysDefined);

	mockSendFtsResponse(FTS_MSG_PROBE);

	HandleFtsWalRepProbe();
}

void
test_HandleFtsWalRepSyncRepOff(void **state)
{
	FtsResponse mockresponse;
	mockresponse.IsMirrorUp = false;
	mockresponse.IsInSync = false;
	mockresponse.IsSyncRepEnabled = true;

	expect_any(GetMirrorStatus, response);
	will_assign_memory(GetMirrorStatus, response, &mockresponse, sizeof(FtsResponse));
	will_be_called(GetMirrorStatus);

	will_be_called(UnsetSyncStandbysDefined);

	mockSendFtsResponse(FTS_MSG_SYNCREP_OFF);

	HandleFtsWalRepSyncRepOff();
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_HandleFtsWalRepProbe),
		unit_test(test_HandleFtsWalRepSyncRepOff)
	};
	return run_tests(tests);
}
