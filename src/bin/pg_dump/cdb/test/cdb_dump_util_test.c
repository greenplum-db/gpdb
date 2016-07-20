#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "c.h"
#include "../cdb_dump_util.c"

void 
test__isFilteringAllowed1(void **state)
{
	int role = ROLE_MASTER;
	bool incremental = true;
	char *filter = NULL;
	bool result = isFilteringAllowedNow(role, incremental, filter);
	assert_false(result);
}

void 
test__isFilteringAllowed2(void **state)
{
	int role = ROLE_MASTER;
	bool incremental = false;
	char *filter = NULL;
	bool result = isFilteringAllowedNow(role, incremental, filter);
	assert_true(result);
}

void 
test__isFilteringAllowed3(void **state)
{
	int role = ROLE_SEGDB;
	bool incremental = true;
	char *filter = NULL;
	bool result = isFilteringAllowedNow(role, incremental, filter);
	assert_true(result);
}

void 
test__isFilteringAllowed4(void **state)
{
	int role = ROLE_SEGDB;
	bool incremental = false;
	char *filter = NULL;
	bool result = isFilteringAllowedNow(role, incremental, filter);
	assert_true(result);
}

void 
test__isFilteringAllowed5(void **state)
{
	int role = ROLE_MASTER;
	bool incremental = true;
	char *filter = "Filename";
	bool result = isFilteringAllowedNow(role, incremental, filter);
	assert_true(result);
}

void 
test__isFilteringAllowed6(void **state)
{
	int role = ROLE_MASTER;
	bool incremental = false;
	char *filter = "Filename";
	bool result = isFilteringAllowedNow(role, incremental, filter);
	assert_true(result);
}

void 
test__isFilteringAllowed7(void **state)
{
	int role = ROLE_SEGDB;
	bool incremental = true;
	char *filter = "Filename";
	bool result = isFilteringAllowedNow(role, incremental, filter);
	assert_true(result);
}

void 
test__isFilteringAllowed8(void **state)
{
	int role = ROLE_SEGDB;
	bool incremental = false;
	char *filter = "Filename";
	bool result = isFilteringAllowedNow(role, incremental, filter);
	assert_true(result);
}

void 
test__backupTypeString5(void **state)
{
	bool incremental = false;
	const char* result = getBackupTypeString(incremental);
	assert_true(strcmp(result, "Backup Type: Full") == 0);
}

void 
test__backupTypeString6(void **state)
{
	bool incremental = true;
	const char* result = getBackupTypeString(incremental);
	assert_true(strcmp(result, "Backup Type: Incremental") == 0);
}

void
test__validateTimeStamp7(void **state)
{
	char* ts = "20100729093000";
	bool result = ValidateTimestampKey(ts);
	assert_true(result);
}

void
test__validateTimeStamp8(void **state)
{
	// too few chars
	char* ts = "2010072909300";
	bool result = ValidateTimestampKey(ts);
	assert_false(result);
}

void
test__validateTimeStamp9(void **state)
{
	// too many chars
	char* ts = "201007290930000";
	bool result = ValidateTimestampKey(ts);
	assert_false(result);
}

void
test__validateTimeStamp10(void **state)
{
	// as long as its a digit its valid
	char* ts = "00000000000000";
	bool result = ValidateTimestampKey(ts);
	assert_true(result);
}

void
test__validateTimeStamp11(void **state)
{
	// as long as its a digit its valid
	char* ts = "0a000000000000";
	bool result = ValidateTimestampKey(ts);
	assert_false(result);
}

void
test__validateTimeStamp12(void **state)
{
	// as long as its a digit its valid
	char* ts = "0q000000000000";
	bool result = ValidateTimestampKey(ts);
	assert_false(result);
}

void
test__validateTimeStamp13(void **state)
{
	// leading space
	char* ts = " 00000000000000";
	bool result = ValidateTimestampKey(ts);
	assert_false(result);
}

void
test__validateTimeStamp14(void **state)
{
	// trailing space
	char* ts = "00000000000000 ";
	bool result = ValidateTimestampKey(ts);
	assert_false(result);
}

void
test__validateTimeStamp15(void **state)
{
	// NULL pointer
	char* ts = NULL;
	bool result = ValidateTimestampKey(ts);
	assert_false(result);
}

void
test__validateTimeStamp16(void **state)
{
	// non-terminated string
	char ts[20];
	int i = 0;
	for (i = 0; i < sizeof(ts); ++i){
		ts[i] = '1';
	}
	bool result = ValidateTimestampKey((char*)ts);
	assert_false(result);
}

void
test__getTimeStampKey17(void **state)
{
	char* ts = "20100729093000";
	assert_true(!strcmp(ts, GetTimestampKey(ts)));
}

void
test__getTimeStampKey18(void **state)
{
	char* ts = NULL;
	assert_true(GetTimestampKey(ts) != NULL);
}

void 
test__formCompressionProgramString1(void **state)
{
	char* tmp = calloc(10, 1);
	strcat(tmp, "/bin/gzip");
	tmp = formCompressionProgramString(tmp);
	assert_string_equal(tmp, "/bin/gzip -c ");
	free(tmp);
}

void 
test__formCompressionProgramString2(void **state)
{
	char* tmp = calloc(100, 1);
	strcat(tmp, "/bin/gzip");
	tmp = formCompressionProgramString(tmp);
	assert_string_equal(tmp, "/bin/gzip -c ");
	free(tmp);
}

void test__formSegmentPsqlCommandLine1(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = true;
	const char* compProg = "gzip -c";
	const char* filter_script = "filter.py"; 
	const char* table_filter_file = "filter.conf";
	int role = ROLE_SEGDB;
	const char* psqlPg = "psql"; 
	const char* catPg = "cat";
	const char* gpNBURestorePg = "gp_bsa_restore_agent";
	const char* netbackupServiceHost = NULL;
	const char* netbackupBlockSize = NULL;

	formSegmentPsqlCommandLine(&cmdLine, inputFileSpec, compUsed, compProg, 
							   filter_script, table_filter_file, role,
							   psqlPg, catPg, gpNBURestorePg,
							   netbackupServiceHost, netbackupBlockSize, NULL, NULL);

	char *expected_output = "cat fileSpec | gzip -c | filter.py -t filter.conf | psql";
	assert_string_equal(cmdLine, expected_output);
	free(cmdLine);
}

void test__formSegmentPsqlCommandLine2(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = true;
	const char* compProg = "gzip -c";
	int role = ROLE_SEGDB;
	const char* psqlPg = "psql"; 
	const char* catPg = "cat";
	const char* gpNBURestorePg = "gp_bsa_restore_agent";
	const char* netbackupServiceHost = NULL;
	const char* netbackupBlockSize = NULL;

	formSegmentPsqlCommandLine(&cmdLine, inputFileSpec, compUsed, compProg, 
							   NULL, NULL, role,
							   psqlPg, catPg, gpNBURestorePg,
							   netbackupServiceHost, netbackupBlockSize, NULL, NULL);

	char *expected_output = "cat fileSpec | gzip -c | psql";
	assert_string_equal(cmdLine, expected_output);
	free(cmdLine);
}

