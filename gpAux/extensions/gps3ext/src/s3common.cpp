#include "s3common.h"

string s3extErrorMessage;

void CheckEssentialConfig(const S3Params &params) {
    if (params.getCred().accessID.empty()) {
        CHECK_OR_DIE_MSG(false, "%s", "\"FATAL: access id not set\"");
    }

    if (params.getCred().secret.empty()) {
        CHECK_OR_DIE_MSG(false, "%s", "\"FATAL: secret id not set\"");
    }

    if (s3ext_segnum <= 0) {
        CHECK_OR_DIE_MSG(false, "%s", "\"FATAL: segment info is invalid\"");
    }
}

// Note: better to sort queries automatically
// for more information refer to Amazon S3 document:
// http://docs.aws.amazon.com/AmazonS3/latest/API/sigv4-query-string-auth.html
void SignRequestV4(const string &method, HTTPHeaders *h, const string &orig_region,
                   const string &path, const string &query, const S3Credential &cred) {
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

    stringstream canonical_str;

    canonical_str << method << "\n"
                  << path << "\n"
                  << query << "\nhost:" << h->Get(HOST)
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
    string2sign_str << "AWS4-HMAC-SHA256\n"
                    << h->Get(X_AMZ_DATE) << "\n"
                    << date_str << "/" << region << "/s3/aws4_request\n"
                    << canonical_hex;

    stringstream kSecret;
    kSecret << "AWS4" << cred.secret;

    sha256hmac(date_str, key_date, kSecret.str().c_str(), strlen(kSecret.str().c_str()));
    sha256hmac(region.c_str(), key_region, (char *)key_date, SHA256_DIGEST_LENGTH);
    sha256hmac("s3", key_service, (char *)key_region, SHA256_DIGEST_LENGTH);
    sha256hmac("aws4_request", signing_key, (char *)key_service, SHA256_DIGEST_LENGTH);
    sha256hmac_hex(string2sign_str.str().c_str(), signature_hex, (char *)signing_key,
                   SHA256_DIGEST_LENGTH);

    stringstream signature_header;
    signature_header << "AWS4-HMAC-SHA256 Credential=" << cred.accessID << "/" << date_str << "/"
                     << region << "/"
                     << "s3"
                     << "/aws4_request,SignedHeaders=" << signed_headers
                     << ",Signature=" << signature_hex;

    h->Add(AUTHORIZATION, signature_header.str());

    return;
}

// get_opt_s3 returns first value according to given key.
// key=value pair are separated by whitespace.
string get_opt_s3(const string &urlWithOptions, const string &key) {
    string keyStr = " ";
    keyStr += key;
    keyStr += "=";

    size_t beginIndex = urlWithOptions.find(keyStr);
    if (beginIndex == urlWithOptions.npos) {
        return "";
    } else {
        beginIndex += keyStr.length();
        size_t endIndex = urlWithOptions.find(" ", beginIndex);

        if (endIndex == urlWithOptions.npos) {
            return urlWithOptions.substr(beginIndex);
        } else {
            return urlWithOptions.substr(beginIndex, endIndex - beginIndex);
        }
    }
}

// truncate_options truncates substring after first whitespace.
string truncate_options(const string &urlWithOptions) {
    size_t firstSpace = urlWithOptions.find(" ");
    if (firstSpace == urlWithOptions.npos) {
        return urlWithOptions;
    } else {
        return urlWithOptions.substr(0, firstSpace);
    }
}
