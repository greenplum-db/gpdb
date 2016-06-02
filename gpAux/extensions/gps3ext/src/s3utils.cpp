#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <curl/curl.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/des.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>

#include "s3utils.h"

using std::string;

#ifndef S3_STANDALONE
extern "C" {
void write_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
}
#endif

bool gethttpnow(char datebuf[65]) {  //('D, d M Y H:i:s T')
    struct tm tm_info;
    time_t t;
    if (!datebuf) {
        return false;
    }
    time(&t);
    localtime_r(&t, &tm_info);
    strftime(datebuf, 65, "%a, %d %b %Y %H:%M:%S %z", &tm_info);
    return true;
}

bool trim(char *out, const char *in, const char *trimed) {
    int targetlen;

    if (!out || !in) {  // invalid string params
        return false;
    }

    targetlen = strlen(in);

    while (targetlen > 0) {
        if (strchr(trimed, in[targetlen - 1]) == NULL)  // can't find stripped char
            break;
        else
            targetlen--;
    }

    while (targetlen > 0) {
        if (strchr(trimed, *in) == NULL)  // normal string
            break;
        else {
            in++;
            targetlen--;
        }
    }

    memcpy(out, in, targetlen);
    out[targetlen] = 0;
    return true;
}

// not returning the normal hex result, might have '\0'
bool sha1hmac(const char *str, unsigned char out_hash[20], const char *secret, int secret_len) {
    if (!str) return false;

    unsigned int len = 32;

    HMAC_CTX hmac;
    HMAC_CTX_init(&hmac);
    HMAC_Init_ex(&hmac, secret, secret_len, EVP_sha1(), NULL);
    HMAC_Update(&hmac, (unsigned char *)str, strlen(str));
    HMAC_Final(&hmac, out_hash, &len);

    HMAC_CTX_cleanup(&hmac);

    return true;
}

bool sha1hmac_hex(const char *str, char out_hash_hex[41], const char *secret, int secret_len) {
    if (!str) return false;

    unsigned char hash[20];

    sha1hmac(str, hash, secret, secret_len);

    for (int i = 0; i < 20; i++) {
        sprintf(out_hash_hex + (i * 2), "%02x", hash[i]);
    }
    out_hash_hex[40] = 0;

    return true;
}

// SHA256_DIGEST_LENGTH == 32
bool sha256(const char *string, unsigned char out_hash[32]) {
    if (!string) return false;

    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, string, strlen(string));
    SHA256_Final(out_hash, &sha256);

    return true;
}

// SHA256_DIGEST_LENGTH * 2 + 1 == 65
bool sha256_hex(const char *string, char out_hash_hex[65]) {
    if (!string) return false;

    unsigned char hash[SHA256_DIGEST_LENGTH];  // 32

    sha256(string, hash);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(out_hash_hex + (i * 2), "%02x", hash[i]);
    }
    out_hash_hex[64] = 0;

    return true;
}

bool sha256hmac(const char *str, unsigned char out_hash[32], const char *secret, int secret_len) {
    if (!str) return false;

    unsigned int len = 32;

    HMAC_CTX hmac;
    HMAC_CTX_init(&hmac);
    HMAC_Init_ex(&hmac, secret, secret_len, EVP_sha256(), NULL);
    HMAC_Update(&hmac, (unsigned char *)str, strlen(str));
    HMAC_Final(&hmac, out_hash, &len);

    HMAC_CTX_cleanup(&hmac);

    return true;
}

bool sha256hmac_hex(const char *str, char out_hash_hex[65], const char *secret, int secret_len) {
    if (!str) return false;

    unsigned char hash[SHA256_DIGEST_LENGTH];  // 32

    sha256hmac(str, hash, secret, secret_len);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(out_hash_hex + (i * 2), "%02x", hash[i]);
    }
    out_hash_hex[64] = 0;

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
                unsigned N,         // N'th ocurrence
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
    memset(this->md5, 0, 17);
    MD5_Init(&this->c);
}

bool MD5Calc::Update(const char *data, int len) {
    MD5_Update(&this->c, data, len);
    return true;
}

