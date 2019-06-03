/*-------------------------------------------------------------------------
 *
 * gddbackend.c
 *	  Global DeadLock Detector - Backend
 *
 *
 * Copyright (c) 2018-Present Pivotal Software, Inc.
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <unistd.h>

#include "access/xact.h"
#include "access/transam.h"
#include "postmaster/fork_process.h"
#include "postmaster/postmaster.h"
#include "catalog/catalog.h"
#include "catalog/pg_authid.h"
#include "storage/ipc.h"
#include "storage/pmsignal.h"			/* PostmasterIsAlive */
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/procsignal.h"
#include "storage/backendid.h"
#include "storage/bufmgr.h"
#include "storage/sinvaladt.h"
#include "tcop/tcopprot.h"
#include "utils/gdd.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/snapmgr.h"
#include "utils/memutils.h"
#include "utils/syscache.h"
#include "utils/ps_status.h"
#include "utils/faultinjector.h"
#include "cdb/cdbvars.h"

#include "libpq/pqsignal.h"
#include "libpq/libpq-be.h"
#include "executor/spi.h"
#include "utils/sharedsnapshot.h"
#include "utils/timeout.h"

#include "gdddetector.h"
#include "gdddetectorpriv.h"

#define RET_STATUS_OK 0
#define RET_STATUS_ERROR 1

#define PGLOCKS_BATCH_SIZE 32

typedef struct VertSatelliteData
{
	int   pid;
	int   sessionid;
} VertSatelliteData;

typedef struct EdgeSatelliteData
{
	char *lockmode;
	char *locktype;
} EdgeSatelliteData;


#define GET_PID_FROM_VERT(vert) (((VertSatelliteData *) ((vert)->data))->pid)
#define GET_SESSIONID_FROM_VERT(vert) (((VertSatelliteData *) ((vert)->data))->sessionid)
#define GET_LOCKMODE_FROM_EDGE(edge) (((EdgeSatelliteData *) ((edge)->data))->lockmode)
#define GET_LOCKTYPE_FROM_EDGE(edge) (((EdgeSatelliteData *) ((edge)->data))->locktype)

#ifdef EXEC_BACKEND
static pid_t global_deadlock_detector_forkexec(void);
#endif
NON_EXEC_STATIC void GlobalDeadLockDetectorMain(int argc, char *argv[]);

static void GlobalDeadLockDetectorLoop(void);
static int  doDeadLockCheck(void);
static void buildWaitGraph(GddCtx *ctx);
static void breakDeadLock(GddCtx *ctx);
static void dumpCancelResult(StringInfo str, List *xids);
extern void dumpGddCtx(GddCtx *ctx, StringInfo str);
static void dumpGddGraph(GddGraph *graph, StringInfo str);
static void dumpGddEdge(GddEdge *edge, StringInfo str);
static void TimeoutHandler(void);

static MemoryContext	gddContext;
static MemoryContext    oldContext;

static volatile sig_atomic_t got_SIGHUP = false;
static volatile bool shutdown_requested = false;

int gp_global_deadlock_detector_period;

bool am_global_deadlock_detector = false;

/*
 * Main entry point for global deadlock detector process.
 *
 * This code is heavily based on pgarch.c, q.v.
 */
int
global_deadlock_detector_start(void)
{
	pid_t		GlobalDeadLockDetectorPID;

#ifdef EXEC_BACKEND
	switch ((GlobalDeadLockDetectorPID = global_deadlock_detector_forkexec()))
#else
	switch ((GlobalDeadLockDetectorPID = fork_process()))
#endif
	{
		case -1:
			ereport(LOG,
					(errmsg("could not fork global dead lock detector process: %m")));
			return 0;

#ifndef EXEC_BACKEND
		case 0:
			/* in postmaster child ... */
			/* Close the postmaster's sockets */
			ClosePostmasterPorts(false);

			GlobalDeadLockDetectorMain(0, NULL);
			break;
#endif
		default:
			return (int) GlobalDeadLockDetectorPID;
	}

	/* shouldn't get here */
	return 0;
}


#ifdef EXEC_BACKEND
/*
 * global_deadlock_detector_forkexec()
 *
 * Format up the arglist for the serqserver process, then fork and exec.
 */
