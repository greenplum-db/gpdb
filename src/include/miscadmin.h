/*-------------------------------------------------------------------------
 *
 * miscadmin.h
 *	  This file contains general postgres administration and initialization
 *	  stuff that used to be spread out between the following files:
 *		globals.h						global variables
 *		pdir.h							directory path crud
 *		pinit.h							postgres initialization
 *		pmod.h							processing modes
 *	  Over time, this has also become the preferred place for widely known
 *	  resource-limitation stuff, such as work_mem and check_stack_depth().
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/miscadmin.h,v 1.202 2008/04/23 13:44:59 mha Exp $
 *
 * NOTES
 *	  some of the information in this file should be moved to other files.
 *
 *-------------------------------------------------------------------------
 */
#ifndef MISCADMIN_H
#define MISCADMIN_H

#include "pgtime.h"				/* for pg_time_t */


#define PG_VERSIONSTR "postgres (Greenplum Database) " PG_VERSION "\n"
#define PG_BACKEND_VERSIONSTR "postgres (Greenplum Database) " PG_VERSION "\n"


/*****************************************************************************
 *	  System interrupt and critical section handling
 *
 * There are two types of interrupts that a running backend needs to accept
 * without messing up its state: QueryCancel (SIGINT) and ProcDie (SIGTERM).
 * In both cases, we need to be able to clean up the current transaction
 * gracefully, so we can't respond to the interrupt instantaneously ---
 * there's no guarantee that internal data structures would be self-consistent
 * if the code is interrupted at an arbitrary instant.	Instead, the signal
 * handlers set flags that are checked periodically during execution.
 *
 * The CHECK_FOR_INTERRUPTS() macro is called at strategically located spots
 * where it is normally safe to accept a cancel or die interrupt.  In some
 * cases, we invoke CHECK_FOR_INTERRUPTS() inside low-level subroutines that
 * might sometimes be called in contexts that do *not* want to allow a cancel
 * or die interrupt.  The HOLD_INTERRUPTS() and RESUME_INTERRUPTS() macros
 * allow code to ensure that no cancel or die interrupt will be accepted,
 * even if CHECK_FOR_INTERRUPTS() gets called in a subroutine.	The interrupt
 * will be held off until CHECK_FOR_INTERRUPTS() is done outside any
 * HOLD_INTERRUPTS() ... RESUME_INTERRUPTS() section.
 *
 * Special mechanisms are used to let an interrupt be accepted when we are
 * waiting for a lock or when we are waiting for command input (but, of
 * course, only if the interrupt holdoff counter is zero).	See the
 * related code for details.
 *
 * A lost connection is handled similarly, although the loss of connection
 * does not raise a signal, but is detected when we fail to write to the
 * socket. If there was a signal for a broken connection, we could make use of
 * it by setting ClientConnectionLost in the signal handler.
 *
 * A related, but conceptually distinct, mechanism is the "critical section"
 * mechanism.  A critical section not only holds off cancel/die interrupts,
 * but causes any ereport(ERROR) or ereport(FATAL) to become ereport(PANIC)
 * --- that is, a system-wide reset is forced.	Needless to say, only really
 * *critical* code should be marked as a critical section!	Currently, this
 * mechanism is only used for XLOG-related code.
 *
 *****************************************************************************/

/* in globals.c */
/* these are marked volatile because they are set by signal handlers: */
extern PGDLLIMPORT volatile bool InterruptPending;
extern volatile bool QueryCancelPending;
extern volatile bool QueryCancelCleanup; /* GPDB only */
extern volatile bool QueryFinishPending;
extern volatile bool ProcDiePending;

extern volatile bool ClientConnectionLost;

/* these are marked volatile because they are examined by signal handlers: */
extern volatile bool ImmediateInterruptOK;
extern volatile bool ImmediateDieOK;
extern volatile bool TermSignalReceived;
extern PGDLLIMPORT volatile int32 InterruptHoldoffCount;
extern PGDLLIMPORT volatile int32 CritSectionCount;

