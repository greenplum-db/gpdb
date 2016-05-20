#include "s3common.cpp"
#include "gtest/gtest.h"
#include "http_parser.cpp"

TEST(Common, UrlParser) {
    UrlParser *p = new UrlParser(
        "https://www.google.com/search?sclient=psy-ab&site=&source=hp");
    ASSERT_NE((void *)NULL, p);

    EXPECT_STREQ("https", p->Schema());
    EXPECT_STREQ("www.google.com", p->Host());
    EXPECT_STREQ("/search", p->Path());
    delete p;
}

TEST(Common, UrlParser_LongURL) {
    UrlParser *p = new UrlParser(
        "http://s3-us-west-2.amazonaws.com/metro.pivotal.io/test/"
        "data1234?partNumber=1&uploadId=."
        "CXn7YDXxGo7aDLxEyX5wxaDivCw5ACWfaMQts8_4M6."
        "NbGeeaI1ikYlO5zWZOpclVclZRAq5758oCxk_DtiX5BoyiMr7Ym6TKiEqqmNpsE-");
    ASSERT_NE((void *)NULL, p);

    EXPECT_STREQ("http", p->Schema());
    EXPECT_STREQ("s3-us-west-2.amazonaws.com", p->Host());
    EXPECT_STREQ("/metro.pivotal.io/test/data1234", p->Path());
    delete p;
}

TEST(Common, GetFieldString) {
    EXPECT_STREQ("Host", GetFieldString(HOST));
    EXPECT_STREQ("Range", GetFieldString(RANGE));
    EXPECT_STREQ("Date", GetFieldString(DATE));
    EXPECT_STREQ("Content-Length", GetFieldString(CONTENTLENGTH));
    EXPECT_STREQ("Content-MD5", GetFieldString(CONTENTMD5));
    EXPECT_STREQ("Content-Type", GetFieldString(CONTENTTYPE));
    EXPECT_STREQ("Expect", GetFieldString(EXPECT));
    EXPECT_STREQ("Authorization", GetFieldString(AUTHORIZATION));
    EXPECT_STREQ("ETag", GetFieldString(ETAG));
    EXPECT_STREQ("x-amz-date", GetFieldString(X_AMZ_DATE));
    EXPECT_STREQ("x-amz-content-sha256", GetFieldString(X_AMZ_CONTENT_SHA256));
}

TEST(Common, HeaderContent) {
#define HOSTSTR "www.google.com"
#define RANGESTR "1-10000"
#define MD5STR "xxxxxxxxxxxxxxxxxxx"
    HeaderContent *h = new HeaderContent();
    ASSERT_NE((void *)NULL, h);

    ASSERT_TRUE(h->Add(HOST, HOSTSTR));
    ASSERT_TRUE(h->Add(RANGE, RANGESTR));
    ASSERT_TRUE(h->Add(CONTENTMD5, MD5STR));

    EXPECT_STREQ(HOSTSTR, h->Get(HOST));
    EXPECT_STREQ(RANGESTR, h->Get(RANGE));
    EXPECT_STREQ(MD5STR, h->Get(CONTENTMD5));

    h->CreateList();
    curl_slist *l = h->GetList();
    ASSERT_NE((void *)NULL, l);
    h->FreeList();

    delete h;
}

TEST(Common, SignRequestV4) {
    S3Credential cred = {"keyid/foo", "secret/bar"};

    HeaderContent *h = new HeaderContent();
    ASSERT_NE((void *)NULL, h);

    ASSERT_TRUE(h->Add(HOST, "iam.amazonaws.com"));
    ASSERT_TRUE(h->Add(X_AMZ_DATE, "20150830T123600Z"));
    ASSERT_TRUE(h->Add(X_AMZ_CONTENT_SHA256, "UNSIGNED-PAYLOAD"));

    SignRequestV4("GET", h, "us-east-1", "/where/ever",
                  "?parameter1=whatever1&parameter2=whatever2", cred);

    EXPECT_STREQ(
        "AWS4-HMAC-SHA256 "
        "Credential=keyid/foo/20150830/us-east-1/s3/"
        "aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date,"
        "Signature="
        "9f500a13e81c2dc6cb47551e416b2734e401d7b7b8f7ae99b09bccc22b81132d",
        h->Get(AUTHORIZATION));

    delete h;
}

