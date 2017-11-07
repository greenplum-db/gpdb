/*-------------------------------------------------------------------------
 *
 * session_state.h
 *	  This file contains declarations for session state information.
 *
 * Copyright (c) 2014-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/utils/session_state.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef SESSIONSTATE_H
#define SESSIONSTATE_H

/* The runaway status of a session based on vmem usage */
typedef enum RunawayStatus
{
	/* The session is not runaway */
	RunawayStatus_NotRunaway = 0,
	/* The session is marked as runaway because of most vmem usage */
	RunawayStatus_PrimaryRunawaySession = 1,
	/*
	 * The session is marked as runaway despite being *not* the highest consumer,
	 * as the topmost session is idle and therefore cannot be marked as runaway
	 */
	RunawayStatus_SecondaryRunawaySession = 2,
} RunawayStatus;

/*
 * SessionState maintains a set of session properties such as the vmem usage,
 * number of active processes, a session's runaway status because of high vmem
 * usage etc.
 */
typedef struct SessionState
{
	int sessionId;

	/* Number of VMEM chunks reserved by this session on this segment */
	int sessionVmem;

	/*
	 * Lock to update activeQECount and cleanupCountdown.
	 */
	slock_t	spinLock;

	/*
	 * Is this session chosen by a runaway session detector as a runaway session?
	 */
	RunawayStatus runawayStatus;

	/*
	 * The amount of Vmem used by the session when it was flagged as runaway
	 */
	int sessionVmemRunaway;

	/*
	 * The value of the command count running when the session was flagged as runaway
	 */
	int commandCountRunaway;

	/* How many QEs are not blocked in ReadCommand */
	int activeProcessCount;

	/* The last seen time that a QE from this session became idle */
	TimestampTz idle_start;

	/*
	 * At the time of a runaway event, we set this to the activeQECount and
	 * as each QE cleans up, it decrements this counter. Once the counter
	 * reaches 0, the session is considered clean.
	 */
	int cleanupCountdown;

	/* Additional info for shared memory maintenance */

	/*
	 * Number of processes sharing this session state. Once we hit 0, we
	 * release the state back to the freeList of SessionState so that
	 * another session can use it.
	 */
	int pinCount;

	/* Next in the list. If the current entry is in free list,
	 * the next would point to the next entry in the free list.
	 * If, however, the current entry is in the used list, the
	 * next entry would point to the next used entry */
	struct SessionState *next;

	/*
	 * Resource group per-session slot information.
	 */
	void *resGroupSlot;

#ifdef USE_ASSERT_CHECKING
	/* If we modify the sessionId in ProcMppSessionId, this field is turned on */
	bool isModifiedSessionId;
#endif
} SessionState;

/*
 * Contains pointer to the shared memory array of SessionState entries.
 * Additionally, we save the number of used entries, and the total number
 * of entries, along with free list and used list for these entries.
 */
typedef struct SessionStateArray
{
	/* Number of allocated entries */
	int			numSession;
	/* Maximum number of entries */
	int			maxSession;

	/* Head of the list of free entries */
	SessionState *freeList;
	/* Head of the list of used entries */
	SessionState *usedList;
	/* Pointer to the head of the entries */
	SessionState	   *sessions;
	/* Placeholder to find the address where the array of entries begin */
	void *data;
} SessionStateArray;

extern volatile SessionState *MySessionState;
extern volatile SessionStateArray *AllSessionStateEntries;

extern Size SessionState_ShmemSize(void);
extern void SessionState_ShmemInit(void);
extern void SessionState_Init(void);
extern void SessionState_Shutdown(void);
extern bool SessionState_IsAcquired(SessionState *sessionState);

#endif   /* SESSIONSTATE_H */
