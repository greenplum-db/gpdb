#ifndef GPMONDB_H
#define GPMONDB_H

#include "libpq-fe.h"
#include "apr_general.h"
#include "apr_md5.h"
#include "apr_hash.h"

struct hostinfo_holder_t
{
	addressinfo_holder_t* addressinfo_head;
	addressinfo_holder_t* addressinfo_tail;
	apr_uint32_t address_count;

	char* datadir;
	char* smon_dir;
	char* hostname;
	int is_master;
	int is_hdm;
	int is_hdw;
	int is_hbw;
	int is_hdc;
	int is_etl;
};

APR_DECLARE(int) gpdb_exec_search_for_at_least_one_row(const char* QUERY, PGconn* persistant_conn);

/**
 * Validate the the gpperfmon database is correct and
 * gpmon user has correct access.
 */
APR_DECLARE(int) gpdb_validate_gpperfmon(void);

/**
 * Check if gpperfmon database exists.
 */
APR_DECLARE(int) gpdb_gpperfmon_db_exists(void);

/**
 * Check if gpmon user has access to ext tables
 */
APR_DECLARE(int) gpdb_validate_ext_table_access(void);

/**
 * Check if perfmon is enabled
 */
APR_DECLARE(int) gpdb_gpperfmon_enabled(void);

/**
 *  Retrieve the gpmon_port from GPDB. (SHOW GPMON_PORT)
 */
APR_DECLARE(int) gpdb_get_gpmon_port(void);

/**
 *  Retrieve a list of all hosts in the GPDB.
 *  @param hostcnt return # elements in hostvec
 *  @param hostvec return array of hostnames.
 *  @param pool where to allocate hostvec and its contents.
 */
APR_DECLARE(void) gpdb_get_hostlist(int* hostcnt, host_t** host_table, apr_pool_t* global_pool, mmon_options_t* opt);

/**
 *  Get the master data directory in the GPDB.
 *  @param mstrdir return the master data directory
 *  @param hostname return the master hostname
 *  @param pool where to allocate hostname and mstrdir
 */
APR_DECLARE(void) gpdb_get_master_data_dir(char** hostname, char** mstrdir, apr_pool_t* pool);

/**
 * Find out all read only alert tail files, which don't include the one being written by
 * gpdb currently. Merge them into stage file, and load into log_alert_history table,
 * and remove them when successful.
 */
APR_DECLARE(void) gpdb_import_alert_log(apr_pool_t* pool);

/**
 * check if new historical partitions are required and create them
 */
APR_DECLARE(apr_status_t) gpdb_check_partitions(mmon_options_t *opt);

/**
 * insert _tail data into history table
 */
APR_DECLARE(apr_status_t) gpdb_harvest(void);

/**
 * truncate _tail files to clear data already loaded into the DB
 */
APR_DECLARE( apr_status_t) gpdb_empty_harvest_files(apr_pool_t* pool);

/**
 * rename tail to stage files to allow continuous reading and allow new data to go into tail files
 */
APR_DECLARE (apr_status_t) gpdb_rename_tail_files(apr_pool_t* pool);

/**
 * add the data from the stage files into the harvest (_tail) files for loading into history
 */
APR_DECLARE (apr_status_t) gpdb_copy_stage_to_harvest_files(apr_pool_t* pool);

/**
 * restart with empty tail files
 */
APR_DECLARE (apr_status_t) gpdb_truncate_tail_files(apr_pool_t* pool);

APR_DECLARE (apr_status_t) gpdb_harvest_one(const char* table);

APR_DECLARE (apr_status_t) gpdb_harvest_healthdata(void);

APR_DECLARE (apr_status_t) remove_segid_constraint(void);

APR_DECLARE (apr_hash_t *) get_active_queries(apr_pool_t* pool);

APR_DECLARE (void) create_log_alert_table(void);

#endif /* GPMONDB_H */

