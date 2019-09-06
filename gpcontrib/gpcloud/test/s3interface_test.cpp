#include "s3interface.cpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_classes.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;
using ::testing::Throw;

class S3InterfaceServiceTest : public testing::Test, public S3InterfaceService {
   public:
    S3InterfaceServiceTest() : params("s3://a/a"), mockRESTfulService(this->params) {
    }

   protected:
    // Remember that SetUp() is run immediately before a test starts.
    virtual void SetUp() {
        s3ext_logtype = STDERR_LOG;
        s3ext_loglevel = EXT_INFO;

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

    MockS3RESTfulService mockRESTfulService;

    ListBucketResult result;
};

TEST_F(S3InterfaceServiceTest, GetResponseWithZeroRetry) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;

    EXPECT_THROW(this->getResponseWithRetries(url, headers, 0), S3FailedAfterRetry);
}

TEST_F(S3InterfaceServiceTest, GetResponseWithTwoRetries) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;

    EXPECT_CALL(mockRESTfulService, get(_, _))
        .Times(2)
        .WillOnce(Throw(S3ConnectionError("")))
        .WillOnce(Throw(S3ConnectionError("")));

    EXPECT_THROW(this->getResponseWithRetries(url, headers, 2), S3FailedAfterRetry);
}

TEST_F(S3InterfaceServiceTest, GetResponseWithRetriesAndSuccess) {
    string url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever");
    HTTPHeaders headers;

    EXPECT_CALL(mockRESTfulService, get(_, _))
        .Times(2)
        .WillOnce(Throw(S3ConnectionError("")))
        .WillOnce(Return(Response(RESPONSE_OK)));

    EXPECT_EQ(RESPONSE_OK, this->getResponseWithRetries(url, headers).getStatus());
}

TEST_F(S3InterfaceServiceTest, PutResponseWithZeroRetry) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;

    S3VectorUInt8 data;
    EXPECT_THROW(this->putResponseWithRetries(url, headers, data, 0), S3FailedAfterRetry);
}

TEST_F(S3InterfaceServiceTest, PutResponseWithTwoRetries) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;

    S3VectorUInt8 data;

    EXPECT_CALL(mockRESTfulService, put(_, _, _))
        .Times(2)
        .WillOnce(Throw(S3ConnectionError("")))
        .WillOnce(Throw(S3ConnectionError("")));

    EXPECT_THROW(this->putResponseWithRetries(url, headers, data, 2), S3FailedAfterRetry);
}

TEST_F(S3InterfaceServiceTest, PutResponseWithRetriesAndSuccess) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;

    S3VectorUInt8 data;

    EXPECT_CALL(mockRESTfulService, put(_, _, _))
        .Times(2)
        .WillOnce(Throw(S3ConnectionError("")))
        .WillOnce(Return(Response(RESPONSE_OK)));

    EXPECT_EQ(RESPONSE_OK, this->putResponseWithRetries(url, headers, data).getStatus());
}

TEST_F(S3InterfaceServiceTest, HeadResponseWithZeroRetry) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;

    EXPECT_THROW(this->headResponseWithRetries(url, headers, 0), S3FailedAfterRetry);
}

TEST_F(S3InterfaceServiceTest, HeadResponseWithRetriesAndFail) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;

    EXPECT_CALL(mockRESTfulService, head(_, _))
        .Times(S3_REQUEST_MAX_RETRIES)
        .WillRepeatedly(Throw(S3ConnectionError("")));

    EXPECT_THROW(this->headResponseWithRetries(url, headers), S3FailedAfterRetry);
}

TEST_F(S3InterfaceServiceTest, HeadResponseWithRetriesAndSuccess) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;

    EXPECT_CALL(mockRESTfulService, head(_, _))
        .Times(2)
        .WillOnce(Throw(S3ConnectionError("")))
        .WillOnce(Return(404));

    EXPECT_EQ(404, this->headResponseWithRetries(url, headers));
}

TEST_F(S3InterfaceServiceTest, PostResponseWithZeroRetry) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;
    vector<uint8_t> data;

    EXPECT_THROW(this->postResponseWithRetries(url, headers, data, 0), S3FailedAfterRetry);
}

TEST_F(S3InterfaceServiceTest, PostResponseWithTwoRetries) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;
    vector<uint8_t> data;

    EXPECT_CALL(mockRESTfulService, post(_, _, _))
        .Times(2)
        .WillOnce(Throw(S3ConnectionError("")))
        .WillOnce(Throw(S3ConnectionError("")));

    EXPECT_THROW(this->postResponseWithRetries(url, headers, data, 2), S3FailedAfterRetry);
}

