/*-------------------------------------------------------------------------
 *
 * cdbmirroredflatfile.c
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbmirroredflatfile.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "miscadmin.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <signal.h>

#include "access/twophase.h"
#include "access/slru.h"
#include "access/xlog_internal.h"
#include "utils/palloc.h"
#include "cdb/cdbfilerepprimary.h"
#include "cdb/cdbmirroredflatfile.h"
#include "cdb/cdbpersistentdatabase.h"
#include "cdb/cdbpersistenttablespace.h"
#include "storage/fd.h"
#include "catalog/catalog.h"
#include "catalog/pg_tablespace.h"
#include "utils/memutils.h"

#define PGVERSION "PG_VERSION"

/* Aligning xlog mirrored buffer */
static int32 writeBufLen = 0;
static char	*writeBuf = NULL;
static char *writeBufAligned = NULL;


/*
 * The following two gucs
 *					a) fsync=on
 *					b) wal_sync_method=open_sync
 * open XLOG files with O_DIRECT flag.
 * O_DIRECT flag requires XLOG buffer to be 512 byte aligned.
 */
static void
XLogInitMirroredAlignedBuffer(int32 bufferLen)
{
	if (bufferLen > writeBufLen)
	{
		if (writeBuf != NULL)
		{
			pfree(writeBuf);
			writeBuf = NULL;
			writeBufAligned = NULL;
			writeBufLen = 0;
		}
	}

	if (writeBuf == NULL)
	{
		writeBufLen = bufferLen;

		writeBuf = MemoryContextAlloc(TopMemoryContext, writeBufLen + ALIGNOF_XLOG_BUFFER);
		if (writeBuf == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_OUT_OF_MEMORY),
					 (errmsg("could not allocate memory for mirrored aligned buffer"))));
		writeBufAligned = (char *) TYPEALIGN(ALIGNOF_XLOG_BUFFER, writeBuf);
	}
}

/*
 * Open a relation for mirrored write.
 */
int MirroredFlatFile_Open(
	MirroredFlatFileOpen *open,
				/* The resulting open struct. */

	char 			*subDirectory,

	char			*simpleFileName,
				/* The simple file name. */

	int 			fileFlags,

	int 			fileMode,

	bool			suppressError,

	bool			atomic_op,

	bool			isMirrorRecovery)
/* NOTE do we need to consider supressError on mirror */

{
	int save_errno = 0;

	char *dir = NULL, *mirrorDir = NULL;

	Assert(open != NULL);

	/* If the user wants an atomic write, they better be creating a file */
	Assert((atomic_op && (fileFlags & O_CREAT) == O_CREAT) ||
		   !atomic_op);

	MemSet(open, 0, sizeof(MirroredFlatFileOpen));

	open->atomic_op = atomic_op;

	if (atomic_op)
	{
		char *tmp_filename;

		tmp_filename = (char*)palloc(MAXPGPATH + 1);

		open->atomicSimpleFileName = MemoryContextStrdup(TopMemoryContext, simpleFileName);
		sprintf(tmp_filename, "%s.%i",
				 simpleFileName, MyProcPid);
		open->simpleFileName = MemoryContextStrdup(TopMemoryContext, tmp_filename);

		pfree(tmp_filename);
	}
	else
	{
		open->simpleFileName = MemoryContextStrdup(TopMemoryContext, simpleFileName);
	}

	open->isMirrorRecovery = isMirrorRecovery;
	open->usingDbDirNode = false;

	/*
	 * Now that the transaction filespace is configurable, the subdirectory is not always
	 * relative to the data directory. If we have configured the transaction files to be on
	 * the non-default filespace, then we need the absolute path for the sub directory
         */
	if (isTxnDir(subDirectory))
	{
		dir = makeRelativeToTxnFilespace(subDirectory);
		mirrorDir = makeRelativeToPeerTxnFilespace(subDirectory);
		open->subDirectory = MemoryContextStrdup(TopMemoryContext, mirrorDir);
	}
	else
	{
		/* Default case */
		dir = MemoryContextStrdup(TopMemoryContext, subDirectory);
		open->subDirectory = MemoryContextStrdup(TopMemoryContext, subDirectory);
	}

	open->path = MemoryContextAlloc(TopMemoryContext, strlen(dir) + 1 +
									strlen(open->simpleFileName) + 1);


	if (snprintf(open->path, MAXPGPATH, "%s/%s", dir, open->simpleFileName) > MAXPGPATH)
	{
		ereport(ERROR, (errmsg("cannot generate path %s/%s", dir, open->simpleFileName)));
	}

	if (FileRepPrimary_MirrorOpen(
				  FileRep_GetFlatFileIdentifier(
								open->subDirectory,
								open->simpleFileName),
				  FileRepRelationTypeFlatFile,
				  FILEREP_OFFSET_UNDEFINED,
				  fileFlags,
				  fileMode,
				  suppressError) != 0)
		ereport(LOG,
				(errmsg("could not sent open file to mirror \"%s\" open file flags:'%x' ",
						open->path,
						fileFlags)));

	errno = 0;

	if (! open->isMirrorRecovery) {
		open->primaryFile = PathNameOpenFile(open->path, fileFlags, fileMode);
		save_errno = errno;

		if (open->primaryFile < 0)
		{
			if (!suppressError)
			{
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not open file \"%s\": %m", open->path)));
			}

			pfree(open->subDirectory);
			open->subDirectory = NULL;

			pfree(open->simpleFileName);
			open->simpleFileName = NULL;

			pfree(open->path);
			open->path = NULL;

			open->isActive = false;

		}
		else
		{
			open->appendPosition = 0;

			open->isActive = true;
		}
	}
	else
	{
		open->isActive = true;
	}

	pfree(dir);
	if (mirrorDir)
		pfree(mirrorDir);

	return save_errno;
}

