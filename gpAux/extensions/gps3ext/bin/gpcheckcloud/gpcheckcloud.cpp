#include "gpcheckcloud.h"

volatile bool QueryCancelPending = false;

int main(int argc, char *argv[]) {
    int opt = 0;
    bool ret = true;

    s3ext_loglevel = EXT_ERROR;
    s3ext_logtype = STDERR_LOG;

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
        fprintf(stderr, "Failed. Please check the arguments and configuration file.\n\n");
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
    if (!url_with_options) {
        return false;
    }

    GPReader *reader = reader_init(url_with_options);
    if (!reader) {
        return false;
    }

    ListBucketResult *result = reader->getKeyList();
    if (result != NULL) {
        if (print_contents(result)) {
            printf("\nYea! Your configuration works well.\n");
        } else {
            printf(
                "\nYour configuration works well, however there is no file "
                "matching your prefix.\n");
        }
    }

    reader_cleanup(&reader);

    return true;
}

bool s3_download(const char *url_with_options) {
    if (!url_with_options) {
        return false;
    }

    int data_len = BUF_SIZE;
    char data_buf[BUF_SIZE];
    bool ret = true;

    thread_setup();

    GPReader *reader = reader_init(url_with_options);
    if (!reader) {
        return false;
    }

    do {
        data_len = BUF_SIZE;

        if (!reader_transfer_data(reader, data_buf, data_len)) {
            fprintf(stderr, "Failed to read data\n");
            ret = false;
            break;
        }

        fwrite(data_buf, data_len, 1, stdout);
    } while (data_len);

    reader_cleanup(&reader);

    thread_cleanup();

    return ret;
}
