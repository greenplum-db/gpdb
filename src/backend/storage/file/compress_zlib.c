/* compress_zlib.c */
#include "postgres.h"
#include "fstream/gfile.h"
#include "storage/bfz.h"
#include <zlib.h>

struct bfz_zlib_freeable_stuff
{
	struct bfz_freeable_stuff super;

	/* handle for the compressed file */
	gfile_t *gfile;
};

typedef struct FileObject {
	FileAccessInterface file_access;
	File file;
} FileObject;

static ssize_t
read_file(FileAccessInterface *file_access, void *ptr, size_t size)
{
	Assert(file_access);
	Assert(PostgresFileObject == file_access->ftype);
	FileObject* fobj = (FileObject*)file_access;
	return FileRead(fobj->file, ptr, size);
}

static ssize_t
write_file(FileAccessInterface *file_access, void *ptr, size_t size)
{
	Assert(file_access);
	Assert(PostgresFileObject == file_access->ftype);
	FileObject* fobj = (FileObject*)file_access;
	return FileWrite(fobj->file, ptr, size);
}

static int
close_file(FileAccessInterface *file_access)
{
	/* don't call the `FileClose`. BFZ owns File and it knows when to close it.*/
	return 0;
}

static FileAccessInterface*
gfile_create_fileobj_access(File file, GFileAlloca gfile_alloca)
{
	FileObject *fobj = gfile_alloca(sizeof(FileObject));
	fobj->file_access.ftype = PostgresFileObj;
	fobj->file_access.read_file = read_file;
	fobj->file_access.write_file = write_file;
	fobj->file_access.close_file = close_file;
	fobj->file = file;
	return &fobj->file_access;
}

static void
gfile_report_error(gfile_t *gf)
{
	Assert(gf);
	GFileErrorCode error_code = gfile_error_code(gf);
	char* msg_detail = gfile_error_detail(gf);
	gfile_reset_error(gf);
	switch(error_code)
	{
		case GF_NoError:
			return;
		case GF_AccessError:
			ereport(ERROR,
					(errcode_for_file_access(),
							errmsg("could not write to temporary file: %m")));
			break;
		case GF_BZDecompressInitError:
			ereport(ERROR,
					(errmsg("bzlib decompression init failed")));
			break;
		case GF_BZDecompressError:
			ereport(ERROR,
					(errmsg("bzlib decompression failed")));
			break;
		case GF_InflateInit2Error:
			ereport(ERROR,
					(errmsg("zlib inflateInit2 failed"),
							msg_detail ? errdetail("%s", msg_detail) : 0));
			break;
		case GF_InflateError:
			ereport(ERROR,
					(errmsg("zlib inflate failed"),
							msg_detail ? errdetail("%s", msg_detail) : 0));
			break;
		case GF_DeflateInit2Error:
			ereport(ERROR,
					(errmsg("zlib deflateInit2 failed"),
							msg_detail ? errdetail("%s", msg_detail) : 0));
			break;
		case GF_DeflateError:
			ereport(ERROR,
					(errmsg("zlib deflate failed"),
							msg_detail ? errdetail("%s", msg_detail) : 0));
			break;
		case GF_OutOfMemory:
			ereport(ERROR,
					(errmsg("Out of memory")));
			break;
		default:
			ereport(ERROR,
					(errmsg("Unexpected error from gfile")));
	}
}

void*
bfz_gfile_palloc(size_t size)
{
	return palloc(size);
}

void
bfz_gfile_pfree(void*a)
{
	pfree(a);
}

/*
 * bfz_zlib_close_ex
 *	Close buffers etc. Does not close the underlying file!
 */
static void
bfz_zlib_close_ex(bfz_t *thiz)
{
	struct bfz_zlib_freeable_stuff *fs = (void *) thiz->freeable_stuff;

	if (NULL != fs)
	{
		Assert(NULL != fs->gfile);

		gfile_close(fs->gfile);
		if (gfile_has_error(fs->gfile))
		{
			gfile_report_error(fs->gfile);
		}
		gfile_destroy(fs->gfile);
		fs->gfile = NULL;

		pfree(fs);
		thiz->freeable_stuff = NULL;
	}
}

/*
 * bfz_zlib_write_ex
 *	 Write data to an opened compressed file.
 *	 An exception is thrown if the data cannot be written for any reason.
 */
static void
bfz_zlib_write_ex(bfz_t *thiz, const char *buffer, int size)
{
	struct bfz_zlib_freeable_stuff *fs = (void *) thiz->freeable_stuff;

	gfile_write(fs->gfile, (void *)buffer, size);
	if (gfile_has_error(fs->gfile))
	{
		Assert(read < 0);
		gfile_report_error(fs->gfile);
	}
	else
	{
		Assert(read >= 0);
	}
}

/*
 * bfz_zlib_read_ex
 *	Read data from an already opened compressed file.
 *
 *	The buffer pointer must be valid and have at least size bytes.
 *	An exception is thrown if the data cannot be read for any reason.
 *
 * The buffer is filled completely.
 */
static int
bfz_zlib_read_ex(bfz_t *thiz, char *buffer, int size)
{
	struct bfz_zlib_freeable_stuff *fs = (void *) thiz->freeable_stuff;

	ssize_t read = gfile_read(fs->gfile, buffer, size);
	if (gfile_has_error(fs->gfile))
	{
		Assert(read < 0);
		gfile_report_error(fs->gfile);
	}
	else
	{
		Assert(read >= 0);
	}
	return read;
}

/*
 * bfz_zlib_init
 *	Initialize the zlib subsystem for a file.
 *
 *	The underlying file descriptor fd should already be opened
 *	and valid. Memory is allocated in the current memory context.
 */
void
bfz_zlib_init(bfz_t * thiz)
{
	struct bfz_zlib_freeable_stuff *fs = palloc(sizeof *fs);
	bool is_write = thiz->mode == BFZ_MODE_APPEND;
	fs->gfile = gfile_create(GZ_COMPRESSION, is_write, bfz_gfile_palloc, bfz_gfile_pfree);
	FileAccessInterface* file_access = gfile_create_fileobj_access(thiz->file, bfz_gfile_palloc);
	gz_file_open(fs->gfile, file_access);

	if (gfile_has_error(fs->gfile))
	{
		gfile_report_error(fs->gfile);
	}

	thiz->freeable_stuff = &fs->super;
	fs->super.read_ex = bfz_zlib_read_ex;
	fs->super.write_ex = bfz_zlib_write_ex;
	fs->super.close_ex = bfz_zlib_close_ex;
}
