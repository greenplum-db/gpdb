/*-------------------------------------------------------------------------
 *
 * gdddetector_test.c
 *	  Unit tests for GDD.
 *
 * NOTES
 *    Tests in this file may serve two purposes:
 *    1) Unit-test individual internal functions in GDD.
 *    2) Construct artificial wait graphs and test if the GDD functions
 *       behave as expected. Comparing to those tests in isolation2/gdd,
 *       tests presented here can cover large-scale scenarios and has
 *       finer-grained control over how GDD executes. For example, here
 *       we can simulate a scenario with hundreds of transactions running
 *       on hundreds of segments. It would be quite challenging to do so
 *       with isolation2 test framework.
 *       As another example, we can force the reduce procedure to start
 *       from a particular vertex and check if GDD ends up with the right
 *       conclusion. Again, this is not easy to do with isolation2 test
 *       framework, as the order of vertices stored in GddCtx is determined
 *       by the order of tuples returned from gp_dist_wait_status(), which
 *       is non-deterministic.
 *
 *-------------------------------------------------------------------------
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "cmockery.h"

#include "postgres.h"
#include "storage/lmgr.h"
#include "storage/lock.h"
#include "utils/memutils.h"

/* Actual function body */
#include "../gdddetector.c"

/*
 * This struct defines a wait relation between two transactions.
 * It represents all the information related to a GddEdge.
 *
 * One can specify an arbitrary wait graph with an array of
 * TestWaitRelations, and call loadTestWaitRelations() to initialize
 * a GddCtx. This way, we can exercise GDD functions without running a
 * real GPDB cluster.
 */
typedef struct TestWaitRelation
{
	int 		seg_id;
	DistributedTransactionId	waiter_xid;
	DistributedTransactionId	holder_xid;
	bool		solid_edge;
	int 		waiter_pid;
	int 		holder_pid;
	LOCKMETHODID	lock_methodid;
	LOCKMODE 	lock_mode;
	LockTagType	lock_tagtype;
	int 		waiter_sessionid;
	int 		holder_sessionid;
} TestWaitRelation;

/*
 * Data structures and variables copied from gddbackend.c.
 */
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

static MemoryContext gddContext;
static MemoryContext oldContext;

/*
 * Initialize a GddCtx with the given wait relations.
 */
static void
loadTestWaitRelations(GddCtx *ctx, TestWaitRelation *wait_relations, int num)
{
	Assert(ctx);
	Assert(wait_relations);

	int i;

	/*
	 * Add each edge into GddCtx.
	 * The code below mimics buildWaitGraph() and gp_dist_wait_status().
	 */
	for (i = 0; i < num; i++)
	{
		DistributedTransactionId  waiter_xid;
		DistributedTransactionId  holder_xid;
		bool		   solidedge;
		int			   segid;
		GddEdge       *edge;
		VertSatelliteData *waiter_data = NULL;
		VertSatelliteData *holder_data = NULL;
		EdgeSatelliteData *edge_data = NULL;

		waiter_data = (VertSatelliteData *) palloc(sizeof(VertSatelliteData));
		holder_data = (VertSatelliteData *) palloc(sizeof(VertSatelliteData));
		edge_data = (EdgeSatelliteData *) palloc(sizeof(EdgeSatelliteData));

		segid = wait_relations[i].seg_id;

		waiter_xid = wait_relations[i].waiter_xid;

		holder_xid = wait_relations[i].holder_xid;

		solidedge = wait_relations[i].solid_edge;

		waiter_data->pid = wait_relations[i].waiter_pid;

		holder_data->pid = wait_relations[i].holder_pid;

		edge_data->lockmode = pstrdup(GetLockmodeName(wait_relations[i].lock_methodid, wait_relations[i].lock_mode));

		edge_data->locktype = pstrdup(GetLockNameFromTagType(wait_relations[i].lock_tagtype));

		waiter_data->sessionid = wait_relations[i].waiter_sessionid;

		holder_data->sessionid = wait_relations[i].holder_sessionid;

		edge = GddCtxAddEdge(ctx, segid, waiter_xid, holder_xid, solidedge);
		edge->data = (void *) edge_data;
		edge->from->data = (void *) waiter_data;
		edge->to->data = (void *) holder_data;
	}
}