TEST(Common, UrlOptions) {
    char *option = NULL;
    EXPECT_STREQ(
        "secret_test",
        option = get_opt_s3("s3://neverland.amazonaws.com secret=secret_test",
                            "secret"));
    if (option) {
        free(option);
        option = NULL;
    }

    EXPECT_STREQ(
        "\".\\!@#$%^&*()DFGHJK\"",
        option = get_opt_s3(
            "s3://neverland.amazonaws.com accessid=\".\\!@#$%^&*()DFGHJK\"",
            "accessid"));
    if (option) {
        free(option);
        option = NULL;
    }

    EXPECT_STREQ(
        "3456789",
        option = get_opt_s3("s3://neverland.amazonaws.com chunksize=3456789",
                            "chunksize"));
    if (option) {
        free(option);
        option = NULL;
    }

    EXPECT_STREQ("secret_test",
                 option = get_opt_s3(
                     "s3://neverland.amazonaws.com secret=secret_test "
                     "accessid=\".\\!@#$%^&*()DFGHJK\" chunksize=3456789",
                     "secret"));
    if (option) {
        free(option);
        option = NULL;
    }

    EXPECT_STREQ("\".\\!@#$%^&*()DFGHJK\"",
                 option = get_opt_s3(
                     "s3://neverland.amazonaws.com secret=secret_test "
                     "accessid=\".\\!@#$%^&*()DFGHJK\" chunksize=3456789",
                     "accessid"));
    if (option) {
        free(option);
        option = NULL;
    }

    EXPECT_STREQ("3456789",
                 option = get_opt_s3(
                     "s3://neverland.amazonaws.com secret=secret_test "
                     "accessid=\".\\!@#$%^&*()DFGHJK\" chunksize=3456789",
                     "chunksize"));
    if (option) {
        free(option);
        option = NULL;
    }

    EXPECT_STREQ(
        "secret_test",
        option = get_opt_s3("s3://neverland.amazonaws.com secret=secret_test "
                            "blah=whatever accessid=\".\\!@#$%^&*()DFGHJK\" "
                            "chunksize=3456789 KingOfTheWorld=sanpang",
                            "secret"));
    if (option) {
        free(option);
        option = NULL;
    }

    EXPECT_STREQ(
        "secret_test",
        option = get_opt_s3("s3://neverland.amazonaws.com secret=secret_test "
                            "blah= accessid=\".\\!@#$%^&*()DFGHJK\" "
                            "chunksize=3456789 KingOfTheWorld=sanpang",
                            "secret"));
    if (option) {
        free(option);
        option = NULL;
    }

    EXPECT_STREQ(
        "3456789",
        option = get_opt_s3("s3://neverland.amazonaws.com secret=secret_test "
                            "chunksize=3456789 KingOfTheWorld=sanpang ",
                            "chunksize"));
    if (option) {
        free(option);
        option = NULL;
    }

    EXPECT_STREQ(
        "3456789",
        option = get_opt_s3("s3://neverland.amazonaws.com   secret=secret_test "
                            "chunksize=3456789  KingOfTheWorld=sanpang ",
                            "chunksize"));
    if (option) {
        free(option);
        option = NULL;
    }

    EXPECT_STREQ(
        "=sanpang",
        option = get_opt_s3("s3://neverland.amazonaws.com secret=secret_test "
                            "chunksize=3456789 KingOfTheWorld==sanpang ",
                            "KingOfTheWorld"));
    if (option) {
        free(option);
        option = NULL;
    }

    EXPECT_EQ((char *)NULL, option = get_opt_s3("", "accessid"));

    EXPECT_EQ((char *)NULL, option = get_opt_s3(NULL, "accessid"));

    EXPECT_EQ((char *)NULL,
              option = get_opt_s3("s3://neverland.amazonaws.com", "secret"));

    EXPECT_EQ(
        (char *)NULL,
        option = get_opt_s3("s3://neverland.amazonaws.com secret=secret_test "
                            "blah=whatever accessid= chunksize=3456789 "
                            "KingOfTheWorld=sanpang",
                            "accessid"));

    EXPECT_EQ((char *)NULL,
              option = get_opt_s3(
                  "s3://neverland.amazonaws.com secret=secret_test "
                  "blah=whatever chunksize=3456789 KingOfTheWorld=sanpang",
                  ""));

    EXPECT_EQ((char *)NULL,
              option = get_opt_s3(
                  "s3://neverland.amazonaws.com secret=secret_test "
                  "blah=whatever chunksize=3456789 KingOfTheWorld=sanpang",
                  NULL));

    EXPECT_EQ(
        (char *)NULL,
        option = get_opt_s3("s3://neverland.amazonaws.com secret=secret_test "
                            "chunksize=3456789 KingOfTheWorld=sanpang ",
                            "chunk size"));
}

TEST(Common, TruncateOptions) {
    char *truncated = NULL;

    EXPECT_STREQ("s3://neverland.amazonaws.com",
                 truncated = truncate_options(
                     "s3://neverland.amazonaws.com secret=secret_test"));
    if (truncated) {
        free(truncated);
        truncated = NULL;
    }

    EXPECT_STREQ(
        "s3://neverland.amazonaws.com",
        truncated = truncate_options(
            "s3://neverland.amazonaws.com accessid=\".\\!@#$%^&*()DFGHJK\""));
    if (truncated) {
        free(truncated);
        truncated = NULL;
    }

    EXPECT_STREQ("s3://neverland.amazonaws.com",
                 truncated = truncate_options(
                     "s3://neverland.amazonaws.com secret=secret_test "
                     "accessid=\".\\!@#$%^&*()DFGHJK\" chunksize=3456789"));
    if (truncated) {
        free(truncated);
        truncated = NULL;
    }

    EXPECT_STREQ("s3://neverland.amazonaws.com",
                 truncated = truncate_options(
                     "s3://neverland.amazonaws.com secret=secret_test "
                     "blah= accessid=\".\\!@#$%^&*()DFGHJK\" "
                     "chunksize=3456789 KingOfTheWorld=sanpang"));
    if (truncated) {
        free(truncated);
        truncated = NULL;
    }
}

TEST(Common, EncodeQuery) {
    string src1 = "This is a simple & short test.";
    string src2 = "$ & < > ? ; # : = , \" ' ~ + %-_";

    string dst1 = "This%20is%20a%20simple%20&%20short%20test.";
    string dst2 =
        "%24%20&%20%3C%20%3E%20%3F%20%3B%20%23%20%3A%20=%20%2C%20%22%20%27%"
        "20~%20%2B%20%25-_";

    EXPECT_STREQ(dst1.c_str(), encode_query_str(src1).c_str());
    EXPECT_STREQ(dst2.c_str(), encode_query_str(src2).c_str());
}

TEST(Common, ThreadFunctions) {
    // just to test if these two are functional
    thread_setup();
    EXPECT_NE((void *)NULL, mutex_buf);

    thread_cleanup();
    EXPECT_EQ((void *)NULL, mutex_buf);

    EXPECT_NE(0, id_function());
}
