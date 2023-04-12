/*-------------------------------------------------------------------------
 *
 * nodeShareInputScan.c
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodeShareInputScan.c
 *
 *-------------------------------------------------------------------------
 */

/*
 * INTERFACE ROUNTINES
 *	ExecInitShareInputScan
 * 	ExecShareInputScan
 * 	ExecEndShareInputScan
 * 	ExecShareInputMarkPosScan
 * 	ExecShareInputRestrPosScan
 * 	ExecShareInputReScanScanv
 */

#include "postgres.h"

#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

#include "access/xact.h"
#include "cdb/cdbvars.h"
#include "commands/tablespace.h"
#include "executor/executor.h"
#include "executor/nodeShareInputScan.h"
#include "miscadmin.h"
#include "utils/faultinjector.h"
#include "utils/gp_alloc.h"
#include "utils/tuplesort.h"
#include "utils/tuplestorenew.h"

typedef struct ShareInput_Lk_Context
{
	int readyfd;
	int donefd;
	int  zcnt;
	bool del_ready;
	bool del_done;
	char lkname_ready[MAXPGPATH];
	char lkname_done[MAXPGPATH];
} ShareInput_Lk_Context;

static void writer_wait_for_acks(ShareInput_Lk_Context *pctxt, int share_id, int xslice);

static void ExecEagerFreeShareInputScan(ShareInputScanState *node);

/*
 * init_tuplestore_state
 *    Initialize the tuplestore state for the Shared node if the state
 *    is not initialized.
 */
static void
init_tuplestore_state(ShareInputScanState *node)
{
	Assert(node->ts_state == NULL);

	EState *estate = node->ss.ps.state;
	ShareInputScan *sisc = (ShareInputScan *)node->ss.ps.plan;
	ShareNodeEntry *snEntry = ExecGetShareNodeEntry(estate, sisc->share_id, false);
	PlanState *snState = NULL;
	ShareType share_type = sisc->share_type;

	if(snEntry)
	{
		snState = (PlanState *) snEntry->shareState;
		if(snState)
		{
			ExecProcNode(snState);
		}

		else
		{
			Assert(share_type == SHARE_MATERIAL_XSLICE || share_type == SHARE_SORT_XSLICE);
		}
	}

	if(share_type == SHARE_MATERIAL_XSLICE)
	{
		node->ts_state = palloc0(sizeof(GenericTupStore));
		node->ts_state->matstore = ntuplestore_create_readerwriter(node->share_bufname_prefix, 0, false);
		node->ts_pos = (void *) ntuplestore_create_accessor(node->ts_state->matstore, false);
		ntuplestore_acc_seek_bof((NTupleStoreAccessor *)node->ts_pos);
	}
	else if(share_type == SHARE_MATERIAL)
	{
		/* The materialstate->ts_state structure should have been initialized already, during init of material node */
		node->ts_state = ((MaterialState *)snState)->ts_state;
		Assert(NULL != node->ts_state->matstore);
		node->ts_pos = (void *) ntuplestore_create_accessor(node->ts_state->matstore, false);
		ntuplestore_acc_seek_bof((NTupleStoreAccessor *)node->ts_pos);
	}
	else if(share_type == SHARE_SORT_XSLICE)
	{
		node->ts_state = palloc0(sizeof(GenericTupStore));
		node->ts_state->sortstore = tuplesort_begin_heap_file_readerwriter(
			&node->ss,
			node->share_bufname_prefix,
			false, /* isWriter */
			NULL, /* tupDesc */
			0, /* nkeys */
			NULL, /* attNums */
			NULL, /* sortOperators */
			NULL, /* sortCollations */
			NULL, /* nullsFirstFlags */
			PlanStateOperatorMemKB((PlanState *) node),
			true /* randomAccess */);

		tuplesort_begin_pos(node->ts_state->sortstore, (TuplesortPos **)(&node->ts_pos));
		tuplesort_rescan_pos(node->ts_state->sortstore, (TuplesortPos *)node->ts_pos);
	}
	else
	{
		Assert(sisc->share_type == SHARE_SORT);
		Assert(snState != NULL);

		node->ts_state = ((SortState *)snState)->tuplesortstate;
		Assert(NULL != node->ts_state->sortstore);
		tuplesort_begin_pos(node->ts_state->sortstore, (TuplesortPos **)(&node->ts_pos));
		tuplesort_rescan_pos(node->ts_state->sortstore, (TuplesortPos *)node->ts_pos);
	}

	Assert(NULL != node->ts_state);
	Assert(NULL != node->ts_state->matstore || NULL != node->ts_state->sortstore);
}


