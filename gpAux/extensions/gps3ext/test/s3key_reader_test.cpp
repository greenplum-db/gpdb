#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "mock_classes.h"
#include "reader_params.h"
#include "s3key_reader.cpp"

using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::Throw;
using ::testing::_;

volatile bool QueryCancelPending = false;

// ================== S3KeyReaderTest ===================
class S3KeyReaderTest : public testing::Test, public S3KeyReader {
   protected:
    // Remember that SetUp() is run immediately before a test starts.
    virtual void SetUp() {
        QueryCancelPending = false;
        this->setS3interface(&s3interface);
    }

    // TearDown() is invoked immediately after a test finishes.
    virtual void TearDown() {
        this->close();
    }

    ReaderParams params;
    char buffer[256];

    MockS3Interface s3interface;
};

TEST(OffsetMgr, simple) {
    OffsetMgr o;
    o.setKeySize(4096);
    o.setChunkSize(1000);

    EXPECT_EQ(1000, o.getChunkSize());
    EXPECT_EQ(4096, o.getKeySize());

    Range r = o.getNextOffset();
    EXPECT_EQ(r.offset, 0);
    EXPECT_EQ(r.length, 1000);

    r = o.getNextOffset();
    EXPECT_EQ(r.offset, 1000);
    EXPECT_EQ(r.length, 1000);

    r = o.getNextOffset();
    EXPECT_EQ(r.offset, 2000);
    EXPECT_EQ(r.length, 1000);

    r = o.getNextOffset();
    EXPECT_EQ(r.offset, 3000);
    EXPECT_EQ(r.length, 1000);

    r = o.getNextOffset();
    EXPECT_EQ(r.offset, 4000);
    EXPECT_EQ(r.length, 96);

    EXPECT_EQ(1000, o.getChunkSize());
    EXPECT_EQ(4096, o.getKeySize());
}

TEST(OffsetMgr, KeySizeSmallerThanChunkSize) {
    OffsetMgr o;
    o.setKeySize(127);
    o.setChunkSize(1000);

    EXPECT_EQ(1000, o.getChunkSize());
    EXPECT_EQ(127, o.getKeySize());

    Range r = o.getNextOffset();
    EXPECT_EQ(r.offset, 0);
    EXPECT_EQ(r.length, 127);

    r = o.getNextOffset();
    EXPECT_EQ(r.offset, 127);
    EXPECT_EQ(r.length, 0);

    r = o.getNextOffset();
    EXPECT_EQ(r.offset, 127);
    EXPECT_EQ(r.length, 0);
}

TEST(OffsetMgr, KeySizeEqualToChunkSize) {
    OffsetMgr o;
    o.setKeySize(127);
    o.setChunkSize(127);

    EXPECT_EQ(127, o.getChunkSize());
    EXPECT_EQ(127, o.getKeySize());

    Range r = o.getNextOffset();
    EXPECT_EQ(r.offset, 0);
    EXPECT_EQ(r.length, 127);

    r = o.getNextOffset();
    EXPECT_EQ(r.offset, 127);
    EXPECT_EQ(r.length, 0);

    r = o.getNextOffset();
    EXPECT_EQ(r.offset, 127);
    EXPECT_EQ(r.length, 0);
}

TEST(OffsetMgr, KeySizeIsDevidedByChunkSize) {
    OffsetMgr o;
    o.setKeySize(635);
    o.setChunkSize(127);

    EXPECT_EQ(127, o.getChunkSize());
    EXPECT_EQ(635, o.getKeySize());

    Range r = o.getNextOffset();
    EXPECT_EQ(r.offset, 0);
    EXPECT_EQ(r.length, 127);

    r = o.getNextOffset();
    EXPECT_EQ(r.offset, 127);
    EXPECT_EQ(r.length, 127);

    r = o.getNextOffset();
    EXPECT_EQ(r.offset, 254);
    EXPECT_EQ(r.length, 127);

    r = o.getNextOffset();
    EXPECT_EQ(r.offset, 381);
    EXPECT_EQ(r.length, 127);

    r = o.getNextOffset();
    EXPECT_EQ(r.offset, 508);
    EXPECT_EQ(r.length, 127);

    r = o.getNextOffset();
    EXPECT_EQ(r.offset, 635);
    EXPECT_EQ(r.length, 0);

    r = o.getNextOffset();
    EXPECT_EQ(r.offset, 635);
    EXPECT_EQ(r.length, 0);
}

