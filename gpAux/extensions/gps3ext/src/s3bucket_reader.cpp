#include "s3bucket_reader.h"

S3BucketReader::S3BucketReader() : Reader() {
    this->keyIndex = s3ext_segid;

    this->s3interface = NULL;
    this->upstreamReader = NULL;

    this->needNewReader = true;
}

S3BucketReader::~S3BucketReader() {
    this->close();
}

void S3BucketReader::open(const S3Params& params) {
    this->params = params;

    S3_CHECK_OR_DIE_MSG(this->s3interface != NULL, S3RuntimeError, "s3interface is NULL");

    this->parseURL();

    this->keyList =
        this->s3interface->listBucket(this->schema, this->region, this->bucket, this->prefix);
}

BucketContent& S3BucketReader::getNextKey() {
    BucketContent& key = this->keyList.contents[this->keyIndex];
    this->keyIndex += s3ext_segnum;
    return key;
}

S3Params S3BucketReader::constructReaderParams(BucketContent& key) {
    S3Params readerParams = this->params;

    // encode the key name but leave the "/"
    // "/encoded_path/encoded_name"
    string keyEncoded = uri_encode(key.getName());
    find_replace(keyEncoded, "%2F", "/");

    readerParams.setKeyUrl(this->getKeyURL(keyEncoded));
    readerParams.setRegion(this->region);
    readerParams.setKeySize(key.getSize());

    S3DEBUG("key: %s, size: %" PRIu64, readerParams.getKeyUrl().c_str(), readerParams.getKeySize());
    return readerParams;
}

uint64_t S3BucketReader::read(char* buf, uint64_t count) {
    S3_CHECK_OR_DIE_MSG(this->upstreamReader != NULL, S3RuntimeError, "upstreamReader is NULL");

    while (true) {
        if (this->needNewReader) {
            if (this->keyIndex >= this->keyList.contents.size()) {
                S3DEBUG("Read finished for segment: %d", s3ext_segid);
                return 0;
            }
            BucketContent& key = this->getNextKey();

            this->upstreamReader->open(constructReaderParams(key));
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
}

string S3BucketReader::getKeyURL(const string& key) {
    stringstream sstr;
    sstr << this->schema << "://"
         << "s3-" << this->region << ".amazonaws.com/";
    sstr << this->bucket << "/" << key;
    return sstr.str();
}

void S3BucketReader::parseURL() {
    this->schema = this->params.isEncryption() ? "https" : "http";
    this->region = S3UrlUtility::getRegionFromURL(this->params.getBaseUrl());
    this->bucket = S3UrlUtility::getBucketFromURL(this->params.getBaseUrl());
    this->prefix = S3UrlUtility::getPrefixFromURL(this->params.getBaseUrl());

    bool ok = !(this->schema.empty() || this->region.empty() || this->bucket.empty());
    S3_CHECK_OR_DIE_MSG(ok, S3ConfigError, this->params.getBaseUrl() + " is not valid",
                        this->params.getBaseUrl());
}
