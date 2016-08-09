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

        S3WARN("Failed to get a good response in GET from '%s', retrying ...", url.c_str());
    };

    // an empty response(default status is RESPONSE_FAIL) returned if retries is 0
    return Response();
};

Response S3Service::putResponseWithRetries(const string &url, HTTPHeaders &headers,
                                           const map<string, string> &params, vector<uint8_t> &data,
                                           uint64_t retries) {
    while (retries--) {
        // declare response here to leverage RVO (Return Value Optimization)
        Response response = this->restfulService->put(url, headers, params, data);
        if (response.isSuccess() || (retries == 0)) {
            return response;
        };

        S3WARN("Failed to get a good response in PUT from '%s', retrying ...", url.c_str());
    };

    // an empty response(default status is RESPONSE_FAIL) returned if retries is 0
    return Response();
};

Response S3Service::postResponseWithRetries(const string &url, HTTPHeaders &headers,
                                            const map<string, string> &params,
                                            const vector<uint8_t> &data, uint64_t retries) {
    while (retries--) {
        // declare response here to leverage RVO (Return Value Optimization)
        Response response = this->restfulService->post(url, headers, params, data);
        if (response.isSuccess() || (retries == 0)) {
            return response;
        };

        S3WARN("Failed to get a good response in POST from '%s', retrying ...", url.c_str());
    };

    // an empty response(default status is RESPONSE_FAIL) returned if retries is 0
    return Response();
}

bool S3Service::isKeyExisted(ResponseCode code) {
    return isSuccessfulResponse(code);
}

bool S3Service::isHeadResponseCodeNeedRetry(ResponseCode code) {
    return code == HeadResponseFail;
}

ResponseCode S3Service::headResponseWithRetries(const string &url, HTTPHeaders &headers,
                                                const map<string, string> &params,
                                                uint64_t retries) {
    ResponseCode response = HeadResponseFail;

    while (retries--) {
        response = this->restfulService->head(url, headers, params);
        if (!isHeadResponseCodeNeedRetry(response) || (retries == 0)) {
            return response;
        };

        S3WARN("Failed to get a good response in PUT from '%s', retrying ...", url.c_str());
    };

    return response;
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
string S3Service::parseXMLMessage(xmlParserCtxtPtr xmlcontext, const string &tag) {
    string returnMessage;

    if (xmlcontext == NULL) {
        return returnMessage;
    }

    xmlNode *rootElement = xmlDocGetRootElement(xmlcontext->myDoc);
    if (rootElement == NULL) {
        S3ERROR("Failed to parse returned xml of bucket list");
        return returnMessage;
    }

    xmlNodePtr curNode = rootElement->xmlChildrenNode;
    while (curNode != NULL) {
        if (xmlStrcmp(curNode->name, (const xmlChar *)tag.c_str()) == 0) {
            char *content = (char *)xmlNodeGetContent(curNode);
            if (content != NULL) {
                returnMessage = content;
                xmlFree(content);
            }
            return returnMessage;
        }

        curNode = curNode->next;
    }

    return returnMessage;
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
            S3ERROR("Amazon S3 returns error \"%s\"",
                    parseXMLMessage(xmlContext, "Message").c_str());
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
            S3ERROR("Amazon S3 returns error \"%s\"",
                    parseXMLMessage(xmlContext, "Message").c_str());
        }

        S3ERROR("Failed to request: %s, Response message: %s", sourceUrl.c_str(),
                resp.getMessage().c_str());
        CHECK_OR_DIE_MSG(false, "Failed to request: %s, Response message: %s", sourceUrl.c_str(),
                         resp.getMessage().c_str());
    } else {
        S3ERROR("Failed to request: %s, Response message: %s", sourceUrl.c_str(),
                resp.getMessage().c_str());
        CHECK_OR_DIE_MSG(false, "Failed to request: %s, Response message: %s", sourceUrl.c_str(),
                         resp.getMessage().c_str());
    }

    return 0;
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
                S3ERROR("Amazon S3 returns error \"%s\"",
                        parseXMLMessage(xmlContext, "Message").c_str());
            }
        }
        CHECK_OR_DIE_MSG(false, "Failed to request: %s, Response message: %s", keyUrl.c_str(),
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

    return isKeyExisted(headResponseWithRetries(keyUrl, headers, params));
}

