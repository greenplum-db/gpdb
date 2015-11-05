/*--------------------------------------------------------------------
 * guc.c
 *
 * Support for grand unified configuration scheme, including SET
 * command, configuration file, and command line options.
 * See src/backend/utils/misc/README for more information.
 *
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Copyright (c) 2000-2009, PostgreSQL Global Development Group
 * Written by Peter Eisentraut <peter_e@gmx.net>.
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/utils/misc/guc.c,v 1.360.2.6 2010/01/24 21:49:48 tgl Exp $
 *
 *--------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef HAVE_SYSLOG
#include <syslog.h>
#endif

#include <signal.h>

#include "access/gin.h"
#include "access/transam.h"
#include "access/aosegfiles.h"
#include "access/reloptions.h"
#include "access/twophase.h"
#include "access/xact.h"
#include "access/xlog_internal.h"
#include "access/transam.h"
#include "access/url.h"
#include "catalog/namespace.h"
#include "catalog/pg_type.h"
#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbfilerep.h"
#include "cdb/cdbsreh.h"
#include "cdb/tupchunk.h"
#include "commands/async.h"
#include "commands/vacuum.h"
#include "commands/variable.h"
#include "executor/executor.h"          /* TupOutputState, TupleDesc */
#include "executor/execWorkfile.h"
#include "funcapi.h"
#include "libpq/auth.h"
#include "libpq/password_hash.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "optimizer/cost.h"
#include "optimizer/paths.h"
#include "optimizer/planmain.h"
#include "parser/gramparse.h"
#include "parser/parse_expr.h"
#include "parser/parse_relation.h"
#include "parser/scansup.h"
#include "pgstat.h"
#include "postmaster/autovacuum.h"
#include "postmaster/bgwriter.h"
#include "postmaster/checkpoint.h"
#include "postmaster/postmaster.h"
#include "postmaster/syslogger.h"
#include "postmaster/backoff.h"
#include "replication/walsender.h"
#include "replication/walreceiver.h"
#include "storage/fd.h"
#include "storage/freespace.h"
#include "storage/bfz.h"
#include "tcop/tcopprot.h"
#include "utils/builtins.h"
#include "utils/guc_tables.h"
#include "utils/inval.h"
#include "utils/memutils.h"
#include "utils/pg_locale.h"
#include "utils/ps_status.h"
#include "utils/tzparser.h"
#include "utils/resscheduler.h"
#include "pgstat.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbdisp.h"
#include "cdb/cdbappendonlyblockdirectory.h"
#include "cdb/memquota.h"
#include "utils/vmem_tracker.h"

#ifdef USE_SSL
#include <openssl/crypto.h>
#include <openssl/err.h>
#endif

#ifdef USE_CONNECTEMC
#include "emcconnect/api.h"
#endif

#ifndef PG_KRB_SRVTAB
#define PG_KRB_SRVTAB ""
#endif
#ifndef PG_KRB_SRVNAM
#define PG_KRB_SRVNAM ""
#endif

#define CONFIG_FILENAME "postgresql.conf"
#define HBA_FILENAME	"pg_hba.conf"
#define IDENT_FILENAME	"pg_ident.conf"

#ifdef EXEC_BACKEND
#define CONFIG_EXEC_PARAMS "global/config_exec_params"
#define CONFIG_EXEC_PARAMS_NEW "global/config_exec_params.new"
#endif

/* upper limit for GUC variables measured in kilobytes of memory */
#if SIZEOF_SIZE_T > 4
#define MAX_KILOBYTES	INT_MAX
#else
#define MAX_KILOBYTES	(INT_MAX / 1024)
#endif

#define KB_PER_MB (1024)
#define KB_PER_GB (1024*1024)

#define MS_PER_S 1000
#define S_PER_MIN 60
#define MS_PER_MIN (1000 * 60)
#define MIN_PER_H 60
#define S_PER_H (60 * 60)
#define MS_PER_H (1000 * 60 * 60)
#define MIN_PER_D (60 * 24)
#define S_PER_D (60 * 60 * 24)
#define MS_PER_D (1000 * 60 * 60 * 24)

/* XXX these should appear in other modules' header files */
extern bool Log_disconnections;
extern int	CommitDelay;
extern int	CommitSiblings;
extern char *default_tablespace;
extern bool fullPageWrites;

extern bool enable_partition_rules;

extern bool gp_hash_index;

/* GUC lists for gp_guc_list_show().  (List of struct config_generic) */
List       *gp_guc_list_for_explain;
List       *gp_guc_list_for_no_plan;


#ifdef USE_SSL
extern char *SSLCipherSuites;
#endif
/*
 * Assign/Show hook functions defined in this module
 */
static const char *assign_hashagg_compress_spill_files(const char *newval, bool doit, GucSource source);
static const char *assign_gp_workfile_compress_algorithm(const char *newval, bool doit, GucSource source);
static const char *assign_gp_workfile_type_hashjoin(const char *newval, bool doit, GucSource source);
static const char *assign_log_destination(const char *value,
					   bool doit, GucSource source);

#ifdef HAVE_SYSLOG
static int	syslog_facility = LOG_LOCAL0;

static const char *assign_syslog_facility(const char *facility,
					   bool doit, GucSource source);
static const char *assign_syslog_ident(const char *ident,
					bool doit, GucSource source);
#endif

static const char *assign_defaultxactisolevel(const char *newval, bool doit,
						   GucSource source);
static const char *assign_debug_persistent_print_level(const char *newval,
						bool doit, GucSource source);
static const char *assign_debug_persistent_recovery_print_level(const char *newval,
						bool doit, GucSource source);
static const char *assign_debug_persistent_store_print_level(const char *newval,
						bool doit, GucSource source);
static const char *assign_debug_database_command_print_level(const char *newval,
						bool doit, GucSource source);
static const char *assign_log_min_messages(const char *newval, bool doit,
						GucSource source);
static const char *assign_client_min_messages(const char *newval,
						   bool doit, GucSource source);
static const char *assign_optimizer_log_failure(const char *newval,
							bool doit, GucSource source);
static const char *assign_optimizer_minidump(const char *newval,
							bool doit, GucSource source);
static bool assign_optimizer(bool newval, bool doit, GucSource source);
static const char *assign_optimizer_cost_model(const char *newval,
                                                        bool doit, GucSource source);
static const char *assign_min_error_statement(const char *newval, bool doit,
						   GucSource source);
static const char *assign_gp_workfile_caching_loglevel(const char *newval,
						   bool doit, GucSource source);
static const char *assign_gp_sessionstate_loglevel(const char *newval,
						   bool doit, GucSource source);
static const char *assign_time_slice_report_level(const char *newval, bool doit,
						   GucSource source);
static const char *assign_deadlock_hazard_report_level(const char *newval, bool doit,
						   GucSource source);
static const char *assign_system_cache_flush_force(const char *newval, bool doit,
						   GucSource source);
static const char *assign_gp_idf_deduplicate(const char *newval, bool doit,
						   GucSource source);
static const char *assign_msglvl(int *var, const char *newval, bool doit,
			  GucSource source);
static const char *assign_IntervalStyle(const char *newval, bool doit,
						   GucSource source);
static const char *assign_log_error_verbosity(const char *newval, bool doit,
						   GucSource source);
static const char *assign_log_statement(const char *newval, bool doit,
					 GucSource source);
static const char *show_num_temp_buffers(void);
static bool assign_phony_autocommit(bool newval, bool doit, GucSource source);
static const char *assign_custom_variable_classes(const char *newval, bool doit,
							   GucSource source);
static const char *assign_explain_memory_verbosity(const char *newval, bool doit, GucSource source);
static bool assign_debug_assertions(bool newval, bool doit, GucSource source);
static bool assign_ssl(bool newval, bool doit, GucSource source);
static bool assign_stage_log_stats(bool newval, bool doit, GucSource source);
static bool assign_log_stats(bool newval, bool doit, GucSource source);
static bool assign_dispatch_log_stats(bool newval, bool doit, GucSource source);
static bool assign_transaction_read_only(bool newval, bool doit, GucSource source);
static const char *assign_canonical_path(const char *newval, bool doit, GucSource source);
static const char *assign_backslash_quote(const char *newval, bool doit, GucSource source);
static const char *assign_timezone_abbreviations(const char *newval, bool doit, GucSource source);

static bool assign_tcp_keepalives_idle(int newval, bool doit, GucSource source);
static bool assign_tcp_keepalives_interval(int newval, bool doit, GucSource source);
static bool assign_tcp_keepalives_count(int newval, bool doit, GucSource source);
static const char *show_IntervalStyle(void);
static const char *show_tcp_keepalives_idle(void);
static const char *show_tcp_keepalives_interval(void);
static const char *show_tcp_keepalives_count(void);

static const char *assign_pgstat_temp_directory(const char *newval, bool doit, GucSource source);
static const char *assign_application_name(const char *newval, bool doit, GucSource source);

static const char *assign_debug_dtm_action(const char *newval,
						bool doit, GucSource source);
static const char *assign_debug_dtm_action_target(const char *newval,
						bool doit, GucSource source);
static const char *assign_debug_dtm_action_protocol(const char *newval,
						bool doit, GucSource source);
static const char *assign_gp_log_format(const char *value, bool doit,
										GucSource source);

#ifdef USE_CONNECTEMC
static const char *assign_connectemc_mode(const char *newval, bool doit, GucSource source);
#endif

static bool dummy_autovac=false;
static bool assign_autovacuum_warning(bool newval, bool doit, GucSource source);

/* Helper function for guc setter */
extern const char *gpvars_assign_gp_resqueue_priority_default_value(const char *newval,
		bool doit,
		GucSource source __attribute__((unused)) );

static const char *assign_password_hash_algorithm(const char *newval,
												  bool doit, GucSource source);
static const char *assign_gp_default_storage_options(
		const char *newval, bool doit, GucSource source);
/*
 * GUC option variables that are exported from this module
 */
#ifdef USE_ASSERT_CHECKING
bool		assert_enabled = true;
#else
bool		assert_enabled = false;
#endif
bool		log_duration = false;
bool		Debug_print_plan = false;
bool		Debug_print_parse = false;
bool		Debug_print_rewritten = false;
bool		Debug_pretty_print = false;
bool		Explain_pretty_print = true;
bool		Debug_print_full_dtm = false;
bool		Debug_print_snapshot_dtm = false;
bool		Debug_print_qd_mirroring = false;
bool		Debug_permit_same_host_standby = false;
bool 		Debug_print_semaphore_detail = false;
bool		Debug_disable_distributed_snapshot = false;
bool 		Debug_abort_after_distributed_prepared = false;
bool 		Debug_abort_after_segment_prepared = false;
bool		Debug_print_tablespace = false;
bool		Debug_print_server_processes = false;
bool		Debug_print_control_checkpoints = false;
bool 		Debug_appendonly_print_insert = false;
bool 		Debug_appendonly_print_insert_tuple = false;
bool 		Debug_appendonly_print_scan = false;
bool 		Debug_appendonly_print_scan_tuple = false;
bool		Debug_appendonly_print_delete = false;
bool 		Debug_appendonly_print_update = false;
bool 		Debug_appendonly_print_update_tuple = false;
bool 		Debug_appendonly_print_storage_headers = false;
bool 		Debug_appendonly_print_verify_write_block = false;
bool		Debug_appendonly_use_no_toast = false;
bool 		Debug_appendonly_print_blockdirectory = false;
bool 		Debug_appendonly_print_read_block = false;
bool 		Debug_appendonly_print_append_block = false;
bool 		Debug_appendonly_print_segfile_choice = false;
int			Debug_appendonly_bad_header_print_level = ERROR;
bool		Debug_appendonly_print_datumstream = false;
bool		Debug_appendonly_print_visimap = false;
bool		Debug_appendonly_print_compaction = false;
bool 		Debug_gp_relation_node_fetch_wait_for_debugging = false;
bool        gp_crash_recovery_abort_suppress_fatal = false;
bool		gp_persistent_statechange_suppress_error = false;
bool		Debug_bitmap_print_insert = false;
bool 		Test_appendonly_override = false;
bool		Test_print_direct_dispatch_info = false;
bool 		gp_test_orientation_override = false;
bool		gp_permit_persistent_metadata_update = false;
bool		gp_permit_relation_node_change = false;
int         Test_compresslevel_override = 0;
int			Test_blocksize_override = 0;
int			Test_safefswritesize_override = 0;
bool        Master_mirroring_administrator_disable = false;
int			gp_max_local_distributed_cache = 1024;
bool		gp_appendonly_verify_block_checksums = true;
bool 		gp_appendonly_verify_write_block = false;
bool        gp_appendonly_verify_eof = true;
bool		gp_appendonly_compaction = true;
int         gp_appendonly_compaction_threshold = 0;
bool		gp_heap_require_relhasoids_match = true;
bool		Debug_appendonly_rezero_quicklz_compress_scratch = false;
bool		Debug_appendonly_rezero_quicklz_decompress_scratch = false;
bool		Debug_appendonly_guard_end_quicklz_scratch = false;
bool 		gp_local_distributed_cache_stats = false;
bool		Debug_xlog_insert_print = false;
bool		debug_xlog_record_read = false;
bool		Debug_persistent_print = false;
int			Debug_persistent_print_level = LOG;
bool		Debug_persistent_recovery_print = true;
int			Debug_persistent_recovery_print_level = LOG;
bool		Disable_persistent_recovery_logging = false;
bool		Debug_persistent_store_print = false;
int			Debug_persistent_store_print_level = LOG;
bool		Debug_persistent_bootstrap_print = false;
bool		persistent_integrity_checks = true;
bool		disable_persistent_diagnostic_dump = false;
bool		debug_persistent_ptcat_verification = false;
bool		debug_print_persistent_checks = false;
bool		Debug_bulk_load_bypass_wal = true;
bool		Debug_persistent_appendonly_commit_count_print = false;
bool		Debug_cancel_print = false;
bool		Debug_datumstream_write_print_small_varlena_info = false;
bool		Debug_datumstream_write_print_large_varlena_info = false;
bool		Debug_datumstream_read_check_large_varlena_integrity = false;
bool		Debug_datumstream_block_read_check_integrity = false;
bool		Debug_datumstream_block_write_check_integrity = false;
bool		Debug_datumstream_read_print_varlena_info = false;
bool		Debug_datumstream_write_use_small_initial_buffers = false;
bool		gp_temporary_files_filespace_repair = false;
bool		gp_create_table_random_default_distribution = true;
bool		gp_allow_non_uniform_partitioning_ddl = true;
bool		gp_enable_exchange_default_partition = false;

int			explain_memory_verbosity = 0;
char* 		memory_profiler_run_id = "none";
char* 		memory_profiler_dataset_id = "none";
char* 		memory_profiler_query_id = "none";
int 		memory_profiler_dataset_size = 0;
bool 		gp_dump_memory_usage = FALSE;

#define VERIFY_CHECKPOINT_INTERVAL_DEFAULT 180
int         verify_checkpoint_interval =
	VERIFY_CHECKPOINT_INTERVAL_DEFAULT;

bool Debug_rle_type_compression = false;
bool rle_type_compression_stats = false;

bool		Debug_database_command_print = false;
int			Debug_database_command_print_level = LOG;
#define GP_MAX_DATABASES_DEFAULT 16
int			gp_max_databases = GP_MAX_DATABASES_DEFAULT;
#define GP_MAX_TABLESPACES_DEFAULT 16
int			gp_max_tablespaces = GP_MAX_TABLESPACES_DEFAULT;
#define GP_MAX_FILESPACES_DEFAULT 8
int			gp_max_filespaces = GP_MAX_FILESPACES_DEFAULT;
bool 		gp_initdb_mirrored = false;
bool 		gp_before_persistence_work = false;
bool 		gp_before_filespace_setup = false;
bool		gp_startup_integrity_checks = true;
bool		gp_change_tracking = true;
bool		gp_persistent_skip_free_list = false;
bool		gp_persistent_repair_global_sequence = false;
bool 		Debug_print_xlog_relation_change_info = false;
bool 		Debug_print_xlog_relation_change_info_skip_issues_only = false;
bool 		Debug_print_xlog_relation_change_info_backtrace_skip_issues = false;
bool 		Debug_check_for_invalid_persistent_tid = false;

bool		Debug_filerep_crc_on = true;
bool		Debug_filerep_print = false;
bool		Debug_filerep_gcov = false;
bool		Debug_filerep_config_print = false;
bool		Debug_filerep_verify_performance_print = false;
bool		Debug_filerep_memory_log_flush = false;
bool		filerep_inject_listener_fault = false;
bool		filerep_inject_db_startup_fault = false;
bool		filerep_inject_change_tracking_recovery_fault = false;
bool		filerep_mirrorvalidation_during_resync = false;

/* WAL based replication debug GUCs */
bool	 	debug_walrepl_snd = false;
bool	 	debug_walrepl_syncrep = false;
bool	 	debug_walrepl_rcv = false;
bool		debug_basebackup = false;

/* Latch mechanism debug GUCs */
bool		debug_latch = false;

bool		gp_crash_recovery_suppress_ao_eof = false;
bool 	        gp_keep_all_xlog = false;
int			keep_wal_segments = 0;
int 		ddboost_buf_size = 512*1024;

#define DEBUG_DTM_ACTION_PRIMARY_DEFAULT true
bool		Debug_dtm_action_primary = DEBUG_DTM_ACTION_PRIMARY_DEFAULT;

bool 		gp_log_optimization_time = false;
bool		log_parser_stats = false;
bool		log_planner_stats = false;
bool		log_executor_stats = false;
bool		log_statement_stats = false;		/* this is sort of all three
												 * above together */
bool 		log_dispatch_stats = false;
bool		log_btree_build_stats = false;

bool		check_function_bodies = true;
bool		default_with_oids = false;
bool		SQL_inheritance = true;

bool		Password_encryption = true;

int			log_min_error_statement = ERROR;
int			log_min_messages = WARNING;
int			client_min_messages = NOTICE;
int			log_min_duration_statement = -1;

int			num_temp_buffers = 1000;

int			Debug_delay_prepare_broadcast_ms = 0;
int			Debug_delay_commit_broadcast_ms = 0;
int			Debug_delay_abort_broadcast_ms = 0;

int			Debug_dtm_action = DEBUG_DTM_ACTION_NONE;

#define DEBUG_DTM_ACTION_TARGET_DEFAULT DEBUG_DTM_ACTION_TARGET_NONE

int			Debug_dtm_action_target = DEBUG_DTM_ACTION_TARGET_DEFAULT;

#define DEBUG_DTM_ACTION_PROTOCOL_DEFAULT DTX_PROTOCOL_COMMAND_COMMIT_PREPARED

int			Debug_dtm_action_protocol = DEBUG_DTM_ACTION_PROTOCOL_DEFAULT;

#define DEBUG_DTM_ACTION_DELAY_MS_DEFAULT 5000
#define DEBUG_DTM_ACTION_SEGMENT_DEFAULT 1
#define DEBUG_DTM_ACTION_NESTINGLEVEL_DEFAULT 0

int			Debug_dtm_action_delay_ms = DEBUG_DTM_ACTION_DELAY_MS_DEFAULT;
int			Debug_dtm_action_segment = DEBUG_DTM_ACTION_SEGMENT_DEFAULT;
int			Debug_dtm_action_nestinglevel = DEBUG_DTM_ACTION_NESTINGLEVEL_DEFAULT;

char	   *ConfigFileName;
char	   *HbaFileName;
char	   *IdentFileName;
char	   *external_pid_file;
char	   *Debug_dtm_action_sql_command_tag;
char       *Debug_dtm_action_str;
char       *Debug_dtm_action_target_str;
char       *Debug_dtm_action_protocol_str;

/* Enable check for compatibility of encoding and locale in createdb */
bool		gp_encoding_check_locale_compatibility;

char	   *pgstat_temp_directory;

char	   *application_name;

int			tcp_keepalives_idle;
int			tcp_keepalives_interval;
int			tcp_keepalives_count;

int			gp_connection_send_timeout;

int			gp_filerep_tcp_keepalives_idle;
int			gp_filerep_tcp_keepalives_interval;
int			gp_filerep_tcp_keepalives_count;

int			WalSendClientTimeout = 30000;	// 30 seconds.

char 	   *data_directory;

int 		defunct_int = 0;
bool 		defunct_bool = 0;
double 		defunct_double = 0;

char	   *gp_email_smtp_server;
char	   *gp_email_smtp_userid;
char	   *gp_email_smtp_password;
char	   *gp_email_from;
char	   *gp_email_to;
int        gp_email_connect_timeout;
int        gp_email_connect_failures;
int        gp_email_connect_avoid_duration;

#if USE_SNMP
char	   *gp_snmp_community;
char	   *gp_snmp_monitor_address;
char	   *gp_snmp_use_inform_or_trap;
char       *gp_snmp_debug_log;
#endif

char	   *gp_connectemc_mode;
#ifdef USE_CONNECTEMC
EmcConnectModeType_t gp_emcconnect_transport;
#endif

/* The following GUC holds the default version for append-only tables */
int	test_appendonly_version_default = AORelationVersion_GetLatest();

/*
 * These variables are all dummies that don't do anything, except in some
 * cases provide the value for SHOW to display.  The real state is elsewhere
 * and is kept in sync by assign_hooks.
 */
static char *gp_hashagg_compress_spill_files_str;
static char *gp_workfile_compress_algorithm_str;
static char *gp_workfile_type_hashjoin_str;
static char *client_min_messages_str;
static char *optimizer_log_failure_str;
static char *optimizer_minidump_str;
static char *optimizer_cost_model_str;
static char *Debug_persistent_print_level_str;
static char *Debug_persistent_recovery_print_level_str;
static char *Debug_persistent_store_print_level_str;
static char *Debug_database_command_print_level_str;
static char *log_min_messages_str;
static char *log_error_verbosity_str;
static char *log_statement_str;
static char *log_min_error_statement_str;
static char *log_destination_string;
static char *gp_log_format_string;
static char *gp_workfile_caching_loglevel_str;
static char *gp_sessionstate_loglevel_str;
static char *explain_memory_verbosity_str;

#ifdef HAVE_SYSLOG
static char *syslog_facility_str;
static char *syslog_ident_str;
#endif
static bool phony_autocommit;
static bool session_auth_is_superuser;
static double phony_random_seed;
static char *backslash_quote_string;
static char *client_encoding_string;
static char *IntervalStyle_string;
static char *datestyle_string;
static char *default_iso_level_string;
static char *locale_ctype;
static char *regex_flavor_string;
static char *server_encoding_string;
static char *server_version_string;
static int	server_version_num;
static char *timezone_string;
static char *log_timezone_string;
static char *timezone_abbreviations_string;
static char *XactIsoLevel_string;
static char *custom_variable_classes;
static int	max_function_args;
static int	max_index_keys;
static int	max_identifier_length;
static int	block_size;
static bool integer_datetimes;
//static bool standard_conforming_strings;
static char *gp_log_gang_str;
static char *gp_log_fts_str;
static char *gp_log_interconnect_str;
static char *gp_interconnect_type_str;
static char *gp_interconnect_fc_method_str;

/* should be static, but commands/variable.c needs to get at these */
char	   *role_string;
char	   *session_authorization_string;

/* Backoff-related GUCs */
bool gp_enable_resqueue_priority;
int gp_resqueue_priority_local_interval;
int gp_resqueue_priority_sweeper_interval;
int gp_resqueue_priority_inactivity_timeout;
int gp_resqueue_priority_grouping_timeout;
double gp_resqueue_priority_cpucores_per_segment;
char* gp_resqueue_priority_default_value;
bool gp_debug_resqueue_priority = false;

/* Perfmon segment GUCs */
int gp_perfmon_segment_interval;
static char *gpperfmon_log_alert_level_str;

/* Perfmon debug GUC */
bool gp_perfmon_print_packet_info;

/* Simulator of Exceptions (SimEx) GUCs */
bool gp_simex_init;
bool gp_simex_run;
int gp_simex_class;
double gp_simex_rand;

/* time slice enforcement */
bool gp_test_time_slice;
int gp_test_time_slice_interval;
int gp_test_time_slice_report_level = ERROR;
static char *gp_test_time_slice_report_level_str;

/* database-lightweight lock hazard detection */
bool gp_test_deadlock_hazard;
int gp_test_deadlock_hazard_report_level = ERROR;
static char *gp_test_deadlock_hazard_report_level_str;

/* query cancellation GUC */
bool gp_cancel_query_print_log;
int gp_cancel_query_delay_time;

/* partitioning GUC */
bool gp_partitioning_dynamic_selection_log;
int gp_max_partition_level;

/* Upgrade & maintenance GUCs */
bool gp_upgrade_mode;
bool gp_maintenance_mode;
bool gp_maintenance_conn;
bool allow_segment_DML;
static char *allow_system_table_mods_str;

/* ignore EXCLUDE clauses in window spec for backwards compatibility */
bool gp_ignore_window_exclude = false;

static const char *
assign_allow_system_table_mods(const char *newval,
							   bool doit,
							   GucSource source);

static const char *
show_allow_system_table_mods(void);

/* Hadoop Integration GUCs */
char  *gp_hadoop_connector_jardir;
char  *gp_hadoop_connector_version = ""; /* old GUC; now it's a global var. */
char  *gp_hadoop_target_version;
char  *gp_hadoop_home;

/* Time based authentication GUC */
char  *gp_auth_time_override_str = NULL;

/* Password hashing */
char  *password_hash_algorithm_str = "MD5";
PasswdHashAlg password_hash_algorithm = PASSWORD_HASH_MD5;

/* system cache invalidation mode*/
int gp_test_system_cache_flush_force = SysCacheFlushForce_Off;
static char *gp_test_system_cache_flush_force_str;

/* include file/line information to stack traces */
bool gp_log_stack_trace_lines;

/* Planner gucs */
bool		enable_seqscan = true;
bool		enable_indexscan = true;
bool		enable_bitmapscan = true;
bool		force_bitmap_table_scan = false;
bool		enable_tidscan = true;
bool		enable_sort = true;
bool		enable_hashagg = true;
bool		enable_groupagg = true;
bool		enable_nestloop = false;
bool		enable_mergejoin = false;
bool		enable_hashjoin = true;
bool        gp_enable_hashjoin_size_heuristic = false;
bool		gp_enable_fallback_plan = true;
bool        gp_enable_predicate_propagation = false;
bool		constraint_exclusion = false;
bool		gp_enable_multiphase_agg = true;
bool		gp_enable_preunique = TRUE;
bool		gp_eager_preunique = FALSE;
bool		gp_enable_sequential_window_plans = FALSE;
bool 		gp_hashagg_streambottom = true;
bool		gp_enable_agg_distinct = true;
bool		gp_enable_dqa_pruning = true;
bool		gp_eager_dqa_pruning = FALSE;
bool		gp_eager_one_phase_agg = FALSE;
bool		gp_eager_two_phase_agg = FALSE;
bool        gp_enable_groupext_distinct_pruning = true;
bool        gp_enable_groupext_distinct_gather = true;
bool		gp_dynamic_partition_pruning = true;
bool		gp_log_dynamic_partition_pruning = false;
bool		gp_cte_sharing = false;

/*
 * If set to true, we will silently insert into the correct leaf
 * part even if the user specifies a wrong leaf part as a insert target
 */
bool		dml_ignore_target_partition_check = false;

char	   *gp_idf_deduplicate_str;

bool fts_diskio_check = false;
/* gp_disable_catalog_access_on_segment */
bool gp_disable_catalog_access_on_segment = false;

/* ORCA related gucs */
bool		optimizer_control = true;
bool		optimizer;
bool		optimizer_log;
bool		optimizer_partition_selection_log;
bool		optimizer_minidump;
int             optimizer_cost_model;
bool		optimizer_print_query;
bool		optimizer_print_plan;
bool		optimizer_print_xform;
bool		optimizer_release_mdcache;
bool		optimizer_disable_xform_result_printing;
bool		optimizer_print_memo_after_exploration;
bool		optimizer_print_memo_after_implementation;
bool		optimizer_print_memo_after_optimization;
bool		optimizer_print_job_scheduler;
bool		optimizer_print_expression_properties;
bool		optimizer_print_group_properties;
bool		optimizer_print_optimization_context;
bool		optimizer_print_optimization_stats;
bool		optimizer_parallel;
bool		optimizer_local;
int 		optimizer_retries;
bool  		optimizer_xforms[OPTIMIZER_XFORMS_COUNT] = {[0 ... OPTIMIZER_XFORMS_COUNT - 1] = false}; /* array of xforms disable flags */
char 		*optimizer_search_strategy_path = NULL;
char	   *gp_idf_deduplicate_str;
bool		optimizer_extract_dxl_stats;
bool		optimizer_extract_dxl_stats_all_nodes;
bool 		optimizer_disable_missing_stats_collection;
bool        optimizer_dpe_stats;
bool		optimizer_enable_indexjoin;
bool		optimizer_enable_motions_masteronly_queries;
bool		optimizer_enable_motions;
bool		optimizer_enable_motion_broadcast;
bool		optimizer_enable_motion_gather;
bool		optimizer_enable_motion_redistribute;
bool		optimizer_enable_sort;
bool		optimizer_enable_materialize;
bool		optimizer_enable_partition_propagation;
bool		optimizer_enable_partition_selection;
bool		optimizer_enable_outerjoin_rewrite;
bool		optimizer_enable_space_pruning;
bool		optimizer_prefer_multistage_agg;
bool		optimizer_enable_multiple_distinct_aggs;
bool		optimizer_prefer_expanded_distinct_aggs;
bool		optimizer_prune_computed_columns;
bool		optimizer_push_requirements_from_consumer_to_producer;
bool		optimizer_direct_dispatch;
bool		optimizer_enable_hashjoin_redistribute_broadcast_children;
bool		optimizer_enable_broadcast_nestloop_outer_child;
bool		optimizer_enforce_subplans;
bool		optimizer_enable_assert_maxonerow;
bool		optimizer_enumerate_plans;
bool		optimizer_sample_plans;
int			optimizer_plan_id;
int 		optimizer_samples_number;
int			optimizer_log_failure;
double		optimizer_cost_threshold;
double		optimizer_nestloop_factor;
bool		optimizer_cte_inlining;
int			optimizer_cte_inlining_bound;
double 		optimizer_damping_factor_filter;
double		optimizer_damping_factor_join;
double 		optimizer_damping_factor_groupby;
int			optimizer_segments;
bool		optimizer_analyze_root_partition;
bool		optimizer_analyze_midlevel_partition;
bool		optimizer_enable_constant_expression_evaluation;
bool		optimizer_use_external_constant_expression_evaluation_for_ints;
bool		optimizer_enable_bitmapscan;
bool		optimizer_enable_outerjoin_to_unionall_rewrite;
bool		optimizer_apply_left_outer_to_union_all_disregarding_stats;
bool		optimizer_enable_ctas;
bool		optimizer_remove_order_below_dml;
bool		optimizer_static_partition_selection;
bool		optimizer_enable_partial_index;
bool		optimizer_dml_triggers;
bool		optimizer_dml_constraints;
bool 		optimizer_enable_master_only_queries;
bool 		optimizer_multilevel_partitioning;
bool		optimizer_explain_show_status;

/* Security */
bool		gp_reject_internal_tcp_conn = true;

/* plpgsql plancache GUC */
bool gp_plpgsql_clear_cache_always = false;

/*
 * Default storage options GUC.  Value is comma-separated name=value
 * pairs.  E.g. "appendonly=true,orientation=column"
 */
char *gp_default_storage_options = NULL;

/*
 * Displayable names for context types (enum GucContext)
 *
 * Note: these strings are deliberately not localized.
 */
const char *const GucContext_Names[] =
{
	 /* PGC_INTERNAL */ "internal",
	 /* PGC_POSTMASTER */ "postmaster",
	 /* PGC_SIGHUP */ "sighup",
	 /* PGC_BACKEND */ "backend",
	 /* PGC_SUSET */ "superuser",
	 /* PGC_USERSET */ "user"
};

/*
 * Displayable names for source types (enum GucSource)
 *
 * Note: these strings are deliberately not localized.
 */
const char *const GucSource_Names[] =
{
	 /* PGC_S_DEFAULT */ "default",
	 /* PGC_S_ENV_VAR */ "environment variable",
	 /* PGC_S_FILE */ "configuration file",
	 /* PGC_S_ARGV */ "command line",
	 /* PGC_S_DATABASE */ "database",
	 /* PGC_S_USER */ "user",
	 /* PGC_S_CLIENT */ "client",
	 /* PGC_S_OVERRIDE */ "override",
	 /* PGC_S_INTERACTIVE */ "interactive",
	 /* PGC_S_TEST */ "test",
	 /* PGC_S_SESSION */ "session"
};

/*
 * Displayable names for the groupings defined in enum config_group
 */
const char *const config_group_names[] =
{
	/* UNGROUPED */
	gettext_noop("Ungrouped"),
	/* FILE_LOCATIONS */
	gettext_noop("File Locations"),
	/* CONN_AUTH */
	gettext_noop("Connections and Authentication"),
	/* CONN_AUTH_SETTINGS */
	gettext_noop("Connections and Authentication / Connection Settings"),
	/* CONN_AUTH_SECURITY */
	gettext_noop("Connections and Authentication / Security and Authentication"),


	/* EXTERNAL_TABLES */
	gettext_noop("External Tables"),
	/* APPENDONLY_TABLES */
	gettext_noop("Append-Only Tables"),
	/* RESOURCES */
	gettext_noop("Resource Usage"),
	/* RESOURCES_MEM */
	gettext_noop("Resource Usage / Memory"),
	/* RESOURCES_FSM */
	gettext_noop("Resource Usage / Free Space Map"),


	/* RESOURCES_KERNEL */
	gettext_noop("Resource Usage / Kernel Resources"),
	/* RESOURCES_MGM */
	gettext_noop("Resource Usage / Resources Management"),
	/* WAL */
	gettext_noop("Write-Ahead Log"),
	/* WAL_SETTINGS */
	gettext_noop("Write-Ahead Log / Settings"),
	/* WAL_CHECKPOINTS */
	gettext_noop("Write-Ahead Log / Checkpoints"),
	/* WAL based REPLICATION */
	gettext_noop("WAL Replication"),


	/* QUERY_TUNING */
	gettext_noop("Query Tuning"),
	/* QUERY_TUNING_METHOD */
	gettext_noop("Query Tuning / Planner Method Configuration"),
	/* QUERY_TUNING_COST */
	gettext_noop("Query Tuning / Planner Cost Constants"),
	/* QUERY_TUNING_OTHER */
	gettext_noop("Query Tuning / Other Planner Options"),


	/* LOGGING */
	gettext_noop("Reporting and Logging"),
	/* LOGGING_WHERE */
	gettext_noop("Reporting and Logging / Where to Log"),
	/* LOGGING_WHEN */
	gettext_noop("Reporting and Logging / When to Log"),
	/* LOGGING_WHAT */
	gettext_noop("Reporting and Logging / What to Log"),
	/* STATS */
	gettext_noop("Statistics"),


	/* STATS_ANALYZE */
	gettext_noop("Statistics / ANALYZE Database Contents"),
	/* STATS_MONITORING */
	gettext_noop("Statistics / Monitoring"),
	/* STATS_COLLECTOR */
	gettext_noop("Statistics / Query and Index Statistics Collector"),
	/* AUTOVACUUM */
	gettext_noop("Autovacuum"),
	/* CLIENT_CONN */
	gettext_noop("Client Connection Defaults"),


	/* CLIENT_CONN_STATEMENT */
	gettext_noop("Client Connection Defaults / Statement Behavior"),
	/* CLIENT_CONN_LOCALE */
	gettext_noop("Client Connection Defaults / Locale and Formatting"),
	/* CLIENT_CONN_OTHER */
	gettext_noop("Client Connection Defaults / Other Defaults"),
	/* LOCK_MANAGEMENT */
	gettext_noop("Lock Management"),
	/* COMPAT_OPTIONS */
	gettext_noop("Version and Platform Compatibility"),


	/* COMPAT_OPTIONS_PREVIOUS */
	gettext_noop("Version and Platform Compatibility / Previous PostgreSQL Versions"),
	/* COMPAT_OPTIONS_CLIENT */
	gettext_noop("Version and Platform Compatibility / Other Platforms and Clients"),
    /* COMPAT_OPTIONS_IGNORED */
    gettext_noop("Version and Platform Compatibility / Ignored"),
    /* GP_ARRAY_CONFIGURATION */
    gettext_noop(PACKAGE_NAME " / Array Configuration"),
    /* GP_ARRAY_TUNING */
    gettext_noop(PACKAGE_NAME " / Array Tuning"),


    /* GP_WORKER_IDENTITY */
    gettext_noop(PACKAGE_NAME " / Worker Process Identity"),
	/* GP_ERROR_HANDLING */
	gettext_noop("GPDB Error Handling"),
	/* PRESET_OPTIONS */
	gettext_noop("Preset Options"),
	/* CUSTOM_OPTIONS */
	gettext_noop("Customized Options"),
	/* DEVELOPER_OPTIONS */
	gettext_noop("Developer Options"),


	/* DEPRECATED_OPTIONS */
	gettext_noop("Deprecated Options"),


	/* DEFUNCT_OPTIONS */
	gettext_noop("Defunct Options"),

	/* help_config wants this array to be null-terminated */
	NULL
};

/*
 * Displayable names for GUC variable types (enum config_type)
 *
 * Note: these strings are deliberately not localized.
 */
const char *const config_type_names[] =
{
	 /* PGC_BOOL */ "bool",
	 /* PGC_INT */ "integer",
	 /* PGC_REAL */ "real",
	 /* PGC_STRING */ "string"
};


/*
 * Contents of GUC tables
 *
 * See src/backend/utils/misc/README for design notes.
 *
 * TO ADD AN OPTION:
 *
 * 1. Declare a global variable of type bool, int, double, or char*
 *	  and make use of it.
 *
 * 2. Decide at what times it's safe to set the option. See guc.h for
 *	  details.
 *
 * 3. Decide on a name, a default value, upper and lower bounds (if
 *	  applicable), etc.
 *
 * 4. Add a record below.
 *
 * 5. Add it to src/backend/utils/misc/postgresql.conf.sample, if
 *	  appropriate.
 *
 * 6. Add it to src/bin/psql/tab-complete.c, if it's a USERSET option.
 *
 * 7. Don't forget to document the option.
 *
 * 8. If it's a new GUC_LIST option you must edit pg_dumpall.c to ensure
 *	  it is not single quoted at dump time.
 */


/******** option records follow ********/

static struct config_bool ConfigureNamesBool[] =
{
	{
		{"upgrade_mode", PGC_POSTMASTER, CUSTOM_OPTIONS,
			gettext_noop("Upgrade Mode"),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_upgrade_mode,
		false, NULL, NULL
	},

	{
		{"maintenance_mode", PGC_POSTMASTER, CUSTOM_OPTIONS,
			gettext_noop("Maintenance Mode"),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_maintenance_mode,
		false, NULL, NULL
	},

	{
		{"gp_maintenance_conn", PGC_BACKEND, CUSTOM_OPTIONS,
			gettext_noop("Maintenance Connection"),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_maintenance_conn,
		false, NULL, NULL
	},

	{
		{"allow_segment_DML", PGC_USERSET, CUSTOM_OPTIONS,
			gettext_noop("Allow DML on segments"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&allow_segment_DML,
		false, NULL, NULL
	},

	{
		{"enable_seqscan", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Enables the planner's use of sequential-scan plans."),
			NULL
		},
		&enable_seqscan,
		true, NULL, NULL
	},
	{
		{"enable_indexscan", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Enables the planner's use of index-scan plans."),
			NULL
		},
		&enable_indexscan,
		true, NULL, NULL
	},
	{
		{"enable_bitmapscan", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Enables the planner's use of bitmap-scan plans."),
			NULL
		},
		&enable_bitmapscan,
		true, NULL, NULL
	},
	{
		{"enable_tidscan", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Enables the planner's use of TID scan plans."),
			NULL
		},
		&enable_tidscan,
		true, NULL, NULL
	},
	{
		{"enable_sort", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Enables the planner's use of explicit sort steps."),
			NULL
		},
		&enable_sort,
		true, NULL, NULL
	},
	{
		{"enable_hashagg", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Enables the planner's use of hashed aggregation plans."),
			NULL
		},
		&enable_hashagg,
		true, NULL, NULL
	},
	{
		{"enable_groupagg", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Enables the planner's use of grouping aggregation plans."),
			NULL
		},
		&enable_groupagg,
		true, NULL, NULL
	},
	{
		{"enable_nestloop", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Enables the planner's use of nested-loop join plans."),
			NULL
		},
		&enable_nestloop,
		false, NULL, NULL
	},
	{
		{"enable_mergejoin", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Enables the planner's use of merge join plans."),
			NULL
		},
		&enable_mergejoin,
		false, NULL, NULL
	},
	{
		{"enable_hashjoin", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Enables the planner's use of hash join plans."),
			NULL
		},
		&enable_hashjoin,
		true, NULL, NULL
	},
	{
		{"gp_enable_hashjoin_size_heuristic", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("In hash join plans, the smaller of the two inputs "
                         "(as estimated) is used to build the hash table."),
			gettext_noop("If false, either input could be used to build the "
                         "hash table; the choice depends on the estimated hash "
                         "join cost, which the planner computes for both "
                         "alternatives.  Has no effect on outer or adaptive "
                         "joins."),
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_enable_hashjoin_size_heuristic,
		false, NULL, NULL
	},
	{
		{"gp_enable_fallback_plan", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Plan types which are not enabled may be used when a "
                         "query would be infeasible without them."),
			gettext_noop("If false, planner rejects queries that cannot be "
                         "satisfied using only the enabled plan types.")
		},
		&gp_enable_fallback_plan,
		true, NULL, NULL
	},
	{
		{"gp_enable_direct_dispatch", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Enable dispatch for single-row-insert targetted mirror-pairs."),
			gettext_noop("Don't involve the whole cluster if it isn't needed.")
		},
		&gp_enable_direct_dispatch,
		true, NULL, NULL
	},
	{
		{"gp_enable_predicate_propagation", PGC_USERSET, QUERY_TUNING_OTHER,
			gettext_noop("When two expressions are equivalent (such as with "
			             "equijoined keys) then the planner applies predicates "
			             "on one expression to the other expression."),
			gettext_noop("If false, planner does not copy predicates.")
		},
		&gp_enable_predicate_propagation,
		true, NULL, NULL
	},
	{
		{"gp_workfile_checksumming", PGC_USERSET, QUERY_TUNING_OTHER,
		 gettext_noop("Enable checksumming on the executor work files in order to "
					  "catch possible faulty writes caused by your disk drivers."),
		 NULL,
		 GUC_GPDB_ADDOPT
		},
		&gp_workfile_checksumming,
		true, NULL, NULL
	},
	{
		{"gp_workfile_caching", PGC_SUSET, QUERY_TUNING_OTHER,
			gettext_noop("Enable work file caching"),
			gettext_noop("When enabled, work files are persistent "
					     "and their contents can be reused."),
			GUC_GPDB_ADDOPT | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_workfile_caching,
		false, NULL, NULL
	},
	{
		{"force_bitmap_table_scan", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Forces bitmap table scan instead of bitmap heap/ao/aoco scan."),
			NULL,
			GUC_GPDB_ADDOPT | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&force_bitmap_table_scan,
		false, NULL, NULL
	},
	{
		{"gp_workfile_faultinject", PGC_SUSET, DEVELOPER_OPTIONS,
		 gettext_noop("Fault inject a torn page to an executor workfile."),
		 gettext_noop("Used to simulate a failure and test workfile checksumming."),
		 GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_workfile_faultinject,
		false, NULL, NULL
	},
	{
		{"constraint_exclusion", PGC_USERSET, QUERY_TUNING_OTHER,
			gettext_noop("Enables the planner to use constraints to optimize queries."),
			gettext_noop("Child table scans will be skipped if their "
						 "constraints guarantee that no rows match the query."),
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&constraint_exclusion,
		true, NULL, NULL
	},
	{
		{"geqo", PGC_USERSET, DEFUNCT_OPTIONS,
			gettext_noop("Unused. Syntax check only for PostgreSQL compatibility."),
            NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&defunct_bool,
		false, NULL, NULL
	},

	{
		/* Not for general use --- used by SET SESSION AUTHORIZATION */
		{"is_superuser", PGC_INTERNAL, UNGROUPED,
			gettext_noop("Shows whether the current user is a superuser."),
			NULL,
			GUC_REPORT | GUC_NO_SHOW_ALL | GUC_NO_RESET_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&session_auth_is_superuser,
		false, NULL, NULL
	},
	{
		{"ssl", PGC_POSTMASTER, CONN_AUTH_SECURITY,
			gettext_noop("Enables SSL connections."),
			NULL
		},
		&EnableSSL,
		false, assign_ssl, NULL
	},
	{
		{"fsync", PGC_SIGHUP, WAL_SETTINGS,
			gettext_noop("Forces synchronization of updates to disk."),
			gettext_noop("The server will use the fsync() system call in several places to make "
						 "sure that updates are physically written to disk. This insures "
						 "that a database cluster will recover to a consistent state after "
						 "an operating system or hardware crash."),
		  GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL | GUC_DISALLOW_USER_SET
		},
		&enableFsync,
		true, NULL, NULL
	},
	{
		{"zero_damaged_pages", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Continues processing past damaged page headers."),
			gettext_noop("Detection of a damaged page header normally causes PostgreSQL to "
				"report an error, aborting the current transaction. Setting "
						 "zero_damaged_pages to true causes the system to instead report a "
						 "warning, zero out the damaged page, and continue processing. This "
						 "behavior will destroy data, namely all the rows on the damaged page."),
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&zero_damaged_pages,
		false, NULL, NULL
	},
	{
		{"memory_protect_buffer_pool", PGC_POSTMASTER, DEVELOPER_OPTIONS,
			gettext_noop("Enables memory protection of the buffer pool"),
			gettext_noop("Turn on memory protection of the buffer pool "
						 "to detect invalid accesses to the buffer pool memory."),
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&memory_protect_buffer_pool, false, NULL, NULL
	},
	{
		{"gp_flush_buffer_pages_when_evicted", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Always flushes buffer pages, regardless of dirty status"),
			gettext_noop("When buffer pool pages are evicted, flush them to disk regardless of their dirty status."),
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&flush_buffer_pages_when_evicted,
		false, NULL, NULL
	},
	{
		{"full_page_writes", PGC_SIGHUP, WAL_SETTINGS,
			gettext_noop("Writes full pages to WAL when first modified after a checkpoint."),
			gettext_noop("A page write in process during an operating system crash might be "
						 "only partially written to disk.  During recovery, the row changes "
						 "stored in WAL are not enough to recover.  This option writes "
						 "pages when first modified after a checkpoint to WAL so full recovery "
						 "is possible."),
			 GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&fullPageWrites,
		true, NULL, NULL
	},
	{
		{"silent_mode", PGC_POSTMASTER, LOGGING_WHEN,
			gettext_noop("Runs the server silently."),
			gettext_noop("If this parameter is set, the server will automatically run in the "
				 "background and any controlling terminals are dissociated."),
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&SilentMode,
		false, NULL, NULL
	},
	{
		{"log_connections", PGC_BACKEND, LOGGING_WHAT,
			gettext_noop("Logs each successful connection."),
			NULL
		},
		&Log_connections,
		false, NULL, NULL
	},
	{
		{"log_disconnections", PGC_BACKEND, LOGGING_WHAT,
			gettext_noop("Logs end of a session, including duration."),
			NULL
		},
		&Log_disconnections,
		false, NULL, NULL
	},
	{
		{"debug_assertions", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Turns on various assertion checks."),
			gettext_noop("This is a debugging aid."),
			GUC_NOT_IN_SAMPLE
		},
		&assert_enabled,
#ifdef USE_ASSERT_CHECKING
		true,
#else
		false,
#endif
		assign_debug_assertions, NULL
	},
	{
		/* currently undocumented, so don't show in SHOW ALL */
		{"exit_on_error", PGC_USERSET, UNGROUPED,
			gettext_noop("no description available"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&ExitOnAnyError,
		false, NULL, NULL
	},
	{
		{"log_duration", PGC_SUSET, LOGGING_WHAT,
			gettext_noop("Logs the duration of each completed SQL statement."),
			NULL,
			GUC_GPDB_ADDOPT
		},
		&log_duration,
		false, NULL, NULL
	},
	{
		{"debug_print_parse", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Prints the parse tree to the server log."),
			NULL
		},
		&Debug_print_parse,
		false, NULL, NULL
	},
	{
		{"debug_print_rewritten", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Prints the parse tree after rewriting to server log."),
			NULL
		},
		&Debug_print_rewritten,
		false, NULL, NULL
	},
	{
		{"debug_print_prelim_plan", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Prints the preliminary execution plan to server log."),
			NULL
		},
		&Debug_print_prelim_plan,
		false, NULL, NULL
	},
	{
		{"debug_print_slice_table", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Prints the slice table to server log."),
			NULL
		},
		&Debug_print_slice_table,
		false, NULL, NULL
	},
	{
		{"debug_print_plan", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Prints the execution plan to server log."),
			NULL
		},
		&Debug_print_plan,
		false, NULL, NULL
	},
	{
		{"debug_pretty_print", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Indents parse and plan tree displays."),
			NULL
		},
		&Debug_pretty_print,
		false, NULL, NULL
	},
	{
		{"log_parser_stats", PGC_SUSET, STATS_MONITORING,
			gettext_noop("Writes parser performance statistics to the server log."),
			NULL
		},
		&log_parser_stats,
		false, assign_stage_log_stats, NULL
	},
	{
		{"log_planner_stats", PGC_SUSET, STATS_MONITORING,
			gettext_noop("Writes planner performance statistics to the server log."),
			NULL
		},
		&log_planner_stats,
		false, assign_stage_log_stats, NULL
	},
	{
		{"log_executor_stats", PGC_SUSET, STATS_MONITORING,
			gettext_noop("Writes executor performance statistics to the server log."),
			NULL,
			GUC_GPDB_ADDOPT
		},
		&log_executor_stats,
		false, assign_stage_log_stats, NULL
	},
	{
		{"log_statement_stats", PGC_SUSET, STATS_MONITORING,
			gettext_noop("Writes cumulative performance statistics to the server log."),
			NULL,
			GUC_GPDB_ADDOPT
		},
		&log_statement_stats,
		false, assign_log_stats, NULL
	},
	{
		{"log_dispatch_stats", PGC_SUSET, STATS_MONITORING,
			gettext_noop("Writes dispatcher performance statistics to the server log."),
			NULL,
			GUC_GPDB_ADDOPT
		},
		&log_dispatch_stats,
		false, assign_dispatch_log_stats, NULL
	},
#ifdef BTREE_BUILD_STATS
	{
		{"log_btree_build_stats", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("no description available"),
			NULL,
			GUC_NOT_IN_SAMPLE
		},
		&log_btree_build_stats,
		false, NULL, NULL
	},
#endif

	{
		{"explain_pretty_print", PGC_USERSET, CLIENT_CONN_OTHER,
			gettext_noop("Uses the indented output format for EXPLAIN VERBOSE."),
			NULL
		},
		&Explain_pretty_print,
		true, NULL, NULL
	},
	{
		{"track_activities", PGC_SUSET, STATS_COLLECTOR,
			gettext_noop("Collects information about executing commands."),
			gettext_noop("Enables the collection of information on the currently "
						 "executing command of each session, along with "
						 "the time at which that command began execution.")
		},
		&pgstat_track_activities,
		true, NULL, NULL
	},
	{
		{"track_counts", PGC_SUSET, STATS_COLLECTOR,
			gettext_noop("Collects statistics on database activity."),
			NULL
		},
		&pgstat_track_counts,
		false, NULL, NULL
	},

	{
		{"stats_queue_level", PGC_SUSET, STATS_COLLECTOR,
			gettext_noop("Collects resource queue-level statistics on database activity."),
			NULL
		},
		&pgstat_collect_queuelevel,
		false, NULL, NULL
	},

	{
		{"update_process_title", PGC_SUSET, CLIENT_CONN_OTHER,
			gettext_noop("Updates the process title to show the active SQL command."),
			gettext_noop("Enables updating of the process title every time a new SQL command is received by the server.")
		},
		&update_process_title,
		true, NULL, NULL
	},

	{
		{"autovacuum", PGC_SIGHUP, DEFUNCT_OPTIONS,
			gettext_noop("Starts the autovacuum subprocess."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&dummy_autovac,
		false, assign_autovacuum_warning, NULL
	},

	{
		{"trace_notify", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Generates debugging output for LISTEN and NOTIFY."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&Trace_notify,
		false, NULL, NULL
	},

#ifdef LOCK_DEBUG
	{
		{"trace_locks", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("no description available"),
			NULL,
			GUC_NOT_IN_SAMPLE
		},
		&Trace_locks,
		false, NULL, NULL
	},
	{
		{"trace_userlocks", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("no description available"),
			NULL,
			GUC_NOT_IN_SAMPLE
		},
		&Trace_userlocks,
		false, NULL, NULL
	},
	{
		{"trace_lwlocks", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("no description available"),
			NULL,
			GUC_NOT_IN_SAMPLE
		},
		&Trace_lwlocks,
		false, NULL, NULL
	},
	{
		{"debug_deadlocks", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("no description available"),
			NULL,
			GUC_NOT_IN_SAMPLE
		},
		&Debug_deadlocks,
		false, NULL, NULL
	},
#endif

	{
		{"log_hostname", PGC_SIGHUP, LOGGING_WHAT,
			gettext_noop("Logs the host name in the connection logs."),
			gettext_noop("By default, connection logs only show the IP address "
						 "of the connecting host. If you want them to show the host name you "
			  "can turn this on, but depending on your host name resolution "
			   "setup it might impose a non-negligible performance penalty.")
		},
		&log_hostname,
		false, NULL, NULL
	},
	{
		{"sql_inheritance", PGC_USERSET, COMPAT_OPTIONS_PREVIOUS,
			gettext_noop("Causes subtables to be included by default in various commands."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&SQL_inheritance,
		true, NULL, NULL
	},
	{
		{"password_encryption", PGC_USERSET, CONN_AUTH_SECURITY,
			gettext_noop("Encrypt passwords."),
			gettext_noop("When a password is specified in CREATE USER or "
			   "ALTER USER without writing either ENCRYPTED or UNENCRYPTED, "
						 "this parameter determines whether the password is to be encrypted.")
		},
		&Password_encryption,
		true, NULL, NULL
	},
	{
		{"transform_null_equals", PGC_USERSET, COMPAT_OPTIONS_CLIENT,
			gettext_noop("Treats \"expr=NULL\" as \"expr IS NULL\"."),
			gettext_noop("When turned on, expressions of the form expr = NULL "
			   "(or NULL = expr) are treated as expr IS NULL, that is, they "
				"return true if expr evaluates to the null value, and false "
			   "otherwise. The correct behavior of expr = NULL is to always "
						 "return null (unknown).")
		},
		&Transform_null_equals,
		false, NULL, NULL
	},
	{
		{"db_user_namespace", PGC_SIGHUP, CONN_AUTH_SECURITY,
			gettext_noop("Enables per-database user names."),
			NULL
		},
		&Db_user_namespace,
		false, NULL, NULL
	},
	{
		/* only here for backwards compatibility */
		{"autocommit", PGC_USERSET, COMPAT_OPTIONS_IGNORED,
			gettext_noop("This parameter doesn't do anything."),
			gettext_noop("It's just here so that we won't choke on SET AUTOCOMMIT TO ON from 7.3-vintage clients."),
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&phony_autocommit,
		true, assign_phony_autocommit, NULL
	},
	{
		{"default_transaction_read_only", PGC_USERSET, CLIENT_CONN_STATEMENT,
			gettext_noop("Sets the default read-only status of new transactions."),
			NULL
		},
		&DefaultXactReadOnly,
		false, NULL, NULL
	},
	{
		{"transaction_read_only", PGC_USERSET, CLIENT_CONN_STATEMENT,
			gettext_noop("Sets the current transaction's read-only status."),
			NULL,
			GUC_NO_RESET_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&XactReadOnly,
		false, assign_transaction_read_only, NULL
	},
	{
		{"add_missing_from", PGC_USERSET, COMPAT_OPTIONS_PREVIOUS,
			gettext_noop("Automatically adds missing table references to FROM clauses."),
			NULL
		},
		&add_missing_from,
		false, NULL, NULL
	},
	{
		{"check_function_bodies", PGC_USERSET, CLIENT_CONN_STATEMENT,
			gettext_noop("Check function bodies during CREATE FUNCTION."),
			NULL
		},
		&check_function_bodies,
		true, NULL, NULL
	},
	{
		{"array_nulls", PGC_USERSET, COMPAT_OPTIONS_PREVIOUS,
			gettext_noop("Enable input of NULL elements in arrays."),
			gettext_noop("When turned on, unquoted NULL in an array input "
						 "value means a null value; "
						 "otherwise it is taken literally.")
		},
		&Array_nulls,
		true, NULL, NULL
	},
	{
		{"default_with_oids", PGC_USERSET, COMPAT_OPTIONS_PREVIOUS,
			gettext_noop("Create new tables with OIDs by default."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&default_with_oids,
		false, NULL, NULL
	},
	{
		{"redirect_stderr", PGC_POSTMASTER, DEFUNCT_OPTIONS,
			gettext_noop("Defunct: start a subprocess to capture stderr output into log files."),
		 	NULL,
		 	GUC_NO_SHOW_ALL
		},
		&Redirect_stderr,
		true, NULL, NULL
	},
	{
		{"log_truncate_on_rotation", PGC_SIGHUP, LOGGING_WHERE,
			gettext_noop("Truncate existing log files of same name during log rotation."),
			NULL
		},
		&Log_truncate_on_rotation,
		false, NULL, NULL
	},

	{
		{"trace_sort", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Emit information about resource usage in sorting."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&trace_sort,
		false, NULL, NULL
	},

#ifdef WAL_DEBUG
	{
		{"wal_debug", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Emit WAL-related debugging output."),
			NULL,
			GUC_NOT_IN_SAMPLE
		},
		&XLOG_DEBUG,
		false, NULL, NULL
	},
#endif

	{
		{"integer_datetimes", PGC_INTERNAL, PRESET_OPTIONS,
			gettext_noop("Datetimes are integer based."),
			NULL,
			GUC_REPORT | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&integer_datetimes,
#ifdef HAVE_INT64_TIMESTAMP
		true, NULL, NULL
#else
		false, NULL, NULL
#endif
	},

	{
		{"krb_caseins_users", PGC_POSTMASTER, CONN_AUTH_SECURITY,
			gettext_noop("Sets whether Kerberos user names should be treated as case-insensitive."),
			NULL
		},
		&pg_krb_caseins_users,
		false, NULL, NULL
	},

	{
		{"escape_string_warning", PGC_USERSET, COMPAT_OPTIONS_PREVIOUS,
			gettext_noop("Warn about backslash escapes in ordinary string literals."),
			NULL
		},
		&escape_string_warning,
		true, NULL, NULL
	},

	{
		{"standard_conforming_strings", PGC_USERSET, COMPAT_OPTIONS_PREVIOUS,
			gettext_noop("'...' strings treat backslashes literally."),
			NULL,
			GUC_REPORT
		},
		&standard_conforming_strings,
		false, NULL, NULL
	},

    {
		{"enable_agg_restructure", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Deprecated: use gp_enable_multiphase_agg."),
			NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&defunct_bool,
		true, NULL, NULL
 	},

	{
		/* MPP-9772, MPP-9773: remove support for CREATE INDEX CONCURRENTLY */
		{"gp_create_index_concurrently", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Allow concurrent index creation."),
			NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_create_index_concurrently,
		false, NULL, NULL
	},

	{
		{"gp_enable_multiphase_agg", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Enables the planner's use of two- or three-stage parallel aggregation plans."),
			gettext_noop("Allows partial aggregation before motion.")
		},
		&gp_enable_multiphase_agg,
		true, NULL, NULL
	},

	{
		{"gp_enable_hash_partitioned_tables", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable hash partitioned tables."),
			NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_enable_hash_partitioned_tables,
		false, NULL, NULL
	},

	{
		{"gp_foreign_data_access", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable SQL/MED operation."),
			NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_foreign_data_access,
		false, NULL, NULL
	},

	{
		{"gp_setwith_alter_storage", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Let SET WITH alter the storage options."),
			NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_setwith_alter_storage,
		false, NULL, NULL
	},

	{
		{"gp_enable_tablespace_auto_mkdir", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable tablespace code to create empty directory if necessary"),
			NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_enable_tablespace_auto_mkdir,
		false, NULL, NULL
	},

	{
		{"gp_enable_preunique", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Enable 2-phase duplicate removal."),
			gettext_noop("If true, planner may choose to remove duplicates in "
						 "two phases--before and after redistribution.")
		},
		&gp_enable_preunique,
		true, NULL, NULL
	},

	{
		{"gp_eager_preunique", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Experimental feature: 2-phase duplicate removal - cost override."),
			NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_eager_preunique,
		false, NULL, NULL
	},

	{
		{"gp_enable_sequential_window_plans", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Experimental feature: Enable non-parallel window plans."),
			gettext_noop("The planner will evaluate window functions associated with separate "
						 "window specifications sequentially rather that in parallel.")
		},
		&gp_enable_sequential_window_plans,
		true, NULL, NULL
	},

	{
		{"gp_hashagg_recalc_density", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("(Obsolete) Executor can recalculate grouping density based on pre-spill real density."),
			NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&defunct_bool,
		true, NULL, NULL
	},

	{
		{"gp_enable_agg_distinct", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Enable 2-phase aggregation to compute a single distinct-qualified aggregate."),
			NULL,
		},
		&gp_enable_agg_distinct,
		true, NULL, NULL
	},

	{
		{"gp_enable_agg_distinct_pruning", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Enable 3-phase aggregation and join to compute distinct-qualified aggregates."),
			NULL,
		},
		&gp_enable_dqa_pruning,
		true, NULL, NULL
	},

	{
		{"gp_enable_groupext_distinct_pruning", PGC_USERSET, QUERY_TUNING_METHOD,
		     gettext_noop("Enable 3-phase aggregation and join to compute distinct-qualified aggregates"
						  " on grouping extention queries."),
		     NULL,
		},
		&gp_enable_groupext_distinct_pruning,
		true, NULL, NULL
	},

	{
		{"gp_enable_groupext_distinct_gather", PGC_USERSET, QUERY_TUNING_METHOD,
		     gettext_noop("Enable gathering data to a single node to compute distinct-qualified aggregates"
						  " on grouping extention queries."),
		     NULL,
		},
		&gp_enable_groupext_distinct_gather,
		true, NULL, NULL
	},

	{
		{"gp_eager_agg_distinct_pruning", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Prefer 3-phase aggregation [and join] to compute distinct-qualified aggregates."),
			gettext_noop("The planner will prefer to use 3-phase aggregation and join to compute "
						 "distinct-qualified aggregates whenever enabled and possible"
						 "regardless of cost estimates."),
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_eager_dqa_pruning,
		false, NULL, NULL
	},

	{
		{"gp_eager_one_phase_agg", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Prefer 1-phase aggregation."),
			gettext_noop("The planner will prefer to use 1-phase aggregation whenever possible"
						 "regardless of cost estimates."),
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_eager_one_phase_agg,
		false, NULL, NULL
	},

	{
		{"gp_eager_two_phase_agg", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Prefer 2-phase aggregation."),
			gettext_noop("The planner will prefer to use 2-phase aggregation whenever"
						 "enabled and possible regardless of cost estimates."),
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_eager_two_phase_agg,
		false, NULL, NULL
	},

	{
		{"gp_dev_notice_agg_cost", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Trace aggregate costing decisions as NOTICEs."),
			NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_dev_notice_agg_cost,
		false, NULL, NULL
	},

	{
		{"gp_enable_explain_allstat", PGC_USERSET, CLIENT_CONN_OTHER,
			gettext_noop("Experimental feature: dump stats for all segments in EXPLAIN ANALYZE."),
			NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_enable_explain_allstat,
		false, NULL, NULL
	},

	{
		{"gp_dump_memory_usage", PGC_USERSET, CLIENT_CONN_OTHER,
			gettext_noop("Save memory usage in each segment."),
			NULL,
			GUC_GPDB_ADDOPT | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_dump_memory_usage,
		false, NULL, NULL
	},

	{
		{"gp_enable_sort_limit", PGC_USERSET, QUERY_TUNING_METHOD,
            gettext_noop("Enable LIMIT operation to be performed while sorting."),
			gettext_noop("Sort more efficiently when plan requires the first <n> rows at most.")
		},
		&gp_enable_sort_limit,
		true, NULL, NULL
	},

	{
		{"gp_enable_sort_distinct", PGC_USERSET, QUERY_TUNING_METHOD,
            gettext_noop("Enable duplicate removal to be performed while sorting."),
            gettext_noop("Reduces data handling when plan calls for removing duplicates from sorted rows.")
		},
		&gp_enable_sort_distinct,
		true, NULL, NULL
	},

	{
		{"gp_parquet_insert_sort", PGC_USERSET, RESOURCES_MEM,
			gettext_noop("Enable sorting of tuples during insertion in parquet partitioned tables."),
			gettext_noop("Reduces memory usage required for insertion by keeping on part open at a time"),
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT

		},
		&gp_parquet_insert_sort,
		true, NULL, NULL
	},

	{
		{"gp_enable_mk_sort", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Enable multi-key sort."),
			gettext_noop("A faster sort."),
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT

		},
		&gp_enable_mk_sort,
		true, NULL, NULL
	},

	{
		{"gp_enable_motion_mk_sort", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Enable multi-key sort in sorted motion recv."),
			gettext_noop("A faster sort for recv motion"),
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT

		},
		&gp_enable_motion_mk_sort,
		true, NULL, NULL
	},


#ifdef USE_ASSERT_CHECKING
	{
		{"gp_mk_sort_check", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Extensive check mk_sort"),
			gettext_noop("Expensive debug checking"),
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_mk_sort_check,
		false, NULL, NULL
	},
#endif

	{
		{"gp_hashagg_streambottom", PGC_USERSET, QUERY_TUNING_METHOD,
            gettext_noop("Stream the bottom stage of two stage hashagg"),
            gettext_noop("Avoid spilling at the bottom stage of two stage hashagg"),
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_hashagg_streambottom,
		true, NULL, NULL
	},

	{
		{"gp_enable_motion_deadlock_sanity", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable verbose check at planning time."),
			NULL,
            GUC_NO_RESET_ALL | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_enable_motion_deadlock_sanity,
		false, NULL, NULL
	},

	{
		{"gp_adjust_selectivity_for_outerjoins", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Adjust selectivity of null tests over outer joins."),
			NULL,
			GUC_NOT_IN_SAMPLE
		},
		&gp_adjust_selectivity_for_outerjoins,
		true, NULL, NULL
	},

	{
		{"gp_selectivity_damping_for_scans", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Damping of selectivities for clauses over the same base relation."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
  		},
		&gp_selectivity_damping_for_scans,
		true, NULL, NULL
	},

	{
		{"gp_selectivity_damping_for_joins", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Damping of selectivities in join clauses."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
  		},
		&gp_selectivity_damping_for_joins,
		false, NULL, NULL
	},

	{
		{"gp_selectivity_damping_sigsort", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Sort selectivities by ascending significance, i.e. smallest first"),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
  		},
		&gp_selectivity_damping_sigsort,
		true, NULL, NULL
	},

	{
		{"gp_enable_interconnect_aggressive_retry", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable application-level fast-track interconnect retries"),
			NULL,
            GUC_NO_RESET_ALL | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_interconnect_aggressive_retry,
		true, NULL, NULL
	},
	{
	    {"gp_crash_recovery_abort_suppress_fatal", PGC_SUSET, DEVELOPER_OPTIONS,
	            gettext_noop("Warning about crash recovery abort transaction issue"),
	            NULL,
	            GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
	    },
	    &gp_crash_recovery_abort_suppress_fatal,
	    false, NULL, NULL
    },
	{
		{"gp_persistent_statechange_suppress_error", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Warning about persistent state-change issue"),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&gp_persistent_statechange_suppress_error,
		false, NULL, NULL
	},
	{
		{"gp_select_invisible", PGC_USERSET, DEVELOPER_OPTIONS,
		 gettext_noop("Use dummy snapshot for MVCC visibility calculation."),
		 NULL,
		 GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL | GUC_GPDB_ADDOPT
		},
		&gp_select_invisible,
		false, NULL, NULL
	},

	{
		{"gp_enable_slow_writer_testmode", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Slow down writer gangs -- to facilitate race-condition testing."),
			NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_enable_slow_writer_testmode,
		false, NULL, NULL
	},

	{
		{"gp_enable_slow_cursor_testmode", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Slow down cursor gangs -- to facilitate race-condition testing."),
			NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_enable_slow_cursor_testmode,
		false, NULL, NULL
	},

	{
		{"gp_fts_probe_pause", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Stop active probes."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_fts_probe_pause,
		false, gpvars_assign_gp_fts_probe_pause, NULL
	},

	{
		{"gp_fts_transition_parallel", PGC_POSTMASTER, GP_ARRAY_TUNING,
			gettext_noop("Activate parallel segment transition."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_fts_transition_parallel,
		true, NULL, NULL
	},

	{
		{"gp_debug_pgproc", PGC_POSTMASTER, DEVELOPER_OPTIONS,
			gettext_noop("Print debug info relevant to PGPROC."),
		 	NULL /* long description */,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_debug_pgproc,
		false, NULL, NULL
	},

	{
		{"Debug_print_combocid_detail", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("When running into combocid limit, emit detailed snapshot information."),
			NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_print_combocid_detail,
		false, NULL, NULL
	},

	{
		{"gp_appendonly_verify_block_checksums", PGC_USERSET, DEVELOPER_OPTIONS,
		 gettext_noop("Verify the append-only block checksum when reading."),
		 NULL,
		 GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&gp_appendonly_verify_block_checksums,
		true, NULL, NULL
	},

	{
		{"gp_appendonly_verify_write_block", PGC_USERSET, DEVELOPER_OPTIONS,
		 gettext_noop("Verify the append-only block as it is being written."),
		 NULL,
		 GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&gp_appendonly_verify_write_block,
		false, NULL, NULL
	},

	{
		{"gp_appendonly_verify_eof", PGC_USERSET, DEVELOPER_OPTIONS,
		 gettext_noop("Verify append-only eof integrity before writing."),
		 NULL,
		 GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&gp_appendonly_verify_eof,
		true, NULL, NULL
	},

	{
		{"gp_appendonly_compaction", PGC_SUSET, APPENDONLY_TABLES,
		 gettext_noop("Perform append-only compaction instead of eof truncation on vacuum."),
		 NULL,
		 GUC_SUPERUSER_ONLY | GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&gp_appendonly_compaction,
		true, NULL, NULL
	},

	{
		{"gp_heap_require_relhasoids_match", PGC_USERSET, DEVELOPER_OPTIONS,
		 gettext_noop("Issue an error on discovery of a mismatch between relhasoids and a tuple header."),
		 NULL,
		 GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&gp_heap_require_relhasoids_match,
		true, NULL, NULL
	},

	{
		{"debug_appendonly_rezero_quicklz_compress_scratch", PGC_USERSET, DEVELOPER_OPTIONS,
		 gettext_noop("Zero the QuickLZ scratch buffer before each append-only block that is being compressed."),
		 NULL,
		 GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&Debug_appendonly_rezero_quicklz_compress_scratch,
		false, NULL, NULL
	},

	{
		{"debug_appendonly_guard_end_quicklz_scratch", PGC_USERSET, DEVELOPER_OPTIONS,
		 gettext_noop("Put a guard area of all zeroes after the QuickLZ scratch buffers and verify it is still zero before and after compress and decompress operations."),
		 NULL,
		 GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&Debug_appendonly_guard_end_quicklz_scratch,
		false, NULL, NULL
	},

	{
		{"debug_appendonly_rezero_quicklz_decompress_scratch", PGC_USERSET, DEVELOPER_OPTIONS,
		 gettext_noop("Zero the QuickLZ scratch buffer before each append-only block that is being decompressed."),
		 NULL,
		 GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&Debug_appendonly_rezero_quicklz_decompress_scratch,
		false, NULL, NULL
	},

	{
		{"gp_cost_hashjoin_chainwalk", PGC_USERSET, QUERY_TUNING_COST,
			gettext_noop("Enable the cost for walking the chain in the hash join"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_cost_hashjoin_chainwalk,
		false, NULL, NULL
	},

    {
        {"gp_set_read_only", PGC_SUSET, GP_ARRAY_CONFIGURATION,
		 gettext_noop("Sets the system read only"),
		 NULL,
		 GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
        },
        &gp_set_read_only,
        false, NULL, NULL
    },

	{
		{"gp_set_proc_affinity", PGC_POSTMASTER, RESOURCES_KERNEL,
		 gettext_noop("On postmaster startup, attempt to bind postmaster to a processor"),
		 NULL
		},
		&gp_set_proc_affinity,
        false, NULL, NULL
	},

    {
        {"gp_enable_adaptive_nestloop", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Enables the planner's use of the Adaptive Join Choice operator."),
            gettext_noop("During query execution, actual outer row count determines choice of Nested or Hash Join.")
        },
        &enable_adaptive_nestloop,
        true, NULL, NULL
    },

    {
    	{"gp_is_writer", PGC_BACKEND, GP_WORKER_IDENTITY,
    		gettext_noop("True in a worker process which can directly update its local database segment."),
    		NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
    	},
    	&Gp_is_writer,
    	false, NULL, NULL
    },

	{
		{"gp_write_shared_snapshot", PGC_USERSET, UNGROUPED,
			gettext_noop("Forces the writer gang to set the shared snapshot."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NO_RESET_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
    	&Gp_write_shared_snapshot,
    	false, assign_gp_write_shared_snapshot, NULL
    },

    {
    	{"gp_enable_functions", PGC_USERSET, DEFUNCT_OPTIONS,
    		gettext_noop("Enable functions that must use the callback from the QE to QD."),
    		NULL,
				GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
    	},
    	&defunct_bool,
    	false, NULL, NULL
    },

    {
    	{"gp_use_snapshop_during_callback", PGC_USERSET, CLIENT_CONN_STATEMENT,
    		gettext_noop("For debugging.. If SPI should try to send snapshot on callback to QD."),
    		NULL,
    		GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
    	},
    	&gp_use_snapshop_during_callback,
    	true, NULL, NULL
    },

    {
    	{"gp_is_callback", PGC_BACKEND, GP_WORKER_IDENTITY,
    		gettext_noop("True in a worker process executing on behalf of a callback from the QE to QD."),
    		NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE | GUC_GPDB_ADDOPT
    	},
    	&gp_is_callback,
    	false, NULL, NULL
    },

    {
    	{"gp_use_dispatch_agent", PGC_USERSET, DEVELOPER_OPTIONS,
    		gettext_noop("True says to use Query Dispatch Agents to improve dispatching."),
    		NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_NO_RESET_ALL
    	},
    	&gp_use_dispatch_agent,
    	false, assign_gp_use_dispatch_agent, NULL
    },

    {
    	{"gp_reraise_signal", PGC_SUSET, DEVELOPER_OPTIONS,
    		gettext_noop("Do we attempt to dump core when a serious problem occurs."),
    		NULL,
    		GUC_NO_RESET_ALL
    	},
    	&gp_reraise_signal,
    	true, NULL, NULL
    },

    {
        {"gp_backup_directIO", PGC_USERSET, DEVELOPER_OPTIONS,
                gettext_noop("Enable direct IO dump"),
                NULL
        },
        &gp_backup_directIO,
        false, NULL, NULL
    },

    {
        {"gp_external_enable_exec", PGC_POSTMASTER, EXTERNAL_TABLES,
                gettext_noop("Enable selecting from an external table with an EXECUTE clause."),
                NULL
        },
        &gp_external_enable_exec,
        true, NULL, NULL
    },

    {
        {"gp_enable_fast_sri", PGC_USERSET, QUERY_TUNING_OTHER,
                gettext_noop("Enable single-slice single-row inserts."),
                NULL
        },
        &gp_enable_fast_sri,
        true, NULL, NULL
    },

	{
		{"gp_interconnect_full_crc", PGC_USERSET, QUERY_TUNING_OTHER,
			gettext_noop("Sanity check incoming data stream."),
			NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_interconnect_full_crc,
		false, NULL, NULL
	},

	{
		{"gp_interconnect_elide_setup", PGC_USERSET, DEPRECATED_OPTIONS,
			gettext_noop("Avoid performing full startup handshake for every statement."),
			NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_interconnect_elide_setup,
		true, NULL, NULL
	},

	{
		{"gp_interconnect_log_stats", PGC_USERSET, QUERY_TUNING_OTHER,
			gettext_noop("Emit statistics from the UDP-IC at the end of every statement."),
			NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_interconnect_log_stats,
		false, NULL, NULL
	},

	{
		{"gp_interconnect_cache_future_packets", PGC_USERSET, GP_ARRAY_TUNING,
            gettext_noop("Control whether future packets are cached."),
			NULL,
        },
        &gp_interconnect_cache_future_packets,
		true, NULL, NULL
	},

	{
		{"gp_version_mismatch_error", PGC_USERSET, QUERY_TUNING_OTHER,
			gettext_noop("QD/QE version string mismatches reported as an error"),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&gp_version_mismatch_error,
		true, NULL, NULL
    },

	{
		{"gp_external_grant_privileges", PGC_POSTMASTER, EXTERNAL_TABLES,
			gettext_noop("Enable non superusers to create http or gpfdist external tables."),
			NULL
		},
		&gp_external_grant_privileges,
		false, NULL, NULL
    },

	{
		{"resource_scheduler", PGC_POSTMASTER, RESOURCES_MGM,
			gettext_noop("Enable resource scheduling."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&ResourceScheduler,
		true, NULL, NULL
	},
	{
		{"resource_select_only", PGC_POSTMASTER, RESOURCES_MGM,
			gettext_noop("Enable resource locking of SELECT only."),
			NULL
		},
		&ResourceSelectOnly,
		false, NULL, NULL
	},
	{
		{"resource_cleanup_gangs_on_wait", PGC_USERSET, RESOURCES_MGM,
			gettext_noop("Enable idle gang cleanup before resource lockwait."),
			NULL
		},
		&ResourceCleanupIdleGangs,
		true, NULL, NULL
	},

	{
		{"gp_debug_resqueue_priority", PGC_USERSET, RESOURCES_MGM,
			gettext_noop("Print out debugging information about backoff calls."),
			NULL,
			GUC_NO_SHOW_ALL
			},
		&gp_debug_resqueue_priority,
		false, NULL, NULL
	},

	{
		{"ignore_system_indexes", PGC_BACKEND, DEVELOPER_OPTIONS,
			gettext_noop("Disables reading from system indexes."),
			gettext_noop("It does not prevent updating the indexes, so it is safe "
						 "to use.  The worst consequence is slowness."),
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&IgnoreSystemIndexes,
		false, NULL, NULL
	},

	{
		{"debug_print_full_dtm", PGC_SUSET, LOGGING_WHAT,
			gettext_noop("Prints full DTM information to server log."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_print_full_dtm,
		false, NULL, NULL
	},

	{
		{"debug_print_snapshot_dtm", PGC_SUSET, LOGGING_WHAT,
			gettext_noop("Prints snapshot DTM information to server log."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_print_snapshot_dtm,
		false, NULL, NULL
	},

	{
		{"debug_print_qd_mirroring", PGC_SUSET, LOGGING_WHAT,
			gettext_noop("Prints QD mirroring information to server log."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_print_qd_mirroring,
		false, NULL, NULL
	},

	{
		{"debug_permit_same_host_standby", PGC_SUSET, LOGGING_WHAT,
			gettext_noop("Permits QD mirroring standby to exist on same host as primary."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_permit_same_host_standby,
		false, NULL, NULL
	},

	{
		{"Debug_print_semaphore_detail", PGC_SUSET, LOGGING_WHAT,
			gettext_noop("Print semaphore detailed information."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_print_semaphore_detail,
		false, NULL, NULL
	},

	{
		{"debug_disable_distributed_snapshot", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Disables distributed snapshots."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_disable_distributed_snapshot,
		false, NULL, NULL
	},

	{
		{"debug_abort_after_distributed_prepared", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Cause an abort after all segments are prepared but before the distributed commit is written."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_abort_after_distributed_prepared,
		false, NULL, NULL
	},

	{
		{"debug_abort_after_segment_prepared", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Cause an abort after segment has written prepared XLOG record."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_abort_after_segment_prepared,
		false, NULL, NULL
	},

	{
		{"debug_print_tablespace", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print TABLESPACE debugging information."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_print_tablespace,
		false, NULL, NULL
	},

	{
		{"debug_appendonly_print_blockdirectory", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print log messages for append-only block directory."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_appendonly_print_blockdirectory,
		false, NULL, NULL
	},

	{
		{"Debug_appendonly_print_read_block", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print log messages for append-only reads."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_appendonly_print_read_block,
		false, NULL, NULL
	},

	{
		{"Debug_appendonly_print_append_block", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print log messages for append-only writes."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_appendonly_print_append_block,
		false, NULL, NULL
	},

	{
		{"debug_appendonly_print_visimap", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print log messages for append-only visibility bitmap information."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_appendonly_print_visimap,
		false, NULL, NULL
	},

	{
		{"debug_appendonly_print_compaction", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print log messages about append-only visibility compactions."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_appendonly_print_compaction,
		false, NULL, NULL
	},

	{
		{"debug_appendonly_print_insert", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print log messages for append-only insert."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_appendonly_print_insert,
		false, NULL, NULL
	},

	{
		{"debug_appendonly_print_insert_tuple", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print log messages for append-only insert tuples (caution -- generates a lot of log!)."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_appendonly_print_insert_tuple,
		false, NULL, NULL
	},

	{
		{"debug_appendonly_print_scan", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print log messages for append-only scan."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_appendonly_print_scan,
		false, NULL, NULL
	},

	{
		{"debug_appendonly_print_scan_tuple", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print log messages for append-only scan tuples (caution -- generates a lot of log!)."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_appendonly_print_scan_tuple,
		false, NULL, NULL
	},

	{
		{"debug_appendonly_print_delete", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print log messages for append-only delete."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_appendonly_print_delete,
		false, NULL, NULL
	},

	{
		{"debug_appendonly_print_update", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print log messages for append-only update."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_appendonly_print_update,
		false, NULL, NULL
	},

	{
		{"debug_appendonly_print_update_tuple", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print log messages for append-only update tuples (caution -- generates a lot of log!)."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_appendonly_print_update_tuple,
		false, NULL, NULL
	},

	{
		{"debug_appendonly_print_storage_headers", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print log messages for append-only storage headers."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_appendonly_print_storage_headers,
		false, NULL, NULL
	},

	{
		{"debug_appendonly_print_verify_write_block", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print log messages for append-only verify block during write."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_appendonly_print_verify_write_block,
		false, NULL, NULL
	},

	{
		{"debug_appendonly_use_no_toast", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Use no toast for an append-only table.  Store the large row inline."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_appendonly_use_no_toast,
		false, NULL, NULL
	},

	{
		{"debug_appendonly_print_segfile_choice", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print log messages for append-only writers about their choice for AO segment file."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_appendonly_print_segfile_choice,
		false, NULL, NULL
	},


	{
		{"debug_appendonly_print_datumstream", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print log messages for append-only datum stream content."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_appendonly_print_datumstream,
		false, NULL, NULL
	},

	{
		{"debug_gp_relation_node_fetch_wait_for_debugging", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Wait for debugger attach for MPP-16395 RelationFetchGpRelationNodeForXLog issue."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_gp_relation_node_fetch_wait_for_debugging,
		false, NULL, NULL
	},

	{
		{"debug_xlog_insert_print", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print XLOG Insert record debugging information."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_xlog_insert_print,
		false, NULL, NULL
	},

	{
		{"debug_xlog_record_read", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print debug information for xlog record read."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&debug_xlog_record_read,
		false, NULL, NULL
	},

	{
		{"debug_persistent_print", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print persistent file-system object debugging information."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_persistent_print,
		false, NULL, NULL
	},

	{
		{"debug_persistent_recovery_print", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print persistent recovery debugging information."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_persistent_recovery_print,
		false, NULL, NULL
	},

	{
		{"Disable_persistent_recovery_logging", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("By default important persistent recovery information is logged, setting this GUC allows to disable it if required"),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Disable_persistent_recovery_logging,
		true, NULL, NULL
	},

	{
		{"debug_persistent_store_print", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print persistent file-system object store debugging information."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_persistent_store_print,
		false, NULL, NULL
	},

	{
		{"debug_persistent_bootstrap_print", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print persistent store debugging information during bootstrap."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_persistent_bootstrap_print,
		false, NULL, NULL
	},

	{
		{"persistent_integrity_checks", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("When set enables all set of integrity checks for persistent tables."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&persistent_integrity_checks,
		false, NULL, NULL
	},

	{
		{"disable_persistent_diagnostic_dump", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("When set disables printing full PT on erros."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&disable_persistent_diagnostic_dump,
		true, NULL, NULL
	},

	{
		{"debug_persistent_ptcat_verification", PGC_SUSET, DEVELOPER_OPTIONS,
				gettext_noop("When set, it enables all set of cross catalog consistency integrity checks for persistent tables in Startup pass 4."),
				NULL,
				GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&debug_persistent_ptcat_verification,
		false, NULL, NULL
	},

	{
		{"debug_print_persistent_checks", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("When set enables printing of logs in integrity checks for persistent tables module."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&debug_print_persistent_checks,
		false, NULL, NULL
	},

	{
		{"Debug_bulk_load_bypass_wal", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Use new bulk load bypass WAL logic."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_bulk_load_bypass_wal,
		true, NULL, NULL
	},

	{
		{"debug_persistent_appendonly_commit_count_print", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print persistent Append-Only resync commit count information."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_persistent_appendonly_commit_count_print,
		true, NULL, NULL
	},

	{
		{"debug_cancel_print", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print cancel detail information."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_cancel_print,
		false, NULL, NULL
	},

	{
		{"debug_datumstream_write_print_small_varlena_info", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print datum stream write small varlena information."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_datumstream_write_print_small_varlena_info,
		false, NULL, NULL
	},

	{
		{"debug_datumstream_write_print_large_varlena_info", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print datum stream write large varlena information."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_datumstream_write_print_large_varlena_info,
		false, NULL, NULL
	},

	{
		{"debug_datumstream_read_check_large_varlena_integrity", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Check datum stream large object integrity."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_datumstream_read_check_large_varlena_integrity,
		false, NULL, NULL
	},

	{
		{"debug_datumstream_block_read_check_integrity", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Check datum stream block read integrity."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_datumstream_block_read_check_integrity,
		false, NULL, NULL
	},

	{
		{"debug_datumstream_block_write_check_integrity", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Check datum stream block write integrity."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_datumstream_block_write_check_integrity,
		false, NULL, NULL
	},

	{
		{"debug_datumstream_read_print_varlena_info", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print datum stream read varlena information."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_datumstream_read_print_varlena_info,
		false, NULL, NULL
	},

	{
		{"debug_datumstream_write_use_small_initial_buffers", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Use small datum stream write buffers to stress growing logic."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_datumstream_write_use_small_initial_buffers,
		false, NULL, NULL
	},

	{
		{"debug_database_command_print", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print database command debugging information."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_database_command_print,
		false, NULL, NULL
	},

	{
		{"gp_initdb_mirrored", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Indicate we are initializing a mirrored cluster during initdb."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_initdb_mirrored,
		false, NULL, NULL
	},

	{
		{"gp_before_persistence_work", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Indicate we are initializing / upgrading and do not want to do persistence work yet."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_before_persistence_work,
		false, NULL, NULL
	},

	{
		{"gp_before_filespace_setup", PGC_POSTMASTER, DEVELOPER_OPTIONS,
			gettext_noop("Indicates that the gp_persistent_filespace_node table is not setup and should not be used for lookups."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_before_filespace_setup,
		false, NULL, NULL
	},

	{
		{"gp_startup_integrity_checks", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Perform integrity checks after performing startup but before allowing connections in."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_startup_integrity_checks,
		true, NULL, NULL
	},

	{
		{"debug_print_xlog_relation_change_info", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print relation change information"),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_print_xlog_relation_change_info,
		false, NULL, NULL
	},

	{
		{"debug_print_xlog_relation_change_info_skip_issues_only", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print relation change information only when there is a skip issue"),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_print_xlog_relation_change_info_skip_issues_only,
		false, NULL, NULL
	},

	{
		{"debug_print_xlog_relation_change_info_backtrace_skip_issues", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print relation change information backtrace when there is a skip issue"),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_print_xlog_relation_change_info_backtrace_skip_issues,
		false, NULL, NULL
	},

	{
		{"debug_check_for_invalid_persistent_tid", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Check for invalid persistent TID"),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_check_for_invalid_persistent_tid,
		false, NULL, NULL
	},

	{
		{"test_appendonly_override", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("For testing purposes, change the default of the appendonly create table option."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Test_appendonly_override,
		false, NULL, NULL
	},

	{
		{"test_print_direct_dispatch_info", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("For testing purposes, print information about direct dispatch decisions."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Test_print_direct_dispatch_info,
		false, NULL, NULL
	},

	{
		{"debug_bitmap_print_insert", PGC_SUSET, DEVELOPER_OPTIONS,
		 gettext_noop("Print log messages for bitmap index insert routines (caution-- generate a lot of logs!)"),
		 NULL,
		 GUC_SUPERUSER_ONLY | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_bitmap_print_insert,
		false, NULL, NULL
	},

	{
		{"gp_test_orientation_override", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("For testing purposes, change the default of the orientation to column."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_test_orientation_override,
		false, NULL, NULL
	},

	{
		{"gp_permit_persistent_metadata_update", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Permit updates to persistent metadata tables."),
			gettext_noop("For system repair by experts."),
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_permit_persistent_metadata_update,
		false, NULL, NULL
	},

	{
		{"gp_permit_relation_node_change", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Permit updates to gp_relation_node tables."),
			gettext_noop("For system repair by experts."),
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_permit_relation_node_change,
		false, NULL, NULL
	},

	{
		{"master_mirroring_administrator_disable", PGC_POSTMASTER, GP_ARRAY_CONFIGURATION,
			gettext_noop("Used by gpstart to indicate the standby master was not started by the administrator."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Master_mirroring_administrator_disable,
		false, NULL, NULL
	},

	{
		{"debug_dtm_action_primary", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Specify if the primary or mirror segment is the target of the debug DTM action."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_dtm_action_primary,
		DEBUG_DTM_ACTION_PRIMARY_DEFAULT, NULL, NULL
	},

	{
		{"gp_disable_tuple_hints", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Specify if reader should set hint bits on tuples."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_disable_tuple_hints,
		true, NULL, NULL
	},
 	{
 		{"debug_print_server_processes", PGC_SUSET, LOGGING_WHAT,
 			gettext_noop("Prints server process management to server log."),
 			NULL,
 			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
 		},
 		&Debug_print_server_processes,
 		false, NULL, NULL
 	},

 	{
 		{"debug_print_control_checkpoints", PGC_SUSET, LOGGING_WHAT,
 			gettext_noop("Prints pg_control file checkpoint changes to server log."),
 			NULL,
 			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
 		},
 		&Debug_print_control_checkpoints,
 		false, NULL, NULL
 	},

	{
		{"gp_local_distributed_cache_stats", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Prints local-distributed cache statistics at end of commit / prepare."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_local_distributed_cache_stats,
		false, NULL, NULL
	},

	{
		{"enable_partition_rules", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable creation of RULEs to implement partitioning"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&enable_partition_rules,
		false, NULL, NULL
	},

	{
		{"gp_enable_gpperfmon", PGC_POSTMASTER, UNGROUPED,
			gettext_noop("Enable gpperfmon monitoring."),
			NULL,
		},
		&gp_enable_gpperfmon,
		false, gpvars_assign_gp_enable_gpperfmon, NULL
	},

	{
		{"gp_enable_alter_table_inherit_cols", PGC_USERSET, UNGROUPED,
			gettext_noop("Enable extended 'ALTER TABLE child INHERIT parent' syntax."),
			gettext_noop("The extended syntax adds an optional '( column_list )' identifying columns "
					"to be marked as fully-inherited from parent."),
			GUC_NO_SHOW_ALL | GUC_NO_RESET_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&gp_enable_alter_table_inherit_cols,
		false, NULL, NULL
	},

	{
		{"gp_mapreduce_define", PGC_USERSET, DEVELOPER_OPTIONS,
		 gettext_noop("Prepare mapreduce object creation"),  /* turn off statement logging */
		 NULL,
		 GUC_NO_SHOW_ALL | GUC_NO_RESET_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE | GUC_GPDB_ADDOPT
		},
		&gp_mapreduce_define,
		false, NULL, NULL
	},

	{
		{"coredump_on_memerror", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Generate core dump on memory error."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&coredump_on_memerror,
		false, NULL, NULL
	},
    {
		{"log_autostats", PGC_SUSET, LOGGING_WHAT,
		 gettext_noop("Logs details of auto-stats issued ANALYZEs."),
		 NULL
		 },
		&log_autostats,
		true, NULL, NULL
	},
	{
		{"gp_statistics_pullup_from_child_partition", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("This guc enables the planner to utilize statistics from partitions in planning queries on the parent."),
			NULL
		},
		&gp_statistics_pullup_from_child_partition,
		true, NULL, NULL
	},
	{
		{"gp_statistics_use_fkeys", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("This guc enables the planner to utilize statistics derived from foreign key relationships."),
			NULL
		},
		&gp_statistics_use_fkeys,
		true, NULL, NULL
	},

	{
		{"gp_eager_hashtable_release", PGC_USERSET, DEPRECATED_OPTIONS,
			gettext_noop("This guc determines if a hash-join eagerly releases its hash table."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_eager_hashtable_release,
		true, NULL, NULL
	},
	{
		{"gp_resqueue_priority", PGC_POSTMASTER, RESOURCES_MGM,
			gettext_noop("Enables priority scheduling."),
			NULL
		},
		&gp_enable_resqueue_priority,
		true, NULL, NULL
	},
	{
		{"gp_hash_index", PGC_SUSET, UNGROUPED,
			gettext_noop("Specify whether hash indexes can be used. Deprecated and has no effect."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_hash_index,
		false, gpvars_assign_gp_hash_index, NULL
	},

	{
		{"gp_change_tracking", PGC_SUSET, UNGROUPED,
			gettext_noop("Allows disabling change tracking."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_change_tracking,
		true, NULL, NULL
	},


	{
		{"gp_persistent_skip_free_list", PGC_SUSET, UNGROUPED,
			gettext_noop("Avoids using the persistent free list and always allocates a new tuple."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_persistent_skip_free_list,
		false, NULL, NULL
	},

	{
		{"gp_persistent_repair_global_sequence", PGC_SUSET, UNGROUPED,
			gettext_noop("Repair a global sequence number to use the maximum scanned value."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_persistent_repair_global_sequence,
		false, NULL, NULL
	},

	{
		{"filerep_crc_on", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("enable adler 32 crc in filerep"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_filerep_crc_on,
		false, NULL, NULL
	},
	{
		{"debug_rle_type_compression", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("enable extensive debugging information for rle_type compression"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_rle_type_compression,
		false, NULL, NULL
	},

	{
		{"rle_type_compression_stats", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("show compression ratio stats for rle_type compression"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&rle_type_compression_stats,
		false, NULL, NULL
	},

	{
		{"debug_filerep_print", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("enable filerep logs"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_filerep_print,
		false, NULL, NULL
	},

	{
		{"debug_filerep_gcov", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("workaround for filerep gcov issue"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_filerep_gcov,
		false, NULL, NULL
	},

	{
		{"debug_filerep_config_print", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("enable filerep config logs"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_filerep_config_print,
		false, NULL, NULL
	},

	{
		{"debug_filerep_verify_performance_print", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("enable filerep verify performance tracing"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_filerep_verify_performance_print,
		false, NULL, NULL
	},

	{
		{"debug_filerep_memory_log_flush", PGC_SIGHUP, DEVELOPER_OPTIONS,
			gettext_noop("enable filerep config logs"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_filerep_memory_log_flush,
		false, NULL, NULL
	},

	{
		{"filerep_inject_listener_fault", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("inject fault before filerep listener is started"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&filerep_inject_listener_fault,
		false, NULL, NULL
	},

	{
		{"filerep_inject_db_startup_fault", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("inject mirroring fault during database startup"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&filerep_inject_db_startup_fault,
		false, NULL, NULL
	},

	{
		{"filerep_inject_change_tracking_recovery_fault", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("inject fault during change tracking recovery"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&filerep_inject_change_tracking_recovery_fault,
		false, NULL, NULL
	},

	{
		{"filerep_mirrorvalidation_during_resync", PGC_POSTMASTER, DEVELOPER_OPTIONS,
			gettext_noop("Setting enables checking for file existence for all relations on mirror during incremental resynchronization"),
			NULL,
			GUC_NO_SHOW_ALL
		},
		&filerep_mirrorvalidation_during_resync,
		false, NULL, NULL
	},

	{
		{"debug_walrepl_snd", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print debug messages for WAL sender in WAL based replication (Master Mirroring)."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&debug_walrepl_snd,
		false, NULL, NULL
	},

	{
		{"debug_walrepl_syncrep", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print debug messages for synchronous behavior in WAL based replication (Master Mirroring)."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&debug_walrepl_syncrep,
		false, NULL, NULL
	},

	{
		{"debug_walrepl_rcv", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print debug messages for WAL receiver in WAL based replication (Master Mirroring)."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&debug_walrepl_rcv,
		false, NULL, NULL
	},

	{
		{"debug_basebackup", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print debug messages for basebackup mechanism (Master Mirroring)."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&debug_basebackup,
		false, NULL, NULL
	},

	{
		{"debug_latch", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Print debug messages for latch mechanism."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&debug_latch,
		false, NULL, NULL
	},

	{
		{"gp_crash_recovery_suppress_ao_eof", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Warning about crash recovery append only eof issue"),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&gp_crash_recovery_suppress_ao_eof,
		false, NULL, NULL
	},

	{
		{"gp_encoding_check_locale_compatibility", PGC_POSTMASTER, CLIENT_CONN_LOCALE,
					gettext_noop("Enable check for compatibility of encoding and locale in createdb"),
					NULL,
					GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_encoding_check_locale_compatibility,
		true, NULL, NULL
	},

	{
		{"gp_temporary_files_filespace_repair", PGC_SUSET, DEVELOPER_OPTIONS,
                        gettext_noop("Change the filespace inconsistency to a warning"),
                        NULL,
                        GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
                },
                &gp_temporary_files_filespace_repair,
                false, NULL, NULL
	},

	/* for pljava */
	{
		{"pljava_release_lingering_savepoints", PGC_SUSET, CUSTOM_OPTIONS,
			gettext_noop("If true, lingering savepoints will be released on function exit; if false, they will be rolled back"),
			NULL,
		 GUC_GPDB_ADDOPT | GUC_NOT_IN_SAMPLE | GUC_SUPERUSER_ONLY
		},
		&pljava_release_lingering_savepoints,
		false, NULL, NULL
	},
	{
		{"pljava_debug", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Stop the backend to attach a debugger"),
			NULL,
		 GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_SUPERUSER_ONLY
		},
		&pljava_debug,
		false, NULL, NULL
	},

	/* for SimEx */
	{
		{"gp_simex_init", PGC_POSTMASTER, GP_ERROR_HANDLING,
			gettext_noop("Initialize exception simulation - used to set up simulation at startup"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_simex_init,
		false, NULL, NULL
	},

	{
		{"gp_simex_run", PGC_USERSET, GP_ERROR_HANDLING,
			gettext_noop("Run exception simulation - used to control starting/stopping simulation at runtime"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_simex_run,
		false, NULL, NULL
	},

        {
          {"gp_keep_all_xlog", PGC_SUSET, DEVELOPER_OPTIONS,
           gettext_noop("Do not remove old xlog files."),
           NULL,
           GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
          },
          &gp_keep_all_xlog,
          false, NULL, NULL
        },

	{
		{"gp_test_time_slice", PGC_USERSET, GP_ERROR_HANDLING,
		 gettext_noop("Check for time slice violation between checks for interrupts"),
		 NULL,
		 GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_test_time_slice,
		false, NULL, NULL
	},

	{
		{"gp_test_deadlock_hazard", PGC_USERSET, GP_ERROR_HANDLING,
		 gettext_noop("Check if a lightweight lock is already held when requesting a database lock"),
		 NULL,
		 GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_test_deadlock_hazard,
		false, NULL, NULL
	},

	{
		{"gp_cancel_query_print_log", PGC_USERSET, DEVELOPER_OPTIONS,
		 gettext_noop("Print out debugging info for a canceled query"),
		 NULL,
		 GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_cancel_query_print_log,
		false, NULL, NULL
	},

	{
	 	{"gp_partitioning_dynamic_selection_log", PGC_USERSET, DEVELOPER_OPTIONS,
		 gettext_noop("Print out debugging info for GPDB dynamic partition selection"),
		 NULL,
		 GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_partitioning_dynamic_selection_log,
		false, NULL, NULL
	},

	{
	 	{"gp_perfmon_print_packet_info", PGC_USERSET, DEVELOPER_OPTIONS,
		 gettext_noop("Print out debugging info for a Perfmon packet"),
		 NULL,
		 GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_perfmon_print_packet_info,
		false, NULL, NULL
	},

	{
		{"gp_log_stack_trace_lines", PGC_USERSET, LOGGING_WHAT,
		 gettext_noop("Control if file/line information is included in stack traces"),
		 NULL,
		 GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_log_stack_trace_lines,
		true, NULL, NULL
	},

	{

		{"gp_log_resqueue_memory", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Prints out messages related to resource queue's memory management."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_log_resqueue_memory,
		false, NULL, NULL
	},

	{
		{"gp_resqueue_print_operator_memory_limits", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Prints out the memory limit for operators (in explain) assigned by resource queue's "
						 "memory management."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_resqueue_print_operator_memory_limits,
		false, NULL, NULL
	},

	{
		{"gp_dynamic_partition_pruning", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("This guc enables plans that can dynamically eliminate scanning of partitions."),
			NULL
		},
		&gp_dynamic_partition_pruning,
		true, NULL, NULL
	},

	{
		{"gp_cte_sharing", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("This guc enables sharing of plan fragments for common table expressions."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_cte_sharing,
		false, NULL, NULL
	},

	{
		{"gp_log_dynamic_partition_pruning", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("This guc enables debug messages related to dynamic partition pruning."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_log_dynamic_partition_pruning,
		false, NULL, NULL
	},

	{
		{"gp_ignore_window_exclude", PGC_USERSET, COMPAT_OPTIONS_PREVIOUS,
		    gettext_noop("Ignore EXCLUDE in window frame specifications."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_ignore_window_exclude,
		false, NULL, NULL
	},

	{
		{"gp_create_table_random_default_distribution", PGC_USERSET, COMPAT_OPTIONS,
			gettext_noop("Set the default distribution of a table to RANDOM."),
			NULL,
			GUC_NOT_IN_SAMPLE
		},
		&gp_create_table_random_default_distribution,
		false, NULL, NULL
	},

	{
		{"gp_allow_non_uniform_partitioning_ddl", PGC_USERSET, COMPAT_OPTIONS,
			gettext_noop("Allow DDL that will create multi-level partition table with non-uniform hierarchy."),
			NULL,
			GUC_SUPERUSER_ONLY | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_allow_non_uniform_partitioning_ddl,
		true, NULL, NULL
	},

	{
		{"gp_enable_exchange_default_partition", PGC_USERSET, COMPAT_OPTIONS,
			gettext_noop("Allow DDL that will exchange default partitions."),
			NULL
		},
		&gp_enable_exchange_default_partition,
		false, NULL, NULL
	},

	{
		{"optimizer", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Enable the new optimizer."),
			NULL
		},
		&optimizer,
		false, assign_optimizer, NULL
	},

	{
		{"optimizer_log", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Log optimizer messages."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_log,
		true, NULL, NULL
	},

	{
		{"optimizer_partition_selection_log", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Log optimizer partition selection."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&optimizer_partition_selection_log,
		false, NULL, NULL
	},

	{
		{"optimizer_print_query", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Prints the optimizer's input query expression tree."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_print_query,
		false, NULL, NULL
	},

	{
		{"optimizer_print_plan", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Prints the plan expression tree produced by the optimizer."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_print_plan,
		false, NULL, NULL
	},

	{
		{"optimizer_print_xform", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Prints optimizer transformation information."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_print_xform,
		false, NULL, NULL
	},

	{
		{"optimizer_release_mdcache", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Release MDCache after each query."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_release_mdcache,
		true, NULL, NULL
	},

	{
		{"optimizer_disable_missing_stats_collection", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Disable collecting of columns with missing statistics."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_disable_missing_stats_collection,
		false, NULL, NULL
	},

	{
		{"optimizer_disable_xform_result_printing", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Disable printing the input and output of optimizer transformations."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_disable_xform_result_printing,
		false, NULL, NULL
	},

	{
		{"optimizer_print_memo_after_exploration", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Print optimizer memo structure after the exploration phase."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_print_memo_after_exploration,
		false, NULL, NULL
	},

	{
		{"optimizer_print_memo_after_implementation", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Print optimizer memo structure after the implementation phase."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_print_memo_after_implementation,
		false, NULL, NULL
	},

	{
		{"optimizer_print_memo_after_optimization", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Print optimizer memo structure after optimization."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_print_memo_after_optimization,
		false, NULL, NULL
	},

	{
		{"optimizer_print_job_scheduler", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Print the jobs in the scheduler on each job completion."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_print_job_scheduler,
		false, NULL, NULL
	},

	{
		{"optimizer_print_expression_properties", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Print expression properties."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_print_expression_properties,
		false, NULL, NULL
	},

	{
		{"optimizer_print_group_properties", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Print group properties."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_print_group_properties,
		false, NULL, NULL
	},

	{
		{"optimizer_print_optimization_context", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Print the optimization context."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_print_optimization_context,
		false, NULL, NULL
	},
 	{
 		{"gp_reject_internal_tcp_connection", PGC_POSTMASTER,
			DEVELOPER_OPTIONS,
			gettext_noop("Permit internal TCP connections to the master."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_reject_internal_tcp_conn,
		true, NULL, NULL
	},

	{
		{"optimizer_print_optimization_stats", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Print optimization stats."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
 		&optimizer_print_optimization_stats,
		false, NULL, NULL
	},

	{
		{"optimizer_parallel", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Enable using threads in optimization engine."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_parallel,
		false, NULL, NULL
	},
 	{
		{"optimizer_extract_dxl_stats", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Extract plan stats in dxl."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_extract_dxl_stats,
		false, NULL, NULL
	},
	{
		{"optimizer_extract_dxl_stats_all_nodes", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Extract plan stats for all physical dxl nodes."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_extract_dxl_stats_all_nodes,
		false, NULL, NULL
	},
	{
		{"optimizer_dpe_stats", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Enable statistics derivation for partitioned tables with dynamic partition elimination."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_dpe_stats,
		false, NULL, NULL
	},
	{
		{"optimizer_enable_indexjoin", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable index nested loops join plans in the optimizer."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_enable_indexjoin,
		true, NULL, NULL
	},
	{
		{"optimizer_enable_motions_masteronly_queries", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable plans with Motion operators in the optimizer for queries with no distributed tables."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_enable_motions_masteronly_queries,
		false, NULL, NULL
	},
	{
		{"optimizer_enable_motions", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable plans with Motion operators in the optimizer."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_enable_motions,
		true, NULL, NULL
	},
	{
		{"optimizer_enable_motion_broadcast", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable plans with Motion Broadcast operators in the optimizer."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_enable_motion_broadcast,
		true, NULL, NULL
	},
	{
		{"optimizer_enable_motion_gather", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable plans with Motion Gather operators in the optimizer."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_enable_motion_gather,
		true, NULL, NULL
	},
	{
		{"optimizer_enable_motion_redistribute", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable plans with Motion Redistribute operators in the optimizer."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_enable_motion_redistribute,
		true, NULL, NULL
	},
	{
		{"optimizer_enable_sort", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable plans with Sort operators in the optimizer."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_enable_sort,
		true, NULL, NULL
	},
	{
		{"optimizer_enable_materialize", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable plans with Materialize operators in the optimizer."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_enable_materialize,
		true, NULL, NULL
	},
	{
		{"optimizer_enable_partition_propagation", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable plans with Partition Propagation operators in the optimizer."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_enable_partition_propagation,
		true, NULL, NULL
	},
	{
		{"optimizer_enable_partition_selection", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable plans with Partition Selection operators in the optimizer."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_enable_partition_selection,
		true, NULL, NULL
	},
	{
		{"optimizer_enable_outerjoin_rewrite", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable outer join to inner join rewrite in the optimizer."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_enable_outerjoin_rewrite,
		true, NULL, NULL
	},
	{
		{"optimizer_direct_dispatch", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable direct dispatch in the optimizer."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_direct_dispatch,
		true, NULL, NULL
	},
    {
    	{"optimizer_control", PGC_SUSET, DEVELOPER_OPTIONS,
    		gettext_noop("Allow/disallow turning the optimizer on or off."),
    		NULL
    	},
    	&optimizer_control,
    	true, NULL, NULL
    },
        {
                {"optimizer_enable_space_pruning", PGC_USERSET, DEVELOPER_OPTIONS,
                        gettext_noop("Enable space pruning in the optimizer."),
                        NULL,
                        GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
                },
                &optimizer_enable_space_pruning,
                true, NULL, NULL
        },

        {
                {"optimizer_enable_master_only_queries", PGC_USERSET, DEVELOPER_OPTIONS,
                        gettext_noop("Process master only queries via the optimizer."),
                        NULL,
                        GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
                },
                &optimizer_enable_master_only_queries,
                false, NULL, NULL
        },

        {
                {"optimizer_multilevel_partitioning", PGC_USERSET, DEVELOPER_OPTIONS,
                        gettext_noop("Enable optimization of queries on multilevel partitioned tables."),
                        NULL,
                        GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
                },
                &optimizer_multilevel_partitioning,
                true, NULL, NULL
        },

        {
                {"optimizer_explain_show_status", PGC_USERSET, DEVELOPER_OPTIONS,
                        gettext_noop("Display optimizer version information in explain messages."),
                        NULL,
                        GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
                },
                &optimizer_explain_show_status,
                true, NULL, NULL
        },

        {
                {"optimizer_prefer_multistage_agg", PGC_USERSET, DEVELOPER_OPTIONS,
                        gettext_noop("Prefer multistage aggregates in the optimizer."),
                        NULL,
                        GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
                },
                &optimizer_prefer_multistage_agg,
                true, NULL, NULL
        },

        {
                {"optimizer_enable_multiple_distinct_aggs", PGC_USERSET, DEVELOPER_OPTIONS,
                        gettext_noop("Enable plans with multiple distinct aggregates in the optimizer."),
                        NULL,
                        GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
                },
                &optimizer_enable_multiple_distinct_aggs,
                false, NULL, NULL
        },

        {
                {"optimizer_prefer_expanded_distinct_aggs", PGC_USERSET, DEVELOPER_OPTIONS,
                        gettext_noop("Prefer plans that expand multiple distinct aggregates into join of single distinct aggregate in the optimizer."),
                        NULL,
                        GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
                },
                &optimizer_prefer_expanded_distinct_aggs,
                true, NULL, NULL
        },

		{
				{"optimizer_prune_computed_columns", PGC_USERSET, DEVELOPER_OPTIONS,
						gettext_noop("Prune unused computed columns when pre-processing query"),
						NULL,
						GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		        },
				&optimizer_prune_computed_columns,
				true, NULL, NULL
		},

        {
        		{"optimizer_push_requirements_from_consumer_to_producer", PGC_USERSET, DEVELOPER_OPTIONS,
        				gettext_noop("Optimize CTE producer plan on requirements enforced on top of CTE consumer in the optimizer."),
        				NULL,
        				GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
        		},
        		&optimizer_push_requirements_from_consumer_to_producer,
        		false, NULL, NULL
        },

        {
                {"optimizer_enable_hashjoin_redistribute_broadcast_children", PGC_USERSET, DEVELOPER_OPTIONS,
                        gettext_noop("Enable hash join plans with, Redistribute outer child and Broadcast inner child, in the optimizer."),
                        NULL,
                        GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
                },
                &optimizer_enable_hashjoin_redistribute_broadcast_children,
                false, NULL, NULL
        },
        {
        		{"optimizer_enable_broadcast_nestloop_outer_child", PGC_USERSET, DEVELOPER_OPTIONS,
                         gettext_noop("Enable nested loops join plans with replicated outer child in the optimizer."),
                         NULL,
                         GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
                 },
                 &optimizer_enable_broadcast_nestloop_outer_child,
                 true, NULL, NULL
        },
        {
        		{"optimizer_enforce_subplans", PGC_USERSET, DEVELOPER_OPTIONS,
                         gettext_noop("Enforce correlated execution in the optimizer"),
                         NULL,
                         GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
                 },
                 &optimizer_enforce_subplans,
                 false, NULL, NULL
        },
        {
                {"optimizer_enable_assert_maxonerow", PGC_USERSET, DEVELOPER_OPTIONS,
                        gettext_noop("Enable Assert MaxOneRow plans to check number of rows at runtime."),
                        NULL,
                        GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
                },
                &optimizer_enable_assert_maxonerow,
                true, NULL, NULL
        },
	{
		{"optimizer_enumerate_plans", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Enable plan enumeration"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_enumerate_plans,
		false, NULL, NULL
	},

	{
		{"optimizer_sample_plans", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable plan sampling"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_sample_plans,
		false, NULL, NULL
	},

	{
		{"optimizer_cte_inlining", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable CTE inlining"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_cte_inlining,
		false, NULL, NULL
	},

	{
		{"optimizer_analyze_root_partition", PGC_USERSET, STATS_ANALYZE,
			gettext_noop("Enable statistics collection on root partitions during ANALYZE"),
			NULL
		},
		&optimizer_analyze_root_partition,
		false, NULL, NULL
	},

	{
		{"optimizer_analyze_midlevel_partition", PGC_USERSET, STATS_ANALYZE,
			gettext_noop("Enable statistics collection on intermediate partitions during ANALYZE"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_analyze_midlevel_partition,
		false, NULL, NULL
	},

	{
		{"optimizer_enable_constant_expression_evaluation", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable constant expression evaluation in the optimizer"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_enable_constant_expression_evaluation,
		true, NULL, NULL
	},

	{
		{"optimizer_use_external_constant_expression_evaluation_for_ints", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Use external constant expression evaluation in the optimizer for all integer types"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_use_external_constant_expression_evaluation_for_ints,
		false, NULL, NULL
	},

	{
		{"optimizer_enable_bitmapscan", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable bitmap plans in the optimizer"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_enable_bitmapscan,
		true, NULL, NULL
	},

        {
                {"optimizer_enable_outerjoin_to_unionall_rewrite", PGC_USERSET, DEVELOPER_OPTIONS,
                        gettext_noop("Enable rewriting Left Outer Join to UnionAll"),
                        NULL,
                        GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
                },
                &optimizer_enable_outerjoin_to_unionall_rewrite,
		false, NULL, NULL
        },

	{
		{"optimizer_apply_left_outer_to_union_all_disregarding_stats", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Always apply Left Outer Join to Inner Join UnionAll Left Anti Semi Join without looking at stats."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_apply_left_outer_to_union_all_disregarding_stats,
		false, NULL, NULL
	},

	{
		{"optimizer_enable_ctas", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable CTAS plans in the optimizer"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_enable_ctas,
		true, NULL, NULL
	},

	{
		{"optimizer_remove_order_below_dml", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Remove OrderBy below a DML operation"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_remove_order_below_dml,
		false, NULL, NULL
	},

	{
		{"optimizer_static_partition_selection", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable static partition selection"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_static_partition_selection,
		true, NULL, NULL
	},

	{
		{"optimizer_enable_partial_index", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable heterogeneous index plans."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_enable_partial_index,
		true, NULL, NULL
	},

	{
		{"optimizer_dml_triggers", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Support DML with triggers."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_dml_triggers,
		false, NULL, NULL
	},

	{
		{"optimizer_dml_constraints", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Support DML with CHECK constraints and NOT NULL constraints."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_dml_constraints,
		true, NULL, NULL
	},

	{
		{"gp_log_optimization_time", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Writes time spent producing a plan to the server log"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
 		&gp_log_optimization_time,
		false, NULL, NULL
	},

	{
		{"gp_reject_internal_tcp_connection", PGC_POSTMASTER,
			DEVELOPER_OPTIONS,
			gettext_noop("Permit internal TCP connections to the master."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_reject_internal_tcp_conn,
		true, NULL, NULL
	},

	{
		{"gp_plpgsql_clear_cache_always", PGC_USERSET, DEVELOPER_OPTIONS,
		 gettext_noop("Controls caching of plpgsql plans in session"),
		 NULL,
		 GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_plpgsql_clear_cache_always,
		false, NULL, NULL
	},

	{
		{"fts_diskio_check", PGC_USERSET, DEVELOPER_OPTIONS,
		 gettext_noop("Controls if FTS should perform disk IO checks on primary segments as part of probe"),
		 NULL,
		 GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&fts_diskio_check,
		true, NULL, NULL
	},

	{
		{"gp_enable_caql_logging", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Enable caql logging."),
			NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_enable_caql_logging,
		true, NULL, NULL
	},

	{
		{"gp_disable_catalog_access_on_segment", PGC_USERSET, DEVELOPER_OPTIONS,
		 gettext_noop("Disables non-builtin object access on segments"),
		 NULL,
		 GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_disable_catalog_access_on_segment,
		false, NULL, NULL
	},

	{
		{"dml_ignore_target_partition_check", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Ignores checking whether the user provided correct partition during a direct insert to a leaf partition"),
			NULL,
			 GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&dml_ignore_target_partition_check,
		false, NULL, NULL
	},

	/* End-of-list marker */
	{
		{NULL, 0, 0, NULL, NULL}, NULL, false, NULL, NULL
	}
};


static struct config_int ConfigureNamesInt[] =
{
	/* maximum read/write I/O size for DD Boost is 1MB */
	{
		{"ddboost_buf_size", PGC_SIGHUP, DEVELOPER_OPTIONS,
			gettext_noop("size of ddboost dump/restore buffer"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&ddboost_buf_size,
		524288, 65536, 1048576, NULL, NULL
	},

	{
		{"readable_external_table_timeout", PGC_USERSET, EXTERNAL_TABLES,
			gettext_noop("Cancel the query if no data read within N seconds."),
			gettext_noop("A value of 0 turns off the timeout."),
			GUC_UNIT_S | GUC_NOT_IN_SAMPLE
		},
		&readable_external_table_timeout,
		0, 0, INT_MAX, NULL, NULL
	},

	{
		{"archive_timeout", PGC_SIGHUP, WAL_SETTINGS,
			gettext_noop("Forces a switch to the next xlog file if a "
						 "new file has not been started within N seconds."),
			NULL,
			GUC_UNIT_S | GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&XLogArchiveTimeout,
		0, 0, INT_MAX, NULL, NULL
	},
	{
		{"post_auth_delay", PGC_BACKEND, DEVELOPER_OPTIONS,
			gettext_noop("Waits N seconds on connection startup after authentication."),
			gettext_noop("This allows attaching a debugger to the process."),
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL | GUC_UNIT_S
		},
		&PostAuthDelay,
		0, 0, INT_MAX, NULL, NULL
	},
	{
		{"default_statistics_target", PGC_USERSET, STATS_ANALYZE,
			gettext_noop("Sets the default statistics target."),
			gettext_noop("This applies to table columns that have not had a "
				"column-specific target set via ALTER TABLE SET STATISTICS.")
		},
		&default_statistics_target,
		25, 1, 1000, NULL, NULL
	},
	{
		{"from_collapse_limit", PGC_USERSET, QUERY_TUNING_OTHER,
			gettext_noop("Sets the FROM-list size beyond which subqueries are not "
						 "collapsed."),
			gettext_noop("The planner will merge subqueries into upper "
				"queries if the resulting FROM list would have no more than "
						 "this many items.")
		},
		&from_collapse_limit,
		20, 1, INT_MAX, NULL, NULL
	},
	{
		{"join_collapse_limit", PGC_USERSET, QUERY_TUNING_OTHER,
			gettext_noop("Sets the FROM-list size beyond which JOIN constructs are not "
						 "flattened."),
			gettext_noop("The planner will flatten explicit JOIN "
			"constructs into lists of FROM items whenever a list of no more "
						 "than this many items would result.")
		},
		&join_collapse_limit,
		20, 1, INT_MAX, NULL, NULL
	},
	{
		{"geqo_threshold", PGC_USERSET, DEFUNCT_OPTIONS,
			gettext_noop("Unused. Syntax check only for PostgreSQL compatibility."),
            NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&defunct_int,
		12, 2, INT_MAX, NULL, NULL
	},
	{
		{"geqo_effort", PGC_USERSET, DEFUNCT_OPTIONS,
			gettext_noop("Unused. Syntax check only for PostgreSQL compatibility."),
            NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&defunct_int,
		//DEFAULT_GEQO_EFFORT, MIN_GEQO_EFFORT, MAX_GEQO_EFFORT, NULL, NULL
		0, 0, INT_MAX, NULL, NULL
	},
	{
		{"geqo_pool_size", PGC_USERSET, DEFUNCT_OPTIONS,
			gettext_noop("Unused. Syntax check only for PostgreSQL compatibility."),
            NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&defunct_int,
		0, 0, INT_MAX, NULL, NULL
	},
	{
		{"geqo_generations", PGC_USERSET, DEFUNCT_OPTIONS,
			gettext_noop("Unused. Syntax check only for PostgreSQL compatibility."),
            NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&defunct_int,
		0, 0, INT_MAX, NULL, NULL
	},
	{
		{"deadlock_timeout", PGC_SIGHUP, LOCK_MANAGEMENT,
			gettext_noop("The time in milliseconds to wait on lock before checking for deadlock."),
			NULL,
			GUC_UNIT_MS
		},
		&DeadlockTimeout,
		1000, 0, INT_MAX, NULL, NULL
	},

	{
		{"gp_cancel_query_delay_time", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("The time in milliseconds to delay a query cancellation."),
			NULL,
			GUC_UNIT_MS | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_cancel_query_delay_time,
		0, 0, INT_MAX, NULL, NULL
	},

	/*
	 * Note: There is some postprocessing done in PostmasterMain() to make
	 * sure the buffers are at least twice the number of backends, so the
	 * constraints here are partially unused. Similarly, the superuser
	 * reserved number is checked to ensure it is less than the max backends
	 * number.
	 *
	 * MaxBackends is limited to INT_MAX/4 because some places compute
	 * 4*MaxBackends without any overflow check.  Likewise we have to limit
	 * NBuffers to INT_MAX/2.
	 */
	{
		{"max_connections", PGC_POSTMASTER, CONN_AUTH_SETTINGS,
			gettext_noop("Sets the maximum number of concurrent connections."),
			NULL
		},
		&MaxBackends,
		200, 10, MAX_MAX_BACKENDS, NULL, NULL
	},

	{
		{"superuser_reserved_connections", PGC_POSTMASTER, CONN_AUTH_SETTINGS,
			gettext_noop("Sets the number of connection slots reserved for superusers."),
			NULL
		},
		&ReservedBackends,
		3, 0, INT_MAX / 4, NULL, NULL
	},

	{
		{"shared_buffers", PGC_POSTMASTER, RESOURCES_MEM,
			gettext_noop("Sets the number of shared memory buffers used by the server."),
			NULL,
			GUC_UNIT_BLOCKS
		},
		&NBuffers,
		4096, 16, INT_MAX / 2, NULL, NULL
	},

	{
		{"temp_buffers", PGC_USERSET, RESOURCES_MEM,
			gettext_noop("Sets the maximum number of temporary buffers used by each session."),
			NULL,
			GUC_UNIT_BLOCKS | GUC_GPDB_ADDOPT
		},
		&num_temp_buffers,
		1024, 100, INT_MAX / 2, NULL, show_num_temp_buffers
	},

	{
		{"gp_max_local_distributed_cache", PGC_POSTMASTER, RESOURCES_MEM,
			gettext_noop("Sets the number of local-distributed transactions to cache for optimizing visibility processing by backends."),
			NULL
		},
		&gp_max_local_distributed_cache,
		1024, 0, INT_MAX, NULL, NULL
	},

	{
		{"gp_max_databases", PGC_POSTMASTER, RESOURCES_MEM,
			gettext_noop("Sets the maximum number of databases."),
			NULL
		},
		&gp_max_databases,
		GP_MAX_DATABASES_DEFAULT, 8, 256, NULL, NULL
	},

	{
		{"gp_max_tablespaces", PGC_POSTMASTER, RESOURCES_MEM,
			gettext_noop("Sets the maximum number of tablespaces."),
			NULL
		},
		&gp_max_tablespaces,
		GP_MAX_TABLESPACES_DEFAULT, 8, 2048, NULL, NULL
	},

	{
		{"gp_max_filespaces", PGC_POSTMASTER, RESOURCES_MEM,
			gettext_noop("Sets the maximum number of filespaces."),
			NULL
		},
		&gp_max_filespaces,
		GP_MAX_FILESPACES_DEFAULT, 8, 256, NULL, NULL
	},

	{
		{"debug_dtm_action_delay_ms", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Sets the debug DTM action delay in milliseconds."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_UNIT_MS
		},
		&Debug_dtm_action_delay_ms,
		DEBUG_DTM_ACTION_DELAY_MS_DEFAULT, 0, INT_MAX / 1000, NULL, NULL
	},

	{
		{"debug_dtm_action_segment", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Sets the debug DTM action segment."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_dtm_action_segment,
		DEBUG_DTM_ACTION_SEGMENT_DEFAULT, 0, 1000, NULL, NULL
	},

	{
		{"debug_dtm_action_nestinglevel", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Sets the debug DTM action transaction nesting level."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_dtm_action_nestinglevel,
		DEBUG_DTM_ACTION_NESTINGLEVEL_DEFAULT, 0, 1000, NULL, NULL
	},

	{
		{"test_compresslevel_override", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("For testing purposes, the override value for compresslevel when non-default."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Test_compresslevel_override,
		DEFAULT_COMPRESS_LEVEL, 0, 9, NULL, NULL
	},

	{
		{"test_blocksize_override", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("For testing purposes, the override value for append only blocksize when non-default."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Test_blocksize_override,
		DEFAULT_APPENDONLY_BLOCK_SIZE, MIN_APPENDONLY_BLOCK_SIZE, MAX_APPENDONLY_BLOCK_SIZE, NULL, NULL
	},

	{
		{"test_safefswritesize_override", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("For testing purposes, the override value for append only safefswritesize when non-default."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&Test_safefswritesize_override,
		DEFAULT_FS_SAFE_WRITE_SIZE, 0, INT_MAX, NULL, NULL
	},

	{
		{"gp_safefswritesize", PGC_BACKEND, RESOURCES,
			gettext_noop("Minimum FS safe write size."),
			NULL
		},
		&gp_safefswritesize,
		DEFAULT_FS_SAFE_WRITE_SIZE, 0, INT_MAX, NULL, NULL
    },

	{
		{"port", PGC_POSTMASTER, CONN_AUTH_SETTINGS,
			gettext_noop("Sets the TCP port the server listens on."),
			NULL
		},
		&PostPortNumber,
		DEF_PGPORT, 1, 65535, NULL, NULL
	},

	{
		{"unix_socket_permissions", PGC_POSTMASTER, CONN_AUTH_SETTINGS,
			gettext_noop("Sets the access permissions of the Unix-domain socket."),
			gettext_noop("Unix-domain sockets use the usual Unix file system "
						 "permission set. The parameter value is expected to be an numeric mode "
						 "specification in the form accepted by the chmod and umask system "
						 "calls. (To use the customary octal format the number must start with "
						 "a 0 (zero).)")
		},
		&Unix_socket_permissions,
		0777, 0000, 0777, NULL, NULL
	},

	{
		{"planner_work_mem", PGC_USERSET, RESOURCES_MEM,
			gettext_noop("Sets the maximum memory to be used for query workspaces, "
						 "used in the planner only."),
			gettext_noop("The planner considers this much memory may be used by each internal "
						 "sort operation and hash table before switching to "
						 "temporary disk files."),
			GUC_UNIT_KB | GUC_GPDB_ADDOPT | GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&planner_work_mem,
        32768, 2 * BLCKSZ / 1024, MAX_KILOBYTES, NULL, NULL
	},

	{
		{"work_mem", PGC_USERSET, DEPRECATED_OPTIONS,
			gettext_noop("Sets the maximum memory to be used for query workspaces."),
			gettext_noop("This much memory may be used by each internal "
						 "sort operation and hash table before switching to "
						 "temporary disk files."),
			GUC_UNIT_KB | GUC_GPDB_ADDOPT
		},
		&work_mem,
#ifdef USE_ASSERT_CHECKING      /* allow executor testing with low memory */
        32768, 2 * BLCKSZ / 1024, MAX_KILOBYTES, NULL, NULL
#else
		32768, 8 * BLCKSZ / 1024, MAX_KILOBYTES, NULL, NULL
#endif
	},

	{
		{"max_work_mem", PGC_SUSET, DEPRECATED_OPTIONS,
		 	gettext_noop("Sets the maximum value for work_mem setting."),
		 	NULL,
			GUC_UNIT_KB | GUC_GPDB_ADDOPT
		},
		&max_work_mem,
		1024000, 8 * BLCKSZ / 1024, MAX_KILOBYTES, NULL, NULL
	},

	{
		{"maintenance_work_mem", PGC_USERSET, RESOURCES_MEM,
			gettext_noop("Sets the maximum memory to be used for maintenance operations."),
			gettext_noop("This includes operations such as VACUUM and CREATE INDEX."),
			GUC_UNIT_KB | GUC_GPDB_ADDOPT
		},
		&maintenance_work_mem,
		65536, 1024, MAX_KILOBYTES, NULL, NULL
	},

	{
		{"statement_mem", PGC_USERSET, RESOURCES_MEM,
			gettext_noop("Sets the memory to be reserved for a statement."),
			NULL,
			GUC_UNIT_KB | GUC_GPDB_ADDOPT
		},
		&statement_mem,
#ifdef USE_ASSERT_CHECKING
		/** Allow lower values for testing */
		128000, 50, INT_MAX, gpvars_assign_statement_mem, NULL
#else
		128000, 1000, INT_MAX, gpvars_assign_statement_mem, NULL
#endif
	},

	{
		{"max_statement_mem", PGC_SUSET, RESOURCES_MEM,
		 	gettext_noop("Sets the maximum value for statement_mem setting."),
		 	NULL,
			GUC_UNIT_KB | GUC_GPDB_ADDOPT
		},
		&max_statement_mem,
		2048000, 32768, INT_MAX, NULL, NULL
	},

	{
		{"gp_vmem_limit_per_query", PGC_POSTMASTER, RESOURCES_MEM,
		 	gettext_noop("Sets the maximum allowed memory per-statement on each segment."),
		 	NULL,
			GUC_UNIT_KB | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_vmem_limit_per_query,
		0, 0, INT_MAX / 2, NULL, NULL
	},

	{
		{"gp_max_plan_size", PGC_SUSET, RESOURCES_MEM,
		 	gettext_noop("Sets the maximum size of a plan to be dispatched."),
		 	NULL,
			GUC_UNIT_KB
		},
		&gp_max_plan_size,
		0, 0, MAX_KILOBYTES, NULL, NULL
	},

	{
		{"gp_max_partition_level", PGC_SUSET, PRESET_OPTIONS,
		 	gettext_noop("Sets the maximum number of levels allowed when creating a partitioned table."),
		 	gettext_noop("Use 0 for no limit."),
		 	GUC_GPDB_ADDOPT | GUC_SUPERUSER_ONLY | GUC_NOT_IN_SAMPLE
		},
		&gp_max_partition_level,
		0, 0, INT_MAX, NULL, NULL
	},

	{
		{"max_stack_depth", PGC_SUSET, RESOURCES_MEM,
			gettext_noop("Sets the maximum stack depth, in kilobytes."),
			NULL,
			GUC_UNIT_KB
		},
		&max_stack_depth,
		100, 100, MAX_KILOBYTES, assign_max_stack_depth, NULL
	},

	{
		{"vacuum_cost_page_hit", PGC_USERSET, RESOURCES,
			gettext_noop("Vacuum cost for a page found in the buffer cache."),
			NULL
		},
		&VacuumCostPageHit,
		1, 0, 10000, NULL, NULL
	},

	{
		{"vacuum_cost_page_miss", PGC_USERSET, RESOURCES,
			gettext_noop("Vacuum cost for a page not found in the buffer cache."),
			NULL
		},
		&VacuumCostPageMiss,
		10, 0, 10000, NULL, NULL
	},

	{
		{"vacuum_cost_page_dirty", PGC_USERSET, RESOURCES,
			gettext_noop("Vacuum cost for a page dirtied by vacuum."),
			NULL
		},
		&VacuumCostPageDirty,
		20, 0, 10000, NULL, NULL
	},

	{
		{"vacuum_cost_limit", PGC_USERSET, RESOURCES,
			gettext_noop("Vacuum cost amount available before napping."),
			NULL
		},
		&VacuumCostLimit,
		200, 1, 10000, NULL, NULL
	},

	{
		{"vacuum_cost_delay", PGC_USERSET, RESOURCES,
			gettext_noop("Vacuum cost delay in milliseconds."),
			NULL,
			GUC_UNIT_MS
		},
		&VacuumCostDelay,
		0, 0, 1000, NULL, NULL
	},

	{
		{"autovacuum_vacuum_cost_delay", PGC_SIGHUP, DEFUNCT_OPTIONS,
			gettext_noop("Vacuum cost delay in milliseconds, for autovacuum."),
			NULL,
			GUC_UNIT_MS | GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&autovacuum_vac_cost_delay,
		-1, -1, 1000, NULL, NULL
	},

	{
		{"autovacuum_vacuum_cost_limit", PGC_SIGHUP, DEFUNCT_OPTIONS,
			gettext_noop("Vacuum cost amount available before napping, for autovacuum."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&autovacuum_vac_cost_limit,
		-1, -1, 10000, NULL, NULL
	},

	{
		{"gp_appendonly_compaction_threshold", PGC_USERSET, APPENDONLY_TABLES,
			gettext_noop("Threshold of the ratio of dirty data in a segment file over which the file"
					     " will be compacted during lazy vacuum."),
			NULL
		},
		&gp_appendonly_compaction_threshold,
		10, 0, 100, NULL, NULL
	},

	{
		{"max_prepared_transactions", PGC_POSTMASTER, RESOURCES,
			gettext_noop("Sets the maximum number of simultaneously prepared transactions."),
            NULL
		},
		&max_prepared_xacts,
		50, 1, 1000, NULL, NULL
	},

	{
		{"gp_workfile_max_entries", PGC_POSTMASTER, RESOURCES,
			gettext_noop("Sets the maximum number of entries that can be stored in the workfile directory"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_workfile_max_entries,
		8192, 32, INT_MAX, NULL, NULL
	},

	{
		{"max_files_per_process", PGC_POSTMASTER, RESOURCES_KERNEL,
			gettext_noop("Sets the maximum number of simultaneously open files for each server process."),
			NULL
		},
		&max_files_per_process,
		1000, 25, INT_MAX, NULL, NULL
	},

	{
		{"gp_workfile_limit_files_per_query", PGC_USERSET, RESOURCES,
			gettext_noop("Maximum number of workfiles allowed per query per segment."),
			gettext_noop("0 for no limit. Current query is terminated when limit is exceeded."),
			GUC_GPDB_ADDOPT
		},
		&gp_workfile_limit_files_per_query,
		100000, 0, INT_MAX, NULL, NULL,
	},

#ifdef LOCK_DEBUG
	{
		{"trace_lock_oidmin", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("no description available"),
			NULL,
			GUC_NOT_IN_SAMPLE
		},
		&Trace_lock_oidmin,
		FirstNormalObjectId, 0, INT_MAX, NULL, NULL
	},
	{
		{"trace_lock_table", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("no description available"),
			NULL,
			GUC_NOT_IN_SAMPLE
		},
		&Trace_lock_table,
		0, 0, INT_MAX, NULL, NULL
	},
#endif

	{
		{"statement_timeout", PGC_USERSET, CLIENT_CONN_STATEMENT,
			gettext_noop("Sets the maximum allowed duration (in milliseconds) of any statement."),
			gettext_noop("A value of 0 turns off the timeout."),
			GUC_UNIT_MS | GUC_GPDB_ADDOPT
		},
		&StatementTimeout,
		0, 0, INT_MAX, NULL, NULL
	},

	{
		{"gp_vmem_idle_resource_timeout", PGC_USERSET, CLIENT_CONN_OTHER,
			gettext_noop("Sets the time a session can be idle (in milliseconds) before we release gangs on the segment DBs to free resources."),
			gettext_noop("A value of 0 turns off the timeout."),
			GUC_UNIT_MS | GUC_GPDB_ADDOPT
		},
		&IdleSessionGangTimeout,
#ifdef USE_ASSERT_CHECKING
		600000, 0, INT_MAX, NULL, NULL /* 10 minutes by default on debug builds.*/
#else
		18000, 0, INT_MAX, NULL, NULL
#endif
	},

	{
		{"vacuum_freeze_min_age", PGC_USERSET, CLIENT_CONN_STATEMENT,
			gettext_noop("Minimum age at which VACUUM should freeze a table row."),
			NULL
		},
		&vacuum_freeze_min_age,
		100000000, 0, 1000000000, NULL, NULL
	},

	{
		{"xid_stop_limit", PGC_POSTMASTER, WAL,
			gettext_noop("Sets the number of XIDs before XID wraparound at which we will no longer allow the system to be started."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&xid_stop_limit,
		1000000000, 10000000, INT_MAX, NULL, NULL
	},
	{
		{"xid_warn_limit", PGC_POSTMASTER, WAL,
			gettext_noop("Sets the number of XIDs before xid_stop_limit at which we will begin emitting warnings regarding XID wraparound."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&xid_warn_limit,
		500000000, 10000000, INT_MAX, NULL, NULL
	},
	{
		{"max_fsm_relations", PGC_POSTMASTER, RESOURCES_FSM,
			gettext_noop("Sets the maximum number of tables and indexes for which free space is tracked."),
		        NULL
		},
		&MaxFSMRelations,
		1000, 100, INT_MAX, NULL, NULL
	},
	{
		{"max_fsm_pages", PGC_POSTMASTER, RESOURCES_FSM,
			gettext_noop("Sets the maximum number of disk pages for which free space is tracked."),
			NULL
		},
		&MaxFSMPages,
		20000, 1000, INT_MAX, NULL, NULL
	},

	{
		{"max_locks_per_transaction", PGC_POSTMASTER, LOCK_MANAGEMENT,
			gettext_noop("Sets the maximum number of locks per transaction."),
			gettext_noop("The shared lock table is sized on the assumption that "
			  "at most max_locks_per_transaction * max_connections distinct "
						 "objects will need to be locked at any one time.")
		},
		&max_locks_per_xact,
		128, 10, INT_MAX, NULL, NULL
	},

	{
		{"authentication_timeout", PGC_SIGHUP, CONN_AUTH_SECURITY,
			gettext_noop("Sets the maximum time in seconds to complete client authentication."),
			NULL,
			GUC_UNIT_S
		},
		&AuthenticationTimeout,
		60, 1, 600, NULL, NULL
	},

	{
		/* Not for general use */
		{"pre_auth_delay", PGC_SIGHUP, DEVELOPER_OPTIONS,
			gettext_noop("no description available"),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL | GUC_UNIT_S
		},
		&PreAuthDelay,
		0, 0, 60, NULL, NULL
	},

	{
		{"checkpoint_segments", PGC_SIGHUP, WAL_CHECKPOINTS,
			gettext_noop("Sets the maximum distance in log segments between automatic WAL checkpoints."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&CheckPointSegments,
		8, 1, INT_MAX, NULL, NULL
	},

	{
		{"checkpoint_timeout", PGC_SIGHUP, WAL_CHECKPOINTS,
			gettext_noop("Sets the maximum time in seconds between automatic WAL checkpoints."),
			NULL,
			GUC_UNIT_S | GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL | GUC_DISALLOW_USER_SET
		},
		&CheckPointTimeout,
		300, 30, 3600, NULL, NULL
	},

	{
		{"checkpoint_warning", PGC_SIGHUP, WAL_CHECKPOINTS,
			gettext_noop("Logs if filling of checkpoint segments happens more "
						 "frequently than this (in seconds)."),
			gettext_noop("Write a message to the server log if checkpoints "
			"caused by the filling of checkpoint segment files happens more "
						 "frequently than this number of seconds. Zero turns off the warning."),
			GUC_UNIT_S | GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&CheckPointWarning,
		30, 0, INT_MAX, NULL, NULL
	},

	{
		{"wal_buffers", PGC_POSTMASTER, WAL_SETTINGS,
			gettext_noop("Sets the number of disk-page buffers in shared memory for WAL."),
			NULL,
			GUC_UNIT_XBLOCKS | GUC_NOT_IN_SAMPLE
		},
		&XLOGbuffers,
		8, 4, INT_MAX, NULL, NULL
	},

	{
		{"commit_delay", PGC_USERSET, WAL_CHECKPOINTS,
			gettext_noop("Sets the delay in microseconds between transaction commit and "
						 "flushing WAL to disk."),
			NULL,
			GUC_GPDB_ADDOPT | GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL | GUC_DISALLOW_USER_SET
		},
		&CommitDelay,
		0, 0, 100000, NULL, NULL
	},

	{
		{"commit_siblings", PGC_USERSET, WAL_CHECKPOINTS,
			gettext_noop("Sets the minimum concurrent open transactions before performing "
						 "commit_delay."),
			NULL,
			GUC_GPDB_ADDOPT | GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL | GUC_DISALLOW_USER_SET
		},
		&CommitSiblings,
		5, 1, 1000, NULL, NULL
	},

	{
		{"extra_float_digits", PGC_USERSET, CLIENT_CONN_LOCALE,
			gettext_noop("Sets the number of digits displayed for floating-point values."),
			gettext_noop("This affects real, double precision, and geometric data types. "
			 "The parameter value is added to the standard number of digits "
						 "(FLT_DIG or DBL_DIG as appropriate).")
		},
		&extra_float_digits,
		0, -15, 2, NULL, NULL
	},

	{
		{"log_min_duration_statement", PGC_SUSET, LOGGING_WHEN,
			gettext_noop("Sets the minimum execution time in milliseconds above which statements will "
						 "be logged."),
			gettext_noop("Zero prints all queries. The default is -1 (turning this feature off)."),
			GUC_UNIT_MS | GUC_GPDB_ADDOPT
		},
		&log_min_duration_statement,
		-1, -1, INT_MAX / 1000, NULL, NULL
	},

	{
		{"bgwriter_delay", PGC_SIGHUP, RESOURCES,
			gettext_noop("Background writer sleep time between rounds in milliseconds"),
			NULL,
			GUC_UNIT_MS | GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&BgWriterDelay,
		200, 10, 10000, NULL, NULL
	},

	{
		{"bgwriter_lru_maxpages", PGC_SIGHUP, RESOURCES,
			gettext_noop("Background writer maximum number of LRU pages to flush per round"),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&bgwriter_lru_maxpages,
		5, 0, 1000, NULL, NULL
	},

	{
		{"bgwriter_all_maxpages", PGC_SIGHUP, RESOURCES,
			gettext_noop("Background writer maximum number of all pages to flush per round"),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&bgwriter_all_maxpages,
		5, 0, 1000, NULL, NULL
	},

	{
		{"log_rotation_age", PGC_SIGHUP, LOGGING_WHERE,
			gettext_noop("Automatic log file rotation will occur after N minutes"),
			NULL,
			GUC_UNIT_MIN
		},
		&Log_RotationAge,
		HOURS_PER_DAY * MINS_PER_HOUR, 0, INT_MAX / MINS_PER_HOUR, NULL, NULL
	},

	{
		{"log_rotation_size", PGC_SIGHUP, LOGGING_WHERE,
			gettext_noop("Automatic log file rotation will occur after N kilobytes"),
			NULL,
			GUC_UNIT_KB
		},
		&Log_RotationSize,
		0, 0, INT_MAX / 1024, NULL, NULL
	},

	{
		{"max_function_args", PGC_INTERNAL, PRESET_OPTIONS,
			gettext_noop("Shows the maximum number of function arguments."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&max_function_args,
		FUNC_MAX_ARGS, FUNC_MAX_ARGS, FUNC_MAX_ARGS, NULL, NULL
	},

	{
		{"max_index_keys", PGC_INTERNAL, PRESET_OPTIONS,
			gettext_noop("Shows the maximum number of index keys."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&max_index_keys,
		INDEX_MAX_KEYS, INDEX_MAX_KEYS, INDEX_MAX_KEYS, NULL, NULL
	},

	{
		{"max_identifier_length", PGC_INTERNAL, PRESET_OPTIONS,
			gettext_noop("Shows the maximum identifier length"),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&max_identifier_length,
		NAMEDATALEN - 1, NAMEDATALEN - 1, NAMEDATALEN - 1, NULL, NULL
	},

	{
		{"gp_dbid", PGC_POSTMASTER, PRESET_OPTIONS,
			gettext_noop("The dbid used by this server."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&GpIdentity.dbid,
		UNINITIALIZED_GP_IDENTITY_VALUE, INT_MIN, INT_MAX, NULL, NULL
	},

    {
		{"gp_contentid", PGC_POSTMASTER, PRESET_OPTIONS,
			gettext_noop("The contentid used by this server."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&GpIdentity.segindex,
		UNINITIALIZED_GP_IDENTITY_VALUE, INT_MIN, INT_MAX, NULL, NULL
	},

    {
		{"gp_num_contents_in_cluster", PGC_POSTMASTER, PRESET_OPTIONS,
			gettext_noop("Sets the number of segments in the cluster."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&GpIdentity.numsegments,
		UNINITIALIZED_GP_IDENTITY_VALUE, INT_MIN, INT_MAX, NULL, NULL
	},

	{
		{"gp_standby_dbid", PGC_POSTMASTER, PRESET_OPTIONS,
			gettext_noop("Sets DBID of standby master."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&GpStandbyDbid,
		0, INT_MIN, 32767, NULL, NULL
	},

	{
		{"gp_max_csv_line_length", PGC_USERSET, EXTERNAL_TABLES,
			gettext_noop("Maximum allowed length of a csv input data row in bytes"),
			NULL,
		},
		&gp_max_csv_line_length,
		1*1024*1024, 32*1024, 4*1024*1024, NULL, NULL
	},

	{
		{"block_size", PGC_INTERNAL, PRESET_OPTIONS,
			gettext_noop("Shows size of a disk block"),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&block_size,
		BLCKSZ, BLCKSZ, BLCKSZ, NULL, NULL
	},

	{
		{"autovacuum_naptime", PGC_SIGHUP, DEFUNCT_OPTIONS,
			gettext_noop("Time to sleep between autovacuum runs, in seconds."),
			NULL,
			GUC_UNIT_S | GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&autovacuum_naptime,
		60, 1, INT_MAX, NULL, NULL
	},
	{
		{"autovacuum_vacuum_threshold", PGC_SIGHUP, DEFUNCT_OPTIONS,
			gettext_noop("Minimum number of tuple updates or deletes prior to vacuum."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&autovacuum_vac_thresh,
		500, 0, INT_MAX, NULL, NULL
	},
	{
		{"autovacuum_analyze_threshold", PGC_SIGHUP, DEFUNCT_OPTIONS,
			gettext_noop("Minimum number of tuple inserts, updates or deletes prior to analyze."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&autovacuum_anl_thresh,
		250, 0, INT_MAX, NULL, NULL
	},
	{
		/* see varsup.c for why this is PGC_POSTMASTER not PGC_SIGHUP */
		{"autovacuum_freeze_max_age", PGC_POSTMASTER, DEFUNCT_OPTIONS,
			gettext_noop("Age at which to autovacuum a table to prevent transaction ID wraparound."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&autovacuum_freeze_max_age,
		200000000, 1000, 2000000000, NULL, NULL
	},

	{
		{"tcp_keepalives_idle", PGC_USERSET, CLIENT_CONN_OTHER,
			gettext_noop("Seconds between issuing TCP keepalives."),
			gettext_noop("A value of 0 uses the system default."),
			GUC_UNIT_S
		},
		&tcp_keepalives_idle,
		0, 0, INT_MAX, assign_tcp_keepalives_idle, show_tcp_keepalives_idle
	},

	{
		{"tcp_keepalives_interval", PGC_USERSET, CLIENT_CONN_OTHER,
			gettext_noop("Seconds between TCP keepalive retransmits."),
			gettext_noop("A value of 0 uses the system default."),
			GUC_UNIT_S
		},
		&tcp_keepalives_interval,
		0, 0, INT_MAX, assign_tcp_keepalives_interval, show_tcp_keepalives_interval
	},

	{
		{"tcp_keepalives_count", PGC_USERSET, CLIENT_CONN_OTHER,
			gettext_noop("Maximum number of TCP keepalive retransmits."),
			gettext_noop("This controls the number of consecutive keepalive retransmits that can be "
						 "lost before a connection is considered dead. A value of 0 uses the "
						 "system default."),
		},
		&tcp_keepalives_count,
		0, 0, INT_MAX, assign_tcp_keepalives_count, show_tcp_keepalives_count
	},

	/* Solaris doesn't support setting SO_SNDTIMEO, so setting this won't work on Solaris (MPP-22526) */
	{
		{"gp_connection_send_timeout", PGC_SIGHUP, CLIENT_CONN_OTHER,
			gettext_noop("Timeout for sending data to unresponsive clients (seconds)"),
			gettext_noop("A value of 0 uses the system default."),
		},
		&gp_connection_send_timeout,
		3600, 0, INT_MAX, NULL, NULL
	},

	{
		{"gp_filerep_tcp_keepalives_idle", PGC_USERSET, GP_ARRAY_TUNING,
			gettext_noop("Seconds between issuing TCP keepalives for FileRep connection."),
			gettext_noop("A value of 0 uses the system default."),
			GUC_UNIT_S
		},
		&gp_filerep_tcp_keepalives_idle,
		60, 0, INT_MAX, NULL, NULL
	},

	{
		{"gp_filerep_tcp_keepalives_interval", PGC_USERSET, GP_ARRAY_TUNING,
			gettext_noop("Seconds between TCP keepalive retransmits for FileRep connection."),
			gettext_noop("A value of 0 uses the system default."),
			GUC_UNIT_S
		},
		&gp_filerep_tcp_keepalives_interval,
		30, 0, INT_MAX, NULL, NULL
	},

	{
		{"gp_filerep_tcp_keepalives_count", PGC_USERSET, GP_ARRAY_TUNING,
			gettext_noop("Maximum number of TCP keepalive retransmits for FileRep connection."),
			gettext_noop("This controls the number of consecutive keepalive retransmits that can be "
						 "lost before a connection is considered dead. A value of 0 uses the "
						 "system default."),
		},
		&gp_filerep_tcp_keepalives_count,
		2, 0, INT_MAX, NULL, NULL
	},


	{
		{"max_resource_queues", PGC_POSTMASTER, RESOURCES_MGM,
			gettext_noop("Maximum number of resource queues."),
			NULL
		},
		&MaxResourceQueues,
		9, 0, INT_MAX, NULL, NULL
	},

	{
		{"max_resource_portals_per_transaction", PGC_POSTMASTER, RESOURCES_MGM,
			gettext_noop("Maximum number of resource queues."),
			NULL
		},
		&MaxResourcePortalsPerXact,
		64, 0, INT_MAX, NULL, NULL
	},

	{
		{"max_appendonly_tables", PGC_POSTMASTER, APPENDONLY_TABLES,
			gettext_noop("Maximum number of different (unrelated) append only tables that can participate in writing data concurrently."),
			NULL
		},
		&MaxAppendOnlyTables,
		2048, 0, INT_MAX, NULL, NULL
	},

	{
		{"test_appendonly_version_default", PGC_USERSET, APPENDONLY_TABLES,
		 gettext_noop("Align append-only blocks to 64 bits."),
		 NULL,
		 GUC_GPDB_ADDOPT | GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&test_appendonly_version_default,
		AORelationVersion_GetLatest(), 0, INT_MAX, NULL, NULL
	},

	{
		{"gp_external_max_segs", PGC_USERSET, EXTERNAL_TABLES,
			gettext_noop("Maximum number of segments that connect to a single gpfdist URL."),
			NULL
		},
		&gp_external_max_segs,
		64, 1, INT_MAX, NULL, NULL
    },

	{
		{"gp_max_packet_size", PGC_BACKEND, GP_ARRAY_TUNING,
            gettext_noop("Sets the max packet size for the Interconnect."),
            NULL,
			GUC_GPDB_ADDOPT
        },
        &Gp_max_packet_size,
        DEFAULT_PACKET_SIZE, MIN_PACKET_SIZE, MAX_PACKET_SIZE, NULL, NULL
	},

	{
		{"gp_interconnect_queue_depth", PGC_USERSET, GP_ARRAY_TUNING,
            gettext_noop("Sets the maximum size of the receive queue for each connection in the UDP interconnect"),
            NULL,
			GUC_GPDB_ADDOPT
        },
        &Gp_interconnect_queue_depth,
        4, 1, 4096, NULL, NULL
	},

	{
		{"gp_interconnect_snd_queue_depth", PGC_USERSET, GP_ARRAY_TUNING,
            gettext_noop("Sets the maximum size of the send queue for each connection in the UDP interconnect"),
            NULL,
			GUC_GPDB_ADDOPT
        },
        &Gp_interconnect_snd_queue_depth,
        2, 1, 4096, NULL, NULL
	},

	{
		{"gp_interconnect_timer_period", PGC_USERSET, GP_ARRAY_TUNING,
            gettext_noop("Sets the timer period (in ms) for UDP interconnect"),
            NULL,
            GUC_UNIT_MS | GUC_GPDB_ADDOPT
        },
        &Gp_interconnect_timer_period,
        5, 1, 100, NULL, NULL
	},

	{
		{"gp_interconnect_timer_checking_period", PGC_USERSET, GP_ARRAY_TUNING,
            gettext_noop("Sets the timer checking period (in ms) for UDP interconnect"),
            NULL,
            GUC_UNIT_MS | GUC_GPDB_ADDOPT
        },
        &Gp_interconnect_timer_checking_period,
        20, 1, 100, NULL, NULL
	},

	{
		{"gp_interconnect_default_rtt", PGC_USERSET, GP_ARRAY_TUNING,
            gettext_noop("Sets the default rtt (in ms) for UDP interconnect"),
            NULL,
            GUC_UNIT_MS | GUC_GPDB_ADDOPT
        },
        &Gp_interconnect_default_rtt,
        20, 1, 1000, NULL, NULL
	},

	{
		{"gp_interconnect_min_rto", PGC_USERSET, GP_ARRAY_TUNING,
            gettext_noop("Sets the min rto (in ms) for UDP interconnect"),
            NULL,
            GUC_UNIT_MS | GUC_GPDB_ADDOPT
        },
        &Gp_interconnect_min_rto,
        20, 1, 1000, NULL, NULL
	},

	{
		{"gp_interconnect_transmit_timeout", PGC_USERSET, GP_ARRAY_TUNING,
			gettext_noop("Timeout (in seconds) on interconnect to transmit a packet"),
			gettext_noop("Used by Interconnect to timeout packet transmission."),
			GUC_UNIT_S | GUC_GPDB_ADDOPT
		},
		&Gp_interconnect_transmit_timeout,
		3600, 1, 7200, NULL, NULL
	},

	{
		{"gp_interconnect_min_retries_before_timeout", PGC_USERSET, GP_ARRAY_TUNING,
            gettext_noop("Sets the min retries before reporting a transmit timeout in the interconnect."),
            NULL,
			GUC_GPDB_ADDOPT
        },
        &Gp_interconnect_min_retries_before_timeout,
        100, 1, 4096, NULL, NULL
	},

	{
		{"gp_udp_bufsize_k", PGC_BACKEND, GP_ARRAY_TUNING,
            gettext_noop("Sets recv buf size of UDP interconnect, for testing."),
            NULL,
            GUC_GPDB_ADDOPT
        },
        &Gp_udp_bufsize_k,
        0, 0, 32768, NULL, NULL
	},

#ifdef USE_ASSERT_CHECKING
	{
		{"gp_udpic_dropseg", PGC_USERSET, GP_ARRAY_TUNING,
            gettext_noop("Specifies a segment to which the dropacks, and dropxmit settings will be applied, for testing. (The default is to apply the dropacks and dropxmit settings to all segments)"),
            NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
        },
        &gp_udpic_dropseg,
        UNDEF_SEGMENT, UNDEF_SEGMENT, INT_MAX, NULL, NULL
	},

	{
		{"gp_udpic_dropacks_percent", PGC_USERSET, GP_ARRAY_TUNING,
            gettext_noop("Sets the percentage of correctly-received acknowledgment packets to synthetically drop, for testing. (affected by gp_udpic_dropseg)"),
            NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
        },
        &gp_udpic_dropacks_percent,
        0, 0, 100, NULL, NULL
	},

	{
		{"gp_udpic_dropxmit_percent", PGC_USERSET, GP_ARRAY_TUNING,
            gettext_noop("Sets the percentage of correctly-received data packets to synthetically drop, for testing. (affected by gp_udpic_dropseg)"),
            NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
        },
        &gp_udpic_dropxmit_percent,
        0, 0, 100, NULL, NULL
	},

	{
		{"gp_udpic_fault_inject_percent", PGC_USERSET, GP_ARRAY_TUNING,
			gettext_noop("Sets the percentage of fault injected into system calls, for testing. (affected by gp_udpic_dropseg)"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_udpic_fault_inject_percent,
		0, 0, 100, NULL, NULL
	},

	{
		{"gp_udpic_fault_inject_bitmap", PGC_USERSET, GP_ARRAY_TUNING,
			gettext_noop("Sets the bitmap for faults injection, for testing. (affected by gp_udpic_dropseg)"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_udpic_fault_inject_bitmap,
		0, 0, INT_MAX, NULL, NULL
	},

	{
		{"gp_udpic_network_disable_ipv6", PGC_USERSET, GP_ARRAY_TUNING,
			gettext_noop("Sets the address info hint to disable the ipv6, for testing. (affected by gp_udpic_dropseg)"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_udpic_network_disable_ipv6,
		0, 0, 1, NULL, NULL
	},

#endif
	{
		{"gp_interconnect_hash_multiplier", PGC_SUSET, GP_ARRAY_TUNING,
            gettext_noop("Sets the number of hash buckets used by the UDP interconnect to track connections (the number of buckets is given by the product of the segment count and the hash multipliers)."),
            NULL,
        },
        &Gp_interconnect_hash_multiplier,
        2, 1, 256, NULL, NULL
	},

	{
		{"gp_command_count", PGC_INTERNAL, CLIENT_CONN_OTHER,
			gettext_noop("Shows the number of commands received from the client in this session."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&gp_command_count,
		0, 0, INT_MAX, NULL, NULL
	},

	{
		{"gp_connections_per_thread", PGC_SUSET, GP_ARRAY_TUNING,
			gettext_noop("Sets the number of client connections handled in each thread."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_connections_per_thread,
		512, 0, INT_MAX, assign_gp_connections_per_thread, show_gp_connections_per_thread
	},

	{
		{"gp_subtrans_warn_limit", PGC_POSTMASTER, RESOURCES,
			gettext_noop("Sets the warning limit on number of subtransactions in a transaction."),
			NULL,
			GUC_NOT_IN_SAMPLE
		},
		&gp_subtrans_warn_limit,
		16777216, 0, INT_MAX, NULL, NULL
	},

	{
		{"gp_cached_segworkers_threshold", PGC_USERSET, GP_ARRAY_TUNING,
			gettext_noop("Sets the maximum number of segment workers to cache between statements."),
			NULL,
			GUC_NOT_IN_SAMPLE
		},
		&gp_cached_gang_threshold,
		5, 0, INT_MAX, NULL, NULL
		},


	{
#ifdef USE_ASSERT_CHECKING
		{"gp_debug_linger", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Number of seconds for QD/QE process to linger upon fatal internal error."),
			gettext_noop("Allows an opportunity to debug the backend process before it terminates."),
			GUC_NOT_IN_SAMPLE | GUC_NO_RESET_ALL | GUC_UNIT_S | GUC_GPDB_ADDOPT
		},
		&gp_debug_linger,
		120, 0, 3600, NULL, NULL
#else
		{"gp_debug_linger", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Number of seconds for QD/QE process to linger upon fatal internal error."),
			gettext_noop("Allows an opportunity to debug the backend process before it terminates."),
			GUC_NOT_IN_SAMPLE | GUC_NO_RESET_ALL | GUC_UNIT_S | GUC_GPDB_ADDOPT
		},
		&gp_debug_linger,
		0, 0, 3600, NULL, NULL
#endif
	},

	{
		{"gp_segment", PGC_BACKEND, GP_WORKER_IDENTITY,
			gettext_noop("Segment id of the segment db which is local to this worker process."),
			gettext_noop("-1 for a session's entry process (qDisp) or a worker on the entry db."),
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&Gp_segment,
		-999, INT_MIN, INT_MAX, NULL, NULL
	},

	{
		{"gp_qd_port", PGC_BACKEND, GP_WORKER_IDENTITY,
			gettext_noop("Shows the Master Postmaster port."),
			gettext_noop("0 for a session's entry process (qDisp)"),
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&qdPostmasterPort,
		0, INT_MIN, INT_MAX, NULL, NULL
	},


	{
		{"gp_sort_flags", PGC_USERSET, QUERY_TUNING_OTHER,
			gettext_noop("Experimental feature: Generic sort flags."),
			NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_sort_flags,
		10000, 0, INT_MAX, NULL, NULL
	},

	{
		{"gp_sort_max_distinct", PGC_USERSET, QUERY_TUNING_OTHER,
			gettext_noop("Experimental feature: max number of distinct values for sort."),
			NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_sort_max_distinct,
		20000, 0, INT_MAX, NULL, NULL
	},

	{
		{"gp_interconnect_setup_timeout", PGC_USERSET, GP_ARRAY_TUNING,
			gettext_noop("Timeout (in seconds) on interconnect setup that occurs at query start"),
			gettext_noop("Used by Interconnect to timeout the setup of the communication fabric."),
			GUC_UNIT_S | GUC_GPDB_ADDOPT
		},
		&interconnect_setup_timeout,
		7200, 0, 7200, NULL, NULL
	},

	{
		{"gp_snapshotadd_timeout", PGC_USERSET, GP_ARRAY_TUNING,
			gettext_noop("Timeout (in seconds) on setup of new connection snapshot"),
			gettext_noop("Used by the transaction manager."),
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_UNIT_S | GUC_GPDB_ADDOPT
		},
		&gp_snapshotadd_timeout,
		10, 0, INT_MAX, NULL, NULL
	},

	{
		{"gp_segment_connect_timeout", PGC_USERSET, GP_ARRAY_TUNING,
			gettext_noop("Maximum time (in seconds) allowed for a new worker process to start or a mirror to respond."),
			gettext_noop("0 indicates 'wait forever'."),
			GUC_UNIT_S
		},
		&gp_segment_connect_timeout,
		180, 0, INT_MAX, NULL, NULL
	},

	{
		{"verify_checkpoint_interval", PGC_POSTMASTER, DEVELOPER_OPTIONS,
			gettext_noop("set the online verification checkpoint interval (seconds)"),
		 gettext_noop("0 means do not checkpoint"),
			GUC_UNIT_S | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&verify_checkpoint_interval,
		VERIFY_CHECKPOINT_INTERVAL_DEFAULT, 0, 1800, NULL, NULL
	},

	{
		{"gp_fts_probe_retries", PGC_POSTMASTER, GP_ARRAY_TUNING,
			gettext_noop("Number of retries for FTS to complete probing a segment."),
			gettext_noop("Used by the fts-probe process."),
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_fts_probe_retries,
		5, 0, 100, NULL, NULL
	},

	{
		{"gp_fts_probe_timeout", PGC_USERSET, GP_ARRAY_TUNING,
			gettext_noop("Maximum time (in seconds) allowed for FTS to complete probing a segment."),
			gettext_noop("Used by the fts-probe process."),
			GUC_UNIT_S
		},
		&gp_fts_probe_timeout,
		20, 0, INT_MAX, NULL, NULL
	},

	{
		{"gp_fts_probe_interval", PGC_POSTMASTER, GP_ARRAY_TUNING,
			gettext_noop("A complete probe of all segments starts each time a timer with this period expires."),
			gettext_noop("Used by the fts-probe process. "),
			GUC_UNIT_S
		},
		&gp_fts_probe_interval,
		60, 10, INT_MAX, NULL, NULL
	},

	{
		{"gp_fts_probe_threadcount", PGC_POSTMASTER, GP_ARRAY_TUNING,
			gettext_noop("Use this number of threads for probing the segments."),
			gettext_noop("The number of threads to create at each probe interval expiration."),
			GUC_NOT_IN_SAMPLE
		},
		&gp_fts_probe_threadcount,
		16, 1, 128, NULL, NULL
	},

	{
		{"gp_fts_transition_retries", PGC_POSTMASTER, GP_ARRAY_TUNING,
			gettext_noop("The number of retries for FTS to request a segment state transition."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&gp_fts_transition_retries,
		5, 1, 100, NULL, NULL
	},

	{
		{"gp_fts_transition_timeout", PGC_POSTMASTER, GP_ARRAY_TUNING,
			gettext_noop("Timeout (in seconds) for FTS to request a segment state transition."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL  | GUC_UNIT_S
		},
		&gp_fts_transition_timeout,
		3600, 1, 36000, NULL, NULL
	},

	{
		{"gp_gang_creation_retry_count", PGC_USERSET, GP_ARRAY_TUNING,
		 gettext_noop("After a gang-creation fails, retry the number of times if failure is retryable."),
		 gettext_noop("A value of zero disables retries."),
		 GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_gang_creation_retry_count,
		5, 0, 128, NULL, NULL
	},

	{
		{"gp_gang_creation_retry_timer", PGC_USERSET, GP_ARRAY_TUNING,
		 gettext_noop("Wait this many milliseconds between gang-creation-retries."),
		 NULL,
		 GUC_UNIT_MS | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_gang_creation_retry_timer,
		2000, 100, 120000, NULL, NULL
	},

	{
		{"gp_session_id", PGC_BACKEND, CLIENT_CONN_OTHER,
			gettext_noop("Global ID used to uniquely identify a particular session in an Greenplum Database array"),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&gp_session_id,
		-1, INT_MIN, INT_MAX, NULL, NULL
	},

	{
		{"gp_segments_for_planner", PGC_USERSET, QUERY_TUNING_COST,
            gettext_noop("If >0, number of segment dbs for the planner to assume in its cost and size estimates."),
            gettext_noop("If 0, estimates are based on the actual number of segment dbs.")
		},
		&gp_segments_for_planner,
		0, 0, INT_MAX, NULL, NULL
	},

	{
		{"gp_qd_proc_offset", PGC_BACKEND, GP_WORKER_IDENTITY,
			gettext_noop("The shmmem offset of the original QD backend that is handling this query"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		(int *)&gp_qd_proc_offset,
		-1, -1, INT_MAX, NULL, NULL
	},

	{
		{"gp_hashjoin_tuples_per_bucket", PGC_USERSET, GP_ARRAY_TUNING,
		 gettext_noop("Target density of hashtable used by Hashjoin during execution"),
		 gettext_noop("A smaller value will tend to produce larger hashtables, which increases join performance"),
		 GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_hashjoin_tuples_per_bucket,
		5, 1, 25, NULL, NULL
	},

	{
		{"gp_hashjoin_metadata_memory_percent", PGC_USERSET, GP_ARRAY_TUNING,
		gettext_noop("Percentage of the operator memory allowed to store hashtable metadata. Set to 0 for unlimited amount of metadata memory."),
		gettext_noop("A small value can cause certain queries to fail, a large or unbounded value can cause the system to run out of memory"),
		GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL | GUC_GPDB_ADDOPT
		},
		&gp_hashjoin_metadata_memory_percent,
		20, 0, INT_MAX, NULL, NULL
	},

	{
		{"gp_hashagg_groups_per_bucket", PGC_USERSET, GP_ARRAY_TUNING,
		 gettext_noop("Target density of hashtable used by Hashagg during execution"),
		 gettext_noop("A smaller value will tend to produce larger hashtables, which increases agg performance"),
		 GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL | GUC_GPDB_ADDOPT
		},
		&gp_hashagg_groups_per_bucket,
		5, 1, 25, NULL, NULL
	},

	{
		{"gp_hashagg_default_nbatches", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Default number of batches for hashagg's (re-)spilling phases"),
			NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_hashagg_default_nbatches,
		32, 4, 1000000, NULL, NULL
	},

	{
		{"gp_hashagg_spillbatch_min", PGC_USERSET, GP_ARRAY_TUNING,
		 gettext_noop("(Obsolete) Minimum number of spill batches of HashAgg"),
		 gettext_noop("Controlling number of spill batches, if too small, hashagg may respill"),
		 GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&gp_hashagg_spillbatch_min,
		0, 0, 1000000, NULL, NULL
	},

	{
		{"gp_hashagg_spillbatch_max", PGC_USERSET, GP_ARRAY_TUNING,
		 gettext_noop("(Obsolete) Maximum number of spill batches of HashAgg"),
		 gettext_noop("Controlling number of spill batches, if too big, hashagg spill I/O will be slow"),
		 GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&gp_hashagg_spillbatch_max,
		0, 0, 1000000, NULL, NULL
	},

	{
		{"gp_hashjoin_bloomfilter", PGC_USERSET, GP_ARRAY_TUNING,
		 gettext_noop("Use bloomfilter in hash join"),
		 gettext_noop("Use bloomfilter may speed up hashtable probing"),
		 GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL | GUC_GPDB_ADDOPT
		},
		&gp_hashjoin_bloomfilter,
		1, 0, 1, NULL, NULL
	},

	{
		{"gp_motion_slice_noop", PGC_USERSET, GP_ARRAY_TUNING,
		 gettext_noop("Make motion nodes in certain slices noop"),
		 gettext_noop("Make motion nodes noop, to help analyze performace"),
		 GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL | GUC_GPDB_ADDOPT
		},
		&gp_motion_slice_noop,
		0, 0, INT_MAX, NULL, NULL
	},

#ifdef ENABLE_LTRACE
	{
		{"gp_ltrace_flag", PGC_USERSET, GP_ARRAY_TUNING,
		 gettext_noop("Linux Tracing flag"),
		 gettext_noop("Linux Tracing flag"),
		 GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL | GUC_GPDB_ADDOPT
		},
		&gp_ltrace_flag,
		0, 0, INT_MAX, NULL, NULL
	},
#endif

	{
		{"gp_reject_percent_threshold", PGC_USERSET, GP_ERROR_HANDLING,
			gettext_noop("Reject limit in percent starts calculating after this number of rows processed"),
			NULL
		},
		&gp_reject_percent_threshold,
		300, 0, INT_MAX, NULL, NULL
	},

	{
		{"gp_gpperfmon_send_interval", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Interval in seconds between sending messages to gpperfmon."),
			NULL,
			GUC_GPDB_ADDOPT
		},
		&gp_gpperfmon_send_interval,
		1, 1, 3600, gpvars_assign_gp_gpperfmon_send_interval, NULL
	},

	/* MPP-9413: gin indexes are disabled */
	{
		{"gin_fuzzy_search_limit", PGC_USERSET, CLIENT_CONN_OTHER,
			gettext_noop("Sets the maximum allowed result for exact search by GIN."),
			NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&GinFuzzySearchLimit,
		0, 0, INT_MAX, NULL, NULL
	},

	{
		{"effective_cache_size", PGC_USERSET, QUERY_TUNING_COST,
			gettext_noop("Sets the planner's assumption about size of the disk cache."),
			gettext_noop("That is, the portion of the kernel's disk cache that "
						 "will be used for PostgreSQL data files. This is measured in disk "
						 "pages, which are normally 8 kB each."),
			GUC_UNIT_BLOCKS,
		},
		&effective_cache_size,
		DEFAULT_EFFECTIVE_CACHE_SIZE, 1, INT_MAX, NULL, NULL
	},

	{
		/* Can't be set in postgresql.conf */
		{"server_version_num", PGC_INTERNAL, PRESET_OPTIONS,
			gettext_noop("Shows the server version as an integer."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&server_version_num,
		PG_VERSION_NUM, PG_VERSION_NUM, PG_VERSION_NUM, NULL, NULL
	},

	{
		{"wal_send_client_timeout", PGC_SIGHUP, GP_ARRAY_TUNING,
			gettext_noop("The time in milliseconds for a backend process to wait on the WAL Send server to finish a request to the QD mirroring standby."),
			NULL,
			GUC_UNIT_MS | GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&WalSendClientTimeout,
		30000, 100, INT_MAX/1000, NULL, NULL
	},

	{
		{"gpperfmon_port", PGC_POSTMASTER, UNGROUPED,
			gettext_noop("Sets the port number of gpperfmon."),
			NULL,
		},
		&gpperfmon_port,
		8888, 1024, 65535, NULL, NULL
	},

	{
		{"gp_vmem_protect_limit", PGC_POSTMASTER, RESOURCES_MEM,
			gettext_noop("Virtual memory limit (in MB) of Greenplum memory protection."),
			NULL,
		},
		&gp_vmem_protect_limit,
#ifdef __darwin__
        0,
#else
        8192,
#endif
            0, INT_MAX / 2, NULL, NULL
	},

	{
		{"runaway_detector_activation_percent", PGC_POSTMASTER, RESOURCES_MEM,
			gettext_noop("The runaway detector activates if the used vmem exceeds this percentage of the vmem quota. Set to 100 to disable runaway detection."),
			NULL,
		},
		&runaway_detector_activation_percent,
		90, 0, 100, NULL, NULL
	},

	{
		{"gp_vmem_protect_segworker_cache_limit", PGC_POSTMASTER, RESOURCES_MEM,
			gettext_noop("Max virtual memory limit (in MB) for a segworker to be cachable."),
			NULL,
		},
		&gp_vmem_protect_gang_cache_limit,
        500, 1, INT_MAX / 2, NULL, NULL
	},

	{
		{"gp_autostats_on_change_threshold", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Threshold for number of tuples added to table by CTAS or Insert-to to trigger autostats in on_change mode. See gp_autostats_mode."),
			NULL
		},
		&gp_autostats_on_change_threshold,
		INT_MAX, 0, INT_MAX, NULL, NULL
	},

	{
		{"gp_distinct_grouping_sets_threshold", PGC_USERSET, QUERY_TUNING_METHOD,
		 gettext_noop("Threshold for the number of grouping sets whose distinct-qualified "
					  "aggregate computation will be rewritten based on the multi-phrase aggregation. "
					  "The rest of grouping sets in the same query will not be rewritten."),
		 NULL,
		 GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_distinct_grouping_sets_threshold,
		32, 0, 1024, NULL, NULL
	},

	{
		{"gp_dbg_flags", PGC_USERSET, QUERY_TUNING_OTHER,
			gettext_noop("Experimental feature: Generic sort flags."),
			NULL,
            GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_dbg_flags,
		0, 0, INT_MAX, NULL, NULL
	},
	{
		{"gp_statistics_blocks_target", PGC_USERSET, STATS_ANALYZE,
			gettext_noop("The number of blocks to be sampled to estimate reltuples/relpages for heap tables."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_statistics_blocks_target,
		25, 1, 1000, NULL, NULL
	},
	{
		{"gp_resqueue_priority_local_interval", PGC_POSTMASTER, RESOURCES_MGM,
			gettext_noop("A measure of how often a backend process must consider backing off."),
			NULL,
			GUC_NO_SHOW_ALL
		},
		&gp_resqueue_priority_local_interval,
		100000, 500, INT_MAX, NULL, NULL
	},
	{
		{"gp_resqueue_priority_sweeper_interval", PGC_POSTMASTER, RESOURCES_MGM,
			gettext_noop("Frequency (in ms) at which sweeper process re-evaluates CPU shares."),
			NULL
		},
		&gp_resqueue_priority_sweeper_interval,
		1000, 500, 15000, NULL, NULL
	},
	{
		{"gp_resqueue_priority_inactivity_timeout", PGC_POSTMASTER, RESOURCES_MGM,
			gettext_noop("If a backend does not report progress in this time (in ms), it is deemed inactive."),
			NULL,
			GUC_NO_SHOW_ALL
		},
		&gp_resqueue_priority_inactivity_timeout,
		2000, 500, INT_MAX, NULL, NULL
	},
	{
		{"gp_resqueue_priority_grouping_timeout", PGC_POSTMASTER, RESOURCES_MGM,
			gettext_noop("A backend gives up on finding a better group leader after this timeout (in ms)."),
			NULL,
			GUC_NO_SHOW_ALL
		},
		&gp_resqueue_priority_grouping_timeout,
		1000, 1000, INT_MAX, NULL, NULL
	},

	{
		{"gp_perfmon_segment_interval", PGC_POSTMASTER, STATS,
			gettext_noop("Interval (in ms) between sending segment statistics to perfmon."),
			NULL,
			GUC_NO_SHOW_ALL
		},
		&gp_perfmon_segment_interval,
		1000, 500, INT_MAX, NULL, NULL
	},

	{
		{"filerep_message_body_length", PGC_POSTMASTER, DEVELOPER_OPTIONS,
			gettext_noop("size (in bytes) for filerep message body."),
			NULL,
			GUC_NO_SHOW_ALL
		},
		&file_rep_message_body_length,
		32768, 32768, 262144, NULL, NULL
	},

	{
		{"filerep_buffer_size", PGC_POSTMASTER, DEVELOPER_OPTIONS,
			gettext_noop("size (in bytes) for filerep shared memory"),
			NULL,
			GUC_NO_SHOW_ALL
		},
		&file_rep_buffer_size,
		2097152, 1048576, 16777216, NULL, NULL
	},

	{
		{"filerep_ack_buffer_size", PGC_POSTMASTER, DEVELOPER_OPTIONS,
			gettext_noop("size (in bytes) for filerep ack shared memory"),
			NULL,
			GUC_NO_SHOW_ALL
		},
		&file_rep_ack_buffer_size,
		524288, 131072, 1048576, NULL, NULL
	},

	{
		{"filerep_min_data_before_flush", PGC_POSTMASTER, DEVELOPER_OPTIONS,
			gettext_noop("size (in bytes) that a filerep buffer must reach before the client flushes the data"),
			NULL,
			GUC_NO_SHOW_ALL
		},
		&file_rep_min_data_before_flush,
		128*1024, 8*1024, 16*1024*1024, NULL, NULL
	},

    {
        {"filerep_socket_timeout", PGC_POSTMASTER, DEVELOPER_OPTIONS,
            gettext_noop("Timeout (in seconds) if we cannot write to socket (the socket is blocking)"),
            NULL,
            GUC_NO_SHOW_ALL
        },
        &file_rep_socket_timeout,
        10, 0, 300, NULL, NULL
    },

	{
		{"gp_blockdirectory_entry_min_range", PGC_USERSET, GP_ARRAY_TUNING,
		 gettext_noop("Minimal range in bytes one block directory entry covers."),
		 gettext_noop("Used to reduce the size of a block directory."),
		 GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_blockdirectory_entry_min_range,
		0, 0, INT_MAX, NULL, NULL
	},

	{
		{"gp_blockdirectory_minipage_size", PGC_USERSET, GP_ARRAY_TUNING,
		 gettext_noop("Number of entries one row in a block directory table contains."),
		 gettext_noop("Use smaller value in non-bulk load cases."),
		 GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_blockdirectory_minipage_size,
		NUM_MINIPAGE_ENTRIES, 1, NUM_MINIPAGE_ENTRIES, NULL, NULL
	},


	{
		{"gp_segworker_relative_priority", PGC_POSTMASTER, RESOURCES_MGM,
		gettext_noop("Priority for the segworkers relative to the postmaster's priority."),
		NULL,
		GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_segworker_relative_priority,
		PRIO_MAX,
		0, PRIO_MAX, NULL, NULL
	},

	{
		{"pgstat_track_activity_query_size", PGC_POSTMASTER, UNGROUPED,
		gettext_noop("Maximum length of the query to be displayed in pg_stat_activity"),
		NULL,
		GUC_NOT_IN_SAMPLE
		},
		&pgstat_track_activity_query_size,
		1024, 1024, INT_MAX,
		NULL, NULL
	},

	{
		{"gp_workfile_bytes_to_checksum", PGC_USERSET, GP_ARRAY_TUNING,
		 gettext_noop("The number of bytes to be checksummed in every WORKFILE_SAFEWRITE_SIZE bytes."),
		 NULL,
		 GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_workfile_bytes_to_checksum,
		16, 0, WORKFILE_SAFEWRITE_SIZE, NULL, NULL
	},

	/* for pljava */
	{
		{"pljava_statement_cache_size", PGC_SUSET, CUSTOM_OPTIONS,
			gettext_noop("Size of the prepared statement MRU cache"),
			NULL,
		 GUC_GPDB_ADDOPT | GUC_NOT_IN_SAMPLE | GUC_SUPERUSER_ONLY
		},
		&pljava_statement_cache_size,
		0, 0, 512, NULL, NULL
	},

	/* for SimEx */
	{
		{"gp_simex_class", PGC_POSTMASTER, GP_ERROR_HANDLING,
		 gettext_noop("Simulated Exceptional Situation class."),
		 gettext_noop("Sets the ES class to be simulated. Default value is 0 (Out-Of-Memory)."),
		 GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_simex_class,
		0, 0, INT_MAX, NULL, NULL
	},

	{
		{"gp_test_time_slice_interval", PGC_USERSET, GP_ERROR_HANDLING,
		 gettext_noop("Maximum interval in ms between successive checks for interrupts."),
		 NULL,
		 GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_test_time_slice_interval,
		1000, 1, 10000, NULL, NULL
	},

	{
		{"gp_resqueue_memory_policy_auto_fixed_mem", PGC_USERSET, RESOURCES_MEM,
			gettext_noop("Sets the fixed amount of memory reserved for non-memory intensive operators in the AUTO policy."),
			NULL,
			GUC_UNIT_KB | GUC_GPDB_ADDOPT | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_resqueue_memory_policy_auto_fixed_mem,
		100, 50, INT_MAX, NULL, NULL
	},

       {
            {"gp_backup_directIO_read_chunk_mb", PGC_USERSET, DEVELOPER_OPTIONS,
                    gettext_noop("Size of read Chunk buffer in directIO dump (in MB)"),
                    NULL,
            },
            &gp_backup_directIO_read_chunk_mb,
            20, 1, 200, NULL, NULL
        },

	{
		{"gp_email_connect_timeout", PGC_SUSET, LOGGING,
			gettext_noop("Sets the amount of time (in secs) after which SMTP sockets would timeout"),
			NULL,
			GUC_SUPERUSER_ONLY| GUC_NOT_IN_SAMPLE
		},
		&gp_email_connect_timeout,
		15, 10, 120, NULL, NULL
	},
	{
		{"gp_email_connect_failures", PGC_SUSET, LOGGING,
			gettext_noop("Sets the number of consecutive connect failures before declaring SMTP server as unavailable"),
			NULL,
			GUC_SUPERUSER_ONLY| GUC_NOT_IN_SAMPLE
		},
		&gp_email_connect_failures,
		5, 3, 100, NULL, NULL
	},
	{
		{"gp_email_connect_avoid_duration", PGC_SUSET, LOGGING,
			gettext_noop("Sets the amount of time (in secs) to avoid connecting to SMTP server"),
			NULL,
			GUC_SUPERUSER_ONLY| GUC_NOT_IN_SAMPLE
		},
		&gp_email_connect_avoid_duration,
		7200, 300, 86400, NULL, NULL
	},

	{
		{"optimizer_plan_id", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Choose a plan alternative"),
			NULL,
			GUC_GPDB_ADDOPT | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_plan_id,
		0, 0, INT_MAX, NULL, NULL
	},

	{
		{"optimizer_samples_number", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Set the number of plan samples"),
			NULL,
			GUC_GPDB_ADDOPT | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_samples_number,
		1000, 1, INT_MAX, NULL, NULL
	},

	{
		{"optimizer_cte_inlining_bound", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Set the CTE inlining cutoff"),
			NULL,
			GUC_GPDB_ADDOPT | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_cte_inlining_bound,
		0, 0, INT_MAX, NULL, NULL
	},

	{
		{"optimizer_segments", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Number of segments to be considered by the optimizer during costing, or 0 to take the actual number of segments."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_segments,
		0, 0, INT_MAX, NULL, NULL
	},
	{
		{"memory_profiler_dataset_size", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Set the size in GB"),
			NULL,
			GUC_GPDB_ADDOPT | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&memory_profiler_dataset_size,
		0, 0, INT_MAX, NULL, NULL
	},

	{
		{"keep_wal_segments", PGC_SIGHUP, WAL_SETTINGS,
			gettext_noop("Sets the number of WAL files held for standby servers."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&keep_wal_segments,
		5, 0, INT_MAX, NULL, NULL
	},

	{
		{"wal_receiver_status_interval", PGC_SUSET, WAL_REPLICATION,
			gettext_noop("Sets the maximum interval between WAL receiver "
						 "status reports to the primary. (Master Mirroring)"),
			NULL,
			GUC_UNIT_S
		},
		&wal_receiver_status_interval,
		10, 0, INT_MAX / 1000, NULL, NULL
	},

	{
		{"replication_timeout", PGC_SUSET, WAL_REPLICATION,
			gettext_noop("Sets the maximum time to wait for WAL replication (Master Mirroring)"),
			NULL,
			GUC_UNIT_MS | GUC_SUPERUSER_ONLY
		},
		&replication_timeout,
		60 * 1000, 0, INT_MAX, NULL, NULL
	},
	{
		{"repl_catchup_within_range", PGC_SUSET, WAL_REPLICATION,
			gettext_noop("Sets the maximum number of xlog segments allowed to lag"
						 " when the backends can start blocking despite the WAL"
						 " sender being in catchup phase. (Master Mirroring)"),
			NULL,
			GUC_SUPERUSER_ONLY
		},
		&repl_catchup_within_range,
		1, 0, XLogSegsPerFile, NULL, NULL
	},
	{
		{"gp_initial_bad_row_limit", PGC_USERSET, EXTERNAL_TABLES,
			gettext_noop("Stops processing when number of the first bad rows exceeding this value"),
			NULL,
			GUC_GPDB_ADDOPT
		},
		&gp_initial_bad_row_limit,
		1000, 0, INT_MAX, NULL, NULL
	},

	{
		{"log_count_recovered_files_batch", PGC_POSTMASTER, DEVELOPER_OPTIONS,
			gettext_noop("Logs the total number of files shipped to the mirror after every batch of size specified by this value"),
			NULL,
			GUC_NOT_IN_SAMPLE,
		},
		&log_count_recovered_files_batch,
		1000, 0, INT_MAX, NULL, NULL
	},

	/* End-of-list marker */
	{
		{NULL, 0, 0, NULL, NULL}, NULL, 0, 0, 0, NULL, NULL
	}
};


static struct config_real ConfigureNamesReal[] =
{
	{
		{"seq_page_cost", PGC_USERSET, QUERY_TUNING_COST,
			gettext_noop("Sets the planner's estimate of the cost of a "
						 "sequentially fetched disk page."),
			NULL
		},
		&seq_page_cost,
		DEFAULT_SEQ_PAGE_COST, 0, DBL_MAX, NULL, NULL
	},
	{
		{"random_page_cost", PGC_USERSET, QUERY_TUNING_COST,
			gettext_noop("Sets the planner's estimate of the cost of a "
						 "nonsequentially fetched disk page."),
			NULL
		},
		&random_page_cost,
		DEFAULT_RANDOM_PAGE_COST, 0, DBL_MAX, NULL, NULL
	},
	{
		{"cpu_tuple_cost", PGC_USERSET, QUERY_TUNING_COST,
			gettext_noop("Sets the planner's estimate of the cost of "
						 "processing each tuple (row)."),
			NULL
		},
		&cpu_tuple_cost,
		DEFAULT_CPU_TUPLE_COST, 0, DBL_MAX, NULL, NULL
	},
	{
		{"cpu_index_tuple_cost", PGC_USERSET, QUERY_TUNING_COST,
			gettext_noop("Sets the planner's estimate of the cost of "
						 "processing each index entry during an index scan."),
			NULL
		},
		&cpu_index_tuple_cost,
		DEFAULT_CPU_INDEX_TUPLE_COST, 0, DBL_MAX, NULL, NULL
	},
	{
		{"cpu_operator_cost", PGC_USERSET, QUERY_TUNING_COST,
			gettext_noop("Sets the planner's estimate of the cost of "
						 "processing each operator or function call."),
			NULL
		},
		&cpu_operator_cost,
		DEFAULT_CPU_OPERATOR_COST, 0, DBL_MAX, NULL, NULL
	},

	{
		{"cursor_tuple_fraction", PGC_USERSET, QUERY_TUNING_OTHER,
			gettext_noop("Sets the planner's estimate of the fraction of "
						 "a cursor's rows that will be retrieved."),
			NULL
		},
		&cursor_tuple_fraction,
		DEFAULT_CURSOR_TUPLE_FRACTION, 0.0, 1.0, NULL, NULL
	},
	{
		{"geqo_selection_bias", PGC_USERSET, DEFUNCT_OPTIONS,
			gettext_noop("Unused. Syntax check only for PostgreSQL compatibility."),
            NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&defunct_double,
		1.0, 0.0, 100.0, NULL, NULL
	},
	{
		{"bgwriter_lru_percent", PGC_SIGHUP, RESOURCES,
			gettext_noop("Background writer percentage of LRU buffers to flush per round"),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&bgwriter_lru_percent,
		1.0, 0.0, 100.0, NULL, NULL
	},

	{
		{"bgwriter_all_percent", PGC_SIGHUP, RESOURCES,
			gettext_noop("Background writer percentage of all buffers to flush per round"),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&bgwriter_all_percent,
		0.333, 0.0, 100.0, NULL, NULL
	},

	{
		{"seed", PGC_USERSET, UNGROUPED,
			gettext_noop("Sets the seed for random-number generation."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NO_RESET_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&phony_random_seed,
		0.5, 0.0, 1.0, assign_random_seed, show_random_seed
	},

	{
		{"autovacuum_vacuum_scale_factor", PGC_SIGHUP, DEFUNCT_OPTIONS,
			gettext_noop("Number of tuple updates or deletes prior to vacuum as a fraction of reltuples."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&autovacuum_vac_scale,
		0.2, 0.0, 100.0, NULL, NULL
	},
	{
		{"autovacuum_analyze_scale_factor", PGC_SIGHUP, DEFUNCT_OPTIONS,
			gettext_noop("Number of tuple inserts, updates or deletes prior to analyze as a fraction of reltuples."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&autovacuum_anl_scale,
		0.1, 0.0, 100.0, NULL, NULL
	},

	{
		{"gp_process_memory_cutoff", PGC_USERSET, RESOURCES_MEM,
			gettext_noop("Virtual memory limit per process, in kilobytes."),
			gettext_noop("0 for no limit.  Process is terminated if limit is exceeded."),
			GUC_UNIT_KB | GUC_NO_SHOW_ALL
		},
		&gp_process_memory_cutoff,
		0, 0, SIZE_MAX/2/1024, NULL, NULL,
	},

	{
		{"gp_workfile_limit_per_segment", PGC_POSTMASTER, RESOURCES,
			gettext_noop("Maximum disk space (in KB) used for workfiles per segment."),
			gettext_noop("0 for no limit. Current query is terminated when limit is exceeded."),
			GUC_UNIT_KB
		},
		&gp_workfile_limit_per_segment,
		0, 0, SIZE_MAX/1024, NULL, NULL,
	},

	{
		{"gp_workfile_limit_per_query", PGC_USERSET, RESOURCES,
			gettext_noop("Maximum disk space (in KB) used for workfiles per query per segment."),
			gettext_noop("0 for no limit. Current query is terminated when limit is exceeded."),
			GUC_GPDB_ADDOPT | GUC_UNIT_KB
		},
		&gp_workfile_limit_per_query,
		0, 0, SIZE_MAX/1024, NULL, NULL,
	},

	{
		{"gp_motion_cost_per_row", PGC_USERSET, QUERY_TUNING_COST,
			gettext_noop("Sets the planner's estimate of the cost of "
						 "moving a row between worker processes."),
            gettext_noop("If >0, the planner uses this value -- instead of double the "
                         "cpu_tuple_cost -- for Motion operator cost estimation.")
		},
		&gp_motion_cost_per_row,
		0, 0, DBL_MAX, NULL, NULL
	},

	{
		{"gp_hashagg_rewrite_limit", PGC_USERSET, QUERY_TUNING_OTHER,
            gettext_noop("(Obsolete) Planner will not choose hashed aggregation if "
                         "expected number of extra passes exceeds limit."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&defunct_double,
		2.0, 1.0, 100.0, NULL, NULL
	},

	{
		{"gp_analyze_relative_error", PGC_USERSET, STATS_ANALYZE,
		 gettext_noop("target relative error fraction for row sampling during analyze"),
		 NULL
		},
		&analyze_relative_error,
		0.25, 0, 1.0, NULL, NULL
	},

	{
		{"gp_selectivity_damping_factor", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Factor used in selectivity damping."),
			gettext_noop("Values 1..N, 1 = basic damping, greater values emphasize damping"),
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
  		},
		&gp_selectivity_damping_factor,
		1.0, 1.0, DBL_MAX, NULL, NULL
	},

	{
		{"gp_statistics_ndistinct_scaling_ratio_threshold", PGC_USERSET, STATS_ANALYZE,
			gettext_noop("If the ratio of number of distinct values of an attribute to the number of rows is greater than this value, it is assumed that ndistinct will scale with table size."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
  		},
		&gp_statistics_ndistinct_scaling_ratio_threshold,
		0.10, 0.001, 1.0, NULL, NULL
	},
	{
		{"gp_statistics_sampling_threshold", PGC_USERSET, STATS_ANALYZE,
			gettext_noop("Only tables larger than this size will be sampled."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
  		},
		&gp_statistics_sampling_threshold,
		20000.0, 0.0, DBL_MAX, NULL, NULL
	},
	{
		{"gp_resqueue_priority_cpucores_per_segment", PGC_POSTMASTER, RESOURCES_MGM,
			gettext_noop("Number of processing units associated with a segment."),
			NULL
		},
		&gp_resqueue_priority_cpucores_per_segment,
		4.0, 0.1, 512.0, NULL, NULL
	},

	{
		{"gp_simex_rand", PGC_USERSET, GP_ERROR_HANDLING,
			gettext_noop("Propability of injecting an Exceptional Situation in SimEx."),
			gettext_noop("Controls randomized ES simulation."),
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_GPDB_ADDOPT
		},
		&gp_simex_rand,
		100.0, 0.001, 100.0, NULL, NULL
	},

	{
		{"optimizer_damping_factor_filter", PGC_USERSET, QUERY_TUNING_METHOD,
		 gettext_noop("select predicate damping factor in optimizer, 1.0 means no damping"),
		 NULL,
		 GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_damping_factor_filter,
		0.75, 0.0, 1.0, NULL, NULL
	},

	{
		{"optimizer_damping_factor_join", PGC_USERSET, QUERY_TUNING_METHOD,
		 gettext_noop("join predicate damping factor in optimizer, 1.0 means no damping"),
		 NULL,
		 GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_damping_factor_join,
		0.01, 0.0, 1.0, NULL, NULL
	},
	{
		{"optimizer_damping_factor_groupby", PGC_USERSET, QUERY_TUNING_METHOD,
		 gettext_noop("groupby operator damping factor in optimizer, 1.0 means no damping"),
		 NULL,
		 GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_damping_factor_groupby,
		0.75, 0.0, 1.0, NULL, NULL
	},

	{
		{"optimizer_cost_threshold",PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("set the threshold for plan smapling relative to the cost of best plan, 0.0 means unbounded"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_cost_threshold,
		0.0, 0.0, INT_MAX, NULL, NULL
	},

	{
		{"optimizer_nestloop_factor",PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("set the nestloop join cost factor in the optimizer"),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_nestloop_factor,
		1024.0, 1.0, DBL_MAX, NULL, NULL
	},

/* End-of-list marker */
	{
		{NULL, 0, 0, NULL, NULL}, NULL, 0.0, 0.0, 0.0, NULL, NULL
	}
};


static struct config_string ConfigureNamesString[] =
{
	{
		{"allow_system_table_mods", PGC_USERSET, CUSTOM_OPTIONS,
			gettext_noop("Allows ddl/dml modifications of system tables."),
			gettext_noop("Valid values are \"NONE\", \"DDL\", \"DML\", \"ALL\". " ),
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&allow_system_table_mods_str,
		"none", assign_allow_system_table_mods, show_allow_system_table_mods
	},

	{
		{"gp_hashagg_compress_spill_files", PGC_USERSET, DEPRECATED_OPTIONS,
			gettext_noop("Specify if spill files in HashAggregate should be compressed."),
			gettext_noop("Valid values are \"NONE\"(or \"NOTHING\"), \"ZLIB\"."),
			GUC_GPDB_ADDOPT | GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&gp_hashagg_compress_spill_files_str,
		"none", assign_hashagg_compress_spill_files, NULL
	},

	{
		{"gp_workfile_compress_algorithm", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Specify the compression algorithm that work files in the query executor use."),
			gettext_noop("Valid values are \"NONE\", \"ZLIB\"."),
			GUC_GPDB_ADDOPT
		},
		&gp_workfile_compress_algorithm_str,
		"none", assign_gp_workfile_compress_algorithm, NULL
	},

	{
		{"gp_workfile_type_hashjoin", PGC_USERSET, QUERY_TUNING_OTHER,
			gettext_noop("Specify the type of work files to use for executing hash join plans."),
			gettext_noop("Valid values are \"BFZ\", \"BUFFILE\"."),
			GUC_GPDB_ADDOPT | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_workfile_type_hashjoin_str,
		"bfz", assign_gp_workfile_type_hashjoin, NULL
	},

	{
		{"gpperfmon_log_alert_level", PGC_USERSET, LOGGING,
			gettext_noop("Specify the log alert level used by gpperfmon."),
			gettext_noop("Valid values are 'none', 'warning', 'error', 'fatal', 'panic'.")
		},
		&gpperfmon_log_alert_level_str,
		"none", gpvars_assign_gp_gpperfmon_log_alert_level, gpvars_show_gp_gpperfmon_log_alert_level
	},

	{
		{"archive_command", PGC_SIGHUP, WAL_SETTINGS,
			gettext_noop("WAL archiving command."),
			gettext_noop("The shell command that will be called to archive a WAL file."),
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL
		},
		&XLogArchiveCommand,
		"", NULL, NULL
	},

	{
		{"backslash_quote", PGC_USERSET, COMPAT_OPTIONS_PREVIOUS,
			gettext_noop("Sets whether \"\\'\" is allowed in string literals."),
			gettext_noop("Valid values are ON, OFF, and SAFE_ENCODING.")
		},
		&backslash_quote_string,
		"safe_encoding", assign_backslash_quote, NULL
	},

	{
		{"client_encoding", PGC_USERSET, CLIENT_CONN_LOCALE,
			gettext_noop("Sets the client's character set encoding."),
			NULL,
			GUC_IS_NAME | GUC_REPORT
		},
		&client_encoding_string,
		"SQL_ASCII", assign_client_encoding, NULL
	},

	{
		{"client_min_messages", PGC_USERSET, LOGGING_WHEN,
			gettext_noop("Sets the message levels that are sent to the client."),
			gettext_noop("Valid values are DEBUG5, DEBUG4, DEBUG3, DEBUG2, "
						 "DEBUG1, LOG, NOTICE, WARNING, and ERROR. Each level includes all the "
						 "levels that follow it. The later the level, the fewer messages are "
						 "sent."),
			GUC_GPDB_ADDOPT
		},
		&client_min_messages_str,
		"notice", assign_client_min_messages, NULL
	},

	{
		{"explain_memory_verbosity", PGC_USERSET, RESOURCES_MEM,
			gettext_noop("Experimental feature: show memory account usage in EXPLAIN ANALYZE."),
			gettext_noop("Valid values are SUPPRESS, SUMMARY, and DETAIL."),
			GUC_GPDB_ADDOPT
		},
		&explain_memory_verbosity_str,
		"suppress", assign_explain_memory_verbosity, NULL
	},

	{
		{"memory_profiler_run_id", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Set the unique run ID for memory profiling"),
			gettext_noop("Any string is acceptable"),
			GUC_GPDB_ADDOPT | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&memory_profiler_run_id,
		"none", NULL, NULL
	},

	{
		{"memory_profiler_dataset_id", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Set the dataset ID for memory profiling"),
			gettext_noop("Any string is acceptable"),
			GUC_GPDB_ADDOPT | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&memory_profiler_dataset_id,
		"none", NULL, NULL
	},

	{
		{"memory_profiler_query_id", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Set the query ID for memory profiling"),
			gettext_noop("Any string is acceptable"),
			GUC_GPDB_ADDOPT | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&memory_profiler_query_id,
		"none", NULL, NULL
	},

	{
		{"optimizer_log_failure", PGC_USERSET, LOGGING_WHEN,
			gettext_noop("Sets which optimizer failures are logged."),
			gettext_noop("Valid values are unexpected, expected, all"),
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_log_failure_str,
		"all", assign_optimizer_log_failure, NULL
	},

	{
		{"optimizer_minidump", PGC_USERSET, LOGGING_WHEN,
			gettext_noop("Generate optimizer minidump."),
			gettext_noop("Valid values are onerror, always"),
		},
		&optimizer_minidump_str,
		"onerror", assign_optimizer_minidump, NULL
	},

        {
                {"optimizer_cost_model", PGC_USERSET, LOGGING_WHEN,
                        gettext_noop("Set optimizer cost model."),
                        gettext_noop("Valid values are legacy, calibrated"),
                        GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
                },
                &optimizer_cost_model_str,
                "calibrated", assign_optimizer_cost_model, NULL
        },

	{
		{"log_min_messages", PGC_SUSET, LOGGING_WHEN,
			gettext_noop("Sets the message levels that are logged."),
			gettext_noop("Valid values are DEBUG5, DEBUG4, DEBUG3, DEBUG2, DEBUG1, "
			"INFO, NOTICE, WARNING, ERROR, LOG, FATAL, and PANIC. Each level "
						 "includes all the levels that follow it."),
			GUC_GPDB_ADDOPT
		},
		&log_min_messages_str,
		"warning", assign_log_min_messages, NULL
	},
	{
		{"gp_workfile_caching_loglevel", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Sets the logging level for workfile caching debugging messages"),
			gettext_noop("Valid values are DEBUG5, DEBUG4, DEBUG3, DEBUG2, "
						 "DEBUG1, LOG, NOTICE, WARNING, and ERROR. Each level includes all the "
						 "levels that follow it. The later the level, the fewer messages are "
						 "sent."),
			GUC_GPDB_ADDOPT | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_workfile_caching_loglevel_str,
		"debug1", assign_gp_workfile_caching_loglevel, NULL
	},

	{
		{"gp_sessionstate_loglevel", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Sets the logging level for session state debugging messages"),
			gettext_noop("Valid values are DEBUG5, DEBUG4, DEBUG3, DEBUG2, "
						 "DEBUG1, LOG, NOTICE, WARNING, and ERROR. Each level includes all the "
						 "levels that follow it. The later the level, the fewer messages are "
						 "sent."),
			GUC_GPDB_ADDOPT | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_sessionstate_loglevel_str,
		"debug1", assign_gp_sessionstate_loglevel, NULL
	},

	{
		{"debug_persistent_print_level", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Sets the persistent relation debug message levels that are logged."),
			gettext_noop("Valid values are DEBUG5, DEBUG4, DEBUG3, DEBUG2, DEBUG1, "
			"INFO, NOTICE, WARNING, ERROR, LOG, FATAL, and PANIC. Each level "
						 "includes all the levels that follow it."),
			GUC_GPDB_ADDOPT | GUC_NO_SHOW_ALL
		},
		&Debug_persistent_print_level_str,
		"debug1", assign_debug_persistent_print_level, NULL
	},

	{
		{"debug_persistent_recovery_print_level", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Sets the persistent recovery debug message levels that are logged."),
			gettext_noop("Valid values are DEBUG5, DEBUG4, DEBUG3, DEBUG2, DEBUG1, "
			"INFO, NOTICE, WARNING, ERROR, LOG, FATAL, and PANIC. Each level "
						 "includes all the levels that follow it."),
			GUC_GPDB_ADDOPT | GUC_NO_SHOW_ALL
		},
		&Debug_persistent_recovery_print_level_str,
		"debug1", assign_debug_persistent_recovery_print_level, NULL
	},

	{
		{"debug_persistent_store_print_level", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Sets the persistent relation store debug message levels that are logged."),
			gettext_noop("Valid values are DEBUG5, DEBUG4, DEBUG3, DEBUG2, DEBUG1, "
			"INFO, NOTICE, WARNING, ERROR, LOG, FATAL, and PANIC. Each level "
						 "includes all the levels that follow it."),
			GUC_GPDB_ADDOPT | GUC_NO_SHOW_ALL
		},
		&Debug_persistent_store_print_level_str,
		"debug1", assign_debug_persistent_store_print_level, NULL
	},

	{
		{"debug_database_command_error_level", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Sets the database command debug message levels that are logged."),
			gettext_noop("Valid values are DEBUG5, DEBUG4, DEBUG3, DEBUG2, DEBUG1, "
			"INFO, NOTICE, WARNING, ERROR, LOG, FATAL, and PANIC. Each level "
						 "includes all the levels that follow it."),
			GUC_GPDB_ADDOPT | GUC_NO_SHOW_ALL
		},
		&Debug_database_command_print_level_str,
		"log", assign_debug_database_command_print_level, NULL
	},

 	{
		{"IntervalStyle", PGC_USERSET, CLIENT_CONN_LOCALE,
			gettext_noop("Sets the display format for interval values."),
			NULL,
			GUC_REPORT
		},
		&IntervalStyle_string,
		"postgres", assign_IntervalStyle, show_IntervalStyle
	},

	{
		{"log_error_verbosity", PGC_SUSET, LOGGING_WHEN,
			gettext_noop("Sets the verbosity of logged messages."),
			gettext_noop("Valid values are \"terse\", \"default\", and \"verbose\"."),
			GUC_GPDB_ADDOPT
		},
		&log_error_verbosity_str,
		"default", assign_log_error_verbosity, NULL
	},
	{
		{"log_statement", PGC_SUSET, LOGGING_WHAT,
			gettext_noop("Sets the type of statements logged."),
			gettext_noop("Valid values are \"none\", \"ddl\", \"mod\", and \"all\".")
		},
		&log_statement_str,
		"none", assign_log_statement, NULL
	},

	{
		{"log_min_error_statement", PGC_SUSET, LOGGING_WHEN,
			gettext_noop("Causes all statements generating error at or above this level to be logged."),
			gettext_noop("All SQL statements that cause an error of the "
						 "specified level or a higher level are logged."),
			GUC_GPDB_ADDOPT
		},
		&log_min_error_statement_str,
		"error", assign_min_error_statement, NULL
	},

	{
		{"log_line_prefix", PGC_SIGHUP, DEFUNCT_OPTIONS,
			gettext_noop("Defunct: Controls information prefixed to each log line"),
		 	gettext_noop("if blank no prefix is used"),
		 	GUC_NO_SHOW_ALL
		},
		&Log_line_prefix,
		"%m|%u|%d|%p|%I|%X|:-", NULL, NULL
	},

	{
		{"log_timezone", PGC_SIGHUP, LOGGING_WHAT,
			gettext_noop("Sets the time zone to use in log messages."),
			NULL
		},
		&log_timezone_string,
		"UNKNOWN", assign_log_timezone, show_log_timezone
	},

	{
		{"DateStyle", PGC_USERSET, CLIENT_CONN_LOCALE,
			gettext_noop("Sets the display format for date and time values."),
			gettext_noop("Also controls interpretation of ambiguous "
						 "date inputs."),
			GUC_LIST_INPUT | GUC_REPORT | GUC_GPDB_ADDOPT
		},
		&datestyle_string,
		"ISO, MDY", assign_datestyle, NULL
	},

	{
		{"default_tablespace", PGC_USERSET, CLIENT_CONN_STATEMENT,
			gettext_noop("Sets the default tablespace to create tables and indexes in."),
			gettext_noop("An empty string selects the database's default tablespace."),
			GUC_IS_NAME
		},
		&default_tablespace,
		"", assign_default_tablespace, NULL
	},

	{
		{"default_transaction_isolation", PGC_USERSET, CLIENT_CONN_STATEMENT,
			gettext_noop("Sets the transaction isolation level of each new transaction."),
			gettext_noop("Each SQL transaction has an isolation level, which "
						 "can be either \"read uncommitted\", \"read committed\", \"repeatable read\", or \"serializable\"."),
		},
		&default_iso_level_string,
		"read committed", assign_defaultxactisolevel, NULL
	},

	{
		{"dynamic_library_path", PGC_SUSET, CLIENT_CONN_OTHER,
			gettext_noop("Sets the path for dynamically loadable modules."),
			gettext_noop("If a dynamically loadable module needs to be opened and "
						 "the specified name does not have a directory component (i.e., the "
						 "name does not contain a slash), the system will search this path for "
						 "the specified file."),
			GUC_SUPERUSER_ONLY
		},
		&Dynamic_library_path,
		"$libdir", NULL, NULL
	},

	{
		{"krb_server_keyfile", PGC_SIGHUP, CONN_AUTH_SECURITY,
			gettext_noop("Sets the location of the Kerberos server key file."),
			NULL,
			GUC_SUPERUSER_ONLY
		},
		&pg_krb_server_keyfile,
		PG_KRB_SRVTAB, NULL, NULL
	},

	{
		{"krb_srvname", PGC_SIGHUP, CONN_AUTH_SECURITY,
			gettext_noop("Sets the name of the Kerberos service."),
			NULL
		},
		&pg_krb_srvnam,
		PG_KRB_SRVNAM, NULL, NULL
	},

	{
		{"bonjour_name", PGC_POSTMASTER, CONN_AUTH_SETTINGS,
			gettext_noop("Sets the Bonjour broadcast service name."),
			NULL
		},
		&bonjour_name,
		"", NULL, NULL
	},

	/* See main.c about why defaults for LC_foo are not all alike */

	{
		{"lc_collate", PGC_INTERNAL, CLIENT_CONN_LOCALE,
			gettext_noop("Shows the collation order locale."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&locale_collate,
		"C", NULL, NULL
	},

	{
		{"lc_ctype", PGC_INTERNAL, CLIENT_CONN_LOCALE,
			gettext_noop("Shows the character classification and case conversion locale."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&locale_ctype,
		"C", NULL, NULL
	},

	{
		{"lc_messages", PGC_SUSET, CLIENT_CONN_LOCALE,
			gettext_noop("Sets the language in which messages are displayed."),
			NULL
		},
		&locale_messages,
		"", locale_messages_assign, NULL
	},

	{
		{"lc_monetary", PGC_USERSET, CLIENT_CONN_LOCALE,
			gettext_noop("Sets the locale for formatting monetary amounts."),
			NULL
		},
		&locale_monetary,
		"C", locale_monetary_assign, NULL
	},

	{
		{"lc_numeric", PGC_USERSET, CLIENT_CONN_LOCALE,
			gettext_noop("Sets the locale for formatting numbers."),
			NULL
		},
		&locale_numeric,
		"C", locale_numeric_assign, NULL
	},

	{
		{"lc_time", PGC_USERSET, CLIENT_CONN_LOCALE,
			gettext_noop("Sets the locale for formatting date and time values."),
			NULL
		},
		&locale_time,
		"C", locale_time_assign, NULL
	},

	{
		{"shared_preload_libraries", PGC_POSTMASTER, RESOURCES_KERNEL,
			gettext_noop("Lists shared libraries to preload into server."),
			NULL,
			GUC_LIST_INPUT | GUC_LIST_QUOTE | GUC_SUPERUSER_ONLY
		},
		&shared_preload_libraries_string,
		"", NULL, NULL
	},

	{
		{"local_preload_libraries", PGC_BACKEND, CLIENT_CONN_OTHER,
			gettext_noop("Lists shared libraries to preload into each backend."),
			NULL,
			GUC_LIST_INPUT | GUC_LIST_QUOTE
		},
		&local_preload_libraries_string,
		"", NULL, NULL
	},

	{
		{"regex_flavor", PGC_USERSET, COMPAT_OPTIONS_PREVIOUS,
			gettext_noop("Sets the regular expression \"flavor\"."),
			gettext_noop("This can be set to advanced, extended, or basic.")
		},
		&regex_flavor_string,
		"advanced", assign_regex_flavor, NULL
	},

	{
		{"search_path", PGC_USERSET, CLIENT_CONN_STATEMENT,
			gettext_noop("Sets the schema search order for names that are not schema-qualified."),
			NULL,
			GUC_LIST_INPUT | GUC_LIST_QUOTE | GUC_GPDB_ADDOPT
		},
		&namespace_search_path,
		"\"$user\",public", assign_search_path, NULL
	},

	{
		/* Can't be set in postgresql.conf */
		{"server_encoding", PGC_INTERNAL, CLIENT_CONN_LOCALE,
			gettext_noop("Sets the server (database) character set encoding."),
			NULL,
			GUC_IS_NAME | GUC_REPORT | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&server_encoding_string,
		"SQL_ASCII", NULL, NULL
	},

	{
		/* Can't be set in postgresql.conf */
		{"server_version", PGC_INTERNAL, PRESET_OPTIONS,
			gettext_noop("Shows the server version."),
			NULL,
			GUC_REPORT | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&server_version_string,
		PG_VERSION, NULL, NULL
	},

	{
		/* Not for general use --- used by SET ROLE */
		{"role", PGC_USERSET, UNGROUPED,
			gettext_noop("Sets the current role."),
			NULL,
			GUC_IS_NAME | GUC_NO_SHOW_ALL | GUC_NO_RESET_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE | GUC_NOT_WHILE_SEC_REST
		},
		&role_string,
		"none", assign_role, show_role
	},

	{
		/* Not for general use --- used by SET SESSION AUTHORIZATION */
		{"session_authorization", PGC_USERSET, UNGROUPED,
			gettext_noop("Sets the session user name."),
			NULL,
			GUC_IS_NAME | GUC_REPORT | GUC_NO_SHOW_ALL | GUC_NO_RESET_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE | GUC_NOT_WHILE_SEC_REST
		},
		&session_authorization_string,
		NULL, assign_session_authorization, show_session_authorization
	},

	{
		{"log_destination", PGC_SIGHUP, DEFUNCT_OPTIONS,
			gettext_noop("Defunct: Sets the destination for server log output."),
			gettext_noop("Valid values are combinations of \"stderr\", \"syslog\", "
						 "and \"eventlog\", depending on the platform."),
			GUC_LIST_INPUT | GUC_NO_SHOW_ALL
		},
		&log_destination_string,
		"stderr", assign_log_destination, NULL
	},
	{
		{"log_directory", PGC_SIGHUP, DEFUNCT_OPTIONS,
			gettext_noop("Defunct: Sets the destination directory for log files."),
			gettext_noop("May be specified as relative to the data directory "
						 "or as absolute path."),
			GUC_SUPERUSER_ONLY | GUC_NO_SHOW_ALL
		},
		&Log_directory,
		"pg_log", assign_canonical_path, NULL
	},
	{
		{"log_filename", PGC_SIGHUP, LOGGING_WHERE,
			gettext_noop("Sets the file name pattern for log files."),
			NULL,
			GUC_SUPERUSER_ONLY | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Log_filename,
		"gpdb-%Y-%m-%d_%H%M%S.csv", NULL, NULL
	},

	{
		{"gp_log_format", PGC_POSTMASTER, LOGGING_WHERE,
		 gettext_noop("Sets the format for log files."),
		 gettext_noop("Valid values are TEXT, CSV.")
		},
		&gp_log_format_string,
		"csv", assign_gp_log_format, NULL
	},

#ifdef HAVE_SYSLOG
	{
		{"syslog_facility", PGC_SIGHUP, DEFUNCT_OPTIONS,
			gettext_noop("Sets the syslog \"facility\" to be used when syslog enabled."),
			gettext_noop("Valid values are LOCAL0, LOCAL1, LOCAL2, LOCAL3, "
						 "LOCAL4, LOCAL5, LOCAL6, LOCAL7."),
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&syslog_facility_str,
		"LOCAL0", assign_syslog_facility, NULL
	},
	{
		{"syslog_ident", PGC_SIGHUP, DEFUNCT_OPTIONS,
			gettext_noop("Sets the program name used to identify PostgreSQL "
						 "messages in syslog."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&syslog_ident_str,
		"postgres", assign_syslog_ident, NULL
	},
#endif

	{
		{"TimeZone", PGC_USERSET, CLIENT_CONN_LOCALE,
			gettext_noop("Sets the time zone for displaying and interpreting time stamps."),
			NULL,
			GUC_REPORT | GUC_GPDB_ADDOPT
		},
		&timezone_string,
		"UNKNOWN", assign_timezone, show_timezone
	},
	{
		{"timezone_abbreviations", PGC_USERSET, CLIENT_CONN_LOCALE,
			gettext_noop("Selects a file of time zone abbreviations."),
			NULL
		},
		&timezone_abbreviations_string,
		"UNKNOWN", assign_timezone_abbreviations, NULL
	},

	{
		{"transaction_isolation", PGC_USERSET, CLIENT_CONN_STATEMENT,
			gettext_noop("Sets the current transaction's isolation level."),
			NULL,
			GUC_NO_RESET_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&XactIsoLevel_string,
		NULL, assign_XactIsoLevel, show_XactIsoLevel
	},

	{
		{"unix_socket_group", PGC_POSTMASTER, CONN_AUTH_SETTINGS,
			gettext_noop("Sets the owning group of the Unix-domain socket."),
			gettext_noop("The owning user of the socket is always the user "
						 "that starts the server.")
		},
		&Unix_socket_group,
		"", NULL, NULL
	},

	{
		{"unix_socket_directory", PGC_POSTMASTER, CONN_AUTH_SETTINGS,
			gettext_noop("Sets the directory where the Unix-domain socket will be created."),
			NULL,
			GUC_SUPERUSER_ONLY
		},
		&UnixSocketDir,
		"", assign_canonical_path, NULL
	},

	{
		{"listen_addresses", PGC_POSTMASTER, CONN_AUTH_SETTINGS,
			gettext_noop("Sets the host name or IP address(es) to listen to."),
			NULL,
			GUC_LIST_INPUT
		},
		&ListenAddresses,
		"localhost", NULL, NULL
	},

	{
		{"wal_sync_method", PGC_SIGHUP, WAL_SETTINGS,
			gettext_noop("Selects the method used for forcing WAL updates out to disk."),
			NULL,
			GUC_NOT_IN_SAMPLE | GUC_NO_SHOW_ALL | GUC_DISALLOW_USER_SET
		},
		&XLOG_sync_method,
		XLOG_sync_method_default, assign_xlog_sync_method, NULL
	},

	{
		{"custom_variable_classes", PGC_SIGHUP, CUSTOM_OPTIONS,
			gettext_noop("Sets the list of known custom variable classes."),
			NULL,
			GUC_LIST_INPUT | GUC_LIST_QUOTE
		},
		&custom_variable_classes,
		NULL, assign_custom_variable_classes, NULL
	},

	{
		{"data_directory", PGC_POSTMASTER, FILE_LOCATIONS,
			gettext_noop("Sets the server's data directory."),
			NULL,
			GUC_SUPERUSER_ONLY | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&data_directory,
		NULL, NULL, NULL
	},

	{
		{"config_file", PGC_POSTMASTER, FILE_LOCATIONS,
			gettext_noop("Sets the server's main configuration file."),
			NULL,
			GUC_DISALLOW_IN_FILE | GUC_SUPERUSER_ONLY | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&ConfigFileName,
		NULL, NULL, NULL
	},

	{
		{"hba_file", PGC_POSTMASTER, FILE_LOCATIONS,
			gettext_noop("Sets the server's \"hba\" configuration file."),
			NULL,
			GUC_SUPERUSER_ONLY | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&HbaFileName,
		NULL, NULL, NULL
	},

	{
		{"ident_file", PGC_POSTMASTER, FILE_LOCATIONS,
			gettext_noop("Sets the server's \"ident\" configuration file."),
			NULL,
			GUC_SUPERUSER_ONLY | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&IdentFileName,
		NULL, NULL, NULL
	},

#ifdef USE_SSL
	{
		{"ssl_ciphers", PGC_POSTMASTER, CONN_AUTH_SECURITY,
			gettext_noop("Sets the list of allowed SSL ciphers."),
			NULL,
			GUC_SUPERUSER_ONLY
		},
		&SSLCipherSuites,
		"ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH", NULL, NULL
	},
#endif   /* USE_SSL */

	{
		{"application_name", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Sets the application name to be reported in statistics and logs."),
			NULL,
			GUC_IS_NAME | GUC_REPORT | GUC_NOT_IN_SAMPLE
		},
		&application_name,
		"", assign_application_name, NULL
	},

	{
		{"external_pid_file", PGC_POSTMASTER, FILE_LOCATIONS,
			gettext_noop("Writes the postmaster PID to the specified file."),
			NULL,
			GUC_SUPERUSER_ONLY | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&external_pid_file,
		NULL, assign_canonical_path, NULL
	},


	{
		{"stats_temp_directory", PGC_SIGHUP, STATS_COLLECTOR,
			gettext_noop("Writes temporary statistics files to the specified directory."),
			NULL,
			GUC_SUPERUSER_ONLY | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&pgstat_temp_directory,
		"pg_stat_tmp", assign_pgstat_temp_directory, NULL
	},

	{
		{"gp_session_role", PGC_BACKEND, GP_WORKER_IDENTITY,
			gettext_noop("Reports the default role for the session."),
			gettext_noop("Valid values are DISPATCH, EXECUTE, and UTILITY."),
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&gp_session_role_string,
		"dispatch", assign_gp_session_role, show_gp_session_role
	},

	{
		{"gp_role", PGC_SUSET, CLIENT_CONN_OTHER,
			gettext_noop("Sets the role for the session."),
			gettext_noop("Valid values are DISPATCH, EXECUTE, and UTILITY."),
			GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&gp_role_string,
		"dispatch", assign_gp_role, show_gp_role
	},

    {
       {"gp_fault_action", PGC_POSTMASTER, DEFUNCT_OPTIONS,
		gettext_noop("Sets the fault action for fault tolerance management."),
		gettext_noop("Valid values are CONTINUE, READONLY, and SHUTDOWN."),
		GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
       },
       &gp_fault_action_string,
       "continue", NULL, NULL
    },

	{
		{"gp_log_gang", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Sets the verbosity of logged messages pertaining to worker process creation and management."),
			gettext_noop("Valid values are \"off\", \"terse\", \"verbose\" and \"debug\"."),
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_log_gang_str,
		"terse", gpvars_assign_gp_log_gang, gpvars_show_gp_log_gang
	},

	{
		{"gp_log_fts", PGC_SIGHUP, LOGGING_WHAT,
			gettext_noop("Sets the verbosity of logged messages pertaining to fault probing."),
			gettext_noop("Valid values are \"off\", \"terse\", \"verbose\" and \"debug\"."),
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_log_fts_str,
		"terse", gpvars_assign_gp_log_fts, gpvars_show_gp_log_fts
	},

	{
		{"gp_log_interconnect", PGC_USERSET, LOGGING_WHAT,
			gettext_noop("Sets the verbosity of logged messages pertaining to connections between worker processes."),
			gettext_noop("Valid values are \"off\", \"terse\", \"verbose\" and \"debug\"."),
			GUC_GPDB_ADDOPT | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_log_interconnect_str,
		"terse", gpvars_assign_gp_log_interconnect, gpvars_show_gp_log_interconnect
	},

	{
		{"gp_interconnect_type", PGC_BACKEND, GP_ARRAY_TUNING,
		 gettext_noop("Sets the protocol used for inter-node communication."),
		 gettext_noop("Valid values are \"tcp\", \"udp\" and \"udpifc\"."),
		 GUC_GPDB_ADDOPT
		},
		&gp_interconnect_type_str,
		"udpifc", gpvars_assign_gp_interconnect_type, gpvars_show_gp_interconnect_type
	},

	{
		{"gp_interconnect_fc_method", PGC_USERSET, GP_ARRAY_TUNING,
		 gettext_noop("Sets the flow control method used for UDP interconnect."),
		 gettext_noop("Valid values are \"capacity\" and \"loss\"."),
		 GUC_GPDB_ADDOPT
		},
		&gp_interconnect_fc_method_str,
		"loss", gpvars_assign_gp_interconnect_fc_method, gpvars_show_gp_interconnect_fc_method
	},

	{
		{"gp_qd_callback_info", PGC_BACKEND, GP_WORKER_IDENTITY,
			gettext_noop("Sets the QD callback information for the QEs in this session."),
			gettext_noop("Right now, this is the port information"),
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&gp_qd_callback_info,
		"", NULL, NULL
	},

	{
		{"gp_qd_hostname", PGC_BACKEND, GP_WORKER_IDENTITY,
			gettext_noop("Shows the QD Hostname. Blank when run on the QD"),
		    NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_DISALLOW_IN_FILE
		},
		&qdHostname,
		"", NULL, NULL
	},

	{
		{"debug_dtm_action", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Sets the debug DTM action."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_dtm_action_str,
		"none", assign_debug_dtm_action, NULL
	},

	{
		{"debug_dtm_action_target", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Sets the debug DTM action target."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_dtm_action_target_str,
		"none", assign_debug_dtm_action_target, NULL
	},

	{
		{"debug_dtm_action_protocol", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Sets the debug DTM action protocol."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_dtm_action_protocol_str,
		"none", assign_debug_dtm_action_protocol, NULL
	},

	{
		{"debug_dtm_action_sql_command_tag", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Sets the debug DTM action sql command tag."),
			NULL,
			GUC_SUPERUSER_ONLY |  GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&Debug_dtm_action_sql_command_tag,
		"", NULL, NULL
	},
	{
		{"gp_autostats_mode", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Sets the autostats mode."),
			gettext_noop("Valid values are NONE, ON_CHANGE, ON_NO_STATS. ON_CHANGE requires setting gp_autostats_on_change_threshold.")
		},
		&gp_autostats_mode_string,
		"none", gpvars_assign_gp_autostats_mode, gpvars_show_gp_autostats_mode
	},
	{
		{"gp_autostats_mode_in_functions", PGC_USERSET, DEVELOPER_OPTIONS,
			gettext_noop("Sets the autostats mode for statements in procedural language functions."),
			gettext_noop("Valid values are NONE, ON_CHANGE, ON_NO_STATS. ON_CHANGE requires setting gp_autostats_on_change_threshold.")
		},
		&gp_autostats_mode_in_functions_string,
		"none", gpvars_assign_gp_autostats_mode_in_functions, gpvars_show_gp_autostats_mode_in_functions
	},
	{
		{"gp_resqueue_priority_default_value", PGC_POSTMASTER, RESOURCES_MGM,
			gettext_noop("Default weight when one cannot be associated with a statement."),
			NULL,
			GUC_NO_SHOW_ALL
  		},
		&gp_resqueue_priority_default_value,
		"MEDIUM", gpvars_assign_gp_resqueue_priority_default_value, NULL
	},

	{
		{"gp_email_smtp_server", PGC_SUSET, LOGGING,
			gettext_noop("Sets the SMTP server and port used to send e-mail alerts."),
			NULL,
			GUC_SUPERUSER_ONLY
		},
		&gp_email_smtp_server,
		"localhost:25", NULL, NULL
	},
	{
		{"gp_email_smtp_userid", PGC_SUSET, LOGGING,
			gettext_noop("Sets the userid used for the SMTP server, if required."),
			NULL,
			GUC_SUPERUSER_ONLY
		},
		&gp_email_smtp_userid,
		"", NULL, NULL
	},
	{
		{"gp_email_smtp_password", PGC_SUSET, LOGGING,
			gettext_noop("Sets the password used for the SMTP server, if required."),
			NULL,
			GUC_SUPERUSER_ONLY
		},
		&gp_email_smtp_password,
		"", NULL, NULL
	},
	{
		{"gp_email_from", PGC_SUSET, LOGGING,
			gettext_noop("Sets email address of the sender of email alerts (our e-mail id)."),
			NULL,
			GUC_SUPERUSER_ONLY
		},
		&gp_email_from,
		"", NULL, NULL
	},
	{
		{"gp_email_to", PGC_SUSET, LOGGING,
			gettext_noop("Sets email address(es) to send alerts to.  Maybe be multiple email addresses."),
			NULL,
			GUC_SUPERUSER_ONLY| GUC_LIST_INPUT
		},
		&gp_email_to,
		"", NULL, NULL
	},

#if USE_SNMP
	{
		{"gp_snmp_community", PGC_SUSET, LOGGING,
			gettext_noop("Sets SNMP community name to send alerts (inform or trap messages) to."),
			NULL,
			GUC_SUPERUSER_ONLY| GUC_LIST_INPUT
		},
		&gp_snmp_community,
		"public", NULL, NULL
	},
	{
		{"gp_snmp_monitor_address", PGC_SUSET, LOGGING,
			gettext_noop("Sets the network address to send SNMP alerts (inform or trap messages) to."),
			NULL,
			GUC_SUPERUSER_ONLY| GUC_LIST_INPUT
		},
		&gp_snmp_monitor_address,
		"", NULL, NULL
	},
	{
		{"gp_snmp_use_inform_or_trap", PGC_SUSET, LOGGING,
			gettext_noop("If 'inform', we send alerts as SNMP v2c inform messages, if 'trap', we use SNMP v2 trap messages.."),
			NULL,
			GUC_SUPERUSER_ONLY| GUC_LIST_INPUT
		},
		&gp_snmp_use_inform_or_trap,
		"trap", NULL, NULL
	},
	{
		{"gp_snmp_debug_log", PGC_SUSET, DEVELOPER_OPTIONS,
			gettext_noop("Logs snmp activity to this file for debugging purposes."),
			NULL,
			GUC_SUPERUSER_ONLY| GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_snmp_debug_log,
		"", NULL, NULL
	},

#endif

#ifdef USE_CONNECTEMC
	{
		{"gp_connectemc_mode", PGC_POSTMASTER, LOGGING,
			gettext_noop("control connectemc functionality"),
			gettext_noop("If 'on' send connectemc messages and log them locally, if 'local' log connectemc message locally only, if 'remote' send connectemc messages only, if 'off', no connectemc messages"),
			GUC_SUPERUSER_ONLY
		},
		&gp_connectemc_mode,
		"on", assign_connectemc_mode, NULL
	},
#endif

	/* for pljava */
	{
		{"pljava_vmoptions", PGC_SUSET, CUSTOM_OPTIONS,
			gettext_noop("Options sent to the JVM when it is created"),
			NULL,
		 GUC_GPDB_ADDOPT | GUC_NOT_IN_SAMPLE | GUC_SUPERUSER_ONLY
		},
		&pljava_vmoptions,
		"", NULL, NULL
	},
	{
		{"pljava_classpath", PGC_USERSET, CUSTOM_OPTIONS,
			gettext_noop("classpath used by the the JVM"),
			NULL,
		 GUC_GPDB_ADDOPT
		},
		&pljava_classpath,
		"", NULL, NULL
	},

	{
		{"gp_resqueue_memory_policy", PGC_SUSET, RESOURCES_MGM,
			gettext_noop("Sets the policy for memory allocation of queries."),
			gettext_noop("Valid values are NONE, AUTO, EAGER_FREE.")
		},
		&gp_resqueue_memory_policy_str,
		"none", gpvars_assign_gp_resqueue_memory_policy, gpvars_show_gp_resqueue_memory_policy
	},

	{
		{"gp_test_time_slice_report_level", PGC_USERSET, LOGGING_WHEN,
			gettext_noop("Sets the message level for time slice violation reports."),
			gettext_noop("Valid values are NOTICE, WARNING, ERROR, FATAL and PANIC."),
		 GUC_GPDB_ADDOPT | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_test_time_slice_report_level_str,
		"error", assign_time_slice_report_level, NULL
	},

	{
		{"gp_test_deadlock_hazard_report_level", PGC_USERSET, LOGGING_WHEN,
			gettext_noop("Sets the message level for deadlock hazard reports."),
			gettext_noop("Valid values are NOTICE, WARNING, ERROR, FATAL and PANIC."),
		 GUC_GPDB_ADDOPT | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_test_deadlock_hazard_report_level_str,
		"error", assign_deadlock_hazard_report_level, NULL
	},

	{
		{"gp_test_system_cache_flush_force", PGC_USERSET, GP_ERROR_HANDLING,
			gettext_noop("Force invalidation of system caches on each access"),
			gettext_noop("Valid values are OFF, PLAIN and RECURSIVE."),
		 GUC_GPDB_ADDOPT | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_test_system_cache_flush_force_str,
		"off", assign_system_cache_flush_force, NULL
	},

	{
		{"gp_hadoop_connector_jardir", PGC_USERSET, CUSTOM_OPTIONS,
			gettext_noop("The directory of the Hadoop connector JAR, relative to $GPHOME."),
			NULL,
			GUC_GPDB_ADDOPT | GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_hadoop_connector_jardir,
		"lib//hadoop", NULL, NULL
	},

	{
		{"gp_hadoop_target_version", PGC_USERSET, CUSTOM_OPTIONS,
			gettext_noop("The distro/version of Hadoop that external table is connecting to."),
			gettext_noop("See release notes or gppkg readme for details."),
			GUC_GPDB_ADDOPT
		},
		&gp_hadoop_target_version,
		"gphd-1.1", NULL, NULL
	},

	{
		{"gp_hadoop_home", PGC_USERSET, CUSTOM_OPTIONS,
			gettext_noop("The location where Hadoop is installed in each segment."),
			NULL,
			GUC_GPDB_ADDOPT
		},
		&gp_hadoop_home,
		"", NULL, NULL
	},

	{
		{"gp_auth_time_override", PGC_SIGHUP, DEVELOPER_OPTIONS,
			gettext_noop("The timestamp used for enforcing time constraints."),
			gettext_noop("For testing purposes only."),
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&gp_auth_time_override_str,
		"", NULL, NULL
	},

	{
		{"password_hash_algorithm", PGC_SUSET, CONN_AUTH_SECURITY,
			gettext_noop("The cryptograph hash algorithm to apply to passwords before storing them."),
			gettext_noop("Valid values are MD5 or SHA-256."),
			GUC_SUPERUSER_ONLY
		},
		&password_hash_algorithm_str,
		"MD5", assign_password_hash_algorithm, NULL
	},

	{
		{"gp_idf_deduplicate", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Sets the mode to control inverse distribution function's de-duplicate strategy."),
			gettext_noop("Valid values are AUTO, NONE, and FORCE.")
		},
		&gp_idf_deduplicate_str,
		"auto", assign_gp_idf_deduplicate, NULL
	},

	{
		{"optimizer_search_strategy_path", PGC_USERSET, QUERY_TUNING_METHOD,
			gettext_noop("Sets the search strategy used by gp optimizer."),
			NULL,
			GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE
		},
		&optimizer_search_strategy_path,
		"default", NULL, NULL
	},

	{
		{"gp_default_storage_options", PGC_USERSET, APPENDONLY_TABLES,
			gettext_noop("Default options for appendonly storage."),
			NULL,
			GUC_NOT_IN_SAMPLE
		},
		&gp_default_storage_options, "", assign_gp_default_storage_options, NULL
	},

	/* End-of-list marker */
	{
		{NULL, 0, 0, NULL, NULL}, NULL, NULL, NULL, NULL
	}
};


/******** end of options list ********/


/*
 * To allow continued support of obsolete names for GUC variables, we apply
 * the following mappings to any unrecognized name.  Note that an old name
 * should be mapped to a new one only if the new variable has very similar
 * semantics to the old.
 */
static const char *const map_old_guc_names[] = {
	"sort_mem", "work_mem",
	"vacuum_mem", "maintenance_work_mem",
	NULL
};


/*
 * Actual lookup of variables is done through this single, sorted array.
 */
static struct config_generic **guc_variables;

/* Current number of variables contained in the vector */
static int	num_guc_variables;

/* Vector capacity */
static int	size_guc_variables;


static bool guc_dirty;			/* TRUE if need to do commit/abort work */

static bool guc_cannotprepare;                        /* TRUE if this transaction cannot be prepared for 2PC */

static bool reporting_enabled;	/* TRUE to enable GUC_REPORT */

static int	GUCNestLevel = 0;	/* 1 when in main transaction */


static void gp_guc_list_init(void);
static int	guc_var_compare(const void *a, const void *b);
static int	guc_name_compare(const char *namea, const char *nameb);
static void push_old_value(struct config_generic * gconf);
static void ReportGUCOption(struct config_generic * record);
static void ShowGUCConfigOption(const char *name, DestReceiver *dest);
static void ShowAllGUCConfig(DestReceiver *dest);
static char *_ShowOption(struct config_generic * record, bool use_units);
static bool is_newvalue_equal(struct config_generic * record, const char *newvalue);


/*
 * Some infrastructure for checking malloc/strdup/realloc calls
 */
static void *
guc_malloc(int elevel, size_t size)
{
	void	   *data;

	data = malloc(size);
	if (data == NULL)
		ereport(elevel,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("out of memory")));
	return data;
}

static void *
guc_realloc(int elevel, void *old, size_t size)
{
	void	   *data;

	data = realloc(old, size);
	if (data == NULL)
		ereport(elevel,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("out of memory")));
	return data;
}

static char *
guc_strdup(int elevel, const char *src)
{
	char	   *data;

	data = strdup(src);
	if (data == NULL)
		ereport(elevel,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("out of memory")));
	return data;
}


/*
 * Support for assigning to a field of a string GUC item.  Free the prior
 * value if it's not referenced anywhere else in the item (including stacked
 * states).
 */
static void
set_string_field(struct config_string * conf, char **field, char *newval)
{
	char	   *oldval = *field;
	GucStack   *stack;

	/* Do the assignment */
	*field = newval;

	/* Exit if any duplicate references, or if old value was NULL anyway */
	if (oldval == NULL ||
		oldval == *(conf->variable) ||
		oldval == conf->reset_val ||
		oldval == conf->tentative_val)
		return;
	for (stack = conf->gen.stack; stack; stack = stack->prev)
	{
		if (oldval == stack->tentative_val.stringval ||
			oldval == stack->value.stringval)
			return;
	}

	/* Not used anymore, so free it */
	free(oldval);
}

/*
 * Detect whether strval is referenced anywhere in a GUC string item
 */
static bool
string_field_used(struct config_string * conf, char *strval)
{
	GucStack   *stack;

	if (strval == *(conf->variable) ||
		strval == conf->reset_val ||
		strval == conf->tentative_val)
		return true;
	for (stack = conf->gen.stack; stack; stack = stack->prev)
	{
		if (strval == stack->tentative_val.stringval ||
			strval == stack->value.stringval)
			return true;
	}
	return false;
}


struct config_generic **
get_guc_variables(void)
{
	return guc_variables;
}
int get_num_guc_variables(void)
{
	return num_guc_variables;
}


/*
 * Build the sorted array.	This is split out so that it could be
 * re-executed after startup (eg, we could allow loadable modules to
 * add vars, and then we'd need to re-sort).
 */
void
build_guc_variables(void)
{
	int			size_vars;
	int			num_vars = 0;
	struct config_generic **guc_vars;
	int			i;

	/* validate that the config_group array has same # of elements as config_group enumeration */
	for ( i = 0; i < ___CONFIG_GROUP_COUNT; i++)
	{
	    Assert(config_group_names[i] != NULL);
	}
	Assert(config_group_names[___CONFIG_GROUP_COUNT] == NULL);

	for (i = 0; ConfigureNamesBool[i].gen.name; i++)
	{
		struct config_bool *conf = &ConfigureNamesBool[i];

		/* Rather than requiring vartype to be filled in by hand, do this: */
		conf->gen.vartype = PGC_BOOL;
		num_vars++;
	}

	for (i = 0; ConfigureNamesInt[i].gen.name; i++)
	{
		struct config_int *conf = &ConfigureNamesInt[i];

		conf->gen.vartype = PGC_INT;
		num_vars++;
	}

	for (i = 0; ConfigureNamesReal[i].gen.name; i++)
	{
		struct config_real *conf = &ConfigureNamesReal[i];

		conf->gen.vartype = PGC_REAL;
		num_vars++;
	}

	for (i = 0; ConfigureNamesString[i].gen.name; i++)
	{
		struct config_string *conf = &ConfigureNamesString[i];

		conf->gen.vartype = PGC_STRING;
		num_vars++;
	}

	/*
	 * Create table with 20% slack
	 */
	size_vars = num_vars + num_vars / 4;

	guc_vars = (struct config_generic **)
		guc_malloc(FATAL, size_vars * sizeof(struct config_generic *));

	num_vars = 0;

	for (i = 0; ConfigureNamesBool[i].gen.name; i++)
		guc_vars[num_vars++] = &ConfigureNamesBool[i].gen;

	for (i = 0; ConfigureNamesInt[i].gen.name; i++)
		guc_vars[num_vars++] = &ConfigureNamesInt[i].gen;

	for (i = 0; ConfigureNamesReal[i].gen.name; i++)
		guc_vars[num_vars++] = &ConfigureNamesReal[i].gen;

	for (i = 0; ConfigureNamesString[i].gen.name; i++)
		guc_vars[num_vars++] = &ConfigureNamesString[i].gen;

	if (guc_variables)
		free(guc_variables);
	guc_variables = guc_vars;
	num_guc_variables = num_vars;
	size_guc_variables = size_vars;
	qsort((void *) guc_variables, num_guc_variables,
		  sizeof(struct config_generic *), guc_var_compare);
}

static bool
is_custom_class(const char *name, int dotPos)
{
	/*
	 * assign_custom_variable_classes() has made sure no empty identifiers or
	 * whitespace exists in the variable
	 */
	bool		result = false;
	const char *ccs = GetConfigOption("custom_variable_classes");

	if (ccs != NULL)
	{
		const char *start = ccs;

		for (;; ++ccs)
		{
			int			c = *ccs;

			if (c == 0 || c == ',')
			{
				if (dotPos == ccs - start && strncmp(start, name, dotPos) == 0)
				{
					result = true;
					break;
				}
				if (c == 0)
					break;
				start = ccs + 1;
			}
		}
	}
	return result;
}

/*
 * Add a new GUC variable to the list of known variables. The
 * list is expanded if needed.
 */
static bool
add_guc_variable(struct config_generic * var, int elevel)
{
	if (num_guc_variables + 1 >= size_guc_variables)
	{
		/*
		 * Increase the vector by 25%
		 */
		int			size_vars = size_guc_variables + size_guc_variables / 4;
		struct config_generic **guc_vars;

		if (size_vars == 0)
		{
			size_vars = 100;
			guc_vars = (struct config_generic **)
				guc_malloc(elevel, size_vars * sizeof(struct config_generic *));
		}
		else
		{
			guc_vars = (struct config_generic **)
				guc_realloc(elevel, guc_variables, size_vars * sizeof(struct config_generic *));
		}

		if (guc_vars == NULL)
			return false;		/* out of memory */

		guc_variables = guc_vars;
		size_guc_variables = size_vars;
	}
	guc_variables[num_guc_variables++] = var;
	qsort((void *) guc_variables, num_guc_variables,
		  sizeof(struct config_generic *), guc_var_compare);
	return true;
}

/*
 * Create and add a placeholder variable. It's presumed to belong
 * to a valid custom variable class at this point.
 */
static struct config_string *
add_placeholder_variable(const char *name, int elevel)
{
	size_t		sz = sizeof(struct config_string) + sizeof(char *);
	struct config_string *var;
	struct config_generic *gen;

	var = (struct config_string *) guc_malloc(elevel, sz);
	if (var == NULL)
		return NULL;

	gen = &var->gen;
	memset(var, 0, sz);

	gen->name = guc_strdup(elevel, name);
	if (gen->name == NULL)
	{
		free(var);
		return NULL;
	}

	gen->context = PGC_USERSET;
	gen->group = CUSTOM_OPTIONS;
	gen->short_desc = "GUC placeholder variable";
	gen->flags = GUC_NO_SHOW_ALL | GUC_NOT_IN_SAMPLE | GUC_CUSTOM_PLACEHOLDER;
	gen->vartype = PGC_STRING;

	/*
	 * The char* is allocated at the end of the struct since we have no
	 * 'static' place to point to.
	 */
	var->variable = (char **) (var + 1);

	if (!add_guc_variable((struct config_generic *) var, elevel))
	{
		free((void *) gen->name);
		free(var);
		return NULL;
	}

	return var;
}

/*
 * Look up option NAME. If it exists, return a pointer to its record,
 * else return NULL.
 */
static struct config_generic *
find_option(const char *name, int elevel)
{
	const char *dot;
	const char **key = &name;
	struct config_generic **res;
	int			i;

	Assert(name);

	/*
	 * By equating const char ** with struct config_generic *, we are assuming
	 * the name field is first in config_generic.
	 */
	res = (struct config_generic **) bsearch((void *) &key,
											 (void *) guc_variables,
											 num_guc_variables,
											 sizeof(struct config_generic *),
											 guc_var_compare);
	if (res)
		return *res;

	/*
	 * See if the name is an obsolete name for a variable.	We assume that the
	 * set of supported old names is short enough that a brute-force search is
	 * the best way.
	 */
	for (i = 0; map_old_guc_names[i] != NULL; i += 2)
	{
		if (guc_name_compare(name, map_old_guc_names[i]) == 0)
			return find_option(map_old_guc_names[i + 1], elevel);
	}

	/*
	 * Check if the name is qualified, and if so, check if the qualifier maps
	 * to a custom variable class.
	 */
	dot = strchr(name, GUC_QUALIFIER_SEPARATOR);
	if (dot != NULL && is_custom_class(name, dot - name))
		/* Add a placeholder variable for this name */
		return (struct config_generic *) add_placeholder_variable(name, elevel);

	/* Unknown name */
	return NULL;
}


/*
 * comparator for qsorting and bsearching guc_variables array
 */
static int
guc_var_compare(const void *a, const void *b)
{
	struct config_generic *confa = *(struct config_generic **) a;
	struct config_generic *confb = *(struct config_generic **) b;

	return guc_name_compare(confa->name, confb->name);
}

/*
 * the bare comparison function for GUC names
 */
static int
guc_name_compare(const char *namea, const char *nameb)
{
	/*
	 * The temptation to use strcasecmp() here must be resisted, because the
	 * array ordering has to remain stable across setlocale() calls. So, build
	 * our own with a simple ASCII-only downcasing.
	 */
	while (*namea && *nameb)
	{
		char		cha = *namea++;
		char		chb = *nameb++;

		if (cha >= 'A' && cha <= 'Z')
			cha += 'a' - 'A';
		if (chb >= 'A' && chb <= 'Z')
			chb += 'a' - 'A';
		if (cha != chb)
			return cha - chb;
	}
	if (*namea)
		return 1;				/* a is longer */
	if (*nameb)
		return -1;				/* b is longer */
	return 0;
}


/*
 * Initialize GUC options during program startup.
 *
 * Note that we cannot read the config file yet, since we have not yet
 * processed command-line switches.
 */
void
InitializeGUCOptions(void)
{
	int			i;
	char	   *env;
	long		stack_rlimit;
	/*
	 * Before log_line_prefix could possibly receive a nonempty setting, make
	 * sure that timezone processing is minimally alive (see elog.c).
	 */
	pg_timezone_pre_initialize();

	/*
	 * Build sorted array of all GUC variables.
	 */
	build_guc_variables();

	/*
	 * Load all variables with their compiled-in defaults, and initialize
	 * status fields as needed.
	 */
	for (i = 0; i < num_guc_variables; i++)
	{
		struct config_generic *gconf = guc_variables[i];

		gconf->status = 0;
		gconf->reset_source = PGC_S_DEFAULT;
		gconf->tentative_source = PGC_S_DEFAULT;
		gconf->source = PGC_S_DEFAULT;
		gconf->stack = NULL;

		switch (gconf->vartype)
		{
			case PGC_BOOL:
				{
					struct config_bool *conf = (struct config_bool *) gconf;

					if (conf->assign_hook)
						if (!(*conf->assign_hook) (conf->reset_val, true,
												   PGC_S_DEFAULT))
							elog(FATAL, "failed to initialize %s to %d",
								 conf->gen.name, (int) conf->reset_val);
					*conf->variable = conf->reset_val;
					break;
				}
			case PGC_INT:
				{
					struct config_int *conf = (struct config_int *) gconf;

					Assert(conf->reset_val >= conf->min);
					Assert(conf->reset_val <= conf->max);
					if (conf->assign_hook)
						if (!(*conf->assign_hook) (conf->reset_val, true,
												   PGC_S_DEFAULT))
							elog(FATAL, "failed to initialize %s to %d",
								 conf->gen.name, conf->reset_val);
					*conf->variable = conf->reset_val;
					break;
				}
			case PGC_REAL:
				{
					struct config_real *conf = (struct config_real *) gconf;

					Assert(conf->reset_val >= conf->min);
					Assert(conf->reset_val <= conf->max);
					if (conf->assign_hook)
						if (!(*conf->assign_hook) (conf->reset_val, true,
												   PGC_S_DEFAULT))
							elog(FATAL, "failed to initialize %s to %g",
								 conf->gen.name, conf->reset_val);
					*conf->variable = conf->reset_val;
					break;
				}
			case PGC_STRING:
				{
					struct config_string *conf = (struct config_string *) gconf;
					char	   *str;

					*conf->variable = NULL;
					conf->reset_val = NULL;
					conf->tentative_val = NULL;

					if (conf->boot_val == NULL)
					{
						/* Cannot set value yet */
						break;
					}

					str = guc_strdup(FATAL, conf->boot_val);
					conf->reset_val = str;

					if (conf->assign_hook)
					{
						const char *newstr;

						newstr = (*conf->assign_hook) (str, true,
													   PGC_S_DEFAULT);
						if (newstr == NULL)
						{
							elog(FATAL, "failed to initialize %s to \"%s\"",
								 conf->gen.name, str);
						}
						else if (newstr != str)
						{
							free(str);

							/*
							 * See notes in set_config_option about casting
							 */
							str = (char *) newstr;
							conf->reset_val = str;
						}
					}
					*conf->variable = str;
					break;
				}
		}
	}

	guc_dirty = false;
	guc_cannotprepare = false;

	reporting_enabled = false;

	/*
	 * Prevent any attempt to override the transaction modes from
	 * non-interactive sources.
	 */
	SetConfigOption("transaction_isolation", "default",
					PGC_POSTMASTER, PGC_S_OVERRIDE);
	SetConfigOption("transaction_read_only", "no",
					PGC_POSTMASTER, PGC_S_OVERRIDE);

	/*
	 * For historical reasons, some GUC parameters can receive defaults from
	 * environment variables.  Process those settings.
	 */

	env = getenv("PGPORT");
	if (env != NULL)
		SetConfigOption("port", env, PGC_POSTMASTER, PGC_S_ENV_VAR);

	env = getenv("PGDATESTYLE");
	if (env != NULL)
		SetConfigOption("datestyle", env, PGC_POSTMASTER, PGC_S_ENV_VAR);

	env = getenv("PGCLIENTENCODING");
	if (env != NULL)
		SetConfigOption("client_encoding", env, PGC_POSTMASTER, PGC_S_ENV_VAR);

	/*
	 * rlimit isn't exactly an "environment variable", but it behaves about
	 * the same.  If we can identify the platform stack depth rlimit, increase
	 * default stack depth setting up to whatever is safe (but at most 2MB).
	 */
	stack_rlimit = get_stack_depth_rlimit();
	if (stack_rlimit > 0)
	{
		int		new_limit = (stack_rlimit - STACK_DEPTH_SLOP) / 1024L;

		if (new_limit > 100)
		{
			char	limbuf[16];

			new_limit = Min(new_limit, 2048);
			sprintf(limbuf, "%d", new_limit);
			SetConfigOption("max_stack_depth", limbuf,
							PGC_POSTMASTER, PGC_S_ENV_VAR);
		}
	}
}


/*
 * Select the configuration files and data directory to be used, and
 * do the initial read of postgresql.conf.
 *
 * This is called after processing command-line switches.
 *		userDoption is the -D switch value if any (NULL if unspecified).
 *		progname is just for use in error messages.
 *
 * Returns true on success; on failure, prints a suitable error message
 * to stderr and returns false.
 */
bool
SelectConfigFiles(const char *userDoption, const char *progname)
{
	char	   *configdir = NULL;
	char	   *fname;
	struct stat stat_buf;

	/* configdir is -D option, or $PGDATA if no -D */
	if (userDoption)
		configdir = make_absolute_path(userDoption);
	else
		configdir = make_absolute_path(getenv("PGDATA"));

	/*
	 * Find the configuration file: if config_file was specified on the
	 * command line, use it, else use configdir/postgresql.conf.  In any case
	 * ensure the result is an absolute path, so that it will be interpreted
	 * the same way by future backends.
	 */
	if (ConfigFileName)
		fname = make_absolute_path(ConfigFileName);
	else if (configdir)
	{
		fname = guc_malloc(FATAL,
						   strlen(configdir) + strlen(CONFIG_FILENAME) + 2);
		sprintf(fname, "%s/%s", configdir, CONFIG_FILENAME);
	}
	else
	{
		write_stderr("%s does not know where to find the server configuration file.\n"
					 "You must specify the --config-file or -D invocation "
					 "option or set the PGDATA environment variable.\n",
					 progname);
		return false;
	}

	/*
	 * Set the ConfigFileName GUC variable to its final value, ensuring that
	 * it can't be overridden later.
	 */
	SetConfigOption("config_file", fname, PGC_POSTMASTER, PGC_S_OVERRIDE);
	free(fname);

	/*
	 * Now read the config file for the first time.
	 */
	if (stat(ConfigFileName, &stat_buf) != 0)
	{
		write_stderr("%s cannot access the server configuration file \"%s\": %s\n",
					 progname, ConfigFileName, strerror(errno));
		return false;
	}

	ProcessConfigFile(PGC_POSTMASTER);

	/*
	 * If the data_directory GUC variable has been set, use that as DataDir;
	 * otherwise use configdir if set; else punt.
	 *
	 * Note: SetDataDir will copy and absolute-ize its argument, so we don't
	 * have to.
	 */
	if (data_directory)
		SetDataDir(data_directory);
	else if (configdir)
		SetDataDir(configdir);
	else
	{
		write_stderr("%s does not know where to find the database system data.\n"
					 "This can be specified as \"data_directory\" in \"%s\", "
					 "or by the -D invocation option, or by the "
					 "PGDATA environment variable.\n",
					 progname, ConfigFileName);
		return false;
	}

	/*
	 * Reflect the final DataDir value back into the data_directory GUC var.
	 * (If you are wondering why we don't just make them a single variable,
	 * it's because the EXEC_BACKEND case needs DataDir to be transmitted to
	 * child backends specially.  XXX is that still true?  Given that we now
	 * chdir to DataDir, EXEC_BACKEND can read the config file without knowing
	 * DataDir in advance.)
	 */
	SetConfigOption("data_directory", DataDir, PGC_POSTMASTER, PGC_S_OVERRIDE);

	/*
	 * Figure out where pg_hba.conf is, and make sure the path is absolute.
	 */
	if (HbaFileName)
		fname = make_absolute_path(HbaFileName);
	else if (configdir)
	{
		fname = guc_malloc(FATAL,
						   strlen(configdir) + strlen(HBA_FILENAME) + 2);
		sprintf(fname, "%s/%s", configdir, HBA_FILENAME);
	}
	else
	{
		write_stderr("%s does not know where to find the \"hba\" configuration file.\n"
					 "This can be specified as \"hba_file\" in \"%s\", "
					 "or by the -D invocation option, or by the "
					 "PGDATA environment variable.\n",
					 progname, ConfigFileName);
		return false;
	}
	SetConfigOption("hba_file", fname, PGC_POSTMASTER, PGC_S_OVERRIDE);
	free(fname);

	/*
	 * Likewise for pg_ident.conf.
	 */
	if (IdentFileName)
		fname = make_absolute_path(IdentFileName);
	else if (configdir)
	{
		fname = guc_malloc(FATAL,
						   strlen(configdir) + strlen(IDENT_FILENAME) + 2);
		sprintf(fname, "%s/%s", configdir, IDENT_FILENAME);
	}
	else
	{
		write_stderr("%s does not know where to find the \"ident\" configuration file.\n"
					 "This can be specified as \"ident_file\" in \"%s\", "
					 "or by the -D invocation option, or by the "
					 "PGDATA environment variable.\n",
					 progname, ConfigFileName);
		return false;
	}
	SetConfigOption("ident_file", fname, PGC_POSTMASTER, PGC_S_OVERRIDE);
	free(fname);

	free(configdir);

	return true;
}


/*
 * Reset all options to their saved default values (implements RESET ALL)
 */
void
ResetAllOptions(void)
{
	int			i;

	for (i = 0; i < num_guc_variables; i++)
	{
		struct config_generic *gconf = guc_variables[i];

		/* Don't reset non-SET-able values */
		if (gconf->context != PGC_SUSET &&
			gconf->context != PGC_USERSET)
			continue;
		/* Don't reset if special exclusion from RESET ALL */
		if (gconf->flags & GUC_NO_RESET_ALL)
			continue;
		/* No need to reset if wasn't SET */
		if (gconf->source <= PGC_S_OVERRIDE)
			continue;

		/* Save old value to support transaction abort */
		push_old_value(gconf);

		switch (gconf->vartype)
		{
			case PGC_BOOL:
				{
					struct config_bool *conf = (struct config_bool *) gconf;

					if (conf->assign_hook)
						if (!(*conf->assign_hook) (conf->reset_val, true,
												   PGC_S_SESSION))
							elog(ERROR, "failed to reset %s", conf->gen.name);
					*conf->variable = conf->reset_val;
					conf->tentative_val = conf->reset_val;
					conf->gen.source = conf->gen.reset_source;
					conf->gen.tentative_source = conf->gen.reset_source;
					conf->gen.status |= GUC_HAVE_TENTATIVE;
					guc_dirty = true;
					guc_cannotprepare = true;
					break;
				}
			case PGC_INT:
				{
					struct config_int *conf = (struct config_int *) gconf;

					if (conf->assign_hook)
						if (!(*conf->assign_hook) (conf->reset_val, true,
												   PGC_S_SESSION))
							elog(ERROR, "failed to reset %s", conf->gen.name);
					*conf->variable = conf->reset_val;
					conf->tentative_val = conf->reset_val;
					conf->gen.source = conf->gen.reset_source;
					conf->gen.tentative_source = conf->gen.reset_source;
					conf->gen.status |= GUC_HAVE_TENTATIVE;
					guc_dirty = true;
					guc_cannotprepare = true;
					break;
				}
			case PGC_REAL:
				{
					struct config_real *conf = (struct config_real *) gconf;

					if (conf->assign_hook)
						if (!(*conf->assign_hook) (conf->reset_val, true,
												   PGC_S_SESSION))
							elog(ERROR, "failed to reset %s", conf->gen.name);
					*conf->variable = conf->reset_val;
					conf->tentative_val = conf->reset_val;
					conf->gen.source = conf->gen.reset_source;
					conf->gen.tentative_source = conf->gen.reset_source;
					conf->gen.status |= GUC_HAVE_TENTATIVE;
					guc_dirty = true;
					guc_cannotprepare = true;
					break;
				}
			case PGC_STRING:
				{
					struct config_string *conf = (struct config_string *) gconf;
					char	   *str;

					if (conf->reset_val == NULL)
					{
						/* Nothing to reset to, as yet; so do nothing */
						break;
					}

					/* We need not strdup here */
					str = conf->reset_val;

					if (conf->assign_hook)
					{
						const char *newstr;

						newstr = (*conf->assign_hook) (str, true,
													   PGC_S_SESSION);
						if (newstr == NULL)
							elog(ERROR, "failed to reset %s", conf->gen.name);
						else if (newstr != str)
						{
							/*
							 * See notes in set_config_option about casting
							 */
							str = (char *) newstr;
						}
					}

					set_string_field(conf, conf->variable, str);
					set_string_field(conf, &conf->tentative_val, str);
					conf->gen.source = conf->gen.reset_source;
					conf->gen.tentative_source = conf->gen.reset_source;
					conf->gen.status |= GUC_HAVE_TENTATIVE;
					guc_dirty = true;
					guc_cannotprepare = true;
					break;
				}
		}

		if (gconf->flags & GUC_REPORT)
			ReportGUCOption(gconf);
	}
}


/*
 * push_old_value
 *		Push previous state during first assignment to a GUC variable
 *		within a particular transaction.
 *
 * We have to be willing to "back-fill" the state stack if the first
 * assignment occurs within a subtransaction nested several levels deep.
 * This ensures that if an intermediate transaction aborts, it will have
 * the proper value available to restore the setting to.
 */
static void
push_old_value(struct config_generic * gconf)
{
	GucStack   *stack;

	/* If we're not inside a transaction, do nothing */
	if (GUCNestLevel == 0)
		return;

	for (;;)
	{
		/* Done if we already pushed it at this nesting depth */
		if (gconf->stack && gconf->stack->nest_level >= GUCNestLevel)
			return;

		/*
		 * We keep all the stack entries in TopTransactionContext so as to
		 * avoid allocation problems when a subtransaction back-fills stack
		 * entries for upper transaction levels.
		 */
		stack = (GucStack *) MemoryContextAlloc(TopTransactionContext,
												sizeof(GucStack));

		stack->prev = gconf->stack;
		stack->nest_level = stack->prev ? stack->prev->nest_level + 1 : 1;
		stack->status = gconf->status;
		stack->tentative_source = gconf->tentative_source;
		stack->source = gconf->source;

		switch (gconf->vartype)
		{
			case PGC_BOOL:
				stack->tentative_val.boolval =
					((struct config_bool *) gconf)->tentative_val;
				stack->value.boolval =
					*((struct config_bool *) gconf)->variable;
				break;

			case PGC_INT:
				stack->tentative_val.intval =
					((struct config_int *) gconf)->tentative_val;
				stack->value.intval =
					*((struct config_int *) gconf)->variable;
				break;

			case PGC_REAL:
				stack->tentative_val.realval =
					((struct config_real *) gconf)->tentative_val;
				stack->value.realval =
					*((struct config_real *) gconf)->variable;
				break;

			case PGC_STRING:
				stack->tentative_val.stringval =
					((struct config_string *) gconf)->tentative_val;
				stack->value.stringval =
					*((struct config_string *) gconf)->variable;
				break;
		}

		gconf->stack = stack;

		/* Set state to indicate nothing happened yet within this level */
		gconf->status = GUC_HAVE_STACK;

		/* Ensure we remember to pop at end of xact */
		guc_dirty = true;
	}
}

/*
 * Do GUC processing at main transaction start.
 */
void
AtStart_GUC(void)
{
	/*
	 * The nest level should be 0 between transactions; if it isn't,
	 * somebody didn't call AtEOXact_GUC, or called it with the wrong
	 * nestLevel.  We throw a warning but make no other effort to clean up.
	 */
	if (GUCNestLevel != 0)
		elog(WARNING, "GUC nest level = %d at transaction start",
			 GUCNestLevel);
	GUCNestLevel = 1;
}

/*
 * Enter a new nesting level for GUC values.  This is called at subtransaction
 * start and when entering a function that has proconfig settings.  NOTE that
 * we must not risk error here, else subtransaction start will be unhappy.
 */
int
NewGUCNestLevel(void)
{
	return ++GUCNestLevel;
}

/*
 * Do GUC processing at transaction or subtransaction commit or abort, or
 * when exiting a function that has proconfig settings.  (The name is thus
 * a bit of a misnomer; perhaps it should be ExitGUCNestLevel or some such.)
 * During abort, we discard all GUC settings that were applied at nesting
 * levels >= nestLevel.  nestLevel == 1 corresponds to the main transaction.
 */
void
AtEOXact_GUC(bool isCommit, int nestLevel)
{
	int			i;

	/*
	 * Note: it's possible to get here with GUCNestLevel == nestLevel-1 during
	 * abort, if there is a failure during transaction start before
	 * AtStart_GUC is called.
	 */
	Assert(nestLevel > 0 &&
		   (nestLevel <= GUCNestLevel ||
			(nestLevel == GUCNestLevel + 1 && !isCommit)));

	/* Quick exit if nothing's changed in this transaction */
	if (!guc_dirty)
	{
		GUCNestLevel = nestLevel - 1;
		return;
	}

	for (i = 0; i < num_guc_variables; i++)
	{
		struct config_generic *gconf = guc_variables[i];
		int			my_status = gconf->status;
		GucStack   *stack = gconf->stack;
		bool		useTentative;
		bool		changed;

		/*
		 * Skip if nothing's happened to this var in this transaction
		 */
		if (my_status == 0)
		{
			Assert(stack == NULL);
			continue;
		}
		/* Assert that we stacked old value before changing it */
		Assert(stack != NULL && (my_status & GUC_HAVE_STACK));
		/* However, the last change may have been at an outer xact level */
		if (stack->nest_level < nestLevel)
			continue;
		Assert(stack->nest_level == nestLevel);

		/*
		 * We will pop the stack entry.  Start by restoring outer xact status
		 * (since we may want to modify it below).	Be careful to use
		 * my_status to reference the inner xact status below this point...
		 */
		gconf->status = stack->status;

		/*
		 * We have two cases:
		 *
		 * If commit and HAVE_TENTATIVE, set actual value to tentative (this
		 * is to override a SET LOCAL if one occurred later than SET). We keep
		 * the tentative value and propagate HAVE_TENTATIVE to the parent
		 * status, allowing the SET's effect to percolate up. (But if we're
		 * exiting the outermost transaction, we'll drop the HAVE_TENTATIVE
		 * bit below.)
		 *
		 * Otherwise, we have a transaction that aborted or executed only SET
		 * LOCAL (or no SET at all).  In either case it should have no further
		 * effect, so restore both tentative and actual values from the stack
		 * entry.
		 */

		useTentative = isCommit && (my_status & GUC_HAVE_TENTATIVE) != 0;
		changed = false;

		switch (gconf->vartype)
		{
			case PGC_BOOL:
				{
					struct config_bool *conf = (struct config_bool *) gconf;
					bool		newval;
					GucSource	newsource;

					if (useTentative)
					{
						newval = conf->tentative_val;
						newsource = conf->gen.tentative_source;
						conf->gen.status |= GUC_HAVE_TENTATIVE;
					}
					else
					{
						newval = stack->value.boolval;
						newsource = stack->source;
						conf->tentative_val = stack->tentative_val.boolval;
						conf->gen.tentative_source = stack->tentative_source;
					}

					if (*conf->variable != newval)
					{
						if (conf->assign_hook)
							if (!(*conf->assign_hook) (newval,
													   true, PGC_S_OVERRIDE))
								elog(LOG, "failed to commit %s",
									 conf->gen.name);
						*conf->variable = newval;
						changed = true;
					}
					conf->gen.source = newsource;
					break;
				}
			case PGC_INT:
				{
					struct config_int *conf = (struct config_int *) gconf;
					int			newval;
					GucSource	newsource;

					if (useTentative)
					{
						newval = conf->tentative_val;
						newsource = conf->gen.tentative_source;
						conf->gen.status |= GUC_HAVE_TENTATIVE;
					}
					else
					{
						newval = stack->value.intval;
						newsource = stack->source;
						conf->tentative_val = stack->tentative_val.intval;
						conf->gen.tentative_source = stack->tentative_source;
					}

					if (*conf->variable != newval)
					{
						if (conf->assign_hook)
							if (!(*conf->assign_hook) (newval,
													   true, PGC_S_OVERRIDE))
								elog(LOG, "failed to commit %s",
									 conf->gen.name);
						*conf->variable = newval;
						changed = true;
					}
					conf->gen.source = newsource;
					break;
				}
			case PGC_REAL:
				{
					struct config_real *conf = (struct config_real *) gconf;
					double		newval;
					GucSource	newsource;

					if (useTentative)
					{
						newval = conf->tentative_val;
						newsource = conf->gen.tentative_source;
						conf->gen.status |= GUC_HAVE_TENTATIVE;
					}
					else
					{
						newval = stack->value.realval;
						newsource = stack->source;
						conf->tentative_val = stack->tentative_val.realval;
						conf->gen.tentative_source = stack->tentative_source;
					}

					if (*conf->variable != newval)
					{
						if (conf->assign_hook)
							if (!(*conf->assign_hook) (newval,
													   true, PGC_S_OVERRIDE))
								elog(LOG, "failed to commit %s",
									 conf->gen.name);
						*conf->variable = newval;
						changed = true;
					}
					conf->gen.source = newsource;
					break;
				}
			case PGC_STRING:
				{
					struct config_string *conf = (struct config_string *) gconf;
					char	   *newval;
					GucSource	newsource;

					if (useTentative)
					{
						newval = conf->tentative_val;
						newsource = conf->gen.tentative_source;
						conf->gen.status |= GUC_HAVE_TENTATIVE;
					}
					else
					{
						newval = stack->value.stringval;
						newsource = stack->source;
						set_string_field(conf, &conf->tentative_val,
										 stack->tentative_val.stringval);
						conf->gen.tentative_source = stack->tentative_source;
					}

					if (*conf->variable != newval)
					{
						if (conf->assign_hook)
						{
							const char *newstr;

							newstr = (*conf->assign_hook) (newval, true,
														   PGC_S_OVERRIDE);
							if (newstr == NULL)
								elog(LOG, "failed to commit %s",
									 conf->gen.name);
							else if (newstr != newval)
							{
								/*
								 * If newval should now be freed, it'll be
								 * taken care of below.
								 *
								 * See notes in set_config_option about
								 * casting
								 */
								newval = (char *) newstr;
							}
						}

						set_string_field(conf, conf->variable, newval);
						changed = true;
					}
					conf->gen.source = newsource;
					/* Release stacked values if not used anymore */
					set_string_field(conf, &stack->value.stringval,
									 NULL);
					set_string_field(conf, &stack->tentative_val.stringval,
									 NULL);
					/* Don't store tentative value separately after commit */
					if (nestLevel == 1)
						set_string_field(conf, &conf->tentative_val, NULL);
					break;
				}
		}

		/* Finish popping the state stack */
		gconf->stack = stack->prev;
		pfree(stack);

		/*
		 * If we're now out of all xact levels, forget TENTATIVE status bit;
		 * there's nothing tentative about the value anymore.
		 */
		if (nestLevel == 1)
		{
			Assert(gconf->stack == NULL);
			gconf->status = 0;
		}

		/* Report new value if we changed it */
		if (changed && (gconf->flags & GUC_REPORT))
			ReportGUCOption(gconf);
	}

	/*
	 * If we're now out of all xact levels, we can clear guc_dirty. (Note: we
	 * cannot reset guc_dirty when exiting a subtransaction, because we know
	 * that all outer transaction levels will have stacked values to deal
	 * with.)
	 */
	if (nestLevel == 1)
		guc_dirty = false;

	/* Update nesting level */
	GUCNestLevel = nestLevel - 1;
}


/*
 * Start up automatic reporting of changes to variables marked GUC_REPORT.
 * This is executed at completion of backend startup.
 */
void
BeginReportingGUCOptions(void)
{
	int			i;

    /* Build global lists of GUCs for use by callers of gp_guc_list_show(). */
    gp_guc_list_init();

	/*
	 * Don't do anything unless talking to an interactive frontend of protocol
	 * 3.0 or later.
	 */
	if (whereToSendOutput != DestRemote ||
		PG_PROTOCOL_MAJOR(FrontendProtocol) < 3)
		return;

	reporting_enabled = true;

	/* Transmit initial values of interesting variables */
	for (i = 0; i < num_guc_variables; i++)
	{
		struct config_generic *conf = guc_variables[i];

		if (conf->flags & GUC_REPORT)
			ReportGUCOption(conf);
	}
}

/*
 * ReportGUCOption: if appropriate, transmit option value to frontend
 */
static void
ReportGUCOption(struct config_generic * record)
{
	if (reporting_enabled && (record->flags & GUC_REPORT))
	{
		char	   *val = _ShowOption(record, false);
		StringInfoData msgbuf;

		pq_beginmessage(&msgbuf, 'S');
		pq_sendstring(&msgbuf, record->name);
		pq_sendstring(&msgbuf, val);
		pq_endmessage(&msgbuf);

		pfree(val);
	}
}


/*
 * Try to parse value as an integer.  The accepted formats are the
 * usual decimal, octal, or hexadecimal formats, optionally followed by
 * a unit name if "flags" indicates a unit is allowed.
 *
 * If the string parses okay, return true, else false.
 * If okay and result is not NULL, return the value in *result.
 * If not okay and hintmsg is not NULL, *hintmsg is set to a suitable
 *	HINT message, or NULL if no hint provided.
 */
bool
parse_int(const char *value, int *result, int flags, const char **hintmsg)
{
	int64		val;
	char	   *endptr;

	/* To suppress compiler warnings, always set output params */
	if (result)
		*result = 0;
	if (hintmsg)
		*hintmsg = NULL;

	/* We assume here that int64 is at least as wide as long */
	errno = 0;
	val = strtol(value, &endptr, 0);

	if (endptr == value)
		return false;			/* no HINT for integer syntax error */

	if (errno == ERANGE || val != (int64) ((int32) val))
	{
		if (hintmsg)
			*hintmsg = gettext_noop("Value exceeds integer range.");
		return false;
	}

	/* allow whitespace between integer and unit */
	while (isspace((unsigned char) *endptr))
		endptr++;

	/* Handle possible unit */
	if (*endptr != '\0')
	{
		/*
		 * Note: the multiple-switch coding technique here is a bit tedious,
		 * but seems necessary to avoid intermediate-value overflows.
		 *
		 * If INT64_IS_BUSTED (ie, it's really int32) we will fail to detect
		 * overflow due to units conversion, but there are few enough such
		 * machines that it does not seem worth trying to be smarter.
		 */
		if (flags & GUC_UNIT_MEMORY)
		{
			/* Set hint for use if no match or trailing garbage */
			if (hintmsg)
				*hintmsg = gettext_noop("Valid units for this parameter are \"kB\", \"MB\", and \"GB\".");

#if BLCKSZ < 1024 || BLCKSZ > (1024*1024)
#error BLCKSZ must be between 1KB and 1MB
#endif
#if XLOG_BLCKSZ < 1024 || XLOG_BLCKSZ > (1024*1024)
#error XLOG_BLCKSZ must be between 1KB and 1MB
#endif

			if (strncmp(endptr, "kB", 2) == 0)
			{
				endptr += 2;
				switch (flags & GUC_UNIT_MEMORY)
				{
					case GUC_UNIT_BLOCKS:
						val /= (BLCKSZ / 1024);
						break;
					case GUC_UNIT_XBLOCKS:
						val /= (XLOG_BLCKSZ / 1024);
						break;
				}
			}
			else if (strncmp(endptr, "MB", 2) == 0)
			{
				endptr += 2;
				switch (flags & GUC_UNIT_MEMORY)
				{
					case GUC_UNIT_KB:
						val *= KB_PER_MB;
						break;
					case GUC_UNIT_BLOCKS:
						val *= KB_PER_MB / (BLCKSZ / 1024);
						break;
					case GUC_UNIT_XBLOCKS:
						val *= KB_PER_MB / (XLOG_BLCKSZ / 1024);
						break;
				}
			}
			else if (strncmp(endptr, "GB", 2) == 0)
			{
				endptr += 2;
				switch (flags & GUC_UNIT_MEMORY)
				{
					case GUC_UNIT_KB:
						val *= KB_PER_GB;
						break;
					case GUC_UNIT_BLOCKS:
						val *= KB_PER_GB / (BLCKSZ / 1024);
						break;
					case GUC_UNIT_XBLOCKS:
						val *= KB_PER_GB / (XLOG_BLCKSZ / 1024);
						break;
				}
			}
		}
		else if (flags & GUC_UNIT_TIME)
		{
			/* Set hint for use if no match or trailing garbage */
			if (hintmsg)
				*hintmsg = gettext_noop("Valid units for this parameter are \"ms\", \"s\", \"min\", \"h\", and \"d\".");

			if (strncmp(endptr, "ms", 2) == 0)
			{
				endptr += 2;
				switch (flags & GUC_UNIT_TIME)
				{
					case GUC_UNIT_S:
						val /= MS_PER_S;
						break;
					case GUC_UNIT_MIN:
						val /= MS_PER_MIN;
						break;
				}
			}
			else if (strncmp(endptr, "s", 1) == 0)
			{
				endptr += 1;
				switch (flags & GUC_UNIT_TIME)
				{
					case GUC_UNIT_MS:
						val *= MS_PER_S;
						break;
					case GUC_UNIT_MIN:
						val /= S_PER_MIN;
						break;
				}
			}
			else if (strncmp(endptr, "min", 3) == 0)
			{
				endptr += 3;
				switch (flags & GUC_UNIT_TIME)
				{
					case GUC_UNIT_MS:
						val *= MS_PER_MIN;
						break;
					case GUC_UNIT_S:
						val *= S_PER_MIN;
						break;
				}
			}
			else if (strncmp(endptr, "h", 1) == 0)
			{
				endptr += 1;
				switch (flags & GUC_UNIT_TIME)
				{
					case GUC_UNIT_MS:
						val *= MS_PER_H;
						break;
					case GUC_UNIT_S:
						val *= S_PER_H;
						break;
					case GUC_UNIT_MIN:
						val *= MIN_PER_H;
						break;
				}
			}
			else if (strncmp(endptr, "d", 1) == 0)
			{
				endptr += 1;
				switch (flags & GUC_UNIT_TIME)
				{
					case GUC_UNIT_MS:
						val *= MS_PER_D;
						break;
					case GUC_UNIT_S:
						val *= S_PER_D;
						break;
					case GUC_UNIT_MIN:
						val *= MIN_PER_D;
						break;
				}
			}
		}

		/* allow whitespace after unit */
		while (isspace((unsigned char) *endptr))
			endptr++;

		if (*endptr != '\0')
			return false;		/* appropriate hint, if any, already set */

		/* Check for overflow due to units conversion */
		if (val != (int64) ((int32) val))
		{
			if (hintmsg)
				*hintmsg = gettext_noop("Value exceeds integer range.");
			return false;
		}
	}

	if (result)
		*result = (int) val;
	return true;
}



/*
 * Try to parse value as a floating point constant in the usual
 * format.	If the value parsed okay return true, else false.  If
 * result is not NULL, return the semantic value there.
 */
static bool
parse_real(const char *value, double *result, int flags)
{
	double		val;
	char	   *endptr;

	errno = 0;
	val = strtod(value, &endptr);

	if ((flags & GUC_UNIT_MEMORY) && endptr != value)
	{
		bool		used = false;

		while (*endptr == ' ')
			endptr++;

		if (strcmp(endptr, "kB") == 0)
		{
			used = true;
			endptr += 2;
		}
		else if (strcmp(endptr, "MB") == 0)
		{
			val *= KB_PER_MB;
			used = true;
			endptr += 2;
		}
		else if (strcmp(endptr, "GB") == 0)
		{
			val *= KB_PER_GB;
			used = true;
			endptr += 2;
		}

		if (used)
		{
			switch (flags & GUC_UNIT_MEMORY)
			{
				case GUC_UNIT_BLOCKS:
					val /= (BLCKSZ / 1024);
					break;
				case GUC_UNIT_XBLOCKS:
					val /= (XLOG_BLCKSZ / 1024);
					break;
			}
		}
	}

    if (endptr == value || *endptr != '\0' || errno == ERANGE)
	{
		if (result)
			*result = 0;		/* suppress compiler warning */
		return false;
	}
	if (result)
		*result = val;
	return true;
}


/*
 * Call a GucStringAssignHook function, being careful to free the
 * "newval" string if the hook ereports.
 *
 * This is split out of set_config_option just to avoid the "volatile"
 * qualifiers that would otherwise have to be plastered all over.
 */
static const char *
call_string_assign_hook(GucStringAssignHook assign_hook,
						char *newval, bool doit, GucSource source)
{
	const char *result = NULL;

	PG_TRY();
	{
		result = (*assign_hook) (newval, doit, source);
	}
	PG_CATCH();
	{
		free(newval);
		PG_RE_THROW();
	}
	PG_END_TRY();

	return result;
}


/*
 * Sets option `name' to given value. The value should be a string
 * which is going to be parsed and converted to the appropriate data
 * type.  The context and source parameters indicate in which context this
 * function is being called so it can apply the access restrictions
 * properly.
 *
 * If value is NULL, set the option to its default value (normally the
 * reset_val, but if source == PGC_S_DEFAULT we instead use the boot_val).
 *
 * action indicates whether to set the value globally in the session, locally
 * to the current top transaction, or just for the duration of a function call.
 *
 * If changeVal is false then don't really set the option but do all
 * the checks to see if it would work.
 *
 * If there is an error (non-existing option, invalid value) then an
 * ereport(ERROR) is thrown *unless* this is called in a context where we
 * don't want to ereport (currently, startup or SIGHUP config file reread).
 * In that case we write a suitable error message via ereport(LOG) and
 * return false. This is working around the deficiencies in the ereport
 * mechanism, so don't blame me.  In all other cases, the function
 * returns true, including cases where the input is valid but we chose
 * not to apply it because of context or source-priority considerations.
 *
 * See also SetConfigOption for an external interface.
 */
bool
set_config_option(const char *name, const char *value,
				  GucContext context, GucSource source,
				  bool isLocal, bool changeVal)
{
	struct config_generic *record;
	int			elevel;
	bool		makeDefault;

	if (context == PGC_SIGHUP || context == PGC_POSTMASTER || source == PGC_S_DEFAULT)
	{
		/*
		 * To avoid cluttering the log, only the postmaster bleats loudly
		 * about problems with the config file.
		 */
		elevel = IsUnderPostmaster ? DEBUG3 : LOG;
	}
	else if (source == PGC_S_DATABASE || source == PGC_S_USER)
		elevel = WARNING;
	else
		elevel = ERROR;

	record = find_option(name, elevel);
	if (record == NULL)
	{
		ereport(elevel,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
			   errmsg("unrecognized configuration parameter \"%s\"", name),
			   errSendAlert(false)));
		return false;
	}

        /*
         * Check if option can be set by the user.
         */
        if (record->flags & GUC_DISALLOW_USER_SET)
 	{
              /* Only print a warning in the dispatch or utility mode */
          if (changeVal)
  	          if (Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_UTILITY)
	              elog(WARNING, "\"%s\": can not be set by the user and will be ignored.", name);
	      return true;
        }  /* end if (record->flags & GUC_DISALLOW_USER_SET) */

	/*
	 * Check if the option can be set at this time. See guc.h for the precise
	 * rules. Note that we don't want to throw errors if we're in the SIGHUP
	 * context. In that case we just ignore the attempt and return true.
	 */
	switch (record->context)
	{
		case PGC_INTERNAL:
			if (context == PGC_SIGHUP)
				return true;
			if (context != PGC_INTERNAL)
			{
				ereport(elevel,
						(errcode(ERRCODE_CANT_CHANGE_RUNTIME_PARAM),
						 errmsg("parameter \"%s\" cannot be changed",
								name)));
				return false;
			}
			break;
		case PGC_POSTMASTER:
			if (context == PGC_SIGHUP)
			{
				/*
				 * We are reading a PGC_POSTMASTER var from postgresql.conf.
				 * We can't change the setting, so give a warning if the DBA
				 * tries to change it.	(Throwing an error would be more
				 * consistent, but seems overly rigid.)
				 */
				if (changeVal && !is_newvalue_equal(record, value))
					ereport(elevel,
							(errcode(ERRCODE_CANT_CHANGE_RUNTIME_PARAM),
					   errmsg("attempted change of parameter \"%s\" ignored",
							  name),
							 errdetail("This parameter cannot be changed after server start.")));
				return true;
			}
			if (context != PGC_POSTMASTER)
			{
				ereport(elevel,
						(errcode(ERRCODE_CANT_CHANGE_RUNTIME_PARAM),
					   errmsg("attempted change of parameter \"%s\" ignored",
							  name),
						 errdetail("This parameter cannot be changed after server start.")));
				return false;
			}
			break;
		case PGC_SIGHUP:
			if (context != PGC_SIGHUP && context != PGC_POSTMASTER)
			{
				ereport(elevel,
						(errcode(ERRCODE_CANT_CHANGE_RUNTIME_PARAM),
						 errmsg("parameter \"%s\" cannot be changed now",
								name)));
				return false;
			}

			/*
			 * Hmm, the idea of the SIGHUP context is "ought to be global, but
			 * can be changed after postmaster start". But there's nothing
			 * that prevents a crafty administrator from sending SIGHUP
			 * signals to individual backends only.
			 */
			break;
		case PGC_BACKEND:
			if (context == PGC_SIGHUP)
			{
				/*
				 * If a PGC_BACKEND parameter is changed in the config file,
				 * we want to accept the new value in the postmaster (whence
				 * it will propagate to subsequently-started backends), but
				 * ignore it in existing backends.	This is a tad klugy, but
				 * necessary because we don't re-read the config file during
				 * backend start.
				 */
				if (IsUnderPostmaster)
					return true;
			}
			else if (context != PGC_BACKEND && context != PGC_POSTMASTER &&
					 source != PGC_S_CLIENT)
			{
				ereport(elevel,
						(errcode(ERRCODE_CANT_CHANGE_RUNTIME_PARAM),
						 errmsg("parameter \"%s\" cannot be set after connection start",
								name)));
				return false;
			}
			break;
		case PGC_SUSET:
			if (context == PGC_USERSET || context == PGC_BACKEND)
			{
				ereport(elevel,
						(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
						 errmsg("permission denied to set parameter \"%s\"",
								name)));
				return false;
			}
			break;
		case PGC_USERSET:
			/* always okay */
			break;
	}

	/* Print out warnings for the attempt to set the GUC in DEPRECATED_OPTIONS. */
	if (record->group == DEPRECATED_OPTIONS)
	{
		/* Only print a warning in the dispatch or utility mode */
		if (Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_UTILITY)
			elog(WARNING, "\"%s\": setting is deprecated, and may be removed"
				 " in a future release.", name);
	}

	/* Ignore attempted set if the config_group is DEFUNCT_OPTIONS.*/
	if (record->group == DEFUNCT_OPTIONS)
	{
		/* Only print a warning in the dispatch or utility mode */
		if (Gp_role == GP_ROLE_DISPATCH || Gp_role == GP_ROLE_UTILITY)
			elog(WARNING, "\"%s\": setting is ignored because it is defunct",
				 name);
		return true;
	}

	/*
	 * Disallow changing GUC_NOT_WHILE_SEC_REST values if we are inside a
	 * security restriction context.  We can reject this regardless of
	 * the GUC context or source, mainly because sources that it might be
	 * reasonable to override for won't be seen while inside a function.
	 *
	 * Note: variables marked GUC_NOT_WHILE_SEC_REST should usually be marked
	 * GUC_NO_RESET_ALL as well, because ResetAllOptions() doesn't check this.
	 * An exception might be made if the reset value is assumed to be "safe".
	 *
	 * Note: this flag is currently used for "session_authorization" and
	 * "role".  We need to prohibit changing these inside a local userid
	 * context because when we exit it, GUC won't be notified, leaving things
	 * out of sync.  (This could be fixed by forcing a new GUC nesting level,
	 * but that would change behavior in possibly-undesirable ways.)  Also,
	 * we prohibit changing these in a security-restricted operation because
	 * otherwise RESET could be used to regain the session user's privileges.
	 */
	if (record->flags & GUC_NOT_WHILE_SEC_REST)
	{
		if (InLocalUserIdChange())
		{
			/*
			 * Phrasing of this error message is historical, but it's the
			 * most common case.
			 */
			ereport(elevel,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("cannot set parameter \"%s\" within security-definer function",
							name)));
			return false;
		}
		if (InSecurityRestrictedOperation())
		{
			ereport(elevel,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("cannot set parameter \"%s\" within security-restricted operation",
							name)));
			return false;
		}
	}

	/*
	 * Should we set reset/stacked values?	(If so, the behavior is not
	 * transactional.)	This is done either when we get a default value from
	 * the database's/user's/client's default settings or when we reset a
	 * value to its default.
	 */
	makeDefault = changeVal && (source <= PGC_S_OVERRIDE) &&
	    (value != NULL);

	/*
	 * Ignore attempted set if overridden by previously processed setting.
	 * However, if changeVal is false then plow ahead anyway since we are
	 * trying to find out if the value is potentially good, not actually use
	 * it. Also keep going if makeDefault is true, since we may want to set
	 * the reset/stacked values even if we can't set the variable itself.
	 */
	if (record->source > source)
	{
		if (changeVal && !makeDefault)
		{
			elog(DEBUG3, "\"%s\": setting ignored because previous source is higher priority",
				 name);
			return true;
		}
		changeVal = false;
	}

	/*
	 * Evaluate value and set variable.
	 */
	switch (record->vartype)
	{
		case PGC_BOOL:
			{
				struct config_bool *conf = (struct config_bool *) record;
				bool		newval;

				if (value)
				{
					if (!parse_bool(value, &newval))
					{
						ereport(elevel,
								(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						  errmsg("parameter \"%s\" requires a Boolean value",
								 name)));
						return false;
					}
				}
				else
				{
					newval = conf->reset_val;
					source = conf->gen.reset_source;
				}

				/* Save old value to support transaction abort */
				if (changeVal && !makeDefault)
					push_old_value(&conf->gen);

				if (conf->assign_hook)
					if (!(*conf->assign_hook) (newval, changeVal, source))
					{
						ereport(elevel,
								(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							 errmsg("invalid value for parameter \"%s\": %d",
									name, (int) newval)));
						return false;
					}

				if (changeVal || makeDefault)
				{
					if (changeVal)
					{
						*conf->variable = newval;
						conf->gen.source = source;
					}
					if (makeDefault)
					{
						GucStack   *stack;

						if (conf->gen.reset_source <= source)
						{
							conf->reset_val = newval;
							conf->gen.reset_source = source;
						}
						for (stack = conf->gen.stack; stack; stack = stack->prev)
						{
							if (stack->source <= source)
							{
								stack->value.boolval = newval;
								stack->source = source;
							}
						}
					}
					else if (isLocal)
					{
						conf->gen.status |= GUC_HAVE_LOCAL;
						guc_dirty = true;
					}
					else
					{
						conf->tentative_val = newval;
						conf->gen.tentative_source = source;
						conf->gen.status |= GUC_HAVE_TENTATIVE;
						guc_dirty = true;
					}
				}
				break;
			}

		case PGC_INT:
			{
				struct config_int *conf = (struct config_int *) record;
				int			newval;

				if (value)
				{
					const char *hintmsg;

					if (!parse_int(value, &newval, conf->gen.flags, &hintmsg))
					{
						ereport(elevel,
								(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 		 errmsg("invalid value for parameter \"%s\": \"%s\"",
										name, value),
						 		 hintmsg ? errhint("%s", hintmsg) : 0));
						return false;
					}
					if (newval < conf->min || newval > conf->max)
					{
						ereport(elevel,
								(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
								 errmsg("%d is outside the valid range for parameter \"%s\" (%d .. %d)",
										newval, name, conf->min, conf->max)));
						return false;
					}

					/*
					 * If this is for "work_mem", its value also has to be smaller than or equal to
					 * max_work_mem setting.
					 */
					if (strcmp(conf->gen.name, "work_mem") == 0 &&
						newval > max_work_mem)
					{
						ereport(elevel,
								(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
								 errmsg("%d is outside the valid range for parameter \"%s\" (%d .. %d)",
										newval, name, conf->min, max_work_mem)));
						return false;
					}

					/*
					 * If this is for "max_work_mem", its value has to be greater than or equal to
					 * current work_mem setting.
					 */
					if (strcmp(conf->gen.name, "max_work_mem") == 0 &&
						newval < work_mem)
					{
						ereport(elevel,
								(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
								 errmsg("%d is outside the valid range for parameter \"%s\" (%d .. %d)",
										newval, name, work_mem, conf->max)));
						return false;
					}
				}
				else
				{
					newval = conf->reset_val;
					source = conf->gen.reset_source;
				}

				/* Save old value to support transaction abort */
				if (changeVal && !makeDefault)
					push_old_value(&conf->gen);

				if (conf->assign_hook)
					if (!(*conf->assign_hook) (newval, changeVal, source))
					{
						ereport(elevel,
								(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							 errmsg("invalid value for parameter \"%s\": %d",
									name, newval)));
						return false;
					}

				if (changeVal || makeDefault)
				{
					if (changeVal)
					{
						*conf->variable = newval;
						conf->gen.source = source;
					}
					if (makeDefault)
					{
						GucStack   *stack;

						if (conf->gen.reset_source <= source)
						{
							conf->reset_val = newval;
							conf->gen.reset_source = source;
						}
						for (stack = conf->gen.stack; stack; stack = stack->prev)
						{
							if (stack->source <= source)
							{
								stack->value.intval = newval;
								stack->source = source;
							}
						}
					}
					else if (isLocal)
					{
						conf->gen.status |= GUC_HAVE_LOCAL;
						guc_dirty = true;
					}
					else
					{
						conf->tentative_val = newval;
						conf->gen.tentative_source = source;
						conf->gen.status |= GUC_HAVE_TENTATIVE;
						guc_dirty = true;
					}
				}
				break;
			}

		case PGC_REAL:
			{
				struct config_real *conf = (struct config_real *) record;
				double		newval;

				if (value)
				{
					if (!parse_real(value, &newval, conf->gen.flags))
					{
						ereport(elevel,
								(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						  errmsg("parameter \"%s\" requires a numeric value",
								 name)));
						return false;
					}
					if (newval < conf->min || newval > conf->max)
					{
						ereport(elevel,
								(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
								 errmsg("%g is outside the valid range for parameter \"%s\" (%g .. %g)",
										newval, name, conf->min, conf->max)));
						return false;
					}
				}
				else
				{
					newval = conf->reset_val;
					source = conf->gen.reset_source;
				}

				/* Save old value to support transaction abort */
				if (changeVal && !makeDefault)
					push_old_value(&conf->gen);

				if (conf->assign_hook)
					if (!(*conf->assign_hook) (newval, changeVal, source))
					{
						ereport(elevel,
								(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							 errmsg("invalid value for parameter \"%s\": %g",
									name, newval)));
						return false;
					}

				if (changeVal || makeDefault)
				{
					if (changeVal)
					{
						*conf->variable = newval;
						conf->gen.source = source;
					}
					if (makeDefault)
					{
						GucStack   *stack;

						if (conf->gen.reset_source <= source)
						{
							conf->reset_val = newval;
							conf->gen.reset_source = source;
						}
						for (stack = conf->gen.stack; stack; stack = stack->prev)
						{
							if (stack->source <= source)
							{
								stack->value.realval = newval;
								stack->source = source;
							}
						}
					}
					else if (isLocal)
					{
						conf->gen.status |= GUC_HAVE_LOCAL;
						guc_dirty = true;
					}
					else
					{
						conf->tentative_val = newval;
						conf->gen.tentative_source = source;
						conf->gen.status |= GUC_HAVE_TENTATIVE;
						guc_dirty = true;
					}
				}
				break;
			}

		case PGC_STRING:
			{
				struct config_string *conf = (struct config_string *) record;
				char	   *newval;

				if (value)
				{
					newval = guc_strdup(elevel, value);
					if (newval == NULL)
						return false;

					/*
					 * The only sort of "parsing" check we need to do is apply
					 * truncation if GUC_IS_NAME.
					 */
					if (conf->gen.flags & GUC_IS_NAME)
						truncate_identifier(newval, strlen(newval), true);
				}
				else if (conf->reset_val)
				{
					/*
					 * We could possibly avoid strdup here, but easier to make
					 * this case work the same as the normal assignment case.
					 */
					newval = guc_strdup(elevel, conf->reset_val);
					if (newval == NULL)
						return false;
					source = conf->gen.reset_source;
				}
				else
				{
					/* Nothing to reset to, as yet; so do nothing */
					break;
				}

				/* Save old value to support transaction abort */
				if (changeVal && !makeDefault)
					push_old_value(&conf->gen);

				if (conf->assign_hook)
				{
					const char *hookresult;

					/*
					 * If the hook ereports, we have to make sure we free
					 * newval, else it will be a permanent memory leak.
					 */
					hookresult = call_string_assign_hook(conf->assign_hook,
														 newval,
														 changeVal,
														 source);
					if (hookresult == NULL)
					{
						free(newval);
						ereport(elevel,
								(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("invalid value for parameter \"%s\": \"%s\"",
								name, value ? value : "")));
						return false;
					}
					else if (hookresult != newval)
					{
						free(newval);

						/*
						 * Having to cast away const here is annoying, but the
						 * alternative is to declare assign_hooks as returning
						 * char*, which would mean they'd have to cast away
						 * const, or as both taking and returning char*, which
						 * doesn't seem attractive either --- we don't want
						 * them to scribble on the passed str.
						 */
						newval = (char *) hookresult;
					}
				}

				if (changeVal || makeDefault)
				{
					if (changeVal)
					{
						set_string_field(conf, conf->variable, newval);
						conf->gen.source = source;
					}
					if (makeDefault)
					{
						GucStack   *stack;

						if (conf->gen.reset_source <= source)
						{
							set_string_field(conf, &conf->reset_val, newval);
							conf->gen.reset_source = source;
						}
						for (stack = conf->gen.stack; stack; stack = stack->prev)
						{
							if (stack->source <= source)
							{
								set_string_field(conf, &stack->value.stringval,
												 newval);
								stack->source = source;
							}
						}
						/* Perhaps we didn't install newval anywhere */
						if (!string_field_used(conf, newval))
							free(newval);
					}
					else if (isLocal)
					{
						conf->gen.status |= GUC_HAVE_LOCAL;
						guc_dirty = true;
					}
					else
					{
						set_string_field(conf, &conf->tentative_val, newval);
						conf->gen.tentative_source = source;
						conf->gen.status |= GUC_HAVE_TENTATIVE;
						guc_dirty = true;
					}
				}
				else
					free(newval);
				break;
			}
	}

	if (changeVal && (record->flags & GUC_REPORT))
		ReportGUCOption(record);

	return true;
}


/*
 * Set a config option to the given value. See also set_config_option,
 * this is just the wrapper to be called from outside GUC.	NB: this
 * is used only for non-transactional operations.
 */
void
SetConfigOption(const char *name, const char *value,
				GucContext context, GucSource source)
{
	(void) set_config_option(name, value, context, source, false, true);
}



/*
 * Fetch the current value of the option `name'. If the option doesn't exist,
 * throw an ereport and don't return.
 *
 * The string is *not* allocated for modification and is really only
 * valid until the next call to configuration related functions.
 */
const char *
GetConfigOption(const char *name)
{
	struct config_generic *record;
	static char buffer[256];

	record = find_option(name, ERROR);
	if (record == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
			   errmsg("unrecognized configuration parameter \"%s\"", name)));
	if ((record->flags & GUC_SUPERUSER_ONLY) && !superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("must be superuser to examine \"%s\"", name)));

	switch (record->vartype)
	{
		case PGC_BOOL:
			return *((struct config_bool *) record)->variable ? "on" : "off";

		case PGC_INT:
			snprintf(buffer, sizeof(buffer), "%d",
					 *((struct config_int *) record)->variable);
			return buffer;

		case PGC_REAL:
			snprintf(buffer, sizeof(buffer), "%g",
					 *((struct config_real *) record)->variable);
			return buffer;

		case PGC_STRING:
			return *((struct config_string *) record)->variable;
	}
	return NULL;
}

/*
 * Get the RESET value associated with the given option.
 */
const char *
GetConfigOptionResetString(const char *name)
{
	struct config_generic *record;
	static char buffer[256];

	record = find_option(name, ERROR);
	if (record == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
			   errmsg("unrecognized configuration parameter \"%s\"", name)));
	if ((record->flags & GUC_SUPERUSER_ONLY) && !superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("must be superuser to examine \"%s\"", name)));

	switch (record->vartype)
	{
		case PGC_BOOL:
			return ((struct config_bool *) record)->reset_val ? "on" : "off";

		case PGC_INT:
			snprintf(buffer, sizeof(buffer), "%d",
					 ((struct config_int *) record)->reset_val);
			return buffer;

		case PGC_REAL:
			snprintf(buffer, sizeof(buffer), "%g",
					 ((struct config_real *) record)->reset_val);
			return buffer;

		case PGC_STRING:
			return ((struct config_string *) record)->reset_val;
	}
	return NULL;
}

/*
 * Detect whether the given configuration option can only be set by
 * a superuser.
 */
bool
IsSuperuserConfigOption(const char *name)
{
	struct config_generic *record;

	record = find_option(name, ERROR);
	/* On an unrecognized name, don't error, just return false. */
	if (record == NULL)
		return false;
	return (record->context == PGC_SUSET);
}


/*
 * flatten_set_variable_args
 *		Given a parsenode List as emitted by the grammar for SET,
 *		convert to the flat string representation used by GUC.
 *
 * We need to be told the name of the variable the args are for, because
 * the flattening rules vary (ugh).
 *
 * The result is NULL if input is NIL (ie, SET ... TO DEFAULT), otherwise
 * a palloc'd string.
 */
char *
flatten_set_variable_args(const char *name, List *args)
{
	struct config_generic *record;
	int			flags;
	StringInfoData buf;
	ListCell   *l;

	/*
	 * Fast path if just DEFAULT.  We do not check the variable name in this
	 * case --- necessary for RESET ALL to work correctly.
	 */
	if (args == NIL)
		return NULL;

	/* Else get flags for the variable */
	record = find_option(name, ERROR);
	if (record == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
			   errmsg("unrecognized configuration parameter \"%s\"", name)));

	flags = record->flags;

	/* Complain if list input and non-list variable */
	if ((flags & GUC_LIST_INPUT) == 0 &&
		list_length(args) != 1)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("SET %s takes only one argument", name)));

	initStringInfo(&buf);

	foreach(l, args)
	{
		A_Const    *arg = (A_Const *) lfirst(l);
		char	   *val;

		if (l != list_head(args))
			appendStringInfo(&buf, ", ");

		if (!IsA(arg, A_Const))
			elog(ERROR, "unrecognized node type: %d", (int) nodeTag(arg));

		switch (nodeTag(&arg->val))
		{
			case T_Integer:
				appendStringInfo(&buf, "%ld", intVal(&arg->val));
				break;
			case T_Float:
				/* represented as a string, so just copy it */
				appendStringInfoString(&buf, strVal(&arg->val));
				break;
			case T_String:
				val = strVal(&arg->val);
				if (arg->typname != NULL)
				{
					/*
					 * Must be a ConstInterval argument for TIME ZONE. Coerce
					 * to interval and back to normalize the value and account
					 * for any typmod.
					 */
					Datum		interval;
					char	   *intervalout;

					interval =
						DirectFunctionCall3(interval_in,
											CStringGetDatum(val),
											ObjectIdGetDatum(InvalidOid),
									   Int32GetDatum(arg->typname->typmod));

					intervalout =
						DatumGetCString(DirectFunctionCall1(interval_out,
															interval));
					appendStringInfo(&buf, "INTERVAL '%s'", intervalout);
				}
				else
				{
					/*
					 * Plain string literal or identifier.	For quote mode,
					 * quote it if it's not a vanilla identifier.
					 */
					if (flags & GUC_LIST_QUOTE)
						appendStringInfoString(&buf, quote_identifier(val));
					else
						appendStringInfoString(&buf, val);
				}
				break;
			default:
				elog(ERROR, "unrecognized node type: %d",
					 (int) nodeTag(&arg->val));
				break;
		}
	}

	return buf.data;
}


/*
 * SET command
 */
void
SetPGVariable(const char *name, List *args, bool is_local, bool gp_dispatch)
{
	char	   *argstring = flatten_set_variable_args(name, args);

	/* Note SET DEFAULT (argstring == NULL) is equivalent to RESET */
	set_config_option(name,
					  argstring,
					  (superuser() ? PGC_SUSET : PGC_USERSET),
					  PGC_S_SESSION,
					  is_local,
					  true);

	if (Gp_role == GP_ROLE_DISPATCH && !IsBootstrapProcessingMode() &&
			strcmp(name,"gp_use_dispatch_agent")!=0)
	{

		ListCell * l;
		StringInfoData buffer;

		initStringInfo( &buffer );

        if (argstring == NULL)
        {
            appendStringInfo(&buffer, "RESET %s", name);
            args = NIL;
        }
        else
        {
            appendStringInfo(&buffer, "SET ");
		    if (is_local)
			    appendStringInfo(&buffer, "LOCAL ");

		    appendStringInfo(&buffer, "%s TO ", name);
        }

		foreach(l, args)
		{
			A_Const    *arg = (A_Const *) lfirst(l);
			char	   *val;

			if (l != list_head(args))
				appendStringInfo(&buffer, ", ");

			if (!IsA(arg, A_Const))
				elog(ERROR, "unrecognized node type: %d", (int) nodeTag(arg));

			switch (nodeTag(&arg->val))
			{
				case T_Integer:
					appendStringInfo(&buffer, "%ld", intVal(&arg->val));
					break;
				case T_Float:
					/* represented as a string, so just copy it */
					appendStringInfoString(&buffer, strVal(&arg->val));
					break;
				case T_String:
					val = strVal(&arg->val);

						/*
						 * Plain string literal or identifier., quote it
						 */

					if (val[0] != '\'')
						appendStringInfo(&buffer, "%s", quote_literal_internal(val));
					else
						appendStringInfo(&buffer, "%s",val);


					break;
				default:
					elog(ERROR, "unrecognized node type: %d",
						 (int) nodeTag(&arg->val));
					break;
			}
		}

		if (gp_dispatch)
		{
	        CdbDoCommandOnAllGangs( buffer.data, false, /*no txn*/ false );
		}
	}
}

/*
 * SET command wrapped as a SQL callable function.
 */
Datum
set_config_by_name(PG_FUNCTION_ARGS)
{
	char	   *name;
	char	   *value;
	char	   *new_value;
	bool		is_local;
	text	   *result_text;

	if (PG_ARGISNULL(0))
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
				 errmsg("SET requires parameter name")));

	/* Get the GUC variable name */
	name = DatumGetCString(DirectFunctionCall1(textout, PG_GETARG_DATUM(0)));

	/* Get the desired value or set to NULL for a reset request */
	if (PG_ARGISNULL(1))
		value = NULL;
	else
		value = DatumGetCString(DirectFunctionCall1(textout, PG_GETARG_DATUM(1)));

	/*
	 * Get the desired state of is_local. Default to false if provided value
	 * is NULL
	 */
	if (PG_ARGISNULL(2))
		is_local = false;
	else
		is_local = PG_GETARG_BOOL(2);

	/* Note SET DEFAULT (argstring == NULL) is equivalent to RESET */
	set_config_option(name,
					  value,
					  (superuser() ? PGC_SUSET : PGC_USERSET),
					  PGC_S_SESSION,
					  is_local,
					  true);

    if (Gp_role == GP_ROLE_DISPATCH && !IsBootstrapProcessingMode() &&
        strcmp(name, "gp_use_dispatch_agent") != 0)
    {
			StringInfoData buffer;
			initStringInfo(&buffer);
			appendStringInfo(&buffer, "SET ");
			if (is_local)
					appendStringInfo(&buffer, "LOCAL ");
			appendStringInfo(&buffer, "%s TO '%s'", name, value);
			CdbDoCommandOnAllGangs(buffer.data,
								   false /* cancelOnError */,
								   false /* no two phase commit */);
    }

	/* get the new current value */
	new_value = GetConfigOptionByName(name, NULL);

	/* Convert return string to text */
	result_text = DatumGetTextP(DirectFunctionCall1(textin, CStringGetDatum(new_value)));

	/* return it */
	PG_RETURN_TEXT_P(result_text);
}

static void
define_custom_variable(struct config_generic * variable)
{
	const char *name = variable->name;
	const char **nameAddr = &name;
	const char *value;
	struct config_string *pHolder;
	struct config_generic **res = (struct config_generic **) bsearch(
														  (void *) &nameAddr,
													  (void *) guc_variables,
														   num_guc_variables,
											 sizeof(struct config_generic *),
															guc_var_compare);

	if (res == NULL)
	{
		add_guc_variable(variable, ERROR);
		return;
	}

	/*
	 * This better be a placeholder
	 */
	if (((*res)->flags & GUC_CUSTOM_PLACEHOLDER) == 0)
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("attempt to redefine parameter \"%s\"", name)));

	Assert((*res)->vartype == PGC_STRING);
	pHolder = (struct config_string *) * res;

	/* We have the same name, no sorting is necessary */
	*res = variable;

	value = *pHolder->variable;

	/*
	 * Assign the string value stored in the placeholder to the real variable.
	 *
	 * XXX this is not really good enough --- it should be a nontransactional
	 * assignment, since we don't want it to roll back if the current xact
	 * fails later.
	 */
	set_config_option(name, value,
					  pHolder->gen.context, pHolder->gen.source,
					  false, true);

	/*
	 * Free up as much as we conveniently can of the placeholder structure
	 * (this neglects any stack items...)
	 */
	set_string_field(pHolder, pHolder->variable, NULL);
	set_string_field(pHolder, &pHolder->reset_val, NULL);
	set_string_field(pHolder, &pHolder->tentative_val, NULL);

	free(pHolder);
}

static void
init_custom_variable(struct config_generic * gen,
					 const char *name,
					 const char *short_desc,
					 const char *long_desc,
					 GucContext context,
					 enum config_type type)
{
	gen->name = guc_strdup(ERROR, name);
	gen->context = context;
	gen->group = CUSTOM_OPTIONS;
	gen->short_desc = short_desc;
	gen->long_desc = long_desc;
	gen->vartype = type;
}

void
DefineCustomBoolVariable(const char *name,
						 const char *short_desc,
						 const char *long_desc,
						 bool *valueAddr,
						 GucContext context,
						 GucBoolAssignHook assign_hook,
						 GucShowHook show_hook)
{
	size_t		sz = sizeof(struct config_bool);
	struct config_bool *var = (struct config_bool *) guc_malloc(ERROR, sz);

	memset(var, 0, sz);
	init_custom_variable(&var->gen, name, short_desc, long_desc, context, PGC_BOOL);

	var->variable = valueAddr;
	var->reset_val = *valueAddr;
	var->assign_hook = assign_hook;
	var->show_hook = show_hook;
	define_custom_variable(&var->gen);
}

void
DefineCustomIntVariable(const char *name,
						const char *short_desc,
						const char *long_desc,
						int *valueAddr,
						int minValue,
						int maxValue,
						GucContext context,
						GucIntAssignHook assign_hook,
						GucShowHook show_hook)
{
	size_t		sz = sizeof(struct config_int);
	struct config_int *var = (struct config_int *) guc_malloc(ERROR, sz);

	memset(var, 0, sz);
	init_custom_variable(&var->gen, name, short_desc, long_desc, context, PGC_INT);

	var->variable = valueAddr;
	var->reset_val = *valueAddr;
	var->min = minValue;
	var->max = maxValue;
	var->assign_hook = assign_hook;
	var->show_hook = show_hook;
	define_custom_variable(&var->gen);
}

void
DefineCustomRealVariable(const char *name,
						 const char *short_desc,
						 const char *long_desc,
						 double *valueAddr,
						 double minValue,
						 double maxValue,
						 GucContext context,
						 GucRealAssignHook assign_hook,
						 GucShowHook show_hook)
{
	size_t		sz = sizeof(struct config_real);
	struct config_real *var = (struct config_real *) guc_malloc(ERROR, sz);

	memset(var, 0, sz);
	init_custom_variable(&var->gen, name, short_desc, long_desc, context, PGC_REAL);

	var->variable = valueAddr;
	var->reset_val = *valueAddr;
	var->min = minValue;
	var->max = maxValue;
	var->assign_hook = assign_hook;
	var->show_hook = show_hook;
	define_custom_variable(&var->gen);
}

void
DefineCustomStringVariable(const char *name,
						   const char *short_desc,
						   const char *long_desc,
						   char **valueAddr,
						   GucContext context,
						   GucStringAssignHook assign_hook,
						   GucShowHook show_hook)
{
	size_t		sz = sizeof(struct config_string);
	struct config_string *var = (struct config_string *) guc_malloc(ERROR, sz);

	memset(var, 0, sz);
	init_custom_variable(&var->gen, name, short_desc, long_desc, context, PGC_STRING);

	var->variable = valueAddr;
	var->reset_val = *valueAddr;
	var->assign_hook = assign_hook;
	var->show_hook = show_hook;
	define_custom_variable(&var->gen);
}

void
EmitWarningsOnPlaceholders(const char *className)
{
	struct config_generic **vars = guc_variables;
	struct config_generic **last = vars + num_guc_variables;

	int			nameLen = strlen(className);

	while (vars < last)
	{
		struct config_generic *var = *vars++;

		if ((var->flags & GUC_CUSTOM_PLACEHOLDER) != 0 &&
			strncmp(className, var->name, nameLen) == 0 &&
			var->name[nameLen] == GUC_QUALIFIER_SEPARATOR)
		{
			if (Gp_role != GP_ROLE_EXECUTE)
			ereport(INFO,
					(errcode(ERRCODE_UNDEFINED_OBJECT),
					 errmsg("unrecognized configuration parameter \"%s\"", var->name)));
		}
	}
}


/*
 * SHOW command
 */
void
GetPGVariable(const char *name, DestReceiver *dest)
{
	if (pg_strcasecmp(name, "all") == 0)
		ShowAllGUCConfig(dest);
	else
		ShowGUCConfigOption(name, dest);
}

TupleDesc
GetPGVariableResultDesc(const char *name)
{
	TupleDesc	tupdesc;

	if (pg_strcasecmp(name, "all") == 0)
	{
		/* need a tuple descriptor representing three TEXT columns */
		tupdesc = CreateTemplateTupleDesc(3, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "name",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "setting",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "description",
						   TEXTOID, -1, 0);

	}
	else
	{
		const char *varname;

		/* Get the canonical spelling of name */
		(void) GetConfigOptionByName(name, &varname);

		/* need a tuple descriptor representing a single TEXT column */
		tupdesc = CreateTemplateTupleDesc(1, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, varname,
						   TEXTOID, -1, 0);
	}
	return tupdesc;
}

/*
 * RESET command
 */
void
ResetPGVariable(const char *name)
{
	if (pg_strcasecmp(name, "all") == 0)
		ResetAllOptions();
	else
		set_config_option(name,
						  NULL,
						  (superuser() ? PGC_SUSET : PGC_USERSET),
						  PGC_S_SESSION,
						  false,
						  true);
}


/*
 * SHOW command
 */
static void
ShowGUCConfigOption(const char *name, DestReceiver *dest)
{
	TupOutputState *tstate;
	TupleDesc	tupdesc;
	const char *varname;
	char	   *value;

	/* Get the value and canonical spelling of name */
	value = GetConfigOptionByName(name, &varname);

	/* need a tuple descriptor representing a single TEXT column */
	tupdesc = CreateTemplateTupleDesc(1, false);
	TupleDescInitEntry(tupdesc, (AttrNumber) 1, varname,
					   TEXTOID, -1, 0);

	/* prepare for projection of tuples */
	tstate = begin_tup_output_tupdesc(dest, tupdesc);

	/* Send it */
	do_text_output_oneline(tstate, value);

	end_tup_output(tstate);
}

/*
 * SHOW ALL command
 */
static void
ShowAllGUCConfig(DestReceiver *dest)
{
	bool		am_superuser = superuser();
	int			i;
	TupOutputState *tstate;
	TupleDesc	tupdesc;
	char	   *values[3];

	/* need a tuple descriptor representing three TEXT columns */
	tupdesc = CreateTemplateTupleDesc(3, false);
	TupleDescInitEntry(tupdesc, (AttrNumber) 1, "name",
					   TEXTOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 2, "setting",
					   TEXTOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 3, "description",
					   TEXTOID, -1, 0);


	/* prepare for projection of tuples */
	tstate = begin_tup_output_tupdesc(dest, tupdesc);

	for (i = 0; i < num_guc_variables; i++)
	{
		struct config_generic *conf = guc_variables[i];

		if ((conf->flags & GUC_NO_SHOW_ALL) ||
			((conf->flags & GUC_SUPERUSER_ONLY) && !am_superuser))
			continue;

		/* assign to the values array */
		values[0] = (char *) conf->name;
		values[1] = _ShowOption(conf, true);
		values[2] = (char *) conf->short_desc;

		/* send it to dest */
		do_tup_output(tstate, values);

		/* clean up */
		if (values[1] != NULL)
			pfree(values[1]);
	}

	end_tup_output(tstate);
}

/*
 * Return GUC variable value by name; optionally return canonical
 * form of name.  Return value is palloc'd.
 */
char *
GetConfigOptionByName(const char *name, const char **varname)
{
	struct config_generic *record;

	record = find_option(name, ERROR);
	if (record == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
			   errmsg("unrecognized configuration parameter \"%s\"", name)));
	if ((record->flags & GUC_SUPERUSER_ONLY) && !superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("must be superuser to examine \"%s\"", name)));

	if (varname)
		*varname = record->name;

	return _ShowOption(record, true);
}

/*
 * Return GUC variable value by variable number; optionally return canonical
 * form of name.  Return value is palloc'd.
 */
void
GetConfigOptionByNum(int varnum, const char **values, bool *noshow)
{
	char		buffer[256];
	struct config_generic *conf;

	/* check requested variable number valid */
	Assert((varnum >= 0) && (varnum < num_guc_variables));

	conf = guc_variables[varnum];

	if (noshow)
	{
		if ((conf->flags & GUC_NO_SHOW_ALL) ||
			((conf->flags & GUC_SUPERUSER_ONLY) && !superuser()))
			*noshow = true;
		else
			*noshow = false;
	}

	/* first get the generic attributes */

	/* name */
	values[0] = conf->name;

	/* setting : use _ShowOption in order to avoid duplicating the logic */
	values[1] = _ShowOption(conf, false);

	/* unit */
	if (conf->vartype == PGC_INT)
	{
		static char buf[8];

		switch (conf->flags & (GUC_UNIT_MEMORY | GUC_UNIT_TIME))
		{
			case GUC_UNIT_KB:
				values[2] = "kB";
				break;
			case GUC_UNIT_BLOCKS:
				snprintf(buf, sizeof(buf), "%dkB", BLCKSZ / 1024);
				values[2] = buf;
				break;
			case GUC_UNIT_XBLOCKS:
				snprintf(buf, sizeof(buf), "%dkB", XLOG_BLCKSZ / 1024);
				values[2] = buf;
				break;
			case GUC_UNIT_MS:
				values[2] = "ms";
				break;
			case GUC_UNIT_S:
				values[2] = "s";
				break;
			case GUC_UNIT_MIN:
				values[2] = "min";
				break;
			default:
				values[2] = "";
				break;
		}
	}
	else
		values[2] = NULL;

	/* group */
	values[3] = config_group_names[conf->group];

	/* short_desc */
	values[4] = conf->short_desc;

	/* extra_desc */
	values[5] = conf->long_desc;

	/* context */
	values[6] = GucContext_Names[conf->context];

	/* vartype */
	values[7] = config_type_names[conf->vartype];

	/* source */
	values[8] = GucSource_Names[conf->source];

	/* now get the type specifc attributes */
	switch (conf->vartype)
	{
		case PGC_BOOL:
			{
				/* min_val */
				values[9] = NULL;

				/* max_val */
				values[10] = NULL;
			}
			break;

		case PGC_INT:
			{
				struct config_int *lconf = (struct config_int *) conf;

				/* min_val */
				snprintf(buffer, sizeof(buffer), "%d", lconf->min);
				values[9] = pstrdup(buffer);

				/* max_val */
				snprintf(buffer, sizeof(buffer), "%d", lconf->max);
				values[10] = pstrdup(buffer);
			}
			break;

		case PGC_REAL:
			{
				struct config_real *lconf = (struct config_real *) conf;

				/* min_val */
				snprintf(buffer, sizeof(buffer), "%g", lconf->min);
				values[9] = pstrdup(buffer);

				/* max_val */
				snprintf(buffer, sizeof(buffer), "%g", lconf->max);
				values[10] = pstrdup(buffer);
			}
			break;

		case PGC_STRING:
			{
				/* min_val */
				values[9] = NULL;

				/* max_val */
				values[10] = NULL;
			}
			break;

		default:
			{
				/*
				 * should never get here, but in case we do, set 'em to NULL
				 */

				/* min_val */
				values[9] = NULL;

				/* max_val */
				values[10] = NULL;
			}
			break;
	}
}

/*
 * Return the total number of GUC variables
 */
int
GetNumConfigOptions(void)
{
	return num_guc_variables;
}

/*
 * show_config_by_name - equiv to SHOW X command but implemented as
 * a function.
 */
Datum
show_config_by_name(PG_FUNCTION_ARGS)
{
	char	   *varname;
	char	   *varval;
	text	   *result_text;

	/* Get the GUC variable name */
	varname = DatumGetCString(DirectFunctionCall1(textout, PG_GETARG_DATUM(0)));

	/* Get the value */
	varval = GetConfigOptionByName(varname, NULL);

	/* Convert to text */
	result_text = DatumGetTextP(DirectFunctionCall1(textin, CStringGetDatum(varval)));

	/* return it */
	PG_RETURN_TEXT_P(result_text);
}

/*
 * show_all_settings - equiv to SHOW ALL command but implemented as
 * a Table Function.
 */
#define NUM_PG_SETTINGS_ATTS	11

Datum
show_all_settings(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	TupleDesc	tupdesc;
	int			call_cntr;
	int			max_calls;
	AttInMetadata *attinmeta;
	MemoryContext oldcontext;

	/* stuff done only on the first call of the function */
	if (SRF_IS_FIRSTCALL())
	{
		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/*
		 * switch to memory context appropriate for multiple function calls
		 */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/*
		 * need a tuple descriptor representing NUM_PG_SETTINGS_ATTS columns
		 * of the appropriate types
		 */
		tupdesc = CreateTemplateTupleDesc(NUM_PG_SETTINGS_ATTS, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "name",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "setting",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "unit",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "category",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "short_desc",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 6, "extra_desc",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 7, "context",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 8, "vartype",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 9, "source",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 10, "min_val",
						   TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 11, "max_val",
						   TEXTOID, -1, 0);

		/*
		 * Generate attribute metadata needed later to produce tuples from raw
		 * C strings
		 */
		attinmeta = TupleDescGetAttInMetadata(tupdesc);
		funcctx->attinmeta = attinmeta;

		/* total number of tuples to be returned */
		funcctx->max_calls = GetNumConfigOptions();

		MemoryContextSwitchTo(oldcontext);
	}

	/* stuff done on every call of the function */
	funcctx = SRF_PERCALL_SETUP();

	call_cntr = funcctx->call_cntr;
	max_calls = funcctx->max_calls;
	attinmeta = funcctx->attinmeta;

	if (call_cntr < max_calls)	/* do when there is more left to send */
	{
		char	   *values[NUM_PG_SETTINGS_ATTS];
		bool		noshow;
		HeapTuple	tuple;
		Datum		result;

		/*
		 * Get the next visible GUC variable name and value
		 */
		do
		{
			GetConfigOptionByNum(call_cntr, (const char **) values, &noshow);
			if (noshow)
			{
				/* bump the counter and get the next config setting */
				call_cntr = ++funcctx->call_cntr;

				/* make sure we haven't gone too far now */
				if (call_cntr >= max_calls)
					SRF_RETURN_DONE(funcctx);
			}
		} while (noshow);

		/* build a tuple */
		tuple = BuildTupleFromCStrings(attinmeta, values);

		/* make the tuple into a datum */
		result = HeapTupleGetDatum(tuple);

		SRF_RETURN_NEXT(funcctx, result);
	}
	else
	{
		/* do when there is no more left */
		SRF_RETURN_DONE(funcctx);
	}
}

static char *
_ShowOption(struct config_generic * record, bool use_units)
{
	char		buffer[256];
	const char *val;

	switch (record->vartype)
	{
		case PGC_BOOL:
			{
				struct config_bool *conf = (struct config_bool *) record;

				if (conf->show_hook)
					val = (*conf->show_hook) ();
				else
					val = *conf->variable ? "on" : "off";
			}
			break;

		case PGC_INT:
			{
				struct config_int *conf = (struct config_int *) record;

				if (conf->show_hook)
					val = (*conf->show_hook) ();
				else
				{
					/*
					 * Use int64 arithmetic to avoid overflows in units
					 * conversion.  If INT64_IS_BUSTED we might overflow
					 * anyway and print bogus answers, but there are few
					 * enough such machines that it doesn't seem worth
					 * trying harder.
					 */
					int64		result = *conf->variable;
					const char *unit;

					if (use_units && result > 0 &&
						(record->flags & GUC_UNIT_MEMORY))
					{
						switch (record->flags & GUC_UNIT_MEMORY)
						{
							case GUC_UNIT_BLOCKS:
								result *= BLCKSZ / 1024;
								break;
							case GUC_UNIT_XBLOCKS:
								result *= XLOG_BLCKSZ / 1024;
								break;
						}

						if (result % KB_PER_GB == 0)
						{
							result /= KB_PER_GB;
							unit = "GB";
						}
						else if (result % KB_PER_MB == 0)
						{
							result /= KB_PER_MB;
							unit = "MB";
						}
						else
						{
							unit = "kB";
						}
					}
					else if (use_units && result > 0 &&
							 (record->flags & GUC_UNIT_TIME))
					{
						switch (record->flags & GUC_UNIT_TIME)
						{
							case GUC_UNIT_S:
								result *= MS_PER_S;
								break;
							case GUC_UNIT_MIN:
								result *= MS_PER_MIN;
								break;
						}

						if (result % MS_PER_D == 0)
						{
							result /= MS_PER_D;
							unit = "d";
						}
						else if (result % MS_PER_H == 0)
						{
							result /= MS_PER_H;
							unit = "h";
						}
						else if (result % MS_PER_MIN == 0)
						{
							result /= MS_PER_MIN;
							unit = "min";
						}
						else if (result % MS_PER_S == 0)
						{
							result /= MS_PER_S;
							unit = "s";
						}
						else
						{
							unit = "ms";
						}
					}
					else
						unit = "";

					snprintf(buffer, sizeof(buffer), INT64_FORMAT "%s",
							 result, unit);
					val = buffer;
				}
			}
			break;

		case PGC_REAL:
			{
				struct config_real *conf = (struct config_real *) record;

				if (conf->show_hook)
					val = (*conf->show_hook) ();
				else
				{
					char		unit[4];
					double		result = *conf->variable;

					if (use_units && result > 0 && (record->flags & GUC_UNIT_MEMORY))
					{
                        double result_gb;
                        double result_mb;

                        switch (record->flags & GUC_UNIT_MEMORY)
						{
							case GUC_UNIT_BLOCKS:
								result *= BLCKSZ / 1024;
								break;
							case GUC_UNIT_XBLOCKS:
								result *= XLOG_BLCKSZ / 1024;
								break;
						}

                        result_gb = result / KB_PER_GB;
                        result_mb = result / KB_PER_MB;

						if (result_gb >= 1.0 &&
                            result_gb * KB_PER_GB == result)
						{
							result = result_gb;
							strcpy(unit, "GB");
						}
						else if (result_mb >= 1.0 &&
                                 result_mb * KB_PER_MB == result)
						{
							result = result_mb;
							strcpy(unit, "MB");
						}
						else
							strcpy(unit, "kB");
					}
					else
						strcpy(unit, "");

					snprintf(buffer, sizeof(buffer), "%g%s", result, unit);
					val = buffer;
				}
			}
			break;

		case PGC_STRING:
			{
				struct config_string *conf = (struct config_string *) record;

				if (conf->show_hook)
					val = (*conf->show_hook) ();
				else if (*conf->variable && **conf->variable)
					val = *conf->variable;
				else
					val = "";
			}
			break;

		default:
			/* just to keep compiler quiet */
			val = "???";
			break;
	}

	return pstrdup(val);
}


/*
 * gp_guc_list_init
 *
 * Builds global lists of interesting GUCs for use with gp_guc_list_show()...
 *
 * - gp_guc_list_for_explain: consists of planner GUCs, plus 'work_mem'
 * - gp_guc_list_for_no_plan: planner method enables for cdb_no_plan_for_query().
 */
void
gp_guc_list_init(void)
{
    int         i;

    if (gp_guc_list_for_explain)
    {
        list_free(gp_guc_list_for_explain);
        gp_guc_list_for_explain = NIL;
    }
    if (gp_guc_list_for_no_plan)
    {
        list_free(gp_guc_list_for_no_plan);
        gp_guc_list_for_no_plan = NIL;
    }

   	for (i = 0; i < num_guc_variables; i++)
	{
		struct config_generic  *gconf = guc_variables[i];
        bool    explain = false;
        bool    no_plan = false;

        switch (gconf->group)
        {
            case QUERY_TUNING:
            case QUERY_TUNING_COST:
            case QUERY_TUNING_OTHER:
                explain = true;
                break;

            case QUERY_TUNING_METHOD:
                explain = true;
                no_plan = true;
                break;

            case RESOURCES_MEM:
                if (0 == guc_name_compare(gconf->name, "work_mem"))
                    explain = true;
                break;

            default:
                break;
        }

        if (explain)
            gp_guc_list_for_explain = lappend(gp_guc_list_for_explain, gconf);
        if (no_plan)
            gp_guc_list_for_no_plan = lappend(gp_guc_list_for_no_plan, gconf);
	}
}                               /* gp_guc_list_init */


/*
 * gp_guc_list_show
 *
 * Given a list of GUCs (a List of struct config_generic), appends the
 * option names and values to caller's buffer, skipping any whose
 * source <= 'excluding'.  The 'pfx' string is inserted at the
 * beginning.  The 'fmt' string must contain two occurrences of "%s",
 * which are expanded to the GUC name and value, respectively.
 *
 * Returns the length of the string appended to the buffer.
 */
int
gp_guc_list_show(struct StringInfoData    *buf,
                  const char               *pfx,
                  const char               *fmt,
                  GucSource                 excluding,
                  List                     *guclist)
{
    int         oldlen = buf->len;
    ListCell   *cell;
    char       *value;
    struct config_generic  *gconf;

   	foreach(cell, guclist)
	{
		gconf = (struct config_generic *)lfirst(cell);
		if (gconf->source > excluding)
        {
            if (pfx)
            {
                appendStringInfoString(buf, pfx);
                pfx = NULL;
            }
            value = _ShowOption(gconf, true);
            appendStringInfo(buf, fmt, gconf->name, value);
            pfree(value);
        }
	}
    return buf->len - oldlen;
}                               /* gp_guc_list_show */


static bool
is_newvalue_equal(struct config_generic * record, const char *newvalue)
{
	switch (record->vartype)
	{
		case PGC_BOOL:
			{
				struct config_bool *conf = (struct config_bool *) record;
				bool		newval;

				return parse_bool(newvalue, &newval) &&
					*conf->variable == newval;
			}
		case PGC_INT:
			{
				struct config_int *conf = (struct config_int *) record;
				int			newval;

				return parse_int(newvalue, &newval, record->flags, NULL)
					&& *conf->variable == newval;
			}
		case PGC_REAL:
			{
				struct config_real *conf = (struct config_real *) record;
				double		newval;

				return parse_real(newvalue, &newval, record->flags)
					&& *conf->variable == newval;
			}
		case PGC_STRING:
			{
				struct config_string *conf = (struct config_string *) record;

				return strcmp(*conf->variable, newvalue) == 0;
			}
	}

	return false;
}


#ifdef EXEC_BACKEND

/*
 *	This routine dumps out all non-default GUC options into a binary
 *	file that is read by all exec'ed backends.  The format is:
 *
 *		variable name, string, null terminated
 *		variable value, string, null terminated
 *		variable source, integer
 */
void
write_nondefault_variables(GucContext context)
{
	int			i;
	int			elevel;
	FILE	   *fp;

	Assert(context == PGC_POSTMASTER || context == PGC_SIGHUP);

	elevel = (context == PGC_SIGHUP) ? LOG : ERROR;

	/*
	 * Open file
	 */
	fp = AllocateFile(CONFIG_EXEC_PARAMS_NEW, "w");
	if (!fp)
	{
		ereport(elevel,
				(errcode_for_file_access(),
				 errmsg("could not write to file \"%s\": %m",
						CONFIG_EXEC_PARAMS_NEW)));
		return;
	}

	for (i = 0; i < num_guc_variables; i++)
	{
		struct config_generic *gconf = guc_variables[i];

		if (gconf->source != PGC_S_DEFAULT)
		{
			fprintf(fp, "%s", gconf->name);
			fputc(0, fp);

			switch (gconf->vartype)
			{
				case PGC_BOOL:
					{
						struct config_bool *conf = (struct config_bool *) gconf;

						if (*conf->variable == 0)
							fprintf(fp, "false");
						else
							fprintf(fp, "true");
					}
					break;

				case PGC_INT:
					{
						struct config_int *conf = (struct config_int *) gconf;

						fprintf(fp, "%d", *conf->variable);
					}
					break;

				case PGC_REAL:
					{
						struct config_real *conf = (struct config_real *) gconf;

						/* Could lose precision here? */
						fprintf(fp, "%f", *conf->variable);
					}
					break;

				case PGC_STRING:
					{
						struct config_string *conf = (struct config_string *) gconf;

						fprintf(fp, "%s", *conf->variable);
					}
					break;
			}

			fputc(0, fp);

			fwrite(&gconf->source, sizeof(gconf->source), 1, fp);
		}
	}

	if (FreeFile(fp))
	{
		ereport(elevel,
				(errcode_for_file_access(),
				 errmsg("could not write to file \"%s\": %m",
						CONFIG_EXEC_PARAMS_NEW)));
		return;
	}

	/*
	 * Put new file in place.  This could delay on Win32, but we don't hold
	 * any exclusive locks.
	 */
	rename(CONFIG_EXEC_PARAMS_NEW, CONFIG_EXEC_PARAMS);
}


/*
 *	Read string, including null byte from file
 *
 *	Return NULL on EOF and nothing read
 */
static char *
read_string_with_null(FILE *fp)
{
	int			i = 0,
				ch,
				maxlen = 256;
	char	   *str = NULL;

	do
	{
		if ((ch = fgetc(fp)) == EOF)
		{
			if (i == 0)
				return NULL;
			else
				elog(FATAL, "invalid format of exec config params file");
		}
		if (i == 0)
			str = guc_malloc(FATAL, maxlen);
		else if (i == maxlen)
			str = guc_realloc(FATAL, str, maxlen *= 2);
		str[i++] = ch;
	} while (ch != 0);

	return str;
}


/*
 *	This routine loads a previous postmaster dump of its non-default
 *	settings.
 */
void
read_nondefault_variables(void)
{
	FILE	   *fp;
	char	   *varname,
			   *varvalue;
	int			varsource;

	/*
	 * Open file
	 */
	fp = AllocateFile(CONFIG_EXEC_PARAMS, "r");
	if (!fp)
	{
		/* File not found is fine */
		if (errno != ENOENT)
			ereport(FATAL,
					(errcode_for_file_access(),
					 errmsg("could not read from file \"%s\": %m",
							CONFIG_EXEC_PARAMS)));
		return;
	}

	for (;;)
	{
		struct config_generic *record;

		if ((varname = read_string_with_null(fp)) == NULL)
			break;

		if ((record = find_option(varname, FATAL)) == NULL)
			elog(FATAL, "failed to locate variable %s in exec config params file", varname);
		if ((varvalue = read_string_with_null(fp)) == NULL)
			elog(FATAL, "invalid format of exec config params file");
		if (fread(&varsource, sizeof(varsource), 1, fp) == 0)
			elog(FATAL, "invalid format of exec config params file");

		(void) set_config_option(varname, varvalue, record->context,
								 varsource, false, true);
		free(varname);
		free(varvalue);
	}

	FreeFile(fp);
}
#endif   /* EXEC_BACKEND */


/*
 * A little "long argument" simulation, although not quite GNU
 * compliant. Takes a string of the form "some-option=some value" and
 * returns name = "some_option" and value = "some value" in malloc'ed
 * storage. Note that '-' is converted to '_' in the option name. If
 * there is no '=' in the input string then value will be NULL.
 */
void
ParseLongOption(const char *string, char **name, char **value)
{
	size_t		equal_pos;
	char	   *cp;

	AssertArg(string);
	AssertArg(name);
	AssertArg(value);

	equal_pos = strcspn(string, "=");

	if (string[equal_pos] == '=')
	{
		*name = guc_malloc(FATAL, equal_pos + 1);
		strncpy(*name, string, equal_pos);
		(*name)[equal_pos] = '\0';

		*value = guc_strdup(FATAL, &string[equal_pos + 1]);
	}
	else
	{
		/* no equal sign in string */
		*name = guc_strdup(FATAL, string);
		*value = NULL;
	}

	for (cp = *name; *cp; cp++)
		if (*cp == '-')
			*cp = '_';
}

/*
 * Handle options fetched from pg_database.datconfig or pg_authid.rolconfig.
 * The array parameter must be an array of TEXT (it must not be NULL).
 */
void
ProcessGUCArray(ArrayType *array, GucSource source)
{
	int			i;

	Assert(array != NULL);
	Assert(ARR_ELEMTYPE(array) == TEXTOID);
	Assert(ARR_NDIM(array) == 1);
	Assert(ARR_LBOUND(array)[0] == 1);
	Assert(source == PGC_S_DATABASE || source == PGC_S_USER);

	for (i = 1; i <= ARR_DIMS(array)[0]; i++)
	{
		Datum		d;
		bool		isnull;
		char	   *s;
		char	   *name;
		char	   *value;

		d = array_ref(array, 1, &i,
					  -1 /* varlenarray */ ,
					  -1 /* TEXT's typlen */ ,
					  false /* TEXT's typbyval */ ,
					  'i' /* TEXT's typalign */ ,
					  &isnull);

		if (isnull)
			continue;

		s = DatumGetCString(DirectFunctionCall1(textout, d));

		ParseLongOption(s, &name, &value);
		if (!value)
		{
			ereport(WARNING,
					(errcode(ERRCODE_SYNTAX_ERROR),
			  errmsg("could not parse setting for parameter \"%s\"", name)));
			free(name);
			continue;
		}

		/*
		 * We process all these options at SUSET level.  We assume that the
		 * right to insert an option into pg_database or pg_authid was checked
		 * when it was inserted.
		 */
		SetConfigOption(name, value, PGC_SUSET, source);

		free(name);
		if (value)
			free(value);
		pfree(s);
	}
}


/*
 * Add an entry to an option array.  The array parameter may be NULL
 * to indicate the current table entry is NULL.
 */
ArrayType *
GUCArrayAdd(ArrayType *array, const char *name, const char *value)
{
	const char *varname;
	Datum		datum;
	char	   *newval;
	ArrayType  *a;

	Assert(name);
	Assert(value);

	/* test if the option is valid */
	set_config_option(name, value,
					  superuser() ? PGC_SUSET : PGC_USERSET,
					  PGC_S_TEST, false, false);

	/* convert name to canonical spelling, so we can use plain strcmp */
	(void) GetConfigOptionByName(name, &varname);
	name = varname;

	newval = palloc(strlen(name) + 1 + strlen(value) + 1);
	sprintf(newval, "%s=%s", name, value);
	datum = DirectFunctionCall1(textin, CStringGetDatum(newval));

	if (array)
	{
		int			index;
		bool		isnull;
		int			i;

		Assert(ARR_ELEMTYPE(array) == TEXTOID);
		Assert(ARR_NDIM(array) == 1);
		Assert(ARR_LBOUND(array)[0] == 1);

		index = ARR_DIMS(array)[0] + 1; /* add after end */

		for (i = 1; i <= ARR_DIMS(array)[0]; i++)
		{
			Datum		d;
			char	   *current;

			d = array_ref(array, 1, &i,
						  -1 /* varlenarray */ ,
						  -1 /* TEXT's typlen */ ,
						  false /* TEXT's typbyval */ ,
						  'i' /* TEXT's typalign */ ,
						  &isnull);
			if (isnull)
				continue;
			current = DatumGetCString(DirectFunctionCall1(textout, d));
			if (strncmp(current, newval, strlen(name) + 1) == 0)
			{
				index = i;
				break;
			}
		}

		a = array_set(array, 1, &index,
					  datum,
					  false,
					  -1 /* varlena array */ ,
					  -1 /* TEXT's typlen */ ,
					  false /* TEXT's typbyval */ ,
					  'i' /* TEXT's typalign */ );
	}
	else
		a = construct_array(&datum, 1,
							TEXTOID,
							-1, false, 'i');

	return a;
}


/*
 * Delete an entry from an option array.  The array parameter may be NULL
 * to indicate the current table entry is NULL.  Also, if the return value
 * is NULL then a null should be stored.
 */
ArrayType *
GUCArrayDelete(ArrayType *array, const char *name)
{
	const char *varname;
	ArrayType  *newarray;
	int			i;
	int			index;

	Assert(name);

	/* test if the option is valid */
	set_config_option(name, NULL,
					  superuser() ? PGC_SUSET : PGC_USERSET,
					  PGC_S_TEST, false, false);

	/* convert name to canonical spelling, so we can use plain strcmp */
	(void) GetConfigOptionByName(name, &varname);
	name = varname;

	/* if array is currently null, then surely nothing to delete */
	if (!array)
		return NULL;

	newarray = NULL;
	index = 1;

	for (i = 1; i <= ARR_DIMS(array)[0]; i++)
	{
		Datum		d;
		char	   *val;
		bool		isnull;

		d = array_ref(array, 1, &i,
					  -1 /* varlenarray */ ,
					  -1 /* TEXT's typlen */ ,
					  false /* TEXT's typbyval */ ,
					  'i' /* TEXT's typalign */ ,
					  &isnull);
		if (isnull)
			continue;
		val = DatumGetCString(DirectFunctionCall1(textout, d));

		/* ignore entry if it's what we want to delete */
		if (strncmp(val, name, strlen(name)) == 0
			&& val[strlen(name)] == '=')
			continue;

		/* else add it to the output array */
		if (newarray)
		{
			newarray = array_set(newarray, 1, &index,
								 d,
								 false,
								 -1 /* varlenarray */ ,
								 -1 /* TEXT's typlen */ ,
								 false /* TEXT's typbyval */ ,
								 'i' /* TEXT's typalign */ );
		}
		else
			newarray = construct_array(&d, 1,
									   TEXTOID,
									   -1, false, 'i');

		index++;
	}

	return newarray;
}

/*
 * Given a GUC array, delete all settings from it that our permission
 * level allows: if superuser, delete them all; if regular user, only
 * those that are PGC_USERSET
 */
ArrayType *
GUCArrayReset(ArrayType *array)
{
	ArrayType  *newarray;
	int			i;
	int			index;

	/* if array is currently null, nothing to do */
	if (!array)
		return NULL;

	/* if we're superuser, we can delete everything */
	if (superuser())
		return NULL;

	newarray = NULL;
	index = 1;

	for (i = 1; i <= ARR_DIMS(array)[0]; i++)
	{
		Datum		d;
		char	   *val;
		char	   *eqsgn;
		bool		isnull;
		struct config_generic *gconf;

		d = array_ref(array, 1, &i,
					  -1 /* varlenarray */ ,
					  -1 /* TEXT's typlen */ ,
					  false /* TEXT's typbyval */ ,
					  'i' /* TEXT's typalign */ ,
					  &isnull);

		if (isnull)
			continue;
		val = DatumGetCString(DirectFunctionCall1(textout, d));

		eqsgn = strchr(val, '=');
		*eqsgn = '\0';

		gconf = find_option(val, WARNING);
		if (!gconf)
			continue;

		/* note: superuser-ness was already checked above */
		/* skip entry if OK to delete */
		if (gconf->context == PGC_USERSET)
			continue;

		/* XXX do we need to worry about database owner? */

		/* else add it to the output array */
		if (newarray)
		{
			newarray = array_set(newarray, 1, &index,
								 d,
								 false,
								 -1 /* varlenarray */ ,
								 -1 /* TEXT's typlen */ ,
								 false /* TEXT's typbyval */ ,
								 'i' /* TEXT's typalign */ );
		}
		else
			newarray = construct_array(&d, 1,
									   TEXTOID,
									   -1, false, 'i');

		index++;
		pfree(val);
	}

	return newarray;
}


/*
 * assign_hook subroutines
 */

static const char *
assign_log_destination(const char *value, bool doit, GucSource source)
{
	char	   *rawstring;
	List	   *elemlist;
	ListCell   *l;
	int			newlogdest = 0;

	/* Need a modifiable copy of string */
	rawstring = pstrdup(value);

	/* Parse string into list of identifiers */
	if (!SplitIdentifierString(rawstring, ',', &elemlist))
	{
		/* syntax error in list */
		pfree(rawstring);
		list_free(elemlist);
		if (source >= PGC_S_INTERACTIVE)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			errmsg("invalid list syntax for parameter \"log_destination\"")));
		return NULL;
	}

	foreach(l, elemlist)
	{
		char	   *tok = (char *) lfirst(l);

		if (pg_strcasecmp(tok, "stderr") == 0)
			newlogdest |= LOG_DESTINATION_STDERR;
#ifdef HAVE_SYSLOG
		else if (pg_strcasecmp(tok, "syslog") == 0)
			newlogdest |= LOG_DESTINATION_SYSLOG;
#endif
#ifdef WIN32
		else if (pg_strcasecmp(tok, "eventlog") == 0)
			newlogdest |= LOG_DESTINATION_EVENTLOG;
#endif
		else
		{
			if (source >= PGC_S_INTERACTIVE)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				  errmsg("unrecognized \"log_destination\" key word: \"%s\"",
						 tok)));
			pfree(rawstring);
			list_free(elemlist);
			return NULL;
		}
	}

	if (doit)
		Log_destination = newlogdest;

	pfree(rawstring);
	list_free(elemlist);

	return value;
}

static const char *
assign_gp_log_format(const char *value, bool doit, GucSource source)
{
	int log_format = 0;

	if (pg_strcasecmp(value, "text") == 0)
		log_format = 0;
	else if (pg_strcasecmp(value, "csv") == 0)
		log_format = 1;
	else
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("unrecognized \"gp_log_format\" key word: \"%s\"",
						value)));
	if (doit)
		gp_log_format = log_format;

	return value;
}

#ifdef HAVE_SYSLOG

static const char *
assign_syslog_facility(const char *facility, bool doit, GucSource source)
{
	int			syslog_fac;

	if (pg_strcasecmp(facility, "LOCAL0") == 0)
		syslog_fac = LOG_LOCAL0;
	else if (pg_strcasecmp(facility, "LOCAL1") == 0)
		syslog_fac = LOG_LOCAL1;
	else if (pg_strcasecmp(facility, "LOCAL2") == 0)
		syslog_fac = LOG_LOCAL2;
	else if (pg_strcasecmp(facility, "LOCAL3") == 0)
		syslog_fac = LOG_LOCAL3;
	else if (pg_strcasecmp(facility, "LOCAL4") == 0)
		syslog_fac = LOG_LOCAL4;
	else if (pg_strcasecmp(facility, "LOCAL5") == 0)
		syslog_fac = LOG_LOCAL5;
	else if (pg_strcasecmp(facility, "LOCAL6") == 0)
		syslog_fac = LOG_LOCAL6;
	else if (pg_strcasecmp(facility, "LOCAL7") == 0)
		syslog_fac = LOG_LOCAL7;
	else
		return NULL;			/* reject */

	if (doit)
	{
		syslog_facility = syslog_fac;
		set_syslog_parameters(syslog_ident_str ? syslog_ident_str : "postgres",
							  syslog_facility);
	}

	return facility;
}

static const char *
assign_syslog_ident(const char *ident, bool doit, GucSource source)
{
	if (doit)
		set_syslog_parameters(ident, syslog_facility);

	return ident;
}
#endif   /* HAVE_SYSLOG */


static const char *
assign_defaultxactisolevel(const char *newval, bool doit, GucSource source)
{
	if (pg_strcasecmp(newval, "serializable") == 0)
	{
		if (doit)
			DefaultXactIsoLevel = XACT_SERIALIZABLE;
	}
	else if (pg_strcasecmp(newval, "repeatable read") == 0)
	{
		if (doit)
			DefaultXactIsoLevel = XACT_REPEATABLE_READ;
	}
	else if (pg_strcasecmp(newval, "read committed") == 0)
	{
		if (doit)
			DefaultXactIsoLevel = XACT_READ_COMMITTED;
	}
	else if (pg_strcasecmp(newval, "read uncommitted") == 0)
	{
		if (doit)
			DefaultXactIsoLevel = XACT_READ_UNCOMMITTED;
	}
	else
		return NULL;
	return newval;
}

static const char *
assign_debug_persistent_print_level(const char *newval,
						bool doit, GucSource source)
{
	return (assign_msglvl(&Debug_persistent_print_level, newval, doit, source));
}

static const char *
assign_debug_persistent_recovery_print_level(const char *newval,
						bool doit, GucSource source)
{
	return (assign_msglvl(&Debug_persistent_recovery_print_level, newval, doit, source));
}

static const char *
assign_debug_persistent_store_print_level(const char *newval,
						bool doit, GucSource source)
{
	return (assign_msglvl(&Debug_persistent_store_print_level, newval, doit, source));
}

static const char *
assign_debug_database_command_print_level(const char *newval,
						bool doit, GucSource source)
{
	return (assign_msglvl(&Debug_database_command_print_level, newval, doit, source));
}

static const char *
assign_log_min_messages(const char *newval,
						bool doit, GucSource source)
{
	return (assign_msglvl(&log_min_messages, newval, doit, source));
}

static const char *
assign_client_min_messages(const char *newval, bool doit, GucSource source)
{
	return (assign_msglvl(&client_min_messages, newval, doit, source));
}

static const char *
assign_gp_workfile_caching_loglevel(const char *newval,
						bool doit, GucSource source)
{
	return (assign_msglvl(&gp_workfile_caching_loglevel, newval, doit, source));
}

static const char *
assign_gp_sessionstate_loglevel(const char *newval,
						bool doit, GucSource source)
{
	return (assign_msglvl(&gp_sessionstate_loglevel, newval, doit, source));
}

static const char *
assign_optimizer_log_failure(const char *val, bool assign, GucSource source)
{
	if (pg_strcasecmp(val, "all") == 0 && assign)
	{
		optimizer_log_failure = OPTIMIZER_ALL_FAIL;
	}
	else if (pg_strcasecmp(val, "unexpected") == 0 && assign)
	{
		optimizer_log_failure = OPTIMIZER_UNEXPECTED_FAIL;
	}
	else if (pg_strcasecmp(val, "expected") == 0 && assign)
	{
		optimizer_log_failure = OPTIMIZER_EXPECTED_FAIL;
	}
	else
	{
		return NULL; /* fail */
	}

	return val;
}


static const char *
assign_optimizer_minidump(const char *val, bool assign, GucSource source)
{
	if (pg_strcasecmp(val, "onerror") == 0 && assign)
	{
		optimizer_minidump = OPTIMIZER_MINIDUMP_FAIL;
	}
	else if (pg_strcasecmp(val, "always") == 0 && assign)
	{
		optimizer_minidump = OPTIMIZER_MINIDUMP_ALWAYS;
	}
	else
	{
		return NULL; /* fail */
	}

	return val;
}

static const char *
assign_optimizer_cost_model(const char *val, bool assign, GucSource source)
{
        if (pg_strcasecmp(val, "legacy") == 0 && assign)
        {
                optimizer_cost_model = OPTIMIZER_GPDB_LEGACY;
        }
        else if (pg_strcasecmp(val, "calibrated") == 0 && assign)
        {
                optimizer_cost_model = OPTIMIZER_GPDB_CALIBRATED;
        }
        else
        {
                return NULL; /* fail */
        }
        return val;
}


static const char *
assign_min_error_statement(const char *newval, bool doit, GucSource source)
{
	return (assign_msglvl(&log_min_error_statement, newval, doit, source));
}

static const char *
assign_time_slice_report_level(const char *newval, bool doit, GucSource source)
{
	return (assign_msglvl(&gp_test_time_slice_report_level, newval, doit, source));
}

static const char *
assign_deadlock_hazard_report_level(const char *newval, bool doit, GucSource source)
{
	return (assign_msglvl(&gp_test_deadlock_hazard_report_level, newval, doit, source));
}

static const char *
assign_msglvl(int *var, const char *newval, bool doit, GucSource source)
{
	if (pg_strcasecmp(newval, "debug") == 0)
	{
		if (doit)
			(*var) = DEBUG2;
	}
	else if (pg_strcasecmp(newval, "debug5") == 0)
	{
		if (doit)
			(*var) = DEBUG5;
	}
	else if (pg_strcasecmp(newval, "debug4") == 0)
	{
		if (doit)
			(*var) = DEBUG4;
	}
	else if (pg_strcasecmp(newval, "debug3") == 0)
	{
		if (doit)
			(*var) = DEBUG3;
	}
	else if (pg_strcasecmp(newval, "debug2") == 0)
	{
		if (doit)
			(*var) = DEBUG2;
	}
	else if (pg_strcasecmp(newval, "debug1") == 0)
	{
		if (doit)
			(*var) = DEBUG1;
	}
	else if (pg_strcasecmp(newval, "log") == 0)
	{
		if (doit)
			(*var) = LOG;
	}

	/*
	 * Client_min_messages always prints 'info', but we allow it as a value
	 * anyway.
	 */
	else if (pg_strcasecmp(newval, "info") == 0)
	{
		if (doit)
			(*var) = INFO;
	}
	else if (pg_strcasecmp(newval, "notice") == 0)
	{
		if (doit)
			(*var) = NOTICE;
	}
	else if (pg_strcasecmp(newval, "warning") == 0)
	{
		if (doit)
			(*var) = WARNING;
	}
	else if (pg_strcasecmp(newval, "error") == 0)
	{
		if (doit)
			(*var) = ERROR;
	}
	/* We allow FATAL/PANIC for client-side messages too. */
	else if (pg_strcasecmp(newval, "fatal") == 0)
	{
		if (doit)
			(*var) = FATAL;
	}
	else if (pg_strcasecmp(newval, "panic") == 0)
	{
		if (doit)
			(*var) = PANIC;
	}
	else
		return NULL;			/* fail */
	return newval;				/* OK */
}

static const char *
assign_explain_memory_verbosity(const char *newval, bool doit, GucSource source)
{
	if (pg_strcasecmp(newval, "suppress") == 0)
	{
		if (doit)
			explain_memory_verbosity = EXPLAIN_MEMORY_VERBOSITY_SUPPRESS;
	}
	else if (pg_strcasecmp(newval, "summary") == 0)
	{
		if (doit)
			explain_memory_verbosity = EXPLAIN_MEMORY_VERBOSITY_SUMMARY;
	}
	else if (pg_strcasecmp(newval, "detail") == 0)
	{
		if (doit)
			explain_memory_verbosity = EXPLAIN_MEMORY_VERBOSITY_DETAIL;
	}
	else
	{
		printf("Unknown memory verbosity.");
		return NULL;
	}

	return newval;
}

static const char *
assign_system_cache_flush_force(const char *newval, bool doit, GucSource source)
{
	if (pg_strcasecmp(newval, "off") == 0)
	{
		if (doit)
			gp_test_system_cache_flush_force = SysCacheFlushForce_Off;
	}
	else if (pg_strcasecmp(newval, "recursive") == 0)
	{
		if (doit)
			gp_test_system_cache_flush_force = SysCacheFlushForce_Recursive;
	}
	else if (pg_strcasecmp(newval, "plain") == 0)
	{
		if (doit)
			gp_test_system_cache_flush_force = SysCacheFlushForce_NonRecursive;
	}
	else
	{
		return NULL;
	}

	return newval;
}

static const char *
assign_password_hash_algorithm(const char *newval, bool doit, GucSource source)
{
	if (pg_strcasecmp(newval, "MD5") == 0)
	{
		if (doit)
			password_hash_algorithm = PASSWORD_HASH_MD5;
	}
	else if (pg_strcasecmp(newval, "SHA-256") == 0)
	{
		if (doit)
			password_hash_algorithm = PASSWORD_HASH_SHA_256;
	}
	else
		return NULL;

	return newval;
}

static const char *
assign_gp_idf_deduplicate(const char *newval, bool doit, GucSource source)
{
	if (pg_strcasecmp(newval, "auto") == 0 ||
		pg_strcasecmp(newval, "none") == 0 ||
		pg_strcasecmp(newval, "force") == 0)
	{
		return newval;
	}
	return NULL;
}

static const char *
assign_IntervalStyle(const char *newval, bool doit, GucSource source)
{
	if (pg_strcasecmp(newval, "postgres")==0)
	{
		if (doit)
			IntervalStyle = INTSTYLE_POSTGRES;
	}
	else if (pg_strcasecmp(newval, "postgres_verbose")==0)
	{
		if (doit)
			IntervalStyle = INTSTYLE_POSTGRES_VERBOSE;
	}
	else if (pg_strcasecmp(newval, "sql_standard")==0)
	{
		if (doit)
			IntervalStyle = INTSTYLE_SQL_STANDARD;
	}
	else if (pg_strcasecmp(newval, "ISO_8601")==0)
	{
		if (doit)
			IntervalStyle = INTSTYLE_ISO_8601;
	}
	else
		return NULL;
	return newval;

}

static const char *
show_IntervalStyle(void)
{
	switch(IntervalStyle)
	{
	case INTSTYLE_POSTGRES:  return "postgres";
	case INTSTYLE_POSTGRES_VERBOSE:  return "postgres_verbose";
	case INTSTYLE_SQL_STANDARD:  return "sql_standard";
	case INTSTYLE_ISO_8601:  return "ISO_8601";
	};
	return NULL;

}

static const char *
assign_log_error_verbosity(const char *newval, bool doit, GucSource source)
{
	if (pg_strcasecmp(newval, "terse") == 0)
	{
		if (doit)
			Log_error_verbosity = PGERROR_TERSE;
	}
	else if (pg_strcasecmp(newval, "default") == 0)
	{
		if (doit)
			Log_error_verbosity = PGERROR_DEFAULT;
	}
	else if (pg_strcasecmp(newval, "verbose") == 0)
	{
		if (doit)
			Log_error_verbosity = PGERROR_VERBOSE;
	}
	else
		return NULL;			/* fail */
	return newval;				/* OK */
}

static const char *
assign_log_statement(const char *newval, bool doit, GucSource source)
{
	if (pg_strcasecmp(newval, "none") == 0)
	{
		if (doit)
			log_statement = LOGSTMT_NONE;
	}
	else if (pg_strcasecmp(newval, "ddl") == 0)
	{
		if (doit)
			log_statement = LOGSTMT_DDL;
	}
	else if (pg_strcasecmp(newval, "mod") == 0)
	{
		if (doit)
			log_statement = LOGSTMT_MOD;
	}
	else if (pg_strcasecmp(newval, "all") == 0)
	{
		if (doit)
			log_statement = LOGSTMT_ALL;
	}
	else
		return NULL;			/* fail */
	return newval;				/* OK */
}

static const char *
assign_hashagg_compress_spill_files(const char *newval, bool doit, GucSource source)
{
	int i;
	const char *val = newval;

	if (pg_strcasecmp(newval, "nothing") == 0)
		val = "none";

	i = bfz_string_to_compression(val);
	if (i == -1)
		return NULL;			/* fail */
	if (doit)
		gp_hashagg_compress_spill_files = i;
	return newval;				/* OK */
}

static const char *
assign_gp_workfile_compress_algorithm(const char *newval, bool doit, GucSource source)
{
	int i = bfz_string_to_compression(newval);
	if (i == -1)
		return NULL;			/* fail */
	if (doit)
		gp_workfile_compress_algorithm = i;
	return newval;				/* OK */
}

static const char *
assign_gp_workfile_type_hashjoin(const char * newval, bool doit, GucSource source)
{
	ExecWorkFileType newtype = BFZ;
	if (!pg_strcasecmp(newval, "bfz"))
	{
		newtype = BFZ;
	}
	else if (!pg_strcasecmp(newval,"buffile"))
	{
		newtype = BUFFILE;
	}
	else
		return NULL;
	if (doit)
		gp_workfile_type_hashjoin = newtype;

	return newval;
}

static const char *
show_num_temp_buffers(void)
{
	/*
	 * We show the GUC var until local buffers have been initialized, and
	 * NLocBuffer afterwards.
	 */
	static char nbuf[32];

	sprintf(nbuf, "%d", NLocBuffer ? NLocBuffer : num_temp_buffers);
	return nbuf;
}

static bool
assign_phony_autocommit(bool newval, bool doit, GucSource source)
{
	if (!newval)
	{
		if (doit && source >= PGC_S_INTERACTIVE)
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("SET AUTOCOMMIT TO OFF is no longer supported")));
		return false;
	}
	return true;
}

static const char *
assign_custom_variable_classes(const char *newval, bool doit, GucSource source)
{
	/*
	 * Check syntax. newval must be a comma separated list of identifiers.
	 * Whitespace is allowed but removed from the result.
	 */
	bool		hasSpaceAfterToken = false;
	const char *cp = newval;
	int			symLen = 0;
	int			c;
	StringInfoData buf;

	initStringInfo(&buf);
	while ((c = *cp++) != 0)
	{
		if (isspace((unsigned char) c))
		{
			if (symLen > 0)
				hasSpaceAfterToken = true;
			continue;
		}

		if (c == ',')
		{
			hasSpaceAfterToken = false;
			if (symLen > 0)
			{
				symLen = 0;
				appendStringInfoChar(&buf, ',');
			}
			continue;
		}

		if (hasSpaceAfterToken || !isalnum((unsigned char) c))
		{
			/*
			 * Syntax error due to token following space after token or non
			 * alpha numeric character
			 */
			ereport(LOG,
					(errcode(ERRCODE_SYNTAX_ERROR),
					 errmsg("invalid syntax for \"custom_variable_classes\": \"%s\"", newval)));
			pfree(buf.data);
			return NULL;
		}
		symLen++;
		appendStringInfoChar(&buf, (char) c);
	}

	/* Remove stray ',' at end */
	if (symLen == 0 && buf.len > 0)
		buf.data[--buf.len] = '\0';

	if (buf.len == 0)
		newval = NULL;
	else if (doit)
		newval = strdup(buf.data);

	pfree(buf.data);
	return newval;
}

static bool
assign_debug_assertions(bool newval, bool doit, GucSource source)
{
#ifndef USE_ASSERT_CHECKING
	if (newval)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			   errmsg("assertion checking is not supported by this build")));
#endif
	return true;
}

static bool
assign_autovacuum_warning(bool newval, bool doit, GucSource source)
{
	if (newval)
		elog(WARNING, "Autovacuum is not supported in GPDB.");

	return true;
}

static bool
assign_ssl(bool newval, bool doit, GucSource source)
{
#ifndef USE_SSL
	if (newval)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("SSL is not supported by this build")));
#endif
	return true;
}

static bool
assign_optimizer(bool newval, bool doit, GucSource source)
{
#ifndef USE_ORCA
	if (newval)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("ORCA is not supported by this build")));
#endif

	if (!optimizer_control)
	{
		if (source >= PGC_S_INTERACTIVE)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("cannot change the value of \"optimizer\"")));
		}

		/* source == PGC_S_OVERRIDE means do it anyway, eg at xact abort */
		if (source != PGC_S_OVERRIDE)
		{
			return false;
		}
	}

	return true;
}

static bool
assign_stage_log_stats(bool newval, bool doit, GucSource source)
{
	if (newval && log_statement_stats)
	{
		if (source >= PGC_S_INTERACTIVE)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("cannot enable parameter when \"log_statement_stats\" is true")));
		/* source == PGC_S_OVERRIDE means do it anyway, eg at xact abort */
		else if (source != PGC_S_OVERRIDE)
			return false;
	}
	return true;
}

static bool
assign_log_stats(bool newval, bool doit, GucSource source)
{
	if (newval &&
		(log_parser_stats || log_planner_stats || log_executor_stats || log_dispatch_stats))
	{
		if (source >= PGC_S_INTERACTIVE)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("cannot enable \"log_statement_stats\" when "
							"\"log_parser_stats\", \"log_planner_stats\", "
							"\"log_dispatch_stats\", "
							"or \"log_executor_stats\" is true")));
		/* source == PGC_S_OVERRIDE means do it anyway, eg at xact abort */
		else if (source != PGC_S_OVERRIDE)
			return false;
	}
	return true;
}

static bool
assign_dispatch_log_stats(bool newval, bool doit, GucSource source)
{
	if (newval &&
			(log_parser_stats || log_planner_stats || log_executor_stats || log_statement_stats))
	{
		if (source >= PGC_S_INTERACTIVE)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("cannot enable \"log_dispatch_stats\" when "
							"\"log_statement_stats\", "
							"\"log_parser_stats\", \"log_planner_stats\", "
							"or \"log_executor_stats\" is true")));
		/* source == PGC_S_OVERRIDE means do it anyway, eg at xact abort */
		else if (source != PGC_S_OVERRIDE)
			return false;
	}
	return true;
}

static bool
assign_transaction_read_only(bool newval, bool doit, GucSource source)
{
	/* Can't go to r/w mode inside a r/o transaction */
	if (newval == false && XactReadOnly && IsSubTransaction())
	{
		if (source >= PGC_S_INTERACTIVE)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("cannot set transaction read-write mode inside a read-only transaction")));
		/* source == PGC_S_OVERRIDE means do it anyway, eg at xact abort */
		else if (source != PGC_S_OVERRIDE)
			return false;
	}
	return true;
}

static const char *
assign_canonical_path(const char *newval, bool doit, GucSource source)
{
	if (doit)
	{
		char	   *canon_val = guc_strdup(ERROR, newval);

		canonicalize_path(canon_val);
		return canon_val;
	}
	else
		return newval;
}

static const char *
assign_backslash_quote(const char *newval, bool doit, GucSource source)
{
	BackslashQuoteType bq;
	bool		bqbool;

	/*
	 * Although only "on", "off", and "safe_encoding" are documented, we use
	 * parse_bool so we can accept all the likely variants of "on" and "off".
	 */
	if (pg_strcasecmp(newval, "safe_encoding") == 0)
		bq = BACKSLASH_QUOTE_SAFE_ENCODING;
	else if (parse_bool(newval, &bqbool))
	{
		bq = bqbool ? BACKSLASH_QUOTE_ON : BACKSLASH_QUOTE_OFF;
	}
	else
		return NULL;			/* reject */

	if (doit)
		backslash_quote = bq;

	return newval;
}

static const char *
assign_timezone_abbreviations(const char *newval, bool doit, GucSource source)
{
	/*
	 * The powerup value shown above for timezone_abbreviations is "UNKNOWN".
	 * When we see this we just do nothing.  If this value isn't overridden
	 * from the config file then pg_timezone_abbrev_initialize() will
	 * eventually replace it with "Default".  This hack has two purposes: to
	 * avoid wasting cycles loading values that might soon be overridden from
	 * the config file, and to avoid trying to read the timezone abbrev files
	 * during InitializeGUCOptions().  The latter doesn't work in an
	 * EXEC_BACKEND subprocess because my_exec_path hasn't been set yet and so
	 * we can't locate PGSHAREDIR.  (Essentially the same hack is used to
	 * delay initializing TimeZone ... if we have any more, we should try to
	 * clean up and centralize this mechanism ...)
	 */
	if (strcmp(newval, "UNKNOWN") == 0)
	{
		return newval;
	}

	/* Loading abbrev file is expensive, so only do it when value changes */
	if (timezone_abbreviations_string == NULL ||
		strcmp(timezone_abbreviations_string, newval) != 0)
	{
		int			elevel;

		/*
		 * If reading config file, only the postmaster should bleat loudly
		 * about problems.	Otherwise, it's just this one process doing it,
		 * and we use WARNING message level.
		 */
		if (source == PGC_S_FILE)
			elevel = IsUnderPostmaster ? DEBUG3 : LOG;
		else
			elevel = WARNING;
		if (!load_tzoffsets(newval, doit, elevel))
			return NULL;
	}
	return newval;
}

/*
 * pg_timezone_abbrev_initialize --- set default value if not done already
 *
 * This is called after initial loading of postgresql.conf.  If no
 * timezone_abbreviations setting was found therein, select default.
 */
void
pg_timezone_abbrev_initialize(void)
{
	if (strcmp(timezone_abbreviations_string, "UNKNOWN") == 0)
	{
		SetConfigOption("timezone_abbreviations", "Default",
						PGC_POSTMASTER, PGC_S_ARGV);
	}
}

static bool
assign_tcp_keepalives_idle(int newval, bool doit, GucSource source)
{
	if (doit)
		return (pq_setkeepalivesidle(newval, MyProcPort) == STATUS_OK);

	return true;
}

static const char *
show_tcp_keepalives_idle(void)
{
	static char nbuf[16];

	snprintf(nbuf, sizeof(nbuf), "%d", pq_getkeepalivesidle(MyProcPort));
	return nbuf;
}

static bool
assign_tcp_keepalives_interval(int newval, bool doit, GucSource source)
{
	if (doit)
		return (pq_setkeepalivesinterval(newval, MyProcPort) == STATUS_OK);

	return true;
}

static const char *
show_tcp_keepalives_interval(void)
{
	static char nbuf[16];

	snprintf(nbuf, sizeof(nbuf), "%d", pq_getkeepalivesinterval(MyProcPort));
	return nbuf;
}

static bool
assign_tcp_keepalives_count(int newval, bool doit, GucSource source)
{
	if (doit)
		return (pq_setkeepalivescount(newval, MyProcPort) == STATUS_OK);

	return true;
}

static const char *
show_tcp_keepalives_count(void)
{
	static char nbuf[16];

	snprintf(nbuf, sizeof(nbuf), "%d", pq_getkeepalivescount(MyProcPort));
	return nbuf;
}

#ifdef USE_CONNECTEMC
static const char *
assign_connectemc_mode(const char *newval, bool doit, GucSource source)
{
	if (pg_strcasecmp(newval, "on") == 0)
	{
		if (doit)
		{
			gp_emcconnect_transport = EMCCONNECT_MODE_TYPE_ON;
		}
	}
	else if (pg_strcasecmp(newval, "local") == 0)
	{
		if (doit)
		{
			gp_emcconnect_transport = EMCCONNECT_MODE_TYPE_LOCAL;
		}
	}
	else if (pg_strcasecmp(newval, "remote") == 0)
	{
		if (doit)
		{
			gp_emcconnect_transport = EMCCONNECT_MODE_TYPE_REMOTE;
		}
	}
	else if (pg_strcasecmp(newval, "off") == 0)
	{
		if (doit)
		{
			gp_emcconnect_transport = EMCCONNECT_MODE_TYPE_OFF;
		}
	}
	else
		return NULL;			/* fail */
	return newval;				/* OK */
}
#endif


static const char *
assign_debug_dtm_action(const char *newval,
						bool doit, GucSource source)
{
	if (pg_strcasecmp(newval, "none") == 0)
	{
		if (doit)
			Debug_dtm_action = DEBUG_DTM_ACTION_NONE;
	}
	else if (pg_strcasecmp(newval, "delay") == 0)
	{
		if (doit)
			Debug_dtm_action = DEBUG_DTM_ACTION_DELAY;
	}
	else if (pg_strcasecmp(newval, "fail_begin_command") == 0)
	{
		if (doit)
			Debug_dtm_action = DEBUG_DTM_ACTION_FAIL_BEGIN_COMMAND;
	}
	else if (pg_strcasecmp(newval, "fail_end_command") == 0)
	{
		if (doit)
			Debug_dtm_action = DEBUG_DTM_ACTION_FAIL_END_COMMAND;
	}
	else if (pg_strcasecmp(newval, "panic_begin_command") == 0)
	{
		if (doit)
			Debug_dtm_action = DEBUG_DTM_ACTION_PANIC_BEGIN_COMMAND;
	}
	else
		return NULL;			/* fail */
	return newval;				/* OK */
}

static const char *
assign_debug_dtm_action_target(const char *newval,
						bool doit, GucSource source)
{
	if (pg_strcasecmp(newval, "none") == 0)
	{
		if (doit)
			Debug_dtm_action_target = DEBUG_DTM_ACTION_TARGET_NONE;
	}
	else if (pg_strcasecmp(newval, "protocol") == 0)
	{
		if (doit)
			Debug_dtm_action_target = DEBUG_DTM_ACTION_TARGET_PROTOCOL;
	}
	else if (pg_strcasecmp(newval, "sql") == 0)
	{
		if (doit)
			Debug_dtm_action_target = DEBUG_DTM_ACTION_TARGET_SQL;
	}
	else
		return NULL;			/* fail */
	return newval;				/* OK */
}

static const char *
assign_pgstat_temp_directory(const char *newval, bool doit, GucSource source)
{
	if (doit)
	{
		char	   *canon_val = guc_strdup(ERROR, newval);
		char	   *tname;
		char	   *fname;

		canonicalize_path(canon_val);

		tname = guc_malloc(ERROR, strlen(canon_val) + 12);		/* /pgstat.tmp */
		sprintf(tname, "%s/pgstat.tmp", canon_val);
		fname = guc_malloc(ERROR, strlen(canon_val) + 13);		/* /pgstat.stat */
		sprintf(fname, "%s/pgstat.stat", canon_val);

		if (pgstat_stat_tmpname)
			free(pgstat_stat_tmpname);
		pgstat_stat_tmpname = tname;
		if (pgstat_stat_filename)
			free(pgstat_stat_filename);
		pgstat_stat_filename = fname;

		return canon_val;
	}
	else
		return newval;
}


static const char *
assign_debug_dtm_action_protocol(const char *newval,
						bool doit, GucSource source)
{
	if (pg_strcasecmp(newval, "none") == 0)
	{
		if (doit)
			Debug_dtm_action_protocol = DTX_PROTOCOL_COMMAND_NONE;
	}
	else if (pg_strcasecmp(newval, "abort_no_prepared") == 0)
	{
		if (doit)
			Debug_dtm_action_protocol = DTX_PROTOCOL_COMMAND_ABORT_NO_PREPARED;
	}
	else if (pg_strcasecmp(newval, "prepare") == 0)
	{
		if (doit)
			Debug_dtm_action_protocol = DTX_PROTOCOL_COMMAND_PREPARE;
	}
	else if (pg_strcasecmp(newval, "abort_some_prepared") == 0)
	{
		if (doit)
			Debug_dtm_action_protocol = DTX_PROTOCOL_COMMAND_ABORT_SOME_PREPARED;
	}
	else if (pg_strcasecmp(newval, "commit_prepared") == 0)
	{
		if (doit)
			Debug_dtm_action_protocol = DTX_PROTOCOL_COMMAND_COMMIT_PREPARED;
	}
	else if (pg_strcasecmp(newval, "abort_prepared") == 0)
	{
		if (doit)
			Debug_dtm_action_protocol = DTX_PROTOCOL_COMMAND_ABORT_PREPARED;
	}
	else if (pg_strcasecmp(newval, "retry_commit_prepared") == 0)
	{
		if (doit)
			Debug_dtm_action_protocol = DTX_PROTOCOL_COMMAND_RETRY_COMMIT_PREPARED;
	}
	else if (pg_strcasecmp(newval, "retry_abort_prepared") == 0)
	{
		if (doit)
			Debug_dtm_action_protocol = DTX_PROTOCOL_COMMAND_RETRY_ABORT_PREPARED;
	}
	else if (pg_strcasecmp(newval, "recovery_commit_prepared") == 0)
	{
		if (doit)
			Debug_dtm_action_protocol = DTX_PROTOCOL_COMMAND_RECOVERY_COMMIT_PREPARED;
	}
	else if (pg_strcasecmp(newval, "recovery_abort_prepared") == 0)
	{
		if (doit)
			Debug_dtm_action_protocol = DTX_PROTOCOL_COMMAND_RECOVERY_ABORT_PREPARED;
	}
	else if (pg_strcasecmp(newval, "subtransaction_begin") == 0)
	{
		if (doit)
			Debug_dtm_action_protocol = DTX_PROTOCOL_COMMAND_SUBTRANSACTION_BEGIN_INTERNAL;
	}
	else if (pg_strcasecmp(newval, "subtransaction_release") == 0)
	{
		if (doit)
			Debug_dtm_action_protocol = DTX_PROTOCOL_COMMAND_SUBTRANSACTION_RELEASE_INTERNAL;
	}
	else if (pg_strcasecmp(newval, "subtransaction_rollback") == 0)
	{
		if (doit)
			Debug_dtm_action_protocol = DTX_PROTOCOL_COMMAND_SUBTRANSACTION_ROLLBACK_INTERNAL;
	}
	else
		return NULL;			/* fail */
	return newval;				/* OK */
}

static const char *
assign_application_name(const char *newval, bool doit, GucSource source)
{
	if (doit)
	{
		/* Only allow clean ASCII chars in the application name */
		char	   *repval = guc_strdup(ERROR, newval);
		char	   *p;

		for (p = repval; *p; p++)
		{
			if (*p < 32 || *p > 126)
				*p = '?';
		}

		/* Update the pg_stat_activity view */
		pgstat_report_appname(repval);

		return repval;
	}
	else
		return newval;
}

/*
 * Assign hook routine for "allow_system_table_mods" GUC.
 * Setting it DDL or ALL requires either:
 * 		- upgrade_mode GUC set
 * 		- in single user mode
 */
static const char *
assign_allow_system_table_mods(const char *newval,
							   bool doit,
							   GucSource source)
{
	bool valueDDL = false;
	bool valueDML = false;

	if (newval == NULL || newval[0] == 0 ||
		!pg_strcasecmp("none", newval));
	else if (!pg_strcasecmp("dml", newval))
		valueDML = true;
	else if (!pg_strcasecmp("ddl", newval) &&
			 (gp_upgrade_mode || !IsUnderPostmaster))
		valueDDL = true;
	else if (!pg_strcasecmp("all", newval) &&
			 (gp_upgrade_mode || !IsUnderPostmaster))
	{
		valueDML = true;
		valueDDL = true;
	}
	else
	{
		if (source >= PGC_S_INTERACTIVE)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("Unknown system table modification policy. (Current policy: '%s')",
						show_allow_system_table_mods())));

		return NULL;
	}

	if (doit)
	{
		allowSystemTableModsDML = valueDML;
		allowSystemTableModsDDL = valueDDL;
	}

	return newval;
}

static const char *
show_allow_system_table_mods(void)
{
	if (allowSystemTableModsDML && allowSystemTableModsDDL)
		return "ALL";
	else if (allowSystemTableModsDDL)
		return "DDL";
	else if (allowSystemTableModsDML)
		return "DML";
	else
		return "NONE";
}

/*
 * Malloc a new string representing current storage_opts.
 */
static char *
storageOptToString(void)
{
	StringInfoData buf;
	char *ret;
	const StdRdOptions *ao_opts = currentAOStorageOptions();
	initStringInfo(&buf);
	appendStringInfo(&buf, "%s=%s", SOPT_APPENDONLY,
					 ao_opts->appendonly ? "true" : "false");
	appendStringInfo(&buf, ",%s=%d,", SOPT_BLOCKSIZE,
					 ao_opts->blocksize);
	if (ao_opts->compresstype)
	{
		appendStringInfo(&buf, "%s=%s,", SOPT_COMPTYPE,
						 ao_opts->compresstype);
	}
	else
	{
		appendStringInfo(&buf, "%s=none,", SOPT_COMPTYPE);
	}
	/*
	 * MPP-14504: we seem to allow compresslevel > 0 even when
	 * compression is disabled.
	 */
	if (ao_opts->compresslevel > 0)
	{
		appendStringInfo(&buf, "%s=%d,", SOPT_COMPLEVEL,
						 ao_opts->compresslevel);
	}
	appendStringInfo(&buf, "%s=%s,", SOPT_CHECKSUM,
					 ao_opts->checksum ? "true" : "false");
	appendStringInfo(&buf, "%s=%s", SOPT_ORIENTATION,
					 ao_opts->columnstore ? "column" : "row");
	ret = strdup(buf.data);
	if (ret == NULL)
		elog(ERROR, "out of memory");
	pfree(buf.data);
	return ret;
}

/*
 * Parse new value of storage options.  Update both, the GUC and
 * global ao_storage_opts object.
 */
static const char *
assign_gp_default_storage_options(const char *newval,
								  bool doit, GucSource source)
{
	if (source == PGC_S_DEFAULT || newval[0] == '\0')
	{
		/*
		 * Reset/init case, newval = "none".
		 */
		if (doit)
			resetDefaultAOStorageOpts();
	}
	else
	{
		PG_TRY();
		{
			/* Value of "appendonly" option if one is specified. */
			bool aovalue = false;
			StdRdOptions *newopts = (StdRdOptions *)
					palloc0(sizeof(StdRdOptions));
			Datum newopts_datum = parseAOStorageOpts(newval, &aovalue);
			/*
			 * Perform identical validations as in case of options
			 * specified in a WITH() clause.
			 */
			resetAOStorageOpts(newopts);
			parse_validate_reloptions(newopts, newopts_datum,
									  /* validate */ true, RELKIND_RELATION);
			validateAppendOnlyRelOptions(
					newopts->appendonly,
					newopts->blocksize,
					gp_safefswritesize,
					newopts->compresslevel,
					newopts->compresstype,
					newopts->checksum,
					RELKIND_RELATION,
					newopts->columnstore);
			/*
			 * All validations succeeded, it is safe to udpate global
			 * appendonly storage options.
			 */
			if (doit)
			{
				newopts->appendonly = aovalue;
				setDefaultAOStorageOpts(newopts);
			}
		}
		PG_CATCH();
		{
			if (source >= PGC_S_INTERACTIVE)
				PG_RE_THROW();
			else
			{
				/*
				 * We are in the middle of backend / postmaster
				 * startup.  The configured value is bad, proceed with
				 * factory defaults.
				 */
				elog(WARNING, "Unable to set gp_default_storage_options to '%s'",
					 newval);
				resetDefaultAOStorageOpts();
			}
		}
		PG_END_TRY();
	}
	return doit ? storageOptToString() : newval;
}

#include "guc-file.c"
