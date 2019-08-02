#include "../resgroup.c"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#define test_with_setup_and_teardown(test_func) \
	unit_test_setup_teardown(test_func, setup, teardown)

static void
setup(void **state)
{
	/* reset the hook function pointer to avoid test pollution. */
	resgroup_assign_hook = NULL;
}

static void
teardown(void **state)
{
	/* No-op for now. This is just to make CMockery happy. */
}

Oid decide_resource_group_fake_rv = InvalidOid;

static Oid
decide_resource_group_fake()
{
	return decide_resource_group_fake_rv;
}

static void
test__decideResGroupId_when_resgroup_assign_hook_is_not_set(void **state)
{
	will_return(GetUserId, 1);

	expect_value(GetResGroupIdForRole, roleid, 1);
	will_return(GetResGroupIdForRole, 9);

	assert_int_equal(decideResGroupId(), 9);
}

static void
test__decideResGroupId_when_resgroup_assign_hook_is_set(void **state)
{
	decide_resource_group_fake_rv = (Oid) 5;
	resgroup_assign_hook = decide_resource_group_fake;
	assert_int_equal(decideResGroupId(), 5);
}

static void
test__decideResGroupId_when_resgroup_assign_hook_returns_InvalidOid(void **state)
{
	decide_resource_group_fake_rv = InvalidOid;
	resgroup_assign_hook = decide_resource_group_fake;

	will_return(GetUserId, 2);

	expect_value(GetResGroupIdForRole, roleid, 2);
	will_return(GetResGroupIdForRole, 3);

	assert_int_equal(decideResGroupId(), 3);
}

static void
test__CpusetToBitset_bad_arguments(void **state)
{
	char cpuset[200];
	assert_true(!CpusetToBitset(NULL, -1));
	assert_true(!CpusetToBitset(cpuset, -1));
	assert_true(!CpusetToBitset(cpuset, 200));
}

static void
test__CpusetToBitset_normal_case(void **state)
{
	const char *cpusetList[] = {
		"0",
		"0,1,2,3,4-10\n",
		"0,2,4,6,10\n",
		"0-10",
		"4,3,2,1\n",
		"4,3,2,1,,,\n",
		"0-0",
		"1000",
	};
	Bitmapset	*bms1, *bms2;
	int i;

	for (i = 0; i < sizeof(cpusetList) / sizeof(char*); ++i)
	{
		assert_true(CpusetToBitset(cpusetList[i], 1024) != NULL);
	}

	//0
	bms1 = CpusetToBitset(cpusetList[0], 1024);
	bms2 = bms_make_singleton(0);
	assert_true(bms1 != NULL && bms2 != NULL);
	assert_true(bms_equal(bms1, bms2));
	//0,1,2,3,4-10
	bms1 = CpusetToBitset(cpusetList[1], 1024);
	bms2 = NULL;
	for (i = 0; i <= 10; ++i)
	{
		bms2 = bms_union(bms2, bms_make_singleton(i));
	}
	assert_true(bms1 != NULL && bms2 != NULL);
	assert_true(bms_equal(bms1, bms2));
	//0,2,4,6,10
	bms1 = CpusetToBitset(cpusetList[2], 1024);
	bms2 = NULL;
	bms2 = bms_union(bms2, bms_make_singleton(0));
	bms2 = bms_union(bms2, bms_make_singleton(2));
	bms2 = bms_union(bms2, bms_make_singleton(4));
	bms2 = bms_union(bms2, bms_make_singleton(6));
	bms2 = bms_union(bms2, bms_make_singleton(10));
	assert_true(bms1 != NULL && bms2 != NULL);
	assert_true(bms_equal(bms1, bms2));
	//0-10
	bms1 = CpusetToBitset(cpusetList[3], 1024);
	bms2 = NULL;
	for (i = 0; i <= 10; ++i)
	{
		bms2 = bms_union(bms2, bms_make_singleton(i));
	}
	assert_true(bms1 != NULL && bms2 != NULL);
	assert_true(bms_equal(bms1, bms2));
	//4,3,2,1
	bms1 = CpusetToBitset(cpusetList[4], 1024);
	bms2 = NULL;
	for (i = 4; i > 0; --i)
	{
		bms2 = bms_union(bms2, bms_make_singleton(i));
	}
	assert_true(bms1 != NULL && bms2 != NULL);
	assert_true(bms_equal(bms1, bms2));
	//4,3,2,1,,,
	bms1 = CpusetToBitset(cpusetList[5], 1024);
	bms2 = NULL;
	for (i = 4; i > 0; --i)
	{
		bms2 = bms_union(bms2, bms_make_singleton(i));
	}
	assert_true(bms1 != NULL && bms2 != NULL);
	assert_true(bms_equal(bms1, bms2));
	//0-0
	bms1 = CpusetToBitset(cpusetList[6], 1024);
	bms2 = bms_make_singleton(0);
	assert_true(bms1 != NULL && bms2 != NULL);
	assert_true(bms_equal(bms1, bms2));
	//1000
	bms1 = CpusetToBitset(cpusetList[7], 1024);
	bms2 = bms_make_singleton(1000);
	assert_true(bms1 != NULL && bms2 != NULL);
	assert_true(bms_equal(bms1, bms2));
}

