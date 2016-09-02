#ifndef __S3_PARAMS_H__
#define __S3_PARAMS_H__

#include <string>

#include "s3common.h"
#include "s3common_headers.h"

struct S3Credential {
    bool operator==(const S3Credential& other) const {
        return this->accessID == other.accessID && this->secret == other.secret &&
               this->token == other.token;
    }

    string accessID;
    string secret;
    string token;
};

class S3Params {
   public:
    S3Params() : keySize(0), chunkSize(0), numOfChunks(0) {
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

    void setCred(const string accessId, const string secret, const string token) {
        this->cred.accessID = accessId;
        this->cred.secret = secret;
        this->cred.token = token;
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

    uint64_t getLowSpeedLimit() const {
        return lowSpeedLimit;
    }

    void setLowSpeedLimit(uint64_t lowSpeedLimit) {
        this->lowSpeedLimit = lowSpeedLimit;
    }

    uint64_t getLowSpeedTime() const {
        return lowSpeedTime;
    }

    void setLowSpeedTime(uint64_t lowSpeedTime) {
        this->lowSpeedTime = lowSpeedTime;
    }

    bool isEncryption() const {
        return encryption;
    }

    void setEncryption(bool encryption) {
        this->encryption = encryption;
    }

    bool isDebugCurl() const {
        return debugCurl;
    }

    void setDebugCurl(bool debugCurl) {
        this->debugCurl = debugCurl;
    }

    bool isAutoCompress() const {
        return autoCompress;
    }

    void setAutoCompress(bool autoCompress) {
        this->autoCompress = autoCompress;
    }

   private:
    string baseUrl;  // original url to read/write.

    string keyUrl;
    uint64_t keySize;  // key/file size.

    S3Credential cred;  // S3 credential.

    string region;
    uint64_t chunkSize;    // chunk size
    uint64_t numOfChunks;  // number of chunks(threads).

    uint64_t lowSpeedLimit;  // low speed limit
    uint64_t lowSpeedTime;   // low speed timeout

    bool encryption;    // HTTP or HTTPS
    bool debugCurl;     // debug curl or not
    bool autoCompress;  // whether to compress data before uploading
};

#endif
