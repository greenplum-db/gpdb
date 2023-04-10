/*-------------------------------------------------------------------------
 * ic_udpifc.c
 *	   Interconnect code specific to UDP transport.
 *
 * Portions Copyright (c) 2005-2011, Greenplum Inc.
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Copyright (c) 2011-2012, EMC Corporation
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/motion/ic_udpifc.c
 *
 *-------------------------------------------------------------------------
 */

#ifdef WIN32
/*
 * Need this to get WSAPoll (poll). And it
 * has to be set before any header from the Win32 API is loaded.
 */
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include "postgres.h"

#include <pthread.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "access/transam.h"
#include "access/xact.h"
#include "common/ip.h"
#include "nodes/execnodes.h"
#include "nodes/pg_list.h"
#include "nodes/print.h"
#include "miscadmin.h"
#include "libpq/libpq-be.h"
#include "port/atomics.h"
#include "port/pg_crc32c.h"
#include "pgstat.h"
#include "postmaster/postmaster.h"
#include "storage/latch.h"
#include "storage/pmsignal.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/faultinjector.h"

#include "cdb/tupchunklist.h"
#include "cdb/ml_ipc.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbdisp.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbicudpfaultinjection.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

/* If we have old platform sdk headers, WSAPoll() might not be there */
#ifndef POLLIN
/* Event flag definitions for WSAPoll(). */

#define POLLRDNORM	0x0100
#define POLLRDBAND	0x0200
#define POLLIN		(POLLRDNORM | POLLRDBAND)
#define POLLPRI		0x0400

#define POLLWRNORM	0x0010
#define POLLOUT		(POLLWRNORM)
#define POLLWRBAND	0x0020

#define POLLERR		0x0001
#define POLLHUP		0x0002
#define POLLNVAL	0x0004

typedef struct pollfd
{

	SOCKET		fd;
	SHORT		events;
	SHORT		revents;

}			WSAPOLLFD, *PWSAPOLLFD, FAR * LPWSAPOLLFD;

__control_entrypoint(DllExport)
WINSOCK_API_LINKAGE
int
			WSAAPI
WSAPoll(
		IN OUT LPWSAPOLLFD fdArray,
		IN ULONG fds,
		IN INT timeout
);
#endif

#define poll WSAPoll

/*
 * Postgres normally uses it's own custom select implementation
 * on Windows, but they haven't implemented execeptfds, which
 * we use here.  So, undef this to use the normal Winsock version
 * for now
 */
#undef select
#endif

#define MAX_TRY (11)
int
			timeoutArray[] =
{
	1,
	1,
	2,
	4,
	8,
	16,
	32,
	64,
	128,
	256,
	512,
	512							/* MAX_TRY */
};
#define TIMEOUT(try) ((try) < MAX_TRY ? (timeoutArray[(try)]) : (timeoutArray[MAX_TRY]))

#define USECS_PER_SECOND 1000000
#define MSECS_PER_SECOND 1000

/* 1/4 sec in msec */
#define RX_THREAD_POLL_TIMEOUT (250)

/*
 * Flags definitions for flag-field of UDP-messages
 *
 * We use bit operations to test these, flags are powers of two only
 */
#define UDPIC_FLAGS_RECEIVER_TO_SENDER  (1)
#define UDPIC_FLAGS_ACK					(2)
#define UDPIC_FLAGS_STOP				(4)
#define UDPIC_FLAGS_EOS					(8)
#define UDPIC_FLAGS_NAK					(16)
#define UDPIC_FLAGS_DISORDER    		(32)
#define UDPIC_FLAGS_DUPLICATE   		(64)
#define UDPIC_FLAGS_CAPACITY    		(128)

#define UDPIC_MIN_BUF_SIZE (128 * 1024)

/*
 * ConnHtabBin
 *
 * A connection hash table bin.
 *
 */
typedef struct ConnHtabBin ConnHtabBin;
struct ConnHtabBin
{
	MotionConn *conn;
	struct ConnHtabBin *next;
};

/*
 * ConnHashTable
 *
 * Connection hash table definition.
 *
 */
typedef struct ConnHashTable ConnHashTable;
struct ConnHashTable
{
	MemoryContext cxt;
	ConnHtabBin **table;
	int			size;
};

#define CONN_HASH_VALUE(icpkt) ((uint32)((((icpkt)->srcPid ^ (icpkt)->dstPid)) + (icpkt)->dstContentId))
#define CONN_HASH_MATCH(a, b) (((a)->motNodeId == (b)->motNodeId && \
								(a)->dstContentId == (b)->dstContentId && \
								(a)->srcContentId == (b)->srcContentId && \
								(a)->recvSliceIndex == (b)->recvSliceIndex && \
								(a)->sendSliceIndex == (b)->sendSliceIndex && \
								(a)->srcPid == (b)->srcPid &&			\
								(a)->dstPid == (b)->dstPid && (a)->icId == (b)->icId))


/*
 * Cursor IC table definition.
 *
 * For cursor case, there may be several concurrent interconnect
 * instances on QD. The table is used to track the status of the
 * instances, which is quite useful for "ACK the past and NAK the future" paradigm.
 *
 */
#define CURSOR_IC_TABLE_SIZE (128)

/*
 * CursorICHistoryEntry
 *
 * The definition of cursor IC history entry.
 */
typedef struct CursorICHistoryEntry CursorICHistoryEntry;
struct CursorICHistoryEntry
{
	/* Interconnect instance id. */
	uint32		icId;

	/* Command id. */
	uint32		cid;

	/*
	 * Interconnect instance status. state 1 (value 1): interconnect is setup
	 * state 0 (value 0): interconnect was torn down.
	 */
	uint8		status;

	/* Next entry. */
	CursorICHistoryEntry *next;
};

/*
 * CursorICHistoryTable
 *
 * Cursor IC history table. It is a small hash table.
 */
typedef struct CursorICHistoryTable CursorICHistoryTable;
struct CursorICHistoryTable
{
	uint32		count;
	CursorICHistoryEntry *table[CURSOR_IC_TABLE_SIZE];
};

/*
 * Synchronization timeout values
 *
 * MAIN_THREAD_COND_TIMEOUT - 1/4 second
 */
#define MAIN_THREAD_COND_TIMEOUT_MS (250)

/*
 *  Used for synchronization between main thread (receiver) and background thread.
 *
 */
typedef struct ThreadWaitingState ThreadWaitingState;
struct ThreadWaitingState
{
	bool		waiting;
	int			waitingNode;
	int			waitingRoute;
	int			reachRoute;

	/* main_thread_waiting_query is needed to disambiguate for cursors */
	int			waitingQuery;
};

/*
 * ReceiveControlInfo
 *
 * The related control information for receiving data packets.
 * Main thread (Receiver) and background thread use the information in
 * this data structure to handle data packets.
 *
 */
typedef struct ReceiveControlInfo ReceiveControlInfo;
struct ReceiveControlInfo
{
	/* Main thread waiting state. */
	ThreadWaitingState mainWaitingState;

	/*
	 * Buffers used to assemble disorder messages at receiver side.
	 */
	icpkthdr   *disorderBuffer;

	/* The last interconnect instance id which is torn down. */
	uint32		lastTornIcId;

	/* Cursor history table. */
	CursorICHistoryTable cursorHistoryTable;
};

/*
 * Main thread (Receiver) and background thread use the information in
 * this data structure to handle data packets.
 */
static ReceiveControlInfo rx_control_info;


/*
 * RxBufferPool
 *
 * Receive thread buffer pool definition. The implementation of
 * receive side buffer pool is different from send side buffer pool.
 * It is because receive side buffer pool needs a ring buffer to
 * easily implement disorder message handling logic.
 */

typedef struct RxBufferPool RxBufferPool;
struct RxBufferPool
{
	/* The max number of buffers we can get from this pool. */
	int			maxCount;

	/* The number of allocated buffers */
	int			count;

	/* The list of free buffers. */
	char	   *freeList;
};

/*
 * The buffer pool used for keeping data packets.
 *
 * maxCount is set to 1 to make sure there is always a buffer
 * for picking packets from OS buffer.
 */
static RxBufferPool rx_buffer_pool = {1, 0, NULL};

/*
 * SendBufferPool
 *
 * The send side buffer pool definition.
 *
 */
typedef struct SendBufferPool SendBufferPool;
struct SendBufferPool
{
	/* The maximal number of buffers sender can use. */
	int			maxCount;

	/* The number of buffers sender already used. */
	int			count;

	/* The free buffer list at the sender side. */
	ICBufferList freeList;
};

/*
 * The sender side buffer pool.
 */
static SendBufferPool snd_buffer_pool;

/*
 * SendControlInfo
 *
 * The related control information for sending data packets and handling acks.
 * Main thread use the information in this data structure to do ack handling
 * and congestion control.
 *
 */
typedef struct SendControlInfo SendControlInfo;
struct SendControlInfo
{
	/* The buffer used for accepting acks */
	icpkthdr   *ackBuffer;

	/* congestion window */
	float		cwnd;

	/* minimal congestion control window */
	float		minCwnd;

	/* slow start threshold */
	float		ssthresh;

};

/*
 * Main thread use the information in this data structure to do ack handling
 * and congestion control.
 */
static SendControlInfo snd_control_info;

/* WaitEventSet for the icudp */
static WaitEventSet *ICWaitSet = NULL;

/*
 * ICGlobalControlInfo
 *
 * Some shared control information that is used by main thread (senders, receivers, or both)
 * and the background thread.
 *
 */
typedef struct ICGlobalControlInfo ICGlobalControlInfo;
struct ICGlobalControlInfo
{
	/* The background thread handle. */
	pthread_t	threadHandle;

	/* Keep the udp socket buffer size used. */
	uint32		socketSendBufferSize;
	uint32		socketRecvBufferSize;

	uint64		lastExpirationCheckTime;
	uint64		lastDeadlockCheckTime;

	/* Used to decide whether to retransmit for capacity based FC. */
	uint64		lastPacketSendTime;

	/* MemoryContext for UDP interconnect. */
	MemoryContext memContext;

	/*
	 * Lock and latch for coordination between main thread and
	 * background thread. It protects the shared data between the two threads
	 * (the connHtab, rx buffer pool and the mainWaitingState etc.).
	 */
	pthread_mutex_t lock;
	Latch		latch;

	/* Am I a sender? */
	bool		isSender;

	/* Flag showing whether the thread is created. */
	bool		threadCreated;

	/* Error number. Actually int but we do not have pg_atomic_int32. */
	pg_atomic_uint32 eno;

	/*
	 * Global connection htab for both sending connections and receiving
	 * connections. Protected by the lock in this data structure.
	 */
	ConnHashTable connHtab;

	/* The connection htab used to cache future packets. */
	ConnHashTable startupCacheHtab;

	/* Used by main thread to ask the background thread to exit. */
	pg_atomic_uint32 shutdown;

	/*
	 * Used by ic thread in the QE to identify the current serving ic instance
	 * and handle the mismatch packets. It is not used by QD because QD may have
	 * cursors, QD may receive packets for open the cursors with lower instance
	 * id, QD use cursorHistoryTable to handle packets mismatch.
	 */
	uint32		ic_instance_id;
};

/*
 * Shared control information that is used by senders, receivers and background thread.
 */
static ICGlobalControlInfo ic_control_info;

/*
 * Macro for unack queue ring, round trip time (RTT) and expiration period (RTO)
 *
 * UNACK_QUEUE_RING_SLOTS_NUM - the number of slots in the unack queue ring.
 *                              this value should be greater than or equal to 2.
 * TIMER_SPAN                 - timer period in us
 * TIMER_CHECKING_PERIOD      - timer checking period in us
 * UNACK_QUEUE_RING_LENGTH    - the whole time span of the unack queue ring
 * DEFAULT_RTT                - default rtt in us.
 * MIN_RTT                    - min rtt in us
 * MAX_RTT                    - max rtt in us
 * RTT_SHIFT_COEFFICIENT      - coefficient for RTT computation
 *
 * DEFAULT_DEV                - default round trip standard deviation
 * MAX_DEV                    - max dev
 * DEV_SHIFT_COEFFICIENT      - coefficient for DEV computation
 *
 * MAX_EXPIRATION_PERIOD      - max expiration period in us
 * MIN_EXPIRATION_PERIOD      - min expiration period in us
 * MAX_TIME_NO_TIMER_CHECKING - max time without checking timer
 * DEADLOCK_CHECKING_TIME     - deadlock checking time
 *
 * MAX_SEQS_IN_DISORDER_ACK   - max number of sequences that can be transmitted in a
 *                              disordered packet ack.
 *
 *
 * Considerations on the settings of the values:
 *
 * TIMER_SPAN and UNACK_QUEUE_RING_SLOTS_NUM define the ring period.
 * Currently, it is UNACK_QUEUE_RING_LENGTH (default 10 seconds).
 *
 * The definition of UNACK_QUEUE_RING_LENGTH is quite related to the size of
 * sender side buffer and the size we may resend in a burst for an expiration event
 * (which may overwhelm switch or OS if it is too large).
 * Thus, we do not want to send too much data in a single expiration event. Here, a
 * relatively large UNACK_QUEUE_RING_SLOTS_NUM value is used to avoid that.
 *
 * If the sender side buffer is X (MB), then on each slot,
 * there are about X/UNACK_QUEUE_RING_SLOTS_NUM. Even we have a very large sender buffer,
 * for example, 100MB, there is about 96M/2000 = 50K per slot.
 * This is fine for the OS (with buffer 2M for each socket generally) and switch.
 *
 * Note that even when the buffers are not evenly distributed in the ring and there are some packet
 * losses, the congestion control mechanism, the disorder and duplicate packet handling logic will
 * assure the number of outstanding buffers (in unack queues) to be not very large.
 *
 * MIN_RTT/MAX_RTT/DEFAULT_RTT/MIN_EXPIRATION_PERIOD/MAX_EXPIRATION_PERIOD gives some heuristic values about
 * the computation of RTT and expiration period. RTT and expiration period (RTO) are not
 * constant for various kinds of hardware and workloads. Thus, they are computed dynamically.
 * But we also want to bound the values of RTT and MAX_EXPIRATION_PERIOD. It is
 * because there are some faults that may make RTT a very abnormal value. Thus, RTT and
 * expiration period are upper and lower bounded.
 *
 * MAX_SEQS_IN_DISORDER_ACK should be smaller than (MIN_PACKET_SIZE - sizeof(icpkthdr))/sizeof(uint32).
 * It is due to the limitation of the ack receive buffer size.
 *
 */
#define UNACK_QUEUE_RING_SLOTS_NUM (2000)
#define TIMER_SPAN (Gp_interconnect_timer_period * 1000ULL)	/* default: 5ms */
#define TIMER_CHECKING_PERIOD (Gp_interconnect_timer_checking_period)	/* default: 20ms */
#define UNACK_QUEUE_RING_LENGTH (UNACK_QUEUE_RING_SLOTS_NUM * TIMER_SPAN)

#define DEFAULT_RTT (Gp_interconnect_default_rtt * 1000)	/* default: 20ms */
#define MIN_RTT (100)			/* 0.1ms */
#define MAX_RTT (200 * 1000)	/* 200ms */
#define RTT_SHIFT_COEFFICIENT (3)	/* RTT_COEFFICIENT 1/8 (0.125) */

#define DEFAULT_DEV (0)
#define MIN_DEV MIN_RTT
#define MAX_DEV MAX_RTT
#define DEV_SHIFT_COEFFICIENT (2)	/* DEV_COEFFICIENT 1/4 (0.25) */

#define MAX_EXPIRATION_PERIOD (1000 * 1000) /* 1s */
#define MIN_EXPIRATION_PERIOD (Gp_interconnect_min_rto * 1000)	/* default: 20ms */

#define MAX_TIME_NO_TIMER_CHECKING (50 * 1000)	/* 50ms */
#define DEADLOCK_CHECKING_TIME  (512 * 1000)	/* 512ms */

#define MAX_SEQS_IN_DISORDER_ACK (4)

/*
 * UnackQueueRing
 *
 * An unacked queue ring is used to decide which packet is expired in constant time.
 *
 * Each slot of the ring represents a fixed time span, for example 1ms, and
 * each slot has a associated buffer list/queue which contains the packets
 * which will expire in the time span.
 *
 * If the current time pointer (time t) points to slot 1,
 * then slot 2 represents the time span from t + 1ms to t + 2ms.
 * When we check whether there are some packets expired, we start from the last
 * current time recorded, and resend all the packets in the queue
 * until we reach the slot that the updated current time points to.
 *
 */
typedef struct UnackQueueRing UnackQueueRing;
struct UnackQueueRing
{
	/* save the current time when we check the time wheel for expiration */
	uint64		currentTime;

	/* the slot index corresponding to current time */
	int			idx;

	/* the number of outstanding packets in unack queue ring */
	int			numOutStanding;

	/*
	 * the number of outstanding packets that use the shared bandwidth in the
	 * congestion window.
	 */
	int			numSharedOutStanding;

	/* time slots */
	ICBufferList slots[UNACK_QUEUE_RING_SLOTS_NUM];
};

/*
 * All connections in a process share this unack queue ring instance.
 */
static UnackQueueRing unack_queue_ring = {0, 0, 0};

static int	ICSenderSocket = -1;
static uint16 ICSenderPort = 0;
static int	ICSenderFamily = 0;

/*
 * AckSendParam
 *
 * The parameters for ack sending.
 */
typedef struct AckSendParam
{
	/* header for the ack */
	icpkthdr	msg;

	/* peer address for the ack */
	struct sockaddr_storage peer;
	socklen_t	peer_len;
} AckSendParam;

/*
 * ICStatistics
 *
 * A structure keeping various statistics about interconnect internal.
 *
 * Note that the statistics for ic are not accurate for multiple cursor case on QD.
 *
 * totalRecvQueueSize        - receive queue size sum when main thread is trying to get a packet.
 * recvQueueSizeCountingTime - counting times when computing totalRecvQueueSize.
 * totalCapacity             - the capacity sum when packets are tried to be sent.
 * capacityCountingTime      - counting times used to compute totalCapacity.
 * totalBuffers              - total buffers available when sending packets.
 * bufferCountingTime        - counting times when compute totalBuffers.
 * activeConnectionsNum      - the number of active connections.
 * retransmits               - the number of packet retransmits.
 * mismatchNum               - the number of mismatched packets received.
 * crcErrors                 - the number of crc errors.
 * sndPktNum                 - the number of packets sent by sender.
 * recvPktNum                - the number of packets received by receiver.
 * disorderedPktNum          - disordered packet number.
 * duplicatedPktNum          - duplicate packet number.
 * recvAckNum                - the number of Acks received.
 * statusQueryMsgNum         - the number of status query messages sent.
 *
 */
typedef struct ICStatistics
{
	uint64		totalRecvQueueSize;
	uint64		recvQueueSizeCountingTime;
	uint64		totalCapacity;
	uint64		capacityCountingTime;
	uint64		totalBuffers;
	uint64		bufferCountingTime;
	uint32		activeConnectionsNum;
	int32		retransmits;
	int32		startupCachedPktNum;
	int32		mismatchNum;
	int32		crcErrors;
	int32		sndPktNum;
	int32		recvPktNum;
	int32		disorderedPktNum;
	int32		duplicatedPktNum;
	int32		recvAckNum;
	int32		statusQueryMsgNum;
} ICStatistics;

/* Statistics for UDP interconnect. */
static ICStatistics ic_statistics;

/*=========================================================================
 * STATIC FUNCTIONS declarations
 */

/* Cursor IC History table related functions. */
static void initCursorICHistoryTable(CursorICHistoryTable *t);
static void addCursorIcEntry(CursorICHistoryTable *t, uint32 icId, uint32 cid);
static void updateCursorIcEntry(CursorICHistoryTable *t, uint32 icId, uint8 status);
static CursorICHistoryEntry *getCursorIcEntry(CursorICHistoryTable *t, uint32 icId);
static void pruneCursorIcEntry(CursorICHistoryTable *t, uint32 icId);
static void purgeCursorIcEntry(CursorICHistoryTable *t);

static void resetMainThreadWaiting(ThreadWaitingState *state);
static void setMainThreadWaiting(ThreadWaitingState *state, int motNodeId, int route, int icId);

/* Background thread error handling functions. */
static void checkRxThreadError(void);
static void setRxThreadError(int eno);
static void resetRxThreadError(void);
static void SendDummyPacket(void);

static void getSockAddr(struct sockaddr_storage *peer, socklen_t *peer_len, const char *listenerAddr, int listenerPort);
static uint32 setUDPSocketBufferSize(int ic_socket, int buffer_type);
static void setupUDPListeningSocket(int *listenerSocketFd, uint16 *listenerPort, int *txFamily);
static ChunkTransportStateEntry *startOutgoingUDPConnections(ChunkTransportState *transportStates,
							ExecSlice *sendSlice,
							int *pOutgoingCount);
static void setupOutgoingUDPConnection(ChunkTransportState *transportStates,
						   ChunkTransportStateEntry *pEntry, MotionConn *conn);

/* Connection hash table functions. */
static bool initConnHashTable(ConnHashTable *ht, MemoryContext ctx);
static bool connAddHash(ConnHashTable *ht, MotionConn *conn);
static MotionConn *findConnByHeader(ConnHashTable *ht, icpkthdr *hdr);
static void destroyConnHashTable(ConnHashTable *ht);

static inline void sendAckWithParam(AckSendParam *param);
static void sendAck(MotionConn *conn, int32 flags, uint32 seq, uint32 extraSeq);
static void sendDisorderAck(MotionConn *conn, uint32 seq, uint32 extraSeq, uint32 lostPktCnt);
static void sendStatusQueryMessage(MotionConn *conn, int fd, uint32 seq);
static inline void sendControlMessage(icpkthdr *pkt, int fd, struct sockaddr *addr, socklen_t peerLen);

static void putRxBufferAndSendAck(MotionConn *conn, AckSendParam *param);
static inline void putRxBufferToFreeList(RxBufferPool *p, icpkthdr *buf);
static inline icpkthdr *getRxBufferFromFreeList(RxBufferPool *p);
static icpkthdr *getRxBuffer(RxBufferPool *p);

/* ICBufferList functions. */
static inline void icBufferListInitHeadLink(ICBufferLink *link);
static inline void icBufferListInit(ICBufferList *list, ICBufferListType type);
static inline bool icBufferListIsHead(ICBufferList *list, ICBufferLink *link);
static inline ICBufferLink *icBufferListFirst(ICBufferList *list);
static inline int icBufferListLength(ICBufferList *list);
static inline ICBuffer *icBufferListDelete(ICBufferList *list, ICBuffer *buf);
static inline ICBuffer *icBufferListPop(ICBufferList *list);
static void icBufferListFree(ICBufferList *list);
static inline ICBuffer *icBufferListAppend(ICBufferList *list, ICBuffer *buf);
static void icBufferListReturn(ICBufferList *list, bool inExpirationQueue);

static ChunkTransportState *SetupUDPIFCInterconnect_Internal(SliceTable *sliceTable);
static inline TupleChunkListItem RecvTupleChunkFromAnyUDPIFC_Internal(ChunkTransportState *transportStates,
									 int16 motNodeID,
									 int16 *srcRoute);
static inline TupleChunkListItem RecvTupleChunkFromUDPIFC_Internal(ChunkTransportState *transportStates,
								  int16 motNodeID,
								  int16 srcRoute);
static void TeardownUDPIFCInterconnect_Internal(ChunkTransportState *transportStates,
									bool hasErrors);

static void freeDisorderedPackets(MotionConn *conn);

static void prepareRxConnForRead(MotionConn *conn);
static TupleChunkListItem RecvTupleChunkFromAnyUDPIFC(ChunkTransportState *transportStates,
							int16 motNodeID,
							int16 *srcRoute);

static TupleChunkListItem RecvTupleChunkFromUDPIFC(ChunkTransportState *transportStates,
						 int16 motNodeID,
						 int16 srcRoute);
static TupleChunkListItem receiveChunksUDPIFC(ChunkTransportState *pTransportStates, ChunkTransportStateEntry *pEntry,
					int16 motNodeID, int16 *srcRoute, MotionConn *conn);

static TupleChunkListItem receiveChunksUDPIFCLoop(ChunkTransportState *pTransportStates, ChunkTransportStateEntry *pEntry,
						int16 *srcRoute, MotionConn *conn, WaitEventSet *waitset, int nevent);

static void SendEosUDPIFC(ChunkTransportState *transportStates,
			  int motNodeID, TupleChunkListItem tcItem);
static bool SendChunkUDPIFC(ChunkTransportState *transportStates,
				ChunkTransportStateEntry *pEntry, MotionConn *conn, TupleChunkListItem tcItem, int16 motionId);

static void doSendStopMessageUDPIFC(ChunkTransportState *transportStates, int16 motNodeID);
static void dispatcherAYT(void);
static void checkQDConnectionAlive(void);


static void *rxThreadFunc(void *arg);

static bool handleMismatch(icpkthdr *pkt, struct sockaddr_storage *peer, int peer_len);
static void handleAckedPacket(MotionConn *ackConn, ICBuffer *buf, uint64 now);
static bool handleAcks(ChunkTransportState *transportStates, ChunkTransportStateEntry *pEntry);
static void handleStopMsgs(ChunkTransportState *transportStates, ChunkTransportStateEntry *pEntry, int16 motionId);
static void handleDisorderPacket(MotionConn *conn, int pos, uint32 tailSeq, icpkthdr *pkt);
static bool handleDataPacket(MotionConn *conn, icpkthdr *pkt, struct sockaddr_storage *peer, socklen_t *peerlen, AckSendParam *param, bool *wakeup_mainthread);
static bool handleAckForDuplicatePkt(MotionConn *conn, icpkthdr *pkt);
static bool handleAckForDisorderPkt(ChunkTransportState *transportStates, ChunkTransportStateEntry *pEntry, MotionConn *conn, icpkthdr *pkt);

static inline void prepareXmit(MotionConn *conn);
static inline void addCRC(icpkthdr *pkt);
static inline bool checkCRC(icpkthdr *pkt);
static void sendBuffers(ChunkTransportState *transportStates, ChunkTransportStateEntry *pEntry, MotionConn *conn);
static void sendOnce(ChunkTransportState *transportStates, ChunkTransportStateEntry *pEntry, ICBuffer *buf, MotionConn *conn);
static inline uint64 computeExpirationPeriod(MotionConn *conn, uint32 retry);

static ICBuffer *getSndBuffer(MotionConn *conn);
static void initSndBufferPool();

static void putIntoUnackQueueRing(UnackQueueRing *uqr, ICBuffer *buf, uint64 expTime, uint64 now);
static void initUnackQueueRing(UnackQueueRing *uqr);

static void checkExpiration(ChunkTransportState *transportStates, ChunkTransportStateEntry *pEntry, MotionConn *triggerConn, uint64 now);
static void checkDeadlock(ChunkTransportStateEntry *pEntry, MotionConn *conn);

static bool cacheFuturePacket(icpkthdr *pkt, struct sockaddr_storage *peer, int peer_len);
static void cleanupStartupCache(void);
static void handleCachedPackets(void);

static uint64 getCurrentTime(void);
static void initMutex(pthread_mutex_t *mutex);

static inline void logPkt(char *prefix, icpkthdr *pkt);
static void aggregateStatistics(ChunkTransportStateEntry *pEntry);

static inline bool pollAcks(ChunkTransportState *transportStates, int fd, int timeout);

static ssize_t sendtoWithRetry(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len, int retry, const char *errDetail);

/* #define TRANSFER_PROTOCOL_STATS */

#ifdef TRANSFER_PROTOCOL_STATS
typedef enum TransProtoEvent TransProtoEvent;
enum TransProtoEvent
{
	TPE_DATA_PKT_SEND,
	TPE_ACK_PKT_QUERY
};

typedef struct TransProtoStatEntry TransProtoStatEntry;
struct TransProtoStatEntry
{
	TransProtoStatEntry *next;

	/* Basic information */
	uint32		time;
	TransProtoEvent event;
	int			dstPid;
	uint32		seq;

	/* more attributes can be added on demand. */

	/*
	 * float			cwnd; int				capacity;
	 */
};

typedef struct TransProtoStats TransProtoStats;
struct TransProtoStats
{
	pthread_mutex_t lock;
	TransProtoStatEntry *head;
	TransProtoStatEntry *tail;
	uint64		count;
	uint64		startTime;
};

static TransProtoStats trans_proto_stats =
{
	PTHREAD_MUTEX_INITIALIZER, NULL, NULL, 0
};

/*
 * initTransProtoStats
 * 		Initialize the transport protocol states data structures.
 */
