/*-------------------------------------------------------------------------
 *
 * cdbdisp.h
 * routines for dispatching commands from the dispatcher process
 * to the qExec processes.
 *
 * Copyright (c) 2005-2008, Greenplum inc
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBDISP_H
#define CDBDISP_H

#include "lib/stringinfo.h"         /* StringInfo */

#include "cdb/cdbtm.h"
#include <pthread.h>

#define CDB_MOTION_LOST_CONTACT_STRING "Interconnect error master lost contact with segment."

struct CdbDispatchResult;           /* #include "cdb/cdbdispatchresult.h" */
struct CdbDispatchResults;          /* #include "cdb/cdbdispatchresult.h" */
struct pg_result;                   /* #include "gp-libpq-fe.h" */
struct Gang;                        /* #include "cdb/cdbgang.h" */
struct Node;                        /* #include "nodes/nodes.h" */
struct QueryDesc;                   /* #include "executor/execdesc.h" */
struct SegmentDatabaseDescriptor;   /* #include "cdb/cdbconn.h" */

typedef enum
{
	GP_DISPATCH_COMMAND_TYPE_QUERY,
	GP_DISPATCH_COMMAND_TYPE_DTX_PROTOCOL
} GpDispatchCommandType;

/*
 * Parameter structure for Greenplum Database Queries
 */
typedef struct DispatchCommandQueryParms
{
	/*
	 * The SQL command
	 */
	const char	*strCommand;
	int			strCommandlen;
	char		*serializedQuerytree;
	int			serializedQuerytreelen;
	char		*serializedPlantree;
	int			serializedPlantreelen;
	char		*serializedParams;
	int			serializedParamslen;
	char		*serializedSliceInfo;
	int			serializedSliceInfolen;
	
	/*
	 * serialized DTX context string
	 */
	char		*serializedDtxContextInfo;
	int			serializedDtxContextInfolen;
	
	int			rootIdx;

	/*
	 * the sequence server info.
	 */
	char *seqServerHost;		/* If non-null, sequence server host name. */
	int seqServerHostlen;
	int seqServerPort;			/* If seqServerHost non-null, sequence server port. */
	
	/*
	 * Used by dispatch agent if NOT using sliced execution
	 */
	int			primary_gang_id;

} DispatchCommandQueryParms;

/*
 * Parameter structure for DTX protocol commands
 */
typedef struct DispatchCommandDtxProtocolParms
{
	DtxProtocolCommand			dtxProtocolCommand;
	int							flags;
	char*						dtxProtocolCommandLoggingStr;
	char						gid[TMGIDSIZE];
	DistributedTransactionId	gxid;
	int							primary_gang_id;
	char *argument;
	int argumentLength;
} DispatchCommandDtxProtocolParms;

/*
 * Types of message to QE when we wait for it.
 */
typedef enum DispatchWaitMode
{
	DISPATCH_WAIT_NONE = 0,			/* wait until QE fully completes */
	DISPATCH_WAIT_FINISH,			/* send query finish */
	DISPATCH_WAIT_CANCEL			/* send query cancel */
} DispatchWaitMode;

/*
 * Parameter structure for the DispatchCommand threads
 */
typedef struct DispatchCommandParms
{
	GpDispatchCommandType			mppDispatchCommandType;
	DispatchCommandQueryParms		queryParms;
	DispatchCommandDtxProtocolParms	dtxProtocolParms;
	
	char		*query_text;
	int			query_text_len;
	
	int			localSlice;
	
	/*
	 * options for controlling certain behavior
	 * on the remote backend before query processing. Like
	 * explicitly opening a transaction.
	 */
	int			txnOptions;
	
	/*
	 * store the command here, so that we make sure that every
	 * thread gets the same command-id
	 */
	int			cmdID;
	
	/*
	 * Session auth info
	 */
	Oid			sessUserId;
	Oid			outerUserId;
	Oid			currUserId;
	bool		sessUserId_is_super;
	bool		outerUserId_is_super;

	/*
	 * Depending on this mode, we may send query cancel or query finish
	 * message to QE while we are waiting it to complete.  NONE means
	 * we expect QE to complete without any instruction.
	 */
	volatile DispatchWaitMode waitMode;
	
}	DispatchCommandParms;

typedef struct CdbDispatchDirectDesc
{
	bool directed_dispatch;
	uint16 count;
	uint16 content[1];
} CdbDispatchDirectDesc;

extern CdbDispatchDirectDesc default_dispatch_direct_desc;
#define DEFAULT_DISP_DIRECT (&default_dispatch_direct_desc)

typedef struct CdbDispatcherState
{
	struct CdbDispatchResults    *primaryResults;
	struct DispatchCommandParms  *dispatchCommandParms;
} CdbDispatcherState;

