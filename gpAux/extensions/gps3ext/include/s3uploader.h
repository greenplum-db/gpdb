#ifndef __S3UPLOADER_H__
#define __S3UPLOADER_H__

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>
// #include <cstdint>
#include <cstring>

#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <curl/curl.h>

#include "s3common.h"
#include "s3url_parser.h"

using std::vector;

struct Uploader {
    Uploader();
    ~Uploader();

    bool init(const char *data, S3Credential *cred);
    bool write(char *buf, uint64_t &len);
    void destroy();

   private:
    // pthread_t* threads;
};

const char *GetUploadId(const char *host, const char *bucket, const char *obj_name,
                        const S3Credential &cred);

const char *PartPutS3Object(const char *host, const char *bucket, const char *obj_name,
                            const S3Credential &cred, const char *data, uint64_t data_size,
                            uint64_t part_number, const char *upload_id);

bool CompleteMultiPutS3(const char *host, const char *bucket, const char *obj_name,
                        const char *upload_id, const char **etag_array, uint64_t count,
                        const S3Credential &cred);

#endif
