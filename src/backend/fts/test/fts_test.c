#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"
/* Actual function body */
#include "../fts.c"
#include "./fts_test_helper.h"

static void
InitFtsProbeInfo(void)
{
	static FtsProbeInfo fts_info;
	ftsProbeInfo = &fts_info;
}

static void
InitContext(fts_context *context, PGconn *pgconn)
{
	int i;
	pg_time_t now = (pg_time_t) time(NULL);
	for (i = 0; i < context->num_pairs; i++)
	{
		context->perSegInfos[i].conn = pgconn;
		context->perSegInfos[i].startTime = now;
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

		if (cdb->segindex == segindex && cdb->role == role)
			return cdb;
	}
	return NULL;
}

/* mock FtsIsActive */
void
MockFtsIsActive(bool expected_return_value)
{
	skipFtsProbe = !expected_return_value;
}

static int
CheckConfigVals(const Datum *value, /* from the function under test */
				const Datum *check_value)	/* from the test case */
{
	return datumIsEqual(value[Anum_gp_segment_configuration_status - 1],
						check_value[Anum_gp_segment_configuration_status - 1],
						true, false) &&
		datumIsEqual(value[Anum_gp_segment_configuration_mode - 1],
					 check_value[Anum_gp_segment_configuration_mode - 1],
					 true, false);
}

static int
CheckHistrelVals(const Datum *value,	/* from the function under test */
				 const Datum *check_value)	/* from the test case */
{
	return datumIsEqual(value[Anum_gp_configuration_history_dbid - 1],
						check_value[Anum_gp_configuration_history_dbid - 1],
						true, false) &&
		datumIsEqual(value[Anum_gp_configuration_history_desc - 1],
					 check_value[Anum_gp_configuration_history_desc - 1],
					 false, -1);
}

static void
probeWalRepUpdateConfig_will_be_called_with(
											int16 dbid,
											int16 segindex,
											char role,
											char status,
											char mode)
{
	/* Mock heap_open gp_configuration_history_relation */
	static RelationData gp_configuration_history_relation;

	expect_value(heap_open, relationId, GpConfigHistoryRelationId);
	expect_any(heap_open, lockmode);
	will_return(heap_open, &gp_configuration_history_relation);

	expect_value(FaultInjector_InjectFaultIfSet, identifier, FtsUpdateConfig);
	expect_value(FaultInjector_InjectFaultIfSet, ddlStatement, DDLNotSpecified);
	expect_value(FaultInjector_InjectFaultIfSet, databaseName, "");
	expect_value(FaultInjector_InjectFaultIfSet, tableName, "");
	will_return(FaultInjector_InjectFaultIfSet, FaultInjectorTypeNotSpecified);
	/* Mock heap_open gp_segment_configuration_relation */
	static RelationData gp_segment_configuration_relation;

	expect_value(heap_open, relationId, GpSegmentConfigRelationId);
	expect_any(heap_open, lockmode);
	will_return(heap_open, &gp_segment_configuration_relation);

	char		desc[SQL_CMD_BUF_SIZE];
	Datum	   *histvals = palloc(sizeof(Datum) * Natts_gp_configuration_history);

	histvals[Anum_gp_configuration_history_dbid - 1] =
		Int16GetDatum(dbid);

	snprintf(desc, sizeof(desc),
		 "FTS: update role, status, and mode for dbid %d with contentid %d to %c, %c, and %c",
		 dbid, segindex, role,
		 status,
		 mode
		 );

	histvals[Anum_gp_configuration_history_desc - 1] =
		CStringGetTextDatum(desc);


	/* Mock heap_form_tuple inline function */
	expect_any_count(heaptuple_form_to, tupleDescriptor, 1);
	expect_check(heaptuple_form_to, values, CheckHistrelVals, histvals);
	expect_any_count(heaptuple_form_to, isnull, 1);
	expect_any_count(heaptuple_form_to, dst, 1);
	expect_any_count(heaptuple_form_to, dstlen, 1);
	will_be_called_count(heaptuple_form_to, 1);

	/* Mock simple_heap_insert */
	expect_any_count(simple_heap_insert, relation, 1);
	expect_any_count(simple_heap_insert, tup, 1);
	will_be_called_count(simple_heap_insert, 1);

	/* Mock CatalogUpdateIndexes */
	expect_any_count(CatalogUpdateIndexes, heapRel, 2);
	expect_any_count(CatalogUpdateIndexes, heapTuple, 2);
	will_be_called_count(CatalogUpdateIndexes, 2);

	/* Mock heap_close */
	expect_any_count(relation_close, relation, 2);
	expect_any_count(relation_close, lockmode, 2);
	will_be_called_count(relation_close, 2);

	/* Mock ScanKeyInit */
	expect_any_count(ScanKeyInit, entry, 1);
	expect_any_count(ScanKeyInit, attributeNumber, 1);
	expect_any_count(ScanKeyInit, strategy, 1);
	expect_any_count(ScanKeyInit, procedure, 1);
	expect_value_count(ScanKeyInit, argument, dbid, 1);
	will_be_called_count(ScanKeyInit, 1);

	/* Mock systable_beginscan */
	expect_any_count(systable_beginscan, heapRelation, 1);
	expect_any_count(systable_beginscan, indexId, 1);
	expect_any_count(systable_beginscan, indexOK, 1);
	expect_any_count(systable_beginscan, snapshot, 1);
	expect_any_count(systable_beginscan, nkeys, 1);
	expect_any_count(systable_beginscan, key, 1);
	will_be_called_count(systable_beginscan, 1);

	static HeapTupleData config_tuple;

	typedef struct {
	  HeapTupleHeaderData header;
	  FormData_gp_segment_configuration data;
	} gp_segment_configuration_tuple;

	static gp_segment_configuration_tuple tuple;

	config_tuple.t_data = &tuple;
	tuple.data.role = role;

	expect_any(systable_getnext, sysscan);
	will_return(systable_getnext, &config_tuple);

	Datum	   *configvals = palloc(sizeof(Datum) * Natts_gp_segment_configuration);

	configvals[Anum_gp_segment_configuration_status - 1] =
		CharGetDatum(status);
	configvals[Anum_gp_segment_configuration_mode - 1] =
		CharGetDatum(mode);

	HeapTuple	new_tuple = palloc(sizeof(HeapTupleData));

	expect_any(heap_modify_tuple, tuple);
	expect_any(heap_modify_tuple, tupleDesc);
	expect_check(heap_modify_tuple, replValues, CheckConfigVals, configvals);
	expect_any(heap_modify_tuple, replIsnull);
	expect_any(heap_modify_tuple, doReplace);
	will_return(heap_modify_tuple, new_tuple);

	expect_any_count(simple_heap_update, relation, 1);
	expect_any_count(simple_heap_update, otid, 1);
	expect_any_count(simple_heap_update, tup, 1);
	will_be_called_count(simple_heap_update, 1);

	expect_any_count(systable_endscan, sysscan, 1);
	will_be_called_count(systable_endscan, 1);
}

