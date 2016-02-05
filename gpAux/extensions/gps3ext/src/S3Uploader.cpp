#include "S3Uploader.h"
#include "S3Common.h"
#include "S3Downloader.h"

#include <algorithm>  // std::min
#include <sstream>
#include <iostream>

#include "utils.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <fcntl.h>
#include <sys/stat.h>

#include <string>
using std::string;
using std::stringstream;

#ifdef DEBUG_CURL
struct debug_data {
    char trace_ascii; /* 1 or 0 */
};

static void dump_debug_data(const char *text, FILE *stream, unsigned char *ptr,
                            size_t size, char nohex) {
    size_t i;
    size_t c;

    unsigned int width = 0x10;

    if (nohex) /* without the hex output, we can fit more on screen */
        width = 0x40;

    fprintf(stream, "%s, %10.10ld bytes (0x%8.8lx)\n", text, (long)size,
            (long)size);

    for (i = 0; i < size; i += width) {
        fprintf(stream, "%4.4lx: ", (long)i);

        if (!nohex) {
            /* hex not disabled, show it */
            for (c = 0; c < width; c++)
                if (i + c < size)
                    fprintf(stream, "%02x ", ptr[i + c]);
                else
                    fputs("   ", stream);
        }

        for (c = 0; (c < width) && (i + c < size); c++) {
            /* check for 0D0A; if found, skip past and start a new line of
             * output */
            if (nohex && (i + c + 1 < size) && ptr[i + c] == 0x0D &&
                ptr[i + c + 1] == 0x0A) {
                i += (c + 2 - width);
                break;
            }
            fprintf(stream, "%c", (ptr[i + c] >= 0x20) && (ptr[i + c] < 0x80)
                                      ? ptr[i + c]
                                      : '.');
            /* check again for 0D0A, to avoid an extra \n if it's at width */
            if (nohex && (i + c + 2 < size) && ptr[i + c + 1] == 0x0D &&
                ptr[i + c + 2] == 0x0A) {
                i += (c + 3 - width);
                break;
            }
        }
        fputc('\n', stream); /* newline */
    }
    fflush(stream);
}

static int trace_debug_data(CURL *handle, curl_infotype type, char *data,
                            size_t size, void *userp) {
    struct debug_data *config = (struct debug_data *)userp;
    const char *text;
    (void)handle; /* prevent compiler warning */

    switch (type) {
        case CURLINFO_TEXT:
            fprintf(stderr, "== Info: %s", data);
        default: /* in case a new one is introduced to shock us */
            return 0;

        case CURLINFO_HEADER_OUT:
            text = "=> Send header";
            break;
        case CURLINFO_DATA_OUT:
            text = "=> Send data";
            break;
        case CURLINFO_SSL_DATA_OUT:
            text = "=> Send SSL data";
            break;
        case CURLINFO_HEADER_IN:
            text = "<= Recv header";
            break;
        case CURLINFO_DATA_IN:
            text = "<= Recv data";
            break;
        case CURLINFO_SSL_DATA_IN:
            text = "<= Recv SSL data";
            break;
    }

    dump_debug_data(text, stderr, (unsigned char *)data, size,
                    config->trace_ascii);
    return 0;
}
#endif

struct MemoryData {
    char *advance;
    size_t sizeleft;
};

static size_t mem_read_callback(void *ptr, size_t size, size_t nmemb,
                                void *userp) {
    struct MemoryData *puppet = (struct MemoryData *)userp;

#ifdef MULTIBYTESMODE
    size_t realsize = size * nmemb;

    size_t n2read = realsize < puppet->sizeleft ? realsize : puppet->sizeleft;

    printf("n2read = %d, realsize = %d, puppet->sizeleft = %d\n", n2read,
           realsize, puppet->sizeleft);

    if (!n2read) return 0;

    memcpy(ptr, puppet->advance, n2read);
    puppet->advance += n2read;  /* advance pointer */
    puppet->sizeleft -= n2read; /* less data left */

    return n2read;
#else
    if (size * nmemb < 1) return 0;

    while (puppet->sizeleft) {
        *(char *)ptr = puppet->advance[0]; /* copy one single byte */

        puppet->advance++;  /* advance pointer */
        puppet->sizeleft--; /* less data left */

        return 1;
    }

    return 0;
#endif
}

static size_t header_write_callback(void *contents, size_t size, size_t nmemb,
                                    void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryData *puppet = (struct MemoryData *)userp;

    if (!puppet->advance) return 0;

    // write all or fail
    if (realsize > puppet->sizeleft) {
        printf("not enough memory in header_data\n");
        return 0;
    }

    memcpy(puppet->advance, contents, realsize);
    puppet->advance += realsize;
    puppet->sizeleft -= realsize;

    return realsize;
}

