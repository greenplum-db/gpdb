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
static void cleanup_test(void);

void
test__set_gp_replication_config_synchronous_standby_names_to_empty(void **state)
{
	setup_test();

	set_gp_replication_config("synchronous_standby_names", "");

	assert_true(check_result("synchronous_standby_names = ''"));

	cleanup_test();
}

void
test__set_gp_replication_config_synchronous_standby_names_to_star(void **state)
{
	setup_test();

	set_gp_replication_config("synchronous_standby_names", "*");

	assert_true(check_result("synchronous_standby_names = '*'"));

	cleanup_test();
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

	cleanup_test();
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

	cleanup_test();
}

void
test__validate_gp_replication_conf_option(void **state)
{
	struct config_generic *record;
	build_guc_variables();

	/*
	 * PGC_BOOL
	 */
	record = find_option("optimizer", false, LOG);

	PG_TRY();
	{
		validate_gp_replication_conf_option(record, "should output fatal", ERROR);
		fail();
	}
	PG_CATCH();
	{
	}
	PG_END_TRY();

	assert_true(validate_gp_replication_conf_option(record, "true", NOTICE));
	assert_false(validate_gp_replication_conf_option(record, "should output fatal", NOTICE));

	/*
	 * PGC_INT
	 */
	record = find_option("max_connections", false, LOG);
	assert_true(validate_gp_replication_conf_option(record, "10", NOTICE));
	assert_false(validate_gp_replication_conf_option(record, "-10", NOTICE));

	/*
	 * PGC_REAL
	 */
	record = find_option("cursor_tuple_fraction", false, LOG);
	assert_true(validate_gp_replication_conf_option(record, "0.0", NOTICE));
	assert_false(validate_gp_replication_conf_option(record, "2.0", NOTICE));

	/*
	 * PGC_STRING
	 */
	char valid_name[NAMEDATALEN];
	char invalid_name[NAMEDATALEN + 10];
	sprintf(valid_name, "%0*d", sizeof(valid_name)-1, 0);
	sprintf(invalid_name, "%0*d", sizeof(invalid_name)-1, 0);

	record = find_option("client_encoding", false, LOG);
	assert_true(validate_gp_replication_conf_option(record, valid_name, NOTICE));
	assert_false(validate_gp_replication_conf_option(record, invalid_name, NOTICE));
}

static void
setup_test(void)
{
	will_return(superuser, true);

	expect_any(LWLockAcquire, lockid);
	expect_any(LWLockAcquire, mode);
	will_be_called(LWLockAcquire);

	expect_any(LWLockRelease, lockid);
	will_be_called(LWLockRelease);

	gp_replication_config_filename = "/tmp/test_gp_replication.conf";
	build_guc_variables();
}

static void
cleanup_test(void)
{
	unlink(gp_replication_config_filename);
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
		unit_test(test__validate_gp_replication_conf_option)
	};
	return run_tests(tests);
}