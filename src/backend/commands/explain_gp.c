/*-------------------------------------------------------------------------
 *
 * explain_gp.c
 *	  Functions supporting the Greenplum extensions to EXPLAIN ANALYZE
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/commands/explain_gp.c
 *
 *-------------------------------------------------------------------------
 */

#include "portability/instr_time.h"

#include "libpq-fe.h"
#include "libpq-int.h"
#include "cdb/cdbconn.h"		/* SegmentDatabaseDescriptor */
#include "cdb/cdbdisp.h"                /* CheckDispatchResult() */
#include "cdb/cdbdispatchresult.h"	/* CdbDispatchResults */
#include "cdb/cdbexplain.h"		/* me */
#include "cdb/cdbpartition.h"
#include "cdb/cdbpathlocus.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"		/* GpIdentity.segindex */
#include "cdb/cdbendpoint.h"
#include "cdb/memquota.h"
#include "libpq/pqformat.h"		/* pq_beginmessage() etc. */
#include "miscadmin.h"
#include "utils/resscheduler.h"
#include "utils/memaccounting.h"
#include "utils/memutils.h"		/* MemoryContextGetPeakSpace() */
#include "utils/vmem_tracker.h"

#include "cdb/cdbexplain.h"             /* cdbexplain_recvExecStats */

#define NUM_SORT_METHOD 5

#define TOP_N_HEAP_SORT_STR "top-N heapsort"
#define QUICK_SORT_STR "quicksort"
#define EXTERNAL_SORT_STR "external sort"
#define EXTERNAL_MERGE_STR "external merge"
#define IN_PROGRESS_SORT_STR "sort still in progress"

#define NUM_SORT_SPACE_TYPE 2
#define MEMORY_STR_SORT_SPACE_TYPE "Memory"
#define DISK_STR_SORT_SPACE_TYPE "Disk"

/* Convert bytes into kilobytes */
#define kb(x) (floor((x + 1023.0) / 1024.0))

/*
 * Different sort method in GPDB.
 *
 * Make sure to update NUM_SORT_METHOD when this enum changes.
 * This enum value is used an index in the array sortSpaceUsed
 * in struct CdbExplain_NodeSummary.
 */
typedef enum
{
	UNINITIALIZED_SORT = 0,
	TOP_N_HEAP_SORT = 1,
	QUICK_SORT = 2,
	EXTERNAL_SORT = 3,
	EXTERNAL_MERGE = 4,
	IN_PROGRESS_SORT = 5
} ExplainSortMethod;

typedef enum
{
	UNINITIALIZED_SORT_SPACE_TYPE = 0,
	MEMORY_SORT_SPACE_TYPE = 1,
	DISK_SORT_SPACE_TYPE = 2
} ExplainSortSpaceType;

/*
 * Convert the above enum `ExplainSortMethod` to printable string for
 * Explain Analyze.
 * Note : No conversion available for `UNINITALIZED_SORT`. Caller has to index
 * this array by subtracting 1 from origin enum value.
 *
 * E.g. sort_method_enum_str[TOP_N_HEAP_SORT-1]
 */
const char *sort_method_enum_str[] = {
	TOP_N_HEAP_SORT_STR,
	QUICK_SORT_STR,
	EXTERNAL_SORT_STR,
	EXTERNAL_MERGE_STR,
	IN_PROGRESS_SORT_STR
};

/* EXPLAIN ANALYZE statistics for one plan node of a slice */
typedef struct CdbExplain_StatInst
{
	NodeTag		pstype;			/* PlanState node type */
	instr_time	starttime;		/* Start time of current iteration of node */
	instr_time	counter;		/* Accumulated runtime for this node */
	double		firsttuple;		/* Time for first tuple of this cycle */
	double		startup;		/* Total startup time (in seconds) */
	double		total;			/* Total total time (in seconds) */
	double		ntuples;		/* Total tuples produced */
	double		nloops;			/* # of run cycles for this node */
	double		execmemused;	/* executor memory used (bytes) */
	double		workmemused;	/* work_mem actually used (bytes) */
	double		workmemwanted;	/* work_mem to avoid workfile i/o (bytes) */
	bool		workfileCreated;	/* workfile created in this node */
	instr_time	firststart;		/* Start time of first iteration of node */
	double		peakMemBalance; /* Max mem account balance */
	int			numPartScanned; /* Number of part tables scanned */
	ExplainSortMethod sortMethod;	/* Type of sort */
	ExplainSortSpaceType sortSpaceType; /* Sort space type */
	long		sortSpaceUsed;	/* Memory / Disk used by sort(KBytes) */
	int			bnotes;			/* Offset to beginning of node's extra text */
	int			enotes;			/* Offset to end of node's extra text */
} CdbExplain_StatInst;


/* EXPLAIN ANALYZE statistics for one process working on one slice */
typedef struct CdbExplain_SliceWorker
{
	double		peakmemused;	/* bytes alloc in per-query mem context tree */
	double		vmem_reserved;	/* vmem reserved by a QE */
	double		memory_accounting_global_peak;	/* peak memory observed during
												 * memory accounting */
} CdbExplain_SliceWorker;


/* Header of EXPLAIN ANALYZE statistics message sent from qExec to qDisp */
typedef struct CdbExplain_StatHdr
{
	NodeTag		type;			/* T_CdbExplain_StatHdr */
	int			segindex;		/* segment id */
	int			nInst;			/* num of StatInst entries following StatHdr */
	int			bnotes;			/* offset to extra text area */
	int			enotes;			/* offset to end of extra text area */

	int			memAccountCount;	/* How many mem account we serialized */
	int			memAccountStartOffset;	/* Where in the header our memory
										 * account array is serialized */

	CdbExplain_SliceWorker worker;	/* qExec's overall stats for slice */

	/*
	 * During serialization, we use this as a temporary StatInst and save
	 * "one-at-a-time" StatInst into this variable. We then write this
	 * variable into buffer (serialize it) and then "recycle" the same inst
	 * for next plan node's StatInst. During deserialization, an Array
	 * [0..nInst-1] of StatInst entries is appended starting here.
	 */
	CdbExplain_StatInst inst[1];

	/* extra text is appended after that */
} CdbExplain_StatHdr;


/* Dispatch status summarized over workers in a slice */
typedef struct CdbExplain_DispatchSummary
{
	int			nResult;
	int			nOk;
	int			nError;
	int			nCanceled;
	int			nNotDispatched;
	int			nIgnorableError;
} CdbExplain_DispatchSummary;


/* One node's EXPLAIN ANALYZE statistics for all the workers of its segworker group */
typedef struct CdbExplain_NodeSummary
{
	/* Summary over all the node's workers */
	CdbExplain_Agg ntuples;
	CdbExplain_Agg execmemused;
	CdbExplain_Agg workmemused;
	CdbExplain_Agg workmemwanted;
	CdbExplain_Agg totalWorkfileCreated;
	CdbExplain_Agg peakMemBalance;
	/* Used for DynamicSeqScan, DynamicIndexScan and DynamicBitmapHeapScan */
	CdbExplain_Agg totalPartTableScanned;
	/* Summary of space used by sort */
	CdbExplain_Agg sortSpaceUsed[NUM_SORT_SPACE_TYPE][NUM_SORT_METHOD];

	/* insts array info */
	int			segindex0;		/* segment id of insts[0] */
	int			ninst;			/* num of StatInst entries in inst array */

	/* Array [0..ninst-1] of StatInst entries is appended starting here */
	CdbExplain_StatInst insts[1];	/* variable size - must be last */
} CdbExplain_NodeSummary;


/* One slice's statistics for all the workers of its segworker group */
typedef struct CdbExplain_SliceSummary
{
	Slice	   *slice;

	/* worker array */
	int			nworker;		/* num of SliceWorker slots in worker array */
	int			segindex0;		/* segment id of workers[0] */
	CdbExplain_SliceWorker *workers;	/* -> array [0..nworker-1] of
										 * SliceWorker */

	/*
	 * We use void ** as we don't have access to MemoryAccount struct, which
	 * is private to memory accounting framework
	 */
	void	  **memoryAccounts; /* Array of pointers to serialized memory
								 * accounts array, one array per worker
								 * [0...nworker-1]. */
	MemoryAccountIdType *memoryAccountCount;	/* Array of memory account
												 * counts, one per slice */

	CdbExplain_Agg peakmemused; /* Summary of SliceWorker stats over all of
								 * the slice's workers */

	CdbExplain_Agg vmem_reserved;	/* vmem reserved by QEs */

	CdbExplain_Agg memory_accounting_global_peak;	/* Peak memory accounting
													 * balance by QEs */

	/* Rollup of per-node stats over all of the slice's workers and nodes */
	double		workmemused_max;
	double		workmemwanted_max;

	/* How many workers were dispatched and returned results? (0 if local) */
	CdbExplain_DispatchSummary dispatchSummary;
} CdbExplain_SliceSummary;


/* State for cdbexplain_showExecStats() */
typedef struct CdbExplain_ShowStatCtx
{
	StringInfoData extratextbuf;
	instr_time	querystarttime;

	/* Rollup of per-node stats over the entire query plan */
	double		workmemused_max;
	double		workmemwanted_max;

	bool		stats_gathered;
	/* Per-slice statistics are deposited in this SliceSummary array */
	int			nslice;			/* num of slots in slices array */
	CdbExplain_SliceSummary *slices;	/* -> array[0..nslice-1] of
										 * SliceSummary */
} CdbExplain_ShowStatCtx;


/* State for cdbexplain_sendStatWalker() and cdbexplain_collectStatsFromNode() */
typedef struct CdbExplain_SendStatCtx
{
	StringInfoData *notebuf;
	StringInfoData buf;
	CdbExplain_StatHdr hdr;
} CdbExplain_SendStatCtx;


/* State for cdbexplain_recvStatWalker() and cdbexplain_depositStatsToNode() */
typedef struct CdbExplain_RecvStatCtx
{
	/*
	 * iStatInst is the current StatInst serial during the depositing process
	 * for a slice. We walk the plan tree, and for each node we deposit stat
	 * from all the QEs of the segworker group for current slice. After we
	 * finish one node, we increase iStatInst, which means we are done with
	 * one plan node's stat across all segments and now moving forward to the
	 * next one. Once we are done processing all the plan node of a PARTICULAR
	 * slice, then we switch to the next slice, read the messages from all the
	 * QEs of the next slice (another segworker group) store them in the
	 * msgptrs, reset the iStatInst and then start parsing these messages and
	 * depositing them in the nodes of the new slice.
	 */
	int			iStatInst;

	/*
	 * nStatInst is the total number of StatInst for current slice. Typically
	 * this is the number of plan nodes in the current slice.
	 */
	int			nStatInst;

	/*
	 * segIndexMin is the min of segment index from which we collected message
	 * (i.e., saved msgptrs)
	 */
	int			segindexMin;

	/*
	 * segIndexMax is the max of segment index from which we collected message
	 * (i.e., saved msgptrs)
	 */
	int			segindexMax;

	/*
	 * We deposit stat for one slice at a time. sliceIndex saves the current
	 * slice
	 */
	int			sliceIndex;

	/*
	 * The number of msgptrs that we have saved for current slice. This is
	 * typically the number of QE processes
	 */
	int			nmsgptr;
	/* The actual messages. Contains an array of StatInst too */
	CdbExplain_StatHdr **msgptrs;
	CdbDispatchResults *dispatchResults;
	StringInfoData *extratextbuf;
	CdbExplain_ShowStatCtx *showstatctx;

	/* Rollup of per-node stats over all of the slice's workers and nodes */
	double		workmemused_max;
	double		workmemwanted_max;
} CdbExplain_RecvStatCtx;


/* State for cdbexplain_localStatWalker() */
typedef struct CdbExplain_LocalStatCtx
{
	CdbExplain_SendStatCtx send;
	CdbExplain_RecvStatCtx recv;
	CdbExplain_StatHdr *msgptrs[1];
} CdbExplain_LocalStatCtx;


static void
cdbexplain_showExecStatsEnd(struct PlannedStmt *stmt,
							struct CdbExplain_ShowStatCtx  *showstatctx,
                            struct EState                  *estate,
							ExplainState *es);

