#ifndef __S3_PARAMS_H__
#define __S3_PARAMS_H__

#include <string>

#include "s3common.h"

using std::string;

class S3Params {
   public:
    S3Params() : keySize(0), chunkSize(0), numOfChunks(0), segId(0), segNum(1) {
    }
    virtual ~S3Params() {
    }

    uint64_t getChunkSize() const {
        return chunkSize;
    }

    void setChunkSize(uint64_t chunkSize) {
        this->chunkSize = chunkSize;
    }

    const S3Credential& getCred() const {
        return cred;
    }

    void setCred(const S3Credential& cred) {
        this->cred = cred;
    }

    const string& getRegion() const {
        return region;
    }

    void setRegion(const string& region) {
        this->region = region;
    }

    const string& getKeyUrl() const {
        return keyUrl;
    }

    void setKeyUrl(const string& url) {
        this->keyUrl = url;
    }

    uint64_t getSegId() const {
        return segId;
    }

    void setSegId(uint64_t segId) {
        this->segId = segId;
    }

    uint64_t getSegNum() const {
        return segNum;
    }

    void setSegNum(uint64_t segNum) {
        this->segNum = segNum;
    }

    uint64_t getNumOfChunks() const {
        return numOfChunks;
    }

    void setNumOfChunks(uint64_t numOfChunks) {
        this->numOfChunks = numOfChunks;
    }

    uint64_t getKeySize() const {
        return keySize;
    }

    void setKeySize(uint64_t size) {
        this->keySize = size;
    }

    const string& getBaseUrl() const {
        return baseUrl;
    }

    void setBaseUrl(const string& url) {
        this->baseUrl = url;
    }

   private:
    string baseUrl;  // original url to read/write.

    string keyUrl;
    uint64_t keySize;  // key/file size.

    string region;
    uint64_t chunkSize;    // chunk size
    uint64_t numOfChunks;  // number of chunks(threads).
    S3Credential cred;
    uint64_t segId;
    uint64_t segNum;
};

class WriterParams : public S3Params {};

class ReaderParams : public S3Params {};

#endif