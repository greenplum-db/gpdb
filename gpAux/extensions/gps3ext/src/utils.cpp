#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <unistd.h>

#include <time.h>
#include <string.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <openssl/des.h>
#include <stdbool.h>
#include <openssl/hmac.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <stdint.h>

#include <curl/curl.h>

#include "utils.h"

#include <string>
#include <sstream>
#include <algorithm>
using std::string;

#include <iomanip>

#ifndef DEBUGS3
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
    strftime(datebuf, 64, "%a, %d %b %Y %H:%M:%S %z", &tm_info);
    return true;
}

void _tolower(char *buf) {
    do {
        if (*buf >= 'A' && *buf <= 'Z') *buf |= 0x60;
    } while (*buf++);
    return;
}

bool trim(char *out, const char *in, const char *trimed) {
    int targetlen;

    if (!out || !in) {  // invalid string params
        return false;
    }

    targetlen = strlen(in);

    while (targetlen > 0) {
        if (strchr(trimed, in[targetlen - 1]) ==
            NULL)  // can't find stripped char
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

//! returning value is malloced
char *Base64Encode(const char *buffer,
                   size_t length) {  // Encodes a binary safe base 64 string
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;
    char *ret;
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);  // Ignore newlines - write
                                                 // everything in one line
    BIO_write(bio, buffer, length);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);

    ret = (char *)malloc(bufferPtr->length + 1);
    memcpy(ret, bufferPtr->data, bufferPtr->length);
    ret[bufferPtr->length] = 0;

    BIO_set_close(bio, BIO_NOCLOSE);
    BIO_free_all(bio);
    return ret;  // s
}

bool sha256(const char *string, char outputBuffer[65]) {
    if (!string) return false;

    unsigned char hash[SHA256_DIGEST_LENGTH];  // 32
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, string, strlen(string));
    SHA256_Final(hash, &sha256);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
    }
    outputBuffer[64] = 0;

    return true;
}

// not returning the normal hex result, might have '\0'
bool sha1hmac(const char *str, char hash[20], const char *secret) {
    if (!str) return false;

    unsigned int len = SHA_DIGEST_LENGTH;  // 20
    HMAC_CTX hmac;
    HMAC_CTX_init(&hmac);
    HMAC_Init_ex(&hmac, secret, strlen(secret), EVP_sha1(), NULL);
    HMAC_Update(&hmac, (unsigned char *)str, strlen(str));
    HMAC_Final(&hmac, (unsigned char *)hash, &len);

    HMAC_CTX_cleanup(&hmac);

    return true;
}

bool sha256hmac(const char *str, char out[65], const char *secret) {
    if (!str) return false;

    unsigned char hash[32];  // must be unsigned here
    unsigned int len = 32;
    HMAC_CTX hmac;
    HMAC_CTX_init(&hmac);
    HMAC_Init_ex(&hmac, secret, strlen(secret), EVP_sha256(), NULL);
    HMAC_Update(&hmac, (unsigned char *)str, strlen(str));

    HMAC_Final(&hmac, hash, &len);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(out + (i * 2), "%02x", hash[i]);
    }
    HMAC_CTX_cleanup(&hmac);
    out[64] = 0;

    return true;
}

// return malloced memory because Base64Encode() does so
char *SignatureV2(const char *date, const char *path, const char *key) {
    int maxlen, len;
    char *tmpbuf;
    char outbuf[20];  // SHA_DIGEST_LENGTH is 20

    if (!date || !path || !key) {
        return NULL;
    }
    maxlen = strlen(date) + strlen(path) + 20;
    tmpbuf = (char *)alloca(maxlen);
    sprintf(tmpbuf, "GET\n\n\n%s\n%s", date, path);
    // printf("%s\n",tmpbuf);
    if (!sha1hmac(tmpbuf, outbuf, key)) {
        return NULL;
    }
    return Base64Encode(outbuf, 20);
}

char *SignatureV4(const char *date, const char *path, const char *key) {
    return NULL;
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
    /**
It would be more efficient to use a variation of KMP to
benefit from the failure function.
- Algorithm inspired by James Kanze.
- http://stackoverflow.com/questions/20406744/
    */
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
        ss << std::hex << std::setw(2) << std::setfill('0')
           << (int)this->md5[i];
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
    memcpy(this->data + length, buf, copylen);
    this->length += copylen;
    return copylen;
}

Config::Config(string filename) : _conf(NULL) {
    if (filename != "") this->_conf = ini_load(filename.c_str());
    if (this->_conf == NULL) {
#ifndef DEBUGS3
        write_log("Failed to load config file\n");
#endif
    }
}

Config::~Config() {
    if (this->_conf) ini_free(this->_conf);
}

string Config::Get(string sec, string key, string defaultvalue) {
    string ret = defaultvalue;
    if ((key == "") || (sec == "")) return ret;

    if (this->_conf) {
        const char *tmp = ini_get(this->_conf, sec.c_str(), key.c_str());
        if (tmp) ret = tmp;
    }
    return ret;
}

bool Config::Scan(string sec, string key, const char *scanfmt, void *dst) {
    if ((key == "") || (sec == "")) return false;

    if (this->_conf) {
        return ini_sget(this->_conf, sec.c_str(), key.c_str(), scanfmt, dst);
    }
    return false;
}

bool to_bool(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    if ((str == "yes") || (str == "true") || (str == "y") || (str == "t") ||
        (str == "1")) {
        return true;
    } else {
        return false;
    }
}
