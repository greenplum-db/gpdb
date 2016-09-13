#ifndef __S3_COMMON_H__
#define __S3_COMMON_H__

#include "s3common_headers.h"
#include "s3conf.h"
#include "s3http_headers.h"
#include "s3macros.h"
#include "s3utils.h"

struct S3Credential;

extern string s3extErrorMessage;

void CheckEssentialConfig(const S3Params& params);

#define DATE_STR_LEN 9
#define TIME_STAMP_STR_LEN 17

void SignRequestV4(const string& method, HTTPHeaders* h, const string& orig_region,
                   const string& path, const string& query, const S3Credential& cred);

string getOptS3(const string& options, const string& key);

string truncate_options(const string& url_with_options);

class UniqueLock {
   public:
    UniqueLock(pthread_mutex_t* m) : mutex(m) {
        pthread_mutex_lock(this->mutex);
    }
    ~UniqueLock() {
        pthread_mutex_unlock(this->mutex);
    }

   private:
    pthread_mutex_t* mutex;
};

#endif  // __S3_COMMON_H__
