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

#include "gps3ext.h"
#include "s3conf.h"
#include "s3log.h"
#include "s3utils.h"

//#include <cdb/cdbvars.h>

#ifndef DEBUG_S3
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
// invoked by s3_import(), need to be exception safe
bool InitConfig(const string& conf_path,
                const string& section /*not used currently*/) {
    try {
        if (conf_path == "") {
#ifndef DEBUG_S3
            write_log("Config file is not specified\n");
#else
            S3ERROR("Config file is not specified");
#endif
            return false;
        }

        if (s3cfg) delete s3cfg;

        s3cfg = new Config(conf_path);
        if (!s3cfg || !s3cfg->Handle()) {
#ifndef DEBUG_S3
            write_log("Failed to parse config file\n");
#else
            S3ERROR("Failed to parse config file");
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

#ifdef S3_CHK_CFG
        if (s3ext_loglevel == -1) {
            content = cfg->Get("default", "loglevel", "WARNING");
            s3ext_loglevel = getLogLevel(content.c_str());
        }

        if (s3ext_logtype == -1) {
            content = cfg->Get("default", "logtype", "INTERNAL");
            s3ext_logtype = getLogType(content.c_str());
        }
#else
        content = cfg->Get("default", "loglevel", "WARNING");
        s3ext_loglevel = getLogLevel(content.c_str());

        content = cfg->Get("default", "logtype", "INTERNAL");
        s3ext_logtype = getLogType(content.c_str());
#endif

        s3ext_accessid = cfg->Get("default", "accessid", "");
        s3ext_secret = cfg->Get("default", "secret", "");
        s3ext_token = cfg->Get("default", "token", "");

#ifdef DEBUG_S3
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
        if (s3ext_threadnum > 8) {
            S3INFO("The given thread number is too big, use max value 8");
            s3ext_threadnum = 8;
        }
        if (s3ext_threadnum < 1) {
            S3INFO("The given thread number is too small, use min value 1");
            s3ext_threadnum = 1;
        }

        ret = cfg->Scan("default", "chunksize", "%d", &s3ext_chunksize);
        if (!ret) {
            S3INFO("Failed to get chunksize, use default value 64MB");
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

        ret = cfg->Scan("default", "low_speed_limit", "%d",
                        &s3ext_low_speed_limit);
        if (!ret) {
            S3INFO(
                "Failed to get low_speed_limit, use default value %d bytes/s",
                10240);
            s3ext_low_speed_limit = 10240;
        }

        ret =
            cfg->Scan("default", "low_speed_time", "%d", &s3ext_low_speed_time);
        if (!ret) {
            S3INFO("Failed to get low_speed_time, use default value %d seconds",
                   60);
            s3ext_low_speed_time = 60;
        }

        content = cfg->Get("default", "encryption", "true");
        s3ext_encryption = to_bool(content);

#ifdef DEBUG_S3
        s3ext_segid = 0;
        s3ext_segnum = 1;
#else
        s3ext_segid = GpIdentity.segindex;
        s3ext_segnum = GpIdentity.numsegments;
#endif
    } catch (...) {
        return false;
    }

    return true;
}

// invoked by s3_import(), need to be exception safe
void ClearConfig() {
    try {
        if (s3cfg) {
            delete s3cfg;
            s3cfg = NULL;
        }
    } catch (...) {
        return;
    }
}
