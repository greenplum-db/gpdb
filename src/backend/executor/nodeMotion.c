/*-------------------------------------------------------------------------
 *
 * nodeMotion.c
 *	  Routines to handle moving tuples around in Greenplum Database.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/executor/nodeMotion.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/heapam.h"
#include "nodes/execnodes.h"	/* Slice, SliceTable */
#include "cdb/cdbmotion.h"
#include "cdb/cdbutil.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbhash.h"
#include "executor/executor.h"
#include "executor/execdebug.h"
#include "executor/execUtils.h"
#include "executor/nodeMotion.h"
#include "lib/binaryheap.h"
#include "utils/tuplesort.h"
#include "miscadmin.h"
#include "utils/memutils.h"


/* #define MEASURE_MOTION_TIME */

#ifdef MEASURE_MOTION_TIME
#include <unistd.h>				/* gettimeofday */
#endif

/* #define CDB_MOTION_DEBUG */

#ifdef CDB_MOTION_DEBUG
#include "utils/lsyscache.h"	/* getTypeOutputInfo */
#include "lib/stringinfo.h"		/* StringInfo */
#endif


/*=========================================================================
 * FUNCTIONS PROTOTYPES
 */
static TupleTableSlot *execMotionSender(MotionState *node);
static TupleTableSlot *execMotionUnsortedReceiver(MotionState *node);
static TupleTableSlot *execMotionSortedReceiver(MotionState *node);

static int	CdbMergeComparator(Datum lhs, Datum rhs, void *context);
static uint32 evalHashKey(ExprContext *econtext, List *hashkeys, CdbHash *h);

static void doSendEndOfStream(Motion *motion, MotionState *node);
static void doSendTuple(Motion *motion, MotionState *node, TupleTableSlot *outerTupleSlot);


/*=========================================================================
 */

#ifdef CDB_MOTION_DEBUG
static void
formatTuple(StringInfo buf, TupleTableSlot *slot, Oid *outputFunArray)
{
	TupleDesc tupdesc = slot->tts_tupleDescriptor;
	int			i;

	for (i = 0; i < tupdesc->natts; i++)
	{
		bool		isnull;
		Datum		d = slot_getattr(slot, i + 1, &isnull);

		if (d && !isnull)
		{
			char	   *s = OidOutputFunctionCall(outputFunArray[i], d);
			char	   *name = NameStr(tupdesc->attrs[i]->attname);

			if (name && *name)
				appendStringInfo(buf, "  %s=\"%.30s\"", name, s);
			else
				appendStringInfo(buf, "  \"%.30s\"", s);
			pfree(s);
		}
	}
	appendStringInfoChar(buf, '\n');
}
#endif

/* ----------------------------------------------------------------
 *		ExecMotion
 * ----------------------------------------------------------------
 */
TupleTableSlot *
ExecMotion(MotionState *node)
{
	Motion	   *motion = (Motion *) node->ps.plan;

	/* sanity check */
 	if (node->stopRequested)
 		ereport(ERROR,
 				(errcode(ERRCODE_INTERNAL_ERROR),
 				 errmsg("unexpected internal error"),
 				 errmsg("Already stopped motion node is executed again, data will lost"),
 				 errhint("Likely motion node is incorrectly squelched earlier")));

	/*
	 * at the top here we basically decide: -- SENDER vs. RECEIVER and --
	 * SORTED vs. UNSORTED
	 */
	if (node->mstype == MOTIONSTATE_RECV)
	{
		TupleTableSlot *tuple;
#ifdef MEASURE_MOTION_TIME
		struct timeval startTime;
		struct timeval stopTime;

		gettimeofday(&startTime, NULL);
#endif

		if (node->ps.state->active_recv_id >= 0)
		{
			if (node->ps.state->active_recv_id != motion->motionID)
			{
				/*
				 * See motion_sanity_walker() for details on how a deadlock
				 * may occur.
				 */
				elog(LOG, "DEADLOCK HAZARD: Updating active_motion_id from %d to %d",
					 node->ps.state->active_recv_id, motion->motionID);
				node->ps.state->active_recv_id = motion->motionID;
			}
		}
		else
			node->ps.state->active_recv_id = motion->motionID;

		if (motion->sendSorted)
			tuple = execMotionSortedReceiver(node);
		else
			tuple = execMotionUnsortedReceiver(node);

		/*
		 * We tell the upper node as if this was the end of tuple stream if
		 * query-finish is requested.  Unlike other nodes, we skipped this
		 * check in ExecProc because this node in sender mode should send EoS
		 * to the receiver side, but the receiver side can simply stop
		 * processing the stream.  The sender side of this stream could still
		 * be sending more tuples, but this slice will eventually clean up the
		 * executor and eventually Stop message will be delivered to the
		 * sender side.
		 */
		if (QueryFinishPending)
			tuple = NULL;

		if (tuple == NULL)
			node->ps.state->active_recv_id = -1;
#ifdef MEASURE_MOTION_TIME
		gettimeofday(&stopTime, NULL);

		node->motionTime.tv_sec += stopTime.tv_sec - startTime.tv_sec;
		node->motionTime.tv_usec += stopTime.tv_usec - startTime.tv_usec;

		while (node->motionTime.tv_usec < 0)
		{
			node->motionTime.tv_usec += 1000000;
			node->motionTime.tv_sec--;
		}

		while (node->motionTime.tv_usec >= 1000000)
		{
			node->motionTime.tv_usec -= 1000000;
			node->motionTime.tv_sec++;
		}
#endif
		return tuple;
	}
	else if (node->mstype == MOTIONSTATE_SEND)
	{
		return execMotionSender(node);
	}
	else
	{
		elog(ERROR, "cannot execute inactive Motion");
		return NULL;
	}
}

