#ifndef INCLUDE_S3RESTFUL_SERVICE_H_
#define INCLUDE_S3RESTFUL_SERVICE_H_

#include "restful_service.h"
#include "s3common_headers.h"

class S3RESTfulService : public RESTfulService {
   public:
    S3RESTfulService(const S3Params& params);
    virtual ~S3RESTfulService();

    ResponseCode head(const string& url, HTTPHeaders& headers);

    Response get(const string& url, HTTPHeaders& headers);

    Response put(const string& url, HTTPHeaders& headers, const vector<uint8_t>& data);

    Response post(const string& url, HTTPHeaders& headers, const vector<uint8_t>& data);

    Response deleteRequest(const string& url, HTTPHeaders& headers);

   private:
    uint64_t lowSpeedLimit;
    uint64_t lowSpeedTime;
    bool debugCurl;
};

#endif /* INCLUDE_S3RESTFUL_SERVICE_H_ */
