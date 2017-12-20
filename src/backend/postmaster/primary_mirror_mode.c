/**
 * Portions Copyright (c) 2005-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/postmaster/primary_mirror_mode.c
 *
 * Manages transitions between primary/mirror modes.
 *
 */
#include "postgres.h"
#include "postmaster/postmaster.h"
#include "postmaster/primary_mirror_mode.h"
#include "storage/pmsignal.h"
#include "storage/shmem.h"
#include "storage/spin.h"
#include "miscadmin.h"
#include "access/xlog.h"
#include "utils/guc.h"
#include "utils/netcheck.h"
#include "cdb/cdbvars.h"
#include <string.h>
#include "access/slru.h"
#include "access/xlog_internal.h"

#define BUFFER_LEN (2 * (MAXPGPATH))
/**
 * todo MIRRORING Review setup of signal handlers here --
 *                if we are inside a transition will we quit out properly on shutdown?
 */

/**
 * The transition state is used when the postmaster needs to return back to the server loop.
 *   The primary/mirror mode function will mark its transition as being in-progress...
 *   later calls to doRequestedPrimaryMirrorModeTransition will check this field.
 *
 * Note: if the order or # of values is changed then update the labels below.
 *
 * this implements a state machine.  Each transition (like to-primary-segment, or to-mirror-segment)
 *             manages its own transition order through this machine.
 *
 * Note that some places (notably, getFileRepRoleAndState) rely on the numerical order of these
 *   for implementation.  Be careful of that when changing this.
 */
typedef enum
{
        TSNone = 0,

        /*
         * The postmaster has signaled filerep to shutdown and is now waiting for it to
         *   complete
         */
        TSDoFilerepBackendsShutdown,
        TSAwaitingFilerepBackendsShutdown,
        TSDoneFilerepBackendsShutdown,
        TSDoFilerepShutdown,
        TSAwaitingFilerepShutdown,
        TSDoneFilerepShutdown,

        /* sequence for doing filerep startup */
        TSDoFilerepStartup,
        TSAwaitingFilerepStartup,
        TSDoneFilerepStartup,

        /* for two-phase recovery (where filerep goes into changelogging before database startup and
         *   needs to be notified after the database is up) */

        TSDoDatabaseStartup,
        TSAwaitingDatabaseStartup,
        TSDoneDatabaseStartup,

        TSDone,

        /* note: these should be in sync with getTransitionStateLabel */

        /* the number of elements in the enumeration */
        TS__EnumerationCount
} TransitionState;

/* State management */
static void assertModuleInitialized(void);
static void assertIsTransitioning(void);

/* actual transitions */
static PrimaryMirrorModeTransitionResult applyStepForTransitionToUninitializedMode(PrimaryMirrorModeTransitionArguments *args,
                                TransitionState *stateInOut, char extraResultInfoOut[MAX_TRANSITION_RESULT_EXTRA_INFO]);
static PrimaryMirrorModeTransitionResult applyStepForTransitionToMasterOrMirrorlessMode(PrimaryMirrorModeTransitionArguments *args,
                                TransitionState *stateInOut, char extraResultInfoOut[MAX_TRANSITION_RESULT_EXTRA_INFO]);
static PrimaryMirrorModeTransitionResult applyStepForTransitionToQuiescentSegmentMode(PrimaryMirrorModeTransitionArguments *args,
                                TransitionState *stateInOut, char extraResultInfoOut[MAX_TRANSITION_RESULT_EXTRA_INFO]);
static PrimaryMirrorModeTransitionResult applyStepForTransitionToPrimarySegmentMode(PrimaryMirrorModeTransitionArguments *args,
                                TransitionState *stateInOut, char extraResultInfoOut[MAX_TRANSITION_RESULT_EXTRA_INFO]);
static PrimaryMirrorModeTransitionResult applyStepForTransitionToMirrorSegmentMode(PrimaryMirrorModeTransitionArguments *args,
                                TransitionState *stateInOut, char extraResultInfoOut[MAX_TRANSITION_RESULT_EXTRA_INFO]);

/**
 * Shared globals for this module live in a single instance of PMModuleState.
 */
typedef struct PMModuleState
{
	/** all accesses to the isTransitionRequested and requestedTransition must lock on this */
	slock_t    lock;

    /* TRANSITION INFORMATION */

    /**
     * Has a transition been requested (and not yet fulfilled completed by the postmaster)?
     */
	volatile bool isTransitionRequested;

    /**
     * Has the backend that requested the current transition completed execution?
     */
	volatile bool isTransitionListenerFinished;

	/**
	 * The requested transition.
	 */
	volatile PrimaryMirrorModeTransitionArguments requestedTransition;

	/**
	 * The result of the transition; only valid when isTransitionRequested is false (postmaster done) and
	 *    isTransitionListenerFinished is also false (backend has not yet received this transitionResult
	 */
	volatile PrimaryMirrorModeTransitionResult transitionResult;

	/**
	 * The extra information about the transition result; used to report more info
	 *   back to the caller (such as to differentiate a mirroring failure from other failure cases)
	 */
	volatile char transitionResultExtraInfo[MAX_TRANSITION_RESULT_EXTRA_INFO];

	/**
	 * For transitions that require the postmaster to do multiple steps of work, this will hold the current
	 *    progress point.  Only valid while isTransitionRequested is true
	 */
	volatile TransitionState transitionState;

	/**
	  * Has the postmaster copied the currently requested transition to its local memory yet?
	  */
	volatile bool transitionCopiedToPMLocalMem;

    /* FILE REPLICATION PARAMETERS */
	volatile PrimaryMirrorMode mode;
	volatile SegmentState_e segmentState;
	volatile DataState_e dataState;
	volatile FaultType_e faultType;

	/* flag indicating if either primary or mirror has not completed re-synchronizing */
	volatile bool isResyncRunning;

	/* changeTrackingSessionId starts at zero and is incremented each time the system enters change tracking */
	volatile int64 changeTrackingSessionId;
	volatile bool isDatabaseRunning;

	/* connection info for self (which can be a mirror or primary, depending on mode) */
	volatile char hostAddress[PM_MAX_HOST_NAME_LENGTH+1];

	/* file replication port on self */
	volatile int hostPort;

	/* connection info for the peer (which can be a mirror or primary, depending on mode) */
	volatile char peerAddress[PM_MAX_HOST_NAME_LENGTH+1];

	/* file replication port on self */
	volatile int peerPort;

	/* is this current resync request a full resync? */
	volatile bool forceFullResync;

    /* OTHER FIELDS */
	volatile bool haveStartedDatabase;

    /**
     * a counter used to assign values to transitions; that values are for logging.
     */
	volatile int transitionNumberCounter;

	/**
	 *  if true, means we are in fault after postmaster reset and waiting for a transition to be sent
	 */
    volatile bool isInFaultFromPostmasterReset;

    /**
     * Maintained by the filerep code -- has I/O been suspended because of a fault or transition?
     */
    volatile bool isIOSuspended;

	/*
	 * We use PMModuleState to tell to change current dbid on the fly,
	 * because this structure is the only sane place we could use to tell some
	 * value from child process to postmaster.  This is mainly for master
	 * mirroring, although it sounds bizarre to use PMModuleState for this
	 * purpose.
	 */
	volatile int16 newdbid;
} PMModuleState;

static volatile PMModuleState *pmModuleState = NULL;

/**
 * Local globals live in a single PMLocalState instance
 */
typedef struct PMLocalState
{
    /**
     * The most recent transition applied.  This variable is only valid
     *   in the postmaster process; it is used to save around transitions
     *   for later postmaster reset.
     *
     * Note also that the dataState value in this transition MAY CHANGE because of an internal transition
     *    to resync!
     */
    PrimaryMirrorModeTransitionArguments mostRecentTransition;

    /**
     * See mostRecentTransition
     */
    bool haveSetMostRecentTransition;

    /**
     * Used if the reset of the filerep peer cannot be done to indicate that the postmaster reset should
     *   transition to fault rather than trying a peer postmaster reset
     */
    bool doNextResetToFault;

    /**
     * Was the most recent (non-fault) segment state "change tracking disabled"?
     */
    bool mostRecentSegmentStateWasChangeTrackingDisabled;

    /**
     * only valid when between calls to
     *   acquireModuleSpinLockAndMaybeStartCriticalSection and releaseModuleSpinLockAndEndCriticalSection
     * tells the value that was passed to acquireModuleSpinLockAndMaybeStartCriticalSection
     */
    bool criticalSectionStarted;
} PMLocalState;

PMLocalState pmLocalState;
bool gHaveInitializedLocalState = false;

static int numFilerepStartupPrint = 0;
static int numDatabaseStartupPrint = 0;

#define PRINT_BATCH_SIZE 500

/**
 * Extra result information that can be returned to the calling script so it
 *       can choose to take a different action besides just standard, generic handling
 */
#define RESULT_INFO_MIRRORING_FAILURE "MirroringFailure"
#define RESULT_INFO_POSTMASTER_DIED "PostmasterDied"
#define RESULT_INFO_IN_SHUTDOWN "ServerIsInShutdown"
#define RESULT_INFO_INVALID_STATE_TRANSITION "InvalidStateTransition"

/**
 * Return the log level to be used for the PrimaryMirror log messages
 *
 * @param isLowPriority is this a low-priority message? If so, the log level will be higher.  This can be set to true,
 *               for example, for messages that are printed out during polling events when a state has NOT changed.
 */
static int
getPrimaryMirrorModeDebugLogLevel(bool isLowPriority)
{
	if (isLowPriority)
		return DEBUG3;
	else return DEBUG1;
}

