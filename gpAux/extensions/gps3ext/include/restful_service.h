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
    RESPONSE_OK,     // everything is OK
    RESPONSE_FAIL,   // curl failed (i.e., the status is not CURLE_OK)
    RESPONSE_ERROR,  // server error (server return code is not 200)
};

typedef long ResponseCode;
#define HeadResponseFail -1

// 2XX are successful response.
// Here we deal with 200 (OK) and 206 (partial content) currently.
//
//   We may move this function to RESTfulService() in future
inline bool isSuccessfulResponse(ResponseCode code) {
    return (code == 200 || code == 206);
}

struct UploadData {
    UploadData(const vector<uint8_t>& buff) : buffer(buff), currentPosition(0) {
    }

    const vector<uint8_t>& buffer;
    uint64_t currentPosition;
};

class Response {
   public:
    Response() : status(RESPONSE_FAIL) {
    }
    Response(ResponseStatus status, const vector<uint8_t>& dataBuffer)
        : status(status), dataBuffer(dataBuffer) {
    }
    Response(ResponseStatus status, const vector<uint8_t>& headersBuffer,
             const vector<uint8_t>& dataBuffer)
        : status(status), headersBuffer(headersBuffer), dataBuffer(dataBuffer) {
    }

    bool isSuccess() {
        return status == RESPONSE_OK;
    }

    vector<uint8_t>& getRawHeaders() {
        return headersBuffer;
    }

    vector<uint8_t>&& moveHeadersBuffer() {
        return std::move(headersBuffer);
    }

    void appendHeadersBuffer(char* ptr, size_t size) {
        // Fix Eclipse warning
        headersBuffer.insert(headersBuffer.end(), ptr, ptr + size);
    }

    void clearHeadersBuffer() {
        headersBuffer = vector<uint8_t>();
    }

    vector<uint8_t>& getRawData() {
        return dataBuffer;
    }

    vector<uint8_t>&& moveDataBuffer() {
        return std::move(dataBuffer);
    }

    void appendDataBuffer(char* ptr, size_t size) {
        // Fix Eclipse warning
        dataBuffer.insert(dataBuffer.end(), ptr, ptr + size);
    }

    void clearBuffers() {
        // don't use vector.clear() because it may not be able to release memory.
        dataBuffer = vector<uint8_t>();
        headersBuffer = vector<uint8_t>();
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

   private:
    // status is OK when get full HTTP response even response body may means request failure.
    ResponseStatus status;
    string message;
    vector<uint8_t> headersBuffer;
    vector<uint8_t> dataBuffer;
};

class RESTfulService {
   public:
    RESTfulService() {
    }
    virtual ~RESTfulService() {
    }

    virtual Response get(const string& url, HTTPHeaders& headers,
                         const map<string, string>& params) = 0;

    virtual Response put(const string& url, HTTPHeaders& headers, const map<string, string>& params,
                         const vector<uint8_t>& data) = 0;

    virtual Response post(const string& url, HTTPHeaders& headers,
                          const map<string, string>& params, const vector<uint8_t>& data) = 0;

    virtual ResponseCode head(const string& url, HTTPHeaders& headers,
                              const map<string, string>& params) = 0;
};

#endif /* INCLUDE_RESTFUL_SERVICE_H_ */