void test__formSegmentPsqlCommandLine3(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = false;
	const char* compProg = "gzip -c ";
	const char* filter_script = "filter.py"; 
	const char* table_filter_file = "filter.conf";
	int role = ROLE_SEGDB;
	const char* psqlPg = "psql"; 
	const char* catPg = "cat";
	const char* gpNBURestorePg = "gp_bsa_restore_agent";
	const char* netbackupServiceHost = NULL;
	const char* netbackupBlockSize = NULL;

	formSegmentPsqlCommandLine(&cmdLine, inputFileSpec, compUsed, compProg, 
							   filter_script, table_filter_file, role,
							   psqlPg, catPg, gpNBURestorePg,
							   netbackupServiceHost, netbackupBlockSize, NULL, NULL);

	char *expected_output = "cat fileSpec | filter.py -t filter.conf | psql";
	assert_string_equal(cmdLine, expected_output);
	free(cmdLine);
}

void test__formSegmentPsqlCommandLine4(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = false;
	const char* compProg = "gzip -c ";
	int role = ROLE_SEGDB;
	const char* psqlPg = "psql"; 
	const char* catPg = "cat";
	const char* gpNBURestorePg = "gp_bsa_restore_agent";
	const char* netbackupServiceHost = NULL;
	const char* netbackupBlockSize = NULL;

	formSegmentPsqlCommandLine(&cmdLine, inputFileSpec, compUsed, compProg, 
							   NULL, NULL, role,
							   psqlPg, catPg, gpNBURestorePg,
							   netbackupServiceHost, netbackupBlockSize, NULL, NULL);

	char *expected_output = "cat fileSpec | psql";
	assert_string_equal(cmdLine, expected_output);
	free(cmdLine);
}

void test__formSegmentPsqlCommandLine5(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = true;
	const char* compProg = "gzip -c";
	const char* filter_script = "filter.py"; 
	const char* table_filter_file = "filter.conf";
	int role = ROLE_MASTER;
	const char* psqlPg = "psql"; 
	const char* catPg = "cat";
	const char* gpNBURestorePg = "gp_bsa_restore_agent";
	const char* netbackupServiceHost = NULL;
	const char* netbackupBlockSize = NULL;

	formSegmentPsqlCommandLine(&cmdLine, inputFileSpec, compUsed, compProg, 
							   filter_script, table_filter_file, role,
							   psqlPg, catPg, gpNBURestorePg,
							   netbackupServiceHost, netbackupBlockSize, NULL, NULL);

	char *expected_output = "cat fileSpec | gzip -c | filter.py -m -t filter.conf | psql";
	assert_string_equal(cmdLine, expected_output);
	free(cmdLine);
}

void test__formSegmentPsqlCommandLine6(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = true;
	const char* compProg = "gzip -c";
	int role = ROLE_MASTER;
	const char* psqlPg = "psql"; 
	const char* catPg = "cat";
	const char* gpNBURestorePg = "gp_bsa_restore_agent";
	const char* netbackupServiceHost = NULL;
	const char* netbackupBlockSize = NULL;

	formSegmentPsqlCommandLine(&cmdLine, inputFileSpec, compUsed, compProg, 
							   NULL, NULL, role,
							   psqlPg, catPg, gpNBURestorePg,
							   netbackupServiceHost, netbackupBlockSize, NULL, NULL);

	char *expected_output = "cat fileSpec | gzip -c | psql";
	assert_string_equal(cmdLine, expected_output);
	free(cmdLine);
}

void test__formSegmentPsqlCommandLine7(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = false;
	const char* compProg = "gzip -c ";
	const char* filter_script = "filter.py"; 
	const char* table_filter_file = "filter.conf";
	int role = ROLE_MASTER;
	const char* psqlPg = "psql"; 
	const char* catPg = "cat";
	const char* gpNBURestorePg = "gp_bsa_restore_agent";
	const char* netbackupServiceHost = NULL;
	const char* netbackupBlockSize = NULL;

	formSegmentPsqlCommandLine(&cmdLine, inputFileSpec, compUsed, compProg, 
							   filter_script, table_filter_file, role,
							   psqlPg, catPg, gpNBURestorePg,
							   netbackupServiceHost, netbackupBlockSize, NULL, NULL);

	char *expected_output = "cat fileSpec | filter.py -m -t filter.conf | psql";
	assert_string_equal(cmdLine, expected_output);
	free(cmdLine);
}

void test__formSegmentPsqlCommandLine8(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = false;
	const char* compProg = "gzip -c ";
	int role = ROLE_MASTER;
	const char* psqlPg = "psql"; 
	const char* catPg = "cat";
	const char* gpNBURestorePg = "gp_bsa_restore_agent";
	const char* netbackupServiceHost = NULL;
	const char* netbackupBlockSize = NULL;

	formSegmentPsqlCommandLine(&cmdLine, inputFileSpec, compUsed, compProg, 
							   NULL, NULL, role,
							   psqlPg, catPg, gpNBURestorePg,
							   netbackupServiceHost, netbackupBlockSize, NULL, NULL);

	char *expected_output = "cat fileSpec | psql";
	assert_string_equal(cmdLine, expected_output);
	free(cmdLine);
}

void test__formSegmentPsqlCommandLine9(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = false;
	const char* compProg = "gzip -c ";
	const char* filter_script = "filter.py";
	const char* table_filter_file = "filter.conf";
	int role = ROLE_SEGDB;
	const char* psqlPg = "psql";
	const char* catPg = "cat";
	const char* gpNBURestorePg = "gp_bsa_restore_agent";
	const char* netbackupServiceHost = "mdw";
	const char* netbackupBlockSize = "1024";

	formSegmentPsqlCommandLine(&cmdLine, inputFileSpec, compUsed, compProg,
							   filter_script, table_filter_file, role,
							   psqlPg, catPg, gpNBURestorePg,
							   netbackupServiceHost, netbackupBlockSize, NULL, NULL);

	char *expected_output = "gp_bsa_restore_agent --netbackup-service-host mdw --netbackup-filename fileSpec --netbackup-block-size 1024 | filter.py -t filter.conf | psql";
	assert_string_equal(cmdLine, expected_output);
	free(cmdLine);
}

void test__formSegmentPsqlCommandLine10(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = false;
	const char* compProg = "gzip -c ";
	int role = ROLE_SEGDB;
	const char* psqlPg = "psql";
	const char* catPg = "cat";
	const char* gpNBURestorePg = "gp_bsa_restore_agent";
	const char* netbackupServiceHost = "mdw";
	const char* netbackupBlockSize = "1024";

	formSegmentPsqlCommandLine(&cmdLine, inputFileSpec, compUsed, compProg,
							   NULL, NULL, role,
							   psqlPg, catPg, gpNBURestorePg,
							   netbackupServiceHost, netbackupBlockSize, NULL, NULL);

	char *expected_output = "gp_bsa_restore_agent --netbackup-service-host mdw --netbackup-filename fileSpec --netbackup-block-size 1024 | psql";
	assert_string_equal(cmdLine, expected_output);
	free(cmdLine);
}

