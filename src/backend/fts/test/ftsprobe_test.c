#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "utils/memutils.h"

#include <poll.h>

static int poll_expected_return_value;
static int poll_expected_revents;

#define poll poll_mock
static struct pollfd *PollFds;

static int
poll_mock (struct pollfd * p1, nfds_t p2, int p3)
{
	int i;
	for (i = 0; i < poll_expected_return_value; i++)
		PollFds[i].revents = poll_expected_revents;
	return poll_expected_return_value;
}

#include "postgres.h"

/* Actual function body */
#include "../ftsprobe.c"

static void
InitFtsProbeInfo(void)
{
	static FtsProbeInfo fts_info;
	ftsProbeInfo = &fts_info;
}

static void poll_will_return(int expected_return_value, int revents)
{
	poll_expected_return_value = expected_return_value;
	poll_expected_revents = revents;
}

/*
 * Function to help create representation of gp_segment_configuration, for
 * ease of testing different scenarios. Using the same can mock out different
 * configuration layouts.
 *
 * --------------------------------
 * Inputs:
 *    segCnt - number of primary segments the configuration should mock
 *    has_mirrors - controls if mirrors corresponding to primary are created
 * --------------------------------
 *
 * Function always adds master to the configuration. Also, all the segments
 * are by default marked up. Tests leverage to create initial configuration
 * using this and then modify the same as per needs to mock different
 * scenarios like mirror down, primary down, etc...
 */
static CdbComponentDatabases *
InitTestCdb(int segCnt, bool has_mirrors, char default_mode)
{
	int			i = 0;
	int			mirror_multiplier = 1;

	if (has_mirrors)
		mirror_multiplier = 2;

	CdbComponentDatabases *cdb =
	(CdbComponentDatabases *) palloc(sizeof(CdbComponentDatabases));

	cdb->total_entry_dbs = 1;
	cdb->total_segment_dbs = segCnt * mirror_multiplier;	/* with mirror? */
	cdb->total_segments = segCnt;
	cdb->entry_db_info = palloc(
		sizeof(CdbComponentDatabaseInfo) * cdb->total_entry_dbs);
	cdb->segment_db_info = palloc(
		sizeof(CdbComponentDatabaseInfo) * cdb->total_segment_dbs);


	/* create the master entry_db_info */
	CdbComponentDatabaseInfo *cdbinfo = &cdb->entry_db_info[0];

	cdbinfo->config = (GpSegConfigEntry*)palloc(sizeof(GpSegConfigEntry));
	cdbinfo->config->dbid = 1;
	cdbinfo->config->segindex = -1;

	cdbinfo->config->role = GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;
	cdbinfo->config->preferred_role = GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;
	cdbinfo->config->status = GP_SEGMENT_CONFIGURATION_STATUS_UP;

	/* create the segment_db_info entries */
	for (i = 0; i < cdb->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *cdbinfo = &cdb->segment_db_info[i];
		cdbinfo->config = (GpSegConfigEntry*)palloc(sizeof(GpSegConfigEntry));

		cdbinfo->config->dbid = i + 2;
		cdbinfo->config->segindex = i / 2;
		cdbinfo->config->hostip = palloc(4);
		snprintf(cdbinfo->config->hostip, 4, "%d", cdbinfo->config->dbid);
		cdbinfo->config->port = cdbinfo->config->dbid;

		if (has_mirrors)
		{
			cdbinfo->config->role = i % 2 ?
				GP_SEGMENT_CONFIGURATION_ROLE_MIRROR :
				GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;

			cdbinfo->config->preferred_role = i % 2 ?
				GP_SEGMENT_CONFIGURATION_ROLE_MIRROR :
				GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;
		}
		else
		{
			cdbinfo->config->role = GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;
			cdbinfo->config->preferred_role = GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;
		}
		cdbinfo->config->status = GP_SEGMENT_CONFIGURATION_STATUS_UP;
		cdbinfo->config->mode = default_mode;
	}

	return cdb;
}

/* Initialize connection and startTime for each primary-mirror pair. */
static void
init_fts_context(fts_context *context, FtsMessageState state)
{
	pg_time_t now = (pg_time_t) time(NULL);
	int i;
	for(i = 0; i < context->num_pairs; i++)
	{
		context->perSegInfos[i].state = state;
		context->perSegInfos[i].startTime = now;
		context->perSegInfos[i].conn = (PGconn *) palloc0(sizeof(PGconn));
	}
}

static CdbComponentDatabaseInfo *
GetSegmentFromCdbComponentDatabases(CdbComponentDatabases *dbs,
									int16 segindex, char role)
{
	int			i;
	for (i = 0; i < dbs->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *cdb = &dbs->segment_db_info[i];
		if (cdb->config->segindex == segindex && cdb->config->role == role)
			return cdb;
	}
	return NULL;
}

static void
ExpectedPrimaryAndMirrorConfiguration(CdbComponentDatabaseInfo *primary,
									  CdbComponentDatabaseInfo *mirror,
									  char primaryStatus,
									  char mirrorStatus,
									  char mode,
									  char newPrimaryRole,
									  char newMirrorRole,
									  bool willUpdatePrimary,
									  bool willUpdateMirror)
{
	/* mock probeWalRepUpdateConfig */
	if (willUpdatePrimary)
	{
		expect_value(probeWalRepUpdateConfig, dbid, primary->config->dbid);
		expect_value(probeWalRepUpdateConfig, segindex, primary->config->segindex);
		expect_value(probeWalRepUpdateConfig, role, newPrimaryRole);
		expect_value(probeWalRepUpdateConfig, IsSegmentAlive,
					 primaryStatus == GP_SEGMENT_CONFIGURATION_STATUS_UP ? true : false);
		expect_value(probeWalRepUpdateConfig, IsInSync,
					 mode == GP_SEGMENT_CONFIGURATION_MODE_INSYNC ? true : false);
		will_be_called(probeWalRepUpdateConfig);
	}

	if (willUpdateMirror)
	{
		expect_value(probeWalRepUpdateConfig, dbid, mirror->config->dbid);
		expect_value(probeWalRepUpdateConfig, segindex, mirror->config->segindex);
		expect_value(probeWalRepUpdateConfig, role, newMirrorRole);
		expect_value(probeWalRepUpdateConfig, IsSegmentAlive,
					 mirrorStatus == GP_SEGMENT_CONFIGURATION_STATUS_UP ? true : false);
		expect_value(probeWalRepUpdateConfig, IsInSync,
					 mode == GP_SEGMENT_CONFIGURATION_MODE_INSYNC ? true : false);
		will_be_called(probeWalRepUpdateConfig);
	}
}

static void
PrimaryOrMirrorWillBeUpdated(int count)
{
	will_be_called_count(StartTransactionCommand, count);
	will_be_called_count(GetTransactionSnapshot, count);
	will_be_called_count(CommitTransactionCommand, count);
	will_be_called_count(ftsLock, count);
	will_be_called_count(ftsUnlock, count);
}

/*
 * One primary segment, connection starts successfully from initial state.
 */
