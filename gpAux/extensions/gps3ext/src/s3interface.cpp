#include <unistd.h>
#include <algorithm>
#include <iostream>
#include <sstream>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "gps3ext.h"
#include "s3http_headers.h"
#include "s3log.h"
#include "s3macros.h"
#include "s3reader.h"
#include "s3url_parser.h"
#include "s3utils.h"

#include "s3interface.h"
using std::stringstream;

class XMLContextHolder {
   public:
    XMLContextHolder(xmlParserCtxtPtr ctx) : context(ctx) {}
    ~XMLContextHolder() {
        if (context != NULL) {
            xmlFreeDoc(context->myDoc);
            xmlFreeParserCtxt(context);
        }
    }

   private:
    xmlParserCtxtPtr context;
};

S3Service::S3Service() : service(NULL) {}

S3Service::~S3Service() {}

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
    UrlParser p(url.c_str());
    header.Add(X_AMZ_CONTENT_SHA256, "UNSIGNED-PAYLOAD");
    std::stringstream query;
    if (!marker.empty()) {
        query << "marker=" << marker;
        if (!prefix.empty()) {
            query << "&";
        }
    }
    if (!prefix.empty()) {
        query << "prefix=" << prefix;
    }

    SignRequestV4("GET", &header, region, p.Path(), query.str(), cred);

    return header;
}

// require curl 7.17 higher
// http://docs.aws.amazon.com/AmazonS3/latest/API/RESTBucketGET.html
xmlParserCtxtPtr S3Service::getBucketXML(const string &region, const string &url,
                                         const string &prefix, const S3Credential &cred,
                                         const string &marker) {
    HTTPHeaders header = composeHTTPHeaders(url, marker, prefix, region, cred);
    std::map<string, string> empty;

    Response response = service->get(url, header, empty);
    if (!response.isSuccess()) {
        S3ERROR("Failed to GET bucket list of '%s'", url.c_str());
        return NULL;
    }

    xmlParserCtxtPtr xmlptr =
        xmlCreatePushParserCtxt(NULL, NULL, (const char *)response.getRawData().data(),
                                response.getRawData().size(), "resp.xml");
    if (xmlptr != NULL) {
        xmlParseChunk(xmlptr, "", 0, 1);
    } else {
        S3ERROR("Failed to create XML parser context");
    }

    return xmlptr;
}

bool S3Service::checkAndParseBucketXML(ListBucketResult *result, xmlParserCtxtPtr xmlcontext,
                                       string &marker) {
    XMLContextHolder holder(xmlcontext);

    xmlNode *rootElement = xmlDocGetRootElement(xmlcontext->myDoc);
    if (rootElement == NULL) {
        S3ERROR("Failed to parse returned xml of bucket list");
        return false;
    }

    xmlNodePtr curNode = rootElement->xmlChildrenNode;
    while (curNode != NULL) {
        if (xmlStrcmp(curNode->name, (const xmlChar *)"Message") == 0) {
            char *content = (char *)xmlNodeGetContent(curNode);
            if (content != NULL) {
                S3ERROR("Amazon S3 returns error \"%s\"", content);
                xmlFree(content);
            }
            return false;
        }

        curNode = curNode->next;
    }

    // parseBucketXML will set marker for next round.
    this->parseBucketXML(result, rootElement, marker);

    return true;
}

void S3Service::parseBucketXML(ListBucketResult *result, xmlNode *root_element, string &marker) {
    CHECK_OR_DIE((result != NULL && root_element != NULL));

    xmlNodePtr cur;
    bool is_truncated = false;
    char *content = NULL;
    char *key = NULL;
    char *key_size = NULL;

    cur = root_element->xmlChildrenNode;
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

    return;
}

// ListBucket list all keys in given bucket with given prefix.
//
// Return NULL when there is failure due to network instability or
// service unstable, so that caller could retry.
//
// Caller should delete returned object.
ListBucketResult *S3Service::ListBucket(const string &schema, const string &region,
                                        const string &bucket, const string &prefix,
                                        const S3Credential &cred) {
    stringstream host;
    host << "s3-" << region << ".amazonaws.com";
    S3DEBUG("Host url is %s", host.str().c_str());

    // TODO: here we have memory leak.
    ListBucketResult *result = new ListBucketResult();
    CHECK_OR_DIE_MSG(result != NULL, "%s", "Failed to allocate bucket list result");

    string marker = "";
    do {  // To get next set(up to 1000) keys in one iteration.
        // S3 requires query parameters specified alphabetically.
        string url = this->getUrl(prefix, schema, host.str(), bucket, marker);

        xmlParserCtxtPtr xmlcontext = getBucketXML(region, url, prefix, cred, marker);
        if (xmlcontext == NULL) {
            S3ERROR("Failed to list bucket for '%s'", url.c_str());
            delete result;
            return NULL;
        }

        // parseBucketXML must not throw exception, otherwise result is leaked.
        if (!checkAndParseBucketXML(result, xmlcontext, marker)) {
            delete result;
            return NULL;
        }
    } while (!marker.empty());

    return result;
}