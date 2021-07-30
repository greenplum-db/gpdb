/*-------------------------------------------------------------------------
 * cdbendpoint.c
 *
 * An endpoint is a query result source for a parallel retrieve cursor on a
 * dedicated QE. One parallel retrieve cursor could have multiple endpoints
 * on different QEs to allow retrieving in parallel.
 *
 * This file implements the sender part of endpoint.
 *
 * Endpoint may exist on the coordinator or segments, depends on the query of
 * the PARALLEL RETRIEVE CURSOR:
 * (1) An endpoint is on QD only if the query of the parallel cursor needs to
 *     be finally gathered by the master. e.g.:
 *     > DECLARE c1 PARALLEL RETRIEVE CURSOR FOR SELECT * FROM T1 ORDER BY C1;
 * (2) The endpoints are on specific segments node if the direct dispatch happens.
 *	   e.g.:
 *     > DECLARE c1 PARALLEL RETRIEVE CURSOR FOR SELECT * FROM T1 WHERE C1=1;
 * (3) The endpoints are on all segments node. e.g:
 *     > DECLARE c1 PARALLEL RETRIEVE CURSOR FOR SELECT * FROM T1;
 *
 * When a parallel retrieve cursor is declared, the query plan will be
 * dispatched to the corresponding QEs. Before the query execution, endpoints
 * will be created first on QEs. An instance of Endpoint struct in the shared
 * memory represents the endpoint. Through the Endpoint, the client could know
 * the endpoint's identification (endpoint name), location (dbid, host, port
 * and session id), and the state for the retrieve session. All of those
 * information can be obtained on QD by UDF gp_endpoints() via dispatching
 * endpoint queries or on QE's retrieve session by UDF gp_segment_endpoints().
 *
 * Instead of returning the query result to QD through a normal dest receiver,
 * endpoints write the results to TQueueDestReceiver which is a shared memory
 * queue and can be retrieved from a different process. See
 * SetupEndpointExecState(). The information about the message queue
 * is also stored in the Endpoint so that the retrieve session on the same QE
 * can know.
 *
 * The token is stored in a different structure SessionInfoEntry to make the
 * tokens same for all endpoints in the same session. The token is created on
 * each QE after plan get dispatched.
 *
 * DECLARE returns only when endpoint and token are ready and query starts
 * execution. See WaitEndpointsReady().
 *
 * When the query finishes, the endpoint won't be destroyed immediately since we
 * may still want to check its state on QD. In the implementation, the
 * DestroyEndpointExecState() is blocked until the parallel retrieve cursor
 * is closed explicitly through CLOSE statement or error happens.
 *
 * UDF gp_check_parallel_retrieve_cursor() and
 * gp_wait_parallel_retrieve_cursor() are supplied as helper functions
 * to monitor the retrieve state. They should be run in the declare transaction
 * block on QD.
 *
 * Copyright (c) 2020-Present VMware, Inc. or its affiliates
 *
 * IDENTIFICATION
 *		src/backend/cdb/cdbendpoint.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/session.h"
#include "access/tupdesc.h"
#include "access/xact.h"
#include "libpq-fe.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "pgstat.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/procsignal.h"
#include "utils/backend_cancel.h"
#include "utils/builtins.h"
#include "utils/faultinjector.h"
#include "cdb/cdbdisp_query.h"
#include "cdb/cdbdispatchresult.h"
#include "cdb/cdbendpoint.h"
#include "cdbendpoint_private.h"
#include "cdb/cdbsrlz.h"
#include "cdb/cdbvars.h"

#define WAIT_ENDPOINT_TIMEOUT				100

/*
 * The size of endpoint tuple queue in bytes.
 * This value refers upstream PARALLEL_TUPLE_QUEUE_SIZE
 */
#define ENDPOINT_TUPLE_QUEUE_SIZE		65536

#define SHMEM_ENDPOINTS_ENTRIES			"SharedMemoryEndpointEntries"
#define SHMEM_ENPOINTS_SESSION_INFO		"EndpointsSessionInfosHashtable"

#ifdef FAULT_INJECTOR
#define DUMMY_ENDPOINT_NAME "DUMMYENDPOINTNAME"
#define DUMMY_CURSOR_NAME	"DUMMYCURSORNAME"
#endif

static EndpointExecState *CurrEndpointExecState;

typedef struct SessionTokenTag
{
	int			sessionID;
	Oid			userID;
}	SessionTokenTag;

/*
 * sharedSessionInfoHash is located in shared memory on each segment for
 * authentication purpose.
 */
