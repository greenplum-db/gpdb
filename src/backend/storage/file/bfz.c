/* bfz.c */
#include "postgres.h"

#include "storage/bfz.h"
#include "storage/fd.h"
#include "miscadmin.h"

#include "utils/memutils.h"		/* For MemoryContext stuff */
#include "cdb/cdbvars.h"
#include "utils/workfile_mgr.h"
#include "storage/fd.h"

typedef pg_crc32 BFZ_CHECKSUM_TYPE;

#define BFZ_CHECKSUM_SIZE(has_checksum)	\
	((has_checksum) ? sizeof(BFZ_CHECKSUM_TYPE) : 0)

/*
 * Since checksumming a full BFZ block is too expensive,
 * we only checksum several bytes in every sector in a BFZ
 * block. The size of a sector is defined as below. The bytes
 * to be checksummed are defined by gp_workfile_bytes_to_checksum.
 */
#define BFZ_CHECKSUM_SECTOR_SIZE WORKFILE_SAFEWRITE_SIZE

#define BFZ_MKTEMP_MASK  "XXXXXXXXXX"

static const struct{
    const char*name[6];
    void(*init)(bfz_t*thiz);
} compression_algorithms[] =
{
    {{"none", "false", "no", "off", "0", 0}, bfz_nothing_init},
    {{"zlib", 0}, bfz_zlib_init},
    {{0}}
};

static bfz_t *bfz_create_internal(const char *fileName, bool open_existing, bool delOnClose, int compress);

int
bfz_string_to_compression(const char *string)
{
	int			i;
	const char *const * a;

	for (i = 0; compression_algorithms[i].name[0]; i++)
		for (a = compression_algorithms[i].name; *a; a++)
			if (!pg_strcasecmp(*a, string))
				return i;
	return -1;
}

static const char *
bfz_compression_to_string(int index)
{
	return compression_algorithms[index].name[0];
}

#define BFZ_CHECKSUM_EQ(c1, c2) EQ_CRC32C(c1, c2)

/*
 * Compute a checksum for a given char array.
 */
static BFZ_CHECKSUM_TYPE
compute_checksum(const char *buffer, uint32 size)
{
	BFZ_CHECKSUM_TYPE crc = 0;
	/*
	 * We only checksum the first gp_workfile_bytes_to_checksum bytes
	 * in every BFZ_CHECKSUM_SECTOR_SIZE bytes.
	 */
	uint32 currSectorBegin = 0;
	
	INIT_CRC32C(crc);
	
	while (currSectorBegin < size)
	{
		COMP_CRC32C(crc, buffer + currSectorBegin,
				   Min(size - currSectorBegin,
					   gp_workfile_bytes_to_checksum));
		currSectorBegin += BFZ_CHECKSUM_SECTOR_SIZE;
	}

	/*
	 * By historical accident, the checksum calculated here
	 * is *not* inverted, like CRC-32C checksums usually are.
	 */
	/* FIN_CRC32C(crc); */

	return crc;
}

/*
 * Write out a bfz buffer.
 *
 * If isLast is true, the size of content of the buffer could be
 * smaller than BFZ_BUFFER_SIZE - BFZ_CHECKSUM_SIZE. Otherwise,
 * the content of the buffer has the size of
 * BFZ_BUFFER_SIZE - BFZ_CHECKSUM_SIZE.
 *
 * If computing a checksum for the block is requested, this function
 * computes the checksum for the content in the buffer and stores
 * it at the end of the buffer.
 */
static void
write_bfz_buffer(bfz_t *bfz, bool isLast)
{
	struct bfz_freeable_stuff *fs = bfz->freeable_stuff;
	
	AssertImply(isLast, fs->buffer_pointer - fs->buffer <=
				sizeof(fs->buffer) - BFZ_CHECKSUM_SIZE(bfz->has_checksum));
	AssertImply(!isLast, fs->buffer_pointer -  fs->buffer ==
				sizeof(fs->buffer) - BFZ_CHECKSUM_SIZE(bfz->has_checksum));
	
	fs->tot_bytes += fs->buffer_pointer - fs->buffer + BFZ_CHECKSUM_SIZE(bfz->has_checksum);

	if (bfz->has_checksum)
	{
		BFZ_CHECKSUM_TYPE crc;

		Assert(fs->buffer_pointer - fs->buffer >= 0);
		crc =
			compute_checksum(fs->buffer, fs->buffer_pointer - fs->buffer);
		
		memcpy(fs->buffer_pointer, &crc, sizeof(BFZ_CHECKSUM_TYPE));
		fs->buffer_pointer += sizeof(BFZ_CHECKSUM_TYPE);
	}
	
	PG_TRY();
	{
		fs->write_ex(bfz, fs->buffer, fs->buffer_pointer - fs->buffer);
	}
	PG_CATCH();
	{
		Assert(!WorkfileDiskspace_IsFull());
		elog(gp_workfile_caching_loglevel, "BFZ write buffer failed, presumably because of OODS, seting flag");
		WorkfileDiskspace_SetFull(true /* isFull */);
		PG_RE_THROW();
	}
	PG_END_TRY();

	bfz->numBlocks ++;
}

