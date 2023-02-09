/*-------------------------------------------------------------------------
 *
 * cdbvars.h
 *	  definitions for Greenplum-specific global variables
 *
 * Portions Copyright (c) 2003-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbvars.h
 *
 * NOTES
 *	  See src/backend/utils/misc/guc_gp.c for variable external specification.
 *
 *-------------------------------------------------------------------------
 */

#ifndef CDBVARS_H
#define CDBVARS_H

#include "access/xlogdefs.h"  /*XLogRecPtr*/
#include "catalog/gp_segment_configuration.h" /* MASTER_CONTENT_ID */

/*
 * ----- Declarations of Greenplum-specific global variables ------
 */

#define WRITER_IS_MISSING_MSG "reader could not find writer proc entry"

#ifndef PRIO_MAX
#define PRIO_MAX 20
#endif

/*
 * Parameters gp_role
 *
 * The run-time parameters (GUC variables) gp_role
 * reports and provides control over the role assumed by a
 * postgres process.
 *
 * Valid  roles are the following:
 *
 *	dispatch	The process acts as a parallel SQL dispatcher.
 *	execute		The process acts as a parallel SQL executor.
 *	utility		The process acts as a simple SQL engine.
 *
 * For postmaster, the parameter is initialized by '-c' (required).
 *
 * For normal connections to cluster, you can connect to QD directly,
 * but can not connect to QE directly unless specifying the utility role.
 *
 * For utility role connection to either QD or QE, PGOPTIONS could be used.
 * Alternatively, libpq-based applications can be modified to set
 * the value directly via an argument to one of the libpq functions
 * PQconnectdb, PQsetdbLogin or PQconnectStart.
 *
 * For single backend connection, utility role is enforced in code.
 */

typedef enum
{
	GP_ROLE_UNDEFINED = 0,		/* Should never see this role in use */
	GP_ROLE_UTILITY,			/* Operating as a simple database engine */
	GP_ROLE_DISPATCH,			/* Operating as the parallel query dispatcher */
	GP_ROLE_EXECUTE,			/* Operating as a parallel query executor */
} GpRoleValue;

extern GpRoleValue Gp_role;	/* GUC var - server operating mode.  */
extern char *gp_role_string;	/* Use by guc.c as staging area for value. */

extern bool gp_reraise_signal; /* try to force a core dump ?*/

extern bool gp_set_proc_affinity; /* try to bind postmaster to a processor */

/* Parameter Gp_is_writer
 *
 * This run_time parameter indicates whether session is a qExec that is a
 * "writer" process.  Only writers are allowed to write changes to the database
 * and currently there is only one writer per group of segmates.
 *
 * It defaults to false, but may be specified as true using a connect option.
 * This should only ever be set by a QD connecting to a QE, rather than
 * directly.
 */
extern bool Gp_is_writer;

/* Parameter gp_session_id
 *
 * This run time parameter indicates a unique id to identify a particular user
 * session throughout the entire Greenplum array.
 */
extern int gp_session_id;
#define InvalidGpSessionId	(-1)

/* The Hostname where this segment's QD is located. This variable is NULL for the QD itself */
extern char * qdHostname;

/* The Postmaster listener port for the QD.  This variable is 0 for the QD itself.*/
extern int qdPostmasterPort;

/*How many gangs to keep around from stmt to stmt.*/
extern int			gp_cached_gang_threshold;

/*
 * gp_reject_percent_threshold
 *
 * When reject limit in single row error handling is specified in PERCENT,
 * we don't want to start calculating the percent of rows rejected right
 * from the 1st row, since for example, if the second row is bad this will
 * mean that 50% of the data is bad even if all the following rows are valid.
 * Therefore we want to keep track of count of bad rows but start calculating
 * the percent at a later stage in the game. By default we'll start the
 * calculation after 100 rows were processed, but this GUC can be set for
 * some other number if it makes more sense in certain situations.
 */
extern int			gp_reject_percent_threshold;

/*
 * For use while debugging DTM issues: alter MVCC semantics such that
 * "invisible" rows are returned.
 */
extern bool           gp_select_invisible;

/*
 * Used to set the maximum length of the current query which is displayed
 * when the user queries pg_stat_activty table.
 */
extern int			pgstat_track_activity_query_size;

/*
 * In order to facilitate testing of reader-gang/writer-gang synchronization
 * it is very handy to slow down the writer (opens important race-window).
 */