/*
 * Open a relation for mirrored write.
 */
int MirroredFlatFile_OpenInDbDir(
	MirroredFlatFileOpen *open,
				/* The resulting open struct. */

	DbDirNode 		*dbDirNode,

	char			*simpleFileName,
				/* The simple file name. */

	int 			fileFlags,

	int 			fileMode,

	bool			suppressError)

{
	int save_errno = 0;

	char *primaryFilespaceLocation = NULL;
	char *mirrorFilespaceLocation = NULL;

	char *subDirectoryPrimary;
	char *subDirectoryMirror;

	Assert(open != NULL);

	MemSet(open, 0, sizeof(MirroredFlatFileOpen));

	open->usingDbDirNode = true;
	open->dbDirNode = *dbDirNode;

	subDirectoryPrimary = (char*)palloc(MAXPGPATH + 1);
	subDirectoryMirror = (char*)palloc(MAXPGPATH + 1);

	PersistentTablespace_GetPrimaryAndMirrorFilespaces(
													   dbDirNode->tablespace,
													   &primaryFilespaceLocation,
													   &mirrorFilespaceLocation);
	FormDatabasePath(
					 subDirectoryPrimary,
					 primaryFilespaceLocation,
					 dbDirNode->tablespace,
					 dbDirNode->database);

	/* Skip mirror if Master of Primary without mirrors */
	if (dbDirNode->tablespace == GLOBALTABLESPACE_OID ||
		dbDirNode->tablespace == DEFAULTTABLESPACE_OID ||
		mirrorFilespaceLocation != NULL)
	{
		FormDatabasePath(
						 subDirectoryMirror,
						 mirrorFilespaceLocation,
						 dbDirNode->tablespace,
						 dbDirNode->database);

		open->subDirectory = MemoryContextStrdup(TopMemoryContext, subDirectoryMirror);
	}
	else
	{
		/* If Master Node or Primary Segment with no Mirrors */
		open->subDirectory = MemoryContextStrdup(TopMemoryContext, subDirectoryPrimary);
	}

	open->simpleFileName = MemoryContextStrdup(TopMemoryContext, simpleFileName);
	open->path = MemoryContextAlloc(TopMemoryContext, strlen(subDirectoryPrimary) + 1 + strlen(simpleFileName) + 1);

	sprintf(open->path, "%s/%s", subDirectoryPrimary, simpleFileName);

	if (FileRepPrimary_MirrorOpen(
					  FileRep_GetFlatFileIdentifier(
							open->subDirectory,
							open->simpleFileName),
					  FileRepRelationTypeFlatFile,
					  FILEREP_OFFSET_UNDEFINED,
					  fileFlags,
					  fileMode,
					  suppressError) != 0)
		ereport(LOG,
				(errmsg("could not sent open file to mirror '%s/%s' open file flags:'%x' ",
						open->subDirectory,
						open->simpleFileName,
						fileFlags)));

	errno = 0;
	open->primaryFile = PathNameOpenFile(open->path, fileFlags, fileMode);
	save_errno = errno;

	if (open->primaryFile < 0)
	{
		if (!suppressError)
		{
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not open file \"%s\": %m", open->path)));
		}

		pfree(open->subDirectory);
		open->subDirectory = NULL;

		pfree(open->simpleFileName);
		open->simpleFileName = NULL;

		pfree(open->path);
		open->path = NULL;

		open->isActive = false;
	}
	else
	{
		open->appendPosition = 0;

		open->isActive = true;
	}

	pfree(subDirectoryPrimary);
	pfree(subDirectoryMirror);

	if (primaryFilespaceLocation != NULL)
		pfree(primaryFilespaceLocation);

	if (mirrorFilespaceLocation != NULL)
		pfree(mirrorFilespaceLocation);

	return save_errno;
}


extern bool MirroredFlatFile_IsActive(
	MirroredFlatFileOpen *open)
				/* The open struct. */
{
	return open->isActive;
}

/*
 * Flush a flat file.
 *
 */
int MirroredFlatFile_Flush(
	MirroredFlatFileOpen *open,
				/* The open struct. */

	bool				suppressError)