TEST(OffsetMgr, KeySizeIsZero) {
    OffsetMgr o;
    o.setKeySize(0);
    o.setChunkSize(1000);

    EXPECT_EQ(1000, o.getChunkSize());
    EXPECT_EQ(0, o.getKeySize());

    Range r = o.getNextOffset();
    EXPECT_EQ(r.offset, 0);
    EXPECT_EQ(r.length, 0);

    r = o.getNextOffset();
    EXPECT_EQ(r.offset, 0);
    EXPECT_EQ(r.length, 0);

    r = o.getNextOffset();
    EXPECT_EQ(r.offset, 0);
    EXPECT_EQ(r.length, 0);
}

TEST(OffsetMgr, reset) {
    OffsetMgr o;
    o.setKeySize(4096);
    o.setChunkSize(1000);

    EXPECT_EQ(1000, o.getChunkSize());
    EXPECT_EQ(4096, o.getKeySize());

    Range r = o.getNextOffset();
    EXPECT_EQ(r.offset, 0);
    EXPECT_EQ(r.length, 1000);

    r = o.getNextOffset();
    EXPECT_EQ(r.offset, 1000);
    EXPECT_EQ(r.length, 1000);

    o.reset();

    EXPECT_EQ(0, o.getChunkSize());
    EXPECT_EQ(0, o.getKeySize());
    EXPECT_EQ(0, o.getCurPos());
}

TEST_F(S3KeyReaderTest, OpenWithZeroChunk) {
    params.setNumOfChunks(0);

    EXPECT_THROW(this->open(params), std::runtime_error);
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

    uint64_t operator()(uint64_t offset, vector<uint8_t> &data, uint64_t len,
                        const string &sourceUrl, const string &region, const S3Credential &cred) {
        data = std::move(chunkData);
        return retnLen;
    }

   private:
    uint64_t retnLen;
    vector<uint8_t> chunkData;
};

TEST_F(S3KeyReaderTest, ReadWithSingleChunk) {
    params.setNumOfChunks(1);
    params.setRegion("us-west-2");
    params.setKeySize(255);
    params.setChunkSize(8192);

    EXPECT_CALL(s3interface, fetchData(_, _, _, _, _, _))
        .WillOnce(Invoke(MockFetchData(255, 8192)));

    this->open(params);

    EXPECT_EQ(255, this->read(buffer, 64 * 1024));
    EXPECT_EQ(0, this->read(buffer, 64 * 1024));
}

TEST_F(S3KeyReaderTest, ReadWithSingleChunkNormalCase) {
    // Read buffer < chunk size < key size
    params.setNumOfChunks(1);
    params.setRegion("us-west-2");
    params.setKeySize(255);
    params.setChunkSize(64);

    EXPECT_CALL(s3interface, fetchData(_, _, _, _, _, _))
        .WillOnce(Invoke(MockFetchData(64, 64)))
        .WillOnce(Invoke(MockFetchData(64, 64)))
        .WillOnce(Invoke(MockFetchData(64, 64)))
        .WillOnce(Invoke(MockFetchData(63, 64)));

    this->open(params);

    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(31, this->read(buffer, 32));
    EXPECT_EQ(0, this->read(buffer, 32));
}

TEST_F(S3KeyReaderTest, ReadWithSmallBuffer) {
    params.setNumOfChunks(1);
    params.setRegion("us-west-2");
    params.setKeySize(255);
    params.setChunkSize(8192);

    EXPECT_CALL(s3interface, fetchData(_, _, _, _, _, _))
        .WillOnce(Invoke(MockFetchData(255, 8192)));

    this->open(params);

    EXPECT_EQ(64, this->read(buffer, 64));
    EXPECT_EQ(64, this->read(buffer, 64));
    EXPECT_EQ(64, this->read(buffer, 64));
    EXPECT_EQ(63, this->read(buffer, 64));
    EXPECT_EQ(0, this->read(buffer, 64));
}

TEST_F(S3KeyReaderTest, CloseWithoutFinishReading) {
    params.setNumOfChunks(1);
    params.setRegion("us-west-2");
    params.setKeySize(255);
    params.setChunkSize(8192);

    EXPECT_CALL(s3interface, fetchData(_, _, _, _, _, _))
        .WillOnce(Invoke(MockFetchData(255, 8192)));

    // expect data fetched in 64,64,64,63,0 bulks
    //   when close it before finish, should have no problem
    this->open(params);

    EXPECT_EQ(64, this->read(buffer, 64));
    EXPECT_EQ(64, this->read(buffer, 64));
}

