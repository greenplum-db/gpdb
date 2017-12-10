#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "../auth.c"

#ifdef ENABLE_GSS

/* Unit tests for check_valid_until_for_gssapi() function */
void
test_checkValidUntilForGssapi1(void **state)
{
	int			result = -1;
	Port	   *port = (Port *) malloc(sizeof(Port));

	port->user_name = "foo";
	expect_any(get_role_line, role);
	will_return(get_role_line, NULL);
	result = check_valid_until_for_gssapi(port);
	assert_true(result == STATUS_ERROR);
}

void
test_checkValidUntilForGssapi2(void **state)
{
	int			result = -1;
	List	   *list = list_make1("foo");
	List	  **line = &list;
	Port	   *port = (Port *) malloc(sizeof(Port));

	port->user_name = "foo";
	expect_any(get_role_line, role);
	will_return(get_role_line, line);
	result = check_valid_until_for_gssapi(port);
	assert_true(result == STATUS_OK);
}

void
test_checkValidUntilForGssapi3(void **state)
{
	int			result = -1;
	ListCell   *cell = NULL;
	List	   *list = list_make1(cell);
	List	  **line = &list;
	Port	   *port = (Port *) malloc(sizeof(Port));

	port->user_name = "foo";
	expect_any(get_role_line, role);
	will_return(get_role_line, line);
	result = check_valid_until_for_gssapi(port);
	assert_true(result == STATUS_OK);
}

void
test_checkValidUntilForGssapi4(void **state)
{
	int			result = -1;
	ListCell   *cell = NULL;
	ListCell   *cell1 = NULL;
	List	   *list = list_make2(cell, cell1);
	List	  **line = &list;
	Port	   *port = (Port *) malloc(sizeof(Port));

	port->user_name = "foo";
	expect_any(get_role_line, role);
	will_return(get_role_line, line);
	result = check_valid_until_for_gssapi(port);
	assert_true(result == STATUS_OK);
}

void
test_checkValidUntilForGssapi5(void **state)
{
	int			result = -1;
	ListCell   *cell = NULL;
	List	   *list = list_make3(cell, "foo", "bar");
	List	  **line = &list;
	Port	   *port = (Port *) malloc(sizeof(Port));

	port->user_name = "foo";
	expect_any(get_role_line, role);
	will_return(get_role_line, line);
	expect_any(DirectFunctionCall3, func);
	expect_any(DirectFunctionCall3, arg1);
	expect_any(DirectFunctionCall3, arg2);
	expect_any(DirectFunctionCall3, arg3);
	will_return(DirectFunctionCall3, 10293842);
	will_return(GetCurrentTimestamp, 10293843);
	result = check_valid_until_for_gssapi(port);
	assert_true(result == STATUS_ERROR);
}

void
test_checkValidUntilForGssapi6(void **state)
{
	int			result = -1;
	ListCell   *cell = NULL;
	List	   *list = list_make3(cell, "foo", "bar");
	List	  **line = &list;
	Port	   *port = (Port *) malloc(sizeof(Port));

	port->user_name = "foo";
	expect_any(get_role_line, role);
	will_return(get_role_line, line);
	expect_any(DirectFunctionCall3, func);
	expect_any(DirectFunctionCall3, arg1);
	expect_any(DirectFunctionCall3, arg2);
	expect_any(DirectFunctionCall3, arg3);
	will_return(DirectFunctionCall3, 10293844);
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
		unit_test(test_checkValidUntilForGssapi1),
		unit_test(test_checkValidUntilForGssapi2),
		unit_test(test_checkValidUntilForGssapi3),
		unit_test(test_checkValidUntilForGssapi4),
		unit_test(test_checkValidUntilForGssapi5),
		unit_test(test_checkValidUntilForGssapi6)
#endif
	};

	MemoryContextInit();

	return run_tests(tests);
}