static void
initTransProtoStats()
{
	pthread_mutex_lock(&trans_proto_stats.lock);

	while (trans_proto_stats.head)
	{
		TransProtoStatEntry *cur = NULL;

		cur = trans_proto_stats.head;
		trans_proto_stats.head = trans_proto_stats.head->next;

		free(cur);
		trans_proto_stats.count--;
	}

	trans_proto_stats.head = NULL;
	trans_proto_stats.tail = NULL;
	trans_proto_stats.count = 0;
	trans_proto_stats.startTime = getCurrentTime();
	pthread_mutex_unlock(&trans_proto_stats.lock);
}

static void
updateStats(TransProtoEvent event, MotionConn *conn, icpkthdr *pkt)
{
	TransProtoStatEntry *new = NULL;

	/* Add to list */
	new = (TransProtoStatEntry *) malloc(sizeof(TransProtoStatEntry));
	if (!new)
		return;

	memset(new, 0, sizeof(*new));

	/* change the list */
	pthread_mutex_lock(&trans_proto_stats.lock);
	if (trans_proto_stats.count == 0)
	{
		/* 1st element */
		trans_proto_stats.head = new;
		trans_proto_stats.tail = new;
	}
	else
	{
		trans_proto_stats.tail->next = new;
		trans_proto_stats.tail = new;
	}
	trans_proto_stats.count++;

	new->time = getCurrentTime() - trans_proto_stats.startTime;
	new->event = event;
	new->dstPid = pkt->dstPid;
	new->seq = pkt->seq;

	/*
	 * Other attributes can be added on demand new->cwnd =
	 * snd_control_info.cwnd; new->capacity = conn->capacity;
	 */

	pthread_mutex_unlock(&trans_proto_stats.lock);
}

static void
dumpTransProtoStats()
{
	char		tmpbuf[32];

	snprintf(tmpbuf, 32, "%d." UINT64_FORMAT "txt", MyProcPid, getCurrentTime());
	FILE	   *ofile = fopen(tmpbuf, "w+");

	pthread_mutex_lock(&trans_proto_stats.lock);
	while (trans_proto_stats.head)
	{
		TransProtoStatEntry *cur = NULL;

		cur = trans_proto_stats.head;
		trans_proto_stats.head = trans_proto_stats.head->next;

		fprintf(ofile, "time %d event %d seq %d destpid %d\n", cur->time, cur->event, cur->seq, cur->dstPid);
		free(cur);
		trans_proto_stats.count--;
	}

	trans_proto_stats.tail = NULL;

	pthread_mutex_unlock(&trans_proto_stats.lock);

	fclose(ofile);
}

#endif							/* TRANSFER_PROTOCOL_STATS */

/*
 * initCursorICHistoryTable
 * 		Initialize cursor ic history table.
 */
static void
initCursorICHistoryTable(CursorICHistoryTable *t)
{
	t->count = 0;
	memset(t->table, 0, sizeof(t->table));
}

/*
 * addCursorIcEntry
 * 		Add an entry to the cursor ic table.
 */
static void
addCursorIcEntry(CursorICHistoryTable *t, uint32 icId, uint32 cid)
{
	MemoryContext old;
	CursorICHistoryEntry *p;
	uint32		index = icId % CURSOR_IC_TABLE_SIZE;

	old = MemoryContextSwitchTo(ic_control_info.memContext);
	p = palloc0(sizeof(struct CursorICHistoryEntry));
	MemoryContextSwitchTo(old);

	p->icId = icId;
	p->cid = cid;
	p->status = 1;
	p->next = t->table[index];
	t->table[index] = p;
	t->count++;

	elog(DEBUG2, "add icid %d cid %d status %d", p->icId, p->cid, p->status);

	return;
}

/*
 * updateCursorIcEntry
 * 		Update the status of the cursor ic entry for a given interconnect instance id.
 *
 * There are two states for an instance of interconnect.
 * 		state 1 (value 1): interconnect is setup
 * 		state 0 (value 0): interconnect was torn down.
 */
static void
updateCursorIcEntry(CursorICHistoryTable *t, uint32 icId, uint8 status)
{
	struct CursorICHistoryEntry *p;
	uint8		index = icId % CURSOR_IC_TABLE_SIZE;

	for (p = t->table[index]; p; p = p->next)
	{
		if (p->icId == icId)
		{
			p->status = status;
			return;
		}
	}
	/* not found */
}

/*
 * getCursorIcEntry
 * 		Get the cursor entry given an interconnect id.
 */
static CursorICHistoryEntry *
getCursorIcEntry(CursorICHistoryTable *t, uint32 icId)
{
	struct CursorICHistoryEntry *p;
	uint8		index = icId % CURSOR_IC_TABLE_SIZE;

	for (p = t->table[index]; p; p = p->next)
	{
		if (p->icId == icId)
		{
			return p;
		}
	}
	/* not found */
	return NULL;
}

/*
 * pruneCursorIcEntry
 * 		Prune entries in the hash table.
 */
static void
pruneCursorIcEntry(CursorICHistoryTable *t, uint32 icId)
{
	uint8		index;

	for (index = 0; index < CURSOR_IC_TABLE_SIZE; index++)
	{
		struct CursorICHistoryEntry *p,
				   *q;

		p = t->table[index];
		q = NULL;
		while (p)
		{
			/* remove an entry if it is older than the prune-point */
			if (p->icId < icId)
			{
				struct CursorICHistoryEntry *trash;

				if (!q)
				{
					t->table[index] = p->next;
				}
				else
				{
					q->next = p->next;
				}

				trash = p;

				/* set up next loop */
				p = trash->next;
				pfree(trash);

				t->count--;
			}
			else
			{
				q = p;
				p = p->next;
			}
		}
	}
}

/*
 * purgeCursorIcEntry
 *		Clean cursor ic history table.
 */
static void
purgeCursorIcEntry(CursorICHistoryTable *t)
{
	uint8		index;

	for (index = 0; index < CURSOR_IC_TABLE_SIZE; index++)
	{
		struct CursorICHistoryEntry *trash;

		while (t->table[index])
		{
			trash = t->table[index];
			t->table[index] = trash->next;

			pfree(trash);
		}
	}
}

/*
 * resetMainThreadWaiting
 * 		Reset main thread waiting state.
 */
static void
resetMainThreadWaiting(ThreadWaitingState *state)
{
	state->waiting = false;
	state->waitingNode = -1;
	state->waitingRoute = ANY_ROUTE;
	state->reachRoute = ANY_ROUTE;
	state->waitingQuery = -1;
}

/*
 * setMainThreadWaiting
 * 		Set main thread waiting state.
 */
static void
setMainThreadWaiting(ThreadWaitingState *state, int motNodeId, int route, int icId)
{
	state->waiting = true;
	state->waitingNode = motNodeId;
	state->waitingRoute = route;
	state->reachRoute = ANY_ROUTE;
	state->waitingQuery = icId;
}

/*
 * checkRxThreadError
 * 		Check whether there was error in the background thread in main thread.
 *
 * 	If error found, report it.
 */
static void
checkRxThreadError()
{
	int eno;

	eno = pg_atomic_read_u32(&ic_control_info.eno);
	if (eno != 0)
	{
		errno = eno;

		ereport(ERROR,
				(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
				 errmsg("interconnect encountered an error"),
				 errdetail("%s: %m", "in receive background thread")));
	}
}

/*
 * setRxThreadError
 * 		Set the error no in background thread.
 *
 * 	Record the error in background thread. Main thread checks the errors periodically.
 * 	If main thread will find it, main thread will handle it.
 */
static void
setRxThreadError(int eno)
{
	uint32 expected = 0;

	/* always let main thread know the error that occurred first. */
	if (pg_atomic_compare_exchange_u32(&ic_control_info.eno, &expected, (uint32) eno))
	{
		write_log("Interconnect error: in background thread, set ic_control_info.eno to %d, rx_buffer_pool.count %d, rx_buffer_pool.maxCount %d", expected, rx_buffer_pool.count, rx_buffer_pool.maxCount);
	}
}

/*
 * resetRxThreadError
 * 		Reset the error no.
 *
 */
static void
resetRxThreadError()
{
	pg_atomic_write_u32(&ic_control_info.eno, 0);
}


/*
 * setupUDPListeningSocket
 * 		Setup udp listening socket.
 */
static void
setupUDPListeningSocket(int *listenerSocketFd, uint16 *listenerPort, int *txFamily)
{
	struct addrinfo 		*addrs = NULL;
	struct addrinfo 		*addr;
	struct addrinfo 		hints;
	int						ret;
	int 					ic_socket = PGINVALID_SOCKET;
	struct sockaddr_storage ic_socket_addr;
	int 					tries = 0;
	struct sockaddr_storage listenerAddr;
	socklen_t 				listenerAddrlen = sizeof(ic_socket_addr);
	uint32					socketSendBufferSize;
	uint32					socketRecvBufferSize;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;	/* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
	hints.ai_protocol = 0;
	hints.ai_addrlen = 0;
	hints.ai_addr = NULL;
	hints.ai_canonname = NULL;
	hints.ai_next = NULL;
	hints.ai_flags |= AI_NUMERICHOST;

#ifdef USE_ASSERT_CHECKING
	if (gp_udpic_network_disable_ipv6)
		hints.ai_family = AF_INET;
#endif

	if (Gp_interconnect_address_type == INTERCONNECT_ADDRESS_TYPE_UNICAST)
	{
		Assert(interconnect_address && strlen(interconnect_address) > 0);
		hints.ai_flags |= AI_NUMERICHOST;
		ereportif(gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG, DEBUG3,
				  (errmsg("getaddrinfo called with unicast address: %s",
						  interconnect_address)));
	}
	else
	{
		Assert(interconnect_address == NULL);
		hints.ai_flags |= AI_PASSIVE;
		ereportif(gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG, DEBUG3,
				  (errmsg("getaddrinfo called with wildcard address")));
	}

	/*
	 * Restrict what IP address we will listen on to just the one that was
	 * used to create this QE session.
	 */
	Assert(interconnect_address && strlen(interconnect_address) > 0);
	ret = pg_getaddrinfo_all(interconnect_address, NULL, &hints, &addrs);
	if (ret || !addrs)
	{
		ereport(LOG,
				(errmsg("could not resolve address for UDP IC socket %s: %s",
						interconnect_address,
						gai_strerror(ret))));
		goto startup_failed;
	}

	/*
	 * On some platforms, pg_getaddrinfo_all() may return multiple addresses
	 * only one of which will actually work (eg, both IPv6 and IPv4 addresses
	 * when kernel will reject IPv6).  Worse, the failure may occur at the
	 * bind() or perhaps even connect() stage.  So we must loop through the
	 * results till we find a working combination. We will generate DEBUG
	 * messages, but no error, for bogus combinations.
	 */
	for (addr = addrs; addr != NULL; addr = addr->ai_next)
	{

#ifdef HAVE_UNIX_SOCKETS
		/* Ignore AF_UNIX sockets, if any are returned. */
		if (addr->ai_family == AF_UNIX)
			continue;
#endif

		ereportif(++tries > 1 && gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG, DEBUG3,
				  errmsg("trying another address for UDP interconnect socket"));

		ic_socket = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (ic_socket == PGINVALID_SOCKET)
		{
			ereportif(gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG, DEBUG3,
					(errcode_for_socket_access(),
						errmsg("could not create UDP interconnect socket: %m")));
			continue;
		}

		/*
		 * Bind the socket to a kernel assigned ephemeral port on the
		 * interconnect_address.
		 */
		if (bind(ic_socket, addr->ai_addr, addr->ai_addrlen) < 0)
		{
			ereportif(gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG, DEBUG3,
					(errcode_for_socket_access(),
						errmsg("could not bind UDP interconnect socket: %m")));
			closesocket(ic_socket);
			ic_socket = PGINVALID_SOCKET;
			continue;
		}

		/* Call getsockname() to eventually obtain the assigned ephemeral port */
		if (getsockname(ic_socket, (struct sockaddr *) &listenerAddr, &listenerAddrlen) < 0)
		{
			ereportif(gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG, DEBUG3,
					(errcode_for_socket_access(),
						errmsg("could not get address of socket for UDP interconnect: %m")));
			closesocket(ic_socket);
			ic_socket = PGINVALID_SOCKET;
			continue;
		}

		/* If we get here, we have a working socket */
		break;
	}

	if (!addr || ic_socket == PGINVALID_SOCKET)
		goto startup_failed;

	/* Memorize the socket fd, kernel assigned port and address family */
	*listenerSocketFd = ic_socket;
	if (listenerAddr.ss_family == AF_INET6)
	{
		*listenerPort = ntohs(((struct sockaddr_in6 *) &listenerAddr)->sin6_port);
		*txFamily = AF_INET6;
	}
	else
	{
		*listenerPort = ntohs(((struct sockaddr_in *) &listenerAddr)->sin_port);
		*txFamily = AF_INET;
	}

	/* Set up socket non-blocking mode */
	if (!pg_set_noblock(ic_socket))
	{
		ereport(LOG,
				(errcode_for_socket_access(),
					errmsg("could not set UDP interconnect socket to nonblocking mode: %m")));
		goto startup_failed;
	}

	/* Set up the socket's send and receive buffer sizes. */
	socketRecvBufferSize = setUDPSocketBufferSize(ic_socket, SO_RCVBUF);
	if (socketRecvBufferSize == -1)
		goto startup_failed;
	ic_control_info.socketRecvBufferSize = socketRecvBufferSize;

	socketSendBufferSize = setUDPSocketBufferSize(ic_socket, SO_SNDBUF);
	if (socketSendBufferSize == -1)
		goto startup_failed;
	ic_control_info.socketSendBufferSize = socketSendBufferSize;

	pg_freeaddrinfo_all(hints.ai_family, addrs);
	return;

startup_failed:
	if (addrs)
		pg_freeaddrinfo_all(hints.ai_family, addrs);
	if (ic_socket != PGINVALID_SOCKET)
		closesocket(ic_socket);
	ereport(ERROR,
			(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
			 errmsg("interconnect error: Could not set up udp interconnect socket: %m")));
}

/*
 * InitMutex
 * 		Initialize mutex.
 */
static void
initMutex(pthread_mutex_t *mutex)
{
	pthread_mutexattr_t m_atts;

	pthread_mutexattr_init(&m_atts);
	pthread_mutexattr_settype(&m_atts, PTHREAD_MUTEX_ERRORCHECK);

	pthread_mutex_init(mutex, &m_atts);
}

/*
 * Set up the udp interconnect pthread signal mask, we don't want to run our signal handlers
 */
static void
ic_set_pthread_sigmasks(sigset_t *old_sigs)
{
#ifndef WIN32
	sigset_t sigs;
	int		 err;

	sigfillset(&sigs);

	err = pthread_sigmask(SIG_BLOCK, &sigs, old_sigs);
	if (err != 0)
		elog(ERROR, "Failed to get pthread signal masks with return value: %d", err);
#else
	(void) old_sigs;
#endif

	return;
}

static void
ic_reset_pthread_sigmasks(sigset_t *sigs)
{
#ifndef WIN32
	int err;

	err = pthread_sigmask(SIG_SETMASK, sigs, NULL);
	if (err != 0)
		elog(ERROR, "Failed to reset pthread signal masks with return value: %d", err);
#else
	(void) sigs;
#endif

	return;
}

/*
 * InitMotionUDPIFC
 * 		Initialize UDP specific comms, and create rx-thread.
 */
void
InitMotionUDPIFC(int *listenerSocketFd, uint16 *listenerPort)
{
	int			pthread_err;
	int			txFamily = -1;

	/* attributes of the thread we're creating */
	pthread_attr_t t_atts;
	sigset_t	   pthread_sigs;
	MemoryContext  old;

#ifdef USE_ASSERT_CHECKING
	set_test_mode();
#endif

	/* Initialize global ic control data. */
	pg_atomic_init_u32(&ic_control_info.eno, 0);
	ic_control_info.isSender = false;
	ic_control_info.socketSendBufferSize = 2 * 1024 * 1024;
	ic_control_info.socketRecvBufferSize = 2 * 1024 * 1024;
	ic_control_info.memContext = AllocSetContextCreate(TopMemoryContext,
													   "UdpInterconnectMemContext",
													   ALLOCSET_DEFAULT_MINSIZE,
													   ALLOCSET_DEFAULT_INITSIZE,
													   ALLOCSET_DEFAULT_MAXSIZE);
	initMutex(&ic_control_info.lock);
	InitLatch(&ic_control_info.latch);
	pg_atomic_init_u32(&ic_control_info.shutdown, 0);
	ic_control_info.threadCreated = false;
	ic_control_info.ic_instance_id = 0;

	old = MemoryContextSwitchTo(ic_control_info.memContext);

	initConnHashTable(&ic_control_info.connHtab, ic_control_info.memContext);
	if (!initConnHashTable(&ic_control_info.startupCacheHtab, NULL))
		ereport(FATAL,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("failed to initialize connection htab for startup cache")));

	/*
	 * setup listening socket and sending socket for Interconnect.
	 */
	setupUDPListeningSocket(listenerSocketFd, listenerPort, &txFamily);
	setupUDPListeningSocket(&ICSenderSocket, &ICSenderPort, &ICSenderFamily);

	/* Initialize receive control data. */
	resetMainThreadWaiting(&rx_control_info.mainWaitingState);

	/* allocate a buffer for sending disorder messages */
	rx_control_info.disorderBuffer = palloc0(MIN_PACKET_SIZE);
	rx_control_info.lastTornIcId = 0;
	initCursorICHistoryTable(&rx_control_info.cursorHistoryTable);

	/* Initialize receive buffer pool */
	rx_buffer_pool.count = 0;
	rx_buffer_pool.maxCount = 1;
	rx_buffer_pool.freeList = NULL;

	/* Initialize send control data */
	snd_control_info.cwnd = 0;
	snd_control_info.minCwnd = 0;
	snd_control_info.ackBuffer = palloc0(MIN_PACKET_SIZE);

	MemoryContextSwitchTo(old);

#ifdef TRANSFER_PROTOCOL_STATS
	initMutex(&trans_proto_stats.lock);
#endif

	/* Start up our rx-thread */

	/*
	 * save ourselves some memory: the defaults for thread stack size are
	 * large (1M+)
	 */
	pthread_attr_init(&t_atts);

	pthread_attr_setstacksize(&t_atts, Max(PTHREAD_STACK_MIN, (128 * 1024)));
	ic_set_pthread_sigmasks(&pthread_sigs);
	pthread_err = pthread_create(&ic_control_info.threadHandle, &t_atts, rxThreadFunc, NULL);
	ic_reset_pthread_sigmasks(&pthread_sigs);

	pthread_attr_destroy(&t_atts);
	if (pthread_err != 0)
	{
		ic_control_info.threadCreated = false;
		ereport(FATAL,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("InitMotionLayerIPC: failed to create thread"),
				 errdetail("pthread_create() failed with err %d", pthread_err)));
	}

	ic_control_info.threadCreated = true;
	return;
}

/*
 * CleanupMotionUDPIFC
 * 		Clean up UDP specific stuff such as cursor ic hash table, thread etc.
 */
void
CleanupMotionUDPIFC(void)
{
	elog(DEBUG2, "udp-ic: telling receiver thread to shutdown.");

	/*
	 * We should not hold any lock when we reach here even when we report
	 * FATAL errors. Just in case, We still release the locks here.
	 */
	pthread_mutex_unlock(&ic_control_info.lock);

	/* Shutdown rx thread. */
	pg_atomic_write_u32(&ic_control_info.shutdown, 1);

	if (ic_control_info.threadCreated)
		pthread_join(ic_control_info.threadHandle, NULL);

	elog(DEBUG2, "udp-ic: receiver thread shutdown.");

	purgeCursorIcEntry(&rx_control_info.cursorHistoryTable);

	destroyConnHashTable(&ic_control_info.connHtab);

	/* background thread exited, we can do the cleanup without locking. */
	cleanupStartupCache();
	destroyConnHashTable(&ic_control_info.startupCacheHtab);

	/* free the disorder buffer */
	pfree(rx_control_info.disorderBuffer);
	rx_control_info.disorderBuffer = NULL;

	/* free the buffer for acks */
	pfree(snd_control_info.ackBuffer);
	snd_control_info.ackBuffer = NULL;

	MemoryContextDelete(ic_control_info.memContext);

	if (ICSenderSocket >= 0)
		closesocket(ICSenderSocket);
	ICSenderSocket = -1;
	ICSenderPort = 0;
	ICSenderFamily = 0;

#ifdef USE_ASSERT_CHECKING

	/*
	 * Check malloc times, in Interconnect part, memory are carefully released
	 * in tear down code (even when error occurred). But if a FATAL error is
	 * reported, tear down code will not be executed. Thus, it is still
	 * possible the malloc times and free times do not match when we reach
	 * here. The process will die in this case, the mismatch does not
	 * introduce issues.
	 */
	if (icudp_malloc_times != 0)
		elog(LOG, "WARNING: malloc times and free times do not match.");
#endif
}

/*
 * initConnHashTable
 * 		Initialize a connection hash table.
 */
static bool
initConnHashTable(ConnHashTable *ht, MemoryContext cxt)
{
	int			i;

	ht->cxt = cxt;
	ht->size = Gp_role == GP_ROLE_DISPATCH ? (getgpsegmentCount() * 2) : ic_htab_size;
	Assert(ht->size > 0);

	if (ht->cxt)
	{
		ht->table = (struct ConnHtabBin **) palloc0(ht->size * sizeof(struct ConnHtabBin *));
	}
	else
	{
		ht->table = (struct ConnHtabBin **) malloc(ht->size * sizeof(struct ConnHtabBin *));
		if (ht->table == NULL)
			return false;
	}

	for (i = 0; i < ht->size; i++)
		ht->table[i] = NULL;

	return true;
}

/*
 * connAddHash
 * 		Add a connection to the hash table
 *
 * Note: we want to add a connection to the hashtable if it isn't
 * already there ... so we just have to check the pointer values -- no
 * need to use CONN_HASH_MATCH() at all!
 */
static bool
connAddHash(ConnHashTable *ht, MotionConn *conn)
{
	uint32		hashcode;
	struct ConnHtabBin *bin,
			   *newbin;
	MemoryContext old = NULL;

	hashcode = CONN_HASH_VALUE(&conn->conn_info) % ht->size;

	/*
	 * check for collision -- if we already have an entry for this connection,
	 * don't add another one.
	 */
	for (bin = ht->table[hashcode]; bin != NULL; bin = bin->next)
	{
		if (bin->conn == conn)
		{
			elog(DEBUG5, "connAddHash(): duplicate ?! node %d route %d", conn->conn_info.motNodeId, conn->route);
			return true;		/* false *only* indicates memory-alloc
								 * failure. */
		}
	}

	if (ht->cxt)
	{
		old = MemoryContextSwitchTo(ht->cxt);
		newbin = (struct ConnHtabBin *) palloc0(sizeof(struct ConnHtabBin));
	}
	else
	{
		newbin = (struct ConnHtabBin *) malloc(sizeof(struct ConnHtabBin));
		if (newbin == NULL)
			return false;
	}

	newbin->conn = conn;
	newbin->next = ht->table[hashcode];
	ht->table[hashcode] = newbin;

	if (ht->cxt)
		MemoryContextSwitchTo(old);

	ic_statistics.activeConnectionsNum++;

	return true;
}

/*
 * connDelHash
 * 		Delete a connection from the hash table
 *
 * Note: we want to remove a connection from the hashtable if it is
 * there ... so we just have to check the pointer values -- no need to
 * use CONN_HASH_MATCH() at all!
 */
static void
connDelHash(ConnHashTable *ht, MotionConn *conn)
{
	uint32		hashcode;
	struct ConnHtabBin *c,
			   *p,
			   *trash;

	hashcode = CONN_HASH_VALUE(&conn->conn_info) % ht->size;

	c = ht->table[hashcode];

	/* find entry */
	p = NULL;
	while (c != NULL)
	{
		/* found ? */
		if (c->conn == conn)
			break;

		p = c;
		c = c->next;
	}

	/* not found ? */
	if (c == NULL)
	{
		return;
	}

	/* found the connection, remove from the chain. */
	trash = c;

	if (p == NULL)
		ht->table[hashcode] = c->next;
	else
		p->next = c->next;

	if (ht->cxt)
		pfree(trash);
	else
		free(trash);

	ic_statistics.activeConnectionsNum--;

	return;
}

/*
 * findConnByHeader
 * 		Find the corresponding connection given a pkt header information.
 *
 * With the new mirroring scheme, the interconnect is no longer involved:
 * we don't have to disambiguate anymore.
 *
 * NOTE: the icpkthdr field dstListenerPort is used for disambiguation.
 * on receivers it may not match the actual port (it may have an extra bit
 * set (1<<31)).
 */
static MotionConn *
findConnByHeader(ConnHashTable *ht, icpkthdr *hdr)
{
	uint32		hashcode;
	struct ConnHtabBin *bin;
	MotionConn *ret = NULL;

	hashcode = CONN_HASH_VALUE(hdr) % ht->size;

	for (bin = ht->table[hashcode]; bin != NULL; bin = bin->next)
	{
		if (CONN_HASH_MATCH(&bin->conn->conn_info, hdr))
		{
			ret = bin->conn;

			if (DEBUG5 >= log_min_messages)
				write_log("findConnByHeader: found. route %d state %d hashcode %d conn %p",
						  ret->route, ret->state, hashcode, ret);

			return ret;
		}
	}

	if (DEBUG5 >= log_min_messages)
		write_log("findConnByHeader: not found! (hdr->srcPid %d "
				  "hdr->srcContentId %d hdr->dstContentId %d hdr->dstPid %d sess(%d:%d) cmd(%d:%d)) hashcode %d",
				  hdr->srcPid, hdr->srcContentId, hdr->dstContentId, hdr->dstPid, hdr->sessionId,
				  gp_session_id, hdr->icId, ic_control_info.ic_instance_id, hashcode);

	return NULL;
}

/*
 * destroyConnHashTable
 * 		Release the connection hash table.
 */
static void
destroyConnHashTable(ConnHashTable *ht)
{
	int			i;

	for (i = 0; i < ht->size; i++)
	{
		struct ConnHtabBin *trash;

		while (ht->table[i] != NULL)
		{
			trash = ht->table[i];
			ht->table[i] = trash->next;

			if (ht->cxt)
				pfree(trash);
			else
				free(trash);
		}
	}

	if (ht->cxt)
		pfree(ht->table);
	else
		free(ht->table);

	ht->table = NULL;
	ht->size = 0;
}

/*
 * sendControlMessage
 * 		Helper function to send a control message.
 */
static inline void
sendControlMessage(icpkthdr *pkt, int fd, struct sockaddr *addr, socklen_t peerLen)
{
	int			n;

#ifdef USE_ASSERT_CHECKING
	if (testmode_inject_fault(gp_udpic_dropacks_percent))
	{
#ifdef AMS_VERBOSE_LOGGING
		write_log("THROW CONTROL MESSAGE with seq %d extraSeq %d srcpid %d despid %d", pkt->seq, pkt->extraSeq, pkt->srcPid, pkt->dstPid);
#endif
		return;
	}
#endif

	/* Add CRC for the control message. */
	if (gp_interconnect_full_crc)
		addCRC(pkt);

	char errDetail[100];
	snprintf(errDetail, sizeof(errDetail), "Send control message: got error with seq %u", pkt->seq);
	/* Retry for infinite times since we have no retransmit mechanism for control message */
	n = sendtoWithRetry(fd, (const char *) pkt, pkt->len, 0, addr, peerLen, -1, errDetail);
	if (n < pkt->len)
		write_log("sendcontrolmessage: got error %d errno %d seq %d", n, errno, pkt->seq);
}

/*
 * setAckSendParam
 * 		Set the ack sending parameters.
 */
static inline void
setAckSendParam(AckSendParam *param, MotionConn *conn, int32 flags, uint32 seq, uint32 extraSeq)
{
	memcpy(&param->msg, (char *) &conn->conn_info, sizeof(icpkthdr));
	param->msg.flags = flags;
	param->msg.seq = seq;
	param->msg.extraSeq = extraSeq;
	param->msg.len = sizeof(icpkthdr);
	param->peer = conn->peer;
	param->peer_len = conn->peer_len;
}

/*
 * sendAckWithParam
 * 		Send acknowledgment to sender.
 */
static inline void
sendAckWithParam(AckSendParam *param)
{
	sendControlMessage(&param->msg, UDP_listenerFd, (struct sockaddr *) &param->peer, param->peer_len);
}

/*
 * sendAck
 * 		Send acknowledgment to sender.
 */
static void
sendAck(MotionConn *conn, int32 flags, uint32 seq, uint32 extraSeq)
{
	icpkthdr	msg;

	memcpy(&msg, (char *) &conn->conn_info, sizeof(msg));

	msg.flags = flags;
	msg.seq = seq;
	msg.extraSeq = extraSeq;
	msg.len = sizeof(icpkthdr);

#ifdef AMS_VERBOSE_LOGGING
	write_log("sendack: flags 0x%x node %d route %d seq %d extraSeq %d",
			  msg.flags, msg.motNodeId, conn->route, msg.seq, msg.extraSeq);
#endif

	sendControlMessage(&msg, UDP_listenerFd, (struct sockaddr *) &conn->peer, conn->peer_len);

}

