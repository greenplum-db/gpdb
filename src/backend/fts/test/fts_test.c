#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

/* Actual function body */
#include "../fts.c"

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
	cdb->my_dbid = 1;
	cdb->my_segindex = -1;
	cdb->my_isprimary = true;
	cdb->entry_db_info = palloc(
								sizeof(CdbComponentDatabaseInfo) * cdb->total_entry_dbs);
	cdb->segment_db_info = palloc(
								  sizeof(CdbComponentDatabaseInfo) * cdb->total_segment_dbs);


	/* create the master entry_db_info */
	CdbComponentDatabaseInfo *cdbinfo = &cdb->entry_db_info[0];

	cdbinfo->dbid = 1;
	cdbinfo->segindex = '-1';

	cdbinfo->role = GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;
	cdbinfo->preferred_role = GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;
	cdbinfo->status = GP_SEGMENT_CONFIGURATION_STATUS_UP;

	/* create the segment_db_info entries */
	for (i = 0; i < cdb->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *cdbinfo = &cdb->segment_db_info[i];


		cdbinfo->dbid = i + 2;
		cdbinfo->segindex = i / 2;

		if (has_mirrors)
		{
			cdbinfo->role = i % 2 ?
				GP_SEGMENT_CONFIGURATION_ROLE_MIRROR :
				GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;

			cdbinfo->preferred_role = i % 2 ?
				GP_SEGMENT_CONFIGURATION_ROLE_MIRROR :
				GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;
		}
		else
		{
			cdbinfo->role = GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;
			cdbinfo->preferred_role = GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;
		}
		cdbinfo->status = GP_SEGMENT_CONFIGURATION_STATUS_UP;
		cdbinfo->mode = default_mode;
	}

	return cdb;
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
	static FtsProbeInfo fts_info;

	ftsProbeInfo = &fts_info;

	if (shutdown_requested)
	{
		ftsProbeInfo->fts_discardResults = false;
	}
	else
	{
		ftsProbeInfo->fts_discardResults = !expected_return_value;
	}
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

	context.num_primary_segments = 0;

	bool is_updated = probeWalRepPublishUpdate(NULL, &context);

	assert_false(is_updated);
	MockFtsIsActive(true);
	assert_false(FtsWalRepSetupMessageContext(&context));
}

/*
 * 1 segment, is_updated is false, because FtsIsActive failed
 */
void
test_probeWalRepPublishUpdate_for_FtsIsActive_false(void **state)
{
	fts_context context;

	context.num_primary_segments = 1;
	probe_response_per_segment response;

	context.responses = &response;
	CdbComponentDatabaseInfo info;

	response.segment_db_info = &info;
	info.role = GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;

	/* mock FtsIsActive false */
	MockFtsIsActive(false);

	bool is_updated = probeWalRepPublishUpdate(NULL, &context);

	assert_false(is_updated);
	assert_false(FtsWalRepSetupMessageContext(&context));
}

/*
 * 1 segment, is_updated is false, because shutdown_requested is true.
 */
