#include "gpwriter.cpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_classes.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::Throw;

class MockGPWriter : public GPWriter {
   public:
    MockGPWriter(const S3Params& params, S3RESTfulService* mockRESTfulService) : GPWriter(params) {
        restfulServicePtr = mockRESTfulService;
    }
};

class GPWriterTest : public testing::Test {
   protected:
    virtual void SetUp() {
    }
    virtual void TearDown() {
    }

    MockS3Interface mockS3Interface;
};

TEST_F(GPWriterTest, ConstructKeyName) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/normal";

    S3Params p = InitConfig(url + " config=data/s3test.conf");
    p.setAutoCompress(false);

    MockS3RESTfulService mockRESTfulService(p);
    MockGPWriter gpwriter(p, &mockRESTfulService);
    EXPECT_CALL(mockRESTfulService, head(_, _)).WillOnce(Return(404));

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

    gpwriter.open(p);

    // "0"+".data"'s length is 6
    EXPECT_EQ((uint64_t)8, gpwriter.getKeyUrlToUpload().length() - url.length() - 6);
}

TEST_F(GPWriterTest, GenerateUniqueKeyName) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/normal";
    S3Params p = InitConfig(url + " config=data/s3test.conf");
    p.setAutoCompress(false);

    MockS3RESTfulService mockRESTfulService(p);
    MockGPWriter gpwriter(p, &mockRESTfulService);
    EXPECT_CALL(mockRESTfulService, head(_, _)).Times(AtLeast(1)).WillRepeatedly(Return(404));

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

    EXPECT_CALL(mockRESTfulService, post(_, _, vector<uint8_t>()))
        .WillOnce(Return(response))
        .WillOnce(Return(response));

    gpwriter.open(p);

    MockGPWriter gpwriter2(p, &mockRESTfulService);
    EXPECT_CALL(mockRESTfulService, head(gpwriter.getKeyUrlToUpload(), _))
        .Times(AtMost(1))
        .WillOnce(Return(200));

    gpwriter2.open(p);

    EXPECT_NE(gpwriter.getKeyUrlToUpload(), gpwriter2.getKeyUrlToUpload());
}

TEST_F(GPWriterTest, ReGenerateKeyName) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/normal";

    S3Params p = InitConfig(url + " config=data/s3test.conf");
    p.setAutoCompress(false);

    MockS3RESTfulService mockRESTfulService(p);
    MockGPWriter gpwriter(p, &mockRESTfulService);

    EXPECT_CALL(mockRESTfulService, head(_, _)).WillOnce(Return(200)).WillOnce(Return(404));

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

    gpwriter.open(p);

    // expect the restfulService->head() was called twice
}
