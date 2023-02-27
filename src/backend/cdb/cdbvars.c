/*-------------------------------------------------------------------------
 *
 * cdbvars.c
 *	  Provides storage areas and processing routines for Greenplum Database variables
 *	  managed by GUC.
 *
 * Portions Copyright (c) 2003-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbvars.c
 *
 *
 * NOTES
 *	  See src/backend/utils/misc/guc.c for variable external specification.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "miscadmin.h"
#include "utils/guc.h"
#include "cdb/cdbvars.h"
#include "libpq-fe.h"
#include "libpq-int.h"
#include "cdb/cdbfts.h"
#include "cdb/cdbdisp.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbdisp.h"
#include "lib/stringinfo.h"
#include "libpq/libpq-be.h"
#include "postmaster/backoff.h"
#include "utils/resource_manager.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "cdb/memquota.h"
#include "postmaster/fts.h"

/*
 * ----------------
 *		GUC/global variables
 *
 *	Initial values are set by guc.c function "InitializeGUCOptions" called
 *	*very* early during postmaster, postgres, or bootstrap initialization.
 * ----------------
 */



GpRoleValue Gp_role;			/* Role paid by this Greenplum Database
								 * backend */
char	   *gp_role_string;		/* Staging area for guc.c */

bool		Gp_is_writer;		/* is this qExec a "writer" process. */

int			gp_session_id;		/* global unique id for session. */

char	   *qdHostname;			/* QD hostname */
int			qdPostmasterPort;	/* Master Segment Postmaster port. */

int			gp_command_count;	/* num of commands from client */

bool		gp_debug_pgproc;	/* print debug info for PGPROC */
bool		Debug_print_prelim_plan;	/* Shall we log plan before adding
										 * Motions to subplans? */

bool		Debug_print_slice_table;	/* Shall we log the slice table? */

bool		Debug_resource_group = false;	/* Shall we log the resource group? */

bool		Debug_burn_xids;

bool		gp_external_enable_exec = true; /* allow ext tables with EXECUTE */

bool		verify_gpfdists_cert; /* verifies gpfdist's certificate */

int			gp_external_max_segs;	/* max segdbs per gpfdist/gpfdists URI */

int			gp_cached_gang_threshold;	/* How many gangs to keep around from
										 * stmt to stmt. */

bool		Gp_write_shared_snapshot;	/* tell the writer QE to write the
										 * shared snapshot */

bool		gp_reraise_signal = false;	/* try to dump core when we get
										 * SIGABRT & SIGSEGV */

bool		gp_set_proc_affinity = false;	/* set processor affinity (if
											 * platform supports it) */

int			gp_reject_percent_threshold;	/* SREH reject % kicks off only
											 * after * <num> records have been
											 * processed	*/

bool		gp_select_invisible = false;	/* debug mode to allow select to
											 * see "invisible" rows */

int         gp_segment_connect_timeout = 180;  /* Maximum time (in seconds) allowed 
												* for a new worker process to start
												* or a mirror to respond.
												*/

/*
 * Configurable timeout for snapshot add: exceptionally busy systems may take
 * longer than our old hard-coded version -- so here is a tuneable version.
 */
int			gp_snapshotadd_timeout = 10;


/*
 * Probe retry count for fts prober.
 */
int			gp_fts_probe_retries = 5;

/*
 * Probe timeout for fts prober.
 */
int			gp_fts_probe_timeout = 20;

/*
 * Polling interval for the fts prober. A scan of the entire system starts
 * every time this expires.
 */
int			gp_fts_probe_interval = 60;

/*
 * If mirror disconnects and re-connects between this period, or just takes
 * this much time during initial connection of cluster start, it will not get
 * reported as down to FTS.
 */
int gp_fts_mark_mirror_down_grace_period = 30;

/*
 * If primary-mirror replication attempts to connect continuously and exceed
 * this count, mark the mirror down to prevent wal sync block.
 * More details please refer to FTSGetReplicationDisconnectTime.
 */