typedef struct SessionInfoEntry
{
	SessionTokenTag tag;

	/* The auth token for this session. */
	int8		token[ENDPOINT_TOKEN_HEX_LEN];

	/* How many endpoints are referred to this entry. */
	uint16		refCount;

}	SessionInfoEntry;

/* Shared hash table for session infos */
static HTAB *sharedSessionInfoHash = NULL;

/* Point to Endpoint entries in shared memory */
static struct EndpointData *sharedEndpoints = NULL;

/* Init helper functions */
static void InitSharedEndpoints(void);

/* Token utility functions */
static const int8 *get_or_create_token(void);

/* Endpoint helper function */
static void EndpointNotifyQD(const char *message);
static Endpoint alloc_endpoint(const char *cursorName, dsm_handle dsmHandle);
static void free_endpoint(Endpoint endpoint);
static void create_and_connect_mq(TupleDesc tupleDesc,
					  dsm_segment **mqSeg /* out */ ,
					  shm_mq_handle **mqHandle /* out */ );
static void detach_mq(dsm_segment *dsmSeg);
static void setup_session_info_entry(void);
static void wait_receiver(EndpointExecState *state);
static void unset_endpoint_sender_pid(Endpoint endPoint);
static void abort_endpoint(EndpointExecState *state);
static void wait_parallel_retrieve_close(void);

/* utility */
static void generate_endpoint_name(char *name, const char *cursorName);

/*
 * Calculate the shared memory size for PARALLEL RETRIEVE CURSOR execute.
 */
Size
EndpointShmemSize(void)
{
	Size		size;

	size = MAXALIGN(mul_size(MAX_ENDPOINT_SIZE, sizeof(struct EndpointData)));
	/*
	 * Maximum parallel retrieve cursor session number should be no more than
	 * the maximum endpoint number, so use MAX_ENDPOINT_SIZE here.
	 */
	size = add_size(
	  size, hash_estimate_size(MAX_ENDPOINT_SIZE, sizeof(SessionInfoEntry)));
	return size;
}

/*
 * Initialize shared memory for PARALLEL RETRIEVE CURSOR.
 */
void
EndpointShmemInit(void)
{
	bool		found;
	HASHCTL		hctl;

	sharedEndpoints = (Endpoint)
		ShmemInitStruct(SHMEM_ENDPOINTS_ENTRIES,
						MAXALIGN(mul_size(MAX_ENDPOINT_SIZE, sizeof(struct EndpointData))),
						&found);
	if (!found)
		InitSharedEndpoints();

	memset(&hctl, 0, sizeof(hctl));
	hctl.keysize = sizeof(SessionTokenTag);
	hctl.entrysize = sizeof(SessionInfoEntry);
	hctl.hash = tag_hash;
	sharedSessionInfoHash =
		ShmemInitHash(SHMEM_ENPOINTS_SESSION_INFO, MAX_ENDPOINT_SIZE,
					  MAX_ENDPOINT_SIZE, &hctl, HASH_ELEM | HASH_FUNCTION);
}

/*
 * Initialize shared memory Endpoint array.
 */
static void
InitSharedEndpoints()
{
	Endpoint endpoints = sharedEndpoints;

	for (int i = 0; i < MAX_ENDPOINT_SIZE; ++i)
	{
		endpoints[i].name[0] = '\0';
		endpoints[i].cursorName[0] = '\0';
		endpoints[i].databaseID = InvalidOid;
		endpoints[i].senderPid = InvalidPid;
		endpoints[i].receiverPid = InvalidPid;
		endpoints[i].mqDsmHandle = DSM_HANDLE_INVALID;
		endpoints[i].sessionDsmHandle = DSM_HANDLE_INVALID;
		endpoints[i].sessionID = InvalidEndpointSessionId;
		endpoints[i].userID = InvalidOid;
		endpoints[i].state = ENDPOINTSTATE_INVALID;
		endpoints[i].empty = true;
		InitSharedLatch(&endpoints[i].ackDone);
	}
}

/*
 * Get where is the endpoint located? Currently used in EXPLAIN only.
 */
enum EndPointExecPosition
GetParallelCursorEndpointPosition(PlannedStmt *plan)
{
	if (plan->planTree->flow->flotype == FLOW_SINGLETON)
	{
		if (plan->planTree->flow->locustype == CdbLocusType_SegmentGeneral)
			return ENDPOINT_ON_SINGLE_QE;
		else
			return ENDPOINT_ON_ENTRY_DB;
	}
	else if (plan->slices[0].directDispatch.isDirectDispatch &&
			 plan->slices[0].directDispatch.contentIds != NULL)
	{
		return ENDPOINT_ON_SOME_QE;
	}
	else
		return ENDPOINT_ON_ALL_QE;
}

