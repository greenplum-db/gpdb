/*--------------------------------------------------------------------
 * guc.h
 *
 * External declarations pertaining to backend/utils/misc/guc.c and
 * backend/utils/misc/guc-file.l
 *
 * Portions Copyright (c) 2007-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Copyright (c) 2000-2019, PostgreSQL Global Development Group
 * Written by Peter Eisentraut <peter_e@gmx.net>.
 *
 * src/include/utils/guc.h
 *--------------------------------------------------------------------
 */
#ifndef GUC_H
#define GUC_H

#include "nodes/parsenodes.h"
#include "tcop/dest.h"
#include "utils/array.h"

#define MAX_AUTHENTICATION_TIMEOUT (600)
#define MAX_PRE_AUTH_DELAY (60)
/*
 * One connection must be reserved for FTS to always able to probe
 * primary. So, this acts as lower limit on reserved superuser connections on
 * primaries.
 */
#define RESERVED_FTS_CONNECTIONS (1)


/*
 * Automatic configuration file name for ALTER SYSTEM.
 * This file will be used to store values of configuration parameters
 * set by ALTER SYSTEM command.
 */
#define PG_AUTOCONF_FILENAME		"postgresql.auto.conf"

/* upper limit for GUC variables measured in kilobytes of memory */
/* note that various places assume the byte size fits in a "long" variable */
#if SIZEOF_SIZE_T > 4 && SIZEOF_LONG > 4
#define MAX_KILOBYTES	INT_MAX
#else
#define MAX_KILOBYTES	(INT_MAX / 1024)
#endif

/*
 * Automatic configuration file name for ALTER SYSTEM.
 * This file will be used to store values of configuration parameters
 * set by ALTER SYSTEM command.
 */
#define PG_AUTOCONF_FILENAME		"postgresql.auto.conf"

/*
 * Certain options can only be set at certain times. The rules are
 * like this:
 *
 * INTERNAL options cannot be set by the user at all, but only through
 * internal processes ("server_version" is an example).  These are GUC
 * variables only so they can be shown by SHOW, etc.
 *
 * POSTMASTER options can only be set when the postmaster starts,
 * either from the configuration file or the command line.
 *
 * SIGHUP options can only be set at postmaster startup or by changing
 * the configuration file and sending the HUP signal to the postmaster
 * or a backend process. (Notice that the signal receipt will not be
 * evaluated immediately. The postmaster and the backend check it at a
 * certain point in their main loop. It's safer to wait than to read a
 * file asynchronously.)
 *
 * BACKEND and SU_BACKEND options can only be set at postmaster startup,
 * from the configuration file, or by client request in the connection
 * startup packet (e.g., from libpq's PGOPTIONS variable).  SU_BACKEND
 * options can be set from the startup packet only when the user is a
 * superuser.  Furthermore, an already-started backend will ignore changes
 * to such an option in the configuration file.  The idea is that these
 * options are fixed for a given backend once it's started, but they can
 * vary across backends.
 *
 * SUSET options can be set at postmaster startup, with the SIGHUP
 * mechanism, or from the startup packet or SQL if you're a superuser.
 *
 * USERSET options can be set by anyone any time.
 */
typedef enum
{
	PGC_INTERNAL,
	PGC_POSTMASTER,
	PGC_SIGHUP,
	PGC_SU_BACKEND,
	PGC_BACKEND,
	PGC_SUSET,
	PGC_USERSET
} GucContext;

/*
 * The following type records the source of the current setting.  A
 * new setting can only take effect if the previous setting had the
 * same or lower level.  (E.g, changing the config file doesn't
 * override the postmaster command line.)  Tracking the source allows us
 * to process sources in any convenient order without affecting results.
 * Sources <= PGC_S_OVERRIDE will set the default used by RESET, as well
 * as the current value.  Note that source == PGC_S_OVERRIDE should be
 * used when setting a PGC_INTERNAL option.
 *
 * PGC_S_INTERACTIVE isn't actually a source value, but is the
 * dividing line between "interactive" and "non-interactive" sources for
 * error reporting purposes.
 *
 * PGC_S_TEST is used when testing values to be used later ("doit" will always
 * be false, so this never gets stored as the actual source of any value).
 * For example, ALTER DATABASE/ROLE tests proposed per-database or per-user
 * defaults this way, and CREATE FUNCTION tests proposed function SET clauses
 * this way.  This is an interactive case, but it needs its own source value
 * because some assign hooks need to make different validity checks in this
 * case.  In particular, references to nonexistent database objects generally
 * shouldn't throw hard errors in this case, at most NOTICEs, since the
 * objects might exist by the time the setting is used for real.
 *
 * NB: see GucSource_Names in guc.c if you change this.
 */
