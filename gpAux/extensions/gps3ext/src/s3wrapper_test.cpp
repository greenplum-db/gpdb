#include "s3wrapper.cpp"
#include "gtest/gtest.h"

class S3Reader_fake : public S3Reader {
   public:
    S3Reader_fake(string url);
    virtual ~S3Reader_fake();
    virtual bool Init(int segid, int segnum, int chunksize);
    virtual bool Destroy();
    virtual bool ValidateURL();

   protected:
    virtual string getKeyURL(const string key);
};

S3Reader_fake::S3Reader_fake(string url) : S3Reader(url) {}

S3Reader_fake::~S3Reader_fake() {}

bool S3Reader_fake::Destroy() {
    if (this->filedownloader) {
        this->filedownloader->destroy();
        delete this->filedownloader;
    }

    if (this->keylist) {
        delete this->keylist;
    }
    return true;
}

bool S3Reader_fake::Init(int segid, int segnum, int chunksize) {
    // set segment id and num
    this->segid = segid;    // fake
    this->segnum = segnum;  // fake
    this->contentindex = this->segid;

    this->chunksize = chunksize;

    // Validate url first
    if (!this->ValidateURL()) {
        S3ERROR("validate url fail %s\n", this->url.c_str());
    }

    this->keylist = ListBucket_FakeHTTP("localhost", this->bucket.c_str());

    if (!this->keylist) {
        return false;
    }

    this->getNextDownloader();
    return this->filedownloader ? true : false;
}

string S3Reader_fake::getKeyURL(const string key) {
    stringstream sstr;
    sstr << this->schema << "://"
         << "localhost/";
    sstr << this->bucket << "/" << key;
    return sstr.str();
}

bool S3Reader_fake::ValidateURL() {
    this->schema = "http";
    this->region = "raycom";
    int ibegin, iend;
    ibegin = find_Nth(this->url, 3, "/");
    iend = find_Nth(this->url, 4, "/");

    if ((iend == string::npos) || (ibegin == string::npos)) {
        return false;
    }
    this->bucket = url.substr(ibegin + 1, iend - ibegin - 1);

    this->prefix = "";
    return true;
}

void ExtWrapperTest(const char *url, uint64_t buffer_size, const char *md5_str,
                    int segid, int segnum, uint64_t chunksize) {
    InitConfig("test/s3.conf", "default");

    s3ext_segid = segid;
    s3ext_segnum = segnum;

    MD5Calc m;
    S3ExtBase *myData;
    uint64_t nread = 0;
    uint64_t buf_len = buffer_size;
    char *buf = (char *)malloc(buffer_size);

    ASSERT_NE((void *)NULL, buf);

    if (strncmp(url, "http://localhost/", 17) == 0) {
        myData = new S3Reader_fake(url);
    } else {
        myData = new S3Reader(url);
    }

    ASSERT_NE((void *)NULL, myData);
    ASSERT_TRUE(myData->Init(segid, segnum, chunksize));

    while (1) {
        nread = buf_len;
        ASSERT_TRUE(myData->TransferData(buf, nread));
        if (nread == 0) break;
        m.Update(buf, nread);
    }

    EXPECT_STREQ(md5_str, m.Get());
    myData->Destroy();
    delete myData;
    free(buf);
}

TEST(ExtWrapper, ValidateURL_normal) {
    S3ExtBase *myData;
    myData = new S3Reader(
        "s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/normal");

    ASSERT_TRUE(myData->ValidateURL());
    EXPECT_STREQ("us-west-2", myData->get_region().c_str());

    delete myData;
}

TEST(ExtWrapper, ValidateURL_default) {
    S3ExtBase *myData;
    myData =
        new S3Reader("s3://s3.amazonaws.com/s3test.pivotal.io/dataset1/normal");

    ASSERT_TRUE(myData->ValidateURL());
    EXPECT_STREQ("external-1", myData->get_region().c_str());

    delete myData;
}

