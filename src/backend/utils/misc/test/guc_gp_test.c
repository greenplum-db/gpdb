/*
 * Unit tests for the functions in guc_gp.c.
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

#include "cmockery.h"
#include "../guc_gp.c"

static bool check_result(const char *expected_result);
static void setup_test(void);

void
test__set_gp_replication_config_synchronous_standby_names_to_empty(void **state)
{
	setup_test();

	set_gp_replication_config("synchronous_standby_names", "");

	assert_true(check_result("synchronous_standby_names = ''"));
}

void
test__set_gp_replication_config_synchronous_standby_names_to_star(void **state)
{
	setup_test();

	set_gp_replication_config("synchronous_standby_names", "*");

	assert_true(check_result("synchronous_standby_names = '*'"));
}

void
test__set_gp_replication_config_synchronous_standby_names_to_null(void **state)
{
	/*
	 * initialize the guc to '*'
	 */
	setup_test();
	set_gp_replication_config("synchronous_standby_names", "*");
	assert_true(check_result("synchronous_standby_names = '*'"));

	setup_test();
	set_gp_replication_config("synchronous_standby_names", NULL);

	/*
	 * it should be removed
	 */
	assert_false(check_result("synchronous_standby_names = '*'"));
}

void
test__set_gp_replication_config_new_guc_to_null(void **state)
{
	/*
	 * initialize the guc to '*'
	 */
	setup_test();
	set_gp_replication_config("synchronous_standby_names", "*");
	assert_true(check_result("synchronous_standby_names = '*'"));

	/*
	 * this is a NO-OP, since NULL valued GUC will be removed.
	 */
	setup_test();
	set_gp_replication_config("gp_select_invisible", NULL);

	/*
	 * it should be removed
	 */
	assert_true(check_result("synchronous_standby_names = '*'"));
	assert_false(check_result("gp_select_invisible = false"));
}

static void
setup_test(void)
{
	will_return(superuser, true);

	expect_any(LWLockAcquire, l);
	expect_any(LWLockAcquire, mode);
	will_return(LWLockAcquire, true);

	expect_any(LWLockRelease, l);
	will_be_called(LWLockRelease);

	build_guc_variables();
}

static bool
check_result(const char *expected_result)
{
	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	bool found = false;

	fp = fopen(gp_replication_config_filename, "r");
	assert_true(fp);

	while ((read = getline(&line, &len, fp)) != -1)
	{
		if (strncmp(line, expected_result, strlen(expected_result)) == 0)
		{
			found = true;
			break;
		}
	}

	fclose(fp);
	if (line)
		free(line);

	return found;
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test__set_gp_replication_config_synchronous_standby_names_to_empty),
		unit_test(test__set_gp_replication_config_synchronous_standby_names_to_star),
		unit_test(test__set_gp_replication_config_synchronous_standby_names_to_null),
		unit_test(test__set_gp_replication_config_new_guc_to_null),
	};

	MemoryContextInit();
	return run_tests(tests);
}
