/*-------------------------------------------------------------------------
 *
 * buffile.c
 *	  Management of large buffered temporary files.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
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
 * will go away automatically at query/transaction end.  Since the underlying
 * virtual Files are made with OpenTemporaryFile, all resources for
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
 * BufFile supports temporary files that can be made read-only and shared with
 * other backends, as infrastructure for parallel execution.  Such files need
 * to be created as a member of a SharedFileSet that all participants are
 * attached to.
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#ifdef HAVE_LIBZSTD
#include <zstd.h>
#endif

#include "commands/tablespace.h"
#include "executor/instrument.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/fd.h"
#include "storage/buffile.h"
#include "storage/buf_internals.h"
#include "utils/resowner.h"

#include "storage/gp_compress.h"
#include "utils/faultinjector.h"
#include "utils/workfile_mgr.h"

/*
 * We break BufFiles into gigabyte-sized segments, regardless of RELSEG_SIZE.
 * The reason is that we'd like large BufFiles to be spread across multiple
 * tablespaces when available.
 */
#define MAX_PHYSICAL_FILESIZE	0x40000000 
#define BUFFILE_SEG_SIZE		(MAX_PHYSICAL_FILESIZE / BLCKSZ)

/* To align upstream's structure, minimize the code differences */
typedef union FakeAlignedBlock
{
	/*
	 * Greenplum uses char * so it could suspend and resume, to give the hash
	 * table as much space as possible.
	 */
	char *data;
} FakeAlignedBlock;

/*
 * This data structure represents a buffered file that consists of one or
 * more physical files (each accessed through a virtual file descriptor
 * managed by fd.c).
 */
struct BufFile
{
	int			numFiles;		/* number of physical files in set */
	/* all files except the last have length exactly MAX_PHYSICAL_FILESIZE */
	File	   *files;			/* palloc'd array with numFiles entries */

	bool		isInterXact;	/* keep open over transactions? */
	bool		dirty;			/* does buffer need to be written? */
	bool		readOnly;		/* has the file been set to read only? */

	char	   *operation_name; /* for naming temporary files. */

	SharedFileSet *fileset;		/* space for segment files if shared */
	const char *name;			/* name of this BufFile if shared */

	/*
	 * workfile_set for the files in current buffile. The workfile_set creator
	 * should take care of the workfile_set's lifecycle. So, no need to call
	 * workfile_mgr_close_set under the buffile logic.
	 * If the workfile_set is created in BufFileCreateTemp. The workfile_set
	 * should get freed once all the files in it are closed in BufFileClose.
	 */
	workfile_set *work_set;

	/*
	 * resowner is the ResourceOwner to use for underlying temp files.  (We
	 * don't need to remember the memory context we're using explicitly,
	 * because after creation we only repalloc our arrays larger.)
	 */
	ResourceOwner resowner;

	/*
	 * "current pos" is position of start of buffer within the logical file.
	 * Position as seen by user of BufFile is (curFile, curOffset + pos).
	 */
	int			curFile;		/* file index (0..n) part of current pos */
	off_t		curOffset;		/* offset part of current pos */
	off_t		pos;			/* next read/write position in buffer */
	int64		nbytes;			/* total # of valid bytes in buffer */
	FakeAlignedBlock buffer;	/* GPDB: PG upstream uses PGAlignedBlock */

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
	 * During compression, tracks of the original, uncompressed size.
	 */
	size_t		uncompressed_bytes;

	/* This holds compressed input, during decompression. */
	ZSTD_inBuffer compressed_buffer;
	bool		decompression_finished;
#endif
};

static BufFile *makeBufFileCommon(int nfiles);
static BufFile *makeBufFile(File firstfile, const char *operation_name);
static void extendBufFile(BufFile *file);
static void BufFileLoadBuffer(BufFile *file);
static void BufFileDumpBuffer(BufFile *file);
static int	BufFileFlush(BufFile *file);
static File MakeNewSharedSegment(BufFile *file, int segment);

static void BufFileStartCompression(BufFile *file);
static void BufFileDumpCompressedBuffer(BufFile *file, const void *buffer, Size nbytes);
static void BufFileEndCompression(BufFile *file);
static int BufFileLoadCompressedBuffer(BufFile *file, void *buffer, size_t bufsize);

/*
 * Create BufFile and perform the common initialization.
 */
