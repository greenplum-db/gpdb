#ifndef PG_UPGRADE_GREENPLUM_H
#define PG_UPGRADE_GREENPLUM_H
/*
 *	greenplum/pg_upgrade_greenplum.h
 *
 *	Portions Copyright (c) 2019-Present, VMware, Inc. or its affiliates
 *	src/bin/pg_upgrade/greenplum/pg_upgrade_greenplum.h
 */


#include "pg_upgrade.h"


#define PG_OPTIONS_UTILITY_MODE " PGOPTIONS='-c gp_role=utility' "


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
	GREENPLUM_MODE_OPTION = 10,
	GREENPLUM_PROGRESS_OPTION = 11,
	GREENPLUM_ADD_CHECKSUM_OPTION = 12,
	GREENPLUM_REMOVE_CHECKSUM_OPTION = 13
} greenplumOption;

#define GREENPLUM_OPTIONS \
	{"mode", required_argument, NULL, GREENPLUM_MODE_OPTION}, \
	{"progress", no_argument, NULL, GREENPLUM_PROGRESS_OPTION}, \
	{"add-checksum", no_argument, NULL, GREENPLUM_ADD_CHECKSUM_OPTION}, \
	{"remove-checksum", no_argument, NULL, GREENPLUM_REMOVE_CHECKSUM_OPTION},

#define GREENPLUM_USAGE "\
      --mode=TYPE               designate node type to upgrade, \"segment\" or \"dispatcher\" (default \"segment\")\n\
      --progress                enable progress reporting\n\
      --remove-checksum         remove data checksums when creating new cluster\n\
      --add-checksum            add data checksumming to the new cluster\n\
"

/* option_gp.c */
void initialize_greenplum_user_options(void);
bool process_greenplum_option(greenplumOption option, char *option_value);
bool is_greenplum_dispatcher_mode(void);
bool is_checksum_mode(checksumMode mode);
bool is_show_progress_mode(void);

/* pg_upgrade_greenplum.c */
void freeze_master_data(void);
void reset_system_identifier(void);


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
void old_GPDB6_check_for_unsupported_sha256_password_hashes(void);
void after_create_new_objects_greenplum(void);

/* check_gp.c */

void check_greenplum(void);

/* reporting.c */

void report_progress(ClusterInfo *cluster, progress_type op, char *fmt,...)
pg_attribute_printf(3, 4);
void close_progress(void);

#endif /* PG_UPGRADE_GREENPLUM_H */