/*
 * sendDisorderAck
 *		Send a disorder message to the sender.
 *
 * Whenever the receiver detects a disorder packet, it will assemble a disorder message
 * which contains the sequence numbers of the possibly lost packets.
 *
 */
static void
sendDisorderAck(MotionConn *conn, uint32 seq, uint32 extraSeq, uint32 lostPktCnt)
{
	icpkthdr   *disorderBuffer = rx_control_info.disorderBuffer;

	memcpy(disorderBuffer, (char *) &conn->conn_info, sizeof(icpkthdr));

	disorderBuffer->flags |= UDPIC_FLAGS_DISORDER;
	disorderBuffer->seq = seq;
	disorderBuffer->extraSeq = extraSeq;
	disorderBuffer->len = lostPktCnt * sizeof(uint32) + sizeof(icpkthdr);

#ifdef AMS_VERBOSE_LOGGING
	if (!(conn->peer.ss_family == AF_INET || conn->peer.ss_family == AF_INET6))
	{
		write_log("UDP Interconnect bug (in sendDisorderAck): trying to send ack when we don't know where to send to %s", conn->remoteHostAndPort);
	}
#endif

	sendControlMessage(disorderBuffer, UDP_listenerFd, (struct sockaddr *) &conn->peer, conn->peer_len);

}

/*
 * sendStatusQueryMessage
 *		Used by senders to send a status query message for a connection to receivers.
 *
 * When receivers get such a message, they will respond with
 * the connection status (consumed seq, received seq ...).
 */
static void
sendStatusQueryMessage(MotionConn *conn, int fd, uint32 seq)
{
	icpkthdr	msg;

	memcpy(&msg, (char *) &conn->conn_info, sizeof(msg));
	msg.flags = UDPIC_FLAGS_CAPACITY;
	msg.seq = seq;
	msg.extraSeq = 0;
	msg.len = sizeof(msg);

#ifdef TRANSFER_PROTOCOL_STATS
	updateStats(TPE_ACK_PKT_QUERY, conn, &msg);
#endif

	sendControlMessage(&msg, fd, (struct sockaddr *) &conn->peer, conn->peer_len);

}

/*
 * putRxBufferAndSendAck
 * 		Return a buffer and send an acknowledgment.
 *
 *  SHOULD BE CALLED WITH ic_control_info.lock *LOCKED*
 */
static void
putRxBufferAndSendAck(MotionConn *conn, AckSendParam *param)
{
	icpkthdr   *buf;
	uint32		seq;

	buf = (icpkthdr *) conn->pkt_q[conn->pkt_q_head];
	if (buf == NULL)
	{
		pthread_mutex_unlock(&ic_control_info.lock);
		elog(FATAL, "putRxBufferAndSendAck: buffer is NULL");
	}

	seq = buf->seq;

#ifdef AMS_VERBOSE_LOGGING
	elog(LOG, "putRxBufferAndSendAck conn %p pkt [seq %d] for node %d route %d, [head seq] %d queue size %d, queue head %d queue tail %d", conn, seq, buf->motNodeId, conn->route, conn->conn_info.seq - conn->pkt_q_size, conn->pkt_q_size, conn->pkt_q_head, conn->pkt_q_tail);
#endif

	conn->pkt_q[conn->pkt_q_head] = NULL;
	conn->pBuff = NULL;
	conn->pkt_q_head = (conn->pkt_q_head + 1) % conn->pkt_q_capacity;
	conn->pkt_q_size--;

#ifdef AMS_VERBOSE_LOGGING
	elog(LOG, "putRxBufferAndSendAck conn %p pkt [seq %d] for node %d route %d, [head seq] %d queue size %d, queue head %d queue tail %d", conn, seq, buf->motNodeId, conn->route, conn->conn_info.seq - conn->pkt_q_size, conn->pkt_q_size, conn->pkt_q_head, conn->pkt_q_tail);
#endif

	putRxBufferToFreeList(&rx_buffer_pool, buf);

	conn->conn_info.extraSeq = seq;

	/* Send an Ack to the sender. */
	if ((seq % 2 == 0) || (conn->pkt_q_capacity == 1))
	{
		if (param != NULL)
		{
			setAckSendParam(param, conn, UDPIC_FLAGS_ACK | UDPIC_FLAGS_CAPACITY | conn->conn_info.flags, conn->conn_info.seq - 1, seq);
		}
		else
		{
			sendAck(conn, UDPIC_FLAGS_ACK | UDPIC_FLAGS_CAPACITY | conn->conn_info.flags, conn->conn_info.seq - 1, seq);
		}
	}
}

/*
 * MlPutRxBufferIFC
 *
 * The cdbmotion code has discarded our pointer to the motion-conn
 * structure, but has enough info to fully specify it.
 */
void
MlPutRxBufferIFC(ChunkTransportState *transportStates, int motNodeID, int route)
{
	ChunkTransportStateEntry *pEntry = NULL;
	MotionConn *conn = NULL;
	AckSendParam param;

	getChunkTransportState(transportStates, motNodeID, &pEntry);

	conn = pEntry->conns + route;

	memset(&param, 0, sizeof(AckSendParam));

	pthread_mutex_lock(&ic_control_info.lock);

	if (conn->pBuff != NULL)
	{
		putRxBufferAndSendAck(conn, &param);
	}
	else
	{
		pthread_mutex_unlock(&ic_control_info.lock);
		elog(FATAL, "Interconnect error: tried to release a NULL buffer");
	}

	pthread_mutex_unlock(&ic_control_info.lock);

	/*
	 * real ack sending is after lock release to decrease the lock holding
	 * time.
	 */
	if (param.msg.len != 0)
		sendAckWithParam(&param);
}

/*
 * getRxBuffer
 * 		Get a receive buffer.
 *
 * SHOULD BE CALLED WITH ic_control_info.lock *LOCKED*
 *
 * NOTE: This function MUST NOT contain elog or ereport statements.
 * elog is NOT thread-safe.  Developers should instead use something like:
 *
 *	if (DEBUG3 >= log_min_messages)
 *		write_log("my brilliant log statement here.");
 *
 * NOTE: In threads, we cannot use palloc/pfree, because it's not thread safe.
 */
static icpkthdr *
getRxBuffer(RxBufferPool *p)
{
	icpkthdr   *ret = NULL;

#ifdef USE_ASSERT_CHECKING
	if (FINC_HAS_FAULT(FINC_RX_BUF_NULL) &&
		testmode_inject_fault(gp_udpic_fault_inject_percent))
		return NULL;
#endif

	do
	{
		if (p->freeList == NULL)
		{
			if (p->count > p->maxCount)
			{
				if (DEBUG3 >= log_min_messages)
					write_log("Interconnect ran out of rx-buffers count/max %d/%d", p->count, p->maxCount);
				break;
			}

			/* malloc is used for thread safty. */
			ret = (icpkthdr *) malloc(Gp_max_packet_size);

			/*
			 * Note: we return NULL if the malloc() fails -- and the
			 * background thread will set the error. Main thread will check
			 * the error, report it and start teardown.
			 */
			if (ret != NULL)
				p->count++;

			break;
		}

		/* we have buffers available in our freelist */

		ret = getRxBufferFromFreeList(p);

	} while (0);

	return ret;
}

/*
 * putRxBufferToFreeList
 * 		Return a receive buffer to free list
 *
 *  SHOULD BE CALLED WITH ic_control_info.lock *LOCKED*
 */
static inline void
putRxBufferToFreeList(RxBufferPool *p, icpkthdr *buf)
{
	/* return the buffer into the free list. */
	*(char **) buf = p->freeList;
	p->freeList = (char *) buf;
}

/*
 * getRxBufferFromFreeList
 * 		Get a receive buffer from free list
 *
 * SHOULD BE CALLED WITH ic_control_info.lock *LOCKED*
 *
 * NOTE: This function MUST NOT contain elog or ereport statements.
 * elog is NOT thread-safe.  Developers should instead use something like:
 *
 *	if (DEBUG3 >= log_min_messages)
 *		write_log("my brilliant log statement here.");
 *
 * NOTE: In threads, we cannot use palloc/pfree, because it's not thread safe.
 */
static inline icpkthdr *
getRxBufferFromFreeList(RxBufferPool *p)
{
	icpkthdr   *buf = NULL;

	buf = (icpkthdr *) p->freeList;
	p->freeList = *(char **) (p->freeList);
	return buf;
}

/*
 * freeRxBuffer
 * 		Free a receive buffer.
 *
 * NOTE: This function MUST NOT contain elog or ereport statements.
 * elog is NOT thread-safe.  Developers should instead use something like:
 *
 *	if (DEBUG3 >= log_min_messages)
 *		write_log("my brilliant log statement here.");
 *
 * NOTE: In threads, we cannot use palloc/pfree, because it's not thread safe.
 */
static inline void
freeRxBuffer(RxBufferPool *p, icpkthdr *buf)
{
	free(buf);
	p->count--;
}

/*
 * Set UDP IC send/receive socket buffer size.
 *
 * We must carefully size the UDP IC socket's send/receive buffers. If the size
 * is too small, say 128K, and send queue depth and receive queue depth are
 * large, then there might be a lot of dropped/reordered packets. We start
 * trying from a size of 2MB (unless Gp_udp_bufsize_k is specified), and
 * gradually back off to UDPIC_MIN_BUF_SIZE. For a given size setting to be
 * successful, the corresponding UDP kernel buffer size params must be adequate.
 *
 */
static uint32
setUDPSocketBufferSize(int ic_socket, int buffer_type)
{
	int 				expected_size;
	int 				curr_size;
	ACCEPT_TYPE_ARG3 	option_len = 0;

	Assert(buffer_type == SO_SNDBUF || buffer_type == SO_RCVBUF);

	expected_size = (Gp_udp_bufsize_k ? Gp_udp_bufsize_k * 1024 : 2048 * 1024);

	curr_size = expected_size;
	option_len = sizeof(curr_size);
	while (setsockopt(ic_socket, SOL_SOCKET, buffer_type, (const char *) &curr_size, option_len) < 0)
	{
		ereportif(gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG, DEBUG3,
				  (errmsg("UDP-IC: setsockopt %s failed to set buffer size = %d bytes: %m",
						  buffer_type == SO_SNDBUF ? "send": "receive",
						  curr_size)));
		curr_size = curr_size >> 1;
		if (curr_size < UDPIC_MIN_BUF_SIZE)
			return -1;
	}

	ereportif(gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG, DEBUG3,
			  (errmsg("UDP-IC: socket %s current buffer size = %d bytes",
					  buffer_type == SO_SNDBUF ? "send": "receive",
					  curr_size)));

	return curr_size;
}

#if defined(USE_ASSERT_CHECKING) || defined(AMS_VERBOSE_LOGGING)

/*
 * icBufferListLog
 * 		Log the buffer list.
 */
static void
icBufferListLog(ICBufferList *list)
{
	write_log("Length %d, type %d headptr %p", list->length, list->type, &list->head);

	ICBufferLink *bufLink = list->head.next;

	int			len = list->length;
	int			i = 0;

	while (bufLink != &list->head && len > 0)
	{
		ICBuffer   *buf = (list->type == ICBufferListType_Primary ? GET_ICBUFFER_FROM_PRIMARY(bufLink)
						   : GET_ICBUFFER_FROM_SECONDARY(bufLink));

		write_log("Node %d, linkptr %p", i++, bufLink);
		logPkt("from list", buf->pkt);
		bufLink = bufLink->next;
		len--;
	}
}
#endif

#ifdef USE_ASSERT_CHECKING
/*
 * icBufferListCheck
 * 		Buffer list sanity check.
 */
static void
icBufferListCheck(char *prefix, ICBufferList *list)
{
	if (list == NULL)
	{
		write_log("ICBufferList ERROR %s: NULL list", prefix);
		goto error;
	}
	if (list->length < 0)
	{
		write_log("ICBufferList ERROR %s: list length %d < 0 ", prefix, list->length);
		goto error;
	}

	if (list->length == 0 && (list->head.prev != list->head.next && list->head.prev != &list->head))
	{
		write_log("ICBufferList ERROR %s: length is 0, &list->head %p, prev %p, next %p", prefix, &list->head, list->head.prev, list->head.next);
		icBufferListLog(list);
		goto error;
	}

	int			len = list->length;

	ICBufferLink *link = list->head.next;

	while (len > 0)
	{
		link = link->next;
		len--;
	}

	if (link != &list->head)
	{
		write_log("ICBufferList ERROR: %s len %d", prefix, list->length);
		icBufferListLog(list);
		goto error;
	}

	return;

error:
	write_log("wait for 120s and then abort.");
	pg_usleep(120000000);
	abort();
}
#endif

/*
 * icBufferListInitHeadLink
 * 		Initialize the pointers in the head link to point to itself.
 */
static inline void
icBufferListInitHeadLink(ICBufferLink *link)
{
	link->next = link->prev = link;
}

/*
 * icBufferListInit
 * 		Initialize the buffer list with the given type.
 */
static inline void
icBufferListInit(ICBufferList *list, ICBufferListType type)
{
	list->type = type;
	list->length = 0;

	icBufferListInitHeadLink(&list->head);

#ifdef USE_ASSERT_CHECKING
	icBufferListCheck("icBufferListInit", list);
#endif
}

/*
 * icBufferListIsHead
 * 		Return whether the given link is the head link of the list.
 *
 * 	This function is often used as the end condition of an iteration of the list.
 */
static inline bool
icBufferListIsHead(ICBufferList *list, ICBufferLink *link)
{
#ifdef USE_ASSERT_CHECKING
	icBufferListCheck("icBufferListIsHead", list);
#endif
	return (link == &list->head);
}

/*
 * icBufferListFirst
 * 		Return the first link after the head link.
 *
 * 	Note that the head link is a pseudo link used to only to ease the operations of the link list.
 * 	If the list only contains the head link, this function will return the head link.
 */
static inline ICBufferLink *
icBufferListFirst(ICBufferList *list)
{
#ifdef USE_ASSERT_CHECKING
	icBufferListCheck("icBufferListFirst", list);
#endif
	return list->head.next;
}

/*
 * icBufferListLength
 * 		Get the list length.
 */
static inline int
icBufferListLength(ICBufferList *list)
{
#ifdef USE_ASSERT_CHECKING
	icBufferListCheck("icBufferListLength", list);
#endif
	return list->length;
}

/*
 * icBufferListDelete
 *		Remove an buffer from the buffer list and return the buffer.
 */
static inline ICBuffer *
icBufferListDelete(ICBufferList *list, ICBuffer *buf)
{
#ifdef USE_ASSERT_CHECKING
	icBufferListCheck("icBufferListDelete", list);
#endif

	ICBufferLink *bufLink = NULL;

	bufLink = (list->type == ICBufferListType_Primary ? &buf->primary : &buf->secondary);

	bufLink->prev->next = bufLink->next;
	bufLink->next->prev = bufLink->prev;

	list->length--;

	return buf;
}

/*
 * icBufferListPop
 * 		Remove the head buffer from the list.
 */
static inline ICBuffer *
icBufferListPop(ICBufferList *list)
{
	ICBuffer   *buf = NULL;
	ICBufferLink *bufLink = NULL;

#ifdef USE_ASSERT_CHECKING
	icBufferListCheck("icBufferListPop", list);
#endif

	if (list->length == 0)
		return NULL;

	bufLink = icBufferListFirst(list);
	buf = (list->type == ICBufferListType_Primary ? GET_ICBUFFER_FROM_PRIMARY(bufLink)
		   : GET_ICBUFFER_FROM_SECONDARY(bufLink));

	bufLink->prev->next = bufLink->next;
	bufLink->next->prev = bufLink->prev;

	list->length--;

	return buf;
}

/*
 * icBufferListFree
 * 		Free all the buffers in the list.
 */
static void
icBufferListFree(ICBufferList *list)
{
	ICBuffer   *buf = NULL;

#ifdef USE_ASSERT_CHECKING
	icBufferListCheck("icBufferListFree", list);
#endif

	while ((buf = icBufferListPop(list)) != NULL)
		pfree(buf);

}

/*
 * icBufferListAppend
 * 		Append a buffer to a list.
 */
static inline ICBuffer *
icBufferListAppend(ICBufferList *list, ICBuffer *buf)
{
#ifdef USE_ASSERT_CHECKING
	icBufferListCheck("icBufferListAppend", list);
#endif

	ICBufferLink *bufLink = NULL;

	bufLink = (list->type == ICBufferListType_Primary ? &buf->primary : &buf->secondary);

	bufLink->prev = list->head.prev;
	bufLink->next = &list->head;

	list->head.prev->next = bufLink;
	list->head.prev = bufLink;

	list->length++;

	return buf;
}

/*
 * icBufferListReturn
 * 		Return the buffers in the list to the free buffer list.
 *
 * If the buf is also in an expiration queue, we also need to remove it from the expiration queue.
 *
 */
static void
icBufferListReturn(ICBufferList *list, bool inExpirationQueue)
{
#ifdef USE_ASSERT_CHECKING
	icBufferListCheck("icBufferListReturn", list);
#endif
	ICBuffer   *buf = NULL;

	while ((buf = icBufferListPop(list)) != NULL)
	{
		if (inExpirationQueue)	/* the buf is in also in the expiration queue */
		{
			icBufferListDelete(&unack_queue_ring.slots[buf->unackQueueRingSlot], buf);
			unack_queue_ring.numOutStanding--;
			if (icBufferListLength(list) >= 1)
				unack_queue_ring.numSharedOutStanding--;
		}

		icBufferListAppend(&snd_buffer_pool.freeList, buf);
	}
}

/*
 * initUnackQueueRing
 *		Initialize an unack queue ring.
 *
 *	Align current time to a slot boundary and set current slot index (time pointer) to 0.
 */
static void
initUnackQueueRing(UnackQueueRing *uqr)
{
	int			i = 0;

	uqr->currentTime = 0;
	uqr->idx = 0;
	uqr->numOutStanding = 0;
	uqr->numSharedOutStanding = 0;

	for (; i < UNACK_QUEUE_RING_SLOTS_NUM; i++)
	{
		icBufferListInit(&uqr->slots[i], ICBufferListType_Secondary);
	}
}

/*
 * computeExpirationPeriod
 * 		Compute expiration period according to the connection information.
 *
 * Considerations on expiration period computation:
 *
 * RTT is dynamically computed, and expiration period is based on RTT values.
 * We cannot simply use RTT as the expiration value, since real workload does
 * not always have a stable RTT. A small constant value is multiplied to the RTT value
 * to make the resending logic insensitive to the frequent small changes of RTT.
 *
 */
static inline uint64
computeExpirationPeriod(MotionConn *conn, uint32 retry)
{
	/*
	 * In fault injection mode, we often use DEFAULT_RTT, because the
	 * intentional large percent of packet/ack losses will make the RTT too
	 * large. This will lead to a slow retransmit speed. In real hardware
	 * environment/workload, we do not expect such a packet loss pattern.
	 */
#ifdef USE_ASSERT_CHECKING
	if (udp_testmode)
	{
		return DEFAULT_RTT;
	}
	else
#endif
	{
		uint32		factor = (retry <= 12 ? retry : 12);

		return Max(MIN_EXPIRATION_PERIOD, Min(MAX_EXPIRATION_PERIOD, (conn->rtt + (conn->dev << 2)) << (factor)));
	}
}

/*
 * initSndBufferPool
 * 		Initialize the send buffer pool.
 *
 * The initial maxCount is set to 1 for gp_interconnect_snd_queue_depth = 1 case,
 * then there is at least an extra free buffer to send for that case.
 */
static void
initSndBufferPool(SendBufferPool *p)
{
	icBufferListInit(&p->freeList, ICBufferListType_Primary);
	p->count = 0;
	p->maxCount = (Gp_interconnect_snd_queue_depth == 1 ? 1 : 0);
}

/*
 * cleanSndBufferPool
 * 		Clean the send buffer pool.
 */
static inline void
cleanSndBufferPool(SendBufferPool *p)
{
	icBufferListFree(&p->freeList);
	p->count = 0;
	p->maxCount = 0;
}

/*
 * getSndBuffer
 * 		Get a send buffer for a connection.
 *
 *  Different flow control mechanisms use different buffer management policies.
 *  Capacity based flow control uses per-connection buffer policy and Loss based
 *  flow control uses shared buffer policy.
 *
 * 	Return NULL when no free buffer available.
 */
static ICBuffer *
getSndBuffer(MotionConn *conn)
{
	ICBuffer   *ret = NULL;

	ic_statistics.totalBuffers += (icBufferListLength(&snd_buffer_pool.freeList) + snd_buffer_pool.maxCount - snd_buffer_pool.count);
	ic_statistics.bufferCountingTime++;

	/* Capacity based flow control does not use shared buffers */
	if (Gp_interconnect_fc_method == INTERCONNECT_FC_METHOD_CAPACITY)
	{
		Assert(icBufferListLength(&conn->unackQueue) + icBufferListLength(&conn->sndQueue) <= Gp_interconnect_snd_queue_depth);
		if (icBufferListLength(&conn->unackQueue) + icBufferListLength(&conn->sndQueue) >= Gp_interconnect_snd_queue_depth)
			return NULL;
	}

	if (icBufferListLength(&snd_buffer_pool.freeList) > 0)
	{
		return icBufferListPop(&snd_buffer_pool.freeList);
	}
	else
	{
		if (snd_buffer_pool.count < snd_buffer_pool.maxCount)
		{
			MemoryContext oldContext;

			oldContext = MemoryContextSwitchTo(InterconnectContext);

			ret = (ICBuffer *) palloc0(Gp_max_packet_size + sizeof(ICBuffer));
			snd_buffer_pool.count++;
			ret->conn = NULL;
			ret->nRetry = 0;
			icBufferListInitHeadLink(&ret->primary);
			icBufferListInitHeadLink(&ret->secondary);
			ret->unackQueueRingSlot = 0;

			MemoryContextSwitchTo(oldContext);
		}
		else
		{
			return NULL;
		}
	}

	return ret;
}


/*
 * startOutgoingUDPConnections
 * 		Used to initially kick-off any outgoing connections for mySlice.
 *
 * This should not be called for root slices (i.e. QD ones) since they don't
 * ever have outgoing connections.
 *
 * PARAMETERS
 *
 *	 sendSlice	- Slice that this process is a member of.
 *
 * RETURNS
 *	 Initialized ChunkTransportState for the Sending Motion Node Id.
 */
static ChunkTransportStateEntry *
startOutgoingUDPConnections(ChunkTransportState *transportStates,
							ExecSlice *sendSlice,
							int *pOutgoingCount)
{
	ChunkTransportStateEntry *pEntry;
	MotionConn *conn;
	ListCell   *cell;
	ExecSlice  *recvSlice;
	CdbProcess *cdbProc;
	int			i;

	*pOutgoingCount = 0;

	recvSlice = &transportStates->sliceTable->slices[sendSlice->parentIndex];

	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		elog(DEBUG1, "Interconnect seg%d slice%d setting up sending motion node",
			 GpIdentity.segindex, sendSlice->sliceIndex);

	pEntry = createChunkTransportState(transportStates,
									   sendSlice,
									   recvSlice,
									   list_length(recvSlice->primaryProcesses));

	Assert(pEntry && pEntry->valid);

	/*
	 * Setup a MotionConn entry for each of our outbound connections. Request
	 * a connection to each receiving backend's listening port. NB: Some
	 * mirrors could be down & have no CdbProcess entry.
	 */
	conn = pEntry->conns;

	i = 0;
	foreach(cell, recvSlice->primaryProcesses)
	{
		cdbProc = (CdbProcess *) lfirst(cell);
		if (cdbProc)
		{
			conn->cdbProc = cdbProc;
			icBufferListInit(&conn->sndQueue, ICBufferListType_Primary);
			icBufferListInit(&conn->unackQueue, ICBufferListType_Primary);
			conn->capacity = Gp_interconnect_queue_depth;

			/* send buffer pool must be initialized before this. */
			snd_buffer_pool.maxCount += Gp_interconnect_snd_queue_depth;
			snd_control_info.cwnd += 1;
			conn->curBuff = getSndBuffer(conn);

			/* should have at least one buffer for each connection */
			Assert(conn->curBuff != NULL);

			conn->rtt = DEFAULT_RTT;
			conn->dev = DEFAULT_DEV;
			conn->deadlockCheckBeginTime = 0;
			conn->tupleCount = 0;
			conn->msgSize = sizeof(conn->conn_info);
			conn->sentSeq = 0;
			conn->receivedAckSeq = 0;
			conn->consumedSeq = 0;
			conn->pBuff = (uint8 *) conn->curBuff->pkt;
			conn->state = mcsSetupOutgoingConnection;
			conn->route = i++;

			(*pOutgoingCount)++;
		}

		conn++;
	}

	pEntry->txfd = ICSenderSocket;
	pEntry->txport = ICSenderPort;
	pEntry->txfd_family = ICSenderFamily;

	return pEntry;

}


/*
 * getSockAddr
 * 		Convert IP addr and port to sockaddr
 */
static void
getSockAddr(struct sockaddr_storage *peer, socklen_t *peer_len, const char *listenerAddr, int listenerPort)
{
	int			ret;
	char		portNumberStr[32];
	char	   *service;
	struct addrinfo *addrs = NULL;
	struct addrinfo hint;

	/*
	 * Get socketaddr to connect to.
	 */

	/* Initialize hint structure */
	MemSet(&hint, 0, sizeof(hint));
	hint.ai_socktype = SOCK_DGRAM;	/* UDP */
	hint.ai_family = AF_UNSPEC; /* Allow for any family (v4, v6, even unix in
								 * the future)  */
#ifdef AI_NUMERICSERV
	hint.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;	/* Never do name
														 * resolution */
#else
	hint.ai_flags = AI_NUMERICHOST; /* Never do name resolution */
#endif

	snprintf(portNumberStr, sizeof(portNumberStr), "%d", listenerPort);
	service = portNumberStr;

	ret = pg_getaddrinfo_all(listenerAddr, service, &hint, &addrs);
	if (ret || !addrs)
	{
		if (addrs)
			pg_freeaddrinfo_all(hint.ai_family, addrs);

		ereport(ERROR,
				(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
				 errmsg("interconnect error: Could not parse remote listener address: '%s' port '%d': %s",
						listenerAddr, listenerPort, gai_strerror(ret)),
				 errdetail("getaddrinfo() unable to parse address: '%s'",
						   listenerAddr)));
	}

	/*
	 * Since we aren't using name resolution, getaddrinfo will return only 1
	 * entry
	 */

	elog(DEBUG1, "GetSockAddr socket ai_family %d ai_socktype %d ai_protocol %d for %s ", addrs->ai_family, addrs->ai_socktype, addrs->ai_protocol, listenerAddr);
	memset(peer, 0, sizeof(struct sockaddr_storage));
	memcpy(peer, addrs->ai_addr, addrs->ai_addrlen);
	*peer_len = addrs->ai_addrlen;

	pg_freeaddrinfo_all(addrs->ai_family, addrs);
}

/*
 * setupOutgoingUDPConnection
 *		Setup outgoing UDP connection.
 */
