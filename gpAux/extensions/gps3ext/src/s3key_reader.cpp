#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "s3common.h"
#include "s3interface.h"
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

ChunkBuffer::ChunkBuffer(const string& url, OffsetMgr& mgr, bool& sharedError,
                         const S3Credential& cred, const string& region)
    : sourceUrl(url),
      sharedError(sharedError),
      chunkData(NULL),
      offsetMgr(mgr),
      credential(cred),
      region(region),
      s3interface(NULL) {
    Range range = mgr.getNextOffset();
    curFileOffset = range.offset;
    chunkDataSize = range.length;
    status = ReadyToFill;
    eof = false;
    curChunkOffset = 0;
}

ChunkBuffer::~ChunkBuffer() {
    this->destroy();
}

// Copy constructor will copy members, but chunkData must not be initialized before copy.
// otherwise when worked with vector it will be freed twice.
void ChunkBuffer::init() {
    CHECK_OR_DIE_MSG(chunkData == NULL, "%s", "Error: reinitializing chunkBuffer.");

    chunkData = new char[offsetMgr.getChunkSize()];
    CHECK_OR_DIE_MSG(chunkData != NULL, "%s", "Failed to allocate Buffer, no enough memory?");

    pthread_mutex_init(&this->stat_mutex, NULL);
    pthread_cond_init(&this->stat_cond, NULL);
}

void ChunkBuffer::destroy() {
    if (this->chunkData != NULL) {
        delete this->chunkData;
        this->chunkData = NULL;

        pthread_mutex_destroy(&this->stat_mutex);
        pthread_cond_destroy(&this->stat_cond);
    }
}

// ret < len means EMPTY
// that's why it checks if leftLen is larger than *or equal to* len below[1], provides a chance ret
// is 0, which is smaller than len. Otherwise, other functions won't know when to read next buffer.
uint64_t ChunkBuffer::read(char* buf, uint64_t len) {
    // QueryCancelPending stops s3_import(), this check is not needed if
    // s3_import() every time calls ChunkBuffer->Read() only once, otherwise(as we did in
    // downstreamReader->read() for decompression feature before), first call sets buffer to
    // ReadyToFill, second call hangs.
    CHECK_OR_DIE_MSG(!QueryCancelPending, "%s", "ChunkBuffer reading is interrupted by GPDB");

    pthread_mutex_lock(&this->stat_mutex);
    while (this->status != ReadyToRead) {
        pthread_cond_wait(&this->stat_cond, &this->stat_mutex);
    }

    // Error is shared between all chunks.
    if (this->isError()) {
        pthread_mutex_unlock(&this->stat_mutex);
        CHECK_OR_DIE_MSG(false, "%s", "ChunkBuffers encounter a downloading error.");
    }

    uint64_t leftLen = this->chunkDataSize - this->curChunkOffset;
    uint64_t lenToRead = std::min(len, leftLen);

    if (lenToRead != 0) {
        memcpy(buf, this->chunkData + this->curChunkOffset, lenToRead);
    }

    if (len <= leftLen) {                   // [1]
        this->curChunkOffset += lenToRead;  // not empty
    } else {                                // empty, reset everything
        this->curChunkOffset = 0;

        if (!this->isEOF()) {
            this->status = ReadyToFill;

            Range range = this->offsetMgr.getNextOffset();
            this->curFileOffset = range.offset;
            this->chunkDataSize = range.length;

            pthread_cond_signal(&this->stat_cond);
        }
    }

    pthread_mutex_unlock(&this->stat_mutex);

    return lenToRead;
}

// returning -1 means error
uint64_t ChunkBuffer::fill() {
    pthread_mutex_lock(&this->stat_mutex);
    while (this->status != ReadyToFill) {
        pthread_cond_wait(&this->stat_cond, &this->stat_mutex);
    }

    uint64_t offset = this->curFileOffset;
    uint64_t leftLen = this->chunkDataSize;

    uint64_t readLen = 0;

    if (leftLen != 0) {
        readLen = this->s3interface->fetchData(offset, this->chunkData, leftLen, this->sourceUrl,
                                               this->region, this->credential);
        if (readLen != leftLen) {
            S3DEBUG("Failed to fetch expected data from S3");
            this->sharedError = true;
        } else {
            S3DEBUG("Got %" PRIu64 " bytes from S3", readLen);
        }
    } else {
        readLen = 0;  // Nothing to read, EOF
        S3DEBUG("Reached the end of file");
        this->eof = true;
    }

    this->status = ReadyToRead;
    pthread_cond_signal(&this->stat_cond);
    pthread_mutex_unlock(&this->stat_mutex);

    return (this->sharedError) ? -1 : readLen;
}

void* DownloadThreadFunc(void* data) {
    ChunkBuffer* buffer = static_cast<ChunkBuffer*>(data);
    uint64_t filledSize = 0;
    S3DEBUG("Downloading thread starts");
    do {
        if (QueryCancelPending) {
            S3INFO("Downloading thread is interrupted by GPDB");

            // error is shared between all chunks, so all chunks will stop.
            buffer->setError();

            // have to unlock ChunkBuffer::read in some certain conditions, for instance, status is
            // not ReadyToRead, and read() is waiting for signal stat_cond.
            buffer->setStatus(ReadyToRead);
            pthread_cond_signal(const_cast<pthread_cond_t*>(buffer->getStatCond()));

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

void S3KeyReader::open(const ReaderParams& params) {
    this->numOfChunks = params.getNumOfChunks();
    CHECK_OR_DIE_MSG(this->numOfChunks > 0, "%s", "numOfChunks must not be zero");

    this->region = params.getRegion();

    this->offsetMgr.setKeySize(params.getKeySize());
    this->offsetMgr.setChunkSize(params.getChunkSize());

    for (uint64_t i = 0; i < this->numOfChunks; i++) {
        // when vector reallocate memory, it will copy object.
        // chunkData must be initialized after all copy.
        this->chunkBuffers.push_back(ChunkBuffer(params.getKeyUrl(), this->offsetMgr,
                                                 this->sharedError, params.getCred(),
                                                 this->region));
    }

    for (uint64_t i = 0; i < this->numOfChunks; i++) {
        this->chunkBuffers[i].init();
        this->chunkBuffers[i].setS3interface(this->s3interface);

        pthread_t thread;
        pthread_create(&thread, NULL, DownloadThreadFunc, &this->chunkBuffers[i]);
        this->threads.push_back(thread);
    }

    return;
}

uint64_t S3KeyReader::read(char* buf, uint64_t count) {
    uint64_t fileLen = this->offsetMgr.getKeySize();
    uint64_t readLen = 0;

    do {
        // confirm there is no more available data, done with this file
        if (this->transferredKeyLen >= fileLen) {
            return 0;
        }

        ChunkBuffer& buffer = chunkBuffers[this->curReadingChunk % this->numOfChunks];

        readLen = buffer.read(buf, count);

        this->transferredKeyLen += readLen;

        if (readLen < count) {
            this->curReadingChunk++;
            CHECK_OR_DIE_MSG(!buffer.isError(), "%s", "Error occurs while downloading, skip");
        }

        // retry to confirm whether thread reading is finished or chunk size is
        // divisible by get()'s buffer size
    } while (readLen == 0);

    return readLen;
}

void S3KeyReader::close() {
    for (uint64_t i = 0; i < this->threads.size(); i++) {
        pthread_cancel(this->threads[i]);
        pthread_join(this->threads[i], NULL);
    }

    threads.clear();
    return;
}