static TupleTableSlot *
execMotionSender(MotionState *node)
{
	/* SENDER LOGIC */
	TupleTableSlot *outerTupleSlot;
	PlanState  *outerNode;
	Motion	   *motion = (Motion *) node->ps.plan;
	bool		done = false;

#ifdef MEASURE_MOTION_TIME
	struct timeval time1;
	struct timeval time2;

	gettimeofday(&time1, NULL);
#endif

	AssertState(motion->motionType == MOTIONTYPE_GATHER ||
				motion->motionType == MOTIONTYPE_GATHER_SINGLE ||
				motion->motionType == MOTIONTYPE_HASH ||
				motion->motionType == MOTIONTYPE_BROADCAST ||
				(motion->motionType == MOTIONTYPE_EXPLICIT && motion->segidColIdx > 0));
	Assert(node->ps.state->interconnect_context);

	while (!done)
	{
		/* grab TupleTableSlot from our child. */
		outerNode = outerPlanState(node);
		outerTupleSlot = ExecProcNode(outerNode);

#ifdef MEASURE_MOTION_TIME
		gettimeofday(&time2, NULL);

		node->otherTime.tv_sec += time2.tv_sec - time1.tv_sec;
		node->otherTime.tv_usec += time2.tv_usec - time1.tv_usec;

		while (node->otherTime.tv_usec < 0)
		{
			node->otherTime.tv_usec += 1000000;
			node->otherTime.tv_sec--;
		}

		while (node->otherTime.tv_usec >= 1000000)
		{
			node->otherTime.tv_usec -= 1000000;
			node->otherTime.tv_sec++;
		}
#endif

		if (done || TupIsNull(outerTupleSlot))
		{
			doSendEndOfStream(motion, node);
			done = true;
		}
		else if (motion->motionType == MOTIONTYPE_GATHER_SINGLE &&
				 GpIdentity.segindex != (gp_session_id % node->numInputSegs))
		{
			/*
			 * For explicit gather motion, receiver gets data from one
			 * segment only. The others execute the subplan normally, but
			 * throw away the resulting tuples.
			 */
		}
		else
		{
			doSendTuple(motion, node, outerTupleSlot);
			/* doSendTuple() may have set node->stopRequested as a side-effect */

			if (node->stopRequested)
			{
				elog(gp_workfile_caching_loglevel, "Motion calling Squelch on child node");
				/* propagate stop notification to our children */
				ExecSquelchNode(outerNode);
				done = true;
			}
		}
#ifdef MEASURE_MOTION_TIME
		gettimeofday(&time1, NULL);

		node->motionTime.tv_sec += time1.tv_sec - time2.tv_sec;
		node->motionTime.tv_usec += time1.tv_usec - time2.tv_usec;

		while (node->motionTime.tv_usec < 0)
		{
			node->motionTime.tv_usec += 1000000;
			node->motionTime.tv_sec--;
		}

		while (node->motionTime.tv_usec >= 1000000)
		{
			node->motionTime.tv_usec -= 1000000;
			node->motionTime.tv_sec++;
		}
#endif
	}

	Assert(node->stopRequested || node->numTuplesFromChild == node->numTuplesToAMS);

	/* nothing else to send out, so we return NULL up the tree. */
	return NULL;
}


static TupleTableSlot *
execMotionUnsortedReceiver(MotionState *node)
{
	/* RECEIVER LOGIC */
	TupleTableSlot *slot;
	GenericTuple tuple;
	Motion	   *motion = (Motion *) node->ps.plan;

	AssertState(motion->motionType == MOTIONTYPE_GATHER ||
				motion->motionType == MOTIONTYPE_GATHER_SINGLE ||
				motion->motionType == MOTIONTYPE_HASH ||
				motion->motionType == MOTIONTYPE_BROADCAST ||
				(motion->motionType == MOTIONTYPE_EXPLICIT && motion->segidColIdx > 0));

	Assert(node->ps.state->motionlayer_context);
	Assert(node->ps.state->interconnect_context);

	if (node->stopRequested)
	{
		SendStopMessage(node->ps.state->motionlayer_context,
						node->ps.state->interconnect_context,
						motion->motionID);
		return NULL;
	}

	tuple = RecvTupleFrom(node->ps.state->motionlayer_context,
						  node->ps.state->interconnect_context,
						  motion->motionID, ANY_ROUTE);

	if (!tuple)
	{
#ifdef CDB_MOTION_DEBUG
		if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
			elog(DEBUG4, "motionID=%d saw end of stream", motion->motionID);
#endif
		Assert(node->numTuplesFromAMS == node->numTuplesToParent);
		Assert(node->numTuplesFromChild == 0);
		Assert(node->numTuplesToAMS == 0);
		return NULL;
	}

	node->numTuplesFromAMS++;
	node->numTuplesToParent++;

	/* store it in our result slot and return this. */
	slot = node->ps.ps_ResultTupleSlot;
	slot = ExecStoreGenericTuple(tuple, slot, true /* shouldFree */ );

#ifdef CDB_MOTION_DEBUG
	if (node->numTuplesToParent <= 20)
	{
		StringInfoData buf;

		initStringInfo(&buf);
		appendStringInfo(&buf, "   motion%-3d rcv      %5d.",
						 motion->motionID,
						 node->numTuplesToParent);
		formatTuple(&buf, slot, node->outputFunArray);
		elog(DEBUG3, "%s", buf.data);
		pfree(buf.data);
	}
#endif

	return slot;
}



