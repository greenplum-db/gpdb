#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string>
#include <errno.h>
#include <unistd.h>
#include <cstdio>
#include <cstdarg>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <netinet/in.h>
// #include <thread>

#include <vector>
#include <memory>
using std::vector;
// using std::shared_ptr;
// using std::make_shared;
#include "utils.h"

#include "S3Log.h"
#include "gps3conf.h"

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

#ifndef DEBUGS3
extern "C" {
void write_log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
}
#endif

// fake implement
void _LogMessage(const char* fmt, va_list args) {
    char buf[1024];
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    if (len >= 1024) len = 1023;
    buf[len] = 0;
#ifdef DEBUGS3
    fprintf(stderr, "%s\n", buf);
#else
    write_log("%s", buf);
#endif
}

void _send_to_local(const char* fmt, va_list args) {
    char buf[1024];
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    if (len >= 1024) len = 1023;
    buf[len] = 0;
    sendto(s3ext_logsock_local, buf, len, 0,
           (struct sockaddr*)&s3ext_logserverpath, sizeof(struct sockaddr_un));
}

void _send_to_remote(const char* fmt, va_list args) {
    char buf[1024];
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    if (len >= 1024) len = 1023;
    buf[len] = 0;
    sendto(s3ext_logsock_udp, buf, len, 0,
           (struct sockaddr*)&s3ext_logserveraddr, sizeof(struct sockaddr_in));
}

void LogMessage(LOGLEVEL loglevel, const char* fmt, ...) {
    if (loglevel > s3ext_loglevel) return;
    va_list args;
    va_start(args, fmt);
    switch (s3ext_logtype) {
        case INTERNAL_LOG:
            _LogMessage(fmt, args);
            break;
        case STDERR_LOG:
            vfprintf(stderr, fmt, args);
            break;
        case REMOTE_LOG:
            _send_to_remote(fmt, args);
            break;
        case LOCAL_LOG:
            _send_to_local(fmt, args);
            break;
        default:
            break;
    }
    va_end(args);
}

static bool loginited = false;
void InitLog() {
    if (!loginited) {
        s3ext_logsock_local = socket(PF_UNIX, SOCK_DGRAM, 0);
        if (s3ext_logsock_local < 0) {
            perror("Failed to create socket while InitLog()");
        }

        /* start with a clean address structure */
        memset(&s3ext_logserverpath, 0, sizeof(struct sockaddr_un));
        s3ext_logserverpath.sun_family = AF_UNIX;
        snprintf(s3ext_logserverpath.sun_path, UNIX_PATH_MAX, "%s",
                 s3ext_logpath.c_str());

        s3ext_logsock_udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s3ext_logsock_udp < 0) {
            perror("Failed to create socket while InitLog()");
        }

        memset(&s3ext_logserveraddr, 0, sizeof(struct sockaddr_in));
        s3ext_logserveraddr.sin_family = AF_INET;
        s3ext_logserveraddr.sin_port = htons(s3ext_logserverport);
        inet_aton(s3ext_logserverhost.c_str(), &s3ext_logserveraddr.sin_addr);

        loginited = true;
    }
}

LOGTYPE getLogType(const char* v) {
    if (!v) return STDERR_LOG;
    if (strcmp(v, "REMOTE") == 0) return REMOTE_LOG;
    if (strcmp(v, "LOCAL") == 0) return LOCAL_LOG;
    if (strcmp(v, "INTERNAL") == 0) return INTERNAL_LOG;
    return STDERR_LOG;
}

LOGLEVEL getLogLevel(const char* v) {
    if (!v) return EXT_FATAL;
    if (strcmp(v, "DEBUG") == 0) return EXT_DEBUG;
    if (strcmp(v, "WARNING") == 0) return EXT_WARNING;
    if (strcmp(v, "INFO") == 0) return EXT_INFO;
    if (strcmp(v, "ERROR") == 0) return EXT_ERROR;
    return EXT_FATAL;
}
