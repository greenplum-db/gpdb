#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cmockery_gp.h"

#include "postgres_fe.h"

/*
 * implements:
 */
#include "gpdb5-cluster.h"
#include "test-upgrade-helpers.h"
#include "cluster-upgrade.h"
#include "pg-upgrade-copy.h"

static char *_pg_upgrade_output;
static int _pg_upgrade_exit_status;

typedef struct SegmentConfiguration
{
	char *old_data_directory;
	char *new_data_directory;
	char *segment_path;

	int new_dbid;
	int old_dbid;
} SegmentConfiguration;

void
performUpgrade(void)
{
	performUpgradeWithTablespaces(NULL);
}

void
performUpgradeWithTablespaces(char *mappingFilePath)
{
	char current_directory[1000];
	char master_hostname[1000];
	char *master_host_username = getenv("USER");
	gethostname(master_hostname, sizeof(master_hostname));
	getcwd(current_directory, sizeof(current_directory));

	char *new_master_data_directory = psprintf("%s/gpdb6-data/qddir/demoDataDir-1", current_directory);
	char *old_master_data_directory = psprintf("%s/gpdb5-data/qddir/demoDataDir-1", current_directory);
	char *old_bin_dir = psprintf("%s/gpdb5/bin", current_directory);
	char *new_bin_dir = psprintf("%s/gpdb6/bin", current_directory);
	int old_master_gp_dbid = 1;
	int new_master_gp_dbid = 1;

	SegmentConfiguration master_segment = {
		.old_dbid=1,
		.new_dbid=1,
		.old_data_directory=old_master_data_directory,
		.new_data_directory=psprintf("%s/gpdb6-data/qddir/demoDataDir-1", current_directory)
	};

	int number_of_segments = 3;
	SegmentConfiguration segment_configurations[] = {
		{
			.old_data_directory=psprintf("%s/gpdb5-data/dbfast1/demoDataDir0", current_directory),
			.new_data_directory=psprintf("%s/gpdb6-data/dbfast1/demoDataDir0", current_directory),
			.old_dbid=2,
			.new_dbid=2
		},
		{
			.old_data_directory=psprintf("%s/gpdb5-data/dbfast2/demoDataDir1", current_directory),
			.new_data_directory=psprintf("%s/gpdb6-data/dbfast2/demoDataDir1", current_directory),
			.old_dbid=3,
			.new_dbid=3,
		},
		{
			.old_data_directory=psprintf("%s/gpdb5-data/dbfast3/demoDataDir2", current_directory),
			.new_data_directory=psprintf("%s/gpdb6-data/dbfast3/demoDataDir2", current_directory),
			.old_dbid=4,
			.new_dbid=4,
		}
	};

	PgUpgradeResponse *response = perform_upgrade(
		make_pg_upgrade_options(
			old_master_data_directory,
			master_segment.new_data_directory,
			master_segment.old_dbid,
			master_segment.new_dbid,
			true,
			NULL,
			old_bin_dir,
			new_bin_dir,
			GPDB_FIVE_PORT));

	_pg_upgrade_exit_status = pg_upgrade_exit_status(response);

	if (_pg_upgrade_exit_status != 0)
		return;

	PgUpgradeResponse *segment_response;

	for (int i = 0; i < number_of_segments; i++)
	{
		SegmentConfiguration segment = segment_configurations[i];

		PgUpgradeOptions *segment_upgrade_options = make_pg_upgrade_options(
			segment.old_data_directory,
			segment.new_data_directory,
			segment.old_dbid,
			segment.new_dbid,
			false,
			mappingFilePath,
			old_bin_dir,
			new_bin_dir,
			GPDB_FIVE_PORT);

		PgUpgradeCopyOptions *segment_copy_options = make_copy_options(
			master_host_username,
			master_hostname,
			new_master_data_directory,
			old_master_gp_dbid,
			new_master_gp_dbid,
			segment.new_data_directory,
			segment.new_dbid,
			mappingFilePath);

		prepare_segment_for_upgrade(segment_copy_options);
		segment_response = perform_upgrade(segment_upgrade_options);

		_pg_upgrade_exit_status = pg_upgrade_exit_status(segment_response);

		if (_pg_upgrade_exit_status != 0)
			return;

		enable_segment_after_upgrade(segment_copy_options);
	}
}

char *
upgradeCheckOutput(void)
{
	return _pg_upgrade_output;
}

bool
upgradeReturnedSuccess(void)
{
	return _pg_upgrade_exit_status == 0;
}

void
initializePgUpgradeStatus(void)
{
	_pg_upgrade_exit_status = 0;
}

void
resetPgUpgradeStatus(void)
{
	_pg_upgrade_output = NULL;
}

void
performUpgradeCheck(void)
{
	char	   *old_master_data_directory_path = "./gpdb5-data/qddir/demoDataDir-1";
	char	   *new_master_data_directory_path = "./gpdb6-data/qddir/demoDataDir-1";
	int         old_master_gp_dbid = 1;
	int         new_master_gp_dbid = 1;

	PgUpgradeOptions *options = make_pg_upgrade_options(
		old_master_data_directory_path,
		new_master_data_directory_path,
		old_master_gp_dbid,
		new_master_gp_dbid,
		true,
		NULL,
		"./gpdb5/bin",
		"./gpdb6/bin",
		GPDB_FIVE_PORT
		);

	PgUpgradeResponse *response = perform_upgrade_check(options);
	_pg_upgrade_output = pg_upgrade_output(response);
	_pg_upgrade_exit_status = pg_upgrade_exit_status(response);
}
