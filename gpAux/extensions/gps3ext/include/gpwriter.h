#ifndef INCLUDE_GPWRITER_H_
#define INCLUDE_GPWRITER_H_

#include <string.h>
#include <string>

#include "s3key_writer.h"
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
        return this->params.getKeyUrl();
    }

    void setKeyToUpload(const string &keyToUpload) {
        this->params.setKeyUrl(keyToUpload);
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

    S3KeyWriter keyWriter;

    // it links to itself by default
    // but the pointer here leaves a chance to mock it in unit test
    S3RESTfulService *restfulServicePtr;
};

// Following 3 functions are invoked by s3_export(), need to be exception safe
GPWriter *writer_init(const char *url_with_options);
bool writer_transfer_data(GPWriter *writer, char *data_buf, int &data_len);
bool writer_cleanup(GPWriter **writer);

#endif /* INCLUDE_GPWRITER_H_ */