extern bool           gp_enable_slow_writer_testmode;

/*
 * MPP-6926: Resource Queues on by default
 */
#define GP_DEFAULT_RESOURCE_QUEUE_NAME "pg_default"

/* Parameter gp_debug_pgproc
 *
 * This run-time parameter requests to print out detailed info relevant to
 * PGPROC.
 */
extern bool gp_debug_pgproc;

/* Parameter debug_print_prelim_plan
 *
 * This run-time parameter is closely related to the PostgreSQL parameter
 * debug_print_plan which, if true, causes the final plan to display on the
 * server log prior to execution.  This parameter, if true, causes the
 * preliminary plan (from the optimizer prior to adding Motions for subplans)
 * to display on the log.
 */
extern bool Debug_print_prelim_plan;

/* Parameter debug_print_slice_table
 *
 * This run-time parameter, if true, causes the slice table for a plan to
 * display on the server log.  On a QD this occurs prior to sending the
 * slice table to any QE.  On a QE it occurs just prior to execution of
 * the plan slice.
 */
extern bool Debug_print_slice_table;

extern bool Debug_resource_group;

extern bool Debug_burn_xids;

/*
 * gp_external_enable_exec
 *
 * when set to 'true' external tables that were defined with an EXECUTE
 * clause (will execute OS level commands) are allowed to be SELECTed from.
 * When set to 'false' trying to SELECT from such a table will result in an
 * error. Default is 'true'
 */
extern bool gp_external_enable_exec;

/* gp_external_max_segs
 *
 * The maximum number of segment databases that will get assigned to
 * connect to a specific gpfdist (or gpfdists) client (1 gpfdist:// URI). The limit
 * is good to have as there is a number of segdbs that as we increase
 * it won't add to performance but may actually decrease it, while
 * occupying unneeded resources. By having a limit we can control our
 * server resources better while not harming performance.
 */
extern int gp_external_max_segs;

/*
 * This option determines whether curl verifies the authenticity of the
 * gpfdist's certificate.
 */
extern bool verify_gpfdists_cert;

/*
 * gp_command_count
 *
 * This GUC is 0 at the beginning of a client session and
 * increments for each command received from the client.
 */
extern int gp_command_count;

/*
 * Gp_write_shared_snapshot
 *
 * The value of this variable is actually meaningless. We use it simply
 * in order to have a mechanism of dispatching a SET query that will
 * allocate writer gangs that will write the shared snapshot and make
 * it available for the reader gangs. This is currently only needed
 * before DECLARE'ing a cursor, since a cursor query uses only reader
 * gangs. But should be used for other things if it makes sense to do so.
 */
extern bool Gp_write_shared_snapshot;

extern int gp_fts_transition_retries;
extern int gp_fts_transition_timeout;

/*
 * If number of subtransactions within a transaction exceed this limit,
 * then a warning is given to the user.
 */
extern int32 gp_subtrans_warn_limit;

extern const char *role_to_string(GpRoleValue role);

extern int	gp_segment_connect_timeout; /* GUC var - timeout specifier for gang creation */
extern int	gp_snapshotadd_timeout; /* GUC var - timeout specifier for snapshot-creation wait */

extern int	gp_fts_probe_retries; /* GUC var - specifies probe number of retries for FTS */
extern int	gp_fts_probe_timeout; /* GUC var - specifies probe timeout for FTS */
extern int	gp_fts_probe_interval; /* GUC var - specifies polling interval for FTS */
extern int	gp_fts_mark_mirror_down_grace_period;
extern int	gp_fts_replication_attempt_count; /* GUC var - specifies replication max attempt count for FTS */
extern int	gp_dtx_recovery_interval;
extern int	gp_dtx_recovery_prepared_period;

extern int gp_gang_creation_retry_count; /* How many retries ? */
extern int gp_gang_creation_retry_timer; /* How long between retries */

/* GUCs to control TCP keepalive settings for dispatch libpq connections */
extern int 			gp_dispatch_keepalives_idle;
extern int			gp_dispatch_keepalives_interval;
extern int			gp_dispatch_keepalives_count;