int			gp_fts_replication_attempt_count = 10;

/*
 * Polling interval for the dtx recovery. Checking in dtx recovery starts every
 * time this expires.
 */
int			gp_dtx_recovery_interval = 60;

/*
 * Gather prepared transactions that live longer than the time to find possible
 * orphaned prepared transactions.
 */
int			gp_dtx_recovery_prepared_period = 300;

/*
 * When we have certain types of failures during gang creation which indicate
 * that a segment is in recovery mode we may be able to retry.
 */
int			gp_gang_creation_retry_count = 5;	/* disable by default */
int			gp_gang_creation_retry_timer = 2000;	/* 2000ms */

/*
 * These are GUCs to tune the TCP_KEEPALIVE parameters
 * for QD/QE libpq connections
 */
int			gp_dispatch_keepalives_idle = 0;
int			gp_dispatch_keepalives_interval = 0;
int			gp_dispatch_keepalives_count = 0;

/*
 * gp_enable_slow_writer_testmode
 *
 * In order facilitate testing of reader-gang/writer-gang synchronization,
 * this inserts a pg_usleep call at the start of writer-gang processing.
 */
bool		gp_enable_slow_writer_testmode = false;

/*
 * gp_enable_slow_cursor_testmode
 *
 * In order facilitate testing of reader-gang/writer-gang synchronization,
 * this inserts a pg_usleep call at the start of cursor-gang processing.
 */
bool		gp_enable_slow_cursor_testmode = false;

/*
 * TCP port the Interconnect listens on for incoming connections from other
 * backends.  Assigned by initMotionLayerIPC() at process startup.  This port
 * is used for the duration of this process and should never change.
 */
uint32		Gp_listener_port;

int			Gp_max_packet_size; /* max Interconnect packet size */

int			Gp_interconnect_queue_depth = 4;	/* max number of messages
												 * waiting in rx-queue before
												 * we drop. */
int			Gp_interconnect_snd_queue_depth = 2;
int			Gp_interconnect_timer_period = 5;
int			Gp_interconnect_timer_checking_period = 20;
int			Gp_interconnect_default_rtt = 20;
int			Gp_interconnect_min_rto = 20;
int			Gp_interconnect_fc_method = INTERCONNECT_FC_METHOD_LOSS;
int			Gp_interconnect_transmit_timeout = 3600;
int			Gp_interconnect_min_retries_before_timeout = 100;
int			Gp_interconnect_debug_retry_interval = 10;

int			interconnect_setup_timeout = 7200;

int			Gp_interconnect_type = INTERCONNECT_TYPE_UDPIFC;
int 		Gp_interconnect_address_type = INTERCONNECT_ADDRESS_TYPE_UNICAST;

bool		gp_interconnect_aggressive_retry = true;	/* fast-track app-level
														 * retry */

bool		gp_interconnect_full_crc = false;	/* sanity check UDP data. */

bool		gp_interconnect_log_stats = false;	/* emit stats at log-level */

bool		gp_interconnect_cache_future_packets = true;

/*
 * format: dbid:content:address:port,dbid:content:address:port ...
 * example: 1:-1:10.0.0.1:2000 2:0:10.0.0.2:2000 3:1:10.0.0.2:2001
 *
 * FIXME: at the moment:
 * - the address must be specified as IP;
 */
char	   *gp_interconnect_proxy_addresses = NULL;

int			Gp_udp_bufsize_k;	/* UPD recv buf size, in KB */

#ifdef USE_ASSERT_CHECKING
/*
 * UDP-IC Test hooks (for fault injection).
 *
 * Dropseg: specifies which segment to apply the drop_percent to.
 */
int			gp_udpic_dropseg = UNDEF_SEGMENT;
int			gp_udpic_dropxmit_percent = 0;
int			gp_udpic_dropacks_percent = 0;
int			gp_udpic_fault_inject_percent = 0;
int			gp_udpic_fault_inject_bitmap = 0;
int			gp_udpic_network_disable_ipv6 = 0;
#endif

