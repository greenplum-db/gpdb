/*-------------------------------------------------------------------------
 *
 * portalcmds.c
 *	  Utility commands affecting portals (that is, SQL cursor commands)
 *
 * Note: see also tcop/pquery.c, which implements portal operations for
 * the FE/BE protocol.	This module uses pquery.c for some operations.
 * And both modules depend on utils/mmgr/portalmem.c, which controls
 * storage management for portals (but doesn't run any queries in them).
 *
 * Portions Copyright (c) 2006-2008, Greenplum inc
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/backend/commands/portalcmds.c,v 1.60 2007/02/06 22:49:24 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <limits.h>

#include "access/xact.h"
#include "commands/portalcmds.h"
#include "executor/executor.h"
#include "executor/tstoreReceiver.h"
#include "optimizer/planner.h"
#include "rewrite/rewriteHandler.h"
#include "tcop/pquery.h"
#include "tcop/tcopprot.h"
#include "utils/memutils.h"
#include "utils/resscheduler.h"

#include "cdb/cdbgang.h"
#include "cdb/cdbgangmgr.h"
#include "cdb/cdbvars.h"
#include "postmaster/backoff.h"

extern char *savedSeqServerHost;
extern int savedSeqServerPort;

static void PortalCleanupHelper(Portal portal, volatile int *cleanupstate);

/*
 * PerformCursorOpen
 *		Execute SQL DECLARE CURSOR command.
 */
void
PerformCursorOpen(PlannedStmt *stmt, ParamListInfo params,
				  const char *queryString, bool isTopLevel)
{
	DeclareCursorStmt *cstmt = (DeclareCursorStmt *) stmt->utilityStmt;
	Portal		portal;
	MemoryContext oldContext;

	if (cstmt == NULL || !IsA(cstmt, DeclareCursorStmt))
		elog(ERROR, "PerformCursorOpen called for non-cursor query");

	/*
	 * Disallow empty-string cursor name (conflicts with protocol-level
	 * unnamed portal).
	 */
	if (!cstmt->portalname || cstmt->portalname[0] == '\0')
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_CURSOR_NAME),
				 errmsg("invalid cursor name: must not be empty")));

	/*
	 * If this is a non-holdable cursor, we require that this statement has
	 * been executed inside a transaction block (or else, it would have no
	 * user-visible effect).
	 */
	if (!(cstmt->options & CURSOR_OPT_HOLD))
		RequireTransactionChain((void *) cstmt, "DECLARE CURSOR");

	/*
	 * Allow using the SCROLL keyword even though we don't support its
	 * functionality (backward scrolling). Silently accept it and instead
	 * of reporting an error like before, override it to NO SCROLL.
	 * 
	 * for information see: MPP-5305 and BIT-93
	 */
	if (cstmt->options & CURSOR_OPT_SCROLL)
	{
		/*ereport(ERROR,
				(errcode(ERRCODE_GP_FEATURE_NOT_YET),
				 errmsg("scrollable cursors are not yet supported in Greenplum Database")));*/

		cstmt->options -= CURSOR_OPT_SCROLL;
	}

	cstmt->options |= CURSOR_OPT_NO_SCROLL;
	
	Assert(!(cstmt->options & CURSOR_OPT_SCROLL && cstmt->options & CURSOR_OPT_NO_SCROLL));

	/*
	 * Create a portal and copy the plan and queryString into its memory.
	 */
	portal = CreatePortal(cstmt->portalname, false, false);

	oldContext = MemoryContextSwitchTo(PortalGetHeapMemory(portal));

	stmt = copyObject(stmt);
	stmt->utilityStmt = NULL;	/* make it look like plain SELECT */

	stmt->qdContext = PortalGetHeapMemory(portal); /* Temporary! See comment in PlannedStmt. */

	queryString = pstrdup(queryString);

	PortalDefineQuery(portal,
					  NULL,
					  queryString,
					  T_DeclareCursorStmt,
					  "SELECT", /* cursor's query is always a SELECT */
					  list_make1(stmt),
					  PortalGetHeapMemory(portal));

	portal->is_extended_query = true; /* cursors run in extended query mode */

	/* 
	 * DeclareCursorStmt is a hybrid utility/select statement. Above, we've nullified
	 * the utilityStmt within PlannedStmt so this appears like plain SELECT. As a consequence,
	 * we lose access to the DeclareCursorStmt. To cope, we simply cover over the 
	 * is_simply_updatable calculation for consumption by CURRENT OF constant folding.
	 */
	portal->is_simply_updatable = cstmt->is_simply_updatable;

	/*
	 * Also copy the outer portal's parameter list into the inner portal's
	 * memory context.	We want to pass down the parameter values in case we
	 * had a command like DECLARE c CURSOR FOR SELECT ... WHERE foo = $1 This
	 * will have been parsed using the outer parameter set and the parameter
	 * value needs to be preserved for use when the cursor is executed.
	 */
	params = copyParamList(params);

	MemoryContextSwitchTo(oldContext);

	portal->cursorOptions = cstmt->options;

	/*
	 * Set up options for portal.
	 *
	 * If the user didn't specify a SCROLL type, allow or disallow scrolling
	 * based on whether it would require any additional runtime overhead to do
	 * so.
	 *
	 * GPDB: we do not allow backward scans at the moment regardless
	 * of any additional runtime overhead. We forced CURSOR_OPT_NO_SCROLL
	 * above. Comment out this logic.
	 */
	/*
	if (!(portal->cursorOptions & (CURSOR_OPT_SCROLL | CURSOR_OPT_NO_SCROLL)))
	{
		if (ExecSupportsBackwardScan(plan))
			portal->cursorOptions |= CURSOR_OPT_SCROLL;
		else
			portal->cursorOptions |= CURSOR_OPT_NO_SCROLL;
	}
	*/

	/*
	 * Start execution, inserting parameters if any.
	 */
	PortalStart(portal, params, ActiveSnapshot,
				savedSeqServerHost, savedSeqServerPort);

	Assert(portal->strategy == PORTAL_ONE_SELECT);

	/*
	 * We're done; the query won't actually be run until PerformPortalFetch is
	 * called.
	 */
}

