#ifndef __S3_URL_PARSER_H__
#define __S3_URL_PARSER_H__

#include "http_parser.h"

class UrlParser {
   public:
    UrlParser(const char* url);
    ~UrlParser();
    const char* Schema() { return this->schema; };
    const char* Host() { return this->host; };
    const char* Path() { return this->path; };

    /* data */
   private:
    char* extractField(const struct http_parser_url* u,
                        http_parser_url_fields i);
    char* schema;
    char* host;
    char* path;
    char* fullurl;
};

#endif