/* ------------------------------------------------------------------
 * 	ExecShareInputScan
 * 	Retrieve a tuple from the ShareInputScan
 * ------------------------------------------------------------------
 */
TupleTableSlot *
ExecShareInputScan(ShareInputScanState *node)
{
	EState *estate;
	ScanDirection dir;
	bool forward;
	TupleTableSlot *slot;

	ShareInputScan * sisc = (ShareInputScan *) node->ss.ps.plan;

	ShareType share_type = sisc->share_type;

	/*
	 * get state info from node
	 */
	estate = node->ss.ps.state;
	dir = estate->es_direction;
	forward = ScanDirectionIsForward(dir);


	/* if first time call, need to initialize the tuplestore state.  */
	if(node->ts_state == NULL)
	{
		elog(DEBUG1, "SISC (shareid=%d, slice=%d): No tuplestore yet, initializing tuplestore",
				sisc->share_id, currentSliceId);
		init_tuplestore_state(node);
	}

	/*
	 * Return NULL when necessary.
	 * This could help improve performance, especially when tuplestore is huge, because ShareInputScan
	 * do not need to read tuple from tuplestore when discard_output is true, which means current
	 * ShareInputScan is one but not the last one of Sequence's subplans.
	 */
	if (sisc->discard_output)
		return NULL;

	slot = node->ss.ps.ps_ResultTupleSlot;

	while(1)
	{
		bool gotOK = false;

		if(share_type == SHARE_MATERIAL || share_type == SHARE_MATERIAL_XSLICE)
		{
			ntuplestore_acc_advance((NTupleStoreAccessor *) node->ts_pos, forward ? 1 : -1);
			gotOK = ntuplestore_acc_current_tupleslot((NTupleStoreAccessor *) node->ts_pos, slot);
		}
		else
		{
			gotOK = tuplesort_gettupleslot_pos(node->ts_state->sortstore, (TuplesortPos *)node->ts_pos, forward, slot, CurrentMemoryContext);
		}

		if(!gotOK)
			return NULL;

		SIMPLE_FAULT_INJECTOR("execshare_input_next");

		return slot;
	}

	Assert(!"should not be here");
	return NULL;
}

/*  ------------------------------------------------------------------
 * 	ExecInitShareInputScan
 * ------------------------------------------------------------------
 */
ShareInputScanState *
ExecInitShareInputScan(ShareInputScan *node, EState *estate, int eflags)
{
	ShareInputScanState *sisstate;
	Plan *outerPlan;
	TupleDesc tupDesc;

	Assert(innerPlan(node) == NULL);

	/* create state data structure */
	sisstate = makeNode(ShareInputScanState);
	sisstate->ss.ps.plan = (Plan *) node;
	sisstate->ss.ps.state = estate;

	sisstate->ts_state = NULL;
	sisstate->ts_pos = NULL;
	sisstate->ts_markpos = NULL;

	sisstate->share_lk_ctxt = NULL;
	sisstate->freed = false;

	if (node->share_type == SHARE_MATERIAL_XSLICE || node->share_type == SHARE_SORT_XSLICE)
	{
		sisstate->share_bufname_prefix = shareinput_create_bufname_prefix(node->share_id);
		sisstate->share_lk_ctxt = shareinput_init_lk_ctxt(node->share_id);
	}

	/*
	 * init child node.
	 * if outerPlan is NULL, this is no-op (so that the ShareInput node will be
	 * only init-ed once).
	 */
	outerPlan = outerPlan(node);
	outerPlanState(sisstate) = ExecInitNode(outerPlan, estate, eflags);

	sisstate->ss.ps.targetlist = (List *)
		ExecInitExpr((Expr *) node->scan.plan.targetlist, (PlanState *) sisstate);
	Assert(node->scan.plan.qual == NULL);
	sisstate->ss.ps.qual = NULL;

	/* Misc initialization 
	 * 
	 * Create expression context 
	 */
	ExecAssignExprContext(estate, &sisstate->ss.ps);

	/* tuple table init */
	ExecInitResultTupleSlot(estate, &sisstate->ss.ps);
	sisstate->ss.ss_ScanTupleSlot = ExecInitExtraTupleSlot(estate);

	/*
	 * init tuple type.
	 */
	ExecAssignResultTypeFromTL(&sisstate->ss.ps);

	{
		bool hasoid;
		if (!ExecContextForcesOids(&sisstate->ss.ps, &hasoid))
			hasoid = false;

		tupDesc = ExecTypeFromTL(node->scan.plan.targetlist, hasoid);
	}

	ExecAssignScanType(&sisstate->ss, tupDesc);

	sisstate->ss.ps.ps_ProjInfo = NULL;

	/*
	 * If this is an intra-slice share node, increment reference count to
	 * tell the underlying node not to be freed before this node is ready to
	 * be freed.  fCreate flag to ExecGetShareNodeEntry is true because
	 * at this point we don't have the entry which will be initialized in
	 * the underlying node initialization later.
	 */
	if (node->share_type == SHARE_MATERIAL || node->share_type == SHARE_SORT)
	{
		ShareNodeEntry *snEntry = ExecGetShareNodeEntry(estate, node->share_id, true);
		snEntry->refcount++;
	}

	/*
	 * `PrepareTempTablespaces()` should be called when initializing ShareInputScanState.
	 * The shareinput-reader will open/create the named pipe file in
	 * ExecSliceDependencyShareInputScan() which is called at the begining of ExecutePlan().
	 * The shareinput-writer will open/create the named pipe file when data is ready.
	 * The READER and the WRITER share the pipe file for communication, so the pipe file
	 * must be in the same tablespace.
	 *
	 * We can't call PrepareTempTablespaces() under ExecShareInputScan()/ExecProcNode()
	 * like other callers, because it's too late for the READER.
	 */
	PrepareTempTablespaces();

	return sisstate;
}

