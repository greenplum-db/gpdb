/*-------------------------------------------------------------------------
 * cdbendpointretrieve.c
 *
 * This file includes code of the RETRIEVE operation for the PARALLEL RETRIEVE
 * CURSOR, Once a PARALLEL RETRIEVE CURSOR is declared, the QE backends start
 * to send query results to so-called endpoints (see cdbendpoint.c). The
 * results can be retrieved through dedicated retrieve sessions in shared
 * memory via the shared-memory base message queue mechanism. A retrieve
 * session is a special session that can executes the RETRIEVE statement only.
 *
 * To start a retrieve session, the endpoint's token is needed as the password
 * for authentication. The token could be obtained via some endpoint related
 * UDFs. The user should be the same as the one who declares the parallel
 * retrieve cursor. The guc gp_retrieve_conn=true needs to be set to start the
 * retrieve session. The guc "gp_retrieve_conn=true" imples utility mode
 * connection.
 *
 * Once a retrieve session has attached to an endpoint, no other retrieve
 * session can attach to that endpoint.  It is possible to retrieve multiple
 * endpoints from the same retrieve session if they share the same token. In
 * other words, one retrieve session can attach and retrieve from multiple
 * endpoints.
 *
 * Copyright (c) 2020-Present VMware, Inc. or its affiliates
 *
 * IDENTIFICATION
 *		src/backend/cdb/cdbendpointretrieve.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/session.h"
#include "access/xact.h"
#include "common/hashfn.h"
#include "storage/ipc.h"
#include "utils/backend_cancel.h"
#include "utils/dynahash.h"
#include "utils/elog.h"
#include "utils/faultinjector.h"
#include "utils/guc.h"
#include "utils/typcache.h"
#include "cdbendpoint_private.h"
#include "cdb/cdbendpoint.h"
#include "cdb/cdbsrlz.h"

/* Is this the utility-mode backend for RETRIEVE? */
bool		am_cursor_retrieve_handler = false;

/*
 * Was the utility-mode connection retrieve connection authenticated? Just used
 * for sanity-checking.
 */
bool		retrieve_conn_authenticated = false;

/* Retrieve role state. */
enum RetrieveState
{
	RETRIEVE_STATE_INIT,
	RETRIEVE_STATE_ATTACHED,
	RETRIEVE_STATE_RECEIVING,
	RETRIEVE_STATE_FINISHED,
};

/*
 * For receiver, we have a hash table to store connected endpoint's shared
 * message queue. So that we can retrieve from different endpoints in the same
 * retriever and switch between different endpoints.
 *
 * For endpoint, only keep one entry to track current message queue.
 */
typedef struct RetrieveExecEntry
{
	/* The name of endpoint to be retrieved, also behave as hash key */
	char		endpointName[NAMEDATALEN];
	/* The endpoint to be retrieved */
	Endpoint	*endpoint;
	/* The dsm handle which contains shared memory message queue */
	dsm_segment *mqSeg;
	/* Shared memory message queue */
	shm_mq_handle *mqHandle;
	/* tuple slot used for retrieve data */
	TupleTableSlot *retrieveTs;
	/* TupleQueueReader to read tuple from message queue */
	TupleQueueReader *tqReader;
	/* Track retrieve state */
	enum RetrieveState retrieveState;
}			RetrieveExecEntry;

/*
 * Local structure to the current retrieve operation.
 */
typedef struct RetrieveControl
{
	/*
	 * Track current retrieve entry in executor. Multiple entries are allowed
	 * to be in one retrieve session but only one entry is active.
	 */
	RetrieveExecEntry *current_entry;

	/*
	 * Hash table to cache tuple descriptors for all endpoint_names which have
	 * been retrieved in this retrieve session.
	 */
	HTAB	   *RetrieveExecEntryHTB;

	/*
	 * The endpoint sessionID of the current retrieve entry. Assigned after
	 * authentication.
	 */
	int			sessionID;
}			RetrieveControl;

static RetrieveControl RetrieveCtl =
{
	NULL, NULL, InvalidEndpointSessionId
};

