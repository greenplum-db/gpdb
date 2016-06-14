#include "s3http_headers.cpp"
#include "gtest/gtest.h"

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

TEST(Common, HTTPHeaders) {
#define HOSTSTR "www.google.com"
#define RANGESTR "1-10000"
#define MD5STR "xxxxxxxxxxxxxxxxxxx"
    HTTPHeaders *h = new HTTPHeaders();
    ASSERT_NE((void *)NULL, h);

    h->CreateList();
    curl_slist *l = h->GetList();
    EXPECT_EQ((void *)NULL, l);
    h->FreeList();

    ASSERT_TRUE(h->Add(HOST, HOSTSTR));
    ASSERT_TRUE(h->Add(RANGE, RANGESTR));
    ASSERT_TRUE(h->Add(CONTENTMD5, MD5STR));

    EXPECT_STREQ(HOSTSTR, h->Get(HOST));
    EXPECT_STREQ(RANGESTR, h->Get(RANGE));
    EXPECT_STREQ(MD5STR, h->Get(CONTENTMD5));

    h->CreateList();
    l = h->GetList();
    ASSERT_NE((void *)NULL, l);
    h->FreeList();

    delete h;
}
