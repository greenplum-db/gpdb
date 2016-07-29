#include <unistd.h>
#include <algorithm>
#include <iostream>
#include <sstream>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "s3http_headers.h"
#include "s3key_reader.h"
#include "s3log.h"
#include "s3macros.h"
#include "s3url_parser.h"
#include "s3utils.h"

#include "s3interface.h"
using std::stringstream;

// use destructor ~XMLContextHolder() to do the cleanup
class XMLContextHolder {
   public:
    XMLContextHolder(xmlParserCtxtPtr ctx) : context(ctx) {
    }
    ~XMLContextHolder() {
        if (context != NULL) {
            xmlFreeDoc(context->myDoc);
            xmlFreeParserCtxt(context);
        }
    }

   private:
    xmlParserCtxtPtr context;
};

S3Service::S3Service() : restfulService(NULL) {
    xmlInitParser();
}

S3Service::~S3Service() {
    // Cleanup function for the XML library.
    xmlCleanupParser();
}

Response S3Service::getResponseWithRetries(const string &url, HTTPHeaders &headers,
                                           const map<string, string> &params, uint64_t retries) {
    while (retries--) {
        // declare response here to leverage RVO (Return Value Optimization)
        Response response = this->restfulService->get(url, headers, params);
        if (response.isSuccess() || (retries == 0)) {
            return response;
        };

        S3WARN("Failed to get a good response from '%s', retrying ...", url.c_str());
    };

    // an empty response(default status is RESPONSE_FAIL) returned if retries is 0
    return Response();
};

// S3 requires query parameters specified alphabetically.
string S3Service::getUrl(const string &prefix, const string &schema, const string &host,
                         const string &bucket, const string &marker) {
    stringstream url;
    url << schema << "://" << host << "/" << bucket;

    if (!marker.empty()) {
        url << "?marker=" << marker;
    }

    if (!prefix.empty()) {
        url << (marker.empty() ? "?" : "&") << "prefix=" << prefix;
    }

    return url.str();
}

HTTPHeaders S3Service::composeHTTPHeaders(const string &url, const string &marker,
                                          const string &prefix, const string &region,
                                          const S3Credential &cred) {
    stringstream host;
    host << "s3-" << region << ".amazonaws.com";

    HTTPHeaders header;
    header.Add(HOST, host.str());
    header.Add(X_AMZ_CONTENT_SHA256, "UNSIGNED-PAYLOAD");

    UrlParser p(url);

    stringstream query;
    if (!marker.empty()) {
        query << "marker=" << marker;
        if (!prefix.empty()) {
            query << "&";
        }
    }
    if (!prefix.empty()) {
        query << "prefix=" << prefix;
    }

    SignRequestV4("GET", &header, region, p.getPath(), query.str(), cred);

    return header;
}

xmlParserCtxtPtr S3Service::getXMLContext(Response &response) {
    xmlParserCtxtPtr xmlptr =
        xmlCreatePushParserCtxt(NULL, NULL, (const char *)(response.getRawData().data()),
                                response.getRawData().size(), "resp.xml");
    if (xmlptr != NULL) {
        xmlParseChunk(xmlptr, "", 0, 1);
    } else {
        S3ERROR("Failed to create XML parser context");
    }
    return xmlptr;
}

// require curl 7.17 higher
// http://docs.aws.amazon.com/AmazonS3/latest/API/RESTBucketGET.html
Response S3Service::getBucketResponse(const string &region, const string &url, const string &prefix,
                                      const S3Credential &cred, const string &marker) {
    HTTPHeaders header = composeHTTPHeaders(url, marker, prefix, region, cred);
    std::map<string, string> empty;

    return this->getResponseWithRetries(url, header, empty);
}

// parseXMLMessage must not throw exception, otherwise result is leaked.
void S3Service::parseXMLMessage(xmlParserCtxtPtr xmlcontext) {
    if (xmlcontext == NULL) {
        return;
    }

    xmlNode *rootElement = xmlDocGetRootElement(xmlcontext->myDoc);
    if (rootElement == NULL) {
        S3ERROR("Failed to parse returned xml of bucket list");
        return;
    }

    xmlNodePtr curNode = rootElement->xmlChildrenNode;
    while (curNode != NULL) {
        if (xmlStrcmp(curNode->name, (const xmlChar *)"Message") == 0) {
            char *content = (char *)xmlNodeGetContent(curNode);
            if (content != NULL) {
                S3ERROR("Amazon S3 returns error \"%s\"", content);
                xmlFree(content);
            }
            return;
        }

        curNode = curNode->next;
    }
}

