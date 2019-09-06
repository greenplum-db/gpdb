#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "utils/memutils.h"

#include "../syslogger.c"

time_t
time(time_t *unused)
{
    return (time_t)mock();
}


static void
test__open_alert_log_file__NonGucOpen(void **state)
{
    gpperfmon_log_alert_level = GPPERFMON_LOG_ALERT_LEVEL_NONE;
    open_alert_log_file();
    assert_false(alert_log_level_opened);
}

static void 
test__open_alert_log_file__NonMaster(void **state)
{
    gpperfmon_log_alert_level = GPPERFMON_LOG_ALERT_LEVEL_WARNING;
    open_alert_log_file();
    assert_false(alert_log_level_opened);
}

static void 
test__logfile_getname(void **state)
{
    char *alert_file_name;

    alert_file_pattern = "alert_log";
    will_return(time, 12345);

	log_timezone = pg_tzset("GMT");

	alert_file_name = logfile_getname(time(NULL), NULL, "gpperfmon/logs", "alert_log-%F");
	assert_true(strcmp(alert_file_name, "gpperfmon/logs/alert_log-1970-01-01") == 0);
}

int
main(int argc, char* argv[]) {
    cmockery_parse_arguments(argc, argv);

    const UnitTest tests[] = {
    		unit_test(test__open_alert_log_file__NonGucOpen),
    		unit_test(test__open_alert_log_file__NonMaster),
    		unit_test(test__logfile_getname)
    };

	MemoryContextInit();

    return run_tests(tests);
}
