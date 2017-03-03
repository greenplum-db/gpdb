/* compress_nothing.c */

#include "postgres.h"

#include <storage/bfz.h>
#include <storage/fd.h>

/*
 * This file implements bfz compression algorithm "nothing".
 * We don't compress at all.
 */

/*
 * bfz_nothing_close_ex
 *	Free up descriptor, buffers etc. Does not close the underlying file!
 */
static void
bfz_nothing_close_ex(bfz_t * thiz)
{
	pfree(thiz->freeable_stuff);
	thiz->freeable_stuff = NULL;
}

static int
bfz_nothing_read_ex(bfz_t * thiz, char *buffer, int size)
{
	int			orig_size = size;

	while (size)
	{
		int			i = FileRead(thiz->file, buffer, size);

		if (i < 0)
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not read from temporary file: %m")));
		if (i == 0)
			break;
		buffer += i;
		size -= i;
	}
	return orig_size - size;
}

static void
bfz_nothing_write_ex(bfz_t * bfz, const char *buffer, int size)
{
	while (size)
	{
		int			i = FileWrite(bfz->file, (char *) buffer, size);

		if (i < 0)
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not write to temporary file: %m")));
		buffer += i;
		size -= i;
	}
}

void
bfz_nothing_init(bfz_t * thiz)
{
	struct bfz_freeable_stuff *fs = palloc(sizeof *fs);

	thiz->freeable_stuff = fs;

	fs->read_ex = bfz_nothing_read_ex;
	fs->write_ex = bfz_nothing_write_ex;
	fs->close_ex = bfz_nothing_close_ex;
}