static void
test_ftsConnect_FTS_PROBE_SEGMENT(void **state)
{
	CdbComponentDatabases *cdbs = InitTestCdb(
		1, true, GP_SEGMENT_CONFIGURATION_MODE_INSYNC);
	fts_context context;
	FtsWalRepInitProbeContext(cdbs, &context);
	char primary_conninfo[1024];
	fts_segment_info *ftsInfo = &context.perSegInfos[0];

	ftsInfo->conn = NULL;
	ftsInfo->startTime = 0;

	PGconn *pgconn = palloc(sizeof(PGconn));
	pgconn->status = CONNECTION_STARTED;
	pgconn->sock = 11;
	snprintf(primary_conninfo, 1024, "host=%s port=%d gpconntype=%s",
			 ftsInfo->primary_cdbinfo->config->hostip, ftsInfo->primary_cdbinfo->config->port,
			 GPCONN_TYPE_FTS);
	expect_string(PQconnectStart, conninfo, primary_conninfo);
	will_return(PQconnectStart, pgconn);
	ftsConnect(&context);

	assert_true(ftsInfo->state == FTS_PROBE_SEGMENT);
	/* Successful ftsConnect must set the socket to be polled for writing. */
	assert_true(ftsInfo->poll_events & POLLOUT);
	/* Successful connections must have their startTime recorded. */
	assert_true(ftsInfo->startTime > 0);
}

/*
 * Two primary segments, connection for one segment fails due to libpq
 * returning CONNECTION_BAD.  Connection for the other is in FTS_PROBE_SEGMENT
 * and advances to the next libpq state.
 */
