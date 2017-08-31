/*-------------------------------------------------------------------------
 *
 * execWorkfile.h
 *    prototypes for execworkfiles.c
 *
 * Portions Copyright (c) 2010, Greenplum Inc.
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/executor/execWorkfile.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef EXECWORKFILE_H
#define EXECWORKFILE_H

#include "utils/guc.h"

typedef enum ExecWorkFileType
{
	BUFFILE = 0,
	BFZ
} ExecWorkFileType;

/*
 * The size of a single write that OS disk drivers guaranteed to write to
 * disk.
 */ 
#define WORKFILE_SAFEWRITE_SIZE 512

/* Flags that describe capabilities of a workfile */

/* File supports random-access (seek) capabilities */
#define EXEC_WORKFILE_RANDOM_ACCESS 0x1

/* File supports suspend/restart capabilities */
#define EXEC_WORKFILE_SUSPENDABLE 0x2

/* File is marked for automatic deletion upon close */
#define EXEC_WORKFILE_DEL_ON_CLOSE 0x4

/* File was created by us */
#define EXEC_WORKFILE_CREATED 0x8

/* This file's size should be checked against the set limits during writes */
#define EXEC_WORKFILE_LIMIT_SIZE 0x10

/* 
 * ExecWorkFile structure.
 */
typedef struct ExecWorkFile
{
	ExecWorkFileType fileType;
	int compressType;
	int64 size;
	
	int flags;

	void *file;
	char *fileName;

	struct workfile_set *work_set;

} ExecWorkFile;

/*
 * ExecWorkFile_Create
 *    create a new work file with the specified name, the file type,
 * and the compression type.
 *
 * If this fails, NULL is returned.
 */
ExecWorkFile *
ExecWorkFile_Create(const char *fileName,
					ExecWorkFileType fileType,
					bool delOnClose,
					int compressType);
ExecWorkFile *
ExecWorkFile_CreateUnique(const char *filename,
		ExecWorkFileType fileType,
		bool delOnClose,
		int compressType);

StringInfo
ExecWorkFile_AddUniqueSuffix(const char *filename);

ExecWorkFile *
ExecWorkFile_Open(const char *fileName,
					ExecWorkFileType fileType,
					bool delOnClose,
					int compressType);

/*
 * ExecWorkFile_Write
 *    write the given data from the end of the last write position.
 *
 * This function returns true if the write succeeds. Otherwise, return false.
 */
bool
ExecWorkFile_Write(ExecWorkFile *workfile,
				   void *data,
				   uint64 size);

/*
 * ExecWorkFile_Read
 *    read the data with specified size to the given buffer.
 *
 * The given buffer should contain at least the space specified by 
 * 'size'.
 *
 * If the read succeeds, this function returns the number of bytes
 * that are read. Otherwise, returns 0.
 */
uint64
ExecWorkFile_Read(ExecWorkFile *workfile,
				  void *data,
				  uint64 size);

void *
ExecWorkFile_ReadFromBuffer(ExecWorkFile *workfile,
				  uint64 size);

/*
 * ExecWorkFile_Rewind
 *    rewind the pointer position to the beginning of the file.
 *
 * This function returns true if this succeeds. Otherwise, return false.
 */
bool
ExecWorkFile_Rewind(ExecWorkFile *workfile);

/*
 * ExecWorkFile_Tell64
 *    return the value of the current file position indicator.
 */
uint64
ExecWorkFile_Tell64(ExecWorkFile *workfile);

/*
 * ExecWorkFile_Close
 *    close the work file, and release the space.
 */
int64
ExecWorkFile_Close(ExecWorkFile *workfile);

int ExecWorkFile_Seek(ExecWorkFile *workfile, uint64 offset, int whence);
void ExecWorkFile_Flush(ExecWorkFile *workfile);
int64 ExecWorkFile_GetSize(ExecWorkFile *workfile);
int64 ExecWorkFile_Suspend(ExecWorkFile *workfile);
void ExecWorkFile_Restart(ExecWorkFile *workfile);
char * ExecWorkFile_GetFileName(ExecWorkFile *workfile);
void ExecWorkfile_SetWorkset(ExecWorkFile *workfile, struct workfile_set *work_set);

#endif /* EXECWORKFILE_H */