{
	bool	mirrorFlush = TRUE;
	int		save_errno = 0;

	Assert(open != NULL);
	Assert(open->isActive);

	if (FileRepPrimary_MirrorFlush(
			   FileRep_GetFlatFileIdentifier(
							 open->subDirectory,
							 open->simpleFileName),
			   FileRepRelationTypeFlatFile) != 0) {

		mirrorFlush = FALSE;
		ereport(LOG,
				(errmsg("could not sent fsync file to mirror '%s/%s' ",
						open->subDirectory,
						open->simpleFileName)));
	}

	if (! open->isMirrorRecovery) {
		errno = 0;
		if (FileSync(open->primaryFile) != 0)
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not fsync file \"%s\": %m", open->path)));

		save_errno = errno;
	}

	if (mirrorFlush) {
		if (FileRepPrimary_IsOperationCompleted(
					   FileRep_GetFlatFileIdentifier(
								 open->subDirectory,
								 open->simpleFileName),
					   FileRepRelationTypeFlatFile) == FALSE){
			ereport(LOG,
				    (errmsg("could not fsync file on mirror '%s/%s' ",
							open->subDirectory,
							open->simpleFileName)));
		}
	}

	return save_errno;
}

/*
 * Close a bulk relation file.
 *
 */
void MirroredFlatFile_Close(
	MirroredFlatFileOpen *open)
				/* The open struct. */

{
	Assert(open != NULL);
	Assert(open->isActive);


	if (FileRepPrimary_MirrorClose(
				   FileRep_GetFlatFileIdentifier(
							 open->subDirectory,
							 open->simpleFileName),
				   FileRepRelationTypeFlatFile) != 0)
		ereport(LOG,
				(errmsg("could not sent close file to mirror \"%s\"", open->path)));

	if (! open->isMirrorRecovery) {
		errno = 0;
		FileClose(open->primaryFile);
	}

	if (open->atomic_op)
	{
		MirroredFlatFile_Rename(open->subDirectory,
								open->simpleFileName,
								open->atomicSimpleFileName,
								true,
								false);
	}

	Assert(open->subDirectory != NULL);
	pfree(open->subDirectory);
	open->subDirectory = NULL;
	Assert(open->simpleFileName != NULL);
	pfree(open->simpleFileName);
	open->simpleFileName = NULL;
	Assert(open->path != NULL);
	pfree(open->path);
	open->path = NULL;

	if (open->atomic_op)
		pfree(open->atomicSimpleFileName);

	open->isActive = false;
}

/*
 * Rename a flat file.
 *
 */
int MirroredFlatFile_Rename(
							char 			*subDirectory,

							char			*oldSimpleFileName,
								/* The simple file name. */

							char			*newSimpleFileName,
								/* The simple file name. */

							bool			new_can_exist,

							bool			isMirrorRecovery)
					/* if TRUE then request is not performed on primary */
{
	char *oldPath;
	char *newPath;
	bool mirrorRename = TRUE;
	int	 save_errno = 0;
	struct stat buf;

	char *dir = NULL, *mirrorDir = NULL;

	oldPath = (char*)palloc(MAXPGPATH + 1);
	newPath = (char*)palloc(MAXPGPATH + 1);

	if (isTxnDir(subDirectory))
	{
		dir = makeRelativeToTxnFilespace(subDirectory);
		mirrorDir = makeRelativeToPeerTxnFilespace(subDirectory);
	}
	else
	{
		dir = MemoryContextStrdup(TopMemoryContext, subDirectory);
		mirrorDir = MemoryContextStrdup(TopMemoryContext, subDirectory);
	}

	if (snprintf(oldPath, MAXPGPATH, "%s/%s", dir, oldSimpleFileName) > MAXPGPATH)
	{
		ereport(ERROR, (errmsg("cannot generate path %s/%s", dir, oldSimpleFileName)));
	}
	if (snprintf(newPath, MAXPGPATH, "%s/%s", dir, newSimpleFileName) > MAXPGPATH)
	{
		ereport(ERROR, (errmsg("cannot generate path %s/%s", dir, newSimpleFileName)));
	}

	if (!new_can_exist)
	{
		if (stat(newPath, &buf) == 0)
			elog(ERROR, "cannot rename flat file %s to %s: File exists",
				 oldPath,
				 newPath);
	}

	if (FileRepPrimary_MirrorRename(
								   FileRep_GetFlatFileIdentifier(
												mirrorDir,
												oldSimpleFileName),
									FileRep_GetFlatFileIdentifier(
											  mirrorDir,
											  newSimpleFileName),
									FileRepRelationTypeFlatFile) != 0) {

		mirrorRename = FALSE;
		ereport(LOG,
				(errmsg("could not sent rename file to mirror from \"%s\" to \"%s\"",
						oldPath, newPath)));
	}

	if (!isMirrorRecovery)
	{
		if (rename(oldPath, newPath) != 0)
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not rename file from \"%s\" to \"%s\": %m",
							oldPath, newPath)));
		else
			errno = 0;
		save_errno = errno;
	}

	if (mirrorRename)
	{
		if (FileRepPrimary_IsOperationCompleted(
							FileRep_GetFlatFileIdentifier(mirrorDir,
														  oldSimpleFileName),
							FileRepRelationTypeFlatFile) == FALSE)
			ereport(LOG,
					(errmsg("could not rename file on mirror from \"%s\" to \"%s\" ",
							oldPath, newPath)));
	}

	pfree(oldPath);
	pfree(newPath);

	pfree(dir);
	pfree(mirrorDir);

	return save_errno;
}


