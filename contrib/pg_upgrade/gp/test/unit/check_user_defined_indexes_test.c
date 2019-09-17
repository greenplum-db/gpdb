/*
 *
 *  check_user_defined_indexes_test.c
 *
 *	Copyright (c) 2019-Present Pivotal Software, Inc
 *
 */


#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"
#include "stdlib.h"

#include "../../checks.h"
#include "libpq-int.h"
#include "../../../pg_upgrade.h"

static int stubbed_number_of_rows = 0;


/* implements */
#include "gp/query_for_user_defined_indexes.h"

static int size_of_test_message = 1000;
static char failure_messages[3000];
static UserDefinedIndexes *_stubbedIndexes = NULL;
static bool cleanup_was_called = false;

UserDefinedIndexes *
query_for_user_defined_indexes(ClusterInfo *cluster)
{
	return _stubbedIndexes;
}

static void
setup_rows_to_return(UserDefinedIndexes *stubbedIndexes)
{
	_stubbedIndexes = stubbedIndexes;
}

static void
setup_no_rows_to_return(void)
{
	UserDefinedIndexes *indexes = malloc(sizeof(UserDefinedIndexes));
	indexes->number_of_user_defined_indexes = 0;
	_stubbedIndexes = indexes;
};

static ClusterInfo *
make_cluster()
{
	ClusterInfo *cluster;
	cluster = malloc(sizeof(ClusterInfo));
	return cluster;
}

void report_info(const char *restrict fmt,...)
{
	char *message = (char *) calloc(size_of_test_message, sizeof(char));

	va_list args;
	va_start(args, fmt);
	vsnprintf(message, sizeof(message) * size_of_test_message, fmt, args);
	va_end(args);

	strcat(failure_messages, message);
}

void
teardown(ClusterInfo *clusterInfo)
{
	free(clusterInfo);
}

void setup() 
{
	strcpy(failure_messages, "");
	setup_no_rows_to_return();
	cleanup_was_called = false;
}

void cleanup_query_for_user_defined_indexes(UserDefinedIndexes *indexes)
{
	cleanup_was_called = true;
}

static UserDefinedIndex *
buildUserDefinedIndex(char *database_name, char *namespace_name, char *index_name)
{
	UserDefinedIndex *userDefinedIndex = malloc(sizeof(UserDefinedIndex));
	userDefinedIndex->database_name = database_name;
	userDefinedIndex->namespace_name = namespace_name;
	userDefinedIndex->index_name = index_name;
	return userDefinedIndex;
}

static UserDefinedIndex **
buildUserDefinedIndexArray(int size)
{
	return calloc(size, sizeof(UserDefinedIndex*));
}

static UserDefinedIndexes *
buildUserDefinedIndexes(UserDefinedIndex **indexes, int size)
{
	UserDefinedIndexes *userDefinedIndexes = malloc(sizeof(UserDefinedIndexes));
	userDefinedIndexes->number_of_user_defined_indexes = 1;
	userDefinedIndexes->foundIndexes = indexes;
	return userDefinedIndexes;
}


/*
 * Tests
 */
static void
test_it_does_not_pass_the_check_and_returns_false_when_there_are_indexes_found(void **state)
{
	setup();

	ClusterInfo *cluster = make_cluster();

	UserDefinedIndex *userDefinedIndex = buildUserDefinedIndex("", "", "");
	UserDefinedIndex **foundIndexes = buildUserDefinedIndexArray(1);
	foundIndexes[0] = userDefinedIndex;
	UserDefinedIndexes *indexes = buildUserDefinedIndexes(foundIndexes, 1);

	setup_rows_to_return(indexes);

	bool result = check_user_defined_indexes(cluster);

	assert_false(result);

	teardown(cluster);
}

static void
test_it_returns_true(void **state)
{
	setup();

	ClusterInfo *cluster = make_cluster();

	setup_no_rows_to_return();

	bool result = check_user_defined_indexes(cluster);

	assert_true(result);

	teardown(cluster);
}

static void 
test_it_notifies_which_user_defined_indexes_were_found_when_failing_the_check(void **state)
{
	setup();

	ClusterInfo *cluster = make_cluster();

	UserDefinedIndex *first_index = buildUserDefinedIndex("some_database", "some_namespace", "some_index");
	UserDefinedIndex *second_index = buildUserDefinedIndex("some_other_database", "some_other_namespace", "some_other_index");

	UserDefinedIndex **foundIndexes = buildUserDefinedIndexArray(2);
	foundIndexes[0] = first_index;
	foundIndexes[1] = second_index;

	UserDefinedIndexes *stubbedUserDefinedIndexes = malloc(sizeof(UserDefinedIndexes));
	stubbedUserDefinedIndexes->number_of_user_defined_indexes = 2;
	stubbedUserDefinedIndexes->foundIndexes = foundIndexes;

	setup_rows_to_return(stubbedUserDefinedIndexes);

	bool result = check_user_defined_indexes(cluster);

	assert_string_equal(failure_messages, 
		"total number of found user defined indexes: 2\nsome_database.some_namespace.some_index\nsome_other_database.some_other_namespace.some_other_index\n");

	teardown(cluster);
}

static void
test_it_cleans_up_the_query_when_finished(void **state)
{
	setup();

	ClusterInfo *cluster = make_cluster();

	bool result = check_user_defined_indexes(cluster);

	assert_true(cleanup_was_called);

	teardown(cluster);
}

/*
 * Runner
 */
int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_it_returns_true),
		unit_test(test_it_does_not_pass_the_check_and_returns_false_when_there_are_indexes_found),
		unit_test(test_it_notifies_which_user_defined_indexes_were_found_when_failing_the_check),
		unit_test(test_it_cleans_up_the_query_when_finished)
	};

	run_tests(tests);
}