/*
 * PerformPortalFetch
 *		Execute SQL FETCH or MOVE command.
 *
 *	stmt: parsetree node for command
 *	dest: where to send results
 *	completionTag: points to a buffer of size COMPLETION_TAG_BUFSIZE
 *		in which to store a command completion status string.
 *
 * completionTag may be NULL if caller doesn't want a status string.
 */
void
PerformPortalFetch(FetchStmt *stmt,
				   DestReceiver *dest,
				   char *completionTag)
{
	Portal		portal;
	long		nprocessed;

	/*
	 * Disallow empty-string cursor name (conflicts with protocol-level
	 * unnamed portal).
	 */
	if (!stmt->portalname || stmt->portalname[0] == '\0')
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_CURSOR_NAME),
				 errmsg("invalid cursor name: must not be empty")));

	/* get the portal from the portal name */
	portal = GetPortalByName(stmt->portalname);
	if (!PortalIsValid(portal))
	{
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_CURSOR),
				 errmsg("cursor \"%s\" does not exist", stmt->portalname)));
		return;					/* keep compiler happy */
	}

	/* Adjust dest if needed.  MOVE wants destination DestNone */
	if (stmt->ismove)
		dest = None_Receiver;

	/* Do it */
	nprocessed = PortalRunFetch(portal,
								stmt->direction,
								stmt->howMany,
								dest);

	/* Return command status if wanted */
	if (completionTag)
		snprintf(completionTag, COMPLETION_TAG_BUFSIZE, "%s %ld",
				 stmt->ismove ? "MOVE" : "FETCH",
				 nprocessed);
}

/*
 * PerformPortalClose
 *		Close a cursor.
 */
void
PerformPortalClose(const char *name)
{
	Portal		portal;

	/*
	 * Disallow empty-string cursor name (conflicts with protocol-level
	 * unnamed portal).
	 */
	if (!name || name[0] == '\0')
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_CURSOR_NAME),
				 errmsg("invalid cursor name: must not be empty")));

	/*
	 * get the portal from the portal name
	 */
	portal = GetPortalByName(name);
	if (!PortalIsValid(portal))
	{
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_CURSOR),
				 errmsg("cursor \"%s\" does not exist", name)));
		return;					/* keep compiler happy */
	}

	/*
	 * Note: PortalCleanup is called as a side-effect
	 */
	PortalDrop(portal, false);
}

/*
 * PortalCleanup
 *
 * Clean up a portal when it's dropped.  This is the standard cleanup hook
 * for portals.
 *
 * CDB: If anything goes wrong in here, try to just log it and keep going.
 * Likely we're cleaning up after some earlier error.  Don't confuse the
 * user with additional fallout secondary to the original error.  Caller
 * isn't interested in any further chatter from this portal, so pipe down.
 */
