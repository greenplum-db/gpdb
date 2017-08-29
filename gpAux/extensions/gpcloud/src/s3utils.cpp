#include "s3utils.h"

#include <iomanip>
#ifndef S3_STANDALONE
extern "C" {
void write_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
}
#endif

// not returning the normal hex result, might have '\0'
bool sha1hmac(const char *str, unsigned char out_hash[SHA_DIGEST_LENGTH], const char *secret,
              int secret_len) {
    if (!str) return false;

    // SHA_DIGEST_LENGTH is 20
    HMAC(EVP_sha1(), secret, secret_len, (unsigned char *)str, strlen(str), out_hash, NULL);

    return true;
}

bool sha1hmac_hex(const char *str, char out_hash_hex[SHA_DIGEST_STRING_LENGTH], const char *secret,
                  int secret_len) {
    if (!str) return false;

    unsigned char hash[SHA_DIGEST_LENGTH];

    sha1hmac(str, hash, secret, secret_len);

    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        sprintf(out_hash_hex + (i * 2), "%02x", hash[i]);
    }
    out_hash_hex[SHA_DIGEST_STRING_LENGTH - 1] = 0;

    return true;
}

bool sha256_hex(const char *string, uint64_t length,
                char out_hash_hex[SHA256_DIGEST_STRING_LENGTH]) {
    if (!string) return false;

    unsigned char hash[SHA256_DIGEST_LENGTH];  // 32

    SHA256((unsigned char *)string, length, hash);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(out_hash_hex + (i * 2), "%02x", hash[i]);
    }
    out_hash_hex[SHA256_DIGEST_STRING_LENGTH - 1] = 0;

    return true;
}

bool sha256_hex(const char *string, char out_hash_hex[SHA256_DIGEST_STRING_LENGTH]) {
    return sha256_hex(string, strlen(string), out_hash_hex);
}

bool sha256hmac(const char *str, unsigned char out_hash[32], const char *secret, int secret_len) {
    if (!str) return false;

    // SHA256_DIGEST_LENGTH is 32
    HMAC(EVP_sha256(), secret, secret_len, (unsigned char *)str, strlen(str), out_hash, NULL);
    return true;
}

bool sha256hmac_hex(const char *str, char out_hash_hex[65], const char *secret, int secret_len) {
    if (!str) return false;

    unsigned char hash[SHA256_DIGEST_LENGTH];  // 32

    sha256hmac(str, hash, secret, secret_len);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(out_hash_hex + (i * 2), "%02x", hash[i]);
    }
    out_hash_hex[SHA256_DIGEST_STRING_LENGTH - 1] = 0;

    return true;
}

CURL *CreateCurlHandler(const char *path) {
    CURL *curl = NULL;
    if (!path) {
        return NULL;
    } else {
        curl = curl_easy_init();
    }

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, path);
        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    }
    return curl;
}

/*
 * It would be more efficient to use a variation of KMP to
 * benefit from the failure function.
 * - Algorithm inspired by James Kanze.
 * - http://stackoverflow.com/questions/20406744/
 */
size_t find_Nth(const string &str,  // where to work
                unsigned N,         // N'th occurrence
                const string &find  // what to 'find'
                ) {
    if (0 == N) {
        return string::npos;
    }
    size_t pos, from = 0;
    unsigned i = 0;
    while (i < N) {
        pos = str.find(find, from);
        if (string::npos == pos) {
            break;
        }
        from = pos + 1;  // from = pos + find.size();
        ++i;
    }
    return pos;
}

MD5Calc::MD5Calc() {
    memset(this->md5, 0, MD5_DIGEST_STRING_LENGTH);
    MD5_Init(&this->c);
}

bool MD5Calc::Update(const char *data, int len) {
    MD5_Update(&this->c, data, len);
    return true;
}

const char *MD5Calc::Get() {
    MD5_Final(this->md5, &c);
    std::stringstream ss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)this->md5[i];
    this->result = ss.str();

    // Reset MD5 context
    memset(this->md5, 0, MD5_DIGEST_STRING_LENGTH);
    MD5_Init(&this->c);
    return this->result.c_str();
}