#define MAX_GP_DISPATCH_KEEPALIVES_IDLE 32767 /* Linux MAX_TCP_KEEPIDLE */
#define MAX_GP_DISPATCH_KEEPALIVES_INTERVAL 32767 /* Linux MAX_TCP_KEEPINTVL */
#define MAX_GP_DISPATCH_KEEPALIVES_COUNT 127 /* Linux MAX_TCP_KEEPCNT */

/*
 * Parameter Gp_max_packet_size
 *
 * The run-time parameter gp_max_packet_size controls the largest packet
 * size that the interconnect will transmit.  In general, the interconnect
 * tries to squeeze as many tuplechunks as possible into a packet without going
 * above this size limit.
 *
 * The largest tuple chunk size that the system will form can be derived
 * from this with:
 *		MAX_CHUNK_SIZE = Gp_max_packet_size - PACKET_HEADER_SIZE - TUPLE_CHUNK_HEADER_SIZE
 *
 *
 */
extern int	Gp_max_packet_size;	/* GUC var */

#define DEFAULT_PACKET_SIZE 8192
#define MIN_PACKET_SIZE 512
#define MAX_PACKET_SIZE 65507 /* Max payload for IPv4/UDP (subtract 20 more for IPv6 without extensions) */

/*
 * Support for multiple "types" of interconnect
 */
typedef enum GpVars_Interconnect_Type
{
	INTERCONNECT_TYPE_TCP = 0,
	INTERCONNECT_TYPE_UDPIFC,
	INTERCONNECT_TYPE_PROXY,
} GpVars_Interconnect_Type;

extern int Gp_interconnect_type;

/*
 * We support different strategies for address binding for sockets used for
 * motion communication over the interconnect.
 *
 * One approach is to use an unicast address, specifically the segment's
 * gp_segment_configuration.address field to perform the address binding. This
 * has the benefits of reducing port usage on a segment host and ensures that
 * the NIC backed by the address field is the only one used for communication
 * (and not an externally facing slower NIC, like the ones that typically back
 * the gp_segment_configuration.hostname field)
 *
 * In some cases, inter-segment communication using the unicast address
 * mentioned above, may not be possible. One such example is if the source
 * segment's address field and the destination segment's address field are on
 * different subnets and/or existing routing rules don't allow for such
 * communication. In these cases, using a wildcard address for address binding
 * is the only available fallback, enabling the use of any network interface
 * compliant with routing rules.
 */
typedef enum GpVars_Interconnect_Address_Type
{
	INTERCONNECT_ADDRESS_TYPE_UNICAST = 0,
	INTERCONNECT_ADDRESS_TYPE_WILDCARD
} GpVars_Interconnect_Address_Type;

extern int Gp_interconnect_address_type;

extern char *gp_interconnect_proxy_addresses;

typedef enum GpVars_Interconnect_Method
{
	INTERCONNECT_FC_METHOD_CAPACITY = 0,
	INTERCONNECT_FC_METHOD_LOSS = 2,
} GpVars_Interconnect_Method;

extern int Gp_interconnect_fc_method;

/*
 * Parameter Gp_interconnect_queue_depth
 *
 * The run-time parameter Gp_interconnect_queue_depth controls the
 * number of outstanding messages allowed to accumulated on a
 * 'connection' before the peer will start to backoff.
 *
 * This guc is specific to the UDP-interconnect.
 *
 */
extern int	Gp_interconnect_queue_depth;

/*
 * Parameter Gp_interconnect_snd_queue_depth
 *
 * The run-time parameter Gp_interconnect_snd_queue_depth controls the
 * average number of outstanding messages on the sender side
 *
 * This guc is specific to the UDP-interconnect.
 *
 */
extern int	Gp_interconnect_snd_queue_depth;
extern int	Gp_interconnect_timer_period;
extern int	Gp_interconnect_timer_checking_period;
extern int	Gp_interconnect_default_rtt;
extern int	Gp_interconnect_min_rto;
extern int  Gp_interconnect_transmit_timeout;
extern int	Gp_interconnect_min_retries_before_timeout;
extern int	Gp_interconnect_debug_retry_interval;

/* UDP recv buf size in KB.  For testing */
extern int 	Gp_udp_bufsize_k;

/*
 * Parameter gp_interconnect_aggressive_retry
 *
 * The run-time parameter gp_interconnect_aggressive_retry controls the
 * activation of the application-level retry (which acts much faster than the OS-level
 * TCP retries); In most cases this should stay enabled.
 */