typedef enum
{
	PGC_S_DEFAULT,				/* hard-wired default ("boot_val") */
	PGC_S_DYNAMIC_DEFAULT,		/* default computed during initialization */
	PGC_S_ENV_VAR,				/* postmaster environment variable */
	PGC_S_FILE,					/* postgresql.conf */
	PGC_S_ARGV,					/* postmaster command line */
	PGC_S_GLOBAL,				/* global in-database setting */
	PGC_S_DATABASE,				/* per-database setting */
	PGC_S_USER,					/* per-user setting */
	PGC_S_DATABASE_USER,		/* per-user-and-database setting */
	PGC_S_CLIENT,				/* from client connection request */
	PGC_S_RESGROUP,				/* per-resgroup setting */
	PGC_S_OVERRIDE,				/* special case to forcibly set default */
	PGC_S_INTERACTIVE,			/* dividing line for error reporting */
	PGC_S_TEST,					/* test per-database or per-user setting */
	PGC_S_SESSION				/* SET command */
} GucSource;

/*
 * Parsing the configuration file(s) will return a list of name-value pairs
 * with source location info.  We also abuse this data structure to carry
 * error reports about the config files.  An entry reporting an error will
 * have errmsg != NULL, and might have NULLs for name, value, and/or filename.
 *
 * If "ignore" is true, don't attempt to apply the item (it might be an error
 * report, or an item we determined to be duplicate).  "applied" is set true
 * if we successfully applied, or could have applied, the setting.
 */
typedef struct ConfigVariable
{
	char	   *name;
	char	   *value;
	char	   *errmsg;
	char	   *filename;
	int			sourceline;
	bool		ignore;
	bool		applied;
	struct ConfigVariable *next;
} ConfigVariable;

extern bool ParseConfigFile(const char *config_file, bool strict,
							const char *calling_file, int calling_lineno,
							int depth, int elevel,
							ConfigVariable **head_p, ConfigVariable **tail_p);
extern bool ParseConfigFp(FILE *fp, const char *config_file,
						  int depth, int elevel,
						  ConfigVariable **head_p, ConfigVariable **tail_p);
extern bool ParseConfigDirectory(const char *includedir,
								 const char *calling_file, int calling_lineno,
								 int depth, int elevel,
								 ConfigVariable **head_p,
								 ConfigVariable **tail_p);
extern void FreeConfigVariables(ConfigVariable *list);

/*
 * The possible values of an enum variable are specified by an array of
 * name-value pairs.  The "hidden" flag means the value is accepted but
 * won't be displayed when guc.c is asked for a list of acceptable values.
 */
struct config_enum_entry
{
	const char *name;
	int			val;
	bool		hidden;
};

/*
 * Signatures for per-variable check/assign/show hook functions
 */
typedef bool (*GucBoolCheckHook) (bool *newval, void **extra, GucSource source);
typedef bool (*GucIntCheckHook) (int *newval, void **extra, GucSource source);
typedef bool (*GucRealCheckHook) (double *newval, void **extra, GucSource source);
typedef bool (*GucStringCheckHook) (char **newval, void **extra, GucSource source);
typedef bool (*GucEnumCheckHook) (int *newval, void **extra, GucSource source);

typedef void (*GucBoolAssignHook) (bool newval, void *extra);
typedef void (*GucIntAssignHook) (int newval, void *extra);
typedef void (*GucRealAssignHook) (double newval, void *extra);
typedef void (*GucStringAssignHook) (const char *newval, void *extra);
typedef void (*GucEnumAssignHook) (int newval, void *extra);

typedef const char *(*GucShowHook) (void);

/*
 * Miscellaneous
 */