/*
 * QD waits until the cursor ready for retrieve on the related segments.
 */
void
WaitEndpointsReady(EState *estate)
{
	Assert(estate);
	CdbDispatcherState *ds = estate->dispatcherState;

	cdbdisp_checkDispatchAckMessage(ds, ENDPOINT_READY_ACK_MSG, true, DISPATCH_WAIT_ACK_ROOT);
	check_parallel_retrieve_cursor_errors(estate);
}

/*
 * Get or create a authentication token for current session.
 */
static const int8 *
get_or_create_token(void)
{
	static int	sessionId = InvalidEndpointSessionId;
	static int8 currentToken[ENDPOINT_TOKEN_HEX_LEN] = {0};

	if (sessionId != gp_session_id)
	{
		sessionId = gp_session_id;
		if (!pg_strong_random(currentToken, ENDPOINT_TOKEN_HEX_LEN))
			ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
							errmsg("failed to generate a new random token.")));
	}
	return currentToken;
}

/*
 * Send acknowledge message to QD.
 */
static void
EndpointNotifyQD(const char *message)
{
	StringInfoData buf;

	pq_beginmessage(&buf, 'A');
	pq_sendint(&buf, MyProcPid, sizeof(int32));
	pq_sendstring(&buf, CDB_NOTIFY_ENDPOINT_ACK);
	pq_sendstring(&buf, message);
	pq_endmessage(&buf);
	pq_flush();
}

/*
 * Allocate and initialize an endpoint and then create a dest receiver for
 * PARALLEL RETRIEVE CURSOR. The dest receiver is based on shm_mq that is used
 * by the upstream parallel work.
 */
void
SetupEndpointExecState(TupleDesc tupleDesc, const char *cursorName,
					   EndpointExecState *state)
{
	shm_mq_handle	*shmMqHandle;
	DestReceiver	*endpointDest;

	/*
	 * The message queue needs to be created first since the dsm_handle has to
	 * be ready when create EndpointDesc entry.
	 */
	create_and_connect_mq(tupleDesc, &state->dsmSeg, &shmMqHandle);

	/*
	 * Alloc endpoint and set it as the active one for sender.
	 */
	state->endpoint =
		alloc_endpoint(cursorName, dsm_segment_handle(state->dsmSeg));
	setup_session_info_entry();

	/*
	 * Once the endpoint has been created in shared memory, send acknowledge
	 * message to QD so DECLARE PARALLEL RETRIEVE CURSOR statement can finish.
	 */
	EndpointNotifyQD(ENDPOINT_READY_ACK_MSG);
	endpointDest	= CreateTupleQueueDestReceiver(shmMqHandle);
	state->dest = endpointDest;
}

/*
 * Wait until the endpoint finishes and then clean up.
 *
 * If the queue is large enough for tuples to send, must wait for a receiver
 * to attach the message queue before endpoint detaches the message queue.
 * Cause if the queue gets detached before receiver attaches, the queue
 * will never be attached by a receiver.
 *
 * Should also clean all other endpoint info here.
 */
void
DestroyEndpointExecState(EndpointExecState *state)
{
	DestReceiver	*endpointDest = state->dest;

	Assert(state->endpoint);
	Assert(state->dsmSeg);

	/*
	 * wait for receiver to start tuple retrieving. ackDone latch will
	 * be reset to be re-used when retrieving finished. See notify_sender()
	 * callers.
	 */
	wait_receiver(state);

	/*
	 * tqueueShutdownReceiver() (rShutdown callback) will call
	 * shm_mq_detach(), so need to call it before detach_mq(). Retrieving
	 * session will set ackDone latch again after shm_mq_detach() called here.
	 */
	(*endpointDest->rShutdown) (endpointDest);
	(*endpointDest->rDestroy) (endpointDest);
	state->dest = NULL;

	/*
	 * Wait until all data is retrieved by receiver. This is needed because
	 * when the endpoint sends all data to shared message queue. The retrieve
	 * session may still not get all data.
	 */
	wait_receiver(state);

	LWLockAcquire(ParallelCursorEndpointLock, LW_EXCLUSIVE);
	unset_endpoint_sender_pid(state->endpoint);
	LWLockRelease(ParallelCursorEndpointLock);
	/* Notify QD */
	EndpointNotifyQD(ENDPOINT_FINISHED_ACK_MSG);

	/*
	 * If all data get sent, hang the process and wait for QD to close it. The
	 * purpose is to not clean up Endpoint entry until CLOSE/COMMIT/ABORT
	 * (i.e. PortalCleanup get executed). So user can still see the finished
	 * endpoint status through the gp_endpoints() UDF. This is needed because
	 * pg_cursor view can still see the PARALLEL RETRIEVE CURSOR
	 */
	wait_parallel_retrieve_close();

	LWLockAcquire(ParallelCursorEndpointLock, LW_EXCLUSIVE);
	free_endpoint(state->endpoint);
	LWLockRelease(ParallelCursorEndpointLock);
	state->endpoint = NULL;

	detach_mq(state->dsmSeg);
	state->dsmSeg = NULL;

	CurrEndpointExecState = NULL;
}

