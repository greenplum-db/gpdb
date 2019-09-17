#include "stdlib.h"

/*
 * implements:
 */
#include "upgrade-helpers.h"


static void
copy_file_from_backup_to_datadir(char *filename, char *segment_path)
{
	char		buffer[2000];

	sprintf(buffer,
			"cp gpdb6-data-copy/%s/%s gpdb6-data/%s/%s",
			segment_path, filename, segment_path, filename);

	system(buffer);
}

static void
copy_configuration_files_from_backup_to_datadirs(char *segment_path)
{
	char	   *files_to_copy[] = {
		"internal.auto.conf",
		"postgresql.conf",
		"pg_hba.conf",
		"postmaster.opts",
		"postgresql.auto.conf"
	};

	for (int i = 0; i < 5; i++)
	{
		char	   *filename = files_to_copy[i];

		copy_file_from_backup_to_datadir(filename, segment_path);
	}
}

static void
execute_pg_upgrade_for_qd(char *segment_path)
{
	char		buffer[2000];

	sprintf(buffer, ""
			"./gpdb6/bin/pg_upgrade "
			"--mode=dispatcher "
			"--link "
			"--old-bindir=./gpdb5/bin "
			"--new-bindir=./gpdb6/bin "
			"--old-datadir=./gpdb5-data/%s "
			"--new-datadir=./gpdb6-data/%s "
			,segment_path, segment_path);

	system(buffer);
}

static void
execute_pg_upgrade_for_primary(char *segment_path)
{
	char		buffer[2000];

	sprintf(buffer, ""
			"./gpdb6/bin/pg_upgrade "
			"--mode=segment "
			"--link "
			"--old-bindir=./gpdb5/bin "
			"--new-bindir=./gpdb6/bin "
			"--old-datadir=./gpdb5-data/%s "
			"--new-datadir=./gpdb6-data/%s "
			,segment_path, segment_path);

	system(buffer);
}

static void
copy_master_data_directory_into_segment_data_directory(char *segment_path)
{
	char		buffer[2000];
	char	   *master_data_directory_path = "qddir/demoDataDir-1";

	sprintf(buffer,
			"rsync -a --delete "
			"./gpdb6-data/%s/ "
			"./gpdb6-data/%s ",
			master_data_directory_path,
			segment_path);

	system(buffer);
}

static void
upgradeSegment(char *segment_path)
{
	copy_master_data_directory_into_segment_data_directory(segment_path);
	execute_pg_upgrade_for_primary(segment_path);
	copy_configuration_files_from_backup_to_datadirs(
													 segment_path);
}

static void
upgradeMaster(void)
{
	char	   *master_data_directory_path = "qddir/demoDataDir-1";

	execute_pg_upgrade_for_qd(master_data_directory_path);

	copy_configuration_files_from_backup_to_datadirs(
													 master_data_directory_path);
}

static void
upgradeContentId0(void)
{
	char	   *segment_path = "dbfast1/demoDataDir0";

	upgradeSegment(segment_path);
}

static void
upgradeContentId1(void)
{
	char	   *segment_path = "dbfast2/demoDataDir1";

	upgradeSegment(segment_path);
}

static void
upgradeContentId2(void)
{
	char	   *segment_path = "dbfast3/demoDataDir2";

	upgradeSegment(segment_path);
}

void
performUpgrade(void)
{
	upgradeMaster();
	upgradeContentId0();
	upgradeContentId1();
	upgradeContentId2();
}
