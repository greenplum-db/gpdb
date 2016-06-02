#ifndef __S3_COMMON_H__
#define __S3_COMMON_H__

#include <map>
#include <string>

#include <curl/curl.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "http_parser.h"
#include "s3http_headers.h"
#include "s3log.h"

using std::string;

struct S3Credential {
    string keyid;
    string secret;
};

enum Method { GET, PUT, POST, DELETE, HEAD };

void SignRequestV4(const string& method, HTTPHeaders* h, const string& orig_region,
                   const string& path, const string& query, const S3Credential& cred);

struct XMLInfo {
    xmlParserCtxtPtr ctxt;
};

uint64_t XMLParserCallback(void* contents, uint64_t size, uint64_t nmemb, void* userp);

char* get_opt_s3(const char* url, const char* key);

char* truncate_options(const char* url_with_options);

#endif  // __S3_COMMON_H__
