/*-------------------------------------------------------------------------
 * cdbinterconnect.h
 *	  Defines state that is used by both the Motion Layer and IPC Layer.
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbinterconnect.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef CDBINTERCONNECT_H
#define CDBINTERCONNECT_H

#include "libpq/libpq-be.h"
#include "nodes/primnodes.h"
#include "cdb/tupchunklist.h"
#include "access/htup.h"
#include "cdb/htupfifo.h"

#include "cdb/cdbselect.h"
#include "cdb/tupser.h"
#include "cdb/tupchunk.h"
#include "cdb/tupchunklist.h"
#include "cdb/tupleremap.h"

struct CdbProcess;                          /* #include "nodes/execnodes.h" */
struct ExecSlice;                           /* #include "nodes/execnodes.h" */
struct SliceTable;                          /* #include "nodes/execnodes.h" */
struct EState;                              /* #include "nodes/execnodes.h" */
/* TODO: move "src/backend/cdb/motion/ic_proxy_backend.h" into public include folder*/
struct ICProxyBackendContext;

typedef struct icpkthdr
{
	int32		motNodeId;

	/*
	 * three pairs which seem useful for identifying packets.
	 *
	 * MPP-4194:
	 * It turns out that these can cause collisions; but the
	 * high bit (1<<31) of the dstListener port is now used
	 * for disambiguation with mirrors.
	 */
	int32		srcPid;
	int32		srcListenerPort;

	int32		dstPid;
	int32		dstListenerPort;

    int32       sessionId;
    uint32      icId;

    int32       recvSliceIndex;
    int32       sendSliceIndex;
	int32       srcContentId;
	int32		dstContentId;

	/* MPP-6042: add CRC field */
	uint32		crc;

	/* packet specific info */
	int32		flags;
	int32		len;

    /*
     * The usage of seq and extraSeq field
     * a) In a normal DATA packet
     *    seq      -> the data packet sequence number
     *    extraSeq -> not used
     * b) In a normal ACK message (UDPIC_FLAGS_ACK | UDPIC_FLAGS_CAPACITY)
     *    seq      -> the largest seq of the continuously cached packets
     *                sometimes, it is special, for exampke, conn req ack, mismatch ack.
     *    extraSeq -> the largest seq of the consumed packets
     * c) In a start race NAK message (UPDIC_FLAGS_NAK)
     *    seq      -> the seq from the pkt
     *    extraSeq -> the extraSeq from the pkt
     * d) In a DISORDER message (UDPIC_FLAGS_DISORDER)
     *    seq      -> packet sequence number that triggers the disorder message
     *    extraSeq -> the largest seq of the received packets
     * e) In a DUPLICATE message (UDPIC_FLAGS_DUPLICATE)
     *    seq      -> packet sequence number that triggers the duplicate message
     *    extraSeq -> the largest seq of the continuously cached packets
     * f) In a stop messege (UDPIC_FLAGS_STOP | UDPIC_FLAGS_ACK | UDPIC_FLAGS_CAPACITY)
     *    seq      -> the largest seq of the continuously cached packets
     *    extraSeq -> the largest seq of the continuously cached packets
     *
     *
     * NOTE that: EOS/STOP flags are often saved in conn_info structure of a connection.
     *			  It is possible for them to be sent together with other flags.
     *
     */
    uint32      seq;
    uint32      extraSeq;
} icpkthdr;

typedef enum MotionConnState
{
    mcsNull,
	mcsAccepted,
    mcsSetupOutgoingConnection,
	mcsConnecting,
	mcsRecvRegMsg,
	mcsSendRegMsg,
    mcsStarted,
	mcsEosSent
} MotionConnState;

typedef struct ICBuffer ICBuffer;
typedef struct ICBufferLink ICBufferLink;

typedef enum ICBufferListType
{
	ICBufferListType_Primary,
	ICBufferListType_Secondary,
	ICBufferListType_UNDEFINED
}	ICBufferListType;

struct ICBufferLink
{
	ICBufferLink *next;
	ICBufferLink *prev;
};

/*
 * ICBufferList
 * 		ic buffer list data structure.
 *
 * There are two kinds of lists. The first kind of list uses the primary next/prev pointers.
 * And the second kind uses the secondary next/prev pointers.
 */