void test__formSegmentPsqlCommandLine11(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = false;
	const char* compProg = "gzip -c ";
	const char* filter_script = "filter.py";
	const char* table_filter_file = "filter.conf";
	int role = ROLE_MASTER;
	const char* psqlPg = "psql";
	const char* catPg = "cat";
	const char* gpNBURestorePg = "gp_bsa_restore_agent";
	const char* netbackupServiceHost = "mdw";
	const char* netbackupBlockSize = NULL;

	formSegmentPsqlCommandLine(&cmdLine, inputFileSpec, compUsed, compProg,
							   filter_script, table_filter_file, role,
							   psqlPg, catPg, gpNBURestorePg,
							   netbackupServiceHost, netbackupBlockSize, NULL, NULL);

	char *expected_output = "gp_bsa_restore_agent --netbackup-service-host mdw --netbackup-filename fileSpec | filter.py -m -t filter.conf | psql";
	assert_string_equal(cmdLine, expected_output);
	free(cmdLine);
}

void test__formSegmentPsqlCommandLine12(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = false;
	const char* compProg = "gzip -c ";
	int role = ROLE_MASTER;
	const char* psqlPg = "psql";
	const char* catPg = "cat";
	const char* gpNBURestorePg = "gp_bsa_restore_agent";
	const char* netbackupServiceHost = "mdw";
	const char* netbackupBlockSize = "1024";

	formSegmentPsqlCommandLine(&cmdLine, inputFileSpec, compUsed, compProg,
							   NULL, NULL, role,
							   psqlPg, catPg, gpNBURestorePg,
							   netbackupServiceHost, netbackupBlockSize, NULL, NULL);

	char *expected_output = "gp_bsa_restore_agent --netbackup-service-host mdw --netbackup-filename fileSpec --netbackup-block-size 1024 | psql";
	assert_string_equal(cmdLine, expected_output);
	free(cmdLine);
}

/* Test to verify command line for gp_restore_agent */
void test__formSegmentPsqlCommandLine13(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = false;
	const char* compProg = "gzip -c ";
	int role = ROLE_MASTER;
	const char* filter_script = "filter.py"; 
	const char* table_filter_file = "filter.conf";
	const char* psqlPg = "psql";
	const char* catPg = "cat";
	const char* gpNBURestorePg = "gp_bsa_restore_agent";
	const char* netbackupServiceHost = "mdw";
	const char* netbackupBlockSize = "1024";
	const char* change_schema = "newschema_file";

	formSegmentPsqlCommandLine(&cmdLine, inputFileSpec, compUsed, compProg,
							   filter_script, table_filter_file, role,
							   psqlPg, catPg, gpNBURestorePg,
							   netbackupServiceHost, netbackupBlockSize, change_schema, NULL);

	char *expected_output = "gp_bsa_restore_agent --netbackup-service-host mdw --netbackup-filename fileSpec --netbackup-block-size 1024 | filter.py -m -t filter.conf -c newschema_file | psql";
	assert_string_equal(cmdLine, expected_output);
	free(cmdLine);
}

/* Test to verify command line for restore filter */
void test__formSegmentPsqlCommandLine14(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = false;
	const char* compProg = "gzip -c ";
	int role = ROLE_MASTER;
	const char* filter_script = "filter.py"; 
	const char* table_filter_file = "filter.conf";
	const char* psqlPg = "psql";
	const char* catPg = "cat";
	const char* gpNBURestorePg = "gp_bsa_restore_agent";
	const char* change_schema = "newschema_file";

	formSegmentPsqlCommandLine(&cmdLine, inputFileSpec, compUsed, compProg,
							   filter_script, table_filter_file, role,
							   psqlPg, catPg, gpNBURestorePg,
							   NULL, NULL, change_schema, NULL);

	char *expected_output = "cat fileSpec | filter.py -m -t filter.conf -c newschema_file | psql";
	assert_string_equal(cmdLine, expected_output);
	free(cmdLine);
}

/* Test to verify command line for gp_restore_agent */
void test__formSegmentPsqlCommandLine15(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = false;
	const char* compProg = "gzip -c ";
	int role = ROLE_MASTER;
	const char* filter_script = "filter.py";
	const char* table_filter_file = "filter.conf";
	const char* psqlPg = "psql";
	const char* catPg = "cat";
	const char* gpNBURestorePg = "gp_bsa_restore_agent";
	const char* netbackupServiceHost = "mdw";
	const char* netbackupBlockSize = "1024";
	const char* schema_level_file = "schema_level_file";

	formSegmentPsqlCommandLine(&cmdLine, inputFileSpec, compUsed, compProg,
							   filter_script, table_filter_file, role,
							   psqlPg, catPg, gpNBURestorePg,
							   netbackupServiceHost, netbackupBlockSize, NULL, schema_level_file);

	char *expected_output = "gp_bsa_restore_agent --netbackup-service-host mdw --netbackup-filename fileSpec --netbackup-block-size 1024 | filter.py -m -t filter.conf -s schema_level_file | psql";
	assert_string_equal(cmdLine, expected_output);
	free(cmdLine);
}

/* Test to verify command line for restore filter */
void test__formSegmentPsqlCommandLine16(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = false;
	const char* compProg = "gzip -c ";
	int role = ROLE_MASTER;
	const char* filter_script = "filter.py";
	const char* table_filter_file = "filter.conf";
	const char* psqlPg = "psql";
	const char* catPg = "cat";
	const char* gpNBURestorePg = "gp_bsa_restore_agent";
	const char* schema_level_file = "schema_level_file";

	formSegmentPsqlCommandLine(&cmdLine, inputFileSpec, compUsed, compProg,
							   filter_script, table_filter_file, role,
							   psqlPg, catPg, gpNBURestorePg,
							   NULL, NULL, NULL, schema_level_file);

	char *expected_output = "cat fileSpec | filter.py -m -t filter.conf -s schema_level_file | psql";
	assert_string_equal(cmdLine, expected_output);
	free(cmdLine);
}

#ifdef USE_DDBOOST
void test__formDDBoostPsqlCommandLine1(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = true;
	const char* compProg = "gzip -c";
	const char* filter_script = "filter.py"; 
	const char* table_filter_file = "filter.conf";
	int role = ROLE_SEGDB;
	const char* psqlPg = "psql"; 
	const char* ddboostPg = "ddboostPg";
	const char* ddp_file_name = "ddb_filename";
	const char* dd_boost_buf_size = "512MB";
	bool postSchemaOnly = false;

	formDDBoostPsqlCommandLine(&cmdLine, compUsed, ddboostPg, compProg,
							   ddp_file_name, dd_boost_buf_size,
							   filter_script, table_filter_file,
							   role, psqlPg, postSchemaOnly, NULL, NULL, NULL);

	char *e = "ddboostPg --readFile --from-file=ddb_filename.gz --dd_boost_buf_size=512MB | gzip -c | filter.py -t filter.conf | psql";
	assert_string_equal(cmdLine, e);
	free(cmdLine);
}

