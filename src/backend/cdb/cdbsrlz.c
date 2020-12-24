/*-------------------------------------------------------------------------
 *
 * cdbsrlz.c
 *	  Serialize a PostgreSQL sequential plan tree.
 *
 * Portions Copyright (c) 2004-2008, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 *
 *
 * IDENTIFICATION
 *	    src/backend/cdb/cdbsrlz.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "cdb/cdbsrlz.h"
#include "nodes/nodes.h"
#include "utils/memutils.h"

#ifdef HAVE_LIBZSTD
/* Zstandard library is provided */

#include <zstd.h>

static char *compress_string(const char *src, int uncompressed_size, int *compressed_size_p);
static char *uncompress_string(const char *src, int size, int *uncompressed_size_p);

/* zstandard compression level to use. */
#define COMPRESS_LEVEL 3

#endif			/* HAVE_LIBZSTD */

/*
 * This is used by dispatcher to serialize Plan and Query Trees for
 * dispatching to qExecs.
 * The returned string is palloc'ed in the current memory context.
 */
char *
serializeNode(Node *node, int *size, int *uncompressed_size_out)
{
	char	   *pszNode;
	char	   *sNode;
	int			uncompressed_size;

	Assert(node != NULL);
	Assert(size != NULL);

	pszNode = nodeToBinaryStringFast(node, &uncompressed_size);
	Assert(pszNode != NULL);

	/* If we have been compiled with libzstd, use it to compress it */
#ifdef HAVE_LIBZSTD
	sNode = compress_string(pszNode, uncompressed_size, size);
	pfree(pszNode);
#else
	sNode = pszNode;
	*size = uncompressed_size;
#endif

	if (NULL != uncompressed_size_out)
		*uncompressed_size_out = uncompressed_size;
	return sNode;
}

/*
 * This is used on the qExecs to deserialize serialized Plan and Query Trees
 * received from the dispatcher.
 * The returned node is palloc'ed in the current memory context.
 */
Node *
deserializeNode(const char *strNode, int size)
{
	Node	   *node;
#ifdef HAVE_LIBZSTD
	char	   *sNode;
	int			uncompressed_len;
#endif			/* HAVE_LIBZSTD */

	Assert(strNode != NULL);

	/* If we have been compiled with libzstd, decompress */
#ifdef HAVE_LIBZSTD
	sNode = uncompress_string(strNode, size, &uncompressed_len);
	Assert(sNode != NULL);
	node = readNodeFromBinaryString(sNode, uncompressed_len);
	pfree(sNode);
#else
	node = readNodeFromBinaryString(strNode, size);
#endif			/* HAVE_LIBZSTD */

	return node;
}

#ifdef HAVE_LIBZSTD
/*
 * Compress a (binary) string using libzstd
 *
 * returns the compressed data and the size of the compressed data.
 */
static char *
compress_string(const char *src, int uncompressed_size, int *size)
{
	static ZSTD_CCtx  *cxt = NULL;      /* ZSTD compression context */
	size_t		compressed_size;
	size_t		dst_length_used;
	char	   *result;

	if (!cxt)
	{
		cxt = ZSTD_createCCtx();
		if (!cxt)
			elog(ERROR, "out of memory");
	}

	compressed_size = ZSTD_compressBound(uncompressed_size);	/* worst case */

	result = palloc(compressed_size);

	dst_length_used = ZSTD_compressCCtx(cxt,
										result, compressed_size,
										src, uncompressed_size,
										COMPRESS_LEVEL);
	if (ZSTD_isError(dst_length_used))
		elog(ERROR, "Compression failed: %s uncompressed len %d",
			 ZSTD_getErrorName(dst_length_used), uncompressed_size);

	*size = dst_length_used;
	return (char *) result;
}

/*
 * Uncompress the binary string
 */
static char *
uncompress_string(const char *src, int size, int *uncompressed_size_p)
{
	static ZSTD_DCtx  *cxt = NULL;      /* ZSTD decompression context */
	char	   *result;
	unsigned long long uncompressed_size;
	int			dst_length_used;

	*uncompressed_size_p = 0;

	if (!cxt)
	{
		cxt = ZSTD_createDCtx();
		if (!cxt)
			elog(ERROR, "out of memory");
	}

	Assert(size >= sizeof(int));

	uncompressed_size = ZSTD_getFrameContentSize(src, size);
	if (uncompressed_size == ZSTD_CONTENTSIZE_UNKNOWN)
		elog(ERROR, "decompressed size not known");
	if (uncompressed_size == ZSTD_CONTENTSIZE_ERROR)
		elog(ERROR, "invalid compressed data");

	if (uncompressed_size > MaxAllocSize)
		elog(ERROR, "decompressed plan tree too large (" UINT64_FORMAT " bytes)",
			 (uint64) uncompressed_size);

	result = palloc(uncompressed_size);

	dst_length_used = ZSTD_decompressDCtx(cxt,
										  result, uncompressed_size,
										  src, size);
	if (ZSTD_isError(dst_length_used))
	{
		elog(ERROR, "%s", ZSTD_getErrorName(dst_length_used));
	}
	Assert(dst_length_used == uncompressed_size);

	*uncompressed_size_p = dst_length_used;

	return (char *) result;
}
#endif			/* HAVE_LIBZSTD */
