#include "s3conf.h"
#include "s3macros.h"
#include "s3params.h"

#include <arpa/inet.h>

#ifndef S3_STANDALONE
extern "C" {
void write_log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
}
#endif

// configurable parameters
int32_t s3ext_segid = -1;
int32_t s3ext_segnum = -1;

string s3ext_logserverhost;
int32_t s3ext_loglevel = EXT_WARNING;
int32_t s3ext_logtype = INTERNAL_LOG;
int32_t s3ext_logserverport = -1;
int32_t s3ext_logsock_udp = -1;
struct sockaddr_in s3ext_logserveraddr;

S3Params InitConfig(const string& urlWithOptions) {
#ifdef S3_STANDALONE
    s3ext_segid = 0;
    s3ext_segnum = 1;
#else
    s3ext_segid = GpIdentity.segindex;
    s3ext_segnum = GpIdentity.numsegments;
#endif

    if (s3ext_segid == -1 && s3ext_segnum > 0) {
        s3ext_segid = 0;
        s3ext_segnum = 1;
    }

    string sourceUrl = TruncateOptions(urlWithOptions);
    S3_CHECK_OR_DIE(!sourceUrl.empty(), S3RuntimeError, "URL not found from location string");

    string configPath = GetOptS3(urlWithOptions, "config");
    if (configPath.empty()) {
        S3WARN("The 'config' parameter is not provided, use default value 's3/s3.conf'.");
        configPath = "s3/s3.conf";
    }

    string configSection = GetOptS3(urlWithOptions, "section");
    if (configSection.empty()) {
        configSection = "default";
    }

    // region could be empty
    string urlRegion = GetOptS3(urlWithOptions, "region");

    // read configurations from file
    Config s3Cfg(configPath);

    S3_CHECK_OR_DIE(s3Cfg.Handle() != NULL, S3RuntimeError,
                    "Failed to parse config file '" + configPath + "', or it doesn't exist");

    S3_CHECK_OR_DIE(s3Cfg.SectionExist(configSection), S3ConfigError,
                    "Selected section '" + configSection +
                        "' does not exist, please check your configuration file",
                    configSection);

    bool useHttps = s3Cfg.GetBool(configSection, "encryption", "true");

    string version = s3Cfg.Get(configSection, "version", "");

    S3Params params(sourceUrl, useHttps, version, urlRegion);

    string content = s3Cfg.Get(configSection, "loglevel", "WARNING");
    s3ext_loglevel = getLogLevel(content.c_str());

    content = s3Cfg.Get(configSection, "logtype", "INTERNAL");
    s3ext_logtype = getLogType(content.c_str());

    params.setDebugCurl(s3Cfg.GetBool(configSection, "debug_curl", "false"));

    params.setCred(s3Cfg.Get(configSection, "accessid", ""), s3Cfg.Get(configSection, "secret", ""),
                   s3Cfg.Get(configSection, "token", ""));

    s3ext_logserverhost = s3Cfg.Get(configSection, "logserverhost", "127.0.0.1");

    s3ext_logserverport = s3Cfg.SafeScan("logserverport", configSection, 1111, 1, 65535);

    int64_t numOfChunks = s3Cfg.SafeScan("threadnum", configSection, 4, 1, 8);
    params.setNumOfChunks(numOfChunks);

    int64_t chunkSize = s3Cfg.SafeScan("chunksize", configSection, 64 * 1024 * 1024,
                                       8 * 1024 * 1024, 128 * 1024 * 1024);
    params.setChunkSize(chunkSize);

    int64_t lowSpeedLimit = s3Cfg.SafeScan("low_speed_limit", configSection, 10240, 0, INT_MAX);
    params.setLowSpeedLimit(lowSpeedLimit);

    int64_t lowSpeedTime = s3Cfg.SafeScan("low_speed_time", configSection, 60, 0, INT_MAX);
    params.setLowSpeedTime(lowSpeedTime);

    params.setProxy(s3Cfg.Get(configSection, "proxy", ""));

    params.setAutoCompress(s3Cfg.GetBool(configSection, "autocompress", "true"));

    params.setVerifyCert(s3Cfg.GetBool(configSection, "verifycert", "true"));

    string sse_type = s3Cfg.Get(configSection, "server_side_encryption", "");
    if (sse_type == "sse-s3") {
        params.setSSEType(SSE_S3);
    } else {
        params.setSSEType(SSE_NONE);
    }

    params.setGpcheckcloud_newline(s3Cfg.Get(configSection, "gpcheckcloud_newline", "\n"));

    CheckEssentialConfig(params);

    return params;
}

void CheckEssentialConfig(const S3Params& params) {
    if (params.getCred().accessID.empty()) {
        S3_CHECK_OR_DIE(false, S3ConfigError, "\"FATAL: access id not set\"", "accessid");
    }

    if (params.getCred().secret.empty()) {
        S3_CHECK_OR_DIE(false, S3ConfigError, "\"FATAL: secret id not set\"", "secret");
    }

    if (s3ext_segnum <= 0) {
        S3_CHECK_OR_DIE(false, S3ConfigError, "\"FATAL: segment info is invalid\"", "segment");
    }

    string newline = params.getGpcheckcloud_newline();
    if (newline.compare("\n") && newline.compare("\r\n") && newline.compare("\r")) {
        S3_CHECK_OR_DIE(false, S3ConfigError, "\"FATAL: gpcheckcloud_newline is invalid\"\"",
                        "gpcheckcloud_newline");
    }
}
