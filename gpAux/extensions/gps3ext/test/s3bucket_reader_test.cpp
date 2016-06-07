#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "mock_classes.h"
#include "reader_params.h"
#include "s3bucket_reader.cpp"

using ::testing::AtLeast;
using ::testing::Return;
using ::testing::Throw;
using ::testing::_;

class MockS3Reader : public Reader {
   public:
    MOCK_METHOD1(open, void(const ReaderParams& params));
    MOCK_METHOD2(read, uint64_t(char*, uint64_t));
    MOCK_METHOD0(close, void());
};

// ================== S3BucketReaderTest ===================

class S3BucketReaderTest : public testing::Test {
   protected:
    // Remember that SetUp() is run immediately before a test starts.
    virtual void SetUp() {
        bucketReader = new S3BucketReader();
        bucketReader->setS3interface(&s3interface);
    }

    // TearDown() is invoked immediately after a test finishes.
    virtual void TearDown() {
        bucketReader->close();
        delete bucketReader;
    }

    S3BucketReader* bucketReader;
    ReaderParams params;
    char buf[64];

    MockS3Interface s3interface;
    MockS3Reader s3reader;
};

TEST_F(S3BucketReaderTest, OpenInvalidURL) {
    string url = "https://s3-us-east-2.amazon.com/s3test.pivotal.io/whatever";
    params.setUrlToLoad(url);
    EXPECT_THROW(bucketReader->open(params), std::runtime_error);
}

TEST_F(S3BucketReaderTest, OpenURL) {
    ListBucketResult* result = new ListBucketResult();

    EXPECT_CALL(s3interface, listBucket(_, _, _, _, _)).Times(1).WillOnce(Return(result));

    string url = "https://s3-us-east-2.amazonaws.com/s3test.pivotal.io/whatever";
    params.setUrlToLoad(url);

    EXPECT_NO_THROW(bucketReader->open(params));
}

TEST_F(S3BucketReaderTest, ListBucketWithRetryThrowException) {
    EXPECT_THROW(bucketReader->listBucketWithRetry(0), std::runtime_error);
}

TEST_F(S3BucketReaderTest, ListBucketWithRetryThrowExceptionWhenS3InterfaceIsNULL) {
    bucketReader->setS3interface(NULL);
    EXPECT_THROW(bucketReader->listBucketWithRetry(1), std::runtime_error);
}

TEST_F(S3BucketReaderTest, ListBucketWithRetry) {
    ListBucketResult* result = new ListBucketResult();

    EXPECT_CALL(s3interface, listBucket(_, _, _, _, _)).Times(1).WillOnce(Return(result));

    EXPECT_NE((void*)NULL, bucketReader->listBucketWithRetry(1));
}

TEST_F(S3BucketReaderTest, ListBucketWithRetries) {
    ListBucketResult* result = new ListBucketResult();

    EXPECT_CALL(s3interface, listBucket(_, _, _, _, _))
        .Times(3)
        .WillOnce(Return((ListBucketResult*)NULL))
        .WillOnce(Return((ListBucketResult*)NULL))
        .WillOnce(Return(result));

    EXPECT_EQ(result, bucketReader->listBucketWithRetry(3));
}

TEST_F(S3BucketReaderTest, ReaderThrowExceptionWhenUpstreamReaderIsNULL) {
    EXPECT_THROW(bucketReader->read(buf, sizeof(buf)), std::runtime_error);
}

TEST_F(S3BucketReaderTest, ReaderReturnZeroForEmptyBucket) {
    ListBucketResult* result = new ListBucketResult();
    EXPECT_CALL(s3interface, listBucket(_, _, _, _, _)).Times(1).WillOnce(Return(result));

    params.setUrlToLoad("https://s3-us-east-2.amazonaws.com/s3test.pivotal.io/whatever");
    bucketReader->open(params);
    bucketReader->setUpstreamReader(&s3reader);
    EXPECT_EQ(0, bucketReader->read(buf, sizeof(buf)));
}

