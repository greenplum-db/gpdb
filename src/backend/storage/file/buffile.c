/*-------------------------------------------------------------------------
 *
 * buffile.c
 *	  Management of large buffered files, primarily temporary files.
 *
 * Portions Copyright (c) 2007-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/storage/file/buffile.c
 *
 * NOTES:
 *
 * BufFiles provide a very incomplete emulation of stdio atop virtual Files
 * (as managed by fd.c).  Currently, we only support the buffered-I/O
 * aspect of stdio: a read or write of the low-level File occurs only
 * when the buffer is filled or emptied.  This is an even bigger win
 * for virtual Files than for ordinary kernel files, since reducing the
 * frequency with which a virtual File is touched reduces "thrashing"
 * of opening/closing file descriptors.
 *
 * Note that BufFile structs are allocated with palloc(), and therefore
 * will go away automatically at transaction end.  If the underlying
 * virtual File is made with OpenTemporaryFile, then all resources for
 * the file are certain to be cleaned up even if processing is aborted
 * by ereport(ERROR).  The data structures required are made in the
 * palloc context that was current when the BufFile was created, and
 * any external resources such as temp files are owned by the ResourceOwner
 * that was current at that time.
 *
 * BufFile also supports temporary files that exceed the OS file size limit
 * (by opening multiple fd.c temporary files).  This is an essential feature
 * for sorts and hashjoins on large amounts of data.
 *
 * GPDB:
 *
 * The purpose of BufFiles is the same in GPDB as in PostgreSQL, but the
 * implementation has been changed somewhat:
 *
 * - PostgreSQL breaks the files into 1 GB segments. We don't do that in
 *   GPDB. The 'fileno' argument in BufFileSeek/tell is unused.
 *
 * - There is an additional concept of "work files", and tracking their
 *   sizes by the "workfile manager". Work file is another name for
 *   a temporary file, but we track their sizes in shared memory, and
 *   enforce additional limits. Buffile code (and fd.c) has been modified
 *   to for the tracking.
 *
 * - The buffer management works slightly differently. It's not visible
 *   to callers, but ought to perform better.
 *
 * - We support compressing the files, with some limitations. See
 *   BufFilePledgeSequential().
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#ifdef HAVE_LIBZSTD
#include <zstd.h>
#endif

#include "executor/instrument.h"
#include "storage/fd.h"
#include "storage/buffile.h"
#include "storage/buf_internals.h"
#include "utils/resowner.h"

#include "cdb/cdbvars.h"
#include "storage/gp_compress.h"
#include "utils/faultinjector.h"
#include "utils/memutils.h"
#include "utils/workfile_mgr.h"

/*
 * This data structure represents a buffered file that consists of one
 * physical file (accessed through a virtual file descriptor
 * managed by fd.c).
 */
struct BufFile
{
	File		file;			/* palloc'd file */

	bool		isTemp;			/* can only add files if this is TRUE */
	bool		dirty;			/* does buffer need to be written? */

	int64		offset;			/* offset part of current pos */
	int64		pos;			/* next read/write position in buffer */

	/*
	 * resowner is the ResourceOwner to use for underlying temp files.  (We
	 * don't need to remember the memory context we're using explicitly,
	 * because after creation we only repalloc our arrays larger.)
	 */
	ResourceOwner resowner;

	int			nbytes;			/* total # of valid bytes in buffer */
	int64		maxoffset;		/* maximum offset that this file has reached, for disk usage */

	char        *buffer;        /* GPDB: PG upstream uses PGAlignedBlock */

	/*
	 * Current stage, if this is a sequential BufFile. A sequential BufFile
	 * can be written to once, and read once after that. Without compression,
	 * there is no real difference between sequential and random access
	 * buffiles, but we enforce the limitations anyway, to uncover possible
	 * bugs in sequential BufFile usage earlier.
	 */
	enum
	{
		BFS_RANDOM_ACCESS = 0,
		BFS_SEQUENTIAL_WRITING,
		BFS_SEQUENTIAL_READING,
		BFS_COMPRESSED_WRITING,
		BFS_COMPRESSED_READING
	} state;