void
test_probeWalRepPublishUpdate_for_shutdown_requested(void **state)
{
	fts_context context;

	context.num_primary_segments = 1;
	probe_response_per_segment response;

	context.responses = &response;
	CdbComponentDatabaseInfo info;

	response.segment_db_info = &info;
	info.role = GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY;

	/* mock FtsIsActive true */
	shutdown_requested = true;
	MockFtsIsActive(true);

	bool is_updated = probeWalRepPublishUpdate(NULL, &context);

	/* restore the original value to let the rest of the test pass */
	shutdown_requested = false;

	assert_false(is_updated);
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

	cdb_component_dbs = InitTestCdb(2,
									true,
									GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);
	FtsWalRepInitProbeContext(cdb_component_dbs, &context);

	/* Primary must block commits as long as it and its mirror are alive. */
	context.responses[0].result.isPrimaryAlive = true;
	context.responses[0].result.isSyncRepEnabled = true;
	context.responses[1].result.isPrimaryAlive = true;
	context.responses[1].result.isSyncRepEnabled = true;

	/* probeWalRepPublishUpdate should not update a probe state */
	bool is_updated = probeWalRepPublishUpdate(cdb_component_dbs, &context);

	assert_false(is_updated);
	assert_false(FtsWalRepSetupMessageContext(&context));
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

	cdb_component_dbs = InitTestCdb(2,
									true,
									GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);
	FtsWalRepInitProbeContext(cdb_component_dbs, &context);

	/* No response received from first segment */

	/* Response received from second segment */
	context.responses[1].result.isPrimaryAlive = true;
	context.responses[1].result.isSyncRepEnabled = true;

	/* No update must happen */
	bool is_updated = probeWalRepPublishUpdate(cdb_component_dbs, &context);

	assert_false(is_updated);
	assert_false(FtsWalRepSetupMessageContext(&context));
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

	cdb_component_dbs = InitTestCdb(2,
									true,
									GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);

	FtsWalRepInitProbeContext(cdb_component_dbs, &context);

	/*
	 * Test response received from both segments. First primary's mirror is
	 * reported as DOWN.
	 */
	context.responses[0].result.isPrimaryAlive = true;
	context.responses[0].result.isMirrorAlive = false;

	/* Syncrep must be enabled because mirror is up. */
	context.responses[1].result.isPrimaryAlive = true;
	context.responses[1].result.isSyncRepEnabled = true;

	/* the mirror will be updated */
	PrimaryOrMirrorWillBeUpdated(1);
	ExpectedPrimaryAndMirrorConfiguration(cdb_component_dbs,
										   /* primary */ context.responses[0].segment_db_info,
										   /* primary status */ GP_SEGMENT_CONFIGURATION_STATUS_UP,
										   /* mirror status */ GP_SEGMENT_CONFIGURATION_STATUS_DOWN,
										   /* mode */ GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC,
										   /* newPrimaryRole */ GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY,
										   /* newMirrorRole */ GP_SEGMENT_CONFIGURATION_ROLE_MIRROR,
										   /* willUpdatePrimary */ false,
										   /* willUpdateMirror */ true);

	bool is_updated = probeWalRepPublishUpdate(cdb_component_dbs, &context);

	assert_false(FtsWalRepSetupMessageContext(&context));
	assert_true(is_updated);
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

	cdb_component_dbs = InitTestCdb(3,
									true,
									GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);

	/* set the mirror down in config */
	CdbComponentDatabaseInfo *cdbinfo =
	GetSegmentFromCdbComponentDatabases(
										cdb_component_dbs, 0, GP_SEGMENT_CONFIGURATION_ROLE_MIRROR);

	cdbinfo->status = GP_SEGMENT_CONFIGURATION_STATUS_DOWN;

	FtsWalRepInitProbeContext(cdb_component_dbs, &context);

	/*
	 * Response received from all three segments, DOWN mirror is reported UP
	 * for first primary.
	 */
	context.responses[0].result.isPrimaryAlive = true;
	context.responses[0].result.isMirrorAlive = true;
	context.responses[0].isScheduled = true;
	context.responses[0].result.isSyncRepEnabled = true;

	/* no change */
	context.responses[1].result.isPrimaryAlive = true;
	context.responses[1].result.isSyncRepEnabled = true;
	context.responses[2].result.isPrimaryAlive = true;
	context.responses[2].result.isSyncRepEnabled = true;

	/* the mirror will be updated */
	PrimaryOrMirrorWillBeUpdated(1);
	ExpectedPrimaryAndMirrorConfiguration(cdb_component_dbs,
										   /* primary */ context.responses[0].segment_db_info,
										   /* primary status */ GP_SEGMENT_CONFIGURATION_STATUS_UP,
										   /* mirror status */ GP_SEGMENT_CONFIGURATION_STATUS_UP,
										   /* mode */ GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC,
										   /* newPrimaryRole */ GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY,
										   /* newMirrorRole */ GP_SEGMENT_CONFIGURATION_ROLE_MIRROR,
										   /* willUpdatePrimary */ false,
										   /* willUpdateMirror */ true);

	bool is_updated = probeWalRepPublishUpdate(cdb_component_dbs, &context);

	/* we do not expect to send syncrep off libpq message */
	assert_false(FtsWalRepSetupMessageContext(&context));
	assert_true(context.responses[0].isScheduled);

	assert_true(is_updated);
	pfree(cdb_component_dbs);
}