void
PortalCleanup(Portal portal)
{
	ResourceOwner	saveResourceOwner = CurrentResourceOwner;   /* save */
	MemoryContext	saveContext = CurrentMemoryContext;         /* save */
	volatile int	cleanupstate = 0;

	Assert(portal);

	/* We must make the portal's resource owner current */
	if (portal->resowner)
		CurrentResourceOwner = portal->resowner;

	PG_TRY();
	{
		/* My helper does all the work. */
		PortalCleanupHelper(portal, &cleanupstate);
	}
	/* Log and dismiss an error, then loop to do next step of cleanup. */
	PG_CATCH();
	{
		CurrentResourceOwner = saveResourceOwner;           /* restore */
		MemoryContextSwitchTo(saveContext);                 /* restore */

		/*
		 * If PortalCleanupHelper() threw an error -- it means that
		 * ExecutorEnd() threw and error, which means that our gangs
		 * may be sitting on the allocated-list without having been
		 * properly free()ed.
		 *
		 * For cursor-queries with large numbers of slices, this
		 * can "leak" a lot of resources on the segments.
		 */
		if (cleanupstate < 1 && Gp_role == GP_ROLE_DISPATCH)
		{
			cleanupstate = 1;
			GetGangMgr().freeGangsForPortal((char *)portal->name);
			GetGangMgr().cleanupPortalGangs(portal);
		}

		/* Sorry, can't dismiss this error. */
		portal->status = PORTAL_FAILED;

		PG_RE_THROW();
	}
	PG_END_TRY();

	CurrentResourceOwner = saveResourceOwner;                   /* restore */
	MemoryContextSwitchTo(saveContext);                         /* restore */
}                               /* PortalCleanup */

/*
 * PortalCleanupHelper
 *
 * This could be called more than once.  Each step should guard itself
 * so that it will be executed only once even if called more than once.
 * The *cleanupstate arg can be used to remember what has been done.
 */
static void
PortalCleanupHelper(Portal portal, volatile int *cleanupstate)
{
	QueryDesc      *queryDesc = PortalGetQueryDesc(portal);

	/*
	 * Shut down executor, if still running.  We skip this during error abort,
	 * since other mechanisms will take care of releasing executor resources,
	 * and we can't be sure that ExecutorEnd itself wouldn't fail.
	 */
	portal->queryDesc = NULL;
	if (queryDesc)
	{
		if (queryDesc->estate)
		{
			/*
			 * If we still have an estate -- then we need to cancel any
			 * unfinished work.
			 */
			queryDesc->estate->cancelUnfinished = true;

			/* we do not need AfterTriggerEndQuery() here */
			ExecutorEnd(queryDesc);
		}
	}

	/*
	 * Terminate unneeded QE processes.
	 */
	if (*cleanupstate < 1 && Gp_role == GP_ROLE_DISPATCH)
	{
		*cleanupstate = 1;
		GetGangMgr().cleanupPortalGangs(portal);
	}

	/* 
	 * If resource scheduling is enabled, release the resource lock. 
	 */
	if (portal->releaseResLock)
	{
		portal->releaseResLock = false;
        ResUnLockPortal(portal);
	}

	/**
	 * Clean up backend's backoff entry
	 */
	if (gp_enable_resqueue_priority
			&& Gp_role == GP_ROLE_DISPATCH
			&& gp_session_id > -1)
	{
		BackoffBackendEntryExit();
	}

}                               /* PortalCleanupHelper */


/*
 * PersistHoldablePortal
 *
 * Prepare the specified Portal for access outside of the current
 * transaction. When this function returns, all future accesses to the
 * portal must be done via the Tuplestore (not by invoking the
 * executor).
 */