	/* ZStandard compression support */
#ifdef HAVE_LIBZSTD
	zstd_context *zstd_context;	/* ZStandard library handles. */

	/*
	 * During compression, tracks of the original, uncompressed size. (maxoffset
	 * trackes the compressed size.
	 */
	size_t		uncompressed_bytes;

	/* This holds holds compressed input, during decompression. */
	ZSTD_inBuffer compressed_buffer;
	bool		decompression_finished;
#endif
};

static BufFile *makeBufFile(File firstfile);
static void BufFileUpdateSize(BufFile *buffile);

static void BufFileStartCompression(BufFile *file);
static void BufFileDumpCompressedBuffer(BufFile *file, const void *buffer, Size nbytes);
static void BufFileEndCompression(BufFile *file);
static int BufFileLoadCompressedBuffer(BufFile *file, void *buffer, size_t bufsize);


/*
 * Create a BufFile given the first underlying physical file.
 * NOTE: caller must set isTemp if appropriate.
 */
static BufFile *
makeBufFile(File firstfile)
{
	BufFile	   *file = (BufFile *) palloc0(sizeof(BufFile));

	file->file = firstfile;

	file->isTemp = false;
	file->dirty = false;
	/*
	 * "current pos" is a position of start of buffer within the logical file.
	 * Position as seen by user of Buffile is (offset+pos).
	 * */
	file->offset = 0L;
	file->resowner = CurrentResourceOwner;
	file->pos = 0;
	file->nbytes = 0;
	file->maxoffset = 0L;
	file->buffer = palloc(BLCKSZ);

	return file;
}


/*
 * Create a BufFile for a new temporary file.
 *
 * Adds the pgsql_tmp/ prefix to the file path before creating.
 *
 * Note: if interXact is true, the caller had better be calling us in a
 * memory context that will survive across transaction boundaries.
 *
 * A unique filename will be chosen.
 */
BufFile *
BufFileCreateTemp(char *operation_name, bool interXact)
{
	workfile_set *work_set;

	work_set = workfile_mgr_create_set(operation_name, NULL);

	return BufFileCreateTempInSet(work_set, interXact);
}


BufFile *
BufFileCreateTempInSet(workfile_set *work_set, bool interXact)
{
	BufFile	   *file;
	File		pfile;
	char		filePrefix[MAXPGPATH];

	snprintf(filePrefix, MAXPGPATH, "_%s_", work_set->prefix);

	pfile = OpenTemporaryFile(interXact, filePrefix);
	Assert(pfile >= 0);

	file = makeBufFile(pfile);
	file->isTemp = true;

	FileSetIsWorkfile(file->file);
	RegisterFileWithSet(file->file, work_set);

	SIMPLE_FAULT_INJECTOR("workfile_creation_failure");

	return file;
}


/*
 * Create a BufFile for a new file.
 *
 * Adds the pgsql_tmp/ prefix to the file path before creating.
 *
 * If interXact is true, the temp file will not be automatically deleted
 * at end of transaction.
 *
 * Note: if interXact is true, the caller had better be calling us in a
 * memory context, and with a resource owner, that will survive across
 * transaction boundaries.
 *
 * This variant creates the file with the exact given name. The caller
 * must ensure that it's unique. If 'work_set' is given, the file is
 * associated with that workfile set. Otherwise, it is not tracked
 * by the workfile manager.
 */
BufFile *
BufFileCreateNamedTemp(const char *fileName, bool interXact, workfile_set *work_set)
{
	File		pfile;
	BufFile	   *file;

	pfile = OpenNamedTemporaryFile(fileName,
								   true, /* create */
								   true, /* delOnClose */
								   interXact);
	Assert(pfile >= 0);

	file = makeBufFile(pfile);
	file->isTemp = true;

	if (work_set)
	{
		FileSetIsWorkfile(file->file);
		RegisterFileWithSet(file->file, work_set);
	}

	return file;
}

/*
 * Opens an existing file as BufFile
 *
 * Adds the pgsql_tmp/ prefix to the file path before opening.
 */
