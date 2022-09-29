#ifndef PG_UPGRADE_GREENPLUM_H
#define PG_UPGRADE_GREENPLUM_H
/*
 *	greenplum/pg_upgrade_greenplum.h
 *
 *	Portions Copyright (c) 2019-Present, VMware, Inc. or its affiliates
 *	src/bin/pg_upgrade/greenplum/pg_upgrade_greenplum.h
 */


#include "pg_upgrade.h"


#define PG_OPTIONS_UTILITY_MODE_VERSION(major_version) \
	( (GET_MAJOR_VERSION(major_version)) < 1200 ?      \
		" PGOPTIONS='-c gp_session_role=utility' " :   \
		" PGOPTIONS='-c gp_role=utility' ")

/*
 * Enumeration for operations in the progress report
 */
typedef enum
{
	CHECK,
	SCHEMA_DUMP,
	SCHEMA_RESTORE,
	FILE_MAP,
	FILE_COPY,
	FIXUP,
	ABORT,
	DONE
} progress_type;

typedef enum {
	GREENPLUM_MODE_OPTION = 10,
	GREENPLUM_PROGRESS_OPTION = 11,
	GREENPLUM_CONTINUE_CHECK_ON_FATAL = 12,
	GREENPLUM_SKIP_TARGET_CHECK = 13
} greenplumOption;

#define GREENPLUM_OPTIONS \
	{"mode", required_argument, NULL, GREENPLUM_MODE_OPTION}, \
	{"progress", no_argument, NULL, GREENPLUM_PROGRESS_OPTION}, \
	{"continue-check-on-fatal", no_argument, NULL, GREENPLUM_CONTINUE_CHECK_ON_FATAL}, \
	{"skip-target-check", no_argument, NULL, GREENPLUM_SKIP_TARGET_CHECK},

#define GREENPLUM_USAGE "\
      --mode=TYPE               designate node type to upgrade, \"segment\" or \"dispatcher\" (default \"segment\")\n\
      --progress                enable progress reporting\n\
      --continue-check-on-fatal continue to run through all pg_upgrade checks without upgrade. Stops on major issues\n\
      --skip-target-check       skip all checks on new/target cluster\n\
"

/* option_gp.c */
void initialize_greenplum_user_options(void);
bool process_greenplum_option(greenplumOption option);
bool is_greenplum_dispatcher_mode(void);
bool is_show_progress_mode(void);
bool is_continue_check_on_fatal(void);
void set_check_fatal_occured(void);
bool get_check_fatal_occurred(void);
bool is_skip_target_check(void);

/* pg_upgrade_greenplum.c */
void freeze_master_data(void);
void reset_system_identifier(void);


/* aotable.c */

void		restore_aosegment_tables(void);
bool        is_appendonly(char relstorage);


/* version_gp.c */

void check_hash_partition_usage(void);
void old_GPDB5_check_for_unsupported_distribution_key_data_types(void);
void old_GPDB6_check_for_unsupported_sha256_password_hashes(void);
void new_gpdb_invalidate_bitmap_indexes(void);

/* check_gp.c */

void check_greenplum(void);

/* reporting.c */

void report_progress(ClusterInfo *cluster, progress_type op, char *fmt,...)
pg_attribute_printf(3, 4);
void close_progress(void);

#endif /* PG_UPGRADE_GREENPLUM_H */