extern bool gp_interconnect_aggressive_retry; /* fast-track app-level retry */

/*
 * Parameter gp_interconnect_full_crc
 *
 * Perform a full CRC on UDP-packets as they depart and arrive.
 */
extern bool gp_interconnect_full_crc;

/*
 * Parameter gp_interconnect_log_stats
 *
 * Emit interconnect statistics at log-level, instead of debug1
 */
extern bool gp_interconnect_log_stats;

extern bool gp_interconnect_cache_future_packets;

#define UNDEF_SEGMENT -2

/*
 * Parameter interconnect_setup_timeout
 *
 * The run-time parameter (GUC variable) interconnect_setup_timeout is used
 * during SetupInterconnect() to timeout the operation.  This value is in
 * seconds.  Setting it to zero effectively disables the timeout.
 */
extern int	interconnect_setup_timeout;

#ifdef USE_ASSERT_CHECKING
/*
 * UDP-IC Test hooks (for fault injection).
 */
extern int gp_udpic_dropseg; /* specifies which segments to apply the
							  * following two gucs -- if set to
							  * UNDEF_SEGMENT, all segments apply
							  * them. */
extern int gp_udpic_dropxmit_percent;
extern int gp_udpic_dropacks_percent;
extern int gp_udpic_fault_inject_percent;
extern int gp_udpic_fault_inject_bitmap;
extern int gp_udpic_network_disable_ipv6;
#endif

/*
 * Each slice table has a unique ID (certain commands like "vacuum
 * analyze" run many many slice-tables for each gp_command_id). This
 * gets passed around as part of the slice-table (not used as a guc!).
 */
extern uint32 gp_interconnect_id;

/* --------------------------------------------------------------------------------------------------
 * Logging
 */

typedef enum GpVars_Verbosity
{
	GPVARS_VERBOSITY_UNDEFINED = 0,
    GPVARS_VERBOSITY_OFF,
	GPVARS_VERBOSITY_TERSE,
	GPVARS_VERBOSITY_VERBOSE,
    GPVARS_VERBOSITY_DEBUG,
} GpVars_Verbosity;

/* Enable single-slice single-row inserts. */
extern bool gp_enable_fast_sri;

/* Enable single-mirror pair dispatch. */
extern bool gp_enable_direct_dispatch;

/* Name of pseudo-function to access any table as if it was randomly distributed. */
#define GP_DIST_RANDOM_NAME "GP_DIST_RANDOM"

/*
 * gp_log_gang
 *
 * Should creation, reallocation and cleanup of gangs of QE processes be logged?
 * "OFF"     -> only errors are logged
 * "TERSE"   -> terse logging of routine events, e.g. creation of new qExecs
 * "VERBOSE" -> gang allocation per command is logged
 * "DEBUG"   -> additional events are logged at severity level DEBUG1 to DEBUG5
 *
 * The messages that are enabled by the TERSE and VERBOSE settings are
 * written with a severity level of LOG.
 */
extern int gp_log_gang;

/*
 * gp_log_fts
 *
 * What kind of messages should be logged by the fault-prober
 * "OFF"     -> only errors are logged
 * "TERSE"   -> terse logging of routine events
 * "VERBOSE" -> more messages
 * "DEBUG"   -> additional events are logged at severity level DEBUG1 to DEBUG5
 *
 * The messages that are enabled by the TERSE and VERBOSE settings are
 * written with a severity level of LOG.
 */
extern int gp_log_fts;

/*
 * gp_log_interconnect
 *
 * Should connections between internal processes be logged?  (qDisp/qExec/etc)
 * "OFF"     -> connection errors are logged
 * "TERSE"   -> terse logging of routine events, e.g. successful connections
 * "VERBOSE" -> most interconnect setup events are logged
 * "DEBUG"   -> additional events are logged at severity level DEBUG1 to DEBUG5.
 *
 * The messages that are enabled by the TERSE and VERBOSE settings are
 * written with a severity level of LOG.
 */
extern int gp_log_interconnect;

/* --------------------------------------------------------------------------------------------------
 * Greenplum Optimizer GUCs
 */

/*
 * "gp_motion_cost_per_row"
 *
 * If >0, the planner uses this value -- instead of 2 * "cpu_tuple_cost" --
 * for Motion operator cost estimation.
 */
extern double   gp_motion_cost_per_row;