BufFile *
BufFileOpenNamedTemp(const char *fileName, bool interXact)
{
	File		pfile;
	BufFile	   *file;

	pfile = OpenNamedTemporaryFile(fileName,
								   false,	/* create */
								   false,	/* delOnClose */
								   interXact);
	/*
	 * If we are trying to open an existing file and it failed,
	 * signal this to the caller.
	 */
	if (pfile <= 0)
		return NULL;

	Assert(pfile >= 0);

	file = makeBufFile(pfile);
	file->isTemp = false;

	/* Open existing file, initialize its size */
	file->maxoffset = FileDiskSize(file->file);

	return file;
}

/*
 * Close a BufFile
 *
 * Like fclose(), this also implicitly FileCloses the underlying File.
 */
void
BufFileClose(BufFile *file)
{
	/* flush any unwritten data */
	if (!file->isTemp)
	{
		/* This can thrown an exception */
		BufFileFlush(file);
	}

	FileClose(file->file);

	/* release the buffer space */
	if (file->buffer)
		pfree(file->buffer);

	/* release zstd handles */
#ifdef HAVE_LIBZSTD
	if (file->zstd_context)
		zstd_free_context(file->zstd_context);
#endif

	pfree(file);
}

/*
 * BufFileLoadBuffer
 *
 * Load some data into buffer, if possible, starting from curOffset.
 * At call, must have dirty = false, pos and nbytes = 0.
 * On exit, nbytes is number of bytes loaded.
 */
static int
BufFileLoadBuffer(BufFile *file, void* buffer, size_t bufsize)
{
	int			nb;

	/*
	 * May need to reposition physical file.
	 */
	if (FileSeek(file->file, file->offset, SEEK_SET) != file->offset)
	{
		elog(ERROR, "could not seek in temporary file: %m");
	}

	/*
	 * Read whatever we can get, up to a full bufferload.
	 */
	nb = FileRead(file->file, buffer, (int)bufsize);
	if (nb < 0)
	{
		elog(ERROR, "could not read from temporary file: %m");
	}

	/* we choose not to advance curOffset here */

	pgBufferUsage.temp_blks_read++;

	return nb;
}

/*
 * BufFileDumpBuffer
 *
 * Dump buffer contents starting at curOffset.
 * At call, should have dirty = true, nbytes > 0.
 * On exit, dirty is cleared if successful write, and curOffset is advanced.
 */
static void
BufFileDumpBuffer(BufFile *file, const void* buffer, Size nbytes)
{
	size_t wpos = 0;
	size_t bytestowrite;
	int wrote = 0;

	/*
	 * Unlike BufFileLoadBuffer, we must dump the whole buffer.
	 */
	while (wpos < nbytes)
	{
		bytestowrite = nbytes - wpos;

		if (FileSeek(file->file, file->offset, SEEK_SET) != file->offset)
		{
			elog(ERROR, "could not seek in temporary file: %m");
		}

		wrote = FileWrite(file->file, (char *)buffer + wpos, (int)bytestowrite);
		if (wrote != bytestowrite)
			elog(ERROR, "could not write %d bytes to temporary file: %m", (int) bytestowrite);
		file->offset += wrote;
		wpos += wrote;

		pgBufferUsage.temp_blks_written++;
	}
	file->dirty = false;

	/*
	 * Now we can set the buffer empty without changing the logical position
	 */
	file->pos = 0;
	file->nbytes = 0;
}

/*
 * BufFileRead
 *
 * Like fread() except we assume 1-byte element size.
 */
