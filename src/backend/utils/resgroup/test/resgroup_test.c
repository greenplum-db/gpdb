#include "../resgroup.c"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#define test_with_setup_and_teardown(test_func) \
	unit_test_setup_teardown(test_func, setup, teardown)

void
setup(void **state)
{
	/* reset the hook function pointer to avoid test pollution. */
	resgroup_assign_hook = NULL;
}

void
teardown(void **state)
{
	/* No-op for now. This is just to make CMockery happy. */
}

Oid decide_resource_group_fake_rv = InvalidOid;

Oid
decide_resource_group_fake()
{
	return decide_resource_group_fake_rv;
}

void
test__decideResGroupId_when_resgroup_assign_hook_is_not_set(void **state)
{
	will_return(GetUserId, 1);

	expect_value(GetResGroupIdForRole, roleid, 1);
	will_return(GetResGroupIdForRole, 9);

	assert_int_equal(decideResGroupId(), 9);
}

void
test__decideResGroupId_when_resgroup_assign_hook_is_set(void **state)
{
	decide_resource_group_fake_rv = (Oid) 5;
	resgroup_assign_hook = decide_resource_group_fake;
	assert_int_equal(decideResGroupId(), 5);
}

void
test__decideResGroupId_when_resgroup_assign_hook_returns_InvalidOid(void **state)
{
	decide_resource_group_fake_rv = InvalidOid;
	resgroup_assign_hook = decide_resource_group_fake;

	will_return(GetUserId, 2);

	expect_value(GetResGroupIdForRole, roleid, 2);
	will_return(GetResGroupIdForRole, 3);

	assert_int_equal(decideResGroupId(), 3);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
			test_with_setup_and_teardown(test__decideResGroupId_when_resgroup_assign_hook_is_not_set),
			test_with_setup_and_teardown(test__decideResGroupId_when_resgroup_assign_hook_is_set),
			test_with_setup_and_teardown(test__decideResGroupId_when_resgroup_assign_hook_returns_InvalidOid),
	};

	run_tests(tests);
}
