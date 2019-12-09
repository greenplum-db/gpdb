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

typedef struct {
	bool progress;
	segmentMode segment_mode;
	checksumMode checksum_mode;
} GreenplumUserOpts;

extern GreenplumUserOpts greenplum_user_opts;

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