void test__formDDBoostPsqlCommandLine2(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = true;
	const char* compProg = "gzip -c";
	int role = ROLE_SEGDB;
	const char* psqlPg = "psql"; 
	const char* ddboostPg = "ddboostPg";
	const char* ddp_file_name = "ddb_filename";
	const char* dd_boost_buf_size = "512MB";
	bool postSchemaOnly = false;

	formDDBoostPsqlCommandLine(&cmdLine, compUsed, ddboostPg, compProg, 
							   ddp_file_name, dd_boost_buf_size, 
							   NULL, NULL,
							   role, psqlPg, postSchemaOnly, NULL, NULL, NULL);

	char *e = "ddboostPg --readFile --from-file=ddb_filename.gz --dd_boost_buf_size=512MB | gzip -c | psql";
	printf("cmdLine is %s", cmdLine);

	assert_string_equal(cmdLine, e);
	free(cmdLine);
}

void test__formDDBoostPsqlCommandLine3(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = false;
	const char* compProg = "gzip -c";
	const char* filter_script = "filter.py"; 
	const char* table_filter_file = "filter.conf";
	int role = ROLE_SEGDB;
	const char* psqlPg = "psql"; 
	const char* ddboostPg = "ddboostPg";
	const char* ddp_file_name = "ddb_filename";
	const char* dd_boost_buf_size = "512MB";
	bool postSchemaOnly = false;

	formDDBoostPsqlCommandLine(&cmdLine, compUsed, ddboostPg, compProg, 
							   ddp_file_name, dd_boost_buf_size, 
							   filter_script, table_filter_file,
							   role, psqlPg, postSchemaOnly, NULL, NULL, NULL);

    char *e = "ddboostPg --readFile --from-file=ddb_filename --dd_boost_buf_size=512MB | filter.py -t filter.conf | psql";

	assert_string_equal(cmdLine, e);
	free(cmdLine);
}

void test__formDDBoostPsqlCommandLine4(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = false;
	const char* compProg = "gzip -c";
	int role = ROLE_SEGDB;
	const char* psqlPg = "psql"; 
	const char* ddboostPg = "ddboostPg";
	const char* ddp_file_name = "ddb_filename";
	const char* dd_boost_buf_size = "512MB";
	bool postSchemaOnly = false;

	formDDBoostPsqlCommandLine(&cmdLine, compUsed, ddboostPg, compProg, 
							   ddp_file_name, dd_boost_buf_size, 
							   NULL, NULL,
							   role, psqlPg, postSchemaOnly, NULL, NULL, NULL);

	char *e = "ddboostPg --readFile --from-file=ddb_filename --dd_boost_buf_size=512MB | psql";
	assert_string_equal(cmdLine, e);
	free(cmdLine);
}

void test__formDDBoostPsqlCommandLine5(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = true;
	const char* compProg = "gzip -c";
	const char* filter_script = "filter.py"; 
	const char* table_filter_file = "filter.conf";
	int role = ROLE_MASTER;
	const char* psqlPg = "psql"; 
	const char* ddboostPg = "ddboostPg";
	const char* ddp_file_name = "ddb_filename";
	const char* dd_boost_buf_size = "512MB";
	bool postSchemaOnly = false;

	formDDBoostPsqlCommandLine(&cmdLine, compUsed, ddboostPg, compProg, 
							   ddp_file_name, dd_boost_buf_size, 
							   filter_script, table_filter_file,
							   role, psqlPg, postSchemaOnly, NULL, NULL, NULL);

	char *e = "ddboostPg --readFile --from-file=ddb_filename.gz --dd_boost_buf_size=512MB | gzip -c | filter.py -m -t filter.conf | psql";
	assert_string_equal(cmdLine, e);
	free(cmdLine);
}

void test__formDDBoostPsqlCommandLine6(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = true;
	const char* compProg = "gzip -c";
	int role = ROLE_MASTER;
	const char* psqlPg = "psql"; 
	const char* ddboostPg = "ddboostPg";
	const char* ddp_file_name = "ddb_filename";
	const char* dd_boost_buf_size = "512MB";
	bool postSchemaOnly = false;

	formDDBoostPsqlCommandLine(&cmdLine, compUsed, ddboostPg, compProg, 
							   ddp_file_name, dd_boost_buf_size, 
							   NULL, NULL,
							   role, psqlPg, postSchemaOnly, NULL, NULL, NULL);

	char *e = "ddboostPg --readFile --from-file=ddb_filename.gz --dd_boost_buf_size=512MB | gzip -c | psql";
	assert_string_equal(cmdLine, e);
	free(cmdLine);
}

void test__formDDBoostPsqlCommandLine7(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = false;
	const char* compProg = "gzip -c";
	const char* filter_script = "filter.py"; 
	const char* table_filter_file = "filter.conf";
	int role = ROLE_MASTER;
	const char* psqlPg = "psql"; 
	const char* ddboostPg = "ddboostPg";
	const char* ddp_file_name = "ddb_filename";
	const char* dd_boost_buf_size = "512MB";
	bool postSchemaOnly =false;

	formDDBoostPsqlCommandLine(&cmdLine, compUsed, ddboostPg, compProg, 
							   ddp_file_name, dd_boost_buf_size, 
							   filter_script, table_filter_file,
							   role, psqlPg, postSchemaOnly, NULL, NULL, NULL);

	char *e = "ddboostPg --readFile --from-file=ddb_filename --dd_boost_buf_size=512MB | filter.py -m -t filter.conf | psql";
	assert_string_equal(cmdLine, e);
	free(cmdLine);
}

void test__formDDBoostPsqlCommandLine8(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = false;
	const char* compProg = "gzip -c";
	int role = ROLE_MASTER;
	const char* psqlPg = "psql"; 
	const char* ddboostPg = "ddboostPg";
	const char* ddp_file_name = "ddb_filename";
	const char* dd_boost_buf_size = "512MB";
	bool postSchemaOnly = false;

	formDDBoostPsqlCommandLine(&cmdLine, compUsed, ddboostPg, compProg, 
							   ddp_file_name, dd_boost_buf_size, 
							   NULL, NULL,
							   role, psqlPg, postSchemaOnly, NULL, NULL, NULL);

	char *e = "ddboostPg --readFile --from-file=ddb_filename --dd_boost_buf_size=512MB | psql";
	assert_string_equal(cmdLine, e);
	free(cmdLine);
}

void test__formDDBoostPsqlCommandLine9(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = true;
	const char* compProg = "gzip -c";
	int role = ROLE_MASTER;
	const char* filter_script = "filter.py";
	const char* table_filter_file = "filter.conf";
	const char* psqlPg = "psql";
	const char* ddboostPg = "ddboostPg";
	const char* ddp_file_name = "ddb_filename";
	const char* dd_boost_buf_size = "512MB";
	bool postSchemaOnly = false;
	const char* change_schema_file = "/tmp/change_schema_file";

	formDDBoostPsqlCommandLine(&cmdLine, compUsed, ddboostPg, compProg,
							   ddp_file_name, dd_boost_buf_size,
							   filter_script, table_filter_file,
							   role, psqlPg, postSchemaOnly, change_schema_file, NULL, NULL);

	char *e = "ddboostPg --readFile --from-file=ddb_filename.gz --dd_boost_buf_size=512MB | gzip -c | filter.py -m -t filter.conf -c /tmp/change_schema_file | psql";
	assert_string_equal(cmdLine, e);
	free(cmdLine);
}

