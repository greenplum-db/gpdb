/* compress_zlib.c */
#include "c.h"
#include "fstream/gfile.h"
#include "storage/bfz.h"
#include <zlib.h>

struct bfz_zlib_freeable_stuff
{
	struct bfz_freeable_stuff super;

	/* handle for the compressed file */
	gfile_t *gfile;
};

/* This file implements bfz compression algorithm "zlib". */

/*
 * bfz_zlib_close_ex
 *  Close a file and freeing up descriptor, buffers etc.
 *
 *  This is also called from an xact end callback, hence it should
 *  not contain any elog(ERROR) calls.
 */
static void
bfz_zlib_close_ex(bfz_t * thiz)
{
	struct bfz_zlib_freeable_stuff *fs = (void *) thiz->freeable_stuff;

	if (NULL != fs)
	{
		Assert(NULL != fs->gfile);

		fs->gfile->close(fs->gfile);
		pfree(fs->gfile);
		fs->gfile = NULL;

		pfree(fs);
		thiz->freeable_stuff = NULL;
	}

	/*
	 * gfile->close() does not close the underlying file descriptor.
	 * Let's close it here.
	 */
	if (thiz->fd != -1)
	{
		close(thiz->fd);
		thiz->fd = -1;
	}
}

/*
 * bfz_zlib_write_ex
 *   Write data to an opened compressed file.
 *   An exception is thrown if the data cannot be written for any reason.
 */
static void
bfz_zlib_write_ex(bfz_t *thiz, const char *buffer, int size)
{
	struct bfz_zlib_freeable_stuff *fs = (void *) thiz->freeable_stuff;

	ssize_t written = gfile_write(fs->gfile, buffer, size);
	if (written < 0)
	{
		ereport(ERROR,
				(errcode(ERRCODE_IO_ERROR),
				errmsg("could not write to temporary file: %m")));
	}
}

/*
 * bfz_zlib_read_ex
 *  Read data from an already opened compressed file.
 *
 *  The buffer pointer must be valid and have at least size bytes.
 *  An exception is thrown if the data cannot be read for any reason.
 */
static int
bfz_zlib_read_ex(bfz_t *thiz, char *buffer, int size)
{
	struct bfz_zlib_freeable_stuff *fs = (void *) thiz->freeable_stuff;

	ssize_t read = gfile_read(fs->gfile, buffer, size);
	if (read < 0)
	{
		ereport(ERROR,
				(errcode(ERRCODE_IO_ERROR),
				errmsg("could not read from temporary file: %m")));
	}

	return read;
}

/*
 * bfz_zlib_init
 *  Initialize the zlib subsystem for a file.
 *
 *  The underlying file descriptor fd should already be opened
 *  and valid. Memory is allocated in the current memory context.
 */
void
bfz_zlib_init(bfz_t * thiz)
{
	Assert(TopMemoryContext == CurrentMemoryContext);
	struct bfz_zlib_freeable_stuff *fs = palloc(sizeof *fs);
	fs->gfile = palloc0(sizeof *fs->gfile);

	fs->gfile->fd.filefd = thiz->fd;
	fs->gfile->compression = GZ_COMPRESSION;

	if (thiz->mode == BFZ_MODE_APPEND)
	{
		fs->gfile->is_write = TRUE;
	}

	int res = gz_file_open(fs->gfile);

	if (res == 1)
	{
		ereport(ERROR,
				(errcode(ERRCODE_IO_ERROR),
				errmsg("gz_file_open failed: %m")));
	}

	thiz->freeable_stuff = &fs->super;
	fs->super.read_ex = bfz_zlib_read_ex;
	fs->super.write_ex = bfz_zlib_write_ex;
	fs->super.close_ex = bfz_zlib_close_ex;
}
