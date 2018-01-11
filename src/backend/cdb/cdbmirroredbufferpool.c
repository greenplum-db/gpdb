/*-------------------------------------------------------------------------
 *
 * cdbmirroredbufferpool.c
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbmirroredbufferpool.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <signal.h>

#include "access/xlogmm.h"
#include "catalog/pg_tablespace.h"
#include "cdb/cdbfilerepprimary.h"
#include "cdb/cdbmirroredbufferpool.h"
#include "cdb/cdbfilerepresyncmanager.h"
#include "cdb/cdbfilerepresyncworker.h"
#include "cdb/cdbfilerepprimary.h"
#include "cdb/cdbpersistenttablespace.h"
#include "cdb/cdbpersistentstore.h"
#include "storage/bufmgr.h"
#include "storage/fd.h"
#include "storage/smgr.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/palloc.h"
#include "cdb/cdbpersistentfilesysobj.h"
#include "cdb/cdbpersistentrelation.h"

static int32 writeBufLen = 0;
static char *writeBuf = NULL;

static void
MirroredBufferPool_SetUpMirrorAccess(
									 RelFileNode *relFileNode,
 /* The tablespace, database, and relation OIDs for the open. */

									 int32 segmentFileNum,

									 char *relationName,
 /* For tracing only.  Can be NULL in some execution paths. */

									 MirrorDataLossTrackingState mirrorDataLossTrackingState,

									 int64 mirrorDataLossTrackingSessionNum,

									 bool primaryOnly,

									 bool mirrorOnly,

									 StorageManagerMirrorMode *mirrorMode,

									 bool *mirrorDataLossOccurred)
{
	*mirrorMode = StorageManagerMirrorMode_None;
	*mirrorDataLossOccurred = false;
	/* Assume. */

	if (gp_initdb_mirrored)
	{
		/* Kludge for initdb */
		*mirrorMode = StorageManagerMirrorMode_Both;
	}
	else
	{
		switch (mirrorDataLossTrackingState)
		{
			case MirrorDataLossTrackingState_MirrorNotConfigured:
				if (mirrorOnly)
					elog(ERROR, "No mirror configured for mirror only");

				*mirrorMode = StorageManagerMirrorMode_PrimaryOnly;
				break;

			case MirrorDataLossTrackingState_MirrorCurrentlyUpInSync:
				if (primaryOnly)
					*mirrorMode = StorageManagerMirrorMode_PrimaryOnly;
				else if (!mirrorOnly)
					*mirrorMode = StorageManagerMirrorMode_Both;
				else
					*mirrorMode = StorageManagerMirrorMode_MirrorOnly;
				break;

			case MirrorDataLossTrackingState_MirrorCurrentlyUpInResync:
				if (primaryOnly)
					*mirrorMode = StorageManagerMirrorMode_PrimaryOnly;
				else if (!mirrorOnly)
					*mirrorMode = StorageManagerMirrorMode_Both;
				else
					*mirrorMode = StorageManagerMirrorMode_MirrorOnly;
				break;

			case MirrorDataLossTrackingState_MirrorDown:
				if (!mirrorOnly)
					*mirrorMode = StorageManagerMirrorMode_PrimaryOnly;
				else
					*mirrorMode = StorageManagerMirrorMode_MirrorOnly;
				/* Mirror only operations fails from the outset. */

				*mirrorDataLossOccurred = true;
				/* Mirror communication is down. */
				break;

			default:
				elog(ERROR, "Unexpected mirror data loss tracking state: %d",
					 mirrorDataLossTrackingState);
				*mirrorMode = StorageManagerMirrorMode_None;
				/* A happy optimizer is the sound of one hand clapping. */
		}
	}

	if (Debug_persistent_print)
	{
		SUPPRESS_ERRCONTEXT_DECLARE;

		SUPPRESS_ERRCONTEXT_PUSH();

		elog(Persistent_DebugPrintLevel(),
			 "MirroredBufferPool_SetUpMirrorAccess %u/%u/%u, segment file #%d, relation name '%s': primaryOnly %s, mirrorOnly %s, mirror mode %s, mirror data loss occurred %s",
			 relFileNode->spcNode,
			 relFileNode->dbNode,
			 relFileNode->relNode,
			 segmentFileNum,
			 (relationName == NULL ? "<null>" : relationName),
			 (primaryOnly ? "true" : "false"),
			 (mirrorOnly ? "true" : "false"),
			 StorageManagerMirrorMode_Name(*mirrorMode),
			 (*mirrorDataLossOccurred ? "true" : "false"));

		SUPPRESS_ERRCONTEXT_POP();
	}

}

/*
 * The Background writer (for example) might have an open in the primary when the mirror was down.
 * Later when the mirror comes up we need to recognize that and send new writes there....
 */