static void cdbexplain_showExecStats(struct PlanState *planstate,
									 ExplainState *es);
static CdbVisitOpt cdbexplain_localStatWalker(PlanState *planstate,
											  void *context);
static CdbVisitOpt cdbexplain_sendStatWalker(PlanState *planstate,
											 void *context);
static CdbVisitOpt cdbexplain_recvStatWalker(PlanState *planstate,
											 void *context);
static void cdbexplain_collectSliceStats(PlanState *planstate,
										 CdbExplain_SliceWorker *out_worker);
static void cdbexplain_depositSliceStats(CdbExplain_StatHdr *hdr,
										 CdbExplain_RecvStatCtx *recvstatctx);
static void cdbexplain_collectStatsFromNode(PlanState *planstate,
											CdbExplain_SendStatCtx *ctx);
static void cdbexplain_depositStatsToNode(PlanState *planstate,
										  CdbExplain_RecvStatCtx *ctx);
static int cdbexplain_collectExtraText(PlanState *planstate,
									   StringInfo notebuf);
static int cdbexplain_countLeafPartTables(PlanState *planstate);

static void show_motion_keys(PlanState *planstate, List *hashExpr, int nkeys,
							 AttrNumber *keycols, const char *qlabel,
							 List *ancestors, ExplainState *es);
static void explain_partition_selector(PartitionSelector *ps,
						   PlanState *parentstate,
						   List *ancestors, ExplainState *es);
static void
gpexplain_formatSlicesOutput(struct CdbExplain_ShowStatCtx *showstatctx,
                             struct EState *estate,
                             ExplainState *es);

/*
 * Convert the sort method in string to corresponding
 * enum ExplainSortMethod.
 *
 * If you change please update tuplesort_get_stats / tuplesort_get_stats_mk
 * in tuplesort.c / tuplesort_mk.c
 */
static ExplainSortMethod
String2ExplainSortMethod(const char *sortMethod)
{
	if (sortMethod == NULL)
	{
		return UNINITIALIZED_SORT;
	}
	else if (strcmp(TOP_N_HEAP_SORT_STR, sortMethod) == 0)
	{
		return TOP_N_HEAP_SORT;
	}
	else if (strcmp(QUICK_SORT_STR, sortMethod) == 0)
	{
		return QUICK_SORT;
	}
	else if (strcmp(EXTERNAL_SORT_STR, sortMethod) == 0)
	{
		return EXTERNAL_SORT;
	}
	else if (strcmp(EXTERNAL_MERGE_STR, sortMethod) == 0)
	{
		return EXTERNAL_MERGE;
	}
	else if (strcmp(IN_PROGRESS_SORT_STR, sortMethod) == 0)
	{
		return IN_PROGRESS_SORT;
	}
	return UNINITIALIZED_SORT;
}

static ExplainSortSpaceType
String2ExplainSortSpaceType(const char *sortSpaceType, ExplainSortMethod sortMethod)
{
	if (sortSpaceType == NULL ||
		sortMethod == UNINITIALIZED_SORT)
	{
		return UNINITIALIZED_SORT_SPACE_TYPE;
	}
	else if (strcmp(MEMORY_STR_SORT_SPACE_TYPE, sortSpaceType) == 0)
	{
		return MEMORY_SORT_SPACE_TYPE;
	}
	else
	{
		Assert(strcmp(DISK_STR_SORT_SPACE_TYPE, sortSpaceType) == 0);
		return DISK_SORT_SPACE_TYPE;
	}
}

/*
 * cdbexplain_localExecStats
 *	  Called by qDisp to build NodeSummary and SliceSummary blocks
 *	  containing EXPLAIN ANALYZE statistics for a root slice that
 *	  has been executed locally in the qDisp process.  Attaches these
 *	  structures to the PlanState nodes' Instrumentation objects for
 *	  later use by cdbexplain_showExecStats().
 *
 * 'planstate' is the top PlanState node of the slice.
 * 'showstatctx' is a CdbExplain_ShowStatCtx object which was created by
 *		calling cdbexplain_showExecStatsBegin().
 */
void
cdbexplain_localExecStats(struct PlanState *planstate,
						  struct CdbExplain_ShowStatCtx *showstatctx)
{
	CdbExplain_LocalStatCtx ctx;

	Assert(Gp_role != GP_ROLE_EXECUTE);

	Insist(planstate && planstate->instrument && showstatctx);

	memset(&ctx, 0, sizeof(ctx));

	/* Set up send context area. */
	ctx.send.notebuf = &showstatctx->extratextbuf;

	/* Set up a temporary StatHdr for both collecting and depositing stats. */
	ctx.msgptrs[0] = &ctx.send.hdr;
	ctx.send.hdr.segindex = GpIdentity.segindex;
	ctx.send.hdr.nInst = 1;

	/* Set up receive context area referencing our temp StatHdr. */
	ctx.recv.nStatInst = ctx.send.hdr.nInst;
	ctx.recv.segindexMin = ctx.recv.segindexMax = ctx.send.hdr.segindex;

	ctx.recv.sliceIndex = LocallyExecutingSliceIndex(planstate->state);
	ctx.recv.msgptrs = ctx.msgptrs;
	ctx.recv.nmsgptr = 1;
	ctx.recv.dispatchResults = NULL;
	ctx.recv.extratextbuf = NULL;
	ctx.recv.showstatctx = showstatctx;

	/*
	 * Collect and redeposit statistics from each PlanState node in this
	 * slice. Any extra message text will be appended directly to
	 * extratextbuf.
	 */
	planstate_walk_node(planstate, cdbexplain_localStatWalker, &ctx);

	/* Obtain per-slice stats and put them in SliceSummary. */
	cdbexplain_collectSliceStats(planstate, &ctx.send.hdr.worker);
	cdbexplain_depositSliceStats(&ctx.send.hdr, &ctx.recv);
}								/* cdbexplain_localExecStats */


/*
 * cdbexplain_localStatWalker
 */
static CdbVisitOpt
cdbexplain_localStatWalker(PlanState *planstate, void *context)
{
	CdbExplain_LocalStatCtx *ctx = (CdbExplain_LocalStatCtx *) context;

	/* Collect stats into our temporary StatInst and caller's extratextbuf. */
	cdbexplain_collectStatsFromNode(planstate, &ctx->send);

	/* Redeposit stats back into Instrumentation, and attach a NodeSummary. */
	cdbexplain_depositStatsToNode(planstate, &ctx->recv);

	/* Don't descend across a slice boundary. */
	if (IsA(planstate, MotionState))
		return CdbVisit_Skip;

	return CdbVisit_Walk;
}								/* cdbexplain_localStatWalker */


/*
 * cdbexplain_sendExecStats
 *	  Called by qExec process to send EXPLAIN ANALYZE statistics to qDisp.
 *	  On the qDisp, libpq will recognize our special message type ('Y') and
 *	  attach the message to the current command's PGresult object.
 */
void
cdbexplain_sendExecStats(QueryDesc *queryDesc)
{
	EState	   *estate;
	PlanState  *planstate;
	CdbExplain_SendStatCtx ctx;
	StringInfoData notebuf;
	StringInfoData memoryAccountTreeBuffer;

	/* Header offset (where header begins in the message buffer) */
	int			hoff;

	Assert(Gp_role == GP_ROLE_EXECUTE);

	if (!queryDesc ||
		!queryDesc->estate)
		return;

	/* If executing a root slice (UPD/DEL/INS), start at top of plan tree. */
	estate = queryDesc->estate;
	if (LocallyExecutingSliceIndex(estate) == RootSliceIndex(estate))
		planstate = queryDesc->planstate;

	/* Non-root slice: Start at child of our sending Motion node. */
	else
	{
		planstate = &(getMotionState(queryDesc->planstate, LocallyExecutingSliceIndex(estate))->ps);
		Assert(planstate &&
			   IsA(planstate, MotionState) &&
			   planstate->lefttree);
		planstate = planstate->lefttree;
	}

	if (planstate == NULL)
		return;

	/* Start building the message header in our context area. */
	memset(&ctx, 0, sizeof(ctx));
	ctx.hdr.type = T_CdbExplain_StatHdr;
	ctx.hdr.segindex = GpIdentity.segindex;
	ctx.hdr.nInst = 0;

	/* Allocate a separate buffer where nodes can append extra message text. */
	initStringInfo(&notebuf);
	ctx.notebuf = &notebuf;

	/* Reserve buffer space for the message header (excluding 'inst' array). */
	pq_beginmessage(&ctx.buf, 'Y');

	/* Where the actual StatHdr begins */
	hoff = ctx.buf.len;

	/*
	 * Write everything until inst member including "CdbExplain_SliceWorker
	 * worker"
	 */
	appendBinaryStringInfo(&ctx.buf, (char *) &ctx.hdr, sizeof(ctx.hdr) - sizeof(ctx.hdr.inst));

	/* Append statistics from each PlanState node in this slice. */
	planstate_walk_node(planstate, cdbexplain_sendStatWalker, &ctx);

	/* Obtain per-slice stats and put them in StatHdr. */
	cdbexplain_collectSliceStats(planstate, &ctx.hdr.worker);

	/* Append MemoryAccount Tree */
	ctx.hdr.memAccountStartOffset = ctx.buf.len - hoff;
	initStringInfo(&memoryAccountTreeBuffer);
	uint		totalSerialized = MemoryAccounting_Serialize(&memoryAccountTreeBuffer);

	ctx.hdr.memAccountCount = totalSerialized;
	appendBinaryStringInfo(&ctx.buf, memoryAccountTreeBuffer.data, memoryAccountTreeBuffer.len);
	pfree(memoryAccountTreeBuffer.data);

	/* Append the extra message text. */
	ctx.hdr.bnotes = ctx.buf.len - hoff;
	appendBinaryStringInfo(&ctx.buf, notebuf.data, notebuf.len);
	ctx.hdr.enotes = ctx.buf.len - hoff;
	pfree(notebuf.data);

	/*
	 * Move the message header into the buffer. Rewrite the updated header
	 * (with bnotes, enotes, nInst etc.) Note: this is the second time we are
	 * writing the header. The first write merely reserves space for the
	 * header
	 */
	memcpy(ctx.buf.data + hoff, (char *) &ctx.hdr, sizeof(ctx.hdr) - sizeof(ctx.hdr.inst));

#ifdef FAULT_INJECTOR
	/* Inject a fault before sending a message to qDisp process */
	SIMPLE_FAULT_INJECTOR("send_exec_stats");
#endif /* FAULT_INJECTOR */

	/* Send message to qDisp process. */
	pq_endmessage(&ctx.buf);
}								/* cdbexplain_sendExecStats */


/*
 * cdbexplain_sendStatWalker
 */
static CdbVisitOpt
cdbexplain_sendStatWalker(PlanState *planstate, void *context)
{
	CdbExplain_SendStatCtx *ctx = (CdbExplain_SendStatCtx *) context;
	CdbExplain_StatInst *si = &ctx->hdr.inst[0];

	/* Stuff stats into our temporary StatInst.  Add extra text to notebuf. */
	cdbexplain_collectStatsFromNode(planstate, ctx);

	/* Append StatInst instance to message. */
	appendBinaryStringInfo(&ctx->buf, (char *) si, sizeof(*si));
	ctx->hdr.nInst++;

	/* Don't descend across a slice boundary. */
	if (IsA(planstate, MotionState))
		return CdbVisit_Skip;

	return CdbVisit_Walk;
}								/* cdbexplain_sendStatWalker */


/*
 * cdbexplain_recvExecStats
 *	  Called by qDisp to transfer a slice's EXPLAIN ANALYZE statistics
 *	  from the CdbDispatchResults structures to the PlanState tree.
 *	  Recursively does the same for slices that are descendants of the
 *	  one specified.
 *
 * 'showstatctx' is a CdbExplain_ShowStatCtx object which was created by
 *		calling cdbexplain_showExecStatsBegin().
 */
