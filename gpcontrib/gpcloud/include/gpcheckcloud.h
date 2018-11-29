#ifndef __GP_CHECK_CLOUD_H__
#define __GP_CHECK_CLOUD_H__

#include "gpreader.h"
#include "gpwriter.h"
#include "s3common_headers.h"
#include "s3interface.h"
#include "s3memory_mgmt.h"

#define BUF_SIZE 64 * 1024

extern volatile bool QueryCancelPending;
extern bool S3QueryIsAbortInProgress(void);

#endif
