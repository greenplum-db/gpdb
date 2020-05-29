/*-------------------------------------------------------------------------
 *
 * cdbmotion.c
 *		Access into the motion-layer in order to send and receive tuples
 *		within a motion node ID within a particular process group id.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/motion/cdbmotion.c
 *
 * Reviewers: jzhang, tkordas
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup.h"
#include "libpq-fe.h"
#include "libpq-int.h"
#include "cdb/cdbconn.h"
#include "cdb/cdbmotion.h"
#include "cdb/cdbvars.h"
#include "cdb/htupfifo.h"
#include "cdb/ml_ipc.h"
#include "cdb/tupser.h"
#include "utils/memutils.h"
#include "utils/typcache.h"


/*
 * MOTION NODE INFO DATA STRUCTURES
 */
int			Gp_max_tuple_chunk_size;

/*
 * STATIC STATE VARS
 *
 * Note, the alignment here isn't quite right.
 * GCC4.0 doesn't like our earlier initializer method of declaration.
 *
 * static TupleChunkListItemData s_eos_chunk_data = {NULL, TUPLE_CHUNK_HEADER_SIZE, NULL, "                "};
 */
static uint8 s_eos_buffer[sizeof(TupleChunkListItemData) + 8];
static TupleChunkListItem s_eos_chunk_data = (TupleChunkListItem) s_eos_buffer;

/*
 * HELPER FUNCTION DECLARATIONS
 */
static MotionNodeEntry *getMotionNodeEntry(MotionLayerState *mlStates, int16 motNodeID);
static ChunkSorterEntry *getChunkSorterEntry(MotionLayerState *mlStates,
					MotionNodeEntry *motNodeEntry,
					int16 srcRoute);
static void addChunkToSorter(ChunkTransportState *transportStates,
							 MotionNodeEntry *pMNEntry,
							 TupleChunkListItem tcItem,
							 int16 motNodeID,
							 ChunkSorterEntry *chunkSorterEntry,
							 ChunkTransportStateEntry *pEntry,
							 MotionConn *conn,
							 int16 srcRoute);

static void processIncomingChunks(MotionLayerState *mlStates,
					  ChunkTransportState *transportStates,
					  MotionNodeEntry *pMNEntry,
					  int16 motNodeID,
					  int16 srcRoute);

static inline void reconstructTuple(MotionNodeEntry *pMNEntry, ChunkSorterEntry *pCSEntry, TupleRemapper *remapper);

/* Stats-function declarations. */
static void statSendTuple(MotionLayerState *mlStates, MotionNodeEntry *pMNEntry, TupleChunkList tcList);
static void statSendEOS(MotionLayerState *mlStates, MotionNodeEntry *pMNEntry);
static void statChunksProcessed(MotionLayerState *mlStates, MotionNodeEntry *pMNEntry, int chunksProcessed, int chunkBytes, int tupleBytes);
static void statNewTupleArrived(MotionNodeEntry *pMNEntry, ChunkSorterEntry *pCSEntry);
static void statRecvTuple(MotionNodeEntry *pMNEntry, ChunkSorterEntry *pCSEntry);
static bool ShouldSendRecordCache(MotionConn *conn, SerTupInfo *pSerInfo);
static void UpdateSentRecordCache(MotionConn *conn);



/* Helper function to perform the operations necessary to reconstruct a
 * HeapTuple from a list of tuple-chunks, and then update the Motion Layer
 * state appropriately.  This includes storing the tuple, cleaning out the
 * tuple-chunk list, and recording statistics about the newly formed tuple.
 */
static inline void
reconstructTuple(MotionNodeEntry *pMNEntry, ChunkSorterEntry *pCSEntry, TupleRemapper *remapper)
{
	GenericTuple tup;
	SerTupInfo *pSerInfo = &pMNEntry->ser_tup_info;

	/*
	 * Convert the list of chunks into a tuple, then stow it away.
	 */
	tup = CvtChunksToTup(&pCSEntry->chunk_list, pSerInfo, remapper);

	/* We're done with the chunks now. */
	clearTCList(NULL, &pCSEntry->chunk_list);

	if (!tup)
		return;

	tup = TRCheckAndRemap(remapper, pSerInfo->tupdesc, tup);

	htfifo_addtuple(pCSEntry->ready_tuples, tup);

	/* Stats */
	statNewTupleArrived(pMNEntry, pCSEntry);
}

/*
 * FUNCTION DEFINITIONS
 */

/*
 * This function deletes the Motion Layer state at the end of a query
 * execution.
 */
void
RemoveMotionLayer(MotionLayerState *mlStates)
{
	if (!mlStates)
		return;

	if (Gp_role == GP_ROLE_UTILITY)
		return;

#ifdef AMS_VERBOSE_LOGGING
	/* Emit statistics to log */
	if (gp_log_interconnect >= GPVARS_VERBOSITY_VERBOSE)
		elog(LOG, "RemoveMotionLayer(): dumping stats\n"
			 "      Sent: %9u chunks %9u total bytes %9u tuple bytes\n"
			 "  Received: %9u chunks %9u total bytes %9u tuple bytes; "
			 "%9u chunkproc calls\n",
			 mlStates->stat_total_chunks_sent,
			 mlStates->stat_total_bytes_sent,
			 mlStates->stat_tuple_bytes_sent,
			 mlStates->stat_total_chunks_recvd,
			 mlStates->stat_total_bytes_recvd,
			 mlStates->stat_tuple_bytes_recvd,
			 mlStates->stat_total_chunkproc_calls);
#endif

	/*
	 * Free all memory used by the Motion Layer in the processing of this
	 * query.
	 */
	if (mlStates->motion_layer_mctx != NULL)
		MemoryContextDelete(mlStates->motion_layer_mctx);
}


