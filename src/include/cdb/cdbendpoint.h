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

/*
 * Endpoint attach status, used by parallel retrieve cursor.
 */
typedef enum EndpointState
{
	ENDPOINTSTATE_INVALID,
	ENDPOINTSTATE_READY,
	ENDPOINTSTATE_RETRIEVING,
	ENDPOINTSTATE_ATTACHED,
	ENDPOINTSTATE_FINISHED,
	ENDPOINTSTATE_RELEASED,
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
	dsm_handle	sessionDsmHandle;	/* DSM handle, which contains per-session
									 * DSM (see session.c). */
};

typedef struct EndpointData *Endpoint;

/*
 * The state information for parallel retrieve cursor
 */
typedef struct EndpointExecState
{
	Endpoint			 endpoint;      /* endpoint entry */
	DestReceiver		*dest;
	dsm_segment			*dsmSeg;        /* dsm_segment pointer */
} EndpointExecState;

extern bool am_cursor_retrieve_handler;
extern bool retrieve_conn_authenticated;

/* cbdendpoint.c */

/* Endpoint shared memory context init */
extern Size EndpointShmemSize(void);
extern void EndpointShmemInit(void);

/*
 * Below functions should run on dispatcher.
 */
extern enum EndPointExecPosition GetParallelCursorEndpointPosition(PlannedStmt *plan);
extern void WaitEndpointsReady(EState *estate);
extern void AtAbort_EndpointExecState(void);
extern EndpointExecState *allocEndpointExecState(void);

/*
 * Below functions should run on Endpoints(QE/Entry DB).
 */
extern void SetupEndpointExecState(TupleDesc tupleDesc,
		const char *cursorName, EndpointExecState *state);
extern void DestroyEndpointExecState(EndpointExecState *state);

/* cdbendpointretrieve.c */

/*
 * Below functions should run on the retrieve backend.
 */
extern bool AuthEndpoint(Oid userID, const char *tokenStr);
extern TupleDesc GetRetrieveStmtTupleDesc(const RetrieveStmt *stmt);
extern void ExecRetrieveStmt(const RetrieveStmt *stmt, DestReceiver *dest);

#endif   /* CDBENDPOINT_H */
