#include "s3restful_service.h"

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
    if (S3QueryIsAbortInProgress()) {
        return -1;
    }

    size_t realsize = size * nmemb;
    Response *resp = (Response *)userp;
    resp->appendDataBuffer(ptr, realsize);
    return realsize;
}

// cURL's write function callback, only used by DELETE request when query is canceled.
// It shouldn't be interrupted.
size_t RESTfulServiceAbortFuncCallback(char *ptr, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    Response *resp = (Response *)userp;
    resp->appendDataBuffer(ptr, realsize);
    return realsize;
}

// curl's headers write function callback.
size_t RESTfulServiceHeadersWriteFuncCallback(char *ptr, size_t size, size_t nmemb, void *userp) {
    if (S3QueryIsAbortInProgress()) {
        return -1;
    }

    size_t realsize = size * nmemb;
    Response *resp = (Response *)userp;
    resp->appendHeadersBuffer(ptr, realsize);
    return realsize;
}

// curl's reading function callback.
size_t RESTfulServiceReadFuncCallback(char *ptr, size_t size, size_t nmemb, void *userp) {
    if (S3QueryIsAbortInProgress()) {
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
Response S3RESTfulService::get(const string &url, HTTPHeaders &headers, const S3Params &params) {
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
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, params.getLowSpeedLimit());
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, params.getLowSpeedTime());

    if (params.isDebugCurl()) {
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    }

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        S3ERROR("curl_easy_perform() failed: %s", curl_easy_strerror(res));

        response.clearBuffers();
        response.setStatus(RESPONSE_FAIL);
        response.setMessage(string("Server connection failed: ").append(curl_easy_strerror(res)));

    } else {
        long responseCode;
        // Get the HTTP response status code from HTTP header
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

        // 2XX are successful response. Here we deal with 200 (OK) and 206 (partial content)
        // firstly.
        if (isSuccessfulResponse(responseCode)) {
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

Response S3RESTfulService::put(const string &url, HTTPHeaders &headers, const S3Params &params,
                               const vector<uint8_t> &data) {
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
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)data.size());
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

    curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, RESTfulServiceHeadersWriteFuncCallback);

    // consider low speed as timeout
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, params.getLowSpeedLimit());
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, params.getLowSpeedTime());

    if (params.isDebugCurl()) {
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    }

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        S3ERROR("curl_easy_perform() failed: %s", curl_easy_strerror(res));

        if (S3QueryIsAbortInProgress()) {
            response.setStatus(RESPONSE_ABORT);
            response.setMessage("Query cancelled by user");
        } else {
            response.setStatus(RESPONSE_FAIL);
            response.setMessage(
                string("Server connection failed: ").append(curl_easy_strerror(res)));
        }

        response.clearBuffers();

    } else {
        long responseCode;
        // Get the HTTP response status code from HTTP header
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

        // 2XX are successful response. Here we deal with 200 (OK) and 206 (partial content)
        // firstly.
        if (isSuccessfulResponse(responseCode)) {
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

Response S3RESTfulService::post(const string &url, HTTPHeaders &headers, const S3Params &params,
                                const vector<uint8_t> &data) {
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

    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    UploadData uploadData(data);
    curl_easy_setopt(curl, CURLOPT_READDATA, (void *)&uploadData);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, RESTfulServiceReadFuncCallback);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)data.size());

    // consider low speed as timeout
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, params.getLowSpeedLimit());
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, params.getLowSpeedTime());

    if (params.isDebugCurl()) {
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    }

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        S3ERROR("curl_easy_perform() failed: %s", curl_easy_strerror(res));

        if (S3QueryIsAbortInProgress()) {
            response.setStatus(RESPONSE_ABORT);
            response.setMessage("Query cancelled by user");
        } else {
            response.setStatus(RESPONSE_FAIL);
            response.setMessage(
                string("Server connection failed: ").append(curl_easy_strerror(res)));
        }

        response.clearBuffers();

    } else {
        long responseCode;
        // Get the HTTP response status code from HTTP header
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

        // 2XX are successful response. Here we deal with 200 (OK) and 206 (partial content)
        // firstly.
        if (isSuccessfulResponse(responseCode)) {
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
                                    const S3Params &params) {
    ResponseCode responseCode = HeadResponseFail;

    CURL *curl = curl_easy_init();
    CHECK_OR_DIE_MSG(curl != NULL, "%s", "Failed to create curl handler");

    headers.CreateList();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.GetList());

    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "HEAD");
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

    if (params.isDebugCurl()) {
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

Response S3RESTfulService::deleteRequest(const string &url, HTTPHeaders &headers,
                                         const S3Params &params) {
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
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, RESTfulServiceAbortFuncCallback);

    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");

    vector<uint8_t> data;

    UploadData uploadData(data);
    curl_easy_setopt(curl, CURLOPT_READDATA, (void *)&uploadData);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, RESTfulServiceReadFuncCallback);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)data.size());

    // consider low speed as timeout
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, params.getLowSpeedLimit());
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, params.getLowSpeedTime());

    if (params.isDebugCurl()) {
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    }

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        S3ERROR("curl_easy_perform() failed: %s", curl_easy_strerror(res));

        response.clearBuffers();
        response.setStatus(RESPONSE_FAIL);
        response.setMessage(string("Server connection failed: ").append(curl_easy_strerror(res)));

    } else {
        long responseCode;
        // Get the HTTP response status code from HTTP header
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

        // 2XX are successful response. Here we deal with 200 (OK) 204 (no content), and 206
        // (partial content) firstly.
        if (isSuccessfulResponse(responseCode)) {
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
