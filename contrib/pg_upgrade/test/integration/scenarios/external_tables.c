#include <stdarg.h>
#include <setjmp.h>
#include <stdlib.h>
#include <unistd.h>

#include "cmockery.h"

#include "utilities/upgrade-helpers.h"
#include "utilities/test-helpers.h"
#include "utilities/query-helpers.h"
#include "utilities/bdd-helpers.h"
#include "scenarios/external_tables.h"

static void
readableExternalTableHaveBeenUpgraded(void)
{
	PGconn *connection = connectToSix();
	PGresult *result;
	
	result = executeQuery(connection, "SELECT * FROM r_t1");
	assert_int_equal(2, PQntuples(result));
	PQclear(result);
	PQfinish(connection);
}

static void
createReadableExternalTable(void)
{
	PGconn *connection = connectToFive();
	char hostname[100];
	gethostname(hostname, 100);
	char buffer[1000];
	char *src_filename = "./test-data/external_table_data.csv";
	char cwd[200];
	getcwd(cwd, sizeof(cwd));

	sprintf(buffer,  "CREATE READABLE EXTERNAL TABLE r_t1 (a int, b int) LOCATION ('file://%s%s/%s') FORMAT 'TEXT' (DELIMITER '|');", hostname, cwd, src_filename);
	
	executeQuery(connection, buffer);
	PQfinish(connection);
}

static void
anAdministratorPerformsAnUpgrade()
{
	performUpgrade();
}

void
test_a_readable_external_table_can_be_upgraded(void ** state) 
{
	given(withinGpdbFiveCluster(createReadableExternalTable));
	when(anAdministratorPerformsAnUpgrade);
	then(withinGpdbSixCluster(readableExternalTableHaveBeenUpgraded));
}