/*
 * Allocate an Endpoint entry in shared memory.
 *
 * cursorName - the parallel retrieve cursor name.
 * dsmHandle  - dsm handle of shared memory message queue.
 */
static Endpoint
alloc_endpoint(const char *cursorName, dsm_handle dsmHandle)
{
	int			i;
	int			foundIdx = -1;
	Endpoint	ret = NULL;
	dsm_handle	session_dsm_handle;

	session_dsm_handle = GetSessionDsmHandle();
	if (session_dsm_handle == DSM_HANDLE_INVALID)
		ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY),
				errmsg("failed to create the per-session DSM segment.")));

	LWLockAcquire(ParallelCursorEndpointLock, LW_EXCLUSIVE);

#ifdef FAULT_INJECTOR
	/* inject fault "skip" to set end-point shared memory slot full */
	if (SIMPLE_FAULT_INJECTOR("alloc_endpoint_slot_full") == FaultInjectorTypeSkip)
	{
		for (i = 0; i < MAX_ENDPOINT_SIZE; ++i)
		{
			if (sharedEndpoints[i].empty)
			{
				/* pretend to set a valid endpoint */
				snprintf(sharedEndpoints[i].name, NAMEDATALEN, "%s",
						 DUMMY_ENDPOINT_NAME);
				snprintf(sharedEndpoints[i].cursorName, NAMEDATALEN, "%s",
						 DUMMY_CURSOR_NAME);
				sharedEndpoints[i].databaseID = MyDatabaseId;
				sharedEndpoints[i].mqDsmHandle = DSM_HANDLE_INVALID;
				sharedEndpoints[i].sessionDsmHandle = DSM_HANDLE_INVALID;
				sharedEndpoints[i].sessionID = gp_session_id;
				sharedEndpoints[i].userID = GetSessionUserId();
				sharedEndpoints[i].senderPid = InvalidPid;
				sharedEndpoints[i].receiverPid = InvalidPid;
				sharedEndpoints[i].empty = false;
			}
		}
	}

	if (SIMPLE_FAULT_INJECTOR("alloc_endpoint_slot_full_reset") == FaultInjectorTypeSkip)
	{
		for (i = 0; i < MAX_ENDPOINT_SIZE; ++i)
		{
			if (endpoint_name_equals(sharedEndpoints[i].name,
									 DUMMY_ENDPOINT_NAME))
			{
				sharedEndpoints[i].mqDsmHandle = DSM_HANDLE_INVALID;
				sharedEndpoints[i].empty = true;
			}
		}
	}
#endif

	/* find an available slot */
	for (i = 0; i < MAX_ENDPOINT_SIZE; ++i)
	{
		if (sharedEndpoints[i].empty)
		{
			foundIdx = i;
			break;
		}
	}

	if (foundIdx == -1)
		ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
						errmsg("failed to allocate endpoint")));

	generate_endpoint_name(sharedEndpoints[i].name, cursorName);
	StrNCpy(sharedEndpoints[i].cursorName, cursorName, NAMEDATALEN);
	sharedEndpoints[i].databaseID = MyDatabaseId;
	sharedEndpoints[i].sessionID = gp_session_id;
	sharedEndpoints[i].userID = GetSessionUserId();
	sharedEndpoints[i].senderPid = MyProcPid;
	sharedEndpoints[i].receiverPid = InvalidPid;
	sharedEndpoints[i].state = ENDPOINTSTATE_READY;
	sharedEndpoints[i].empty = false;
	sharedEndpoints[i].mqDsmHandle = dsmHandle;
	sharedEndpoints[i].sessionDsmHandle = session_dsm_handle;
	OwnLatch(&sharedEndpoints[i].ackDone);
	ret = &sharedEndpoints[i];

	LWLockRelease(ParallelCursorEndpointLock);
	return ret;
}

