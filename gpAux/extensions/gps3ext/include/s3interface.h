#ifndef INCLUDE_S3INTERFACE_H_
#define INCLUDE_S3INTERFACE_H_

#include "s3reader.h"

class S3Interface {
   public:
    virtual ~S3Interface() {}

    // It is caller's responsibility to free returned memory.
    virtual ListBucketResult* ListBucket(const string& schema,
                                         const string& region,
                                         const string& bucket,
                                         const string& prefix,
                                         const S3Credential& cred) = 0;
};

class S3Service : public S3Interface {
   public:
    S3Service();
    virtual ~S3Service();
    ListBucketResult* ListBucket(const string& schema, const string& region,
                                 const string& bucket, const string& prefix,
                                 const S3Credential& cred);
};

#endif /* INCLUDE_S3INTERFACE_H_ */
