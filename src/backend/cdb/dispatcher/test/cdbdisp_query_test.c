#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"
#include "postgres.h"

#include "storage/ipc.h"
#include "storage/proc.h"

#include "../cdbdisp_query.c"


#undef PG_RE_THROW
#define PG_RE_THROW() siglongjmp(*PG_exception_stack, 1)


int			__wrap_errmsg(const char *fmt,...);
int			__wrap_errcode(int sqlerrcode);
bool		__wrap_errstart(int elevel, const char *filename, int lineno,
							const char *funcname, const char *domain);
void		__wrap_errfinish(int dummy __attribute__((unused)),...);
Gang	   *__wrap_cdbgang_createGang_async(List *segments, SegmentType segmentType);
int			__wrap_pqPutMsgStart(char msg_type, bool force_len, PGconn *conn);
int			__wrap_PQcancel(PGcancel *cancel, char *errbuf, int errbufsize);
char	   *__wrap_serializeNode(Node *node, int *size, int *uncompressed_size_out);
char	   *__wrap_qdSerializeDtxContextInfo(int *size, bool wantSnapshot, bool inCursor, int txnOptions, char *debugCaller);
void		__wrap_VirtualXactLockTableInsert(VirtualTransactionId vxid);
void		__wrap_AcceptInvalidationMessages(void);
static void terminate_process();


int
__wrap_errmsg(const char *fmt,...)
{
	check_expected(fmt);
	optional_assignment(fmt);
	return (int) mock();
}


int
__wrap_errcode(int sqlerrcode)
{
	check_expected(sqlerrcode);
	return (int) mock();
}


bool
__wrap_errstart(int elevel, const char *filename, int lineno,
				const char *funcname, const char *domain)
{
	if (elevel < LOG)
		return false;

	check_expected(elevel);
	check_expected(filename);
	check_expected(lineno);
	check_expected(funcname);
	check_expected(domain);
	optional_assignment(filename);
	optional_assignment(funcname);
	optional_assignment(domain);
	return (bool) mock();
}


void
__wrap_errfinish(int dummy __attribute__((unused)),...)
{
	PG_RE_THROW();
}


static void
expect_ereport(int expect_elevel)
{
	expect_any(__wrap_errmsg, fmt);
	will_be_called(__wrap_errmsg);

	expect_any(__wrap_errcode, sqlerrcode);
	will_be_called(__wrap_errcode);

	expect_value(__wrap_errstart, elevel, expect_elevel);
	expect_any(__wrap_errstart, filename);
	expect_any(__wrap_errstart, lineno);
	expect_any(__wrap_errstart, funcname);
	expect_any(__wrap_errstart, domain);
	if (expect_elevel < ERROR)
	{
		will_return(__wrap_errstart, false);
	}
	else
	{
		will_return(__wrap_errstart, true);
	}
}


Gang *
__wrap_cdbgang_createGang_async(List *segments, SegmentType segmentType)
{
	MemoryContext oldContext = MemoryContextSwitchTo(DispatcherContext);
	Gang	   *gang = buildGangDefinition(segments, segmentType);

	MemoryContextSwitchTo(oldContext);

	PGconn	   *conn = (PGconn *) malloc(sizeof(PGconn));

	MemSet(conn, 0, sizeof(PGconn));
	initPQExpBuffer(&conn->errorMessage);
	initPQExpBuffer(&conn->workBuffer);
	gang->db_descriptors[0]->conn = conn;

	return gang;
}


int
__wrap_pqPutMsgStart(char msg_type, bool force_len, PGconn *conn)
{
	if (conn->outBuffer_shared)
		fail_msg("Mustn't send something else during dispatch!");
	check_expected(msg_type);
	check_expected(force_len);
	check_expected(conn);
	optional_assignment(conn);
	return (int) mock();
}


int
__wrap_PQcancel(PGcancel *cancel, char *errbuf, int errbufsize)
{
	return (int) mock();
}


char *
__wrap_serializeNode(Node *node, int *size, int *uncompressed_size_out)
{
	const int	alloc_size = 1024;

	if (size != NULL)
		*size = alloc_size;
	if (uncompressed_size_out != NULL)
		*uncompressed_size_out = alloc_size;

	return (char *) palloc(alloc_size);
}


char *
__wrap_qdSerializeDtxContextInfo(int *size, bool wantSnapshot, bool inCursor, int txnOptions, char *debugCaller)
{
	const int	alloc_size = 1024;

	assert_int_not_equal(size, NULL);
	*size = alloc_size;

	return (char *) palloc(alloc_size);
}


void
__wrap_VirtualXactLockTableInsert(VirtualTransactionId vxid)
{
	mock();
}

void
__wrap_AcceptInvalidationMessages(void)
{
	mock();
}


static void
terminate_process()
{
	die(SIGTERM);
}

/*
 * Test query may be interrupted during plan dispatching
 */
