#include "s3interface.cpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_classes.h"

using ::testing::AtLeast;
using ::testing::Return;
using ::testing::Throw;
using ::testing::_;

class S3ServiceTest : public testing::Test, public S3Service {
   protected:
    // Remember that SetUp() is run immediately before a test starts.
    virtual void SetUp() {
        result = NULL;

        s3ext_logtype = STDERR_LOG;
        s3ext_loglevel = EXT_INFO;

        schema = "https";

        this->setRESTfulService(&mockRestfulService);
    }

    // TearDown() is invoked immediately after a test finishes.
    virtual void TearDown() {
        if (result != NULL) {
            delete result;
            result = NULL;
        }
    }

    Response buildListBucketResponse(int numOfContent, bool isTruncated, int numOfZeroKeys = 0) {
        XMLGenerator generator;
        XMLGenerator *gen = &generator;
        gen->setName("s3test.pivotal.io")
            ->setPrefix("s3files/")
            ->setIsTruncated(isTruncated)
            ->pushBuckentContent(BucketContent("s3files/", 0));

        char buffer[32] = {0};
        for (int i = 0; i < numOfContent; ++i) {
            snprintf(buffer, 32, "files%d", i);
            gen->pushBuckentContent(BucketContent(buffer, i + 1));
        }

        for (int i = 0; i < numOfZeroKeys; i++) {
            snprintf(buffer, 32, "zerofiles%d", i);
            gen->pushBuckentContent(BucketContent(buffer, 0));
        }

        return Response(RESPONSE_OK, gen->toXML());
    }

    S3Credential cred;
    string schema;
    string region;
    string bucket;
    string prefix;

    MockS3RESTfulService mockRestfulService;
    Response response;

    ListBucketResult *result;
};

TEST_F(S3ServiceTest, GetResponseWithZeroRetry) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;
    map<string, string> params;
    EXPECT_EQ(RESPONSE_FAIL, this->getResponseWithRetries(url, headers, params, 0).getStatus());
}

TEST_F(S3ServiceTest, GetResponseWithTwoRetries) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;
    map<string, string> params;

    EXPECT_CALL(mockRestfulService, get(_, _, _))
        .Times(2)
        .WillOnce(Return(response))
        .WillOnce(Return(response));

    EXPECT_EQ(RESPONSE_FAIL, this->getResponseWithRetries(url, headers, params, 2).getStatus());
}

TEST_F(S3ServiceTest, GetResponseWithRetriesAndSuccess) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;
    map<string, string> params;

    Response responseSuccess;
    responseSuccess.setStatus(RESPONSE_OK);

    EXPECT_CALL(mockRestfulService, get(_, _, _))
        .Times(2)
        .WillOnce(Return(response))
        .WillOnce(Return(responseSuccess));

    EXPECT_EQ(RESPONSE_OK, this->getResponseWithRetries(url, headers, params).getStatus());
}

TEST_F(S3ServiceTest, PutResponseWithZeroRetry) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;
    map<string, string> params;
    vector<uint8_t> data;
    EXPECT_EQ(RESPONSE_FAIL,
              this->putResponseWithRetries(url, headers, params, data, 0).getStatus());
}

TEST_F(S3ServiceTest, PutResponseWithTwoRetries) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;
    map<string, string> params;
    vector<uint8_t> data;

    EXPECT_CALL(mockRestfulService, put(_, _, _, _))
        .Times(2)
        .WillOnce(Return(response))
        .WillOnce(Return(response));

    EXPECT_EQ(RESPONSE_FAIL,
              this->putResponseWithRetries(url, headers, params, data, 2).getStatus());
}

TEST_F(S3ServiceTest, PutResponseWithRetriesAndSuccess) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;
    map<string, string> params;
    vector<uint8_t> data;

    Response responseSuccess;
    responseSuccess.setStatus(RESPONSE_OK);

    EXPECT_CALL(mockRestfulService, put(_, _, _, _))
        .Times(2)
        .WillOnce(Return(response))
        .WillOnce(Return(responseSuccess));

    EXPECT_EQ(RESPONSE_OK, this->putResponseWithRetries(url, headers, params, data).getStatus());
}

