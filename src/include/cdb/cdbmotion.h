/*-------------------------------------------------------------------------
 *
 * cdbmotion.h
 *
 * Portions Copyright (c) 2004-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbmotion.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBMOTION_H
#define CDBMOTION_H

#include "access/htup.h"
#include "cdb/cdbselect.h"
#include "cdb/cdbinterconnect.h"
#include "cdb/ml_ipc.h"

/* Define this if you want tons of logs! */
#undef AMS_VERBOSE_LOGGING


typedef enum SendReturnCode
{
	SEND_COMPLETE,
	STOP_SENDING
}	SendReturnCode;

/*
 * Struct describing the direct transmit buffer.  see:
 * getTransportDirectBuffer() (in ic_common.c) and
 * SerializeTupleDirect() (in cdbmotion.c).
 *
 * Simplified somewhat in 4.0 to remove mirror-data.
 */
struct directTransportBuffer
{
	unsigned char		*pri;
	int					prilen;
};

/* Max message size */
extern int Gp_max_tuple_chunk_size;

/* API FUNCTION CALLS */

/* Initialization of motion layer for this query */
extern MotionLayerState *createMotionLayerState(int maxMotNodeID);

/* Initialization of each motion node in execution plan. */
extern void UpdateMotionLayerNode(MotionLayerState *mlStates, int16 motNodeID, bool preserveOrder,
								  TupleDesc tupDesc);

/* Cleanup of each motion node in execution plan (normal termination). */
extern void EndMotionLayerNode(MotionLayerState *mlStates, int16 motNodeID, bool flushCommLayer);

/* Reset the Motion Layer's state between query executions (normal termination
 * or error-cleanup). */
extern void RemoveMotionLayer(MotionLayerState *ml_states);

extern void CheckAndSendRecordCache(MotionLayerState *mlStates,
									ChunkTransportState *transportStates,
									int16 motNodeID,
									int16 targetRoute);

/* non-blocking operation that may perform only part (or none) of the
 * send before returning.  The TupleSendContext is used to help keep track
 * of the send operations status.  The caller of SendTuple() is responsible
 * for continuing to call SendTuple() until the entire tuple has been sent.
 *
 * Failing to do so and calling SendTuple() with a new Tuple before the old
 * one has been sent is a VERY BAD THING to do and will surely cause bad
 * failures.
 *
 * PARAMETERS:
 *	 - mn_info:  Motion node this tuple is being sent from.
 *
 *	 - tupCtxt: tuple data to send, and state of send operation.
 *
 * RETURN:	return codes to indicate result of send. Possible values are:
 *
 *		SEND_COMPLETE - The entire tuple was accepted by the AMS.
 *				Note that the tuple data may still be in the
 *				AMS send-buffers, but as far as the motion node
 *				is concerned the send is done.
 *
 *		STOP_SENDING - Receiver no longer wants to receive from us.
 */
extern SendReturnCode SendTuple(MotionLayerState *mlStates,
								ChunkTransportState *transportStates,
								int16 motNodeID,
		  						TupleTableSlot *slot,
								int16 targetRoute);


/* Send or broadcast an END_OF_STREAM token to the corresponding motion-node
 * on other segments.
 */
void
SendEndOfStream(MotionLayerState       *mlStates,
                ChunkTransportState    *transportStates,
                int                     motNodeID);

/*
 * Receive a tuple from the corresponding motion-node on any query-executor
 * in the process-group.
 *
 * To get an result for unordered receive (we used to provide a separate
 * RecvTuple() function, set the srcRoute to ANY_ROUTE
 *
 * Returns the next tuple, or NULL if end-of-stream was reached.
 */
extern MinimalTuple RecvTupleFrom(MotionLayerState *mlStates,
								  ChunkTransportState *transportStates,
								  int16 motNodeID,
								  int16 srcRoute);

extern void SendStopMessage(MotionLayerState *mlStates,
							ChunkTransportState *transportStates,
							int16 motNodeID);

/* used by ml_ipc to set the number of receivers that the motion node is expecting.
 * This is used by cdbmotion to keep track of when its seen enough EndOfStream
 * messages.
 */
extern void UpdateMotionExpectedReceivers(MotionLayerState *mlStates,
										  struct SliceTable *sliceTable);

/*
 * Return a pointer to the internal "end-of-stream" message
 */
extern TupleChunkListItem get_eos_tuplechunklist(void);

#endif   /* CDBMOTION_H */