/* in tcop/postgres.c */
extern void ProcessInterrupts(const char* filename, int lineno);
extern void BackoffBackendTick(void);
extern bool gp_enable_resqueue_priority;
extern void gp_set_thread_sigmasks(void);

/* in utils/resource_manager.h */
extern bool IsResQueueEnabled(void);

/*
 * We don't want to include the entire vmem_tracker.h, and so,
 * declare the only function we use from vmem_tracker.h.
 */
extern void RedZoneHandler_DetectRunawaySession(void);

#ifndef WIN32

#ifdef USE_TEST_UTILS
#define CHECK_FOR_INTERRUPTS() \
do { \
	if (gp_test_time_slice) \
	{ \
		CHECK_TIME_SLICE(); \
	} \
\
	if (gp_simex_init && gp_simex_run && gp_simex_class == SimExESClass_Cancel && !InterruptPending) \
	{\
		SimExESSubClass subclass = SimEx_CheckInject(); \
		if (subclass == SimExESSubClass_Cancel_QueryCancel) \
		{\
			InterruptPending = true; \
			QueryCancelPending = true; \
		}\
		else if (subclass == SimExESSubClass_Cancel_ProcDie) \
		{\
			InterruptPending = true; \
			ProcDiePending = true; \
		}\
	}\
	if (InterruptPending) \
		ProcessInterrupts(__FILE__, __LINE__); \
	if (IsResQueueEnabled() && gp_enable_resqueue_priority)	\
		BackoffBackendTick(); \
	ReportOOMConsumption(); \
	RedZoneHandler_DetectRunawaySession();\
} while(0)
#else
#define CHECK_FOR_INTERRUPTS() \
do { \
	if (InterruptPending) \
		ProcessInterrupts(__FILE__, __LINE__); \
	if (IsResQueueEnabled() && gp_enable_resqueue_priority)	\
		BackoffBackendTick(); \
	ReportOOMConsumption(); \
	RedZoneHandler_DetectRunawaySession();\
} while(0)
#endif   /* USE_TEST_UTILS */

#else							/* WIN32 */

#define CHECK_FOR_INTERRUPTS() \
do { \
	if (UNBLOCKED_SIGNAL_QUEUE()) \
		pgwin32_dispatch_queued_signals(); \
	if (InterruptPending) \
		ProcessInterrupts(__FILE__, __LINE__); \
} while(0)
#endif   /* WIN32 */


#define HOLD_INTERRUPTS() \
do { \
	if (InterruptHoldoffCount < 0) \
		elog(PANIC, "Hold interrupt holdoff count is bad (%d)", InterruptHoldoffCount); \
	InterruptHoldoffCount++; \
} while(0)

#define RESUME_INTERRUPTS() \
do { \
	if (InterruptHoldoffCount <= 0) \
		elog(PANIC, "Resume interrupt holdoff count is bad (%d)", InterruptHoldoffCount); \
	InterruptHoldoffCount--; \
} while(0)

#define START_CRIT_SECTION() \
do { \
	if (CritSectionCount < 0) \
		elog(PANIC, "Start critical section count is bad (%d)", CritSectionCount); \
	CritSectionCount++; \
} while(0)

#define END_CRIT_SECTION() \
do { \
	if (CritSectionCount <= 0) \
		elog(PANIC, "End critical section count is bad (%d)", CritSectionCount); \
	CritSectionCount--; \
} while(0)

/* check CritSectionCount without modification */
#define ASSERT_IN_CRIT_SECTION() \
do { \
	Assert(CritSectionCount > 0); \
} while(0)

/*****************************************************************************
 *	  globals.h --															 *
 *****************************************************************************/

/*
 * from utils/init/globals.c
 */
extern pid_t PostmasterPid;
extern bool IsPostmasterEnvironment;
extern PGDLLIMPORT bool IsUnderPostmaster;
extern bool IsBinaryUpgrade;