/**
 * LABELS and parsing of values
 */

/**
 * Get the label for a primary mirror mode.  Returns a pointer to static memory.
 *
 * Note that the label is externally visible (through a getStatus call to the postmaster, for example)
 *
 * note also that some python code (as used by gpstate) may use these label values, so when changing them
 *  be sure to search the management scripts code.
 */
const char *
getMirrorModeLabel(PrimaryMirrorMode mode)
{
	static const char *labels[] =
		{
			"Uninitialized", "Master", "MirrorlessSegment",
			"QuiescentSegment", "PrimarySegment", "MirrorSegment"
		};

	COMPILE_ASSERT(ARRAY_SIZE(labels) == PMMode__EnumerationCount);

	return labels[mode];
}

/**
 * Get the label for a data state.  Returns a pointer to static memory.
 *
 * Note that the label is externally visible (through a getStatus call to the postmaster, for example)
 *
 * note also that some python code (as used by gpstate) may use these label values, so when changing them
 *  be sure to search the management scripts code.
 */
const char *
getDataStateLabel(DataState_e state)
{
	static const char *labels[] =
		{
			"NotInitialized", "InSync", "InChangeTracking",
			"InResync"
		};
	COMPILE_ASSERT(ARRAY_SIZE(labels) == DataState__EnumerationCount);

	return labels[state];
}

/**
 * Note that the label is externally visible (through a getStatus call to the postmaster, for example)
 *
 * note also that some python code (as used by gpstate) may use these label values, so when changing them
 *  be sure to search the management scripts code.
 */
const char *
getSegmentStateLabel(SegmentState_e state)
{
	static const char *labels[] =
		{
			"NotInitialized", "Initialization", "InChangeTrackingTransition", "InResyncTransition",
			"InSyncTransition", "Ready", "ChangeTrackingDisabled", "Fault",
			"ShutdownBackends", "Shutdown", "ImmediateShutdown"
		};
	COMPILE_ASSERT(ARRAY_SIZE(labels) == SegmentState__EnumerationCount);

	return labels[state];
}

/**
 * Note that the label is externally visible (through a getStatus call to the postmaster, for example)
 *
 * note also that some python code (as used by gpstate) may use these label values, so when changing them
 *  be sure to search the management scripts code.
 */
const char *
getFaultTypeLabel(FaultType_e faultType)
{
	static const char *labels[] =
	{
		"NotInitialized",
		"FaultIO",
		"FaultDB",
		"FaultMirror",
		"FaultNet",
	};
	COMPILE_ASSERT(ARRAY_SIZE(labels) == FaultType__EnumerationCount);

	if (faultType < ARRAY_SIZE(labels))
		return labels[faultType];
	else
		return "UnrecognizedFaultType";
}

/**
 * Get the label for a transition result.  Returns a pointer to static memory.
 *
 * Note that scripts use these values in strings that may be parsed, so please keep
 *      them as alphabetic values only
 */
const char *
getTransitionResultLabel(PrimaryMirrorModeTransitionResult res)
{
	static const char *labels[] =
		{
			"Success", "InvalidTargetMode", "OtherTransitionInProgress",
			"InvalidInputParameter", "Error", "OkayButStillInProgress"
		};
	COMPILE_ASSERT(ARRAY_SIZE(labels) == PMTransition__EnumerationCount);

	return labels[res];
}

/**
 * Get the label for transition state.  Returns a pointer to static memory.
 */
static const char *
getTransitionStateLabel(TransitionState state)
{
	static const char *labels[] =
		{
			"None",

			"DoFilerepBackendsShutdown",
			"AwaitingFilerepBackendsShutdown",
			"DoneFilerepBackendsShutdown",

			"DoFilerepShutdown",
			"AwaitingFilerepShutdown",
			"DoneFilerepShutdown",

			"DoFilerepStartup",
			"AwaitingFilerepStartup",
			"DoneFilerepStartup",

			"DoDatabaseStartup",
			"AwaitingDatabaseStartup",
			"DoneDatabaseStartup",

			"Done"
		};
	COMPILE_ASSERT(ARRAY_SIZE(labels) == TS__EnumerationCount);

	return labels[state];
}


/**
 * Translate PrimaryMirrorMode to character compatible with 'role', as stored in gp_segment_configuration
 */
char getRole(PrimaryMirrorMode mode)
{
	Assert(mode >= PMModeUninitialized && mode < PMMode__EnumerationCount);

	switch (mode)
	{
		case PMModeMaster:
		case PMModeMirrorlessSegment:
		case PMModePrimarySegment:
			return 'p';

		case PMModeMirrorSegment:
			return 'm';

		case PMModeQuiescentSegment:
		case PMModeUninitialized:
			return '-';

		default:
			Assert(!"Invalid role");
			return '\0';
	}
}


/**
 * Translate DataState_e to character compatible with 'mode', as stored in gp_segment_configuration
 */
char getMode(DataState_e state)
{
	Assert(state >= DataStateNotInitialized && state < DataState__EnumerationCount);

	switch (state)
	{
		case DataStateInSync:
			return 's';

		case DataStateInResync:
			return 'r';

		case DataStateInChangeTracking:
			return 'c';

		case DataStateNotInitialized:
			return '-';

		default:
			Assert(!"Invalid mode");
			return '\0';
	}
}


/**
 * Convert the given string name for a primary mirror mode to a mode.  Note
 *    that any unparseable arg value returns PMModeUninitialized.
 *
 * @param arg should be non-NULL
 */
PrimaryMirrorMode
decipherPrimaryMirrorModeArgument(const char *arg)
{
	if (strcmp("master", arg) == 0)
		return PMModeMaster;
	else if (strcmp("mirrorless", arg) == 0)
		return PMModeMirrorlessSegment;
	else if (strcmp("quiescent", arg) == 0)
		return PMModeQuiescentSegment;
	else if (strcmp("primary", arg) == 0)
		return PMModePrimarySegment;
	else if (strcmp("mirror", arg) == 0)
		return PMModeMirrorSegment;
	else
		return PMModeUninitialized;
}

/**
 * acquire the module spin lock that is used to protect the contents of pmModuleState,
 *   and perhaps start a critical section (START_CRIT_SECTION), according to the parameter startCriticalSection
 */
static void
acquireModuleSpinLockAndMaybeStartCriticalSection(bool startCriticalSection)
{
	Assert(!pmLocalState.criticalSectionStarted);
	if (startCriticalSection)
	{
		pmLocalState.criticalSectionStarted = true;
		START_CRIT_SECTION();
	}
	SpinLockAcquire(&pmModuleState->lock);
}

/**
 * release the module spin lock that is used to protect the contents of pmModuleState
 */
static void
releaseModuleSpinLockAndEndCriticalSectionAsNeeded(void)
{
	SpinLockRelease(&pmModuleState->lock);
	if (pmLocalState.criticalSectionStarted)
	{
		pmLocalState.criticalSectionStarted = false;
		END_CRIT_SECTION();
	}
}

static void
clearTransitionArgs(volatile PrimaryMirrorModeTransitionArguments *args)
{
	args->mode = PMModeUninitialized;
	args->dataState = DataStateNotInitialized;
	args->hostAddress[0] = '\0';
	args->hostPort = 0;
	args->peerAddress[0] = '\0';
	args->peerPort = 0;
	args->forceFullResync = false;
	args->transitionToFault = false;
	args->transitionToChangeTrackingDisabledMode = false;
}

PrimaryMirrorModeTransitionArguments *
createNewTransitionArguments(void)
{
	PrimaryMirrorModeTransitionArguments *result = palloc(sizeof(PrimaryMirrorModeTransitionArguments));
	clearTransitionArgs(result);
	return result;
}

static void
assertModuleInitialized(void)
{
	Assert(pmModuleState != NULL && "init must be called first");
}

static void
copyHostData(char **addressOut, int *portOut, char *address, int port)
{
	*addressOut = pstrdup(address);
	*portOut = port;
}

void
getPrimaryMirrorStatusCodes(PrimaryMirrorMode *pm_mode,
							SegmentState_e *s_state,
							DataState_e *d_state,
							FaultType_e *f_type)
{
	acquireModuleSpinLockAndMaybeStartCriticalSection(false);
	{
        if (pm_mode)
            *pm_mode = pmModuleState->mode;

        if (s_state)
            *s_state = pmModuleState->segmentState;

        if (d_state)
            *d_state = pmModuleState->dataState;

		if (f_type)
			*f_type = pmModuleState->faultType;
	}
	releaseModuleSpinLockAndEndCriticalSectionAsNeeded();
}


void
getPrimaryMirrorModeStatus(char *bufOut, int bufSize)
{
	PrimaryMirrorMode pm_mode;
	SegmentState_e s_state;
	DataState_e d_state;
	FaultType_e f_type;

	assertModuleInitialized();

	getPrimaryMirrorStatusCodes(&pm_mode, &s_state, &d_state, &f_type);

	snprintf(bufOut, bufSize, "mode: %s\n""segmentState: %s\n""dataState: %s\n""faultType: %s",
			 getMirrorModeLabel(pm_mode), getSegmentStateLabel(s_state), getDataStateLabel(d_state), getFaultTypeLabel(f_type));
}

/**
 * This should only be called by FileRepMain during its startup
 */
