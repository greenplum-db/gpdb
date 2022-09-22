#include "postgres_fe.h"
#include "pg_upgrade_greenplum.h"
#include "tablespace_gp_internal.h"
#include "greenplum_cluster_info_internal.h"

typedef enum
{
	DISPATCHER = 0,
	SEGMENT
} segmentMode;

typedef struct {
	bool progress;
	segmentMode segment_mode;
	char *old_tablespace_file_path;
	bool continue_check_on_fatal;
	bool skip_target_check;
} GreenplumUserOpts;

static GreenplumUserOpts greenplum_user_opts;
static bool check_fatal_occurred;

void
initialize_greenplum_user_options(void)
{
	greenplum_user_opts.segment_mode = SEGMENT;
	greenplum_user_opts.old_tablespace_file_path = NULL;

	old_cluster.greenplum_cluster_info = make_cluster_info();
	new_cluster.greenplum_cluster_info = make_cluster_info();
	greenplum_user_opts.continue_check_on_fatal = false;
	greenplum_user_opts.skip_target_check = false;
}

bool
process_greenplum_option(greenplumOption option)
{
	switch (option)
	{
		case GREENPLUM_MODE_OPTION:        /* --mode={dispatcher|segment} */
			if (pg_strcasecmp("dispatcher", optarg) == 0)
				greenplum_user_opts.segment_mode = DISPATCHER;
			else if (pg_strcasecmp("segment", optarg) == 0)
				greenplum_user_opts.segment_mode = SEGMENT;
			else
			{
				pg_log(PG_FATAL, "invalid segment configuration\n");
				exit(1);
			}
			break;

		case GREENPLUM_PROGRESS_OPTION:        /* --progress */
			greenplum_user_opts.progress = true;
			break;

		case GREENPLUM_OLD_GP_DBID: /* --old-gp-dbid */
			set_gp_dbid(old_cluster.greenplum_cluster_info, atoi(optarg));
			break;

		case GREENPLUM_NEW_GP_DBID: /* --new-gp-dbid */
			set_gp_dbid(new_cluster.greenplum_cluster_info, atoi(optarg));
			break;

		case GREENPLUM_OLD_TABLESPACES_FILE: /* --old-tablespaces-file */
			greenplum_user_opts.old_tablespace_file_path = pg_strdup(optarg);
			break;

		case GREENPLUM_CONTINUE_CHECK_ON_FATAL:
			if (user_opts.check)
			{
				greenplum_user_opts.continue_check_on_fatal = true;
				check_fatal_occurred = false;
			}
			else
			{
				pg_log(PG_FATAL,
					"--continue-check-on-fatal: should be used with check mode (-c)\n");
				exit(1);
			}
			break;

		case GREENPLUM_SKIP_TARGET_CHECK:
			if (user_opts.check)
				greenplum_user_opts.skip_target_check = true;
			else
			{
				pg_log(PG_FATAL,
					"--skip-target-check: should be used with check mode (-c)\n");
				exit(1);
			}
			break;

		default:
			return false;
	}

	return true;
}

void
validate_greenplum_options(void)
{

	if (!is_gp_dbid_set(old_cluster.greenplum_cluster_info))
		pg_fatal("--old-gp-dbid must be set\n");

	if (!is_gp_dbid_set(new_cluster.greenplum_cluster_info) && !is_skip_target_check())
		pg_fatal("--new-gp-dbid must be set\n");

	if (greenplum_user_opts.old_tablespace_file_path) {
		populate_old_cluster_with_old_tablespaces(
			&old_cluster,
			greenplum_user_opts.old_tablespace_file_path);
	}
}

bool
is_greenplum_dispatcher_mode()
{
	return greenplum_user_opts.segment_mode == DISPATCHER;
}

bool
is_show_progress_mode(void)
{
	return greenplum_user_opts.progress;
}

bool
is_continue_check_on_fatal(void)
{
	return greenplum_user_opts.continue_check_on_fatal;
}

void
set_check_fatal_occured(void)
{
	check_fatal_occurred = true;
}

bool
get_check_fatal_occurred(void)
{
	return check_fatal_occurred;
}

bool
is_skip_target_check(void)
{
	return greenplum_user_opts.skip_target_check;
}
