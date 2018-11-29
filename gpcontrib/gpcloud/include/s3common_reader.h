#ifndef INCLUDE_S3COMMON_READER_H_
#define INCLUDE_S3COMMON_READER_H_

#include "decompress_reader.h"
#include "s3common_headers.h"
#include "s3exception.h"
#include "s3key_reader.h"

class S3CommonReader : public Reader {
   public:
    S3CommonReader() : upstreamReader(NULL), s3InterfaceService(NULL) {
    }

    virtual ~S3CommonReader() {
        this->close();
    }

    virtual void open(const S3Params& params);

    // read() attempts to read up to count bytes into the buffer.
    // Return 0 if EOF. Throw exception if encounters errors.
    virtual uint64_t read(char* buf, uint64_t count);

    // This should be reentrant, has no side effects when called multiple times.
    virtual void close();

    // Used by Mock, DO NOT call it in other places.
    void setS3InterfaceService(S3Interface* s3InterfaceService) {
        this->s3InterfaceService = s3InterfaceService;
    }

   protected:
    Reader* upstreamReader;
    S3Interface* s3InterfaceService;
    S3KeyReader keyReader;
    DecompressReader decompressReader;
};

#endif /* INCLUDE_S3COMMON_READER_H_ */
