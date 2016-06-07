#ifndef TEST_MOCK_CLASSES_H_
#define TEST_MOCK_CLASSES_H_

#include "gmock/gmock.h"

#include "s3common.h"
#include "s3interface.h"

#include <string>
using std::string;

class MockS3Interface : public S3Interface {
   public:
    MOCK_METHOD5(listBucket,
                 ListBucketResult*(const string& schema, const string& region, const string& bucket,
                                   const string& prefix, const S3Credential& cred));

    MOCK_METHOD6(fetchData,
                 uint64_t(uint64_t offset, char *data, uint64_t len, const string &sourceUrl,
                          const string &region, const S3Credential &cred));
};

#endif /* TEST_MOCK_CLASSES_H_ */