static void
ExpectedPrimaryAndMirrorConfiguration(CdbComponentDatabases *cdbs,
									  CdbComponentDatabaseInfo *primary,
									  char primaryStatus,
									  char mirrorStatus,
									  char mode,
					  char newPrimaryRole,
					  char newMirrorRole,
									  bool willUpdatePrimary,
									  bool willUpdateMirror)
{
	/* mock FtsIsActive true */
	MockFtsIsActive(true);

	/* mock probeWalRepUpdateConfig */
	if (willUpdatePrimary)
	{
		probeWalRepUpdateConfig_will_be_called_with(
													primary->dbid,
													primary->segindex,
													newPrimaryRole,
													primaryStatus,
													mode);
	}

	if (willUpdateMirror)
	{
		CdbComponentDatabaseInfo *mirror =
		FtsGetPeerSegment(cdbs, primary->segindex, primary->dbid);

		probeWalRepUpdateConfig_will_be_called_with(
													mirror->dbid,
													mirror->segindex,
													newMirrorRole,
													mirrorStatus,
													mode);
	}
}

static void
PrimaryOrMirrorWillBeUpdated(int count)
{
	will_be_called_count(StartTransactionCommand, count);
	will_be_called_count(GetTransactionSnapshot, count);
	will_be_called_count(CommitTransactionCommand, count);
}

/* 0 segments, is_updated is always false */
void
test_probeWalRepPublishUpdate_for_zero_segment(void **state)
{
	fts_context context;

	context.num_pairs = 0;

	bool is_updated = probeWalRepPublishUpdate(&context);

	assert_false(is_updated);
}

/*
 * 1 segment, is_updated is false, because FtsIsActive failed
 */
void
test_probeWalRepPublishUpdate_for_FtsIsActive_false(void **state)
{
	fts_context context;

	context.num_pairs = 1;
	per_segment_info ftsInfo;
	ftsInfo.state = FTS_PROBE_SUCCESS;

	context.perSegInfos = &ftsInfo;
	PGconn conn;

	InitContext(&context, &conn);

	CdbComponentDatabaseInfo info;

	ftsInfo.primary_cdbinfo = &info;
	info.role = GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;

	/* mock FtsIsActive false */
	MockFtsIsActive(false);

	bool is_updated = probeWalRepPublishUpdate(&context);

	assert_false(is_updated);
	/* Assert that no action was taken on ftsInfo object. */
	assert_true(ftsInfo.conn == &conn);
}

/*
 * 1 segment, is_updated is false, because shutdown_requested is true.
 */
void
test_probeWalRepPublishUpdate_for_shutdown_requested(void **state)
{
	fts_context context;

	context.num_pairs = 1;
	per_segment_info ftsInfo;
	ftsInfo.state = FTS_PROBE_SUCCESS;
	context.perSegInfos = &ftsInfo;
	PGconn conn;
	InitContext(&context, &conn);
	CdbComponentDatabaseInfo info;

	ftsInfo.primary_cdbinfo = &info;
	info.role = GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;

	/* mock FtsIsActive true */
	shutdown_requested = true;
	MockFtsIsActive(true);

	bool is_updated = probeWalRepPublishUpdate(&context);

	/* restore the original value to let the rest of the test pass */
	shutdown_requested = false;

	assert_false(is_updated);
	/* Assert that no action was taken on response object. */
	assert_true(ftsInfo.conn == &conn);
}

