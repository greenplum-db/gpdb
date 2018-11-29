#include "s3key_reader.cpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_classes.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::Throw;

bool hasHeader = false;

char eolString[EOL_CHARS_MAX_LEN + 1] = "\n";  // LF by default

string s3extErrorMessage;

volatile bool QueryCancelPending = false;

// As we cannot catch 'IsAbortInProgress()' in UT, so here consider QueryCancelPending only
bool S3QueryIsAbortInProgress(void) {
    return QueryCancelPending;
}

void MaskThreadSignals() {
}

void *S3Alloc(size_t size) {
    return malloc(size);
}

void S3Free(void *p) {
    free(p);
}

// ================== S3KeyReaderTest ===================
class S3KeyReaderTest : public testing::Test, public S3KeyReader {
   protected:
    // Remember that SetUp() is run immediately before a test starts.
    virtual void SetUp() {
        memset(buffer, 0, 256);

        eolString[0] = '\n';
        eolString[1] = '\0';

        QueryCancelPending = false;

        this->setS3InterfaceService(&s3Interface);
    }

    // TearDown() is invoked immediately after a test finishes.
    virtual void TearDown() {
        this->close();

        QueryCancelPending = false;
    }

    char buffer[256];

    MockS3Interface s3Interface;
};

TEST(OffsetMgr, simple) {
    OffsetMgr o;
    o.setKeySize(4096);
    o.setChunkSize(1000);

    EXPECT_EQ((uint64_t)1000, o.getChunkSize());
    EXPECT_EQ((uint64_t)4096, o.getKeySize());

    Range r = o.getNextOffset();
    EXPECT_EQ((uint64_t)0, r.offset);
    EXPECT_EQ((uint64_t)1000, r.length);

    r = o.getNextOffset();
    EXPECT_EQ((uint64_t)1000, r.offset);
    EXPECT_EQ((uint64_t)1000, r.length);

    r = o.getNextOffset();
    EXPECT_EQ((uint64_t)2000, r.offset);
    EXPECT_EQ((uint64_t)1000, r.length);

    r = o.getNextOffset();
    EXPECT_EQ((uint64_t)3000, r.offset);
    EXPECT_EQ((uint64_t)1000, r.length);

    r = o.getNextOffset();
    EXPECT_EQ((uint64_t)4000, r.offset);
    EXPECT_EQ((uint64_t)96, r.length);

    EXPECT_EQ((uint64_t)1000, o.getChunkSize());
    EXPECT_EQ((uint64_t)4096, o.getKeySize());
}

TEST(OffsetMgr, KeySizeSmallerThanChunkSize) {
    OffsetMgr o;
    o.setKeySize(127);
    o.setChunkSize(1000);

    EXPECT_EQ((uint64_t)1000, o.getChunkSize());
    EXPECT_EQ((uint64_t)127, o.getKeySize());

    Range r = o.getNextOffset();
    EXPECT_EQ((uint64_t)0, r.offset);
    EXPECT_EQ((uint64_t)127, r.length);

    r = o.getNextOffset();
    EXPECT_EQ((uint64_t)127, r.offset);
    EXPECT_EQ((uint64_t)0, r.length);

    r = o.getNextOffset();
    EXPECT_EQ((uint64_t)127, r.offset);
    EXPECT_EQ((uint64_t)0, r.length);
}

TEST(OffsetMgr, KeySizeEqualToChunkSize) {
    OffsetMgr o;
    o.setKeySize(127);
    o.setChunkSize(127);

    EXPECT_EQ((uint64_t)127, o.getChunkSize());
    EXPECT_EQ((uint64_t)127, o.getKeySize());

    Range r = o.getNextOffset();
    EXPECT_EQ((uint64_t)0, r.offset);
    EXPECT_EQ((uint64_t)127, r.length);

    r = o.getNextOffset();
    EXPECT_EQ((uint64_t)127, r.offset);
    EXPECT_EQ((uint64_t)0, r.length);

    r = o.getNextOffset();
    EXPECT_EQ((uint64_t)127, r.offset);
    EXPECT_EQ((uint64_t)0, r.length);
}