Config::Config(const string &filename) : _conf(NULL) {
    if (!filename.empty()) this->_conf = ini_load(filename.c_str());
    if (this->_conf == NULL) {
#ifndef S3_STANDALONE
        write_log("Failed to load config file:'%s'\n", filename.c_str());
#else
        S3ERROR("Failed to load config file:'%s'", filename.c_str());
#endif
    }
}

Config::~Config() {
    if (this->_conf) ini_free(this->_conf);
}

bool Config::SectionExist(const string &sec) {
    return ini_section_exist(this->_conf, sec.c_str());
}

string Config::Get(const string &sec, const string &key, const string &defaultvalue) {
    string ret = defaultvalue;
    if ((key == "") || (sec == "") || (this->_conf == NULL)) return ret;

    const char *tmp = ini_get(this->_conf, sec.c_str(), key.c_str());
    if (tmp) ret = tmp;
    return ret;
}

bool Config::GetBool(const string &sec, const string &key, const string &defaultvalue) {
    return ToBool(Get(sec, key, defaultvalue));
}

bool Config::Scan(const string &sec, const string &key, const char *scanfmt, void *dst) {
    if ((key == "") || (sec == "") || (this->_conf == NULL)) return false;

    return ini_sget(this->_conf, sec.c_str(), key.c_str(), scanfmt, dst);
}

int64_t Config::SafeScan(const string &varName, const string &section, int64_t defaultValue,
                         int64_t minValue, int64_t maxValue) {
    int64_t scannedValue = 0;

    // Here we use %12lld to truncate long number string, because configure value
    // has type of int32_t, who has the range [-2^31, 2^31-1] (10-digit number)
    bool isSuccess = Scan(section, varName, "%12lld", &scannedValue);

    if (!isSuccess) {
        scannedValue = defaultValue;
    } else if (scannedValue > maxValue) {
        scannedValue = maxValue;
    } else if (scannedValue < minValue) {
        scannedValue = minValue;
    }

    return scannedValue;
}

bool ToBool(string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    if ((str == "yes") || (str == "true") || (str == "y") || (str == "t") || (str == "1")) {
        return true;
    } else {
        return false;
    }
}

const signed char UriMapping[256] = {
    /*       0   1   2   3   4   5   6   7
     *       8   9   A   B   C   D   E   F */
    /* 0 */ -1, -1, -1, -1, -1, -1, -1, -1,
    /*   */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* 1 */ -1, -1, -1, -1, -1, -1, -1, -1,
    /*   */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* 2 */ -1, -1, -1, -1, -1, -1, -1, -1,
    /*   */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* 3 */ 0,  1,  2,  3,  4,  5,  6,  7,
    /*   */ 8,  9,  -1, -1, -1, -1, -1, -1,

    /* 4 */ -1, 10, 11, 12, 13, 14, 15, -1,
    /*   */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* 5 */ -1, -1, -1, -1, -1, -1, -1, -1,
    /*   */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* 6 */ -1, 10, 11, 12, 13, 14, 15, -1,
    /*   */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* 7 */ -1, -1, -1, -1, -1, -1, -1, -1,
    /*   */ -1, -1, -1, -1, -1, -1, -1, -1,

    /* 8 */ -1, -1, -1, -1, -1, -1, -1, -1,
    /*   */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* 9 */ -1, -1, -1, -1, -1, -1, -1, -1,
    /*   */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* A */ -1, -1, -1, -1, -1, -1, -1, -1,
    /*   */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* B */ -1, -1, -1, -1, -1, -1, -1, -1,
    /*   */ -1, -1, -1, -1, -1, -1, -1, -1,

    /* C */ -1, -1, -1, -1, -1, -1, -1, -1,
    /*   */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* D */ -1, -1, -1, -1, -1, -1, -1, -1,
    /*   */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* E */ -1, -1, -1, -1, -1, -1, -1, -1,
    /*   */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* F */ -1, -1, -1, -1, -1, -1, -1, -1,
    /*   */ -1, -1, -1, -1, -1, -1, -1, -1};

