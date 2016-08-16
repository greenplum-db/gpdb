#include "s3url.cpp"
#include "gtest/gtest.h"
#include "s3macros.h"

TEST(UrlParser, Routine) {
    UrlParser p("https://www.google.com/search?sclient=psy-ab&site=&source=hp");

    EXPECT_EQ("https", p.getSchema());
    EXPECT_EQ("www.google.com", p.getHost());
    EXPECT_EQ("/search", p.getPath());
}

TEST(UrlParser, LongURL) {
    UrlParser p(
        "http://s3-us-west-2.amazonaws.com/metro.pivotal.io/test/"
        "data1234?partNumber=1&uploadId=."
        "CXn7YDXxGo7aDLxEyX5wxaDivCw5ACWfaMQts8_4M6."
        "NbGeeaI1ikYlO5zWZOpclVclZRAq5758oCxk_DtiX5BoyiMr7Ym6TKiEqqmNpsE-");

    EXPECT_EQ("http", p.getSchema());
    EXPECT_EQ("s3-us-west-2.amazonaws.com", p.getHost());
    EXPECT_EQ("/metro.pivotal.io/test/data1234", p.getPath());
}

TEST(UrlParser, InvalidURL) {
    EXPECT_THROW(new UrlParser(string()), std::runtime_error);

    EXPECT_THROW(new UrlParser("abc\\:"), std::runtime_error);
}

TEST(UrlParser, EmptyField) {
    UrlParser p("http://www.google.com");
    EXPECT_EQ("", p.getQuery());
}