/*
 * Test case #1: test_reduce_simple_graph_no_deadlock
 *
 * - 1 segment
 * - 4 transactions (gxids are 10, 20, 30, and 40)
 * - Wait relations:
 *   - solid edge from txn 20 to txn 10
 *   - solid edge from txn 30 to txn 20
 *   - solid edge from txn 40 to txn 20
 */
static void
test_reduce_simple_graph_no_deadlock(void **state)
{
	TestWaitRelation wait_relations[3] = {
		/* solid edge from txn 20 to txn 10 */
		{
			.seg_id = 0,
			.waiter_xid = 20,
			.holder_xid = 10,
			.solid_edge = true,
			.waiter_pid = 200,
			.holder_pid = 100,
			.lock_methodid = DEFAULT_LOCKMETHOD,
			.lock_mode = AccessExclusiveLock,
			.lock_tagtype = LOCKTAG_RELATION,
			.waiter_sessionid = 3000,
			.holder_sessionid = 1000
		},
		/* solid edge from txn 30 to txn 20 */
		{
			.seg_id = 0,
			.waiter_xid = 30,
			.holder_xid = 20,
			.solid_edge = true,
			.waiter_pid = 300,
			.holder_pid = 200,
			.lock_methodid = DEFAULT_LOCKMETHOD,
			.lock_mode = AccessExclusiveLock,
			.lock_tagtype = LOCKTAG_RELATION,
			.waiter_sessionid = 3000,
			.holder_sessionid = 2000
		},
		/* solid edge from txn 40 to txn 20 */
		{
			.seg_id = 0,
			.waiter_xid = 40,
			.holder_xid = 20,
			.solid_edge = true,
			.waiter_pid = 400,
			.holder_pid = 200,
			.lock_methodid = DEFAULT_LOCKMETHOD,
			.lock_mode = AccessExclusiveLock,
			.lock_tagtype = LOCKTAG_RELATION,
			.waiter_sessionid = 4000,
			.holder_sessionid = 2000
		}
	};

	GddCtx *ctx;

	oldContext = MemoryContextSwitchTo(gddContext);

	ctx = GddCtxNew();

	loadTestWaitRelations(ctx, wait_relations, 3);

	GddCtxReduce(ctx);

	/* There should be no deadlock and hence no vertex left. */
	bool is_empty = GddCtxEmpty(ctx);

	MemoryContextSwitchTo(oldContext);
	MemoryContextReset(gddContext);

	assert_true(is_empty);
}

/*
 * Test case #2: test_reduce_large_graph_pair_deadlocks
 *
 * - 100 segments
 * - 200 transactions (gxids are from 1 to 200)
 * - Wait relations:
 *   - The i-th transaction and (i+100)-th transaction form a pair,
 *     and they wait for each other with solid edges on segments i-1
 *     and i. E.g., txn 1 waits for txn 101 on segment 0, and txn 101
 *     waits for txn 1 on segment 1, both with solid edges.
 *   - In total, there are 200 solid edges and 100 cycles (i.e., deadlocks).
 */
