#ifndef INCLUDE_S3RESTUL_SERVICE_H_
#define INCLUDE_S3RESTUL_SERVICE_H_

#include "restful_service.h"

class S3RESTfulService : public RESTfulService {
   public:
    S3RESTfulService();
    virtual ~S3RESTfulService();
    Response get(const string& url, HTTPHeaders& headers, const map<string, string>& params);
};

#endif /* INCLUDE_S3RESTUL_SERVICE_H_ */
