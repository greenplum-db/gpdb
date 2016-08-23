#ifndef INCLUDE_COMPRESS_WRITER_H_
#define INCLUDE_COMPRESS_WRITER_H_

#include <zlib.h>
#include "writer.h"

// 2MB by default
extern uint64_t S3_ZIP_COMPRESS_CHUNKSIZE;

class CompressWriter : public Writer {
   public:
    CompressWriter();
    virtual ~CompressWriter();

    virtual void open(const WriterParams &params);

    // write() attempts to write up to count bytes from the buffer.
    // Always return 0 if EOF, no matter how many times it's invoked. Throw exception if encounters
    // errors.
    virtual uint64_t write(const char *buf, uint64_t count);

    // This should be reentrant, has no side effects when called multiple times.
    virtual void close();

    void setWriter(Writer *writer);

   private:
    void flush();

    Writer *writer;

    // zlib related variables.
    z_stream zstream;
    char *out;  // Output buffer for compression.

    // add this flag to make close() reentrant
    bool isClosed;
};

#endif