/*
 * 4 segments, is_updated is true, as we are changing the state of
 * several segment pairs.
 */
void
test_probeWalRepPublishUpdate_multiple_segments(void **state)
{
	fts_context context;
	CdbComponentDatabases *cdb_component_dbs;

	cdb_component_dbs = InitTestCdb(4,
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

	/* First segment DOWN mirror, now reported UP */
	context.responses[0].result.isPrimaryAlive = true;
	context.responses[0].result.isMirrorAlive = true;
	context.responses[0].result.isSyncRepEnabled = true;

	/* Second segment no response received */

	/* Third segment UP mirror, now reported DOWN */
	context.responses[2].result.isPrimaryAlive = true;
	context.responses[2].result.isMirrorAlive = false;

	/* Fourth segment, response received no change */
	context.responses[3].result.isPrimaryAlive = true;
	context.responses[3].result.isSyncRepEnabled = true;

	/* we are updating two of the four segments */
	PrimaryOrMirrorWillBeUpdated(2);

	/* First segment */
	ExpectedPrimaryAndMirrorConfiguration(cdb_component_dbs,
										   /* primary */ context.responses[0].segment_db_info,
										   /* primary status */ GP_SEGMENT_CONFIGURATION_STATUS_UP,
										   /* mirror status */ GP_SEGMENT_CONFIGURATION_STATUS_UP,
										   /* mode */ GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC,
										   /* newPrimaryRole */ GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY,
										   /* newMirrorRole */ GP_SEGMENT_CONFIGURATION_ROLE_MIRROR,
										   /* willUpdatePrimary */ false,
										   /* willUpdateMirror */ true);

	/*
	 * Second segment acts as double fault as not in sync and primary probe
	 * failed
	 */
	/* Third segment */
	ExpectedPrimaryAndMirrorConfiguration(cdb_component_dbs,
										   /* primary */ context.responses[2].segment_db_info,
										   /* primary status */ GP_SEGMENT_CONFIGURATION_STATUS_UP,
										   /* mirror status */ GP_SEGMENT_CONFIGURATION_STATUS_DOWN,
										   /* mode */ GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC,
										   /* newPrimaryRole */ GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY,
										   /* newMirrorRole */ GP_SEGMENT_CONFIGURATION_ROLE_MIRROR,
										   /* willUpdatePrimary */ false,
										   /* willUpdateMirror */ true);
	/* Fourth segment will not change status */

	bool is_updated = probeWalRepPublishUpdate(cdb_component_dbs, &context);

	assert_false(FtsWalRepSetupMessageContext(&context));
	assert_true(is_updated);
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

	/* Start in SYNC state */
	cdb_component_dbs = InitTestCdb(1,
									true,
									GP_SEGMENT_CONFIGURATION_MODE_INSYNC);

	FtsWalRepInitProbeContext(cdb_component_dbs, &context);

	/* Probe responded with Mirror Up and Not In SYNC with syncrep enabled */
	context.responses[0].result.isPrimaryAlive = true;
	context.responses[0].result.isInSync = false;
	context.responses[0].result.isSyncRepEnabled = true;
	context.responses[0].isScheduled = true;

	/* we are updating one segment pair */
	PrimaryOrMirrorWillBeUpdated(1);

	ExpectedPrimaryAndMirrorConfiguration(cdb_component_dbs,
										   /* primary */ context.responses[0].segment_db_info,
										   /* primary status */ GP_SEGMENT_CONFIGURATION_STATUS_UP,
										   /* mirror status */ GP_SEGMENT_CONFIGURATION_STATUS_UP,
										   /* mode */ GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC,
										   /* newPrimaryRole */ GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY,
										   /* newMirrorRole */ GP_SEGMENT_CONFIGURATION_ROLE_MIRROR,
										   /* willUpdatePrimary */ true,
										   /* willUpdateMirror */ true);

	bool is_updated = probeWalRepPublishUpdate(cdb_component_dbs, &context);

	/* we do not expect to send syncrep off libpq message */
	assert_false(FtsWalRepSetupMessageContext(&context));
	assert_true(context.responses[0].isScheduled);

	assert_true(is_updated);
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

	/* Start in SYNC mode */
	cdb_component_dbs = InitTestCdb(2,
									true,
									GP_SEGMENT_CONFIGURATION_MODE_INSYNC);

	FtsWalRepInitProbeContext(cdb_component_dbs, &context);

	/* Probe responded with one mirror down and not in-sync, and syncrep enabled on both primaries */
	context.responses[0].result.isPrimaryAlive = true;
	context.responses[0].result.isMirrorAlive = false;
	context.responses[0].result.isInSync = false;
	context.responses[0].result.isSyncRepEnabled = true;
	context.responses[0].isScheduled = true;
	context.responses[1].result.isPrimaryAlive = true;
	context.responses[1].result.isMirrorAlive = true;
	context.responses[1].result.isInSync = true;
	context.responses[1].result.isSyncRepEnabled = true;
	context.responses[1].isScheduled = true;

	/* we are updating one segment pair */
	PrimaryOrMirrorWillBeUpdated(1);

	ExpectedPrimaryAndMirrorConfiguration(cdb_component_dbs,
										   /* primary */ context.responses[0].segment_db_info,
										   /* primary status */ GP_SEGMENT_CONFIGURATION_STATUS_UP,
										   /* mirror status */ GP_SEGMENT_CONFIGURATION_STATUS_DOWN,
										   /* mode */ GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC,
										   /* newPrimaryRole */ GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY,
										   /* newMirrorRole */ GP_SEGMENT_CONFIGURATION_ROLE_MIRROR,
										   /* willUpdatePrimary */ true,
										   /* willUpdateMirror */ true);

	bool is_updated = probeWalRepPublishUpdate(cdb_component_dbs, &context);

	/* we expect to send syncrep off libpq message to only one segment */
	assert_true(FtsWalRepSetupMessageContext(&context));
	assert_false(context.responses[0].isScheduled);
	assert_true(context.responses[1].isScheduled);

	assert_true(is_updated);
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

	/* set the mode to SYNC in config */
	cdb_component_dbs = InitTestCdb(1,
									true,
									GP_SEGMENT_CONFIGURATION_MODE_INSYNC);

	FtsWalRepInitProbeContext(cdb_component_dbs, &context);

	/* Probe responded with Mirror Up and Not In SYNC */
	context.responses[0].result.isPrimaryAlive = false;
	context.responses[0].result.isSyncRepEnabled = true;

	/* we are updating one segment pair */
	PrimaryOrMirrorWillBeUpdated(1);

	ExpectedPrimaryAndMirrorConfiguration(cdb_component_dbs,
										   /* primary */ context.responses[0].segment_db_info,
										   /* primary status */ GP_SEGMENT_CONFIGURATION_STATUS_DOWN,
										   /* mirror status */ GP_SEGMENT_CONFIGURATION_STATUS_UP,
										   /* mode */ GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC,
										   /* newPrimaryRole */ GP_SEGMENT_CONFIGURATION_ROLE_MIRROR,
										   /* newMirrorRole */ GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY,
										   /* willUpdatePrimary */ true,
										   /* willUpdateMirror */ true);

	bool is_updated = probeWalRepPublishUpdate(cdb_component_dbs, &context);

	assert_true(is_updated);
	/* expect to be true, since we need to send PROMOTE message to the mirror now. */
	assert_true(FtsWalRepSetupMessageContext(&context));
	assert_string_equal(context.responses[0].message, FTS_MSG_PROMOTE);
	CdbComponentDatabaseInfo *mirror =
	GetSegmentFromCdbComponentDatabases(
										cdb_component_dbs, 0, GP_SEGMENT_CONFIGURATION_ROLE_MIRROR);

	assert_int_equal(context.responses[0].segment_db_info->dbid, mirror->dbid);

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

	cdb_component_dbs = InitTestCdb(1,
									true,
									GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);

	FtsWalRepInitProbeContext(cdb_component_dbs, &context);

	/* Probe responded with Mirror Up and SYNC */
	context.responses[0].result.isPrimaryAlive = true;
	context.responses[0].result.isInSync = true;
	context.responses[0].result.isSyncRepEnabled = true;

	/* we are updating one segment pair */
	PrimaryOrMirrorWillBeUpdated(1);

	ExpectedPrimaryAndMirrorConfiguration(cdb_component_dbs,
										   /* primary */ context.responses[0].segment_db_info,
										   /* primary status */ GP_SEGMENT_CONFIGURATION_STATUS_UP,
										   /* mirror status */ GP_SEGMENT_CONFIGURATION_STATUS_UP,
										   /* mode */ GP_SEGMENT_CONFIGURATION_MODE_INSYNC,
										   /* newPrimaryRole */ GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY,
										   /* newMirrorRole */ GP_SEGMENT_CONFIGURATION_ROLE_MIRROR,
										   /* willUpdatePrimary */ true,
										   /* willUpdateMirror */ true);

	bool is_updated = probeWalRepPublishUpdate(cdb_component_dbs, &context);

	assert_true(is_updated);
	assert_false(FtsWalRepSetupMessageContext(&context));
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
	context.responses[0].result.isPrimaryAlive = true;
	context.responses[0].result.isMirrorAlive = true;
	context.responses[0].result.isInSync = true;
	context.responses[0].result.isSyncRepEnabled = true;

	/* we are updating one segment pair */
	PrimaryOrMirrorWillBeUpdated(1);

	ExpectedPrimaryAndMirrorConfiguration(cdb_component_dbs,
										   /* primary */ context.responses[0].segment_db_info,
										   /* primary status */ GP_SEGMENT_CONFIGURATION_STATUS_UP,
										   /* mirror status */ GP_SEGMENT_CONFIGURATION_STATUS_UP,
										   /* mode */ GP_SEGMENT_CONFIGURATION_MODE_INSYNC,
										   /* newPrimaryRole */ GP_SEGMENT_CONFIGURATION_ROLE_PRIMARY,
										   /* newMirrorRole */ GP_SEGMENT_CONFIGURATION_ROLE_MIRROR,
										   /* willUpdatePrimary */ true,
										   /* willUpdateMirror */ true);

	bool is_updated = probeWalRepPublishUpdate(cdb_component_dbs, &context);

	assert_true(is_updated);
	assert_false(FtsWalRepSetupMessageContext(&context));
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
	context.responses[0].result.isPrimaryAlive = true;
	context.responses[0].result.isMirrorAlive = false;
	context.responses[0].result.isInSync = false;

	bool is_updated = probeWalRepPublishUpdate(cdb_component_dbs, &context);

	assert_false(FtsWalRepSetupMessageContext(&context));
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

	cdb_component_dbs = InitTestCdb(2,
									true,
									GP_SEGMENT_CONFIGURATION_MODE_NOTINSYNC);

	/* set the mirror down in config */
	CdbComponentDatabaseInfo *cdbinfo =
	GetSegmentFromCdbComponentDatabases(
										cdb_component_dbs, 0, GP_SEGMENT_CONFIGURATION_ROLE_MIRROR);

	cdbinfo->status = GP_SEGMENT_CONFIGURATION_STATUS_DOWN;

	FtsWalRepInitProbeContext(cdb_component_dbs, &context);

	/* No response received from segment 1 (content 0 primary) */

	/* no change for segment 2, probe returned */
	context.responses[1].result.isPrimaryAlive = true;
	context.responses[1].result.isSyncRepEnabled = true;

	bool is_updated = probeWalRepPublishUpdate(cdb_component_dbs, &context);

	assert_false(is_updated);
	assert_false(FtsWalRepSetupMessageContext(&context));
	pfree(cdb_component_dbs);
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
		unit_test(test_PrimaryUpMirrorDownNotInSync_to_PrimaryDown)
		/*-----------------------------------------------------------------------*/
	};

	MemoryContextInit();

	return run_tests(tests);
}
