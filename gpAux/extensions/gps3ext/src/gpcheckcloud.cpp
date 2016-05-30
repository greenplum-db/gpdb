#include "gpcheckcloud.h"

volatile bool QueryCancelPending = false;

int main(int argc, char *argv[]) {
    int opt = 0;
    bool ret = true;

    s3ext_logtype = STDERR_LOG;
    s3ext_loglevel = EXT_ERROR;

    if (argc == 1) {
        print_usage(stderr);
        exit(EXIT_FAILURE);
    }

    while ((opt = getopt(argc, argv, "c:d:ht")) != -1) {
        switch (opt) {
            case 'c':
                ret = check_config(optarg);
                break;
            case 'd':
                ret = s3_download(optarg);
                break;
            case 'h':
                print_usage(stdout);
                break;
            case 't':
                print_template();
                break;
            default:
                print_usage(stderr);
                exit(EXIT_FAILURE);
        }

        break;
    }

    if (ret) {
        exit(EXIT_SUCCESS);
    } else {
        fprintf(stderr, "Failed. Please check the arguments.\n\n");
        print_usage(stderr);
        exit(EXIT_FAILURE);
    }
}

void print_template() {
    printf(
        "[default]\n"
        "secret = \"aws secret\"\n"
        "accessid = \"aws access id\"\n"
        "threadnum = 4\n"
        "chunksize = 67108864\n"
        "low_speed_limit = 10240\n"
        "low_speed_time = 60\n"
        "encryption = true\n");
}

void print_usage(FILE *stream) {
    fprintf(stream,
            "Usage: gpcheckcloud -c \"s3://endpoint/bucket/prefix "
            "config=path_to_config_file\", to check the configuration.\n"
            "       gpcheckcloud -d \"s3://endpoint/bucket/prefix "
            "config=path_to_config_file\", to download and output to stdout.\n"
            "       gpcheckcloud -t, to show the config template.\n"
            "       gpcheckcloud -h, to show this help.\n");
}

bool read_config(const char *config) {
    bool ret = false;

    ret = InitConfig(config, "default");
    s3ext_logtype = STDERR_LOG;

    return ret;
}

ListBucketResult *list_bucket(S3Reader *wrapper) {
    S3Credential g_cred = {s3ext_accessid, s3ext_secret};

    ListBucketResult *r =
        ListBucket("https", wrapper->get_region(), wrapper->get_bucket(),
                   wrapper->get_prefix(), g_cred);

    return r;
}

uint8_t print_contents(ListBucketResult *r) {
    char urlbuf[256];
    uint8_t count = 0;
    vector<BucketContent *>::iterator i;

    for (i = r->contents.begin(); i != r->contents.end(); i++) {
        if ((s3ext_loglevel <= EXT_WARNING) && count > 8) {
            printf("... ...\n");
            break;
        }

        BucketContent *p = *i;
        snprintf(urlbuf, 256, "%s", p->getName().c_str());
        printf("File: %s, Size: %" PRIu64 "\n", urlbuf, p->getSize());

        count++;
    }

    return count;
}

bool check_config(const char *url_with_options) {
    char *url_str = truncate_options(url_with_options);
    if (!url_str) {
        return false;
    }

    char *config_path = get_opt_s3(url_with_options, "config");
    if (!config_path) {
        free(url_str);
        return false;
    }

    curl_global_init(CURL_GLOBAL_ALL);

    S3Reader *wrapper = NULL;
    ListBucketResult *result = NULL;
    bool ret = false;

    if (!read_config(config_path)) {
        goto FAIL;
    }

    wrapper = new S3Reader(url_str);
    if (!wrapper) {
        fprintf(stderr, "Failed to allocate wrapper\n");
        goto FAIL;
    }

    if (!wrapper->ValidateURL()) {
        fprintf(stderr, "Failed: URL is not valid.\n");
        goto FAIL;
    }

    result = list_bucket(wrapper);
    if (!result) {
        goto FAIL;
    } else {
        if (print_contents(result)) {
            printf("Yea! Your configuration works well.\n");
        } else {
            printf(
                "Your configuration works well, however there is no file "
                "matching your prefix.\n");
        }
        delete result;
    }

    ret = true;

FAIL:
    free(url_str);
    free(config_path);

    if (wrapper) {
        delete wrapper;
    }

    return ret;
}

bool s3_download(const char *url_with_options) {
    if (!url_with_options) {
        return false;
    }

    S3Reader *wrapper = NULL;
    int data_len = BUF_SIZE;

    thread_setup();

    char *data_buf = (char *)malloc(BUF_SIZE);
    if (!data_buf) {
        goto FAIL;
    }

    s3ext_logtype = STDERR_LOG;

    wrapper = reader_init(url_with_options);
    if (!wrapper) {
        fprintf(stderr, "Failed to init wrapper\n");
        goto FAIL;
    }

    s3ext_segid = 0;
    s3ext_segnum = 1;

    do {
        data_len = BUF_SIZE;

        if (!reader_transfer_data(wrapper, data_buf, data_len)) {
            fprintf(stderr, "Failed to read data\n");
            goto FAIL;
        }

        fwrite(data_buf, data_len, 1, stdout);
    } while (data_len);

    thread_cleanup();

    if (!reader_cleanup(&wrapper)) {
        fprintf(stderr, "Failed to cleanup wrapper\n");
        goto FAIL;
    }

    free(data_buf);

    return true;

FAIL:
    if (data_buf) {
        free(data_buf);
    }

    if (wrapper) {
        delete wrapper;
    }

    return false;
}
