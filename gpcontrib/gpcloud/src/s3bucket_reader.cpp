#include "s3bucket_reader.h"

S3BucketReader::S3BucketReader() : Reader() {
    this->keyIndex = 0;  // doesn't matter, be set in open()

    this->s3Interface = NULL;
    this->upstreamReader = NULL;

    this->needNewReader = true;
    this->isFirstFile = true;
}

S3BucketReader::~S3BucketReader() {
    this->close();
}

void S3BucketReader::open(const S3Params& params) {
    this->params = params;

    this->keyIndex = s3ext_segid;  // we may change it in unit tests

    S3_CHECK_OR_DIE(this->s3Interface != NULL, S3RuntimeError, "s3Interface is NULL");

    S3Url& s3Url = this->params.getS3Url();

    S3_CHECK_OR_DIE(s3Url.isValidUrl(), S3ConfigError, s3Url.getFullUrlForCurl() + " is not valid",
                    s3Url.getFullUrlForCurl());

    this->keyList = this->s3Interface->listBucket(s3Url);
}

BucketContent& S3BucketReader::getNextKey() {
    BucketContent& key = this->keyList.contents[this->keyIndex];
    this->keyIndex += s3ext_segnum;
    return key;
}

S3Params S3BucketReader::constructReaderParams(BucketContent& key) {
    // encode the key name but leave the "/"
    // "/encoded_path/encoded_name"
    string keyEncoded = UriEncode(key.getName());
    FindAndReplace(keyEncoded, "%2F", "/");

    S3Params readerParams = this->params.setPrefix(keyEncoded);

    readerParams.setKeySize(key.getSize());

    S3DEBUG("key: %s, size: %" PRIu64, readerParams.getS3Url().getFullUrlForCurl().c_str(),
            readerParams.getKeySize());
    return readerParams;
}

uint64_t S3BucketReader::readWithoutHeaderLine(char* buf, uint64_t count) {
    char* current = NULL;
    char* end = NULL;
    char* currentEOL = eolString;

    // check one char at a time
    while (*currentEOL != '\0') {
        if (current == end) {
            uint64_t readCount = this->upstreamReader->read(buf, count);
            // we have reach the end of file but found no matching EOL.
            if (readCount == 0) {
                S3WARN("%s", "Reach end of file before matching line terminator");
                return 0;
            }

            current = buf;
            end = buf + readCount;
        }

        // skip until we met next newline char
        for (; current != end; current++) {
            if (*current == *currentEOL) {
                currentEOL++;
                current++;
                break;
            } else {
                currentEOL = eolString;
            }
        }
    }

    // move remained data to front.
    uint64_t remain = end - current;
    char* p = buf;
    while (current != end) {
        *p = *current;
        p++;
        current++;
    }

    return remain;
}

uint64_t S3BucketReader::read(char* buf, uint64_t count) {
    S3_CHECK_OR_DIE(this->upstreamReader != NULL, S3RuntimeError, "upstreamReader is NULL");
    uint64_t readCount = 0;
    while (true) {
        if (this->needNewReader) {
            if (this->keyIndex >= this->keyList.contents.size()) {
                S3DEBUG("Read finished for segment: %d", s3ext_segid);
                return 0;
            }
            BucketContent& key = this->getNextKey();

            this->upstreamReader->open(constructReaderParams(key));
            this->needNewReader = false;

            // ignore header line if it is not the first file
            if (hasHeader && !this->isFirstFile) {
                readCount = readWithoutHeaderLine(buf, count);
                if (readCount != 0) {
                    return readCount;
                }
            }
        }

        readCount = this->upstreamReader->read(buf, count);
        if (readCount != 0) {
            return readCount;
        }

        // Finished one file, continue to next
        this->upstreamReader->close();
        this->needNewReader = true;
        this->isFirstFile = false;
    }
}

void S3BucketReader::close() {
    if (this->upstreamReader != NULL) {
        this->upstreamReader->close();
        this->upstreamReader = NULL;
    }

    if (!this->keyList.contents.empty()) {
        this->keyList.contents.clear();
    }
}