static void init_retrieve_exec_entry(RetrieveExecEntry * entry);
static Endpoint *get_endpoint_from_retrieve_exec_entry(RetrieveExecEntry *entry, bool noError);
static void start_retrieve(const char *endpointName);
static void validate_retrieve_endpoint(Endpoint *endpointDesc, const char *endpointName);
static void finish_retrieve(bool resetPID);
static void attach_receiver_mq(dsm_handle dsmHandle);
static void detach_receiver_mq(RetrieveExecEntry *entry);
static void notify_sender(bool finished);
static void retrieve_cancel_action(RetrieveExecEntry *entry, char *msg);
static void retrieve_exit_callback(int code, Datum arg);
static void retrieve_xact_callback(XactEvent ev, void *arg);
static void retrieve_subxact_callback(SubXactEvent event,
									  SubTransactionId mySubid,
									  SubTransactionId parentSubid,
									  void *arg);
static TupleTableSlot *retrieve_next_tuple(void);
static void retrieve_conn_detach(dsm_segment *segment, Datum datum);

/*
 * AuthEndpoint - Authenticate for retrieve connection.
 *
 * Return true if authentication passes.
 */
bool
AuthEndpoint(Oid userID, const char *tokenStr)
{
	bool		found = false;
	int8		token[ENDPOINT_TOKEN_ARR_LEN] = {0};

	endpoint_token_str2arr(tokenStr, token);

	RetrieveCtl.sessionID = get_session_id_from_token(userID, token);
	if (RetrieveCtl.sessionID != InvalidEndpointSessionId)
	{
		found = true;
		before_shmem_exit(retrieve_exit_callback, (Datum) 0);
		RegisterSubXactCallback(retrieve_subxact_callback, NULL);
		RegisterXactCallback(retrieve_xact_callback, NULL);
	}

	return found;
}

/*
 * GetRetrieveStmtTupleDesc - Gets TupleDesc for the given retrieve statement.
 *
 * This function calls start_retrieve() to initialize related data structure
 * and returns the tuple descriptor.
 */
TupleDesc
GetRetrieveStmtTupleDesc(const RetrieveStmt * stmt)
{
	start_retrieve(stmt->endpoint_name);

	return RetrieveCtl.current_entry->retrieveTs->tts_tupleDescriptor;
}

/*
 * ExecRetrieveStmt - Execute the given RetrieveStmt.
 *
 * This function tries to use the endpoint name in the RetrieveStmt to find the
 * attached endpoint in this retrieve session. If the endpoint can be found,
 * then read from the message queue to feed the active portal's tuplestore. And
 * mark the endpoint as detached before returning.
 */
void
ExecRetrieveStmt(const RetrieveStmt *stmt, DestReceiver *dest)
{
	TupleTableSlot *result = NULL;
	int64		retrieveCount = 0;

	if (RetrieveCtl.current_entry == NULL)
		ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
						errmsg("endpoint %s is not attached",
							   stmt->endpoint_name)));

	retrieveCount = stmt->count;
	if (retrieveCount <= 0 && !stmt->is_all)
		ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						errmsg("RETRIEVE statement only supports forward scan, "
							   "count should not be: %ld",
							   retrieveCount)));

	Assert(dest->mydest == DestTuplestore);
	Assert(RetrieveCtl.current_entry->retrieveState > RETRIEVE_STATE_INIT);

	if (RetrieveCtl.current_entry->retrieveState < RETRIEVE_STATE_FINISHED)
	{
		while (stmt->is_all || retrieveCount > 0)
		{
			result = retrieve_next_tuple();
			if (!result)
				break;

			(*dest->receiveSlot) (result, dest);
			if (!stmt->is_all)
				retrieveCount--;
		}
	}
	else
	{
		/* All tuples have already been retrieved. Nothing to do */
	}

	finish_retrieve(false);
}

/*
 * init_retrieve_exec_entry - Initialize RetrieveExecEntry.
 */
static void
init_retrieve_exec_entry(RetrieveExecEntry * entry)
{
	entry->mqSeg = NULL;
	entry->endpoint = NULL;
	entry->mqHandle = NULL;
	entry->retrieveTs = NULL;
	entry->retrieveState = RETRIEVE_STATE_INIT;
}

