#include <string>
using std::string;

// UDP socket to send log
extern int32_t s3ext_logsock_udp;

// default log level
extern int32_t s3ext_loglevel;

// thread number for downloading
extern int32_t s3ext_threadnum;

// chunk size for each downloading
extern int32_t s3ext_chunksize;

// segment id
extern int32_t s3ext_segid;

// total segment number
extern int32_t s3ext_segnum;

// log type
extern int32_t s3ext_logtype;

// remote server port if use external log server
extern int32_t s3ext_logserverport;

// remote server address if use external log server
extern string s3ext_logserverhost;

// s3 access id
extern string s3ext_accessid;

// s3 secret
extern string s3ext_secret;

// s3 token
extern string s3ext_token;

// HTTP or HTTPS
extern bool s3ext_encryption;

// debug curl or not
extern bool s3ext_debug_curl;

// server address where log message is sent to
extern struct sockaddr_in s3ext_logserveraddr;

// low speed timeout
extern int32_t s3ext_low_speed_limit;
extern int32_t s3ext_low_speed_time;

// not thread safe!! call it only once.
bool InitConfig(const string &path, const string section);
void CheckEssentialConfig();
