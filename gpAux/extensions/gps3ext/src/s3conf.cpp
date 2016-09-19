#include "s3conf.h"

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

bool InitConfig(S3Params& params, const string& conf_path, const string& section) {
// initialize segment related info before loading config file, otherwise, if
// it throws during parsing, segid and segnum values will be undefined.
#ifdef S3_STANDALONE
    s3ext_segid = 0;
    s3ext_segnum = 1;
#else
    s3ext_segid = GpIdentity.segindex;
    s3ext_segnum = GpIdentity.numsegments;
#endif

    S3_CHECK_OR_DIE_MSG(!conf_path.empty(), S3RuntimeError, "Config file is not specified");

    Config s3cfg(conf_path);

    S3_CHECK_OR_DIE_MSG(s3cfg.Handle(), S3RuntimeError,
                        "Failed to parse config file '" + conf_path + "', or it doesn't exist");

    S3_CHECK_OR_DIE_MSG(
        s3cfg.SectionExist(section), S3ConfigError,
        "Selected section '" + section + "' does not exist, please check your configuration file",
        section);

    string content = s3cfg.Get(section.c_str(), "loglevel", "WARNING");
    s3ext_loglevel = getLogLevel(content.c_str());

#ifndef S3_STANDALONE_CHECKCLOUD
    content = s3cfg.Get(section.c_str(), "logtype", "INTERNAL");
    s3ext_logtype = getLogType(content.c_str());

    params.setDebugCurl(to_bool(s3cfg.Get(section.c_str(), "debug_curl", "false")));
#endif

    params.setCred(s3cfg.Get(section.c_str(), "accessid", ""),
                   s3cfg.Get(section.c_str(), "secret", ""),
                   s3cfg.Get(section.c_str(), "token", ""));

    s3ext_logserverhost = s3cfg.Get(section.c_str(), "logserverhost", "127.0.0.1");

    bool ret = s3cfg.Scan(section.c_str(), "logserverport", "%d", &s3ext_logserverport);
    if (!ret) {
        s3ext_logserverport = 1111;
    }

    int32_t scannedValue;
    ret = s3cfg.Scan(section.c_str(), "threadnum", "%d", &scannedValue);
    if (!ret) {
        S3INFO("The thread number is set to default value 4");
        params.setNumOfChunks(4);
    } else if (scannedValue > 8) {
        S3INFO("The given thread number is too big, use max value 8");
        params.setNumOfChunks(8);
    } else if (scannedValue < 1) {
        S3INFO("The given thread number is too small, use min value 1");
        params.setNumOfChunks(1);
    } else {
        params.setNumOfChunks(scannedValue);
    }

    ret = s3cfg.Scan(section.c_str(), "chunksize", "%d", &scannedValue);
    if (!ret) {
        S3INFO("The chunksize is set to default value 64MB");
        params.setChunkSize(64 * 1024 * 1024);
    } else if (scannedValue > 128 * 1024 * 1024) {
        S3INFO("The given chunksize is too large, use max value 128MB");
        params.setChunkSize(128 * 1024 * 1024);
    } else if (scannedValue < 8 * 1024 * 1024) {
        // multipart uploading requires the chunksize larger than 5MB(only the last part to upload
        // could be smaller than 5MB)
        S3INFO("The given chunksize is too small, use min value 8MB");
        params.setChunkSize(8 * 1024 * 1024);
    } else {
        params.setChunkSize(scannedValue);
    }

    ret = s3cfg.Scan(section.c_str(), "low_speed_limit", "%d", &scannedValue);
    if (!ret) {
        S3INFO("The low_speed_limit is set to default value %d bytes/s", 10240);
        params.setLowSpeedLimit(10240);
    } else {
        params.setLowSpeedLimit(scannedValue);
    }

    ret = s3cfg.Scan(section.c_str(), "low_speed_time", "%d", &scannedValue);
    if (!ret) {
        S3INFO("The low_speed_time is set to default value %d seconds", 60);
        params.setLowSpeedTime(60);
    } else {
        params.setLowSpeedTime(scannedValue);
    }

    params.setEncryption(to_bool(s3cfg.Get(section.c_str(), "encryption", "true")));

    params.setAutoCompress(to_bool(s3cfg.Get(section.c_str(), "autocompress", "true")));

    return true;
}

bool InitConfig(S3Params& params, const string& urlWithOptions) {
    string configPath = getOptS3(urlWithOptions, "config");
    if (configPath.empty()) {
        S3WARN("The 'config' parameter is not provided, use default value 's3/s3.conf'.");
        configPath = "s3/s3.conf";
    }

    string configSection = getOptS3(urlWithOptions, "section");
    if (configSection.empty()) {
        configSection = "default";
    }

    return InitConfig(params, configPath, configSection);
}
