#ifndef __S3_READER_H__
#define __S3_READER_H__

#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <curl/curl.h>
#include <zlib.h>

#include "s3common.h"
#include "s3macros.h"
#include "s3url_parser.h"

using std::vector;
using std::stringstream;

struct Range {
    /* data */
    uint64_t offset;
    uint64_t len;
};

typedef enum compression_type {
    S3_ZIP_NONE,
    S3_ZIP_GZIP,
} compression_type_t;

class OffsetMgr {
   public:
    OffsetMgr(uint64_t maxsize, uint64_t chunksize);
    ~OffsetMgr() { pthread_mutex_destroy(&this->offset_lock); };
    Range NextOffset();  // ret.len == 0 means EOF
    void Reset(uint64_t n);
    uint64_t Chunksize() { return this->chunksize; };
    uint64_t Size() { return this->maxsize; };

   private:
    pthread_mutex_t offset_lock;
    uint64_t maxsize;
    uint64_t chunksize;
    uint64_t curpos;
};

class BlockingBuffer {
   public:
    static BlockingBuffer* CreateBuffer(const string& url, const string& region, OffsetMgr* o,
                                        S3Credential* pcred);
    BlockingBuffer(const string& url, OffsetMgr* o);
    virtual ~BlockingBuffer();
    bool Init();
    bool EndOfFile() { return this->eof; };
    bool Error() { return this->error; };

    uint64_t Read(char* buf, uint64_t len);
    uint64_t Fill();

    static const int STATUS_EMPTY = 0;
    static const int STATUS_READY = 1;

    /* data */
   protected:
    string sourceurl;
    uint64_t bufcap;
    virtual uint64_t fetchdata(uint64_t offset, char* data, uint64_t len) = 0;

   private:
    int status;
    bool eof;
    bool error;
    pthread_mutex_t stat_mutex;
    pthread_cond_t stat_cond;
    uint64_t readpos;
    uint64_t realsize;
    char* bufferdata;
    OffsetMgr* mgr;
    Range nextpos;
};

struct zstream_info {
    z_stream zstream;
    bool inited;
    unsigned char* in;
    unsigned char* out;
    uint64_t have_out;
    uint64_t done_out;
};

class Downloader {
   public:
    Downloader(uint8_t part_num);
    ~Downloader();
    bool init(const string& url, const string& region, uint64_t size, uint64_t chunksize,
              S3Credential* pcred);
    bool get(char* buf, uint64_t& len);
    void destroy();

   private:
    const uint8_t num;
    pthread_t* threads;
    BlockingBuffer** buffers;
    OffsetMgr* o;
    uint64_t chunkcount;
    uint64_t readlen;

    unsigned char magic_bytes[4];
    uint64_t magic_bytes_num;
    compression_type_t compression;
    bool set_compression();

    bool plain_get(char* buf, uint64_t& len);

    struct zstream_info* z_info;
    bool zstream_get(char* buf, uint64_t& len);
};

struct Bufinfo {
    /* data */
    char* buf;
    uint64_t maxsize;
    uint64_t len;
};

class HTTPFetcher : public BlockingBuffer {
   public:
    HTTPFetcher(const string& url, OffsetMgr* o);
    ~HTTPFetcher();
    bool SetMethod(Method m);
    bool AddHeaderField(HeaderField f, const string& v);

   protected:
    uint64_t fetchdata(uint64_t offset, char* data, uint64_t len);
    virtual void signHeader() {}
    CURL* curl;
    Method method;
    HTTPHeaders headers;
    UrlParser urlparser;
};

class S3Fetcher : public HTTPFetcher {
   public:
    S3Fetcher(const string& url, const string& region, OffsetMgr* o, const S3Credential& cred);
    ~S3Fetcher(){};

   protected:
    virtual void signHeader();

   private:
    string region;
    S3Credential cred;
};

struct BucketContent;

// To avoid double delete and core dump, always use new to create
// ListBucketResult object,
// unless we upgrade to c++ 11. Reason is we delete ListBucketResult in close()
// explicitly.
struct ListBucketResult {
    string Name;
    string Prefix;
    vector<BucketContent*> contents;

    ~ListBucketResult();
};

struct BucketContent {
    BucketContent() : name(""), size(0) {}
    BucketContent(string name, uint64_t size) {
        this->name = name;
        this->size = size;
    }
    ~BucketContent() {}
    string getName() const { return this->name; };
    uint64_t getSize() const { return this->size; };

    string name;
    uint64_t size;
};

#endif