/*
 * get_endpoint_from_retrieve_exec_entry
 *
 * Get endpoint from the entry if exists and validate the endpoint slot
 * still belong to current entry since it may get reused by other endpoints.
 *
 * if there is something wrong during validation, warn or error out, depending
 * on the parameter noError.
 */
static Endpoint*
get_endpoint_from_retrieve_exec_entry(RetrieveExecEntry * entry, bool noError)
{
	Assert(LWLockHeldByMe(ParallelCursorEndpointLock));

	/* Sanity check and error out if needed. */
	if (entry->endpoint != NULL)
	{
		if (entry->endpoint->empty)
		{
			ereport(noError ? WARNING : ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("endpoint is not available because the parallel "
							"retrieve cursor was aborted")));
			entry->endpoint = NULL;
		}
		else if (!(endpoint_name_equals(entry->endpoint->name, entry->endpointName) &&
				   RetrieveCtl.sessionID == entry->endpoint->sessionID))
		{
			ereport(noError ? WARNING : ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("endpoint slot in RetrieveExecEntry is reused by others"),
					 errdetail("endpoint name (%s vs %s), session ID (%d vs %d)",
							   entry->endpoint->name, entry->endpointName,
							   RetrieveCtl.sessionID, entry->endpoint->sessionID)));
			entry->endpoint = NULL;
		}
	}

	return entry->endpoint;
}

/*
 * Initialize a hashtable, its key is the endpoint's name, its value is
 * RetrieveExecEntry
*/
void InitRetrieveCtl(void)
{
		HASHCTL		ctl;

		if (RetrieveCtl.RetrieveExecEntryHTB)
			return;

		MemSet(&ctl, 0, sizeof(ctl));
		ctl.keysize = NAMEDATALEN;
		ctl.entrysize = sizeof(RetrieveExecEntry);
		ctl.hash = string_hash;
		RetrieveCtl.RetrieveExecEntryHTB = hash_create("retrieve hash", MAX_ENDPOINT_SIZE, &ctl,
													   (HASH_ELEM | HASH_FUNCTION));
		RetrieveCtl.current_entry = NULL;
}

/*
 * start_retrieve - start to retrieve an endpoint.
 *
 * Initialize current retrieve RetrieveExecEntry for the given
 * endpoint if it's the first time to retrieve the endpoint.
 * Attach to the endpoint's shm_mq.
 *
 * Set the endpoint status to ENDPOINTSTATE_RETRIEVING.
 *
 * When call RETRIEVE statement in PQprepare() & PQexecPrepared(), this func will
 * be called 2 times.
 */
static void
start_retrieve(const char *endpointName)
{
	HTAB	   *entryHTB = RetrieveCtl.RetrieveExecEntryHTB;
	RetrieveExecEntry *entry = NULL;
	bool		found = false;
	Endpoint	*endpoint;
	dsm_handle	handle = DSM_HANDLE_INVALID;


	entry = hash_search(entryHTB, endpointName, HASH_FIND, &found);

	LWLockAcquire(ParallelCursorEndpointLock, LW_EXCLUSIVE);

	if (found)
		endpoint = get_endpoint_from_retrieve_exec_entry(entry, false);
	else
	{
		endpoint = find_endpoint(endpointName, RetrieveCtl.sessionID);
		if (!endpoint)
			ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							errmsg("the endpoint %s does not exist for session id %d",
								   endpointName, RetrieveCtl.sessionID)));
		validate_retrieve_endpoint(endpoint, endpointName);
		endpoint->receiverPid = MyProcPid;
		handle = endpoint->mqDsmHandle;

		/* insert it into hashtable */
		entry = hash_search(entryHTB, endpointName, HASH_ENTER, NULL);
		init_retrieve_exec_entry(entry);
	}

	/* begins to retrieve tuples from endpoint if still have data to retrieve. */
	if (endpoint->state == ENDPOINTSTATE_READY ||
		endpoint->state == ENDPOINTSTATE_ATTACHED)
	{
		endpoint->state = ENDPOINTSTATE_RETRIEVING;
	}

	LWLockRelease(ParallelCursorEndpointLock);

	entry->endpoint = endpoint;

	RetrieveCtl.current_entry = entry;

	if (!found)
		attach_receiver_mq(handle);

	if (CurrentSession->segment != NULL &&
		dsm_segment_handle(CurrentSession->segment) != endpoint->sessionDsmHandle)
	{
		DetachSession();
	}

	if (CurrentSession->segment == NULL)
	{
		AttachSession(endpoint->sessionDsmHandle);
		on_dsm_detach(CurrentSession->segment, &retrieve_conn_detach, (Datum) 0);
	}
}