/*
 * Create and setup the shared memory message queue.
 *
 * Create a dsm which contains a TOC(table of content). It has 3 parts:
 * 1. Tuple's TupleDesc length.
 * 2. Tuple's TupleDesc.
 * 3. Shared memory message queue.
 */
static void
create_and_connect_mq(TupleDesc tupleDesc, dsm_segment **mqSeg /* out */ ,
					  shm_mq_handle **mqHandle /* out */ )
{
	shm_toc    *toc;
	shm_mq	   *mq;
	shm_toc_estimator tocEst;
	Size		tocSize;
	int			tupdescLen;
	char	   *tupdescSer;
	char	   *tdlenSpace;
	char	   *tupdescSpace;
	TupleDescNode *node = makeNode(TupleDescNode);

	elog(DEBUG3,
		 "CDB_ENDPOINTS: create and setup the shared memory message queue.");

	/* Serialize TupleDesc */
	node->natts = tupleDesc->natts;
	node->tuple = tupleDesc;
	tupdescSer =
		serializeNode((Node *) node, &tupdescLen, NULL /* uncompressed_size */ );

	/* Estimate the dsm size */
	shm_toc_initialize_estimator(&tocEst);
	shm_toc_estimate_chunk(&tocEst, sizeof(tupdescLen));
	shm_toc_estimate_chunk(&tocEst, tupdescLen);
	shm_toc_estimate_chunk(&tocEst, ENDPOINT_TUPLE_QUEUE_SIZE);
	shm_toc_estimate_keys(&tocEst, 3);
	tocSize = shm_toc_estimate(&tocEst);

	/* Create dsm and initilaize toc. */
	*mqSeg = dsm_create(tocSize, 0);
	dsm_pin_mapping(*mqSeg);

	toc = shm_toc_create(ENDPOINT_MSG_QUEUE_MAGIC, dsm_segment_address(*mqSeg),
						 tocSize);

	tdlenSpace = shm_toc_allocate(toc, sizeof(tupdescLen));
	memcpy(tdlenSpace, &tupdescLen, sizeof(tupdescLen));
	shm_toc_insert(toc, ENDPOINT_KEY_TUPLE_DESC_LEN, tdlenSpace);

	tupdescSpace = shm_toc_allocate(toc, tupdescLen);
	memcpy(tupdescSpace, tupdescSer, tupdescLen);
	shm_toc_insert(toc, ENDPOINT_KEY_TUPLE_DESC, tupdescSpace);

	mq = shm_mq_create(shm_toc_allocate(toc, ENDPOINT_TUPLE_QUEUE_SIZE),
					   ENDPOINT_TUPLE_QUEUE_SIZE);
	shm_toc_insert(toc, ENDPOINT_KEY_TUPLE_QUEUE, mq);
	shm_mq_set_sender(mq, MyProc);
	*mqHandle = shm_mq_attach(mq, *mqSeg, NULL);
}

/*
 * Create/reuse SessionInfoEntry for current session in shared memory.
 * SessionInfoEntry is used for authentication in the retrieve sessions.
 */
static void
setup_session_info_entry()
{
	SessionInfoEntry *infoEntry = NULL;
	bool		found = false;
	SessionTokenTag tag;
	const int8 *token = NULL;

	tag.sessionID = gp_session_id;
	tag.userID = GetSessionUserId();

	LWLockAcquire(ParallelCursorEndpointLock, LW_EXCLUSIVE);
	infoEntry = (SessionInfoEntry *) hash_search(sharedSessionInfoHash, &tag,
												 HASH_ENTER, &found);
	elog(DEBUG3, "CDB_ENDPOINT: Finish endpoint init. Found SessionInfoEntry? %d",
		 found);

	/*
	 * Save the token if it is the first time we create endpoint in current
	 * session. One session will be mapped to one token only.
	 */
	if (!found)
	{
		token = get_or_create_token();
		memcpy(infoEntry->token, token, ENDPOINT_TOKEN_HEX_LEN);
		infoEntry->refCount = 0;
	}

	infoEntry->refCount++;
	Assert(infoEntry->refCount <= MAX_ENDPOINT_SIZE);

	LWLockRelease(ParallelCursorEndpointLock);
}

/*
 * check if QD connection still alive.
 */
static bool
checkQDConnectionAlive()
{
	ssize_t		ret;
	char		buf;

	Assert(MyProcPort != NULL);

	if (MyProcPort->sock < 0)
		return false;

#ifndef WIN32
	ret = recv(MyProcPort->sock, &buf, 1, MSG_PEEK | MSG_DONTWAIT);
#else
	ret = recv(MyProcPort->sock, &buf, 1, MSG_PEEK | MSG_PARTIAL);
#endif

	if (ret == 0)				/* socket has been closed. EOF */
		return false;

	if (ret > 0)				/* data waiting on socket, it must be OK. */
		return true;

	if (ret == -1)				/* error, or would be block. */
	{
		if (errno == EAGAIN || errno == EINPROGRESS)
			return true;		/* connection intact, no data available */
		else
			return false;
	}

	/* not reached */
	return true;
}

