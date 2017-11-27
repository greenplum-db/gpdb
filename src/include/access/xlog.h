/*
 * xlog.h
 *
 * PostgreSQL transaction log manager
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/access/xlog.h,v 1.89 2009/01/01 17:23:56 momjian Exp $
 */
#ifndef XLOG_H
#define XLOG_H

#include "access/rmgr.h"
#include "access/xlogdefs.h"
#include "catalog/gp_segment_config.h"
#include "catalog/pg_control.h"
#include "lib/stringinfo.h"
#include "storage/buf.h"
#include "utils/pg_crc.h"
#include "utils/relcache.h"
#include "utils/timestamp.h"
#include "cdb/cdbpublic.h"
#include "replication/walsender.h"

/*
 * REDO Tracking DEFINEs.
 */
#define REDO_PRINT_READ_BUFFER_NOT_FOUND(rnode,blkno,buffer,lsn) \
{ \
	if (Debug_persistent_recovery_print && !BufferIsValid(buffer)) \
	{ \
		xlog_print_redo_read_buffer_not_found(rnode, blkno, lsn, PG_FUNCNAME_MACRO); \
	} \
}

#define REDO_PRINT_LSN_APPLICATION(rnode,blkno,page,lsn) \
{ \
	if (Debug_persistent_recovery_print) \
	{ \
		xlog_print_redo_lsn_application(rnode, blkno, (void*)page, lsn, PG_FUNCNAME_MACRO); \
	} \
}

/*
 * The overall layout of an XLOG record is:
 *		Fixed-size header (XLogRecord struct)
 *		rmgr-specific data
 *		BkpBlock
 *		backup block data
 *		BkpBlock
 *		backup block data
 *		...
 *
 * where there can be zero to three backup blocks (as signaled by xl_info flag
 * bits).  XLogRecord structs always start on MAXALIGN boundaries in the WAL
 * files, and we round up SizeOfXLogRecord so that the rmgr data is also
 * guaranteed to begin on a MAXALIGN boundary.	However, no padding is added
 * to align BkpBlock structs or backup block data.
 *
 * NOTE: xl_len counts only the rmgr data, not the XLogRecord header,
 * and also not any backup blocks.	xl_tot_len counts everything.  Neither
 * length field is rounded up to an alignment boundary.
 */
typedef struct XLogRecord
{
	pg_crc32	xl_crc;			/* CRC for this record */
	XLogRecPtr	xl_prev;		/* ptr to previous record in log */
	TransactionId xl_xid;		/* xact id */
	uint32		xl_tot_len;		/* total len of entire record */
	uint32		xl_len;			/* total len of rmgr data */
	uint8		xl_info;		/* flag bits, see below */
	RmgrId		xl_rmid;		/* resource manager for this record */
	uint8       xl_extended_info; /* flag bits, see below */

	/* Depending on MAXALIGN, there are either 2 or 6 wasted bytes here */

	/* ACTUAL LOG DATA FOLLOWS AT END OF STRUCT */

} XLogRecord;

#define SizeOfXLogRecord	MAXALIGN(sizeof(XLogRecord))

#define XLogRecGetData(record)	((char*) (record) + SizeOfXLogRecord)

/*
 * XLOG uses only low 4 bits of xl_info. High 4 bits may be used by rmgr.
 * XLR_CHECK_CONSISTENCY bits can be passed by XLogInsert caller.
 */
#define XLR_INFO_MASK			0x0F

/*
 * Enforces consistency checks of replayed WAL at recovery. If enabled,
 * each record will log a full-page write for each block modified by the
 * record and will reuse it afterwards for consistency checks. The caller
 * of XLogInsert can use this value if necessary, but if
 * wal_consistency_checking is enabled for a rmgr this is set unconditionally.
 */
#define XLR_CHECK_CONSISTENCY 0x02

/*
 * If we backed up any disk blocks with the XLOG record, we use flag bits in
 * xl_info to signal it.  We support backup of up to 3 disk blocks per XLOG
 * record.
 */
