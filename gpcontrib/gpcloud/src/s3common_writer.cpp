#include "s3common_writer.h"

void S3CommonWriter::open(const S3Params& params) {
    this->keyWriter.setS3InterfaceService(this->s3InterfaceService);

    if (params.isAutoCompress()) {
        this->upstreamWriter = &this->compressWriter;
        this->compressWriter.setWriter(&this->keyWriter);
    } else {
        this->upstreamWriter = &this->keyWriter;
    }

    this->upstreamWriter->open(params);
}

uint64_t S3CommonWriter::write(const char* buf, uint64_t count) {
    return this->upstreamWriter->write(buf, count);
}

void S3CommonWriter::close() {
    if (this->upstreamWriter != NULL) {
        this->upstreamWriter->close();
    }
}
