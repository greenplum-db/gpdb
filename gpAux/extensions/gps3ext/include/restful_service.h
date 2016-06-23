#ifndef INCLUDE_RESTFUL_SERVICE_H_
#define INCLUDE_RESTFUL_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include <stdint.h>

#include "s3http_headers.h"

using std::string;
using std::vector;
using std::map;

enum ResponseStatus {
    RESPONSE_OK,     // everything is ok
    RESPONSE_FAIL,   // curl failed (i.e., the status is not CURLE_OK)
    RESPONSE_ERROR,  // server error (server return code is not 200)
};

class Response {
   public:
    Response() : status(RESPONSE_FAIL) {
    }
    Response(ResponseStatus status, const vector<uint8_t>& buf) : status(status), buffer(buf) {
    }

    bool isSuccess() {
        return status == RESPONSE_OK;
    }

    vector<uint8_t>& getRawData() {
        return buffer;
    }

    const string& getMessage() const {
        return message;
    }

    void setMessage(const string& message) {
        this->message = message;
    }

    const ResponseStatus& getStatus() const {
        return status;
    }

    void setStatus(const ResponseStatus& status) {
        this->status = status;
    }

    void appendBuffer(char* ptr, size_t size) {
        // Fix Eclipse warning
        buffer.insert(buffer.end(), ptr, ptr + size);
    }

    void clearBuffer() {
        buffer.clear();
        buffer.shrink_to_fit();
    }

   private:
    // status is OK when get full HTTP response even response body may means request failure.
    ResponseStatus status;
    string message;
    vector<uint8_t> buffer;
};

class RESTfulService {
   public:
    RESTfulService() {
    }
    virtual ~RESTfulService() {
    }

    virtual Response get(const string& url, HTTPHeaders& headers,
                         const map<string, string>& params) = 0;
};

#endif /* INCLUDE_RESTFUL_SERVICE_H_ */