TEST(ExtWrapper, ValidateURL_useast1) {
    S3ExtBase *myData;
    myData = new S3Reader(
        "s3://s3-us-east-1.amazonaws.com/s3test.pivotal.io/dataset1/normal");

    ASSERT_TRUE(myData->ValidateURL());
    EXPECT_STREQ("external-1", myData->get_region().c_str());

    delete myData;
}

TEST(ExtWrapper, ValidateURL_eucentral1) {
    S3ExtBase *myData;
    myData = new S3Reader(
        "s3://s3.eu-central-1.amazonaws.com/s3test.pivotal.io/dataset1/normal");

    ASSERT_TRUE(myData->ValidateURL());
    EXPECT_STREQ("eu-central-1", myData->get_region().c_str());

    delete myData;
}

TEST(ExtWrapper, ValidateURL_eucentral11) {
    S3ExtBase *myData;
    myData = new S3Reader(
        "s3://s3-eu-central-1.amazonaws.com/s3test.pivotal.io/dataset1/normal");

    ASSERT_TRUE(myData->ValidateURL());
    EXPECT_STREQ("eu-central-1", myData->get_region().c_str());

    delete myData;
}

TEST(ExtWrapper, ValidateURL_apnortheast2) {
    S3ExtBase *myData;
    myData = new S3Reader(
        "s3://s3.ap-northeast-2.amazonaws.com/s3test.pivotal.io/dataset1/"
        "normal");

    ASSERT_TRUE(myData->ValidateURL());
    EXPECT_STREQ("ap-northeast-2", myData->get_region().c_str());

    delete myData;
}

TEST(ExtWrapper, ValidateURL_apnortheast21) {
    S3ExtBase *myData;
    myData = new S3Reader(
        "s3://s3-ap-northeast-2.amazonaws.com/s3test.pivotal.io/dataset1/"
        "normal");

    ASSERT_TRUE(myData->ValidateURL());
    EXPECT_STREQ("ap-northeast-2", myData->get_region().c_str());

    delete myData;
}

#ifdef AWS_TEST

TEST(ExtWrapper, normal_region_default) {
    ExtWrapperTest(
        "https://s3.amazonaws.com/useast1.s3test.pivotal.io/small17/",
        64 * 1024, "138fc555074671912125ba692c678246", 0, 1, 64 * 1024 * 1024);
}

TEST(ExtWrapper, normal_region_useast1) {
    ExtWrapperTest(
        "https://s3-us-east-1.amazonaws.com/useast1.s3test.pivotal.io/small17/",
        64 * 1024, "138fc555074671912125ba692c678246", 0, 1, 64 * 1024 * 1024);
}

TEST(ExtWrapper, normal) {
    ExtWrapperTest(
        "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/"
        "small17/",
        64 * 1024, "138fc555074671912125ba692c678246", 0, 1, 64 * 1024 * 1024);
}

TEST(ExtWrapper, normal_2segs) {
    ExtWrapperTest(
        "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/"
        "small17/",
        64 * 1024, "a861acda78891b48b25a2788e028a740", 0, 2, 64 * 1024 * 1024);
    ExtWrapperTest(
        "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/"
        "small17/",
        64 * 1024, "db05de0ec7e0808268e2363d3572dc7f", 1, 2, 64 * 1024 * 1024);
}

#ifdef BIG_FILE_TEST
TEST(ExtWrapper, normal_3segs) {
    ExtWrapperTest(
        "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/"
        "small17/",
        64 * 1024, "4d9ccad20bca50d2d1bc9c4eb4958e2c", 0, 3, 64 * 1024 * 1024);
    ExtWrapperTest(
        "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/"
        "small17/",
        64 * 1024, "561597859d093905e2b21e896259ae79", 1, 3, 64 * 1024 * 1024);
    ExtWrapperTest(
        "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/"
        "small17/",
        64 * 1024, "98d4e5348356ceee46d15c4e5f37845b", 2, 3, 64 * 1024 * 1024);
}