#define XLR_BKP_BLOCK_MASK		0x0E	/* all info bits used for bkp blocks */
#define XLR_MAX_BKP_BLOCKS		3
#define XLR_SET_BKP_BLOCK(iblk) (0x08 >> (iblk))
#define XLR_BKP_BLOCK_1			XLR_SET_BKP_BLOCK(0)	/* 0x08 */
#define XLR_BKP_BLOCK_2			XLR_SET_BKP_BLOCK(1)	/* 0x04 */
#define XLR_BKP_BLOCK_3			XLR_SET_BKP_BLOCK(2)	/* 0x02 */

/*
 * Bit 0 of xl_info is set if the backed-up blocks could safely be removed
 * from a compressed version of XLOG (that is, they are backed up only to
 * prevent partial-page-write problems, and not to ensure consistency of PITR
 * recovery).  The compression algorithm would need to extract data from the
 * blocks to create an equivalent non-full-page XLOG record.
 */
#define XLR_BKP_REMOVABLE		0x01

/* Sync methods */
#define SYNC_METHOD_FSYNC		0
#define SYNC_METHOD_FDATASYNC	1
#define SYNC_METHOD_OPEN		2		/* for O_SYNC */
#define SYNC_METHOD_FSYNC_WRITETHROUGH	3
#define SYNC_METHOD_OPEN_DSYNC	4		/* for O_DSYNC */
extern int	sync_method;

/*
 * The rmgr data to be written by XLogInsert() is defined by a chain of
 * one or more XLogRecData structs.  (Multiple structs would be used when
 * parts of the source data aren't physically adjacent in memory, or when
 * multiple associated buffers need to be specified.)
 *
 * If buffer is valid then XLOG will check if buffer must be backed up
 * (ie, whether this is first change of that page since last checkpoint).
 * If so, the whole page contents are attached to the XLOG record, and XLOG
 * sets XLR_BKP_BLOCK_X bit in xl_info.  Note that the buffer must be pinned
 * and exclusive-locked by the caller, so that it won't change under us.
 * NB: when the buffer is backed up, we DO NOT insert the data pointed to by
 * this XLogRecData struct into the XLOG record, since we assume it's present
 * in the buffer.  Therefore, rmgr redo routines MUST pay attention to
 * XLR_BKP_BLOCK_X to know what is actually stored in the XLOG record.
 * The i'th XLR_BKP_BLOCK bit corresponds to the i'th distinct buffer
 * value (ignoring InvalidBuffer) appearing in the rdata chain.
 *
 * When buffer is valid, caller must set buffer_std to indicate whether the
 * page uses standard pd_lower/pd_upper header fields.	If this is true, then
 * XLOG is allowed to omit the free space between pd_lower and pd_upper from
 * the backed-up page image.  Note that even when buffer_std is false, the
 * page MUST have an LSN field as its first eight bytes!
 *
 * Note: data can be NULL to indicate no rmgr data associated with this chain
 * entry.  This can be sensible (ie, not a wasted entry) if buffer is valid.
 * The implication is that the buffer has been changed by the operation being
 * logged, and so may need to be backed up, but the change can be redone using
 * only information already present elsewhere in the XLOG entry.
 */
typedef struct XLogRecData
{
	char	   *data;			/* start of rmgr data to include */
	uint32		len;			/* length of rmgr data to include */
	Buffer		buffer;			/* buffer associated with data, if any */
	bool		buffer_std;		/* buffer has standard pd_lower/pd_upper */
	struct XLogRecData *next;	/* next struct in chain, or NULL */
} XLogRecData;

extern TimeLineID ThisTimeLineID;		/* current TLI */
extern bool InRecovery;
extern XLogRecPtr XactLastRecEnd;

/* these variables are GUC parameters related to XLOG */
extern int	CheckPointSegments;
extern int	XLOGbuffers;
extern bool XLogArchiveMode;
extern char *XLogArchiveCommand;
extern int	XLogArchiveTimeout;
extern bool gp_keep_all_xlog;
extern int keep_wal_segments;

extern bool *wal_consistency_checking;
extern char *wal_consistency_checking_string;

extern bool log_checkpoints;

#define XLogArchivingActive()	(XLogArchiveMode)
#define XLogArchiveCommandSet() (XLogArchiveCommand[0] != '\0')

/*
 * Is WAL-logging necessary? We need to log an XLOG record iff either
 * WAL archiving is enabled or XLOG streaming is allowed.
 */

