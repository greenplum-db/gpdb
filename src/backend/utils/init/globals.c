/*-------------------------------------------------------------------------
 *
 * globals.c
 *	  global variable declarations
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/utils/init/globals.c,v 1.107 2009/01/01 17:23:51 momjian Exp $
 *
 * NOTES
 *	  Globals used all over the place should be declared here and not
 *	  in other modules.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "pgtime.h"
#include "libpq/pqcomm.h"
#include "miscadmin.h"
#include "storage/backendid.h"


ProtocolVersion FrontendProtocol = PG_PROTOCOL_LATEST;

volatile bool InterruptPending = false;
volatile bool QueryCancelPending = false;
volatile bool QueryCancelCleanup = false;
volatile bool QueryFinishPending = false;
volatile bool ProcDiePending = false;
volatile bool ClientConnectionLost = false;
volatile bool ImmediateInterruptOK = false;
volatile bool ImmediateDieOK = false;
volatile bool TermSignalReceived = false;

// Make these signed integers (instead of uint32) to detect garbage negative values.
volatile int32 InterruptHoldoffCount = 0;
volatile int32 CritSectionCount = 0;

int			MyProcPid;
pg_time_t	MyStartTime;
struct Port *MyProcPort;
long		MyCancelKey;
int			MyPMChildSlot = -1;

/*
 * DataDir is the absolute path to the top level of the PGDATA directory tree.
 * Except during early startup, this is also the server's working directory;
 * most code therefore can simply use relative paths and not reference DataDir
 * explicitly.
 */
char	   *DataDir = NULL;

char		OutputFileName[MAXPGPATH];	/* debugging output file */

char		my_exec_path[MAXPGPATH];	/* full path to my executable */
char		pkglib_path[MAXPGPATH];		/* full path to lib directory */

#ifdef EXEC_BACKEND
char		postgres_exec_path[MAXPGPATH];		/* full path to backend */

/* note: currently this is not valid in backend processes */
#endif

BackendId	MyBackendId = InvalidBackendId;

Oid			MyDatabaseId = InvalidOid;

Oid			MyDatabaseTableSpace = InvalidOid;

/*
 * DatabasePath is the path (relative to DataDir) of my database's
 * primary directory, ie, its directory in the default tablespace.
 */
char	   *DatabasePath = NULL;

pid_t		PostmasterPid = 0;

/*
 * IsPostmasterEnvironment is true in a postmaster process and any postmaster
 * child process; it is false in a standalone process (bootstrap or
 * standalone backend).  IsUnderPostmaster is true in postmaster child
 * processes.  Note that "child process" includes all children, not only
 * regular backends.  These should be set correctly as early as possible
 * in the execution of a process, so that error handling will do the right
 * things if an error should occur during process initialization.
 *
 * These are initialized for the bootstrap/standalone case.
 */
bool		IsPostmasterEnvironment = false;
bool		IsUnderPostmaster = false;
bool		IsBinaryUpgrade = false;
bool		IsBackgroundWorker = false; /* Greenplum specific bgworker */

bool		ExitOnAnyError = false;

int			DateStyle = USE_ISO_DATES;
int			DateOrder = DATEORDER_MDY;
int			IntervalStyle = INTSTYLE_POSTGRES;
bool		HasCTZSet = false;
int			CTimeZone = 0;

bool		enableFsync = true;
bool		allowSystemTableModsDDL = false;
bool		allowSystemTableModsDML = false;
int			planner_work_mem = 32768;
int			work_mem = 32768;
int			statement_mem = 256000;
int			max_statement_mem = 2048000;
/*
 * gp_vmem_limit_per_query set to 0 means we
 * do not enforce per-query memory limit
 */
int			gp_vmem_limit_per_query = 0;
int			maintenance_work_mem = 65536;

/*
 * Primary determinants of sizes of shared-memory structures.  MaxBackends is
 * MaxConnections + autovacuum_max_workers (it is computed by the GUC assign
 * hook):
 */
int			NBuffers = 4096;
int			MaxBackends = 200;
int			MaxConnections = 90;

int			gp_workfile_max_entries = 8192; /* Number of unique entries we can hold in the workfile directory */

int			VacuumCostPageHit = 1;		/* GUC parameters for vacuum */
int			VacuumCostPageMiss = 10;
int			VacuumCostPageDirty = 20;
int			VacuumCostLimit = 200;
int			VacuumCostDelay = 0;

int			VacuumCostBalance = 0;		/* working state for vacuum */
bool		VacuumCostActive = false;

int			GinFuzzySearchLimit = 0;

/* gpperfmon port number */
int 	gpperfmon_port = 8888;

/* for pljava */
char*	pljava_vmoptions = NULL;
char*	pljava_classpath = NULL;
int		pljava_statement_cache_size 	= 512;
bool	pljava_release_lingering_savepoints = false;
bool	pljava_debug = false;
bool	pljava_classpath_insecure = false;


/* Memory protection GUCs*/
#ifdef __darwin__
int gp_vmem_protect_limit = 0; 
#else
int gp_vmem_protect_limit = 8192;
#endif
int gp_vmem_protect_gang_cache_limit = 500;