typedef struct ICBufferList
{
	int		length;
	ICBufferListType type; /* primary or secondary */

	ICBufferLink head;
}	ICBufferList;

#define CONTAINER_OF(ptr, type, member) \
	({ \
		const typeof( ((type *)0)->member ) *__member_ptr = (ptr); \
		(type *)( (char *)__member_ptr - offsetof(type,member) ); \
	})

#define GET_ICBUFFER_FROM_PRIMARY(ptr) CONTAINER_OF(ptr, ICBuffer, primary)
#define GET_ICBUFFER_FROM_SECONDARY(ptr) CONTAINER_OF(ptr, ICBuffer, secondary)

/*
 * ICBuffer
 * 		interconnect buffer data structure.
 *
 * In some cases, an ICBuffer may exists in two lists/queues,
 * thus it has two sets of pointers. For example, an ICBuffer
 * can exist in an unack queue and an expiration queue at the same time.
 *
 * It is important to get the ICBuffer address when we iterate a list of
 * ICBuffers through primary/secondary links. The Macro GET_ICBUFFER_FROM_PRIMARY
 * and GET_ICBUFFER_FROM_SECONDARY are for this purpose.
 *
 */
struct ICBuffer
{
	/* primary next and prev pointers */
	ICBufferLink primary;

	/* secondary next and prev pointers */
	ICBufferLink secondary;

	/* connection that this buffer belongs to */
	MotionConn *conn;

	/*
	 * Three fields for expiration processing
	 *
	 * sentTime - the time this buffer was sent
	 * nRetry   - the number of send retries
	 * unackQueueRingSlot - unack queue ring slot index
	 */
	uint64 sentTime;
	uint32 nRetry;
	int32 unackQueueRingSlot;

	/* real data */
	icpkthdr pkt[0];
};


/*
 * Structure used for keeping track of a pt-to-pt connection between two
 * Cdb Entities (either QE or QD).
 */
struct MotionConn
{
	/* socket file descriptor. */
	int			sockfd;

	/* send side queue for packets to be sent */
	ICBufferList sndQueue;
	int capacity;

	/* seq already sent */
	uint32 sentSeq;

	/* ack of this seq and packets with smaller seqs have been received */
	uint32 receivedAckSeq;

	/* packets with this seq or smaller seqs have been consumed */
	uint32 consumedSeq;

	uint64 rtt;
	uint64 dev;
	uint64 deadlockCheckBeginTime;


	ICBuffer *curBuff;

	/* send side unacked packet queue. Since it is often
	 * accessed at the same time with unack queue ring,
	 * it is protected with unqck queue ring lock.
	 */
	ICBufferList unackQueue;

	/* pointer to the data buffer. */
	uint8	   *pBuff;

	uint16		route;

	/* size of the message in the buffer, if any. */
	int32		msgSize;

	/* position of message inside of buffer, "cursor" pointer */
	uint8	   *msgPos;

	/*
	 * recv bytes: we can have more than one message/message fragment in recv
	 * queue at once
	 */
	int32		recvBytes;

	int			tupleCount;

	/*
	 * false means 1) received a stop message and has handled it. 2) received
	 * EOS message or sent out EOS message 3) received a QueryFinishPending
	 * notify and has handled it.
	 */
	bool		stillActive;
	/*
	 * used both by motion sender and motion receiver
	 *
	 * sender: true means receiver don't need to consume tuples any more, sender
	 * is also responsible to send stop message to its senders.
	 *
	 * receiver: true means have sent out a stop message to its senders. The stop
	 * message might be lost, stopRequested can also tell sender that no more
	 * data needed in the ack message.
	 */
	bool		stopRequested;

    MotionConnState state;

	uint64		wakeup_ms;

	struct icpkthdr		conn_info;

    struct CdbProcess  *cdbProc;
    int			remoteContentId;
    char        remoteHostAndPort[128];	/* Numeric IP addresses should never be longer than about 50 chars, but play it safe */
    char        localHostAndPort[128];

	struct sockaddr_storage peer;		/* Allow for IPv4 or IPv6 */
	socklen_t peer_len;					/* And remember the actual length */

