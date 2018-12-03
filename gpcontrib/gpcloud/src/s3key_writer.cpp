#include "s3key_writer.h"

void S3KeyWriter::open(const S3Params& params) {
    this->params = params;

    S3_CHECK_OR_DIE(this->s3Interface != NULL, S3RuntimeError, "s3Interface must not be NULL");
    S3_CHECK_OR_DIE(this->params.getChunkSize() > 0, S3RuntimeError, "chunkSize must not be zero");

    buffer.reserve(this->params.getChunkSize());

    this->uploadId = this->s3Interface->getUploadId(this->params.getS3Url());
    S3_CHECK_OR_DIE(!this->uploadId.empty(), S3RuntimeError, "Failed to get upload id");

    S3DEBUG("key: %s, upload id: %s", this->params.getS3Url().getFullUrlForCurl().c_str(),
            this->uploadId.c_str());
}

// write() first fills up the data buffer before flush it out
uint64_t S3KeyWriter::write(const char* buf, uint64_t count) {
    // Defensive code
    S3_CHECK_OR_DIE(buf != NULL, S3RuntimeError, "Buffer is NULL");
    this->checkQueryCancelSignal();

    uint64_t offset = 0;
    while (offset < count) {
        if (sharedError) {
            std::rethrow_exception(sharedException);
        }

        uint64_t bufferRemaining = this->params.getChunkSize() - this->buffer.size();
        uint64_t dataRemaining = count - offset;
        uint64_t dataToBuffer = bufferRemaining < dataRemaining ? bufferRemaining : dataRemaining;

        this->buffer.insert(this->buffer.end(), buf + offset, buf + offset + dataToBuffer);

        if (this->buffer.size() == this->params.getChunkSize()) {
            this->flushBuffer();
        }

        offset += dataToBuffer;
    }

    return count;
}

// This should be reentrant, has no side effects when called multiple times.
void S3KeyWriter::close() {
    if (!this->uploadId.empty()) {
        this->completeKeyWriting();
    }
}

void S3KeyWriter::checkQueryCancelSignal() {
    if (S3QueryIsAbortInProgress() && !this->uploadId.empty()) {
        // to avoid dead-lock when other upload threads hold the lock
        pthread_mutex_unlock(&this->mutex);

        // wait for all threads to complete
        for (size_t i = 0; i < threadList.size(); i++) {
            pthread_join(threadList[i], NULL);
        }
        this->threadList.clear();

        // to avoid double unlock as other parts may lock it
        pthread_mutex_lock(&this->mutex);

        S3DEBUG("Start aborting multipart uploading (uploadID: %s, %lu parts uploaded)",
                this->uploadId.c_str(), this->etagList.size());
        this->s3Interface->abortUpload(this->params.getS3Url(), this->uploadId);
        S3DEBUG("Finished aborting multipart uploading (uploadID: %s)", this->uploadId.c_str());

        this->etagList.clear();
        this->uploadId.clear();

        S3_DIE(S3QueryAbort, "Uploading is interrupted");
    }
}

struct ThreadParams {
    S3KeyWriter* keyWriter;
    S3VectorUInt8 data;
    uint64_t currentNumber;
};

void* S3KeyWriter::UploadThreadFunc(void* data) {
    MaskThreadSignals();

    ThreadParams* params = (ThreadParams*)data;
    S3KeyWriter* writer = params->keyWriter;

    try {
        S3DEBUG("Upload thread start: %" PRIX64 ", part number: %" PRIu64 ", data size: %zu",
                (uint64_t) pthread_self(), params->currentNumber, params->data.size());
        string etag = writer->s3Interface->uploadPartOfData(
            params->data, writer->params.getS3Url(), params->currentNumber, writer->uploadId);

        // when unique_lock destructs it will automatically unlock the mutex.
        UniqueLock threadLock(&writer->mutex);

        // etag is empty if the query is cancelled by user.
        if (!etag.empty()) {
            writer->etagList[params->currentNumber] = etag;
        }
        writer->activeThreads--;
        pthread_cond_broadcast(&writer->cv);
        S3DEBUG("Upload part finish: %" PRIX64 ", eTag: %s, part number: %" PRIu64, (uint64_t) pthread_self(),
                etag.c_str(), params->currentNumber);
    } catch (S3Exception& e) {
        S3ERROR("Upload thread error: %s", e.getMessage().c_str());
        UniqueLock exceptLock(&writer->exceptionMutex);
        writer->sharedError = true;
        writer->sharedException = std::current_exception();

        // notify the flushBuffer, otherwise it will be locked when trying to create a new thread.
        writer->activeThreads--;
        pthread_cond_broadcast(&writer->cv);
    }

    delete params;
    return NULL;
}

void S3KeyWriter::flushBuffer() {
    if (!this->buffer.empty()) {
        UniqueLock queueLock(&this->mutex);
        while (this->activeThreads >= this->params.getNumOfChunks()) {
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

        this->buffer.reserve(this->params.getChunkSize());
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
        this->s3Interface->completeMultiPart(this->params.getS3Url(), this->uploadId, etags);
    }

    S3DEBUG("Segment %d has finished uploading \"%s\"", s3ext_segid,
            this->params.getS3Url().getFullUrlForCurl().c_str());

    this->buffer.clear();
    this->etagList.clear();
    this->uploadId.clear();
}
