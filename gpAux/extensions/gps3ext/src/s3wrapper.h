#ifndef __S3_EXT_WRAPPER__
#define __S3_EXT_WRAPPER__

#include <string>

#include "s3downloader.h"
#include "s3uploader.h"

using std::string;

class S3ExtBase {
   public:
    S3ExtBase(const string& url);
    virtual ~S3ExtBase();
    virtual bool Init(int segid, int segnum, int chunksize) = 0;
    virtual bool TransferData(char* data, uint64_t& len) = 0;
    virtual bool Destroy() = 0;
    virtual bool ValidateURL();

    string get_region() { return this->region; }

   protected:
    S3Credential cred;

    string url;
    string schema;
    string bucket;
    string prefix;
    string region;

    int segid;
    int segnum;

    int concurrent_num;
    int chunksize;
};

class S3Reader : public S3ExtBase {
   public:
    S3Reader(const string& url);
    virtual ~S3Reader();
    virtual bool Init(int segid, int segnum, int chunksize);
    virtual bool TransferData(char* data, uint64_t& len);
    virtual bool Destroy();

   protected:
    virtual string getKeyURL(const string& key);
    void getNextDownloader();

    // private:
    unsigned int contentindex;
    Downloader* filedownloader;
    ListBucketResult* keylist;
};

class S3Writer : public S3ExtBase {};

extern "C" S3ExtBase* CreateExtWrapper(const char* url);

#endif
