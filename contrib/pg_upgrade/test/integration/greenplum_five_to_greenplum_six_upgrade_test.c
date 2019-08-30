#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"
#include "libpq-fe.h"
#include "stdbool.h"
#include "stdlib.h"

#include "utilities/gpdb5-cluster.h"
#include "utilities/gpdb6-cluster.h"

static int set_value = 0;

static void
setup(void **state)
{
	printf("\nMaking a copy of gpdb5 data directories.\n");
	system("rsync -a --delete ./gpdb5-data/ ./gpdb5-data-copy");
	
	printf("\nMaking a copy of gpdb6 data directories.\n");
	system("rsync -a --delete ./gpdb6-data/ ./gpdb6-data-copy");
}

static void
teardown(void **state)
{
}

static PGconn *
connectToFive()
{
	PGconn
		*connection = PQconnectdb("dbname=postgres user=adamberlin port=50000");

	if (PQstatus(connection) != CONNECTION_OK)
		printf("error: failed to connect to greenplum 5 database");

	return connection;
}

static PGconn *
connectToSix()
{
	PGconn
		*connection = PQconnectdb("dbname=postgres user=adamberlin port=60000");

	if (PQstatus(connection) != CONNECTION_OK)
		printf("error: failed to connect to greenplum 6 database\n");

	return connection;
}

PGresult *
executeQuery(PGconn *connection, char *const query)
{
	return PQexec(connection, query);
}

static void
upgradeMaster()
{
	system(""
			"./gpdb6/bin/pg_upgrade "
			"--mode=dispatcher "
			"--old-bindir=./gpdb5/bin "
			"--new-bindir=./gpdb6/bin "
			"--old-datadir=./gpdb5-data-copy/qddir/demoDataDir-1 "
			"--new-datadir=./gpdb6-data-copy/qddir/demoDataDir-1 "
	);
}

static void
upgradeContentId0()
{
	system("rsync -a --delete ./gpdb6-data-copy/qddir/demoDataDir-1/ ./gpdb6-data-copy/dbfast1/demoDataDir0");

	system(""
		"./gpdb6/bin/pg_upgrade "
			"--mode=segment "
			"--old-bindir=./gpdb5/bin "
			"--new-bindir=./gpdb6/bin "
			"--old-datadir=./gpdb5-data-copy/dbfast1/demoDataDir0 "
			"--new-datadir=./gpdb6-data-copy/dbfast1/demoDataDir0 "
	);
}

static void
upgradeContentId1()
{
	system("rsync -a --delete ./gpdb6-data-copy/qddir/demoDataDir-1/ ./gpdb6-data-copy/dbfast2/demoDataDir1");

	system(""
		"./gpdb6/bin/pg_upgrade "
			"--mode=segment "
			"--old-bindir=./gpdb5/bin "
			"--new-bindir=./gpdb6/bin "
			"--old-datadir=./gpdb5-data-copy/dbfast2/demoDataDir1 "
			"--new-datadir=./gpdb6-data-copy/dbfast2/demoDataDir1 "
	);
}

static void
upgradeContentId2()
{
	system("rsync -a --delete ./gpdb6-data-copy/qddir/demoDataDir-1/ ./gpdb6-data-copy/dbfast3/demoDataDir2");

	system(""
		"./gpdb6/bin/pg_upgrade "
			"--mode=segment "
			"--old-bindir=./gpdb5/bin "
			"--new-bindir=./gpdb6/bin "
			"--old-datadir=./gpdb5-data-copy/dbfast3/demoDataDir2 "
			"--new-datadir=./gpdb6-data-copy/dbfast3/demoDataDir2 "
	);
}

typedef struct UserData
{
	int  id;
	char *name;
}          User;

static bool
users_match(User *expected_user, User *actual_user)
{
	return expected_user->id == actual_user->id &&
		expected_user->name == actual_user->name;
}

static void
assert_rows_contain_user(User expected_user, User *rows[])
{
	User *current_user;
	bool found = false;

	for (current_user = *rows; current_user != NULL; rows++)
	{
		if (users_match(&expected_user, current_user))
		{
			found = true;
			break;
		}
	}

	assert_true(found);
}

void
assert_number_of_rows(User *rows[], int expected_number)
{
	int i;

	for (i = 0; rows[i] != NULL; i++);

	assert_int_equal(expected_number, i);
}

static void
initialize_user_rows(User *rows[], int size)
{
	for (int i       = 0; i < size; i++)
		rows[i] = NULL;
}

static void
extract_user_rows(PGresult *result, User *rows[])
{
	for (int i = 0; i < PQntuples(result); i++)
	{
		User *user = malloc(sizeof(User));
		user->id   = atoi(PQgetvalue(result, i, PQfnumber(result, "id")));
		user->name = PQgetvalue(result, i, PQfnumber(result, "name"));
		rows[i] = user;
	}
}

static void
test_upgrade(void **state)
{
	/*
	 * Given a heap table in a greenplum 5 database
	 * with rows of data
	 */
	startGpdbFiveCluster();
	PGconn *connection = connectToFive();
	executeQuery(connection, "alter role adamberlin NOCREATEEXTTABLE(protocol='gphdfs',type='readable');");
	executeQuery(connection, "alter role adamberlin NOCREATEEXTTABLE(protocol='gphdfs',type='writable');");
	executeQuery(connection, "create schema five_to_six_upgrade;");
	executeQuery(connection, "set search_path to five_to_six_upgrade");
	executeQuery(connection,
	             "create table users (id integer, name text) distributed by (id);");
	executeQuery(connection, "insert into users (1, 'Jane')");
	executeQuery(connection, "insert into users (2, 'John')");
	executeQuery(connection, "insert into users (3, 'Joe')");
	PQfinish(connection);
	stopGpdbFiveCluster();

	/*
	 * When I upgrade the database
	 */
	upgradeMaster();
	upgradeContentId0();
	upgradeContentId1();
	upgradeContentId2();

	/* 
	 * Then that heap table should exist in the greenplum 6 cluster
	 * with all of its data
	 */
	startGpdbSixCluster();
	connection = connectToSix();
	executeQuery(connection, "set search_path to five_to_six_upgrade;");
	PGresult *result = executeQuery(connection, "select * from users;");
	PQfinish(connection);
	stopGpdbSixCluster();

	const int size = 10;
	User     *rows[size];
	initialize_user_rows(rows, size);
	extract_user_rows(result, rows);

	assert_number_of_rows(rows, 3);
	assert_rows_contain_user((User) {.id=1, .name="Jane"}, rows);
	assert_rows_contain_user((User) {.id=2, .name="John"}, rows);
	assert_rows_contain_user((User) {.id=3, .name="Joe"}, rows);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test_setup_teardown(test_upgrade, setup, teardown)
	};

	return run_tests(tests);
}