TEST_F(S3InterfaceServiceTest, PostResponseWithRetriesAndSuccess) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever";
    HTTPHeaders headers;
    vector<uint8_t> data;

    EXPECT_CALL(mockRESTfulService, post(_, _, _))
        .Times(2)
        .WillOnce(Throw(S3ConnectionError("")))
        .WillOnce(Return(Response(RESPONSE_OK)));

    EXPECT_EQ(RESPONSE_OK, this->postResponseWithRetries(url, headers, data).getStatus());
}

TEST_F(S3InterfaceServiceTest, ListBucketWithWrongRegion) {
    EXPECT_CALL(mockRESTfulService, get(_, _)).WillRepeatedly(Throw(S3ConnectionError("")));

    S3Url s3Url("s3://s3-noexist.amazonaws.com/bucket/prefix");
    EXPECT_THROW(this->listBucket(s3Url), S3FailedAfterRetry);
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

    EXPECT_THROW(this->listBucket(this->params.getS3Url()), S3LogicError);
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

    result = this->listBucket(this->params.getS3Url());
    EXPECT_EQ((uint64_t)1, result.contents.size());
}

TEST_F(S3InterfaceServiceTest, ListBucketWithBucketWith1000Keys) {
    EXPECT_CALL(mockRESTfulService, get(_, _))
        .WillOnce(Return(this->buildListBucketResponse(1000, false)));

    result = this->listBucket(this->params.getS3Url());
    EXPECT_EQ((uint64_t)1000, result.contents.size());
}

TEST_F(S3InterfaceServiceTest, ListBucketWithBucketWith1001Keys) {
    EXPECT_CALL(mockRESTfulService, get(_, _))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(1, false)));

    result = this->listBucket(this->params.getS3Url());
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

    result = this->listBucket(this->params.getS3Url());
    EXPECT_EQ((uint64_t)5120, result.contents.size());
}

TEST_F(S3InterfaceServiceTest, ListBucketWithBucketWithTruncatedResponse) {
    EXPECT_CALL(mockRESTfulService, get(_, _))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillRepeatedly(Throw(S3ConnectionError("")));

    EXPECT_THROW(this->listBucket(this->params.getS3Url()), S3FailedAfterRetry);
}

TEST_F(S3InterfaceServiceTest, ListBucketWithBucketWithZeroSizedKeys) {
    EXPECT_CALL(mockRESTfulService, get(_, _))
        .WillOnce(Return(this->buildListBucketResponse(0, true, 8)))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(120, false, 8)));

    result = this->listBucket(this->params.getS3Url());
    EXPECT_EQ((uint64_t)1120, result.contents.size());
}

TEST_F(S3InterfaceServiceTest, ListBucketWithEmptyBucket) {
    EXPECT_CALL(mockRESTfulService, get(_, _))
        .WillOnce(Return(this->buildListBucketResponse(0, false, 0)));

    result = this->listBucket(this->params.getS3Url());

    EXPECT_EQ((uint64_t)0, result.contents.size());
}

TEST_F(S3InterfaceServiceTest, ListBucketWithAllZeroedFilesBucket) {
    EXPECT_CALL(mockRESTfulService, get(_, _))
        .WillOnce(Return(this->buildListBucketResponse(0, false, 2)));

    result = this->listBucket(this->params.getS3Url());
    EXPECT_EQ((uint64_t)0, result.contents.size());
}

TEST_F(S3InterfaceServiceTest, ListBucketWithErrorResponse) {
    EXPECT_CALL(mockRESTfulService, get(_, _)).WillRepeatedly(Throw(S3ConnectionError("")));

    EXPECT_THROW(this->listBucket(this->params.getS3Url()), S3FailedAfterRetry);
}

TEST_F(S3InterfaceServiceTest, ListBucketWithErrorReturnedXML) {
    uint8_t xml[] = "whatever";
    vector<uint8_t> raw(xml, xml + sizeof(xml) - 1);
    Response response(RESPONSE_ERROR, raw);

    EXPECT_CALL(mockRESTfulService, get(_, _)).WillRepeatedly(Return(response));

    EXPECT_THROW(this->listBucket(this->params.getS3Url()), S3LogicError);
}

