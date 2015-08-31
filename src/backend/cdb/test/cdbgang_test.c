#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../cdbgang.c"

/*
 * Make sure resetSessionForPrimaryGangLoss doesn't access catalog.
 */
static void
test__resetSessionForPrimaryGangLoss(void **state)
{
	PROC_HDR	dummyGlobal;
	PGPROC		dummyProc;

	will_be_called(RedZoneHandler_DetectRunawaySession);
	will_return(ProcCanSetMppSessionId, true);

	/* Assum we have created a temporary namespace. */
	will_return(TempNamespaceOidIsValid, true);
	will_return(ResetTempNamespace, 9999);
	OldTempNamespace = InvalidOid;

	/* Assume we are out of transaction. */
	CurrentResourceOwner = NULL;

	resetSessionForPrimaryGangLoss();
	assert_int_equal(OldTempNamespace, 9999);
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__resetSessionForPrimaryGangLoss),
	};

	MemoryContextInit();

	return run_tests(tests);
}