MotionLayerState *
createMotionLayerState(int maxMotNodeID)
{
	MemoryContext oldCtxt;
	MemoryContext ml_mctx;
	uint8	   *pData;
	MotionLayerState *mlState;

	if (Gp_role == GP_ROLE_UTILITY)
		return NULL;

	if (Gp_interconnect_type == INTERCONNECT_TYPE_UDPIFC)
		Gp_max_tuple_chunk_size = Gp_max_packet_size - sizeof(struct icpkthdr) - TUPLE_CHUNK_HEADER_SIZE;
	else if (Gp_interconnect_type == INTERCONNECT_TYPE_TCP)
		Gp_max_tuple_chunk_size = Gp_max_packet_size - PACKET_HEADER_SIZE - TUPLE_CHUNK_HEADER_SIZE;

	/*
	 * Use the statically allocated chunk that is intended for sending end-of-
	 * stream messages so that we don't incur allocation and deallocation
	 * overheads.
	 */
	s_eos_chunk_data->p_next = NULL;
	s_eos_chunk_data->inplace = NULL;
	s_eos_chunk_data->chunk_length = TUPLE_CHUNK_HEADER_SIZE;

	pData = s_eos_chunk_data->chunk_data;

	SetChunkDataSize(pData, 0);
	SetChunkType(pData, TC_END_OF_STREAM);

	/*
	 * Create the memory-contexts that we will use within the Motion Layer.
	 *
	 * We make the Motion Layer memory-context a child of the ExecutorState
	 * Context, as it lives inside of the estate of a specific query and needs
	 * to get freed when the query is finished.
	 *
	 * The tuple-serial memory-context is a child of the Motion Layer
	 * memory-context
	 *
	 * NOTE: we need to be sure the caller is in ExecutorState memory context
	 * (estate->es_query_cxt) before calling us .
	 */
	ml_mctx =
		AllocSetContextCreate(CurrentMemoryContext, "MotionLayerMemCtxt",
							  ALLOCSET_SMALL_MINSIZE,
							  ALLOCSET_SMALL_INITSIZE,
							  ALLOCSET_DEFAULT_MAXSIZE);	/* use a setting bigger
															 * than "small" */

	/*
	 * Switch to the Motion Layer memory context, so that we can clean things
	 * up easily.
	 */
	oldCtxt = MemoryContextSwitchTo(ml_mctx);

	mlState = palloc0(sizeof(MotionLayerState));

	mlState->mnEntries = palloc0(maxMotNodeID * sizeof(MotionNodeEntry));
	mlState->mneCount = maxMotNodeID;

	for (int motNodeID = 1; motNodeID <= maxMotNodeID; motNodeID++)
	{
		MotionNodeEntry *pEntry = &mlState->mnEntries[motNodeID - 1];

		pEntry->motion_node_id = motNodeID;
		pEntry->valid = true;

		/*
		 * we'll just set this to 0.  later, ml_ipc will call
		 * setExpectedReceivers() to set this if we are a "Receiving" motion node.
		 */
		pEntry->num_senders = 0;
	}

	/* Allocation is done.	Go back to caller memory-context. */
	MemoryContextSwitchTo(oldCtxt);

	/*
	 * Keep our motion layer memory context in our newly created motion layer.
	 */
	mlState->motion_layer_mctx = ml_mctx;

	return mlState;
}

/*
 * Initialize a single motion node.  This is called by the executor when a
 * motion node in the plan tree is being initialized.
 *
 * This function is called from:  ExecInitMotion()
 */
void
UpdateMotionLayerNode(MotionLayerState *mlStates, int16 motNodeID, bool preserveOrder, TupleDesc tupDesc)
{
	MemoryContext oldCtxt;
	MotionNodeEntry *pEntry;

	if (motNodeID < 1 || motNodeID > mlStates->mneCount)
		elog(ERROR, "invalid motion node ID %d", motNodeID);

	AssertArg(tupDesc != NULL);

	/*
	 * Switch to the Motion Layer's memory-context, so that the motion node
	 * can be reset later.
	 */
	oldCtxt = MemoryContextSwitchTo(mlStates->motion_layer_mctx);

	pEntry = &mlStates->mnEntries[motNodeID - 1];

	Assert(pEntry->valid);

	pEntry->motion_node_id = motNodeID;

	/* Finish up initialization of the motion node entry. */
	pEntry->preserve_order = preserveOrder;
	pEntry->tuple_desc = CreateTupleDescCopy(tupDesc);
	InitSerTupInfo(pEntry->tuple_desc, &pEntry->ser_tup_info);

	if (!preserveOrder)
	{
		/* Create a tuple-store for the motion node's incoming tuples. */
		pEntry->ready_tuples = htfifo_create();
	}
	else
		pEntry->ready_tuples = NULL;


	pEntry->num_stream_ends_recvd = 0;

	/* Initialize statistics counters. */
	pEntry->stat_total_chunks_sent = 0;
	pEntry->stat_total_bytes_sent = 0;
	pEntry->stat_tuple_bytes_sent = 0;
	pEntry->stat_total_sends = 0;
	pEntry->stat_total_recvs = 0;
	pEntry->stat_tuples_available = 0;
	pEntry->stat_tuples_available_hwm = 0;
	pEntry->stat_total_chunks_recvd = 0;
	pEntry->stat_total_bytes_recvd = 0;
	pEntry->stat_tuple_bytes_recvd = 0;

	pEntry->cleanedUp = false;
	pEntry->stopped = false;
	pEntry->moreNetWork = true;


	/* All done!  Go back to caller memory-context. */
	MemoryContextSwitchTo(oldCtxt);
}

