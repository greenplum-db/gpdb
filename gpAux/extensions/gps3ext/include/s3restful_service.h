#ifndef INCLUDE_S3RESTFUL_SERVICE_H_
#define INCLUDE_S3RESTFUL_SERVICE_H_

#include "restful_service.h"

class S3RESTfulService : public RESTfulService {
   public:
    S3RESTfulService();
    virtual ~S3RESTfulService();

    ResponseCode head(const string& url, HTTPHeaders& headers, const S3Params& params);

    Response get(const string& url, HTTPHeaders& headers, const S3Params& params);

    Response put(const string& url, HTTPHeaders& headers, const S3Params& params,
                 const vector<uint8_t>& data);

    Response post(const string& url, HTTPHeaders& headers, const S3Params& params,
                  const vector<uint8_t>& data);

    Response deleteRequest(const string& url, HTTPHeaders& headers, const S3Params& params);
};

#endif /* INCLUDE_S3RESTFUL_SERVICE_H_ */
