#include "gpreader.cpp"
#include "gtest/gtest.h"
#include "s3extbase.cpp"

TEST(ExtBase, ValidateURL_normal) {
    S3ExtBase *myData;
    myData = new S3Reader("s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/normal");

    ASSERT_TRUE(myData->ValidateURL());
    EXPECT_STREQ("us-west-2", myData->get_region().c_str());
    EXPECT_STREQ("s3test.pivotal.io", myData->get_bucket().c_str());
    EXPECT_EQ("dataset1/normal", myData->get_prefix());

    delete myData;
}

TEST(ExtBase, ValidateURL_NoPrefixAndSlash) {
    S3ExtBase *myData;
    myData = new S3Reader("s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io");

    ASSERT_TRUE(myData->ValidateURL());
    EXPECT_STREQ("s3test.pivotal.io", myData->get_bucket().c_str());
    EXPECT_STREQ("", myData->get_prefix().c_str());

    delete myData;
}

TEST(ExtBase, ValidateURL_NoPrefix) {
    S3ExtBase *myData;
    myData = new S3Reader("s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/");

    ASSERT_TRUE(myData->ValidateURL());
    EXPECT_STREQ("s3test.pivotal.io", myData->get_bucket().c_str());
    EXPECT_STREQ("", myData->get_prefix().c_str());

    delete myData;
}

TEST(ExtBase, ValidateURL_default) {
    S3ExtBase *myData;
    myData = new S3Reader("s3://s3.amazonaws.com/s3test.pivotal.io/dataset1/normal");

    ASSERT_TRUE(myData->ValidateURL());
    EXPECT_STREQ("external-1", myData->get_region().c_str());
    EXPECT_STREQ("s3test.pivotal.io", myData->get_bucket().c_str());
    EXPECT_STREQ("dataset1/normal", myData->get_prefix().c_str());

    delete myData;
}

TEST(ExtBase, ValidateURL_useast1) {
    S3ExtBase *myData;
    myData = new S3Reader("s3://s3-us-east-1.amazonaws.com/s3test.pivotal.io/dataset1/normal");

    ASSERT_TRUE(myData->ValidateURL());
    EXPECT_STREQ("external-1", myData->get_region().c_str());
    EXPECT_STREQ("s3test.pivotal.io", myData->get_bucket().c_str());
    EXPECT_STREQ("dataset1/normal", myData->get_prefix().c_str());

    delete myData;
}

TEST(ExtBase, ValidateURL_eucentral1) {
    S3ExtBase *myData;
    myData = new S3Reader("s3://s3.eu-central-1.amazonaws.com/s3test.pivotal.io/dataset1/normal");

    ASSERT_TRUE(myData->ValidateURL());
    EXPECT_STREQ("eu-central-1", myData->get_region().c_str());
    EXPECT_STREQ("s3test.pivotal.io", myData->get_bucket().c_str());
    EXPECT_STREQ("dataset1/normal", myData->get_prefix().c_str());

    delete myData;
}

TEST(ExtBase, ValidateURL_eucentral11) {
    S3ExtBase *myData;
    myData = new S3Reader("s3://s3-eu-central-1.amazonaws.com/s3test.pivotal.io/dataset1/normal");

    ASSERT_TRUE(myData->ValidateURL());
    EXPECT_STREQ("eu-central-1", myData->get_region().c_str());
    EXPECT_STREQ("s3test.pivotal.io", myData->get_bucket().c_str());
    EXPECT_STREQ("dataset1/normal", myData->get_prefix().c_str());

    delete myData;
}

TEST(ExtBase, ValidateURL_apnortheast2) {
    S3ExtBase *myData;
    myData = new S3Reader(
        "s3://s3.ap-northeast-2.amazonaws.com/s3test.pivotal.io/dataset1/"
        "normal");

    ASSERT_TRUE(myData->ValidateURL());
    EXPECT_STREQ("ap-northeast-2", myData->get_region().c_str());
    EXPECT_STREQ("s3test.pivotal.io", myData->get_bucket().c_str());
    EXPECT_STREQ("dataset1/normal", myData->get_prefix().c_str());

    delete myData;
}

TEST(ExtBase, ValidateURL_apnortheast21) {
    S3ExtBase *myData;
    myData = new S3Reader(
        "s3://s3-ap-northeast-2.amazonaws.com/s3test.pivotal.io/dataset1/"
        "normal");

    ASSERT_TRUE(myData->ValidateURL());
    EXPECT_STREQ("ap-northeast-2", myData->get_region().c_str());
    EXPECT_STREQ("s3test.pivotal.io", myData->get_bucket().c_str());
    EXPECT_STREQ("dataset1/normal", myData->get_prefix().c_str());

    delete myData;
}

TEST(Common, ThreadFunctions) {
    // just to test if these two are functional
    thread_setup();
    EXPECT_NE((void *)NULL, mutex_buf);

    thread_cleanup();
    EXPECT_EQ((void *)NULL, mutex_buf);

    EXPECT_NE(0, id_function());
}