/* Are we binary-upgrading a QE node? */
#define IsBinaryUpgradeQE() (IsBinaryUpgrade && GpIdentity.segindex >= 0)

extern bool ExitOnAnyError;

extern PGDLLIMPORT char *DataDir;

extern PGDLLIMPORT int NBuffers;
extern int	MaxBackends;
extern int	MaxConnections;
extern int gp_workfile_max_entries;

extern PGDLLIMPORT int MyProcPid;
extern PGDLLIMPORT pg_time_t MyStartTime;
extern PGDLLIMPORT struct Port *MyProcPort;
extern long MyCancelKey;
extern int	MyPMChildSlot;

extern char OutputFileName[];
extern PGDLLIMPORT char my_exec_path[];
extern char pkglib_path[];

#ifdef EXEC_BACKEND
extern char postgres_exec_path[];
#endif

extern PGDLLIMPORT int gpperfmon_port; 

/* for pljava */
extern PGDLLIMPORT char* pljava_vmoptions;
extern PGDLLIMPORT char* pljava_classpath;
extern PGDLLIMPORT int   pljava_statement_cache_size;
extern PGDLLIMPORT bool  pljava_debug;
extern PGDLLIMPORT bool  pljava_release_lingering_savepoints;
extern PGDLLIMPORT bool  pljava_classpath_insecure;

/*
 * done in storage/backendid.h for now.
 *
 * extern BackendId    MyBackendId;
 */
extern PGDLLIMPORT Oid MyDatabaseId;

extern PGDLLIMPORT Oid MyDatabaseTableSpace;

/*
 * Date/Time Configuration
 *
 * DateStyle defines the output formatting choice for date/time types:
 *	USE_POSTGRES_DATES specifies traditional Postgres format
 *	USE_ISO_DATES specifies ISO-compliant format
 *	USE_SQL_DATES specifies Oracle/Ingres-compliant format
 *	USE_GERMAN_DATES specifies German-style dd.mm/yyyy
 *
 * DateOrder defines the field order to be assumed when reading an
 * ambiguous date (anything not in YYYY-MM-DD format, with a four-digit
 * year field first, is taken to be ambiguous):
 *	DATEORDER_YMD specifies field order yy-mm-dd
 *	DATEORDER_DMY specifies field order dd-mm-yy ("European" convention)
 *	DATEORDER_MDY specifies field order mm-dd-yy ("US" convention)
 *
 * In the Postgres and SQL DateStyles, DateOrder also selects output field
 * order: day comes before month in DMY style, else month comes before day.
 *
 * The user-visible "DateStyle" run-time parameter subsumes both of these.
 */

/* valid DateStyle values */
#define USE_POSTGRES_DATES		0
#define USE_ISO_DATES			1
#define USE_SQL_DATES			2
#define USE_GERMAN_DATES		3
#define USE_XSD_DATES			4

/* valid DateOrder values */
#define DATEORDER_YMD			0
#define DATEORDER_DMY			1
#define DATEORDER_MDY			2

extern int	DateStyle;
extern int	DateOrder;

/*
 * IntervalStyles
 *	 INTSTYLE_POSTGRES			   Like Postgres < 8.4 when DateStyle = 'iso'
 *	 INTSTYLE_POSTGRES_VERBOSE	   Like Postgres < 8.4 when DateStyle != 'iso'
 *	 INTSTYLE_SQL_STANDARD		   SQL standard interval literals
 *	 INTSTYLE_ISO_8601			   ISO-8601-basic formatted intervals
 */
#define INTSTYLE_POSTGRES			0
#define INTSTYLE_POSTGRES_VERBOSE	1
#define INTSTYLE_SQL_STANDARD		2
#define INTSTYLE_ISO_8601			3

extern int	IntervalStyle;

/*
 * HasCTZSet is true if user has set timezone as a numeric offset from UTC.
 * If so, CTimeZone is the timezone offset in seconds (using the Unix-ish
 * sign convention, ie, positive offset is west of UTC, rather than the
 * SQL-ish convention that positive is east of UTC).
 */
