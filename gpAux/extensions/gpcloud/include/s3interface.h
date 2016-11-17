#ifndef INCLUDE_S3INTERFACE_H_
#define INCLUDE_S3INTERFACE_H_

#include "gpcommon.h"
#include "s3common_headers.h"
#include "s3exception.h"
#include "s3log.h"
#include "s3restful_service.h"
#include "s3url.h"

#define S3_MAGIC_BYTES_NUM 4

#define S3_REQUEST_NO_RETRY 1
#define S3_REQUEST_MAX_RETRIES 3

#define S3_RANGE_HEADER_STRING_LEN 128

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

struct ListBucketResult {
    string Name;
    string Prefix;
    vector<BucketContent> contents;
};

class S3MessageParser {
   public:
    S3MessageParser(const Response& resp);

    ~S3MessageParser();

    const string& getMessage() const {
        return message;
    }
    const string& getCode() const {
        return code;
    }
    string parseS3Tag(const string& tag);

   private:
    xmlParserCtxtPtr xmlptr;
    string message;
    string code;
};

class S3Interface {
   public:
    virtual ~S3Interface() {
    }

    virtual ListBucketResult listBucket(const string& schema, const string& region,
                                        const string& bucket, const string& prefix) = 0;

    virtual uint64_t fetchData(uint64_t offset, S3VectorUInt8& data, uint64_t len,
                               const string& sourceUrl, const string& region) = 0;

    virtual S3CompressionType checkCompressionType(const string& keyUrl, const string& region) = 0;

    virtual bool checkKeyExistence(const string& keyUrl, const string& region) = 0;

    virtual string getUploadId(const string& keyUrl, const string& region) = 0;

    virtual string uploadPartOfData(S3VectorUInt8& data, const string& keyUrl, const string& region,
                                    uint64_t partNumber, const string& uploadId) = 0;

    virtual bool completeMultiPart(const string& keyUrl, const string& region,
                                   const string& uploadId, const vector<string>& etagArray) = 0;

    virtual bool abortUpload(const string& keyUrl, const string& region,
                             const string& uploadId) = 0;
};

class S3InterfaceService : public S3Interface {
   public:
    S3InterfaceService();
    S3InterfaceService(const S3Params& params);
    virtual ~S3InterfaceService();

    ListBucketResult listBucket(const string& schema, const string& region, const string& bucket,
                                const string& prefix);

    uint64_t fetchData(uint64_t offset, S3VectorUInt8& data, uint64_t len, const string& sourceUrl,
                       const string& region);

    S3CompressionType checkCompressionType(const string& keyUrl, const string& region);

    bool checkKeyExistence(const string& keyUrl, const string& region);

    void setRESTfulService(RESTfulService* restfullService) {
        this->restfulService = restfullService;
    }

   protected:
    Response getResponseWithRetries(const string& url, HTTPHeaders& headers,
                                    uint64_t retries = S3_REQUEST_MAX_RETRIES);

    Response putResponseWithRetries(const string& url, HTTPHeaders& headers, S3VectorUInt8& data,
                                    uint64_t retries = S3_REQUEST_MAX_RETRIES);

    Response postResponseWithRetries(const string& url, HTTPHeaders& headers,
                                     const vector<uint8_t>& data,
                                     uint64_t retries = S3_REQUEST_MAX_RETRIES);

    ResponseCode headResponseWithRetries(const string& url, HTTPHeaders& headers,
                                         uint64_t retries = S3_REQUEST_MAX_RETRIES);

    Response deleteRequestWithRetries(const string& url, HTTPHeaders& headers,
                                      uint64_t retries = S3_REQUEST_MAX_RETRIES);

    string getUploadId(const string& keyUrl, const string& region);

    string uploadPartOfData(S3VectorUInt8& data, const string& keyUrl, const string& region,
                            uint64_t partNumber, const string& uploadId);

    bool completeMultiPart(const string& keyUrl, const string& region, const string& uploadId,
                           const vector<string>& etagArray);

    bool abortUpload(const string& keyUrl, const string& region, const string& uploadId);

   private:
    string getUrl(const string& prefix, const string& schema, const string& host,
                  const string& bucket, const string& marker);

    bool parseBucketXML(ListBucketResult* result, xmlParserCtxtPtr xmlcontext, string& marker);

    Response getBucketResponse(const string& region, const string& url, const string& prefix,
                               const string& marker);

    HTTPHeaders composeHTTPHeaders(const string& url, const string& marker, const string& prefix,
                                   const string& region);

    xmlParserCtxtPtr getXMLContext(Response& response);

    bool isKeyExisted(ResponseCode code);

    bool isHeadResponseCodeNeedRetry(ResponseCode code);

   private:
    RESTfulService* restfulService;
    S3Params params;
};

#endif /* INCLUDE_S3INTERFACE_H_ */
