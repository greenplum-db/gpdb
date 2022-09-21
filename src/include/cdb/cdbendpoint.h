/*-------------------------------------------------------------------------
 * cdbendpoint.h
 *	  Functions supporting the Greenplum Endpoint PARALLEL RETRIEVE CURSOR.
 *
 * The PARALLEL RETRIEVE CURSOR is introduced to reduce the heavy burdens of
 * the coordinator node. If possible it will not gather the result, but keep
 * the result on segments. However some query may still need to gather to the
 * coordinator, so the ENDPOINT is introduced to present these node entities
 * that when the PARALLEL RETRIEVE CURSOR executed, the query result will be
 * redirected to, not matter they are the coordinator, some segments or all
 * segments.
 *
 * When the PARALLEL RETRIEVE CURSOR is executed, user can setup a retrieve
 * only connection (in a retrieve connection, the libpq authentication will not
 * depends on pg_hba) to all endpoints for retrieving result data in parallel.
 * The RETRIEVE statement behavior is similar to the "FETCH count" statement,
 * while it only can be executed in retrieve connections.
 *
 * Copyright (c) 2020-Present VMware, Inc. or its affiliates
 *
 *
 * IDENTIFICATION
 *		src/include/cdb/cdbendpoint.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBENDPOINT_H
#define CDBENDPOINT_H

#include "executor/tqueue.h"
#include "storage/shm_toc.h"
#include "nodes/execnodes.h"

/*
 * Endpoint allocate positions.
 */
enum EndPointExecPosition
{
	ENDPOINT_ON_ENTRY_DB,
	ENDPOINT_ON_SINGLE_QE,
	ENDPOINT_ON_SOME_QE,
	ENDPOINT_ON_ALL_QE
};

#define STR_ENDPOINT_STATE_READY		"READY"
#define STR_ENDPOINT_STATE_ATTACHED		"ATTACHED"
#define STR_ENDPOINT_STATE_RETRIEVING	"RETRIEVING"
#define STR_ENDPOINT_STATE_FINISHED		"FINISHED"
#define STR_ENDPOINT_STATE_RELEASED		"RELEASED"

#define MAX_ENDPOINT_SIZE				1024
#define ENDPOINT_TOKEN_ARR_LEN			16
#define ENDPOINT_TOKEN_STR_LEN			(ENDPOINT_TOKEN_ARR_LEN<<1)
#define InvalidEndpointSessionId		(-1)	/* follows invalid gp_session_id */

/* ACK NOTICE MESSAGE FROM ENDPOINT QE/Entry DB to QD */
#define ENDPOINT_READY_ACK_MSG			"ENDPOINT_READY"
#define ENDPOINT_FINISHED_ACK_MSG		"ENDPOINT_FINISHED"

/*
 * Endpoint attach status, used by parallel retrieve cursor.
 */
typedef enum EndpointState
{
	ENDPOINTSTATE_INVALID,   /* The initial state of an endpoint. */
	ENDPOINTSTATE_READY,	 /* After retrieve cursor is declared and endpoint
							  * is allocated. */
	ENDPOINTSTATE_RETRIEVING,/* When a retrieve statement begin to retrieve
							  * tuples from an endpoint, this state may be
							  * transformed from READY or ATTACHED.
							  *
							  * READY->RETRIEVING: if this is the first retrieve statement.
							  * ATTACHED->RETRIEVING: if this is not the first retrieve statement.
							  */
	ENDPOINTSTATE_ATTACHED,  /* After a retrieve statement is executed and not
							  * all tuples are retrieved.
							  *
							  * RETRIEVING-->ATTACHED
							  */
	ENDPOINTSTATE_FINISHED,  /* After a retrieve statement is executed and all
							  * tuples are retrieved.
							  *
							  * RETRIEVING-->ATTACHED
							  */
	ENDPOINTSTATE_RELEASED,  /* Retrieve role exits with error. */
} EndpointState;

/*
 * Endpoint, used by parallel retrieve cursor.
 * Entries are maintained in shared memory.
 */
struct EndpointData
{
	char		name[NAMEDATALEN];		/* Endpoint name */
	char		cursorName[NAMEDATALEN];		/* Parallel cursor name */
	Oid			databaseID;		/* Database OID */
	pid_t		senderPid;		/* The PID of EPR_SENDER(endpoint), set before
								 * endpoint sends data */
	pid_t		receiverPid;	/* The retrieve role's PID that connect to
								 * current endpoint */
	dsm_handle	mqDsmHandle;	/* DSM handle, which contains shared message
								 * queue */
	Latch		ackDone;		/* Latch to sync EPR_SENDER and EPR_RECEIVER
								 * status */
	EndpointState	state;		/* The state of the endpoint */
	int			sessionID;		/* Connection session id */
	Oid			userID;			/* User ID of the current executed PARALLEL
								 * RETRIEVE CURSOR */
	bool		empty;			/* Whether current Endpoint slot in DSM is
								 * free */
};

typedef struct EndpointData Endpoint;

/*
 * The state information for parallel retrieve cursor
 */
typedef struct EndpointExecState
{
	Endpoint			*endpoint;      /* endpoint entry */
	DestReceiver		*dest;
	dsm_segment			*dsmSeg;        /* dsm_segment pointer */
} EndpointExecState;

extern bool am_cursor_retrieve_handler;
extern bool retrieve_conn_authenticated;

/* cbdendpoint.c */

/* Endpoint shared memory context init */
extern Size EndpointShmemSize(void);
extern void EndpointShmemInit(void);
extern Size ParallelCursorCountSize(void);
extern void ParallelCursorCountInit(void);

/*
 * Below functions should run on the QD.
 */
extern enum EndPointExecPosition GetParallelCursorEndpointPosition(PlannedStmt *plan);
extern void WaitEndpointsReady(EState *estate);
extern void AtAbort_EndpointExecState(void);
extern void allocEndpointExecState(void);

/*
 * Below functions should run on Endpoints(QE/Entry DB).
 */
extern void SetupEndpointExecState(TupleDesc tupleDesc, const char *cursorName, CmdType operation, DestReceiver **endpointDest);
extern void DestroyEndpointExecState(void);
extern void EndpointNotifyQD(const char *message);

/* cdbendpointretrieve.c */

/*
 * Below functions should run on the retrieve backend.
 */
extern void InitRetrieveCtl(void);
extern bool AuthEndpoint(Oid userID, const char *tokenStr);
extern TupleDesc GetRetrieveStmtTupleDesc(const RetrieveStmt *stmt);
extern void ExecRetrieveStmt(const RetrieveStmt *stmt, DestReceiver *dest);
extern void check_parallel_retrieve_cursor_errors(EState *estate);

/* Endpoint shared memory utility functions in "cdbendpoint.c" */
extern Endpoint *get_endpointdesc_by_index(int index);
extern void get_token_from_session_hashtable(int sessionId, Oid userID, int8 *token /* out */ );

/* utility functions in "cdbendpointutilities.c" */
extern void endpoint_token_str2arr(const char *tokenStr, int8 *token);
extern void endpoint_token_arr2str(const int8 *token, char *tokenStr);
extern char *state_enum_to_string(EndpointState state);
extern void generate_endpoint_name(char *name, const char *cursorName);

#endif   /* CDBENDPOINT_H */
