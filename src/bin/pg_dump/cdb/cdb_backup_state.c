/*-------------------------------------------------------------------------
 *
 * cdb_backup_state.c
 *
 * Structures and functions to keep track of the progress
 * of a remote backup, impolemented as a state machine
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"
#include <assert.h>
#include "libpq-fe.h"
#include "pqexpbuffer.h"
#include "cdb_backup_status.h"
#include "cdb_dump_util.h"
#include "cdb_backup_state.h"

/* increase timeout to 10min for heavily loaded system */
#define WAIT_COUNT_MAX 300

static int	assignSortVal(const PGnotify *pNotify);
static int	cmpNotifyStructs(const void *lhs, const void *rhs);
static bool endsWith(const char *psz, const char *pszSuffix);

/* AddNotificationtoBackupStateMachine: This function adds a PGnotify* to an internal
 * array for later processing.	It was reallocate the array if it needs to.
 */
bool
AddNotificationtoBackupStateMachine(BackupStateMachine * pStateMachine, PGnotify *pNotify)
{
	if (pStateMachine->nArCount == pStateMachine->nArSize)
	{
		PGnotify  **ppNotifyAr = (PGnotify **) realloc(pStateMachine->ppNotifyAr,
							pStateMachine->nArSize * 2 * sizeof(PGnotify *));

		if (ppNotifyAr == NULL)
		{
			return false;
		}

		pStateMachine->nArSize *= 2;
		pStateMachine->ppNotifyAr = ppNotifyAr;
		memset(pStateMachine->ppNotifyAr + pStateMachine->nArCount, 0,
			   pStateMachine->nArCount * sizeof(PGnotify *));
	}

	pStateMachine->ppNotifyAr[pStateMachine->nArCount] = pNotify;
	pStateMachine->nArCount++;

	return true;
}

/* CleanupNotifications: Frees the PGnotify * objects in the internal array, and resets the array count to 0.
 * Does NOT deallocate the array memory, as it wiull be reused.
 */
void
CleanupNotifications(BackupStateMachine *pStateMachine)
{
	int			i;

	for (i = 0; i < pStateMachine->nArCount; i++)
	{
		if (pStateMachine->ppNotifyAr[i] != NULL)
		{
			PQfreemem(pStateMachine->ppNotifyAr[i]);
			pStateMachine->ppNotifyAr[i] = NULL;
		}
	}

	pStateMachine->nArCount = 0;
}

/* CreateBackupStateMachine: Constructs a BackupStateMachine object and initializes the members */
BackupStateMachine *
CreateBackupStateMachine(const char *pszKey, int instid, int segid)
{
	BackupStateMachine *pStateMachine = (BackupStateMachine *) malloc(sizeof(BackupStateMachine));

	if (pStateMachine == NULL)
		return pStateMachine;

	pStateMachine->currentState = STATE_INITIAL;
	pStateMachine->nWaits = 0;
	pStateMachine->bStatus = true;
	pStateMachine->ppNotifyAr = NULL;
	pStateMachine->bReceivedSetSerializable = false;
	pStateMachine->bReceivedGotLocks = false;

	pStateMachine->pszNotifyRelName = MakeString("N%s_%d_%d", pszKey, instid, segid);
	pStateMachine->pszNotifyRelNameStart = MakeString("%s_%s", pStateMachine->pszNotifyRelName, SUFFIX_START);
	pStateMachine->pszNotifyRelNameSetSerializable = MakeString("%s_%s", pStateMachine->pszNotifyRelName, SUFFIX_SET_SERIALIZABLE);
	pStateMachine->pszNotifyRelNameGotLocks = MakeString("%s_%s", pStateMachine->pszNotifyRelName, SUFFIX_GOTLOCKS);
	pStateMachine->pszNotifyRelNameSucceed = MakeString("%s_%s", pStateMachine->pszNotifyRelName, SUFFIX_SUCCEED);
	pStateMachine->pszNotifyRelNameFail = MakeString("%s_%s", pStateMachine->pszNotifyRelName, SUFFIX_FAIL);
	pStateMachine->pszNotifyRelNameMasterProbe = MakeString("%s_%s", pStateMachine->pszNotifyRelName, SUFFIX_MASTER_PROBE);
	pStateMachine->pszNotifyRelNameSegmentProbe = MakeString("%s_%s", pStateMachine->pszNotifyRelName, SUFFIX_SEGMENT_PROBE);

	pStateMachine->nArCount = 0;
	pStateMachine->nArSize = 10;

	pStateMachine->ppNotifyAr = (PGnotify **) calloc(pStateMachine->nArSize, sizeof(PGnotify *));
	if (pStateMachine->ppNotifyAr == NULL)
	{
		DestroyBackupStateMachine(pStateMachine);
		return NULL;
	}

	return pStateMachine;
}