/*
 * General background on Sorted Motion:
 * -----------------------------------
 * NOTE: This function is only used for order-preserving motion.  There are
 * only 2 types of motion that order-preserving makes sense for: FIXED and
 * BROADCAST (HASH does not make sense). so we have:
 *
 * CASE 1:	 broadcast order-preserving fixed motion.  This should only be
 *			 called for SENDERs.
 *
 * CASE 2:	 single-destination order-preserving fixed motion.	The SENDER
 *			 side will act like Unsorted motion and won't call this. So only
 *			 the RECEIVER should be called for this case.
 *
 *
 * Sorted Receive Notes:
 * --------------------
 *
 * The 1st time we execute, we need to pull a tuple from each of our source
 * and store them in our tupleheap.  Once that is done, we can pick the lowest
 * (or whatever the criterion is) value from amongst all the sources.  This
 * works since each stream is sorted itself.
 *
 * We keep track of which one was selected, this will be slot we will need
 * to fill during the next call.
 *
 * Subsequent calls to this function (after the 1st time) will start by
 * trying to receive a tuple for the slot that was emptied the previous call.
 * Then we again select the lowest value and return that tuple.
 */

/* Sorted receiver using binary heap */
static TupleTableSlot *
execMotionSortedReceiver(MotionState *node)
{
	TupleTableSlot *slot;
	binaryheap *hp = node->tupleheap;
	GenericTuple inputTuple;
	Motion	   *motion = (Motion *) node->ps.plan;
	EState	   *estate = node->ps.state;

	AssertState(motion->motionType == MOTIONTYPE_GATHER &&
				motion->sendSorted &&
				hp != NULL);

	/* Notify senders and return EOS if caller doesn't want any more data. */
	if (node->stopRequested)
	{

		SendStopMessage(node->ps.state->motionlayer_context,
						node->ps.state->interconnect_context,
						motion->motionID);
		return NULL;
	}

	/*
	 * On first call, fill the priority queue with each sender's first tuple.
	 */
	if (!node->tupleheapReady)
	{
		GenericTuple inputTuple;
		binaryheap *hp = node->tupleheap;
		Motion	   *motion = (Motion *) node->ps.plan;
		int			iSegIdx;
		ListCell   *lcProcess;
		ExecSlice  *sendSlice = &node->ps.state->es_sliceTable->slices[motion->motionID];

		Assert(sendSlice->sliceIndex == motion->motionID);

		foreach_with_count(lcProcess, sendSlice->primaryProcesses, iSegIdx)
		{
			MemoryContext oldcxt;

			if (lfirst(lcProcess) == NULL)
				continue;			/* skip this one: we are not receiving from it */

			inputTuple = RecvTupleFrom(node->ps.state->motionlayer_context,
									   node->ps.state->interconnect_context,
									   motion->motionID, iSegIdx);

			if (!inputTuple)
				continue;			/* skip this one: received nothing */

			/*
			 * Make a slot to hold this tuple. We will reuse it to hold any
			 * future tuples from the same sender. We initialized the result
			 * tuple slot with the correct type earlier, so make the new slot
			 * have the same type.
			 */
			oldcxt = MemoryContextSwitchTo(estate->es_query_cxt);
			node->slots[iSegIdx] = MakeTupleTableSlot();
			ExecSetSlotDescriptor(node->slots[iSegIdx],
								  node->ps.ps_ResultTupleSlot->tts_tupleDescriptor);
			MemoryContextSwitchTo(oldcxt);

			/*
			 * Store the tuple in the slot, and add it to the heap.
			 *
			 * Use slot_getsomeattrs() to materialize the columns we need for
			 * the comparisons in the tts_values/isnull arrays. The comparator
			 * can then peek directly into the arrays, which is cheaper than
			 * calling slot_getattr() all the time.
			 */
			ExecStoreGenericTuple(inputTuple, node->slots[iSegIdx], true);
			slot_getsomeattrs(node->slots[iSegIdx], node->lastSortColIdx);
			binaryheap_add_unordered(hp, iSegIdx);

			node->numTuplesFromAMS++;

#ifdef CDB_MOTION_DEBUG
			if (node->numTuplesFromAMS <= 20)
			{
				StringInfoData buf;

				initStringInfo(&buf);
				appendStringInfo(&buf, "   motion%-3d rcv<-%-3d %5d.",
								 motion->motionID,
								 iSegIdx,
								 node->numTuplesFromAMS);
				formatTuple(&buf, node->slots[iSegIdx], node->outputFunArray);
				elog(DEBUG3, "%s", buf.data);
				pfree(buf.data);
			}
#endif
		}
		Assert(iSegIdx == node->numInputSegs);

		/*
		 * Done adding the elements, now arrange the heap to satisfy the heap
		 * property. This is quicker than inserting the initial elements one by
		 * one.
		 */
		binaryheap_build(hp);

		node->tupleheapReady = true;
	}

	/*
	 * Delete from the priority queue the element that we fetched last time.
	 * Receive and insert the next tuple from that same sender.
	 */
	else
	{
		/* Old element is still at the head of the pq. */
		Assert(DatumGetInt32(binaryheap_first(hp)) == node->routeIdNext);

		/* Receive the successor of the tuple that we returned last time. */
		inputTuple = RecvTupleFrom(node->ps.state->motionlayer_context,
								   node->ps.state->interconnect_context,
								   motion->motionID,
								   node->routeIdNext);

		/* Substitute it in the pq for its predecessor. */
		if (inputTuple)
		{
			ExecStoreGenericTuple(inputTuple, node->slots[node->routeIdNext], true);
			slot_getsomeattrs(node->slots[node->routeIdNext], node->lastSortColIdx);
			binaryheap_replace_first(hp, Int32GetDatum(node->routeIdNext));

			node->numTuplesFromAMS++;

#ifdef CDB_MOTION_DEBUG
			if (node->numTuplesFromAMS <= 20)
			{
				StringInfoData buf;

				initStringInfo(&buf);
				appendStringInfo(&buf, "   motion%-3d rcv<-%-3d %5d.",
								 motion->motionID,
								 node->routeIdNext,
								 node->numTuplesFromAMS);
				formatTuple(&buf, node->slots[node->routeIdNext], node->outputFunArray);
				elog(DEBUG3, "%s", buf.data);
				pfree(buf.data);
			}
#endif
		}
		else
		{
			/* At EOS, drop this sender from the priority queue. */
			binaryheap_remove_first(hp);
		}
	}

	/* Finished if all senders have returned EOS. */
	if (binaryheap_empty(hp))
	{
		Assert(node->numTuplesFromAMS == node->numTuplesToParent);
		Assert(node->numTuplesFromChild == 0);
		Assert(node->numTuplesToAMS == 0);
		return NULL;
	}

	/*
	 * Our next result tuple, with lowest key among all senders, is now at the
	 * head of the priority queue.  Get it from there.
	 *
	 * We transfer ownership of the tuple from the pq element to our caller,
	 * but the pq element itself will remain in place until the next time we
	 * are called, to avoid an unnecessary rearrangement of the priority
	 * queue.
	 */
	node->routeIdNext = binaryheap_first(hp);
	slot = node->slots[node->routeIdNext];

	/* Update counters. */
	node->numTuplesToParent++;

#ifdef CDB_MOTION_DEBUG
	if (node->numTuplesToParent <= 20)
	{
		StringInfoData buf;

		initStringInfo(&buf);
		appendStringInfo(&buf, "   motion%-3d mrg<-%-3d %5d.",
						 motion->motionID,
						 node->routeIdNext,
						 node->numTuplesToParent);
		formatTuple(&buf, slot, node->outputFunArray);
		elog(DEBUG3, "%s", buf.data);
		pfree(buf.data);
	}
#endif

	/* Return result slot. */
	return slot;
}								/* execMotionSortedReceiver */

