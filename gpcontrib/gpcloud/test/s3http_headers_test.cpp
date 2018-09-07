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
    EXPECT_STREQ("x-amz-server-side-encryption", GetFieldString(X_AMZ_SERVER_SIDE_ENCRYPTION));
    EXPECT_STREQ("Unknown", GetFieldString((HeaderField)INT_MAX));
}

TEST(Common, HTTPHeaders) {
#define HOSTSTR "www.google.com"
#define RANGESTR "1-10000"
#define MD5STR "xxxxxxxxxxxxxxxxxxx"
    HTTPHeaders headers;

    headers.CreateList();
    curl_slist *headersList = headers.GetList();
    EXPECT_EQ((void *)NULL, headersList);
    headers.FreeList();

    ASSERT_FALSE(headers.Add(HOST, ""));
    ASSERT_TRUE(headers.Add(HOST, HOSTSTR));
    ASSERT_TRUE(headers.Add(RANGE, RANGESTR));
    ASSERT_TRUE(headers.Add(CONTENTMD5, MD5STR));

    EXPECT_STREQ(HOSTSTR, headers.Get(HOST));
    EXPECT_STREQ(RANGESTR, headers.Get(RANGE));
    EXPECT_STREQ(MD5STR, headers.Get(CONTENTMD5));

    headers.CreateList();
    headersList = headers.GetList();
    ASSERT_NE((void *)NULL, headersList);

    EXPECT_STREQ(headersList->data, "Host: www.google.com");
    headersList = headersList->next;

    EXPECT_STREQ(headersList->data, "Range: 1-10000");
    headersList = headersList->next;

    EXPECT_STREQ(headersList->data, "Content-MD5: xxxxxxxxxxxxxxxxxxx");
    headersList = headersList->next;

    EXPECT_EQ((void *)NULL, headersList);

    headers.FreeList();
}

TEST(Common, HTTPHeadersDisable) {
    HTTPHeaders headers;
    headers.Disable(CONTENTLENGTH);

    headers.CreateList();
    curl_slist *headersList = headers.GetList();

    ASSERT_NE((void *)NULL, headersList);

    EXPECT_STREQ(headersList->data, "Content-Length:");

    headersList = headersList->next;
    EXPECT_EQ((void *)NULL, headersList);

    headers.FreeList();
}