void
cdbexplain_recvExecStats(struct PlanState *planstate,
						 struct CdbDispatchResults *dispatchResults,
						 int sliceIndex,
						 struct CdbExplain_ShowStatCtx *showstatctx)
{
	CdbDispatchResult *dispatchResultBeg;
	CdbDispatchResult *dispatchResultEnd;
	CdbExplain_RecvStatCtx ctx;
	CdbExplain_DispatchSummary ds;
	int			gpsegmentCount = getgpsegmentCount();
	int			iDispatch;
	int			nDispatch;
	int			imsgptr;

	if (!planstate ||
		!planstate->instrument ||
		!showstatctx)
		return;

	/*
	 * Note that the caller may free the CdbDispatchResults upon return, maybe
	 * before EXPLAIN ANALYZE examines the PlanState tree.  Consequently we
	 * must not return ptrs into the dispatch result buffers, but must copy
	 * any needed information into a sufficiently long-lived memory context.
	 */

	/* Initialize treewalk context. */
	memset(&ctx, 0, sizeof(ctx));
	ctx.dispatchResults = dispatchResults;
	ctx.extratextbuf = &showstatctx->extratextbuf;
	ctx.showstatctx = showstatctx;
	ctx.sliceIndex = sliceIndex;

	/* Find the slice's CdbDispatchResult objects. */
	dispatchResultBeg = cdbdisp_resultBegin(dispatchResults, sliceIndex);
	dispatchResultEnd = cdbdisp_resultEnd(dispatchResults, sliceIndex);
	nDispatch = dispatchResultEnd - dispatchResultBeg;

	/* Initialize worker counts. */
	memset(&ds, 0, sizeof(ds));
	ds.nResult = nDispatch;

	/* Find and validate the statistics returned from each qExec. */
	if (nDispatch > 0)
		ctx.msgptrs = (CdbExplain_StatHdr **) palloc0(nDispatch * sizeof(ctx.msgptrs[0]));
	for (iDispatch = 0; iDispatch < nDispatch; iDispatch++)
	{
		CdbDispatchResult *dispatchResult = &dispatchResultBeg[iDispatch];
		PGresult   *pgresult;
		CdbExplain_StatHdr *hdr;
		pgCdbStatCell *statcell;

		/* Update worker counts. */
		if (!dispatchResult->hasDispatched)
			ds.nNotDispatched++;
		else if (dispatchResult->wasCanceled)
			ds.nCanceled++;
		else if (dispatchResult->errcode)
			ds.nError++;
		else if (dispatchResult->okindex >= 0)
			ds.nOk++;			/* qExec returned successful completion */
		else
			ds.nIgnorableError++;	/* qExec returned an error that's likely a
									 * side-effect of another qExec's failure,
									 * e.g. an interconnect error */

		/* Find this qExec's last PGresult.  If none, skip to next qExec. */
		pgresult = cdbdisp_getPGresult(dispatchResult, -1);
		if (!pgresult)
			continue;

		/* Find our statistics in list of response messages.  If none, skip. */
		for (statcell = pgresult->cdbstats; statcell; statcell = statcell->next)
		{
			if (IsA((Node *) statcell->data, CdbExplain_StatHdr))
				break;
		}
		if (!statcell)
			continue;

		/* Validate the message header. */
		hdr = (CdbExplain_StatHdr *) statcell->data;
		if ((size_t) statcell->len < sizeof(*hdr) ||
			(size_t) statcell->len != (sizeof(*hdr) - sizeof(hdr->inst) +
									   hdr->nInst * sizeof(hdr->inst) +
									   hdr->memAccountCount * MemoryAccounting_SizeOfAccountInBytes() +
									   hdr->enotes - hdr->bnotes) ||
			statcell->len != hdr->enotes ||
			hdr->segindex < -1 ||
			hdr->segindex >= gpsegmentCount)
		{
			ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
							errmsg_internal("Invalid execution statistics "
											"response returned from seg%d.  "
											"length=%d",
											hdr->segindex,
											statcell->len),
							errhint("Please verify that all instances are using "
									"the correct %s software version.",
									PACKAGE_NAME)
							));
		}

		/* Slice should have same number of plan nodes on every qExec. */
		if (iDispatch == 0)
			ctx.nStatInst = hdr->nInst;
		else
		{
			/* Check for stats corruption */
			if (ctx.nStatInst != hdr->nInst)
				ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
								errmsg("Invalid execution statistics "
									   "received stats node-count mismatch: cdbexplain_recvExecStats() ctx.nStatInst %d hdr->nInst %d", ctx.nStatInst, hdr->nInst),
								errhint("Please verify that all instances are using "
										"the correct %s software version.",
										PACKAGE_NAME)));

			Insist(ctx.nStatInst == hdr->nInst);
		}

		/* Save lowest and highest segment id for which we have stats. */
		if (iDispatch == 0)
			ctx.segindexMin = ctx.segindexMax = hdr->segindex;
		else if (ctx.segindexMax < hdr->segindex)
			ctx.segindexMax = hdr->segindex;
		else if (ctx.segindexMin > hdr->segindex)
			ctx.segindexMin = hdr->segindex;

		/* Save message ptr for easy reference. */
		ctx.msgptrs[ctx.nmsgptr] = hdr;
		ctx.nmsgptr++;
	}

	/* Attach NodeSummary to each PlanState node's Instrumentation node. */
	planstate_walk_node(planstate, cdbexplain_recvStatWalker, &ctx);

	/* Make sure we visited the right number of PlanState nodes. */
	Insist(ctx.iStatInst == ctx.nStatInst);

	/* Transfer per-slice stats from message headers to the SliceSummary. */
	for (imsgptr = 0; imsgptr < ctx.nmsgptr; imsgptr++)
		cdbexplain_depositSliceStats(ctx.msgptrs[imsgptr], &ctx);

	/* Transfer worker counts to SliceSummary. */
	showstatctx->slices[sliceIndex].dispatchSummary = ds;

	/* Signal that we've gathered all the statistics
	 * For some query, which has initplan on top of the plan,
	 * its `ANALYZE EXPLAIN` invoke `cdbexplain_recvExecStats`
	 * multi-times in different recursive routine to collect
	 * metrics on both initplan and plan. Thus, this variable
	 * should only assign on slice 0 after gather result done
	 * to promise all slices information have been collected.
	 */
	if (sliceIndex == 0)
		showstatctx->stats_gathered = true;

	/* Clean up. */
	if (ctx.msgptrs)
		pfree(ctx.msgptrs);
}								/* cdbexplain_recvExecStats */


/*
 * cdbexplain_recvStatWalker
 *	  Update the given PlanState node's Instrument node with statistics
 *	  received from qExecs.  Attach a CdbExplain_NodeSummary block to
 *	  the Instrument node.  At a MotionState node, descend to child slice.
 */
static CdbVisitOpt
cdbexplain_recvStatWalker(PlanState *planstate, void *context)
{
	CdbExplain_RecvStatCtx *ctx = (CdbExplain_RecvStatCtx *) context;

	/* If slice was dispatched to qExecs, and stats came back, grab 'em. */
	if (ctx->nmsgptr > 0)
	{
		/* Transfer received stats to Instrumentation, NodeSummary, etc. */
		cdbexplain_depositStatsToNode(planstate, ctx);

		/* Advance to next node's entry in all of the StatInst arrays. */
		ctx->iStatInst++;
	}

	/* Motion operator?  Descend to next slice. */
	if (IsA(planstate, MotionState))
	{
		cdbexplain_recvExecStats(planstate->lefttree,
								 ctx->dispatchResults,
								 ((Motion *) planstate->plan)->motionID,
								 ctx->showstatctx);
		return CdbVisit_Skip;
	}

	return CdbVisit_Walk;
}								/* cdbexplain_recvStatWalker */


/*
 * cdbexplain_collectSliceStats
 *	  Obtain per-slice statistical observations from the current slice
 *	  (which has just completed execution in the current process) and
 *	  store the information in the given SliceWorker struct.
 *
 * 'planstate' is the top PlanState node of the current slice.
 */
static void
cdbexplain_collectSliceStats(PlanState *planstate,
							 CdbExplain_SliceWorker *out_worker)
{
	EState	   *estate = planstate->state;

	/* Max bytes malloc'ed under executor's per-query memory context. */
	out_worker->peakmemused =
		(double) MemoryContextGetPeakSpace(estate->es_query_cxt);

	out_worker->vmem_reserved = (double) VmemTracker_GetMaxReservedVmemBytes();

	out_worker->memory_accounting_global_peak = (double) MemoryAccounting_GetGlobalPeak();

}								/* cdbexplain_collectSliceStats */


/*
 * cdbexplain_depositSliceStats
 *	  Transfer a worker's per-slice stats contribution from StatHdr into the
 *	  SliceSummary array in the ShowStatCtx.  Transfer the rollup of per-node
 *	  stats from the RecvStatCtx into the SliceSummary.
 *
 * Kludge: In a non-parallel plan, slice numbers haven't been assigned, so we
 * may be called more than once with sliceIndex == 0: once for the outermost
 * query and once for each InitPlan subquery.  In this case we dynamically
 * expand the SliceSummary array.  CDB TODO: Always assign proper root slice
 * ids (in qDispSliceId field of SubPlan node); then remove this kludge.
 */
static void
cdbexplain_depositSliceStats(CdbExplain_StatHdr *hdr,
							 CdbExplain_RecvStatCtx *recvstatctx)
{
	int			sliceIndex = recvstatctx->sliceIndex;
	CdbExplain_ShowStatCtx *showstatctx = recvstatctx->showstatctx;
	CdbExplain_SliceSummary *ss = &showstatctx->slices[sliceIndex];
	CdbExplain_SliceWorker *ssw;
	int			iworker;

	Insist(sliceIndex >= 0 &&
		   sliceIndex < showstatctx->nslice);

	/* Kludge:	QD can have more than one 'Slice 0' if plan is non-parallel. */
	if (sliceIndex == 0 &&
		recvstatctx->dispatchResults == NULL &&
		ss->workers)
	{
		Assert(ss->nworker == 1 &&
			   recvstatctx->segindexMin == hdr->segindex &&
			   recvstatctx->segindexMax == hdr->segindex);

		/* Expand the SliceSummary array to make room for InitPlan subquery. */
		sliceIndex = showstatctx->nslice++;
		showstatctx->slices = (CdbExplain_SliceSummary *)
			repalloc(showstatctx->slices, showstatctx->nslice * sizeof(showstatctx->slices[0]));
		ss = &showstatctx->slices[sliceIndex];
		memset(ss, 0, sizeof(*ss));
	}

	/* Slice's first worker? */
	if (!ss->workers)
	{
		/* Allocate SliceWorker array and attach it to the SliceSummary. */
		ss->segindex0 = recvstatctx->segindexMin;
		ss->nworker = recvstatctx->segindexMax + 1 - ss->segindex0;
		ss->workers = (CdbExplain_SliceWorker *) palloc0(ss->nworker * sizeof(ss->workers[0]));
		ss->memoryAccounts = (void **) palloc0(ss->nworker * sizeof(ss->memoryAccounts[0]));
		ss->memoryAccountCount = (MemoryAccountIdType *) palloc0(ss->nworker * sizeof(ss->memoryAccountCount[0]));
	}

	/* Save a copy of this SliceWorker instance in the worker array. */
	iworker = hdr->segindex - ss->segindex0;
	ssw = &ss->workers[iworker];
	Insist(iworker >= 0 && iworker < ss->nworker);
	Insist(ssw->peakmemused == 0);	/* each worker should be seen just once */
	*ssw = hdr->worker;

	const char *originalSerializedMemoryAccountingStartAddress = ((const char *) hdr) +
	hdr->memAccountStartOffset;

	size_t		byteCount = MemoryAccounting_SizeOfAccountInBytes() * hdr->memAccountCount;

	/*
	 * We need to copy of the serialized bits. These bits have shorter
	 * lifespan and can get out of scope before we finish explain analyze.
	 */
	void	   *copiedSerializedMemoryAccountingStartAddress = palloc(byteCount);

	memcpy(copiedSerializedMemoryAccountingStartAddress, originalSerializedMemoryAccountingStartAddress, byteCount);

	ss->memoryAccounts[iworker] = copiedSerializedMemoryAccountingStartAddress;
	ss->memoryAccountCount[iworker] = hdr->memAccountCount;

	/* Rollup of per-worker stats into SliceSummary */
	cdbexplain_agg_upd(&ss->peakmemused, hdr->worker.peakmemused, hdr->segindex);
	cdbexplain_agg_upd(&ss->vmem_reserved, hdr->worker.vmem_reserved, hdr->segindex);
	cdbexplain_agg_upd(&ss->memory_accounting_global_peak, hdr->worker.memory_accounting_global_peak, hdr->segindex);

	/* Rollup of per-node stats over all nodes of the slice into SliceSummary */
	ss->workmemused_max = recvstatctx->workmemused_max;
	ss->workmemwanted_max = recvstatctx->workmemwanted_max;

	/* Rollup of per-node stats over the whole query into ShowStatCtx. */
	showstatctx->workmemused_max = Max(showstatctx->workmemused_max, recvstatctx->workmemused_max);
	showstatctx->workmemwanted_max = Max(showstatctx->workmemwanted_max, recvstatctx->workmemwanted_max);
}								/* cdbexplain_depositSliceStats */


