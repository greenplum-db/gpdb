#ifndef __S3_BUCKET_READER__
#define __S3_BUCKET_READER__

#include "reader.h"
#include "s3common_headers.h"
#include "s3exception.h"
#include "s3interface.h"

// S3BucketReader read multiple files in a bucket.
class S3BucketReader : public Reader {
   public:
    S3BucketReader();
    virtual ~S3BucketReader();

    void open(const S3Params &params);
    uint64_t read(char *buf, uint64_t count);
    void close();

    void setS3InterfaceService(S3Interface *s3) {
        this->s3Interface = s3;
    }

    void setUpstreamReader(Reader *reader) {
        this->upstreamReader = reader;
    }

    const ListBucketResult &getKeyList() {
        return keyList;
    }

   private:
    S3Params params;

    S3Interface *s3Interface;

    // upstreamReader is where we get data from.
    Reader *upstreamReader;
    bool needNewReader;

    // when load multiple files on one segment and each of them has a header line,
    // we should read header line only for the 1st file and ignore remainings.
    bool isFirstFile;

    // Skip the header line (terminated with eol) if necessary.
    // copy valid data into buf and return its size.
    uint64_t readWithoutHeaderLine(char *buf, uint64_t count);

    ListBucketResult keyList;  // List of matched keys/files.
    uint64_t keyIndex;         // BucketContent index of keylist->contents.

    BucketContent &getNextKey();
    S3Params constructReaderParams(BucketContent &key);
};

#endif
