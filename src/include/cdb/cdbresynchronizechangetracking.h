/*-------------------------------------------------------------------------
 *
 * cdbresynchronizechangetracking.h
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbresynchronizechangetracking.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBRESYNCHRONIZECHANGETRACKING_H
#define CDBRESYNCHRONIZECHANGETRACKING_H

#include "access/gist_private.h"
#include "access/xlog.h"
#include "access/xlogdefs.h"
#include "storage/relfilenode.h"
#include "utils/guc.h"
#include "utils/pg_crc.h"

#define CHANGETRACKINGDIR  "pg_changetracking"
#define CHANGETRACKING_STORAGE_VERSION 2
#define CHANGETRACKING_BLCKSZ BLCKSZ
#define	CHANGETRACKING_XLOGDATASZ (128 * 1024)
#define CHANGETRACKING_METABUFLEN 128
#define CHANGETRACKING_COMPACT_THRESHOLD (1 * 1024 * 1024 * 1024) /* 1GB */

/*
 * The LOG file is where all the incremental buffer
 * pool changes are stored. 
 * 
 * The "Full" log includes all the changetracking records that 
 * were accumulated from the xlog. The full log is the first 
 * "landing place" for all the relevant xlog records.
 * 
 * The "Compact" log is a much smaller version and includes 
 * only the necessary change tracking records that are needed for 
 * resync. This is essentially a shrinked down version of all
 * the records that ever landed in the full log.
 * 
 * The "Transient" log is a temporary log file that is used
 * to help with the process of compacting the full and compact 
 * logs. In general, when we want to compact the full or compact
 * log files, we rename them to be "transient" and then do
 * the actual compacting into the new compact file.
 * 
 * The META file is where we store information about
 * if it's a full resync and the last resync lsn, and
 * other meta info.
 * 
 */
typedef enum CTFType
{
	CTF_LOG_FULL = 0,
	CTF_LOG_TRANSIENT,
	CTF_LOG_COMPACT,
	CTF_META
} CTFType;

#define ChangeTrackingFullLogFileName(fname)	\
	snprintf(fname, MAXFNAMELEN, "CT_LOG_FULL")

#define ChangeTrackingFullLogFilePath(path)	\
	snprintf(path, MAXPGPATH, CHANGETRACKINGDIR "/CT_LOG_FULL")

#define ChangeTrackingTransientLogFileName(fname)	\
	snprintf(fname, MAXFNAMELEN, "CT_LOG_TRANSIENT")

#define ChangeTrackingTransientLogFilePath(path)	\
	snprintf(path, MAXPGPATH, CHANGETRACKINGDIR "/CT_LOG_TRANSIENT")

#define ChangeTrackingCompactLogFileName(fname)	\
	snprintf(fname, MAXFNAMELEN, "CT_LOG_COMPACT")

#define ChangeTrackingCompactLogFilePath(path)	\
	snprintf(path, MAXPGPATH, CHANGETRACKINGDIR "/CT_LOG_COMPACT")

#define ChangeTrackingMetaFileName(fname)	\
	snprintf(fname, MAXFNAMELEN, "CT_METADATA")

#define ChangeTrackingMetaFilePath(path)	\
	snprintf(path, MAXPGPATH, CHANGETRACKINGDIR "/CT_METADATA")

/* 
 * these are currently extern for gp_changetracking_log() use 
 * other modules should NOT need to use those. 
 * TODO: make this more secure 
 */
extern File ChangeTracking_OpenFile(CTFType ftype);
extern void ChangeTracking_CloseFile(File file);
extern void ChangeTracking_FsyncDataIntoLog(CTFType ftype);

/*
 * This module is for initializing, recovering, writing, reading, checkpointing, and dropping the
 * mirror resynchronize change log.
 *
 * Multiple backends can write and read the resynchronize change log at the same time.
 */


extern bool ChangeTracking_ShouldTrackChanges(void);

