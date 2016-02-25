#include "gtest/gtest.h"
#include "S3Uploader.cpp"

#define S3HOST "s3-us-west-2.amazonaws.com"
#define S3BUCKET "metro.pivotal.io"

#define KEYID "AKIAIAFSMJUMQWXB2PUQ"
#define SECRET "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"

TEST(Uploader, get_upload_id) {
    S3Credential g_cred = {KEYID, SECRET};
    const char *upload_id = GetUploadId(S3HOST, S3BUCKET, "data1234", g_cred);

    // printf("uploadid = %s\n", upload_id);

    EXPECT_NE(upload_id, (void *)NULL);
}

TEST(Uploader, get_upload_id_directories) {
    S3Credential g_cred = {KEYID, SECRET};
    const char *upload_id =
        GetUploadId(S3HOST, S3BUCKET, "test/upload/data1234", g_cred);

    // printf("uploadid = %s\n", upload_id);

    EXPECT_NE(upload_id, (void *)NULL);
}

TEST(Uploader, get_upload_id_long_url) {
    S3Credential g_cred = {KEYID, SECRET};
    const char *upload_id =
        GetUploadId(S3HOST, S3BUCKET,
                    "test/upload/"
                    "data1234aaaaaaaaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbcccccc"
                    "ccccccccccccccccddddddddddddddddddddddddeeeeeeeeeeeeeeeeee"
                    "effffffffffffffffgggggggggggggggggggg",
                    g_cred);

    // printf("uploadid = %s\n", upload_id);

    EXPECT_NE(upload_id, (void *)NULL);
}

TEST(Uploader, get_upload_id_long_url_directories) {
    S3Credential g_cred = {KEYID, SECRET};
    const char *upload_id = GetUploadId(
        S3HOST, S3BUCKET,
        "test/upload/data1234/aaaaaaaaaaaaaaaaaaaaaaaaa/"
        "bbbbbbbbbbbbbbbbbbbccccccccccccccccccccccddddddddddddddddddddddddeeeee"
        "eeeeeeeeeeeeeeffffffffffffffffggggggggggggggggggg/g",
        g_cred);

    // printf("uploadid = %s\n", upload_id);

    EXPECT_NE(upload_id, (void *)NULL);
}

/*
#define OBJ2UPLOAD "itfinallyworks"
TEST(Uploader, upload) {
    S3Credential g_cred = {KEYID, SECRET};

    printf("init, getting uploadid\n");
    const char *upload_id = GetUploadId(S3HOST, S3BUCKET, OBJ2UPLOAD, g_cred);

    printf("upload part1, getting etag1\n");
    char *upload_buf = (char *)malloc(5 * 1024 * 1024 + 1);
    memset(upload_buf, 42, 5 * 1024 * 1024 + 1);
    const char *etag_1 =
        PartPutS3Object(S3HOST, S3BUCKET, OBJ2UPLOAD, g_cred, upload_buf,
                        5 * 1024 * 1024 + 1, 1, upload_id);

    printf("upload part2, getting etag2\n");
    char *upload_buf_2 = (char *)malloc(1 * 1024 * 1024 + 1);
    memset(upload_buf_2, 70, 1 * 1024 * 1024 + 1);
    const char *etag_2 =
        PartPutS3Object(S3HOST, S3BUCKET, OBJ2UPLOAD, g_cred, upload_buf_2,
                        1 * 1024 * 1024 + 1, 2, upload_id);

    const char *etag_array[2] = {etag_1, etag_2};
    printf("completing multi upload\n");
    bool ret = CompleteMultiPutS3(S3HOST, S3BUCKET, OBJ2UPLOAD, upload_id,
                                  etag_array, 2, g_cred);

    EXPECT_TRUE(ret);
}

// need to convert url string to Percent-encoding
// https://en.wikipedia.org/wiki/Percent-encoding
TEST(Uploader, get_upload_id_spaces) {
    S3Credential g_cred = {KEYID, SECRET};
    const char *upload_id = GetUploadId(S3HOST, S3BUCKET, "data 1234", g_cred);

    // printf("uploadid = %s\n", upload_id);

    EXPECT_NE(upload_id, (void *)NULL);
}

TEST(Uploader, get_upload_id_spaces_directories) {
    S3Credential g_cred = {KEYID, SECRET};
    const char *upload_id =
        GetUploadId(S3HOST, S3BUCKET, "test/ up load/data 1234", g_cred);

    // printf("uploadid = %s\n", upload_id);

    EXPECT_NE(upload_id, (void *)NULL);
}
*/