void
primaryMirrorGetFilerepArguments(
	FileRepRole_e	*fileRepRoleOut,
	SegmentState_e	*segmentStateOut,
	DataState_e		*dataStateOut,
	char **primaryHostAddressOut,
	int *primaryPortOut,
	char **mirrorHostAddressOut,
	int *mirrorPortOut,
	bool *fullSyncOut)
{
	PrimaryMirrorMode mode;
	volatile PrimaryMirrorModeTransitionArguments *args;

	assertModuleInitialized();

	acquireModuleSpinLockAndMaybeStartCriticalSection(false);
	{
		args = &pmModuleState->requestedTransition;
		mode = args->mode;

		if (mode == PMModePrimarySegment)
		{
			*fileRepRoleOut = FileRepPrimaryRole;
			copyHostData(primaryHostAddressOut, primaryPortOut, (char*)args->hostAddress, args->hostPort);
			copyHostData(mirrorHostAddressOut, mirrorPortOut, (char*)args->peerAddress, args->peerPort);
			*dataStateOut = args->dataState;
			*segmentStateOut = pmModuleState->segmentState;
			*fullSyncOut = args->forceFullResync;
		}
		else if (mode == PMModeMirrorSegment)
		{
			*fileRepRoleOut = FileRepMirrorRole;
			copyHostData(primaryHostAddressOut, primaryPortOut, (char*)args->peerAddress, args->peerPort);
			copyHostData(mirrorHostAddressOut, mirrorPortOut, (char*)args->hostAddress, args->hostPort);
			*dataStateOut = args->dataState;
			*segmentStateOut = pmModuleState->segmentState;
			*fullSyncOut = false;
		}
		else
		{
			*fileRepRoleOut = FileRepNoRoleConfigured;
			*primaryHostAddressOut = *mirrorHostAddressOut = NULL;
			*primaryPortOut = *mirrorPortOut = 0;
			*segmentStateOut = SegmentStateNotInitialized;
			*dataStateOut = DataStateNotInitialized;
			*fullSyncOut = false;
		}
	}
	releaseModuleSpinLockAndEndCriticalSectionAsNeeded();
}

bool
isFullResync(void)
{
	bool result;
	assertModuleInitialized();

	acquireModuleSpinLockAndMaybeStartCriticalSection(false);
	{
		result = pmModuleState->forceFullResync;
	}
	releaseModuleSpinLockAndEndCriticalSectionAsNeeded();

	return result;
}

void
setFullResync(bool isFullResync)
{
	acquireModuleSpinLockAndMaybeStartCriticalSection(true);
	{
		pmModuleState->forceFullResync = isFullResync;
	}
	releaseModuleSpinLockAndEndCriticalSectionAsNeeded();
}

bool
isResyncRunning(void)
{
	assertModuleInitialized();

	return pmModuleState->isResyncRunning;
}

void
setResyncCompleted()
{
	assertModuleInitialized();

	Assert(pmModuleState->dataState == DataStateInSync);

	pmModuleState->isResyncRunning = false;
}

bool
isDatabaseRunning(void)
{
	bool isDatabaseRunning = FALSE;

	assertModuleInitialized();

	acquireModuleSpinLockAndMaybeStartCriticalSection(false);
	{
	    isDatabaseRunning = pmModuleState->isDatabaseRunning;
	}
	releaseModuleSpinLockAndEndCriticalSectionAsNeeded();

	return(isDatabaseRunning);
}

/**
 * While holding the module spin lock, return whether or not we are in a valid transition.
 *
 * This is more than just checking pmModulestate->isTransitionRequested because that requested transition
 *      may be illegal
 */
static bool
isInValidTransition_UnderLock(void)
{
    return
		pmModuleState->isTransitionRequested && pmModuleState->transitionState >= TSDoFilerepBackendsShutdown;
}

/**
 *
 * Fetch the current file and target replication role and states.  Returns values through the pointer arguments.
 *
 * Note that any of the arguments may be NULL in which case that piece of information is not returned.
 *
 * @param fileRepRoleOut may be NULL.  If non-null, *fileRepRoleOut is filled in with the current filerep role
 * @param segmentStateOut may be NULL.  If non-null, *segmentStateOut is filled in with the current segment state
 * @param dataStateOut  may be NULL.  If non-null, *dataStateOut is filled in with the current segment state
 * @param isInFilerepTransitionOut  may be NULL.  If non-null, *isInTransitionOut is filled in with true or false depending on
 *                           if the segment is in filerep transition to primary or mirror mode
 * @param transitionTargetDataStateOut  may be NULL.  If non-null, *transitionTargetDataStateOut is filled
 *                           in with the target data state, or DataStateNotInitialized if there is
 *                           no transition to primary or mirror mode is in progress.
 */
void
getFileRepRoleAndState(
	FileRepRole_e	*fileRepRoleOut,
	SegmentState_e	*segmentStateOut,
	DataState_e		*dataStateOut,

	bool *isInFilerepTransitionOut,
	DataState_e *transitionTargetDataStateOut
	)
{
	assertModuleInitialized();

	FileRepRole_e fileRepRole;
	SegmentState_e	segmentState;
	DataState_e		dataState;
	bool isInFilerepTransition;
	DataState_e transitionTargetDataState;

	acquireModuleSpinLockAndMaybeStartCriticalSection(false);
	{
		/* check to see if postmaster is still processing a transition.  Note that here we don't
		 *  need to check to see if the listener is complete
		 **/
		isInFilerepTransition = isInValidTransition_UnderLock();

		PrimaryMirrorMode mode = pmModuleState->mode;
		switch (mode)
		{
			case PMModePrimarySegment:
			case PMModeMirrorSegment:
				fileRepRole = mode == PMModePrimarySegment ? FileRepPrimaryRole : FileRepMirrorRole;
				dataState = pmModuleState->dataState;
				segmentState = pmModuleState->segmentState;

				if (isInFilerepTransition)
				{
					transitionTargetDataState = pmModuleState->requestedTransition.dataState;
				}
				else
				{
					transitionTargetDataState = DataStateNotInitialized;
				}

				break;
			default:
				fileRepRole = FileRepNoRoleConfigured;
				dataState = DataStateNotInitialized;
				segmentState = SegmentStateNotInitialized;
				transitionTargetDataState = DataStateNotInitialized;
				break;
		}
	}
	releaseModuleSpinLockAndEndCriticalSectionAsNeeded();

	/* now fill in output! */
	if (fileRepRoleOut)
		*fileRepRoleOut = fileRepRole;
	if (segmentStateOut)
		*segmentStateOut = segmentState;
	if (dataStateOut)
		*dataStateOut = dataState;

	if (isInFilerepTransitionOut)
		*isInFilerepTransitionOut = isInFilerepTransition;
	if (transitionTargetDataStateOut)
		*transitionTargetDataStateOut = transitionTargetDataState;
}

static void
assertIsTransitioning(void)
{
	assertModuleInitialized();
	Assert((pmModuleState->isTransitionRequested || !pmModuleState->isTransitionListenerFinished)
	                    && "transition must be marked in-progress");
}

/**
 * NOTE: in some cases the caller should also call isInFaultFromPostmasterReset() to check to
 *       make sure the database will be started/should be started.  When in fault from postmaster reset,
 *       database processes should not be started.
 */
bool
isPrimaryMirrorModeAFullPostmaster(bool checkTargetModeAsWell)
{
	bool result;

	assertModuleInitialized();

	/* the locking here may not be technically necessary */
	acquireModuleSpinLockAndMaybeStartCriticalSection(false);
	{
		PrimaryMirrorMode mode = pmModuleState->mode;
		result = mode == PMModeMaster || mode == PMModePrimarySegment || mode == PMModeMirrorlessSegment;

		if (checkTargetModeAsWell && isInValidTransition_UnderLock())
		{
			mode = pmModuleState->requestedTransition.mode;
			result |= mode == PMModeMaster || mode == PMModePrimarySegment || mode == PMModeMirrorlessSegment;
		}
	}
	releaseModuleSpinLockAndEndCriticalSectionAsNeeded();
	return result;
}

bool
isInFaultFromPostmasterReset()
{
    bool result;

    assertModuleInitialized();
	/* the locking here may not be technically necessary */
	acquireModuleSpinLockAndMaybeStartCriticalSection(false);
	{
		result = pmModuleState->isInFaultFromPostmasterReset;
	}
	releaseModuleSpinLockAndEndCriticalSectionAsNeeded();
	return result;
}

bool
doesPrimaryMirrorModeRequireFilerepProcess(bool checkTargetModeAsWell)
{
	bool result;

	assertModuleInitialized();

	/* the locking here may not be technically necessary */
	acquireModuleSpinLockAndMaybeStartCriticalSection(false);
	{
		PrimaryMirrorMode mode = pmModuleState->mode;
		result = mode == PMModePrimarySegment || mode == PMModeMirrorSegment;

		if (checkTargetModeAsWell && isInValidTransition_UnderLock())
		{
			mode = pmModuleState->requestedTransition.mode;
			result |= mode == PMModePrimarySegment || mode == PMModeMirrorSegment;
		}
	}
	releaseModuleSpinLockAndEndCriticalSectionAsNeeded();
	return result;
}

Size
primaryMirrorModeShmemSize(void)
{
	Size		size;

	size = sizeof(PMModuleState);
	return size;
}