extern bool ChangeTracking_RetrieveLastChangeTrackedLoc(void);

extern bool ChangeTracking_RetrieveIsTransitionToInsync(void);

extern bool ChangeTracking_RetrieveIsTransitionToResync(void);

extern void ChangeTracking_MarkFullResync(void);

extern void ChangeTracking_MarkIncrResync(void);

extern void ChangeTracking_MarkTransitionToResyncCompleted(void);

extern void ChangeTracking_MarkTransitionToInsyncCompleted(void);

extern void ChangeTracking_RecordLastChangeTrackedLoc(void);

// -----------------------------------------------------------------------------
// Creation, Recovery, and Drop
// -----------------------------------------------------------------------------

/*
 * Create the initial portion of the resynchronize change log after mirror loss by processing
 * the XLOG starting at the previous checkpoint.
 *
 * UNDONE: Talk about XLOG stability -- can have XLOG files disappearing or new XLOG writes
 * UNDONE: occurring since they would intermix and we can't handle that..
 */
extern void ChangeTracking_CreateInitialFromPreviousCheckpoint(
	XLogRecPtr	*lastChangeTrackingEndLoc);

/*
 * Make a copy of the rdata data buffers so that we use it 
 * in case xlog changes it later on.
 */
extern char* ChangeTracking_CopyRdataBuffers(XLogRecData*	rdata, 
											  RmgrId		rmid,
											  uint8			info,
											  bool*			iscopy);

/*
 * Send a single xlog record into the changetracker to examine it and add
 * the relevant information from it into the changetracker if desired.
 */
extern void ChangeTracking_AddRecordFromXlog(RmgrId rmid, 
											 uint8 info, 
											 void*  data,
											 XLogRecPtr*	loc);

// -----------------------------------------------------------------------------
// Various log compacting related routines
// -----------------------------------------------------------------------------

extern int ChangeTracking_CompactLogFile(CTFType source, CTFType dest, XLogRecPtr*	uptolsn);
extern bool ChangeTracking_doesFileNeedCompacting(CTFType ftype);
extern void ChangeTracking_CompactLogsIfPossible(void);
extern void ChangeTracking_DoFullCompactingRound(XLogRecPtr* upto_lsn);
extern bool ChangeTrackingIsCompactingInProgress(void);

/*
 * The information needed for a change tracking record to be
 * stored.
 */
typedef struct RelationChangeInfo
{
	RelFileNode 	relFileNode;
	BlockNumber 	blockNumber;
	ItemPointerData persistentTid;
	int64	  		persistentSerialNum;
} RelationChangeInfo;

extern void ChangeTracking_GetRelationChangeInfoFromXlog(
									  RmgrId	xl_rmid,
									  uint8 	xl_info,
									  void		*data, 
									  RelationChangeInfo	*relationChangeInfoArray,
									  int					*relationChangeInfoArrayCount,
									  int					relationChangeInfoMaxSize);

extern bool ChangeTracking_PrintRelationChangeInfo(
									  RmgrId	xl_rmid,
									  uint8		xl_info,
									  void		*data, 
									  XLogRecPtr *loc,
									  bool	  weAreGeneratingXLogNow,
									  bool	  printSkipIssuesOnly);

extern int ChangeTracking_GetInfoArrayDesiredMaxLength(RmgrId rmid, uint8 info);

/**
 * Get the total amount of space, in bytes, used by the changetracking information.
 */
extern int64 ChangeTracking_GetTotalSpaceUsedOnDisk(void);

extern bool ChangeTracking_DoesFileExist(CTFType ftype);

extern char *ChangeTracking_FtypeToString(CTFType ftype);

/*
 * Drop the resynchronize change log and meta files.
 */
extern void ChangeTracking_DropAll(void);

// -----------------------------------------------------------------------------
// Add 
// -----------------------------------------------------------------------------

							
/*
 * Add notification of an initial bulk change that was not made to the mirror.
 */