void
UpdateMotionExpectedReceivers(MotionLayerState *mlStates, SliceTable *sliceTable)
{
	ExecSlice  *mySlice;
	ExecSlice  *aSlice;
	ListCell   *cell;
	CdbProcess *cdbProc;
	MotionNodeEntry *pEntry;

	mySlice = &sliceTable->slices[sliceTable->localSlice];
	foreach(cell, mySlice->children)
	{
		int			totalNumProcs, activeNumProcs, i;
		int			childId = lfirst_int(cell);

		aSlice = &sliceTable->slices[childId];

		/*
		 * If we're using directed-dispatch we have dummy primary-process
		 * entries, so we count the entries.
		 */
		activeNumProcs = 0;
		totalNumProcs = list_length(aSlice->primaryProcesses);
		for (i = 0; i < totalNumProcs; i++)
		{
			cdbProc = list_nth(aSlice->primaryProcesses, i);
			if (cdbProc)
				activeNumProcs++;
		}

		pEntry = getMotionNodeEntry(mlStates, childId);
		pEntry->num_senders = activeNumProcs;
	}
}

void
SendStopMessage(MotionLayerState *mlStates,
				ChunkTransportState *transportStates,
				int16 motNodeID)
{
	MotionNodeEntry *pEntry = getMotionNodeEntry(mlStates, motNodeID);

	pEntry->stopped = true;
	if (transportStates != NULL && transportStates->doSendStopMessage != NULL)
		transportStates->doSendStopMessage(transportStates, motNodeID);
}

void
CheckAndSendRecordCache(MotionLayerState *mlStates,
						ChunkTransportState *transportStates,
						int16 motNodeID,
						int16 targetRoute)
{
	MotionNodeEntry *pMNEntry;
	TupleChunkListData tcList;
	MemoryContext oldCtxt;
	ChunkTransportStateEntry *pEntry = NULL;
	MotionConn *conn;

	getChunkTransportState(transportStates, motNodeID, &pEntry);

	/*
	 * for broadcast we only mark sent_record_typmod for connection 0 for
	 * efficiency and convenience
	 */
	if (targetRoute == BROADCAST_SEGIDX)
		conn = &pEntry->conns[0];
	else
		conn = &pEntry->conns[targetRoute];

	/*
	 * Analyze tools.  Do not send any thing if this slice is in the bit mask
	 */
	if (gp_motion_slice_noop != 0 && (gp_motion_slice_noop & (1 << currentSliceId)) != 0)
		return;

	/*
	 * Pull up the motion node entry with the node's details.  This includes
	 * details that affect sending, such as whether the motion node needs to
	 * include backup segment-dbs.
	 */
	pMNEntry = getMotionNodeEntry(mlStates, motNodeID);

	if (!ShouldSendRecordCache(conn, &pMNEntry->ser_tup_info))
		return;

#ifdef AMS_VERBOSE_LOGGING
	elog(DEBUG5, "Serializing RecordCache for sending.");
#endif

	/* Create and store the serialized form, and some stats about it. */
	oldCtxt = MemoryContextSwitchTo(mlStates->motion_layer_mctx);

	SerializeRecordCacheIntoChunks(&pMNEntry->ser_tup_info, &tcList, conn);

	MemoryContextSwitchTo(oldCtxt);

#ifdef AMS_VERBOSE_LOGGING
	elog(DEBUG5, "Serialized RecordCache for sending:\n"
		 "\ttarget-route %d \n"
		 "\t%d bytes in serial form\n"
		 "\tbroken into %d chunks",
		 targetRoute,
		 tcList.serialized_data_length,
		 tcList.num_chunks);
#endif

	/* do the send. */
	if (!SendTupleChunkToAMS(mlStates, transportStates, motNodeID, targetRoute, tcList.p_first))
	{
		pMNEntry->stopped = true;
	}
	else
	{
		/* update stats */
		statSendTuple(mlStates, pMNEntry, &tcList);
	}

	/* cleanup */
	clearTCList(&pMNEntry->ser_tup_info.chunkCache, &tcList);

	UpdateSentRecordCache(conn);
}

/*
 * Function:  SendTuple - Sends a portion or whole tuple to the AMS layer.
 */
