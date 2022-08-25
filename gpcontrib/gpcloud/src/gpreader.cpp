#include "gpreader.h"
#include "s3memory_mgmt.h"

// Thread related functions, called only by gpreader and gpcheckcloud
#define MUTEX_TYPE pthread_mutex_t
#define MUTEX_SETUP(x) pthread_mutex_init(&(x), NULL)
#define MUTEX_CLEANUP(x) pthread_mutex_destroy(&(x))
#define MUTEX_LOCK(x) pthread_mutex_lock(&(x))
#define MUTEX_UNLOCK(x) pthread_mutex_unlock(&(x))
#define THREAD_ID pthread_self()

/* This array will store all of the mutexes available to OpenSSL. */
static MUTEX_TYPE* mutex_buf = NULL;

// These functions are not used outside this file. However, they are unused
// when building with OpenSSL 1.1.0 and above, where CRYPTO_set_id_callback
// and CRYPTO_set_locking_callbacks are no-ops. To avoid compiler warnings
// about unused functions, don't mark them as 'static'.
void gpcloud_locking_function(int mode, int n, const char* file, int line);
unsigned long gpcloud_id_function(void);

void gpcloud_locking_function(int mode, int n, const char* file, int line) {
    if (mode & CRYPTO_LOCK) {
        MUTEX_LOCK(mutex_buf[n]);
    } else {
        MUTEX_UNLOCK(mutex_buf[n]);
    }
}

unsigned long gpcloud_id_function(void) {
    return ((unsigned long)THREAD_ID);
}

int thread_setup(void) {
    mutex_buf = new pthread_mutex_t[CRYPTO_num_locks()];
    if (mutex_buf == NULL) {
        return 0;
    }
    for (int i = 0; i < CRYPTO_num_locks(); i++) {
        MUTEX_SETUP(mutex_buf[i]);
    }
    CRYPTO_set_id_callback(gpcloud_id_function);
    CRYPTO_set_locking_callback(gpcloud_locking_function);
    return 1;
}

int thread_cleanup(void) {
    if (mutex_buf == NULL) {
        return 0;
    }
    CRYPTO_set_id_callback(NULL);
    CRYPTO_set_locking_callback(NULL);
    for (int i = 0; i < CRYPTO_num_locks(); i++) {
        MUTEX_CLEANUP(mutex_buf[i]);
    }
    delete[] mutex_buf;
    mutex_buf = NULL;
    return 1;
}

GPReader::GPReader(const S3Params& params)
    : params(params), restfulService(this->params), s3InterfaceService(this->params) {
    restfulServicePtr = &restfulService;
}

void GPReader::open(const S3Params& params) {
    this->s3InterfaceService.setRESTfulService(this->restfulServicePtr);
    this->bucketReader.setS3InterfaceService(&this->s3InterfaceService);
    this->bucketReader.setUpstreamReader(&this->commonReader);
    this->commonReader.setS3InterfaceService(&this->s3InterfaceService);
    this->bucketReader.open(this->params);
}

// read() attempts to read up to count bytes into the buffer.
// Return 0 if EOF. Throw exception if encounters errors.
uint64_t GPReader::read(char* buf, uint64_t count) {
    return this->bucketReader.read(buf, count);
}

// This should be reentrant, has no side effects when called multiple times.
void GPReader::close() {
    this->bucketReader.close();
}

// invoked by s3_import(), need to be exception safe
GPReader* reader_init(const char* url_with_options) {
    GPReader* reader = NULL;
    s3extErrorMessage.clear();

    try {
        if (!url_with_options) {
            return NULL;
        }

        string urlWithOptions(url_with_options);

        S3Params params = InitConfig(urlWithOptions);

        InitRemoteLog();

        // Prepare memory to be used for thread chunk buffer.
        PrepareS3MemContext(params);

        reader = new GPReader(params);
        if (reader == NULL) {
            return NULL;
        }

        reader->open(params);
        return reader;
    } catch (S3Exception& e) {
        delete reader;
        s3extErrorMessage =
            "reader_init caught a " + e.getType() + " exception: " + e.getFullMessage();
        S3ERROR("reader_init caught %s: %s", e.getType().c_str(), s3extErrorMessage.c_str());
        return NULL;
    } catch (...) {
        delete reader;
        S3ERROR("Caught an unexpected exception.");
        s3extErrorMessage = "Caught an unexpected exception.";
        return NULL;
    }
}

// invoked by s3_import(), need to be exception safe
bool reader_transfer_data(GPReader* reader, char* data_buf, int& data_len) {
    try {
        if (!reader || !data_buf || (data_len <= 0)) {
            return false;
        }

        uint64_t read_len = reader->read(data_buf, data_len);

        // sure read_len <= data_len here, hence truncation will never happen
        data_len = (int)read_len;
    } catch (S3Exception& e) {
        s3extErrorMessage =
            "reader_transfer_data caught a " + e.getType() + " exception: " + e.getFullMessage();
        S3ERROR("reader_transfer_data caught %s: %s", e.getType().c_str(),
                s3extErrorMessage.c_str());
        return false;
    } catch (...) {
        S3ERROR("Caught an unexpected exception.");
        s3extErrorMessage = "Caught an unexpected exception.";
        return false;
    }

    return true;
}

// invoked by s3_import(), need to be exception safe
bool reader_cleanup(GPReader** reader) {
    bool result = true;
    try {
        if (*reader) {
            (*reader)->close();
            delete *reader;
            *reader = NULL;
        } else {
            result = false;
        }
    } catch (S3Exception& e) {
        s3extErrorMessage =
            "reader_cleanup caught a " + e.getType() + " exception: " + e.getFullMessage();
        S3ERROR("reader_cleanup caught %s: %s", e.getType().c_str(), s3extErrorMessage.c_str());
        result = false;
    } catch (...) {
        S3ERROR("Caught an unexpected exception.");
        s3extErrorMessage = "Caught an unexpected exception.";
        result = false;
    }

    return result;
}
