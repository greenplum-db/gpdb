/*-------------------------------------------------------------------------
 * ic_common.c
 *	   Interconnect code shared between UDP, and TCP IPC Layers.
 *
 * Portions Copyright (c) 2005-2008, Greenplum
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/motion/ic_common.c
 *
 * Reviewers: jzhang, tkordas
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "nodes/execnodes.h"	/* ExecSlice, SliceTable */
#include "miscadmin.h"
#include "libpq/libpq-be.h"
#include "libpq/ip.h"
#include "utils/builtins.h"
#include "utils/memutils.h"

#include "cdb/ml_ipc.h"
#include "cdb/cdbvars.h"
#include "cdb/cdbdisp.h"

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <netinet/in.h>

/*
  #define AMS_VERBOSE_LOGGING
*/

/*=========================================================================
 * STRUCTS
 */
typedef struct interconnect_handle_t
{
	ChunkTransportState	*interconnect_context; /* Interconnect state */

	ResourceOwner owner;	/* owner of this handle */
	struct interconnect_handle_t *next;
	struct interconnect_handle_t *prev;
} interconnect_handle_t;

/*=========================================================================
 * GLOBAL STATE VARIABLES
 */

/* Socket file descriptor for the listener. */
int			TCP_listenerFd;
int			UDP_listenerFd;

static interconnect_handle_t *open_interconnect_handles;
static bool interconnect_resowner_callback_registered;

/*=========================================================================
 * FUNCTIONS PROTOTYPES
 */

static void interconnect_abort_callback(ResourceReleasePhase phase,
								   bool isCommit,
								   bool isTopLevel,
								   void *arg);
static void cleanup_interconnect_handle(interconnect_handle_t *h);
static interconnect_handle_t *allocate_interconnect_handle(void);
static void destroy_interconnect_handle(interconnect_handle_t *h);
static interconnect_handle_t *find_interconnect_handle(ChunkTransportState *icContext);

static void
logChunkParseDetails(MotionConn *conn, uint32 ic_instance_id)
{
	struct icpkthdr *pkt;

	Assert(conn != NULL);
	Assert(conn->pBuff != NULL);

	pkt = (struct icpkthdr *) conn->pBuff;

	elog(LOG, "Interconnect parse details: pkt->len %d pkt->seq %d pkt->flags 0x%x conn->active %d conn->stopRequest %d pkt->icId %d my_icId %d",
		 pkt->len, pkt->seq, pkt->flags, conn->stillActive, conn->stopRequested, pkt->icId, ic_instance_id);

	elog(LOG, "Interconnect parse details continued: peer: srcpid %d dstpid %d recvslice %d sendslice %d srccontent %d dstcontent %d",
		 pkt->srcPid, pkt->dstPid, pkt->recvSliceIndex, pkt->sendSliceIndex, pkt->srcContentId, pkt->dstContentId);
}

