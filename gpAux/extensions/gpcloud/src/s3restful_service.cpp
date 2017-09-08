#include "s3restful_service.h"

S3RESTfulService::S3RESTfulService()
    : lowSpeedLimit(0),
      lowSpeedTime(0),
      proxy(""),
      debugCurl(false),
      verifyCert(true),
      chunkBufferSize(64 * 1024) {
}

S3RESTfulService::S3RESTfulService(const string &proxy)
    : lowSpeedLimit(0),
      lowSpeedTime(0),
      proxy(proxy),
      debugCurl(false),
      verifyCert(true),
      chunkBufferSize(64 * 1024) {
}

S3RESTfulService::S3RESTfulService(const S3Params &params)
    : s3MemContext(const_cast<S3MemoryContext &>(params.getMemoryContext())) {
    // This function is not thread safe, must NOT call it when any other
    // threads are running, that is, do NOT put it in threads.
    curl_global_init(CURL_GLOBAL_ALL);

    this->lowSpeedLimit = params.getLowSpeedLimit();
    this->lowSpeedTime = params.getLowSpeedTime();
    this->debugCurl = params.isDebugCurl();
    this->chunkBufferSize = params.getChunkSize();
    this->verifyCert = params.isVerifyCert();
    this->proxy = params.getProxy();
}

S3RESTfulService::~S3RESTfulService() {
    // This function is not thread safe, must NOT call it when any other
    // threads are running, that is, do NOT put it in threads.
    curl_global_cleanup();
}

// curl's write function callback.
static size_t RESTfulServiceWriteFuncCallback(char *ptr, size_t size, size_t nmemb, void *userp) {
    if (S3QueryIsAbortInProgress()) {
        return 0;
    }

    size_t realsize = size * nmemb;
    Response *resp = (Response *)userp;
    resp->appendDataBuffer(ptr, realsize);
    return realsize;
}

// cURL's write function callback, only used by DELETE request when query is canceled.
// It shouldn't be interrupted.
static size_t RESTfulServiceAbortFuncCallback(char *ptr, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    Response *resp = (Response *)userp;
    resp->appendDataBuffer(ptr, realsize);
    return realsize;
}

// curl's headers write function callback.
static size_t RESTfulServiceHeadersWriteFuncCallback(char *ptr, size_t size, size_t nmemb,
                                                     void *userp) {
    if (S3QueryIsAbortInProgress()) {
        return 0;
    }

    size_t realsize = size * nmemb;
    Response *resp = (Response *)userp;
    resp->appendHeadersBuffer(ptr, realsize);
    return realsize;
}

// curl's reading function callback.
static size_t RESTfulServiceReadFuncCallback(char *ptr, size_t size, size_t nmemb, void *userp) {
    if (S3QueryIsAbortInProgress()) {
        return CURL_READFUNC_ABORT;
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

struct CURLWrapper {
    CURLWrapper(const string &url, curl_slist *headers, uint64_t lowSpeedLimit,
                uint64_t lowSpeedTime, bool debugCurl, string proxy) {
        curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, lowSpeedLimit);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, lowSpeedTime);

        if (debugCurl) {
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        }

        if (!proxy.empty()) {
            curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
        }
    }
    ~CURLWrapper() {
        curl_easy_cleanup(curl);
    }
    CURL *curl;
};

void S3RESTfulService::performCurl(CURL *curl, Response &response) {
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        if (res == CURLE_COULDNT_RESOLVE_HOST) {
            S3_DIE(S3ResolveError, curl_easy_strerror(res));
        } else {
            S3_DIE(S3ConnectionError, curl_easy_strerror(res));
        }
    } else {
        long responseCode;
        // Get the HTTP response status code from HTTP header
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

        if (responseCode == 500) {
            S3_DIE(S3ConnectionError, "Server temporary unavailable");
        }

        response.FillResponse(responseCode);
    }
}

// get() will execute HTTP GET RESTful API with given url/headers/params,
// and return raw response content.
//
// This method does not care about response format, caller need to handle
// response format accordingly.
Response S3RESTfulService::get(const string &url, HTTPHeaders &headers) {
    Response response(RESPONSE_ERROR, this->s3MemContext);
    response.getRawData().reserve(this->chunkBufferSize);

    headers.CreateList();
    CURLWrapper wrapper(url, headers.GetList(), this->lowSpeedLimit, this->lowSpeedTime,
                        this->debugCurl, this->proxy);
    CURL *curl = wrapper.curl;

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, RESTfulServiceWriteFuncCallback);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, this->verifyCert);

    this->performCurl(curl, response);

    return response;
}

Response S3RESTfulService::put(const string &url, HTTPHeaders &headers, const S3VectorUInt8 &data) {
    Response response(RESPONSE_ERROR);

    headers.CreateList();
    CURLWrapper wrapper(url, headers.GetList(), this->lowSpeedLimit, this->lowSpeedTime,
                        this->debugCurl, this->proxy);
    CURL *curl = wrapper.curl;

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, RESTfulServiceWriteFuncCallback);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, this->verifyCert);

    UploadData uploadData(data);
    curl_easy_setopt(curl, CURLOPT_READDATA, (void *)&uploadData);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, RESTfulServiceReadFuncCallback);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)data.size());
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

    curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, RESTfulServiceHeadersWriteFuncCallback);

    this->performCurl(curl, response);

    return response;
}

Response S3RESTfulService::post(const string &url, HTTPHeaders &headers,
                                const vector<uint8_t> &data) {
    Response response(RESPONSE_ERROR);

    headers.CreateList();
    CURLWrapper wrapper(url, headers.GetList(), this->lowSpeedLimit, this->lowSpeedTime,
                        this->debugCurl, this->proxy);
    CURL *curl = wrapper.curl;

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, RESTfulServiceWriteFuncCallback);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, this->verifyCert);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    S3VectorUInt8 s3data(data);
    UploadData uploadData(s3data);
    curl_easy_setopt(curl, CURLOPT_READDATA, (void *)&uploadData);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, RESTfulServiceReadFuncCallback);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)data.size());

    this->performCurl(curl, response);

    return response;
}

// head() will execute HTTP HEAD RESTful API with given url/headers/params, and return the HTTP
// response code.
//
// Currently, this method only return the HTTP code, will be extended if needed in the future
// implementation.
ResponseCode S3RESTfulService::head(const string &url, HTTPHeaders &headers) {
    Response response(RESPONSE_ERROR);

    headers.CreateList();
    CURLWrapper wrapper(url, headers.GetList(), this->lowSpeedLimit, this->lowSpeedTime,
                        this->debugCurl, this->proxy);
    CURL *curl = wrapper.curl;

    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "HEAD");
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, this->verifyCert);

    this->performCurl(curl, response);

    return response.getResponseCode();
}

Response S3RESTfulService::deleteRequest(const string &url, HTTPHeaders &headers) {
    Response response(RESPONSE_ERROR);

    headers.CreateList();
    CURLWrapper wrapper(url, headers.GetList(), this->lowSpeedLimit, this->lowSpeedTime,
                        this->debugCurl, this->proxy);
    CURL *curl = wrapper.curl;

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, RESTfulServiceAbortFuncCallback);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, this->verifyCert);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");

    S3VectorUInt8 data;
    UploadData uploadData(data);
    curl_easy_setopt(curl, CURLOPT_READDATA, (void *)&uploadData);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, RESTfulServiceReadFuncCallback);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)data.size());

    this->performCurl(curl, response);

    return response;
}
