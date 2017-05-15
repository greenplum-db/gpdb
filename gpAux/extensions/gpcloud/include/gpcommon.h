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
typedef int32_t int32;
typedef int32 int4;
typedef struct GpId {
    int4 numsegments; /* count of distinct segindexes */
    int4 dbid;        /* the dbid of this database */
    int4 segindex;    /* content indicator: -1 for entry database,
                       * 0, ..., n-1 for segment database *
                       * a primary and its mirror have the same segIndex */
} GpId;
extern GpId GpIdentity;

#endif
