#include "s3url.h"

S3Url::S3Url(const string &sourceUrl, bool useHttps, const string &version, const string &region)
    : version(version), sourceUrl(sourceUrl), region(region) {
    string schemaStr = useHttps ? "https" : "http";
    FindAndReplace(this->sourceUrl, "s3://", schemaStr + "://");

    // else it was initialized with urlRegion
    if (this->version == "1") {
        extractRegionFromUrl();
    } else if (this->version.empty()) {
        if (extractRegionFromUrl()) {
            this->version = "1";
        } else {
            this->version = "2";
        }
    }

    http_parser_url urlParser;
    int result =
        http_parser_parse_url(this->sourceUrl.c_str(), this->sourceUrl.length(), false, &urlParser);

    S3_CHECK_OR_DIE(result == 0, S3RuntimeError,
                    "Failed to parse URL " + sourceUrl + " at field " +
                        std::to_string((unsigned long long)result));

    this->schema = schemaStr;
    this->host = extractField(&urlParser, UF_HOST);
    this->port = extractField(&urlParser, UF_PORT);

    if (this->port != "443" && !this->port.empty() && useHttps) {
        S3WARN("You are using https on port '%s'", this->port.c_str());
    }

    extractBucket();
    extractEncodedPrefix();
}

string S3Url::getFullUrlForCurl() const {
    stringstream fullUrl;

    fullUrl << this->getSchema() << "://" << this->getHostForCurl() << "/" << this->getBucket()
            << "/" << this->getPrefix();

    return fullUrl.str();
}

string S3Url::getHostForCurl() const {
    if (version != "1") {
        if (port.empty()) {
            return host;
        } else {
            return host + ":" + port;
        }
    } else {
        return "s3-" + region + ".amazonaws.com";
    }
}

string S3Url::getPathForCurl() const {
    stringstream pathSs;

    if (!this->getBucket().empty()) {
        pathSs << "/" << this->getBucket();
    }

    pathSs << "/" << this->getPrefix();

    return pathSs.str();
}

void S3Url::extractEncodedPrefix() {
    size_t ibegin = find_Nth(this->sourceUrl, 3, "/");
    size_t iend = find_Nth(this->sourceUrl, 4, "/");

    // s3://s3-region.amazonaws.com/bucket
    if (ibegin == string::npos || iend == string::npos) {
        return;
    }

    // s3://s3-region.amazonaws.com/bucket/
    if (iend == this->sourceUrl.length() - 1) {
        return;
    }

    ibegin = find_Nth(this->sourceUrl, 4, "/");
    // s3://s3-region.amazonaws.com/bucket/prefix
    // s3://s3-region.amazonaws.com/bucket/prefix/whatever
    this->prefix = this->sourceUrl.substr(ibegin + 1, this->sourceUrl.length() - ibegin - 1);

    this->prefix = UriEncode(this->prefix);
    FindAndReplace(this->prefix, "%2F", "/");
}

void S3Url::extractBucket() {
    size_t ibegin = find_Nth(this->sourceUrl, 3, "/");
    size_t iend = find_Nth(this->sourceUrl, 4, "/");
    if (ibegin == string::npos) {
        return;
    }
    // s3://s3-region.amazonaws.com/bucket
    if (iend == string::npos) {
        this->bucket = this->sourceUrl.substr(ibegin + 1, this->sourceUrl.length() - ibegin - 1);
        return;
    }

    this->bucket = this->sourceUrl.substr(ibegin + 1, iend - ibegin - 1);
}

bool S3Url::extractRegionFromUrl() {
    size_t ibegin = this->sourceUrl.find("://s3") +
                    strlen("://s3");  // index of character('.' or '-') after "3"
    size_t iend = this->sourceUrl.find(".amazonaws.com");

    if (iend == string::npos) {
        return false;
    } else if (ibegin == iend) {  // "s3.amazonaws.com"
        this->region = "external-1";
    } else {
        // ibegin + 1 is the character after "s3." or "s3-"
        // for instance: s3-us-west-2.amazonaws.com
        this->region = this->sourceUrl.substr(ibegin + 1, iend - (ibegin + 1));
    }

    if (this->region.compare("us-east-1") == 0) {
        this->region = "external-1";
    }

    return true;
}

bool S3Url::isValidUrl() const {
    return !this->getBucket().empty();
}

string S3Url::extractField(const struct http_parser_url *urlParser, http_parser_url_fields i) {
    if ((urlParser->field_set & (1 << i)) == 0) {
        return "";
    }

    return this->sourceUrl.substr(urlParser->field_data[i].off, urlParser->field_data[i].len);
}

string S3Url::getExtension() const {
    const string& path = this->prefix;
    std::string::size_type pos = path.find_last_of('/');
    string filename = (pos == path.npos) ? path : path.substr(pos + 1);

    pos = filename.find_last_of('.');
    if (pos == filename.npos) {
        return "";
    }
    return filename.substr(pos);
}
