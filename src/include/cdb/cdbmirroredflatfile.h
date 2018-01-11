/*-------------------------------------------------------------------------
 *
 * cdbmirroredflatfile.h
 *
 * Portions Copyright (c) 2009-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/cdb/cdbmirroredflatfile.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef CDBMIRROREDFLATFILE_H
#define CDBMIRROREDFLATFILE_H

#include "access/xlogdefs.h"
#include "storage/fd.h"
#include "storage/dbdirnode.h"

/*
 * This module is for doing mirrored writes for system flat files being written
 * by multiple backend process.
 *
 * It is must manage a shared open to the mirror sender.
 */

/*
 * This structure contains write open information.  Consider the fields
 * inside to be private.
 */

/*
 * This structure contains write open information.  Consider the fields
 * inside to be private.
 */
typedef struct MirroredFlatFileOpen
{
	bool	isActive;

	bool	usingDbDirNode;

	DbDirNode 	dbDirNode;

	char 	*subDirectory;

	char    *simpleFileName;

	char 	*path;

	File	primaryFile;

	int32	appendPosition;
	
	bool	isMirrorRecovery;

	/* 
	 * Perform the entire operation atomically. We implement this by writing to
	 * a temporary file and then renaming.
	 */
	bool	atomic_op;
	char   *atomicSimpleFileName; /* original file that will be replaced with temporary file */

} MirroredFlatFileOpen;

#define MirroredFlatFileOpen_Init {false, false, {0, 0}, NULL, NULL, NULL, -1, 0, false, false, NULL}

// -----------------------------------------------------------------------------
// CleanMirrorDirectory, Create, Open, Flush, and Close 
// -----------------------------------------------------------------------------

#ifdef suppress
/*
 * Remove all files in the mirror's directory.
 */
extern void MirroredFlatFile_CleanMirrorDirectory(
	char 			*directorySimpleName);
				/* The simple name of the directory in the instance database directory. */
#endif

/*
 * Open a relation for mirrored write.
 */
extern int MirroredFlatFile_Open(
	MirroredFlatFileOpen *open,
				/* The resulting open struct. */

	char 			*subDirectory,
	
	char			*simpleFileName,
				/* The simple file name. */

	int 			fileFlags,

	int 			fileMode,

	bool			suppressError,

	bool			atomic_op,

	bool			isMirrorRecovery);

/*
 * Open a relation for mirrored write.
 */
extern int MirroredFlatFile_OpenInDbDir(
	MirroredFlatFileOpen *open,
				/* The resulting open struct. */

	DbDirNode 			*dbDirNode,
	
	char			*simpleFileName,
				/* The simple file name. */

	int 			fileFlags,

	int 			fileMode,

	bool			suppressError);


/*
 * Flush a flat file.
 */
extern int MirroredFlatFile_Flush(
	MirroredFlatFileOpen *open,
				/* The open struct. */

	bool				 suppressError);

				
extern bool MirroredFlatFile_IsActive(
	MirroredFlatFileOpen *open);
				/* The open struct. */
/*
 * Close a flat file.
 */
extern void MirroredFlatFile_Close(
	MirroredFlatFileOpen *open);
				/* The open struct. */
// -----------------------------------------------------------------------------
// Rename
// -----------------------------------------------------------------------------

extern int MirroredFlatFile_Rename(
							char 			*subDirectory,
							
							char			*oldSimpleFileName,
							/* The simple file name. */
							
							char			*newimpleFileName,
							/* The simple file name. */
							
							bool			suppressError,
							
							bool			isMirrorRecovery);
				
								
// -----------------------------------------------------------------------------
// Append and Write 
// -----------------------------------------------------------------------------

/*
* Append to a mirrored flat file.
*
* Assumed to start with an empty just created file at ~_Open.
*/
extern int MirroredFlatFile_Append(
	MirroredFlatFileOpen *open,
				/* The open struct. */

	void		*data,
				/* Pointer to the data. */

	int32		dataLen,
				/* The byte length of the data. */

	bool		suppressError);

/*
 * Write a mirrored flat file.
 */
extern int MirroredFlatFile_Write(
	MirroredFlatFileOpen *open,
				/* The open struct. */

	int32		position,
				/* The position to write the data. */

	void		*data,
				/* Pointer to the data. */

	int32		dataLen,
				/* The byte length of the data. */

	bool		suppressError);

extern int MirroredFlatFile_OpenPrimary(
				 MirroredFlatFileOpen *open,
				 /* The resulting open struct. */
				 
				 char 			*subDirectory,
				 
				 char			*simpleFileName,
				 /* The simple file name. */
				 
				 int 			fileFlags, 
				 
				 int 			fileMode,
				 
				 bool			suppressError);

extern void MirroredFlatFile_ClosePrimary(
								  MirroredFlatFileOpen *open);

extern int MirroredFlatFile_Read(
	MirroredFlatFileOpen *open,
				/* The open struct. */

	int32		position,

	void		*buffer,
				/* Pointer to the Buffer Pool page, properly protected by locks. */

	int32		bufferLen);

extern int64 MirroredFlatFile_SeekSet(
	MirroredFlatFileOpen *open,
				/* The open struct. */

	int32		position);

extern int64 MirroredFlatFile_SeekEnd(
									  MirroredFlatFileOpen *open);

// -----------------------------------------------------------------------------
// Drop 
// -----------------------------------------------------------------------------

/*
 * Mirrored drop.
 */
extern int MirroredFlatFile_Drop(
	char 			*subDirectory,
	
	char			*simpleFileName,
				/* The simple file name. */

	bool			suppressError,

	bool			isMirrorRecovery);


// -----------------------------------------------------------------------------
// Reconcile XLog Eof
// -----------------------------------------------------------------------------

/*
 * Mirrored reconcile.
 */
extern int MirroredFlatFile_ReconcileXLogEof(	
						  char 			*subDirectory,
						  
						  char			*simpleFileName,
						  
						  XLogRecPtr	primaryXLogEof,
						  
						  XLogRecPtr	*mirrorXLogEof);


extern int MirrorFlatFile(
			   char 		*subDirectory,
			   char			*simpleFileName);

extern void MirroredFlatFile_DropFilesFromDir(void);

extern void MirroredFlatFile_DropTemporaryFiles(void);

extern void MirroredFlatFile_MirrorDropTemporaryFiles(void);

extern int PgVersionRecoverMirror(void);

#endif   /* CDBMIRROREDFLATFILE_H */

