#ifndef INCLUDE_DECOMPRESS_READER_H_
#define INCLUDE_DECOMPRESS_READER_H_

#include <zlib.h>
#include "reader.h"

// 256K by default
extern unsigned int S3_ZIP_CHUNKSIZE;

class DecompressReader : public Reader {
   public:
    DecompressReader();
    virtual ~DecompressReader();

    void open(const ReaderParams &params);

    // read() attempts to read up to count bytes into the buffer starting at
    // buffer.
    // Return 0 if EOF. Throw exception if encounters errors.
    uint64_t read(char *buf, uint64_t count);

    // This should be reentrant, has no side effects when called multiple times.
    void close();

    void setReader(Reader *reader);

    void resizeDecompressReaderBuffer(int size);

   private:
    void decompress();

    Reader *reader;

    // zlib related variables.
    z_stream zstream;
    char *in;       // Input buffer for decompression.
    char *out;      // Output buffer for decompression.
    int outOffset;  // Next position to read in out buffer.
};

#endif /* INCLUDE_DECOMPRESS_READER_H_ */