void
PersistHoldablePortal(Portal portal)
{
	QueryDesc  *queryDesc = PortalGetQueryDesc(portal);
	Portal		saveActivePortal;
	Snapshot	saveActiveSnapshot;
	ResourceOwner saveResourceOwner;
	MemoryContext savePortalContext;
	MemoryContext saveQueryContext;
	MemoryContext oldcxt;

	/*
	 * If we're preserving a holdable portal, we had better be inside the
	 * transaction that originally created it.
	 */
	Assert(portal->createSubid != InvalidSubTransactionId);
	Assert(queryDesc != NULL);

	/*
	 * Caller must have created the tuplestore already.
	 */
	Assert(portal->holdContext != NULL);
	Assert(portal->holdStore != NULL);

	/*
	 * Before closing down the executor, we must copy the tupdesc into
	 * long-term memory, since it was created in executor memory.
	 */
	oldcxt = MemoryContextSwitchTo(portal->holdContext);

	portal->tupDesc = CreateTupleDescCopy(portal->tupDesc);

	MemoryContextSwitchTo(oldcxt);

	/*
	 * Check for improper portal use, and mark portal active.
	 */
	if (portal->status != PORTAL_READY)
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("portal \"%s\" cannot be run", portal->name)));

	portal->status = PORTAL_ACTIVE;

	/*
	 * Set up global portal context pointers.
	 */
	saveActivePortal = ActivePortal;
	saveActiveSnapshot = ActiveSnapshot;
	saveResourceOwner = CurrentResourceOwner;
	savePortalContext = PortalContext;
	saveQueryContext = QueryContext;
	PG_TRY();
	{
		ActivePortal = portal;
		ActiveSnapshot = queryDesc->snapshot;
		if (portal->resowner)
			CurrentResourceOwner = portal->resowner;
		PortalContext = PortalGetHeapMemory(portal);
		QueryContext = portal->queryContext;

		MemoryContextSwitchTo(PortalContext);

		/*
		 * Rewind the executor: we need to store the entire result set in the
		 * tuplestore, so that subsequent backward FETCHs can be processed.
		 */
		/*
		 * We don't allow scanning backwards in MPP! skip this call and 
		 * skip the reset position call few lines down.
		 */
		if(Gp_role == GP_ROLE_UTILITY)
			ExecutorRewind(queryDesc);

		/*
		 * Change the destination to output to the tuplestore.  Note we
		 * tell the tuplestore receiver to detoast all data passed through it.
		 */
		queryDesc->dest = CreateDestReceiver(DestTuplestore, portal);
		SetTuplestoreDestReceiverDeToast(queryDesc->dest, true);

		/* Fetch the result set into the tuplestore */
		ExecutorRun(queryDesc, ForwardScanDirection, 0L);

		(*queryDesc->dest->rDestroy) (queryDesc->dest);
		queryDesc->dest = NULL;

		/*
		 * Now shut down the inner executor.
		 */
		portal->queryDesc = NULL;		/* prevent double shutdown */
		/* we do not need AfterTriggerEndQuery() here */
		ExecutorEnd(queryDesc);

		/*
		 * Set the position in the result set: ideally, this could be
		 * implemented by just skipping straight to the tuple # that we need
		 * to be at, but the tuplestore API doesn't support that. So we start
		 * at the beginning of the tuplestore and iterate through it until we
		 * reach where we need to be.  FIXME someday?  (Fortunately, the
		 * typical case is that we're supposed to be at or near the start
		 * of the result set, so this isn't as bad as it sounds.)
		 */
		MemoryContextSwitchTo(portal->holdContext);

		/*
		 * Since we don't allow backward scan in MPP we didn't do the 
		 * ExecutorRewind() call few lines just above. Therefore we 
		 * don't want to reset the position because we are already in
		 * the position we need to be. Allow this only in utility mode.
		 */
		if(Gp_role == GP_ROLE_UTILITY)
		{
			if (portal->atEnd)
			{
				/* we can handle this case even if posOverflow */
				while (tuplestore_advance(portal->holdStore, true))
					/* continue */ ;
			}
			else
			{
				int64		store_pos;
	
				if (portal->posOverflow)	/* oops, cannot trust portalPos */
					ereport(ERROR,
							(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
							 errmsg("could not reposition held cursor")));
	
				tuplestore_rescan(portal->holdStore);
	
				for (store_pos = 0; store_pos < portal->portalPos; store_pos++)
				{
					if (!tuplestore_advance(portal->holdStore, true))
						elog(ERROR, "unexpected end of tuple stream");
				}
			}
		}
	}
	PG_CATCH();
	{
		/* Uncaught error while executing portal: mark it dead */
		portal->status = PORTAL_FAILED;

		/* Restore global vars and propagate error */
		ActivePortal = saveActivePortal;
		ActiveSnapshot = saveActiveSnapshot;
		CurrentResourceOwner = saveResourceOwner;
		PortalContext = savePortalContext;
		QueryContext = saveQueryContext;

		PG_RE_THROW();
	}
	PG_END_TRY();

	MemoryContextSwitchTo(oldcxt);

	/* Mark portal not active */
	portal->status = PORTAL_READY;

	ActivePortal = saveActivePortal;
	ActiveSnapshot = saveActiveSnapshot;
	CurrentResourceOwner = saveResourceOwner;
	PortalContext = savePortalContext;
	QueryContext = saveQueryContext;

	/*
	 * We can now release any subsidiary memory of the portal's heap context;
	 * we'll never use it again.  The executor already dropped its context,
	 * but this will clean up anything that glommed onto the portal's heap via
	 * PortalContext.
	 */
	MemoryContextDeleteChildren(PortalGetHeapMemory(portal));
}
