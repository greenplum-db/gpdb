#include "postgres_fe.h"
#include "greenplum/old_tablespace_file_contents.h"

/* 
 * Implements
 */
#include "pg-upgrade-copy.h"

struct PgUpgradeCopyOptionsData {
	char *master_host_username;
	char *master_hostname;
	char *master_data_directory_path;
	char *new_segment_path;
	int old_master_gp_dbid;
	int new_master_gp_dbid;
	int new_gp_dbid;
	char *old_tablespace_mapping_file_path;
	bool has_tablespaces;
};

static int number_of_config_files_to_preserve = 5;
static char *config_files_to_preserve[] = {
	"internal.auto.conf",
	"postgresql.conf",
	"pg_hba.conf",
	"postmaster.opts",
	"postgresql.auto.conf"
};

static void
copy_configuration_file_to_directory(char *base_directory, char *file_name, char *destination_directory){
	system(psprintf("cp %s/%s %s/", base_directory, file_name, destination_directory));
}

static void
restore_configuration_files(PgUpgradeCopyOptions *copy_options)
{
	char *backup_configuration_directory = psprintf("%s/backup-configuration", copy_options->new_segment_path);

	for (int i = 0; i < number_of_config_files_to_preserve; i++)
	{
		char *filename = config_files_to_preserve[i];
		copy_configuration_file_to_directory(backup_configuration_directory, filename, copy_options->new_segment_path);
	}
}

static void
copy_tablespace_from(
	char *master_tablespace_location_directory,
	char *segment_tablespace_location_directory,
	PgUpgradeCopyOptions *copy_options)
{
	system(psprintf(
		"rsync -a --delete "
		"%s@%s:%s/%d/ "
		"%s/%d ",
		copy_options->master_host_username,
		copy_options->master_hostname,
		master_tablespace_location_directory,
		copy_options->new_master_gp_dbid,
		segment_tablespace_location_directory,
		copy_options->new_gp_dbid));
}

static void
copy_master_data_directory_into_segment_data_directory(PgUpgradeCopyOptions *options)
{
	system(psprintf(
		"rsync -a --delete --exclude='backup-configuration' "
		"%s@%s:%s/ "
		"%s ",
		options->master_host_username,
		options->master_hostname,
		options->master_data_directory_path,
		options->new_segment_path));
}

static void
backup_configuration_files(PgUpgradeCopyOptions *options)
{
	char *backup_directory = psprintf("%s/backup-configuration", options->new_segment_path);

	system(psprintf("mkdir -p %s", backup_directory));

	for (int i = 0; i < number_of_config_files_to_preserve; i++)
	{
		char *filename = config_files_to_preserve[i];
		copy_configuration_file_to_directory(
			options->new_segment_path,
			filename,
			backup_directory);
	}
}

static void
update_symlinks_for_tablespaces_from(char *segment_path, Oid tablespace_oid, char *new_tablespace_path)
{
	system(psprintf("find %s/pg_tblspc/%u | xargs -I '{}' ln -sfn %s '{}'",
	                segment_path,
	                tablespace_oid,
	                new_tablespace_path));
}

static void
copy_tablespaces_from_the_master(PgUpgradeCopyOptions *copy_options)
{
	if (!copy_options->has_tablespaces) return;

	OldTablespaceFileContents *contents = parse_old_tablespace_file_contents(
		copy_options->old_tablespace_mapping_file_path);

	OldTablespaceFileContents *contents_for_segment = filter_old_tablespace_file_for_dbid(
		contents, copy_options->new_gp_dbid);

	OldTablespaceRecord **segment_records = OldTablespaceFileContents_GetTablespaceRecords(contents_for_segment);

	for (int i = 0; i < OldTablespaceFileContents_TotalNumberOfTablespaces(contents_for_segment); i++)
	{
		OldTablespaceRecord *current_segment_record = segment_records[i];
		OldTablespaceRecord *master_segment_record = OldTablespaceFileContents_GetTablespaceRecord(
			contents,
			copy_options->old_master_gp_dbid,
			OldTablespaceRecord_GetTablespaceName(current_segment_record)
			);

		char *master_tablespace_location_directory = OldTablespaceRecord_GetDirectoryPath(master_segment_record);
		char *segment_tablespace_location_directory = OldTablespaceRecord_GetDirectoryPath(current_segment_record);

		if (OldTablespaceRecord_GetIsUserDefinedTablespace(current_segment_record))
			copy_tablespace_from(
				master_tablespace_location_directory,
				segment_tablespace_location_directory,
				copy_options);

		char *segment_tablespace_location_directory_with_gp_dbid = psprintf("%s/%d",
			segment_tablespace_location_directory,
			copy_options->new_gp_dbid);

		update_symlinks_for_tablespaces_from(
			copy_options->new_segment_path,
			OldTablespaceRecord_GetOid(current_segment_record),
			segment_tablespace_location_directory_with_gp_dbid);
	}
}


PgUpgradeCopyOptions *
make_copy_options(
	char *master_host_username,
	char *master_hostname,
	char *master_data_directory,
	int old_master_gp_dbid,
	int new_master_gp_dbid,
	char *new_segment_path,
	int new_gp_dbid,
	char *old_tablespace_mapping_file_path)
{
	PgUpgradeCopyOptions *copy_options = palloc0(sizeof(PgUpgradeCopyOptions));
	copy_options->master_host_username = master_host_username;
	copy_options->master_hostname = master_hostname;
	copy_options->master_data_directory_path = master_data_directory;
	copy_options->new_segment_path = new_segment_path;
	copy_options->old_master_gp_dbid = old_master_gp_dbid;
	copy_options->new_master_gp_dbid = new_master_gp_dbid;
	copy_options->new_gp_dbid = new_gp_dbid;
	copy_options->old_tablespace_mapping_file_path = old_tablespace_mapping_file_path;
	copy_options->has_tablespaces = old_tablespace_mapping_file_path != NULL;
	return copy_options;
}

void 
prepare_segment_for_upgrade(PgUpgradeCopyOptions *copy_options)
{
	backup_configuration_files(copy_options);
	copy_master_data_directory_into_segment_data_directory(copy_options);
	copy_tablespaces_from_the_master(copy_options);
}

void
enable_segment_after_upgrade(PgUpgradeCopyOptions *copy_options)
{
	restore_configuration_files(copy_options);
}