void
primaryMirrorModeShmemInit(void)
{
	if (!gHaveInitializedLocalState)
	{
		/* only initialize local state once */
		pmLocalState.haveSetMostRecentTransition = false;
		pmLocalState.mostRecentSegmentStateWasChangeTrackingDisabled = false;
		pmLocalState.criticalSectionStarted = false;

		gHaveInitializedLocalState = true;
	}

	pmModuleState = (PMModuleState *) ShmemAlloc(sizeof(PMModuleState));
	pmModuleState->isTransitionRequested = false;
	pmModuleState->transitionCopiedToPMLocalMem = false;
	pmModuleState->mode = PMModeUninitialized;
	pmModuleState->segmentState = SegmentStateNotInitialized;
	pmModuleState->dataState = DataStateNotInitialized;
	pmModuleState->faultType = FaultTypeNotInitialized;
	pmModuleState->changeTrackingSessionId = 0;
	pmModuleState->isDatabaseRunning = false;
	pmModuleState->isTransitionListenerFinished = true;
	pmModuleState->transitionState = TSNone;
	pmModuleState->transitionResult = PMTransitionSuccess;
	pmModuleState->isResyncRunning = false;

	pmModuleState->hostAddress[0] = '\0';
	pmModuleState->hostPort = 0;
	pmModuleState->peerAddress[0] = '\0';
	pmModuleState->peerPort = 0;
	pmModuleState->forceFullResync = false;

	pmModuleState->transitionResultExtraInfo[0] = '\0';

	pmModuleState->haveStartedDatabase = false;
	pmModuleState->transitionNumberCounter = 1;
	pmModuleState->isInFaultFromPostmasterReset = false;
	pmModuleState->isIOSuspended = false;

	clearTransitionArgs(&pmModuleState->requestedTransition);

	SpinLockInit(&pmModuleState->lock);
}

/**
 * Reset all necessary parameters EXCEPT for the requested transition itself.  Caller must do that.
 *
 * Will be used for setting initial mode (on startup) and handling postmaster reset
 *
 * The only requestedTransition field reset is the transitionNumber
 */
static void
resetModuleStateForInitialTransition(void)
{
	pmModuleState->isTransitionRequested = true;
	pmModuleState->transitionCopiedToPMLocalMem = false;
	pmModuleState->isTransitionListenerFinished = true; /* yes, true, because there is no waiting dispatcher */
	pmModuleState->transitionState = TSNone;
    pmModuleState->requestedTransition.transitionNumber = pmModuleState->transitionNumberCounter++;
}


void
setInitialRequestedPrimaryMirrorMode(PrimaryMirrorMode targetMode)
{
	Assert(pmModuleState->mode == PMModeUninitialized);
	Assert(targetMode != PMModePrimarySegment && targetMode != PMModeMirrorSegment);

	pmModuleState->requestedTransition.mode = targetMode;
    resetModuleStateForInitialTransition();
}

/**
 * Mark that after postmaster reset (as processed by primaryMirrorHandlePostmasterReset)
 * segment should transition to mirroring fault state
 */
void
setFaultAfterReset(bool val)
{
	pmLocalState.doNextResetToFault = val;
}


/**
 * Update shared memory to transition segment to mirroring fault
 */
void
setTransitionToFault()
{
	/* mark mirroring fault if segment is not marked to transition to change-tracking */
	Assert(pmModuleState->requestedTransition.dataState != DataStateInChangeTracking);

	pmModuleState->requestedTransition.transitionToFault = true;
}


/**
 * Get flag from shared memory indicating transition to mirroring fault
 */
bool
getTransitionToFault()
{
	return pmModuleState->requestedTransition.transitionToFault;
}


/**
 * Mark completion of transition from resync to sync in shared memory
 */
void
primaryMirrorSetInSync()
{
	Assert(pmModuleState->dataState == DataStateInSync);

	acquireModuleSpinLockAndMaybeStartCriticalSection(true);
	{
		/* check if the last requested state was resync */
		if (pmModuleState->requestedTransition.dataState == DataStateInResync)
		{
			pmModuleState->requestedTransition.dataState = DataStateInSync;
		}
	}
	releaseModuleSpinLockAndEndCriticalSectionAsNeeded();
}


/**
 * Check if one of the NICs where the primary and mirror are attached is not active
 */
bool
primaryMirrorCheckNICFailure()
{
	char localHost[sizeof(pmModuleState->hostAddress)];
	char peerHost[sizeof(pmModuleState->peerAddress)];

	acquireModuleSpinLockAndMaybeStartCriticalSection(true);
	{
		/* get local and peer host names */
		strlcpy(localHost, (const char *) pmModuleState->hostAddress, sizeof(localHost));
		strlcpy(peerHost, (const char *) pmModuleState->peerAddress, sizeof(peerHost));
	}
	releaseModuleSpinLockAndEndCriticalSectionAsNeeded();

	/*
	 * if primary and mirror are uninitialized or co-located,
	 * there is no NIC redundancy
	 */
	if (0 == strcmp(localHost, peerHost))
	{
		return false;
	}

	int running = 0;
	if (NetCheckNIC(localHost))
	{
		running++;
	}
	if (NetCheckNIC(peerHost))
	{
		running++;
	}

	Assert(running > 0);

	/*
	 * report only a single NIC failure;
	 * double failures indicate errors in NIC configuration retrieval
	 */
	return (running == 1);
}


/**
 * Called on postmaster reset, after all children have been shut down but before starting the children back up.
 *   Returns true if the postmaster should try to reset the peer before doing a reset.
 */
bool
primaryMirrorPostmasterResetShouldRestartPeer(void)
{
	Assert(pmLocalState.haveSetMostRecentTransition);

	/* if we're a primary AND we were in resync/sync then try to restart in that mode */
	return (
		pmLocalState.mostRecentTransition.mode == PMModePrimarySegment &&
		pmLocalState.mostRecentTransition.dataState != DataStateInChangeTracking &&
		!getTransitionToFault());
}

/**
 * Handle a postmaster reset.  This is called AFTER shared memory has been reset.
 *
 * This function will use the locally stored data from the most recently requested transition
 *    in order to transition.
 */
void
primaryMirrorHandlePostmasterReset(void)
{
	bool transitionToFault = false;

	Assert(pmModuleState->mode == PMModeUninitialized);
	Assert(pmLocalState.haveSetMostRecentTransition);

	elog(LOG,
		 "PrimaryMirrorMode: Processing postmaster reset with recent mode of %d",
		 pmLocalState.mostRecentTransition.mode);

	if (pmLocalState.mostRecentTransition.mode == PMModePrimarySegment &&
	    pmLocalState.mostRecentTransition.dataState != DataStateInChangeTracking)
	{
		/* transition to fault, FTS will pick this up and take proper action */
		transitionToFault = pmLocalState.doNextResetToFault;
	}
	else
	{
		transitionToFault = false;
	}

	pmLocalState.doNextResetToFault = false;

	elog(LOG,
		 "PrimaryMirrorMode: Processing postmaster reset to %s state",
		 transitionToFault ? "fault" : "non-fault");

	pmModuleState->requestedTransition = pmLocalState.mostRecentTransition;
	pmModuleState->requestedTransition.transitionToFault = transitionToFault;

	if (pmModuleState->requestedTransition.dataState == DataStateInChangeTracking &&
		pmModuleState->requestedTransition.mode == PMModePrimarySegment &&

		pmLocalState.mostRecentSegmentStateWasChangeTrackingDisabled)
	{
		/*
		 * before reset we were in change tracking disabled; continue that through to after the reset
		 */
		pmModuleState->requestedTransition.transitionToChangeTrackingDisabledMode = true;
	}

	/* come after assigning requestedTransition, so we get the transition number set */
	resetModuleStateForInitialTransition();
}

/**
 *
 * @param extraResultInfoOut in the case of error, this may be populated with addition data (the
 *         RESULT_INFO_* values are the possible ones)
 */