static BufFile *
makeBufFileCommon(int nfiles)
{
	BufFile    *file = (BufFile *) palloc0(sizeof(BufFile));

	file->numFiles = nfiles;
	file->isInterXact = false;
	file->dirty = false;
	file->resowner = CurrentResourceOwner;
	file->curFile = 0;
	file->curOffset = 0L;
	file->pos = 0;
	file->nbytes = 0;
	file->buffer.data = palloc(BLCKSZ);

	return file;
}

/*
 * Create a BufFile given the first underlying physical file.
 * NOTE: caller must set isInterXact if appropriate.
 */
static BufFile *
makeBufFile(File firstfile, const char *operation_name)
{
	BufFile    *file = makeBufFileCommon(1);

	file->files = (File *) palloc(sizeof(File));
	file->files[0] = firstfile;
	file->readOnly = false;
	file->fileset = NULL;
	file->name = NULL;

	file->operation_name = pstrdup(operation_name);

	return file;
}

/*
 * Add another component temp file.
 */
static void
extendBufFile(BufFile *file)
{
	File		pfile;
	ResourceOwner oldowner;

	/* Be sure to associate the file with the BufFile's resource owner */
	oldowner = CurrentResourceOwner;
	CurrentResourceOwner = file->resowner;

	if (file->fileset == NULL)
		pfile = OpenTemporaryFile(file->isInterXact, file->operation_name);
	else
		pfile = MakeNewSharedSegment(file, file->numFiles);

	Assert(pfile >= 0);

	CurrentResourceOwner = oldowner;

	file->files = (File *) repalloc(file->files,
									(file->numFiles + 1) * sizeof(File));
	file->files[file->numFiles] = pfile;
	file->numFiles++;

	/*
	 * Register the file as a "work file", so that the Greenplum workfile
	 * limits apply to it.
	 *
	 * GPDB_12_MERGE_FIXME: In previous Greenplum versions, we had disabled
	 * the Postgres 1 GB segmentation of BufFiles. It was resurrected with
	 * The v12 merge. Now each 1 GB segment file counts as one work file.
	 * That makes the limit on the number of work files work differently.
	 * Is that OK? Documentation changes needed, at least.
	 */
	FileSetIsWorkfile(pfile);
	RegisterFileWithSet(pfile, file->work_set);
}

/*
 * Create a BufFile for a new temporary file (which will expand to become
 * multiple temporary files if more than MAX_PHYSICAL_FILESIZE bytes are
 * written to it).
 *
 * If interXact is true, the temp file will not be automatically deleted
 * at end of transaction.
 *
 * Note: if interXact is true, the caller had better be calling us in a
 * memory context, and with a resource owner, that will survive across
 * transaction boundaries.
 */
BufFile *
BufFileCreateTempInSet(char *operation_name, bool interXact, workfile_set *work_set)
{
	BufFile    *file;
	File		pfile;

	/*
	 * Ensure that temp tablespaces are set up for OpenTemporaryFile to use.
	 * Possibly the caller will have done this already, but it seems useful to
	 * double-check here.  Failure to do this at all would result in the temp
	 * files always getting placed in the default tablespace, which is a
	 * pretty hard-to-detect bug.  Callers may prefer to do it earlier if they
	 * want to be sure that any required catalog access is done in some other
	 * resource context.
	 */
	PrepareTempTablespaces();

	pfile = OpenTemporaryFile(interXact, operation_name);
	Assert(pfile >= 0);

	file = makeBufFile(pfile, operation_name);
	file->isInterXact = interXact;

	/*
	 * Register the file as a "work file", so that the Greenplum workfile
	 * limits apply to it.
	 */
	file->work_set = work_set;
	FileSetIsWorkfile(pfile);
	RegisterFileWithSet(pfile, work_set);

	SIMPLE_FAULT_INJECTOR("workfile_creation_failure");

	return file;
}

BufFile *
BufFileCreateTemp(char *operation_name, bool interXact)
{
	workfile_set *work_set;

	work_set = workfile_mgr_create_set(operation_name, NULL, false /* hold pin */);

	return BufFileCreateTempInSet(operation_name, interXact, work_set);
}

/*
 * Build the name for a given segment of a given BufFile.
 */
static void
SharedSegmentName(char *name, const char *buffile_name, int segment)
{
	snprintf(name, MAXPGPATH, "%s.%d", buffile_name, segment);
}

