#include "s3interface.cpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_classes.h"

using ::testing::AtLeast;
using ::testing::Return;
using ::testing::Throw;
using ::testing::_;

class S3InterfaceServiceTest : public testing::Test, public S3InterfaceService {
   public:
    S3InterfaceServiceTest() : params(), mockRESTfulService(this->params) {
    }

   protected:
    // Remember that SetUp() is run immediately before a test starts.
    virtual void SetUp() {
        s3ext_logtype = STDERR_LOG;
        s3ext_loglevel = EXT_INFO;

        schema = "https";

        this->setRESTfulService(&mockRESTfulService);
    }

    // TearDown() is invoked immediately after a test finishes.
    virtual void TearDown() {
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

    S3Params params;

    string schema;
    string region;
    string bucket;
    string prefix;

    MockS3RESTfulService mockRESTfulService;
    Response response;

    ListBucketResult result;
};

TEST_F(S3InterfaceServiceTest, GetResponseWithZeroRetry) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;

    EXPECT_EQ(RESPONSE_FAIL, this->getResponseWithRetries(url, headers, 0).getStatus());
}

TEST_F(S3InterfaceServiceTest, GetResponseWithTwoRetries) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;

    EXPECT_CALL(mockRESTfulService, get(_, _))
        .Times(2)
        .WillOnce(Return(response))
        .WillOnce(Return(response));

    EXPECT_EQ(RESPONSE_FAIL, this->getResponseWithRetries(url, headers, 2).getStatus());
}

TEST_F(S3InterfaceServiceTest, GetResponseWithRetriesAndSuccess) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;

    Response responseSuccess;
    responseSuccess.setStatus(RESPONSE_OK);

    EXPECT_CALL(mockRESTfulService, get(_, _))
        .Times(2)
        .WillOnce(Return(response))
        .WillOnce(Return(responseSuccess));

    EXPECT_EQ(RESPONSE_OK, this->getResponseWithRetries(url, headers).getStatus());
}

TEST_F(S3InterfaceServiceTest, PutResponseWithZeroRetry) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;

    vector<uint8_t> data;
    EXPECT_EQ(RESPONSE_FAIL, this->putResponseWithRetries(url, headers, data, 0).getStatus());
}

TEST_F(S3InterfaceServiceTest, PutResponseWithTwoRetries) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;

    vector<uint8_t> data;

    EXPECT_CALL(mockRESTfulService, put(_, _, _))
        .Times(2)
        .WillOnce(Return(response))
        .WillOnce(Return(response));

    EXPECT_EQ(RESPONSE_FAIL, this->putResponseWithRetries(url, headers, data, 2).getStatus());
}

TEST_F(S3InterfaceServiceTest, PutResponseWithRetriesAndSuccess) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;

    vector<uint8_t> data;

    Response responseSuccess;
    responseSuccess.setStatus(RESPONSE_OK);

    EXPECT_CALL(mockRESTfulService, put(_, _, _))
        .Times(2)
        .WillOnce(Return(response))
        .WillOnce(Return(responseSuccess));

    EXPECT_EQ(RESPONSE_OK, this->putResponseWithRetries(url, headers, data).getStatus());
}

TEST_F(S3InterfaceServiceTest, HeadResponseWithZeroRetry) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;

    EXPECT_EQ(HeadResponseFail, this->headResponseWithRetries(url, headers, 0));
}

TEST_F(S3InterfaceServiceTest, HeadResponseWithRetriesAndFail) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;

    EXPECT_CALL(mockRESTfulService, head(_, _))
        .Times(S3_REQUEST_MAX_RETRIES)
        .WillOnce(Return(HeadResponseFail))
        .WillOnce(Return(HeadResponseFail))
        .WillOnce(Return(HeadResponseFail));

    EXPECT_EQ(HeadResponseFail, this->headResponseWithRetries(url, headers));
}

TEST_F(S3InterfaceServiceTest, HeadResponseWithRetriesAndSuccess) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;

    EXPECT_CALL(mockRESTfulService, head(_, _))
        .Times(2)
        .WillOnce(Return(HeadResponseFail))
        .WillOnce(Return(404));

    EXPECT_EQ(404, this->headResponseWithRetries(url, headers));
}

TEST_F(S3InterfaceServiceTest, PostResponseWithZeroRetry) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;
    vector<uint8_t> data;

    EXPECT_EQ(RESPONSE_FAIL, this->postResponseWithRetries(url, headers, data, 0).getStatus());
}