/*
 * cdbexplain_collectStatsFromNode
 *
 * Called by sendStatWalker and localStatWalker to obtain a node's statistics
 * and transfer them into the temporary StatHdr and StatInst in the SendStatCtx.
 * Also obtains the node's extra message text, which it appends to the caller's
 * cxt->nodebuf.
 */
static void
cdbexplain_collectStatsFromNode(PlanState *planstate, CdbExplain_SendStatCtx *ctx)
{
	CdbExplain_StatInst *si = &ctx->hdr.inst[0];
	Instrumentation *instr = planstate->instrument;

	Insist(instr);

	/* We have to finalize statistics, since ExecutorEnd hasn't been called. */
	InstrEndLoop(instr);

	/* Initialize the StatInst slot in the temporary StatHdr. */
	memset(si, 0, sizeof(*si));
	si->pstype = planstate->type;

	/* Add this node's extra message text to notebuf.  Store final stats. */
	si->bnotes = cdbexplain_collectExtraText(planstate, ctx->notebuf);
	si->enotes = ctx->notebuf->len;

	/* Make sure there is a '\0' between this node's message and the next. */
	if (si->bnotes < si->enotes)
		appendStringInfoChar(ctx->notebuf, '\0');

	/* Transfer this node's statistics from Instrumentation into StatInst. */
	si->starttime = instr->starttime;
	si->counter = instr->counter;
	si->firsttuple = instr->firsttuple;
	si->startup = instr->startup;
	si->total = instr->total;
	si->ntuples = instr->ntuples;
	si->nloops = instr->nloops;
	si->execmemused = instr->execmemused;
	si->workmemused = instr->workmemused;
	si->workmemwanted = instr->workmemwanted;
	si->workfileCreated = instr->workfileCreated;
	si->peakMemBalance = MemoryAccounting_GetAccountPeakBalance(planstate->memoryAccountId);
	si->firststart = instr->firststart;
	si->numPartScanned = instr->numPartScanned;
	si->sortMethod = String2ExplainSortMethod(instr->sortMethod);
	si->sortSpaceType = String2ExplainSortSpaceType(instr->sortSpaceType, si->sortMethod);
	si->sortSpaceUsed = instr->sortSpaceUsed;
}								/* cdbexplain_collectStatsFromNode */


/*
 * CdbExplain_DepStatAcc
 *	  Segment statistic accumulator used by cdbexplain_depositStatsToNode().
 */
typedef struct CdbExplain_DepStatAcc
{
	/* vmax, vsum, vcnt, segmax */
	CdbExplain_Agg agg;
	/* max's received StatHdr */
	CdbExplain_StatHdr *rshmax;
	/* max's received inst in StatHdr */
	CdbExplain_StatInst *rsimax;
	/* max's inst in NodeSummary */
	CdbExplain_StatInst *nsimax;
	/* max run-time of all the segments */
	double		max_total;
	/* start time of the first iteration for node with maximum runtime */
	instr_time	firststart_of_max_total;
} CdbExplain_DepStatAcc;

static void
cdbexplain_depStatAcc_init0(CdbExplain_DepStatAcc *acc)
{
	cdbexplain_agg_init0(&acc->agg);
	acc->rshmax = NULL;
	acc->rsimax = NULL;
	acc->nsimax = NULL;
	acc->max_total = 0;
	INSTR_TIME_SET_ZERO(acc->firststart_of_max_total);
}								/* cdbexplain_depStatAcc_init0 */

static inline void
cdbexplain_depStatAcc_upd(CdbExplain_DepStatAcc *acc,
						  double v,
						  CdbExplain_StatHdr *rsh,
						  CdbExplain_StatInst *rsi,
						  CdbExplain_StatInst *nsi)
{
	if (cdbexplain_agg_upd(&acc->agg, v, rsh->segindex))
	{
		acc->rshmax = rsh;
		acc->rsimax = rsi;
		acc->nsimax = nsi;
	}

	if (acc->max_total < nsi->total)
	{
		acc->max_total = nsi->total;
		INSTR_TIME_ASSIGN(acc->firststart_of_max_total, nsi->firststart);
	}
}								/* cdbexplain_depStatAcc_upd */

static void
cdbexplain_depStatAcc_saveText(CdbExplain_DepStatAcc *acc,
							   StringInfoData *extratextbuf,
							   bool *saved_inout)
{
	CdbExplain_StatHdr *rsh = acc->rshmax;
	CdbExplain_StatInst *rsi = acc->rsimax;
	CdbExplain_StatInst *nsi = acc->nsimax;

	if (acc->agg.vcnt > 0 &&
		nsi->bnotes == nsi->enotes &&
		rsi->bnotes < rsi->enotes)
	{
		/* Locate extra message text in dispatch result buffer. */
		int			notelen = rsi->enotes - rsi->bnotes;
		const char *notes = (const char *) rsh + rsh->bnotes + rsi->bnotes;

		Insist(rsh->bnotes + rsi->enotes < rsh->enotes &&
			   notes[notelen] == '\0');

		/* Append to extratextbuf. */
		nsi->bnotes = extratextbuf->len;
		appendBinaryStringInfo(extratextbuf, notes, notelen);
		nsi->enotes = extratextbuf->len;

		/* Tell caller that some extra text has been saved. */
		if (saved_inout)
			*saved_inout = true;
	}
}								/* cdbexplain_depStatAcc_saveText */


/*
 * cdbexplain_depositStatsToNode
 *
 * Called by recvStatWalker and localStatWalker to update the given
 * PlanState node's Instrument node with statistics received from
 * workers or collected locally.  Attaches a CdbExplain_NodeSummary
 * block to the Instrument node.  If top node of slice, per-slice
 * statistics are transferred from the StatHdr to the SliceSummary.
 */
static void
cdbexplain_depositStatsToNode(PlanState *planstate, CdbExplain_RecvStatCtx *ctx)
{
	Instrumentation *instr = planstate->instrument;
	CdbExplain_StatHdr *rsh;	/* The header (which includes StatInst) */
	CdbExplain_StatInst *rsi;	/* The current StatInst */

	/*
	 * Points to the insts array of node summary (CdbExplain_NodeSummary).
	 * Used for saving every rsi in the node summary (in addition to saving
	 * the max/avg).
	 */
	CdbExplain_StatInst *nsi;

	/*
	 * ns is the node summary across all QEs of the segworker group. It also
	 * contains detailed "unsummarized" raw stat for a node across all QEs in
	 * current segworker group (in the insts array)
	 */
	CdbExplain_NodeSummary *ns;
	CdbExplain_DepStatAcc ntuples;
	CdbExplain_DepStatAcc execmemused;
	CdbExplain_DepStatAcc workmemused;
	CdbExplain_DepStatAcc workmemwanted;
	CdbExplain_DepStatAcc totalWorkfileCreated;
	CdbExplain_DepStatAcc peakmemused;
	CdbExplain_DepStatAcc vmem_reserved;
	CdbExplain_DepStatAcc memory_accounting_global_peak;
	CdbExplain_DepStatAcc peakMemBalance;
	CdbExplain_DepStatAcc totalPartTableScanned;
	CdbExplain_DepStatAcc sortSpaceUsed[NUM_SORT_SPACE_TYPE][NUM_SORT_METHOD];
	int			imsgptr;
	int			nInst;

	Insist(instr &&
		   ctx->iStatInst < ctx->nStatInst);

	/* Allocate NodeSummary block. */
	nInst = ctx->segindexMax + 1 - ctx->segindexMin;
	ns = (CdbExplain_NodeSummary *) palloc0(sizeof(*ns) - sizeof(ns->insts) +
											nInst * sizeof(ns->insts[0]));
	ns->segindex0 = ctx->segindexMin;
	ns->ninst = nInst;

	/* Attach our new NodeSummary to the Instrumentation node. */
	instr->cdbNodeSummary = ns;

	/* Initialize per-node accumulators. */
	cdbexplain_depStatAcc_init0(&ntuples);
	cdbexplain_depStatAcc_init0(&execmemused);
	cdbexplain_depStatAcc_init0(&workmemused);
	cdbexplain_depStatAcc_init0(&workmemwanted);
	cdbexplain_depStatAcc_init0(&totalWorkfileCreated);
	cdbexplain_depStatAcc_init0(&peakMemBalance);
	cdbexplain_depStatAcc_init0(&totalPartTableScanned);
	for (int idx = 0; idx < NUM_SORT_METHOD; ++idx)
	{
		cdbexplain_depStatAcc_init0(&sortSpaceUsed[MEMORY_SORT_SPACE_TYPE - 1][idx]);
		cdbexplain_depStatAcc_init0(&sortSpaceUsed[DISK_SORT_SPACE_TYPE - 1][idx]);
	}

	/* Initialize per-slice accumulators. */
	cdbexplain_depStatAcc_init0(&peakmemused);
	cdbexplain_depStatAcc_init0(&vmem_reserved);
	cdbexplain_depStatAcc_init0(&memory_accounting_global_peak);

	/* Examine the statistics from each qExec. */
	for (imsgptr = 0; imsgptr < ctx->nmsgptr; imsgptr++)
	{
		/* Locate PlanState node's StatInst received from this qExec. */
		rsh = ctx->msgptrs[imsgptr];
		rsi = &rsh->inst[ctx->iStatInst];

		Insist(rsi->pstype == planstate->type &&
			   ns->segindex0 <= rsh->segindex &&
			   rsh->segindex < ns->segindex0 + ns->ninst);

		/* Locate this qExec's StatInst slot in node's NodeSummary block. */
		nsi = &ns->insts[rsh->segindex - ns->segindex0];

		/* Copy the StatInst to NodeSummary from dispatch result buffer. */
		*nsi = *rsi;

		/*
		 * Drop qExec's extra text.  We rescue it below if qExec is a winner.
		 * For local qDisp slice, ctx->extratextbuf is NULL, which tells us to
		 * leave the extra text undisturbed in its existing buffer.
		 */
		if (ctx->extratextbuf)
			nsi->bnotes = nsi->enotes = 0;

		/* Update per-node accumulators. */
		cdbexplain_depStatAcc_upd(&ntuples, rsi->ntuples, rsh, rsi, nsi);
		cdbexplain_depStatAcc_upd(&execmemused, rsi->execmemused, rsh, rsi, nsi);
		cdbexplain_depStatAcc_upd(&workmemused, rsi->workmemused, rsh, rsi, nsi);
		cdbexplain_depStatAcc_upd(&workmemwanted, rsi->workmemwanted, rsh, rsi, nsi);
		cdbexplain_depStatAcc_upd(&totalWorkfileCreated, (rsi->workfileCreated ? 1 : 0), rsh, rsi, nsi);
		cdbexplain_depStatAcc_upd(&peakMemBalance, rsi->peakMemBalance, rsh, rsi, nsi);
		cdbexplain_depStatAcc_upd(&totalPartTableScanned, rsi->numPartScanned, rsh, rsi, nsi);
		if (rsi->sortMethod < NUM_SORT_METHOD && rsi->sortMethod != UNINITIALIZED_SORT && rsi->sortSpaceType != UNINITIALIZED_SORT_SPACE_TYPE)
		{
			Assert(rsi->sortSpaceType <= NUM_SORT_SPACE_TYPE);
			cdbexplain_depStatAcc_upd(&sortSpaceUsed[rsi->sortSpaceType - 1][rsi->sortMethod - 1], (double) rsi->sortSpaceUsed, rsh, rsi, nsi);
		}

		/* Update per-slice accumulators. */
		cdbexplain_depStatAcc_upd(&peakmemused, rsh->worker.peakmemused, rsh, rsi, nsi);
		cdbexplain_depStatAcc_upd(&vmem_reserved, rsh->worker.vmem_reserved, rsh, rsi, nsi);
		cdbexplain_depStatAcc_upd(&memory_accounting_global_peak, rsh->worker.memory_accounting_global_peak, rsh, rsi, nsi);
	}

	/* Save per-node accumulated stats in NodeSummary. */
	ns->ntuples = ntuples.agg;
	ns->execmemused = execmemused.agg;
	ns->workmemused = workmemused.agg;
	ns->workmemwanted = workmemwanted.agg;
	ns->totalWorkfileCreated = totalWorkfileCreated.agg;
	ns->peakMemBalance = peakMemBalance.agg;
	ns->totalPartTableScanned = totalPartTableScanned.agg;
	for (int idx = 0; idx < NUM_SORT_METHOD; ++idx)
	{
		ns->sortSpaceUsed[MEMORY_SORT_SPACE_TYPE - 1][idx] = sortSpaceUsed[MEMORY_SORT_SPACE_TYPE - 1][idx].agg;
		ns->sortSpaceUsed[DISK_SORT_SPACE_TYPE - 1][idx] = sortSpaceUsed[DISK_SORT_SPACE_TYPE - 1][idx].agg;
	}

	/* Roll up summary over all nodes of slice into RecvStatCtx. */
	ctx->workmemused_max = Max(ctx->workmemused_max, workmemused.agg.vmax);
	ctx->workmemwanted_max = Max(ctx->workmemwanted_max, workmemwanted.agg.vmax);

	instr->total = ntuples.max_total;
	INSTR_TIME_ASSIGN(instr->firststart, ntuples.firststart_of_max_total);

	/* Put winner's stats into qDisp PlanState's Instrument node. */
	if (ntuples.agg.vcnt > 0)
	{
		instr->starttime = ntuples.nsimax->starttime;
		instr->counter = ntuples.nsimax->counter;
		instr->firsttuple = ntuples.nsimax->firsttuple;
		instr->startup = ntuples.nsimax->startup;
		instr->total = ntuples.nsimax->total;
		instr->ntuples = ntuples.nsimax->ntuples;
		instr->nloops = ntuples.nsimax->nloops;
		instr->execmemused = ntuples.nsimax->execmemused;
		instr->workmemused = ntuples.nsimax->workmemused;
		instr->workmemwanted = ntuples.nsimax->workmemwanted;
		instr->workfileCreated = ntuples.nsimax->workfileCreated;
		instr->firststart = ntuples.nsimax->firststart;
	}

	/* Save extra message text for the most interesting winning qExecs. */
	if (ctx->extratextbuf)
	{
		bool		saved = false;

		/* One worker which used or wanted the most work_mem */
		if (workmemwanted.agg.vmax >= workmemused.agg.vmax)
			cdbexplain_depStatAcc_saveText(&workmemwanted, ctx->extratextbuf, &saved);
		else if (workmemused.agg.vmax > 1.05 * cdbexplain_agg_avg(&workmemused.agg))
			cdbexplain_depStatAcc_saveText(&workmemused, ctx->extratextbuf, &saved);

		/* Worker which used the most executor memory (this node's usage) */
		if (execmemused.agg.vmax > 1.05 * cdbexplain_agg_avg(&execmemused.agg))
			cdbexplain_depStatAcc_saveText(&execmemused, ctx->extratextbuf, &saved);

		/*
		 * For the worker which had the highest peak executor memory usage
		 * overall across the whole slice, we'll report the extra message text
		 * from all of the nodes in the slice.  But only if that worker stands
		 * out more than 5% above the average.
		 */
		if (peakmemused.agg.vmax > 1.05 * cdbexplain_agg_avg(&peakmemused.agg))
			cdbexplain_depStatAcc_saveText(&peakmemused, ctx->extratextbuf, &saved);

		/*
		 * One worker which produced the greatest number of output rows.
		 * (Always give at least one node a chance to have its extra message
		 * text seen.  In case no node stood out above the others, make a
		 * repeatable choice based on the number of output rows.)
		 */
		if (!saved ||
			ntuples.agg.vmax > 1.05 * cdbexplain_agg_avg(&ntuples.agg))
			cdbexplain_depStatAcc_saveText(&ntuples, ctx->extratextbuf, &saved);
	}
}								/* cdbexplain_depositStatsToNode */


