#include <fcntl.h>
#include <openssl/crypto.h>
#include <pthread.h>
#include <sstream>
#include <string>

#include "gpcommon.h"
#include "gpreader.h"
#include "gpwriter.h"

#include "s3common.h"
#include "s3conf.h"
#include "s3log.h"
#include "s3macros.h"
#include "s3url.h"
#include "s3utils.h"

using std::string;
using std::stringstream;

GPWriter::GPWriter(const string& url, string fmt) : format(fmt) {
    string file = S3UrlUtility::replaceSchemaFromURL(url, s3ext_encryption);
    constructWriterParams(file);
    restfulServicePtr = &restfulService;
}

void GPWriter::constructWriterParams(const string& url) {
    this->params.setBaseUrl(url);
    this->params.setSegId(s3ext_segid);
    this->params.setSegNum(s3ext_segnum);
    this->params.setNumOfChunks(s3ext_threadnum);
    this->params.setChunkSize(s3ext_chunksize);
    this->params.setRegion(S3UrlUtility::getRegionFromURL(url));

    this->cred.accessID = s3ext_accessid;
    this->cred.secret = s3ext_secret;
    this->params.setCred(this->cred);
}

void GPWriter::open(const WriterParams& params) {
    this->s3service.setRESTfulService(this->restfulServicePtr);
    this->params.setKeyUrl(this->genUniqueKeyName(this->params.getBaseUrl()));
    this->keyWriter.setS3interface(&this->s3service);
    this->keyWriter.open(this->params);
}

uint64_t GPWriter::write(char* buf, uint64_t count) {
    return this->keyWriter.write(buf, count);
}

void GPWriter::close() {
    this->keyWriter.close();
}

string GPWriter::genUniqueKeyName(const string& url) {
    string keyName;

    do {
        keyName = this->constructKeyName(url);
    } while (this->s3service.checkKeyExistence(keyName, this->params.getRegion(),
                                               this->params.getCred()));

    return keyName;
}

string GPWriter::constructKeyName(const string& url) {
    int randomDevice = ::open("/dev/urandom", O_RDONLY);
    char randomData[32];
    size_t randomDataLen = 0;

    while (randomDataLen < sizeof(randomData)) {
        ssize_t result =
            ::read(randomDevice, randomData + randomDataLen, sizeof(randomData) - randomDataLen);
        if (result < 0) {
            break;
        }
        randomDataLen += result;
    }
    ::close(randomDevice);

    char out_hash_hex[SHA256_DIGEST_STRING_LENGTH];

    sha256_hex(randomData, out_hash_hex);

    stringstream ss;
    ss << url << s3ext_segid << out_hash_hex + SHA256_DIGEST_STRING_LENGTH - 8 - 1 << '.'
       << this->format;

    return ss.str();
}

// invoked by s3_export(), need to be exception safe
GPWriter* writer_init(const char* url_with_options, const char* format) {
    GPWriter* writer = NULL;
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

        writer = new GPWriter(url, format);
        if (writer == NULL) {
            return NULL;
        }

        WriterParams params;
        writer->open(params);
        return writer;

    } catch (std::exception& e) {
        if (writer != NULL) {
            delete writer;
        }
        S3ERROR("writer_init caught an exception: %s", e.what());
        s3extErrorMessage = e.what();
        return NULL;
    }
}

// invoked by s3_export(), need to be exception safe
bool writer_transfer_data(GPWriter* writer, char* data_buf, int data_len) {
    try {
        if (!writer || !data_buf || (data_len <= 0)) {
            return false;
        }

        uint64_t write_len = writer->write(data_buf, data_len);

        CHECK_OR_DIE_MSG(write_len == (uint64_t)data_len, "%s",
                         "Failed to upload the data completely.");
    } catch (std::exception& e) {
        S3ERROR("writer_transfer_data caught an exception: %s", e.what());
        s3extErrorMessage = e.what();
        return false;
    }

    return true;
}

// invoked by s3_export(), need to be exception safe
bool writer_cleanup(GPWriter** writer) {
    bool result = true;
    try {
        if (*writer) {
            (*writer)->close();
            delete *writer;
            *writer = NULL;
        } else {
            result = false;
        }
    } catch (std::exception& e) {
        S3ERROR("writer_cleanup caught an exception: %s", e.what());
        s3extErrorMessage = e.what();
        result = false;
    }

    return result;
}
