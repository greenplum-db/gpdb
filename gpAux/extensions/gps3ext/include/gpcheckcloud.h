#ifndef __GP_CHECK_CLOUD__
#define __GP_CHECK_CLOUD__

#include <unistd.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "gpcommon.h"
#include "gpreader.h"
#include "s3common.h"
#include "s3conf.h"
#include "s3interface.h"
#include "s3key_reader.h"
#include "s3log.h"

#define BUF_SIZE 64 * 1024

extern volatile bool QueryCancelPending;

void print_template();

void print_usage(FILE *stream);

uint8_t print_contents(ListBucketResult *r);

bool check_config(const char *url_with_options);

bool s3_download(const char *url_with_options);

#endif
