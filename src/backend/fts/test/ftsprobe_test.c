#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include <poll.h>

static int poll_expected_return_value;
static int poll_expected_revents;
static const char true_value = 1;
static const char false_value = 0;

#define poll poll_mock
static struct pollfd *PollFds;
int poll_mock (struct pollfd * p1, nfds_t p2, int p3)
{
	int i;
	for (i = 0; i < poll_expected_return_value; i++)
		PollFds[i].revents = poll_expected_revents;
	return poll_expected_return_value;
}

#include "postgres.h"

/* Actual function body */
#include "../ftsprobe.c"
#include "./fts_test_helper.h"

static void poll_will_return(int expected_return_value, int revents)
{
	poll_expected_return_value = expected_return_value;
	poll_expected_revents = revents;
}

static fts_context*
init_fts_context(int num_contents, FtsProbeState state, char mode)
{
	fts_context *context;
	int i;

	CdbComponentDatabases *cdbs = InitTestCdb(num_contents, true, mode);
	context = palloc0(sizeof(fts_context));

	context->perSegInfos = palloc0(sizeof(per_segment_info) * num_contents);
	context->num_pairs = num_contents;

	int response_index = 0;
	for(i = 0; i<cdbs->total_segment_dbs; i++)
	{
		if (cdbs->segment_db_info[i].role == GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY)
		{
			per_segment_info *response = &context->perSegInfos[response_index++];
			response->primary_cdbinfo = &cdbs->segment_db_info[i];
			response->mirror_cdbinfo = FtsGetPeerSegment(
				cdbs, cdbs->segment_db_info[i].segindex, cdbs->segment_db_info[i].dbid);
			response->result.isPrimaryAlive = false;
			response->result.isMirrorAlive = SEGMENT_IS_ALIVE(response->mirror_cdbinfo);
			response->result.isInSync = false;
			response->result.isSyncRepEnabled = false;
			response->result.retryRequested = false;
			response->result.isRoleMirror = false;
			response->result.dbid = cdbs->segment_db_info[i].dbid;
			response->state = state;
		}
	}
	return context;
}

static void write_log_will_be_called()
{
	expect_any(write_log, fmt);
	will_be_called(write_log);
}

static void probeTimeout_mock_timeout(per_segment_info *info)
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

static void probePollIn_mock_success(per_segment_info *info)
{
	expect_any(PQsocket, conn);
	will_be_called(PQsocket);

	poll_will_return(1, 0);
}


