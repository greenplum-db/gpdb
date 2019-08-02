#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "utils/memutils.h"

#include "../nodeShareInputScan.c"

#define FIXED_POINTER_VAL ((LargestIntegralType) 0x0000beef)
#define SHARE_NODE_ENTRY_REFCOUNT 78

/*
 * Tests ExecEagerFreeShareInputScan when plan->share_type = SHARE_NOTSHARED
 * Verifies that all the pointers are set to NULL
 */
static void
test__ExecEagerFreeShareInputScan_SHARE_NOTSHARED(void **state)
{
	ShareInputScanState *sisc = makeNode(ShareInputScanState);
	ShareInputScan *plan = makeNode(ShareInputScan);
	sisc->ss.ps.plan = (Plan*) plan;

	sisc->ts_markpos = (void*)FIXED_POINTER_VAL;
	sisc->ts_pos = (void*)FIXED_POINTER_VAL;
	sisc->ts_state = (void*)FIXED_POINTER_VAL;
	sisc->freed = false;

	plan->share_type = SHARE_NOTSHARED;

	ExecEagerFreeShareInputScan(sisc);

	assert_int_equal(sisc->ts_markpos, NULL);
	assert_int_equal(sisc->ts_pos, NULL);
	assert_int_equal(sisc->ts_state, NULL);
	assert_true(sisc->freed);

	return;
}

/* ==================== ExecEagerFreeShareInputScan ==================== */
/*
 * Tests ExecEagerFreeShareInputScan when plan->share_type = SHARE_MATERIAL
 * Verifies that the tuplestore accessor and the tuplestore state are destroyed,
 * and that all the pointers are set to NULL
 */
static void
test__ExecEagerFreeShareInputScan_SHARE_MATERIAL(void **state)
{
	ShareInputScanState *sisc = makeNode(ShareInputScanState);
	ShareInputScan *plan = makeNode(ShareInputScan);
	sisc->ss.ps.plan = (Plan *) plan;

	sisc->ts_markpos = NULL;
	sisc->ts_pos = (void*) FIXED_POINTER_VAL;
	sisc->ts_state = (GenericTupStore *) palloc0(sizeof(GenericTupStore));
	sisc->ts_state->matstore = (void *) FIXED_POINTER_VAL;
	sisc->freed = false;

	plan->share_type = SHARE_MATERIAL;

	expect_value(ntuplestore_destroy_accessor, acc, FIXED_POINTER_VAL);
	will_be_called(ntuplestore_destroy_accessor);

	expect_value(ntuplestore_is_readerwriter_reader, nts, FIXED_POINTER_VAL);
	will_return(ntuplestore_is_readerwriter_reader, true);

	expect_value(ntuplestore_destroy, ts, FIXED_POINTER_VAL);
	will_be_called(ntuplestore_destroy);


	ShareNodeEntry *shareNodeEntry = makeNode(ShareNodeEntry);
	shareNodeEntry->refcount = SHARE_NODE_ENTRY_REFCOUNT;

	expect_any(ExecGetShareNodeEntry, estate);
	expect_any(ExecGetShareNodeEntry, shareidx);
	expect_value(ExecGetShareNodeEntry, fCreate, false);
	will_return(ExecGetShareNodeEntry, shareNodeEntry);

	ExecEagerFreeShareInputScan(sisc);

	assert_int_equal(sisc->ts_markpos, NULL);
	assert_int_equal(sisc->ts_pos, NULL);
	assert_int_equal(sisc->ts_state, NULL);
	assert_int_equal(shareNodeEntry->refcount, SHARE_NODE_ENTRY_REFCOUNT - 1);
	assert_true(sisc->freed);

	return;
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__ExecEagerFreeShareInputScan_SHARE_NOTSHARED),
		unit_test(test__ExecEagerFreeShareInputScan_SHARE_MATERIAL)
	};

	MemoryContextInit();

	return run_tests(tests);
}
