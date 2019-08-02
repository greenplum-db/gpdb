#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "utils/memutils.h"

/* Define UNIT_TESTING so that the extension can skip declaring PG_MODULE_MAGIC */
#define UNIT_TESTING

/* include unit under test */
#include "../src/pxfutils.c"

/* helper functions */
static void restore_env(const char *name, const char *value);

static void
test_normalize_key_name(void **state)
{
	char	   *replaced = normalize_key_name("bar");

	assert_string_equal(replaced, "X-GP-OPTIONS-BAR");
	pfree(replaced);

	replaced = normalize_key_name("FOO");
	assert_string_equal(replaced, "X-GP-OPTIONS-FOO");
	pfree(replaced);

	/* test null string */
	MemoryContext old_context = CurrentMemoryContext;

	PG_TRY();
	{
		replaced = normalize_key_name(NULL);
		assert_false("Expected Exception");
	}
	PG_CATCH();
	{
		MemoryContextSwitchTo(old_context);
		ErrorData  *edata = CopyErrorData();

		assert_true(edata->elevel == ERROR);
		char	   *expected_message = pstrdup("internal error in pxfutils.c:normalize_key_name, parameter key is null or empty");

		assert_string_equal(edata->message, expected_message);
		pfree(expected_message);
	}
	PG_END_TRY();

	/* test empty string */
	PG_TRY();
	{
		replaced = normalize_key_name("");
		assert_false("Expected Exception");
	}
	PG_CATCH();
	{
		MemoryContextSwitchTo(old_context);
		ErrorData  *edata = CopyErrorData();

		assert_true(edata->elevel == ERROR);
		char	   *expected_message = pstrdup("internal error in pxfutils.c:normalize_key_name, parameter key is null or empty");

		assert_string_equal(edata->message, expected_message);
		pfree(expected_message);
	}
	PG_END_TRY();

}

static void
test_get_authority(void **state)
{
	char	*authority = get_authority();
	assert_string_equal(authority, "localhost:5888");
	pfree(authority);
}

static void
test_get_authority_from_env(void **state)
{
	const char *pStore = getenv(ENV_PXF_PORT);
	const char *hStore = getenv(ENV_PXF_HOST);

	setenv(ENV_PXF_HOST, "bar.com", 1);
	setenv(ENV_PXF_PORT, "777", 1);

	char	*authority = get_authority();
	assert_string_equal(authority, "bar.com:777");
	pfree(authority);

	restore_env(ENV_PXF_PORT, pStore);
	restore_env(ENV_PXF_HOST, hStore);
}

static void
test_get_pxf_port_fails_with_invalid_value(void **state)
{
	// store the existing value of the environment variable
	const char *store = getenv(ENV_PXF_PORT);
	setenv(ENV_PXF_PORT, "bad_value", 1);

	MemoryContext old_context = CurrentMemoryContext;

	PG_TRY();
	{
		get_pxf_port();
	}
	PG_CATCH();
	{
		MemoryContextSwitchTo(old_context);
		ErrorData  *edata = CopyErrorData();

		assert_true(edata->elevel == ERROR);
		char	   *expected_message = pstrdup("unable to parse PXF port number PXF_PORT=bad_value");

		assert_string_equal(edata->message, expected_message);
		pfree(expected_message);
	}
	PG_END_TRY();

	// restore environment variable
	restore_env(ENV_PXF_PORT, store);
}

static void
test_get_pxf_port_with_env_var_set(void **state)
{
	// store the existing value of the environment variable
	const char *store = getenv(ENV_PXF_PORT);
	setenv(ENV_PXF_PORT, "6162", 1);

	const int port = get_pxf_port();
	assert_int_equal(port, 6162);

	// restore environment variable
	restore_env(ENV_PXF_PORT, store);
}

static void
test_get_pxf_host_with_env_var_set(void **state)
{
	// store the existing value of the environment variable
	const char *store = getenv(ENV_PXF_HOST);
	setenv(ENV_PXF_HOST, "foo.com", 1);

	const char *host = get_pxf_host();
	assert_string_equal(host, "foo.com");

	// restore environment variable
	restore_env(ENV_PXF_HOST, store);
}

static void
restore_env(const char *name, const char *value)
{
	if (value)
		setenv(name, value, 1);
	else
		unsetenv(name);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const		UnitTest tests[] = {
		unit_test(test_normalize_key_name),
		unit_test(test_get_authority),
		unit_test(test_get_authority_from_env),
		unit_test(test_get_pxf_port_with_env_var_set),
		unit_test(test_get_pxf_host_with_env_var_set),
		unit_test(test_get_pxf_port_fails_with_invalid_value)
	};

	MemoryContextInit();

	return run_tests(tests);
}