/* ----------------------------------------------------------------
 *		ExecInitMotion
 *
 * NOTE: have to be a bit careful, estate->es_cur_slice_idx is not the
 *		 ultimate correct value that it should be on the QE. this happens
 *		 after this call in mppexec.c.	This is ok since we don't need it,
 *		 but just be aware before you try and use it here.
 * ----------------------------------------------------------------
 */

MotionState *
ExecInitMotion(Motion *node, EState *estate, int eflags)
{
	MotionState *motionstate = NULL;
	TupleDesc	tupDesc;
	ExecSlice  *sendSlice;
	ExecSlice  *recvSlice;
	SliceTable *sliceTable = estate->es_sliceTable;
	PlanState  *outerPlan;
	int			parentIndex;

	/*
	 * If GDD is enabled, the lock of table may downgrade to RowExclusiveLock,
	 * (see CdbTryOpenRelation function), then EPQ would be triggered, EPQ will
	 * execute the subplan in the executor, so it will create a new EState,
	 * but there are no slice tables in the new EState and we can not AssignGangs
	 * on the QE. In this case, we raise an error.
	 */
	if (estate->es_epqTuple)
		ereport(ERROR,
				(errcode(ERRCODE_T_R_SERIALIZATION_FAILURE),
				 errmsg("EvalPlanQual can not handle subPlan with Motion node")));

	Assert(node->motionID > 0);
	Assert(node->motionID < sliceTable->numSlices);
	AssertImply(node->motionType == MOTIONTYPE_HASH, node->numHashSegments > 0);

	parentIndex = estate->currentSliceId;
	estate->currentSliceId = node->motionID;

	/*
	 * create state structure
	 */
	motionstate = makeNode(MotionState);
	motionstate->ps.plan = (Plan *) node;
	motionstate->ps.state = estate;
	motionstate->mstype = MOTIONSTATE_NONE;
	motionstate->stopRequested = false;
	motionstate->hashExprs = NIL;
	motionstate->cdbhash = NULL;

	/* Look up the sending and receiving gang's slice table entries. */
	sendSlice = &sliceTable->slices[node->motionID];
	Assert(sendSlice->sliceIndex == node->motionID);
	recvSlice = &sliceTable->slices[parentIndex];
	Assert(parentIndex == sendSlice->parentIndex);

	/* QD must fill in the global slice table. */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		MemoryContext oldcxt = MemoryContextSwitchTo(estate->es_query_cxt);

		if (node->motionType == MOTIONTYPE_GATHER ||
			node->motionType == MOTIONTYPE_GATHER_SINGLE)
		{
			/* Sending to a single receiving process on the entry db? */
			/* Is receiving slice a root slice that runs here in the qDisp? */
			if (recvSlice->sliceIndex == recvSlice->rootIndex)
			{
				motionstate->mstype = MOTIONSTATE_RECV;
				Assert(recvSlice->gangType == GANGTYPE_UNALLOCATED ||
					   recvSlice->gangType == GANGTYPE_PRIMARY_WRITER);
			}
			else
			{
				/* sanity checks */
				if (list_length(recvSlice->segments) != 1)
					elog(ERROR, "unexpected gang size: %d", list_length(recvSlice->segments));
			}
		}

		MemoryContextSwitchTo(oldcxt);
	}

	/* QE must fill in map from motionID to MotionState node. */
	else
	{
		Insist(Gp_role == GP_ROLE_EXECUTE);

		if (LocallyExecutingSliceIndex(estate) == recvSlice->sliceIndex)
		{
			/* this is recv */
			motionstate->mstype = MOTIONSTATE_RECV;
		}
		else if (LocallyExecutingSliceIndex(estate) == sendSlice->sliceIndex)
		{
			/* this is send */
			motionstate->mstype = MOTIONSTATE_SEND;
		}
		/* TODO: If neither sending nor receiving, don't bother to initialize. */
	}

	motionstate->tupleheapReady = false;
	motionstate->sentEndOfStream = false;

	motionstate->otherTime.tv_sec = 0;
	motionstate->otherTime.tv_usec = 0;
	motionstate->motionTime.tv_sec = 0;
	motionstate->motionTime.tv_usec = 0;

	motionstate->numTuplesFromChild = 0;
	motionstate->numTuplesToAMS = 0;
	motionstate->numTuplesFromAMS = 0;
	motionstate->numTuplesToParent = 0;

	motionstate->stopRequested = false;
	motionstate->numInputSegs = list_length(sendSlice->segments);

	/*
	 * Miscellaneous initialization
	 *
	 * create expression context for node
	 */
	ExecAssignExprContext(estate, &motionstate->ps);

	/*
	 * tuple table initialization
	 */
	ExecInitResultTupleSlot(estate, &motionstate->ps);

	/*
	 * Initializes child nodes. If alien elimination is on, we skip children
	 * of receiver motion.
	 */
	if (!estate->eliminateAliens || motionstate->mstype == MOTIONSTATE_SEND)
	{
		outerPlanState(motionstate) = ExecInitNode(outerPlan(node), estate, eflags);
	}

	/*
	 * initialize tuple type.  no need to initialize projection info because
	 * this node doesn't do projections.
	 */
	outerPlan = outerPlanState(motionstate);

	/*
	 * The advertised 'tdhasoid' flag in our result tuple desc must match what
	 * the outer plan produces. Otherwise, the sender will send tuples that
	 * have OIDs, but the receiver treats the tuples as if they doesn't have
	 * OIDs, or vice versa. This isn't so important for HeapTuples, which have
	 * an HAS_OIDS flag on every tuple, but for MemTuples it is critical,
	 * because it affects the way the they are deformed.
	 *
	 * GPDB_95_MERGE_FIXME: Should we force ORCA to always use the TL for
	 * motion nodes or modify ORCA to use the TL from the outer node?
	 */
	if (outerPlan && ExecGetResultType(outerPlan) && estate->es_plannedstmt->planGen == PLANGEN_PLANNER)
	{
		/*
		 * This is like ExecAssignResultTypeFromTL(), but we copy the tdhasoid
		 * flag from the subplan.
		 */
		bool		hasoid = ExecGetResultType(outerPlan)->tdhasoid;

		tupDesc = ExecTypeFromTL(motionstate->ps.plan->targetlist, hasoid);
		ExecAssignResultType(&motionstate->ps, tupDesc);
	}
	else
	{
		ExecAssignResultTypeFromTL(&motionstate->ps);
		tupDesc = ExecGetResultType(&motionstate->ps);
	}

	motionstate->ps.ps_ProjInfo = NULL;
	motionstate->numHashSegments = node->numHashSegments;

	/* Set up motion send data structures */
	if (motionstate->mstype == MOTIONSTATE_SEND && node->motionType == MOTIONTYPE_HASH)
	{
		int			nkeys;

		Assert(node->numHashSegments > 0);
		Assert(node->numHashSegments <= recvSlice->planNumSegments);
		nkeys = list_length(node->hashExprs);

		if (nkeys > 0)
			motionstate->hashExprs = (List *) ExecInitExpr((Expr *) node->hashExprs,
														   (PlanState *) motionstate);

		/*
		 * Create hash API reference
		 */
		motionstate->cdbhash = makeCdbHash(motionstate->numHashSegments,
										   nkeys,
										   node->hashFuncs);
	}

	/*
	 * Merge Receive: Set up the key comparator and priority queue.
	 *
	 * This is very similar to a Merge Append.
	 */
	if (node->sendSorted && motionstate->mstype == MOTIONSTATE_RECV)
	{
		int			numInputSegs = motionstate->numInputSegs;
		int			lastSortColIdx = 0;

		/* Allocate array to slots for the next tuple from each sender */
		motionstate->slots = palloc0(numInputSegs * sizeof(TupleTableSlot *));

		/* Prepare SortSupport data for each column */
		motionstate->numSortCols = node->numSortCols;
		motionstate->sortKeys = (SortSupport) palloc0(node->numSortCols * sizeof(SortSupportData));

		for (int i = 0; i < node->numSortCols; i++)
		{
			SortSupport sortKey = &motionstate->sortKeys[i];

			AssertArg(node->sortColIdx[i] != 0);
			AssertArg(node->sortOperators[i] != 0);

			sortKey->ssup_cxt = CurrentMemoryContext;
			sortKey->ssup_collation = node->collations[i];
			sortKey->ssup_nulls_first = node->nullsFirst[i];
			sortKey->ssup_attno = node->sortColIdx[i];

			PrepareSortSupportFromOrderingOp(node->sortOperators[i], sortKey);

			/* Also make note of the last column used in the sort key */
			if (node->sortColIdx[i] > lastSortColIdx)
				lastSortColIdx = node->sortColIdx[i];
		}
		motionstate->lastSortColIdx = lastSortColIdx;
		motionstate->tupleheap =
			binaryheap_allocate(motionstate->numInputSegs,
								CdbMergeComparator,
								motionstate);
	}

	/*
	 * Perform per-node initialization in the motion layer.
	 */
	UpdateMotionLayerNode(motionstate->ps.state->motionlayer_context,
						  node->motionID,
						  node->sendSorted,
						  tupDesc);


