#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>

#include "gtest/gtest.h"
#include "s3http_headers.h"
#include "s3restful_service.cpp"

using namespace std;

TEST(S3RESTfulService, GetWithWrongHeader) {
    HTTPHeaders headers;
    map<string, string> params;
    S3RESTfulService service;

    string url = "https://www.bing.com/";
    headers.Add(HOST, url);
    headers.Add(CONTENTTYPE, "plain/text");

    Response resp = service.get(url, headers, params);

    EXPECT_EQ(OK, resp.getStatus());
    EXPECT_EQ("Success", resp.getMessage());
    // 302 returned from bing.com
    EXPECT_EQ(true, resp.getRawData().size() < 500);
}

TEST(S3RESTfulService, DISABLED_GetWithEmptyHeader) {
    HTTPHeaders headers;
    map<string, string> params;
    string url;
    S3RESTfulService service;

    url = "https://www.bing.com/";

    Response resp = service.get(url, headers, params);

    EXPECT_EQ(OK, resp.getStatus());
    EXPECT_EQ("Success", resp.getMessage());
    EXPECT_EQ(true, resp.getRawData().size() > 10000);
}

TEST(S3RESTfulService, GetWithoutURL) {
    HTTPHeaders headers;
    map<string, string> params;
    string url;
    S3RESTfulService service;

    Response resp = service.get(url, headers, params);

    EXPECT_EQ(FAIL, resp.getStatus());
    EXPECT_EQ("failed to talk to s3 service URL using bad/illegal format or missing URL",
              resp.getMessage());
}

TEST(S3RESTfulService, GetWithWrongURL) {
    HTTPHeaders headers;
    map<string, string> params;
    string url;
    S3RESTfulService service;

    url = "https://www.bing.com/pivotal.html";

    Response resp = service.get(url, headers, params);

    EXPECT_EQ(OK, resp.getStatus());
    EXPECT_EQ("Success", resp.getMessage());

    // 404 returned from bing.com
    EXPECT_EQ(0, resp.getRawData().size());
}