/* DestroyBackupStateMachine: Frees a BackupStateMachine object, after freeing all the members */
void
DestroyBackupStateMachine(BackupStateMachine *pStateMachine)
{
	if (pStateMachine == NULL)
		return;

	if (pStateMachine->pszNotifyRelName != NULL)
		free(pStateMachine->pszNotifyRelName);

	if (pStateMachine->pszNotifyRelNameStart != NULL)
		free(pStateMachine->pszNotifyRelNameStart);

	if (pStateMachine->pszNotifyRelNameSetSerializable != NULL)
		free(pStateMachine->pszNotifyRelNameSetSerializable);

	if (pStateMachine->pszNotifyRelNameGotLocks != NULL)
		free(pStateMachine->pszNotifyRelNameGotLocks);

	if (pStateMachine->pszNotifyRelNameSucceed != NULL)
		free(pStateMachine->pszNotifyRelNameSucceed);

	if (pStateMachine->pszNotifyRelNameFail != NULL)
		free(pStateMachine->pszNotifyRelNameFail);

	if (pStateMachine->ppNotifyAr != NULL)
	{
		CleanupNotifications(pStateMachine);
		free(pStateMachine->ppNotifyAr);
	}

	free(pStateMachine);
}

/* HasReceivedSetSerializable: accessor function to the bReceivedSetSerializable member of the StateMachine */
bool
HasReceivedSetSerializable(BackupStateMachine *pStateMachine)
{
	return pStateMachine->bReceivedSetSerializable;
}

/* HasReceivedGotLocks: accessor function to the bReceivedGotLocks member of the StateMachine */
bool
HasReceivedGotLocks(BackupStateMachine *pStateMachine)
{
	return pStateMachine->bReceivedGotLocks;
}

/* HasStarted: Test the currentState to see whether it's left the start state yet. */
bool
HasStarted(BackupStateMachine *pStateMachine)
{
	return (pStateMachine->currentState != STATE_INITIAL);
}

/* IsFinalState: Test the currentState to see whether it's in a final state. */
bool
IsFinalState(BackupStateMachine *pStateMachine)
{
	return (pStateMachine->currentState == STATE_BACKUP_FINISHED ||
			pStateMachine->currentState == STATE_TIMEOUT ||
			pStateMachine->currentState == STATE_BACKUP_ERROR ||
			pStateMachine->currentState == STATE_UNEXPECTED_INPUT);
}

/* ProcessInput: loops through the array of PGnotify * elements, and processes them.
 * Based on the current state and the notification message, it changes the current state.
 * It's important to sort the PGnotify* elements so that we don't get the notifications
 * out of the proper order.
 */
