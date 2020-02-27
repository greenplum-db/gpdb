#include "s3url.cpp"
#include "gtest/gtest.h"

TEST(S3UrlTest, Routine) {
    S3Url s3Url("https://www.google.com:8080/search?sclient=psy-ab&site=&source=hp", true);

    EXPECT_EQ("https", s3Url.getSchema());
    EXPECT_EQ("www.google.com", s3Url.getHost());
    EXPECT_EQ("www.google.com:8080", s3Url.getHostForCurl());
    EXPECT_EQ("8080", s3Url.getPort());
}

TEST(S3UrlTest, LongURL) {
    S3Url s3Url(
        "http://s3-us-west-2.amazonaws.com/metro.pivotal.io/test/"
        "data1234?partNumber=1&uploadId=."
        "CXn7YDXxGo7aDLxEyX5wxaDivCw5ACWfaMQts8_4M6."
        "NbGeeaI1ikYlO5zWZOpclVclZRAq5758oCxk_DtiX5BoyiMr7Ym6TKiEqqmNpsE-",
        false);

    EXPECT_EQ("http", s3Url.getSchema());
    EXPECT_EQ("s3-us-west-2.amazonaws.com", s3Url.getHost());
    EXPECT_EQ("s3-us-west-2.amazonaws.com", s3Url.getHostForCurl());
}

TEST(S3UrlTest, InvalidURL) {
    S3Url s3Url("");
    EXPECT_FALSE(s3Url.isValidUrl());

    EXPECT_THROW(new S3Url("abc\\:"), S3RuntimeError);
}

TEST(S3UrlTest, EmptyField) {
    S3Url s3Url("http://www.google.com");
    EXPECT_EQ("", s3Url.getBucket());
}

TEST(S3UrlTest, Normal) {
    S3Url s3Url("http://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/normal");

    EXPECT_EQ("us-west-2", s3Url.getRegion());
    EXPECT_EQ("s3test.pivotal.io", s3Url.getBucket());
    EXPECT_EQ("dataset1/normal", s3Url.getPrefix());
}

TEST(S3UrlTest, SpecialCharacters) {
    S3Url s3Url("http://s3-us-west-2.amazonaws.com/s3test.pivotal.io/?:&/=+");

    EXPECT_EQ("/s3test.pivotal.io/%3F%3A%26/%3D%2B", s3Url.getPathForCurl());
}

TEST(S3UrlTest, NoPrefixAndSlash) {
    S3Url s3Url("http://s3-us-west-2.amazonaws.com/s3test.pivotal.io");

    EXPECT_EQ("/s3test.pivotal.io/", s3Url.getPathForCurl());
    EXPECT_EQ("s3test.pivotal.io", s3Url.getBucket());
    EXPECT_EQ("", s3Url.getPrefix());
}

TEST(S3UrlTest, NoPrefix) {
    S3Url s3Url("http://s3-us-west-2.amazonaws.com/s3test.pivotal.io/");

    EXPECT_EQ("/s3test.pivotal.io/", s3Url.getPathForCurl());
    EXPECT_EQ("s3test.pivotal.io", s3Url.getBucket());
    EXPECT_EQ("", s3Url.getPrefix());
}

TEST(S3UrlTest, Region_default) {
    S3Url s3Url("http://s3.amazonaws.com/s3test.pivotal.io/dataset1/normal");

    EXPECT_EQ("external-1", s3Url.getRegion());
    EXPECT_EQ("s3test.pivotal.io", s3Url.getBucket());
    EXPECT_EQ("dataset1/normal", s3Url.getPrefix());
}

TEST(S3UrlTest, Region_useast1) {
    S3Url s3Url("http://s3-us-east-1.amazonaws.com/s3test.pivotal.io/dataset1/normal");
    EXPECT_EQ("external-1", s3Url.getRegion());
    EXPECT_EQ("s3test.pivotal.io", s3Url.getBucket());
    EXPECT_EQ("dataset1/normal", s3Url.getPrefix());
}

TEST(S3UrlTest, Region_eucentral1) {
    S3Url s3Url("http://s3.eu-central-1.amazonaws.com/s3test.pivotal.io/dataset1/normal");
    EXPECT_EQ("eu-central-1", s3Url.getRegion());
    EXPECT_EQ("s3test.pivotal.io", s3Url.getBucket());
    EXPECT_EQ("dataset1/normal", s3Url.getPrefix());
}

TEST(S3UrlTest, Region_apnortheast2) {
    S3Url s3Url("http://s3.ap-northeast-2.amazonaws.com/s3test.pivotal.io/dataset1/normal");
    EXPECT_EQ("ap-northeast-2", s3Url.getRegion());
    EXPECT_EQ("s3test.pivotal.io", s3Url.getBucket());
    EXPECT_EQ("dataset1/normal", s3Url.getPrefix());
}

TEST(S3UrlTest, Extension) {
    S3Url s3Url("http://s3-us-west-2.amazonaws.com/bucket/");
    EXPECT_EQ("", s3Url.getPrefix());
    EXPECT_EQ("", s3Url.getExtension());
    s3Url.setPrefix("abc");
    EXPECT_EQ("", s3Url.getExtension());
    s3Url.setPrefix("a.b.c");
    EXPECT_EQ(".c", s3Url.getExtension());
    s3Url.setPrefix("/");
    EXPECT_EQ("", s3Url.getExtension());
    s3Url.setPrefix("//");
    EXPECT_EQ("", s3Url.getExtension());
    s3Url.setPrefix("./");
    EXPECT_EQ("", s3Url.getExtension());
    s3Url.setPrefix("/a.b/");
    EXPECT_EQ("", s3Url.getExtension());
    s3Url.setPrefix("/.");
    EXPECT_EQ(".", s3Url.getExtension());
    s3Url.setPrefix("a.b");
    EXPECT_EQ(".b", s3Url.getExtension());
    s3Url.setPrefix("/a.b");
    EXPECT_EQ(".b", s3Url.getExtension());
    s3Url.setPrefix("ab/a.b");
    EXPECT_EQ(".b", s3Url.getExtension());
}
