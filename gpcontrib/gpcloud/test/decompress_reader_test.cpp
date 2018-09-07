#include "decompress_reader.cpp"
#include "gtest/gtest.h"

class MockBufferReader : public Reader {
   public:
    MockBufferReader() {
        this->offset = 0;
        this->chunkSize = 0;
    }

    void open(const S3Params &params) {
    }
    void close() {
    }

    void setData(const void *input, uint64_t size) {
        const char *p = static_cast<const char *>(input);

        this->clear();
        this->data.insert(this->data.end(), p, p + size);
    }

    uint64_t read(char *buf, uint64_t count) {
        uint64_t remaining = this->data.size() - offset;
        if (remaining <= 0) {
            return 0;
        }

        uint64_t size = (remaining > count) ? count : remaining;
        size = size < this->chunkSize ? size : this->chunkSize;
        for (uint64_t i = 0; i < size; i++) {
            buf[i] = this->data[offset + i];
        }

        this->offset += size;
        return size;
    }

    void clear() {
        this->data.clear();
        this->offset = 0;
    }

    void setChunkSize(uint64_t size) {
        this->chunkSize = size;
    }

   private:
    std::vector<uint8_t> data;
    uint64_t offset;
    uint64_t chunkSize;
};

class DecompressReaderTest : public testing::Test {
   protected:
    // Remember that SetUp() is run immediately before a test starts.
    virtual void SetUp() {
        // reset to default, because some tests will modify it
        S3_ZIP_DECOMPRESS_CHUNKSIZE = S3_ZIP_DEFAULT_CHUNKSIZE;

        // need to setup upstreamReader before open.
        this->bufReader.setChunkSize(1024 * 1024 * 64);
        decompressReader.setReader(&bufReader);
        decompressReader.open(S3Params("s3://abc/def"));
    }

    // TearDown() is invoked immediately after a test finishes.
    virtual void TearDown() {
        decompressReader.close();

        // reset to default, because some tests will modify it
        S3_ZIP_DECOMPRESS_CHUNKSIZE = S3_ZIP_DEFAULT_CHUNKSIZE;
    }

    void setBufReaderByRawData(const void *input, int len) {
        uLong compressedLen = sizeof(compressionBuff);

        int err = compress(compressionBuff, &compressedLen, (const Bytef *)input, len);
        if (err != Z_OK) {
            S3DEBUG("failed to compress sample data");
        }

        bufReader.setData(compressionBuff, compressedLen);
    }

    DecompressReader decompressReader;
    MockBufferReader bufReader;
    Byte compressionBuff[10000];
};

TEST_F(DecompressReaderTest, AbleToDecompressEmptyData) {
    unsigned char input[10] = {0};
    bufReader.setData(input, 0);

    char buf[10000];
    uint64_t count = decompressReader.read(buf, sizeof(buf));

    EXPECT_EQ((uint64_t)0, count);
}

TEST_F(DecompressReaderTest, AbleToDecompressSmallCompressedData) {
    // 1. compressed data to decompress
    const char hello[] = "The quick brown fox jumps over the lazy dog";
    setBufReaderByRawData(hello, sizeof(hello));

    // 2. call API
    char buf[10000];
    uint64_t count = decompressReader.read(buf, sizeof(buf));

    // 3. assertion
    EXPECT_EQ(sizeof(hello), count);
    EXPECT_EQ(0, strncmp(hello, buf, count));
}

TEST_F(DecompressReaderTest, AbleToDecompressFragmentalCompressedData) {
    // Fragmental data might not be decompressed, this case makes sure following data will be read
    // to compose a decompressable unit.

    // compressed data to decompress
    const char hello[] = "The quick brown fox jumps over the lazy dog";
    setBufReaderByRawData(hello, sizeof(hello));

    // set chunk size large enough to bypass gzip header.
    this->bufReader.setChunkSize(10);

    char buf[100];
    uint64_t offset = decompressReader.read(buf, sizeof(buf));

    // set chunk size to 1 byte, which could not be decompressed by itself.
    this->bufReader.setChunkSize(1);
    while (offset < sizeof(hello)) {
        uint64_t count = decompressReader.read(buf + offset, sizeof(buf));

        ASSERT_NE(count, (uint64_t)0);
        offset += count;
    }

    uint64_t count = decompressReader.read(buf + offset, sizeof(buf));

    EXPECT_EQ((uint64_t)0, count);
    EXPECT_EQ(0, strncmp(hello, buf, count));
}

