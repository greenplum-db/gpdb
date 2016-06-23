#include <unistd.h>

#define __STDC_FORMAT_MACROS
#include <curl/curl.h>
#include <inttypes.h>
#include <map>
#include <string>

#include "s3http_headers.h"
#include "s3log.h"
#include "s3macros.h"
#include "s3restful_service.h"

using namespace std;

S3RESTfulService::S3RESTfulService() {
}

S3RESTfulService::~S3RESTfulService() {
}

// curl's write function callback.
size_t RESTfulServiceCallback(char *ptr, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    Response *resp = (Response *)userp;
    resp->appendBuffer(ptr, realsize);
    return realsize;
}

// get() will execute HTTP GET RESTful API with given url/headers/params,
// and return raw response content.
//
// This method does not care about response format, caller need to handle
// response format accordingly.
Response S3RESTfulService::get(const string &url, HTTPHeaders &headers,
                               const map<string, string> &params) {
    Response response;

    CURL *curl = curl_easy_init();
    CHECK_OR_DIE_MSG(curl != NULL, "%s", "Failed to create curl handler");

    headers.CreateList();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.GetList());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, RESTfulServiceCallback);

    // consider low speed as timeout
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, s3ext_low_speed_limit);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, s3ext_low_speed_time);

    map<string, string>::const_iterator iter = params.find("debug");
    if (iter != params.end() && iter->second == "true") {
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    }

    if (s3ext_debug_curl) {
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    }

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        S3ERROR("curl_easy_perform() failed: %s", curl_easy_strerror(res));

        response.clearBuffer();
        response.setStatus(RESPONSE_FAIL);
        response.setMessage(
            string("Failed to talk to s3 service ").append(curl_easy_strerror(res)));

    } else {
        long responseCode;
        // Get the HTTP response status code from HTTP header
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

        if (responseCode != 200) {  // Server error, set status to RESPONSE_ERROR
            stringstream sstr;

            sstr << "S3 server returned error, error code is " << responseCode;
            response.setStatus(RESPONSE_ERROR);
            response.setMessage(sstr.str());
        } else {
            response.setStatus(RESPONSE_OK);
            response.setMessage("Success");
        }
    }

    curl_easy_cleanup(curl);
    headers.FreeList();

    return response;
}