/*
 * validate_retrieve_endpoint - after finding the retrieve endpoint,
 * validate whether it meets the requirement.
 */
static void
validate_retrieve_endpoint(Endpoint *endpoint, const char *endpointName)
{
	Assert(endpoint->mqDsmHandle != DSM_HANDLE_INVALID);

	if (endpoint->userID != GetSessionUserId())
	{
		ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
						errmsg("the PARALLEL RETRIEVE CURSOR was created by "
							   "a different user"),
						errhint("Use the same user as the PARALLEL "
								"RETRIEVE CURSOR creator to retrieve.")));
	}

	switch (endpoint->state)
	{
		case ENDPOINTSTATE_FINISHED:
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("another session (pid: %d) used the endpoint and "
							"completed retrieving",
							endpoint->receiverPid)));
			break;				/* make compiler happy */
		case ENDPOINTSTATE_READY:
		case ENDPOINTSTATE_ATTACHED:
			break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("endpoint %s (state: %s) was used by another retrieve session (pid: %d)",
							endpointName,
							state_enum_to_string(endpoint->state),
							endpoint->receiverPid),
					 errdetail("If pid is -1, that session has been detached.")));
	}

	if (endpoint->receiverPid != InvalidPid && endpoint->receiverPid != MyProcPid)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("endpoint %s was already attached by receiver(pid: %d)",
						endpointName, endpoint->receiverPid),
				 errdetail("An endpoint can only be attached by one retrieving session.")));
	}
}

/*
 * Attach to the endpoint's shared memory message queue.
 */
static void
attach_receiver_mq(dsm_handle dsmHandle)
{
	TupleDesc	td;
	TupleDescNode *tupdescnode;
	MemoryContext oldcontext;
	shm_toc    *toc;
	void	   *lookup_space;
	int			td_len;
	RetrieveExecEntry *entry = RetrieveCtl.current_entry;

	Assert(!entry->mqSeg);
	Assert(!entry->mqHandle);
	Assert(entry->retrieveState == RETRIEVE_STATE_INIT);

	/*
	 * Store the result slot all the retrieve mode QE life cycle, we only have
	 * one chance to build it.
	 */
	oldcontext = MemoryContextSwitchTo(TopMemoryContext);

	elogif(gp_log_endpoints, LOG, "CDB_ENDPOINT: init message queue conn for receiver");

	entry->mqSeg = dsm_attach(dsmHandle);
	if (entry->mqSeg == NULL)
		ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
						errmsg("attach to endpoint shared message queue failed")));

	dsm_pin_mapping(entry->mqSeg);
	toc = shm_toc_attach(ENDPOINT_MSG_QUEUE_MAGIC, dsm_segment_address(entry->mqSeg));
	if (toc == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("invalid magic number in dynamic shared memory segment")));

	/*
	 * Find the shared mq for tuple receiving from 'toc' and set up the
	 * connection.
	 */
	shm_mq	   *mq = shm_toc_lookup(toc, ENDPOINT_KEY_TUPLE_QUEUE, false);

	shm_mq_set_receiver(mq, MyProc);
	entry->mqHandle = shm_mq_attach(mq, entry->mqSeg, NULL);

	/*
	 * Find the tuple descritpr information from 'toc' and set the tuple
	 * descriptor.
	 */
	lookup_space = shm_toc_lookup(toc, ENDPOINT_KEY_TUPLE_DESC_LEN, false);
	td_len = *(int *) lookup_space;
	lookup_space = shm_toc_lookup(toc, ENDPOINT_KEY_TUPLE_DESC, false);
	tupdescnode = (TupleDescNode *) deserializeNode(lookup_space, td_len);
	td = tupdescnode->tuple;
	if (entry->retrieveTs != NULL)
		ExecClearTuple(entry->retrieveTs);
	else
		entry->retrieveTs = MakeTupleTableSlot(td, &TTSOpsHeapTuple);

	/* Create the tuple queue reader. */
	entry->tqReader = CreateTupleQueueReader(entry->mqHandle);
	entry->retrieveState = RETRIEVE_STATE_ATTACHED;

	MemoryContextSwitchTo(oldcontext);
}

