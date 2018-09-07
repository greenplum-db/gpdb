#include "s3key_reader.h"

// Return (offset, length) of next chunk to download,
// or (fileSize, 0) if reach end of file.
Range OffsetMgr::getNextOffset() {
    Range ret;

    pthread_mutex_lock(&this->offsetLock);
    ret.offset = std::min(this->curPos, this->keySize);

    if (this->curPos + this->chunkSize > this->keySize) {
        ret.length = this->keySize - this->curPos;
        this->curPos = this->keySize;
    } else {
        ret.length = this->chunkSize;
        this->curPos += this->chunkSize;
    }
    pthread_mutex_unlock(&this->offsetLock);

    return ret;
}

ChunkBuffer::ChunkBuffer(const S3Url& s3Url, S3KeyReader& reader, const S3MemoryContext& context)
    : s3Url(s3Url), chunkData(context), offsetMgr(reader.getOffsetMgr()), sharedKeyReader(reader) {
    s3Interface = NULL;
    Range range = offsetMgr.getNextOffset();
    curFileOffset = range.offset;
    chunkDataSize = range.length;
    status = ReadyToFill;
    eof = false;
    curChunkOffset = 0;
    pthread_mutex_init(&this->statusMutex, NULL);
    pthread_cond_init(&this->statusCondVar, NULL);
}

ChunkBuffer::~ChunkBuffer() {
    pthread_mutex_destroy(&this->statusMutex);
    pthread_cond_destroy(&this->statusCondVar);
}

ChunkBuffer& ChunkBuffer::operator=(const ChunkBuffer& other) {
    this->s3Url = other.s3Url;
    this->eof = other.eof;
    this->status = other.status;
    this->curFileOffset = other.curFileOffset;
    this->curChunkOffset = other.curChunkOffset;
    this->chunkDataSize = other.chunkDataSize;

    return *this;
}

// ret < len means EMPTY
// that's why it checks if leftLen is larger than *or equal to* len below[1], provides a chance ret
// is 0, which is smaller than len. Otherwise, other functions won't know when to read next buffer.
uint64_t ChunkBuffer::read(char* buf, uint64_t len) {
    // GPDB abort signal stops s3_import(), this check is not needed if s3_import() every time calls
    // ChunkBuffer->Read() only once, otherwise(as we did in downstreamReader->read() for
    // decompression feature before), first call sets buffer to ReadyToFill, second call hangs.
    S3_CHECK_OR_DIE(!S3QueryIsAbortInProgress(), S3QueryAbort, "");

    UniqueLock statusLock(&this->statusMutex);
    while (this->status != ReadyToRead) {
        pthread_cond_wait(&this->statusCondVar, &this->statusMutex);
    }

    // Error is shared between all chunks.
    if (this->isError()) {
        return 0;
    }

    uint64_t leftLen = this->chunkDataSize - this->curChunkOffset;
    uint64_t lenToRead = std::min(len, leftLen);

    if (lenToRead != 0) {
        memcpy(buf, this->chunkData.data() + this->curChunkOffset, lenToRead);
    }

    if (len <= leftLen) {                   // [1]
        this->curChunkOffset += lenToRead;  // not empty
    } else {                                // empty, reset everything
        this->curChunkOffset = 0;

        if (!this->isEOF()) {
            // Release chunkData memory to reduce consumption.
            this->chunkData.release();

            this->status = ReadyToFill;

            Range range = this->offsetMgr.getNextOffset();
            this->curFileOffset = range.offset;
            this->chunkDataSize = range.length;

            pthread_cond_signal(&this->statusCondVar);
        }
    }

    return lenToRead;
}

// returning uint64_t(-1) means error
uint64_t ChunkBuffer::fill() {
    UniqueLock statusLock(&this->statusMutex);

    while (this->status != ReadyToFill) {
        pthread_cond_wait(&this->statusCondVar, &this->statusMutex);
    }

    if (S3QueryIsAbortInProgress() || this->isError()) {
        this->setSharedError(true);
        this->status = ReadyToRead;
        pthread_cond_signal(&this->statusCondVar);
        return -1;
    }

    uint64_t offset = this->curFileOffset;
    uint64_t leftLen = this->chunkDataSize;

    uint64_t readLen = 0;

    if (leftLen != 0) {
        try {
            readLen = this->s3Interface->fetchData(offset, this->chunkData, leftLen, this->s3Url);
            if (readLen != leftLen) {
                S3DEBUG("Failed to fetch expected data from S3");
                this->setSharedError(true, S3PartialResponseError(leftLen, readLen));
            } else {
                S3DEBUG("Got %" PRIu64 " bytes from S3", readLen);
            }
        } catch (S3Exception& e) {
            S3DEBUG("Failed to fetch expected data from S3");
            this->setSharedError(true);
        }
    }

    if (offset + leftLen >= offsetMgr.getKeySize()) {
        readLen = 0;  // Nothing to read, EOF
        S3DEBUG("Reached the end of file");
        this->eof = true;
    }

    this->status = ReadyToRead;
    pthread_cond_signal(&this->statusCondVar);

    return (this->isError()) ? -1 : readLen;
}