void
setupOutgoingUDPConnection(ChunkTransportState *transportStates, ChunkTransportStateEntry *pEntry, MotionConn *conn)
{
	CdbProcess *cdbProc = conn->cdbProc;
	SliceTable *sliceTbl = transportStates->sliceTable;

	Assert(conn->state == mcsSetupOutgoingConnection);
	Assert(conn->cdbProc);

	conn->wakeup_ms = 0;
	conn->remoteContentId = cdbProc->contentid;
	conn->stat_min_ack_time = ~((uint64) 0);

	/* Save the information for the error message if getaddrinfo fails */
	if (strchr(cdbProc->listenerAddr, ':') != 0)
		snprintf(conn->remoteHostAndPort, sizeof(conn->remoteHostAndPort),
				 "[%s]:%d", cdbProc->listenerAddr, cdbProc->listenerPort);
	else
		snprintf(conn->remoteHostAndPort, sizeof(conn->remoteHostAndPort),
				 "%s:%d", cdbProc->listenerAddr, cdbProc->listenerPort);

	/*
	 * Get socketaddr to connect to.
	 */
	getSockAddr(&conn->peer, &conn->peer_len, cdbProc->listenerAddr, cdbProc->listenerPort);

	/* Save the destination IP address */
	format_sockaddr(&conn->peer, conn->remoteHostAndPort,
				   sizeof(conn->remoteHostAndPort));

	Assert(conn->peer.ss_family == AF_INET || conn->peer.ss_family == AF_INET6);

	{
#ifdef USE_ASSERT_CHECKING
		{
			struct sockaddr_storage source_addr;
			socklen_t	source_addr_len;

			MemSet(&source_addr, 0, sizeof(source_addr));
			source_addr_len = sizeof(source_addr);

			if (getsockname(pEntry->txfd, (struct sockaddr *) &source_addr, &source_addr_len) == -1)
			{
				ereport(ERROR,
						(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						 errmsg("interconnect Error: Could not get port from socket"),
						 errdetail("%m")));
			}
			Assert(pEntry->txfd_family == source_addr.ss_family);
		}
#endif

		/*
		 * If the socket was created with a different address family than the
		 * place we are sending to, we might need to do something special.
		 */
		if (pEntry->txfd_family != conn->peer.ss_family)
		{
			/*
			 * If the socket was created AF_INET6, but the address we want to
			 * send to is IPv4 (AF_INET), we might need to change the address
			 * format.  On Linux, it isn't necessary:  glibc automatically
			 * handles this.  But on MAC OSX and Solaris, we need to convert
			 * the IPv4 address to an V4-MAPPED address in AF_INET6 format.
			 */
			if (pEntry->txfd_family == AF_INET6)
			{
				struct sockaddr_storage temp;
				const struct sockaddr_in *in = (const struct sockaddr_in *) &conn->peer;
				struct sockaddr_in6 *in6_new = (struct sockaddr_in6 *) &temp;

				memset(&temp, 0, sizeof(temp));

				elog(DEBUG1, "We are inet6, remote is inet.  Converting to v4 mapped address.");

				/* Construct a V4-to-6 mapped address.  */
				temp.ss_family = AF_INET6;
				in6_new->sin6_family = AF_INET6;
				in6_new->sin6_port = in->sin_port;
				in6_new->sin6_flowinfo = 0;

				memset(&in6_new->sin6_addr, '\0', sizeof(in6_new->sin6_addr));
				/* in6_new->sin6_addr.s6_addr16[5] = 0xffff; */
				((uint16 *) &in6_new->sin6_addr)[5] = 0xffff;
				/* in6_new->sin6_addr.s6_addr32[3] = in->sin_addr.s_addr; */
				memcpy(((char *) &in6_new->sin6_addr) + 12, &(in->sin_addr), 4);
				in6_new->sin6_scope_id = 0;

				/* copy it back */
				memcpy(&conn->peer, &temp, sizeof(struct sockaddr_in6));
				conn->peer_len = sizeof(struct sockaddr_in6);
			}
			else
			{
				/*
				 * If we get here, something is really wrong.  We created the
				 * socket as IPv4-only (AF_INET), but the address we are
				 * trying to send to is IPv6.  It's possible we could have a
				 * V4-mapped address that we could convert to an IPv4 address,
				 * but there is currently no code path where that could
				 * happen.  So this must be an error.
				 */
				elog(ERROR, "Trying to use an IPv4 (AF_INET) socket to send to an IPv6 address");
			}
		}
	}

	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		ereport(DEBUG1, (errmsg("Interconnect connecting to seg%d slice%d %s "
								"pid=%d sockfd=%d",
								conn->remoteContentId,
								pEntry->recvSlice->sliceIndex,
								conn->remoteHostAndPort,
								conn->cdbProc->pid,
								conn->sockfd)));

	/* send connection request */
	MemSet(&conn->conn_info, 0, sizeof(conn->conn_info));
	conn->conn_info.len = 0;
	conn->conn_info.flags = 0;
	conn->conn_info.motNodeId = pEntry->motNodeId;

	conn->conn_info.recvSliceIndex = pEntry->recvSlice->sliceIndex;
	conn->conn_info.sendSliceIndex = pEntry->sendSlice->sliceIndex;
	conn->conn_info.srcContentId = GpIdentity.segindex;
	conn->conn_info.dstContentId = conn->cdbProc->contentid;

	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		elog(DEBUG1, "setupOutgoingUDPConnection: node %d route %d srccontent %d dstcontent %d: %s",
			 pEntry->motNodeId, conn->route, GpIdentity.segindex, conn->cdbProc->contentid, conn->remoteHostAndPort);

	conn->conn_info.srcListenerPort = (Gp_listener_port >> 16) & 0x0ffff;
	conn->conn_info.srcPid = MyProcPid;
	conn->conn_info.dstPid = conn->cdbProc->pid;
	conn->conn_info.dstListenerPort = conn->cdbProc->listenerPort;

	conn->conn_info.sessionId = gp_session_id;
	conn->conn_info.icId = sliceTbl->ic_instance_id;

	connAddHash(&ic_control_info.connHtab, conn);

	/*
	 * No need to get the connection lock here, since background rx thread
	 * will never access send connections.
	 */
	conn->msgPos = NULL;
	conn->msgSize = sizeof(conn->conn_info);
	conn->stillActive = true;
	conn->conn_info.seq = 1;
	Assert(conn->peer.ss_family == AF_INET || conn->peer.ss_family == AF_INET6);

}								/* setupOutgoingUDPConnection */

/*
 * handleCachedPackets
 * 		Deal with cached packets.
 */
static void
handleCachedPackets(void)
{
	MotionConn *cachedConn = NULL;
	MotionConn *setupConn = NULL;
	ConnHtabBin *bin = NULL;
	icpkthdr   *pkt = NULL;
	AckSendParam param;
	int			i = 0;
	int			j = 0;
	bool		dummy;

	for (i = 0; i < ic_control_info.startupCacheHtab.size; i++)
	{
		bin = ic_control_info.startupCacheHtab.table[i];

		while (bin)
		{
			cachedConn = bin->conn,
				setupConn = NULL;

			for (j = 0; j < cachedConn->pkt_q_size; j++)
			{
				pkt = (icpkthdr *) cachedConn->pkt_q[j];

				if (pkt == NULL)
					continue;

				rx_buffer_pool.maxCount--;

				/* look up this pkt's connection in connHtab */
				setupConn = findConnByHeader(&ic_control_info.connHtab, pkt);
				if (setupConn == NULL)
				{
					/* mismatch! */
					putRxBufferToFreeList(&rx_buffer_pool, pkt);
					cachedConn->pkt_q[j] = NULL;
					continue;
				}

				memset(&param, 0, sizeof(param));
				if (!handleDataPacket(setupConn, pkt, &cachedConn->peer, &cachedConn->peer_len, &param, &dummy))
				{
					/* no need to cache this packet */
					putRxBufferToFreeList(&rx_buffer_pool, pkt);
				}

				ic_statistics.recvPktNum++;
				if (param.msg.len != 0)
					sendAckWithParam(&param);

				cachedConn->pkt_q[j] = NULL;
			}
			bin = bin->next;
			connDelHash(&ic_control_info.startupCacheHtab, cachedConn);

			/*
			 * MPP-19981 free the cached connections; otherwise memory leak
			 * would be introduced.
			 */
			free(cachedConn->pkt_q);
			free(cachedConn);
		}
	}
}

/*
 * SetupUDPIFCInterconnect_Internal
 * 		Internal function for setting up UDP interconnect.
 */
static ChunkTransportState *
SetupUDPIFCInterconnect_Internal(SliceTable *sliceTable)
{
	int			i,
				n;
	ListCell   *cell;
	ExecSlice  *mySlice;
	ExecSlice  *aSlice;
	MotionConn *conn = NULL;
	int			incoming_count = 0;
	int			outgoing_count = 0;
	int			expectedTotalIncoming = 0;
	int			expectedTotalOutgoing = 0;

	ChunkTransportStateEntry *sendingChunkTransportState = NULL;
	ChunkTransportState *interconnect_context;

	pthread_mutex_lock(&ic_control_info.lock);

	Assert(sliceTable->ic_instance_id > 0);

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		Assert(gp_interconnect_id == sliceTable->ic_instance_id);
		/*
		 * QD use cursorHistoryTable to handle mismatch packets, no
		 * need to update ic_control_info.ic_instance_id
		 */
	}
	else
	{
		/*
		 * update ic_control_info.ic_instance_id, it is mainly used
		 * by rx thread to handle mismatch packets
		 */
		ic_control_info.ic_instance_id = sliceTable->ic_instance_id;
	}

	interconnect_context = palloc0(sizeof(ChunkTransportState));

	/* initialize state variables */
	Assert(interconnect_context->size == 0);
	interconnect_context->size = CTS_INITIAL_SIZE;
	interconnect_context->states = palloc0(CTS_INITIAL_SIZE * sizeof(ChunkTransportStateEntry));

	interconnect_context->networkTimeoutIsLogged = false;
	interconnect_context->teardownActive = false;
	interconnect_context->activated = false;
	interconnect_context->incompleteConns = NIL;
	interconnect_context->sliceTable = copyObject(sliceTable);
	interconnect_context->sliceId = sliceTable->localSlice;

	interconnect_context->RecvTupleChunkFrom = RecvTupleChunkFromUDPIFC;
	interconnect_context->RecvTupleChunkFromAny = RecvTupleChunkFromAnyUDPIFC;
	interconnect_context->SendEos = SendEosUDPIFC;
	interconnect_context->SendChunk = SendChunkUDPIFC;
	interconnect_context->doSendStopMessage = doSendStopMessageUDPIFC;

	mySlice = &interconnect_context->sliceTable->slices[sliceTable->localSlice];

	Assert(mySlice &&
		   mySlice->sliceIndex == sliceTable->localSlice);

#ifdef USE_ASSERT_CHECKING
	set_test_mode();
#endif

	if (Gp_role == GP_ROLE_DISPATCH)
	{
		/* 
		 * Prune the history table if it is too large
		 * 
		 * We only keep history of constant length so that
		 * - The history table takes only constant amount of memory.
		 * - It is long enough so that it is almost impossible to receive 
		 *   packets from an IC instance that is older than the first one 
		 *   in the history.
		 */
		if (rx_control_info.cursorHistoryTable.count > (2 * CURSOR_IC_TABLE_SIZE))
		{
			uint32 prune_id = sliceTable->ic_instance_id - CURSOR_IC_TABLE_SIZE;

			/* 
			 * Only prune if we didn't underflow -- also we want the prune id
			 * to be newer than the limit (hysteresis)
			 */
			if (prune_id < sliceTable->ic_instance_id)
			{
				if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
					elog(DEBUG1, "prune cursor history table (count %d), icid %d", rx_control_info.cursorHistoryTable.count, sliceTable->ic_instance_id);
				pruneCursorIcEntry(&rx_control_info.cursorHistoryTable, prune_id);
			}
		}

		addCursorIcEntry(&rx_control_info.cursorHistoryTable, sliceTable->ic_instance_id, gp_command_count);
	}

	/* now we'll do some setup for each of our Receiving Motion Nodes. */
	foreach(cell, mySlice->children)
	{
		int			numProcs;
		int			childId = lfirst_int(cell);
		ChunkTransportStateEntry *pEntry = NULL;

		aSlice = &interconnect_context->sliceTable->slices[childId];
		numProcs = list_length(aSlice->primaryProcesses);

		if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
			elog(DEBUG1, "Setup recving connections: my slice %d, childId %d",
				 mySlice->sliceIndex, childId);

		pEntry = createChunkTransportState(interconnect_context, aSlice, mySlice, numProcs);

		Assert(pEntry);
		Assert(pEntry->valid);

		for (i = 0; i < pEntry->numConns; i++)
		{
			conn = &pEntry->conns[i];
			conn->cdbProc = list_nth(aSlice->primaryProcesses, i);

			if (conn->cdbProc)
			{
				expectedTotalIncoming++;

				/* rx_buffer_queue */
				conn->pkt_q_capacity = Gp_interconnect_queue_depth;
				conn->pkt_q_size = 0;
				conn->pkt_q_head = 0;
				conn->pkt_q_tail = 0;

				SIMPLE_FAULT_INJECTOR("interconnect_setup_palloc");
				conn->pkt_q = (uint8 **) palloc0(conn->pkt_q_capacity * sizeof(uint8 *));

				/* update the max buffer count of our rx buffer pool.  */
				rx_buffer_pool.maxCount += conn->pkt_q_capacity;


				/*
				 * connection header info (defining characteristics of this
				 * connection)
				 */
				MemSet(&conn->conn_info, 0, sizeof(conn->conn_info));
				conn->route = i;

				conn->conn_info.seq = 1;
				conn->stillActive = true;
				conn->remapper = CreateTupleRemapper();

				incoming_count++;

				conn->conn_info.motNodeId = pEntry->motNodeId;
				conn->conn_info.recvSliceIndex = mySlice->sliceIndex;
				conn->conn_info.sendSliceIndex = aSlice->sliceIndex;

				conn->conn_info.srcContentId = conn->cdbProc->contentid;
				conn->conn_info.dstContentId = GpIdentity.segindex;

				conn->conn_info.srcListenerPort = conn->cdbProc->listenerPort;
				conn->conn_info.srcPid = conn->cdbProc->pid;
				conn->conn_info.dstPid = MyProcPid;
				conn->conn_info.dstListenerPort = (Gp_listener_port >> 16) & 0x0ffff;
				conn->conn_info.sessionId = gp_session_id;
				conn->conn_info.icId = sliceTable->ic_instance_id;
				conn->conn_info.flags = UDPIC_FLAGS_RECEIVER_TO_SENDER;

				connAddHash(&ic_control_info.connHtab, conn);
			}
		}
	}

	snd_control_info.cwnd = 0;
	snd_control_info.minCwnd = 0;
	snd_control_info.ssthresh = 0;

	/* Initiate outgoing connections. */
	if (mySlice->parentIndex != -1)
	{
		initSndBufferPool(&snd_buffer_pool);
		initUnackQueueRing(&unack_queue_ring);
		ic_control_info.isSender = true;
		ic_control_info.lastExpirationCheckTime = getCurrentTime();
		ic_control_info.lastPacketSendTime = ic_control_info.lastExpirationCheckTime;
		ic_control_info.lastDeadlockCheckTime = ic_control_info.lastExpirationCheckTime;

		sendingChunkTransportState = startOutgoingUDPConnections(interconnect_context, mySlice, &expectedTotalOutgoing);
		n = sendingChunkTransportState->numConns;

		for (i = 0; i < n; i++)
		{						/* loop to set up outgoing connections */
			conn = &sendingChunkTransportState->conns[i];

			if (conn->cdbProc)
			{
				setupOutgoingUDPConnection(interconnect_context, sendingChunkTransportState, conn);
				outgoing_count++;
			}
		}
		snd_control_info.minCwnd = snd_control_info.cwnd;
		snd_control_info.ssthresh = snd_buffer_pool.maxCount;

#ifdef TRANSFER_PROTOCOL_STATS
		initTransProtoStats();
#endif

	}
	else
	{
		ic_control_info.isSender = false;
		ic_control_info.lastExpirationCheckTime = 0;
	}

	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		ereport(DEBUG1, (errmsg("SetupUDPInterconnect will activate "
								"%d incoming, %d outgoing routes for ic_instancce_id %d. "
								"Listening on ports=%d/%d sockfd=%d.",
								expectedTotalIncoming, expectedTotalOutgoing, sliceTable->ic_instance_id,
								Gp_listener_port & 0x0ffff, (Gp_listener_port >> 16) & 0x0ffff, UDP_listenerFd)));

	/*
	 * If there are packets cached by background thread, add them to the
	 * connections.
	 */
	if (gp_interconnect_cache_future_packets)
		handleCachedPackets();

	interconnect_context->activated = true;

	pthread_mutex_unlock(&ic_control_info.lock);

	return interconnect_context;
}

/*
 * SetupUDPIFCInterconnect
 * 		setup UDP interconnect.
 */
void
SetupUDPIFCInterconnect(EState *estate)
{
	ChunkTransportState *icContext = NULL;
	PG_TRY();
	{
		/*
		 * The rx-thread might have set an error since last teardown,
		 * technically it is not part of current query, discard it directly.
		 */
		resetRxThreadError();

		icContext = SetupUDPIFCInterconnect_Internal(estate->es_sliceTable);

		/* Internal error if we locked the mutex but forgot to unlock it. */
		Assert(pthread_mutex_unlock(&ic_control_info.lock) != 0);
	}
	PG_CATCH();
	{
		/*
		 * Remove connections from hash table to avoid packet handling in the
		 * rx pthread, else the packet handling code could use memory whose
		 * context (InterconnectContext) would be soon reset - that could
		 * panic the process.
		 */
		ConnHashTable *ht = &ic_control_info.connHtab;

		for (int i = 0; i < ht->size; i++)
		{
			struct ConnHtabBin *trash;
			MotionConn *conn;

			trash = ht->table[i];
			while (trash != NULL)
			{
				conn = trash->conn;
				/* Get trash at first as trash will be pfree-ed in connDelHash. */
				trash = trash->next;
				connDelHash(ht, conn);
			}
		}
		pthread_mutex_unlock(&ic_control_info.lock);

		PG_RE_THROW();
	}
	PG_END_TRY();

	icContext->estate = estate;
	estate->interconnect_context = icContext;
	estate->es_interconnect_is_setup = true;

	/* Check if any of the QEs has already finished with error */
	if (Gp_role == GP_ROLE_DISPATCH)
		checkForCancelFromQD(icContext);
}


/*
 * freeDisorderedPackets
 * 		Put the disordered packets into free buffer list.
 */
static void
freeDisorderedPackets(MotionConn *conn)
{
	int			k;

	if (conn->pkt_q == NULL)
		return;

	for (k = 0; k < conn->pkt_q_capacity; k++)
	{
		icpkthdr   *buf = (icpkthdr *) conn->pkt_q[k];

		if (buf != NULL)
		{
			if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
				elog(DEBUG1, "CLEAR Out-of-order PKT: conn %p pkt [seq %d] for node %d route %d, [head seq] %d queue size %d, queue head %d queue tail %d", conn, buf->seq, buf->motNodeId, conn->route, conn->conn_info.seq - conn->pkt_q_size, conn->pkt_q_size, conn->pkt_q_head, conn->pkt_q_tail);

			/* return the buffer into the free list. */
			putRxBufferToFreeList(&rx_buffer_pool, buf);
			conn->pkt_q[k] = NULL;
		}
	}
}

/*
 * chunkTransportStateEntryInitialized
 *  	Check whether the transport state entry is initialized.
 */
static bool
chunkTransportStateEntryInitialized(ChunkTransportState *transportStates,
									int16 motNodeID)
{
	if (motNodeID > transportStates->size || !transportStates->states[motNodeID - 1].valid)
		return false;

	return true;
}

/*
 * computeNetworkStatistics
 * 		Compute the max/min/avg network statistics.
 */
static inline void
computeNetworkStatistics(uint64 value, uint64 *min, uint64 *max, double *sum)
{
	if (value >= *max)
		*max = value;
	if (value <= *min)
		*min = value;
	*sum += value;
}

/*
 * TeardownUDPIFCInterconnect_Internal
 * 		Helper function for TeardownUDPIFCInterconnect.
 *
 * Developers should pay attention to:
 *
 * 1) Do not handle interrupts/throw errors in Teardown, otherwise, Teardown may be called twice.
 *    It will introduce an undefined behavior. And memory leaks will be introduced.
 *
 * 2) Be careful about adding elog/ereport/write_log in Teardown function,
 *    esp, out of HOLD_INTERRUPTS/RESUME_INTERRUPTS pair, since elog/ereport/write_log may
 *    handle interrupts.
 *
 */
static void
TeardownUDPIFCInterconnect_Internal(ChunkTransportState *transportStates,
									bool hasErrors)
{
	ChunkTransportStateEntry *pEntry = NULL;
	int			i;
	ExecSlice  *mySlice;
	MotionConn *conn;

	uint64		maxRtt = 0;
	double		avgRtt = 0;
	uint64		minRtt = ~((uint64) 0);

	uint64		maxDev = 0;
	double		avgDev = 0;
	uint64		minDev = ~((uint64) 0);

	bool		isReceiver = false;

	if (transportStates == NULL || transportStates->sliceTable == NULL)
	{
		elog(LOG, "TeardownUDPIFCInterconnect: missing slice table.");
		return;
	}

	if (!transportStates->states)
	{
		elog(LOG, "TeardownUDPIFCInterconnect: missing states.");
		return;
	}

	mySlice = &transportStates->sliceTable->slices[transportStates->sliceId];

	HOLD_INTERRUPTS();

	/* Log the start of TeardownInterconnect. */
	if (gp_log_interconnect >= GPVARS_VERBOSITY_TERSE)
	{
		int			elevel = 0;

		if (hasErrors || !transportStates->activated)
		{
			if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
				elevel = LOG;
			else
				elevel = DEBUG1;
		}
		else if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
			elevel = DEBUG4;

		if (elevel)
			ereport(elevel, (errmsg("Interconnect seg%d slice%d cleanup state: "
									"%s; setup was %s",
									GpIdentity.segindex, mySlice->sliceIndex,
									hasErrors ? "hasErrors" : "normal",
									transportStates->activated ? "completed" : "exited")));

		/* if setup did not complete, log the slicetable */
		if (!transportStates->activated &&
			gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
			elog_node_display(DEBUG3, "local slice table", transportStates->sliceTable, true);
	}

	/*
	 * add lock to protect the hash table, since background thread is still
	 * working.
	 */
	pthread_mutex_lock(&ic_control_info.lock);

	if (gp_interconnect_cache_future_packets)
		cleanupStartupCache();

	/*
	 * Now "normal" connections which made it through our peer-registration
	 * step. With these we have to worry about "in-flight" data.
	 */
	if (mySlice->parentIndex != -1)
	{
		ExecSlice  *parentSlice;

		parentSlice = &transportStates->sliceTable->slices[mySlice->parentIndex];

		/* cleanup a Sending motion node. */
		if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
			elog(DEBUG1, "Interconnect seg%d slice%d closing connections to slice%d (%d peers)",
				 GpIdentity.segindex, mySlice->sliceIndex, mySlice->parentIndex,
				 list_length(parentSlice->primaryProcesses));

		/*
		 * In the olden days, we required that the error case successfully
		 * transmit and end-of-stream message here. But the introduction of
		 * cdbdisp_check_estate_for_cancel() alleviates for the QD case, and
		 * the cross-connection of writer gangs in the dispatcher (propagation
		 * of cancel between them) fixes the I-S case.
		 *
		 * So the call to forceEosToPeers() is no longer required.
		 */
		if (chunkTransportStateEntryInitialized(transportStates, mySlice->sliceIndex))
		{
			/* now it is safe to remove. */
			pEntry = removeChunkTransportState(transportStates, mySlice->sliceIndex);

			/* connection array allocation may fail in interconnect setup. */
			if (pEntry->conns)
			{
				for (i = 0; i < pEntry->numConns; i++)
				{
					conn = pEntry->conns + i;
					if (conn->cdbProc == NULL)
						continue;

					/* compute some statistics */
					computeNetworkStatistics(conn->rtt, &minRtt, &maxRtt, &avgRtt);
					computeNetworkStatistics(conn->dev, &minDev, &maxDev, &avgDev);

					icBufferListReturn(&conn->sndQueue, false);
					icBufferListReturn(&conn->unackQueue, Gp_interconnect_fc_method == INTERCONNECT_FC_METHOD_CAPACITY ? false : true);

					connDelHash(&ic_control_info.connHtab, conn);
				}
				avgRtt = avgRtt / pEntry->numConns;
				avgDev = avgDev / pEntry->numConns;

				/* free all send side buffers */
				cleanSndBufferPool(&snd_buffer_pool);
			}
		}
#ifdef TRANSFER_PROTOCOL_STATS
		dumpTransProtoStats();
#endif

	}

	/*
	 * Previously, there is a piece of code that deals with pending stops. Now
	 * it is delegated to background rx thread which will deal with any
	 * mismatched packets.
	 */

	/*
	 * cleanup all of our Receiving Motion nodes, these get closed immediately
	 * (the receiver know for real if they want to shut down -- they aren't
	 * going to be processing any more data).
	 */
	ListCell   *cell;

	foreach(cell, mySlice->children)
	{
		ExecSlice  *aSlice;
		int			childId = lfirst_int(cell);

		aSlice = &transportStates->sliceTable->slices[childId];

		/*
		 * First check whether the entry is initialized to avoid the potential
		 * errors thrown out from the removeChunkTransportState, which may
		 * introduce some memory leaks.
		 */
		if (chunkTransportStateEntryInitialized(transportStates, aSlice->sliceIndex))
		{
			/* remove it */
			pEntry = removeChunkTransportState(transportStates, aSlice->sliceIndex);
			Assert(pEntry);

			if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
				elog(DEBUG1, "Interconnect closing connections from slice%d",
					 aSlice->sliceIndex);
			isReceiver = true;

			if (pEntry->conns)
			{
				/*
				 * receivers know that they no longer care about data from
				 * below ... so we can safely discard data queued in both
				 * directions
				 */
				for (i = 0; i < pEntry->numConns; i++)
				{
					conn = pEntry->conns + i;
					if (conn->cdbProc == NULL)
						continue;

					/* out of memory has occurred, break out */
					if (!conn->pkt_q)
						break;

					rx_buffer_pool.maxCount -= conn->pkt_q_capacity;

					connDelHash(&ic_control_info.connHtab, conn);

					/*
					 * putRxBufferAndSendAck() dequeues messages and moves
					 * them to pBuff
					 */
					while (conn->pkt_q_size > 0)
					{
						putRxBufferAndSendAck(conn, NULL);
					}

					/* we also need to clear all the out-of-order packets */
					freeDisorderedPackets(conn);

					/* free up the packet queue */
					pfree(conn->pkt_q);
					conn->pkt_q = NULL;

					/* free up the tuple remapper */
					if (conn->remapper)
						DestroyTupleRemapper(conn->remapper);
				}
				pfree(pEntry->conns);
				pEntry->conns = NULL;
			}
		}
	}

	/*
	 * now that we've moved active rx-buffers to the freelist, we can prune
	 * the freelist itself
	 */
	while (rx_buffer_pool.count > rx_buffer_pool.maxCount)
	{
		icpkthdr   *buf = NULL;

		/* If this happened, there are some memory leaks.. */
		if (rx_buffer_pool.freeList == NULL)
		{
			pthread_mutex_unlock(&ic_control_info.lock);
			elog(FATAL, "freelist NULL: count %d max %d buf %p", rx_buffer_pool.count, rx_buffer_pool.maxCount, rx_buffer_pool.freeList);
		}

		buf = getRxBufferFromFreeList(&rx_buffer_pool);
		freeRxBuffer(&rx_buffer_pool, buf);
	}

	/*
	 * Update the history of interconnect instance id.
	 */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		updateCursorIcEntry(&rx_control_info.cursorHistoryTable, transportStates->sliceTable->ic_instance_id, 0);
	}
	else if (Gp_role == GP_ROLE_EXECUTE)
	{
		rx_control_info.lastTornIcId = transportStates->sliceTable->ic_instance_id;
	}

	elog((gp_interconnect_log_stats ? LOG : DEBUG1), "Interconnect State: "
		 "isSender %d isReceiver %d "
		 "snd_queue_depth %d recv_queue_depth %d Gp_max_packet_size %d "
		 "UNACK_QUEUE_RING_SLOTS_NUM %d TIMER_SPAN %lld DEFAULT_RTT %d "
		 "hasErrors %d, ic_instance_id %d ic_id_last_teardown %d "
		 "snd_buffer_pool.count %d snd_buffer_pool.maxCount %d snd_sock_bufsize %d recv_sock_bufsize %d "
		 "snd_pkt_count %d retransmits %d crc_errors %d"
		 " recv_pkt_count %d recv_ack_num %d"
		 " recv_queue_size_avg %f"
		 " capacity_avg %f"
		 " freebuf_avg %f "
		 "mismatch_pkt_num %d disordered_pkt_num %d duplicated_pkt_num %d"
		 " rtt/dev [" UINT64_FORMAT "/" UINT64_FORMAT ", %f/%f, " UINT64_FORMAT "/" UINT64_FORMAT "] "
		 " cwnd %f status_query_msg_num %d",
		 ic_control_info.isSender, isReceiver,
		 Gp_interconnect_snd_queue_depth, Gp_interconnect_queue_depth, Gp_max_packet_size,
		 UNACK_QUEUE_RING_SLOTS_NUM, TIMER_SPAN, DEFAULT_RTT,
		 hasErrors, transportStates->sliceTable->ic_instance_id, rx_control_info.lastTornIcId,
		 snd_buffer_pool.count, snd_buffer_pool.maxCount, ic_control_info.socketSendBufferSize, ic_control_info.socketRecvBufferSize,
		 ic_statistics.sndPktNum, ic_statistics.retransmits, ic_statistics.crcErrors,
		 ic_statistics.recvPktNum, ic_statistics.recvAckNum,
		 (double) ((double) ic_statistics.totalRecvQueueSize) / ((double) ic_statistics.recvQueueSizeCountingTime),
		 (double) ((double) ic_statistics.totalCapacity) / ((double) ic_statistics.capacityCountingTime),
		 (double) ((double) ic_statistics.totalBuffers) / ((double) ic_statistics.bufferCountingTime),
		 ic_statistics.mismatchNum, ic_statistics.disorderedPktNum, ic_statistics.duplicatedPktNum,
		 (minRtt == ~((uint64) 0) ? 0 : minRtt), (minDev == ~((uint64) 0) ? 0 : minDev), avgRtt, avgDev, maxRtt, maxDev,
		 snd_control_info.cwnd, ic_statistics.statusQueryMsgNum);

	ic_control_info.isSender = false;
	memset(&ic_statistics, 0, sizeof(ICStatistics));

	pthread_mutex_unlock(&ic_control_info.lock);

	/* reset the rx thread network error flag */
	resetRxThreadError();

	transportStates->activated = false;
	transportStates->sliceTable = NULL;

	if (transportStates != NULL)
	{
		if (transportStates->states != NULL)
		{
			pfree(transportStates->states);
			transportStates->states = NULL;
		}
		pfree(transportStates);
	}

	if (gp_log_interconnect >= GPVARS_VERBOSITY_TERSE)
		elog(DEBUG1, "TeardownUDPIFCInterconnect successful");

	RESUME_INTERRUPTS();
}

