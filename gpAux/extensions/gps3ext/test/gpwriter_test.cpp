#include "gpwriter.cpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_classes.h"

using ::testing::AtMost;
using ::testing::AtLeast;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::Throw;
using ::testing::_;

class MockGPWriter : public GPWriter {
   public:
    MockGPWriter(const S3Params& params, const string& urlWithOptions,
                 S3RESTfulService* mockService)
        : GPWriter(params, urlWithOptions) {
        restfulServicePtr = mockService;
    }
};

class GPWriterTest : public testing::Test {
   protected:
    virtual void SetUp() {
        InitConfig(this->params, "data/s3test.conf", "default");
    }
    virtual void TearDown() {
    }

    S3Params params;
    MockS3Interface mocks3interface;
};

TEST_F(GPWriterTest, ConstructKeyName) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/normal";

    MockS3RESTfulService mockRestfulService;
    MockGPWriter gpwriter(this->params, url, &mockRestfulService);
    EXPECT_CALL(mockRestfulService, head(_, _, _)).WillOnce(Return(404));

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

    gpwriter.open(this->params);

    // "0"+".data"'s length is 6
    EXPECT_EQ((uint64_t)8, gpwriter.getKeyToUpload().length() - url.length() - 6);
}

TEST_F(GPWriterTest, GenerateUniqueKeyName) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/normal";

    MockS3RESTfulService mockRestfulService;
    MockGPWriter gpwriter(this->params, url, &mockRestfulService);
    EXPECT_CALL(mockRestfulService, head(_, _, _)).Times(AtLeast(1)).WillRepeatedly(Return(404));

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

    EXPECT_CALL(mockRestfulService, post(_, _, _, vector<uint8_t>()))
        .WillOnce(Return(response))
        .WillOnce(Return(response));

    params.setKeyUrl(url);
    gpwriter.open(this->params);

    MockGPWriter gpwriter2(this->params, url, &mockRestfulService);
    EXPECT_CALL(mockRestfulService, head(gpwriter.getKeyToUpload(), _, _))
        .Times(AtMost(1))
        .WillOnce(Return(200));

    gpwriter2.open(params);

    EXPECT_NE(gpwriter.getKeyToUpload(), gpwriter2.getKeyToUpload());
}

TEST_F(GPWriterTest, ReGenerateKeyName) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/normal";
    MockS3RESTfulService mockRestfulService;
    MockGPWriter gpwriter(this->params, url, &mockRestfulService);

    EXPECT_CALL(mockRestfulService, head(_, _, _)).WillOnce(Return(200)).WillOnce(Return(404));

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

    params.setKeyUrl(url);
    gpwriter.open(this->params);

    // expect the restfulService->head() was called twice
}