/*
 * Each slice table has a unique ID (certain commands like "vacuum analyze"
 * run many many slice-tables for each gp_command_id).
 */
uint32		gp_interconnect_id = 0;

/* --------------------------------------------------------------------------------------------------
 * Greenplum Optimizer GUCs
 */

double		gp_motion_cost_per_row = 0;
int			gp_segments_for_planner = 0;

int			gp_hashagg_default_nbatches = 32;

bool		gp_adjust_selectivity_for_outerjoins = true;
bool		gp_selectivity_damping_for_scans = false;
bool		gp_selectivity_damping_for_joins = false;
double		gp_selectivity_damping_factor = 1;
bool		gp_selectivity_damping_sigsort = true;

int			gp_hashjoin_tuples_per_bucket = 5;
int			gp_hashagg_groups_per_bucket = 5;

/* Analyzing aid */
int			gp_motion_slice_noop = 0;

/* Greenplum Database Experimental Feature GUCs */
bool		gp_enable_explain_allstat = false;
bool		gp_enable_motion_deadlock_sanity = false;	/* planning time sanity
														 * check */

bool		gp_enable_tablespace_auto_mkdir = false;

/* Enable check for compatibility of encoding and locale in createdb */
bool		gp_encoding_check_locale_compatibility = true;

/* Priority for the segworkers relative to the postmaster's priority */
int			gp_segworker_relative_priority = PRIO_MAX;

/* Max size of dispatched plans; 0 if no limit */
int			gp_max_plan_size = 0;

/* Disable setting of tuple hints while reading */
bool		gp_disable_tuple_hints = false;

int			gp_workfile_compress_algorithm = 0;
bool		gp_workfile_checksumming = false;
int			gp_workfile_caching_loglevel = DEBUG1;
int			gp_sessionstate_loglevel = DEBUG1;

/* Maximum disk space to use for workfiles on a segment, in kilobytes */
int			gp_workfile_limit_per_segment = 0;

/* Maximum disk space to use for workfiles per query on a segment, in kilobytes */
int			gp_workfile_limit_per_query = 0;

/* Maximum number of workfiles to be created by a query */
int			gp_workfile_limit_files_per_query = 0;

/* Enable single-slice single-row inserts ?*/
bool		gp_enable_fast_sri = true;

/* Enable single-mirror pair dispatch. */
bool		gp_enable_direct_dispatch = true;

/* Force core dump on memory context error */
bool		coredump_on_memerror = false;

/* Experimental feature for MPP-4082. Please read doc before setting this guc */
int			gp_autostats_mode;
char	   *gp_autostats_mode_string;
int			gp_autostats_mode_in_functions;
char	   *gp_autostats_mode_in_functions_string;
int			gp_autostats_on_change_threshold = 100000;
bool		gp_autostats_allow_nonowner = false;
bool		log_autostats = true;

/* GUC to toggle JIT instrumentation output for EXPLAIN */
bool		gp_explain_jit = true;

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
int			gp_debug_linger = 30;

/* ----------------
 * Non-GUC globals
 */

int			currentSliceId = UNSET_SLICE_ID;	/* used by elog to show the
												 * current slice the process
												 * is executing. */

bool		gp_cost_hashjoin_chainwalk = false;

/* ----------------
 * This variable is initialized by the postmaster from command line arguments
 *
 * Any code needing the "numsegments"
 * can simply #include cdbvars.h, and use GpIdentity.numsegments
 */
GpId		GpIdentity = {UNINITIALIZED_GP_IDENTITY_VALUE, UNINITIALIZED_GP_IDENTITY_VALUE};

/*
 * Keep track of a few dispatch-related  statistics:
 */
int			cdb_total_slices = 0;
int			cdb_total_plans = 0;
int			cdb_max_slices = 0;