static pid_t
global_deadlock_detector_forkexec(void)
{
	char	   *av[10];
	int			ac = 0;

	av[ac++] = "postgres";
	av[ac++] = "--globaldeadlockdetector";
	av[ac++] = NULL;			/* filled in by postmaster_forkexec */
	av[ac] = NULL;

	Assert(ac < lengthof(av));

	return postmaster_forkexec(ac, av);
}
#endif   /* EXEC_BACKEND */

static void
RequestShutdown(SIGNAL_ARGS)
{
	shutdown_requested = true;
}

/* SIGHUP: set flag to reload config file */
static void
sigHupHandler(SIGNAL_ARGS)
{
	got_SIGHUP = true;
}

/*
 * GlobalDeadLockDetectorMain
 */
NON_EXEC_STATIC void
GlobalDeadLockDetectorMain(int argc, char *argv[])
{
	sigjmp_buf	local_sigjmp_buf;
	char	   *fullpath;

	IsUnderPostmaster = true;
	am_global_deadlock_detector = true;

	/* Stay away from PMChildSlot */
	MyPMChildSlot = -1;

	/* reset MyProcPid */
	MyProcPid = getpid();

	/* record Start Time for logging */
	MyStartTime = time(NULL);

	/* Lose the postmaster's on-exit routines */
	on_exit_reset();

	/*
	 * If possible, make this process a group leader, so that the postmaster
	 * can signal any child processes too.	(gdd probably never has any
	 * child processes, but for consistency we make all postmaster child
	 * processes do this.)
	 */
#ifdef HAVE_SETSID
	if (setsid() < 0)
		elog(FATAL, "setsid() failed: %m");
#endif

	/* Identify myself via ps */
	init_ps_display("global deadlock detector process", "", "", "");

	SetProcessingMode(InitProcessing);

	pqsignal(SIGHUP, sigHupHandler);
	pqsignal(SIGINT, StatementCancelHandler);
	pqsignal(SIGTERM, die);
	pqsignal(SIGQUIT, quickdie); /* we don't do any seq-server specific cleanup, just use the standard. */
	InitializeTimeouts();

	pqsignal(SIGPIPE, SIG_IGN);
	pqsignal(SIGUSR1, procsignal_sigusr1_handler);
	/* We don't listen for async notifies */
	pqsignal(SIGUSR2, RequestShutdown);
	pqsignal(SIGFPE, FloatExceptionHandler);
	pqsignal(SIGCHLD, SIG_DFL);

	RegisterTimeout(DEADLOCK_TIMEOUT, TimeoutHandler);
	RegisterTimeout(STATEMENT_TIMEOUT, TimeoutHandler);
	RegisterTimeout(LOCK_TIMEOUT, TimeoutHandler);
	RegisterTimeout(GANG_TIMEOUT, TimeoutHandler);

	/*
	 * Create a resource owner to keep track of our resources (currently only
	 * buffer pins).
	 */
	CurrentResourceOwner = ResourceOwnerCreate(NULL, "GlobalDeadLockDetector");

	/* Early initialization */
	BaseInit();

	/* See InitPostgres()... */
	/*
	 * Create a per-backend PGPROC struct in shared memory, except in the
	 * EXEC_BACKEND case where this was done in SubPostmasterMain. We must do
	 * this before we can use LWLocks (and in the EXEC_BACKEND case we already
	 * had to do some stuff with LWLocks).
	 */
#ifndef EXEC_BACKEND
	InitProcess();
#endif
	InitBufferPoolBackend();
	InitXLOGAccess();

	SetProcessingMode(NormalProcessing);

	/* Allocate MemoryContext */
	gddContext = AllocSetContextCreate(TopMemoryContext,
									   "GddContext",
									   ALLOCSET_DEFAULT_MINSIZE,
									   ALLOCSET_DEFAULT_INITSIZE,
									   ALLOCSET_DEFAULT_MAXSIZE);

	/*
	 * If an exception is encountered, processing resumes here.
	 *
	 * See notes in postgres.c about the design of this coding.
	 */
	if (sigsetjmp(local_sigjmp_buf, 1) != 0)
	{
		/* Prevents interrupts while cleaning up */
		HOLD_INTERRUPTS();

		/* Report the error to the server log */
		EmitErrorReport();

		AbortCurrentTransaction();
		/*
		 * We can now go away.	Note that because we'll call InitProcess, a
		 * callback will be registered to do ProcKill, which will clean up
		 * necessary state.
		 */
		proc_exit(0);
	}

	/* We can now handle ereport(ERROR) */
	PG_exception_stack = &local_sigjmp_buf;

	PG_SETMASK(&UnBlockSig);


	/*
	 * The following additional initialization allows us to call the persistent meta-data modules.
	 */

	/*
	 * Add my PGPROC struct to the ProcArray.
	 *
	 * Once I have done this, I am visible to other backends!
	 */
	InitProcessPhase2();

	/*
	 * Initialize my entry in the shared-invalidation manager's array of
	 * per-backend data.
	 *
	 * Sets up MyBackendId, a unique backend identifier.
	 */
	MyBackendId = InvalidBackendId;

	SharedInvalBackendInit(false);

	if (MyBackendId > MaxBackends || MyBackendId <= 0)
		elog(FATAL, "bad backend id: %d", MyBackendId);

	/*
	 * bufmgr needs another initialization call too
	 */
	InitBufferPoolBackend();

	/* heap access requires the rel-cache */
	RelationCacheInitialize();
	InitCatalogCache();

	/*
	 * It's now possible to do real access to the system catalogs.
	 *
	 * Load relcache entries for the system catalogs.  This must create at
	 * least the minimum set of "nailed-in" cache entries.
	 */
	RelationCacheInitializePhase2();

	/*
	 * Start a new transaction here before first access to db, and get a
	 * snapshot.  We don't have a use for the snapshot itself, but we're
	 * interested in the secondary effect that it sets RecentGlobalXmin.
	 */
	StartTransactionCommand();
	(void) GetTransactionSnapshot();

	/*
	 * In order to access the catalog, we need a database, and a
	 * tablespace; our access to the heap is going to be slightly
	 * limited, so we'll just use some defaults.
	 */
	if (!FindMyDatabase(DB_FOR_COMMON_ACCESS, &MyDatabaseId, &MyDatabaseTableSpace))
		ereport(FATAL,
				(errcode(ERRCODE_UNDEFINED_DATABASE),
				 errmsg("database \"%s\" does not exit", DB_FOR_COMMON_ACCESS)));

	/* Now we can mark our PGPROC entry with the database ID */
	/* (We assume this is an atomic store so no lock is needed) */
	MyProc->databaseId = MyDatabaseId;

	fullpath = GetDatabasePath(MyDatabaseId, MyDatabaseTableSpace);

	SetDatabasePath(fullpath);

	RelationCacheInitializePhase3();

	InitializeSessionUserIdStandalone();

	/* close the transaction we started above */
	CommitTransactionCommand();

	/* disable orca here */
	extern bool optimizer;
	optimizer = false;

	GlobalDeadLockDetectorLoop();

	/* One iteration done, go away */
	proc_exit(0);
}

