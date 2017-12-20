/*-------------------------------------------------------------------------
 *
 * xlog.c
 *		PostgreSQL transaction log manager
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/backend/access/transam/xlog.c,v 1.345 2009/06/26 20:29:04 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <ctype.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "access/clog.h"
#include "access/multixact.h"
#include "access/distributedlog.h"
#include "access/subtrans.h"
#include "access/transam.h"
#include "access/tuptoaster.h"
#include "access/twophase.h"
#include "access/xact.h"
#include "access/xlog_internal.h"
#include "access/xlogdefs.h"
#include "access/xlogutils.h"
#include "catalog/catalog.h"
#include "catalog/catversion.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_control.h"
#include "catalog/pg_type.h"
#include "catalog/pg_database.h"
#include "catalog/pg_tablespace.h"
#include "executor/spi.h"
#include "funcapi.h"
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/bgwriter.h"
#include "postmaster/postmaster.h"
#include "storage/bufpage.h"
#include "storage/bufmgr.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/pmsignal.h"
#include "storage/procarray.h"
#include "storage/smgr.h"
#include "storage/spin.h"
#include "utils/builtins.h"
#include "utils/nabstime.h"
#include "utils/faultinjector.h"
#include "utils/flatfiles.h"
#include "utils/guc.h"
#include "utils/ps_status.h"
#include "pg_trace.h"
#include "utils/catcache.h"
#include "utils/memutils.h"
#include "utils/syscache.h"
#include "utils/pg_crc.h"
#include "utils/ps_status.h"
#include "replication/walreceiver.h"
#include "replication/walsender.h"
#include "storage/backendid.h"
#include "storage/sinvaladt.h"

#include "cdb/cdbtm.h"
#include "cdb/cdbvars.h"
#include "utils/resscheduler.h"
#include "utils/snapmgr.h"

extern uint32 bootstrap_data_checksum_version;

/* File path names (all relative to $PGDATA) */
#define RECOVERY_COMMAND_FILE	"recovery.conf"
#define RECOVERY_COMMAND_DONE	"recovery.done"
#define PROMOTE_SIGNAL_FILE "promote"


/* User-settable parameters */
int			CheckPointSegments = 3;
int			XLOGbuffers = 8;
int			XLogArchiveTimeout = 0;
bool		XLogArchiveMode = false;
char	   *XLogArchiveCommand = NULL;
bool		fullPageWrites = true;
char   *wal_consistency_checking_string = NULL;
bool   *wal_consistency_checking = NULL;
bool		log_checkpoints = false;
int			sync_method = DEFAULT_SYNC_METHOD;

#ifdef WAL_DEBUG
bool		XLOG_DEBUG = false;
#endif

/*
 * XLOGfileslop is the maximum number of preallocated future XLOG segments.
 * When we are done with an old XLOG segment file, we will recycle it as a
 * future XLOG segment as long as there aren't already XLOGfileslop future
 * segments; else we'll delete it.  This could be made a separate GUC
 * variable, but at present I think it's sufficient to hardwire it as
 * 2*CheckPointSegments+1.	Under normal conditions, a checkpoint will free
 * no more than 2*CheckPointSegments log segments, and we want to recycle all
 * of them; the +1 allows boundary cases to happen without wasting a
 * delete/create-segment cycle.
 */
#define XLOGfileslop	(2*CheckPointSegments + 1)

bool am_startup = false;

/*
 * GUC support
 */
const struct config_enum_entry sync_method_options[] = {
	{"fsync", SYNC_METHOD_FSYNC, false},
#ifdef HAVE_FSYNC_WRITETHROUGH
	{"fsync_writethrough", SYNC_METHOD_FSYNC_WRITETHROUGH, false},
#endif
#ifdef HAVE_FDATASYNC
	{"fdatasync", SYNC_METHOD_FDATASYNC, false},
#endif
#ifdef OPEN_SYNC_FLAG
	{"open_sync", SYNC_METHOD_OPEN, false},
#endif
#ifdef OPEN_DATASYNC_FLAG
	{"open_datasync", SYNC_METHOD_OPEN_DSYNC, false},
#endif
	{NULL, 0, false}
};

/*
 * Statistics for current checkpoint are collected in this global struct.
 * Because only the background writer or a stand-alone backend can perform
 * checkpoints, this will be unused in normal backends.
 */
CheckpointStatsData CheckpointStats;

/*
 * ThisTimeLineID will be same in all backends --- it identifies current
 * WAL timeline for the database system.
 */
TimeLineID	ThisTimeLineID = 0;

/*
 * Are we doing recovery from XLOG?
 *
 * This is only ever true in the startup process; it should be read as meaning
 * "this process is replaying WAL records", rather than "the system is in
 * recovery mode".  It should be examined primarily by functions that need
 * to act differently when called from a WAL redo function (e.g., to skip WAL
 * logging).  To check whether the system is in recovery regardless of which
 * process you're running in, use RecoveryInProgress().
 */
bool		InRecovery = false;

/*
 * Local copy of SharedRecoveryInProgress variable. True actually means "not
 * known, need to check the shared state".
 */
static bool LocalRecoveryInProgress = true;

/*
 * Local state for XLogInsertAllowed():
 *		1: unconditionally allowed to insert XLOG
 *		0: unconditionally not allowed to insert XLOG
 *		-1: must check RecoveryInProgress(); disallow until it is false
 * Most processes start with -1 and transition to 1 after seeing that recovery
 * is not in progress.  But we can also force the value for special cases.
 * The coding in XLogInsertAllowed() depends on the first two of these states
 * being numerically the same as bool true and false.
 */
static int	LocalXLogInsertAllowed = -1;

/* Are we recovering using offline XLOG archives? */
static bool InArchiveRecovery = false;

/* Was the last xlog file restored from archive, or local? */
static bool restoredFromArchive = false;

/* options taken from recovery.conf */
#ifdef NOT_USED
static char *recoveryRestoreCommand = NULL;
#endif
static char *recoveryEndCommand = NULL;
static bool recoveryTarget = false;
static bool recoveryTargetExact = false;
static bool recoveryTargetInclusive = true;
static TransactionId recoveryTargetXid;
static TimestampTz recoveryTargetTime;
static TimestampTz recoveryLastXTime = 0;

static char *replay_image_masked = NULL;
static char *master_image_masked = NULL;

/* options taken from recovery.conf for XLOG streaming */
static bool StandbyModeRequested = false;
static char *PrimaryConnInfo = NULL;

/* are we currently in standby mode? */
bool StandbyMode = false;

/* if recoveryStopsHere returns true, it saves actual stop xid/time here */
static TransactionId recoveryStopXid;
static TimestampTz recoveryStopTime;
static bool recoveryStopAfter;

/*
 * During normal operation, the only timeline we care about is ThisTimeLineID.
 * During recovery, however, things are more complicated.  To simplify life
 * for rmgr code, we keep ThisTimeLineID set to the "current" timeline as we
 * scan through the WAL history (that is, it is the line that was active when
 * the currently-scanned WAL record was generated).  We also need these
 * timeline values:
 *
 * recoveryTargetTLI: the desired timeline that we want to end in.
 *
 * expectedTLIs: an integer list of recoveryTargetTLI and the TLIs of
 * its known parents, newest first (so recoveryTargetTLI is always the
 * first list member).	Only these TLIs are expected to be seen in the WAL
 * segments we read, and indeed only these TLIs will be considered as
 * candidate WAL files to open at all.
 *
 * curFileTLI: the TLI appearing in the name of the current input WAL file.
 * (This is not necessarily the same as ThisTimeLineID, because we could
 * be scanning data that was copied from an ancestor timeline when the current
 * file was created.)  During a sequential scan we do not allow this value
 * to decrease.
 */
static TimeLineID recoveryTargetTLI;
List *expectedTLIs;
static TimeLineID curFileTLI;

/*
 * ProcLastRecPtr points to the start of the last XLOG record inserted by the
 * current backend.  It is updated for all inserts.  XactLastRecEnd points to
 * end+1 of the last record, and is reset when we end a top-level transaction,
 * or start a new one; so it can be used to tell if the current transaction has
 * created any XLOG records.
 */
static XLogRecPtr ProcLastRecPtr = {0, 0};

XLogRecPtr	XactLastRecEnd = {0, 0};

static uint32 ProcLastRecTotalLen = 0;

static uint32 ProcLastRecDataLen = 0;

static XLogRecPtr InvalidXLogRecPtr = {0, 0};

/*
 * RedoRecPtr is this backend's local copy of the REDO record pointer
 * (which is almost but not quite the same as a pointer to the most recent
 * CHECKPOINT record).	We update this from the shared-memory copy,
 * XLogCtl->Insert.RedoRecPtr, whenever we can safely do so (ie, when we
 * hold the Insert lock).  See XLogInsert for details.	We are also allowed
 * to update from XLogCtl->Insert.RedoRecPtr if we hold the info_lck;
 * see GetRedoRecPtr.  A freshly spawned backend obtains the value during
 * InitXLOGAccess.
 */
static XLogRecPtr RedoRecPtr;

/*
 * RedoStartLSN points to the checkpoint's REDO location which is specified
 * in a backup label file, backup history file or control file. In standby
 * mode, XLOG streaming usually starts from the position where an invalid
 * record was found. But if we fail to read even the initial checkpoint
 * record, we use the REDO location instead of the checkpoint location as
 * the start position of XLOG streaming. Otherwise we would have to jump
 * backwards to the REDO location after reading the checkpoint record,
 * because the REDO record can precede the checkpoint record.
 */
static XLogRecPtr RedoStartLSN = {0, 0};

/*----------
 * Shared-memory data structures for XLOG control
 *
 * LogwrtRqst indicates a byte position that we need to write and/or fsync
 * the log up to (all records before that point must be written or fsynced).
 * LogwrtResult indicates the byte positions we have already written/fsynced.
 * These structs are identical but are declared separately to indicate their
 * slightly different functions.
 *
 * We do a lot of pushups to minimize the amount of access to lockable
 * shared memory values.  There are actually three shared-memory copies of
 * LogwrtResult, plus one unshared copy in each backend.  Here's how it works:
 *		XLogCtl->LogwrtResult is protected by info_lck
 *		XLogCtl->Write.LogwrtResult is protected by WALWriteLock
 *		XLogCtl->Insert.LogwrtResult is protected by WALInsertLock
 * One must hold the associated lock to read or write any of these, but
 * of course no lock is needed to read/write the unshared LogwrtResult.
 *
 * XLogCtl->LogwrtResult and XLogCtl->Write.LogwrtResult are both "always
 * right", since both are updated by a write or flush operation before
 * it releases WALWriteLock.  The point of keeping XLogCtl->Write.LogwrtResult
 * is that it can be examined/modified by code that already holds WALWriteLock
 * without needing to grab info_lck as well.
 *
 * XLogCtl->Insert.LogwrtResult may lag behind the reality of the other two,
 * but is updated when convenient.	Again, it exists for the convenience of
 * code that is already holding WALInsertLock but not the other locks.
 *
 * The unshared LogwrtResult may lag behind any or all of these, and again
 * is updated when convenient.
 *
 * The request bookkeeping is simpler: there is a shared XLogCtl->LogwrtRqst
 * (protected by info_lck), but we don't need to cache any copies of it.
 *
 * Note that this all works because the request and result positions can only
 * advance forward, never back up, and so we can easily determine which of two
 * values is "more up to date".
 *
 * info_lck is only held long enough to read/update the protected variables,
 * so it's a plain spinlock.  The other locks are held longer (potentially
 * over I/O operations), so we use LWLocks for them.  These locks are:
 *
 * WALInsertLock: must be held to insert a record into the WAL buffers.
 *
 * WALWriteLock: must be held to write WAL buffers to disk (XLogWrite or
 * XLogFlush).
 *
 * ControlFileLock: must be held to read/update control file or create
 * new log file.
 *
 * CheckpointLock: must be held to do a checkpoint or restartpoint (ensures
 * only one checkpointer at a time; currently, with all checkpoints done by
 * the bgwriter, this is just pro forma).
 *
 *----------
 */

typedef struct XLogwrtRqst
{
	XLogRecPtr	Write;			/* last byte + 1 to write out */
	XLogRecPtr	Flush;			/* last byte + 1 to flush */
} XLogwrtRqst;

typedef struct XLogwrtResult
{
	XLogRecPtr	Write;			/* last byte + 1 written out */
	XLogRecPtr	Flush;			/* last byte + 1 flushed */
} XLogwrtResult;

/*
 * Shared state data for XLogInsert.
 */
typedef struct XLogCtlInsert
{
	XLogwrtResult LogwrtResult; /* a recent value of LogwrtResult */
	XLogRecPtr	PrevRecord;		/* start of previously-inserted record */
	int			curridx;		/* current block index in cache */
	XLogPageHeader currpage;	/* points to header of block in cache */
	char	   *currpos;		/* current insertion point in cache */
	XLogRecPtr	RedoRecPtr;		/* current redo point for insertions */
	bool		forcePageWrites;	/* forcing full-page writes for PITR? */

	/*
	 * exclusiveBackup is true if a backup started with pg_start_backup() is
	 * in progress, and nonExclusiveBackups is a counter indicating the number
	 * of streaming base backups currently in progress. forcePageWrites is set
	 * to true when either of these is non-zero. lastBackupStart is the latest
	 * checkpoint redo location used as a starting point for an online backup.
	 */
	bool		exclusiveBackup;
	int			nonExclusiveBackups;
	XLogRecPtr	lastBackupStart;
} XLogCtlInsert;

/*
 * Shared state data for XLogWrite/XLogFlush.
 */
typedef struct XLogCtlWrite
{
	XLogwrtResult LogwrtResult; /* current value of LogwrtResult */
	int			curridx;		/* cache index of next block to write */
	pg_time_t	lastSegSwitchTime;		/* time of last xlog segment switch */
} XLogCtlWrite;

/*
 * Total shared-memory state for XLOG.
 */
typedef struct XLogCtlData
{
	/* Protected by WALInsertLock: */
	XLogCtlInsert Insert;

	/* Protected by info_lck: */
	XLogwrtRqst LogwrtRqst;
	XLogwrtResult LogwrtResult;
	uint32		ckptXidEpoch;	/* nextXID & epoch of latest checkpoint */
	TransactionId ckptXid;
	XLogRecPtr	asyncCommitLSN; /* LSN of newest async commit */
	uint32		lastRemovedLog; /* latest removed/recycled XLOG segment */
	uint32		lastRemovedSeg;

	/* Protected by WALWriteLock: */
	XLogCtlWrite Write;

	/*
	 * These values do not change after startup, although the pointed-to pages
	 * and xlblocks values certainly do.  Permission to read/write the pages
	 * and xlblocks values depends on WALInsertLock and WALWriteLock.
	 */
	char	   *pages;			/* buffers for unwritten XLOG pages */
	XLogRecPtr *xlblocks;		/* 1st byte ptr-s + XLOG_BLCKSZ */
	int			XLogCacheBlck;	/* highest allocated xlog buffer index */
	TimeLineID	ThisTimeLineID;

	/*
	 * SharedRecoveryInProgress indicates if we're still in crash or archive
	 * recovery.  Protected by info_lck.
	 */
	bool		SharedRecoveryInProgress;

	/*
	 * recoveryWakeupLatch is used to wake up the startup process to continue
	 * WAL replay, if it is waiting for WAL to arrive or failover trigger file
	 * to appear.
	 */
	Latch		recoveryWakeupLatch;

	/*
	 * the standby's dbid when it runs.  Used in mmxlog to emit standby filepath.
	 * Protected by info_lck
	 */
	int16		standbyDbid;

	/*
	 * During recovery, we keep a copy of the latest checkpoint record here.
	 * Used by the background writer when it wants to create a restartpoint.
	 *
	 * Protected by info_lck.
	 */
	XLogRecPtr	lastCheckPointRecPtr;
	CheckPoint	lastCheckPoint;

	/*
	 * Save the location of the last checkpoint record to enable supressing
	 * unnecessary checkpoint records -- when no new xlog has been written
	 * since the last one.
	 */
	bool 		haveLastCheckpointLoc;
	XLogRecPtr	lastCheckpointLoc;
	XLogRecPtr	lastCheckpointEndLoc;

	/*
	 * lastReplayedEndRecPtr points to end+1 of the last record successfully
	 * replayed. When we're currently replaying a record, ie. in a redo
	 * function, replayEndRecPtr points to the end+1 of the record being
	 * replayed, otherwise it's equal to lastReplayedEndRecPtr.
	 */
	XLogRecPtr	lastReplayedEndRecPtr;
	XLogRecPtr	replayEndRecPtr;

	slock_t		info_lck;		/* locks shared variables shown above */

	/* current effective recovery target timeline */
	TimeLineID	RecoveryTargetTLI;

	/*
	 * timestamp of when we started replaying the current chunk of WAL data,
	 * only relevant for replication or archive recovery
	 */
	TimestampTz currentChunkStartTime;
} XLogCtlData;

static XLogCtlData *XLogCtl = NULL;

/*
 * We maintain an image of pg_control in shared memory.
 */
static ControlFileData *ControlFile = NULL;

typedef struct ControlFileWatch
{
	bool		watcherInitialized;
	XLogRecPtr	current_checkPointLoc;		/* current last check point record ptr */
	XLogRecPtr	current_prevCheckPointLoc;  /* current previous check point record ptr */
	XLogRecPtr	current_checkPointCopy_redo;
								/* current checkpointCopy value for
								 * next RecPtr available when we began to
								 * create CheckPoint (i.e. REDO start point) */

} ControlFileWatch;


/*
 * We keep the watcher in shared memory.
 */
static ControlFileWatch *ControlFileWatcher = NULL;

/*
 * Macros for managing XLogInsert state.  In most cases, the calling routine
 * has local copies of XLogCtl->Insert and/or XLogCtl->Insert->curridx,
 * so these are passed as parameters instead of being fetched via XLogCtl.
 */

/* Free space remaining in the current xlog page buffer */
#define INSERT_FREESPACE(Insert)  \
	(XLOG_BLCKSZ - ((Insert)->currpos - (char *) (Insert)->currpage))

/* Construct XLogRecPtr value for current insertion point */
#define INSERT_RECPTR(recptr,Insert,curridx)  \
	( \
	  (recptr).xlogid = XLogCtl->xlblocks[curridx].xlogid, \
	  (recptr).xrecoff = \
		XLogCtl->xlblocks[curridx].xrecoff - INSERT_FREESPACE(Insert) \
	)

#define PrevBufIdx(idx)		\
		(((idx) == 0) ? XLogCtl->XLogCacheBlck : ((idx) - 1))

#define NextBufIdx(idx)		\
		(((idx) == XLogCtl->XLogCacheBlck) ? 0 : ((idx) + 1))

/*
 * Private, possibly out-of-date copy of shared LogwrtResult.
 * See discussion above.
 */
static XLogwrtResult LogwrtResult = {{0, 0}, {0, 0}};

/*
 * Codes indicating where we got a WAL file from during recovery, or where
 * to attempt to get one.  These are chosen so that they can be OR'd together
 * in a bitmask state variable.
 */
#define XLOG_FROM_ARCHIVE		(1<<0)	/* Restored using restore_command */
#define XLOG_FROM_PG_XLOG		(1<<1)	/* Existing file in pg_xlog */
#define XLOG_FROM_STREAM		(1<<2)	/* Streamed from master */

/*
 * openLogFile is -1 or a kernel FD for an open log file segment.
 * When it's open, openLogOff is the current seek offset in the file.
 * openLogId/openLogSeg identify the segment.  These variables are only
 * used to write the XLOG, and so will normally refer to the active segment.
 */
static int	openLogFile = -1;
static uint32 openLogId = 0;
static uint32 openLogSeg = 0;
static uint32 openLogOff = 0;

/*
 * These variables are used similarly to the ones above, but for reading
 * the XLOG.  Note, however, that readOff generally represents the offset
 * of the page just read, not the seek position of the FD itself, which
 * will be just past that page.readLen indicates how much of the current
 * page has been read into readBuf, and readSource indicates where we got
 * the currently open file from.
 */
static int	readFile = -1;
static uint32 readId = 0;
static uint32 readSeg = 0;
static uint32 readOff = 0;
static uint32 readLen = 0;
static int	readSource = 0;		/* XLOG_FROM_* code */

/*
 * Keeps track of which sources we've tried to read the current WAL
 * record from and failed.
 */
static int	failedSources = 0;	/* OR of XLOG_FROM_* codes */

/*
 * These variables track when we last obtained some WAL data to process,
 * and where we got it from.  (XLogReceiptSource is initially the same as
 * readSource, but readSource gets reset to zero when we don't have data
 * to process right now.)
 */
static TimestampTz XLogReceiptTime = 0;
static int	XLogReceiptSource = 0;		/* XLOG_FROM_* code */

/* Buffer for currently read page (XLOG_BLCKSZ bytes) */
static char *readBuf = NULL;

/* Buffer for current ReadRecord result (expandable) */
static char *readRecordBuf = NULL;
static uint32 readRecordBufSize = 0;

/* State information for XLOG reading */
static XLogRecPtr ReadRecPtr;	/* start of last record read */
static XLogRecPtr EndRecPtr;	/* end+1 of last record read */
static XLogRecord *nextRecord = NULL;
static TimeLineID lastPageTLI = 0;
static TimeLineID lastSegmentTLI = 0;

static XLogRecPtr minRecoveryPoint;		/* local copy of
										 * ControlFile->minRecoveryPoint */
static bool updateMinRecoveryPoint = true;

static bool InRedo = false;

/*
 * Flags set by interrupt handlers for later service in the redo loop.
 */
static volatile sig_atomic_t got_SIGHUP = false;
static volatile sig_atomic_t shutdown_requested = false;

/*
 * Flag set when executing a restore command, to tell SIGTERM signal handler
 * that it's safe to just proc_exit.
 */
static volatile sig_atomic_t in_restore_command = false;


static void XLogArchiveNotify(const char *xlog);
static void XLogArchiveNotifySeg(uint32 log, uint32 seg);
static bool XLogArchiveCheckDone(const char *xlog);
static bool XLogArchiveIsBusy(const char *xlog);
static void XLogArchiveCleanup(const char *xlog);
static void exitArchiveRecovery(TimeLineID endTLI,
					uint32 endLogId, uint32 endLogSeg);
static bool recoveryStopsHere(XLogRecord *record, bool *includeThis);
static void LocalSetXLogInsertAllowed(void);
static void CheckPointGuts(XLogRecPtr checkPointRedo, int flags);

static bool XLogCheckBuffer(XLogRecData *rdata, bool holdsExclusiveLock,
							bool wal_check_consistency_enabled,
							XLogRecPtr *lsn, BkpBlock *bkpb);
static void RestoreBackupBlockContents(XLogRecPtr lsn, BkpBlock bkpb,
				char *blk, bool get_cleanup_lock, bool keep_buffer);

static bool AdvanceXLInsertBuffer(bool new_segment);
static void XLogWrite(XLogwrtRqst WriteRqst, bool flexible, bool xlog_switch);
static bool InstallXLogFileSegment(uint32 *log, uint32 *seg, char *tmppath,
					   bool find_free, int *max_advance,
					   bool use_lock);
static void XLogFileClose(void);
static bool XLogPageRead(XLogRecPtr *RecPtr, int emode, bool fetching_ckpt,
			 bool randAccess);

#ifdef NOT_USED
static bool RestoreArchivedFile(char *path, const char *xlogfname,
					const char *recovername, off_t expectedSize);
static void ExecuteRecoveryEndCommand(void);
#endif
static void PreallocXlogFiles(XLogRecPtr endptr);
static void UpdateLastRemovedPtr(char *filename);
static void ValidateXLOGDirectoryStructure(void);
static void CleanupBackupHistory(void);
static void UpdateMinRecoveryPoint(XLogRecPtr lsn, bool force);
#ifdef NOT_USED
static XLogRecord *ReadRecord(XLogRecPtr *RecPtr, int emode);
#endif
static XLogRecord *ReadCheckpointRecord(XLogRecPtr RecPtr, int whichChkpt);
static bool ValidXLOGHeader(XLogPageHeader hdr, int emode, bool segmentonly);

typedef struct CheckpointExtendedRecord
{
	TMGXACT_CHECKPOINT	*dtxCheckpoint;
	uint32				dtxCheckpointLen;
	prepared_transaction_agg_state  *ptas;
} CheckpointExtendedRecord;

static void UnpackCheckPointRecord(XLogRecord *record,
								   CheckpointExtendedRecord *ckptExtended);
static bool existsTimeLineHistory(TimeLineID probeTLI);
static TimeLineID findNewestTimeLine(TimeLineID startTLI);
static void writeTimeLineHistory(TimeLineID newTLI, TimeLineID parentTLI,
					 TimeLineID endTLI,
					 uint32 endLogId, uint32 endLogSeg);
static void ControlFileWatcherSaveInitial(void);
static void ControlFileWatcherCheckForChange(void);
static bool XLogGetWriteAndFlushedLoc(XLogRecPtr *writeLoc, XLogRecPtr *flushedLoc);
static XLogRecPtr XLogInsert_Internal(RmgrId rmid, uint8 info, XLogRecData *rdata, TransactionId headerXid);
static void WriteControlFile(void);
static void ReadControlFile(void);

static char *str_time(pg_time_t tnow);

static void xlog_outrec(StringInfo buf, XLogRecord *record);
static void pg_start_backup_callback(int code, Datum arg);
static bool read_backup_label(XLogRecPtr *checkPointLoc, bool *backupEndRequired);
static void rm_redo_error_callback(void *arg);
static int	get_sync_bit(int method);

/* New functions added for WAL replication */
static void SetCurrentChunkStartTime(TimestampTz xtime);
static int XLogFileReadAnyTLI(uint32 log, uint32 seg, int emode, int sources);
static void XLogProcessCheckpointRecord(XLogRecord *rec, XLogRecPtr loc);

typedef struct RedoErrorCallBack
{
	XLogRecPtr	location;

	XLogRecord 	*record;
} RedoErrorCallBack;

void HandleStartupProcInterrupts(void);
static bool CheckForStandbyTrigger(void);

static void GetXLogCleanUpTo(XLogRecPtr recptr, uint32 *_logId, uint32 *_logSeg);
static void checkXLogConsistency(XLogRecord *record, XLogRecPtr EndRecPtr);

static char *XLogContiguousCopy(
	XLogRecord 		*record,

	XLogRecData 	*rdata)
{
	XLogRecData *rdt;
	int32 len;
	char *buffer;

	rdt = rdata;
	len = SizeOfXLogRecord;
	while (rdt != NULL)
	{
		if (rdt->data != NULL)
		{
			len += rdt->len;
		}
		rdt = rdt->next;
	}

	buffer = (char*)palloc(len);

	memcpy(buffer, record, SizeOfXLogRecord);
	rdt = rdata;
	len = SizeOfXLogRecord;
	while (rdt != NULL)
	{
		if (rdt->data != NULL)
		{
			memcpy(&buffer[len], rdt->data, rdt->len);
			len += rdt->len;
		}
		rdt = rdt->next;
	}

	return buffer;
}

/*
 * Insert an XLOG record having the specified RMID and info bytes,
 * with the body of the record being the data chunk(s) described by
 * the rdata chain (see xlog.h for notes about rdata).
 *
 * Returns XLOG pointer to end of record (beginning of next record).
 * This can be used as LSN for data pages affected by the logged action.
 * (LSN is the XLOG point up to which the XLOG must be flushed to disk
 * before the data page can be written out.  This implements the basic
 * WAL rule "write the log before the data".)
 *
 * NB: this routine feels free to scribble on the XLogRecData structs,
 * though not on the data they reference.  This is OK since the XLogRecData
 * structs are always just temporaries in the calling code.
 */
XLogRecPtr
XLogInsert(RmgrId rmid, uint8 info, XLogRecData *rdata)
{
	return XLogInsert_Internal(rmid, info, rdata, GetCurrentTransactionIdIfAny());
}

XLogRecPtr
XLogInsert_OverrideXid(RmgrId rmid, uint8 info, XLogRecData *rdata, TransactionId overrideXid)
{
	return XLogInsert_Internal(rmid, info, rdata, overrideXid);
}


static XLogRecPtr
XLogInsert_Internal(RmgrId rmid, uint8 info, XLogRecData *rdata, TransactionId headerXid)
{

	XLogCtlInsert *Insert = &XLogCtl->Insert;
	XLogRecord *record;
	XLogContRecord *contrecord;
	XLogRecPtr	RecPtr;
	XLogRecPtr	WriteRqst;
	uint32		freespace;
	int			curridx;
	XLogRecData *rdt;
	Buffer		dtbuf[XLR_MAX_BKP_BLOCKS];
	bool		dtbuf_bkp[XLR_MAX_BKP_BLOCKS];
	BkpBlock	dtbuf_xlg[XLR_MAX_BKP_BLOCKS];
	XLogRecPtr	dtbuf_lsn[XLR_MAX_BKP_BLOCKS];
	XLogRecData dtbuf_rdt1[XLR_MAX_BKP_BLOCKS];
	XLogRecData dtbuf_rdt2[XLR_MAX_BKP_BLOCKS];
	XLogRecData dtbuf_rdt3[XLR_MAX_BKP_BLOCKS];
	pg_crc32	rdata_crc;
	uint32		len,
				write_len;
	unsigned	i;
	bool		updrqst;
	bool		doPageWrites;
	bool		isLogSwitch = (rmid == RM_XLOG_ID && info == XLOG_SWITCH);
	uint8       extended_info = 0;

    /* Safety check in case our assumption is ever broken. */
	/* NOTE: This is slightly modified from the one in xact.c -- the test for */
 	/* NOTE: seqXlogWrite is omitted... */
	/* NOTE: some local-only changes are OK */
 	if (Gp_role == GP_ROLE_EXECUTE && !Gp_is_writer)
 	{
 		/*
 	     * we better only do really minor things on the reader that result
 	     * in writing to the xlog here at commit.  for now sequences
 	     * should be the only one
 	     */
		if (DistributedTransactionContext == DTX_CONTEXT_LOCAL_ONLY)
		{
			/* MPP-1687: readers may under some circumstances extend the CLOG
			 * rmid == RM_CLOG_ID and info having CLOG_ZEROPAGE set */
			elog(LOG, "Reader qExec committing LOCAL_ONLY changes. (%d %d)", rmid, info);
		}
		else
		{
			/*
			 * We are allowing the QE Reader to write to support error tables.
			 */
			elog(DEBUG1, "Reader qExec writing changes. (%d %d)", rmid, info);
#ifdef nothing
			ereport(ERROR,
					(errmsg("Reader qExec had local changes to commit! (rmid = %u)",
							rmid),
					 errdetail("A Reader qExec tried to commit local changes.  "
							   "Only the single Writer qExec can do so. "),
					 errhint("This is most likely the result of a feature being turned "
							 "on that violates the single WRITER principle")));
#endif
		}
 	}

	/* GPDB_84_MERGE_FIXME: This cross-check was added in upstream, but it's failing
	 * in Startup pass 2. Disable it for now. */
#if 0
	/* cross-check on whether we should be here or not */
	if (!XLogInsertAllowed())
		elog(ERROR, "cannot make new WAL entries during recovery");
#endif

	/* info's high bits are reserved for use by me */
	if (info & XLR_INFO_MASK)
		elog(PANIC, "invalid xlog info mask %02X", info);

	TRACE_POSTGRESQL_XLOG_INSERT(rmid, info);

	/*
	 * In bootstrap mode, we don't actually log anything but XLOG resources;
	 * return a phony record pointer.
	 */
	if (IsBootstrapProcessingMode() && rmid != RM_XLOG_ID)
	{
		RecPtr.xlogid = 0;
		RecPtr.xrecoff = SizeOfXLogLongPHD;		/* start of 1st chkpt record */
		return RecPtr;
	}

	/*
	 * Enforce consistency checks for this record if user is looking for
	 * it. Do this before at the beginning of this routine to give the
	 * possibility for callers of XLogInsert() to pass XLR_CHECK_CONSISTENCY
	 * directly for a record.
	 */
	if (wal_consistency_checking[rmid])
		extended_info |= XLR_CHECK_CONSISTENCY;

	/*
	 * Here we scan the rdata chain, determine which buffers must be backed
	 * up, and compute the CRC values for the data.  Note that the record
	 * header isn't added into the CRC initially since we don't know the final
	 * length or info bits quite yet.  Thus, the CRC will represent the CRC of
	 * the whole record in the order "rdata, then backup blocks, then record
	 * header".
	 *
	 * We may have to loop back to here if a race condition is detected below.
	 * We could prevent the race by doing all this work while holding the
	 * insert lock, but it seems better to avoid doing CRC calculations while
	 * holding the lock.  This means we have to be careful about modifying the
	 * rdata chain until we know we aren't going to loop back again.  The only
	 * change we allow ourselves to make earlier is to set rdt->data = NULL in
	 * chain items we have decided we will have to back up the whole buffer
	 * for.  This is OK because we will certainly decide the same thing again
	 * for those items if we do it over; doing it here saves an extra pass
	 * over the chain later.
	 */
begin:;
	for (i = 0; i < XLR_MAX_BKP_BLOCKS; i++)
	{
		dtbuf[i] = InvalidBuffer;
		dtbuf_bkp[i] = false;
	}

	/*
	 * Decide if we need to do full-page writes in this XLOG record: true if
	 * full_page_writes is on or we have a PITR request for it.  Since we
	 * don't yet have the insert lock, forcePageWrites could change under us,
	 * but we'll recheck it once we have the lock.
	 */
	doPageWrites = fullPageWrites || Insert->forcePageWrites;

	INIT_CRC32C(rdata_crc);
	len = 0;
	for (rdt = rdata;;)
	{
		if (rdt->buffer == InvalidBuffer)
		{
			/* Simple data, just include it */
			len += rdt->len;
			COMP_CRC32C(rdata_crc, rdt->data, rdt->len);
		}
		else
		{
			/* Find info for buffer */
			for (i = 0; i < XLR_MAX_BKP_BLOCKS; i++)
			{
				if (rdt->buffer == dtbuf[i])
				{
					/*
					 * Buffer already referenced by earlier chain item and
					 * will be applied then only ignore it. Block can exist
					 * for consistency check purpose and hence should include
					 * original data along if its only for that purpose.
					 */
					if (dtbuf_bkp[i] && (dtbuf_xlg[i].block_info & BLOCK_APPLY))
						rdt->data = NULL;
					else if (rdt->data)
					{
						len += rdt->len;
						COMP_CRC32C(rdata_crc, rdt->data, rdt->len);
					}
					break;
				}
				if (dtbuf[i] == InvalidBuffer)
				{
					/* OK, put it in this slot */
					dtbuf[i] = rdt->buffer;

					if (doPageWrites && XLogCheckBuffer(rdt, true,
										(extended_info & XLR_CHECK_CONSISTENCY) != 0,
										&(dtbuf_lsn[i]), &(dtbuf_xlg[i])))
					{
						dtbuf_bkp[i] = true;

						if (dtbuf_xlg[i].block_info & BLOCK_APPLY)
							rdt->data = NULL;
						else
						{
							if (rdt->data)
							{
								len += rdt->len;
								COMP_CRC32C(rdata_crc, rdt->data, rdt->len);
							}
						}
					}
					else if (rdt->data)
					{
						len += rdt->len;
						COMP_CRC32C(rdata_crc, rdt->data, rdt->len);
					}
					break;
				}
			}
			if (i >= XLR_MAX_BKP_BLOCKS)
				elog(PANIC, "can backup at most %d blocks per xlog record",
					 XLR_MAX_BKP_BLOCKS);
		}
		/* Break out of loop when rdt points to last chain item */
		if (rdt->next == NULL)
			break;
		rdt = rdt->next;
	}

	/*
	 * Now add the backup block headers and data into the CRC
	 */
	for (i = 0; i < XLR_MAX_BKP_BLOCKS; i++)
	{
		if (dtbuf_bkp[i])
		{
			BkpBlock   *bkpb = &(dtbuf_xlg[i]);
			char	   *page;

			COMP_CRC32C(rdata_crc,
					   (char *) bkpb,
					   sizeof(BkpBlock));
			page = (char *) BufferGetBlock(dtbuf[i]);
			if (bkpb->hole_length == 0)
			{
				COMP_CRC32C(rdata_crc, page, BLCKSZ);
			}
			else
			{
				/* must skip the hole */
				COMP_CRC32C(rdata_crc, page, bkpb->hole_offset);
				COMP_CRC32C(rdata_crc,
						   page + (bkpb->hole_offset + bkpb->hole_length),
						   BLCKSZ - (bkpb->hole_offset + bkpb->hole_length));
			}
		}
	}

	/*
	 * NOTE: We disallow len == 0 because it provides a useful bit of extra
	 * error checking in ReadRecord.  This means that all callers of
	 * XLogInsert must supply at least some not-in-a-buffer data.  However, we
	 * make an exception for XLOG SWITCH records because we don't want them to
	 * ever cross a segment boundary.
	 */
	if (len == 0 && !isLogSwitch)
		elog(PANIC, "invalid xlog record length %u", len);

	START_CRIT_SECTION();

	/* Now wait to get insert lock */
	LWLockAcquire(WALInsertLock, LW_EXCLUSIVE);

	/*
	 * Check to see if my RedoRecPtr is out of date.  If so, may have to go
	 * back and recompute everything.  This can only happen just after a
	 * checkpoint, so it's better to be slow in this case and fast otherwise.
	 *
	 * If we aren't doing full-page writes then RedoRecPtr doesn't actually
	 * affect the contents of the XLOG record, so we'll update our local copy
	 * but not force a recomputation.
	 */
	if (!XLByteEQ(RedoRecPtr, Insert->RedoRecPtr))
	{
		Assert(XLByteLT(RedoRecPtr, Insert->RedoRecPtr));
		RedoRecPtr = Insert->RedoRecPtr;

		if (doPageWrites)
		{
			for (i = 0; i < XLR_MAX_BKP_BLOCKS; i++)
			{
				if (dtbuf[i] == InvalidBuffer)
					continue;
				if (dtbuf_bkp[i] == false &&
					XLByteLE(dtbuf_lsn[i], RedoRecPtr))
				{
					/*
					 * Oops, this buffer now needs to be backed up, but we
					 * didn't think so above.  Start over.
					 */
					LWLockRelease(WALInsertLock);

					END_CRIT_SECTION();
					goto begin;
				}
			}
		}
	}

	/*
	 * Also check to see if forcePageWrites was just turned on; if we weren't
	 * already doing full-page writes then go back and recompute. (If it was
	 * just turned off, we could recompute the record without full pages, but
	 * we choose not to bother.)
	 */
	if (Insert->forcePageWrites && !doPageWrites)
	{
		/* Oops, must redo it with full-page data */
		LWLockRelease(WALInsertLock);
		END_CRIT_SECTION();
		goto begin;
	}

	/*
	 * Make additional rdata chain entries for the backup blocks, so that we
	 * don't need to special-case them in the write loop.  Note that we have
	 * now irrevocably changed the input rdata chain.  At the exit of this
	 * loop, write_len includes the backup block data.
	 *
	 * Also set the appropriate info bits to show which buffers were backed
	 * up. The i'th XLR_SET_BKP_BLOCK bit corresponds to the i'th distinct
	 * buffer value (ignoring InvalidBuffer) appearing in the rdata chain.
	 */
	write_len = len;
	for (i = 0; i < XLR_MAX_BKP_BLOCKS; i++)
	{
		BkpBlock   *bkpb;
		char	   *page;

		if (!dtbuf_bkp[i])
			continue;

		info |= XLR_SET_BKP_BLOCK(i);

		bkpb = &(dtbuf_xlg[i]);
		page = (char *) BufferGetBlock(dtbuf[i]);

		rdt->next = &(dtbuf_rdt1[i]);
		rdt = rdt->next;

		rdt->data = (char *) bkpb;
		rdt->len = sizeof(BkpBlock);
		write_len += sizeof(BkpBlock);

		rdt->next = &(dtbuf_rdt2[i]);
		rdt = rdt->next;

		if (bkpb->hole_length == 0)
		{
			rdt->data = page;
			rdt->len = BLCKSZ;
			write_len += BLCKSZ;
			rdt->next = NULL;
		}
		else
		{
			/* must skip the hole */
			rdt->data = page;
			rdt->len = bkpb->hole_offset;
			write_len += bkpb->hole_offset;

			rdt->next = &(dtbuf_rdt3[i]);
			rdt = rdt->next;

			rdt->data = page + (bkpb->hole_offset + bkpb->hole_length);
			rdt->len = BLCKSZ - (bkpb->hole_offset + bkpb->hole_length);
			write_len += rdt->len;
			rdt->next = NULL;
		}
	}

	/*
	 * If we backed up any full blocks and online backup is not in progress,
	 * mark the backup blocks as removable.  This allows the WAL archiver to
	 * know whether it is safe to compress archived WAL data by transforming
	 * full-block records into the non-full-block format.
	 *
	 * Note: we could just set the flag whenever !forcePageWrites, but
	 * defining it like this leaves the info bit free for some potential other
	 * use in records without any backup blocks.
	 */
	if ((info & XLR_BKP_BLOCK_MASK) && !Insert->forcePageWrites)
		info |= XLR_BKP_REMOVABLE;

	/*
	 * If there isn't enough space on the current XLOG page for a record
	 * header, advance to the next page (leaving the unused space as zeroes).
	 */
	updrqst = false;
	freespace = INSERT_FREESPACE(Insert);
	if (freespace < SizeOfXLogRecord)
	{
		updrqst = AdvanceXLInsertBuffer(false);
		freespace = INSERT_FREESPACE(Insert);
	}

	/* Compute record's XLOG location */
	curridx = Insert->curridx;
	INSERT_RECPTR(RecPtr, Insert, curridx);

	/*
	 * If the record is an XLOG_SWITCH, and we are exactly at the start of a
	 * segment, we need not insert it (and don't want to because we'd like
	 * consecutive switch requests to be no-ops).  Instead, make sure
	 * everything is written and flushed through the end of the prior segment,
	 * and return the prior segment's end address.
	 */
	if (isLogSwitch &&
		(RecPtr.xrecoff % XLogSegSize) == SizeOfXLogLongPHD)
	{
		/* We can release insert lock immediately */
		LWLockRelease(WALInsertLock);

		RecPtr.xrecoff -= SizeOfXLogLongPHD;
		if (RecPtr.xrecoff == 0)
		{
			/* crossing a logid boundary */
			RecPtr.xlogid -= 1;
			RecPtr.xrecoff = XLogFileSize;
		}

		LWLockAcquire(WALWriteLock, LW_EXCLUSIVE);
		LogwrtResult = XLogCtl->Write.LogwrtResult;
		if (!XLByteLE(RecPtr, LogwrtResult.Flush))
		{
			XLogwrtRqst FlushRqst;

			FlushRqst.Write = RecPtr;
			FlushRqst.Flush = RecPtr;
			XLogWrite(FlushRqst, false, false);
		}
		LWLockRelease(WALWriteLock);

		END_CRIT_SECTION();

		return RecPtr;
	}

	/* Insert record header */

	record = (XLogRecord *) Insert->currpos;
	record->xl_prev = Insert->PrevRecord;
	record->xl_xid = headerXid;
	record->xl_tot_len = SizeOfXLogRecord + write_len;
	record->xl_len = len;		/* doesn't include backup blocks */
	record->xl_info = info;
	record->xl_rmid = rmid;
	record->xl_extended_info = extended_info;

	/* Now we can finish computing the record's CRC */
	COMP_CRC32C(rdata_crc, (char *) record + sizeof(pg_crc32),
			   SizeOfXLogRecord - sizeof(pg_crc32));
	FIN_CRC32C(rdata_crc);
	record->xl_crc = rdata_crc;

	/* Record begin of record in appropriate places */
	ProcLastRecPtr = RecPtr;
	Insert->PrevRecord = RecPtr;

	ProcLastRecTotalLen = record->xl_tot_len;
	ProcLastRecDataLen = write_len;

	Insert->currpos += SizeOfXLogRecord;
	freespace -= SizeOfXLogRecord;

	if (Debug_xlog_insert_print)
	{
		StringInfoData buf;
		char *contiguousCopy;

		initStringInfo(&buf);
		appendStringInfo(&buf, "XLOG INSERT @ %s, total length %u, data length %u: ",
						 XLogLocationToString(&RecPtr),
						 ProcLastRecTotalLen,
						 ProcLastRecDataLen);
		xlog_outrec(&buf, record);

		contiguousCopy = XLogContiguousCopy(record, rdata);
		appendStringInfo(&buf, " - ");
		RmgrTable[record->xl_rmid].rm_desc(&buf, RecPtr, (XLogRecord*)contiguousCopy);
		pfree(contiguousCopy);

		elog(LOG, "%s", buf.data);
		pfree(buf.data);
	}

	/*
	 * Append the data, including backup blocks if any
	 */
	while (write_len)
	{
		while (rdata->data == NULL)
			rdata = rdata->next;

		if (freespace > 0)
		{
			if (rdata->len > freespace)
			{
				memcpy(Insert->currpos, rdata->data, freespace);
				rdata->data += freespace;
				rdata->len -= freespace;
				write_len -= freespace;
			}
			else
			{
				/* enough room to write whole data. do it. */
				memcpy(Insert->currpos, rdata->data, rdata->len);
				freespace -= rdata->len;
				write_len -= rdata->len;
				Insert->currpos += rdata->len;
				rdata = rdata->next;
				continue;
			}
		}

		/* Use next buffer */
		updrqst = AdvanceXLInsertBuffer(false);
		curridx = Insert->curridx;
		/* Insert cont-record header */
		Insert->currpage->xlp_info |= XLP_FIRST_IS_CONTRECORD;
		contrecord = (XLogContRecord *) Insert->currpos;
		contrecord->xl_rem_len = write_len;
		Insert->currpos += SizeOfXLogContRecord;
		freespace = INSERT_FREESPACE(Insert);
	}

	/* Ensure next record will be properly aligned */
	Insert->currpos = (char *) Insert->currpage +
		MAXALIGN(Insert->currpos - (char *) Insert->currpage);
	freespace = INSERT_FREESPACE(Insert);

	/*
	 * The recptr I return is the beginning of the *next* record. This will be
	 * stored as LSN for changed data pages...
	 */
	INSERT_RECPTR(RecPtr, Insert, curridx);

	/*
	 * If the record is an XLOG_SWITCH, we must now write and flush all the
	 * existing data, and then forcibly advance to the start of the next
	 * segment.  It's not good to do this I/O while holding the insert lock,
	 * but there seems too much risk of confusion if we try to release the
	 * lock sooner.  Fortunately xlog switch needn't be a high-performance
	 * operation anyway...
	 */
	if (isLogSwitch)
	{
		XLogCtlWrite *Write = &XLogCtl->Write;
		XLogwrtRqst FlushRqst;
		XLogRecPtr	OldSegEnd;

		TRACE_POSTGRESQL_XLOG_SWITCH();

		LWLockAcquire(WALWriteLock, LW_EXCLUSIVE);

		/*
		 * Flush through the end of the page containing XLOG_SWITCH, and
		 * perform end-of-segment actions (eg, notifying archiver).
		 */
		WriteRqst = XLogCtl->xlblocks[curridx];
		FlushRqst.Write = WriteRqst;
		FlushRqst.Flush = WriteRqst;
		XLogWrite(FlushRqst, false, true);

		/* Set up the next buffer as first page of next segment */
		/* Note: AdvanceXLInsertBuffer cannot need to do I/O here */
		(void) AdvanceXLInsertBuffer(true);

		/* There should be no unwritten data */
		curridx = Insert->curridx;
		Assert(curridx == Write->curridx);

		/* Compute end address of old segment */
		OldSegEnd = XLogCtl->xlblocks[curridx];
		OldSegEnd.xrecoff -= XLOG_BLCKSZ;
		if (OldSegEnd.xrecoff == 0)
		{
			/* crossing a logid boundary */
			OldSegEnd.xlogid -= 1;
			OldSegEnd.xrecoff = XLogFileSize;
		}

		/* Make it look like we've written and synced all of old segment */
		LogwrtResult.Write = OldSegEnd;
		LogwrtResult.Flush = OldSegEnd;

		/*
		 * Update shared-memory status --- this code should match XLogWrite
		 */
		{
			/* use volatile pointer to prevent code rearrangement */
			volatile XLogCtlData *xlogctl = XLogCtl;

			SpinLockAcquire(&xlogctl->info_lck);
			xlogctl->LogwrtResult = LogwrtResult;
			if (XLByteLT(xlogctl->LogwrtRqst.Write, LogwrtResult.Write))
				xlogctl->LogwrtRqst.Write = LogwrtResult.Write;
			if (XLByteLT(xlogctl->LogwrtRqst.Flush, LogwrtResult.Flush))
				xlogctl->LogwrtRqst.Flush = LogwrtResult.Flush;
			SpinLockRelease(&xlogctl->info_lck);
		}

		Write->LogwrtResult = LogwrtResult;

		LWLockRelease(WALWriteLock);

		updrqst = false;		/* done already */
	}
	else
	{
		/* normal case, ie not xlog switch */

		/* Need to update shared LogwrtRqst if some block was filled up */
		if (freespace < SizeOfXLogRecord)
		{
			/* curridx is filled and available for writing out */
			updrqst = true;
		}
		else
		{
			/* if updrqst already set, write through end of previous buf */
			curridx = PrevBufIdx(curridx);
		}
		WriteRqst = XLogCtl->xlblocks[curridx];
	}

	LWLockRelease(WALInsertLock);

	if (updrqst)
	{
		/* use volatile pointer to prevent code rearrangement */
		volatile XLogCtlData *xlogctl = XLogCtl;

		SpinLockAcquire(&xlogctl->info_lck);
		/* advance global request to include new block(s) */
		if (XLByteLT(xlogctl->LogwrtRqst.Write, WriteRqst))
			xlogctl->LogwrtRqst.Write = WriteRqst;
		/* update local result copy while I have the chance */
		LogwrtResult = xlogctl->LogwrtResult;
		SpinLockRelease(&xlogctl->info_lck);
	}

	XactLastRecEnd = RecPtr;

	END_CRIT_SECTION();

	return RecPtr;
}

XLogRecPtr
XLogLastInsertBeginLoc(void)
{
	return ProcLastRecPtr;
}

/*
 * Determine whether the buffer referenced by an XLogRecData item has to
 * be backed up, and if so fill a BkpBlock struct for it.  In any case
 * save the buffer's LSN at *lsn.
 */
static bool
XLogCheckBuffer(XLogRecData *rdata, bool holdsExclusiveLock,
				bool wal_check_consistency_enabled,
				XLogRecPtr *lsn, BkpBlock *bkpb)
{
	Page		page;
	bool needs_backup;

	page = BufferGetPage(rdata->buffer);

	/*
	 * We assume page LSN is first data on *every* page that can be passed
	 * to XLogInsert, whether it has the standard page layout or not. We
	 * don't need to take the buffer header lock for PageGetLSN if we hold
	 * an exclusive lock on the page and/or the relation.
	 */
	if (holdsExclusiveLock)
		*lsn = PageGetLSN(page);
	else
		*lsn = BufferGetLSNAtomic(rdata->buffer);

	needs_backup = XLByteLE(((PageHeader) page)->pd_lsn, RedoRecPtr);

	if (needs_backup || wal_check_consistency_enabled)
	{
		/*
		 * The page needs to be backed up, so set up *bkpb
		 */
		BufferGetTag(rdata->buffer, &bkpb->node, &bkpb->fork, &bkpb->block);
		bkpb->block_info = 0;

		/*
		 * If WAL consistency checking is enabled for the
		 * resource manager of this WAL record, a full-page
		 * image is included in the record for the block
		 * modified. During redo, the full-page is replayed
		 * only if block_apply is set.
		 */
		if (needs_backup)
			bkpb->block_info |= BLOCK_APPLY;

		if (rdata->buffer_std)
		{
			/* Assume we can omit data between pd_lower and pd_upper */
			uint16		lower = ((PageHeader) page)->pd_lower;
			uint16		upper = ((PageHeader) page)->pd_upper;

			if (lower >= SizeOfPageHeaderData &&
				upper > lower &&
				upper <= BLCKSZ)
			{
				bkpb->hole_offset = lower;
				bkpb->hole_length = upper - lower;
			}
			else
			{
				/* No "hole" to compress out */
				bkpb->hole_offset = 0;
				bkpb->hole_length = 0;
			}
		}
		else
		{
			/* Not a standard page header, don't try to eliminate "hole" */
			bkpb->hole_offset = 0;
			bkpb->hole_length = 0;
		}

		return true;			/* buffer requires backup */
	}

	return false;				/* buffer does not need to be backed up */
}

/*
 * XLogArchiveNotify
 *
 * Create an archive notification file
 *
 * The name of the notification file is the message that will be picked up
 * by the archiver, e.g. we write 0000000100000001000000C6.ready
 * and the archiver then knows to archive XLOGDIR/0000000100000001000000C6,
 * then when complete, rename it to 0000000100000001000000C6.done
 */
static void
XLogArchiveNotify(const char *xlog)
{
	char		archiveStatusPath[MAXPGPATH];
	FILE	   *fd;

	/* insert an otherwise empty file called <XLOG>.ready */
	StatusFilePath(archiveStatusPath, xlog, ".ready");
	fd = AllocateFile(archiveStatusPath, "w");
	if (fd == NULL)
	{
		ereport(LOG,
				(errcode_for_file_access(),
				 errmsg("could not create archive status file \"%s\": %m",
						archiveStatusPath)));
		return;
	}
	if (FreeFile(fd))
	{
		ereport(LOG,
				(errcode_for_file_access(),
				 errmsg("could not write archive status file \"%s\": %m",
						archiveStatusPath)));
		return;
	}

	/* Notify archiver that it's got something to do */
	if (IsUnderPostmaster)
		SendPostmasterSignal(PMSIGNAL_WAKEN_ARCHIVER);
}

/*
 * Convenience routine to notify using log/seg representation of filename
 */
static void
XLogArchiveNotifySeg(uint32 log, uint32 seg)
{
	char		xlog[MAXFNAMELEN];

	XLogFileName(xlog, ThisTimeLineID, log, seg);
	XLogArchiveNotify(xlog);
}

/*
 * XLogArchiveCheckDone
 *
 * This is called when we are ready to delete or recycle an old XLOG segment
 * file or backup history file.  If it is okay to delete it then return true.
 * If it is not time to delete it, make sure a .ready file exists, and return
 * false.
 *
 * If <XLOG>.done exists, then return true; else if <XLOG>.ready exists,
 * then return false; else create <XLOG>.ready and return false.
 *
 * The reason we do things this way is so that if the original attempt to
 * create <XLOG>.ready fails, we'll retry during subsequent checkpoints.
 */
static bool
XLogArchiveCheckDone(const char *xlog)
{
	char		archiveStatusPath[MAXPGPATH];
	struct stat stat_buf;

	/* Always deletable if archiving is off */
	if (!XLogArchivingActive())
		return true;

	/* First check for .done --- this means archiver is done with it */
	StatusFilePath(archiveStatusPath, xlog, ".done");
	if (stat(archiveStatusPath, &stat_buf) == 0)
		return true;

	/* check for .ready --- this means archiver is still busy with it */
	StatusFilePath(archiveStatusPath, xlog, ".ready");
	if (stat(archiveStatusPath, &stat_buf) == 0)
		return false;

	/* Race condition --- maybe archiver just finished, so recheck */
	StatusFilePath(archiveStatusPath, xlog, ".done");
	if (stat(archiveStatusPath, &stat_buf) == 0)
		return true;

	/* Retry creation of the .ready file */
	XLogArchiveNotify(xlog);
	return false;
}

/*
 * XLogArchiveIsBusy
 *
 * Check to see if an XLOG segment file is still unarchived.
 * This is almost but not quite the inverse of XLogArchiveCheckDone: in
 * the first place we aren't chartered to recreate the .ready file, and
 * in the second place we should consider that if the file is already gone
 * then it's not busy.  (This check is needed to handle the race condition
 * that a checkpoint already deleted the no-longer-needed file.)
 */
static bool
XLogArchiveIsBusy(const char *xlog)
{
	char		archiveStatusPath[MAXPGPATH];
	struct stat stat_buf;

	/* First check for .done --- this means archiver is done with it */
	StatusFilePath(archiveStatusPath, xlog, ".done");
	if (stat(archiveStatusPath, &stat_buf) == 0)
		return false;

	/* check for .ready --- this means archiver is still busy with it */
	StatusFilePath(archiveStatusPath, xlog, ".ready");
	if (stat(archiveStatusPath, &stat_buf) == 0)
		return true;

	/* Race condition --- maybe archiver just finished, so recheck */
	StatusFilePath(archiveStatusPath, xlog, ".done");
	if (stat(archiveStatusPath, &stat_buf) == 0)
		return false;

	/*
	 * Check to see if the WAL file has been removed by checkpoint, which
	 * implies it has already been archived, and explains why we can't see a
	 * status file for it.
	 */
	snprintf(archiveStatusPath, MAXPGPATH, XLOGDIR "/%s", xlog);
	if (stat(archiveStatusPath, &stat_buf) != 0 &&
		errno == ENOENT)
		return false;

	return true;
}

/*
 * XLogArchiveCleanup
 *
 * Cleanup archive notification file(s) for a particular xlog segment
 */
static void
XLogArchiveCleanup(const char *xlog)
{
	char		archiveStatusPath[MAXPGPATH];

	/* Remove the .done file */
	StatusFilePath(archiveStatusPath, xlog, ".done");
	unlink(archiveStatusPath);
	/* should we complain about failure? */

	/* Remove the .ready file if present --- normally it shouldn't be */
	StatusFilePath(archiveStatusPath, xlog, ".ready");
	unlink(archiveStatusPath);
	/* should we complain about failure? */
}

/*
 * Advance the Insert state to the next buffer page, writing out the next
 * buffer if it still contains unwritten data.
 *
 * If new_segment is TRUE then we set up the next buffer page as the first
 * page of the next xlog segment file, possibly but not usually the next
 * consecutive file page.
 *
 * The global LogwrtRqst.Write pointer needs to be advanced to include the
 * just-filled page.  If we can do this for free (without an extra lock),
 * we do so here.  Otherwise the caller must do it.  We return TRUE if the
 * request update still needs to be done, FALSE if we did it internally.
 *
 * Must be called with WALInsertLock held.
 */
static bool
AdvanceXLInsertBuffer(bool new_segment)
{
	XLogCtlInsert *Insert = &XLogCtl->Insert;
	XLogCtlWrite *Write = &XLogCtl->Write;
	int			nextidx = NextBufIdx(Insert->curridx);
	bool		update_needed = true;
	XLogRecPtr	OldPageRqstPtr;
	XLogwrtRqst WriteRqst;
	XLogRecPtr	NewPageEndPtr;
	XLogPageHeader NewPage;

	/* Use Insert->LogwrtResult copy if it's more fresh */
	if (XLByteLT(LogwrtResult.Write, Insert->LogwrtResult.Write))
		LogwrtResult = Insert->LogwrtResult;

	/*
	 * Get ending-offset of the buffer page we need to replace (this may be
	 * zero if the buffer hasn't been used yet).  Fall through if it's already
	 * written out.
	 */
	OldPageRqstPtr = XLogCtl->xlblocks[nextidx];
	if (!XLByteLE(OldPageRqstPtr, LogwrtResult.Write))
	{
		/* nope, got work to do... */
		XLogRecPtr	FinishedPageRqstPtr;

		FinishedPageRqstPtr = XLogCtl->xlblocks[Insert->curridx];

		/* Before waiting, get info_lck and update LogwrtResult */
		{
			/* use volatile pointer to prevent code rearrangement */
			volatile XLogCtlData *xlogctl = XLogCtl;

			SpinLockAcquire(&xlogctl->info_lck);
			if (XLByteLT(xlogctl->LogwrtRqst.Write, FinishedPageRqstPtr))
				xlogctl->LogwrtRqst.Write = FinishedPageRqstPtr;
			LogwrtResult = xlogctl->LogwrtResult;
			SpinLockRelease(&xlogctl->info_lck);
		}

		update_needed = false;	/* Did the shared-request update */

		if (XLByteLE(OldPageRqstPtr, LogwrtResult.Write))
		{
			/* OK, someone wrote it already */
			Insert->LogwrtResult = LogwrtResult;
		}
		else
		{
			/* Must acquire write lock */
			LWLockAcquire(WALWriteLock, LW_EXCLUSIVE);
			LogwrtResult = Write->LogwrtResult;
			if (XLByteLE(OldPageRqstPtr, LogwrtResult.Write))
			{
				/* OK, someone wrote it already */
				LWLockRelease(WALWriteLock);
				Insert->LogwrtResult = LogwrtResult;
			}
			else
			{
				/*
				 * Have to write buffers while holding insert lock. This is
				 * not good, so only write as much as we absolutely must.
				 */
				TRACE_POSTGRESQL_WAL_BUFFER_WRITE_DIRTY_START();
				WriteRqst.Write = OldPageRqstPtr;
				WriteRqst.Flush.xlogid = 0;
				WriteRqst.Flush.xrecoff = 0;
				XLogWrite(WriteRqst, false, false);
				LWLockRelease(WALWriteLock);
				Insert->LogwrtResult = LogwrtResult;
				TRACE_POSTGRESQL_WAL_BUFFER_WRITE_DIRTY_DONE();
			}
		}
	}

	/*
	 * Now the next buffer slot is free and we can set it up to be the next
	 * output page.
	 */
	NewPageEndPtr = XLogCtl->xlblocks[Insert->curridx];

	if (new_segment)
	{
		/* force it to a segment start point */
		NewPageEndPtr.xrecoff += XLogSegSize - 1;
		NewPageEndPtr.xrecoff -= NewPageEndPtr.xrecoff % XLogSegSize;
	}

	if (NewPageEndPtr.xrecoff >= XLogFileSize)
	{
		/* crossing a logid boundary */
		NewPageEndPtr.xlogid += 1;
		NewPageEndPtr.xrecoff = XLOG_BLCKSZ;
	}
	else
		NewPageEndPtr.xrecoff += XLOG_BLCKSZ;
	XLogCtl->xlblocks[nextidx] = NewPageEndPtr;
	NewPage = (XLogPageHeader) (XLogCtl->pages + nextidx * (Size) XLOG_BLCKSZ);

	Insert->curridx = nextidx;
	Insert->currpage = NewPage;

	Insert->currpos = ((char *) NewPage) +SizeOfXLogShortPHD;

	/*
	 * Be sure to re-zero the buffer so that bytes beyond what we've written
	 * will look like zeroes and not valid XLOG records...
	 */
	MemSet((char *) NewPage, 0, XLOG_BLCKSZ);

	/*
	 * Fill the new page's header
	 */
	NewPage   ->xlp_magic = XLOG_PAGE_MAGIC;

	/* NewPage->xlp_info = 0; */	/* done by memset */
	NewPage   ->xlp_tli = ThisTimeLineID;
	NewPage   ->xlp_pageaddr.xlogid = NewPageEndPtr.xlogid;
	NewPage   ->xlp_pageaddr.xrecoff = NewPageEndPtr.xrecoff - XLOG_BLCKSZ;

	/*
	 * If first page of an XLOG segment file, make it a long header.
	 */
	if ((NewPage->xlp_pageaddr.xrecoff % XLogSegSize) == 0)
	{
		XLogLongPageHeader NewLongPage = (XLogLongPageHeader) NewPage;

		NewLongPage->xlp_sysid = ControlFile->system_identifier;
		NewLongPage->xlp_seg_size = XLogSegSize;
		NewLongPage->xlp_xlog_blcksz = XLOG_BLCKSZ;
		NewPage   ->xlp_info |= XLP_LONG_HEADER;

		Insert->currpos = ((char *) NewPage) +SizeOfXLogLongPHD;
	}

	return update_needed;
}

/*
 * Check whether we've consumed enough xlog space that a checkpoint is needed.
 *
 * Caller must have just finished filling the open log file (so that
 * openLogId/openLogSeg are valid).  We measure the distance from RedoRecPtr
 * to the open log file and see if that exceeds CheckPointSegments.
 *
 * Note: it is caller's responsibility that RedoRecPtr is up-to-date.
 */
static bool
XLogCheckpointNeeded(void)
{
	/*
	 * A straight computation of segment number could overflow 32 bits. Rather
	 * than assuming we have working 64-bit arithmetic, we compare the
	 * highest-order bits separately, and force a checkpoint immediately when
	 * they change.
	 */
	uint32		old_segno,
				new_segno;
	uint32		old_highbits,
				new_highbits;

	old_segno = (RedoRecPtr.xlogid % XLogSegSize) * XLogSegsPerFile +
		(RedoRecPtr.xrecoff / XLogSegSize);
	old_highbits = RedoRecPtr.xlogid / XLogSegSize;
	new_segno = (openLogId % XLogSegSize) * XLogSegsPerFile + openLogSeg;
	new_highbits = openLogId / XLogSegSize;
	if (new_highbits != old_highbits ||
		new_segno >= old_segno + (uint32) (CheckPointSegments - 1))
		return true;
	return false;
}

/*
 * Write and/or fsync the log at least as far as WriteRqst indicates.
 *
 * If flexible == TRUE, we don't have to write as far as WriteRqst, but
 * may stop at any convenient boundary (such as a cache or logfile boundary).
 * This option allows us to avoid uselessly issuing multiple writes when a
 * single one would do.
 *
 * If xlog_switch == TRUE, we are intending an xlog segment switch, so
 * perform end-of-segment actions after writing the last page, even if
 * it's not physically the end of its segment.  (NB: this will work properly
 * only if caller specifies WriteRqst == page-end and flexible == false,
 * and there is some data to write.)
 *
 * Must be called with WALWriteLock held.
 */
static void
XLogWrite(XLogwrtRqst WriteRqst, bool flexible, bool xlog_switch)
{
	XLogCtlWrite *Write = &XLogCtl->Write;
	bool		ispartialpage;
	bool		last_iteration;
	bool		finishing_seg;
	bool		use_existent;
	int			curridx;
	int			npages;
	int			startidx;
	uint32		startoffset;

	/* We should always be inside a critical section here */
	Assert(CritSectionCount > 0);

	/*
	 * Update local LogwrtResult (caller probably did this already, but...)
	 */
	LogwrtResult = Write->LogwrtResult;

	/*
	 * Since successive pages in the xlog cache are consecutively allocated,
	 * we can usually gather multiple pages together and issue just one
	 * write() call.  npages is the number of pages we have determined can be
	 * written together; startidx is the cache block index of the first one,
	 * and startoffset is the file offset at which it should go. The latter
	 * two variables are only valid when npages > 0, but we must initialize
	 * all of them to keep the compiler quiet.
	 */
	npages = 0;
	startidx = 0;
	startoffset = 0;

	/*
	 * Within the loop, curridx is the cache block index of the page to
	 * consider writing.  We advance Write->curridx only after successfully
	 * writing pages.  (Right now, this refinement is useless since we are
	 * going to PANIC if any error occurs anyway; but someday it may come in
	 * useful.)
	 */
	curridx = Write->curridx;

	while (XLByteLT(LogwrtResult.Write, WriteRqst.Write))
	{
		/*
		 * Make sure we're not ahead of the insert process.  This could happen
		 * if we're passed a bogus WriteRqst.Write that is past the end of the
		 * last page that's been initialized by AdvanceXLInsertBuffer.
		 */
		if (!XLByteLT(LogwrtResult.Write, XLogCtl->xlblocks[curridx]))
			elog(PANIC, "xlog write request %X/%X is past end of log %X/%X",
				 LogwrtResult.Write.xlogid, LogwrtResult.Write.xrecoff,
				 XLogCtl->xlblocks[curridx].xlogid,
				 XLogCtl->xlblocks[curridx].xrecoff);

		/* Advance LogwrtResult.Write to end of current buffer page */
		LogwrtResult.Write = XLogCtl->xlblocks[curridx];
		ispartialpage = XLByteLT(WriteRqst.Write, LogwrtResult.Write);

		if (!XLByteInPrevSeg(LogwrtResult.Write, openLogId, openLogSeg))
		{
			/*
			 * Switch to new logfile segment.  We cannot have any pending
			 * pages here (since we dump what we have at segment end).
			 */
			Assert(npages == 0);
			if (openLogFile >= 0)
				XLogFileClose();
			XLByteToPrevSeg(LogwrtResult.Write, openLogId, openLogSeg);

			/* create/use new log file */
			use_existent = true;
			openLogFile = XLogFileInit(openLogId, openLogSeg,
									   &use_existent, true);
			openLogOff = 0;
		}

		/* Make sure we have the current logfile open */
		if (openLogFile < 0)
		{
			XLByteToPrevSeg(LogwrtResult.Write, openLogId, openLogSeg);
			openLogFile = XLogFileOpen(openLogId, openLogSeg);
			openLogOff = 0;
		}

		/* Add current page to the set of pending pages-to-dump */
		if (npages == 0)
		{
			/* first of group */
			startidx = curridx;
			startoffset = (LogwrtResult.Write.xrecoff - XLOG_BLCKSZ) % XLogSegSize;
		}
		npages++;

		/*
		 * Dump the set if this will be the last loop iteration, or if we are
		 * at the last page of the cache area (since the next page won't be
		 * contiguous in memory), or if we are at the end of the logfile
		 * segment.
		 */
		last_iteration = !XLByteLT(LogwrtResult.Write, WriteRqst.Write);

		finishing_seg = !ispartialpage &&
			(startoffset + npages * XLOG_BLCKSZ) >= XLogSegSize;

		if (last_iteration ||
			curridx == XLogCtl->XLogCacheBlck ||
			finishing_seg)
		{
			char	   *from;
			Size		nbytes;

			/* Need to seek in the file? */
			if (openLogOff != startoffset)
			{
				if (lseek(openLogFile, (off_t) startoffset, SEEK_SET) < 0)
					ereport(PANIC,
							(errcode_for_file_access(),
							 errmsg("could not seek in log file %u, "
									"segment %u to offset %u: %m",
									openLogId, openLogSeg, startoffset)));
				openLogOff = startoffset;
			}

			/* OK to write the page(s) */
			from = XLogCtl->pages + startidx * (Size) XLOG_BLCKSZ;
			nbytes = npages * (Size) XLOG_BLCKSZ;
			errno = 0;
			if (write(openLogFile, from, nbytes) != nbytes)
			{
				/* if write didn't set errno, assume no disk space */
				if (errno == 0)
					errno = ENOSPC;
				ereport(PANIC,
						(errcode_for_file_access(),
						 errmsg("could not write to log file %u, segment %u "
								"at offset %u, length %lu: %m",
								openLogId, openLogSeg,
								openLogOff, (unsigned long) nbytes)));
			}

			/* Update state for write */
			openLogOff += nbytes;
			Write->curridx = ispartialpage ? curridx : NextBufIdx(curridx);
			npages = 0;

			/*
			 * If we just wrote the whole last page of a logfile segment,
			 * fsync the segment immediately.  This avoids having to go back
			 * and re-open prior segments when an fsync request comes along
			 * later. Doing it here ensures that one and only one backend will
			 * perform this fsync.
			 *
			 * We also do this if this is the last page written for an xlog
			 * switch.
			 *
			 * This is also the right place to notify the Archiver that the
			 * segment is ready to copy to archival storage, and to update the
			 * timer for archive_timeout, and to signal for a checkpoint if
			 * too many logfile segments have been used since the last
			 * checkpoint.
			 */
			if (finishing_seg || (xlog_switch && last_iteration))
			{
				issue_xlog_fsync(openLogFile, openLogId, openLogSeg);
				LogwrtResult.Flush = LogwrtResult.Write;		/* end of page */

				if (XLogArchivingActive())
					XLogArchiveNotifySeg(openLogId, openLogSeg);

				Write->lastSegSwitchTime = (pg_time_t) time(NULL);

				/*
				 * Signal bgwriter to start a checkpoint if we've consumed too
				 * much xlog since the last one.  For speed, we first check
				 * using the local copy of RedoRecPtr, which might be out of
				 * date; if it looks like a checkpoint is needed, forcibly
				 * update RedoRecPtr and recheck.
				 */
				if (IsUnderPostmaster &&
					XLogCheckpointNeeded())
				{
					(void) GetRedoRecPtr();
					if (XLogCheckpointNeeded())
						RequestCheckpoint(CHECKPOINT_CAUSE_XLOG);
				}
			}
		}

		if (ispartialpage)
		{
			/* Only asked to write a partial page */
			LogwrtResult.Write = WriteRqst.Write;
			break;
		}
		curridx = NextBufIdx(curridx);

		/* If flexible, break out of loop as soon as we wrote something */
		if (flexible && npages == 0)
			break;
	}

	Assert(npages == 0);
	Assert(curridx == Write->curridx);

	/*
	 * If asked to flush, do so
	 */
	if (XLByteLT(LogwrtResult.Flush, WriteRqst.Flush) &&
		XLByteLT(LogwrtResult.Flush, LogwrtResult.Write))
	{
		/*
		 * Could get here without iterating above loop, in which case we might
		 * have no open file or the wrong one.	However, we do not need to
		 * fsync more than one file.
		 */
		if (sync_method != SYNC_METHOD_OPEN &&
			sync_method != SYNC_METHOD_OPEN_DSYNC)
		{
			if (openLogFile >= 0 &&
				!XLByteInPrevSeg(LogwrtResult.Write, openLogId, openLogSeg))
				XLogFileClose();
			if (openLogFile < 0)
			{
				XLByteToPrevSeg(LogwrtResult.Write, openLogId, openLogSeg);
				openLogFile = XLogFileOpen(openLogId, openLogSeg);
				openLogOff = 0;
			}
			issue_xlog_fsync(openLogFile, openLogId, openLogSeg);
		}
		LogwrtResult.Flush = LogwrtResult.Write;
	}

	/*
	 * Update shared-memory status
	 *
	 * We make sure that the shared 'request' values do not fall behind the
	 * 'result' values.  This is not absolutely essential, but it saves some
	 * code in a couple of places.
	 */
	{
		/* use volatile pointer to prevent code rearrangement */
		volatile XLogCtlData *xlogctl = XLogCtl;

		SpinLockAcquire(&xlogctl->info_lck);
		xlogctl->LogwrtResult = LogwrtResult;
		if (XLByteLT(xlogctl->LogwrtRqst.Write, LogwrtResult.Write))
			xlogctl->LogwrtRqst.Write = LogwrtResult.Write;
		if (XLByteLT(xlogctl->LogwrtRqst.Flush, LogwrtResult.Flush))
			xlogctl->LogwrtRqst.Flush = LogwrtResult.Flush;
		SpinLockRelease(&xlogctl->info_lck);
	}

	Write->LogwrtResult = LogwrtResult;
}

/*
 * Record the LSN for an asynchronous transaction commit.
 * (This should not be called for aborts, nor for synchronous commits.)
 */
void
XLogSetAsyncCommitLSN(XLogRecPtr asyncCommitLSN)
{
	/* use volatile pointer to prevent code rearrangement */
	volatile XLogCtlData *xlogctl = XLogCtl;

	SpinLockAcquire(&xlogctl->info_lck);
	if (XLByteLT(xlogctl->asyncCommitLSN, asyncCommitLSN))
		xlogctl->asyncCommitLSN = asyncCommitLSN;
	SpinLockRelease(&xlogctl->info_lck);
}

/*
 * Advance minRecoveryPoint in control file.
 *
 * If we crash during recovery, we must reach this point again before the
 * database is consistent.
 *
 * If 'force' is true, 'lsn' argument is ignored. Otherwise, minRecoveryPoint
 * is only updated if it's not already greater than or equal to 'lsn'.
 */
static void
UpdateMinRecoveryPoint(XLogRecPtr lsn, bool force)
{
	/* Quick check using our local copy of the variable */
	if (!updateMinRecoveryPoint || (!force && XLByteLE(lsn, minRecoveryPoint)))
		return;

	LWLockAcquire(ControlFileLock, LW_EXCLUSIVE);

	/* update local copy */
	minRecoveryPoint = ControlFile->minRecoveryPoint;

	/*
	 * An invalid minRecoveryPoint means that we need to recover all the WAL,
	 * i.e., we're doing crash recovery.  We never modify the control file's
	 * value in that case, so we can short-circuit future checks here too.
	 */
	if (minRecoveryPoint.xlogid == 0 && minRecoveryPoint.xrecoff == 0)
		updateMinRecoveryPoint = false;
	else if (force || XLByteLT(minRecoveryPoint, lsn))
	{
		/* use volatile pointer to prevent code rearrangement */
		volatile XLogCtlData *xlogctl = XLogCtl;
		XLogRecPtr	newMinRecoveryPoint;

		/*
		 * To avoid having to update the control file too often, we update it
		 * all the way to the last record being replayed, even though 'lsn'
		 * would suffice for correctness.  This also allows the 'force' case
		 * to not need a valid 'lsn' value.
		 *
		 * Another important reason for doing it this way is that the passed
		 * 'lsn' value could be bogus, i.e., past the end of available WAL,
		 * if the caller got it from a corrupted heap page.  Accepting such
		 * a value as the min recovery point would prevent us from coming up
		 * at all.  Instead, we just log a warning and continue with recovery.
		 * (See also the comments about corrupt LSNs in XLogFlush.)
		 */
		SpinLockAcquire(&xlogctl->info_lck);
		newMinRecoveryPoint = xlogctl->replayEndRecPtr;
		SpinLockRelease(&xlogctl->info_lck);

		if (!force && XLByteLT(newMinRecoveryPoint, lsn))
			elog(WARNING,
				 "xlog min recovery request %X/%X is past current point %X/%X",
				 lsn.xlogid, lsn.xrecoff,
				 newMinRecoveryPoint.xlogid, newMinRecoveryPoint.xrecoff);

		/* update control file */
		if (XLByteLT(ControlFile->minRecoveryPoint, newMinRecoveryPoint))
		{
			ControlFile->minRecoveryPoint = newMinRecoveryPoint;
			UpdateControlFile();
			minRecoveryPoint = newMinRecoveryPoint;

			ereport(DEBUG2,
					(errmsg("updated min recovery point to %X/%X",
						minRecoveryPoint.xlogid, minRecoveryPoint.xrecoff)));
		}
	}
	LWLockRelease(ControlFileLock);
}

/*
 * Ensure that all XLOG data through the given position is flushed to disk.
 *
 * NOTE: this differs from XLogWrite mainly in that the WALWriteLock is not
 * already held, and we try to avoid acquiring it if possible.
 */
void
XLogFlush(XLogRecPtr record)
{
	XLogRecPtr	WriteRqstPtr;
	XLogwrtRqst WriteRqst;

	/*
	 * During REDO, we are reading not writing WAL.  Therefore, instead of
	 * trying to flush the WAL, we should update minRecoveryPoint instead.
	 * We test XLogInsertAllowed(), not InRecovery, because we need the
	 * bgwriter to act this way too, and because when the bgwriter tries
	 * to write the end-of-recovery checkpoint, it should indeed flush.
	 */
	if (!XLogInsertAllowed())
	{
		UpdateMinRecoveryPoint(record, false);
		return;
	}

	/* Quick exit if already known flushed */
	if (XLByteLE(record, LogwrtResult.Flush))
		return;

#ifdef WAL_DEBUG
	if (XLOG_DEBUG)
		elog(LOG, "xlog flush request %X/%X; write %X/%X; flush %X/%X",
			 record.xlogid, record.xrecoff,
			 LogwrtResult.Write.xlogid, LogwrtResult.Write.xrecoff,
			 LogwrtResult.Flush.xlogid, LogwrtResult.Flush.xrecoff);
#endif

	START_CRIT_SECTION();

	/*
	 * Since fsync is usually a horribly expensive operation, we try to
	 * piggyback as much data as we can on each fsync: if we see any more data
	 * entered into the xlog buffer, we'll write and fsync that too, so that
	 * the final value of LogwrtResult.Flush is as large as possible. This
	 * gives us some chance of avoiding another fsync immediately after.
	 */

	/* initialize to given target; may increase below */
	WriteRqstPtr = record;

	/* read LogwrtResult and update local state */
	{
		/* use volatile pointer to prevent code rearrangement */
		volatile XLogCtlData *xlogctl = XLogCtl;

		SpinLockAcquire(&xlogctl->info_lck);
		if (XLByteLT(WriteRqstPtr, xlogctl->LogwrtRqst.Write))
			WriteRqstPtr = xlogctl->LogwrtRqst.Write;
		LogwrtResult = xlogctl->LogwrtResult;
		SpinLockRelease(&xlogctl->info_lck);
	}

	/* done already? */
	if (!XLByteLE(record, LogwrtResult.Flush))
	{
		/* now wait for the write lock */
		LWLockAcquire(WALWriteLock, LW_EXCLUSIVE);
		LogwrtResult = XLogCtl->Write.LogwrtResult;
		if (!XLByteLE(record, LogwrtResult.Flush))
		{
			/* try to write/flush later additions to XLOG as well */
			if (LWLockConditionalAcquire(WALInsertLock, LW_EXCLUSIVE))
			{
				XLogCtlInsert *Insert = &XLogCtl->Insert;
				uint32		freespace = INSERT_FREESPACE(Insert);

				if (freespace < SizeOfXLogRecord)		/* buffer is full */
					WriteRqstPtr = XLogCtl->xlblocks[Insert->curridx];
				else
				{
					WriteRqstPtr = XLogCtl->xlblocks[Insert->curridx];
					WriteRqstPtr.xrecoff -= freespace;
				}
				LWLockRelease(WALInsertLock);
				WriteRqst.Write = WriteRqstPtr;
				WriteRqst.Flush = WriteRqstPtr;
			}
			else
			{
				WriteRqst.Write = WriteRqstPtr;
				WriteRqst.Flush = record;
			}
			XLogWrite(WriteRqst, false, false);
		}
		LWLockRelease(WALWriteLock);
	}

	END_CRIT_SECTION();

	/*
	 * If we still haven't flushed to the request point then we have a
	 * problem; most likely, the requested flush point is past end of XLOG.
	 * This has been seen to occur when a disk page has a corrupted LSN.
	 *
	 * Formerly we treated this as a PANIC condition, but that hurts the
	 * system's robustness rather than helping it: we do not want to take down
	 * the whole system due to corruption on one data page.  In particular, if
	 * the bad page is encountered again during recovery then we would be
	 * unable to restart the database at all!  (This scenario actually
	 * happened in the field several times with 7.1 releases.)  As of 8.4,
	 * bad LSNs encountered during recovery are UpdateMinRecoveryPoint's
	 * problem; the only time we can reach here during recovery is while
	 * flushing the end-of-recovery checkpoint record, and we don't expect
	 * that to have a bad LSN.
	 *
	 * Note that for calls from xact.c, the ERROR will
	 * be promoted to PANIC since xact.c calls this routine inside a critical
	 * section.  However, calls from bufmgr.c are not within critical sections
	 * and so we will not force a restart for a bad LSN on a data page.
	 */
	if (XLByteLT(LogwrtResult.Flush, record))
		elog(ERROR,
		"xlog flush request %X/%X is not satisfied --- flushed only to %X/%X",
			 record.xlogid, record.xrecoff,
			 LogwrtResult.Flush.xlogid, LogwrtResult.Flush.xrecoff);
}

/*
 * Flush xlog, but without specifying exactly where to flush to.
 *
 * We normally flush only completed blocks; but if there is nothing to do on
 * that basis, we check for unflushed async commits in the current incomplete
 * block, and flush through the latest one of those.  Thus, if async commits
 * are not being used, we will flush complete blocks only.	We can guarantee
 * that async commits reach disk after at most three cycles; normally only
 * one or two.	(We allow XLogWrite to write "flexibly", meaning it can stop
 * at the end of the buffer ring; this makes a difference only with very high
 * load or long wal_writer_delay, but imposes one extra cycle for the worst
 * case for async commits.)
 *
 * This routine is invoked periodically by the background walwriter process.
 */
void
XLogBackgroundFlush(void)
{
	XLogRecPtr	WriteRqstPtr;
	bool		flexible = true;

	/* XLOG doesn't need flushing during recovery */
	if (RecoveryInProgress())
		return;

	/* read LogwrtResult and update local state */
	{
		/* use volatile pointer to prevent code rearrangement */
		volatile XLogCtlData *xlogctl = XLogCtl;

		SpinLockAcquire(&xlogctl->info_lck);
		LogwrtResult = xlogctl->LogwrtResult;
		WriteRqstPtr = xlogctl->LogwrtRqst.Write;
		SpinLockRelease(&xlogctl->info_lck);
	}

	/* back off to last completed page boundary */
	WriteRqstPtr.xrecoff -= WriteRqstPtr.xrecoff % XLOG_BLCKSZ;

	/* if we have already flushed that far, consider async commit records */
	if (XLByteLE(WriteRqstPtr, LogwrtResult.Flush))
	{
		/* use volatile pointer to prevent code rearrangement */
		volatile XLogCtlData *xlogctl = XLogCtl;

		SpinLockAcquire(&xlogctl->info_lck);
		WriteRqstPtr = xlogctl->asyncCommitLSN;
		SpinLockRelease(&xlogctl->info_lck);
		flexible = false;		/* ensure it all gets written */
	}

	/*
	 * If already known flushed, we're done. Just need to check if we
	 * are holding an open file handle to a logfile that's no longer
	 * in use, preventing the file from being deleted.
	 */
	if (XLByteLE(WriteRqstPtr, LogwrtResult.Flush))
	{
		if (openLogFile >= 0)
		{
			if (!XLByteInPrevSeg(LogwrtResult.Write, openLogId, openLogSeg))
			{
				XLogFileClose();
			}
		}
		return;
	}

#ifdef WAL_DEBUG
	if (XLOG_DEBUG)
		elog(LOG, "xlog bg flush request %X/%X; write %X/%X; flush %X/%X",
			 WriteRqstPtr.xlogid, WriteRqstPtr.xrecoff,
			 LogwrtResult.Write.xlogid, LogwrtResult.Write.xrecoff,
			 LogwrtResult.Flush.xlogid, LogwrtResult.Flush.xrecoff);
#endif

	START_CRIT_SECTION();

	/* now wait for the write lock */
	LWLockAcquire(WALWriteLock, LW_EXCLUSIVE);
	LogwrtResult = XLogCtl->Write.LogwrtResult;
	if (!XLByteLE(WriteRqstPtr, LogwrtResult.Flush))
	{
		XLogwrtRqst WriteRqst;

		WriteRqst.Write = WriteRqstPtr;
		WriteRqst.Flush = WriteRqstPtr;
		XLogWrite(WriteRqst, flexible, false);
	}
	LWLockRelease(WALWriteLock);

	END_CRIT_SECTION();
}

/*
 * Flush any previous asynchronously-committed transactions' commit records.
 *
 * NOTE: it is unwise to assume that this provides any strong guarantees.
 * In particular, because of the inexact LSN bookkeeping used by clog.c,
 * we cannot assume that hint bits will be settable for these transactions.
 */
void
XLogAsyncCommitFlush(void)
{
	XLogRecPtr	WriteRqstPtr;

	/* use volatile pointer to prevent code rearrangement */
	volatile XLogCtlData *xlogctl = XLogCtl;

	/* There's no asynchronously committed transactions during recovery */
	if (RecoveryInProgress())
		return;

	SpinLockAcquire(&xlogctl->info_lck);
	WriteRqstPtr = xlogctl->asyncCommitLSN;
	SpinLockRelease(&xlogctl->info_lck);

	XLogFlush(WriteRqstPtr);
}

/*
 * Test whether XLOG data has been flushed up to (at least) the given position.
 *
 * Returns true if a flush is still needed.  (It may be that someone else
 * is already in process of flushing that far, however.)
 */
bool
XLogNeedsFlush(XLogRecPtr record)
{
	/* XLOG doesn't need flushing during recovery */
	if (RecoveryInProgress())
		return false;

	/* Quick exit if already known flushed */
	if (XLByteLE(record, LogwrtResult.Flush))
		return false;

	/* read LogwrtResult and update local state */
	{
		/* use volatile pointer to prevent code rearrangement */
		volatile XLogCtlData *xlogctl = XLogCtl;

		SpinLockAcquire(&xlogctl->info_lck);
		LogwrtResult = xlogctl->LogwrtResult;
		SpinLockRelease(&xlogctl->info_lck);
	}

	/* check again */
	if (XLByteLE(record, LogwrtResult.Flush))
		return false;

	return true;
}

/*
 * Create a new XLOG file segment, or open a pre-existing one.
 *
 * log, seg: identify segment to be created/opened.
 *
 * *use_existent: if TRUE, OK to use a pre-existing file (else, any
 * pre-existing file will be deleted).	On return, TRUE if a pre-existing
 * file was used.
 *
 * use_lock: if TRUE, acquire ControlFileLock while moving file into
 * place.  This should be TRUE except during bootstrap log creation.  The
 * caller must *not* hold the lock at call.
 *
 * Returns FD of opened file.
 *
 * Note: errors here are ERROR not PANIC because we might or might not be
 * inside a critical section (eg, during checkpoint there is no reason to
 * take down the system on failure).  They will promote to PANIC if we are
 * in a critical section.
 */
int
XLogFileInit(uint32 log, uint32 seg,
			 bool *use_existent, bool use_lock)
{
	char		path[MAXPGPATH];
	char		tmppath[MAXPGPATH];
	char	   *zbuffer;
	uint32		installed_log;
	uint32		installed_seg;
	int			max_advance;
	int			fd;
	int			nbytes;

	XLogFilePath(path, ThisTimeLineID, log, seg);

	/*
	 * Try to use existent file (checkpoint maker may have created it already)
	 */
	if (*use_existent)
	{
		fd = BasicOpenFile(path, O_RDWR | PG_BINARY | get_sync_bit(sync_method),
						   S_IRUSR | S_IWUSR);
		if (fd < 0)
		{
			if (errno != ENOENT)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not open file \"%s\" (log file %u, segment %u): %m",
								path, log, seg)));
		}
		else
			return fd;
	}

	/*
	 * Initialize an empty (all zeroes) segment.  NOTE: it is possible that
	 * another process is doing the same thing.  If so, we will end up
	 * pre-creating an extra log segment.  That seems OK, and better than
	 * holding the lock throughout this lengthy process.
	 */
	elog(DEBUG2, "creating and filling new WAL file");

	snprintf(tmppath, MAXPGPATH, XLOGDIR "/xlogtemp.%d", (int) getpid());

	unlink(tmppath);

	/* do not use get_sync_bit here --- want to fsync only at end of fill */
	fd = BasicOpenFile(tmppath, O_RDWR | O_CREAT | O_EXCL | PG_BINARY,
					   S_IRUSR | S_IWUSR);
	if (fd < 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not create file \"%s\": %m", tmppath)));

	/*
	 * Zero-fill the file.	We have to do this the hard way to ensure that all
	 * the file space has really been allocated --- on platforms that allow
	 * "holes" in files, just seeking to the end doesn't allocate intermediate
	 * space.  This way, we know that we have all the space and (after the
	 * fsync below) that all the indirect blocks are down on disk.	Therefore,
	 * fdatasync(2) or O_DSYNC will be sufficient to sync future writes to the
	 * log file.
	 *
	 * Note: palloc zbuffer, instead of just using a local char array, to
	 * ensure it is reasonably well-aligned; this may save a few cycles
	 * transferring data to the kernel.
	 */
	zbuffer = (char *) palloc0(XLOG_BLCKSZ);
	for (nbytes = 0; nbytes < XLogSegSize; nbytes += XLOG_BLCKSZ)
	{
		errno = 0;
		if ((int) write(fd, zbuffer, XLOG_BLCKSZ) != (int) XLOG_BLCKSZ)
		{
			int			save_errno = errno;

			/*
			 * If we fail to make the file, delete it to release disk space
			 */
			unlink(tmppath);
			/* if write didn't set errno, assume problem is no disk space */
			errno = save_errno ? save_errno : ENOSPC;

			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not write to file \"%s\": %m", tmppath)));
		}
	}
	pfree(zbuffer);

	if (pg_fsync(fd) != 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not fsync file \"%s\": %m", tmppath)));

	if (close(fd))
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not close file \"%s\": %m", tmppath)));

	/*
	 * Now move the segment into place with its final name.
	 *
	 * If caller didn't want to use a pre-existing file, get rid of any
	 * pre-existing file.  Otherwise, cope with possibility that someone else
	 * has created the file while we were filling ours: if so, use ours to
	 * pre-create a future log segment.
	 */
	installed_log = log;
	installed_seg = seg;
	max_advance = XLOGfileslop;
	if (!InstallXLogFileSegment(&installed_log, &installed_seg, tmppath,
								*use_existent, &max_advance,
								use_lock))
	{
		/*
		 * No need for any more future segments, or InstallXLogFileSegment()
		 * failed to rename the file into place. If the rename failed, opening
		 * the file below will fail.
		 */
		unlink(tmppath);
	}

	/* Set flag to tell caller there was no existent file */
	*use_existent = false;

	/* Now open original target segment (might not be file I just made) */
	fd = BasicOpenFile(path, O_RDWR | PG_BINARY | get_sync_bit(sync_method),
					   S_IRUSR | S_IWUSR);
	if (fd < 0)
		ereport(ERROR,
				(errcode_for_file_access(),
		   errmsg("could not open file \"%s\" (log file %u, segment %u): %m",
				  path, log, seg)));

	elog(DEBUG2, "done creating and filling new WAL file");

	return fd;
}

/*
 * Create a new XLOG file segment by copying a pre-existing one.
 *
 * log, seg: identify segment to be created.
 *
 * srcTLI, srclog, srcseg: identify segment to be copied (could be from
 *		a different timeline)
 *
 * Currently this is only used during recovery, and so there are no locking
 * considerations.	But we should be just as tense as XLogFileInit to avoid
 * emplacing a bogus file.
 */
static void
XLogFileCopy(uint32 log, uint32 seg,
			 TimeLineID srcTLI, uint32 srclog, uint32 srcseg)
{
	char		path[MAXPGPATH];
	char		tmppath[MAXPGPATH];
	char		buffer[XLOG_BLCKSZ];
	int			srcfd;
	int			fd;
	int			nbytes;

	/*
	 * Open the source file
	 */
	XLogFilePath(path, srcTLI, srclog, srcseg);
	srcfd = BasicOpenFile(path, O_RDONLY | PG_BINARY, 0);
	if (srcfd < 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not open file \"%s\": %m", path)));

	/*
	 * Copy into a temp file name.
	 */
	snprintf(tmppath, MAXPGPATH, XLOGDIR "/xlogtemp.%d", (int) getpid());
	unlink(tmppath);

	/* do not use get_sync_bit() here --- want to fsync only at end of fill */
	fd = BasicOpenFile(tmppath, O_RDWR | O_CREAT | O_EXCL | PG_BINARY,
					   S_IRUSR | S_IWUSR);
	if (fd < 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not create file \"%s\": %m", tmppath)));

	/*
	 * Do the data copying.
	 */
	for (nbytes = 0; nbytes < XLogSegSize; nbytes += sizeof(buffer))
	{
		errno = 0;
		if ((int) read(srcfd, buffer, sizeof(buffer)) != (int) sizeof(buffer))
		{
			if (errno != 0)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not read file \"%s\": %m", path)));
			else
				ereport(ERROR,
						(errmsg("not enough data in file \"%s\"", path)));
		}
		errno = 0;
		if ((int) write(fd, buffer, sizeof(buffer)) != (int) sizeof(buffer))
		{
			int			save_errno = errno;

			/*
			 * If we fail to make the file, delete it to release disk space
			 */
			unlink(tmppath);
			/* if write didn't set errno, assume problem is no disk space */
			errno = save_errno ? save_errno : ENOSPC;

			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not write to file \"%s\": %m", tmppath)));
		}
	}

	if (pg_fsync(fd) != 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not fsync file \"%s\": %m", tmppath)));

	if (close(fd))
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not close file \"%s\": %m", tmppath)));

	close(srcfd);

	/*
	 * Now move the segment into place with its final name.
	 */
	if (!InstallXLogFileSegment(&log, &seg, tmppath, false, NULL, false))
		elog(ERROR, "InstallXLogFileSegment should not have failed");
}

/*
 * Install a new XLOG segment file as a current or future log segment.
 *
 * This is used both to install a newly-created segment (which has a temp
 * filename while it's being created) and to recycle an old segment.
 *
 * *log, *seg: identify segment to install as (or first possible target).
 * When find_free is TRUE, these are modified on return to indicate the
 * actual installation location or last segment searched.
 *
 * tmppath: initial name of file to install.  It will be renamed into place.
 *
 * find_free: if TRUE, install the new segment at the first empty log/seg
 * number at or after the passed numbers.  If FALSE, install the new segment
 * exactly where specified, deleting any existing segment file there.
 *
 * *max_advance: maximum number of log/seg slots to advance past the starting
 * point.  Fail if no free slot is found in this range.  On return, reduced
 * by the number of slots skipped over.  (Irrelevant, and may be NULL,
 * when find_free is FALSE.)
 *
 * use_lock: if TRUE, acquire ControlFileLock while moving file into
 * place.  This should be TRUE except during bootstrap log creation.  The
 * caller must *not* hold the lock at call.
 *
 * Returns TRUE if the file was installed successfully.  FALSE indicates that
 * max_advance limit was exceeded, or an error occurred while renaming the
 * file into place.
 */
static bool
InstallXLogFileSegment(uint32 *log, uint32 *seg, char *tmppath,
					   bool find_free, int *max_advance,
					   bool use_lock)
{
	char		path[MAXPGPATH];
	struct stat stat_buf;

	XLogFilePath(path, ThisTimeLineID, *log, *seg);

	/*
	 * We want to be sure that only one process does this at a time.
	 */
	if (use_lock)
		LWLockAcquire(ControlFileLock, LW_EXCLUSIVE);

	if (!find_free)
	{
		/* Force installation: get rid of any pre-existing segment file */
		unlink(path);
	}
	else
	{
		/* Find a free slot to put it in */
		while (stat(path, &stat_buf) == 0)
		{
			if (*max_advance <= 0)
			{
				/* Failed to find a free slot within specified range */
				if (use_lock)
					LWLockRelease(ControlFileLock);
				return false;
			}
			NextLogSeg(*log, *seg);
			(*max_advance)--;
			XLogFilePath(path, ThisTimeLineID, *log, *seg);
		}
	}

	/*
	 * Prefer link() to rename() here just to be really sure that we don't
	 * overwrite an existing logfile.  However, there shouldn't be one, so
	 * rename() is an acceptable substitute except for the truly paranoid.
	 */
#if HAVE_WORKING_LINK
	if (link(tmppath, path) < 0)
	{
		if (use_lock)
			LWLockRelease(ControlFileLock);
		ereport(LOG,
				(errcode_for_file_access(),
				 errmsg("could not link file \"%s\" to \"%s\" (initialization of log file %u, segment %u): %m",
						tmppath, path, *log, *seg)));
		return false;
	}
	unlink(tmppath);
#else
	if (rename(tmppath, path) < 0)
	{
		if (use_lock)
			LWLockRelease(ControlFileLock);
		ereport(LOG,
				(errcode_for_file_access(),
				 errmsg("could not rename file \"%s\" to \"%s\" (initialization of log file %u, segment %u): %m",
						tmppath, path, *log, *seg)));
		return false;
	}
#endif

	if (use_lock)
		LWLockRelease(ControlFileLock);

	return true;
}

/*
 * Open a pre-existing logfile segment for writing.
 */
int
XLogFileOpen(uint32 log, uint32 seg)
{
	char		path[MAXPGPATH];
	int			fd;

	XLogFilePath(path, ThisTimeLineID, log, seg);

	fd = BasicOpenFile(path, O_RDWR | PG_BINARY | get_sync_bit(sync_method),
					   S_IRUSR | S_IWUSR);
	if (fd < 0)
		ereport(PANIC,
				(errcode_for_file_access(),
		   errmsg("could not open file \"%s\" (log file %u, segment %u): %m",
				  path, log, seg)));

	return fd;
}

/*
 * Close the current logfile segment for writing.
 */
static void
XLogFileClose(void)
{
	Assert(openLogFile >= 0);

	/*
	 * WAL segment files will not be re-read in normal operation, so we advise
	 * the OS to release any cached pages.	But do not do so if WAL archiving
	 * is active, because archiver process could use the cache to read the WAL
	 * segment.
	 */
#if defined(USE_POSIX_FADVISE) && defined(POSIX_FADV_DONTNEED)
	if (!XLogIsNeeded())
		(void) posix_fadvise(openLogFile, 0, 0, POSIX_FADV_DONTNEED);
#endif

	if (close(openLogFile))
		ereport(PANIC,
				(errcode_for_file_access(),
				 errmsg("could not close log file %u, segment %u: %m",
						openLogId, openLogSeg)));
	openLogFile = -1;
}

#if 0 /* GPDB doesn't make use of this function */
/*
 * Attempt to retrieve the specified file from off-line archival storage.
 * If successful, fill "path" with its complete path (note that this will be
 * a temp file name that doesn't follow the normal naming convention), and
 * return TRUE.
 *
 * If not successful, fill "path" with the name of the normal on-line file
 * (which may or may not actually exist, but we'll try to use it), and return
 * FALSE.
 *
 * For fixed-size files, the caller may pass the expected size as an
 * additional crosscheck on successful recovery.  If the file size is not
 * known, set expectedSize = 0.
 */
static bool
RestoreArchivedFile(char *path, const char *xlogfname,
					const char *recovername, off_t expectedSize)
{
	char		xlogpath[MAXPGPATH];
	char		xlogRestoreCmd[MAXPGPATH];
	char		lastRestartPointFname[MAXPGPATH];
	char	   *dp;
	char	   *endp;
	const char *sp;
	int			rc;
	bool		signaled;
	struct stat stat_buf;
	uint32		restartLog;
	uint32		restartSeg;

	/*
	 * When doing archive recovery, we always prefer an archived log file even
	 * if a file of the same name exists in XLOGDIR.  The reason is that the
	 * file in XLOGDIR could be an old, un-filled or partly-filled version
	 * that was copied and restored as part of backing up $PGDATA.
	 *
	 * We could try to optimize this slightly by checking the local copy
	 * lastchange timestamp against the archived copy, but we have no API to
	 * do this, nor can we guarantee that the lastchange timestamp was
	 * preserved correctly when we copied to archive. Our aim is robustness,
	 * so we elect not to do this.
	 *
	 * If we cannot obtain the log file from the archive, however, we will try
	 * to use the XLOGDIR file if it exists.  This is so that we can make use
	 * of log segments that weren't yet transferred to the archive.
	 *
	 * Notice that we don't actually overwrite any files when we copy back
	 * from archive because the recoveryRestoreCommand may inadvertently
	 * restore inappropriate xlogs, or they may be corrupt, so we may wish to
	 * fallback to the segments remaining in current XLOGDIR later. The
	 * copy-from-archive filename is always the same, ensuring that we don't
	 * run out of disk space on long recoveries.
	 */
	snprintf(xlogpath, MAXPGPATH, XLOGDIR "/%s", recovername);

	/*
	 * Make sure there is no existing file named recovername.
	 */
	if (stat(xlogpath, &stat_buf) != 0)
	{
		if (errno != ENOENT)
			ereport(FATAL,
					(errcode_for_file_access(),
					 errmsg("could not stat file \"%s\": %m",
							xlogpath)));
	}
	else
	{
		if (unlink(xlogpath) != 0)
			ereport(FATAL,
					(errcode_for_file_access(),
					 errmsg("could not remove file \"%s\": %m",
							xlogpath)));
	}

	/*
	 * Calculate the archive file cutoff point for use during log shipping
	 * replication. All files earlier than this point can be deleted from the
	 * archive, though there is no requirement to do so.
	 *
	 * We initialise this with the filename of an InvalidXLogRecPtr, which
	 * will prevent the deletion of any WAL files from the archive because of
	 * the alphabetic sorting property of WAL filenames.
	 *
	 * Once we have successfully located the redo pointer of the checkpoint
	 * from which we start recovery we never request a file prior to the redo
	 * pointer of the last restartpoint. When redo begins we know that we have
	 * successfully located it, so there is no need for additional status
	 * flags to signify the point when we can begin deleting WAL files from
	 * the archive.
	 */
	if (InRedo)
	{
		XLByteToSeg(ControlFile->checkPointCopy.redo,
					restartLog, restartSeg);
		XLogFileName(lastRestartPointFname,
					 ControlFile->checkPointCopy.ThisTimeLineID,
					 restartLog, restartSeg);
		/* we shouldn't need anything earlier than last restart point */
		Assert(strcmp(lastRestartPointFname, xlogfname) <= 0);
	}
	else
		XLogFileName(lastRestartPointFname, 0, 0, 0);

	/*
	 * construct the command to be executed
	 */
	dp = xlogRestoreCmd;
	endp = xlogRestoreCmd + MAXPGPATH - 1;
	*endp = '\0';

	for (sp = recoveryRestoreCommand; *sp; sp++)
	{
		if (*sp == '%')
		{
			switch (sp[1])
			{
				case 'p':
					/* %p: relative path of target file */
					sp++;
					StrNCpy(dp, xlogpath, endp - dp);
					make_native_path(dp);
					dp += strlen(dp);
					break;
				case 'f':
					/* %f: filename of desired file */
					sp++;
					StrNCpy(dp, xlogfname, endp - dp);
					dp += strlen(dp);
					break;
				case 'r':
					/* %r: filename of last restartpoint */
					sp++;
					StrNCpy(dp, lastRestartPointFname, endp - dp);
					dp += strlen(dp);
					break;
				case '%':
					/* convert %% to a single % */
					sp++;
					if (dp < endp)
						*dp++ = *sp;
					break;
				default:
					/* otherwise treat the % as not special */
					if (dp < endp)
						*dp++ = *sp;
					break;
			}
		}
		else
		{
			if (dp < endp)
				*dp++ = *sp;
		}
	}
	*dp = '\0';

	ereport(DEBUG3,
			(errmsg_internal("executing restore command \"%s\"",
							 xlogRestoreCmd)));

	/*
	 * Set in_restore_command to tell the signal handler that we should exit
	 * right away on SIGTERM. We know that we're at a safe point to do that.
	 * Check if we had already received the signal, so that we don't miss a
	 * shutdown request received just before this.
	 */
	in_restore_command = true;
	if (shutdown_requested)
		proc_exit(1);

	/*
	 * Copy xlog from archival storage to XLOGDIR
	 */
	rc = system(xlogRestoreCmd);

	in_restore_command = false;

	if (rc == 0)
	{
		/*
		 * command apparently succeeded, but let's make sure the file is
		 * really there now and has the correct size.
		 *
		 * XXX I made wrong-size a fatal error to ensure the DBA would notice
		 * it, but is that too strong?	We could try to plow ahead with a
		 * local copy of the file ... but the problem is that there probably
		 * isn't one, and we'd incorrectly conclude we've reached the end of
		 * WAL and we're done recovering ...
		 */
		if (stat(xlogpath, &stat_buf) == 0)
		{
			if (expectedSize > 0 && stat_buf.st_size != expectedSize)
				ereport(FATAL,
						(errmsg("archive file \"%s\" has wrong size: %lu instead of %lu",
								xlogfname,
								(unsigned long) stat_buf.st_size,
								(unsigned long) expectedSize)));
			else
			{
				ereport(LOG,
						(errmsg("restored log file \"%s\" from archive",
								xlogfname)));
				strcpy(path, xlogpath);
				return true;
			}
		}
		else
		{
			/* stat failed */
			if (errno != ENOENT)
				ereport(FATAL,
						(errcode_for_file_access(),
						 errmsg("could not stat file \"%s\": %m",
								xlogpath)));
		}
	}

	/*
	 * Remember, we rollforward UNTIL the restore fails so failure here is
	 * just part of the process... that makes it difficult to determine
	 * whether the restore failed because there isn't an archive to restore,
	 * or because the administrator has specified the restore program
	 * incorrectly.  We have to assume the former.
	 *
	 * However, if the failure was due to any sort of signal, it's best to
	 * punt and abort recovery.  (If we "return false" here, upper levels will
	 * assume that recovery is complete and start up the database!) It's
	 * essential to abort on child SIGINT and SIGQUIT, because per spec
	 * system() ignores SIGINT and SIGQUIT while waiting; if we see one of
	 * those it's a good bet we should have gotten it too.
	 *
	 * On SIGTERM, assume we have received a fast shutdown request, and exit
	 * cleanly. It's pure chance whether we receive the SIGTERM first, or the
	 * child process. If we receive it first, the signal handler will call
	 * proc_exit, otherwise we do it here. If we or the child process received
	 * SIGTERM for any other reason than a fast shutdown request, postmaster
	 * will perform an immediate shutdown when it sees us exiting
	 * unexpectedly.
	 *
	 * Per the Single Unix Spec, shells report exit status > 128 when a called
	 * command died on a signal.  Also, 126 and 127 are used to report
	 * problems such as an unfindable command; treat those as fatal errors
	 * too.
	 */
	if (WIFSIGNALED(rc) && WTERMSIG(rc) == SIGTERM)
		proc_exit(1);

	signaled = WIFSIGNALED(rc) || WEXITSTATUS(rc) > 125;

	ereport(signaled ? FATAL : DEBUG2,
		(errmsg("could not restore file \"%s\" from archive: return code %d",
				xlogfname, rc)));

	/*
	 * if an archived file is not available, there might still be a version of
	 * this file in XLOGDIR, so return that as the filename to open.
	 *
	 * In many recovery scenarios we expect this to fail also, but if so that
	 * just means we've reached the end of WAL.
	 */
	snprintf(path, MAXPGPATH, XLOGDIR "/%s", xlogfname);
	return false;
}
#endif

#ifdef NOT_USED
/*
 * Attempt to execute the recovery_end_command.
 */
static void
ExecuteRecoveryEndCommand(void)
{
	char		xlogRecoveryEndCmd[MAXPGPATH];
	char		lastRestartPointFname[MAXPGPATH];
	char	   *dp;
	char	   *endp;
	const char *sp;
	int			rc;
	bool		signaled;
	uint32		restartLog;
	uint32		restartSeg;

	Assert(recoveryEndCommand);

	/*
	 * Calculate the archive file cutoff point for use during log shipping
	 * replication. All files earlier than this point can be deleted from the
	 * archive, though there is no requirement to do so.
	 *
	 * We initialise this with the filename of an InvalidXLogRecPtr, which
	 * will prevent the deletion of any WAL files from the archive because of
	 * the alphabetic sorting property of WAL filenames.
	 *
	 * Once we have successfully located the redo pointer of the checkpoint
	 * from which we start recovery we never request a file prior to the redo
	 * pointer of the last restartpoint. When redo begins we know that we have
	 * successfully located it, so there is no need for additional status
	 * flags to signify the point when we can begin deleting WAL files from
	 * the archive.
	 */
	if (InRedo)
	{
		XLByteToSeg(ControlFile->checkPointCopy.redo,
					restartLog, restartSeg);
		XLogFileName(lastRestartPointFname,
					 ControlFile->checkPointCopy.ThisTimeLineID,
					 restartLog, restartSeg);
	}
	else
		XLogFileName(lastRestartPointFname, 0, 0, 0);

	/*
	 * construct the command to be executed
	 */
	dp = xlogRecoveryEndCmd;
	endp = xlogRecoveryEndCmd + MAXPGPATH - 1;
	*endp = '\0';

	for (sp = recoveryEndCommand; *sp; sp++)
	{
		if (*sp == '%')
		{
			switch (sp[1])
			{
				case 'r':
					/* %r: filename of last restartpoint */
					sp++;
					StrNCpy(dp, lastRestartPointFname, endp - dp);
					dp += strlen(dp);
					break;
				case '%':
					/* convert %% to a single % */
					sp++;
					if (dp < endp)
						*dp++ = *sp;
					break;
				default:
					/* otherwise treat the % as not special */
					if (dp < endp)
						*dp++ = *sp;
					break;
			}
		}
		else
		{
			if (dp < endp)
				*dp++ = *sp;
		}
	}
	*dp = '\0';

	ereport(DEBUG3,
			(errmsg_internal("executing recovery end command \"%s\"",
							 xlogRecoveryEndCmd)));

	/*
	 * execute the constructed command
	 */
	rc = system(xlogRecoveryEndCmd);
	if (rc != 0)
	{
		/*
		 * If the failure was due to any sort of signal, it's best to punt and
		 * abort recovery. See also detailed comments on signals in
		 * RestoreArchivedFile().
		 */
		signaled = WIFSIGNALED(rc) || WEXITSTATUS(rc) > 125;

		ereport(signaled ? FATAL : WARNING,
				(errmsg("recovery_end_command \"%s\": return code %d",
						xlogRecoveryEndCmd, rc)));
	}
}
#endif

/*
 * Preallocate log files beyond the specified log endpoint.
 *
 * XXX this is currently extremely conservative, since it forces only one
 * future log segment to exist, and even that only if we are 75% done with
 * the current one.  This is only appropriate for very low-WAL-volume systems.
 * High-volume systems will be OK once they've built up a sufficient set of
 * recycled log segments, but the startup transient is likely to include
 * a lot of segment creations by foreground processes, which is not so good.
 */
static void
PreallocXlogFiles(XLogRecPtr endptr)
{
	uint32		_logId;
	uint32		_logSeg;
	int			lf;
	bool		use_existent;

	XLByteToPrevSeg(endptr, _logId, _logSeg);
	if ((endptr.xrecoff - 1) % XLogSegSize >=
		(uint32) (0.75 * XLogSegSize))
	{
		NextLogSeg(_logId, _logSeg);
		use_existent = true;
		lf = XLogFileInit(_logId, _logSeg, &use_existent, true);
		close(lf);
		if (!use_existent)
			CheckpointStats.ckpt_segs_added++;
	}
}

/*
 * Get the log/seg of the latest removed or recycled WAL segment.
 * Returns 0/0 if no WAL segments have been removed since startup.
 */
void
XLogGetLastRemoved(uint32 *log, uint32 *seg)
{
	/* use volatile pointer to prevent code rearrangement */
	volatile XLogCtlData *xlogctl = XLogCtl;

	SpinLockAcquire(&xlogctl->info_lck);
	*log = xlogctl->lastRemovedLog;
	*seg = xlogctl->lastRemovedSeg;
	SpinLockRelease(&xlogctl->info_lck);
}

/*
 * Update the last removed log/seg pointer in shared memory, to reflect
 * that the given XLOG file has been removed.
 */
static void
UpdateLastRemovedPtr(char *filename)
{
	/* use volatile pointer to prevent code rearrangement */
	volatile XLogCtlData *xlogctl = XLogCtl;
	uint32		tli,
				log,
				seg;

	XLogFromFileName(filename, &tli, &log, &seg);

	SpinLockAcquire(&xlogctl->info_lck);
	if (log > xlogctl->lastRemovedLog ||
		(log == xlogctl->lastRemovedLog && seg > xlogctl->lastRemovedSeg))
	{
		xlogctl->lastRemovedLog = log;
		xlogctl->lastRemovedSeg = seg;
	}
	SpinLockRelease(&xlogctl->info_lck);
}

/*
 * Recycle or remove all log files older or equal to passed log/seg#
 *
 * endptr is current (or recent) end of xlog; this is used to determine
 * whether we want to recycle rather than delete no-longer-wanted log files.
 */
static void
RemoveOldXlogFiles(uint32 log, uint32 seg, XLogRecPtr endptr)
{
	uint32		endlogId;
	uint32		endlogSeg;
	int			max_advance;
	DIR		   *xldir;
	struct dirent *xlde;
	char		lastoff[MAXFNAMELEN];
	char		path[MAXPGPATH];
#ifdef WIN32
	char		newpath[MAXPGPATH];
#endif
	struct stat statbuf;

	/*
	 * Initialize info about where to try to recycle to.  We allow recycling
	 * segments up to XLOGfileslop segments beyond the current XLOG location.
	 */
	XLByteToPrevSeg(endptr, endlogId, endlogSeg);
	max_advance = XLOGfileslop;

	xldir = AllocateDir(XLOGDIR);
	if (xldir == NULL)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not open transaction log directory \"%s\": %m",
						XLOGDIR)));

	XLogFileName(lastoff, ThisTimeLineID, log, seg);

	while ((xlde = ReadDir(xldir, XLOGDIR)) != NULL)
	{
		/*
		 * We ignore the timeline part of the XLOG segment identifiers in
		 * deciding whether a segment is still needed.	This ensures that we
		 * won't prematurely remove a segment from a parent timeline. We could
		 * probably be a little more proactive about removing segments of
		 * non-parent timelines, but that would be a whole lot more
		 * complicated.
		 *
		 * We use the alphanumeric sorting property of the filenames to decide
		 * which ones are earlier than the lastoff segment.
		 */
		if (strlen(xlde->d_name) == 24 &&
			strspn(xlde->d_name, "0123456789ABCDEF") == 24 &&
			strcmp(xlde->d_name + 8, lastoff + 8) <= 0)
		{
			if (XLogArchiveCheckDone(xlde->d_name))
			{
				snprintf(path, MAXPGPATH, XLOGDIR "/%s", xlde->d_name);

				/* Update the last removed location in shared memory first */
				UpdateLastRemovedPtr(xlde->d_name);

				/*
				 * Before deleting the file, see if it can be recycled as a
				 * future log segment. Only recycle normal files, pg_standby
				 * for example can create symbolic links pointing to a
				 * separate archive directory.
				 */
				if (lstat(path, &statbuf) == 0 && S_ISREG(statbuf.st_mode) &&
					InstallXLogFileSegment(&endlogId, &endlogSeg, path,
										   true, &max_advance, true))
				{
					ereport(DEBUG2,
							(errmsg("recycled transaction log file \"%s\"",
									xlde->d_name)));
					CheckpointStats.ckpt_segs_recycled++;
					/* Needn't recheck that slot on future iterations */
					if (max_advance > 0)
					{
						NextLogSeg(endlogId, endlogSeg);
						max_advance--;
					}
				}
				else
				{
					/* No need for any more future segments... */
					int rc = 0;

					ereport(DEBUG2,
							(errmsg("removing transaction log file \"%s\"",
									xlde->d_name)));

#ifdef WIN32
					/*
					 * On Windows, if another process (e.g another backend)
					 * holds the file open in FILE_SHARE_DELETE mode, unlink
					 * will succeed, but the file will still show up in
					 * directory listing until the last handle is closed.
					 * To avoid confusing the lingering deleted file for a
					 * live WAL file that needs to be archived, rename it
					 * before deleting it.
					 *
					 * If another process holds the file open without
					 * FILE_SHARE_DELETE flag, rename will fail. We'll try
					 * again at the next checkpoint.
					 */
					snprintf(newpath, MAXPGPATH, "%s.deleted", path);
					if (rename(path, newpath) != 0)
					{
						ereport(LOG,
								(errcode_for_file_access(),
								 errmsg("could not rename old transaction log file \"%s\": %m",
										path)));
						continue;
					}
					snprintf(newpath, MAXPGPATH, "%s.deleted", path);
					if (rename(path, newpath) != 0)
					{
						ereport(LOG,
								(errcode_for_file_access(),
								 errmsg("could not rename old transaction log file \"%s\": %m",
										path)));
						continue;
					}
					rc = unlink(newpath);
#else
					rc = unlink(path);
#endif
					if (rc != 0)
					{
						ereport(LOG,
								(errcode_for_file_access(),
								 errmsg("could not remove old transaction log file \"%s\": %m",
										path)));
						continue;
					}
					CheckpointStats.ckpt_segs_removed++;
				}

				XLogArchiveCleanup(xlde->d_name);
			}
		}
	}

	FreeDir(xldir);
}

/*
 * Remove previous backup history files.  This also retries creation of
 * .ready files for any backup history files for which XLogArchiveNotify
 * failed earlier.
 */
static void
CleanupBackupHistory(void)
{
	DIR		   *xldir;
	struct dirent *xlde;
	char		path[MAXPGPATH];

	xldir = AllocateDir(XLOGDIR);
	if (xldir == NULL)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not open transaction log directory \"%s\": %m",
						XLOGDIR)));

	while ((xlde = ReadDir(xldir, XLOGDIR)) != NULL)
	{
		if (strlen(xlde->d_name) > 24 &&
			strspn(xlde->d_name, "0123456789ABCDEF") == 24 &&
			strcmp(xlde->d_name + strlen(xlde->d_name) - strlen(".backup"),
				   ".backup") == 0)
		{
			if (XLogArchiveCheckDone(xlde->d_name))
			{
				ereport(DEBUG2,
				(errmsg("removing transaction log backup history file \"%s\"",
						xlde->d_name)));
				snprintf(path, MAXPGPATH, XLOGDIR "/%s", xlde->d_name);
				unlink(path);
				XLogArchiveCleanup(xlde->d_name);
			}
		}
	}

	FreeDir(xldir);
}

/*
 * Restore the backup blocks present in an XLOG record, if any.
 *
 * We assume all of the record has been read into memory at *record.
 *
 * Note: when a backup block is available in XLOG, we restore it
 * unconditionally, even if the page in the database appears newer.
 * This is to protect ourselves against database pages that were partially
 * or incorrectly written during a crash.  We assume that the XLOG data
 * must be good because it has passed a CRC check, while the database
 * page might not be.  This will force us to replay all subsequent
 * modifications of the page that appear in XLOG, rather than possibly
 * ignoring them as already applied, but that's not a huge drawback.
 *
 * If 'cleanup' is true, a cleanup lock is used when restoring blocks.
 * Otherwise, a normal exclusive lock is used.	At the moment, that's just
 * pro forma, because there can't be any regular backends in the system
 * during recovery.  The 'cleanup' argument applies to all backup blocks
 * in the WAL record, that suffices for now.
 */
void
RestoreBkpBlocks(XLogRecPtr lsn, XLogRecord *record, bool cleanup)
{
	BkpBlock	bkpb;
	char	   *blk;
	int			i;

	blk = (char *) XLogRecGetData(record) + record->xl_len;
	for (i = 0; i < XLR_MAX_BKP_BLOCKS; i++)
	{
		if (!(record->xl_info & XLR_SET_BKP_BLOCK(i)))
			continue;

		memcpy(&bkpb, blk, sizeof(BkpBlock));
		blk += sizeof(BkpBlock);

		/* get_cleanup_lock is ignored in GPDB */
		RestoreBackupBlockContents(lsn, bkpb, blk, false, false);

		blk += BLCKSZ - bkpb.hole_length;
	}
}

/*
 * Workhorse for RestoreBackupBlock usable without an xlog record
 *
 * Restores a full-page image from BkpBlock and a data pointer.
 */
static void
RestoreBackupBlockContents(XLogRecPtr lsn, BkpBlock bkpb, char *blk,
						   bool get_cleanup_lock, bool keep_buffer)
{
	Buffer		buffer;
	Page		page;

	if (! (bkpb.block_info & BLOCK_APPLY))
		return;

	buffer = XLogReadBuffer(bkpb.node, bkpb.block, true);
	Assert(BufferIsValid(buffer));
#if 0 /* upstream merge */
	if (get_cleanup_lock)
		LockBufferForCleanup(buffer);
	else
		LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);
#endif

	page = (Page) BufferGetPage(buffer);

	if (bkpb.hole_length == 0)
	{
		memcpy((char *) page, blk, BLCKSZ);
	}
	else
	{
		memcpy((char *) page, blk, bkpb.hole_offset);
		/* must zero-fill the hole */
		MemSet((char *) page + bkpb.hole_offset, 0, bkpb.hole_length);
		memcpy((char *) page + (bkpb.hole_offset + bkpb.hole_length),
			   blk + bkpb.hole_offset,
			   BLCKSZ - (bkpb.hole_offset + bkpb.hole_length));
	}

	/*
	 * The checksum value on this page is currently invalid. We don't
	 * need to reset it here since it will be set before being written.
	 */

	PageSetLSN(page, lsn);
	MarkBufferDirty(buffer);

	if (!keep_buffer)
		UnlockReleaseBuffer(buffer);

	return;
}

bool
IsBkpBlockApplied(XLogRecord *record, uint8 block_id)
{
	BkpBlock	bkpb;
	char	   *blk;
	int			i;

	Assert(block_id < XLR_MAX_BKP_BLOCKS);

	blk = (char *) XLogRecGetData(record) + record->xl_len;
	for (i = 0; i <= block_id; i++)
	{
		if (!(record->xl_info & XLR_SET_BKP_BLOCK(i)))
			continue;

		memcpy(&bkpb, blk, sizeof(BkpBlock));
		blk += sizeof(BkpBlock);

		if (i == block_id)
			return (bkpb.block_info & BLOCK_APPLY) != 0;

		blk += BLCKSZ - bkpb.hole_length;
	}

	return false;
}

/*
 * CRC-check an XLOG record.  We do not believe the contents of an XLOG
 * record (other than to the minimal extent of computing the amount of
 * data to read in) until we've checked the CRCs.
 *
 * We assume all of the record has been read into memory at *record.
 */
static bool
RecordIsValid(XLogRecord *record, XLogRecPtr recptr, int emode)
{
	pg_crc32	crc;
	int			i;
	uint32		len = record->xl_len;
	BkpBlock	bkpb;
	char	   *blk;

	/*
	 * Calculate the crc using the new fast crc32c algorithm
	 */

	/* First the rmgr data */
	INIT_CRC32C(crc);
	COMP_CRC32C(crc, XLogRecGetData(record), len);

	/* Add in the backup blocks, if any */
	blk = (char *) XLogRecGetData(record) + len;
	for (i = 0; i < XLR_MAX_BKP_BLOCKS; i++)
	{
		uint32		blen;

		if (!(record->xl_info & XLR_SET_BKP_BLOCK(i)))
			continue;

		memcpy(&bkpb, blk, sizeof(BkpBlock));
		if (bkpb.hole_offset + bkpb.hole_length > BLCKSZ)
		{
			ereport(emode,
					(errmsg("incorrect hole size in record at %X/%X",
							recptr.xlogid, recptr.xrecoff)));
			return false;
		}
		blen = sizeof(BkpBlock) + BLCKSZ - bkpb.hole_length;
		COMP_CRC32C(crc, blk, blen);
		blk += blen;
	}

	/* Check that xl_tot_len agrees with our calculation */
	if (blk != (char *) record + record->xl_tot_len)
	{
		ereport(emode,
				(errmsg("incorrect total length in record at %X/%X",
						recptr.xlogid, recptr.xrecoff)));
		return false;
	}

	/* Finally include the record header */
	COMP_CRC32C(crc, (char *) record + sizeof(pg_crc32),
			   SizeOfXLogRecord - sizeof(pg_crc32));
	FIN_CRC32C(crc);

	if (!EQ_CRC32C(record->xl_crc, crc))
	{
		ereport(emode,
		(errmsg("incorrect resource manager data checksum in record at %X/%X",
				recptr.xlogid, recptr.xrecoff)));
		return false;
	}

	return true;
}

/*
 * Verify whether pg_xlog exists
 *
 * It is not the goal of this function to verify the contents of these
 * directories, but to help in cases where someone has performed a
 * copy but omitted pg_xlog from the copy.
 */
static void
ValidateXLOGDirectoryStructure(void)
{
	char		path[MAXPGPATH];
	struct stat stat_buf;

	/* Check for pg_xlog; if it doesn't exist, error out */
	if (stat(XLOGDIR, &stat_buf) != 0 ||
			!S_ISDIR(stat_buf.st_mode))
			ereport(FATAL,
					(errmsg("required WAL directory \"%s\" does not exist",
							XLOGDIR)));

	/* Check for archive_status */
	snprintf(path, MAXPGPATH, XLOGDIR "/archive_status");
	if (stat(path, &stat_buf) == 0)
	{
		/* Check for weird cases where it exists but isn't a directory */
		if (!S_ISDIR(stat_buf.st_mode))
			ereport(FATAL, 
					(errmsg("required WAL directory \"%s\" does not exist",
							path)));
	}
	else
	{
		ereport(LOG,
				(errmsg("creating missing WAL directory \"%s\"", path)));
		if (mkdir(path, 0700) < 0)
			ereport(FATAL, 
					(errmsg("could not create missing directory \"%s\": %m",
							path)));
	}
}

/*
 * Open a logfile segment for reading (during recovery).
 * It's assumed to be already available in pg_xlog.
 */
static int
XLogFileRead(uint32 log, uint32 seg, int emode, TimeLineID tli,
			 int source, bool notfoundOk)
{
	char		xlogfname[MAXFNAMELEN];
	char		activitymsg[MAXFNAMELEN + 16];
	char		path[MAXPGPATH];
	int			fd;

	XLogFileName(xlogfname, tli, log, seg);

	switch (source)
	{
		case XLOG_FROM_PG_XLOG:
		case XLOG_FROM_STREAM:
			XLogFilePath(path, tli, log, seg);
			restoredFromArchive = false;
			break;

		default:
			elog(ERROR, "invalid XLogFileRead source %d", source);
	}

	elogif(debug_xlog_record_read, LOG,
		   "xlog file read -- File read request with log %u, seg %u,"
		   "tli %u, source = %s, notfoundok = %s",
		   log, seg, (uint32) tli,
		   source == XLOG_FROM_PG_XLOG ? "xlog" : "stream",
		   notfoundOk ? "true" : "false");

	fd = BasicOpenFile(path, O_RDONLY | PG_BINARY, 0);
	if (fd >= 0)
	{
		/* Success! */
		curFileTLI = tli;

		/*
		 * Report recovery progress in PS display, if we are in
		 * startup process.  There are more cases like Filerep recovery
		 * and Prepare phase where we don't want to report it.
		 */
		if (am_startup)
		{
			snprintf(activitymsg, sizeof(activitymsg), "recovering %s",
					 xlogfname);
			set_ps_display(activitymsg, false);
		}

		/* Track source of data in assorted state variables */
		readSource = source;
		XLogReceiptSource = source;
		/* In FROM_STREAM case, caller tracks receipt time, not me */
		if (source != XLOG_FROM_STREAM)
			XLogReceiptTime = GetCurrentTimestamp();

		elogif(debug_xlog_record_read, LOG,
			   "xlog file read -- Read file %s (log %u, seg %u)",
			   path, log, seg);

		return fd;
	}

	if (errno != ENOENT || !notfoundOk) /* unexpected failure? */
		ereport(PANIC,
				(errcode_for_file_access(),
		   errmsg("could not open file \"%s\" (log file %u, segment %u): %m",
				  path, log, seg)));

	elogif(debug_xlog_record_read, LOG,
		   "xlog file read -- Couldn't read file %s (log %u, seg %u)",
		   path, log, seg);
	return -1;
}


/*
 * Open a logfile segment for reading (during recovery).
 *
 * This version searches for the segment with any TLI listed in expectedTLIs.
 */
static int
XLogFileReadAnyTLI(uint32 log, uint32 seg, int emode, int sources)
{
	char		path[MAXPGPATH];
	ListCell   *cell;
	int			fd;

	/*
	 * Loop looking for a suitable timeline ID: we might need to read any of
	 * the timelines listed in expectedTLIs.
	 *
	 * We expect curFileTLI on entry to be the TLI of the preceding file in
	 * sequence, or 0 if there was no predecessor.	We do not allow curFileTLI
	 * to go backwards; this prevents us from picking up the wrong file when a
	 * parent timeline extends to higher segment numbers than the child we
	 * want to read.
	 */
	foreach(cell, expectedTLIs)
	{
		TimeLineID	tli = (TimeLineID) lfirst_int(cell);

		if (tli < curFileTLI)
			break;				/* don't bother looking at too-old TLIs */

		if (sources & XLOG_FROM_PG_XLOG)
		{
			elogif(debug_xlog_record_read, LOG,
				   "xlog file read (tli) -- requesting a file read (log %u, seg %u)"
				   "with currenttli %d ", log, seg, curFileTLI);

			fd = XLogFileRead(log, seg, emode, tli, XLOG_FROM_PG_XLOG, true);
			if (fd != -1)
				return fd;
		}
	}

	/* Couldn't find it.  For simplicity, complain about front timeline */
	XLogFilePath(path, recoveryTargetTLI, log, seg);
	errno = ENOENT;
	ereport(emode,
			(errcode_for_file_access(),
		   errmsg("could not open file \"%s\" (log file %u, segment %u): %m",
				  path, log, seg)));
	return -1;
}


/*
 * Read the XLOG page containing RecPtr into readBuf (if not read already).
 * Returns true if the page is read successfully.
 *
 * This is responsible for waiting for the requested WAL record to arrive in
 * standby mode.
 *
 * 'emode' specifies the log level used for reporting "file not found" or
 * "end of WAL" situations in standby mode when a trigger file is found.
 * If set to WARNING or below, XLogPageRead() returns false in those situations
 * on higher log levels the ereport() won't return.
 *
 * In standby mode, this only returns false if promotion has been triggered.
 * Otherwise it keeps sleeping and retrying indefinitely.
 */
static bool
XLogPageRead(XLogRecPtr *RecPtr, int emode, bool fetching_ckpt,
			 bool randAccess)
{
	static XLogRecPtr receivedUpto = {0, 0};
	bool		switched_segment = false;
	uint32		targetPageOff;
	uint32		targetRecOff;
	uint32		targetId;
	uint32		targetSeg;
	static pg_time_t last_fail_time = 0;

	XLByteToSeg(*RecPtr, targetId, targetSeg);
	targetPageOff = ((RecPtr->xrecoff % XLogSegSize) / XLOG_BLCKSZ) * XLOG_BLCKSZ;
	targetRecOff = RecPtr->xrecoff % XLOG_BLCKSZ;

	/* Fast exit if we have read the record in the current buffer already */
	if (failedSources == 0 && targetId == readId && targetSeg == readSeg &&
		targetPageOff == readOff && targetRecOff < readLen)
	{
		elogif(debug_xlog_record_read, LOG,
			   "xlog page read -- Requested record %X/%X (targetlogid %u,"
			   "targetset %u, targetpageoff %u, targetrecoff %u) already"
			   "exists in current read buffer",
			   RecPtr->xlogid, RecPtr->xrecoff,
			   targetId, targetSeg, targetPageOff, targetRecOff);

		return true;
	}

	/*
	 * See if we need to switch to a new segment because the requested record
	 * is not in the currently open one.
	 */
	if (readFile >= 0 && !XLByteInSeg(*RecPtr, readId, readSeg))
	{
		elogif(debug_xlog_record_read, LOG,
			   "xlog page read -- Requested record %X/%X does not exist in"
			   "current read xlog file (readlog %u, readseg %u)",
			   RecPtr->xlogid, RecPtr->xrecoff, readId, readSeg);

		close(readFile);
		readFile = -1;
		readSource = 0;
	}

	XLByteToSeg(*RecPtr, readId, readSeg);

	elogif(debug_xlog_record_read, LOG,
		   "xlog page read -- Requested record %X/%X has targetlogid %u, "
		   "targetseg %u, targetpageoff %u, targetrecoff %u",
		   RecPtr->xlogid, RecPtr->xrecoff,
		   targetId, targetSeg, targetPageOff, targetRecOff);

retry:
	/* See if we need to retrieve more data */
	if (readFile < 0 ||
		(readSource == XLOG_FROM_STREAM && !XLByteLT(*RecPtr, receivedUpto)))
	{
		if (StandbyMode)
		{
			/*
			 * In standby mode, wait for the requested record to become
			 * available, via WAL receiver having streamed the record.
			 */
			for (;;)
			{
				if (WalRcvInProgress())
				{
					bool		havedata;

					/*
					 * If we find an invalid record in the WAL streamed from
					 * master, something is seriously wrong. There's little
					 * chance that the problem will just go away, but PANIC is
					 * not good for availability. Disconnect, and retry from
					 * pg_xlog again (That may spawn the Wal receiver again!).
					 * XXX
					 */
					if (failedSources & XLOG_FROM_STREAM)
					{
						elogif(debug_xlog_record_read, LOG,
							   "xlog page read -- Xlog from stream is a failed"
							   "source, hence requesting walreceiver shutdown.");

						ShutdownWalRcv();
						continue;
					}

					/*
					 * WAL receiver is active, so see if new data has arrived.
					 *
					 * We only advance XLogReceiptTime when we obtain fresh
					 * WAL from walreceiver and observe that we had already
					 * processed everything before the most recent "chunk"
					 * that it flushed to disk.  In steady state where we are
					 * keeping up with the incoming data, XLogReceiptTime will
					 * be updated on each cycle.  When we are behind,
					 * XLogReceiptTime will not advance, so the grace time
					 * alloted to conflicting queries will decrease.
					 */
					if (XLByteLT(*RecPtr, receivedUpto))
						havedata = true;
					else
					{
						XLogRecPtr	latestChunkStart;

						receivedUpto = GetWalRcvWriteRecPtr(&latestChunkStart);
						if (XLByteLT(*RecPtr, receivedUpto))
						{
							havedata = true;
							if (!XLByteLT(*RecPtr, latestChunkStart))
							{
								XLogReceiptTime = GetCurrentTimestamp();
								SetCurrentChunkStartTime(XLogReceiptTime);
							}
						}
						else
							havedata = false;
					}

					if (havedata)
					{
						elogif(debug_xlog_record_read, LOG,
							   "xlog page read -- There is enough xlog data to be "
							   "read (receivedupto %X/%X, requestedrec %X/%X)",
							   receivedUpto.xlogid, receivedUpto.xrecoff,
							   RecPtr->xlogid, RecPtr->xrecoff);

						/*
						 * Great, streamed far enough. Open the file if it's
						 * not open already.  Use XLOG_FROM_STREAM so that
						 * source info is set correctly and XLogReceiptTime
						 * isn't changed.
						 */
						if (readFile < 0)
						{
							readFile =
								XLogFileRead(readId, readSeg, PANIC,
											 recoveryTargetTLI,
											 XLOG_FROM_STREAM, false);
							Assert(readFile >= 0);
							switched_segment = true;
						}
						else
						{
							/* just make sure source info is correct... */
							readSource = XLOG_FROM_STREAM;
							XLogReceiptSource = XLOG_FROM_STREAM;
						}
						break;
					}

					/*
					 * Data not here yet, so check for trigger then sleep for
					 * five seconds like in the WAL file polling case below.
					 */
					if (CheckForStandbyTrigger())
					{
						elogif(debug_xlog_record_read, LOG,
							   "xlog page read -- Standby trigger was activated");

						goto retry;
					}

					elogif(debug_xlog_record_read, LOG,
						   "xlog page read -- No xlog data to read as of now. "
						   "Will Wait on latch till some event occurs");

					/*
					 * Wait for more WAL to arrive, or timeout to be reached
					 */
					WaitLatch(&XLogCtl->recoveryWakeupLatch,
							  WL_LATCH_SET | WL_TIMEOUT,
							  5000L);
					ResetLatch(&XLogCtl->recoveryWakeupLatch);
				}
				else
				{
					int			sources;
					pg_time_t	now;

					if (readFile >= 0)
					{
						close(readFile);
						readFile = -1;
					}

					/* Reset curFileTLI if random fetch. */
					if (randAccess)
						curFileTLI = 0;

					/* Read an existing file from pg_xlog. */
					sources = XLOG_FROM_PG_XLOG;
					if (!(sources & ~failedSources))
					{
						/*
						 * Check if we have been asked to be promoted. If yes,
						 * no use of requesting a new WAL receiver
						 */
						if (CheckForStandbyTrigger())
							goto triggered;

						/*
						 * We've exhausted all options for retrieving the
						 * file. Retry.
						 */
						failedSources = 0;

						elogif(debug_xlog_record_read, LOG,
							   "xlog page read -- All read sources have failed. So, retry.");

						/*
						 * If it hasn't been long since last attempt, sleep to
						 * avoid busy-waiting.
						 */
						now = (pg_time_t) time(NULL);
						if ((now - last_fail_time) < 5)
						{
							pg_usleep(1000000L * (5 - (now - last_fail_time)));
							now = (pg_time_t) time(NULL);
						}
						last_fail_time = now;

						/*
						 * If primary_conninfo is set, launch walreceiver to
						 * try to stream the missing WAL.
						 *
						 * If fetching_ckpt is TRUE, RecPtr points to the
						 * initial checkpoint location. In that case, we use
						 * RedoStartLSN as the streaming start position
						 * instead of RecPtr, so that when we later jump
						 * backwards to start redo at RedoStartLSN, we will
						 * have the logs streamed already.
						 */
						if (PrimaryConnInfo)
						{
							RequestXLogStreaming(
									  fetching_ckpt ? RedoStartLSN : *RecPtr,
												 PrimaryConnInfo);
							continue;
						}
					}
					/* Don't try to read from a source that just failed */
					sources &= ~failedSources;
					readFile = XLogFileReadAnyTLI(readId, readSeg, DEBUG2,
												  sources);
					switched_segment = true;
					if (readFile >= 0)
						break;

					/*
					 * Nope, not found in pg_xlog.
					 */
					failedSources |= sources;

					/*
					 * Check to see if the trigger file exists. Note that we
					 * do this only after failure, so when you create the
					 * trigger file, we still finish replaying as much as we
					 * can from pg_xlog before failover.
					 */
					if (CheckForStandbyTrigger())
						goto triggered;
				}

				/*
				 * This possibly-long loop needs to handle interrupts of
				 * startup process.
				 */
				HandleStartupProcInterrupts();
			}
		}
		else
		{
			/* In crash recovery. */
			if (readFile < 0)
			{
				int			sources;

				/* Reset curFileTLI if random fetch. */
				if (randAccess)
					curFileTLI = 0;

				sources = XLOG_FROM_PG_XLOG;

				readFile = XLogFileReadAnyTLI(readId, readSeg, emode,
											sources);
				switched_segment = true;
				if (readFile < 0)
					return false;
			}
		}
	}

	/*
	 * At this point, we have the right segment open and if we're streaming we
	 * know the requested record is in it.
	 */
	Assert(readFile != -1);

	/*
	 * If the current segment is being streamed from master, calculate how
	 * much of the current page we have received already. We know the
	 * requested record has been received, but this is for the benefit of
	 * future calls, to allow quick exit at the top of this function.
	 */
	if (readSource == XLOG_FROM_STREAM)
	{
		if (RecPtr->xlogid != receivedUpto.xlogid ||
			(RecPtr->xrecoff / XLOG_BLCKSZ) != (receivedUpto.xrecoff / XLOG_BLCKSZ))
		{
			readLen = XLOG_BLCKSZ;
		}
		else
			readLen = receivedUpto.xrecoff % XLogSegSize - targetPageOff;
	}
	else
		readLen = XLOG_BLCKSZ;

	if (switched_segment && targetPageOff != 0)
	{
		/*
		 * Whenever switching to a new WAL segment, we read the first page of
		 * the file and validate its header, even if that's not where the
		 * target record is.  This is so that we can check the additional
		 * identification info that is present in the first page's "long"
		 * header.
		 */
		readOff = 0;
		if (read(readFile, readBuf, XLOG_BLCKSZ) != XLOG_BLCKSZ)
		{
			ereport(emode,
					(errcode_for_file_access(),
					 errmsg("could not read from log file %u, segment %u, offset %u: %m",
							readId, readSeg, readOff)));
			goto next_record_is_invalid;
		}
		if (!ValidXLOGHeader((XLogPageHeader) readBuf, emode, true))
		{
			ereport(emode,
					(errcode_for_file_access(),
					 errmsg("could not read from log file %u, segment %u, offset %u: %m",
							readId, readSeg, readOff)));
			goto next_record_is_invalid;
		}
	}

	/* Read the requested page */
	readOff = targetPageOff;
	if (lseek(readFile, (off_t) readOff, SEEK_SET) < 0)
	{
		ereport(emode,
				(errcode_for_file_access(),
		 errmsg("could not seek in log file %u, segment %u to offset %u: %m",
				readId, readSeg, readOff)));
		goto next_record_is_invalid;
	}
	if (read(readFile, readBuf, XLOG_BLCKSZ) != XLOG_BLCKSZ)
	{
		ereport(emode,
				(errcode_for_file_access(),
		 errmsg("could not read from log file %u, segment %u, offset %u: %m",
				readId, readSeg, readOff)));
		goto next_record_is_invalid;
	}
	if (!ValidXLOGHeader((XLogPageHeader) readBuf, emode, false))
	{
		elogif(debug_xlog_record_read, LOG,
			   "xlog page read -- xlog page header invalid");
		goto next_record_is_invalid;
	}

	Assert(targetId == readId);
	Assert(targetSeg == readSeg);
	Assert(targetPageOff == readOff);
	Assert(targetRecOff < readLen);

	return true;

next_record_is_invalid:

	elogif(debug_xlog_record_read, LOG,
		   "xlog page read -- next record is invalid.");

	failedSources |= readSource;

	if (readFile >= 0)
		close(readFile);
	readFile = -1;
	readLen = 0;
	readSource = 0;

	/* In standby-mode, keep trying */
	if (StandbyMode)
		goto retry;
	else
		return false;

triggered:
	if (readFile >= 0)
		close(readFile);
	readFile = -1;
	readLen = 0;
	readSource = 0;

	return false;
}

/*
 * Attempt to read an XLOG record.
 *
 * If RecPtr is not NULL, try to read a record at that position.  Otherwise
 * try to read a record just after the last one previously read.
 *
 * If no valid record is available, returns NULL, or fails if emode is PANIC.
 * (emode must be either PANIC or LOG.)
 *
 * The record is copied into readRecordBuf, so that on successful return,
 * the returned record pointer always points there.
 */
XLogRecord *
XLogReadRecord(XLogRecPtr *RecPtr, bool fetching_ckpt, int emode)
{
	XLogRecord *record;
	char	   *buffer;
	XLogRecPtr	tmpRecPtr = EndRecPtr;
	bool		randAccess = false;
	uint32		len,
				total_len;
	uint32		targetRecOff;
	uint32		pageHeaderSize;

	if (readBuf == NULL)
	{
		/*
		 * First time through, permanently allocate readBuf.  We do it this
		 * way, rather than just making a static array, for two reasons: (1)
		 * no need to waste the storage in most instantiations of the backend;
		 * (2) a static char array isn't guaranteed to have any particular
		 * alignment, whereas malloc() will provide MAXALIGN'd storage.
		 */
		readBuf = (char *) malloc(XLOG_BLCKSZ);
		if(!readBuf)
			ereport(PANIC, (errmsg("Cannot allocate memory for read log record. Out of Memory")));
	}

	if (RecPtr == NULL)
	{
		RecPtr = &tmpRecPtr;

		/*
		 * RecPtr is pointing to end+1 of the previous WAL record. We must
		 * advance it if necessary to where the next record starts.  First,
		 * align to next page if no more records can fit on the current page.
		 */
		if (nextRecord == NULL)
		{
			/* align old recptr to next page */
			if (RecPtr->xrecoff % XLOG_BLCKSZ != 0)
				RecPtr->xrecoff += (XLOG_BLCKSZ - RecPtr->xrecoff % XLOG_BLCKSZ);
			if (RecPtr->xrecoff >= XLogFileSize)
			{
				(RecPtr->xlogid)++;
				RecPtr->xrecoff = 0;
			}
		}
		/* We will account for page header size below */
	}
	else
	{
		/*
		 * In this case, the passed-in record pointer should already be
		 * pointing to a valid record starting position.
		 */
		if (!XRecOffIsValid(RecPtr->xrecoff))
			ereport(PANIC,
					(errmsg("invalid record offset at %X/%X",
							RecPtr->xlogid, RecPtr->xrecoff)));

		/*
		 * Since we are going to a random position in WAL, forget any prior
		 * state about what timeline we were in, and allow it to be any
		 * timeline in expectedTLIs.  We also set a flag to allow curFileTLI
		 * to go backwards (but we can't reset that variable right here, since
		 * we might not change files at all).
		 */
		lastPageTLI = 0;		/* see comment in ValidXLOGHeader */
		lastSegmentTLI = 0;
		randAccess = true;		/* allow curFileTLI to go backwards too */
	}

	/* This is the first try to read this page. */
	failedSources = 0;
retry:
	/* Read the page containing the record */
	if (!XLogPageRead(RecPtr, emode, fetching_ckpt, randAccess))
	{
		/*
		 * In standby mode, XLogPageRead returning false means that promotion
		 * has been triggered.
		 */
		if (StandbyMode)
			return NULL;
		else
			goto next_record_is_invalid;
	}

	/* *********Above this xlogpageread should called ***********/
	pageHeaderSize = XLogPageHeaderSize((XLogPageHeader) readBuf);
	targetRecOff = RecPtr->xrecoff % XLOG_BLCKSZ;
	if (targetRecOff == 0)
	{
		/*
		 * At page start, so skip over page header.  The Assert checks that
		 * we're not scribbling on caller's record pointer; it's OK because we
		 * can only get here in the continuing-from-prev-record case, since
		 * XRecOffIsValid rejected the zero-page-offset case otherwise.
		 */
		Assert(RecPtr == &tmpRecPtr);
		RecPtr->xrecoff += pageHeaderSize;
		targetRecOff = pageHeaderSize;
	}
	else if (targetRecOff < pageHeaderSize)
	{
		ereport(emode,
				(errmsg("invalid record offset at %X/%X",
						RecPtr->xlogid, RecPtr->xrecoff)));
		goto next_record_is_invalid;
	}
	if ((((XLogPageHeader) readBuf)->xlp_info & XLP_FIRST_IS_CONTRECORD) &&
		targetRecOff == pageHeaderSize)
	{
		ereport(emode,
				(errmsg("contrecord is requested by %X/%X",
						RecPtr->xlogid, RecPtr->xrecoff)));
		goto next_record_is_invalid;
	}
	record = (XLogRecord *) ((char *) readBuf + RecPtr->xrecoff % XLOG_BLCKSZ);

	/*
	 * xl_len == 0 is bad data for everything except XLOG SWITCH, where it is
	 * required.
	 */
	if (record->xl_rmid == RM_XLOG_ID && record->xl_info == XLOG_SWITCH)
	{
		if (record->xl_len != 0)
		{
			ereport(emode,
					(errmsg("invalid xlog switch record at %X/%X",
							RecPtr->xlogid, RecPtr->xrecoff)));
			goto next_record_is_invalid;
		}
	}
	else if (record->xl_len == 0)
	{
		ereport(emode,
				(errmsg("record with zero length at %X/%X",
						RecPtr->xlogid, RecPtr->xrecoff)));
		goto next_record_is_invalid;
	}
	if (record->xl_tot_len < SizeOfXLogRecord + record->xl_len ||
		record->xl_tot_len > SizeOfXLogRecord + record->xl_len +
		XLR_MAX_BKP_BLOCKS * (sizeof(BkpBlock) + BLCKSZ))
	{
		ereport(emode,
				(errmsg("invalid record length at %X/%X",
						RecPtr->xlogid, RecPtr->xrecoff)));
		goto next_record_is_invalid;
	}
	if (record->xl_rmid > RM_MAX_ID)
	{
		ereport(emode,
				(errmsg("invalid resource manager ID %u at %X/%X",
						record->xl_rmid, RecPtr->xlogid, RecPtr->xrecoff)));
		goto next_record_is_invalid;
	}
	if (randAccess)
	{
		/*
		 * We can't exactly verify the prev-link, but surely it should be less
		 * than the record's own address.
		 */
		if (!XLByteLT(record->xl_prev, *RecPtr))
		{
			ereport(emode,
					(errmsg("record with incorrect prev-link %X/%X at %X/%X",
							record->xl_prev.xlogid, record->xl_prev.xrecoff,
							RecPtr->xlogid, RecPtr->xrecoff)));
			goto next_record_is_invalid;
		}
	}
	else
	{
		/*
		 * Record's prev-link should exactly match our previous location. This
		 * check guards against torn WAL pages where a stale but valid-looking
		 * WAL record starts on a sector boundary.
		 */
		if (!XLByteEQ(record->xl_prev, ReadRecPtr))
		{
			ereport(emode,
					(errmsg("record with incorrect prev-link %X/%X at %X/%X",
							record->xl_prev.xlogid, record->xl_prev.xrecoff,
							RecPtr->xlogid, RecPtr->xrecoff)));
			goto next_record_is_invalid;
		}
	}

	/*
	 * Allocate or enlarge readRecordBuf as needed.  To avoid useless small
	 * increases, round its size to a multiple of XLOG_BLCKSZ, and make sure
	 * it's at least 4*Max(BLCKSZ, XLOG_BLCKSZ) to start with.  (That is
	 * enough for all "normal" records, but very large commit or abort records
	 * might need more space.)
	 */
	total_len = record->xl_tot_len;
	if (total_len > readRecordBufSize)
	{
		uint32		newSize = total_len;

		newSize += XLOG_BLCKSZ - (newSize % XLOG_BLCKSZ);
		newSize = Max(newSize, 4 * Max(BLCKSZ, XLOG_BLCKSZ));
		if (readRecordBuf)
			free(readRecordBuf);
		readRecordBuf = (char *) malloc(newSize);
		if (!readRecordBuf)
		{
			readRecordBufSize = 0;
			ereport(emode,
					(errmsg("cannot allocate %u bytes for record at %X/%X",
							newSize, RecPtr->xlogid, RecPtr->xrecoff)));
			goto next_record_is_invalid;
		}
		readRecordBufSize = newSize;
	}

	buffer = readRecordBuf;
	nextRecord = NULL;
	len = XLOG_BLCKSZ - RecPtr->xrecoff % XLOG_BLCKSZ;
	if (total_len > len)
	{
		/* Need to reassemble record */
		XLogContRecord *contrecord;
		XLogRecPtr	pagelsn;
		uint32		gotlen = len;

		/* Initialize pagelsn to the beginning of the page this record is on */
		pagelsn = *RecPtr;
		pagelsn.xrecoff = (pagelsn.xrecoff / XLOG_BLCKSZ) * XLOG_BLCKSZ;

		memcpy(buffer, record, len);
		record = (XLogRecord *) buffer;
		buffer += len;
		for (;;)
		{
			/* Calculate pointer to beginning of next page */
			pagelsn.xrecoff += XLOG_BLCKSZ;
			if (pagelsn.xrecoff >= XLogFileSize)
			{
				(pagelsn.xlogid)++;
				pagelsn.xrecoff = 0;
			}
			/* Wait for the next page to become available */
			if (!XLogPageRead(&pagelsn, emode, false, false))
			{
				/*
				 * In standby-mode, XLogPageRead returning false means that
				 * promotion has been triggered.
				 */
				if (StandbyMode)
					return NULL;
				else
					goto next_record_is_invalid;
			}

			if (!(((XLogPageHeader) readBuf)->xlp_info & XLP_FIRST_IS_CONTRECORD))
			{
				ereport(emode,
						(errmsg("there is no contrecord flag in log file %u, segment %u, offset %u",
								readId, readSeg, readOff)));
				goto next_record_is_invalid;
			}
			pageHeaderSize = XLogPageHeaderSize((XLogPageHeader) readBuf);
			contrecord = (XLogContRecord *) ((char *) readBuf + pageHeaderSize);
			if (contrecord->xl_rem_len == 0 ||
				total_len != (contrecord->xl_rem_len + gotlen))
			{
				ereport(emode,
						(errmsg("invalid contrecord length %u in log file %u, segment %u, offset %u",
								contrecord->xl_rem_len,
								readId, readSeg, readOff)));
				goto next_record_is_invalid;
			}
			len = XLOG_BLCKSZ - pageHeaderSize - SizeOfXLogContRecord;
			if (contrecord->xl_rem_len > len)
			{
				memcpy(buffer, (char *) contrecord + SizeOfXLogContRecord, len);
				gotlen += len;
				buffer += len;
				continue;
			}
			memcpy(buffer, (char *) contrecord + SizeOfXLogContRecord,
				   contrecord->xl_rem_len);
			break;
		}
		if (!RecordIsValid(record, *RecPtr, emode))
			goto next_record_is_invalid;
		pageHeaderSize = XLogPageHeaderSize((XLogPageHeader) readBuf);
		if (XLOG_BLCKSZ - SizeOfXLogRecord >= pageHeaderSize +
			MAXALIGN(SizeOfXLogContRecord + contrecord->xl_rem_len))
		{
			nextRecord = (XLogRecord *) ((char *) contrecord +
					MAXALIGN(SizeOfXLogContRecord + contrecord->xl_rem_len));
		}
		EndRecPtr.xlogid = readId;
		EndRecPtr.xrecoff = readSeg * XLogSegSize + readOff +
			pageHeaderSize +
			MAXALIGN(SizeOfXLogContRecord + contrecord->xl_rem_len);
		ReadRecPtr = *RecPtr;
		/* needn't worry about XLOG SWITCH, it can't cross page boundaries */
		return record;
	}

	/* Record does not cross a page boundary */
	if (!RecordIsValid(record, *RecPtr, emode))
		goto next_record_is_invalid;
	if (XLOG_BLCKSZ - SizeOfXLogRecord >= RecPtr->xrecoff % XLOG_BLCKSZ +
		MAXALIGN(total_len))
		nextRecord = (XLogRecord *) ((char *) record + MAXALIGN(total_len));
	EndRecPtr.xlogid = RecPtr->xlogid;
	EndRecPtr.xrecoff = RecPtr->xrecoff + MAXALIGN(total_len);
	ReadRecPtr = *RecPtr;
	memcpy(buffer, record, total_len);

	/*
	 * Special processing if it's an XLOG SWITCH record
	 */
	if (record->xl_rmid == RM_XLOG_ID && record->xl_info == XLOG_SWITCH)
	{
		/* Pretend it extends to end of segment */
		EndRecPtr.xrecoff += XLogSegSize - 1;
		EndRecPtr.xrecoff -= EndRecPtr.xrecoff % XLogSegSize;
		nextRecord = NULL;		/* definitely not on same page */

		/*
		 * Pretend that readBuf contains the last page of the segment. This is
		 * just to avoid Assert failure in StartupXLOG if XLOG ends with this
		 * segment.
		 */
		readOff = XLogSegSize - XLOG_BLCKSZ;
	}

	elogif(debug_xlog_record_read, LOG,
		   "xlog read record -- Read record %X/%X successfully with endrecptr %X/%X",
		   ReadRecPtr.xlogid, ReadRecPtr.xrecoff,
		   EndRecPtr.xlogid, EndRecPtr.xrecoff);

	return (XLogRecord *) buffer;

next_record_is_invalid:

	elogif(debug_xlog_record_read, LOG,
		   "xlog record read -- next record is invalid.");

	failedSources |= readSource;

	if (readFile >= 0)
	{
		close(readFile);
		readFile = -1;
	}

	nextRecord = NULL;

	/* In standby-mode, keep trying */
	if (StandbyMode && !CheckForStandbyTrigger())
		goto retry;
	else
		return NULL;
}

/*
 * Close, re-set and clean all the necessary resources used during reading
 * XLog records.
 */
void
XLogCloseReadRecord(void)
{
	if (readFile >= 0)
	{
		close(readFile);
		readFile = -1;
	}
	else
		Assert(readFile == -1);

	if (readBuf)
	{
		free(readBuf);
		readBuf = NULL;
	}

	if (readRecordBuf)
	{
		free(readRecordBuf);
		readRecordBuf = NULL;
	}

	readId = 0;
	readSeg = 0;
	readOff = 0;
	readLen = 0;
	readRecordBufSize = 0;
	nextRecord = NULL;

	memset(&ReadRecPtr, 0, sizeof(XLogRecPtr));
	memset(&EndRecPtr, 0, sizeof(XLogRecPtr));
}

/*
 * Check whether the xlog header of a page just read in looks valid.
 *
 * This is just a convenience subroutine to avoid duplicated code in
 * ReadRecord.	It's not intended for use from anywhere else.
 */
static bool
ValidXLOGHeader(XLogPageHeader hdr, int emode, bool segmentonly)
{
	XLogRecPtr	recaddr;

	if (hdr->xlp_magic != XLOG_PAGE_MAGIC)
	{
		ereport(emode,
				(errmsg("invalid magic number %04X in log file %u, segment %u, offset %u",
						hdr->xlp_magic, readId, readSeg, readOff)));
		return false;
	}
	if ((hdr->xlp_info & ~XLP_ALL_FLAGS) != 0)
	{
		ereport(emode,
				(errmsg("invalid info bits %04X in log file %u, segment %u, offset %u",
						hdr->xlp_info, readId, readSeg, readOff)));
		return false;
	}
	if (hdr->xlp_info & XLP_LONG_HEADER)
	{
		XLogLongPageHeader longhdr = (XLogLongPageHeader) hdr;

		if (longhdr->xlp_sysid != ControlFile->system_identifier)
		{
			char		fhdrident_str[32];
			char		sysident_str[32];

			/*
			 * Format sysids separately to keep platform-dependent format code
			 * out of the translatable message string.
			 */
			snprintf(fhdrident_str, sizeof(fhdrident_str), UINT64_FORMAT,
					 longhdr->xlp_sysid);
			snprintf(sysident_str, sizeof(sysident_str), UINT64_FORMAT,
					 ControlFile->system_identifier);
			ereport(emode,
					(errmsg("WAL file is from different system"),
					 errdetail("WAL file SYSID is %s, pg_control SYSID is %s",
							   fhdrident_str, sysident_str)));
			return false;
		}
		if (longhdr->xlp_seg_size != XLogSegSize)
		{
			ereport(emode,
					(errmsg("WAL file is from different system"),
					 errdetail("Incorrect XLOG_SEG_SIZE in page header.")));
			return false;
		}
		if (longhdr->xlp_xlog_blcksz != XLOG_BLCKSZ)
		{
			ereport(emode,
					(errmsg("WAL file is from different system"),
					 errdetail("Incorrect XLOG_BLCKSZ in page header.")));
			return false;
		}
	}
	else if (readOff == 0)
	{
		/* hmm, first page of file doesn't have a long header? */
		ereport(emode,
				(errmsg("invalid info bits %04X in log file %u, segment %u, offset %u",
						hdr->xlp_info, readId, readSeg, readOff)));
		return false;
	}

	recaddr.xlogid = readId;
	recaddr.xrecoff = readSeg * XLogSegSize + readOff;
	if (!XLByteEQ(hdr->xlp_pageaddr, recaddr))
	{
		ereport(emode,
				(errmsg("unexpected pageaddr %X/%X in log file %u, segment %u, offset %u",
						hdr->xlp_pageaddr.xlogid, hdr->xlp_pageaddr.xrecoff,
						readId, readSeg, readOff)));
		return false;
	}

	/*
	 * Check page TLI is one of the expected values.
	 */
	if (!list_member_int(expectedTLIs, (int) hdr->xlp_tli))
	{
		ereport(emode,
				(errmsg("unexpected timeline ID %u in log file %u, segment %u, offset %u",
						hdr->xlp_tli,
						readId, readSeg, readOff)));
		return false;
	}

	/*
	 * Since child timelines are always assigned a TLI greater than their
	 * immediate parent's TLI, we should never see TLI go backwards across
	 * successive pages of a consistent WAL sequence.
	 *
	 * Of course this check should only be applied when advancing sequentially
	 * across pages; therefore ReadRecord resets lastPageTLI and
	 * lastSegmentTLI to zero when going to a random page.
	 *
	 * Sometimes we re-open a segment that's already been partially replayed.
	 * In that case we cannot perform the normal TLI check: if there is a
	 * timeline switch within the segment, the first page has a smaller TLI
	 * than later pages following the timeline switch, and we might've read
	 * them already. As a weaker test, we still check that it's not smaller
	 * than the TLI we last saw at the beginning of a segment. Pass
	 * segmentonly = true when re-validating the first page like that, and the
	 * page you're actually interested in comes later.
	 */
	if (hdr->xlp_tli < (segmentonly ? lastSegmentTLI : lastPageTLI))
	{
		ereport(emode,
				(errmsg("out-of-sequence timeline ID %u (after %u) in log file %u, segment %u, offset %u",
						hdr->xlp_tli, lastPageTLI,
						readId, readSeg, readOff)));
		return false;
	}
	lastPageTLI = hdr->xlp_tli;
	if (readOff == 0)
		lastSegmentTLI = hdr->xlp_tli;

	return true;
}

/*
 * Try to read a timeline's history file.
 *
 * If successful, return the list of component TLIs (the given TLI followed by
 * its ancestor TLIs).	If we can't find the history file, assume that the
 * timeline has no parents, and return a list of just the specified timeline
 * ID.
 */
List *
XLogReadTimeLineHistory(TimeLineID targetTLI)
{
	List	   *result;
	char		path[MAXPGPATH];
	char		fline[MAXPGPATH];
	FILE	   *fd;

	/* Timeline 1 does not have a history file, so no need to check */
	if (targetTLI == 1)
		return list_make1_int((int) targetTLI);

	TLHistoryFilePath(path, targetTLI);

	fd = AllocateFile(path, "r");
	if (fd == NULL)
	{
		if (errno != ENOENT)
			ereport(FATAL,
					(errcode_for_file_access(),
					 errmsg("could not open file \"%s\": %m", path)));
		/* Not there, so assume no parents */
		return list_make1_int((int) targetTLI);
	}

	result = NIL;

	/*
	 * Parse the file...
	 */
	while (fgets(fline, sizeof(fline), fd) != NULL)
	{
		/* skip leading whitespace and check for # comment */
		char	   *ptr;
		char	   *endptr;
		TimeLineID	tli;

		for (ptr = fline; *ptr; ptr++)
		{
			if (!isspace((unsigned char) *ptr))
				break;
		}
		if (*ptr == '\0' || *ptr == '#')
			continue;

		/* expect a numeric timeline ID as first field of line */
		tli = (TimeLineID) strtoul(ptr, &endptr, 0);
		if (endptr == ptr)
			ereport(FATAL,
					(errmsg("syntax error in history file: %s", fline),
					 errhint("Expected a numeric timeline ID.")));

		if (result &&
			tli <= (TimeLineID) linitial_int(result))
			ereport(FATAL,
					(errmsg("invalid data in history file: %s", fline),
				   errhint("Timeline IDs must be in increasing sequence.")));

		/* Build list with newest item first */
		result = lcons_int((int) tli, result);

		/* we ignore the remainder of each line */
	}

	FreeFile(fd);

	if (result &&
		targetTLI <= (TimeLineID) linitial_int(result))
		ereport(FATAL,
				(errmsg("invalid data in history file \"%s\"", path),
			errhint("Timeline IDs must be less than child timeline's ID.")));

	result = lcons_int((int) targetTLI, result);

	ereport(DEBUG3,
			(errmsg_internal("history of timeline %u is %s",
							 targetTLI, nodeToString(result))));

	return result;
}

/*
 * Probe whether a timeline history file exists for the given timeline ID
 */
static bool
existsTimeLineHistory(TimeLineID probeTLI)
{
	char		path[MAXPGPATH];
	FILE	   *fd;

	TLHistoryFilePath(path, probeTLI);

	fd = AllocateFile(path, "r");
	if (fd != NULL)
	{
		FreeFile(fd);
		return true;
	}
	else
	{
		if (errno != ENOENT)
			ereport(FATAL,
					(errcode_for_file_access(),
					 errmsg("could not open file \"%s\": %m", path)));
		return false;
	}
}

/*
 * Find the newest existing timeline, assuming that startTLI exists.
 *
 * Note: while this is somewhat heuristic, it does positively guarantee
 * that (result + 1) is not a known timeline, and therefore it should
 * be safe to assign that ID to a new timeline.
 */
static TimeLineID
findNewestTimeLine(TimeLineID startTLI)
{
	TimeLineID	newestTLI;
	TimeLineID	probeTLI;

	/*
	 * The algorithm is just to probe for the existence of timeline history
	 * files.  XXX is it useful to allow gaps in the sequence?
	 */
	newestTLI = startTLI;

	for (probeTLI = startTLI + 1;; probeTLI++)
	{
		if (existsTimeLineHistory(probeTLI))
		{
			newestTLI = probeTLI;		/* probeTLI exists */
		}
		else
		{
			/* doesn't exist, assume we're done */
			break;
		}
	}

	return newestTLI;
}

/*
 * Create a new timeline history file.
 *
 *	newTLI: ID of the new timeline
 *	parentTLI: ID of its immediate parent
 *	endTLI et al: ID of the last used WAL file, for annotation purposes
 *
 * Currently this is only used during recovery, and so there are no locking
 * considerations.	But we should be just as tense as XLogFileInit to avoid
 * emplacing a bogus file.
 */
static void
writeTimeLineHistory(TimeLineID newTLI, TimeLineID parentTLI,
					 TimeLineID endTLI, uint32 endLogId, uint32 endLogSeg)
{
	char		path[MAXPGPATH];
	char		tmppath[MAXPGPATH];
	char		histfname[MAXFNAMELEN];
	char		xlogfname[MAXFNAMELEN];
	char		buffer[BLCKSZ];
	int			srcfd;
	int			fd;
	int			nbytes;

	Assert(newTLI > parentTLI); /* else bad selection of newTLI */

	/*
	 * Write into a temp file name.
	 */
	snprintf(tmppath, MAXPGPATH, XLOGDIR "/xlogtemp.%d", (int) getpid());

	unlink(tmppath);

	/* do not use get_sync_bit() here --- want to fsync only at end of fill */
	fd = BasicOpenFile(tmppath, O_RDWR | O_CREAT | O_EXCL,
					   S_IRUSR | S_IWUSR);
	if (fd < 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not create file \"%s\": %m", tmppath)));

	TLHistoryFilePath(path, parentTLI);

	srcfd = BasicOpenFile(path, O_RDONLY, 0);
	if (srcfd < 0)
	{
		if (errno != ENOENT)
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not open file \"%s\": %m", path)));
		/* Not there, so assume parent has no parents */
	}
	else
	{
		for (;;)
		{
			errno = 0;
			nbytes = (int) read(srcfd, buffer, sizeof(buffer));
			if (nbytes < 0 || errno != 0)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not read file \"%s\": %m", path)));
			if (nbytes == 0)
				break;
			errno = 0;
			if ((int) write(fd, buffer, nbytes) != nbytes)
			{
				int			save_errno = errno;

				/*
				 * If we fail to make the file, delete it to release disk
				 * space
				 */
				unlink(tmppath);

				/*
				 * if write didn't set errno, assume problem is no disk space
				 */
				errno = save_errno ? save_errno : ENOSPC;

				ereport(ERROR,
						(errcode_for_file_access(),
					 errmsg("could not write to file \"%s\": %m", tmppath)));
			}
		}
		close(srcfd);
	}

	/*
	 * Append one line with the details of this timeline split.
	 *
	 * If we did have a parent file, insert an extra newline just in case the
	 * parent file failed to end with one.
	 */
	XLogFileName(xlogfname, endTLI, endLogId, endLogSeg);

	snprintf(buffer, sizeof(buffer),
			 "%s%u\t%s\t%s transaction %u at %s\n",
			 (srcfd < 0) ? "" : "\n",
			 parentTLI,
			 xlogfname,
			 recoveryStopAfter ? "after" : "before",
			 recoveryStopXid,
			 timestamptz_to_str(recoveryStopTime));

	nbytes = strlen(buffer);
	errno = 0;
	if ((int) write(fd, buffer, nbytes) != nbytes)
	{
		int			save_errno = errno;

		/*
		 * If we fail to make the file, delete it to release disk space
		 */
		unlink(tmppath);
		/* if write didn't set errno, assume problem is no disk space */
		errno = save_errno ? save_errno : ENOSPC;

		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not write to file \"%s\": %m", tmppath)));
	}

	if (pg_fsync(fd) != 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not fsync file \"%s\": %m", tmppath)));

	if (close(fd))
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not close file \"%s\": %m", tmppath)));


	/*
	 * Now move the completed history file into place with its final name.
	 */
	TLHistoryFilePath(path, newTLI);

	/*
	 * Prefer link() to rename() here just to be really sure that we don't
	 * overwrite an existing logfile.  However, there shouldn't be one, so
	 * rename() is an acceptable substitute except for the truly paranoid.
	 */
#if HAVE_WORKING_LINK
	if (link(tmppath, path) < 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not link file \"%s\" to \"%s\": %m",
						tmppath, path)));
	unlink(tmppath);
#else
	if (rename(tmppath, path) < 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not rename file \"%s\" to \"%s\": %m",
						tmppath, path)));
#endif

	/* The history file can be archived immediately. */
	TLHistoryFileName(histfname, newTLI);
	XLogArchiveNotify(histfname);
}

static void
ControlFileWatcherSaveInitial(void)
{
	ControlFileWatcher->current_checkPointLoc = ControlFile->checkPoint;
	ControlFileWatcher->current_prevCheckPointLoc = ControlFile->prevCheckPoint;
	ControlFileWatcher->current_checkPointCopy_redo = ControlFile->checkPointCopy.redo;

	if (Debug_print_control_checkpoints)
		elog(LOG,"pg_control checkpoint: initial values (checkpoint loc %s, previous loc %s, copy's redo loc %s)",
			 XLogLocationToString_Long(&ControlFile->checkPoint),
			 XLogLocationToString2_Long(&ControlFile->prevCheckPoint),
			 XLogLocationToString3_Long(&ControlFile->checkPointCopy.redo));

	ControlFileWatcher->watcherInitialized = true;
}

static void
ControlFileWatcherCheckForChange(void)
{
	XLogRecPtr  writeLoc;
	XLogRecPtr  flushedLoc;

	if (!XLByteEQ(ControlFileWatcher->current_checkPointLoc,ControlFile->checkPoint) ||
		!XLByteEQ(ControlFileWatcher->current_prevCheckPointLoc,ControlFile->prevCheckPoint) ||
		!XLByteEQ(ControlFileWatcher->current_checkPointCopy_redo,ControlFile->checkPointCopy.redo))
	{
		ControlFileWatcher->current_checkPointLoc = ControlFile->checkPoint;
		ControlFileWatcher->current_prevCheckPointLoc = ControlFile->prevCheckPoint;
		ControlFileWatcher->current_checkPointCopy_redo = ControlFile->checkPointCopy.redo;

		if (XLogGetWriteAndFlushedLoc(&writeLoc, &flushedLoc))
		{
			bool problem = XLByteLE(flushedLoc,ControlFile->checkPoint);
			if (problem)
				elog(PANIC,"Checkpoint location %s for pg_control file is not flushed (write loc %s, flushed loc is %s)",
				     XLogLocationToString_Long(&ControlFile->checkPoint),
				     XLogLocationToString2_Long(&writeLoc),
				     XLogLocationToString3_Long(&flushedLoc));

			if (Debug_print_control_checkpoints)
				elog(LOG,"pg_control checkpoint: change (checkpoint loc %s, previous loc %s, copy's redo loc %s, write loc %s, flushed loc %s)",
					 XLogLocationToString_Long(&ControlFile->checkPoint),
					 XLogLocationToString2_Long(&ControlFile->prevCheckPoint),
					 XLogLocationToString3_Long(&ControlFile->checkPointCopy.redo),
					 XLogLocationToString4_Long(&writeLoc),
					 XLogLocationToString5_Long(&flushedLoc));
		}
		else
		{
			if (Debug_print_control_checkpoints)
				elog(LOG,"pg_control checkpoint: change (checkpoint loc %s, previous loc %s, copy's redo loc %s)",
					 XLogLocationToString_Long(&ControlFile->checkPoint),
					 XLogLocationToString2_Long(&ControlFile->prevCheckPoint),
					 XLogLocationToString3_Long(&ControlFile->checkPointCopy.redo));
		}
	}
}

/*
 * I/O routines for pg_control
 *
 * *ControlFile is a buffer in shared memory that holds an image of the
 * contents of pg_control.	WriteControlFile() initializes pg_control
 * given a preloaded buffer, ReadControlFile() loads the buffer from
 * the pg_control file (during postmaster or standalone-backend startup),
 * and UpdateControlFile() rewrites pg_control after we modify xlog state.
 *
 * For simplicity, WriteControlFile() initializes the fields of pg_control
 * that are related to checking backend/database compatibility, and
 * ReadControlFile() verifies they are correct.  We could split out the
 * I/O and compatibility-check functions, but there seems no need currently.
 */
static void
WriteControlFile(void)
{
	int			fd;
	char		buffer[PG_CONTROL_SIZE];		/* need not be aligned */

	/*
	 * Initialize version and compatibility-check fields
	 */
	ControlFile->pg_control_version = PG_CONTROL_VERSION;
	ControlFile->catalog_version_no = CATALOG_VERSION_NO;

	ControlFile->maxAlign = MAXIMUM_ALIGNOF;
	ControlFile->floatFormat = FLOATFORMAT_VALUE;

	ControlFile->blcksz = BLCKSZ;
	ControlFile->relseg_size = RELSEG_SIZE;
	ControlFile->xlog_blcksz = XLOG_BLCKSZ;
	ControlFile->xlog_seg_size = XLOG_SEG_SIZE;

	ControlFile->nameDataLen = NAMEDATALEN;
	ControlFile->indexMaxKeys = INDEX_MAX_KEYS;

	ControlFile->toast_max_chunk_size = TOAST_MAX_CHUNK_SIZE;

#ifdef HAVE_INT64_TIMESTAMP
	ControlFile->enableIntTimes = true;
#else
	ControlFile->enableIntTimes = false;
#endif
	ControlFile->float4ByVal = FLOAT4PASSBYVAL;
	ControlFile->float8ByVal = FLOAT8PASSBYVAL;

	/* Contents are protected with a CRC */
	INIT_CRC32C(ControlFile->crc);
	COMP_CRC32C(ControlFile->crc,
			   (char *) ControlFile,
			   offsetof(ControlFileData, crc));
	FIN_CRC32C(ControlFile->crc);

	/*
	 * We write out PG_CONTROL_SIZE bytes into pg_control, zero-padding the
	 * excess over sizeof(ControlFileData).  This reduces the odds of
	 * premature-EOF errors when reading pg_control.  We'll still fail when we
	 * check the contents of the file, but hopefully with a more specific
	 * error than "couldn't read pg_control".
	 */
	if (sizeof(ControlFileData) > PG_CONTROL_SIZE)
		elog(PANIC, "sizeof(ControlFileData) is larger than PG_CONTROL_SIZE; fix either one");

	memset(buffer, 0, PG_CONTROL_SIZE);
	memcpy(buffer, ControlFile, sizeof(ControlFileData));

	fd = BasicOpenFile(XLOG_CONTROL_FILE,
					   O_RDWR | O_CREAT | O_EXCL | PG_BINARY,
					   S_IRUSR | S_IWUSR);
	if (fd < 0)
		ereport(PANIC,
				(errcode_for_file_access(),
				 errmsg("could not create control file \"%s\": %m",
						XLOG_CONTROL_FILE)));

	errno = 0;
	if (write(fd, buffer, PG_CONTROL_SIZE) != PG_CONTROL_SIZE)
	{
		/* if write didn't set errno, assume problem is no disk space */
		if (errno == 0)
			errno = ENOSPC;
		ereport(PANIC,
				(errcode_for_file_access(),
				 errmsg("could not write to control file: %m")));
	}

	if (pg_fsync(fd) != 0)
		ereport(PANIC,
				(errcode_for_file_access(),
				 errmsg("could not fsync control file: %m")));

	if (close(fd))
		ereport(PANIC,
				(errcode_for_file_access(),
				 errmsg("could not close control file: %m")));

	ControlFileWatcherSaveInitial();
}

static void
ReadControlFile(void)
{
	pg_crc32	crc;
	int			fd;

	/*
	 * Read data...
	 */
	fd = BasicOpenFile(XLOG_CONTROL_FILE,
					   O_RDWR | PG_BINARY,
					   S_IRUSR | S_IWUSR);
	if (fd < 0)
		ereport(PANIC,
				(errcode_for_file_access(),
				 errmsg("could not open control file \"%s\": %m",
						XLOG_CONTROL_FILE)));

	if (read(fd, ControlFile, sizeof(ControlFileData)) != sizeof(ControlFileData))
		ereport(PANIC,
				(errcode_for_file_access(),
				 errmsg("could not read from control file: %m")));

	close(fd);

	/*
	 * Check for expected pg_control format version.  If this is wrong, the
	 * CRC check will likely fail because we'll be checking the wrong number
	 * of bytes.  Complaining about wrong version will probably be more
	 * enlightening than complaining about wrong CRC.
	 */

	if (ControlFile->pg_control_version != PG_CONTROL_VERSION && ControlFile->pg_control_version % 65536 == 0 && ControlFile->pg_control_version / 65536 != 0)
		ereport(FATAL,
				(errmsg("database files are incompatible with server"),
				 errdetail("The database cluster was initialized with PG_CONTROL_VERSION %d (0x%08x),"
		 " but the server was compiled with PG_CONTROL_VERSION %d (0x%08x).",
			ControlFile->pg_control_version, ControlFile->pg_control_version,
						   PG_CONTROL_VERSION, PG_CONTROL_VERSION),
				 errhint("This could be a problem of mismatched byte ordering.  It looks like you need to initdb.")));

	if (ControlFile->pg_control_version != PG_CONTROL_VERSION)
		ereport(FATAL,
				(errmsg("database files are incompatible with server"),
				 errdetail("The database cluster was initialized with PG_CONTROL_VERSION %d,"
				  " but the server was compiled with PG_CONTROL_VERSION %d.",
						ControlFile->pg_control_version, PG_CONTROL_VERSION),
				 errhint("It looks like you need to initdb.")));

	/* Now check the CRC. */
	INIT_CRC32C(crc);
	COMP_CRC32C(crc,
			   (char *) ControlFile,
			   offsetof(ControlFileData, crc));
	FIN_CRC32C(crc);

	if (!EQ_CRC32C(crc, ControlFile->crc))
		ereport(FATAL,
				(errmsg("incorrect checksum in control file")));

	/*
	 * Do compatibility checking immediately.  If the database isn't
	 * compatible with the backend executable, we want to abort before we can
	 * possibly do any damage.
	 */
	if (ControlFile->catalog_version_no != CATALOG_VERSION_NO)
		ereport(FATAL,
				(errmsg("database files are incompatible with server"),
				 errdetail("The database cluster was initialized with CATALOG_VERSION_NO %d,"
				  " but the server was compiled with CATALOG_VERSION_NO %d.",
						ControlFile->catalog_version_no, CATALOG_VERSION_NO),
				 errhint("It looks like you need to initdb.")));
	if (ControlFile->maxAlign != MAXIMUM_ALIGNOF)
		ereport(FATAL,
				(errmsg("database files are incompatible with server"),
		   errdetail("The database cluster was initialized with MAXALIGN %d,"
					 " but the server was compiled with MAXALIGN %d.",
					 ControlFile->maxAlign, MAXIMUM_ALIGNOF),
				 errhint("It looks like you need to initdb.")));
	if (ControlFile->floatFormat != FLOATFORMAT_VALUE)
		ereport(FATAL,
				(errmsg("database files are incompatible with server"),
				 errdetail("The database cluster appears to use a different floating-point number format than the server executable."),
				 errhint("It looks like you need to initdb.")));
	if (ControlFile->blcksz != BLCKSZ)
		ereport(FATAL,
				(errmsg("database files are incompatible with server"),
			 errdetail("The database cluster was initialized with BLCKSZ %d,"
					   " but the server was compiled with BLCKSZ %d.",
					   ControlFile->blcksz, BLCKSZ),
				 errhint("It looks like you need to recompile or initdb.")));
	if (ControlFile->relseg_size != RELSEG_SIZE)
		ereport(FATAL,
				(errmsg("database files are incompatible with server"),
		errdetail("The database cluster was initialized with RELSEG_SIZE %d,"
				  " but the server was compiled with RELSEG_SIZE %d.",
				  ControlFile->relseg_size, RELSEG_SIZE),
				 errhint("It looks like you need to recompile or initdb.")));
	if (ControlFile->xlog_blcksz != XLOG_BLCKSZ)
		ereport(FATAL,
				(errmsg("database files are incompatible with server"),
		errdetail("The database cluster was initialized with XLOG_BLCKSZ %d,"
				  " but the server was compiled with XLOG_BLCKSZ %d.",
				  ControlFile->xlog_blcksz, XLOG_BLCKSZ),
				 errhint("It looks like you need to recompile or initdb.")));
	if (ControlFile->xlog_seg_size != XLOG_SEG_SIZE)
		ereport(FATAL,
				(errmsg("database files are incompatible with server"),
				 errdetail("The database cluster was initialized with XLOG_SEG_SIZE %d,"
					   " but the server was compiled with XLOG_SEG_SIZE %d.",
						   ControlFile->xlog_seg_size, XLOG_SEG_SIZE),
				 errhint("It looks like you need to recompile or initdb.")));
	if (ControlFile->nameDataLen != NAMEDATALEN)
		ereport(FATAL,
				(errmsg("database files are incompatible with server"),
		errdetail("The database cluster was initialized with NAMEDATALEN %d,"
				  " but the server was compiled with NAMEDATALEN %d.",
				  ControlFile->nameDataLen, NAMEDATALEN),
				 errhint("It looks like you need to recompile or initdb.")));
	if (ControlFile->indexMaxKeys != INDEX_MAX_KEYS)
		ereport(FATAL,
				(errmsg("database files are incompatible with server"),
				 errdetail("The database cluster was initialized with INDEX_MAX_KEYS %d,"
					  " but the server was compiled with INDEX_MAX_KEYS %d.",
						   ControlFile->indexMaxKeys, INDEX_MAX_KEYS),
				 errhint("It looks like you need to recompile or initdb.")));
	if (ControlFile->toast_max_chunk_size != TOAST_MAX_CHUNK_SIZE)
		ereport(FATAL,
				(errmsg("database files are incompatible with server"),
				 errdetail("The database cluster was initialized with TOAST_MAX_CHUNK_SIZE %d,"
				" but the server was compiled with TOAST_MAX_CHUNK_SIZE %d.",
			  ControlFile->toast_max_chunk_size, (int) TOAST_MAX_CHUNK_SIZE),
				 errhint("It looks like you need to recompile or initdb.")));

#ifdef HAVE_INT64_TIMESTAMP
	if (ControlFile->enableIntTimes != true)
		ereport(FATAL,
				(errmsg("database files are incompatible with server"),
				 errdetail("The database cluster was initialized without HAVE_INT64_TIMESTAMP"
				  " but the server was compiled with HAVE_INT64_TIMESTAMP."),
				 errhint("It looks like you need to recompile or initdb.")));
#else
	if (ControlFile->enableIntTimes != false)
		ereport(FATAL,
				(errmsg("database files are incompatible with server"),
				 errdetail("The database cluster was initialized with HAVE_INT64_TIMESTAMP"
			   " but the server was compiled without HAVE_INT64_TIMESTAMP."),
				 errhint("It looks like you need to recompile or initdb.")));
#endif

#ifdef USE_FLOAT4_BYVAL
	if (ControlFile->float4ByVal != true)
		ereport(FATAL,
				(errmsg("database files are incompatible with server"),
				 errdetail("The database cluster was initialized without USE_FLOAT4_BYVAL"
					  " but the server was compiled with USE_FLOAT4_BYVAL."),
				 errhint("It looks like you need to recompile or initdb.")));
#else
	if (ControlFile->float4ByVal != false)
		ereport(FATAL,
				(errmsg("database files are incompatible with server"),
		errdetail("The database cluster was initialized with USE_FLOAT4_BYVAL"
				  " but the server was compiled without USE_FLOAT4_BYVAL."),
				 errhint("It looks like you need to recompile or initdb.")));
#endif

#ifdef USE_FLOAT8_BYVAL
	if (ControlFile->float8ByVal != true)
		ereport(FATAL,
				(errmsg("database files are incompatible with server"),
				 errdetail("The database cluster was initialized without USE_FLOAT8_BYVAL"
					  " but the server was compiled with USE_FLOAT8_BYVAL."),
				 errhint("It looks like you need to recompile or initdb.")));
#else
	if (ControlFile->float8ByVal != false)
		ereport(FATAL,
				(errmsg("database files are incompatible with server"),
		errdetail("The database cluster was initialized with USE_FLOAT8_BYVAL"
				  " but the server was compiled without USE_FLOAT8_BYVAL."),
				 errhint("It looks like you need to recompile or initdb.")));
#endif

	/* Make the initdb settings visible as GUC variables, too */
	SetConfigOption("data_checksums", DataChecksumsEnabled() ? "yes" : "no",
					PGC_INTERNAL, PGC_S_OVERRIDE);

	if (!ControlFileWatcher->watcherInitialized)
	{
		ControlFileWatcherSaveInitial();
	}
	else
	{
		ControlFileWatcherCheckForChange();
	}
}

static bool
XLogGetWriteAndFlushedLoc(XLogRecPtr *writeLoc, XLogRecPtr *flushedLoc)
{
	/* use volatile pointer to prevent code rearrangement */
	volatile XLogCtlData *xlogctl = XLogCtl;

	SpinLockAcquire(&xlogctl->info_lck);
	*writeLoc = xlogctl->LogwrtResult.Write;
	*flushedLoc = xlogctl->LogwrtResult.Flush;
	SpinLockRelease(&xlogctl->info_lck);

	return (writeLoc->xlogid != 0 || writeLoc->xrecoff != 0);
}

void
UpdateControlFile(void)
{
	int			fd;

	INIT_CRC32C(ControlFile->crc);
	COMP_CRC32C(ControlFile->crc,
				   (char *) ControlFile,
				   offsetof(ControlFileData, crc));
	FIN_CRC32C(ControlFile->crc);

	fd = BasicOpenFile(XLOG_CONTROL_FILE,
					   O_RDWR | PG_BINARY,
					   S_IRUSR | S_IWUSR);
	if (fd < 0)
		ereport(PANIC,
				(errcode_for_file_access(),
				 errmsg("could not open control file \"%s\": %m",
						XLOG_CONTROL_FILE)));

	errno = 0;
	if (write(fd, ControlFile, sizeof(ControlFileData)) != sizeof(ControlFileData))
	{
		/* if write didn't set errno, assume problem is no disk space */
		if (errno == 0)
			errno = ENOSPC;
		ereport(PANIC,
				(errcode_for_file_access(),
				 errmsg("could not write to control file: %m")));
	}

	if (pg_fsync(fd) != 0)
		ereport(PANIC,
				(errcode_for_file_access(),
				 errmsg("could not fsync control file: %m")));

	if (close(fd))
		ereport(PANIC,
				(errcode_for_file_access(),
				 errmsg("could not close control file: %m")));

	Assert (ControlFileWatcher->watcherInitialized);
	ControlFileWatcherCheckForChange();
}

/*
 * Returns the unique system identifier from control file.
 */
uint64
GetSystemIdentifier(void)
{
	Assert(ControlFile != NULL);
	return ControlFile->system_identifier;
}

/*
 * Initialization of shared memory for XLOG
 */
Size
XLOGShmemSize(void)
{
	Size		size;

	/* XLogCtl */
	size = sizeof(XLogCtlData);
	/* xlblocks array */
	size = add_size(size, mul_size(sizeof(XLogRecPtr), XLOGbuffers));
	/* extra alignment padding for XLOG I/O buffers */
	size = add_size(size, ALIGNOF_XLOG_BUFFER);
	/* and the buffers themselves */
	size = add_size(size, mul_size(XLOG_BLCKSZ, XLOGbuffers));

	/*
	 * Note: we don't count ControlFileData, it comes out of the "slop factor"
	 * added by CreateSharedMemoryAndSemaphores.  This lets us use this
	 * routine again below to compute the actual allocation size.
	 */

	/*
	 * Similary, we also don't PgControlWatch for the above reasons, too.
	 */

	return size;
}

void
XLOGShmemInit(void)
{
	bool		foundCFile,
				foundXLog,
				foundCFileWatcher;
	char	   *allocptr;

	ControlFile = (ControlFileData *)
		ShmemInitStruct("Control File", sizeof(ControlFileData), &foundCFile);
	ControlFileWatcher = (ControlFileWatch *)
		ShmemInitStruct("Control File Watcher", sizeof(ControlFileWatch), &foundCFileWatcher);
	XLogCtl = (XLogCtlData *)
		ShmemInitStruct("XLOG Ctl", XLOGShmemSize(), &foundXLog);

	if (foundCFile || foundXLog || foundCFileWatcher)
	{
		/* both should be present or neither */
		Assert(foundCFile && foundXLog && foundCFileWatcher);
		return;
	}

	memset(XLogCtl, 0, sizeof(XLogCtlData));

	/*
	 * Since XLogCtlData contains XLogRecPtr fields, its sizeof should be a
	 * multiple of the alignment for same, so no extra alignment padding is
	 * needed here.
	 */
	allocptr = ((char *) XLogCtl) + sizeof(XLogCtlData);
	XLogCtl->xlblocks = (XLogRecPtr *) allocptr;
	memset(XLogCtl->xlblocks, 0, sizeof(XLogRecPtr) * XLOGbuffers);
	allocptr += sizeof(XLogRecPtr) * XLOGbuffers;

	/*
	 * Align the start of the page buffers to an ALIGNOF_XLOG_BUFFER boundary.
	 */
	allocptr = (char *) TYPEALIGN(ALIGNOF_XLOG_BUFFER, allocptr);
	XLogCtl->pages = allocptr;
	memset(XLogCtl->pages, 0, (Size) XLOG_BLCKSZ * XLOGbuffers);

	/*
	 * Do basic initialization of XLogCtl shared data. (StartupXLOG will fill
	 * in additional info.)
	 */
	XLogCtl->XLogCacheBlck = XLOGbuffers - 1;
	XLogCtl->SharedRecoveryInProgress = true;
	XLogCtl->Insert.currpage = (XLogPageHeader) (XLogCtl->pages);
	SpinLockInit(&XLogCtl->info_lck);
	InitSharedLatch(&XLogCtl->recoveryWakeupLatch);

	XLogCtl->haveLastCheckpointLoc = false;
	memset(&XLogCtl->lastCheckpointLoc, 0, sizeof(XLogRecPtr));
	memset(&XLogCtl->lastCheckpointEndLoc, 0, sizeof(XLogRecPtr));

	/*
	 * Initialize the shared memory by the parameter given to postmaster.
	 * GpStandbyDbid could be inconsistent with the catalog if the postmaster
	 * is given wrong id, but there is no chance to check it in this early
	 * stage of startup, and this is how we have been doing historically.
	 */
	XLogCtl->standbyDbid = GpStandbyDbid;
}

/**
 * This should be called when we are sure that it is safe to try to read the control file and BEFORE
 *  we have launched any child processes that need access to collation and ctype data.
 *
 * It is not safe to read the control file on a mirror because it may not be synchronized
 */
void
XLogStartupInit(void)
{
	/*
	 * If we are not in bootstrap mode, pg_control should already exist. Read
	 * and validate it immediately (see comments in ReadControlFile() for the
	 * reasons why).
	 */
	if (!IsBootstrapProcessingMode())
		ReadControlFile();
}

/*
 * Are checksums enabled for data pages?
 */
bool
DataChecksumsEnabled(void)
{
	Assert(ControlFile != NULL);
	return (ControlFile->data_checksum_version > 0);
}

/*
 * This func must be called ONCE on system install.  It creates pg_control
 * and the initial XLOG segment.
 */
void
BootStrapXLOG(void)
{
	CheckPoint	checkPoint;
	char	   *buffer;
	XLogPageHeader page;
	XLogLongPageHeader longpage;
	XLogRecord *record;
	bool		use_existent;
	uint64		sysidentifier;
	struct timeval tv;
	pg_crc32	crc;

	/*
	 * Select a hopefully-unique system identifier code for this installation.
	 * We use the result of gettimeofday(), including the fractional seconds
	 * field, as being about as unique as we can easily get.  (Think not to
	 * use random(), since it hasn't been seeded and there's no portable way
	 * to seed it other than the system clock value...)  The upper half of the
	 * uint64 value is just the tv_sec part, while the lower half is the XOR
	 * of tv_sec and tv_usec.  This is to ensure that we don't lose uniqueness
	 * unnecessarily if "uint64" is really only 32 bits wide.  A person
	 * knowing this encoding can determine the initialization time of the
	 * installation, which could perhaps be useful sometimes.
	 */
	gettimeofday(&tv, NULL);
	sysidentifier = ((uint64) tv.tv_sec) << 32;
	sysidentifier |= (uint32) (tv.tv_sec | tv.tv_usec);

	/* First timeline ID is always 1 */
	ThisTimeLineID = 1;

	/* page buffer must be aligned suitably for O_DIRECT */
	buffer = (char *) palloc(XLOG_BLCKSZ + ALIGNOF_XLOG_BUFFER);
	page = (XLogPageHeader) TYPEALIGN(ALIGNOF_XLOG_BUFFER, buffer);
	memset(page, 0, XLOG_BLCKSZ);

	/*
	 * Set up information for the initial checkpoint record
	 *
	 * The initial checkpoint record is written to the beginning of the WAL
	 * segment with logid=0 logseg=1. The very first WAL segment, 0/0, is not
	 * used, so that we can use 0/0 to mean "before any valid WAL segment".
	 */
	checkPoint.redo.xlogid = 0;
	checkPoint.redo.xrecoff = XLogSegSize + SizeOfXLogLongPHD;
	checkPoint.ThisTimeLineID = ThisTimeLineID;
	checkPoint.nextXidEpoch = 0;
	checkPoint.nextXid = FirstNormalTransactionId;
	checkPoint.nextOid = FirstBootstrapObjectId;
	checkPoint.nextRelfilenode = FirstNormalObjectId;
	checkPoint.nextMulti = FirstMultiXactId;
	checkPoint.nextMultiOffset = 0;
	checkPoint.time = (pg_time_t) time(NULL);

	ShmemVariableCache->nextXid = checkPoint.nextXid;
	ShmemVariableCache->nextOid = checkPoint.nextOid;
	ShmemVariableCache->oidCount = 0;
	ShmemVariableCache->nextRelfilenode = checkPoint.nextRelfilenode;
	ShmemVariableCache->relfilenodeCount = 0;
	MultiXactSetNextMXact(checkPoint.nextMulti, checkPoint.nextMultiOffset);

	/* Set up the XLOG page header */
	page->xlp_magic = XLOG_PAGE_MAGIC;
	page->xlp_info = XLP_LONG_HEADER;
	page->xlp_tli = ThisTimeLineID;
	page->xlp_pageaddr.xlogid = 0;
	page->xlp_pageaddr.xrecoff = XLogSegSize;
	longpage = (XLogLongPageHeader) page;
	longpage->xlp_sysid = sysidentifier;
	longpage->xlp_seg_size = XLogSegSize;
	longpage->xlp_xlog_blcksz = XLOG_BLCKSZ;

	/* Insert the initial checkpoint record */
	record = (XLogRecord *) ((char *) page + SizeOfXLogLongPHD);
	record->xl_prev.xlogid = 0;
	record->xl_prev.xrecoff = XLogSegSize;
	record->xl_xid = InvalidTransactionId;
	record->xl_tot_len = SizeOfXLogRecord + sizeof(checkPoint);
	record->xl_len = sizeof(checkPoint);
	record->xl_info = XLOG_CHECKPOINT_SHUTDOWN;
	record->xl_rmid = RM_XLOG_ID;
	memcpy(XLogRecGetData(record), &checkPoint, sizeof(checkPoint));

	INIT_CRC32C(crc);
	COMP_CRC32C(crc, &checkPoint, sizeof(checkPoint));
	COMP_CRC32C(crc, (char *) record + sizeof(pg_crc32),
			   SizeOfXLogRecord - sizeof(pg_crc32));
	FIN_CRC32C(crc);

	record->xl_crc = crc;

	/* Create first XLOG segment file */
	use_existent = false;
	openLogFile = XLogFileInit(0, 1, &use_existent, false);

	/* Write the first page with the initial record */
	errno = 0;
	if (write(openLogFile, page, XLOG_BLCKSZ) != XLOG_BLCKSZ)
	{
		/* if write didn't set errno, assume problem is no disk space */
		if (errno == 0)
			errno = ENOSPC;
		ereport(PANIC,
				(errcode_for_file_access(),
			  errmsg("could not write bootstrap transaction log file: %m")));
	}

	if (pg_fsync(openLogFile) != 0)
		ereport(PANIC,
				(errcode_for_file_access(),
			  errmsg("could not fsync bootstrap transaction log file: %m")));

	if (close(openLogFile))
		ereport(PANIC,
				(errcode_for_file_access(),
			  errmsg("could not close bootstrap transaction log file: %m")));

	openLogFile = -1;

	/* Now create pg_control */

	memset(ControlFile, 0, sizeof(ControlFileData));
	/* Initialize pg_control status fields */
	ControlFile->system_identifier = sysidentifier;
	ControlFile->state = DB_SHUTDOWNED;
	ControlFile->time = checkPoint.time;
	ControlFile->checkPoint = checkPoint.redo;
	ControlFile->checkPointCopy = checkPoint;
	ControlFile->data_checksum_version = bootstrap_data_checksum_version;

	/* some additional ControlFile fields are set in WriteControlFile() */

	WriteControlFile();

	/* Bootstrap the commit log, too */
	BootStrapCLOG();
	BootStrapSUBTRANS();
	BootStrapMultiXact();
	DistributedLog_BootStrap();

	pfree(buffer);
}

static char *
str_time(pg_time_t tnow)
{
	static char buf[128];

	pg_strftime(buf, sizeof(buf),
				"%Y-%m-%d %H:%M:%S %Z",
				pg_localtime(&tnow, log_timezone));

	return buf;
}

/*
 * See if there is a recovery command file (recovery.conf), and if so
 * read in parameters for recovery in standby mode.
 *
 * XXX longer term intention is to expand this to
 * cater for additional parameters and controls
 * possibly use a flex lexer similar to the GUC one
 */
void
XLogReadRecoveryCommandFile(int emode)
{
	FILE	   *fd;
	char		cmdline[MAXPGPATH];
	TimeLineID	rtli = 0;
	bool		rtliGiven = false;
	bool		syntaxError = false;

	fd = AllocateFile(RECOVERY_COMMAND_FILE, "r");
	if (fd == NULL)
	{
		if (errno == ENOENT)
			return;				/* not there, so no recovery in standby mode */
		ereport(FATAL,
				(errcode_for_file_access(),
				 errmsg("could not open recovery command file \"%s\": %m",
						RECOVERY_COMMAND_FILE)));
	}

	ereport(emode,
			(errmsg("Found recovery.conf file, checking appropriate parameters "
					" for recovery in standby mode")));

	/*
	 * Parse the file...
	 */
	while (fgets(cmdline, sizeof(cmdline), fd) != NULL)
	{
		/* skip leading whitespace and check for # comment */
		char	   *ptr;
		char	   *tok1;
		char	   *tok2;

		for (ptr = cmdline; *ptr; ptr++)
		{
			if (!isspace((unsigned char) *ptr))
				break;
		}
		if (*ptr == '\0' || *ptr == '#')
			continue;

		/* identify the quoted parameter value */
		tok1 = strtok(ptr, "'");
		if (!tok1)
		{
			syntaxError = true;
			break;
		}
		tok2 = strtok(NULL, "'");
		if (!tok2)
		{
			syntaxError = true;
			break;
		}
		/* reparse to get just the parameter name */
		tok1 = strtok(ptr, " \t=");
		if (!tok1)
		{
			syntaxError = true;
			break;
		}

		if (strcmp(tok1, "primary_conninfo") == 0)
		{
			PrimaryConnInfo = pstrdup(tok2);
			ereport(emode,
					(errmsg("primary_conninfo = \"%s\"",
							PrimaryConnInfo)));
		}
		else if (strcmp(tok1, "recovery_end_command") == 0)
		{
			recoveryEndCommand = pstrdup(tok2);
			ereport(LOG,
					(errmsg("recovery_end_command = '%s'",
							recoveryEndCommand)));
		}
		else if (strcmp(tok1, "recovery_target_timeline") == 0)
		{
			rtliGiven = true;
			if (strcmp(tok2, "latest") == 0)
				rtli = 0;
			else
			{
				errno = 0;
				rtli = (TimeLineID) strtoul(tok2, NULL, 0);
				if (errno == EINVAL || errno == ERANGE)
					ereport(FATAL,
							(errmsg("recovery_target_timeline is not a valid number: \"%s\"",
									tok2)));
			}
			if (rtli)
				ereport(LOG,
						(errmsg("recovery_target_timeline = %u", rtli)));
			else
				ereport(LOG,
						(errmsg("recovery_target_timeline = latest")));
		}
		else if (strcmp(tok1, "recovery_target_xid") == 0)
		{
			errno = 0;
			recoveryTargetXid = (TransactionId) strtoul(tok2, NULL, 0);
			if (errno == EINVAL || errno == ERANGE)
				ereport(FATAL,
				 (errmsg("recovery_target_xid is not a valid number: \"%s\"",
						 tok2)));
			ereport(LOG,
					(errmsg("recovery_target_xid = %u",
							recoveryTargetXid)));
			recoveryTarget = true;
			recoveryTargetExact = true;
		}
		else if (strcmp(tok1, "recovery_target_time") == 0)
		{
			/*
			 * if recovery_target_xid specified, then this overrides
			 * recovery_target_time
			 */
			if (recoveryTargetExact)
				continue;
			recoveryTarget = true;
			recoveryTargetExact = false;

			/*
			 * Convert the time string given by the user to TimestampTz form.
			 */
			recoveryTargetTime =
				DatumGetTimestampTz(DirectFunctionCall3(timestamptz_in,
														CStringGetDatum(tok2),
												ObjectIdGetDatum(InvalidOid),
														Int32GetDatum(-1)));
			ereport(LOG,
					(errmsg("recovery_target_time = '%s'",
							timestamptz_to_str(recoveryTargetTime))));
		}
		else if (strcmp(tok1, "recovery_target_inclusive") == 0)
		{
			/*
			 * does nothing if a recovery_target is not also set
			 */
			if (!parse_bool(tok2, &recoveryTargetInclusive))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("parameter \"recovery_target_inclusive\" requires a Boolean value")));
			ereport(LOG,
					(errmsg("standby_mode = %s", tok2)));
		}
		else if (strcmp(tok1, "standby_mode") == 0)
		{
			if (!parse_bool(tok2, &StandbyModeRequested))
				  ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					  errmsg("parameter \"standby_mode\" requires a Boolean value")));
		}
		else
			ereport(FATAL,
					(errmsg("unrecognized recovery parameter \"%s\"",
							tok1)));
	}

	FreeFile(fd);

	if (syntaxError)
		ereport(FATAL,
				(errmsg("syntax error in recovery command file: %s",
						cmdline),
			  errhint("Lines should have the format parameter = 'value'.")));

	/*
	 * Check for compulsory parameters
	 */
	if (StandbyModeRequested)
	{
		if (PrimaryConnInfo == NULL)
			ereport(FATAL,
					(errmsg("recovery command file \"%s\" primary_conninfo not specified",
							RECOVERY_COMMAND_FILE),
					 errhint("The database server in standby mode needs primary_connection to connect to primary.")));
	}
	else
	{
		/* Currently, standby mode request is a must if recovery.conf file exists */
		ereport(FATAL,
				(errmsg("recovery command file \"%s\" request for standby mode not specified",
						RECOVERY_COMMAND_FILE)));
	}
}

/*
 * Exit archive-recovery state
 */
static void
exitArchiveRecovery(TimeLineID endTLI, uint32 endLogId, uint32 endLogSeg)
{
	char		recoveryPath[MAXPGPATH];
	char		xlogpath[MAXPGPATH];
	XLogRecPtr	InvalidXLogRecPtr = {0, 0};

	/*
	 * We are no longer in archive recovery state.
	 */
	InArchiveRecovery = false;

	/*
	 * Update min recovery point one last time.
	 */
	UpdateMinRecoveryPoint(InvalidXLogRecPtr, true);

	/*
	 * We should have the ending log segment currently open.  Verify, and then
	 * close it (to avoid problems on Windows with trying to rename or delete
	 * an open file).
	 */
	Assert(readFile >= 0);
	Assert(readId == endLogId);
	Assert(readSeg == endLogSeg);

	close(readFile);
	readFile = -1;

	/*
	 * If the segment was fetched from archival storage, we want to replace
	 * the existing xlog segment (if any) with the archival version.  This is
	 * because whatever is in XLOGDIR is very possibly older than what we have
	 * from the archives, since it could have come from restoring a PGDATA
	 * backup.	In any case, the archival version certainly is more
	 * descriptive of what our current database state is, because that is what
	 * we replayed from.
	 *
	 * Note that if we are establishing a new timeline, ThisTimeLineID is
	 * already set to the new value, and so we will create a new file instead
	 * of overwriting any existing file.  (This is, in fact, always the case
	 * at present.)
	 */
	snprintf(recoveryPath, MAXPGPATH, XLOGDIR "/RECOVERYXLOG");
	XLogFilePath(xlogpath, ThisTimeLineID, endLogId, endLogSeg);

	if (restoredFromArchive)
	{
		ereport(DEBUG3,
				(errmsg_internal("moving last restored xlog to \"%s\"",
								 xlogpath)));
		unlink(xlogpath);		/* might or might not exist */
		if (rename(recoveryPath, xlogpath) != 0)
			ereport(FATAL,
					(errcode_for_file_access(),
					 errmsg("could not rename file \"%s\" to \"%s\": %m",
							recoveryPath, xlogpath)));
		/* XXX might we need to fix permissions on the file? */
	}
	else
	{
		/*
		 * If the latest segment is not archival, but there's still a
		 * RECOVERYXLOG laying about, get rid of it.
		 */
		unlink(recoveryPath);	/* ignore any error */

		/*
		 * If we are establishing a new timeline, we have to copy data from
		 * the last WAL segment of the old timeline to create a starting WAL
		 * segment for the new timeline.
		 *
		 * Notify the archiver that the last WAL segment of the old timeline
		 * is ready to copy to archival storage. Otherwise, it is not archived
		 * for a while.
		 */
		if (endTLI != ThisTimeLineID)
		{
			XLogFileCopy(endLogId, endLogSeg,
						 endTLI, endLogId, endLogSeg);

			if (XLogArchivingActive())
			{
				XLogFileName(xlogpath, endTLI, endLogId, endLogSeg);
				XLogArchiveNotify(xlogpath);
			}
		}
	}

	/*
	 * Let's just make real sure there are not .ready or .done flags posted
	 * for the new segment.
	 */
	XLogFileName(xlogpath, ThisTimeLineID, endLogId, endLogSeg);
	XLogArchiveCleanup(xlogpath);

	/* Get rid of any remaining recovered timeline-history file, too */
	snprintf(recoveryPath, MAXPGPATH, XLOGDIR "/RECOVERYHISTORY");
	unlink(recoveryPath);		/* ignore any error */

	/*
	 * Rename the config file out of the way, so that we don't accidentally
	 * re-enter archive recovery mode in a subsequent crash.
	 */
	unlink(RECOVERY_COMMAND_DONE);
	if (rename(RECOVERY_COMMAND_FILE, RECOVERY_COMMAND_DONE) != 0)
		ereport(FATAL,
				(errcode_for_file_access(),
				 errmsg("could not rename file \"%s\" to \"%s\": %m",
						RECOVERY_COMMAND_FILE, RECOVERY_COMMAND_DONE)));
}

/*
 * For point-in-time recovery, this function decides whether we want to
 * stop applying the XLOG at or after the current record.
 *
 * Returns TRUE if we are stopping, FALSE otherwise.  On TRUE return,
 * *includeThis is set TRUE if we should apply this record before stopping.
 *
 * We also track the timestamp of the latest applied COMMIT/ABORT record
 * in recoveryLastXTime, for logging purposes.
 * Also, some information is saved in recoveryStopXid et al for use in
 * annotating the new timeline's history file.
 */
static bool
recoveryStopsHere(XLogRecord *record, bool *includeThis)
{
	bool		stopsHere;
	uint8		record_info;
	TimestampTz recordXtime;

	/* We only consider stopping at COMMIT or ABORT records */
	if (record->xl_rmid != RM_XACT_ID)
		return false;
	record_info = record->xl_info & ~XLR_INFO_MASK;
	if (record_info == XLOG_XACT_COMMIT)
	{
		xl_xact_commit *recordXactCommitData;

		recordXactCommitData = (xl_xact_commit *) XLogRecGetData(record);
		recordXtime = recordXactCommitData->xact_time;
	}
	else if (record_info == XLOG_XACT_ABORT)
	{
		xl_xact_abort *recordXactAbortData;

		recordXactAbortData = (xl_xact_abort *) XLogRecGetData(record);
		recordXtime = recordXactAbortData->xact_time;
	}
	else
		return false;

	/* Do we have a PITR target at all? */
	if (!recoveryTarget)
	{
		recoveryLastXTime = recordXtime;
		return false;
	}

	if (recoveryTargetExact)
	{
		/*
		 * there can be only one transaction end record with this exact
		 * transactionid
		 *
		 * when testing for an xid, we MUST test for equality only, since
		 * transactions are numbered in the order they start, not the order
		 * they complete. A higher numbered xid will complete before you about
		 * 50% of the time...
		 */
		stopsHere = (record->xl_xid == recoveryTargetXid);
		if (stopsHere)
			*includeThis = recoveryTargetInclusive;
	}
	else
	{
		/*
		 * there can be many transactions that share the same commit time, so
		 * we stop after the last one, if we are inclusive, or stop at the
		 * first one if we are exclusive
		 */
		if (recoveryTargetInclusive)
			stopsHere = (recordXtime > recoveryTargetTime);
		else
			stopsHere = (recordXtime >= recoveryTargetTime);
		if (stopsHere)
			*includeThis = false;
	}

	if (stopsHere)
	{
		recoveryStopXid = record->xl_xid;
		recoveryStopTime = recordXtime;
		recoveryStopAfter = *includeThis;

		if (record_info == XLOG_XACT_COMMIT)
		{
			if (recoveryStopAfter)
				ereport(LOG,
						(errmsg("recovery stopping after commit of transaction %u, time %s",
								recoveryStopXid,
								timestamptz_to_str(recoveryStopTime))));
			else
				ereport(LOG,
						(errmsg("recovery stopping before commit of transaction %u, time %s",
								recoveryStopXid,
								timestamptz_to_str(recoveryStopTime))));
		}
		else
		{
			if (recoveryStopAfter)
				ereport(LOG,
						(errmsg("recovery stopping after abort of transaction %u, time %s",
								recoveryStopXid,
								timestamptz_to_str(recoveryStopTime))));
			else
				ereport(LOG,
						(errmsg("recovery stopping before abort of transaction %u, time %s",
								recoveryStopXid,
								timestamptz_to_str(recoveryStopTime))));
		}

		if (recoveryStopAfter)
			recoveryLastXTime = recordXtime;
	}
	else
		recoveryLastXTime = recordXtime;

	return stopsHere;
}

/*
 * Save timestamp of the next chunk of WAL records to apply.
 *
 * We keep this in XLogCtl, not a simple static variable, so that it can be
 * seen by all backends.
 */
static void
SetCurrentChunkStartTime(TimestampTz xtime)
{
	/* use volatile pointer to prevent code rearrangement */
	volatile XLogCtlData *xlogctl = XLogCtl;

	SpinLockAcquire(&xlogctl->info_lck);
	xlogctl->currentChunkStartTime = xtime;
	SpinLockRelease(&xlogctl->info_lck);
}

static void
printEndOfXLogFile(XLogRecPtr	*loc)
{
	uint32 seg = loc->xrecoff / XLogSegSize;

	XLogRecPtr roundedDownLoc;

	XLogRecord *record;
	XLogRecPtr	LastRec;

	/*
	 * Go back to the beginning of the log file and read forward to find
	 * the end of the transaction log.
	 */
	roundedDownLoc.xlogid = loc->xlogid;
	roundedDownLoc.xrecoff = (seg * XLogSegSize) + SizeOfXLogLongPHD;

	XLogCloseReadRecord();

	record = XLogReadRecord(&roundedDownLoc, false, LOG);
	if (record == NULL)
	{
		elog(LOG,"Couldn't read transaction log file (logid %d, seg %d)",
			 loc->xlogid, seg);
		return;
	}

	do
	{
		LastRec = ReadRecPtr;

		record = XLogReadRecord(NULL, false, DEBUG5);
	} while (record != NULL);

	record = XLogReadRecord(&LastRec, false, ERROR);

	elog(LOG,"found end of transaction log file %s",
		 XLogLocationToString_Long(&EndRecPtr));

	XLogCloseReadRecord();
}

static void
ApplyStartupRedo(
	XLogRecPtr		*beginLoc,

	XLogRecPtr		*lsn,

	XLogRecord		*record)
{
	/* use volatile pointer to prevent code rearrangement */
	volatile XLogCtlData *xlogctl = XLogCtl;
	RedoErrorCallBack redoErrorCallBack;

	ErrorContextCallback errcontext;

	/* Setup error traceback support for ereport() */
	redoErrorCallBack.location = *beginLoc;
	redoErrorCallBack.record = record;

	errcontext.callback = rm_redo_error_callback;
	errcontext.arg = (void *) &redoErrorCallBack;
	errcontext.previous = error_context_stack;
	error_context_stack = &errcontext;

	/* nextXid must be beyond record's xid */
	if (TransactionIdFollowsOrEquals(record->xl_xid,
									 ShmemVariableCache->nextXid))
	{
		ShmemVariableCache->nextXid = record->xl_xid;
		TransactionIdAdvance(ShmemVariableCache->nextXid);
	}

	/*
	 * Update shared replayEndRecPtr before replaying this record,
	 * so that XLogFlush will update minRecoveryPoint correctly.
	 */
	SpinLockAcquire(&xlogctl->info_lck);
	xlogctl->replayEndRecPtr = EndRecPtr;
	SpinLockRelease(&xlogctl->info_lck);

	RmgrTable[record->xl_rmid].rm_redo(*beginLoc, *lsn, record);

	/*
	 * After redo, check whether the backup pages associated with
	 * the WAL record are consistent with the existing pages. This
	 * check is done only if consistency check is enabled for this
	 * record.
	 */
	if ((record->xl_extended_info & XLR_CHECK_CONSISTENCY) != 0)
		checkXLogConsistency(record, *lsn);
	/* Pop the error context stack */
	error_context_stack = errcontext.previous;

}

/*
 * Process passed checkpoint record either during normal recovery or
 * in standby mode.
 *
 * If in standby mode, master mirroring information stored by the checkpoint
 * record is processed as well.
 */
static void
XLogProcessCheckpointRecord(XLogRecord *rec, XLogRecPtr loc)
{
	CheckpointExtendedRecord ckptExtended;

	UnpackCheckPointRecord(rec, &ckptExtended);

	if (ckptExtended.dtxCheckpoint)
	{
		/* Handle the DTX information. */
		UtilityModeFindOrCreateDtmRedoFile();
		redoDtxCheckPoint(ckptExtended.dtxCheckpoint);
		UtilityModeCloseDtmRedoFile();
	}
}


/*
 * This must be called ONCE during postmaster or standalone-backend startup
 */
void
StartupXLOG(void)
{
	XLogCtlInsert *Insert;
	CheckPoint	checkPoint;
	bool		wasShutdown;
	bool		reachedStopPoint = false;
	bool		haveBackupLabel = false;
	XLogRecPtr	RecPtr,
				LastRec,
				checkPointLoc,
				EndOfLog;
	uint32		endLogId;
	uint32		endLogSeg;
	XLogRecord *record;
	uint32		freespace;
	bool		backupEndRequired = false;
	bool		bgwriterLaunched = false;

	/*
	 * Read control file and check XLOG status looks valid.
	 *
	 * Note: in most control paths, *ControlFile is already valid and we need
	 * not do ReadControlFile() here, but might as well do it to be sure.
	 */
	ReadControlFile();

	if (ControlFile->state < DB_SHUTDOWNED ||
		ControlFile->state > DB_IN_PRODUCTION ||
		!XRecOffIsValid(ControlFile->checkPoint.xrecoff))
		ereport(FATAL,
				(errmsg("control file contains invalid data")));

	if (ControlFile->state == DB_SHUTDOWNED)
		ereport(LOG,
				(errmsg("database system was shut down at %s",
						str_time(ControlFile->time))));
	else if (ControlFile->state == DB_SHUTDOWNING)
		ereport(LOG,
				(errmsg("database system shutdown was interrupted; last known up at %s",
						str_time(ControlFile->time))));
	else if (ControlFile->state == DB_IN_CRASH_RECOVERY)
		ereport(LOG,
		   (errmsg("database system was interrupted while in recovery at %s",
				   str_time(ControlFile->time)),
			errhint("This probably means that some data is corrupted and"
					" you will have to use the last backup for recovery."),
			errSendAlert(true)));
	else if (ControlFile->state == DB_IN_STANDBY_MODE)
		ereport(LOG,
				(errmsg("database system was interrupted while in standby mode at  %s",
						str_time(ControlFile->checkPointCopy.time)),
						errhint("This probably means something unexpected happened either"
								" during replay at standby or receipt of XLog from primary."),
				 errSendAlert(true)));
	else if (ControlFile->state == DB_IN_STANDBY_PROMOTED)
		ereport(LOG,
				(errmsg("database system was interrupted after standby was promoted at %s",
						str_time(ControlFile->checkPointCopy.time)),
				 errhint("If this has occurred more than once something unexpected is happening"
				" after standby has been promoted"),
				 errSendAlert(true)));
	else if (ControlFile->state == DB_IN_PRODUCTION)
		ereport(LOG,
				(errmsg("database system was interrupted; last known up at %s",
						str_time(ControlFile->time))));

	/* This is just to allow attaching to startup process with a debugger */
#ifdef XLOG_REPLAY_DELAY
	if (ControlFile->state != DB_SHUTDOWNED)
		pg_usleep(60000000L);
#endif

	/*
	 * Verify that pg_xlog and pg_xlog/archive_status exist.  In cases where
	 * someone has performed a copy for PITR, these directories may have been
	 * excluded and need to be re-created.
	 */
	ValidateXLOGDirectoryStructure();

	/*
	 * Clear out any old relcache cache files.	This is *necessary* if we do
	 * any WAL replay, since that would probably result in the cache files
	 * being out of sync with database reality.  In theory we could leave them
	 * in place if the database had been cleanly shut down, but it seems
	 * safest to just remove them always and let them be rebuilt during the
	 * first backend startup.
	 */
	RelationCacheInitFileRemove();

	/*
	 * Initialize on the assumption we want to recover to the same timeline
	 * that's active according to pg_control.
	 */
	recoveryTargetTLI = ControlFile->checkPointCopy.ThisTimeLineID;

	/*
	 * Check for recovery control file, and if so set up state for offline
	 * recovery
	 */
	XLogReadRecoveryCommandFile(LOG);

	if (StandbyModeRequested)
	{
		Assert(ControlFile->state != DB_IN_CRASH_RECOVERY);

		/*
		 * If the standby was promoted (last time) and recovery.conf
		 * is still found this time with standby mode request,
		 * it means the standby crashed post promotion but before recovery.conf
		 * cleanup. Hence, it is not considered a standby request this time.
		 */
		if (ControlFile->state == DB_IN_STANDBY_PROMOTED)
			StandbyModeRequested = false;
	}

	/* Now we can determine the list of expected TLIs */
	expectedTLIs = XLogReadTimeLineHistory(recoveryTargetTLI);

	/*
	 * If pg_control's timeline is not in expectedTLIs, then we cannot
	 * proceed: the backup is not part of the history of the requested
	 * timeline.
	 */
	if (!list_member_int(expectedTLIs,
						 (int) ControlFile->checkPointCopy.ThisTimeLineID))
		ereport(FATAL,
				(errmsg("requested timeline %u is not a child of database system timeline %u",
						recoveryTargetTLI,
						ControlFile->checkPointCopy.ThisTimeLineID)));
	/*
	 * Save the selected recovery target timeline ID in shared memory so that
	 * other processes can see them
	 */
	XLogCtl->RecoveryTargetTLI = recoveryTargetTLI;

	if (StandbyModeRequested)
		ereport(LOG,
				(errmsg("entering standby mode")));

	/*
	 * Take ownership of the wakeup latch if we're going to sleep during
	 * recovery.
	 */
	if (StandbyModeRequested)
		OwnLatch(&XLogCtl->recoveryWakeupLatch);

	/*
	 * Allocate pages dedicated to WAL consistency checks, those had better
	 * be aligned.
	 */
	replay_image_masked = (char *) palloc(BLCKSZ);
	master_image_masked = (char *) palloc(BLCKSZ);

	if (read_backup_label(&checkPointLoc, &backupEndRequired))
	{
		/*
		 * Currently, it is assumed that a backup file exists iff a base backup
		 * has been performed and then the recovery.conf file is generated, thus
		 * standby mode has to be requested
		 */
		if (!StandbyModeRequested)
			ereport(FATAL,
					(errmsg("Found backup.label file without any standby mode request")));

		/* Activate recovery in standby mode */
		StandbyMode = true;

		Assert(backupEndRequired);

		/*
		 * When a backup_label file is present, we want to roll forward from
		 * the checkpoint it identifies, rather than using pg_control.
		 */
		record = ReadCheckpointRecord(checkPointLoc, 0);
		if (record != NULL)
		{
			memcpy(&checkPoint, XLogRecGetData(record), sizeof(CheckPoint));
			wasShutdown = (record->xl_info == XLOG_CHECKPOINT_SHUTDOWN);
			ereport(DEBUG1,
					(errmsg("checkpoint record is at %X/%X",
							checkPointLoc.xlogid, checkPointLoc.xrecoff)));
			InRecovery = true;	/* force recovery even if SHUTDOWNED */

			/*
			 * Make sure that REDO location exists. This may not be
			 * the case if there was a crash during an online backup,
			 * which left a backup_label around that references a WAL
			 * segment that's already been archived.
			 */
			if (XLByteLT(checkPoint.redo, checkPointLoc))
			{
				if (!XLogReadRecord(&(checkPoint.redo), false, LOG))
					ereport(FATAL,
							(errmsg("could not find redo location referenced by checkpoint record"),
							 errhint("If you are not restoring from a backup, try removing the file \"%s/backup_label\".", DataDir)));
			}
		}
		else
		{
			ereport(FATAL,
					(errmsg("could not locate required checkpoint record"),
					 errhint("If you are not restoring from a backup, try removing the file \"%s/backup_label\".", DataDir)));
			wasShutdown = false; /* keep compiler quiet */
		}
		/* set flag to delete it later */
		haveBackupLabel = true;
	}
	else
	{
		if (StandbyModeRequested)
		{
			/* Activate recovery in standby mode */
			StandbyMode = true;
		}

		/*
		 * Get the last valid checkpoint record.  If the latest one according
		 * to pg_control is broken, try the next-to-last one.
		 */
		checkPointLoc = ControlFile->checkPoint;
		RedoStartLSN = ControlFile->checkPointCopy.redo;

		record = ReadCheckpointRecord(checkPointLoc, 1);
		if (record != NULL)
		{
			ereport(DEBUG1,
					(errmsg("checkpoint record is at %X/%X",
							checkPointLoc.xlogid, checkPointLoc.xrecoff)));
		}
		else if (StandbyMode)
		{
			/*
			 * The last valid checkpoint record required for a streaming
			 * recovery exists in neither standby nor the primary.
			 */
			ereport(PANIC,
					(errmsg("could not locate a valid checkpoint record")));
		}
		else
		{
			printEndOfXLogFile(&checkPointLoc);

			checkPointLoc = ControlFile->prevCheckPoint;
			record = ReadCheckpointRecord(checkPointLoc, 2);
			if (record != NULL)
			{
				ereport(LOG,
						(errmsg("using previous checkpoint record at %X/%X",
							  checkPointLoc.xlogid, checkPointLoc.xrecoff)));
				InRecovery = true;		/* force recovery even if SHUTDOWNED */
			}
			else
			{
				printEndOfXLogFile(&checkPointLoc);
				ereport(PANIC,
					 (errmsg("could not locate a valid checkpoint record")));
			}
		}
		memcpy(&checkPoint, XLogRecGetData(record), sizeof(CheckPoint));
		wasShutdown = (record->xl_info == XLOG_CHECKPOINT_SHUTDOWN);
	}

	LastRec = RecPtr = checkPointLoc;

	/*
	 * Currently, standby mode (WAL based replication support) is not provided
	 * to segments.
	 * Hence it's okay to do the following only once on the segments as there
	 * will be only one checkpoint to be analyzed.
	 */
	if (GpIdentity.segindex != MASTER_CONTENT_ID)
	{
		CheckpointExtendedRecord ckptExtended;
		UnpackCheckPointRecord(record, &ckptExtended);
		if (ckptExtended.ptas)
			SetupCheckpointPreparedTransactionList(ckptExtended.ptas);
	}

	/*
	 * Find Xacts that are distributed committed from the checkpoint record and
	 * store them such that they can utilized later during DTM recovery.
	 */
	XLogProcessCheckpointRecord(record, checkPointLoc);

	ereport(DEBUG1,
			(errmsg("redo record is at %X/%X; shutdown %s",
					checkPoint.redo.xlogid, checkPoint.redo.xrecoff,
					wasShutdown ? "TRUE" : "FALSE")));
	ereport(DEBUG1,
			(errmsg("next transaction ID: %u/%u; next OID: %u; next relfilenode: %u",
					checkPoint.nextXidEpoch, checkPoint.nextXid,
					checkPoint.nextOid, checkPoint.nextRelfilenode)));
	ereport(DEBUG1,
			(errmsg("next MultiXactId: %u; next MultiXactOffset: %u",
					checkPoint.nextMulti, checkPoint.nextMultiOffset)));

	if (!TransactionIdIsNormal(checkPoint.nextXid))
		ereport(PANIC,
				(errmsg("invalid next transaction ID")));

	/* initialize shared memory variables from the checkpoint record */
	ShmemVariableCache->nextXid = checkPoint.nextXid;
	ShmemVariableCache->nextOid = checkPoint.nextOid;
	ShmemVariableCache->oidCount = 0;
	ShmemVariableCache->nextRelfilenode = checkPoint.nextRelfilenode;
	ShmemVariableCache->relfilenodeCount = 0;
	MultiXactSetNextMXact(checkPoint.nextMulti, checkPoint.nextMultiOffset);
	XLogCtl->ckptXidEpoch = checkPoint.nextXidEpoch;
	XLogCtl->ckptXid = checkPoint.nextXid;

	/*
	 * We must replay WAL entries using the same TimeLineID they were created
	 * under, so temporarily adopt the TLI indicated by the checkpoint (see
	 * also xlog_redo()).
	 */
	ThisTimeLineID = checkPoint.ThisTimeLineID;

	RedoRecPtr = XLogCtl->Insert.RedoRecPtr = checkPoint.redo;

	if (XLByteLT(RecPtr, checkPoint.redo))
		ereport(PANIC,
				(errmsg("invalid redo in checkpoint record")));

	/*
	 * Check whether we need to force recovery from WAL.  If it appears to
	 * have been a clean shutdown and we did not have a recovery.conf file,
	 * then assume no recovery needed.
	 */
	if (XLByteLT(checkPoint.redo, RecPtr))
	{
		if (wasShutdown)
			ereport(PANIC,
					(errmsg("invalid redo record in shutdown checkpoint")));
		InRecovery = true;
	}
	else if (StandbyModeRequested)
	{
		/* force recovery due to presence of recovery.conf */
		ereport(LOG,
				(errmsg("setting recovery standby mode active")));
		InRecovery = true;
	}
	else if (ControlFile->state != DB_SHUTDOWNED)
		InRecovery = true;

	if (InRecovery && !IsUnderPostmaster)
	{
		ereport(FATAL,
				(errmsg("Database must be shutdown cleanly when using single backend start")));
	}

	/* Recovery from xlog */
	if (InRecovery)
	{
		int			rmid;

		/* use volatile pointer to prevent code rearrangement */
		volatile XLogCtlData *xlogctl = XLogCtl;

		/*
		 * Update pg_control to show that we are recovering and to show the
		 * selected checkpoint as the place we are starting from. We also mark
		 * pg_control with any minimum recovery stop point
		 */
		if (StandbyMode)
		{
			ereport(LOG,
					(errmsg("recovery in standby mode in progress")));
			ControlFile->state = DB_IN_STANDBY_MODE;
		}
		else
		{
			ereport(LOG,
					(errmsg("database system was not properly shut down; "
							"automatic recovery in progress")));

			if (ControlFile->state != DB_IN_STANDBY_PROMOTED)
				ControlFile->state = DB_IN_CRASH_RECOVERY;
		}

		ControlFile->prevCheckPoint = ControlFile->checkPoint;
		ControlFile->checkPoint = checkPointLoc;
		ControlFile->checkPointCopy = checkPoint;

		if (StandbyMode)
		{
			/* initialize minRecoveryPoint if not set yet */
			if (XLByteLT(ControlFile->minRecoveryPoint, checkPoint.redo))
				ControlFile->minRecoveryPoint = checkPoint.redo;
		}

		/* Set backupStartPoint if we're starting recovery from a base backup. */
		if (haveBackupLabel)
		{
			Assert(ControlFile->state == DB_IN_STANDBY_MODE);
			ControlFile->backupStartPoint = checkPoint.redo;
			ControlFile->backupEndRequired = backupEndRequired;
		}

		ControlFile->time = (pg_time_t) time(NULL);
		/* No need to hold ControlFileLock yet, we aren't up far enough */
		UpdateControlFile();

		/* initialize our local copy of minRecoveryPoint */
		minRecoveryPoint = ControlFile->minRecoveryPoint;

		/*
		 * Reset pgstat data, because it may be invalid after recovery.
		 */
		pgstat_reset_all();

		/*
		 * If there was a backup label file, it's done its job and the info
		 * has now been propagated into pg_control.  We must get rid of the
		 * label file so that if we crash during recovery, we'll pick up at
		 * the latest recovery restartpoint instead of going all the way back
		 * to the backup start point.  It seems prudent though to just rename
		 * the file out of the way rather than delete it completely.
		 */
		if (haveBackupLabel)
		{
			unlink(BACKUP_LABEL_OLD);
			if (rename(BACKUP_LABEL_FILE, BACKUP_LABEL_OLD) != 0)
				ereport(FATAL,
						(errcode_for_file_access(),
						 errmsg("could not rename file \"%s\" to \"%s\": %m",
								BACKUP_LABEL_FILE, BACKUP_LABEL_OLD)));
		}

		UtilityModeFindOrCreateDtmRedoFile();

		/* Initialize resource managers */
		for (rmid = 0; rmid <= RM_MAX_ID; rmid++)
		{
			if (RmgrTable[rmid].rm_startup != NULL)
				RmgrTable[rmid].rm_startup();
		}

		/*
		 * Initialize shared variables for tracking progress of WAL replay,
		 * as if we had just replayed the record before the REDO location
		 * (or the checkpoint record itself, if it's a shutdown checkpoint).
		 */
		SpinLockAcquire(&xlogctl->info_lck);
		if (XLByteLT(checkPoint.redo, RecPtr))
			xlogctl->replayEndRecPtr = checkPoint.redo;
		else
			xlogctl->replayEndRecPtr = EndRecPtr;
		xlogctl->lastReplayedEndRecPtr = xlogctl->replayEndRecPtr;
		xlogctl->currentChunkStartTime = 0;
		SpinLockRelease(&xlogctl->info_lck);

		/* Also ensure XLogReceiptTime has a sane value */
		XLogReceiptTime = GetCurrentTimestamp();

		/*
		 * Find the first record that logically follows the checkpoint --- it
		 * might physically precede it, though.
		 */
		if (XLByteLT(checkPoint.redo, RecPtr))
		{
			/* back up to find the record */
			record = XLogReadRecord(&(checkPoint.redo), false, PANIC);
		}
		else
		{
			/* just have to read next record after CheckPoint */
			record = XLogReadRecord(NULL, false, LOG);
		}

		/*
		 * In case where its not a clean shutdown but it doesn't have a record
		 * following the checkpoint record, just proceed with the Pass 2, 3, 4
		 * to clear any inconsistent entries in Persistent Tables without
		 * doing the whole redo loop below.
		 */
		if (record == NULL)	
		{
			/*
			 * There are no WAL records following the checkpoint
			 */
			ereport(LOG,
					(errmsg("no record for redo after checkpoint, skip redo and proceed for recovery pass")));
		}

		/*
		 * main redo apply loop, executed if we have record after checkpoint
		 */
		if (record != NULL)
		{
			bool		recoveryContinue = true;
			bool		recoveryApply = true;
			bool		reachedMinRecoveryPoint = false;

			InRedo = true;

			if (minRecoveryPoint.xlogid == 0 && minRecoveryPoint.xrecoff == 0)
				ereport(LOG,
						(errmsg("redo starts at %X/%X",
								ReadRecPtr.xlogid, ReadRecPtr.xrecoff)));
			else
				ereport(LOG,
						(errmsg("redo starts at %X/%X, consistency will be reached at %X/%X",
								ReadRecPtr.xlogid, ReadRecPtr.xrecoff,
						minRecoveryPoint.xlogid, minRecoveryPoint.xrecoff)));

			/*
			 * Let postmaster know we've started redo now, so that it can
			 * launch bgwriter to perform restartpoints.  We don't bother
			 * during crash recovery as restartpoints can only be performed
			 * during archive recovery.  And we'd like to keep crash recovery
			 * simple, to avoid introducing bugs that could you from
			 * recovering after crash.
			 *
			 * After this point, we can no longer assume that we're the only
			 * process in addition to postmaster!  Also, fsync requests are
			 * subsequently to be handled by the bgwriter, not locally.
			 */
			if (InArchiveRecovery && IsUnderPostmaster)
			{
				SetForwardFsyncRequests();
				SendPostmasterSignal(PMSIGNAL_RECOVERY_STARTED);
				bgwriterLaunched = true;
			}

			/*
			 * main redo apply loop
			 */
			do
			{
				HandleStartupProcInterrupts();

				/*
				 * Check if we were requested to re-read config file.
				 */
				if (got_SIGHUP)
				{
					got_SIGHUP = false;
					ProcessConfigFile(PGC_SIGHUP);
				}

				/*
				 * Check if we were requested to exit without finishing
				 * recovery.
				 */
				if (shutdown_requested)
					proc_exit(1);

				/*
				 * Have we passed our safe starting point? If so, we can tell
				 * postmaster that the database is consistent now.
				 */
				if (!reachedMinRecoveryPoint &&
					XLByteLT(minRecoveryPoint, EndRecPtr))
				{
					reachedMinRecoveryPoint = true;
					if (InArchiveRecovery)
					{
						ereport(LOG,
							  (errmsg("consistent recovery state reached")));
						if (IsUnderPostmaster)
							SendPostmasterSignal(PMSIGNAL_RECOVERY_CONSISTENT);
					}
				}

				/*
				 * Have we reached our recovery target?
				 */
				if (recoveryStopsHere(record, &recoveryApply))
				{
					reachedStopPoint = true;	/* see below */
					recoveryContinue = false;
					if (!recoveryApply)
						break;
				}

				/*
				 * See if this record is a checkpoint, if yes then uncover it to
				 * find distributed committed Xacts.
				 * No need to unpack checkpoint in crash recovery mode
				 */
				uint8 xlogRecInfo = record->xl_info & ~XLR_INFO_MASK;

				if (IsStandbyMode() &&
					record->xl_rmid == RM_XLOG_ID &&
					(xlogRecInfo == XLOG_CHECKPOINT_SHUTDOWN
					 || xlogRecInfo == XLOG_CHECKPOINT_ONLINE))
				{
					XLogProcessCheckpointRecord(record, ReadRecPtr);
					memcpy(&checkPoint, XLogRecGetData(record), sizeof(CheckPoint));
				}

				/*
				 * Update shared replayEndRecPtr before replaying this record,
				 * so that XLogFlush will update minRecoveryPoint correctly.
				 */
				SpinLockAcquire(&xlogctl->info_lck);
				xlogctl->replayEndRecPtr = EndRecPtr;
				SpinLockRelease(&xlogctl->info_lck);

				ApplyStartupRedo(&ReadRecPtr, &EndRecPtr, record);

				/*
				 * Update lastReplayedEndRecPtr after this record has been
				 * successfully replayed.
				 */
				SpinLockAcquire(&xlogctl->info_lck);
				xlogctl->lastReplayedEndRecPtr = EndRecPtr;
				SpinLockRelease(&xlogctl->info_lck);

				/*
				 * GPDB_84_MERGE_FIXME: Create restartpoints aggressively.
				 *
				 * In PostgreSQL, the bgwriter creates restartpoints during archive
				 * recovery at its own leisure. In GDPB, with WAL replication based
				 * mirroring, that was tripping the gp_replica_check checks, because
				 * it bypasses the shared buffer cache and reads directly from disk.
				 * For now, restore the old behavior, before the upstream change
				 * to start bgwriter during archive recovery, and create a
				 * restartpoint immediately after replaying a checkpoint record.
				 */
				{
					uint8 xlogRecInfo = record->xl_info & ~XLR_INFO_MASK;

					if (record->xl_rmid == RM_XLOG_ID &&
						(xlogRecInfo == XLOG_CHECKPOINT_SHUTDOWN ||
						 xlogRecInfo == XLOG_CHECKPOINT_ONLINE))
					{
						if (bgwriterLaunched)
							RequestCheckpoint(CHECKPOINT_IMMEDIATE | CHECKPOINT_WAIT);
						else
							CreateRestartPoint(CHECKPOINT_IMMEDIATE);
					}
				}

				LastRec = ReadRecPtr;

				record = XLogReadRecord(NULL, false, LOG);
			} while (record != NULL && recoveryContinue);

			ereport(LOG,
					(errmsg("redo done at %X/%X",
							ReadRecPtr.xlogid, ReadRecPtr.xrecoff)));

			if (recoveryLastXTime)
				ereport(LOG,
					 (errmsg("last completed transaction was at log time %s",
							 timestamptz_to_str(recoveryLastXTime))));
			InRedo = false;
		}
		/*
		 * end of main redo apply loop
		 */
	}

	/*
	 * Kill WAL receiver, if it's still running, before we continue to write
	 * the startup checkpoint record. It will trump over the checkpoint and
	 * subsequent records if it's still alive when we start writing WAL.
	 */
	ShutdownWalRcv();

	/*
	 * We don't need the latch anymore. It's not strictly necessary to disown
	 * it, but let's do it for the sake of tidiness.
	 */
	if (StandbyModeRequested)
		DisownLatch(&XLogCtl->recoveryWakeupLatch);

	/*
	 * We are now done reading the xlog from stream.
	 */
	if (StandbyMode)
	{
		Assert(ControlFile->state == DB_IN_STANDBY_MODE);
		StandbyMode = false;

		/* Transition to promoted mode */
		ControlFile->state = DB_IN_STANDBY_PROMOTED;
		ControlFile->time = (pg_time_t) time(NULL);
		UpdateControlFile();
	}

	/*
	 * Re-fetch the last valid or last applied record, so we can identify the
	 * exact endpoint of what we consider the valid portion of WAL.
	 */
	record = XLogReadRecord(&LastRec, false, PANIC);
	EndOfLog = EndRecPtr;
	XLByteToPrevSeg(EndOfLog, endLogId, endLogSeg);

	elog(LOG,"end of transaction log location is %s",
		 XLogLocationToString(&EndOfLog));

	/*
	 * Complain if we did not roll forward far enough to render the backup
	 * dump consistent.  Note: it is indeed okay to look at the local variable
	 * minRecoveryPoint here, even though ControlFile->minRecoveryPoint might
	 * be further ahead --- ControlFile->minRecoveryPoint cannot have been
	 * advanced beyond the WAL we processed.
	 */
	if (InRecovery &&
		(XLByteLT(EndOfLog, ControlFile->minRecoveryPoint) ||
		 !XLogRecPtrIsInvalid(ControlFile->backupStartPoint)))
	{
		if (reachedStopPoint)
		{
			/* stopped because of stop request */
			ereport(FATAL,
					(errmsg("requested recovery stop point is before consistent recovery point")));
		}

		/*
		 * Ran off end of WAL before reaching end-of-backup WAL record, or
		 * minRecoveryPoint. That's usually a bad sign, indicating that you
		 * tried to recover from an online backup but never called
		 * pg_stop_backup(), or you didn't archive all the WAL up to that
		 * point. However, this also happens in crash recovery, if the system
		 * crashes while an online backup is in progress. We must not treat
		 * that as an error, or the database will refuse to start up.
		 */
		// WALREP_FIXME: But we should probably do this check in standby mode, too
		if (StandbyModeRequested || ControlFile->backupEndRequired)
		{
			if (ControlFile->backupEndRequired)
				ereport(FATAL,
						(errmsg("WAL ends before end of online backup"),
						 errhint("All WAL generated while online backup was taken must be available at recovery.")));
			else if (!XLogRecPtrIsInvalid(ControlFile->backupStartPoint))
				ereport(FATAL,
						(errmsg("WAL ends before end of online backup"),
						 errhint("Online backup should be complete, and all WAL up to that point must be available at recovery.")));
			else
				ereport(FATAL,
					  (errmsg("WAL ends before consistent recovery point")));
		}
	}

	/*
	 * Consider whether we need to assign a new timeline ID.
	 *
	 * If we are doing an archive recovery, we always assign a new ID.	This
	 * handles a couple of issues.	If we stopped short of the end of WAL
	 * during recovery, then we are clearly generating a new timeline and must
	 * assign it a unique new ID.  Even if we ran to the end, modifying the
	 * current last segment is problematic because it may result in trying to
	 * overwrite an already-archived copy of that segment, and we encourage
	 * DBAs to make their archive_commands reject that.  We can dodge the
	 * problem by making the new active segment have a new timeline ID.
	 *
	 * In a normal crash recovery, we can just extend the timeline we were in.
	 *
	 * GPDB: Greenplum doesn't support archive recovery.
	 */
	if (InArchiveRecovery)
	{
		ThisTimeLineID = findNewestTimeLine(recoveryTargetTLI) + 1;
		ereport(LOG,
				(errmsg("selected new timeline ID: %u", ThisTimeLineID)));
		writeTimeLineHistory(ThisTimeLineID, recoveryTargetTLI,
							 curFileTLI, endLogId, endLogSeg);
	}

	/* Save the selected TimeLineID in shared memory, too */
	XLogCtl->ThisTimeLineID = ThisTimeLineID;

	/*
	 * We are now done reading the old WAL.  Turn off archive fetching if it
	 * was active, and make a writable copy of the last WAL segment. (Note
	 * that we also have a copy of the last block of the old WAL in readBuf;
	 * we will use that below.)
	 */
	if (InArchiveRecovery)
		exitArchiveRecovery(curFileTLI, endLogId, endLogSeg);

	/*
	 * Prepare to write WAL starting at EndOfLog position, and init xlog
	 * buffer cache using the block containing the last record from the
	 * previous incarnation.
	 */
	openLogId = endLogId;
	openLogSeg = endLogSeg;
	openLogFile = XLogFileOpen(openLogId, openLogSeg);
	openLogOff = 0;
	Insert = &XLogCtl->Insert;
	Insert->PrevRecord = LastRec;
	XLogCtl->xlblocks[0].xlogid = openLogId;
	XLogCtl->xlblocks[0].xrecoff =
		((EndOfLog.xrecoff - 1) / XLOG_BLCKSZ + 1) * XLOG_BLCKSZ;

	/*
	 * Tricky point here: readBuf contains the *last* block that the LastRec
	 * record spans, not the one it starts in.	The last block is indeed the
	 * one we want to use.
	 */
	Assert(readOff == (XLogCtl->xlblocks[0].xrecoff - XLOG_BLCKSZ) % XLogSegSize);
	memcpy((char *) Insert->currpage, readBuf, XLOG_BLCKSZ);
	Insert->currpos = (char *) Insert->currpage +
		(EndOfLog.xrecoff + XLOG_BLCKSZ - XLogCtl->xlblocks[0].xrecoff);

	LogwrtResult.Write = LogwrtResult.Flush = EndOfLog;

	XLogCtl->Write.LogwrtResult = LogwrtResult;
	Insert->LogwrtResult = LogwrtResult;
	XLogCtl->LogwrtResult = LogwrtResult;

	XLogCtl->LogwrtRqst.Write = EndOfLog;
	XLogCtl->LogwrtRqst.Flush = EndOfLog;

	freespace = INSERT_FREESPACE(Insert);
	if (freespace > 0)
	{
		/* Make sure rest of page is zero */
		MemSet(Insert->currpos, 0, freespace);
		XLogCtl->Write.curridx = 0;
	}
	else
	{
		/*
		 * Whenever Write.LogwrtResult points to exactly the end of a page,
		 * Write.curridx must point to the *next* page (see XLogWrite()).
		 *
		 * Note: it might seem we should do AdvanceXLInsertBuffer() here, but
		 * this is sufficient.	The first actual attempt to insert a log
		 * record will advance the insert state.
		 */
		XLogCtl->Write.curridx = NextBufIdx(0);
	}

	if (InRecovery)
	{
		/*
		 * Close down Recovery for Startup PASS 1.
		 */
		int			rmid;

		/*
		 * Resource managers might need to write WAL records, eg, to record
		 * index cleanup actions.  So temporarily enable XLogInsertAllowed in
		 * this process only.
		 */
		LocalSetXLogInsertAllowed();

		/*
		 * Allow resource managers to do any required cleanup.
		 */
		for (rmid = 0; rmid <= RM_MAX_ID; rmid++)
		{
			if (RmgrTable[rmid].rm_cleanup != NULL)
				RmgrTable[rmid].rm_cleanup();
		}

		/* Disallow XLogInsert again */
		LocalXLogInsertAllowed = -1;

		/*
		 * Check to see if the XLOG sequence contained any unresolved
		 * references to uninitialized pages.
		 */
		XLogCheckInvalidPages();

		/*
		 * Reset pgstat data, because it may be invalid after recovery.
		 */
		pgstat_reset_all();

		/*
		 * Perform a checkpoint to update all our recovery activity to disk.
		 *
		 * Note that we write a shutdown checkpoint rather than an on-line
		 * one. This is not particularly critical, but since we may be
		 * assigning a new TLI, using a shutdown checkpoint allows us to have
		 * the rule that TLI only changes in shutdown checkpoints, which
		 * allows some extra error checking in xlog_redo.
		 */
		if (bgwriterLaunched)
			RequestCheckpoint(CHECKPOINT_END_OF_RECOVERY |
							  CHECKPOINT_IMMEDIATE |
							  CHECKPOINT_WAIT);
		else
			CreateCheckPoint(CHECKPOINT_END_OF_RECOVERY | CHECKPOINT_IMMEDIATE);

		UtilityModeCloseDtmRedoFile();
	}

	/*
	 * Preallocate additional log files, if wanted.
	 */
	PreallocXlogFiles(EndOfLog);

	/*
	 * Okay, we're officially UP.
	 */
	InRecovery = false;

	/* start the archive_timeout timer running */
	XLogCtl->Write.lastSegSwitchTime = (pg_time_t) time(NULL);

	/* initialize shared-memory copy of latest checkpoint XID/epoch */
	XLogCtl->ckptXidEpoch = ControlFile->checkPointCopy.nextXidEpoch;
	XLogCtl->ckptXid = ControlFile->checkPointCopy.nextXid;

	TransactionId oldestActiveXID;

	/* Pre-scan prepared transactions to find out the range of XIDs present */
	oldestActiveXID = PrescanPreparedTransactions();

	elog(LOG, "Oldest active transaction from prepared transactions %u", oldestActiveXID);

	/*
	 * Initialize TransactionXmin to current oldestActiveXID, generally
	 * initialized during GetSnapshotData(). This is to avoid situations where
	 * scanning pg_authid or other tables mostly in BuildFlatFiles() below via
	 * SnapshotNow may try to chase down pg_subtrans for older "sub-committed"
	 * transaction, file corresponding to which may not and is not supposed to
	 * exist. Setting this here will avoid calling SubTransGetParent() in
	 * TransactionIdDidCommit() for older XIDs. Also, set RecentGlobalXmin
	 * since Heap access method functions needs it to have good value as well.
	 */
	TransactionXmin = RecentGlobalXmin = oldestActiveXID;

	/* Start up the commit log and related stuff, too */
	StartupCLOG();
	StartupSUBTRANS(oldestActiveXID);
	StartupMultiXact();
	DistributedLog_Startup(
						oldestActiveXID,
						ShmemVariableCache->nextXid);

	/* also initialize latestCompletedXid, to nextXid - 1 */
	ShmemVariableCache->latestCompletedXid = ShmemVariableCache->nextXid;
	TransactionIdRetreat(ShmemVariableCache->latestCompletedXid);
	elog(LOG, "latest completed transaction id is %u and next transaction id is %u",
		ShmemVariableCache->latestCompletedXid,
		ShmemVariableCache->nextXid);

	/* Reload shared-memory state for prepared transactions */
	RecoverPreparedTransactions();

	/*
	 * Perform a checkpoint to update all our recovery activity to disk.
	 *
	 * Note that we write a shutdown checkpoint rather than an on-line
	 * one. This is not particularly critical, but since we may be
	 * assigning a new TLI, using a shutdown checkpoint allows us to have
	 * the rule that TLI only changes in shutdown checkpoints, which
	 * allows some extra error checking in xlog_redo.
	 *
	 * Note that - Creation of shutdown checkpoint changes the state in pg_control.
	 * If that happens when we are standby who was recently promoted, the
	 * state in pg_control indicating promotion phases (e.g. DB_IN_STANDBY_PROMOTION,
	 * DB_INSTANDBY_NEW_TLI_SET) before the checkpoint creation will get
	 * overwritten posing a problem for further flow. Hence, CreateCheckpoint()
	 * has an ungly hack to avoid this situation and thus we avoid change of
	 * pg_control state just in this special situation. CreateCheckpoint() also
	 * has a comment referring this.
	 */
	if (bgwriterLaunched)
		RequestCheckpoint(CHECKPOINT_END_OF_RECOVERY |
						  CHECKPOINT_IMMEDIATE |
						  CHECKPOINT_WAIT);
	else
		CreateCheckPoint(CHECKPOINT_END_OF_RECOVERY | CHECKPOINT_IMMEDIATE);

#ifdef NOT_USED
	/*
	 * And finally, execute the recovery_end_command, if any.
	 */
	if (recoveryEndCommand)
		ExecuteRecoveryCommand(recoveryEndCommand,
							   "recovery_end_command",
							   true);
#endif

	LWLockAcquire(ControlFileLock, LW_EXCLUSIVE);
	ControlFile->state = DB_IN_PRODUCTION;
	ControlFile->time = (pg_time_t) time(NULL);
	UpdateControlFile();
	ereport(LOG, (errmsg("database system is ready")));
	LWLockRelease(ControlFileLock);

	{
		char version[512];

		strcpy(version, PG_VERSION_STR " compiled on " __DATE__ " " __TIME__);

#ifdef USE_ASSERT_CHECKING
		strcat(version, " (with assert checking)");
#endif
		ereport(LOG,(errmsg("%s", version)));

	}

	/*
	 * All done.  Allow backends to write WAL.	(Although the bool flag is
	 * probably atomic in itself, we use the info_lck here to ensure that
	 * there are no race conditions concerning visibility of other recent
	 * updates to shared memory.)
	 */
	{
		/* use volatile pointer to prevent code rearrangement */
		volatile XLogCtlData *xlogctl = XLogCtl;

		SpinLockAcquire(&xlogctl->info_lck);
		xlogctl->SharedRecoveryInProgress = false;
		SpinLockRelease(&xlogctl->info_lck);
	}

	XLogCloseReadRecord();
}

/*
 * Determine the recovery redo start location from the pg_control file.
 *
 *    1) Only uses information from the pg_control file.
 *    2) This simplified routine does not examine the offline recovery file or
 *       the online backup labels, etc.
 *    3) This routine is a heavily reduced version of StartXLOG.
 *    4) IMPORTANT NOTE: This routine sets global variables that establish
 *       the timeline context necessary to do ReadRecord.  The ThisTimeLineID
 *       and expectedTLIs globals are set.
 *
 */
void
XLogGetRecoveryStart(char *callerStr, char *reasonStr, XLogRecPtr *redoCheckPointLoc, CheckPoint *redoCheckPoint)
{
	CheckPoint	checkPoint;
	XLogRecPtr	checkPointLoc;
	XLogRecord *record;
	bool previous;
	XLogRecPtr checkPointLSN;

	Assert(redoCheckPointLoc != NULL);
	Assert(redoCheckPoint != NULL);

	XLogCloseReadRecord();

	/*
	 * Read control file and verify XLOG status looks valid.
	 *
	 */
	ReadControlFile();

	if (ControlFile->state < DB_SHUTDOWNED ||
		ControlFile->state > DB_IN_PRODUCTION ||
		!XRecOffIsValid(ControlFile->checkPoint.xrecoff))
		ereport(FATAL,
				(errmsg("%s: control file contains invalid data", callerStr)));

	/*
	 * Get the last valid checkpoint record.  If the latest one according
	 * to pg_control is broken, try the next-to-last one.
	 */
	checkPointLoc = ControlFile->checkPoint;
	ThisTimeLineID = ControlFile->checkPointCopy.ThisTimeLineID;

	/*
	 * Check for recovery control file, and if so set up state for offline
	 * recovery
	 */
	XLogReadRecoveryCommandFile(DEBUG5);

	/* Now we can determine the list of expected TLIs */
	expectedTLIs = XLogReadTimeLineHistory(ThisTimeLineID);

	record = ReadCheckpointRecord(checkPointLoc, 1);
	if (record != NULL)
	{
		previous = false;
	}
	else
	{
		previous = true;
		checkPointLoc = ControlFile->prevCheckPoint;
		record = ReadCheckpointRecord(checkPointLoc, 2);
		if (record != NULL)
		{
			ereport(LOG,
					(errmsg("%s: using previous checkpoint record at %s (LSN %s)",
						    callerStr,
							XLogLocationToString(&checkPointLoc),
						    XLogLocationToString2(&EndRecPtr))));
		}
		else
		{
			ereport(ERROR,
				 (errmsg("%s: could not locate a valid checkpoint record", callerStr)));
		}
	}

	memcpy(&checkPoint, XLogRecGetData(record), sizeof(CheckPoint));
	checkPointLSN = EndRecPtr;

	if (XLByteEQ(checkPointLoc,checkPoint.redo))
	{
		{
			elog(LOG,
				 "control file has restart '%s' and redo start checkpoint at location(lsn) '%s(%s)' ",
				 (previous ? "previous " : ""),
				 XLogLocationToString3(&checkPointLoc),
				 XLogLocationToString4(&checkPointLSN));
		}
	}
 	else if (XLByteLT(checkPointLoc, checkPoint.redo))
	{
		ereport(ERROR,
				(errmsg("%s: invalid redo in checkpoint record", callerStr)));
	}
	else
	{
		XLogRecord *record;

		record = XLogReadRecord(&checkPoint.redo, false, LOG);
		if (record == NULL)
		{
			ereport(ERROR,
			 (errmsg("%s: first redo record before checkpoint not found at %s",
					 callerStr, XLogLocationToString(&checkPoint.redo))));
		}

		{
			elog(LOG,
				 "control file has restart '%s' checkpoint at location(lsn) '%s(%s)', redo starts at location(lsn) '%s(%s)' ",
				 (previous ? "previous " : ""),
				 XLogLocationToString3(&checkPointLoc),
				 XLogLocationToString4(&checkPointLSN),
				 XLogLocationToString(&checkPoint.redo),
				 XLogLocationToString2(&EndRecPtr));
		}
	}

	XLogCloseReadRecord();

	*redoCheckPointLoc = checkPointLoc;
	*redoCheckPoint = checkPoint;

}

/*
 * Is the system still in recovery?
 *
 * Unlike testing InRecovery, this works in any process that's connected to
 * shared memory.
 *
 * As a side-effect, we initialize the local TimeLineID and RedoRecPtr
 * variables the first time we see that recovery is finished.
 */
bool
RecoveryInProgress(void)
{
	/*
	 * We check shared state each time only until we leave recovery mode. We
	 * can't re-enter recovery, so there's no need to keep checking after the
	 * shared variable has once been seen false.
	 */
	if (!LocalRecoveryInProgress)
		return false;
	else
	{
		/* use volatile pointer to prevent code rearrangement */
		volatile XLogCtlData *xlogctl = XLogCtl;

		/* spinlock is essential on machines with weak memory ordering! */
		SpinLockAcquire(&xlogctl->info_lck);
		LocalRecoveryInProgress = xlogctl->SharedRecoveryInProgress;
		SpinLockRelease(&xlogctl->info_lck);

		/*
		 * Initialize TimeLineID and RedoRecPtr when we discover that recovery
		 * is finished. InitPostgres() relies upon this behaviour to ensure
		 * that InitXLOGAccess() is called at backend startup.	(If you change
		 * this, see also LocalSetXLogInsertAllowed.)
		 */
		if (!LocalRecoveryInProgress)
			InitXLOGAccess();

		return LocalRecoveryInProgress;
	}

	/*
	 * All done.  Allow backends to write WAL.  (Although the bool flag is
	 * probably atomic in itself, we use the info_lck here to ensure that
	 * there are no race conditions concerning visibility of other recent
	 * updates to shared memory.)
	 */
	{
		/* use volatile pointer to prevent code rearrangement */
		volatile XLogCtlData *xlogctl = XLogCtl;

		SpinLockAcquire(&xlogctl->info_lck);
		xlogctl->SharedRecoveryInProgress = false;
		SpinLockRelease(&xlogctl->info_lck);
	}
}

/*
 * Is this process allowed to insert new WAL records?
 *
 * Ordinarily this is essentially equivalent to !RecoveryInProgress().
 * But we also have provisions for forcing the result "true" or "false"
 * within specific processes regardless of the global state.
 */
bool
XLogInsertAllowed(void)
{
	/*
	 * If value is "unconditionally true" or "unconditionally false",
	 * just return it.  This provides the normal fast path once recovery
	 * is known done.
	 */
	if (LocalXLogInsertAllowed >= 0)
		return (bool) LocalXLogInsertAllowed;

	/*
	 * Else, must check to see if we're still in recovery.
	 */
	if (RecoveryInProgress())
		return false;

	/*
	 * On exit from recovery, reset to "unconditionally true", since there
	 * is no need to keep checking.
	 */
	LocalXLogInsertAllowed = 1;
	return true;
}

/*
 * Make XLogInsertAllowed() return true in the current process only.
 */
static void
LocalSetXLogInsertAllowed(void)
{
	Assert(LocalXLogInsertAllowed == -1);
	LocalXLogInsertAllowed = 1;

	/* Initialize as RecoveryInProgress() would do when switching state */
	InitXLOGAccess();
}

/*
 * Subroutine to try to fetch and validate a prior checkpoint record.
 *
 * whichChkpt identifies the checkpoint (merely for reporting purposes).
 * 1 for "primary", 2 for "secondary", 0 for "other" (backup_label)
 */
static XLogRecord *
ReadCheckpointRecord(XLogRecPtr RecPtr, int whichChkpt)
{
	XLogRecord *record;
	bool sizeOk;
	uint32 delta_xl_tot_len;		/* delta of total len of entire record */
	uint32 delta_xl_len;			/* delta of total len of rmgr data */

	if (!XRecOffIsValid(RecPtr.xrecoff))
	{
		switch (whichChkpt)
		{
			case 1:
				ereport(LOG,
				(errmsg("invalid primary checkpoint link in control file")));
				break;
			case 2:
				ereport(LOG,
						(errmsg("invalid secondary checkpoint link in control file")));
				break;
			default:
				ereport(LOG,
				   (errmsg("invalid checkpoint link in backup_label file")));
				break;
		}
		return NULL;
	}

	/*
	 * Set fetching_ckpt to true here, so that XLogReadRecord()
	 * uses RedoStartLSN as the start replication location used
	 * by WAL receiver (when StandbyMode is on). See comments
	 * for fetching_ckpt in XLogReadPage()
	 */
	record = XLogReadRecord(&RecPtr, true /* fetching_checkpoint */, LOG);

	if (record == NULL)
	{
		switch (whichChkpt)
		{
			case 1:
				ereport(LOG,
						(errmsg("invalid primary checkpoint record at location %s",
						        XLogLocationToString_Long(&RecPtr))));
				break;
			case 2:
				ereport(LOG,
						(errmsg("invalid secondary checkpoint record at location %s",
						        XLogLocationToString_Long(&RecPtr))));
				break;
			default:
				ereport(LOG,
						(errmsg("invalid checkpoint record at location %s",
						        XLogLocationToString_Long(&RecPtr))));
				break;
		}
		return NULL;
	}
	if (record->xl_rmid != RM_XLOG_ID)
	{
		switch (whichChkpt)
		{
			case 1:
				ereport(LOG,
						(errmsg("invalid resource manager ID in primary checkpoint record at location %s",
						        XLogLocationToString_Long(&RecPtr))));
				break;
			case 2:
				ereport(LOG,
						(errmsg("invalid resource manager ID in secondary checkpoint record at location %s",
						        XLogLocationToString_Long(&RecPtr))));
				break;
			default:
				ereport(LOG,
				(errmsg("invalid resource manager ID in checkpoint record at location %s",
				        XLogLocationToString_Long(&RecPtr))));
				break;
		}
		return NULL;
	}
	if (record->xl_info != XLOG_CHECKPOINT_SHUTDOWN &&
		record->xl_info != XLOG_CHECKPOINT_ONLINE)
	{
		switch (whichChkpt)
		{
			case 1:
				ereport(LOG,
				   (errmsg("invalid xl_info in primary checkpoint record at location %s",
				           XLogLocationToString_Long(&RecPtr))));
				break;
			case 2:
				ereport(LOG,
				 (errmsg("invalid xl_info in secondary checkpoint record at location %s",
				         XLogLocationToString_Long(&RecPtr))));
				break;
			default:
				ereport(LOG,
						(errmsg("invalid xl_info in checkpoint record at location %s",
						        XLogLocationToString_Long(&RecPtr))));
				break;
		}
		return NULL;
	}

	sizeOk = false;
	if (record->xl_len == sizeof(CheckPoint) &&
		record->xl_tot_len == SizeOfXLogRecord + sizeof(CheckPoint))
	{
		sizeOk = true;
	}
	else if (record->xl_len > sizeof(CheckPoint) &&
		record->xl_tot_len > SizeOfXLogRecord + sizeof(CheckPoint))
	{
		delta_xl_len = record->xl_len - sizeof(CheckPoint);
		delta_xl_tot_len = record->xl_tot_len - (SizeOfXLogRecord + sizeof(CheckPoint));

		if (delta_xl_len == delta_xl_tot_len)
		{
			sizeOk = true;
		}
	}

	if (!sizeOk)
	{
		switch (whichChkpt)
		{
			case 1:
				ereport(LOG,
					(errmsg("invalid length of primary checkpoint at location %s",
					        XLogLocationToString_Long(&RecPtr))));
				break;
			case 2:
				ereport(LOG,
				  (errmsg("invalid length of secondary checkpoint record at location %s",
				          XLogLocationToString_Long(&RecPtr))));
				break;
			default:
				ereport(LOG,
						(errmsg("invalid length of checkpoint record at location %s",
						        XLogLocationToString_Long(&RecPtr))));
				break;
		}
		return NULL;
	}
	return record;
}

static void
UnpackCheckPointRecord(
	XLogRecord			*record,
	CheckpointExtendedRecord *ckptExtended)
{
	char *current_record_ptr;
	int remainderLen;

	if (record->xl_len == sizeof(CheckPoint))
	{
		/* Special (for bootstrap, xlog switch, maybe others) */
		ckptExtended->dtxCheckpoint = NULL;
		ckptExtended->dtxCheckpointLen = 0;
		ckptExtended->ptas = NULL;
		return;
	}

	/* Normal checkpoint Record */
	Assert(record->xl_len > sizeof(CheckPoint));

	current_record_ptr = ((char*)XLogRecGetData(record)) + sizeof(CheckPoint);
	remainderLen = record->xl_len - sizeof(CheckPoint);

	/* Start of distributed transaction information */
	ckptExtended->dtxCheckpoint = (TMGXACT_CHECKPOINT *)current_record_ptr;
	ckptExtended->dtxCheckpointLen =
		TMGXACT_CHECKPOINT_BYTES((ckptExtended->dtxCheckpoint)->committedCount);

	/*
	 * The master prepared transaction aggregate state (ptas) will be skipped
	 * when gp_before_filespace_setup is ON.
	 */
	if (remainderLen > ckptExtended->dtxCheckpointLen)
	{
		current_record_ptr = current_record_ptr + ckptExtended->dtxCheckpointLen;
		remainderLen -= ckptExtended->dtxCheckpointLen;

		/* Finally, point to prepared transaction information */
		ckptExtended->ptas = (prepared_transaction_agg_state *) current_record_ptr;
		Assert(remainderLen == PREPARED_TRANSACTION_CHECKPOINT_BYTES(ckptExtended->ptas->count));
	}
	else
	{
		Assert(remainderLen == ckptExtended->dtxCheckpointLen);
		ckptExtended->ptas = NULL;
	}
}

/*
 * This must be called during startup of a backend process, except that
 * it need not be called in a standalone backend (which does StartupXLOG
 * instead).  We need to initialize the local copies of ThisTimeLineID and
 * RedoRecPtr.
 *
 * Note: before Postgres 8.0, we went to some effort to keep the postmaster
 * process's copies of ThisTimeLineID and RedoRecPtr valid too.  This was
 * unnecessary however, since the postmaster itself never touches XLOG anyway.
 */
void
InitXLOGAccess(void)
{
	/* ThisTimeLineID doesn't change so we need no lock to copy it */
	ThisTimeLineID = XLogCtl->ThisTimeLineID;
	/* GPDB_84_MERGE_FIXME: Disabled, because FTS process was tripping it.
	 * This assertion was added by the merge, so I suspect it's been wrong
	 * all along, but we haven't noticed. */
#if 0
	Assert(ThisTimeLineID != 0);
#endif

	/* Use GetRedoRecPtr to copy the RedoRecPtr safely */
	(void) GetRedoRecPtr();
}

/*
 * Once spawned, a backend may update its local RedoRecPtr from
 * XLogCtl->Insert.RedoRecPtr; it must hold the insert lock or info_lck
 * to do so.  This is done in XLogInsert() or GetRedoRecPtr().
 */
XLogRecPtr
GetRedoRecPtr(void)
{
	/* use volatile pointer to prevent code rearrangement */
	volatile XLogCtlData *xlogctl = XLogCtl;

	SpinLockAcquire(&xlogctl->info_lck);
	Assert(XLByteLE(RedoRecPtr, xlogctl->Insert.RedoRecPtr));
	RedoRecPtr = xlogctl->Insert.RedoRecPtr;
	SpinLockRelease(&xlogctl->info_lck);

	return RedoRecPtr;
}

/*
 * GetInsertRecPtr -- Returns the current insert position.
 *
 * NOTE: The value *actually* returned is the position of the last full
 * xlog page. It lags behind the real insert position by at most 1 page.
 * For that, we don't need to acquire WALInsertLock which can be quite
 * heavily contended, and an approximation is enough for the current
 * usage of this function.
 */
XLogRecPtr
GetInsertRecPtr(void)
{
	/* use volatile pointer to prevent code rearrangement */
	volatile XLogCtlData *xlogctl = XLogCtl;
	XLogRecPtr	recptr;

	SpinLockAcquire(&xlogctl->info_lck);
	recptr = xlogctl->LogwrtRqst.Write;
	SpinLockRelease(&xlogctl->info_lck);

	return recptr;
}

/*
 * GetFlushRecPtr -- Returns the current flush position, ie, the last WAL
 * position known to be fsync'd to disk.
 */
XLogRecPtr
GetFlushRecPtr(void)
{
	/* use volatile pointer to prevent code rearrangement */
	volatile XLogCtlData *xlogctl = XLogCtl;
	XLogRecPtr	recptr;

	SpinLockAcquire(&xlogctl->info_lck);
	recptr = xlogctl->LogwrtResult.Flush;
	SpinLockRelease(&xlogctl->info_lck);

	return recptr;
}

/*
 * Get the time of the last xlog segment switch
 */
pg_time_t
GetLastSegSwitchTime(void)
{
	pg_time_t	result;

	/* Need WALWriteLock, but shared lock is sufficient */
	LWLockAcquire(WALWriteLock, LW_SHARED);
	result = XLogCtl->Write.lastSegSwitchTime;
	LWLockRelease(WALWriteLock);

	return result;
}

/*
 * GetNextXidAndEpoch - get the current nextXid value and associated epoch
 *
 * This is exported for use by code that would like to have 64-bit XIDs.
 * We don't really support such things, but all XIDs within the system
 * can be presumed "close to" the result, and thus the epoch associated
 * with them can be determined.
 */
void
GetNextXidAndEpoch(TransactionId *xid, uint32 *epoch)
{
	uint32		ckptXidEpoch;
	TransactionId ckptXid;
	TransactionId nextXid;

	/* Must read checkpoint info first, else have race condition */
	{
		/* use volatile pointer to prevent code rearrangement */
		volatile XLogCtlData *xlogctl = XLogCtl;

		SpinLockAcquire(&xlogctl->info_lck);
		ckptXidEpoch = xlogctl->ckptXidEpoch;
		ckptXid = xlogctl->ckptXid;
		SpinLockRelease(&xlogctl->info_lck);
	}

	/* Now fetch current nextXid */
	nextXid = ReadNewTransactionId();

	/*
	 * nextXid is certainly logically later than ckptXid.  So if it's
	 * numerically less, it must have wrapped into the next epoch.
	 */
	if (nextXid < ckptXid)
		ckptXidEpoch++;

	*xid = nextXid;
	*epoch = ckptXidEpoch;
}

/*
 * This must be called ONCE during postmaster or standalone-backend shutdown
 */
void
ShutdownXLOG(int code __attribute__((unused)) , Datum arg __attribute__((unused)) )
{
	ereport(LOG,
			(errmsg("shutting down")));

	if (RecoveryInProgress())
		CreateRestartPoint(CHECKPOINT_IS_SHUTDOWN | CHECKPOINT_IMMEDIATE);
	else
	{
		/*
		 * If archiving is enabled, rotate the last XLOG file so that all the
		 * remaining records are archived (postmaster wakes up the archiver
		 * process one more time at the end of shutdown). The checkpoint
		 * record will go to the next XLOG file and won't be archived (yet).
		 */
		if (XLogArchivingActive() && XLogArchiveCommandSet())
			RequestXLogSwitch();

		CreateCheckPoint(CHECKPOINT_IS_SHUTDOWN | CHECKPOINT_IMMEDIATE);
	}
	ShutdownCLOG();
	ShutdownSUBTRANS();
	ShutdownMultiXact();
	DistributedLog_Shutdown();

	ereport(LOG,
			(errmsg("database system is shut down"),
					errSendAlert(true)));
}

/*
 * Calculate the last segment that we need to retain because of
 * keep_wal_segments, by subtracting keep_wal_segments from the passed
 * xlog location
 */
static void
CheckKeepWalSegments(XLogRecPtr recptr, uint32 *_logId, uint32 *_logSeg)
{
	uint32	log;
	uint32	seg;
	uint32	keep_log;
	uint32	keep_seg;

	if (keep_wal_segments <= 0)
		return;

	XLByteToSeg(recptr, log, seg);

	keep_seg = keep_wal_segments % XLogSegsPerFile;
	keep_log = keep_wal_segments / XLogSegsPerFile;
	ereport(DEBUG1,
			(errmsg("%s: Input %d %d (Keep %d %d) (current %d %d)",
					PG_FUNCNAME_MACRO, *_logId, *_logSeg, keep_log,
					keep_seg, log, seg)));
	if (seg < keep_seg)
	{
		keep_log += 1;
		seg = seg - keep_seg + XLogSegsPerFile;
	}
	else
	{
		seg = seg - keep_seg;
	}

	/* Avoid underflow, don't go below (0,1) */
	if (log < keep_log || (log == keep_log && seg == 0))
	{
		log = 0;
		seg = 1;
	}
	else
	{
		log = log - keep_log;
	}

	/* check not to delete WAL segments newer than the calculated segment */
	if (log < *_logId || (log == *_logId && seg < *_logSeg))
	{
		*_logId = log;
		*_logSeg = seg;
	}

	ereport(DEBUG1,
			(errmsg("%s: Output %d %d",
					PG_FUNCNAME_MACRO, *_logId, *_logSeg)));
}

/*
 * Log start of a checkpoint.
 */
static void
LogCheckpointStart(int flags, bool restartpoint)
{
	const char *msg;

	/*
	 * XXX: This is hopelessly untranslatable. We could call gettext_noop for
	 * the main message, but what about all the flags?
	 */
	if (restartpoint)
		msg = "restartpoint starting:%s%s%s%s%s%s%s";
	else
		msg = "checkpoint starting:%s%s%s%s%s%s%s";

	elog(LOG, msg,
		 (flags & CHECKPOINT_IS_SHUTDOWN) ? " shutdown" : "",
		 (flags & CHECKPOINT_END_OF_RECOVERY) ? " end-of-recovery" : "",
		 (flags & CHECKPOINT_IMMEDIATE) ? " immediate" : "",
		 (flags & CHECKPOINT_FORCE) ? " force" : "",
		 (flags & CHECKPOINT_WAIT) ? " wait" : "",
		 (flags & CHECKPOINT_CAUSE_XLOG) ? " xlog" : "",
		 (flags & CHECKPOINT_CAUSE_TIME) ? " time" : "");
}

/*
 * Log end of a checkpoint.
 */
static void
LogCheckpointEnd(bool restartpoint)
{
	long		write_secs,
				sync_secs,
				total_secs;
	int			write_usecs,
				sync_usecs,
				total_usecs;

	CheckpointStats.ckpt_end_t = GetCurrentTimestamp();

	TimestampDifference(CheckpointStats.ckpt_start_t,
						CheckpointStats.ckpt_end_t,
						&total_secs, &total_usecs);

	TimestampDifference(CheckpointStats.ckpt_write_t,
						CheckpointStats.ckpt_sync_t,
						&write_secs, &write_usecs);

	TimestampDifference(CheckpointStats.ckpt_sync_t,
						CheckpointStats.ckpt_sync_end_t,
						&sync_secs, &sync_usecs);

	if (restartpoint)
		elog(LOG, "restartpoint complete: wrote %d buffers (%.1f%%); "
			 "write=%ld.%03d s, sync=%ld.%03d s, total=%ld.%03d s",
			 CheckpointStats.ckpt_bufs_written,
			 (double) CheckpointStats.ckpt_bufs_written * 100 / NBuffers,
			 write_secs, write_usecs / 1000,
			 sync_secs, sync_usecs / 1000,
			 total_secs, total_usecs / 1000);
	else
		elog(LOG, "checkpoint complete: wrote %d buffers (%.1f%%); "
			 "%d transaction log file(s) added, %d removed, %d recycled; "
			 "write=%ld.%03d s, sync=%ld.%03d s, total=%ld.%03d s",
			 CheckpointStats.ckpt_bufs_written,
			 (double) CheckpointStats.ckpt_bufs_written * 100 / NBuffers,
			 CheckpointStats.ckpt_segs_added,
			 CheckpointStats.ckpt_segs_removed,
			 CheckpointStats.ckpt_segs_recycled,
			 write_secs, write_usecs / 1000,
			 sync_secs, sync_usecs / 1000,
			 total_secs, total_usecs / 1000);
}

/*
 * Perform a checkpoint --- either during shutdown, or on-the-fly
 *
 * flags is a bitwise OR of the following:
 *	CHECKPOINT_IS_SHUTDOWN: checkpoint is for database shutdown.
 *	CHECKPOINT_END_OF_RECOVERY: checkpoint is for end of WAL recovery.
 *	CHECKPOINT_IMMEDIATE: finish the checkpoint ASAP,
 *		ignoring checkpoint_completion_target parameter.
 *	CHECKPOINT_FORCE: force a checkpoint even if no XLOG activity has occured
 *		since the last one (implied by CHECKPOINT_IS_SHUTDOWN or
 *		CHECKPOINT_END_OF_RECOVERY).
 *
 * Note: flags contains other bits, of interest here only for logging purposes.
 * In particular note that this routine is synchronous and does not pay
 * attention to CHECKPOINT_WAIT.
 */
void
CreateCheckPoint(int flags)
{
	bool		shutdown;
	CheckPoint	checkPoint;
	XLogRecPtr	recptr;
	XLogCtlInsert *Insert = &XLogCtl->Insert;
	XLogRecData rdata[6];
	char* 		dtxCheckPointInfo;
	int			dtxCheckPointInfoSize;
	uint32		freespace;
	uint32		_logId;
	uint32		_logSeg;
	VirtualTransactionId *vxids;
	int     	nvxids;

	/*
	 * An end-of-recovery checkpoint is really a shutdown checkpoint, just
	 * issued at a different time.
	 */
	if (flags & (CHECKPOINT_IS_SHUTDOWN | CHECKPOINT_END_OF_RECOVERY))
		shutdown = true;
	else
		shutdown = false;

	if (shutdown && ControlFile->state == DB_STARTUP)
	{
		return;
	}

#ifdef FAULT_INJECTOR
	if (FaultInjector_InjectFaultIfSet(
			Checkpoint,
			DDLNotSpecified,
			"" /* databaseName */,
			"" /* tableName */) == FaultInjectorTypeSkip)
		return;  // skip checkpoint
#endif

	/* sanity check */
	if (RecoveryInProgress() && (flags & CHECKPOINT_END_OF_RECOVERY) == 0)
		elog(ERROR, "can't create a checkpoint during recovery");

	/*
	 * Acquire CheckpointLock to ensure only one checkpoint happens at a time.
	 * (This is just pro forma, since in the present system structure there is
	 * only one process that is allowed to issue checkpoints at any given
	 * time.)
	 */
	LWLockAcquire(CheckpointLock, LW_EXCLUSIVE);

	/*
	 * Prepare to accumulate statistics.
	 *
	 * Note: because it is possible for log_checkpoints to change while a
	 * checkpoint proceeds, we always accumulate stats, even if
	 * log_checkpoints is currently off.
	 */
	MemSet(&CheckpointStats, 0, sizeof(CheckpointStats));
	CheckpointStats.ckpt_start_t = GetCurrentTimestamp();

	/*
	 * Use a critical section to force system panic if we have trouble.
	 */
	START_CRIT_SECTION();

	if (shutdown)
	{
		/*
		 * This is an ugly fix to dis-allow changing the pg_control
		 * state for standby promotion continuity.
		 *
		 * Refer to Startup_InProduction() for more details
		 */
		if (ControlFile->state != DB_IN_STANDBY_PROMOTED)
		{
			LWLockAcquire(ControlFileLock, LW_EXCLUSIVE);
			ControlFile->state = DB_SHUTDOWNING;
			ControlFile->time = (pg_time_t) time(NULL);
			UpdateControlFile();
			LWLockRelease(ControlFileLock);
		}
	}

	/*
	 * Let smgr prepare for checkpoint; this has to happen before we determine
	 * the REDO pointer.  Note that smgr must not do anything that'd have to
	 * be undone if we decide no checkpoint is needed.
	 */
	smgrpreckpt();

	/* Begin filling in the checkpoint WAL record */
	MemSet(&checkPoint, 0, sizeof(checkPoint));
	checkPoint.time = (pg_time_t) time(NULL);

	/*
	 * The WRITE_PERSISTENT_STATE_ORDERED_LOCK gets these locks:
	 *    MirroredLock SHARED, and
	 *    PersistentObjLock EXCLUSIVE.
	 * as well as set MyProc->inCommit = true.
	 *
	 * The READ_PERSISTENT_STATE_ORDERED_LOCK gets this lock:
	 *    PersistentObjLock SHARED.
	 *
	 * They do this to prevent Persistent object changes during checkpoint and
	 * prevent persistent object reads while writing.  And acquire the MirroredLock
	 * at a level that blocks DDL during FileRep statechanges...
	 */

	/*
	 * We must hold WALInsertLock while examining insert state to determine
	 * the checkpoint REDO pointer.
	 */
	LWLockAcquire(WALInsertLock, LW_EXCLUSIVE);

	/*
	 * If this isn't a shutdown or forced checkpoint, and we have not inserted
	 * any XLOG records since the start of the last checkpoint, skip the
	 * checkpoint.	The idea here is to avoid inserting duplicate checkpoints
	 * when the system is idle. That wastes log space, and more importantly it
	 * exposes us to possible loss of both current and previous checkpoint
	 * records if the machine crashes just as we're writing the update.
	 * (Perhaps it'd make even more sense to checkpoint only when the previous
	 * checkpoint record is in a different xlog page?)
	 *
	 * We have to make two tests to determine that nothing has happened since
	 * the start of the last checkpoint: current insertion point must match
	 * the end of the last checkpoint record, and its redo pointer must point
	 * to itself.
	 */
	if ((flags & (CHECKPOINT_IS_SHUTDOWN | CHECKPOINT_END_OF_RECOVERY |
				  CHECKPOINT_FORCE)) == 0)
	{
		XLogRecPtr	curInsert;

		INSERT_RECPTR(curInsert, Insert, Insert->curridx);
#ifdef originalCheckpointChecking
		if (curInsert.xlogid == ControlFile->checkPoint.xlogid &&
			curInsert.xrecoff == ControlFile->checkPoint.xrecoff +
			MAXALIGN(SizeOfXLogRecord + sizeof(CheckPoint)) &&
			ControlFile->checkPoint.xlogid ==
			ControlFile->checkPointCopy.redo.xlogid &&
			ControlFile->checkPoint.xrecoff ==
			ControlFile->checkPointCopy.redo.xrecoff)
#else
		/*
		 * GP: Modified since the checkpoint record is not fixed length
		 * so we keep track of the last checkpoint locations (beginning and
		 * end) and use thoe values for comparison.
		 */
		if (XLogCtl->haveLastCheckpointLoc &&
			XLByteEQ(XLogCtl->lastCheckpointLoc,ControlFile->checkPoint) &&
			XLByteEQ(curInsert,XLogCtl->lastCheckpointEndLoc) &&
			XLByteEQ(ControlFile->checkPoint,ControlFile->checkPointCopy.redo))
#endif
		{
			LWLockRelease(WALInsertLock);
			LWLockRelease(CheckpointLock);

			END_CRIT_SECTION();
			return;
		}
	}

	/*
	 * An end-of-recovery checkpoint is created before anyone is allowed to
	 * write WAL. To allow us to write the checkpoint record, temporarily
	 * enable XLogInsertAllowed.  (This also ensures ThisTimeLineID is
	 * initialized, which we need here and in AdvanceXLInsertBuffer.)
	 */
	if (flags & CHECKPOINT_END_OF_RECOVERY)
		LocalSetXLogInsertAllowed();

	checkPoint.ThisTimeLineID = ThisTimeLineID;

	/*
	 * Compute new REDO record ptr = location of next XLOG record.
	 *
	 * NB: this is NOT necessarily where the checkpoint record itself will be,
	 * since other backends may insert more XLOG records while we're off doing
	 * the buffer flush work.  Those XLOG records are logically after the
	 * checkpoint, even though physically before it.  Got that?
	 */
	freespace = INSERT_FREESPACE(Insert);
	if (freespace < SizeOfXLogRecord)
	{
		(void) AdvanceXLInsertBuffer(false);
		/* OK to ignore update return flag, since we will do flush anyway */
		freespace = INSERT_FREESPACE(Insert);
	}
	INSERT_RECPTR(checkPoint.redo, Insert, Insert->curridx);

	/*
	 * Here we update the shared RedoRecPtr for future XLogInsert calls; this
	 * must be done while holding the insert lock AND the info_lck.
	 *
	 * Note: if we fail to complete the checkpoint, RedoRecPtr will be left
	 * pointing past where it really needs to point.  This is okay; the only
	 * consequence is that XLogInsert might back up whole buffers that it
	 * didn't really need to.  We can't postpone advancing RedoRecPtr because
	 * XLogInserts that happen while we are dumping buffers must assume that
	 * their buffer changes are not included in the checkpoint.
	 */
	{
		/* use volatile pointer to prevent code rearrangement */
		volatile XLogCtlData *xlogctl = XLogCtl;

		SpinLockAcquire(&xlogctl->info_lck);
		RedoRecPtr = xlogctl->Insert.RedoRecPtr = checkPoint.redo;
		SpinLockRelease(&xlogctl->info_lck);
	}

	/*
	 * Now we can release WAL insert lock, allowing other xacts to proceed
	 * while we are flushing disk buffers.
	 */
	LWLockRelease(WALInsertLock);

	/*
	 * If enabled, log checkpoint start.  We postpone this until now so as not
	 * to log anything if we decided to skip the checkpoint.
	 */
	if (log_checkpoints)
		LogCheckpointStart(flags, false);

	TRACE_POSTGRESQL_CHECKPOINT_START(flags);

	/*
	 * Before flushing data, we must wait for any transactions that are
	 * currently in their commit critical sections.  If an xact inserted its
	 * commit record into XLOG just before the REDO point, then a crash
	 * restart from the REDO point would not replay that record, which means
	 * that our flushing had better include the xact's update of pg_clog.  So
	 * we wait till he's out of his commit critical section before proceeding.
	 * See notes in RecordTransactionCommit().
	 *
	 * Because we've already released WALInsertLock, this test is a bit fuzzy:
	 * it is possible that we will wait for xacts we didn't really need to
	 * wait for.  But the delay should be short and it seems better to make
	 * checkpoint take a bit longer than to hold locks longer than necessary.
	 * (In fact, the whole reason we have this issue is that xact.c does
	 * commit record XLOG insertion and clog update as two separate steps
	 * protected by different locks, but again that seems best on grounds of
	 * minimizing lock contention.)
	 *
	 * A transaction that has not yet set inCommit when we look cannot be at
	 * risk, since he's not inserted his commit record yet; and one that's
	 * already cleared it is not at risk either, since he's done fixing clog
	 * and we will correctly flush the update below.  So we cannot miss any
	 * xacts we need to wait for.
	 */
	vxids = GetVirtualXIDsDelayingChkpt(&nvxids);
	if (nvxids > 0)
	{
		do
		{
			/*
			 * GPDB needs to AbsorbFsyncRequests() here to avoid deadlock when
			 * fsync request queue is full while backend is in commit and
			 * performing ForgetRelationFsyncRequests() or
			 * ForgetDatabaseFsyncRequests(). Since for GPDB the mdlink
			 * happens through persistent tables cleanup, during which
			 * inCommit flag is set to avoid checkpoint from happening.
			 * PostgreSQL doesn't need this as ForgetRelationFsyncRequests()
			 * or ForgetDatabaseFsyncRequests() are not under inCommit=true.
			 */
			AbsorbFsyncRequests();
			pg_usleep(10000L);	/* wait for 10 msec */
		} while (HaveVirtualXIDsDelayingChkpt(vxids, nvxids));
	}
	pfree(vxids);

	/*
	 * Get the other info we need for the checkpoint record.
	 */
	LWLockAcquire(XidGenLock, LW_SHARED);
	checkPoint.nextXid = ShmemVariableCache->nextXid;
	LWLockRelease(XidGenLock);

	/* Increase XID epoch if we've wrapped around since last checkpoint */
	checkPoint.nextXidEpoch = ControlFile->checkPointCopy.nextXidEpoch;
	if (checkPoint.nextXid < ControlFile->checkPointCopy.nextXid)
		checkPoint.nextXidEpoch++;

	LWLockAcquire(OidGenLock, LW_SHARED);
	checkPoint.nextOid = ShmemVariableCache->nextOid;
	if (!shutdown)
		checkPoint.nextOid += ShmemVariableCache->oidCount;
	LWLockRelease(OidGenLock);

	LWLockAcquire(RelfilenodeGenLock, LW_SHARED);
	checkPoint.nextRelfilenode = ShmemVariableCache->nextRelfilenode;
	if (!shutdown)
		checkPoint.nextRelfilenode += ShmemVariableCache->relfilenodeCount;
	LWLockRelease(RelfilenodeGenLock);

	MultiXactGetCheckptMulti(shutdown,
							 &checkPoint.nextMulti,
							 &checkPoint.nextMultiOffset);

	/*
	 * Having constructed the checkpoint record, ensure all shmem disk buffers
	 * and commit-log buffers are flushed to disk.
	 *
	 * This I/O could fail for various reasons.  If so, we will fail to
	 * complete the checkpoint, but there is no reason to force a system
	 * panic. Accordingly, exit critical section while doing it.
	 */
	END_CRIT_SECTION();

	CheckPointGuts(checkPoint.redo, flags);

	START_CRIT_SECTION();

	/*
	 * Now insert the checkpoint record into XLOG.
	 *
	 * Here is the locking order and scope:
	 *
	 * getDtxCheckPointInfoAndLock (i.e. shmControlLock)
	 * 	READ_PERSISTENT_STATE_ORDERED_LOCK (i.e. PersistentObjLock)
	 * 		mmxlog_append_checkpoint_data
	 * 		XLogInsert
	 * 	READ_PERSISTENT_STATE_ORDERED_UNLOCK
	 * freeDtxCheckPointInfoAndUnlock
	 * XLogFlush
	 *
	 * We get the PersistentObjLock to prevent Persistent Object writers as
	 * we collect the Master Mirroring information from mmxlog_append_checkpoint_data()
	 * until finally after the checkpoint record is inserted into the XLOG to prevent the
	 * persistent information from changing.
	 *
	 * For example, if we don't hold the PersistentObjLock across mmxlog_append_checkpoint_data()
	 * and XLogInsert(), another xlog activity like drop tablespace could happen in between, which
	 * might caused wrong behavior when master standby replay checkpoint record.
	 *
	 * Master standby replay (mmxlog_read_checkpoint_data) the mmxlog information stored in the checkpoint
	 * record to recreate those persistent objects like filespace, tablespace, database dir, etc. If those
	 * objects dropped after checkpoint collected persistent objects information, but before checkpoint
	 * record write to XLOG, then the standby replay would first drop the object based on mmxlog record,
	 * then recreated based on the checkpoint record. That will ends-up left behind the directories already
	 * dropped on the master, break the consistency between the master and the standby.
	 */

	getDtxCheckPointInfoAndLock(&dtxCheckPointInfo, &dtxCheckPointInfoSize);

	rdata[0].data = (char *) (&checkPoint);
	rdata[0].len = sizeof(checkPoint);
	rdata[0].buffer = InvalidBuffer;
	rdata[0].next = &(rdata[1]);

	rdata[1].data = (char *) dtxCheckPointInfo;
	rdata[1].len = dtxCheckPointInfoSize;
	rdata[1].buffer = InvalidBuffer;
	rdata[1].next = NULL;

	prepared_transaction_agg_state *p = NULL;

	getTwoPhasePreparedTransactionData(&p, "CreateCheckPoint");
	rdata[5].data = (char*)p;
	rdata[5].buffer = InvalidBuffer;
	rdata[5].len = PREPARED_TRANSACTION_CHECKPOINT_BYTES(p->count);
	rdata[4].next = &(rdata[5]);
	rdata[5].next = NULL;

	/*
	 * Need to save the oldest prepared transaction XLogRecPtr for use later.
	 * It is not sufficient to just save the pointer because we may remove the
	 * space after it is written in XLogInsert.
	 */
	XLogRecPtr *ptrd_oldest_ptr = NULL;
	XLogRecPtr ptrd_oldest;

	memset(&ptrd_oldest, 0, sizeof(ptrd_oldest));

	ptrd_oldest_ptr = getTwoPhaseOldestPreparedTransactionXLogRecPtr(&rdata[5]);

	if (ptrd_oldest_ptr != NULL)
		memcpy(&ptrd_oldest, ptrd_oldest_ptr, sizeof(ptrd_oldest));

	recptr = XLogInsert(RM_XLOG_ID,
			            shutdown ? XLOG_CHECKPOINT_SHUTDOWN : XLOG_CHECKPOINT_ONLINE,
			            rdata);

	freeDtxCheckPointInfoAndUnlock(dtxCheckPointInfo, dtxCheckPointInfoSize, &recptr);

	XLogFlush(recptr);

	/*
	 * We mustn't write any new WAL after a shutdown checkpoint, or it will
	 * be overwritten at next startup.  No-one should even try, this just
	 * allows sanity-checking.  In the case of an end-of-recovery checkpoint,
	 * we want to just temporarily disable writing until the system has exited
	 * recovery.
	 */
	if (shutdown)
	{
		if (flags & CHECKPOINT_END_OF_RECOVERY)
			LocalXLogInsertAllowed = -1;	/* return to "check" state */
		else
			LocalXLogInsertAllowed = 0;		/* never again write WAL */
	}

	/*
	 * We now have ProcLastRecPtr = start of actual checkpoint record, recptr
	 * = end of actual checkpoint record.
	 */
	if (shutdown && !XLByteEQ(checkPoint.redo, ProcLastRecPtr))
		ereport(PANIC,
				(errmsg("concurrent transaction log activity while database system is shutting down")));

	/*
	 * Select point at which we can truncate the log, which we base on the
	 * prior checkpoint's earliest info or the oldest prepared transaction xlog record's info.
	 */
	if (ptrd_oldest_ptr != NULL && XLByteLE(ptrd_oldest, ControlFile->checkPointCopy.redo))
		XLByteToSeg(ptrd_oldest, _logId, _logSeg);
	else
		XLByteToSeg(ControlFile->checkPointCopy.redo, _logId, _logSeg);

	/*
	 * Update the control file.
	 */
	LWLockAcquire(ControlFileLock, LW_EXCLUSIVE);
	if (shutdown)
	{
		/*
		 * Ugly fix to dis-allow changing pg_control state
		 * for standby promotion continuity
		 */
		if (ControlFile->state != DB_IN_STANDBY_PROMOTED)
			ControlFile->state = DB_SHUTDOWNED;
	}

	ControlFile->prevCheckPoint = ControlFile->checkPoint;
	ControlFile->checkPoint = ProcLastRecPtr;
	ControlFile->checkPointCopy = checkPoint;
	/* crash recovery should always recover to the end of WAL */
	MemSet(&ControlFile->minRecoveryPoint, 0, sizeof(XLogRecPtr));
	ControlFile->time = (pg_time_t) time(NULL);

	/*
	 * Save the last checkpoint position.
	 */
	XLogCtl->haveLastCheckpointLoc = true;
	XLogCtl->lastCheckpointLoc = ProcLastRecPtr;
	XLogCtl->lastCheckpointEndLoc = XactLastRecEnd;

	UpdateControlFile();
	LWLockRelease(ControlFileLock);

	/* Update shared-memory copy of checkpoint XID/epoch */
	{
		/* use volatile pointer to prevent code rearrangement */
		volatile XLogCtlData *xlogctl = XLogCtl;

		SpinLockAcquire(&xlogctl->info_lck);
		xlogctl->ckptXidEpoch = checkPoint.nextXidEpoch;
		xlogctl->ckptXid = checkPoint.nextXid;
		SpinLockRelease(&xlogctl->info_lck);
	}

	/*
	 * We are now done with critical updates; no need for system panic if we
	 * have trouble while fooling with old log segments.
	 */
	END_CRIT_SECTION();

	/*
	 * Let smgr do post-checkpoint cleanup (eg, deleting old files).
	 */
	smgrpostckpt();

	/*
	 * Delete old log files (those no longer needed even for previous
	 * checkpoint).
	 */
	if (gp_keep_all_xlog == false && (_logId || _logSeg))
	{
		GetXLogCleanUpTo(recptr, &_logId, &_logSeg);

		PrevLogSeg(_logId, _logSeg);
		RemoveOldXlogFiles(_logId, _logSeg, recptr);
	}

	/*
	 * Make more log segments if needed.  (Do this after deleting offline log
	 * segments, to avoid having peak disk space usage higher than necessary.)
	 */
	if (!shutdown)
		PreallocXlogFiles(recptr);

	/*
	 * Truncate pg_subtrans if possible.  We can throw away all data before
	 * the oldest XMIN of any running transaction.	No future transaction will
	 * attempt to reference any pg_subtrans entry older than that (see Asserts
	 * in subtrans.c).	During recovery, though, we mustn't do this because
	 * StartupSUBTRANS hasn't been called yet.
	 */
	if (!RecoveryInProgress())
		TruncateSUBTRANS(GetOldestXmin(true, false));

	/* All real work is done, but log before releasing lock. */
	if (log_checkpoints)
		LogCheckpointEnd(false);

	TRACE_POSTGRESQL_CHECKPOINT_DONE(CheckpointStats.ckpt_bufs_written,
									 NBuffers,
									 CheckpointStats.ckpt_segs_added,
									 CheckpointStats.ckpt_segs_removed,
									 CheckpointStats.ckpt_segs_recycled);

	LWLockRelease(CheckpointLock);
}

/*
 * Flush all data in shared memory to disk, and fsync
 *
 * This is the common code shared between regular checkpoints and
 * recovery restartpoints.
 */
static void
CheckPointGuts(XLogRecPtr checkPointRedo, int flags)
{
	CheckPointCLOG();
	CheckPointSUBTRANS();
	CheckPointMultiXact();
	DistributedLog_CheckPoint();
	CheckPointBuffers(flags);	/* performs all required fsyncs */
	/* We deliberately delay 2PC checkpointing as long as possible */
	CheckPointTwoPhase(checkPointRedo);
}

/*
 * Save a checkpoint for recovery restart if appropriate
 *
 * This function is called each time a checkpoint record is read from XLOG.
 * It must determine whether the checkpoint represents a safe restartpoint or
 * not.  If so, the checkpoint record is stashed in shared memory so that
 * CreateRestartPoint can consult it.  (Note that the latter function is
 * executed by the bgwriter, while this one will be executed by the startup
 * process.)
 */
static void
RecoveryRestartPoint(const CheckPoint *checkPoint)
{
	int			rmid;

	/* use volatile pointer to prevent code rearrangement */
	volatile XLogCtlData *xlogctl = XLogCtl;

	/*
	 * Is it safe to checkpoint?  We must ask each of the resource managers
	 * whether they have any partial state information that might prevent a
	 * correct restart from this point.  If so, we skip this opportunity, but
	 * return at the next checkpoint record for another try.
	 */
	for (rmid = 0; rmid <= RM_MAX_ID; rmid++)
	{
		if (RmgrTable[rmid].rm_safe_restartpoint != NULL)
			if (!(RmgrTable[rmid].rm_safe_restartpoint()))
			{
				elog(DEBUG2, "RM %d not safe to record restart point at %X/%X",
					 rmid,
					 checkPoint->redo.xlogid,
					 checkPoint->redo.xrecoff);
				return;
			}
	}

	/* Update the shared RedoRecPtr */
	 SpinLockAcquire(&xlogctl->info_lck);
	 xlogctl->Insert.RedoRecPtr = checkPoint->redo;
	 SpinLockRelease(&xlogctl->info_lck);

	/*
	 * Copy the checkpoint record to shared memory, so that bgwriter can use
	 * it the next time it wants to perform a restartpoint.
	 */
	SpinLockAcquire(&xlogctl->info_lck);
	XLogCtl->lastCheckPointRecPtr = ReadRecPtr;
	memcpy(&XLogCtl->lastCheckPoint, checkPoint, sizeof(CheckPoint));
	SpinLockRelease(&xlogctl->info_lck);
}

/*
 * Establish a restartpoint if possible.
 *
 * This is similar to CreateCheckPoint, but is used during WAL recovery
 * to establish a point from which recovery can roll forward without
 * replaying the entire recovery log.
 *
 * Returns true if a new restartpoint was established. We can only establish
 * a restartpoint if we have replayed a safe checkpoint record since last
 * restartpoint.
 */
bool
CreateRestartPoint(int flags)
{
	XLogRecPtr	lastCheckPointRecPtr;
	CheckPoint	lastCheckPoint;
	uint32		_logId = 0;
	uint32		_logSeg = 0;

	/* use volatile pointer to prevent code rearrangement */
	volatile XLogCtlData *xlogctl = XLogCtl;

	/*
	 * Acquire CheckpointLock to ensure only one restartpoint or checkpoint
	 * happens at a time.
	 */
	LWLockAcquire(CheckpointLock, LW_EXCLUSIVE);

	/* Get a local copy of the last safe checkpoint record. */
	SpinLockAcquire(&xlogctl->info_lck);
	lastCheckPointRecPtr = xlogctl->lastCheckPointRecPtr;
	memcpy(&lastCheckPoint, &XLogCtl->lastCheckPoint, sizeof(CheckPoint));
	SpinLockRelease(&xlogctl->info_lck);

	if (IsStandbyMode())
	{
		/*
		 * Select point at which we can truncate the log, which we base on the
		 * prior checkpoint's earliest info.
		*/
		XLByteToSeg(ControlFile->checkPointCopy.redo, _logId, _logSeg);
	}

	/*
	 * Check that we're still in recovery mode. It's ok if we exit recovery
	 * mode after this check, the restart point is valid anyway.
	 */
	if (!RecoveryInProgress())
	{
		ereport(DEBUG2,
			  (errmsg("skipping restartpoint, recovery has already ended")));
		LWLockRelease(CheckpointLock);
		return false;
	}

	/*
	 * If the last checkpoint record we've replayed is already our last
	 * restartpoint, we can't perform a new restart point. We still update
	 * minRecoveryPoint in that case, so that if this is a shutdown restart
	 * point, we won't start up earlier than before. That's not strictly
	 * necessary, but when we get hot standby capability, it would be rather
	 * weird if the database opened up for read-only connections at a
	 * point-in-time before the last shutdown. Such time travel is still
	 * possible in case of immediate shutdown, though.
	 *
	 * We don't explicitly advance minRecoveryPoint when we do create a
	 * restartpoint. It's assumed that flushing the buffers will do that as a
	 * side-effect.
	 */
	if (XLogRecPtrIsInvalid(lastCheckPointRecPtr) ||
		XLByteLE(lastCheckPoint.redo, ControlFile->checkPointCopy.redo))
	{
		XLogRecPtr	InvalidXLogRecPtr = {0, 0};

		ereport(DEBUG2,
				(errmsg("skipping restartpoint, already performed at %X/%X",
				  lastCheckPoint.redo.xlogid, lastCheckPoint.redo.xrecoff)));

		UpdateMinRecoveryPoint(InvalidXLogRecPtr, true);
		LWLockRelease(CheckpointLock);
		return false;
	}

	if (log_checkpoints)
	{
		/*
		 * Prepare to accumulate statistics.
		 */
		MemSet(&CheckpointStats, 0, sizeof(CheckpointStats));
		CheckpointStats.ckpt_start_t = GetCurrentTimestamp();

		LogCheckpointStart(flags, true);
	}

	CheckPointGuts(lastCheckPoint.redo, flags);

	/*
	 * Update pg_control, using current time.  Check that it still shows
	 * IN_ARCHIVE_RECOVERY state and an older checkpoint, else do nothing;
	 * this is a quick hack to make sure nothing really bad happens if
	 * somehow we get here after the end-of-recovery checkpoint.
	 *
	 * GPDB allows replay to also change the control file during
	 * DB_IN_STANDBY_MODE so that mirror can be restarted from the latest
	 * checkpoint location. This will save the recovery time of mirror, and also
	 * allow mirror to remove already replayed xlogs.
	 *
	 * FIXME: need to consider consolidating the DB_IN_ARCHIVE_RECOVERY (upstream)
	 * and DB_IN_STANDBY_MODE (GPDB only)
	 */
	LWLockAcquire(ControlFileLock, LW_EXCLUSIVE);
	if ((ControlFile->state == DB_IN_ARCHIVE_RECOVERY
		     || ControlFile->state == DB_IN_STANDBY_MODE) &&
		XLByteLT(ControlFile->checkPointCopy.redo, lastCheckPoint.redo))
	{
		ControlFile->prevCheckPoint = ControlFile->checkPoint;
		ControlFile->checkPoint = lastCheckPointRecPtr;
		ControlFile->checkPointCopy = lastCheckPoint;
		ControlFile->time = (pg_time_t) time(NULL);
		UpdateControlFile();
	}
	LWLockRelease(ControlFileLock);

	/*
	 * Currently, there is no need to truncate pg_subtrans during recovery. If
	 * we did do that, we will need to have called StartupSUBTRANS() already
	 * and then TruncateSUBTRANS() would go here.
	 */

	/* All real work is done, but log before releasing lock. */
	if (log_checkpoints)
		LogCheckpointEnd(true);

	ereport((log_checkpoints ? LOG : DEBUG2),
			(errmsg("recovery restart point at %X/%X",
				  lastCheckPoint.redo.xlogid, lastCheckPoint.redo.xrecoff)));

	/* XXX this is currently BROKEN because we are in the wrong process */
	if (recoveryLastXTime)
		ereport((log_checkpoints ? LOG : DEBUG2),
				(errmsg("last completed transaction was at log time %s",
						timestamptz_to_str(recoveryLastXTime))));

	elog((Debug_print_qd_mirroring ? LOG : DEBUG1), "RecoveryRestartPoint: checkpoint copy redo location %s, previous checkpoint location %s",
		 XLogLocationToString(&ControlFile->checkPointCopy.redo),
		 XLogLocationToString2(&ControlFile->prevCheckPoint));

	if (IsStandbyMode())
	{
		/*
		 * Delete offline log files (those no longer needed even for previous
		 * checkpoint).
		 */
		if (gp_keep_all_xlog == false && (_logId || _logSeg))
		{
			XLogRecPtr endptr;

			/* Get the current (or recent) end of xlog */
			endptr = GetStandbyFlushRecPtr(NULL);

			PrevLogSeg(_logId, _logSeg);
			RemoveOldXlogFiles(_logId, _logSeg, endptr);
		}
	}

	LWLockRelease(CheckpointLock);
	return true;
}

/*
 * Write a NEXTOID log record
 */
void
XLogPutNextOid(Oid nextOid)
{
	XLogRecData rdata;

	rdata.data = (char *) (&nextOid);
	rdata.len = sizeof(Oid);
	rdata.buffer = InvalidBuffer;
	rdata.next = NULL;
	(void) XLogInsert(RM_XLOG_ID, XLOG_NEXTOID, &rdata);

	/*
	 * We need not flush the NEXTOID record immediately, because any of the
	 * just-allocated OIDs could only reach disk as part of a tuple insert or
	 * update that would have its own XLOG record that must follow the NEXTOID
	 * record.	Therefore, the standard buffer LSN interlock applied to those
	 * records will ensure no such OID reaches disk before the NEXTOID record
	 * does.
	 *
	 * Note, however, that the above statement only covers state "within" the
	 * database.  When we use a generated OID as a file or directory name, we
	 * are in a sense violating the basic WAL rule, because that filesystem
	 * change may reach disk before the NEXTOID WAL record does.  The impact
	 * of this is that if a database crash occurs immediately afterward, we
	 * might after restart re-generate the same OID and find that it conflicts
	 * with the leftover file or directory.  But since for safety's sake we
	 * always loop until finding a nonconflicting filename, this poses no real
	 * problem in practice. See pgsql-hackers discussion 27-Sep-2006.
	 */
}

/*
 * Write a NEXTRELFILENODE log record similar to XLogPutNextOid
 */
void
XLogPutNextRelfilenode(Oid nextRelfilenode)
{
	XLogRecData rdata;

	rdata.data = (char *) (&nextRelfilenode);
	rdata.len = sizeof(Oid);
	rdata.buffer = InvalidBuffer;
	rdata.next = NULL;
	(void) XLogInsert(RM_XLOG_ID, XLOG_NEXTRELFILENODE, &rdata);
}

/*
 * Write an XLOG SWITCH record.
 *
 * Here we just blindly issue an XLogInsert request for the record.
 * All the magic happens inside XLogInsert.
 *
 * The return value is either the end+1 address of the switch record,
 * or the end+1 address of the prior segment if we did not need to
 * write a switch record because we are already at segment start.
 */
XLogRecPtr
RequestXLogSwitch(void)
{
	XLogRecPtr	RecPtr;
	XLogRecData rdata;

	/* XLOG SWITCH, alone among xlog record types, has no data */
	rdata.buffer = InvalidBuffer;
	rdata.data = NULL;
	rdata.len = 0;
	rdata.next = NULL;

	RecPtr = XLogInsert(RM_XLOG_ID, XLOG_SWITCH, &rdata);

	return RecPtr;
}

/*
 * Write a backup block if needed when we are setting a hint. Note that
 * this may be called for a variety of page types, not just heaps.
 *
 * Callable while holding just share lock on the buffer content.
 *
 * We can't use the plain backup block mechanism since that relies on the
 * Buffer being exclusively locked. Since some modifications (setting LSN, hint
 * bits) are allowed in a sharelocked buffer that can lead to wal checksum
 * failures. So instead we copy the page and insert the copied data as normal
 * record data.
 *
 * We only need to do something if page has not yet been full page written in
 * this checkpoint round. The LSN of the inserted wal record is returned if we
 * had to write, InvalidXLogRecPtr otherwise.
 *
 * It is possible that multiple concurrent backends could attempt to write WAL
 * records. In that case, multiple copies of the same block would be recorded
 * in separate WAL records by different backends, though that is still OK from
 * a correctness perspective.
 *
 * Note that this only works for buffers that fit the standard page model,
 * i.e. those for which buffer_std == true
 */
XLogRecPtr
XLogSaveBufferForHint(Buffer buffer, Relation relation)
{
	XLogRecPtr recptr = InvalidXLogRecPtr;
	XLogRecPtr lsn;
	XLogRecData rdata[2];
	BkpBlock	bkpb;

	/*
	 * Ensure no checkpoint can change our view of RedoRecPtr.
	 */
	Assert(MyProc->inCommit);

	/*
	 * Update RedoRecPtr so XLogCheckBuffer can make the right decision
	 */
	GetRedoRecPtr();

	/*
	 * Setup phony rdata element for use within XLogCheckBuffer only.
	 * We reuse and reset rdata for any actual WAL record insert.
	 */
	rdata[0].buffer = buffer;
	rdata[0].buffer_std = true;

	/*
	 * Check buffer while not holding an exclusive lock.
	 */
	if (XLogCheckBuffer(rdata, false, false, &lsn, &bkpb))
	{
		char copied_buffer[BLCKSZ];
		char *origdata = (char *) BufferGetBlock(buffer);

		/*
		 * Copy buffer so we don't have to worry about concurrent hint bit or
		 * lsn updates. We assume pd_lower/upper cannot be changed without an
		 * exclusive lock, so the contents bkp are not racy.
		 */
		memcpy(copied_buffer, origdata, bkpb.hole_offset);
		memcpy(copied_buffer + bkpb.hole_offset,
			   origdata + bkpb.hole_offset + bkpb.hole_length,
			   BLCKSZ - bkpb.hole_offset - bkpb.hole_length);

		/*
		 * Header for backup block.
		 */
		rdata[0].data = (char *) &bkpb;
		rdata[0].len = sizeof(BkpBlock);
		rdata[0].buffer = InvalidBuffer;
		rdata[0].next = &(rdata[1]);

		/*
		 * Save copy of the buffer.
		 */
		rdata[1].data = copied_buffer;
		rdata[1].len = BLCKSZ - bkpb.hole_length;
		rdata[1].buffer = InvalidBuffer;
		rdata[1].next = NULL;

		recptr = XLogInsert(RM_XLOG_ID, XLOG_HINT, rdata);
	}

	return recptr;
}

/*
 * XLOG resource manager's routines
 *
 * Definitions of info values are in include/catalog/pg_control.h, though
 * not all record types are related to control file updates.
 */
void
xlog_redo(XLogRecPtr beginLoc __attribute__((unused)), XLogRecPtr lsn __attribute__((unused)), XLogRecord *record)
{
	uint8		info = record->xl_info & ~XLR_INFO_MASK;

	/* Backup blocks are not used in xlog records */
	Assert(!(record->xl_info & XLR_BKP_BLOCK_MASK));

	if (info == XLOG_NEXTOID)
	{
		Oid			nextOid;

		/*
		 * We used to try to take the maximum of ShmemVariableCache->nextOid
		 * and the recorded nextOid, but that fails if the OID counter wraps
		 * around.  Since no OID allocation should be happening during replay
		 * anyway, better to just believe the record exactly.
		 */
		memcpy(&nextOid, XLogRecGetData(record), sizeof(Oid));
		ShmemVariableCache->nextOid = nextOid;
		ShmemVariableCache->oidCount = 0;
	}
	if (info == XLOG_NEXTRELFILENODE)
	{
		Oid			nextRelfilenode;

		memcpy(&nextRelfilenode, XLogRecGetData(record), sizeof(Oid));
		ShmemVariableCache->nextRelfilenode = nextRelfilenode;
		ShmemVariableCache->relfilenodeCount = 0;
	}
	else if (info == XLOG_CHECKPOINT_SHUTDOWN)
	{
		CheckPoint	checkPoint;

		memcpy(&checkPoint, XLogRecGetData(record), sizeof(CheckPoint));
		/* In a SHUTDOWN checkpoint, believe the counters exactly */
		ShmemVariableCache->nextXid = checkPoint.nextXid;
		ShmemVariableCache->nextOid = checkPoint.nextOid;
		ShmemVariableCache->oidCount = 0;
		ShmemVariableCache->nextRelfilenode = checkPoint.nextRelfilenode;
		ShmemVariableCache->relfilenodeCount = 0;
		MultiXactSetNextMXact(checkPoint.nextMulti,
							  checkPoint.nextMultiOffset);

		/*
		 * If we see a shutdown checkpoint while waiting for an end-of-backup
		 * record, the backup was canceled and the end-of-backup record will
		 * never arrive.
		 */
		if (StandbyMode &&
			!XLogRecPtrIsInvalid(ControlFile->backupStartPoint))
			ereport(PANIC,
			(errmsg("online backup was canceled, recovery cannot continue")));

		/* ControlFile->checkPointCopy always tracks the latest ckpt XID */
		ControlFile->checkPointCopy.nextXidEpoch = checkPoint.nextXidEpoch;
		ControlFile->checkPointCopy.nextXid = checkPoint.nextXid;

		/* Update shared-memory copy of checkpoint XID/epoch */
		 {
			 /* use volatile pointer to prevent code rearrangement */
			 volatile XLogCtlData *xlogctl = XLogCtl;

			 SpinLockAcquire(&xlogctl->info_lck);
			 xlogctl->ckptXidEpoch = checkPoint.nextXidEpoch;
			 xlogctl->ckptXid = checkPoint.nextXid;
			 SpinLockRelease(&xlogctl->info_lck);
		 }

		/*
		 * TLI may change in a shutdown checkpoint, but it shouldn't decrease
		 */
		if (checkPoint.ThisTimeLineID != ThisTimeLineID)
		{
			if (checkPoint.ThisTimeLineID < ThisTimeLineID ||
				!list_member_int(expectedTLIs,
								 (int) checkPoint.ThisTimeLineID))
				ereport(PANIC,
						(errmsg("unexpected timeline ID %u (after %u) in checkpoint record",
								checkPoint.ThisTimeLineID, ThisTimeLineID)));
			/* Following WAL records should be run with new TLI */
			ThisTimeLineID = checkPoint.ThisTimeLineID;
		}

		RecoveryRestartPoint(&checkPoint);
	}
	else if (info == XLOG_CHECKPOINT_ONLINE)
	{
		CheckPoint	checkPoint;

		memcpy(&checkPoint, XLogRecGetData(record), sizeof(CheckPoint));
		/* In an ONLINE checkpoint, treat the XID counter as a minimum */
		if (TransactionIdPrecedes(ShmemVariableCache->nextXid,
								  checkPoint.nextXid))
			ShmemVariableCache->nextXid = checkPoint.nextXid;
		/* ... but still treat OID counter as exact */
		ShmemVariableCache->nextOid = checkPoint.nextOid;
		ShmemVariableCache->oidCount = 0;
		ShmemVariableCache->nextRelfilenode = checkPoint.nextRelfilenode;
		ShmemVariableCache->relfilenodeCount = 0;
		MultiXactAdvanceNextMXact(checkPoint.nextMulti,
								  checkPoint.nextMultiOffset);

		/* ControlFile->checkPointCopy always tracks the latest ckpt XID */
		ControlFile->checkPointCopy.nextXidEpoch = checkPoint.nextXidEpoch;
		ControlFile->checkPointCopy.nextXid = checkPoint.nextXid;

		/* Update shared-memory copy of checkpoint XID/epoch */
		 {
			 /* use volatile pointer to prevent code rearrangement */
			 volatile XLogCtlData *xlogctl = XLogCtl;

			 SpinLockAcquire(&xlogctl->info_lck);
			 xlogctl->ckptXidEpoch = checkPoint.nextXidEpoch;
			 xlogctl->ckptXid = checkPoint.nextXid;
			 SpinLockRelease(&xlogctl->info_lck);
		 }

		/* TLI should not change in an on-line checkpoint */
		if (checkPoint.ThisTimeLineID != ThisTimeLineID)
			ereport(PANIC,
					(errmsg("unexpected timeline ID %u (should be %u) in checkpoint record",
							checkPoint.ThisTimeLineID, ThisTimeLineID)));

		RecoveryRestartPoint(&checkPoint);
	}
	else if (info == XLOG_NOOP)
	{
		/* nothing to do here */
	}
	else if (info == XLOG_SWITCH)
	{
		/* nothing to do here */
	}
	else if (info == XLOG_HINT)
	{
		char *data;
		BkpBlock bkpb;

		/*
		 * Hint bit records contain a backup block stored "inline" in the normal
		 * data since the locking when writing hint records isn't sufficient to
		 * use the normal backup block mechanism, which assumes exclusive lock
		 * on the buffer supplied.
		 *
		 * Since the only change in these backup block are hint bits, there are
		 * no recovery conflicts generated.
		 *
		 * This also means there is no corresponding API call for this,
		 * so an smgr implementation has no need to implement anything.
		 * Which means nothing is needed in md.c etc
		 */
		data = XLogRecGetData(record);
		memcpy(&bkpb, data, sizeof(BkpBlock));
		data += sizeof(BkpBlock);

		RestoreBackupBlockContents(lsn, bkpb, data, false, false);
	}
	else if (info == XLOG_BACKUP_END)
	{
		XLogRecPtr	startpoint;

		memcpy(&startpoint, XLogRecGetData(record), sizeof(startpoint));

		if (XLByteEQ(ControlFile->backupStartPoint, startpoint))
		{
			/*
			 * We have reached the end of base backup, the point where
			 * pg_stop_backup() was done.
			 * Reset backupStartPoint, and update minRecoveryPoint to make
			 * sure we don't allow starting up at an earlier point even if
			 * recovery is stopped and restarted soon after this.
			 */
			elog(DEBUG1, "end of backup reached");

			LWLockAcquire(ControlFileLock, LW_EXCLUSIVE);

			if (XLByteLT(ControlFile->minRecoveryPoint, lsn))
				ControlFile->minRecoveryPoint = lsn;
			MemSet(&ControlFile->backupStartPoint, 0, sizeof(XLogRecPtr));
			ControlFile->backupEndRequired = false;
			UpdateControlFile();

			LWLockRelease(ControlFileLock);
		}
	}
}

void
xlog_desc(StringInfo buf, XLogRecPtr beginLoc, XLogRecord *record)
{
	uint8		info = record->xl_info & ~XLR_INFO_MASK;
	char		*rec = XLogRecGetData(record);

	if (info == XLOG_CHECKPOINT_SHUTDOWN ||
		info == XLOG_CHECKPOINT_ONLINE)
	{
		CheckPoint *checkpoint = (CheckPoint *) rec;

		CheckpointExtendedRecord ckptExtended;

		appendStringInfo(buf, "checkpoint: redo %X/%X; "
						 "tli %u; xid %u/%u; oid %u; relfilenode %u; multi %u; offset %u; %s",
						 checkpoint->redo.xlogid, checkpoint->redo.xrecoff,
						 checkpoint->ThisTimeLineID,
						 checkpoint->nextXidEpoch, checkpoint->nextXid,
						 checkpoint->nextOid,
						 checkpoint->nextRelfilenode,
						 checkpoint->nextMulti,
						 checkpoint->nextMultiOffset,
				 (info == XLOG_CHECKPOINT_SHUTDOWN) ? "shutdown" : "online");

		UnpackCheckPointRecord(record, &ckptExtended);

		if (ckptExtended.dtxCheckpointLen > 0)
		{
			appendStringInfo(buf,
				 ", checkpoint record data length = %u, DTX committed count %d, DTX data length %u",
							 record->xl_len,
							 ckptExtended.dtxCheckpoint->committedCount,
							 ckptExtended.dtxCheckpointLen);
			if (ckptExtended.ptas != NULL)
				appendStringInfo(buf,
								 ", prepared transaction agg state count = %d",
								 ckptExtended.ptas->count);
		}
	}
	else if (info == XLOG_NOOP)
	{
		appendStringInfo(buf, "xlog no-op");
	}
	else if (info == XLOG_NEXTOID)
	{
		Oid			nextOid;

		memcpy(&nextOid, rec, sizeof(Oid));
		appendStringInfo(buf, "nextOid: %u", nextOid);
	}
	else if (info == XLOG_HINT)
	{
		BkpBlock *bkpb = (BkpBlock *) rec;
		appendStringInfo(buf, "page hint: %u/%u/%u block %u",
						 bkpb->node.spcNode,
						 bkpb->node.dbNode,
						 bkpb->node.relNode,
						 bkpb->block);
	}
	else if (info == XLOG_NEXTRELFILENODE)
	{
		Oid			nextRelfilenode;

		memcpy(&nextRelfilenode, rec, sizeof(Oid));
		appendStringInfo(buf, "nextRelfilenode: %u", nextRelfilenode);
	}
	else if (info == XLOG_SWITCH)
	{
		appendStringInfo(buf, "xlog switch");
	}
	else
		appendStringInfo(buf, "UNKNOWN");
}

static void
xlog_outrec(StringInfo buf, XLogRecord *record)
{
	int			i;

	appendStringInfo(buf, "prev %X/%X; xid %u",
					 record->xl_prev.xlogid, record->xl_prev.xrecoff,
					 record->xl_xid);

	for (i = 0; i < XLR_MAX_BKP_BLOCKS; i++)
	{
		if (record->xl_info & XLR_SET_BKP_BLOCK(i))
			appendStringInfo(buf, "; bkpb%d", i + 1);
	}

	appendStringInfo(buf, ": %s", RmgrTable[record->xl_rmid].rm_name);
}


/*
 * Return the (possible) sync flag used for opening a file, depending on the
 * value of the GUC wal_sync_method.
 */
static int
get_sync_bit(int method)
{
	/* If fsync is disabled, never open in sync mode */
	if (!enableFsync)
		return 0;

	/*
	 * walreceiver process receives xlog data from walsender process.
	 * It needs to write the xlog data as soon as it receives and the amount it receives.
	 * As the amount of data received by it to write cannot be guaranteed to be
	 * OS/FS block size aligned, should never use O_DIRECT for the same.
	 * Also, as code is not expecting O_DIRECT to be used for xlog writes on walreceiver,
	 * the buffer pointer to perform xlog writes is not made usre to be OS/FS blocks size aligned.
	 */
	if (MyAuxProcType == WalReceiverProcess)
		return 0;

	switch (method)
	{
			/*
			 * enum values for all sync options are defined even if they are
			 * not supported on the current platform.  But if not, they are
			 * not included in the enum option array, and therefore will never
			 * be seen here.
			 */
		case SYNC_METHOD_FSYNC:
		case SYNC_METHOD_FSYNC_WRITETHROUGH:
		case SYNC_METHOD_FDATASYNC:
			return 0;
#ifdef OPEN_SYNC_FLAG
		case SYNC_METHOD_OPEN:
			return OPEN_SYNC_FLAG;
#endif
#ifdef OPEN_DATASYNC_FLAG
		case SYNC_METHOD_OPEN_DSYNC:
			return OPEN_DATASYNC_FLAG;
#endif
		default:
			/* can't happen (unless we are out of sync with option array) */
			elog(ERROR, "unrecognized wal_sync_method: %d", method);
			return 0;			/* silence warning */
	}
}

/*
 * GUC support
 */
bool
assign_xlog_sync_method(int new_sync_method, bool doit, GucSource source pg_attribute_unused() )
{
	if (!doit)
		return true;

	if (sync_method != new_sync_method)
	{
		/*
		 * To ensure that no blocks escape unsynced, force an fsync on the
		 * currently open log segment (if any).  Also, if the open flag is
		 * changing, close the log file so it will be reopened (with new flag
		 * bit) at next use.
		 */
		if (openLogFile >= 0)
		{
			if (pg_fsync(openLogFile) != 0)
				ereport(PANIC,
						(errcode_for_file_access(),
						 errmsg("could not fsync log file %u, segment %u: %m",
								openLogId, openLogSeg)));
			if (get_sync_bit(sync_method) != get_sync_bit(new_sync_method))
				XLogFileClose();
		}
	}

	return true;
}


/*
 * Issue appropriate kind of fsync (if any) for an XLOG output file.
 *
 * 'fd' is a file descriptor for the XLOG file to be fsync'd.
 * 'log' and 'seg' are for error reporting purposes.
 */
void
issue_xlog_fsync(int fd, uint32 log, uint32 seg)
{
	switch (sync_method)
	{
		case SYNC_METHOD_FSYNC:
			if (pg_fsync_no_writethrough(fd) != 0)
				ereport(PANIC,
						(errcode_for_file_access(),
						 errmsg("could not fsync log file %u, segment %u: %m",
								log, seg)));
			break;
#ifdef HAVE_FSYNC_WRITETHROUGH
		case SYNC_METHOD_FSYNC_WRITETHROUGH:
			if (pg_fsync_writethrough(fd) != 0)
				ereport(PANIC,
						(errcode_for_file_access(),
						 errmsg("could not fsync write-through log file %u, segment %u: %m",
								log, seg)));
			break;
#endif
#ifdef HAVE_FDATASYNC
		case SYNC_METHOD_FDATASYNC:
			if (pg_fdatasync(fd) != 0)
				ereport(PANIC,
						(errcode_for_file_access(),
					errmsg("could not fdatasync log file %u, segment %u: %m",
						   log, seg)));
			break;
#endif
		case SYNC_METHOD_OPEN:
		case SYNC_METHOD_OPEN_DSYNC:
			/* write synced it already */
			break;
		default:
			elog(PANIC, "unrecognized wal_sync_method: %d", sync_method);
			break;
	}
}


/*
 * do_pg_start_backup is the workhorse of the user-visible pg_start_backup()
 * function. It creates the necessary starting checkpoint and constructs the
 * backup label file.
 *
 * There are two kind of backups: exclusive and non-exclusive. An exclusive
 * backup is started with pg_start_backup(), and there can be only one active
 * at a time. The backup label file of an exclusive backup is written to
 * $PGDATA/backup_label, and it is removed by pg_stop_backup().
 *
 * A non-exclusive backup is used for the streaming base backups (see
 * src/backend/replication/basebackup.c). The difference to exclusive backups
 * is that the backup label file is not written to disk. Instead, its would-be
 * contents are returned in *labelfile, and the caller is responsible for
 * including it in the backup archive as 'backup_label'. There can be many
 * non-exclusive backups active at the same time, and they don't conflict
 * with an exclusive backup either.
 *
 * Every successfully started non-exclusive backup must be stopped by calling
 * do_pg_stop_backup() or do_pg_abort_backup().
 */
XLogRecPtr
do_pg_start_backup(const char *backupidstr, bool fast, char **labelfile)
{
	bool		exclusive = (labelfile == NULL);
	bool		backup_started_in_recovery = false;
	XLogRecPtr	checkpointloc;
	XLogRecPtr	startpoint;
	pg_time_t	stamp_time;
	char		strfbuf[128];
	char		xlogfilename[MAXFNAMELEN];
	uint32		_logId;
	uint32		_logSeg;
	struct stat stat_buf;
	FILE	   *fp;
	StringInfoData labelfbuf;

	/* base backup in recovery mode not currently supported */
	backup_started_in_recovery = false;

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
		   errmsg("must be superuser or replication role to run a backup")));

	if (strlen(backupidstr) > MAXPGPATH)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("backup label too long (max %d bytes)",
						MAXPGPATH)));

	if (!XLogIsNeeded())
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("WAL level not sufficient for making an online backup"),
				 errhint("wal_level must be set to \"archive\" or \"hot_standby\" at server start.")));

	/*
	 * Mark backup active in shared memory.  We must do full-page WAL writes
	 * during an on-line backup even if not doing so at other times, because
	 * it's quite possible for the backup dump to obtain a "torn" (partially
	 * written) copy of a database page if it reads the page concurrently with
	 * our write to the same page.	This can be fixed as long as the first
	 * write to the page in the WAL sequence is a full-page write. Hence, we
	 * turn on forcePageWrites and then force a CHECKPOINT, to ensure there
	 * are no dirty pages in shared memory that might get dumped while the
	 * backup is in progress without having a corresponding WAL record.  (Once
	 * the backup is complete, we need not force full-page writes anymore,
	 * since we expect that any pages not modified during the backup interval
	 * must have been correctly captured by the backup.)
	 *
	 * Note that forcePageWrites has no effect during an online backup from
	 * the standby.
	 *
	 * We must hold WALInsertLock to change the value of forcePageWrites, to
	 * ensure adequate interlocking against XLogInsert().
	 */
	LWLockAcquire(WALInsertLock, LW_EXCLUSIVE);
	if (exclusive)
	{
		if (XLogCtl->Insert.exclusiveBackup)
		{
			LWLockRelease(WALInsertLock);
			ereport(ERROR,
					(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
					 errmsg("a backup is already in progress"),
					 errhint("Run pg_stop_backup() and try again.")));
		}
		XLogCtl->Insert.exclusiveBackup = true;
	}
	else
		XLogCtl->Insert.nonExclusiveBackups++;
	XLogCtl->Insert.forcePageWrites = true;
	LWLockRelease(WALInsertLock);

	/*
	 * Force an XLOG file switch before the checkpoint, to ensure that the WAL
	 * segment the checkpoint is written to doesn't contain pages with old
	 * timeline IDs. That would otherwise happen if you called
	 * pg_start_backup() right after restoring from a PITR archive: the first
	 * WAL segment containing the startup checkpoint has pages in the
	 * beginning with the old timeline ID. That can cause trouble at recovery:
	 * we won't have a history file covering the old timeline if pg_xlog
	 * directory was not included in the base backup and the WAL archive was
	 * cleared too before starting the backup.
	 */
	RequestXLogSwitch();

	/* Ensure we release forcePageWrites if fail below */
	PG_ENSURE_ERROR_CLEANUP(pg_start_backup_callback, (Datum) BoolGetDatum(exclusive));
	{
		bool		gotUniqueStartpoint = false;

		/*
		 * Force an XLOG file switch before the checkpoint, to ensure that the
		 * WAL segment the checkpoint is written to doesn't contain pages with
		 * old timeline IDs.  That would otherwise happen if you called
		 * pg_start_backup() right after restoring from a PITR archive: the
		 * first WAL segment containing the startup checkpoint has pages in
		 * the beginning with the old timeline ID.	That can cause trouble at
		 * recovery: we won't have a history file covering the old timeline if
		 * pg_xlog directory was not included in the base backup and the WAL
		 * archive was cleared too before starting the backup.
		 *
		 * This also ensures that we have emitted a WAL page header that has
		 * XLP_BKP_REMOVABLE off before we emit the checkpoint record.
		 * Therefore, if a WAL archiver (such as pglesslog) is trying to
		 * compress out removable backup blocks, it won't remove any that
		 * occur after this point.
		 *
		 * During recovery, we skip forcing XLOG file switch, which means that
		 * the backup taken during recovery is not available for the special
		 * recovery case described above.
		 */
		if (!backup_started_in_recovery)
			RequestXLogSwitch();

		do
		{
			/*
			 * Force a CHECKPOINT.	Aside from being necessary to prevent torn
			 * page problems, this guarantees that two successive backup runs
			 * will have different checkpoint positions and hence different
			 * history file names, even if nothing happened in between.
			 *
			 * During recovery, establish a restartpoint if possible. We use
			 * the last restartpoint as the backup starting checkpoint. This
			 * means that two successive backup runs can have same checkpoint
			 * positions.
			 *
			 * Since the fact that we are executing do_pg_start_backup()
			 * during recovery means that checkpointer is running, we can use
			 * RequestCheckpoint() to establish a restartpoint.
			 *
			 * We use CHECKPOINT_IMMEDIATE only if requested by user (via
			 * passing fast = true).  Otherwise this can take awhile.
			 */
			RequestCheckpoint(CHECKPOINT_FORCE | CHECKPOINT_WAIT |
							  (fast ? CHECKPOINT_IMMEDIATE : 0));

			/*
			 * Now we need to fetch the checkpoint record location, and also
			 * its REDO pointer.  The oldest point in WAL that would be needed
			 * to restore starting from the checkpoint is precisely the REDO
			 * pointer.
			 */
			LWLockAcquire(ControlFileLock, LW_SHARED);
			checkpointloc = ControlFile->checkPoint;
			startpoint = ControlFile->checkPointCopy.redo;
			LWLockRelease(ControlFileLock);

			/*
			 * If two base backups are started at the same time (in WAL sender
			 * processes), we need to make sure that they use different
			 * checkpoints as starting locations, because we use the starting
			 * WAL location as a unique identifier for the base backup in the
			 * end-of-backup WAL record and when we write the backup history
			 * file. Perhaps it would be better generate a separate unique ID
			 * for each backup instead of forcing another checkpoint, but
			 * taking a checkpoint right after another is not that expensive
			 * either because only few buffers have been dirtied yet.
			 */
			LWLockAcquire(WALInsertLock, LW_SHARED);
			if (XLByteLT(XLogCtl->Insert.lastBackupStart, startpoint))
			{
				XLogCtl->Insert.lastBackupStart = startpoint;
				gotUniqueStartpoint = true;
			}
			LWLockRelease(WALInsertLock);
		} while (!gotUniqueStartpoint);

		XLByteToSeg(startpoint, _logId, _logSeg);
		XLogFileName(xlogfilename, ThisTimeLineID, _logId, _logSeg);

		/*
		 * Construct backup label file
		 */
		initStringInfo(&labelfbuf);

		/* Use the log timezone here, not the session timezone */
		stamp_time = (pg_time_t) time(NULL);
		pg_strftime(strfbuf, sizeof(strfbuf),
					"%Y-%m-%d %H:%M:%S %Z",
					pg_localtime(&stamp_time, log_timezone));
		appendStringInfo(&labelfbuf, "START WAL LOCATION: %X/%X (file %s)\n",
						 startpoint.xlogid, startpoint.xrecoff, xlogfilename);
		appendStringInfo(&labelfbuf, "CHECKPOINT LOCATION: %X/%X\n",
						 checkpointloc.xlogid, checkpointloc.xrecoff);
		appendStringInfo(&labelfbuf, "BACKUP METHOD: %s\n",
						 exclusive ? "pg_start_backup" : "streamed");
		appendStringInfo(&labelfbuf, "BACKUP FROM: %s\n",
						 backup_started_in_recovery ? "standby" : "master");
		appendStringInfo(&labelfbuf, "START TIME: %s\n", strfbuf);
		appendStringInfo(&labelfbuf, "LABEL: %s\n", backupidstr);

		elogif(debug_basebackup, LOG, "basebackup label file --\n%s", labelfbuf.data);

		/*
		 * Okay, write the file, or return its contents to caller.
		 */
		if (exclusive)
		{
			/*
			 * Check for existing backup label --- implies a backup is already
			 * running.  (XXX given that we checked exclusiveBackup above,
			 * maybe it would be OK to just unlink any such label file?)
			 */
			if (stat(BACKUP_LABEL_FILE, &stat_buf) != 0)
			{
				if (errno != ENOENT)
					ereport(ERROR,
							(errcode_for_file_access(),
							 errmsg("could not stat file \"%s\": %m",
									BACKUP_LABEL_FILE)));
			}
			else
				ereport(ERROR,
						(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
						 errmsg("a backup is already in progress"),
						 errhint("If you're sure there is no backup in progress, remove file \"%s\" and try again.",
								 BACKUP_LABEL_FILE)));

			fp = AllocateFile(BACKUP_LABEL_FILE, "w");

			if (!fp)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not create file \"%s\": %m",
								BACKUP_LABEL_FILE)));
			if (fwrite(labelfbuf.data, labelfbuf.len, 1, fp) != 1 ||
				fflush(fp) != 0 ||
				pg_fsync(fileno(fp)) != 0 ||
				ferror(fp) ||
				FreeFile(fp))
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not write file \"%s\": %m",
								BACKUP_LABEL_FILE)));
			pfree(labelfbuf.data);
		}
		else
			*labelfile = labelfbuf.data;
	}
	PG_END_ENSURE_ERROR_CLEANUP(pg_start_backup_callback, (Datum) BoolGetDatum(exclusive));

	/*
	 * We're done.  As a convenience, return the starting WAL location.
	 */
	return startpoint;
}

/* Error cleanup callback for pg_start_backup */
static void
pg_start_backup_callback(int code, Datum arg)
{
	bool		exclusive = DatumGetBool(arg);

	/* Update backup counters and forcePageWrites on failure */
	LWLockAcquire(WALInsertLock, LW_EXCLUSIVE);
	if (exclusive)
	{
		Assert(XLogCtl->Insert.exclusiveBackup);
		XLogCtl->Insert.exclusiveBackup = false;
	}
	else
	{
		Assert(XLogCtl->Insert.nonExclusiveBackups > 0);
		XLogCtl->Insert.nonExclusiveBackups--;
	}

	if (!XLogCtl->Insert.exclusiveBackup &&
		XLogCtl->Insert.nonExclusiveBackups == 0)
	{
		XLogCtl->Insert.forcePageWrites = false;
	}
	LWLockRelease(WALInsertLock);
}

/*
 * do_pg_stop_backup is the workhorse of the user-visible pg_stop_backup()
 * function.

 * If labelfile is NULL, this stops an exclusive backup. Otherwise this stops
 * the non-exclusive backup specified by 'labelfile'.
 */
XLogRecPtr
do_pg_stop_backup(char *labelfile)
{
	bool		exclusive = (labelfile == NULL);
	bool		backup_started_in_recovery = false;
	XLogRecPtr	startpoint;
	XLogRecPtr	stoppoint;
	XLogRecData rdata;
	pg_time_t	stamp_time;
	char		strfbuf[128];
	char		histfilepath[MAXPGPATH];
	char		startxlogfilename[MAXFNAMELEN];
	char		stopxlogfilename[MAXFNAMELEN];
	char		lastxlogfilename[MAXFNAMELEN];
	char		histfilename[MAXFNAMELEN];
	char		backupfrom[20];
	uint32		_logId;
	uint32		_logSeg;
	FILE	   *lfp;
	FILE	   *fp;
	char		ch;
	int			seconds_before_warning;
	int			waits = 0;
	char	   *remaining;
	char	   *ptr;

	/* Currently backup during recovery not supported */
	backup_started_in_recovery = false;

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
		 (errmsg("must be superuser or replication role to run a backup"))));

	if (!XLogIsNeeded())
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("WAL level not sufficient for making an online backup"),
				 errhint("wal_level must be set to \"archive\" or \"hot_standby\" at server start.")));

	/*
	 * OK to update backup counters and forcePageWrites
	 */
	LWLockAcquire(WALInsertLock, LW_EXCLUSIVE);
	if (exclusive)
		XLogCtl->Insert.exclusiveBackup = false;
	else
	{
		/*
		 * The user-visible pg_start/stop_backup() functions that operate on
		 * exclusive backups can be called at any time, but for non-exclusive
		 * backups, it is expected that each do_pg_start_backup() call is
		 * matched by exactly one do_pg_stop_backup() call.
		 */
		Assert(XLogCtl->Insert.nonExclusiveBackups > 0);
		XLogCtl->Insert.nonExclusiveBackups--;
	}

	if (!XLogCtl->Insert.exclusiveBackup &&
		XLogCtl->Insert.nonExclusiveBackups == 0)
	{
		XLogCtl->Insert.forcePageWrites = false;
	}
	LWLockRelease(WALInsertLock);

	if (exclusive)
	{
		/*
		 * Read the existing label file into memory.
		 */
		struct stat statbuf;
		int			r;

		if (stat(BACKUP_LABEL_FILE, &statbuf))
		{
			if (errno != ENOENT)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not stat file \"%s\": %m",
								BACKUP_LABEL_FILE)));
			ereport(ERROR,
					(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
					 errmsg("a backup is not in progress")));
		}

		lfp = AllocateFile(BACKUP_LABEL_FILE, "r");
		if (!lfp)
		{
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not read file \"%s\": %m",
							BACKUP_LABEL_FILE)));
		}
		labelfile = palloc(statbuf.st_size + 1);
		r = fread(labelfile, statbuf.st_size, 1, lfp);
		labelfile[statbuf.st_size] = '\0';

		/*
		 * Close and remove the backup label file
		 */
		if (r != 1 || ferror(lfp) || FreeFile(lfp))
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not read file \"%s\": %m",
							BACKUP_LABEL_FILE)));
		if (unlink(BACKUP_LABEL_FILE) != 0)
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not remove file \"%s\": %m",
							BACKUP_LABEL_FILE)));
	}

	/*
	 * Read and parse the START WAL LOCATION line (this code is pretty crude,
	 * but we are not expecting any variability in the file format).
	 */
	if (sscanf(labelfile, "START WAL LOCATION: %X/%X (file %24s)%c",
			   &startpoint.xlogid, &startpoint.xrecoff, startxlogfilename,
			   &ch) != 4 || ch != '\n')
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("invalid data in file \"%s\"", BACKUP_LABEL_FILE)));
	remaining = strchr(labelfile, '\n') + 1;	/* %n is not portable enough */

	/*
	 * Parse the BACKUP FROM line. If we are taking an online backup from the
	 * standby, we confirm that the standby has not been promoted during the
	 * backup.
	 */
	ptr = strstr(remaining, "BACKUP FROM:");
	if (!ptr || sscanf(ptr, "BACKUP FROM: %19s\n", backupfrom) != 1)
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("invalid data in file \"%s\"", BACKUP_LABEL_FILE)));
	if (strcmp(backupfrom, "standby") == 0 && !backup_started_in_recovery)
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("the standby was promoted during online backup"),
				 errhint("This means that the backup being taken is corrupt "
						 "and should not be used. "
						 "Try taking another online backup.")));

	/*
	 * Write the backup-end xlog record
	 */
	rdata.data = (char *) (&startpoint);
	rdata.len = sizeof(startpoint);
	rdata.buffer = InvalidBuffer;
	rdata.next = NULL;
	stoppoint = XLogInsert(RM_XLOG_ID, XLOG_BACKUP_END, &rdata);

	elog(LOG, "Basebackup stop point is at %X/%X.",
			   stoppoint.xlogid, stoppoint.xrecoff);

	/*
	 * Force a switch to a new xlog segment file, so that the backup is valid
	 * as soon as archiver moves out the current segment file.
	 */
	RequestXLogSwitch();

	XLByteToPrevSeg(stoppoint, _logId, _logSeg);
	XLogFileName(stopxlogfilename, ThisTimeLineID, _logId, _logSeg);

	/* Use the log timezone here, not the session timezone */
	stamp_time = (pg_time_t) time(NULL);
	pg_strftime(strfbuf, sizeof(strfbuf),
				"%Y-%m-%d %H:%M:%S %Z",
				pg_localtime(&stamp_time, log_timezone));

	/*
	 * Write the backup history file
	 */
	XLByteToSeg(startpoint, _logId, _logSeg);
	BackupHistoryFilePath(histfilepath, ThisTimeLineID, _logId, _logSeg,
						  startpoint.xrecoff % XLogSegSize);
	fp = AllocateFile(histfilepath, "w");
	if (!fp)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not create file \"%s\": %m",
						histfilepath)));
	fprintf(fp, "START WAL LOCATION: %X/%X (file %s)\n",
			startpoint.xlogid, startpoint.xrecoff, startxlogfilename);
	fprintf(fp, "STOP WAL LOCATION: %X/%X (file %s)\n",
			stoppoint.xlogid, stoppoint.xrecoff, stopxlogfilename);
	/* transfer remaining lines from label to history file */
	fprintf(fp, "%s", remaining);
	fprintf(fp, "STOP TIME: %s\n", strfbuf);
	if (fflush(fp) || ferror(fp) || FreeFile(fp))
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not write file \"%s\": %m",
						histfilepath)));

	/*
	 * Clean out any no-longer-needed history files.  As a side effect, this
	 * will post a .ready file for the newly created history file, notifying
	 * the archiver that history file may be archived immediately.
	 */
	CleanupBackupHistory();

	/*
	 * If archiving is enabled, wait for all the required WAL files to be
	 * archived before returning. If archiving isn't enabled, the required
	 * WAL needs to be transported via streaming replication (hopefully
	 * with wal_keep_segments set high enough), or some more exotic
	 * mechanism like polling and copying files from pg_xlog with script.
	 * We have no knowledge of those mechanisms, so it's up to the user to
	 * ensure that he gets all the required WAL.
	 *
	 * We wait until both the last WAL file filled during backup and the
	 * history file have been archived, and assume that the alphabetic
	 * sorting property of the WAL files ensures any earlier WAL files are
	 * safely archived as well.
	 *
	 * We wait forever, since archive_command is supposed to work and we
	 * assume the admin wanted his backup to work completely. If you don't
	 * wish to wait, you can set statement_timeout.
	 */
	if (XLogArchivingActive())
	{
	XLByteToPrevSeg(stoppoint, _logId, _logSeg);
	XLogFileName(lastxlogfilename, ThisTimeLineID, _logId, _logSeg);

	XLByteToSeg(startpoint, _logId, _logSeg);
	BackupHistoryFileName(histfilename, ThisTimeLineID, _logId, _logSeg,
						  startpoint.xrecoff % XLogSegSize);

	seconds_before_warning = 60;
	waits = 0;

	while (XLogArchiveIsBusy(lastxlogfilename) ||
		   XLogArchiveIsBusy(histfilename))
	{
		CHECK_FOR_INTERRUPTS();

		pg_usleep(1000000L);

		if (++waits >= seconds_before_warning)
		{
			seconds_before_warning *= 2;		/* This wraps in >10 years... */
			ereport(WARNING,
					(errmsg("pg_stop_backup still waiting for archive to complete (%d seconds elapsed)",
							waits)));
		}
	}
	}
	else
		ereport(NOTICE,
				(errmsg("WAL archiving is not enabled; you must ensure that all required WAL segments are copied through other means to complete the backup")));

	/*
	 * We're done.  As a convenience, return the ending WAL location.
	 */
	return stoppoint;
}

/*
 * do_pg_abort_backup: abort a running backup
 *
 * This does just the most basic steps of do_pg_stop_backup(), by taking the
 * system out of backup mode, thus making it a lot more safe to call from
 * an error handler.
 *
 * NB: This is only for aborting a non-exclusive backup that doesn't write
 * backup_label. A backup started with pg_stop_backup() needs to be finished
 * with pg_stop_backup().
 */
void
do_pg_abort_backup(void)
{
	LWLockAcquire(WALInsertLock, LW_EXCLUSIVE);
	Assert(XLogCtl->Insert.nonExclusiveBackups > 0);
	XLogCtl->Insert.nonExclusiveBackups--;

	if (!XLogCtl->Insert.exclusiveBackup &&
		XLogCtl->Insert.nonExclusiveBackups == 0)
	{
		XLogCtl->Insert.forcePageWrites = false;
	}
	LWLockRelease(WALInsertLock);
}


/*
 * pg_switch_xlog: switch to next xlog file
 */
Datum
pg_switch_xlog(PG_FUNCTION_ARGS)
{
	XLogRecPtr	switchpoint;
	char		location[MAXFNAMELEN];

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
			 (errmsg("must be superuser to switch transaction log files"))));

	switchpoint = RequestXLogSwitch();

	/*
	 * As a convenience, return the WAL location of the switch record
	 */
	snprintf(location, sizeof(location), "%X/%X",
			 switchpoint.xlogid, switchpoint.xrecoff);
	PG_RETURN_TEXT_P(cstring_to_text(location));
}

/*
 * Report the current WAL write location (same format as pg_start_backup etc)
 *
 * This is useful for determining how much of WAL is visible to an external
 * archiving process.  Note that the data before this point is written out
 * to the kernel, but is not necessarily synced to disk.
 */
Datum
pg_current_xlog_location(PG_FUNCTION_ARGS __attribute__((unused)) )
{
	char		location[MAXFNAMELEN];

	/* Make sure we have an up-to-date local LogwrtResult */
	{
		/* use volatile pointer to prevent code rearrangement */
		volatile XLogCtlData *xlogctl = XLogCtl;

		SpinLockAcquire(&xlogctl->info_lck);
		LogwrtResult = xlogctl->LogwrtResult;
		SpinLockRelease(&xlogctl->info_lck);
	}

	snprintf(location, sizeof(location), "%X/%X",
			 LogwrtResult.Write.xlogid, LogwrtResult.Write.xrecoff);
	PG_RETURN_TEXT_P(cstring_to_text(location));
}

/*
 * Report the current WAL insert location (same format as pg_start_backup etc)
 *
 * This function is mostly for debugging purposes.
 */
Datum
pg_current_xlog_insert_location(PG_FUNCTION_ARGS __attribute__((unused)) )
{
	XLogCtlInsert *Insert = &XLogCtl->Insert;
	XLogRecPtr	current_recptr;
	char		location[MAXFNAMELEN];

	/*
	 * Get the current end-of-WAL position ... shared lock is sufficient
	 */
	LWLockAcquire(WALInsertLock, LW_SHARED);
	INSERT_RECPTR(current_recptr, Insert, Insert->curridx);
	LWLockRelease(WALInsertLock);

	snprintf(location, sizeof(location), "%X/%X",
			 current_recptr.xlogid, current_recptr.xrecoff);
	PG_RETURN_TEXT_P(cstring_to_text(location));
}

/*
 * Compute an xlog file name and decimal byte offset given a WAL location,
 * such as is returned by pg_stop_backup() or pg_xlog_switch().
 *
 * Note that a location exactly at a segment boundary is taken to be in
 * the previous segment.  This is usually the right thing, since the
 * expected usage is to determine which xlog file(s) are ready to archive.
 */
Datum
pg_xlogfile_name_offset(PG_FUNCTION_ARGS)
{
	text	   *location = PG_GETARG_TEXT_P(0);
	char	   *locationstr;
	unsigned int uxlogid;
	unsigned int uxrecoff;
	uint32		xlogid;
	uint32		xlogseg;
	uint32		xrecoff;
	XLogRecPtr	locationpoint;
	char		xlogfilename[MAXFNAMELEN];
	Datum		values[2];
	bool		isnull[2];
	TupleDesc	resultTupleDesc;
	HeapTuple	resultHeapTuple;
	Datum		result;

	/*
	 * Read input and parse
	 */
	locationstr = text_to_cstring(location);

	if (sscanf(locationstr, "%X/%X", &uxlogid, &uxrecoff) != 2)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("could not parse transaction log location \"%s\"",
						locationstr)));

	locationpoint.xlogid = uxlogid;
	locationpoint.xrecoff = uxrecoff;

	/*
	 * Construct a tuple descriptor for the result row.  This must match this
	 * function's pg_proc entry!
	 */
	resultTupleDesc = CreateTemplateTupleDesc(2, false);
	TupleDescInitEntry(resultTupleDesc, (AttrNumber) 1, "file_name",
					   TEXTOID, -1, 0);
	TupleDescInitEntry(resultTupleDesc, (AttrNumber) 2, "file_offset",
					   INT4OID, -1, 0);

	resultTupleDesc = BlessTupleDesc(resultTupleDesc);

	/*
	 * xlogfilename
	 */
	XLByteToPrevSeg(locationpoint, xlogid, xlogseg);
	XLogFileName(xlogfilename, ThisTimeLineID, xlogid, xlogseg);

	values[0] = CStringGetTextDatum(xlogfilename);
	isnull[0] = false;

	/*
	 * offset
	 */
	xrecoff = locationpoint.xrecoff - xlogseg * XLogSegSize;

	values[1] = UInt32GetDatum(xrecoff);
	isnull[1] = false;

	/*
	 * Tuple jam: Having first prepared your Datums, then squash together
	 */
	resultHeapTuple = heap_form_tuple(resultTupleDesc, values, isnull);

	result = HeapTupleGetDatum(resultHeapTuple);

	PG_RETURN_DATUM(result);
}

/*
 * Compute an xlog file name given a WAL location,
 * such as is returned by pg_stop_backup() or pg_xlog_switch().
 */
Datum
pg_xlogfile_name(PG_FUNCTION_ARGS)
{
	text	   *location = PG_GETARG_TEXT_P(0);
	char	   *locationstr;
	unsigned int uxlogid;
	unsigned int uxrecoff;
	uint32		xlogid;
	uint32		xlogseg;
	XLogRecPtr	locationpoint;
	char		xlogfilename[MAXFNAMELEN];

	locationstr = text_to_cstring(location);

	if (sscanf(locationstr, "%X/%X", &uxlogid, &uxrecoff) != 2)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("could not parse transaction log location \"%s\"",
						locationstr)));

	locationpoint.xlogid = uxlogid;
	locationpoint.xrecoff = uxrecoff;

	XLByteToPrevSeg(locationpoint, xlogid, xlogseg);
	XLogFileName(xlogfilename, ThisTimeLineID, xlogid, xlogseg);

	PG_RETURN_TEXT_P(cstring_to_text(xlogfilename));
}

/*
 * read_backup_label: check to see if a backup_label file is present
 *
 * If we see a backup_label during recovery, we assume that we are recovering
 * from a backup dump file, and we therefore roll forward from the checkpoint
 * identified by the label file, NOT what pg_control says.	This avoids the
 * problem that pg_control might have been archived one or more checkpoints
 * later than the start of the dump, and so if we rely on it as the start
 * point, we will fail to restore a consistent database state.
 *
 * Returns TRUE if a backup_label was found (and fills the checkpoint
 * location and its REDO location into *checkPointLoc and RedoStartLSN,
 * respectively); returns FALSE if not. If this backup_label came from a
 * streamed backup, *backupEndRequired is set to TRUE.
 */
static bool
read_backup_label(XLogRecPtr *checkPointLoc, bool *backupEndRequired)
{
	char		startxlogfilename[MAXFNAMELEN];
	TimeLineID	tli;
	FILE	   *lfp;
	char		ch;
	char		backuptype[20];
	char		backupfrom[20];

	*backupEndRequired = false;

	/*
	 * See if label file is present
	 */
	lfp = AllocateFile(BACKUP_LABEL_FILE, "r");
	if (!lfp)
	{
		if (errno != ENOENT)
			ereport(FATAL,
					(errcode_for_file_access(),
					 errmsg("could not read file \"%s\": %m",
							BACKUP_LABEL_FILE)));
		return false;			/* it's not there, all is fine */
	}

	/*
	 * Read and parse the START WAL LOCATION, CHECKPOINT and BACKUP_METHOD
	 * lines (this code is pretty crude, but we are not expecting any variability
	 * in the file format).
	 */
	if (fscanf(lfp, "START WAL LOCATION: %X/%X (file %08X%16s)%c",
			   &RedoStartLSN.xlogid, &RedoStartLSN.xrecoff, &tli,
			   startxlogfilename, &ch) != 5 || ch != '\n')
		ereport(FATAL,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("invalid data in file \"%s\"", BACKUP_LABEL_FILE)));

	if (fscanf(lfp, "CHECKPOINT LOCATION: %X/%X%c",
			   &checkPointLoc->xlogid, &checkPointLoc->xrecoff,
			   &ch) != 3 || ch != '\n')
		ereport(FATAL,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("invalid data in file \"%s\"", BACKUP_LABEL_FILE)));

	if (fscanf(lfp, "BACKUP METHOD: %19s\n", backuptype) == 1)
	{
		/* Streaming backup method is only supported */
		if (strcmp(backuptype, "streamed") == 0)
			*backupEndRequired = true;
		else
			ereport(FATAL,
					(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
					 errmsg("invalid data in file \"%s\"", BACKUP_LABEL_FILE)));

	}

	if (fscanf(lfp, "BACKUP FROM: %19s\n", backupfrom) == 1)
	{
		/* Backup from standby is not supported */
		if (strcmp(backupfrom, "master") != 0)
			ereport(FATAL,
					(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
					 errmsg("invalid data in file \"%s\"", BACKUP_LABEL_FILE)));
	}

	if (ferror(lfp) || FreeFile(lfp))
		ereport(FATAL,
				(errcode_for_file_access(),
				 errmsg("could not read file \"%s\": %m",
						BACKUP_LABEL_FILE)));

	return true;
}

/*
 * Get latest redo apply position.
 *
 * Optionally, returns the current recovery target timeline. Callers not
 * interested in that may pass NULL for targetTLI.
 *
 * Exported to allow WAL receiver to read the pointer directly.
 */
XLogRecPtr
GetXLogReplayRecPtr(TimeLineID *targetTLI)
{
	/* use volatile pointer to prevent code rearrangement */
	volatile XLogCtlData *xlogctl = XLogCtl;
	XLogRecPtr	recptr;
	uint32		freespace;

	SpinLockAcquire(&xlogctl->info_lck);
	recptr = xlogctl->lastReplayedEndRecPtr;
	if (targetTLI)
		*targetTLI = xlogctl->RecoveryTargetTLI;
	SpinLockRelease(&xlogctl->info_lck);

	/*
	 * No more records fit on this page. Report the apply location
	 * as the end of the page.
	 *
	 * GPDB_93_MERGE_FIXME: The need for this goes away in PG 9.3. After
	 * commit 061e7efb1b, the page header can be split across pages, too.
	 */
	freespace = XLOG_BLCKSZ - (recptr.xrecoff % XLOG_BLCKSZ);
	if (freespace < SizeOfXLogRecord)
	{
		XLByteAdvance(recptr, freespace);
	}

	return recptr;
}

/*
 * Get current standby flush position, ie, the last WAL position
 * known to be fsync'd to disk in standby.
 *
 * If 'targetTLI' is not NULL, it's set to the current recovery target
 * timeline.
 */
XLogRecPtr
GetStandbyFlushRecPtr(TimeLineID *targetTLI)
{
	XLogRecPtr      receivePtr;
	XLogRecPtr      replayPtr;

	receivePtr = GetWalRcvWriteRecPtr(NULL);
	replayPtr = GetXLogReplayRecPtr(targetTLI);

	if (XLByteLT(receivePtr, replayPtr))
		return replayPtr;
	else
		return receivePtr;
}

/*
 * GetRecoveryTargetTLI - get the current recovery target timeline ID
 */
TimeLineID
GetRecoveryTargetTLI(void)
{
	/* use volatile pointer to prevent code rearrangement */
	volatile XLogCtlData *xlogctl = XLogCtl;
	TimeLineID result;

	SpinLockAcquire(&xlogctl->info_lck);
	result = xlogctl->RecoveryTargetTLI;
	SpinLockRelease(&xlogctl->info_lck);

	return result;
}

/*
 * Error context callback for errors occurring during rm_redo().
 */
static void
rm_redo_error_callback(void *arg)
{
	RedoErrorCallBack *redoErrorCallBack = (RedoErrorCallBack*) arg;
	StringInfoData buf;

	initStringInfo(&buf);
	RmgrTable[redoErrorCallBack->record->xl_rmid].rm_desc(
												   &buf,
												   redoErrorCallBack->location,
												   redoErrorCallBack->record);

	/* don't bother emitting empty description */
	if (buf.len > 0)
		errcontext("xlog redo %s", buf.data);

	pfree(buf.data);
}

#if 0 /* GPDB doesn't have online backup */
/*
 * BackupInProgress: check if online backup mode is active
 *
 * This is done by checking for existence of the "backup_label" file.
 */
bool
BackupInProgress(void)
{
	struct stat stat_buf;

	return (stat(BACKUP_LABEL_FILE, &stat_buf) == 0);
}

/*
 * CancelBackup: rename the "backup_label" file to cancel backup mode
 *
 * If the "backup_label" file exists, it will be renamed to "backup_label.old".
 * Note that this will render an online backup in progress useless.
 * To correctly finish an online backup, pg_stop_backup must be called.
 */
void
CancelBackup(void)
{
	struct stat stat_buf;

	/* if the file is not there, return */
	if (stat(BACKUP_LABEL_FILE, &stat_buf) < 0)
		return;

	/* remove leftover file from previously cancelled backup if it exists */
	unlink(BACKUP_LABEL_OLD);

	if (rename(BACKUP_LABEL_FILE, BACKUP_LABEL_OLD) == 0)
	{
		ereport(LOG,
				(errmsg("online backup mode cancelled"),
				 errdetail("\"%s\" was renamed to \"%s\".",
						   BACKUP_LABEL_FILE, BACKUP_LABEL_OLD)));
	}
	else
	{
		ereport(WARNING,
				(errcode_for_file_access(),
				 errmsg("online backup mode was not cancelled"),
				 errdetail("Could not rename \"%s\" to \"%s\": %m.",
						   BACKUP_LABEL_FILE, BACKUP_LABEL_OLD)));
	}
}
#endif

static char *
XLogLocationToBuffer(char *buffer, XLogRecPtr *loc, bool longFormat)
{

	if (longFormat)
	{
		uint32 seg = loc->xrecoff / XLogSegSize;
		uint32 offset = loc->xrecoff % XLogSegSize;
		sprintf(buffer,
			    "%X/%X (==> seg %d, offset 0x%X)",
			    loc->xlogid, loc->xrecoff,
			    seg, offset);
	}
	else
		sprintf(buffer,
			    "%X/%X",
			    loc->xlogid, loc->xrecoff);

	return buffer;
}

static char xlogLocationBuffer[50];
static char xlogLocationBuffer2[50];
static char xlogLocationBuffer3[50];
static char xlogLocationBuffer4[50];
static char xlogLocationBuffer5[50];

char *
XLogLocationToString(XLogRecPtr *loc)
{
	return XLogLocationToBuffer(xlogLocationBuffer, loc, Debug_print_qd_mirroring);
}

char *
XLogLocationToString2(XLogRecPtr *loc)
{
	return XLogLocationToBuffer(xlogLocationBuffer2, loc, Debug_print_qd_mirroring);
}

char *
XLogLocationToString3(XLogRecPtr *loc)
{
	return XLogLocationToBuffer(xlogLocationBuffer3, loc, Debug_print_qd_mirroring);
}

char *
XLogLocationToString4(XLogRecPtr *loc)
{
	return XLogLocationToBuffer(xlogLocationBuffer4, loc, Debug_print_qd_mirroring);
}

char *
XLogLocationToString5(XLogRecPtr *loc)
{
	return XLogLocationToBuffer(xlogLocationBuffer5, loc, Debug_print_qd_mirroring);
}

char *
XLogLocationToString_Long(XLogRecPtr *loc)
{
	return XLogLocationToBuffer(xlogLocationBuffer, loc, true);
}

char *
XLogLocationToString2_Long(XLogRecPtr *loc)
{
	return XLogLocationToBuffer(xlogLocationBuffer2, loc, true);
}

char *
XLogLocationToString3_Long(XLogRecPtr *loc)
{
	return XLogLocationToBuffer(xlogLocationBuffer3, loc, true);
}

char *
XLogLocationToString4_Long(XLogRecPtr *loc)
{
	return XLogLocationToBuffer(xlogLocationBuffer4, loc, true);
}

char *
XLogLocationToString5_Long(XLogRecPtr *loc)
{
	return XLogLocationToBuffer(xlogLocationBuffer5, loc, true);
}


/* ------------------------------------------------------
 *	Startup Process main entry point and signal handlers
 * ------------------------------------------------------
 */

/*
 * startupproc_quickdie() occurs when signalled SIGQUIT by the postmaster.
 *
 * Some backend has bought the farm,
 * so we need to stop what we're doing and exit.
 */
static void
startupproc_quickdie(SIGNAL_ARGS)
{
	PG_SETMASK(&BlockSig);

	/*
	 * We DO NOT want to run proc_exit() callbacks -- we're here because
	 * shared memory may be corrupted, so we don't want to try to clean up our
	 * transaction.  Just nail the windows shut and get out of town.  Now that
	 * there's an atexit callback to prevent third-party code from breaking
	 * things by calling exit() directly, we have to reset the callbacks
	 * explicitly to make this work as intended.
	 */
	on_exit_reset();

	/*
	 * Note we do exit(2) not exit(0).	This is to force the postmaster into a
	 * system reset cycle if some idiot DBA sends a manual SIGQUIT to a random
	 * backend.  This is necessary precisely because we don't clean up our
	 * shared memory state.  (The "dead man switch" mechanism in pmsignal.c
	 * should ensure the postmaster sees this as a crash, too, but no harm in
	 * being doubly sure.)
	 */
	exit(2);
}

/* SIGUSR2: set flag to finish recovery */
static void
StartupProcTriggerHandler(SIGNAL_ARGS)
{
	int			save_errno = errno;

	WakeupRecovery();

	errno = save_errno;
}

/* SIGUSR1: let latch facility handle the signal */
static void
StartupProcSigUsr1Handler(SIGNAL_ARGS)
{
	latch_sigusr1_handler();
}

/* SIGHUP: set flag to re-read config file at next convenient time */
static void
StartupProcSigHupHandler(SIGNAL_ARGS)
{
	int			save_errno = errno;

	got_SIGHUP = true;
	WakeupRecovery();

	errno = save_errno;
}

/* SIGTERM: set flag to abort redo and exit */
static void
StartupProcShutdownHandler(SIGNAL_ARGS)
{
	int			save_errno = errno;

	if (in_restore_command)
		proc_exit(1);
	else
		shutdown_requested = true;
	WakeupRecovery();

	errno = save_errno;
}

/* Handle SIGHUP and SIGTERM signals of startup process */
void
HandleStartupProcInterrupts(void)
{
	/*
	 * Check if we were requested to re-read config file.
	 */
	if (got_SIGHUP)
	{
		got_SIGHUP = false;
		ProcessConfigFile(PGC_SIGHUP);
	}

	/*
	 * Check if we were requested to exit without finishing recovery.
	 */
	if (shutdown_requested)
		proc_exit(1);

	/*
	 * Emergency bailout if postmaster has died.  This is to avoid the
	 * necessity for manual cleanup of all postmaster children.
	 */
	if (IsUnderPostmaster && !PostmasterIsAlive(true))
		exit(1);
}

static void
HandleCrash(SIGNAL_ARGS)
{
    /**
     * Handle crash is registered as a signal handler for SIGILL/SIGBUS/SIGSEGV
     *
     * This simply calls the standard handler which will log the signal and reraise the
     *      signal if needed
     */
    StandardHandlerForSigillSigsegvSigbus_OnMainThread("a startup process", PASS_SIGNAL_ARGS);
}

/* Main entry point for startup process */
void
StartupProcessMain(void)
{
	am_startup = true;
	/*
	 * If possible, make this process a group leader, so that the postmaster
	 * can signal any child processes too.
	 */
#ifdef HAVE_SETSID
	if (setsid() < 0)
		elog(FATAL, "setsid() failed: %m");
#endif

	/*
	 * Properly accept or ignore signals the postmaster might send us
	 */
	pqsignal(SIGHUP, StartupProcSigHupHandler);	 /* reload config file */
	pqsignal(SIGINT, SIG_IGN);					/* ignore query cancel */
	pqsignal(SIGTERM, StartupProcShutdownHandler); /* request shutdown */
	pqsignal(SIGQUIT, startupproc_quickdie);		/* hard crash time */
	pqsignal(SIGALRM, SIG_IGN);
	pqsignal(SIGPIPE, SIG_IGN);
	pqsignal(SIGUSR1, StartupProcSigUsr1Handler);
	pqsignal(SIGUSR2, StartupProcTriggerHandler);

#ifdef SIGBUS
	pqsignal(SIGBUS, HandleCrash);
#endif
#ifdef SIGILL
    pqsignal(SIGILL, HandleCrash);
#endif
#ifdef SIGSEGV
	pqsignal(SIGSEGV, HandleCrash);
#endif

	/*
	 * Reset some signals that are accepted by postmaster but not here
	 */
	pqsignal(SIGCHLD, SIG_DFL);
	pqsignal(SIGTTIN, SIG_DFL);
	pqsignal(SIGTTOU, SIG_DFL);
	pqsignal(SIGCONT, SIG_DFL);
	pqsignal(SIGWINCH, SIG_DFL);

	/*
	 * Unblock signals (they were blocked when the postmaster forked us)
	 */
	PG_SETMASK(&UnBlockSig);

	StartupXLOG();

	BuildFlatFiles(false);

	/*
	 * Exit normally. Exit code 0 tells postmaster that we completed
	 * recovery successfully.
	 */
	proc_exit(0);
}

/*
 * Check to see whether the user-specified trigger file exists and whether a
 * promote request has arrived.  If either condition holds, request postmaster
 * to shut down walreceiver, wait for it to exit, and return true.
 */
static bool
CheckForStandbyTrigger(void)
{
	static bool triggered = false;

	if (triggered)
		return true;

	if (CheckPromoteSignal(true))
	{
		ereport(LOG,
				(errmsg("received promote request")));
		ShutdownWalRcv();
		triggered = true;
		return true;
	}

	return false;
}

/*
 * Check to see if a promote request has arrived. Should be
 * called by postmaster after receiving SIGUSR1.
 */
bool
CheckPromoteSignal(bool do_unlink)
{
	struct stat stat_buf;

	if (stat(PROMOTE_SIGNAL_FILE, &stat_buf) == 0)
	{
		/*
		 * Since we are in a signal handler, it's not safe to elog. We
		 * silently ignore any error from unlink.
		 */
		if (do_unlink)
			unlink(PROMOTE_SIGNAL_FILE);
		return true;
	}
	return false;
}

/*
 * Put the current standby master dbid in the shared memory, which will
 * be looked up from mmxlog.
 */
void
SetStandbyDbid(int16 dbid)
{
	/* use volatile pointer to prevent code rearrangement */
	volatile XLogCtlData *xlogctl = XLogCtl;

	SpinLockAcquire(&xlogctl->info_lck);
	xlogctl->standbyDbid = dbid;
	SpinLockRelease(&xlogctl->info_lck);

	/*
	 * Let postmaster know we've changed standby dbid.
	 */
	SendPostmasterSignal(PMSIGNAL_SEGCONFIG_CHANGE);
}

/*
 * Returns current standby dbid.
 */
int16
GetStandbyDbid(void)
{
	/* use volatile pointer to prevent code rearrangement */
	volatile XLogCtlData *xlogctl = XLogCtl;
	int16	dbid;

	SpinLockAcquire(&xlogctl->info_lck);
	dbid = xlogctl->standbyDbid;
	SpinLockRelease(&xlogctl->info_lck);

	return dbid;
}

/*
 * True if we are running standby-mode continuous recovery.
 * Note this would return false after finishing the recovery, even if
 * we are still on standby master with a primary master running.
 * Also this only works in the startup process as the StandbyMode
 * flag is not in shared memory.
 */
bool
IsStandbyMode(void)
{
	return StandbyMode;
}

static void
GetXLogCleanUpTo(XLogRecPtr recptr, uint32 *_logId, uint32 *_logSeg)
{
	/*
	 * See if we have a live WAL sender and see if it has a
	 * start xlog location (with active basebackup) or standby fsync location
	 * (with active standby). We have to compare it with prev. checkpoint
	 * location. We use the min out of them to figure out till
	 * what point we need to save the xlog seg files
	 */
	XLogRecPtr xlogCleanUpTo = WalSndCtlGetXLogCleanUpTo();
	if (!XLogRecPtrIsInvalid(xlogCleanUpTo))
	{
		if (XLByteLT(recptr, xlogCleanUpTo))
			xlogCleanUpTo = recptr;
	}
	else
		xlogCleanUpTo = recptr;

	CheckKeepWalSegments(xlogCleanUpTo, _logId, _logSeg);
}

/*
 * Checks whether the current buffer page and backup page stored in the
 * WAL record are consistent or not. Before comparing the two pages, a
 * masking can be applied to the pages to ignore certain areas like hint bits,
 * unused space between pd_lower and pd_upper among other things. This
 * function should be called once WAL replay has been completed for a
 * given record.
 */
static void
checkXLogConsistency(XLogRecord *record, XLogRecPtr EndRecPtr)
{
	RmgrId		rmid = record->xl_rmid;
	char       *blk;

	/* Records with no backup blocks have no need for consistency checks. */
	if (!(record->xl_info & XLR_BKP_BLOCK_MASK))
		return;

	Assert((record->xl_extended_info & XLR_CHECK_CONSISTENCY) != 0);

	blk = (char *) XLogRecGetData(record) + record->xl_len;
	for (int i = 0; i < XLR_MAX_BKP_BLOCKS; i++)
	{
		BkpBlock    bkpb;
		Buffer		buf;
		Page		page;
		char       *src_buffer;

		if (!(record->xl_info & XLR_SET_BKP_BLOCK(i)))
		{
			/*
			 * WAL record doesn't contain a block do nothing.
			 */
			continue;
		}

		memcpy(&bkpb, blk, sizeof(BkpBlock));
		blk += sizeof(BkpBlock);
		src_buffer = blk;
		/* move on to point to next block */
		blk += BLCKSZ - bkpb.hole_length;

		if (bkpb.block_info & BLOCK_APPLY)
		{
			/*
			 * WAL record has already applied the page, so bypass the
			 * consistency check as that would result in comparing the full
			 * page stored in the record with itself.
			 */
			continue;
		}

		/*
		 * Read the contents from the current buffer and store it in a
		 * temporary page.
		 */
		buf = XLogReadBuffer(bkpb.node, bkpb.block, false);
		if (!BufferIsValid(buf))
			continue;

		page = BufferGetPage(buf);

		/*
		 * Take a copy of the local page where WAL has been applied to have a
		 * comparison base before masking it...
		 */
		memcpy(replay_image_masked, page, BLCKSZ);

		/* No need for this page anymore now that a copy is in. */
		UnlockReleaseBuffer(buf);

		/*
		 * If the block LSN is already ahead of this WAL record, we can't
		 * expect contents to match.  This can happen if recovery is
		 * restarted.
		 */
		if (XLByteLT(EndRecPtr, PageGetLSN(replay_image_masked)))
			continue;

		/*
		 * Read the contents from the backup copy, stored in WAL record and
		 * store it in a temporary page. There is no need to allocate a new
		 * page here, a local buffer is fine to hold its contents and a mask
		 * can be directly applied on it.
		 */
		if (bkpb.hole_length == 0)
		{
			memcpy((char *) master_image_masked, src_buffer, BLCKSZ);
		}
		else
		{
			/* zero-fill the hole, anyways gets masked out */
			MemSet((char *) master_image_masked, 0, BLCKSZ);
			memcpy((char *) master_image_masked, src_buffer, bkpb.hole_offset);
			memcpy((char *) master_image_masked + (bkpb.hole_offset + bkpb.hole_length),
				   src_buffer + bkpb.hole_offset,
				   BLCKSZ - (bkpb.hole_offset + bkpb.hole_length));
		}

		/*
		 * If masking function is defined, mask both the master and replay
		 * images
		 */
		if (RmgrTable[rmid].rm_mask != NULL)
		{
			RmgrTable[rmid].rm_mask(replay_image_masked, bkpb.block);
			RmgrTable[rmid].rm_mask(master_image_masked, bkpb.block);
		}

		/* Time to compare the master and replay images. */
		if (memcmp(replay_image_masked, master_image_masked, BLCKSZ) != 0)
		{
			elog(FATAL,
				 "inconsistent page found, rel %u/%u/%u, blkno %u",
				 bkpb.node.spcNode, bkpb.node.dbNode, bkpb.node.relNode,
				 bkpb.block);
		}
		else
		{
			elog(DEBUG1,
				 "Consistent page for rel %u/%u/%u, blkno %u",
				 bkpb.node.spcNode, bkpb.node.dbNode, bkpb.node.relNode,
				 bkpb.block);
		}
	}
}

/*
 * Wake up startup process to replay newly arrived WAL, or to notice that
 * failover has been requested.
 */
void
WakeupRecovery(void)
{
	SetLatch(&XLogCtl->recoveryWakeupLatch);
}