/*
 * wait_receiver - wait receiver to retrieve at least once from the
 * shared memory message queue.
 *
 * If the queue only attached by the sender and the queue is large enough
 * for all tuples, sender should wait receiver. Cause if sender detached
 * from the queue, the queue will be not available for receiver.
 */
static void
wait_receiver(EndpointExecState *state)
{
	elog(DEBUG3, "CDB_ENDPOINTS: wait receiver.");
	while (true)
	{
		int			wr = 0;

		CHECK_FOR_INTERRUPTS();

		if (QueryFinishPending)
			break;

		elog(DEBUG5, "CDB_ENDPOINT: sender wait latch in wait_receiver()");
		wr = WaitLatchOrSocket(&state->endpoint->ackDone,
							   WL_LATCH_SET | WL_POSTMASTER_DEATH | WL_TIMEOUT | WL_SOCKET_READABLE,
							   MyProcPort->sock,
							   WAIT_ENDPOINT_TIMEOUT,
							   PG_WAIT_PARALLEL_RETRIEVE_CURSOR);

		if (wr & WL_SOCKET_READABLE)
		{
			if (!checkQDConnectionAlive())
			{
				ereport(LOG,
						(errmsg("CDB_ENDPOINT: sender found that the connection to QD is broken.")));
				abort_endpoint(state);
				proc_exit(0);
			}
		}

		if (wr & WL_POSTMASTER_DEATH)
		{
			abort_endpoint(state);
			ereport(LOG,
					(errmsg("CDB_ENDPOINT: postmaster exit, close shared memory message queue.")));
			proc_exit(0);
		}

		if(wr & WL_LATCH_SET)
		{
			elog(DEBUG3, "CDB_ENDPOINT:sender reset latch in wait_receiver()");
			ResetLatch(&state->endpoint->ackDone);
			break;
		}
	}
}

/*
 * Detach the shared memory message queue.
 * This should happen after free endpoint, otherwise endpoint->mq_dsm_handle
 * becomes invalid pointer.
 */
static void
detach_mq(dsm_segment *dsmSeg)
{
	elog(DEBUG3, "CDB_ENDPOINT: Sender message queue detaching. '%p'",
		 (void *) dsmSeg);

	Assert(dsmSeg);
	dsm_detach(dsmSeg);
}

/*
 * Unset endpoint sender pid.
 *
 * Clean the Endpoint entry sender pid when endpoint finish it's
 * job or abort.
 * Needs to be called with exclusive lock on ParallelCursorEndpointLock.
 */
static void
unset_endpoint_sender_pid(Endpoint endpoint)
{
	SessionTokenTag tag;

	tag.sessionID = gp_session_id;
	tag.userID = GetSessionUserId();

	if (endpoint == NULL || endpoint->empty)
		return;

	elog(DEBUG3, "CDB_ENDPOINT: unset endpoint sender pid.");

	/*
	 * Only the endpoint QE/entry DB execute this unset sender pid function.
	 * The sender pid in Endpoint entry must be MyProcPid or InvalidPid.
	 */
	Assert(MyProcPid == endpoint->senderPid ||
		   endpoint->senderPid == InvalidPid);
	if (MyProcPid == endpoint->senderPid)
	{
		endpoint->senderPid = InvalidPid;
	}
}

/*
 * abort_endpoint - xact abort routine for endpoint
 */
static void
abort_endpoint(EndpointExecState *state)
{
	if (state->dest)
	{
		/*
		 * rShutdown callback will call shm_mq_detach(),
		 * so need to call it before detach_mq() to clean up.
		 */
		DestReceiver    *endpointDest = state->dest;
		(*endpointDest->rShutdown) (endpointDest);
		(*endpointDest->rDestroy) (endpointDest);
		state->dest = NULL;
	}

	if (state->endpoint)
	{
		LWLockAcquire(ParallelCursorEndpointLock, LW_EXCLUSIVE);
		/*
		 * These two better be called in one lock section. So retriever abort
		 * will not execute extra works.
		 */
		unset_endpoint_sender_pid(state->endpoint);
		free_endpoint(state->endpoint);
		LWLockRelease(ParallelCursorEndpointLock);
		/* Notify QD */
		EndpointNotifyQD(ENDPOINT_FINISHED_ACK_MSG);
		state->endpoint = NULL;
	}

	/*
	 * During xact abort, should make sure the endpoint_cleanup called first.
	 * Cause if call detach_mq to detach the message queue first, the
	 * retriever may read NULL from message queue, then retrieve mark itself
	 * down.
	 *
	 * So here, need to make sure signal retrieve abort first before endpoint
	 * detach message queue.
	 */
	if (state->dsmSeg)
	{
		detach_mq(state->dsmSeg);
		state->dsmSeg = NULL;
	}
}