extern bool HasCTZSet;
extern int	CTimeZone;

#define MAXTZLEN		10		/* max TZ name len, not counting tr. null */

extern bool enableFsync;
extern bool allowSystemTableModsDDL;
extern bool allowSystemTableModsDML;
extern PGDLLIMPORT int planner_work_mem;
extern PGDLLIMPORT int work_mem;
extern PGDLLIMPORT int maintenance_work_mem;
extern PGDLLIMPORT int statement_mem;
extern uint64 GetStatementMem(void);
extern PGDLLIMPORT int max_statement_mem;
extern PGDLLIMPORT int gp_vmem_limit_per_query;

extern int	VacuumCostPageHit;
extern int	VacuumCostPageMiss;
extern int	VacuumCostPageDirty;
extern int	VacuumCostLimit;
extern int	VacuumCostDelay;

extern int	VacuumCostBalance;
extern bool VacuumCostActive;

extern int gp_vmem_protect_limit;
extern int gp_vmem_protect_gang_cache_limit;

/* in tcop/postgres.c */

#if defined(__ia64__) || defined(__ia64)
typedef struct
{
	char	   *stack_base_ptr;
	char	   *register_stack_base_ptr;
} pg_stack_base_t;
#else
typedef char *pg_stack_base_t;
#endif

extern pg_stack_base_t set_stack_base(void);
extern void restore_stack_base(pg_stack_base_t base);
extern void check_stack_depth(void);


/*****************************************************************************
 *	  pdir.h --																 *
 *			POSTGRES directory path definitions.							 *
 *****************************************************************************/

/* flags to be OR'd to form sec_context */
#define SECURITY_LOCAL_USERID_CHANGE	0x0001
#define SECURITY_RESTRICTED_OPERATION	0x0002

extern char *DatabasePath;

/* now in utils/init/miscinit.c */
extern void SetDatabasePath(const char *path);

extern char *GetUserNameFromId(Oid roleid);
extern Oid	GetUserId(void);
extern Oid	GetOuterUserId(void);
extern Oid	GetSessionUserId(void);
extern void 	SetSessionUserId(Oid, bool);
extern Oid	GetAuthenticatedUserId(void);
extern bool IsAuthenticatedUserSuperUser(void);
extern void GetUserIdAndSecContext(Oid *userid, int *sec_context);
extern void SetUserIdAndSecContext(Oid userid, int sec_context);
extern bool InLocalUserIdChange(void);
extern bool InSecurityRestrictedOperation(void);
extern void GetUserIdAndContext(Oid *userid, bool *sec_def_context);
extern void SetUserIdAndContext(Oid userid, bool sec_def_context);
extern void InitializeSessionUserId(const char *rolename);
extern void InitializeSessionUserIdStandalone(void);
extern void SetSessionAuthorization(Oid userid, bool is_superuser);
extern Oid	GetCurrentRoleId(void);
extern void SetCurrentRoleId(Oid roleid, bool is_superuser);

extern void SetDataDir(const char *dir);
extern void ChangeToDataDir(void);
extern char *make_absolute_path(const char *path);

/* in utils/misc/superuser.c */
extern bool superuser(void);	/* current user is superuser */
extern bool procRoleIsSuperuser(void); /* proc role id is superuser */
extern bool superuser_arg(Oid roleid);	/* given user is superuser */


/*****************************************************************************
 *	  pmod.h --																 *
 *			POSTGRES processing mode definitions.							 *
 *****************************************************************************/

/*
 * Description:
 *		There are three processing modes in POSTGRES.  They are
 * BootstrapProcessing or "bootstrap," InitProcessing or
 * "initialization," and NormalProcessing or "normal."
 *
 * The first two processing modes are used during special times. When the
 * system state indicates bootstrap processing, transactions are all given
 * transaction id "one" and are consequently guaranteed to commit. This mode
 * is used during the initial generation of template databases.
 *
 * Initialization mode: used while starting a backend, until all normal
 * initialization is complete.	Some code behaves differently when executed
 * in this mode to enable system bootstrapping.
 *
 * If a POSTGRES binary is in normal mode, then all code may be executed
 * normally.
 */

