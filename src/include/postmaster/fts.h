/*-------------------------------------------------------------------------
 *
 * fts.h
 *	  Interface for fault tolerance service (FTS).
 *
 * Portions Copyright (c) 2005-2010, Greenplum Inc.
 * Portions Copyright (c) 2011, EMC Corp.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/postmaster/fts.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef FTS_H
#define FTS_H

#include "cdb/cdbutil.h"

#ifdef USE_SEGWALREP
typedef struct
{
	int16 dbid;
	bool isPrimaryAlive;
	bool isMirrorAlive;
} probe_result;

typedef struct
{
	CdbComponentDatabaseInfo *segment_db_info;
	probe_result result;
	bool isScheduled;
} probe_response_per_segment;

typedef struct
{
	int count;
	probe_response_per_segment *responses;
} probe_context;

typedef struct ProbeResponse
{
	bool IsMirrorUp;
} ProbeResponse;

#endif

/*
 * ENUMS
 */

enum probe_result_e
{
	PROBE_DEAD            = 0x00,
	PROBE_ALIVE           = 0x01,
	PROBE_SEGMENT         = 0x02,
	PROBE_RESYNC_COMPLETE = 0x04,
	PROBE_FAULT_CRASH     = 0x08,
	PROBE_FAULT_MIRROR    = 0x10,
	PROBE_FAULT_NET       = 0x20,
};

#define PROBE_CHECK_FLAG(result, flag) (((result) & (flag)) == (flag))

#define PROBE_IS_ALIVE(dbInfo) \
	PROBE_CHECK_FLAG(probe_results[(dbInfo)->dbid], PROBE_ALIVE)
#define PROBE_IS_RESYNC_COMPLETE(dbInfo) \
	PROBE_CHECK_FLAG(probe_results[(dbInfo)->dbid], PROBE_RESYNC_COMPLETE)
#define PROBE_HAS_FAULT_CRASH(dbInfo) \
	PROBE_CHECK_FLAG(probe_results[(dbInfo)->dbid], PROBE_FAULT_CRASH)
#define PROBE_HAS_FAULT_MIRROR(dbInfo) \
	PROBE_CHECK_FLAG(probe_results[(dbInfo)->dbid], PROBE_FAULT_MIRROR)
#define PROBE_HAS_FAULT_NET(dbInfo) \
	PROBE_CHECK_FLAG(probe_results[(dbInfo)->dbid], PROBE_FAULT_NET)

/*
 * primary/mirror state after probing;
 * this is used to transition the segment pair to next state;
 * we ignore the case where both segments are down, as no transition is performed;
 * each segment can be:
 *    1) (U)p or (D)own
 */
enum probe_transition_e
{
	TRANS_D_D = 0x01,	/* not used for state transitions */
	TRANS_D_U = 0x02,
	TRANS_U_D = 0x04,
	TRANS_U_U = 0x08,

	TRANS_SENTINEL
};

#define IS_VALID_TRANSITION(trans) \
	(trans == TRANS_D_D || trans == TRANS_D_U || trans == TRANS_U_D || trans == TRANS_U_U)


/*
 * STRUCTURES
 */

/* prototype */
struct CdbComponentDatabaseInfo;

typedef struct
{
	int	dbid;
	int16 segindex;
	uint8 oldStatus;
	uint8 newStatus;
} FtsSegmentStatusChange;

typedef struct
{
	CdbComponentDatabaseInfo *primary;
	CdbComponentDatabaseInfo *mirror;
	uint32 stateNew;
	uint32 statePrimary;
	uint32 stateMirror;
} FtsSegmentPairState;

/*
 * FTS process interface
 */
extern int ftsprobe_start(void);

/*
 * Interface for probing segments
 */
extern void FtsProbeSegments(CdbComponentDatabases *dbs, uint8 *scan_status);

#ifdef USE_SEGWALREP
extern void FtsWalRepProbeSegments(probe_context *context);
#endif

/*
 * Interface for segment state checking
 */
extern bool FtsIsSegmentAlive(CdbComponentDatabaseInfo *segInfo);
extern CdbComponentDatabaseInfo *FtsGetPeerSegment(int content, int dbid);
extern void FtsMarkSegmentsInSync(CdbComponentDatabaseInfo *primary, CdbComponentDatabaseInfo *mirror);
extern void FtsDumpChanges(FtsSegmentStatusChange *changes, int changeEntries);

/*
 * Interface for checking if FTS is active
 */
extern bool FtsIsActive(void);

#ifdef USE_SEGWALREP
/*
 * Interface for WALREP specific checking
 */
extern void HandleFtsWalRepProbe(void);
#endif

/*
 * Interface for FireRep-specific segment state machine and transitions
 */
extern uint32 FtsGetPairStateFilerep(CdbComponentDatabaseInfo *primary, CdbComponentDatabaseInfo *mirror);
extern uint32 FtsTransitionFilerep(uint32 stateOld, uint32 trans);
extern void FtsResolveStateFilerep(FtsSegmentPairState *pairState);

extern void FtsPreprocessProbeResultsFilerep(CdbComponentDatabases *dbs, uint8 *probe_results);
extern void FtsFailoverFilerep(FtsSegmentStatusChange *changes, int changeCount);


/*
 * Interface for requesting master to shut down
 */
extern void FtsRequestPostmasterShutdown(CdbComponentDatabaseInfo *primary, CdbComponentDatabaseInfo *mirror);
extern bool FtsMasterShutdownRequested(void);
extern void FtsRequestMasterShutdown(void);

/*
 * If master has requested FTS to shutdown.
 */
#ifdef FAULT_INJECTOR
extern bool IsFtsShudownRequested(void);
#endif
#endif   /* FTS_H */

