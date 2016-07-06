#ifndef __S3_COMMON_H__
#define __S3_COMMON_H__

#include <map>
#include <string>

#include "gpcommon.h"
#include "http_parser.h"
#include "s3http_headers.h"
#include "s3log.h"

#define DATE_STR_LEN 9
#define TIME_STAMP_STR_LEN 17

using std::string;

struct S3Credential {
    string accessID;
    string secret;
};

enum Method { GET, PUT, POST, DELETE, HEAD };

void SignRequestV4(const string& method, HTTPHeaders* h, const string& orig_region,
                   const string& path, const string& query, const S3Credential& cred);

char* get_opt_s3(const char* url, const char* key);
string get_opt_s3(const string& options, const string& key);

char* truncate_options(const char* url_with_options);
string truncate_options(const string& url_with_options);

#endif  // __S3_COMMON_H__