/*
 * Detach from the endpoint's message queue.
 */
static void
detach_receiver_mq(RetrieveExecEntry * entry)
{
	shm_mq_detach(entry->mqHandle);
	entry->mqHandle = NULL;

	dsm_detach(entry->mqSeg);
	entry->mqSeg = NULL;
}

/*
 * Notify the sender to stop waiting on the ackDone latch.
 *
 * If current endpoint get freed, it means the endpoint aborted.
 */
static void
notify_sender(bool finished)
{
	Endpoint	*endpoint;

	LWLockAcquire(ParallelCursorEndpointLock, LW_SHARED);
	endpoint = get_endpoint_from_retrieve_exec_entry(RetrieveCtl.current_entry, false);
	if (finished)
		endpoint->state = ENDPOINTSTATE_FINISHED;
	SetLatch(&endpoint->ackDone);
	LWLockRelease(ParallelCursorEndpointLock);
}

/*
 * Read a tuple from shared memory message queue.
 *
 * When reading all tuples, should tell sender that retrieve is done.
 */
static TupleTableSlot *
retrieve_next_tuple()
{
	TupleTableSlot *result = NULL;
	HeapTuple	tup = NULL;
	bool		readerdone = false;
	RetrieveExecEntry *entry = RetrieveCtl.current_entry;

	CHECK_FOR_INTERRUPTS();

	/* at the first time to retrieve data */
	if (entry->retrieveState == RETRIEVE_STATE_ATTACHED)
	{
		/*
		 * try to receive data with nowait, so that empty result will not hang
		 * here
		 */
		tup = TupleQueueReaderNext(entry->tqReader, true, &readerdone);

		entry->retrieveState = RETRIEVE_STATE_RECEIVING;

		/*
		 * at the first time to retrieve data, tell sender not to wait at
		 * wait_receiver()
		 */
		elogif(gp_log_endpoints, LOG, "CDB_ENDPOINT: receiver notifies sender in "
			   "retrieve_next_tuple() when retrieving data for the first time");
		notify_sender(false);
	}

	SIMPLE_FAULT_INJECTOR("fetch_tuples_from_endpoint");

	/*
	 * re-retrieve data in wait mode if not the first time retrieve data or if
	 * the first time retrieve an invalid data, but not finish
	 */
	if (readerdone == false && tup == NULL)
		tup = TupleQueueReaderNext(entry->tqReader, false, &readerdone);

	/* readerdone returns true only after sender detached message queue */
	if (readerdone)
	{
		Assert(!tup);
		DestroyTupleQueueReader(entry->tqReader);
		entry->tqReader = NULL;

		/*
		 * dsm_detach will send SIGUSR1 to sender which may interrupt the
		 * procLatch. But sender will wait on procLatch after finishing
		 * sending. So dsm_detach has to be called earlier to ensure the
		 * SIGUSR1 is coming from the CLOSE CURSOR.
		 */
		detach_receiver_mq(entry);

		/*
		 * We do not call DetachSession() here since we still need that for
		 * the transient record tuples.
		 */
		entry->retrieveState = RETRIEVE_STATE_FINISHED;
		notify_sender(true);
		return NULL;
	}

	if (HeapTupleIsValid(tup))
	{
		ExecClearTuple(entry->retrieveTs);
		result = entry->retrieveTs;
		ExecStoreHeapTuple(tup, /* tuple to store */
						   result,	/* slot in which to store the tuple */
						   false);	/* slot should not pfree tuple */
	}
	return result;
}