size_t
BufFileRead(BufFile *file, void *ptr, size_t size)
{
	size_t		nread = 0;
	size_t		nthistime;

	switch (file->state)
	{
		case BFS_RANDOM_ACCESS:
		case BFS_SEQUENTIAL_READING:
			break;

		case BFS_SEQUENTIAL_WRITING:
		case BFS_COMPRESSED_WRITING:
			elog(ERROR, "cannot read from sequential BufFile before rewinding to start");
			break;

		case BFS_COMPRESSED_READING:
			return BufFileLoadCompressedBuffer(file, ptr, size);
	}

	if (file->dirty)
		BufFileFlush(file);

	while (size > 0)
	{
		if (file->pos >= file->nbytes)
		{
			Assert(file->pos == file->nbytes);

			file->offset += file->pos;
			file->pos = 0;
			file->nbytes = 0;

			/*
			 * Read full blocks directly into caller's buffer.
			 */
			while (size >= BLCKSZ)
			{
				size_t nwant;

				nwant = size - size % BLCKSZ;

				nthistime = BufFileLoadBuffer(file, ptr, nwant);
				file->offset += nthistime;
				ptr = (char *) ptr + nthistime;
				size -= nthistime;
				nread += nthistime;

				if (size == 0 || nthistime == 0)
				{
					return nread;
				}
			}

			/* Try to load more data into buffer. */
			file->nbytes = BufFileLoadBuffer(file, file->buffer, BLCKSZ);
			if (file->nbytes == 0)
			{
				break; /* no more data available */
			}
		}

		nthistime = file->nbytes - file->pos;
		if (nthistime > size)
		{
			nthistime = (int) size;
		}

		Assert(nthistime > 0);

		memcpy(ptr, file->buffer + file->pos, nthistime);

		file->pos += nthistime;
		ptr = (void *) ((char *) ptr + nthistime);
		size -= nthistime;
		nread += nthistime;
	}

	return nread;
}

/*
 * BufFileReadFromBuffer
 *
 * This function provides a faster implementation of Read which applies
 * when the data is already in the underlying buffer.
 * In that case, it returns a pointer to the data in the buffer
 * If the data is not in the buffer, returns NULL and the caller must
 * call the regular BufFileRead with a destination buffer.
 */
void *
BufFileReadFromBuffer(BufFile *file, size_t size)
{
	void	   *result = NULL;

	switch (file->state)
	{
		case BFS_RANDOM_ACCESS:
		case BFS_SEQUENTIAL_READING:
			break;

		case BFS_SEQUENTIAL_WRITING:
		case BFS_COMPRESSED_WRITING:
			elog(ERROR, "cannot read from sequential BufFile before rewinding to start");
			return NULL;

		case BFS_COMPRESSED_READING:
			return NULL;
	}

	if (file->dirty)
		BufFileFlush(file);

	if (file->pos + size < file->nbytes)
	{
		result = file->buffer + file->pos;
		file->pos += size;
	}

	return result;
}

/*
 * BufFileWrite
 *
 * Like fwrite() except we assume 1-byte element size.
 */
size_t
BufFileWrite(BufFile *file, const void *ptr, size_t size)
{
	size_t		nwritten = 0;
	size_t		nthistime;

	SIMPLE_FAULT_INJECTOR("workfile_write_failure");

	switch (file->state)
	{
		case BFS_RANDOM_ACCESS:
		case BFS_SEQUENTIAL_WRITING:
			break;

		case BFS_COMPRESSED_WRITING:
			BufFileDumpCompressedBuffer(file, ptr, size);
			return size;

		case BFS_SEQUENTIAL_READING:
		case BFS_COMPRESSED_READING:
			elog(ERROR, "cannot write to sequential BufFile after reading");
	}

	while (size > 0)
	{
		if ((size_t) file->pos >= BLCKSZ)
		{
			Assert((size_t)file->pos == BLCKSZ);

			/* Buffer full, dump it out */
			if (file->dirty)
			{
				/* This can throw an exception, but it correctly updates the size when that happens */
				BufFileDumpBuffer(file, file->buffer, file->nbytes);
			}
			else
			{
				/* Hmm, went directly from reading to writing? */
				file->offset += file->pos;
				file->pos = 0;
				file->nbytes = 0;
			}
		}

		/*
		 * Write full blocks directly from caller's buffer.
		 */
		if (size >= BLCKSZ && file->pos == 0)
		{
			nthistime = size - size % BLCKSZ;

			/* This can throw an exception, but it correctly updates the size when that happens */
			BufFileDumpBuffer(file, ptr, nthistime);

			ptr = (void *) ((char *) ptr + nthistime);
			size -= nthistime;
			nwritten += nthistime;

			BufFileUpdateSize(file);

			if (size == 0)
			{
				return nwritten;
			}
		}

		nthistime = BLCKSZ - file->pos;
		if (nthistime > size)
			nthistime = size;
		Assert(nthistime > 0);

		memcpy(file->buffer + file->pos, ptr, nthistime);

		file->dirty = true;
		file->pos += (int) nthistime;
		if (file->nbytes < file->pos)
			file->nbytes = file->pos;
		ptr = (void *) ((char *) ptr + nthistime);
		size -= nthistime;
		nwritten += nthistime;
	}

	BufFileUpdateSize(file);
	return nwritten;
}

