#ifndef PG_UPGRADE_COPY_H
#define PG_UPGRADE_COPY_H

typedef struct PgUpgradeCopyOptionsData PgUpgradeCopyOptions;

PgUpgradeCopyOptions *make_copy_options(
	char *master_host_username,
	char *master_hostname,
	char *master_data_directory,
	int old_master_gp_dbid,
	int new_master_gp_dbid,
	char *new_segment_path,
	int new_gp_dbid,
	char *old_tablespace_mapping_file_path);

void prepare_segment_for_upgrade(PgUpgradeCopyOptions *copy_options);
void enable_segment_after_upgrade(PgUpgradeCopyOptions *copy_options);

#endif /* PG_UPGRADE_COPY_H */
