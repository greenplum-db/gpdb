#ifndef __GP_READER__
#define __GP_READER__

#include <string>

#include "s3bucket_reader.h"
#include "s3common_reader.h"
#include "s3interface.h"
#include "s3key_reader.h"

class GPReader : public Reader {
   public:
    GPReader(const string &url);
    virtual ~GPReader() {
    }

    virtual void open(const ReaderParams &params);

    // read() attempts to read up to count bytes into the buffer starting at
    // buffer.
    // Return 0 if EOF. Throw exception if encounters errors.
    virtual uint64_t read(char *buf, uint64_t count);

    // This should be reentrant, has no side effects when called multiple times.
    virtual void close();

   private:
    void constructReaderParam(const string &url);

   protected:
    S3BucketReader bucketReader;
    S3CommonReader commonReader;
    DecompressReader uncomressReader;
    S3RESTfulService restfulService;

    S3Service s3service;
    ReaderParams params;
    S3Credential cred;

    // it links to itself by default
    // but the pointer here leaves a chance to mock it in unit test
    S3RESTfulService *restfulServicePtr;
};

// Following 3 functions are invoked by s3_import(), need to be exception safe
GPReader *reader_init(const char *url_with_options);
bool reader_transfer_data(GPReader *reader, char *data_buf, int &data_len);
bool reader_cleanup(GPReader **reader);

// Two thread related functions, called only by gpreader and gpcheckcloud
int thread_setup(void);
int thread_cleanup(void);

#endif
