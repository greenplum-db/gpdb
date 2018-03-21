/*-------------------------------------------------------------------------
 *
 * idle_resource_cleaner.c
 *
 *
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

int IdleSessionGangTimeout = 18000;

static int
get_idle_gang_timeout(void)
{
	return GangsExist() ? IdleSessionGangTimeout : 0;
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
 * We can then free resources (right now, just the gangs on the segDBs).
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
 *
 * GPDB_93_MERGE_FIXME: replace the enable_sig_alarm() calls with the
 * new functionality provided in timeout.c from PG 9.3.
 */
void StartIdleResourceCleanupTimers()
{
	int sigalarm_timeout = get_idle_gang_timeout();

	if (sigalarm_timeout > 0)
	{
		if (!enable_sig_alarm(sigalarm_timeout, false))
			elog(FATAL, "could not set timer for client wait timeout");
	}
}

/*
 * We get here when a session has been idle for a while (waiting for the client
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
 *
 */
void
DoIdleResourceCleanup(void)
{
	elog(DEBUG2,"DoIdleResourceCleanup");

	/*
	 * Cancel the timer, as there is no reason we need it to go off again
	 * (setitimer repeats by default).
	 */
	disable_sig_alarm(false);

	idle_gang_timeout_action();
}
