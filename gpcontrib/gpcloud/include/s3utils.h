#ifndef __S3_UTILS_H__
#define __S3_UTILS_H__

#include "s3common_headers.h"
#include "s3http_headers.h"
#include "s3log.h"

#define MD5_DIGEST_STRING_LENGTH 17
#define SHA_DIGEST_STRING_LENGTH 41
#define SHA256_DIGEST_STRING_LENGTH 65

bool sha1hmac(const char* str, unsigned char out_hash[SHA_DIGEST_LENGTH], const char* secret,
              int secret_len);

bool sha1hmac_hex(const char* str, char out_hash_hex[SHA_DIGEST_STRING_LENGTH], const char* secret,
                  int secret_len);

bool sha256(const char* string, unsigned char out_hash[SHA256_DIGEST_LENGTH]);

bool sha256_hex(const char* string, uint64_t length,
                char out_hash_hex[SHA256_DIGEST_STRING_LENGTH]);

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
    ~Config();
    bool SectionExist(const string& sec);
    string Get(const string& sec, const string& key, const string& defaultvalue);
    bool GetBool(const string& sec, const string& key, const string& defaultvalue);
    bool Scan(const string& sec, const string& key, const char* scanfmt, void* dst);
    int64_t SafeScan(const string& varName, const string& section, int64_t defaultValue,
                     int64_t minValue, int64_t maxValue);

    void* Handle() {
        return (void*)this->_conf;
    };

   private:
    ini_t* _conf;
};

bool ToBool(string str);

string UriEncode(const string& src);

string UriDecode(const string& src);

void FindAndReplace(string& str, const string& find, const string& replace);

void SignRequestV4(const string& method, HTTPHeaders* headers, const string& origRegion,
                   const string& path, const string& query, const S3Credential& cred);

string GetOptS3(const string& options, const string& key);

string TruncateOptions(const string& url_with_options);

#endif  // __S3_UTILS_H__