void test__formDDBoostPsqlCommandLine10(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = true;
	const char* compProg = "gzip -c";
	int role = ROLE_MASTER;
	const char* filter_script = "filter.py";
	const char* table_filter_file = "filter.conf";
	const char* psqlPg = "psql";
	const char* ddboostPg = "ddboostPg";
	const char* ddp_file_name = "ddb_filename";
	const char* dd_boost_buf_size = "512MB";
	bool postSchemaOnly = false;
	const char* schema_level_file = "/tmp/schema_level_file";

	formDDBoostPsqlCommandLine(&cmdLine, compUsed, ddboostPg, compProg,
							   ddp_file_name, dd_boost_buf_size,
							   filter_script, table_filter_file,
							   role, psqlPg, postSchemaOnly, NULL, schema_level_file, NULL);

	char *e = "ddboostPg --readFile --from-file=ddb_filename.gz --dd_boost_buf_size=512MB | gzip -c | filter.py -m -t filter.conf -s /tmp/schema_level_file | psql";
	assert_string_equal(cmdLine, e);
	free(cmdLine);
}


void test__formDDBoostPsqlCommandLine11(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = true;
	const char* compProg = "gzip -c";
	int role = ROLE_SEGDB;
	const char* filter_script = "filter.py";
	const char* table_filter_file = "filter.conf";
	const char* psqlPg = "psql";
	const char* ddboostPg = "ddboostPg";
	const char* ddp_file_name = "ddb_filename";
	const char* dd_boost_buf_size = "512MB";
	bool postSchemaOnly = false;
	const char* schema_level_file = "/tmp/schema_level_file";

	formDDBoostPsqlCommandLine(&cmdLine, compUsed, ddboostPg, compProg,
							   ddp_file_name, dd_boost_buf_size,
							   filter_script, table_filter_file,
							   role, psqlPg, postSchemaOnly, NULL, schema_level_file, NULL);

	char *e = "ddboostPg --readFile --from-file=ddb_filename.gz --dd_boost_buf_size=512MB | gzip -c | filter.py -t filter.conf -s /tmp/schema_level_file | psql";
	assert_string_equal(cmdLine, e);
	free(cmdLine);
}

void test__formDDBoostPsqlCommandLine12(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = true;
	const char* compProg = "gzip -c";
	int role = ROLE_SEGDB;
	const char* filter_script = "filter.py";
	const char* table_filter_file = "filter.conf";
	const char* psqlPg = "psql";
	const char* ddboostPg = "ddboostPg";
	const char* ddp_file_name = "ddb_filename";
	const char* dd_boost_buf_size = "512MB";
	bool postSchemaOnly = false;
	const char* change_schema_file = "/tmp/change_schema_file";

	formDDBoostPsqlCommandLine(&cmdLine, compUsed, ddboostPg, compProg,
							   ddp_file_name, dd_boost_buf_size,
							   filter_script, table_filter_file,
							   role, psqlPg, postSchemaOnly, change_schema_file, NULL, NULL);

	char *e = "ddboostPg --readFile --from-file=ddb_filename.gz --dd_boost_buf_size=512MB | gzip -c | filter.py -t filter.conf -c /tmp/change_schema_file | psql";
	assert_string_equal(cmdLine, e);
	free(cmdLine);
}

void test__formDDBoostPsqlCommandLine13_with_storage_unit(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = true;
	const char* compProg = "gzip -c";
	int role = ROLE_SEGDB;
	const char* filter_script = "filter.py";
	const char* table_filter_file = "filter.conf";
	const char* psqlPg = "psql";
	const char* ddboostPg = "ddboostPg";
	const char* ddp_file_name = "ddb_filename";
	const char* dd_boost_buf_size = "512MB";
	bool postSchemaOnly = false;
	const char* change_schema_file = "/tmp/change_schema_file";
	const char* ddboost_storage_unit = "foo";

	formDDBoostPsqlCommandLine(&cmdLine, compUsed, ddboostPg, compProg,
							   ddp_file_name, dd_boost_buf_size,
							   filter_script, table_filter_file,
							   role, psqlPg, postSchemaOnly, change_schema_file, NULL,
							ddboost_storage_unit);

	char *e = "ddboostPg --readFile --from-file=ddb_filename.gz --ddboost-storage-unit=foo --dd_boost_buf_size=512MB | gzip -c | filter.py -t filter.conf -c /tmp/change_schema_file | psql";
	assert_string_equal(cmdLine, e);
	free(cmdLine);
}

void test__formDDBoostPsqlCommandLine_with_postSchemaOnly_and_master_role1(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = false;
	const char* compProg = "gzip -c";
	int role = ROLE_MASTER;
	const char* psqlPg = "psql";
	const char* ddboostPg = "ddboostPg";
	const char* ddp_file_name = "ddb_filename";
	const char* postDataFilterScript = "gprestore_post_data_filter.py";
	const char* tableFilterFile = "tablefilter";
	const char* dd_boost_buf_size = "512MB";
	bool postSchemaOnly = true;
	const char* change_schema_file = "/tmp/change_schema_file";

	formDDBoostPsqlCommandLine(&cmdLine, compUsed, ddboostPg, compProg,
							   ddp_file_name, dd_boost_buf_size,
							   postDataFilterScript, tableFilterFile,
							   role, psqlPg, postSchemaOnly, change_schema_file, NULL, NULL);

	char *e = "ddboostPg --readFile --from-file=ddb_filename --dd_boost_buf_size=512MB | gprestore_post_data_filter.py -t tablefilter -c /tmp/change_schema_file | psql";
	assert_string_equal(cmdLine, e);
	free(cmdLine);
}

void test__formDDBoostPsqlCommandLine_with_postSchemaOnly_and_master_role2(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = true;
	const char* compProg = "gzip -c";
	int role = ROLE_MASTER;
	const char* psqlPg = "psql";
	const char* ddboostPg = "ddboostPg";
	const char* ddp_file_name = "ddb_filename";
	const char* postDataFilterScript = "gprestore_post_data_filter.py";
	const char* tableFilterFile = "tablefilter";
	const char* dd_boost_buf_size = "512MB";
	const char* schema_level_file= "/tmp/schema_level_file";
	bool postSchemaOnly = true;

	formDDBoostPsqlCommandLine(&cmdLine, compUsed, ddboostPg, compProg,
							   ddp_file_name, dd_boost_buf_size,
							   postDataFilterScript, tableFilterFile,
							   role, psqlPg, postSchemaOnly, NULL, schema_level_file, NULL);

	char *e = "ddboostPg --readFile --from-file=ddb_filename.gz --dd_boost_buf_size=512MB | gzip -c | gprestore_post_data_filter.py -t tablefilter -s /tmp/schema_level_file | psql";
	assert_string_equal(cmdLine, e);
	free(cmdLine);
}
void test__formDDBoostPsqlCommandLine_with_postSchemaOnly_and_segment_role1(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = false;
	const char* compProg = "gzip -c";
	int role = ROLE_SEGDB;
	const char* psqlPg = "psql";
	const char* ddboostPg = "ddboostPg";
	const char* ddp_file_name = "ddb_filename";
	const char* postDataFilterScript = "gprestore_post_data_filter.py";
	const char* tableFilterFile = "tablefilter";
	const char* dd_boost_buf_size = "512MB";
	bool postSchemaOnly = true;
	const char* change_schema_file = "/tmp/change_schema_file";

	formDDBoostPsqlCommandLine(&cmdLine, compUsed, ddboostPg, compProg,
							   ddp_file_name, dd_boost_buf_size,
							   postDataFilterScript, tableFilterFile,
							   role, psqlPg, postSchemaOnly, change_schema_file, NULL, NULL);

	char *e = "ddboostPg --readFile --from-file=ddb_filename --dd_boost_buf_size=512MB | gprestore_post_data_filter.py -t tablefilter -c /tmp/change_schema_file | psql";
	assert_string_equal(cmdLine, e);
	free(cmdLine);
}


