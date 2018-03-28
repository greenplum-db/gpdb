#include "../idle_resource_cleaner.c"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"

int			idle_session_timeout_action_calls = 0;

void
idle_session_timeout_action_spy()
{
	idle_session_timeout_action_calls++;
}

int
returns1000_stub(void)
{
	return 1000;
}

#define test_with_setup_and_teardown(test_func) \
	unit_test_setup_teardown(test_func, setup, teardown)

void
setup(void **state)
{
	idle_session_timeout_action_hook = idle_session_timeout_action_spy;
	idle_session_timeout_action_calls = 0;
}

void
teardown(void **state)
{
	idle_session_timeout_action_calls = 0;
}

void
test__StartIdleResourceCleanupTimersWhenIdleGangTimeoutIs0AndNoSessionTimeoutHook(
																				  void **state)
{
	IdleSessionGangTimeout = 0;
	get_idle_session_timeout_hook = NULL;

	/*
	 * cmockery implicitly asserts that GangsExist and enable_sig_alarm are
	 * not called
	 */

	StartIdleResourceCleanupTimers();
}

void
test__StartIdleResourceCleanupTimersWhenNoGangsExistAndNoSessionTimeoutHook(
																			void **state)
{
	IdleSessionGangTimeout = 10000;
	get_idle_session_timeout_hook = NULL;

	will_return(GangsExist, false);
	/* cmockery implicitly asserts that enable_sig_alarm is not called */

	StartIdleResourceCleanupTimers();
}

void
test__StartIdleResourceCleanupTimersWhenGangsExistAndNoSessionTimeoutHook(
																		  void **state)
{
	IdleSessionGangTimeout = 10000;
	get_idle_session_timeout_hook = NULL;

	NextTimeoutAction = -1;

	will_return(GangsExist, true);
	expect_value(enable_sig_alarm, delayms, 10000);
	expect_value(enable_sig_alarm, is_statement_timeout, false);
	will_return(enable_sig_alarm, true);

	StartIdleResourceCleanupTimers();

	assert_int_equal(NextTimeoutAction, GANG_TIMEOUT);
}

void
test__StartIdleResourceCleanupTimersWhenGangsExistAndSessionTimeoutEnabled(
																		   void **state)
{
	IdleSessionGangTimeout = 10000;
	get_idle_session_timeout_hook = returns1000_stub;
	NextTimeoutAction = -1;

	will_return(GangsExist, true);
	expect_value(enable_sig_alarm, delayms, 10000);
	expect_value(enable_sig_alarm, is_statement_timeout, false);
	will_return(enable_sig_alarm, true);

	StartIdleResourceCleanupTimers();

	assert_int_equal(NextTimeoutAction, GANG_TIMEOUT);
}

void
test__StartIdleResourceCleanupTimersWhenGangTimeoutIsDisabledAndSessionTimeoutEnabled(
																					  void **state)
{
	IdleSessionGangTimeout = 0;
	get_idle_session_timeout_hook = returns1000_stub;

	expect_value(enable_sig_alarm, delayms, 1000);
	expect_value(enable_sig_alarm, is_statement_timeout, false);
	will_return(enable_sig_alarm, true);

	StartIdleResourceCleanupTimers();

	assert_int_equal(NextTimeoutAction, IDLE_SESSION_TIMEOUT);
}

void
test__DoIdleResourceCleanup_HandlesGangTimeoutWhenSessionTimeoutIsLater(
																		void **state)
{
	IdleSessionGangTimeout = 20000;
	IdleSessionTimeoutCached = 25000;
	NextTimeoutAction = GANG_TIMEOUT;

	will_be_called(DisconnectAndDestroyUnusedGangs);
	expect_value(disable_sig_alarm, is_statement_timeout, false);
	will_return(disable_sig_alarm, true);
	expect_value(enable_sig_alarm, delayms, 5000);
	expect_value(enable_sig_alarm, is_statement_timeout, false);
	will_return(enable_sig_alarm, true);

	DoIdleResourceCleanup();

	assert_int_equal(0, idle_session_timeout_action_calls);
	assert_int_equal(IDLE_SESSION_TIMEOUT, NextTimeoutAction);
}

void
test__DoIdleResourceCleanup_HandlesSessionTimeoutWhenGangTimeoutIsLater(
																		void **state)
{
	IdleSessionGangTimeout = 24000;
	IdleSessionTimeoutCached = 19000;
	NextTimeoutAction = GANG_TIMEOUT;

	will_be_called(DisconnectAndDestroyUnusedGangs);
	expect_value(disable_sig_alarm, is_statement_timeout, false);
	will_return(disable_sig_alarm, true);
	/* cmockery implicitly asserts that enable_sig_alarm is not called */

	DoIdleResourceCleanup();

	/*
	 * TODO: tell Jing behavior; the idle session timmeout action will be
	 * called after 19000
	 */
	assert_int_equal(1, idle_session_timeout_action_calls);
}