static void
test__CdbDispatchPlan_may_be_interrupted(void **state)
{
	PlannedStmt *plannedstmt = (PlannedStmt *) palloc(sizeof(PlannedStmt));

	/* slice table is needed to allocate gang */
	plannedstmt->slices = palloc0(sizeof(PlanSlice));
	plannedstmt->numSlices = 1;
	PlanSlice  *slice = &plannedstmt->slices[0];

	slice->sliceIndex = 1;
	slice->gangType = GANGTYPE_PRIMARY_READER;
	slice->numsegments = 1;
	slice->parentIndex = -1;
	slice->segindex = 0;

	QueryDesc  *queryDesc = (QueryDesc *) palloc(sizeof(QueryDesc));

	queryDesc->plannedstmt = plannedstmt;
	/* ddesc->secContext is filled in cdbdisp_buildPlanQueryParms() */
	queryDesc->ddesc = (QueryDispatchDesc *) palloc(sizeof(QueryDispatchDesc));
	/* source text is required for buildGpQueryString() */
	queryDesc->sourceText = "select a from t1;";

	queryDesc->estate = CreateExecutorState();

	/* cdbcomponent_getCdbComponents() mocks */
	will_be_called(FtsNotifyProber);
	will_return(getFtsVersion, 1);
	will_return(GetGpExpandVersion, 1);

	/* StartTransactionCommand() mocks */
	will_return(RecoveryInProgress, false);
	will_be_called(__wrap_VirtualXactLockTableInsert);
	will_be_called(__wrap_AcceptInvalidationMessages);
	will_be_called(initialize_wal_bytes_written);

	/*
	 * cdbdisp_dispatchToGang()
	 *
	 * start sending MPP query to QE inside PQsendGpQuery_shared() replace
	 * connection buffer with the shared one
	 */
	expect_any(PQsendQueryStart, conn);
	will_return(PQsendQueryStart, true);

	/* first try to flush MPP query inside PQsendGpQuery_shared() */
	expect_any(pqFlushNonBlocking, conn);
	will_return(pqFlushNonBlocking, 1);

	/*
	 * cdbdisp_waitDispatchFinish()
	 *
	 * query will be interrupted before poll()
	 */
	expect_any_count(ResetWaitEventSet, pset, 2);
	expect_any_count(ResetWaitEventSet, context, 2);
	expect_any_count(ResetWaitEventSet, nevents, 2);
	will_be_called_count(ResetWaitEventSet, 2);

	expect_any(pqFlushNonBlocking, conn);
	will_return_with_sideeffect(pqFlushNonBlocking, 1, &terminate_process, NULL);

	expect_any(SetLatch, latch);
	will_be_called(SetLatch);

	expect_any(AddWaitEventToSet, set);
	expect_any(AddWaitEventToSet, events);
	expect_any(AddWaitEventToSet, fd);
	expect_any(AddWaitEventToSet, latch);
	expect_any(AddWaitEventToSet, user_data);
	will_be_called(AddWaitEventToSet);

	will_return(IsLogicalLauncher, false);

	/* process was terminated by administrative command */
	expect_ereport(FATAL);

	/* QD will trying to cancel queries on QEs */
	will_return(__wrap_PQcancel, true);

	/* during close and free connection */
	expect_any_count(pqClearAsyncResult, conn, 2);
	will_be_called_count(pqClearAsyncResult, 2);

	/*
	 * BUT! pqPutMsgStart mustn't be called
	 *
	 * we can't send termination message (X) until shared message isn't sent
	 * out the buffer completely
	 */

	/*
	 * dirty hack. cluster topology needed to allocate gangs is loaded from
	 * gpsegconfig_dump outside of transaction
	 */
	cdbcomponent_getCdbComponents();

	StartTransactionCommand();

	PG_TRY();
	{
		queryDesc->estate->es_sliceTable = InitSliceTable(queryDesc->estate, plannedstmt);

		CdbDispatchPlan(queryDesc, queryDesc->estate->es_param_exec_vals,
						false, false);
		fail();
	}
	PG_CATCH();
	{
		/*
		 * SIGTERM handling emulation gpdb bail out from CheckDispatchResult
		 * without flushing unsent messages in case of process exit in
		 * progress AtAbort_DispatcherState will be called during transaction
		 * abort
		 */
		proc_exit_inprogress = true;

		AtAbort_DispatcherState();
	}
	PG_END_TRY();
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] =
	{
		unit_test(test__CdbDispatchPlan_may_be_interrupted)
	};

	Gp_role = GP_ROLE_DISPATCH;
	/* to start transaction */
	PGPROC		proc;

	MyBackendId = 7;
	proc.backendId = MyBackendId;
	MyProc = &proc;
	/* to build cdb components info */
	GpIdentity.dbid = 1;
	GpIdentity.segindex = -1;

	MemoryContextInit();

	/* to avoid mocking cdbtm.c functions */
	MyTmGxactLocal = (TMGXACTLOCAL *) MemoryContextAllocZero(TopMemoryContext, sizeof(TMGXACTLOCAL));

	SetSessionUserId(1000, true);

	return run_tests(tests);
}