/*
 * cdbexplain_collectExtraText
 *	  Allow a node to supply additional text for its EXPLAIN ANALYZE report.
 *
 * Returns the starting offset of the extra message text from notebuf->data.
 * The caller can compute the length as notebuf->len minus the starting offset.
 * If the node did not provide any extra message text, the length will be 0.
 */
static int
cdbexplain_collectExtraText(PlanState *planstate, StringInfo notebuf)
{
	int			bnotes = notebuf->len;

	/*
	 * Invoke node's callback.  It may append to our notebuf and/or its own
	 * cdbexplainbuf; and store final statistics in its Instrumentation node.
	 */
	if (planstate->cdbexplainfun)
		planstate->cdbexplainfun(planstate, notebuf);

	/*
	 * Append contents of node's extra message buffer.  This allows nodes to
	 * contribute EXPLAIN ANALYZE info without having to set up a callback.
	 */
	if (planstate->cdbexplainbuf && planstate->cdbexplainbuf->len > 0)
	{
		/* If callback added to notebuf, make sure text ends with a newline. */
		if (bnotes < notebuf->len &&
			notebuf->data[notebuf->len - 1] != '\n')
			appendStringInfoChar(notebuf, '\n');

		appendBinaryStringInfo(notebuf, planstate->cdbexplainbuf->data,
							   planstate->cdbexplainbuf->len);

		resetStringInfo(planstate->cdbexplainbuf);
	}

	return bnotes;
}								/* cdbexplain_collectExtraText */


/*
 * cdbexplain_formatExtraText
 *	  Format extra message text into the EXPLAIN output buffer.
 */
static void
cdbexplain_formatExtraText(StringInfo str,
						   int indent,
						   int segindex,
						   const char *notes,
						   int notelen)
{
	const char *cp = notes;
	const char *ep = notes + notelen;

	/* Could be more than one line... */
	while (cp < ep)
	{
		const char *nlp = strchr(cp, '\n');
		const char *dp = nlp ? nlp : ep;

		/* Strip trailing whitespace. */
		while (cp < dp &&
			   isspace(dp[-1]))
			dp--;

		/* Add to output buffer. */
		if (cp < dp)
		{
			appendStringInfoSpaces(str, indent * 2);
			if (segindex >= 0)
			{
				appendStringInfo(str, "(seg%d) ", segindex);
				if (segindex < 10)
					appendStringInfoChar(str, ' ');
				if (segindex < 100)
					appendStringInfoChar(str, ' ');
			}
			appendBinaryStringInfo(str, cp, dp - cp);
			if (nlp)
				appendStringInfoChar(str, '\n');
		}

		if (!nlp)
			break;
		cp = nlp + 1;
	}
}								/* cdbexplain_formatExtraText */



/*
 * cdbexplain_formatMemory
 *	  Convert memory size to string from (double) bytes.
 *
 *		outbuf:  [output] pointer to a char buffer to be filled
 *		bufsize: [input] maximum number of characters to write to outbuf (must be set by the caller)
 *		bytes:	 [input] a value representing memory size in bytes to be written to outbuf
 */
static void
cdbexplain_formatMemory(char *outbuf, int bufsize, double bytes)
{
	Assert(outbuf != NULL && "CDBEXPLAIN: char buffer is null");
	Assert(bufsize > 0 && "CDBEXPLAIN: size of char buffer is zero");
	/* check if truncation occurs */
#ifdef USE_ASSERT_CHECKING
	int			nchars_written =
#endif							/* USE_ASSERT_CHECKING */
	snprintf(outbuf, bufsize, "%.0fK bytes", kb(bytes));

	Assert(nchars_written < bufsize &&
		   "CDBEXPLAIN:  size of char buffer is smaller than the required number of chars");
}								/* cdbexplain_formatMemory */



/*
 * cdbexplain_formatSeconds
 *	  Convert time in seconds to readable string
 *
 *		outbuf:  [output] pointer to a char buffer to be filled
 *		bufsize: [input] maximum number of characters to write to outbuf (must be set by the caller)
 *		seconds: [input] a value representing no. of seconds to be written to outbuf
 */
static void
cdbexplain_formatSeconds(char *outbuf, int bufsize, double seconds, bool unit)
{
	Assert(outbuf != NULL && "CDBEXPLAIN: char buffer is null");
	Assert(bufsize > 0 && "CDBEXPLAIN: size of char buffer is zero");
	double		ms = seconds * 1000.0;

	/* check if truncation occurs */
#ifdef USE_ASSERT_CHECKING
	int			nchars_written =
#endif							/* USE_ASSERT_CHECKING */
	snprintf(outbuf, bufsize, "%.*f%s",
			 (ms < 10.0 && ms != 0.0 && ms > -10.0) ? 3 : 0,
			 ms, (unit ? " ms" : ""));

	Assert(nchars_written < bufsize &&
		   "CDBEXPLAIN:  size of char buffer is smaller than the required number of chars");
}								/* cdbexplain_formatSeconds */


/*
 * cdbexplain_formatSeg
 *	  Convert segment id to string.
 *
 *		outbuf:  [output] pointer to a char buffer to be filled
 *		bufsize: [input] maximum number of characters to write to outbuf (must be set by the caller)
 *		segindex:[input] a value representing segment index to be written to outbuf
 *		nInst:	 [input] no. of stat instances
 */
static void
cdbexplain_formatSeg(char *outbuf, int bufsize, int segindex, int nInst)
{
	Assert(outbuf != NULL && "CDBEXPLAIN: char buffer is null");
	Assert(bufsize > 0 && "CDBEXPLAIN: size of char buffer is zero");

	if (nInst > 1 && segindex >= 0)
	{
		/* check if truncation occurs */
#ifdef USE_ASSERT_CHECKING
		int			nchars_written =
#endif							/* USE_ASSERT_CHECKING */
		snprintf(outbuf, bufsize, " (seg%d)", segindex);

		Assert(nchars_written < bufsize &&
			   "CDBEXPLAIN:  size of char buffer is smaller than the required number of chars");
	}
	else
	{
		outbuf[0] = '\0';
	}
}								/* cdbexplain_formatSeg */


/*
 * cdbexplain_showExecStatsBegin
 *	  Called by qDisp process to create a CdbExplain_ShowStatCtx structure
 *	  in which to accumulate overall statistics for a query.
 *
 * 'querystarttime' is the timestamp of the start of the query, in a
 *		platform-dependent format.
 *
 * Note this function is called before ExecutorStart(), so there is no EState
 * or SliceTable yet.
 */
struct CdbExplain_ShowStatCtx *
cdbexplain_showExecStatsBegin(struct QueryDesc *queryDesc,
							  instr_time querystarttime)
{
	CdbExplain_ShowStatCtx *ctx;
	int			nslice;

	Assert(Gp_role != GP_ROLE_EXECUTE);

	/* Allocate and zero the ShowStatCtx */
	ctx = (CdbExplain_ShowStatCtx *) palloc0(sizeof(*ctx));

	ctx->querystarttime = querystarttime;

	/* Determine number of slices.  (SliceTable hasn't been built yet.) */
	nslice = 1 + queryDesc->plannedstmt->planTree->nMotionNodes + queryDesc->plannedstmt->planTree->nInitPlans;

	/* Allocate and zero the SliceSummary array. */
	ctx->nslice = nslice;
	ctx->slices = (CdbExplain_SliceSummary *) palloc0(nslice * sizeof(ctx->slices[0]));

	/* Allocate a buffer in which we can collect any extra message text. */
	initStringInfoOfSize(&ctx->extratextbuf, 4000);

	return ctx;
}								/* cdbexplain_showExecStatsBegin */

