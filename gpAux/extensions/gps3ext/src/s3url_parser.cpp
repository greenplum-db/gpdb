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

UrlParser::UrlParser(const string &url) {
    CHECK_ARG_OR_DIE(!url.empty());

    this->fullurl = url;

    struct http_parser_url url_parser;
    int result =
        http_parser_parse_url(this->fullurl.c_str(), this->fullurl.length(), false, &url_parser);
    CHECK_OR_DIE_MSG(result == 0, "Failed to parse URL %s at field %d", this->fullurl.c_str(),
                     result);

    this->schema = extractField(&url_parser, UF_SCHEMA);
    this->host = extractField(&url_parser, UF_HOST);
    this->path = extractField(&url_parser, UF_PATH);
    this->query = extractField(&url_parser, UF_QUERY);
}

string UrlParser::extractField(const struct http_parser_url *url_parser, http_parser_url_fields i) {
    if ((url_parser->field_set & (1 << i)) == 0) {
        return "";
    }

    return this->fullurl.substr(url_parser->field_data[i].off, url_parser->field_data[i].len);
}