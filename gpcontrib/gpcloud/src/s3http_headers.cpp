#include "s3http_headers.h"

HTTPHeaders::HTTPHeaders() {
    this->header_list = NULL;
}

HTTPHeaders::~HTTPHeaders() {
    this->FreeList();
}

bool HTTPHeaders::Add(HeaderField f, const string &v) {
    if (v.empty()) {
        return false;
    } else {
        this->fields[f] = v;
        return true;
    }
}

void HTTPHeaders::Disable(HeaderField f) {
    this->disabledFields.insert(f);
}

const char *HTTPHeaders::Get(HeaderField f) {
    return this->fields[f].empty() ? NULL : this->fields[f].c_str();
}

// Convert this->fields map to header list used by curl.
void HTTPHeaders::CreateList() {
    struct curl_slist *headers = NULL;
    std::map<HeaderField, std::string>::iterator it;

    for (it = this->fields.begin(); it != this->fields.end(); it++) {
        std::stringstream sstr;
        sstr << GetFieldString(it->first) << ": " << it->second;
        headers = curl_slist_append(headers, sstr.str().c_str());
    }

    std::set<HeaderField>::iterator it2;
    for (it2 = this->disabledFields.begin(); it2 != this->disabledFields.end(); it2++) {
        std::stringstream sstr;
        sstr << GetFieldString(*it2) << ":";
        headers = curl_slist_append(headers, sstr.str().c_str());
    }

    this->header_list = headers;
}

struct curl_slist *HTTPHeaders::GetList() {
    return this->header_list;
}

void HTTPHeaders::FreeList() {
    if (this->header_list) {
        curl_slist_free_all(this->header_list);
        this->header_list = NULL;
    }
}

const char *GetFieldString(HeaderField f) {
    switch (f) {
        case HOST:
            return "Host";
        case RANGE:
            return "Range";
        case DATE:
            return "Date";
        case CONTENTLENGTH:
            return "Content-Length";
        case CONTENTMD5:
            return "Content-MD5";
        case CONTENTTYPE:
            return "Content-Type";
        case EXPECT:
            return "Expect";
        case AUTHORIZATION:
            return "Authorization";
        case ETAG:
            return "ETag";
        case X_AMZ_DATE:
            return "x-amz-date";
        case X_AMZ_CONTENT_SHA256:
            return "x-amz-content-sha256";
        case X_AMZ_SERVER_SIDE_ENCRYPTION:
            return "x-amz-server-side-encryption";
        default:
            return "Unknown";
    }
}
