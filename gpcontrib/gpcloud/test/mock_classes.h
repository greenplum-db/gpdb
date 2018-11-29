#ifndef TEST_MOCK_CLASSES_H_
#define TEST_MOCK_CLASSES_H_

#include "gmock/gmock.h"

#include "s3common_headers.h"
#include "s3interface.h"
#include "s3restful_service.h"

class MockS3Interface : public S3Interface {
   public:
    MOCK_METHOD1(listBucket,
                 ListBucketResult(S3Url &));

    MOCK_METHOD4(fetchData,
                 uint64_t(uint64_t , S3VectorUInt8& , uint64_t len, const S3Url &));

    MOCK_METHOD1(checkCompressionType, S3CompressionType(const S3Url&));

    MOCK_METHOD1(checkKeyExistence, bool(const S3Url&));

    MOCK_METHOD1(getUploadId, string(const S3Url&));

    MOCK_METHOD4(uploadPartOfData, string(S3VectorUInt8&, const S3Url&,
                            uint64_t partNumber, const string& uploadId));

    MOCK_METHOD3(completeMultiPart, bool(const S3Url&,
                           const string&, const vector<string>&));

    MOCK_METHOD2(abortUpload, bool(const S3Url &,
                 const string &));

};

class MockS3RESTfulService : public S3RESTfulService {
   public:
    MockS3RESTfulService(const S3Params& params):S3RESTfulService(params){}

    MOCK_METHOD2(head, ResponseCode(const string &, HTTPHeaders &));

    MOCK_METHOD2(get, Response(const string &, HTTPHeaders &));

    MOCK_METHOD3(put, Response(const string &, HTTPHeaders &,
                               const S3VectorUInt8 &));

    MOCK_METHOD3(post, Response(const string &, HTTPHeaders &,
                                const vector<uint8_t> &));

    MOCK_METHOD2(deleteRequest, Response(const string &, HTTPHeaders &));
};

class XMLGenerator {
   public:
    XMLGenerator() : isTruncated(false) {
    }

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

struct DebugSwitch {
	static void enable() {
		s3ext_loglevel = EXT_DEBUG;
		s3ext_logtype = STDERR_LOG;
	}

	static void disable() {
		s3ext_loglevel = EXT_WARNING;
		s3ext_logtype = INTERNAL_LOG;
	}
};

#endif /* TEST_MOCK_CLASSES_H_ */
