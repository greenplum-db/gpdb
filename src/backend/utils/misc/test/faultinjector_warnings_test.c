#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"


#include "postgres.h"
#include "nodes/pg_list.h"
#include "utils/memutils.h"

#include "utils/faultinjector.h"
#include "../faultinjector_warnings.h"


static bool warning_was_called = false;


static void
some_warning(FaultInjectorEntry_s faultEntry)
{
	warning_was_called = true;
}

static void
setup()
{
	warning_was_called = false;

	warnings_init();
}

static void
test_fault_injection_warnings_can_be_added_and_emitted(void **state)
{
	setup();

	FaultInjectorEntry_s faultEntry;
	faultEntry.startOccurrence = 0;

	register_fault_injection_warning(some_warning);
	emit_warnings(faultEntry);

	assert_true(warning_was_called);
}

static void
test_fault_injection_warnings_not_called_when_empty(void **state)
{
	setup();

	FaultInjectorEntry_s faultEntry;
	faultEntry.startOccurrence = 0;

	emit_warnings(faultEntry);

	assert_false(warning_was_called);
}

int main(int argc, char* argv[]) {
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_fault_injection_warnings_can_be_added_and_emitted),
		unit_test(test_fault_injection_warnings_not_called_when_empty)
	};

	MemoryContextInit();

	return run_tests(tests);
}
