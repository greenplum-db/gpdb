#ifndef TEST_MOCK_CLASSES_H_
#define TEST_MOCK_CLASSES_H_

#include "gmock/gmock.h"

#include "s3common.h"
#include "s3interface.h"
#include "s3restful_service.h"

#include <string>
#include <sstream>
using std::string;
using std::stringstream;

class MockS3Interface : public S3Interface {
   public:
    MOCK_METHOD5(listBucket,
                 ListBucketResult*(const string& schema, const string& region, const string& bucket,
                                   const string& prefix, const S3Credential& cred));

    MOCK_METHOD6(fetchData,
                 uint64_t(uint64_t offset, char *data, uint64_t len, const string &sourceUrl,
                          const string &region, const S3Credential &cred));

    MOCK_METHOD3(checkCompressionType, S3CompressionType(const string& keyUrl, const string& region,
                                           const S3Credential& cred));
};


class MockS3RESTfulService : public S3RESTfulService {
   public:
    MOCK_METHOD3(get, Response(const string &url, HTTPHeaders &headers,
                               const map<string, string> &params));
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