TEST(OffsetMgr, KeySizeIsDevidedByChunkSize) {
    OffsetMgr o;
    o.setKeySize(635);
    o.setChunkSize(127);

    EXPECT_EQ((uint64_t)127, o.getChunkSize());
    EXPECT_EQ((uint64_t)635, o.getKeySize());

    Range r = o.getNextOffset();
    EXPECT_EQ((uint64_t)0, r.offset);
    EXPECT_EQ((uint64_t)127, r.length);

    r = o.getNextOffset();
    EXPECT_EQ((uint64_t)127, r.offset);
    EXPECT_EQ((uint64_t)127, r.length);

    r = o.getNextOffset();
    EXPECT_EQ((uint64_t)254, r.offset);
    EXPECT_EQ((uint64_t)127, r.length);

    r = o.getNextOffset();
    EXPECT_EQ((uint64_t)381, r.offset);
    EXPECT_EQ((uint64_t)127, r.length);

    r = o.getNextOffset();
    EXPECT_EQ((uint64_t)508, r.offset);
    EXPECT_EQ((uint64_t)127, r.length);

    r = o.getNextOffset();
    EXPECT_EQ((uint64_t)635, r.offset);
    EXPECT_EQ((uint64_t)0, r.length);

    r = o.getNextOffset();
    EXPECT_EQ((uint64_t)635, r.offset);
    EXPECT_EQ((uint64_t)0, r.length);
}

TEST(OffsetMgr, KeySizeIsZero) {
    OffsetMgr o;
    o.setKeySize(0);
    o.setChunkSize(1000);

    EXPECT_EQ((uint64_t)1000, o.getChunkSize());
    EXPECT_EQ((uint64_t)0, o.getKeySize());

    Range r = o.getNextOffset();
    EXPECT_EQ((uint64_t)0, r.offset);
    EXPECT_EQ((uint64_t)0, r.length);

    r = o.getNextOffset();
    EXPECT_EQ((uint64_t)0, r.offset);
    EXPECT_EQ((uint64_t)0, r.length);

    r = o.getNextOffset();
    EXPECT_EQ((uint64_t)0, r.offset);
    EXPECT_EQ((uint64_t)0, r.length);
}

TEST(OffsetMgr, reset) {
    OffsetMgr o;
    o.setKeySize(4096);
    o.setChunkSize(1000);

    EXPECT_EQ((uint64_t)1000, o.getChunkSize());
    EXPECT_EQ((uint64_t)4096, o.getKeySize());

    Range r = o.getNextOffset();
    EXPECT_EQ((uint64_t)0, r.offset);
    EXPECT_EQ((uint64_t)1000, r.length);

    r = o.getNextOffset();
    EXPECT_EQ((uint64_t)1000, r.offset);
    EXPECT_EQ((uint64_t)1000, r.length);

    o.reset();

    EXPECT_EQ((uint64_t)0, o.getChunkSize());
    EXPECT_EQ((uint64_t)0, o.getKeySize());
    EXPECT_EQ((uint64_t)0, o.getCurPos());
}

TEST_F(S3KeyReaderTest, OpenWithZeroChunk) {
    S3Params params("s3://abc/def");

    params.setNumOfChunks(0);

    EXPECT_THROW(this->open(params), S3RuntimeError);
}

// Mock function object of fetchData
// Precise control return value and content of data buffer.
class MockFetchData {
   public:
    MockFetchData(uint64_t returnLen, uint64_t chunkSize)
        : retnLen(returnLen), chunkData(chunkSize) {
    }

    MockFetchData(const MockFetchData &other) {
        retnLen = other.retnLen;
        chunkData = other.chunkData;
    }

    uint64_t operator()(uint64_t offset, S3VectorUInt8 &data, uint64_t len,
                        const S3Url &sourceUrl) {
        data = std::move(chunkData);
        return retnLen;
    }

   private:
    uint64_t retnLen;
    S3VectorUInt8 chunkData;
};

TEST_F(S3KeyReaderTest, ReadWithSingleChunk) {
    S3Params params("s3://abc/def");
    params.setNumOfChunks(1);
    params.setKeySize(255);
    params.setChunkSize(8192);

    EXPECT_CALL(s3Interface, fetchData(_, _, _, _)).WillOnce(Invoke(MockFetchData(255, 8192)));

    this->open(params);

    EXPECT_EQ((uint64_t)255, this->read(buffer, 64 * 1024));
    EXPECT_EQ((uint64_t)1, this->read(buffer, 64 * 1024));
    EXPECT_EQ((uint64_t)0, this->read(buffer, 64 * 1024));
}