// -----------------------------------------------------------------------------
// Append
// -----------------------------------------------------------------------------

/*
 * Write a mirrored flat file.
 */
int MirroredFlatFile_Append(
	MirroredFlatFileOpen *open,
				/* The open struct. */

	void		*buffer,
				/* Pointer to the buffer. */

	int32		bufferLen,
				/* Byte length of buffer. */

	bool		suppressError)

{
	int save_errno = 0;

	Assert(open != NULL);
	Assert(open->isActive);

	if (FileRepPrimary_MirrorWrite(
				   FileRep_GetFlatFileIdentifier(
								 open->subDirectory,
								 open->simpleFileName),
				   FileRepRelationTypeFlatFile,
				   FILEREP_OFFSET_UNDEFINED,
				   buffer,
				   bufferLen,
				   FileRep_GetXLogRecPtrUndefined())	!= 0)
		ereport(LOG,
				(errmsg("could not sent write to mirror '%s/%s' ",
						open->subDirectory,
						open->simpleFileName)));

	if (! open->isMirrorRecovery) {
		errno = 0;
		if ((int) FileWrite(open->primaryFile, buffer, bufferLen) != bufferLen)
		{
			/* if write didn't set errno, assume problem is no disk space */
			if (errno == 0)
				errno = ENOSPC;

			if (!suppressError)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not write to file \"%s\": %m", open->path)));
		}
		save_errno = errno;
	}

	open->appendPosition += bufferLen;

	return save_errno;
}

/*
 * Write a mirrored flat file.
 */
int MirroredFlatFile_Write(
	MirroredFlatFileOpen *open,
				/* The open struct. */

	int32		position,
				/* The position to write the data. */

	void		*buffer,
				/* Pointer to the buffer. */

	int32		bufferLen,
				/* Byte length of buffer. */

	bool		suppressError)

{
	int64	seekResult;
	int		save_errno = 0;

	Assert(open != NULL);
	Assert(open->isActive);

	XLogInitMirroredAlignedBuffer(bufferLen);

	/*
	 * memcpy() is required since buffer is still changing, however filerep
	 * requires identical data are written to primary and its mirror
	 */
	memcpy(writeBufAligned, buffer, bufferLen);

	if (FileRepPrimary_MirrorWrite(
				   FileRep_GetFlatFileIdentifier(
								 open->subDirectory,
								 open->simpleFileName),
				   FileRepRelationTypeFlatFile,
				   position,
				   writeBufAligned,
				   bufferLen,
				   FileRep_GetXLogRecPtrUndefined())	!= 0)
		ereport(LOG,
				(errmsg("could not sent seek and write to mirror '%s/%s' ",
						open->subDirectory,
						open->simpleFileName)));

	if (! open->isMirrorRecovery) {
		errno = 0;
		seekResult = FileSeek(open->primaryFile, position, SEEK_SET);
		if (seekResult != position)
		{
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not seek in file to position '%d' in file '%s': %m",
							position,
							open->path)));
		}

		if ((int) FileWrite(open->primaryFile, writeBufAligned, bufferLen) != bufferLen)
		{
			/* if write didn't set errno, assume problem is no disk space */
			if (errno == 0)
				errno = ENOSPC;
			if (!suppressError)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not write to file \"%s\": %m", open->path)));
		}
		save_errno = errno;
	}

	return save_errno;
}

/*
 * Open a relation on primary
 */
int MirroredFlatFile_OpenPrimary(
						  MirroredFlatFileOpen *open,
						  /* The resulting open struct. */

						  char 			*subDirectory,

						  char			*simpleFileName,
						  /* The simple file name. */

						  int 			fileFlags,

						  int 			fileMode,

						  bool			suppressError)
/* NOTE do we need to consider supressError on mirror */

{
	Assert(open != NULL);

	MemSet(open, 0, sizeof(MirroredFlatFileOpen));

	open->usingDbDirNode = false;
	open->subDirectory = MemoryContextStrdup(TopMemoryContext, subDirectory);
	open->simpleFileName = MemoryContextStrdup(TopMemoryContext, simpleFileName);
	open->path = MemoryContextAlloc(TopMemoryContext, strlen(subDirectory) + 1 + strlen(simpleFileName) + 1);

	sprintf(open->path, "%s/%s", subDirectory, simpleFileName);

	errno = 0;
	open->primaryFile = PathNameOpenFile(open->path, fileFlags, fileMode);
	if (open->primaryFile < 0)
	{
		if (!suppressError)
		{
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not open file \"%s\": %m", open->path)));
		}

		pfree(open->subDirectory);
		open->subDirectory = NULL;

		pfree(open->simpleFileName);
		open->simpleFileName = NULL;

		pfree(open->path);
		open->path = NULL;

		open->isActive = false;
	}
	else
	{
		open->isActive = true;

		open->appendPosition = 0;
	}

	return errno;
}

/*
 * Close a bulk relation file.
 *
 */
void MirroredFlatFile_ClosePrimary(
							MirroredFlatFileOpen *open)
/* The open struct. */

