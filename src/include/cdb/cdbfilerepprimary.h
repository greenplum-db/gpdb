/*
 *  cdbfilerepprimary.h
 *  
 *
 *  Copyright 2009-2010, Greenplum Inc. All rights reserved.
 *
 */

#ifndef CDBFILEREPPRIMARY_H
#define CDBFILEREPPRIMARY_H

#include "access/filerepdefs.h"
#include "access/persistentfilesysobjname.h"
#include "cdb/cdbfilerep.h"



/*
 * Responsibilities of this module!!!
 *
 * Construct Message
 * Send Messages to Mirror
 */

extern bool FileRepPrimary_IsResyncWorker(void);
extern bool FileRepPrimary_IsResyncManagerOrWorker(void);

extern MirrorDataLossTrackingState 
FileRepPrimary_GetMirrorDataLossTrackingSessionNum(int64 *sessionNum);

extern bool FileRepPrimary_IsMirrorDataLossOccurred(void);

extern int FileRepPrimary_IntentAppendOnlyCommitWork(void);

extern int FileRepPrimary_FinishedAppendOnlyCommitWork(int count);

extern int FileRepPrimary_GetAppendOnlyCommitWorkCount(void);

// -----------------------------------------------------------------------------
// Construct Messages for Open, Flush, Close, and Write 
// -----------------------------------------------------------------------------

/*
 * Open
 *		Construct Message Header for open operation
 *		Construct Message Header CRC
 *		Insert Message into Mirror Messages Shared Memory
 *
 * Output
 *		The routine returns status (error code or ok) 
 */
extern int FileRepPrimary_MirrorOpen(
	FileRepIdentifier_u	fileRepIdentifier,
		/* */
		
	FileRepRelationType_e fileRepRelationType,
		/* */
	int64				logicalEof,
		/* it is used only for Append Only */							 
	int					fileFlags,

	int					fileMode,

	bool				suppressError);

/*
 * Flush
 *		Construct Message Header for synchronous flush operation
 *		Construct Message Header CRC
 *		Insert Message into Mirror Messages Shared Memory
 *
 * Output
 *		The routine returns status (error code or ok) 
 */
extern int FileRepPrimary_MirrorFlush(
	FileRepIdentifier_u	fileRepIdentifier,
		/* */
									  
	FileRepRelationType_e fileRepRelationType);
		/* */

/*
 * Close
 *		Construct Message Header for close operation
 *		Construct Message Header CRC
 *		Insert Message into Mirror Messages Shared Memory
 *
 * Output
 *		The routine returns status (error code or ok) 
 */
extern int FileRepPrimary_MirrorClose(
	  FileRepIdentifier_u	fileRepIdentifier,
		/* */
	  
	  FileRepRelationType_e fileRepRelationType);
		/* */
/*
 * Flush And Close
 *		Construct Message Header for flush and close operation
 *		Construct Message Header CRC
 *		Insert Message into Mirror Messages Shared Memory
 *
 * Output
 *		The routine returns status (error code or ok) 
 */
extern int FileRepPrimary_MirrorFlushAndClose(
					  FileRepIdentifier_u	fileRepIdentifier,
						/* */
					  
					  FileRepRelationType_e fileRepRelationType);
						/* */

/*
 * Truncate
 *		Construct Message Header for flush and close operation
 *		Construct Message Header CRC
 *		Insert Message into Mirror Messages Shared Memory
 *
 * Output
 *		The routine returns status (error code or ok) 
 */
extern int FileRepPrimary_MirrorTruncate(
			  FileRepIdentifier_u	fileRepIdentifier,
				/* */
			  
			  FileRepRelationType_e fileRepRelationType,
				/* */

			  int64 position);
				/* */

/*
 * Reconcile
 *
 */
extern int FileRepPrimary_ReconcileXLogEof(
				FileRepIdentifier_u		fileRepIdentifier,
									   
									   
				FileRepRelationType_e	fileRepRelationType,
									   
									   
				XLogRecPtr				primaryXLogEof);

/*
 * Rename
 */
extern int FileRepPrimary_MirrorRename(
								FileRepIdentifier_u		oldFileRepIdentifier,
								FileRepIdentifier_u		newFileRepIdentifier,
								FileRepRelationType_e	fileRepRelationType);

