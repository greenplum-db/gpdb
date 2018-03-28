/*-------------------------------------------------------------------------
 *
 * idle_resource_cleaner.c
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/tcop/idle_resource_cleaner.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "cdb/cdbgang.h"
#include "storage/proc.h"
#include "tcop/idle_resource_cleaner.h"

int			IdleSessionGangTimeout = 18000;
int			IdleSessionTimeoutCached = 0;

int			(*get_idle_session_timeout_hook) (void) = NULL;
void		(*idle_session_timeout_action_hook) (void) = NULL;

static enum
{
	GANG_TIMEOUT,
	IDLE_SESSION_TIMEOUT
}			NextTimeoutAction;

static int
get_idle_gang_timeout(void)
{
	if (IdleSessionGangTimeout == 0 || !GangsExist())
		return 0;

	return IdleSessionGangTimeout;
}

static void
idle_gang_timeout_action(void)
{
	DisconnectAndDestroyUnusedGangs();
}

/*
 * We want to check to see if our session goes "idle" (nobody sending us work to
 * do). We decide this is true if after waiting a while, we don't get a message
 * from the client.
 * We can then free resources. Currently, we only free gangs on the segDBs by
 * default, but extensions can implement their own functionality with the
 * idle_session_timeout_action_hook.
 *
 * A bit ugly:  We share the sig alarm timer with the deadlock detection.
 * We know which it is (deadlock detection needs to run or idle
 * session resource release) based on the DoingCommandRead flag.
 *
 * Note we don't need to worry about the statement timeout timer
 * because it can't be running when we are idle.
 *
 * We want the time value to be long enough so we don't free gangs prematurely.
 * This means giving the end user enough time to type in the next SQL statement
 *
 * GPDB_93_MERGE_FIXME: replace the enable_sig_alarm() calls with the
 * new functionality provided in timeout.c from PG 9.3.
 */
void
StartIdleResourceCleanupTimers()
{
	/* get_idle_session_timeout_hook() may return different values later */
	IdleSessionTimeoutCached =
		get_idle_session_timeout_hook ? (*get_idle_session_timeout_hook) () : 0;
	const int	idleGangTimeout = get_idle_gang_timeout();

	if (idleGangTimeout > 0)
	{
		NextTimeoutAction = GANG_TIMEOUT;
		if (!enable_sig_alarm(idleGangTimeout, false))
			elog(FATAL, "could not set itimer for idle gang timeout");
	}
	else if (IdleSessionTimeoutCached > 0)
	{
		elog(DEBUG2, "Setting IdleSessionTimeout");
		NextTimeoutAction = IDLE_SESSION_TIMEOUT;
		if (!enable_sig_alarm(IdleSessionTimeoutCached, false))
			elog(FATAL, "could not set itimer for idle session timeout");
	}
}

/*
 * Called when a session has been idle for a while (waiting for the client
 * to send us SQL to execute). The idea is to consume less resources while
 * sitting idle, so we can support more sessions being logged on.
 *
 * The expectation is that if the session is logged on, but nobody is sending us
 * work to do, we want to free up whatever resources we can. Usually it means
 * there is a human being at the other end of the connection, and that person
 * has walked away from their terminal, or just hasn't decided what to do next.
 * We could be idle for a very long time (many hours).
 *
 * Of course, freeing gangs means that the next time the user does send in an
 * SQL statement, we need to allocate gangs (at least the writer gang) to do
 * anything. This entails extra work, so we don't want to do this if we don't
 * think the session has gone idle.
 *
 * PS: Is there anything we can free up on the master (QD) side? I can't
 * think of anything.
 */
void
DoIdleResourceCleanup(void)
{
	/* cancel the itimer so it doesn't recur */
	disable_sig_alarm(false);

	switch (NextTimeoutAction)
	{
		case GANG_TIMEOUT:
			elog(DEBUG2, "DoIdleResourceCleanup: idle gang timeout reached; killing gangs");
			idle_gang_timeout_action();

			if (IdleSessionTimeoutCached <= 0)
				return;

			if (IdleSessionGangTimeout < IdleSessionTimeoutCached)
			{					/* schedule the idle session timeout action */
				NextTimeoutAction = IDLE_SESSION_TIMEOUT;
				if (!enable_sig_alarm(IdleSessionTimeoutCached - IdleSessionGangTimeout, false))
					elog(FATAL, "could not set itimer for idle session timeout after gang timeout");
			}
			else
			{
				elog(DEBUG2, "DoIdleResourceCleanup: idle session timeout reached as GANG_TIMEOUT");

				/*
				 * the idle session timeout action should have occurred
				 * earlier, but we just do it now as it is a rare edge case
				 * and hard to handle properly
				 */
				if (idle_session_timeout_action_hook)
					(*idle_session_timeout_action_hook) ();
			}
			break;

		case IDLE_SESSION_TIMEOUT:
			elog(DEBUG2, "DoIdleResourceCleanup: idle session timeout reached as IDLE_SESSION_TIMEOUT");
			if (idle_session_timeout_action_hook)
				(*idle_session_timeout_action_hook) ();
			break;

		default:
			elog(FATAL, "DoIdleResourceCleanup: unrecognized NextTimeoutAction: %d", NextTimeoutAction);

	}
}