TEST_F(S3InterfaceServiceTest, PostResponseWithTwoRetries) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;
    vector<uint8_t> data;

    EXPECT_CALL(mockRESTfulService, post(_, _, _))
        .Times(2)
        .WillOnce(Return(response))
        .WillOnce(Return(response));

    EXPECT_EQ(RESPONSE_FAIL, this->postResponseWithRetries(url, headers, data, 2).getStatus());
}

TEST_F(S3InterfaceServiceTest, PostResponseWithRetriesAndSuccess) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;
    vector<uint8_t> data;

    Response responseSuccess;
    responseSuccess.setStatus(RESPONSE_OK);

    EXPECT_CALL(mockRESTfulService, post(_, _, _))
        .Times(2)
        .WillOnce(Return(response))
        .WillOnce(Return(responseSuccess));

    EXPECT_EQ(RESPONSE_OK, this->postResponseWithRetries(url, headers, data).getStatus());
}

TEST_F(S3InterfaceServiceTest, ListBucketThrowExceptionWhenBucketStringIsEmpty) {
    EXPECT_THROW(result = this->listBucket("", "", "", ""), std::runtime_error);
}

TEST_F(S3InterfaceServiceTest, ListBucketWithWrongRegion) {
    EXPECT_CALL(mockRESTfulService, get(_, _)).WillRepeatedly(Return(response));

    EXPECT_THROW(this->listBucket(schema, "nonexist", "", ""), std::runtime_error);
}

TEST_F(S3InterfaceServiceTest, ListBucketWithWrongBucketName) {
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

    EXPECT_CALL(mockRESTfulService, get(_, _)).WillRepeatedly(Return(response));

    EXPECT_THROW(this->listBucket(schema, "us-west-2", "foo/bar", ""), std::runtime_error);
}

TEST_F(S3InterfaceServiceTest, ListBucketWithNormalBucket) {
    XMLGenerator generator;
    XMLGenerator *gen = &generator;
    gen->setName("s3test.pivotal.io")
        ->setPrefix("threebytes/")
        ->setIsTruncated(false)
        ->pushBuckentContent(BucketContent("threebytes/", 0))
        ->pushBuckentContent(BucketContent("threebytes/threebytes", 3));

    Response response(RESPONSE_OK, gen->toXML());

    EXPECT_CALL(mockRESTfulService, get(_, _)).WillOnce(Return(response));

    result = this->listBucket(schema, "us-west-2", "s3test.pivotal.io", "threebytes/");
    EXPECT_EQ((uint64_t)1, result.contents.size());
}

TEST_F(S3InterfaceServiceTest, ListBucketWithBucketWith1000Keys) {
    EXPECT_CALL(mockRESTfulService, get(_, _))
        .WillOnce(Return(this->buildListBucketResponse(1000, false)));

    result = this->listBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/");
    EXPECT_EQ((uint64_t)1000, result.contents.size());
}

TEST_F(S3InterfaceServiceTest, ListBucketWithBucketWith1001Keys) {
    EXPECT_CALL(mockRESTfulService, get(_, _))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(1, false)));

    result = this->listBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/");
    EXPECT_EQ((uint64_t)1001, result.contents.size());
}

TEST_F(S3InterfaceServiceTest, ListBucketWithBucketWithMoreThan1000Keys) {
    EXPECT_CALL(mockRESTfulService, get(_, _))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(120, false)));

    result = this->listBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/");
    EXPECT_EQ((uint64_t)5120, result.contents.size());
}

TEST_F(S3InterfaceServiceTest, ListBucketWithBucketWithTruncatedResponse) {
    Response EmptyResponse;

    EXPECT_CALL(mockRESTfulService, get(_, _))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillRepeatedly(Return(EmptyResponse));

    EXPECT_THROW(this->listBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/"),
                 std::runtime_error);
}

TEST_F(S3InterfaceServiceTest, ListBucketWithBucketWithZeroSizedKeys) {
    EXPECT_CALL(mockRESTfulService, get(_, _))
        .WillOnce(Return(this->buildListBucketResponse(0, true, 8)))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(120, false, 8)));

    result = this->listBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/");
    EXPECT_EQ((uint64_t)1120, result.contents.size());
}

