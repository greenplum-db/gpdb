#include <algorithm>

#include "s3log.h"
#include "s3macros.h"
#include "uncompress_reader.h"
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

UncompressReader::UncompressReader() {
    this->reader = NULL;
    this->in = new char[S3_ZIP_CHUNKSIZE];
    this->out = new char[S3_ZIP_CHUNKSIZE];
    this->outOffset = 0;
}

UncompressReader::~UncompressReader() {
    delete this->in;
    delete this->out;
}

// Used for unit test to adjust buf size
void UncompressReader::resizeUncompressReaderBuffer(int size) {
    delete this->in;
    delete this->out;
    this->in = new char[size];
    this->out = new char[size];
    this->outOffset = 0;
    this->zstream.avail_out = size;
}

void UncompressReader::setReader(Reader *reader) {
    this->reader = reader;
}

void UncompressReader::open(const ReaderParams &params) {
    // allocate inflate state
    zstream.zalloc = Z_NULL;
    zstream.zfree = Z_NULL;
    zstream.opaque = Z_NULL;
    zstream.next_in = Z_NULL;
    zstream.next_out = (Byte *)this->out;

    zstream.avail_in = 0;
    zstream.avail_out = S3_ZIP_CHUNKSIZE;
    this->outOffset = 0;

    // 47 is the number of windows bits, to make sure zlib could recognize and decode gzip stream.
    int ret = inflateInit2(&zstream, 47);
    CHECK_OR_DIE_MSG(ret == Z_OK, "%s", "failed to initialize zlib library");
}

uint64_t UncompressReader::read(char *buf, uint64_t bufSize) {
    uint64_t ret = 0;
    uint64_t remainingBufLen = bufSize;
    uint64_t remainingOutLen = S3_ZIP_CHUNKSIZE - this->zstream.avail_out - this->outOffset;
    bool readFinished = false;

    do {
        S3DEBUG("has = %" PRIu64 ", offset = %d, chunksize = %u, avail_out = %u, count = %" PRIu64,
                remainingOutLen, outOffset, S3_ZIP_CHUNKSIZE, this->zstream.avail_out, bufSize);

        if (this->outOffset < (S3_ZIP_CHUNKSIZE - this->zstream.avail_out)) {
            // Read remaining data in out buffer uncompressed last time.
        } else {
            this->uncompress();
            this->outOffset = 0;  // reset cursor for out buffer to read from beginning.
            remainingOutLen = S3_ZIP_CHUNKSIZE - this->zstream.avail_out;
        }

        int count = std::min(remainingOutLen, remainingBufLen);
        memcpy(buf + ret, this->out + outOffset, count);
        this->outOffset += count;
        remainingBufLen -= count;
        ret += count;

        // When to break?
        //  1) Decompress is done, no more data in 'in' buf. Or
        //  2) We have no enough free-space in 'buf' to hold next entire 'out' buf.
        //      maybe next decompressed size < S3_ZIP_CHUNKSIZE, but we have no chance
        //      to predict it, so use the max value 'S3_ZIP_CHUNKSIZE' instead.
        //      We can improve it in future by optimizing this conditions.
        if ((this->zstream.avail_in == 0 && this->zstream.avail_out == S3_ZIP_CHUNKSIZE) ||
            remainingBufLen < S3_ZIP_CHUNKSIZE) {
            readFinished = true;
        }

    } while (!readFinished);

    return ret;
}

// Read compressed data from underlying reader and uncompress to this->out buffer.
// If no more data to consume, this->zstream.avail_out == S3_ZIP_CHUNKSIZE;
void UncompressReader::uncompress() {
    if (this->zstream.avail_in == 0) {
        this->zstream.avail_out = S3_ZIP_CHUNKSIZE;
        this->zstream.next_out = (Byte *)this->out;

        // reader S3_ZIP_CHUNKSIZE data from underlying reader and put into this->in buffer.
        uint64_t hasRead = this->reader->read(this->in, S3_ZIP_CHUNKSIZE);
        if (hasRead == 0) {
            S3DEBUG(
                "No more data to uncompress: avail_in = %u, avail_out = %u, total_in = %u, "
                "total_out = %u",
                zstream.avail_in, zstream.avail_out, zstream.total_in, zstream.total_out);
            return;  // EOF
        }

        this->zstream.next_in = (Byte *)this->in;
        this->zstream.avail_in = hasRead;
    } else {
        // Still have more data in 'in' buffer to decode.
        this->zstream.avail_out = S3_ZIP_CHUNKSIZE;
        this->zstream.next_out = (Byte *)this->out;
    }

    S3DEBUG("Before decompress: avail_in = %u, avail_out = %u, total_in = %u, total_out = %u",
            zstream.avail_in, zstream.avail_out, zstream.total_in, zstream.total_out);

    int status = inflate(&this->zstream, Z_NO_FLUSH);
    if (status == Z_STREAM_END) {
        S3DEBUG("compression finished: Z_STREAM_END.");
    } else if (status < 0 || status == Z_NEED_DICT) {
        inflateEnd(&this->zstream);
        CHECK_OR_DIE_MSG(false, "failed to decompress data: %d", status);
    }

    S3DEBUG("After decompress: avail_in = %u, avail_out = %u, total_in = %u, total_out = %u",
            zstream.avail_in, zstream.avail_out, zstream.total_in, zstream.total_out);

    return;
}

void UncompressReader::close() {
    inflateEnd(&zstream);
}