TEST_F(S3ServiceTest, HeadResponseWithZeroRetry) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;
    map<string, string> params;
    EXPECT_EQ(HeadResponseFail, this->headResponseWithRetries(url, headers, params, 0));
}

TEST_F(S3ServiceTest, HeadResponseWithRetriesAndFail) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;
    map<string, string> params;

    EXPECT_CALL(mockRestfulService, head(_, _, _))
        .Times(S3_REQUEST_MAX_RETRIES)
        .WillOnce(Return(HeadResponseFail))
        .WillOnce(Return(HeadResponseFail))
        .WillOnce(Return(HeadResponseFail));

    EXPECT_EQ(HeadResponseFail, this->headResponseWithRetries(url, headers, params));
}

TEST_F(S3ServiceTest, HeadResponseWithRetriesAndSuccess) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;
    map<string, string> params;

    EXPECT_CALL(mockRestfulService, head(_, _, _))
        .Times(2)
        .WillOnce(Return(HeadResponseFail))
        .WillOnce(Return(404));

    EXPECT_EQ(404, this->headResponseWithRetries(url, headers, params));
}

TEST_F(S3ServiceTest, PostResponseWithZeroRetry) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;
    map<string, string> params;
    vector<uint8_t> data;

    EXPECT_EQ(RESPONSE_FAIL,
              this->postResponseWithRetries(url, headers, params, data, 0).getStatus());
}

TEST_F(S3ServiceTest, PostResponseWithTwoRetries) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;
    map<string, string> params;
    vector<uint8_t> data;

    EXPECT_CALL(mockRestfulService, post(_, _, _, _))
        .Times(2)
        .WillOnce(Return(response))
        .WillOnce(Return(response));

    EXPECT_EQ(RESPONSE_FAIL,
              this->postResponseWithRetries(url, headers, params, data, 2).getStatus());
}

TEST_F(S3ServiceTest, PostResponseWithRetriesAndSuccess) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;
    map<string, string> params;
    vector<uint8_t> data;

    Response responseSuccess;
    responseSuccess.setStatus(RESPONSE_OK);

    EXPECT_CALL(mockRestfulService, post(_, _, _, _))
        .Times(2)
        .WillOnce(Return(response))
        .WillOnce(Return(responseSuccess));

    EXPECT_EQ(RESPONSE_OK, this->postResponseWithRetries(url, headers, params, data).getStatus());
}

TEST_F(S3ServiceTest, ListBucketThrowExceptionWhenBucketStringIsEmpty) {
    // here is a memory leak because we haven't defined ~S3ServiceTest() to clean up.
    EXPECT_THROW(result = this->listBucket("", "", "", "", cred), std::runtime_error);
}

TEST_F(S3ServiceTest, ListBucketWithWrongRegion) {
    EXPECT_CALL(mockRestfulService, get(_, _, _)).WillRepeatedly(Return(response));

    EXPECT_EQ((void *)NULL, result = this->listBucket(schema, "nonexist", "", "", cred));
}

