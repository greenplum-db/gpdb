#include <openssl/crypto.h>
#include <pthread.h>
#include <sstream>
#include <string>

#include "gpcommon.h"
#include "gpreader.h"

#include "s3conf.h"
#include "s3log.h"
#include "s3macros.h"
#include "s3utils.h"

string s3extErrorMessage;

// Thread related functions, called only by gpreader and gpcheckcloud
#define MUTEX_TYPE pthread_mutex_t
#define MUTEX_SETUP(x) pthread_mutex_init(&(x), NULL)
#define MUTEX_CLEANUP(x) pthread_mutex_destroy(&(x))
#define MUTEX_LOCK(x) pthread_mutex_lock(&(x))
#define MUTEX_UNLOCK(x) pthread_mutex_unlock(&(x))
#define THREAD_ID pthread_self()

/* This array will store all of the mutexes available to OpenSSL. */
static MUTEX_TYPE* mutex_buf = NULL;

static void locking_function(int mode, int n, const char* file, int line) {
    if (mode & CRYPTO_LOCK) {
        MUTEX_LOCK(mutex_buf[n]);
    } else {
        MUTEX_UNLOCK(mutex_buf[n]);
    }
}

static unsigned long id_function(void) {
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
    CRYPTO_set_id_callback(id_function);
    CRYPTO_set_locking_callback(locking_function);
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
    delete mutex_buf;
    mutex_buf = NULL;
    return 1;
}

GPReader::GPReader(const string& url) {
    // construct a canonical URL string
    // schema://domain/uri_encoded_path/
    string encodedURL = uri_encode(url);
    find_replace(encodedURL, "%3A%2F%2F", "://");
    find_replace(encodedURL, "%2F", "/");

    constructReaderParams(encodedURL);
    restfulServicePtr = &restfulService;
}

void GPReader::constructReaderParams(const string& url) {
    this->params.setBaseUrl(url);
    this->params.setSegId(s3ext_segid);
    this->params.setSegNum(s3ext_segnum);
    this->params.setNumOfChunks(s3ext_threadnum);
    this->params.setChunkSize(s3ext_chunksize);

    this->cred.accessID = s3ext_accessid;
    this->cred.secret = s3ext_secret;
    this->params.setCred(this->cred);
}

void GPReader::open(const ReaderParams& params) {
    this->s3service.setRESTfulService(this->restfulServicePtr);
    this->bucketReader.setS3interface(&this->s3service);
    this->bucketReader.setUpstreamReader(&this->commonReader);
    this->commonReader.setS3service(&this->s3service);
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

void CheckEssentialConfig() {
    if (s3ext_accessid.empty()) {
        CHECK_OR_DIE_MSG(false, "%s", "\"FATAL: access id not set\"");
    }

    if (s3ext_secret.empty()) {
        CHECK_OR_DIE_MSG(false, "%s", "\"FATAL: secret id not set\"");
    }

    if ((s3ext_segnum == -1) || (s3ext_segid == -1)) {
        CHECK_OR_DIE_MSG(false, "%s", "\"FATAL: segment id is invalid\"");
    }
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
        string url = truncate_options(urlWithOptions);

        if (url.empty()) {
            return NULL;
        }

        string config_path = get_opt_s3(urlWithOptions, "config");
        if (config_path.empty()) {
            S3ERROR("The 'config' parameter is not provided, use default value 's3/s3.conf'.");
            config_path = "s3/s3.conf";
        }

        if (!InitConfig(config_path, "default")) {
            return NULL;
        }

        CheckEssentialConfig();

        InitRemoteLog();

        reader = new GPReader(url);
        if (reader == NULL) {
            return NULL;
        }

        ReaderParams params;
        reader->open(params);
        return reader;

    } catch (std::exception& e) {
        if (reader != NULL) {
            delete reader;
        }

        if (S3QueryIsAbortInProgress()) {
            S3ERROR("reader_init caught an exception: %s", "Downloading is interrupted by user");
            s3extErrorMessage = "Downloading is interrupted by user";
        } else {
            S3ERROR("reader_init caught an exception: %s", e.what());
            s3extErrorMessage = e.what();
        }

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
    } catch (std::exception& e) {
        if (S3QueryIsAbortInProgress()) {
            S3ERROR("reader_transfer_data caught an exception: %s",
                    "Downloading is interrupted by user");
            s3extErrorMessage = "Downloading is interrupted by user";
        } else {
            S3ERROR("reader_transfer_data caught an exception: %s", e.what());
            s3extErrorMessage = e.what();
        }

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
    } catch (std::exception& e) {
        if (S3QueryIsAbortInProgress()) {
            S3ERROR("reader_cleanup caught an exception: %s", "Downloading is interrupted by user");
            s3extErrorMessage = "Downloading is interrupted by user";
        } else {
            S3ERROR("reader_cleanup caught an exception: %s", e.what());
            s3extErrorMessage = e.what();
        }

        result = false;
    }

    return result;
}