void
ExecSliceDependencyShareInputScan(ShareInputScanState *node)
{
	ShareInputScan * sisc = (ShareInputScan *) node->ss.ps.plan;

	elog(DEBUG1, "SISC READER (shareid=%d, slice=%d): exec dependency on slice %d, driver_slice is %d",
			sisc->share_id, currentSliceId,
			currentSliceId, sisc->driver_slice);

	EState *estate = node->ss.ps.state;
	if(sisc->driver_slice >= 0 && sisc->driver_slice == currentSliceId)
	{
		shareinput_reader_waitready(node->share_lk_ctxt, sisc->share_id, estate->es_plannedstmt->planGen);
	}
}

/* ------------------------------------------------------------------
 * 	ExecEndShareInputScan
 * ------------------------------------------------------------------
 */
void ExecEndShareInputScan(ShareInputScanState *node)
{

	/* clean up tuple table */
	ExecClearTuple(node->ss.ss_ScanTupleSlot);
	ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);

	ShareInputScan * sisc = (ShareInputScan *) node->ss.ps.plan;
	if(node->share_lk_ctxt)
		shareinput_reader_notifydone(node->share_lk_ctxt, sisc->share_id);

	ExecEagerFreeShareInputScan(node);

	/*
	 * shutdown subplan.  First scanner of underlying share input will
	 * do the shutdown, all other scanners are no-op because outerPlanState
	 * is NULL
	 */
	ExecEndNode(outerPlanState(node));

	EndPlanStateGpmonPkt(&node->ss.ps);
}

/* ------------------------------------------------------------------
 * 	ExecReScanShareInputScan
 * ------------------------------------------------------------------
 */
void
ExecReScanShareInputScan(ShareInputScanState *node)
{
	/* if first time call, need to initialize the tuplestore state */
	if(node->ts_state == NULL)
	{
		init_tuplestore_state(node);
	}

	ShareInputScan *sisc = (ShareInputScan *) node->ss.ps.plan;

	ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
	Assert(NULL != node->ts_pos);

	if(sisc->share_type == SHARE_MATERIAL || sisc->share_type == SHARE_MATERIAL_XSLICE)
	{
		Assert(NULL != node->ts_state->matstore);
		ntuplestore_acc_seek_bof((NTupleStoreAccessor *) node->ts_pos);
	}
	else if (sisc->share_type == SHARE_SORT || sisc->share_type == SHARE_SORT_XSLICE)
	{
		Assert(NULL != node->ts_state->sortstore);
		tuplesort_rescan_pos(node->ts_state->sortstore, (TuplesortPos *) node->ts_pos);
	}
	else
	{
		Assert(!"ExecShareInputScanReScan: invalid share type ");
	}
}

