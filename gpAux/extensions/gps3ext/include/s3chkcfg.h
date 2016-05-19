#ifndef __S3_CHK_CFG__
#define __S3_CHK_CFG__

#include <unistd.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "s3common.h"
#include "s3conf.h"
#include "s3downloader.h"
#include "s3log.h"
#include "s3wrapper.h"

#define BUF_SIZE 64 * 1024

extern volatile bool QueryCancelPending;

extern S3Reader *wrapper;

void print_template();

void print_usage(FILE *stream);

bool read_config(const char *config);

ListBucketResult *list_bucket(S3Reader *wrapper);

uint8_t print_contents(ListBucketResult *r);

bool check_config(const char *url_with_options);

bool s3_download(const char *url_with_options);

#endif
