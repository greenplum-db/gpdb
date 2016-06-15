#include "s3conf.cpp"
#include "gtest/gtest.h"
#include "ini.cpp"

TEST(Config, NonExistFile) {
    bool ret = InitConfig("notexist/path/s3test.conf");

    EXPECT_FALSE(ret);
}

TEST(Config, Basic) {
    InitConfig("test/data/s3test.conf", "default");

    EXPECT_STREQ("secret_test", s3ext_secret.c_str());
    EXPECT_STREQ("accessid_test", s3ext_accessid.c_str());
    EXPECT_STREQ("ABCDEFGabcdefg", s3ext_token.c_str());

#ifdef S3_STANDALONE
    EXPECT_EQ(0, s3ext_segid);
    EXPECT_EQ(1, s3ext_segnum);
#endif

    EXPECT_EQ(6, s3ext_threadnum);
    EXPECT_EQ(64 * 1024 * 1024 + 1, s3ext_chunksize);

    EXPECT_EQ(EXT_DEBUG, s3ext_loglevel);
    EXPECT_EQ(STDERR_LOG, s3ext_logtype);

    EXPECT_EQ(1111, s3ext_logserverport);
    EXPECT_STREQ("127.0.0.1", s3ext_logserverhost.c_str());

    EXPECT_EQ(1024, s3ext_low_speed_limit);
    EXPECT_EQ(600, s3ext_low_speed_time);

    EXPECT_FALSE(s3ext_encryption);
    EXPECT_TRUE(s3ext_debug_curl);
}

TEST(Config, SpecialSectionValues) {
    InitConfig("test/data/s3test.conf", "special_over");

    EXPECT_EQ(8, s3ext_threadnum);
    EXPECT_EQ(128 * 1024 * 1024, s3ext_chunksize);
    EXPECT_EQ(10240, s3ext_low_speed_limit);
    EXPECT_EQ(60, s3ext_low_speed_time);

    EXPECT_TRUE(s3ext_encryption);
    EXPECT_FALSE(s3ext_debug_curl);
}

TEST(Config, SpecialSectionLowValues) {
    InitConfig("test/data/s3test.conf", "special_low");

    EXPECT_EQ(1, s3ext_threadnum);
    EXPECT_EQ(2 * 1024 * 1024, s3ext_chunksize);
}

TEST(Config, SpecialSectionWrongKeyName) {
    InitConfig("test/data/s3test.conf", "special_wrongkeyname");

    EXPECT_EQ(4, s3ext_threadnum);
    EXPECT_EQ(64 * 1024 * 1024, s3ext_chunksize);
}
