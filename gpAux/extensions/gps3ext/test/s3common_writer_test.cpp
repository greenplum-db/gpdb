#include "s3common_writer.cpp"
#include "gtest/gtest.h"
#include "mock_classes.h"
#include "s3macros.h"

#include <map>
#include <vector>
using std::vector;

using ::testing::AtLeast;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::Throw;
using ::testing::_;

class MockS3InterfaceForCompressionWrite : public MockS3Interface {
   public:
    MockS3InterfaceForCompressionWrite(){};

    const uint8_t* getRawData() const {
        return this->data.data();
    }

    size_t getDataSize() const {
        return this->data.size();
    }

    size_t getPartNumber() const {
        return this->dataMap.size();
    }

    bool mockCheckKeyExistence(const string& keyUrl, const string& region,
                               const S3Credential& cred) {
        return false;
    }

    string mockGetUploadId(const string& keyUrl, const string& region, const S3Credential& cred) {
        return this->uploadID;
    }

    string mockUploadPartOfData(vector<uint8_t>& data, const string& keyUrl, const string& region,
                                const S3Credential& cred, uint64_t partNumber,
                                const string& uploadId) {
        this->dataMap[partNumber] = data;
        return this->uploadID;
    }

    bool mockCompleteMultiPart(const string& keyUrl, const string& region, const S3Credential& cred,
                               const string& uploadId, const vector<string>& etagArray) {
        map<uint64_t, vector<uint8_t>>::iterator it;
        for (it = this->dataMap.begin(); it != this->dataMap.end(); it++) {
            this->data.insert(this->data.end(), it->second.begin(), it->second.end());
        }
        return true;
    }

   private:
    vector<uint8_t> data;
    map<uint64_t, vector<uint8_t>> dataMap;
    const string uploadID = "I_am_an_uploadID";
};

class S3CommonWriteTest : public ::testing::Test, public S3CommonWriter {
   protected:
    // Remember that SetUp() is run immediately before a test starts.
    virtual void SetUp() {
        this->setS3service(&mockS3Interface);

        this->out = new Byte[S3_ZIP_DECOMPRESS_CHUNKSIZE];
    }

    // TearDown() is invoked immediately after a test finishes.
    virtual void TearDown() {
        this->close();

        delete this->out;
    }

    void simpleUncompress(const char* input, uint64_t len) {
        z_stream zstream;

        // allocate inflate state for zlib
        zstream.zalloc = Z_NULL;
        zstream.zfree = Z_NULL;
        zstream.opaque = Z_NULL;

        int ret = inflateInit2(&zstream, S3_INFLATE_WINDOWSBITS);
        CHECK_OR_DIE_MSG(ret == Z_OK, "%s", "failed to initialize zlib library");

        zstream.avail_in = len;
        zstream.next_in = (Byte*)input;
        zstream.next_out = this->out;
        zstream.avail_out = S3_ZIP_DECOMPRESS_CHUNKSIZE;

        ret = inflate(&zstream, Z_FULL_FLUSH);

        if (ret != Z_STREAM_END) {
            S3DEBUG("Failed to uncompress sample data");
        }

        inflateEnd(&zstream);
    }

    MockS3InterfaceForCompressionWrite mockS3Interface;
    Byte* out;
};

TEST_F(S3CommonWriteTest, UsingGZip) {
    s3ext_autocompress = true;

    EXPECT_CALL(mockS3Interface, getUploadId(_, _, _))
        .WillOnce(Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockGetUploadId));
    EXPECT_CALL(mockS3Interface, uploadPartOfData(_, _, _, _, _, _))
        .WillOnce(
            Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockUploadPartOfData));
    EXPECT_CALL(mockS3Interface, completeMultiPart(_, _, _, _, _))
        .WillOnce(
            Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockCompleteMultiPart));

    WriterParams params;
    params.setNumOfChunks(1);
    params.setChunkSize(S3_ZIP_COMPRESS_CHUNKSIZE + 1);

    this->open(params);

    ASSERT_EQ(this->upstreamWriter, &this->compressWriter);
    ASSERT_TRUE(NULL != dynamic_cast<CompressWriter*>(this->upstreamWriter));
}