PrimaryMirrorModeTransitionResult
requestTransitionToPrimaryMirrorMode(PrimaryMirrorModeTransitionArguments *args,
									 char extraResultInfoOut[MAX_TRANSITION_RESULT_EXTRA_INFO])
{
	bool isTransitionRequested = false;
	PrimaryMirrorMode targetMode;
	extraResultInfoOut[0] = '\0';

	Assert(args != NULL);
	assertModuleInitialized();
	targetMode = args->mode;

	elog(getPrimaryMirrorModeDebugLogLevel(true),
		 "PrimaryMirrorTransitionRequest: to primary/mirror mode %s, "
		 "data state %s, host %s, port %d, peer %s, peerPort %d%s%s",
		 getMirrorModeLabel(targetMode), getDataStateLabel(args->dataState),
		 args->hostAddress, args->hostPort,
		 args->peerAddress, args->peerPort,
		 args->forceFullResync ? ", force full resync" : "",
		 args->transitionToFault ? ", transition to fault" : ""
		);

	/* first, request the transition */
	PG_TRY();
	{
		bool isTransitionPending = false;
		bool isTransitionNoop = false;

		acquireModuleSpinLockAndMaybeStartCriticalSection(true);
		{
			args->transitionNumber = pmModuleState->transitionNumberCounter++;

			/* check if another transition is currently in progress */
			if (!pmModuleState->isTransitionListenerFinished)
			{
				isTransitionPending = true;
			}
			else
			{
				/* check if the transition is a no-op */
				if (args->mode == pmModuleState->mode &&
					args->dataState == pmModuleState->dataState &&
					args->hostPort == pmModuleState->hostPort &&
					args->peerPort == pmModuleState->peerPort &&
					strcmp((char *)args->hostAddress, (char *)pmModuleState->hostAddress) == 0 &&
					strcmp((char *)args->peerAddress, (char *)pmModuleState->peerAddress) == 0 &&
					!args->forceFullResync /* a force full resync can never be a no-op */
					)
				{
					isTransitionNoop = true;
				}
				else
				{
					isTransitionRequested = true;

					/* record new transition request */
					pmModuleState->isTransitionRequested = true;
					pmModuleState->requestedTransition = *args;
					pmModuleState->isTransitionListenerFinished = false;
					pmModuleState->transitionCopiedToPMLocalMem = false;
					pmModuleState->transitionState = TSNone;
					pmModuleState->requestedTransition.transitionNumber = args->transitionNumber;
				}
			}
		}
		releaseModuleSpinLockAndEndCriticalSectionAsNeeded(); /* set the postmaster loose */

		if (isTransitionPending)
		{
			Assert(!isTransitionNoop);
			Assert(!isTransitionRequested);

			elog(getPrimaryMirrorModeDebugLogLevel(false),
			     "PrimaryMirrorTransitionRequest (%d) Result: Transition to primary/mirror mode %s, "
			     "data state %s resulted in %s, pending request (%s %s)",
			     args->transitionNumber,
			     getMirrorModeLabel(targetMode), getDataStateLabel(args->dataState),
			     getTransitionResultLabel(PMTransitionOtherTransitionInProgress),
			     getMirrorModeLabel(pmModuleState->requestedTransition.mode),
			     getDataStateLabel(pmModuleState->requestedTransition.dataState));

			return PMTransitionOtherTransitionInProgress;
		}

		if (isTransitionNoop)
		{
			Assert(!isTransitionPending);
			Assert(!isTransitionRequested);

			elog(getPrimaryMirrorModeDebugLogLevel(false),
			     "PrimaryMirrorTransitionRequest (%d) Result: Transition to primary/mirror mode %s, "
			     "data state %s is no-op",
			     args->transitionNumber,
			     getMirrorModeLabel(targetMode),
			     getDataStateLabel(args->dataState));

			return PMTransitionSuccess;
		}

		/* tell postmaster to wake up */
		SendPostmasterSignal(PMSIGNAL_PRIMARY_MIRROR_TRANSITION_RECEIVED);

		/* Wait for postmaster to apply transition and turn isTransitionRequested to false */
		int counter = 0;
		for (;;)
		{
			PrimaryMirrorModeTransitionResult result = PMTransitionSuccess;
			bool isDone = false;
			bool postmasterIsDead = false;

			/* sleep 10 ms, waiting for postmaster to do something
			 * don't change the scale here without updating counter check below!
			 */
			pg_usleep(10 * 1000L);

			/* every ~ 1 second, see if postmaster is gone */
			counter++;
			if (counter == 100)
			{
				if (!PostmasterIsAlive(true))
					postmasterIsDead = true;
				counter = 0;
			}

			acquireModuleSpinLockAndMaybeStartCriticalSection(true);
			{
				if (postmasterIsDead)
				{
					pmModuleState->isTransitionRequested = false;
					pmModuleState->isTransitionListenerFinished = true;
					result = PMTransitionError;
					strlcpy(extraResultInfoOut, RESULT_INFO_POSTMASTER_DIED, MAX_TRANSITION_RESULT_EXTRA_INFO);
					isDone = true;
				}
				else if (!pmModuleState->isTransitionRequested)
				{
					// postmaster completed the work
					pmModuleState->isTransitionListenerFinished = true;
					result = pmModuleState->transitionResult;
					strlcpy(extraResultInfoOut, (char*) pmModuleState->transitionResultExtraInfo,
							MAX_TRANSITION_RESULT_EXTRA_INFO);
					isDone = true;
				}
			}
			releaseModuleSpinLockAndEndCriticalSectionAsNeeded();

			if (isDone)
			{
				/* this won't get transitionToFault, because that 'succeeds', but that's okay because
				 *    that happens on a panic from in-sync anyway */
				int logLevel = result == PMTransitionError ? WARNING : getPrimaryMirrorModeDebugLogLevel(false);
				elog(logLevel, "PrimaryMirrorTransitionRequest (%d) Result: Transition to primary/mirror mode %s, "
					 "data state %s%s resulted in %s",
					 args->transitionNumber,
					 getMirrorModeLabel(targetMode), getDataStateLabel(args->dataState),
					 args->transitionToFault ? " (to fault)" : "",
					 getTransitionResultLabel(result));
				return result;
			}
		}
	}
	PG_CATCH();
	{
		elog(getPrimaryMirrorModeDebugLogLevel(false),
			 "PrimaryMirrorTransitionRequest: Caught exception in requestTransitionToPrimaryMirrorMode(), "
			 "isTransitioning %d", isTransitionRequested);

		if (isTransitionRequested)
		{
			acquireModuleSpinLockAndMaybeStartCriticalSection(true);
			{
				pmModuleState->isTransitionListenerFinished = true;
			}
			releaseModuleSpinLockAndEndCriticalSectionAsNeeded();
		}
		PG_RE_THROW();
	}
	PG_END_TRY();
}

static bool
updateDataStateUnderSpinLock(DataState_e dataState)
{
	if (pmModuleState->dataState != dataState)
	{
		if (dataState == DataStateInChangeTracking)
		{
			pmModuleState->changeTrackingSessionId++;
		}
		if (dataState == DataStateInResync)
		{
			pmModuleState->isResyncRunning = true;
		}
		pmModuleState->dataState = dataState;
		return true;
	}
	return false;
}

int64
getChangeTrackingSessionId(void)
{
	assertModuleInitialized();
	return pmModuleState->changeTrackingSessionId;
}

/**
 * Update data state from an internal transition.  This should ONLY be called by
 *    filerep backend processes.
 */
bool
updateDataState(DataState_e dataState)
{
		bool isUpdated = FALSE;

		acquireModuleSpinLockAndMaybeStartCriticalSection(true);
		{
			(void) updateDataStateUnderSpinLock(dataState);
		}
		releaseModuleSpinLockAndEndCriticalSectionAsNeeded();

		return isUpdated;
	}

bool
updateSegmentState(SegmentState_e segmentState, FaultType_e faultType)
{
	bool isUpdated = FALSE;

	acquireModuleSpinLockAndMaybeStartCriticalSection(true);
	{
		SegmentState_e curState = pmModuleState->segmentState;
		if (curState == SegmentStateImmediateShutdown)
		{
			/* never set to anything else during immediate shutdown */
		}
		else if (curState == SegmentStateShutdown)
		{
			/* allow promotion from shutdown to immediate shutdown */
			if (segmentState == SegmentStateImmediateShutdown)
			{
				isUpdated = TRUE;
			}
		}
		else if (curState == SegmentStateShutdownFilerepBackends)
		{
			/* allow promotion from backend shutdown to other shutdowns */
			if (segmentState == SegmentStateShutdown ||
				segmentState == SegmentStateImmediateShutdown)
			{
				isUpdated = TRUE;
			}
		}
		else if (curState != segmentState)
		{
			/* During change tracking failure is reported by signaling postmaster Reset (no Fault) */
			if (! (segmentState == SegmentStateFault &&
				   pmModuleState->dataState == DataStateInChangeTracking))
			{
				isUpdated = TRUE;
			}
			else
			{
				if (curState == SegmentStateInChangeTrackingTransition)
				{
					isUpdated = TRUE;
				}
			}
		}
		if (isUpdated)
		{
			pmModuleState->segmentState = segmentState;

			if (segmentState == SegmentStateFault)
			{
				if (pmModuleState->faultType == FaultTypeNotInitialized)
					pmModuleState->faultType = faultType;
			}
		}
	}
	releaseModuleSpinLockAndEndCriticalSectionAsNeeded();

	return isUpdated;
}

/**
 * To be called by the postmaster.  The answer may become true immediately
 *  after this call but shouldn't (because postmaster is the only one who resets it)
 *  become false
 *
 * Does not acquire the lock as we are only peeking.
 */
bool
isPrimaryMirrorModeTransitionRequested(void)
{
	return pmModuleState->isTransitionRequested;
}


/*
 * Update local memory with passed transition request
 */
static void
setTransitionParametersToLocalMemory(PrimaryMirrorModeTransitionArguments *args)
{
	Assert(pmModuleState->isTransitionRequested);
	if (!pmModuleState->transitionCopiedToPMLocalMem)
	{
		pmLocalState.mostRecentTransition = *args;
		pmLocalState.haveSetMostRecentTransition = true;
		pmModuleState->transitionCopiedToPMLocalMem = true;
	}
}


/*
 * Copy most recent transition request from shared memory to local memory
 */
void copyTransitionParametersToLocalMemory()
{
	/* this is called before resetting shared memory so no locking is required */
	pmLocalState.mostRecentTransition = pmModuleState->requestedTransition;
	pmLocalState.haveSetMostRecentTransition = true;
	pmModuleState->transitionCopiedToPMLocalMem = true;
}


/**
 * Copy the transition input parameters into shared memory and reset isInFaultFromPostmasterReset
 */
static void
copyTransitionInputParameters(PrimaryMirrorModeTransitionArguments *args, SegmentState_e segmentState)
{
	acquireModuleSpinLockAndMaybeStartCriticalSection(true);
	{
		Assert(sizeof(args->hostAddress) == sizeof(pmModuleState->hostAddress));
		Assert(sizeof(args->peerAddress) == sizeof(pmModuleState->peerAddress));

		strcpy((char*) pmModuleState->hostAddress, args->hostAddress);
		pmModuleState->hostPort = args->hostPort;

		strcpy((char*) pmModuleState->peerAddress, args->peerAddress);
		pmModuleState->peerPort = args->peerPort;

		updateDataStateUnderSpinLock(args->dataState);
		pmModuleState->segmentState = segmentState;

		pmModuleState->isInFaultFromPostmasterReset = false;

		pmModuleState->forceFullResync = args->forceFullResync;

		if (segmentState != SegmentStateFault)
		{
			pmModuleState->faultType = FaultTypeNotInitialized;
		}

        /* we've reached the point of no-return, so update this */
        pmModuleState->mode = args->mode;
	}
	releaseModuleSpinLockAndEndCriticalSectionAsNeeded();
}


