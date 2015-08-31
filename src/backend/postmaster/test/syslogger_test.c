#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../syslogger.c"

int
mkdir(const char *path, mode_t mode)
{
    check_expected(path);
    check_expected(mode);
    return (int)mock();
}

FILE*
fopen(const char *restrict filename, const char *restrict mode)
{
    check_expected(filename);
    check_expected(mode);
    return (FILE*)mock();
}

int
setvbuf(FILE *restrict stream, char *restrict buf, int type, size_t size)
{
    check_expected(buf);
    check_expected(type);
    check_expected(size);
    return (int)mock();
}

time_t
time(time_t *unused)
{
    return (time_t)mock();
}


void
test__open_alert_log_file__NonGucOpen(void **state)
{
    gpperfmon_log_alert_level = GPPERFMON_LOG_ALERT_LEVEL_NONE;
    open_alert_log_file();
    assert_false(alert_log_level_opened);
}

void 
test__open_alert_log_file__NonMaster(void **state)
{
    Gp_entry_postmaster = false;
    gpperfmon_log_alert_level = GPPERFMON_LOG_ALERT_LEVEL_WARNING;
    open_alert_log_file();
    assert_false(alert_log_level_opened);
}

void 
test__open_alert_log_file__OpenAlertLog(void **state)
{
    Gp_entry_postmaster = true;
    Gp_role = GP_ROLE_DISPATCH;
    gpperfmon_log_alert_level = GPPERFMON_LOG_ALERT_LEVEL_WARNING;

    alert_file_pattern = "alert_log";
    expect_string(mkdir, path, gp_perf_mon_directory);
    expect_value(mkdir, mode, 0700);
    will_return(mkdir, 0);
    will_return(time, 12345);
    expect_string(fopen, filename, "gpperfmon/logs/alert_log.12345");
    expect_string(fopen, mode, "a");
    will_return(fopen, (FILE*)0x0DA7ABA53);
    expect_value(setvbuf, buf, NULL);
    expect_value(setvbuf, type, LBF_MODE);
    expect_value(setvbuf, size, 0);
    will_return(setvbuf, 0);
    open_alert_log_file();
    assert_true(alert_log_level_opened);
}

int
main(int argc, char* argv[]) {
    cmockery_parse_arguments(argc, argv);

    const UnitTest tests[] = {
    		unit_test(test__open_alert_log_file__NonGucOpen),
    		unit_test(test__open_alert_log_file__NonMaster),
    		unit_test(test__open_alert_log_file__OpenAlertLog)
    };

	MemoryContextInit();

    return run_tests(tests);
}
