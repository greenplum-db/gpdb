#include "gtest/gtest.h"
#include "utils.cpp"
#include <string>

TEST(utils, lower) {
    char data[] = "aAbBcCdDEeFfGgHhIiJjKkLlMmNnOopPQqRrSsTtuUVvwWxXYyZz";
    _tolower(data);
    EXPECT_STREQ("aabbccddeeffgghhiijjkkllmmnnooppqqrrssttuuvvwwxxyyzz", data);
}

// Tests factorial of positive numbers.
TEST(utils, trim) {
    char data[] = " \t\n\r  abc \r\r\n\r \t";
    char out[8] = {0};
    bool ret;
    ret = trim(out, data);
    EXPECT_EQ(ret, true);
    EXPECT_STREQ("abc", out);
}

TEST(utils, time) {
    char data[65];
    gethttpnow(data);
}

#include <curl/curl.h>
TEST(utils, simplecurl) {
    CURL *c = CreateCurlHandler(NULL);
    EXPECT_EQ(c, (void *)NULL);
    c = CreateCurlHandler("www.google.com");
    EXPECT_NE(c, (void *)NULL);
    curl_easy_cleanup(c);
}

TEST(utils, nth) {
    const char *teststr = "aaabbbcccaaatttaaa";
    EXPECT_EQ(find_Nth(teststr, 1, "aaa"), 0);
    EXPECT_EQ(find_Nth(teststr, 2, "aaa"), 9);
    EXPECT_EQ(find_Nth(teststr, 3, "aaa"), 15);
    EXPECT_EQ(find_Nth(teststr, 1, "abc"), -1);
    EXPECT_EQ(find_Nth(teststr, 1, ""), 0);
}

#define MD5TESTBUF "abcdefghijklmnopqrstuvwxyz\n"
TEST(utils, md5) {
    MD5Calc m;
    m.Update(MD5TESTBUF, strlen(MD5TESTBUF));
    EXPECT_STREQ("e302f9ecd2d189fa80aac1c3392e9b9c", m.Get());
    m.Update(MD5TESTBUF, strlen(MD5TESTBUF));
    m.Update(MD5TESTBUF, strlen(MD5TESTBUF));
    m.Update(MD5TESTBUF, strlen(MD5TESTBUF));
    EXPECT_STREQ("3f8c2c6e2579e864071c33919fac61ee", m.Get());
}

#define TEST_STRING "The quick brown fox jumps over the lazy dog"
TEST(utils, base64) {
    char *encoded = NULL;
    EXPECT_STREQ("VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wcyBvdmVyIHRoZSBsYXp5IGRvZw==",
                 encoded = Base64Encode(TEST_STRING, strlen(TEST_STRING)));
    free(encoded);
}

TEST(utils, sha256) {
    char hash_str[65] = {0};
    EXPECT_TRUE(sha256_hex(TEST_STRING, hash_str));
    EXPECT_STREQ(
        "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592",
        hash_str);
}

TEST(utils, sha1hmac) {
    char hash_hex[41] = {0};

    EXPECT_TRUE(sha1hmac_hex(TEST_STRING, (char *)hash_hex, "key", 3));

    EXPECT_STREQ("de7c9b85b8b78aa6bc8a7a36f70a90701c9db4d9", hash_hex);
}

TEST(utils, sha256hmac) {
    char hash_str[65] = {0};
    EXPECT_TRUE(sha256hmac_hex(TEST_STRING, hash_str, "key", 3));
    EXPECT_STREQ(
        "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8",
        hash_str);
}

#if 0
concurrent_queue<int> q;
void *worker(void *a) {
    int r;
    while (1) {
        q.deQ(r);
        if (r == -1) break;
    }
    return NULL;
}

#define num_threads 5

TEST(queue, simple) {
    pthread_t thread_id[num_threads];
    for (int tnum = 0; tnum < num_threads; tnum++) {
        pthread_create(&thread_id[tnum], NULL, &worker, NULL);
    }
    int r = 1024;
    while (r--) {
        q.enQ(r);
    }

    for (int i = 0; i < num_threads; i++) {
        q.enQ(-1);
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(thread_id[i], NULL);
    }
}
#endif

#if 0
#include <memory>
using std::shared_ptr;
using std::make_shared;

