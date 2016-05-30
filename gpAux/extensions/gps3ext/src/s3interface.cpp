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
#include "s3reader.h"
#include "s3url_parser.h"
#include "s3utils.h"

#include "s3interface.h"
using std::stringstream;

S3Service::S3Service() {
    // TODO Auto-generated constructor stub
}

S3Service::~S3Service() {
    // TODO Auto-generated destructor stub
}

// It is caller's responsibility to free returned memory.
ListBucketResult *S3Service::ListBucket(const string &schema,
                                        const string &region,
                                        const string &bucket,
                                        const string &prefix,
                                        const S3Credential &cred) {
    // To get next up to 1000 keys.
    // If marker is empty, get first 1000 then.
    // S3 will return the last key as the next marker.
    string marker = "";

    stringstream host;
    host << "s3-" << region << ".amazonaws.com";

    S3DEBUG("Host url is %s", host.str().c_str());

    ListBucketResult *result = new ListBucketResult();
    if (!result) {
        S3ERROR("Failed to allocate bucket list result");
        return NULL;
    }

    stringstream url;
    xmlParserCtxtPtr xmlcontext = NULL;

    do {
        if (prefix != "") {
            url << schema << "://" << host.str() << "/" << bucket << "?";

            if (marker != "") {
                url << "marker=" << marker << "&";
            }

            url << "prefix=" << prefix;
        } else {
            url << schema << "://" << bucket << "." << host.str() << "?";

            if (marker != "") {
                url << "marker=" << marker;
            }
        }

        xmlcontext = DoGetXML(region, url.str(), prefix, cred, marker);
        if (!xmlcontext) {
            S3ERROR("Failed to list bucket for %s", url.str().c_str());
            delete result;
            return NULL;
        }

        xmlDocPtr doc = xmlcontext->myDoc;
        xmlNode *root_element = xmlDocGetRootElement(xmlcontext->myDoc);
        if (!root_element) {
            S3ERROR("Failed to parse returned xml of bucket list");
            delete result;
            xmlFreeParserCtxt(xmlcontext);
            xmlFreeDoc(doc);
            return NULL;
        }

        xmlNodePtr cur = root_element->xmlChildrenNode;
        while (cur != NULL) {
            if (!xmlStrcmp(cur->name, (const xmlChar *)"Message")) {
                char *content = (char *)xmlNodeGetContent(cur);
                if (content) {
                    S3ERROR("Amazon S3 returns error \"%s\"", content);
                    xmlFree(content);
                }
                delete result;
                xmlFreeParserCtxt(xmlcontext);
                xmlFreeDoc(doc);
                return NULL;
            }

            cur = cur->next;
        }

        if (!extractContent(result, root_element, marker)) {
            S3ERROR("Failed to extract key from bucket list");
            delete result;
            xmlFreeParserCtxt(xmlcontext);
            xmlFreeDoc(doc);
            return NULL;
        }

        // clear url
        url.str("");

        // always cleanup
        xmlFreeParserCtxt(xmlcontext);
        xmlFreeDoc(doc);
        xmlcontext = NULL;
    } while (marker != "");

    return result;
}
