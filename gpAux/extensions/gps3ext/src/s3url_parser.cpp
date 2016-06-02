#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include <map>
#include <string>

#include "http_parser.h"

#include "s3log.h"
#include "s3macros.h"
#include "s3url_parser.h"

using std::string;
using std::stringstream;

UrlParser::UrlParser(const char *url) {
    CHECK_ARG_OR_DIE(url);

    this->schema = NULL;
    this->host = NULL;
    this->path = NULL;
    this->fullurl = NULL;

    this->fullurl = strdup(url);
    CHECK_OR_DIE_MSG(this->fullurl != NULL, "%s", "Could not allocate memory for fullurl");

    struct http_parser_url url_parser;
    int result = http_parser_parse_url(this->fullurl, strlen(this->fullurl), false, &url_parser);
    CHECK_OR_DIE_MSG(result == 0, "Failed to parse URL %s at field %d", this->fullurl, result);

    this->schema = extractField(&url_parser, UF_SCHEMA);
    this->host = extractField(&url_parser, UF_HOST);
    this->path = extractField(&url_parser, UF_PATH);
}

UrlParser::~UrlParser() {
    if (this->schema) free(this->schema);
    if (this->host) free(this->host);
    if (this->path) free(this->path);
    if (this->fullurl) free(this->fullurl);

    this->schema = NULL;
    this->host = NULL;
    this->path = NULL;
    this->fullurl = NULL;
}

char *UrlParser::extractField(const struct http_parser_url *url_parser, http_parser_url_fields i) {
    if ((url_parser->field_set & (1 << i)) == 0) {
        return NULL;
    }

    return strndup(this->fullurl + url_parser->field_data[i].off, url_parser->field_data[i].len);
}
