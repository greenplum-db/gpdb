#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"

#include "../pg_upgrade.h"

LogOpts		log_opts;

void
report_status(eLogType type, const char *fmt,...)
{
	check_expected(type);
	check_expected(fmt);
	mock();
}

void
end_progress_output(void)
{
	mock();
}

void
prep_status(const char *fmt,...)
{
	check_expected(fmt);
	mock();
}

void
pg_log(eLogType type, const char *fmt,...)
{
	check_expected(type);
	check_expected(fmt);
	mock();
}

void
pg_fatal(const char *fmt,...)
{
	check_expected(fmt);
	mock();
}

void
check_ok(void)
{
	mock();
}

char *
quote_identifier(const char *s)
{
	check_expected(s);
	return (char *)mock();
}

void
appendShellString(PQExpBuffer buf, const char *str)
{
	check_expected(buf);
	check_expected(str);
	mock();
}

void
appendConnStrVal(PQExpBuffer buf, const char *str)
{
	check_expected(buf);
	check_expected(str);
	mock();
}

void
appendPsqlMetaConnect(PQExpBuffer buf, const char *dbname)
{
	check_expected(buf);
	check_expected(dbname);
	mock();
}

int
get_user_info(char **user_name_p)
{
	check_expected(user_name_p);
	return (int)mock();
}

const char *
getErrorText(void)
{
	return (const char *)mock();
}

unsigned int
str2uint(const char *str)
{
	check_expected(str);
	return (unsigned int)mock();
}

void
pg_putenv(const char *var, const char *val)
{
	check_expected(var);
	check_expected(val);
	mock();
}
