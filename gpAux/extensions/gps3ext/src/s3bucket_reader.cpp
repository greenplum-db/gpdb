#include <sstream>
#include <string>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "reader.h"
#include "s3bucket_reader.h"
#include "s3common.h"
#include "s3conf.h"
#include "s3log.h"
#include "s3macros.h"
#include "s3params.h"
#include "s3url.h"
#include "s3utils.h"

using std::string;
using std::stringstream;

S3BucketReader::S3BucketReader() : Reader() {
    this->keyIndex = -1;

    this->s3interface = NULL;
    this->upstreamReader = NULL;

    this->numOfChunks = 0;
    this->chunkSize = -1;

    this->segId = -1;
    this->segNum = -1;

    this->needNewReader = true;
}

S3BucketReader::~S3BucketReader() {
    this->close();
}

void S3BucketReader::open(const ReaderParams &params) {
    this->url = params.getBaseUrl();
    this->segId = params.getSegId();
    this->segNum = params.getSegNum();
    this->cred = params.getCred();
    this->chunkSize = params.getChunkSize();
    this->numOfChunks = params.getNumOfChunks();

    this->parseURL();

    CHECK_OR_DIE(this->s3interface != NULL);

    this->keyList = this->s3interface->listBucket(this->schema, this->region, this->bucket,
                                                  this->prefix, this->cred);

    return;
}

BucketContent *S3BucketReader::getNextKey() {
    this->keyIndex = (this->keyIndex == (uint64_t)-1) ? this->segId : this->keyIndex + this->segNum;

    if (this->keyIndex >= this->keyList.contents.size()) {
        return NULL;
    }

    return &this->keyList.contents[this->keyIndex];
}

ReaderParams S3BucketReader::getReaderParams(BucketContent *key) {
    ReaderParams params = ReaderParams();
    params.setKeyUrl(this->getKeyURL(key->getName()));
    params.setRegion(this->region);
    params.setKeySize(key->getSize());
    params.setChunkSize(this->chunkSize);
    params.setNumOfChunks(this->numOfChunks);
    params.setCred(this->cred);

    S3DEBUG("key: %s, size: %" PRIu64, params.getKeyUrl().c_str(), params.getKeySize());
    return params;
}

uint64_t S3BucketReader::read(char *buf, uint64_t count) {
    CHECK_OR_DIE(this->upstreamReader != NULL);

    while (true) {
        if (this->needNewReader) {
            BucketContent *key = this->getNextKey();
            if (key == NULL) {
                S3DEBUG("Read finished for segment: %d", this->segId);
                return 0;
            }

            this->upstreamReader->open(getReaderParams(key));
            this->needNewReader = false;
        }

        uint64_t readCount = this->upstreamReader->read(buf, count);

        if (readCount != 0) {
            return readCount;
        }

        // Finished one file, continue to next
        this->upstreamReader->close();
        this->needNewReader = true;
    }
}

void S3BucketReader::close() {
    if (!this->keyList.contents.empty()) {
        this->keyList.contents.clear();
    }

    return;
}

string S3BucketReader::getKeyURL(const string &key) {
    stringstream sstr;
    sstr << this->schema << "://"
         << "s3-" << this->region << ".amazonaws.com/";
    sstr << this->bucket << "/" << key;
    return sstr.str();
}

void S3BucketReader::parseURL() {
    this->schema = s3ext_encryption ? "https" : "http";
    this->region = S3UrlUtility::getRegionFromURL(this->url);
    this->bucket = S3UrlUtility::getBucketFromURL(this->url);
    this->prefix = S3UrlUtility::getPrefixFromURL(this->url);

    bool ok = !(this->schema.empty() || this->region.empty() || this->bucket.empty());
    CHECK_OR_DIE_MSG(ok, "'%s' is not valid", this->url.c_str());
}
