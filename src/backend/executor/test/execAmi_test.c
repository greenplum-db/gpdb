#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../execAmi.c"

/* ==================== ExecEagerFree ==================== */
/*
 * Tests that ExecEageFree calls the new ExecEagerFreeShareInputScan
 * function when the input is a ShareInputScanState
 */
void
test__ExecEagerFree_ExecEagerFreeShareInputScan(void **state)
{
	ShareInputScanState *sisc = makeNode(ShareInputScanState);

	expect_value(ExecEagerFreeShareInputScan, node, sisc);
	will_be_called(ExecEagerFreeShareInputScan);

	ExecEagerFree(sisc);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__ExecEagerFree_ExecEagerFreeShareInputScan)
	};

	MemoryContextInit();

	return run_tests(tests);
}