#ifdef CDB_MOTION_DEBUG
	motionstate->outputFunArray = (Oid *) palloc(tupDesc->natts * sizeof(Oid));
	for (int i = 0; i < tupDesc->natts; i++)
	{
		bool		typisvarlena;

		getTypeOutputInfo(tupDesc->attrs[i]->atttypid,
						  &motionstate->outputFunArray[i],
						  &typisvarlena);
	}
#endif

	estate->currentSliceId = parentIndex;

	return motionstate;
}

/* ----------------------------------------------------------------
 *		ExecEndMotion(node)
 * ----------------------------------------------------------------
 */
void
ExecEndMotion(MotionState *node)
{
	Motion	   *motion = (Motion *) node->ps.plan;
#ifdef MEASURE_MOTION_TIME
	double		otherTimeSec;
	double		motionTimeSec;
#endif

	ExecFreeExprContext(&node->ps);

	/*
	 * clean out the tuple table
	 */
	ExecClearTuple(node->ps.ps_ResultTupleSlot);

	/*
	 * Set the slice no for the nodes under this motion.
	 */
	Assert(node->ps.state != NULL);

	/*
	 * shut down the subplan
	 */
	ExecEndNode(outerPlanState(node));

#ifdef MEASURE_MOTION_TIME
	motionTimeSec = (double) node->motionTime.tv_sec + (double) node->motionTime.tv_usec / 1000000.0;

	if (node->mstype == MOTIONSTATE_RECV)
	{
		elog(DEBUG1,
			 "Motion Node %d (RECEIVER) Statistics:\n"
			 "Timing:  \n"
			 "\t Time receiving the tuple: %f sec\n"
			 "Counters: \n"
			 "\tnumTuplesFromChild: %d\n"
			 "\tnumTuplesFromAMS: %d\n"
			 "\tnumTuplesToAMS: %d\n"
			 "\tnumTuplesToParent: %d\n",
			 motNodeID,
			 motionTimeSec,
			 node->numTuplesFromChild,
			 node->numTuplesFromAMS,
			 node->numTuplesToAMS,
			 node->numTuplesToParent
			);
	}
	else if (node->mstype == MOTIONSTATE_SEND)
	{
		otherTimeSec = (double) node->otherTime.tv_sec + (double) node->otherTime.tv_usec / 1000000.0;
		elog(DEBUG1,
			 "Motion Node %d (SENDER) Statistics:\n"
			 "Timing:  \n"
			 "\t Time getting next tuple to send: %f sec \n"
			 "\t Time sending the tuple:          %f  sec\n"
			 "\t Percentage of time sending:      %2.2f%% \n"
			 "Counters: \n"
			 "\tnumTuplesFromChild: %d\n"
			 "\tnumTuplesToAMS: %d\n",
			 motNodeID,
			 otherTimeSec,
			 motionTimeSec,
			 (double) (motionTimeSec / (otherTimeSec + motionTimeSec)) * 100,
			 node->numTuplesFromChild,
			 node->numTuplesToAMS
			);
	}
#endif							/* MEASURE_MOTION_TIME */

	/* Merge Receive: Free the priority queue and associated structures. */
	if (node->tupleheap != NULL)
	{
		binaryheap_free(node->tupleheap);
		node->tupleheap = NULL;
	}

	/* Free the slices and routes */
	if (node->cdbhash != NULL)
	{
		pfree(node->cdbhash);
		node->cdbhash = NULL;
	}

	/*
	 * Free up this motion node's resources in the Motion Layer.
	 *
	 * TODO: For now, we don't flush the comm-layer.  NO ERRORS DURING AMS!!!
	 */
	EndMotionLayerNode(node->ps.state->motionlayer_context, motion->motionID,
					   /* flush-comm-layer */ false);

#ifdef CDB_MOTION_DEBUG
	if (node->outputFunArray)
		pfree(node->outputFunArray);
#endif
}



