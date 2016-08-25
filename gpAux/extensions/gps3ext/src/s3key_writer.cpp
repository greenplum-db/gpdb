#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "s3key_writer.h"

void S3KeyWriter::open(const WriterParams &params) {
    this->url = params.getKeyUrl();
    this->region = params.getRegion();
    this->cred = params.getCred();
    this->chunkSize = params.getChunkSize();

    CHECK_OR_DIE_MSG(this->s3interface != NULL, "%s", "s3interface must not be NULL");
    CHECK_OR_DIE_MSG(this->chunkSize > 0, "%s", "chunkSize must not be zero");

    buffer.reserve(this->chunkSize);

    this->uploadId = this->s3interface->getUploadId(this->url, this->region, this->cred);
    CHECK_OR_DIE_MSG(!this->uploadId.empty(), "%s", "Failed to get upload id");

    S3DEBUG("key: %s, upload id: %s", this->url.c_str(), this->uploadId.c_str());
}

// write() attempts to write up to count bytes from the buffer.
// Always return 0 if EOF, no matter how many times it's invoked.
// Throw exception if encounters errors.
uint64_t S3KeyWriter::write(const char *buf, uint64_t count) {
    CHECK_OR_DIE(buf != NULL);
    this->checkQueryCancelSignal();

    // GPDB issues 64K- block every time and chunkSize is 8MB+
    if (count > this->chunkSize) {
        S3ERROR(PRIu64 " is larger than chunkSize " PRIu64, count, this->chunkSize);
        CHECK_OR_DIE_MSG(false, PRIu64 " is larger than chunkSize " PRIu64, count, this->chunkSize);
    }

    if ((this->buffer.size() + count) > this->chunkSize) {
        flushBuffer();
    }

    this->buffer.insert(this->buffer.end(), buf, buf + count);

    return count;
}

// This should be reentrant, has no side effects when called multiple times.
void S3KeyWriter::close() {
    this->checkQueryCancelSignal();

    if (!this->uploadId.empty()) {
        this->completeKeyWriting();
    }
}

void S3KeyWriter::checkQueryCancelSignal() {
    if (QueryCancelPending && !this->uploadId.empty()) {
        this->s3interface->abortUpload(this->url, this->region, this->cred, this->uploadId);
        this->etagList.clear();
        this->uploadId.clear();
        CHECK_OR_DIE_MSG(false, "%s", "Uploading is interrupted by user");
    }
}

void S3KeyWriter::flushBuffer() {
    if (!this->buffer.empty()) {
        string etag = this->s3interface->uploadPartOfData(
            buffer, this->url, this->region, this->cred, etagList.size() + 1, this->uploadId);

        etagList.push_back(etag);

        S3DEBUG("Uploaded one part to S3, eTag: %s", etag.c_str());

        this->buffer.clear();

        // Most time query is canceled during uploadPartOfData. This is the first chance to cancel
        // and clean up upload. Otherwise GPDB will call with LAST_CALL but QueryCancelPending is
        // set to false, and we can't detect query cancel signal in S3KeyWriter::close().
        this->checkQueryCancelSignal();
    }
}

void S3KeyWriter::completeKeyWriting() {
    // make sure the buffer is clear
    this->flushBuffer();

    if (!this->etagList.empty() && !this->uploadId.empty()) {
        this->s3interface->completeMultiPart(this->url, this->region, this->cred, this->uploadId,
                                             etagList);
    }

    S3DEBUG("Segment %d has finished uploading \"%s\"", s3ext_segid, this->url.c_str());

    this->etagList.clear();
    this->uploadId.clear();
}