TupleChunkListItem
RecvTupleChunk(MotionConn *conn, ChunkTransportState *transportStates)
{
	TupleChunkListItem tcItem;
	TupleChunkListItem firstTcItem = NULL;
	TupleChunkListItem lastTcItem = NULL;
	uint32		tcSize;
	int			bytesProcessed = 0;

	if (Gp_interconnect_type == INTERCONNECT_TYPE_TCP)
	{
		/* read the packet in from the network. */
		readPacket(conn, transportStates);

		/* go through and form us some TupleChunks. */
		bytesProcessed = PACKET_HEADER_SIZE;
	}
	else
	{
		/* go through and form us some TupleChunks. */
		bytesProcessed = sizeof(struct icpkthdr);
	}

#ifdef AMS_VERBOSE_LOGGING
	elog(DEBUG5, "recvtuple chunk recv bytes %d msgsize %d conn->pBuff %p conn->msgPos: %p",
		 conn->recvBytes, conn->msgSize, conn->pBuff, conn->msgPos);
#endif

	while (bytesProcessed != conn->msgSize)
	{
		if (conn->msgSize - bytesProcessed < TUPLE_CHUNK_HEADER_SIZE)
		{
			logChunkParseDetails(conn, transportStates->sliceTable->ic_instance_id);

			ereport(ERROR,
					(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
					 errmsg("interconnect error parsing message: insufficient data received"),
					 errdetail("conn->msgSize %d bytesProcessed %d < chunk-header %d",
							   conn->msgSize, bytesProcessed, TUPLE_CHUNK_HEADER_SIZE)));
		}

		tcSize = TUPLE_CHUNK_HEADER_SIZE + (*(uint16 *) (conn->msgPos + bytesProcessed));

		/* sanity check */
		if (tcSize > Gp_max_packet_size)
		{
			/*
			 * see MPP-720: it is possible that our message got messed up by a
			 * cancellation ?
			 */
			ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);

			/*
			 * MPP-4010: add some extra debugging.
			 */
			if (lastTcItem != NULL)
				elog(LOG, "Interconnect error parsing message: last item length %d inplace %p", lastTcItem->chunk_length, lastTcItem->inplace);
			else
				elog(LOG, "Interconnect error parsing message: no last item");

			logChunkParseDetails(conn, transportStates->sliceTable->ic_instance_id);

			ereport(ERROR,
					(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
					 errmsg("interconnect error parsing message"),
					 errdetail("tcSize %d > max %d header %d processed %d/%d from %p",
							   tcSize, Gp_max_packet_size,
							   TUPLE_CHUNK_HEADER_SIZE, bytesProcessed,
							   conn->msgSize, conn->msgPos)));
		}


		/*
		 * we only check for interrupts here when we don't have a guaranteed
		 * full-message
		 */
		if (Gp_interconnect_type == INTERCONNECT_TYPE_TCP)
		{
			if (tcSize >= conn->msgSize)
			{
				/*
				 * see MPP-720: it is possible that our message got messed up
				 * by a cancellation ?
				 */
				ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);

				logChunkParseDetails(conn, transportStates->sliceTable->ic_instance_id);

				ereport(ERROR,
						(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						 errmsg("interconnect error parsing message"),
						 errdetail("tcSize %d >= conn->msgSize %d",
								   tcSize, conn->msgSize)));
			}
		}
		Assert(tcSize < conn->msgSize);

		/*
		 * We store the data inplace, and handle any necessary copying later
		 * on
		 */
		tcItem = (TupleChunkListItem) palloc(sizeof(TupleChunkListItemData));

		tcItem->p_next = NULL;
		tcItem->chunk_length = tcSize;
		tcItem->inplace = (char *) (conn->msgPos + bytesProcessed);

		bytesProcessed += tcSize;

		if (firstTcItem == NULL)
		{
			firstTcItem = tcItem;
			lastTcItem = tcItem;
		}
		else
		{
			lastTcItem->p_next = tcItem;
			lastTcItem = tcItem;
		}
	}

	conn->recvBytes -= conn->msgSize;
	if (conn->recvBytes != 0)
	{
#ifdef AMS_VERBOSE_LOGGING
		elog(DEBUG5, "residual message %d bytes", conn->recvBytes);
#endif
		conn->msgPos += conn->msgSize;
	}

	conn->msgSize = 0;

	return firstTcItem;
}

/*=========================================================================
 * VISIBLE FUNCTIONS
 */

/* See ml_ipc.h */
void
InitMotionLayerIPC(void)
{
	uint16		tcp_listener = 0;
	uint16		udp_listener = 0;

	/* activated = false; */

	if (Gp_interconnect_type == INTERCONNECT_TYPE_TCP)
		InitMotionTCP(&TCP_listenerFd, &tcp_listener);
	else if (Gp_interconnect_type == INTERCONNECT_TYPE_UDPIFC)
		InitMotionUDPIFC(&UDP_listenerFd, &udp_listener);

	Gp_listener_port = (udp_listener << 16) | tcp_listener;

	elog(DEBUG1, "Interconnect listening on tcp port %d udp port %d (0x%x)", tcp_listener, udp_listener, Gp_listener_port);
}

/* See ml_ipc.h */
void
CleanUpMotionLayerIPC(void)
{
	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
		elog(DEBUG3, "Cleaning Up Motion Layer IPC...");

	if (Gp_interconnect_type == INTERCONNECT_TYPE_TCP)
		CleanupMotionTCP();
	else if (Gp_interconnect_type == INTERCONNECT_TYPE_UDPIFC)
		CleanupMotionUDPIFC();

	/* close down the Interconnect listener socket. */
	if (TCP_listenerFd >= 0)
		closesocket(TCP_listenerFd);

	if (UDP_listenerFd >= 0)
		closesocket(UDP_listenerFd);

	/* be safe and reset global state variables. */
	Gp_listener_port = 0;
	TCP_listenerFd = -1;
	UDP_listenerFd = -1;
}

