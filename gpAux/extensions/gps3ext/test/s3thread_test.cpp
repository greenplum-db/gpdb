#include "s3thread.cpp"
#include "gtest/gtest.h"

TEST(Common, ThreadFunctions) {
    // just to test if these two are functional
    thread_setup();
    EXPECT_NE((void *)NULL, mutex_buf);

    thread_cleanup();
    EXPECT_EQ((void *)NULL, mutex_buf);

    EXPECT_NE(0, id_function());
}