static void
test_reduce_large_graph_pair_deadlocks(void **state)
{
	const int num_transactions = 200;
	TestWaitRelation *wait_relations = palloc(num_transactions * sizeof(TestWaitRelation));
	int i;

	/*
	 * Build the wait relations programmatically.
	 *
	 * txn 1 waits for txn 101 on segment 0; txn 101 waits for txn 2 on segment 1;
	 * txn 2 waits for txn 101 on segment 2; txn 101 waits for txn 2 on segment 3;
	 * ...
	 * txn 100 waits for txn 200 on segment 98; txn 200 waits for txn 100 on segment 99.
	 *
	 * All edges are solid.
	 */
	for (i = 0; i < num_transactions/2; i ++)
	{
		TestWaitRelation *r1 = &wait_relations[i*2];
		TestWaitRelation *r2 = &wait_relations[i*2+1];

		DistributedTransactionId gxid1 = i + 1; /* Valid gxids start with 1. */
		DistributedTransactionId gxid2 = gxid1 + 100;
		int segment1 = i; /* segment id starts with 0. */
		int segment2 = i+1;

		/* Make up pids and session ids. */
		int pid1_segment1 = segment1 * num_transactions + gxid1;
		int pid2_segment1 = segment1 * num_transactions + gxid2;
		int pid1_segment2 = segment1 * num_transactions + gxid1;
		int pid2_segment2 = segment1 * num_transactions + gxid2;
		int sessionid1 = gxid1 * 10;
		int sessionid2 = gxid2 * 10;

		/* r1: gxid1 waits for gxid2 on segment1 with solid edge. */
		r1->seg_id = segment1;
		r1->waiter_xid = gxid1;
		r1->holder_xid = gxid2;
		r1->solid_edge = true;
		r1->waiter_pid = pid1_segment1;
		r1->holder_pid = pid2_segment1;
		r1->lock_methodid = DEFAULT_LOCKMETHOD;
		r1->lock_mode = AccessExclusiveLock;
		r1->lock_tagtype = LOCKTAG_RELATION;
		r1->waiter_sessionid = sessionid1;
		r1->holder_sessionid = sessionid2;

		/* r2: gxid2 waits for gxid1 on segment2 with solid edge. */
		r2->seg_id = segment2;
		r2->waiter_xid = gxid2;
		r2->holder_xid = gxid1;
		r2->solid_edge = true;
		r2->waiter_pid = pid2_segment2;
		r2->holder_pid = pid1_segment2;
		r2->lock_methodid = DEFAULT_LOCKMETHOD;
		r2->lock_mode = AccessExclusiveLock;
		r2->lock_tagtype = LOCKTAG_RELATION;
		r2->waiter_sessionid = sessionid2;
		r2->holder_sessionid = sessionid1;
	}

	GddCtx *ctx;

	oldContext = MemoryContextSwitchTo(gddContext);

	ctx = GddCtxNew();

	loadTestWaitRelations(ctx, wait_relations, num_transactions);

	GddCtxReduce(ctx);

	int indeg_count = ctx->topstat.indeg;
	int outdeg_count = ctx->topstat.outdeg;

	MemoryContextSwitchTo(oldContext);
	MemoryContextReset(gddContext);
	pfree(wait_relations);

	assert_int_equal(indeg_count, num_transactions);
	assert_int_equal(outdeg_count, num_transactions);
}

/*
 * Test case #3: test_reduce_large_graph_no_deadlock1
 *
 * - 100 segments
 * - 200 transactions (gxids are from 1 to 200)
 * - Wait relations:
 *   - The i-th transaction and (i+100)-th transaction form a pair,
 *     and they wait for each other with solid edges on segments i-1
 *     and i. E.g., txn 1 waits for txn 101 on segment 0, and txn 101
 *     waits for txn 1 on segment 1, both with dotted edges.
 *   - In total, there are 200 dotted edges and no deadlock.
 */