/*
 * finish_retrieve - Finish a retrieve statement.
 *
 * If current retrieve statement retrieve all tuples from endpoint. Set
 * endpoint state to ENDPOINTSTATE_FINISHED.  Otherwise, set endpoint's status
 * from ENDPOINTSTATE_RETRIEVING to ENDPOINTSTATE_ATTACHED.
 *
 * Note: don't drop the result slot, we only have one chance to built it.
 * Errors in these function is not expected to be raised.
 */
static void
finish_retrieve(bool resetPID)
{
	Endpoint	*endpoint = NULL;
	RetrieveExecEntry *entry = RetrieveCtl.current_entry;

	Assert(entry);

	LWLockAcquire(ParallelCursorEndpointLock, LW_EXCLUSIVE);
	endpoint = get_endpoint_from_retrieve_exec_entry(entry, true);
	if (endpoint == NULL)
	{
		/*
		 * The endpoint has already cleaned up the Endpoint entry, or during
		 * the retrieve abort stage, sender cleaned the Endpoint entry. And
		 * another endpoint gets allocated just after the cleanup, which will
		 * occupy current endpoint entry.
		 * remove the entry from RetrieveCtl.RetrieveExecEntryHTB also.
		 * to avoid next statement in start_retrieve can get entry from RetrieveCtl.RetrieveExecEntryHTB,
		 * however, can not get endpoint through get_endpoint_from_retrieve_exec_entry.
		 */
		LWLockRelease(ParallelCursorEndpointLock);
		elogif(gp_log_endpoints, LOG, "the Endpoint entry %s has already been cleaned, \
			   remove from RetrieveCtl.RetrieveExecEntryHTB hash table", entry->endpointName);
		hash_search(RetrieveCtl.RetrieveExecEntryHTB, entry->endpointName, HASH_REMOVE, NULL);
		RetrieveCtl.current_entry = NULL;
		return;
	}

	/*
	 * If the receiver pid get retrieve_cancel_action, the receiver pid is
	 * invalid.
	 */
	if (endpoint->receiverPid != MyProcPid && endpoint->receiverPid != InvalidPid)
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("unmatched pid, expected %d but it's %d", MyProcPid,
						endpoint->receiverPid)));

	if (resetPID)
		endpoint->receiverPid = InvalidPid;

	/* Don't set if ENDPOINTSTATE_FINISHED */
	if (endpoint->state == ENDPOINTSTATE_RETRIEVING)
	{
		/*
		 * If finish retrieving, set the endpoint to FINISHED, otherwise set
		 * the endpoint to ATTACHED.
		 */
		if (entry->retrieveState == RETRIEVE_STATE_FINISHED)
			endpoint->state = ENDPOINTSTATE_FINISHED;
		else
			endpoint->state = ENDPOINTSTATE_ATTACHED;
	}

	LWLockRelease(ParallelCursorEndpointLock);
	RetrieveCtl.current_entry = NULL;
}

/*
 * When retrieve role exit with error, let endpoint/sender know exception
 * happened.
 */
static void
retrieve_cancel_action(RetrieveExecEntry * entry, char *msg)
{
	Endpoint	*endpoint;

	Assert(entry);

	LWLockAcquire(ParallelCursorEndpointLock, LW_EXCLUSIVE);

	endpoint = get_endpoint_from_retrieve_exec_entry(entry, true);

	if (endpoint != NULL &&
		endpoint->receiverPid == MyProcPid &&
		endpoint->state != ENDPOINTSTATE_FINISHED)
	{
		endpoint->receiverPid = InvalidPid;
		endpoint->state = ENDPOINTSTATE_RELEASED;
		if (endpoint->senderPid != InvalidPid)
		{
			elogif(gp_log_endpoints, LOG, "CDB_ENDPOINT: signal sender to abort");
			SetBackendCancelMessage(endpoint->senderPid, msg);
			kill(endpoint->senderPid, SIGINT);
		}
	}

	LWLockRelease(ParallelCursorEndpointLock);
}

