#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "mock_classes.h"
#include "s3key_writer.cpp"
#include "s3params.h"

using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::Throw;
using ::testing::_;

class S3KeyWriterTest : public testing::Test, public S3KeyWriter {
   protected:
    virtual void SetUp() {
        this->setS3interface(&mocks3interface);

        testParams.setKeyUrl("testurl");
        testParams.setRegion("testregion");
        testParams.setChunkSize(1000);
        testParams.setCred({"accessid", "secret"});
    }

    virtual void TearDown() {
    }

    MockS3Interface mocks3interface;
    WriterParams testParams;
};

TEST_F(S3KeyWriterTest, TestEmptyParams) {
    WriterParams param;

    // zero chunk size causes exception
    EXPECT_THROW(this->open(param), std::runtime_error);
}

TEST_F(S3KeyWriterTest, TestValidParams) {
    EXPECT_CALL(mocks3interface, getUploadId(_, _, _)).WillOnce(Return("uploadId"));

    this->open(testParams);

    EXPECT_EQ(url, testParams.getKeyUrl());
    EXPECT_EQ(region, testParams.getRegion());
    EXPECT_EQ(chunkSize, testParams.getChunkSize());
    EXPECT_EQ(cred, testParams.getCred());
    EXPECT_EQ(buffer.capacity(), testParams.getChunkSize());
}

TEST_F(S3KeyWriterTest, TestEmptyWrite) {
    EXPECT_CALL(mocks3interface, getUploadId(_, _, _)).WillOnce(Return("uploadId"));

    this->open(testParams);

    EXPECT_THROW(this->write(NULL, 0), std::runtime_error);
}

TEST_F(S3KeyWriterTest, TestZeroWrite) {
    char buff[0x10];
    EXPECT_CALL(mocks3interface, getUploadId(_, _, _)).WillOnce(Return("uploadId"));

    this->open(testParams);

    EXPECT_EQ(0, this->write(buff, 0));
    EXPECT_EQ(0, buffer.size());
}

TEST_F(S3KeyWriterTest, TestSmallWrite) {
    testParams.setChunkSize(0x100);

    char data[0x10];
    EXPECT_CALL(this->mocks3interface, getUploadId(_, _, _)).WillOnce(Return("uploadId"));
    EXPECT_CALL(this->mocks3interface, uploadPartOfData(_, _, _, _, _, _))
        .WillOnce(Return("\"etag\""));
    EXPECT_CALL(this->mocks3interface, completeMultiPart(_, _, _, _, _)).WillOnce(Return(true));

    this->open(testParams);
    ASSERT_EQ(sizeof(data), this->write(data, sizeof(data)));
    ASSERT_EQ(buffer.size(), sizeof(data));

    // Buffer is not full, so call close() to force upload.
    this->close();
}

TEST_F(S3KeyWriterTest, TestChunkSizeSmallerThanInput) {
    testParams.setChunkSize(0x100);
    EXPECT_CALL(mocks3interface, getUploadId(_, _, _)).WillOnce(Return("uploadId"));
    EXPECT_CALL(mocks3interface, uploadPartOfData(_, _, _, _, _, _))
        .WillOnce(Return("\"etag1\""))
        .WillOnce(Return("\"etag2\""));
    EXPECT_CALL(this->mocks3interface, completeMultiPart(_, _, _, _, _)).WillOnce(Return(true));

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

    string operator()(vector<uint8_t> &data, const string &keyUrl, const string &region,
                      const S3Credential &cred, uint64_t partNumber, const string &uploadId) {
        EXPECT_EQ(data.size(), expectedLength);
        return "\"etag\"";
    }

   private:
    uint64_t expectedLength;
};

TEST_F(S3KeyWriterTest, TestBufferedWrite) {
    testParams.setChunkSize(0x100);

    char data[0x100];
    EXPECT_CALL(this->mocks3interface, getUploadId(_, _, _)).WillOnce(Return("uploadid1"));
    EXPECT_CALL(this->mocks3interface, uploadPartOfData(_, _, _, _, 1, "uploadid1"))
        .WillOnce(Invoke(MockUploadPartOfData(0x100)));
    EXPECT_CALL(this->mocks3interface, uploadPartOfData(_, _, _, _, 2, "uploadid1"))
        .WillOnce(Invoke(MockUploadPartOfData(0x1)));
    EXPECT_CALL(this->mocks3interface, completeMultiPart(_, _, _, _, _)).WillOnce(Return(true));

    this->open(testParams);
    ASSERT_EQ(0x80, this->write(data, 0x80));

    // Buffer have little space, will uploadData and clear buffer.
    ASSERT_EQ(0x81, this->write(data, 0x81));

    // Buffer is not empty, close() will upload remaining data in buffer.
    this->close();
}

TEST_F(S3KeyWriterTest, TestUploadContent) {
    testParams.setChunkSize(0x100);

    char data[] = "The quick brown fox jumps over the lazy dog";
    EXPECT_CALL(this->mocks3interface, getUploadId(_, _, _)).WillOnce(Return("uploadId"));
    EXPECT_CALL(this->mocks3interface, uploadPartOfData(_, _, _, _, _, _))
        .WillOnce(Return("\"etag\""));
    EXPECT_CALL(this->mocks3interface, completeMultiPart(_, _, _, _, _)).WillOnce(Return(true));

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
    EXPECT_CALL(this->mocks3interface, getUploadId(_, _, _)).WillOnce(Return("uploadid1"));
    EXPECT_CALL(this->mocks3interface, abortUpload(_, _, _, _)).WillOnce(Return(true));

    this->open(testParams);
    QueryCancelPending = true;

    EXPECT_THROW(this->write(data, 0x80), std::runtime_error);

    QueryCancelPending = false;
}

TEST_F(S3KeyWriterTest, TestWriteAbortInClosing) {
    testParams.setChunkSize(0x100);

    char data[0x100];
    EXPECT_CALL(this->mocks3interface, getUploadId(_, _, _)).WillOnce(Return("uploadid1"));
    EXPECT_CALL(this->mocks3interface, uploadPartOfData(_, _, _, _, 1, "uploadid1"))
        .WillOnce(Invoke(MockUploadPartOfData(0x100)));
    EXPECT_CALL(this->mocks3interface, abortUpload(_, _, _, _)).WillOnce(Return(true));

    this->open(testParams);
    ASSERT_EQ(0x80, this->write(data, 0x80));

    // Buffer have little space, will uploadData and clear buffer.
    ASSERT_EQ(0x81, this->write(data, 0x81));

    QueryCancelPending = true;
    // Buffer is not empty, close() will upload remaining data in buffer.
    EXPECT_THROW(this->close(), std::runtime_error);
    QueryCancelPending = false;
}