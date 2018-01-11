#ifndef INCLUDE_S3RESTFUL_SERVICE_H_
#define INCLUDE_S3RESTFUL_SERVICE_H_

#include "gpcommon.h"
#include "restful_service.h"
#include "s3common_headers.h"
#include "s3exception.h"
#include "s3http_headers.h"
#include "s3log.h"
#include "s3macros.h"
#include "s3params.h"

class S3RESTfulService : public RESTfulService {
   public:
    S3RESTfulService();
    S3RESTfulService(const string& proxy);
    S3RESTfulService(const S3Params& params);
    virtual ~S3RESTfulService();

    ResponseCode head(const string& url, HTTPHeaders& headers);

    Response get(const string& url, HTTPHeaders& headers);

    Response put(const string& url, HTTPHeaders& headers, const S3VectorUInt8& data);

    Response post(const string& url, HTTPHeaders& headers, const vector<uint8_t>& data);

    Response deleteRequest(const string& url, HTTPHeaders& headers);

   private:
    uint64_t lowSpeedLimit;
    uint64_t lowSpeedTime;

    string proxy;

    bool debugCurl;
    bool verifyCert;

    uint64_t chunkBufferSize;
    S3MemoryContext s3MemContext;

    void performCurl(CURL* curl, Response& response);
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

#endif /* INCLUDE_S3RESTFUL_SERVICE_H_ */