/*
 * cdbexplain_showStatCtxFree
 *	  Release memory allocated for CdbExplain_ShowStatCtx structure and its
 *	  internals. Memory for insides of the slices array elements is allocated
 *	  in ExplainPrintPlan(). If ExplainPrintPlan() is called from the
 *	  auto_explain extension, then this memory is released in
 *	  standard_ExecutorEnd() -> FreeExecutorState() to avoid memory leak in the
 *	  case of queries with multiple call of SQL functions.
 *	  If ExplainPrintPlan() is called from ExplainOnePlan(), then this memory
 *	  is released in PortalDrop().
 */
void
cdbexplain_showStatCtxFree(struct CdbExplain_ShowStatCtx *ctx)
{
	Assert(ctx != NULL);

	pfree(ctx->extratextbuf.data);
	pfree(ctx->slices);
	pfree(ctx);
}

/*
 * nodeSupportWorkfileCaching
 *	 Return true if a given node supports workfile caching.
 */
static bool
nodeSupportWorkfileCaching(PlanState *planstate)
{
	return (IsA(planstate, SortState) ||
			IsA(planstate, HashJoinState) ||
			(IsA(planstate, AggState) &&((Agg *) planstate->plan)->aggstrategy == AGG_HASHED) ||
			IsA(planstate, MaterialState));
}

/*
 * cdbexplain_showExecStats
 *	  Called by qDisp process to format a node's EXPLAIN ANALYZE statistics.
 *
 * 'planstate' is the node whose statistics are to be displayed.
 * 'str' is the output buffer.
 * 'indent' is the root indentation for all the text generated for explain output
 * 'ctx' is a CdbExplain_ShowStatCtx object which was created by a call to
 *		cdbexplain_showExecStatsBegin().
 */
static void
cdbexplain_showExecStats(struct PlanState *planstate, ExplainState *es)
{
	struct CdbExplain_ShowStatCtx *ctx = es->showstatctx;
	Instrumentation *instr = planstate->instrument;
	CdbExplain_NodeSummary *ns = instr->cdbNodeSummary;
	instr_time	timediff;
	int			i;

	char		totalbuf[50];
	char		avgbuf[50];
	char		maxbuf[50];
	char		segbuf[50];
	char		startbuf[50];

	/* Might not have received stats from qExecs if they hit errors. */
	if (!ns)
		return;

	Assert(instr != NULL);

	if ((EXPLAIN_MEMORY_VERBOSITY_DETAIL <= explain_memory_verbosity)
		&& planstate->type == T_MotionState)
	{
		Motion	   *pMotion = (Motion *) planstate->plan;
		int			curSliceId = pMotion->motionID;

		for (int iWorker = 0; iWorker < ctx->slices[curSliceId].nworker; iWorker++)
		{
			void *memacc = ctx->slices[curSliceId].memoryAccounts[iWorker];
			int segno = iWorker + ctx->slices[curSliceId].segindex0;
			/*
			 * The content id of QEs in slice may not be continuous when Direct Dispatch is enabled in this slice,
			 * which means memacc may be uninitialized by cdbexplain_depositSliceStats(), and the initial value of
			 * memacc is NULL.
			 */
			if (memacc == NULL)
				continue;

			if (es->format == EXPLAIN_FORMAT_TEXT)
			{
				appendStringInfoSpaces(es->str, es->indent * 2);
				appendStringInfo(es->str, "slice %d, seg %d\n", curSliceId, segno);
			}
			else
			{
				ExplainOpenGroup("MemoryAccounting", NULL, false, es);
				ExplainPropertyInteger("Slice", curSliceId, es);
				ExplainPropertyInteger("Segment", segno, es);
			}

			MemoryAccounting_CombinedAccountArrayToExplain(memacc,
														   ctx->slices[curSliceId].memoryAccountCount[iWorker],
														   es);
			if (es->format != EXPLAIN_FORMAT_TEXT)
				ExplainCloseGroup("MemoryAccounting", NULL, false, es);
		}
	}

	/*
	 * Executor memory used by this individual node, if it allocates from a
	 * memory context of its own instead of sharing the per-query context.
	 */
	if (es->analyze && es->verbose && ns->execmemused.vcnt > 0)
	{
		if (es->format == EXPLAIN_FORMAT_TEXT)
		{
			appendStringInfoSpaces(es->str, es->indent * 2);
			appendStringInfo(es->str, "Executor Memory: %ldkB  Segments: %d  Max: %ldkB (segment %d)\n",
							 (long) kb(ns->execmemused.vsum),
							 ns->execmemused.vcnt,
							 (long) kb(ns->execmemused.vmax),
							 ns->execmemused.imax);
		}
		else
		{
			ExplainPropertyLong("Executor Memory", (long) kb(ns->execmemused.vsum), es);
			ExplainPropertyInteger("Executor Memory Segments", ns->execmemused.vcnt, es);
			ExplainPropertyLong("Executor Max Memory", (long) kb(ns->execmemused.vmax), es);
			ExplainPropertyInteger("Executor Max Memory Segment", ns->execmemused.imax, es);
		}
	}

	/*
	 * Actual work_mem used and wanted
	 */
	if (es->analyze && es->verbose && ns->workmemused.vcnt > 0)
	{
		if (es->format == EXPLAIN_FORMAT_TEXT)
		{
			appendStringInfoSpaces(es->str, es->indent * 2);
			appendStringInfo(es->str, "work_mem: %ldkB  Segments: %d  Max: %ldkB (segment %d)",
							 (long) kb(ns->workmemused.vsum),
							 ns->workmemused.vcnt,
							 (long) kb(ns->workmemused.vmax),
							 ns->workmemused.imax);

			/*
			 * Total number of segments in which this node reuses cached or
			 * creates workfiles.
			 */
			if (nodeSupportWorkfileCaching(planstate))
				appendStringInfo(es->str, "  Workfile: (%d spilling)",
								 ns->totalWorkfileCreated.vcnt);

			appendStringInfo(es->str, "\n");

			if (ns->workmemwanted.vcnt > 0)
			{
				appendStringInfoSpaces(es->str, es->indent * 2);
				cdbexplain_formatMemory(maxbuf, sizeof(maxbuf), ns->workmemwanted.vmax);
				if (ns->ninst == 1)
				{
					appendStringInfo(es->str,
								 "Work_mem wanted: %s to lessen workfile I/O.",
								 maxbuf);
				}
				else
				{
					cdbexplain_formatMemory(avgbuf, sizeof(avgbuf), cdbexplain_agg_avg(&ns->workmemwanted));
					cdbexplain_formatSeg(segbuf, sizeof(segbuf), ns->workmemwanted.imax, ns->ninst);
					appendStringInfo(es->str,
									 "Work_mem wanted: %s avg, %s max%s"
									 " to lessen workfile I/O affecting %d workers.",
									 avgbuf, maxbuf, segbuf, ns->workmemwanted.vcnt);
				}

				appendStringInfo(es->str, "\n");
			}
		}
		else
		{
			ExplainOpenGroup("work_mem", "work_mem", true, es);
			ExplainPropertyLong("Used", (long) kb(ns->workmemused.vsum), es);
			ExplainPropertyInteger("Segments", ns->workmemused.vcnt, es);
			ExplainPropertyLong("Max Memory", (long) kb(ns->workmemused.vmax), es);
			ExplainPropertyLong("Max Memory Segment", ns->workmemused.imax, es);

			/*
			 * Total number of segments in which this node reuses cached or
			 * creates workfiles.
			 */
			if (nodeSupportWorkfileCaching(planstate))
				ExplainPropertyInteger("Workfile Spilling", ns->totalWorkfileCreated.vcnt, es);

			if (ns->workmemwanted.vcnt > 0)
			{
				ExplainPropertyLong("Max Memory Wanted", (long) kb(ns->workmemwanted.vmax), es);

				if (ns->ninst > 1)
				{
					ExplainPropertyInteger("Max Memory Wanted Segment", ns->workmemwanted.imax, es);
					ExplainPropertyLong("Avg Memory Wanted", (long) kb(cdbexplain_agg_avg(&ns->workmemwanted)), es);
					ExplainPropertyInteger("Segments Affected", ns->ninst, es);
				}
			}

			ExplainCloseGroup("work_mem", "work_mem", true, es);
		}
	}

	if (es->verbose && EXPLAIN_MEMORY_VERBOSITY_SUPPRESS < explain_memory_verbosity)
	{
		/*
		 * Memory account balance without overhead
		 */
		appendStringInfoSpaces(es->str, es->indent * 2);
		cdbexplain_formatMemory(maxbuf, sizeof(maxbuf), ns->peakMemBalance.vmax);
		if (ns->peakMemBalance.vcnt == 1)
		{
			appendStringInfo(es->str,
							 "Memory:  %s.\n",
							 maxbuf);
		}
		else
		{
			cdbexplain_formatSeg(segbuf, sizeof(segbuf), ns->peakMemBalance.imax, ns->ninst);
			cdbexplain_formatMemory(avgbuf, sizeof(avgbuf), cdbexplain_agg_avg(&ns->peakMemBalance));
			appendStringInfo(es->str,
							 "Memory:  %s avg, %s max%s.\n",
							 avgbuf,
							 maxbuf,
							 segbuf);
		}
	}

	/*
	 * Print number of partitioned tables scanned for dynamic scans.
	 */
	if (0 <= ns->totalPartTableScanned.vcnt && (T_DynamicSeqScanState == planstate->type
												|| T_DynamicIndexScanState == planstate->type))
	{
		/*
		 * FIXME: Only displayed in TEXT format
		 * [#159443692]
		 */
		if (es->format == EXPLAIN_FORMAT_TEXT)
		{
			double		nPartTableScanned_avg = cdbexplain_agg_avg(&ns->totalPartTableScanned);

			if (0 == nPartTableScanned_avg)
			{
				if (T_DynamicBitmapHeapScanState == planstate->type)
				{
					int			numTotalLeafParts = cdbexplain_countLeafPartTables(planstate);

					appendStringInfoSpaces(es->str, es->indent * 2);
					appendStringInfo(es->str,
									 "Partitions scanned:  0 (out of %d).\n",
									 numTotalLeafParts);
				}
			}
			else
			{
				cdbexplain_formatSeg(segbuf, sizeof(segbuf), ns->totalPartTableScanned.imax, ns->ninst);
				int			numTotalLeafParts = cdbexplain_countLeafPartTables(planstate);

				appendStringInfoSpaces(es->str, es->indent * 2);

				/* only 1 segment scans partitions */
				if (1 == ns->totalPartTableScanned.vcnt)
				{
					/* rescan */
					if (1 < instr->nloops)
					{
						double		totalPartTableScannedPerRescan = ns->totalPartTableScanned.vmax / instr->nloops;

						appendStringInfo(es->str,
										 "Partitions scanned:  %.0f (out of %d) %s of %ld scans.\n",
										 totalPartTableScannedPerRescan,
										 numTotalLeafParts,
										 segbuf,
										 instr->nloops);
					}
					else
					{
						appendStringInfo(es->str,
										 "Partitions scanned:  %.0f (out of %d) %s.\n",
										 ns->totalPartTableScanned.vmax,
										 numTotalLeafParts,
										 segbuf);
					}
				}
				else
				{
					/* rescan */
					if (1 < instr->nloops)
					{
						double		totalPartTableScannedPerRescan = nPartTableScanned_avg / instr->nloops;
						double		maxPartTableScannedPerRescan = ns->totalPartTableScanned.vmax / instr->nloops;

						appendStringInfo(es->str,
										 "Partitions scanned:  Avg %.1f (out of %d) x %d workers of %ld scans."
										 "  Max %.0f parts%s.\n",
										 totalPartTableScannedPerRescan,
										 numTotalLeafParts,
										 ns->totalPartTableScanned.vcnt,
										 instr->nloops,
										 maxPartTableScannedPerRescan,
										 segbuf
							);
					}
					else
					{
						appendStringInfo(es->str,
										 "Partitions scanned:  Avg %.1f (out of %d) x %d workers."
										 "  Max %.0f parts%s.\n",
										 nPartTableScanned_avg,
										 numTotalLeafParts,
										 ns->totalPartTableScanned.vcnt,
										 ns->totalPartTableScanned.vmax,
										 segbuf);
					}
				}
			}
		}
	}

	bool 			haveExtraText = false;
	StringInfoData	extraData;

	initStringInfo(&extraData);

	for (i = 0; i < ns->ninst; i++)
	{
		CdbExplain_StatInst *nsi = &ns->insts[i];

		if (nsi->bnotes < nsi->enotes)
		{
			if (!haveExtraText)
			{
				ExplainOpenGroup("Extra Text", "Extra Text", false, es);
				ExplainOpenGroup("Segment", NULL, true, es);
				haveExtraText = true;
			}
			
			resetStringInfo(&extraData);

			cdbexplain_formatExtraText(&extraData,
									   0,
									   (ns->ninst == 1) ? -1
									   : ns->segindex0 + i,
									   ctx->extratextbuf.data + nsi->bnotes,
									   nsi->enotes - nsi->bnotes);
			ExplainPropertyStringInfo("Extra Text", es, "%s", extraData.data);
		}
	}

	if (haveExtraText)
	{
		ExplainCloseGroup("Segment", NULL, true, es);
		ExplainCloseGroup("Extra Text", "Extra Text", false, es);
	}
	pfree(extraData.data);

	/*
	 * Dump stats for all workers.
	 */
	if (gp_enable_explain_allstat && ns->segindex0 >= 0 && ns->ninst > 0)
	{
		if (es->format == EXPLAIN_FORMAT_TEXT)
		{
			/*
			 * create a header for all stats: separate each individual stat by an
			 * underscore, separate the grouped stats for each node by a slash
			 */
			appendStringInfoSpaces(es->str, es->indent * 2);
			appendStringInfoString(es->str,
								   "allstat: seg_firststart_total_ntuples");
		}
		else
			ExplainOpenGroup("Allstat", "Allstat", true, es);

		for (i = 0; i < ns->ninst; i++)
		{
			CdbExplain_StatInst *nsi = &ns->insts[i];

			if (INSTR_TIME_IS_ZERO(nsi->firststart))
				continue;

			/* Time from start of query on qDisp to worker's first result row */
			INSTR_TIME_SET_ZERO(timediff);
			INSTR_TIME_ACCUM_DIFF(timediff, nsi->firststart, ctx->querystarttime);

			if (es->format == EXPLAIN_FORMAT_TEXT)
			{
				cdbexplain_formatSeconds(startbuf, sizeof(startbuf),
										 INSTR_TIME_GET_DOUBLE(timediff), true);
				cdbexplain_formatSeconds(totalbuf, sizeof(totalbuf),
										 nsi->total, true);
				appendStringInfo(es->str,
								 "/seg%d_%s_%s_%.0f",
								 ns->segindex0 + i,
								 startbuf,
								 totalbuf,
								 nsi->ntuples);
			}
			else
			{
				cdbexplain_formatSeconds(startbuf, sizeof(startbuf),
										 INSTR_TIME_GET_DOUBLE(timediff), false);
				cdbexplain_formatSeconds(totalbuf, sizeof(totalbuf),
										 nsi->total, false);

				ExplainOpenGroup("Segment", NULL, false, es);
				ExplainPropertyInteger("Segment index", ns->segindex0 + i, es);
				ExplainPropertyText("Time To First Result", startbuf, es);
				ExplainPropertyText("Time To Total Result", totalbuf, es);
				ExplainPropertyFloat("Tuples", nsi->ntuples, 1, es);
				ExplainCloseGroup("Segment", NULL, false, es);
			}
		}

		if (es->format == EXPLAIN_FORMAT_TEXT)
			appendStringInfoString(es->str, "//end\n");
		else
			ExplainCloseGroup("Allstat", "Allstat", true, es);
	}
}								/* cdbexplain_showExecStats */