TEST_F(S3ServiceTest, ListBucketWithWrongBucketName) {
    uint8_t xml[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<Error>"
        "<Code>PermanentRedirect</Code>"
        "<Message>The bucket you are attempting to access must be addressed "
        "using the specified endpoint. "
        "Please send all future requests to this endpoint.</Message>"
        "<Bucket>foo</Bucket><Endpoint>s3.amazonaws.com</Endpoint>"
        "<RequestId>27DD9B7004AF83E3</RequestId>"
        "<HostId>NL3pyGvn+FajhQLKz/"
        "hXUzV1VnFbbwNjUQsqWeFiDANkV4EVkh8Kpq5NNAi27P7XDhoA9M9Xhg0=</HostId>"
        "</Error>";
    vector<uint8_t> raw(xml, xml + sizeof(xml) - 1);
    Response response(RESPONSE_ERROR, raw);

    EXPECT_CALL(mockRestfulService, get(_, _, _)).WillRepeatedly(Return(response));

    EXPECT_EQ((void *)NULL, result = this->listBucket(schema, "us-west-2", "foo/bar", "", cred));
}

TEST_F(S3ServiceTest, ListBucketWithNormalBucket) {
    XMLGenerator generator;
    XMLGenerator *gen = &generator;
    gen->setName("s3test.pivotal.io")
        ->setPrefix("threebytes/")
        ->setIsTruncated(false)
        ->pushBuckentContent(BucketContent("threebytes/", 0))
        ->pushBuckentContent(BucketContent("threebytes/threebytes", 3));

    Response response(RESPONSE_OK, gen->toXML());

    EXPECT_CALL(mockRestfulService, get(_, _, _)).WillOnce(Return(response));

    result = this->listBucket(schema, "us-west-2", "s3test.pivotal.io", "threebytes/", cred);
    ASSERT_NE((void *)NULL, result);
    EXPECT_EQ(1, result->contents.size());
}

TEST_F(S3ServiceTest, ListBucketWithBucketWith1000Keys) {
    EXPECT_CALL(mockRestfulService, get(_, _, _))
        .WillOnce(Return(this->buildListBucketResponse(1000, false)));

    result = this->listBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/", cred);
    ASSERT_NE((void *)NULL, result);
    EXPECT_EQ(1000, result->contents.size());
}

TEST_F(S3ServiceTest, ListBucketWithBucketWith1001Keys) {
    EXPECT_CALL(mockRestfulService, get(_, _, _))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(1, false)));

    result = this->listBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/", cred);
    ASSERT_NE((void *)NULL, result);
    EXPECT_EQ(1001, result->contents.size());
}

TEST_F(S3ServiceTest, ListBucketWithBucketWithMoreThan1000Keys) {
    EXPECT_CALL(mockRestfulService, get(_, _, _))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(120, false)));

    result = this->listBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/", cred);
    ASSERT_NE((void *)NULL, result);
    EXPECT_EQ(5120, result->contents.size());
}

TEST_F(S3ServiceTest, ListBucketWithBucketWithTruncatedResponse) {
    Response EmptyResponse;

    EXPECT_CALL(mockRestfulService, get(_, _, _))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillRepeatedly(Return(EmptyResponse));

    result = this->listBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/", cred);
    EXPECT_EQ((void *)NULL, result);
}

TEST_F(S3ServiceTest, ListBucketWithBucketWithZeroSizedKeys) {
    EXPECT_CALL(mockRestfulService, get(_, _, _))
        .WillOnce(Return(this->buildListBucketResponse(0, true, 8)))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(120, false, 8)));

    result = this->listBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/", cred);
    ASSERT_NE((void *)NULL, result);
    EXPECT_EQ(1120, result->contents.size());
}

TEST_F(S3ServiceTest, ListBucketWithEmptyBucket) {
    EXPECT_CALL(mockRestfulService, get(_, _, _))
        .WillOnce(Return(this->buildListBucketResponse(0, false, 0)));

    result = this->listBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/", cred);
    ASSERT_NE((void *)NULL, result);
    EXPECT_EQ(0, result->contents.size());
}

TEST_F(S3ServiceTest, ListBucketWithAllZeroedFilesBucket) {
    EXPECT_CALL(mockRestfulService, get(_, _, _))
        .WillOnce(Return(this->buildListBucketResponse(0, false, 2)));

    result = this->listBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/", cred);
    ASSERT_NE((void *)NULL, result);
    EXPECT_EQ(0, result->contents.size());
}

TEST_F(S3ServiceTest, ListBucketWithErrorResponse) {
    EXPECT_CALL(mockRestfulService, get(_, _, _)).WillRepeatedly(Return(response));

    EXPECT_EQ((void *)NULL,
              result = this->listBucket(schema, "nonexist", "s3test.pivotal.io", "s3files/", cred));
}

