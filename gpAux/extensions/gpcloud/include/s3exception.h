#ifndef __S3_EXCEPTION_H__
#define __S3_EXCEPTION_H__

#include "s3common_headers.h"

class S3Exception {
   public:
    string file;
    uint64_t line;
    string func;
    S3Exception() : line(0) {
    }

    virtual ~S3Exception() {
    }
    virtual string getMessage() {
        return "";
    }
    virtual string getFullMessage() {
        std::stringstream errorMessage;
        errorMessage << this->getMessage() << ", Function: " << this->func
                     << ", File: " << this->file << "(" << this->line << "). ";
        return errorMessage.str();
    }
    virtual string getType() {
        return "S3Exception";
    }
};

// HTTP request failed
// InternalError, RequestTimeout or ServiceUnavailable
class S3ConnectionError : public S3Exception {
   public:
    S3ConnectionError(const string& msg) : message(msg) {
    }
    virtual ~S3ConnectionError() {
    }
    virtual string getMessage() {
        return "Server connection failed: " + message;
    }
    virtual string getType() {
        return "S3ConnectionError";
    }

    string message;
};

class S3ResolveError : public S3Exception {
   public:
    S3ResolveError(const string& msg) : message(msg) {
    }
    virtual ~S3ResolveError() {
    }
    virtual string getMessage() {
        return "Server connection failed: " + message;
    }
    virtual string getType() {
        return "S3ResolveError";
    }

    string message;
};

class S3FailedAfterRetry : public S3Exception {
   public:
    S3FailedAfterRetry(const string& url, uint64_t times, string msg)
        : requestUrl(url), retryTimes(times), message(msg) {
    }
    virtual ~S3FailedAfterRetry() {
    }
    virtual string getMessage() {
        return "Request failed after " + std::to_string((unsigned long long)retryTimes) +
               " attempts. Message: " + message;
    }
    virtual string getType() {
        return "S3FailedAfterRetry";
    }
    string requestUrl;
    uint64_t retryTimes;
    string message;
};

// HTTP request successes, but the data is not received completely.
class S3PartialResponseError : public S3Exception {
   public:
    S3PartialResponseError(uint64_t expected, uint64_t received)
        : expectedLength(expected), receivedLength(received) {
    }
    virtual ~S3PartialResponseError() {
    }
    virtual string getMessage() {
        return "Response is not fully received. Expected: " +
               std::to_string((unsigned long long)expectedLength) +
               ", actual received: " + std::to_string((unsigned long long)receivedLength);
    }
    virtual string getType() {
        return "S3PartialResponseError";
    }

    uint64_t expectedLength;
    uint64_t receivedLength;
};

// User presses Ctrl + C or the transaction is aborted.
class S3QueryAbort : public S3Exception {
   public:
    S3QueryAbort() : message("Query is aborted") {
    }
    S3QueryAbort(const string& msg) : message(msg) {
    }
    virtual ~S3QueryAbort() {
    }
    virtual string getMessage() {
        return message;
    }

    virtual string getType() {
        return "S3QueryAbort";
    }
    string message;
};

// AWS S3 responds errors
// AccessDenied, NoSuchBucket or other kinds of InvalidRequest
class S3LogicError : public S3Exception {
   public:
    S3LogicError(string code, string msg) : message(msg), awscode(code) {
    }
    virtual ~S3LogicError() {
    }
    virtual string getMessage() {
        return "AWS returns error " + awscode + " : " + message;
    }

    virtual string getType() {
        return "S3LogicError";
    }

    string message;
    string awscode;
};

class S3RuntimeError : public S3Exception {
   public:
    S3RuntimeError(const string& msg) : message(msg) {
    }
    virtual ~S3RuntimeError() {
    }
    virtual string getMessage() {
        return "Unexpected error: " + message;
    }

    virtual string getType() {
        return "S3RuntimeError";
    }

    string message;
};

class S3ConfigError : public S3Exception {
   public:
    S3ConfigError(const string& msg, const string& field) : message(msg) {
    }
    virtual ~S3ConfigError() {
    }
    virtual string getMessage() {
        return message;
    }
    virtual string getType() {
        return "S3ConfigError";
    }

    string message;
};

class S3MemoryOverLimit : public S3Exception {
   public:
    S3MemoryOverLimit(uint64_t limit, uint64_t allocSize) : limit(limit), allocSize(allocSize) {
    }
    virtual ~S3MemoryOverLimit() {
    }
    virtual string getMessage() {
        return "Memory allocation is over limit, requested: " +
               std::to_string((unsigned long long)allocSize) +
               ", limit: " + std::to_string((unsigned long long)limit);
    }
    virtual string getType() {
        return "S3MemoryOverLimit";
    }

    uint64_t limit;
    uint64_t allocSize;
};

class S3AllocationError : public S3Exception {
   public:
    S3AllocationError(uint64_t allocSize) : allocSize(allocSize) {
    }
    virtual ~S3AllocationError() {
    }
    virtual string getMessage() {
        return "S3Alloc failed";
    }
    virtual string getType() {
        return "S3AllocationError";
    }

    uint64_t allocSize;
};
#endif