#define XLogIsNeeded() (XLogArchivingActive() || (max_wal_senders > 0))

extern bool am_startup;

#ifdef WAL_DEBUG
extern bool XLOG_DEBUG;
#endif

/*
 * OR-able request flag bits for checkpoints.  The "cause" bits are used only
 * for logging purposes.  Note: the flags must be defined so that it's
 * sensible to OR together request flags arising from different requestors.
 */

/* These directly affect the behavior of CreateCheckPoint and subsidiaries */
#define CHECKPOINT_IS_SHUTDOWN	0x0001	/* Checkpoint is for shutdown */
#define CHECKPOINT_IMMEDIATE	0x0002	/* Do it without delays */
#define CHECKPOINT_FORCE		0x0004	/* Force even if no activity */
/* These are important to RequestCheckpoint */
#define CHECKPOINT_WAIT			0x0008	/* Wait for completion */
/* These indicate the cause of a checkpoint request */
#define CHECKPOINT_CAUSE_XLOG	0x0010	/* XLOG consumption */
#define CHECKPOINT_CAUSE_TIME	0x0020	/* Elapsed time */
/*
 * This falls in two categories, affects behavior of CreateCheckPoint and also
 * indicates request is coming from ResyncManager process to switch primary
 * segment from resync mode to sync mode.
 */
#define CHECKPOINT_RESYNC_TO_INSYNC_TRANSITION 0x0040

/* Checkpoint statistics */
typedef struct CheckpointStatsData
{
	TimestampTz ckpt_start_t;	/* start of checkpoint */
	TimestampTz ckpt_write_t;	/* start of flushing buffers */
	TimestampTz ckpt_sync_t;	/* start of fsyncs */
	TimestampTz ckpt_sync_end_t;	/* end of fsyncs */
	TimestampTz ckpt_end_t;		/* end of checkpoint */

	int			ckpt_bufs_written;		/* # of buffers written */

	int			ckpt_segs_added;	/* # of new xlog segments created */
	int			ckpt_segs_removed;		/* # of xlog segments deleted */
	int			ckpt_segs_recycled;		/* # of xlog segments recycled */
} CheckpointStatsData;

extern CheckpointStatsData CheckpointStats;


extern XLogRecPtr XLogInsert(RmgrId rmid, uint8 info, XLogRecData *rdata);
extern XLogRecPtr XLogInsert_OverrideXid(RmgrId rmid, uint8 info, XLogRecData *rdata, TransactionId overrideXid);
extern XLogRecPtr XLogLastInsertBeginLoc(void);
extern XLogRecPtr XLogLastInsertEndLoc(void);
extern XLogRecPtr XLogLastChangeTrackedLoc(void);
extern uint32 XLogLastInsertTotalLen(void);
extern uint32 XLogLastInsertDataLen(void);
extern void XLogFlush(XLogRecPtr RecPtr);
extern void XLogFileRepFlushCache(
	XLogRecPtr	*lastChangeTrackingEndLoc);

extern void XLogGetLastRemoved(uint32 *log, uint32 *seg);
extern XLogRecPtr XLogSaveBufferForHint(Buffer buffer, Relation relation);

extern void xlog_redo(XLogRecPtr beginLoc __attribute__((unused)), XLogRecPtr lsn __attribute__((unused)), XLogRecord *record);
extern void xlog_desc(StringInfo buf, XLogRecPtr beginLoc, XLogRecord *record);

extern void issue_xlog_fsync(int fd, uint32 log, uint32 seg);
extern void XLogBackgroundFlush(void);
extern void XLogAsyncCommitFlush(void);
extern bool XLogNeedsFlush(XLogRecPtr RecPtr);

extern void XLogSetAsyncCommitLSN(XLogRecPtr record);

extern bool RecoveryInProgress(void);
extern XLogRecPtr GetInsertRecPtr(void);
extern XLogRecPtr GetFlushRecPtr(void);

