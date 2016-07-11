#include "s3common.cpp"
#include "gtest/gtest.h"
#include "http_parser.cpp"

TEST(Common, SignRequestV4) {
    S3Credential cred = {"keyid/foo", "secret/bar"};

    HTTPHeaders *h = new HTTPHeaders();
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
    EXPECT_EQ("secret_test",
              get_opt_s3(string("s3://neverland.amazonaws.com secret=secret_test"), "secret"));

    EXPECT_EQ("\".\\!@#$%^&*()DFGHJK\"",
              get_opt_s3(string("s3://neverland.amazonaws.com accessid=\".\\!@#$%^&*()DFGHJK\""),
                         "accessid"));

    EXPECT_EQ("3456789",
              get_opt_s3(string("s3://neverland.amazonaws.com chunksize=3456789"), "chunksize"));

    EXPECT_EQ("secret_test",
              get_opt_s3(string("s3://neverland.amazonaws.com secret=secret_test "
                                "accessid=\".\\!@#$%^&*()DFGHJK\" chunksize=3456789"),
                         "secret"));

    EXPECT_EQ("\".\\!@#$%^&*()DFGHJK\"",
              get_opt_s3(string("s3://neverland.amazonaws.com secret=secret_test "
                                "accessid=\".\\!@#$%^&*()DFGHJK\" chunksize=3456789"),
                         "accessid"));

    EXPECT_EQ("3456789", get_opt_s3(string("s3://neverland.amazonaws.com secret=secret_test "
                                           "accessid=\".\\!@#$%^&*()DFGHJK\" chunksize=3456789"),
                                    "chunksize"));

    EXPECT_EQ("secret_test", get_opt_s3(string("s3://neverland.amazonaws.com secret=secret_test "
                                               "blah=whatever accessid=\".\\!@#$%^&*()DFGHJK\" "
                                               "chunksize=3456789 KingOfTheWorld=sanpang"),
                                        "secret"));

    EXPECT_EQ("secret_test", get_opt_s3(string("s3://neverland.amazonaws.com secret=secret_test "
                                               "blah= accessid=\".\\!@#$%^&*()DFGHJK\" "
                                               "chunksize=3456789 KingOfTheWorld=sanpang"),
                                        "secret"));

    EXPECT_EQ("3456789", get_opt_s3(string("s3://neverland.amazonaws.com secret=secret_test "
                                           "chunksize=3456789 KingOfTheWorld=sanpang "),
                                    "chunksize"));

    EXPECT_EQ("3456789", get_opt_s3(string("s3://neverland.amazonaws.com   secret=secret_test "
                                           "chunksize=3456789  KingOfTheWorld=sanpang "),
                                    "chunksize"));

    EXPECT_EQ("=sanpang", get_opt_s3(string("s3://neverland.amazonaws.com secret=secret_test "
                                            "chunksize=3456789 KingOfTheWorld==sanpang "),
                                     "KingOfTheWorld"));

    EXPECT_TRUE(get_opt_s3(string(""), "accessid").empty());

    EXPECT_TRUE(get_opt_s3(string("s3://neverland.amazonaws.com secret=secret_test "
                                  "accessid=\".\\!@#$%^&*()DFGHJK\" chunksize=3456789"),
                           "secret1")
                    .empty());
    EXPECT_TRUE(get_opt_s3(string("s3://neverland.amazonaws.com"), "secret").empty());

    EXPECT_TRUE(get_opt_s3(string("s3://neverland.amazonaws.com secret=secret_test blah=whatever "
                                  "accessid= chunksize=3456789 KingOfTheWorld=sanpang"),
                           "accessid")
                    .empty());

    EXPECT_TRUE(get_opt_s3(string("s3://neverland.amazonaws.com secret=secret_test blah=whatever "
                                  "chunksize=3456789 KingOfTheWorld=sanpang"),
                           "")
                    .empty());

    EXPECT_TRUE(get_opt_s3(string("s3://neverland.amazonaws.com secret=secret_test "
                                  "chunksize=3456789 KingOfTheWorld=sanpang "),
                           "chunk size")
                    .empty());
}

TEST(Common, TruncateOptions) {
    EXPECT_EQ("s3://neverland.amazonaws.com",
              truncate_options(string("s3://neverland.amazonaws.com secret=secret_test")));

    EXPECT_EQ(
        "s3://neverland.amazonaws.com",
        truncate_options(string("s3://neverland.amazonaws.com accessid=\".\\!@#$%^&*()DFGHJK\"")));

    EXPECT_EQ("s3://neverland.amazonaws.com",
              truncate_options(string("s3://neverland.amazonaws.com secret=secret_test "
                                      "accessid=\".\\!@#$%^&*()DFGHJK\" chunksize=3456789")));

    EXPECT_EQ("s3://neverland.amazonaws.com",
              truncate_options(string("s3://neverland.amazonaws.com secret=secret_test "
                                      "blah= accessid=\".\\!@#$%^&*()DFGHJK\" "
                                      "chunksize=3456789 KingOfTheWorld=sanpang")));
}

TEST(Common, EncodeQuery) {
    string src1 = "This is a simple & short test.";
    string src2 = "$ & < > ? ; # : = , \" ' ~ + %-_";

    string dst1 = "This%20is%20a%20simple%20&%20short%20test.";
    string dst2 =
        "%24%20&%20%3C%20%3E%20%3F%20%3B%20%23%20%3A%20=%20%2C%20%22%20%27%"
        "20~%20%2B%20%25-_";

    EXPECT_EQ(dst1.c_str(), encode_query_str(src1));
    EXPECT_EQ(dst2.c_str(), encode_query_str(src2));
}