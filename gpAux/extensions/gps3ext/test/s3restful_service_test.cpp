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
    S3Params params;
    S3RESTfulService service;

    string url = "https://www.bing.com/";
    headers.Add(HOST, url);
    headers.Add(CONTENTTYPE, "plain/text");

    Response resp = service.get(url, headers, params);

    EXPECT_EQ(RESPONSE_ERROR, resp.getStatus());
}

TEST(S3RESTfulService, GetWithEmptyHeader) {
    HTTPHeaders headers;
    S3Params params;
    string url;
    S3RESTfulService service;

    url = "https://www.bing.com/";

    Response resp = service.get(url, headers, params);

    EXPECT_EQ(RESPONSE_OK, resp.getStatus());
    EXPECT_EQ("Success", resp.getMessage());
    EXPECT_EQ(true, resp.getRawData().size() > 10000);
}

TEST(S3RESTfulService, GetWithoutURL) {
    HTTPHeaders headers;
    S3Params params;
    string url;
    S3RESTfulService service;

    Response resp = service.get(url, headers, params);

    EXPECT_EQ(RESPONSE_FAIL, resp.getStatus());
    EXPECT_EQ("Server connection failed: URL using bad/illegal format or missing URL",
              resp.getMessage());
}

TEST(S3RESTfulService, GetWithWrongURL) {
    HTTPHeaders headers;
    S3Params params;
    string url;
    S3RESTfulService service;

    url = "https://www.bing.com/pivotal.html";

    Response resp = service.get(url, headers, params);

    EXPECT_EQ(RESPONSE_ERROR, resp.getStatus());
}

TEST(S3RESTfulService, GetWithoutURLWithDebugParam) {
    HTTPHeaders headers;
    S3Params params;
    params.setDebugCurl(true);

    string url;
    S3RESTfulService service;

    Response resp = service.get(url, headers, params);

    EXPECT_EQ(RESPONSE_FAIL, resp.getStatus());
    EXPECT_EQ("Server connection failed: URL using bad/illegal format or missing URL",
              resp.getMessage());
}

TEST(S3RESTfulService, PutWithoutURL) {
    HTTPHeaders headers;
    S3Params params;
    string url;
    S3RESTfulService service;
    vector<uint8_t> data;

    Response resp = service.put(url, headers, params, data);

    EXPECT_EQ(RESPONSE_FAIL, resp.getStatus());
    EXPECT_EQ("Server connection failed: URL using bad/illegal format or missing URL",
              resp.getMessage());
}

TEST(S3RESTfulService, PutToServerWithBlindPutService) {
    HTTPHeaders headers;
    S3Params params;
    string url;
    S3RESTfulService service;

    /* data = "abcdefghij", len = 11 (including '\0') */
    vector<uint8_t> data;
    for (int i = 0; i < 10; i++) data.push_back('a' + i);
    data.push_back(0);

    url = "https://www.bing.com";

    Response resp = service.put(url, headers, params, data);

    EXPECT_EQ(RESPONSE_OK, resp.getStatus());
}

TEST(S3RESTfulService, PutToServerWith404Page) {
    HTTPHeaders headers;
    S3Params params;
    string url;
    S3RESTfulService service;

    /* data = "abcdefghij", len = 11 (including '\0') */
    vector<uint8_t> data;
    for (int i = 0; i < 10; i++) data.push_back('a' + i);
    data.push_back(0);

    url = "https://www.bing.com/pivotal.html";

    Response resp = service.put(url, headers, params, data);

    EXPECT_EQ(RESPONSE_ERROR, resp.getStatus());
    EXPECT_EQ("S3 server returned error, error code is 404", resp.getMessage());
}

TEST(S3RESTfulService, PutWithoutURLWithDebugParam) {
    HTTPHeaders headers;
    S3Params params;
    params.setDebugCurl(true);

    string url;
    S3RESTfulService service;
    vector<uint8_t> data;

    Response resp = service.put(url, headers, params, data);

    EXPECT_EQ(RESPONSE_FAIL, resp.getStatus());
    EXPECT_EQ("Server connection failed: URL using bad/illegal format or missing URL",
              resp.getMessage());
}

/*
 * The reason we define our vector-compare function is because:
 *   we may suffer from the Segment fault error when using std::equal() for comparison
 */
template <typename T>
bool compareVector(const vector<T>& a, const vector<T>& b) {
    if (a.size() != b.size()) return false;

    for (size_t i = 0; i < a.size(); i++) {
        if (a[i] != b[i]) return false;
    }

    return true;
}

/* Run './bin/dummyHTTPServer.py' before enabling this test */
TEST(S3RESTfulService, DISABLED_PutToDummyServer) {
    HTTPHeaders headers;
    S3Params params;
    string url;
    S3RESTfulService service;

    /* data = "abcdefghij", length = 11 (including '\0') */
    vector<uint8_t> data;
    for (int i = 0; i < 10; i++) data.push_back('a' + i);
    data.push_back(0);

    headers.Add(CONTENTTYPE, "text/plain");
    headers.Add(CONTENTLENGTH, std::to_string((unsigned long long)data.size()));

    url = "http://localhost:8553";

    Response resp = service.put(url, headers, params, data);
    EXPECT_EQ(RESPONSE_OK, resp.getStatus());
    EXPECT_TRUE(compareVector(data, resp.getRawData()));
}

TEST(S3RESTfulService, HeadWithoutURL) {
    HTTPHeaders headers;
    S3Params params;
    string url;
    S3RESTfulService service;

    ResponseCode code = service.head(url, headers, params);

    EXPECT_EQ(-1, code);
}

