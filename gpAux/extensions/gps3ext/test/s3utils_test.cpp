#include "s3utils.cpp"
#include "gtest/gtest.h"

// Tests factorial of positive numbers.
TEST(Utils, trim) {
    char data[] = " \t\n\r  abc \r\r\n\r \t";
    char out[8] = {0};
    bool ret;
    ret = trim(out, data);
    EXPECT_EQ(ret, true);
    EXPECT_STREQ("abc", out);
}

TEST(Utils, time) {
    char data[65];
    gethttpnow(data);
}

TEST(Utils, simplecurl) {
    CURL *c = CreateCurlHandler(NULL);
    EXPECT_EQ(c, (void *)NULL);
    c = CreateCurlHandler("www.google.com");
    EXPECT_NE(c, (void *)NULL);
    curl_easy_cleanup(c);
}

TEST(Utils, nth) {
    const char *teststr = "aaabbbcccaaatttaaa";
    EXPECT_EQ(find_Nth(teststr, 1, "aaa"), 0);
    EXPECT_EQ(find_Nth(teststr, 2, "aaa"), 9);
    EXPECT_EQ(find_Nth(teststr, 3, "aaa"), 15);
    EXPECT_EQ(find_Nth(teststr, 1, "abc"), -1);
    EXPECT_EQ(find_Nth(teststr, 1, ""), 0);
}

#define MD5TESTBUF "abcdefghijklmnopqrstuvwxyz\n"
TEST(Utils, md5) {
    MD5Calc m;
    m.Update(MD5TESTBUF, strlen(MD5TESTBUF));
    EXPECT_STREQ("e302f9ecd2d189fa80aac1c3392e9b9c", m.Get());
    m.Update(MD5TESTBUF, strlen(MD5TESTBUF));
    m.Update(MD5TESTBUF, strlen(MD5TESTBUF));
    m.Update(MD5TESTBUF, strlen(MD5TESTBUF));
    EXPECT_STREQ("3f8c2c6e2579e864071c33919fac61ee", m.Get());
}

#define TEST_STRING "The quick brown fox jumps over the lazy dog"
TEST(Utils, sha256) {
    char hash_str[65] = {0};
    EXPECT_TRUE(sha256_hex(TEST_STRING, hash_str));
    EXPECT_STREQ("d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592", hash_str);
}

TEST(Utils, sha1hmac) {
    char hash_hex[41] = {0};

    EXPECT_TRUE(sha1hmac_hex(TEST_STRING, (char *)hash_hex, "key", 3));

    EXPECT_STREQ("de7c9b85b8b78aa6bc8a7a36f70a90701c9db4d9", hash_hex);
}

TEST(Utils, sha256hmac) {
    char hash_str[65] = {0};
    EXPECT_TRUE(sha256hmac_hex(TEST_STRING, hash_str, "key", 3));
    EXPECT_STREQ("f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8", hash_str);
}

TEST(Utils, Config) {
    Config c("data/s3test.conf");
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

TEST(Utils, UriCoding) {
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

TEST(Utils, find_replace) {
    string str1 = "This is a simple & short test.";

    find_replace(str1, "simple", "");
    EXPECT_STREQ("This is a  & short test.", str1.c_str());

    find_replace(str1, "short ", "");
    EXPECT_STREQ("This is a  & test.", str1.c_str());

    find_replace(str1, "test.", "");
    EXPECT_STREQ("This is a  & ", str1.c_str());

    find_replace(str1, "This", "");
    EXPECT_STREQ(" is a  & ", str1.c_str());

    find_replace(str1, "is a", "abcdefghijklmn");
    EXPECT_STREQ(" abcdefghijklmn  & ", str1.c_str());

    find_replace(str1, " a", "a");
    EXPECT_STREQ("abcdefghijklmn  & ", str1.c_str());

    find_replace(str1, "abc", "abcabc");
    EXPECT_STREQ("abcabcdefghijklmn  & ", str1.c_str());
}
