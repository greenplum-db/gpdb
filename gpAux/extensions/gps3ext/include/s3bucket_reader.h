#ifndef __S3_BUCKET_READER__
#define __S3_BUCKET_READER__

#include <string>

#include "reader.h"
#include "s3interface.h"
#include "s3key_reader.h"

using std::string;

// S3BucketReader read multiple files in a bucket.
class S3BucketReader : public Reader {
   public:
    S3BucketReader();
    virtual ~S3BucketReader();

    void open(const S3Params &params);
    uint64_t read(char *buf, uint64_t count);
    void close();

    void setS3interface(S3Interface *s3) {
        this->s3interface = s3;
    }

    void setUpstreamReader(Reader *reader) {
        this->upstreamReader = reader;
    }

    void parseURL();
    void parseURL(const string &url) {
        this->params.setBaseUrl(url);
        parseURL();
    };

    const ListBucketResult &getKeyList() {
        return keyList;
    }

    const string &getRegion() {
        return region;
    }
    const string &getBucket() {
        return bucket;
    }
    const string &getPrefix() {
        return prefix;
    }

   protected:
    // Get URL for a S3 object/file.
    string getKeyURL(const string &key);

   private:
    S3Params params;

    string schema;
    string region;
    string bucket;
    string prefix;

    S3Interface *s3interface;

    // upstreamReader is where we get data from.
    Reader *upstreamReader;
    bool needNewReader;

    ListBucketResult keyList;  // List of matched keys/files.
    uint64_t keyIndex;         // BucketContent index of keylist->contents.

    BucketContent *getNextKey();
    S3Params constructReaderParams(BucketContent *key);
};

#endif
