#include "S3Common.h"
#include "utils.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include "gps3conf.h"

using std::string;
using std::stringstream;

bool SignGETv2(HeaderContent *h, string path_with_query,
               const S3Credential &cred) {
    char timestr[65];
    unsigned char tmpbuf[20];  // SHA_DIGEST_LENGTH is 20

    // CONTENT_LENGTH is not a part of StringToSign
    h->Add(CONTENTLENGTH, "0");

    gethttpnow(timestr);
    h->Add(DATE, timestr);
    stringstream sstr;
    sstr << "GET\n\n\n" << timestr << "\n" << path_with_query;
    if (!sha1hmac(sstr.str().c_str(), tmpbuf, cred.secret.c_str(),
                  strlen(cred.secret.c_str()))) {
        return false;
    }
    // S3DEBUG("%s", sstr.str().c_str());
    char *signature =
        Base64Encode((char *)tmpbuf, 20);  // SHA_DIGEST_LENGTH is 20
    // S3DEBUG("%s", signature);
    sstr.clear();
    sstr.str("");
    sstr << "AWS " << cred.keyid << ":" << signature;
    free(signature);
    // S3DEBUG("%s", sstr.str().c_str());
    h->Add(AUTHORIZATION, sstr.str());

    return true;
}

bool SignPUTv2(HeaderContent *h, string path_with_query,
               const S3Credential &cred) {
    char timestr[65];
    unsigned char tmpbuf[20];  // SHA_DIGEST_LENGTH is 20
    string typestr;

    gethttpnow(timestr);
    h->Add(DATE, timestr);
    stringstream sstr;

    typestr = h->Get(CONTENTTYPE);

    sstr << "PUT\n\n" << typestr << "\n" << timestr << "\n" << path_with_query;
    if (!sha1hmac(sstr.str().c_str(), tmpbuf, cred.secret.c_str(),
                  strlen(cred.secret.c_str()))) {
        return false;
    }
    char *signature =
        Base64Encode((char *)tmpbuf, 20);  // SHA_DIGEST_LENGTH is 20
    sstr.clear();
    sstr.str("");
    sstr << "AWS " << cred.keyid << ":" << signature;
    free(signature);
    h->Add(AUTHORIZATION, sstr.str());

    return true;
}

bool SignPOSTv2(HeaderContent *h, string path_with_query,
                const S3Credential &cred) {
    char timestr[65];
    unsigned char tmpbuf[20];  // SHA_DIGEST_LENGTH is 20
    // string md5str;

    gethttpnow(timestr);
    h->Add(DATE, timestr);
    stringstream sstr;
    // md5str = h->Get(CONTENTMD5);
    const char *typestr = h->Get(CONTENTTYPE);

    if (typestr != NULL) {
        sstr << "POST\n"
             << "\n"
             << typestr << "\n"
             << timestr << "\n"
             << path_with_query;
    } else {
        sstr << "POST\n"
             << "\n"
             << "\n"
             << timestr << "\n"
             << path_with_query;
    }
    // printf("%s\n", sstr.str().c_str());
    if (!sha1hmac(sstr.str().c_str(), tmpbuf, cred.secret.c_str(),
                  strlen(cred.secret.c_str()))) {
        return false;
    }
    char *signature =
        Base64Encode((char *)tmpbuf, 20);  // SHA_DIGEST_LENGTH is 20
    sstr.clear();
    sstr.str("");
    sstr << "AWS " << cred.keyid << ":" << signature;
    free(signature);
    h->Add(AUTHORIZATION, sstr.str());

    return true;
}

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

    // XXX need to do more than this, incase of non-ASCII characters
    // sort queries
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
    const char *key_f = NULL;
    const char *key_tailing = NULL;
    char *key_val = NULL;
    int val_len = 0;

    if (!url || !key) {
        return NULL;
    }

    char *key2search = (char *)malloc(strlen(key) + 3);
    if (!key2search) {
        S3ERROR("Can't allocate memory for string");
        return NULL;
    }

    int key_len = strlen(key);

    key2search[0] = ' ';
    memcpy(key2search + 1, key, key_len);
    key2search[key_len + 1] = '=';
    key2search[key_len + 2] = 0;

    // printf("key2search=%s\n", key2search);
    const char *delimiter = " ";
    const char *options = strstr(url, delimiter);
    if (!options) {  // no options string
        goto FAIL;
    }
    key_f = strstr(options, key2search);
    if (key_f == NULL) {
        goto FAIL;
    }

    key_f += strlen(key2search);
    if (*key_f == ' ') {
        goto FAIL;
    }
    // printf("key_f=%s\n", key_f);

    key_tailing = strstr(key_f, delimiter);
    // printf("key_tailing=%s\n", key_tailing);
    val_len = 0;
    if (key_tailing) {
        val_len = strlen(key_f) - strlen(key_tailing);
    } else {
        val_len = strlen(key_f);
    }

    key_val = (char *)malloc(val_len + 1);
    if (!key_val) {
        goto FAIL;
    }

    memcpy(key_val, key_f, val_len);
    key_val[val_len] = 0;

    free(key2search);

    return key_val;
FAIL:
    if (key2search) {
        free(key2search);
    }
    return NULL;
}

// returned memory needs to be freed
// invoked by s3_import(), need to be exception safe
char *truncate_options(const char *url_with_options) {
    const char *delimiter = " ";
    char *options = strstr((char *)url_with_options, delimiter);
    int url_len = strlen(url_with_options);

    if (options) {
        url_len = strlen(url_with_options) - strlen(options);
    }

    char *url = (char *)malloc(url_len + 1);
    if (url) {
        memcpy(url, url_with_options, url_len);
        url[url_len] = 0;
    }

    return url;
}
