#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

/* Fetch definition of PG_exception_stack */
#include "postgres.h"

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

void
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

	/* Mock MIRRORED_LOCK */
	InterruptHoldoffCount = 1;
	expect_value(LWLockHeldByMe, lockid, MirroredLock);
	will_return(LWLockHeldByMe, true);

	expect_any(LWLockAcquire, lockid);
	expect_any(LWLockAcquire, mode);
	will_be_called(LWLockAcquire);

	expect_ereport(ERROR, ERRCODE_PROGRAM_LIMIT_EXCEEDED);

	PG_TRY();
	{
		GetNewTransactionId(false, false);
		assert_false("No ereport(ERROR, ...) was called");
	}
	PG_CATCH();
	{
	}
	PG_END_TRY();
}

void
test_GetNewTransactionId_xid_warn_limit(void **state)
{
	const int xid = 25;
	VariableCacheData data;
	PGPROC proc;

	MyProc = &proc;

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

	/* Mock MIRRORED_LOCK */
	InterruptHoldoffCount = 1;
	expect_value(LWLockHeldByMe, lockid, MirroredLock);
	will_return(LWLockHeldByMe, true);

	expect_any(LWLockAcquire, lockid);
	expect_any(LWLockAcquire, mode);
	will_be_called(LWLockAcquire);

	expect_ereport(WARNING, ERRORCODE_NONE);

	/*
	 * verify rest of function logic, including assign MyProc->xid
	 */
	expect_any(ExtendCLOG, newestXact);
	will_be_called(ExtendCLOG);
	expect_any(ExtendSUBTRANS, newestXact);
	will_be_called(ExtendSUBTRANS);
	expect_any(DistributedLog_Extend, newestXact);
	will_be_called(DistributedLog_Extend);

	expect_any(LWLockRelease, lockid);
	will_be_called(LWLockRelease);
	
	PG_TRY();
	{
		GetNewTransactionId(false, true /*setProcXid*/);
		assert_int_equal(proc.xid, xid);
	}
	PG_CATCH();
	{
		assert_false("No ereport(WARNING, ...) was called");
	}
	PG_END_TRY();
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_GetNewTransactionId_xid_stop_limit),
		unit_test(test_GetNewTransactionId_xid_warn_limit)
	};

	return run_tests(tests);
}
