#include "s3common_reader.cpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_classes.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::Throw;

class MockS3InterfaceForCompressionRead : public MockS3Interface {
   public:
    void setData(Byte *rawData, uLong len) {
        data.insert(data.begin(), rawData, rawData + len);
    }
    uint64_t mockFetchData(uint64_t offset, S3VectorUInt8 &data, uint64_t len, const S3Url &s3Url) {
        data = std::move(this->data);
        return data.size();
    }

    S3VectorUInt8 data;
};

class S3CommonReaderTest : public ::testing::Test, public S3CommonReader {
   protected:
    // Remember that SetUp() is run immediately before a test starts.
    virtual void SetUp() {
        this->setS3InterfaceService(&mockS3Interface);
    }

    // TearDown() is invoked immediately after a test finishes.
    virtual void TearDown() {
        this->close();
    }

    MockS3InterfaceForCompressionRead mockS3Interface;
};

TEST_F(S3CommonReaderTest, OpenGZip) {
    // test case for: the file format is gzip, then decompressReader should be called
    EXPECT_CALL(mockS3Interface, checkCompressionType(_)).WillOnce(Return(S3_COMPRESSION_GZIP));
    S3Params params("s3://abc/def");
    params.setNumOfChunks(1);
    params.setChunkSize(1024 * 1024 * 2);
    this->open(params);

    ASSERT_EQ(this->upstreamReader, &this->decompressReader);
    ASSERT_TRUE(NULL != dynamic_cast<DecompressReader *>(this->upstreamReader));
}

TEST_F(S3CommonReaderTest, OpenPlain) {
    // test case for: the file format is gzip, then S3keyReader should be called
    EXPECT_CALL(mockS3Interface, checkCompressionType(_)).WillOnce(Return(S3_COMPRESSION_PLAIN));
    S3Params params("s3://abc/def");
    params.setNumOfChunks(1);
    params.setChunkSize(1024 * 1024 * 2);
    this->open(params);

    ASSERT_EQ(this->upstreamReader, &this->keyReader);
    ASSERT_TRUE(NULL != dynamic_cast<S3KeyReader *>(this->upstreamReader));
}

TEST_F(S3CommonReaderTest, ReadGZip) {
    Byte compressionBuff[0x100];
    uLong compressedLen = sizeof(compressionBuff);
    const char hello[] = "The quick brown fox jumps over the lazy dog";

    compress(compressionBuff, &compressedLen, (const Bytef *)hello, sizeof(hello));

    mockS3Interface.setData(compressionBuff, compressedLen);

    EXPECT_CALL(mockS3Interface, checkCompressionType(_)).WillOnce(Return(S3_COMPRESSION_GZIP));

    EXPECT_CALL(mockS3Interface, fetchData(_, _, _, _))
        .WillOnce(Invoke(&mockS3Interface, &MockS3InterfaceForCompressionRead::mockFetchData));

    char result[0x100];
    S3Params params("s3://abc/def");
    params.setNumOfChunks(1);
    params.setChunkSize(1024 * 1024 * 2);
    params.setKeySize(compressedLen);
    this->open(params);

    EXPECT_EQ(sizeof(hello), this->upstreamReader->read(result, sizeof(result)));
    EXPECT_EQ((uint64_t)0, this->upstreamReader->read(result, sizeof(result)));
    EXPECT_EQ(0, memcmp(result, hello, sizeof(hello)));
}