/*
 * 2 segments, is_updated is false, because neither primary nor mirror
 * state changed.
 */
void
test_PrimayUpMirrorUpNotInSync_to_PrimayUpMirrorUpNotInSync(void **state)
{
	fts_context context;
	CdbComponentDatabases *cdb_component_dbs;
	PGconn pgconn;

	cdb_component_dbs = InitTestCdb(2,
									true,
									GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);
	FtsWalRepInitProbeContext(cdb_component_dbs, &context);
	InitContext(&context, &pgconn);

	/* Primary must block commits as long as it and its mirror are alive. */
	context.perSegInfos[0].result.isPrimaryAlive = true;
	context.perSegInfos[0].result.isMirrorAlive = true;
	context.perSegInfos[0].result.isSyncRepEnabled = true;
	context.perSegInfos[0].state = FTS_PROBE_SUCCESS;
	context.perSegInfos[1].result.isPrimaryAlive = true;
	context.perSegInfos[1].result.isMirrorAlive = true;
	context.perSegInfos[1].result.isSyncRepEnabled = true;
	context.perSegInfos[1].state = FTS_PROBE_SUCCESS;

	/* Active connections must be closed after processing response. */
	expect_value_count(PQfinish, conn, &pgconn, 2);
	will_be_called_count(PQfinish, 2);

	/* probeWalRepPublishUpdate should not update a probe state */
	bool is_updated = probeWalRepPublishUpdate(&context);

	assert_true(context.perSegInfos[0].conn == NULL);
	assert_true(context.perSegInfos[1].conn == NULL);
	assert_false(is_updated);
	pfree(cdb_component_dbs);
}

/*
 * 2 segments, is_updated is false, because its double fault scenario
 * primary and mirror are not in sync hence cannot promote mirror, hence
 * current primary needs to stay marked as up.
 */
void
test_PrimayUpMirrorUpNotInSync_to_PrimaryDown(void **state)
{
	fts_context context;
	CdbComponentDatabases *cdb_component_dbs;
	PGconn pgconn;

	cdb_component_dbs = InitTestCdb(2,
									true,
									GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);
	FtsWalRepInitProbeContext(cdb_component_dbs, &context);
	InitContext(&context, &pgconn);

	/* No response received from first segment */
	context.perSegInfos[0].state = FTS_PROBE_FAILED;
	context.perSegInfos[0].retry_count = gp_fts_probe_retries;

	/* Response received from second segment */
	context.perSegInfos[1].result.isPrimaryAlive = true;
	context.perSegInfos[1].result.isMirrorAlive = true;
	context.perSegInfos[1].result.isSyncRepEnabled = true;
	context.perSegInfos[1].state = FTS_PROBE_SUCCESS;

	/* Active connections must be closed after processing response. */
	expect_value_count(PQfinish, conn, &pgconn, 2);
	will_be_called_count(PQfinish, 2);

	/* No update must happen */
	bool is_updated = probeWalRepPublishUpdate(&context);

	assert_true(context.perSegInfos[0].conn == NULL);
	assert_true(context.perSegInfos[1].conn == NULL);
	assert_false(is_updated);
	pfree(cdb_component_dbs);
}

/*
 * 2 segments, is_updated is true, because content 0 mirror is updated
 */
void
test_PrimayUpMirrorUpNotInSync_to_PrimaryUpMirrorDownNotInSync(void **state)
{
	fts_context context;
	CdbComponentDatabases *cdb_component_dbs;
	PGconn pgconn;

	cdb_component_dbs = InitTestCdb(2,
									true,
									GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);

	FtsWalRepInitProbeContext(cdb_component_dbs, &context);
	InitContext(&context, &pgconn);

	/*
	 * Test response received from both segments. First primary's mirror is
	 * reported as DOWN.
	 */
	context.perSegInfos[0].result.isPrimaryAlive = true;
	context.perSegInfos[0].result.isMirrorAlive = false;
	context.perSegInfos[0].state = FTS_PROBE_SUCCESS;

	/* Syncrep must be enabled because mirror is up. */
	context.perSegInfos[1].result.isPrimaryAlive = true;
	context.perSegInfos[1].result.isMirrorAlive = true;
	context.perSegInfos[1].result.isSyncRepEnabled = true;
	context.perSegInfos[1].state = FTS_PROBE_SUCCESS;

	/* the mirror will be updated */
	PrimaryOrMirrorWillBeUpdated(1);
	ExpectedPrimaryAndMirrorConfiguration(
		cdb_component_dbs,
		context.perSegInfos[0].primary_cdbinfo, /* primary */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* primary status */
		GP_SEGMENT_CONFIGURATION_STATUS_DOWN, /* mirror status */
		GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC, /* mode */
		GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY, /* newPrimaryRole */
		GP_SEGMENT_CONFIGURATION_ROLE_MIRROR, /* newMirrorRole */
		false, /* willUpdatePrimary */
		true /* willUpdateMirror */);

	/* Active connections must be closed after processing response. */
	expect_value_count(PQfinish, conn, &pgconn, 2);
	will_be_called_count(PQfinish, 2);

	bool is_updated = probeWalRepPublishUpdate(&context);

	assert_true(is_updated);
	assert_true(context.perSegInfos[0].conn == NULL);
	assert_true(context.perSegInfos[1].conn == NULL);
	pfree(cdb_component_dbs);
}