static void
MirroredBufferPool_RecheckMirrorAccess(
									   MirroredBufferPoolOpen *open)
 /* The resulting open struct. */
{
	MirrorDataLossTrackingState mirrorDataLossTrackingState;
	int64		mirrorDataLossTrackingSessionNum;

	/*
	 * Make this call while under the MirroredLock (unless we are a resync
	 * worker).
	 */
	mirrorDataLossTrackingState =
		FileRepPrimary_GetMirrorDataLossTrackingSessionNum(
														   &mirrorDataLossTrackingSessionNum);
	MirroredBufferPool_SetUpMirrorAccess(
										 &open->relFileNode,
										 open->segmentFileNum,
										  /* relationName, don't know the name here */ NULL,
										 mirrorDataLossTrackingState,
										 mirrorDataLossTrackingSessionNum,
										  /* primaryOnly */ false,
										 open->mirrorOnly,
										 &open->mirrorMode,
										 &open->mirrorDataLossOccurred);
	/*---------------------------------------------------------------------
	 * mirror filespace location has to be populated for
	 *			a) adding mirror with filespaces
	 *			b) resynchronization with filespaces and full copy to new location
	 *---------------------------------------------------------------------
	 */
	if (open->relFileNode.spcNode != GLOBALTABLESPACE_OID &&
		open->relFileNode.spcNode != DEFAULTTABLESPACE_OID &&
		strcmp(open->mirrorFilespaceLocation, "") == 0)
	{
		if (mirrorDataLossTrackingState == MirrorDataLossTrackingState_MirrorCurrentlyUpInSync ||
			mirrorDataLossTrackingState == MirrorDataLossTrackingState_MirrorCurrentlyUpInResync)
		{
			char	   *primaryFilespaceLocation;
			char	   *mirrorFilespaceLocation;

			PersistentTablespace_GetPrimaryAndMirrorFilespaces(
															   open->relFileNode.spcNode,
															   &primaryFilespaceLocation,
															   &mirrorFilespaceLocation);

			if (mirrorFilespaceLocation != NULL)
			{
				sprintf(open->mirrorFilespaceLocation, "%s", mirrorFilespaceLocation);

				{
					char		tmpBuf[FILEREP_MAX_LOG_DESCRIPTION_LEN];

					snprintf(tmpBuf, sizeof(tmpBuf),
							 "recheck mirror access, identifier '%s/%u/%u/%u' ",
							 open->mirrorFilespaceLocation,
							 open->relFileNode.spcNode,
							 open->relFileNode.dbNode,
							 open->relFileNode.relNode);

					FileRep_InsertConfigLogEntry(tmpBuf);
				}
			}

			if (primaryFilespaceLocation != NULL)
				pfree(primaryFilespaceLocation);

			if (mirrorFilespaceLocation != NULL)
				pfree(mirrorFilespaceLocation);
		}
	}
}

/*
 * Open a relation for mirrored write.
 */
static void
MirroredBufferPool_DoOpen(
						  MirroredBufferPoolOpen *open,
 /* The resulting open struct. */

						  RelFileNode *relFileNode,
 /* The tablespace, database, and relation OIDs for the open. */

						  uint32 segmentFileNum,
 /* Which segment file. */

						  char *relationName,
 /* For tracing only.  Can be NULL in some execution paths. */

						  MirrorDataLossTrackingState mirrorDataLossTrackingState,

						  int64 mirrorDataLossTrackingSessionNum,

						  bool create,

						  bool mirrorOnly,

						  bool copyToMirror,

						  int *primaryError,

						  bool *mirrorDataLossOccurred)
{
	int			fileFlags = O_RDWR | PG_BINARY;
	int			fileMode = 0600;

	/*
	 * File mode is S_IRUSR 00400 user has read permission + S_IWUSR 00200
	 * user has write permission
	 */

	char	   *primaryFilespaceLocation = NULL;
	char	   *mirrorFilespaceLocation = NULL;

	Assert(open != NULL);

	*primaryError = 0;
	*mirrorDataLossOccurred = false;

	/*
	 * Using O_EXCL to make it atomic operation to avoid overwriting an
	 * existing file.
	 */
	if (create)
		fileFlags = O_CREAT | O_RDWR | O_EXCL | PG_BINARY;

	PersistentTablespace_GetPrimaryAndMirrorFilespaces(
													   relFileNode->spcNode,
													   &primaryFilespaceLocation,
													   &mirrorFilespaceLocation);

	if (Debug_persistent_print
		&&
		(create || mirrorOnly || copyToMirror))
	{
		SUPPRESS_ERRCONTEXT_DECLARE;

		SUPPRESS_ERRCONTEXT_PUSH();

		elog(Persistent_DebugPrintLevel(),
			 "MirroredBufferPool_DoOpen: Special open %u/%u/%u --> create %s, mirrorOnly %s, copyToMirror %s, "
			 "primary filespace location %s "
			 "mirror filespace location %s ",
			 relFileNode->spcNode,
			 relFileNode->dbNode,
			 relFileNode->relNode,
			 (create ? "true" : "false"),
			 (mirrorOnly ? "true" : "false"),
			 (copyToMirror ? "true" : "false"),
			 (primaryFilespaceLocation == NULL) ? "<null>" : primaryFilespaceLocation,
			 (mirrorFilespaceLocation == NULL) ? "<null>" : mirrorFilespaceLocation);

		SUPPRESS_ERRCONTEXT_POP();
	}

	MemSet(open, 0, sizeof(MirroredBufferPoolOpen));
	open->primaryFile = -1;

	if (mirrorFilespaceLocation == NULL)
		sprintf(open->mirrorFilespaceLocation, "%s", "");
	else
		sprintf(open->mirrorFilespaceLocation, "%s", mirrorFilespaceLocation);

	open->relFileNode = *relFileNode;
	open->segmentFileNum = segmentFileNum;

	open->create = create;
	open->mirrorOnly = mirrorOnly;
	open->copyToMirror = copyToMirror;

	MirroredBufferPool_SetUpMirrorAccess(
										 relFileNode,
										 segmentFileNum,
										 relationName,
										 mirrorDataLossTrackingState,
										 mirrorDataLossTrackingSessionNum,
										  /* primaryOnly */ false,
										 mirrorOnly,
										 &open->mirrorMode,
										 &open->mirrorDataLossOccurred);

	if (StorageManagerMirrorMode_DoPrimaryWork(open->mirrorMode))
	{
		char	   *dbPath;
		char	   *path;

		dbPath = (char *) palloc(MAXPGPATH + 1);
		path = (char *) palloc(MAXPGPATH + 1);

		/*
		 * Do the primary work first so we don't leave files on the mirror or
		 * have an open to clean up.
		 */

		FormDatabasePath(
						 dbPath,
						 primaryFilespaceLocation,
						 relFileNode->spcNode,
						 relFileNode->dbNode);

		if (segmentFileNum == 0)
			sprintf(path, "%s/%u", dbPath, relFileNode->relNode);
		else
			sprintf(path, "%s/%u.%u", dbPath, relFileNode->relNode, segmentFileNum);

		errno = 0;

		open->primaryFile = PathNameOpenFile(path, fileFlags, fileMode);

		if (open->primaryFile < 0)
		{
			*primaryError = errno;
		}

		pfree(dbPath);
		pfree(path);
	}

	if (StorageManagerMirrorMode_SendToMirror(open->mirrorMode) &&
		*primaryError == 0 &&
		!open->mirrorDataLossOccurred)
	{
		if (FileRepPrimary_MirrorOpen(
									  FileRep_GetRelationIdentifier(
																	open->mirrorFilespaceLocation,
																	open->relFileNode,
																	open->segmentFileNum),
									  FileRepRelationTypeBufferPool,
									  FILEREP_OFFSET_UNDEFINED,
									  fileFlags,
									  fileMode,
									  TRUE /* supressError */ ) != 0)
		{
			if (Debug_filerep_print)
				ereport(LOG,
						(errmsg("could not sent file open request to mirror "),
						 FileRep_ReportRelationPath(
													open->mirrorFilespaceLocation,
													open->relFileNode,
													open->segmentFileNum)));
		}

		open->mirrorDataLossOccurred = FileRepPrimary_IsMirrorDataLossOccurred();

	}

	if (*primaryError != 0)
	{
		open->isActive = false;
	}
	else if (StorageManagerMirrorMode_DoPrimaryWork(open->mirrorMode))
	{
		open->isActive = true;
	}
	else if (StorageManagerMirrorMode_SendToMirror(open->mirrorMode) &&
			 !open->mirrorDataLossOccurred)
	{
		open->isActive = true;
	}

	*mirrorDataLossOccurred = open->mirrorDataLossOccurred;

	if (primaryFilespaceLocation != NULL)
		pfree(primaryFilespaceLocation);

	if (mirrorFilespaceLocation != NULL)
		pfree(mirrorFilespaceLocation);
}

