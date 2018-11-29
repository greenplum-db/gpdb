#ifndef INCLUDE_GPWRITER_H_
#define INCLUDE_GPWRITER_H_

#include "s3common_headers.h"
#include "s3common_writer.h"
#include "s3interface.h"
#include "s3log.h"
#include "s3macros.h"
#include "s3utils.h"
#include "writer.h"

#define S3_DEFAULT_FORMAT "data"

class GPWriter : public Writer {
   public:
    GPWriter(const S3Params &params, string fmt = S3_DEFAULT_FORMAT);
    virtual ~GPWriter() {
        this->close();
    }

    virtual void open(const S3Params &params);

    // write() attempts to write up to count bytes from the buffer.
    // Always return 0 if EOF, no matter how many times it's invoked. Throw exception if encounters
    // errors.
    virtual uint64_t write(const char *buf, uint64_t count);

    // This should be reentrant, has no side effects when called multiple times.
    virtual void close();

    string getKeyUrlToUpload() {
        return this->params.getS3Url().getFullUrlForCurl();
    }

   private:
    string constructRandomStr();
    string genUniqueKeyName(const S3Url &s3Url);

   protected:
    string format;
    S3Params params;
    S3RESTfulService restfulService;
    S3InterfaceService s3InterfaceService;
    S3CommonWriter commonWriter;

    // it links to itself by default
    // but the pointer here leaves a chance to mock it in unit test
    S3RESTfulService *restfulServicePtr;
};

// Following 3 functions are invoked by s3_export(), need to be exception safe
GPWriter *writer_init(const char *url_with_options, const char *format = S3_DEFAULT_FORMAT);
bool writer_transfer_data(GPWriter *writer, char *data_buf, int data_len);
bool writer_cleanup(GPWriter **writer);

#endif /* INCLUDE_GPWRITER_H_ */