/*
 * Read a buffer length of content from the bfz file into a given array.
 *
 * This function always tries to read BFZ_BUFFER_SIZE of bytes from
 * the bfz file, and stores them into the buffer.
 *
 * The returned value is the number of bytes that are actually read - 
 * BFZ_CHECKSUM_SIZE.
 *
 * Note that the caller should provide the given array to have the size of
 * at least BFZ_BUFFER_SIZE.
 */
static int
read_bfz_buffer(bfz_t *bfz, char *buffer)
{
	int bytesRead = 0;
	struct bfz_freeable_stuff *fs = bfz->freeable_stuff;
	int dataSize = 0;
	char *oldBuffer = NULL;
	
	/*
	 * Copy the original buffer so that we can simulate a torn page
	 * later.
	 */
	if (gp_workfile_faultinject)
	{
		oldBuffer = palloc(sizeof(fs->buffer));
		memcpy(oldBuffer, buffer, sizeof(fs->buffer));
	}

	bytesRead = fs->read_ex(bfz, buffer, sizeof(fs->buffer));
	Assert(bytesRead <= sizeof(fs->buffer));

	if (bytesRead == 0)
		return 0;

	dataSize = bytesRead;

	/*
	 * If size is greater than WORKFILE_SAFEWRITE_SIZE, and the GUC
	 * gp_workfile_faultinject is on, we simulate a torn page
	 * if this block is chosen to do so.
	 */
	if (dataSize > WORKFILE_SAFEWRITE_SIZE &&
		gp_workfile_faultinject)
	{
		if (bfz->blockNo == bfz->chosenBlockNo)
		{
			Assert(oldBuffer != NULL);
			
			/*
			 * Simulate a torn page by copying the data after
			 * WORKFILE_SAFEWRITE_SIZE in the old buffer into
			 * the new buffer.
			 */
			memcpy(buffer + WORKFILE_SAFEWRITE_SIZE,
				   oldBuffer + WORKFILE_SAFEWRITE_SIZE,
				   sizeof(fs->buffer) - WORKFILE_SAFEWRITE_SIZE);
			elog(NOTICE, "Simulate a torn page at block " INT64_FORMAT, bfz->blockNo);
		}
	}

	if (gp_workfile_faultinject)
		pfree(oldBuffer);

	if (bfz->has_checksum)
	{
		BFZ_CHECKSUM_TYPE storedCrc;
		BFZ_CHECKSUM_TYPE crc;

		dataSize -= sizeof(BFZ_CHECKSUM_TYPE);
		Assert(dataSize >= 0);

		/*
		 * Verify the stored checksum for this block with the computed
		 * value.
		 */
		crc = compute_checksum(buffer, dataSize);
		memcpy(&storedCrc, buffer + dataSize, sizeof(BFZ_CHECKSUM_TYPE));

		if (!BFZ_CHECKSUM_EQ(crc,storedCrc))
			ereport(ERROR,
					(errcode(ERRCODE_IO_ERROR),
					 errmsg("temporary file block checksum mismatch: current %u, "
							"expected %u", storedCrc, crc)));
	}

	bfz->blockNo++;

	return dataSize;
}

bfz_t *
bfz_create(const char *fileName, bool delOnClose, int compress)
{
	return bfz_create_internal(fileName,
							   false, /* open_existing */
							   delOnClose, compress);
}

/*
 * Open an existing bfz file. Returns NULL if it fails
 * e.g. if the file/path does not exist
 */
