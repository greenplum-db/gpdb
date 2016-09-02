#ifndef INCLUDE_S3COMMON_WRITER_H_
#define INCLUDE_S3COMMON_WRITER_H_

#include "compress_writer.h"
#include "s3key_writer.h"

class S3CommonWriter : public Writer {
   public:
    S3CommonWriter() : upstreamWriter(NULL), s3service(NULL) {
    }

    virtual ~S3CommonWriter() {
    }

    virtual void open(const S3Params& params);

    // write() attempts to write up to count bytes from the buffer.
    // Always return 0 if EOF, no matter how many times it's invoked. Throw exception if encounters
    // errors.
    virtual uint64_t write(const char* buf, uint64_t count);

    // This should be reentrant, has no side effects when called multiple times.
    virtual void close();

    // Used by Mock, DO NOT call it in other places.
    void setS3service(S3Interface* s3service) {
        this->s3service = s3service;
    }

   protected:
    Writer* upstreamWriter;
    S3Interface* s3service;
    S3KeyWriter keyWriter;
    CompressWriter compressWriter;
};

#endif
