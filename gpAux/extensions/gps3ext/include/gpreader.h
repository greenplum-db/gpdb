#ifndef __GP_READER__
#define __GP_READER__

#include <string>

#include "s3extbase.h"
#include "s3reader.h"

using std::string;

// S3Reader implements readable external table.
class S3Reader : public S3ExtBase {
   public:
    S3Reader(const string& url);
    virtual ~S3Reader();

    virtual bool Init(int segid, int segnum, int chunksize);
    virtual bool TransferData(char* data, uint64_t& len);
    virtual bool Destroy();

   protected:
    // Get URL for a S3 object/file.
    virtual string getKeyURL(const string& key);
    // Get downloading handler for next file.
    bool getNextDownloader();

    // private:
    unsigned int contentindex;
    // Downloader is repsonsible for downloading one file using multiple
    // threads.
    Downloader* filedownloader;
    // List of matched keys/files.
    ListBucketResult* keylist;
};

// Following 3 functions are invoked by s3_import(), need to be exception safe

S3Reader* reader_init(const char* url_with_options);

bool reader_transfer_data(S3Reader* reader, char* data_buf, int& data_len);

bool reader_cleanup(S3Reader** reader);

#endif