TEST(S3RESTfulService, HeadWithCorrectURLAndDebugParam) {
    HTTPHeaders headers;
    S3Params params;
    params.setDebugCurl(true);

    string url;
    S3RESTfulService service;

    url = "https://www.bing.com/";

    ResponseCode code = service.head(url, headers, params);

    EXPECT_EQ(200, code);
}

TEST(S3RESTfulService, HeadWithWrongURL) {
    HTTPHeaders headers;
    S3Params params;
    string url;
    S3RESTfulService service;

    url = "https://www.bing.com/pivotal.html";

    ResponseCode code = service.head(url, headers, params);

    EXPECT_EQ(404, code);
}

TEST(S3RESTfulService, HeadWithCorrectURL) {
    HTTPHeaders headers;
    S3Params params;
    string url;
    S3RESTfulService service;

    url = "https://www.bing.com/";

    ResponseCode code = service.head(url, headers, params);

    EXPECT_EQ(200, code);
}

TEST(S3RESTfulService, PostWithoutURL) {
    HTTPHeaders headers;
    S3Params params;
    string url;
    S3RESTfulService service;

    Response resp = service.post(url, headers, params, vector<uint8_t>());

    EXPECT_EQ(RESPONSE_FAIL, resp.getStatus());
    EXPECT_EQ("Server connection failed: URL using bad/illegal format or missing URL",
              resp.getMessage());
}

TEST(S3RESTfulService, PostToServerWithBlindPutServiceAndDebugParam) {
    HTTPHeaders headers;
    S3Params params;
    params.setDebugCurl(true);

    string url;
    S3RESTfulService service;

    headers.Add(CONTENTLENGTH, "3");
    url = "https://www.bing.com/?abcdefghij";

    Response resp = service.post(url, headers, params, vector<uint8_t>({1, 2, 3}));

    EXPECT_EQ(RESPONSE_OK, resp.getStatus());
}

TEST(S3RESTfulService, PostToServerWithBlindPutService) {
    HTTPHeaders headers;
    S3Params params;
    string url;
    S3RESTfulService service;

    url = "https://www.bing.com/?abcdefghij";

    headers.Add(CONTENTLENGTH, "3");
    Response resp = service.post(url, headers, params, vector<uint8_t>({1, 2, 3}));

    EXPECT_EQ(RESPONSE_OK, resp.getStatus());
}

TEST(S3RESTfulService, PostToServerWith404Page) {
    HTTPHeaders headers;
    S3Params params;
    string url;
    S3RESTfulService service;

    url = "https://www.bing.com/pivotal.html/?abcdefghij";

    headers.Add(CONTENTLENGTH, "3");
    Response resp = service.post(url, headers, params, vector<uint8_t>({1, 2, 3}));

    EXPECT_EQ(RESPONSE_ERROR, resp.getStatus());
    EXPECT_EQ("S3 server returned error, error code is 404", resp.getMessage());
}

TEST(S3RESTfulService, PostToServerWithData) {
    HTTPHeaders headers;
    S3Params params;
    string url;
    S3RESTfulService service;

    url = "https://www.bing.com";

    /* data = "abcdefghij", length = 11 (including '\0') */
    vector<uint8_t> data;
    for (int i = 0; i < 10; i++) data.push_back('a' + i);
    data.push_back(0);

    headers.Add(CONTENTTYPE, "text/plain");
    headers.Add(CONTENTLENGTH, std::to_string(data.size()));

    Response resp = service.post(url, headers, params, data);

    EXPECT_EQ(RESPONSE_OK, resp.getStatus());
}

/* Run './bin/dummyHTTPServer.py' before enabling this test */
TEST(S3RESTfulService, DISABLED_PostToDummyServer) {
    HTTPHeaders headers;
    S3Params params;
    string url;
    S3RESTfulService service;

    url = "http://localhost:8553/?abcdefghij";

    Response resp = service.post(url, headers, params, vector<uint8_t>());
    EXPECT_EQ(RESPONSE_OK, resp.getStatus());
    EXPECT_EQ("abcdefghij", string(resp.getRawData().begin(), resp.getRawData().end()));
}

/* Run './bin/dummyHTTPServer.py' before enabling this test */
TEST(S3RESTfulService, DISABLED_PostToDummyServerWithData) {
    HTTPHeaders headers;
    S3Params params;
    string url;
    S3RESTfulService service;

    /* data = "abcdefghij", length = 11 (including '\0') */
    vector<uint8_t> data;
    for (int i = 0; i < 10; i++) data.push_back('a' + i);
    data.push_back(0);

    headers.Add(CONTENTTYPE, "text/plain");
    headers.Add(CONTENTLENGTH, std::to_string(data.size()));

    url = "http://localhost:8553";

    Response resp = service.post(url, headers, params, data);
    EXPECT_EQ(RESPONSE_OK, resp.getStatus());
    EXPECT_TRUE(compareVector(data, resp.getRawData()));
}

/* Run './bin/dummyHTTPServer.py' before enabling this test */
TEST(S3RESTfulService, DISABLED_DeleteToDummyServerWithData) {
    HTTPHeaders headers;
    S3Params params;
    string url;
    S3RESTfulService service;

    headers.Add(CONTENTTYPE, "text/plain");
    headers.Add(CONTENTLENGTH, "0");

    url = "http://localhost:8553";

    Response resp = service.deleteRequest(url, headers, params);
    EXPECT_EQ(RESPONSE_OK, resp.getStatus());
}