bfz_t *
bfz_open(const char *fileName, bool delOnClose, int compress)
{
	bfz_t *new_bfz;

	new_bfz = bfz_create_internal(fileName,
								  true, /* open_existing */
								  delOnClose, compress);

	/* Failed to open existing file. Inform the caller */
	if (new_bfz == NULL)
		return NULL;

	/*
	 * Since we are opening an existing file for reading,
	 * prepare the state for scan_begin.
	 */
	new_bfz->mode = BFZ_MODE_FREED;

	return new_bfz;
}

/*
 * Open or create a bfz file.
 *
 * Returns the bfz handle filled in with the file descriptor if successful,
 * NULL if could not open existing file.
 */
static bfz_t *
bfz_create_internal(const char *fileName, bool open_existing,
					bool delOnClose, int compress)
{
	struct bfz_freeable_stuff *fs;
	bfz_t *bfz_handle;

	bfz_handle = palloc0(sizeof(bfz_t));
	bfz_handle->filename = pstrdup(fileName);

	bfz_handle->file = OpenNamedFile(bfz_handle->filename,
									 !open_existing,
									 delOnClose,
									 true);
	if (bfz_handle->file == -1)
	{
		if (open_existing)
		{
			/*
			 * If we failed during opening an existing file, notify the caller
			 * instead of erroring out.
			 */
			pfree(bfz_handle->filename);
			pfree(bfz_handle);
			return NULL;
		}
		else
		{
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not create temporary file %s:%m", bfz_handle->filename)));
		}
	}

	bfz_handle->mode = BFZ_MODE_APPEND;
	bfz_handle->compression_index = compress;
	bfz_handle->del_on_close = delOnClose;

	compression_algorithms[compress].init(bfz_handle);

	bfz_handle->has_checksum = gp_workfile_checksumming;

	bfz_handle->numBlocks = bfz_handle->blockNo = bfz_handle->chosenBlockNo = 0;
	
	fs = bfz_handle->freeable_stuff;
	fs->tot_bytes = 0;

	if (open_existing)
	{
		/* We might have opened an existing files. Set the correct number
		 * of bytes in here.
		 */
		int64 ret;

		ret = FileSeek(bfz_handle->file, 0, SEEK_END);
		if (ret < 0)
			ereport(ERROR,
					(errcode_for_file_access(),
					 errmsg("could not seek in temporary file: %m")));

		fs->tot_bytes = ret;
	}

	fs->buffer_pointer = fs->buffer;
	fs->buffer_end = fs->buffer + sizeof(fs->buffer) - BFZ_CHECKSUM_SIZE(bfz_handle->has_checksum);

	return bfz_handle;
}

/*
 * bfz_close
 *		Close and free used resources
 */
void
bfz_close(bfz_t *thiz)
{
	if (thiz->freeable_stuff)
	{
		thiz->freeable_stuff->close_ex(thiz);
	}
	if (thiz->file > 0)
	{
		FileClose(thiz->file);
		thiz->file = -1;
	}

	/* If del_on_close was used, FileClose() unlinked the file */

	thiz->mode = BFZ_MODE_CLOSED;
	pfree(thiz);
}

/*
 * bfz_append_end
 *  Flushes data to a bfz file and then closes the file.
 *  It also makes a duplicate of the file descriptor so we can re-open it
 *  for reading using bfz_scan_begin at a later time.
 *
 *  It returns the actual size of the file on disk
 */
int64
bfz_append_end(bfz_t * thiz)
{
	struct bfz_freeable_stuff *fs = thiz->freeable_stuff;
	int64		tot_compressed,
				tot_bytes;

	Assert(thiz->mode == BFZ_MODE_APPEND);

	if (WorkfileDiskspace_IsFull())
	{
		elog(gp_workfile_caching_loglevel, "closing workfile while workfile diskspace full, skipping flush");
	}
	else
	{
		write_bfz_buffer(thiz, true);
	}

	tot_bytes = fs->tot_bytes;

	/*
	 * Close the compressor. But we keep the underlying file open for reading
	 */
	fs->close_ex(thiz);

	thiz->mode = BFZ_MODE_FREED;

	if ((tot_compressed = FileSeek(thiz->file, 0, SEEK_END)) == -1)
		ereport(ERROR,
				(errcode(ERRCODE_IO_ERROR),
				errmsg("could not seek in temporary file: %m")));

	elog(DEBUG1, "bfz file size uncompressed " INT64_FORMAT ", compressed with %s to " INT64_FORMAT", savings %d%%",
		 tot_bytes,
		 bfz_compression_to_string(thiz->compression_index),
		 tot_compressed,
		 tot_bytes == 0 ? 0 : (int) ((tot_bytes - tot_compressed) * 100 / tot_bytes));

	return tot_compressed;
}