	/* a queue of maximum length Gp_interconnect_queue_depth */
	int			pkt_q_capacity;			/*max capacity of the queue*/
	int			pkt_q_size;				/*number of packets in the queue*/
	int			pkt_q_head;
	int			pkt_q_tail;
	uint8		**pkt_q;

	uint64 stat_total_ack_time;
	uint64 stat_count_acks;
	uint64 stat_max_ack_time;
	uint64 stat_min_ack_time;
	uint64 stat_count_resent;
	uint64 stat_max_resent;
	uint64 stat_count_dropped;

	/*
	 * used by the sender.
	 *
	 * the typmod of last sent record type in current connection,
	 * if the connection is for broadcasting then we only check
	 * and update this attribute on connection 0.
	 */
	int32		 sent_record_typmod;

	/*
	 * used by the receiver.
	 *
	 * all the remap information.
	 */
	TupleRemapper	*remapper;
};

/*
 * Used to organize all of the information for a given motion node.
 */
typedef struct ChunkTransportStateEntry
{
	int         motNodeId;
	bool		valid;

	/* Connection array */
    MotionConn *conns;
	int			numConns;

	/*
	 * used for receiving. to select() from a set of interesting MotionConns
	 * to see when data is ready to be read.  When the incoming connections
	 * are established, read interest is turned on.  It is turned off when an
	 * EOS (End of Stream) message is read.
	 */
	mpp_fd_set  readSet;

	/* highest file descriptor in the readSet. */
	int			highReadSock;

    int         scanStart;

	/* slice table entries */
	struct ExecSlice *sendSlice;
	struct ExecSlice *recvSlice;

	/* setup info */
	int			txfd;
	int			txfd_family;
	unsigned short txport;

	bool		sendingEos;

	/* Statistics info for this motion on the interconnect level */
	uint64 stat_total_ack_time;
	uint64 stat_count_acks;
	uint64 stat_max_ack_time;
	uint64 stat_min_ack_time;
	uint64 stat_count_resent;
	uint64 stat_max_resent;
	uint64 stat_count_dropped;

}	ChunkTransportStateEntry;

/* ChunkTransportState array initial size */
#define CTS_INITIAL_SIZE (10)

/*
 * This structure is used to keep track of partially completed tuples,
 * and tuples that have been completed but have not been consumed by
 * the executor yet.
 */
typedef struct ChunkSorterEntry
{
	bool		init;

	/*
	 * A tuple-chunk list containing the chunks for the currently incomplete
	 * HeapTuple being received.
	 */
	TupleChunkListData chunk_list;

	/*
	 * A FIFO to hold the tuples that have been completed but not yet
	 * retrieved.  This will not be initialized until it is actually needed.
	 */
	htup_fifo	ready_tuples;

	/*
	 * Flag recording whether end-of-stream has been reported from the source.
	 */
	bool		end_of_stream;
}	ChunkSorterEntry;

/* This is the entry data-structure for a motion node. */
typedef struct MotionNodeEntry
{
	/*
	 * First value in entry has to be the key value.  The key is the motion
	 * node ID.
	 */
	int16           motion_node_id;

	/*
	 * Flag specifying whether the order of tuples from each source should be
	 * maintained or preserved.
	 */
	bool            preserve_order;

	/*
	 * Our route-based array of htup_fifos, for the case where we are a merge receive.
	 */
	ChunkSorterEntry *ready_tuple_lists;

	/* The description of tuples that this motion node will be exchanging. */
	TupleDesc       tuple_desc;

	/*
	 * The cached information to perform tuple serialization and
	 * deserialization as quickly as possible.
	 */
	SerTupInfo      ser_tup_info;

	/*
	 * If preserve_order is false, this is used to hold completed tuples that
	 * have not yet been consumed.  If preserve_order is true, this is NULL.
	 */
	htup_fifo       ready_tuples;

	/*
	 * Variable that records the total number of senders to this motion node.
	 * This is expected to always be (number of qExecs).
	 */
	uint32          num_senders;

	/*
	 * Variable that tracks number of senders that have reported end-of-stream
	 * for this motion node.  When the local node sends end-of-stream, that is
	 * also recorded.
	 */
	uint32          num_stream_ends_recvd;

	bool            cleanedUp;
	bool            valid;
	bool            moreNetWork;
	bool            stopped;

	/*
	 * PER-MOTION-NODE STATISTICS
	 */

	uint64          stat_total_chunks_sent; /* Tuple-chunks sent. */
	uint64          stat_total_bytes_sent;  /* Bytes sent, including headers. */
	uint64          stat_tuple_bytes_sent;  /* Bytes of pure tuple-data sent. */

	uint64          stat_total_chunks_recvd;                /* Tuple-chunks received. */
	uint64          stat_total_bytes_recvd; /* Bytes received, including headers. */
	uint64          stat_tuple_bytes_recvd; /* Bytes of pure tuple-data received. */

	uint64          stat_total_sends;               /* Total calls to SendTuple. */

	uint64          stat_total_recvs;               /* Total calls to RecvTuple/etc. */

	uint64          stat_tuples_available;  /* Total tuples awaiting receive. */
	uint64          stat_tuples_available_hwm;              /* High-water-mark of this
		* value. */
}       MotionNodeEntry;