TEST_F(S3ServiceTest, ListBucketWithErrorReturnedXML) {
    uint8_t xml[] = "whatever";
    vector<uint8_t> raw(xml, xml + sizeof(xml) - 1);
    Response response(RESPONSE_ERROR, raw);

    EXPECT_CALL(mockRestfulService, get(_, _, _)).WillRepeatedly(Return(response));

    EXPECT_EQ((void *)NULL, result = this->listBucket(schema, "us-west-2", "s3test.pivotal.io",
                                                      "s3files/", cred));
}

TEST_F(S3ServiceTest, ListBucketWithNonRootXML) {
    uint8_t xml[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    vector<uint8_t> raw(xml, xml + sizeof(xml) - 1);
    Response response(RESPONSE_ERROR, raw);

    EXPECT_CALL(mockRestfulService, get(_, _, _)).WillRepeatedly(Return(response));

    EXPECT_EQ((void *)NULL, result = this->listBucket(schema, "us-west-2", "s3test.pivotal.io",
                                                      "s3files/", cred));
}

TEST_F(S3ServiceTest, fetchDataRoutine) {
    vector<uint8_t> raw;

    srand(time(NULL));

    for (int i = 0; i < 100; i++) {
        raw.push_back(rand() & 0xFF);
    }

    Response response(RESPONSE_OK, raw);
    EXPECT_CALL(mockRestfulService, get(_, _, _)).WillOnce(Return(response));

    vector<uint8_t> buffer;

    uint64_t len = this->fetchData(0, buffer, 100,
                                   "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever",
                                   region, cred);

    EXPECT_EQ(buffer.size(), raw.size());
    EXPECT_EQ(0, memcmp(buffer.data(), raw.data(), 100));
    EXPECT_EQ(100, len);
}

TEST_F(S3ServiceTest, fetchDataErrorResponse) {
    vector<uint8_t> raw;
    raw.resize(100);
    Response response(RESPONSE_ERROR, raw);
    EXPECT_CALL(mockRestfulService, get(_, _, _)).WillRepeatedly(Return(response));

    vector<uint8_t> buffer;

    EXPECT_THROW(this->fetchData(0, buffer, 100,
                                 "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever",
                                 region, cred),
                 std::runtime_error);
}

TEST_F(S3ServiceTest, fetchDataFailedResponse) {
    vector<uint8_t> raw;
    raw.resize(100);
    Response response(RESPONSE_FAIL, raw);
    EXPECT_CALL(mockRestfulService, get(_, _, _)).WillRepeatedly(Return(response));

    vector<uint8_t> buffer;

    EXPECT_THROW(this->fetchData(0, buffer, 100,
                                 "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever",
                                 region, cred),
                 std::runtime_error);
}

TEST_F(S3ServiceTest, fetchDataPartialResponse) {
    vector<uint8_t> raw;
    raw.resize(80);
    Response response(RESPONSE_OK, raw);
    EXPECT_CALL(mockRestfulService, get(_, _, _)).WillOnce(Return(response));
    vector<uint8_t> buffer;

    EXPECT_THROW(this->fetchData(0, buffer, 100,
                                 "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever",
                                 region, cred),
                 std::runtime_error);
}

TEST_F(S3ServiceTest, checkSmallFile) {
    vector<uint8_t> raw;
    raw.resize(2);
    raw[0] = 0x1f;
    raw[1] = 0x8b;
    Response response(RESPONSE_OK, raw);
    EXPECT_CALL(mockRestfulService, get(_, _, _)).WillOnce(Return(response));

    EXPECT_EQ(S3_COMPRESSION_PLAIN,
              this->checkCompressionType(
                  "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever", region, cred));
}

TEST_F(S3ServiceTest, checkItsGzipCompressed) {
    vector<uint8_t> raw;
    raw.resize(4);
    raw[0] = 0x1f;
    raw[1] = 0x8b;
    Response response(RESPONSE_OK, raw);
    EXPECT_CALL(mockRestfulService, get(_, _, _)).WillOnce(Return(response));

    EXPECT_EQ(S3_COMPRESSION_GZIP,
              this->checkCompressionType(
                  "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever", region, cred));
}

TEST_F(S3ServiceTest, checkItsNotCompressed) {
    vector<uint8_t> raw;
    raw.resize(4);
    raw[0] = 0x1f;
    raw[1] = 0x88;
    Response response(RESPONSE_OK, raw);
    EXPECT_CALL(mockRestfulService, get(_, _, _)).WillOnce(Return(response));

    EXPECT_EQ(S3_COMPRESSION_PLAIN,
              this->checkCompressionType(
                  "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever", region, cred));
}

TEST_F(S3ServiceTest, checkCompreesionTypeWithResponseError) {
    uint8_t xml[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<Error>"
        "<Code>PermanentRedirect</Code>"
        "<Message>The bucket you are attempting to access must be addressed "
        "using the specified endpoint. "
        "Please send all future requests to this endpoint.</Message>"
        "<Bucket>foo</Bucket><Endpoint>s3.amazonaws.com</Endpoint>"
        "<RequestId>27DD9B7004AF83E3</RequestId>"
        "<HostId>NL3pyGvn+FajhQLKz/"
        "hXUzV1VnFbbwNjUQsqWeFiDANkV4EVkh8Kpq5NNAi27P7XDhoA9M9Xhg0=</HostId>"
        "</Error>";
    vector<uint8_t> raw(xml, xml + sizeof(xml) - 1);
    Response response(RESPONSE_ERROR, raw);

    EXPECT_CALL(mockRestfulService, get(_, _, _)).WillRepeatedly(Return(response));

    EXPECT_THROW(this->checkCompressionType(
                     "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever", region, cred),
                 std::runtime_error);
}

TEST_F(S3ServiceTest, fetchDataWithResponseError) {
    uint8_t xml[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<Error>"
        "<Code>PermanentRedirect</Code>"
        "<Message>The bucket you are attempting to access must be addressed "
        "using the specified endpoint. "
        "Please send all future requests to this endpoint.</Message>"
        "<Bucket>foo</Bucket><Endpoint>s3.amazonaws.com</Endpoint>"
        "<RequestId>27DD9B7004AF83E3</RequestId>"
        "<HostId>NL3pyGvn+FajhQLKz/"
        "hXUzV1VnFbbwNjUQsqWeFiDANkV4EVkh8Kpq5NNAi27P7XDhoA9M9Xhg0=</HostId>"
        "</Error>";
    vector<uint8_t> raw(xml, xml + sizeof(xml) - 1);
    Response response(RESPONSE_ERROR, raw);
    vector<uint8_t> buffer;

    EXPECT_CALL(mockRestfulService, get(_, _, _)).WillRepeatedly(Return(response));

    EXPECT_THROW(this->fetchData(0, buffer, 128,
                                 "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever",
                                 region, cred),
                 std::runtime_error);
}

TEST_F(S3ServiceTest, HeadResponseWithHeadResponseFail) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";

    EXPECT_CALL(mockRestfulService, head(_, _, _))
        .Times(3)
        .WillRepeatedly(Return(HeadResponseFail));

    EXPECT_FALSE(this->checkKeyExistence(url, this->region, this->cred));
}

TEST_F(S3ServiceTest, HeadResponse200) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";

    EXPECT_CALL(mockRestfulService, head(_, _, _)).WillOnce(Return(200));

    EXPECT_TRUE(this->checkKeyExistence(url, this->region, this->cred));
}

TEST_F(S3ServiceTest, HeadResponse206) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";

    EXPECT_CALL(mockRestfulService, head(_, _, _)).WillOnce(Return(206));

    EXPECT_TRUE(this->checkKeyExistence(url, this->region, this->cred));
}

