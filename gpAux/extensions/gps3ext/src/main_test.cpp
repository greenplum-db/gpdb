#include "gtest/gtest.h"
#include "gps3conf.h"

int main(int argc, char **argv) {
    InitConfig("test/s3.conf", "default");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
