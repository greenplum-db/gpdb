#ifndef INCLUDE_GPWRITER_H_
#define INCLUDE_GPWRITER_H_

#include <string.h>
#include <string>

#include "writer.h"

class GPWriter : public Writer {
   public:
    GPWriter(const string &url);
    virtual ~GPWriter() {
    }

    virtual void open(const WriterParams &params);

    // write() attempts to write up to count bytes from the buffer.
    // Always return 0 if EOF, no matter how many times it's invoked. Throw exception if encounters
    // errors.
    virtual uint64_t write(char *buf, uint64_t count);

    // This should be reentrant, has no side effects when called multiple times.
    virtual void close();

    const string &getKeyToUpload() const {
        return keyToUpload;
    }

    void setKeyToUpload(const string &keyToUpload) {
        this->keyToUpload = keyToUpload;
    }

   private:
    void constructWriterParams(const string &url);
    string constructKeyName(const string &url);
    string genUniqueKeyName(const string &url);

   protected:
    WriterParams params;
    S3RESTfulService restfulService;
    S3Service s3service;
    S3Credential cred;
    string keyToUpload;

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