/*
 * "gp_segments_for_planner"
 *
 * If >0, number of segment dbs for the planner to assume in its cost and size
 * estimates.  If 0, estimates are based on the actual number of segment dbs.
 */
extern int      gp_segments_for_planner;

/*
 * Enable/disable the special optimization of MIN/MAX aggregates as
 * Index Scan with limit.
 */
extern bool gp_enable_minmax_optimization;

/*
 * "gp_enable_multiphase_agg"
 *
 * Unlike some other enable... vars, gp_enable_multiphase_agg is not cost based.
 * When set to false, the planner will not use multi-phase aggregation.
 */
extern bool gp_enable_multiphase_agg;

/*
 * Perform a post-planning scan of the final plan looking for motion deadlocks:
 * emit verbose messages about any found.
 */
extern bool gp_enable_motion_deadlock_sanity;

/*
 * Adjust selectivity for nulltests atop of outer joins;
 * Special casing prominent use case to work around lack of (NOT) IN subqueries
 */
extern bool gp_adjust_selectivity_for_outerjoins;

/*
 * Target density for hash-node (HJ).
 */
extern int gp_hashjoin_tuples_per_bucket;
extern int gp_hashagg_groups_per_bucket;

/*
 * Damping of selectivities of clauses which pertain to the same base
 * relation; compensates for undetected correlation
 */
extern bool gp_selectivity_damping_for_scans;

/*
 * Damping of selectivities of clauses in joins
 */
extern bool gp_selectivity_damping_for_joins;

/*
 * Damping factor for selecticity damping
 */
extern double gp_selectivity_damping_factor;

/*
 * Sort selectivities by significance before applying
 * damping (ON by default)
 */
extern bool gp_selectivity_damping_sigsort;


/* ----- Experimental Features ----- */

/*
 * "gp_enable_agg_distinct"
 *
 * May Greenplum redistribute on the argument of a lone aggregate distinct in
 * order to use 2-phase aggregation?
 *
 * The code does uses planner estimates to decide whether to use this feature,
 * when enabled.
 */
extern bool gp_enable_agg_distinct;

/*
 * "gp_enable_agg_distinct_pruning"
 *
 * May Greenplum use grouping in the first phases of 3-phase aggregation to
 * prune values from DISTINCT-qualified aggregate function arguments?
 *
 * The code uses planner estimates to decide whether to use this feature,
 * when enabled.
 */
extern bool gp_enable_dqa_pruning;

/* May Greenplum apply Unique operator (and possibly a Sort) in parallel prior
 * to the collocation motion for a Unique operator?  The idea is to reduce
 * the number of rows moving over the interconnect.
 *
 * The code uses planner estimates for this.  If enabled, the tactic is used
 * only if it is estimated to be cheaper that a 1-phase approach.  However,
 * see gp_eqger_preunique.
 */
extern bool gp_enable_preunique;

/* May Greenplum dump statistics for all segments as a huge ugly string
 * during EXPLAIN ANALYZE?
 *
 */
extern bool gp_enable_explain_allstat;

/*
 * What level of details of the memory accounting information to show during EXPLAIN ANALYZE?
 */
extern int explain_memory_verbosity;

/* May Greenplum restrict ORDER BY sorts to the first N rows if the ORDER BY
 * is wrapped by a LIMIT clause (where N=OFFSET+LIMIT)?
 *
 * The code does not currently use planner estimates for this.  If enabled,
 * the tactic is used whenever possible.
 */
extern bool gp_enable_sort_limit;

extern bool trace_sort;

/**
 * Enable dynamic pruning of partitions based on join condition.
 */
extern bool gp_dynamic_partition_pruning;

/* Sharing of plan fragments for common table expressions */
extern bool gp_cte_sharing;
/* Enable RECURSIVE clauses in common table expressions */
extern bool gp_recursive_cte;

/* Enable check for compatibility of encoding and locale in createdb */
extern bool gp_encoding_check_locale_compatibility;

/* Priority for the segworkers relative to the postmaster's priority */
extern int gp_segworker_relative_priority;

/*  Max size of dispatched plans; 0 if no limit */
extern int gp_max_plan_size;

/* The default number of batches to use when the hybrid hashed aggregation
 * algorithm (re-)spills in-memory groups to disk.
 */
extern int gp_hashagg_default_nbatches;

/* Get statistics for partitioned parent from a child */
extern bool 	gp_statistics_pullup_from_child_partition;

