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
test__StartIdleResourceCleanupTimersWhenIdleGangTimeoutIs0AndNoSessionTimeoutHook(
																				  void **state)
{
	IdleSessionGangTimeout = 0;
	get_idle_session_timeout_hook = NULL;

	/*
	 * cmockery implicitly asserts that cdbcomponent_segdbsExist and enable_sig_alarm are
	 * not called
	 */

	StartIdleResourceCleanupTimers();
}

void
test__StartIdleResourceCleanupTimersWhenNoGangsExistAndNoSessionTimeoutHook(
																			void **state)
{
	IdleSessionGangTimeout = 10000;

	will_return(cdbcomponent_segdbsExist, false);
	/* cmockery implicitly asserts that enable_sig_alarm is not called */

	StartIdleResourceCleanupTimers();
}

void
test__StartIdleResourceCleanupTimersWhenGangsExistAndNoSessionTimeoutHook(
																		  void **state)
{
	IdleSessionGangTimeout = 10000;

	NextTimeoutAction = -1;

	will_return(cdbcomponent_segdbsExist, true);
	expect_value(enable_sig_alarm, delayms, 10000);
	expect_value(enable_sig_alarm, is_statement_timeout, false);
	will_return(enable_sig_alarm, true);

	StartIdleResourceCleanupTimers();

	assert_int_equal(NextTimeoutAction, GANG_TIMEOUT);
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