/*
 *	ExplainPrintExecStatsEnd
 *			External API wrapper for cdbexplain_showExecStatsEnd
 *
 * This is an externally exposed wrapper for cdbexplain_showExecStatsEnd such
 * that extensions, such as auto_explain, can leverage the Greenplum specific
 * parts of the EXPLAIN machinery.
 */
void
ExplainPrintExecStatsEnd(ExplainState *es, QueryDesc *queryDesc)
{
	cdbexplain_showExecStatsEnd(queryDesc->plannedstmt,
								queryDesc->showstatctx,
								queryDesc->estate, es);
}

/*
 * cdbexplain_showExecStatsEnd
 *	  Called by qDisp process to format the overall statistics for a query
 *	  into the caller's buffer.
 *
 * 'ctx' is the CdbExplain_ShowStatCtx object which was created by a call to
 *		cdbexplain_showExecStatsBegin() and contains statistics which have
 *		been accumulated over a series of calls to cdbexplain_showExecStats().
 *		Invalid on return (it is freed).
 *
 * This doesn't free the CdbExplain_ShowStatCtx object or buffers, because
 * they will be free'd shortly by the end of statement anyway.
 */
static void
cdbexplain_showExecStatsEnd(struct PlannedStmt *stmt,
							struct CdbExplain_ShowStatCtx *showstatctx,
							struct EState *estate,
							ExplainState *es)
{
    gpexplain_formatSlicesOutput(showstatctx, estate, es);

	if (!IsResManagerMemoryPolicyNone())
	{
		ExplainOpenGroup("Statement statistics", "Statement statistics", true, es);
		if (es->format == EXPLAIN_FORMAT_TEXT)
			appendStringInfo(es->str, "Memory used:  %ldkB\n", (long) kb(stmt->query_mem));
		else
			ExplainPropertyLong("Memory used", (long) kb(stmt->query_mem), es);

		if (showstatctx->workmemwanted_max > 0)
		{
			long mem_wanted;

			mem_wanted = (long) PolicyAutoStatementMemForNoSpill(stmt,
							(uint64) showstatctx->workmemwanted_max);

			/*
			 * Round up to a kilobyte in case we end up requiring less than
			 * that.
			 */
			if (mem_wanted <= 1024L)
				mem_wanted = 1L;
			else
				mem_wanted = mem_wanted / 1024L;

			if (es->format == EXPLAIN_FORMAT_TEXT)
				appendStringInfo(es->str, "Memory wanted:  %ldkB\n", mem_wanted);
			else
				ExplainPropertyLong("Memory wanted", mem_wanted, es);
		}

		ExplainCloseGroup("Statement statistics", "Statement statistics", true, es);
	}
}								/* cdbexplain_showExecStatsEnd */

/*
 * Given a statistics context search for all the slice statistics
 * and format them to the correct layout
 */