static void* DownloadThreadFunc(void* data) {
    MaskThreadSignals();

    ChunkBuffer* buffer = static_cast<ChunkBuffer*>(data);

    uint64_t filledSize = 0;
    S3DEBUG("Downloading thread starts");
    do {
        if (S3QueryIsAbortInProgress()) {
            S3INFO("Downloading thread is interrupted");

            // error is shared between all chunks, so all chunks will stop.
            buffer->setSharedError(true, S3QueryAbort("Downloading thread is interrupted"));

            // have to unlock ChunkBuffer::read in some certain conditions, for instance, status is
            // not ReadyToRead, and read() is waiting for signal stat_cond.
            buffer->setStatus(ReadyToRead);
            pthread_cond_signal(buffer->getStatCond());

            return NULL;
        }

        filledSize = buffer->fill();

        if (filledSize != 0) {
            if (buffer->isError()) {
                S3DEBUG("Failed to fill downloading buffer");
                break;
            } else {
                S3DEBUG("Size of filled data is %" PRIu64, filledSize);
            }
        }
    } while (!buffer->isEOF());
    S3DEBUG("Downloading thread ended");
    return NULL;
}

void S3KeyReader::open(const S3Params& params) {
    S3_CHECK_OR_DIE(this->s3Interface != NULL, S3RuntimeError, "s3Interface must not be NULL");

    this->sharedError = false;

    this->numOfChunks = params.getNumOfChunks();
    S3_CHECK_OR_DIE(this->numOfChunks > 0, S3RuntimeError, "numOfChunks must not be zero");

    this->offsetMgr.setKeySize(params.getKeySize());
    this->offsetMgr.setChunkSize(params.getChunkSize());

    S3_CHECK_OR_DIE(params.getChunkSize() > 0, S3RuntimeError,
                    "chunk size must be greater than zero");

    this->chunkBuffers.reserve(this->numOfChunks);

    for (uint64_t i = 0; i < this->numOfChunks; i++) {
        this->chunkBuffers.emplace_back(params.getS3Url(), *this, params.getMemoryContext());
    }

    for (uint64_t i = 0; i < this->numOfChunks; i++) {
        this->chunkBuffers[i].setS3InterfaceService(this->s3Interface);

        pthread_t thread;
        pthread_create(&thread, NULL, DownloadThreadFunc, &this->chunkBuffers[i]);
        this->threads.push_back(thread);
    }
}

uint64_t S3KeyReader::read(char* buf, uint64_t count) {
    uint64_t fileLen = this->offsetMgr.getKeySize();
    uint64_t readLen = 0;

    do {
        // confirm there is no more available data, done with this file
        if (this->transferredKeyLen >= fileLen) {
            if (!this->hasEol && !this->eolAppended) {
                uint64_t eolLen = strlen(eolString);
                strncpy(buf, eolString, eolLen);

                this->eolAppended = true;

                return eolLen;
            }

            return 0;
        }

        ChunkBuffer& buffer = chunkBuffers[this->curReadingChunk % this->numOfChunks];

        readLen = buffer.read(buf, count);

        if (this->isSharedError()) {
            if (this->sharedException != NULL) {
                std::rethrow_exception(this->sharedException);
            } else {
                throw S3RuntimeError("Unexpected runtime error, sharedException is NULL");
            }
        }

        this->transferredKeyLen += readLen;
        if (this->transferredKeyLen == fileLen) {
            if (buf[readLen - 1] == '\r' || buf[readLen - 1] == '\n') {
                this->hasEol = true;
            }
        }

        if (readLen < count) {
            this->curReadingChunk++;
        }

        count -= readLen;
    } while (readLen == 0);  // retry to confirm whether thread reading is finished or chunk size is
                             // divisible by get()'s buffer size

    return readLen;
}

// reset marks before reading next key
void S3KeyReader::reset() {
    this->sharedError = false;
    this->curReadingChunk = 0;
    this->transferredKeyLen = 0;

    this->offsetMgr.reset();

    this->chunkBuffers.clear();
    this->threads.clear();

    this->hasEol = false;
    this->eolAppended = false;
}

void S3KeyReader::close() {
    // to interupt downlading thread, we must: (check ChunkBuffer::fill())
    // 1. set condition to ReadyToFill and signal conditional_variable.
    // 2. set the shared error status to prevent download thread from continuing.
    this->sharedError = true;

    for (uint64_t i = 0; i < this->chunkBuffers.size(); i++) {
        UniqueLock lock(this->chunkBuffers[i].getStatMutex());
        this->chunkBuffers[i].setStatus(ReadyToFill);
        pthread_cond_signal(this->chunkBuffers[i].getStatCond());
    }

    for (uint64_t i = 0; i < this->threads.size(); i++) {
        if (this->threads[i] == 0) {
            continue;
        }

        pthread_join(this->threads[i], NULL);

        this->threads[i] = 0;
    }

    this->reset();
}
