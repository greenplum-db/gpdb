/*-------------------------------------------------------------------------
 *
 * buffile.h
 *	  Management of large buffered files, primarily temporary files.
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
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/storage/buffile.h,v 1.25 2009/01/01 17:24:01 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */

#ifndef BUFFILE_H
#define BUFFILE_H

/* BufFile is an opaque type whose details are not known outside buffile.c. */

typedef struct BufFile BufFile;

/*
 * prototypes for functions in buffile.c
 */

extern BufFile *BufFileCreateFile(const char *filePrefix, bool delOnClose, bool interXact);
extern BufFile *BufFileOpenFile(const char * fileName, bool create, bool delOnClose, bool interXact);
extern BufFile *BufFileCreateTemp(const char *filePrefix, bool interXact);
extern BufFile *BufFileCreateTemp_ReaderWriter(const char *fileName, bool isWriter,
							   bool interXact);
extern void BufFileClose(BufFile *file);
extern Size BufFileRead(BufFile *file, void *ptr, Size size);
extern Size BufFileWrite(BufFile *file, const void *ptr, Size size);

extern int	BufFileSeek(BufFile *file, int fileno, off_t offset, int whence);
extern void BufFileTell(BufFile *file, int *fileno, off_t *offset);
extern int	BufFileSeekBlock(BufFile *file, int64 blknum);
extern void BufFileFlush(BufFile *file);
extern int64 BufFileGetSize(BufFile *buffile);
extern void BufFileSetWorkfile(BufFile *buffile);

#endif   /* BUFFILE_H */
