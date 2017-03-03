#ifndef INCLUDE_S3KEY_READER_H_
#define INCLUDE_S3KEY_READER_H_

#include "reader.h"
#include "s3common_headers.h"
#include "s3exception.h"
#include "s3interface.h"

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

    void setCurPos(uint64_t curPos) {
        this->curPos = curPos;
    }

    void reset() {
        this->setCurPos(0);
        this->setChunkSize(0);
        this->setKeySize(0);
    }

    uint64_t getCurPos() const {
        return curPos;
    }

   private:
    pthread_mutex_t offsetLock;
    uint64_t keySize;  // size of S3 key(file)
    uint64_t chunkSize;
    uint64_t curPos;
};

enum ChunkStatus {
    ReadyToRead,
    ReadyToFill,
};

class ChunkBuffer;

class S3KeyReader : public Reader {
   public:
    S3KeyReader()
        : sharedError(false),
          numOfChunks(0),
          curReadingChunk(0),
          transferredKeyLen(0),
          s3Interface(NULL),
          hasEol(false),
          eolAppended(false) {
        pthread_mutex_init(&this->mutexErrorMessage, NULL);
    }
    virtual ~S3KeyReader() {
        this->close();
        pthread_mutex_destroy(&this->mutexErrorMessage);
    }

    void open(const S3Params& params);
    uint64_t read(char* buf, uint64_t count);
    void close();

    void setS3InterfaceService(S3Interface* s3) {
        this->s3Interface = s3;
    }

    const vector<ChunkBuffer>& getChunkBuffers() const {
        return chunkBuffers;
    }

    uint64_t getCurReadingChunk() const {
        return curReadingChunk;
    }

    bool isSharedError() const {
        return sharedError;
    }

    void setSharedError(bool sharedError) {
        pthread_mutex_lock(&this->mutexErrorMessage);
        if (this->sharedException == NULL) {
            this->sharedException = std::current_exception();
        }
        this->sharedError = sharedError;
        pthread_mutex_unlock(&this->mutexErrorMessage);
    }

    template <typename E>
    void setSharedError(bool sharedError, const E& e) {
        pthread_mutex_lock(&this->mutexErrorMessage);
        try {
            throw e;
        } catch (...) {
            this->sharedException = std::current_exception();
        }
        this->sharedError = sharedError;
        pthread_mutex_unlock(&this->mutexErrorMessage);
    }

    const vector<pthread_t>& getThreads() const {
        return threads;
    }

    uint64_t getTransferredKeyLen() const {
        return transferredKeyLen;
    }

    OffsetMgr& getOffsetMgr() {
        return offsetMgr;
    }

    const string& getRegion() const {
        return region;
    }

   private:
    pthread_mutex_t mutexErrorMessage;

    bool sharedError;

    // exception_ptr is used to store exception object
    // and share across threads.
    std::exception_ptr sharedException;

    uint64_t numOfChunks;
    uint64_t curReadingChunk;
    uint64_t transferredKeyLen;
    string region;
    OffsetMgr offsetMgr;

    vector<ChunkBuffer> chunkBuffers;
    vector<pthread_t> threads;

    S3Interface* s3Interface;

    void reset();

    bool hasEol;
    bool eolAppended;
};

class ChunkBuffer {
   public:
    ChunkBuffer(const S3Url& s3Url, S3KeyReader& reader, const S3MemoryContext& context);

    ~ChunkBuffer();

    // if a class has reference member, then it can't be
    // copy assigned by default, we need to implement operator= explicitly.
    // it's needed for vector.
    ChunkBuffer& operator=(const ChunkBuffer& other);

    bool isEOF() {
        return this->eof;
    }

    // Error is shared among all ChunkBuffers of a KeyReader.
    bool isError() {
        return this->sharedKeyReader.isSharedError();
    }

    uint64_t read(char* buf, uint64_t len);
    uint64_t fill();

    void setS3InterfaceService(S3Interface* s3) {
        this->s3Interface = s3;
    }

    pthread_mutex_t* getStatMutex() {
        return &statusMutex;
    }

    pthread_cond_t* getStatCond() {
        return &statusCondVar;
    }

    void setStatus(ChunkStatus status) {
        this->status = status;
    }

    ChunkStatus getStatus() const {
        return status;
    }

    void setSharedError(bool sharedError) {
        this->sharedKeyReader.setSharedError(sharedError);
    }

    template <typename E>
    void setSharedError(bool sharedError, const E& e) {
        this->sharedKeyReader.setSharedError(sharedError, e);
    }

   protected:
    S3Url s3Url;

   private:
    bool eof;

    ChunkStatus status;

    pthread_mutex_t statusMutex;
    pthread_cond_t statusCondVar;

    uint64_t curFileOffset;
    uint64_t curChunkOffset;
    uint64_t chunkDataSize;

    S3VectorUInt8 chunkData;
    OffsetMgr& offsetMgr;
    S3Interface* s3Interface;
    S3KeyReader& sharedKeyReader;
};

#endif /* INCLUDE_S3KEYREADER_H_ */