/*
 * BufFileFlush
 *
 * Like fflush()
 */
void
BufFileFlush(BufFile *file)
{
	switch (file->state)
	{
		case BFS_RANDOM_ACCESS:
		case BFS_SEQUENTIAL_WRITING:
			break;

		case BFS_COMPRESSED_WRITING:
			BufFileEndCompression(file);
			break;

		case BFS_SEQUENTIAL_READING:
		case BFS_COMPRESSED_READING:
			/* no-op. */
			return;
	}

	if (file->dirty)
	{
		int nbytes = file->nbytes;
		int pos = file->pos;

		BufFileDumpBuffer(file, file->buffer, nbytes);

		/*
		 * At this point, curOffset has been advanced to the end of the buffer,
		 * ie, its original value + nbytes.  We need to make it point to the
		 * logical file position, ie, original value + pos, in case that is less
		 * (as could happen due to a small backwards seek in a dirty buffer!)
		 */
		file->offset -= (nbytes - pos);
		BufFileUpdateSize(file);
	}
}

/*
 * BufFileSeek
 *
 * Like fseek(), except that target position needs two values in order to
 * work when logical filesize exceeds maximum value representable by long.
 * We do not support relative seeks across more than LONG_MAX, however.
 *
 * Result is 0 if OK, EOF if not.  Logical position is not moved if an
 * impossible seek is attempted.
 */
int
BufFileSeek(BufFile *file, int fileno, off_t offset, int whence)
{
	int64 newOffset;

	switch (file->state)
	{
		case BFS_RANDOM_ACCESS:
			break;

		case BFS_SEQUENTIAL_WRITING:
			/*
			 * We have been writing. The uncompressed sequential mode is the
			 * same as uncompressed, but we check that the caller doesn't try
			 * to do random access after pledging sequential mode.
			 */
			if (fileno != 0 || offset != 0 || whence != SEEK_SET)
				elog(ERROR, "invalid seek in sequential BufFile");
			break;

		case BFS_COMPRESSED_WRITING:
			/* We have been writing. Flush the last data, and switch to reading mode */
			if (fileno != 0 || offset != 0 || whence != SEEK_SET)
				elog(ERROR, "invalid seek in sequential BufFile");
			BufFileEndCompression(file);
			file->offset = 0;
			file->pos = 0;
			file->nbytes = 0;
			return 0;

		case BFS_COMPRESSED_READING:
		case BFS_SEQUENTIAL_READING:
			elog(ERROR, "cannot seek in sequential BufFile");
	}

	/* GPDB doesn't support multiple files */
	Assert(fileno == 0);

	switch (whence)
	{
		case SEEK_SET:
			newOffset = offset;
			break;

		case SEEK_CUR:
			newOffset = (file->offset + file->pos) + offset;
			break;

#ifdef NOT_USED
		case SEEK_END:
			/* could be implemented, not needed currently */
			break;
#endif
		default:
			elog(ERROR, "invalid whence: %d", whence);
			return EOF;
	}

	if (newOffset < 0)
	{
		newOffset = file->offset - newOffset;
		return EOF;
	}

	if (newOffset >= file->offset &&
		newOffset <= file->offset + file->nbytes)
	{
		/*
		 * Seek is to a point within existing buffer; we can just adjust
		 * pos-within-buffer, without flushing buffer.  Note this is OK
		 * whether reading or writing, but buffer remains dirty if we were
		 * writing.
		 */
		file->pos = (newOffset - file->offset);
		BufFileUpdateSize(file);
		return 0;
	}
	/* Otherwise, must reposition buffer, so flush any dirty data */
	BufFileFlush(file);

	/* Seek is OK! */
	file->offset = newOffset;
	file->pos = 0;
	file->nbytes = 0;
	BufFileUpdateSize(file);
	return 0;
}

