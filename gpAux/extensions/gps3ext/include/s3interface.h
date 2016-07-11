#ifndef INCLUDE_S3INTERFACE_H_
#define INCLUDE_S3INTERFACE_H_

// need to include stdexcept to use std::runtime_error
#include <stdexcept>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "restful_service.h"

#define S3_MAGIC_BYTES_NUM 4

#define S3_GET_RESPONSE_NO_RETRY 1
#define S3_GET_RESPONSE_MAX_RETRIES 3

enum S3CompressionType {
    S3_COMPRESSION_GZIP,
    S3_COMPRESSION_PLAIN,
};

struct BucketContent {
    BucketContent() : name(""), size(0) {
    }
    BucketContent(string name, uint64_t size) {
        this->name = name;
        this->size = size;
    }
    ~BucketContent() {
    }
    string getName() const {
        return this->name;
    };
    uint64_t getSize() const {
        return this->size;
    };

    string name;
    uint64_t size;
};

// To avoid double delete and core dump, always use new to create
// ListBucketResult object,
// unless we upgrade to c++11. Reason is we delete ListBucketResult in close()
// explicitly.
struct ListBucketResult {
    string Name;
    string Prefix;
    vector<BucketContent*> contents;

    ~ListBucketResult();
};

class S3Interface {
   public:
    virtual ~S3Interface() {
    }

    // It is caller's responsibility to free returned memory.
    virtual ListBucketResult* listBucket(const string& schema, const string& region,
                                         const string& bucket, const string& prefix,
                                         const S3Credential& cred) {
        throw std::runtime_error("Default implementation must not be called.");
    }

    virtual uint64_t fetchData(uint64_t offset, vector<uint8_t>& data, uint64_t len,
                               const string& sourceUrl, const string& region,
                               const S3Credential& cred) {
        throw std::runtime_error("Default implementation must not be called.");
    }

    virtual S3CompressionType checkCompressionType(const string& keyUrl, const string& region,
                                                   const S3Credential& cred) {
        throw std::runtime_error("Default implementation must not be called.");
    }
};

class S3Service : public S3Interface {
   public:
    S3Service();
    virtual ~S3Service();
    ListBucketResult* listBucket(const string& schema, const string& region, const string& bucket,
                                 const string& prefix, const S3Credential& cred);

    uint64_t fetchData(uint64_t offset, vector<uint8_t>& data, uint64_t len,
                       const string& sourceUrl, const string& region, const S3Credential& cred);

    S3CompressionType checkCompressionType(const string& keyUrl, const string& region,
                                           const S3Credential& cred);

    // following two functions are exposed publicly for UT tests
    void setRESTfulService(RESTfulService* restfullService) {
        this->restfulService = restfullService;
    }

    Response getResponseWithRetries(const string& url, HTTPHeaders& headers,
                                    const map<string, string>& params,
                                    uint64_t retries = S3_GET_RESPONSE_MAX_RETRIES);

   private:
    string getUrl(const string& prefix, const string& schema, const string& host,
                  const string& bucket, const string& marker);

    bool parseBucketXML(ListBucketResult* result, xmlParserCtxtPtr xmlcontext, string& marker);

    Response getBucketResponse(const string& region, const string& url, const string& prefix,
                               const S3Credential& cred, const string& marker);

    void parseXMLMessage(xmlParserCtxtPtr xmlcontext);

    HTTPHeaders composeHTTPHeaders(const string& url, const string& marker, const string& prefix,
                                   const string& region, const S3Credential& cred);

    xmlParserCtxtPtr getXMLContext(Response& response);

    RESTfulService* restfulService;
};

#endif /* INCLUDE_S3INTERFACE_H_ */