TEST_F(S3KeyReaderTest, ResetByInvokingClose) {
    params.setNumOfChunks(1);
    params.setRegion("us-west-2");
    params.setKeySize(255);
    params.setChunkSize(8192);

    EXPECT_CALL(s3interface, fetchData(_, _, _, _, _, _))
        .WillOnce(Invoke(MockFetchData(255, 8192)));

    this->open(params);

    EXPECT_EQ(64, this->read(buffer, 64));
    EXPECT_EQ(64, this->read(buffer, 64));
    EXPECT_EQ(64, this->read(buffer, 64));
    EXPECT_EQ(63, this->read(buffer, 64));
    EXPECT_EQ(0, this->read(buffer, 64));

    this->close();

    EXPECT_TRUE(this->getThreads().empty());
    EXPECT_TRUE(this->getChunkBuffers().empty());

    EXPECT_EQ(0, this->getCurReadingChunk());
    EXPECT_EQ(0, this->getTransferredKeyLen());
    EXPECT_FALSE(this->isSharedError());
}

TEST_F(S3KeyReaderTest, ReadWithSmallKeySize) {
    params.setNumOfChunks(1);
    params.setRegion("us-west-2");
    params.setKeySize(2);
    params.setChunkSize(8192);

    EXPECT_CALL(s3interface, fetchData(_, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(2, 8192)));

    this->open(params);

    EXPECT_EQ(2, this->read(buffer, 64));
    EXPECT_EQ(0, this->read(buffer, 64));
}

TEST_F(S3KeyReaderTest, ReadWithSmallChunk) {
    params.setNumOfChunks(1);
    params.setRegion("us-west-2");
    params.setKeySize(255);
    params.setChunkSize(64);

    EXPECT_CALL(s3interface, fetchData(_, _, _, _, _, _))
        .WillOnce(Invoke(MockFetchData(64, 64)))
        .WillOnce(Invoke(MockFetchData(64, 64)))
        .WillOnce(Invoke(MockFetchData(64, 64)))
        .WillOnce(Invoke(MockFetchData(63, 64)));

    this->open(params);

    EXPECT_EQ(64, this->read(buffer, 64));
    EXPECT_EQ(64, this->read(buffer, 64));
    EXPECT_EQ(64, this->read(buffer, 64));
    EXPECT_EQ(63, this->read(buffer, 64));
    EXPECT_EQ(0, this->read(buffer, 64));
}

TEST_F(S3KeyReaderTest, ReadWithSmallChunkDividedKeySize) {
    params.setNumOfChunks(1);
    params.setRegion("us-west-2");
    params.setKeySize(256);
    params.setChunkSize(64);

    EXPECT_CALL(s3interface, fetchData(_, _, _, _, _, _))
        .WillOnce(Invoke(MockFetchData(64, 64)))
        .WillOnce(Invoke(MockFetchData(64, 64)))
        .WillOnce(Invoke(MockFetchData(64, 64)))
        .WillOnce(Invoke(MockFetchData(64, 64)));

    this->open(params);

    EXPECT_EQ(64, this->read(buffer, 64));
    EXPECT_EQ(64, this->read(buffer, 64));
    EXPECT_EQ(64, this->read(buffer, 64));
    EXPECT_EQ(64, this->read(buffer, 64));
    EXPECT_EQ(0, this->read(buffer, 64));
}

TEST_F(S3KeyReaderTest, ReadWithChunkLargerThanReadBufferAndKeySize) {
    params.setNumOfChunks(1);
    params.setRegion("us-west-2");
    params.setKeySize(255);
    params.setChunkSize(8192);

    EXPECT_CALL(s3interface, fetchData(_, _, _, _, _, _))
        .WillOnce(Invoke(MockFetchData(255, 8192)));

    this->open(params);

    EXPECT_EQ(255, this->read(buffer, 255));
    EXPECT_EQ(0, this->read(buffer, 255));
}