TEST_F(S3KeyReaderTest, ReadWithSingleChunkNormalCase) {
    // Read buffer < chunk size < key size
    S3Params params("s3://abc/def");
    params.setNumOfChunks(1);
    params.setKeySize(255);
    params.setChunkSize(64);

    EXPECT_CALL(s3Interface, fetchData(_, _, _, _))
        .WillOnce(Invoke(MockFetchData(64, 64)))
        .WillOnce(Invoke(MockFetchData(64, 64)))
        .WillOnce(Invoke(MockFetchData(64, 64)))
        .WillOnce(Invoke(MockFetchData(63, 64)));

    this->open(params);

    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)1, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)0, this->read(buffer, 32));
}

TEST_F(S3KeyReaderTest, ReadWithSmallBuffer) {
    S3Params params("s3://abc/def");

    params.setNumOfChunks(1);
    params.setKeySize(255);
    params.setChunkSize(8192);

    EXPECT_CALL(s3Interface, fetchData(_, _, _, _)).WillOnce(Invoke(MockFetchData(255, 8192)));

    this->open(params);

    EXPECT_EQ((uint64_t)64, this->read(buffer, 64));
    EXPECT_EQ((uint64_t)64, this->read(buffer, 64));
    EXPECT_EQ((uint64_t)64, this->read(buffer, 64));
    EXPECT_EQ((uint64_t)63, this->read(buffer, 64));
    EXPECT_EQ((uint64_t)1, this->read(buffer, 64));
    EXPECT_EQ((uint64_t)0, this->read(buffer, 64));
}

TEST_F(S3KeyReaderTest, CloseWithoutFinishReading) {
    S3Params params("s3://abc/def");

    params.setNumOfChunks(1);
    params.setKeySize(255);
    params.setChunkSize(8192);

    EXPECT_CALL(s3Interface, fetchData(_, _, _, _)).WillOnce(Invoke(MockFetchData(255, 8192)));

    // expect data fetched in 64,64,64,63,0 bulks
    //   when close it before finish, should have no problem
    this->open(params);

    EXPECT_EQ((uint64_t)64, this->read(buffer, 64));
    EXPECT_EQ((uint64_t)64, this->read(buffer, 64));
}

TEST_F(S3KeyReaderTest, ResetByInvokingClose) {
    S3Params params("s3://abc/def");

    params.setNumOfChunks(1);
    params.setKeySize(255);
    params.setChunkSize(8192);

    EXPECT_CALL(s3Interface, fetchData(_, _, _, _)).WillOnce(Invoke(MockFetchData(255, 8192)));

    this->open(params);

    EXPECT_EQ((uint64_t)64, this->read(buffer, 64));
    EXPECT_EQ((uint64_t)64, this->read(buffer, 64));
    EXPECT_EQ((uint64_t)64, this->read(buffer, 64));
    EXPECT_EQ((uint64_t)63, this->read(buffer, 64));
    EXPECT_EQ((uint64_t)1, this->read(buffer, 64));
    EXPECT_EQ((uint64_t)0, this->read(buffer, 64));

    this->close();

    EXPECT_TRUE(this->getThreads().empty());
    EXPECT_TRUE(this->getChunkBuffers().empty());

    EXPECT_EQ((uint64_t)0, this->getCurReadingChunk());
    EXPECT_EQ((uint64_t)0, this->getTransferredKeyLen());
    EXPECT_FALSE(this->isSharedError());
}

TEST_F(S3KeyReaderTest, ReadWithSmallKeySize) {
    S3Params params("s3://abc/def");

    params.setNumOfChunks(1);
    params.setKeySize(2);
    params.setChunkSize(8192);

    EXPECT_CALL(s3Interface, fetchData(_, _, _, _)).WillOnce(Invoke(MockFetchData(2, 8192)));

    this->open(params);

    EXPECT_EQ((uint64_t)2, this->read(buffer, 64));
    EXPECT_EQ((uint64_t)1, this->read(buffer, 64));
    EXPECT_EQ((uint64_t)0, this->read(buffer, 64));
}

