#include "s3common_writer.cpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_classes.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::Throw;

class MockS3InterfaceForCompressionWrite : public MockS3Interface {
   public:
    MockS3InterfaceForCompressionWrite() {
        pthread_mutex_init(&this->lock, NULL);
    };

    ~MockS3InterfaceForCompressionWrite() {
        pthread_mutex_destroy(&this->lock);
    }

    const uint8_t *getRawData() const {
        return this->data.data();
    }

    size_t getDataSize() const {
        return this->data.size();
    }

    size_t getPartNumber() const {
        return this->dataMap.size();
    }

    string mockGetUploadId(const S3Url &s3Url) {
        return this->uploadID;
    }

    string mockUploadPartOfData(S3VectorUInt8 &data, const S3Url &s3Url, uint64_t partNumber,
                                const string &uploadId) {
        UniqueLock uniqueLock(&this->lock);
        this->dataMap[partNumber] = data;
        return this->uploadID;
    }

    bool mockCompleteMultiPart(const S3Url &s3Url, const string &uploadId,
                               const vector<string> &etagArray) {
        map<uint64_t, S3VectorUInt8>::iterator it;
        for (it = this->dataMap.begin(); it != this->dataMap.end(); it++) {
            this->data.insert(this->data.end(), it->second.begin(), it->second.end());
        }
        return true;
    }

   private:
    pthread_mutex_t lock;
    vector<uint8_t> data;
    map<uint64_t, S3VectorUInt8> dataMap;
    const string uploadID = "I_am_an_uploadID";
};

class S3CommonWriteTest : public ::testing::Test, public S3CommonWriter {
   protected:
    // Remember that SetUp() is run immediately before a test starts.
    virtual void SetUp() {
        this->setS3InterfaceService(&mockS3Interface);

        this->out = new Byte[S3_ZIP_DECOMPRESS_CHUNKSIZE];
    }

    // TearDown() is invoked immediately after a test finishes.
    virtual void TearDown() {
        this->close();

        delete this->out;
    }

    void simpleUncompress(const char *input, uint64_t len) {
        z_stream zstream;

        // allocate inflate state for zlib
        zstream.zalloc = Z_NULL;
        zstream.zfree = Z_NULL;
        zstream.opaque = Z_NULL;

        int ret = inflateInit2(&zstream, S3_INFLATE_WINDOWSBITS);
        S3_CHECK_OR_DIE(ret == Z_OK, S3RuntimeError, "failed to initialize zlib library");

        zstream.avail_in = len;
        zstream.next_in = (Byte *)input;
        zstream.next_out = this->out;
        zstream.avail_out = S3_ZIP_DECOMPRESS_CHUNKSIZE;

        ret = inflate(&zstream, Z_FULL_FLUSH);

        if (ret != Z_STREAM_END) {
            S3DEBUG("Failed to uncompress sample data");
        }

        inflateEnd(&zstream);
    }

    MockS3InterfaceForCompressionWrite mockS3Interface;
    Byte *out;
};

// We need to mock uploadPartOfData() and completeMultiPart() in GZip mode,
// because even an empty string will produce 20+ bytes for GZip header and trailer,
// and then in open() may flush those data into S3KeyWriter.
TEST_F(S3CommonWriteTest, UsingGZip) {
    EXPECT_CALL(mockS3Interface, getUploadId(_))
        .WillOnce(Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockGetUploadId));
    EXPECT_CALL(mockS3Interface, uploadPartOfData(_, _, _, _))
        .WillOnce(
            Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockUploadPartOfData));
    EXPECT_CALL(mockS3Interface, completeMultiPart(_, _, _))
        .WillOnce(
            Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockCompleteMultiPart));

    S3Params params("s3://abc/def");
    params.setAutoCompress(true);
    params.setNumOfChunks(1);
    params.setChunkSize(S3_ZIP_COMPRESS_CHUNKSIZE + 1);

    this->open(params);

    ASSERT_EQ(this->upstreamWriter, &this->compressWriter);
    ASSERT_TRUE(NULL != dynamic_cast<CompressWriter *>(this->upstreamWriter));
}

