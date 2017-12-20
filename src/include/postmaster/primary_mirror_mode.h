/*-------------------------------------------------------------------------
 *
 * primary_mirror_mode.h
 *	  Exports from primary_mirror_mode.c.
 *
 * Portions Copyright (c) 2006-2009, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 * IDENTIFICATION
 *	    src/include/postmaster/primary_mirror_mode.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef _PRIMARY_MIRROR_MODE_H
#define _PRIMARY_MIRROR_MODE_H

#include "c.h"
#include "postgres.h"

#define MAX_TRANSITION_RESULT_EXTRA_INFO 256

/*
 * Segment states are set only when mirroring is configured (that is, when
 *    the FileRepRole is either FileRepPrimary or FileRepMirrorRole)
 *
 * Note: if the order or # of values is changed then update the gDataStateLabels in primary_mirror_mode.c
 */
typedef enum DataState_e
{
    /* State has not been set */
    DataStateNotInitialized=0,

    /* Primary and mirror are in sync */
    DataStateInSync,

    /* Primary is logging changing. Mirror is down. */
    DataStateInChangeTracking,

    /**
     * Changes are begin re-synchronized from primary to mirror.
     */
    DataStateInResync,

    /* note that these must be in sync with gDataStateLabels inside the implementation file */

    /* The number of elements in this enumeration */
    DataState__EnumerationCount
} DataState_e;

/**
 * Note: if the order or # of values is changed then update the labels in primary_mirror_mode.c
 */
typedef enum SegmentState_e {

    SegmentStateNotInitialized=0,
    /* State has not been set */

    SegmentStateInitialization,
    /*
     * FileRep, Change Tracking and Resync initialization and recovery.
     */

	SegmentStateInChangeTrackingTransition,
	/*
	 * Transition from InSync to InChangeTracking
	 */

    SegmentStateInResyncTransition,
    /*
     * Transition from Change Tracking or from no mirroring into Resync.
     * This state is used while dataState == InResync.
     */

    SegmentStateInSyncTransition,
    /*
     * Transition from Resync to Sync segment state.
     * This state is used while dataState == InResync.
     */

    SegmentStateReady,
    /* Normal operation. */

    SegmentStateChangeTrackingDisabled,
    /*
     * Change Tracking was turned off due to no disk space or
     * gprecoverseg with full copy was requested.
     * This state is used while dataState == InChangeTracking.
     */

    SegmentStateFault,
    /*
     * Fault is detected. Notify fts prober.
     * Waiting for third coordinator decision about recovery.
     */

    SegmentStateShutdownFilerepBackends,
    /* graceful shutdown of filerep processes which act as backends is in progress. */

    SegmentStateShutdown,
    /* graceful shutdown in progress. */

    SegmentStateImmediateShutdown,
    /* immediate shutdown in progress */

    /* The number of elements in this enumeration */
    SegmentState__EnumerationCount

} SegmentState_e;


typedef enum FileRepRole_e {
    FileRepRoleNotInitialized=0,
	/* Configuration has not been set */

    FileRepNoRoleConfigured,
	/* Master or Mirroring is not configured.  */

    FileRepPrimaryRole,
	/* Mirroring is configured. Segment is in the role of primary. */

    FileRepMirrorRole,
	/* Mirroring is configured. Segment is in the role of mirror. */

    /* The number of elements in this enumeration */
	FileRepRole_e_Count
} FileRepRole_e;

typedef enum FaultType_e {
    FaultTypeNotInitialized=0,
		/* Fault type has not been set */

    FaultTypeIO,
		/* IO related Fault   */

    FaultTypeDB,
		/* Database related Fault */

	FaultTypeMirror,
		/* Mirroring, filerep related Fault between primary and mirror, failover can happen to primary */

	FaultTypeNet,
		/* Network, one of the NICs used to communicate with remote segments is down */

	/* The number of elements in this enumeration */
	FaultType__EnumerationCount

} FaultType_e;

