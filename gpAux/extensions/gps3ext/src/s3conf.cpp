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

typedef void (S3Params::*setFunction)(uint64_t);

struct SetValueFunction {
    virtual void setValue(int64_t value) = 0;
    void operator()(int64_t value) {
        this->setValue(value);
    }
};

struct S3ParamSetFunction : SetValueFunction {
    S3ParamSetFunction(S3Params& params, setFunction func) : params(params), func(func) {
    }

    virtual void setValue(int64_t value) {
        (params.*func)(value);
    }

    S3Params& params;
    setFunction func;
};

struct GlobalSetFunction : SetValueFunction {
    GlobalSetFunction(int32_t& globalValue) : globalValue(globalValue) {
    }

    virtual void setValue(int64_t value) {
        globalValue = value;
    }

    int32_t& globalValue;
};

// Scan an integer config value with given range. If scan failed, use default value instead.
void SafeScanValue(const char* varName, Config& s3cfg, const string& section,
                   SetValueFunction&& set, int64_t defaultValue, int64_t minValue,
                   int64_t maxValue) {
    int64_t scannedValue = 0;

    // Here we use %12lld to truncate long number string, because configure value
    // has type of int32_t, who has the range [-2^31, 2^31-1] (10-digit number)
    bool isSuccess = s3cfg.Scan(section.c_str(), varName, "%12lld", &scannedValue);

    if (!isSuccess) {
        S3INFO("The %s is set to default value %lld", varName, defaultValue);
        scannedValue = defaultValue;
    } else if (scannedValue > maxValue) {
        S3INFO("The given %s (%lld) is too big, use max value %lld", varName, scannedValue,
               maxValue);
        scannedValue = maxValue;
    } else if (scannedValue < minValue) {
        S3INFO("The given %s (%lld) is too small, use min value %lld", varName, scannedValue,
               minValue);
        scannedValue = minValue;
    }

    set(scannedValue);
}

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

    SafeScanValue("logserverport", s3cfg, section, GlobalSetFunction(s3ext_logserverport), 1111, 1,
                  65535);

    SafeScanValue("threadnum", s3cfg, section,
                  S3ParamSetFunction(params, &S3Params::setNumOfChunks), 4, 1, 8);

    SafeScanValue("chunksize", s3cfg, section, S3ParamSetFunction(params, &S3Params::setChunkSize),
                  64 * 1024 * 1024, 8 * 1024 * 1024, 128 * 1024 * 1024);

    SafeScanValue("low_speed_limit", s3cfg, section,
                  S3ParamSetFunction(params, &S3Params::setLowSpeedLimit), 10240, 0, INT_MAX);

    SafeScanValue("low_speed_time", s3cfg, section,
                  S3ParamSetFunction(params, &S3Params::setLowSpeedTime), 60, 0, INT_MAX);

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
