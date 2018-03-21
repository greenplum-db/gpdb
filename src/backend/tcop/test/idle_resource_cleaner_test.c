#include "../idle_resource_cleaner.c"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

void
test__StartIdleResourceCleanupTimersWhenNoGangsExist(
	void **state)
{
	IdleSessionGangTimeout = 10000;

	will_return(GangsExist, false);
	/* cmockery implicitly asserts that enable_sig_alarm is not called */

	StartIdleResourceCleanupTimers();
}

void
test__StartIdleResourceCleanupTimersWhenGangsExist(
	void **state)
{
	IdleSessionGangTimeout = 10000;

	will_return(GangsExist, true);
	/* cmockery implicitly asserts that enable_sig_alarm is not called */
	will_return(enable_sig_alarm, true);

	expect_value(enable_sig_alarm, delayms, 10000);
	expect_value(enable_sig_alarm, is_statement_timeout, false);

	StartIdleResourceCleanupTimers();
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__StartIdleResourceCleanupTimersWhenNoGangsExist),
		unit_test(test__StartIdleResourceCleanupTimersWhenGangsExist),

	};

	run_tests(tests);
}
