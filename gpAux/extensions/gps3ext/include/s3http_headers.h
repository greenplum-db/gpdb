#ifndef __S3_HTTP_HEADERS_H__
#define __S3_HTTP_HEADERS_H__

#include <map>
#include <string>

#include <curl/curl.h>

#include "http_parser.h"
#include "s3log.h"

using std::string;

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

// HTTPHeaders wraps curl_slist using std::map to ease manipulating HTTP
// headers.
class HTTPHeaders {
   public:
    HTTPHeaders();
    ~HTTPHeaders();

    bool Add(HeaderField f, const string& value);
    const char* Get(HeaderField f);

    void CreateList();
    struct curl_slist* GetList();
    void FreeList();

   private:
    struct curl_slist* header_list;
    std::map<HeaderField, string> fields;
};

const char* GetFieldString(HeaderField f);

#endif
