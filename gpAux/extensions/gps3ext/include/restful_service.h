#ifndef INCLUDE_RESTFUL_SERVICE_H_
#define INCLUDE_RESTFUL_SERVICE_H_

#include <stdint.h>
#include <map>
#include <string>
#include <vector>
#include "s3http_headers.h"
using std::string;
using std::vector;
using std::map;

enum ResponseStatus {
    OK,
    FAIL,
};

class Response {
   public:
    Response() : status(FAIL) {
    }
    Response(ResponseStatus status, const vector<uint8_t>& buf) : status(status), buffer(buf) {
    }

    bool isSuccess() {
        return status == OK;
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
