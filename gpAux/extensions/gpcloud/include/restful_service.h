#ifndef INCLUDE_RESTFUL_SERVICE_H_
#define INCLUDE_RESTFUL_SERVICE_H_

#include "s3memory_mgmt.h"

class HTTPHeaders;

enum ResponseStatus {
    RESPONSE_OK,    // everything is OK
    RESPONSE_ERROR  // server error (server return code is not 2XX)
};

typedef long ResponseCode;

// 2XX are successful response.
// Here we deal with 200 (OK), 204 (no content) and 206 (partial content) currently,
// 204 is for DELETE request.
//   We may move this function to RESTfulService() in future
inline bool isSuccessfulResponse(ResponseCode code) {
    return (code == 200 || code == 206 || code == 204);
}

struct UploadData {
    UploadData(const S3VectorUInt8& buff) : buffer(buff), currentPosition(0) {
    }

    const S3VectorUInt8& buffer;
    uint64_t currentPosition;
};

class Response {
   public:
    explicit Response(ResponseStatus status) : responseCode(-1), status(status) {
    }
    explicit Response(ResponseStatus status, S3MemoryContext& context)
        : responseCode(-1), status(status), dataBuffer(context) {
    }

    explicit Response(ResponseStatus status, const vector<uint8_t>& dataBuffer)
        : responseCode(-1), status(status), dataBuffer(dataBuffer) {
    }
    explicit Response(ResponseStatus status, const vector<uint8_t>& headersBuffer,
                      const S3VectorUInt8& dataBuffer)
        : responseCode(-1), status(status), headersBuffer(headersBuffer), dataBuffer(dataBuffer) {
    }

    void FillResponse(ResponseCode responseCode) {
        this->setResponseCode(responseCode);

        if (isSuccessfulResponse(responseCode)) {
            this->setStatus(RESPONSE_OK);
            this->setMessage("Success");
        } else {  // Server error, set status to RESPONSE_ERROR
            stringstream sstr;
            sstr << "Server returned error, error code is " << responseCode;

            this->setStatus(RESPONSE_ERROR);
            this->setMessage(sstr.str());
        }
    }

    bool isSuccess() {
        return status == RESPONSE_OK;
    }

    vector<uint8_t>& getRawHeaders() {
        return headersBuffer;
    }

    void appendHeadersBuffer(char* ptr, size_t size) {
        // Fix Eclipse warning
        headersBuffer.insert(headersBuffer.end(), ptr, ptr + size);
    }

    S3VectorUInt8& getRawData() {
        return dataBuffer;
    }

    const S3VectorUInt8& getRawData() const {
        return dataBuffer;
    }

    void appendDataBuffer(char* ptr, size_t size) {
        // Fix Eclipse warning
        dataBuffer.insert(dataBuffer.end(), ptr, ptr + size);
    }

    void clearBuffers() {
        dataBuffer.release();

        vector<uint8_t> emptyHeaders;
        headersBuffer.swap(emptyHeaders);
    }

    ResponseCode getResponseCode() const {
        return responseCode;
    }

    void setResponseCode(ResponseCode responseCode) {
        this->responseCode = responseCode;
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
    ResponseCode responseCode;

    // status is OK when get full HTTP response even response body may means request failure.
    ResponseStatus status;
    string message;

    vector<uint8_t> headersBuffer;
    S3VectorUInt8 dataBuffer;
};

class RESTfulService {
   public:
    RESTfulService() {
    }
    virtual ~RESTfulService() {
    }

    virtual Response get(const string& url, HTTPHeaders& headers) = 0;

    virtual Response put(const string& url, HTTPHeaders& headers, const S3VectorUInt8& data) = 0;

    virtual Response post(const string& url, HTTPHeaders& headers, const vector<uint8_t>& data) = 0;

    virtual ResponseCode head(const string& url, HTTPHeaders& headers) = 0;

    virtual Response deleteRequest(const string& url, HTTPHeaders& headers) = 0;
};

#endif /* INCLUDE_RESTFUL_SERVICE_H_ */
