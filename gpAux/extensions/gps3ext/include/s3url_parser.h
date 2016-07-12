#ifndef __S3_URL_PARSER_H__
#define __S3_URL_PARSER_H__

#include "http_parser.h"

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
    string extractField(const struct http_parser_url *u, http_parser_url_fields i);

    string schema;
    string host;
    string path;
    string query;
    string fullurl;
};

#endif