{
	Assert(open != NULL);
	Assert(open->isActive);

	errno = 0;

	FileClose(open->primaryFile);

	Assert(open->subDirectory != NULL);
	pfree(open->subDirectory);
	open->subDirectory = NULL;
	Assert(open->simpleFileName != NULL);
	pfree(open->simpleFileName);
	open->simpleFileName = NULL;
	Assert(open->path != NULL);
	pfree(open->path);
	open->path = NULL;

	open->isActive = false;
}


/*
 * Read a local flat file.
 */
int MirroredFlatFile_Read(
	MirroredFlatFileOpen *open,
				/* The open struct. */

	int32		position,
				/* The position to write the data. */

	void		*buffer,
				/* Pointer to the buffer. */

	int32		bufferLen)
				/* Byte length of buffer. */

{
	int64 seekResult;

	Assert(open != NULL);
	Assert(buffer != NULL);
	Assert(open->isActive);

	errno = 0;

	seekResult = FileSeek(open->primaryFile, position, SEEK_SET);
	if (seekResult != position)
	{
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not seek in file to position %d in file \"%s\": %m",
				        position,
				        open->path)));
	}

	return FileRead(open->primaryFile, buffer, bufferLen);
}

/*
 * Seek in local flat file
 */
int64 MirroredFlatFile_SeekSet(
	MirroredFlatFileOpen *open,
				/* The open struct. */

	int32		position)
{
	errno = 0;
	return FileSeek(open->primaryFile, position, SEEK_SET);
}

/*
 * Seek to the end of local flat file
 */
int64 MirroredFlatFile_SeekEnd(
							   MirroredFlatFileOpen *open)
							   /* The open struct. */
{
	errno = 0;
	return FileSeek(open->primaryFile, 0, SEEK_END);
}


/*
 * Mirrored drop.
 */
int MirroredFlatFile_Drop(
	char 			*subDirectory,

	char			*simpleFileName,
		/* The simple file name. */

	bool			suppressError,

	bool			isMirrorRecovery)
		/* if TRUE then request is not performed on primary */
{
	char *path;
	bool mirrorDrop = TRUE;
	int	 save_errno = 0;
	int	return_value = 0;
	char *dir = NULL, *mirrorDir = NULL;

	if (isTxnDir(subDirectory))
	{
		dir = makeRelativeToTxnFilespace(subDirectory);
		mirrorDir = makeRelativeToPeerTxnFilespace(subDirectory);
	}
	else
	{
		dir = MemoryContextStrdup(TopMemoryContext, subDirectory);
		mirrorDir = MemoryContextStrdup(TopMemoryContext, subDirectory);
	}

	path = (char*)palloc(MAXPGPATH + 1);

	if (snprintf(path, MAXPGPATH, "%s/%s", dir, simpleFileName) > MAXPGPATH)
	{
		ereport(ERROR, (errmsg("cannot generate path %s/%s", dir, simpleFileName)));
	}

	if (FileRepPrimary_MirrorDrop(
				   FileRep_GetFlatFileIdentifier(
							 mirrorDir,
							 simpleFileName),
				   FileRepRelationTypeFlatFile) != 0) {

		mirrorDrop = FALSE;
		ereport(LOG,
				(errmsg("could not sent drop file to mirror \"%s\"", path)));
	}

	if (! isMirrorRecovery) {
		errno = 0;
		return_value = unlink(path);
		if (return_value < 0 && !suppressError){
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not unlink file \"%s\": %m", path)));
		}
		save_errno = errno;
	}

	if (mirrorDrop) {
		if (FileRepPrimary_IsOperationCompleted(
					   FileRep_GetFlatFileIdentifier(
								 mirrorDir,
								 simpleFileName),
					   FileRepRelationTypeFlatFile) == FALSE)
			ereport(LOG,
					(errmsg("could not drop file on mirror \"%s\"", path)));
	}

	pfree(path);
	pfree(dir);
	pfree(mirrorDir);

	errno = save_errno;
	return return_value;
}


/*
 * Reconcile xlog (WAL) EOF between primary and mirror.
 *		xlog gets truncated to the same EOF on primary and mirror.
 *		Truncated area is set to zero.
 */
int MirroredFlatFile_ReconcileXLogEof(
							  char 			*subDirectory,

							  char			*simpleFileName,

							  XLogRecPtr	primaryXLogEof,

							  XLogRecPtr	*mirrorXLogEof)
{
	bool	mirrorReconcile = TRUE;
	int		status = STATUS_OK;

	if (FileRepPrimary_ReconcileXLogEof(
						FileRep_GetFlatFileIdentifier(
											  subDirectory,
											  simpleFileName),
						FileRepRelationTypeFlatFile,
						primaryXLogEof) != 0) {

		mirrorReconcile = FALSE;
		ereport(LOG,
				(errmsg("could not sent reconcile Xlog EOF request to mirror")));
		status = STATUS_ERROR;
	}

	if (mirrorReconcile) {
		if (FileRepPrimary_IsOperationCompleted(
								FileRep_GetFlatFileIdentifier(
													  subDirectory,
													  simpleFileName),
								FileRepRelationTypeFlatFile) == FALSE) {
			ereport(LOG,
					(errmsg("could not reconcile Xlog EOF on mirror")));
			status = STATUS_ERROR;

		} else {

			*mirrorXLogEof = FileRepPrimary_GetMirrorXLogEof();
			if (mirrorXLogEof->xlogid == 0 && mirrorXLogEof->xrecoff == 0) {
				ereport(LOG,
						(errmsg("could not get Xlog EOF from mirror")));
				status = STATUS_ERROR;
			}
		}
	}

	return status;
}

