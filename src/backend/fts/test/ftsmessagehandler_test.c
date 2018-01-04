#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"

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

	expect_any_count(pq_sendint, buf, -1);
	expect_any_count(pq_sendint, b, -1);

	/* verify the schema */
	expect_value(pq_sendint, i, Natts_fts_message_response);
	expect_any_count(pq_sendint, i, Natts_fts_message_response * 6 /* calls_per_column_for_schema */);

	/* verify the data */
	expect_value(pq_sendint, i, Natts_fts_message_response);
	expect_value(pq_sendint, i, 1);
  	expect_value(pq_sendint, i, expectedResponse->IsMirrorUp);
	expect_value(pq_sendint, i, 1);
	expect_value(pq_sendint, i, expectedResponse->IsInSync);
	expect_value(pq_sendint, i, 1);
	expect_value(pq_sendint, i, expectedResponse->IsSyncRepEnabled);
	expect_value(pq_sendint, i, 1);
	expect_value(pq_sendint, i, expectedResponse->IsRoleMirror);
	expect_value(pq_sendint, i, 1);
	expect_value(pq_sendint, i, expectedResponse->RequestRetry);

	will_be_called_count(pq_sendint, -1);

	expect_any_count(pq_sendstring, buf, -1);
	expect_any_count(pq_sendstring, str, -1);
	will_be_called_count(pq_sendstring, Natts_fts_message_response);

	expect_value(EndCommand, commandTag, expectedMessageType);
	expect_value(EndCommand, dest, DestRemote);
	will_be_called(EndCommand);

	will_be_called(pq_flush);
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
	mockresponse.RequestRetry = false;

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
	mockresponse.IsMirrorUp       = false;
	mockresponse.IsInSync         = false;
	mockresponse.IsSyncRepEnabled = false;
	mockresponse.IsRoleMirror     = false;
	mockresponse.RequestRetry     = false;

	expect_any(GetMirrorStatus, response);
	will_assign_memory(GetMirrorStatus, response, &mockresponse, sizeof(FtsResponse));
	will_be_called(GetMirrorStatus);

	/* expect the IsRoleMirror changed to reflect the global variable */
	am_mirror = true;
	mockresponse.IsRoleMirror = true;
	expectSendFtsResponse(FTS_MSG_PROBE, &mockresponse);

	HandleFtsWalRepProbe();
}

void
test_HandleFtsWalRepPromoteMirror(void **state)
{
	am_mirror = true;

	will_return(GetCurrentDBState, DB_IN_STANDBY_MODE);
	will_be_called(SignalPromote);

	FtsResponse mockresponse;
	mockresponse.IsMirrorUp       = false;
	mockresponse.IsInSync         = false;
	mockresponse.IsSyncRepEnabled = false;
	mockresponse.IsRoleMirror     = am_mirror;
	mockresponse.RequestRetry     = false;

	/* expect SignalPromote() */
	expectSendFtsResponse(FTS_MSG_PROMOTE, &mockresponse);

	HandleFtsWalRepPromote();
}

void
test_HandleFtsWalRepPromotePrimary(void **state)
{
	am_mirror = false;

	will_return(GetCurrentDBState, DB_IN_PRODUCTION);

	FtsResponse mockresponse;
	mockresponse.IsMirrorUp       = false;
	mockresponse.IsInSync         = false;
	mockresponse.IsSyncRepEnabled = false;
	mockresponse.IsRoleMirror     = false;
	mockresponse.RequestRetry     = false;

	/* expect no SignalPromote() */
	expectSendFtsResponse(FTS_MSG_PROMOTE, &mockresponse);

	HandleFtsWalRepPromote();
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_HandleFtsWalRepProbePrimary),
		unit_test(test_HandleFtsWalRepSyncRepOff),
		unit_test(test_HandleFtsWalRepProbeMirror),
		unit_test(test_HandleFtsWalRepPromoteMirror),
		unit_test(test_HandleFtsWalRepPromotePrimary)
	};
	return run_tests(tests);
}