/* See ml_ipc.h */
bool
SendTupleChunkToAMS(MotionLayerState *mlStates,
					ChunkTransportState *transportStates,
					int16 motNodeID,
					int16 targetRoute,
					TupleChunkListItem tcItem)
{
	int			i,
				recount = 0;
	ChunkTransportStateEntry *pEntry = NULL;
	MotionConn *conn;
	TupleChunkListItem currItem;

	if (!transportStates)
		elog(FATAL, "SendTupleChunkToAMS: no transport-states.");
	if (!transportStates->activated)
		elog(FATAL, "SendTupleChunkToAMS: transport states inactive");

	/* check em' */
	ML_CHECK_FOR_INTERRUPTS(transportStates->teardownActive);

#ifdef AMS_VERBOSE_LOGGING
	elog(DEBUG3, "sendtuplechunktoams: calling get_transport_state"
		 "w/transportStates %p transportState->size %d motnodeid %d route %d",
		 transportStates, transportStates->size, motNodeID, targetRoute);
#endif

	getChunkTransportState(transportStates, motNodeID, &pEntry);

	/*
	 * tcItem can actually be a chain of tcItems.  we need to send out all of
	 * them.
	 */
	currItem = tcItem;

	for (currItem = tcItem; currItem != NULL; currItem = currItem->p_next)
	{
#ifdef AMS_VERBOSE_LOGGING
		elog(DEBUG5, "SendTupleChunkToAMS: chunk length %d", currItem->chunk_length);
#endif

		if (targetRoute == BROADCAST_SEGIDX)
		{
			doBroadcast(transportStates, pEntry, currItem, &recount);
		}
		else
		{
			/* handle pt-to-pt message. Primary */
			Assert(targetRoute >= 0);
			Assert(targetRoute < pEntry->numConns);
			conn = pEntry->conns + targetRoute;
			/* only send to interested connections */
			if (conn->stillActive)
			{
				transportStates->SendChunk(transportStates, pEntry, conn, currItem, motNodeID);
				if (!conn->stillActive)
					recount = 1;
			}
			/* in 4.0 logical mirror xmit eliminated. */
		}
	}

	if (recount == 0)
		return true;

	/* if we don't have any connections active, return false */
	for (i = 0; i < pEntry->numConns; i++)
	{
		conn = pEntry->conns + i;
		if (conn->stillActive)
			break;
	}

	/* if we found an active connection we're not done */
	return (i < pEntry->numConns);
}

/*
 * The fetches a direct pointer into our transmit buffers, along with
 * an indication as to how much data can be safely shoved into the
 * buffer (started at the pointed location).
 *
 * This works a lot like SendTupleChunkToAMS().
 */
void
getTransportDirectBuffer(ChunkTransportState *transportStates,
						 int16 motNodeID,
						 int16 targetRoute,
						 struct directTransportBuffer *b)
{
	ChunkTransportStateEntry *pEntry = NULL;
	MotionConn *conn;

	if (!transportStates)
	{
		elog(FATAL, "getTransportDirectBuffer: no transport states");
	}
	else if (!transportStates->activated)
	{
		elog(FATAL, "getTransportDirectBuffer: inactive transport states");
	}
	else if (targetRoute == BROADCAST_SEGIDX)
	{
		elog(FATAL, "getTransportDirectBuffer: can't direct-transport to broadcast");
	}

	Assert(b != NULL);

	do
	{
		getChunkTransportState(transportStates, motNodeID, &pEntry);

		/* handle pt-to-pt message. Primary */
		conn = pEntry->conns + targetRoute;
		/* only send to interested connections */
		if (!conn->stillActive)
		{
			break;
		}

		b->pri = conn->pBuff + conn->msgSize;
		b->prilen = Gp_max_packet_size - conn->msgSize;

		/* got buffer. */
		return;
	}
	while (0);

	/* buffer is missing ? */

	b->pri = NULL;
	b->prilen = 0;

	return;
}

/*
 * The fetches a direct pointer into our transmit buffers, along with
 * an indication as to how much data can be safely shoved into the
 * buffer (started at the pointed location).
 *
 * This works a lot like SendTupleChunkToAMS().
 */