/*
 * Write
 *		Construct Message Header for write operation
 *		Issue memcpy() to construct Message Body
 *		Calculate CRC for message body
 *		Calculate CRC for message header
 *		Insert Message into Mirror Messages Shared Memory
 *
 * Output
 *		The routine returns status (error code or ok) 
 */
extern int FileRepPrimary_MirrorWrite(
	FileRepIdentifier_u	fileRepIdentifier,
		/* */
		
    FileRepRelationType_e	fileRepRelationType,
		/* */
									  
    int32					offset,
		/* */
									  
    char					*data, 
		/* */
									  
    uint32					dataLength,
		/* */
									  
    XLogRecPtr				lsn);
		/* */

// -----------------------------------------------------------------------------
// Construct Messages for Create and drop of file and dir
// -----------------------------------------------------------------------------

/*
 * Create
 *		Construct Message Header for create operation (file or dir)
 *		Construct Message Header CRC
 *		Insert Message into Mirror Messages Shared Memory
 *
 * Output
 *		The routine returns status (error code or ok) 
 */
extern int FileRepPrimary_MirrorCreate(
	FileRepIdentifier_u	fileRepIdentifier,
		/* file or dir to be created */
									 
	FileRepRelationType_e fileRepRelationType,
		/* FileRepRelationTypeFile or FileRepRelationTypeDir */

	bool reportStatus);
		/* Report Status to Primary, error on mirror is not reported as Fault. */

/*
 * Drop
 *		Construct Message Header for drop operation (file or dir)
 *		Construct Message Header CRC
 *		Insert Message into Mirror Messages Shared Memory
 *
 * Output
 *		The routine returns status (error code or ok) 
 */

extern int FileRepPrimary_MirrorDrop(
	FileRepIdentifier_u	fileRepIdentifier,
		/* file or dir to be removed */
									 
	FileRepRelationType_e fileRepRelationType);								 
		/* FileRepRelationTypeFile or FileRepRelationTypeDir */

extern int FileRepPrimary_MirrorDropFilesFromDir(
	FileRepIdentifier_u	fileRepIdentifier,
		/* file or dir to be removed */
									 
	FileRepRelationType_e fileRepRelationType);								 
		/* FileRepRelationTypeFile or FileRepRelationTypeDir */

extern int FileRepPrimary_MirrorDropTemporaryFiles(
												   FileRepIdentifier_u		fileRepIdentifier,
												   FileRepRelationType_e	fileRepRelationType);

extern int FileRepPrimary_MirrorValidation(
		FileRepIdentifier_u		fileRepIdentifier,
				   
		FileRepRelationType_e	fileRepRelationType,

		FileRepValidationType_e fileRepValidationType);

extern bool FileRepPrimary_IsOperationCompleted(
	FileRepIdentifier_u	fileRepIdentifier,
		/* identifier of the operation */
				 
	FileRepRelationType_e fileRepRelationType);								 
		/* type on which operation was performed */

extern XLogRecPtr FileRepPrimary_GetMirrorXLogEof(void);

extern int FileRepPrimary_GetMirrorStatus(void);

/*
 * Inform mirror that gracefull shutdown is performing o primary
 */
extern void FileRepPrimary_MirrorShutdown(void);

/*
 * Transition mirror from InResync to InSync dataState
 */
extern void FileRepPrimary_MirrorInSyncTransition(void);

/*
 *	Verify that flow from primary to mirror and back to primary is alive
 */
extern void FileRepPrimary_MirrorHeartBeat(FileRepConsumerProcIndex_e index);

extern int FileRepPrimary_MirrorVerify(
									   FileRepIdentifier_u				fileRepIdentifier,
									   FileRepRelationType_e			fileRepRelationType,
									   FileRepOperationDescription_u	fileRepOperationDescription,
									   char								*data, 
									   uint32							dataLength,
									   void								**responseData, 
									   uint32							*responseDataLength, 
									   FileRepOperationDescription_u	*responseDesc);


extern int FileRepPrimary_MirrorStartChecksum(FileRepIdentifier_u fileRepIdentifier);

extern int FileRepPrimary_MirrorVerifyDirectoryChecksum(
	FileRepIdentifier_u fileRepIdentifier,
	char *md);

// -----------------------------------------------------------------------------
// SENDER THREAD
// -----------------------------------------------------------------------------

/*
 *
 */
extern void FileRepPrimary_StartSender(void);

#endif   /* CDBFILEREPPRIMARY_H */