/*=========================================================================
 * HELPER FUNCTIONS
 */

/*
 * CdbMergeComparator:
 * Used to compare tuples for a sorted motion node.
 */
static int
CdbMergeComparator(Datum lhs, Datum rhs, void *context)
{
	MotionState *node = (MotionState *) context;
	int			lSegIdx = DatumGetInt32(lhs);
	int			rSegIdx = DatumGetInt32(rhs);

	TupleTableSlot *lslot = node->slots[lSegIdx];
	TupleTableSlot *rslot = node->slots[rSegIdx];
	SortSupport	sortKeys = node->sortKeys;
	int			nkey;
	int			compare;

	Assert(lslot && rslot);

	for (nkey = 0; nkey < node->numSortCols; nkey++)
	{
		SortSupport ssup = &sortKeys[nkey];
		AttrNumber	attno = ssup->ssup_attno;
		Datum		datum1,
					datum2;
		bool		isnull1,
					isnull2;

		/*
		 * The caller has called slot_getsomeattrs() to ensure
		 * that all the columns we need are available directly in
		 * the values/isnull arrays.
		 */
		datum1 = lslot->PRIVATE_tts_values[attno - 1];
		isnull1 = lslot->PRIVATE_tts_isnull[attno - 1];
		datum2 = rslot->PRIVATE_tts_values[attno - 1];
		isnull2 = rslot->PRIVATE_tts_isnull[attno - 1];

		compare = ApplySortComparator(datum1, isnull1,
									  datum2, isnull2,
									  ssup);
		if (compare != 0)
		{
			INVERT_COMPARE_RESULT(compare);
			return compare;
		}
	}
	return 0;
}								/* CdbMergeComparator */

