#include "s3log.h"
#include "s3macros.h"
#include "s3params.h"

#include <arpa/inet.h>

#define MAX_MESSAGE_LINE_LENGTH 1024

#ifndef S3_STANDALONE
extern "C" {
void write_log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
}
#endif

void LogMessage(LOGLEVEL loglevel, const char* fmt, ...) {
    if (loglevel > s3ext_loglevel) return;
    char buf[MAX_MESSAGE_LINE_LENGTH];
    size_t len;
    va_list args;
    va_start(args, fmt);
    switch (s3ext_logtype)
    {
        case INTERNAL_LOG:
            vsnprintf(buf, sizeof(buf), fmt, args);
#ifdef S3_STANDALONE
            fprintf(stderr, "%s", buf);
#else
            write_log("%s", buf);
#endif
            break;

        case STDERR_LOG:
            vfprintf(stderr, fmt, args);
            break;

        case REMOTE_LOG:  // `socat UDP-RECV:[port] STDOUT` to listen
            len = vsnprintf(buf, sizeof(buf), fmt, args);
            sendto(s3ext_logsock_udp, buf, len, 0, (struct sockaddr*)&s3ext_logserveraddr,
                   sizeof(struct sockaddr_in));
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
    S3_CHECK_OR_DIE(s3ext_logsock_udp != -1, S3RuntimeError,
                    string("Failed to create socket: ") + strerror(errno));

    memset(&s3ext_logserveraddr, 0, sizeof(struct sockaddr_in));
    s3ext_logserveraddr.sin_family = AF_INET;
    s3ext_logserveraddr.sin_port = htons(s3ext_logserverport);
    inet_aton(s3ext_logserverhost.c_str(), &s3ext_logserveraddr.sin_addr);

    loginited = true;
}

LOGTYPE getLogType(const char* v) {
    if (!v) return STDERR_LOG;
    if (strcmpci(v, "REMOTE") == 0) return REMOTE_LOG;
    if (strcmpci(v, "INTERNAL") == 0) return INTERNAL_LOG;
    return STDERR_LOG;
}

LOGLEVEL getLogLevel(const char* v) {
    if (!v) return EXT_FATAL;
    if (strcmpci(v, "DEBUG") == 0) return EXT_DEBUG;
    if (strcmpci(v, "WARNING") == 0) return EXT_WARNING;
    if (strcmpci(v, "INFO") == 0) return EXT_INFO;
    if (strcmpci(v, "ERROR") == 0) return EXT_ERROR;
    return EXT_FATAL;
}