TEST_F(S3InterfaceServiceTest, ListBucketWithEmptyBucket) {
    EXPECT_CALL(mockRESTfulService, get(_, _))
        .WillOnce(Return(this->buildListBucketResponse(0, false, 0)));

    result = this->listBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/");

    EXPECT_EQ((uint64_t)0, result.contents.size());
}

TEST_F(S3InterfaceServiceTest, ListBucketWithAllZeroedFilesBucket) {
    EXPECT_CALL(mockRESTfulService, get(_, _))
        .WillOnce(Return(this->buildListBucketResponse(0, false, 2)));

    result = this->listBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/");
    EXPECT_EQ((uint64_t)0, result.contents.size());
}

TEST_F(S3InterfaceServiceTest, ListBucketWithErrorResponse) {
    EXPECT_CALL(mockRESTfulService, get(_, _)).WillRepeatedly(Return(response));

    EXPECT_THROW(this->listBucket(schema, "nonexist", "s3test.pivotal.io", "s3files/"),
                 std::runtime_error);
}

TEST_F(S3InterfaceServiceTest, ListBucketWithErrorReturnedXML) {
    uint8_t xml[] = "whatever";
    vector<uint8_t> raw(xml, xml + sizeof(xml) - 1);
    Response response(RESPONSE_ERROR, raw);

    EXPECT_CALL(mockRESTfulService, get(_, _)).WillRepeatedly(Return(response));

    EXPECT_THROW(this->listBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/"),
                 std::runtime_error);
}

TEST_F(S3InterfaceServiceTest, ListBucketWithNonRootXML) {
    uint8_t xml[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    vector<uint8_t> raw(xml, xml + sizeof(xml) - 1);
    Response response(RESPONSE_ERROR, raw);

    EXPECT_CALL(mockRESTfulService, get(_, _)).WillRepeatedly(Return(response));

    EXPECT_THROW(this->listBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/"),
                 std::runtime_error);
}

TEST_F(S3InterfaceServiceTest, fetchDataRoutine) {
    vector<uint8_t> raw;

    srand(time(NULL));

    for (int i = 0; i < 100; i++) {
        raw.push_back(rand() & 0xFF);
    }

    Response response(RESPONSE_OK, raw);
    EXPECT_CALL(mockRESTfulService, get(_, _)).WillOnce(Return(response));

    vector<uint8_t> buffer;

    uint64_t len = this->fetchData(
        0, buffer, 100, "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever", region);

    EXPECT_EQ(buffer.size(), raw.size());
    EXPECT_EQ(0, memcmp(buffer.data(), raw.data(), 100));
    EXPECT_EQ((uint64_t)100, len);
}

TEST_F(S3InterfaceServiceTest, fetchDataErrorResponse) {
    vector<uint8_t> raw;
    raw.resize(100);
    Response response(RESPONSE_ERROR, raw);
    EXPECT_CALL(mockRESTfulService, get(_, _)).WillRepeatedly(Return(response));

    vector<uint8_t> buffer;

    EXPECT_THROW(
        this->fetchData(0, buffer, 100,
                        "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever", region),
        std::runtime_error);
}

TEST_F(S3InterfaceServiceTest, fetchDataFailedResponse) {
    vector<uint8_t> raw;
    raw.resize(100);
    Response response(RESPONSE_FAIL, raw);
    EXPECT_CALL(mockRESTfulService, get(_, _)).WillRepeatedly(Return(response));

    vector<uint8_t> buffer;

    EXPECT_THROW(
        this->fetchData(0, buffer, 100,
                        "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever", region),
        std::runtime_error);
}

TEST_F(S3InterfaceServiceTest, fetchDataPartialResponse) {
    vector<uint8_t> raw;
    raw.resize(80);
    Response response(RESPONSE_OK, raw);
    EXPECT_CALL(mockRESTfulService, get(_, _)).WillOnce(Return(response));
    vector<uint8_t> buffer;

    EXPECT_THROW(
        this->fetchData(0, buffer, 100,
                        "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever", region),
        std::runtime_error);
}

TEST_F(S3InterfaceServiceTest, checkSmallFile) {
    vector<uint8_t> raw;
    raw.resize(2);
    raw[0] = 0x1f;
    raw[1] = 0x8b;
    Response response(RESPONSE_OK, raw);
    EXPECT_CALL(mockRESTfulService, get(_, _)).WillOnce(Return(response));

    EXPECT_EQ(S3_COMPRESSION_PLAIN,
              this->checkCompressionType(
                  "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever", region));
}

TEST_F(S3InterfaceServiceTest, checkItsGzipCompressed) {
    vector<uint8_t> raw;
    raw.resize(4);
    raw[0] = 0x1f;
    raw[1] = 0x8b;
    Response response(RESPONSE_OK, raw);
    EXPECT_CALL(mockRESTfulService, get(_, _)).WillOnce(Return(response));

    EXPECT_EQ(S3_COMPRESSION_GZIP,
              this->checkCompressionType(
                  "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever", region));
}

TEST_F(S3InterfaceServiceTest, checkItsNotCompressed) {
    vector<uint8_t> raw;
    raw.resize(4);
    raw[0] = 0x1f;
    raw[1] = 0x88;
    Response response(RESPONSE_OK, raw);
    EXPECT_CALL(mockRESTfulService, get(_, _)).WillOnce(Return(response));

    EXPECT_EQ(S3_COMPRESSION_PLAIN,
              this->checkCompressionType(
                  "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever", region));
}

TEST_F(S3InterfaceServiceTest, checkCompreesionTypeWithResponseError) {
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

    EXPECT_CALL(mockRESTfulService, get(_, _)).WillRepeatedly(Return(response));

    EXPECT_THROW(this->checkCompressionType(
                     "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever", region),
                 std::runtime_error);
}

TEST_F(S3InterfaceServiceTest, fetchDataWithResponseError) {
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

    EXPECT_CALL(mockRESTfulService, get(_, _)).WillRepeatedly(Return(response));

    EXPECT_THROW(
        this->fetchData(0, buffer, 128,
                        "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever", region),
        std::runtime_error);
}

TEST_F(S3InterfaceServiceTest, HeadResponseWithHeadResponseFail) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";

    EXPECT_CALL(mockRESTfulService, head(_, _)).Times(3).WillRepeatedly(Return(HeadResponseFail));

    EXPECT_FALSE(this->checkKeyExistence(url, this->region));
}