void
BufFileTell(BufFile *file, int *fileno, off_t *offset)
{
	if (file->state != BFS_RANDOM_ACCESS)
		elog(ERROR, "cannot tell current position in sequential BufFile");

	if (fileno != NULL)
	{
		*fileno = 0;
	}
	*offset = file->offset + file->pos;
}

/*
 * BufFileSeekBlock --- block-oriented seek
 *
 * Performs absolute seek to the start of the n'th BLCKSZ-sized block of
 * the file.  Note that users of this interface will fail if their files
 * exceed BLCKSZ * LONG_MAX bytes, but that is quite a lot; we don't work
 * with tables bigger than that, either...
 *
 * Result is 0 if OK, EOF if not.  Logical position is not moved if an
 * impossible seek is attempted.
 */
int
BufFileSeekBlock(BufFile *file, int64 blknum)
{
	return BufFileSeek(file, 0 /* fileno */, blknum * BLCKSZ, SEEK_SET);
}

/*
 * BufFileUpdateSize
 *
 * Updates the total disk size of a BufFile after a write
 *
 * Some of the data might still be in the buffer and not on disk, but we count
 * it here regardless
 */
static void
BufFileUpdateSize(BufFile *buffile)
{
	Assert(NULL != buffile);

	if (buffile->offset + buffile->pos > buffile->maxoffset)
	{
		buffile->maxoffset = buffile->offset + buffile->pos;
	}
}

/*
 * Returns the size of this file according to current accounting.
 *
 * For a compressed BufFile, this returns the uncompressed size!
 */
int64
BufFileGetSize(BufFile *buffile)
{
	Assert(NULL != buffile);

	switch (buffile->state)
	{
		case BFS_RANDOM_ACCESS:
		case BFS_SEQUENTIAL_WRITING:
		case BFS_SEQUENTIAL_READING:
			break;
		case BFS_COMPRESSED_WRITING:
		case BFS_COMPRESSED_READING:
#ifdef HAVE_LIBZSTD
			return buffile->uncompressed_bytes;
#else
			Assert(false);
			break;
#endif
	}

	BufFileUpdateSize(buffile);
	return buffile->maxoffset;
}

const char *
BufFileGetFilename(BufFile *buffile)
{
	return FileGetFilename(buffile->file);
}

void
BufFileSuspend(BufFile *buffile)
{
	switch (buffile->state)
	{
		case BFS_RANDOM_ACCESS:
		case BFS_SEQUENTIAL_WRITING:
			break;
		case BFS_COMPRESSED_WRITING:
			return BufFileEndCompression(buffile);

		case BFS_SEQUENTIAL_READING:
		case BFS_COMPRESSED_READING:
			elog(ERROR, "cannot suspend a sequential BufFile after reading");
	}

	BufFileFlush(buffile);
	pfree(buffile->buffer);
	buffile->buffer = NULL;
	buffile->nbytes = 0;
}

void
BufFileResume(BufFile *buffile)
{
	switch (buffile->state)
	{
		case BFS_RANDOM_ACCESS:
		case BFS_SEQUENTIAL_READING:
			break;

		case BFS_COMPRESSED_READING:
			/* no buffer needed */
			return;

		case BFS_SEQUENTIAL_WRITING:
		case BFS_COMPRESSED_WRITING:
			elog(ERROR, "cannot resume a sequential BufFile that is still writing");
			break;
	}

	Assert(buffile->buffer == NULL);
	buffile->buffer = palloc(BLCKSZ);

	BufFileSeek(buffile, 0, 0, SEEK_SET);
}


/*
 * ZStandard Compression support
 */

bool gp_workfile_compression;		/* GUC */