string S3Service::getUploadId(const string &keyUrl, const string &region,
                              const S3Credential &cred) {
    HTTPHeaders headers;
    map<string, string> params;
    UrlParser parser(keyUrl);

    headers.Add(HOST, parser.getHost());
    headers.Disable(CONTENTTYPE);
    headers.Disable(CONTENTLENGTH);
    headers.Add(X_AMZ_CONTENT_SHA256, "UNSIGNED-PAYLOAD");

    SignRequestV4("POST", &headers, region, parser.getPath(), "uploads=", cred);

    stringstream urlWithQuery;
    urlWithQuery << keyUrl << "?uploads";

    Response resp =
        this->postResponseWithRetries(urlWithQuery.str(), headers, params, vector<uint8_t>());
    if (resp.getStatus() == RESPONSE_OK) {
        xmlParserCtxtPtr xmlContext = getXMLContext(resp);
        if (xmlContext != NULL) {
            XMLContextHolder holder(xmlContext);
            return parseXMLMessage(xmlContext, "UploadId");
        }
    } else if (resp.getStatus() == RESPONSE_ERROR) {
        xmlParserCtxtPtr xmlContext = getXMLContext(resp);
        if (xmlContext != NULL) {
            XMLContextHolder holder(xmlContext);
            S3ERROR("Amazon S3 returns error \"%s\"",
                    parseXMLMessage(xmlContext, "Message").c_str());
        }

        S3ERROR("Failed to request: %s, Response message: %s", keyUrl.c_str(),
                resp.getMessage().c_str());
        CHECK_OR_DIE_MSG(false, "Failed to request: %s, Response message: %s", keyUrl.c_str(),
                         resp.getMessage().c_str());
    } else {
        S3ERROR("Failed to request: %s, Response message: %s", keyUrl.c_str(),
                resp.getMessage().c_str());
        CHECK_OR_DIE_MSG(false, "Failed to request: %s, Response message: %s", keyUrl.c_str(),
                         resp.getMessage().c_str());
    }

    return "";
}

string S3Service::uploadPartOfData(vector<uint8_t> &data, const string &keyUrl,
                                   const string &region, const S3Credential &cred,
                                   uint64_t partNumber, const string &uploadId) {
    HTTPHeaders headers;
    map<string, string> params;
    UrlParser parser(keyUrl);
    stringstream queryString;

    headers.Add(HOST, parser.getHost());
    headers.Add(X_AMZ_CONTENT_SHA256, "UNSIGNED-PAYLOAD");

    headers.Add(CONTENTTYPE, "text/plain");
    headers.Add(CONTENTLENGTH, std::to_string((unsigned long long)data.size()));

    queryString << "partNumber=" << partNumber << "&uploadId=" << uploadId;

    SignRequestV4("PUT", &headers, region, parser.getPath(), queryString.str(), cred);

    stringstream urlWithQuery;
    urlWithQuery << keyUrl << "?partNumber=" << partNumber << "&uploadId=" << uploadId;

    Response resp = this->putResponseWithRetries(urlWithQuery.str(), headers, params, data);
    if (resp.getStatus() == RESPONSE_OK) {
        string headers(resp.getRawHeaders().begin(), resp.getRawHeaders().end());

        uint64_t etagStartPos = headers.find("ETag: ") + 6;
        string etagToEnd = headers.substr(etagStartPos);
        // RFC 2616 states "HTTP/1.1 defines the sequence CR LF as the end-of-line
        // marker for all protocol elements except the entity-body"
        uint64_t etagStrLen = etagToEnd.find("\r");

        return etagToEnd.substr(0, etagStrLen);
    } else if (resp.getStatus() == RESPONSE_ERROR) {
        xmlParserCtxtPtr xmlContext = getXMLContext(resp);
        if (xmlContext != NULL) {
            XMLContextHolder holder(xmlContext);
            S3ERROR("Amazon S3 returns error \"%s\"",
                    parseXMLMessage(xmlContext, "Message").c_str());
        }

        S3ERROR("Failed to request: %s, Response message: %s", keyUrl.c_str(),
                resp.getMessage().c_str());
        CHECK_OR_DIE_MSG(false, "Failed to request: %s, Response message: %s", keyUrl.c_str(),
                         resp.getMessage().c_str());
    } else {
        S3ERROR("Failed to request: %s, Response message: %s", keyUrl.c_str(),
                resp.getMessage().c_str());
        CHECK_OR_DIE_MSG(false, "Failed to request: %s, Response message: %s", keyUrl.c_str(),
                         resp.getMessage().c_str());
    }

    return "";
}

