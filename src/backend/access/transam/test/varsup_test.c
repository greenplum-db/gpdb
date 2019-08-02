#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

/* Fetch definition of PG_exception_stack */
#include "postgres.h"

#undef ereport
#define ereport(elevel, rest) ereport_mock(elevel, rest)

static int expected_elevel;

static void ereport_mock(int elevel, int dummy __attribute__((unused)),...)
{
	assert_int_equal(elevel, expected_elevel);
	
	if (elevel >= ERROR)
		siglongjmp(*PG_exception_stack, 1);
}

#include "../varsup.c"

#define ERRORCODE_NONE -1

static void expect_ereport(int log_level, int errcode)
{
	expect_any(errmsg, fmt);
	will_be_called(errmsg);
	expect_any(errhint, fmt);
	will_be_called(errhint);
	
	if (errcode != ERRORCODE_NONE)
	{
		expect_value(errcode, sqlerrcode, errcode);
		will_be_called(errcode);
	}

	expected_elevel = log_level;
}

static void
test_GetNewTransactionId_xid_stop_limit(void **state)
{
	VariableCacheData data;

	/*
	 * make sure the nextXid is larger than xidVacLimit to check xidStopLimit,
	 * and it's larger than xidStopLimit to trigger the ereport(ERROR).
	 */
	ShmemVariableCache = &data;
	ShmemVariableCache->nextXid = 30;
	ShmemVariableCache->xidVacLimit = 10;
	ShmemVariableCache->xidStopLimit = 20;
	IsUnderPostmaster = true;

	will_return(RecoveryInProgress, false);

	expect_any(LWLockAcquire, l);
	expect_any(LWLockAcquire, mode);
	will_return(LWLockAcquire, true);

	expect_any(LWLockRelease, l);
	will_be_called(LWLockRelease);

	expect_any(get_database_name, dbid);
	will_return(get_database_name, "foo");
	
	expect_ereport(ERROR, ERRCODE_PROGRAM_LIMIT_EXCEEDED);

	PG_TRY();
	{
		GetNewTransactionId(false);
		assert_false("No ereport(ERROR, ...) was called");
	}
	PG_CATCH();
	{
	}
	PG_END_TRY();
}

static void
test_GetNewTransactionId_xid_warn_limit(void **state)
{
	const int xid = 25;
	VariableCacheData data;
	PGPROC proc;
	PGXACT xact;

	MyProc = &proc;
	MyPgXact = &xact;
	/*
	 * make sure nextXid is between xidWarnLimit and xidStopLimit to trigger
	 * the ereport(WARNING).
	 */
	ShmemVariableCache = &data;
	ShmemVariableCache->nextXid = xid;
	ShmemVariableCache->xidVacLimit = 10;
	ShmemVariableCache->xidWarnLimit = 20;
	ShmemVariableCache->xidStopLimit = 30;
	IsUnderPostmaster = true;

	will_return(RecoveryInProgress, false);

	expect_any(LWLockAcquire, l);
	expect_any(LWLockAcquire, mode);
	will_return(LWLockAcquire, true);

	expect_any(LWLockRelease, l);
	will_be_called(LWLockRelease);

	expect_any(get_database_name, dbid);
	will_return(get_database_name, "foo");
	
	expect_ereport(WARNING, ERRORCODE_NONE);

	/*
	 * verify rest of function logic, including assign MyProc->xid
	 */
	expect_any(LWLockAcquire, l);
	expect_any(LWLockAcquire, mode);
	will_return(LWLockAcquire, true);

	expect_any(ExtendCLOG, newestXact);
	will_be_called(ExtendCLOG);
	expect_any(ExtendSUBTRANS, newestXact);
	will_be_called(ExtendSUBTRANS);
	expect_any(DistributedLog_Extend, newestXact);
	will_be_called(DistributedLog_Extend);

	expect_any(LWLockRelease, l);
	will_be_called(LWLockRelease);
	
	PG_TRY();
	{
		GetNewTransactionId(false);
		assert_int_equal(xact.xid, xid);
	}
	PG_CATCH();
	{
		assert_false("No ereport(WARNING, ...) was called");
	}
	PG_END_TRY();
}