static void
GlobalDeadLockDetectorLoop(void)
{
	int status;

	for (;;)
	{
		CHECK_FOR_INTERRUPTS();

		if (shutdown_requested)
			break;

		/* no need to live on if postmaster has died */
		if (!PostmasterIsAlive())
			exit(1);

		if (got_SIGHUP)
		{
			got_SIGHUP = false;
			ProcessConfigFile(PGC_SIGHUP);
		}

#ifdef FAULT_INJECTOR
		if (SIMPLE_FAULT_INJECTOR(GddProbe) == FaultInjectorTypeSkip)
			continue;
#endif

		StartTransactionCommand();
		status = doDeadLockCheck();
		if (status == STATUS_OK)
			CommitTransactionCommand();
		else
			AbortCurrentTransaction();

		/* GUC gp_global_deadlock_detector_period may be changed, skip sleep */
		if (!got_SIGHUP)
			pg_usleep(gp_global_deadlock_detector_period * 1000000L);
	}

	return;
}

static int
doDeadLockCheck(void)
{
	GddCtx		 *ctx;
	volatile int ret_status = RET_STATUS_OK;

	oldContext = MemoryContextSwitchTo(gddContext);

	PG_TRY();
	{
		ctx = GddCtxNew();

		buildWaitGraph(ctx);

		GddCtxReduce(ctx);

		if (!GddCtxEmpty(ctx))
		{
			StringInfoData wait_graph_str;
			StringInfoData pglock_str;

			/*
			 * At least one deadlock cycle is detected, and as all the invalid
			 * verts and edges were filtered out at the beginning, the left
			 * deadlock cycles are all valid ones.
			 */

			initStringInfo(&pglock_str);
			initStringInfo(&wait_graph_str);
			dumpGddCtx(ctx, &wait_graph_str);

			elog(LOG,
				 "global deadlock detected! Final graph is :%s",
				 wait_graph_str.data);

			breakDeadLock(ctx);
		}
	}
	PG_CATCH();
	{
		EmitErrorReport();
		FlushErrorState();
		ret_status = RET_STATUS_ERROR;
	}
	PG_END_TRY();

	MemoryContextSwitchTo(oldContext);
	MemoryContextReset(gddContext);

	return ret_status;
}