/*
 * 3 segments, is_updated is true, because content 0 mirror is down and
 * probe response is up
 */
void
test_PrimaryUpMirrorDownNotInSync_to_PrimayUpMirrorUpNotInSync(void **state)
{
	fts_context context;
	CdbComponentDatabases *cdb_component_dbs;
	PGconn pgconn;

	cdb_component_dbs = InitTestCdb(3,
									true,
									GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);

	/* set the mirror down in config */
	CdbComponentDatabaseInfo *cdbinfo =
		GetSegmentFromCdbComponentDatabases(
			cdb_component_dbs, 0, GP_SEGMENT_CONFIGURATION_ROLE_MIRROR);

	cdbinfo->status = GP_SEGMENT_CONFIGURATION_STATUS_DOWN;

	FtsWalRepInitProbeContext(cdb_component_dbs, &context);
	InitContext(&context, &pgconn);

	/*
	 * Response received from all three segments, DOWN mirror is reported UP
	 * for first primary.
	 */
	context.perSegInfos[0].result.isPrimaryAlive = true;
	context.perSegInfos[0].result.isMirrorAlive = true;
	context.perSegInfos[0].result.isSyncRepEnabled = true;
	context.perSegInfos[0].state = FTS_PROBE_SUCCESS;

	/* no change */
	context.perSegInfos[1].result.isPrimaryAlive = true;
	context.perSegInfos[1].result.isMirrorAlive = true;
	context.perSegInfos[1].result.isSyncRepEnabled = true;
	context.perSegInfos[1].state = FTS_PROBE_SUCCESS;
	context.perSegInfos[2].result.isPrimaryAlive = true;
	context.perSegInfos[2].result.isMirrorAlive = true;
	context.perSegInfos[2].result.isSyncRepEnabled = true;
	context.perSegInfos[2].state = FTS_PROBE_SUCCESS;

	/* the mirror will be updated */
	PrimaryOrMirrorWillBeUpdated(1);
	ExpectedPrimaryAndMirrorConfiguration(
		cdb_component_dbs,
		context.perSegInfos[0].primary_cdbinfo, /* primary */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* primary status */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* mirror status */
		GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC, /* mode */
		GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY, /* newPrimaryRole */
		GP_SEGMENT_CONFIGURATION_ROLE_MIRROR, /* newMirrorRole */
		false, /* willUpdatePrimary */
		true /* willUpdateMirror */);

	/* Active connections must be closed after processing response. */
	expect_value_count(PQfinish, conn, &pgconn, 3);
	will_be_called_count(PQfinish, 3);

	bool is_updated = probeWalRepPublishUpdate(&context);

	assert_true(is_updated);
	/*
	 * Assert that connections are closed and the state of the segments is not
	 * changed (no further messages needed from FTS).
	 */
	assert_true(context.perSegInfos[0].conn == NULL);
	assert_true(context.perSegInfos[0].state == FTS_PROBE_SUCCESS);
	assert_true(context.perSegInfos[1].conn == NULL);
	assert_true(context.perSegInfos[1].state == FTS_PROBE_SUCCESS);
	assert_true(context.perSegInfos[2].conn == NULL);
	assert_true(context.perSegInfos[2].state == FTS_PROBE_SUCCESS);
	pfree(cdb_component_dbs);
}

/*
 * 5 segments, is_updated is true, as we are changing the state of several
 * segment pairs.  This test also validates that sync-rep off and promotion
 * messages are not blocked by primary retry requests.
 */
