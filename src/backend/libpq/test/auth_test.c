#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "utils/memutils.h"

#include "../auth.c"

#ifdef ENABLE_GSS

static void
pg_authid_search_will_return(const char *user_name, HeapTuple retval)
{
	expect_value(SearchSysCache, cacheId, AUTHNAME);
	expect_string(SearchSysCache, key1, user_name);
	expect_any(SearchSysCache, key2);
	expect_any(SearchSysCache, key3);
	expect_any(SearchSysCache, key4);

	will_return(SearchSysCache, retval);

	if (retval != NULL)
	{
		expect_value(ReleaseSysCache, tuple, retval);
		will_be_called(ReleaseSysCache);
	}
}

static void
pg_authid_tuple_attribute_will_be(HeapTuple tuple, AttrNumber attr, Datum retval)
{
	expect_value(SysCacheGetAttr, cacheId, AUTHNAME);
	expect_value(SysCacheGetAttr, tup, tuple);
	expect_value(SysCacheGetAttr, attributeNumber, attr);
	expect_any(SysCacheGetAttr, isNull);

	/*
	 * The cast to bool here is required; otherwise will_assign_value assumes it
	 * has an int's worth of space to set and we smash the stack.
	 */
	will_assign_value(SysCacheGetAttr, isNull, (bool) (retval == 0));
	will_return(SysCacheGetAttr, retval);
}

/* Unit tests for check_valid_until_for_gssapi() function */
void
test_checkValidUntilForGssapi_returns_error_for_nonexistent_user(void **state)
{
	int			result;
	Port	   *port = (Port *) malloc(sizeof(Port));

	port->user_name = "foo";
	pg_authid_search_will_return(port->user_name, NULL);

	result = check_valid_until_for_gssapi(port);
	assert_true(result == STATUS_ERROR);
}

void
test_checkValidUntilForGssapi_returns_ok_for_user_with_null_validuntil(void **state)
{
	int			result;
	Port	   *port = (Port *) malloc(sizeof(Port));
	HeapTuple	tuple = malloc(sizeof(*tuple));

	port->user_name = "foo";
	pg_authid_search_will_return(port->user_name, tuple);
	pg_authid_tuple_attribute_will_be(tuple, Anum_pg_authid_rolvaliduntil, (Datum) 0);

	result = check_valid_until_for_gssapi(port);
	assert_true(result == STATUS_OK);
}

void
test_checkValidUntilForGssapi_returns_error_for_user_with_expired_validuntil(void **state)
{
	int			result;
	Port	   *port = (Port *) malloc(sizeof(Port));
	HeapTuple	tuple = malloc(sizeof(*tuple));

	port->user_name = "foo";
	pg_authid_search_will_return(port->user_name, tuple);
	pg_authid_tuple_attribute_will_be(tuple, Anum_pg_authid_rolvaliduntil, (Datum) 10293842);
	will_return(GetCurrentTimestamp, 10293843);

	result = check_valid_until_for_gssapi(port);
	assert_true(result == STATUS_ERROR);
}

void
test_checkValidUntilForGssapi_returns_ok_for_user_with_unexpired_validuntil(void **state)
{
	int			result;
	Port	   *port = (Port *) malloc(sizeof(Port));
	HeapTuple	tuple = malloc(sizeof(*tuple));

	port->user_name = "foo";
	pg_authid_search_will_return(port->user_name, tuple);
	pg_authid_tuple_attribute_will_be(tuple, Anum_pg_authid_rolvaliduntil, (Datum) 10293844);
	will_return(GetCurrentTimestamp, 10293843);

	result = check_valid_until_for_gssapi(port);
	assert_true(result == STATUS_OK);
}
#endif

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
#ifdef ENABLE_GSS
		unit_test(test_checkValidUntilForGssapi_returns_error_for_nonexistent_user),
		unit_test(test_checkValidUntilForGssapi_returns_ok_for_user_with_null_validuntil),
		unit_test(test_checkValidUntilForGssapi_returns_error_for_user_with_expired_validuntil),
		unit_test(test_checkValidUntilForGssapi_returns_ok_for_user_with_unexpired_validuntil),
#endif
	};

	MemoryContextInit();

	return run_tests(tests);
}