const char *MD5Calc::Get() {
    MD5_Final(this->md5, &c);
    std::stringstream ss;
    for (int i = 0; i < 16; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)this->md5[i];
    this->result = ss.str();

    // Reset MD5 context
    memset(this->md5, 0, 17);
    MD5_Init(&this->c);
    return this->result.c_str();
}

DataBuffer::DataBuffer(uint64_t size) : maxsize(size), length(0) {
    this->data = (char *)malloc(this->maxsize);
}

DataBuffer::~DataBuffer() {
    if (this->data) {
        free(this->data);
    }
}

uint64_t DataBuffer::append(const char *buf, uint64_t len) {
    uint64_t copylen = std::min(len, maxsize - length);
    if (this->data) {
        memcpy(this->data + length, buf, copylen);
        this->length += copylen;
        return copylen;
    } else {
        return 0;
    }
}

Config::Config(const string &filename) : _conf(NULL) {
    if (filename != "") this->_conf = ini_load(filename.c_str());
    if (this->_conf == NULL) {
#ifndef S3_STANDALONE
        write_log("Failed to load config file\n");
#endif
    }
}

Config::~Config() {
    if (this->_conf) ini_free(this->_conf);
}

string Config::Get(const string &sec, const string &key, const string &defaultvalue) {
    string ret = defaultvalue;
    if ((key == "") || (sec == "")) return ret;

    if (this->_conf) {
        const char *tmp = ini_get(this->_conf, sec.c_str(), key.c_str());
        if (tmp) ret = tmp;
    }
    return ret;
}

bool Config::Scan(const string &sec, const string &key, const char *scanfmt, void *dst) {
    if ((key == "") || (sec == "")) return false;

    if (this->_conf) {
        return ini_sget(this->_conf, sec.c_str(), key.c_str(), scanfmt, dst);
    }
    return false;
}

bool to_bool(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    if ((str == "yes") || (str == "true") || (str == "y") || (str == "t") || (str == "1")) {
        return true;
    } else {
        return false;
    }
}

const char uri_mapping[256] = {
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

// alpha, num and - _ . ~ are reserved(RFC 3986).
const char uri_reserved[256] = {
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

std::string uri_encode(const std::string &src) {
    const unsigned char *src_str = (const unsigned char *)src.c_str();
    const int src_len = src.length();

    unsigned char *const sub_start = new unsigned char[src_len * 3];
    unsigned char *sub_end = sub_start;
    const unsigned char *const src_end = src_str + src_len;

    const char uri_rmapping[16 + 1] = "0123456789ABCDEF";

    while (src_str < src_end) {
        if (uri_reserved[*src_str]) {
            *sub_end++ = *src_str;
        } else {
            *sub_end++ = '%';
            *sub_end++ = uri_rmapping[*src_str >> 4];
            *sub_end++ = uri_rmapping[*src_str & 0x0F];
        }

        src_str++;
    }

    std::string ret_str((char *)sub_start, (char *)sub_end);
    delete[] sub_start;
    return ret_str;
}

std::string uri_decode(const std::string &src) {
    const unsigned char *src_str = (const unsigned char *)src.c_str();
    const int src_len = src.length();

    const unsigned char *const src_end = src_str + src_len;
    const unsigned char *const src_last_dec = src_end - 2;

    char *const sub_start = new char[src_len];
    char *sub_end = sub_start;

    char dec1, dec2;

    while (src_str < src_last_dec) {
        if (*src_str == '%') {
            dec1 = uri_mapping[*(src_str + 1)];
            dec2 = uri_mapping[*(src_str + 2)];

            if ((dec1 != -1) && (dec2 != -1)) {
                *sub_end++ = (dec1 << 4) + dec2;
                src_str += 3;
                continue;
            }
        }

        *sub_end++ = *src_str++;
    }

    while (src_str < src_end) *sub_end++ = *src_str++;

    std::string ret_str(sub_start, sub_end);
    delete[] sub_start;
    return ret_str;
}

void find_replace(string &str, const string &find, const string &replace) {
    if (find.empty()) return;

    size_t pos = 0;

    while ((pos = str.find(find, pos)) != string::npos) {
        str.replace(pos, find.length(), replace);
        pos += replace.length();
    }
}
