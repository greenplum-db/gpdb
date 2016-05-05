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

S3Reader* reader_init(const char* url_with_options);

bool reader_transfer_data(S3Reader* reader, char* data_buf, int& data_len);

bool reader_cleanup(S3Reader** reader);

#endif
