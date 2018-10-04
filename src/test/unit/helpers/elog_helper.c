#include "elog_helper.h"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "c.h"

#include "postgres.h"

#define PG_RE_THROW() siglongjmp(*PG_exception_stack, 1)

/*
 * This method will emulate the real ExceptionalCondition
 * function by re-throwing the exception, essentially falling
 * back to the next available PG_CATCH();
 */
void
_ExceptionalCondition()
{
	PG_RE_THROW();
}

void
expect_elog(int log_level)
{
	expect_any(elog_start, filename);
	expect_any(elog_start, lineno);
	expect_any(elog_start, funcname);
	will_be_called(elog_start);
	if (log_level < ERROR)
		will_be_called(elog_finish);
	else
		will_be_called_with_sideeffect(elog_finish,
		                               &_ExceptionalCondition,
		                               NULL);

	expect_value(elog_finish, elevel, log_level);
	expect_any(elog_finish, fmt);
}