TEST_F(S3KeyReaderTest, ReadWithKeyLargerThanChunkSize) {
    params.setNumOfChunks(1);
    params.setRegion("us-west-2");
    params.setKeySize(1024);
    params.setChunkSize(255);

    EXPECT_CALL(s3interface, fetchData(_, _, _, _, _, _))
        .WillOnce(Invoke(MockFetchData(255, 255)))
        .WillOnce(Invoke(MockFetchData(255, 255)))
        .WillOnce(Invoke(MockFetchData(255, 255)))
        .WillOnce(Invoke(MockFetchData(255, 255)))
        .WillOnce(Invoke(MockFetchData(4, 255)));

    this->open(params);

    EXPECT_EQ(255, this->read(buffer, 255));
    EXPECT_EQ(255, this->read(buffer, 255));
    EXPECT_EQ(255, this->read(buffer, 255));
    EXPECT_EQ(255, this->read(buffer, 255));
    EXPECT_EQ(4, this->read(buffer, 255));
    EXPECT_EQ(0, this->read(buffer, 255));
}

TEST_F(S3KeyReaderTest, ReadWithSameKeyChunkReadSize) {
    params.setNumOfChunks(1);
    params.setRegion("us-west-2");
    params.setKeySize(255);
    params.setChunkSize(255);

    EXPECT_CALL(s3interface, fetchData(_, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(255, 255)));

    this->open(params);

    EXPECT_EQ(255, this->read(buffer, 255));
    EXPECT_EQ(0, this->read(buffer, 255));
}

