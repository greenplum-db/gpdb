#include <unistd.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "gps3ext.h"
#include "s3conf.h"
#include "s3downloader.h"
#include "s3log.h"
#include "s3wrapper.h"

volatile bool QueryCancelPending = false;

S3Reader *wrapper = NULL;

void print_template() {
    printf(
        "[default]\n"
        "secret = \"<aws secret>\"\n"
        "accessid = \"<aws access id>\"\n"
        "threadnum = 4\n"
        "chunksize = 67108864\n"
        "low_speed_limit = 10240\n"
        "low_speed_time = 60\n"
        "encryption = true\n");
}

void print_usage(FILE *stream) {
    fprintf(stream,
            "Usage: s3chkcfg -c \"s3://endpoint/bucket/prefix "
            "config=path_to_config_file\", to check the configuration.\n"
            "       s3chkcfg -d \"s3://endpoint/bucket/prefix "
            "config=path_to_config_file\", to download and output to stdout.\n"
            "       s3chkcfg -t, to show the config template.\n"
            "       s3chkcfg -h, to show this help.\n");
}

bool read_config(char *config) {
    bool ret = false;

    ret = InitConfig(config, "default");
    s3ext_logtype = STDERR_LOG;
    s3ext_loglevel = EXT_ERROR;

    return ret;
}

ListBucketResult *list_bucket(char *url) {
    S3Credential g_cred = {s3ext_accessid, s3ext_secret};

    wrapper = new S3Reader(url);
    if (!wrapper) {
        fprintf(stderr, "Failed to allocate wrapper\n");
        return NULL;
    }

    if (!wrapper->ValidateURL()) {
        fprintf(stderr, "Failed: URL is not valid.\n");
        return NULL;
    }

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
        if (count > 8) {
            printf("... ...\n");
            break;
        }

        BucketContent *p = *i;
        snprintf(urlbuf, 256, "%s", p->Key().c_str());
        printf("File: %s, Size: %" PRIu64 "\n", urlbuf, p->Size());

        count++;
    }

    return count;
}

bool check_config(char *url_with_options) {
    char *url_str = truncate_options(url_with_options);
    char *config_path = get_opt_s3(url_with_options, "config");

    if (!read_config(config_path)) {
        return false;
    }

    ListBucketResult *r = list_bucket(url_str);
    if (!r) {
        return false;
    } else {
        if (print_contents(r)) {
            printf("Yea! Your configuration works well.\n");
        } else {
            printf(
                "Your configuration works well, however there is no file "
                "matching your prefix.\n");
        }
        delete r;
    }

    free(url_str);
    free(config_path);

    return true;
}

#define BUF_SIZE 64 * 1024
bool s3_download(char *url_with_options) {
    char *url_str = truncate_options(url_with_options);
    char *config_path = get_opt_s3(url_with_options, "config");
    vector<BucketContent *>::iterator i;
    ListBucketResult *r = NULL;
    Downloader *d = NULL;

    char *buf = (char *)malloc(BUF_SIZE);
    if (!buf) {
        goto FAIL;
    }

    if (!read_config(config_path)) {
        goto FAIL;
    }

    r = list_bucket(url_str);
    if (!r) {
        goto FAIL;
    }

    for (i = r->contents.begin(); i != r->contents.end(); i++) {
        BucketContent *p = *i;
        S3Credential g_cred = {s3ext_accessid, s3ext_secret};

        uint64_t buf_len = BUF_SIZE;

        d = new Downloader(s3ext_threadnum);
        if (!d) {
            goto FAIL;
        }

        if (!d->init(wrapper->getKeyURL(p->Key()), wrapper->get_region(),
                     p->Size(), s3ext_chunksize, &g_cred)) {
            goto FAIL;
        }

        while (1) {
            if (!d->get(buf, buf_len)) {
                goto FAIL;
            }

            if (buf_len == 0) {
                break;
            }

            fwrite(buf, buf_len, 1, stdout);

            buf_len = BUF_SIZE;
        }

        d->destroy();
        delete d;
    }

    free(url_str);
    free(config_path);
    free(buf);
    delete r;

    return true;

FAIL:
    if (url_str) {
        free(url_str);
    }

    if (config_path) {
        free(config_path);
    }

    if (buf) {
        free(buf);
    }

    if (r) {
        delete r;
    }

    if (d) {
        d->destroy();
        delete r;
    }

    return false;
}

int main(int argc, char *argv[]) {
    int opt = 0;
    int ret = 0;

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
    }

    if (wrapper) {
        delete wrapper;
    }

    return ret;
}
