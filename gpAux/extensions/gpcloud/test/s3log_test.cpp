#include "s3log.cpp"
#include "gtest/gtest.h"

TEST(Logger, simple) {
    S3Params params = InitConfig("s3://abc/a config=data/s3test.conf section=default");

    InitRemoteLog();

    s3ext_logtype = STDERR_LOG;
    S3DEBUG("Hello, STDERR DEBUG");
    S3ERROR("Hello, STDERR ERROR");

    s3ext_logtype = INTERNAL_LOG;
    S3DEBUG("Hello, INTERNAL DEBUG");
    S3ERROR("Hello, INTERNAL ERROR");

    s3ext_logtype = REMOTE_LOG;
    S3DEBUG("Hello, REMOTE DEBUG");
    S3ERROR("Hello, REMOTE ERROR");
}

TEST(Logger, getstr) {
    EXPECT_EQ(EXT_DEBUG, getLogLevel("DEBUG"));
    EXPECT_EQ(EXT_WARNING, getLogLevel("WARNING"));
    EXPECT_EQ(EXT_INFO, getLogLevel("INFO"));
    EXPECT_EQ(EXT_ERROR, getLogLevel("ERROR"));
    EXPECT_EQ(EXT_FATAL, getLogLevel("FATAL"));
    EXPECT_EQ(EXT_FATAL, getLogLevel("XX"));
    EXPECT_EQ(EXT_FATAL, getLogLevel(NULL));

    EXPECT_EQ(STDERR_LOG, getLogType("STDERR"));
    EXPECT_EQ(REMOTE_LOG, getLogType("REMOTE"));
    EXPECT_EQ(INTERNAL_LOG, getLogType("INTERNAL"));
    EXPECT_EQ(STDERR_LOG, getLogType(""));
    EXPECT_EQ(STDERR_LOG, getLogType(NULL));
}

TEST(Logger, getstrCaseInsensitive) {
    EXPECT_EQ(EXT_DEBUG, getLogLevel("DeBug"));
    EXPECT_EQ(EXT_ERROR, getLogLevel("error"));

    EXPECT_EQ(INTERNAL_LOG, getLogType("Internal"));
    EXPECT_EQ(REMOTE_LOG, getLogType("reMOTE"));
}
