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

    EXPECT_STREQ("secret_test",
                 option = get_opt_s3(
                     "s3://neverland.amazonaws.com secret=secret_test "
                     "blah=whatever accessid=\".\\!@#$%^&*()DFGHJK\" "
                     "chunksize=3456789 KingOfTheWorld=sanpang",
                     "secret"));
    if (option) {
        free(option);
        option = NULL;
    }

    EXPECT_STREQ("secret_test",
                 option = get_opt_s3(
                     "s3://neverland.amazonaws.com secret=secret_test "
                     "blah= accessid=\".\\!@#$%^&*()DFGHJK\" "
                     "chunksize=3456789 KingOfTheWorld=sanpang",
                     "secret"));
    if (option) {
        free(option);
        option = NULL;
    }

    EXPECT_STREQ("3456789",
                 option = get_opt_s3(
                     "s3://neverland.amazonaws.com secret=secret_test "
                     "chunksize=3456789 KingOfTheWorld=sanpang ",
                     "chunksize"));
    if (option) {
        free(option);
        option = NULL;
    }

    EXPECT_STREQ("3456789",
                 option = get_opt_s3(
                     "s3://neverland.amazonaws.com   secret=secret_test "
                     "chunksize=3456789  KingOfTheWorld=sanpang ",
                     "chunksize"));
    if (option) {
        free(option);
        option = NULL;
    }

    EXPECT_STREQ("=sanpang",
                 option = get_opt_s3(
                     "s3://neverland.amazonaws.com secret=secret_test "
                     "chunksize=3456789 KingOfTheWorld==sanpang ",
                     "KingOfTheWorld"));
    if (option) {
        free(option);
        option = NULL;
    }

    EXPECT_THROW(get_opt_s3("", "accessid"), std::runtime_error);

    EXPECT_THROW(get_opt_s3(NULL, "accessid"), std::runtime_error);

    EXPECT_THROW(get_opt_s3("s3://neverland.amazonaws.com", "secret"),
                 std::runtime_error);

    EXPECT_THROW(get_opt_s3(
                     "s3://neverland.amazonaws.com secret=secret_test "
                     "blah=whatever accessid= chunksize=3456789 "
                     "KingOfTheWorld=sanpang",
                     "accessid"),
                 std::runtime_error);

    EXPECT_THROW(get_opt_s3(
                     "s3://neverland.amazonaws.com secret=secret_test "
                     "blah=whatever chunksize=3456789 KingOfTheWorld=sanpang",
                     ""),
                 std::runtime_error);

    EXPECT_THROW(get_opt_s3(
                     "s3://neverland.amazonaws.com secret=secret_test "
                     "blah=whatever chunksize=3456789 KingOfTheWorld=sanpang",
                     NULL),
                 std::runtime_error);

    EXPECT_THROW(get_opt_s3(
                     "s3://neverland.amazonaws.com secret=secret_test "
                     "chunksize=3456789 KingOfTheWorld=sanpang ",
                     "chunk size"),
                 std::runtime_error);
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
