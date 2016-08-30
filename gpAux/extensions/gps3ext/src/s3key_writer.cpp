#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "s3key_writer.h"

void S3KeyWriter::open(const WriterParams& params) {
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
uint64_t S3KeyWriter::write(const char* buf, uint64_t count) {
    CHECK_OR_DIE(buf != NULL);
    this->checkQueryCancelSignal();

    // GPDB issues 64K- block every time and chunkSize is 8MB+
    if (count > this->chunkSize) {
        S3ERROR("%" PRIu64 " is larger than chunkSize %" PRIu64, count, this->chunkSize);
        CHECK_OR_DIE_MSG(false, "%" PRIu64 " is larger than chunkSize %" PRIu64, count,
                         this->chunkSize);
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
    if (S3QueryIsAbortInProgress() && !this->uploadId.empty()) {
        // wait for all threads to complete
        for (size_t i = 0; i < threadList.size(); i++) {
            pthread_join(threadList[i], NULL);
        }
        this->threadList.clear();

        S3DEBUG("Start aborting multipart uploading (uploadID: %s, %lu parts uploaded)",
                this->uploadId.c_str(), this->etagList.size());
        this->s3interface->abortUpload(this->url, this->region, this->cred, this->uploadId);
        S3DEBUG("Finished aborting multipart uploading (uploadID: %s)", this->uploadId.c_str());

        this->etagList.clear();
        this->uploadId.clear();
        CHECK_OR_DIE_MSG(false, "%s", "Uploading is interrupted by user");
    }
}

struct ThreadParams {
    S3KeyWriter* keyWriter;
    WriterBuffer data;
    uint64_t currentNumber;
};

void* S3KeyWriter::UploadThreadFunc(void* data) {
    ThreadParams* params = (ThreadParams*)data;
    S3KeyWriter* writer = params->keyWriter;

    try {
        S3DEBUG("Upload thread start: %p, part number: %" PRIu64 ", data size: %" PRIu64,
                pthread_self(), params->currentNumber, params->data.size());
        string etag = writer->s3interface->uploadPartOfData(
            params->data, writer->url, writer->region, writer->cred, params->currentNumber,
            writer->uploadId);

        // when unique_lock destructs it will automatically unlock the mutex.
        UniqueLock threadLock(&writer->mutex);

        // etag is empty if the query is cancelled by user.
        if (!etag.empty()) {
            writer->etagList[params->currentNumber] = etag;
        }
        writer->activeThreads--;
        pthread_cond_broadcast(&writer->cv);
        S3DEBUG("Upload part finish: %p, eTag: %s, part number: %" PRIu64, pthread_self(),
                etag.c_str(), params->currentNumber);
    } catch (std::exception& e) {
        S3ERROR("Upload thread error: %s", e.what());
    }

    delete params;
    return NULL;
}

void S3KeyWriter::flushBuffer() {
    if (!this->buffer.empty()) {
        UniqueLock queueLock(&this->mutex);
        while (this->activeThreads >= (uint64_t)s3ext_threadnum) {
            pthread_cond_wait(&this->cv, &this->mutex);
        }

        // Most time query is canceled during uploadPartOfData(). This is the first chance to cancel
        // and clean up upload.
        this->checkQueryCancelSignal();

        this->activeThreads++;

        pthread_t writerThread;
        ThreadParams* params = new ThreadParams();
        params->keyWriter = this;
        params->data.swap(this->buffer);
        params->currentNumber = ++this->partNumber;
        pthread_create(&writerThread, NULL, UploadThreadFunc, params);
        threadList.emplace_back(writerThread);

        this->buffer.clear();
    }
}

void S3KeyWriter::completeKeyWriting() {
    // make sure the buffer is clear
    this->flushBuffer();

    // wait for all threads to complete
    for (size_t i = 0; i < threadList.size(); i++) {
        pthread_join(threadList[i], NULL);
    }
    this->threadList.clear();

    this->checkQueryCancelSignal();

    vector<string> etags;
    // it is equivalent to foreach(e in etagList) push_back(e.second);
    // transform(etagList.begin(), etagList.end(), etags.begin(),
    //          [](std::pair<const uint64_t, string>& p) { return p.second; });
    etags.reserve(etagList.size());

    for (map<uint64_t, string>::iterator i = etagList.begin(); i != etagList.end(); i++) {
        etags.push_back(i->second);
    }

    if (!this->etagList.empty() && !this->uploadId.empty()) {
        this->s3interface->completeMultiPart(this->url, this->region, this->cred, this->uploadId,
                                             etags);
    }

    S3DEBUG("Segment %d has finished uploading \"%s\"", s3ext_segid, this->url.c_str());

    this->etagList.clear();
    this->uploadId.clear();
}