/*
 *
 */
int
MirrorFlatFile(
			   char 		*subDirectory,
			   char			*simpleFileName)
{
	MirroredFlatFileOpen	mirroredOpen;
	MirroredFlatFileOpen	primaryOpen;
	char					*buffer = NULL;
	int32					endOffset = 0;
	int32					startOffset = 0;
	int32					bufferLen = 0;
	int						retval = 0;
	char 			*dir = NULL, *mirrorDir = NULL;

	errno = 0;

	/*
	 * This is mainly important to set mirroredOpen.isActive to false, to
	 * avoid calling MirroredFlatFile_Close on uninitialized pointer.
	 */
	MemSet(&mirroredOpen, 0, sizeof(MirroredFlatFileOpen));

	if (isTxnDir(subDirectory))
	{
		dir = makeRelativeToTxnFilespace(subDirectory);
		mirrorDir = makeRelativeToPeerTxnFilespace(subDirectory);
	}
	else
	{
		dir = MemoryContextStrdup(TopMemoryContext, subDirectory);
		mirrorDir = MemoryContextStrdup(TopMemoryContext, subDirectory);
	}

	while (1) {

		retval = MirroredFlatFile_OpenPrimary(
											  &primaryOpen,
											  dir,
											  simpleFileName,
											  O_RDONLY | PG_BINARY,
											  S_IRUSR | S_IWUSR,
											  /* suppressError */ false);
		if (retval != 0)
			break;

		/*
		 * Determine the end.
		 */
		endOffset = MirroredFlatFile_SeekEnd(&primaryOpen);
		if (endOffset < 0) {
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not seek to end of file \"%s\" : %m",
							primaryOpen.path)));
			break;
		}

		retval = MirroredFlatFile_Drop(
									   subDirectory,
									   simpleFileName,
									   /* suppressError */ false,
									   /*isMirrorRecovery */ TRUE);
		if (retval != 0)
			break;

		retval = MirroredFlatFile_Open(
									   &mirroredOpen,
									   subDirectory,
									   simpleFileName,
									   O_CREAT | O_EXCL | O_RDWR | PG_BINARY,
									   S_IRUSR | S_IWUSR,
									   /* suppressError */ false,
									   /* atomic write/ */ false,
									   /*isMirrorRecovery */ TRUE);

		if (retval != 0)
			break;

		bufferLen = (Size) Min(BLCKSZ, endOffset - startOffset);
		buffer = (char*) palloc(bufferLen);

		MemSet(buffer, 0, bufferLen);

		while (startOffset < endOffset) {

			retval = MirroredFlatFile_Read(
										   &primaryOpen,
										   startOffset,
										   buffer,
										   bufferLen);

			if (retval != bufferLen) {
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not read from position:%d in file \"%s\" : %m",
								startOffset, primaryOpen.path)));

				break;
			}

			retval = MirroredFlatFile_Append(
											 &mirroredOpen,
											 buffer,
											 bufferLen,
											 /* suppressError */ false);

			if (retval != 0)
				break;

			startOffset += bufferLen;

			bufferLen = (Size) Min(BLCKSZ, endOffset - startOffset);
		} // while

		if (retval != 0)
			break;

		retval = MirroredFlatFile_Flush(
										&mirroredOpen,
										/* suppressError */ FALSE);
		if (retval != 0)
			break;

		break;
	} // while(1)

	if (buffer) {
		pfree(buffer);
		buffer = NULL;
	}

	if (MirroredFlatFile_IsActive(&mirroredOpen)) {
		MirroredFlatFile_Close(&mirroredOpen);
	}

	if (MirroredFlatFile_IsActive(&primaryOpen)) {
		MirroredFlatFile_ClosePrimary(&primaryOpen);
	}

	pfree(dir);
	pfree(mirrorDir);

	return retval;
}