typedef enum
{
	/* Types of set_config_option actions */
	GUC_ACTION_SET,				/* regular SET command */
	GUC_ACTION_LOCAL,			/* SET LOCAL command */
	GUC_ACTION_SAVE				/* function SET option, or temp assignment */
} GucAction;

#define GUC_QUALIFIER_SEPARATOR '.'

/*
 * bit values in "flags" of a GUC variable
 */
#define GUC_LIST_INPUT			0x0001	/* input can be list format */
#define GUC_LIST_QUOTE			0x0002	/* double-quote list elements */
#define GUC_NO_SHOW_ALL			0x0004	/* exclude from SHOW ALL */
#define GUC_NO_RESET_ALL		0x0008	/* exclude from RESET ALL */
#define GUC_REPORT				0x0010	/* auto-report changes to client */
#define GUC_NOT_IN_SAMPLE		0x0020	/* not in postgresql.conf.sample */
#define GUC_DISALLOW_IN_FILE	0x0040	/* can't set in postgresql.conf */
#define GUC_CUSTOM_PLACEHOLDER	0x0080	/* placeholder for custom variable */
#define GUC_SUPERUSER_ONLY		0x0100	/* show only to superusers */
#define GUC_IS_NAME				0x0200	/* limit string to NAMEDATALEN-1 */
#define GUC_NOT_WHILE_SEC_REST	0x0400	/* can't set if security restricted */
#define GUC_DISALLOW_IN_AUTO_FILE 0x0800	/* can't set in
											 * PG_AUTOCONF_FILENAME */

#define GUC_UNIT_KB				0x1000	/* value is in kilobytes */
#define GUC_UNIT_BLOCKS			0x2000	/* value is in blocks */
#define GUC_UNIT_XBLOCKS		0x3000	/* value is in xlog blocks */
#define GUC_UNIT_MB				0x4000	/* value is in megabytes */
#define GUC_UNIT_BYTE			0x8000	/* value is in bytes */
#define GUC_UNIT_MEMORY			0xF000	/* mask for size-related units */

#define GUC_UNIT_MS			   0x10000	/* value is in milliseconds */
#define GUC_UNIT_S			   0x20000	/* value is in seconds */
#define GUC_UNIT_MIN		   0x30000	/* value is in minutes */
#define GUC_UNIT_TIME		   0xF0000	/* mask for time-related units */

#define GUC_EXPLAIN			  0x100000	/* include in explain */

#define GUC_UNIT				(GUC_UNIT_MEMORY | GUC_UNIT_TIME)

/* GPDB speific */
#define GUC_DISALLOW_USER_SET  0x00200000 /* Do not allow this GUC to be set by the user */
#define GUC_GPDB_NEED_SYNC     0x00400000  /* guc value is synced between master and primary */
#define GUC_GPDB_NO_SYNC       0x00800000  /* guc value is not synced between master and primary */

/* GUC lists for gp_guc_list_show().  (List of struct config_generic) */
extern List    *gp_guc_list_for_explain;
extern List    *gp_guc_list_for_no_plan;

/* Changed GUC which need to be pass to QE from QD */
extern List *gp_guc_restore_list;
extern bool gp_guc_need_restore;

/* GUC vars that are actually declared in guc.c, rather than elsewhere */
extern bool log_duration;
extern bool Debug_print_plan;
extern bool Debug_print_parse;
extern bool Debug_print_rewritten;
extern bool Debug_pretty_print;

extern bool	Debug_print_full_dtm;
extern bool	Debug_print_snapshot_dtm;
extern bool Debug_disable_distributed_snapshot;
extern bool Debug_abort_after_distributed_prepared;
extern bool Debug_appendonly_print_insert;
extern bool Debug_appendonly_print_insert_tuple;
extern bool Debug_appendonly_print_scan;
extern bool Debug_appendonly_print_scan_tuple;
extern bool Debug_appendonly_print_delete;
extern bool Debug_appendonly_print_storage_headers;
extern bool Debug_appendonly_use_no_toast;
extern bool Debug_appendonly_print_blockdirectory;
extern bool Debug_appendonly_print_read_block;
extern bool Debug_appendonly_print_append_block;
extern bool Debug_appendonly_print_segfile_choice;
extern bool test_AppendOnlyHash_eviction_vs_just_marking_not_inuse;
extern bool Debug_appendonly_print_datumstream;
extern bool Debug_appendonly_print_visimap;
extern bool Debug_appendonly_print_compaction;
extern bool Debug_bitmap_print_insert;
extern bool enable_checksum_on_tables;
extern int  gp_max_local_distributed_cache;
extern bool gp_local_distributed_cache_stats;
extern bool gp_appendonly_verify_block_checksums;
extern bool gp_appendonly_verify_write_block;
extern bool gp_appendonly_compaction;

