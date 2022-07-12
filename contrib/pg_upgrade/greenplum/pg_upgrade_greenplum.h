#ifndef PG_UPGRADE_GREENPLUM_H
#define PG_UPGRADE_GREENPLUM_H
/*
 *	greenplum/pg_upgrade_greenplum.h
 *
 *	Portions Copyright (c) 2019-Present, Pivotal Software Inc
 *	contrib/pg_upgrade/greenplum/pg_upgrade_greenplum.h
 */


#include "pg_upgrade.h"
#include <portability/instr_time.h>


#define PG_OPTIONS_UTILITY_MODE " PGOPTIONS='-c gp_session_role=utility' "

typedef struct {
	instr_time start_time;
	instr_time end_time;
} step_timer;

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

typedef enum
{
	CHECKSUM_NONE = 0,
	CHECKSUM_ADD,
	CHECKSUM_REMOVE
} checksumMode;

typedef enum {
	GREENPLUM_MODE_OPTION = 1,
	GREENPLUM_PROGRESS_OPTION = 2,
	GREENPLUM_ADD_CHECKSUM_OPTION = 3,
	GREENPLUM_REMOVE_CHECKSUM_OPTION = 4,
	GREENPLUM_OLD_GP_DBID = 5,
	GREENPLUM_NEW_GP_DBID = 6,
	GREENPLUM_OLD_TABLESPACES_FILE = 7,
	GREENPLUM_CONTINUE_CHECK_ON_FATAL = 8,
	GREENPLUM_SKIP_TARGET_CHECK = 9
} greenplumOption;


#define GREENPLUM_OPTIONS \
	{"mode", required_argument, NULL, GREENPLUM_MODE_OPTION}, \
	{"progress", no_argument, NULL, GREENPLUM_PROGRESS_OPTION}, \
	{"add-checksum", no_argument, NULL, GREENPLUM_ADD_CHECKSUM_OPTION}, \
	{"remove-checksum", no_argument, NULL, GREENPLUM_REMOVE_CHECKSUM_OPTION}, \
	{"old-gp-dbid", required_argument, NULL, GREENPLUM_OLD_GP_DBID}, \
	{"new-gp-dbid", required_argument, NULL, GREENPLUM_NEW_GP_DBID}, \
	{"old-tablespaces-file", required_argument, NULL, GREENPLUM_OLD_TABLESPACES_FILE}, \
	{"continue-check-on-fatal", no_argument, NULL, GREENPLUM_CONTINUE_CHECK_ON_FATAL}, \
	{"skip-target-check", no_argument, NULL, GREENPLUM_SKIP_TARGET_CHECK},

#define GREENPLUM_USAGE "\
	--mode=TYPE               designate node type to upgrade, \"segment\" or \"dispatcher\" (default \"segment\")\n\
	--progress                enable progress reporting\n\
	--remove-checksum         remove data checksums when creating new cluster\n\
	--add-checksum            add data checksumming to the new cluster\n\
	--old-gp-dbid             greenplum database id of the old segment\n\
	--new-gp-dbid             greenplum database id of the new segment\n\
	--old-tablespaces-file    file containing the tablespaces from an old gpdb five cluster\n\
	--continue-check-on-fatal continue to run through all pg_upgrade checks without upgrade. Stops on major issues\n\
	--skip-target-check       skip all checks and comparisons of new cluster\n\
"

/* option_gp.c */
void initialize_greenplum_user_options(void);
bool process_greenplum_option(greenplumOption option);
bool is_greenplum_dispatcher_mode(void);
bool is_checksum_mode(checksumMode mode);
bool is_show_progress_mode(void);
void validate_greenplum_options(void);
bool is_continue_check_on_fatal(void);
void set_check_fatal_occured(void);
bool get_check_fatal_occurred(void);
bool is_skip_target_check(void);

/* pg_upgrade_greenplum.c */
void freeze_all_databases(void);
void reset_system_identifier(void);

/* frozenxids_gp.c */
void update_segment_db_xids(void);

/* aotable.c */

void		restore_aosegment_tables(void);
bool        is_appendonly(char relstorage);


/* gpdb4_heap_convert.c */

const char *convert_gpdb4_heap_file(const char *src, const char *dst,
                                    bool has_numerics, AttInfo *atts, int natts);
void		finish_gpdb4_page_converter(void);

/* file_gp.c */

const char * rewriteHeapPageChecksum( const char *fromfile, const char *tofile,
                                      const char *schemaName, const char *relName);

/* version_gp.c */

void old_GPDB4_check_for_money_data_type_usage(void);
void old_GPDB4_check_no_free_aoseg(void);
void check_hash_partition_usage(void);
void new_gpdb5_0_invalidate_indexes(void);
Oid *get_numeric_types(PGconn *conn);
void old_GPDB5_check_for_unsupported_distribution_key_data_types(void);
void invalidate_indexes(void);
void reset_invalid_indexes(void);

/* check_gp.c */

void check_greenplum(void);

/* reporting.c */

void report_progress(ClusterInfo *cluster, progress_type op, char *fmt,...)
pg_attribute_printf(3, 4);
void close_progress(void);
void log_with_timing(step_timer *timer, const char *msg);
void duration(instr_time duration, char *buf, size_t len);

/* tablespace_gp.c */

void generate_old_tablespaces_file(ClusterInfo *oldCluster);
void populate_gpdb6_cluster_tablespace_suffix(ClusterInfo *cluster);
bool is_gpdb_version_with_filespaces(ClusterInfo *cluster);
void populate_os_info_with_file_contents(void);

/* server_gp.c */
char *greenplum_extra_pg_ctl_flags(GreenplumClusterInfo *info);

static inline bool
is_gpdb6(ClusterInfo *cluster)
{
	return GET_MAJOR_VERSION(cluster->major_version) == 904;
}

extern void compute_old_cluster_chkpnt_oldstxid(void);

#endif /* PG_UPGRADE_GREENPLUM_H */
