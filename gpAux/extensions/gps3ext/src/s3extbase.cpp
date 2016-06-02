#include <sstream>
#include <string>

#include "gps3ext.h"
#include "s3conf.h"
#include "s3extbase.h"
#include "s3log.h"
#include "s3utils.h"

using std::string;
using std::stringstream;

S3ExtBase::S3ExtBase(const string &url) {
    this->url = url;

    // TODO: move into config
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

// Set schema to 'https' or 'http'
void S3ExtBase::SetSchema() {
    size_t iend = this->url.find("://");
    if (iend == string::npos) {
        return;
    }

    this->schema = this->url.substr(0, iend);
    if (this->schema == "s3") {
        this->schema = s3ext_encryption ? "https" : "http";
    }
}

// Set AWS region, use 'external-1' if it is 'us-east-1' or not present
// http://docs.aws.amazon.com/general/latest/gr/rande.html#s3_region
void S3ExtBase::SetRegion() {
    size_t ibegin =
        this->url.find("://s3") + strlen("://s3");  // index of character('.' or '-') after "3"
    size_t iend = this->url.find(".amazonaws.com");

    if (iend == string::npos) {
        return;
    } else if (ibegin == iend) {  // "s3.amazonaws.com"
        this->region = "external-1";
    } else {
        // ibegin + 1 is the character after "s3." or "s3-"
        // for case: s3-us-west-2.amazonaws.com
        this->region = this->url.substr(ibegin + 1, iend - (ibegin + 1));
    }

    if (this->region.compare("us-east-1") == 0) {
        this->region = "external-1";
    }
}

void S3ExtBase::SetBucketAndPrefix() {
    size_t ibegin = find_Nth(this->url, 3, "/");
    size_t iend = find_Nth(this->url, 4, "/");

    if (ibegin == string::npos) {
        return;
    }
    // s3://s3-region.amazonaws.com/bucket
    if (iend == string::npos) {
        this->bucket = url.substr(ibegin + 1, url.length() - ibegin);
        this->prefix = "";
        return;
    }

    this->bucket = url.substr(ibegin + 1, iend - ibegin - 1);

    // s3://s3-region.amazonaws.com/bucket/
    if (iend == url.length()) {
        this->prefix = "";
        return;
    }

    ibegin = find_Nth(url, 4, "/");
    // s3://s3-region.amazonaws.com/bucket/prefix
    // s3://s3-region.amazonaws.com/bucket/prefix/whatever
    this->prefix = url.substr(ibegin + 1, url.length() - ibegin - 1);
}

bool S3ExtBase::ValidateURL() {
    this->SetSchema();
    this->SetRegion();
    this->SetBucketAndPrefix();

    return !(this->schema.empty() || this->region.empty() || this->bucket.empty());
}
