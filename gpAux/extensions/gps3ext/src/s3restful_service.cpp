#include <unistd.h>

#define __STDC_FORMAT_MACROS
#include <curl/curl.h>
#include <inttypes.h>
#include <map>
#include <string>

#include "s3http_headers.h"
#include "s3log.h"
#include "s3macros.h"
#include "s3restul_service.h"

using namespace std;

S3RESTfulService::S3RESTfulService() {}

S3RESTfulService::~S3RESTfulService() {}

size_t RESTfulServiceCallback(char *ptr, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    Response *resp = (Response *)userp;
    resp->appendBuffer(ptr, realsize);
    return realsize;
}

Response S3RESTfulService::get(const string &url, HTTPHeaders &headers,
                               const map<string, string> &params) {
    Response response;

    CURL *curl = curl_easy_init();
    CHECK_OR_DIE_MSG(curl != NULL, "%s", "Failed to create curl handler");

    headers.CreateList();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.GetList());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, RESTfulServiceCallback);

    map<string, string>::const_iterator iter = params.find("debug");
    if (iter != params.end() && iter->second == "true") {
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    }
#if DEBUG_S3_CURL
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
#endif

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        S3ERROR("curl_easy_perform() failed: %s", curl_easy_strerror(res));

        response.clearBuffer();
        response.setStatus(FAIL);
        response.setMessage(
            string("failed to talk to s3 service ").append(curl_easy_strerror(res)));
    } else {
        response.setStatus(OK);
        response.setMessage("Success");
    }

    curl_easy_cleanup(curl);
    headers.FreeList();

    return response;
}