// XXX need free
const char *GetUploadId(const char *host, const char *bucket,
                        const char *obj_name, const S3Credential &cred) {
    // POST /ObjectName?uploads HTTP/1.1
    // Host: BucketName.s3.amazonaws.com
    // Date: date
    // Authorization: authorization string (see Authenticating Requests (AWS
    // Signature Version 4))

    // HTTP/1.1 200 OK
    // x-amz-id-2: Uuag1LuByRx9e6j5Onimru9pO4ZVKnJ2Qz7/C1NPcfTWAtRPfTaOFg==
    // x-amz-request-id: 656c76696e6727732072657175657374
    // Date:  Mon, 1 Nov 2010 20:34:56 GMT
    // Content-Length: 197
    // Connection: keep-alive
    // Server: AmazonS3
    //
    // <?xml version="1.0" encoding="UTF-8"?>
    // <InitiateMultipartUploadResult
    // xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
    //   <Bucket>example-bucket</Bucket>
    //     <Key>example-object</Key>
    //       <UploadId>VXBsb2FkIElEIGZvciA2aWWpbmcncyBteS1tb3ZpZS5tMnRzIHVwbG9hZA</UploadId>
    //       </InitiateMultipartUploadResult>
    std::stringstream url;
    std::stringstream path_with_query;
    XMLInfo xml;
    xml.ctxt = NULL;

    if (!host || !bucket || !obj_name) return NULL;

    url << "http://" << host << "/" << bucket << "/" << obj_name;

    HeaderContent *header = new HeaderContent();
    header->Add(HOST, host);
    // XXX
    // header->Disable(CONTENTTYPE);
    header->Add(CONTENTTYPE, "application/x-www-form-urlencoded");
    UrlParser p(url.str().c_str());
    path_with_query << p.Path() << "?uploads";
    SignPOSTv2(header, path_with_query.str().c_str(), cred);

    CURL *curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());

        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);

        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&xml);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ParserCallback);

        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "uploads");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen("uploads"));
    } else {
        return NULL;
    }

    struct curl_slist *chunk = header->GetList();
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK)
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));

    xmlParseChunk(xml.ctxt, "", 0, 1);
    curl_slist_free_all(chunk);
    curl_easy_cleanup(curl);

    if (!xml.ctxt) {
        printf("xmlParseChunk failed\n");
        return NULL;
    }
    xmlNode *root_element = xmlDocGetRootElement(xml.ctxt->myDoc);
    if (!root_element) return NULL;

    char *upload_id = NULL;
    xmlNodePtr cur = root_element->xmlChildrenNode;
    while (cur != NULL) {
        if (!xmlStrcmp(cur->name, (const xmlChar *)"UploadId")) {
            upload_id = strdup((const char *)xmlNodeGetContent(cur));
            break;
        }

        cur = cur->next;
    }

    /* always cleanup */
    xmlFreeParserCtxt(xml.ctxt);

    return upload_id;
}