/*************************************************************************
 * XXX
 * we need some IPC mechanism for shareinput_read_wait/writer_notify.  Semaphore is
 * the first thing come to mind but it turns out postgres is very picky about
 * how to use semaphore and we do not want to mess up with it.
 *
 * Here we used FIFO (named pipe).  mkfifo is Posix.1 and should be available on any
 * reasonable Unix like system.
 *
 * When we open fifo, we open it with O_RDWR.  So the fifo has both reader and writer.
 * That also means, for write, it will not block, but reader will until writer writes
 * something.
 *
 * At first, I used postgres File to manage the FIFO.  It turns out this is not
 * correct because when postgres run out of file descriptors, it will try to close
 * some file descriptors using an LRU algorithm.  Later when the File is used again,
 * postgres will reopen it. The FIFO here is used for synchronization so it is simply
 * wrong.  Here we use the file descriptor directly, and use a XCallBack to cleanup
 * the resource at the end of transaction (commit or abort).
 *
 * XXX However, it is always better to have this kind of stuff abstracted out
 * by the system.
 **************************************************************************/

#include "fcntl.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "storage/fd.h"
#include "cdb/cdbselect.h"

char *shareinput_create_bufname_prefix(int share_id)
{
	return psprintf("SIRW_%d_%d_%d", gp_session_id, gp_command_count, share_id);
}

/* Here we use the absolute path name as the lock name.  See fd.c
 * for how the name is created (GP_TEMP_FILE_DIR and make_database_relative).
 */
static void
sisc_lockname(char *p, int size, int share_id, const char* name)
{
	char		filename[MAXPGPATH];
	char	   *path;

	snprintf(filename, sizeof(filename),
			 "gpcdb2.sisc_%d_%d_%d_%d_%s",
			 GpIdentity.segindex, gp_session_id, gp_command_count, share_id, name);

	/* Ensure that temp tablespaces are set up to build temporary path. */
	PrepareTempTablespaces();
	path = GetTempFilePath(filename, true);
	if (strlen(path) >= size)
		elog(ERROR, "path to temporary file too long: %s", path);
	strcpy(p, path);
}

void *shareinput_init_lk_ctxt(int share_id)
{
	ShareInput_Lk_Context *pctxt = gp_malloc(sizeof(ShareInput_Lk_Context));

	if(!pctxt)
		ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY),
			errmsg("Share input reader failed: out of memory")));

	pctxt->readyfd = -1;
	pctxt->donefd = -1;
	pctxt->zcnt = 0;
	pctxt->del_ready = false;
	pctxt->del_done = false;

	sisc_lockname(pctxt->lkname_ready, MAXPGPATH, share_id, "ready");
	sisc_lockname(pctxt->lkname_done, MAXPGPATH, share_id, "done");

	return pctxt;
}

static void shareinput_clean_lk_ctxt(ShareInput_Lk_Context *lk_ctxt)
{
	elog(DEBUG1, "shareinput_clean_lk_ctxt cleanup lk ctxt %p", lk_ctxt);
	if (!lk_ctxt)
		return;

	if (lk_ctxt->readyfd >= 0)
	{
		if (gp_retry_close(lk_ctxt->readyfd))
			ereport(WARNING,
				(errcode(ERRCODE_IO_ERROR),
				errmsg("shareinput_clean_lk_ctxt cannot close readyfd: %m")));
	}

	if (lk_ctxt->donefd >= 0)
	{
		if (gp_retry_close(lk_ctxt->donefd))
			ereport(WARNING,
				(errcode(ERRCODE_IO_ERROR),
				errmsg("shareinput_clean_lk_ctxt cannot close donefd: %m")));
	}

	if (lk_ctxt->del_ready && lk_ctxt->lkname_ready[0])
	{
		if (unlink(lk_ctxt->lkname_ready))
			ereport(WARNING,
				(errcode(ERRCODE_IO_ERROR),
				errmsg("shareinput_clean_lk_ctxt cannot unlink \"%s\": %m",
					lk_ctxt->lkname_ready)));
	}

	if (lk_ctxt->del_done && lk_ctxt->lkname_done[0])
	{
		if (unlink(lk_ctxt->lkname_done))
			ereport(WARNING,
				(errcode(ERRCODE_IO_ERROR),
				errmsg("shareinput_clean_lk_ctxt cannot unlink \"%s\": %m",
					lk_ctxt->lkname_done)));
	}

	gp_free(lk_ctxt);
}

static void XCallBack_ShareInput_FIFO(XactEvent ev, void* vp)
{
	ShareInput_Lk_Context *lk_ctxt = (ShareInput_Lk_Context *) vp;
	shareinput_clean_lk_ctxt(lk_ctxt);
}