void
test__DoIdleResourceCleanup_HandlesSessionTimeoutWithSimultaneousGangTimeout(
																			 void **state)
{
	IdleSessionGangTimeout = 24000;
	IdleSessionTimeoutCached = 24000;
	NextTimeoutAction = GANG_TIMEOUT;

	will_be_called(DisconnectAndDestroyUnusedGangs);
	expect_value(disable_sig_alarm, is_statement_timeout, false);
	will_return(disable_sig_alarm, true);

	DoIdleResourceCleanup();

	assert_int_equal(1, idle_session_timeout_action_calls);
}
void
test__DoIdleResourceCleanup_HandlesSessionTimeoutWithSimultaneousGangTimeoutButNullActionHook(
																							  void **state)
{
	IdleSessionGangTimeout = 20000;
	IdleSessionTimeoutCached = 20000;
	NextTimeoutAction = GANG_TIMEOUT;

	idle_session_timeout_action_hook = NULL;

	will_be_called(DisconnectAndDestroyUnusedGangs);
	expect_value(disable_sig_alarm, is_statement_timeout, false);
	will_return(disable_sig_alarm, true);

	DoIdleResourceCleanup();

	/* doesn't segfault */
}

void
test__DoIdleResourceCleanup_HandlesIdleSessionTimeoutAfterGangTimeout(void **state)
{
	IdleSessionGangTimeout = 5000;
	IdleSessionTimeoutCached = 15000;
	NextTimeoutAction = IDLE_SESSION_TIMEOUT;

	/* not called: DisconnectAndDestroyUnusedGangs, enable_sig_alarm; */
	expect_value(disable_sig_alarm, is_statement_timeout, false);
	will_return(disable_sig_alarm, true);

	DoIdleResourceCleanup();

	assert_int_equal(1, idle_session_timeout_action_calls);
}

void
test__DoIdleResourceCleanup_HandlesIdleSessionTimeoutAfterGangTimeoutWithNullActionHook(
																						void **state)
{
	IdleSessionGangTimeout = 5000;
	IdleSessionTimeoutCached = 15000;
	NextTimeoutAction = IDLE_SESSION_TIMEOUT;

	idle_session_timeout_action_hook = NULL;

	/* not called: DisconnectAndDestroyUnusedGangs, enable_sig_alarm; */
	expect_value(disable_sig_alarm, is_statement_timeout, false);
	will_return(disable_sig_alarm, true);

	DoIdleResourceCleanup();

	/* doesn't segfault */
}

void
test__DoIdleResourceCleanup_HandlesGangTimeoutWhenSessionTimeoutDisabled(void **state)
{
	IdleSessionGangTimeout = 24000;
	IdleSessionTimeoutCached = 0;
	NextTimeoutAction = GANG_TIMEOUT;

	/* not called: enable_sig_alarm */
	will_be_called(DisconnectAndDestroyUnusedGangs);
	expect_value(disable_sig_alarm, is_statement_timeout, false);
	will_return(disable_sig_alarm, true);

	DoIdleResourceCleanup();

	assert_int_equal(0, idle_session_timeout_action_calls);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test(test__StartIdleResourceCleanupTimersWhenIdleGangTimeoutIs0AndNoSessionTimeoutHook),
		unit_test(test__StartIdleResourceCleanupTimersWhenNoGangsExistAndNoSessionTimeoutHook),
		unit_test(test__StartIdleResourceCleanupTimersWhenGangsExistAndNoSessionTimeoutHook),
		unit_test(test__StartIdleResourceCleanupTimersWhenGangsExistAndSessionTimeoutEnabled),
		unit_test(test__StartIdleResourceCleanupTimersWhenGangTimeoutIsDisabledAndSessionTimeoutEnabled),
		test_with_setup_and_teardown(test__DoIdleResourceCleanup_HandlesGangTimeoutWhenSessionTimeoutIsLater),
		test_with_setup_and_teardown(test__DoIdleResourceCleanup_HandlesSessionTimeoutWhenGangTimeoutIsLater),
		test_with_setup_and_teardown(test__DoIdleResourceCleanup_HandlesSessionTimeoutWithSimultaneousGangTimeout),
		test_with_setup_and_teardown(test__DoIdleResourceCleanup_HandlesSessionTimeoutWithSimultaneousGangTimeoutButNullActionHook),
		test_with_setup_and_teardown(test__DoIdleResourceCleanup_HandlesIdleSessionTimeoutAfterGangTimeout),
		test_with_setup_and_teardown(test__DoIdleResourceCleanup_HandlesIdleSessionTimeoutAfterGangTimeoutWithNullActionHook),
		test_with_setup_and_teardown(test__DoIdleResourceCleanup_HandlesGangTimeoutWhenSessionTimeoutDisabled),
	};

	run_tests(tests);
}