/*
 * Threshold of the ratio of dirty data in a segment file
 * over which the segment file will be compacted during
 * lazy vacuum.
 * 0 indicates compact whenever there is hidden data.
 * 10 indicates that a segment should be compacted when more than
 * 10% of the tuples are hidden.
 */
extern int  gp_appendonly_compaction_threshold;
extern bool gp_heap_require_relhasoids_match;
extern bool	debug_xlog_record_read;
extern bool Debug_cancel_print;
extern bool Debug_datumstream_write_print_small_varlena_info;
extern bool Debug_datumstream_write_print_large_varlena_info;
extern bool Debug_datumstream_read_check_large_varlena_integrity;
extern bool Debug_datumstream_block_read_check_integrity;
extern bool Debug_datumstream_block_write_check_integrity;
extern bool Debug_datumstream_read_print_varlena_info;
extern bool Debug_datumstream_write_use_small_initial_buffers;
extern bool	Debug_database_command_print;
extern bool Debug_resource_group;
extern bool gp_create_table_random_default_distribution;
extern bool gp_allow_non_uniform_partitioning_ddl;
extern int  dtx_phase2_retry_second;

/* WAL replication debug gucs */
extern bool debug_walrepl_snd;
extern bool debug_walrepl_syncrep;
extern bool debug_walrepl_rcv;
extern bool debug_basebackup;

extern int rep_lag_avoidance_threshold;

extern bool gp_maintenance_mode;
extern bool gp_maintenance_conn;
extern bool allow_segment_DML;
extern bool gp_allow_rename_relation_without_lock;

extern bool gp_ignore_error_table;

extern bool	Debug_dtm_action_primary;

extern bool gp_log_optimization_time;
extern bool log_parser_stats;
extern bool log_planner_stats;
extern bool log_executor_stats;
extern bool log_statement_stats;
extern bool log_dispatch_stats;
extern bool log_btree_build_stats;

extern PGDLLIMPORT bool check_function_bodies;
extern bool session_auth_is_superuser;

extern int	log_min_error_statement;
extern PGDLLIMPORT int log_min_messages;
extern PGDLLIMPORT int client_min_messages;
extern int	log_min_duration_statement;
extern int	log_temp_files;
extern double log_statement_sample_rate;
extern double log_xact_sample_rate;

extern int	temp_file_limit;

extern int	num_temp_buffers;

extern char *cluster_name;
extern PGDLLIMPORT char *ConfigFileName;
extern char *HbaFileName;
extern char *IdentFileName;
extern char *external_pid_file;

extern PGDLLIMPORT char *application_name;

extern int	tcp_keepalives_idle;
extern int	tcp_keepalives_interval;
extern int	tcp_keepalives_count;
extern int	tcp_user_timeout;

#ifdef TRACE_SORT
extern bool trace_sort;
#endif

extern bool vmem_process_interrupt;
extern bool execute_pruned_plan;

extern bool gp_enable_relsize_collection;

/* Debug DTM Action */
typedef enum
{
	DEBUG_DTM_ACTION_NONE = 0,
	DEBUG_DTM_ACTION_DELAY = 1,
	DEBUG_DTM_ACTION_FAIL_BEGIN_COMMAND = 2,
	DEBUG_DTM_ACTION_FAIL_END_COMMAND = 3,
	DEBUG_DTM_ACTION_PANIC_BEGIN_COMMAND = 4,

	DEBUG_DTM_ACTION_LAST = 4
}	DebugDtmAction;