// parseBucketXML must not throw exception, otherwise result is leaked.
bool S3Service::parseBucketXML(ListBucketResult *result, xmlParserCtxtPtr xmlcontext,
                               string &marker) {
    if ((result == NULL) || (xmlcontext == NULL)) {
        return false;
    }

    xmlNode *rootElement = xmlDocGetRootElement(xmlcontext->myDoc);
    if (rootElement == NULL) {
        S3WARN("Failed to parse returned xml of bucket list");
        return false;
    }

    xmlNodePtr cur;
    bool is_truncated = false;
    char *content = NULL;
    char *key = NULL;
    char *key_size = NULL;

    cur = rootElement->xmlChildrenNode;
    while (cur != NULL) {
        if (key) {
            xmlFree(key);
            key = NULL;
        }

        if (!xmlStrcmp(cur->name, (const xmlChar *)"IsTruncated")) {
            content = (char *)xmlNodeGetContent(cur);
            if (content) {
                if (!strncmp(content, "true", 4)) {
                    is_truncated = true;
                }
                xmlFree(content);
            }
        }

        if (!xmlStrcmp(cur->name, (const xmlChar *)"Name")) {
            content = (char *)xmlNodeGetContent(cur);
            if (content) {
                result->Name = content;
                xmlFree(content);
            }
        }

        if (!xmlStrcmp(cur->name, (const xmlChar *)"Prefix")) {
            content = (char *)xmlNodeGetContent(cur);
            if (content) {
                result->Prefix = content;
                xmlFree(content);
                // content is not used anymore in this loop
                content = NULL;
            }
        }

        if (!xmlStrcmp(cur->name, (const xmlChar *)"Contents")) {
            xmlNodePtr contNode = cur->xmlChildrenNode;
            uint64_t size = 0;

            while (contNode != NULL) {
                // no memleak here, every content has only one Key/Size node
                if (!xmlStrcmp(contNode->name, (const xmlChar *)"Key")) {
                    key = (char *)xmlNodeGetContent(contNode);
                }
                if (!xmlStrcmp(contNode->name, (const xmlChar *)"Size")) {
                    key_size = (char *)xmlNodeGetContent(contNode);
                    // Size of S3 file is a natural number, don't worry
                    size = (uint64_t)atoll((const char *)key_size);
                }
                contNode = contNode->next;
            }

            if (key) {
                if (size > 0) {  // skip empty item
                    BucketContent *item = new BucketContent(key, size);
                    if (item) {
                        result->contents.push_back(item);
                    } else {
                        S3ERROR("Faild to create item for %s", key);
                    }
                } else {
                    S3INFO("Size of \"%s\" is %" PRIu64 ", skip it", key, size);
                }
            }

            if (key_size) {
                xmlFree(key_size);
                key_size = NULL;
            }
        }

        cur = cur->next;
    }

    marker = (is_truncated && key) ? key : "";

    if (key) {
        xmlFree(key);
    }

    return true;
}

// ListBucket list all keys in given bucket with given prefix.
//
// Return NULL when there is failure due to network instability or
// service unstable, so that caller could retry.
//
// Caller should delete returned object.
ListBucketResult *S3Service::listBucket(const string &schema, const string &region,
                                        const string &bucket, const string &prefix,
                                        const S3Credential &cred) {
    stringstream host;
    host << "s3-" << region << ".amazonaws.com";
    S3DEBUG("Host url is %s", host.str().c_str());

    // NOTE: here we might have memory leak.
    ListBucketResult *result = new ListBucketResult();
    CHECK_OR_DIE_MSG(result != NULL, "%s", "Failed to allocate bucket list result");

    string marker = "";
    do {  // To get next set(up to 1000) keys in one iteration.
        // S3 requires query parameters specified alphabetically.
        string url = this->getUrl(prefix, schema, host.str(), bucket, marker);

        Response response = getBucketResponse(region, url, prefix, cred, marker);

        xmlParserCtxtPtr xmlContext = getXMLContext(response);
        XMLContextHolder holder(xmlContext);

        if (response.isSuccess()) {
            if (parseBucketXML(result, xmlContext, marker)) {
                continue;
            }
        } else {
            parseXMLMessage(xmlContext);
        }

        delete result;
        return NULL;
    } while (!marker.empty());

    return result;
}

uint64_t S3Service::fetchData(uint64_t offset, vector<uint8_t> &data, uint64_t len,
                              const string &sourceUrl, const string &region,
                              const S3Credential &cred) {
    HTTPHeaders headers;
    map<string, string> params;
    UrlParser parser(sourceUrl);

    char rangeBuf[S3_RANGE_HEADER_STRING_LEN] = {0};
    snprintf(rangeBuf, sizeof(rangeBuf), "bytes=%" PRIu64 "-%" PRIu64, offset, offset + len - 1);
    headers.Add(HOST, parser.getHost());
    headers.Add(RANGE, rangeBuf);
    headers.Add(X_AMZ_CONTENT_SHA256, "UNSIGNED-PAYLOAD");

    SignRequestV4("GET", &headers, region, parser.getPath(), "", cred);

    Response resp = this->getResponseWithRetries(sourceUrl, headers, params);
    if (resp.getStatus() == RESPONSE_OK) {
        data = resp.moveDataBuffer();
        if (data.size() != len) {
            S3ERROR("%s", "Response is not fully received.");
            CHECK_OR_DIE_MSG(false, "%s", "Response is not fully received.");
        }

        return data.size();
    } else if (resp.getStatus() == RESPONSE_ERROR) {
        xmlParserCtxtPtr xmlContext = getXMLContext(resp);
        if (xmlContext != NULL) {
            XMLContextHolder holder(xmlContext);
            parseXMLMessage(xmlContext);
        }

        S3ERROR("Failed to fetch: %s, Response message: %s", sourceUrl.c_str(),
                resp.getMessage().c_str());
        CHECK_OR_DIE_MSG(false, "Failed to fetch: %s, Response message: %s", sourceUrl.c_str(),
                         resp.getMessage().c_str());

        return 0;
    } else {
        S3ERROR("Failed to fetch: %s, Response message: %s", sourceUrl.c_str(),
                resp.getMessage().c_str());
        CHECK_OR_DIE_MSG(false, "Failed to fetch: %s, Response message: %s", sourceUrl.c_str(),
                         resp.getMessage().c_str());
        return 0;
    }
}

