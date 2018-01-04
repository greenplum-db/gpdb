/*-------------------------------------------------------------------------
 * cdbfts.h
 *
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBFTS_H
#define CDBFTS_H

#include "storage/lwlock.h"
#include "cdb/cdbconn.h"
#include "utils/guc.h"
#include "cdb/cdbgang.h"

#define FTS_MAX_DBS (128 * 1024)

/*
 * There used to many more states here but currently dispatch is only checking
 * if segment is UP or not. So just have that, when needed for other states
 * this can be extended.
 */
#define FTS_STATUS_UP				(1<<0)

#define FTS_STATUS_TEST(status, flag) (((status) & (flag)) ? true : false)
#define FTS_STATUS_IS_UP(status) FTS_STATUS_TEST((status), FTS_STATUS_UP)

#define FTS_STATUS_SET(status, flag) ((status) |= (flag))
#define FTS_STATUS_SET_UP(status) FTS_STATUS_SET((status), FTS_STATUS_UP)

#define FTS_STATUS_RESET(status, flag) ((status) &= ~(flag))
#define FTS_STATUS_SET_DOWN(status) FTS_STATUS_RESET((status), FTS_STATUS_UP)

typedef struct FtsProbeInfo
{
	volatile uint32		fts_probePid;
	volatile uint64		fts_probeScanRequested;
	volatile uint64		fts_statusVersion;
	volatile bool		fts_pauseProbes;
	volatile bool		fts_discardResults;
	volatile bool       fts_status_initialized;
	volatile uint8		fts_status[FTS_MAX_DBS];
} FtsProbeInfo;

#define FTS_MAX_TRANSIENT_STATE 100

typedef struct FtsControlBlock
{
	bool		ftsEnabled;
	bool		ftsShutdownMaster;

	LWLockId	ControlLock;

	bool		ftsReadOnlyFlag;
	bool		ftsAdminRequestedRO;

	FtsProbeInfo fts_probe_info;

}	FtsControlBlock;

extern volatile bool *ftsEnabled;

extern FtsProbeInfo *ftsProbeInfo;

#define isFTSEnabled() (ftsEnabled != NULL && *ftsEnabled)

#define disableFTS() (*ftsEnabled = false)

extern int	FtsShmemSize(void);
extern void FtsShmemInit(void);

extern bool FtsTestConnection(CdbComponentDatabaseInfo *db_to_test, bool full_scan);
extern void FtsReConfigureMPP(bool create_new_gangs);
extern void FtsHandleNetFailure(SegmentDatabaseDescriptor **, int);
extern bool FtsTestSegmentDBIsDown(SegmentDatabaseDescriptor *, int);

extern bool verifyFtsSyncCount(void);
extern void FtsCondSetTxnReadOnly(bool *);
extern void ftsLock(void);
extern void ftsUnlock(void);

extern void FtsNotifyProber(void);

extern bool isFtsReadOnlySet(void);
extern uint64 getFtsVersion(void);

/* markStandbyStatus forces persistent state change ?! */
#define markStandbyStatus(dbid, state) (markSegDBPersistentState((dbid), (state)))

#endif   /* CDBFTS_H */
