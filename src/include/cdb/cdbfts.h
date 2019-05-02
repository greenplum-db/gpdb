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
#define FTS_STATUS_DOWN				(1<<0)

#define FTS_STATUS_TEST(status, flag) (((status) & (flag)) ? true : false)
#define FTS_STATUS_IS_DOWN(status) FTS_STATUS_TEST((status), FTS_STATUS_DOWN)

#define FTS_STATUS_SET(status, flag) ((status) |= (flag))
#define FTS_STATUS_SET_DOWN(status) FTS_STATUS_SET((status), FTS_STATUS_DOWN)

#define FTS_STATUS_RESET(status, flag) ((status) &= ~(flag))
#define FTS_STATUS_SET_UP(status) FTS_STATUS_RESET((status), FTS_STATUS_DOWN)

typedef struct FtsProbeInfo
{
	volatile uint8		status_version;
	volatile uint8		status[FTS_MAX_DBS];
	volatile slock_t	lock;
	volatile int32		start_count;
	volatile int32		done_count;
} FtsProbeInfo;

#define FTS_MAX_TRANSIENT_STATE 100

typedef struct FtsControlBlock
{
	LWLockId		ControlLock;
	FtsProbeInfo	fts_probe_info;
	volatile bool	pm_launch_walreceiver;
}	FtsControlBlock;

extern volatile FtsProbeInfo *ftsProbeInfo;

extern int	FtsShmemSize(void);
extern void FtsShmemInit(void);

extern bool FtsIsSegmentDown(CdbComponentDatabaseInfo *dBInfo);
extern bool FtsTestSegmentDBIsDown(SegmentDatabaseDescriptor **, int);

extern bool verifyFtsSyncCount(void);
extern void ftsLock(void);
extern void ftsUnlock(void);
extern void FtsNotifyProber(void);
extern uint8 getFtsVersion(void);
#endif   /* CDBFTS_H */
