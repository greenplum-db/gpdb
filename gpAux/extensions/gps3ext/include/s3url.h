#ifndef __S3_URL_H__
#define __S3_URL_H__

#include "s3common_headers.h"
#include "s3macros.h"
#include "s3utils.h"

class UrlParser {
   public:
    UrlParser(const string &url);

    ~UrlParser() {
    }

    const string &getSchema() {
        return this->schema;
    };
    const string &getHost() {
        return this->host;
    };
    const string &getPath() {
        return this->path;
    };
    const string &getQuery() {
        return this->query;
    }

   private:
    // get the string of URL field
    string extractField(const struct http_parser_url *u, http_parser_url_fields i);

    string schema;
    string host;
    string path;
    string query;
    string fullurl;
};

class S3UrlUtility {
   public:
    // Set schema to 'https' or 'http'
    static string replaceSchemaFromURL(const string &url, bool useHttps = true);

    static string getDefaultSchema(bool useHttps = true);

    static string getRegionFromURL(const string &url);

    static string getBucketFromURL(const string &url);

    static string getPrefixFromURL(const string &url);
};
#endif