TEST_F(S3ServiceTest, HeadResponse404) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";

    EXPECT_CALL(mockRestfulService, head(_, _, _)).WillOnce(Return(404));

    EXPECT_FALSE(this->checkKeyExistence(url, this->region, this->cred));
}

TEST_F(S3ServiceTest, HeadResponse403) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";

    EXPECT_CALL(mockRestfulService, head(_, _, _)).WillOnce(Return(403));

    EXPECT_FALSE(this->checkKeyExistence(url, this->region, this->cred));
}

TEST_F(S3ServiceTest, getUploadIdRoutine) {
    uint8_t xml[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<InitiateMultipartUploadResult"
        " xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">"
        "<Bucket>example-bucket</Bucket>"
        "<Key>example-object</Key>"
        "<UploadId>VXBsb2FkIElEIGZvciA2aWWpbmcncyBteS1tb3ZpZS5tMnRzIHVwbG9hZA</UploadId>"
        "</InitiateMultipartUploadResult>";
    vector<uint8_t> raw(xml, xml + sizeof(xml) - 1);
    Response response(RESPONSE_OK, raw);

    EXPECT_CALL(mockRestfulService, post(_, _, _, vector<uint8_t>())).WillOnce(Return(response));

    EXPECT_EQ("VXBsb2FkIElEIGZvciA2aWWpbmcncyBteS1tb3ZpZS5tMnRzIHVwbG9hZA",
              this->getUploadId("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever",
                                region, cred));
}

TEST_F(S3ServiceTest, getUploadIdFailedResponse) {
    vector<uint8_t> raw;
    raw.resize(100);
    Response response(RESPONSE_FAIL, raw);

    EXPECT_CALL(mockRestfulService, post(_, _, _, vector<uint8_t>()))
        .WillRepeatedly(Return(response));

    EXPECT_THROW(this->getUploadId("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever",
                                   region, cred),
                 std::runtime_error);
}

TEST_F(S3ServiceTest, getUploadIdErrorResponse) {
    uint8_t xml[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<Error>"
        "<Code>PermanentRedirect</Code>"
        "<Message>The bucket you are attempting to access must be addressed "
        "using the specified endpoint. "
        "Please send all future requests to this endpoint.</Message>"
        "<Bucket>foo</Bucket><Endpoint>s3.amazonaws.com</Endpoint>"
        "<RequestId>27DD9B7004AF83E3</RequestId>"
        "<HostId>NL3pyGvn+FajhQLKz/"
        "hXUzV1VnFbbwNjUQsqWeFiDANkV4EVkh8Kpq5NNAi27P7XDhoA9M9Xhg0=</HostId>"
        "</Error>";
    vector<uint8_t> raw(xml, xml + sizeof(xml) - 1);
    Response response(RESPONSE_ERROR, raw);

    EXPECT_CALL(mockRestfulService, post(_, _, _, vector<uint8_t>()))
        .WillRepeatedly(Return(response));

    EXPECT_THROW(this->getUploadId("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever",
                                   region, cred),
                 std::runtime_error);
}

TEST_F(S3ServiceTest, uploadPartOfDataRoutine) {
    vector<uint8_t> raw;
    raw.resize(100);

    uint8_t headers[] =
        "x-amz-id-2: Vvag1LuByRx9e6j5Onimru9pO4ZVKnJ2Qz7/C1NPcfTWAtRPfTaOFg==\r\n"
        "x-amz-request-id: 656c76696e6727732072657175657374\r\n"
        "Date:  Mon, 1 Nov 2010 20:34:56 GMT\r\n"
        "ETag: \"b54357faf0632cce46e942fa68356b38\"\r\n"
        "Content-Length: 0\r\n"
        "Connection: keep-alive\r\n"
        "Server: AmazonS3\r\n";
    vector<uint8_t> data(headers, headers + sizeof(headers) - 1);

    Response response(RESPONSE_OK, data, raw);

    EXPECT_CALL(mockRestfulService, put(_, _, _, _)).WillOnce(Return(response));

    EXPECT_EQ(
        "\"b54357faf0632cce46e942fa68356b38\"",
        this->uploadPartOfData(raw, "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever",
                               region, cred, 11, "xyz"));
}

TEST_F(S3ServiceTest, uploadPartOfDataFailedResponse) {
    vector<uint8_t> raw;
    raw.resize(100);
    Response response(RESPONSE_FAIL, raw);
    EXPECT_CALL(mockRestfulService, put(_, _, _, _)).WillRepeatedly(Return(response));

    EXPECT_THROW(
        this->uploadPartOfData(raw, "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever",
                               region, cred, 11, "xyz"),
        std::runtime_error);
}

TEST_F(S3ServiceTest, uploadPartOfDataErrorResponse) {
    uint8_t xml[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<Error>"
        "<Code>PermanentRedirect</Code>"
        "<Message>The bucket you are attempting to access must be addressed "
        "using the specified endpoint. "
        "Please send all future requests to this endpoint.</Message>"
        "<Bucket>foo</Bucket><Endpoint>s3.amazonaws.com</Endpoint>"
        "<RequestId>27DD9B7004AF83E3</RequestId>"
        "<HostId>NL3pyGvn+FajhQLKz/"
        "hXUzV1VnFbbwNjUQsqWeFiDANkV4EVkh8Kpq5NNAi27P7XDhoA9M9Xhg0=</HostId>"
        "</Error>";
    vector<uint8_t> raw(xml, xml + sizeof(xml) - 1);
    Response response(RESPONSE_ERROR, raw);

    EXPECT_CALL(mockRestfulService, put(_, _, _, _)).WillRepeatedly(Return(response));

    EXPECT_THROW(
        this->uploadPartOfData(raw, "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever",
                               region, cred, 11, "xyz"),
        std::runtime_error);
}

TEST_F(S3ServiceTest, completeMultiPartRoutine) {
    vector<uint8_t> raw;
    raw.resize(100);
    Response response(RESPONSE_OK, raw);
    EXPECT_CALL(mockRestfulService, post(_, _, _, _)).WillOnce(Return(response));

    vector<string> etagArray;

    etagArray.push_back("\"abc\"");
    etagArray.push_back("\"def\"");

    EXPECT_TRUE(
        this->completeMultiPart("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever",
                                region, cred, "xyz", etagArray));
}

TEST_F(S3ServiceTest, completeMultiPartFailedResponse) {
    vector<uint8_t> raw;
    raw.resize(100);
    Response response(RESPONSE_FAIL, raw);
    EXPECT_CALL(mockRestfulService, post(_, _, _, _)).WillRepeatedly(Return(response));

    vector<string> etagArray;

    etagArray.push_back("\"abc\"");
    etagArray.push_back("\"def\"");

    EXPECT_THROW(
        this->completeMultiPart("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever",
                                region, cred, "xyz", etagArray),
        std::runtime_error);
}

TEST_F(S3ServiceTest, completeMultiPartErrorResponse) {
    uint8_t xml[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<Error>"
        "<Code>PermanentRedirect</Code>"
        "<Message>The bucket you are attempting to access must be addressed "
        "using the specified endpoint. "
        "Please send all future requests to this endpoint.</Message>"
        "<Bucket>foo</Bucket><Endpoint>s3.amazonaws.com</Endpoint>"
        "<RequestId>27DD9B7004AF83E3</RequestId>"
        "<HostId>NL3pyGvn+FajhQLKz/"
        "hXUzV1VnFbbwNjUQsqWeFiDANkV4EVkh8Kpq5NNAi27P7XDhoA9M9Xhg0=</HostId>"
        "</Error>";
    vector<uint8_t> raw(xml, xml + sizeof(xml) - 1);
    Response response(RESPONSE_ERROR, raw);

    EXPECT_CALL(mockRestfulService, post(_, _, _, _)).WillRepeatedly(Return(response));

    vector<string> etagArray;

    etagArray.push_back("\"abc\"");
    etagArray.push_back("\"def\"");

    EXPECT_THROW(
        this->completeMultiPart("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever",
                                region, cred, "xyz", etagArray),
        std::runtime_error);
}
