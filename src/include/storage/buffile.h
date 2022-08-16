/*-------------------------------------------------------------------------
 *
 * buffile.h
 *	  Management of large buffered temporary files.
 *
 * The BufFile routines provide a partial replacement for stdio atop
 * virtual file descriptors managed by fd.c.  Currently they only support
 * buffered access to a virtual file, without any of stdio's formatting
 * features.  That's enough for immediate needs, but the set of facilities
 * could be expanded if necessary.
 *
 * BufFile also supports working with temporary files that exceed the OS
 * file size limit and/or the largest offset representable in an int.
 * It might be better to split that out as a separately accessible module,
 * but currently we have no need for oversize temp files without buffered
 * access.
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/storage/buffile.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef BUFFILE_H
#define BUFFILE_H

#include "storage/sharedfileset.h"
#include "utils/workfile_mgr.h"

/* BufFile is an opaque type whose details are not known outside buffile.c. */

typedef struct BufFile BufFile;

struct workfile_set;

/*
 * prototypes for functions in buffile.c
 */

extern BufFile *BufFileCreateTemp(char *operation_name, bool interXact);
extern BufFile *BufFileCreateTempInSet(char *operation_name, bool interXact, struct workfile_set *work_set);
extern void BufFileClose(BufFile *file);
extern size_t BufFileRead(BufFile *file, void *ptr, size_t size);
extern size_t BufFileWrite(BufFile *file, void *ptr, size_t size);

extern int	BufFileSeek(BufFile *file, int fileno, off_t offset, int whence);
extern void BufFileTell(BufFile *file, int *fileno, off_t *offset);
extern int	BufFileSeekBlock(BufFile *file, int64 blknum);
extern int64 BufFileSize(BufFile *file);
extern int64 BufFileGetSize(BufFile *file);
extern long BufFileAppend(BufFile *target, BufFile *source);

extern BufFile *BufFileCreateShared(SharedFileSet *fileset, const char *name, struct workfile_set *work_set);
extern void BufFileExportShared(BufFile *file);
extern BufFile *BufFileOpenShared(SharedFileSet *fileset, const char *name);
extern void BufFileDeleteShared(SharedFileSet *fileset, const char *name);

extern void *BufFileReadFromBuffer(BufFile *file, size_t size);

extern const char *BufFileGetFilename(BufFile *buffile);

extern void BufFileSuspend(BufFile *buffile);
extern void BufFileResume(BufFile *buffile);

extern bool gp_workfile_compression;
extern void BufFilePledgeSequential(BufFile *buffile);
extern void BufFileSetIsTempFile(BufFile *file, bool isTempFile);

#endif							/* BUFFILE_H */
