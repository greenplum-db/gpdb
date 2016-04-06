#ifndef __S3_COMMON_H__
#define __S3_COMMON_H__

#include <curl/curl.h>
#include <map>
#include <string>
#include "http_parser.h"
using std::string;

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "S3Log.h"

struct S3Credential {
    string keyid;
    string secret;
};

enum HeaderField {
    HOST,
    RANGE,
    DATE,
    CONTENTLENGTH,
    CONTENTMD5,
    CONTENTTYPE,
    EXPECT,
    AUTHORIZATION,
    ETAG,
    X_AMZ_DATE,
    X_AMZ_CONTENT_SHA256,
};

enum Method { GET, PUT, POST, DELETE, HEAD };

class HeaderContent {
   public:
    HeaderContent(){};
    ~HeaderContent(){};
    bool Add(HeaderField f, const string& value);
    const char* Get(HeaderField f);
    struct curl_slist* GetList();

   private:
    std::map<HeaderField, string> fields;
};

bool SignGETv2(HeaderContent* h, string path_with_query,
               const S3Credential& cred);

bool SignPUTv2(HeaderContent* h, string path_with_query,
               const S3Credential& cred);

bool SignPOSTv2(HeaderContent* h, string path_with_query,
                const S3Credential& cred);

bool SignRequestV4(string method, HeaderContent* h, string region, string path,
                   string query, const S3Credential& cred);

class UrlParser {
   public:
    UrlParser(const char* url);
    ~UrlParser();
    const char* Schema() { return this->schema; };
    const char* Host() { return this->host; };
    const char* Path() { return this->path; };

    /* data */
   private:
    char* extract_field(const struct http_parser_url* u,
                        http_parser_url_fields i);
    char* schema;
    char* host;
    char* path;
    char* fullurl;
};

const char* GetFieldString(HeaderField f);
CURL* CreateCurlHandler(const char* path);

struct XMLInfo {
    xmlParserCtxtPtr ctxt;
};

uint64_t ParserCallback(void* contents, uint64_t size, uint64_t nmemb,
                        void* userp);

char* get_opt_s3(const char* url, const char* key);

char* truncate_options(const char* url_with_options);

#endif  // __S3_COMMON_H__