static void
test__CpusetToBitset_abnormal_case(void **state)
{
	const char *cpusetList[] = {
		"",
		" ",
		",",
		"-1",
		"abc",
		"12a",
		"1 2 3",
		"0-",
		"-",
		"1-0",
	};
	int i;

	for (i = 0; i < sizeof(cpusetList) / sizeof(char*); ++i)
	{
		assert_true(!CpusetToBitset(cpusetList[i], 1024));
	}
}

static void
test_BitsetToCpuset(void **state)
{
	char cpusetList[1024] = {0};
	Bitmapset *bms = NULL;
	int i;

	//
	bms = NULL;
	for (i = 0; i < 8; ++i)
	{
		bms = bms_union(bms, bms_make_singleton(i));
	}
	BitsetToCpuset(bms, cpusetList, 1024);
	assert_string_equal(cpusetList, "0-7");
	//
	bms = NULL;
	for (i = 0; i < 10; i += 2)
	{
		bms = bms_union(bms, bms_make_singleton(i));
	}
	BitsetToCpuset(bms, cpusetList, 1024);
	assert_string_equal(cpusetList, "0,2,4,6,8");
	//
	bms = NULL;
	for (i = 8; i < 24; ++i)
	{
		bms = bms_union(bms, bms_make_singleton(i));
	}
	BitsetToCpuset(bms, cpusetList, 1024);
	assert_string_equal(cpusetList, "8-23");
	//
	bms = NULL;
	for (i = 0; i < 1024; ++i)
	{
		bms = bms_union(bms, bms_make_singleton(i));
	}
	BitsetToCpuset(bms, cpusetList, 1024);
	assert_string_equal(cpusetList, "0-1023");
	//
	bms = NULL;
	for (i = 0; i < 16; ++i)
	{
		bms = bms_union(bms, bms_make_singleton(i));
	}
	BitsetToCpuset(bms, cpusetList, 1024);
	assert_string_equal(cpusetList, "0-15");
	//
	bms = NULL;
	bms = bms_union(bms, bms_make_singleton(0));
	bms = bms_union(bms, bms_make_singleton(100));
	BitsetToCpuset(bms, cpusetList, 4);
	assert_string_equal(cpusetList, "0,");
}

static void
test_CpusetOperation(void **state)
{
	char cpuset[1024];

	strcpy(cpuset, "0-100");
	cpusetOperation(cpuset, "1-99", 1024, true);
	assert_string_equal(cpuset, "0,100");

	strcpy(cpuset, "0,1,2,3");
	cpusetOperation(cpuset, "0,3", 1024, true);
	assert_string_equal(cpuset, "1-2");

	strcpy(cpuset, "1-10");
	cpusetOperation(cpuset, "3-100", 1024, true);
	assert_string_equal(cpuset, "1-2");

	strcpy(cpuset, "1-10");
	cpusetOperation(cpuset, "0-100", 1024, false);
	assert_string_equal(cpuset, "0-100");

	strcpy(cpuset, "1-10");
	cpusetOperation(cpuset, "100-200", 1024, false);
	assert_string_equal(cpuset, "1-10,100-200");

	strcpy(cpuset, "1-10");
	cpusetOperation(cpuset, "5-15", 1024, false);
	assert_string_equal(cpuset, "1-15");

	strcpy(cpuset, "1-10");
	cpusetOperation(cpuset, "", 1024, false);
	assert_string_equal(cpuset, "1-10");

	strcpy(cpuset, "1-10");
	cpusetOperation(cpuset, "", 1024, true);
	assert_string_equal(cpuset, "1-10");

	//ResGroupOps_Probe();
	//strcpy(cpuset, "1-10");
	//cpusetOperation(cpuset, "0-100", 1024, true);
	//assert_string_equal(cpuset, "0");
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
			test_with_setup_and_teardown(test__decideResGroupId_when_resgroup_assign_hook_is_not_set),
			test_with_setup_and_teardown(test__decideResGroupId_when_resgroup_assign_hook_is_set),
			test_with_setup_and_teardown(test__decideResGroupId_when_resgroup_assign_hook_returns_InvalidOid),
			unit_test(test__CpusetToBitset_bad_arguments),
			unit_test(test__CpusetToBitset_normal_case),
			unit_test(test__CpusetToBitset_abnormal_case),
			unit_test(test_BitsetToCpuset),
			unit_test(test_CpusetOperation),
	};

	MemoryContextInit();
	run_tests(tests);
}