typedef enum ProcessingMode
{
	BootstrapProcessing,		/* bootstrap creation of template database */
	InitProcessing,				/* initializing system */
	NormalProcessing			/* normal processing */
} ProcessingMode;

extern ProcessingMode Mode;

#define IsBootstrapProcessingMode() ((bool)(Mode == BootstrapProcessing))
#define IsInitProcessingMode() ((bool)(Mode == InitProcessing))
#define IsNormalProcessingMode() ((bool)(Mode == NormalProcessing))

#define SetProcessingMode(mode) \
	do { \
		AssertArg((mode) == BootstrapProcessing || \
				  (mode) == InitProcessing || \
				  (mode) == NormalProcessing); \
		Mode = (mode); \
	} while(0)

#define GetProcessingMode() Mode


/*****************************************************************************
 *	  pinit.h --															 *
 *			POSTGRES initialization and cleanup definitions.				 *
 *****************************************************************************/

/* in utils/init/postinit.c */
extern bool FindMyDatabase(const char *dbname, Oid *db_id, Oid *db_tablespace);
extern void pg_split_opts(char **argv, int *argcp, char *optstr);
extern void InitPostgres(const char *in_dbname, Oid dboid, const char *username,
			 char *out_dbname);
extern void BaseInit(void);

/* in utils/init/miscinit.c */
extern bool IgnoreSystemIndexes;
extern PGDLLIMPORT bool process_shared_preload_libraries_in_progress;
extern char *shared_preload_libraries_string;
extern char *local_preload_libraries_string;

extern void SetReindexProcessing(Oid heapOid, Oid indexOid);
extern void ResetReindexProcessing(void);
extern bool ReindexIsProcessingHeap(Oid heapOid);
extern bool ReindexIsProcessingIndex(Oid indexOid);
extern void CreateDataDirLockFile(bool amPostmaster);
extern void CreateSocketLockFile(const char *socketfile, bool amPostmaster);
extern void TouchSocketLockFile(void);
extern void RecordSharedMemoryInLockFile(unsigned long id1,
							 unsigned long id2);
extern void ValidatePgVersion(const char *path);
extern void process_shared_preload_libraries(void);
extern void process_local_preload_libraries(void);
extern void pg_bindtextdomain(const char *domain);

extern int64 db_dir_size(const char *path); /* implemented in dbsize.c */

/*
 * Auxiliary-process type identifiers.  These used to be in bootstrap.h
 * but it seems saner to have them here, with the ProcessingMode stuff.
 * The MyAuxProcType global is defined and set in bootstrap.c.
 */
typedef enum
{
	NotAnAuxProcess = -1,
	CheckerProcess = 0,
	BootstrapProcess,
	StartupProcess,
	StartupPass2Process,
	StartupPass3Process,
	StartupPass4Process,
	BgWriterProcess,
	CheckpointerProcess,
	WalWriterProcess,
	WalReceiverProcess,
	FilerepProcess,
	FilerepResetPeerProcess
} AuxProcType;

extern AuxProcType MyAuxProcType; /* bootstrap.c */
#define AmBootstrapProcess()        (MyAuxProcType == BootstrapProcess)
#define AmStartupProcess()          (MyAuxProcType >= StartupProcess && MyAuxProcType <= StartupPass4Process)
#define AmBackgroundWriterProcess() (MyAuxProcType == BgWriterProcess)
#define AmCheckpointerProcess()     (MyAuxProcType == CheckpointerProcess)
#define AmWalWriterProcess()        (MyAuxProcType == WalWriterProcess)
#define AmWalReceiverProcess()      (MyAuxProcType == WalReceiverProcess)

/* in access/transam/xlog.c */
extern bool BackupInProgress(void);
extern void CancelBackup(void);

#endif   /* MISCADMIN_H */