/*
 * Wait for PARALLEL RETRIEVE CURSOR cleanup after the endpoint sends all data.
 *
 * If all data get sent, hang the process and wait for QD to close it.
 * The purpose is to not clean up Endpoint entry until
 * CLOSE/COMMIT/ABORT (ie. PortalCleanup get executed).
 */
static void
wait_parallel_retrieve_close(void)
{
	ResetLatch(&MyProc->procLatch);
	while (true)
	{
		int			wr;

		CHECK_FOR_INTERRUPTS();

		if (QueryFinishPending)
			break;

		elog(DEBUG3, "CDB_ENDPOINT: wait for parallel retrieve cursor close");
		wr = WaitLatchOrSocket(&MyProc->procLatch,
							   WL_LATCH_SET | WL_POSTMASTER_DEATH | WL_TIMEOUT | WL_SOCKET_READABLE,
							   MyProcPort->sock,
							   WAIT_ENDPOINT_TIMEOUT,
							   PG_WAIT_PARALLEL_RETRIEVE_CURSOR);

		if (wr & WL_POSTMASTER_DEATH)
		{
			ereport(LOG,
					(errmsg("CDB_ENDPOINT: postmaster exit, close shared memory message queue.")));
			proc_exit(0);
		}

		if (wr & WL_SOCKET_READABLE)
		{
			if (!checkQDConnectionAlive())
			{
				ereport(LOG,
						(errmsg("CDB_ENDPOINT: sender found that the connection to QD is broken.")));
				proc_exit(0);
			}
		}

		if (wr & WL_LATCH_SET)
			ResetLatch(&MyProc->procLatch);

		/*
		 * procLatch may be set by a timeout, e.g. AuthenticationTimeout, to
		 * handle this case, we check QueryFinishPending and
		 * QueryCancelPending to make sure we can continue waiting.
		 */
		if (QueryFinishPending || QueryCancelPending)
		{
			elog(DEBUG3, "CDB_ENDPOINT: reset procLatch and quit waiting");
			break;
		}
	}
}

/*
 * free_endpoint - Frees the given endpoint.
 *
 * Needs to be called with exclusive lock on ParallelCursorEndpointLock.
 */
static void
free_endpoint(Endpoint endpoint)
{
	SessionTokenTag tag;
	SessionInfoEntry *infoEntry = NULL;

	Assert(endpoint);
	Assert(!endpoint->empty);

	elog(DEBUG3, "CDB_ENDPOINTS: Free endpoint '%s'.", endpoint->name);

	endpoint->databaseID = InvalidOid;
	endpoint->mqDsmHandle = DSM_HANDLE_INVALID;
	endpoint->sessionDsmHandle = DSM_HANDLE_INVALID;
	endpoint->empty = true;
	memset((char *) endpoint->name, '\0', NAMEDATALEN);
	memset((char *) endpoint->cursorName, '\0', NAMEDATALEN);
	ResetLatch(&endpoint->ackDone);
	DisownLatch(&endpoint->ackDone);

	tag.sessionID = endpoint->sessionID;
	tag.userID = endpoint->userID;
	infoEntry = (SessionInfoEntry *) hash_search(
							   sharedSessionInfoHash, &tag, HASH_FIND, NULL);
	if (infoEntry)
	{
		infoEntry->refCount--;
		if (infoEntry->refCount == 0)
			hash_search(sharedSessionInfoHash, &tag, HASH_REMOVE, NULL);
	}
	endpoint->sessionID = InvalidEndpointSessionId;
	endpoint->userID = InvalidOid;
}

Endpoint
get_endpointdesc_by_index(int index)
{
	Assert(index > -1 && index < MAX_ENDPOINT_SIZE);
	return &sharedEndpoints[index];
}

/*
 *
 * find_endpoint - Find the endpoint by given endpoint name and session id.
 *
 * For the endpoint, the session_id is the gp_session_id since it is the same
 * with the session which created the parallel retrieve cursor.
 * For the retriever, the session_id is picked by the token when perform the
 * authentication.
 *
 * The caller is responsible for acquiring ParallelCursorEndpointLock lock.
 */