void test__formDDBoostPsqlCommandLine_with_postSchemaOnly_and_segment_role2(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = true;
	const char* compProg = "gzip -c";
	int role = ROLE_SEGDB;
	const char* psqlPg = "psql";
	const char* ddboostPg = "ddboostPg";
	const char* ddp_file_name = "ddb_filename";
	const char* postDataFilterScript = "gprestore_post_data_filter.py";
	const char* tableFilterFile = "tablefilter";
	const char* dd_boost_buf_size = "512MB";
	bool postSchemaOnly = true;
	const char* schema_level_file = "/tmp/schema_level_file";

	formDDBoostPsqlCommandLine(&cmdLine, compUsed, ddboostPg, compProg,
							   ddp_file_name, dd_boost_buf_size,
							   postDataFilterScript, tableFilterFile,
							   role, psqlPg, postSchemaOnly, NULL, schema_level_file, NULL);

	char *e = "ddboostPg --readFile --from-file=ddb_filename.gz --dd_boost_buf_size=512MB | gzip -c | gprestore_post_data_filter.py -t tablefilter -s /tmp/schema_level_file | psql";
	assert_string_equal(cmdLine, e);
	free(cmdLine);
}

#endif

void test__shouldExpandChildren1(void **state)
{
    bool g_gp_supportsPartitioning = false;
    bool no_expand_children = false;
    bool result = shouldExpandChildren(g_gp_supportsPartitioning, no_expand_children);
    assert_false(result);
}

void test__shouldExpandChildren2(void **state)
{
    bool g_gp_supportsPartitioning = true;
    bool no_expand_children = false;
    bool result = shouldExpandChildren(g_gp_supportsPartitioning, no_expand_children);
    assert_true(result);
}

void test__shouldExpandChildren3(void **state)
{
    bool g_gp_supportsPartitioning = true;
    bool no_expand_children = true;
    bool result = shouldExpandChildren(g_gp_supportsPartitioning, no_expand_children);
    assert_false(result);
}

void test__shouldExpandChildren4(void **state)
{
    bool g_gp_supportsPartitioning = false;
    bool no_expand_children = true;
    bool result = shouldExpandChildren(g_gp_supportsPartitioning, no_expand_children);
    assert_false(result);
}

void test__formPostDataSchemaOnlyPsqlCommandLine1(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = true;
	const char* compProg = "gzip -c";
	const char* psqlPg = "psql"; 
	const char* catPg = "cat";
	const char* postDataFilterScript = "gprestore_post_data_filter.py";
	const char* tableFilterFile = "tablefilter";
	const char* gpNBURestorePg = "gp_bsa_restore_agent";
	const char* netbackupServiceHost = NULL;
	const char* netbackupBlockSize = NULL;
	
	formPostDataSchemaOnlyPsqlCommandLine(&cmdLine, inputFileSpec, compUsed, compProg, 
							postDataFilterScript, tableFilterFile, 
							psqlPg, catPg, gpNBURestorePg,
							netbackupServiceHost, netbackupBlockSize, NULL, NULL);

	char *expected_output = "cat fileSpec | gzip -c | gprestore_post_data_filter.py -t tablefilter | psql";
	assert_string_equal(cmdLine, expected_output);
	free(cmdLine);
}

void test__formPostDataSchemaOnlyPsqlCommandLine2(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = false;
	const char* compProg = "gzip -c";
	const char* psqlPg = "psql"; 
	const char* catPg = "cat";
	const char* postDataFilterScript = "gprestore_post_data_filter.py";
	const char* tableFilterFile = "tablefilter";
	const char* gpNBURestorePg = "gp_bsa_restore_agent";
	const char* netbackupServiceHost = NULL;
	const char* netbackupBlockSize = NULL;

	formPostDataSchemaOnlyPsqlCommandLine(&cmdLine, inputFileSpec, compUsed, compProg, 
							postDataFilterScript, tableFilterFile,
							psqlPg, catPg, gpNBURestorePg,
							netbackupServiceHost, netbackupBlockSize, NULL, NULL);

	char *expected_output = "cat fileSpec | gprestore_post_data_filter.py -t tablefilter | psql";
	assert_string_equal(cmdLine, expected_output);
	free(cmdLine);
}

void test__formPostDataSchemaOnlyPsqlCommandLine3(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = false;
	const char* compProg = "gzip -c";
	const char* psqlPg = "psql";
	const char* catPg = "cat";
	const char* postDataFilterScript = "gprestore_post_data_filter.py";
	const char* tableFilterFile = "tablefilter";
	const char* gpNBURestorePg = "gp_bsa_restore_agent";
	const char* netbackupServiceHost = "mdw";
	const char* netbackupBlockSize = "1024";

	formPostDataSchemaOnlyPsqlCommandLine(&cmdLine, inputFileSpec, compUsed, compProg,
							postDataFilterScript, tableFilterFile,
							psqlPg, catPg, gpNBURestorePg,
							netbackupServiceHost, netbackupBlockSize, NULL, NULL);

	char *expected_output = "gp_bsa_restore_agent --netbackup-service-host mdw --netbackup-filename fileSpec --netbackup-block-size 1024 | gprestore_post_data_filter.py -t tablefilter | psql";
	assert_string_equal(cmdLine, expected_output);
	free(cmdLine);
}

void test__formPostDataSchemaOnlyPsqlCommandLine4(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = false;
	const char* compProg = "gzip -c";
	const char* psqlPg = "psql";
	const char* catPg = "cat";
	const char* postDataFilterScript = "gprestore_post_data_filter.py";
	const char* tableFilterFile = "tablefilter";
	const char* gpNBURestorePg = "gp_bsa_restore_agent";
	const char* netbackupServiceHost = "mdw";
	const char* netbackupBlockSize = "1024";
	const char* change_schema_file = "/tmp/change_schema_file";

	formPostDataSchemaOnlyPsqlCommandLine(&cmdLine, inputFileSpec, compUsed, compProg,
							postDataFilterScript, tableFilterFile,
							psqlPg, catPg, gpNBURestorePg,
							netbackupServiceHost, netbackupBlockSize, change_schema_file, NULL);

	char *expected_output = "gp_bsa_restore_agent --netbackup-service-host mdw --netbackup-filename fileSpec --netbackup-block-size 1024 | gprestore_post_data_filter.py -t tablefilter -c /tmp/change_schema_file | psql";
	assert_string_equal(cmdLine, expected_output);
	free(cmdLine);
}

