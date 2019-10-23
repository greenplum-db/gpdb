/*-------------------------------------------------------------------------
 *
 * cdbdisp.h
 * routines for dispatching commands from the dispatcher process
 * to the qExec processes.
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbdisp.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBDISP_H
#define CDBDISP_H

#include "cdb/cdbtm.h"
#include "utils/resowner.h"

#define CDB_MOTION_LOST_CONTACT_STRING "Interconnect error master lost contact with segment."

struct CdbDispatchResults; /* #include "cdb/cdbdispatchresult.h" */
struct CdbPgResults;
struct Gang; /* #include "cdb/cdbgang.h" */
struct ResourceOwnerData;
enum GangType;

/*
 * Types of message to QE when we wait for it.
 */
typedef enum DispatchWaitMode
{
	DISPATCH_WAIT_NONE = 0,			/* wait until QE fully completes */
	DISPATCH_WAIT_FINISH,			/* send query finish */
	DISPATCH_WAIT_CANCEL			/* send query cancel */
} DispatchWaitMode;

typedef struct CdbDispatcherState
{
	List *allocatedGangs;
	struct CdbDispatchResults *primaryResults;
	void *dispatchParams;
	int	largestGangSize;
	bool forceDestroyGang;
	bool isExtendedQuery;
#ifdef USE_ASSERT_CHECKING
	bool isGangDestroying;
#endif
} CdbDispatcherState;

typedef struct DispatcherInternalFuncs
{
	bool (*checkForCancel)(struct CdbDispatcherState *ds);
	int (*getWaitSocketFd)(struct CdbDispatcherState *ds);
	void* (*makeDispatchParams)(int maxSlices, int largestGangSize, char *queryText, int queryTextLen);
	void (*checkResults)(struct CdbDispatcherState *ds, DispatchWaitMode waitMode);
	void (*dispatchToGang)(struct CdbDispatcherState *ds, struct Gang *gp, int sliceIndex);
	void (*waitDispatchFinish)(struct CdbDispatcherState *ds);

}DispatcherInternalFuncs;

typedef struct dispatcher_handle_t
{
	struct CdbDispatcherState *dispatcherState;

	ResourceOwner owner;	/* owner of this handle */
	struct dispatcher_handle_t *next;
	struct dispatcher_handle_t *prev;
} dispatcher_handle_t;

extern dispatcher_handle_t *open_dispatcher_handles;

/*--------------------------------------------------------------------*/
/*
 * cdbdisp_dispatchToGang:
 * Send the strCommand SQL statement to the subset of all segdbs in the cluster
 * specified by the gang parameter. cancelOnError indicates whether an error
 * occurring on one of the qExec segdbs should cause all still-executing commands to cancel
 * on other qExecs. Normally this would be true. The commands are sent over the libpq
 * connections that were established during cdblink_setup.
 *
 * The caller must provide a CdbDispatchResults object having available
 * resultArray slots sufficient for the number of QEs to be dispatched:
 * i.e., resultCapacity - resultCount >= gp->size. This function will
 * assign one resultArray slot per QE of the Gang, paralleling the Gang's
 * db_descriptors array. Success or failure of each QE will be noted in
 * the QE's CdbDispatchResult entry; but before examining the results, the
 * caller must wait for execution to end by calling CdbCheckDispatchResult().
 *
 * The CdbDispatchResults object owns some malloc'ed storage, so the caller
 * must make certain to free it by calling cdbdisp_destroyDispatcherState().
 *
 * When dispatchResults->cancelOnError is false, strCommand is to be
 * dispatched to every connected gang member if possible, despite any
 * cancellation requests, QE errors, connection failures, etc.
 *
 * NB: This function should return normally even if there is an error.
 * It should not longjmp out via elog(ERROR, ...), ereport(ERROR, ...),
 * PG_THROW, CHECK_FOR_INTERRUPTS, etc.
 */
void
cdbdisp_dispatchToGang(struct CdbDispatcherState *ds,
					   struct Gang *gp,
					   int sliceIndex);

/*
 * cdbdisp_waitDispatchFinish:
 *
 * For asynchronous dispatcher, we have to wait all dispatch to finish before we move on to query execution,
 * otherwise we may get into a deadlock situation, e.g, gather motion node waiting for data,
 * while segments waiting for plan.
 */
void
cdbdisp_waitDispatchFinish(struct CdbDispatcherState *ds);

/*
 * CdbCheckDispatchResult:
 *
 * Waits for completion of threads launched by cdbdisp_dispatchToGang().
 *
 * QEs that were dispatched with 'cancelOnError' true and are not yet idle
 * will be canceled/finished according to waitMode.
 */
void
cdbdisp_checkDispatchResult(struct CdbDispatcherState *ds, DispatchWaitMode waitMode);

/*
 * cdbdisp_getDispatchResults:
 *
 * Block until all QEs return results or report errors.
 *
 * Return Values:
 *   Return NULL If one or more QEs got Error in which case qeErrorMsg contain
 *   QE error messages and qeErrorCode the thrown ERRCODE.
 */
struct CdbDispatchResults *
cdbdisp_getDispatchResults(struct CdbDispatcherState *ds, ErrorData **qeError);

/*
 * CdbDispatchHandleError
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
CdbDispatchHandleError(struct CdbDispatcherState *ds);

void
cdbdisp_cancelDispatch(CdbDispatcherState *ds);

/*
 * Allocate memory and initialize CdbDispatcherState.
 *
 * Call cdbdisp_destroyDispatcherState to free it.
 */
CdbDispatcherState * cdbdisp_makeDispatcherState(bool isExtendedQuery);

/*
 * Free memory in CdbDispatcherState
 *
 * Free the PQExpBufferData allocated in libpq.
 * Free dispatcher memory context.
 */
void cdbdisp_destroyDispatcherState(CdbDispatcherState *ds);

void
cdbdisp_makeDispatchParams(CdbDispatcherState *ds,
						   int maxSlices,
						   char *queryText,
						   int queryTextLen);

bool cdbdisp_checkForCancel(CdbDispatcherState * ds);
int cdbdisp_getWaitSocketFd(CdbDispatcherState *ds);

void cdbdisp_cleanupDispatcherHandle(const struct ResourceOwnerData * owner);

void AtAbort_DispatcherState(void);

void AtSubAbort_DispatcherState(void);

char *
segmentsToContentStr(List *segments);

#endif   /* CDBDISP_H */