static void
create_tmp_fifo(const char *fifoname)
{
#ifdef WIN32
	elog(ERROR, "mkfifo not supported on win32");
#else
	int err = mkfifo(fifoname, 0600);
	if (err < 0 && errno != EEXIST)
		elog(ERROR, "could not create temporary fifo \"%s\": %m", fifoname);
#endif
}

/*
 * As all other read/write in postgres, we may be interrupted so retry is needed.
 */
static int retry_read(int fd, char *buf, int rsize)
{
	int sz;
	Assert(rsize > 0);

read_retry:
	sz = read(fd, buf, rsize);
	if (sz > 0)
		return sz;
	else if(sz == 0 || errno == EINTR)
		goto read_retry;
	else
		elog(ERROR, "could not read from fifo: %m");

	Assert(!"Never be here");
	return 0;
}

static int retry_write(int fd, char *buf, int wsize)
{
	int sz;
	Assert(wsize > 0);

write_retry:
	sz = write(fd, buf, wsize);
	if(sz > 0)
		return sz;
	else if(sz == 0 || errno == EINTR)
		goto write_retry;
	else
		elog(ERROR, "could not write to fifo: %m");

	Assert(!"Never be here");
	return 0;
}

#ifdef FAULT_INJECTOR
/**
 * create and open many tmp files, so the next fd number is bigger.
 **/
static void fi_create_many_fds(int *fds, char *file_prefix, int num)
{
	for (int i = 0; i < num; i++)
	{
		char filepath[1024];
		snprintf(filepath, sizeof(filepath), "%s/si_%d", file_prefix, i);
		fds[i] = open(filepath, O_RDWR | O_CREAT, 0666);
	}
	if (fds[num-1] > 0)
		Assert(fds[num-1] > num);
}

/**
 * close opened fds and delete the tmp files
 **/
static void fi_close_created_fds(int *fds, char *file_prefix, int num)
{
	for (int i = 0; i < num; i++)
	{
		char filepath[1024];
		snprintf(filepath, sizeof(filepath), "%s/si_%d", file_prefix, i);
		if (fds[i] > 0) {
			close(fds[i]);
			unlink(filepath);
		}
	}
}
#endif

/*
 * Readiness (a) synchronization.
 *
 * For readiness, the shared node will write xslice of 'a' into the pipe.
 * For each share, there is just one ready writer.  Once sharer starts write
 * it need to write all xslice copies of 'a', even if we are interrupted, that
 * is, we should not call CHECK_FOR_INTERRUPTS.
 *
 * For sharer, it need to check for ready to read (using select), because read
 * is blocking.  Otherwise if shared is cancelled before write, then we will be
 * blocked here forever.  Once shared has write at least one 'a', it will write
 * all xslice of 'a', so once select succeed, read will eventually succeed.  Once
 * sharer got 'a', it write 'b' back to shared.
 *
 * Done (b and z) synchronization.
 * For done, the shared is the only reader.  sharer will not block for writing,
 * but shared may block for read, therefore, we much call select before shared
 * calling read.  Because there is only one shared, nobody can steal char from
 * the pipe, therefore, if select succeed, read will not block forever.
 *
 * One thing to note is that some 'z' may comeback before all 'b' come back.
 * So, need to handle this in notifyready.
 *
 * For optimizer-generated plans, we skip the 'b' synchronization. The writer
 * does not wait for readers to acknowledge the "ready" handshake anymore, as
 * that can cause deadlocks (OPT-2690).
 */

/*
 * shareinput_reader_waitready
 *
 *  Called by the reader (consumer) to wait for the writer (producer) to produce
 *  all the tuples and write them to disk.
 *
 *  This is a blocking operation.
 */
