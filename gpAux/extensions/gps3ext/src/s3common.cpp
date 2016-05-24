#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include <openssl/err.h>
#include <openssl/sha.h>

#include "s3common.h"
#include "s3conf.h"
#include "s3http_headers.h"
#include "s3macros.h"
#include "s3utils.h"

using std::string;
using std::stringstream;

// Note: better to sort queries automatically
// for more information refer to Amazon S3 document:
// http://docs.aws.amazon.com/AmazonS3/latest/API/sigv4-query-string-auth.html
static string encode_query_str(const string &query) {
    string query_encoded = uri_encode(query);
    find_replace(query_encoded, "%26", "&");
    find_replace(query_encoded, "%3D", "=");

    return query_encoded;
}

#define DATE_STR_LEN 9
#define TIME_STAMP_STR_LEN 17
#define SHA256_DIGEST_STRING_LENGTH 65
bool SignRequestV4(const string &method, HTTPHeaders *h,
                   const string &orig_region, const string &path,
                   const string &query, const S3Credential &cred) {
    struct tm tm_info;
    char date_str[DATE_STR_LEN] = {0};
    char timestamp_str[TIME_STAMP_STR_LEN] = {0};

    char canonical_hex[SHA256_DIGEST_STRING_LENGTH] = {0};
    char signature_hex[SHA256_DIGEST_STRING_LENGTH] = {0};

    unsigned char key_date[SHA256_DIGEST_LENGTH] = {0};
    unsigned char key_region[SHA256_DIGEST_LENGTH] = {0};
    unsigned char key_service[SHA256_DIGEST_LENGTH] = {0};
    unsigned char signing_key[SHA256_DIGEST_LENGTH] = {0};

    // YYYYMMDD'T'HHMMSS'Z'
    time_t t = time(NULL);
    gmtime_r(&t, &tm_info);
    strftime(timestamp_str, TIME_STAMP_STR_LEN, "%Y%m%dT%H%M%SZ", &tm_info);

    // for unit tests' convenience
    if (!h->Get(X_AMZ_DATE)) {
        h->Add(X_AMZ_DATE, timestamp_str);
    }
    memcpy(date_str, h->Get(X_AMZ_DATE), DATE_STR_LEN - 1);

    string query_encoded = encode_query_str(query);
    stringstream canonical_str;

    canonical_str << method << "\n" << path << "\n" << query_encoded
                  << "\nhost:" << h->Get(HOST)
                  << "\nx-amz-content-sha256:" << h->Get(X_AMZ_CONTENT_SHA256)
                  << "\nx-amz-date:" << h->Get(X_AMZ_DATE) << "\n\n"
                  << "host;x-amz-content-sha256;x-amz-date\n"
                  << h->Get(X_AMZ_CONTENT_SHA256);
    string signed_headers = "host;x-amz-content-sha256;x-amz-date";

    sha256_hex(canonical_str.str().c_str(), canonical_hex);

    // http://docs.aws.amazon.com/general/latest/gr/rande.html#s3_region
    string region = orig_region;
    find_replace(region, "external-1", "us-east-1");

    stringstream string2sign_str;
    string2sign_str << "AWS4-HMAC-SHA256\n" << h->Get(X_AMZ_DATE) << "\n"
                    << date_str << "/" << region << "/s3/aws4_request\n"
                    << canonical_hex;

    stringstream kSecret;
    kSecret << "AWS4" << cred.secret;

    sha256hmac(date_str, key_date, kSecret.str().c_str(),
               strlen(kSecret.str().c_str()));
    sha256hmac(region.c_str(), key_region, (char *)key_date,
               SHA256_DIGEST_LENGTH);
    sha256hmac("s3", key_service, (char *)key_region, SHA256_DIGEST_LENGTH);
    sha256hmac("aws4_request", signing_key, (char *)key_service,
               SHA256_DIGEST_LENGTH);
    sha256hmac_hex(string2sign_str.str().c_str(), signature_hex,
                   (char *)signing_key, SHA256_DIGEST_LENGTH);

    stringstream signature_header;
    signature_header << "AWS4-HMAC-SHA256 Credential=" << cred.keyid << "/"
                     << date_str << "/" << region << "/"
                     << "s3"
                     << "/aws4_request,SignedHeaders=" << signed_headers
                     << ",Signature=" << signature_hex;

    h->Add(AUTHORIZATION, signature_header.str());

    return true;
}

// return the number of items
uint64_t XMLParserCallback(void *contents, uint64_t size, uint64_t nmemb,
                           void *userp) {
    uint64_t realsize = size * nmemb;
    struct XMLInfo *pxml = (struct XMLInfo *)userp;

    if (!pxml->ctxt) {
        pxml->ctxt = xmlCreatePushParserCtxt(NULL, NULL, (const char *)contents,
                                             realsize, "resp.xml");
    } else {
        xmlParseChunk(pxml->ctxt, (const char *)contents, realsize, 0);
    }

    return nmemb;
}

// Returns string lengh till next occurence of given character.
static int strlen_to_next_char(const char *ptr, char ch) {
    int len = 0;
    while ((*ptr != '\0') && (*ptr != ch)) {
        len++;
        ptr++;
    }

    return len;
}

// get_opt_s3 returns first value according to given key.
// key=value pair are separated by whitespace.
// It is caller's responsibility to free returned memory.
char *get_opt_s3(const char *url, const char *key) {
    CHECK_ARG_OR_DIE((url != NULL) && (key != NULL));

    // construct the key to search " key="
    int key_len = strlen(key);
    char *key2search = (char *)malloc(key_len + 3);
    CHECK_OR_DIE_MSG(key2search != NULL,
                     "Can not allocate %d bytes memory for key string",
                     key_len + 3);

    snprintf(key2search, key_len + 3, " %s=", key);

    // get the pointer " key1=blah1 key2=blah2 ..."
    char *key_start = strstr((char *)url, key2search);
    free(key2search);

    CHECK_OR_DIE_MSG(key_start != NULL, "Can not find %s in %s", key, url);

    // get the pointer "blah1 key2=blah2 ..."
    char *value_start = key_start + key_len + 2;

    // get the length of string "blah1"
    int value_len = strlen_to_next_char(value_start, ' ');

    CHECK_OR_DIE_MSG(value_len != 0, "Can not find value of %s in %s", key,
                     url);

    // get the string "blah1"
    char *value = strndup(value_start, value_len);
    CHECK_OR_DIE_MSG(value != NULL,
                     "Can not allocate %d bytes memory for value string",
                     value_len);

    return value;
}

// truncate_options truncates substring after first whitespace.
// It is caller's responsibility to free returned memory.
char *truncate_options(const char *url_with_options) {
    // get the length of url
    int url_len = strlen_to_next_char(url_with_options, ' ');

    // get the string of url
    char *url = strndup(url_with_options, url_len);
    CHECK_OR_DIE_MSG(url != NULL,
                     "Can not allocate %d bytes memory for value string",
                     url_len);

    return url;
}