/*
 * Create a new segment file backing a shared BufFile.
 */
static File
MakeNewSharedSegment(BufFile *buffile, int segment)
{
	char		name[MAXPGPATH];
	File		file;

	/*
	 * It is possible that there are files left over from before a crash
	 * restart with the same name.  In order for BufFileOpenShared() not to
	 * get confused about how many segments there are, we'll unlink the next
	 * segment number if it already exists.
	 */
	SharedSegmentName(name, buffile->name, segment + 1);
	SharedFileSetDelete(buffile->fileset, name, true);

	/* Create the new segment. */
	SharedSegmentName(name, buffile->name, segment);
	file = SharedFileSetCreate(buffile->fileset, name);

	/* SharedFileSetCreate would've errored out */
	Assert(file > 0);

	return file;
}

/*
 * Create a BufFile that can be discovered and opened read-only by other
 * backends that are attached to the same SharedFileSet using the same name.
 *
 * The naming scheme for shared BufFiles is left up to the calling code.  The
 * name will appear as part of one or more filenames on disk, and might
 * provide clues to administrators about which subsystem is generating
 * temporary file data.  Since each SharedFileSet object is backed by one or
 * more uniquely named temporary directory, names don't conflict with
 * unrelated SharedFileSet objects.
 */
BufFile *
BufFileCreateShared(SharedFileSet *fileset, const char *name, workfile_set *work_set)
{
	BufFile    *file;

	file = makeBufFileCommon(1);
	file->fileset = fileset;
	file->name = pstrdup(name);
	file->files = (File *) palloc(sizeof(File));
	file->files[0] = MakeNewSharedSegment(file, 0);
	file->readOnly = false;
	/*
	 * Register the file as a "work file", so that the Greenplum workfile
	 * limits apply to it.
	 */
	file->work_set = work_set;

	FileSetIsWorkfile(file->files[0]);
	RegisterFileWithSet(file->files[0], work_set);

	return file;
}

/*
 * Open a file that was previously created in another backend (or this one)
 * with BufFileCreateShared in the same SharedFileSet using the same name.
 * The backend that created the file must have called BufFileClose() or
 * BufFileExportShared() to make sure that it is ready to be opened by other
 * backends and render it read-only.
 */
BufFile *
BufFileOpenShared(SharedFileSet *fileset, const char *name)
{
	BufFile    *file;
	char		segment_name[MAXPGPATH];
	Size		capacity = 16;
	File	   *files;
	int			nfiles = 0;

	files = palloc(sizeof(File) * capacity);

	/*
	 * We don't know how many segments there are, so we'll probe the
	 * filesystem to find out.
	 */
	for (;;)
	{
		/* See if we need to expand our file segment array. */
		if (nfiles + 1 > capacity)
		{
			capacity *= 2;
			files = repalloc(files, sizeof(File) * capacity);
		}
		/* Try to load a segment. */
		SharedSegmentName(segment_name, name, nfiles);
		files[nfiles] = SharedFileSetOpen(fileset, segment_name);
		if (files[nfiles] <= 0)
			break;
		++nfiles;

		CHECK_FOR_INTERRUPTS();
	}

	/*
	 * If we didn't find any files at all, then no BufFile exists with this
	 * name.
	 */
	if (nfiles == 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not open temporary file \"%s\" from BufFile \"%s\": %m",
						segment_name, name)));

	file = makeBufFileCommon(nfiles);
	file->files = files;
	file->readOnly = true;		/* Can't write to files opened this way */
	file->fileset = fileset;
	file->name = pstrdup(name);

	return file;
}

/*
 * Delete a BufFile that was created by BufFileCreateShared in the given
 * SharedFileSet using the given name.
 *
 * It is not necessary to delete files explicitly with this function.  It is
 * provided only as a way to delete files proactively, rather than waiting for
 * the SharedFileSet to be cleaned up.
 *
 * Only one backend should attempt to delete a given name, and should know
 * that it exists and has been exported or closed.
 */