void
test_probeWalRepPublishUpdate_multiple_segments(void **state)
{
	fts_context context;
	CdbComponentDatabases *cdb_component_dbs;
	PGconn pgconn;

	cdb_component_dbs = InitTestCdb(5,
									true,
									GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);

	/*
	 * Mark the mirror for content 0 down in configuration, probe response
	 * indicates it's up.
	 */
	CdbComponentDatabaseInfo *cdbinfo =
		GetSegmentFromCdbComponentDatabases(
			cdb_component_dbs, 0, GP_SEGMENT_CONFIGURATION_ROLE_MIRROR);

	cdbinfo->status = GP_SEGMENT_CONFIGURATION_STATUS_DOWN;

	FtsWalRepInitProbeContext(cdb_component_dbs, &context);
	InitContext(&context, &pgconn);

	/* First segment DOWN mirror, now reported UP */
	context.perSegInfos[0].result.isPrimaryAlive = true;
	context.perSegInfos[0].result.isMirrorAlive = true;
	context.perSegInfos[0].result.isSyncRepEnabled = true;
	context.perSegInfos[0].state = FTS_PROBE_SUCCESS;

	/*
	 * Mark the primary-mirror pair for content 1 as in-sync in configuration
	 * so that the mirror can be promoted.
	 */
	cdbinfo = GetSegmentFromCdbComponentDatabases(
		cdb_component_dbs, 1, GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY);
	cdbinfo->mode = GP_SEGMENT_CONFIGURATION_MODE_INSYNC;
	cdbinfo = GetSegmentFromCdbComponentDatabases(
		cdb_component_dbs, 1, GP_SEGMENT_CONFIGURATION_ROLE_MIRROR);
	cdbinfo->mode = GP_SEGMENT_CONFIGURATION_MODE_INSYNC;

	/* Second segment no response received, mirror will be promoted */
	context.perSegInfos[1].state = FTS_PROBE_FAILED;
	context.perSegInfos[1].retry_count = gp_fts_probe_retries;

	/* Third segment UP mirror, now reported DOWN */
	context.perSegInfos[2].result.isPrimaryAlive = true;
	context.perSegInfos[2].result.isSyncRepEnabled = true;
	context.perSegInfos[2].result.isMirrorAlive = false;
	context.perSegInfos[2].state = FTS_PROBE_SUCCESS;

	/* Fourth segment, response received no change */
	context.perSegInfos[3].result.isPrimaryAlive = true;
	context.perSegInfos[3].result.isSyncRepEnabled = true;
	context.perSegInfos[3].result.isMirrorAlive = true;
	context.perSegInfos[3].state = FTS_PROBE_SUCCESS;

	/* Fifth segment, probe failed but retries not exhausted */
	context.perSegInfos[4].result.isPrimaryAlive = false;
	context.perSegInfos[4].result.isSyncRepEnabled = false;
	context.perSegInfos[4].result.isMirrorAlive = false;
	context.perSegInfos[4].state = FTS_PROBE_RETRY_WAIT;

	/* we are updating three of the four segments */
	PrimaryOrMirrorWillBeUpdated(3);

	/* First segment */
	ExpectedPrimaryAndMirrorConfiguration(
		cdb_component_dbs,
		context.perSegInfos[0].primary_cdbinfo, /* primary */
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
		cdb_component_dbs,
		context.perSegInfos[1].primary_cdbinfo, /* primary */
		GP_SEGMENT_CONFIGURATION_STATUS_DOWN, /* primary status */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* mirror status */
		GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC, /* mode */
		GP_SEGMENT_CONFIGURATION_ROLE_MIRROR, /* newPrimaryRole */
		GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY, /* newMirrorRole */
		true, /* willUpdatePrimary */
		true /* willUpdateMirror */);

	/* Third segment */
	ExpectedPrimaryAndMirrorConfiguration(
		cdb_component_dbs,
		context.perSegInfos[2].primary_cdbinfo, /* primary */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* primary status */
		GP_SEGMENT_CONFIGURATION_STATUS_DOWN, /* mirror status */
		GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC, /* mode */
		GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY, /* newPrimaryRole */
		GP_SEGMENT_CONFIGURATION_ROLE_MIRROR, /* newMirrorRole */
		false, /* willUpdatePrimary */
		true /* willUpdateMirror */);

	/* Fourth segment will not change status */

	/* Active connections must be closed after processing response. */
	expect_value_count(PQfinish, conn, &pgconn, 4);
	will_be_called_count(PQfinish, 4);

	bool is_updated = probeWalRepPublishUpdate(&context);

	assert_true(is_updated);
	/* mirror found up */
	assert_true(context.perSegInfos[0].state == FTS_PROBE_SUCCESS);
	/* mirror promotion should be triggered */
	assert_true(context.perSegInfos[1].state == FTS_PROMOTE_SEGMENT);
	/* mirror found down, must turn off syncrep on primary */
	assert_true(context.perSegInfos[2].state == FTS_SYNCREP_SEGMENT);
	/* no change in configuration */
	assert_true(context.perSegInfos[3].state == FTS_PROBE_SUCCESS);
	/* retry possible, final state is not yet reached */
	assert_true(context.perSegInfos[4].state == FTS_PROBE_RETRY_WAIT);
	pfree(cdb_component_dbs);
}

/*
 * 1 segment, is_updated is true, because primary and mirror will be
 * marked not in sync
 */
