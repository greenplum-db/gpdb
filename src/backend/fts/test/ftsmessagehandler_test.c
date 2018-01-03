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
expectSendFtsResponse(const char *expectedMessageType, const FtsResponse *expectedResponse)
{
	expect_value(BeginCommand, commandTag, expectedMessageType);
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

	int number_of_columns = Natts_fts_message_response;
	int calls_per_column_for_schema = 6;
	int calls_per_column_for_data = 2;
	int total_number_of_calls_of_pq_sendint = number_of_columns *
	  (calls_per_column_for_schema + calls_per_column_for_data) +
	  1 /*fixed for schema*/ + 1 /*fixed for data*/;
	int total_number_of_calls_of_pq_sendint_for_none_data = total_number_of_calls_of_pq_sendint - number_of_columns;
	int total_number_of_calls_of_pq_sendstring = number_of_columns;
	
	expect_any_count(pq_sendint, buf, -1);
	expect_any_count(pq_sendint, i, total_number_of_calls_of_pq_sendint_for_none_data);
  	expect_value(pq_sendint, i, expectedResponse->IsMirrorUp);
	expect_value(pq_sendint, i, expectedResponse->IsInSync);
	expect_value(pq_sendint, i, expectedResponse->IsSyncRepEnabled);
	expect_value(pq_sendint, i, expectedResponse->IsRoleMirror);
	expect_value(pq_sendint, i, expectedResponse->RequestRetry);
	expect_any_count(pq_sendint, b, -1);
	will_be_called_count(pq_sendint, total_number_of_calls_of_pq_sendint);

	expect_any_count(pq_sendstring, buf, -1);
	expect_any_count(pq_sendstring, str, -1);
	will_be_called_count(pq_sendstring, total_number_of_calls_of_pq_sendstring);

	expect_value(EndCommand, commandTag, expectedMessageType);
	expect_value(EndCommand, dest, DestRemote);
	will_be_called(EndCommand);

	will_be_called(pq_flush);
}

void
mockIsRoleMirror(bool expectedRoleMirror)
{
	am_mirror = expectedRoleMirror;
}

void
test_HandleFtsWalRepProbePrimary(void **state)
{
	FtsResponse mockresponse;
	mockresponse.IsMirrorUp = true;
	mockresponse.IsInSync = true;
	mockresponse.IsSyncRepEnabled = false;
	mockresponse.IsRoleMirror = false;
	mockresponse.RequestRetry = false;

	expect_any(GetMirrorStatus, response);
	will_assign_memory(GetMirrorStatus, response, &mockresponse, sizeof(FtsResponse));
	will_be_called(GetMirrorStatus);

	will_be_called(SetSyncStandbysDefined);

	mockIsRoleMirror(mockresponse.IsRoleMirror);

	/* SyncRep should be enabled as soon as we found mirror is up. */
	mockresponse.IsSyncRepEnabled = true;
	expectSendFtsResponse(FTS_MSG_PROBE, &mockresponse);

	HandleFtsWalRepProbe();
}

void
test_HandleFtsWalRepSyncRepOff(void **state)
{
	FtsResponse mockresponse;
	mockresponse.IsMirrorUp = false;
	mockresponse.IsInSync = false;
	mockresponse.RequestRetry = false;
	/* unblock primary if FTS requests it */
	mockresponse.IsSyncRepEnabled = false;

	expect_any(GetMirrorStatus, response);
	will_assign_memory(GetMirrorStatus, response, &mockresponse, sizeof(FtsResponse));
	will_be_called(GetMirrorStatus);

	will_be_called(UnsetSyncStandbysDefined);

	/* since this function doesn't have any logic, the test just verified the message type */
	expectSendFtsResponse(FTS_MSG_SYNCREP_OFF, &mockresponse);
	
	HandleFtsWalRepSyncRepOff();
}

void
test_HandleFtsWalRepProbeMirror(void **state)
{
  FtsResponse mockresponse;
  mockresponse.IsMirrorUp = false;
  mockresponse.IsInSync = false;
  mockresponse.IsSyncRepEnabled = false;
  mockresponse.IsRoleMirror = true;

  /* TODO: figure out scenario of probing a mirror */
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_HandleFtsWalRepProbePrimary),
		unit_test(test_HandleFtsWalRepSyncRepOff),
		unit_test(test_HandleFtsWalRepProbeMirror)
	};
	return run_tests(tests);
}
