#include "gtest/gtest.h"
#include "S3Downloader.cpp"

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

#define HOSTSTR "localhost"
#define BUCKETSTR "metro.pivotal.io"
TEST(ListBucket, fake) {
    ListBucketResult *r = ListBucket_FakeHTTP(HOSTSTR, BUCKETSTR);

    ASSERT_NE(r, (void *)NULL);
    EXPECT_EQ(r->contents.size(), 16);

    char urlbuf[256];
    vector<BucketContent *>::iterator i;
    for (i = r->contents.begin(); i != r->contents.end(); i++) {
        BucketContent *p = *i;
        sprintf(urlbuf, "http://%s/%s/%s", HOSTSTR, BUCKETSTR,
                p->Key().c_str());
        printf("%s, %llu\n", urlbuf, p->Size());
    }
    delete r;
}

#define S3HOST "s3-us-west-2.amazonaws.com"
#define S3BUCKET "s3test.pivotal.io"
#define S3PREFIX "dataset1/small17"

#ifdef AWSTEST

TEST(ListBucket, s3) {
    InitConfig("test/s3.conf", "default");

    S3Credential g_cred = {s3ext_accessid, s3ext_secret};

    ListBucketResult *r =
        ListBucket("https", S3HOST, S3BUCKET, S3PREFIX, g_cred);

    ASSERT_NE(r, (void *)NULL);
    EXPECT_EQ(r->contents.size(), 16);

    char urlbuf[256];
    vector<BucketContent *>::iterator i;
    for (i = r->contents.begin(); i != r->contents.end(); i++) {
        BucketContent *p = *i;
        sprintf(urlbuf, "http://%s/%s/%s", S3HOST, S3BUCKET, p->Key().c_str());
        printf("%s, %d\n", urlbuf, p->Size());
    }

    delete r;
}

#endif  // AWSTEST

void DownloadTest(const char *url, uint64_t file_size, const char *md5_str,
                  uint8_t thread_num, uint64_t chunk_size, uint64_t buffer_size,
                  bool use_credential) {
    InitConfig("test/s3.conf", "default");

    S3Credential g_cred = {s3ext_accessid, s3ext_secret};

    uint64_t buf_len = buffer_size;
    char *buf = (char *)malloc(buffer_size);
    Downloader *d = new Downloader(thread_num);
    MD5Calc m;
    bool result = false;

    ASSERT_NE((void *)NULL, buf);

    if (use_credential) {
        result = d->init(url, file_size, chunk_size, &g_cred);
    } else {
        result = d->init(url, file_size, chunk_size, NULL);
    }

    ASSERT_TRUE(result);

    while (1) {
        ASSERT_TRUE(d->get(buf, buf_len));
        if (buf_len == 0) {
            break;
        }
        m.Update(buf, buf_len);
        buf_len = buffer_size;
    }

    ASSERT_STREQ(md5_str, m.Get());
    delete d;
    free(buf);
}

void HTTPDownloaderTest(const char *url, uint64_t file_size,
                        const char *md5_str, uint8_t thread_num,
                        uint64_t chunk_size, uint64_t buffer_size) {
    return DownloadTest(url, file_size, md5_str, thread_num, chunk_size,
                        buffer_size, false);
}

void S3DwonloadTest(const char *url, uint64_t file_size, const char *md5_str,
                    uint8_t thread_num, uint64_t chunk_size,
                    uint64_t buffer_size) {
    InitConfig("test/s3.conf", "default");

    return DownloadTest(url, file_size, md5_str, thread_num, chunk_size,
                        buffer_size, true);
}

TEST(HTTPDownloader, divisible) {
    HTTPDownloaderTest("http://localhost/testdata/8M", 8388608,
                       "22129b81bf8f96a06a8b7f3d2a683588", 4, 4 * 1024,
                       16 * 1024);
}

TEST(HTTPDownloader, equal) {
    HTTPDownloaderTest("http://localhost/testdata/8M", 8388608,
                       "22129b81bf8f96a06a8b7f3d2a683588", 4, 16 * 1024,
                       16 * 1024);
}