static void
should_acquire_and_release_oid_gen_lock()
{
	expect_value(LWLockAcquire, l, OidGenLock);
	expect_value(LWLockAcquire, mode, LW_EXCLUSIVE);
	will_be_called(LWLockAcquire);

	expect_value(LWLockRelease, l, OidGenLock);
	will_be_called(LWLockRelease);
}

static void
should_generate_xlog_for_next_oid(Oid expected_oid_in_xlog_rec)
{
	expect_value(XLogPutNextOid, nextOid, expected_oid_in_xlog_rec);
	will_be_called(XLogPutNextOid);
}

/*
 * QD wrapped around before QE did
 * QD nextOid: FirstNormalObjectId, QE nextOid: PG_UINT32_MAX
 * QE should set its nextOid to FirstNormalObjectId and create an xlog
 */
static void
test_AdvanceObjectId_QD_wrapped_before_QE(void **state)
{
	VariableCacheData data = {.nextOid = PG_UINT32_MAX, .oidCount = 1};
	ShmemVariableCache = &data;

	should_generate_xlog_for_next_oid(FirstNormalObjectId + VAR_OID_PREFETCH);

	should_acquire_and_release_oid_gen_lock();

	/* Run the test */
	AdvanceObjectId(FirstNormalObjectId);

	/* Check if shared memory Oid values have been changed correctly */
	assert_int_equal(ShmemVariableCache->nextOid, FirstNormalObjectId);
	assert_int_equal(ShmemVariableCache->oidCount, VAR_OID_PREFETCH);
}

/*
 * QE wrapped around but QD did not
 * QD nextOid: PG_UINT32_MAX, QE nextOid: FirstNormalObjectId
 * QE should do nothing
 */
static void
test_AdvanceObjectId_QE_wrapped_before_QD(void **state)
{
	VariableCacheData data = {.nextOid = FirstNormalObjectId, .oidCount = VAR_OID_PREFETCH};
	ShmemVariableCache = &data;

	should_acquire_and_release_oid_gen_lock();

	/* Run the test */
	AdvanceObjectId(PG_UINT32_MAX);

	/* Check if shared memory Oid values have been changed correctly */
	assert_int_equal(ShmemVariableCache->nextOid, FirstNormalObjectId);
	assert_int_equal(ShmemVariableCache->oidCount, VAR_OID_PREFETCH);
}

/*
 * Regular normal operation flow
 * QD nextOid: FirstNormalObjectId + 2, QE nextOid: FirstNormalObjectId
 * QE should set its nextOid to FirstNormalObjectId + 2 and not create an xlog
 */
static void
test_AdvanceObjectId_normal_flow(void **state)
{
	VariableCacheData data = {.nextOid = FirstNormalObjectId, .oidCount = VAR_OID_PREFETCH};
	ShmemVariableCache = &data;

	should_acquire_and_release_oid_gen_lock();

	/* Run the test */
	AdvanceObjectId(FirstNormalObjectId + 2);

	/* Check if shared memory Oid values have been changed correctly */
	assert_int_equal(ShmemVariableCache->nextOid, FirstNormalObjectId + 2);
	assert_int_equal(ShmemVariableCache->oidCount, VAR_OID_PREFETCH -2);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_GetNewTransactionId_xid_stop_limit),
		unit_test(test_GetNewTransactionId_xid_warn_limit),
		unit_test(test_AdvanceObjectId_QD_wrapped_before_QE),
		unit_test(test_AdvanceObjectId_QE_wrapped_before_QD),
		unit_test(test_AdvanceObjectId_normal_flow)
	};

	return run_tests(tests);
}