/**
 * Note: if the order or # of values is changed then update the labels in primary_mirror_mode.c
 */
typedef enum
{
	/* PMModeUninitialized: the server's mode has never been set */
	PMModeUninitialized = 0,

	/* PMModeMaster: acting as a cluster master */
	PMModeMaster,

	/* A segment without an associated mirror */
	PMModeMirrorlessSegment,

	/* PMModeQuiescentSegment: a quiescent segment cannot receive work; it can only transition to other states (like PrimarySegment or MirrorSegment) */
	PMModeQuiescentSegment,

	/* PMModePrimarySegment: a primary segment can answer queries, and can also send file system updates to a mirror */
	PMModePrimarySegment,

	/* PMModeMirrorSegment: a mirror segment cannot answer queries; it can only wait for primary-segment connections and handle them */
	PMModeMirrorSegment,

	/* note that these must be in sync with gModeLabels inside the implementation file */

    /* The number of elements in this enumeration */
	PMMode__EnumerationCount
} PrimaryMirrorMode;

/**
 * Note: if the order or # of values is changed then update the labels in primary_mirror_mode.c
 *
 * When adding a new value here, you must search primary_mirror_mode.c for occurrences of it
 *    -- for example, the call to copyTransitionParametersToLocalMemoryIfNeeded is only done on
 *       certain transition result codes and that must be checked for a new code.
 *
 */
typedef enum
{
	/* Success! */
	PMTransitionSuccess = 0,

	/* The target mode is invalid given the current mode */
	PMTransitionInvalidTargetMode,

	/* Some other transition is in progress */
	PMTransitionOtherTransitionInProgress,

	/* an input parameter to the transition was wrong */
	PMTransitionInvalidInputParameter,

	/* an error happened during transition */
	PMTransitionError,

	/**
	 * The transition was okay but is still in progress.
	 *
	 * Note that this will only be used by the postmaster process while processing the request and never
	 *   actually returned to the original caller
	 */
	PMTransitionOkayButStillInProgress,

	/* note that these must be in sync with labels inside the implementation file */

	PMTransition__EnumerationCount
} PrimaryMirrorModeTransitionResult;

#define PM_MAX_HOST_NAME_LENGTH (200)

/*
 * This structure will live in shared memory so it must not use any allocation to build it!
 */
typedef struct
{
	PrimaryMirrorMode mode;
	DataState_e dataState;

	char hostAddress[PM_MAX_HOST_NAME_LENGTH+1];
	int hostPort;
	char peerAddress[PM_MAX_HOST_NAME_LENGTH+1];

	/** The peer's replication port.  Filerep will connect to this port */
	int peerPort;

	/** The peer's postmaster port.  Filerep will connect to this port */
	int peerPostmasterPort;

    /* a unique identifer for logging, will be filled in by the call to requestTransition... */
	int transitionNumber;

    /* if true then a full resynchronization will be begun */
	bool forceFullResync;

	/** external callers will never set this to true.  It will become true if we are doing a postmaster reset
	 *   but the reset did not work.  This flag indicates that we should transition immmediately to a fault
	 *   state and set isInFaultFromPostmasterReset inside the module state.
	 */
	bool transitionToFault;

	/**
	 * external callers will never set this to true.  It will become true if we are doing a postmaster reset
	 *   and need to disable change tracking after the reset.
	 *
	 * This is used for the case where we get a postmaster reset from filerep while changeTrackingDisabled is set
	 */
	bool transitionToChangeTrackingDisabledMode;

	/* note that if more fields are added here, must be sure to update the no-op check in primary_mirror_mode.c */
} PrimaryMirrorModeTransitionArguments;

/* module initialization */
extern void primaryMirrorModeShmemInit(void);
extern Size primaryMirrorModeShmemSize(void);
extern void setInitialRequestedPrimaryMirrorMode( PrimaryMirrorMode targetMode );

/* returns PMModeUninitialized if the value could not be parsed as an initialized mode! */
extern PrimaryMirrorMode decipherPrimaryMirrorModeArgument(const char *arg);