void
MirroredBufferPool_Open(
						MirroredBufferPoolOpen *open,
 /* The resulting open struct. */

						RelFileNode *relFileNode,
 /* The tablespace, database, and relation OIDs for the open. */

						uint32 segmentFileNum,
 /* Which segment file. */

						char *relationName,
 /* For tracing only.  Can be NULL in some execution paths. */

						int *primaryError,

						bool *mirrorDataLossOccurred)

{
	MIRROREDLOCK_BUFMGR_DECLARE;

	MirrorDataLossTrackingState mirrorDataLossTrackingState;
	int64		mirrorDataLossTrackingSessionNum;

	/* -------- MirroredLock ---------- */
	MIRROREDLOCK_BUFMGR_LOCK;

	/*
	 * Make this call while under the MirroredLock (unless we are a resync
	 * worker).
	 */
	mirrorDataLossTrackingState =
		FileRepPrimary_GetMirrorDataLossTrackingSessionNum(
														   &mirrorDataLossTrackingSessionNum);

	MirroredBufferPool_DoOpen(
							  open,
							  relFileNode,
							  segmentFileNum,
							  relationName,
							  mirrorDataLossTrackingState,
							  mirrorDataLossTrackingSessionNum,
							   /* create */ false,
							   /* mirrorOnly */ false,
							   /* copyToMirror */ FileRepResyncWorker_IsResyncRequest(),
							  primaryError,
							  mirrorDataLossOccurred);

	MIRROREDLOCK_BUFMGR_UNLOCK;
	/* -------- MirroredLock ---------- */
}

void
MirroredBufferPool_Create(
						  MirroredBufferPoolOpen *open,
 /* The resulting open struct. */

						  RelFileNode *relFileNode,
 /* The tablespace, database, and relation OIDs for the open. */

						  uint32 segmentFileNum,
 /* Which segment file. */

						  char *relationName,
 /* For tracing only.  Can be NULL in some execution paths. */

						  MirrorDataLossTrackingState mirrorDataLossTrackingState,

						  int64 mirrorDataLossTrackingSessionNum,

						  int *primaryError,

						  bool *mirrorDataLossOccurred)

{
	MIRROREDLOCK_BUFMGR_DECLARE;

	*primaryError = 0;
	*mirrorDataLossOccurred = false;

	/* -------- MirroredLock ---------- */
	MIRROREDLOCK_BUFMGR_LOCK;

	MirroredBufferPool_DoOpen(
							  open,
							  relFileNode,
							  segmentFileNum,
							  relationName,
							  mirrorDataLossTrackingState,
							  mirrorDataLossTrackingSessionNum,
							   /* create */ true,
							   /* mirrorOnly */ false,
							   /* copyToMirror */ false,
							  primaryError,
							  mirrorDataLossOccurred);

	MIRROREDLOCK_BUFMGR_UNLOCK;
	/* -------- MirroredLock ---------- */
}

void
MirroredBufferPool_MirrorReCreate(
								  RelFileNode *relFileNode,
 /* The tablespace, database, and relation OIDs for the open. */

								  uint32 segmentFileNum,
 /* Which segment file. */

								  MirrorDataLossTrackingState mirrorDataLossTrackingState,

								  int64 mirrorDataLossTrackingSessionNum,

								  bool *mirrorDataLossOccurred)
{
	MirroredBufferPoolOpen mirroredOpen;

	int			primaryError;

	MirroredBufferPool_DoOpen(
							  &mirroredOpen,
							  relFileNode,
							  segmentFileNum,
							   /* relationName, don't know here */ NULL,
							  mirrorDataLossTrackingState,
							  mirrorDataLossTrackingSessionNum,
							   /* create */ true,
							   /* mirrorOnly */ true,
							   /* copyToMirror */ false,
							  &primaryError,
							  mirrorDataLossOccurred);
	Assert(primaryError == 0);
	/* No primary work. */

	if (!*mirrorDataLossOccurred)
	{
		Assert(MirroredBufferPool_IsActive(&mirroredOpen));
		MirroredBufferPool_Close(&mirroredOpen);
	}
	else
		Assert(!MirroredBufferPool_IsActive(&mirroredOpen));
}

bool
MirroredBufferPool_IsActive(
							MirroredBufferPoolOpen *open)
 /* The open struct. */
{
	return open->isActive;
}

