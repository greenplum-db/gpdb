#ifndef __S3_UTILFUNCTIONS__
#define __S3_UTILFUNCTIONS__

#include <stdint.h>
#include <sys/types.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <openssl/md5.h>
#include <openssl/sha.h>

#include "ini.h"
#include "s3log.h"

using std::string;

bool gethttpnow(char datebuf[65]);

bool trim(char* out, const char* in, const char* trimed = " \t\r\n");

bool sha1hmac(const char* str, unsigned char out_hash[20], const char* secret,
              int secret_len);

bool sha1hmac_hex(const char* str, char out_hash_hex[41], const char* secret,
                  int secret_len);

bool sha256(const char* string, unsigned char out_hash[32]);

bool sha256_hex(const char* string, char out_hash_hex[65]);

bool sha256hmac(const char* str, unsigned char out_hash[32], const char* secret,
                int secret_len);

bool sha256hmac_hex(const char* str, char out_hash_hex[65], const char* secret,
                    int secret_len);

size_t find_Nth(const string& str,  // where to work
                unsigned N,         // N'th ocurrence
                const string& find  // what to 'find'
                );

class MD5Calc {
   public:
    MD5Calc();
    ~MD5Calc(){};
    bool Update(const char* data, int len);
    const char* Get();

   private:
    MD5_CTX c;
    unsigned char md5[17];
    string result;
};

class DataBuffer {
   public:
    DataBuffer(uint64_t size);
    ~DataBuffer();
    void reset() { length = 0; };

    uint64_t append(const char* buf, uint64_t len);  // ret < len means full
    const char* getdata() { return data; };
    uint64_t len() { return this->length; };
    bool full() { return maxsize == length; };
    bool empty() { return 0 == length; };

   private:
    const uint64_t maxsize;
    uint64_t length;
    // uint64_t offset;
    char* data;
};

class Config {
   public:
    Config(const string& filename);
    ~Config();
    string Get(const string& sec, const string& key,
               const string& defaultvalue);
    bool Scan(const string& sec, const string& key, const char* scanfmt,
              void* dst);
    void* Handle() { return (void*)this->_conf; };

   private:
    ini_t* _conf;
};

bool to_bool(std::string str);

std::string uri_encode(const std::string& src);

std::string uri_decode(const std::string& src);

void find_replace(string& str, const string& find, const string& replace);

#endif  // _UTILFUNCTIONS_