void
shareinput_reader_waitready(void *ctxt, int share_id, PlanGenerator planGen)
{
	struct pollfd fds[1];
	int nfds = 0;
	char a;
	ShareInput_Lk_Context *pctxt = (ShareInput_Lk_Context *) ctxt;
	RegisterXactCallbackOnce(XCallBack_ShareInput_FIFO, pctxt);

#ifdef FAULT_INJECTOR
	/**
	 * In preivous code, use MPP_FD_SET(call select() internally) to operate FIFO,
	 * so the FIFO's fd number cannot exceed 65536.
	 *
	 * After using poll() instead of select(), it can overcome this limit.
	 * Using FAULT_INJECTOR here to test whether it works normally in this
	 * scenario (already opened many fds).
	 **/
	// we should use 70000(>65536) to test here, but the test env (docker)'s open files
	// is not very big, so only using a smaller value instead.
	// const int num = 70000;
	const int num = 40000;
	int tmp_fds[num];
	memset(tmp_fds, 0, sizeof(tmp_fds));
	char tmpfile_prefix[] = "/tmp/_gpdb_fault_inject_tmp_dir/"; // need create the dir first
	if (SIMPLE_FAULT_INJECTOR("inject_many_fds_for_shareinputscan") == FaultInjectorTypeSkip)
		fi_create_many_fds(tmp_fds, tmpfile_prefix, num);
#endif

	create_tmp_fifo(pctxt->lkname_ready);
	pctxt->readyfd = open(pctxt->lkname_ready, O_RDWR, 0600);
	if(pctxt->readyfd < 0)
		elog(ERROR, "could not open fifo \"%s\": %m", pctxt->lkname_ready);

	create_tmp_fifo(pctxt->lkname_done);
	pctxt->donefd = open(pctxt->lkname_done, O_RDWR, 0600);
	if(pctxt->donefd < 0)
		elog(ERROR, "could not open fifo \"%s\": %m", pctxt->lkname_done);

#ifdef FAULT_INJECTOR
	/* close opened fds */
	fi_close_created_fds(tmp_fds, tmpfile_prefix, num);
#endif

	fds[0].fd = pctxt->readyfd;
	fds[0].events = POLLIN;
	nfds++;
	while(1)
	{
		CHECK_FOR_INTERRUPTS();

		int nready = 0;
		int poll_timeout = 1000; // unit: ms

		nready = poll(fds, nfds, poll_timeout);

		if (nready == 1)
		{
#if USE_ASSERT_CHECKING
			int rwsize =
#endif
			retry_read(pctxt->readyfd, &a, 1);
			Assert(rwsize == 1 && a == 'a');

			elog(DEBUG1, "SISC READER (shareid=%d, slice=%d): Wait ready got writer's handshake",
					share_id, currentSliceId);

			if (planGen == PLANGEN_PLANNER)
			{
				/* For planner-generated plans, we send ack back after receiving the handshake */
				elog(DEBUG1, "SISC READER (shareid=%d, slice=%d): Wait ready writing ack back to writer",
						share_id, currentSliceId);

#if USE_ASSERT_CHECKING
				rwsize =
#endif
				retry_write(pctxt->donefd, "b", 1);
				Assert(rwsize == 1);
			}

			break;
		}
		else if (nready == 0)
		{
			elog(DEBUG1, "SISC READER (shareid=%d, slice=%d): Wait ready time out once",
					share_id, currentSliceId);
		}
		else
		{
			int save_errno = errno;
			elog(DEBUG1, "SISC READER (shareid=%d, slice=%d): Wait ready try again, errno %d ... ",
					share_id, currentSliceId, save_errno);
		}
	}
}

/*
 * shareinput_writer_notifyready
 *
 *  Called by the writer (producer) once it is done producing all tuples and
 *  writing them to disk. It notifies all the readers (consumers) that tuples
 *  are ready to be read from disk.
 *
 *  For planner-generated plans we wait for acks from all the readers before
 *  proceedings. It is a blocking operation.
 *
 *	For optimizer-generated plans we don't wait for acks, we proceed immediately.
 *  It is a non-blocking operation.
 */
void
shareinput_writer_notifyready(void *ctxt, int share_id, int xslice, PlanGenerator planGen)
{
	int n;
	ShareInput_Lk_Context *pctxt = (ShareInput_Lk_Context *) ctxt;
	RegisterXactCallbackOnce(XCallBack_ShareInput_FIFO, pctxt);

	create_tmp_fifo(pctxt->lkname_ready);
	pctxt->del_ready = true;
	pctxt->readyfd = open(pctxt->lkname_ready, O_RDWR, 0600);
	if(pctxt->readyfd < 0)
		elog(ERROR, "could not open fifo \"%s\": %m", pctxt->lkname_ready);

	create_tmp_fifo(pctxt->lkname_done);
	pctxt->del_done = true;
	pctxt->donefd = open(pctxt->lkname_done, O_RDWR, 0600);
	if(pctxt->donefd < 0)
		elog(ERROR, "could not open fifo \"%s\": %m", pctxt->lkname_done);

	for(n=0; n<xslice; ++n)
	{
#if USE_ASSERT_CHECKING
		int rwsize =
#endif
		retry_write(pctxt->readyfd, "a", 1);
		Assert(rwsize == 1);
	}
	elog(DEBUG1, "SISC WRITER (shareid=%d, slice=%d): wrote notify_ready to %d xslice readers",
						share_id, currentSliceId, xslice);

	if (planGen == PLANGEN_PLANNER)
	{
		/* For planner-generated plans, we wait for acks from all the readers */
		writer_wait_for_acks(pctxt, share_id, xslice);
	}
}