/*
 * Flush a flat file.
 *
 */
bool
MirroredBufferPool_Flush(
						 MirroredBufferPoolOpen *open)
 /* The open struct. */

{
	int			primaryError;

	Assert(open != NULL);
	Assert(open->isActive);

	primaryError = 0;

	/*
	 * For Buffer Pool managed, we are normally not session oriented like
	 * Append-Only.
	 *
	 * Figure out mirroring each time...
	 */
	MirroredBufferPool_RecheckMirrorAccess(open);

	if (StorageManagerMirrorMode_SendToMirror(open->mirrorMode) &&
		!open->mirrorDataLossOccurred)
	{
		if (FileRepPrimary_MirrorFlush(
									   FileRep_GetRelationIdentifier(
																	 open->mirrorFilespaceLocation,
																	 open->relFileNode,
																	 open->segmentFileNum),
									   FileRepRelationTypeBufferPool) != 0)
		{
			if (Debug_filerep_print)
				ereport(LOG,
						(errmsg("could not sent file fsync request to mirror "),
						 FileRep_ReportRelationPath(
													open->mirrorFilespaceLocation,
													open->relFileNode,
													open->segmentFileNum)));
		}

		open->mirrorDataLossOccurred = FileRepPrimary_IsMirrorDataLossOccurred();
	}

	if (StorageManagerMirrorMode_DoPrimaryWork(open->mirrorMode) &&
		!FileRepResyncWorker_IsResyncRequest())
	{
		errno = 0;

		if (FileSync(open->primaryFile) < 0)
			primaryError = errno;
	}

	if (StorageManagerMirrorMode_SendToMirror(open->mirrorMode) &&
		!open->mirrorDataLossOccurred)
	{
		if (FileRepPrimary_IsOperationCompleted(
												FileRep_GetRelationIdentifier(
																			  open->mirrorFilespaceLocation,
																			  open->relFileNode,
																			  open->segmentFileNum),
												FileRepRelationTypeBufferPool) == FALSE)
		{
			ereport(LOG,
					(errmsg("could not fsync file on mirror "),
					 FileRep_ReportRelationPath(
												open->mirrorFilespaceLocation,
												open->relFileNode,
												open->segmentFileNum)));
		}
		open->mirrorDataLossOccurred = FileRepPrimary_IsMirrorDataLossOccurred();

	}

	errno = primaryError;
	return (errno == 0);

}

/*
 * Close a bulk relation file.
 *
 */
void
MirroredBufferPool_Close(
						 MirroredBufferPoolOpen *open)
 /* The open struct. */

{
	Assert(open != NULL);
	Assert(open->isActive);

	/* NOTE: Do not figure out mirroring here. */

	if (StorageManagerMirrorMode_SendToMirror(open->mirrorMode) &&
		!open->mirrorDataLossOccurred)
	{
		if (FileRepPrimary_MirrorClose(
									   FileRep_GetRelationIdentifier(
																	 open->mirrorFilespaceLocation,
																	 open->relFileNode,
																	 open->segmentFileNum),
									   FileRepRelationTypeBufferPool) != 0)
		{
			if (Debug_filerep_print)
			{
				ereport(LOG,
						(errmsg("could not sent file close request to mirror"),
						 FileRep_ReportRelationPath(
													open->mirrorFilespaceLocation,
													open->relFileNode,
													open->segmentFileNum)));
			}
		}

		open->mirrorDataLossOccurred = FileRepPrimary_IsMirrorDataLossOccurred();
	}

	if (StorageManagerMirrorMode_DoPrimaryWork(open->mirrorMode))
	{
		errno = 0;

		FileClose(open->primaryFile);

		open->primaryFile = -1;
	}

	open->isActive = false;

}

/*
 * Write a mirrored flat file.
 */
bool
MirroredBufferPool_Write(
						 MirroredBufferPoolOpen *open,
 /* The open struct. */

						 int32 position,
 /* The position to write the data. */

						 void *buffer,
 /* Pointer to the buffer. */

						 int32 bufferLen)
 /* Byte length of buffer. */

{
	int64		seekResult;
	int			primaryError;

	Assert(open != NULL);
	Assert(buffer != NULL);
	Assert(open->isActive);

	primaryError = 0;

	if (bufferLen > writeBufLen)
	{
		if (writeBuf != NULL)
		{
			pfree(writeBuf);
			writeBuf = NULL;
			writeBufLen = 0;
		}
	}

	if (writeBuf == NULL)
	{
		writeBufLen = bufferLen;

		writeBuf = MemoryContextAlloc(TopMemoryContext, writeBufLen);
		if (writeBuf == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_OUT_OF_MEMORY),
					 (errmsg("could not allocate memory for mirrored aligned buffer"))));
	}

	/*
	 * For Buffer Pool managed, we are normally not session oriented like
	 * Append-Only.
	 *
	 * Figure out mirroring each time...
	 */
	MirroredBufferPool_RecheckMirrorAccess(open);

	/*
	 * memcpy() is required since buffer is still changing, however filerep
	 * requires identical data to be written to primary and its mirror
	 */
	memcpy(writeBuf, buffer, bufferLen);

	if (StorageManagerMirrorMode_SendToMirror(open->mirrorMode) &&
		!open->mirrorDataLossOccurred)
	{
		if (FileRepPrimary_MirrorWrite(
									   FileRep_GetRelationIdentifier(
																	 open->mirrorFilespaceLocation,
																	 open->relFileNode,
																	 open->segmentFileNum),
									   FileRepRelationTypeBufferPool,
									   position,
									   writeBuf,
									   bufferLen,
									   FileRep_GetXLogRecPtrUndefined()) != 0)
		{
			if (Debug_filerep_print)
				ereport(LOG,
						(errmsg("could not sent write request to mirror position '%d' ",
								position),
						 FileRep_ReportRelationPath(
													open->mirrorFilespaceLocation,
													open->relFileNode,
													open->segmentFileNum)));
		}

		open->mirrorDataLossOccurred = FileRepPrimary_IsMirrorDataLossOccurred();

	}

	if (StorageManagerMirrorMode_DoPrimaryWork(open->mirrorMode) &&
		!FileRepResyncWorker_IsResyncRequest())
	{
		errno = 0;

		seekResult = FileSeek(open->primaryFile, position, SEEK_SET);
		if (seekResult != position)
		{
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not seek in file to position %d in file '%s', segment file %d : %m",
							position,
							relpath(open->relFileNode, MAIN_FORKNUM),
							open->segmentFileNum)));
		}

		errno = 0;
		if ((int) FileWrite(open->primaryFile, writeBuf, bufferLen) != bufferLen)
		{
			/* if write didn't set errno, assume problem is no disk space */
			if (errno == 0)
				errno = ENOSPC;
			primaryError = errno;
		}
	}

	errno = primaryError;
	return (primaryError == 0);

}