TEST_F(S3KeyReaderTest, ReadWithSmallChunk) {
    S3Params params("s3://abc/def");

    params.setNumOfChunks(1);
    params.setKeySize(255);
    params.setChunkSize(64);

    EXPECT_CALL(s3Interface, fetchData(_, _, _, _))
        .WillOnce(Invoke(MockFetchData(64, 64)))
        .WillOnce(Invoke(MockFetchData(64, 64)))
        .WillOnce(Invoke(MockFetchData(64, 64)))
        .WillOnce(Invoke(MockFetchData(63, 64)));

    this->open(params);

    EXPECT_EQ((uint64_t)64, this->read(buffer, 64));
    EXPECT_EQ((uint64_t)64, this->read(buffer, 64));
    EXPECT_EQ((uint64_t)64, this->read(buffer, 64));
    EXPECT_EQ((uint64_t)63, this->read(buffer, 64));
    EXPECT_EQ((uint64_t)1, this->read(buffer, 64));
    EXPECT_EQ((uint64_t)0, this->read(buffer, 64));
}

TEST_F(S3KeyReaderTest, ReadWithSmallChunkDividedKeySize) {
    S3Params params("s3://abc/def");

    params.setNumOfChunks(1);

    params.setKeySize(256);
    params.setChunkSize(64);

    EXPECT_CALL(s3Interface, fetchData(_, _, _, _))
        .WillOnce(Invoke(MockFetchData(64, 64)))
        .WillOnce(Invoke(MockFetchData(64, 64)))
        .WillOnce(Invoke(MockFetchData(64, 64)))
        .WillOnce(Invoke(MockFetchData(64, 64)));

    this->open(params);

    EXPECT_EQ((uint64_t)64, this->read(buffer, 64));
    EXPECT_EQ((uint64_t)64, this->read(buffer, 64));
    EXPECT_EQ((uint64_t)64, this->read(buffer, 64));
    EXPECT_EQ((uint64_t)64, this->read(buffer, 64));
    EXPECT_EQ((uint64_t)1, this->read(buffer, 64));
    EXPECT_EQ((uint64_t)0, this->read(buffer, 64));
}

TEST_F(S3KeyReaderTest, ReadWithChunkLargerThanReadBufferAndKeySize) {
    S3Params params("s3://abc/def");

    params.setNumOfChunks(1);

    params.setKeySize(255);
    params.setChunkSize(8192);

    EXPECT_CALL(s3Interface, fetchData(_, _, _, _)).WillOnce(Invoke(MockFetchData(255, 8192)));

    this->open(params);

    EXPECT_EQ((uint64_t)255, this->read(buffer, 255));
    EXPECT_EQ((uint64_t)1, this->read(buffer, 255));
    EXPECT_EQ((uint64_t)0, this->read(buffer, 255));
}

TEST_F(S3KeyReaderTest, ReadWithKeyLargerThanChunkSize) {
    S3Params params("s3://abc/def");

    params.setNumOfChunks(1);

    params.setKeySize(1024);
    params.setChunkSize(255);

    EXPECT_CALL(s3Interface, fetchData(_, _, _, _))
        .WillOnce(Invoke(MockFetchData(255, 255)))
        .WillOnce(Invoke(MockFetchData(255, 255)))
        .WillOnce(Invoke(MockFetchData(255, 255)))
        .WillOnce(Invoke(MockFetchData(255, 255)))
        .WillOnce(Invoke(MockFetchData(4, 255)));

    this->open(params);

    EXPECT_EQ((uint64_t)255, this->read(buffer, 255));
    EXPECT_EQ((uint64_t)255, this->read(buffer, 255));
    EXPECT_EQ((uint64_t)255, this->read(buffer, 255));
    EXPECT_EQ((uint64_t)255, this->read(buffer, 255));
    EXPECT_EQ((uint64_t)4, this->read(buffer, 255));
    EXPECT_EQ((uint64_t)1, this->read(buffer, 255));
    EXPECT_EQ((uint64_t)0, this->read(buffer, 255));
}

TEST_F(S3KeyReaderTest, ReadWithSameKeyChunkReadSize) {
    S3Params params("s3://abc/def");

    params.setNumOfChunks(1);

    params.setKeySize(255);
    params.setChunkSize(255);

    EXPECT_CALL(s3Interface, fetchData(_, _, _, _)).WillOnce(Invoke(MockFetchData(255, 255)));

    this->open(params);

    EXPECT_EQ((uint64_t)255, this->read(buffer, 255));
    EXPECT_EQ((uint64_t)1, this->read(buffer, 255));
    EXPECT_EQ((uint64_t)0, this->read(buffer, 255));
}