bool S3Service::completeMultiPart(const string &keyUrl, const string &region,
                                  const S3Credential &cred, const string &uploadId,
                                  const vector<string> &etagArray) {
    HTTPHeaders headers;
    map<string, string> params;
    UrlParser parser(keyUrl);
    stringstream queryString;

    stringstream body;

    // check whether etagList or uploadId are empty,
    // no matter we have done this in upper-layer or not.
    if (etagArray.empty() || uploadId.empty()) {
        return false;
    }

    body << "<CompleteMultipartUpload>\n";
    for (uint64_t i = 0; i < etagArray.size(); ++i) {
        body << "  <Part>\n    <PartNumber>" << i + 1 << "</PartNumber>\n    <ETag>" << etagArray[i]
             << "</ETag>\n  </Part>\n";
    }
    body << "</CompleteMultipartUpload>";

    headers.Add(HOST, parser.getHost());
    headers.Add(CONTENTTYPE, "application/xml");
    headers.Add(X_AMZ_CONTENT_SHA256, "UNSIGNED-PAYLOAD");
    headers.Add(CONTENTLENGTH, std::to_string((unsigned long long)body.str().length()));

    queryString << "uploadId=" << uploadId;

    SignRequestV4("POST", &headers, region, parser.getPath(), queryString.str(), cred);

    stringstream urlWithQuery;
    urlWithQuery << keyUrl << "?uploadId=" << uploadId;

    string bodyString = body.str();
    Response resp = this->postResponseWithRetries(
        urlWithQuery.str(), headers, params, vector<uint8_t>(bodyString.begin(), bodyString.end()));

    if (resp.getStatus() == RESPONSE_OK) {
        return true;
    } else if (resp.getStatus() == RESPONSE_ERROR) {
        xmlParserCtxtPtr xmlContext = getXMLContext(resp);
        if (xmlContext != NULL) {
            XMLContextHolder holder(xmlContext);
            S3ERROR("Amazon S3 returns error \"%s\"",
                    parseXMLMessage(xmlContext, "Message").c_str());
        }

        S3ERROR("Failed to request: %s, Response message: %s", keyUrl.c_str(),
                resp.getMessage().c_str());
        CHECK_OR_DIE_MSG(false, "Failed to request: %s, Response message: %s", keyUrl.c_str(),
                         resp.getMessage().c_str());
    } else {
        S3ERROR("Failed to request: %s, Response message: %s", keyUrl.c_str(),
                resp.getMessage().c_str());
        CHECK_OR_DIE_MSG(false, "Failed to request: %s, Response message: %s", keyUrl.c_str(),
                         resp.getMessage().c_str());
    }

    return false;
}

ListBucketResult::~ListBucketResult() {
    vector<BucketContent *>::iterator i;
    for (i = this->contents.begin(); i != this->contents.end(); i++) {
        delete *i;
    }
}