void test__formPostDataSchemaOnlyPsqlCommandLine5(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = false;
	const char* compProg = "gzip -c";
	const char* psqlPg = "psql";
	const char* catPg = "cat";
	const char* postDataFilterScript = "gprestore_post_data_filter.py";
	const char* tableFilterFile = "tablefilter";
	const char* gpNBURestorePg = "gp_bsa_restore_agent";
	const char* netbackupServiceHost = "mdw";
	const char* netbackupBlockSize = "1024";
	const char* schema_level_file = "/tmp/schema_level_file";

	formPostDataSchemaOnlyPsqlCommandLine(&cmdLine, inputFileSpec, compUsed, compProg,
							postDataFilterScript, tableFilterFile,
							psqlPg, catPg, gpNBURestorePg,
							netbackupServiceHost, netbackupBlockSize, NULL, schema_level_file);

	char *expected_output = "gp_bsa_restore_agent --netbackup-service-host mdw --netbackup-filename fileSpec --netbackup-block-size 1024 | gprestore_post_data_filter.py -t tablefilter -s /tmp/schema_level_file | psql";
	assert_string_equal(cmdLine, expected_output);
	free(cmdLine);
}

void test__formPostDataSchemaOnlyPsqlCommandLine6(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = true;
	const char* compProg = "gzip -c";
	const char* psqlPg = "psql";
	const char* catPg = "cat";
	const char* postDataFilterScript = "gprestore_post_data_filter.py";
	const char* tableFilterFile = "tablefilter";
	const char* gpNBURestorePg = "gp_bsa_restore_agent";
	const char* netbackupServiceHost = "mdw";
	const char* netbackupBlockSize = "1024";
	const char* change_schema_file = "/tmp/change_schema_file";

	formPostDataSchemaOnlyPsqlCommandLine(&cmdLine, inputFileSpec, compUsed, compProg,
							postDataFilterScript, tableFilterFile,
							psqlPg, catPg, gpNBURestorePg,
							netbackupServiceHost, netbackupBlockSize, change_schema_file, NULL);

	char *expected_output = "gp_bsa_restore_agent --netbackup-service-host mdw --netbackup-filename fileSpec --netbackup-block-size 1024 | gzip -c | gprestore_post_data_filter.py -t tablefilter -c /tmp/change_schema_file | psql";
	assert_string_equal(cmdLine, expected_output);
	free(cmdLine);
}

void test__formPostDataSchemaOnlyPsqlCommandLine7(void **state)
{
	char *cmdLine = calloc(1000000, 1);
	char *inputFileSpec = "fileSpec";
	bool compUsed = true;
	const char* compProg = "gzip -c";
	const char* psqlPg = "psql";
	const char* catPg = "cat";
	const char* postDataFilterScript = "gprestore_post_data_filter.py";
	const char* tableFilterFile = "tablefilter";
	const char* gpNBURestorePg = "gp_bsa_restore_agent";
	const char* netbackupServiceHost = "mdw";
	const char* netbackupBlockSize = "1024";
	const char* schema_level_file = "/tmp/schema_level_file";

	formPostDataSchemaOnlyPsqlCommandLine(&cmdLine, inputFileSpec, compUsed, compProg,
							postDataFilterScript, tableFilterFile,
							psqlPg, catPg, gpNBURestorePg,
							netbackupServiceHost, netbackupBlockSize, NULL, schema_level_file);

	char *expected_output = "gp_bsa_restore_agent --netbackup-service-host mdw --netbackup-filename fileSpec --netbackup-block-size 1024 | gzip -c | gprestore_post_data_filter.py -t tablefilter -s /tmp/schema_level_file | psql";
	assert_string_equal(cmdLine, expected_output);
	free(cmdLine);
}
void test__shouldDumpSchemaOnly1(void **state)
{
    int role = ROLE_SEGDB;
    bool incrementalBackup = true;
    void *list = NULL;   
 
    bool result = shouldDumpSchemaOnly(role, incrementalBackup, list);
    assert_true(result);
}

void test__shouldDumpSchemaOnly2(void **state)
{
    int role = ROLE_MASTER;
    bool incrementalBackup = true;
    void *list = NULL;   
 
    bool result = shouldDumpSchemaOnly(role, incrementalBackup, list);
    assert_false(result);
}

void test__shouldDumpSchemaOnly3(void **state)
{
    int role = ROLE_SEGDB;
    bool incrementalBackup = true;
    void *list = "test";
 
    bool result = shouldDumpSchemaOnly(role, incrementalBackup, list);
    assert_false(result);
}
/* Tests for hash table used in cdb_dump_include.c getTableAttributes function */

void 
test__initialize_hash_table(void **state)
{   int a = 90;   
	int result = initializeHashTable(a);
    assert_true(result == 0);
    assert_true(hash_table != NULL);
    assert_true(HASH_TABLE_SIZE == 90); 
	cleanUpTable();
}

void 
test__insert_into_hash_table(void **state)
{   
    initializeHashTable(90);
    Oid o = 139753;
    int t = 'x'; 
	int result = insertIntoHashTable(o, t);
    assert_true(result == 0);
    result = getTypstorage(o);
    assert_true(result == 'x');
	cleanUpTable();
}

void 
test__insert_into_hash_table_duplicate(void **state)
{   
    initializeHashTable(100);
    Oid o = 139753;
    int t = 'x'; 
	int result = insertIntoHashTable(o, t);
    assert_true(result == 0);
	result = insertIntoHashTable(o, t);
    assert_true(result == -1);
	cleanUpTable();
}

void 
test__insert_into_hash_table_large_num_keys(void **state)
{   
    initializeHashTable(10);
    Oid o = 139753;
    int result = 0;
    int i = 0;
    for(; i < 20; i++)
    {
        result = insertIntoHashTable(o + i, 'x');    
        assert_true(result == 0);
    }	

    for(i = 0; i < 20; i++)
    {
        result = removeNode(o + i);    
        assert_true(result == 0);
    }

    for(i = 0; i < 20; i++)
    {
        char typstorage = getTypstorage(o + i);    
		assert_true(typstorage == EMPTY_TYPSTORAGE);
    }
	cleanUpTable();
}

void 
test__hash_func(void **state)
{   
    HASH_TABLE_SIZE = 1000;
    Oid o = 100;
	int result = hashFunc(o);
    assert_true(result == 100);
}

void 
test__hash_func_range(void **state)
{   
    HASH_TABLE_SIZE = 1000;
    Oid o = 1000000;
	int result = hashFunc(o);
    assert_true(result < HASH_TABLE_SIZE && result >= 0);
}

void 
test__get_typstorage(void **state)
{   
    initializeHashTable(100);
    Oid o = 139753;
    int t = 'x'; 
	int result = insertIntoHashTable(o, t);
    assert_true(result == 0);
    char typstorage = getTypstorage(o);
    assert_true(typstorage == t);
	cleanUpTable();
}

void 
test__get_typstorage_invalid_key(void **state)
{   
    initializeHashTable(100);
	char typstorage = getTypstorage(100);
    assert_true(typstorage == EMPTY_TYPSTORAGE);
	cleanUpTable();
}

