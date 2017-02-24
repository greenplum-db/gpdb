#include "compress_writer.cpp"
#include <memory>
#include <random>
#include "gtest/gtest.h"

class MockWriter : public Writer {
   public:
    MockWriter() {
    }
    virtual ~MockWriter() {
    }

    virtual void open(const S3Params &params) {
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
        compressWriter.open(S3Params("s3://abc/def/"));

        this->out = new Byte[S3_ZIP_DECOMPRESS_CHUNKSIZE];
    }

    // TearDown() is invoked immediately after a test finishes.
    virtual void TearDown() {
        compressWriter.close();

        delete this->out;
    }

    void simpleUncompress(const char *input, uint64_t len) {
        this->coreUncompress((Byte *)input, len, this->out, S3_ZIP_COMPRESS_CHUNKSIZE);
    }

    void coreUncompress(Byte *input, uint64_t len, Byte *output, uint64_t out_len) {
        z_stream zstream;

        // allocate inflate state for zlib
        zstream.zalloc = Z_NULL;
        zstream.zfree = Z_NULL;
        zstream.opaque = Z_NULL;

        int ret = inflateInit2(&zstream, S3_INFLATE_WINDOWSBITS);
        S3_CHECK_OR_DIE(ret == Z_OK, S3RuntimeError, "failed to initialize zlib library");

        zstream.avail_in = len;
        zstream.next_in = input;
        zstream.next_out = output;
        zstream.avail_out = out_len;

        ret = inflate(&zstream, Z_FULL_FLUSH);

        if (ret != Z_STREAM_END) {
            S3DEBUG("Failed to uncompress sample data");
        }

        inflateEnd(&zstream);
    }

    CompressWriter compressWriter;
    MockWriter writer;

    Byte *out;
};

TEST_F(CompressWriterTest, AbleToInputNull) {
    compressWriter.write(NULL, 0);
    EXPECT_EQ((uint64_t)0, writer.getDataSize());
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

    // 44 chars
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
    const char pangram[] = "The quick brown fox jumps over the lazy dog";
    uint64_t times = S3_ZIP_COMPRESS_CHUNKSIZE / (sizeof(pangram) - 1) + 1;

    string input;
    for (uint64_t i = 0; i < times; i++) input.append(pangram);

    compressWriter.write(input.c_str(), input.length());
    compressWriter.close();

    Byte *result = new Byte[input.length() + 1];
    this->coreUncompress((Byte *)writer.getRawData(), writer.getDataSize(), result,
                         input.length() + 1);

    EXPECT_TRUE(memcmp(input.c_str(), result, input.length()) == 0);

    delete[] result;
}

// Compress compressed data may generate larger output than input after GZIP compression.
TEST_F(CompressWriterTest, CompressCompressedData) {
    std::random_device rd;
    std::default_random_engine re(rd());

    // 25 is an empirical number to trigger the corner case.
    size_t dataLen = S3_ZIP_COMPRESS_CHUNKSIZE * 25;
    size_t charLen = dataLen * 4;
    size_t *data = new size_t[dataLen];

    for (size_t i = 0; i < dataLen; i += 4) {
        data[i] = re();
    }

    compressWriter.write((const char *)data, charLen);
    compressWriter.close();

    vector<char> compressedData;
    for (size_t i = 0; i < 3; i++) {
        compressWriter.open(S3Params("s3://abc/def"));

        compressedData.swap(writer.getRawDataVector());
        writer.getRawDataVector().clear();

        compressWriter.write((const char *)compressedData.data(), compressedData.size());
        compressWriter.close();
    }

    std::unique_ptr<Byte> result(new Byte[compressedData.size()]);
    this->coreUncompress((Byte *)writer.getRawData(), writer.getDataSize(), result.get(),
                         compressedData.size());

    EXPECT_TRUE(memcmp(compressedData.data(), result.get(), compressedData.size()) == 0);
}
