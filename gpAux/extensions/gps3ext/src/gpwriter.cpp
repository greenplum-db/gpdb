#include "gpwriter.h"

GPWriter::GPWriter(const S3Params& params, const string& url, string fmt)
    : format(fmt), s3service(params) {
    this->params = params;

    string replacedURL = S3UrlUtility::replaceSchemaFromURL(url, this->params.isEncryption());

    // construct a canonical URL string
    // schema://domain/uri_encoded_path/
    string encodedURL = uri_encode(replacedURL);
    find_replace(encodedURL, "%3A%2F%2F", "://");
    find_replace(encodedURL, "%2F", "/");

    constructS3Params(encodedURL);
    restfulServicePtr = &restfulService;
}

void GPWriter::constructS3Params(const string& url) {
    this->params.setBaseUrl(url);
    this->params.setRegion(S3UrlUtility::getRegionFromURL(url));
}

void GPWriter::open(const S3Params& params) {
    this->s3service.setRESTfulService(this->restfulServicePtr);
    this->params.setKeyUrl(this->genUniqueKeyName(this->params.getBaseUrl()));
    this->commonWriter.setS3service(&this->s3service);
    this->commonWriter.open(this->params);
}

uint64_t GPWriter::write(const char* buf, uint64_t count) {
    return this->commonWriter.write(buf, count);
}

void GPWriter::close() {
    this->commonWriter.close();
}

string GPWriter::genUniqueKeyName(const string& url) {
    string keyName;

    do {
        keyName = this->constructKeyName(url);
    } while (this->s3service.checkKeyExistence(keyName, this->params.getRegion()));

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

        S3Params params;
        if (!InitConfig(params, config_path, "default")) {
            return NULL;
        }

        CheckEssentialConfig(params);

        InitRemoteLog();

        string extName = params.isAutoCompress() ? string(format) + ".gz" : format;
        writer = new GPWriter(params, url, extName);
        if (writer == NULL) {
            return NULL;
        }

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
