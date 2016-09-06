#include "s3conf.cpp"
#include "gtest/gtest.h"

TEST(Config, NonExistFile) {
    S3Params params;

    EXPECT_THROW(InitConfig(params, "notexist/path/s3test.conf", "default"), std::runtime_error);
}

TEST(Config, Basic) {
    S3Params params;
    InitConfig(params, "data/s3test.conf", "default");

    EXPECT_EQ("secret_test", params.getCred().secret);
    EXPECT_EQ("accessid_test", params.getCred().accessID);
    EXPECT_EQ("ABCDEFGabcdefg", params.getCred().token);

#ifdef S3_STANDALONE
    EXPECT_EQ(0, s3ext_segid);
    EXPECT_EQ(1, s3ext_segnum);
#endif

    EXPECT_EQ((uint64_t)6, params.getNumOfChunks());
    EXPECT_EQ((uint64_t)(64 * 1024 * 1024 + 1), params.getChunkSize());

    EXPECT_EQ(EXT_INFO, s3ext_loglevel);
    EXPECT_EQ(STDERR_LOG, s3ext_logtype);

    EXPECT_EQ(1111, s3ext_logserverport);
    EXPECT_EQ("127.0.0.1", s3ext_logserverhost);

    EXPECT_EQ((uint64_t)1024, params.getLowSpeedLimit());
    EXPECT_EQ((uint64_t)600, params.getLowSpeedTime());

    EXPECT_TRUE(params.isEncryption());
    EXPECT_FALSE(params.isDebugCurl());
}

TEST(Config, SpecialSectionValues) {
    S3Params params;
    InitConfig(params, "data/s3test.conf", "special_over");

    EXPECT_EQ((uint64_t)8, params.getNumOfChunks());
    EXPECT_EQ((uint64_t)(128 * 1024 * 1024), params.getChunkSize());

    EXPECT_EQ((uint64_t)10240, params.getLowSpeedLimit());
    EXPECT_EQ((uint64_t)60, params.getLowSpeedTime());

    EXPECT_TRUE(params.isEncryption());
    EXPECT_FALSE(params.isDebugCurl());
}

TEST(Config, SpecialSectionLowValues) {
    S3Params params;
    InitConfig(params, "data/s3test.conf", "special_low");

    EXPECT_EQ((uint64_t)1, params.getNumOfChunks());
    EXPECT_EQ((uint64_t)(8 * 1024 * 1024), params.getChunkSize());
}

TEST(Config, SpecialSectionWrongKeyName) {
    S3Params params;
    InitConfig(params, "data/s3test.conf", "special_wrongkeyname");

    EXPECT_EQ((uint64_t)4, params.getNumOfChunks());
    EXPECT_EQ((uint64_t)(64 * 1024 * 1024), params.getChunkSize());
}

TEST(Config, SpecialSwitches) {
    S3Params params;
    InitConfig(params, "data/s3test.conf", "special_switches");

    EXPECT_FALSE(params.isEncryption());
    EXPECT_TRUE(params.isDebugCurl());
}

TEST(Config, SectionExist) {
    Config s3cfg("data/s3test.conf");
    EXPECT_TRUE(s3cfg.SectionExist("special_switches"));
}

TEST(Config, SectionNotExist) {
    Config s3cfg("data/s3test.conf");
    EXPECT_FALSE(s3cfg.SectionExist("not_exist"));
}