TEST_F(S3BucketReaderTest, ReadBucketWithSingleFile) {
    ListBucketResult* result = new ListBucketResult();
    BucketContent* item = new BucketContent("foo", 456);
    result->contents.push_back(item);

    EXPECT_CALL(s3interface, listBucket(_, _, _, _, _)).Times(1).WillOnce(Return(result));

    EXPECT_CALL(s3reader, read(_, _))
        .Times(3)
        .WillOnce(Return(256))
        .WillOnce(Return(200))
        .WillOnce(Return(0));

    EXPECT_CALL(s3reader, open(_)).Times(1);
    EXPECT_CALL(s3reader, close()).Times(1);

    params.setSegId(0);
    params.setSegNum(1);
    params.setUrlToLoad("https://s3-us-east-2.amazonaws.com/s3test.pivotal.io/whatever");
    bucketReader->open(params);
    bucketReader->setUpstreamReader(&s3reader);

    EXPECT_EQ(256, bucketReader->read(buf, sizeof(buf)));
    EXPECT_EQ(200, bucketReader->read(buf, sizeof(buf)));
    EXPECT_EQ(0, bucketReader->read(buf, sizeof(buf)));
}

TEST_F(S3BucketReaderTest, ReadBuckeWithOneEmptyFileOneNonEmptyFile) {
    ListBucketResult* result = new ListBucketResult();
    BucketContent* item = new BucketContent("foo", 0);
    result->contents.push_back(item);
    item = new BucketContent("bar", 456);
    result->contents.push_back(item);

    EXPECT_CALL(s3interface, listBucket(_, _, _, _, _)).Times(1).WillOnce(Return(result));

    EXPECT_CALL(s3reader, read(_, _))
        .Times(3)
        .WillOnce(Return(0))
        .WillOnce(Return(256))
        .WillOnce(Return(0));

    EXPECT_CALL(s3reader, open(_)).Times(2);
    EXPECT_CALL(s3reader, close()).Times(2);

    params.setSegId(0);
    params.setSegNum(1);
    params.setUrlToLoad("https://s3-us-east-2.amazonaws.com/s3test.pivotal.io/whatever");
    bucketReader->open(params);
    bucketReader->setUpstreamReader(&s3reader);

    EXPECT_EQ(256, bucketReader->read(buf, sizeof(buf)));
    EXPECT_EQ(0, bucketReader->read(buf, sizeof(buf)));
}

TEST_F(S3BucketReaderTest, ReaderShouldSkipIfFileIsNotForThisSegment) {
    ListBucketResult* result = new ListBucketResult();
    BucketContent* item = new BucketContent("foo", 456);
    result->contents.push_back(item);

    EXPECT_CALL(s3interface, listBucket(_, _, _, _, _)).Times(1).WillOnce(Return(result));

    params.setSegId(10);
    params.setSegNum(16);
    params.setUrlToLoad("https://s3-us-east-2.amazonaws.com/s3test.pivotal.io/whatever");
    bucketReader->open(params);
    bucketReader->setUpstreamReader(&s3reader);

    EXPECT_EQ(0, bucketReader->read(buf, sizeof(buf)));
}

TEST_F(S3BucketReaderTest, UpstreamReaderThrowException) {
    ListBucketResult* result = new ListBucketResult();
    BucketContent* item = new BucketContent("foo", 0);
    result->contents.push_back(item);
    item = new BucketContent("bar", 456);
    result->contents.push_back(item);

    EXPECT_CALL(s3interface, listBucket(_, _, _, _, _)).Times(1).WillOnce(Return(result));

    EXPECT_CALL(s3reader, read(_, _))
        .Times(AtLeast(1))
        .WillRepeatedly(Throw(std::runtime_error("")));

    EXPECT_CALL(s3reader, open(_)).Times(1);
    EXPECT_CALL(s3reader, close()).Times(0);

    params.setSegId(0);
    params.setSegNum(1);
    params.setUrlToLoad("https://s3-us-east-2.amazonaws.com/s3test.pivotal.io/whatever");
    bucketReader->open(params);
    bucketReader->setUpstreamReader(&s3reader);

    EXPECT_THROW(bucketReader->read(buf, sizeof(buf)), std::runtime_error);
    EXPECT_THROW(bucketReader->read(buf, sizeof(buf)), std::runtime_error);
}
