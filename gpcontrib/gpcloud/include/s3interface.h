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
#define S3_REQUEST_MAX_RETRIES 5

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

class S3Interface {
   public:
    virtual ~S3Interface() {
    }

    virtual ListBucketResult listBucket(S3Url &s3Url) = 0;

    virtual uint64_t fetchData(uint64_t offset, S3VectorUInt8 &data, uint64_t len,
                               const S3Url &s3Url) = 0;

    virtual S3CompressionType checkCompressionType(const S3Url &s3Url) = 0;

    virtual bool checkKeyExistence(const S3Url &s3Url) = 0;

    virtual string getUploadId(const S3Url &s3Url) = 0;

    virtual string uploadPartOfData(S3VectorUInt8 &data, const S3Url &s3Url, uint64_t partNumber,
                                    const string &uploadId) = 0;

    virtual bool completeMultiPart(const S3Url &s3Url, const string &uploadId,
                                   const vector<string> &etagArray) = 0;

    virtual bool abortUpload(const S3Url &s3Url, const string &uploadId) = 0;
};

class S3InterfaceService : public S3Interface {
   public:
    S3InterfaceService();
    S3InterfaceService(const S3Params &);
    virtual ~S3InterfaceService();

    ListBucketResult listBucket(S3Url &s3Url);

    uint64_t fetchData(uint64_t offset, S3VectorUInt8 &data, uint64_t len, const S3Url &s3Url);

    S3CompressionType checkCompressionType(const S3Url &s3Url);

    bool checkKeyExistence(const S3Url &s3Url);

    void setRESTfulService(RESTfulService *restfullService) {
        this->restfulService = restfullService;
    }

   protected:
    Response getResponseWithRetries(const string &url, HTTPHeaders &headers,
                                    uint64_t retries = S3_REQUEST_MAX_RETRIES);

    Response putResponseWithRetries(const string &url, HTTPHeaders &headers, S3VectorUInt8 &data,
                                    uint64_t retries = S3_REQUEST_MAX_RETRIES);

    Response postResponseWithRetries(const string &url, HTTPHeaders &headers,
                                     const vector<uint8_t> &data,
                                     uint64_t retries = S3_REQUEST_MAX_RETRIES);

    ResponseCode headResponseWithRetries(const string &url, HTTPHeaders &headers,
                                         uint64_t retries = S3_REQUEST_MAX_RETRIES);

    Response deleteRequestWithRetries(const string &url, HTTPHeaders &headers,
                                      uint64_t retries = S3_REQUEST_MAX_RETRIES);

    string getUploadId(const S3Url &s3Url);

    string uploadPartOfData(S3VectorUInt8 &data, const S3Url &s3Url, uint64_t partNumber,
                            const string &uploadId);

    bool completeMultiPart(const S3Url &s3Url, const string &uploadId,
                           const vector<string> &etagArray);

    bool abortUpload(const S3Url &s3Url, const string &uploadId);

   private:
    bool parseBucketXML(ListBucketResult *result, xmlParserCtxtPtr xmlcontext, string &marker);

    Response getBucketResponse(const S3Url &s3Url, const string &encodedQuery);

    xmlParserCtxtPtr getXMLContext(Response &response);

    bool isKeyExisted(ResponseCode code);

   private:
    RESTfulService *restfulService;
    S3Params params;
};

#endif /* INCLUDE_S3INTERFACE_H_ */