/*
 * Local macro to provide string values of numeric defines.
 */
#define CppNumericAsString(s) CppAsString(s)

/*
 *	Forward declarations of local function.
 */
static GpRoleValue string_to_role(const char *string);


/*
 * Convert a Greenplum Database role string (as for gp_role) to an
 * enum value of type GpRoleValue. Return GP_ROLE_UNDEFINED in case the
 * string is unrecognized.
 */
static GpRoleValue
string_to_role(const char *string)
{
	GpRoleValue role = GP_ROLE_UNDEFINED;

	if (pg_strcasecmp(string, "dispatch") == 0)
		role = GP_ROLE_DISPATCH;
	else if (pg_strcasecmp(string, "execute") == 0)
		role = GP_ROLE_EXECUTE;
	else if (pg_strcasecmp(string, "utility") == 0)
		role = GP_ROLE_UTILITY;

	return role;
}

/*
 * Convert a GpRoleValue to a role string (as for gp_role).  Return eyecatcher
 * in the unexpected event that the value is unknown or undefined.
 */
const char *
role_to_string(GpRoleValue role)
{
	switch (role)
	{
		case GP_ROLE_DISPATCH:
			return "dispatch";
		case GP_ROLE_EXECUTE:
			return "execute";
		case GP_ROLE_UTILITY:
			return "utility";
		case GP_ROLE_UNDEFINED:
		default:
			return "undefined";
	}
}

bool
check_gp_role(char **newval, void **extra, GucSource source)
{
	GpRoleValue newrole = string_to_role(*newval);

	/* Force utility mode in a stand-alone backend. */
	if (!IsPostmasterEnvironment && newrole != GP_ROLE_UTILITY)
	{
		elog(LOG, "gp_role forced to 'utility' in single-user mode");
		*newval = strdup("utility");
		return true;
	}

	if (source == PGC_S_DEFAULT)
		return true;
	else if (Gp_role == GP_ROLE_UNDEFINED)
		return (newrole != GP_ROLE_UNDEFINED);
	else /* can only downgrade to utility. */
		return (newrole == GP_ROLE_UTILITY);
}

void
assign_gp_role(const char *newval, void *extra)
{
	Gp_role = string_to_role(newval);

	if (Gp_role == GP_ROLE_UTILITY && MyProc != NULL)
		MyProc->mppIsWriter = false;
}

/*
 * Show hook routine for "gp_role" option.
 *
 * See src/backend/util/misc/guc.c for option definition.
 */
const char *
show_gp_role(void)
{
	return role_to_string(Gp_role);
}



/* --------------------------------------------------------------------------------------------------
 * Logging
 */


/*
 * gp_log_gangs (string)
 *
 * Should creation, reallocation and cleanup of gangs of QE processes be logged?
 * "OFF"	 -> only errors are logged
 * "TERSE"	 -> terse logging of routine events, e.g. creation of new qExecs
 * "VERBOSE" -> gang allocation per command is logged
 * "DEBUG"	 -> additional events are logged at severity level DEBUG1 to DEBUG5
 *
 * The messages that are enabled by the TERSE and VERBOSE settings are
 * written with a severity level of LOG.
 */
int gp_log_gang;

/*
 * gp_log_fts (string)
 *
 * What kind of messages should the fault-prober log ?
 * "OFF"	 -> only errors are logged
 * "TERSE"	 -> terse logging of routine events
 * "VERBOSE" -> gang allocation per command is logged
 * "DEBUG"	 -> additional events are logged at severity level DEBUG1 to DEBUG5
 *
 * The messages that are enabled by the TERSE and VERBOSE settings are
 * written with a severity level of LOG.
 */
int gp_log_fts;