/*
 * BufFilePledgeSequential
 *
 * Promise that the caller will only do sequential I/O on the given file.
 * This allows the BufFile to be compressed, if 'gp_workfile_compression=on'.
 *
 * A sequential file is used in two stages:
 *
 * 0. Create file with BufFileCreateTemp().
 * 1. Write all data, using BufFileWrite()
 * 2. Rewind to beginning, with BufFileSeek(file, 0, 0, SEEK_SET).
 * 3. Read as much as you want with BufFileRead()
 * 4. BufFileClose()
 *
 * Trying to do arbitrary seeks
 *
 * A sequential file that is to be passed between processes, using
 * BufFileCreateNamedTemp/BufFileOpenNamedTemp(), can also be used in
 * sequential mode. If the file was pledged as sequential when creating
 * it, the reading side must also pledge sequential access after calling
 * BufFileOpenNamedTemp(). Otherwise, the reader might try to read a
 * compressed file as uncompressed. (As of this writing, none of the callers
 * that use buffiles across processes pledge sequential access, though.)
 */
void
BufFilePledgeSequential(BufFile *buffile)
{
	if (buffile->maxoffset != 0)
		elog(ERROR, "cannot pledge sequential access to a temporary file after writing it");

	if (gp_workfile_compression)
		BufFileStartCompression(buffile);
}

/*
 * The rest of the code is only needed when compression support is compiled in.
 */
#ifdef HAVE_LIBZSTD

#define BUFFILE_ZSTD_COMPRESSION_LEVEL 1

/*
 * Temporary buffer used during compresion. It's used only within the
 * functions, so we can allocate this once and reuse it for all files.
 */
static char *compression_buffer;

/*
 * Initialize the compressor.
 */
static void
BufFileStartCompression(BufFile *file)
{
	ResourceOwner oldowner;

	/*
	 * When working with compressed files, we rely on libzstd's buffer,
	 * and the BufFile's own buffer is unused. It's a bit silly that we
	 * allocate it in makeBufFile(), just to free it here again, but it
	 * doesn't seem worth the trouble to avoid that either.
	 */
	if (file->buffer)
	{
		pfree(file->buffer);
		file->buffer = NULL;
	}

	if (compression_buffer == NULL)
		compression_buffer = MemoryContextAlloc(TopMemoryContext, BLCKSZ);

	/*
	 * Make sure the zstd handle is kept in the same resource owner as
	 * the underlying file. In the typical use, when BufFileCompressOK is
	 * called immediately after opening the file, this wouldn't be
	 * necessary, but better safe than sorry.
	 */
	oldowner = CurrentResourceOwner;
	CurrentResourceOwner = file->resowner;

	file->zstd_context = zstd_alloc_context();
	file->zstd_context->cctx = ZSTD_createCStream();
	ZSTD_initCStream(file->zstd_context->cctx, BUFFILE_ZSTD_COMPRESSION_LEVEL);

	CurrentResourceOwner = oldowner;

	file->state = BFS_COMPRESSED_WRITING;
}

static void
BufFileDumpCompressedBuffer(BufFile *file, const void *buffer, Size nbytes)
{
	ZSTD_inBuffer input;

	file->uncompressed_bytes += nbytes;

	/*
	 * Call ZSTD_compressStream() until all the input has been consumed.
	 */
	input.src = buffer;
	input.size = nbytes;
	input.pos = 0;
	while (input.pos < input.size)
	{
		ZSTD_outBuffer output;
		size_t		ret;

		output.dst = compression_buffer;
		output.size = BLCKSZ;
		output.pos = 0;

		ret = ZSTD_compressStream(file->zstd_context->cctx, &output, &input);
		if (ZSTD_isError(ret))
			elog(ERROR, "%s", ZSTD_getErrorName(ret));

		if (output.pos > 0)
		{
			int			wrote;

			wrote = FileWrite(file->file, output.dst, output.pos);
			if (wrote != output.pos)
				elog(ERROR, "could not write %d bytes to compressed temporary file: %m", (int) output.pos);
			file->maxoffset += wrote;
		}
	}
}

/*
 * End compression stage. Rewind and prepare the BufFile for decompression.
 */
