#ifndef INCLUDE_S3COMMON_READER_H_
#define INCLUDE_S3COMMON_READER_H_

#include "decompress_reader.h"
#include "s3key_reader.h"

class S3CommonReader : public Reader {
   public:
    S3CommonReader() : upstreamReader(NULL), s3service(NULL) {
    }

    virtual ~S3CommonReader() {
    }

    virtual void open(const S3Params& params);

    // read() attempts to read up to count bytes into the buffer.
    // Return 0 if EOF. Throw exception if encounters errors.
    virtual uint64_t read(char* buf, uint64_t count);

    // This should be reentrant, has no side effects when called multiple times.
    virtual void close();

    // Used by Mock, DO NOT call it in other places.
    void setS3service(S3Interface* s3service) {
        this->s3service = s3service;
    }

   protected:
    Reader* upstreamReader;
    S3Interface* s3service;
    S3KeyReader keyReader;
    DecompressReader decompressReader;
};

#endif /* INCLUDE_S3COMMON_READER_H_ */
