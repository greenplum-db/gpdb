#ifndef INCLUDE_READER_PARAMS_H_
#define INCLUDE_READER_PARAMS_H_

#include <string>

#include "s3common.h"

using std::string;

class ReaderParams {
   public:
    ReaderParams() : keySize(0), chunkSize(0), numOfChunks(0), segId(0), segNum(1) {
    }
    virtual ~ReaderParams() {
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

    const string& getKeyUrl() const {
        return keyUrl;
    }

    void setKeyUrl(const string& keyUrl) {
        this->keyUrl = keyUrl;
    }

    const string& getRegion() const {
        return region;
    }

    void setRegion(const string& region) {
        this->region = region;
    }

    uint64_t getKeySize() const {
        return keySize;
    }

    void setKeySize(uint64_t size) {
        this->keySize = size;
    }

    const string& getUrlToLoad() const {
        return urlToLoad;
    }

    void setUrlToLoad(const string& url) {
        this->urlToLoad = url;
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

   private:
    string urlToLoad;  // original url to read/write.
    string keyUrl;     // key url in s3 bucket.
    string region;
    uint64_t keySize;      // key/file size.
    uint64_t chunkSize;    // chunk size
    uint64_t numOfChunks;  // number of chunks(threads).
    S3Credential cred;
    uint64_t segId;
    uint64_t segNum;
};

#endif /* INCLUDE_READER_PARAMS_H_ */