void
test_PrimayUpMirrorUpSync_to_PrimaryUpMirrorUpNotInSync(void **state)
{
	fts_context context;
	CdbComponentDatabases *cdb_component_dbs;
	PGconn pgconn;

	/* Start in SYNC state */
	cdb_component_dbs = InitTestCdb(1,
									true,
									GP_SEGMENT_CONFIGURATION_MODE_INSYNC);

	FtsWalRepInitProbeContext(cdb_component_dbs, &context);
	InitContext(&context, &pgconn);

	/* Probe responded with Mirror Up and Not In SYNC with syncrep enabled */
	context.perSegInfos[0].result.isPrimaryAlive = true;
	context.perSegInfos[0].result.isMirrorAlive = true;
	context.perSegInfos[0].result.isInSync = false;
	context.perSegInfos[0].result.isSyncRepEnabled = true;
	context.perSegInfos[0].state = FTS_PROBE_SUCCESS;

	/* we are updating one segment pair */
	PrimaryOrMirrorWillBeUpdated(1);

	ExpectedPrimaryAndMirrorConfiguration(
		cdb_component_dbs,
		context.perSegInfos[0].primary_cdbinfo, /* primary */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* primary status */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* mirror status */
		GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC, /* mode */
		GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY, /* newPrimaryRole */
		GP_SEGMENT_CONFIGURATION_ROLE_MIRROR, /* newMirrorRole */
		true, /* willUpdatePrimary */
		true /* willUpdateMirror */);

	/* Active connections must be closed after processing response. */
	expect_value(PQfinish, conn, &pgconn);
	will_be_called(PQfinish);

	bool is_updated = probeWalRepPublishUpdate(&context);

	assert_true(is_updated);
	assert_true(context.perSegInfos[0].state == FTS_PROBE_SUCCESS);
	assert_true(context.perSegInfos[0].conn == NULL);
	pfree(cdb_component_dbs);
}

/*
 * 2 segments, is_updated is true, because mirror will be marked down and
 * both will be marked not in sync for first primary mirror pair
 */
void
test_PrimayUpMirrorUpSync_to_PrimaryUpMirrorDownNotInSync(void **state)
{
	fts_context context;
	CdbComponentDatabases *cdb_component_dbs;
	PGconn pgconn;

	/* Start in SYNC mode */
	cdb_component_dbs = InitTestCdb(2,
									true,
									GP_SEGMENT_CONFIGURATION_MODE_INSYNC);

	FtsWalRepInitProbeContext(cdb_component_dbs, &context);
	InitContext(&context, &pgconn);

	/*
	 * Probe responded with one mirror down and not in-sync, and syncrep
	 * enabled on both primaries.
	 */
	context.perSegInfos[0].result.isPrimaryAlive = true;
	context.perSegInfos[0].result.isMirrorAlive = false;
	context.perSegInfos[0].result.isInSync = false;
	context.perSegInfos[0].result.isSyncRepEnabled = true;
	context.perSegInfos[0].state = FTS_PROBE_SUCCESS;
	context.perSegInfos[1].result.isPrimaryAlive = true;
	context.perSegInfos[1].result.isMirrorAlive = true;
	context.perSegInfos[1].result.isInSync = true;
	context.perSegInfos[1].result.isSyncRepEnabled = true;
	context.perSegInfos[1].state = FTS_PROBE_SUCCESS;

	/* we are updating one segment pair */
	PrimaryOrMirrorWillBeUpdated(1);

	ExpectedPrimaryAndMirrorConfiguration(
		cdb_component_dbs,
		context.perSegInfos[0].primary_cdbinfo, /* primary */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* primary status */
		GP_SEGMENT_CONFIGURATION_STATUS_DOWN, /* mirror status */
		GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC, /* mode */
		GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY, /* newPrimaryRole */
		GP_SEGMENT_CONFIGURATION_ROLE_MIRROR, /* newMirrorRole */
		true, /* willUpdatePrimary */
		true /* willUpdateMirror */);

	/* Active connections must be closed after processing response. */
	expect_value_count(PQfinish, conn, &pgconn, 2);
	will_be_called_count(PQfinish, 2);

	bool is_updated = probeWalRepPublishUpdate(&context);

	assert_true(is_updated);
	/* mirror is down but syncrep is enabled, so it must be turned off */
	assert_true(context.perSegInfos[0].state == FTS_SYNCREP_SEGMENT);
	/* no change in config */
	assert_true(context.perSegInfos[1].state == FTS_PROBE_SUCCESS);
	assert_true(context.perSegInfos[0].conn == NULL);
	assert_true(context.perSegInfos[1].conn == NULL);
	pfree(cdb_component_dbs);
}

/*
 * 1 segment, is_updated is true, because FTS found primary goes down and
 * both will be marked not in sync, then FTS promote mirror
 */
void
test_PrimayUpMirrorUpSync_to_PrimaryDown_to_MirrorPromote(void **state)
{
	fts_context context;
	CdbComponentDatabases *cdb_component_dbs;
	PGconn pgconn;

	/* set the mode to SYNC in config */
	cdb_component_dbs = InitTestCdb(1,
									true,
									GP_SEGMENT_CONFIGURATION_MODE_INSYNC);

	FtsWalRepInitProbeContext(cdb_component_dbs, &context);
	InitContext(&context, &pgconn);

	/* Probe response was not received. */
	context.perSegInfos[0].state = FTS_PROBE_FAILED;
	context.perSegInfos[0].retry_count = gp_fts_probe_retries;
	/* Store reference to mirror object for validation later. */
	CdbComponentDatabaseInfo *mirror = context.perSegInfos[0].mirror_cdbinfo;

	/* we are updating one segment pair */
	PrimaryOrMirrorWillBeUpdated(1);

	ExpectedPrimaryAndMirrorConfiguration(
		cdb_component_dbs,
		context.perSegInfos[0].primary_cdbinfo, /* primary */
		GP_SEGMENT_CONFIGURATION_STATUS_DOWN, /* primary status */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* mirror status */
		GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC, /* mode */
		GP_SEGMENT_CONFIGURATION_ROLE_MIRROR, /* newPrimaryRole */
		GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY, /* newMirrorRole */
		true, /* willUpdatePrimary */
		true /* willUpdateMirror */);

	/* Active connections must be closed after processing response. */
	expect_value(PQfinish, conn, &pgconn);
	will_be_called(PQfinish);

	bool is_updated = probeWalRepPublishUpdate(&context);

	assert_true(is_updated);
	/* the mirror must be marked for promotion */
	assert_true(context.perSegInfos[0].state == FTS_PROMOTE_SEGMENT);
	assert_int_equal(context.perSegInfos[0].primary_cdbinfo->dbid, mirror->dbid);
	assert_true(context.perSegInfos[0].conn == NULL);
	pfree(cdb_component_dbs);
}

