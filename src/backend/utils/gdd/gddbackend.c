/*-------------------------------------------------------------------------
 *
 * gddbackend.c
 *	  Global DeadLock Detector - Backend
 *
 *
 * Copyright (c) 2018-Present VMware, Inc. or its affiliates.
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <unistd.h>

/* These are always necessary for a bgworker */
#include "miscadmin.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/lwlock.h"
#include "storage/proc.h"
#include "storage/shmem.h"
#include "storage/procarray.h"

#include "access/xact.h"
#include "cdb/cdbvars.h"
#include "executor/spi.h"
#include "postmaster/postmaster.h"
#include "tcop/tcopprot.h"
#include "utils/gdd.h"
#include "utils/builtins.h"
#include "utils/snapmgr.h"
#include "utils/memutils.h"
#include "utils/faultinjector.h"
#include "gdddetector.h"
#include "gdddetectorpriv.h"
#include "pgstat.h"

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

void GlobalDeadLockDetectorMain(Datum main_arg);

static void GlobalDeadLockDetectorLoop(void);
static int  doDeadLockCheck(void);
static void buildWaitGraph(GddCtx *ctx);
static void breakDeadLock(GddCtx *ctx);
static void dumpCancelResult(StringInfo str, List *xids);
extern void dumpGddCtx(GddCtx *ctx, StringInfo str);
static void dumpGddGraph(GddGraph *graph, StringInfo str);
static void dumpGddEdge(GddEdge *edge, StringInfo str);

static MemoryContext	gddContext;
static MemoryContext    oldContext;

static volatile sig_atomic_t got_SIGHUP = false;

int gp_global_deadlock_detector_period;

static bool am_global_deadlock_detector = false;

/* SIGHUP: set flag to reload config file */
static void
sigHupHandler(SIGNAL_ARGS)
{
	got_SIGHUP = true;

	if(MyProc)
		SetLatch(&MyProc->procLatch);
}

bool
GlobalDeadLockDetectorStartRule(Datum main_arg)
{
	if (Gp_role == GP_ROLE_DISPATCH &&
		gp_enable_global_deadlock_detector)
		return true;

	return false;
}

/*
 * GlobalDeadLockDetectorMain
 */
void
GlobalDeadLockDetectorMain(Datum main_arg)
{
	am_global_deadlock_detector = true;

	pqsignal(SIGHUP, sigHupHandler);

	/* We're now ready to receive signals */
	BackgroundWorkerUnblockSignals();

	/* Connect to our database */
	BackgroundWorkerInitializeConnection(DB_FOR_COMMON_ACCESS, NULL, 0);

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
	int	status;

	/* Allocate MemoryContext */
	gddContext = AllocSetContextCreate(TopMemoryContext,
									   "GddContext",
									   ALLOCSET_DEFAULT_MINSIZE,
									   ALLOCSET_DEFAULT_INITSIZE,
									   ALLOCSET_DEFAULT_MAXSIZE);

	while (true)
	{
		int			rc;
		int			timeout;

		if (got_SIGHUP)
		{
			got_SIGHUP = false;
			ProcessConfigFile(PGC_SIGHUP);
		}

#ifdef FAULT_INJECTOR
		if (SIMPLE_FAULT_INJECTOR("gdd_probe") == FaultInjectorTypeSkip)
			continue;
#endif

		StartTransactionCommand();
		status = doDeadLockCheck();
		if (status == STATUS_OK)
			CommitTransactionCommand();
		else
			AbortCurrentTransaction();

		timeout = got_SIGHUP ? 0 : gp_global_deadlock_detector_period;

		rc = WaitLatch(&MyProc->procLatch,
					   WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
					   timeout * 1000L,
					   WAIT_EVENT_GLOBAL_DEADLOCK_DETECTOR_MAIN);

		ResetLatch(&MyProc->procLatch);

		/* emergency bailout if postmaster has died */
		if (rc & WL_POSTMASTER_DEATH)
			proc_exit(1);
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