// XXX need free
const char *PartPutS3Object(const char *host, const char *bucket,
                            const char *obj_name, const S3Credential &cred,
                            const char *data, uint64_t data_size,
                            uint64_t part_number, const char *upload_id) {
    std::stringstream url;
    std::stringstream path_with_query;
    XMLInfo xml;
    xml.ctxt = NULL;

    char *header_buf = (char *)malloc(4 * 1024);
    if (!header_buf) {
        printf("failed to malloc header_buf\n");
        return NULL;
    } else {
        memset(header_buf, 0, 4 * 1024);
    }

    struct MemoryData header_data = {header_buf, 4 * 1024};
    struct MemoryData read_data = {(char *)data, data_size};

    if (!host || !bucket || !obj_name) return NULL;

    url << "http://" << host << "/" << bucket << "/" << obj_name;

    // PUT /ObjectName?partNumber=PartNumber&uploadId=UploadId HTTP/1.1
    // Host: BucketName.s3.amazonaws.com
    // Date: date
    // Content-Length: Size
    // Authorization: authorization string

    url << "?partNumber=" << part_number << "&uploadId=" << upload_id;

    HeaderContent *header = new HeaderContent();
    header->Add(HOST, host);
    // MIME type doesn't matter actually, server wouldn't store it either
    header->Add(CONTENTTYPE, "text/plain");
    header->Add(CONTENTLENGTH, std::to_string(data_size));
    UrlParser p(url.str().c_str());
    path_with_query << p.Path() << "?partNumber=" << part_number
                    << "&uploadId=" << upload_id;
    SignPUTv2(header, path_with_query.str().c_str(), cred);

    CURL *curl = curl_easy_init();
    if (curl) {
        /* specify target URL, and note that this URL should include a file
           name, not only a directory */
        curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());

        /* now specify which file/data to upload */
        curl_easy_setopt(curl, CURLOPT_READDATA, (void *)&read_data);

        /* we want to use our own read function */
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, mem_read_callback);

        /* provide the size of the upload, we specicially typecast the value
           to curl_off_t since we must be sure to use the correct data size */
        curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)data_size);

        /* enable uploading */
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

        /* HTTP PUT please */
        curl_easy_setopt(curl, CURLOPT_PUT, 1L);

        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);

        curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void *)&header_data);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_write_callback);

        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&xml);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ParserCallback);
    } else {
        return NULL;
    }

    struct curl_slist *chunk = header->GetList();
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));

    // to get the Etag from response
    // HTTP/1.1 200 OK
    // x-amz-id-2: Vvag1LuByRx9e6j5Onimru9pO4ZVKnJ2Qz7/C1NPcfTWAtRPfTaOFg==
    // x-amz-request-id: 656c76696e6727732072657175657374
    // Date:  Mon, 1 Nov 2010 20:34:56 GMT
    // ETag: "b54357faf0632cce46e942fa68356b38"
    // Content-Length: 0
    // Connection: keep-alive
    // Server: AmazonS3

    std::ostringstream out;
    out << header_buf;

    // std::cout << header_buf << std::endl;

    // TODO general header content extracting func
    uint64_t etag_start_pos = out.str().find("ETag: ") + 6;
    std::string etag_to_end = out.str().substr(etag_start_pos);
    // RFC 2616 states "HTTP/1.1 defines the sequence CR LF as the end-of-line
    // marker for all protocol elements except the entity-body"
    uint64_t etag_len = etag_to_end.find("\r");

    const char *etag = etag_to_end.substr(0, etag_len).c_str();

    if (etag) {
        curl_slist_free_all(chunk);
        curl_easy_cleanup(curl);
        return strdup(etag);
    }

    // <Error>
    //   <Code>AccessDenied</Code>
    //   <Message>Access Denied</Message>
    //   <RequestId>656c76696e6727732072657175657374</RequestId>
    //   <HostId>Uuag1LuByRx9e6j5Onimru9pO4ZVKnJ2Qz7/C1NPcfTWAtRPfTaOFg==</HostId>
    // </Error>
    xmlParseChunk(xml.ctxt, "", 0, 1);
    if (!xml.ctxt) {
        printf("xmlParseChunk failed\n");
        return NULL;
    }
    xmlNode *root_element = xmlDocGetRootElement(xml.ctxt->myDoc);
    if (!root_element) return NULL;

    char *response_code = NULL;
    xmlNodePtr cur = root_element->xmlChildrenNode;
    while (cur != NULL) {
        if (!xmlStrcmp(cur->name, (const xmlChar *)"Code")) {
            response_code = strdup((const char *)xmlNodeGetContent(cur));
            break;
        }

        cur = cur->next;
    }

    if (response_code) {
        std::cout << response_code << std::endl;
    }

    xmlFreeParserCtxt(xml.ctxt);

    curl_slist_free_all(chunk);
    curl_easy_cleanup(curl);
    free(header_buf);

    return NULL;
}

bool CompleteMultiPutS3(const char *host, const char *bucket,
                        const char *obj_name, const char *upload_id,
                        const char **etag_array, uint64_t count,
                        const S3Credential &cred) {
    std::stringstream url;
    std::stringstream path_with_query;
    XMLInfo xml;
    xml.ctxt = NULL;

#ifdef DEBUG_CURL
    struct debug_data config;
    config.trace_ascii = 1;
#endif

    char *header_buf = (char *)malloc(4 * 1024);
    if (!header_buf) {
        printf("failed to malloc header_buf\n");
        return false;
    } else {
        memset(header_buf, 0, 4 * 1024);
    }

    struct MemoryData header_data = {header_buf, 4 * 1024};

    if (!host || !bucket || !obj_name) return false;

    url << "http://" << host << "/" << bucket << "/" << obj_name;

    url << "?uploadId=" << upload_id;

    // POST
    // /example-object?uploadId=AAAsb2FkIElEIGZvciBlbHZpbmcncyWeeS1tb3ZpZS5tMnRzIRRwbG9hZA
    // HTTP/1.1
    // Host: example-bucket.s3.amazonaws.com
    // Date:  Mon, 1 Nov 2010 20:34:56 GMT
    // Content-Length: 391
    // Authorization: authorization string
    //
    // <CompleteMultipartUpload>
    //   <Part>
    //     <PartNumber>1</PartNumber>
    //     <ETag>"a54357aff0632cce46d942af68356b38"</ETag>
    //   </Part>
    //   <Part>
    //     <PartNumber>2</PartNumber>
    //     <ETag>"0c78aef83f66abc1fa1e8477f296d394"</ETag>
    //   </Part>
    //   <Part>
    //     <PartNumber>3</PartNumber>
    //     <ETag>"acbd18db4cc2f85cedef654fccc4a4d8"</ETag>
    //   </Part>
    // </CompleteMultipartUpload>
    std::stringstream body;

    body << "<CompleteMultipartUpload>\n";
    for (uint64_t i = 0; i < count; ++i) {
        body << "  <Part>\n    <PartNumber>" << i + 1
             << "</PartNumber>\n    <ETag>" << etag_array[i]
             << "</ETag>\n  </Part>\n";
    }
    body << "</CompleteMultipartUpload>";

    // std::cout << body.str().c_str() << std::endl;

    struct MemoryData read_data = {(char *)body.str().c_str(),
                                   strlen(body.str().c_str())};

    HeaderContent *header = new HeaderContent();
    header->Add(HOST, host);
    // If you add a header with no content as in 'Accept:' (no data on the right
    // side of the colon), the internally used header will get disabled. With
    // this option you can add new headers, replace internal headers and remove
    // internal headers. To add a header with no content (nothing to the right
    // side of the colon), use the form 'MyHeader;' (note the ending semicolon).
    // still a space character there
    // XXX
    // header->Disable(CONTENTTYPE);
    header->Add(CONTENTTYPE, "application/xml");
    header->Add(CONTENTLENGTH, std::to_string(strlen(body.str().c_str())));
    UrlParser p(url.str().c_str());
    path_with_query << p.Path() << "?uploadId=" << upload_id;
    SignPOSTv2(header, path_with_query.str().c_str(), cred);

    CURL *curl = curl_easy_init();
    if (curl) {
#ifdef DEBUG_CURL
        curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, trace_debug_data);
        curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &config);