SendReturnCode
SendTuple(MotionLayerState *mlStates,
		  ChunkTransportState *transportStates,
		  int16 motNodeID,
		  TupleTableSlot *slot,
		  int16 targetRoute)
{
	MotionNodeEntry *pMNEntry;
	TupleChunkListData tcList;
	MemoryContext oldCtxt;
	SendReturnCode rc;

	AssertArg(!TupIsNull(slot));

	/*
	 * Analyze tools.  Do not send any thing if this slice is in the bit mask
	 */
	if (gp_motion_slice_noop != 0 && (gp_motion_slice_noop & (1 << currentSliceId)) != 0)
		return SEND_COMPLETE;

	/*
	 * Pull up the motion node entry with the node's details.  This includes
	 * details that affect sending, such as whether the motion node needs to
	 * include backup segment-dbs.
	 */
	pMNEntry = getMotionNodeEntry(mlStates, motNodeID);

#ifdef AMS_VERBOSE_LOGGING
	elog(DEBUG5, "Serializing HeapTuple for sending.");
#endif

	struct directTransportBuffer b;
	if (targetRoute != BROADCAST_SEGIDX)
		getTransportDirectBuffer(transportStates, motNodeID, targetRoute, &b);

	int			sent = 0;

	/* Create and store the serialized form, and some stats about it. */
	oldCtxt = MemoryContextSwitchTo(mlStates->motion_layer_mctx);

	sent = SerializeTuple(slot, &pMNEntry->ser_tup_info, &b, &tcList, targetRoute);

	MemoryContextSwitchTo(oldCtxt);
	if (sent > 0)
	{
		putTransportDirectBuffer(transportStates, motNodeID, targetRoute, sent);

		/* fill-in tcList fields to update stats */
		tcList.num_chunks = 1;
		tcList.serialized_data_length = sent;

		/* update stats */
		statSendTuple(mlStates, pMNEntry, &tcList);

		return SEND_COMPLETE;
	}
	/* Otherwise fall-through */

#ifdef AMS_VERBOSE_LOGGING
	elog(DEBUG5, "Serialized HeapTuple for sending:\n"
		 "\ttarget-route %d \n"
		 "\t%d bytes in serial form\n"
		 "\tbroken into %d chunks",
		 targetRoute,
		 tcList.serialized_data_length,
		 tcList.num_chunks);
#endif

	/* do the send. */
	if (!SendTupleChunkToAMS(mlStates, transportStates, motNodeID, targetRoute, tcList.p_first))
	{
		pMNEntry->stopped = true;
		rc = STOP_SENDING;
	}
	else
	{
		/* update stats */
		statSendTuple(mlStates, pMNEntry, &tcList);

		rc = SEND_COMPLETE;
	}

	/* cleanup */
	clearTCList(&pMNEntry->ser_tup_info.chunkCache, &tcList);

	return rc;
}

TupleChunkListItem
get_eos_tuplechunklist(void)
{
	return s_eos_chunk_data;
}

/*
 * Sends a token to all peer Motion Nodes, indicating that this motion
 * node has no more tuples to send out.
 */
void
SendEndOfStream(MotionLayerState *mlStates,
				ChunkTransportState *transportStates,
				int motNodeID)
{
	MotionNodeEntry *pMNEntry;

	/*
	 * Pull up the motion node entry with the node's details.  This includes
	 * details that affect sending, such as whether the motion node needs to
	 * include backup segment-dbs.
	 */
	pMNEntry = getMotionNodeEntry(mlStates, motNodeID);

	transportStates->SendEos(transportStates, motNodeID, s_eos_chunk_data);

	/*
	 * We increment our own "stream-ends received" count when we send our own,
	 * as well as when we receive one.
	 */
	pMNEntry->num_stream_ends_recvd++;

	/* We record EOS as if a tuple were sent. */
	statSendEOS(mlStates, pMNEntry);
}

/* An unordered receiver will call this with srcRoute == ANY_ROUTE */
GenericTuple
RecvTupleFrom(MotionLayerState *mlStates,
			  ChunkTransportState *transportStates,
			  int16 motNodeID,
			  int16 srcRoute)
{
	MotionNodeEntry *pMNEntry;
	ChunkSorterEntry *pCSEntry;
	htup_fifo	ReadyList;
	GenericTuple tuple = NULL;

#ifdef AMS_VERBOSE_LOGGING
	elog(DEBUG5, "RecvTupleFrom( motNodeID = %d, srcRoute = %d )", motNodeID, srcRoute);
#endif

	pMNEntry = getMotionNodeEntry(mlStates, motNodeID);

	if (srcRoute == ANY_ROUTE)
	{
		Assert(pMNEntry->preserve_order == 0);
		pCSEntry = NULL;

		ReadyList = pMNEntry->ready_tuples;
	}
	else
	{
		Assert(pMNEntry->preserve_order != 0);

		/*
		 * Pull up the chunk-sorter entry for the specified sender, and get
		 * the tuple-store we should use.
		 */
		pCSEntry = getChunkSorterEntry(mlStates, pMNEntry, srcRoute);
		ReadyList = pCSEntry->ready_tuples;
	}

	for (;;)
	{
		/* Get the next tuple from the FIFO, if one is available. */
		tuple = htfifo_gettuple(ReadyList);
		if (tuple)
			break;

		/*
		 * We need to get more chunks before we have a full tuple to return. Loop
		 * until we get one, or we reach end-of-stream.
		 */
		if ((srcRoute == ANY_ROUTE && !pMNEntry->moreNetWork) ||
			(srcRoute != ANY_ROUTE && pCSEntry->end_of_stream))
		{
			/*
			 * No tuple was available (tuple-store was at EOF), and
			 * end-of-stream has been marked.  No more tuples are going to
			 * show up.
			 */
			break;
		}

		processIncomingChunks(mlStates, transportStates, pMNEntry, motNodeID, srcRoute);
	}

	/* Stats */
	if (tuple)
		statRecvTuple(pMNEntry, pCSEntry);

	return tuple;
}


/*
 * This helper function is the receive-tuple workhorse.  It pulls
 * tuple chunks from the AMS, and pushes them to the chunk-sorter
 * where they can be sorted based on sender, and reconstituted into
 * whole HeapTuples.  Functions like RecvTuple() and RecvTupleFrom()
 * should definitely call this before trying to get a HeapTuple to
 * return.  It can also be called during other operations if that
 * seems like a good idea.  For example, it can be called sometime
 * during send-tuple operations as well.
 */
