#include <stdlib.h>

#include "postgres_fe.h"
#include "pqexpbuffer.h"

/*
 * implements:
 */
#include "cluster-upgrade.h"

struct PgUpgradeOptionsData
{
	int old_gp_dbid;
	int new_gp_dbid;
	int old_master_port;
	char *old_segment_path;
	char *new_segment_path;
	char *old_bin_dir;
	char *new_bin_dir;
	char *mode; /* dispatcher or segment */
	char *old_tablespace_mapping_file_path;
	bool has_tablespaces;
};

struct PgUpgradeResponse
{
	int exit_code;
	char *stdout;
};

static void
log_upgrade_line(char *output)
{
	printf("%s", output);
}

static PgUpgradeResponse *
perform_upgrade_command(const char *command)
{
	PQExpBufferData stdout;
	PgUpgradeResponse *response;
	char buffer[2000];
	char *output;

	response = palloc0(sizeof(PgUpgradeResponse));
	initPQExpBuffer(&stdout);

#ifndef WIN32
	log_upgrade_line(psprintf("%s\n", command));
	FILE *output_file = popen(command, "r");
	while ((output = fgets(buffer, sizeof(buffer), output_file)) != NULL)
	{
		log_upgrade_line(output);
		appendPQExpBufferStr(&stdout, output);
	}

	response->exit_code = pclose(output_file);
	response->stdout = stdout.data;
#endif

	return response;
}

static char *
base_upgrade_executable_string(PgUpgradeOptions *options)
{
	char *tablespace_mapping_option = "";

	if (options->old_tablespace_mapping_file_path)
		tablespace_mapping_option = psprintf("--old-tablespaces-file=%s", options->old_tablespace_mapping_file_path);

	return psprintf(
		"%s/pg_upgrade "
		"--link "
		"--old-bindir=%s "
		"--new-bindir=%s "
		"--old-datadir=%s "
		"--new-datadir=%s "
		"--old-gp-dbid=%d "
		"--new-gp-dbid=%d "
		"--old-port=%d "
		"--mode=%s "
		"%s ",
		options->new_bin_dir,
		options->old_bin_dir,
		options->new_bin_dir,
		options->old_segment_path,
		options->new_segment_path,
		options->old_gp_dbid,
		options->new_gp_dbid,
		options->old_master_port,
		options->mode,
		tablespace_mapping_option);
}

PgUpgradeOptions *
make_pg_upgrade_options(
	char *old_segment_path,
	char *new_segment_path,
	int old_gp_dbid,
	int new_gp_dbid,
	bool is_dispatcher,
	char *old_tablespace_mapping_file_path,
	char *old_bin_dir,
	char *new_bin_dir,
	int old_master_port)
{
	char *mode = "segment";

	if (is_dispatcher)
		mode = "dispatcher";

	PgUpgradeOptions *options = palloc0(sizeof(PgUpgradeOptions));
	options->old_gp_dbid = old_gp_dbid;
	options->new_gp_dbid = new_gp_dbid;
	options->old_segment_path = old_segment_path;
	options->new_segment_path = new_segment_path;
	options->old_bin_dir = old_bin_dir;
	options->new_bin_dir = new_bin_dir;
	options->mode = mode;
	options->old_tablespace_mapping_file_path = old_tablespace_mapping_file_path;
	options->has_tablespaces = old_tablespace_mapping_file_path != NULL;
	options->old_master_port = old_master_port;
	return options;
}

PgUpgradeResponse *
perform_upgrade(PgUpgradeOptions *options)
{
	return perform_upgrade_command(
		base_upgrade_executable_string(options));
}

int
pg_upgrade_exit_status(PgUpgradeResponse *status)
{
	return status->exit_code;
}

char *
pg_upgrade_output(PgUpgradeResponse *status)
{
	return status->stdout;
}

PgUpgradeResponse *
perform_upgrade_check(PgUpgradeOptions *options)
{
	return perform_upgrade_command(psprintf(
		"%s "
		"--check ",
		base_upgrade_executable_string(options)));
}