/*
 * Read a mirrored buffer pool page.
 */
int
MirroredBufferPool_Read(
						MirroredBufferPoolOpen *open,
 /* The open struct. */

						int32 position,
 /* The position to write the data. */

						void *buffer,
 /* Pointer to the buffer. */

						int32 bufferLen)
 /* Byte length of buffer. */

{
	int64		seekResult;

	Assert(open != NULL);
	Assert(buffer != NULL);
	Assert(open->isActive);

	if (!StorageManagerMirrorMode_DoPrimaryWork(open->mirrorMode))
		elog(ERROR, "file not open on the primary");

	errno = 0;
	seekResult = FileSeek(open->primaryFile, position, SEEK_SET);
	if (seekResult != position)
	{
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not seek in file to position %d in file '%s', segment file %d: %m",
						position,
						relpath(open->relFileNode, MAIN_FORKNUM),
						open->segmentFileNum)));
	}

	return FileRead(open->primaryFile, buffer, bufferLen);
}

int64
MirroredBufferPool_SeekSet(
						   MirroredBufferPoolOpen *open,
 /* The open struct. */

						   int32 position)
{
	Assert(open != NULL);
	Assert(open->isActive);

	if (!StorageManagerMirrorMode_DoPrimaryWork(open->mirrorMode))
		elog(ERROR, "file not open on the primary");

	errno = 0;
	return FileSeek(open->primaryFile, position, SEEK_SET);
}


int64
MirroredBufferPool_SeekEnd(
						   MirroredBufferPoolOpen *open)
 /* The open struct. */
{
	Assert(open != NULL);
	Assert(open->isActive);

	if (!StorageManagerMirrorMode_DoPrimaryWork(open->mirrorMode))
		elog(ERROR, "File not open on the primary");

	errno = 0;
	return FileSeek(open->primaryFile, 0L, SEEK_END);
}

void
MirroredBufferPool_BeginBulkLoad(
								 RelFileNode *relFileNode,
 /* The tablespace, database, and relation OIDs for the relation. */

								 ItemPointer persistentTid,

								 int64 persistentSerialNum,

								 MirroredBufferPoolBulkLoadInfo *bulkLoadInfo)
{
	MIRROREDLOCK_BUFMGR_DECLARE;

	MemSet(bulkLoadInfo, 0, sizeof(MirroredBufferPoolBulkLoadInfo));

	/* -------- MirroredLock ---------- */
	MIRROREDLOCK_BUFMGR_LOCK;

	/*
	 * Make this call while under the MirroredLock (unless we are a resync
	 * worker).
	 */
	bulkLoadInfo->mirrorDataLossTrackingState =
		FileRepPrimary_GetMirrorDataLossTrackingSessionNum(
														   &bulkLoadInfo->mirrorDataLossTrackingSessionNum);

	MIRROREDLOCK_BUFMGR_UNLOCK;
	/* -------- MirroredLock ---------- */

	bulkLoadInfo->relFileNode = *relFileNode;
	bulkLoadInfo->persistentTid = *persistentTid;
	bulkLoadInfo->persistentSerialNum = persistentSerialNum;

	if (Debug_persistent_print)
	{
		SUPPRESS_ERRCONTEXT_DECLARE;

		SUPPRESS_ERRCONTEXT_PUSH();

		elog(Persistent_DebugPrintLevel(),
			 "MirroredBufferPool_BeginBulkLoad %u/%u/%u: mirror data loss tracking (state '%s', session num " INT64_FORMAT "), persistent serial num " INT64_FORMAT ", TID %s",
			 relFileNode->spcNode,
			 relFileNode->dbNode,
			 relFileNode->relNode,
			 MirrorDataLossTrackingState_Name(bulkLoadInfo->mirrorDataLossTrackingState),
			 bulkLoadInfo->mirrorDataLossTrackingSessionNum,
			 persistentSerialNum,
			 ItemPointerToString(persistentTid));

		SUPPRESS_ERRCONTEXT_POP();
	}
}

