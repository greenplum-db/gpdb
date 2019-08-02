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

#define errfinish errfinish_impl
static int
errfinish_impl(int dummy __attribute__((unused)),...)
{
	PG_RE_THROW();
}

#include "../postgres.c"

#define EXPECT_EREPORT(LOG_LEVEL)     \
	expect_any(errmsg, fmt); \
	will_be_called(errmsg); \
	expect_any(errcode, sqlerrcode); \
	will_be_called(errcode); \
	expect_value(errstart, elevel, (LOG_LEVEL)); \
	expect_any(errstart, filename); \
	expect_any(errstart, lineno); \
	expect_any(errstart, funcname); \
	expect_any(errstart, domain); \
	if (LOG_LEVEL < ERROR) \
	{ \
		will_return(errstart, false); \
	} \
    else \
    { \
		will_return(errstart, true);\
    } \

const char *progname = "postgres";

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
test__IsTransactionExitStmt__IsTransactionStmt(void **state)
{
	TransactionStmt *stmt = makeNode(TransactionStmt);
	stmt->kind = TRANS_STMT_COMMIT;

	List *list = list_make1(stmt);

	assert_true(IsTransactionExitStmtList(list));

	list_free(list);
	pfree(stmt);
}

/* Query with Transaction with Exit Statement, return TRUE. */
static void
test__IsTransactionExitStmt__IsQuery(void **state)
{
	TransactionStmt *stmt = makeNode(TransactionStmt);
	stmt->kind = TRANS_STMT_COMMIT;
	Query *query = (Query *)palloc(sizeof(Query));
	query->type = T_Query;
	query->commandType = CMD_UTILITY;
	query->utilityStmt = (Node*) stmt;

	List *list = list_make1(query);

	assert_true(IsTransactionExitStmtList(list));

	list_free(list);
	pfree(query);
	pfree(stmt);
}

/*
 * Test ProcessInterrupts when ClientConnectionLost flag is set
 */
static void
test__ProcessInterrupts__ClientConnectionLost(void **state)
{
	will_be_called(DisableNotifyInterrupt);
	will_be_called(DisableCatchupInterrupt);

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
	assert_false(ImmediateInterruptOK);
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

	/* Mock up elog_start and elog_finish */
	expect_any(elog_start, filename);
	expect_any(elog_start, lineno);
	expect_any(elog_start, funcname);
	will_be_called(elog_start);
	expect_value(elog_finish, elevel, LOG);
	expect_any(elog_finish, fmt);
	will_be_called(elog_finish);

	ProcessInterrupts(__FILE__, __LINE__);

	assert_false(QueryCancelPending);

	/*
	 * QueryCancelPending = true, DoingCommandRead = false; we should
	 * error out.
	 */
	QueryCancelPending = true;
	DoingCommandRead = false;

	/* Mock up elog_start and elog_finish */
	expect_any(elog_start, filename);
	expect_any(elog_start, lineno);
	expect_any(elog_start, funcname);
	will_be_called(elog_start);
	expect_value(elog_finish, elevel, LOG);
	expect_any(elog_finish, fmt);
	will_be_called(elog_finish);

	will_be_called(DisableNotifyInterrupt);
	will_be_called(DisableCatchupInterrupt);

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
	assert_false(ImmediateInterruptOK);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__IsTransactionExitStmtList__MultipleElementList),
		unit_test(test__IsTransactionExitStmt__IsTransactionStmt),
		unit_test(test__IsTransactionExitStmt__IsQuery),
		unit_test(test__ProcessInterrupts__ClientConnectionLost),
		unit_test(test__ProcessInterrupts__DoingCommandRead)
	};

	MemoryContextInit();

	return run_tests(tests);
}