/* Debug DTM Action */
typedef enum
{
	DEBUG_DTM_ACTION_TARGET_NONE = 0,
	DEBUG_DTM_ACTION_TARGET_PROTOCOL = 1,
	DEBUG_DTM_ACTION_TARGET_SQL = 2,

	DEBUG_DTM_ACTION_TARGET_LAST = 2
}	DebugDtmActionTarget;

extern char *Debug_dtm_action_sql_command_tag;
extern char *Debug_dtm_action_str;
extern char *Debug_dtm_action_target_str;
extern int Debug_dtm_action;
extern int Debug_dtm_action_target;
extern int Debug_dtm_action_protocol;
extern int Debug_dtm_action_segment;
extern int Debug_dtm_action_nestinglevel;


/* Enable check for compatibility of encoding and locale in createdb */
extern bool gp_encoding_check_locale_compatibility;


extern int	gp_connection_send_timeout;

extern bool create_restartpoint_on_ckpt_record_replay;

/* Macros to define the level of memory accounting to show in EXPLAIN ANALYZE */
#define EXPLAIN_MEMORY_VERBOSITY_SUPPRESS	0 /* Suppress memory reporting in explain analyze */
#define EXPLAIN_MEMORY_VERBOSITY_SUMMARY	1 /* Summary of memory usage for each owner in explain analyze */
#define EXPLAIN_MEMORY_VERBOSITY_DETAIL		2 /* Detail memory accounting tree for each slice in explain analyze */

/* ORCA related definitions */
#define OPTIMIZER_XFORMS_COUNT 400 /* number of transformation rules */

/* types of optimizer failures */
#define OPTIMIZER_ALL_FAIL 			0  /* all failures */
#define OPTIMIZER_UNEXPECTED_FAIL 	1  /* unexpected failures */
#define OPTIMIZER_EXPECTED_FAIL 	2 /* expected failures */

/* optimizer minidump mode */
#define OPTIMIZER_MINIDUMP_FAIL  	0  /* create optimizer minidump on failure */
#define OPTIMIZER_MINIDUMP_ALWAYS 	1  /* always create optimizer minidump */

/* optimizer cost model */
#define OPTIMIZER_GPDB_LEGACY           0       /* GPDB's legacy cost model */
#define OPTIMIZER_GPDB_CALIBRATED       1       /* GPDB's calibrated cost model */
#define OPTIMIZER_GPDB_EXPERIMENTAL     2       /* GPDB's experimental cost model */


/* Optimizer related gucs */
extern bool	optimizer;
extern bool optimizer_control;	/* controls whether the user can change the setting of the "optimizer" guc */
extern bool	optimizer_log;
extern int  optimizer_log_failure;
extern bool	optimizer_trace_fallback;
extern int optimizer_minidump;
extern int  optimizer_cost_model;
extern bool optimizer_metadata_caching;
extern int	optimizer_mdcache_size;

/* Optimizer debugging GUCs */
extern bool optimizer_print_query;
extern bool optimizer_print_plan;
extern bool optimizer_print_xform;
extern bool	optimizer_print_memo_after_exploration;
extern bool	optimizer_print_memo_after_implementation;
extern bool	optimizer_print_memo_after_optimization;
extern bool	optimizer_print_job_scheduler;
extern bool	optimizer_print_expression_properties;
extern bool	optimizer_print_group_properties;
extern bool	optimizer_print_optimization_context;
extern bool optimizer_print_optimization_stats;
extern bool optimizer_print_xform_results;

/* array of xforms disable flags */
extern bool optimizer_xforms[OPTIMIZER_XFORMS_COUNT];
extern char *optimizer_search_strategy_path;