TEST(ExtWrapper, normal_4segs) {
    ExtWrapperTest(
        "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/"
        "small17/",
        64 * 1024, "b87b5d79e2bcb8dc1d0fd289fbfa5829", 0, 4, 64 * 1024 * 1024);
    ExtWrapperTest(
        "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/"
        "small17/",
        64 * 1024, "4df154611d394c60084bb5b97bdb19be", 1, 4, 64 * 1024 * 1024);
    ExtWrapperTest(
        "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/"
        "small17/",
        64 * 1024, "238affbb831ff27df9d09afeeb2e59f9", 2, 4, 64 * 1024 * 1024);
    ExtWrapperTest(
        "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/"
        "small17/",
        64 * 1024, "dceb001d03d54d61874d27c9f04596b1", 3, 4, 64 * 1024 * 1024);
}

TEST(ExtWrapper, huge_1seg) {
    ExtWrapperTest(
        "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset2/"
        "hugefile/",
        64 * 1024, "75baaa39f2b1544ed8af437c2cad86b7", 0, 1, 64 * 1024 * 1024);
}

TEST(ExtWrapper, normal2_3segs) {
    ExtWrapperTest(
        "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset2/normal/",
        64 * 1024, "1c1b198b246160733f7a3491bff5cd52", 0, 3, 64 * 1024 * 1024);
    ExtWrapperTest(
        "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset2/normal/",
        64 * 1024, "296856eb9739d3022b3e9d8bf3b1ea2e", 1, 3, 64 * 1024 * 1024);
    ExtWrapperTest(
        "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset2/normal/",
        64 * 1024, "00675684b6d6697571f22baaf407c6df", 2, 3, 64 * 1024 * 1024);
}
#endif

#endif  // AWS_TEST

#ifdef FAKE_TEST

TEST(FakeExtWrapper, simple) {
    ExtWrapperTest("http://localhost/metro.pivotal.io/", 64 * 1024,
                   "138fc555074671912125ba692c678246", 0, 1, 64 * 1024 * 1024);
}

TEST(FakeExtWrapper, normal_2segs) {
    ExtWrapperTest("http://localhost/metro.pivotal.io/", 64 * 1024,
                   "a861acda78891b48b25a2788e028a740", 0, 2, 64 * 1024 * 1024);
    ExtWrapperTest("http://localhost/metro.pivotal.io/", 64 * 1024,
                   "db05de0ec7e0808268e2363d3572dc7f", 1, 2, 64 * 1024 * 1024);
}

TEST(FakeExtWrapper, normal_3segs) {
    ExtWrapperTest("http://localhost/metro.pivotal.io/", 64 * 1024,
                   "4d9ccad20bca50d2d1bc9c4eb4958e2c", 0, 3, 64 * 1024 * 1024);
    ExtWrapperTest("http://localhost/metro.pivotal.io/", 64 * 1024,
                   "561597859d093905e2b21e896259ae79", 1, 3, 64 * 1024 * 1024);
    ExtWrapperTest("http://localhost/metro.pivotal.io/", 64 * 1024,
                   "98d4e5348356ceee46d15c4e5f37845b", 2, 3, 64 * 1024 * 1024);
}

TEST(FakeExtWrapper, normal_4segs) {
    ExtWrapperTest("http://localhost/metro.pivotal.io/", 64 * 1024,
                   "b87b5d79e2bcb8dc1d0fd289fbfa5829", 0, 4, 64 * 1024 * 1024);
    ExtWrapperTest("http://localhost/metro.pivotal.io/", 64 * 1024,
                   "4df154611d394c60084bb5b97bdb19be", 1, 4, 64 * 1024 * 1024);
    ExtWrapperTest("http://localhost/metro.pivotal.io/", 64 * 1024,
                   "238affbb831ff27df9d09afeeb2e59f9", 2, 4, 64 * 1024 * 1024);
    ExtWrapperTest("http://localhost/metro.pivotal.io/", 64 * 1024,
                   "dceb001d03d54d61874d27c9f04596b1", 3, 4, 64 * 1024 * 1024);
}

#ifdef BIG_FILE_TEST
TEST(FakeExtWrapper, bigfile) {
    ExtWrapperTest("http://localhost/bigfile/", 64 * 1024,
                   "83c7ab787e3f1d1e7880dcae954ab4a4", 0, 1, 64 * 1024 * 1024);
}
#endif

#endif  // FAKE_TEST