void
putTransportDirectBuffer(ChunkTransportState *transportStates,
						 int16 motNodeID,
						 int16 targetRoute, int length)
{
	ChunkTransportStateEntry *pEntry = NULL;
	MotionConn *conn;

	if (!transportStates)
	{
		elog(FATAL, "putTransportDirectBuffer: no transport states");
	}
	else if (!transportStates->activated)
	{
		elog(FATAL, "putTransportDirectBuffer: inactive transport states");
	}
	else if (targetRoute == BROADCAST_SEGIDX)
	{
		elog(FATAL, "putTransportDirectBuffer: can't direct-transport to broadcast");
	}

	getChunkTransportState(transportStates, motNodeID, &pEntry);

	/* handle pt-to-pt message. Primary */
	conn = pEntry->conns + targetRoute;
	/* only send to interested connections */
	if (conn->stillActive)
	{
		conn->msgSize += length;
		conn->tupleCount++;
	}

	/* put buffer. */
	return;
}

/*
 * DeregisterReadInterest is called on receiving nodes when they
 * believe that they're done with the receiver
 */
void
DeregisterReadInterest(ChunkTransportState *transportStates,
					   int motNodeID,
					   int srcRoute,
					   const char *reason)
{
	ChunkTransportStateEntry *pEntry = NULL;
	MotionConn *conn;

	if (!transportStates)
	{
		elog(FATAL, "DeregisterReadInterest: no transport states");
	}

	if (!transportStates->activated)
		return;

	getChunkTransportState(transportStates, motNodeID, &pEntry);
	conn = pEntry->conns + srcRoute;

	if (gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG)
	{
		elog(DEBUG3, "Interconnect finished receiving "
			 "from seg%d slice%d %s pid=%d sockfd=%d; %s",
			 conn->remoteContentId,
			 pEntry->sendSlice->sliceIndex,
			 conn->remoteHostAndPort,
			 conn->cdbProc->pid,
			 conn->sockfd,
			 reason);
	}

	if (Gp_interconnect_type == INTERCONNECT_TYPE_UDPIFC)
	{
#ifdef AMS_VERBOSE_LOGGING
		elog(LOG, "deregisterReadInterest set stillactive = false for node %d route %d (%s)", motNodeID, srcRoute, reason);
#endif
		markUDPConnInactiveIFC(conn);
	}
	else
	{
		/*
		 * we also mark the connection as "done." The way synchronization
		 * works is strange. On QDs the "teardown" doesn't get called until
		 * all segments are finished, which means that we need some way for
		 * the QEs to know that Teardown should complete, otherwise we
		 * deadlock the entire query (QEs wait in their Teardown calls, while
		 * the QD waits for them to finish)
		 */
		shutdown(conn->sockfd, SHUT_WR);

		MPP_FD_CLR(conn->sockfd, &pEntry->readSet);
	}
	return;
}

void
SetupInterconnect(EState *estate)
{
	interconnect_handle_t *h;
	MemoryContext oldContext;

	if (estate->interconnect_context)
	{
		elog(ERROR, "SetupInterconnect: already initialized.");
	}
	else if (!estate->es_sliceTable)
	{
		elog(ERROR, "SetupInterconnect: no slice table ?");
	}

	h = allocate_interconnect_handle();

	Assert(InterconnectContext != NULL);
	oldContext = MemoryContextSwitchTo(InterconnectContext);

	if (Gp_interconnect_type == INTERCONNECT_TYPE_UDPIFC)
		SetupUDPIFCInterconnect(estate);
	else if (Gp_interconnect_type == INTERCONNECT_TYPE_TCP)
		SetupTCPInterconnect(estate);
	else
		elog(ERROR, "unsupported expected interconnect type");

	MemoryContextSwitchTo(oldContext);

	h->interconnect_context = estate->interconnect_context;
}

/* TeardownInterconnect() function is used to cleanup interconnect resources that
 * were allocated during SetupInterconnect().  This function should ALWAYS be
 * called after SetupInterconnect to avoid leaking resources (like sockets)
 * even if SetupInterconnect did not complete correctly.
 */
void
TeardownInterconnect(ChunkTransportState *transportStates, bool hasErrors)
{
	interconnect_handle_t *h = find_interconnect_handle(transportStates);

	if (Gp_interconnect_type == INTERCONNECT_TYPE_UDPIFC)
	{
		TeardownUDPIFCInterconnect(transportStates, hasErrors);
	}
	else if (Gp_interconnect_type == INTERCONNECT_TYPE_TCP)
	{
		TeardownTCPInterconnect(transportStates, hasErrors);
	}

	if (h != NULL)
		destroy_interconnect_handle(h);
}

/*=========================================================================
 * HELPER FUNCTIONS
 */


