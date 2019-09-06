#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "utils/memutils.h"

#include "../memquota.c"

/* ==================== ComputeMemLimitForChildGroups ==================== */

/*
 * Tests that EagerFree computes the memory limits for child groups accurately
 * No rounding error is allowed due to scaleFactor. The sum of the child group
 * memory amounts must equal the sum of the parent group (MPP-23130)
 */
static void
test__ComputeMemLimitForChildGroups_rounding(void **state)
{
	OperatorGroupNode *parentGroup = (OperatorGroupNode *) palloc0(sizeof(OperatorGroupNode));
	OperatorGroupNode *childGroup1 = (OperatorGroupNode *) palloc0(sizeof(OperatorGroupNode));
	OperatorGroupNode *childGroup2 = (OperatorGroupNode *) palloc0(sizeof(OperatorGroupNode));

	parentGroup->childGroups = NIL;
	parentGroup->childGroups = lappend(parentGroup->childGroups, childGroup1);
	parentGroup->childGroups = lappend(parentGroup->childGroups, childGroup2);

	*gp_resmanager_memory_policy_auto_fixed_mem = 100;

	/* Case where rounding does not cause error */
	parentGroup->groupMemKB = 3000;
	childGroup1->numMemIntenseOps = 2;
	childGroup2->numMemIntenseOps = 3;

	childGroup1->numNonMemIntenseOps = 2;
	childGroup2->numNonMemIntenseOps = 3;

	/*
	 * For these values, we have the following:
	 *   parentGroupMemKB = 3000
	 *
	 *   childGroupMemKB_1 = 1200 ; SF_1 = 0.4
	 *   ===> Calculation with rounding: (1200 / 0.4) * 0.4 = 1200
	 *
	 *   childGroupMemKB_2 = 1800 ; SF_2 = 0.6
	 *   ===> Calculation with rounding: (1800 / 0.6) * 0.6 = 1800
	 *
	 *   Assert: 1200 + 1800 = 3000, PASS
	 */

	ComputeMemLimitForChildGroups(parentGroup);

	assert_int_equal(childGroup1->groupMemKB + childGroup2->groupMemKB, parentGroup->groupMemKB);


	/* Case where rounding would cause error */
	MemSet(childGroup1, 0, sizeof(OperatorGroupNode));
	MemSet(childGroup2, 0, sizeof(OperatorGroupNode));

	parentGroup->groupMemKB = 3000;
	childGroup1->numMemIntenseOps = 2;
	childGroup2->numMemIntenseOps = 3;

	childGroup1->numNonMemIntenseOps = 2;
	childGroup2->numNonMemIntenseOps = 4;

	/*
	 * For these values, we have the following:
	 *   parentGroupMemKB = 3000
	 *
	 *   childGroupMemKB_1 = 1160 ; SF_1 = 0.386667
	 *   ===> Calculation with rounding: (1160 /  0.386667) *  0.386667 = 1160
	 *
	 *   childGroupMemKB_2 = 1840 ; SF_2 = 0.613333
	 *   ===> Calculation with rounding: (1840 / 0.613333) * 0.613333 = 1839 (!)
	 *
	 *   Assert: 1160 + 1839 = 2999, FAIL!!
	 */

	ComputeMemLimitForChildGroups(parentGroup);

	assert_int_equal(childGroup1->groupMemKB + childGroup2->groupMemKB, parentGroup->groupMemKB);

}

/* ==================== main ==================== */
int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__ComputeMemLimitForChildGroups_rounding)
	};

	MemoryContextInit();

	return run_tests(tests);
}
