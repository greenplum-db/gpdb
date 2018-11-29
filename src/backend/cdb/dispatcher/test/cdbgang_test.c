#include "postgres.h"
#include "miscadmin.h"
#include "cdb/cdbvars.h"
#include "libpq/libpq-be.h"
#include "access/xact.h"
#include "storage/lock.h"
#include "storage/proc.h"
#include "cmockery.h"
#include "libpq-fe.h"
#include "libpq-int.h"
#include "cdb/cdbconn.h"

#include "../cdbgang.c"

#define TOTOAL_SEGMENTS 10

static CdbComponentDatabases *s_cdb = NULL;
static const char *segHostIp[TOTOAL_SEGMENTS * 2] = {
	"10.10.10.0",
	"10.10.10.1",
	"10.10.10.2",
	"10.10.10.3",
	"10.10.10.4",
	"10.10.10.5",
	"10.10.10.6",
	"10.10.10.7",
	"10.10.10.8",
	"10.10.10.9",
	"10.10.10.10",
	"10.10.10.11",
	"10.10.10.12",
	"10.10.10.13",
	"10.10.10.14",
	"10.10.10.15",
	"10.10.10.16",
	"10.10.10.17",
	"10.10.10.18",
	"10.10.10.19"
};
static const char *qdHostIp = "127.0.0.1";
static segBasePort = 30000;
static int	qdPort = 5432;
static PGconn pgconn;

static CdbComponentDatabases *
makeTestCdb(int entryCnt, int segCnt)
{
	int			i = 0;

	CdbComponentDatabases *cdb = palloc0(sizeof(CdbComponentDatabases));

	cdb->total_entry_dbs = entryCnt;
	cdb->total_segments = segCnt;
	cdb->total_segment_dbs = TOTOAL_SEGMENTS * 2;	/* with mirror */
	cdb->my_dbid = 1;
	cdb->my_segindex = -1;
	cdb->my_isprimary = true;
	cdb->entry_db_info = palloc0(
								 sizeof(CdbComponentDatabaseInfo) * cdb->total_entry_dbs);
	cdb->segment_db_info = palloc0(
								   sizeof(CdbComponentDatabaseInfo) * cdb->total_segment_dbs);

	for (i = 0; i < cdb->total_entry_dbs; i++)
	{
		CdbComponentDatabaseInfo *cdbinfo = &cdb->entry_db_info[i];

		cdbinfo->hostip = qdHostIp;
		cdbinfo->port = qdPort;

		cdbinfo->dbid = 1;
		cdbinfo->segindex = '-1';

		cdbinfo->role = 'p';
		cdbinfo->preferred_role = 'p';
		cdbinfo->status = 'u';
		cdbinfo->mode = 's';
	}

	for (i = 0; i < cdb->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *cdbinfo = &cdb->segment_db_info[i];

		cdbinfo->hostip = segHostIp[i];
		cdbinfo->port = segBasePort + i / 2;

		cdbinfo->dbid = i + 2;
		cdbinfo->segindex = i / 2;

		cdbinfo->role = i % 2 ? 'm' : 'p';
		cdbinfo->preferred_role = i % 2 ? 'm' : 'p';
		cdbinfo->status = 'u';
		cdbinfo->mode = 's';
	}

	return cdb;
}

void
validateCdbInfo(CdbComponentDatabaseInfo *cdbinfo, int segindex)
{
	assert_string_equal(cdbinfo->hostip, segHostIp[segindex * 2]);
	assert_int_equal(cdbinfo->port, segBasePort + segindex);
	assert_int_equal(cdbinfo->dbid, segindex * 2 + 2);
	assert_int_equal(cdbinfo->segindex, segindex);
	assert_int_equal(cdbinfo->mode, 's');
	assert_int_equal(cdbinfo->status, 'u');
	assert_int_equal(cdbinfo->role, 'p');
	assert_int_equal(cdbinfo->preferred_role, 'p');
}

void
mockLibpq(PGconn *pgConn, uint32 motionListener, int qePid)
{
	static char motionListener_str[11];

	snprintf(motionListener_str, sizeof(motionListener_str), "%u", motionListener);

	expect_any_count(PQconnectdbParams, keywords, -1);
	expect_any_count(PQconnectdbParams, values, -1);
	expect_any_count(PQconnectdbParams, expand_dbname, -1);
	will_return_count(PQconnectdbParams, pgConn, TOTOAL_SEGMENTS);

	expect_value_count(PQstatus, conn, pgConn, -1);
	will_return_count(PQstatus, CONNECTION_OK, -1);

	expect_value_count(PQsetNoticeReceiver, conn, pgConn, -1);
	expect_any_count(PQsetNoticeReceiver, proc, -1);
	expect_any_count(PQsetNoticeReceiver, arg, -1);
	will_return_count(PQsetNoticeReceiver, CONNECTION_OK, -1);

	expect_value_count(PQparameterStatus, conn, pgConn, -1);
	expect_string_count(PQparameterStatus, paramName, "qe_listener_port", -1);
	will_return_count(PQparameterStatus, motionListener_str, -1);

	expect_value_count(PQbackendPID, conn, pgConn, -1);
	will_return_count(PQbackendPID, qePid, -1);
}

/*
 * Make sure resetSessionForPrimaryGangLoss doesn't access catalog.
 */
static void
test__resetSessionForPrimaryGangLoss(void **state)
{
	PROC_HDR	dummyGlobal;
	PGPROC		dummyProc;

	will_be_called(RedZoneHandler_DetectRunawaySession);
	will_return(ProcCanSetMppSessionId, true);

	/* Assum we have created a temporary namespace. */
	will_return(TempNamespaceOidIsValid, true);
	will_return(ResetTempNamespace, 9999);
	OldTempNamespace = InvalidOid;

	resetSessionForPrimaryGangLoss();
	assert_int_equal(OldTempNamespace, 9999);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] =
	{
		unit_test(test__resetSessionForPrimaryGangLoss),
	};

	MemoryContextInit();
	DispatcherContext = AllocSetContextCreate(TopMemoryContext,
											  "Dispatch Context",
											  ALLOCSET_DEFAULT_MINSIZE,
											  ALLOCSET_DEFAULT_INITSIZE,
											  ALLOCSET_DEFAULT_MAXSIZE);

	CurrentResourceOwner = ResourceOwnerCreate(NULL, "gang test");
	Gp_role = GP_ROLE_DISPATCH;
	GpIdentity.dbid = 1;
	GpIdentity.segindex = -1;

	Port		procport;

	MyProcPort = &procport;
	MyProcPort->database_name = "test";
	MyProcPort->user_name = "gpadmin";

	s_cdb = makeTestCdb(1, TOTOAL_SEGMENTS);

	return run_tests(tests);
}