/*
 * TeardownUDPIFCInterconnect
 * 		Tear down UDP interconnect.
 *
 * This function is called to release the resources used by interconnect.
 */
void
TeardownUDPIFCInterconnect(ChunkTransportState *transportStates,
						   bool hasErrors)
{
	PG_TRY();
	{
		TeardownUDPIFCInterconnect_Internal(transportStates, hasErrors);

		Assert(pthread_mutex_unlock(&ic_control_info.lock) != 0);
	}
	PG_CATCH();
	{
		pthread_mutex_unlock(&ic_control_info.lock);
		PG_RE_THROW();
	}
	PG_END_TRY();
}

/*
 * prepareRxConnForRead
 * 		Prepare the receive connection for reading.
 *
 * MUST BE CALLED WITH ic_control_info.lock LOCKED.
 */
static void
prepareRxConnForRead(MotionConn *conn)
{
	elog(DEBUG3, "In prepareRxConnForRead: conn %p, q_head %d q_tail %d q_size %d", conn, conn->pkt_q_head, conn->pkt_q_tail, conn->pkt_q_size);

	Assert(conn->pkt_q[conn->pkt_q_head] != NULL);
	conn->pBuff = conn->pkt_q[conn->pkt_q_head];
	conn->msgPos = conn->pBuff;
	conn->msgSize = ((icpkthdr *) conn->pBuff)->len;
	conn->recvBytes = conn->msgSize;
}

/*
 * receiveChunksUDPIFC
 * 		Receive chunks from the senders
 *
 * MUST BE CALLED WITH ic_control_info.lock LOCKED.
 */
static TupleChunkListItem
receiveChunksUDPIFC(ChunkTransportState *pTransportStates, ChunkTransportStateEntry *pEntry,
					int16 motNodeID, int16 *srcRoute, MotionConn *conn)
{
	int 		nFds = 0;
	int 		*waitFds = NULL;
	int 		nevent = 0;
	TupleChunkListItem	tcItem = NULL;

#ifdef AMS_VERBOSE_LOGGING
	elog(DEBUG5, "receivechunksUDP: motnodeid %d", motNodeID);
#endif

	Assert(pTransportStates);
	Assert(pTransportStates->sliceTable);

	if (conn != NULL)
	{
		*srcRoute = conn->route;
		setMainThreadWaiting(&rx_control_info.mainWaitingState, motNodeID, conn->route,
							 pTransportStates->sliceTable->ic_instance_id);
	}
	else
	{
		/* non-directed receive */
		setMainThreadWaiting(&rx_control_info.mainWaitingState, motNodeID, ANY_ROUTE,
							 pTransportStates->sliceTable->ic_instance_id);
	}

	nevent = 2; /* nevent = waited fds number + 2 (latch and postmaster) */
	if (Gp_role == GP_ROLE_DISPATCH)
	{
		/* get all wait sock fds */
		waitFds = cdbdisp_getWaitSocketFds(pTransportStates->estate->dispatcherState, &nFds);
		if (waitFds != NULL)
			nevent += nFds;
	}

	/* reset WaitEventSet */
	ResetWaitEventSet(&ICWaitSet, TopMemoryContext, nevent);

	/*
	 * Use PG_TRY() - PG_CATCH() to make sure destroy the waiteventset (close the epoll fd)
	 * The main receive logic is in receiveChunksUDPIFCLoop()
	 */
	PG_TRY();
	{
		AddWaitEventToSet(ICWaitSet, WL_LATCH_SET, PGINVALID_SOCKET, &ic_control_info.latch, NULL);
		AddWaitEventToSet(ICWaitSet, WL_POSTMASTER_DEATH, PGINVALID_SOCKET, NULL, NULL);
		for (int i = 0; i < nFds; i++)
		{
			AddWaitEventToSet(ICWaitSet, WL_SOCKET_READABLE, waitFds[i], NULL, NULL);
		}

		tcItem = receiveChunksUDPIFCLoop(pTransportStates, pEntry, srcRoute, conn, ICWaitSet, nevent);
	}
	PG_CATCH();
	{
		if (waitFds != NULL)
			pfree(waitFds);
		PG_RE_THROW();
	}
	PG_END_TRY();

	if (waitFds != NULL)
		pfree(waitFds);

	return tcItem;
}

static TupleChunkListItem
receiveChunksUDPIFCLoop(ChunkTransportState *pTransportStates, ChunkTransportStateEntry *pEntry,
						int16 *srcRoute, MotionConn *conn, WaitEventSet *waitset, int nevent)
{
	TupleChunkListItem	tcItem = NULL;
	MotionConn 	*rxconn = NULL;
	int			retries = 0;
	bool		directed = false;
	WaitEvent	*rEvents = NULL;

	if (conn != NULL)
		directed = true;

	rEvents = palloc(nevent * sizeof(WaitEvent)); /* returned events */
	/* we didn't have any data, so we've got to read it from the network. */
	for (;;)
	{
		/* 1. Do we have data ready */
		if (rx_control_info.mainWaitingState.reachRoute != ANY_ROUTE)
		{
			rxconn = pEntry->conns + rx_control_info.mainWaitingState.reachRoute;

			prepareRxConnForRead(rxconn);

			elog(DEBUG2, "receiveChunksUDPIFC: non-directed rx woke on route %d", rx_control_info.mainWaitingState.reachRoute);
			resetMainThreadWaiting(&rx_control_info.mainWaitingState);
		}

		aggregateStatistics(pEntry);

		if (rxconn != NULL)
		{
			Assert(rxconn->pBuff);

			pthread_mutex_unlock(&ic_control_info.lock);

			elog(DEBUG2, "got data with length %d", rxconn->recvBytes);
			/* successfully read into this connection's buffer. */
			tcItem = RecvTupleChunk(rxconn, pTransportStates);

			if (!directed)
				*srcRoute = rxconn->route;
			pfree(rEvents);
			return tcItem;
		}

		retries++;

		/*
		 * Ok, we've processed all the items currently in the queue. Arm the
		 * latch (before releasing the mutex), and wait for more messages to
		 * arrive. The RX thread will wake us up using the latch.
		 */
		ResetLatch(&ic_control_info.latch);
		pthread_mutex_unlock(&ic_control_info.lock);

		if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		{
			elog(DEBUG5, "waiting (timed) on route %d %s", rx_control_info.mainWaitingState.waitingRoute,
				 (rx_control_info.mainWaitingState.waitingRoute == ANY_ROUTE ? "(any route)" : ""));
		}

		/*
		 * Wait for data to become ready.
		 *
		 * In the QD, also wake up immediately if any QE reports an
		 * error through the main QD-QE libpq connection. For that, ask
		 * the dispatcher for a file descriptor to wait on for that.
		 */
		int rc = WaitEventSetWait(waitset, MAIN_THREAD_COND_TIMEOUT_MS, rEvents, nevent, WAIT_EVENT_INTERCONNECT);
		if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG && rc == 0)
			elog(DEBUG2, "receiveChunksUDPIFC(): WaitEventSetWait timeout after %d ms", MAIN_THREAD_COND_TIMEOUT_MS);

		/* check the potential errors in rx thread. */
		checkRxThreadError();

		/* do not check interrupts when holding the lock */
		ML_CHECK_FOR_INTERRUPTS(pTransportStates->teardownActive);

		/*
		 * check to see if the dispatcher should cancel
		 */
		if (Gp_role == GP_ROLE_DISPATCH)
		{
			for (int i = 0; i < rc; i++)
				if (rEvents[i].events & WL_SOCKET_READABLE)
				{
					/* event happened on wait fds, need to check cancel */
					checkForCancelFromQD(pTransportStates);
					break;
				}
		}

		/*
		 * 1. NIC on master (and thus the QD connection) may become bad, check
		 * it. 2. Postmaster may become invalid, check it
		 */
		if ((retries & 0x3f) == 0)
		{
			checkQDConnectionAlive();

			if (!PostmasterIsAlive())
				ereport(FATAL,
						(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						 errmsg("interconnect failed to recv chunks"),
						 errdetail("Postmaster is not alive.")));
		}

		pthread_mutex_lock(&ic_control_info.lock);
	} /* for (;;) */

	/* We either got data, or get cancelled. We never make it out to here. */
	return NULL;				/* make GCC behave */
}

/*
 * RecvTupleChunkFromAnyUDPIFC_Internal
 * 		Receive tuple chunks from any route (connections)
 */
static inline TupleChunkListItem
RecvTupleChunkFromAnyUDPIFC_Internal(ChunkTransportState *transportStates,
									 int16 motNodeID,
									 int16 *srcRoute)
{
	ChunkTransportStateEntry *pEntry = NULL;
	MotionConn *conn = NULL;
	int			i,
				index,
				activeCount = 0;
	TupleChunkListItem tcItem = NULL;
	bool		found = false;

	if (!transportStates)
	{
		elog(FATAL, "RecvTupleChunkFromAnyUDPIFC: missing context");
	}
	else if (!transportStates->activated)
	{
		elog(FATAL, "RecvTupleChunkFromAnyUDPIFC: interconnect context not active!");
	}

	getChunkTransportState(transportStates, motNodeID, &pEntry);

	index = pEntry->scanStart;

	pthread_mutex_lock(&ic_control_info.lock);

	for (i = 0; i < pEntry->numConns; i++, index++)
	{
		if (index >= pEntry->numConns)
			index = 0;

		conn = pEntry->conns + index;

		if (conn->stillActive)
			activeCount++;

		ic_statistics.totalRecvQueueSize += conn->pkt_q_size;
		ic_statistics.recvQueueSizeCountingTime++;

		if (conn->pkt_q_size > 0)
		{
			found = true;
			prepareRxConnForRead(conn);
			break;
		}
	}

	if (found)
	{
		pthread_mutex_unlock(&ic_control_info.lock);

		tcItem = RecvTupleChunk(conn, transportStates);
		*srcRoute = conn->route;
		pEntry->scanStart = index + 1;
		return tcItem;
	}

	/* no data pending in our queue */

#ifdef AMS_VERBOSE_LOGGING
	elog(LOG, "RecvTupleChunkFromAnyUDPIFC(): activeCount is %d", activeCount);
#endif
	if (activeCount == 0)
	{
		pthread_mutex_unlock(&ic_control_info.lock);
		return NULL;
	}

	/* receiveChunksUDPIFC() releases ic_control_info.lock as a side-effect */
	tcItem = receiveChunksUDPIFC(transportStates, pEntry, motNodeID, srcRoute, NULL);

	pEntry->scanStart = *srcRoute + 1;

	return tcItem;
}

/*
 * RecvTupleChunkFromAnyUDPIFC
 * 		Receive tuple chunks from any route (connections)
 */
static TupleChunkListItem
RecvTupleChunkFromAnyUDPIFC(ChunkTransportState *transportStates,
							int16 motNodeID,
							int16 *srcRoute)
{
	TupleChunkListItem icItem = NULL;

	PG_TRY();
	{
		icItem = RecvTupleChunkFromAnyUDPIFC_Internal(transportStates, motNodeID, srcRoute);

		/* error if mutex still held (debug build only) */
		Assert(pthread_mutex_unlock(&ic_control_info.lock) != 0);
	}
	PG_CATCH();
	{
		pthread_mutex_unlock(&ic_control_info.lock);

		PG_RE_THROW();
	}
	PG_END_TRY();

	return icItem;
}

/*
 * RecvTupleChunkFromUDPIFC_Internal
 * 		Receive tuple chunks from a specific route (connection)
 */
static inline TupleChunkListItem
RecvTupleChunkFromUDPIFC_Internal(ChunkTransportState *transportStates,
								  int16 motNodeID,
								  int16 srcRoute)
{
	ChunkTransportStateEntry *pEntry = NULL;
	MotionConn *conn = NULL;
	int16		route;

	if (!transportStates)
	{
		elog(FATAL, "RecvTupleChunkFromUDPIFC: missing context");
	}
	else if (!transportStates->activated)
	{
		elog(FATAL, "RecvTupleChunkFromUDPIFC: interconnect context not active!");
	}

#ifdef AMS_VERBOSE_LOGGING
	elog(LOG, "RecvTupleChunkFromUDPIFC().");
#endif

	/* check em' */
	ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);

#ifdef AMS_VERBOSE_LOGGING
	elog(DEBUG5, "RecvTupleChunkFromUDPIFC(motNodID=%d, srcRoute=%d)", motNodeID, srcRoute);
#endif

	getChunkTransportState(transportStates, motNodeID, &pEntry);
	conn = pEntry->conns + srcRoute;

#ifdef AMS_VERBOSE_LOGGING
	if (!conn->stillActive)
	{
		elog(LOG, "RecvTupleChunkFromUDPIFC(): connection inactive ?!");
	}
#endif

	pthread_mutex_lock(&ic_control_info.lock);

	if (!conn->stillActive)
	{
		pthread_mutex_unlock(&ic_control_info.lock);
		return NULL;
	}

	ic_statistics.totalRecvQueueSize += conn->pkt_q_size;
	ic_statistics.recvQueueSizeCountingTime++;

	if (conn->pkt_q[conn->pkt_q_head] != NULL)
	{
		prepareRxConnForRead(conn);

		pthread_mutex_unlock(&ic_control_info.lock);

		TupleChunkListItem tcItem = NULL;

		tcItem = RecvTupleChunk(conn, transportStates);

		return tcItem;
	}

	/* no existing data, we've got to read a packet */
	/* receiveChunksUDPIFC() releases ic_control_info.lock as a side-effect */

	TupleChunkListItem chunks = receiveChunksUDPIFC(transportStates, pEntry, motNodeID, &route, conn);

	return chunks;
}

/*
 * RecvTupleChunkFromUDPIFC
 * 		Receive tuple chunks from a specific route (connection)
 */
static TupleChunkListItem
RecvTupleChunkFromUDPIFC(ChunkTransportState *transportStates,
						 int16 motNodeID,
						 int16 srcRoute)
{
	TupleChunkListItem icItem = NULL;

	PG_TRY();
	{
		icItem = RecvTupleChunkFromUDPIFC_Internal(transportStates, motNodeID, srcRoute);

		/* error if mutex still held (debug build only) */
		Assert(pthread_mutex_unlock(&ic_control_info.lock) != 0);
	}
	PG_CATCH();
	{
		pthread_mutex_unlock(&ic_control_info.lock);

		PG_RE_THROW();
	}
	PG_END_TRY();

	return icItem;
}

/*
 * markUDPConnInactiveIFC
 * 		Mark the connection inactive.
 */
void
markUDPConnInactiveIFC(MotionConn *conn)
{
	pthread_mutex_lock(&ic_control_info.lock);
	conn->stillActive = false;
	pthread_mutex_unlock(&ic_control_info.lock);

	return;
}

/*
 * aggregateStatistics
 * 		aggregate statistics.
 */
static void
aggregateStatistics(ChunkTransportStateEntry *pEntry)
{
	/*
	 * We first clear the stats, and then compute new stats by aggregating the
	 * stats from each connection.
	 */
	pEntry->stat_total_ack_time = 0;
	pEntry->stat_count_acks = 0;
	pEntry->stat_max_ack_time = 0;
	pEntry->stat_min_ack_time = ~((uint64) 0);
	pEntry->stat_count_resent = 0;
	pEntry->stat_max_resent = 0;
	pEntry->stat_count_dropped = 0;

	int			connNo;

	for (connNo = 0; connNo < pEntry->numConns; connNo++)
	{
		MotionConn *conn = &pEntry->conns[connNo];

		pEntry->stat_total_ack_time += conn->stat_total_ack_time;
		pEntry->stat_count_acks += conn->stat_count_acks;
		pEntry->stat_max_ack_time = Max(pEntry->stat_max_ack_time, conn->stat_max_ack_time);
		pEntry->stat_min_ack_time = Min(pEntry->stat_min_ack_time, conn->stat_min_ack_time);
		pEntry->stat_count_resent += conn->stat_count_resent;
		pEntry->stat_max_resent = Max(pEntry->stat_max_resent, conn->stat_max_resent);
		pEntry->stat_count_dropped += conn->stat_count_dropped;
	}
}

/*
 * logPkt
 * 		Log a packet.
 *
 */
static inline void
logPkt(char *prefix, icpkthdr *pkt)
{
	write_log("%s [%s: seq %d extraSeq %d]: motNodeId %d, crc %d len %d "
			  "srcContentId %d dstDesContentId %d "
			  "srcPid %d dstPid %d "
			  "srcListenerPort %d dstListernerPort %d "
			  "sendSliceIndex %d recvSliceIndex %d "
			  "sessionId %d icId %d "
			  "flags %d ",
			  prefix, pkt->flags & UDPIC_FLAGS_RECEIVER_TO_SENDER ? "ACK" : "DATA",
			  pkt->seq, pkt->extraSeq, pkt->motNodeId, pkt->crc, pkt->len,
			  pkt->srcContentId, pkt->dstContentId,
			  pkt->srcPid, pkt->dstPid,
			  pkt->srcListenerPort, pkt->dstListenerPort,
			  pkt->sendSliceIndex, pkt->recvSliceIndex,
			  pkt->sessionId, pkt->icId,
			  pkt->flags);
}

/*
 * handleAckedPacket
 * 		Called by sender to process acked packet.
 *
 * 	Remove it from unack queue and unack queue ring, change the rtt ...
 *
 * 	RTT (Round Trip Time) is computed as the time between we send the packet
 * 	and receive the acknowledgement for the packet. When an acknowledgement
 * 	is received, an estimated RTT value (called SRTT, smoothed RTT) is updated
 * 	by using the following equation. And we also set a limitation of the max
 * 	value and min value for SRTT.
 *	    (1) SRTT = (1 - g) SRTT + g x RTT (0 < g < 1)
 *	where RTT is the measured round trip time of the packet. In implementation,
 *	g is set to 1/8. In order to compute expiration period, we also compute an
 *	estimated delay variance SDEV by using:
 *	    (2) SDEV = (1 - h) x SDEV + h x |SERR| (0 < h < 1, In implementation, h is set to 1/4)
 *	where SERR is calculated by using:
 *	    (3) SERR = RTT - SRTT
 *	Expiration period determines the timing we resend a packet. A long RTT means
 *	a long expiration period. Delay variance is used to incorporate the variance
 *	of workload/network variances at different time. When a packet is retransmitted,
 *	we back off exponentially the expiration period.
 *	    (4) exp_period = (SRTT + y x SDEV) << retry
 *	Here y is a constant (In implementation, we use 4) and retry is the times the
 *	packet is retransmitted.
 */
static void
handleAckedPacket(MotionConn *ackConn, ICBuffer *buf, uint64 now)
{
	uint64		ackTime = 0;

	bool		bufIsHead = (&buf->primary == icBufferListFirst(&ackConn->unackQueue));

	buf = icBufferListDelete(&ackConn->unackQueue, buf);

	if (Gp_interconnect_fc_method == INTERCONNECT_FC_METHOD_LOSS)
	{
		buf = icBufferListDelete(&unack_queue_ring.slots[buf->unackQueueRingSlot], buf);
		unack_queue_ring.numOutStanding--;
		if (icBufferListLength(&ackConn->unackQueue) >= 1)
			unack_queue_ring.numSharedOutStanding--;

		ackTime = now - buf->sentTime;

		/*
		 * In udp_testmode, we do not change rtt dynamically due to the large
		 * number of packet losses introduced by fault injection code. This
		 * can decrease the testing time.
		 */
#ifdef USE_ASSERT_CHECKING
		if (!udp_testmode)
#endif
		{
			uint64		newRTT = 0;
			uint64		newDEV = 0;

			if (buf->nRetry == 0)
			{
				newRTT = buf->conn->rtt - (buf->conn->rtt >> RTT_SHIFT_COEFFICIENT) + (ackTime >> RTT_SHIFT_COEFFICIENT);
				newRTT = Min(MAX_RTT, Max(newRTT, MIN_RTT));
				buf->conn->rtt = newRTT;

				newDEV = buf->conn->dev - (buf->conn->dev >> DEV_SHIFT_COEFFICIENT) + ((Max(ackTime, newRTT) - Min(ackTime, newRTT)) >> DEV_SHIFT_COEFFICIENT);
				newDEV = Min(MAX_DEV, Max(newDEV, MIN_DEV));
				buf->conn->dev = newDEV;

				/* adjust the congestion control window. */
				if (snd_control_info.cwnd < snd_control_info.ssthresh)
					snd_control_info.cwnd += 1;
				else
					snd_control_info.cwnd += 1 / snd_control_info.cwnd;
				snd_control_info.cwnd = Min(snd_control_info.cwnd, snd_buffer_pool.maxCount);
			}
		}
	}

	buf->conn->stat_total_ack_time += ackTime;
	buf->conn->stat_max_ack_time = Max(ackTime, buf->conn->stat_max_ack_time);
	buf->conn->stat_min_ack_time = Min(ackTime, buf->conn->stat_min_ack_time);

	/*
	 * only change receivedAckSeq when it is the smallest pkt we sent and have
	 * not received ack for it.
	 */
	if (bufIsHead)
		ackConn->receivedAckSeq = buf->pkt->seq;

	/* The first packet acts like a connect setup packet */
	if (buf->pkt->seq == 1)
		ackConn->state = mcsStarted;

	icBufferListAppend(&snd_buffer_pool.freeList, buf);

#ifdef AMS_VERBOSE_LOGGING
	write_log("REMOVEPKT %d from unack queue for route %d (retry %d) sndbufmaxcount %d sndbufcount %d sndbuffreelistlen %d, sntSeq %d consumedSeq %d recvAckSeq %d capacity %d, sndQ %d, unackQ %d", buf->pkt->seq, ackConn->route, buf->nRetry, snd_buffer_pool.maxCount, snd_buffer_pool.count, icBufferListLength(&snd_buffer_pool.freeList), buf->conn->sentSeq, buf->conn->consumedSeq, buf->conn->receivedAckSeq, buf->conn->capacity, icBufferListLength(&buf->conn->sndQueue), icBufferListLength(&buf->conn->unackQueue));
	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
	{
		icBufferListLog(&buf->conn->unackQueue);
		icBufferListLog(&buf->conn->sndQueue);
	}
#endif
}

/*
 * handleAck
 * 		handle acks incoming from our upstream peers.
 *
 * if we receive a stop message, return true (caller will clean up).
 */
static bool
handleAcks(ChunkTransportState *transportStates, ChunkTransportStateEntry *pEntry)
{

	bool		ret = false;
	MotionConn *ackConn = NULL;
	int			n;

	struct sockaddr_storage peer;
	socklen_t	peerlen;

	struct icpkthdr *pkt = snd_control_info.ackBuffer;


	bool		shouldSendBuffers = false;
	SliceTable	*sliceTbl = transportStates->sliceTable;

	for (;;)
	{

		/* ready to read on our socket ? */
		peerlen = sizeof(peer);
		n = recvfrom(pEntry->txfd, (char *) pkt, MIN_PACKET_SIZE, 0,
					 (struct sockaddr *) &peer, &peerlen);

		if (n < 0)
		{
			if (errno == EWOULDBLOCK)	/* had nothing to read. */
			{
				aggregateStatistics(pEntry);
				return ret;
			}

			ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);
			if (errno == EINTR)
				continue;

			ereport(ERROR,
					(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
					 errmsg("interconnect error waiting for peer ack"),
					 errdetail("During recvfrom() call.")));
		}
		else if (n < sizeof(struct icpkthdr))
		{
			continue;
		}
		else if (n != pkt->len)
		{
			continue;
		}

		/*
		 * check the CRC of the payload.
		 */
		if (gp_interconnect_full_crc)
		{
			if (!checkCRC(pkt))
			{
				pg_atomic_add_fetch_u32((pg_atomic_uint32 *) &ic_statistics.crcErrors, 1);
				if (DEBUG2 >= log_min_messages)
					write_log("received network data error, dropping bad packet, user data unaffected.");
				continue;
			}
		}

		/*
		 * read packet, is this the ack we want ?
		 */
		if (pkt->srcContentId == GpIdentity.segindex &&
			pkt->srcPid == MyProcPid &&
			pkt->srcListenerPort == ((Gp_listener_port >> 16) & 0x0ffff) &&
			pkt->sessionId == gp_session_id &&
			pkt->icId == sliceTbl->ic_instance_id)
		{

			/*
			 * packet is for me. Note here we do not need to get a connection
			 * lock here, since background rx thread only read the hash table.
			 */
			ackConn = findConnByHeader(&ic_control_info.connHtab, pkt);

			if (ackConn == NULL)
			{
				elog(LOG, "Received ack for unknown connection (flags 0x%x)", pkt->flags);
				continue;
			}

			ackConn->stat_count_acks++;
			ic_statistics.recvAckNum++;

			uint64		now = getCurrentTime();

			ackConn->deadlockCheckBeginTime = now;

			/*
			 * We simply disregard pkt losses (NAK) due to process start race
			 * (that is, sender is started earlier than receiver. rx
			 * background thread may receive packets when connections are not
			 * created yet).
			 *
			 * Another option is to resend the packet immediately, but
			 * experiments do not show any benefits.
			 */

			if (pkt->flags & UDPIC_FLAGS_NAK)
				continue;

			while (true)
			{
				if (pkt->flags & UDPIC_FLAGS_CAPACITY)
				{
					if (pkt->extraSeq > ackConn->consumedSeq)
					{
						ackConn->capacity += pkt->extraSeq - ackConn->consumedSeq;
						ackConn->consumedSeq = pkt->extraSeq;
						shouldSendBuffers = true;
					}
				}
				else if (pkt->flags & UDPIC_FLAGS_DUPLICATE)
				{
					if (DEBUG1 >= log_min_messages)
						write_log("GOTDUPACK [seq %d] from route %d; srcpid %d dstpid %d cmd %d flags 0x%x connseq %d", pkt->seq, ackConn->route, pkt->srcPid, pkt->dstPid, pkt->icId, pkt->flags, ackConn->conn_info.seq);

					shouldSendBuffers |= (handleAckForDuplicatePkt(ackConn, pkt));
					break;
				}
				else if (pkt->flags & UDPIC_FLAGS_DISORDER)
				{
					if (DEBUG1 >= log_min_messages)
						write_log("GOTDISORDER [seq %d] from route %d; srcpid %d dstpid %d cmd %d flags 0x%x connseq %d", pkt->seq, ackConn->route, pkt->srcPid, pkt->dstPid, pkt->icId, pkt->flags, ackConn->conn_info.seq);

					shouldSendBuffers |= (handleAckForDisorderPkt(transportStates, pEntry, ackConn, pkt));
					break;
				}

				/*
				 * don't get out of the loop if pkt->seq equals to
				 * ackConn->receivedAckSeq, need to check UDPIC_FLAGS_STOP
				 * flag
				 */
				if (pkt->seq < ackConn->receivedAckSeq)
				{
					if (DEBUG1 >= log_min_messages)
						write_log("ack with bad seq?! expected (%d, %d] got %d flags 0x%x, capacity %d consumedSeq %d", ackConn->receivedAckSeq, ackConn->sentSeq, pkt->seq, pkt->flags, ackConn->capacity, ackConn->consumedSeq);
					break;
				}

				/* haven't gotten a stop request, maybe this is one ? */
				if ((pkt->flags & UDPIC_FLAGS_STOP) && !ackConn->stopRequested && ackConn->stillActive)
				{
#ifdef AMS_VERBOSE_LOGGING
					elog(LOG, "got ack with stop; srcpid %d dstpid %d cmd %d flags 0x%x pktseq %d connseq %d", pkt->srcPid, pkt->dstPid, pkt->icId, pkt->flags, pkt->seq, ackConn->conn_info.seq);
#endif
					ackConn->stopRequested = true;
					ackConn->conn_info.flags |= UDPIC_FLAGS_STOP;
					ret = true;
					/* continue to deal with acks */
				}

				if (pkt->seq == ackConn->receivedAckSeq)
				{
					if (DEBUG1 >= log_min_messages)
						write_log("ack with bad seq?! expected (%d, %d] got %d flags 0x%x, capacity %d consumedSeq %d", ackConn->receivedAckSeq, ackConn->sentSeq, pkt->seq, pkt->flags, ackConn->capacity, ackConn->consumedSeq);
					break;
				}

				/* deal with a regular ack. */
				if (pkt->flags & UDPIC_FLAGS_ACK)
				{
					ICBufferLink *link = NULL;
					ICBufferLink *next = NULL;
					ICBuffer   *buf = NULL;

#ifdef AMS_VERBOSE_LOGGING
					write_log("GOTACK [seq %d] from route %d; srcpid %d dstpid %d cmd %d flags 0x%x connseq %d", pkt->seq, ackConn->route, pkt->srcPid, pkt->dstPid, pkt->icId, pkt->flags, ackConn->conn_info.seq);
#endif

					link = icBufferListFirst(&ackConn->unackQueue);
					buf = GET_ICBUFFER_FROM_PRIMARY(link);

					while (!icBufferListIsHead(&ackConn->unackQueue, link) && buf->pkt->seq <= pkt->seq)
					{
						next = link->next;
						handleAckedPacket(ackConn, buf, now);
						shouldSendBuffers = true;
						link = next;
						buf = GET_ICBUFFER_FROM_PRIMARY(link);
					}
				}
				break;
			}

			/*
			 * When there is a capacity increase or some outstanding buffers
			 * removed from the unack queue ring, we should try to send
			 * buffers for the connection. Even when stop is received, we
			 * still send here, since in STOP/EOS race case, we may have been
			 * in EOS sending logic and will not check stop message.
			 */
			if (shouldSendBuffers)
				sendBuffers(transportStates, pEntry, ackConn);
		}
		else if (DEBUG1 >= log_min_messages)
			write_log("handleAck: not the ack we're looking for (flags 0x%x)...mot(%d) content(%d:%d) srcpid(%d:%d) dstpid(%d) srcport(%d:%d) dstport(%d) sess(%d:%d) cmd(%d:%d)",
					  pkt->flags, pkt->motNodeId,
					  pkt->srcContentId, GpIdentity.segindex,
					  pkt->srcPid, MyProcPid,
					  pkt->dstPid,
					  pkt->srcListenerPort, ((Gp_listener_port >> 16) & 0x0ffff),
					  pkt->dstListenerPort,
					  pkt->sessionId, gp_session_id,
					  pkt->icId, sliceTbl->ic_instance_id);
	}
}

