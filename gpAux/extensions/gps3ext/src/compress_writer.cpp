#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <string.h>

#include "compress_writer.h"
#include "s3log.h"
#include "s3macros.h"

uint64_t S3_ZIP_COMPRESS_CHUNKSIZE = S3_ZIP_DEFAULT_CHUNKSIZE;

CompressWriter::CompressWriter() : writer(NULL), isClosed(false) {
    this->out = new char[S3_ZIP_COMPRESS_CHUNKSIZE];
}

CompressWriter::~CompressWriter() {
    delete this->out;
}

void CompressWriter::open(const WriterParams& params) {
    this->zstream.zalloc = Z_NULL;
    this->zstream.zfree = Z_NULL;
    this->zstream.opaque = Z_NULL;

    // With S3_DEFLATE_WINDOWSBITS, it generates gzip stream with header and trailer
    int ret = deflateInit2(&this->zstream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                           S3_DEFLATE_WINDOWSBITS, 8, Z_DEFAULT_STRATEGY);

    // init them here to get ready for both writer() and close()
    this->zstream.next_in = NULL;
    this->zstream.avail_in = 0;
    this->zstream.next_out = (Byte*)this->out;
    this->zstream.avail_out = S3_ZIP_COMPRESS_CHUNKSIZE;

    CHECK_OR_DIE_MSG(ret == Z_OK, "Failed to initialize zlib library: %s", this->zstream.msg);

    this->writer->open(params);
    this->isClosed = false;
}

uint64_t CompressWriter::write(const char* buf, uint64_t count) {
    if (buf == NULL || count == 0) {
        return 0;
    }

    // we assume data block from upper layer is always smaller than gzip chunkbuffer
    CHECK_OR_DIE_MSG(count < S3_ZIP_COMPRESS_CHUNKSIZE,
                     "Data size %" PRIu64 " is larger than S3_ZIP_COMPRESS_CHUNKSIZE", count);

    this->zstream.next_in = (Byte*)buf;
    this->zstream.avail_in = count;

    int status = deflate(&this->zstream, Z_NO_FLUSH);
    if (status < 0 && status != Z_BUF_ERROR) {
        deflateEnd(&this->zstream);
        CHECK_OR_DIE_MSG(false, "Failed to compress data: %d, %s", status, this->zstream.msg);
    }

    this->flush();

    return count;
}

void CompressWriter::close() {
    if (this->isClosed) {
        return;
    }

    int status;
    do {
        status = deflate(&this->zstream, Z_FINISH);
        this->flush();
    } while (status == Z_OK);

    deflateEnd(&this->zstream);

    if (status != Z_STREAM_END) {
        CHECK_OR_DIE_MSG(false, "Failed to finish data compression: %d, %s", status,
                         this->zstream.msg);
    }

    S3DEBUG("Compression finished: Z_STREAM_END.");

    this->writer->close();
    this->isClosed = true;
}

void CompressWriter::setWriter(Writer* writer) {
    this->writer = writer;
}

void CompressWriter::flush() {
    if (this->zstream.avail_out < S3_ZIP_COMPRESS_CHUNKSIZE) {
        this->writer->write(this->out, S3_ZIP_COMPRESS_CHUNKSIZE - this->zstream.avail_out);
        this->zstream.next_out = (Byte*)this->out;
        this->zstream.avail_out = S3_ZIP_COMPRESS_CHUNKSIZE;
    }
}