/* GUCs to tell Optimizer to enable a physical operator */
extern bool optimizer_enable_indexjoin;
extern bool optimizer_enable_motions_masteronly_queries;
extern bool optimizer_enable_motions;
extern bool optimizer_enable_motion_broadcast;
extern bool optimizer_enable_motion_gather;
extern bool optimizer_enable_motion_redistribute;
extern bool optimizer_enable_sort;
extern bool optimizer_enable_materialize;
extern bool optimizer_enable_partition_propagation;
extern bool optimizer_enable_partition_selection;
extern bool optimizer_enable_outerjoin_rewrite;
extern bool optimizer_enable_multiple_distinct_aggs;
extern bool optimizer_enable_hashjoin_redistribute_broadcast_children;
extern bool optimizer_enable_broadcast_nestloop_outer_child;
extern bool optimizer_enable_streaming_material;
extern bool optimizer_enable_gather_on_segment_for_dml;
extern bool optimizer_enable_assert_maxonerow;
extern bool optimizer_enable_constant_expression_evaluation;
extern bool optimizer_enable_bitmapscan;
extern bool optimizer_enable_outerjoin_to_unionall_rewrite;
extern bool optimizer_enable_ctas;
extern bool optimizer_enable_partial_index;
extern bool optimizer_enable_dml;
extern bool	optimizer_enable_dml_constraints;
extern bool optimizer_enable_direct_dispatch;
extern bool optimizer_enable_master_only_queries;
extern bool optimizer_enable_hashjoin;
extern bool optimizer_enable_dynamictablescan;
extern bool optimizer_enable_indexscan;
extern bool optimizer_enable_indexonlyscan;
extern bool optimizer_enable_tablescan;
extern bool optimizer_enable_eageragg;
extern bool optimizer_expand_fulljoin;
extern bool optimizer_enable_hashagg;
extern bool optimizer_enable_groupagg;
extern bool optimizer_enable_mergejoin;
extern bool optimizer_prune_unused_columns;

/* Optimizer plan enumeration related GUCs */
extern bool optimizer_enumerate_plans;
extern bool optimizer_sample_plans;
extern int	optimizer_plan_id;
extern int	optimizer_samples_number;

/* Cardinality estimation related GUCs used by the Optimizer */
extern bool optimizer_extract_dxl_stats;
extern bool optimizer_extract_dxl_stats_all_nodes;
extern bool optimizer_print_missing_stats;
extern double optimizer_damping_factor_filter;
extern double optimizer_damping_factor_join;
extern double optimizer_damping_factor_groupby;
extern bool optimizer_dpe_stats;
extern bool optimizer_enable_derive_stats_all_groups;

/* Costing or tuning related GUCs used by the Optimizer */
extern int optimizer_segments;
extern int optimizer_penalize_broadcast_threshold;
extern double optimizer_cost_threshold;
extern double optimizer_nestloop_factor;
extern double optimizer_sort_factor;

/* Optimizer hints */
extern int optimizer_array_expansion_threshold;
extern int optimizer_join_order_threshold;
extern int optimizer_join_order;
extern int optimizer_join_arity_for_associativity_commutativity;
extern int optimizer_cte_inlining_bound;
extern int optimizer_push_group_by_below_setop_threshold;
extern bool optimizer_force_multistage_agg;
extern bool optimizer_force_three_stage_scalar_dqa;
extern bool optimizer_force_expanded_distinct_aggs;
extern bool optimizer_force_agg_skew_avoidance;
extern bool optimizer_penalize_skew;
extern bool optimizer_prune_computed_columns;
extern bool optimizer_push_requirements_from_consumer_to_producer;
extern bool optimizer_enforce_subplans;
extern bool optimizer_apply_left_outer_to_union_all_disregarding_stats;
extern bool optimizer_use_external_constant_expression_evaluation_for_ints;
extern bool optimizer_remove_order_below_dml;
extern bool optimizer_multilevel_partitioning;
extern bool optimizer_parallel_union;
extern bool optimizer_array_constraints;
extern bool optimizer_cte_inlining;
extern bool optimizer_enable_space_pruning;
extern bool optimizer_enable_associativity;
extern bool optimizer_enable_range_predicate_dpe;

/* Analyze related GUCs for Optimizer */
extern bool optimizer_analyze_root_partition;
extern bool optimizer_analyze_midlevel_partition;
extern bool optimizer_analyze_enable_merge_of_leaf_stats;

extern bool optimizer_use_gpdb_allocators;

/* optimizer GUCs for replicated table */
extern bool optimizer_replicated_table_insert;

/* GUCs for slice table*/
extern int	gp_max_slices;

/**
 * Enable logging of DPE match in optimizer.
 */
extern bool	optimizer_partition_selection_log;

/* optimizer join heuristic models */
#define JOIN_ORDER_IN_QUERY                 0
#define JOIN_ORDER_GREEDY_SEARCH            1
#define JOIN_ORDER_EXHAUSTIVE_SEARCH        2
#define JOIN_ORDER_EXHAUSTIVE2_SEARCH       3