extern void UpdateControlFile(void);
extern uint64 GetSystemIdentifier(void);
extern bool DataChecksumsEnabled(void);
extern Size XLOGShmemSize(void);
extern void XLOGShmemInit(void);
extern void XLogStartupInit(void);
extern void BootStrapXLOG(void);
extern void StartupXLOG(void);
extern bool XLogStartupMultipleRecoveryPassesNeeded(void);
extern bool XLogStartupIntegrityCheckNeeded(void);
extern bool XLogStartup_DoNextPTCatVerificationIteration(void);
extern void StartupXLOG_Pass2(void);
extern void StartupXLOG_Pass3(void);
extern void StartupXLOG_Pass4(void);
extern void ShutdownXLOG(int code, Datum arg);
extern void InitXLOGAccess(void);
extern void CreateCheckPoint(int flags);
extern void XLogPutNextOid(Oid nextOid);
extern void XLogPutNextRelfilenode(Oid nextRelfilenode);
extern XLogRecPtr GetRedoRecPtr(void);
extern XLogRecPtr GetInsertRecPtr(void);
extern void GetNextXidAndEpoch(TransactionId *xid, uint32 *epoch);

extern void XLogGetRecoveryStart(char *callerStr, char *reasonStr, XLogRecPtr *redoCheckPointLoc, CheckPoint *redoCheckPoint);
extern void XLogPrintLogNames(void);
extern char *XLogLocationToString(XLogRecPtr *loc);
extern char *XLogLocationToString2(XLogRecPtr *loc);
extern char *XLogLocationToString3(XLogRecPtr *loc);
extern char *XLogLocationToString4(XLogRecPtr *loc);
extern char *XLogLocationToString5(XLogRecPtr *loc);
extern char *XLogLocationToString_Long(XLogRecPtr *loc);
extern char *XLogLocationToString2_Long(XLogRecPtr *loc);
extern char *XLogLocationToString3_Long(XLogRecPtr *loc);
extern char *XLogLocationToString4_Long(XLogRecPtr *loc);
extern char *XLogLocationToString5_Long(XLogRecPtr *loc);

extern void HandleStartupProcInterrupts(void);
extern void StartupProcessMain(int passNum);

extern int XLogReconcileEofPrimary(void);

extern int XLogReconcileEofMirror(
					   XLogRecPtr	primaryEof,
					   XLogRecPtr	*mirrorEof);

extern int XLogRecoverMirrorControlFile(void);
extern int XLogAddRecordsToChangeTracking(
	XLogRecPtr	*lastChangeTrackingEndLoc);
extern void XLogInChangeTrackingTransition(void);

extern void xlog_print_redo_read_buffer_not_found(
		RelFileNode		*reln,
		BlockNumber 	blkno,
		XLogRecPtr 		lsn,
		const char		*funcName);
extern void xlog_print_redo_lsn_application(
		RelFileNode		*rnode,
		BlockNumber 	blkno,
		void			*page,
		XLogRecPtr		lsn,
		const char		*funcName);

extern XLogRecord *XLogReadRecord(XLogRecPtr *RecPtr, bool fetching_ckpt, int emode);

extern void XLogCloseReadRecord(void);

extern void XLogReadRecoveryCommandFile(int emode);

extern List *XLogReadTimeLineHistory(TimeLineID targetTLI);

extern int XLogRecoverMirror(void);

extern XLogRecPtr GetStandbyFlushRecPtr(TimeLineID *targetTLI);
extern XLogRecPtr GetXLogReplayRecPtr(TimeLineID *targetTLI);
extern TimeLineID GetRecoveryTargetTLI(void);
extern int XLogFileInitExt(
	uint32 log, uint32 seg,
	bool *use_existent, bool use_lock);

extern bool CheckPromoteSignal(bool do_unlink);
extern void WakeupRecovery(void);
extern void SetStandbyDbid(int16 dbid);
extern int16 GetStandbyDbid(void);
extern bool IsStandbyMode(void);

/*
 * Starting/stopping a base backup
 */
extern XLogRecPtr do_pg_start_backup(const char *backupidstr, bool fast, char **labelfile);
extern XLogRecPtr do_pg_stop_backup(char *labelfile);
extern void do_pg_abort_backup(void);

/* File path names (all relative to $PGDATA) */
#define BACKUP_LABEL_FILE		"backup_label"
#define BACKUP_LABEL_OLD		"backup_label.old"

extern bool
IsBkpBlockApplied(XLogRecord *record, uint8 block_id);

#endif   /* XLOG_H */
