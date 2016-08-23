#ifndef INCLUDE_S3KEY_WRITER_H_
#define INCLUDE_S3KEY_WRITER_H_

#include "s3common.h"
#include "s3interface.h"
#include "s3macros.h"
#include "s3restful_service.h"
#include "s3url.h"
#include "writer.h"

#include <string>
#include <vector>

using std::vector;
using std::string;

class WriterBuffer : public vector<uint8_t> {};

class S3KeyWriter : public Writer {
   public:
    S3KeyWriter() : s3interface(NULL), chunkSize(0) {
    }
    virtual ~S3KeyWriter() {
        this->close();
    }
    virtual void open(const WriterParams& params);

    // write() attempts to write up to count bytes from the buffer.
    // Always return 0 if EOF, no matter how many times it's invoked. Throw exception if encounters
    // errors.
    virtual uint64_t write(const char* buf, uint64_t count);

    // This should be reentrant, has no side effects when called multiple times.
    virtual void close();

    void setS3interface(S3Interface* s3) {
        this->s3interface = s3;
    }

   protected:
    void flushBuffer();
    void completeKeyWriting();
    void checkQueryCancelSignal();

    WriterBuffer buffer;
    S3Interface* s3interface;

    string url;
    string region;

    string uploadId;
    vector<string> etagList;

    uint64_t chunkSize;
    S3Credential cred;
};

#endif /* INCLUDE_S3KEY_WRITER_H_ */