static void
processIncomingChunks(MotionLayerState *mlStates,
					  ChunkTransportState *transportStates,
					  MotionNodeEntry *pMNEntry,
					  int16 motNodeID,
					  int16 srcRoute)
{
	TupleChunkListItem tcItem,
				tcNext;
	MemoryContext oldCtxt;
	ChunkSorterEntry *chunkSorterEntry;
	ChunkTransportStateEntry *pEntry = NULL;
	MotionConn *conn;

	/* Keep track of processed chunk stats. */
	int			numChunks,
				chunkBytes,
				tupleBytes;

	oldCtxt = MemoryContextSwitchTo(mlStates->motion_layer_mctx);

	/*
	 * Get all of the currently available tuple-chunks, and push each one into
	 * the chunk-sorter.
	 */
	if (srcRoute == ANY_ROUTE)
		tcItem = transportStates->RecvTupleChunkFromAny(transportStates, motNodeID, &srcRoute);
	else
		tcItem = transportStates->RecvTupleChunkFrom(transportStates, motNodeID, srcRoute);

	/* Look up various things related to the sender that we received chunks from. */
	chunkSorterEntry = getChunkSorterEntry(mlStates, pMNEntry, srcRoute);
	getChunkTransportState(transportStates, motNodeID, &pEntry);
	conn = pEntry->conns + srcRoute;

	numChunks = 0;
	chunkBytes = 0;
	tupleBytes = 0;

	while (tcItem != NULL)
	{
		numChunks++;

		/* Detach the current chunk off of the front of the list. */
		tcNext = tcItem->p_next;
		tcItem->p_next = NULL;

		/* Track stats. */
		chunkBytes += tcItem->chunk_length;
		if (tcItem->chunk_length >= TUPLE_CHUNK_HEADER_SIZE)
		{
			tupleBytes += tcItem->chunk_length - TUPLE_CHUNK_HEADER_SIZE;
		}
		else
		{
			elog(ERROR, "Received tuple-chunk of size %u; smaller than"
				 " chunk header size %d!", tcItem->chunk_length,
				 TUPLE_CHUNK_HEADER_SIZE);
		}

		/* Stick the chunk into the sorter. */
		addChunkToSorter(transportStates,
						 pMNEntry,
						 tcItem,
						 motNodeID,
						 chunkSorterEntry,
						 pEntry,
						 conn,
						 srcRoute);

		tcItem = tcNext;
	}

	/* The chunk list we just processed freed-up our rx-buffer space. */
	if (numChunks > 0 && Gp_interconnect_type == INTERCONNECT_TYPE_UDPIFC)
		MlPutRxBufferIFC(transportStates, motNodeID, srcRoute);

	/* Stats */
	statChunksProcessed(mlStates, pMNEntry, numChunks, chunkBytes, tupleBytes);

	MemoryContextSwitchTo(oldCtxt);
}

