#ifndef __S3_HTTP_HEADERS_H__
#define __S3_HTTP_HEADERS_H__

#include "s3common_headers.h"

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
    X_AMZ_SERVER_SIDE_ENCRYPTION,
};

// HTTPHeaders wraps curl_slist using std::map to ease manipulating HTTP
// headers.
class HTTPHeaders {
   public:
    HTTPHeaders();
    ~HTTPHeaders();

    bool Add(HeaderField f, const string& value);
    void Disable(HeaderField f);
    const char* Get(HeaderField f);

    void CreateList();
    struct curl_slist* GetList();
    void FreeList();

   private:
    struct curl_slist* header_list;

    std::map<HeaderField, string> fields;
    std::set<HeaderField> disabledFields;
};

const char* GetFieldString(HeaderField f);

#endif
