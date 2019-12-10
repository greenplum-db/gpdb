#ifndef PG_UPGRADE_GREENPLUM_H
#define PG_UPGRADE_GREENPLUM_H
/*
 *	greenplum/pg_upgrade_greenplum.h
 *
 *	Portions Copyright (c) 2019-Present, Pivotal Software Inc
 *	contrib/pg_upgrade/greenplum/pg_upgrade_greenplum.h
 */


#include "pg_upgrade.h"

#define PG_OPTIONS_UTILITY_MODE " PGOPTIONS='-c gp_session_role=utility' "

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

typedef enum
{
	DISPATCHER = 0,
	SEGMENT
} segmentMode;

typedef struct {
	bool progress;
	segmentMode segment_mode;
	checksumMode checksum_mode;
	char *old_tablespace_file_path;
} GreenplumUserOpts;

#define GREENPLUM_MODE_OPTION 1
#define GREENPLUM_PROGRESS_OPTION 2
#define GREENPLUM_ADD_CHECKSUM_OPTION 3
#define GREENPLUM_REMOVE_CHECKSUM_OPTION 4
#define GREENPLUM_OLD_GP_DBID 5
#define GREENPLUM_NEW_GP_DBID 6
#define GREENPLUM_OLD_TABLESPACES_FILE 7


#define GREENPLUM_OPTIONS \
	{"mode", required_argument, NULL, GREENPLUM_MODE_OPTION}, \
	{"progress", no_argument, NULL, GREENPLUM_PROGRESS_OPTION}, \
	{"add-checksum", no_argument, NULL, GREENPLUM_ADD_CHECKSUM_OPTION}, \
	{"remove-checksum", no_argument, NULL, GREENPLUM_REMOVE_CHECKSUM_OPTION},
	{"old-gp-dbid", required_argument, NULL, 5},
	{"new-gp-dbid", required_argument, NULL, 6},
	{"old-tablespaces-file", required_argument, NULL, 7},

#define GREENPLUM_USAGE "\
      --mode=TYPE               designate node type to upgrade, \"segment\" or \"dispatcher\" (default \"segment\")\n\
      --progress                enable progress reporting\n\
      --remove-checksum         remove data checksums when creating new cluster\n\
      --add-checksum            add data checksumming to the new cluster\n\
      --old-gp-dbid             greenplum database id of the old segment\n\
      --new-gp-dbid             greenplum database id of the new segment\n\
      --old-tablespaces-file    file containing the tablespaces from an old gpdb five cluster\n\
"

/* option_gp.c */
extern GreenplumUserOpts greenplum_user_opts;
void initialize_greenplum_user_options(void);
bool process_greenplum_option(int option, char *option_value);
bool is_greenplum_dispatcher_mode(void);

/* aotable.c */

void		restore_aosegment_tables(void);

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
void new_gpdb_invalidate_bitmap_indexes(void);
Oid *get_numeric_types(PGconn *conn);
void old_GPDB5_check_for_unsupported_distribution_key_data_types(void);

/* check_gp.c */

void check_greenplum(void);

/* reporting.c */

void report_progress(ClusterInfo *cluster, progress_type op, char *fmt,...)
pg_attribute_printf(3, 4);
void close_progress(void);

#endif /* PG_UPGRADE_GREENPLUM_H */