extern void ChangeTracking_AddInitialBulkChange(
	XLogRecPtr		xlogLocation,	/* The XLOG LSN of the record that describes the page change. 			*/
	RelFileNode 	*relFileNode);	/* The tablespace, database, and relation OIDs for the changed relation	*/
				
				
// -----------------------------------------------------------------------------
// Reading 
// -----------------------------------------------------------------------------

		
/*
 * This is the format of a change log record on disk.
 * These records are stored in the LOG file.
 */
typedef struct ChangeTrackingRecord
{
	XLogRecPtr		xlogLocation;
	RelFileNode 	relFileNode; 			/* The tablespace, database, and relation OIDs for the changed relation. */
	uint32			bufferPoolBlockNum; 	/* BufferPool */
	ItemPointerData persistentTid;
	int64 			persistentSerialNum;
} ChangeTrackingRecord;

/*
 * The log record block header describes the version and number of 
 * records stored in it. for now it's primitive and not so compact. 
 * We could probably keep it 8 bytes in side if we add another small 
 * field or 2.
 */
typedef struct ChangeTrackingPageHeader
{
	uint32		blockversion;		/* version == CHANGETRACKING_STORAGE_VERSION. increment if we change the block layout in the future */
	uint32		numrecords;			/* number of records stored in the current block */
	pg_crc32	checksum;			/* checksum of the current block */
	
} ChangeTrackingPageHeader;

/*
 * This is the format of a meta data record on disk.
 * These records are stored in the META file.
 */
typedef struct ChangeTrackingMetaRecord
{
	XLogRecPtr		resync_lsn_end;	  /* endResyncLSN                         */
	bool			resync_mode_full; /* marks that resync FULL was requested */
	bool			resync_transition_completed; /* transition to resync mode completed */
	bool			insync_transition_completed; /* transition to insync mode completed */

} ChangeTrackingMetaRecord;

// -----------------------------------------------------------------------------
// Checkpointing and Shutting Down
// -----------------------------------------------------------------------------
				
/*
 * Flush the reschronize change log to disk for checkpoint.
 */
extern void CheckPointChangeTracking(void);
												

// -----------------------------------------------------------------------------
// Get incremental change entries for gp_persistent relation
// -----------------------------------------------------------------------------

typedef struct IncrementalChangeEntry
{
	ItemPointerData persistentTid;
	int64 			persistentSerialNum;
	int64			numblocks;

} IncrementalChangeEntry;

typedef struct IncrementalChangeList
{
	IncrementalChangeEntry* entries;
	int						count;

} IncrementalChangeList;

IncrementalChangeList* ChangeTracking_InitIncrementalChangeList(int count);


IncrementalChangeList* ChangeTracking_GetIncrementalChangeList(void);
void ChangeTracking_FreeIncrementalChangeList(IncrementalChangeList* iclist);

// -----------------------------------------------------------------------------
// Consuming required information from change log
// -----------------------------------------------------------------------------

typedef struct ChangeTrackingRequestEntry
{
	/* Changed relation. */	
	RelFileNode 	relFileNode;
	/*
	 * Block number of the last block fetched from CT log.  This is used only
	 * when the number of changed blocks for a relation is greater than
	 * gp_filerep_ct_batch_size.  The changed blocks are obtained in fixed
	 * sized batche.  The last_fetched block number determines where to start
	 * the next batch from.
	 */
	BlockNumber		last_fetched;
} ChangeTrackingRequestEntry;

typedef struct ChangeTrackingRequest
{
	ChangeTrackingRequestEntry* entries;
	int							count;
	int							max_count;
	
} ChangeTrackingRequest;

/*
 * Changed block of a buffer pooled relation (heap).  All the information in
 * this object is obtained from CT log.
 */