/*
 * Experimental code that will be replaced later with new hashing mechanism
 */
uint32
evalHashKey(ExprContext *econtext, List *hashkeys, CdbHash * h)
{
	ListCell   *hk;
	MemoryContext oldContext;
	unsigned int target_seg;

	ResetExprContext(econtext);

	oldContext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

	/*
	 * If we have 1 or more distribution keys for this relation, hash them.
	 * However, If this happens to be a relation with an empty policy
	 * (partitioning policy with a NULL distribution key list) then we have no
	 * hash key value to feed in, so use cdbhashrandomseg() to pick a segment
	 * at random.
	 */
	if (list_length(hashkeys) > 0)
	{
		int			i;

		cdbhashinit(h);

		i = 0;
		foreach(hk, hashkeys)
		{
			ExprState  *keyexpr = (ExprState *) lfirst(hk);
			Datum		keyval;
			bool		isNull;

			/*
			 * Get the attribute value of the tuple
			 */
			keyval = ExecEvalExpr(keyexpr, econtext, &isNull, NULL);

			/*
			 * Compute the hash function
			 */
			cdbhash(h, i + 1, keyval, isNull);
			i++;
		}
		target_seg = cdbhashreduce(h);
	}
	else
	{
		target_seg = cdbhashrandomseg(h->numsegs);
	}

	MemoryContextSwitchTo(oldContext);

	return target_seg;
}


void
doSendEndOfStream(Motion *motion, MotionState *node)
{
	/*
	 * We have no more child tuples, but we have not successfully sent an
	 * End-of-Stream token yet.
	 */
	SendEndOfStream(node->ps.state->motionlayer_context,
					node->ps.state->interconnect_context,
					motion->motionID);
	node->sentEndOfStream = true;
}

/*
 * A crufty confusing part of the current code is how contentId is used within
 * the motion structures and then how that gets translated to targetRoutes by
 * this motion nodes.
 *
 * WARNING: There are ALOT of assumptions in here about how the motion node
 *			instructions are encoded into motion and stuff.
 *
 * There are 3 types of sending that can happen here:
 *
 *	FIXED - sending to a single process.  the value in node->fixedSegIdxMask[0]
 *			is the contentId of who to send to.  But we can actually ignore that
 *			since now with slice tables, we should only have a single CdbProcess
 *			that we could send to for this motion node.
 *
 *
 *	BROADCAST - actually a subcase of FIXED, but handling is simple. send to all
 *				of our routes.
 *
 *	HASH -	maps hash values to segid.	this mapping is 1->1 such that a hash
 *			value of 2 maps to contentid of 2 (for now).  Since we can't ever
 *			use Hash to send to the QD, the QD's contentid of -1 is not an issue.
 *			Also, the contentid maps directly to the routeid.
 *
 */
