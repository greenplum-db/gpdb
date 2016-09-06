#ifndef __S3_COMMON_HEADERS_H__
#define __S3_COMMON_HEADERS_H__

#include <curl/curl.h>
#include <fcntl.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>
#include <openssl/ssl.h>
#include <pthread.h>
#include <signal.h>
#include <zlib.h>
#include <algorithm>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using std::map;
using std::vector;
using std::string;
using std::stringstream;

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "http_parser.h"
#include "ini.h"

class S3Params;

bool InitConfig(S3Params &params, const string &path, const string &section);
bool InitConfig(S3Params &params, const string &urlWithOptions);
#endif