/*
 * writer_wait_for_acks
 *
 * After sending the handshake to all the reader, the writer waits for acks
 * from all the readers.
 *
 * This is a blocking operation.
 */
static void
writer_wait_for_acks(ShareInput_Lk_Context *pctxt, int share_id, int xslice)
{
	int ack_needed = xslice;
	struct pollfd fds[1];
	int nfds = 0;
	char b;

	fds[0].fd = pctxt->donefd;
	fds[0].events = POLLIN;
	nfds++;
	while(ack_needed > 0)
	{
		CHECK_FOR_INTERRUPTS();

		int nready = 0;
		int poll_timeout = 1000; // unit: ms

		nready = poll(fds, nfds, poll_timeout);

		if (nready == 1)
		{
#if USE_ASSERT_CHECKING
			int rwsize =
#endif
			retry_read(pctxt->donefd, &b, 1);
			Assert(rwsize == 1);

			if(b == 'z')
			{
				++pctxt->zcnt;
			}
			else
			{
				Assert(b == 'b');
				--ack_needed;
				elog(DEBUG1, "SISC WRITER (shareid=%d, slice=%d): notify ready succeed 1, xslice remaining %d",
						share_id, currentSliceId, ack_needed);
			}
		}
		else if (nready == 0)
		{
			elog(DEBUG1, "SISC WRITER (shareid=%d, slice=%d): Notify ready time out once ... ",
					share_id, currentSliceId);
		}
		else
		{
			int save_errno = errno;
			elog(DEBUG1, "SISC WRITER (shareid=%d, slice=%d): notify still wait for an answer, errno %d",
					share_id, currentSliceId, save_errno);
		}
	}
}

/*
 * shareinput_reader_notifydone
 *
 *  Called by the reader (consumer) to notify the writer (producer) that
 *  it is done reading tuples from disk.
 *
 *  This is a non-blocking operation.
 */
void
shareinput_reader_notifydone(void *ctxt, int share_id)
{
	ShareInput_Lk_Context *pctxt = (ShareInput_Lk_Context *) ctxt;

	if (pctxt->donefd < 0)
		return;

#if USE_ASSERT_CHECKING
	int rwsize  =
#endif
	retry_write(pctxt->donefd, "z", 1);
	Assert(rwsize == 1);

	shareinput_clean_lk_ctxt(pctxt);
	UnregisterXactCallbackOnce(XCallBack_ShareInput_FIFO, (void *) ctxt);
}

/*
 * shareinput_writer_waitdone
 *
 *  Called by the writer (producer) to wait for the "done" notfication from
 *  all readers (consumers).
 *
 *  This is a blocking operation.
 */
void
shareinput_writer_waitdone(void *ctxt, int share_id, int nsharer_xslice)
{
	ShareInput_Lk_Context *pctxt = (ShareInput_Lk_Context *) ctxt;
	struct pollfd fds[1];
	int nfds = 0;

	if (pctxt->donefd < 0)
		return;

	char z;
	int ack_needed = nsharer_xslice - pctxt->zcnt;

	elog(DEBUG1, "SISC WRITER (shareid=%d, slice=%d): waiting for DONE message from %d readers",
							share_id, currentSliceId, ack_needed);

	fds[0].fd = pctxt->donefd;
	fds[0].events = POLLIN;
	nfds++;
	while(ack_needed > 0)
	{
		CHECK_FOR_INTERRUPTS();

		int nready = 0;
		int poll_timeout = 1000; // unit: ms

		nready = poll(fds, nfds, poll_timeout);

		if (nready == 1)
		{
#if USE_ASSERT_CHECKING
			int rwsize =
#endif
			retry_read(pctxt->donefd, &z, 1);
			Assert(rwsize == 1 && z == 'z');

			elog(DEBUG1, "SISC WRITER (shareid=%d, slice=%d): wait done get 1 notification",
					share_id, currentSliceId);
			--ack_needed;
		}
		else if (nready == 0)
		{
			elog(DEBUG1, "SISC WRITER (shareid=%d, slice=%d): wait done timeout once",
					share_id, currentSliceId);
		}
		else
		{
			int save_errno = errno;
			elog(DEBUG1, "SISC WRITER (shareid=%d, slice=%d): wait done time out once, errno %d",
					share_id, currentSliceId, save_errno);
		}
	}

	elog(DEBUG1, "SISC WRITER (shareid=%d, slice=%d): Writer received all %d reader done notifications",
			share_id, currentSliceId, nsharer_xslice - pctxt->zcnt);

	shareinput_clean_lk_ctxt(ctxt);
	UnregisterXactCallbackOnce(XCallBack_ShareInput_FIFO, (void *) ctxt);
}

