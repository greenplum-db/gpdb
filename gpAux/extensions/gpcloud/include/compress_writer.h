#ifndef INCLUDE_COMPRESS_WRITER_H_
#define INCLUDE_COMPRESS_WRITER_H_

#include "s3common_headers.h"
#include "s3exception.h"
#include "s3macros.h"
#include "writer.h"

// 2MB by default
extern uint64_t S3_ZIP_COMPRESS_CHUNKSIZE;

class CompressWriter : public Writer {
   public:
    CompressWriter();
    virtual ~CompressWriter();

    virtual void open(const S3Params &params);

    // write() attempts to write up to count bytes from the buffer.
    // If 'count' is larger than Zip chunk-buffer, it invokes writeOneChunk()
    // repeatedly to finish upload. Throw exception if encounters errors.
    virtual uint64_t write(const char *buf, uint64_t count);

    // This should be reentrant, has no side effects when called multiple times.
    virtual void close();

    void setWriter(Writer *writer);

   private:
    void flush();
    uint64_t writeOneChunk(const char *buf, uint64_t count);

    Writer *writer;

    // zlib related variables.
    z_stream zstream;
    char *out;  // Output buffer for compression.

    // add this flag to make close() reentrant
    bool isClosed;
};

#endif
