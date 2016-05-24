#ifndef __S3_EXTBASE__
#define __S3_EXTBASE__

#include <string>

#include "s3common.h"

using std::string;

// Base class for reader and writer wrapper for GPDB, includes common
// data and functions for both reader and writer.
class S3ExtBase {
   public:
    S3ExtBase(const string& url);
    virtual ~S3ExtBase();

    // Init initializes segment information, chunk size and threads.
    virtual bool Init(int segid, int segnum, int chunksize) = 0;
    // Transfer data between endpoints including download and upload.
    virtual bool TransferData(char* data, uint64_t& len) = 0;
    // Destroy allocated resources during initialization.
    virtual bool Destroy() = 0;
    // Check whether URL is valid or not.
    virtual bool ValidateURL();

    string get_region() { return this->region; }
    string get_bucket() { return this->bucket; }
    string get_prefix() { return this->prefix; }

   protected:
    S3Credential cred;

    string url;
    string schema;
    string region;
    string bucket;
    string prefix;

    int segid;
    int segnum;

    int concurrent_num;
    int chunksize;

   private:
    void SetSchema();
    void SetRegion();
    void SetBucketAndPrefix();
};

#endif