static void
buildWaitGraph(GddCtx *ctx)
{
	SPITupleTable *tuptable;
	TupleDesc	tupdesc;
	List		*gxids;
	bool		isnull;
	int			tuple_num;
	int			i;
	int         res;
	MemoryContext spiContext = NULL;
	volatile bool connected = false;

	/*
	 * SPI_connect() will switch to SPI memory context
	 */

	PG_TRY();
	{
		if (SPI_OK_CONNECT != SPI_connect())
		{
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("unable to connect to execute internal query")));
		}

		connected = true;

		PushActiveSnapshot(GetTransactionSnapshot());

		res = SPI_execute("select * from gp_dist_wait_status();", true, 0);

		PopActiveSnapshot();

		if (res == SPI_OK_SELECT && SPI_tuptable != NULL)
		{
			tuple_num = (int) SPI_processed;
			tupdesc = SPI_tuptable->tupdesc;
			tuptable = SPI_tuptable;

			/*
			 * Switch back to gdd memory context otherwise the graphs will be
			 * created in SPI memory context and freed in SPI_finish().
			 */
			spiContext = MemoryContextSwitchTo(gddContext);

			/* Get all valid gxids, any other gxids are considered invalid. */
			gxids = ListAllGxid();

			for (i = 0; i < tuple_num; i++)
			{
				TransactionId  waiter_xid;
				TransactionId  holder_xid;
				HeapTuple	   tuple;
				Datum		   d;
				bool		   solidedge;
				int			   segid;
				GddEdge       *edge;
				VertSatelliteData *waiter_data = NULL;
				VertSatelliteData *holder_data = NULL;
				EdgeSatelliteData *edge_data = NULL;


				waiter_data = (VertSatelliteData *) palloc(sizeof(VertSatelliteData));
				holder_data = (VertSatelliteData *) palloc(sizeof(VertSatelliteData));
				edge_data = (EdgeSatelliteData *) palloc(sizeof(EdgeSatelliteData));
				tuple = tuptable->vals[i];

				d = heap_getattr(tuple, 1, tupdesc, &isnull);
				Assert(!isnull);
				segid = DatumGetInt32(d);

				d = heap_getattr(tuple, 2, tupdesc, &isnull);
				Assert(!isnull);
				waiter_xid = DatumGetTransactionId(d);

				d = heap_getattr(tuple, 3, tupdesc, &isnull);
				Assert(!isnull);
				holder_xid = DatumGetTransactionId(d);

				d = heap_getattr(tuple, 4, tupdesc, &isnull);
				Assert(!isnull);
				solidedge = DatumGetBool(d);

				d = heap_getattr(tuple, 5, tupdesc, &isnull);
				Assert(!isnull);
				waiter_data->pid = DatumGetInt32(d);

				d = heap_getattr(tuple, 6, tupdesc, &isnull);
				Assert(!isnull);
				holder_data->pid = DatumGetInt32(d);

				d = heap_getattr(tuple, 7, tupdesc, &isnull);
				Assert(!isnull);
				edge_data->lockmode = TextDatumGetCString(d);

				d = heap_getattr(tuple, 8, tupdesc, &isnull);
				Assert(!isnull);
				edge_data->locktype = TextDatumGetCString(d);

				d = heap_getattr(tuple, 9, tupdesc, &isnull);
				Assert(!isnull);
				waiter_data->sessionid = DatumGetInt32(d);

				d = heap_getattr(tuple, 10, tupdesc, &isnull);
				Assert(!isnull);
				holder_data->sessionid = DatumGetInt32(d);

				/* Skip edges with invalid gxids */
				if (!list_member_int(gxids, waiter_xid) ||
					!list_member_int(gxids, holder_xid))
					continue;

				edge = GddCtxAddEdge(ctx, segid, waiter_xid, holder_xid, solidedge);
				edge->data = (void *) edge_data;
				edge->from->data = (void *) waiter_data;
				edge->to->data = (void *) holder_data;
			}
		}
	}
	PG_CATCH();
	{
		if (connected)
			SPI_finish();
		PG_RE_THROW();
	}
	PG_END_TRY();

	SPI_finish();
	/* Make sure we are in gddContext */
	MemoryContextSwitchTo(gddContext);
}