TEST_F(S3InterfaceServiceTest, ListBucketWithNonRootXML) {
    uint8_t xml[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    vector<uint8_t> raw(xml, xml + sizeof(xml) - 1);
    Response response(RESPONSE_ERROR, raw);

    EXPECT_CALL(mockRESTfulService, get(_, _)).WillRepeatedly(Return(response));

    EXPECT_THROW(this->listBucket(this->params.getS3Url()), S3LogicError);
}

TEST_F(S3InterfaceServiceTest, fetchDataRoutine) {
    vector<uint8_t> raw;

    srand(time(NULL));

    for (int i = 0; i < 100; i++) {
        raw.push_back(rand() & 0xFF);
    }

    Response response(RESPONSE_OK, raw);
    EXPECT_CALL(mockRESTfulService, get(_, _)).WillOnce(Return(response));

    S3VectorUInt8 buffer;

    uint64_t len = this->fetchData(
        0, buffer, 100, S3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever"));

    EXPECT_EQ(buffer.size(), raw.size());
    EXPECT_EQ(0, memcmp(buffer.data(), raw.data(), 100));
    EXPECT_EQ((uint64_t)100, len);
}

TEST_F(S3InterfaceServiceTest, fetchDataErrorResponse) {
    vector<uint8_t> raw;
    raw.resize(100);
    Response response(RESPONSE_ERROR, raw);
    EXPECT_CALL(mockRESTfulService, get(_, _)).WillRepeatedly(Return(response));

    S3VectorUInt8 buffer;

    EXPECT_THROW(
        this->fetchData(0, buffer, 100,
                        S3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever")),
        S3LogicError);
}

TEST_F(S3InterfaceServiceTest, fetchDataFailedResponse) {
    vector<uint8_t> raw;
    raw.resize(100);
    EXPECT_CALL(mockRESTfulService, get(_, _)).WillRepeatedly(Throw(S3ConnectionError("")));

    S3VectorUInt8 buffer;

    EXPECT_THROW(
        this->fetchData(0, buffer, 100,
                        S3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever")),
        S3FailedAfterRetry);
}

TEST_F(S3InterfaceServiceTest, fetchDataPartialResponse) {
    vector<uint8_t> raw;
    raw.resize(80);
    Response response(RESPONSE_OK, raw);
    EXPECT_CALL(mockRESTfulService, get(_, _)).WillOnce(Return(response));
    S3VectorUInt8 buffer;

    EXPECT_THROW(
        this->fetchData(0, buffer, 100,
                        S3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever")),
        S3PartialResponseError);
}

TEST_F(S3InterfaceServiceTest, checkSmallFile) {
    vector<uint8_t> raw;
    raw.resize(2);
    raw[0] = 0x1f;
    raw[1] = 0x8b;
    Response response(RESPONSE_OK, raw);
    EXPECT_CALL(mockRESTfulService, get(_, _)).WillOnce(Return(response));

    S3Url s3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever");
    EXPECT_EQ(S3_COMPRESSION_PLAIN, this->checkCompressionType(s3Url.getFullUrlForCurl()));
}

TEST_F(S3InterfaceServiceTest, checkItsGzipCompressed) {
    vector<uint8_t> raw;
    raw.resize(4);
    raw[0] = 0x1f;
    raw[1] = 0x8b;
    Response response(RESPONSE_OK, raw);
    EXPECT_CALL(mockRESTfulService, get(_, _)).WillOnce(Return(response));

    S3Url s3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever");
    EXPECT_EQ(S3_COMPRESSION_GZIP, this->checkCompressionType(s3Url));
}

TEST_F(S3InterfaceServiceTest, checkItsNotCompressed) {
    vector<uint8_t> raw;
    raw.resize(4);
    raw[0] = 0x1f;
    raw[1] = 0x88;
    Response response(RESPONSE_OK, raw);
    EXPECT_CALL(mockRESTfulService, get(_, _)).WillOnce(Return(response));

    S3Url s3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever");
    EXPECT_EQ(S3_COMPRESSION_PLAIN, this->checkCompressionType(s3Url));
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

    S3Url s3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever");
    EXPECT_THROW(this->checkCompressionType(s3Url), S3LogicError);
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
    S3VectorUInt8 buffer;

    EXPECT_CALL(mockRESTfulService, get(_, _)).WillRepeatedly(Return(response));

    EXPECT_THROW(
        this->fetchData(0, buffer, 128,
                        S3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever")),
        S3LogicError);
}

TEST_F(S3InterfaceServiceTest, HeadResponseWithHeadResponseFail) {
    S3Url s3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever");

    EXPECT_CALL(mockRESTfulService, head(_, _))
        .Times(S3_REQUEST_MAX_RETRIES)
        .WillRepeatedly(Throw(S3ConnectionError("")));

    EXPECT_THROW(this->checkKeyExistence(s3Url), S3FailedAfterRetry);
}

TEST_F(S3InterfaceServiceTest, HeadResponse200) {
    S3Url s3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever");

    EXPECT_CALL(mockRESTfulService, head(_, _)).WillOnce(Return(200));

    EXPECT_TRUE(this->checkKeyExistence(s3Url));
}

TEST_F(S3InterfaceServiceTest, HeadResponse206) {
    S3Url s3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever");

    EXPECT_CALL(mockRESTfulService, head(_, _)).WillOnce(Return(206));

    EXPECT_TRUE(this->checkKeyExistence(s3Url));
}

TEST_F(S3InterfaceServiceTest, HeadResponse404) {
    S3Url s3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever");

    EXPECT_CALL(mockRESTfulService, head(_, _)).WillOnce(Return(404));

    EXPECT_FALSE(this->checkKeyExistence(s3Url));
}

TEST_F(S3InterfaceServiceTest, HeadResponse403) {
    S3Url s3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever");

    EXPECT_CALL(mockRESTfulService, head(_, _)).WillOnce(Return(403));

    EXPECT_FALSE(this->checkKeyExistence(s3Url));
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
        this->getUploadId(S3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever")));
}

TEST_F(S3InterfaceServiceTest, getUploadIdFailedResponse) {
    vector<uint8_t> raw;
    raw.resize(100);

    EXPECT_CALL(mockRESTfulService, post(_, _, vector<uint8_t>()))
        .WillRepeatedly(Throw(S3ConnectionError("")));

    EXPECT_THROW(
        this->getUploadId(S3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever")),
        S3FailedAfterRetry);
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
        this->getUploadId(S3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever")),
        S3LogicError);
}

TEST_F(S3InterfaceServiceTest, uploadPartOfDataRoutine) {
    S3VectorUInt8 raw;
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

    EXPECT_EQ("\"b54357faf0632cce46e942fa68356b38\"",
              this->uploadPartOfData(
                  raw, S3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever"), 11,
                  "xyz"));
}

TEST_F(S3InterfaceServiceTest, uploadPartOfDataFailedResponse) {
    S3VectorUInt8 raw;
    raw.resize(100);
    EXPECT_CALL(mockRESTfulService, put(_, _, _)).WillRepeatedly(Throw(S3ConnectionError("")));

    EXPECT_THROW(
        this->uploadPartOfData(
            raw, S3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever"), 11, "xyz"),
        S3FailedAfterRetry);
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

    S3VectorUInt8 data;
    EXPECT_THROW(this->uploadPartOfData(
                     data, S3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever"),
                     11, "xyz"),
                 S3LogicError);
}

TEST_F(S3InterfaceServiceTest, uploadPartOfDataAbortResponse) {
    S3VectorUInt8 raw;
    raw.resize(100);

    EXPECT_CALL(mockRESTfulService, put(_, _, _)).WillRepeatedly(Throw(S3QueryAbort()));

    EXPECT_THROW(
        this->uploadPartOfData(
            raw, S3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever"), 11, "xyz"),
        S3QueryAbort);
}

TEST_F(S3InterfaceServiceTest, completeMultiPartRoutine) {
    vector<uint8_t> raw;
    raw.resize(100);
    Response response(RESPONSE_OK, raw);
    EXPECT_CALL(mockRESTfulService, post(_, _, _)).WillOnce(Return(response));

    vector<string> etagArray = {"\"abc\"", "\"def\""};

    EXPECT_TRUE(this->completeMultiPart(
        S3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever"), "xyz", etagArray));
}

TEST_F(S3InterfaceServiceTest, completeMultiPartFailedResponse) {
    vector<uint8_t> raw;
    raw.resize(100);
    EXPECT_CALL(mockRESTfulService, post(_, _, _)).WillRepeatedly(Throw(S3ConnectionError("")));

    vector<string> etagArray = {"\"abc\"", "\"def\""};

    EXPECT_THROW(this->completeMultiPart(
                     S3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever"), "xyz",
                     etagArray),
                 S3FailedAfterRetry);
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

    EXPECT_THROW(this->completeMultiPart(
                     S3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever"), "xyz",
                     etagArray),
                 S3LogicError);
}

TEST_F(S3InterfaceServiceTest, completeMultiPartAbortResponse) {
    vector<uint8_t> raw;
    raw.resize(100);

    EXPECT_CALL(mockRESTfulService, post(_, _, _)).WillRepeatedly(Throw(S3QueryAbort()));

    vector<string> etagArray = {"\"abc\"", "\"def\""};

    EXPECT_THROW(this->completeMultiPart(
                     S3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever"), "xyz",
                     etagArray),
                 S3QueryAbort);
}

TEST_F(S3InterfaceServiceTest, abortUploadRoutine) {
    vector<uint8_t> raw;
    raw.resize(100);
    Response response(RESPONSE_OK, raw);
    EXPECT_CALL(mockRESTfulService, deleteRequest(_, _)).WillOnce(Return(response));

    EXPECT_TRUE(this->abortUpload(
        S3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever"), "xyz"));
}

TEST_F(S3InterfaceServiceTest, abortUploadFailedResponse) {
    vector<uint8_t> raw;
    raw.resize(100);
    EXPECT_CALL(mockRESTfulService, deleteRequest(_, _))
        .WillRepeatedly(Throw(S3ConnectionError("")));

    EXPECT_THROW(this->abortUpload(
                     S3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever"), "xyz"),
                 S3FailedAfterRetry);
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

    EXPECT_THROW(this->abortUpload(
                     S3Url("https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/whatever"), "xyz"),
                 S3LogicError);
}