/**
 * This will be called with signals blocked.
 */
void
doRequestedPrimaryMirrorModeTransitions(bool isInShutdown)
{
	PrimaryMirrorModeTransitionArguments requestedTransition;
	PrimaryMirrorModeTransitionResult result;
	bool isRequested = false;
	bool isPrevRequestComplete = false;
	TransitionState currentState;
	int transitionNumber;
	bool transitionToFault;

	char extraResultInfo[MAX_TRANSITION_RESULT_EXTRA_INFO];

	Assert(!IsUnderPostmaster);
	assertModuleInitialized();

	acquireModuleSpinLockAndMaybeStartCriticalSection(true);
	{
		/* grab out values under spin lock */
		isRequested = pmModuleState->isTransitionRequested;
		isPrevRequestComplete = pmModuleState->isTransitionListenerFinished;
		requestedTransition = pmModuleState->requestedTransition;
		currentState = pmModuleState->transitionState;
		transitionNumber = pmModuleState->requestedTransition.transitionNumber;
		transitionToFault = pmModuleState->requestedTransition.transitionToFault;
	}
	releaseModuleSpinLockAndEndCriticalSectionAsNeeded();

	if (!isRequested /* && isPrevRequestComplete */)
	{
		if (!isPrevRequestComplete)
		{
			elog(getPrimaryMirrorModeDebugLogLevel(false),
				 "PostmasterPrimaryMirrorTransition (%d): Nothing to do but listener is not done",
				 transitionNumber);
		}
		return;
	}

	elog(getPrimaryMirrorModeDebugLogLevel(false),
		 "PostmasterPrimaryMirrorTransition (%d): Attempting requested transition mode to %s, "
		 "data state to %s, state is currently %s",
		 transitionNumber,
		 getMirrorModeLabel(requestedTransition.mode),
		 getDataStateLabel(requestedTransition.dataState),
		 getTransitionStateLabel(currentState));

	extraResultInfo[0] = '\0';
	if (isInShutdown)
	{
		elog(getPrimaryMirrorModeDebugLogLevel(false),
			 "PostmasterPrimaryMirrorTransition error: system is in shutdown");
		strlcpy(extraResultInfo, RESULT_INFO_IN_SHUTDOWN, MAX_TRANSITION_RESULT_EXTRA_INFO);
		result = PMTransitionError;
	}
	else if (transitionToFault)
	{
		assertIsTransitioning();

		/* filerep should have been shut down by postmaster reset prior to this transition */
		Assert(!IsFilerepProcessRunning());

		copyTransitionInputParameters(&requestedTransition, SegmentStateInitialization);

		/**
		 * must be after copying the transition input parameters;
		 * report to prober that we failed to reset peer;
		 */
		pmModuleState->isInFaultFromPostmasterReset = true;
		updateSegmentState(SegmentStateFault, FaultTypeMirror);

		result = PMTransitionSuccess;
	}
	else
	{
		for (;;)
		{
			TransitionState initialState = currentState;

			switch (requestedTransition.mode)
			{
				case PMModeUninitialized:
					result = applyStepForTransitionToUninitializedMode(&requestedTransition, &currentState, extraResultInfo);
					break;
				case PMModeMaster:
				case PMModeMirrorlessSegment:
					result = applyStepForTransitionToMasterOrMirrorlessMode(&requestedTransition, &currentState, extraResultInfo);
					break;
				case PMModePrimarySegment:
					result = applyStepForTransitionToPrimarySegmentMode(&requestedTransition, &currentState, extraResultInfo);
					break;
				case PMModeQuiescentSegment:
					result = applyStepForTransitionToQuiescentSegmentMode(&requestedTransition, &currentState, extraResultInfo);
					break;
				case PMModeMirrorSegment:
					result = applyStepForTransitionToMirrorSegmentMode(&requestedTransition, &currentState, extraResultInfo);
					break;
				default:
					Assert(!"Unknown targetMode");
					result = PMTransitionInvalidTargetMode;
					break;
			}

			if (initialState != currentState)
			{
				acquireModuleSpinLockAndMaybeStartCriticalSection(true);
				{
					pmModuleState->transitionState = currentState;
				}
				releaseModuleSpinLockAndEndCriticalSectionAsNeeded();

				elog(getPrimaryMirrorModeDebugLogLevel(false),
					 "PostmasterPrimaryMirrorTransition (%d), after completed step, transition state is %s",
					 transitionNumber, getTransitionStateLabel(currentState));
			}

			Assert(result != PMTransitionOtherTransitionInProgress);

			if (result != PMTransitionInvalidTargetMode &&
			    result != PMTransitionInvalidInputParameter)
			{
				/* copy transition request to local memory so that a postmaster reset can replay it */
				setTransitionParametersToLocalMemory(&requestedTransition);
			}

			if (result != PMTransitionSuccess)
			{
				break;
			}

			if (currentState == TSDone)
			{
				elog(getPrimaryMirrorModeDebugLogLevel(false),
					 "PostmasterPrimaryMirrorTransition (%d): transition to %s done",
					 transitionNumber,
					 getDataStateLabel(requestedTransition.dataState));
				result = PMTransitionSuccess;
				break;
			}
			else if (currentState == initialState)
			{
				result = PMTransitionOkayButStillInProgress;
				break;
			}
		}
	}

	if (result == PMTransitionOkayButStillInProgress)
	{
		Assert(pmModuleState->transitionState != TSNone);
		elog(getPrimaryMirrorModeDebugLogLevel(true),
			 "PostmasterPrimaryMirrorTransition (%d): in-progress, current state %s",
			 transitionNumber,
			 getTransitionStateLabel(currentState));
	}
	else
	{
		int logLevel = result == PMTransitionError ? WARNING : getPrimaryMirrorModeDebugLogLevel(false);
		elog(logLevel, "PostmasterPrimaryMirrorTransition (%d) Finished with %s",
			 transitionNumber, getTransitionResultLabel(result));
		acquireModuleSpinLockAndMaybeStartCriticalSection(true);
		{
			pmModuleState->transitionResult = result;
			strlcpy((char*)pmModuleState->transitionResultExtraInfo, extraResultInfo, MAX_TRANSITION_RESULT_EXTRA_INFO);
			pmModuleState->isTransitionRequested = false;
			elog(getPrimaryMirrorModeDebugLogLevel(false),
				 "PostmasterPrimaryMirrorTransition (%d): completed (waiting backend finished? %s)",
				 transitionNumber,
				 pmModuleState->isTransitionListenerFinished ? "yes" : "no");
			pmModuleState->transitionState = TSNone;
		}
		releaseModuleSpinLockAndEndCriticalSectionAsNeeded();
	}
}

static PrimaryMirrorModeTransitionResult
stepDoDatabaseStartup(TransitionState nextState, TransitionState *stateInOut)
{
	Assert(!pmModuleState->haveStartedDatabase);

	pmModuleState->haveStartedDatabase = true;
	StartMasterOrPrimaryPostmasterProcesses();
	if (IsDatabaseInRunMode())
	{
		pmModuleState->isDatabaseRunning = true;
	}
	*stateInOut = TSDone;
	return PMTransitionSuccess;
}

static PrimaryMirrorModeTransitionResult
stepDoFilerepStartup(TransitionState nextState, TransitionState *stateInOut)
{
#ifdef USE_ASSERT_CHECKING
	/* verify that filerep will get correct values when it asks for them */
	bool isInTransition = false;
	FileRepRole_e role = FileRepRoleNotInitialized;
	getFileRepRoleAndState(&role, NULL, NULL, &isInTransition, NULL);
	Assert(isInTransition);
	Assert(role == FileRepPrimaryRole || role == FileRepMirrorRole);
#endif

	StartFilerepProcesses();
	*stateInOut = nextState;
	return PMTransitionSuccess;
}

static PrimaryMirrorModeTransitionResult
stepDoFilerepShutdown(TransitionState nextState, TransitionState *stateInOut)
{
	/* shutdown filerep */
	SignalShutdownFilerepProcess();
	*stateInOut = nextState;
	return PMTransitionSuccess;
}

static PrimaryMirrorModeTransitionResult
stepDoFilerepBackendsShutdown(TransitionState nextState, TransitionState *stateInOut)
{
	/* shutdown filerep backend processes */
	SignalShutdownFilerepBackendProcesses();
	*stateInOut = nextState;
	return PMTransitionSuccess;
}

static PrimaryMirrorModeTransitionResult
stepWaitForFilerepShutdown(TransitionState nextState, TransitionState *stateInOut)
{
	/* waiting for filerep to complete shutdown */
	if (IsFilerepProcessRunning())
	{
		elog(getPrimaryMirrorModeDebugLogLevel(true),
			 "PostmasterPrimaryMirrorTransition: filerep still running, will wait longer");
	}
	else
	{
		/** note that at this point the postmaster will have set segment state to fault. */
		*stateInOut = nextState;
	}
	return PMTransitionSuccess;
}