/*
 * addCRC
 * 		add CRC field to the packet.
 */
static inline void
addCRC(icpkthdr *pkt)
{
	pg_crc32c	local_crc;

	INIT_CRC32C(local_crc);
	COMP_CRC32C(local_crc, pkt, pkt->len);
	FIN_CRC32C(local_crc);

	pkt->crc = local_crc;
}

/*
 * checkCRC
 * 		check the validity of the packet.
 */
static inline bool
checkCRC(icpkthdr *pkt)
{
	pg_crc32c	rx_crc,
				local_crc;

	rx_crc = pkt->crc;
	pkt->crc = 0;

	INIT_CRC32C(local_crc);
	COMP_CRC32C(local_crc, pkt, pkt->len);
	FIN_CRC32C(local_crc);

	if (rx_crc != local_crc)
	{
		return false;
	}

	return true;
}


/*
 * prepareXmit
 * 		Prepare connection for transmit.
 */
static inline void
prepareXmit(MotionConn *conn)
{
	Assert(conn != NULL);

	conn->conn_info.len = conn->msgSize;
	conn->conn_info.crc = 0;

	memcpy(conn->pBuff, &conn->conn_info, sizeof(conn->conn_info));

	/* increase the sequence no */
	conn->conn_info.seq++;

	if (gp_interconnect_full_crc)
	{
		icpkthdr   *pkt = (icpkthdr *) conn->pBuff;

		addCRC(pkt);
	}
}

/*
 * sendtoWithRetry
 * 		Retry sendto logic and send the packets.
 */
static ssize_t
sendtoWithRetry(int socket, const void *message, size_t length,
           int flags, const struct sockaddr *dest_addr,
           socklen_t dest_len, int retry, const char *errDetail)
{
	int32		n;
	int count = 0;

xmit_retry:
	/*
	 * If given retry count is positive, retry up to the limited times.
	 * Otherwise, retry for unlimited times until succeed. 
	 */
	if (retry > 0 && ++count > retry)
		return n;
	n = sendto(socket, message, length, flags, dest_addr, dest_len);
	if (n < 0)
	{
		int			save_errno = errno;

		if (errno == EINTR)
			goto xmit_retry;

		/* 
		 * EAGAIN: no space ? not an error.
		 * 
		 * EFAULT: In Linux system call, it only happens when copying a socket 
		 * address into kernel space failed, which is less likely to happen, 
		 * but mocked heavily by our fault injection in regression tests. 
		 */
		if (errno == EAGAIN || errno == EFAULT)
			return n;

		/*
		 * If Linux iptables (nf_conntrack?) drops an outgoing packet, it may
		 * return an EPERM to the application. This might be simply because of
		 * traffic shaping or congestion, so ignore it.
		 */
		if (errno == EPERM)
		{
			ereport(LOG,
					(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
					 errmsg("Interconnect error writing an outgoing packet: %m"),
					 errdetail("error during sendto() %s", errDetail)));
			return n;
		}

		ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						errmsg("Interconnect error writing an outgoing packet: %m"),
						errdetail("error during sendto() call (error:%d).\n"
								  "%s", save_errno, errDetail)));
		/* not reached */
	}

	return n;
}

/*
 * sendOnce
 * 		Send a packet.
 */
static void
sendOnce(ChunkTransportState *transportStates, ChunkTransportStateEntry *pEntry, ICBuffer *buf, MotionConn *conn)
{
	int32 n;

#ifdef USE_ASSERT_CHECKING
	if (testmode_inject_fault(gp_udpic_dropxmit_percent))
	{
#ifdef AMS_VERBOSE_LOGGING
		write_log("THROW PKT with seq %d srcpid %d despid %d", buf->pkt->seq, buf->pkt->srcPid, buf->pkt->dstPid);
#endif
		return;
	}
#endif

	char errDetail[100];
	snprintf(errDetail, sizeof(errDetail), "For Remote Connection: contentId=%d at %s",
					  conn->remoteContentId,
					  conn->remoteHostAndPort);
	n = sendtoWithRetry(pEntry->txfd, buf->pkt, buf->pkt->len, 0,
                          (struct sockaddr *) &conn->peer, conn->peer_len, -1, errDetail);
	if (n != buf->pkt->len)
	{
		if (DEBUG1 >= log_min_messages)
			write_log("Interconnect error writing an outgoing packet [seq %d]: short transmit (given %d sent %d) during sendto() call."
					  "For Remote Connection: contentId=%d at %s", buf->pkt->seq, buf->pkt->len, n,
					  conn->remoteContentId,
					  conn->remoteHostAndPort);
#ifdef AMS_VERBOSE_LOGGING
		logPkt("PKT DETAILS ", buf->pkt);
#endif
	}
	return;
}


/*
 * handleStopMsgs
 *		handle stop messages.
 *
 */
static void
handleStopMsgs(ChunkTransportState *transportStates, ChunkTransportStateEntry *pEntry, int16 motionId)
{
	int			i = 0;

#ifdef AMS_VERBOSE_LOGGING
	elog(DEBUG3, "handleStopMsgs: node %d", motionId);
#endif
	while (i < pEntry->numConns)
	{
		MotionConn *conn = NULL;

		conn = pEntry->conns + i;

#ifdef AMS_VERBOSE_LOGGING
		elog(DEBUG3, "handleStopMsgs: node %d route %d %s %s", motionId, conn->route,
			 (conn->stillActive ? "active" : "NOT active"), (conn->stopRequested ? "stop requested" : ""));
		elog(DEBUG3, "handleStopMsgs: node %d route %d msgSize %d", motionId, conn->route, conn->msgSize);
#endif

		/*
		 * MPP-2427: we're guaranteed to have recently flushed, but this might
		 * not be empty (if we got a stop on a buffer that wasn't the one we
		 * were sending) ... empty it first so the outbound buffer is empty
		 * when we get here.
		 */
		if (conn->stillActive && conn->stopRequested)
		{

			/* mark buffer empty */
			conn->tupleCount = 0;
			conn->msgSize = sizeof(conn->conn_info);

			/* now send our stop-ack EOS */
			conn->conn_info.flags |= UDPIC_FLAGS_EOS;

			Assert(conn->curBuff != NULL);

			conn->pBuff[conn->msgSize] = 'S';
			conn->msgSize += 1;

			prepareXmit(conn);

			/* now ready to actually send */
			if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
				elog(DEBUG1, "handleStopMsgs: node %d route %d, seq %d", motionId, i, conn->conn_info.seq);

			/* place it into the send queue */
			icBufferListAppend(&conn->sndQueue, conn->curBuff);

			/* return all buffers */
			icBufferListReturn(&conn->sndQueue, false);
			icBufferListReturn(&conn->unackQueue, Gp_interconnect_fc_method == INTERCONNECT_FC_METHOD_CAPACITY ? false : true);

			conn->tupleCount = 0;
			conn->msgSize = sizeof(conn->conn_info);

			conn->state = mcsEosSent;
			conn->curBuff = NULL;
			conn->pBuff = NULL;
			conn->stillActive = false;
			conn->stopRequested = false;
		}

		i++;

		if (i == pEntry->numConns)
		{
			if (pollAcks(transportStates, pEntry->txfd, 0))
			{
				if (handleAcks(transportStates, pEntry))
				{
					/* more stops found, loop again. */
					i = 0;
					continue;
				}
			}
		}
	}
}


/*
 * sendBuffers
 * 		Called by sender to send the buffers in the send queue.
 *
 * Send the buffers in the send queue of the connection if there is capacity left
 * and the congestion control condition is satisfied.
 *
 * Here, we make sure that a connection can have at least one outstanding buffer.
 * This is very important for two reasons:
 *
 * 1) The handling logic of the ack of the outstanding buffer can always send a buffer
 *    in the send queue. Otherwise, there may be a deadlock.
 * 2) This makes sure that any connection can have a minimum bandwidth for data
 *    sending.
 *
 * After sending a buffer, the buffer will be placed into both the unack queue and
 * the corresponding queue in the unack queue ring.
 */
static void
sendBuffers(ChunkTransportState *transportStates, ChunkTransportStateEntry *pEntry, MotionConn *conn)
{
	while (conn->capacity > 0 && icBufferListLength(&conn->sndQueue) > 0)
	{
		ICBuffer   *buf = NULL;

		if (Gp_interconnect_fc_method == INTERCONNECT_FC_METHOD_LOSS &&
			(icBufferListLength(&conn->unackQueue) > 0 &&
			 unack_queue_ring.numSharedOutStanding >= (snd_control_info.cwnd - snd_control_info.minCwnd)))
			break;

		/* for connection setup, we only allow one outstanding packet. */
		if (conn->state == mcsSetupOutgoingConnection && icBufferListLength(&conn->unackQueue) >= 1)
			break;

		buf = icBufferListPop(&conn->sndQueue);

		uint64		now = getCurrentTime();

		buf->sentTime = now;
		buf->unackQueueRingSlot = -1;
		buf->nRetry = 0;
		buf->conn = conn;
		conn->capacity--;

		icBufferListAppend(&conn->unackQueue, buf);

		if (Gp_interconnect_fc_method == INTERCONNECT_FC_METHOD_LOSS)
		{
			unack_queue_ring.numOutStanding++;
			if (icBufferListLength(&conn->unackQueue) > 1)
				unack_queue_ring.numSharedOutStanding++;

			putIntoUnackQueueRing(&unack_queue_ring,
								  buf,
								  computeExpirationPeriod(buf->conn, buf->nRetry),
								  now);
		}

		/*
		 * Note the place of sendOnce here. If we send before appending it to
		 * the unack queue and putting it into unack queue ring, and there is
		 * a network error occurred in the sendOnce function, error message
		 * will be output. In the time of error message output, interrupts is
		 * potentially checked, if there is a pending query cancel, it will
		 * lead to a dangled buffer (memory leak).
		 */
#ifdef TRANSFER_PROTOCOL_STATS
		updateStats(TPE_DATA_PKT_SEND, conn, buf->pkt);
#endif

		sendOnce(transportStates, pEntry, buf, conn);
		ic_statistics.sndPktNum++;

#ifdef AMS_VERBOSE_LOGGING
		logPkt("SEND PKT DETAIL", buf->pkt);
#endif

		buf->conn->sentSeq = buf->pkt->seq;
	}
}

/*
 * handleDisorderPacket
 * 		Called by rx thread to assemble and send a disorder message.
 *
 * In current implementation, we limit the number of lost packet sequence numbers
 * in the disorder message by the MIN_PACKET_SIZE. There are two reasons here:
 *
 * 1) The maximal number of lost packet sequence numbers are actually bounded by the
 *    receive queue depth whose maximal value is very large. Since we share the packet
 *    receive and ack receive in the background thread, the size of disorder should be
 *    also limited by the max packet size.
 * 2) We can use Gp_max_packet_size here to limit the number of lost packet sequence numbers.
 *    But considering we do not want to let senders send many packets when getting a lost
 *    message. Here we use MIN_PACKET_SIZE.
 *
 *
 * the format of a disorder message:
 * I) pkt header
 *  - seq      -> packet sequence number that triggers the disorder message
 *  - extraSeq -> the largest seq of the received packets
 *  - flags    -> UDPIC_FLAGS_DISORDER
 *  - len      -> sizeof(icpkthdr) + sizeof(uint32) * (lost pkt count)
 * II) content
 *  - an array of lost pkt sequence numbers (uint32)
 *
 */
static void
handleDisorderPacket(MotionConn *conn, int pos, uint32 tailSeq, icpkthdr *pkt)
{
	int			start = 0;
	uint32		lostPktCnt = 0;
	uint32	   *curSeq = (uint32 *) &rx_control_info.disorderBuffer[1];
	uint32		maxSeqs = MAX_SEQS_IN_DISORDER_ACK;

#ifdef AMS_VERBOSE_LOGGING
	write_log("PROCESS_DISORDER PKT BEGIN:");
#endif

	start = conn->pkt_q_tail;

	while (start != pos && lostPktCnt < maxSeqs)
	{
		if (conn->pkt_q[start] == NULL)
		{
			*curSeq = tailSeq;
			lostPktCnt++;
			curSeq++;

#ifdef AMS_VERBOSE_LOGGING
			write_log("PROCESS_DISORDER add seq [%d], lostPktCnt %d", *curSeq, lostPktCnt);
#endif
		}
		tailSeq++;
		start = (start + 1) % conn->pkt_q_capacity;
	}

#ifdef AMS_VERBOSE_LOGGING
	write_log("PROCESS_DISORDER PKT END:");
#endif

	/* when reaching here, cnt must not be 0 */
	sendDisorderAck(conn, pkt->seq, conn->conn_info.seq - 1, lostPktCnt);
}

/*
 * handleAckForDisorderPkt
 * 		Called by sender to deal with acks for disorder packet.
 */

static bool
handleAckForDisorderPkt(ChunkTransportState *transportStates,
						ChunkTransportStateEntry *pEntry,
						MotionConn *conn,
						icpkthdr *pkt)
{

	ICBufferLink *link = NULL;
	ICBuffer   *buf = NULL;
	ICBufferLink *next = NULL;
	uint64		now = getCurrentTime();
	uint32	   *curLostPktSeq = 0;
	int			lostPktCnt = 0;
	static uint32 times = 0;
	static uint32 lastSeq = 0;
	bool		shouldSendBuffers = false;

	if (pkt->extraSeq != lastSeq)
	{
		lastSeq = pkt->extraSeq;
		times = 0;
		return false;
	}
	else
	{
		times++;
		if (times != 2)
			return false;
	}

	curLostPktSeq = (uint32 *) &pkt[1];
	lostPktCnt = (pkt->len - sizeof(icpkthdr)) / sizeof(uint32);

	/*
	 * Resend all the missed packets and remove received packets from queues
	 */

	link = icBufferListFirst(&conn->unackQueue);
	buf = GET_ICBUFFER_FROM_PRIMARY(link);

#ifdef AMS_VERBOSE_LOGGING
	write_log("DISORDER: pktlen %d cnt %d pktseq %d first loss %d buf %p",
			  pkt->len, lostPktCnt, pkt->seq, *curLostPktSeq, buf);
	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
	{
		icBufferListLog(&conn->unackQueue);
		icBufferListLog(&conn->sndQueue);
	}
#endif

	/*
	 * iterate the unack queue
	 */
	while (!icBufferListIsHead(&conn->unackQueue, link) && buf->pkt->seq <= pkt->seq && lostPktCnt > 0)
	{
#ifdef AMS_VERBOSE_LOGGING
		write_log("DISORDER: bufseq %d curlostpkt %d cnt %d buf %p pkt->seq %d",
				  buf->pkt->seq, *curLostPktSeq, lostPktCnt, buf, pkt->seq);
#endif

		if (buf->pkt->seq == pkt->seq)
		{
			handleAckedPacket(conn, buf, now);
			shouldSendBuffers = true;
			break;
		}

		if (buf->pkt->seq == *curLostPktSeq)
		{
			/* this is a lost packet, retransmit */

			buf->nRetry++;
			if (Gp_interconnect_fc_method == INTERCONNECT_FC_METHOD_LOSS)
			{
				buf = icBufferListDelete(&unack_queue_ring.slots[buf->unackQueueRingSlot], buf);
				putIntoUnackQueueRing(&unack_queue_ring, buf,
									  computeExpirationPeriod(buf->conn, buf->nRetry), now);
			}
#ifdef TRANSFER_PROTOCOL_STATS
			updateStats(TPE_DATA_PKT_SEND, conn, buf->pkt);
#endif

			sendOnce(transportStates, pEntry, buf, buf->conn);

#ifdef AMS_VERBOSE_LOGGING
			write_log("RESEND a buffer for DISORDER: seq %d", buf->pkt->seq);
			logPkt("DISORDER RESEND DETAIL ", buf->pkt);
#endif

			ic_statistics.retransmits++;
			curLostPktSeq++;
			lostPktCnt--;

			link = link->next;
			buf = GET_ICBUFFER_FROM_PRIMARY(link);
		}
		else if (buf->pkt->seq < *curLostPktSeq)
		{
			/* remove packet already received. */

			next = link->next;
			handleAckedPacket(conn, buf, now);
			shouldSendBuffers = true;
			link = next;
			buf = GET_ICBUFFER_FROM_PRIMARY(link);
		}
		else					/* buf->pkt->seq > *curPktSeq */
		{
			/*
			 * this case is introduced when the disorder message tell you a
			 * pkt is lost. But when we handle this message, a message (for
			 * example, duplicate ack, or another disorder message) arriving
			 * before this message already removed the pkt.
			 */
			curLostPktSeq++;
			lostPktCnt--;
		}
	}
	if (Gp_interconnect_fc_method == INTERCONNECT_FC_METHOD_LOSS)
	{
		snd_control_info.ssthresh = Max(snd_control_info.cwnd / 2, snd_control_info.minCwnd);
		snd_control_info.cwnd = snd_control_info.ssthresh;
	}
#ifdef AMS_VERBOSE_LOGGING
	write_log("After DISORDER: sndQ %d unackQ %d",
			  icBufferListLength(&conn->sndQueue), icBufferListLength(&conn->unackQueue));
	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
	{
		icBufferListLog(&conn->unackQueue);
		icBufferListLog(&conn->sndQueue);
	}
#endif

	return shouldSendBuffers;
}

/*
 * handleAckForDuplicatePkt
 * 		Called by sender to deal with acks for duplicate packet.
 *
 */
static bool
handleAckForDuplicatePkt(MotionConn *conn, icpkthdr *pkt)
{
	ICBufferLink *link = NULL;
	ICBuffer   *buf = NULL;
	ICBufferLink *next = NULL;
	uint64		now = getCurrentTime();
	bool		shouldSendBuffers = false;

#ifdef AMS_VERBOSE_LOGGING
	write_log("RESEND the unacked buffers in the queue due to %s", pkt->len == 0 ? "PROCESS_START_RACE" : "DISORDER");
#endif

	if (pkt->seq <= pkt->extraSeq)
	{
		/* Indicate a bug here. */
		write_log("ERROR: invalid duplicate message: seq %d extraSeq %d", pkt->seq, pkt->extraSeq);
		return false;
	}

	link = icBufferListFirst(&conn->unackQueue);
	buf = GET_ICBUFFER_FROM_PRIMARY(link);

	/* deal with continuous pkts */
	while (!icBufferListIsHead(&conn->unackQueue, link) && (buf->pkt->seq <= pkt->extraSeq))
	{
		next = link->next;
		handleAckedPacket(conn, buf, now);
		shouldSendBuffers = true;
		link = next;
		buf = GET_ICBUFFER_FROM_PRIMARY(link);
	}

	/* deal with the single duplicate packet */
	while (!icBufferListIsHead(&conn->unackQueue, link) && buf->pkt->seq <= pkt->seq)
	{
		next = link->next;
		if (buf->pkt->seq == pkt->seq)
		{
			handleAckedPacket(conn, buf, now);
			shouldSendBuffers = true;
			break;
		}
		link = next;
		buf = GET_ICBUFFER_FROM_PRIMARY(link);
	}

	return shouldSendBuffers;
}

/*
 * checkNetworkTimeout
 *		check network timeout case.
 */
static inline void
checkNetworkTimeout(ICBuffer *buf, uint64 now, bool *networkTimeoutIsLogged)
{
	/*
	 * Using only the time to first sent time to decide timeout is not enough,
	 * since there is a possibility the sender process is not scheduled or
	 * blocked by OS for a long time. In this case, only a few times are
	 * tried. Thus, the GUC Gp_interconnect_min_retries_before_timeout is
	 * added here.
	 */
	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG &&
		buf->nRetry % Gp_interconnect_debug_retry_interval == 0)
	{
		ereport(LOG,
				(errmsg("resending packet (seq %d) to %s (pid %d cid %d) with %d retries in %lu seconds",
						buf->pkt->seq, buf->conn->remoteHostAndPort,
						buf->pkt->dstPid, buf->pkt->dstContentId, buf->nRetry,
						(now - buf->sentTime) / 1000 / 1000)));
	}

	if ((buf->nRetry > Gp_interconnect_min_retries_before_timeout) && (now - buf->sentTime) > ((uint64) Gp_interconnect_transmit_timeout * 1000 * 1000))
	{
		ereport(ERROR,
				(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
				 errmsg("interconnect encountered a network error, please check your network"),
				 errdetail("Failed to send packet (seq %u) to %s (pid %d cid %d) after %u retries in %d seconds.",
						   buf->pkt->seq, buf->conn->remoteHostAndPort,
						   buf->pkt->dstPid, buf->pkt->dstContentId,
						   buf->nRetry, Gp_interconnect_transmit_timeout)));
	}

	/*
	 * Default value of Gp_interconnect_transmit_timeout is one hours.
	 * It taks too long time to detect a network error and it is not user friendly.
	 *
	 * Packets would be dropped repeatly on some specific ports. We'd better have
	 * a warning messgage for this case and give the DBA a chance to detect this error
	 * earlier. Since packets would also be dropped when network is bad, we should not
	 * error out here, but just give a warning message. Erroring our is still handled
	 * by GUC Gp_interconnect_transmit_timeout as above. Note that warning message should
	 * be printed for each statement only once.
	 */
	if ((buf->nRetry >= Gp_interconnect_min_retries_before_timeout) && !(*networkTimeoutIsLogged))
	{
		ereport(WARNING,
				(errmsg("interconnect may encountered a network error, please check your network"),
				 errdetail("Failing to send packet (seq %u) to %s (pid %d cid %d) after %u retries.",
						   buf->pkt->seq, buf->conn->remoteHostAndPort,
						   buf->pkt->dstPid, buf->pkt->dstContentId,
						   buf->nRetry)));
		*networkTimeoutIsLogged = true;
	}
}

/*
 * checkExpiration
 * 		Check whether packets expire. If a packet expires, resend the packet,
 * 		and adjust its position in the unack queue ring.
 *
 */
static void
checkExpiration(ChunkTransportState *transportStates,
				ChunkTransportStateEntry *pEntry,
				MotionConn *triggerConn,
				uint64 now)
{
	/* check for expiration */
	int			count = 0;
	int			retransmits = 0;

	Assert(unack_queue_ring.currentTime != 0);
	while (now >= (unack_queue_ring.currentTime + TIMER_SPAN) && count++ < UNACK_QUEUE_RING_SLOTS_NUM)
	{
		/* expired, need to resend them */
		ICBuffer   *curBuf = NULL;

		while ((curBuf = icBufferListPop(&unack_queue_ring.slots[unack_queue_ring.idx])) != NULL)
		{
			curBuf->nRetry++;
			putIntoUnackQueueRing(
								  &unack_queue_ring,
								  curBuf,
								  computeExpirationPeriod(curBuf->conn, curBuf->nRetry), now);

#ifdef TRANSFER_PROTOCOL_STATS
			updateStats(TPE_DATA_PKT_SEND, curBuf->conn, curBuf->pkt);
#endif

			sendOnce(transportStates, pEntry, curBuf, curBuf->conn);

			retransmits++;
			ic_statistics.retransmits++;
			curBuf->conn->stat_count_resent++;
			curBuf->conn->stat_max_resent = Max(curBuf->conn->stat_max_resent,
												curBuf->conn->stat_count_resent);

			checkNetworkTimeout(curBuf, now, &transportStates->networkTimeoutIsLogged);

#ifdef AMS_VERBOSE_LOGGING
			write_log("RESEND pkt with seq %d (retry %d, rtt " UINT64_FORMAT ") to route %d",
					  curBuf->pkt->seq, curBuf->nRetry, curBuf->conn->rtt, curBuf->conn->route);
			logPkt("RESEND PKT in checkExpiration", curBuf->pkt);
#endif
		}

		unack_queue_ring.currentTime += TIMER_SPAN;
		unack_queue_ring.idx = (unack_queue_ring.idx + 1) % (UNACK_QUEUE_RING_SLOTS_NUM);
	}

	/*
	 * deal with case when there is a long time this function is not called.
	 */
	unack_queue_ring.currentTime = now - (now % TIMER_SPAN);
	if (retransmits > 0)
	{
		snd_control_info.ssthresh = Max(snd_control_info.cwnd / 2, snd_control_info.minCwnd);
		snd_control_info.cwnd = snd_control_info.minCwnd;
	}
}

/*
 * checkDeadlock
 * 		Check whether deadlock occurs on a connection.
 *
 * What this function does is to send a status query message to rx thread when
 * the connection has not received any acks for some time. This is to avoid
 * potential deadlock when there are continuous ack losses. Packet resending
 * logic does not help avoiding deadlock here since the packets in the unack
 * queue may already been removed when the sender knows that they have been
 * already buffered in the receiver side queue.
 *
 * Some considerations on deadlock check time period:
 *
 * Potential deadlock occurs rarely. According to our experiments on various
 * workloads and hardware. It occurred only when fault injection is enabled
 * and a large number packets and acknowledgments are discarded. Thus, here we
 * use a relatively large deadlock check period.
 *
 */
static void
checkDeadlock(ChunkTransportStateEntry *pEntry, MotionConn *conn)
{
	uint64		deadlockCheckTime;

	if (icBufferListLength(&conn->unackQueue) == 0 && conn->capacity == 0 && icBufferListLength(&conn->sndQueue) > 0)
	{
		/* we must have received some acks before deadlock occurs. */
		Assert(conn->deadlockCheckBeginTime > 0);

#ifdef USE_ASSERT_CHECKING
		if (udp_testmode)
		{
			deadlockCheckTime = 100000;
		}
		else
#endif
		{
			deadlockCheckTime = DEADLOCK_CHECKING_TIME;
		}

		uint64		now = getCurrentTime();

		/* request the capacity to avoid the deadlock case */
		if (((now - ic_control_info.lastDeadlockCheckTime) > deadlockCheckTime) &&
			((now - conn->deadlockCheckBeginTime) > deadlockCheckTime))
		{
			sendStatusQueryMessage(conn, pEntry->txfd, conn->conn_info.seq - 1);
			ic_control_info.lastDeadlockCheckTime = now;
			ic_statistics.statusQueryMsgNum++;

			/* check network error. */
			if ((now - conn->deadlockCheckBeginTime) > ((uint64) Gp_interconnect_transmit_timeout * 1000 * 1000))
			{
				ereport(ERROR,
						(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						 errmsg("interconnect encountered a network error, please check your network"),
						 errdetail("Did not get any response from %s (pid %d cid %d) in %d seconds.",
								   conn->remoteHostAndPort,
								   conn->conn_info.dstPid,
								   conn->conn_info.dstContentId,
								   Gp_interconnect_transmit_timeout)));
			}
		}
	}
}

