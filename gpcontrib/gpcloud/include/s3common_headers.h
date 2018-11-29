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
#include <zlib.h>
#include <algorithm>
#include <csignal>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using std::map;
using std::string;
using std::stringstream;
using std::vector;

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "http_parser.h"
#include "ini.h"

#define DATE_STR_LEN 9
#define TIME_STAMP_STR_LEN 17

class S3Params;

extern string s3extErrorMessage;

class UniqueLock {
   public:
    UniqueLock(pthread_mutex_t *m) : mutex(m) {
        pthread_mutex_lock(this->mutex);
    }
    ~UniqueLock() {
        pthread_mutex_unlock(this->mutex);
    }

   private:
    pthread_mutex_t *mutex;
};

struct S3Credential {
    bool operator==(const S3Credential &other) const {
        return this->accessID == other.accessID && this->secret == other.secret &&
               this->token == other.token;
    }

    string accessID;
    string secret;
    string token;
};

S3Params InitConfig(const string &urlWithOptions);

void MaskThreadSignals();
#endif