void
BufFileDeleteShared(SharedFileSet *fileset, const char *name)
{
	char		segment_name[MAXPGPATH];
	int			segment = 0;
	bool		found = false;

	/*
	 * We don't know how many segments the file has.  We'll keep deleting
	 * until we run out.  If we don't manage to find even an initial segment,
	 * raise an error.
	 */
	for (;;)
	{
		SharedSegmentName(segment_name, name, segment);
		if (!SharedFileSetDelete(fileset, segment_name, true))
			break;
		found = true;
		++segment;

		CHECK_FOR_INTERRUPTS();
	}

	if (!found)
		elog(ERROR, "could not delete unknown shared BufFile \"%s\"", name);
}

/*
 * BufFileExportShared --- flush and make read-only, in preparation for sharing.
 */
void
BufFileExportShared(BufFile *file)
{
	/* Must be a file belonging to a SharedFileSet. */
	Assert(file->fileset != NULL);

	/* It's probably a bug if someone calls this twice. */
	Assert(!file->readOnly);

	BufFileFlush(file);
	file->readOnly = true;
}

/*
 * Close a BufFile
 *
 * Like fclose(), this also implicitly FileCloses the underlying File.
 */
void
BufFileClose(BufFile *file)
{
	int			i;

	/* flush any unwritten data */
	BufFileFlush(file);
	/* close and delete the underlying file(s) */
	for (i = 0; i < file->numFiles; i++)
		FileClose(file->files[i]);

	/* release the buffer space */
	pfree(file->files);

	if (file->buffer.data)
		pfree(file->buffer.data);

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
static void
BufFileLoadBuffer(BufFile *file)
{
	File		thisfile;

	/*
	 * Advance to next component file if necessary and possible.
	 */
	if (file->curOffset >= MAX_PHYSICAL_FILESIZE &&
		file->curFile + 1 < file->numFiles)
	{
		file->curFile++;
		file->curOffset = 0L;
	}

	/*
	 * Read whatever we can get, up to a full bufferload.
	 */
	thisfile = file->files[file->curFile];
	file->nbytes = FileRead(thisfile,
							file->buffer.data,
							BLCKSZ,
							file->curOffset,
							WAIT_EVENT_BUFFILE_READ);
	if (file->nbytes < 0)
		file->nbytes = 0;
	/* we choose not to advance curOffset here */

	if (file->nbytes > 0)
		pgBufferUsage.temp_blks_read++;
}

/*
 * BufFileDumpBuffer
 *
 * Dump buffer contents starting at curOffset.
 * At call, should have dirty = true, nbytes > 0.
 * On exit, dirty is cleared if successful write, and curOffset is advanced.
 */
static void
BufFileDumpBuffer(BufFile *file)
{
	int			wpos = 0;
	int			bytestowrite;
	File		thisfile;

	/*
	 * Unlike BufFileLoadBuffer, we must dump the whole buffer even if it
	 * crosses a component-file boundary; so we need a loop.
	 */
	while (wpos < file->nbytes)
	{
		off_t		availbytes;

		/*
		 * Advance to next component file if necessary and possible.
		 */
		if (file->curOffset >= MAX_PHYSICAL_FILESIZE)
		{
			while (file->curFile + 1 >= file->numFiles)
				extendBufFile(file);
			file->curFile++;
			file->curOffset = 0L;
		}

		/*
		 * Determine how much we need to write into this file.
		 */
		bytestowrite = file->nbytes - wpos;
		availbytes = MAX_PHYSICAL_FILESIZE - file->curOffset;

		if ((off_t) bytestowrite > availbytes)
			bytestowrite = (int) availbytes;

		thisfile = file->files[file->curFile];
		bytestowrite = FileWrite(thisfile,
								 file->buffer.data + wpos,
								 bytestowrite,
								 file->curOffset,
								 WAIT_EVENT_BUFFILE_WRITE);
		if (bytestowrite <= 0)
			return;				/* failed to write */
		file->curOffset += bytestowrite;
		wpos += bytestowrite;

		pgBufferUsage.temp_blks_written++;
	}
	file->dirty = false;

	/*
	 * At this point, curOffset has been advanced to the end of the buffer,
	 * ie, its original value + nbytes.  We need to make it point to the
	 * logical file position, ie, original value + pos, in case that is less
	 * (as could happen due to a small backwards seek in a dirty buffer!)
	 */
	file->curOffset -= (file->nbytes - file->pos);
	if (file->curOffset < 0)	/* handle possible segment crossing */
	{
		file->curFile--;
		Assert(file->curFile >= 0);
		file->curOffset += MAX_PHYSICAL_FILESIZE;
	}

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
	{
		if (BufFileFlush(file) != 0)
			return 0;			/* could not flush... */
		Assert(!file->dirty);
	}

	while (size > 0)
	{
		if (file->pos >= file->nbytes)
		{
			/* Try to load more data into buffer. */
			file->curOffset += file->pos;
			file->pos = 0;
			file->nbytes = 0;
			BufFileLoadBuffer(file);
			if (file->nbytes <= 0)
				break;			/* no more data available */
		}

		nthistime = file->nbytes - file->pos;
		if (nthistime > size)
			nthistime = size;
		Assert(nthistime > 0);

		memcpy(ptr, file->buffer.data + file->pos, nthistime);

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
		result = file->buffer.data + file->pos;
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
BufFileWrite(BufFile *file, void *ptr, size_t size)
{
	size_t		nwritten = 0;
	size_t		nthistime;

	Assert(!file->readOnly);

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
		if (file->pos >= BLCKSZ)
		{
			/* Buffer full, dump it out */
			if (file->dirty)
			{
				BufFileDumpBuffer(file);
				if (file->dirty)
					break;		/* I/O error */
			}
			else
			{
				/* Hmm, went directly from reading to writing? */
				file->curOffset += file->pos;
				file->pos = 0;
				file->nbytes = 0;
			}
		}

		nthistime = BLCKSZ - file->pos;
		if (nthistime > size)
			nthistime = size;
		Assert(nthistime > 0);

		memcpy(file->buffer.data + file->pos, ptr, nthistime);

		file->dirty = true;
		file->pos += nthistime;
		if (file->nbytes < file->pos)
			file->nbytes = file->pos;
		ptr = (void *) ((char *) ptr + nthistime);
		size -= nthistime;
		nwritten += nthistime;
	}

	return nwritten;
}

/*
 * BufFileFlush
 *
 * Like fflush()
 */
static int
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
			return 0;
	}

	if (file->dirty)
	{
		BufFileDumpBuffer(file);
		if (file->dirty)
			return EOF;
	}

	return 0;
}