void
EndMotionLayerNode(MotionLayerState *mlStates, int16 motNodeID, bool flushCommLayer)
{
	MotionNodeEntry *pMNEntry;
	ChunkSorterEntry *pCSEntry;
	int			i;

	pMNEntry = getMotionNodeEntry(mlStates, motNodeID);

#ifdef AMS_VERBOSE_LOGGING
	elog(DEBUG5, "Cleaning up Motion Layer details for motion node %d.",
		 motNodeID);
#endif

	/*
	 * Iterate through all entries in the motion layer's chunk-sort map, to
	 * see if we have gotten end-of-stream from all senders.
	 */
	if (pMNEntry->preserve_order && pMNEntry->ready_tuple_lists != NULL)
	{
		for (i = 0; i < getgpsegmentCount(); i++)
		{
			pCSEntry = &pMNEntry->ready_tuple_lists[i];

			/*
			 * QD should not expect end-of-stream comes from QEs who is not
			 * members of direct dispatch
			 */
			if (!pCSEntry->init)
				continue;

			if (pMNEntry->preserve_order &&
				gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
			{
				/* Print chunk-sorter entry statistics. */
				elog(DEBUG4, "Chunk-sorter entry [route=%d,node=%d] statistics:\n"
					 "\tAvailable Tuples High-Watermark: " UINT64_FORMAT,
					 i, pMNEntry->motion_node_id,
					 pMNEntry->stat_tuples_available_hwm);
			}

			if (!pMNEntry->stopped && !pCSEntry->end_of_stream)
			{
				if (flushCommLayer)
				{
					elog(FATAL, "Motion layer node %d cleanup - did not receive"
						 " end-of-stream from sender %d.", motNodeID, i);

					/*** TODO - get chunks until end-of-stream comes in. ***/
				}
				else
				{
					elog(LOG, "Motion layer node %d cleanup - did not receive"
						 " end-of-stream from sender %d.", motNodeID, i);
				}
			}
			else
			{
				/* End-of-stream is marked for this entry. */

				/*** TODO - do more than just complain! ***/

				if (pCSEntry->chunk_list.num_chunks > 0)
				{
					elog(LOG, "Motion layer node %d cleanup - there are still"
						 " %d chunks enqueued from sender %d.", motNodeID,
						 pCSEntry->chunk_list.num_chunks, i);
				}

				/***
					TODO - Make sure there are no outstanding tuples in the
					tuple-store.
				***/
			}

			/*
			 * Clean up the chunk-sorter entry, then remove it from the hash
			 * table.
			 */
			clearTCList(&pMNEntry->ser_tup_info.chunkCache, &pCSEntry->chunk_list);
			if (pMNEntry->preserve_order)	/* Clean up the tuple-store. */
				htfifo_destroy(pCSEntry->ready_tuples);
		}
	}
	pMNEntry->cleanedUp = true;

	/* Clean up the motion-node entry, then remove it from the hash table. */
	if (gp_log_interconnect >= GPVARS_VERBOSITY_VERBOSE)
	{
		if (pMNEntry->stat_total_bytes_sent > 0)
		{
			elog(LOG, "Interconnect seg%d slice%d sent " UINT64_FORMAT " tuples, "
				 UINT64_FORMAT " total bytes, " UINT64_FORMAT " tuple bytes, "
				 UINT64_FORMAT " chunks.",
				 GpIdentity.segindex,
				 currentSliceId,
				 pMNEntry->stat_total_sends,
				 pMNEntry->stat_total_bytes_sent,
				 pMNEntry->stat_tuple_bytes_sent,
				 pMNEntry->stat_total_chunks_sent
				);
		}
		if (pMNEntry->stat_total_bytes_recvd > 0)
		{
			elog(LOG, "Interconnect seg%d slice%d received from slice%d: " UINT64_FORMAT " tuples, "
				 UINT64_FORMAT " total bytes, " UINT64_FORMAT " tuple bytes, "
				 UINT64_FORMAT " chunks.",
				 GpIdentity.segindex,
				 currentSliceId,
				 motNodeID,
				 pMNEntry->stat_total_recvs,
				 pMNEntry->stat_total_bytes_recvd,
				 pMNEntry->stat_tuple_bytes_recvd,
				 pMNEntry->stat_total_chunks_recvd
				);
		}
	}

	CleanupSerTupInfo(&pMNEntry->ser_tup_info);
	FreeTupleDesc(pMNEntry->tuple_desc);
	if (!pMNEntry->preserve_order)
		htfifo_destroy(pMNEntry->ready_tuples);

	pMNEntry->valid = false;
}

/*
 * Helper function to get the motion node entry for a given ID.  NULL
 * is returned if the ID is unrecognized.
 */
static MotionNodeEntry *
getMotionNodeEntry(MotionLayerState *mlStates, int16 motNodeID)
{
	MotionNodeEntry *pMNEntry = NULL;

	if (motNodeID > mlStates->mneCount ||
		!mlStates->mnEntries[motNodeID - 1].valid)
	{
		ereport(ERROR,
				(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
				 errmsg("interconnect error: Unexpected Motion Node Id: %d",
						motNodeID),
				 errdetail("This means a motion node that wasn't setup is requesting interconnect resources.")));
	}
	else
		pMNEntry = &mlStates->mnEntries[motNodeID - 1];

	if (pMNEntry != NULL)
		Assert(pMNEntry->motion_node_id == motNodeID);

	return pMNEntry;
}

/*
 * Retrieve the chunk-sorter entry for the specified motion-node/source pair.
 * If one doesn't exist, it is created and initialized.
 *
 * It might not seem obvious why the MotionNodeEntry is required as an
 * argument.  It's in there to ensure that only valid motion nodes are
 * represented in the chunk-sorter.  The motion-node's entry is typically
 * retrieved using getMotionNodeEntry() before this function is called.
 */
ChunkSorterEntry *
getChunkSorterEntry(MotionLayerState *mlStates,
					MotionNodeEntry *motNodeEntry,
					int16 srcRoute)
{
	MemoryContext oldCtxt;
	ChunkSorterEntry *chunkSorterEntry = NULL;

	AssertArg(motNodeEntry != NULL);

	Assert(srcRoute >= 0);
	Assert(srcRoute < getgpsegmentCount());

	/* Do we have a sorter initialized ? */
	if (motNodeEntry->ready_tuple_lists != NULL)
	{
		if (motNodeEntry->ready_tuple_lists[srcRoute].init)
			return &motNodeEntry->ready_tuple_lists[srcRoute];
	}

	/* We have to create an entry */
	oldCtxt = MemoryContextSwitchTo(mlStates->motion_layer_mctx);

	if (motNodeEntry->ready_tuple_lists == NULL)
		motNodeEntry->ready_tuple_lists = (ChunkSorterEntry *) palloc0(getgpsegmentCount() * sizeof(ChunkSorterEntry));

	chunkSorterEntry = &motNodeEntry->ready_tuple_lists[srcRoute];

	if (chunkSorterEntry == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("could not allocate entry for tuple chunk sorter")));

	chunkSorterEntry->chunk_list.serialized_data_length = 0;
	chunkSorterEntry->chunk_list.max_chunk_length = Gp_max_tuple_chunk_size;
	chunkSorterEntry->chunk_list.num_chunks = 0;
	chunkSorterEntry->chunk_list.p_first = NULL;
	chunkSorterEntry->chunk_list.p_last = NULL;
	chunkSorterEntry->end_of_stream = false;
	chunkSorterEntry->init = true;

	/*
	 * If motion node is not order-preserving, then all chunk-sorter entries
	 * share one global tuple-store.  If motion node is order- preserving then
	 * there is a tuple-store per sender per motion node.
	 */
	if (motNodeEntry->preserve_order)
	{
		chunkSorterEntry->ready_tuples = htfifo_create();

#ifdef AMS_VERBOSE_LOGGING
		elog(DEBUG5, "Motion node %d is order-preserving.  Creating tuple-store for entry [src=%d,mn=%d].",
			 motNodeEntry->motion_node_id,
			 srcRoute, motNodeEntry->motion_node_id);
#endif
	}
	else
	{
		chunkSorterEntry->ready_tuples = motNodeEntry->ready_tuples;

#ifdef AMS_VERBOSE_LOGGING
		elog(DEBUG5, "Motion node %d is not order-preserving.  Using shared tuple-store for entry [src=%d,mn=%d].",
			 motNodeEntry->motion_node_id,
			 srcRoute, motNodeEntry->motion_node_id);
#endif

		/* Sanity-check: */
		Assert(motNodeEntry->ready_tuples != NULL);
	}

	MemoryContextSwitchTo(oldCtxt);

	Assert(chunkSorterEntry != NULL);
	return chunkSorterEntry;
}

