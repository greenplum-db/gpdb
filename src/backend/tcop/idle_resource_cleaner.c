/*-------------------------------------------------------------------------
 *
 * idle_resource_cleaner.c
 *
 * Portions Copyright (c) 2005-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/tcop/idle_resource_cleaner.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <signal.h>

#include "cdb/cdbgang.h"
#include "commands/async.h"
#include "storage/sinval.h"
#include "tcop/idle_resource_cleaner.h"
#include "utils/timeout.h"

int			IdleSessionGangTimeout = 18000;

static volatile sig_atomic_t clientWaitTimeoutInterruptEnabled = 0;

static volatile sig_atomic_t idle_gang_timeout_occurred;

/*
 * We want to check to see if our session goes "idle" (nobody sending us work to
 * do). We decide this is true if after waiting a while, we don't get a message
 * from the client.
 * We can then free resources. Currently, we only free gangs on the segDBs.
 *
 * We want the time value to be long enough so we don't free gangs prematurely.
 * This means giving the end user enough time to type in the next SQL statement
 */
void
StartIdleResourceCleanupTimers()
{
	if (IdleSessionGangTimeout <= 0 || !cdbcomponent_qesExist())
		return;

	enable_timeout_after(GANG_TIMEOUT, IdleSessionGangTimeout);
}

void
CancelIdleResourceCleanupTimers()
{
	disable_timeout(GANG_TIMEOUT, false);
}

void
EnableClientWaitTimeoutInterrupt(void)
{
	clientWaitTimeoutInterruptEnabled = 1;

	/* If a timeout occurred while the interrupt was disabled, process it now */
	if (idle_gang_timeout_occurred)
		IdleGangTimeoutHandler();

	/*
	 * NOTE: This is simpler than the corresponding notify and catchup
	 * interrupt code, because we don't expect the idle timeouts to fire again
	 * during the same client wait.
	 */
}

bool
DisableClientWaitTimeoutInterrupt(void)
{
	bool		result = (clientWaitTimeoutInterruptEnabled != 0);

	clientWaitTimeoutInterruptEnabled = 0;

	return result;
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
IdleGangTimeoutHandler(void)
{
	if (clientWaitTimeoutInterruptEnabled)
	{
		idle_gang_timeout_occurred = 0;

		DisconnectAndDestroyUnusedQEs();
	}
	else
		idle_gang_timeout_occurred = 1;
}
