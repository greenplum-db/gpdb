typedef struct PgUpgradeOptionsData PgUpgradeOptions;

PgUpgradeOptions *make_pg_upgrade_options(
	char *old_segment_path,
	char *new_segment_path,
	int old_gp_dbid,
	int new_gp_dbid,
	bool is_dispatcher,
	char *old_tablespace_mapping_file_path,
	char *old_bin_dir,
	char *new_bin_dir);

void perform_upgrade(PgUpgradeOptions *options);

/*
 * Returns file handler to standard out of pg_upgrade --check
 */
FILE *perform_upgrade_check(PgUpgradeOptions *options);