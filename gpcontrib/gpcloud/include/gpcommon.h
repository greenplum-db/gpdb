#ifndef __GP_COMMON_H
#define __GP_COMMON_H

#include "s3common_headers.h"

// GPDB's global variable
extern volatile bool QueryCancelPending;
extern bool S3QueryIsAbortInProgress(void);

#define EOL_STRING_MAX_LEN 4  // 'LF', 'CR', 'CRLF'
#define EOL_CHARS_MAX_LEN 2   // '\n', '\r', '\r\n'
extern char eolString[];
extern bool hasHeader;

// TODO change to functions getgpsegmentId() and getgpsegmentCount()

#endif
