#include "compress_writer.h"

uint64_t S3_ZIP_COMPRESS_CHUNKSIZE = S3_ZIP_DEFAULT_CHUNKSIZE;

CompressWriter::CompressWriter() : writer(NULL), isClosed(true) {
    this->out = new char[S3_ZIP_COMPRESS_CHUNKSIZE];
}

CompressWriter::~CompressWriter() {
    try {
        this->close();
    } catch (...) {
    }
    delete this->out;
}

void CompressWriter::open(const S3Params& params) {
    this->zstream.zalloc = Z_NULL;
    this->zstream.zfree = Z_NULL;
    this->zstream.opaque = Z_NULL;

    // With S3_DEFLATE_WINDOWSBITS, it generates gzip stream with header and trailer
    int ret = deflateInit2(&this->zstream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                           S3_DEFLATE_WINDOWSBITS, 8, Z_DEFAULT_STRATEGY);

    this->isClosed = false;

    // init them here to get ready for both writer() and close()
    this->zstream.next_in = NULL;
    this->zstream.avail_in = 0;
    this->zstream.next_out = (Byte*)this->out;
    this->zstream.avail_out = S3_ZIP_COMPRESS_CHUNKSIZE;

    S3_CHECK_OR_DIE(ret == Z_OK, S3RuntimeError,
                    string("Failed to initialize zlib library: ") + this->zstream.msg);

    this->writer->open(params);
}

uint64_t CompressWriter::writeOneChunk(const char* buf, uint64_t count) {
    // Defensive code
    if (buf == NULL || count == 0) {
        return 0;
    }

    this->zstream.next_in = (Byte*)buf;
    this->zstream.avail_in = count;

    int status;
    do {
        status = deflate(&this->zstream, Z_NO_FLUSH);
        if (status < 0 && status != Z_BUF_ERROR) {
            deflateEnd(&this->zstream);
            S3_CHECK_OR_DIE(false, S3RuntimeError,
                            string("Failed to compress data: ") +
                                std::to_string((unsigned long long)status) + ", " +
                                this->zstream.msg);
        }

        this->flush();

        // output buffer is same size to input buffer, most cases data
        // is smaller after compressed. But if this->zstream.avail_in > 0 after deflate(), then data
        // is larger after compressed and some input data is pending. For example when compressing a
        // chunk that is already compressed, we will encounter this case. So we need to loop here.
    } while (status == Z_OK && (this->zstream.avail_in > 0));

    return count;
}

uint64_t CompressWriter::write(const char* buf, uint64_t count) {
    // Defensive code
    if (buf == NULL || count == 0) {
        return 0;
    }

    uint64_t writtenLen = 0;

    for (uint64_t i = 0; i < (count / S3_ZIP_COMPRESS_CHUNKSIZE); i++) {
        writtenLen += this->writeOneChunk(buf + writtenLen, S3_ZIP_COMPRESS_CHUNKSIZE);
    }

    if (writtenLen < count) {
        writtenLen += this->writeOneChunk(buf + writtenLen, count - writtenLen);
    }

    return writtenLen;
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
        S3_CHECK_OR_DIE(false, S3RuntimeError,
                        string("Failed to compress data: ") +
                            std::to_string((unsigned long long)status) + ", " + this->zstream.msg);
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