/*
 * pollAcks
 * 		Timeout polling of acks
 */
static inline bool
pollAcks(ChunkTransportState *transportStates, int fd, int timeout)
{
	struct pollfd nfd;
	int			n;

	nfd.fd = fd;
	nfd.events = POLLIN;

	n = poll(&nfd, 1, timeout);
	if (n < 0)
	{
		ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);
		if (errno == EINTR)
			return false;

		ereport(ERROR,
				(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
				 errmsg("interconnect error waiting for peer ack"),
				 errdetail("During poll() call.")));

		/* not reached */
	}

	if (n == 0)					/* timeout */
	{
		return false;
	}

	/* got an ack to handle (possibly a stop message) */
	if (n == 1 && (nfd.events & POLLIN))
	{
		return true;
	}

	return false;

}

/*
 * updateRetransmitStatistics
 * 		Update the retransmit statistics.
 */
static inline void
updateRetransmitStatistics(MotionConn *conn)
{
	ic_statistics.retransmits++;
	conn->stat_count_resent++;
	conn->stat_max_resent = Max(conn->stat_max_resent, conn->stat_count_resent);
}

/*
 * checkExpirationCapacityFC
 * 		Check expiration for capacity based flow control method.
 *
 */
static void
checkExpirationCapacityFC(ChunkTransportState *transportStates,
						  ChunkTransportStateEntry *pEntry,
						  MotionConn *conn,
						  int timeout)
{
	if (icBufferListLength(&conn->unackQueue) == 0)
		return;

	uint64		now = getCurrentTime();
	uint64		elapsed = now - ic_control_info.lastPacketSendTime;

	if (elapsed >= ((uint64) timeout * 1000))
	{
		ICBufferLink *bufLink = icBufferListFirst(&conn->unackQueue);
		ICBuffer   *buf = GET_ICBUFFER_FROM_PRIMARY(bufLink);

		sendOnce(transportStates, pEntry, buf, buf->conn);
		buf->nRetry++;
		ic_control_info.lastPacketSendTime = now;

		updateRetransmitStatistics(conn);
		checkNetworkTimeout(buf, now, &transportStates->networkTimeoutIsLogged);
	}
}

/*
 * checkExceptions
 * 		Check exceptions including packet expiration, deadlock, bg thread error, NIC failure...
 * 		Caller should start from 0 with retry, so that the expensive check for deadlock and
 * 		QD connection can be avoided in a healthy state.
 */
static void
checkExceptions(ChunkTransportState *transportStates,
				ChunkTransportStateEntry *pEntry,
				MotionConn *conn,
				int retry,
				int timeout)
{
	if (Gp_interconnect_fc_method == INTERCONNECT_FC_METHOD_CAPACITY	/* || conn->state ==
																		 * mcsSetupOutgoingConnection
		  * */ )
	{
		checkExpirationCapacityFC(transportStates, pEntry, conn, timeout);
	}

	if (Gp_interconnect_fc_method == INTERCONNECT_FC_METHOD_LOSS)
	{
		uint64		now = getCurrentTime();

		if (now - ic_control_info.lastExpirationCheckTime > TIMER_CHECKING_PERIOD)
		{
			checkExpiration(transportStates, pEntry, conn, now);
			ic_control_info.lastExpirationCheckTime = now;
		}
	}

	if ((retry & 0x3) == 2)
	{
		checkDeadlock(pEntry, conn);
		checkRxThreadError();
		ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);
	}

	/*
	 * 1. NIC on master (and thus the QD connection) may become bad, check it.
	 * 2. Postmaster may become invalid, check it
	 *
	 * We check modulo 2 to correlate with the deadlock check above at the
	 * initial iteration.
	 */
	if ((retry & 0x3f) == 2)
	{
		checkQDConnectionAlive();

		if (!PostmasterIsAlive())
			ereport(FATAL,
					(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
					 errmsg("interconnect failed to send chunks"),
					 errdetail("Postmaster is not alive.")));
	}
}

/*
 * computeTimeout
 * 		Compute timeout value in ms.
 */
static inline int
computeTimeout(MotionConn *conn, int retry)
{
	if (icBufferListLength(&conn->unackQueue) == 0)
		return TIMER_CHECKING_PERIOD;

	ICBufferLink *bufLink = icBufferListFirst(&conn->unackQueue);
	ICBuffer   *buf = GET_ICBUFFER_FROM_PRIMARY(bufLink);

	if (buf->nRetry == 0 && retry == 0)
		return 0;

	if (Gp_interconnect_fc_method == INTERCONNECT_FC_METHOD_LOSS)
		return TIMER_CHECKING_PERIOD;

	/* for capacity based flow control */
	return TIMEOUT(buf->nRetry);
}

/*
 * SendChunkUDPIFC
 * 		is used to send a tcItem to a single destination. Tuples often are
 * 		*very small* we aggregate in our local buffer before sending into the kernel.
 *
 * PARAMETERS
 *	 conn - MotionConn that the tcItem is to be sent to.
 *	 tcItem - message to be sent.
 *	 motionId - Node Motion Id.
 */
static bool
SendChunkUDPIFC(ChunkTransportState *transportStates,
				ChunkTransportStateEntry *pEntry,
				MotionConn *conn,
				TupleChunkListItem tcItem,
				int16 motionId)
{

	int			length = tcItem->chunk_length;
	int			retry = 0;
	bool		doCheckExpiration = false;
	bool		gotStops = false;

	Assert(conn->msgSize > 0);

#ifdef AMS_VERBOSE_LOGGING
	elog(DEBUG3, "sendChunk: msgSize %d this chunk length %d conn seq %d",
		 conn->msgSize, tcItem->chunk_length, conn->conn_info.seq);
#endif

	if (conn->msgSize + length <= Gp_max_packet_size)
	{
		memcpy(conn->pBuff + conn->msgSize, tcItem->chunk_data, tcItem->chunk_length);
		conn->msgSize += length;

		conn->tupleCount++;
		return true;
	}

	/* prepare this for transmit */

	ic_statistics.totalCapacity += conn->capacity;
	ic_statistics.capacityCountingTime++;

	/* try to send it */

	prepareXmit(conn);

	icBufferListAppend(&conn->sndQueue, conn->curBuff);
	sendBuffers(transportStates, pEntry, conn);

	uint64		now = getCurrentTime();

	if (Gp_interconnect_fc_method == INTERCONNECT_FC_METHOD_CAPACITY)
		doCheckExpiration = false;
	else
		doCheckExpiration = (now - ic_control_info.lastExpirationCheckTime) > MAX_TIME_NO_TIMER_CHECKING ? true : false;

	/* get a new buffer */
	conn->curBuff = NULL;
	conn->pBuff = NULL;

	ic_control_info.lastPacketSendTime = 0;
	conn->deadlockCheckBeginTime = now;

	while (doCheckExpiration || (conn->curBuff = getSndBuffer(conn)) == NULL)
	{
		int			timeout = (doCheckExpiration ? 0 : computeTimeout(conn, retry));

		if (pollAcks(transportStates, pEntry->txfd, timeout))
		{
			if (handleAcks(transportStates, pEntry))
			{
				/*
				 * We make sure that we deal with the stop messages only after
				 * we get a buffer. Otherwise, if the stop message is not for
				 * this connection, this will lead to an error for the
				 * following data sending of this connection.
				 */
				gotStops = true;
			}
		}
		checkExceptions(transportStates, pEntry, conn, retry++, timeout);
		doCheckExpiration = false;
	}

	conn->pBuff = (uint8 *) conn->curBuff->pkt;

	if (gotStops)
	{
		/* handling stop message will make some connection not active anymore */
		handleStopMsgs(transportStates, pEntry, motionId);
		if (!conn->stillActive)
			return true;
	}

	/* reinitialize connection */
	conn->tupleCount = 0;
	conn->msgSize = sizeof(conn->conn_info);

	/* now we can copy the input to the new buffer */
	memcpy(conn->pBuff + conn->msgSize, tcItem->chunk_data, tcItem->chunk_length);
	conn->msgSize += length;

	conn->tupleCount++;

	return true;
}

/*
 * SendEosUDPIFC
 * 		broadcast eos messages to receivers.
 *
 * See ml_ipc.h
 *
 */
static void
SendEosUDPIFC(ChunkTransportState *transportStates,
			  int motNodeID,
			  TupleChunkListItem tcItem)
{
	ChunkTransportStateEntry *pEntry = NULL;
	MotionConn *conn;
	int			i = 0;
	int			retry = 0;
	int			activeCount = 0;
	int			timeout = 0;

	if (!transportStates)
	{
		elog(FATAL, "SendEosUDPIFC: missing interconnect context.");
	}
	else if (!transportStates->activated && !transportStates->teardownActive)
	{
		elog(FATAL, "SendEosUDPIFC: context and teardown inactive.");
	}
#ifdef AMS_VERBOSE_LOGGING
	elog(LOG, "entering seneosudp");
#endif

	/* check em' */
	ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);

	getChunkTransportState(transportStates, motNodeID, &pEntry);

	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		elog(DEBUG1, "Interconnect seg%d slice%d sending end-of-stream to slice%d",
			 GpIdentity.segindex, motNodeID, pEntry->recvSlice->sliceIndex);

	/*
	 * we want to add our tcItem onto each of the outgoing buffers -- this is
	 * guaranteed to leave things in a state where a flush is *required*.
	 */
	doBroadcast(transportStates, pEntry, tcItem, NULL);

	pEntry->sendingEos = true;

	uint64		now = getCurrentTime();

	/* now flush all of the buffers. */
	for (i = 0; i < pEntry->numConns; i++)
	{
		conn = pEntry->conns + i;

		if (conn->stillActive)
		{
			if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
				elog(DEBUG1, "sent eos to route %d tuplecount %d seq %d flags 0x%x stillActive %s icId %d %d",
					 conn->route, conn->tupleCount, conn->conn_info.seq,
					 conn->conn_info.flags, (conn->stillActive ? "true" : "false"),
					 conn->conn_info.icId, conn->msgSize);

			/* prepare this for transmit */
			if (pEntry->sendingEos)
				conn->conn_info.flags |= UDPIC_FLAGS_EOS;

			prepareXmit(conn);

			/* place it into the send queue */
			icBufferListAppend(&conn->sndQueue, conn->curBuff);
			sendBuffers(transportStates, pEntry, conn);

			conn->tupleCount = 0;
			conn->msgSize = sizeof(conn->conn_info);
			conn->curBuff = NULL;
			conn->deadlockCheckBeginTime = now;

			activeCount++;
		}
	}

	/*
	 * Now waiting for acks from receivers.
	 *
	 * Note here waiting is done in a separate phase from the EOS sending
	 * phase to make the processing faster when a lot of connections are slow
	 * and have frequent packet losses. In fault injection tests, we found
	 * this.
	 *
	 */

	while (activeCount > 0)
	{
		activeCount = 0;

		for (i = 0; i < pEntry->numConns; i++)
		{
			conn = pEntry->conns + i;

			if (conn->stillActive)
			{
				retry = 0;
				ic_control_info.lastPacketSendTime = 0;

				/* wait until this queue is emptied */
				while (icBufferListLength(&conn->unackQueue) > 0 ||
					   icBufferListLength(&conn->sndQueue) > 0)
				{
					timeout = computeTimeout(conn, retry);

					if (pollAcks(transportStates, pEntry->txfd, timeout))
						handleAcks(transportStates, pEntry);

					checkExceptions(transportStates, pEntry, conn, retry++, timeout);

					if (retry >= MAX_TRY)
						break;
				}

				if ((!conn->cdbProc) || (icBufferListLength(&conn->unackQueue) == 0 &&
										 icBufferListLength(&conn->sndQueue) == 0))
				{
					conn->state = mcsEosSent;
					conn->stillActive = false;
				}
				else
					activeCount++;
			}
		}
	}

	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		elog(DEBUG1, "SendEosUDPIFC leaving, activeCount %d", activeCount);
}

/*
 * doSendStopMessageUDPIFC
 * 		Send stop messages to all senders.
 */
static void
doSendStopMessageUDPIFC(ChunkTransportState *transportStates, int16 motNodeID)
{
	ChunkTransportStateEntry *pEntry = NULL;
	MotionConn *conn = NULL;
	int			i;

	if (!transportStates->activated)
		return;

	getChunkTransportState(transportStates, motNodeID, &pEntry);
	Assert(pEntry);

	/*
	 * Note: we're only concerned with receivers here.
	 */
	pthread_mutex_lock(&ic_control_info.lock);

	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		elog(DEBUG1, "Interconnect needs no more input from slice%d; notifying senders to stop.",
			 motNodeID);

	for (i = 0; i < pEntry->numConns; i++)
	{
		conn = pEntry->conns + i;

		/*
		 * Note here, the stillActive flag of a connection may have been set
		 * to false by markUDPConnInactiveIFC.
		 */
		if (conn->stillActive)
		{
			if (conn->conn_info.flags & UDPIC_FLAGS_EOS)
			{
				/*
				 * we have a queued packet that has EOS in it. We've acked it,
				 * so we're done
				 */
				if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
					elog(DEBUG1, "do sendstop: already have queued EOS packet, we're done. node %d route %d",
						 motNodeID, i);

				conn->stillActive = false;

				/* need to drop the queues in the teardown function. */
				while (conn->pkt_q_size > 0)
				{
					putRxBufferAndSendAck(conn, NULL);
				}
			}
			else
			{
				conn->stopRequested = true;
				conn->conn_info.flags |= UDPIC_FLAGS_STOP;

				/*
				 * The peer addresses for incoming connections will not be set
				 * until the first packet has arrived. However, when the lower
				 * slice does not have data to send, the corresponding peer
				 * address for the incoming connection will never be set. We
				 * will skip sending ACKs to those connections.
				 */

#ifdef FAULT_INJECTOR
				if (FaultInjector_InjectFaultIfSet(
												   "interconnect_stop_ack_is_lost",
												   DDLNotSpecified,
												   "" /* databaseName */ ,
												   "" /* tableName */ ) == FaultInjectorTypeSkip)
				{
					continue;
				}
#endif

				if (conn->peer.ss_family == AF_INET || conn->peer.ss_family == AF_INET6)
				{
					uint32		seq = conn->conn_info.seq > 0 ? conn->conn_info.seq - 1 : 0;

					sendAck(conn, UDPIC_FLAGS_STOP | UDPIC_FLAGS_ACK | UDPIC_FLAGS_CAPACITY | conn->conn_info.flags, seq, seq);

					if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
						elog(DEBUG1, "sent stop message. node %d route %d seq %d", motNodeID, i, seq);
				}
				else
				{
					if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
						elog(DEBUG1, "first packet did not arrive yet. don't sent stop message. node %d route %d",
							 motNodeID, i);
				}
			}
		}
	}
	pthread_mutex_unlock(&ic_control_info.lock);
}

/*
 * dispatcherAYT
 * 		Check the connection from the dispatcher to verify that it is still there.
 * 		We do this by calling recv() to receive 1 byte.
 *
 * The connection is a struct Port, stored in the global MyProcPort.
 *
 * ERROR out if the connection was closed or if we encountered an unrecoverable
 * error trying to recv().
 */
static void
dispatcherAYT(void)
{
	ssize_t		ret;
	char		buf;

	/*
	 * For background worker or auxiliary process like gdd, there is no client.
	 * As a result, MyProcPort is NULL. We should skip dispatcherAYT check here.
	 */
	if (MyProcPort == NULL)
		return;

	if (MyProcPort->sock == PGINVALID_SOCKET)
	{
		ereport(ERROR,
				(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
					errmsg("backend socket is invalid (recv)"),
					errdetail("it could already have been closed")));
	}

#ifndef WIN32
	ret = recv(MyProcPort->sock, &buf, 1, MSG_PEEK | MSG_DONTWAIT);
#else
	ret = recv(MyProcPort->sock, &buf, 1, MSG_PEEK | MSG_PARTIAL);
#endif

	if (ret == 0)				/* socket has been closed. EOF */
	{
		ereport(ERROR,
				(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
					errmsg("dispatch connection lost (recv)"),
					errdetail("peer socket has been closed, eof received")));
	}

	if (ret == -1)
	{
		if (errno == EAGAIN || errno == EINPROGRESS)
			return;		/* connection intact, no data available */
		else			/* unrecoverable error */
			ereport(ERROR,
					(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						errmsg("dispatch connection lost (recv): %m")));
	}

	/* data waiting on socket, it must be OK. */
}

/*
 * checkQDConnectionAlive
 * 		Check whether QD connection is still alive. If not, report error.
 * 		Do nothing if we are the QD, or if we are in utility mode.
 */
static void
checkQDConnectionAlive(void)
{
	if (Gp_role == GP_ROLE_EXECUTE)
		dispatcherAYT();
}

/*
 * getCurrentTime
 * 		get current time
 *
 */
static uint64
getCurrentTime(void)
{
	struct timeval newTime;
	int			status = 1;
	uint64		t = 0;

#if HAVE_LIBRT
	/* Use clock_gettime to return monotonic time value. */
	struct timespec ts;

	status = clock_gettime(CLOCK_MONOTONIC, &ts);

	newTime.tv_sec = ts.tv_sec;
	newTime.tv_usec = ts.tv_nsec / 1000;

#endif

	if (status != 0)
		gettimeofday(&newTime, NULL);

	t = ((uint64) newTime.tv_sec) * USECS_PER_SECOND + newTime.tv_usec;
	return t;
}

/*
 * putIntoUnackQueueRing
 * 		Put the buffer into the ring.
 *
 * expTime - expiration time from now
 *
 */
static void
putIntoUnackQueueRing(UnackQueueRing *uqr, ICBuffer *buf, uint64 expTime, uint64 now)
{
	uint64		diff = 0;
	int			idx = 0;

	/* The first packet, currentTime is not initialized */
	if (uqr->currentTime == 0)
		uqr->currentTime = now - (now % TIMER_SPAN);

	diff = now + expTime - uqr->currentTime;
	if (diff >= UNACK_QUEUE_RING_LENGTH)
	{
#ifdef AMS_VERBOSE_LOGGING
		write_log("putIntoUnackQueueRing:" "now " UINT64_FORMAT "expTime " UINT64_FORMAT "diff " UINT64_FORMAT "uqr-currentTime " UINT64_FORMAT, now, expTime, diff, uqr->currentTime);
#endif
		diff = UNACK_QUEUE_RING_LENGTH - 1;
	}
	else if (diff < TIMER_SPAN)
	{
		diff = TIMER_SPAN;
	}

	idx = (uqr->idx + diff / TIMER_SPAN) % UNACK_QUEUE_RING_SLOTS_NUM;

#ifdef AMS_VERBOSE_LOGGING
	write_log("PUTTW: curtime " UINT64_FORMAT " now " UINT64_FORMAT " (diff " UINT64_FORMAT ") expTime " UINT64_FORMAT " previdx %d, nowidx %d, nextidx %d", uqr->currentTime, now, diff, expTime, buf->unackQueueRingSlot, uqr->idx, idx);
#endif

	buf->unackQueueRingSlot = idx;
	icBufferListAppend(&unack_queue_ring.slots[idx], buf);
}

/*
 * handleDataPacket
 * 		Handling the data packet.
 *
 * On return, will set *wakeup_mainthread, if a packet was received successfully
 * and the caller should wake up the main thread, after releasing the mutex.
 */
static bool
handleDataPacket(MotionConn *conn, icpkthdr *pkt, struct sockaddr_storage *peer, socklen_t *peerlen,
				 AckSendParam *param, bool *wakeup_mainthread)
{

	if ((pkt->len == sizeof(icpkthdr)) && (pkt->flags & UDPIC_FLAGS_CAPACITY))
	{
		if (DEBUG1 >= log_min_messages)
			write_log("status queuy message received, seq %d, srcpid %d, dstpid %d, icid %d, sid %d", pkt->seq, pkt->srcPid, pkt->dstPid, pkt->icId, pkt->sessionId);

#ifdef AMS_VERBOSE_LOGGING
		logPkt("STATUS QUERY MESSAGE", pkt);
#endif
		uint32		seq = conn->conn_info.seq > 0 ? conn->conn_info.seq - 1 : 0;
		uint32		extraSeq = conn->stopRequested ? seq : conn->conn_info.extraSeq;

		setAckSendParam(param, conn, UDPIC_FLAGS_CAPACITY | UDPIC_FLAGS_ACK | conn->conn_info.flags, seq, extraSeq);

		return false;
	}

	/*
	 * when we're not doing a full-setup on every statement, we've got to
	 * update the peer info -- full setups do this at setup-time.
	 */

	/*
	 * Note the change here, for process start race and disordered message, if
	 * we do not fill in peer address, then we may send some acks to unknown
	 * address. Thus, the following condition is used.
	 *
	 */
	if (pkt->seq <= conn->pkt_q_capacity)
	{
		/* fill in the peer.  Need to cast away "volatile".  ugly */
		memset((void *) &conn->peer, 0, sizeof(conn->peer));
		memcpy((void *) &conn->peer, peer, *peerlen);
		conn->peer_len = *peerlen;

		conn->conn_info.dstListenerPort = pkt->dstListenerPort;
		if (DEBUG2 >= log_min_messages)
			write_log("received the head packets when eliding setup, pkt seq %d", pkt->seq);
	}

	/* data packet */
	if (pkt->flags & UDPIC_FLAGS_EOS)
	{
		if (DEBUG3 >= log_min_messages)
			write_log("received packet with EOS motid %d route %d seq %d",
					  pkt->motNodeId, conn->route, pkt->seq);
	}

	/*
	 * if we got a stop, but didn't request a stop -- ignore, this is a
	 * startup blip: we must have acked with a stop -- we don't want to do
	 * anything further with the stop-message if we didn't request a stop!
	 *
	 * this is especially important after eliding setup is enabled.
	 */
	if (!conn->stopRequested && (pkt->flags & UDPIC_FLAGS_STOP))
	{
		if (pkt->flags & UDPIC_FLAGS_EOS)
		{
			write_log("non-requested stop flag, EOS! seq %d, flags 0x%x", pkt->seq, pkt->flags);
		}
		return false;
	}

	if (conn->stopRequested && conn->stillActive)
	{
		if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG && DEBUG5 >= log_min_messages)
			write_log("rx_thread got packet on active connection marked stopRequested. "
					  "(flags 0x%x) node %d route %d pkt seq %d conn seq %d",
					  pkt->flags, pkt->motNodeId, conn->route, pkt->seq, conn->conn_info.seq);

		/* can we update stillActive ? */
		if (DEBUG2 >= log_min_messages)
			if (!(pkt->flags & UDPIC_FLAGS_STOP) &&
				!(pkt->flags & UDPIC_FLAGS_EOS))
				write_log("stop requested but no stop flag on return packet ?!");

		if (pkt->flags & UDPIC_FLAGS_EOS)
			conn->conn_info.flags |= UDPIC_FLAGS_EOS;

		if (conn->conn_info.seq < pkt->seq)
			conn->conn_info.seq = pkt->seq; /* note here */

		setAckSendParam(param, conn, UDPIC_FLAGS_ACK | UDPIC_FLAGS_STOP | UDPIC_FLAGS_CAPACITY | conn->conn_info.flags, pkt->seq, pkt->seq);

		/* we only update stillActive if eos has been sent by peer. */
		if (pkt->flags & UDPIC_FLAGS_EOS)
		{
			if (DEBUG2 >= log_min_messages)
				write_log("stop requested and acknowledged by sending peer");
			conn->stillActive = false;
		}

		return false;
	}

	/* dropped ack or timeout */
	if (pkt->seq < conn->conn_info.seq)
	{
		ic_statistics.duplicatedPktNum++;
		if (DEBUG3 >= log_min_messages)
			write_log("dropped ack ? ignored data packet w/ cmd %d conn->cmd %d node %d route %d seq %d expected %d flags 0x%x",
					  pkt->icId, conn->conn_info.icId, pkt->motNodeId,
					  conn->route, pkt->seq, conn->conn_info.seq, pkt->flags);
		setAckSendParam(param, conn, UDPIC_FLAGS_ACK | UDPIC_FLAGS_CAPACITY | conn->conn_info.flags, conn->conn_info.seq - 1, conn->conn_info.extraSeq);

		return false;
	}

	/* sequence number is correct */
	if (!conn->stillActive)
	{
		/* peer may have dropped ack */
		if (gp_log_interconnect >= GPVARS_VERBOSITY_VERBOSE &&
			DEBUG1 >= log_min_messages)
			write_log("received on inactive connection node %d route %d (seq %d pkt->seq %d)",
					  pkt->motNodeId, conn->route, conn->conn_info.seq, pkt->seq);
		if (conn->conn_info.seq < pkt->seq)
			conn->conn_info.seq = pkt->seq;
		setAckSendParam(param, conn, UDPIC_FLAGS_ACK | UDPIC_FLAGS_STOP | UDPIC_FLAGS_CAPACITY | conn->conn_info.flags, pkt->seq, pkt->seq);

		return false;
	}

	/* headSeq is the seq for the head packet. */
	uint32		headSeq = conn->conn_info.seq - conn->pkt_q_size;

	if ((conn->pkt_q_size == conn->pkt_q_capacity) || (pkt->seq - headSeq >= conn->pkt_q_capacity))
	{
		/*
		 * Error case: NO RX SPACE or out of range pkt This indicates a bug.
		 */
		logPkt("Interconnect error: received a packet when the queue is full ", pkt);
		ic_statistics.disorderedPktNum++;
		conn->stat_count_dropped++;
		return false;
	}

	/* put the packet at the his position */
	bool		toWakeup = false;

	int			pos = (pkt->seq - 1) % conn->pkt_q_capacity;

	if (conn->pkt_q[pos] == NULL)
	{
		conn->pkt_q[pos] = (uint8 *) pkt;
		if (pos == conn->pkt_q_head)
		{
#ifdef AMS_VERBOSE_LOGGING
			write_log("SAVE pkt at QUEUE HEAD [seq %d] for node %d route %d, queue head seq %d, queue size %d, queue head %d queue tail %d", pkt->seq, pkt->motNodeId, conn->route, headSeq, conn->pkt_q_size, conn->pkt_q_head, conn->pkt_q_tail);
#endif
			toWakeup = true;
		}

		if (pos == conn->pkt_q_tail)
		{
			/* move the queue tail */
			for (; conn->pkt_q[conn->pkt_q_tail] != NULL && conn->pkt_q_size < conn->pkt_q_capacity;)
			{
				conn->pkt_q_size++;
				conn->pkt_q_tail = (conn->pkt_q_tail + 1) % conn->pkt_q_capacity;
				conn->conn_info.seq++;
			}

			/* set the EOS flag */
			if (((icpkthdr *) (conn->pkt_q[(conn->pkt_q_tail + conn->pkt_q_capacity - 1) % conn->pkt_q_capacity]))->flags & UDPIC_FLAGS_EOS)
			{
				conn->conn_info.flags |= UDPIC_FLAGS_EOS;
				if (DEBUG1 >= log_min_messages)
					write_log("RX_THREAD: the packet with EOS flag is available for access in the queue for route %d", conn->route);
			}

			/* ack data packet */
			setAckSendParam(param, conn, UDPIC_FLAGS_CAPACITY | UDPIC_FLAGS_ACK | conn->conn_info.flags, conn->conn_info.seq - 1, conn->conn_info.extraSeq);

#ifdef AMS_VERBOSE_LOGGING
			write_log("SAVE conn %p pkt at QUEUE TAIL [seq %d] at pos [%d] for node %d route %d, [head seq] %d, queue size %d, queue head %d queue tail %d", conn, pkt->seq, pos, pkt->motNodeId, conn->route, headSeq, conn->pkt_q_size, conn->pkt_q_head, conn->pkt_q_tail);
#endif
		}
		else					/* deal with out-of-order packet */
		{
			if (DEBUG1 >= log_min_messages)
				write_log("SAVE conn %p OUT-OF-ORDER pkt [seq %d] at pos [%d] for node %d route %d, [head seq] %d, queue size %d, queue head %d queue tail %d", conn, pkt->seq, pos, pkt->motNodeId, conn->route, headSeq, conn->pkt_q_size, conn->pkt_q_head, conn->pkt_q_tail);

			/* send an ack for out-of-order packet */
			ic_statistics.disorderedPktNum++;
			handleDisorderPacket(conn, pos, headSeq + conn->pkt_q_size, pkt);
		}
	}
	else						/* duplicate pkt */
	{
		if (DEBUG1 >= log_min_messages)
			write_log("DUPLICATE pkt [seq %d], [head seq] %d, queue size %d, queue head %d queue tail %d", pkt->seq, headSeq, conn->pkt_q_size, conn->pkt_q_head, conn->pkt_q_tail);

		setAckSendParam(param, conn, UDPIC_FLAGS_DUPLICATE | conn->conn_info.flags, pkt->seq, conn->conn_info.seq - 1);
		ic_statistics.duplicatedPktNum++;
		return false;
	}

	/* Was the main thread waiting for something ? */
	if (rx_control_info.mainWaitingState.waiting &&
		rx_control_info.mainWaitingState.waitingNode == pkt->motNodeId &&
		rx_control_info.mainWaitingState.waitingQuery == pkt->icId && toWakeup)
	{
		if (rx_control_info.mainWaitingState.waitingRoute == ANY_ROUTE)
		{
			if (rx_control_info.mainWaitingState.reachRoute == ANY_ROUTE)
				rx_control_info.mainWaitingState.reachRoute = conn->route;
		}
		else if (rx_control_info.mainWaitingState.waitingRoute == conn->route)
		{
			if (DEBUG2 >= log_min_messages)
				write_log("rx thread: main_waiting waking it route %d", rx_control_info.mainWaitingState.waitingRoute);
			rx_control_info.mainWaitingState.reachRoute = conn->route;
		}
		/* WAKE MAIN THREAD HERE */
		*wakeup_mainthread = true;
	}

	return true;
}

