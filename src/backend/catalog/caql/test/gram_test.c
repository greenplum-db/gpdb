#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "catalog/caqlparse.h"
#include "nodes/value.h"


/*
 * Test for simple SELECT
 */
void
test__caql_raw_parse1(void **state)
{
	const char	   *query = "SELECT * FROM pg_class";
	CaQLSelect	   *ast;

	ast = caql_raw_parser(query, __FILE__, __LINE__);

	assert_true(IsA(ast, CaQLSelect));
	assert_string_equal(strVal(linitial(ast->targetlist)), "*");
	assert_string_equal(ast->from, "pg_class");
	assert_true(ast->where == NIL);
}

/*
 * Test for column name in SELECT
 */
void
test__caql_raw_parse2(void **state)
{
	const char	   *query = "SELECT attrelid FROM pg_attribute";
	CaQLSelect	   *ast;

	ast = caql_raw_parser(query, __FILE__, __LINE__);

	assert_true(IsA(ast, CaQLSelect));
	assert_string_equal(strVal(linitial(ast->targetlist)), "attrelid");
	assert_true(ast->orderby == NIL);
}

/*
 * Test for WHERE clause
 */
void
test__caql_raw_parse3(void **state)
{
	const char	   *query = "SELECT * FROM pg_type where oid = :1";
	CaQLSelect	   *ast;
	CaQLExpr		   *pred;

	ast = caql_raw_parser(query, __FILE__, __LINE__);

	assert_true(IsA(ast, CaQLSelect));

	/* WHERE clause will be a list of CaQLExpr */
	pred = linitial(ast->where);
	assert_int_equal(pred->right, 1);
}

/*
 * Test for count(*)
 */
void
test__caql_raw_parse4(void **state)
{
	const char	   *query = "SELECT count(*) FROM pg_proc";
	CaQLSelect	   *ast;
	char		   *tle;

	ast = caql_raw_parser(query, __FILE__, __LINE__);

	assert_true(IsA(ast, CaQLSelect));
	tle = strVal(linitial(ast->targetlist));
	/* count(*) will be translated into "*" */
	assert_string_equal(tle, "*");
	assert_true(ast->count);
}

/*
 * Test for ANDed predicates
 */
void
test__caql_raw_parse5(void **state)
{
	const char	   *query = "SELECT * FROM pg_attribute "
							"WHERE attrelid = :1 AND attnum > :2";
	CaQLSelect	   *ast;
	CaQLExpr		   *pred;

	ast = caql_raw_parser(query, __FILE__, __LINE__);

	assert_true(IsA(ast, CaQLSelect));
	pred = list_nth(ast->where, 0);
	assert_string_equal(pred->op, "=");
	assert_int_equal(pred->right, 1);
	pred = list_nth(ast->where, 1);
	assert_string_equal(pred->op, ">");
	assert_int_equal(pred->right, 2);
}

/*
 * FOR UPDATE allows ORDER BY
 */
void
test__caql_raw_parse6(void **state)
{
	const char	   *query = "SELECT * FROM pg_attribute "
							"ORDER BY attrelid, attnum FOR UPDATE";
	CaQLSelect	   *ast;
	char		   *orderby;

	ast = caql_raw_parser(query, __FILE__, __LINE__);

	assert_true(IsA(ast, CaQLSelect));
	assert_int_equal(list_length(ast->orderby), 2);

	/* ORDER BY clause will be a list of single string */
	orderby = strVal(list_nth(ast->orderby, 0));
	assert_string_equal(orderby, "attrelid");
	orderby = strVal(list_nth(ast->orderby, 1));
	assert_string_equal(orderby, "attnum");
	assert_true(ast->forupdate);
}

static void
negative_test_common(const char *query)
{
	Node		   *ast;
	bool			result = false;

	PG_TRY();
	{
		ast = caql_raw_parser(query, __FILE__, __LINE__);
	}
	PG_CATCH();
	{
		result = true;
	}
	PG_END_TRY();

	assert_true(result);
}

/*
 * Negative test with relation alias
 */
void
test__caql_raw_parse_fail1(void **state)
{
	negative_test_common("SELECT t1.relname FROM pg_class t1");
}

/*
 * Negative test with JOIN
 */
void
test__caql_raw_parse_fail2(void **state)
{
	negative_test_common("SELECT * FROM pg_class JOIN pg_attribute "
						 "ON pg_class.oid = pg_attribute.attrelid");
}

/*
 * Negative test with invalid parameter
 * Currently CaQL does not accept literal in predicate.
 */
void
test__caql_raw_parse_fail3(void **state)
{
	negative_test_common("SELECT * FROM pg_attribute "
						 "WHERE attrelid = :1 AND attnum > 0");
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
			unit_test(test__caql_raw_parse1),
			unit_test(test__caql_raw_parse2),
			unit_test(test__caql_raw_parse3),
			unit_test(test__caql_raw_parse4),
			unit_test(test__caql_raw_parse5),
			unit_test(test__caql_raw_parse6),
			unit_test(test__caql_raw_parse_fail1),
			unit_test(test__caql_raw_parse_fail2),
			unit_test(test__caql_raw_parse_fail3),
	};

	MemoryContextInit();

	return run_tests(tests);
}