typedef struct ChangeTrackingResultEntry
{
	RelFileNode 	relFileNode;
	BlockNumber		block_num;
	/*
	 * Most recent location in XLOG for a change made to this block while a
	 * primary segment was tracking changes.  If a new change happens to the
	 * block after primary segment has transitioned from changetracking to
	 * resync, the block's LSN value will be newer than lsn_end.  If so, the
	 * block will be mirrored already at the time the change was written on
	 * primary and doesn't need to be shipped to mirror during resync.
	 *
	 * In theory, a single global "last change-tracked LSN"
	 * (XLogLastChangeTrackedLoc() or fileRepResyncShmem->endIncrResyncLSN)
	 * should be suffice to decide whether a page should be sent to mirror
	 * during resync or not.  Keeping track of lsn_end for each block of every
	 * relation seems redundant.  We leave this as TODO.
	 */
	XLogRecPtr		lsn_end;

} ChangeTrackingResultEntry;

typedef struct ChangeTrackingResult
{
	ChangeTrackingResultEntry*  entries;
	int							count;
	int							max_count;
	bool						ask_for_more;		/* there are more results for this rel. ask for them */
} ChangeTrackingResult;

extern ChangeTrackingRequest* ChangeTracking_FormRequest(int count);
extern void ChangeTracking_AddRequestEntry(ChangeTrackingRequest* request, 
										   RelFileNode		relFileNode);

extern ChangeTrackingResult* ChangeTracking_GetChanges(ChangeTrackingRequest* request);
extern void ChangeTracking_FreeRequest(ChangeTrackingRequest* request);
extern void ChangeTracking_FreeResult(ChangeTrackingResult* result);

extern bool ChangeTracking_GetLastChangeTrackingLogEndLoc(XLogRecPtr *lastChangeTrackingLogEndLoc);

// -----------------------------------------------------------------------------
// Shmem
// -----------------------------------------------------------------------------

typedef struct ChangeTrackingBufStatusData
{
	uint32			recordcount;  	/* Total change log records in buffer currently 	*/
	uint32			maxbufsize;		/* buffer capacity                              	*/
	uint32			bufsize;		/* Current buffer size                          	*/
	uint32			fileseg;		/* The change log file segment we are writing into	*/
	int64			nextwritepos;   /* The next file position to append buffer data to  */
	
} ChangeTrackingBufStatusData;

/* save resync metadata info in shmem and write to meta file when needed */
typedef struct ChangeTrackingResyncMetaData
{
	XLogRecPtr		resync_lsn_end;	  /* endResyncLSN                         */
	bool			resync_mode_full; /* marks that resync FULL was requested */	
	bool			resync_transition_completed; /* transition to resync mode completed */
	bool			insync_transition_completed; /* transition to resync mode completed */
	
} ChangeTrackingResyncMetaData;

typedef struct ChangeTrackingLogCompactingStateData
{
	uint32			ctfull_bufs_added;    /* num bufs added to LOG_FULL since last time ran compacting */
	uint32			ctcompact_bufs_added; /* num bufs added to LOG_COMPACT since last time ran compacting */
	bool			in_progress;
	XLogRecPtr		xlog_end_location;	
		/* 
		 * Populated during crash recovery 
		 * if Change Tracking log lsn is higher than XLOG lsn. 
		 * All records with higher lsn will be discarded from Change Tracking log file.
		 */
	
} ChangeTrackingLogCompactingStateData;

/*
 * Return the required shared-memory size for this module.
 */
extern Size ChangeTrackingShmemSize(void);
								
/*
 * Initialize the shared-memory for this module.
 */
extern void ChangeTrackingShmemInit(void);

/*
 * Reset shmem variables to zero.
 */
extern void ChangeTrackingShmemReset(void);

extern void ChangeTrackingSetXLogEndLocation(XLogRecPtr upto_lsn);

extern void ChangeTracking_DoFullCompactingRoundIfNeeded(void);

extern void ChangeTracking_CreateTransientLog(void);

#endif   /* CDBRESYNCHRONIZECHANGETRACKING_H */