uint64_t S3Service::uploadData(vector<uint8_t> &data, const string &sourceUrl, const string &region,
                               const S3Credential &cred) {
    HTTPHeaders headers;
    map<string, string> params;
    UrlParser parser(sourceUrl);

    headers.Add(HOST, parser.getHost());
    headers.Add(X_AMZ_CONTENT_SHA256, "UNSIGNED-PAYLOAD");

    headers.Add(CONTENTTYPE, "text/plain");
    headers.Add(CONTENTLENGTH, std::to_string((unsigned long long)data.size()));

    SignRequestV4("PUT", &headers, region, parser.getPath(), "", cred);

    Response resp = this->restfulService->put(sourceUrl, headers, params, data);
    if (resp.getStatus() == RESPONSE_OK) {
        return data.size();
    } else if (resp.getStatus() == RESPONSE_ERROR) {
        xmlParserCtxtPtr xmlContext = getXMLContext(resp);
        if (xmlContext != NULL) {
            XMLContextHolder holder(xmlContext);
            parseXMLMessage(xmlContext);
        }

        S3ERROR("Failed to fetch: %s, Response message: %s", sourceUrl.c_str(),
                resp.getMessage().c_str());
        CHECK_OR_DIE_MSG(false, "Failed to fetch: %s, Response message: %s", sourceUrl.c_str(),
                         resp.getMessage().c_str());

        return 0;
    } else {
        S3ERROR("Failed to fetch: %s, Response message: %s", sourceUrl.c_str(),
                resp.getMessage().c_str());
        CHECK_OR_DIE_MSG(false, "Failed to fetch: %s, Response message: %s", sourceUrl.c_str(),
                         resp.getMessage().c_str());
        return 0;
    }
}

S3CompressionType S3Service::checkCompressionType(const string &keyUrl, const string &region,
                                                  const S3Credential &cred) {
    HTTPHeaders headers;
    map<string, string> params;
    UrlParser parser(keyUrl);

    char rangeBuf[S3_RANGE_HEADER_STRING_LEN] = {0};
    snprintf(rangeBuf, sizeof(rangeBuf), "bytes=%d-%d", 0, S3_MAGIC_BYTES_NUM - 1);

    headers.Add(HOST, parser.getHost());
    headers.Add(RANGE, rangeBuf);
    headers.Add(X_AMZ_CONTENT_SHA256, "UNSIGNED-PAYLOAD");

    SignRequestV4("GET", &headers, region, parser.getPath(), "", cred);

    Response resp = this->getResponseWithRetries(keyUrl, headers, params);
    if (resp.getStatus() == RESPONSE_OK) {
        vector<uint8_t> &responseData = resp.getRawData();
        if (responseData.size() < S3_MAGIC_BYTES_NUM) {
            return S3_COMPRESSION_PLAIN;
        }

        CHECK_OR_DIE_MSG(responseData.size() == S3_MAGIC_BYTES_NUM, "%s",
                         "Response is not fully received.");

        if ((responseData[0] == 0x1f) && (responseData[1] == 0x8b)) {
            return S3_COMPRESSION_GZIP;
        }
    } else {
        if (resp.getStatus() == RESPONSE_ERROR) {
            xmlParserCtxtPtr xmlContext = getXMLContext(resp);
            if (xmlContext != NULL) {
                XMLContextHolder holder(xmlContext);
                parseXMLMessage(xmlContext);
            }
        }
        CHECK_OR_DIE_MSG(false, "Failed to fetch: %s, Response message: %s", keyUrl.c_str(),
                         resp.getMessage().c_str());
    }

    return S3_COMPRESSION_PLAIN;
}

bool S3Service::checkKeyExistence(const string &keyUrl, const string &region,
                                  const S3Credential &cred) {
    HTTPHeaders headers;
    map<string, string> params;
    UrlParser parser(keyUrl);

    headers.Add(HOST, parser.getHost());
    headers.Add(X_AMZ_CONTENT_SHA256, "UNSIGNED-PAYLOAD");

    SignRequestV4("HEAD", &headers, region, parser.getPath(), "", cred);

    ResponseCode code = this->restfulService->head(keyUrl, headers, params);

    if (code == 200 || code == 206) {
        return true;
    }

    return false;
}

ListBucketResult::~ListBucketResult() {
    vector<BucketContent *>::iterator i;
    for (i = this->contents.begin(); i != this->contents.end(); i++) {
        delete *i;
    }
}