// We need not to mock uploadPartOfData() and completeMultiPart() in plain mode,
TEST_F(S3CommonWriteTest, UsingPlain) {
    EXPECT_CALL(mockS3Interface, getUploadId(_))
        .WillOnce(Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockGetUploadId));

    S3Params params("s3://abc/def");
    params.setAutoCompress(false);
    params.setNumOfChunks(1);
    params.setChunkSize(S3_ZIP_COMPRESS_CHUNKSIZE + 1);

    this->open(params);

    ASSERT_EQ(this->upstreamWriter, &this->keyWriter);
    ASSERT_TRUE(NULL != dynamic_cast<S3KeyWriter *>(this->upstreamWriter));
}

TEST_F(S3CommonWriteTest, WritePlainData) {
    EXPECT_CALL(mockS3Interface, getUploadId(_))
        .WillOnce(Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockGetUploadId));
    EXPECT_CALL(mockS3Interface, uploadPartOfData(_, _, _, _))
        .WillOnce(
            Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockUploadPartOfData));
    EXPECT_CALL(mockS3Interface, completeMultiPart(_, _, _))
        .WillOnce(
            Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockCompleteMultiPart));

    S3Params params("s3://abc/def");
    params.setAutoCompress(false);
    params.setNumOfChunks(1);
    params.setChunkSize(S3_ZIP_COMPRESS_CHUNKSIZE + 1);

    this->open(params);

    // 44 bytes
    const char input[] = "The quick brown fox jumps over the lazy dog";
    this->write(input, sizeof(input));
    this->close();

    EXPECT_STREQ(input, (const char *)this->mockS3Interface.getRawData());
}

TEST_F(S3CommonWriteTest, WriteGZipData) {
    EXPECT_CALL(mockS3Interface, getUploadId(_))
        .WillOnce(Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockGetUploadId));
    EXPECT_CALL(mockS3Interface, uploadPartOfData(_, _, _, _))
        .WillOnce(
            Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockUploadPartOfData));
    EXPECT_CALL(mockS3Interface, completeMultiPart(_, _, _))
        .WillOnce(
            Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockCompleteMultiPart));

    S3Params params("s3://abc/def");
    params.setAutoCompress(true);
    params.setNumOfChunks(1);
    params.setChunkSize(S3_ZIP_COMPRESS_CHUNKSIZE + 1);

    this->open(params);

    // 44 bytes
    const char input[] = "The quick brown fox jumps over the lazy dog";
    this->write(input, sizeof(input));
    this->close();

    this->simpleUncompress((const char *)this->mockS3Interface.getRawData(),
                           this->mockS3Interface.getDataSize());
    EXPECT_STREQ(input, (const char *)this->out);
}

TEST_F(S3CommonWriteTest, WriteGZipDataMultipleTimes) {
    EXPECT_CALL(mockS3Interface, getUploadId(_))
        .WillRepeatedly(
            Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockGetUploadId));
    EXPECT_CALL(mockS3Interface, uploadPartOfData(_, _, _, _))
        .WillRepeatedly(
            Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockUploadPartOfData));
    EXPECT_CALL(mockS3Interface, completeMultiPart(_, _, _))
        .WillRepeatedly(
            Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockCompleteMultiPart));

    S3Params params("s3://abc/def");
    params.setAutoCompress(true);
    params.setNumOfChunks(1);
    params.setChunkSize(S3_ZIP_COMPRESS_CHUNKSIZE + 1);

    this->open(params);

    // 44 bytes * 10
    const char input[] = "The quick brown fox jumps over the lazy dog";
    int times = 10;
    for (int i = 0; i < times; i++) {
        this->write(input, sizeof(input));
    }
    this->close();

    this->simpleUncompress((const char *)this->mockS3Interface.getRawData(),
                           this->mockS3Interface.getDataSize());

    for (int i = 0; i < times; i++) {
        ASSERT_TRUE(memcmp(input, (const char *)this->out + i * sizeof(input), sizeof(input)) == 0);
    }
}
