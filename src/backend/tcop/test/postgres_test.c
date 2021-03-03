#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"

/*
 * Mock PG_RE_THROW, because we are not using real elog.o.
 * The closest mockery is to call siglongjmp().
 */
#undef PG_RE_THROW
#define PG_RE_THROW() siglongjmp(*PG_exception_stack, 1)

static void
_errfinish_impl()
{
	PG_RE_THROW();
}

#include "../postgres.c"

#define EXPECT_EREPORT(LOG_LEVEL)     \
	expect_value(errstart, elevel, (LOG_LEVEL)); \
	expect_any(errstart, domain); \
	if (LOG_LEVEL < ERROR) \
	{ \
		will_return(errstart, false); \
	} \
    else \
    { \
		will_return_with_sideeffect(errstart, false, &_errfinish_impl, NULL); \
    } \


/* List with multiple elements, return FALSE. */
static void
test__IsTransactionExitStmtList__MultipleElementList(void **state)
{
	List *list = list_make2_int(1,2);

	assert_false(IsTransactionExitStmtList(list));

	list_free(list);
}

/*  Transaction with Exit Statement, return TRUE. */
static void
test__IsTransactionExitStmt(void **state)
{
	PlannedStmt *pstmt = makeNode(PlannedStmt);
	TransactionStmt *stmt = makeNode(TransactionStmt);

	stmt->kind = TRANS_STMT_COMMIT;
	pstmt->commandType = CMD_UTILITY;
	pstmt->utilityStmt = (Node *)stmt;

	List *list = list_make1(pstmt);

	assert_true(IsTransactionExitStmtList(list));

	list_free(list);
	pfree(stmt);
	pfree(pstmt);
}

/*
 * Test ProcessInterrupts when ClientConnectionLost flag is set
 */
static void
test__ProcessInterrupts__ClientConnectionLost(void **state)
{
	/*
	 * Setting all the flags so that ProcessInterrupts only goes in the if-block
	 * we're interested to test
	 */
	InterruptHoldoffCount = 0;
	CritSectionCount = 0;
	ProcDiePending = 0;
	ClientConnectionLost = 1;
	whereToSendOutput = DestDebug;

	EXPECT_EREPORT(FATAL);

	PG_TRY();
	{
		/* Run function under test */
		ProcessInterrupts(__FILE__, __LINE__);
		assert_true(false);
	}
	PG_CATCH();
	{
	}
	PG_END_TRY();

	assert_true(whereToSendOutput == DestNone);
	assert_false(QueryCancelPending);
}

/*
 * Test ProcessInterrupts when DoingCommandRead is set
 */
static void
test__ProcessInterrupts__DoingCommandRead(void **state)
{
	InterruptHoldoffCount = 0;
	CritSectionCount = 0;
	ProcDiePending = false;
	ClientConnectionLost = false;

	/*
	 * QueryCancelPending = true, DoingCommandRead = true; we should NOT
	 * error out.
	 */
	QueryCancelPending = true;
	DoingCommandRead = true;

	EXPECT_EREPORT(LOG);

	ProcessInterrupts(__FILE__, __LINE__);

	assert_false(QueryCancelPending);

	/*
	 * QueryCancelPending = true, DoingCommandRead = false; we should
	 * error out.
	 */
	QueryCancelPending = true;
	DoingCommandRead = false;

	EXPECT_EREPORT(LOG);
	EXPECT_EREPORT(ERROR);

	PG_TRY();
	{
		/* Run function under test */
		ProcessInterrupts(__FILE__, __LINE__);
		assert_true(false);
	}
	PG_CATCH();
	{
	}
	PG_END_TRY();

	assert_false(QueryCancelPending);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__IsTransactionExitStmtList__MultipleElementList),
		unit_test(test__IsTransactionExitStmt),
		unit_test(test__ProcessInterrupts__ClientConnectionLost),
		unit_test(test__ProcessInterrupts__DoingCommandRead)
	};

	MemoryContextInit();

	return run_tests(tests);
}
