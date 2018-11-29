#include "s3interface.h"

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

S3InterfaceService::S3InterfaceService() : restfulService(NULL), params("") {
    xmlInitParser();
}

S3InterfaceService::S3InterfaceService(const S3Params &p) : restfulService(NULL), params(p) {
    xmlInitParser();
}

S3InterfaceService::~S3InterfaceService() {
    // Cleanup function for the XML library.
    xmlCleanupParser();
}

Response S3InterfaceService::getResponseWithRetries(const string &url, HTTPHeaders &headers,
                                                    uint64_t retries) {
    string message;
    uint64_t retry = retries;

    while (retry--) {
        try {
            return this->restfulService->get(url, headers);
        } catch (S3ConnectionError &e) {
            message = e.getMessage();
            if (S3QueryIsAbortInProgress()) {
                S3_DIE(S3QueryAbort, "Downloading is interrupted");
            }
            S3WARN("Failed to get a good response in GET from '%s', retrying ...", url.c_str());
        }
    };

    S3_DIE(S3FailedAfterRetry, url, retries, message);
};

Response S3InterfaceService::putResponseWithRetries(const string &url, HTTPHeaders &headers,
                                                    S3VectorUInt8 &data, uint64_t retries) {
    string message;
    uint64_t retry = retries;

    while (retry--) {
        try {
            return this->restfulService->put(url, headers, data);
        } catch (S3ConnectionError &e) {
            message = e.getMessage();
            if (S3QueryIsAbortInProgress()) {
                S3_DIE(S3QueryAbort, "Uploading is interrupted");
            }
            S3WARN("Failed to get a good response in PUT from '%s', retrying ...", url.c_str());
        }
    };

    S3_DIE(S3FailedAfterRetry, url, retries, message);
};

Response S3InterfaceService::postResponseWithRetries(const string &url, HTTPHeaders &headers,
                                                     const vector<uint8_t> &data,
                                                     uint64_t retries) {
    string message;
    uint64_t retry = retries;

    while (retry--) {
        try {
            return this->restfulService->post(url, headers, data);
        } catch (S3ConnectionError &e) {
            message = e.getMessage();
            if (S3QueryIsAbortInProgress()) {
                S3_DIE(S3QueryAbort, "Uploading is interrupted");
            }
            S3WARN("Failed to get a good response in POST from '%s', retrying ...", url.c_str());
        }
    };

    S3_DIE(S3FailedAfterRetry, url, retries, message);
}

bool S3InterfaceService::isKeyExisted(ResponseCode code) {
    return isSuccessfulResponse(code);
}

ResponseCode S3InterfaceService::headResponseWithRetries(const string &url, HTTPHeaders &headers,
                                                         uint64_t retries) {
    string message;
    uint64_t retry = retries;

    while (retry--) {
        try {
            return this->restfulService->head(url, headers);
        } catch (S3ConnectionError &e) {
            message = e.getMessage();
            if (S3QueryIsAbortInProgress()) {
                S3_DIE(S3QueryAbort, "Uploading is interrupted");
            }

            S3WARN("Failed to get a good response in HEAD from '%s', retrying ...", url.c_str());
        }
    };

    S3_DIE(S3FailedAfterRetry, url, retries, message);
}

Response S3InterfaceService::deleteRequestWithRetries(const string &url, HTTPHeaders &headers,
                                                      uint64_t retries) {
    string message;
    uint64_t retry = retries;

    while (retry--) {
        try {
            return this->restfulService->deleteRequest(url, headers);
        } catch (S3ConnectionError &e) {
            message = e.getMessage();
            if (S3QueryIsAbortInProgress()) {
                S3_DIE(S3QueryAbort, "Uploading is interrupted");
            }
            S3WARN("Failed to get a good response in DELETE from '%s', retrying ...", url.c_str());
        }
    };

    S3_DIE(S3FailedAfterRetry, url, retries, message);
};

xmlParserCtxtPtr S3InterfaceService::getXMLContext(Response &response) {
    xmlParserCtxtPtr xmlptr =
        xmlCreatePushParserCtxt(NULL, NULL, (const char *)(response.getRawData().data()),
                                response.getRawData().size(), "getXMLContext.xml");
    if (xmlptr != NULL) {
        xmlParseChunk(xmlptr, "", 0, 1);
    } else {
        S3ERROR("Failed to create XML parser context");
    }
    return xmlptr;
}

