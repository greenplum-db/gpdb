#include "s3url_parser.cpp"
#include "gtest/gtest.h"
#include "s3macros.h"

TEST(UrlParser, Routine) {
    UrlParser *p = new UrlParser("https://www.google.com/search?sclient=psy-ab&site=&source=hp");
    ASSERT_NE((void *)NULL, p);

    EXPECT_STREQ("https", p->Schema());
    EXPECT_STREQ("www.google.com", p->Host());
    EXPECT_STREQ("/search", p->Path());
    delete p;
}

TEST(UrlParser, LongURL) {
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

TEST(UrlParser, InvalidURL) {
    EXPECT_THROW(new UrlParser(NULL), std::runtime_error);

    EXPECT_THROW(new UrlParser("abc\\:"), std::runtime_error);
}
