#ifndef __S3_URL_H__
#define __S3_URL_H__

#include "s3common_headers.h"
#include "s3exception.h"
#include "s3macros.h"
#include "s3utils.h"

class S3Url {
   public:
    S3Url(const string &sourceUrl, bool useHttps = true, const string &version = "",
          const string &region = "");

    string getFullUrlForCurl() const;
    string getHostForCurl() const;
    string getPathForCurl() const;

    const string &getSchema() const {
        return schema;
    }

    const string &getHost() const {
        return host;
    }

    const string &getPort() const {
        return port;
    }

    const string &getBucket() const {
        return bucket;
    }

    const string &getPrefix() const {
        return prefix;
    }

    const string &getRegion() const {
        return region;
    }

    const string &getVersion() const {
        return version;
    }

    void setPrefix(const string &prefix) {
        this->prefix = prefix;
    };

    bool isValidUrl() const;

    string getExtension() const;

   private:
    string extractField(const struct http_parser_url *urlParser, http_parser_url_fields i);

    bool extractRegionFromUrl();
    void extractEncodedPrefix();
    void extractBucket();

    string version;

    string sourceUrl;
    string schema;
    string host;
    string port;
    string region;
    string bucket;
    string prefix;
};
#endif