TEST_F(S3InterfaceServiceTest, HeadResponse200) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";

    EXPECT_CALL(mockRESTfulService, head(_, _)).WillOnce(Return(200));

    EXPECT_TRUE(this->checkKeyExistence(url, this->region));
}

TEST_F(S3InterfaceServiceTest, HeadResponse206) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";

    EXPECT_CALL(mockRESTfulService, head(_, _)).WillOnce(Return(206));

    EXPECT_TRUE(this->checkKeyExistence(url, this->region));
}

TEST_F(S3InterfaceServiceTest, HeadResponse404) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";

    EXPECT_CALL(mockRESTfulService, head(_, _)).WillOnce(Return(404));

    EXPECT_FALSE(this->checkKeyExistence(url, this->region));
}

TEST_F(S3InterfaceServiceTest, HeadResponse403) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";

    EXPECT_CALL(mockRESTfulService, head(_, _)).WillOnce(Return(403));

    EXPECT_FALSE(this->checkKeyExistence(url, this->region));
}

TEST_F(S3InterfaceServiceTest, getUploadIdRoutine) {
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

    EXPECT_CALL(mockRESTfulService, post(_, _, vector<uint8_t>())).WillOnce(Return(response));

    EXPECT_EQ(
        "VXBsb2FkIElEIGZvciA2aWWpbmcncyBteS1tb3ZpZS5tMnRzIHVwbG9hZA",
        this->getUploadId("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever", region));
}

TEST_F(S3InterfaceServiceTest, getUploadIdFailedResponse) {
    vector<uint8_t> raw;
    raw.resize(100);
    Response response(RESPONSE_FAIL, raw);

    EXPECT_CALL(mockRESTfulService, post(_, _, vector<uint8_t>())).WillRepeatedly(Return(response));

    EXPECT_THROW(
        this->getUploadId("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever", region),
        std::runtime_error);
}

TEST_F(S3InterfaceServiceTest, getUploadIdErrorResponse) {
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

    EXPECT_CALL(mockRESTfulService, post(_, _, vector<uint8_t>())).WillRepeatedly(Return(response));

    EXPECT_THROW(
        this->getUploadId("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever", region),
        std::runtime_error);
}

TEST_F(S3InterfaceServiceTest, uploadPartOfDataRoutine) {
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

    EXPECT_CALL(mockRESTfulService, put(_, _, _)).WillOnce(Return(response));

    EXPECT_EQ(
        "\"b54357faf0632cce46e942fa68356b38\"",
        this->uploadPartOfData(raw, "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever",
                               region, 11, "xyz"));
}

