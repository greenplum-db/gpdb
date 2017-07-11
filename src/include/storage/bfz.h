#ifndef BFZ_H
#define BFZ_H

#include "storage/fd.h"

#define BFZ_MODE_CLOSED		0
#define BFZ_MODE_APPEND		1
#define BFZ_MODE_FREED		2
#define BFZ_MODE_SCAN		3

#define BFZ_BUFFER_SIZE		(1<<14)

struct bfz;

struct bfz_freeable_stuff
{
	int64 tot_bytes;
	char *buffer_pointer;
	char *buffer_end;

/*
 * We always call read_ex with size==BFZ_BUFFER_SIZE.
 * The checksumming algorithm relies on this to determine the
 * boundary of a buffer that a checksum is applied to.
 */
	int	(*read_ex) (struct bfz * thiz, char *buffer, int size);

/*
 * We always call write_ex with size==BFZ_BUFFER_SIZE, except for the
 * last write. The checksumming algorithm relies on this to
 * determine the boundary of a buffer that a checksum is applied to.
 */
	void (*write_ex) (struct bfz * thiz, const char *buffer, int size);
	void (*close_ex) (struct bfz * thiz);

	char buffer[BFZ_BUFFER_SIZE];
};

typedef struct bfz
{
	struct bfz_freeable_stuff *freeable_stuff;
	File		file;
	char        *filename;
	unsigned char mode;
	unsigned char compression_index;
	bool del_on_close;

	/* Indicate if this bfz file stores block checksums. */
	bool has_checksum;

	/*
	 * The following are used to simulate a torn page.
	 *
	 * 'numBlocks' represents total number of blocks this bfz file contains.
	 * 'blockNo' represents the number for the block that is currently accessed.
	 * 'chosenBlockNo' represents the number for the block that will become
	 *    a torn page.
	 */
	int64 numBlocks;
	int64 blockNo;
	int64 chosenBlockNo;
}	bfz_t;

/* These functions are internal to bfz. */
extern void bfz_nothing_init(bfz_t * thiz);
extern void bfz_zlib_init(bfz_t * thiz);
extern void bfz_lzop_init(bfz_t * thiz);
extern void bfz_write_ex(bfz_t * thiz, const char *buffer, int size);
extern int	bfz_read_ex(bfz_t * thiz, char *buffer, int size);

/* These functions are interface to bfz. */
extern int	bfz_string_to_compression(const char *string);

extern bfz_t *bfz_create(const char *filePrefix, bool delOnClose, int compress);
extern bfz_t *bfz_open(const char *fileName, bool delOnClose, int compress);
extern int64 bfz_append_end(bfz_t * thiz);
extern void bfz_scan_begin(bfz_t * thiz);
extern void bfz_close(bfz_t *thiz);

static inline int64
bfz_totalbytes(bfz_t * bfz)
{
	Assert(NULL != bfz);
	int64 size = 0;
	if (NULL != bfz->freeable_stuff)
	{
		size = bfz->freeable_stuff->tot_bytes;
	}
	return size;
}

static inline void
bfz_append(bfz_t * thiz, const char *buffer, int size)
{
	struct bfz_freeable_stuff *fs = thiz->freeable_stuff;

	Assert(size >= 0);
	Assert(fs != NULL);
	Assert(thiz->mode == BFZ_MODE_APPEND);
	if (fs->buffer_pointer + size <= fs->buffer_end)
	{
		memcpy(fs->buffer_pointer, buffer, size);
		fs->buffer_pointer += size;
	}
	else
		bfz_write_ex(thiz, buffer, size);

	return;
}

/*
 * If the data requested is already in the bfz buffer, this function
 * returns a pointer to the data in the buffer.
 * If more data needs to be read from disk, this function returns NULL and
 * the caller must call bfz_scan_next.
 */
static inline void *
bfz_scan_peek(bfz_t * thiz, int size)
{
	struct bfz_freeable_stuff *fs = thiz->freeable_stuff;

	Assert(size >= 0);
	Assert(fs != NULL);
	Assert(thiz->mode == BFZ_MODE_SCAN);
	if (fs->buffer_pointer + size > fs->buffer_end)
		return NULL;
	fs->buffer_pointer += size;

	return fs->buffer_pointer - size;
}

static inline int
bfz_scan_next(bfz_t * thiz, char *buffer, int size)
{

	Assert(size >= 0);
	Assert(thiz->mode == BFZ_MODE_SCAN || thiz->mode == BFZ_MODE_FREED);

	/* First time we're reading from file, open it for reading */
	if (thiz->mode == BFZ_MODE_FREED)
	{
		bfz_scan_begin(thiz);
	}

	struct bfz_freeable_stuff *fs = thiz->freeable_stuff;
	Assert(fs != NULL);

	if (fs->buffer_pointer + size <= fs->buffer_end)
	{
		memcpy(buffer, fs->buffer_pointer, size);
		fs->buffer_pointer += size;
		return size;
	}

	return bfz_read_ex(thiz, buffer, size);
}

#endif   /* BFZ_H */
