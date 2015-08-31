#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../postgres.c"

const char *progname = "postgres";

/* List with multiple elements, return FALSE. */
void
test__IsTransactionExitStmtList__MultipleElementList(void **state)
{
	List *list = list_make2_int(1,2);

	assert_false(IsTransactionExitStmtList(list));

	list_free(list);
}

/*  Transaction with Exit Statement, return TRUE. */
void
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
void
test__IsTransactionExitStmt__IsQuery(void **state)
{
	TransactionStmt *stmt = makeNode(TransactionStmt);
	stmt->kind = TRANS_STMT_COMMIT;
	Query *query = (Query *)palloc(sizeof(Query));
	query->type = T_Query;
	query->commandType = CMD_UTILITY;
	query->utilityStmt = stmt;

	List *list = list_make1(query);

	assert_true(IsTransactionExitStmtList(list));

	list_free(list);
	pfree(query);
	pfree(stmt);
}

/*
 * Test ProcessInterrupts when ClientConnectionLost flag is set
 */
void
test__ProcessInterrupts__ClientConnectionLost(void **state)
{
	/* Mocking errstart -- expect an ereport(FATAL) to be called */
	expect_value(errstart, elevel, FATAL);
	expect_any(errstart, filename);
	expect_any(errstart, lineno);
	expect_any(errstart, funcname);
	expect_any(errstart, domain);
	will_return(errstart, false);

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

	/* Run function under test */
	ProcessInterrupts();

	assert_true(whereToSendOutput == DestNone);
	assert_false(QueryCancelPending);
	assert_false(ImmediateInterruptOK);
}

/*
 * Test ProcessInterrupts when DoingCommandRead is set
 */
void
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

	ProcessInterrupts();

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

	/* Mock up errstart */
	expect_value(errstart, elevel, ERROR);
	expect_any(errstart, filename);
	expect_any(errstart, lineno);
	expect_any(errstart, funcname);
	expect_any(errstart, domain);
	will_return(errstart, false);

	will_be_called(DisableNotifyInterrupt);
	will_be_called(DisableCatchupInterrupt);

	ProcessInterrupts();

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