#endif

        /* specify target URL, and note that this URL should include a file
           name, not only a directory */
        curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());

        /* now specify which file/data to upload */
        curl_easy_setopt(curl, CURLOPT_READDATA, (void *)&read_data);

        /* we want to use our own read function */
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, mem_read_callback);

        /* provide the size of the upload, we specicially typecast the value
           to curl_off_t since we must be sure to use the correct data size */
        curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
                         (curl_off_t)strlen(body.str().c_str()));

        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);

        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void *)&header_data);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_write_callback);

        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&xml);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ParserCallback);
    } else {
        return false;
    }

    struct curl_slist *chunk = header->GetList();
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));

    // std::cout << header_buf << std::endl;

    // HTTP/1.1 200 OK
    // x-amz-id-2: Uuag1LuByRx9e6j5Onimru9pO4ZVKnJ2Qz7/C1NPcfTWAtRPfTaOFg==
    // x-amz-request-id: 656c76696e6727732072657175657374
    // Date: Mon, 1 Nov 2010 20:34:56 GMT
    // Connection: close
    // Server: AmazonS3
    //
    // <?xml version="1.0" encoding="UTF-8"?>
    // <CompleteMultipartUploadResult
    // xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
    //   <Location>http://Example-Bucket.s3.amazonaws.com/Example-Object</Location>
    //   <Bucket>Example-Bucket</Bucket>
    //   <Key>Example-Object</Key>
    //   <ETag>"3858f62230ac3c915f300c664312c11f-9"</ETag>
    // </CompleteMultipartUploadResult>

    // <Error>
    //   <Code>AccessDenied</Code>
    //   <Message>Access Denied</Message>
    //   <RequestId>656c76696e6727732072657175657374</RequestId>
    //   <HostId>Uuag1LuByRx9e6j5Onimru9pO4ZVKnJ2Qz7/C1NPcfTWAtRPfTaOFg==</HostId>
    // </Error>
    xmlParseChunk(xml.ctxt, "", 0, 1);
    if (!xml.ctxt) {
        printf("xmlParseChunk failed\n");
        return false;
    }

    xmlNode *root_element = xmlDocGetRootElement(xml.ctxt->myDoc);
    if (!root_element) return false;

    char *response_code = NULL;
    xmlNodePtr cur = root_element->xmlChildrenNode;
    while (cur != NULL) {
        if (!xmlStrcmp(cur->name, (const xmlChar *)"Code")) {
            response_code = (char *)xmlNodeGetContent(cur);
            break;
        }

        cur = cur->next;
    }

    if (response_code) {
        std::cout << response_code << std::endl;
    }

    curl_slist_free_all(chunk);
    curl_easy_cleanup(curl);
    free(header_buf);

    if (response_code) {
        return false;
    }

    xmlFreeParserCtxt(xml.ctxt);

    return true;
}

bool Uploader::init(const char *data, S3Credential *cred) {
    // char *url = //TODO;

    return true;
}

Uploader::Uploader() {
    // fork //TODO

    // PutS3Object(host, bucket, url, cred, data);
}

Uploader::~Uploader() {}

void Uploader::destroy() {}

bool Uploader::write(char *data, uint64_t &len) { return true; }
