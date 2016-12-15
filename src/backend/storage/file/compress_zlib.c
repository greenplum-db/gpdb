/* compress_zlib.c */
#include "postgres.h"

#include <zlib.h>

#include "storage/bfz.h"
#include "utils/memutils.h"

#define COMPRESSION_BUFFER_SIZE		(1<<14)

struct bfz_zlib_freeable_stuff
{
	struct bfz_freeable_stuff super;

	/* true if compressing, false if decompressing */
	bool		compressing;

	bool		eof_in;
	bool		eof_out;

	/* handle for the compressed file */
	z_stream	s;
	Byte		buf[COMPRESSION_BUFFER_SIZE];
};

/* This file implements bfz compression algorithm "zlib". */

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
		if (fs->compressing)
		{
			int			ret1;

			do
			{
				int			have;
				int			written;

				/* Flush all remaining output to the underlying file */
				fs->s.avail_in = 0;
				fs->s.avail_out = COMPRESSION_BUFFER_SIZE;
				fs->s.next_out = fs->buf;

				ret1 = deflate(&fs->s, Z_FINISH);
				if (ret1 < 0)
					ereport(ERROR,
							(errmsg("zlib deflate failed"),
							 fs->s.msg ? errdetail("%s", fs->s.msg) : 0));

				/* Write until the output buffer is empty */
				have = COMPRESSION_BUFFER_SIZE - fs->s.avail_out;

				written = 0;
				while (have > 0)
				{
					int			n = FileWrite(thiz->file, (char *) fs->buf + written, have);

					if (n < 0)
						ereport(ERROR,
								(errcode_for_file_access(),
						   errmsg("could not write to temporary file: %m")));
					written += n;
					have -= n;
				}
			} while (ret1 != Z_STREAM_END);

			if (deflateEnd(&fs->s) != Z_OK)
				ereport(ERROR,
						(errmsg("zlib deflateEnd failed"),
						 fs->s.msg ? errdetail("%s", fs->s.msg) : 0));
		}
		else
		{
			if (inflateEnd(&fs->s) != Z_OK)
				ereport(ERROR,
						(errmsg("zlib inflateEnd failed"),
						 fs->s.msg ? errdetail("%s", fs->s.msg) : 0));
		}

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
	int			ret1;

	/* Feed the new data to deflate() */
	fs->s.avail_in = size;
	fs->s.next_in = (Byte *) buffer;

	/* Deflate until the input buffer is empty */
	while (fs->s.avail_in)
	{
		int			have;
		int			written;

		fs->s.avail_out = COMPRESSION_BUFFER_SIZE;
		fs->s.next_out = fs->buf;

		ret1 = deflate(&fs->s, Z_NO_FLUSH);
		if (ret1 == Z_STREAM_ERROR)
			ereport(ERROR,
					(errmsg("zlib deflate failed"),
					 fs->s.msg ? errdetail("%s", fs->s.msg) : 0));

		/* Write until the output buffer is empty */
		have = COMPRESSION_BUFFER_SIZE - fs->s.avail_out;

		written = 0;
		while (have > 0)
		{
			int			n = FileWrite(thiz->file, (char *) fs->buf + written, have);

			if (n < 0)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not write to temporary file: %m")));
			written += n;
			have -= n;
		}
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

	fs->s.next_out = (Byte *) buffer;
	fs->s.avail_out = size;

	while (fs->s.avail_out > 0)
	{
		int			e;

		/*
		 * Fill up our input buffer from the input file.
		 */
		if (fs->s.avail_in == 0 && !fs->eof_in)
		{
			int			s = FileRead(thiz->file, (char *) fs->buf, COMPRESSION_BUFFER_SIZE);

			if (s == 0)
			{
				/* no more data to read */
				fs->eof_in = true;
			}
			if (s < 0)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not read from temporary file: %m")));

			fs->s.next_in = fs->buf;
			fs->s.avail_in = s;
		}

		if (fs->eof_in && fs->s.avail_in == 0)
		{
			if (!fs->eof_out)
			{
				/* No more input, but zlib thinks there should be more */
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("unexpected end of temporary file")));
			}

			/*
			 * end of input file, and buffers are empty, and zlib agrees that
			 * we're at end of a stream.
			 */
			break;
		}

		/* decompress */
		e = inflate(&fs->s, Z_SYNC_FLUSH);

		fs->eof_out = false;
		if (e == Z_STREAM_END)
		{
			fs->eof_out = true;

			/*
			 * we're done decompressing a chunk, but there might be more
			 * input. we need to reset state. see MPP-8012 for info
			 */
			if (inflateReset(&fs->s) != Z_OK)
				ereport(ERROR,
						(errmsg("inflateReset error"),
						 fs->s.msg ? errdetail("%s", fs->s.msg) : 0));
		}
		else if (e != Z_OK)
		{
			ereport(ERROR,
					(errmsg("could not uncompress data from temporary file"),
					 fs->s.msg ? errdetail("%s", fs->s.msg) : 0));
		}
	}

	return (char *) fs->s.next_out - buffer;
}

static voidpf
z_alloc(voidpf a, uInt b, uInt c)
{
	/*
	 * zlib shouldn't allocate anything large, but better safe than sorry.
	 */
	if (b > MaxAllocSize || c > MaxAllocSize ||
		(uint64) b * (uint64) c > MaxAllocSize)
		return NULL;

	return palloc(b * c);
}

static void
z_free(voidpf a, voidpf b)
{
	pfree(b);
}

/*
 * bfz_zlib_init
 *	Initialize the zlib subsystem for a file.
 *
 *	The underlying file descriptor fd should already be opened
 *	and valid. Memory is allocated in the current memory context.
 */
void
bfz_zlib_init(bfz_t *thiz)
{
	struct bfz_zlib_freeable_stuff *fs = palloc(sizeof *fs);

	memset(&fs->s, 0, sizeof(fs->s));
	fs->s.zalloc = z_alloc;
	fs->s.zfree = z_free;
	fs->s.opaque = Z_NULL;

	fs->eof_in = false;
	fs->eof_out = false;
	fs->compressing = (thiz->mode == BFZ_MODE_APPEND);

	if (fs->compressing)
	{
		/*
		 * writing a compressed file
		 */
		if (deflateInit2(&fs->s, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK)
			ereport(ERROR,
					(errmsg("zlib deflateInit2 failed"),
					 fs->s.msg ? errdetail("%s", fs->s.msg) : 0));
	}
	else
	{
		/*
		 * reading a compressed file
		 */
		if (inflateInit2(&fs->s, 31))
			ereport(ERROR,
					(errmsg("zlib inflateInit2 failed"),
					 fs->s.msg ? errdetail("%s", fs->s.msg) : 0));
	}

	thiz->freeable_stuff = &fs->super;
	fs->super.read_ex = bfz_zlib_read_ex;
	fs->super.write_ex = bfz_zlib_write_ex;
	fs->super.close_ex = bfz_zlib_close_ex;
}