/*
 * 1 segment, is_updated is true, because primary and mirror will be
 * marked in sync
 */
void
test_PrimayUpMirrorUpNotInSync_to_PrimayUpMirrorUpSync(void **state)
{
	fts_context context;
	CdbComponentDatabases *cdb_component_dbs;
	PGconn pgconn;

	cdb_component_dbs = InitTestCdb(1,
									true,
									GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);

	FtsWalRepInitProbeContext(cdb_component_dbs, &context);
	InitContext(&context, &pgconn);

	/* Probe responded with Mirror Up and SYNC */
	context.perSegInfos[0].result.isPrimaryAlive = true;
	context.perSegInfos[0].result.isMirrorAlive = true;
	context.perSegInfos[0].result.isInSync = true;
	context.perSegInfos[0].result.isSyncRepEnabled = true;
	context.perSegInfos[0].state = FTS_PROBE_SUCCESS;

	/* we are updating one segment pair */
	PrimaryOrMirrorWillBeUpdated(1);

	ExpectedPrimaryAndMirrorConfiguration(
		cdb_component_dbs,
		context.perSegInfos[0].primary_cdbinfo, /* primary */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* primary status */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* mirror status */
		GP_SEGMENT_CONFIGURATION_MODE_INSYNC, /* mode */
		GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY, /* newPrimaryRole */
		GP_SEGMENT_CONFIGURATION_ROLE_MIRROR, /* newMirrorRole */
		true, /* willUpdatePrimary */
		true /* willUpdateMirror */);

	/* Active connections must be closed after processing response. */
	expect_value(PQfinish, conn, &pgconn);
	will_be_called(PQfinish);

	bool is_updated = probeWalRepPublishUpdate(&context);

	assert_true(is_updated);
	assert_true(context.perSegInfos[0].state == FTS_PROBE_SUCCESS);
	assert_true(context.perSegInfos[0].conn == NULL);
	pfree(cdb_component_dbs);
}

/*
 * 1 segment, is_updated is true, because mirror will be marked UP and
 * both primary and mirror should get updated to SYNC
 */
void
test_PrimaryUpMirrorDownNotInSync_to_PrimayUpMirrorUpSync(void **state)
{
	fts_context context;
	CdbComponentDatabases *cdb_component_dbs;
	PGconn pgconn;

	cdb_component_dbs = InitTestCdb(1,
									true,
									GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);

	/* set the mirror down in config */
	CdbComponentDatabaseInfo *cdbinfo =
		GetSegmentFromCdbComponentDatabases(
			cdb_component_dbs, 0, GP_SEGMENT_CONFIGURATION_ROLE_MIRROR);

	cdbinfo->status = GP_SEGMENT_CONFIGURATION_STATUS_DOWN;

	FtsWalRepInitProbeContext(cdb_component_dbs, &context);
	InitContext(&context, &pgconn);

	/* Probe responded with Mirror Up and SYNC */
	context.perSegInfos[0].result.isPrimaryAlive = true;
	context.perSegInfos[0].result.isMirrorAlive = true;
	context.perSegInfos[0].result.isInSync = true;
	context.perSegInfos[0].result.isSyncRepEnabled = true;
	context.perSegInfos[0].state = FTS_PROBE_SUCCESS;

	/* we are updating one segment pair */
	PrimaryOrMirrorWillBeUpdated(1);

	ExpectedPrimaryAndMirrorConfiguration(
		cdb_component_dbs,
		context.perSegInfos[0].primary_cdbinfo, /* primary */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* primary status */
		GP_SEGMENT_CONFIGURATION_STATUS_UP, /* mirror status */
		GP_SEGMENT_CONFIGURATION_MODE_INSYNC, /* mode */
		GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY, /* newPrimaryRole */
		GP_SEGMENT_CONFIGURATION_ROLE_MIRROR, /* newMirrorRole */
		true, /* willUpdatePrimary */
		true /* willUpdateMirror */);

	/* Active connections must be closed after processing response. */
	expect_value(PQfinish, conn, &pgconn);
	will_be_called(PQfinish);

	bool is_updated = probeWalRepPublishUpdate(&context);

	assert_true(is_updated);
	assert_true(context.perSegInfos[0].conn == NULL);
	assert_true(context.perSegInfos[0].state == FTS_PROBE_SUCCESS);
	pfree(cdb_component_dbs);
}

