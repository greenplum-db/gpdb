#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>

#include "s3conf.h"
#include "s3log.h"
#include "s3utils.h"

#ifndef S3_STANDALONE
extern "C" {
void write_log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
}
#endif

using std::string;
using std::stringstream;

// configurable parameters
int32_t s3ext_loglevel = -1;
int32_t s3ext_threadnum = 5;
int32_t s3ext_chunksize = 64 * 1024 * 1024;
int32_t s3ext_logtype = -1;
int32_t s3ext_logserverport = -1;

int32_t s3ext_low_speed_limit = 10240;
int32_t s3ext_low_speed_time = 60;

string s3ext_logserverhost;
string s3ext_accessid;
string s3ext_secret;
string s3ext_token;

bool s3ext_encryption = true;
bool s3ext_debug_curl = false;

// global variables
int32_t s3ext_segid = -1;
int32_t s3ext_segnum = -1;

string s3ext_config_path;
struct sockaddr_in s3ext_logserveraddr;
int32_t s3ext_logsock_udp = -1;

// not thread safe!!
bool InitConfig(const string& conf_path, const string section = "default") {
    if (conf_path == "") {
#ifndef S3_STANDALONE
        write_log("Config file is not specified\n");
#else
        S3ERROR("Config file is not specified");
#endif
        return false;
    }

    Config* s3cfg = new Config(conf_path);
    if (s3cfg == NULL || !s3cfg->Handle()) {
#ifndef S3_STANDALONE
        write_log("Failed to parse config file \"%s\", or it doesn't exist\n", conf_path.c_str());
#else
        S3ERROR("Failed to parse config file \"%s\", or it doesn't exist", conf_path.c_str());
#endif
        delete s3cfg;
        return false;
    }

    string content = s3cfg->Get(section.c_str(), "loglevel", "WARNING");
    s3ext_loglevel = getLogLevel(content.c_str());

#ifndef S3_CHK_CFG
    content = s3cfg->Get(section.c_str(), "logtype", "INTERNAL");
    s3ext_logtype = getLogType(content.c_str());
#endif

    s3ext_accessid = s3cfg->Get(section.c_str(), "accessid", "");
    s3ext_secret = s3cfg->Get(section.c_str(), "secret", "");
    s3ext_token = s3cfg->Get(section.c_str(), "token", "");

    s3ext_logserverhost = s3cfg->Get(section.c_str(), "logserverhost", "127.0.0.1");

    bool ret = s3cfg->Scan(section.c_str(), "logserverport", "%d", &s3ext_logserverport);
    if (!ret) {
        s3ext_logserverport = 1111;
    }

    ret = s3cfg->Scan(section.c_str(), "threadnum", "%d", &s3ext_threadnum);
    if (!ret) {
        S3INFO("The thread number is set to default value 4");
        s3ext_threadnum = 4;
    }
    if (s3ext_threadnum > 8) {
        S3INFO("The given thread number is too big, use max value 8");
        s3ext_threadnum = 8;
    }
    if (s3ext_threadnum < 1) {
        S3INFO("The given thread number is too small, use min value 1");
        s3ext_threadnum = 1;
    }

    ret = s3cfg->Scan(section.c_str(), "chunksize", "%d", &s3ext_chunksize);
    if (!ret) {
        S3INFO("The chunksize is set to default value 64MB");
        s3ext_chunksize = 64 * 1024 * 1024;
    }
    if (s3ext_chunksize > 128 * 1024 * 1024) {
        S3INFO("The given chunksize is too large, use max value 128MB");
        s3ext_chunksize = 128 * 1024 * 1024;
    }
    if (s3ext_chunksize < 2 * 1024 * 1024) {
        S3INFO("The given chunksize is too small, use min value 2MB");
        s3ext_chunksize = 2 * 1024 * 1024;
    }

    ret = s3cfg->Scan(section.c_str(), "low_speed_limit", "%d", &s3ext_low_speed_limit);
    if (!ret) {
        S3INFO("The low_speed_limit is set to default value %d bytes/s", 10240);
        s3ext_low_speed_limit = 10240;
    }

    ret = s3cfg->Scan(section.c_str(), "low_speed_time", "%d", &s3ext_low_speed_time);
    if (!ret) {
        S3INFO("The low_speed_time is set to default value %d seconds", 60);
        s3ext_low_speed_time = 60;
    }

    content = s3cfg->Get(section.c_str(), "encryption", "true");
    s3ext_encryption = to_bool(content);

    content = s3cfg->Get(section.c_str(), "debug_curl", "false");
    s3ext_debug_curl = to_bool(content);

#ifdef S3_STANDALONE
    s3ext_segid = 0;
    s3ext_segnum = 1;
#else
    s3ext_segid = GpIdentity.segindex;
    s3ext_segnum = GpIdentity.numsegments;
#endif

    delete s3cfg;

    return true;
}