/* Time based authentication GUC */
extern char  *gp_auth_time_override_str;

extern char  *gp_default_storage_options;

/* copy GUC */
extern bool gp_enable_segment_copy_checking;

extern int writable_external_table_bufsize;

/* Enable passing of query constraints to external table providers */
extern bool gp_external_enable_filter_pushdown;

/* Enable the Global Deadlock Detector */
extern bool gp_enable_global_deadlock_detector;

typedef enum
{
	INDEX_CHECK_NONE,
	INDEX_CHECK_SYSTEM,
	INDEX_CHECK_ALL
} IndexCheckType;

extern IndexCheckType gp_indexcheck_insert;

/* Storage option names */
#define SOPT_APPENDONLY    "appendonly"
#define SOPT_FILLFACTOR    "fillfactor"
#define SOPT_BLOCKSIZE     "blocksize"
#define SOPT_COMPTYPE      "compresstype"
#define SOPT_COMPLEVEL     "compresslevel"
#define SOPT_CHECKSUM      "checksum"

/*
 * Functions exported by guc.c
 */
extern void SetConfigOption(const char *name, const char *value,
							GucContext context, GucSource source);

extern void DefineCustomBoolVariable(
									 const char *name,
									 const char *short_desc,
									 const char *long_desc,
									 bool *valueAddr,
									 bool bootValue,
									 GucContext context,
									 int flags,
									 GucBoolCheckHook check_hook,
									 GucBoolAssignHook assign_hook,
									 GucShowHook show_hook);

extern void DefineCustomIntVariable(
									const char *name,
									const char *short_desc,
									const char *long_desc,
									int *valueAddr,
									int bootValue,
									int minValue,
									int maxValue,
									GucContext context,
									int flags,
									GucIntCheckHook check_hook,
									GucIntAssignHook assign_hook,
									GucShowHook show_hook);

extern void DefineCustomRealVariable(
									 const char *name,
									 const char *short_desc,
									 const char *long_desc,
									 double *valueAddr,
									 double bootValue,
									 double minValue,
									 double maxValue,
									 GucContext context,
									 int flags,
									 GucRealCheckHook check_hook,
									 GucRealAssignHook assign_hook,
									 GucShowHook show_hook);

extern void DefineCustomStringVariable(
									   const char *name,
									   const char *short_desc,
									   const char *long_desc,
									   char **valueAddr,
									   const char *bootValue,
									   GucContext context,
									   int flags,
									   GucStringCheckHook check_hook,
									   GucStringAssignHook assign_hook,
									   GucShowHook show_hook);

extern void DefineCustomEnumVariable(
									 const char *name,
									 const char *short_desc,
									 const char *long_desc,
									 int *valueAddr,
									 int bootValue,
									 const struct config_enum_entry *options,
									 GucContext context,
									 int flags,
									 GucEnumCheckHook check_hook,
									 GucEnumAssignHook assign_hook,
									 GucShowHook show_hook);

extern void EmitWarningsOnPlaceholders(const char *className);

extern const char *GetConfigOption(const char *name, bool missing_ok,
								   bool restrict_privileged);
extern const char *GetConfigOptionResetString(const char *name);
extern int	GetConfigOptionFlags(const char *name, bool missing_ok);
extern void ProcessConfigFile(GucContext context);
extern void InitializeGUCOptions(void);
extern bool SelectConfigFiles(const char *userDoption, const char *progname);
extern void ResetAllOptions(void);
extern void AtStart_GUC(void);
extern int	NewGUCNestLevel(void);
extern void AtEOXact_GUC(bool isCommit, int nestLevel);
extern void BeginReportingGUCOptions(void);
extern void ParseLongOption(const char *string, char **name, char **value);
extern bool parse_int(const char *value, int *result, int flags,
					  const char **hintmsg);
extern bool parse_real(const char *value, double *result, int flags,
					   const char **hintmsg);
extern int	set_config_option(const char *name, const char *value,
							  GucContext context, GucSource source,
							  GucAction action, bool changeVal, int elevel,
							  bool is_reload);