/* state */
extern bool isPrimaryMirrorModeAFullPostmaster(bool checkTargetModeAsWell);
extern bool isInFaultFromPostmasterReset(void);
extern bool doesPrimaryMirrorModeRequireFilerepProcess(bool checkTargetModeAsWell);

/**
 * Fetch the current file replication role and states.  Returns values through the pointer arguments.
 *   Note that any of the arguments may be NULL in which case that piece of information is not returned.
 */
extern void getFileRepRoleAndState(
								   FileRepRole_e *fileRepRole,
								   SegmentState_e *segmentState,
								   DataState_e *dataState,
                                   bool *isInFilerepTransitionOut,
                                   DataState_e *transitionTargetDataStateOut);

/* functions for the postmaster/filerep interaction */
extern bool isFullResync(void);

extern void setFullResync(bool isFullResync);

extern bool isResyncRunning(void);

extern void setResyncCompleted(void);

extern bool isDatabaseRunning(void);

extern bool updateSegmentState(SegmentState_e segmentState, FaultType_e faultType);

extern bool updateDataState(DataState_e dataState);

extern int64 getChangeTrackingSessionId(void);

extern void primaryMirrorSetIOSuspended( bool ioSuspended );
extern bool primaryMirrorIsIOSuspended(void);

/* labeling enums */

extern const char * getTransitionResultLabel(PrimaryMirrorModeTransitionResult res);
extern void getPrimaryMirrorModeStatus( char *bufOut, int bufSize);
extern void getPrimaryMirrorStatusCodes(PrimaryMirrorMode *pm_mode,
										SegmentState_e *s_state,
										DataState_e *d_state,
										FaultType_e *f_type);

extern const char *getDataStateLabel(DataState_e state);
extern const char *getMirrorModeLabel(PrimaryMirrorMode mode);
extern const char *getSegmentStateLabel(SegmentState_e state);
extern const char *getFaultTypeLabel(FaultType_e faultType);

/* converting enums to chars compatible with gp_segment_configuration */
extern char getRole(PrimaryMirrorMode mode);
extern char getMode(DataState_e state);

/* transitioning */
extern PrimaryMirrorModeTransitionResult requestTransitionToPrimaryMirrorMode(PrimaryMirrorModeTransitionArguments *args,
                        char extraResultInfoOut[MAX_TRANSITION_RESULT_EXTRA_INFO] );
extern bool isPrimaryMirrorModeTransitionRequested(void);
extern void doRequestedPrimaryMirrorModeTransitions(bool isInShutdown);
extern PrimaryMirrorModeTransitionArguments *createNewTransitionArguments(void);
extern void primaryMirrorGetFilerepArguments(
				FileRepRole_e	*fileRepRoleOut,
				SegmentState_e	*segmentStateOut,
				DataState_e		*dataStateOut,
				char **primaryHostAddressOut,
				int *primaryPortOut,
				char **mirrorHostAddressOut,
				int *mirrorPortOut,
				bool *fullSyncOut);
extern void primaryMirrorHandlePostmasterReset(void);
extern void setFaultAfterReset(bool val);
extern void setTransitionToFault(void);
extern bool getTransitionToFault(void);

extern void primaryMirrorSetInSync(void);
extern void copyTransitionParametersToLocalMemory(void);

extern bool primaryMirrorCheckNICFailure(void);
extern bool primaryMirrorPostmasterResetShouldRestartPeer(void);
extern void primaryMirrorRecordSegmentStateToPostmasterLocalMemory(void);
extern PrimaryMirrorModeTransitionArguments primaryMirrorGetArgumentsFromLocalMemory(void);

extern void primaryMirrorSetNewDbid(int16 newdbid);
extern int16 primaryMirrorGetNewDbid(void);
extern bool isQuiescentMode(PrimaryMirrorMode *outMirrorMode);

#endif   /* _PRIMARY_MIRROR_MODE_H */