/*
 * gp_log_interconnect (string)
 *
 * Should connections between internal processes be logged?  (qDisp/qExec/etc)
 * "OFF"	 -> connection errors are logged
 * "TERSE"	 -> terse logging of routine events, e.g. successful connections
 * "VERBOSE" -> most interconnect setup events are logged
 * "DEBUG"	 -> additional events are logged at severity level DEBUG1 to DEBUG5.
 *
 * The messages that are enabled by the TERSE and VERBOSE settings are
 * written with a severity level of LOG.
 */
int gp_log_interconnect;

/*
 * gpvars_check_gp_resource_manager_policy
 * gpvars_assign_gp_resource_manager_policy
 * gpvars_show_gp_resource_manager_policy
 */
bool
gpvars_check_gp_resource_manager_policy(char **newval, void **extra, GucSource source)
{
	if (*newval == NULL ||
		*newval[0] == 0 ||
		!pg_strcasecmp("queue", *newval) ||
		!pg_strcasecmp("group", *newval) ||
		!pg_strcasecmp("group-v2", *newval))
		return true;

	GUC_check_errmsg("invalid value for resource manager policy: \"%s\"", *newval);
	return false;
}

void
gpvars_assign_gp_resource_manager_policy(const char *newval, void *extra)
{
	if (newval == NULL || newval[0] == 0)
		Gp_resource_manager_policy = RESOURCE_MANAGER_POLICY_QUEUE;
	else if (!pg_strcasecmp("queue", newval))
		Gp_resource_manager_policy = RESOURCE_MANAGER_POLICY_QUEUE;
	else if (!pg_strcasecmp("group", newval))
	{
		Gp_resource_manager_policy = RESOURCE_MANAGER_POLICY_GROUP;
		gp_enable_resqueue_priority = false;
	}
	else if (!pg_strcasecmp("group-v2", newval))
	{
		Gp_resource_manager_policy = RESOURCE_MANAGER_POLICY_GROUP_V2;
		gp_enable_resqueue_priority = false;
	}
	/*
	 * No else should happen, since newval has been checked in check_hook.
	 */
}

const char *
gpvars_show_gp_resource_manager_policy(void)
{
	switch (Gp_resource_manager_policy)
	{
		case RESOURCE_MANAGER_POLICY_QUEUE:
			return "queue";
		case RESOURCE_MANAGER_POLICY_GROUP:
			return "group";
		case RESOURCE_MANAGER_POLICY_GROUP_V2:
			return "group-v2";
		default:
			Assert(!"unexpected resource manager policy");
			return "unknown";
	}
}

/*
 * gpvars_assign_statement_mem
 */
bool
gpvars_check_statement_mem(int *newval, void **extra, GucSource source)
{
	if (*newval >= max_statement_mem)
	{
		GUC_check_errmsg("Invalid input for statement_mem, must be less than max_statement_mem (%d kB)",
						 max_statement_mem);
		return false;
	}

	return true;
}

/*
 * increment_command_count
 *	  Increment gp_command_count. If the new command count is 0 or a negative number, reset it to 1.
 *	  And keep MyProc->queryCommandId synced with gp_command_count.
 */
void
increment_command_count()
{
	gp_command_count++;
	if (gp_command_count <= 0)
		gp_command_count = 1;

	/*
	 * No need to maintain MyProc->queryCommandId elsewhere, we guarantee
	 * they are always synced here.
	 */
	MyProc->queryCommandId = gp_command_count;
}

Datum mpp_execution_segment(PG_FUNCTION_ARGS);
Datum gp_execution_dbid(PG_FUNCTION_ARGS);

/*
 * Implements the gp_execution_segment() function to return the contentid
 * of the current executing segment.
 */
Datum
mpp_execution_segment(PG_FUNCTION_ARGS)
{
	PG_RETURN_INT32(GpIdentity.segindex);
}

/*
 * Implements the gp_execution_dbid() function to return the dbid of the
 * current executing segment.
 */
Datum
gp_execution_dbid(PG_FUNCTION_ARGS)
{
	PG_RETURN_INT32(GpIdentity.dbid);
}
