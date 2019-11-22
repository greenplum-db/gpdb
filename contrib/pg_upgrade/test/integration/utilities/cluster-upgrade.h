typedef struct PgUpgradeOptionsData PgUpgradeOptions;
typedef struct PgUpgradeResponse PgUpgradeResponse;

PgUpgradeOptions *make_pg_upgrade_options(
	char *old_segment_path,
	char *new_segment_path,
	int old_gp_dbid,
	int new_gp_dbid,
	bool is_dispatcher,
	char *old_tablespace_mapping_file_path,
	char *old_bin_dir,
	char *new_bin_dir,
	int old_master_port);

PgUpgradeResponse *perform_upgrade(PgUpgradeOptions *options);
int pg_upgrade_exit_status(PgUpgradeResponse *status);
char *pg_upgrade_output(PgUpgradeResponse *status);

/*
 * Returns file handler to standard out of pg_upgrade --check
 */
PgUpgradeResponse *perform_upgrade_check(PgUpgradeOptions *options);