/*
 * Helper function for converting chunks from the receive state (where
 * they point into a share buffer) into the transient state (where they
 * have their own storage). We need to do this if we didn't receive enough
 * information in one chunk to reconstruct a tuple.
 */
static void
materializeChunk(TupleChunkListItem *tcItem)
{
	TupleChunkListItem newItem;

	/*
	 * This chunk needs to be converted from pointing to global receive buffer
	 * store to having its own storage
	 */
	Assert(tcItem != NULL);
	Assert(*tcItem != NULL);

	newItem = repalloc(*tcItem, sizeof(TupleChunkListItemData) + (*tcItem)->chunk_length);

	memcpy(newItem->chunk_data, newItem->inplace, newItem->chunk_length);
	newItem->inplace = NULL;	/* no need to free, someone else owns it */

	*tcItem = newItem;

	return;
}

/*
 * Add another tuple-chunk to the chunk sorter.  If the new chunk
 * completes another HeapTuple, that tuple will be deserialized and
 * stored into a htfifo.  If not, the chunk is added to the
 * appropriate list of chunks.
 */
static void
addChunkToSorter(ChunkTransportState *transportStates,
				 MotionNodeEntry *pMNEntry,
				 TupleChunkListItem tcItem,
				 int16 motNodeID,
				 ChunkSorterEntry *chunkSorterEntry,
				 ChunkTransportStateEntry *pEntry,
				 MotionConn *conn,
				 int16 srcRoute)
{
	TupleChunkType tcType;

	AssertArg(tcItem != NULL);

	/* Look at the chunk's type, to figure out what to do with it. */
	GetChunkType(tcItem, &tcType);

	switch (tcType)
	{
		case TC_WHOLE:
		case TC_EMPTY:
			/* There shouldn't be any partial tuple data in the list! */
			if (chunkSorterEntry->chunk_list.num_chunks != 0)
			{
				ereport(ERROR,
						(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						 errmsg("received TC_WHOLE chunk from [src=%d,mn=%d] after partial tuple data",
								srcRoute, motNodeID)));
			}

			/* Put this chunk into the list, then turn it into a HeapTuple! */
			appendChunkToTCList(&chunkSorterEntry->chunk_list, tcItem);
			reconstructTuple(pMNEntry, chunkSorterEntry, conn->remapper);

			break;

		case TC_PARTIAL_START:

			/* There shouldn't be any partial tuple data in the list! */
			if (chunkSorterEntry->chunk_list.num_chunks != 0)
			{
				ereport(ERROR,
						(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						 errmsg("received TC_PARTIAL_START chunk from [src=%d,mn=%d] after partial tuple data",
								srcRoute, motNodeID)));
			}

			/*
			 * we don't have enough to reconstruct the tuple, we need to copy
			 * the chunk data out of our shared buffer
			 */
			materializeChunk(&tcItem);

			/* Put this chunk into the list. */
			appendChunkToTCList(&chunkSorterEntry->chunk_list, tcItem);

			break;

		case TC_PARTIAL_MID:

			/* There should be partial tuple data in the list. */
			if (chunkSorterEntry->chunk_list.num_chunks <= 0)
			{
				ereport(ERROR,
						(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						 errmsg("received TC_PARTIAL_MID chunk from [src=%d,mn=%d] without any leading tuple data",
								srcRoute, motNodeID)));
			}

			/*
			 * we don't have enough to reconstruct the tuple, we need to copy
			 * the chunk data out of our shared buffer
			 */
			materializeChunk(&tcItem);

			/* Append this chunk to the list. */
			appendChunkToTCList(&chunkSorterEntry->chunk_list, tcItem);

			break;

		case TC_PARTIAL_END:

			/* There should be partial tuple data in the list. */
			if (chunkSorterEntry->chunk_list.num_chunks <= 0)
			{
				ereport(ERROR,
						(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						 errmsg("received TC_PARTIAL_END chunk from [src=%d,mn=%d] without any leading tuple data",
								srcRoute, motNodeID)));
			}

			/* Put this chunk into the list, then turn it into a HeapTuple! */
			appendChunkToTCList(&chunkSorterEntry->chunk_list, tcItem);
			reconstructTuple(pMNEntry, chunkSorterEntry, conn->remapper);

			break;

		case TC_END_OF_STREAM:
#ifdef AMS_VERBOSE_LOGGING
			elog(LOG, "Got end-of-stream. motnode %d route %d", motNodeID, srcRoute);
#endif
			/* There shouldn't be any partial tuple data in the list! */
			if (chunkSorterEntry->chunk_list.num_chunks != 0)
			{
				ereport(ERROR,
						(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						 errmsg("received TC_END_OF_STREAM chunk from [src=%d,mn=%d] after partial tuple data",
								srcRoute, motNodeID)));
			}

			/* Make sure that we haven't already received end-of-stream! */

			if (chunkSorterEntry->end_of_stream)
			{
				ereport(ERROR,
						(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						 errmsg("received end-of-stream chunk from [src=%d,mn=%d] when already marked as at end-of-stream",
								srcRoute, motNodeID)));
			}

			/* Mark the state as "end of stream." */
			chunkSorterEntry->end_of_stream = true;
			pMNEntry->num_stream_ends_recvd++;

			if (pMNEntry->num_stream_ends_recvd == pMNEntry->num_senders)
				pMNEntry->moreNetWork = false;

			/*
			 * Since we received an end-of-stream.	Then we no longer need
			 * read interest in the interconnect.
			 */
			DeregisterReadInterest(transportStates, motNodeID, srcRoute,
								   "end of stream");
			break;

		default:
			ereport(ERROR,
					(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
					 errmsg("received tuple chunk of unrecognized type %d (len %d) from [src=%d,mn=%d]",
							tcType, tcItem->chunk_length, srcRoute, motNodeID)));
	}
}