static void
BufFileEndCompression(BufFile *file)
{
	ZSTD_outBuffer output;
	size_t		ret;
	int			wrote;

	Assert(file->state == BFS_COMPRESSED_WRITING);

	do {
		output.dst = compression_buffer;
		output.size = BLCKSZ;
		output.pos = 0;

		ret = ZSTD_endStream(file->zstd_context->cctx, &output);
		if (ZSTD_isError(ret))
			elog(ERROR, "%s", ZSTD_getErrorName(ret));

		wrote = FileWrite(file->file, output.dst, output.pos);
		if (wrote != output.pos)
			elog(ERROR, "could not write %d bytes to compressed temporary file: %m", (int) output.pos);
		file->maxoffset += wrote;
	} while (ret > 0);

	ZSTD_freeCCtx(file->zstd_context->cctx);
	file->zstd_context->cctx = NULL;

	elog(DEBUG1, "BufFile compressed from %ld to %ld bytes",
		 file->uncompressed_bytes, file->maxoffset);

	/* Done writing. Initialize for reading */
	file->zstd_context->dctx = ZSTD_createDStream();
	ZSTD_initDStream(file->zstd_context->dctx);

	file->compressed_buffer.src = palloc(BLCKSZ);
	file->compressed_buffer.size = 0;
	file->compressed_buffer.pos = 0;
	file->offset = 0;
	file->state = BFS_COMPRESSED_READING;

	if (FileSeek(file->file, 0, SEEK_SET) != 0)
		elog(ERROR, "could not seek in temporary file: %m");
}

static int
BufFileLoadCompressedBuffer(BufFile *file, void *buffer, size_t bufsize)
{
	ZSTD_outBuffer output;
	size_t		ret;
	bool		eof = false;

	if (file->decompression_finished)
		return 0;

	/* Initialize Zstd output buffer. */
	output.dst = buffer;
	output.size = bufsize;
	output.pos = 0;

	do
	{
		/* No more compressed input? Load some. */
		if (file->compressed_buffer.pos == file->compressed_buffer.size)
		{
			int			nb;

			nb = FileRead(file->file, (char *) file->compressed_buffer.src, BLCKSZ);
			if (nb < 0)
			{
				elog(ERROR, "could not read from temporary file: %m");
			}
			file->compressed_buffer.size = nb;
			file->compressed_buffer.pos = 0;

			if (nb == 0)
				eof = true;
		}

		/* Decompress, and check result */
		ret = ZSTD_decompressStream(file->zstd_context->dctx, &output, &file->compressed_buffer);
		if (ZSTD_isError(ret))
			elog(ERROR, "zstd decompression failed: %s", ZSTD_getErrorName(ret));

		if (ret == 0)
		{
			/* End of compressed data. */
			Assert (file->compressed_buffer.pos == file->compressed_buffer.size);
			file->decompression_finished = true;
			break;
		}

		if (ret > 0 && eof && output.pos < output.size)
		{
			/*
			 * We ran out of compressed input, but Zstd expects more. File was
			 * truncated on disk after we wrote it?
			 */
			elog(ERROR, "unexpected end of compressed temporary file");
		}
	}
	while (output.pos < output.size);

	return output.pos;
}
#else		/* HAVE_ZSTD */

/*
 * Dummy versions of the compression functions, when the server is built
 * without libzstd. gp_workfile_compression cannot be enabled without
 * libzstd - there's a GUC assign hook to check that - so these should
 * never be called. They exists just to avoid having so many #ifdefs in
 * the code.
 */
static void
BufFileStartCompression(BufFile *file)
{
	elog(ERROR, "zstandard compression not supported by this build");
}
static void
BufFileDumpCompressedBuffer(BufFile *file, const void *buffer, Size nbytes)
{
	elog(ERROR, "zstandard compression not supported by this build");
}
static void
BufFileEndCompression(BufFile *file)
{
	elog(ERROR, "zstandard compression not supported by this build");
}
static int
BufFileLoadCompressedBuffer(BufFile *file, void *buffer, size_t bufsize)
{
	elog(ERROR, "zstandard compression not supported by this build");
}

#endif		/* HAVE_ZSTD */
