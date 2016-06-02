#include <sstream>
#include <string>

#include "gpreader.h"
#include "gps3ext.h"
#include "s3conf.h"
#include "s3log.h"
#include "s3macros.h"
#include "s3utils.h"

using std::string;
using std::stringstream;

S3Reader::S3Reader(const string &url) : S3ExtBase(url) {
    this->contentindex = -1;
    this->filedownloader = NULL;
    this->keylist = NULL;
}

S3Reader::~S3Reader() {}

bool S3Reader::Init(int segid, int segnum, int chunksize) {
    // set segment id and num
    this->segid = s3ext_segid;
    this->segnum = s3ext_segnum;
    this->contentindex = this->segid;

    this->chunksize = chunksize;

    // validate url first
    if (!this->ValidateURL()) {
        S3ERROR("The given URL(%s) is invalid", this->url.c_str());
        return false;
    }

    int initretry = 3;
    while (initretry--) {
        // TODO: refactor, ListBucket() here.
        this->keylist = NULL;

        if (!this->keylist) {
            S3INFO("Can't get keylist from bucket %s", this->bucket.c_str());
            if (initretry) {
                S3INFO("Retrying");
                continue;
            } else {
                S3ERROR("Quit initialization because ListBucket keeps failing");
                return false;
            }
        }

        if (this->keylist->contents.size() == 0) {
            S3INFO("Keylist of bucket is empty");
            if (initretry) {
                S3INFO("Retry listing bucket");
                delete this->keylist;
                this->keylist = NULL;
                continue;
            } else {
                S3ERROR("Quit initialization because keylist is empty");
                return false;
            }
        }
        break;
    }
    S3INFO("Got %d files to download", this->keylist->contents.size());
    if (!this->getNextDownloader()) {
        return false;
    }

    return true;
}

// Start a downloader for next file (s3 key).
bool S3Reader::getNextDownloader() {
    // 1. delete previous downloader if exists.
    if (this->filedownloader) {
        filedownloader->destroy();
        delete this->filedownloader;
        this->filedownloader = NULL;
    }

    if (this->contentindex >= this->keylist->contents.size()) {
        S3DEBUG("No more files to download");
        return true;
    }

    // 2. construct a new downloader with argument: concurrent_num
    this->filedownloader = new Downloader(this->concurrent_num);
    CHECK_OR_DIE_MSG(this->filedownloader != NULL, "%s", "Failed to construct filedownloader.");

    // 3. Get KeyURL which downloader will download from S3.
    BucketContent *c = this->keylist->contents[this->contentindex];
    string keyurl = this->getKeyURL(c->getName());
    S3DEBUG("key: %s, size: %llu", keyurl.c_str(), c->getSize());

    // 4. Initialize and kick off Downloader.
    bool ok =
        filedownloader->init(keyurl, this->region, c->getSize(), this->chunksize, &this->cred);
    if (ok) {
        // for now, every segment downloads its assigned files(mod)
        // better to build a workqueue in case not all segments are available
        this->contentindex += this->segnum;
    } else {
        delete this->filedownloader;
        this->filedownloader = NULL;
        return false;
    }

    return true;
}

string S3Reader::getKeyURL(const string &key) {
    stringstream sstr;
    sstr << this->schema << "://"
         << "s3-" << this->region << ".amazonaws.com/";
    sstr << this->bucket << "/" << key;
    return sstr.str();
}

// Read data from downloader
bool S3Reader::TransferData(char *data, uint64_t &len) {
    if (this->filedownloader == NULL) {
        S3INFO("No files to download, exit");
        len = 0;
        return true;
    }

RETRY:
    uint64_t buflen = len;
    bool ok = this->filedownloader->get(data, buflen);
    if (!ok) {
        S3ERROR("Failed to get data from file downloader");
        return false;
    }
    if (buflen == 0) {
        if (!this->getNextDownloader()) {
            return false;
        }

        if (this->filedownloader != NULL) {
            S3INFO("Time to download new file");
            goto RETRY;
        }
    }
    len = buflen;

    return true;
}

bool S3Reader::Destroy() {
    if (this->filedownloader) {
        this->filedownloader->destroy();
        delete this->filedownloader;
        this->filedownloader = NULL;
    }

    if (this->keylist) {
        delete this->keylist;
        this->keylist = NULL;
    }

    return true;
}

// invoked by s3_import(), need to be exception safe
S3Reader *reader_init(const char *url_with_options) {
    try {
        if (!url_with_options) {
            return NULL;
        }

        curl_global_init(CURL_GLOBAL_ALL);

        char *url = truncate_options(url_with_options);
        if (!url) {
            return NULL;
        }

        char *config_path = get_opt_s3(url_with_options, "config");
        if (!config_path) {
            // no config path in url, use default value
            // data_folder/gpseg0/s3/s3.conf
            config_path = strdup("s3/s3.conf");
        }

        bool result = InitConfig(config_path, "default");
        if (!result) {
            free(url);
            free(config_path);
            return NULL;
        } else {
            free(config_path);
        }

        InitRemoteLog();

        S3Reader *reader = new S3Reader(url);

        free(url);

        if (!reader) {
            return NULL;
        }

        if (!reader->Init(s3ext_segid, s3ext_segnum, s3ext_chunksize)) {
            reader->Destroy();
            delete reader;
            return NULL;
        }

        return reader;
    } catch (...) {
        S3ERROR("Caught an exception, aborting");
        return NULL;
    }
}

// invoked by s3_import(), need to be exception safe
bool reader_transfer_data(S3Reader *reader, char *data_buf, int &data_len) {
    try {
        if (!reader || !data_buf || (data_len < 0)) {
            return false;
        }

        if (data_len == 0) {
            return true;
        }

        uint64_t read_len = data_len;
        if (!reader->TransferData(data_buf, read_len)) {
            return false;
        }

        // sure read_len <= data_len here, hence truncation will never happen
        data_len = (int)read_len;
    } catch (...) {
        S3ERROR("Caught an exception, aborting");
        return false;
    }

    return true;
}

// invoked by s3_import(), need to be exception safe
bool reader_cleanup(S3Reader **reader) {
    try {
        if (*reader) {
            if (!(*reader)->Destroy()) {
                delete *reader;
                *reader = NULL;
                return false;
            }

            delete *reader;
            *reader = NULL;
        } else {
            return false;
        }

        // Cleanup function for the XML library.
        xmlCleanupParser();
    } catch (...) {
        S3ERROR("Caught an exception, aborting");
        return false;
    }

    return true;
}