TEST_F(S3CommonWriteTest, UsingPlain) {
    s3ext_autocompress = false;

    EXPECT_CALL(mockS3Interface, getUploadId(_, _, _))
        .WillOnce(Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockGetUploadId));

    WriterParams params;
    params.setNumOfChunks(1);
    params.setChunkSize(S3_ZIP_COMPRESS_CHUNKSIZE + 1);

    this->open(params);

    ASSERT_EQ(this->upstreamWriter, &this->keyWriter);
    ASSERT_TRUE(NULL != dynamic_cast<S3KeyWriter*>(this->upstreamWriter));
}

TEST_F(S3CommonWriteTest, WritePlainData) {
    s3ext_autocompress = false;

    //    EXPECT_CALL(mockS3Interface, checkKeyExistence(_, _, _))
    //        .WillOnce(
    //            Invoke(&mockS3Interface,
    //            &MockS3InterfaceForCompressionWrite::mockCheckKeyExistence));
    EXPECT_CALL(mockS3Interface, getUploadId(_, _, _))
        .WillOnce(Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockGetUploadId));
    EXPECT_CALL(mockS3Interface, uploadPartOfData(_, _, _, _, _, _))
        .WillOnce(
            Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockUploadPartOfData));
    EXPECT_CALL(mockS3Interface, completeMultiPart(_, _, _, _, _))
        .WillOnce(
            Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockCompleteMultiPart));

    WriterParams params;
    params.setNumOfChunks(1);
    params.setChunkSize(S3_ZIP_COMPRESS_CHUNKSIZE + 1);

    this->open(params);

    // 44 bytes
    const char input[] = "The quick brown fox jumps over the lazy dog";
    this->write(input, sizeof(input));
    this->close();

    EXPECT_STREQ(input, (const char*)this->mockS3Interface.getRawData());
}

TEST_F(S3CommonWriteTest, WriteGZipData) {
    s3ext_autocompress = true;

    EXPECT_CALL(mockS3Interface, getUploadId(_, _, _))
        .WillOnce(Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockGetUploadId));
    EXPECT_CALL(mockS3Interface, uploadPartOfData(_, _, _, _, _, _))
        .WillOnce(
            Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockUploadPartOfData));
    EXPECT_CALL(mockS3Interface, completeMultiPart(_, _, _, _, _))
        .WillOnce(
            Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockCompleteMultiPart));

    WriterParams params;
    params.setNumOfChunks(1);
    params.setChunkSize(S3_ZIP_COMPRESS_CHUNKSIZE + 1);

    this->open(params);

    // 44 bytes
    const char input[] = "The quick brown fox jumps over the lazy dog";
    this->write(input, sizeof(input));
    this->close();

    this->simpleUncompress((const char*)this->mockS3Interface.getRawData(),
                           this->mockS3Interface.getDataSize());
    EXPECT_STREQ(input, (const char*)this->out);
}

TEST_F(S3CommonWriteTest, WriteGZipDataMultipleTimes) {
    s3ext_autocompress = true;

    EXPECT_CALL(mockS3Interface, getUploadId(_, _, _))
        .WillRepeatedly(
            Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockGetUploadId));
    EXPECT_CALL(mockS3Interface, uploadPartOfData(_, _, _, _, _, _))
        .WillRepeatedly(
            Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockUploadPartOfData));
    EXPECT_CALL(mockS3Interface, completeMultiPart(_, _, _, _, _))
        .WillRepeatedly(
            Invoke(&mockS3Interface, &MockS3InterfaceForCompressionWrite::mockCompleteMultiPart));

    WriterParams params;
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

    this->simpleUncompress((const char*)this->mockS3Interface.getRawData(),
                           this->mockS3Interface.getDataSize());

    for (int i = 0; i < times; i++) {
        ASSERT_TRUE(memcmp(input, (const char*)this->out + i * sizeof(input), sizeof(input)) == 0);
    }
}