static void probePollIn_mock_timeout(per_segment_info *info)
{
	expect_any(PQsocket, conn);
	will_be_called(PQsocket);

	/*
	 * Need poll() and errno to exercise probeTimeout()
	 */
	poll_will_return(0, 0);
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

static void PQgetvalue_will_return(int attnum, bool *value)
{
	expect_any(PQgetvalue, res);
	expect_value(PQgetvalue, tup_num, 0);
	expect_value(PQgetvalue, field_num, attnum);
	will_return(PQgetvalue, value);
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
	per_segment_info info;
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
	ftsReceive(&info);

}

/*
 * Scenario: if primary responds FATAL to FTS probe, ftsReceive on master
 * should fail due to PQconsumeInput() failed
 */
void
test_ftsReceive_when_fts_handler_FATAL(void **state)
{
	per_segment_info info;
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
	ftsReceive(&info);
}

/*
 * Scenario: if primary response ERROR to FTS probe, ftsReceive on master
 * should fail due to PQresultStatus(lastResult) returned PGRES_FATAL_ERROR
 */
void
test_ftsReceive_when_fts_handler_ERROR(void **state)
{
	per_segment_info info;
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
	ftsReceive(&info);
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

	PQgetvalue_will_return(Anum_fts_message_response_is_mirror_up, &true_value);
	PQgetvalue_will_return(Anum_fts_message_response_is_in_sync, &true_value);
	PQgetvalue_will_return(Anum_fts_message_response_is_syncrep_enabled, &true_value);
	PQgetvalue_will_return(Anum_fts_message_response_is_role_mirror, &true_value);
}

void
test_ftsReceive_when_primary_request_retry_true(void **state)
{
	per_segment_info info;
	struct pg_conn conn;

	info.conn = &conn;
	info.state = FTS_PROBE_SEGMENT;

	ftsReceive_request_retry_setup();
	write_log_will_be_called();
	write_log_will_be_called();
	PQgetvalue_will_return(Anum_fts_message_response_request_retry, &true_value);

	/* TEST */
	ftsReceive(&info);
}

void
test_ftsReceive_when_primary_request_retry_false(void **state)
{
	per_segment_info info;
	struct pg_conn conn;

	info.conn = &conn;
	info.state = FTS_PROBE_SEGMENT;

	ftsReceive_request_retry_setup();
	write_log_will_be_called();
	PQgetvalue_will_return(Anum_fts_message_response_request_retry, &false_value);

	/* TEST */
	ftsReceive(&info);
}

/*
 * One primary segment, connection starts successfully from initial state.
 */
void
test_ftsConnect_FTS_PROBE_SEGMENT(void **state)
{
	fts_context *context = init_fts_context(1, FTS_PROBE_SEGMENT,
											GP_SEGMENT_CONFIGURATION_MODE_INSYNC);
	char primary_conninfo[1024];
	per_segment_info *response = &context->perSegInfos[0];

	PGconn *pgconn = palloc(sizeof(PGconn));
	pgconn->status = CONNECTION_STARTED;
	pgconn->sock = 11;
	snprintf(primary_conninfo, 1024, "host=%s port=%d gpconntype=%s",
			 response->primary_cdbinfo->hostip, response->primary_cdbinfo->port,
			 GPCONN_TYPE_FTS);
	expect_string(PQconnectStart, conninfo, primary_conninfo);
	will_return(PQconnectStart, pgconn);
	ftsConnect(context);

	assert_true(response->state == FTS_PROBE_SEGMENT);
	/* Successful ftsConnect must set the socket to be polled for writing. */
	assert_true(response->poll_events & POLLOUT);
}

/*
 * Two primary segments, connection for one segment fails due to libpq
 * returning CONNECTION_BAD.  Connection for the other is in FTS_PROBE_SEGMENT
 * and advances to the next libpq state.
 */
void
test_ftsConnect_one_failure_one_success(void **state)
{
	fts_context *context = init_fts_context(2, FTS_PROBE_SEGMENT,
											GP_SEGMENT_CONFIGURATION_MODE_INSYNC);
	per_segment_info *success_resp = &context->perSegInfos[0];
	success_resp->conn = palloc(sizeof(PGconn));
	success_resp->conn->status = CONNECTION_STARTED;
	success_resp->conn->sock = 11;
	/* Assume that the successful socket is ready for writing. */
	success_resp->poll_revents = POLLOUT;
	expect_value(PQconnectPoll, conn, success_resp->conn);
	will_return(PQconnectPoll, PGRES_POLLING_READING);
	/* Ensure that PQstatus doesn't report that this connection is established. */
	expect_value(PQstatus, conn, success_resp->conn);
	will_return(PQstatus, CONNECTION_STARTED);

	per_segment_info *failure_resp = &context->perSegInfos[1];
	char primary_conninfo_failure[1024];
	snprintf(primary_conninfo_failure, 1024, "host=%s port=%d gpconntype=%s",
			 failure_resp->primary_cdbinfo->hostip,
			 failure_resp->primary_cdbinfo->port,
			 GPCONN_TYPE_FTS);
	expect_string(PQconnectStart, conninfo, primary_conninfo_failure);
	PGconn *failure_pgconn = palloc(sizeof(PGconn));
	failure_pgconn->status = CONNECTION_BAD;
	will_return(PQconnectStart, failure_pgconn);
	expect_value(PQerrorMessage, conn, failure_pgconn);
	will_be_called(PQerrorMessage);

	ftsConnect(context);

	assert_true(success_resp->state == FTS_PROBE_SEGMENT);
	/*
	 * Successful segment's socket must be set to be polled for reading because
	 * we simulated PQconnectPoll() to return PGRES_POLLING_READING.
	 */
	assert_true(success_resp->poll_events &	POLLIN);
	assert_true(failure_resp->state == FTS_PROBE_FAILED);
}

/*
 * Starting with one content (primary-mirrror pair) in FTS_PROBE_SEGMENT, test
 * ftsConnect() followed by ftsPoll().
 */
void
test_ftsConnect_ftsPoll(void **state)
{
	fts_context *context = init_fts_context(1, FTS_PROBE_SEGMENT,
											GP_SEGMENT_CONFIGURATION_MODE_INSYNC);
	initPollFds(1);
	char primary_conninfo[1024];
	per_segment_info *response = &context->perSegInfos[0];

	PGconn *pgconn = palloc(sizeof(PGconn));
	pgconn->status = CONNECTION_STARTED;
	pgconn->sock = 11;
	snprintf(primary_conninfo, 1024, "host=%s port=%d gpconntype=%s",
			 response->primary_cdbinfo->hostip, response->primary_cdbinfo->port,
			 GPCONN_TYPE_FTS);
	expect_string(PQconnectStart, conninfo, primary_conninfo);
	will_return(PQconnectStart, pgconn);

	ftsConnect(context);

	assert_true(response->state == FTS_PROBE_SEGMENT);
	/* Successful ftsConnect must set the socket to be polled for writing. */
	assert_true(response->poll_events & POLLOUT);

	expect_value(PQsocket, conn, response->conn);
	will_return(PQsocket, response->conn->sock);

#ifdef USE_ASSERT_CHECKING
	expect_value(PQsocket, conn, response->conn);
	will_return(PQsocket, response->conn->sock);
#endif

	/*
	 * Simulate poll() returns write-ready for the only descriptor in
	 * fts_context.
	 */
	poll_will_return(1, POLLOUT);

	ftsPoll(context);

	assert_true(response->poll_revents & POLLOUT);
	assert_true(response->poll_events == 0);
}

/*
 * 1 primary-mirror pair, send successful
 */
void
test_ftsSend_basic(void **state)
{
	fts_context *context = init_fts_context(1, FTS_PROBE_SEGMENT,
											GP_SEGMENT_CONFIGURATION_MODE_INSYNC);
	per_segment_info *response = &context->perSegInfos[0];
	response->state = FTS_PROBE_SEGMENT;
	response->conn = palloc(sizeof(PGconn));
	response->conn->asyncStatus = PGASYNC_IDLE;
	response->poll_revents = POLLOUT;
	expect_value(PQstatus, conn, response->conn);
	will_return(PQstatus, CONNECTION_OK);
	expect_value(PQsendQuery, conn, response->conn);
	expect_string(PQsendQuery, query, FTS_MSG_PROBE);
	will_return(PQsendQuery, 1);

	ftsSend(context);

	assert_true(response->poll_events & POLLIN);
}

/*
 * Receive a response to probe message from one primary segment.
 */
void
test_ftsReceive_basic(void **state)
{
	fts_context *context = init_fts_context(1, FTS_PROBE_SEGMENT,
											GP_SEGMENT_CONFIGURATION_MODE_INSYNC);
	static int true_value = 1;
	static int false_value = 0;

	per_segment_info *response = &context->perSegInfos[0];
	response->state = FTS_PROBE_SEGMENT;
	response->conn = palloc(sizeof(PGconn));
	response->conn->status = CONNECTION_OK;
	/* Simulate the case that data has arrived on this socket. */
	response->poll_revents = POLLIN;

	/* PQstatus is called twice. */
	expect_value(PQstatus, conn, response->conn);
	will_return(PQstatus, CONNECTION_OK);
	expect_value(PQstatus, conn, response->conn);
	will_return(PQstatus, CONNECTION_OK);

	/* Expect async libpq interface to receive is called */
	expect_value(PQconsumeInput, conn, response->conn);
	will_return(PQconsumeInput, 1);
	expect_value(PQisBusy, conn, response->conn);
	will_return(PQisBusy, 0);
	response->conn->result = palloc(sizeof(PGresult));
	
	expect_value(PQgetResult, conn, response->conn);
	will_return(PQgetResult, response->conn->result);

	expect_value(PQresultStatus, res, response->conn->result);
	will_return(PQresultStatus, PGRES_TUPLES_OK);

	expect_value(PQntuples, res, response->conn->result);
	will_return(PQntuples, FTS_MESSAGE_RESPONSE_NTUPLES);

	expect_value(PQnfields, res, response->conn->result);
	will_return(PQnfields, Natts_fts_message_response);

	expect_value(PQgetvalue, res, response->conn->result);
	expect_value(PQgetvalue, tup_num, 0);
	expect_value(PQgetvalue, field_num, Anum_fts_message_response_is_mirror_up);
	will_return(PQgetvalue, &true_value);
	expect_value(PQgetvalue, res, response->conn->result);
	expect_value(PQgetvalue, tup_num, 0);
	expect_value(PQgetvalue, field_num, Anum_fts_message_response_is_in_sync);
	will_return(PQgetvalue, &true_value);
	expect_value(PQgetvalue, res, response->conn->result);
	expect_value(PQgetvalue, tup_num, 0);
	expect_value(PQgetvalue, field_num, Anum_fts_message_response_is_syncrep_enabled);
	will_return(PQgetvalue, &true_value);
	expect_value(PQgetvalue, res, response->conn->result);
	expect_value(PQgetvalue, tup_num, 0);
	expect_value(PQgetvalue, field_num, Anum_fts_message_response_is_role_mirror);
	will_return(PQgetvalue, &false_value);
	expect_value(PQgetvalue, res, response->conn->result);
	expect_value(PQgetvalue, tup_num, 0);
	expect_value(PQgetvalue, field_num, Anum_fts_message_response_request_retry);
	will_return(PQgetvalue, &false_value);

	ftsReceive(context);

	assert_true(response->result.isPrimaryAlive);
	assert_true(response->result.isMirrorAlive);
	assert_false(response->result.retryRequested);
	/*
	 * No further polling on this socket, until it's time to send the next
	 * message.
	 */
	assert_true(response->poll_events == 0);
}

/*
 * 2 primary-mirror pairs: one got a "request retry" response from primary,
 * syncrep_off message failed to get a response from the other primary.
 * Another attempt must be made in both cases after waiting for 1 second.
 */
void
test_processRetry_wait_before_retry(void **state)
{
	/* Start with a failure state and retry_count = 0. */
	fts_context *context = init_fts_context(2, FTS_SYNCREP_FAILED,
											GP_SEGMENT_CONFIGURATION_MODE_INSYNC);
	per_segment_info *response1 = &context->perSegInfos[0];
	per_segment_info *response2 = &context->perSegInfos[1];
	PGconn pgconn1, pgconn2;
	response1->conn = &pgconn1;
	response1->state = FTS_PROBE_SUCCESS;
	response1->result.isPrimaryAlive = true;
	response1->result.retryRequested = true;

	response2->conn = &pgconn2;

	expect_any_count(PQfinish, conn, 2);
	will_be_called_count(PQfinish, 2);

	processRetry(context);

	/* We must wait in a retry_wait state with retryStartTime set. */
	assert_true(response1->state == FTS_PROBE_RETRY_WAIT);
	assert_true(response2->state == FTS_SYNCREP_RETRY_WAIT);
	assert_true(response1->retry_count == 1);
	assert_true(response1->poll_events == 0);
	assert_true(response1->poll_revents == 0);
	pg_time_t retryStartTime1 = response1->retryStartTime;
	assert_true( retryStartTime1 > 0);
	assert_true(response2->retry_count == 1);
	assert_true(response2->poll_events == 0);
	assert_true(response2->poll_revents == 0);
	pg_time_t retryStartTime2 = response2->retryStartTime;
	assert_true(retryStartTime2 > 0);

	/*
	 * We must continue to wait because 1 second hasn't elapsed since the
	 * failure.
	 */
	processRetry(context);

	assert_true(response1->state == FTS_PROBE_RETRY_WAIT);
	assert_true(response2->state == FTS_SYNCREP_RETRY_WAIT);

	/*
	 * Adjust retryStartTime to 1 second in past so that next processRetry()
	 * should make a retry attempt.
	 */
	response1->retryStartTime = retryStartTime1 - 1;
	response2->retryStartTime = retryStartTime2 - 1;

	processRetry(context);

	/* This time, we must be ready to make another retry. */
	assert_true(response1->state == FTS_PROBE_SEGMENT);
	assert_true(response2->state == FTS_SYNCREP_SEGMENT);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_ftsReceive_basic),
		unit_test(test_ftsSend_basic),
		unit_test(test_ftsConnect_ftsPoll),
		unit_test(test_ftsConnect_FTS_PROBE_SEGMENT),
		unit_test(test_ftsConnect_one_failure_one_success),
		unit_test(test_processRetry_wait_before_retry)
	};
	MemoryContextInit();
	return run_tests(tests);
}