/*=========================================================================
* MOTION LAYER DATA STRUCTURE
*/

typedef struct MotionLayerState
{
	/* The host ID that this segment-database is on. */
	int			host_id;

	/*
	 * Memory context for the whole motion layer.  This is a child context of
	 * the Executor State Context, so the if it fails to get cleaned up the
	 * estate context should free our resources at the end of the query.
	 */
	MemoryContext motion_layer_mctx;

	/*
	 * MOTION NODE STATE - Initialized and used on per-statement basis.
	 */
	int			mneCount;
	MotionNodeEntry *mnEntries;

	/*
	 * GLOBAL MOTION-LAYER STATISTICS
	 */
	uint32		stat_total_chunks_sent; /* Tuple-chunks sent. */
	uint32		stat_total_bytes_sent;	/* Bytes sent, including headers. */
	uint32		stat_tuple_bytes_sent;	/* Bytes of pure tuple-data sent. */

	uint32		stat_total_chunks_recvd;/* Tuple-chunks received. */
	uint32		stat_total_bytes_recvd; /* Bytes received, including headers. */
	uint32		stat_tuple_bytes_recvd; /* Bytes of pure tuple-data received. */

	uint32		stat_total_chunkproc_calls;		/* Calls to processIncomingChunks() */

}	MotionLayerState;

typedef struct ChunkTransportState
{
	/* array of per-motion-node chunk transport state */
	int size;
	ChunkTransportStateEntry *states;

	/* keeps track of if we've "activated" connections via SetupInterconnect(). */
	bool		activated;

	bool		aggressiveRetry;
	
	/* whether we've logged when network timeout happens */
	bool		networkTimeoutIsLogged;

	bool		teardownActive;
	List		*incompleteConns;

	/* slice table stuff. */
	struct SliceTable  *sliceTable;
	int			sliceId;

	/* Estate pointer for this statement */
	struct EState *estate;

	/* Function pointers to our send/receive functions */
	bool (*SendChunk)(struct ChunkTransportState *transportStates, ChunkTransportStateEntry *pEntry, MotionConn *conn, TupleChunkListItem tcItem, int16 motionId);
	TupleChunkListItem (*RecvTupleChunkFrom)(struct ChunkTransportState *transportStates, int16 motNodeID, int16 srcRoute);
	TupleChunkListItem (*RecvTupleChunkFromAny)(struct ChunkTransportState *transportStates, int16 motNodeID, int16 *srcRoute);
	void (*doSendStopMessage)(struct ChunkTransportState *transportStates, int16 motNodeID);
	void (*SendEos)(struct ChunkTransportState *transportStates, int motNodeID, TupleChunkListItem tcItem);

	/* ic_proxy backend context */
	struct ICProxyBackendContext *proxyContext;
} ChunkTransportState;

extern void dumpICBufferList(ICBufferList *list, const char *fname);
extern void dumpUnackQueueRing(const char *fname);
extern void dumpConnections(ChunkTransportStateEntry *pEntry, const char *fname);

#endif   /* CDBINTERCONNECT_H */
