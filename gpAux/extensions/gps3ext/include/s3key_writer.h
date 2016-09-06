#ifndef INCLUDE_S3KEY_WRITER_H_
#define INCLUDE_S3KEY_WRITER_H_

#include "s3common_headers.h"
#include "s3interface.h"
#include "writer.h"

class WriterBuffer : public vector<uint8_t> {};

class S3KeyWriter : public Writer {
   public:
    S3KeyWriter() : s3interface(NULL), partNumber(0), activeThreads(0) {
        pthread_mutex_init(&this->mutex, NULL);
        pthread_cond_init(&this->cv, NULL);
    }
    virtual ~S3KeyWriter() {
        this->close();
        pthread_mutex_destroy(&this->mutex);
        pthread_cond_destroy(&this->cv);
    }
    virtual void open(const S3Params& params);

    // write() attempts to write up to count bytes from the buffer.
    // Always return 0 if EOF, no matter how many times it's invoked. Throw exception if encounters
    // errors.
    virtual uint64_t write(const char* buf, uint64_t count);

    // This should be reentrant, has no side effects when called multiple times.
    virtual void close();

    void setS3InterfaceService(S3Interface* s3) {
        this->s3interface = s3;
    }

   protected:
    static void* UploadThreadFunc(void* p);

    void flushBuffer();
    void completeKeyWriting();
    void checkQueryCancelSignal();

    WriterBuffer buffer;
    S3Interface* s3interface;

    string uploadId;
    map<uint64_t, string> etagList;

    vector<pthread_t> threadList;
    pthread_mutex_t mutex;
    pthread_cond_t cv;
    uint64_t partNumber;
    uint64_t activeThreads;

    S3Params params;
};

class UniqueLock {
   public:
    UniqueLock(pthread_mutex_t* m) : mutex(m) {
        pthread_mutex_lock(this->mutex);
    }
    ~UniqueLock() {
        pthread_mutex_unlock(this->mutex);
    }

   private:
    pthread_mutex_t* mutex;
};

#endif /* INCLUDE_S3KEY_WRITER_H_ */
