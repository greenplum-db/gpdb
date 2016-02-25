#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string>
#include <errno.h>
#include <unistd.h>
#include <cstdio>
#include <cstdarg>
#include <sys/stat.h>
#include <fcntl.h>
#include <sstream>

#include "S3Log.h"
#include "utils.h"
#include "gps3conf.h"
#include "gps3ext.h"

//#include <cdb/cdbvars.h>

#ifndef DEBUGS3
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
string s3ext_logpath;
string s3ext_accessid;
string s3ext_secret;
string s3ext_token;

bool s3ext_encryption;

// global variables
int32_t s3ext_segid = -1;
int32_t s3ext_segnum = -1;

string s3ext_config_path;
struct sockaddr_in s3ext_logserveraddr;
struct sockaddr_un s3ext_logserverpath;
int32_t s3ext_logsock_local = -1;
int32_t s3ext_logsock_udp = -1;
Config* s3cfg = NULL;

// not thread safe!!
bool InitConfig(string conf_path, string section /*not used currently*/) {
    if (conf_path == "") {
#ifndef DEBUGS3
        write_log("Config file is not specified\n");
#endif
        return false;
    }

    if (s3cfg) delete s3cfg;

    s3cfg = new Config(conf_path);
    if (!s3cfg || !s3cfg->Handle()) {
#ifndef DEBUGS3
        write_log("Failed to parse config file\n");
#endif
        if (s3cfg) {
            delete s3cfg;
            s3cfg = NULL;
        }
        return false;
    }

    Config* cfg = s3cfg;
    bool ret = false;
    string content;
    content = cfg->Get("default", "loglevel", "INFO");
    s3ext_loglevel = getLogLevel(content.c_str());

    content = cfg->Get("default", "logtype", "INTERNAL");
    s3ext_logtype = getLogType(content.c_str());

    s3ext_accessid = cfg->Get("default", "accessid", "");
    s3ext_secret = cfg->Get("default", "secret", "");
    s3ext_token = cfg->Get("default", "token", "");

#ifdef DEBUGS3
// s3ext_loglevel = EXT_DEBUG;
// s3ext_logtype = LOCAL_LOG;
#endif

    s3ext_logpath = cfg->Get("default", "logpath", "/tmp/.s3log.sock");
    s3ext_logserverhost = cfg->Get("default", "logserverhost", "127.0.0.1");

    ret = cfg->Scan("default", "logserverport", "%d", &s3ext_logserverport);
    if (!ret) {
        s3ext_logserverport = 1111;
    }

    ret = cfg->Scan("default", "threadnum", "%d", &s3ext_threadnum);
    if (!ret) {
        S3INFO("Failed to get thread number, use default value 4");
        s3ext_threadnum = 4;
    }

    ret = cfg->Scan("default", "chunksize", "%d", &s3ext_chunksize);
    if (!ret) {
        S3INFO("Failed to get chunksize, use default value %d",
               64 * 1024 * 1024);
        s3ext_chunksize = 64 * 1024 * 1024;
    }

    ret = cfg->Scan("default", "low_speed_limit", "%d", &s3ext_low_speed_limit);
    if (!ret) {
        S3INFO("Failed to get low_speed_limit, use default value %d bytes/s",
               10240);
        s3ext_low_speed_limit = 10240;
    }

    ret = cfg->Scan("default", "low_speed_time", "%d", &s3ext_low_speed_time);
    if (!ret) {
        S3INFO("Failed to get low_speed_time, use default value %d seconds",
               60);
        s3ext_low_speed_time = 60;
    }

    content = cfg->Get("default", "encryption", "true");
    s3ext_encryption = to_bool(content);

#ifdef DEBUGS3
    s3ext_segid = 0;
    s3ext_segnum = 1;
#else
    s3ext_segid = GpIdentity.segindex;
    s3ext_segnum = GpIdentity.numsegments;
#endif
    return true;
}

void ClearConfig() {
    if (s3cfg) {
        delete s3cfg;
        s3cfg = NULL;
    }
}
