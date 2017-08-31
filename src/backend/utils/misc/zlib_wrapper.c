/*-------------------------------------------------------------------------
 *
 * zlib_wrapper.c
 *	 Wrapper functions for zlib to provide better integration with GPDB
 *
 * Greenplum Database
 * Copyright (c) 2016-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/backend/utils/misc/zlib_wrapper.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "utils/palloc.h"
#include "utils/zlib_wrapper.h"

/*
 * Wrapper for palloc to be passed into zlib.
 */
static
voidpf gp_zpalloc (voidpf opaque, unsigned items, unsigned size)
{
    return (voidpf) palloc(items * size);
}

/*
 * Wrapper for pfree to be passed into zlib.
 */
static
void gp_zpfree (voidpf opaque, voidpf ptr)
{
    pfree(ptr);
}



/* ===========================================================================
     Decompresses the source buffer into the destination buffer.  sourceLen is
   the byte length of the source buffer. Upon entry, destLen is the total
   size of the destination buffer, which must be large enough to hold the
   entire uncompressed data. (The size of the uncompressed data must have
   been saved previously by the compressor and transmitted to the decompressor
   by some mechanism outside the scope of this compression library.)
   Upon exit, destLen is the actual size of the compressed buffer.

     uncompress returns Z_OK if success, Z_MEM_ERROR if there was not
   enough memory, Z_BUF_ERROR if there was not enough room in the output
   buffer, or Z_DATA_ERROR if the input data was corrupted.
*/
int
gp_uncompress (Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen)
{
    z_stream stream;
    int err;

    stream.next_in = (Bytef*)source;
    stream.avail_in = (uInt)sourceLen;
    /* Check for source > 64K on 16-bit machine: */
    if ((uLong)stream.avail_in != sourceLen)
    {
    	return Z_BUF_ERROR;
    }

    stream.next_out = dest;
    stream.avail_out = (uInt)*destLen;
    if ((uLong)stream.avail_out != *destLen)
    {
    	return Z_BUF_ERROR;
    }

    stream.zalloc = (alloc_func)gp_zpalloc;
    stream.zfree = (free_func)gp_zpfree;

    err = inflateInit(&stream);
    if (err != Z_OK)
    {
    	return err;
    }

    err = inflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END)
    {
        inflateEnd(&stream);
        if (err == Z_NEED_DICT || (err == Z_BUF_ERROR && stream.avail_in == 0))
        {
        	return Z_DATA_ERROR;
        }
        return err;
    }
    *destLen = stream.total_out;

    err = inflateEnd(&stream);
    return err;
}


/*
 * compressBound doesn't exist in older zlibs, so let's use our own
 */
unsigned long
gp_compressBound(unsigned long sourceLen)
{
  return sourceLen + (sourceLen >> 12) + (sourceLen >> 14) + 11;
}

/* ===========================================================================
     Compresses the source buffer into the destination buffer. The level
   parameter has the same meaning as in deflateInit.  sourceLen is the byte
   length of the source buffer. Upon entry, destLen is the total size of the
   destination buffer, which must be at least 0.1% larger than sourceLen plus
   12 bytes. Upon exit, destLen is the actual size of the compressed buffer.

     compress2 returns Z_OK if success, Z_MEM_ERROR if there was not enough
   memory, Z_BUF_ERROR if there was not enough room in the output buffer,
   Z_STREAM_ERROR if the level parameter is invalid.
*/
int
gp_compress2(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int level)
{
    z_stream stream;
    int err;

    stream.next_in = (Bytef*)source;
    stream.avail_in = (uInt)sourceLen;
#ifdef MAXSEG_64K
    /* Check for source > 64K on 16-bit machine: */
    if ((uLong)stream.avail_in != sourceLen)
    {
    	return Z_BUF_ERROR;
    }
#endif
    stream.next_out = dest;
    stream.avail_out = (uInt)*destLen;
    if ((uLong)stream.avail_out != *destLen)
    {
    	return Z_BUF_ERROR;
    }

    stream.zalloc = (alloc_func)gp_zpalloc;
    stream.zfree = (free_func)gp_zpfree;
    stream.opaque = (voidpf)0;

    err = deflateInit(&stream, level);
    if (err != Z_OK)
    {
    	return err;
    }

    err = deflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END)
    {
        deflateEnd(&stream);
        return err == Z_OK ? Z_BUF_ERROR : err;
    }
    *destLen = stream.total_out;

    err = deflateEnd(&stream);
    return err;
}

/* ===========================================================================
 */
int gp_compress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen)
{
    return gp_compress2(dest, destLen, source, sourceLen, Z_DEFAULT_COMPRESSION);
}


/* EOF */
