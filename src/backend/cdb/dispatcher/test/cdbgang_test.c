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
static char *segHostIp[TOTOAL_SEGMENTS * 2] = {
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
static char *qdHostIp = "127.0.0.1";
static int segBasePort = 30000;
static int qdPort = 5432;

static CdbComponentDatabases *
makeTestCdb(int entryCnt, int segCnt)
{
	int			i = 0;

	CdbComponentDatabases *cdb = palloc0(sizeof(CdbComponentDatabases));

	cdb->total_entry_dbs = entryCnt;
	cdb->total_segments = segCnt;
	cdb->total_segment_dbs = TOTOAL_SEGMENTS * 2;	/* with mirror */
	cdb->entry_db_info = palloc0(
								 sizeof(CdbComponentDatabaseInfo) * cdb->total_entry_dbs);
	cdb->segment_db_info = palloc0(
								   sizeof(CdbComponentDatabaseInfo) * cdb->total_segment_dbs);

	for (i = 0; i < cdb->total_entry_dbs; i++)
	{
		CdbComponentDatabaseInfo *cdbinfo = &cdb->entry_db_info[i];
		cdbinfo->config = (GpSegConfigEntry*)palloc(sizeof(GpSegConfigEntry));

		cdbinfo->config->hostip = qdHostIp;
		cdbinfo->config->port = qdPort;

		cdbinfo->config->dbid = 1;
		cdbinfo->config->segindex = -1;

		cdbinfo->config->role = 'p';
		cdbinfo->config->preferred_role = 'p';
		cdbinfo->config->status = 'u';
		cdbinfo->config->mode = 's';
	}

	for (i = 0; i < cdb->total_segment_dbs; i++)
	{
		CdbComponentDatabaseInfo *cdbinfo = &cdb->segment_db_info[i];
		cdbinfo->config = (GpSegConfigEntry*)palloc(sizeof(GpSegConfigEntry));

		cdbinfo->config->hostip = segHostIp[i];
		cdbinfo->config->port = segBasePort + i / 2;

		cdbinfo->config->dbid = i + 2;
		cdbinfo->config->segindex = i / 2;

		cdbinfo->config->role = i % 2 ? 'm' : 'p';
		cdbinfo->config->preferred_role = i % 2 ? 'm' : 'p';
		cdbinfo->config->status = 'u';
		cdbinfo->config->mode = 's';
	}

	return cdb;
}

/*
 * Make sure resetSessionForPrimaryGangLoss doesn't access catalog.
 */
static void
test__resetSessionForPrimaryGangLoss(void **state)
{
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