TEST_F(S3KeyReaderTest, MTReadWith2Chunks) {
    S3Params params("s3://abc/def");

    params.setNumOfChunks(2);

    params.setKeySize(255);
    params.setChunkSize(64);

    EXPECT_CALL(s3Interface, fetchData(0, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3Interface, fetchData(64, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3Interface, fetchData(128, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3Interface, fetchData(192, _, _, _)).WillOnce(Invoke(MockFetchData(63, 64)));

    this->open(params);

    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)1, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)0, this->read(buffer, 32));
}

TEST_F(S3KeyReaderTest, MTReadWithRedundantChunks) {
    S3Params params("s3://abc/def");

    params.setNumOfChunks(8);

    params.setKeySize(255);
    params.setChunkSize(64);

    EXPECT_CALL(s3Interface, fetchData(0, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3Interface, fetchData(64, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3Interface, fetchData(128, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3Interface, fetchData(192, _, _, _)).WillOnce(Invoke(MockFetchData(63, 64)));

    this->open(params);

    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)1, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)0, this->read(buffer, 32));
}

TEST_F(S3KeyReaderTest, MTReadWithReusedAndUnreusedChunks) {
    S3Params params("s3://abc/def");

    params.setNumOfChunks(3);

    params.setKeySize(255);
    params.setChunkSize(64);

    EXPECT_CALL(s3Interface, fetchData(0, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3Interface, fetchData(64, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3Interface, fetchData(128, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3Interface, fetchData(192, _, _, _)).WillOnce(Invoke(MockFetchData(63, 64)));

    this->open(params);

    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)32, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)1, this->read(buffer, 32));
    EXPECT_EQ((uint64_t)0, this->read(buffer, 32));
}

TEST_F(S3KeyReaderTest, MTReadWithChunksSmallerThanReadBuffer) {
    S3Params params("s3://abc/def");
    params.setNumOfChunks(3);

    params.setKeySize(255);
    params.setChunkSize(64);

    EXPECT_CALL(s3Interface, fetchData(0, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3Interface, fetchData(64, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3Interface, fetchData(128, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3Interface, fetchData(192, _, _, _)).WillOnce(Invoke(MockFetchData(63, 64)));

    this->open(params);

    EXPECT_EQ((uint64_t)64, this->read(buffer, 127));
    EXPECT_EQ((uint64_t)64, this->read(buffer, 127));
    EXPECT_EQ((uint64_t)64, this->read(buffer, 127));
    EXPECT_EQ((uint64_t)63, this->read(buffer, 127));
    EXPECT_EQ((uint64_t)1, this->read(buffer, 127));
    EXPECT_EQ((uint64_t)0, this->read(buffer, 127));
}

TEST_F(S3KeyReaderTest, MTReadWithFragmentalReadRequests) {
    S3Params params("s3://abc/def");
    params.setNumOfChunks(5);

    params.setKeySize(255);
    params.setChunkSize(64);

    EXPECT_CALL(s3Interface, fetchData(0, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3Interface, fetchData(64, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3Interface, fetchData(128, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3Interface, fetchData(192, _, _, _)).WillOnce(Invoke(MockFetchData(63, 64)));

    this->open(params);

    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)2, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)2, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)2, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)1, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)1, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)0, this->read(buffer, 31));
}

TEST_F(S3KeyReaderTest, MTReadWithHundredsOfThreads) {
    S3Params params("s3://abc/def");
    params.setNumOfChunks(127);

    params.setKeySize(1024);
    params.setChunkSize(64);

    EXPECT_CALL(s3Interface, fetchData(_, _, _, _)).WillRepeatedly(Invoke(MockFetchData(64, 64)));

    this->open(params);

    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)2, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)2, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)2, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)2, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)2, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)2, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)2, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)2, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)2, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)2, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)2, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)2, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)2, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)2, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)2, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)2, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)1, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)0, this->read(buffer, 31));
}