static PrimaryMirrorModeTransitionResult
stepWaitForFilerepBackendsShutdown(TransitionState nextState, TransitionState *stateInOut)
{
	/* waiting for filerep to complete shutdown of its backends */
	if (IsFilerepBackendsDoneShutdown())
	{
		*stateInOut = nextState;
	}
	else
	{
		elog(getPrimaryMirrorModeDebugLogLevel(true),
			 "PostmasterPrimaryMirrorTransition: will wait longer for backends to shutdown");
	}
	return PMTransitionSuccess;
}

static PrimaryMirrorModeTransitionResult
stepWaitForFilerepStartup(TransitionState nextState, TransitionState *stateInOut,
						  char extraResultInfoOut[MAX_TRANSITION_RESULT_EXTRA_INFO])
{
	SegmentState_e segmentState;

	getPrimaryMirrorStatusCodes(NULL, &segmentState, NULL, NULL);

	if (!IsFilerepProcessRunning() ||
		segmentState == SegmentStateFault)
	{
		elog(getPrimaryMirrorModeDebugLogLevel(false),
			 "PostmasterPrimaryMirrorTransition: filerep startup: transition error");
		if (updateSegmentState(SegmentStateFault, FaultTypeMirror))
			NotifyProcessesOfFilerepStateChange();
		strlcpy(extraResultInfoOut, RESULT_INFO_MIRRORING_FAILURE, MAX_TRANSITION_RESULT_EXTRA_INFO);
		return PMTransitionError;
	}

	if (segmentState == SegmentStateReady ||
		segmentState == SegmentStateChangeTrackingDisabled)
	{
		elog(getPrimaryMirrorModeDebugLogLevel(false),
			 "PostmasterPrimaryMirrorTransition: filerep startup: completed");
		*stateInOut = nextState;
	}
	else
	{
		elog(getPrimaryMirrorModeDebugLogLevel(true),
			 "PostmasterPrimaryMirrorTransition: filerep startup: still starting up");
	}
	return PMTransitionSuccess;
}

static PrimaryMirrorModeTransitionResult
invalidMode(PrimaryMirrorModeTransitionArguments *args)
{
	elog(getPrimaryMirrorModeDebugLogLevel(false),
		 "PostmasterPrimaryMirrorTransition: invalid target mode");
	return PMTransitionInvalidTargetMode;
}

/**
 * being in an invalid state is a programming error!
 */
static PrimaryMirrorModeTransitionResult
invalidState(TransitionState *stateInOut, char extraResultInfoOut[MAX_TRANSITION_RESULT_EXTRA_INFO])
{
	elog(ERROR, "Invalid state %d", *stateInOut);
	strlcpy(extraResultInfoOut, RESULT_INFO_INVALID_STATE_TRANSITION, MAX_TRANSITION_RESULT_EXTRA_INFO);
	return PMTransitionError;
}

/**
 * Assuming we are the only transition running, try a transition
 */
static PrimaryMirrorModeTransitionResult
applyStepForTransitionToUninitializedMode(PrimaryMirrorModeTransitionArguments *args, TransitionState *stateInOut,
                        char extraResultInfoOut[MAX_TRANSITION_RESULT_EXTRA_INFO])
{
	assertIsTransitioning();
	return invalidMode(args);
}

/**
 * Assuming we are the only transition running, try a transition
 */
static PrimaryMirrorModeTransitionResult
applyStepForTransitionToMasterOrMirrorlessMode(PrimaryMirrorModeTransitionArguments *args, TransitionState *stateInOut,
											   char extraResultInfoOut[MAX_TRANSITION_RESULT_EXTRA_INFO])
{
	assertIsTransitioning();

	switch (*stateInOut)
	{
		case TSNone:
			elog(LOG, "TransitionToMasterOrMirrorless: initializing XLog startup");
			XLogStartupInit(); /* do this as early as possible */

			switch (pmModuleState->mode)
			{
				case PMModeUninitialized:
					*stateInOut = TSDoDatabaseStartup;
					break;
				default:
					return invalidMode(args);
			}
			return PMTransitionSuccess;

		case TSDoDatabaseStartup:
			elog(LOG, "TransitionToMasterOrMirrorless: starting database");
			copyTransitionInputParameters(args, SegmentStateNotInitialized);
			return stepDoDatabaseStartup(TSDone, stateInOut);

		default:
			return invalidState(stateInOut, extraResultInfoOut);
	}
}

/**
 * Assuming we are the only transition running, try a transition
 */
static PrimaryMirrorModeTransitionResult
applyStepForTransitionToQuiescentSegmentMode(PrimaryMirrorModeTransitionArguments *args, TransitionState *stateInOut,
											 char extraResultInfoOut[MAX_TRANSITION_RESULT_EXTRA_INFO])
{
	assertIsTransitioning();

	switch (*stateInOut)
	{
		case TSNone:
			switch (pmModuleState->mode)
			{
				case PMModeMirrorSegment:
					*stateInOut = TSDoFilerepBackendsShutdown;
					return PMTransitionSuccess;
				case PMModeUninitialized:
					*stateInOut = TSDone;
					return PMTransitionSuccess;
				default:
					return invalidMode(args);
			}

		case TSDoFilerepBackendsShutdown:
			elog(LOG, "TransitionToQuiescent: shutting down filerep backends");
			return stepDoFilerepBackendsShutdown(TSAwaitingFilerepBackendsShutdown, stateInOut);
		case TSAwaitingFilerepBackendsShutdown:
			elog(LOG, "TransitionToQuiescent: waiting for filerep backends to shutdown");
			return stepWaitForFilerepBackendsShutdown(TSDoneFilerepBackendsShutdown, stateInOut);
		case TSDoneFilerepBackendsShutdown:
			elog(LOG, "TransitionToQuiescent: completed filerep backends shutdown");
			*stateInOut = TSDoFilerepShutdown;
			return PMTransitionSuccess;

		case TSDoFilerepShutdown:
			elog(LOG, "TransitionToQuiescent: shutting down filerep");
			return stepDoFilerepShutdown(TSAwaitingFilerepShutdown, stateInOut);
		case TSAwaitingFilerepShutdown:
			elog(LOG, "TransitionToQuiescent: waiting for filerep to shutdown");
			return stepWaitForFilerepShutdown(TSDoneFilerepShutdown, stateInOut);
		case TSDoneFilerepShutdown:

			elog(LOG, "TransitionToQuiescent: completed filerep shutdown");
			*stateInOut = TSDone;
			copyTransitionInputParameters(args, SegmentStateNotInitialized);
			return PMTransitionSuccess;

		default:
			return invalidState(stateInOut, extraResultInfoOut);
	}
	/* else */
	return PMTransitionInvalidTargetMode;
}

/**
 * Assuming we are the only transition running, try a transition
 */
