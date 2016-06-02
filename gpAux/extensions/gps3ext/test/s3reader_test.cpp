#include "s3reader.cpp"
#include "gtest/gtest.h"

volatile bool QueryCancelPending = false;

TEST(OffsetMgr, simple) {
    OffsetMgr *o = new OffsetMgr(4096, 1000);
    Range r = o->NextOffset();
    EXPECT_EQ(r.offset, 0);
    EXPECT_EQ(r.len, 1000);

    r = o->NextOffset();
    EXPECT_EQ(r.offset, 1000);
    EXPECT_EQ(r.len, 1000);

    r = o->NextOffset();
    EXPECT_EQ(r.offset, 2000);
    EXPECT_EQ(r.len, 1000);

    r = o->NextOffset();
    EXPECT_EQ(r.offset, 3000);
    EXPECT_EQ(r.len, 1000);

    r = o->NextOffset();
    EXPECT_EQ(r.offset, 4000);
    EXPECT_EQ(r.len, 96);
    delete o;
}

TEST(OffsetMgr, reset) {
    OffsetMgr *o = new OffsetMgr(1024, 100);

    o->NextOffset();
    o->NextOffset();
    o->Reset(333);
    Range r = o->NextOffset();

    EXPECT_EQ(r.offset, 333);
    EXPECT_EQ(r.len, 100);
    delete o;
}