static void
test_reduce_large_graph_no_deadlock1(void **state)
{
	const int num_transactions = 200;
	TestWaitRelation *wait_relations = palloc(num_transactions * sizeof(TestWaitRelation));
	int i;

	/*
	 * Build the wait relations programmatically.
	 *
	 * txn 1 waits for txn 101 on segment 0; txn 101 waits for txn 2 on segment 1;
	 * txn 2 waits for txn 101 on segment 2; txn 101 waits for txn 2 on segment 3;
	 * ...
	 * txn 100 waits for txn 200 on segment 98; txn 200 waits for txn 100 on segment 99.
	 *
	 * All edges are dotted.
	 */
	for (i = 0; i < num_transactions/2; i ++)
	{
		TestWaitRelation *r1 = &wait_relations[i*2];
		TestWaitRelation *r2 = &wait_relations[i*2+1];

		DistributedTransactionId gxid1 = i + 1; /* Valid gxids start with 1. */
		DistributedTransactionId gxid2 = gxid1 + 100;
		int segment1 = i; /* segment id starts with 0. */
		int segment2 = i+1;

		/* Make up pids and session ids. */
		int pid1_segment1 = segment1 * num_transactions + gxid1;
		int pid2_segment1 = segment1 * num_transactions + gxid2;
		int pid1_segment2 = segment1 * num_transactions + gxid1;
		int pid2_segment2 = segment1 * num_transactions + gxid2;
		int sessionid1 = gxid1 * 10;
		int sessionid2 = gxid2 * 10;

		/* r1: gxid1 waits for gxid2 on segment1 with dotted edge. */
		r1->seg_id = segment1;
		r1->waiter_xid = gxid1;
		r1->holder_xid = gxid2;
		r1->solid_edge = false;
		r1->waiter_pid = pid1_segment1;
		r1->holder_pid = pid2_segment1;
		r1->lock_methodid = DEFAULT_LOCKMETHOD;
		r1->lock_mode = AccessExclusiveLock;
		r1->lock_tagtype = LOCKTAG_TUPLE;
		r1->waiter_sessionid = sessionid1;
		r1->holder_sessionid = sessionid2;

		/* r2: gxid2 waits for gxid1 on segment2 with dotted edge. */
		r2->seg_id = segment2;
		r2->waiter_xid = gxid2;
		r2->holder_xid = gxid1;
		r2->solid_edge = false;
		r2->waiter_pid = pid2_segment2;
		r2->holder_pid = pid1_segment2;
		r2->lock_methodid = DEFAULT_LOCKMETHOD;
		r2->lock_mode = AccessExclusiveLock;
		r2->lock_tagtype = LOCKTAG_TUPLE;
		r2->waiter_sessionid = sessionid2;
		r2->holder_sessionid = sessionid1;
	}

	GddCtx *ctx;

	oldContext = MemoryContextSwitchTo(gddContext);

	ctx = GddCtxNew();

	loadTestWaitRelations(ctx, wait_relations, num_transactions);

	GddCtxReduce(ctx);

	/* There should be no deadlock and hence no vertex left. */
	bool is_empty = GddCtxEmpty(ctx);

	MemoryContextSwitchTo(oldContext);
	MemoryContextReset(gddContext);

	assert_true(is_empty);
}

/*
 * Test case #4: test_reduce_large_graph_single_deadlock
 *
 * - 101 segments
 * - 100 transactions (gxids are from 1 to 100)
 * - Wait relations:
 *   - The i-th transaction waits for the (i+1)-th transaction on segments i-1
 *     and i. E.g., txn 1 waits for txn 2 on segment 0, and txn 2
 *     waits for txn 3 on segment 1, ..., and txn 100 waits for txn 1 on
 *     segment 100, all on solid edges.
 *   - In total, there are 101 solid edges and a single cycle with all txns in it.
 */
static void
test_reduce_large_graph_single_deadlock(void **state)
{
	const int num_transactions = 100;
	TestWaitRelation *wait_relations = palloc((num_transactions + 1) * sizeof(TestWaitRelation));
	int i;

	/*
	 * Build the wait relations programmatically.
	 *
	 * txn 1 waits for txn 2 on segment 0;
	 * txn 2 waits for txn 3 on segment 1;
	 * ...
	 * txn 100 waits for txn 1 on segment 100.
	 *
	 * All edges are solid.
	 */
	for (i = 0; i <= num_transactions; i ++)
	{
		TestWaitRelation *r = &wait_relations[i];

		DistributedTransactionId gxid1 = i + 1; /* Valid gxids start with 1. */
		DistributedTransactionId gxid2 = (gxid1 < num_transactions) ? gxid1 + 1 : 1;
		int segment = i; /* segment id starts with 0. */

		/* Make up pids and session ids. */
		int pid1 = segment * num_transactions + gxid1;
		int pid2 = segment * num_transactions + gxid2;
		int sessionid1 = gxid1 * 10;
		int sessionid2 = gxid2 * 10;

		/* r: gxid1 waits for gxid2 on segment with solid edge. */
		r->seg_id = segment;
		r->waiter_xid = gxid1;
		r->holder_xid = gxid2;
		r->solid_edge = true;
		r->waiter_pid = pid1;
		r->holder_pid = pid2;
		r->lock_methodid = DEFAULT_LOCKMETHOD;
		r->lock_mode = AccessExclusiveLock;
		r->lock_tagtype = LOCKTAG_RELATION;
		r->waiter_sessionid = sessionid1;
		r->holder_sessionid = sessionid2;
	}

	GddCtx *ctx;

	oldContext = MemoryContextSwitchTo(gddContext);

	ctx = GddCtxNew();

	loadTestWaitRelations(ctx, wait_relations, num_transactions+1);

	GddCtxReduce(ctx);

	int indeg_count = ctx->topstat.indeg;
	int outdeg_count = ctx->topstat.outdeg;

	MemoryContextSwitchTo(oldContext);
	MemoryContextReset(gddContext);
	pfree(wait_relations);

	assert_int_equal(indeg_count, num_transactions);
	assert_int_equal(outdeg_count, num_transactions);
}