TEST(HTTPDownloader, one_byte) {
    HTTPDownloaderTest("http://localhost/testdata/8K", 8192,
                       "f92ad4ae5f16a8d04f87e801af5115dc", 4, 4 * 1024, 1);
}

TEST(HTTPDownloader, over_flow) {
    HTTPDownloaderTest("http://localhost/testdata/8M", 8388608,
                       "22129b81bf8f96a06a8b7f3d2a683588", 4, 7 * 1024,
                       15 * 1024);
}

TEST(HTTPDownloader, multi_thread) {
    HTTPDownloaderTest("http://localhost/testdata/1M", 1048576,
                       "06427456b575a3880936b4ae43448082", 64, 4 * 1024,
                       511 * 1023);
}

TEST(HTTPDownloader, single_thread) {
    HTTPDownloaderTest("http://localhost/testdata/1M", 1048576,
                       "06427456b575a3880936b4ae43448082", 1, 4 * 1024,
                       511 * 1023);
}

TEST(HTTPDownloader, random_parameters_1M) {
    HTTPDownloaderTest("http://localhost/testdata/1M", 1048576,
                       "06427456b575a3880936b4ae43448082", 3, 3 * 29,
                       571 * 1023);
}

TEST(HTTPDownloader, random_parameters_1G) {
    HTTPDownloaderTest("http://localhost/testdata/1G", 1073741824,
                       "a2cb2399eb8bc97084aed673e5d09f4d", 9, 42 * 1023,
                       513 * 1025 * 37);
}

TEST(HTTPDownloader, random_parameters_8M) {
    HTTPDownloaderTest("http://localhost/testdata/8M", 8388608,
                       "22129b81bf8f96a06a8b7f3d2a683588", 77, 7 * 10211,
                       777 * 1025);
}

TEST(HTTPDownloader, random_parameters_64M) {
    HTTPDownloaderTest("http://localhost/testdata/64M", 67108864,
                       "0871a7464a7ff85564f4f95b9ac77321", 51, 7 * 1053,
                       77 * 87);
}

TEST(HTTPDownloader, random_parameters_256M) {
    HTTPDownloaderTest("http://localhost/testdata/256M", 268435456,
                       "9cb7c287fdd5a44378798d3e75d2d2a6", 3, 1 * 10523,
                       77 * 879);
}

#ifdef AWSTEST
TEST(S3Downloader, simple) {
    printf("Try downloading data0014\n");
    S3DwonloadTest(
        "http://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/small17/"
        "data0014",
        4420346, "68c4a63b721e7af0ae945ce109ca87ad", 4, 1024 * 1024, 65536);

    printf("Try downloading data0016\n");
    S3DwonloadTest(
        "http://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/small17/"
        "data0016",
        2536018, "0fd502a303eb8f138f5916ec357721b1", 4, 1024 * 1024, 65536);
}

TEST(S3Downloader, httpssimple) {
    printf("Try downloading data0014\n");
    S3DwonloadTest(
        "http://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/small17/"
        "data0014",
        4420346, "68c4a63b721e7af0ae945ce109ca87ad", 4, 1024 * 1024, 65536);

    printf("Try downloading data0016\n");
    S3DwonloadTest(
        "http://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/small17/"
        "data0016",
        2536018, "0fd502a303eb8f138f5916ec357721b1", 4, 1024 * 1024, 65536);
}

TEST(S3Downloader, bigfile) {
    // InitLog();
    // s3ext_loglevel = EXT_DEBUG;
    // s3ext_logtype = STDERR_LOG;
    printf("Try downloading big file 1\n");
    S3DwonloadTest(
        "http://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset2/hugefile/"
        "airlinedata1.csv",
        4664425994, "f5811ad92c994f1d6913d5338575fe38", 4, 64 * 1024 * 1024,
        65536);
}
#endif  // AWSTEST
