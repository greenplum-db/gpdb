
#include "gpwriter.h"
#include "gpcommon.h"

#include "s3conf.h"
#include "s3log.h"
#include "s3macros.h"
#include "s3utils.h"

GPWriter::GPWriter(const string &url) {
    string file = replaceSchemaFromURL(url);

    constructWriterParam(file);
    restfulServicePtr = &restfulService;
}

GPWriter::~GPWriter() {
}

void GPWriter::constructWriterParam(const string &url) {
    this->params.setUrlToLoad(url);
    this->params.setSegId(s3ext_segid);
    this->params.setSegNum(s3ext_segnum);
    this->params.setNumOfChunks(s3ext_threadnum);
    this->params.setChunkSize(s3ext_chunksize);
    this->params.setRegion(getRegionFromURL(url));

    this->cred.accessID = s3ext_accessid;
    this->cred.secret = s3ext_secret;
    this->params.setCred(this->cred);
}

void GPWriter::open(const ReaderParams &params) {
    this->s3service.setRESTfulService(this->restfulServicePtr);
}

uint64_t GPWriter::write(char *buf, uint64_t count) {
    printf("GPWriter::write\n");
    vector<uint8_t> data;
    data.insert(data.end(), buf, buf + count);

    return this->s3service.uploadData(data, this->params.getUrlToLoad(), this->params.getRegion(),
                                      this->params.getCred());
}

void GPWriter::close() {
}

GPWriter *writer_init(const char *url_with_options) {
    GPWriter *gpwriter = NULL;

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

        gpwriter = new GPWriter(url);
        if (gpwriter == NULL) {
            return NULL;
        }

        ReaderParams param;
        gpwriter->open(param);
        return gpwriter;

    } catch (std::exception &e) {
        if (gpwriter != NULL) {
            delete gpwriter;
        }
        S3ERROR("writer_init caught an exception: %s", e.what());
        s3extErrorMessage = e.what();
        return NULL;
    }
}

bool writer_transfer_data(GPWriter *writer, char *data_buf, int &data_len) {
    try {
        if (!writer || !data_buf || (data_len < 0)) {
            return false;
        }

        if (data_len == 0) {
            return true;
        }

        uint64_t write_len = writer->write(data_buf, data_len);

        // sure read_len <= data_len here, hence truncation will never happen
        data_len = (int)write_len;
    } catch (std::exception &e) {
        S3ERROR("writer_transfer_data caught an exception: %s", e.what());
        s3extErrorMessage = e.what();
        return false;
    }

    return true;
}

bool writer_cleanup(GPWriter **writer) {
    bool result = true;
    try {
        if (*writer) {
            (*writer)->close();
            delete *writer;
            *writer = NULL;
        } else {
            result = false;
        }
    } catch (std::exception &e) {
        S3ERROR("writer_cleanup caught an exception: %s", e.what());
        s3extErrorMessage = e.what();
        result = false;
    }

    return result;
}
