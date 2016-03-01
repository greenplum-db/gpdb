#include "S3ExtWrapper.h"
#include "utils.h"
#include "gps3conf.h"
#include "gps3ext.h"
#include "S3Log.h"

#include <string>
#include <sstream>
using std::stringstream;
using std::string;

// invoked by s3_import(), need to be exception safe
S3ExtBase *CreateExtWrapper(const char *url) {
    try {
        S3ExtBase *ret = new S3Reader(url);
        return ret;
    } catch (...) {
        return NULL;
    }
}

S3ExtBase::S3ExtBase(string url) {
    this->url = url;

    // get following from config
    this->cred.secret = s3ext_secret;
    this->cred.keyid = s3ext_accessid;

    this->segid = s3ext_segid;
    this->segnum = s3ext_segnum;

    this->chunksize = s3ext_chunksize;
    this->concurrent_num = s3ext_threadnum;

    S3INFO("Created %d threads for downloading", s3ext_threadnum);
    S3INFO("File is splited to %d each", s3ext_chunksize);
}

S3ExtBase::~S3ExtBase() {}

S3Reader::~S3Reader() {}

S3Reader::S3Reader(string url) : S3ExtBase(url) {
    this->contentindex = -1;
    this->filedownloader = NULL;
    this->keylist = NULL;
}

// invoked by s3_import(), need to be exception safe
bool S3Reader::Init(int segid, int segnum, int chunksize) {
    try {
        // set segment id and num
        this->segid = s3ext_segid;
        this->segnum = s3ext_segnum;
        this->contentindex = this->segid;

        this->chunksize = chunksize;

        // Validate url first
        if (!this->ValidateURL()) {
            S3ERROR("The given URL(%s) is invalid", this->url.c_str());
            return false;
        }

        // TODO: As separated function for generating url
        stringstream sstr;
        sstr << "s3-" << this->region << ".amazonaws.com";
        S3DEBUG("Host url is %s", sstr.str().c_str());
        int initretry = 3;
        while (initretry--) {
            this->keylist = ListBucket(this->schema.c_str(), sstr.str().c_str(),
                                       this->bucket.c_str(),
                                       this->prefix.c_str(), this->cred);

            if (!this->keylist) {
                S3INFO("Can't get keylist from bucket %s",
                       this->bucket.c_str());
                if (initretry) {
                    S3INFO("Retrying");
                    continue;
                } else {
                    S3ERROR(
                        "Quit initialization because ListBucket keeps failing");
                    return false;
                }
            }

            if (this->keylist->contents.size() == 0) {
                S3INFO("Keylist of bucket is empty");
                if (initretry) {
                    S3INFO("Retry listing bucket");
                    continue;
                } else {
                    S3ERROR("Quit initialization because keylist is empty");
                    return false;
                }
            }
            break;
        }
        S3INFO("Got %d files to download", this->keylist->contents.size());
        this->getNextDownloader();
    } catch (...) {
        return false;
    }

    // return this->filedownloader ? true : false;
    return true;
}

void S3Reader::getNextDownloader() {
    if (this->filedownloader) {  // reset old downloader
        filedownloader->destroy();
        delete this->filedownloader;
        this->filedownloader = NULL;
    }

    if (this->contentindex >= this->keylist->contents.size()) {
        S3DEBUG("No more files to download");
        return;
    }
    this->filedownloader = new Downloader(this->concurrent_num);

    if (!this->filedownloader) {
        S3ERROR("Failed to create filedownloader");
        return;
    }
    BucketContent *c = this->keylist->contents[this->contentindex];
    string keyurl = this->getKeyURL(c->Key());
    S3DEBUG("key: %s, size: %llu", keyurl.c_str(), c->Size());

    if (!filedownloader->init(keyurl, c->Size(), this->chunksize,
                              &this->cred)) {
        delete this->filedownloader;
        this->filedownloader = NULL;
    } else {  // move to next file
        // for now, every segment downloads its assigned files(mod)
        // better to build a workqueue in case not all segments are available
        this->contentindex += this->segnum;
    }
    return;
}

string S3Reader::getKeyURL(const string key) {
    stringstream sstr;
    sstr << this->schema << "://"
         << "s3-" << this->region << ".amazonaws.com/";
    sstr << this->bucket << "/" << key;
    return sstr.str();
}

// invoked by s3_import(), need to be exception safe
bool S3Reader::TransferData(char *data, uint64_t &len) {
    try {
        if (!this->filedownloader) {
            S3INFO("No files to download, exit");
            // not initialized?
            len = 0;
            return true;
        }
        uint64_t buflen;
    RETRY:
        buflen = len;
        // S3DEBUG("getlen is %d", len);
        bool result = filedownloader->get(data, buflen);
        if (!result) {  // read fail
            S3ERROR("Failed to get data from filedownloader");
            return false;
        }
        // S3DEBUG("getlen is %lld", buflen);
        if (buflen == 0) {
            // change to next downloader
            this->getNextDownloader();
            if (this->filedownloader) {  // download next file
                S3INFO("Time to download new file");
                goto RETRY;
            }
        }
        len = buflen;
    } catch (...) {
        return false;
    }

    return true;
}

// invoked by s3_import(), need to be exception safe
bool S3Reader::Destroy() {
    try {
        // reset filedownloader
        if (this->filedownloader) {
            this->filedownloader->destroy();
            delete this->filedownloader;
        }

        // Free keylist
        if (this->keylist) {
            delete this->keylist;
        }
    } catch (...) {
        return false;
    }

    return true;
}

bool S3ExtBase::ValidateURL() {
    // TODO http://docs.aws.amazon.com/general/latest/gr/rande.html#s3_region
    // s3.eu-central-1.amazonaws.com -> s3-eu-central-1.amazonaws.com
    // s3.ap-northeast-2.amazonaws.com -> s3-ap-northeast-2.amazonaws.com

    const char *awsdomain = ".amazonaws.com";
    int ibegin = 0;
    int iend = url.find("://");
    if (iend == string::npos) {  // -1
        // error
        return false;
    }

    this->schema = url.substr(ibegin, iend);
    if (this->schema == "s3") {
        if (s3ext_encryption)
            this->schema = "https";
        else
            this->schema = "http";
    }

    ibegin = url.find("-");
    iend = url.find(awsdomain);

    if (iend == string::npos) {
        return false;
    } else if (ibegin == string::npos) {
        this->region = "external-1";
    } else {
        this->region = url.substr(ibegin + 1, iend - ibegin - 1);
    }

    if (this->region.compare("us-east-1") == 0) {
        this->region = "external-1";
    }

    ibegin = find_Nth(url, 3, "/");
    iend = find_Nth(url, 4, "/");
    if ((iend == string::npos) || (ibegin == string::npos)) {
        return false;
    }
    this->bucket = url.substr(ibegin + 1, iend - ibegin - 1);

    this->prefix = url.substr(iend + 1, url.length() - iend - 1);

    /*
    if (url.back() != '/') {
        return false;
    }
    */
    return true;
}

/*
bool S3Protocol_t::Write(char *data, size_t &len) {
    if (!this->fileuploader) {
        // not initialized?
        return false;
    }

    bool result = fileuploader->write(data, len);
    if (!result) {
        S3DEBUG("Failed to write data via fileuploader");
        return false;
    }

    return true;
}
*/