TEST_F(DecompressReaderTest, AbleToDecompressWithSmallReadBuffer) {
    // Test case for: caller uses buffer smaller than internal chunk.
    //      total compressed data is small (12 bytes),
    //      chunk size is 32 bytes
    //      output buffer is smaller than chunk size (16 bytes).

    // resize to 32 for 'in' and 'out' buffer
    S3_ZIP_DECOMPRESS_CHUNKSIZE = 32;
    decompressReader.resizeDecompressReaderBuffer(S3_ZIP_DECOMPRESS_CHUNKSIZE);

    char hello[S3_ZIP_DECOMPRESS_CHUNKSIZE + 2];
    memset((void *)hello, 'A', sizeof(hello));
    hello[sizeof(hello) - 1] = '\0';

    setBufReaderByRawData(hello, sizeof(hello));

    char outputBuffer[16] = {0};

    uint32_t expectedLen[] = {16, 16, 2, 0};
    for (uint32_t i = 0; i < sizeof(expectedLen) / sizeof(uint32_t); i++) {
        uint64_t count = decompressReader.read(outputBuffer, sizeof(outputBuffer));
        ASSERT_EQ(expectedLen[i], count);
    }
}

TEST_F(DecompressReaderTest, AbleToDecompressWithSmallInternalReaderBuffer) {
    // Test case for: internal read buffer (this->in) smaller than compressed data.
    //      total compressed data is small (12 bytes),
    //      chunk size is 10 bytes
    //      output buffer is smaller than chunk size (9 bytes).

    // resize to 32 for 'in' and 'out' buffer
    S3_ZIP_DECOMPRESS_CHUNKSIZE = 10;
    decompressReader.resizeDecompressReaderBuffer(S3_ZIP_DECOMPRESS_CHUNKSIZE);

    char hello[34];  // compress 34 'A' will produce 12 compressed bytes.
    memset((void *)hello, 'A', sizeof(hello));
    hello[sizeof(hello) - 1] = '\0';

    setBufReaderByRawData(hello, sizeof(hello));

    char outputBuffer[9] = {0};

    uint32_t expectedLen[] = {9, 1, 9, 1, 9, 1, 4, 0};
    for (uint32_t i = 0; i < sizeof(expectedLen) / sizeof(uint32_t); i++) {
        ASSERT_EQ(expectedLen[i], decompressReader.read(outputBuffer, sizeof(outputBuffer)));
    }
}

TEST_F(DecompressReaderTest, ReadFromOffsetForEachCall) {
    S3_ZIP_DECOMPRESS_CHUNKSIZE = 128;
    decompressReader.resizeDecompressReaderBuffer(S3_ZIP_DECOMPRESS_CHUNKSIZE);

    // Bigger chunk size, smaller read buffer from caller. Need read from offset for each call.
    char hello[] = "abcdefghigklmnopqrstuvwxyz";
    setBufReaderByRawData(hello, sizeof(hello));

    char outputBuffer[4];

    uint64_t count = decompressReader.read(outputBuffer, sizeof(outputBuffer));
    EXPECT_EQ((uint64_t)4, count);

    // read 2nd 16 bytes
    count = decompressReader.read(outputBuffer, sizeof(outputBuffer));
    EXPECT_EQ((uint64_t)4, count);
    EXPECT_TRUE(strncmp("efgh", outputBuffer, count) == 0);
}

TEST_F(DecompressReaderTest, AbleToDecompressWithAlignedLargeReadBuffer) {
    // Test case for: output buffer read(size) >= chunksize.
    //      total compressed data is small (12 bytes),
    //      chunk size is 8 bytes
    //      output buffer is bigger than chunk size (16 bytes).

    // resize to 8 for 'in' and 'out' buffer

    S3_ZIP_DECOMPRESS_CHUNKSIZE = 8;
    decompressReader.resizeDecompressReaderBuffer(S3_ZIP_DECOMPRESS_CHUNKSIZE);

    char hello[S3_ZIP_DECOMPRESS_CHUNKSIZE * 2 + 2];
    memset((void *)hello, 'A', sizeof(hello));
    hello[sizeof(hello) - 1] = '\0';

    setBufReaderByRawData(hello, sizeof(hello));

    char outputBuffer[16] = {0};

    uint32_t expectedLen[] = {8, 8, 2, 0};
    for (uint32_t i = 0; i < sizeof(expectedLen) / sizeof(uint32_t); i++) {
        ASSERT_EQ(expectedLen[i], decompressReader.read(outputBuffer, sizeof(outputBuffer)));
    }
}