/*
 * BufFileSeek
 *
 * Like fseek(), except that target position needs two values in order to
 * work when logical filesize exceeds maximum value representable by off_t.
 * We do not support relative seeks across more than that, however.
 *
 * Result is 0 if OK, EOF if not.  Logical position is not moved if an
 * impossible seek is attempted.
 */
int
BufFileSeek(BufFile *file, int fileno, off_t offset, int whence)
{
	int			newFile;
	off_t		newOffset;

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
			file->curOffset = 0;
			file->pos = 0;
			file->nbytes = 0;
			return 0;

		case BFS_COMPRESSED_READING:
		case BFS_SEQUENTIAL_READING:
			elog(ERROR, "cannot seek in sequential BufFile");
	}

	switch (whence)
	{
		case SEEK_SET:
			if (fileno < 0)
				return EOF;
			newFile = fileno;
			newOffset = offset;
			break;
		case SEEK_CUR:

			/*
			 * Relative seek considers only the signed offset, ignoring
			 * fileno. Note that large offsets (> 1 gig) risk overflow in this
			 * add, unless we have 64-bit off_t.
			 */
			newFile = file->curFile;
			newOffset = (file->curOffset + file->pos) + offset;
			break;
		case SEEK_END:
			/*
			 * The file size of the last file gives us the end offset of that
			 * file.
			 */
			if (file->curFile == file->numFiles - 1 && file->dirty)
				BufFileFlush(file);
			newFile = file->numFiles - 1;
			newOffset = FileSize(file->files[file->numFiles - 1]);
			if (newOffset < 0)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not determine size of temporary file \"%s\" from BufFile \"%s\": %m",
							 FilePathName(file->files[file->numFiles - 1]),
							 file->name)));
			break;	
		default:
			elog(ERROR, "invalid whence: %d", whence);
			return EOF;
	}
	while (newOffset < 0)
	{
		if (--newFile < 0)
			return EOF;
		newOffset += MAX_PHYSICAL_FILESIZE;
	}
	if (newFile == file->curFile &&
		newOffset >= file->curOffset &&
		newOffset <= file->curOffset + file->nbytes)
	{
		/*
		 * Seek is to a point within existing buffer; we can just adjust
		 * pos-within-buffer, without flushing buffer.  Note this is OK
		 * whether reading or writing, but buffer remains dirty if we were
		 * writing.
		 */
		file->pos = (int) (newOffset - file->curOffset);
		return 0;
	}
	/* Otherwise, must reposition buffer, so flush any dirty data */
	if (BufFileFlush(file) != 0)
		return EOF;

	/*
	 * At this point and no sooner, check for seek past last segment. The
	 * above flush could have created a new segment, so checking sooner would
	 * not work (at least not with this code).
	 */

	/* convert seek to "start of next seg" to "end of last seg" */
	if (newFile == file->numFiles && newOffset == 0)
	{
		newFile--;
		newOffset = MAX_PHYSICAL_FILESIZE;
	}
	while (newOffset > MAX_PHYSICAL_FILESIZE)
	{
		if (++newFile >= file->numFiles)
			return EOF;
		newOffset -= MAX_PHYSICAL_FILESIZE;
	}
	if (newFile >= file->numFiles)
		return EOF;
	/* Seek is OK! */
	file->curFile = newFile;
	file->curOffset = newOffset;
	file->pos = 0;
	file->nbytes = 0;
	return 0;
}