extern void AlterSystemSetConfigFile(AlterSystemStmt *setstmt);
extern char *GetConfigOptionByName(const char *name, const char **varname,
								   bool missing_ok);
extern void GetConfigOptionByNum(int varnum, const char **values, bool *noshow);
extern int	GetNumConfigOptions(void);

extern void SetPGVariable(const char *name, List *args, bool is_local);
extern void SetPGVariableOptDispatch(const char *name, List *args, bool is_local, bool gp_dispatch);
extern void GetPGVariable(const char *name, DestReceiver *dest);
extern TupleDesc GetPGVariableResultDesc(const char *name);

extern void ExecSetVariableStmt(VariableSetStmt *stmt, bool isTopLevel);
extern char *ExtractSetVariableArgs(VariableSetStmt *stmt);

extern void ProcessGUCArray(ArrayType *array,
							GucContext context, GucSource source, GucAction action);
extern ArrayType *GUCArrayAdd(ArrayType *array, const char *name, const char *value);
extern ArrayType *GUCArrayDelete(ArrayType *array, const char *name);
extern ArrayType *GUCArrayReset(ArrayType *array);

extern void pg_timezone_abbrev_initialize(void);

extern List *gp_guc_list_show(GucSource excluding, List *guclist);

extern struct config_generic *find_option(const char *name,
				bool create_placeholders, int elevel);

extern void set_gp_replication_config(const char *name, const char *value);

#ifdef EXEC_BACKEND
extern void write_nondefault_variables(GucContext context);
extern void read_nondefault_variables(void);
#endif

/* GUC serialization */
extern Size EstimateGUCStateSpace(void);
extern void SerializeGUCState(Size maxsize, char *start_address);
extern void RestoreGUCState(void *gucstate);

/* Support for messages reported from GUC check hooks */

extern PGDLLIMPORT char *GUC_check_errmsg_string;
extern PGDLLIMPORT char *GUC_check_errdetail_string;
extern PGDLLIMPORT char *GUC_check_errhint_string;

extern void GUC_check_errcode(int sqlerrcode);

#define GUC_check_errmsg \
	pre_format_elog_string(errno, TEXTDOMAIN), \
	GUC_check_errmsg_string = format_elog_string

#define GUC_check_errdetail \
	pre_format_elog_string(errno, TEXTDOMAIN), \
	GUC_check_errdetail_string = format_elog_string

#define GUC_check_errhint \
	pre_format_elog_string(errno, TEXTDOMAIN), \
	GUC_check_errhint_string = format_elog_string


/*
 * The following functions are not in guc.c, but are declared here to avoid
 * having to include guc.h in some widely used headers that it really doesn't
 * belong in.
 */

/* in commands/tablespace.c */
extern bool check_default_tablespace(char **newval, void **extra, GucSource source);
extern bool check_temp_tablespaces(char **newval, void **extra, GucSource source);
extern void assign_temp_tablespaces(const char *newval, void *extra);

/* in catalog/namespace.c */
extern bool check_search_path(char **newval, void **extra, GucSource source);
extern void assign_search_path(const char *newval, void *extra);

/* in access/transam/xlog.c */
extern bool check_wal_buffers(int *newval, void **extra, GucSource source);
extern void assign_xlog_sync_method(int new_sync_method, void *extra);

/* in cdb/cdbvars.c */
extern bool check_gp_role(char **newval, void **extra, GucSource source);
extern void assign_gp_role(const char *newval, void *extra);
extern const char *show_gp_role(void);
extern void assign_gp_write_shared_snapshot(bool newval, void *extra);
extern bool gpvars_check_gp_resource_manager_policy(char **newval, void **extra, GucSource source);
extern void gpvars_assign_gp_resource_manager_policy(const char *newval, void *extra);
extern const char *gpvars_show_gp_resource_manager_policy(void);
extern const char *gpvars_assign_gp_resqueue_memory_policy(const char *newval, bool doit, GucSource source);
extern const char *gpvars_show_gp_resqueue_memory_policy(void);
extern bool gpvars_check_statement_mem(int *newval, void **extra, GucSource source);
extern int guc_name_compare(const char *namea, const char *nameb);
extern void DispatchSyncPGVariable(struct config_generic * gconfig);

#endif							/* GUC_H */