/*--------------------------------------------------------------------*/

/*
 * CdbCheckDispatchResult:
 *
 * Waits for completion of threads launched by cdbdisp_dispatchToGang().
 *
 * QEs that were dispatched with 'cancelOnError' true and are not yet idle
 * will be canceled/finished according to waitMode.
 */
void
CdbCheckDispatchResult(struct CdbDispatcherState *ds, DispatchWaitMode waitMode);

/*--------------------------------------------------------------------*/

struct pg_result **
cdbdisp_returnResults(struct CdbDispatchResults *primaryResults,
						StringInfo errmsgbuf,
						int *numresults);
/*
 * cdbdisp_dispatchRMCommand:
 * Sends a non-cancelable command to all segment dbs, primary
 *
 * Returns a malloc'ed array containing the PGresult objects thus
 * produced; the caller must PQclear() them and free() the array.
 * A NULL entry follows the last used entry in the array.
 *
 * Any error messages - whether or not they are associated with
 * PGresult objects - are appended to a StringInfo buffer provided
 * by the caller.
 */
struct pg_result **             /* returns ptr to array of PGresult ptrs */
cdbdisp_dispatchRMCommand(const char   *strCommand,
						  bool			withSnapshot,
                          StringInfo    errmsgbuf,
                          int			*numresults);


/*--------------------------------------------------------------------*/

/*
 * cdbdisp_dispatchDtxProtocolCommand:
 * Sends a non-cancelable command to all segment dbs, primary
 *
 * Returns a malloc'ed array containing the PGresult objects thus
 * produced; the caller must PQclear() them and free() the array.
 * A NULL entry follows the last used entry in the array.
 *
 * Any error messages - whether or not they are associated with
 * PGresult objects - are appended to a StringInfo buffer provided
 * by the caller.
 */
struct pg_result **             /* returns ptr to array of PGresult ptrs */
cdbdisp_dispatchDtxProtocolCommand(DtxProtocolCommand		dtxProtocolCommand,
								   int						flags,
								   char						*dtxProtocolCommandLoggingStr,
								   char						*gid,
								   DistributedTransactionId	gxid,
								   StringInfo    			errmsgbuf,
								   int						*numresults,
								   bool 					*badGangs,
								   CdbDispatchDirectDesc *direct,
								   char *argument, int argumentLength );


/*--------------------------------------------------------------------*/

/*
 * cdbdisp_dispatchCommand:
 * Send ths strCommand SQL statement to all segdbs in the cluster
 * cancelOnError indicates whether an error
 * occurring on one of the qExec segdbs should cause all still-executing commands to cancel
 * on other qExecs. Normally this would be true.  The commands are sent over the libpq
 * connections that were established during gang creation.	They are run inside of threads.
 * The number of segdbs handled by any one thread is determined by the
 * guc variable gp_connections_per_thread.
 *
 * The needTwoPhase flag is used to express intent on whether the command to
 * be dispatched should be done inside of a global transaction or not.
 *
 * The CdbDispatchResults objects allocated for the command
 * are returned in *pPrimaryResults
 * The caller, after calling CdbCheckDispatchResult(), can
 * examine the CdbDispatchResults objects, can keep them as
 * long as needed, and ultimately must free them with
 * cdbdisp_destroyDispatchResults() prior to deallocation
 * of the memory context from which they were allocated.
 *
 * NB: Callers should use PG_TRY()/PG_CATCH() if needed to make
 * certain that the CdbDispatchResults objects are destroyed by
 * cdbdisp_destroyDispatchResults() in case of error.
 * To wait for completion, check for errors, and clean up, it is
 * suggested that the caller use cdbdisp_finishCommand().
 */
void
cdbdisp_dispatchCommand(const char                 *strCommand,
						char				   	   *serializedQuerytree,
						int							serializedQuerytreelen,
                        bool                        cancelOnError,
                        bool						needTwoPhase,
                        bool						withSnapshot,
						struct CdbDispatcherState *ds); /* OUT */

/* Wait for all QEs to finish, then report any errors from the given
 * CdbDispatchResults objects and free them.  If not all QEs in the
 * associated gang(s) executed the command successfully, throws an
 * error and does not return.  No-op if both CdbDispatchResults ptrs are NULL.
 * This is a convenience function; callers with unusual requirements may
 * instead call CdbCheckDispatchResult(), etc., directly.
 */
void
cdbdisp_finishCommand(struct CdbDispatcherState *ds,
					  void (*handle_results_callback)(struct CdbDispatchResults *primaryResults, void *ctx),
					  void *ctx);

