#include "gtest/gtest.h"
#include "ini.cpp"
#include "gps3conf.cpp"
#include <cstdlib>

TEST(Config, basic) {
    InitConfig("test/s3test.conf", "");

    EXPECT_STREQ("secret_test", s3ext_secret.c_str());
    EXPECT_STREQ("accessid_test", s3ext_accessid.c_str());
    EXPECT_STREQ("ABCDEFGabcdefg", s3ext_token.c_str());

#ifdef DEBUGS3
    EXPECT_EQ(0, s3ext_segid);
    EXPECT_EQ(1, s3ext_segnum);
#endif

    EXPECT_EQ(6, s3ext_threadnum);
    EXPECT_EQ(64 * 1024 * 1024 + 1, s3ext_chunksize);

    EXPECT_EQ(EXT_DEBUG, s3ext_loglevel);
    EXPECT_EQ(STDERR_LOG, s3ext_logtype);

    EXPECT_EQ(1111, s3ext_logserverport);
    EXPECT_STREQ("127.0.0.1", s3ext_logserverhost.c_str());
    EXPECT_STREQ("'/tmp/abcde'", s3ext_logpath.c_str());
}