TEST_F(S3KeyReaderTest, MTReadWith2Chunks) {
    params.setNumOfChunks(2);
    params.setRegion("us-west-2");
    params.setKeySize(255);
    params.setChunkSize(64);

    EXPECT_CALL(s3interface, fetchData(0, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3interface, fetchData(64, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3interface, fetchData(128, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3interface, fetchData(192, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(63, 64)));

    this->open(params);

    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(31, this->read(buffer, 32));
    EXPECT_EQ(0, this->read(buffer, 32));
}

TEST_F(S3KeyReaderTest, MTReadWithRedundantChunks) {
    params.setNumOfChunks(8);
    params.setRegion("us-west-2");
    params.setKeySize(255);
    params.setChunkSize(64);

    EXPECT_CALL(s3interface, fetchData(0, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3interface, fetchData(64, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3interface, fetchData(128, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3interface, fetchData(192, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(63, 64)));

    this->open(params);

    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(31, this->read(buffer, 32));
    EXPECT_EQ(0, this->read(buffer, 32));
}

TEST_F(S3KeyReaderTest, MTReadWithReusedAndUnreusedChunks) {
    params.setNumOfChunks(3);
    params.setRegion("us-west-2");
    params.setKeySize(255);
    params.setChunkSize(64);

    EXPECT_CALL(s3interface, fetchData(0, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3interface, fetchData(64, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3interface, fetchData(128, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3interface, fetchData(192, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(63, 64)));

    this->open(params);

    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(32, this->read(buffer, 32));
    EXPECT_EQ(31, this->read(buffer, 32));
    EXPECT_EQ(0, this->read(buffer, 32));
}

TEST_F(S3KeyReaderTest, MTReadWithChunksSmallerThanReadBuffer) {
    params.setNumOfChunks(3);
    params.setRegion("us-west-2");
    params.setKeySize(255);
    params.setChunkSize(64);

    EXPECT_CALL(s3interface, fetchData(0, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3interface, fetchData(64, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3interface, fetchData(128, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3interface, fetchData(192, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(63, 64)));

    this->open(params);

    EXPECT_EQ(64, this->read(buffer, 127));
    EXPECT_EQ(64, this->read(buffer, 127));
    EXPECT_EQ(64, this->read(buffer, 127));
    EXPECT_EQ(63, this->read(buffer, 127));
    EXPECT_EQ(0, this->read(buffer, 127));
}

TEST_F(S3KeyReaderTest, MTReadWithFragmentalReadRequests) {
    params.setNumOfChunks(5);
    params.setRegion("us-west-2");
    params.setKeySize(255);
    params.setChunkSize(64);

    EXPECT_CALL(s3interface, fetchData(0, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3interface, fetchData(64, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3interface, fetchData(128, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3interface, fetchData(192, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(63, 64)));

    this->open(params);

    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(2, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(2, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(2, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(1, this->read(buffer, 31));
    EXPECT_EQ(0, this->read(buffer, 31));
}

TEST_F(S3KeyReaderTest, MTReadWithHundredsOfThreads) {
    params.setNumOfChunks(127);
    params.setRegion("us-west-2");
    params.setKeySize(1024);
    params.setChunkSize(64);

    EXPECT_CALL(s3interface, fetchData(_, _, _, _, _, _))
        .WillRepeatedly(Invoke(MockFetchData(64, 64)));

    this->open(params);

    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(2, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(2, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(2, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(2, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(2, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(2, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(2, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(2, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(2, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(2, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(2, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(2, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(2, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(2, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(2, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(2, this->read(buffer, 31));
    EXPECT_EQ(0, this->read(buffer, 31));
}

TEST_F(S3KeyReaderTest, MTReadWithFetchDataError) {
    params.setNumOfChunks(3);
    params.setRegion("us-west-2");
    params.setKeySize(255);
    params.setChunkSize(64);

    EXPECT_CALL(s3interface, fetchData(0, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3interface, fetchData(64, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3interface, fetchData(128, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(-1, 64)));
    EXPECT_CALL(s3interface, fetchData(192, _, _, _, _, _))
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

    EXPECT_THROW(this->read(buffer, 127), std::runtime_error);
    EXPECT_THROW(this->read(buffer, 127), std::runtime_error);
}

TEST_F(S3KeyReaderTest, MTReadWithUnexpectedFetchData) {
    params.setNumOfChunks(3);
    params.setRegion("us-west-2");
    params.setKeySize(255);
    params.setChunkSize(64);

    EXPECT_CALL(s3interface, fetchData(0, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3interface, fetchData(64, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3interface, fetchData(128, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(42, 64)));
    EXPECT_CALL(s3interface, fetchData(192, _, _, _, _, _))
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

    EXPECT_THROW(this->read(buffer, 127), std::runtime_error);
    EXPECT_THROW(this->read(buffer, 127), std::runtime_error);
}

TEST_F(S3KeyReaderTest, MTReadWithUnexpectedFetchDataAtSecondRound) {
    params.setNumOfChunks(2);
    params.setRegion("us-west-2");
    params.setKeySize(255);
    params.setChunkSize(64);

    EXPECT_CALL(s3interface, fetchData(0, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3interface, fetchData(64, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3interface, fetchData(128, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(42, 64)));
    EXPECT_CALL(s3interface, fetchData(192, _, _, _, _, _))
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

    EXPECT_THROW(this->read(buffer, 127), std::runtime_error);
    EXPECT_THROW(this->read(buffer, 127), std::runtime_error);
}

TEST_F(S3KeyReaderTest, MTReadWithGPDBCancel) {
    params.setNumOfChunks(3);
    params.setRegion("us-west-2");
    params.setKeySize(255);
    params.setChunkSize(64);

    EXPECT_CALL(s3interface, fetchData(0, _, _, _, _, _)).WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3interface, fetchData(64, _, _, _, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3interface, fetchData(128, _, _, _, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(MockFetchData(64, 64)));
    EXPECT_CALL(s3interface, fetchData(192, _, _, _, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(MockFetchData(63, 64)));

    this->open(params);

    EXPECT_EQ(64, this->read(buffer, 127));
    QueryCancelPending = true;

    // Note for coverage test, due to multithread execution, in this test case
    // QueryCancelPending may or may not be hit in DownloadThreadFunc,
    // QueryCancelPending in ChunkBuffer::read will always be hit and throw.

    EXPECT_THROW(this->read(buffer, 127), std::runtime_error);

    // be careful to reset it, otherwise following tests will suck.
    QueryCancelPending = false;
}

TEST_F(S3KeyReaderTest, MTReadWithHundredsOfThreadsAndSignalCancel) {
    params.setNumOfChunks(127);
    params.setRegion("us-west-2");
    params.setKeySize(1024);
    params.setChunkSize(64);

    EXPECT_CALL(s3interface, fetchData(_, _, _, _, _, _))
        .WillRepeatedly(Invoke(MockFetchData(64, 64)));

    this->open(params);

    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(2, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(2, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(31, this->read(buffer, 31));
    EXPECT_EQ(2, this->read(buffer, 31));

    QueryCancelPending = true;

    EXPECT_THROW(this->read(buffer, 31), std::runtime_error);

    // be careful to reset it, otherwise following tests will suck.
    QueryCancelPending = false;
}
