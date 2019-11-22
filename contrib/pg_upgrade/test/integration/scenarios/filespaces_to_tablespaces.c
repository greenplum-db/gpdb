#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "cmockery_gp.h"

#include "libpq-fe.h"

#include "utilities/gpdb5-cluster.h"
#include "utilities/gpdb6-cluster.h"
#include "utilities/test-upgrade-helpers.h"
#include "utilities/query-helpers.h"
#include "utilities/test-helpers.h"
#include "utilities/bdd-helpers.h"
#include "utilities/row-assertions.h"

#include "filespaces_to_tablespaces.h"

static void
aFilespaceExistsInTheFiveClusterWithATableAndData(void)
{
	startGpdbFiveCluster();

	PGconn *connection5 = connectToFive();
	PGresult *result5;

	/*
	* Create filespace directories
	*/
	system("rm -rf /tmp/gpdb-filespaces");
	system("mkdir /tmp/gpdb-filespaces");

	/*
	* Create filespace and tablespace within the filespace.
	* Note that supplying identical location dirs for the primary and mirror
	* in a primary-mirror pair requires a multi-node test setup.
	* These locations are ignored in the filespace->tablespace upgrade as the
	* primary locations are used to generate the necessary DDL and tablespace
	* map. Thus, we supply dummy directories here just to make the syntax
	* check happy.
	*/
	result5 = executeQuery(connection5, "CREATE FILESPACE some_filespace ( \n"
	                                               "1: '/tmp/gpdb-filespaces/fsseg-1/', \n"
	                                               "2: '/tmp/gpdb-filespaces/fsseg0/', \n"
	                                               "3: '/tmp/gpdb-filespaces/fsseg1/', \n"
	                                               "4: '/tmp/gpdb-filespaces/fsseg2/', \n"
	                                               "5: '/tmp/gpdb-filespaces/fsdummy1/', \n"
	                                               "6: '/tmp/gpdb-filespaces/fsdummy2/', \n"
	                                               "7: '/tmp/gpdb-filespaces/fsdummy3/', \n"
	                                               "8: '/tmp/gpdb-filespaces/fsdummy4/' );");
	PQclear(result5);

	result5 = executeQuery(connection5, "CREATE TABLESPACE some_tablespace FILESPACE some_filespace;");
	PQclear(executeQuery(connection5, "CREATE SCHEMA five_to_six_upgrade;"));
	PQclear(executeQuery(connection5, "set search_path to five_to_six_upgrade;"));
	result5 = executeQuery(connection5, "CREATE TABLE users (id integer, name text) TABLESPACE some_tablespace;");
	result5 = executeQuery(connection5, "insert into users VALUES (1, 'Joe');");
	result5 = executeQuery(connection5, "insert into users VALUES (2, 'Janet');");
	result5 = executeQuery(connection5, "insert into users VALUES (3, 'James');");
	PQclear(result5);

	PQfinish(connection5);
}

static void
anAdministratorPerformsAnUpgradeWithATablespaceMappingFile(void)
{
	performUpgradeWithTablespaces("./old_tablespaces.txt");
}

typedef struct UserData
{
	int id;
	char *name;
} User;

static Rows *
extract_rows(PGresult *result)
{
	int number_of_rows = PQntuples(result);

	Rows *rows = calloc(1, sizeof(Rows));

	int id_column_index  = PQfnumber(result, "id");
	int name_column_index = PQfnumber(result, "name");

	for (int i = 0; i < number_of_rows; i++)
	{
		User *user = calloc(1, sizeof(User));
		user->id  = atoi(PQgetvalue(result, i, id_column_index));
		user->name = PQgetvalue(result, i, name_column_index);
		rows->rows[i] = user;
	}

	rows->size = number_of_rows;

	return rows;
}

static Rows *
queryForRows(char *queryString)
{
	PGconn *connection = connectToSix();
	PGresult *result = executeQuery(connection, queryString);

	Rows *rows = extract_rows(result);
	PQfinish(connection);

	return rows;
}

static bool
users_match(void *expected, void *actual)
{
	User *first_user = (User *) expected;
	User *second_user = (User *) actual;

	return strcmp(first_user->name, second_user->name) == 0 &&
		first_user->id == second_user->id;
}

static void match_failed_for_user(void *expected_row)
{
	User *expected_user = (User*) expected_row;

	printf("==============> expected {.id=%d, .name=%s} to be in actual rows\n",
	       expected_user->id,
	       expected_user->name);
}

static void
aTablespaceShouldHaveBeenCreatedOnSixCluster(void)
{
	startGpdbSixCluster();

	matcher = users_match;
	match_failed = match_failed_for_user;

	Rows *rows = queryForRows("select * from five_to_six_upgrade.users;");

	User joe = {.id = 1, .name = "Joe"};
	User janet = {.id = 2, .name = "Janet"};
	User james = {.id = 3, .name = "James"};

	assert_rows(rows, (Rows) {
		.size = 3,
		.rows = {&joe, &janet, &james}
	});

	stopGpdbSixCluster();
}

static void
expectTablespaceDirectoryToExist(char *directory_path)
{
	struct stat fileInformation;

	int success = stat(directory_path, &fileInformation) == 0;

	if (!success)
		fail_msg("expected the tablespace directory \"%s\" to exist, but does not.", directory_path);

	assert_true(success);
}

static void
theTablespacesInTheNewClusterShouldBeCreatedInTheSameLocationAsTheOldClustersTablespaces(void)
{
	expectTablespaceDirectoryToExist("/tmp/gpdb-filespaces/fsseg-1/1");
	expectTablespaceDirectoryToExist("/tmp/gpdb-filespaces/fsseg0/2");
	expectTablespaceDirectoryToExist("/tmp/gpdb-filespaces/fsseg1/3");
	expectTablespaceDirectoryToExist("/tmp/gpdb-filespaces/fsseg2/4");
}

void
test_a_filespace_can_be_upgraded_into_new_tablespaces(void **state)
{
	given(withinGpdbFiveCluster(aFilespaceExistsInTheFiveClusterWithATableAndData));
	when(anAdministratorPerformsAnUpgradeWithATablespaceMappingFile);
	then(withinGpdbSixCluster(aTablespaceShouldHaveBeenCreatedOnSixCluster));
	and(theTablespacesInTheNewClusterShouldBeCreatedInTheSameLocationAsTheOldClustersTablespaces);
}
