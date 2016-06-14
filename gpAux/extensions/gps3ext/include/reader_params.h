#ifndef INCLUDE_READER_PARAMS_H_
#define INCLUDE_READER_PARAMS_H_

#include <string>

#include "s3common.h"

using std::string;

class ReaderParams {
   public:
    ReaderParams(){};
    virtual ~ReaderParams(){};

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

    uint64_t getSize() const {
        return size;
    }

    void setSize(uint64_t size) {
        this->size = size;
    }

    const string& getUrl() const {
        return url;
    }

    void setUrl(const string& url) {
        this->url = url;
    }

    int getSegId() const {
        return segId;
    }

    void setSegId(int segId) {
        this->segId = segId;
    }

    int getSegNum() const {
        return segNum;
    }

    void setSegNum(int segNum) {
        this->segNum = segNum;
    }

   private:
    string url;     // original url to read/write.
    string keyUrl;  // key url in s3 bucket.
    string region;
    uint64_t size;       // key/file size.
    uint64_t chunkSize;  // chunk size
    S3Credential cred;
    int segId;
    int segNum;
};

#endif /* INCLUDE_READER_PARAMS_H_ */