/* Function createChunkTransportState() is used to create a ChunkTransportState struct and
 * place it in the hashtab hashtable based on the motNodeID.
 *
 * PARAMETERS
 *
 *	 motNodeID - motion node ID for this ChunkTransportState.
 *
 *	 numConns  - number of primary connections for this motion node.
 *               All are incoming if this is a receiving motion node.
 *               All are outgoing if this is a sending motion node.
 *
 * RETURNS
 *	 An empty and initialized ChunkTransportState struct for the given motion node. If
 *	 a ChuckTransportState struct is already registered for the motNodeID an ERROR is
 *	 thrown.
 */
ChunkTransportStateEntry *
createChunkTransportState(ChunkTransportState *transportStates,
						  ExecSlice *sendSlice,
						  ExecSlice *recvSlice,
						  int numConns)
{
	ChunkTransportStateEntry *pEntry;
	int			motNodeID;
	int			i;

	Assert(recvSlice->sliceIndex >= 0);
	Assert(sendSlice->sliceIndex > 0);

	motNodeID = sendSlice->sliceIndex;
	if (motNodeID > transportStates->size)
	{
		/* increase size of our table */
		ChunkTransportStateEntry *newTable;

		newTable = repalloc(transportStates->states, motNodeID * sizeof(ChunkTransportStateEntry));
		transportStates->states = newTable;
		/* zero-out the new piece at the end */
		MemSet(&transportStates->states[transportStates->size], 0, (motNodeID - transportStates->size) * sizeof(ChunkTransportStateEntry));
		transportStates->size = motNodeID;
	}

	pEntry = &transportStates->states[motNodeID - 1];

	if (pEntry->valid)
	{
		ereport(ERROR,
				(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
				 errmsg("interconnect error: A HTAB entry for motion node %d already exists",
						motNodeID),
				 errdetail("conns %p numConns %d first sock %d",
						   pEntry->conns, pEntry->numConns,
						   pEntry->conns[0].sockfd)));
	}

	pEntry->valid = true;

	pEntry->motNodeId = motNodeID;
	pEntry->numConns = numConns;
	pEntry->scanStart = 0;
	pEntry->sendSlice = sendSlice;
	pEntry->recvSlice = recvSlice;

	pEntry->conns = palloc0(pEntry->numConns * sizeof(pEntry->conns[0]));

	for (i = 0; i < pEntry->numConns; i++)
	{
		MotionConn *conn = &pEntry->conns[i];

		/* Initialize MotionConn entry. */
		conn->state = mcsNull;
		conn->sockfd = -1;
		conn->msgSize = 0;
		conn->tupleCount = 0;
		conn->stillActive = false;
		conn->stopRequested = false;
		conn->wakeup_ms = 0;
		conn->cdbProc = NULL;
		conn->sent_record_typmod = 0;
		conn->remapper = NULL;
	}

	return pEntry;
}

/* Function removeChunkTransportState() is used to remove a ChunkTransportState struct from
 * the hashtab hashtable.
 *
 * This should only be called after createChunkTransportState().
 *
 * PARAMETERS
 *
 *	 motNodeID - motion node ID to lookup the ChunkTransportState.
 *   pIncIdx - parent slice idx in child slice.  If not multiplexed, should be 1.
 *
 * RETURNS
 *	 The ChunkTransportState that was removed from the hashtab hashtable.
 */
ChunkTransportStateEntry *
removeChunkTransportState(ChunkTransportState *transportStates,
						  int16 motNodeID)
{
	ChunkTransportStateEntry *pEntry = NULL;

	if (motNodeID > transportStates->size)
	{
		ereport(ERROR,
				(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
				 errmsg("interconnect error: Unexpected Motion Node Id: %d",
						motNodeID),
				 errdetail("During remove. (size %d)", transportStates->size)));
	}
	else if (!transportStates->states[motNodeID - 1].valid)
	{
		ereport(ERROR,
				(errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
				 errmsg("interconnect error: Unexpected Motion Node Id: %d",
						motNodeID),
				 errdetail("During remove. State not valid")));
	}
	else
	{
		transportStates->states[motNodeID - 1].valid = false;
		pEntry = &transportStates->states[motNodeID - 1];
	}

	MPP_FD_ZERO(&pEntry->readSet);

	return pEntry;
}

/*
 * checkForCancelFromQD
 * 		Check for cancel from QD.
 *
 * Should be called only inside the dispatcher
 */