void
bfz_scan_begin(bfz_t * thiz)
{
	struct bfz_freeable_stuff *fs;

	Assert(thiz->mode == BFZ_MODE_FREED);

	if (FileSeek(thiz->file, 0, SEEK_SET) == -1)
		ereport(ERROR,
				(errcode(ERRCODE_IO_ERROR),
				errmsg("could not seek in temporary file: %m")));

	thiz->mode = BFZ_MODE_SCAN;

	/*
	 * Allocating in the TopMemoryContext since this memory context
	 * is still available when calling the transaction callback at the
	 * time when the transaction aborts.
	 */
	MemoryContext oldcxt = MemoryContextSwitchTo(TopMemoryContext);

	compression_algorithms[thiz->compression_index].init(thiz);
	fs = thiz->freeable_stuff;
	fs->buffer_pointer = fs->buffer_end = fs->buffer;
	fs->tot_bytes = 0L;

	MemoryContextSwitchTo(oldcxt);

	if (gp_workfile_faultinject)
	{
		thiz->chosenBlockNo = (((double)random()) / ((double)MAX_RANDOM_VALUE)) * thiz->numBlocks;
		elog(LOG, "Test workfile checksumming: choose block " INT64_FORMAT
			 " to simulate a torn page",
			 thiz->chosenBlockNo);
	}
}

void
bfz_write_ex(bfz_t * thiz, const char *buffer, int size)
{
	struct bfz_freeable_stuff *fs = thiz->freeable_stuff;

	Assert(thiz->mode == BFZ_MODE_APPEND);
	while (size)
	{
		int	sizeToWrite =
			fs->buffer_end - fs->buffer_pointer;

		sizeToWrite = Min(size, sizeToWrite);
		size -= sizeToWrite;

		memcpy(fs->buffer_pointer, buffer, sizeToWrite);
		fs->buffer_pointer += sizeToWrite;
		if (fs->buffer_end == fs->buffer_pointer)
		{
			write_bfz_buffer(thiz, false);
			fs->buffer_pointer = fs->buffer;
			
		}

		buffer += sizeToWrite;
	}
}

int
bfz_read_ex(bfz_t * thiz, char *buffer, int size)
{
	struct bfz_freeable_stuff *fs = thiz->freeable_stuff;
	int			orig_size = size;

	Assert(thiz->mode == BFZ_MODE_SCAN);
	Assert(size >= 0);

	while (size)
	{
		int sizeForRead = fs->buffer_end - fs->buffer_pointer;

		Assert(sizeForRead >= 0 &&
			   sizeForRead <= sizeof(fs->buffer) - BFZ_CHECKSUM_SIZE(thiz->has_checksum));

		sizeForRead = Min(size, sizeForRead);
		memcpy(buffer, fs->buffer_pointer, sizeForRead);
		fs->buffer_pointer += sizeForRead;
		buffer += sizeForRead;
		size -= sizeForRead;
		Assert(size >= 0);
		Assert(fs->buffer_end >= fs->buffer_pointer);
		if (size > 0 && fs->buffer_pointer == fs->buffer_end)
		{
			/*
			 * If the requested size is greater than or equal to a block size
			 * of data (including the checksum), read and copy the
			 * data directly to the given buffer. We can't do that
			 * when the requested size is smaller than a block size, since
			 * the given buffer does not have space for the checksum.
			 */
			while (size >= sizeof(fs->buffer))
			{
				sizeForRead = read_bfz_buffer(thiz, buffer);
				fs->tot_bytes += sizeForRead;
				buffer += sizeForRead;
				size -= sizeForRead;
				Assert(size >= 0);
				if (sizeForRead == 0)
					break;
			}
			sizeForRead = read_bfz_buffer(thiz, fs->buffer);
			fs->tot_bytes += sizeForRead;
			fs->buffer_pointer = fs->buffer;
			fs->buffer_end = fs->buffer + sizeForRead;
			if (sizeForRead == 0)
				break;
		}
	}
	return orig_size - size;
}
