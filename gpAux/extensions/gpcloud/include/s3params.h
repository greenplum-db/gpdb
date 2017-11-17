#ifndef __S3_PARAMS_H__
#define __S3_PARAMS_H__

#include "s3common_headers.h"
#include "s3memory_mgmt.h"
#include "s3url.h"

enum S3SSEType { SSE_NONE, SSE_S3 };

class S3Params {
   public:
    S3Params(const string& sourceUrl = "", bool useHttps = true, const string& version = "",
             const string& region = "")
        : s3Url(sourceUrl, useHttps, version, region),
          keySize(0),
          chunkSize(0),
          numOfChunks(0),
          lowSpeedLimit(0),
          lowSpeedTime(0),
          proxy(""),
          debugCurl(false),
          autoCompress(false),
          verifyCert(false),
          sseType(SSE_NONE),
          gpcheckcloud_newline("") {
    }

    virtual ~S3Params() {
    }

    S3Params setPrefix(const string& key) {
        S3Params params(*this);
        params.getS3Url().setPrefix(key);
        return params;
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

    bool isDebugCurl() const {
        return debugCurl;
    }

    void setDebugCurl(bool debugCurl) {
        this->debugCurl = debugCurl;
    }

    bool isAutoCompress() const {
        return autoCompress;
    }

    bool isVerifyCert() const {
        return verifyCert;
    }
    void setVerifyCert(bool verifyCert) {
        this->verifyCert = verifyCert;
    }

    void setAutoCompress(bool autoCompress) {
        this->autoCompress = autoCompress;
    }

    const S3MemoryContext& getMemoryContext() const {
        return memoryContext;
    }

    S3Url& getS3Url() {
        return s3Url;
    }

    const S3Url& getS3Url() const {
        return s3Url;
    }

    const S3SSEType& getSSEType() const {
        return sseType;
    }

    void setSSEType(S3SSEType sseType) {
        this->sseType = sseType;
    }

    const string& getProxy() const {
        return proxy;
    }

    void setProxy(string proxy) {
        this->proxy = proxy;
    }

    const string& getGpcheckcloud_newline() const {
        return gpcheckcloud_newline;
    }

    void setGpcheckcloud_newline(string gpcheckcloud_newline) {
        this->gpcheckcloud_newline = gpcheckcloud_newline;
    }

   private:
    S3Url s3Url;  // original url to read/write.

    uint64_t keySize;  // key/file size.

    S3Credential cred;  // S3 credential.

    uint64_t chunkSize;    // chunk size
    uint64_t numOfChunks;  // number of chunks(threads).

    uint64_t lowSpeedLimit;  // low speed limit
    uint64_t lowSpeedTime;   // low speed timeout

    string proxy;  // proxy

    bool debugCurl;     // debug curl or not
    bool autoCompress;  // whether to compress data before uploading
    bool verifyCert;  // This option determines whether curl verifies the authenticity of the peer's
                      // certificate.

    S3SSEType sseType;

    S3MemoryContext memoryContext;

    string gpcheckcloud_newline;  // newline LF, CRLF, CR
};

inline void PrepareS3MemContext(const S3Params& params) {
    S3MemoryContext& memoryContext = const_cast<S3MemoryContext&>(params.getMemoryContext());

    // We need one more chunk of memory for writer to prepare data to upload.
    memoryContext.prepare(params.getChunkSize(), params.getNumOfChunks() + 1);
}

#endif