/*
 * STATISTICS HELPER-FUNCTIONS
 *
 * NOTE: the only fields that are required to be valid are
 * tcList->num_chunks and tcList->serialized_data_length, and
 * SerializeTupleDirect() only fills those fields out.
 */
static void
statSendTuple(MotionLayerState *mlStates, MotionNodeEntry *pMNEntry, TupleChunkList tcList)
{
	int			headerOverhead;

	AssertArg(pMNEntry != NULL);

	headerOverhead = TUPLE_CHUNK_HEADER_SIZE * tcList->num_chunks;

	/* per motion-node stats. */
	pMNEntry->stat_total_sends++;
	pMNEntry->stat_total_chunks_sent += tcList->num_chunks;
	pMNEntry->stat_total_bytes_sent += tcList->serialized_data_length + headerOverhead;
	pMNEntry->stat_tuple_bytes_sent += tcList->serialized_data_length;

	/* Update global motion-layer statistics. */
	mlStates->stat_total_chunks_sent += tcList->num_chunks;

	mlStates->stat_total_bytes_sent +=
		tcList->serialized_data_length + headerOverhead;

	mlStates->stat_tuple_bytes_sent += tcList->serialized_data_length;

}

static void
statSendEOS(MotionLayerState *mlStates, MotionNodeEntry *pMNEntry)
{
	AssertArg(pMNEntry != NULL);

	/* Update motion node statistics. */
	pMNEntry->stat_total_chunks_sent++;
	pMNEntry->stat_total_bytes_sent += TUPLE_CHUNK_HEADER_SIZE;

	/* Update global motion-layer statistics. */
	mlStates->stat_total_chunks_sent++;
	mlStates->stat_total_bytes_sent += TUPLE_CHUNK_HEADER_SIZE;
}

static void
statChunksProcessed(MotionLayerState *mlStates, MotionNodeEntry *pMNEntry, int chunksProcessed, int chunkBytes, int tupleBytes)
{
	AssertArg(chunksProcessed >= 0);
	AssertArg(chunkBytes >= 0);
	AssertArg(tupleBytes >= 0);

	/* Update Global Motion Layer Stats. */
	mlStates->stat_total_chunks_recvd += chunksProcessed;
	mlStates->stat_total_chunkproc_calls++;
	mlStates->stat_total_bytes_recvd += chunkBytes;
	mlStates->stat_tuple_bytes_recvd += tupleBytes;

	/* Update Motion-node stats. */
	pMNEntry->stat_total_chunks_recvd += chunksProcessed;
	pMNEntry->stat_total_bytes_recvd += chunkBytes;
	pMNEntry->stat_tuple_bytes_recvd += tupleBytes;
}

static void
statNewTupleArrived(MotionNodeEntry *pMNEntry, ChunkSorterEntry *pCSEntry)
{
	uint32		tupsAvail;

	AssertArg(pMNEntry != NULL);
	AssertArg(pCSEntry != NULL);

	/*
	 * High-watermarks:  We track the number of tuples available to receive,
	 * but that haven't yet been received.  The high-watermark is recorded.
	 *
	 * Also, if the motion node is order-preserving, we track a per-sender
	 * high-watermark as well.
	 */
	tupsAvail = ++(pMNEntry->stat_tuples_available);
	if (pMNEntry->stat_tuples_available_hwm < tupsAvail)
	{
		/* New high-watermark! */
		pMNEntry->stat_tuples_available_hwm = tupsAvail;
	}
}

static void
statRecvTuple(MotionNodeEntry *pMNEntry, ChunkSorterEntry *pCSEntry)
{
	AssertArg(pMNEntry != NULL);
	AssertArg(pCSEntry != NULL || !pMNEntry->preserve_order);

	/* Count tuples received. */
	pMNEntry->stat_total_recvs++;

	/* Update "tuples available" counts for high watermark stats. */
	pMNEntry->stat_tuples_available--;
}

/*
 * Return true if the record cache should be sent to master
 */
static bool
ShouldSendRecordCache(MotionConn *conn, SerTupInfo *pSerInfo)
{
	return pSerInfo->has_record_types &&
		NextRecordTypmod > 0 &&
		NextRecordTypmod > conn->sent_record_typmod;
}

/*
 * Update the number of sent record types.
 */
static void
UpdateSentRecordCache(MotionConn *conn)
{
	conn->sent_record_typmod = NextRecordTypmod;
}