TEST_F(S3KeyReaderTest, MTReadWithFetchDataError) {
    S3Params params("s3://abc/def");
    params.setNumOfChunks(3);

    params.setKeySize(255);
    params.setChunkSize(64);

    EXPECT_CALL(s3Interface, fetchData(0, _, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3Interface, fetchData(64, _, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3Interface, fetchData(128, _, _, _))
        .Times(AtMost(1))
        .WillOnce(Throw(S3FailedAfterRetry("", 1, "")));
    EXPECT_CALL(s3Interface, fetchData(192, _, _, _))
        .Times(AtMost(1))
        .WillOnce(Throw(S3FailedAfterRetry("", 1, "")));

    this->open(params);

    try {
        this->read(buffer, 127);
    } catch (...) {
    }

    try {
        this->read(buffer, 127);
    } catch (...) {
    }

    EXPECT_THROW(this->read(buffer, 127), S3FailedAfterRetry);
    EXPECT_THROW(this->read(buffer, 127), S3FailedAfterRetry);
}

TEST_F(S3KeyReaderTest, MTReadWithUnexpectedFetchData) {
    S3Params params("s3://abc/def");
    params.setNumOfChunks(3);

    params.setKeySize(255);
    params.setChunkSize(64);

    EXPECT_CALL(s3Interface, fetchData(0, _, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3Interface, fetchData(64, _, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3Interface, fetchData(128, _, _, _))
        .Times(AtMost(1))
        .WillOnce(Throw(S3PartialResponseError(63, 64)));
    EXPECT_CALL(s3Interface, fetchData(192, _, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(MockFetchData(63, 64)));

    this->open(params);

    try {
        this->read(buffer, 127);
    } catch (...) {
    }

    try {
        this->read(buffer, 127);
    } catch (...) {
    }

    EXPECT_THROW(this->read(buffer, 127), S3PartialResponseError);
    EXPECT_THROW(this->read(buffer, 127), S3PartialResponseError);
}

TEST_F(S3KeyReaderTest, MTReadWithUnexpectedFetchDataAtSecondRound) {
    S3Params params("s3://abc/def");
    params.setNumOfChunks(2);

    params.setKeySize(255);
    params.setChunkSize(64);

    EXPECT_CALL(s3Interface, fetchData(0, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3Interface, fetchData(64, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3Interface, fetchData(128, _, _, _))
        .WillOnce(Throw(S3PartialResponseError(63, 64)));
    EXPECT_CALL(s3Interface, fetchData(192, _, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(MockFetchData(63, 64)));

    this->open(params);

    try {
        this->read(buffer, 127);
    } catch (...) {
    }

    try {
        this->read(buffer, 127);
    } catch (...) {
    }

    EXPECT_THROW(this->read(buffer, 127), S3PartialResponseError);
    EXPECT_THROW(this->read(buffer, 127), S3PartialResponseError);
}

TEST_F(S3KeyReaderTest, MTReadWithGPDBCancel) {
    S3Params params("s3://abc/def");
    params.setNumOfChunks(3);

    params.setKeySize(255);
    params.setChunkSize(64);

    EXPECT_CALL(s3Interface, fetchData(0, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3Interface, fetchData(64, _, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3Interface, fetchData(128, _, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3Interface, fetchData(192, _, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(MockFetchData(63, 64)));

    this->open(params);

    EXPECT_EQ((uint64_t)64, this->read(buffer, 127));
    QueryCancelPending = true;

    // Note for coverage test, due to multithread execution, in this test case
    // QueryCancelPending may or may not be hit in DownloadThreadFunc,
    // QueryCancelPending in ChunkBuffer::read will always be hit and throw.

    EXPECT_THROW(this->read(buffer, 127), S3QueryAbort);
}

TEST_F(S3KeyReaderTest, MTReadWithHundredsOfThreadsAndSignalCancel) {
    S3Params params("s3://abc/def");
    params.setNumOfChunks(127);
    params.setKeySize(1024);
    params.setChunkSize(64);

    EXPECT_CALL(s3Interface, fetchData(_, _, _, _)).WillRepeatedly(Invoke(MockFetchData(64, 64)));

    this->open(params);

    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)2, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)2, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)31, this->read(buffer, 31));
    EXPECT_EQ((uint64_t)2, this->read(buffer, 31));

    QueryCancelPending = true;

    EXPECT_THROW(this->read(buffer, 31), S3QueryAbort);
}

TEST(ChunkBuffer, ChunkBufferOperatorEqual) {
    S3Url s3Url("s3://whatever");
    S3KeyReader reader;
    S3MemoryContext context;

    ChunkBuffer buf1(s3Url, reader, context);
    ChunkBuffer buf2(S3Url(""), reader, context);

    buf1.setStatus(ReadyToRead);
    buf2.setStatus(ReadyToFill);
    buf1 = buf2;

    EXPECT_EQ(ReadyToFill, buf1.getStatus());
}
