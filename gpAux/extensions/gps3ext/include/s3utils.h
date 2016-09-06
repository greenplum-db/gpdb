#ifndef __S3_UTILS_H__
#define __S3_UTILS_H__

#include "s3common_headers.h"

#define MD5_DIGEST_STRING_LENGTH 17
#define SHA_DIGEST_STRING_LENGTH 41
#define SHA256_DIGEST_STRING_LENGTH 65

bool sha1hmac(const char* str, unsigned char out_hash[SHA_DIGEST_LENGTH], const char* secret,
              int secret_len);

bool sha1hmac_hex(const char* str, char out_hash_hex[SHA_DIGEST_STRING_LENGTH], const char* secret,
                  int secret_len);

bool sha256(const char* string, unsigned char out_hash[SHA256_DIGEST_LENGTH]);

bool sha256_hex(const char* string, char out_hash_hex[SHA256_DIGEST_STRING_LENGTH]);

bool sha256hmac(const char* str, unsigned char out_hash[SHA256_DIGEST_LENGTH], const char* secret,
                int secret_len);

bool sha256hmac_hex(const char* str, char out_hash_hex[SHA256_DIGEST_STRING_LENGTH],
                    const char* secret, int secret_len);

size_t find_Nth(const string& str,  // where to work
                unsigned N,         // N'th occurrence
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
    unsigned char md5[MD5_DIGEST_STRING_LENGTH];
    string result;
};

class Config {
   public:
    Config(const string& filename);
    Config(const char* filename);
    ~Config();
    bool SectionExist(const string& sec);
    string Get(const string& sec, const string& key, const string& defaultvalue);
    bool Scan(const string& sec, const string& key, const char* scanfmt, void* dst);
    void* Handle() {
        return (void*)this->_conf;
    };

   private:
    ini_t* _conf;
};

bool to_bool(string str);

string uri_encode(const string& src);

string uri_decode(const string& src);

void find_replace(string& str, const string& find, const string& replace);

#endif  // __S3_UTILS_H__
