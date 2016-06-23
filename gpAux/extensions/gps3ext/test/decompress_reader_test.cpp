#include <vector>

#include "decompress_reader.cpp"
#include "gtest/gtest.h"

using std::vector;

class MockBufferReader : public Reader {
   public:
    MockBufferReader() {
        this->offset = 0;
    }

    void open(const ReaderParams &params) {
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

        for (int i = 0; i < size; i++) {
            buf[i] = this->data[offset + i];
        }

        this->offset += size;
        return size;
    }

    void clear() {
        this->data.clear();
        this->offset = 0;
    }

   private:
    std::vector<uint8_t> data;
    uint64_t offset;
};

class DecompressReaderTest : public testing::Test {
   protected:
    // Remember that SetUp() is run immediately before a test starts.
    virtual void SetUp() {
        // need to setup upstreamReader before open.
        decompressReader.setReader(&bufReader);
        decompressReader.open(params);
    }

    // TearDown() is invoked immediately after a test finishes.
    virtual void TearDown() {
        decompressReader.close();
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
    ReaderParams params;
    MockBufferReader bufReader;
    Byte compressionBuff[10000];
};

TEST_F(DecompressReaderTest, AbleToDecompressEmptyData) {
    unsigned char input[10] = {0};
    MockBufferReader bufReader;
    bufReader.setData(input, 0);
    decompressReader.setReader(&bufReader);

    char buf[10000];
    uint64_t count = decompressReader.read(buf, sizeof(buf));

    EXPECT_EQ(0, count);
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

TEST_F(DecompressReaderTest, AbleToDecompressWithSmallReadBuffer) {
    // Test case for: caller uses buffer smaller than internal chunk.
    //      total compressed data is small (12 bytes),
    //      chunk size is 32 bytes
    //      output buffer is smaller than chunk size (16 bytes).

    // resize to 32 for 'in' and 'out' buffer
    S3_ZIP_CHUNKSIZE = 32;
    decompressReader.resizeDecompressReaderBuffer(S3_ZIP_CHUNKSIZE);

    char hello[S3_ZIP_CHUNKSIZE + 2];
    memset((void *)hello, 'A', sizeof(hello));
    hello[sizeof(hello) - 1] = '\0';

    setBufReaderByRawData(hello, sizeof(hello));

    char outputBuffer[16] = {0};

    int expectedLen[] = {16, 16, 2, 0};
    for (int i = 0; i < sizeof(expectedLen) / sizeof(int); i++) {
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
    S3_ZIP_CHUNKSIZE = 10;
    decompressReader.resizeDecompressReaderBuffer(S3_ZIP_CHUNKSIZE);

    char hello[34];  // compress 34 'A' will produce 12 compressed bytes.
    memset((void *)hello, 'A', sizeof(hello));
    hello[sizeof(hello) - 1] = '\0';

    setBufReaderByRawData(hello, sizeof(hello));

    char outputBuffer[9] = {0};

    int expectedLen[] = {9, 1, 9, 1, 9, 1, 4, 0};
    for (int i = 0; i < sizeof(expectedLen) / sizeof(int); i++) {
        uint64_t count = decompressReader.read(outputBuffer, sizeof(outputBuffer));
        ASSERT_EQ(expectedLen[i], count);
    }
}

TEST_F(DecompressReaderTest, ReadFromOffsetForEachCall) {
    S3_ZIP_CHUNKSIZE = 128;
    decompressReader.resizeDecompressReaderBuffer(S3_ZIP_CHUNKSIZE);

    // Bigger chunk size, smaller read buffer from caller. Need read from offset for each call.
    char hello[] = "abcdefghigklmnopqrstuvwxyz";
    setBufReaderByRawData(hello, sizeof(hello));

    char outputBuffer[4];

    uint64_t count = decompressReader.read(outputBuffer, sizeof(outputBuffer));
    EXPECT_EQ(4, count);

    // read 2nd 16 bytes
    count = decompressReader.read(outputBuffer, sizeof(outputBuffer));
    EXPECT_EQ(4, count);
    EXPECT_TRUE(strncmp("efgh", outputBuffer, count) == 0);
}

TEST_F(DecompressReaderTest, AbleToDecompressWithAlignedLargeReadBuffer) {
    // Test case for: output buffer read(size) >= chunksize.
    //      total compressed data is small (12 bytes),
    //      chunk size is 8 bytes
    //      output buffer is bigger than chunk size (16 bytes).

    // resize to 8 for 'in' and 'out' buffer

    S3_ZIP_CHUNKSIZE = 8;
    decompressReader.resizeDecompressReaderBuffer(S3_ZIP_CHUNKSIZE);

    char hello[S3_ZIP_CHUNKSIZE * 2 + 2];
    memset((void *)hello, 'A', sizeof(hello));
    hello[sizeof(hello) - 1] = '\0';

    setBufReaderByRawData(hello, sizeof(hello));

    char outputBuffer[16] = {0};

    // read 1st 16 bytes
    uint64_t count = decompressReader.read(outputBuffer, sizeof(outputBuffer));
    EXPECT_EQ(sizeof(outputBuffer), count);

    // read 2nd 2 bytes
    count = decompressReader.read(outputBuffer, sizeof(outputBuffer));
    EXPECT_EQ(2, count);

    // read 3rd 0 byte
    count = decompressReader.read(outputBuffer, sizeof(outputBuffer));
    EXPECT_EQ(0, count);
}

TEST_F(DecompressReaderTest, AbleToDecompressWithUnalignedLargeReadBuffer) {
    // Test case for: optimal buffer size fill after decompression
    // We need to make sure that we are filling the decompression
    // buffer fully before asking for a new chunck from the read buffer
    S3_ZIP_CHUNKSIZE = 8;
    decompressReader.resizeDecompressReaderBuffer(S3_ZIP_CHUNKSIZE);

    char hello[S3_ZIP_CHUNKSIZE * 6 + 2];
    memset((void *)hello, 'A', sizeof(hello));
    hello[sizeof(hello) - 1] = '\0';

    setBufReaderByRawData(hello, sizeof(hello));

    char outputBuffer[S3_ZIP_CHUNKSIZE * 6 + 4];

    // read once, decompress 6 times
    uint64_t count = decompressReader.read(outputBuffer, sizeof(outputBuffer));
    EXPECT_EQ(48, count);

    // read the last 2 bytes
    count = decompressReader.read(outputBuffer, sizeof(outputBuffer));
    EXPECT_EQ(2, count);
}

TEST_F(DecompressReaderTest, AbleToDecompressWithEnoughSizeReadBuffer) {
    // Test case for: optimal buffer size fill after decompression
    // We need to make sure that we are filling the decompression
    // buffer fully before asking for a new chunck from the read buffer
    S3_ZIP_CHUNKSIZE = 8;
    decompressReader.resizeDecompressReaderBuffer(S3_ZIP_CHUNKSIZE);

    char hello[S3_ZIP_CHUNKSIZE * 6 + 2];
    memset((void *)hello, 'A', sizeof(hello));
    hello[sizeof(hello) - 1] = '\0';

    setBufReaderByRawData(hello, sizeof(hello));

    char outputBuffer[S3_ZIP_CHUNKSIZE * 8];

    // read once, decompress 6 times
    uint64_t count = decompressReader.read(outputBuffer, sizeof(outputBuffer));
    EXPECT_EQ(sizeof(hello), count);
}

TEST_F(DecompressReaderTest, AbleToDecompressWithLargeReadBufferWithDecompressableString) {
    S3_ZIP_CHUNKSIZE = 8;
    decompressReader.resizeDecompressReaderBuffer(S3_ZIP_CHUNKSIZE);

    // Smaller chunk size, bigger read buffer from caller. Need composite multiple chunks.
    char hello[] = "abcdefghigklmnopqrstuvwxyz";  // 26+1 bytes
    setBufReaderByRawData(hello, sizeof(hello));

    // for chunksize = 8, upper string will be decompressed in four steps: 5, 8, 8, 6

    char outputBuffer[20] = {0};

    uint64_t count = decompressReader.read(outputBuffer, sizeof(outputBuffer));
    EXPECT_TRUE(strncmp("abcdefghigklmnopqrstuvwxyz", outputBuffer, 5 + 8) == 0);
    EXPECT_EQ(5 + 8, count);

    count = decompressReader.read(outputBuffer, sizeof(outputBuffer));
    EXPECT_EQ(8 + 6, count);
}

TEST_F(DecompressReaderTest, AbleToDecompressWithEnoughLargeReadBufferWithDecompressableString) {
    // Test case for: the input data is decompressable, hence the size of "compressed" data is
    // larger than original size of data.
    S3_ZIP_CHUNKSIZE = 8;
    decompressReader.resizeDecompressReaderBuffer(S3_ZIP_CHUNKSIZE);

    // Smaller chunk size, bigger read buffer from caller. Need composite multiple chunks.
    char hello[] = "abcdefghigklmnopqrstuvwxyz";  // 26+1 bytes
    setBufReaderByRawData(hello, sizeof(hello));

    // for chunksize = 8, upper string will be decompressed in four steps: 5, 8, 8, 6

    char outputBuffer[40] = {0};

    // read 26+1 bytes
    uint64_t count = decompressReader.read(outputBuffer, sizeof(outputBuffer));
    EXPECT_EQ(sizeof(hello), count);
    EXPECT_TRUE(strncmp("abcdefghigklmnopqrstuvwxyz", outputBuffer, count) == 0);
}

TEST_F(DecompressReaderTest, AbleToDecompressWithSmartLargeReadBufferWithDecompressableString) {
    S3_ZIP_CHUNKSIZE = 7;
    decompressReader.resizeDecompressReaderBuffer(S3_ZIP_CHUNKSIZE);

    // Smaller chunk size, bigger read buffer from caller. Need composite multiple chunks.
    char hello[] = "abcdefghigklmnopqrstuvwxyz";  // 26+1 bytes
    setBufReaderByRawData(hello, sizeof(hello));

    // for chunksize = 7, upper string will be decompressed in four steps: 4, 7, 7, 7, 2

    char outputBuffer[9] = {0};

    int expectedLen[] = {4, 7, 7, 7, 2};
    int offset = 0;
    for (int i = 0; i < sizeof(expectedLen) / sizeof(int); i++) {
        uint64_t count = decompressReader.read(outputBuffer, sizeof(outputBuffer));
        EXPECT_TRUE(strncmp(hello + offset, outputBuffer, count) == 0);
        ASSERT_EQ(expectedLen[i], count);
        offset += count;
    }
}

TEST_F(DecompressReaderTest, AbleToDecompressWithIncorrectEncodedStream) {
    S3_ZIP_CHUNKSIZE = 128;
    decompressReader.resizeDecompressReaderBuffer(S3_ZIP_CHUNKSIZE);

    // set an incorrect encoding stream to Mock directly.
    // it will produce 'Z_DATA_ERROR' when decompressing
    char hello[] = "abcdefghigklmnopqrstuvwxyz";  // 26+1 bytes
    this->bufReader.setData(hello, sizeof(hello));

    char outputBuffer[128] = {0};

    EXPECT_THROW(decompressReader.read(outputBuffer, sizeof(outputBuffer)), std::runtime_error);
}
