#ifndef INCLUDE_GPWRITER_H_
#define INCLUDE_GPWRITER_H_

#include <string.h>
#include "s3common.h"
#include "s3interface.h"
#include "s3restful_service.h"
#include "writer.h"

extern string s3extErrorMessage;

class GPWriter : public Writer {
   public:
    GPWriter(const string &url);
    virtual ~GPWriter();

    virtual void open(const ReaderParams &params);
    virtual uint64_t write(char *buf, uint64_t count);
    virtual void close();

   private:
    void constructWriterParam(const string &url);

    S3Service s3service;
    ReaderParams params;
    S3Credential cred;

    S3RESTfulService restfulService;

    // it links to itself by default
    // but the pointer here leaves a chance to mock it in unit test
    S3RESTfulService *restfulServicePtr;
};

// Following 3 functions are invoked by s3_export(), need to be exception safe
GPWriter *writer_init(const char *url_with_options);
bool writer_transfer_data(GPWriter *writer, char *data_buf, int &data_len);
bool writer_cleanup(GPWriter **writer);

// Set schema to 'https' or 'http'
inline string replaceSchemaFromURL(const string &url) {
    size_t iend = url.find("://");
    if (iend == string::npos) {
        return url;
    }
    return "https" + url.substr(iend);
}

inline string getRegionFromURL(const string &url) {
    size_t ibegin =
        url.find("://s3") + strlen("://s3");  // index of character('.' or '-') after "3"
    size_t iend = url.find(".amazonaws.com");

    string region;
    if (iend == string::npos) {
        return region;
    } else if (ibegin == iend) {  // "s3.amazonaws.com"
        return "external-1";
    } else {
        // ibegin + 1 is the character after "s3." or "s3-"
        // for instance: s3-us-west-2.amazonaws.com
        region = url.substr(ibegin + 1, iend - (ibegin + 1));
    }

    if (region.compare("us-east-1") == 0) {
        region = "external-1";
    }
    return region;
}
#endif /* INCLUDE_GPWRITER_H_ */