static void
test_ftsConnect_one_failure_one_success(void **state)
{
	CdbComponentDatabases *cdbs = InitTestCdb(
		2, true, GP_SEGMENT_CONFIGURATION_MODE_INSYNC);
	fts_context context;
	FtsWalRepInitProbeContext(cdbs, &context);
	init_fts_context(&context, FTS_PROBE_SEGMENT);
	fts_segment_info *success_resp = &context.perSegInfos[0];
	success_resp->conn->status = CONNECTION_STARTED;
	success_resp->conn->sock = 11;
	/* Assume that the successful socket is ready for writing. */
	success_resp->poll_revents = POLLOUT;
	expect_value(PQconnectPoll, conn, success_resp->conn);
	will_return(PQconnectPoll, PGRES_POLLING_READING);
	/* Ensure that PQstatus doesn't report that this connection is established. */
	expect_value(PQstatus, conn, success_resp->conn);
	will_return(PQstatus, CONNECTION_STARTED);

	fts_segment_info *failure_resp = &context.perSegInfos[1];
	pfree(failure_resp->conn);
	failure_resp->conn = NULL;
	char primary_conninfo_failure[1024];
	snprintf(primary_conninfo_failure, 1024, "host=%s port=%d gpconntype=%s",
			 failure_resp->primary_cdbinfo->config->hostip,
			 failure_resp->primary_cdbinfo->config->port,
			 GPCONN_TYPE_FTS);
	expect_string(PQconnectStart, conninfo, primary_conninfo_failure);
	PGconn *failure_pgconn = palloc(sizeof(PGconn));
	failure_pgconn->status = CONNECTION_BAD;
	will_return(PQconnectStart, failure_pgconn);

	expect_value(PQerrorMessage, conn, failure_pgconn);
	will_return(PQerrorMessage, "");

	ftsConnect(&context);

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
static void
test_ftsConnect_ftsPoll(void **state)
{
	CdbComponentDatabases *cdbs = InitTestCdb(
		1, true, GP_SEGMENT_CONFIGURATION_MODE_INSYNC);
	fts_context context;
	FtsWalRepInitProbeContext(cdbs, &context);
	context.perSegInfos[0].state = FTS_PROBE_SEGMENT;

	InitPollFds(1);
	char primary_conninfo[1024];
	fts_segment_info *ftsInfo = &context.perSegInfos[0];

	PGconn *pgconn = palloc(sizeof(PGconn));
	pgconn->status = CONNECTION_STARTED;
	pgconn->sock = 11;
	snprintf(primary_conninfo, 1024, "host=%s port=%d gpconntype=%s",
			 ftsInfo->primary_cdbinfo->config->hostip, ftsInfo->primary_cdbinfo->config->port,
			 GPCONN_TYPE_FTS);
	expect_string(PQconnectStart, conninfo, primary_conninfo);
	will_return(PQconnectStart, pgconn);

	ftsConnect(&context);

	assert_true(ftsInfo->state == FTS_PROBE_SEGMENT);
	assert_true(ftsInfo->startTime > 0);
	/* Successful ftsConnect must set the socket to be polled for writing. */
	assert_true(ftsInfo->poll_events & POLLOUT);

	expect_value(PQsocket, conn, ftsInfo->conn);
	will_return(PQsocket, ftsInfo->conn->sock);

#ifdef USE_ASSERT_CHECKING
	expect_value(PQsocket, conn, ftsInfo->conn);
	will_return(PQsocket, ftsInfo->conn->sock);
#endif

	/*
	 * Simulate poll() returns write-ready for the only descriptor in
	 * fts_context.
	 */
	poll_will_return(1, POLLOUT);

	ftsPoll(&context);

	assert_true(ftsInfo->poll_revents & POLLOUT);
	assert_true(ftsInfo->poll_events == 0);
}

/*
 * 1 primary-mirror pair, send successful
 */
static void
test_ftsSend_success(void **state)
{
	char message[FTS_MSG_MAX_LEN];
	CdbComponentDatabases *cdbs = InitTestCdb(
		1, true, GP_SEGMENT_CONFIGURATION_MODE_INSYNC);
	fts_context context;
	FtsWalRepInitProbeContext(cdbs, &context);
	init_fts_context(&context, FTS_PROBE_SEGMENT);
	fts_segment_info *ftsInfo = &context.perSegInfos[0];
	ftsInfo->conn->asyncStatus = PGASYNC_IDLE;
	ftsInfo->poll_revents = POLLOUT;

	snprintf(message, FTS_MSG_MAX_LEN, FTS_MSG_FORMAT,
			 FTS_MSG_PROBE,
			 ftsInfo->primary_cdbinfo->config->dbid,
			 ftsInfo->primary_cdbinfo->config->segindex);

	expect_value(PQstatus, conn, ftsInfo->conn);
	will_return(PQstatus, CONNECTION_OK);
	expect_value(PQsendQuery, conn, ftsInfo->conn);
	expect_string(PQsendQuery, query, message);
	will_return(PQsendQuery, 1);

	ftsSend(&context);

	assert_true(ftsInfo->poll_events & POLLIN);
}

/*
 * Receive a response to probe message from one primary segment.
 */
static void
test_ftsReceive_success(void **state)
{
	CdbComponentDatabases *cdbs = InitTestCdb(
		1, true, GP_SEGMENT_CONFIGURATION_MODE_INSYNC);
	fts_context context;
	FtsWalRepInitProbeContext(cdbs, &context);
	init_fts_context(&context, FTS_PROBE_SEGMENT);

	static int true_value = 1;
	static int false_value = 0;

	fts_segment_info *ftsInfo = &context.perSegInfos[0];
	ftsInfo->state = FTS_PROBE_SEGMENT;
	ftsInfo->conn = palloc(sizeof(PGconn));
	ftsInfo->conn->status = CONNECTION_OK;
	/* Simulate the case that data has arrived on this socket. */
	ftsInfo->poll_revents = POLLIN;

	/* PQstatus is called twice. */
	expect_value(PQstatus, conn, ftsInfo->conn);
	will_return(PQstatus, CONNECTION_OK);
	expect_value(PQstatus, conn, ftsInfo->conn);
	will_return(PQstatus, CONNECTION_OK);

	/* Expect async libpq interface to receive is called */
	expect_value(PQconsumeInput, conn, ftsInfo->conn);
	will_return(PQconsumeInput, 1);
	expect_value(PQisBusy, conn, ftsInfo->conn);
	will_return(PQisBusy, 0);
	ftsInfo->conn->result = palloc(sizeof(PGresult));

	expect_value(PQgetResult, conn, ftsInfo->conn);
	will_return(PQgetResult, ftsInfo->conn->result);

	expect_value(PQresultStatus, res, ftsInfo->conn->result);
	will_return(PQresultStatus, PGRES_TUPLES_OK);

	expect_value(PQntuples, res, ftsInfo->conn->result);
	will_return(PQntuples, FTS_MESSAGE_RESPONSE_NTUPLES);

	expect_value(PQnfields, res, ftsInfo->conn->result);
	will_return(PQnfields, Natts_fts_message_response);

	expect_value(PQgetvalue, res, ftsInfo->conn->result);
	expect_value(PQgetvalue, tup_num, 0);
	expect_value(PQgetvalue, field_num, Anum_fts_message_response_is_mirror_up);
	will_return(PQgetvalue, &true_value);
	expect_value(PQgetvalue, res, ftsInfo->conn->result);
	expect_value(PQgetvalue, tup_num, 0);
	expect_value(PQgetvalue, field_num, Anum_fts_message_response_is_in_sync);
	will_return(PQgetvalue, &true_value);
	expect_value(PQgetvalue, res, ftsInfo->conn->result);
	expect_value(PQgetvalue, tup_num, 0);
	expect_value(PQgetvalue, field_num, Anum_fts_message_response_is_syncrep_enabled);
	will_return(PQgetvalue, &true_value);
	expect_value(PQgetvalue, res, ftsInfo->conn->result);
	expect_value(PQgetvalue, tup_num, 0);
	expect_value(PQgetvalue, field_num, Anum_fts_message_response_is_role_mirror);
	will_return(PQgetvalue, &false_value);
	expect_value(PQgetvalue, res, ftsInfo->conn->result);
	expect_value(PQgetvalue, tup_num, 0);
	expect_value(PQgetvalue, field_num, Anum_fts_message_response_request_retry);
	will_return(PQgetvalue, &false_value);

	ftsReceive(&context);

	assert_true(ftsInfo->result.isPrimaryAlive);
	assert_true(ftsInfo->result.isMirrorAlive);
	assert_false(ftsInfo->result.retryRequested);
	/*
	 * No further polling on this socket, until it's time to send the next
	 * message.
	 */
	assert_true(ftsInfo->poll_events == 0);
}


/*
 * Scenario: if primary responds FATAL to FTS probe, ftsReceive on master
 * should fail due to PQconsumeInput() failed
 */
static void
test_ftsReceive_when_fts_handler_FATAL(void **state)
{
	CdbComponentDatabases *cdbs = InitTestCdb(
		1, true, GP_SEGMENT_CONFIGURATION_MODE_INSYNC);
	fts_context context;
	FtsWalRepInitProbeContext(cdbs, &context);
	init_fts_context(&context, FTS_PROBE_SEGMENT);
	fts_segment_info *ftsInfo = &context.perSegInfos[0];

	/* Simulate that data is available for reading from the socket. */
	ftsInfo->poll_revents = POLLIN;
	expect_value(PQstatus, conn, ftsInfo->conn);
	will_return(PQstatus, CONNECTION_OK);

	expect_value(PQconsumeInput, conn, ftsInfo->conn);
	will_return(PQconsumeInput, 0);

	expect_value(PQerrorMessage, conn, ftsInfo->conn);
	will_return(PQerrorMessage, "");

	/*
	 * TEST
	 */
	ftsReceive(&context);

	assert_true(ftsInfo->state == FTS_PROBE_FAILED);
}

/*
 * Scenario: if primary response ERROR to FTS probe, ftsReceive on master
 * should fail due to PQresultStatus(lastResult) returned PGRES_FATAL_ERROR
 */
static void
test_ftsReceive_when_fts_handler_ERROR(void **state)
{
	CdbComponentDatabases *cdbs = InitTestCdb(
		1, true, GP_SEGMENT_CONFIGURATION_MODE_INSYNC);
	fts_context context;
	FtsWalRepInitProbeContext(cdbs, &context);
	/*
	 * As long as it is one of the states in which an FTS message can be sent
	 * and a response be received, the state doesn't matter.  Here we chose
	 * FTS_PROMOTE_SEGMENT, to simulate a response being received for a PROMOTE
	 * message.
	 */
	init_fts_context(&context, FTS_PROMOTE_SEGMENT);
	fts_segment_info *ftsInfo = &context.perSegInfos[0];

	/* Simulate that data is available for reading from the socket. */
	ftsInfo->poll_revents = POLLIN;
	/*
	 * PQstatus is called once before consuming input and once more, after
	 * parsing results.
	 */
	expect_value_count(PQstatus, conn, ftsInfo->conn, 2);
	will_return_count(PQstatus, CONNECTION_OK, 2);

	expect_value(PQconsumeInput, conn, ftsInfo->conn);
	will_return(PQconsumeInput, 1);

	expect_value(PQisBusy, conn, ftsInfo->conn);
	will_return(PQisBusy, 0);

	PGresult *result = palloc(sizeof(PGresult));
	expect_value(PQgetResult, conn, ftsInfo->conn);
	will_return(PQgetResult, result);

	expect_value(PQresultStatus, res, result);
	will_return(PQresultStatus, PGRES_FATAL_ERROR);
	expect_value(PQresultErrorMessage, res, result);
	will_be_called(PQresultErrorMessage);

	expect_value(PQclear, res, result);
	will_be_called(PQclear);

	/*
	 * TEST
	 */
	ftsReceive(&context);

	assert_true(ftsInfo->state == FTS_PROMOTE_FAILED);
}

/*
 * 2 primary-mirror pairs: one got a "request retry" response from primary,
 * syncrep_off message failed to get a response from the other primary.
 * Another attempt must be made in both cases after waiting for 1 second.
 */
static void
test_processRetry_wait_before_retry(void **state)
{
	/* Start with a failure state and retry_count = 0. */
	CdbComponentDatabases *cdbs = InitTestCdb(
		2, true, GP_SEGMENT_CONFIGURATION_MODE_INSYNC);
	fts_context context;
	FtsWalRepInitProbeContext(cdbs, &context);
	init_fts_context(&context, FTS_SYNCREP_OFF_FAILED);

	/* First primary sent a response with requestRetry set. */
	fts_segment_info *ftsInfo1 = &context.perSegInfos[0];
	ftsInfo1->state = FTS_PROBE_SUCCESS;
	ftsInfo1->result.isPrimaryAlive = true;
	ftsInfo1->result.retryRequested = true;

	/* Second primary didn't respond to syncrep_off message. */
	fts_segment_info *ftsInfo2 = &context.perSegInfos[1];

	expect_value(PQfinish, conn, ftsInfo1->conn);
	will_be_called(PQfinish);
	expect_value(PQfinish, conn, ftsInfo2->conn);
	will_be_called(PQfinish);

	processRetry(&context);

	/* We must wait in a retry_wait state with retryStartTime set. */
	assert_true(ftsInfo1->state == FTS_PROBE_RETRY_WAIT);
	assert_true(ftsInfo2->state == FTS_SYNCREP_OFF_RETRY_WAIT);
	assert_true(ftsInfo1->retry_count == 1);
	assert_true(ftsInfo1->poll_events == 0);
	assert_true(ftsInfo1->poll_revents == 0);
	pg_time_t retryStartTime1 = ftsInfo1->retryStartTime;
	assert_true(retryStartTime1 > 0);
	assert_true(ftsInfo2->retry_count == 1);
	assert_true(ftsInfo2->poll_events == 0);
	assert_true(ftsInfo2->poll_revents == 0);
	pg_time_t retryStartTime2 = ftsInfo2->retryStartTime;
	assert_true(retryStartTime2 > 0);

	/*
	 * We must continue to wait because 1 second hasn't elapsed since the
	 * failure.
	 */
	processRetry(&context);

	assert_true(ftsInfo1->state == FTS_PROBE_RETRY_WAIT);
	assert_true(ftsInfo2->state == FTS_SYNCREP_OFF_RETRY_WAIT);

	/*
	 * Adjust retryStartTime to 1 second in past so that next processRetry()
	 * should make a retry attempt.
	 */
	ftsInfo1->retryStartTime = retryStartTime1 - 1;
	ftsInfo2->retryStartTime = retryStartTime2 - 1;

	processRetry(&context);

	/* This time, we must be ready to make another retry. */
	assert_true(ftsInfo1->state == FTS_PROBE_SEGMENT);
	assert_true(ftsInfo2->state == FTS_SYNCREP_OFF_SEGMENT);
}


/* 0 segments, is_updated is always false */
static void
test_processResponse_for_zero_segment(void **state)
{
	fts_context context;

	context.num_pairs = 0;

	bool is_updated = processResponse(&context);

	assert_false(is_updated);
}

/*
 * 1 segment, is_updated is false, because FtsIsActive failed
 */
static void
test_processResponse_for_FtsIsActive_false(void **state)
{

	CdbComponentDatabases *cdbs = InitTestCdb(
		1, true, GP_SEGMENT_CONFIGURATION_MODE_INSYNC);
	fts_context context;
	FtsWalRepInitProbeContext(cdbs, &context);
	init_fts_context(&context, FTS_PROBE_SUCCESS);
	context.perSegInfos[0].result.isPrimaryAlive = true;
	context.perSegInfos[0].result.isMirrorAlive = true;
	context.perSegInfos[0].result.isSyncRepEnabled = true;

	/* mock FtsIsActive false */
	will_return(FtsIsActive, false);

	bool is_updated = processResponse(&context);

	assert_false(is_updated);
	assert_true(context.perSegInfos[0].state == FTS_PROBE_SUCCESS);
}

/*
 * 2 segments, is_updated is false, because neither primary nor mirror
 * state changed.
 */
static void
test_PrimayUpMirrorUpNotInSync_to_PrimayUpMirrorUpNotInSync(void **state)
{
	CdbComponentDatabases *cdbs = InitTestCdb(
		2, true, GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);
	fts_context context;
	FtsWalRepInitProbeContext(cdbs, &context);
	init_fts_context(&context, FTS_PROBE_SUCCESS);

	will_return_count(FtsIsActive, true, 2);

	/* Primary must block commits as long as it and its mirror are alive. */
	context.perSegInfos[0].result.isPrimaryAlive = true;
	context.perSegInfos[0].result.isMirrorAlive = true;
	context.perSegInfos[0].result.isSyncRepEnabled = true;
	context.perSegInfos[1].result.isPrimaryAlive = true;
	context.perSegInfos[1].result.isMirrorAlive = true;
	context.perSegInfos[1].result.isSyncRepEnabled = true;

	expect_value(PQfinish, conn, context.perSegInfos[0].conn);
	will_be_called(PQfinish);
	expect_value(PQfinish, conn, context.perSegInfos[1].conn);
	will_be_called(PQfinish);

	/* processResponse should not update a probe state */
	bool is_updated = processResponse(&context);

	/* Active connections must be closed after processing response. */
	assert_true(context.perSegInfos[0].conn == NULL);
	assert_true(context.perSegInfos[1].conn == NULL);
	assert_false(is_updated);
}

/*
 * 2 segments, is_updated is false, because its double fault scenario
 * primary and mirror are not in sync hence cannot promote mirror, hence
 * current primary needs to stay marked as up.
 */
static void
test_PrimayUpMirrorUpNotInSync_to_PrimaryDown(void **state)
{
	CdbComponentDatabases *cdbs = InitTestCdb(
		2, true, GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);
	fts_context context;
	FtsWalRepInitProbeContext(cdbs, &context);
	init_fts_context(&context, FTS_PROBE_SUCCESS);

	will_return_count(FtsIsActive, true, 2);

	/* No response received from first segment */
	context.perSegInfos[0].state = FTS_PROBE_FAILED;
	context.perSegInfos[0].retry_count = gp_fts_probe_retries;

	/* Response received from second segment */
	context.perSegInfos[1].result.isPrimaryAlive = true;
	context.perSegInfos[1].result.isMirrorAlive = true;
	context.perSegInfos[1].result.isSyncRepEnabled = true;

	expect_value(PQfinish, conn, context.perSegInfos[0].conn);
	will_be_called(PQfinish);
	expect_value(PQfinish, conn, context.perSegInfos[1].conn);
	will_be_called(PQfinish);

	/* No update must happen */
	bool is_updated = processResponse(&context);

	/* Active connections must be closed after processing response. */
	assert_true(context.perSegInfos[0].conn == NULL);
	assert_true(context.perSegInfos[1].conn == NULL);
	assert_false(is_updated);
}

/*
 * 2 segments, is_updated is true, because content 0 mirror is updated
 */
static void
test_PrimayUpMirrorUpNotInSync_to_PrimaryUpMirrorDownNotInSync(void **state)
{
	CdbComponentDatabases *cdbs = InitTestCdb(
		2, true, GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);
	fts_context context;
	FtsWalRepInitProbeContext(cdbs, &context);
	init_fts_context(&context, FTS_PROBE_SUCCESS);

	will_return_count(FtsIsActive, true, 2);

	/*
	 * Test response received from both segments. First primary's mirror is
	 * reported as DOWN.
	 */
	context.perSegInfos[0].result.isPrimaryAlive = true;
	context.perSegInfos[0].result.isMirrorAlive = false;

	/* Syncrep must be enabled because mirror is up. */
	context.perSegInfos[1].result.isPrimaryAlive = true;
	context.perSegInfos[1].result.isMirrorAlive = true;
	context.perSegInfos[1].result.isSyncRepEnabled = true;

	/* the mirror will be updated */
	PrimaryOrMirrorWillBeUpdated(1);
	ExpectedPrimaryAndMirrorConfiguration(
		context.perSegInfos[0].primary_cdbinfo, /* primary */
		context.perSegInfos[0].mirror_cdbinfo, /* mirror */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* primary status */
		GP_SEGMENT_CONFIGURATION_STATUS_DOWN, /* mirror status */
		GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC, /* mode */
		GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY, /* newPrimaryRole */
		GP_SEGMENT_CONFIGURATION_ROLE_MIRROR, /* newMirrorRole */
		false, /* willUpdatePrimary */
		true /* willUpdateMirror */);

	expect_value(PQfinish, conn, context.perSegInfos[0].conn);
	will_be_called(PQfinish);
	expect_value(PQfinish, conn, context.perSegInfos[1].conn);
	will_be_called(PQfinish);

	bool is_updated = processResponse(&context);

	assert_true(is_updated);
	/* Active connections must be closed after processing response. */
	assert_true(context.perSegInfos[0].conn == NULL);
	assert_true(context.perSegInfos[1].conn == NULL);
}

/*
 * 3 segments, is_updated is true, because content 0 mirror is down and
 * probe response is up
 */
static void
test_PrimaryUpMirrorDownNotInSync_to_PrimayUpMirrorUpNotInSync(void **state)
{
	CdbComponentDatabases *cdbs = InitTestCdb(
		3, true, GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);
	fts_context context;
	FtsWalRepInitProbeContext(cdbs, &context);
	init_fts_context(&context, FTS_PROBE_SUCCESS);

	will_return_count(FtsIsActive, true, 3);

	/* set the mirror down in config */
	CdbComponentDatabaseInfo *cdbinfo =
		GetSegmentFromCdbComponentDatabases(
			cdbs, 0, GP_SEGMENT_CONFIGURATION_ROLE_MIRROR);

	cdbinfo->config->status = GP_SEGMENT_CONFIGURATION_STATUS_DOWN;

	/*
	 * Response received from all three segments, DOWN mirror is reported UP
	 * for first primary.
	 */
	context.perSegInfos[0].result.isPrimaryAlive = true;
	context.perSegInfos[0].result.isMirrorAlive = true;
	context.perSegInfos[0].result.isSyncRepEnabled = true;

	/* no change */
	context.perSegInfos[1].result.isPrimaryAlive = true;
	context.perSegInfos[1].result.isMirrorAlive = true;
	context.perSegInfos[1].result.isSyncRepEnabled = true;
	context.perSegInfos[2].result.isPrimaryAlive = true;
	context.perSegInfos[2].result.isMirrorAlive = true;
	context.perSegInfos[2].result.isSyncRepEnabled = true;

	/* the mirror will be updated */
	PrimaryOrMirrorWillBeUpdated(1);
	ExpectedPrimaryAndMirrorConfiguration(
		context.perSegInfos[0].primary_cdbinfo, /* primary */
		context.perSegInfos[0].mirror_cdbinfo, /* primary */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* primary status */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* mirror status */
		GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC, /* mode */
		GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY, /* newPrimaryRole */
		GP_SEGMENT_CONFIGURATION_ROLE_MIRROR, /* newMirrorRole */
		false, /* willUpdatePrimary */
		true /* willUpdateMirror */);

	/* Active connections must be closed after processing response. */
	expect_value(PQfinish, conn, context.perSegInfos[0].conn);
	will_be_called(PQfinish);
	expect_value(PQfinish, conn, context.perSegInfos[1].conn);
	will_be_called(PQfinish);
	expect_value(PQfinish, conn, context.perSegInfos[2].conn);
	will_be_called(PQfinish);

	bool is_updated = processResponse(&context);

	assert_true(is_updated);
	/*
	 * Assert that connections are closed and the state of the segments is not
	 * changed (no further messages needed from FTS).
	 */
	assert_true(context.perSegInfos[0].conn == NULL);
	assert_true(context.perSegInfos[0].state == FTS_RESPONSE_PROCESSED);
	assert_true(context.perSegInfos[1].conn == NULL);
	assert_true(context.perSegInfos[1].state == FTS_RESPONSE_PROCESSED);
	assert_true(context.perSegInfos[2].conn == NULL);
	assert_true(context.perSegInfos[2].state == FTS_RESPONSE_PROCESSED);
}

/*
 * 5 segments, is_updated is true, as we are changing the state of several
 * segment pairs.  This test also validates that sync-rep off and promotion
 * messages are not blocked by primary retry requests.
 */
static void
test_processResponse_multiple_segments(void **state)
{
	CdbComponentDatabases *cdbs = InitTestCdb(
		5, true, GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);
	fts_context context;
	FtsWalRepInitProbeContext(cdbs, &context);
	init_fts_context(&context, FTS_PROBE_SUCCESS);

	will_return_count(FtsIsActive, true, 5);

	/*
	 * Mark the mirror for content 0 down in configuration, probe response
	 * indicates it's up.
	 */
	CdbComponentDatabaseInfo *cdbinfo =
		GetSegmentFromCdbComponentDatabases(
			cdbs, 0, GP_SEGMENT_CONFIGURATION_ROLE_MIRROR);

	cdbinfo->config->status = GP_SEGMENT_CONFIGURATION_STATUS_DOWN;

	/* First segment DOWN mirror, now reported UP */
	context.perSegInfos[0].result.isPrimaryAlive = true;
	context.perSegInfos[0].result.isMirrorAlive = true;
	context.perSegInfos[0].result.isSyncRepEnabled = true;

	/*
	 * Mark the primary-mirror pair for content 1 as in-sync in configuration
	 * so that the mirror can be promoted.
	 */
	cdbinfo = GetSegmentFromCdbComponentDatabases(
		cdbs, 1, GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY);
	cdbinfo->config->mode = GP_SEGMENT_CONFIGURATION_MODE_INSYNC;
	cdbinfo = GetSegmentFromCdbComponentDatabases(
		cdbs, 1, GP_SEGMENT_CONFIGURATION_ROLE_MIRROR);
	cdbinfo->config->mode = GP_SEGMENT_CONFIGURATION_MODE_INSYNC;

	/* Second segment no response received, mirror will be promoted */
	context.perSegInfos[1].state = FTS_PROBE_FAILED;
	context.perSegInfos[1].retry_count = gp_fts_probe_retries;

	/* Third segment UP mirror, now reported DOWN */
	context.perSegInfos[2].result.isPrimaryAlive = true;
	context.perSegInfos[2].result.isSyncRepEnabled = true;
	context.perSegInfos[2].result.isMirrorAlive = false;

	/* Fourth segment, response received no change */
	context.perSegInfos[3].result.isPrimaryAlive = true;
	context.perSegInfos[3].result.isSyncRepEnabled = true;
	context.perSegInfos[3].result.isMirrorAlive = true;

	/* Fifth segment, probe failed but retries not exhausted */
	context.perSegInfos[4].result.isPrimaryAlive = false;
	context.perSegInfos[4].result.isSyncRepEnabled = false;
	context.perSegInfos[4].result.isMirrorAlive = false;
	context.perSegInfos[4].state = FTS_PROBE_RETRY_WAIT;

	/* we are updating three of the five segments */
	PrimaryOrMirrorWillBeUpdated(3);

	/* First segment */
	ExpectedPrimaryAndMirrorConfiguration(
		context.perSegInfos[0].primary_cdbinfo, /* primary */
		context.perSegInfos[0].mirror_cdbinfo, /* mirror */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* primary status */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* mirror status */
		GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC, /* mode */
		GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY, /* newPrimaryRole */
		GP_SEGMENT_CONFIGURATION_ROLE_MIRROR, /* newMirrorRole */
		false, /* willUpdatePrimary */
		true /* willUpdateMirror */);

	/*
	 * Second segment should go through mirror promotion.
	 */
	ExpectedPrimaryAndMirrorConfiguration(
		context.perSegInfos[1].primary_cdbinfo, /* primary */
		context.perSegInfos[1].mirror_cdbinfo, /* mirror */
		GP_SEGMENT_CONFIGURATION_STATUS_DOWN, /* primary status */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* mirror status */
		GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC, /* mode */
		GP_SEGMENT_CONFIGURATION_ROLE_MIRROR, /* newPrimaryRole */
		GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY, /* newMirrorRole */
		true, /* willUpdatePrimary */
		true /* willUpdateMirror */);

	/* Third segment */
	ExpectedPrimaryAndMirrorConfiguration(
		context.perSegInfos[2].primary_cdbinfo, /* primary */
		context.perSegInfos[2].mirror_cdbinfo, /* mirror */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* primary status */
		GP_SEGMENT_CONFIGURATION_STATUS_DOWN, /* mirror status */
		GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC, /* mode */
		GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY, /* newPrimaryRole */
		GP_SEGMENT_CONFIGURATION_ROLE_MIRROR, /* newMirrorRole */
		false, /* willUpdatePrimary */
		true /* willUpdateMirror */);

	/* Fourth segment will not change status */

	/* Active connections must be closed after processing response. */
	expect_value(PQfinish, conn, context.perSegInfos[0].conn);
	will_be_called(PQfinish);
	expect_value(PQfinish, conn, context.perSegInfos[1].conn);
	will_be_called(PQfinish);
	expect_value(PQfinish, conn, context.perSegInfos[2].conn);
	will_be_called(PQfinish);
	expect_value(PQfinish, conn, context.perSegInfos[3].conn);
	will_be_called(PQfinish);

	bool is_updated = processResponse(&context);

	assert_true(is_updated);
	/* mirror found up */
	assert_true(context.perSegInfos[0].state == FTS_RESPONSE_PROCESSED);
	/* mirror promotion should be triggered */
	assert_true(context.perSegInfos[1].state == FTS_PROMOTE_SEGMENT);
	/* mirror found down, must turn off syncrep on primary */
	assert_true(context.perSegInfos[2].state == FTS_SYNCREP_OFF_SEGMENT);
	/* no change in configuration */
	assert_true(context.perSegInfos[3].state == FTS_RESPONSE_PROCESSED);
	/* retry possible, final state is not yet reached */
	assert_true(context.perSegInfos[4].state == FTS_PROBE_RETRY_WAIT);
}

/*
 * 1 segment, is_updated is true, because primary and mirror will be
 * marked not in sync
 */
static void
test_PrimayUpMirrorUpSync_to_PrimaryUpMirrorUpNotInSync(void **state)
{
	CdbComponentDatabases *cdbs = InitTestCdb(
		1, true, GP_SEGMENT_CONFIGURATION_MODE_INSYNC);
	fts_context context;
	FtsWalRepInitProbeContext(cdbs, &context);
	init_fts_context(&context, FTS_PROBE_SUCCESS);

	will_return(FtsIsActive, true);

	/* Probe responded with Mirror Up and Not In SYNC with syncrep enabled */
	context.perSegInfos[0].result.isPrimaryAlive = true;
	context.perSegInfos[0].result.isMirrorAlive = true;
	context.perSegInfos[0].result.isInSync = false;
	context.perSegInfos[0].result.isSyncRepEnabled = true;

	/* we are updating one segment pair */
	PrimaryOrMirrorWillBeUpdated(1);

	ExpectedPrimaryAndMirrorConfiguration(
		context.perSegInfos[0].primary_cdbinfo, /* primary */
		context.perSegInfos[0].mirror_cdbinfo, /* mirror */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* primary status */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* mirror status */
		GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC, /* mode */
		GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY, /* newPrimaryRole */
		GP_SEGMENT_CONFIGURATION_ROLE_MIRROR, /* newMirrorRole */
		true, /* willUpdatePrimary */
		true /* willUpdateMirror */);

	/* Active connections must be closed after processing response. */
	expect_value(PQfinish, conn, context.perSegInfos[0].conn);
	will_be_called(PQfinish);

	bool is_updated = processResponse(&context);

	assert_true(is_updated);
	assert_true(context.perSegInfos[0].state == FTS_RESPONSE_PROCESSED);
	assert_true(context.perSegInfos[0].conn == NULL);
}

/*
 * 2 segments, is_updated is true, because mirror will be marked down and
 * both will be marked not in sync for first primary mirror pair
 */
static void
test_PrimayUpMirrorUpSync_to_PrimaryUpMirrorDownNotInSync(void **state)
{
	CdbComponentDatabases *cdbs = InitTestCdb(
		2, true, GP_SEGMENT_CONFIGURATION_MODE_INSYNC);
	fts_context context;
	FtsWalRepInitProbeContext(cdbs, &context);
	init_fts_context(&context, FTS_PROBE_SUCCESS);

	will_return_count(FtsIsActive, true, 2);

	/*
	 * Probe responded with one mirror down and not in-sync, and syncrep
	 * enabled on both primaries.
	 */
	context.perSegInfos[0].result.isPrimaryAlive = true;
	context.perSegInfos[0].result.isMirrorAlive = false;
	context.perSegInfos[0].result.isInSync = false;
	context.perSegInfos[0].result.isSyncRepEnabled = true;
	context.perSegInfos[1].result.isPrimaryAlive = true;
	context.perSegInfos[1].result.isMirrorAlive = true;
	context.perSegInfos[1].result.isInSync = true;
	context.perSegInfos[1].result.isSyncRepEnabled = true;

	/* we are updating one segment pair */
	PrimaryOrMirrorWillBeUpdated(1);

	ExpectedPrimaryAndMirrorConfiguration(
		context.perSegInfos[0].primary_cdbinfo, /* primary */
		context.perSegInfos[0].mirror_cdbinfo, /* mirror */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* primary status */
		GP_SEGMENT_CONFIGURATION_STATUS_DOWN, /* mirror status */
		GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC, /* mode */
		GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY, /* newPrimaryRole */
		GP_SEGMENT_CONFIGURATION_ROLE_MIRROR, /* newMirrorRole */
		true, /* willUpdatePrimary */
		true /* willUpdateMirror */);

	/* Active connections must be closed after processing response. */
	expect_value(PQfinish, conn, context.perSegInfos[0].conn);
	will_be_called(PQfinish);
	expect_value(PQfinish, conn, context.perSegInfos[1].conn);
	will_be_called(PQfinish);

	bool is_updated = processResponse(&context);

	assert_true(is_updated);
	/* mirror is down but syncrep is enabled, so it must be turned off */
	assert_true(context.perSegInfos[0].state == FTS_SYNCREP_OFF_SEGMENT);
	/* no change in config */
	assert_true(context.perSegInfos[1].state == FTS_RESPONSE_PROCESSED);
	assert_true(context.perSegInfos[0].conn == NULL);
	assert_true(context.perSegInfos[1].conn == NULL);
}

/*
 * 1 segment, is_updated is true, because FTS found primary goes down and
 * both will be marked not in sync, then FTS promote mirror
 */
static void
test_PrimayUpMirrorUpSync_to_PrimaryDown_to_MirrorPromote(void **state)
{
	CdbComponentDatabases *cdbs = InitTestCdb(
		1, true, GP_SEGMENT_CONFIGURATION_MODE_INSYNC);
	fts_context context;
	FtsWalRepInitProbeContext(cdbs, &context);
	init_fts_context(&context, FTS_PROBE_SUCCESS);

	will_return(FtsIsActive, true);

	/* Probe response was not received. */
	context.perSegInfos[0].state = FTS_PROBE_FAILED;
	context.perSegInfos[0].retry_count = gp_fts_probe_retries;
	/* Store reference to mirror object for validation later. */
	CdbComponentDatabaseInfo *mirror = context.perSegInfos[0].mirror_cdbinfo;

	/* we are updating one segment pair */
	PrimaryOrMirrorWillBeUpdated(1);

	ExpectedPrimaryAndMirrorConfiguration(
		context.perSegInfos[0].primary_cdbinfo, /* primary */
		context.perSegInfos[0].mirror_cdbinfo, /* mirror */
		GP_SEGMENT_CONFIGURATION_STATUS_DOWN, /* primary status */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* mirror status */
		GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC, /* mode */
		GP_SEGMENT_CONFIGURATION_ROLE_MIRROR, /* newPrimaryRole */
		GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY, /* newMirrorRole */
		true, /* willUpdatePrimary */
		true /* willUpdateMirror */);

	/* Active connections must be closed after processing response. */
	expect_value(PQfinish, conn, context.perSegInfos[0].conn);
	will_be_called(PQfinish);

	bool is_updated = processResponse(&context);

	assert_true(is_updated);
	/* the mirror must be marked for promotion */
	assert_true(context.perSegInfos[0].state == FTS_PROMOTE_SEGMENT);
	assert_int_equal(context.perSegInfos[0].primary_cdbinfo->config->dbid, mirror->config->dbid);
	assert_true(context.perSegInfos[0].conn == NULL);
}

/*
 * 1 segment, is_updated is true, because primary and mirror will be
 * marked in sync
 */
static void
test_PrimayUpMirrorUpNotInSync_to_PrimayUpMirrorUpSync(void **state)
{
	CdbComponentDatabases *cdbs = InitTestCdb(
		1, true, GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);
	fts_context context;
	FtsWalRepInitProbeContext(cdbs, &context);
	init_fts_context(&context, FTS_PROBE_SUCCESS);

	will_return(FtsIsActive, true);

	/* Probe responded with Mirror Up and SYNC */
	context.perSegInfos[0].result.isPrimaryAlive = true;
	context.perSegInfos[0].result.isMirrorAlive = true;
	context.perSegInfos[0].result.isInSync = true;
	context.perSegInfos[0].result.isSyncRepEnabled = true;

	/* we are updating one segment pair */
	PrimaryOrMirrorWillBeUpdated(1);

	ExpectedPrimaryAndMirrorConfiguration(
		context.perSegInfos[0].primary_cdbinfo, /* primary */
		context.perSegInfos[0].mirror_cdbinfo, /* mirror */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* primary status */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* mirror status */
		GP_SEGMENT_CONFIGURATION_MODE_INSYNC, /* mode */
		GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY, /* newPrimaryRole */
		GP_SEGMENT_CONFIGURATION_ROLE_MIRROR, /* newMirrorRole */
		true, /* willUpdatePrimary */
		true /* willUpdateMirror */);

	/* Active connections must be closed after processing response. */
	expect_value(PQfinish, conn, context.perSegInfos[0].conn);
	will_be_called(PQfinish);

	bool is_updated = processResponse(&context);

	assert_true(is_updated);
	assert_true(context.perSegInfos[0].state == FTS_RESPONSE_PROCESSED);
	assert_true(context.perSegInfos[0].conn == NULL);
}

/*
 * 1 segment, is_updated is true, because mirror will be marked UP and
 * both primary and mirror should get updated to SYNC
 */
static void
test_PrimaryUpMirrorDownNotInSync_to_PrimayUpMirrorUpSync(void **state)
{
	CdbComponentDatabases *cdbs = InitTestCdb(
		1, true, GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);
	fts_context context;
	FtsWalRepInitProbeContext(cdbs, &context);
	init_fts_context(&context, FTS_PROBE_SUCCESS);

	will_return(FtsIsActive, true);

	/* set the mirror down in config */
	CdbComponentDatabaseInfo *cdbinfo =
		GetSegmentFromCdbComponentDatabases(
			cdbs, 0, GP_SEGMENT_CONFIGURATION_ROLE_MIRROR);

	cdbinfo->config->status = GP_SEGMENT_CONFIGURATION_STATUS_DOWN;

	/* Probe responded with Mirror Up and SYNC */
	context.perSegInfos[0].result.isPrimaryAlive = true;
	context.perSegInfos[0].result.isMirrorAlive = true;
	context.perSegInfos[0].result.isInSync = true;
	context.perSegInfos[0].result.isSyncRepEnabled = true;

	/* we are updating one segment pair */
	PrimaryOrMirrorWillBeUpdated(1);

	ExpectedPrimaryAndMirrorConfiguration(
		context.perSegInfos[0].primary_cdbinfo, /* primary */
		context.perSegInfos[0].mirror_cdbinfo, /* mirror */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* primary status */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* mirror status */
		GP_SEGMENT_CONFIGURATION_MODE_INSYNC, /* mode */
		GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY, /* newPrimaryRole */
		GP_SEGMENT_CONFIGURATION_ROLE_MIRROR, /* newMirrorRole */
		true, /* willUpdatePrimary */
		true /* willUpdateMirror */);

	/* Active connections must be closed after processing response. */
	expect_value(PQfinish, conn, context.perSegInfos[0].conn);
	will_be_called(PQfinish);

	bool is_updated = processResponse(&context);

	assert_true(is_updated);
	assert_true(context.perSegInfos[0].conn == NULL);
	assert_true(context.perSegInfos[0].state == FTS_RESPONSE_PROCESSED);
}

/*
 * 1 segment, is_updated is false, because there is no status or mode change.
 */
static void
test_PrimaryUpMirrorDownNotInSync_to_PrimayUpMirrorDownNotInSync(void **state)
{
	CdbComponentDatabases *cdbs = InitTestCdb(
		1, true, GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);
	fts_context context;
	FtsWalRepInitProbeContext(cdbs, &context);
	init_fts_context(&context, FTS_PROBE_SUCCESS);

	will_return(FtsIsActive, true);

	/* set the mirror down in config */
	CdbComponentDatabaseInfo *cdbinfo =
		GetSegmentFromCdbComponentDatabases(
			cdbs, 0, GP_SEGMENT_CONFIGURATION_ROLE_MIRROR);

	cdbinfo->config->status = GP_SEGMENT_CONFIGURATION_STATUS_DOWN;

	/* Probe responded with Mirror Up and SYNC */
	context.perSegInfos[0].result.isPrimaryAlive = true;
	context.perSegInfos[0].result.isMirrorAlive = false;
	context.perSegInfos[0].result.isInSync = false;

	expect_value(PQfinish, conn, context.perSegInfos[0].conn);
	will_be_called(PQfinish);

	bool is_updated = processResponse(&context);

	assert_false(is_updated);
	assert_true(context.perSegInfos[0].conn == NULL);
	assert_true(context.perSegInfos[0].state == FTS_RESPONSE_PROCESSED);
}

/*
 * 2 segments, is_updated is false, because content 0 mirror is already
 * down and probe response fails. Means double fault scenario.
 */
static void
test_PrimaryUpMirrorDownNotInSync_to_PrimaryDown(void **state)
{
	CdbComponentDatabases *cdbs = InitTestCdb(
		2, true, GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);
	fts_context context;
	FtsWalRepInitProbeContext(cdbs, &context);
	init_fts_context(&context, FTS_PROBE_SUCCESS);

	will_return_count(FtsIsActive, true, 2);

	/* set the mirror down in config */
	CdbComponentDatabaseInfo *cdbinfo =
		GetSegmentFromCdbComponentDatabases(
			cdbs, 0, GP_SEGMENT_CONFIGURATION_ROLE_MIRROR);

	cdbinfo->config->status = GP_SEGMENT_CONFIGURATION_STATUS_DOWN;

	/* No response received from segment 1 (content 0 primary) */
	context.perSegInfos[0].state = FTS_PROBE_FAILED;
	context.perSegInfos[0].retry_count = gp_fts_probe_retries;

	/* No change for segment 2, probe successful */
	context.perSegInfos[1].result.isPrimaryAlive = true;
	context.perSegInfos[1].result.isSyncRepEnabled = true;
	context.perSegInfos[1].result.isMirrorAlive = true;

	/* Active connections must be closed after processing response. */
	expect_value(PQfinish, conn, context.perSegInfos[0].conn);
	will_be_called(PQfinish);
	expect_value(PQfinish, conn, context.perSegInfos[1].conn);
	will_be_called(PQfinish);

	bool is_updated = processResponse(&context);

	assert_false(is_updated);
	assert_true(context.perSegInfos[0].conn == NULL);
	assert_true(context.perSegInfos[1].conn == NULL);
	assert_true(context.perSegInfos[0].state == FTS_RESPONSE_PROCESSED);
	assert_true(context.perSegInfos[1].state == FTS_RESPONSE_PROCESSED);
}

/*
 * 1 segment, probe times out.
 */
static void
test_probeTimeout(void **state)
{
	CdbComponentDatabases *cdbs = InitTestCdb(
		1, true, GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);
	fts_context context;
	FtsWalRepInitProbeContext(cdbs, &context);
	init_fts_context(&context, FTS_PROBE_SEGMENT);

	pg_time_t now = (pg_time_t) time(NULL);
	context.perSegInfos[0].startTime = now - gp_fts_probe_timeout - 1;

	ftsCheckTimeout(&context.perSegInfos[0], now);

	assert_true(context.perSegInfos[0].state == FTS_PROBE_FAILED);
	/*
	 * Timeout should be treated as just another failure and should be
	 * retried.
	 */
	assert_true(context.perSegInfos[0].retry_count == 0);
}

static void
test_FtsWalRepInitProbeContext_initial_state(void **state)
{
	fts_context context;
	CdbComponentDatabases *cdb_component_dbs;

	cdb_component_dbs = InitTestCdb(2,
									true,
									GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);

	FtsWalRepInitProbeContext(cdb_component_dbs, &context);

	int i;
	for (i=0; i < context.num_pairs; i++)
	{
		assert_true(context.perSegInfos[i].state == FTS_PROBE_SEGMENT);
		assert_true(context.perSegInfos[i].retry_count == 0);
		assert_true(context.perSegInfos[i].conn == NULL);
		assert_true(context.perSegInfos[i].probe_errno == 0);
		assert_true(context.perSegInfos[i].result.dbid ==
					context.perSegInfos[i].primary_cdbinfo->config->dbid);
		assert_false(context.perSegInfos[i].result.isPrimaryAlive);
		assert_false(context.perSegInfos[i].result.isMirrorAlive);
		assert_false(context.perSegInfos[i].result.isInSync);
	}
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_ftsConnect_FTS_PROBE_SEGMENT),
		unit_test(test_ftsConnect_one_failure_one_success),
		unit_test(test_ftsConnect_ftsPoll),
		unit_test(test_ftsSend_success),
		unit_test(test_ftsReceive_success),
		unit_test(test_ftsReceive_when_fts_handler_FATAL),
		unit_test(test_ftsReceive_when_fts_handler_ERROR),
		unit_test(test_processRetry_wait_before_retry),
		/* -----------------------------------------------------------------------
		 * Group of tests for processResponse()
		 * -----------------------------------------------------------------------
		 */
		unit_test(test_processResponse_for_zero_segment),
		unit_test(test_processResponse_for_FtsIsActive_false),
		unit_test(test_processResponse_multiple_segments),
		unit_test(test_PrimayUpMirrorUpSync_to_PrimaryUpMirrorUpNotInSync),
		unit_test(test_PrimayUpMirrorUpSync_to_PrimaryUpMirrorDownNotInSync),
		unit_test(test_PrimayUpMirrorUpSync_to_PrimaryDown_to_MirrorPromote),

		unit_test(test_PrimayUpMirrorUpNotInSync_to_PrimayUpMirrorUpSync),
		unit_test(test_PrimayUpMirrorUpNotInSync_to_PrimayUpMirrorUpNotInSync),
		unit_test(test_PrimayUpMirrorUpNotInSync_to_PrimaryUpMirrorDownNotInSync),
		unit_test(test_PrimayUpMirrorUpNotInSync_to_PrimaryDown),

		unit_test(test_PrimaryUpMirrorDownNotInSync_to_PrimayUpMirrorUpSync),
		unit_test(test_PrimaryUpMirrorDownNotInSync_to_PrimayUpMirrorUpNotInSync),
		unit_test(test_PrimaryUpMirrorDownNotInSync_to_PrimayUpMirrorDownNotInSync),
		unit_test(test_PrimaryUpMirrorDownNotInSync_to_PrimaryDown),
		unit_test(test_probeTimeout),
		/*-----------------------------------------------------------------------*/
		unit_test(test_FtsWalRepInitProbeContext_initial_state)
	};
	MemoryContextInit();
	InitFtsProbeInfo();
	return run_tests(tests);
}
