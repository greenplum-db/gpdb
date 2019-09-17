#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"
#include "libpq-fe.h"
#include "stdbool.h"
#include "stdlib.h"
#include "sys/stat.h"

#include "utilities/gpdb5-cluster.h"
#include "utilities/gpdb6-cluster.h"
#include "utilities/upgrade-helpers.h"
#include "utilities/query-helpers.h"

static void
setup(void **state)
{
	printf("\nMaking a copy of gpdb5 data directories.\n");
	system("rsync -a --delete ./gpdb5-data-copy/ ./gpdb5-data");

	printf("\nMaking a copy of gpdb6 data directories.\n");
	system("rsync -a --delete ./gpdb6-data-copy/ ./gpdb6-data");
}

static void
teardown(void **state)
{
}


PGconn *
connectToFive()
{
	return connectTo(50000);
}

PGconn *
connectToSix()
{
	return connectTo(60000);
}


typedef struct UserData
{
	int  id;
	char *name;
}          User;

static bool
users_match(User *expected_user, User *actual_user)
{
	return (
		expected_user->id == actual_user->id &&
		strncmp(expected_user->name, actual_user->name, strlen(expected_user->name)) == 0
	);
}

static void
assert_rows_contain_user(User expected_user, User *rows[], int max)
{
	int i;
	User *current_user;
	bool found = false;

	for (i = 0; rows[i] != NULL && i < max; i++) {
		current_user = rows[i];

		if (users_match(&expected_user, current_user))
			found = true;
	}

	assert_true(found);
}

static void
assert_number_of_rows(User *rows[], int expected_number, int max)
{
	int i;

	for (i = 0; rows[i] != NULL && i < max; i++);

	assert_int_equal(expected_number, i);
}

static void
initialize_user_rows(User *rows[], int size)
{
	for (int i = 0; i < size; i++) {
		rows[i] = NULL;
	}
}

static void
extract_user_rows(PGresult *result, User *rows[])
{
	int number_of_rows = PQntuples(result);
	int i;

	for (i = 0; i < number_of_rows; i++)
	{
		User *user = malloc(sizeof(User));
		user->id   = atoi(PQgetvalue(result, i, PQfnumber(result, "id")));
		user->name = PQgetvalue(result, i, PQfnumber(result, "name"));
		rows[i] = user;
	}
}

static void 
createHeapTableWithDataInFiveCluster(void)
{
	PGconn *connection = connectToFive();
	executeQuery(connection, "create schema five_to_six_upgrade;");
	executeQuery(connection, "set search_path to five_to_six_upgrade");
	executeQuery(connection, "create table users (id integer, name text) distributed by (id);");
	executeQuery(connection, "insert into users values (1, 'Jane')");
	executeQuery(connection, "insert into users values (2, 'John')");
	executeQuery(connection, "insert into users values (3, 'Joe')");
	/* FIXME: why do we need this ?? */
	executeQuery(connection, "VACUUM FREEZE;");
	PQfinish(connection);
}

static void 
heapTableShouldHaveDataUpgradedToSixCluster()
{
	PGconn *connection = connectToSix();
	executeQuery(connection, "set search_path to five_to_six_upgrade;");
	PGresult *result = executeQuery(connection, "select * from users;");

	const int size = 10;
	User     *rows[size];

	initialize_user_rows(&rows, size);
	extract_user_rows(result, &rows);

	assert_number_of_rows(&rows, 3, size);
	assert_rows_contain_user((User) {.id=1, .name="Jane"}, &rows, size);
	assert_rows_contain_user((User) {.id=2, .name="John"}, &rows, size);
	assert_rows_contain_user((User) {.id=3, .name="Joe"}, &rows, size);

	PQfinish(connection);
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
	char path[2000];

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
	char *segment_path = "qddir/demoDataDir-1";

	assertRelfilenodeHardLinked(
		segment_path, 
		databaseOid, 
		relfilenodeNumber);
}

static void
assertContentId0HasTableLinked(int databaseOid, int relfilenodeNumber)
{
	char *segment_path = "dbfast1/demoDataDir0";

	assertRelfilenodeHardLinked(
		segment_path, 
		databaseOid, 
		relfilenodeNumber);
}

static void
assertContentId1HasTableLinked(int databaseOid, int relfilenodeNumber)
{
	char *segment_path = "dbfast2/demoDataDir1";

	assertRelfilenodeHardLinked(
		segment_path,
		databaseOid,
		relfilenodeNumber);
}

static void
assertContentId2HasTableLinked(int databaseOid, int relfilenodeNumber)
{
	char *segment_path = "dbfast3/demoDataDir2";

	assertRelfilenodeHardLinked(
		segment_path,
		databaseOid,
		relfilenodeNumber);
}



static void 
heapTableShouldBeHardLinked(void)
{
	int rowNumber;
	int databaseOid;
	int relfilenodeNumber;

	PGconn *connection = connectToSix();
	executeQuery(connection, "set search_path to five_to_six_upgrade;");
	PGresult *result = executeQuery(connection, "select pg_database.oid, relfilenode from pg_class, pg_database where relname = 'users' and datname = current_database();");
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
given(void (* arrangeFunction)(void))
{
	startGpdbFiveCluster();
	arrangeFunction();
	stopGpdbFiveCluster();
}

static void
then(void (* assertionFunction)(void))
{
	startGpdbSixCluster();
	assertionFunction();
	stopGpdbSixCluster();
}

static void
when(void (* actFunction)(void))
{
	actFunction();
}

static void 
and(void (* assertionFunction)(void))
{
	/* and has the same behavior as then */
	then(assertionFunction);
}

static void
test_a_heap_table_with_data_can_be_upgraded(void **state)
{
	given(createHeapTableWithDataInFiveCluster);
	when(anAdministratorPerformsAnUpgrade);
	then(heapTableShouldHaveDataUpgradedToSixCluster);
	and(heapTableShouldBeHardLinked);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test_setup_teardown(test_a_heap_table_with_data_can_be_upgraded, setup, teardown)
	};

	return run_tests(tests);
}