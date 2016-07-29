#include <map>
#include <string>

#include <unistd.h>
#define __STDC_FORMAT_MACROS
#include <curl/curl.h>
#include <inttypes.h>
#include <string.h>

#include "gpcommon.h"
#include "s3http_headers.h"
#include "s3log.h"
#include "s3macros.h"
#include "s3restful_service.h"

using namespace std;

S3RESTfulService::S3RESTfulService() {
    // This function is not thread safe, must NOT call it when any other
    // threads are running, that is, do NOT put it in threads.
    curl_global_init(CURL_GLOBAL_ALL);
}

S3RESTfulService::~S3RESTfulService() {
    // This function is not thread safe, must NOT call it when any other
    // threads are running, that is, do NOT put it in threads.
    curl_global_cleanup();
}

// curl's write function callback.
size_t RESTfulServiceWriteFuncCallback(char *ptr, size_t size, size_t nmemb, void *userp) {
    if (QueryCancelPending) {
        return -1;
    }

    size_t realsize = size * nmemb;
    Response *resp = (Response *)userp;
    resp->appendBuffer(ptr, realsize);
    return realsize;
}

// curl's reading function callback.
size_t RESTfulServiceReadFuncCallback(char *ptr, size_t size, size_t nmemb, void *userp) {
    if (QueryCancelPending) {
        return -1;
    }

    UploadData *data = (UploadData *)userp;
    uint64_t dataLeft = data->buffer.size() - data->currentPosition;

    size_t requestedSize = size * nmemb;
    size_t copiedItemNum = requestedSize < dataLeft ? nmemb : (dataLeft / size);
    size_t copiedDataSize = copiedItemNum * size;

    if (copiedDataSize == 0) return 0;

    memcpy(ptr, data->buffer.data() + data->currentPosition, copiedDataSize);

    data->currentPosition += copiedDataSize;

    return copiedItemNum;
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
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, RESTfulServiceWriteFuncCallback);

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

        // 2XX are successful response. Here we deal with 200 (OK) and 206 (partial content)
        // firstly.
        if ((responseCode == 200) || (responseCode == 206)) {
            response.setStatus(RESPONSE_OK);
            response.setMessage("Success");
        } else {  // Server error, set status to RESPONSE_ERROR
            stringstream sstr;

            sstr << "S3 server returned error, error code is " << responseCode;
            response.setStatus(RESPONSE_ERROR);
            response.setMessage(sstr.str());
        }
    }

    curl_easy_cleanup(curl);
    headers.FreeList();

    return response;
}

Response S3RESTfulService::put(const string &url, HTTPHeaders &headers,
                               const map<string, string> &params, const vector<uint8_t> &data) {
    Response response;

    CURL *curl = curl_easy_init();
    CHECK_OR_DIE_MSG(curl != NULL, "%s", "Failed to create curl handler");

    headers.CreateList();

    /* options for downloading */
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.GetList());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, RESTfulServiceWriteFuncCallback);

    /* options for uploading */
    UploadData uploadData(data);
    curl_easy_setopt(curl, CURLOPT_READDATA, (void *)&uploadData);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, RESTfulServiceReadFuncCallback);
    /* CURLOPT_INFILESIZE_LARGE for sending files larger than 2GB.*/
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)data.size());
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

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

        // 2XX are successful response. Here we deal with 200 (OK) and 206 (partial content)
        // firstly.
        if ((responseCode == 200) || (responseCode == 206)) {
            response.setStatus(RESPONSE_OK);
            response.setMessage("Success");
        } else {  // Server error, set status to RESPONSE_ERROR
            stringstream sstr;

            sstr << "S3 server returned error, error code is " << responseCode;
            response.setStatus(RESPONSE_ERROR);
            response.setMessage(sstr.str());
        }
    }

    curl_easy_cleanup(curl);
    headers.FreeList();

    return response;
}

// head() will execute HTTP HEAD RESTful API with given url/headers/params, and return the HTTP
// response code.
//
// Currently, this method only return the HTTP code, will be extended if needed in the future
// implementation.
ResponseCode S3RESTfulService::head(const string &url, HTTPHeaders &headers,
                                    const map<string, string> &params) {
    ResponseCode responseCode = -1;

    CURL *curl = curl_easy_init();
    CHECK_OR_DIE_MSG(curl != NULL, "%s", "Failed to create curl handler");

    headers.CreateList();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.GetList());

    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "HEAD");
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

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
    } else {
        // Get the HTTP response status code from HTTP header
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
    }

    curl_easy_cleanup(curl);
    headers.FreeList();

    return responseCode;
}
