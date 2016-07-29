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
    MockGPWriter(const string& urlWithOptions, S3RESTfulService* mockService)
        : GPWriter(urlWithOptions) {
        restfulServicePtr = mockService;
    }
};

class GPWriterTest : public testing::Test {
   protected:
    virtual void SetUp() {
        InitConfig("data/s3test.conf", "default");
    }
    virtual void TearDown() {
    }
};

TEST_F(GPWriterTest, ConstructKeyName) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/normal";

    MockS3RESTfulService mockRestfulService;
    MockGPWriter gpwriter(url, &mockRestfulService);
    EXPECT_CALL(mockRestfulService, head(_, _, _)).WillOnce(Return(404));

    WriterParams params;
    params.setUrlToUpload(url);
    gpwriter.open(params);

    // "0"+".data"'s length is 6
    EXPECT_EQ(8, gpwriter.getKeyToUpload().length() - url.length() - 6);
}

TEST_F(GPWriterTest, GenerateUniqueKeyName) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/normal";

    MockS3RESTfulService mockRestfulService;
    MockGPWriter gpwriter(url, &mockRestfulService);
    EXPECT_CALL(mockRestfulService, head(_, _, _)).Times(AtLeast(1)).WillRepeatedly(Return(404));

    WriterParams params;
    params.setUrlToUpload(url);
    gpwriter.open(params);

    MockGPWriter gpwriter2(url, &mockRestfulService);
    EXPECT_CALL(mockRestfulService, head(gpwriter.getKeyToUpload(), _, _))
        .Times(AtMost(1))
        .WillOnce(Return(200));

    gpwriter2.open(params);

    EXPECT_NE(gpwriter.getKeyToUpload(), gpwriter2.getKeyToUpload());
}

TEST_F(GPWriterTest, ReGenerateKeyName) {
    string url = "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/normal";
    MockS3RESTfulService mockRestfulService;
    MockGPWriter gpwriter(url, &mockRestfulService);

    EXPECT_CALL(mockRestfulService, head(_, _, _)).WillOnce(Return(200)).WillOnce(Return(404));

    WriterParams params;
    params.setUrlToUpload(url);
    gpwriter.open(params);

    // expect the restfulService->head() was called twice
}
