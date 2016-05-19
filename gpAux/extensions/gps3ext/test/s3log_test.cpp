#include "s3log.cpp"
#include "gtest/gtest.h"

TEST(Logger, simple) {
    InitConfig("test/data/s3test.conf", "");
    InitLog();
    fprintf(stderr, "Hello, log type %d\n", s3ext_logtype);
    S3DEBUG("Hello, DEBUG");
    S3ERROR("Hello, ERROR");
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
    EXPECT_EQ(LOCAL_LOG, getLogType("LOCAL"));
    EXPECT_EQ(INTERNAL_LOG, getLogType("INTERNAL"));
    EXPECT_EQ(STDERR_LOG, getLogType(""));
    EXPECT_EQ(STDERR_LOG, getLogType(NULL));
}