void
BufFileTell(BufFile *file, int *fileno, off_t *offset)
{
	*fileno = file->curFile;
	*offset = file->curOffset + file->pos;
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
BufFileSeekBlock(BufFile *file, long blknum)
{
	return BufFileSeek(file,
					   (int) (blknum / BUFFILE_SEG_SIZE),
					   (off_t) (blknum % BUFFILE_SEG_SIZE) * BLCKSZ,
					   SEEK_SET);
}

#ifdef NOT_USED
/*
 * BufFileTellBlock --- block-oriented tell
 *
 * Any fractional part of a block in the current seek position is ignored.
 */
long
BufFileTellBlock(BufFile *file)
{
	long		blknum;

	blknum = (file->curOffset + file->pos) / BLCKSZ;
	blknum += file->curFile * BUFFILE_SEG_SIZE;
	return blknum;
}

#endif

/*
 * Return the current shared BufFile size.
 *
 * Counts any holes left behind by BufFileAppend as part of the size.
 * ereport()s on failure.
 */
int64
BufFileSize(BufFile *file)
{
	int64		lastFileSize;

	// In upstream, this is only used for shared BufFiles, but in GPDB
	// also for getting the file size for extra EXPLAIN ANALYZE stats.
	//Assert(file->fileset != NULL);

	/* Get the size of the last physical file. */
	lastFileSize = FileSize(file->files[file->numFiles - 1]);
	if (lastFileSize < 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not determine size of temporary file \"%s\" from BufFile \"%s\": %m",
						FilePathName(file->files[file->numFiles - 1]),
						file->name)));

	return ((file->numFiles - 1) * (int64) MAX_PHYSICAL_FILESIZE) +
		lastFileSize;
}

/*
 * Append the contents of source file (managed within shared fileset) to
 * end of target file (managed within same shared fileset).
 *
 * Note that operation subsumes ownership of underlying resources from
 * "source".  Caller should never call BufFileClose against source having
 * called here first.  Resource owners for source and target must match,
 * too.
 *
 * This operation works by manipulating lists of segment files, so the
 * file content is always appended at a MAX_PHYSICAL_FILESIZE-aligned
 * boundary, typically creating empty holes before the boundary.  These
 * areas do not contain any interesting data, and cannot be read from by
 * caller.
 *
 * Returns the block number within target where the contents of source
 * begins.  Caller should apply this as an offset when working off block
 * positions that are in terms of the original BufFile space.
 */
long
BufFileAppend(BufFile *target, BufFile *source)
{
	if (target->state == BFS_COMPRESSED_WRITING ||
		target->state == BFS_COMPRESSED_READING ||
		source->state == BFS_COMPRESSED_WRITING ||
		source->state == BFS_COMPRESSED_READING)
	{
		elog(ERROR, "cannot append a compressed BufFile");
	}

	long		startBlock = target->numFiles * BUFFILE_SEG_SIZE;
	int			newNumFiles = target->numFiles + source->numFiles;
	int			i;

	Assert(target->fileset != NULL);
	Assert(source->readOnly);
	Assert(!source->dirty);
	Assert(source->fileset != NULL);

	if (target->resowner != source->resowner)
		elog(ERROR, "could not append BufFile with non-matching resource owner");

	target->files = (File *)
		repalloc(target->files, sizeof(File) * newNumFiles);
	for (i = target->numFiles; i < newNumFiles; i++)
		target->files[i] = source->files[i - target->numFiles];
	target->numFiles = newNumFiles;

	return startBlock;
}

