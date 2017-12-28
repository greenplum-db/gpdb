#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include <poll.h>

static int poll_expected_return_value;

#define poll poll_mock

int poll_mock (struct pollfd * p1, nfds_t p2, int p3)
{
	return poll_expected_return_value;
}

static void poll_will_return(int expected_return_value)
{
	poll_expected_return_value = expected_return_value;
}

#include "postgres.h"

/* Actual function body */
#include "../ftsprobe.c"

static void write_log_will_be_called()
{
	expect_any(write_log, fmt);
	will_be_called(write_log);
}

static void probeTimeout_mock_timeout(FtsConnectionInfo *info)
{
	/*
	 * set info->startTime to set proper elapse_ms
	 * (which is returned by inline function gp_seg_elapsed_ms()) to simulate
	 * timeout
	 */
	gp_fts_probe_timeout = 0;
	gp_set_monotonic_begin_time(&(info->startTime));
	pg_usleep(10); /* sleep to mock timeout */
	write_log_will_be_called();
}

static void probePollIn_mock_success(FtsConnectionInfo *info)
{
	expect_any(PQsocket, conn);
	will_be_called(PQsocket);

	poll_will_return(1);
}


static void probePollIn_mock_timeout(FtsConnectionInfo *info)
{
	expect_any(PQsocket, conn);
	will_be_called(PQsocket);

	/*
	 * Need poll() and errno to exercise probeTimeout()
	 */
	poll_will_return(0);
	probeTimeout_mock_timeout(info);

	write_log_will_be_called();
}

static void PQisBusy_will_return(bool expected_return_value)
{
	expect_any(PQisBusy, conn);
	will_return(PQisBusy, expected_return_value);
}

static void PQconsumeInput_will_return(int expected_return_value)
{
	expect_any(PQconsumeInput, conn);
	will_return(PQconsumeInput, expected_return_value);
}

static void PQerrorMessage_will_be_called()
{
	expect_any(PQerrorMessage, conn);
	will_be_called(PQerrorMessage);
}

static void PQgetResult_will_return(PGresult *expected_return_value)
{
	expect_any(PQgetResult, conn);
	will_return(PQgetResult, expected_return_value);
}

static void PQresultStatus_will_return(ExecStatusType expected_return_value)
{
	expect_any(PQresultStatus, res);
	will_return(PQresultStatus, expected_return_value);
}

static void PQclear_will_be_called()
{
	expect_any(PQclear, res);
	will_be_called(PQclear);
}

static void PQgetvalue_will_return(int attnum, bool value)
{
	expect_any(PQgetvalue, res);
	expect_value(PQgetvalue, tup_num, 0);
	expect_value(PQgetvalue, field_num, attnum);
	will_return(PQgetvalue, &value);
}

/*
 * Scenario: if primary didn't respond in time to FTS probe, ftsReceive on
 * master should fail.
 *
 * We are mocking the timeout behavior using probeTimeout().
 */
void
test_ftsReceive_when_fts_handler_hung(void **state)
{
	FtsConnectionInfo info;
	struct pg_conn conn;

	info.conn = &conn;

	PQisBusy_will_return(true);

	/*
	 * probePollIn() will mock timeout by mocked version of probeTimeout()
	 */
	probePollIn_mock_timeout(&info);

	/*
	 * TEST
	 */
	bool actual_return_value = ftsReceive(&info);

	assert_false(actual_return_value);
}

/*
 * Scenario: if primary responds FATAL to FTS probe, ftsReceive on master
 * should fail due to PQconsumeInput() failed
 */
void
test_ftsReceive_when_fts_handler_FATAL(void **state)
{
	FtsConnectionInfo info;
	struct pg_conn conn;

	info.conn = &conn;

	PQisBusy_will_return(true);

	probePollIn_mock_success(&info);
	PQconsumeInput_will_return(0);

	write_log_will_be_called();
	PQerrorMessage_will_be_called();

	/*
	 * TEST
	 */
	bool actual_return_value = ftsReceive(&info);

	assert_false(actual_return_value);
}

/*
 * Scenario: if primary response ERROR to FTS probe, ftsReceive on master
 * should fail due to PQresultStatus(lastResult) returned PGRES_FATAL_ERROR
 */
void
test_ftsReceive_when_fts_handler_ERROR(void **state)
{
	FtsConnectionInfo info;
	struct pg_conn conn;

	info.conn = &conn;

	PQisBusy_will_return(false);

	/*
	 * mock tmpResult to NULL, so that we break the for loop.
	 */
	PQgetResult_will_return(NULL);

	PQresultStatus_will_return(PGRES_FATAL_ERROR);
	PQclear_will_be_called();

	write_log_will_be_called();
	PQerrorMessage_will_be_called();

	/*
	 * TEST
	 */
	bool actual_return_value = ftsReceive(&info);

	assert_false(actual_return_value);
}

static void ftsReceive_request_retry_setup()
{
	PQisBusy_will_return(false);

	/*
	 * mock tmpResult to NULL, so that we break the for loop.
	 */
	PQgetResult_will_return(NULL);
	PQresultStatus_will_return(PGRES_TUPLES_OK);

	expect_any(PQnfields, res);
	will_return(PQnfields, Natts_fts_message_response);
	expect_any(PQntuples, res);
	will_return(PQntuples, FTS_MESSAGE_RESPONSE_NTUPLES);

	PQgetvalue_will_return(Anum_fts_message_response_is_mirror_up, true);
	PQgetvalue_will_return(Anum_fts_message_response_is_in_sync, true);
	PQgetvalue_will_return(Anum_fts_message_response_is_syncrep_enabled, true);
}

void
test_ftsReceive_when_primary_request_retry_true(void **state)
{
	FtsConnectionInfo info;
	struct pg_conn conn;
	probe_result result;
	char str[50] = FTS_MSG_PROBE;

	info.conn = &conn;
	info.result = &result;
	info.message = &str;

	ftsReceive_request_retry_setup();
	write_log_will_be_called();
	write_log_will_be_called();
	PQgetvalue_will_return(Anum_fts_message_response_request_retry, true);

	/* TEST */
	bool actual_return_value = ftsReceive(&info);
	assert_false(actual_return_value);
}

void
test_ftsReceive_when_primary_request_retry_false(void **state)
{
	FtsConnectionInfo info;
	struct pg_conn conn;
	probe_result result;
	char str[50] = FTS_MSG_PROBE;

	info.conn = &conn;
	info.result = &result;
	info.message = &str;

	ftsReceive_request_retry_setup();
	write_log_will_be_called();
	PQgetvalue_will_return(Anum_fts_message_response_request_retry, false);

	/* TEST */
	bool actual_return_value = ftsReceive(&info);
	assert_true(actual_return_value);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_ftsReceive_when_fts_handler_hung),
		unit_test(test_ftsReceive_when_fts_handler_FATAL),
		unit_test(test_ftsReceive_when_fts_handler_ERROR),
		unit_test(test_ftsReceive_when_primary_request_retry_true),
		unit_test(test_ftsReceive_when_primary_request_retry_false)
	};
	return run_tests(tests);
}