void
doSendTuple(Motion *motion, MotionState *node, TupleTableSlot *outerTupleSlot)
{
	int16		targetRoute;
	SendReturnCode sendRC;
	ExprContext *econtext = node->ps.ps_ExprContext;

	/* We got a tuple from the child-plan. */
	node->numTuplesFromChild++;

	if (motion->motionType == MOTIONTYPE_GATHER ||
		motion->motionType == MOTIONTYPE_GATHER_SINGLE)
	{
		/*
		 * Actually, since we can only send to a single output segment
		 * here, we are guaranteed that we only have a single targetRoute
		 * setup that we could possibly send to.  So we can cheat and just
		 * fix the targetRoute to 0 (the 1st route).
		 */
		targetRoute = 0;

	}
	else if (motion->motionType == MOTIONTYPE_BROADCAST)
	{
		targetRoute = BROADCAST_SEGIDX;
	}
	else if (motion->motionType == MOTIONTYPE_HASH) /* Redistribute */
	{
		uint32		hval = 0;

		econtext->ecxt_outertuple = outerTupleSlot;

		hval = evalHashKey(econtext, node->hashExprs, node->cdbhash);

#ifdef USE_ASSERT_CHECKING
		Assert(hval < node->numHashSegments &&
			   "redistribute destination outside segment array");
#endif							/* USE_ASSERT_CHECKING */

		/*
		 * hashSegIdx takes our uint32 and maps it to an int, and here we
		 * assign it to an int16. See below.
		 */
		targetRoute = hval;

		/*
		 * see MPP-2099, let's not run into this one again! NOTE: the
		 * definition of BROADCAST_SEGIDX is key here, it *cannot* be a valid
		 * route which our map (above) will *ever* return.
		 *
		 * Note the "mapping" is generated at *planning* time in
		 * makeDefaultSegIdxArray() in cdbmutate.c (it is the trivial map, and
		 * is passed around our system a fair amount!).
		 */
		Assert(targetRoute != BROADCAST_SEGIDX);
	}
	else if (motion->motionType == MOTIONTYPE_EXPLICIT)
	{
		Datum		segidColIdxDatum;

		Assert(motion->segidColIdx > 0 && motion->segidColIdx <= list_length((motion->plan).targetlist));
		bool		is_null = false;

		segidColIdxDatum = slot_getattr(outerTupleSlot, motion->segidColIdx, &is_null);
		targetRoute = Int32GetDatum(segidColIdxDatum);
		Assert(!is_null);
	}
	else
		elog(ERROR, "unknown motion type %d", motion->motionType);

	CheckAndSendRecordCache(node->ps.state->motionlayer_context,
							node->ps.state->interconnect_context,
							motion->motionID,
							targetRoute);

	/* send the tuple out. */
	sendRC = SendTuple(node->ps.state->motionlayer_context,
					   node->ps.state->interconnect_context,
					   motion->motionID,
					   outerTupleSlot,
					   targetRoute);

	Assert(sendRC == SEND_COMPLETE || sendRC == STOP_SENDING);
	if (sendRC == SEND_COMPLETE)
		node->numTuplesToAMS++;
	else
		node->stopRequested = true;

#ifdef CDB_MOTION_DEBUG
	if (sendRC == SEND_COMPLETE && node->numTuplesToAMS <= 20)
	{
		StringInfoData buf;

		initStringInfo(&buf);
		appendStringInfo(&buf, "   motion%-3d snd->%-3d, %5d.",
						 motion->motionID,
						 targetRoute,
						 node->numTuplesToAMS);
		formatTuple(&buf, outerTupleSlot, node->outputFunArray);
		elog(DEBUG3, "%s", buf.data);
		pfree(buf.data);
	}
#endif
}


/*
 * ExecReScanMotion
 *
 * Motion nodes do not allow rescan after a tuple has been fetched.
 *
 * When the planner knows that a NestLoop cannot have more than one outer
 * tuple, it can omit the usual Materialize operator atop the inner subplan,
 * which can lead to invocation of ExecReScanMotion before the motion node's
 * first tuple is fetched.  Rescan can be implemented as a no-op in this case.
 * (After ExecNestLoop fetches an outer tuple, it invokes rescan on the inner
 * subplan before fetching the first inner tuple.  That doesn't bother us,
 * provided there is only one outer tuple.)
 */
void
ExecReScanMotion(MotionState *node)
{
	if (node->mstype != MOTIONSTATE_RECV ||
		node->numTuplesToParent != 0)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("illegal rescan of motion node: invalid plan"),
				 errhint("Likely caused by bad NL-join, try setting enable_nestloop to off")));
	}
	return;
}


/*
 * Mark this node as "stopped." When ExecProcNode() is called on a
 * stopped motion node it should behave as if there are no tuples
 * available.
 *
 * ExecProcNode() on a stopped motion node should also notify the
 * "other end" of the motion node of the stoppage.
 *
 * Note: once this is called, it is possible that the motion node will
 * never be called again, so we *must* send the stop message now.
 */
void
ExecSquelchMotion(MotionState *node)
{
	Motion	   *motion;

	AssertArg(node != NULL);

	motion = (Motion *) node->ps.plan;
	node->stopRequested = true;
	node->ps.state->active_recv_id = -1;

	/* pass down */
	SendStopMessage(node->ps.state->motionlayer_context,
					node->ps.state->interconnect_context,
					motion->motionID);
}
