/*-------------------------------------------------------------------------
 *
 * cdb_backup_state.h
 *
 * Structures and functions to keep track of the progress
 * of a remote backup, implemented as a state machine
 *-------------------------------------------------------------------------
 */
#ifndef CDB_BACKUP_STATE_H
#define CDB_BACKUP_STATE_H

#include "libpq-fe.h"
#include "cdb_backup_status.h"

/* --------------------------------------------------------------------------------------------------
 * Enum for the values of the states
 */
typedef enum backup_state
{
	/* The first state is the start state */
	STATE_INITIAL = 0,
	STATE_BACKUP_STARTED = 1,
	STATE_SET_SERIALIZABLE = 2,
	STATE_SET_GOTLOCKS = 3,
	/* The following are all final states */
	STATE_BACKUP_FINISHED = 4,
	STATE_TIMEOUT = 5,
	STATE_BACKUP_ERROR = 6,
	STATE_UNEXPECTED_INPUT = 7,
}	BackupState;

/* --------------------------------------------------------------------------------------------------
 * Structure for the backup state machine
 */
typedef struct backup_state_machine
{
	BackupState	currentState;
	int			nWaits;
	bool		bStatus;
	bool		bReceivedSetSerializable;
	bool		bReceivedGotLocks;
	char		*pszNotifyRelName;		/* "N<backupkey>_<contentid>_<dbid>" */
	char		*pszNotifyRelNameStart;	/* "<pszNotifyRelName>_Start" */
	char		*pszNotifyRelNameSetSerializable; /* "<pszNotifyRelName>_SetSerializable" */
	char		*pszNotifyRelNameGotLocks; /* "<pszNotifyRelName>_SetGotLocks" */
	char		*pszNotifyRelNameSucceed; /* "<pszNotifyRelName>_Success" */
	char		*pszNotifyRelNameFail;	/* "<pszNotifyRelName>_Fail" */
	char		*pszNotifyRelNameMasterProbe;	/* "<pszNotifyRelName>_MasterProbe" */
	char		*pszNotifyRelNameSegmentProbe;	/* "<pszNotifyRelName>_SegmentProbe" */
	PGnotify	**ppNotifyAr;
	int			nArSize;
	int			nArCount;
}	BackupStateMachine;

/* Adds a notification struct */
extern bool AddNotificationtoBackupStateMachine(BackupStateMachine * pStateMachine, PGnotify *pNotify);

/* Frees notification structs and resets count */
extern void CleanupNotifications(BackupStateMachine * pStateMachine);

/* constructs a BackupStateMachine object */
extern BackupStateMachine *CreateBackupStateMachine(const char *pszKey, int instid, int segid);

/* destroys a BackupStateMachine object */
extern void DestroyBackupStateMachine(BackupStateMachine * pStateMachine);

/* test the current state to see whether it's left the STATE_INITIAL */
extern bool HasReceivedSetSerializable(BackupStateMachine * pStateMachine);

/* test the current state to see whether it's left the STATE_INITIAL */
extern bool HasReceivedGotLocks(BackupStateMachine * pStateMachine);

/* test the current state to see whether it's left the STATE_INITIAL */
extern bool HasStarted(BackupStateMachine * pStateMachine);

/* test the current state to see whether it's in a final state */
extern bool IsFinalState(BackupStateMachine * pStateMachine);

/* takes an input in the form of a StatusRow and moves to the appropriate state.*/
extern void ProcessInput(BackupStateMachine * pStateMachine);

#endif   /* CDB_BACKUP_STATE_H */