/*
 * rxThreadFunc
 * 		Main function of the receive background thread.
 *
 * NOTE: This function MUST NOT contain elog or ereport statements.
 * elog is NOT thread-safe.  Developers should instead use something like:
 *
 *	if (DEBUG3 >= log_min_messages)
 *		write_log("my brilliant log statement here.");
 *
 * NOTE: In threads, we cannot use palloc/pfree, because it's not thread safe.
 */
static void *
rxThreadFunc(void *arg)
{
	icpkthdr   *pkt = NULL;
	bool		skip_poll = false;

	for (;;)
	{
		struct pollfd nfd;
		int			n;

		/* check shutdown condition */
		if (pg_atomic_read_u32(&ic_control_info.shutdown) == 1)
		{
			if (DEBUG1 >= log_min_messages)
			{
				write_log("udp-ic: rx-thread shutting down");
			}
			break;
		}

		/* Try to get a buffer */
		if (pkt == NULL)
		{
			pthread_mutex_lock(&ic_control_info.lock);
			pkt = getRxBuffer(&rx_buffer_pool);
			pthread_mutex_unlock(&ic_control_info.lock);

			if (pkt == NULL)
			{
				setRxThreadError(ENOMEM);
				continue;
			}
		}

		if (!skip_poll)
		{
			/* Do we have inbound traffic to handle ? */
			nfd.fd = UDP_listenerFd;
			nfd.events = POLLIN;

			n = poll(&nfd, 1, RX_THREAD_POLL_TIMEOUT);

			if (pg_atomic_read_u32(&ic_control_info.shutdown) == 1)
			{
				if (DEBUG1 >= log_min_messages)
				{
					write_log("udp-ic: rx-thread shutting down");
				}
				break;
			}

			if (n < 0)
			{
				if (errno == EINTR)
					continue;

				/*
				 * ERROR case: if simply break out the loop here, there will
				 * be a hung here, since main thread will never be waken up,
				 * and senders will not get responses anymore.
				 *
				 * Thus, we set an error flag, and let main thread to report
				 * an error.
				 */
				setRxThreadError(errno);
				continue;
			}

			if (n == 0)
				continue;
		}

		if (skip_poll || (n == 1 && (nfd.events & POLLIN)))
		{
			/* we've got something interesting to read */
			/* handle incoming */
			/* ready to read on our socket */
			MotionConn *conn = NULL;
			int			read_count = 0;

			struct sockaddr_storage peer;
			socklen_t	peerlen;

			peerlen = sizeof(peer);
			read_count = recvfrom(UDP_listenerFd, (char *) pkt, Gp_max_packet_size, 0,
								  (struct sockaddr *) &peer, &peerlen);

			if (pg_atomic_read_u32(&ic_control_info.shutdown) == 1)
			{
				if (DEBUG1 >= log_min_messages)
				{
					write_log("udp-ic: rx-thread shutting down");
				}
				break;
			}

			if (DEBUG5 >= log_min_messages)
				write_log("received inbound len %d", read_count);

			if (read_count < 0)
			{
				skip_poll = false;

				if (errno == EWOULDBLOCK || errno == EINTR)
					continue;

				write_log("Interconnect error: recvfrom (%d)", errno);

				/*
				 * ERROR case: if simply break out the loop here, there will
				 * be a hung here, since main thread will never be waken up,
				 * and senders will not get responses anymore.
				 *
				 * Thus, we set an error flag, and let main thread to report
				 * an error.
				 */
				setRxThreadError(errno);
				continue;
			}

			if (read_count < sizeof(icpkthdr))
			{
				if (DEBUG1 >= log_min_messages)
					write_log("Interconnect error: short conn receive (%d)", read_count);
				continue;
			}

			/*
			 * when we get a "good" recvfrom() result, we can skip poll()
			 * until we get a bad one.
			 */
			skip_poll = true;

			/* length must be >= 0 */
			if (pkt->len < 0)
			{
				if (DEBUG3 >= log_min_messages)
					write_log("received inbound with negative length");
				continue;
			}

			if (pkt->len != read_count)
			{
				if (DEBUG3 >= log_min_messages)
					write_log("received inbound packet [%d], short: read %d bytes, pkt->len %d", pkt->seq, read_count, pkt->len);
				continue;
			}

			/*
			 * check the CRC of the payload.
			 */
			if (gp_interconnect_full_crc)
			{
				if (!checkCRC(pkt))
				{
					pg_atomic_add_fetch_u32((pg_atomic_uint32 *) &ic_statistics.crcErrors, 1);
					if (DEBUG2 >= log_min_messages)
						write_log("received network data error, dropping bad packet, user data unaffected.");
					continue;
				}
			}

#ifdef AMS_VERBOSE_LOGGING
			logPkt("GOT MESSAGE", pkt);
#endif

			bool		wakeup_mainthread = false;
			AckSendParam param;

			memset(&param, 0, sizeof(AckSendParam));

			/*
			 * Get the connection for the pkt.
			 *
			 * The connection hash table should be locked until finishing the
			 * processing of the packet to avoid the connection
			 * addition/removal from the hash table during the mean time.
			 */

			pthread_mutex_lock(&ic_control_info.lock);
			conn = findConnByHeader(&ic_control_info.connHtab, pkt);

			if (conn != NULL)
			{
				/* Handling a regular packet */
				if (handleDataPacket(conn, pkt, &peer, &peerlen, &param, &wakeup_mainthread))
					pkt = NULL;
				ic_statistics.recvPktNum++;
			}
			else
			{
				/*
				 * There may have two kinds of Mismatched packets: a) Past
				 * packets from previous command after I was torn down b)
				 * Future packets from current command before my connections
				 * are built.
				 *
				 * The handling logic is to "Ack the past and Nak the future".
				 */
				if ((pkt->flags & UDPIC_FLAGS_RECEIVER_TO_SENDER) == 0)
				{
					if (DEBUG1 >= log_min_messages)
						write_log("mismatched packet received, seq %d, srcpid %d, dstpid %d, icid %d, sid %d", pkt->seq, pkt->srcPid, pkt->dstPid, pkt->icId, pkt->sessionId);

#ifdef AMS_VERBOSE_LOGGING
					logPkt("Got a Mismatched Packet", pkt);
#endif

					if (handleMismatch(pkt, &peer, peerlen))
						pkt = NULL;
					ic_statistics.mismatchNum++;
				}
			}
			pthread_mutex_unlock(&ic_control_info.lock);

			if (wakeup_mainthread)
				SetLatch(&ic_control_info.latch);

			/*
			 * real ack sending is after lock release to decrease the lock
			 * holding time.
			 */
			if (param.msg.len != 0)
				sendAckWithParam(&param);
		}

		/* pthread_yield(); */
	}

	/* Before return, we release the packet. */
	if (pkt)
	{
		pthread_mutex_lock(&ic_control_info.lock);
		freeRxBuffer(&rx_buffer_pool, pkt);
		pkt = NULL;
		pthread_mutex_unlock(&ic_control_info.lock);
	}

	/* nothing to return */
	return NULL;
}

/*
 * handleMismatch
 * 		If the mismatched packet is from an old connection, we may need to
 * 		send an acknowledgment.
 *
 * We are called with the receiver-lock held, and we never release it.
 *
 * For QD:
 * 1) Not in hashtable     : NAK it/Do nothing
 * 	  Causes:  a) Start race
 * 	           b) Before the entry for the ic instance is inserted, an error happened.
 * 	           c) From past transactions: should no happen.
 * 2) Active in hashtable  : NAK it/Do nothing
 *    Causes:  a) Error reported after the entry is inserted, and connections are
 *                not inserted to the hashtable yet, and before teardown is called.
 * 3) Inactive in hashtable: ACK it (with stop)
 *    Causes: a) Normal execution: after teardown is called on current command.
 *            b) Error case, 2a) after teardown is called.
 *            c) Normal execution: from past history transactions (should not happen).
 *
 * For QE:
 * 1) pkt->id > ic_control_info.ic_instance_id : NAK it/Do nothing
 *    Causes: a) Start race
 *            b) Before ic_control_info.ic_instance_id is assigned to correct value, an error happened.
 * 2) lastTornIcId < pkt->id == ic_control_info.ic_instance_id: NAK it/Do nothing
 *    Causes:  a) Error reported after ic_control_info.ic_instance_id is set, and connections are
 *                not inserted to the hashtable yet, and before teardown is called.
 * 3) lastTornIcId == pkt->id == ic_control_info.ic_instance_id: ACK it (with stop)
 *    Causes:  a) Normal execution: after teardown is called on current command
 * 4) pkt->id < ic_control_info.ic_instance_id: NAK it/Do nothing/ACK it.
 *    Causes:  a) Should not happen.
 *
 */
static bool
handleMismatch(icpkthdr *pkt, struct sockaddr_storage *peer, int peer_len)
{
	bool		cached = false;

	/*
	 * we want to ack old packets; but *must* avoid acking connection
	 * requests:
	 *
	 * "ACK the past, NAK the future" explicit NAKs aren't necessary, we just
	 * don't want to ACK future packets, that confuses everyone.
	 */
	if (pkt->seq > 0 && pkt->sessionId == gp_session_id)
	{
		bool		need_ack = false;
		uint8		ack_flags = 0;

		/*
		 * The QD-backends can't use a counter, they've potentially got
		 * multiple instances (one for each active cursor)
		 */
		if (Gp_role == GP_ROLE_DISPATCH)
		{
			struct CursorICHistoryEntry *p;

			p = getCursorIcEntry(&rx_control_info.cursorHistoryTable, pkt->icId);
			if (p)
			{
				if (p->status == 0)
				{
					/* Torn down. Ack the past. */
					need_ack = true;
				}
				else			/* p->status == 1 */
				{
					/*
					 * Not torn down yet. It happens when an error
					 * (out-of-memory, network error...) occurred after the
					 * cursor entry is inserted into the table in interconnect
					 * setup process. The peer will be canceled.
					 */
					if (DEBUG1 >= log_min_messages)
						write_log("GOT A MISMATCH PACKET WITH ID %d HISTORY THINKS IT IS ACTIVE", pkt->icId);
					return cached;	/* ignore, no ack */
				}
			}
			else
			{
				if (DEBUG1 >= log_min_messages)
					write_log("GOT A MISMATCH PACKET WITH ID %d HISTORY HAS NO RECORD", pkt->icId);

				/*
				 * No record means that two possibilities. 1) It is from the
				 * future. It is due to startup race. We do not ack future
				 * packets 2) Before the entry for the ic instance is
				 * inserted, an error happened. We do not ack for this case
				 * too. The peer will be canceled.
				 */
				ack_flags = UDPIC_FLAGS_NAK;
				need_ack = false;

				if (gp_interconnect_cache_future_packets)
				{
					cached = cacheFuturePacket(pkt, peer, peer_len);
				}
			}
		}
		/* The QEs get to use a simple counter. */
		else if (Gp_role == GP_ROLE_EXECUTE)
		{
			if (ic_control_info.ic_instance_id >= pkt->icId)
			{
				need_ack = true;

				/*
				 * We want to "ACK the past, but NAK the future."
				 *
				 * handleAck() will retransmit.
				 */
				if (pkt->seq >= 1 && pkt->icId > rx_control_info.lastTornIcId)
				{
					ack_flags = UDPIC_FLAGS_NAK;
					need_ack = false;
				}
			}
			else
			{
				/*
				 * ic_control_info.ic_instance_id < pkt->icId, from the future
				 */ 
				if (gp_interconnect_cache_future_packets)
				{
					cached = cacheFuturePacket(pkt, peer, peer_len);
				}
			}
		}

		if (need_ack)
		{
			MotionConn	dummyconn;
			char		buf[128];	/* numeric IP addresses shouldn't exceed
									 * about 50 chars, but play it safe */


			memcpy(&dummyconn.conn_info, pkt, sizeof(icpkthdr));
			dummyconn.peer = *peer;
			dummyconn.peer_len = peer_len;

			dummyconn.conn_info.flags |= ack_flags;

			if (DEBUG1 >= log_min_messages)
				write_log("ACKING PACKET WITH FLAGS: pkt->seq %d 0x%x [pkt->icId %d last-teardown %d interconnect_id %d]",
						  pkt->seq, dummyconn.conn_info.flags, pkt->icId, rx_control_info.lastTornIcId, ic_control_info.ic_instance_id);

			format_sockaddr(&dummyconn.peer, buf, sizeof(buf));

			if (DEBUG1 >= log_min_messages)
				write_log("ACKING PACKET TO %s", buf);

			if ((ack_flags & UDPIC_FLAGS_NAK) == 0)
			{
				ack_flags |= UDPIC_FLAGS_STOP | UDPIC_FLAGS_ACK | UDPIC_FLAGS_CAPACITY | UDPIC_FLAGS_RECEIVER_TO_SENDER;
			}
			else
			{
				ack_flags |= UDPIC_FLAGS_RECEIVER_TO_SENDER;
			}

			/*
			 * There are two cases, we may need to send a response to sender
			 * here. One is start race and the other is receiver becomes idle.
			 *
			 * ack_flags here can take two possible values 1) UDPIC_FLAGS_NAK
			 * | UDPIC_FLAGS_RECEIVER_TO_SENDER (for start race) 2)
			 * UDPIC_FLAGS_STOP | UDPIC_FLAGS_ACK | UDPIC_FLAGS_CAPACITY |
			 * UDPIC_FLAGS_RECEIVER_TO_SENDER (for idle receiver)
			 *
			 * The final flags in the packet may take some extra bits such as
			 * 1) UDPIC_FLAGS_STOP 2) UDPIC_FLAGS_EOS 3) UDPIC_FLAGS_CAPACITY
			 * which are from original packet
			 */
			sendAck(&dummyconn, ack_flags | dummyconn.conn_info.flags, dummyconn.conn_info.seq, dummyconn.conn_info.seq);
		}
	}
	else
	{
		if (DEBUG1 >= log_min_messages)
			write_log("dropping packet from command-id %d seq %d (my cmd %d)", pkt->icId, pkt->seq, ic_control_info.ic_instance_id);
	}

	return cached;
}

/*
 * cacheFuturePacket
 *		Cache the future packets during the setupUDPIFCInterconnect.
 *
 * Return true if packet is cached, otherwise false
 */
static bool
cacheFuturePacket(icpkthdr *pkt, struct sockaddr_storage *peer, int peer_len)
{
	MotionConn *conn;

	conn = findConnByHeader(&ic_control_info.startupCacheHtab, pkt);

	if (conn == NULL)
	{
		conn = malloc(sizeof(MotionConn));
		if (conn == NULL)
		{
			setRxThreadError(errno);
			return false;
		}

		memset((void *) conn, 0, sizeof(MotionConn));
		memcpy(&conn->conn_info, pkt, sizeof(icpkthdr));

		conn->pkt_q_capacity = Gp_interconnect_queue_depth;
		conn->pkt_q_size = Gp_interconnect_queue_depth;
		conn->pkt_q = (uint8 **) malloc(Gp_interconnect_queue_depth * sizeof(uint8 *));

		if (conn->pkt_q == NULL)
		{
			/* malloc failed.  */
			free(conn);
			setRxThreadError(errno);
			return false;
		}

		/* We only use the array to store cached packets. */
		memset(conn->pkt_q, 0, Gp_interconnect_queue_depth * sizeof(uint8 *));

		/* Put connection to the hashtable. */
		if (!connAddHash(&ic_control_info.startupCacheHtab, conn))
		{
			free(conn->pkt_q);
			free(conn);
			setRxThreadError(errno);
			return false;
		}

		/* Setup the peer sock information. */
		memcpy(&conn->peer, peer, peer_len);
		conn->peer_len = peer_len;
	}

	/*
	 * Reject packets with invalid sequence numbers and packets which have
	 * been cached before.
	 */
	if (pkt->seq > conn->pkt_q_size || pkt->seq == 0 || conn->pkt_q[pkt->seq - 1] != NULL)
		return false;

	conn->pkt_q[pkt->seq - 1] = (uint8 *) pkt;
	rx_buffer_pool.maxCount++;
	ic_statistics.startupCachedPktNum++;
	return true;
}

/*
 * cleanupStartupCache
 *		Clean the startup cache.
 */
static void
cleanupStartupCache()
{
	ConnHtabBin *bin = NULL;
	MotionConn *cachedConn = NULL;
	icpkthdr   *pkt = NULL;
	int			i = 0;
	int			j = 0;

	for (i = 0; i < ic_control_info.startupCacheHtab.size; i++)
	{
		bin = ic_control_info.startupCacheHtab.table[i];

		while (bin)
		{
			cachedConn = bin->conn;

			for (j = 0; j < cachedConn->pkt_q_size; j++)
			{
				pkt = (icpkthdr *) cachedConn->pkt_q[j];

				if (pkt == NULL)
					continue;

				rx_buffer_pool.maxCount--;

				putRxBufferToFreeList(&rx_buffer_pool, pkt);
				cachedConn->pkt_q[j] = NULL;
			}
			bin = bin->next;
			connDelHash(&ic_control_info.startupCacheHtab, cachedConn);

			/*
			 * MPP-19981 free the cached connections; otherwise memory leak
			 * would be introduced.
			 */
			free(cachedConn->pkt_q);
			free(cachedConn);
		}
	}
}


/* The following functions are facility methods for debugging.
 * They are quite useful when there are a large number of connections.
 * These functions can be called from gdb to output internal information to a file.
 */

/*
 * dumpICBufferList_Internal
 * 		Dump a buffer list.
 */
static void
dumpICBufferList_Internal(ICBufferList *list, FILE *ofile)
{

	ICBufferLink *bufLink = list->head.next;

	int			len = list->length;
	int			i = 0;

	fprintf(ofile, "List Length %d\n", len);
	while (bufLink != &list->head && len > 0)
	{
		ICBuffer   *buf = (list->type == ICBufferListType_Primary ? GET_ICBUFFER_FROM_PRIMARY(bufLink)
						   : GET_ICBUFFER_FROM_SECONDARY(bufLink));

		fprintf(ofile, "Node %d, linkptr %p ", i++, bufLink);
		fprintf(ofile, "Packet Content [%s: seq %d extraSeq %d]: motNodeId %d, crc %d len %d "
				"srcContentId %d dstDesContentId %d "
				"srcPid %d dstPid %d "
				"srcListenerPort %d dstListernerPort %d "
				"sendSliceIndex %d recvSliceIndex %d "
				"sessionId %d icId %d "
				"flags %d\n",
				buf->pkt->flags & UDPIC_FLAGS_RECEIVER_TO_SENDER ? "ACK" : "DATA",
				buf->pkt->seq, buf->pkt->extraSeq, buf->pkt->motNodeId, buf->pkt->crc, buf->pkt->len,
				buf->pkt->srcContentId, buf->pkt->dstContentId,
				buf->pkt->srcPid, buf->pkt->dstPid,
				buf->pkt->srcListenerPort, buf->pkt->dstListenerPort,
				buf->pkt->sendSliceIndex, buf->pkt->recvSliceIndex,
				buf->pkt->sessionId, buf->pkt->icId,
				buf->pkt->flags);
		bufLink = bufLink->next;
		len--;
	}
}


/*
 * dumpICBufferList
 * 		Dump a buffer list.
 */
void
dumpICBufferList(ICBufferList *list, const char *fname)
{
	FILE	   *ofile = fopen(fname, "w+");

	dumpICBufferList_Internal(list, ofile);
	fclose(ofile);
}

/*
 * dumpUnackQueueRing
 * 		Dump an unack queue ring.
 */
void
dumpUnackQueueRing(const char *fname)
{
	FILE	   *ofile = fopen(fname, "w+");
	int			i;

	fprintf(ofile, "UnackQueueRing: currentTime " UINT64_FORMAT ", idx %d numOutstanding %d numSharedOutstanding %d\n",
			unack_queue_ring.currentTime, unack_queue_ring.idx,
			unack_queue_ring.numOutStanding, unack_queue_ring.numSharedOutStanding);
	fprintf(ofile, "==================================\n");
	for (i = 0; i < UNACK_QUEUE_RING_SLOTS_NUM; i++)
	{
		if (icBufferListLength(&unack_queue_ring.slots[i]) > 0)
		{
			dumpICBufferList_Internal(&unack_queue_ring.slots[i], ofile);
		}
	}

	fclose(ofile);
}

/*
 * dumpConnections
 * 		Dump connections.
 */
void
dumpConnections(ChunkTransportStateEntry *pEntry, const char *fname)
{
	int			i,
				j;
	MotionConn *conn;

	FILE	   *ofile = fopen(fname, "w+");

	fprintf(ofile, "Entry connections: conn num %d \n", pEntry->numConns);
	fprintf(ofile, "==================================\n");

	for (i = 0; i < pEntry->numConns; i++)
	{
		conn = &pEntry->conns[i];

		fprintf(ofile, "conns[%d] motNodeId=%d: remoteContentId=%d pid=%d sockfd=%d remote=%s local=%s "
				"capacity=%d sentSeq=%d receivedAckSeq=%d consumedSeq=%d rtt=" UINT64_FORMAT
				" dev=" UINT64_FORMAT " deadlockCheckBeginTime=" UINT64_FORMAT " route=%d msgSize=%d msgPos=%p"
				" recvBytes=%d tupleCount=%d stillActive=%d stopRequested=%d "
				"state=%d\n",
				i, pEntry->motNodeId,
				conn->remoteContentId,
				conn->cdbProc ? conn->cdbProc->pid : 0,
				conn->sockfd,
				conn->remoteHostAndPort,
				conn->localHostAndPort,
				conn->capacity, conn->sentSeq, conn->receivedAckSeq, conn->consumedSeq,
				conn->rtt, conn->dev, conn->deadlockCheckBeginTime, conn->route, conn->msgSize, conn->msgPos,
				conn->recvBytes, conn->tupleCount, conn->stillActive, conn->stopRequested,
				conn->state);
		fprintf(ofile, "conn_info [%s: seq %d extraSeq %d]: motNodeId %d, crc %d len %d "
				"srcContentId %d dstDesContentId %d "
				"srcPid %d dstPid %d "
				"srcListenerPort %d dstListernerPort %d "
				"sendSliceIndex %d recvSliceIndex %d "
				"sessionId %d icId %d "
				"flags %d\n",
				conn->conn_info.flags & UDPIC_FLAGS_RECEIVER_TO_SENDER ? "ACK" : "DATA",
				conn->conn_info.seq, conn->conn_info.extraSeq, conn->conn_info.motNodeId, conn->conn_info.crc, conn->conn_info.len,
				conn->conn_info.srcContentId, conn->conn_info.dstContentId,
				conn->conn_info.srcPid, conn->conn_info.dstPid,
				conn->conn_info.srcListenerPort, conn->conn_info.dstListenerPort,
				conn->conn_info.sendSliceIndex, conn->conn_info.recvSliceIndex,
				conn->conn_info.sessionId, conn->conn_info.icId,
				conn->conn_info.flags);

		if (!ic_control_info.isSender)
		{
			fprintf(ofile, "pkt_q_size=%d pkt_q_head=%d pkt_q_tail=%d pkt_q=%p\n", conn->pkt_q_size, conn->pkt_q_head, conn->pkt_q_tail, conn->pkt_q);
			for (j = 0; j < conn->pkt_q_capacity; j++)
			{
				if (conn->pkt_q != NULL && conn->pkt_q[j] != NULL)
				{
					icpkthdr   *pkt = (icpkthdr *) conn->pkt_q[j];

					fprintf(ofile, "Packet (pos %d) Info [%s: seq %d extraSeq %d]: motNodeId %d, crc %d len %d "
							"srcContentId %d dstDesContentId %d "
							"srcPid %d dstPid %d "
							"srcListenerPort %d dstListernerPort %d "
							"sendSliceIndex %d recvSliceIndex %d "
							"sessionId %d icId %d "
							"flags %d\n",
							j,
							pkt->flags & UDPIC_FLAGS_RECEIVER_TO_SENDER ? "ACK" : "DATA",
							pkt->seq, pkt->extraSeq, pkt->motNodeId, pkt->crc, pkt->len,
							pkt->srcContentId, pkt->dstContentId,
							pkt->srcPid, pkt->dstPid,
							pkt->srcListenerPort, pkt->dstListenerPort,
							pkt->sendSliceIndex, pkt->recvSliceIndex,
							pkt->sessionId, pkt->icId,
							pkt->flags);
				}
			}
		}
		if (ic_control_info.isSender)
		{
			fprintf(ofile, "sndQueue ");
			dumpICBufferList_Internal(&conn->sndQueue, ofile);
			fprintf(ofile, "unackQueue ");
			dumpICBufferList_Internal(&conn->unackQueue, ofile);
		}
		fprintf(ofile, "\n");
	}
	fclose(ofile);
}

void
WaitInterconnectQuitUDPIFC(void)
{
	/*
	 * Just in case ic thread is waiting on the locks.
	 */
	pthread_mutex_unlock(&ic_control_info.lock);

	pg_atomic_write_u32(&ic_control_info.shutdown, 1);

	if (ic_control_info.threadCreated)
	{
		SendDummyPacket();
		pthread_join(ic_control_info.threadHandle, NULL);
	}
	ic_control_info.threadCreated = false;
}

/*
 * Send a dummy packet to interconnect thread to exit poll() immediately
 */
static void
SendDummyPacket(void)
{
	int			sockfd = -1;
	int			ret;
	struct addrinfo *addrs = NULL;
	struct addrinfo *rp;
	struct addrinfo hint;
	uint16		udp_listener;
	char		port_str[32] = {0};
	char	   *dummy_pkt = "stop it";
	int			counter;

	/*
	 * Get address info from interconnect udp listener port
	 */
	udp_listener = (Gp_listener_port >> 16) & 0x0ffff;
	snprintf(port_str, sizeof(port_str), "%d", udp_listener);

	MemSet(&hint, 0, sizeof(hint));
	hint.ai_socktype = SOCK_DGRAM;
	hint.ai_family = AF_UNSPEC; /* Allow for IPv4 or IPv6  */

	/* Never do name resolution */
#ifdef AI_NUMERICSERV
	hint.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
#else
	hint.ai_flags = AI_NUMERICHOST;
#endif

	ret = pg_getaddrinfo_all(interconnect_address, port_str, &hint, &addrs);
	if (ret || !addrs)
	{
		elog(LOG, "send dummy packet failed, pg_getaddrinfo_all(): %m");
		goto send_error;
	}

	for (rp = addrs; rp != NULL; rp = rp->ai_next)
	{
		/* Create socket according to pg_getaddrinfo_all() */
		sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sockfd < 0)
			continue;

		if (!pg_set_noblock(sockfd))
		{
			if (sockfd >= 0)
			{
				closesocket(sockfd);
				sockfd = -1;
			}
			continue;
		}
		break;
	}

	if (rp == NULL)
	{
		elog(LOG, "send dummy packet failed, create socket failed: %m");
		goto send_error;
	}

	/*
	 * Send a dummy package to the interconnect listener, try 10 times
	 */

	counter = 0;
	while (counter < 10)
	{
		counter++;
		ret = sendto(sockfd, dummy_pkt, strlen(dummy_pkt), 0, rp->ai_addr, rp->ai_addrlen);
		if (ret < 0)
		{
			if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
			else
			{
				elog(LOG, "send dummy packet failed, sendto failed: %m");
				goto send_error;
			}
		}
		break;
	}

	if (counter >= 10)
	{
		elog(LOG, "send dummy packet failed, sendto failed with 10 times: %m");
		goto send_error;
	}

	pg_freeaddrinfo_all(hint.ai_family, addrs);
	closesocket(sockfd);
	return;

send_error:

	if (addrs)
		pg_freeaddrinfo_all(hint.ai_family, addrs);
	if (sockfd != -1)
		closesocket(sockfd);
	return;
}

uint32
getActiveMotionConns(void)
{
	return ic_statistics.activeConnectionsNum;
}