/*
 * Return filename of the underlying file.
 *
 * For debugging purposes only. Returns the filename of the
 * first file, if it's segmented.
 */
const char *
BufFileGetFilename(BufFile *buffile)
{
	return FileGetFilename(buffile->files[0]);
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
	pfree(buffile->buffer.data);
	buffile->buffer.data = NULL;
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

	Assert(buffile->buffer.data == NULL);
	buffile->buffer.data = palloc(BLCKSZ);

	if (BufFileSeek(buffile, 0, 0, SEEK_SET) != 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not seek to the first block of temporary file: %m")));
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
	if (BufFileSize(buffile) != 0)
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
 * Temporary buffer used during compression. It's used only within the
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
	if (file->buffer.data)
	{
		pfree(file->buffer.data);
		file->buffer.data = NULL;
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
	if (!file->zstd_context->cctx)
		elog(ERROR, "out of memory");
	ZSTD_initCStream(file->zstd_context->cctx, BUFFILE_ZSTD_COMPRESSION_LEVEL);

	CurrentResourceOwner = oldowner;

	file->state = BFS_COMPRESSED_WRITING;
}

static void
BufFileDumpCompressedBuffer(BufFile *file, const void *buffer, Size nbytes)
{
	ZSTD_inBuffer input;
	off_t pos = 0;

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

			wrote = FileWrite(file->files[0], output.dst, output.pos, file->curOffset + file->pos + pos, WAIT_EVENT_BUFFILE_WRITE);
			if (wrote != output.pos)
				elog(ERROR, "could not write %d bytes to compressed temporary file: %m", (int) output.pos);
			pos += wrote;
		}
	}
	file->curOffset += pos;
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
	off_t		pos = 0;

	Assert(file->state == BFS_COMPRESSED_WRITING);

	do {
		output.dst = compression_buffer;
		output.size = BLCKSZ;
		output.pos = 0;

		ret = ZSTD_endStream(file->zstd_context->cctx, &output);
		if (ZSTD_isError(ret))
			elog(ERROR, "%s", ZSTD_getErrorName(ret));

		wrote = FileWrite(file->files[0], output.dst, output.pos, file->curOffset + file->pos + pos, WAIT_EVENT_BUFFILE_WRITE);
		if (wrote != output.pos)
			elog(ERROR, "could not write %d bytes to compressed temporary file: %m", (int) output.pos);
		pos += wrote;
	} while (ret > 0);

	ZSTD_freeCCtx(file->zstd_context->cctx);
	file->zstd_context->cctx = NULL;

	elog(DEBUG1, "BufFile compressed from %ld to %ld bytes",
		 file->uncompressed_bytes, BufFileSize(file));

	/* Done writing. Initialize for reading */
	file->zstd_context->dctx = ZSTD_createDStream();
	if (!file->zstd_context->dctx)
		elog(ERROR, "out of memory");
	ZSTD_initDStream(file->zstd_context->dctx);

	file->compressed_buffer.src = palloc(BLCKSZ);
	file->compressed_buffer.size = 0;
	file->compressed_buffer.pos = 0;
	file->state = BFS_RANDOM_ACCESS;

	if (BufFileSeek(file, 0, 0, SEEK_SET) != 0)
		elog(ERROR, "could not seek in temporary file: %m");

	file->state = BFS_COMPRESSED_READING;
}

static int
BufFileLoadCompressedBuffer(BufFile *file, void *buffer, size_t bufsize)
{
	ZSTD_outBuffer output;
	size_t		ret;
	bool		eof = false;
	off_t		pos = 0;

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

			nb = FileRead(file->files[0], (char *) file->compressed_buffer.src, BLCKSZ, file->curOffset + file->pos + pos, WAIT_EVENT_BUFFILE_READ);
			if (nb < 0)
			{
				elog(ERROR, "could not read from temporary file: %m");
			}
			pos += nb;
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

	file->curOffset += pos;

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