TEST(databuffer, simple) {
    MD5Calc m;

    shared_ptr<DataBuffer> pdata = make_shared<DataBuffer>((uint64_t)128);
    EXPECT_EQ(pdata->len(), 0);
    EXPECT_TRUE(pdata->empty());
    EXPECT_FALSE(pdata->full());

    EXPECT_EQ(pdata->append(TEST_STRING, strlen(TEST_STRING)), 43);
    EXPECT_FALSE(pdata->empty());
    EXPECT_EQ(pdata->len(), 43);
    m.Update(pdata->getdata(), 43);
    EXPECT_STREQ("9e107d9d372bb6826bd81d3542a419d6", m.Get());
    EXPECT_EQ(pdata->append(TEST_STRING, strlen(TEST_STRING)), 43);
    EXPECT_EQ(pdata->append(TEST_STRING, strlen(TEST_STRING)), 42);
    EXPECT_EQ(pdata->append(TEST_STRING, strlen(TEST_STRING)), 0);

    EXPECT_TRUE(pdata->full());

    m.Update(pdata->getdata(), 128);
    EXPECT_STREQ("67e3cb156ceb62e5f114f075a8ef3063", m.Get());

    pdata->reset();
    EXPECT_EQ(pdata->len(), 0);
    EXPECT_TRUE(pdata->empty());
    EXPECT_FALSE(pdata->full());

    EXPECT_EQ(pdata->append(TEST_STRING, strlen(TEST_STRING)), 43);
    EXPECT_FALSE(pdata->empty());
    EXPECT_EQ(pdata->len(), 43);
    m.Update(pdata->getdata(), 43);
    EXPECT_STREQ("9e107d9d372bb6826bd81d3542a419d6", m.Get());
    EXPECT_EQ(pdata->append(TEST_STRING, strlen(TEST_STRING)), 43);
    EXPECT_EQ(pdata->append(TEST_STRING, strlen(TEST_STRING)), 42);
    EXPECT_EQ(pdata->append(TEST_STRING, strlen(TEST_STRING)), 0);
}
#endif

TEST(utils, Config) {
    Config c("test/s3test.conf");
    EXPECT_STREQ(c.Get("configtest", "config1", "aaaaaa").c_str(), "abcdefg");
    EXPECT_STREQ(c.Get("configtest", "config2", "tttt").c_str(), "12345");
    EXPECT_STREQ(c.Get("configtest", "config3", "tttt").c_str(), "aaaaa");
    EXPECT_STREQ(c.Get("configtest", "config4", "tttt").c_str(), "123");
    EXPECT_STREQ(c.Get("configtest", "config5", "tttt").c_str(), "tttt");
    EXPECT_STREQ(c.Get("configtest", "config6", "tttt").c_str(), "tttt");
    EXPECT_STREQ(c.Get("configtest", "config7", "xx").c_str(), "xx");

    EXPECT_STREQ(c.Get("configtest", "", "xx").c_str(), "xx");
    EXPECT_STREQ(c.Get("configtest", "config7", "").c_str(), "");

    EXPECT_STREQ(c.Get("configtest", "", "xx").c_str(), "xx");

    uint32_t value = 0;
    EXPECT_TRUE(c.Scan("configtest", "config2", "%ud", &value));
    EXPECT_EQ(value, 12345);

    EXPECT_TRUE(c.Scan("configtest", "config4", "%ud", &value));
    EXPECT_EQ(value, 123);

    EXPECT_FALSE(c.Scan("configtest", "config7", "%ud", &value));
    EXPECT_FALSE(c.Scan("", "config7", "%ud", &value));
    EXPECT_FALSE(c.Scan("configtest", "", "%ud", &value));

    EXPECT_FALSE(c.Scan("configtest", "config5", "%ud", &value));

    char str[128];
    EXPECT_TRUE(c.Scan("configtest", "config3", "%s", str));
    EXPECT_STREQ(str, "aaaaa");
}

TEST(utils, UriCoding) {
    string src1 = "This is a simple & short test.";
    string src2 = "$ & < > ? ; # : = , \" ' ~ + %-_";
    string src3 = "! * ' ( ) ; : @ & = + $ , / ? % # [ ]";
    string src4 =
        "A B C D E F G H I J K L M N O P Q R S T U V W X Y Z a b c d e f g h i "
        "j k l m n o p q r s t u v w x y z 0 1 2 3 4 5 6 7 8 9 - _ . ~";

    string dst1 = "This%20is%20a%20simple%20%26%20short%20test.";
    string dst2 =
        "%24%20%26%20%3C%20%3E%20%3F%20%3B%20%23%20%3A%20%3D%20%2C%20%22%20%27%"
        "20~%20%2B%20%25-_";
    string dst3 =
        "%21%20%2A%20%27%20%28%20%29%20%3B%20%3A%20%40%20%26%20%3D%20%2B%20%24%"
        "20%2C%20%2F%20%3F%20%25%20%23%20%5B%20%5D";
    string dst4 =
        "A%20B%20C%20D%20E%20F%20G%20H%20I%20J%20K%20L%20M%20N%20O%20P%20Q%20R%"
        "20S%20T%20U%20V%20W%20X%20Y%20Z%20a%20b%20c%20d%20e%20f%20g%20h%20i%"
        "20j%20k%20l%20m%20n%20o%20p%20q%20r%20s%20t%20u%20v%20w%20x%20y%20z%"
        "200%201%202%203%204%205%206%207%208%209%20-%20_%20.%20~";

    EXPECT_STREQ(dst1.c_str(), uri_encode(src1).c_str());
    EXPECT_STREQ(dst2.c_str(), uri_encode(src2).c_str());
    EXPECT_STREQ(dst3.c_str(), uri_encode(src3).c_str());
    EXPECT_STREQ(dst4.c_str(), uri_encode(src4).c_str());

    EXPECT_STREQ(src1.c_str(), uri_decode(dst1).c_str());
    EXPECT_STREQ(src2.c_str(), uri_decode(dst2).c_str());
    EXPECT_STREQ(src3.c_str(), uri_decode(dst3).c_str());
    EXPECT_STREQ(src4.c_str(), uri_decode(dst4).c_str());
}
