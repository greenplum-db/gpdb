#ifndef __S3_MACROS_H__
#define __S3_MACROS_H__

#include "s3common_headers.h"

#define PRINTOUT_BUFFER_LEN 1024

extern void StringAppendPrintf(std::string *output, const char *format, ...);

#define CHECK_OR_DIE_MSG(_condition, _format, _args...)                                           \
    do {                                                                                          \
        if (!(_condition)) {                                                                      \
            std::string _error_str;                                                               \
            StringAppendPrintf(&_error_str, _format, _args);                                      \
            std::stringstream _err_msg;                                                           \
            _err_msg << _error_str << ", Function: " << __func__ << ", File: " << __FILE__ << "(" \
                     << __LINE__ << "). ";                                                        \
            throw std::runtime_error(_err_msg.str());                                             \
        }                                                                                         \
    } while (false)

#define CHECK_ARG_OR_DIE(_arg) CHECK_OR_DIE_MSG(_arg, "Null pointer of argument: '%s'", #_arg)

#define CHECK_OR_DIE(_condition)                                                             \
    do {                                                                                     \
        if (!(_condition)) {                                                                 \
            std::stringstream _err_msg;                                                      \
            _err_msg << "Failed expression: (" << #_condition << "), Function: " << __func__ \
                     << ", File: " << __FILE__ << "(" << __LINE__ << "). ";                  \
            throw std::runtime_error(_err_msg.str());                                        \
        }                                                                                    \
    } while (false)

inline void VStringAppendPrintf(string *output, const char *format, va_list argp) {
    CHECK_OR_DIE(output);
    char buffer[PRINTOUT_BUFFER_LEN];
    vsnprintf(buffer, sizeof(buffer), format, argp);
    output->append(buffer);
}

inline void StringAppendPrintf(string *output, const char *format, ...) {
    va_list argp;
    va_start(argp, format);
    VStringAppendPrintf(output, format, argp);
    va_end(argp);
}

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