bool
MirroredBufferPool_EvaluateBulkLoadFinish(
										  MirroredBufferPoolBulkLoadInfo *bulkLoadInfo)
{
	MIRROREDLOCK_BUFMGR_DECLARE;

	MirrorDataLossTrackingState mirrorDataLossTrackingState;
	int64		mirrorDataLossTrackingSessionNum;

	bool		bulkLoadFinished;

	/* -------- MirroredLock ---------- */
	MIRROREDLOCK_BUFMGR_LOCK;

	/*
	 * Make this call while under the MirroredLock (unless we are a resync
	 * worker).
	 */
	mirrorDataLossTrackingState =
		FileRepPrimary_GetMirrorDataLossTrackingSessionNum(
														   &mirrorDataLossTrackingSessionNum);

	bulkLoadFinished = false;
	/* Assume. */
	if (mirrorDataLossTrackingSessionNum == bulkLoadInfo->mirrorDataLossTrackingSessionNum)
	{
		if (bulkLoadInfo->mirrorDataLossTrackingState == MirrorDataLossTrackingState_MirrorDown &&
			mirrorDataLossTrackingState != bulkLoadInfo->mirrorDataLossTrackingState)
		{
			/*
			 * We started with the mirror down and ended with the mirror up in
			 * either Resynchronized or Synchronized state (or not
			 * configured).
			 *
			 * So, there was mirror data loss.
			 */
		}
		else
		{
			bulkLoadFinished = true;
		}
	}

	if (bulkLoadFinished)
	{
		if (Debug_persistent_print)
		{
			SUPPRESS_ERRCONTEXT_DECLARE;

			SUPPRESS_ERRCONTEXT_PUSH();

			elog(Persistent_DebugPrintLevel(),
				 "MirroredBufferPool_EvaluateBulkLoadFinish %u/%u/%u: no change -- mirror stayed up whole time.  Mirror data loss tracking (state '%s', session num " INT64_FORMAT "), persistent serial num " INT64_FORMAT ", TID %s",
				 bulkLoadInfo->relFileNode.spcNode,
				 bulkLoadInfo->relFileNode.dbNode,
				 bulkLoadInfo->relFileNode.relNode,
				 MirrorDataLossTrackingState_Name(bulkLoadInfo->mirrorDataLossTrackingState),
				 bulkLoadInfo->mirrorDataLossTrackingSessionNum,
				 bulkLoadInfo->persistentSerialNum,
				 ItemPointerToString(&bulkLoadInfo->persistentTid));

			SUPPRESS_ERRCONTEXT_POP();
		}
	}
	else
	{
		if (Debug_persistent_print)
		{
			SUPPRESS_ERRCONTEXT_DECLARE;

			SUPPRESS_ERRCONTEXT_PUSH();

			elog(Persistent_DebugPrintLevel(),
				 "MirroredBufferPool_EvaluateBulkLoadFinish %u/%u/%u: change.  Original mirror data loss tracking (state '%s', session num " INT64_FORMAT "), new mirror data loss tracking (state %d, session num " INT64_FORMAT "), persistent serial num " INT64_FORMAT ", TID %s",
				 bulkLoadInfo->relFileNode.spcNode,
				 bulkLoadInfo->relFileNode.dbNode,
				 bulkLoadInfo->relFileNode.relNode,
				 MirrorDataLossTrackingState_Name(bulkLoadInfo->mirrorDataLossTrackingState),
				 bulkLoadInfo->mirrorDataLossTrackingSessionNum,
				 mirrorDataLossTrackingState,
				 mirrorDataLossTrackingSessionNum,
				 bulkLoadInfo->persistentSerialNum,
				 ItemPointerToString(&bulkLoadInfo->persistentTid));

			SUPPRESS_ERRCONTEXT_POP();
		}

		switch (mirrorDataLossTrackingState)
		{
			case MirrorDataLossTrackingState_MirrorNotConfigured:
				bulkLoadFinished = true;
				break;

			case MirrorDataLossTrackingState_MirrorCurrentlyUpInSync:
			case MirrorDataLossTrackingState_MirrorCurrentlyUpInResync:
				bulkLoadFinished = false;
				/* We lost data along the way. */
				break;

			case MirrorDataLossTrackingState_MirrorDown:
				bulkLoadFinished = true;
				/* Give the problem to resync. */
				break;

			default:
				elog(ERROR, "Unexpected mirror data loss tracking state: %d",
					 mirrorDataLossTrackingState);
				bulkLoadFinished = false;
				/* A happy optimizer is the sound of one hand clapping. */
		}
	}

	if (bulkLoadFinished)
	{
		PersistentRelation_FinishBufferPoolBulkLoad(
													&bulkLoadInfo->relFileNode,
													&bulkLoadInfo->persistentTid,
													bulkLoadInfo->persistentSerialNum);
	}
	else
	{
		/*
		 * Save new information so caller can copy to mirror and reevaluate
		 * again.
		 */
		bulkLoadInfo->mirrorDataLossTrackingState = mirrorDataLossTrackingState;
		bulkLoadInfo->mirrorDataLossTrackingSessionNum = mirrorDataLossTrackingSessionNum;
	}

	MIRROREDLOCK_BUFMGR_UNLOCK;
	/* -------- MirroredLock ---------- */

	return bulkLoadFinished;
}

static void
MirroredBufferPool_DoCopyDataToMirror(
									  MirroredBufferPoolOpen *mirroredOpen,

									  int32 numOfBlocksToCopy,

									  bool *mirrorDataLossOccurred)
{
	char	   *buffer;
	int64		endOffset;
	int64		readOffset;
	int32		bufferLen;
	int			retval;

	*mirrorDataLossOccurred = false;

	readOffset = 0;
	endOffset = numOfBlocksToCopy * BLCKSZ;

	bufferLen = (Size) Min(2 * BLCKSZ, endOffset - readOffset);
	buffer = (char *) palloc(bufferLen);

	while (readOffset < endOffset)
	{
		retval = MirroredBufferPool_Read(
										 mirroredOpen,
										 readOffset,
										 buffer,
										 bufferLen);

		if (retval != bufferLen)
		{
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not read from position: " INT64_FORMAT " in file %d/%d/%d.%d : %m",
							readOffset,
							mirroredOpen->relFileNode.dbNode,
							mirroredOpen->relFileNode.spcNode,
							mirroredOpen->relFileNode.relNode,
							mirroredOpen->segmentFileNum)));

		}

		if (!MirroredBufferPool_Write(
									  mirroredOpen,
									  readOffset,
									  buffer,
									  bufferLen))
		{
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not write mirror position: " INT64_FORMAT " in file %d/%d/%d.%d : %m",
							readOffset,
							mirroredOpen->relFileNode.dbNode,
							mirroredOpen->relFileNode.spcNode,
							mirroredOpen->relFileNode.relNode,
							mirroredOpen->segmentFileNum)));

		}

		if (mirroredOpen->mirrorDataLossOccurred)
		{
			*mirrorDataLossOccurred = true;
			break;
		}

		readOffset += bufferLen;

		bufferLen = (Size) Min(2 * BLCKSZ, endOffset - readOffset);
	}

	pfree(buffer);
}

