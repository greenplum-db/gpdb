#include "cmockery_gp.h"

#include "pg_upgrade.h"
#include "portability/instr_time.h"

#include "greenplum/pg_upgrade_greenplum.h"
#include "pg_upgrade_dummies.c"


ClusterInfo new_cluster;
ClusterInfo old_cluster;

bool
is_show_progress_mode(void)
{
	return false;
}

void
pg_log(eLogType type, const char *fmt,...)
{

}

static void
test_duration_ms(void **state) {
	size_t len=20;
	char elapsed_time[len];
	instr_time t = {0, 908234};
	duration(t, elapsed_time, len);
	assert_string_equal(elapsed_time, "908.234ms");
}

static void
test_duration_second(void **state) {
	size_t len=20;
	char elapsed_time[len];
	instr_time t = {0, 9082340};
	duration(t, elapsed_time, len);
	assert_string_equal(elapsed_time, "9s");
}

static void
test_duration_minute_second(void **state) {
	size_t len=20;
	char elapsed_time[len];
	instr_time t = {90, 9082340};
	duration(t, elapsed_time, len);
	assert_string_equal(elapsed_time, "1m39s");
}

static void
test_duration_hour_minute_second(void **state) {
	size_t len=20;
	char elapsed_time[len];
	instr_time t = {9000, 9082340};
	duration(t, elapsed_time, len);
	assert_string_equal(elapsed_time, "2h30m9s");
}

int
main(int argc, char *argv[])
{
	const UnitTest tests[] = {
			unit_test(test_duration_ms),
			unit_test(test_duration_second),
			unit_test(test_duration_minute_second),
			unit_test(test_duration_hour_minute_second),
	};
	return run_tests(tests);
}