TEST_F(DecompressReaderTest, AbleToDecompressWithUnalignedLargeReadBuffer) {
    // Test case for: optimal buffer size fill after decompression
    // We need to make sure that we are filling the decompression
    // buffer fully before asking for a new chunck from the read buffer
    S3_ZIP_DECOMPRESS_CHUNKSIZE = 8;
    decompressReader.resizeDecompressReaderBuffer(S3_ZIP_DECOMPRESS_CHUNKSIZE);

    char hello[S3_ZIP_DECOMPRESS_CHUNKSIZE * 6 + 2];
    memset((void *)hello, 'A', sizeof(hello));
    hello[sizeof(hello) - 1] = '\0';

    setBufReaderByRawData(hello, sizeof(hello));

    char outputBuffer[S3_ZIP_DECOMPRESS_CHUNKSIZE * 6 + 4];

    uint32_t expectedLen[] = {8, 8, 8, 8, 8, 8, 2, 0};
    for (uint32_t i = 0; i < sizeof(expectedLen) / sizeof(uint32_t); i++) {
        ASSERT_EQ(expectedLen[i], decompressReader.read(outputBuffer, sizeof(outputBuffer)));
    }
}

TEST_F(DecompressReaderTest, AbleToDecompressWithLargeReadBufferWithDecompressableString) {
    S3_ZIP_DECOMPRESS_CHUNKSIZE = 8;
    decompressReader.resizeDecompressReaderBuffer(S3_ZIP_DECOMPRESS_CHUNKSIZE);

    // Smaller chunk size, bigger read buffer from caller. Need composite multiple chunks.
    char hello[] = "abcdefghigklmnopqrstuvwxyz";  // 26+1 bytes
    setBufReaderByRawData(hello, sizeof(hello));

    char outputBuffer[30] = {0};
    uint64_t outOffset = 0;

    // read and compose all chunks.
    while (outOffset < sizeof(hello)) {
        uint64_t count =
            decompressReader.read(outputBuffer + outOffset, sizeof(outputBuffer) - outOffset);
        outOffset += count;
    }

    EXPECT_TRUE(strncmp(hello, outputBuffer, sizeof(hello)) == 0);
}

TEST_F(DecompressReaderTest, AbleToDecompressWithSmartLargeReadBufferWithDecompressableString) {
    S3_ZIP_DECOMPRESS_CHUNKSIZE = 7;
    decompressReader.resizeDecompressReaderBuffer(S3_ZIP_DECOMPRESS_CHUNKSIZE);

    // Smaller chunk size, bigger read buffer from caller. Need composite multiple chunks.
    char hello[] = "abcdefghigklmnopqrstuvwxyz";  // 26+1 bytes
    setBufReaderByRawData(hello, sizeof(hello));

    // for chunksize = 7, upper string will be decompressed in four steps: 4, 7, 7, 7, 2

    char outputBuffer[9] = {0};

    uint32_t expectedLen[] = {4, 7, 7, 7, 2};
    uint64_t offset = 0;
    for (uint32_t i = 0; i < sizeof(expectedLen) / sizeof(uint32_t); i++) {
        uint64_t count = decompressReader.read(outputBuffer, sizeof(outputBuffer));
        EXPECT_TRUE(strncmp(hello + offset, outputBuffer, count) == 0);
        ASSERT_EQ(expectedLen[i], count);
        offset += count;
    }
}

TEST_F(DecompressReaderTest, AbleToDecompressWithIncorrectEncodedStream) {
    S3_ZIP_DECOMPRESS_CHUNKSIZE = 128;
    decompressReader.resizeDecompressReaderBuffer(S3_ZIP_DECOMPRESS_CHUNKSIZE);

    // set an incorrect encoding stream to Mock directly.
    // it will produce 'Z_DATA_ERROR' when decompressing
    char hello[] = "abcdefghigklmnopqrstuvwxyz";  // 26+1 bytes
    this->bufReader.setData(hello, sizeof(hello));

    char outputBuffer[128] = {0};

    EXPECT_THROW(decompressReader.read(outputBuffer, sizeof(outputBuffer)), S3RuntimeError);
}
