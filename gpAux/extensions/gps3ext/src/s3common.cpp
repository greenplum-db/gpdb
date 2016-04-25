#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include "s3common.h"
#include "s3conf.h"
#include "s3utils.h"

using std::string;
using std::stringstream;

bool SignRequestV4(string method, HeaderContent *h, string region, string path,
                   string query, const S3Credential &cred) {
    time_t t;
    struct tm tm_info;
    char date_str[17];
    char timestamp_str[17];

    char canonical_hex[65];
    char signature_hex[65];

    string signed_headers;

    unsigned char kDate[SHA256_DIGEST_LENGTH];
    unsigned char kRegion[SHA256_DIGEST_LENGTH];
    unsigned char kService[SHA256_DIGEST_LENGTH];
    unsigned char signingkey[SHA256_DIGEST_LENGTH];

    /* YYYYMMDD'T'HHMMSS'Z' */
    t = time(NULL);
    gmtime_r(&t, &tm_info);
    strftime(timestamp_str, 17, "%Y%m%dT%H%M%SZ", &tm_info);

    h->Add(X_AMZ_DATE, timestamp_str);
    memcpy(date_str, timestamp_str, 8);
    date_str[8] = '\0';

    // XXX sort queries automatically
    // http://docs.aws.amazon.com/AmazonS3/latest/API/sigv4-query-string-auth.html
    string query_encoded = uri_encode(query);
    find_replace(query_encoded, "%26", "&");
    find_replace(query_encoded, "%3D", "=");

    stringstream canonical_str;

    canonical_str << method << "\n"
                  << path << "\n"
                  << query_encoded << "\nhost:" << h->Get(HOST)
                  << "\nx-amz-content-sha256:" << h->Get(X_AMZ_CONTENT_SHA256)
                  << "\nx-amz-date:" << h->Get(X_AMZ_DATE) << "\n\n"
                  << "host;x-amz-content-sha256;x-amz-date\n"
                  << h->Get(X_AMZ_CONTENT_SHA256);
    signed_headers = "host;x-amz-content-sha256;x-amz-date";

    // printf("\n%s\n\n", canonical_str.str().c_str());
    sha256_hex(canonical_str.str().c_str(), canonical_hex);

    // http://docs.aws.amazon.com/general/latest/gr/rande.html#s3_region
    find_replace(region, "external-1", "us-east-1");

    stringstream string2sign_str;
    string2sign_str << "AWS4-HMAC-SHA256\n"
                    << timestamp_str << "\n"
                    << date_str << "/" << region << "/s3/aws4_request\n"
                    << canonical_hex;

    // printf("\n%s\n\n", string2sign_str.str().c_str());
    stringstream kSecret;
    kSecret << "AWS4" << cred.secret;

    sha256hmac(date_str, kDate, kSecret.str().c_str(),
               strlen(kSecret.str().c_str()));
    sha256hmac(region.c_str(), kRegion, (char *)kDate, SHA256_DIGEST_LENGTH);
    sha256hmac("s3", kService, (char *)kRegion, SHA256_DIGEST_LENGTH);
    sha256hmac("aws4_request", signingkey, (char *)kService,
               SHA256_DIGEST_LENGTH);
    sha256hmac_hex(string2sign_str.str().c_str(), signature_hex,
                   (char *)signingkey, SHA256_DIGEST_LENGTH);

    stringstream signature_header;
    signature_header << "AWS4-HMAC-SHA256 Credential=" << cred.keyid << "/"
                     << date_str << "/" << region << "/"
                     << "s3"
                     << "/aws4_request,SignedHeaders=" << signed_headers
                     << ",Signature=" << signature_hex;

    h->Add(AUTHORIZATION, signature_header.str());

    return true;
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
        default:
            return "unknown";
    }
}

bool HeaderContent::Add(HeaderField f, const std::string &v) {
    if (!v.empty()) {
        this->fields[f] = std::string(v);
        return true;
    } else {
        return false;
    }
}

const char *HeaderContent::Get(HeaderField f) {
    const char *ret = NULL;
    if (!this->fields[f].empty()) {
        ret = this->fields[f].c_str();
    }
    return ret;
}

