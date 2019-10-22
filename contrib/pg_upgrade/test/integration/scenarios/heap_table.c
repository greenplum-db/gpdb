#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <sys/stat.h>

#include "cmockery.h"
#include "libpq-fe.h"

#include "utilities/gpdb5-cluster.h"
#include "utilities/gpdb6-cluster.h"
#include "utilities/upgrade-helpers.h"
#include "utilities/query-helpers.h"
#include "utilities/test-helpers.h"
#include "utilities/bdd-helpers.h"
#include "heap_table.h"

#include "utilities/row-assertions.h"

typedef struct UserData
{
	int			id;
	char	   *name;
} User;

static bool
users_match(void * expected, void * actual)
{
	User *expected_user = (User *) expected; 
	User *actual_user = (User *) actual;

	return expected_user->id == actual_user->id && 
		strncmp(expected_user->name, actual_user->name, strlen(expected_user->name)) == 0;
}

static void
user_match_failed(void *expected) {
	User *expected_user = (User *) expected;

	printf("==========> expected {.id=%d, .name=%s}\n",
		expected_user->id,
		expected_user->name);
}

static Rows *
extract_user_rows(PGresult *result)
{
	int number_of_rows = PQntuples(result);

	Rows *rows = calloc(1, sizeof(Rows)); 

	const int	i_id = PQfnumber(result, "id");
	const int	i_name = PQfnumber(result, "name");

	for (int i = 0; i < number_of_rows; i++)
	{
		User *user = calloc(1, sizeof(User));

		user->id = atoi(PQgetvalue(result, i, i_id));
		user->name = strdup(PQgetvalue(result, i, i_name));

		rows->rows[i] = user;
	}

	rows->size = number_of_rows;

	return rows;
}

static void
assertNumberOfHardLinks(struct stat *fileInformation, int expectedNumberOfHardLinks)
{
	assert_int_equal(
		fileInformation->st_nlink,
		expectedNumberOfHardLinks);
}

static void
assertRelfilenodeHardLinked(
	char *segment_path,
	int databaseOid,
	int relfilenodeNumber
)
{
	char		path[2000];

	sprintf(
		path,
		"./gpdb6-data/%s/base/%d/%d",
		segment_path,
		databaseOid,
		relfilenodeNumber);

	struct stat fileInformation;

	stat(path, &fileInformation);

	/*
	 * The file should have two hard links to
	 */
	assertNumberOfHardLinks(
		&fileInformation,
		2);
}

static void
assertMasterHasTableLinked(int databaseOid, int relfilenodeNumber)
{
	char	   *segment_path = "qddir/demoDataDir-1";

	assertRelfilenodeHardLinked(
		segment_path,
		databaseOid,
		relfilenodeNumber);
}

static void
assertContentId0HasTableLinked(int databaseOid, int relfilenodeNumber)
{
	char	   *segment_path = "dbfast1/demoDataDir0";

	assertRelfilenodeHardLinked(
		segment_path,
		databaseOid,
		relfilenodeNumber);
}

static void
assertContentId1HasTableLinked(int databaseOid, int relfilenodeNumber)
{
	char	   *segment_path = "dbfast2/demoDataDir1";

	assertRelfilenodeHardLinked(
		segment_path,
		databaseOid,
		relfilenodeNumber);
}

static void
assertContentId2HasTableLinked(int databaseOid, int relfilenodeNumber)
{
	char	   *segment_path = "dbfast3/demoDataDir2";

	assertRelfilenodeHardLinked(
		segment_path,
		databaseOid,
		relfilenodeNumber);
}

static void
heapTableShouldHaveDataUpgradedToSixCluster()
{
	PGconn	   *connection = connectToSix();

	executeQuery(connection, "set search_path to five_to_six_upgrade;");
	PGresult   *result = executeQuery(connection, "select * from users;");

	Rows *rows = extract_user_rows(result);

	assert_int_equal(3, rows->size);

	User jane = {.id = 1,.name = "Jane"};
	User john = {.id = 2,.name = "John"};
	User joe = {.id = 3,.name = "Joe"};
	
	const Rows expected_users = {
		.size = 3,
		.rows = {&jane, &john, &joe}
	};

	assert_rows(rows, expected_users);

	PQfinish(connection);
}

static void
heapTableShouldBeHardLinked(void)
{
	int			rowNumber;
	int			databaseOid;
	int			relfilenodeNumber;

	PGconn	   *connection = connectToSix();

	executeQuery(connection, "set search_path to five_to_six_upgrade;");
	PGresult   *result = executeQuery(connection, "select pg_database.oid, relfilenode from pg_class, pg_database where relname = 'users' and datname = current_database();");

	rowNumber = 0;
	databaseOid = atoi(PQgetvalue(result, rowNumber, 0));
	relfilenodeNumber = atoi(PQgetvalue(result, rowNumber, 1));
	PQfinish(connection);

	assertMasterHasTableLinked(databaseOid, relfilenodeNumber);
	assertContentId0HasTableLinked(databaseOid, relfilenodeNumber);
	assertContentId1HasTableLinked(databaseOid, relfilenodeNumber);
	assertContentId2HasTableLinked(databaseOid, relfilenodeNumber);
}

static void
anAdministratorPerformsAnUpgrade()
{
	performUpgrade();
}

static void
createHeapTableWithDataInFiveCluster(void)
{
	PGconn	   *connection = connectToFive();

	executeQuery(connection, "create schema five_to_six_upgrade;");
	executeQuery(connection, "set search_path to five_to_six_upgrade");
	executeQuery(connection, "create table users (id integer, name text) distributed by (id);");
	executeQuery(connection, "insert into users values (1, 'Jane')");
	executeQuery(connection, "insert into users values (2, 'John')");
	executeQuery(connection, "insert into users values (3, 'Joe')");
	PQfinish(connection);
}

void
test_a_heap_table_with_data_can_be_upgraded(void **state)
{
	matcher = users_match;
	match_failed = user_match_failed;

	given(withinGpdbFiveCluster(createHeapTableWithDataInFiveCluster));
	when(anAdministratorPerformsAnUpgrade);
	then(withinGpdbSixCluster(heapTableShouldHaveDataUpgradedToSixCluster));
	and(withinGpdbSixCluster(heapTableShouldBeHardLinked));
}