void 
test__remove_node(void **state)
{   
    initializeHashTable(100);
    Oid o = 139753;
    int t = 'x'; 
	int result = insertIntoHashTable(o, t);
    assert_true(result == 0);
	result = removeNode(o);
    assert_true(result == 0);
    char typstorage = getTypstorage(o);
    assert_true(typstorage == EMPTY_TYPSTORAGE);
	cleanUpTable();
}

void 
test__remove_node_not_present(void **state)
{   
    initializeHashTable(100);
	int result = removeNode(101);
    assert_true(result == -1);
	cleanUpTable();
}

void 
test__remove_node_not_present_in_list(void **state)
{   
    initializeHashTable(10);
    Oid o = 139753;
    int i = 0;
    int result = 0;
    for(; i < 20; i++)
    {
        result = insertIntoHashTable(o + i, 'x');    
        assert_true(result == 0);
    }	
	result = removeNode(o + 21);
	assert_true(result == -1);
	cleanUpTable();
}

void 
test__clean_up_table(void **state)
{   
    initializeHashTable(100);
    Oid o = 123456;
    int i = 0;
    int result = 0;
    for(; i < 20; i++)
    {
        result = insertIntoHashTable(o + i, 'x');    
        assert_true(result == 0);
    }
    cleanUpTable();	
    assert_true(hash_table == NULL);
    assert_true(HASH_TABLE_SIZE == 0);
}

/*
 * Test the base64 implementation in cdb_dump_util.c.
 */
void
test__base64(void **state)
{
	unsigned char orig[256];
	char	   *encoded;
	unsigned char *decoded;
	int			backlen;
	int			i;

	for (i = 0; i < 256; i++)
		orig[i] = i;

	encoded = DataToBase64((char *) orig, 256);
	decoded = (unsigned char *) Base64ToData(encoded, &backlen);
	assert_int_equal(backlen, 256);

	for (i = 0; i < 256; i++)
		assert_int_equal(decoded[i], i);
}

int 
main(int argc, char* argv[]) 
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
			unit_test(test__isFilteringAllowed1),
			unit_test(test__isFilteringAllowed2),
			unit_test(test__isFilteringAllowed3), 
			unit_test(test__isFilteringAllowed4), 
			unit_test(test__isFilteringAllowed5),
			unit_test(test__isFilteringAllowed6),
			unit_test(test__isFilteringAllowed7), 
			unit_test(test__isFilteringAllowed8), 
			unit_test(test__backupTypeString5),
			unit_test(test__backupTypeString6),
			unit_test(test__validateTimeStamp7),
			unit_test(test__validateTimeStamp8),
			unit_test(test__validateTimeStamp9),
			unit_test(test__validateTimeStamp10),
			unit_test(test__validateTimeStamp11),
			unit_test(test__validateTimeStamp12),
			unit_test(test__validateTimeStamp13),
			unit_test(test__validateTimeStamp14),
			unit_test(test__validateTimeStamp15),
			unit_test(test__validateTimeStamp16),
			unit_test(test__getTimeStampKey17),
			unit_test(test__getTimeStampKey18),
			unit_test(test__formCompressionProgramString1),
			unit_test(test__formCompressionProgramString2),
			unit_test(test__formSegmentPsqlCommandLine1),
			unit_test(test__formSegmentPsqlCommandLine2),
			unit_test(test__formSegmentPsqlCommandLine3),
			unit_test(test__formSegmentPsqlCommandLine4),
			unit_test(test__formSegmentPsqlCommandLine5),
			unit_test(test__formSegmentPsqlCommandLine6),
			unit_test(test__formSegmentPsqlCommandLine7),
			unit_test(test__formSegmentPsqlCommandLine8),
			unit_test(test__formSegmentPsqlCommandLine9),
			unit_test(test__formSegmentPsqlCommandLine10),
			unit_test(test__formSegmentPsqlCommandLine11),
			unit_test(test__formSegmentPsqlCommandLine12),
			unit_test(test__formSegmentPsqlCommandLine13),
			unit_test(test__formSegmentPsqlCommandLine14),
			unit_test(test__formSegmentPsqlCommandLine15),
			unit_test(test__formSegmentPsqlCommandLine16),
            #ifdef USE_DDBOOST
			unit_test(test__formDDBoostPsqlCommandLine1),
			unit_test(test__formDDBoostPsqlCommandLine2),
			unit_test(test__formDDBoostPsqlCommandLine3),
			unit_test(test__formDDBoostPsqlCommandLine4),
			unit_test(test__formDDBoostPsqlCommandLine5),
			unit_test(test__formDDBoostPsqlCommandLine6),
			unit_test(test__formDDBoostPsqlCommandLine7),
			unit_test(test__formDDBoostPsqlCommandLine8),
			unit_test(test__formDDBoostPsqlCommandLine9),
			unit_test(test__formDDBoostPsqlCommandLine10),
			unit_test(test__formDDBoostPsqlCommandLine11),
			unit_test(test__formDDBoostPsqlCommandLine12),
			unit_test(test__formDDBoostPsqlCommandLine13_with_storage_unit),
			unit_test(test__formDDBoostPsqlCommandLine_with_postSchemaOnly_and_master_role1),
			unit_test(test__formDDBoostPsqlCommandLine_with_postSchemaOnly_and_master_role2),
			unit_test(test__formDDBoostPsqlCommandLine_with_postSchemaOnly_and_segment_role1),
			unit_test(test__formDDBoostPsqlCommandLine_with_postSchemaOnly_and_segment_role2),
            #endif
			unit_test(test__shouldExpandChildren1),
			unit_test(test__shouldExpandChildren2),
			unit_test(test__shouldExpandChildren3),
			unit_test(test__shouldExpandChildren4),
			unit_test(test__formPostDataSchemaOnlyPsqlCommandLine1),
			unit_test(test__formPostDataSchemaOnlyPsqlCommandLine2),
			unit_test(test__formPostDataSchemaOnlyPsqlCommandLine3),
			unit_test(test__formPostDataSchemaOnlyPsqlCommandLine4),
			unit_test(test__formPostDataSchemaOnlyPsqlCommandLine5),
			unit_test(test__formPostDataSchemaOnlyPsqlCommandLine6),
			unit_test(test__formPostDataSchemaOnlyPsqlCommandLine7),
            unit_test(test__shouldDumpSchemaOnly1),
            unit_test(test__shouldDumpSchemaOnly2),
            unit_test(test__shouldDumpSchemaOnly3),
            //hash table tests
            unit_test(test__initialize_hash_table),       
            unit_test(test__insert_into_hash_table),       
            unit_test(test__insert_into_hash_table_duplicate),       
            unit_test(test__insert_into_hash_table_large_num_keys),       
            unit_test(test__hash_func),       
            unit_test(test__hash_func_range),       
            unit_test(test__get_typstorage),       
            unit_test(test__get_typstorage_invalid_key),       
            unit_test(test__remove_node),       
            unit_test(test__remove_node_not_present),       
            unit_test(test__remove_node_not_present_in_list),       
            unit_test(test__clean_up_table),
            unit_test(test__base64),
	};
	return run_tests(tests);
}