void
checkForCancelFromQD(ChunkTransportState *pTransportStates)
{
	Assert(Gp_role == GP_ROLE_DISPATCH);
	Assert(pTransportStates);
	Assert(pTransportStates->estate);

	if (cdbdisp_checkForCancel(pTransportStates->estate->dispatcherState))
	{
		ereport(ERROR, (errcode(ERRCODE_GP_INTERCONNECTION_ERROR),
						errmsg(CDB_MOTION_LOST_CONTACT_STRING)));
		/* not reached */
	}
}

/*
* WaitInterconnectQuit
*
* Wait for the ic thread to quit, don't clean any resource owned by ic thread
*/
void
WaitInterconnectQuit(void)
{
	if (Gp_interconnect_type == INTERCONNECT_TYPE_UDPIFC)
	{
		WaitInterconnectQuitUDPIFC();
	}
}

interconnect_handle_t *
allocate_interconnect_handle(void)
{
	interconnect_handle_t	*h;

	if (InterconnectContext == NULL)
		InterconnectContext = AllocSetContextCreate(TopMemoryContext,
												   "Interconnect Context",
												   ALLOCSET_DEFAULT_MINSIZE,
												   ALLOCSET_DEFAULT_INITSIZE,
												   ALLOCSET_DEFAULT_MAXSIZE);

	h = MemoryContextAllocZero(InterconnectContext, sizeof(interconnect_handle_t));

	h->owner = CurrentResourceOwner;
	h->next = open_interconnect_handles;
	h->prev = NULL;
	if (open_interconnect_handles)
		open_interconnect_handles->prev = h;
	open_interconnect_handles = h;

	if (!interconnect_resowner_callback_registered)
	{
		RegisterResourceReleaseCallback(interconnect_abort_callback, NULL);
		interconnect_resowner_callback_registered = true;
	}
	return h;
}

static void
destroy_interconnect_handle(interconnect_handle_t *h)
{
	h->interconnect_context = NULL;
	/* unlink from linked list first */
	if (h->prev)
		h->prev->next = h->next;
	else
		open_interconnect_handles = h->next;
	if (h->next)
		h->next->prev = h->prev;

	pfree(h);

	if (open_interconnect_handles == NULL)
		MemoryContextReset(InterconnectContext);
}

static interconnect_handle_t *
find_interconnect_handle(ChunkTransportState *icContext)
{
	interconnect_handle_t *head = open_interconnect_handles;
	while (head != NULL)
	{
		if (head->interconnect_context == icContext)
			return head;
		head = head->next;
	}
	return NULL;
}

static void
cleanup_interconnect_handle(interconnect_handle_t *h)
{
	if (h->interconnect_context == NULL)
	{
		destroy_interconnect_handle(h);
		return;
	}
	TeardownInterconnect(h->interconnect_context, true);
}

static void
interconnect_abort_callback(ResourceReleasePhase phase,
					   bool isCommit,
					   bool isTopLevel,
					   void *arg)
{
	interconnect_handle_t *curr;
	interconnect_handle_t *next;

	if (phase != RESOURCE_RELEASE_AFTER_LOCKS)
		return;

	next = open_interconnect_handles;
	while (next)
	{
		curr = next;
		next = curr->next;

		if (curr->owner == CurrentResourceOwner)
		{
			if (isCommit)
				elog(WARNING, "interconnect reference leak: %p still referenced", curr);

			cleanup_interconnect_handle(curr);
		}
	}
}

/*
 * format_sockaddr
 *			Format a sockaddr to a human readable string
 *
 * This function must be kept threadsafe, elog/ereport/palloc etc are not
 * allowed within this function.
 */
char *
format_sockaddr(struct sockaddr_storage *sa, char *buf, size_t len)
{
	int			ret;
	char		remote_host[NI_MAXHOST];
	char		remote_port[NI_MAXSERV];

	ret = pg_getnameinfo_all(sa, sizeof(struct sockaddr_storage),
							 remote_host, sizeof(remote_host),
							 remote_port, sizeof(remote_port),
							 NI_NUMERICHOST | NI_NUMERICSERV);

	if (ret != 0)
		snprintf(buf, len, "?host?:?port?");
	else
	{
#ifdef HAVE_IPV6
		if (sa->ss_family == AF_INET6)
			snprintf(buf, len, "[%s]:%s", remote_host, remote_port);
		else
#endif
			snprintf(buf, len, "%s:%s", remote_host, remote_port);
	}

	return buf;
}