void
MirroredBufferPool_CopyToMirror(
								RelFileNode *relFileNode,

								char *relationName,

								ItemPointer persistentTid,

								int64 persistentSerialNum,

								MirrorDataLossTrackingState mirrorDataLossTrackingState,

								int64 mirrorDataLossTrackingSessionNum,

								int32 numOfBlocks,

								bool *mirrorDataLossOccurred)
{
	MirroredBufferPoolOpen mirroredOpen;

	MirrorDataLossTrackingState currentMirrorDataLossTrackingState;
	int64		currentMirrorDataLossTrackingSessionNum;

	int32		numOfSegments;
	int32		numOfRemainingBlocks;
	int			segmentFileNum;
	int32		numOfBlocksToCopy;

	int			primaryError;

	Assert(numOfBlocks > 0);

	numOfSegments = ((numOfBlocks - 1) / RELSEG_SIZE) + 1;

	Assert(numOfSegments > 0);

	numOfRemainingBlocks = numOfBlocks;

	if (Debug_persistent_print)
	{
		SUPPRESS_ERRCONTEXT_DECLARE;

		SUPPRESS_ERRCONTEXT_PUSH();

		elog(Persistent_DebugPrintLevel(),
			 "MirroredBufferPool_CopyToMirror %u/%u/%u: copy %d blocks (%d segments).  Mirror data loss tracking (state '%s', session num " INT64_FORMAT "), persistent serial num " INT64_FORMAT ", TID %s",
			 relFileNode->spcNode,
			 relFileNode->dbNode,
			 relFileNode->relNode,
			 numOfBlocks,
			 numOfSegments,
			 MirrorDataLossTrackingState_Name(mirrorDataLossTrackingState),
			 mirrorDataLossTrackingSessionNum,
			 persistentSerialNum,
			 ItemPointerToString(persistentTid));

		SUPPRESS_ERRCONTEXT_POP();
	}

	for (segmentFileNum = 0; segmentFileNum < numOfSegments; segmentFileNum++)
	{
		Assert(numOfRemainingBlocks > 0);

		LWLockAcquire(MirroredLock, LW_SHARED);

		currentMirrorDataLossTrackingState =
			FileRepPrimary_GetMirrorDataLossTrackingSessionNum(
															   &currentMirrorDataLossTrackingSessionNum);
		if (currentMirrorDataLossTrackingSessionNum != mirrorDataLossTrackingSessionNum)
		{
			*mirrorDataLossOccurred = true;
			LWLockRelease(MirroredLock);

			return;
		}
		Assert(currentMirrorDataLossTrackingState == mirrorDataLossTrackingState);

		MirroredBufferPool_DoOpen(
								  &mirroredOpen,
								  relFileNode,
								  segmentFileNum,
								  relationName,
								  mirrorDataLossTrackingState,
								  mirrorDataLossTrackingSessionNum,
								   /* create */ false,
								   /* mirrorOnly */ false,
								   /* copyToMirror */ true,
								  &primaryError,
								  mirrorDataLossOccurred);

		if (primaryError != 0)
		{
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not open relation file '%s', relation name '%s': %s",
							relpath(*relFileNode, MAIN_FORKNUM),
							relationName,
							strerror(primaryError))));
		}

		LWLockRelease(MirroredLock);

		if (*mirrorDataLossOccurred)
		{
			return;
		}

		if (numOfRemainingBlocks > RELSEG_SIZE)
			numOfBlocksToCopy = RELSEG_SIZE;
		else
			numOfBlocksToCopy = numOfRemainingBlocks;

		MirroredBufferPool_DoCopyDataToMirror(
											  &mirroredOpen,
											  numOfBlocksToCopy,
											  mirrorDataLossOccurred);

		if (*mirrorDataLossOccurred)
		{
			MirroredBufferPool_Close(&mirroredOpen);
			return;
		}

		if (!MirroredBufferPool_Flush(&mirroredOpen))
		{
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not flush relation file '%s', relation name '%s': %s",
							relpath(*relFileNode, MAIN_FORKNUM),
							relationName,
							strerror(primaryError))));
		}

		if (mirroredOpen.mirrorDataLossOccurred)
		{
			*mirrorDataLossOccurred = true;
			MirroredBufferPool_Close(&mirroredOpen);
			return;
		}

		MirroredBufferPool_Close(&mirroredOpen);

		numOfRemainingBlocks -= numOfBlocksToCopy;
	}
}

/*
 * Mirrored drop.
 */
