#include "s3interface.cpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "restful_service.cpp"

using ::testing::AtLeast;
using ::testing::Return;
using ::testing::Throw;
using ::testing::_;

class MockS3RESTfulService : public RESTfulService {
   public:
    MOCK_METHOD3(get, Response(const string &url, HTTPHeaders &headers,
                               const map<string, string> &params));
};

class XMLGenerator {
   public:
    XMLGenerator() : isTruncated(false) {}

    XMLGenerator *setName(string name) {
        this->name = name;
        return this;
    }
    XMLGenerator *setPrefix(string prefix) {
        this->prefix = prefix;
        return this;
    }
    XMLGenerator *setMarker(string marker) {
        this->marker = marker;
        return this;
    }
    XMLGenerator *setIsTruncated(bool isTruncated) {
        this->isTruncated = isTruncated;
        return this;
    }
    XMLGenerator *pushBuckentContent(BucketContent content) {
        this->contents.push_back(content);
        return this;
    }

    vector<uint8_t> toXML() {
        stringstream sstr;
        sstr << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
             << "<ListBucketResult>"
             << "<Name>" << name << "</Name>"
             << "<Prefix>" << prefix << "</Prefix>"
             << "<Marker>" << marker << "</Marker>"
             << "<IsTruncated>" << (isTruncated ? "true" : "false") << "</IsTruncated>";

        for (vector<BucketContent>::iterator it = contents.begin(); it != contents.end(); it++) {
            sstr << "<Contents>"
                 << "<Key>" << it->name << "</Key>"
                 << "<Size>" << it->size << "</Size>"
                 << "</Contents>";
        }
        sstr << "</ListBucketResult>";
        string xml = sstr.str();
        return vector<uint8_t>(xml.begin(), xml.end());
    }

   private:
    string name;
    string prefix;
    string marker;
    bool isTruncated;

    vector<BucketContent> contents;
};

class S3ServiceTest : public testing::Test {
   protected:
    // Remember that SetUp() is run immediately before a test starts.
    virtual void SetUp() {
        s3ext_logtype = STDERR_LOG;
        s3ext_loglevel = EXT_INFO;

        s3service = new S3Service();
        schema = "https";
        s3service->setRESTfulService(&mockRestfulService);
    }

    // TearDown() is invoked immediately after a test finishes.
    virtual void TearDown() { delete s3service; }

    Response buildListBucketResponse(int numOfContent, bool isTruncated, int numOfZeroKeys = 0) {
        XMLGenerator generator;
        XMLGenerator *gen = &generator;
        gen->setName("s3test.pivotal.io")
            ->setPrefix("s3files/")
            ->setIsTruncated(isTruncated)
            ->pushBuckentContent(BucketContent("s3files/", 0));

        char buffer[32] = {0};
        for (int i = 0; i < numOfContent; ++i) {
            snprintf(buffer, 32, "files%d", i);
            gen->pushBuckentContent(BucketContent(buffer, i + 1));
        }

        for (int i = 0; i < numOfZeroKeys; i++) {
            snprintf(buffer, 32, "zerofiles%d", i);
            gen->pushBuckentContent(BucketContent(buffer, 0));
        }

        return Response(OK, gen->toXML());
    }

    S3Service *s3service;
    S3Credential cred;
    string schema;
    string region;
    string bucket;
    string prefix;

    MockS3RESTfulService mockRestfulService;
    Response response;
};

TEST_F(S3ServiceTest, ListBucketThrowExceptionWhenBucketStringIsEmpty) {
    EXPECT_THROW(s3service->ListBucket("", "", "", "", cred), std::runtime_error);
}

TEST_F(S3ServiceTest, ListBucketWithWrongRegion) {
    EXPECT_CALL(mockRestfulService, get(_, _, _)).WillOnce(Return(response));

    EXPECT_EQ((void *)NULL, s3service->ListBucket(schema, "nonexist", "", "", cred));
}