void
MirroredFlatFile_DropFilesFromDir(void)
{
	char *mirrorXLogDir = makeRelativeToPeerTxnFilespace(XLOGDIR);
	char *mirrorCLogDir = makeRelativeToPeerTxnFilespace(CLOG_DIR);
	char *mirrorDistributedLogDir = makeRelativeToPeerTxnFilespace(DISTRIBUTEDLOG_DIR);
	char *mirrorDistributedXidMapDir = makeRelativeToPeerTxnFilespace(DISTRIBUTEDXIDMAP_DIR);
	char *mirrorMultiXactMemberDir = makeRelativeToPeerTxnFilespace(MULTIXACT_MEMBERS_DIR);
	char *mirrorMultiXactOffsetDir = makeRelativeToPeerTxnFilespace(MULTIXACT_OFFSETS_DIR);
	char *mirrorSubxactDir = makeRelativeToPeerTxnFilespace(SUBTRANS_DIR);

	FileRepPrimary_MirrorDropFilesFromDir(
										 FileRep_GetFlatFileIdentifier(TWOPHASE_DIR, ""),
										 FileRepRelationTypeFlatFile);

	FileRepPrimary_MirrorDropFilesFromDir(
										 FileRep_GetFlatFileIdentifier(mirrorCLogDir, ""),
										 FileRepRelationTypeFlatFile);

	FileRepPrimary_MirrorDropFilesFromDir(
										 FileRep_GetFlatFileIdentifier(mirrorDistributedLogDir, ""),
										 FileRepRelationTypeFlatFile);

	FileRepPrimary_MirrorDropFilesFromDir(
										 FileRep_GetFlatFileIdentifier(mirrorDistributedXidMapDir, ""),
										 FileRepRelationTypeFlatFile);

	FileRepPrimary_MirrorDropFilesFromDir(
										 FileRep_GetFlatFileIdentifier(mirrorMultiXactMemberDir, ""),
										 FileRepRelationTypeFlatFile);

	FileRepPrimary_MirrorDropFilesFromDir(
										 FileRep_GetFlatFileIdentifier(mirrorMultiXactOffsetDir, ""),
										 FileRepRelationTypeFlatFile);

	FileRepPrimary_MirrorDropFilesFromDir(
										 FileRep_GetFlatFileIdentifier(mirrorSubxactDir, ""),
										 FileRepRelationTypeFlatFile);

	FileRepPrimary_MirrorDropFilesFromDir(
										 FileRep_GetFlatFileIdentifier(mirrorXLogDir, ""),
										 FileRepRelationTypeFlatFile);

	pfree(mirrorSubxactDir);
	pfree(mirrorMultiXactOffsetDir);
	pfree(mirrorMultiXactMemberDir);
	pfree(mirrorDistributedXidMapDir);
	pfree(mirrorDistributedLogDir);
	pfree(mirrorCLogDir);
	pfree(mirrorXLogDir);

}

/*
 * Drop temporary files of pg_database, pg_auth, pg_auth_time_constraint
 */
void
MirroredFlatFile_DropTemporaryFiles(void)
{

	DIR             *global_dir;
	struct dirent   *de;
	char			path[MAXPGPATH+1];

	global_dir = AllocateDir("global");

	while ((de = ReadDir(global_dir, "global")) != NULL)
	{
		if (strstr(de->d_name, "pg_database.") != NULL ||
			strstr(de->d_name, "pg_auth.") != NULL ||
			strstr(de->d_name, "pg_auth_time_constraint.") != NULL)
		{
			sprintf(path, "%s/%s", "global", de->d_name);

			errno = 0;
			if (unlink(path))
			{
				char	tmpBuf[FILEREP_MAX_LOG_DESCRIPTION_LEN];

				snprintf(tmpBuf, sizeof(tmpBuf), "unlink failed identifier '%s' errno '%d' ",
						 de->d_name,
						 errno);

				FileRep_InsertConfigLogEntry(tmpBuf);
			}
		}
	}

	if (global_dir)
	{
		FreeDir(global_dir);
	}
}

/*
 * Send to mirror request for drop temporary files of pg_database, pg_auth, pg_auth_time_constraint
 */
void
MirroredFlatFile_MirrorDropTemporaryFiles(void)
{

	if (FileRepPrimary_MirrorDropTemporaryFiles(
												FileRep_GetFlatFileIdentifier(
																			  "global",
																			  "pg_database."),
												FileRepRelationTypeFlatFile) != 0)
	{
		ereport(LOG,
				(errmsg("could not sent drop file to mirror identifier 'global/pg_database' ")));
	}

	if (FileRepPrimary_MirrorDropTemporaryFiles(
												FileRep_GetFlatFileIdentifier(
																			  "global",
																			  "pg_auth."),
												FileRepRelationTypeFlatFile) != 0)
	{
		ereport(LOG,
				(errmsg("could not sent drop file to mirror identifier 'global/pg_auth' ")));
	}

	if (FileRepPrimary_MirrorDropTemporaryFiles(
												FileRep_GetFlatFileIdentifier(
																			  "global",
																			  "pg_auth_time_constraint."),
												FileRepRelationTypeFlatFile) != 0)
	{
		ereport(LOG,
				(errmsg("could not sent drop file to mirror identifier 'global/pg_auth_time_constraint' ")));
	}

	return;
}

/*
 *
 */