TEST_F(S3InterfaceServiceTest, uploadPartOfDataFailedResponse) {
    vector<uint8_t> raw;
    raw.resize(100);
    Response response(RESPONSE_FAIL, raw);
    EXPECT_CALL(mockRESTfulService, put(_, _, _)).WillRepeatedly(Return(response));

    EXPECT_THROW(
        this->uploadPartOfData(raw, "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever",
                               region, 11, "xyz"),
        std::runtime_error);
}

TEST_F(S3InterfaceServiceTest, uploadPartOfDataErrorResponse) {
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

    EXPECT_CALL(mockRESTfulService, put(_, _, _)).WillRepeatedly(Return(response));

    EXPECT_THROW(
        this->uploadPartOfData(raw, "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever",
                               region, 11, "xyz"),
        std::runtime_error);
}

TEST_F(S3InterfaceServiceTest, uploadPartOfDataAbortResponse) {
    vector<uint8_t> raw;
    raw.resize(100);
    Response response(RESPONSE_ABORT, raw);
    EXPECT_CALL(mockRESTfulService, put(_, _, _)).WillRepeatedly(Return(response));

    EXPECT_EQ("", this->uploadPartOfData(
                      raw, "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever", region,
                      11, "xyz"));
}

TEST_F(S3InterfaceServiceTest, completeMultiPartRoutine) {
    vector<uint8_t> raw;
    raw.resize(100);
    Response response(RESPONSE_OK, raw);
    EXPECT_CALL(mockRESTfulService, post(_, _, _)).WillOnce(Return(response));

    vector<string> etagArray = {"\"abc\"", "\"def\""};

    EXPECT_TRUE(this->completeMultiPart(
        "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever", region, "xyz", etagArray));
}

TEST_F(S3InterfaceServiceTest, completeMultiPartFailedResponse) {
    vector<uint8_t> raw;
    raw.resize(100);
    Response response(RESPONSE_FAIL, raw);
    EXPECT_CALL(mockRESTfulService, post(_, _, _)).WillRepeatedly(Return(response));

    vector<string> etagArray = {"\"abc\"", "\"def\""};

    EXPECT_THROW(
        this->completeMultiPart("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever",
                                region, "xyz", etagArray),
        std::runtime_error);
}

TEST_F(S3InterfaceServiceTest, completeMultiPartErrorResponse) {
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

    EXPECT_CALL(mockRESTfulService, post(_, _, _)).WillRepeatedly(Return(response));

    vector<string> etagArray = {"\"abc\"", "\"def\""};

    EXPECT_THROW(
        this->completeMultiPart("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever",
                                region, "xyz", etagArray),
        std::runtime_error);
}

TEST_F(S3InterfaceServiceTest, completeMultiPartAbortResponse) {
    vector<uint8_t> raw;
    raw.resize(100);
    Response response(RESPONSE_ABORT, raw);
    EXPECT_CALL(mockRESTfulService, post(_, _, _)).WillRepeatedly(Return(response));

    vector<string> etagArray = {"\"abc\"", "\"def\""};

    EXPECT_FALSE(this->completeMultiPart(
        "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever", region, "xyz", etagArray));
}

TEST_F(S3InterfaceServiceTest, abortUploadRoutine) {
    vector<uint8_t> raw;
    raw.resize(100);
    Response response(RESPONSE_OK, raw);
    EXPECT_CALL(mockRESTfulService, deleteRequest(_, _)).WillOnce(Return(response));

    EXPECT_TRUE(this->abortUpload("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever",
                                  region, "xyz"));
}

TEST_F(S3InterfaceServiceTest, abortUploadFailedResponse) {
    vector<uint8_t> raw;
    raw.resize(100);
    Response response(RESPONSE_FAIL, raw);
    EXPECT_CALL(mockRESTfulService, deleteRequest(_, _)).WillRepeatedly(Return(response));

    EXPECT_THROW(this->abortUpload("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever",
                                   region, "xyz"),
                 std::runtime_error);
}

TEST_F(S3InterfaceServiceTest, abortUploadErrorResponse) {
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

    EXPECT_CALL(mockRESTfulService, deleteRequest(_, _)).WillRepeatedly(Return(response));

    EXPECT_THROW(this->abortUpload("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever",
                                   region, "xyz"),
                 std::runtime_error);
}
