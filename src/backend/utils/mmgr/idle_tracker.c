/*-------------------------------------------------------------------------
 *
 * idle_tracker.c
 *	 Implementation of the idle tracker that tracks whether a process is idle
 *	 (and therefore is incapable of responding to a runaway cleanup event).
 *	 This module coordinates with the runaway cleaner to ensure that an active
 *	 process cannot become idle before cleaning up for a pending runaway event.
 *
 * Copyright (c) 2014-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/mmgr/idle_tracker.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "cdb/cdbvars.h"
#include "miscadmin.h"
#include "storage/ipc.h"
#include "utils/session_state.h"
#include "utils/vmem_tracker.h"

/* External dependencies within the runaway cleanup framework */
extern bool vmemTrackerInited;
extern volatile EventVersion *CurrentVersion;
extern volatile EventVersion *latestRunawayVersion;
extern void RunawayCleaner_StartCleanup(void);
extern bool RunawayCleaner_IsCleanupInProgress(void );

/* The event version when this process was last activated */
EventVersion activationVersion = 0;

/* The event version when this process was last deactivated */
EventVersion deactivationVersion = 0;

/*
 * Is this process active (i.e., contributing 1 to activeProcessCount of the MySessionState).
 * As the activation/deactivation version can overlap in case there was no
 * runaway event in between activation and deactivation, this boolean flag
 * will indicate whether the activation or the deactivation event happened last.
 */
bool isProcessActive = false;

void IdleTracker_ShmemInit(void);
void IdleTracker_Init(void);
void IdleTracker_Shutdown(void);

/*
 * Initializes the idle tracker's shared memory states.
 */
void
IdleTracker_ShmemInit()
{
	if(!IsUnderPostmaster)
	{
		Assert(*CurrentVersion > 0);
		/*
		 * Set both activation and deactivation version greater than
		 * latestRunawayVersion so that they consider that runaway
		 * event as irrelevant.
		 */
		activationVersion = *CurrentVersion;
		deactivationVersion = *CurrentVersion;
		/* We will soon activate it, once the IdleTracker_Init() is called */
		isProcessActive = false;
	}
}

/*
 * Initializes the per-process states of the idle tracker.
 */
void
IdleTracker_Init()
{
	Assert(!vmemTrackerInited);
	Assert(!isProcessActive);
	/* Every process comes as pre-activated. */
	IdleTracker_ActivateProcess();
	Assert(0 < MySessionState->activeProcessCount);
}

/*
 * Deactivates and shuts down the idle tracker.
 */
void
IdleTracker_Shutdown()
{
	Assert(!vmemTrackerInited);
	/*
	 * Unsubscribe this process from future runaway cleanup consideration
	 * and cleanup one last time if necessary
	 */
	IdleTracker_DeactivateProcess();
	Assert(!isProcessActive);
}

/*
 * Marks the current process as active to the runaway detector. The runaway
 * detector needs at least one active process that can cleanup, if flagged as
 * runaway. If all the processes are idle, the session is incapable of responding
 * to a runaway cleanup request, and therefore is not considered a runaway
 * candidate by the runaway detector.
 */
void
IdleTracker_ActivateProcess()
{
	if (NULL != MySessionState)
	{
		Assert(!isProcessActive);
		/* No new runaway event can come in */
		SpinLockAcquire(&MySessionState->spinLock);

		/* No atomic update necessary as the update is protected by spin lock */
		MySessionState->activeProcessCount += 1;
		activationVersion = *CurrentVersion;
		isProcessActive = true;

		Assert(activationVersion >= deactivationVersion);
		/*
		 * Release spinLock as we no longer contend for isRunaway.
		 */
		SpinLockRelease(&MySessionState->spinLock);
	}
}

/*
 * Marks the current process as idle; i.e., it is no longer able to respond
 * to a runaway cleanup. However, before it returns from this method, it
 * would trigger one last runaway cleanup for a pre-dactivation era runaway
 * event, if necessary.
 */
void
IdleTracker_DeactivateProcess()
{
	if (NULL != MySessionState)
	{
		/*
		 * Verify that deactivation during proc_exit_inprogress is protected in
		 * critical section or the interrupt is disabled so that we don't attempt
		 * any runaway cleanup
		 */
		AssertImply(proc_exit_inprogress, CritSectionCount > 0 || InterruptHoldoffCount > 0);

		/*
		 * When an idle process receives a SIGTERM process, the signal handler
		 * die() calls the cleanup directly, so we get here for an idle process.
		 * Instead of re-activating it forcefully, just special case it
		 * and don't do anything during process exit for already inactive processes.
		 */
		if (proc_exit_inprogress && ! isProcessActive)
		{
			Assert(deactivationVersion >= activationVersion);
			return;
		}

		Assert(isProcessActive);
		Assert(deactivationVersion <= activationVersion);

		/* No new runaway event can come in */
		SpinLockAcquire(&MySessionState->spinLock);

		Assert(MySessionState->activeProcessCount <= MySessionState->pinCount);
		/* No atomic update necessary as the update is protected by spin lock */
		MySessionState->activeProcessCount -= 1;
		Assert(0 <= MySessionState->activeProcessCount);
		MySessionState->idle_start = GetCurrentTimestamp();
		isProcessActive = false;

		/* Save the point where we reduced the activeProcessCount */
		deactivationVersion = *CurrentVersion;
		/*
		 * Release spinLock as we no longer contend for isRunaway.
		 */
		SpinLockRelease(&MySessionState->spinLock);

		/*
		 * We are still deactivated (i.e., activeProcessCount is decremented). If an ERROR is indeed thrown
		 * from the VmemTracker_StartCleanupIfRunaway, the VmemTracker_RunawayCleanupDoneForProcess()
		 * method would reactivate this process.
		 */
		RunawayCleaner_StartCleanup();

		/* At this point the process must be clean, unless we don't have a runaway event before deactivation */
		Assert(*latestRunawayVersion > deactivationVersion ||
				!RunawayCleaner_IsCleanupInProgress());
	}

	/* At this point the process is ready to be blocked in ReadCommand() */
}