static void
MirroredBufferPool_DoDrop(
						  RelFileNode *relFileNode,
 /* The tablespace, database, and relation OIDs for the open. */

						  uint32 segmentFileNum,
 /* Which segment file. */

						  char *relationName,
 /* For tracing only.  Can be NULL in some execution paths. */

						  MirrorDataLossTrackingState mirrorDataLossTrackingState,

						  int64 mirrorDataLossTrackingSessionNum,

						  bool primaryOnly,

						  bool mirrorOnly,

						  int *primaryError,

						  bool *mirrorDataLossOccurred)
{
	StorageManagerMirrorMode mirrorMode;

	char	   *primaryFilespaceLocation;
	char	   *mirrorFilespaceLocation;

	*primaryError = 0;
	*mirrorDataLossOccurred = false;

	MirroredBufferPool_SetUpMirrorAccess(
										 relFileNode,
										 segmentFileNum,
										 relationName,
										 mirrorDataLossTrackingState,
										 mirrorDataLossTrackingSessionNum,
										 primaryOnly,
										 mirrorOnly,
										 &mirrorMode,
										 mirrorDataLossOccurred);

	PersistentTablespace_GetPrimaryAndMirrorFilespaces(
													   relFileNode->spcNode,
													   &primaryFilespaceLocation,
													   &mirrorFilespaceLocation);

	if (StorageManagerMirrorMode_SendToMirror(mirrorMode) &&
		!*mirrorDataLossOccurred)
	{
		if (FileRepPrimary_MirrorDrop(
									  FileRep_GetRelationIdentifier(
																	mirrorFilespaceLocation,
																	*relFileNode,
																	segmentFileNum),
									  FileRepRelationTypeBufferPool) != 0)
		{
			if (Debug_filerep_print)
				ereport(LOG,
						(errmsg("could not sent file drop request to mirror "),
						 FileRep_ReportRelationPath(
													mirrorFilespaceLocation,
													*relFileNode,
													segmentFileNum)));
		}

		*mirrorDataLossOccurred = FileRepPrimary_IsMirrorDataLossOccurred();

	}

	if (StorageManagerMirrorMode_DoPrimaryWork(mirrorMode))
	{
		char	   *dbPath;
		char	   *path;

		dbPath = (char *) palloc(MAXPGPATH + 1);
		path = (char *) palloc(MAXPGPATH + 1);

		FormDatabasePath(
						 dbPath,
						 primaryFilespaceLocation,
						 relFileNode->spcNode,
						 relFileNode->dbNode);

		if (segmentFileNum == 0)
			sprintf(path, "%s/%u", dbPath, relFileNode->relNode);
		else
			sprintf(path, "%s/%u.%u", dbPath, relFileNode->relNode, segmentFileNum);

		errno = 0;

		if (unlink(path) < 0)
		{
			*primaryError = errno;
		}

		pfree(dbPath);
		pfree(path);
	}

	if (StorageManagerMirrorMode_SendToMirror(mirrorMode) &&
		!*mirrorDataLossOccurred)
	{
		if (FileRepPrimary_IsOperationCompleted(
												FileRep_GetRelationIdentifier(
																			  mirrorFilespaceLocation,
																			  *relFileNode,
																			  segmentFileNum),
												FileRepRelationTypeBufferPool) == FALSE)
		{
			if (Debug_filerep_print)
				ereport(LOG,
						(errmsg("could not drop file on mirror "),
						 FileRep_ReportRelationPath(
													mirrorFilespaceLocation,
													*relFileNode,
													segmentFileNum)));
		}

		*mirrorDataLossOccurred = FileRepPrimary_IsMirrorDataLossOccurred();

	}

	if (primaryFilespaceLocation != NULL)
		pfree(primaryFilespaceLocation);

	if (mirrorFilespaceLocation != NULL)
		pfree(mirrorFilespaceLocation);
}

void
MirroredBufferPool_Drop(
						RelFileNode *relFileNode,

						int32 segmentFileNum,

						char *relationName,
 /* For tracing only.  Can be NULL in some execution paths. */

						bool primaryOnly,

						bool isRedo,

						int *primaryError,

						bool *mirrorDataLossOccurred)
{
	MirrorDataLossTrackingState mirrorDataLossTrackingState;
	int64		mirrorDataLossTrackingSessionNum;

	mirrorDataLossTrackingState =
		FileRepPrimary_GetMirrorDataLossTrackingSessionNum(
														   &mirrorDataLossTrackingSessionNum);

	MirroredBufferPool_DoDrop(
							  relFileNode,
							  segmentFileNum,
							  relationName,
							  mirrorDataLossTrackingState,
							  mirrorDataLossTrackingSessionNum,
							  primaryOnly,
							   /* mirrorOnly */ false,
							  primaryError,
							  mirrorDataLossOccurred);
}

void
MirroredBufferPool_MirrorReDrop(
								RelFileNode *relFileNode,

								int32 segmentFileNum,

								MirrorDataLossTrackingState mirrorDataLossTrackingState,

								int64 mirrorDataLossTrackingSessionNum,

								bool *mirrorDataLossOccurred)
{
	int			primaryError;

	MirroredBufferPool_DoDrop(
							  relFileNode,
							  segmentFileNum,
							   /* relationName */ NULL,
							  mirrorDataLossTrackingState,
							  mirrorDataLossTrackingSessionNum,
							   /* primaryOnly */ false,
							   /* mirrorOnly */ true,
							  &primaryError,
							  mirrorDataLossOccurred);
	if (primaryError != 0)
	{
		/* Only doing mirror work here? */
		elog(LOG, "%u/%u/%u: Not expecting a primary error %d ('%s') here!",
			 relFileNode->spcNode,
			 relFileNode->dbNode,
			 relFileNode->relNode,
			 primaryError,
			 strerror(primaryError));
	}
}

bool
MirroredBufferPool_Truncate(
							MirroredBufferPoolOpen *open,
 /* The open struct. */

							int64 position)
 /* The position to cutoff the data. */
{
	int			primaryError;

	primaryError = 0;

	/*
	 * For Buffer Pool managed, we are normally not session oriented like
	 * Append-Only.
	 *
	 * Figure out mirroring each time...
	 */
	MirroredBufferPool_RecheckMirrorAccess(open);

	if (StorageManagerMirrorMode_SendToMirror(open->mirrorMode) &&
		!open->mirrorDataLossOccurred)
	{
		if (FileRepPrimary_MirrorTruncate(
										  FileRep_GetRelationIdentifier(
																		open->mirrorFilespaceLocation,
																		open->relFileNode,
																		open->segmentFileNum),
										  FileRepRelationTypeBufferPool,
										  position) != 0)
		{
			if (Debug_filerep_print)
				ereport(LOG,
						(errmsg("could not send file truncate request to mirror "),
						 FileRep_ReportRelationPath(
													open->mirrorFilespaceLocation,
													open->relFileNode,
													open->segmentFileNum)));
		}

		open->mirrorDataLossOccurred = FileRepPrimary_IsMirrorDataLossOccurred();
	}

	if (StorageManagerMirrorMode_DoPrimaryWork(open->mirrorMode) &&
		!FileRepResyncWorker_IsResyncRequest())
	{
		errno = 0;

		if (FileTruncate(open->primaryFile, position) < 0)
			primaryError = errno;
	}

	errno = primaryError;
	return (primaryError == 0);

}