/*
 * 1 segment, is_updated is false, because there is no status or mode change.
 */
void
test_PrimaryUpMirrorDownNotInSync_to_PrimayUpMirrorDownNotInSync(void **state)
{
	fts_context context;
	CdbComponentDatabases *cdb_component_dbs;

	cdb_component_dbs = InitTestCdb(1,
									true,
									GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);

	/* set the mirror down in config */
	CdbComponentDatabaseInfo *cdbinfo =
	GetSegmentFromCdbComponentDatabases(
										cdb_component_dbs, 0, GP_SEGMENT_CONFIGURATION_ROLE_MIRROR);

	cdbinfo->status = GP_SEGMENT_CONFIGURATION_STATUS_DOWN;

	FtsWalRepInitProbeContext(cdb_component_dbs, &context);

	/* Probe responded with Mirror Up and SYNC */
	context.perSegInfos[0].result.isPrimaryAlive = true;
	context.perSegInfos[0].result.isMirrorAlive = false;
	context.perSegInfos[0].result.isInSync = false;

	bool is_updated = probeWalRepPublishUpdate(&context);

	/* assert_false(FtsWalRepSetupMessageContext(&context)); */
	assert_false(is_updated);
	pfree(cdb_component_dbs);
}

/*
 * 2 segments, is_updated is false, because content 0 mirror is already
 * down and probe response fails. Means double fault scenario.
 */
void
test_PrimaryUpMirrorDownNotInSync_to_PrimaryDown(void **state)
{
	fts_context context;
	CdbComponentDatabases *cdb_component_dbs;
	PGconn pgconn;

	cdb_component_dbs = InitTestCdb(2,
									true,
									GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);

	/* set the mirror down in config */
	CdbComponentDatabaseInfo *cdbinfo =
		GetSegmentFromCdbComponentDatabases(
			cdb_component_dbs, 0, GP_SEGMENT_CONFIGURATION_ROLE_MIRROR);

	cdbinfo->status = GP_SEGMENT_CONFIGURATION_STATUS_DOWN;

	FtsWalRepInitProbeContext(cdb_component_dbs, &context);
	InitContext(&context, &pgconn);

	/* No response received from segment 1 (content 0 primary) */
	context.perSegInfos[0].state = FTS_PROBE_FAILED;
	context.perSegInfos[0].retry_count = gp_fts_probe_retries;

	/* No change for segment 2, probe successful */
	context.perSegInfos[1].result.isPrimaryAlive = true;
	context.perSegInfos[1].result.isSyncRepEnabled = true;
	context.perSegInfos[1].result.isMirrorAlive = true;
	context.perSegInfos[1].state = FTS_PROBE_SUCCESS;

	/* Active connections must be closed after processing response. */
	expect_value_count(PQfinish, conn, &pgconn, 2);
	will_be_called_count(PQfinish, 2);

	bool is_updated = probeWalRepPublishUpdate(&context);

	assert_false(is_updated);
	pfree(cdb_component_dbs);
}

/*
 * 1 segment, probe times out, is_updated = false because it's a double fault.
 */
void
test_probeTimeout(void **state)
{
	fts_context context;
	CdbComponentDatabases *cdb_component_dbs;
	PGconn pgconn;

	cdb_component_dbs = InitTestCdb(1,
									true,
									GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);
	FtsWalRepInitProbeContext(cdb_component_dbs, &context);

	context.perSegInfos[0].startTime =
		(pg_time_t) time(NULL) - gp_fts_probe_timeout - 1;
	context.perSegInfos[0].state = FTS_PROBE_SEGMENT;
	context.perSegInfos[0].conn = &pgconn;

	expect_value(PQfinish, conn, &pgconn);
	will_be_called(PQfinish);

	bool is_updated = probeWalRepPublishUpdate(&context);
	/* double fault, state remains failed due to timeout */
	assert_false(is_updated);
	assert_true(context.perSegInfos[0].state == FTS_PROBE_FAILED);
	pfree(cdb_component_dbs);
}

void
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
		assert_true(context.perSegInfos[i].result.dbid == context.perSegInfos[i].primary_cdbinfo->dbid);
		assert_false(context.perSegInfos[i].result.isPrimaryAlive);
		assert_false(context.perSegInfos[i].result.isMirrorAlive);
		assert_false(context.perSegInfos[i].result.isInSync);
	}
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		/* -----------------------------------------------------------------------
		 * Group of tests for probeWalRepPublishUpdate()
		 * -----------------------------------------------------------------------
		 */
		unit_test(test_probeWalRepPublishUpdate_for_zero_segment),
		unit_test(test_probeWalRepPublishUpdate_for_shutdown_requested),
		unit_test(test_probeWalRepPublishUpdate_for_FtsIsActive_false),
		unit_test(test_probeWalRepPublishUpdate_multiple_segments),

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
		unit_test(test_FtsWalRepInitProbeContext_initial_state),
		unit_test(test_probeTimeout)
		/*-----------------------------------------------------------------------*/
	};

	MemoryContextInit();
	InitFtsProbeInfo();

	return run_tests(tests);
}
