/*-------------------------------------------------------------------------
 *
 * discard.c
 *	  The implementation of the DISCARD command
 *
 * Copyright (c) 1996-2016, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/discard.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/xact.h"
#include "catalog/namespace.h"
#include "commands/async.h"
#include "commands/discard.h"
#include "commands/prepare.h"
#include "commands/sequence.h"
#include "utils/guc.h"
#include "utils/portal.h"

#include "cdb/cdbdisp_query.h"
#include "cdb/cdbgang.h"
#include "cdb/cdbvars.h"

static void DiscardAll(bool isTopLevel);

/*
 * DISCARD { ALL | SEQUENCES | TEMP | PLANS }
 */
void
DiscardCommand(DiscardStmt *stmt, bool isTopLevel)
{
	switch (stmt->target)
	{
		case DISCARD_ALL:
			DiscardAll(isTopLevel);

			/*
			 * DISCARD ALL is not allowed in a transaction block, so no
			 * two-phase commit required.
			 */
			if (Gp_role == GP_ROLE_DISPATCH)
				CdbDispatchCommand("DISCARD ALL", 0, NULL);
			break;

		case DISCARD_PLANS:
			ResetPlanCache();
			/* no dispatch, there should be no cached plans in segments */
			break;

		case DISCARD_SEQUENCES:
			ResetSequenceCaches();
			/* no dispatch, there should be no sequence caches in segments */
			break;

		case DISCARD_TEMP:
			ResetTempTableNamespace();

			/*
			 * Dispatch using two-phase commit, so that the effect of DISCARD
			 * TEMP can be rolled back if it's run in a transaction.
			 */
			if (Gp_role == GP_ROLE_DISPATCH)
				CdbDispatchCommand("DISCARD TEMP", DF_NEED_TWO_PHASE, NULL);
			break;

		default:
			elog(ERROR, "unrecognized DISCARD target: %d", stmt->target);
	}
}

static void
DiscardAll(bool isTopLevel)
{
	/*
	 * Disallow DISCARD ALL in a transaction block. This is arguably
	 * inconsistent (we don't make a similar check in the command sequence
	 * that DISCARD ALL is equivalent to), but the idea is to catch mistakes:
	 * DISCARD ALL inside a transaction block would leave the transaction
	 * still uncommitted.
	 */
	PreventTransactionChain(isTopLevel, "DISCARD ALL");

	/* Closing portals might run user-defined code, so do that first. */
	PortalHashTableDeleteAll();
	SetPGVariable("session_authorization", NIL, false);
	ResetAllOptions();
	DropAllPreparedStatements();
	Async_UnlistenAll();
	LockReleaseAll(USER_LOCKMETHOD, true);
	ResetPlanCache();
	ResetTempTableNamespace();
	ResetSequenceCaches();
}
