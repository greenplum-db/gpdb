#include "gpwriter.h"
#include "s3memory_mgmt.h"

GPWriter::GPWriter(const S3Params& params, string fmt)
    : format(fmt), params(params), restfulService(this->params), s3InterfaceService(this->params) {
    restfulServicePtr = &restfulService;
}

void GPWriter::open(const S3Params& params) {
    this->s3InterfaceService.setRESTfulService(this->restfulServicePtr);
    this->params = this->params.setPrefix(this->genUniqueKeyName(this->params.getS3Url()));
    this->commonWriter.setS3InterfaceService(&this->s3InterfaceService);
    this->commonWriter.open(this->params);
}

uint64_t GPWriter::write(const char* buf, uint64_t count) {
    return this->commonWriter.write(buf, count);
}

void GPWriter::close() {
    this->commonWriter.close();
}

string GPWriter::genUniqueKeyName(const S3Url& s3Url) {
    string fullUrl = s3Url.getFullUrlForCurl();

    while (true) {
        stringstream ss;
        ss << s3ext_segid << this->constructRandomStr() << "." << this->format;

        if (!this->s3InterfaceService.checkKeyExistence(
                S3Url((fullUrl + ss.str()), (s3Url.getSchema() == "https"), s3Url.getVersion(),
                      s3Url.getRegion()))) {
            string keyName = ss.str();
            return s3Url.getPrefix() + keyName;
        }
    }
}

string GPWriter::constructRandomStr() {
    int randomDevice = ::open("/dev/urandom", O_RDONLY);
    char randomData[32];
    size_t randomDataLen = 0;

    S3_CHECK_OR_DIE(randomDevice >= 0, S3RuntimeError, "failed to generate random number");

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

    sha256_hex(randomData, 32, out_hash_hex);

    return out_hash_hex + SHA256_DIGEST_STRING_LENGTH - 8 - 1;
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

        S3Params params = InitConfig(urlWithOptions);

        InitRemoteLog();

        // Prepare memory to be used for thread chunk buffer.
        PrepareS3MemContext(params);

        string extName = params.isAutoCompress() ? string(format) + ".gz" : format;
        writer = new GPWriter(params, extName);
        if (writer == NULL) {
            return NULL;
        }

        writer->open(params);
        return writer;
    } catch (S3Exception& e) {
        delete writer;
        s3extErrorMessage =
            "writer_init caught a " + e.getType() + " exception: " + e.getFullMessage();
        S3ERROR("writer_init caught %s: %s", e.getType().c_str(), s3extErrorMessage.c_str());
        return NULL;
    } catch (...) {
        delete writer;
        S3ERROR("Caught an unexpected exception.");
        s3extErrorMessage = "Caught an unexpected exception.";
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

        S3_CHECK_OR_DIE(write_len == (uint64_t)data_len, S3RuntimeError,
                        "Failed to upload the data completely.");
    } catch (S3Exception& e) {
        s3extErrorMessage =
            "writer_transfer_data caught a " + e.getType() + " exception: " + e.getFullMessage();
        S3ERROR("writer_transfer_data caught %s: %s", e.getType().c_str(),
                s3extErrorMessage.c_str());
        return false;
    } catch (...) {
        S3ERROR("Caught an unexpected exception.");
        s3extErrorMessage = "Caught an unexpected exception.";
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
    } catch (S3Exception& e) {
        s3extErrorMessage =
            "writer_cleanup caught a " + e.getType() + " exception: " + e.getFullMessage();
        S3ERROR("writer_cleanup caught %s: %s", e.getType().c_str(), s3extErrorMessage.c_str());
        result = false;
    } catch (...) {
        S3ERROR("Caught an unexpected exception.");
        s3extErrorMessage = "Caught an unexpected exception.";
        result = false;
    }

    return result;
}
