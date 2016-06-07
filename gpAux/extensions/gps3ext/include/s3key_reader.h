#ifndef INCLUDE_S3KEY_READER_H_
#define INCLUDE_S3KEY_READER_H_

#include <string>
using std::string;

#include "gps3ext.h"
#include "reader.h"
#include "s3common.h"
#include "s3interface.h"
#include "s3macros.h"
#include "s3restful_service.h"
#include "s3url_parser.h"

#include <vector>

using std::vector;
using std::stringstream;

struct Range {
    uint64_t offset;
    uint64_t length;
};

class OffsetMgr {
   public:
    OffsetMgr() : keySize(0), chunkSize(0), curPos(0) {
        pthread_mutex_init(&this->offsetLock, NULL);
    }
    ~OffsetMgr() {
        pthread_mutex_destroy(&this->offsetLock);
    }

    Range getNextOffset();  // ret.length == 0 means EOF

    uint64_t getChunkSize() const {
        return chunkSize;
    }

    void setChunkSize(uint64_t chunkSize) {
        this->chunkSize = chunkSize;
    }

    uint64_t getKeySize() const {
        return keySize;
    }

    void setKeySize(uint64_t keySize) {
        this->keySize = keySize;
    }

   private:
    pthread_mutex_t offsetLock;
    uint64_t keySize;
    uint64_t chunkSize;
    uint64_t curPos;
};

enum ChunkStatus {
    ReadyToRead,
    ReadyToFill,
};

class ChunkBuffer {
   public:
    ChunkBuffer(const string& url, OffsetMgr& mgr, bool& sharedError, const S3Credential& cred,
                const string& region);

    /*ChunkBuffer(const ChunkBuffer& other)
        : sourceUrl(other.sourceUrl),
          sharedError(other.sharedError),
          offsetMgr(other.offsetMgr),
          credential(other.credential),
          region(other.region),
          s3interface(other.s3interface) {
        this->chunkData = other.chunkData;

        curFileOffset = other.curFileOffset;
        chunkDataSize = other.chunkDataSize;
        status = other.status;
        eof = other.eof;
        curChunkOffset = other.curChunkOffset;
    }*/

    ~ChunkBuffer();

    bool isEOF() {
        return this->eof;
    }

    // Error is shared among all ChunkBuffers of a KeyReader.
    bool isError() {
        return this->sharedError;
    }
    void setError() {
        this->sharedError = true;
    }

    uint64_t read(char* buf, uint64_t len);
    uint64_t fill();

    void setS3interface(S3Interface* s3) {
        this->s3interface = s3;
    }

    void init();
    void destroy();

   protected:
    string sourceUrl;

   private:
    bool eof;
    bool& sharedError;

    ChunkStatus status;

    pthread_mutex_t stat_mutex;
    pthread_cond_t stat_cond;

    uint64_t curFileOffset;
    uint64_t curChunkOffset;
    uint64_t chunkDataSize;
    char* chunkData;
    OffsetMgr& offsetMgr;
    const S3Credential& credential;
    const string& region;

    S3Interface* s3interface;
};

class S3KeyReader : public Reader {
   public:
    S3KeyReader()
        : sharedError(false),
          numOfChunks(0),
          curReadingChunk(0),
          transferredKeyLen(0),
          s3interface(NULL) {
    }
    virtual ~S3KeyReader() {
    }

    void open(const ReaderParams& params);
    uint64_t read(char* buf, uint64_t count);
    void close();

    void setS3interface(S3Interface* s3) {
        this->s3interface = s3;
    }

   private:
    bool sharedError;
    uint64_t numOfChunks;
    uint64_t curReadingChunk;
    uint64_t transferredKeyLen;
    string region;
    OffsetMgr offsetMgr;

    vector<ChunkBuffer> chunkBuffers;
    vector<pthread_t> threads;

    S3Interface* s3interface;
};

#endif /* INCLUDE_S3KEYREADER_H_ */