// require curl 7.17 higher
// http://docs.aws.amazon.com/AmazonS3/latest/API/RESTBucketGET.html
Response S3InterfaceService::getBucketResponse(const S3Url &s3Url, const string &encodedQuery) {
    HTTPHeaders headers;
    headers.Add(HOST, s3Url.getHostForCurl());
    headers.Add(X_AMZ_CONTENT_SHA256,
                "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");  // sha256hex
                                                                                      // of empty
                                                                                      // string

    SignRequestV4("GET", &headers, s3Url.getRegion(), s3Url.getPathForCurl(), encodedQuery,
                  this->params.getCred());

    stringstream urlWithQuery;
    urlWithQuery << s3Url.getFullUrlForCurl() << "?" << encodedQuery;

    return this->getResponseWithRetries(urlWithQuery.str(), headers);
}

bool S3InterfaceService::parseBucketXML(ListBucketResult *result, xmlParserCtxtPtr xmlcontext,
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
                    result->contents.emplace_back(key, size);
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

// ListBucket lists all keys in given bucket with given prefix.
//
// Return NULL when there is failure due to network instability or
// service unstable, so that caller could retry.
//
// Caller should delete returned object.
ListBucketResult S3InterfaceService::listBucket(S3Url &s3Url) {
    ListBucketResult result;

    string marker = "";
    string encodedPrefix = s3Url.getPrefix();
    FindAndReplace(encodedPrefix, "/", "%2F");
    do {
        // To get next set(up to 1000) keys in one iteration.
        // S3 requires query parameters specified alphabetically.

        // marker and prefix are used as the values of query parameters here
        // so URI encode their whole string, "/" also.

        // transfer /bucket/prefix to /bucket/?prefix=prefix because we need to "GET" a real thing
        stringstream querySs;
        if (!marker.empty()) {
            querySs << "marker=" << UriEncode(marker);
        }

        if (!encodedPrefix.empty()) {
            querySs << (marker.empty() ? "prefix=" : "&prefix=") << encodedPrefix;
        }
        s3Url.setPrefix("");
        string queryStr = querySs.str();

        Response resp = getBucketResponse(s3Url, queryStr);

        if (resp.getStatus() == RESPONSE_OK) {
            xmlParserCtxtPtr xmlContext = getXMLContext(resp);
            XMLContextHolder holder(xmlContext);
            if (parseBucketXML(&result, xmlContext, marker)) {
                continue;
            }
        } else if (resp.getStatus() == RESPONSE_ERROR) {
            S3MessageParser s3msg(resp);
            S3_DIE(S3LogicError, s3msg.getCode(), s3msg.getMessage());
        } else {
            S3_DIE(S3RuntimeError, "unexpected response status");
        }

        return result;
    } while (!marker.empty());

    return result;
}

uint64_t S3InterfaceService::fetchData(uint64_t offset, S3VectorUInt8 &data, uint64_t len,
                                       const S3Url &s3Url) {
    HTTPHeaders headers;

    char rangeBuf[S3_RANGE_HEADER_STRING_LEN] = {0};
    snprintf(rangeBuf, sizeof(rangeBuf), "bytes=%" PRIu64 "-%" PRIu64, offset, offset + len - 1);
    headers.Add(HOST, s3Url.getHostForCurl());
    headers.Add(RANGE, rangeBuf);
    headers.Add(X_AMZ_CONTENT_SHA256,
                "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");  // sha256hex
                                                                                      // of empty
                                                                                      // string

    SignRequestV4("GET", &headers, s3Url.getRegion(), s3Url.getPathForCurl(), "",
                  this->params.getCred());

    Response resp = this->getResponseWithRetries(s3Url.getFullUrlForCurl(), headers);
    if (resp.getStatus() == RESPONSE_OK) {
        data.swap(resp.getRawData());
        S3_CHECK_OR_DIE(data.size() == len, S3PartialResponseError, len, data.size());
        return data.size();
    } else if (resp.getStatus() == RESPONSE_ERROR) {
        S3MessageParser s3msg(resp);
        S3_DIE(S3LogicError, s3msg.getCode(), s3msg.getMessage());
    } else {
        S3_DIE(S3RuntimeError, "unexpected response status");
    }
}

S3CompressionType S3InterfaceService::checkCompressionType(const S3Url &s3Url) {
    HTTPHeaders headers;

    char rangeBuf[S3_RANGE_HEADER_STRING_LEN] = {0};
    snprintf(rangeBuf, sizeof(rangeBuf), "bytes=%d-%d", 0, S3_MAGIC_BYTES_NUM - 1);

    headers.Add(HOST, s3Url.getHostForCurl());
    headers.Add(RANGE, rangeBuf);
    headers.Add(X_AMZ_CONTENT_SHA256,
                "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");  // sha256hex
                                                                                      // of empty
                                                                                      // string

    SignRequestV4("GET", &headers, s3Url.getRegion(), s3Url.getPathForCurl(), "",
                  this->params.getCred());

    Response resp = this->getResponseWithRetries(s3Url.getFullUrlForCurl(), headers);
    if (resp.getStatus() == RESPONSE_OK) {
        S3VectorUInt8 &responseData = resp.getRawData();
        if (responseData.size() < S3_MAGIC_BYTES_NUM) {
            return S3_COMPRESSION_PLAIN;
        }

        S3_CHECK_OR_DIE(responseData.size() == S3_MAGIC_BYTES_NUM, S3PartialResponseError,
                        S3_MAGIC_BYTES_NUM, responseData.size());

        if ((responseData[0] == 0x1f) && (responseData[1] == 0x8b)) {
            return S3_COMPRESSION_GZIP;
        }
    } else if (resp.getStatus() == RESPONSE_ERROR) {
        S3MessageParser s3msg(resp);
        S3_DIE(S3LogicError, s3msg.getCode(), s3msg.getMessage());
    } else {
        S3_DIE(S3RuntimeError, "unexpected response status");
    }

    return S3_COMPRESSION_PLAIN;
}

bool S3InterfaceService::checkKeyExistence(const S3Url &s3Url) {
    HTTPHeaders headers;

    headers.Add(HOST, s3Url.getHostForCurl());
    headers.Add(X_AMZ_CONTENT_SHA256,
                "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");  // sha256hex
                                                                                      // of empty
                                                                                      // string

    SignRequestV4("HEAD", &headers, s3Url.getRegion(), s3Url.getPathForCurl(), "",
                  this->params.getCred());

    return isKeyExisted(headResponseWithRetries(s3Url.getFullUrlForCurl(), headers));
}

string S3InterfaceService::getUploadId(const S3Url &s3Url) {
    HTTPHeaders headers;

    headers.Add(HOST, s3Url.getHostForCurl());
    headers.Disable(CONTENTTYPE);
    headers.Disable(CONTENTLENGTH);
    headers.Add(X_AMZ_CONTENT_SHA256,
                "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");  // sha256hex
                                                                                      // of empty
                                                                                      // string
    if (params.getSSEType() == SSE_S3) {
        headers.Add(X_AMZ_SERVER_SIDE_ENCRYPTION, "AES256");
    }

    SignRequestV4("POST", &headers, s3Url.getRegion(), s3Url.getPathForCurl(),
                  "uploads=", this->params.getCred());

    stringstream urlWithQuery;
    urlWithQuery << s3Url.getFullUrlForCurl() << "?uploads";

    Response resp = this->postResponseWithRetries(urlWithQuery.str(), headers, vector<uint8_t>());
    S3MessageParser s3msg(resp);

    if (resp.getStatus() == RESPONSE_OK) {
        return s3msg.parseS3Tag("UploadId");
    } else if (resp.getStatus() == RESPONSE_ERROR) {
        S3MessageParser s3msg(resp);
        S3_DIE(S3LogicError, s3msg.getCode(), s3msg.getMessage());
    } else {
        S3_DIE(S3RuntimeError, "unexpected response status");
    }
}

string S3InterfaceService::uploadPartOfData(S3VectorUInt8 &data, const S3Url &s3Url,
                                            uint64_t partNumber, const string &uploadId) {
    HTTPHeaders headers;
    stringstream queryString;

    headers.Add(HOST, s3Url.getHostForCurl());

    char contentSha256[SHA256_DIGEST_STRING_LENGTH];  // 65
    sha256_hex((const char *)data.data(), data.size(), contentSha256);
    headers.Add(X_AMZ_CONTENT_SHA256, contentSha256);

    headers.Add(CONTENTTYPE, "text/plain");
    // headers.Add(CONTENTLENGTH, std::to_string((unsigned long long)data.size()));

    queryString << "partNumber=" << partNumber << "&uploadId=" << uploadId;

    SignRequestV4("PUT", &headers, s3Url.getRegion(), s3Url.getPathForCurl(), queryString.str(),
                  this->params.getCred());

    stringstream urlWithQuery;
    urlWithQuery << s3Url.getFullUrlForCurl() << "?partNumber=" << partNumber
                 << "&uploadId=" << uploadId;

    Response resp = this->putResponseWithRetries(urlWithQuery.str(), headers, data);
    if (resp.getStatus() == RESPONSE_OK) {
        string headers(resp.getRawHeaders().begin(), resp.getRawHeaders().end());

        uint64_t etagStartPos = headers.find("ETag: ") + 6;
        string etagToEnd = headers.substr(etagStartPos);
        // RFC 2616 states "HTTP/1.1 defines the sequence CR LF as the end-of-line
        // marker for all protocol elements except the entity-body"
        uint64_t etagStrLen = etagToEnd.find("\r");

        return etagToEnd.substr(0, etagStrLen);
    } else if (resp.getStatus() == RESPONSE_ERROR) {
        S3MessageParser s3msg(resp);
        S3_DIE(S3LogicError, s3msg.getCode(), s3msg.getMessage());
    } else {
        S3_DIE(S3RuntimeError, "unexpected response status");
    }
}

bool S3InterfaceService::completeMultiPart(const S3Url &s3Url, const string &uploadId,
                                           const vector<string> &etagArray) {
    HTTPHeaders headers;
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

    headers.Add(HOST, s3Url.getHostForCurl());
    headers.Add(CONTENTTYPE, "application/xml");

    char contentSha256[SHA256_DIGEST_STRING_LENGTH];  // 65
    sha256_hex(body.str().c_str(), contentSha256);
    headers.Add(X_AMZ_CONTENT_SHA256, contentSha256);

    headers.Add(CONTENTLENGTH, std::to_string((unsigned long long)body.str().length()));

    queryString << "uploadId=" << uploadId;

    SignRequestV4("POST", &headers, s3Url.getRegion(), s3Url.getPathForCurl(), queryString.str(),
                  this->params.getCred());

    stringstream urlWithQuery;
    urlWithQuery << s3Url.getFullUrlForCurl() << "?uploadId=" << uploadId;

    string bodyString = body.str();
    Response resp = this->postResponseWithRetries(
        urlWithQuery.str(), headers, vector<uint8_t>(bodyString.begin(), bodyString.end()));

    if (resp.getStatus() == RESPONSE_OK) {
        return true;
    } else if (resp.getStatus() == RESPONSE_ERROR) {
        S3MessageParser s3msg(resp);
        S3_DIE(S3LogicError, s3msg.getCode(), s3msg.getMessage());
    } else {
        S3_DIE(S3RuntimeError, "unexpected response status");
    }
}

bool S3InterfaceService::abortUpload(const S3Url &s3Url, const string &uploadId) {
    HTTPHeaders headers;
    stringstream queryString;

    headers.Add(HOST, s3Url.getHostForCurl());
    headers.Disable(CONTENTTYPE);
    headers.Disable(CONTENTLENGTH);
    headers.Add(X_AMZ_CONTENT_SHA256,
                "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");  // sha256hex
                                                                                      // of empty
                                                                                      // string

    queryString << "uploadId=" << uploadId;

    // DELETE /ObjectName?uploadId=UploadId HTTP/1.1
    SignRequestV4("DELETE", &headers, s3Url.getRegion(), s3Url.getPathForCurl(), queryString.str(),
                  this->params.getCred());

    stringstream urlWithQuery;
    urlWithQuery << s3Url.getFullUrlForCurl() << "?uploadId=" << uploadId;

    Response resp = this->deleteRequestWithRetries(urlWithQuery.str(), headers);

    if (resp.getStatus() == RESPONSE_OK) {
        return true;
    } else if (resp.getStatus() == RESPONSE_ERROR) {
        S3MessageParser s3msg(resp);
        S3_DIE(S3LogicError, s3msg.getCode(), s3msg.getMessage());
    } else {
        S3_DIE(S3RuntimeError, "unexpected response status");
    }
}
