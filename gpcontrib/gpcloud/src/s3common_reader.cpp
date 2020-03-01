#include "s3common_reader.h"

void S3CommonReader::open(const S3Params &params) {
    this->keyReader.setS3InterfaceService(s3InterfaceService);

    S3CompressionType compressionType = s3InterfaceService->checkCompressionType(params.getS3Url());

    switch (compressionType) {
        case S3_COMPRESSION_DEFLATE:
        case S3_COMPRESSION_GZIP:
            this->upstreamReader = &this->decompressReader;
            this->decompressReader.setReader(&this->keyReader);
            break;
        case S3_COMPRESSION_PLAIN:
            this->upstreamReader = &this->keyReader;
            break;
        default:
            S3_CHECK_OR_DIE(false, S3RuntimeError, "unknown file type");
    };

    this->upstreamReader->open(params);
}

// read() attempts to read up to count bytes into the buffer.
// Return 0 if EOF. Throw exception if encounters errors.
uint64_t S3CommonReader::read(char *buf, uint64_t count) {
    return this->upstreamReader->read(buf, count);
}

// This should be reentrant, has no side effects when called multiple times.
void S3CommonReader::close() {
    if (this->upstreamReader != NULL) {
        this->upstreamReader->close();
        this->upstreamReader = NULL;
    }
}