/*
 * cdbdisp_handleError
 *
 * When caller catches an error, the PG_CATCH handler can use this
 * function instead of cdbdisp_finishCommand to wait for all QEs
 * to finish, clean up, and report QE errors if appropriate.
 * This function should be called only from PG_CATCH handlers.
 *
 * This function destroys and frees the given CdbDispatchResults objects.
 * It is a no-op if both CdbDispatchResults ptrs are NULL.
 *
 * On return, the caller is expected to finish its own cleanup and
 * exit via PG_RE_THROW().
 */
void
cdbdisp_handleError(struct CdbDispatcherState *ds);

/*
 * CdbDoCommand:
 * Combination of cdbdisp_dispatchCommand and cdbdisp_finishCommand.
 * Called by general users, this method includes global transaction control.
 * If not all QEs execute the command successfully, throws an error and
 * does not return.
 *
 * needTwoPhase specifies whether to dispatch within a distributed 
 * transaction or not.
 */
void
CdbDoCommand(const char *strCommand, bool cancelOnError, bool needTwoPhase);

/*
 * Special for sending SET commands that change GUC variables, so they go to all
 * gangs, both reader and writer
 */
void
CdbSetGucOnAllGangs(const char *strCommand, bool cancelOnError, bool needTwoPhase);

/*--------------------------------------------------------------------*/
struct SliceTable;

/* Compose and dispatch the MPPEXEC commands corresponding to a plan tree
 * within a complete parallel plan.
 *
 * The CdbDispatchResults objects allocated for the plan are
 * returned in *pPrimaryResults
 * The caller, after calling CdbCheckDispatchResult(), can
 * examine the CdbDispatchResults objects, can keep them as
 * long as needed, and ultimately must free them with
 * cdbdisp_destroyDispatchResults() prior to deallocation
 * of the caller's memory context.
 *
 * NB: Callers should use PG_TRY()/PG_CATCH() if needed to make
 * certain that the CdbDispatchResults objects are destroyed by
 * cdbdisp_destroyDispatchResults() in case of error.
 * To wait for completion, check for errors, and clean up, it is
 * suggested that the caller use cdbdisp_finishCommand().
 */
void
cdbdisp_dispatchPlan(struct QueryDesc              *queryDesc,
                     bool                           planRequiresTxn,
                     bool                           cancelOnError,
					 struct CdbDispatcherState *ds); /* OUT: fields filled in */

/* Dispatch a command - already parsed and in the form of a Node
 * tree - to all primary segdbs.  Does not wait for
 * completion.  Does not start a global transaction.
 *
 *
 * The needTwoPhase flag indicates whether you want the dispatched 
 * statement to participate in a distributed transaction or not.
 *
 * NB: Callers should use PG_TRY()/PG_CATCH() if needed to make
 * certain that the CdbDispatchResults objects are destroyed by
 * cdbdisp_destroyDispatchResults() in case of error.
 * To wait for completion, check for errors, and clean up, it is
 * suggested that the caller use cdbdisp_finishCommand().
 */
void
cdbdisp_dispatchUtilityStatement(struct Node   *stmt,
                                 bool           cancelOnError,
                                 bool			needTwoPhase,
                                 bool			withSnapshot,
								 struct CdbDispatcherState *ds,
								 char* debugCaller __attribute__((unused)) );

/* Dispatch a command - already parsed and in the form of a Node
 * tree - to all primary segdbs, and wait for completion.
 * Starts a global transaction first, if not already started.
 * If not all QEs in the given gang(s) executed the command successfully,
 * throws an error and does not return.
 */
void
CdbDispatchUtilityStatement(struct Node *stmt, char* debugCaller __attribute__((unused)) );

void
CdbDispatchUtilityStatement_NoTwoPhase(struct Node *stmt, char* debugCaller __attribute__((unused)) );

/* used to take the current Transaction Snapshot and serialized a version of it
 * into the static variable serializedDtxContextInfo */
char *
qdSerializeDtxContextInfo(int * size, bool wantSnapshot, bool inCursor, int txnOptions, char *debugCaller);


struct EState;
struct PlannedStmt;
struct PlannerInfo;

/* used in the interconnect on the dispatcher to avoid error-cleanup deadlocks. */
bool
cdbdisp_check_estate_for_cancel(struct EState *estate);

/* 
 * make a plan constant, if possible. Call must say if we're doing single row
 * inserts.
 */
extern Node *exec_make_plan_constant(struct PlannedStmt *stmt, bool is_SRI);
extern Node *planner_make_plan_constant(struct PlannerInfo *root, Node *n, bool is_SRI);

/*--------------------------------------------------------------------*/

#endif   /* CDBDISP_H */