/* Extract numdistinct from foreign key relationship */
extern bool		gp_statistics_use_fkeys;

/* Allow user to force tow stage agg */
extern bool     gp_eager_two_phase_agg;

/* Analyze tools */
extern int gp_motion_slice_noop;

/* Disable setting of hint-bits while reading db pages */
extern bool gp_disable_tuple_hints;

/* Enable metrics */
extern bool gp_enable_query_metrics;
extern int gp_instrument_shmem_size;

extern bool dml_ignore_target_partition_check;

extern int gp_workfile_limit_per_segment;
extern int gp_workfile_limit_per_query;
extern int gp_workfile_limit_files_per_query;
extern int gp_workfile_caching_loglevel;
extern int gp_sessionstate_loglevel;
extern int gp_workfile_bytes_to_checksum;

extern bool coredump_on_memerror;

/*
 * Autostats feature, whether or not to to automatically run ANALYZE after 
 * insert/delete/update/ctas or after ctas/copy/insert in case the target
 * table has no statistics
 */
typedef enum
{
	GP_AUTOSTATS_NONE = 0,			/* Autostats is switched off */
	GP_AUTOSTATS_ON_CHANGE,			/* Autostats is enabled on change
									 * (insert/delete/update/ctas) */
	GP_AUTOSTATS_ON_NO_STATS,		/* Autostats is enabled on ctas or copy or
									 * insert if no stats are present */
} GpAutoStatsModeValue;

extern int	gp_autostats_mode;
extern int	gp_autostats_mode_in_functions;
extern int	gp_autostats_on_change_threshold;
extern bool	gp_autostats_allow_nonowner;
extern bool	log_autostats;

extern bool	gp_explain_jit;


/* --------------------------------------------------------------------------------------------------
 * Server debugging
 */

/*
 * gp_debug_linger (integer)
 *
 * Upon an error with severity FATAL and error code ERRCODE_INTERNAL_ERROR,
 * errfinish() will sleep() for the specified number of seconds before
 * termination, to let the user attach a debugger.
 */
extern int  gp_debug_linger;

/* --------------------------------------------------------------------------------------------------
 * Non-GUC globals
 */

#define UNSET_SLICE_ID -1
extern int	currentSliceId;

extern int cdb_total_plans;
/* Enable ading the cost for walking the chain in the hash join. */
extern bool gp_cost_hashjoin_chainwalk;

extern int cdb_total_slices;
extern int cdb_max_slices;

typedef struct GpId
{
	int32		dbid;			/* the dbid of this database */
	int32		segindex;		/* content indicator: -1 for entry database,
								 * 0, ..., n-1 for segment database *
								 * a primary and its mirror have the same segIndex */
} GpId;

/* --------------------------------------------------------------------------------------------------
 * Global variable declaration for the data for the single row of gp_id table
 */
extern GpId GpIdentity;

/*
 * Maximum length of string representation of 'dbid' (same as max length of an int4)
 */
#define MAX_DBID_STRING_LENGTH  11

#define UNINITIALIZED_GP_IDENTITY_VALUE (-10000)
#define IS_QUERY_DISPATCHER() (GpIdentity.segindex == MASTER_CONTENT_ID)

#define IS_QUERY_EXECUTOR_BACKEND() (Gp_role == GP_ROLE_EXECUTE && gp_session_id > 0)

/* Stores the listener port that this process uses to listen for incoming
 * Interconnect connections from other Motion nodes.
 */
extern uint32 Gp_listener_port;

/*
 * Thread-safe routine to write to the log
 */
extern void write_log(const char *fmt,...) pg_attribute_printf(1, 2);


extern void increment_command_count(void);

/* default to RANDOM distribution for CREATE TABLE without DISTRIBUTED BY */
extern bool gp_create_table_random_default_distribution;

/* Functions in guc_gp.c to lookup values in enum GUCs */
extern const char * lookup_autostats_mode_by_value(GpAutoStatsModeValue val);

/* notification condition name of next value, used in PGnotify */
#define CDB_NOTIFY_NEXTVAL "nextval"

/*
 * notification condition name of endpoint ack information. Used in PGnotify
 * for parallel retrieve cursor.
 */
#define CDB_NOTIFY_ENDPOINT_ACK "ack_notify"

#endif   /* CDBVARS_H */