struct curl_slist *HeaderContent::GetList() {
    struct curl_slist *chunk = NULL;
    std::map<HeaderField, std::string>::iterator it;
    for (it = this->fields.begin(); it != this->fields.end(); it++) {
        std::stringstream sstr;
        sstr << GetFieldString(it->first) << ": " << it->second;
        chunk = curl_slist_append(chunk, sstr.str().c_str());
    }
    return chunk;
}

UrlParser::UrlParser(const char *url) {
    this->schema = NULL;
    this->host = NULL;
    this->path = NULL;
    this->fullurl = NULL;

    if (!url) {
        // throw exception
        return;
    }

    struct http_parser_url u;
    int len, result;

    len = strlen(url);
    this->fullurl = (char *)malloc(len + 1);
    if (!this->fullurl) return;

    memcpy(this->fullurl, url, len);
    this->fullurl[len] = 0;

    // only parse len, no need to memset this->fullurl
    result = http_parser_parse_url(this->fullurl, len, false, &u);
    if (result != 0) {
        S3ERROR("Parse error : %d\n", result);
        return;
    }

    // std::cout<<u.field_set<<std::endl;
    this->schema = extract_field(&u, UF_SCHEMA);
    this->host = extract_field(&u, UF_HOST);
    this->path = extract_field(&u, UF_PATH);
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

char *UrlParser::extract_field(const struct http_parser_url *u,
                               http_parser_url_fields i) {
    char *ret = NULL;
    if ((u->field_set & (1 << i)) != 0) {
        ret = (char *)malloc(u->field_data[i].len + 1);
        if (ret) {
            memcpy(ret, this->fullurl + u->field_data[i].off,
                   u->field_data[i].len);
            ret[u->field_data[i].len] = 0;
        }
    }
    return ret;
}

// return the number of items
uint64_t ParserCallback(void *contents, uint64_t size, uint64_t nmemb,
                        void *userp) {
    uint64_t realsize = size * nmemb;
    struct XMLInfo *pxml = (struct XMLInfo *)userp;

    // printf("%.*s",realsize, (char*)contents);

    if (!pxml->ctxt) {
        pxml->ctxt = xmlCreatePushParserCtxt(NULL, NULL, (const char *)contents,
                                             realsize, "resp.xml");
    } else {
        xmlParseChunk(pxml->ctxt, (const char *)contents, realsize, 0);
    }

    return nmemb;
}

// invoked by s3_import(), need to be exception safe
char *get_opt_s3(const char *url, const char *key) {
    char *key_start = NULL;
    char *value_start = NULL;
    char *value = NULL;
    char *ptr = NULL;
    int value_len = 0;

    if (!url || !key) {
        return NULL;
    }

    int key_len = strlen(key);

    // construct the key to search " key="
    char *key2search = (char *)malloc(key_len + 3);
    if (!key2search) {
        S3ERROR("Can't allocate memory for string");
        return NULL;
    }

    key2search[0] = ' ';
    memcpy(key2search + 1, key, key_len);
    key2search[key_len + 1] = '=';
    key2search[key_len + 2] = 0;

    // get the pointer " key1=blah1 key2=blah2 ..."
    key_start = (char *)strstr(url, key2search);
    if (key_start == NULL) {
        goto FAIL;
    }

    // get the pointer "blah1 key2=blah2 ..."
    value_start = key_start + key_len + 2;

    // get the length of string "blah1"
    ptr = value_start;
    while ((*ptr != 0) && (*ptr != ' ')) {
        value_len++;
        ptr++;
    }

    if (!value_len) {
        goto FAIL;
    }

    // get the string "blah1"
    value = (char *)malloc(value_len + 1);
    if (!value) {
        goto FAIL;
    }

    memcpy(value, value_start, value_len);
    value[value_len] = 0;

    free(key2search);

    return value;

FAIL:
    if (key2search) {
        free(key2search);
    }
    return NULL;
}

// returned memory needs to be freed
// invoked by s3_import(), need to be exception safe
char *truncate_options(const char *url_with_options) {
    char *url = NULL;
    int url_len = 0;

    // get the length of url
    char *ptr = (char *)url_with_options;
    while ((*ptr != 0) && (*ptr != ' ')) {
        url_len++;
        ptr++;
    }

    // get the string of string
    url = (char *)malloc(url_len + 1);
    if (!url) {
        return NULL;
    }

    memcpy(url, url_with_options, url_len);
    url[url_len] = 0;

    return url;
}