TEST_F(S3ServiceTest, ListBucketWithWrongBucketName) {
    uint8_t xml[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<Error>"
        "<Code>PermanentRedirect</Code>"
        "<Message>The bucket you are attempting to access must be addressed "
        "using the specified endpoint. "
        "Please send all future requests to this endpoint.</Message>"
        "<Bucket>foo</Bucket><Endpoint>s3.amazonaws.com</Endpoint>"
        "<RequestId>27DD9B7004AF83E3</RequestId>"
        "<HostId>NL3pyGvn+FajhQLKz/"
        "hXUzV1VnFbbwNjUQsqWeFiDANkV4EVkh8Kpq5NNAi27P7XDhoA9M9Xhg0=</HostId>"
        "</Error>";
    vector<uint8_t> raw(xml, xml + sizeof(xml) - 1);
    Response response(OK, raw);

    EXPECT_CALL(mockRestfulService, get(_, _, _)).WillOnce(Return(response));

    EXPECT_EQ((void *)NULL, s3service->ListBucket(schema, "us-west-2", "foo/bar", "", cred));
}

TEST_F(S3ServiceTest, ListBucketWithNormalBucket) {
    XMLGenerator generator;
    XMLGenerator *gen = &generator;
    gen->setName("s3test.pivotal.io")
        ->setPrefix("threebytes/")
        ->setIsTruncated(false)
        ->pushBuckentContent(BucketContent("threebytes/", 0))
        ->pushBuckentContent(BucketContent("threebytes/threebytes", 3));

    Response response(OK, gen->toXML());

    EXPECT_CALL(mockRestfulService, get(_, _, _)).WillOnce(Return(response));

    ListBucketResult *result =
        s3service->ListBucket(schema, "us-west-2", "s3test.pivotal.io", "threebytes/", cred);
    ASSERT_NE((void *)NULL, result);
    EXPECT_EQ(1, result->contents.size());
}

TEST_F(S3ServiceTest, ListBucketWithBucketWith1000Keys) {
    EXPECT_CALL(mockRestfulService, get(_, _, _))
        .WillOnce(Return(this->buildListBucketResponse(1000, false)));

    ListBucketResult *result =
        s3service->ListBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/", cred);
    ASSERT_NE((void *)NULL, result);
    EXPECT_EQ(1000, result->contents.size());
}

TEST_F(S3ServiceTest, ListBucketWithBucketWith1001Keys) {
    EXPECT_CALL(mockRestfulService, get(_, _, _))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(1, false)));

    ListBucketResult *result =
        s3service->ListBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/", cred);
    ASSERT_NE((void *)NULL, result);
    EXPECT_EQ(1001, result->contents.size());
}

TEST_F(S3ServiceTest, ListBucketWithBucketWithMoreThan1000Keys) {
    EXPECT_CALL(mockRestfulService, get(_, _, _))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(120, false)));

    ListBucketResult *result =
        s3service->ListBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/", cred);
    ASSERT_NE((void *)NULL, result);
    EXPECT_EQ(5120, result->contents.size());
}

TEST_F(S3ServiceTest, ListBucketWithBucketWithTruncatedResponse) {
    Response EmptyResponse;

    EXPECT_CALL(mockRestfulService, get(_, _, _))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(EmptyResponse));

    ListBucketResult *result =
        s3service->ListBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/", cred);
    EXPECT_EQ((void *)NULL, result);
}

TEST_F(S3ServiceTest, ListBucketWithBucketWithZeroSizedKeys) {
    EXPECT_CALL(mockRestfulService, get(_, _, _))
        .WillOnce(Return(this->buildListBucketResponse(0, true, 8)))
        .WillOnce(Return(this->buildListBucketResponse(1000, true)))
        .WillOnce(Return(this->buildListBucketResponse(120, false, 8)));

    ListBucketResult *result =
        s3service->ListBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/", cred);
    ASSERT_NE((void *)NULL, result);
    EXPECT_EQ(1120, result->contents.size());
}

TEST_F(S3ServiceTest, ListBucketWithEmptyBucket) {
    EXPECT_CALL(mockRestfulService, get(_, _, _))
        .WillOnce(Return(this->buildListBucketResponse(0, false, 0)));

    ListBucketResult *result =
        s3service->ListBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/", cred);
    ASSERT_NE((void *)NULL, result);
    EXPECT_EQ(0, result->contents.size());
}

TEST_F(S3ServiceTest, ListBucketWithAllZeroedFilesBucket) {
    EXPECT_CALL(mockRestfulService, get(_, _, _))
        .WillOnce(Return(this->buildListBucketResponse(0, false, 2)));

    ListBucketResult *result =
        s3service->ListBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/", cred);
    ASSERT_NE((void *)NULL, result);
    EXPECT_EQ(0, result->contents.size());
}

TEST_F(S3ServiceTest, ListBucketWithErrorResponse) {
    EXPECT_CALL(mockRestfulService, get(_, _, _)).WillOnce(Return(response));

    EXPECT_EQ((void *)NULL,
              s3service->ListBucket(schema, "nonexist", "s3test.pivotal.io", "s3files/", cred));
}

TEST_F(S3ServiceTest, ListBucketWithErrorReturnedXML) {
    uint8_t xml[] = "whatever";
    vector<uint8_t> raw(xml, xml + sizeof(xml) - 1);
    Response response(OK, raw);

    EXPECT_CALL(mockRestfulService, get(_, _, _)).WillOnce(Return(response));

    EXPECT_EQ((void *)NULL,
              s3service->ListBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/", cred));
}

TEST_F(S3ServiceTest, ListBucketWithNonRootXML) {
    uint8_t xml[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    vector<uint8_t> raw(xml, xml + sizeof(xml) - 1);
    Response response(OK, raw);

    EXPECT_CALL(mockRestfulService, get(_, _, _)).WillOnce(Return(response));

    EXPECT_EQ((void *)NULL,
              s3service->ListBucket(schema, "us-west-2", "s3test.pivotal.io", "s3files/", cred));
}