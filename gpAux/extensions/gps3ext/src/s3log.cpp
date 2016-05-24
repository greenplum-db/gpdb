#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "s3conf.h"
#include "s3log.h"
#include "s3utils.h"

#ifndef S3_STANDALONE
extern "C" {
void write_log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
}
#endif

void _LogMessage(const char* fmt, va_list args) {
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
#ifdef S3_STANDALONE
    fprintf(stderr, "%s", buf);
#else
    write_log("%s", buf);
#endif
}

void _send_to_remote(const char* fmt, va_list args) {
    char buf[1024];
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
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
        case REMOTE_LOG:  // `socat UDP-RECV:[port] STDOUT` to listen
            _send_to_remote(fmt, args);
            break;
        default:
            break;
    }
    va_end(args);
}

static bool loginited = false;

void InitRemoteLog() {
    if (loginited) {
        return;
    }

    s3ext_logsock_udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s3ext_logsock_udp < 0) {
        perror("Failed to create socket while InitRemoteLog()");
    }

    memset(&s3ext_logserveraddr, 0, sizeof(struct sockaddr_in));
    s3ext_logserveraddr.sin_family = AF_INET;
    s3ext_logserveraddr.sin_port = htons(s3ext_logserverport);
    inet_aton(s3ext_logserverhost.c_str(), &s3ext_logserveraddr.sin_addr);

    loginited = true;
}

LOGTYPE getLogType(const char* v) {
    if (!v) return STDERR_LOG;
    if (strcmp(v, "REMOTE") == 0) return REMOTE_LOG;
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