Endpoint
find_endpoint(const char *endpointName, int sessionID)
{
	Endpoint res = NULL;

	Assert(endpointName);

	for (int i = 0; i < MAX_ENDPOINT_SIZE; ++i)
	{
		if (!sharedEndpoints[i].empty &&
			sharedEndpoints[i].sessionID == sessionID &&
			endpoint_name_equals(sharedEndpoints[i].name, endpointName) &&
			sharedEndpoints[i].databaseID == MyDatabaseId)
		{
			res = &sharedEndpoints[i];
			break;
		}
	}

	return res;
}

/*
 * Find the token from the hash table based on given session id and user.
 */
void
get_token_from_session_hashtable(int sessionId, Oid userID, int8 *token /* out */ )
{
	SessionInfoEntry *infoEntry = NULL;
	SessionTokenTag tag;

	tag.sessionID = sessionId;
	tag.userID = userID;

	LWLockAcquire(ParallelCursorEndpointLock, LW_SHARED);

	infoEntry = (SessionInfoEntry *) hash_search(sharedSessionInfoHash, &tag,
												 HASH_FIND, NULL);
	if (infoEntry == NULL)
		ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
				   errmsg("token for user id: %u, session: %d doesn't exist",
						  tag.userID, sessionId)));
	memcpy(token, infoEntry->token, ENDPOINT_TOKEN_HEX_LEN);

	LWLockRelease(ParallelCursorEndpointLock);
}

/*
 * Get the corresponding session id by the given token.
 */
int
get_session_id_from_token(Oid userID, const int8 *token)
{
	int			sessionId = InvalidEndpointSessionId;
	SessionInfoEntry *infoEntry = NULL;
	HASH_SEQ_STATUS status;

	LWLockAcquire(ParallelCursorEndpointLock, LW_SHARED);
	hash_seq_init(&status, sharedSessionInfoHash);
	while ((infoEntry = (SessionInfoEntry *) hash_seq_search(&status)) != NULL)
	{
		if (endpoint_token_hex_equals(infoEntry->token, token) &&
			userID == infoEntry->tag.userID)
		{
			sessionId = infoEntry->tag.sessionID;
			hash_seq_term(&status);
			break;
		}
	}
	LWLockRelease(ParallelCursorEndpointLock);

	return sessionId;
}

/*
 * Generate the endpoint name.
 */
static void
generate_endpoint_name(char *name, const char *cursorName)
{
	int			len, cursorLen;

	len = 0;

	/* part1: cursor name */
	cursorLen = strlen(cursorName);
	if (cursorLen > ENDPOINT_NAME_CURSOR_LEN)
		cursorLen = ENDPOINT_NAME_CURSOR_LEN;
	memcpy(name, cursorName, cursorLen);
	len += cursorLen;

	/* part2: gp_session_id */
	snprintf(name + len, ENDPOINT_NAME_SESSIONID_LEN + 1, "%08x", gp_session_id);
	len += ENDPOINT_NAME_SESSIONID_LEN;

	/*
	 * part3: gp_command_count
	 * In theory cursor name + gp_session_id is enough, but we'd keep this part
	 * to avoid confusion or potential issues for the scenario that in the same
	 * session (thus same gp_session_id), two endpoints with same cursor names
	 * (happens the cursor is dropped/rollbacked and then recreated) and
	 * retrieve the endpoints would be confusing for users that in the same
	 * retrieve connection.
	 */
	snprintf(name + len, ENDPOINT_NAME_COMMANDID_LEN + 1, "%08x", gp_command_count);
	len += ENDPOINT_NAME_COMMANDID_LEN;

	name[len] = '\0';
}

/*
 * Called during xaction abort.
 */
void
AtAbort_EndpointExecState()
{
	EndpointExecState *state = CurrEndpointExecState;

	if (state != NULL)
	{
		abort_endpoint(state);
		pfree(state);

		CurrEndpointExecState = NULL;
	}
}

EndpointExecState *
allocEndpointExecState()
{
	EndpointExecState	*endpointExecState;
	MemoryContext		 oldcontext;

	if (unlikely(CurrEndpointExecState != NULL))
		ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
						errmsg("Previous endpoint estate is not cleaned up.")));

	oldcontext = MemoryContextSwitchTo(TopMemoryContext);

	endpointExecState = palloc0(sizeof(EndpointExecState));
	CurrEndpointExecState = endpointExecState;

	MemoryContextSwitchTo(oldcontext);

	return endpointExecState;
}