/*
 * During EagerFree ShareInputScan decrements the
 * reference count in ShareNodeEntry when its intra-slice share node.
 * The reference count tells the underlying Material/Sort node not to free
 * too eagerly as this node still needs to read its tuples.  Once this node
 * is freed, the underlying node can free its content.
 * We consider this reference counter only in intra-slice cases, because
 * inter-slice share nodes have their own pointer to the buffer and
 * there is not way to tell this reference over Motions anyway.
 */
static void
ExecEagerFreeShareInputScan(ShareInputScanState *node)
{
	/*
	 * no need to call tuplestore end.  Underlying ShareInput will take
	 * care of releasing tuplestore resources
	 */
	/*
	 * XXX Do we need to pfree the tuplestore_state and pos?
	 * XXX nodeMaterial.c does not, need to find out why
	 */

	ShareInputScan * sisc = (ShareInputScan *) node->ss.ps.plan;
	if(sisc->share_type == SHARE_MATERIAL || sisc->share_type == SHARE_MATERIAL_XSLICE)
	{
		if(node->ts_pos != NULL)
			ntuplestore_destroy_accessor((NTupleStoreAccessor *) node->ts_pos);
		if(node->ts_markpos != NULL)
			pfree(node->ts_markpos);

		if(NULL != node->ts_state && NULL != node->ts_state->matstore)
		{
			/* Check if shared X-SLICE. In that case, we can safely destroy our tuplestore */
			if(ntuplestore_is_readerwriter_reader(node->ts_state->matstore))
			{
				ntuplestore_destroy(node->ts_state->matstore);
			}
		}
	}
	if (sisc->share_type == SHARE_SORT_XSLICE)
	{
		if (NULL != node->ts_state && NULL != node->ts_state->sortstore)
		{
			tuplesort_end(node->ts_state->sortstore);
			node->ts_state->sortstore = NULL;
		}
	}

	/*
	 * Reset our copy of the pointer to the ts_state. The tuplestore can still be accessed by
	 * the other consumers, but we don't have a pointer to it anymore
	 */
	node->ts_state = NULL;
	node->ts_pos = NULL;
	node->ts_markpos = NULL;

	/* This can be called more than once */
	if (!node->freed &&
			(sisc->share_type == SHARE_MATERIAL || sisc->share_type == SHARE_SORT))
	{
		/*
		 * Decrement reference count when it's intra-slice.  We don't need
		 * two-pass tree descending because ShareInputScan should always appear
		 * before the underlying Material/Sort node.
		 */
		EState *estate = node->ss.ps.state;
		ShareNodeEntry *snEntry = ExecGetShareNodeEntry(estate, sisc->share_id, false);

		Assert(snEntry && snEntry->refcount > 0);
		snEntry->refcount--;
	}
	node->freed = true;
}

void
ExecSquelchShareInputScan(ShareInputScanState *node)
{
	ShareType share_type = ((ShareInputScan *) node->ss.ps.plan)->share_type;
	bool isWriter = outerPlanState(node) != NULL;
	bool tuplestoreInitialized = node->ts_state != NULL;

	/*
	 * If this SharedInputScan is shared within the same slice then its
	 * subtree may still need to be executed and the motions in the subtree
	 * cannot yet be stopped. Thus, don't recurse in this case.
	 *
	 * In squelching a cross-slice SharedInputScan writer, we need to ensure
	 * we don't block any reader on other slices as a result of not
	 * materializing the shared plan.
	 *
	 * Note that we emphatically can't "fake" an empty tuple store and just
	 * go ahead waking up the readers because that can lead to wrong results.
	 */
	switch (share_type)
	{
		case SHARE_MATERIAL:
		case SHARE_SORT:
			/* don't recurse into child */
			return;

		case SHARE_MATERIAL_XSLICE:
		case SHARE_SORT_XSLICE:
			if (isWriter && !tuplestoreInitialized)
				ExecProcNode((PlanState *) node);
			break;
		case SHARE_NOTSHARED:
			break;
	}
	ExecSquelchNode(outerPlanState(node));

	/* Free any resources that we can. */
	ExecEagerFreeShareInputScan(node);
}
