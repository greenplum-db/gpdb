#ifndef INCLUDE_S3COMMON_WRITER_H_
#define INCLUDE_S3COMMON_WRITER_H_

#include "compress_writer.h"
#include "s3common_headers.h"
#include "s3key_writer.h"
#include "s3url.h"

class S3CommonWriter : public Writer {
   public:
    S3CommonWriter() : upstreamWriter(NULL), s3InterfaceService(NULL) {
    }

    virtual ~S3CommonWriter() {
        this->close();
    }

    virtual void open(const S3Params& params);

    // write() attempts to write up to count bytes from the buffer.
    // Always return 0 if EOF, no matter how many times it's invoked. Throw exception if encounters
    // errors.
    virtual uint64_t write(const char* buf, uint64_t count);

    // This should be reentrant, has no side effects when called multiple times.
    virtual void close();

    // Used by Mock, DO NOT call it in other places.
    void setS3InterfaceService(S3Interface* s3InterfaceService) {
        this->s3InterfaceService = s3InterfaceService;
    }

   protected:
    Writer* upstreamWriter;
    S3Interface* s3InterfaceService;
    S3KeyWriter keyWriter;
    CompressWriter compressWriter;
};

#endif