/*
 * Test case #5: test_reduce_large_graph_no_deadlock2
 *
 * - 101 segments
 * - 100 transactions (gxids are from 1 to 100)
 * - Wait relations:
 *   - The i-th transaction waits for the (i+1)-th transaction on segments i-1
 *     and i. E.g., txn 1 waits for txn 2 on segment 0, and txn 2
 *     waits for txn 3 on segment 1, ..., and txn 99 waits for txn 100 on
 *     segment 99, all on solid edges.
 *   - txn 100 waits for txn 1 on segment 100, with a dotted edge.
 *   - In total, there are 100 solid edges plus 1 dotted edge, and no deadlock.
 */
static void
test_reduce_large_graph_no_deadlock2(void **state)
{
	const int num_transactions = 100;
	TestWaitRelation *wait_relations = palloc((num_transactions + 1) * sizeof(TestWaitRelation));
	int i;

	/*
	 * Build the wait relations programmatically.
	 *
	 * txn 1 waits for txn 2 on segment 0;
	 * txn 2 waits for txn 3 on segment 1;
	 * ...
	 * txn 99 waits for txn 100 on segment 99;
	 * txn 100 waits for txn 1 on segment 100.
	 *
	 * All but the last edge are solid.
	 */
	for (i = 0; i <= num_transactions; i ++)
	{
		TestWaitRelation *r = &wait_relations[i];

		DistributedTransactionId gxid1 = i + 1; /* Valid gxids start with 1. */
		DistributedTransactionId gxid2 = (gxid1 < num_transactions) ? gxid1 + 1 : 1;
		int segment = i; /* segment id starts with 0. */

		/* Make up pids and session ids. */
		int pid1 = segment * num_transactions + gxid1;
		int pid2 = segment * num_transactions + gxid2;
		int sessionid1 = gxid1 * 10;
		int sessionid2 = gxid2 * 10;

		/* r: gxid1 waits for gxid2 on segment with solid edge, except the last one. */
		r->seg_id = segment;
		r->waiter_xid = gxid1;
		r->holder_xid = gxid2;
		r->solid_edge = (gxid1 < num_transactions) ? true : false;
		r->waiter_pid = pid1;
		r->holder_pid = pid2;
		r->lock_methodid = DEFAULT_LOCKMETHOD;
		r->lock_mode = AccessExclusiveLock;
		r->lock_tagtype = (gxid1 < num_transactions) ? LOCKTAG_RELATION : LOCKTAG_TUPLE;
		r->waiter_sessionid = sessionid1;
		r->holder_sessionid = sessionid2;
	}

	GddCtx *ctx;

	oldContext = MemoryContextSwitchTo(gddContext);

	ctx = GddCtxNew();

	loadTestWaitRelations(ctx, wait_relations, num_transactions+1);

	GddCtxReduce(ctx);

	/* There should be no deadlock and hence no vertex left. */
	bool is_empty = GddCtxEmpty(ctx);

	MemoryContextSwitchTo(oldContext);
	MemoryContextReset(gddContext);
	pfree(wait_relations);

	assert_true(is_empty);
}

int
main(int argc, char *argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
		unit_test(test_reduce_simple_graph_no_deadlock),
		unit_test(test_reduce_large_graph_pair_deadlocks),
		unit_test(test_reduce_large_graph_no_deadlock1),
		unit_test(test_reduce_large_graph_single_deadlock),
		unit_test(test_reduce_large_graph_no_deadlock2)
	};

	MemoryContextInit();
	gddContext = AllocSetContextCreate(TopMemoryContext,
									   "GddContext",
									   ALLOCSET_DEFAULT_MINSIZE,
									   ALLOCSET_DEFAULT_INITSIZE,
									   ALLOCSET_DEFAULT_MAXSIZE);

	return run_tests(tests);
}