/*
 * Callback when retrieve role on proc exit, before shmem exit.
 *
 * For Process Exists:
 * If a retrieve session has been retrieved from more than one endpoint, all of
 * the endpoints and their message queues in this session have to be detached when
 * process exits. In this case, the active RetrieveExecEntry will be detached
 * first in retrieve_exit_callback. Thus, no need to detach it again in
 * retrieve_xact_callback.
 *
 * shmem_exit()
 * --> ... (other before shmem callback if exists)
 * --> retrieve_exit_callback
 *	   --> cancel sender if needed.
 *	   --> detach all message queue dsm
 * --> ShutdownPostgres (the last before shmem callback)
 *	   --> AbortOutOfAnyTransaction
 *		   --> AbortTransaction
 *			   --> CallXactCallbacks
 *				   --> retrieve_xact_callback
 *		   --> CleanupTransaction
 * --> dsm_backend_shutdown
 *
 * For Normal Transaction Aborts:
 * Retriever clean up job will be done in xact abort callback
 * retrieve_xact_callback which will only clean the active
 * RetrieveExecEntry.
 *
 * Question:
 * Is it better to detach the dsm we created/attached before dsm_backend_shutdown?
 * Or we can let dsm_backend_shutdown do the detach for us, so we don't need
 * register call back in before_shmem_exit.
 */
static void
retrieve_exit_callback(int code, Datum arg)
{
	HASH_SEQ_STATUS status;
	RetrieveExecEntry *entry;
	HTAB	   *entryHTB = RetrieveCtl.RetrieveExecEntryHTB;

	elogif(gp_log_endpoints, LOG, "CDB_ENDPOINT: retrieve exit callback");

	/* Nothing to do if the hashtable is not ready. */
	if (entryHTB == NULL)
		return;

	/* If the current retrieve statement has not fnished in this run. */
	if (RetrieveCtl.current_entry)
		finish_retrieve(true);

	/* Cancel all partially retrieved endpoints in this session. */
	hash_seq_init(&status, entryHTB);
	while ((entry = (RetrieveExecEntry *) hash_seq_search(&status)) != NULL)
	{
		if (entry->retrieveState != RETRIEVE_STATE_FINISHED)
			retrieve_cancel_action(entry, "Endpoint retrieve session is "
								   "quitting. All unfinished parallel retrieve "
								   "cursors on the session will be terminated.");
		if (entry->mqSeg)
			detach_receiver_mq(entry);
	}
	entryHTB = NULL;

	if (CurrentSession->segment != NULL)
		DetachSession();
}

/*
 * Retrieve role xact abort callback.
 *
 * If normal abort, finish_retrieve() and retrieve_cancel_action() will be
 * called once in current function for current endpoint_name. but if it's proc
 * exit, these two methods will be called twice for current endpoint_name,
 * since we call these two methods before dsm detach.
 */
static void
retrieve_xact_callback(XactEvent ev, void *arg pg_attribute_unused())
{
	if (ev == XACT_EVENT_ABORT)
	{
		elogif(gp_log_endpoints, LOG, "CDB_ENDPOINT: retrieve xact abort callback");
		if (RetrieveCtl.sessionID != InvalidEndpointSessionId &&
			RetrieveCtl.current_entry)
		{
			if (RetrieveCtl.current_entry->retrieveState != RETRIEVE_STATE_FINISHED)
				retrieve_cancel_action(RetrieveCtl.current_entry,
									   "Endpoint retrieve statement aborted");
			finish_retrieve(true);

		}

		if (CurrentSession != NULL && CurrentSession->segment != NULL)
			DetachSession();
	}
}

/*
 * Callback for retrieve role's sub-xact abort .
 */
static void
retrieve_subxact_callback(SubXactEvent event,
						  SubTransactionId mySubid pg_attribute_unused(),
						  SubTransactionId parentSubid pg_attribute_unused(),
						  void *arg pg_attribute_unused())
{
	if (event == SUBXACT_EVENT_ABORT_SUB)
		retrieve_xact_callback(XACT_EVENT_ABORT, NULL);
}

/*
 * Reset record cache when detach session.
 */
static void
retrieve_conn_detach(dsm_segment *segment, Datum datum)
{
	reset_record_cache();
}
