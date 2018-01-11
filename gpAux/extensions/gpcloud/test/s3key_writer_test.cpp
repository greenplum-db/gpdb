#include "s3key_writer.cpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_classes.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::Throw;

class S3KeyWriterTest : public testing::Test, public S3KeyWriter {
   public:
    S3KeyWriterTest() : testParams("s3://abc/def") {
    }

   protected:
    virtual void SetUp() {
        this->setS3InterfaceService(&mockS3Interface);

        testParams.setChunkSize(1000);
        testParams.setCred({"accessid", "secret"});
        testParams.setNumOfChunks(3);
    }

    virtual void TearDown() {
    }

    MockS3Interface mockS3Interface;
    S3Params testParams;
};

TEST_F(S3KeyWriterTest, TestEmptyParams) {
    S3Params params("s3://abc/def");

    // zero chunk size causes exception
    EXPECT_THROW(this->open(params), S3RuntimeError);
}

TEST_F(S3KeyWriterTest, TestValidParams) {
    EXPECT_CALL(mockS3Interface, getUploadId(_)).WillOnce(Return("uploadId"));

    this->open(this->testParams);

    EXPECT_EQ(buffer.capacity(), this->testParams.getChunkSize());
}

TEST_F(S3KeyWriterTest, TestEmptyWrite) {
    EXPECT_CALL(mockS3Interface, getUploadId(_)).WillOnce(Return("uploadId"));

    this->open(testParams);

    EXPECT_THROW(this->write(NULL, 0), S3RuntimeError);
}

TEST_F(S3KeyWriterTest, TestZeroWrite) {
    char buff[0x10];
    EXPECT_CALL(mockS3Interface, getUploadId(_)).WillOnce(Return("uploadId"));

    this->open(testParams);

    EXPECT_EQ((uint64_t)0, this->write(buff, 0));
    EXPECT_EQ((uint64_t)0, buffer.size());
}

TEST_F(S3KeyWriterTest, TestSmallWrite) {
    testParams.setChunkSize(0x100);

    char data[0x10];
    EXPECT_CALL(this->mockS3Interface, getUploadId(_)).WillOnce(Return("uploadId"));
    EXPECT_CALL(this->mockS3Interface, uploadPartOfData(_, _, _, _)).WillOnce(Return("\"etag\""));
    EXPECT_CALL(this->mockS3Interface, completeMultiPart(_, _, _)).WillOnce(Return(true));

    this->open(testParams);
    ASSERT_EQ(sizeof(data), this->write(data, sizeof(data)));
    ASSERT_EQ(buffer.size(), sizeof(data));

    // Buffer is not full, so call close() to force upload.
    this->close();
}

TEST_F(S3KeyWriterTest, TestChunkSizeSmallerThanInput) {
    testParams.setChunkSize(0x100);
    EXPECT_CALL(mockS3Interface, getUploadId(_)).WillOnce(Return("uploadId"));
    EXPECT_CALL(mockS3Interface, uploadPartOfData(_, _, _, _))
        .WillOnce(Return("\"etag1\""))
        .WillOnce(Return("\"etag2\""));
    EXPECT_CALL(this->mockS3Interface, completeMultiPart(_, _, _)).WillOnce(Return(true));

    char data[0x101];
    this->open(testParams);
    EXPECT_EQ(sizeof(data), this->write(data, sizeof(data)));
    this->close();
}

class MockUploadPartOfData {
   public:
    MockUploadPartOfData(uint64_t expectedLen) : expectedLength(expectedLen) {
    }

    MockUploadPartOfData(const MockUploadPartOfData &other) {
        expectedLength = other.expectedLength;
    }

    string operator()(S3VectorUInt8 &data, const S3Url &s3Url, uint64_t partNumber,
                      const string &uploadId) {
        EXPECT_EQ(data.size(), expectedLength);
        return "\"etag\"";
    }

   private:
    uint64_t expectedLength;
};

TEST_F(S3KeyWriterTest, TestBufferedWrite) {
    testParams.setChunkSize(0x100);

    char data[0x100];
    EXPECT_CALL(this->mockS3Interface, getUploadId(_)).WillOnce(Return("uploadid1"));
    EXPECT_CALL(this->mockS3Interface, uploadPartOfData(_, _, 1, "uploadid1"))
        .WillOnce(Invoke(MockUploadPartOfData(0x100)));
    EXPECT_CALL(this->mockS3Interface, uploadPartOfData(_, _, 2, "uploadid1"))
        .WillOnce(Invoke(MockUploadPartOfData(0x1)));
    EXPECT_CALL(this->mockS3Interface, completeMultiPart(_, _, _)).WillOnce(Return(true));

    this->open(testParams);
    ASSERT_EQ((uint64_t)0x80, this->write(data, 0x80));

    // Buffer have little space, will uploadData and clear buffer.
    ASSERT_EQ((uint64_t)0x81, this->write(data, 0x81));

    // Buffer is not empty, close() will upload remaining data in buffer.
    this->close();
}

TEST_F(S3KeyWriterTest, TestUploadContent) {
    testParams.setChunkSize(0x100);

    char data[] = "The quick brown fox jumps over the lazy dog";
    EXPECT_CALL(this->mockS3Interface, getUploadId(_)).WillOnce(Return("uploadId"));
    EXPECT_CALL(this->mockS3Interface, uploadPartOfData(_, _, _, _)).WillOnce(Return("\"etag\""));
    EXPECT_CALL(this->mockS3Interface, completeMultiPart(_, _, _)).WillOnce(Return(true));

    this->open(testParams);
    ASSERT_EQ(sizeof(data), this->write(data, sizeof(data)));
    ASSERT_EQ(sizeof(data), buffer.size());
    ASSERT_TRUE(memcmp(data, buffer.data(), sizeof(data)) == 0);

    // Buffer is not empty, close() will upload remaining data in buffer.
    this->close();
}

TEST_F(S3KeyWriterTest, TestWriteAbortInWriting) {
    testParams.setChunkSize(0x100);

    char data[0x100];
    EXPECT_CALL(this->mockS3Interface, getUploadId(_)).WillOnce(Return("uploadid1"));
    EXPECT_CALL(this->mockS3Interface, abortUpload(_, _)).WillOnce(Return(true));

    this->open(testParams);
    QueryCancelPending = true;

    EXPECT_THROW(this->write(data, 0x80), S3QueryAbort);

    QueryCancelPending = false;
}

TEST_F(S3KeyWriterTest, TestWriteAbortInClosing) {
    testParams.setChunkSize(0x100);

    char data[0x100];
    EXPECT_CALL(this->mockS3Interface, getUploadId(_)).WillOnce(Return("uploadid1"));
    EXPECT_CALL(this->mockS3Interface, uploadPartOfData(_, _, 1, "uploadid1"))
        .WillOnce(Invoke(MockUploadPartOfData(0x100)));
    EXPECT_CALL(this->mockS3Interface, abortUpload(_, _)).WillOnce(Return(true));

    this->open(testParams);
    ASSERT_EQ((uint64_t)0x80, this->write(data, 0x80));

    // Buffer have little space, will uploadData and clear buffer.
    ASSERT_EQ((uint64_t)0x81, this->write(data, 0x81));

    QueryCancelPending = true;
    // Buffer is not empty, close() will upload remaining data in buffer.
    EXPECT_THROW(this->close(), S3QueryAbort);
    QueryCancelPending = false;
}
