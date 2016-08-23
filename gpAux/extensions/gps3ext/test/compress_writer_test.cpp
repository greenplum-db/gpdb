#include <vector>

#include "compress_writer.cpp"
#include "gtest/gtest.h"
#include "s3macros.h"

using std::vector;

class MockWriter : public Writer {
   public:
    MockWriter() {
    }
    virtual ~MockWriter() {
    }

    virtual void open(const WriterParams &params) {
    }

    virtual uint64_t write(const char *buf, uint64_t count) {
        this->data.insert(this->data.end(), buf, buf + count);
        return count;
    }

    virtual void close() {
        // Do not clean it, so we can verify the result in UT tests.
        // this->data.clear();
    }

    const char *getRawData() const {
        return this->data.data();
    }

    vector<char> &getRawDataVector() {
        return this->data;
    }

    size_t getDataSize() const {
        return this->data.size();
    }

   private:
    vector<char> data;
};

class CompressWriterTest : public testing::Test {
   protected:
    // Remember that SetUp() is run immediately before a test starts.
    virtual void SetUp() {
        compressWriter.setWriter(&writer);
        compressWriter.open(params);

        this->out = new Byte[S3_ZIP_DECOMPRESS_CHUNKSIZE];
    }

    // TearDown() is invoked immediately after a test finishes.
    virtual void TearDown() {
        compressWriter.close();

        delete this->out;
    }

    void simpleUncompress(const char *input, uint64_t len) {
        z_stream zstream;

        // allocate inflate state for zlib
        zstream.zalloc = Z_NULL;
        zstream.zfree = Z_NULL;
        zstream.opaque = Z_NULL;

        int ret = inflateInit2(&zstream, S3_INFLATE_WINDOWSBITS);
        CHECK_OR_DIE_MSG(ret == Z_OK, "%s", "failed to initialize zlib library");

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

    WriterParams params;
    CompressWriter compressWriter;
    MockWriter writer;

    Byte *out;
};

TEST_F(CompressWriterTest, AbleToInputNull) {
    compressWriter.write(NULL, 0);
    EXPECT_EQ(0, writer.getDataSize());
}

TEST_F(CompressWriterTest, AbleToCompressEmptyData) {
    char input[10] = {0};
    compressWriter.write(input, sizeof(input));
    compressWriter.close();

    this->simpleUncompress(writer.getRawData(), writer.getDataSize());
    EXPECT_STREQ(input, (const char *)this->out);
}

TEST_F(CompressWriterTest, AbleToCompressAndCheckGZipHeader) {
    char input[10] = {0};
    compressWriter.write(input, sizeof(input));
    compressWriter.close();

    const char *header = writer.getRawData();
    ASSERT_TRUE(header[0] == char(0x1f));
    ASSERT_TRUE(header[1] == char(0x8b));
}

TEST_F(CompressWriterTest, CloseMultipleTimes) {
    char input[10] = {0};
    compressWriter.write(input, sizeof(input));

    compressWriter.close();
    compressWriter.close();
    compressWriter.close();
    compressWriter.close();

    this->simpleUncompress(writer.getRawData(), writer.getDataSize());
    EXPECT_STREQ(input, (const char *)this->out);
}

TEST_F(CompressWriterTest, AbleToCompressOneSmallString) {
    const char input[] = "The quick brown fox jumps over the lazy dog";

    compressWriter.write(input, sizeof(input));
    compressWriter.close();

    this->simpleUncompress(writer.getRawData(), writer.getDataSize());
    EXPECT_STREQ(input, (const char *)this->out);
}

TEST_F(CompressWriterTest, AbleToWriteServeralTimesBeforeClose) {
    unsigned int i, times = 100;

    const char input[] = "The quick brown fox jumps over the lazy dog";

    for (i = 0; i < times; i++) {
        compressWriter.write(input, sizeof(input));
    }

    compressWriter.close();

    this->simpleUncompress(writer.getRawData(), writer.getDataSize());

    for (i = 0; i < times; i++) {
        ASSERT_TRUE(memcmp(input, (const char *)this->out + i * sizeof(input), sizeof(input)) == 0);
    }
}

TEST_F(CompressWriterTest, AbleToWriteLargerThanCompressChunkSize) {
    char *input = new char[S3_ZIP_COMPRESS_CHUNKSIZE + 1];

    EXPECT_THROW(compressWriter.write(input, S3_ZIP_COMPRESS_CHUNKSIZE + 1), std::runtime_error);
}
