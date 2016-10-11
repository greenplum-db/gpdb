#ifndef __S3_MACROS_H__
#define __S3_MACROS_H__

#include "s3common_headers.h"
#include "s3log.h"

#define PRINTOUT_BUFFER_LEN 1024

#define S3_CHECK_OR_DIE(_condition, _type, _args...)     \
    do {                                                 \
        if (!(_condition)) {                             \
            _type _except(_args);                        \
            _except.file = __FILE__;                     \
            _except.line = __LINE__;                     \
            _except.func = __func__;                     \
            S3ERROR("%s", _except.getMessage().c_str()); \
            throw _except;                               \
        }                                                \
    } while (false)

#define S3_DIE(_type, _args...) S3_CHECK_OR_DIE(false, _type, _args)

// chunk size for compression/decompression
//    declare them here so UT tests can access each as well
extern uint64_t S3_ZIP_DECOMPRESS_CHUNKSIZE;
extern uint64_t S3_ZIP_COMPRESS_CHUNKSIZE;

#define S3_ZIP_DEFAULT_CHUNKSIZE (1024 * 1024 * 2)

// For deflate, windowBits can be greater than 15 for optional gzip encoding. Add 16 to windowBits
// to write a simple gzip header and trailer around the compressed data instead of a zlib wrapper.
#define S3_DEFLATE_WINDOWSBITS (MAX_WBITS + 16)

// For inflate, windowBits can be greater than 15 for optional gzip decoding. Add 32 to windowBits
// to enable zlib and gzip decoding with automatic header detection.
#define S3_INFLATE_WINDOWSBITS (MAX_WBITS + 16 + 16)

#endif