// alpha, numbers and - _ . ~ are reserved(RFC 3986).
const char UriReserved[256] = {
    /*      0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
    /* 0 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 1 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 2 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0,
    /* 3 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,

    /* 4 */ 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /* 5 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,
    /* 6 */ 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    /* 7 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0,

    /* 8 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 9 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* A */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* B */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    /* C */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* D */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* E */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* F */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

string UriEncode(const string &src) {
    auto srcStr = src.begin();

    string retStr;

    const char uriRmapping[16 + 1] = "0123456789ABCDEF";

    while (srcStr != src.end()) {
        char c = *srcStr;
        if (UriReserved[(int)c]) {
            retStr.push_back(c);
        } else {
            retStr.push_back('%');
            retStr.push_back(uriRmapping[c >> 4]);
            retStr.push_back(uriRmapping[c & 0x0F]);
        }

        srcStr++;
    }

    return retStr;
}

string UriDecode(const string &src) {
    signed char dec1, dec2;

    auto srcStr = src.begin();
    auto srcEnd = src.end() - 2;

    string retStr;
    while (srcStr < srcEnd) {
        if (*srcStr == '%') {
            dec1 = UriMapping[(int)*(srcStr + 1)];
            dec2 = UriMapping[(int)*(srcStr + 2)];

            if ((dec1 != -1) && (dec2 != -1)) {
                retStr.push_back((dec1 << 4) + dec2);
                srcStr += 3;
                continue;
            }
        }

        retStr.push_back(*srcStr);
        srcStr++;
    }

    while (srcStr < src.end()) {
        retStr.push_back(*srcStr);
        srcStr++;
    }

    return retStr;
}

void FindAndReplace(string &str, const string &find, const string &replace) {
    if (find.empty()) return;

    size_t pos = 0;

    while ((pos = str.find(find, pos)) != string::npos) {
        str.replace(pos, find.length(), replace);
        pos += replace.length();
    }
}

// Note: better to sort queries automatically
// for more information refer to Amazon S3 document:
// http://docs.aws.amazon.com/AmazonS3/latest/API/sigv4-query-string-auth.html
void SignRequestV4(const string &method, HTTPHeaders *headers, const string &origRegion,
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
    if (!headers->Get(X_AMZ_DATE)) {
        headers->Add(X_AMZ_DATE, timestamp_str);
    }
    memcpy(date_str, headers->Get(X_AMZ_DATE), DATE_STR_LEN - 1);

    stringstream canonical_str;

    canonical_str << method << "\n"
                  << path << "\n"
                  << query << "\nhost:" << headers->Get(HOST)
                  << "\nx-amz-content-sha256:" << headers->Get(X_AMZ_CONTENT_SHA256)
                  << "\nx-amz-date:" << headers->Get(X_AMZ_DATE);

    string signed_headers;
    if (headers->Get(X_AMZ_SERVER_SIDE_ENCRYPTION) != NULL) {
        canonical_str << "\nx-amz-server-side-encryption:"
                      << headers->Get(X_AMZ_SERVER_SIDE_ENCRYPTION) << "\n\n"
                      << "host;x-amz-content-sha256;x-amz-date;x-amz-server-side-encryption\n"
                      << headers->Get(X_AMZ_CONTENT_SHA256);

        signed_headers = "host;x-amz-content-sha256;x-amz-date;x-amz-server-side-encryption";
    } else {
        canonical_str << "\n\n"
                      << "host;x-amz-content-sha256;x-amz-date\n"
                      << headers->Get(X_AMZ_CONTENT_SHA256);

        signed_headers = "host;x-amz-content-sha256;x-amz-date";
    }

    sha256_hex(canonical_str.str().c_str(), canonical_hex);

    // http://docs.aws.amazon.com/general/latest/gr/rande.html#s3_region
    string region = origRegion;
    FindAndReplace(region, "external-1", "us-east-1");

    stringstream string2sign_str;
    string2sign_str << "AWS4-HMAC-SHA256\n"
                    << headers->Get(X_AMZ_DATE) << "\n"
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

    headers->Add(AUTHORIZATION, signature_header.str());
}

// GetOptS3 returns first value according to given key.
// key=value pair are separated by whitespace.
string GetOptS3(const string &urlWithOptions, const string &key) {
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

// TruncateOptions truncates substring after first whitespace.
string TruncateOptions(const string &urlWithOptions) {
    size_t firstSpace = urlWithOptions.find(" ");
    if (firstSpace == urlWithOptions.npos) {
        return urlWithOptions;
    } else {
        return urlWithOptions.substr(0, firstSpace);
    }
}