void
ProcessInput(BackupStateMachine * pStateMachine)
{
	int			i;

	if (pStateMachine->nArCount == 0)
	{
		/*
		 * NULL input means that the timer on the socket select expired with
		 * no notifications. This is fine, unless we've not yet achieved the
		 * STATE_SET_SERIALIZABLE state. If we've not yet achieved the
		 * STATE_SET_SERIALIZABLE state, we want to timeout after 10 timer
		 * expiries.
		 */
		if (pStateMachine->currentState == STATE_INITIAL || pStateMachine->currentState == STATE_BACKUP_STARTED)
		{
			pStateMachine->nWaits++;
			if (pStateMachine->nWaits == WAIT_COUNT_MAX)
			{
				pStateMachine->currentState = STATE_TIMEOUT;
				pStateMachine->bStatus = false;
			}
		}

		return;
	}


	qsort(pStateMachine->ppNotifyAr, pStateMachine->nArCount, sizeof(PGnotify *), cmpNotifyStructs);

	for (i = 0; i < pStateMachine->nArCount; i++)
	{
		PGnotify   *pNotify = pStateMachine->ppNotifyAr[i];

		/*
		 * Once we are in a final state, we don't move.
		 */
		if (IsFinalState(pStateMachine))
			return;

		/*
		 * If the Notification indicates that the backend failed, we move to
		 * the STATE_BACKUP_ERROR state, irregardless of which state we are
		 * currently in.
		 */
		if (strcasecmp(pStateMachine->pszNotifyRelNameFail, pNotify->relname) == 0)
		{
			pStateMachine->bStatus = false;
			pStateMachine->currentState = STATE_BACKUP_ERROR;
			return;
		}

		/*
		 * Getting here means we didn't get a taskrc of failure.  This is the
		 * simple, non error case.	The order of the taskids should be
		 * TASK_START, TASK_SET_SERIALIZABLE, TASK_GOTLOCKS, TASK_FINISH,
		 * which moves the state from STATE_INITIAL -> STATE_BACKUP_STARTED ->
		 * STATE_SET_SERIALIZABLE -> STATE_GOTLOCKS -> STATE_FINISHED.
		 */
		switch (pStateMachine->currentState)
		{
			case STATE_INITIAL:
				if (strcasecmp(pStateMachine->pszNotifyRelNameStart, pNotify->relname) == 0)
				{
					pStateMachine->currentState = STATE_BACKUP_STARTED;
				}
				else
				{
					pStateMachine->bStatus = false;
					pStateMachine->currentState = STATE_UNEXPECTED_INPUT;
				}
				break;

			case STATE_BACKUP_STARTED:
				if (strcasecmp(pStateMachine->pszNotifyRelNameStart, pNotify->relname) == 0)
					break;

				if (strcasecmp(pStateMachine->pszNotifyRelNameSetSerializable, pNotify->relname) == 0)
				{
					pStateMachine->currentState = STATE_SET_SERIALIZABLE;
					pStateMachine->bReceivedSetSerializable = true;
				}
				else
				{
					pStateMachine->bStatus = false;
					pStateMachine->currentState = STATE_UNEXPECTED_INPUT;
				}
				break;

			case STATE_SET_SERIALIZABLE:
				if (strcasecmp(pStateMachine->pszNotifyRelNameStart, pNotify->relname) == 0)
					break;

				if (strcasecmp(pStateMachine->pszNotifyRelNameSetSerializable, pNotify->relname) == 0)
				{
					pStateMachine->bReceivedSetSerializable = true;
					break;
				}

				if (strcasecmp(pStateMachine->pszNotifyRelNameGotLocks, pNotify->relname) == 0)
				{
					pStateMachine->currentState = STATE_SET_GOTLOCKS;
					pStateMachine->bReceivedGotLocks = true;
				}
				else
				{
					pStateMachine->bStatus = false;
					pStateMachine->currentState = STATE_UNEXPECTED_INPUT;
				}
				break;

			case STATE_SET_GOTLOCKS:
				if (strcasecmp(pStateMachine->pszNotifyRelNameStart, pNotify->relname) == 0)
					break;
				if (strcasecmp(pStateMachine->pszNotifyRelNameSetSerializable, pNotify->relname) == 0)
					break;

				if (strcasecmp(pStateMachine->pszNotifyRelNameGotLocks, pNotify->relname) == 0)
				{
					pStateMachine->bReceivedGotLocks = true;
					break;
				}

				if (strcasecmp(pStateMachine->pszNotifyRelNameSucceed, pNotify->relname) == 0)
				{
					pStateMachine->currentState = STATE_BACKUP_FINISHED;
				}
				else
				{
					pStateMachine->bStatus = false;
					pStateMachine->currentState = STATE_UNEXPECTED_INPUT;
				}
				break;

			default:
				break;
		}
	}
}

/*
 * Static functions used by the above extern functions.
 */


/*
 * CmpNotifyStructs: compare function used in the qsort of the array of PGnotify*'s
 */
int
cmpNotifyStructs(const void *lhs, const void *rhs)
{
	const PGnotify *pLeft = *(const PGnotify **) lhs;
	const PGnotify *pRight = *(const PGnotify **) rhs;
	int			nLeft = assignSortVal(pLeft);
	int			nRight = assignSortVal(pRight);

	return nLeft - nRight;
}

/*
 * AssignSortVal: maps the pssible notifications to integers in the proper order
 */
int
assignSortVal(const PGnotify *pNotify)
{
	if (endsWith(pNotify->relname, SUFFIX_START))
		return 0;
	else if (endsWith(pNotify->relname, SUFFIX_SET_SERIALIZABLE))
		return 1;
	else if (endsWith(pNotify->relname, SUFFIX_GOTLOCKS))
		return 2;
	else if (endsWith(pNotify->relname, SUFFIX_SUCCEED))
		return 3;
	else if (endsWith(pNotify->relname, SUFFIX_FAIL))
		return 4;
	else
	{
		assert(false);
		return 5;
	}
}

/*
 * EndsWith: string utility function that tests whether a string ends with another string (case insensitive)
 */
bool
endsWith(const char *psz, const char *pszSuffix)
{
	if (strlen(psz) < strlen(pszSuffix))
		return false;

	if (strcasecmp(psz + strlen(psz) - strlen(pszSuffix), pszSuffix) == 0)
		return true;
	else
		return false;
}
