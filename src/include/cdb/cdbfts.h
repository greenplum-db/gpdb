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

struct FtsSegDBState
{
	bool	valid;
	bool	primary;
	int16	id;
};

#define FTS_MAX_DBS (128 * 1024)

#define FTS_STATUS_ALIVE				(1<<0)
#define FTS_STATUS_PRIMARY				(1<<1)
#define FTS_STATUS_DEFINEDPRIMARY		(1<<2)
#define FTS_STATUS_SYNCHRONIZED			(1<<3)
#define FTS_STATUS_CHANGELOGGING		(1<<4)

#define FTS_STATUS_TEST(dbid, status, flag) (((status)[(dbid)] & (flag)) ? true : false)
#define FTS_STATUS_ISALIVE(dbid, status) FTS_STATUS_TEST((dbid), (status), FTS_STATUS_ALIVE)
#define FTS_STATUS_ISPRIMARY(dbid, status) FTS_STATUS_TEST((dbid), (status), FTS_STATUS_PRIMARY)
#define FTS_STATUS_ISDEFINEDPRIMARY(dbid, status) FTS_STATUS_TEST((dbid), (status), FTS_STATUS_DEFINEDPRIMARY)
#define FTS_STATUS_IS_SYNCED(dbid, status) FTS_STATUS_TEST((dbid), (status), FTS_STATUS_SYNCHRONIZED)
#define FTS_STATUS_IS_CHANGELOGGING(dbid, status) FTS_STATUS_TEST((dbid), (status), FTS_STATUS_CHANGELOGGING)

typedef struct FtsProbeInfo
{
	volatile uint32		fts_probePid;
	volatile uint64		fts_probeScanRequested;
	volatile uint64		fts_statusVersion;
	volatile bool		fts_pauseProbes;
	volatile bool		fts_discardResults;
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
