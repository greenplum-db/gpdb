#ifndef INCLUDE_DECOMPRESS_READER_H_
#define INCLUDE_DECOMPRESS_READER_H_

#include "reader.h"
#include "s3common_headers.h"
#include "s3exception.h"
#include "s3macros.h"
#include "s3params.h"

// 2MB by default
extern uint64_t S3_ZIP_DECOMPRESS_CHUNKSIZE;

class DecompressReader : public Reader {
   public:
    DecompressReader();
    virtual ~DecompressReader();

    virtual void open(const S3Params &params);

    // read() attempts to read up to count bytes into the buffer.
    // Return 0 if EOF. Throw exception if encounters errors.
    virtual uint64_t read(char *buf, uint64_t count);

    // This should be reentrant, has no side effects when called multiple times.
    virtual void close();

    void setReader(Reader *reader);

    void resizeDecompressReaderBuffer(uint64_t size);

   private:
    void decompress();

    uint64_t getDecompressedBytesNum() {
        return S3_ZIP_DECOMPRESS_CHUNKSIZE - this->zstream.avail_out;
    }

    Reader *reader;

    // zlib related variables.
    z_stream zstream;
    char *in;            // Input buffer for decompression.
    char *out;           // Output buffer for decompression.
    uint64_t outOffset;  // Next position to read in out buffer.

    bool isClosed;
};

#endif /* INCLUDE_DECOMPRESS_READER_H_ */