static void
gpexplain_formatSlicesOutput(struct CdbExplain_ShowStatCtx *showstatctx,
                             struct EState *estate,
                             ExplainState *es)
{
	Slice	   *slice;
	int			sliceIndex;
	int			flag;
	double		total_memory_across_slices = 0;

	char		avgbuf[50];
	char		maxbuf[50];
	char		segbuf[50];

    if (showstatctx->nslice > 0)
        ExplainOpenGroup("Slice statistics", "Slice statistics", false, es);

    for (sliceIndex = 0; sliceIndex < showstatctx->nslice; sliceIndex++)
    {
        CdbExplain_SliceSummary *ss = &showstatctx->slices[sliceIndex];
        CdbExplain_DispatchSummary *ds = &ss->dispatchSummary;
        
        flag = es->str->len;
        if (es->format == EXPLAIN_FORMAT_TEXT)
        {

            appendStringInfo(es->str, "  (slice%d) ", sliceIndex);
            if (sliceIndex < 10)
                appendStringInfoChar(es->str, ' ');

            appendStringInfoString(es->str, "  ");
        }
        else 
        {
            ExplainOpenGroup("Slice", NULL, true, es);
            ExplainPropertyInteger("Slice", sliceIndex, es);
        }

        /* Worker counts */
        slice = getCurrentSlice(estate, sliceIndex);
        if (slice &&
            slice->gangSize > 0 &&
            slice->gangSize != ss->dispatchSummary.nOk)
        {
            int nNotDispatched = slice->gangSize - ds->nResult + ds->nNotDispatched;

            es->str->data[flag] = (ss->dispatchSummary.nError > 0) ? 'X' : '_';
            StringInfoData workersInformationText;
            initStringInfo(&workersInformationText);

            appendStringInfo(&workersInformationText, "Workers:");

            if (es->format == EXPLAIN_FORMAT_TEXT)
            {
                if (ds->nError == 1)
                {
                    appendStringInfo(&workersInformationText,
                                     " %d error;",
                                     ds->nError);
                }
                else if (ds->nError > 1)
                {
                    appendStringInfo(&workersInformationText,
                                     " %d errors;",
                                     ds->nError);
                }
            }
            else
            {
                ExplainOpenGroup("Workers", "Workers", true, es);
                if (ds->nError > 0)
                    ExplainPropertyInteger("Errors", ds->nError, es);
            }

            if (ds->nCanceled > 0)
            {
                if (es->format == EXPLAIN_FORMAT_TEXT)
                {
                    appendStringInfo(&workersInformationText,
                                     " %d canceled;",
                                     ds->nCanceled);
                }
                else
                {
                    ExplainPropertyInteger("Canceled", ds->nCanceled, es);
                }
            }

            if (nNotDispatched > 0)
            {
                if (es->format == EXPLAIN_FORMAT_TEXT)
                {
                    appendStringInfo(&workersInformationText,
                                     " %d not dispatched;",
                                     nNotDispatched);
                }
                else
                {
                    ExplainPropertyInteger("Not Dispatched", nNotDispatched, es);
                }
            }

            if (ds->nIgnorableError > 0)
            {
                if (es->format == EXPLAIN_FORMAT_TEXT)
                {
                    appendStringInfo(&workersInformationText,
                                     " %d aborted;",
                                     ds->nIgnorableError);
                }
                else
                {
                    ExplainPropertyInteger("Aborted", ds->nIgnorableError, es);
                }
            }

            if (ds->nOk > 0)
            {
                if (es->format == EXPLAIN_FORMAT_TEXT)
                {
                    appendStringInfo(&workersInformationText,
                                     " %d ok;",
                                     ds->nOk);
                }
                else
                {
                    ExplainPropertyInteger("Ok", ds->nOk, es);
                }
            }

            if (es->format == EXPLAIN_FORMAT_TEXT)
            {
                workersInformationText.len--;
                ExplainPropertyStringInfo("Workers", es, "%s.  ", workersInformationText.data);
            }
            else
            {
                ExplainCloseGroup("Workers", "Workers", true, es);
            }
        }

        /* Executor memory high-water mark */
        cdbexplain_formatMemory(maxbuf, sizeof(maxbuf), ss->peakmemused.vmax);
        if (ss->peakmemused.vcnt == 1)
        {
            if (es->format == EXPLAIN_FORMAT_TEXT)
            {
                const char *seg = segbuf;

                if (ss->peakmemused.imax >= 0)
                {
                    cdbexplain_formatSeg(segbuf, sizeof(segbuf), ss->peakmemused.imax, 999);
                }
                else if (slice &&
                         slice->gangSize > 0)
                {
                    seg = " (entry db)";
                }
                else
                {
                    seg = "";
                }
                appendStringInfo(es->str,
                                 "Executor memory: %s%s.",
                                 maxbuf,
                                 seg);
            }
            else
            {
                ExplainPropertyInteger("Executor Memory", ss->peakmemused.vmax, es);
            }
        }
        else if (ss->peakmemused.vcnt > 1)
        {
            if (es->format == EXPLAIN_FORMAT_TEXT)
            {
                cdbexplain_formatMemory(avgbuf, sizeof(avgbuf), cdbexplain_agg_avg(&ss->peakmemused));
                cdbexplain_formatSeg(segbuf, sizeof(segbuf), ss->peakmemused.imax, ss->nworker);
                appendStringInfo(es->str,
                                 "Executor memory: %s avg x %d workers, %s max%s.",
                                 avgbuf,
                                 ss->peakmemused.vcnt,
                                 maxbuf,
                                 segbuf);
            }
            else
            {
                ExplainOpenGroup("Executor Memory", "Executor Memory", true, es);
                ExplainPropertyInteger("Average", cdbexplain_agg_avg(&ss->peakmemused), es);
                ExplainPropertyInteger("Workers", ss->peakmemused.vcnt, es);
                ExplainPropertyInteger("Maximum Memory Used", ss->peakmemused.vmax, es);
                ExplainCloseGroup("Executor Memory", "Executor Memory", true, es);
            }
        }

        if (EXPLAIN_MEMORY_VERBOSITY_SUPPRESS < explain_memory_verbosity)
        {
            /* Memory accounting global peak memory usage */
            double peakMemoryUsage = ss->memory_accounting_global_peak.vmax;
            int workers = 1;
            cdbexplain_formatMemory(maxbuf, sizeof(maxbuf), peakMemoryUsage);
			peakMemoryUsage = kb(peakMemoryUsage);
			
            if (ss->memory_accounting_global_peak.vcnt == 1)
            {
                
                if (es->format == EXPLAIN_FORMAT_TEXT)
                {
                    const char *seg = segbuf;

                    if (ss->memory_accounting_global_peak.imax >= 0)
                    {
                        cdbexplain_formatSeg(segbuf, sizeof(segbuf), ss->memory_accounting_global_peak.imax, 999);
                    }
                    else if (slice &&
                             slice->gangSize > 0)
                    {
                        seg = " (entry db)";
                    }
                    else
                    {
                        seg = "";
                    }
                    appendStringInfo(es->str,
                                     "  Peak memory: %s%s.",
                                     maxbuf,
                                     seg);
                } 
                else
                {
                    ExplainPropertyInteger("Global Peak Memory", ss->memory_accounting_global_peak.vmax, es);
                }
            }
            else if (ss->memory_accounting_global_peak.vcnt > 1)
            {
				if (es->format == EXPLAIN_FORMAT_TEXT)
                {
                	peakMemoryUsage = cdbexplain_agg_avg(&ss->memory_accounting_global_peak);
                	workers = ss->memory_accounting_global_peak.vcnt;
                	peakMemoryUsage = kb(peakMemoryUsage);
                	cdbexplain_formatMemory(avgbuf, sizeof(avgbuf), peakMemoryUsage);
                	cdbexplain_formatSeg(segbuf, sizeof(segbuf), ss->memory_accounting_global_peak.imax, ss->nworker);
                	appendStringInfo(es->str,
                	                 "  Peak memory: %s avg x %d workers, %s max%s.",
                	                 avgbuf,
                	                 ss->memory_accounting_global_peak.vcnt,
                	                 maxbuf,
                	                 segbuf);
				}
				else
                {
                    ExplainOpenGroup("Global Peak Memory", "Global Peak Memory", true, es);
                    ExplainPropertyInteger("Average", cdbexplain_agg_avg(&ss->memory_accounting_global_peak), es);
                    ExplainPropertyInteger("Workers", ss->memory_accounting_global_peak.vcnt, es);
                    ExplainPropertyInteger("Maximum Memory Used", ss->memory_accounting_global_peak.vmax, es);
                    ExplainCloseGroup("Global Peak Memory", "Global Peak Memory", true, es);
                }
            }

            total_memory_across_slices += (peakMemoryUsage * workers);

            /* Vmem reserved by QEs */
            cdbexplain_formatMemory(maxbuf, sizeof(maxbuf), ss->vmem_reserved.vmax);
            if (ss->vmem_reserved.vcnt == 1)
            {

                if (es->format == EXPLAIN_FORMAT_TEXT)
                {
                    const char *seg = segbuf;

                    if (ss->vmem_reserved.imax >= 0)
                    {
                        cdbexplain_formatSeg(segbuf, sizeof(segbuf), ss->vmem_reserved.imax, 999);
                    }
                    else if (slice &&
                             slice->gangSize > 0)
                    {
                        seg = " (entry db)";
                    }
                    else
                    {
                        seg = "";
                    }
                    appendStringInfo(es->str,
                                     "  Vmem reserved: %s%s.",
                                     maxbuf,
                                     seg);
                }
                else
                {
                    ExplainPropertyInteger("Virtual Memory", ss->vmem_reserved.vmax, es);
                }
            }
            else if (ss->vmem_reserved.vcnt > 1)
            {
                if (es->format == EXPLAIN_FORMAT_TEXT)
                {
                    cdbexplain_formatMemory(avgbuf, sizeof(avgbuf), cdbexplain_agg_avg(&ss->vmem_reserved));
                    cdbexplain_formatSeg(segbuf, sizeof(segbuf), ss->vmem_reserved.imax, ss->nworker);
                    appendStringInfo(es->str,
                                     "  Vmem reserved: %s avg x %d workers, %s max%s.",
                                     avgbuf,
                                     ss->vmem_reserved.vcnt,
                                     maxbuf,
                                     segbuf);
                }
                else
                {
                    ExplainOpenGroup("Virtual Memory", "Virtual Memory", true, es);
                    ExplainPropertyInteger("Average", cdbexplain_agg_avg(&ss->vmem_reserved), es);
                    ExplainPropertyInteger("Workers", ss->vmem_reserved.vcnt, es);
                    ExplainPropertyInteger("Maximum Memory Used", ss->vmem_reserved.vmax, es);
                    ExplainCloseGroup("Virtual Memory", "Virtual Memory", true, es);
                }

            }
        }

        /* Work_mem used/wanted (max over all nodes and workers of slice) */
        if (ss->workmemused_max + ss->workmemwanted_max > 0)
        {
            if (es->format == EXPLAIN_FORMAT_TEXT)
            {
                cdbexplain_formatMemory(maxbuf, sizeof(maxbuf), ss->workmemused_max);
                appendStringInfo(es->str, "  Work_mem: %s max", maxbuf);
                if (ss->workmemwanted_max > 0)
                {
                    es->str->data[flag] = '*';	/* draw attention to this slice */
                    cdbexplain_formatMemory(maxbuf, sizeof(maxbuf), ss->workmemwanted_max);
                    appendStringInfo(es->str, ", %s wanted", maxbuf);
                }
                appendStringInfoChar(es->str, '.');
            }
            else
            {
                ExplainPropertyInteger("Work Maximum Memory", ss->workmemused_max, es);
            }
        }

        if (es->format == EXPLAIN_FORMAT_TEXT)
            appendStringInfoChar(es->str, '\n');

        ExplainCloseGroup("Slice", NULL, true, es);
    }

    if (showstatctx->nslice > 0)
        ExplainCloseGroup("Slice statistics", "Slice statistics", false, es);

    if (total_memory_across_slices > 0)
    {
        if (es->format == EXPLAIN_FORMAT_TEXT)
        {
            appendStringInfo(es->str, "Total memory used across slices: %.0fK bytes \n", total_memory_across_slices);
        }
        else
        {
            ExplainPropertyInteger("Total memory used across slices", total_memory_across_slices, es);
        }
    }
}

static int
cdbexplain_countLeafPartTables(PlanState *planstate)
{
	Assert(IsA(planstate, DynamicSeqScanState) ||
		   IsA(planstate, DynamicIndexScanState));
	Scan	   *scan = (Scan *) planstate->plan;

	Oid			root_oid = getrelid(scan->scanrelid, planstate->state->es_range_table);

	return countLeafPartTables(root_oid);
}

/*
 * Show the hash and merge keys for a Motion node.
 */
static void
show_motion_keys(PlanState *planstate, List *hashExpr, int nkeys, AttrNumber *keycols,
			     const char *qlabel, List *ancestors, ExplainState *es)
{
	Plan	   *plan = planstate->plan;
	List	   *context;
	char	   *exprstr;
	bool		useprefix = list_length(es->rtable) > 1;
	int			keyno;
	List	   *result = NIL;

	if (!nkeys && !hashExpr)
		return;

	/* Set up deparse context */
	context = set_deparse_context_planstate(es->deparse_cxt,
											(Node *) planstate,
											ancestors);

    /* Merge Receive ordering key */
    for (keyno = 0; keyno < nkeys; keyno++)
    {
	    /* find key expression in tlist */
	    AttrNumber	keyresno = keycols[keyno];
	    TargetEntry *target = get_tle_by_resno(plan->targetlist, keyresno);

	    /* Deparse the expression, showing any top-level cast */
	    if (target)
	        exprstr = deparse_expression((Node *) target->expr, context,
								         useprefix, true);
        else
        {
            elog(WARNING, "Gather Motion %s error: no tlist item %d",
                 qlabel, keyresno);
            exprstr = "*BOGUS*";
        }

		result = lappend(result, exprstr);
    }

	if (list_length(result) > 0)
		ExplainPropertyList(qlabel, result, es);

    /* Hashed repartitioning key */
    if (hashExpr)
    {
	    /* Deparse the expression */
	    exprstr = deparse_expression((Node *)hashExpr, context, useprefix, true);
		ExplainPropertyText("Hash Key", exprstr, es);
    }
}

/*
 * Explain a partition selector node, including partition elimination
 * expression and number of statically selected partitions, if available.
 */
static void
explain_partition_selector(PartitionSelector *ps, PlanState *parentstate,
						   List *ancestors, ExplainState *es)
{
	if (ps->printablePredicate)
	{
		List	   *context;
		bool		useprefix;
		char	   *exprstr;

		/* Set up deparsing context */
		context = set_deparse_context_planstate(es->deparse_cxt,
												(Node *) parentstate,
												ancestors);
		useprefix = list_length(es->rtable) > 1;

		/* Deparse the expression */
		exprstr = deparse_expression(ps->printablePredicate, context, useprefix, false);

		ExplainPropertyText("Filter", exprstr, es);
	}

	if (ps->staticSelection)
	{
		int nPartsSelected = list_length(ps->staticPartOids);
		int nPartsTotal = countLeafPartTables(ps->relid);

		ExplainPropertyStringInfo("Partitions selected", es, "%d (out of %d)", nPartsSelected, nPartsTotal);
	}
}

/*
 * Explain a parallel retrieve cursor,
 * indicate the endpoints exist on entry DB, or on some segments,
 * or on all segments.
 */
void ExplainParallelRetrieveCursor(ExplainState *es, QueryDesc* queryDesc)
{
	PlannedStmt *plan = queryDesc->plannedstmt;
	SliceTable *sliceTable = queryDesc->estate->es_sliceTable;
	StringInfoData            endpointInfoStr;
	enum EndPointExecPosition endPointExecPosition;

	initStringInfo(&endpointInfoStr);

	endPointExecPosition = GetParallelCursorEndpointPosition(plan);
	ExplainOpenGroup("Cursor", "Cursor", true, es);
	switch(endPointExecPosition)
	{
		case ENDPOINT_ON_ENTRY_DB:
		{
			appendStringInfo(&endpointInfoStr, "\"on coordinator\"");
			break;
		}
		case ENDPOINT_ON_SINGLE_QE:
		{
			appendStringInfo(
							 &endpointInfoStr, "\"on segment: contentid [%d]\"",
							 gp_session_id % plan->planTree->flow->numsegments);
			break;
		}
		case ENDPOINT_ON_SOME_QE:
		{
			ListCell * cell;
			bool isFirst = true;
			appendStringInfo(&endpointInfoStr, "on segments: contentid [");

			Slice *slice = (Slice *) list_nth(sliceTable->slices, 0);
			foreach(cell, slice->segments)
			{
				int contentid = lfirst_int(cell);
				appendStringInfo(&endpointInfoStr, (isFirst)?"%d":", %d", contentid);
				isFirst = false;
			}
			appendStringInfo(&endpointInfoStr, "]");
			break;
		}
		case ENDPOINT_ON_ALL_QE:
		{
			appendStringInfo(&endpointInfoStr, "on all %d segments", getgpsegmentCount());
			break;
		}
		default:
		{
			elog(ERROR, "invalid endpoint position : %d", endPointExecPosition);
			break;
		}
	}
	ExplainPropertyText("Endpoint", endpointInfoStr.data, es);
	ExplainCloseGroup("Cursor", "Cursor", true, es);
}