int
PgVersionRecoverMirror(void)
{
	DbDirNode dbDirNode;
	PersistentFileSysState state;

	ItemPointerData persistentTid;
	int64 persistentSerialNum;

	char *primaryFilespaceLocation = NULL;
	char *mirrorFilespaceLocation = NULL;

	char *subDirectoryPrimary;
	char *subDirectoryMirror;

	MirroredFlatFileOpen	mirroredOpen;
	MirroredFlatFileOpen	primaryOpen;
	char					*buffer = NULL;
	int32					endOffset = 0;
	int32					startOffset = 0;
	int32					bufferLen = 0;
	int						retval = 0;
	int						status = STATUS_OK;

	/*
	 * PgVersion is recovered only on mirror segment and not on master standby.
	 *
	 * Attention
	 * =========
	 * Remove the check when master mirroring will be replaced with filerep.
	 */
	getFileRepRoleAndState(&fileRepRole, NULL, NULL, NULL, NULL);

	if (fileRepRole != FileRepPrimaryRole)
	{
		return status;
	}

	subDirectoryPrimary = (char*)palloc(MAXPGPATH + 1);
	subDirectoryMirror = (char*)palloc(MAXPGPATH + 1);

	PersistentDatabase_DirIterateInit();

	while (PersistentDatabase_DirIterateNext(
											 &dbDirNode,
											 &state,
											 &persistentTid,
											 &persistentSerialNum))
	{
		if (state != PersistentFileSysState_Created)
			continue;

		PersistentTablespace_GetPrimaryAndMirrorFilespaces(
														   dbDirNode.tablespace,
														   &primaryFilespaceLocation,
														   &mirrorFilespaceLocation);

		FormDatabasePath(
						 subDirectoryPrimary,
						 primaryFilespaceLocation,
						 dbDirNode.tablespace,
						 dbDirNode.database);

		FormDatabasePath(
						 subDirectoryMirror,
						 mirrorFilespaceLocation,
						 dbDirNode.tablespace,
						 dbDirNode.database);

		errno = 0;

		while (1) {

			retval = MirroredFlatFile_OpenPrimary(
												  &primaryOpen,
												  subDirectoryPrimary,
												  PGVERSION,
												  O_RDONLY | PG_BINARY,
												  S_IRUSR | S_IWUSR,
												  /* suppressError */ true);

			if (! MirroredFlatFile_IsActive(&primaryOpen) || retval != 0) {

				/* it is expected that PG_VERSION may not exist */
				if (retval != ENOENT)
				{
					status = STATUS_ERROR;

					ereport(WARNING,
							(errcode_for_file_access(),
							 errmsg("mirror failure, "
									 "could not open file '%s' : %m "
									 "failover requested",
									 (primaryOpen.path == NULL) ? "<null>" : primaryOpen.path),
							  FileRep_errcontext()));
				}
				break;
			}

			/*
			 * Determine the end.
			 */
			endOffset = MirroredFlatFile_SeekEnd(&primaryOpen);
			if (endOffset < 0)
			{
				status = STATUS_ERROR;

				ereport(WARNING,
						(errcode_for_file_access(),
						 errmsg("mirror failure, "
								 "could not seek to end of file '%s' : %m "
								 "failover requested",
								 (primaryOpen.path == NULL) ? "<null>" : primaryOpen.path),
						  FileRep_errcontext()));

				break;
			}

			MirroredFlatFile_Drop(
								   subDirectoryMirror,
								   PGVERSION,
								   /* suppressError */ false,
								   /*isMirrorRecovery */ TRUE);

			MirroredFlatFile_Open(
								   &mirroredOpen,
								   subDirectoryMirror,
								   PGVERSION,
								   O_CREAT | O_EXCL | O_RDWR | PG_BINARY,
								   S_IRUSR | S_IWUSR,
								   /* suppressError */ false,
								   /* atomic? */ false,
								   /*isMirrorRecovery */ TRUE);

			Assert(MirroredFlatFile_IsActive(&mirroredOpen));

			bufferLen = (Size) Min(BLCKSZ, endOffset - startOffset);
			buffer = (char*) palloc(bufferLen);
			MemSet(buffer, 0, bufferLen);

			while (startOffset < endOffset)
			{

				retval = MirroredFlatFile_Read(
											   &primaryOpen,
											   startOffset,
											   buffer,
											   bufferLen);

				if (retval != bufferLen)
				{
					status = STATUS_ERROR;

					ereport(WARNING,
							(errcode_for_file_access(),
							 errmsg("mirror failure, "
									 "could not read from position '%d' file '%s' : %m "
									 "failover requested",
									 startOffset,
									 (primaryOpen.path == NULL) ? "<null>" : primaryOpen.path),
							  FileRep_errcontext()));

					break;
				}

				MirroredFlatFile_Append(
										&mirroredOpen,
										buffer,
										bufferLen,
										/* suppressError */ false);

				startOffset += bufferLen;

				bufferLen = (Size) Min(BLCKSZ, endOffset - startOffset);
			} // while

			if (status != STATUS_OK)
				break;

			MirroredFlatFile_Flush(
									&mirroredOpen,
									/* suppressError */ false);

			break;
		} // while(1)

		startOffset = 0;

		if (MirroredFlatFile_IsActive(&mirroredOpen)) {
			MirroredFlatFile_Close(&mirroredOpen);
		}

		if (MirroredFlatFile_IsActive(&primaryOpen)) {
			MirroredFlatFile_ClosePrimary(&primaryOpen);
		}

		if (primaryFilespaceLocation != NULL)
		{
			pfree(primaryFilespaceLocation);
			primaryFilespaceLocation = NULL;
		}

		if (mirrorFilespaceLocation != NULL)
		{
			pfree(mirrorFilespaceLocation);
			mirrorFilespaceLocation = NULL;
		}

		if (buffer) {
			pfree(buffer);
			buffer = NULL;
		}

	}

	PersistentDatabase_DirIterateClose();

	pfree(subDirectoryPrimary);
	pfree(subDirectoryMirror);

	return status;
}