static PrimaryMirrorModeTransitionResult
applyStepForTransitionToPrimarySegmentMode(PrimaryMirrorModeTransitionArguments *args, TransitionState *stateInOut,
										   char extraResultInfoOut[MAX_TRANSITION_RESULT_EXTRA_INFO])
{
	assertIsTransitioning();

	switch (*stateInOut)
	{
		case TSNone:
			/* verify the transition mode */
			switch (pmModuleState->mode)
			{
				case PMModeMirrorSegment:
				case PMModePrimarySegment:
				case PMModeQuiescentSegment:
				case PMModeMirrorlessSegment:
				case PMModeUninitialized:
					*stateInOut = TSDoFilerepBackendsShutdown;
					return PMTransitionSuccess;
				default:
					return invalidMode(args);
			}

		case TSDoFilerepBackendsShutdown:
			elog(LOG, "TransitiontoPrimary: shutting down filerep backends");
			return stepDoFilerepBackendsShutdown(TSAwaitingFilerepBackendsShutdown, stateInOut);
		case TSAwaitingFilerepBackendsShutdown:
			elog(LOG, "TransitiontoPrimary: waiting for filerep backends to shutdown");
			return stepWaitForFilerepBackendsShutdown(TSDoneFilerepBackendsShutdown, stateInOut);
		case TSDoneFilerepBackendsShutdown:
			*stateInOut = TSDoFilerepShutdown;
			return PMTransitionSuccess;

		case TSDoFilerepShutdown:
			elog(LOG, "TransitiontoPrimary: shutting down filerep");
			return stepDoFilerepShutdown(TSAwaitingFilerepShutdown, stateInOut);
		case TSAwaitingFilerepShutdown:
			elog(LOG, "TransitiontoPrimary: waiting for filerep to shutdown");
			return stepWaitForFilerepShutdown(TSDoneFilerepShutdown, stateInOut);
		case TSDoneFilerepShutdown:
			elog(LOG, "TransitiontoPrimary: completed filerep to shutdown");
			*stateInOut = TSDoFilerepStartup;
			return PMTransitionSuccess;

		case TSDoFilerepStartup:

			/* do this as early as possible -- must do it before any of the new filerep processes are built */
			elog(LOG, "TransitiontoPrimary: initializing XLog Startup");
			XLogStartupInit();

			if (args->transitionToChangeTrackingDisabledMode)
			{
				copyTransitionInputParameters(args, SegmentStateChangeTrackingDisabled);
				*stateInOut = TSDoDatabaseStartup;
				return PMTransitionSuccess;
			}
			else
			{
				SegmentState_e segState;
				if (args->dataState == DataStateInResync)
				{
					if (pmModuleState->dataState == DataStateNotInitialized &&
						pmModuleState->mode != PMModeMirrorlessSegment)
					{
						Assert(pmModuleState->mode == PMModeQuiescentSegment ||
							   pmModuleState->mode == PMModeUninitialized);

						/* starting up in resync -- just go to initialization */
						/* we may have data state not initialized when going from mirrorless mode (during addmirrors) */
						segState = SegmentStateInitialization;
					}
					else
					{
						Assert(pmModuleState->mode == PMModePrimarySegment ||
							   pmModuleState->mode == PMModeMirrorlessSegment ||
							   pmModuleState->mode == PMModeMirrorSegment);

						segState = SegmentStateInResyncTransition;
					}
				}
				else
				{
					segState = SegmentStateInitialization;
				}
				copyTransitionInputParameters(args, segState);
				elog(LOG, "TransitiontoPrimary: starting filerep");
				return stepDoFilerepStartup(TSAwaitingFilerepStartup, stateInOut);
			}
		case TSAwaitingFilerepStartup:
			if (numFilerepStartupPrint == 0) {
				elog(LOG, "TransitiontoPrimary: waiting for filerep startup");
			}
			else if (numFilerepStartupPrint > PRINT_BATCH_SIZE)
			{
				numFilerepStartupPrint = -1;
			}
			++numFilerepStartupPrint;
			return stepWaitForFilerepStartup(TSDoneFilerepStartup, stateInOut, extraResultInfoOut);
		case TSDoneFilerepStartup:
			elog(LOG, "TransitiontoPrimary: completed filerep startup");
			*stateInOut = TSDoDatabaseStartup;
			return PMTransitionSuccess;

		case TSDoDatabaseStartup:
			if (!pmModuleState->haveStartedDatabase)
			{
				elog(getPrimaryMirrorModeDebugLogLevel(true),
					 "PostmasterPrimaryMirrorTransition: starting database processes");
				pmModuleState->haveStartedDatabase = true;
				StartMasterOrPrimaryPostmasterProcesses();
			}

			*stateInOut = TSAwaitingDatabaseStartup;
			return PMTransitionSuccess;

		case TSAwaitingDatabaseStartup:
			if (numDatabaseStartupPrint == 0)
			{
				elog(LOG, "TransitiontoPrimary: waiting for database startup");
			}
			else if (numDatabaseStartupPrint > PRINT_BATCH_SIZE)
			{
				numDatabaseStartupPrint = -1;
			}
		    ++numDatabaseStartupPrint;
			
			if (IsDatabaseInRunMode())
			{
				*stateInOut = TSDoneDatabaseStartup;
				pmModuleState->isDatabaseRunning = true;
			}
			if (pmModuleState->segmentState == SegmentStateFault)
			{
				/* report mirroring failure */
				strlcpy(extraResultInfoOut, RESULT_INFO_MIRRORING_FAILURE, MAX_TRANSITION_RESULT_EXTRA_INFO);

				return PMTransitionError;
			}
			return PMTransitionSuccess;

		case TSDoneDatabaseStartup:
			elog(LOG, "TransitiontoPrimary: completed database startup");
			if (args->dataState == DataStateInResync &&
				pmModuleState->dataState == DataStateInChangeTracking)
			{
				/* if filerep wants to be notified
				 * when XLOG crash recovery has completed then do that
				 */

				/*
				 * update segment states
				 */
				bool isNowInFault = false;
				acquireModuleSpinLockAndMaybeStartCriticalSection(true);
				{
					if (pmModuleState->segmentState == SegmentStateFault)
					{
						isNowInFault = true;
					}
					else
					{
						updateDataStateUnderSpinLock(DataStateInResync);
						pmModuleState->segmentState = SegmentStateInResyncTransition;
					}
				}
				releaseModuleSpinLockAndEndCriticalSectionAsNeeded();

				if (isNowInFault)
				{
					/* we are now in mirroring fault so we can't go to resync, report as mirroring failure */
					strlcpy(extraResultInfoOut, RESULT_INFO_MIRRORING_FAILURE, MAX_TRANSITION_RESULT_EXTRA_INFO);
					return PMTransitionError;
				}
				/** tell filerep processes that they can begin resync now! */
				NotifyProcessesOfFilerepStateChange();
			}

			*stateInOut = TSDone;
			return PMTransitionSuccess;

		default:
			return invalidState(stateInOut, extraResultInfoOut);
	}
}

/**
 */
static PrimaryMirrorModeTransitionResult
applyStepForTransitionToMirrorSegmentMode(PrimaryMirrorModeTransitionArguments *args, TransitionState *stateInOut,
										  char extraResultInfoOut[MAX_TRANSITION_RESULT_EXTRA_INFO])
{
	assertIsTransitioning();

	switch (*stateInOut)
	{
		case TSNone:
			switch (pmModuleState->mode)
			{
				case PMModeQuiescentSegment:
				case PMModeUninitialized:
					*stateInOut = TSDoFilerepStartup;
					return PMTransitionSuccess;
				default:
					return invalidMode(args);
			}

		case TSDoFilerepStartup:
			elog(LOG, "TransitiontoMirror: starting filerep");
			copyTransitionInputParameters(args, SegmentStateInitialization);
			return stepDoFilerepStartup(TSAwaitingFilerepStartup, stateInOut);

		case TSAwaitingFilerepStartup:
			if (numFilerepStartupPrint == 0) 
			{
				elog(LOG, "TransitiontoMirror: waiting for filerep startup");
			} 
			else if (numFilerepStartupPrint > PRINT_BATCH_SIZE)
			{
				numFilerepStartupPrint = -1;
			}
			++numFilerepStartupPrint;
		 	return stepWaitForFilerepStartup(TSDoneFilerepStartup, stateInOut, extraResultInfoOut);

		case TSDoneFilerepStartup:
			elog(LOG, "TransitiontoMirror: completed filerep startup");
			*stateInOut = TSDone;
			return PMTransitionSuccess;

		default:
			return invalidState(stateInOut, extraResultInfoOut);
	}
}

void primaryMirrorRecordSegmentStateToPostmasterLocalMemory(void)
{
	Assert(!IsUnderPostmaster);

	SegmentState_e segmentState = pmModuleState->segmentState;
	switch (segmentState)
	{
		case SegmentStateInitialization:
		case SegmentStateInResyncTransition:
		case SegmentStateInSyncTransition:
		case SegmentStateReady:
			pmLocalState.mostRecentSegmentStateWasChangeTrackingDisabled = false;
			break;

		case SegmentStateChangeTrackingDisabled:
			pmLocalState.mostRecentSegmentStateWasChangeTrackingDisabled = true;
			break;

		case SegmentStateNotInitialized:
		case SegmentStateInChangeTrackingTransition:
		case SegmentStateFault:
		case SegmentStateShutdownFilerepBackends:
		case SegmentStateShutdown:
		case SegmentStateImmediateShutdown:
			/* does not change mostRecentSegmentStateWasChangeTrackingDisabled */
			break;

		case SegmentState__EnumerationCount:
			Assert(0);
			break;
	}
}

/**
 * Should only be called by filerep code, sets whether IO is suspended.
 */
void primaryMirrorSetIOSuspended(bool ioSuspended)
{
	assertModuleInitialized();

	/* updating single small field, no need for fancy locking */
	pmModuleState->isIOSuspended = ioSuspended;
}

/**
 * Should only be called by postmaster; returns whether IO has been suspended.
 */
bool primaryMirrorIsIOSuspended(void)
{
	assertModuleInitialized();

	return pmModuleState->isIOSuspended;
}

/**
 * Get the arguments that were most recently copied to local memory.  This is ONLY valid
 *  in the postmaster and in children that are forked most recently after the transition.
 */
PrimaryMirrorModeTransitionArguments primaryMirrorGetArgumentsFromLocalMemory(void)
{
	if (!pmLocalState.haveSetMostRecentTransition)
	{
		elog(ERROR, "Request for filerep arguments from local memory made when they do not exist.");
	}
	return pmLocalState.mostRecentTransition;
}

/*
 * This changes the dbid of postmaster from child proces.
 * You should carefully use this, because if any other child
 * process has already forked with old dbid value, that process
 * will be out of sync.
 */
void
primaryMirrorSetNewDbid(int16 newdbid)
{
	Assert(IsUnderPostmaster);
	assertModuleInitialized();
	pmModuleState->newdbid = newdbid;
	SendPostmasterSignal(PMSIGNAL_SEGCONFIG_CHANGE);
}

/*
 * Counterpart of primaryMirrorSetNewDbid().  This is the receiver
 * side at postmaster, so nobody else should call this function.
 */
int16
primaryMirrorGetNewDbid(void)
{
	int16		newdbid;

	Assert(!IsUnderPostmaster);
	assertModuleInitialized();

	newdbid = pmModuleState->newdbid;
	pmModuleState->newdbid = InvalidDbid;

	return newdbid;
}

/*
 * Return true if the postmaster is a quiescent.  Optionally outMirrorMode
 * is set to the current mirror mode.
 */
bool
isQuiescentMode(PrimaryMirrorMode *outMirrorMode)
{
	PrimaryMirrorMode mirrorMode, requestedMode;

	acquireModuleSpinLockAndMaybeStartCriticalSection(false);
	{
		mirrorMode = pmModuleState->mode;
		requestedMode = pmModuleState->requestedTransition.mode;
	}
	releaseModuleSpinLockAndEndCriticalSectionAsNeeded();

	if (outMirrorMode)
		*outMirrorMode = mirrorMode;

	/*
	 * In initial quiescent mode the mirrorMode is "uninitialized."
	 */
	if (mirrorMode == PMModeQuiescentSegment ||
		(requestedMode == PMModeQuiescentSegment &&
		 mirrorMode == PMModeUninitialized))
		return true;

	return false;
}
