#ifndef __S3_LOG_H__
#define __S3_LOG_H__

#include "s3common_headers.h"
#include "s3conf.h"
#include "s3exception.h"

// log level
enum LOGLEVEL { EXT_FATAL, EXT_ERROR, EXT_WARNING, EXT_INFO, EXT_DEBUG };

// log type
enum LOGTYPE {
    REMOTE_LOG,    // log to remote UDP server
    LOCAL_LOG,     // log to local Unix domain socket
    INTERNAL_LOG,  // use PostgreSQL's elog()
    STDERR_LOG     // use STDERR
};

void LogMessage(LOGLEVEL level, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

LOGTYPE getLogType(const char* v);
LOGLEVEL getLogLevel(const char* v);

#define PRINTFUNCTION(i, format, ...) LogMessage(i, format, __VA_ARGS__)

#define LOG_FMT "[%s]#%d#(%" PRIX64 ")%s:%d  "
#define LOG_ARGS(LOGLEVELSTR) LOGLEVELSTR, s3ext_segid, (uint64_t) pthread_self(), __FILE__, __LINE__
#define NEWLINE "\n"

#define S3DEBUG(message, args...)    \
    if (EXT_DEBUG <= s3ext_loglevel) \
    PRINTFUNCTION(EXT_DEBUG, LOG_FMT message NEWLINE, LOG_ARGS("D"), ##args)

#define S3INFO(message, args...)    \
    if (EXT_INFO <= s3ext_loglevel) \
    PRINTFUNCTION(EXT_INFO, LOG_FMT message NEWLINE, LOG_ARGS("I"), ##args)

#define S3WARN(message, args...)       \
    if (EXT_WARNING <= s3ext_loglevel) \
    PRINTFUNCTION(EXT_WARNING, LOG_FMT message NEWLINE, LOG_ARGS("W"), ##args)

#define S3ERROR(message, args...)    \
    if (EXT_ERROR <= s3ext_loglevel) \
    PRINTFUNCTION(EXT_ERROR, LOG_FMT message NEWLINE, LOG_ARGS("E"), ##args)

void InitRemoteLog();

#endif  // __S3_LOG_H__
