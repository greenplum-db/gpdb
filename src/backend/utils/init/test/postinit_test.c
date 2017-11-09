#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

/* Fetch definition of PG_exception_stack */
#include "postgres.h"

#define PG_RE_THROW() siglongjmp(*PG_exception_stack, 1)

#define errfinish errfinish_impl

int errfinish_impl(int dummy __attribute__((unused)),...)
{
	PG_RE_THROW();
}

static void expect_ereport(int expect_elevel)
{
	expect_any(errmsg, fmt);
	will_be_called(errmsg);

	expect_any(errcode, sqlerrcode);
	will_be_called(errcode);

	expect_value(errstart, elevel, expect_elevel);
	expect_any(errstart, filename);
	expect_any(errstart, lineno);
	expect_any(errstart, funcname);
	expect_any(errstart, domain);
	if (expect_elevel < ERROR)
	{
		will_return(errstart, false);
	}
    else
    {
		will_return(errstart, true);
    }
}

#include "../postinit.c"

static void
test_check_superuser_connection_limit_error(void **state)
{
	am_ftshandler = false;

	expect_value(HaveNFreeProcs, n, RESERVED_FTS_CONNECTIONS);
	will_return(HaveNFreeProcs, false);

	expect_ereport(FATAL);

	expect_value(errSendAlert, sendAlert, true);
	will_be_called(errSendAlert);

	/*
	 * Expect ERROR
	 */
	PG_TRY();
	{
		check_superuser_connection_limit();
		fail();
	}
	PG_CATCH();
	{
	}
	PG_END_TRY();
}

static void
test_check_superuser_connection_limit_ok_with_free_procs(void **state)
{
	am_ftshandler = false;

	expect_value(HaveNFreeProcs, n, RESERVED_FTS_CONNECTIONS);
	will_return(HaveNFreeProcs, true);

	/*
	 * Expect OK
	 */
	check_superuser_connection_limit();
}

static void
test_check_superuser_connection_limit_ok_for_ftshandler(void **state)
{
	am_ftshandler = true;

	/*
	 * Expect OK
	 */
	check_superuser_connection_limit();
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_check_superuser_connection_limit_ok_with_free_procs),
		unit_test(test_check_superuser_connection_limit_ok_for_ftshandler),
		unit_test(test_check_superuser_connection_limit_error),
	};

	return run_tests(tests);
}
