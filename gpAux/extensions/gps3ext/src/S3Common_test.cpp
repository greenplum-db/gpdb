#include "gtest/gtest.h"
#include "http_parser.cpp"
#include "S3Common.cpp"

TEST(S3Common, UrlParser) {
    UrlParser *p = new UrlParser(
        "https://www.google.com/search?sclient=psy-ab&site=&source=hp");
    EXPECT_NE((void *)NULL, p);

    if (p) {
        EXPECT_STREQ("https", p->Schema());
        EXPECT_STREQ("www.google.com", p->Host());
        EXPECT_STREQ("/search", p->Path());
        delete p;
    }
}

TEST(S3Common, UrlParser_LongURL) {
    UrlParser *p = new UrlParser(
        "http://s3-us-west-2.amazonaws.com/metro.pivotal.io/test/"
        "data1234?partNumber=1&uploadId=."
        "CXn7YDXxGo7aDLxEyX5wxaDivCw5ACWfaMQts8_4M6."
        "NbGeeaI1ikYlO5zWZOpclVclZRAq5758oCxk_DtiX5BoyiMr7Ym6TKiEqqmNpsE-");
    EXPECT_NE((void *)NULL, p);

    if (p) {
        EXPECT_STREQ("http", p->Schema());
        EXPECT_STREQ("s3-us-west-2.amazonaws.com", p->Host());
        EXPECT_STREQ("/metro.pivotal.io/test/data1234", p->Path());
        delete p;
    }
}

#define HOSTSTR "www.google.com"
#define RANGESTR "1-10000"
#define MD5STR "xxxxxxxxxxxxxxxxxxx"

TEST(S3Common, HeaderContent) {
    HeaderContent *h = new HeaderContent();
    EXPECT_NE((void *)NULL, h);

    // test Add
    if (h) {
        ASSERT_TRUE(h->Add(HOST, HOSTSTR));
        ASSERT_TRUE(h->Add(RANGE, RANGESTR));
        ASSERT_TRUE(h->Add(CONTENTMD5, MD5STR));
    }

// test Get
#if 0
    if (h) {
        EXPECT_STREQ(HOSTSTR, h->Get(HOST));
        EXPECT_STREQ(RANGESTR, h->Get(RANGE));
        EXPECT_STREQ(MD5STR, h->Get(CONTENTMD5));
    }
#endif
    // test GetList
    if (h) {
        curl_slist *l = h->GetList();
        ASSERT_NE((void *)NULL, l);
        if (l) {
            curl_slist_free_all(l);
        }
    }
    if (h) delete h;
}

TEST(S3Common, UrlOptions) {
    EXPECT_STREQ("secret_test",
                 get_opt_s3("s3://neverland.amazonaws.com secret=secret_test",
                            "secret"));

    EXPECT_STREQ(
        "\".\\!@#$%^&*()DFGHJK\"",
        get_opt_s3(
            "s3://neverland.amazonaws.com accessid=\".\\!@#$%^&*()DFGHJK\"",
            "accessid"));

    EXPECT_STREQ("3456789",
                 get_opt_s3("s3://neverland.amazonaws.com chunksize=3456789",
                            "chunksize"));

    EXPECT_STREQ(
        "secret_test",
        get_opt_s3("s3://neverland.amazonaws.com secret=secret_test "
                   "accessid=\".\\!@#$%^&*()DFGHJK\" chunksize=3456789",
                   "secret"));

    EXPECT_STREQ(
        "\".\\!@#$%^&*()DFGHJK\"",
        get_opt_s3("s3://neverland.amazonaws.com secret=secret_test "
                   "accessid=\".\\!@#$%^&*()DFGHJK\" chunksize=3456789",
                   "accessid"));

    EXPECT_STREQ(
        "3456789",
        get_opt_s3("s3://neverland.amazonaws.com secret=secret_test "
                   "accessid=\".\\!@#$%^&*()DFGHJK\" chunksize=3456789",
                   "chunksize"));

    EXPECT_STREQ("secret_test",
                 get_opt_s3("s3://neverland.amazonaws.com secret=secret_test "
                            "blah=whatever accessid=\".\\!@#$%^&*()DFGHJK\" "
                            "chunksize=3456789 KingOfTheWorld=sanpang",
                            "secret"));

    EXPECT_STREQ("secret_test",
                 get_opt_s3("s3://neverland.amazonaws.com secret=secret_test "
                            "blah= accessid=\".\\!@#$%^&*()DFGHJK\" "
                            "chunksize=3456789 KingOfTheWorld=sanpang",
                            "secret"));

    EXPECT_STREQ("3456789",
                 get_opt_s3("s3://neverland.amazonaws.com secret=secret_test "
                            "chunksize=3456789 KingOfTheWorld=sanpang ",
                            "chunksize"));

    EXPECT_STREQ("3456789",
                 get_opt_s3("s3://neverland.amazonaws.com   secret=secret_test "
                            "chunksize=3456789  KingOfTheWorld=sanpang ",
                            "chunksize"));

    EXPECT_STREQ("=sanpang",
                 get_opt_s3("s3://neverland.amazonaws.com secret=secret_test "
                            "chunksize=3456789 KingOfTheWorld==sanpang ",
                            "KingOfTheWorld"));

    EXPECT_EQ((char *)NULL, get_opt_s3("", "accessid"));

    EXPECT_EQ((char *)NULL, get_opt_s3(NULL, "accessid"));

    EXPECT_EQ((char *)NULL,
              get_opt_s3("s3://neverland.amazonaws.com", "secret"));

    EXPECT_EQ((char *)NULL,
              get_opt_s3("s3://neverland.amazonaws.com secret=secret_test "
                         "blah=whatever accessid= chunksize=3456789 "
                         "KingOfTheWorld=sanpang",
                         "accessid"));

    EXPECT_EQ(
        (char *)NULL,
        get_opt_s3("s3://neverland.amazonaws.com secret=secret_test "
                   "blah=whatever chunksize=3456789 KingOfTheWorld=sanpang",
                   ""));

    EXPECT_EQ(
        (char *)NULL,
        get_opt_s3("s3://neverland.amazonaws.com secret=secret_test "
                   "blah=whatever chunksize=3456789 KingOfTheWorld=sanpang",
                   NULL));

    EXPECT_EQ((char *)NULL,
              get_opt_s3("s3://neverland.amazonaws.com secret=secret_test "
                         "chunksize=3456789 KingOfTheWorld=sanpang ",
                         "chunk size"));
}

TEST(S3Common, TruncateOptions) {
    EXPECT_STREQ(
        "s3://neverland.amazonaws.com",
        truncate_options("s3://neverland.amazonaws.com secret=secret_test"));

    EXPECT_STREQ(
        "s3://neverland.amazonaws.com",
        truncate_options(
            "s3://neverland.amazonaws.com accessid=\".\\!@#$%^&*()DFGHJK\""));

    EXPECT_STREQ(
        "s3://neverland.amazonaws.com",
        truncate_options("s3://neverland.amazonaws.com secret=secret_test "
                         "accessid=\".\\!@#$%^&*()DFGHJK\" chunksize=3456789"));

    EXPECT_STREQ(
        "s3://neverland.amazonaws.com",
        truncate_options("s3://neverland.amazonaws.com secret=secret_test "
                         "blah= accessid=\".\\!@#$%^&*()DFGHJK\" "
                         "chunksize=3456789 KingOfTheWorld=sanpang"));
}