static void
breakDeadLock(GddCtx *ctx)
{
	List		   *xids;
	ListCell	   *cell;
	StringInfoData  str;

	xids = GddCtxBreakDeadLock(ctx);

	/* dump results*/
	initStringInfo(&str);

	dumpCancelResult(&str, xids);
	elog(LOG, "these gxids will be cancelled to break global deadlock: %s", str.data);

	foreach(cell, xids)
	{
		int             pid;

		TransactionId	xid = lfirst_int(cell);

		pid = GetPidByGxid(xid);
		Assert(pid > 0);

		DirectFunctionCall2(pg_cancel_backend_msg,
							Int32GetDatum(pid),
							CStringGetTextDatum("cancelled by global deadlock detector"));
	}
}

static void
dumpCancelResult(StringInfo str, List *xids)
{
	ListCell	*cell;

	foreach(cell, xids)
	{
		TransactionId	xid = lfirst_int(cell);

		appendStringInfo(str, "%u(Master Pid: %d)", xid, GetPidByGxid(xid));

		if (lnext(cell))
			appendStringInfo(str, ",");
	}
}

/*
 * Dump the graphs.
 */
void
dumpGddCtx(GddCtx *ctx, StringInfo str)
{
	GddMapIter	iter;

	Assert(ctx != NULL);
	Assert(str != NULL);

	appendStringInfo(str, "{");

	gdd_ctx_foreach_graph(iter, ctx)
	{
		GddGraph	*graph = gdd_map_iter_get_ptr(iter);

		dumpGddGraph(graph, str);

		if (gdd_map_iter_has_next(iter))
			appendStringInfo(str, ",");
	}

	appendStringInfo(str, "}");
}

/*
 * Dump the verts and edges.
 */
static void
dumpGddGraph(GddGraph *graph, StringInfo str)
{
	GddMapIter	vertiter;
	GddListIter	edgeiter;
	bool		first = true;

	Assert(graph != NULL);
	Assert(str != NULL);

	appendStringInfo(str, "\"seg%d\": [", graph->id);

	gdd_graph_foreach_out_edge(vertiter, edgeiter, graph)
	{
		GddEdge		*edge = gdd_list_iter_get_ptr(edgeiter);

		if (first)
			first = false;
		else
			appendStringInfo(str, ",");

		dumpGddEdge(edge, str);
	}

	appendStringInfo(str, "]");
}

/*
 * Dump edge.
 */
static void
dumpGddEdge(GddEdge *edge, StringInfo str)
{
	Assert(edge != NULL);
	Assert(edge->from != NULL);
	Assert(edge->to != NULL);
	Assert(str != NULL);

	appendStringInfo(str,
					 "\"p%d of dtx%d con%d waits for a %s lock on %s mode, "
					 "blocked by p%d of dtx%d con%d\"",
					 GET_PID_FROM_VERT(edge->from),
					 edge->from->id,
					 GET_SESSIONID_FROM_VERT(edge->from),
					 GET_LOCKTYPE_FROM_EDGE(edge),
					 GET_LOCKMODE_FROM_EDGE(edge),
					 GET_PID_FROM_VERT(edge->to),
					 edge->to->id,
					 GET_SESSIONID_FROM_VERT(edge->from));
}

static void
TimeoutHandler(void)
{
	kill(MyProcPid, SIGINT);
}
